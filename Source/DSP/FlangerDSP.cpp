#include "FlangerDSP.h"

// C4: sync-division table. Ordered long-to-short. Index 3 = 1/8 (v1 default).
// `num/den` expresses the LFO cycle length in whole notes.
const FlangerDSP::SyncDiv FlangerDSP::kSyncDivisions[FlangerDSP::kNumSyncDivisions] =
{
    { "1/1",  1, 1  },  // 0
    { "1/2",  1, 2  },  // 1
    { "1/4",  1, 4  },  // 2
    { "1/8",  1, 8  },  // 3  (v1 hardcoded default)
    { "1/8D", 3, 16 },  // 4  dotted 8th
    { "1/4T", 1, 6  },  // 5  quarter triplet
    { "1/16", 1, 16 },  // 6
    { "1/8T", 1, 12 },  // 7  eighth triplet
};

void FlangerDSP::setRate (float hz)
{
    const float n = juce::jlimit (0.05f, 5.0f, hz);
    mManualRate = n;            // (c) shadow for un-sync restore
    if (mSyncBPM) return;       // while synced, the BPM derivation owns mRate
    if (n != mRate) { mRate = n; mRateSmooth.setTargetValue (n); }
}
void FlangerDSP::setDepth (float ms)
{
    const float n = juce::jlimit (0.0f, 10.0f, ms);
    if (n != mDepth) { mDepth = n; mDepthSmooth.setTargetValue (n); }
}
void FlangerDSP::setDelay (float ms)
{
    // A3 -- Smoothed 20 ms ramp eliminates knob-drag clicks at short base delays.
    const float n = juce::jlimit (0.0f, 20.0f, ms);
    if (n != mDelay) { mDelay = n; mDelaySmooth.setTargetValue (n); }
}
void FlangerDSP::setFeedback (float fb)
{
    const float n = juce::jlimit (-1.0f, 1.0f, fb);
    if (n != mFeedback) { mFeedback = n; mFeedbackSmooth.setTargetValue (n); }
}
void FlangerDSP::setFeed (float pct)
{
    const float n = juce::jlimit (-1.0f, 1.0f, pct / 100.0f);
    if (n != mFeedback) { mFeedback = n; mFeedbackSmooth.setTargetValue (n); }
}
void FlangerDSP::setWet (float wet)
{
    // A2 -- Smoothed Wet ramps blend changes to kill zipper on fast drags.
    const float n = juce::jlimit (0.0f, 1.0f, wet);
    if (n != mWet) { mWet = n; mWetSmooth.setTargetValue (n); }
}
void FlangerDSP::setSyncBPM (bool sync)
{
    if (sync != mSyncBPM)
    {
        mSyncBPM = sync;
        // Re-derive rate from BPM using currently-selected division.
        if (mSyncBPM && mHostBPM > 0.0)
        {
            const int i = juce::jlimit (0, kNumSyncDivisions - 1, mSyncDivIdx);
            const double noteFrac = (double) kSyncDivisions[i].num
                                  / (double) std::max (1, kSyncDivisions[i].den);
            // LFO period = noteFrac whole notes; 4 beats per whole note.
            const double periodSec = noteFrac * 4.0 * (60.0 / mHostBPM);
            mRate = (float) (1.0 / std::max (1e-6, periodSec));
            mRateSmooth.setCurrentAndTargetValue (mRate);   // F5: snap on toggle
        }
        else if (! mSyncBPM)
        {
            // (c) un-sync: restore the manual rate the user set before sync
            // (was a no-op, leaving mRate stuck at the synced value).
            mRate = juce::jlimit (0.05f, 5.0f, mManualRate);
            mRateSmooth.setCurrentAndTargetValue (mRate);
        }
    }
}

