#include "DeEsserDSP.h"
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// DeEsserDSP - Phase H-3 (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────

DeEsserDSP::DeEsserDSP()
{
    mScHpfL.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    mScHpfR.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    mAudHpfL.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    mAudHpfR.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    mAudLpfL.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    mAudLpfR.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    mThresholdSmoothed.setCurrentAndTargetValue (mThresholdDb);
    mRangeSmoothed    .setCurrentAndTargetValue (mRangeDb);
    mMixSmoothed      .setCurrentAndTargetValue (mMix);

    calcCoefs();
}

void DeEsserDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;

    juce::dsp::ProcessSpec spec { sampleRate,
                                  (juce::uint32) juce::jmax (1, maxBlockSize),
                                  1 };
    mScHpfL.prepare (spec);  mScHpfR.prepare (spec);
    mAudHpfL.prepare (spec); mAudHpfR.prepare (spec);
    mAudLpfL.prepare (spec); mAudLpfR.prepare (spec);

    // Lookahead buffers sized to max possible (5 ms) + 1 sample for safe wrap.
    const int maxLASamples = static_cast<int> (std::ceil (kMaxLookaheadMs * 0.001 * sampleRate)) + 1;
    mLookaheadL.assign ((size_t) maxLASamples, 0.0f);
    mLookaheadR.assign ((size_t) maxLASamples, 0.0f);
    mLAWritePos = 0;
    mLASamples = juce::jlimit (0, (int) mLookaheadL.size() - 1,
        static_cast<int> (mLookaheadMs * 0.001 * sampleRate));

    constexpr float kGrDecayDbPerSec = 30.0f;
    mGrDecayDbPerBlock = kGrDecayDbPerSec * (float) maxBlockSize
                       / (float) (sampleRate > 0.0 ? sampleRate : 44100.0);

    const double rampSecs = 0.02;
    mThresholdSmoothed.reset (sampleRate, rampSecs);
    mRangeSmoothed    .reset (sampleRate, rampSecs);
    mMixSmoothed      .reset (sampleRate, rampSecs);
    mThresholdSmoothed.setCurrentAndTargetValue (mThresholdDb);
    mRangeSmoothed    .setCurrentAndTargetValue (mRangeDb);
    mMixSmoothed      .setCurrentAndTargetValue (mMix);

    calcCoefs();
    updateScCoefs();
    reset();
}

void DeEsserDSP::reset()
{
    mEnvL = 0.0f;
    mEnvR = 0.0f;
    mGainReductionDb.store (0.0f);
    if (! mLookaheadL.empty()) std::fill (mLookaheadL.begin(), mLookaheadL.end(), 0.0f);
    if (! mLookaheadR.empty()) std::fill (mLookaheadR.begin(), mLookaheadR.end(), 0.0f);
    mLAWritePos = 0;
    mScHpfL.reset();  mScHpfR.reset();
    mAudHpfL.reset(); mAudHpfR.reset();
    mAudLpfL.reset(); mAudLpfR.reset();
}

void DeEsserDSP::calcCoefs()
{
    const double sr = (mSampleRate > 0.0) ? mSampleRate : 44100.0;
    mAttackCoef  = static_cast<float> (std::exp (-1.0 / (mAttackMs  * 0.001 * sr)));
    mReleaseCoef = static_cast<float> (std::exp (-1.0 / (mReleaseMs * 0.001 * sr)));
}

