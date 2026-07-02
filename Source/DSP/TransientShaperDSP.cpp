#include "TransientShaperDSP.h"
#include <cmath>

namespace
{
    // Envelope coefficient from time constant: coef = exp(-1 / (ms * sr / 1000))
    inline float envCoef (float ms, double sr) noexcept
    {
        if (ms <= 0.0f || sr <= 0.0) return 0.0f;
        return std::exp (-1.0f / (float)(ms * 0.001 * sr));
    }

    constexpr float kFastAttMs[3] = { 0.05f, 0.5f, 2.0f };    // per AttackShape (Sharp/Med/Soft)
    constexpr float kSlowRelMs[3] = { 5.0f,  20.0f, 80.0f };  // per ReleaseShape

    constexpr float kSmoothMsFast = 15.0f;
    constexpr float kSmoothMsMed  = 20.0f;
}

TransientShaperDSP::TransientShaperDSP()
{
    recalcEnvelopeCoefs();
}

// -- Helpers ------------------------------------------------------------------

void TransientShaperDSP::recalcEnvelopeCoefs()
{
    const double sr = mSampleRate > 0.0 ? mSampleRate : 44100.0;
    const int atkShape = juce::jlimit (0, 2, mAttackShape);
    const int relShape = juce::jlimit (0, 2, mReleaseShape);

    mFastAttCoef = envCoef (kFastAttMs[atkShape], sr);
    mFastRelCoef = envCoef (mFastRelMs,            sr);    // C4 user-settable
    mSlowAttCoef = envCoef (mSlowAttMs,            sr);    // C4 user-settable
    mSlowRelCoef = envCoef (kSlowRelMs[relShape], sr);
}

void TransientShaperDSP::snapSmoothedToTargets()
{
    mAttackSmooth      .setCurrentAndTargetValue (mAttack);
    mReleaseSmooth     .setCurrentAndTargetValue (mRelease);
    mSensitivitySmooth .setCurrentAndTargetValue (mSensitivity);
    mSplitFreqSmooth   .setCurrentAndTargetValue (mSplitFreq);
    mBalanceSmooth     .setCurrentAndTargetValue ((mSplitBalance + 100.0f) / 200.0f);
    mDriveSmooth       .setCurrentAndTargetValue (mDrive);
    mOutGainSmooth     .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (mOutGainDb));
    mWetSmooth         .setCurrentAndTargetValue (mWet);
}

// -- DSPBase interface --------------------------------------------------------

void TransientShaperDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;
    recalcEnvelopeCoefs();

    // 11b: Linkwitz-Riley 4th-order crossover prep.
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2 };
    mLR_LP.prepare (spec);
    mLR_HP.prepare (spec);
    mLR_LP.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    mLR_HP.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    mLR_LP.setCutoffFrequency (mSplitFreq);
    mLR_HP.setCutoffFrequency (mSplitFreq);

    // 11c / C1: oversampler (always runs for constant latency regardless of drive).
    mOversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2 /* numChannels */,
        juce::jlimit (1, 4, mOsLog2),
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true /* maxQuality */);
    mOversampler->initProcessing ((size_t) maxBlockSize);
    mOversampler->reset();
    mLatencySamples = (int) std::ceil (mOversampler->getLatencyInSamples());

    // Scratch
    mBandBuf.setSize (2, maxBlockSize, false, true, true);
    mBandBuf.clear();
    mWetScr    .assign ((size_t) maxBlockSize, mWet);
    mOutGainScr.assign ((size_t) maxBlockSize, juce::Decibels::decibelsToGain (mOutGainDb));

    // 11d smoothers
    mAttackSmooth      .reset (sampleRate, kSmoothMsFast * 0.001);
    mReleaseSmooth     .reset (sampleRate, kSmoothMsFast * 0.001);
    mSensitivitySmooth .reset (sampleRate, kSmoothMsMed  * 0.001);
    mSplitFreqSmooth   .reset (sampleRate, kSmoothMsMed  * 0.001);
    mBalanceSmooth     .reset (sampleRate, kSmoothMsMed  * 0.001);
    mDriveSmooth       .reset (sampleRate, kSmoothMsFast * 0.001);
    mOutGainSmooth     .reset (sampleRate, kSmoothMsFast * 0.001);
    mWetSmooth         .reset (sampleRate, kSmoothMsFast * 0.001);
    snapSmoothedToTargets();

    reset();
}