void FlangerDSP::setSyncDivision (int divIdx)
{
    const int n = juce::jlimit (0, kNumSyncDivisions - 1, divIdx);
    if (n != mSyncDivIdx)
    {
        mSyncDivIdx = n;
        if (mSyncBPM && mHostBPM > 0.0)
        {
            const double noteFrac = (double) kSyncDivisions[n].num
                                  / (double) std::max (1, kSyncDivisions[n].den);
            const double periodSec = noteFrac * 4.0 * (60.0 / mHostBPM);
            mRate = (float) (1.0 / std::max (1e-6, periodSec));
            mRateSmooth.setCurrentAndTargetValue (mRate);   // snap, like tempo change
        }
    }
}
void FlangerDSP::setPhase (float deg)
{
    const float n = juce::jlimit (0.0f, 360.0f, deg);   // F4: clamp
    if (n != mStereoPhase) mStereoPhase = n;
}
void FlangerDSP::setDampHz (float hz)
{
    // C3 -- Feedback-damp LPF cutoff in Hz (200..20000; 20 kHz ~= transparent).
    const float n = juce::jlimit (200.0f, 20000.0f, hz);
    if (n != mDampHz)
    {
        mDampHz = n;
        recomputeDampAlpha();
    }
}
void FlangerDSP::setShape (float s)
{
    // A4 -- Smoothed Shape ramps sine<->triangle blend on fast drags.
    const float n = juce::jlimit (0.0f, 1.0f, s);
    if (n != mShape) { mShape = n; mShapeSmooth.setTargetValue (n); }
}
void FlangerDSP::setInvertFeedback (bool b)
{
    if (b != mInvertFeedback) mInvertFeedback = b;
}
void FlangerDSP::setInvertWet (bool b)
{
    if (b != mInvertWet) mInvertWet = b;
}
void FlangerDSP::setCrossLevel (float dB)
{
    if (dB != mCrossLevelDb) mCrossLevelDb = dB;
}

void FlangerDSP::setHostBPM (double bpm)
{
    if (bpm == mHostBPM) return;
    mHostBPM = bpm;
    if (mSyncBPM && bpm > 0.0)
    {
        // C4 -- use current sync-division selection (default index 3 = 1/8, matching v1).
        const int i = juce::jlimit (0, kNumSyncDivisions - 1, mSyncDivIdx);
        const double noteFrac = (double) kSyncDivisions[i].num
                              / (double) std::max (1, kSyncDivisions[i].den);
        const double periodSec = noteFrac * 4.0 * (60.0 / bpm);
        mRate = (float) (1.0 / std::max (1e-6, periodSec));
        // F5: tempo-driven Rate change should be instant, not glide across the switch
        mRateSmooth.setCurrentAndTargetValue (mRate);
    }
}

void FlangerDSP::recomputeDampAlpha()
{
    // 1-pole LP state-feedback coefficient: a = exp(-2*pi*fc/sr).
    // Bounds 20 Hz .. 0.45*SR: fc <= 0 would give a >= 1 (runaway inside the
    // feedback loop), and the exp mapping only tracks a real cutoff below Nyquist.
    const double sr = (mSampleRate > 0.0) ? mSampleRate : 44100.0;
    const double fc = juce::jlimit (20.0, 0.45 * sr, (double) mDampHz);
    mDampAlpha = (float) std::exp (-juce::MathConstants<double>::twoPi * fc / sr);
}

void FlangerDSP::allocateBuffers()
{
    // Max delay = kMaxDelayMs at current sample rate
    int bufSize = (int)std::ceil (kMaxDelayMs * 0.001 * mSampleRate) + 4;
    mBufL.assign (bufSize, 0.0f);
    mBufR.assign (bufSize, 0.0f);
    mWritePos = 0;
}

void FlangerDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;
    allocateBuffers();

    // 4c + A2/A3/A4: ~20 ms linear ramp on Rate/Depth/Feedback/Wet/Delay/Shape
    const double rampSecs = 0.02;
    mRateSmooth    .reset (sampleRate, rampSecs);
    mDepthSmooth   .reset (sampleRate, rampSecs);
    mFeedbackSmooth.reset (sampleRate, rampSecs);
    mWetSmooth     .reset (sampleRate, rampSecs);
    mDelaySmooth   .reset (sampleRate, rampSecs);
    mShapeSmooth   .reset (sampleRate, rampSecs);
    mRateSmooth    .setCurrentAndTargetValue (mRate);
    mDepthSmooth   .setCurrentAndTargetValue (mDepth);
    mFeedbackSmooth.setCurrentAndTargetValue (mFeedback);
    mWetSmooth     .setCurrentAndTargetValue (mWet);
    mDelaySmooth   .setCurrentAndTargetValue (mDelay);
    mShapeSmooth   .setCurrentAndTargetValue (mShape);

    // C3 -- Derive damp LPF coefficient from Hz cutoff + current SR.
    recomputeDampAlpha();

    reset();
}

void FlangerDSP::reset()
{
    std::fill (mBufL.begin(), mBufL.end(), 0.0f);
    std::fill (mBufR.begin(), mBufR.end(), 0.0f);
    mWritePos   = 0;
    mPhase      = 0.0;
    mFbL        = 0.0f;
    mFbR        = 0.0f;
    mDampStateL = 0.0f;
    mDampStateR = 0.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// 4-point Catmull-Rom cubic read, reading BACK from writePos by dSamp samples.
// y0 = newest (one sample closer to writePos than y1)
// y1 = current integer delay position (where fractional interp starts)
// y2 = one sample older than y1
// y3 = two samples older than y1
// Matches the direction used by the old linear readAt lambda.
float FlangerDSP::cubicReadBack (const std::vector<float>& buf, float dSamp,
                                 int writePos, int bufSize)
{
    if (bufSize == 0) return 0.0f;

    const int   id = (int) dSamp;
    const float f  = dSamp - (float) id;

    auto idx = [=] (int off) {
        return (writePos - id - off + bufSize * 2) % bufSize;
    };

    const float y0 = buf[idx (-1)];
    const float y1 = buf[idx ( 0)];
    const float y2 = buf[idx ( 1)];
    const float y3 = buf[idx ( 2)];

    const float a = -0.5f*y0 + 1.5f*y1 - 1.5f*y2 + 0.5f*y3;
    const float b =        y0 - 2.5f*y1 + 2.0f*y2 - 0.5f*y3;
    const float c = -0.5f*y0 + 0.5f*y2;

    return ((a*f + b)*f + c)*f + y1;
}

void FlangerDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;

    juce::ScopedNoDenormals noDenormals;   // A1

    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();

    float* L = numCh > 0 ? buffer.getWritePointer (0) : nullptr;
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    if (mBufL.empty()) return;

    const float visIn = visualCaptureIn (buffer);   // T18

    const int    bufSize         = (int) mBufL.size();
    // Stereo phase offset for R channel: convert degrees to 0..1 phase fraction
    const double stereoPhaseFrac = (double) mStereoPhase / 360.0;
    const float  dryLin          = juce::Decibels::decibelsToGain (mDryLevelDb);
    const float  wetLin          = juce::Decibels::decibelsToGain (mWetLevelDb);
    const float  crossLin        = (mCrossLevelDb > -90.0f)
                                      ? juce::Decibels::decibelsToGain (mCrossLevelDb)
                                      : 0.0f;
    const bool   invertWet  = mInvertWet;
    const float  fbSign     = mInvertFeedback ? -1.0f : 1.0f;
    const float  dampAlpha  = mDampAlpha;   // C3: cached per block

    for (int n = 0; n < numSamples; ++n)
    {
        // 4c + A2/A3/A4: pull smoothed Rate/Depth/Feedback/Wet/Delay/Shape per-sample
        const float  rate         = mRateSmooth    .getNextValue();
        const float  depth        = mDepthSmooth   .getNextValue();
        const float  feedback     = mFeedbackSmooth.getNextValue();
        const float  curWet       = mWetSmooth     .getNextValue();
        const float  curDelay     = mDelaySmooth   .getNextValue();
        const float  curShape     = mShapeSmooth   .getNextValue();
        const double phaseInc     = (double) rate / mSampleRate;
        const float  sweepSamp    = (float) (depth * 0.001 * mSampleRate);
        const float  baseDelaySamp = (float) (curDelay * 0.001 * mSampleRate);
        const float  fb           = fbSign * feedback;

        // Legacy wet/dry (0-1) blended with new dB levels:
        // If user only uses old mWet knob, mWetLevelDb=0 and mDryLevelDb=0 -> unity gains.
        const float  wetScale   = curWet * wetLin;
        const float  dryScale   = (1.0f - curWet) * dryLin;
        const float  crossScale = curWet * crossLin;

        // LFO evaluation with smoothed shape. F2: skip asin branch when shape ~= 0.
        const bool useTri = curShape > 1.0e-4f;
        auto lfoVal = [useTri, curShape] (double ph) -> float
        {
            const double twoPiPh = juce::MathConstants<double>::twoPi * ph;
            const float  s = 0.5f + 0.5f * (float) std::sin (twoPiPh);
            if (! useTri) return s;
            const float tri = 0.5f + (float) (std::asin (std::sin (twoPiPh))
                                              / juce::MathConstants<double>::pi);
            return s * (1.0f - curShape) + tri * curShape;
        };

        // LFO for L and R channels (R has stereo phase offset)
        const float lfoL = lfoVal (mPhase);
        const float lfoR = lfoVal (std::fmod (mPhase + stereoPhaseFrac, 1.0));

        // Compute delay times in samples: base + LFO-swept depth
        float delaySampL = juce::jlimit (0.0f, (float) (bufSize - 2), baseDelaySamp + lfoL * sweepSamp);
        float delaySampR = juce::jlimit (0.0f, (float) (bufSize - 2), baseDelaySamp + lfoR * sweepSamp);

        const float inL = L ? L[n] : 0.0f;
        const float inR = R ? R[n] : inL;

        // Write to delay buffer (input + feedback from previous output)
        mBufL[mWritePos] = inL + fb * mFbL;
        mBufR[mWritePos] = inR + fb * mFbR;

        // 4b: cubic Catmull-Rom read
        float delL = cubicReadBack (mBufL, delaySampL, mWritePos, bufSize);
        float delR = cubicReadBack (mBufR, delaySampR, mWritePos, bufSize);

        // 4a + C3: apply damp LP INSIDE feedback loop (before capturing feedback).
        // `dampAlpha` is the state-feedback coefficient derived from mDampHz.
        // dampAlpha ~= 0  -> passthrough (cutoff >= SR/2, effectively off)
        // dampAlpha -> 1  -> heavy LP (cutoff near 0)
        if (dampAlpha > 1.0e-4f)
        {
            mDampStateL = dampAlpha * mDampStateL + (1.0f - dampAlpha) * delL;
            mDampStateR = dampAlpha * mDampStateR + (1.0f - dampAlpha) * delR;
            delL = mDampStateL;
            delR = mDampStateR;
        }

        // Capture feedback (post-damp, pre-invertWet so feedback doesn't double-invert)
        mFbL = delL;
        mFbR = delR;

        // Optional invert wet (output only - feedback stays positive-phase via mFbL/R)
        if (invertWet) { delL = -delL; delR = -delR; }

        // Mix: dry + wet + cross
        if (L) L[n] = dryScale * inL + wetScale * delL + crossScale * delR;
        if (R) R[n] = dryScale * inR + wetScale * delR + crossScale * delL;

        mWritePos = (mWritePos + 1) % bufSize;
        mPhase   += phaseInc;
        // A5: while-wrap (was std::fmod). Cheaper for bounded phase increments
        // and avoids a floating-point division per sample.
        while (mPhase >= 1.0) mPhase -= 1.0;
        while (mPhase <  0.0) mPhase += 1.0;
    }

    visualPushInOut (buffer, visIn);   // T18
}

