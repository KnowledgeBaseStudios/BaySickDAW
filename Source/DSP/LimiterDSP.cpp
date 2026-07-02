#include "LimiterDSP.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kMaxAheadMs = 10.0f;
    constexpr int   kOsFactor   = 4;      // 4× oversampling for TP detection
    constexpr float kRelFastMs  = 20.0f;  // auto-release fast envelope time const
    constexpr float kRelSlowMs  = 300.0f; // auto-release slow envelope time const
}

// ─────────────────────────────────────────────────────────────────────────────
LimiterDSP::LimiterDSP()
{
    mScHpfL.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    mScHpfR.setType (juce::dsp::StateVariableTPTFilterType::highpass);
}

void LimiterDSP::updateScHpfCoefs()
{
    if (mSampleRate <= 0.0) return;
    const float fc = juce::jlimit (20.0f, 2000.0f, mSidechainHPF);
    mScHpfL.setCutoffFrequency (fc);
    mScHpfR.setCutoffFrequency (fc);
    // Butterworth-ish: transparent rolloff
    mScHpfL.setResonance (0.7071f);
    mScHpfR.setResonance (0.7071f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Soft saturator: unity small-signal gain, tanh soft-clip with knee curve.
// drive  = 0       → identity (saturator off)
// drive  > 0       → tanh(drive*x) / drive  (unity gain at small x, soft-clips toward 1/drive)
// curve  ∈ [0, 1]  → blends between linear-ish (curve=0) and heavier saturation (curve=1)
float LimiterDSP::softSat (float x, float drive, float curve)
{
    if (drive < 0.001f) return x;
    // curve shapes the effective drive: higher curve → steeper knee
    const float effDrive = drive * (1.0f + curve * 3.0f);
    return std::tanh (effDrive * x) / effDrive;
}

// ─────────────────────────────────────────────────────────────────────────────
void LimiterDSP::recalcCoefs()
{
    if (mSampleRate <= 0.0) return;

    const double sr = mSampleRate;
    // Exponential time-constant: y[n] = coef*y[n-1] + (1-coef)*target  → 63% in timeMs
    auto expCoef = [sr] (float ms)
    {
        ms = std::max (0.01f, ms);
        return (float) std::exp (-1.0 / ((double) ms * 0.001 * sr));
    };

    mAttackCoef  = expCoef (mAttackMs);
    mReleaseCoef = expCoef (mReleaseMs);
    mRelFastCoef = expCoef (kRelFastMs);
    mRelSlowCoef = expCoef (kRelSlowMs);

    // Linear-release step sizes (linear decay toward target per sample, in linear units):
    // step = 1 / (releaseMs * 0.001 * sr)  → env falls from 1→0 over releaseMs
    auto linStep = [sr] (float ms)
    {
        const float n = std::max (1.0f, (float)(ms * 0.001 * sr));
        return 1.0f / n;
    };
    mRelStepPerSample = linStep (mReleaseMs);
    mRelStepFast      = linStep (kRelFastMs);
    mRelStepSlow      = linStep (kRelSlowMs);

    // SUSTAIN RMS-window coef (0 = off -> coef 0 -> meanSq tracks the peak
    // instantly, so blendedPeak == peak and the hold is a no-op).
    mSusCoef = (mSustainMs <= 0.0f) ? 0.0f : expCoef (mSustainMs);
}

// ─────────────────────────────────────────────────────────────────────────────
void LimiterDSP::allocateDelay()
{
    if (mSampleRate <= 0.0) return;
    const int maxAheadSamp = (int) std::ceil ((double) kMaxAheadMs * 0.001 * mSampleRate);
    // Size = max look-ahead + one full block + safety, so the per-block write pass
    // can never overwrite delayed samples still needed by the read pass.
    mDelaySize = maxAheadSamp + juce::jmax (1, mMaxBlock) + 4;
    mDelayL.assign ((size_t) mDelaySize, 0.0f);
    mDelayR.assign ((size_t) mDelaySize, 0.0f);
    mWritePos = 0;
    mAheadSamples = juce::jlimit (0, maxAheadSamp,
                                  (int) std::round ((double) mAheadMs * 0.001 * mSampleRate));
    mLatencySamples = mAheadSamples + mOsLatencySamples;
}

// ─────────────────────────────────────────────────────────────────────────────
void LimiterDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = juce::jmax (1, maxBlockSize);

    // ── 4× oversampled True Peak detector ─────────────────────────────────────
    // factor=2 → 2^2 = 4× oversampling. Polyphase IIR = low-latency (a few samples).
    mOversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2 /* numChannels */,
        2 /* factor (log2 of ratio): 2 → 4× */,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true /* maxQuality */);
    mOversampler->initProcessing ((size_t) mMaxBlock);
    mOversampler->reset();
    mOsLatencySamples = (int) std::ceil (mOversampler->getLatencyInSamples());

    // ── Detector sidechain buffer (stereo, pre-allocated) ─────────────────────
    mScBuf.setSize (2, mMaxBlock, false, true, true);
    mScBuf.clear();
    mTpPeaks .assign ((size_t) mMaxBlock, 0.0f);
    mTpPeaksL.assign ((size_t) mMaxBlock, 0.0f);
    mTpPeaksR.assign ((size_t) mMaxBlock, 0.0f);

    // ── Look-ahead delay line ─────────────────────────────────────────────────
    allocateDelay();

    // ── SmoothedValues (15 ms linear ramp) ────────────────────────────────────
    mInputGainSmooth.reset (sampleRate, 0.015);
    mCeilingSmooth  .reset (sampleRate, 0.015);
    mSatThreshSmooth.reset (sampleRate, 0.015);
    mSatCurveSmooth .reset (sampleRate, 0.015);   // C1
    mInputGainSmooth.setCurrentAndTargetValue (mInputGainTargetDb);
    mCeilingSmooth  .setCurrentAndTargetValue (mCeilingTargetDb);
    mSatThreshSmooth.setCurrentAndTargetValue (mSatThreshTarget);
    mSatCurveSmooth .setCurrentAndTargetValue (mSatCurve);

    // C2: SC HPF biquads (per-channel, mono spec each)
    juce::dsp::ProcessSpec scSpec { sampleRate,
                                    (juce::uint32) mMaxBlock,
                                    1 };
    mScHpfL.prepare (scSpec);
    mScHpfR.prepare (scSpec);
    mScHpfL.reset();
    mScHpfR.reset();
    updateScHpfCoefs();

    // A2/A3: meter hold+decay (30 dB/sec, SR-aware). Matches InsertNode pattern.
    constexpr float kDecayDbPerSec = 30.0f;
    const float blockSecs = (float) mMaxBlock / (float) sampleRate;
    mLevelDecayDbPerBlock = kDecayDbPerSec * blockSecs;
    mGrDecayDbPerBlock    = kDecayDbPerSec * blockSecs;

    recalcCoefs();
    reset();
}