void TransientShaperDSP::reset()
{
    mFastL   = mFastR   = 0.0f;
    mSlowMs  = mSlowMsR = 0.0f;
    mLR_LP.reset();
    mLR_HP.reset();
    if (mOversampler) mOversampler->reset();
    mBandBuf.clear();
}

// -- Setters (A2: CPU-guarded) ------------------------------------------------

void TransientShaperDSP::setAttack (float a)
{
    // A4: simplified. Panel always sends -100..100; divide by 100 to store -1..1.
    const float n = juce::jlimit (-1.0f, 1.0f, a * 0.01f);
    if (n != mAttack) { mAttack = n; mAttackSmooth.setTargetValue (n); }
}

void TransientShaperDSP::setSustain (float s)
{
    const float n = juce::jlimit (-1.0f, 1.0f, s);
    if (n != mRelease) { mRelease = n; mReleaseSmooth.setTargetValue (n); }
}

void TransientShaperDSP::setSensitivity (float s)
{
    const float n = juce::jlimit (0.0f, 1.0f, s);
    if (n != mSensitivity) { mSensitivity = n; mSensitivitySmooth.setTargetValue (n); }
}

void TransientShaperDSP::setRelease (float rel)
{
    // -100..100 UI -> -1..1 stored in mRelease.
    const float n = juce::jlimit (-1.0f, 1.0f, rel * 0.01f);
    if (n != mRelease) { mRelease = n; mReleaseSmooth.setTargetValue (n); }
}

void TransientShaperDSP::setAttackShape (int shape)
{
    const int n = juce::jlimit (0, 2, shape);
    if (n != mAttackShape) { mAttackShape = n; recalcEnvelopeCoefs(); }
}

void TransientShaperDSP::setReleaseShape (int shape)
{
    const int n = juce::jlimit (0, 2, shape);
    if (n != mReleaseShape) { mReleaseShape = n; recalcEnvelopeCoefs(); }
}

void TransientShaperDSP::setSplitFreq (float hz)
{
    const float n = juce::jlimit (5.0f, 1000.0f, hz);
    if (n != mSplitFreq)
    {
        mSplitFreq = n;
        mSplitFreqSmooth.setTargetValue (n);
        mLR_LP.setCutoffFrequency (n);
        mLR_HP.setCutoffFrequency (n);
    }
}

void TransientShaperDSP::setSplitBalance (float bal)
{
    const float n = juce::jlimit (-100.0f, 100.0f, bal);
    if (n != mSplitBalance)
    {
        mSplitBalance = n;
        mBalanceSmooth.setTargetValue ((n + 100.0f) / 200.0f);
    }
}

void TransientShaperDSP::setDrive (float drive)
{
    const float n = juce::jlimit (0.0f, 10.0f, drive);
    if (n != mDrive) { mDrive = n; mDriveSmooth.setTargetValue (n); }
}

void TransientShaperDSP::setGain (float dB)
{
    if (dB != mOutGainDb)
    {
        mOutGainDb = dB;
        mOutGainSmooth.setTargetValue (juce::Decibels::decibelsToGain (dB));
    }
}

void TransientShaperDSP::setOsLog2 (int factorLog2)
{
    // C1
    const int n = juce::jlimit (1, 4, factorLog2);
    if (n == mOsLog2) return;
    mOsLog2 = n;
    if (mSampleRate > 0.0)
        prepare (mSampleRate, mMaxBlock);
}

void TransientShaperDSP::setStereoDetect (bool on)
{
    // C2
    if (on != mStereoDetect)
    {
        mStereoDetect = on;
        // Reset R envelope state when switching modes so stale L values don't leak.
        mFastR   = 0.0f;
        mSlowMsR = 0.0f;
    }
}

void TransientShaperDSP::setWet (float wet)
{
    const float n = juce::jlimit (0.0f, 1.0f, wet);
    if (n != mWet) { mWet = n; mWetSmooth.setTargetValue (n); }
}

