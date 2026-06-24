#include "OctaveStyleDSP.h"

namespace
{
    constexpr float kRangeMinHz = 300.0f;
    constexpr float kRangeMaxHz = 3000.0f;

    // QA-EffectsReview Task 5: period-doubler confidence gating.
    constexpr float kConfThreshold = 0.5f;   // YIN confidence to engage the doubler
    constexpr float kConfSmooth    = 0.25f;  // per-block confidence fade coef

    // QA-EffectsReview Task 5 (C3): voicing -- transient duck + shifted-voice LP.
    constexpr float kTransRatio = 1.5f;    // fast-env > slow-env * this -> a transient/attack
    constexpr float kTransFloor = 0.01f;   // ignore tiny levels (don't duck on noise)
    constexpr float kDuckDepth  = 0.4f;    // shifted-voice gain during an attack
}

// ─────────────────────────────────────────────────────────────────────────────
// GranularShifter
// ─────────────────────────────────────────────────────────────────────────────
void OctaveStyleDSP::GranularShifter::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    writePos = 0;
    readPos1 = 0.0;
    readPos2 = (double) (grainSize / 2);
}

float OctaveStyleDSP::GranularShifter::interp (double pos) const noexcept
{
    // Wrap into [0, kBufferSize).
    while (pos < 0.0)            pos += kBufferSize;
    while (pos >= kBufferSize)   pos -= kBufferSize;
    const int   i0 = (int) pos;
    const int   i1 = (i0 + 1) % kBufferSize;
    const float f  = (float) (pos - (double) i0);
    return ring[(size_t) i0] * (1.0f - f) + ring[(size_t) i1] * f;
}