// ─────────────────────────────────────────────────────────────────────────────
void LimiterDSP::reset()
{
    std::fill (mDelayL.begin(), mDelayL.end(), 0.0f);
    std::fill (mDelayR.begin(), mDelayR.end(), 0.0f);
    mWritePos = 0;
    mEnv = mEnvFast = mEnvSlow = 0.0f;
    mEnvR = mEnvFastR = mEnvSlowR = 0.0f;   // C5
    mMeanSqL = mMeanSqR = 0.0f;
    if (mOversampler) mOversampler->reset();
    mScBuf.clear();
    std::fill (mTpPeaks .begin(), mTpPeaks .end(), 0.0f);
    std::fill (mTpPeaksL.begin(), mTpPeaksL.end(), 0.0f);
    std::fill (mTpPeaksR.begin(), mTpPeaksR.end(), 0.0f);
    mInputDb .store (-96.0f);
    mOutputDb.store (-96.0f);
    mGrDb    .store (0.0f);
    mScHpfL.reset();
    mScHpfR.reset();
}

// ─────────────────────────────────────────────────────────────────────────────
void LimiterDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;

    juce::ScopedNoDenormals noDenormals;   // A1

    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();
    if (numSamples <= 0 || numCh <= 0) return;
    if (numSamples > mMaxBlock) return;     // host exceeded declared block size - drop frame
    // Defensive: if prepare() was somehow skipped, the oversampler / delay
    // line / scratch bufs are not allocated. Skip the block rather than
    // dereference null state.
    if (mOversampler == nullptr) return;
    if (mDelayL.empty() || mDelayR.empty()) return;
    if ((int) mScBuf.getNumSamples() < numSamples) return;
    if ((int) mTpPeaks.size()        < numSamples) return;

    float* L = buffer.getWritePointer (0);
    float* R = (numCh > 1) ? buffer.getWritePointer (1) : nullptr;

    // ── 1. Input gain + soft-sat (write into detector sidechain + delay line) ─
    // We build mScBuf = input after InputGain + SoftSat (i.e. the signal the
    // limiter's gain reduction should protect). The oversampled TP detector
    // reads from this pre-delay buffer. The same samples are written to the
    // circular delay line for output.
    float* scL = mScBuf.getWritePointer (0);
    float* scR = mScBuf.getWritePointer (1);

    // Capture block-invariant delay-line size once (same reason as readPos below)
    const int writeDelaySize = mDelaySize;
    if (writeDelaySize <= 0) return;

    float inPeakLin = 0.0f;
    const bool scHpfActive = (mSidechainHPF > 21.0f);   // only cost cycles when above floor

    // C.4 Phase 2 (2026-04-30): external-key detection.  When an SC source
    // is connected to this slot, the TP detector reads SC samples instead
    // of the limiter's own input.  Audio path (input gain + soft sat +
    // delay line + output) is unchanged -- the limiter still ducks ITS
    // input, but the duck timing is driven by the SC source.
    const juce::AudioBuffer<float>* extSc = getActiveSidechain();
    const float* extScL = nullptr;
    const float* extScR = nullptr;
    if (extSc != nullptr && extSc->getNumSamples() >= numSamples && extSc->getNumChannels() > 0)
    {
        extScL = extSc->getReadPointer(0);
        extScR = (extSc->getNumChannels() > 1) ? extSc->getReadPointer(1) : extScL;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const float gainDb  = mInputGainSmooth.getNextValue();
        const float gainLin = juce::Decibels::decibelsToGain (gainDb);

        const float rawL = L[i] * gainLin;
        const float rawR = (R ? R[i] : rawL) * gainLin;

        // SAT moved post-limiter (FL flow: ... -> Limiter -> Saturation -> out).
        // The detector + limiter run on the clean post-gain signal so the
        // saturation no longer affects the gain reduction (SAT is applied to the
        // limited output in the gain-computer loop below).
        // Detector path: external SC when keyed, otherwise the post-gain input.
        float detL = (extScL != nullptr) ? extScL[i] : rawL;
        float detR = (extScR != nullptr) ? extScR[i] : rawR;
        if (scHpfActive)
        {
            detL = mScHpfL.processSample (0, detL);
            detR = mScHpfR.processSample (0, detR);
        }
        scL[i] = detL;
        scR[i] = detR;

        // Input meter (post-gain - what the limiter sees)
        inPeakLin = juce::jmax (inPeakLin, std::abs (rawL), std::abs (rawR));

        // Write the post-gain (un-saturated) samples to the delay line.
        mDelayL[(size_t) mWritePos] = rawL;
        mDelayR[(size_t) mWritePos] = rawR;
        mWritePos = (mWritePos + 1) % writeDelaySize;
    }

    // ── 2. Upsample detector block, collect per-sample true-peak, downsample ──
    juce::dsp::AudioBlock<float> scBlock (mScBuf.getArrayOfWritePointers(),
                                          2, 0, (size_t) numSamples);
    auto upBlock = mOversampler->processSamplesUp (scBlock);

    const int upN = (int) upBlock.getNumSamples();   // == numSamples * kOsFactor
    jassert (upN == numSamples * kOsFactor);
    // C5: when linked, mTpPeaks[i] = max(|L|,|R|) over this sample's OS group.
    // When unlinked, pack per-channel peaks in two arrays... but to keep
    // downstream loop simple, we reuse mTpPeaks for the linked case and
    // compute per-channel on the fly below in the envelope loop. Here we
    // always write the linked max -- the unlinked path re-derives per-channel
    // peaks from `upBlock` directly below.
    const bool linked = mStereoLink;
    for (int i = 0; i < numSamples; ++i)
    {
        float pMax = 0.0f;
        for (int j = 0; j < kOsFactor; ++j)
        {
            const int k = i * kOsFactor + j;
            if (k >= upN) break;
            pMax = juce::jmax (pMax, std::abs (upBlock.getSample (0, k)));
            pMax = juce::jmax (pMax, std::abs (upBlock.getSample (1, k)));
        }
        mTpPeaks[(size_t) i] = pMax;
    }
    // C5: unlinked mode - pack per-channel peaks so each envelope can run
    // independently below. Uses separate storage (not mScBuf) because the
    // oversampler still needs mScBuf intact for the processSamplesDown pair.
    if (! linked)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float pL = 0.0f, pR = 0.0f;
            for (int j = 0; j < kOsFactor; ++j)
            {
                const int k = i * kOsFactor + j;
                if (k >= upN) break;
                pL = juce::jmax (pL, std::abs (upBlock.getSample (0, k)));
                pR = juce::jmax (pR, std::abs (upBlock.getSample (1, k)));
            }
            mTpPeaksL[(size_t) i] = pL;
            mTpPeaksR[(size_t) i] = pR;
        }
    }
    // Always pair up/down to keep oversampler state coherent. Output is discarded.
    mOversampler->processSamplesDown (scBlock);

    // ── 3. Per-sample envelope + gain reduction + delayed-audio output ────────
    const float attackCoef  = mAttackCoef;
    const float releaseCoef = mReleaseCoef;
    const float relFastCoef = mRelFastCoef;
    const float relSlowCoef = mRelSlowCoef;
    const float relStep     = mRelStepPerSample;
    const float relStepFast = mRelStepFast;
    const float relStepSlow = mRelStepSlow;
    const float curve       = mReleaseCurve;
    const bool  autoRel     = mAutoRelease;

    // Envelope helper: instantaneous-ish attack + user-curve release.
    auto envStep = [attackCoef, releaseCoef, relStep, curve]
                   (float env, float peak) -> float
    {
        if (peak > env)
            return attackCoef * env + (1.0f - attackCoef) * peak;

        // Release: blend linear and exponential decay toward `peak`.
        const float expRel = releaseCoef * env + (1.0f - releaseCoef) * peak;
        float linRel = env - relStep;
        if (linRel < peak) linRel = peak;       // don't overshoot
        return (1.0f - curve) * linRel + curve * expRel;
    };
    auto envStepFast = [attackCoef, relFastCoef, relStepFast, curve]
                       (float env, float peak) -> float
    {
        if (peak > env)
            return attackCoef * env + (1.0f - attackCoef) * peak;
        const float expRel = relFastCoef * env + (1.0f - relFastCoef) * peak;
        float linRel = env - relStepFast;
        if (linRel < peak) linRel = peak;
        return (1.0f - curve) * linRel + curve * expRel;
    };
    auto envStepSlow = [attackCoef, relSlowCoef, relStepSlow, curve]
                       (float env, float peak) -> float
    {
        if (peak > env)
            return attackCoef * env + (1.0f - attackCoef) * peak;
        const float expRel = relSlowCoef * env + (1.0f - relSlowCoef) * peak;
        float linRel = env - relStepSlow;
        if (linRel < peak) linRel = peak;
        return (1.0f - curve) * linRel + curve * expRel;
    };

    float blockGrDb  = 0.0f;   // max GR across block (negative dB)
    float outPeakLin = 0.0f;

    // Capture block-invariant ahead + delay size once so a knob wiggle on the
    // UI thread cannot change them between samples (int writes aren't atomic
    // on all archs, and even torn reads of mAheadSamples could push readPos
    // out of range). Block the per-sample read against exactly one snapshot.
    const int  aheadSamps = mAheadSamples;
    const int  delaySize  = mDelaySize;
    if (delaySize <= 0) return;   // never processed / torn read guard

    const bool autoMakeupOn = mAutoMakeup;     // C4
    const bool linkOn       = mStereoLink;     // C5
    const bool  susOn   = mSusCoef > 0.0f;     // SUSTAIN RMS-window hold
    const float susCoef = mSusCoef;

    // Gain-computer helper: returns the gain to apply given an envelope value.
    auto computeGain = [] (float env, float ceilingLin) -> float
    {
        return (env > ceilingLin) ? (ceilingLin / std::max (env, 1e-9f)) : 1.0f;
    };

    for (int i = 0; i < numSamples; ++i)
    {
        // Read delayed audio (we already advanced mWritePos; read is aheadSamples back)
        const int readPos = (mWritePos - numSamples + i - aheadSamps + delaySize * 2) % delaySize;
        float outL = mDelayL[(size_t) readPos];
        float outR = mDelayR[(size_t) readPos];

        // Consume the ceiling smoother per sample so 15ms ramps apply here too
        const float ceilingDb  = mCeilingSmooth.getNextValue();
        const float ceilingLin = juce::Decibels::decibelsToGain (ceilingDb);

        // SAT (post-limiter, FL-faithful): smoothers consumed here (not in the
        // input loop) so the saturation colors the limited output and never
        // touches the detector/GR.  drive = 0 when SatThresh == 1 (off).
        const float satTh    = mSatThreshSmooth.getNextValue();
        const float satCv    = mSatCurveSmooth .getNextValue();
        const float satDrive = (1.0f - satTh) * 5.0f;

        // C4: Auto-makeup = -ceilingDb of post-limit boost so the output stays
        // hot when the user lowers the ceiling. Skipped when off.
        const float makeupLin = autoMakeupOn
            ? juce::Decibels::decibelsToGain (-ceilingDb)
            : 1.0f;

        // C5: Envelope is either shared (linked) or per-channel (unlinked).
        float gainL = 1.0f, gainR = 1.0f;
        if (linkOn)
        {
            float peak = mTpPeaks[(size_t) i];
            if (susOn)   // SUSTAIN: hold via mean-square window (true-peak preserved by the max)
            {
                mMeanSqL = (1.0f - susCoef) * peak * peak + susCoef * mMeanSqL;
                peak = juce::jmax (peak, std::sqrt (mMeanSqL));
            }
            if (autoRel)
            {
                mEnvFast = envStepFast (mEnvFast, peak);
                mEnvSlow = envStepSlow (mEnvSlow, peak);
                const float env     = juce::jmax (mEnvFast, mEnvSlow);
                const float grLin   = computeGain (env, ceilingLin);
                const float grDbNow = juce::Decibels::gainToDecibels (grLin, -96.0f);
                const float blendSlow = juce::jlimit (0.0f, 1.0f, -grDbNow / 6.0f);
                const float blended = (1.0f - blendSlow) * mEnvFast + blendSlow * mEnvSlow;
                gainL = computeGain (blended, ceilingLin);
            }
            else
            {
                mEnv = envStep (mEnv, peak);
                gainL = computeGain (mEnv, ceilingLin);
            }
            gainR = gainL;   // single envelope drives both
        }
        else
        {
            // Per-channel envelope + gain reduction.
            float peakL = mTpPeaksL[(size_t) i];
            float peakR = mTpPeaksR[(size_t) i];
            if (susOn)   // SUSTAIN: per-channel mean-square hold
            {
                mMeanSqL = (1.0f - susCoef) * peakL * peakL + susCoef * mMeanSqL;
                peakL = juce::jmax (peakL, std::sqrt (mMeanSqL));
                mMeanSqR = (1.0f - susCoef) * peakR * peakR + susCoef * mMeanSqR;
                peakR = juce::jmax (peakR, std::sqrt (mMeanSqR));
            }
            if (autoRel)
            {
                mEnvFast  = envStepFast (mEnvFast,  peakL);
                mEnvSlow  = envStepSlow (mEnvSlow,  peakL);
                mEnvFastR = envStepFast (mEnvFastR, peakR);
                mEnvSlowR = envStepSlow (mEnvSlowR, peakR);
                auto blendOne = [&] (float fast, float slow) -> float
                {
                    const float e     = juce::jmax (fast, slow);
                    const float grLin = computeGain (e, ceilingLin);
                    const float db    = juce::Decibels::gainToDecibels (grLin, -96.0f);
                    const float bs    = juce::jlimit (0.0f, 1.0f, -db / 6.0f);
                    const float b     = (1.0f - bs) * fast + bs * slow;
                    return computeGain (b, ceilingLin);
                };
                gainL = blendOne (mEnvFast,  mEnvSlow);
                gainR = blendOne (mEnvFastR, mEnvSlowR);
            }
            else
            {
                mEnv  = envStep (mEnv,  peakL);
                mEnvR = envStep (mEnvR, peakR);
                gainL = computeGain (mEnv,  ceilingLin);
                gainR = computeGain (mEnvR, ceilingLin);
            }
        }

        // Apply gain reduction + optional auto-makeup
        outL *= gainL * makeupLin;
        outR *= gainR * makeupLin;

        // SAT: post-limiter saturation (FL final stage) -- colors the limited
        // output.  Skipped when off (SatThresh == 1 -> drive 0).
        if (satDrive > 0.0f)
        {
            outL = softSat (outL, satDrive, satCv);
            outR = softSat (outR, satDrive, satCv);
        }

        // Hard ceiling safety clamp (LAST, so the ceiling guarantee holds even
        // after saturation + makeup).
        outL = juce::jlimit (-ceilingLin, ceilingLin, outL);
        outR = juce::jlimit (-ceilingLin, ceilingLin, outR);

        L[i] = outL;
        if (R) R[i] = outR;

        // Meter accumulation (track deepest GR across block, most negative).
        const float grDbSample = juce::Decibels::gainToDecibels (juce::jmin (gainL, gainR), -96.0f);
        if (grDbSample < blockGrDb) blockGrDb = grDbSample;
        outPeakLin = juce::jmax (outPeakLin, std::abs (outL), std::abs (outR));
    }

    // ── 4. Atomic meter updates (once per block, with hold+decay) ─────────────
    // A2/A3: same pattern as InsertNode / Compressor -- max(thisBlock, prev - decay)
    // for level meters (fall toward -96), min(thisBlock, prev + decay) for GR
    // (GR is <=0; rise toward 0).
    const float inDbNow  = juce::Decibels::gainToDecibels (inPeakLin,  -96.0f);
    const float outDbNow = juce::Decibels::gainToDecibels (outPeakLin, -96.0f);
    {
        const float prev = mInputDb.load (std::memory_order_relaxed);
        const float decayed = juce::jmax (-96.0f, prev - mLevelDecayDbPerBlock);
        mInputDb.store (juce::jmax (inDbNow, decayed), std::memory_order_relaxed);
    }
    {
        const float prev = mOutputDb.load (std::memory_order_relaxed);
        const float decayed = juce::jmax (-96.0f, prev - mLevelDecayDbPerBlock);
        mOutputDb.store (juce::jmax (outDbNow, decayed), std::memory_order_relaxed);
    }
    {
        const float prev = mGrDb.load (std::memory_order_relaxed);
        // GR is <=0 (0 = no reduction, -10 = 10dB cut). Decay toward 0.
        const float decayed = juce::jmin (0.0f, prev + mGrDecayDbPerBlock);
        mGrDb.store (juce::jmin (blockGrDb, decayed), std::memory_order_relaxed);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Setters - CPU-guarded per CLAUDE.md rule
// ─────────────────────────────────────────────────────────────────────────────
void LimiterDSP::setInputGainDb (float dB)
{
    const float n = juce::jlimit (-12.0f, 24.0f, dB);
    if (n != mInputGainTargetDb)
    {
        mInputGainTargetDb = n;
        mInputGainSmooth.setTargetValue (n);
    }
}

void LimiterDSP::setCeilingDb (float dB)
{
    const float n = juce::jlimit (-24.0f, 12.0f, dB);
    if (n != mCeilingTargetDb)
    {
        mCeilingTargetDb = n;
        mCeilingSmooth.setTargetValue (n);
    }
}

void LimiterDSP::setSatThresh (float lin)
{
    const float n = juce::jlimit (0.0f, 1.0f, lin);
    if (n != mSatThreshTarget)
    {
        mSatThreshTarget = n;
        mSatThreshSmooth.setTargetValue (n);
    }
}

void LimiterDSP::setSatCurve (float v01)
{
    const float n = juce::jlimit (0.0f, 1.0f, v01);
    if (n != mSatCurve)
    {
        mSatCurve = n;
        mSatCurveSmooth.setTargetValue (n);   // C1
    }
}

void LimiterDSP::setSidechainHPF (float hz)
{
    const float n = juce::jlimit (20.0f, 2000.0f, hz);
    if (n != mSidechainHPF)
    {
        mSidechainHPF = n;
        updateScHpfCoefs();
    }
}

void LimiterDSP::setAutoMakeup (bool on)
{
    if (on != mAutoMakeup) mAutoMakeup = on;
}

void LimiterDSP::setStereoLink (bool on)
{
    if (on != mStereoLink) mStereoLink = on;
}

void LimiterDSP::setAttackMs (float ms)
{
    const float n = juce::jlimit (0.1f, 20.0f, ms);
    if (n != mAttackMs) { mAttackMs = n; recalcCoefs(); }
}

void LimiterDSP::setReleaseMs (float ms)
{
    const float n = juce::jlimit (10.0f, 1000.0f, ms);
    if (n != mReleaseMs) { mReleaseMs = n; recalcCoefs(); }
}

void LimiterDSP::setAheadMs (float ms)
{
    const float n = juce::jlimit (0.0f, kMaxAheadMs, ms);
    if (n != mAheadMs)
    {
        mAheadMs = n;
        if (mSampleRate > 0.0)
        {
            mAheadSamples = juce::jlimit (0, juce::jmax (0, mDelaySize - 1),
                                          (int) std::round ((double) mAheadMs * 0.001 * mSampleRate));
            mLatencySamples = mAheadSamples + mOsLatencySamples;
        }
    }
}

void LimiterDSP::setReleaseCurve (float v01)
{
    const float n = juce::jlimit (0.0f, 1.0f, v01);
    if (n != mReleaseCurve) mReleaseCurve = n;
}

void LimiterDSP::setSustainMs (float ms)
{
    const float n = juce::jlimit (0.0f, 1000.0f, ms);
    if (n != mSustainMs) { mSustainMs = n; recalcCoefs(); }
}

void LimiterDSP::setAutoRelease (bool on)
{
    if (on != mAutoRelease) mAutoRelease = on;
}

// ─────────────────────────────────────────────────────────────────────────────
// Serialisation
// ─────────────────────────────────────────────────────────────────────────────
void LimiterDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("LimiterDSP");
    state.setProperty ("inputGainDb",  mInputGainTargetDb, nullptr);
    state.setProperty ("ceilingDb",    mCeilingTargetDb,   nullptr);
    state.setProperty ("satThresh",    mSatThreshTarget,   nullptr);
    state.setProperty ("satCurve",     mSatCurve,          nullptr);
    state.setProperty ("attackMs",     mAttackMs,          nullptr);
    state.setProperty ("releaseMs",    mReleaseMs,         nullptr);
    state.setProperty ("aheadMs",      mAheadMs,           nullptr);
    state.setProperty ("releaseCurve", mReleaseCurve,      nullptr);
    state.setProperty ("sustainMs",    mSustainMs,         nullptr);
    state.setProperty ("autoRelease",  (int) mAutoRelease, nullptr);
    state.setProperty ("sidechainHPF", mSidechainHPF,      nullptr);   // C2
    state.setProperty ("autoMakeup",   (int) mAutoMakeup,  nullptr);   // C4
    state.setProperty ("stereoLink",   (int) mStereoLink,  nullptr);   // C5
    state.setProperty ("bypassed",     (int) bypassed,     nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml) juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void LimiterDSP::setStateInformation (const void* data, int sz)
{
    std::unique_ptr<juce::XmlElement> xml (juce::AudioProcessor::getXmlFromBinary (data, sz));
    if (! xml) return;
    if (! xml->hasTagName ("LimiterDSP")) return;

    juce::ValueTree state = juce::ValueTree::fromXml (*xml);

    mInputGainTargetDb = state.getProperty ("inputGainDb",  mInputGainTargetDb);
    mCeilingTargetDb   = state.getProperty ("ceilingDb",    mCeilingTargetDb);
    mSatThreshTarget   = state.getProperty ("satThresh",    mSatThreshTarget);
    mSatCurve          = state.getProperty ("satCurve",     mSatCurve);
    mAttackMs          = state.getProperty ("attackMs",     mAttackMs);
    mReleaseMs         = state.getProperty ("releaseMs",    mReleaseMs);
    mAheadMs           = state.getProperty ("aheadMs",      mAheadMs);
    mReleaseCurve      = state.getProperty ("releaseCurve", mReleaseCurve);
    mSustainMs         = (float)(double) state.getProperty ("sustainMs", 0.0);   // default off (old presets)
    mAutoRelease       = ((int) state.getProperty ("autoRelease", 0)) != 0;
    mSidechainHPF      = (float)(double) state.getProperty ("sidechainHPF", 20.0);   // C2
    mAutoMakeup        = ((int) state.getProperty ("autoMakeup", 0)) != 0;           // C4
    mStereoLink        = ((int) state.getProperty ("stereoLink", 1)) != 0;           // C5 (default link on)
    bypassed           = ((int) state.getProperty ("bypassed",    0)) != 0;

    // Snap smoothed values + refresh coefs/delay
    mInputGainSmooth.setCurrentAndTargetValue (mInputGainTargetDb);
    mCeilingSmooth  .setCurrentAndTargetValue (mCeilingTargetDb);
    mSatThreshSmooth.setCurrentAndTargetValue (mSatThreshTarget);
    mSatCurveSmooth .setCurrentAndTargetValue (mSatCurve);   // C1
    updateScHpfCoefs();

    if (mSampleRate > 0.0)
    {
        recalcCoefs();
        // Recompute mAheadSamples from restored mAheadMs without reallocating
        mAheadSamples = juce::jlimit (0, juce::jmax (0, mDelaySize - 1),
                                      (int) std::round ((double) mAheadMs * 0.001 * mSampleRate));
        mLatencySamples = mAheadSamples + mOsLatencySamples;
        // Clear delay / envelope / oversampler state so preset load doesn't pop
        std::fill (mDelayL.begin(), mDelayL.end(), 0.0f);
        std::fill (mDelayR.begin(), mDelayR.end(), 0.0f);
        mWritePos = 0;
        mEnv = mEnvFast = mEnvSlow = 0.0f;
        mEnvR = mEnvFastR = mEnvSlowR = 0.0f;
        mMeanSqL = mMeanSqR = 0.0f;
        if (mOversampler) mOversampler->reset();
        std::fill (mTpPeaks .begin(), mTpPeaks .end(), 0.0f);
        std::fill (mTpPeaksL.begin(), mTpPeaksL.end(), 0.0f);
        std::fill (mTpPeaksR.begin(), mTpPeaksR.end(), 0.0f);
        mScHpfL.reset();
        mScHpfR.reset();
    }
}