void TransientShaperDSP::setFastRelMs (float ms)
{
    const float n = juce::jlimit (1.0f, 50.0f, ms);
    if (n != mFastRelMs) { mFastRelMs = n; recalcEnvelopeCoefs(); }
}

void TransientShaperDSP::setSlowAttMs (float ms)
{
    const float n = juce::jlimit (1.0f, 50.0f, ms);
    if (n != mSlowAttMs) { mSlowAttMs = n; recalcEnvelopeCoefs(); }
}

// -- Process ------------------------------------------------------------------

void TransientShaperDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;

    juce::ScopedNoDenormals noDenormals;   // A1

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0) return;
    if (numSamples > mMaxBlock) return;
    if (mOversampler == nullptr) return;
    if (mBandBuf.getNumSamples() < numSamples) return;

    const int numCh = std::min (buffer.getNumChannels(), 2);

    // C.4: external sidechain override for detector path. Audio path (band-split,
    // gain application, drive) still operates on `buffer`; only the level used
    // to drive the fast+slow envelopes comes from SC when present.
    const juce::AudioBuffer<float>* extSc = getActiveSidechain();
    const float* extScL = nullptr;
    const float* extScR = nullptr;
    if (extSc != nullptr && extSc->getNumSamples() >= numSamples && extSc->getNumChannels() > 0)
    {
        extScL = extSc->getReadPointer (0);
        extScR = (extSc->getNumChannels() > 1) ? extSc->getReadPointer (1) : extScL;
    }

    const float fastAtt = mFastAttCoef;
    const float fastRel = mFastRelCoef;
    const float slowAtt = mSlowAttCoef;
    const float slowRel = mSlowRelCoef;

    // ---- Phase 0: consume smoothers for values needed in Phase 3 ------------
    // Attack/Sustain/Sensitivity/Balance are consumed per sample in Phase 1.
    // Wet/OutGain are consumed per sample in Phase 3; stash into scratch.
    for (int n = 0; n < numSamples; ++n)
    {
        mWetScr    [(size_t) n] = mWetSmooth    .getNextValue();
        mOutGainScr[(size_t) n] = mOutGainSmooth.getNextValue();
    }

    // SplitFreq smooths via the filter setter (per-block, cheap).
    mLR_LP.setCutoffFrequency (mSplitFreqSmooth.getNextValue());
    mLR_HP.setCutoffFrequency (mSplitFreqSmooth.getCurrentValue());

    // ---- Phase 1: detection + band-split + gain -> mBandBuf -----------------
    for (int n = 0; n < numSamples; ++n)
    {
        const float attack    = mAttackSmooth     .getNextValue();
        const float sustain   = mReleaseSmooth    .getNextValue();
        const float sensScale = 1.0f + mSensitivitySmooth.getNextValue() * 9.0f;
        const float highW     = juce::jlimit (0.0f, 1.0f, mBalanceSmooth.getNextValue());

        // Detection - level per channel or mono sum (C2)
        // We need PER-CHANNEL gain computation anyway (so stereo material can have
        // asymmetric transients when C2 is on). When C2 is off, we apply the same
        // mono gain to both channels.

        auto computeGain = [&](float level, float& fastState, float& slowMsState) -> float
        {
            // Fast envelope - peak follower
            const float fastCoef = (level > fastState) ? fastAtt : fastRel;
            fastState = fastCoef * fastState + (1.0f - fastCoef) * level;

            // Slow envelope - RMS follower (11e)
            const float levelSq = level * level;
            const float slowCoef = (levelSq > slowMsState) ? slowAtt : slowRel;
            slowMsState = slowCoef * slowMsState + (1.0f - slowCoef) * levelSq;
            const float slowRms = std::sqrt (juce::jmax (slowMsState, 1.0e-12f));

            // Transient = fast - slowRms (spec: keep as difference, not ratio)
            const float transient = fastState - slowRms;
            const float tClamp = juce::jlimit (0.0f, 1.0f, transient * sensScale);
            const float sClamp = juce::jlimit (0.0f, 1.0f, slowRms  * sensScale);

            // 11a: quadratic curves (was linear)
            const float tSq = tClamp * tClamp;
            const float sSq = sClamp * sClamp;
            float g = 1.0f + attack * tSq + sustain * sSq;
            return juce::jmax (0.0f, g);
        };

        float gainL = 0.0f, gainR = 0.0f;

        if (mStereoDetect && numCh > 1)
        {
            // Per-channel detection - prefer external SC if connected
            const float levL = (extScL != nullptr) ? std::abs (extScL[n])
                                                   : std::abs (buffer.getSample (0, n));
            const float levR = (extScR != nullptr) ? std::abs (extScR[n])
                                                   : std::abs (buffer.getSample (1, n));
            gainL = computeGain (levL, mFastL, mSlowMs);
            gainR = computeGain (levR, mFastR, mSlowMsR);
        }
        else
        {
            // Mono-sum detection (default / v1 behavior)
            float level = 0.0f;
            if (extScL != nullptr)
            {
                // SC path: same channel-averaging behavior over the SC buffer
                level = std::abs (extScL[n]);
                if (extScR != nullptr && extScR != extScL)
                {
                    level = (level + std::abs (extScR[n])) * 0.5f;
                }
            }
            else
            {
                for (int ch = 0; ch < numCh; ++ch)
                    level += std::abs (buffer.getSample (ch, n));
                if (numCh > 0) level /= (float) numCh;
            }

            gainL = computeGain (level, mFastL, mSlowMs);
            gainR = gainL;   // same gain on both channels
        }

        // Band-split + shape: LR4 LP + HP, apply gain to high band with balance crossfade.
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float samp = buffer.getSample (ch, n);
            const float lo = mLR_LP.processSample (ch, samp);
            const float hi = mLR_HP.processSample (ch, samp);

            const float g = (ch == 0) ? gainL : gainR;
            const float hiShaped = hi * g;
            const float hiOut    = hi * (1.0f - highW) + hiShaped * highW;

            mBandBuf.setSample (ch, n, lo + hiOut);
        }

        // Pad mono -> stereo in scratch so oversampler sees 2 channels.
        if (numCh < 2)
            mBandBuf.setSample (1, n, mBandBuf.getSample (0, n));
    }

    // ---- Phase 2: oversampled drive saturation (11c) ------------------------
    // Always runs the oversampler for constant-latency PDC. Drive=0 is a near-no-op
    // (oversampler + identity shaper); still goes through up/down sample chain.
    juce::dsp::AudioBlock<float> bandBlock (mBandBuf.getArrayOfWritePointers(),
                                             (size_t) 2, 0, (size_t) numSamples);
    auto upBlock = mOversampler->processSamplesUp (bandBlock);
    const int upN = (int) upBlock.getNumSamples();

    // Drive smoothing at the OS rate: advance base-rate smoother per base sample
    // and hold the value constant across the OS inner loop.
    // We snapshot one drive value per base sample into a small scratch read.
    for (int ch = 0; ch < 2; ++ch)
    {
        float* up = upBlock.getChannelPointer ((size_t) ch);
        // Reset smoother read cursor via getCurrentValue for ch 0 only; ch 1 should use
        // the same drive value as ch 0, so we walk once and reuse. Easiest: do both
        // channels inside a single outer loop over base samples. Restructure:
        juce::ignoreUnused (up);
    }

    // Restructured Phase 2: walk base samples outside, OS samples inside.
    {
        float* L = upBlock.getChannelPointer (0);
        float* R = upBlock.getChannelPointer (1);
        const int kOsNow = (int) upBlock.getNumSamples() / juce::jmax (1, numSamples);

        // Use a fresh smoother read per base sample so Drive still rides the 15 ms ramp.
        for (int n = 0; n < numSamples; ++n)
        {
            const float drive = mDriveSmooth.getNextValue();
            const bool  active = (drive > 0.001f);
            const float d = 1.0f + drive;
            const float invD = 1.0f / juce::jmax (1.0e-6f, d);

            const int baseOff = n * kOsNow;
            if (active)
            {
                for (int k = 0; k < kOsNow; ++k)
                {
                    L[baseOff + k] = std::tanh (d * L[baseOff + k]) * invD;
                    R[baseOff + k] = std::tanh (d * R[baseOff + k]) * invD;
                }
            }
            // When drive == 0, samples pass through unchanged; up/down sample chain still
            // runs, so latency is constant.
        }
        juce::ignoreUnused (upN);
    }

    mOversampler->processSamplesDown (bandBlock);   // mBandBuf now holds shaped+driven signal

    // ---- Phase 3: wet/dry mix + output gain ---------------------------------
    for (int ch = 0; ch < numCh; ++ch)
    {
        float*       out     = buffer.getWritePointer (ch);
        const float* shaped  = mBandBuf.getReadPointer (ch);

        for (int n = 0; n < numSamples; ++n)
        {
            const float dry    = out[n];
            const float wet    = juce::jlimit (0.0f, 1.0f, mWetScr[(size_t) n]);
            const float outLin = mOutGainScr[(size_t) n];
            out[n] = (dry * (1.0f - wet) + shaped[n] * wet) * outLin;
        }
    }
}