void OctaveStyleDSP::GranularShifter::process (const float* input,
                                                float* output,
                                                int numSamples)
{
    const float ratio = pitchRatio;
    const float invGrain = 1.0f / (float) grainSize;

    for (int i = 0; i < numSamples; ++i)
    {
        ring[(size_t) writePos] = input[i];
        writePos = (writePos + 1) % kBufferSize;

        // Distance from each read head to the write head, in samples.  When
        // ratio < 1 the read heads fall behind; when ratio > 1 they overtake.
        // We jump a head back by grainSize whenever it gets within one grain
        // of the write head (read overtaking write) or whenever it falls
        // more than (kBufferSize - grainSize) behind (write overtaking read).
        auto adjust = [this] (double& rp)
        {
            // Compute signed distance writePos - rp (modulo buffer).
            double diff = (double) writePos - rp;
            while (diff < -kBufferSize / 2) diff += kBufferSize;
            while (diff >  kBufferSize / 2) diff -= kBufferSize;
            // diff > 0  -> read is behind write by 'diff' samples
            // diff < 0  -> read is ahead of write by '-diff' samples
            // For ratio > 1 read overtakes -> diff goes negative -> jump back.
            // For ratio < 1 read falls behind -> diff grows large positive.
            if (diff < (double) grainSize)
                rp -= (double) grainSize;            // jump back, lengthen lag
            else if (diff > (double) (kBufferSize - grainSize))
                rp += (double) grainSize;            // jump forward, shorten lag
            // Keep rp bounded so interp()'s wrap + the diff wrap above stay O(1).
            // Without this rp grows ~1 sample/sample and the wrap loops climb CPU.
            while (rp < 0.0)                   rp += (double) kBufferSize;
            while (rp >= (double) kBufferSize) rp -= (double) kBufferSize;
        };

        adjust (readPos1);
        adjust (readPos2);

        const float s1 = interp (readPos1);
        const float s2 = interp (readPos2);

        // Hann window phase: how far each read head has progressed since its
        // last jump.  We approximate using (writePos - readPos) modulo grain.
        auto windowPhase = [this, invGrain] (double rp) -> float
        {
            double diff = (double) writePos - rp;
            while (diff < 0.0)               diff += kBufferSize;
            while (diff >= kBufferSize)      diff -= kBufferSize;
            // Modulo grainSize -> 0..grainSize, normalise to 0..1.
            const double mod = std::fmod (diff, (double) grainSize);
            return (float) (mod * (double) invGrain);
        };

        const float w1 = hann (windowPhase (readPos1));
        const float w2 = hann (windowPhase (readPos2));

        // Sum of windows is ~1.0 since they're 50% offset Hann pairs.
        output[i] = s1 * w1 + s2 * w2;

        readPos1 += (double) ratio;
        if (readPos1 >= (double) kBufferSize) readPos1 -= (double) kBufferSize;
        readPos2 += (double) ratio;
        if (readPos2 >= (double) kBufferSize) readPos2 -= (double) kBufferSize;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// PeriodDoubler (PSOLA octave-down) -- QA-EffectsReview Task 5
// ─────────────────────────────────────────────────────────────────────────────
void OctaveStyleDSP::PeriodDoubler::reset()
{
    ring.fill (0.0f);
    writePos    = 0;
    readPos     = 0.0;
    xfadeOldPos = 0.0;
    xfadeLeft   = 0;
    grainCount  = 0;
    repeat      = 0;
    period      = pendingPeriod;
}

float OctaveStyleDSP::PeriodDoubler::interp (double pos) const noexcept
{
    while (pos < 0.0)          pos += kRingSize;
    while (pos >= kRingSize)   pos -= kRingSize;
    const int   i0 = (int) pos;
    const int   i1 = (i0 + 1) % kRingSize;
    const float f  = (float) (pos - (double) i0);
    return ring[(size_t) i0] * (1.0f - f) + ring[(size_t) i1] * f;
}

void OctaveStyleDSP::PeriodDoubler::process (const float* input, float* output, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        ring[(size_t) writePos] = input[i];
        writePos = (writePos + 1) % kRingSize;

        // Read the doubled grain, with a short seam crossfade across the jumps.
        float s = interp (readPos);
        if (xfadeLeft > 0)
        {
            const float t = (float) xfadeLeft / (float) kXfade;   // 1 -> 0
            s = s * (1.0f - t) + interp (xfadeOldPos) * t;
            xfadeOldPos += 1.0;
            if (xfadeOldPos >= (double) kRingSize) xfadeOldPos -= (double) kRingSize;
            --xfadeLeft;
        }
        output[i] = s;

        // Keep readPos wrapped into [0,kRingSize).  CRITICAL: if it grows unbounded
        // (~1 sample/sample) the interp() + lag-guard wraps become O(readPos/kRingSize)
        // loops and the CPU climbs the longer you play (and persists across transport
        // pause, since the pointer isn't reset).
        readPos += 1.0;
        if (readPos >= (double) kRingSize) readPos -= (double) kRingSize;
        if (++grainCount >= period)
        {
            grainCount  = 0;
            xfadeOldPos = readPos;     // old head keeps going during the crossfade
            xfadeLeft   = kXfade;
            if (++repeat < repeatsPerGrain)
            {
                readPos -= (double) period;                         // replay this period
                if (readPos < 0.0) readPos += (double) kRingSize;
            }
            else
            {
                repeat  = 0;
                readPos += (double) (repeatsPerGrain - 1) * period; // skip the discarded periods
                while (readPos >= (double) kRingSize) readPos -= (double) kRingSize;
                period  = pendingPeriod;                            // adopt the new period at the boundary
            }

            // Lag guard: keep readPos safely behind the write head (read only
            // already-written data; never overtake it).  Re-anchor ~2 periods back
            // on drift -- rare, only on large pitch glides.  INVARIANT: minLag must
            // stay >= period + kXfade so the crossfade's old head can't read ahead
            // of the write pointer (the +8 is slack); preserve that if revisiting.
            double lag = (double) writePos - readPos;
            while (lag < 0.0)        lag += (double) kRingSize;
            while (lag >= kRingSize) lag -= (double) kRingSize;
            const double minLag = (double) (period + kXfade + 8);
            const double maxLag = (double) (kRingSize - 2 * period - kXfade - 8);
            if (lag < minLag || lag > maxLag)
            {
                readPos = (double) writePos - (double) (2 * period);
                while (readPos < 0.0) readPos += (double) kRingSize;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// OctaveStyleDSP
// ─────────────────────────────────────────────────────────────────────────────
void OctaveStyleDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;

    juce::dsp::ProcessSpec spec { sampleRate,
                                   (juce::uint32) juce::jmax (1, maxBlockSize),
                                   2 };
    mRangeLpf.prepare (spec);
    mRangeLpf.reset();
    updateRangeCoefs();

    const int n = juce::jmax (1, maxBlockSize);
    mFilteredBuf .setSize (2, n, false, true, true);
    for (auto& b : mShiftedBuf)
        b.setSize (2, n, false, true, true);
    for (auto& b : mDoublerScratch)
        b.setSize (2, n, false, true, true);

    // Granular shifter pitch ratios per octave path.
    for (auto& chArr : mShifters)
    {
        chArr[kPlusOne ].setPitchRatio (2.0f);
        chArr[kMinusOne].setPitchRatio (0.5f);
        chArr[kMinusTwo].setPitchRatio (0.25f);
        for (auto& s : chArr) s.reset();
    }

    // QA-EffectsReview Task 5: PSOLA period-doublers (octave-DOWN) + pitch tracker.
    for (auto& chArr : mDoublers)
    {
        chArr[kDdMinusOne].setRepeats (2);   // -1 oct
        chArr[kDdMinusTwo].setRepeats (4);   // -2 oct
        for (auto& d : chArr) d.reset();
    }
    mConf = 0.0f;
    mYin.prepare (sampleRate);

    // QA-EffectsReview Task 5 (C3): voicing coefs.
    mFastAtkCoef     = 1.0f - std::exp (-1.0f / (float) (0.001 * sampleRate));   // ~1 ms attack
    mFastRelCoef     = 1.0f - std::exp (-1.0f / (float) (0.015 * sampleRate));   // ~15 ms release
    mSlowCoef        = 1.0f - std::exp (-1.0f / (float) (0.080 * sampleRate));   // ~80 ms slow env
    mDuckRecoverCoef = 1.0f - std::exp (-1.0f / (float) (0.025 * sampleRate));   // ~25 ms duck recover
    mVoiceLpCoef     = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 5000.0f / (float) sampleRate);
    mFastEnv = mSlowEnv = 0.0f;
    mDuck = 1.0f;
    for (auto& v : mVoiceLpState) { v[0] = v[1] = 0.0f; }

    mDcCoef = 1.0f - (float) (juce::MathConstants<double>::twoPi * 5.0 / sampleRate);
    mDcXL = mDcYL = mDcXR = mDcYR = 0.0f;
    mFwrDcXL = mFwrDcYL = mFwrDcXR = mFwrDcYR = 0.0f;

    mSchmittHi = false;
    mDivBy2Counter = mDivBy4Counter = 0;
    mDivBy2OutSign = mDivBy4OutSign = 1.0f;
}

void OctaveStyleDSP::reset()
{
    for (auto& chArr : mShifters)
        for (auto& s : chArr)
            s.reset();
    for (auto& chArr : mDoublers)
        for (auto& d : chArr)
            d.reset();
    mYin.reset();
    mConf = 0.0f;
    mFastEnv = mSlowEnv = 0.0f;
    mDuck = 1.0f;
    for (auto& v : mVoiceLpState) { v[0] = v[1] = 0.0f; }
    mRangeLpf.reset();
    mDcXL = mDcYL = mDcXR = mDcYR = 0.0f;
    mFwrDcXL = mFwrDcYL = mFwrDcXR = mFwrDcYR = 0.0f;
    mSchmittHi = false;
    mDivBy2Counter = mDivBy4Counter = 0;
    mDivBy2OutSign = mDivBy4OutSign = 1.0f;
}

void OctaveStyleDSP::updateRangeCoefs()
{
    if (mSampleRate <= 0.0) return;
    const float r  = juce::jlimit (0.0f, 1.0f, mRange);
    const float hz = kRangeMinHz * std::pow (kRangeMaxHz / kRangeMinHz, r);
    *mRangeLpf.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (
        (double) mSampleRate, hz);
}

void OctaveStyleDSP::setMode (int m)
{
    const Mode newMode = static_cast<Mode> (juce::jlimit (0, 1, m));
    if (mMode != newMode) { mMode = newMode; reset(); }
}

void OctaveStyleDSP::setDirectLevel (float v01) { v01 = juce::jlimit (0.0f, 1.0f, v01); if (mDirectLevel != v01) mDirectLevel = v01; }
void OctaveStyleDSP::setOct1Up      (float v01) { v01 = juce::jlimit (0.0f, 1.0f, v01); if (mOct1Up      != v01) mOct1Up      = v01; }
void OctaveStyleDSP::setOct1Down    (float v01) { v01 = juce::jlimit (0.0f, 1.0f, v01); if (mOct1Down    != v01) mOct1Down    = v01; }
void OctaveStyleDSP::setOct2Down    (float v01) { v01 = juce::jlimit (0.0f, 1.0f, v01); if (mOct2Down    != v01) mOct2Down    = v01; }
void OctaveStyleDSP::setRange       (float v01)
{
    v01 = juce::jlimit (0.0f, 1.0f, v01);
    if (mRange != v01) { mRange = v01; updateRangeCoefs(); }
}

void OctaveStyleDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    auto ensureSize = [n, numCh] (juce::AudioBuffer<float>& b)
    {
        if (b.getNumChannels() < numCh || b.getNumSamples() < n)
            b.setSize (numCh, n, false, false, true);
    };
    ensureSize (mFilteredBuf);
    for (auto& b : mShiftedBuf) ensureSize (b);
    for (auto& b : mDoublerScratch) ensureSize (b);

    // 1. Copy input into the Range-filtered scratch (used as the source for
    //    the pitch-shift paths; the dry signal stays in `buffer` until the
    //    final sum below).
    for (int ch = 0; ch < numCh; ++ch)
        mFilteredBuf.copyFrom (ch, 0, buffer, ch, 0, n);
    {
        juce::dsp::AudioBlock<float> blk (mFilteredBuf);
        auto sub = blk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        mRangeLpf.process (ctx);
    }

    // 2. Generate the three pitch-shifted streams.
    if (mMode == Mode::Polyphonic)
    {
        // Feed the pitch tracker (Range-filtered input so the fundamental dominates).
        if (numCh > 1)
            mYin.pushAudio (mFilteredBuf.getReadPointer (0), mFilteredBuf.getReadPointer (1), n);
        else
            mYin.pushAudio (mFilteredBuf.getReadPointer (0), n);

        // Read the published pitch -> a period-aligned granular grain (kills the
        // warble) + the doublers' period; smooth the confidence (it crossfades the
        // crisp doubler vs the granular fallback below).
        const float hz   = mYin.getFrequencyHz();
        const float conf = mYin.getConfidence();
        if (hz >= PitchTrackerYIN::kMinFreqHz)
        {
            const double P     = mSampleRate / (double) hz;
            const int    Pi    = (int) std::lround (P);
            const int    nPer  = juce::jmax (1, (int) std::lround (1024.0 / P));   // ~1024-sample target
            const int    grain = juce::jlimit (256, GranularShifter::kBufferSize / 2, nPer * Pi);
            for (auto& chArr : mShifters) for (auto& s : chArr) s.setGrainSize (grain);
            for (auto& chArr : mDoublers) for (auto& d : chArr) d.setPendingPeriod (Pi);
        }
        const float confTarget = (hz >= PitchTrackerYIN::kMinFreqHz && conf > kConfThreshold) ? 1.0f : 0.0f;
        const float prevConf   = mConf;
        mConf += (confTarget - mConf) * kConfSmooth;   // per-block one-pole

        for (int ch = 0; ch < numCh; ++ch)
        {
            const int chIdx = juce::jmin (ch, 1);
            const float* src = mFilteredBuf.getReadPointer (ch);

            // +1 oct: period-synced granular shifter.
            mShifters[(size_t) chIdx][kPlusOne].process (
                src, mShiftedBuf[kPlusOne].getWritePointer (ch), n);

            // -1 / -2 oct: crossfade the PSOLA doubler (confident single notes) against
            // the period-synced granular (chords / unvoiced) by the smoothed confidence.
            mShifters[(size_t) chIdx][kMinusOne].process (
                src, mShiftedBuf[kMinusOne].getWritePointer (ch), n);
            mShifters[(size_t) chIdx][kMinusTwo].process (
                src, mShiftedBuf[kMinusTwo].getWritePointer (ch), n);
            mDoublers[(size_t) chIdx][kDdMinusOne].process (
                src, mDoublerScratch[0].getWritePointer (ch), n);
            mDoublers[(size_t) chIdx][kDdMinusTwo].process (
                src, mDoublerScratch[1].getWritePointer (ch), n);

            // granular *= (1 - conf); doubler *= conf; sum -> mShiftedBuf.
            mShiftedBuf[kMinusOne].applyGainRamp (ch, 0, n, 1.0f - prevConf, 1.0f - mConf);
            mShiftedBuf[kMinusTwo].applyGainRamp (ch, 0, n, 1.0f - prevConf, 1.0f - mConf);
            mDoublerScratch[0].applyGainRamp (ch, 0, n, prevConf, mConf);
            mDoublerScratch[1].applyGainRamp (ch, 0, n, prevConf, mConf);
            mShiftedBuf[kMinusOne].addFrom (ch, 0, mDoublerScratch[0], ch, 0, n);
            mShiftedBuf[kMinusTwo].addFrom (ch, 0, mDoublerScratch[1], ch, 0, n);
        }
    }
    else // Vintage
    {
        for (int i = 0; i < n; ++i)
        {
            // Mono mix for Vintage trigger paths.
            const float l = mFilteredBuf.getReadPointer (0)[i];
            const float r = (numCh > 1) ? mFilteredBuf.getReadPointer (1)[i] : l;
            const float mono = 0.5f * (l + r);

            // Schmitt trigger zero-crossing detection.
            bool prevHi = mSchmittHi;
            if (mono > kSchmittHi) mSchmittHi = true;
            else if (mono < kSchmittLo) mSchmittHi = false;

            const bool zeroCross = (mSchmittHi != prevHi);
            if (zeroCross)
            {
                if (++mDivBy2Counter >= 2) { mDivBy2Counter = 0; mDivBy2OutSign = -mDivBy2OutSign; }
                if (++mDivBy4Counter >= 4) { mDivBy4Counter = 0; mDivBy4OutSign = -mDivBy4OutSign; }
            }

            // Track input envelope for amplitude scaling on the divider
            // outputs.  Simple peak follower at one-pole release.
            const float env = std::abs (mono);

            // -1 oct = square wave at half input frequency.
            const float minus1 = mDivBy2OutSign * env;
            // -2 oct = square wave at quarter input frequency.
            const float minus2 = mDivBy4OutSign * env;
            // +1 oct = full-wave rectifier (Octavia trick).  Subtract DC
            // pedestal so the rectified stream sits zero-mean before the
            // separate output DC blocker (which catches the rest).
            const float plus1raw = std::abs (mono) - 0.5f * env;

            for (int ch = 0; ch < numCh; ++ch)
            {
                mShiftedBuf[kPlusOne ].getWritePointer (ch)[i] = plus1raw;
                mShiftedBuf[kMinusOne].getWritePointer (ch)[i] = minus1;
                mShiftedBuf[kMinusTwo].getWritePointer (ch)[i] = minus2;
            }
        }

        // DC-block the +1 oct rectifier per channel before mix-in.
        const float R = mDcCoef;
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* d = mShiftedBuf[kPlusOne].getWritePointer (ch);
            float& xPrev = (ch == 0) ? mFwrDcXL : mFwrDcXR;
            float& yPrev = (ch == 0) ? mFwrDcYL : mFwrDcYR;
            for (int i = 0; i < n; ++i)
            {
                const float in = d[i];
                const float y  = in - xPrev + R * yPrev;
                xPrev = in;
                yPrev = y;
                d[i]  = y;
            }
        }
    }

    // 2b. C3 voicing (Polyphonic only -- Vintage keeps its own raw character):
    //     transient-duck the shifted streams (the dry attack leads, the shifted
    //     voices fade in behind it) + a gentle low-pass to mask pitch-shift smear.
    //     The detector runs on the still-dry `buffer`.
    if (mMode == Mode::Polyphonic)
    for (int i = 0; i < n; ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            mono += buffer.getSample (ch, i);
        mono = std::abs (mono / (float) numCh);

        mFastEnv += (mono - mFastEnv) * ((mono > mFastEnv) ? mFastAtkCoef : mFastRelCoef);
        mSlowEnv += (mono - mSlowEnv) * mSlowCoef;
        if (mFastEnv > mSlowEnv * kTransRatio + kTransFloor)
            mDuck = kDuckDepth;                              // duck on the attack
        mDuck += (1.0f - mDuck) * mDuckRecoverCoef;          // recover toward 1

        for (int v = 0; v < 3; ++v)
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d  = mShiftedBuf[v].getWritePointer (ch);
                float& lp = mVoiceLpState[v][ch];
                lp += (d[i] - lp) * mVoiceLpCoef;            // gentle 1-pole LP
                d[i] = lp * mDuck;                           // + transient duck
            }
    }

    // 3. Sum: out = direct*level + sum(shifted * level).  Scale dry by
    // mDirectLevel so 0 = effect-only, 1 = full dry plus shifted.
    for (int ch = 0; ch < numCh; ++ch)
    {
        float* dry = buffer.getWritePointer (ch);
        const float* p1 = mShiftedBuf[kPlusOne ].getReadPointer (ch);
        const float* m1 = mShiftedBuf[kMinusOne].getReadPointer (ch);
        const float* m2 = mShiftedBuf[kMinusTwo].getReadPointer (ch);
        for (int i = 0; i < n; ++i)
            dry[i] = dry[i] * mDirectLevel
                   + p1[i] * mOct1Up
                   + m1[i] * mOct1Down
                   + m2[i] * mOct2Down;
    }

    // 4. 5 Hz DC blocker on the final sum.
    {
        const float R = mDcCoef;
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            float& xPrev = (ch == 0) ? mDcXL : mDcXR;
            float& yPrev = (ch == 0) ? mDcYL : mDcYR;
            for (int i = 0; i < n; ++i)
            {
                const float in = d[i];
                const float y  = in - xPrev + R * yPrev;
                xPrev = in;
                yPrev = y;
                d[i]  = y;
            }
        }
    }
}

void OctaveStyleDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("OctaveStyleDSP");
    state.setProperty ("mode",        (int) mMode,    nullptr);
    state.setProperty ("direct",      mDirectLevel,   nullptr);
    state.setProperty ("oct1up",      mOct1Up,        nullptr);
    state.setProperty ("oct1down",    mOct1Down,      nullptr);
    state.setProperty ("oct2down",    mOct2Down,      nullptr);
    state.setProperty ("range",       mRange,         nullptr);
    state.setProperty ("bypassed",    (int) bypassed, nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void OctaveStyleDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (! xml || ! xml->hasTagName ("OctaveStyleDSP")) return;
    auto state = juce::ValueTree::fromXml (*xml);
    setMode        ((int)            state.getProperty ("mode",     (int) Mode::Polyphonic));
    setDirectLevel ((float)(double) state.getProperty ("direct",   1.0));
    setOct1Up      ((float)(double) state.getProperty ("oct1up",   0.0));
    setOct1Down    ((float)(double) state.getProperty ("oct1down", 0.5));
    setOct2Down    ((float)(double) state.getProperty ("oct2down", 0.0));
    setRange       ((float)(double) state.getProperty ("range",    0.6));
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;
}
