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
    mModeSmoothed     .setCurrentAndTargetValue (mModeBlend / 100.0f);

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

    // Lookahead buffers sized to max possible (kMaxLookaheadMs) + 1 sample for safe wrap.
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
    mModeSmoothed     .reset (sampleRate, rampSecs);
    mThresholdSmoothed.setCurrentAndTargetValue (mThresholdDb);
    mRangeSmoothed    .setCurrentAndTargetValue (mRangeDb);
    mMixSmoothed      .setCurrentAndTargetValue (mMix);
    mModeSmoothed     .setCurrentAndTargetValue (mModeBlend / 100.0f);

    mSpectral.prepare (sampleRate, maxBlockSize);
    // Spectral-path rings sized for the max (HQ) latency (one 2048 frame) + one block.
    const int spRingSize = 2048 + juce::jmax (1, maxBlockSize) + 64;
    mSpDryL .assign ((size_t) spRingSize, 0.0f);
    mSpDryR .assign ((size_t) spRingSize, 0.0f);
    mSpOther.assign ((size_t) spRingSize, 0.0f);
    mSpAxis .assign ((size_t) juce::jmax (1, maxBlockSize), 0.0f);
    mSpRingPos = 0;
    mSpLastLat = -1;
    mSpectralPrimed = false;

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
    mSpectral.reset();
    if (! mSpDryL.empty())
    {
        std::fill (mSpDryL .begin(), mSpDryL .end(), 0.0f);
        std::fill (mSpDryR .begin(), mSpDryR .end(), 0.0f);
        std::fill (mSpOther.begin(), mSpOther.end(), 0.0f);
    }
    mSpRingPos = 0;
    mSpectralPrimed = false;
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

    // Linkwitz-Riley-ish split, FIXED at 4 kHz (the reference hardcodes its
    // Split point; the user's Freq only moves the detector HPF above).
    constexpr float kSplitHz = 4000.0f;
    mAudHpfL.setCutoffFrequency (kSplitHz); mAudHpfL.setResonance (0.7071f);
    mAudHpfR.setCutoffFrequency (kSplitHz); mAudHpfR.setResonance (0.7071f);
    mAudLpfL.setCutoffFrequency (kSplitHz); mAudLpfL.setResonance (0.7071f);
    mAudLpfR.setCutoffFrequency (kSplitHz); mAudLpfR.setResonance (0.7071f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Setters
// ─────────────────────────────────────────────────────────────────────────────
void DeEsserDSP::setModeBlend (float v)
{
    const float vv = juce::jlimit (0.0f, 100.0f, v);
    if (vv != mModeBlend)
    {
        mModeBlend = vv;
        mModeSmoothed.setTargetValue (vv / 100.0f);
    }
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
    const float v = juce::jlimit (-80.0f, 0.0f, dB);
    if (v != mThresholdDb) { mThresholdDb = v; mThresholdSmoothed.setTargetValue (v); }
}

void DeEsserDSP::setRangeDb (float dB)
{
    const float v = juce::jlimit (-48.0f, 0.0f, dB);
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

void DeEsserDSP::setEngine (int e)
{
    const Engine ne = static_cast<Engine> (juce::jlimit (0, 1, e));
    if (ne != mEngine) mEngine = ne;   // path swap + state priming happen at block rate
}

void DeEsserDSP::setSpectralQuality (int q)
{
    const int v = juce::jlimit (0, 1, q);
    if (v != mSpectralQuality)
    {
        mSpectralQuality = v;
        mSpectral.setQuality (v);      // applied on the audio thread next block
    }
}

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

    if (mEngine == Engine::Spectral)
    {
        processSpectral (buffer);
        return;
    }
    mSpectralPrimed = false;   // spectral state re-primes fresh on next activation

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

        // ── Apply GR: continuous Wide<->Split blend on the DELAYED audio ────
        // Wide = full-band duck; Split = only the fixed-4 kHz high band ducks.
        // M/S extracts the axis from the delayed signal and recomposes with the
        // untouched component (detector above already ran on the chosen axis).
        const float blend = mModeSmoothed.getNextValue();   // 0=Wide .. 1=Split
        float wetL, wetR;
        float monL = 0.0f, monR = 0.0f;   // Listen: the removed component

        if (mMsMode == MsMode::Stereo)
        {
            const float lowL   = mAudLpfL.processSample (0, audioL);
            const float highL  = mAudHpfL.processSample (0, audioL);
            const float wideL  = audioL * grLin;
            const float splitL = lowL + highL * grLin;
            wetL = wideL + (splitL - wideL) * blend;
            monL = (1.0f - grLin) * (audioL + (highL - audioL) * blend);

            if (isStereo)
            {
                const float lowR   = mAudLpfR.processSample (0, audioR);
                const float highR  = mAudHpfR.processSample (0, audioR);
                const float wideR  = audioR * grLin;
                const float splitR = lowR + highR * grLin;
                wetR = wideR + (splitR - wideR) * blend;
                monR = (1.0f - grLin) * (audioR + (highR - audioR) * blend);
            }
            else { wetR = wetL; monR = monL; }
        }
        else
        {
            const float procIn = (mMsMode == MsMode::MidOnly)
                               ? (audioL + audioR) * 0.5f
                               : (audioL - audioR) * 0.5f;
            const float other  = (mMsMode == MsMode::MidOnly)
                               ? (audioL - audioR) * 0.5f
                               : (audioL + audioR) * 0.5f;

            const float lowP   = mAudLpfL.processSample (0, procIn);
            const float highP  = mAudHpfL.processSample (0, procIn);
            const float wideP  = procIn * grLin;
            const float splitP = lowP + highP * grLin;
            // Keep the R filter pair fed so its state stays valid across a
            // later switch back to Stereo (output unused).
            mAudLpfR.processSample (0, procIn);
            mAudHpfR.processSample (0, procIn);

            const float wetP = wideP + (splitP - wideP) * blend;
            // Removed component of the processed axis only -- the untouched
            // axis contributes nothing to the monitor.
            const float monP = (1.0f - grLin) * (procIn + (highP - procIn) * blend);

            if (mMsMode == MsMode::MidOnly) { wetL = wetP + other;  wetR = wetP - other;  monL = monP;  monR = monP;  }
            else                            { wetL = other + wetP;  wetR = other - wetP;  monL = monP;  monR = -monP; }
        }

        // ── Listen (reference Monitor): solo the removed component ─────────
        // Computed from the gain reduction directly ((1-gr) x the ducked
        // band), NOT dry-minus-wet: the split filters rotate phase, so plain
        // subtraction leaks the whole program even at zero GR.  Silent when
        // nothing is detected.
        if (mListen)
        {
            outL[i] = monL;
            if (outR) outR[i] = monR;
            continue;
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
// processSpectral - Engine::Spectral block path
// ─────────────────────────────────────────────────────────────────────────────
void DeEsserDSP::processSpectral (juce::AudioBuffer<float>& buffer)
{
    const int n   = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    float* L = buffer.getWritePointer (0);
    float* R = (nCh > 1) ? buffer.getWritePointer (1) : nullptr;
    const bool isStereo = (R != nullptr);

    const int ringSize = (int) mSpDryL.size();
    if (ringSize == 0 || (int) mSpAxis.size() < n) return;   // prepare not run / oversize block

    // Pending == active after the first process() call this block, so this
    // latency is the one the delivered audio actually has.
    const int lat = mSpectral.getLatencySamples();

    // Fresh activation or quality swap: the rings hold audio delayed at the
    // OLD latency -- re-zero so the recompose/mix stays aligned.
    if (! mSpectralPrimed || lat != mSpLastLat)
    {
        if (! mSpectralPrimed) mSpectral.reset();
        std::fill (mSpDryL .begin(), mSpDryL .end(), 0.0f);
        std::fill (mSpDryR .begin(), mSpDryR .end(), 0.0f);
        std::fill (mSpOther.begin(), mSpOther.end(), 0.0f);
        mSpRingPos      = 0;
        mSpLastLat      = lat;
        mSpectralPrimed = true;
    }

    // Keep the time-path smoothers current so switching engines back resumes
    // without a jump (spectral consumes the raw params at frame rate instead).
    mThresholdSmoothed.skip (n);
    mRangeSmoothed    .skip (n);
    mModeSmoothed     .skip (n);

    SibilanceSpectralProcessor::Params p;
    p.detectLoHz    = mFreqHz;
    p.thresholdDb   = mThresholdDb;
    p.rangeDb       = mRangeDb;
    p.blend01       = mModeBlend * 0.01f;
    p.attackMs      = mAttackMs;
    p.releaseMs     = mReleaseMs;
    p.outputRemoved = mListen;

    // Push dry (and the untouched M/S axis) into the latency rings BEFORE the
    // STFT transforms the buffer in place.
    const int base = mSpRingPos;
    for (int i = 0; i < n; ++i)
    {
        const int w = (base + i) % ringSize;
        mSpDryL[(size_t) w] = L[i];
        if (isStereo) mSpDryR[(size_t) w] = R[i];
    }
    mSpRingPos = (base + n) % ringSize;

    if (mMsMode == MsMode::Stereo)
    {
        mSpectral.process (L, isStereo ? R : nullptr, n, p);
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            const float sl   = L[i];
            const float sr   = isStereo ? R[i] : sl;
            const float mid  = (sl + sr) * 0.5f;
            const float side = (sl - sr) * 0.5f;
            mSpAxis[(size_t) i] = (mMsMode == MsMode::MidOnly) ? mid : side;
            mSpOther[(size_t) ((base + i) % ringSize)] =
                (mMsMode == MsMode::MidOnly) ? side : mid;
        }
        mSpectral.process (mSpAxis.data(), nullptr, n, p);
    }

    for (int i = 0; i < n; ++i)
    {
        const int rd = (base + i - lat + 2 * ringSize) % ringSize;
        const float dryL   = mSpDryL[(size_t) rd];
        const float dryR   = isStereo ? mSpDryR[(size_t) rd] : dryL;
        const float curMix = mMixSmoothed.getNextValue();

        float wetL, wetR;
        if (mMsMode == MsMode::Stereo)
        {
            wetL = L[i];
            wetR = isStereo ? R[i] : wetL;
        }
        else
        {
            const float axis  = mSpAxis[(size_t) i];               // delayed processed axis
            const float other = mSpOther[(size_t) rd];             // delayed untouched axis
            if (mListen)
            {
                // Monitor: removed component of the axis only (other adds 0).
                wetL = axis;
                wetR = (mMsMode == MsMode::MidOnly) ? axis : -axis;
            }
            else if (mMsMode == MsMode::MidOnly) { wetL = axis + other;  wetR = axis - other; }
            else                                 { wetL = other + axis;  wetR = other - axis; }
        }

        if (mListen)
        {
            // Monitor bypasses the Mix blend (matches the time path).
            L[i] = wetL;
            if (isStereo) R[i] = wetR;
        }
        else
        {
            L[i] = dryL + curMix * (wetL - dryL);
            if (isStereo) R[i] = dryR + curMix * (wetR - dryR);
        }
    }

    // GR meter: same peak-hold + decay publish as the time path.
    const float grNow  = mSpectral.getLastReductionDb();
    const float lastGr = mGainReductionDb.load (std::memory_order_relaxed);
    float newGr = juce::jmin (lastGr, grNow);
    newGr = juce::jmin (0.0f, newGr + mGrDecayDbPerBlock);
    mGainReductionDb.store (newGr, std::memory_order_relaxed);
}

// ─────────────────────────────────────────────────────────────────────────────
// Serialization
// ─────────────────────────────────────────────────────────────────────────────
void DeEsserDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("DeEsserDSP");
    state.setProperty ("modeBlend",   mModeBlend,         nullptr);
    state.setProperty ("engine",      (int) mEngine,      nullptr);
    state.setProperty ("spQuality",   mSpectralQuality,   nullptr);
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
    // Legacy migration: pre-Task-8 states stored an int "mode" (0=Wide,
    // 1=Split); map to the blend endpoints so old projects keep their sound.
    if (state.hasProperty ("modeBlend"))
        mModeBlend = juce::jlimit (0.0f, 100.0f,
                       (float)(double) state.getProperty ("modeBlend", 50.0));
    else
        mModeBlend = ((int) state.getProperty ("mode", 0) != 0) ? 100.0f : 0.0f;
    setEngine          ((int) state.getProperty ("engine",    0));
    setSpectralQuality ((int) state.getProperty ("spQuality", 0));
    mMsMode      = static_cast<MsMode> (juce::jlimit (0, 2,
                       (int) state.getProperty ("msMode", 0)));
    mFreqHz      = (float)(double) state.getProperty ("freqHz",      6500.0);
    mQ           = (float)(double) state.getProperty ("q",           1.4);
    mThresholdDb = juce::jlimit (-80.0f, 0.0f,
                       (float)(double) state.getProperty ("thresholdDb", -12.5));
    mRangeDb     = juce::jlimit (-48.0f, 0.0f,
                       (float)(double) state.getProperty ("rangeDb",     -14.0));
    mAttackMs    = (float)(double) state.getProperty ("attackMs",    1.0);
    mReleaseMs   = (float)(double) state.getProperty ("releaseMs",   80.0);
    mLookaheadMs = juce::jlimit (0.0f, kMaxLookaheadMs,
                       (float)(double) state.getProperty ("lookaheadMs", 0.0));
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
    mModeSmoothed     .setCurrentAndTargetValue (mModeBlend / 100.0f);
}