void DeEsserDSP::updateScCoefs()
{
    if (mSampleRate <= 0.0) return;
    const float fc = juce::jlimit (4000.0f, 12000.0f, mFreqHz);
    const float q  = juce::jlimit (0.5f, 4.0f, mQ);

    mScHpfL.setCutoffFrequency (fc); mScHpfL.setResonance (q);
    mScHpfR.setCutoffFrequency (fc); mScHpfR.setResonance (q);

    // Linkwitz-Riley-ish split: HPF + LPF at same fc, Q=0.7071 each.
    mAudHpfL.setCutoffFrequency (fc); mAudHpfL.setResonance (0.7071f);
    mAudHpfR.setCutoffFrequency (fc); mAudHpfR.setResonance (0.7071f);
    mAudLpfL.setCutoffFrequency (fc); mAudLpfL.setResonance (0.7071f);
    mAudLpfR.setCutoffFrequency (fc); mAudLpfR.setResonance (0.7071f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Setters
// ─────────────────────────────────────────────────────────────────────────────
void DeEsserDSP::setMode (int m)
{
    const Mode nm = static_cast<Mode> (juce::jlimit (0, 1, m));
    if (nm != mMode) mMode = nm;
}

void DeEsserDSP::setMsMode (int m)
{
    const MsMode nm = static_cast<MsMode> (juce::jlimit (0, 2, m));
    if (nm != mMsMode) mMsMode = nm;
}

void DeEsserDSP::setFrequencyHz (float hz)
{
    const float v = juce::jlimit (4000.0f, 12000.0f, hz);
    if (v != mFreqHz) { mFreqHz = v; updateScCoefs(); }
}

void DeEsserDSP::setQ (float q)
{
    const float v = juce::jlimit (0.5f, 4.0f, q);
    if (v != mQ) { mQ = v; updateScCoefs(); }
}

void DeEsserDSP::setThresholdDb (float dB)
{
    const float v = juce::jlimit (-40.0f, 0.0f, dB);
    if (v != mThresholdDb) { mThresholdDb = v; mThresholdSmoothed.setTargetValue (v); }
}

void DeEsserDSP::setRangeDb (float dB)
{
    const float v = juce::jlimit (-20.0f, 0.0f, dB);
    if (v != mRangeDb) { mRangeDb = v; mRangeSmoothed.setTargetValue (v); }
}

void DeEsserDSP::setAttackMs (float ms)
{
    const float v = juce::jlimit (0.1f, 30.0f, ms);
    if (v != mAttackMs) { mAttackMs = v; calcCoefs(); }
}

void DeEsserDSP::setReleaseMs (float ms)
{
    const float v = juce::jlimit (10.0f, 500.0f, ms);
    if (v != mReleaseMs) { mReleaseMs = v; calcCoefs(); }
}

void DeEsserDSP::setLookaheadMs (float ms)
{
    const float v = juce::jlimit (0.0f, kMaxLookaheadMs, ms);
    if (v == mLookaheadMs) return;
    mLookaheadMs = v;
    if (mSampleRate > 0.0 && ! mLookaheadL.empty())
        mLASamples = juce::jlimit (0, (int) mLookaheadL.size() - 1,
            static_cast<int> (v * 0.001 * mSampleRate));
}

void DeEsserDSP::setMix (float v)
{
    const float vv = juce::jlimit (0.0f, 1.0f, v);
    if (vv != mMix) { mMix = vv; mMixSmoothed.setTargetValue (vv); }
}

void DeEsserDSP::setListen (bool on) { mListen = on; }

// ─────────────────────────────────────────────────────────────────────────────
// process()
// ─────────────────────────────────────────────────────────────────────────────
void DeEsserDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;

    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    float* outL = buffer.getWritePointer (0);
    float* outR = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

    const bool isStereo = (numChannels > 1);
    const float att = mAttackCoef;
    const float rel = mReleaseCoef;
    const bool useLA = (mLASamples > 0);
    const int laSize = (int) mLookaheadL.size();
    const int laSamples = mLASamples;

    float blockMaxGrDb = 0.0f;   // most-negative GR seen this block

    for (int i = 0; i < numSamples; ++i)
    {
        const float thrDb   = mThresholdSmoothed.getNextValue();
        const float rangeDb = mRangeSmoothed    .getNextValue();
        const float curMix  = mMixSmoothed      .getNextValue();

        // ── Pull source samples ─────────────────────────────────────────────
        const float drySrcL = outL[i];
        const float drySrcR = isStereo ? outR[i] : drySrcL;

        // M/S detection / application axis selection.  Stereo uses raw L/R;
        // Mid uses (L+R)/2 in both detector and apply path; Side uses (L-R)/2.
        float dL = drySrcL;
        float dR = drySrcR;
        if (mMsMode == MsMode::MidOnly)
        {
            const float mid = (drySrcL + drySrcR) * 0.5f;
            dL = dR = mid;
        }
        else if (mMsMode == MsMode::SideOnly)
        {
            const float side = (drySrcL - drySrcR) * 0.5f;
            dL = dR = side;
        }

        // ── Sidechain HPF on detection signal ───────────────────────────────
        const float scL = mScHpfL.processSample (0, dL);
        const float scR = isStereo ? mScHpfR.processSample (0, dR) : scL;

        // ── Envelope follower (peak detector, atk/rel one-pole) ─────────────
        const float magL = std::abs (scL);
        const float magR = std::abs (scR);
        const float cL = (magL > mEnvL) ? att : rel;
        const float cR = (magR > mEnvR) ? att : rel;
        mEnvL = cL * mEnvL + (1.0f - cL) * magL;
        mEnvR = cR * mEnvR + (1.0f - cR) * magR;

        // Stereo-linked detector: louder channel drives both gain paths.
        const float envLink = std::max (mEnvL, mEnvR);
        const float envDb   = 20.0f * std::log10 (std::max (envLink, 1e-9f));

        // ── Threshold gate + GR computation ─────────────────────────────────
        // overshoot = how far above threshold the detector is.  Map 0..N dB
        // overshoot to 0..rangeDb (negative) reduction with a soft proportional
        // curve.  rangeDb is the FLOOR (max reduction).
        float grDb = 0.0f;
        const float overshoot = envDb - thrDb;
        if (overshoot > 0.0f)
        {
            // Softer-than-linear pull-down: 1 - exp(-x/N) maps overshoot to
            // [0..1] saturating curve, then scale by rangeDb.
            const float t  = 1.0f - std::exp (-overshoot / 6.0f);   // 0..1
            grDb = rangeDb * t;                                     // 0..rangeDb (negative)
        }
        if (grDb < blockMaxGrDb) blockMaxGrDb = grDb;

        const float grLin = juce::Decibels::decibelsToGain (grDb);

        // ── Lookahead-delayed audio fetch + write ───────────────────────────
        float audioL = drySrcL;
        float audioR = drySrcR;
        if (useLA)
        {
            const int readPos = (mLAWritePos + laSize - laSamples) % laSize;
            audioL = mLookaheadL[(size_t) readPos];
            audioR = mLookaheadR[(size_t) readPos];
            mLookaheadL[(size_t) mLAWritePos] = drySrcL;
            mLookaheadR[(size_t) mLAWritePos] = drySrcR;
            mLAWritePos = (mLAWritePos + 1) % laSize;
        }

        // ── Listen mode: monitor the sidechain HPF output, skip de-essing ──
        if (mListen)
        {
            outL[i] = scL;
            if (outR) outR[i] = scR;
            continue;
        }

        // ── Apply GR per Mode ───────────────────────────────────────────────
        float wetL = audioL;
        float wetR = audioR;

        if (mMode == Mode::Wide)
        {
            // Full-band ducking when sibilance triggers.
            wetL = audioL * grLin;
            wetR = audioR * grLin;
        }
        else // Mode::Split
        {
            // Split into two bands at the sidechain crossover; only high band
            // gets ducked, low band passes unchanged.  Sum the two bands at
            // the output.  Applied to the M/S axis via dL/dR substitution
            // when MsMode != Stereo so the duck respects the user's M/S pick.
            const float bandSrcL = (mMsMode == MsMode::Stereo) ? audioL : dL;
            const float bandSrcR = (mMsMode == MsMode::Stereo) ? audioR : dR;
            const float lowL  = mAudLpfL.processSample (0, bandSrcL);
            const float lowR  = isStereo ? mAudLpfR.processSample (0, bandSrcR) : lowL;
            const float highL = mAudHpfL.processSample (0, bandSrcL);
            const float highR = isStereo ? mAudHpfR.processSample (0, bandSrcR) : highL;
            wetL = lowL + highL * grLin;
            wetR = lowR + highR * grLin;

            // For M/S mode, decode back to stereo from the processed mid/side.
            if (mMsMode == MsMode::MidOnly)
            {
                // dL,dR carried mid; original side is preserved.  Recompose:
                const float origSide = (drySrcL - drySrcR) * 0.5f;
                wetL = wetL + origSide;
                wetR = wetR - origSide;
            }
            else if (mMsMode == MsMode::SideOnly)
            {
                const float origMid = (drySrcL + drySrcR) * 0.5f;
                wetL =  origMid + wetL;
                wetR =  origMid - wetR;
            }
        }

        // ── Wide-mode M/S decode (parallel to the Split-mode block above) ──
        if (mMode == Mode::Wide && mMsMode != MsMode::Stereo)
        {
            const float origMid  = (drySrcL + drySrcR) * 0.5f;
            const float origSide = (drySrcL - drySrcR) * 0.5f;
            if (mMsMode == MsMode::MidOnly)
            {
                // wetL == wetR == ducked mid.  Side is original.
                wetL = wetL + origSide;
                wetR = wetR - origSide;
            }
            else // SideOnly
            {
                wetL = origMid + wetL;
                wetR = origMid - wetR;
            }
        }

        // ── Wet/dry mix ─────────────────────────────────────────────────────
        outL[i] = audioL + curMix * (wetL - audioL);
        if (outR) outR[i] = audioR + curMix * (wetR - audioR);
    }

    // ── GR meter publish (peak hold + decay 30 dB/sec) ─────────────────────
    const float lastGr = mGainReductionDb.load (std::memory_order_relaxed);
    float newGr = std::min (lastGr, blockMaxGrDb);
    newGr = std::min (0.0f, newGr + mGrDecayDbPerBlock);
    mGainReductionDb.store (newGr, std::memory_order_relaxed);
}

// ─────────────────────────────────────────────────────────────────────────────
// Serialization
// ─────────────────────────────────────────────────────────────────────────────
void DeEsserDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("DeEsserDSP");
    state.setProperty ("mode",        (int) mMode,        nullptr);
    state.setProperty ("msMode",      (int) mMsMode,      nullptr);
    state.setProperty ("freqHz",      mFreqHz,            nullptr);
    state.setProperty ("q",           mQ,                 nullptr);
    state.setProperty ("thresholdDb", mThresholdDb,       nullptr);
    state.setProperty ("rangeDb",     mRangeDb,           nullptr);
    state.setProperty ("attackMs",    mAttackMs,          nullptr);
    state.setProperty ("releaseMs",   mReleaseMs,         nullptr);
    state.setProperty ("lookaheadMs", mLookaheadMs,       nullptr);
    state.setProperty ("mix",         mMix,               nullptr);
    state.setProperty ("listen",      (int) mListen,      nullptr);
    state.setProperty ("bypassed",    (int) bypassed,     nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml) juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void DeEsserDSP::setStateInformation (const void* data, int sz)
{
    std::unique_ptr<juce::XmlElement> xml (juce::AudioProcessor::getXmlFromBinary (data, sz));
    if (! xml || ! xml->hasTagName ("DeEsserDSP")) return;

    juce::ValueTree state = juce::ValueTree::fromXml (*xml);
    mMode        = static_cast<Mode> (juce::jlimit (0, 1,
                       (int) state.getProperty ("mode", 0)));
    mMsMode      = static_cast<MsMode> (juce::jlimit (0, 2,
                       (int) state.getProperty ("msMode", 0)));
    mFreqHz      = (float)(double) state.getProperty ("freqHz",      6500.0);
    mQ           = (float)(double) state.getProperty ("q",           1.4);
    mThresholdDb = (float)(double) state.getProperty ("thresholdDb", -24.0);
    mRangeDb     = (float)(double) state.getProperty ("rangeDb",     -12.0);
    mAttackMs    = (float)(double) state.getProperty ("attackMs",    1.0);
    mReleaseMs   = (float)(double) state.getProperty ("releaseMs",   80.0);
    mLookaheadMs = (float)(double) state.getProperty ("lookaheadMs", 0.0);
    mMix         = (float)(double) state.getProperty ("mix",         1.0);
    mListen      = ((int) state.getProperty ("listen",   0)) != 0;
    bypassed     = ((int) state.getProperty ("bypassed", 0)) != 0;

    if (mSampleRate > 0.0 && ! mLookaheadL.empty())
        mLASamples = juce::jlimit (0, (int) mLookaheadL.size() - 1,
            static_cast<int> (mLookaheadMs * 0.001 * mSampleRate));

    calcCoefs();
    updateScCoefs();
    mThresholdSmoothed.setCurrentAndTargetValue (mThresholdDb);
    mRangeSmoothed    .setCurrentAndTargetValue (mRangeDb);
    mMixSmoothed      .setCurrentAndTargetValue (mMix);
}