// -- Serialisation ------------------------------------------------------------

void TransientShaperDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("TransientShaperDSP");
    state.setProperty ("attack",       mAttack,       nullptr);
    state.setProperty ("sustain",      mRelease,      nullptr);
    state.setProperty ("sensitivity",  mSensitivity,  nullptr);
    state.setProperty ("attackShape",  mAttackShape,  nullptr);
    state.setProperty ("releaseShape", mReleaseShape, nullptr);
    state.setProperty ("splitFreq",    mSplitFreq,    nullptr);
    state.setProperty ("splitBalance", mSplitBalance, nullptr);
    state.setProperty ("drive",        mDrive,        nullptr);
    state.setProperty ("outGainDb",    mOutGainDb,    nullptr);
    // C1-C4 new params
    state.setProperty ("osLog2",       mOsLog2,       nullptr);
    state.setProperty ("stereoDetect", mStereoDetect, nullptr);
    state.setProperty ("wet",          mWet,          nullptr);
    state.setProperty ("fastRelMs",    mFastRelMs,    nullptr);
    state.setProperty ("slowAttMs",    mSlowAttMs,    nullptr);
    auto xml = state.createXml();
    if (xml) juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void TransientShaperDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (!xml || xml->getTagName() != "TransientShaperDSP") return;

    mAttack       = (float) xml->getDoubleAttribute ("attack",       mAttack);
    mRelease      = (float) xml->getDoubleAttribute ("sustain",      mRelease);
    mSensitivity  = (float) xml->getDoubleAttribute ("sensitivity",  mSensitivity);
    mAttackShape  = juce::jlimit (0, 2, xml->getIntAttribute ("attackShape",  mAttackShape));
    mReleaseShape = juce::jlimit (0, 2, xml->getIntAttribute ("releaseShape", mReleaseShape));
    mSplitFreq    = (float) xml->getDoubleAttribute ("splitFreq",    mSplitFreq);
    mSplitBalance = (float) xml->getDoubleAttribute ("splitBalance", mSplitBalance);
    mDrive        = (float) xml->getDoubleAttribute ("drive",        mDrive);
    mOutGainDb    = (float) xml->getDoubleAttribute ("outGainDb",    mOutGainDb);
    const int loadedOs = xml->getIntAttribute ("osLog2", mOsLog2);
    const bool osChanged = (loadedOs != mOsLog2);
    mOsLog2       = juce::jlimit (1, 4, loadedOs);
    mStereoDetect =         xml->getBoolAttribute   ("stereoDetect", mStereoDetect);
    mWet          = (float) xml->getDoubleAttribute ("wet",          mWet);
    mFastRelMs    = (float) xml->getDoubleAttribute ("fastRelMs",    mFastRelMs);
    mSlowAttMs    = (float) xml->getDoubleAttribute ("slowAttMs",    mSlowAttMs);

    recalcEnvelopeCoefs();
    snapSmoothedToTargets();
    if (mSampleRate > 0.0)
    {
        if (osChanged) prepare (mSampleRate, mMaxBlock);
        mLR_LP.setCutoffFrequency (mSplitFreq);
        mLR_HP.setCutoffFrequency (mSplitFreq);
        reset();
    }
}
