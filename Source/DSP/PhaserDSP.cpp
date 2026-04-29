#include "PhaserDSP.h"
#include <cmath>

namespace
{
    constexpr float kRateSmoothMs     = 20.0f;
    constexpr float kFeedbackSmoothMs = 20.0f;
    constexpr float kFreqSmoothMs     = 30.0f;
    constexpr float kWetSmoothMs      = 15.0f;   // A2
    constexpr float kStereoSmoothMs   = 20.0f;   // A3
    constexpr float kOutGainSmoothMs  = 15.0f;   // A5
    constexpr float kCrossFBSmoothMs  = 20.0f;   // C5
}

// C2: sync-division table. Ordered long-to-short. Index 2 = 1/4 matches v1 hardcoded (BPM/60/4).
const PhaserDSP::SyncDiv PhaserDSP::kSyncDivisions[PhaserDSP::kNumSyncDivisions] =
{
    { "1/1",  1, 1  },  // 0
    { "1/2",  1, 2  },  // 1
    { "1/4",  1, 4  },  // 2  (v1 hardcoded default)
    { "1/8",  1, 8  },  // 3
    { "1/8D", 3, 16 },  // 4  dotted 8th
    { "1/4T", 1, 6  },  // 5  quarter triplet
    { "1/16", 1, 16 },  // 6
    { "1/8T", 1, 12 },  // 7  eighth triplet
};

// ----- Helpers ---------------------------------------------------------------

void PhaserDSP::reapplyBpmSync()
{
    if (! mSyncBPM || mHostBPM <= 0.0) return;
    const int i = juce::jlimit (0, kNumSyncDivisions - 1, mSyncDivIdx);
    const double noteFrac  = (double) kSyncDivisions[i].num
                           / (double) std::max (1, kSyncDivisions[i].den);
    const double periodSec = noteFrac * 4.0 * (60.0 / mHostBPM);
    const float  rate      = juce::jlimit (0.05f, getRateMaxHz(),
                                           (float) (1.0 / std::max (1e-6, periodSec)));
    if (rate != mRate)
    {
        mRate    = rate;
        mSweepHz = rate;
        mRateSmooth.setCurrentAndTargetValue (rate);   // snap on tempo/division change
    }
}

// ----- Setters ---------------------------------------------------------------

void PhaserDSP::setRate (float hz)
{
    const float n = juce::jlimit (0.05f, getRateMaxHz(), hz);
    if (n == mRate && n == mSweepHz) return;
    mRate     = n;
    mSweepHz  = n;
    mRateSmooth.setTargetValue (n);
}

void PhaserDSP::setDepth (float depth)
{
    const float n = juce::jlimit (0.0f, 1.0f, depth);
    if (n == mDepth) return;
    mDepth = n;
    // Legacy bridge: map 0-1 depth to 200-2000 Hz.
    setMinDepth (200.0f * juce::jmax (0.01f, n));
    setMaxDepth (2000.0f);
}

void PhaserDSP::setFeedback (float fb)
{
    const float n = juce::jlimit (-1.2f, 1.2f, fb);
    if (n == mFeedback) return;
    mFeedback = n;
    mFeedbackSmooth.setTargetValue (n);
}

void PhaserDSP::setWet (float wet)
{
    // A2: smoothed Wet to kill zipper on fast drags.
    const float n = juce::jlimit (0.0f, 1.0f, wet);
    if (n != mWet) { mWet = n; mWetSmooth.setTargetValue (n); }
}

void PhaserDSP::setSyncBPM (bool sync)
{
    if (sync == mSyncBPM) return;
    mSyncBPM = sync;
    if (mSyncBPM) reapplyBpmSync();
}

void PhaserDSP::setFreqRange (int range)
{
    // C1: Slow/Fast now actually clamps Rate. 0=Slow (0.05-2 Hz), 1=Fast (0.05-10 Hz).
    const int n = juce::jlimit (0, 1, range);
    if (n == mFreqRange) return;
    mFreqRange = n;
    const float maxHz = getRateMaxHz();
    if (mRate > maxHz || mSweepHz > maxHz)
    {
        mRate    = juce::jmin (mRate,    maxHz);
        mSweepHz = juce::jmin (mSweepHz, maxHz);
        mRateSmooth.setTargetValue (mRate);
    }
    // Re-apply BPM sync against new range (rate may need to clamp down).
    if (mSyncBPM) reapplyBpmSync();
}