// ── Serialisation ─────────────────────────────────────────────────────────────

void FlangerDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("FlangerDSP");
    state.setProperty ("rate",           mRate,           nullptr);
    state.setProperty ("manualRate",     mManualRate,     nullptr);   // (c)
    state.setProperty ("depth",          mDepth,          nullptr);
    state.setProperty ("delay",          mDelay,          nullptr);
    state.setProperty ("feedback",       mFeedback,       nullptr);
    state.setProperty ("wet",            mWet,            nullptr);
    state.setProperty ("syncBPM",        mSyncBPM,        nullptr);
    state.setProperty ("syncDivIdx",     mSyncDivIdx,     nullptr);   // C4
    state.setProperty ("stereoPhase",    mStereoPhase,    nullptr);
    state.setProperty ("shape",          mShape,          nullptr);
    state.setProperty ("dampHz",         mDampHz,         nullptr);   // C3 (replaces `damp`)
    state.setProperty ("invertFeedback", mInvertFeedback, nullptr);
    state.setProperty ("invertWet",      mInvertWet,      nullptr);
    state.setProperty ("dryLevelDb",     mDryLevelDb,     nullptr);
    state.setProperty ("wetLevelDb",     mWetLevelDb,     nullptr);
    state.setProperty ("crossLevelDb",   mCrossLevelDb,   nullptr);
    auto xml = state.createXml();
    if (xml) juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void FlangerDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (!xml || xml->getTagName() != "FlangerDSP") return;
    mRate           = (float) xml->getDoubleAttribute ("rate",           mRate);
    mManualRate     = (float) xml->getDoubleAttribute ("manualRate",     mRate);   // (c) default to rate for pre-fix projects
    mDepth          = (float) xml->getDoubleAttribute ("depth",          mDepth);
    mDelay          = (float) xml->getDoubleAttribute ("delay",          mDelay);
    mFeedback       = (float) xml->getDoubleAttribute ("feedback",       mFeedback);
    mWet            = (float) xml->getDoubleAttribute ("wet",            mWet);
    mSyncBPM        =         xml->getBoolAttribute   ("syncBPM",        mSyncBPM);
    mSyncDivIdx     =         xml->getIntAttribute    ("syncDivIdx",     mSyncDivIdx);
    mStereoPhase    = (float) xml->getDoubleAttribute ("stereoPhase",    mStereoPhase);
    mShape          = (float) xml->getDoubleAttribute ("shape",          mShape);
    mDampHz         = (float) xml->getDoubleAttribute ("dampHz",         mDampHz);
    mInvertFeedback =         xml->getBoolAttribute   ("invertFeedback", mInvertFeedback);
    mInvertWet      =         xml->getBoolAttribute   ("invertWet",      mInvertWet);
    mDryLevelDb     = (float) xml->getDoubleAttribute ("dryLevelDb",     mDryLevelDb);
    mWetLevelDb     = (float) xml->getDoubleAttribute ("wetLevelDb",     mWetLevelDb);
    mCrossLevelDb   = (float) xml->getDoubleAttribute ("crossLevelDb",   mCrossLevelDb);

    // Clamp division index to table range (defensive, in case state was tampered).
    mSyncDivIdx = juce::jlimit (0, kNumSyncDivisions - 1, mSyncDivIdx);

    // Snap smoothed values to restored targets so preset load doesn't glide audibly
    mRateSmooth    .setCurrentAndTargetValue (mRate);
    mDepthSmooth   .setCurrentAndTargetValue (mDepth);
    mFeedbackSmooth.setCurrentAndTargetValue (mFeedback);
    mWetSmooth     .setCurrentAndTargetValue (mWet);
    mDelaySmooth   .setCurrentAndTargetValue (mDelay);
    mShapeSmooth   .setCurrentAndTargetValue (mShape);

    // C3: re-derive damp alpha from loaded cutoff.
    recomputeDampAlpha();

    // Clear delay / damp / feedback state so preset load doesn't pop
    if (! mBufL.empty())
    {
        std::fill (mBufL.begin(), mBufL.end(), 0.0f);
        std::fill (mBufR.begin(), mBufR.end(), 0.0f);
    }
    mWritePos   = 0;
    mFbL        = 0.0f;
    mFbR        = 0.0f;
    mDampStateL = 0.0f;
    mDampStateR = 0.0f;
}