void PhaserDSP::setSweepFreq (float hz)
{
    // C1: clamped by current Range (not by fixed 0-10).
    const float n = juce::jlimit (0.0f, getRateMaxHz(), hz);
    if (n == mSweepHz && n == mRate) return;
    mSweepHz = n;
    mRate    = n;
    mRateSmooth.setTargetValue (n);
}

void PhaserDSP::setMinDepth (float hz)
{
    const float n = juce::jlimit (10.0f, 20000.0f, hz);
    if (n != mMinDepthHz) { mMinDepthHz = n; mMinDepthSmooth.setTargetValue (n); }
}

void PhaserDSP::setMaxDepth (float hz)
{
    const float n = juce::jlimit (10.0f, 20000.0f, hz);
    if (n != mMaxDepthHz) { mMaxDepthHz = n; mMaxDepthSmooth.setTargetValue (n); }
}

void PhaserDSP::setStereo (float degrees)
{
    // A3: smoothed stereo phase to kill audible step on drag.
    const float n = juce::jlimit (0.0f, 360.0f, degrees);
    if (n != mStereoPhase)
    {
        mStereoPhase = n;
        mStereoSmooth.setTargetValue (n / 360.0f);   // store as cycle fraction
    }
}

void PhaserDSP::setStages (int numStages)
{
    // sec.7b: pure int assign.
    const int n = juce::jlimit (1, kMaxStages, numStages);
    if (n != mNumStages) mNumStages = n;
}

void PhaserDSP::setOutGain (float dB)
{
    const float n = juce::jlimit (-18.0f, 18.0f, dB);
    if (n != mOutGainDb)
    {
        mOutGainDb = n;
        mOutGainSmooth.setTargetValue (juce::Decibels::decibelsToGain (n));   // A5
    }
}

void PhaserDSP::setInvertFeedback (bool on)
{
    if (on != mInvertFeedback) mInvertFeedback = on;
}

void PhaserDSP::setLFOWave (int waveIdx)
{
    // C3: 0=Sine 1=Triangle 2=Saw 3=S&H
    const int n = juce::jlimit (0, 3, waveIdx);
    if (n != mLFOWaveIdx) mLFOWaveIdx = n;
}

void PhaserDSP::setSyncDiv (int divIdx)
{
    // C2
    const int n = juce::jlimit (0, kNumSyncDivisions - 1, divIdx);
    if (n == mSyncDivIdx) return;
    mSyncDivIdx = n;
    if (mSyncBPM) reapplyBpmSync();
}

void PhaserDSP::setCrossFB (float amount)
{
    // C5: 0 = no cross, 1 = full swap (fb also feeds from the other channel's state).
    const float n = juce::jlimit (0.0f, 1.0f, amount);
    if (n != mCrossFB)
    {
        mCrossFB = n;
        mCrossFBSmooth.setTargetValue (n);
    }
}

// ----- DSPBase interface -----------------------------------------------------

void PhaserDSP::setHostBPM (double bpm)
{
    if (bpm == mHostBPM) return;
    mHostBPM = bpm;
    if (mSyncBPM) reapplyBpmSync();
}

void PhaserDSP::snapSmoothedToTargets()
{
    mRateSmooth    .setCurrentAndTargetValue (mRate);
    mFeedbackSmooth.setCurrentAndTargetValue (mFeedback);
    mMinDepthSmooth.setCurrentAndTargetValue (mMinDepthHz);
    mMaxDepthSmooth.setCurrentAndTargetValue (mMaxDepthHz);
    mWetSmooth     .setCurrentAndTargetValue (mWet);
    mStereoSmooth  .setCurrentAndTargetValue (mStereoPhase / 360.0f);
    mOutGainSmooth .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (mOutGainDb));
    mCrossFBSmooth .setCurrentAndTargetValue (mCrossFB);
}

void PhaserDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;

    mXL.assign ((size_t) kMaxStages, 0.0f);
    mYL.assign ((size_t) kMaxStages, 0.0f);
    mXR.assign ((size_t) kMaxStages, 0.0f);
    mYR.assign ((size_t) kMaxStages, 0.0f);

    mRateSmooth    .reset (sampleRate, kRateSmoothMs     * 0.001);
    mFeedbackSmooth.reset (sampleRate, kFeedbackSmoothMs * 0.001);
    mMinDepthSmooth.reset (sampleRate, kFreqSmoothMs     * 0.001);
    mMaxDepthSmooth.reset (sampleRate, kFreqSmoothMs     * 0.001);
    mWetSmooth     .reset (sampleRate, kWetSmoothMs      * 0.001);
    mStereoSmooth  .reset (sampleRate, kStereoSmoothMs   * 0.001);
    mOutGainSmooth .reset (sampleRate, kOutGainSmoothMs  * 0.001);
    mCrossFBSmooth .reset (sampleRate, kCrossFBSmoothMs  * 0.001);
    snapSmoothedToTargets();

    reset();
}

void PhaserDSP::reset()
{
    if ((int) mXL.size() != kMaxStages)
    {
        mXL.assign ((size_t) kMaxStages, 0.0f);
        mYL.assign ((size_t) kMaxStages, 0.0f);
        mXR.assign ((size_t) kMaxStages, 0.0f);
        mYR.assign ((size_t) kMaxStages, 0.0f);
    }
    else
    {
        std::fill (mXL.begin(), mXL.end(), 0.0f);
        std::fill (mYL.begin(), mYL.end(), 0.0f);
        std::fill (mXR.begin(), mXR.end(), 0.0f);
        std::fill (mYR.begin(), mYR.end(), 0.0f);
    }
    mFbL   = 0.0f;
    mFbR   = 0.0f;
    mPhase = 0.0;

    // C3 S&H state
    mSHL        = 0.0f;
    mSHR        = 0.0f;
    mLastPhaseL = 0.0;
    mLastPhaseR = 0.0;
}

void PhaserDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;

    juce::ScopedNoDenormals noDenormals;   // A1

    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();
    if (numSamples <= 0 || numCh <= 0) return;
    if ((int) mXL.size() < kMaxStages) return;

    float* L = numCh > 0 ? buffer.getWritePointer (0) : nullptr;
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    const float sr           = (float) mSampleRate;
    const float nyqLimit     = sr * 0.499f;
    const float fbSign       = mInvertFeedback ? -1.0f : 1.0f;
    const int   activeStages = juce::jlimit (1, kMaxStages, mNumStages);
    const int   waveIdx      = juce::jlimit (0, 3, mLFOWaveIdx);
    const float invSr        = 1.0f / sr;

    for (int n = 0; n < numSamples; ++n)
    {
        const float rate        = mRateSmooth    .getNextValue();
        const float fb          = juce::jlimit (-1.2f, 1.2f, mFeedbackSmooth.getNextValue()) * fbSign;
        const float minHzRaw    = mMinDepthSmooth.getNextValue();
        const float maxHzRaw    = mMaxDepthSmooth.getNextValue();
        const float wet         = juce::jlimit (0.0f, 1.0f, mWetSmooth.getNextValue());
        const float dry         = 1.0f - wet;
        const float stereoFrac  = mStereoSmooth  .getNextValue();
        const float outLin      = mOutGainSmooth .getNextValue();
        const float xfb         = juce::jlimit (0.0f, 1.0f, mCrossFBSmooth.getNextValue());
        const float minHz       = juce::jlimit (10.0f,  nyqLimit, minHzRaw);
        const float maxHz       = juce::jlimit (minHz,  nyqLimit, maxHzRaw);

        // sec.7a: log-scaled LFO -> frequency mapping
        const float logMinHz = std::log (minHz);
        const float logMaxHz = std::log (maxHz);

        // Per-channel raw LFO phase
        // A6: single-branch wrap replaces std::fmod for stereo-offset path.
        double phaseRaw = mPhase;           // L phase
        double phaseR   = mPhase + (double) stereoFrac;
        if (phaseR >= 1.0) phaseR -= 1.0;

        // C3: evaluate selected LFO wave -> 0..1 output for each channel.
        float lfoL = 0.0f, lfoR = 0.0f;
        switch (waveIdx)
        {
            case 0:  // Sine
                lfoL = 0.5f + 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * phaseRaw);
                lfoR = 0.5f + 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * phaseR);
                break;
            case 1:  // Triangle
                lfoL = 1.0f - 2.0f * std::abs ((float) phaseRaw - 0.5f);
                lfoR = 1.0f - 2.0f * std::abs ((float) phaseR   - 0.5f);
                break;
            case 2:  // Saw (descending: 1 -> 0 over one cycle)
                lfoL = 1.0f - (float) phaseRaw;
                lfoR = 1.0f - (float) phaseR;
                break;
            case 3:  // Sample & Hold
            default:
                if (phaseRaw < mLastPhaseL) mSHL = mSHRng.nextFloat();   // wrap detected
                if (phaseR   < mLastPhaseR) mSHR = mSHRng.nextFloat();
                mLastPhaseL = phaseRaw;
                mLastPhaseR = phaseR;
                lfoL = mSHL;
                lfoR = mSHR;
                break;
        }

        const float centerHzL = std::exp (logMinHz + lfoL * (logMaxHz - logMinHz));
        const float centerHzR = std::exp (logMinHz + lfoR * (logMaxHz - logMinHz));

        // All-pass coefficients (one per channel)
        const float tL  = std::tan (juce::MathConstants<float>::pi * centerHzL / sr);
        const float a1L = (tL - 1.0f) / (tL + 1.0f);
        const float tR  = std::tan (juce::MathConstants<float>::pi * centerHzR / sr);
        const float a1R = (tR - 1.0f) / (tR + 1.0f);

        const float inL = L ? L[n] : 0.0f;
        const float inR = R ? R[n] : inL;

        // C5: cross-channel feedback. xfb=0 -> same as v1; xfb=1 -> feedback fully swaps.
        // Use a linear blend: primary = 1-xfb*0.5, cross = xfb*0.5 (conserves feedback energy).
        const float mainFb  = 1.0f - 0.5f * xfb;
        const float crossFb = 0.5f * xfb;

        float procL = inL + fb * (mainFb * mFbL + crossFb * mFbR);
        float procR = inR + fb * (mainFb * mFbR + crossFb * mFbL);

        // Process through N active all-pass stages
        for (int s = 0; s < activeStages; ++s)
        {
            const float yL = a1L * procL + mXL[(size_t) s] - a1L * mYL[(size_t) s];
            const float yR = a1R * procR + mXR[(size_t) s] - a1R * mYR[(size_t) s];

            mXL[(size_t) s] = procL;  mYL[(size_t) s] = yL;
            mXR[(size_t) s] = procR;  mYR[(size_t) s] = yR;

            procL = yL;
            procR = yR;
        }

        procL = juce::jlimit (-4.0f, 4.0f, procL);
        procR = juce::jlimit (-4.0f, 4.0f, procR);

        mFbL = procL;
        mFbR = procR;

        const float outL = (dry * inL + wet * procL) * outLin;
        const float outR = (dry * inR + wet * procR) * outLin;

        if (L) L[n] = outL;
        if (R) R[n] = outR;

        // A4: while-wrap replaces std::fmod (cheaper; the added delta is always > 0).
        mPhase += (double) rate * (double) invSr;
        while (mPhase >= 1.0) mPhase -= 1.0;
    }
}

// ----- Serialisation ---------------------------------------------------------

void PhaserDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("PhaserDSP");
    state.setProperty ("rate",           mRate,           nullptr);
    state.setProperty ("depth",          mDepth,          nullptr);
    state.setProperty ("feedback",       mFeedback,       nullptr);
    state.setProperty ("wet",            mWet,            nullptr);
    state.setProperty ("syncBPM",        mSyncBPM,        nullptr);
    state.setProperty ("freqRange",      mFreqRange,      nullptr);
    state.setProperty ("sweepHz",        mSweepHz,        nullptr);
    state.setProperty ("minDepthHz",     mMinDepthHz,     nullptr);
    state.setProperty ("maxDepthHz",     mMaxDepthHz,     nullptr);
    state.setProperty ("stereoPhase",    mStereoPhase,    nullptr);
    state.setProperty ("numStages",      mNumStages,      nullptr);
    state.setProperty ("outGainDb",      mOutGainDb,      nullptr);
    state.setProperty ("invertFeedback", (int) mInvertFeedback, nullptr);
    state.setProperty ("lfoWaveIdx",     mLFOWaveIdx,     nullptr);   // C3
    state.setProperty ("syncDivIdx",     mSyncDivIdx,     nullptr);   // C2
    state.setProperty ("crossFB",        mCrossFB,        nullptr);   // C5
    auto xml = state.createXml();
    if (xml) juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void PhaserDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (!xml || xml->getTagName() != "PhaserDSP") return;

    mRate           = (float) xml->getDoubleAttribute ("rate",           mRate);
    mDepth          = (float) xml->getDoubleAttribute ("depth",          mDepth);
    mFeedback       = (float) xml->getDoubleAttribute ("feedback",       mFeedback);
    mWet            = (float) xml->getDoubleAttribute ("wet",            mWet);
    mSyncBPM        =         xml->getBoolAttribute   ("syncBPM",        mSyncBPM);
    mFreqRange      = juce::jlimit (0, 1, xml->getIntAttribute ("freqRange", mFreqRange));
    mSweepHz        = (float) xml->getDoubleAttribute ("sweepHz",        mSweepHz);
    mMinDepthHz     = (float) xml->getDoubleAttribute ("minDepthHz",     mMinDepthHz);
    mMaxDepthHz     = (float) xml->getDoubleAttribute ("maxDepthHz",     mMaxDepthHz);
    mStereoPhase    = (float) xml->getDoubleAttribute ("stereoPhase",    mStereoPhase);
    mNumStages      = juce::jlimit (1, kMaxStages,
                                    xml->getIntAttribute ("numStages",  mNumStages));
    mOutGainDb      = (float) xml->getDoubleAttribute ("outGainDb",      mOutGainDb);
    mInvertFeedback =        (xml->getIntAttribute    ("invertFeedback", mInvertFeedback ? 1 : 0)) != 0;
    mLFOWaveIdx     = juce::jlimit (0, 3,
                                    xml->getIntAttribute ("lfoWaveIdx", mLFOWaveIdx));
    mSyncDivIdx     = juce::jlimit (0, kNumSyncDivisions - 1,
                                    xml->getIntAttribute ("syncDivIdx", mSyncDivIdx));
    mCrossFB        = (float) xml->getDoubleAttribute ("crossFB", mCrossFB);

    // P2: reconcile rate aliasing.
    if (mSweepHz != mRate) mRate = mSweepHz;

    // C1: respect restored Range by clamping Rate down if needed.
    const float maxHz = getRateMaxHz();
    if (mRate    > maxHz) mRate    = maxHz;
    if (mSweepHz > maxHz) mSweepHz = maxHz;

    // P4: snap smoothed values to restored targets + clear filter state.
    snapSmoothedToTargets();
    if (mSampleRate > 0.0 && (int) mXL.size() == kMaxStages)
    {
        std::fill (mXL.begin(), mXL.end(), 0.0f);
        std::fill (mYL.begin(), mYL.end(), 0.0f);
        std::fill (mXR.begin(), mXR.end(), 0.0f);
        std::fill (mYR.begin(), mYR.end(), 0.0f);
        mFbL = mFbR = 0.0f;
    }
    mSHL = mSHR = 0.0f;
    mLastPhaseL = mLastPhaseR = 0.0;
}
