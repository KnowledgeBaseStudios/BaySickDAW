#include "OctaveStyleDSP.h"

namespace
{
    constexpr float kRangeMinHz = 300.0f;
    constexpr float kRangeMaxHz = 3000.0f;

    // QA-OctavePedal engine handoff: hysteresis around the YIN confidence so the
    // doubler/granular switch cannot flap and park mid-blend (the old single
    // 0.5 threshold left both octave-down engines summed at half gain = comb).
    constexpr float kConfEngage  = 0.55f;
    constexpr float kConfRelease = 0.35f;
    constexpr float kConfSmooth  = 0.25f;   // per-block switch-fade coef (~50-90 ms at typical block sizes)

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

        // #35 final form (smoke #55): plain overlap-add.  Unity now comes from
        // the STRUCTURE -- setGrainSize re-anchors the pair to an exact
        // half-grain offset (Hann halves sum to 1.0 at every phase) and head
        // jumps preserve phase mod grain.  The interim live-sum divide is
        // GONE: near coincident window nulls it amplified interpolation noise
        // (tiny/tiny) and its epsilon floor stepped 0<->full = the smoke's
        // clicks/tearing/dropouts across all three octave modes.
        output[i] = s1 * w1 + s2 * w2;

        readPos1 += (double) ratio;
        if (readPos1 >= (double) kBufferSize) readPos1 -= (double) kBufferSize;
        readPos2 += (double) ratio;
        if (readPos2 >= (double) kBufferSize) readPos2 -= (double) kBufferSize;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// PeriodDoubler (pitch-synchronous octave-down: 1/N-speed segment playout with
// mark-anchored hops) -- QA-OctavePedal
// ─────────────────────────────────────────────────────────────────────────────
void OctaveStyleDSP::PeriodDoubler::reset()
{
    ring.fill (0.0f);
    markCount   = 0;
    writeIdx    = 0;
    writeAbs    = 0.0;
    readAbs     = 0.0;
    xfadeOldAbs = 0.0;
    xfadeLeft   = 0;
    xfadeLen    = kXfadeMin;
    segStart    = 0.0;
    segEnd      = 0.0;
    primed      = false;
}

float OctaveStyleDSP::PeriodDoubler::interp (double absPos) const noexcept
{
    double p = std::fmod (absPos, (double) kRingSize);
    if (p < 0.0) p += (double) kRingSize;
    const int   i0 = (int) p;
    const int   i1 = (i0 + 1 < kRingSize) ? i0 + 1 : 0;
    const float f  = (float) (p - (double) i0);
    return ring[(size_t) i0] * (1.0f - f) + ring[(size_t) i1] * f;
}

void OctaveStyleDSP::PeriodDoubler::pushMark (double absPos)
{
    if (markCount > 0 && absPos <= marks[(size_t) markCount - 1])
        return;                                        // marks must ascend
    if (markCount >= kMarkQueue)
    {
        // Full: drop consumed marks first; if STILL full the stream is
        // pathological sub-period chatter -- drop the incoming mark and let
        // the owner's synth-fill heal the grid.
        purgeMarksBelow (primed ? segEnd : absPos - 4.0 * period);
        if (markCount >= kMarkQueue) return;
    }
    marks[(size_t) markCount++] = absPos;
}

bool OctaveStyleDSP::PeriodDoubler::markAfter (double x, int k, double& out) const noexcept
{
    // k-th mark strictly greater than x (k >= 1); k == 0 is x itself.
    if (k <= 0) { out = x; return true; }
    int found = 0;
    for (int i = 0; i < markCount; ++i)
        if (marks[(size_t) i] > x + 1.0e-6)
            if (++found == k) { out = marks[(size_t) i]; return true; }
    return false;
}

void OctaveStyleDSP::PeriodDoubler::purgeMarksBelow (double x) noexcept
{
    int keep = 0;
    for (int i = 0; i < markCount; ++i)
        if (marks[(size_t) i] >= x)
            marks[(size_t) keep++] = marks[(size_t) i];
    markCount = keep;
}

void OctaveStyleDSP::PeriodDoubler::beginSeam (double target)
{
    xfadeOldAbs = readAbs;   // old head keeps playing forward under the fade
    xfadeLen    = juce::jlimit (kXfadeMin, kXfadeMax, (int) (period * 0.25));
    xfadeLeft   = xfadeLen;
    readAbs     = target;
}

void OctaveStyleDSP::PeriodDoubler::process (const float* input, float* output, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        ring[(size_t) writeIdx] = input[i];
        if (++writeIdx >= kRingSize) writeIdx = 0;
        writeAbs += 1.0;

        if (! primed)
        {
            // Engage once two marks exist: play [m0, m1) from m0.  Marks are
            // detected at the write head, so the lag is ~one period by
            // construction (the doubler's intrinsic latency).
            if (markCount >= 2)
            {
                segStart = marks[(size_t) markCount - 2];
                segEnd   = marks[(size_t) markCount - 1];
                purgeMarksBelow (segStart);
                readAbs  = segStart;
                primed   = true;
            }
            else
            {
                output[i] = 0.0f;
                continue;
            }
        }

        if (readAbs >= segEnd)
        {
            // Seam: the segment just finished its 1/octaveDiv-speed playout
            // (octaveDiv periods of output time per one input period).  Hop
            // forward octaveDiv-1 marks so consumption balances real time.
            // Lag correction happens HERE, at marks, one period per step:
            // under-lag hops one mark less, over-lag one extra.  Bounds keep
            // both crossfade heads behind the write head and inside the ring.
            const double overshoot = readAbs - segEnd;   // < readRate; preserved across the jump
            const double lag    = writeAbs - readAbs;
            const double lagMin = 0.5 * period + (double) xfadeLen + 8.0;
            const double lagMax = 3.0 * period + (double) xfadeLen;
            int skip = octaveDiv - 1;
            if      (lag < lagMin) skip = juce::jmax (0, skip - 1);
            else if (lag > lagMax) ++skip;

            double ns = 0.0, ne = 0.0;
            if (! (markAfter (segEnd, skip, ns) && markAfter (segEnd, skip + 1, ne)))
            {
                // Mark starvation (quiet input / tracker dropout): stay on
                // the synthesized period grid so the engine never stalls --
                // the owner has faded this path out by then anyway.
                ns = segEnd + (double) skip * period;
                ne = ns + period;
            }
            if (ne - ns < kMinPeriod * 0.5 || ne - ns > kMaxPeriod * 2.0)
                ne = ns + period;                        // degenerate mark spacing
            while (skip > 0 && writeAbs - ns < lagMin)
            {
                --skip;                                  // never let the new segment start hug the write head
                if (! markAfter (segEnd, skip, ns))            ns = segEnd + (double) skip * period;
                if (! markAfter (segEnd, skip + 1, ne) || ne <= ns) ne = ns + period;
            }
            beginSeam (ns + overshoot);
            segStart = ns;
            segEnd   = ne;
            purgeMarksBelow (segStart);
        }

        float s = interp (readAbs);
        if (xfadeLeft > 0)
        {
            const float t = (float) xfadeLeft / (float) xfadeLen;   // 1 -> 0
            s = s * (1.0f - t) + interp (xfadeOldAbs) * t;
            xfadeOldAbs += readRate;   // old head keeps playing at shifted speed
            --xfadeLeft;
        }
        output[i] = s;
        readAbs += readRate;
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
    mMonoScratch .setSize (1, n, false, true, true);
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

    // Pitch-synchronous octave-down shifters + pitch tracker + mark detector.
    for (auto& chArr : mDoublers)
    {
        chArr[kDdMinusOne].setOctaveDivisor (2);   // -1 oct
        chArr[kDdMinusTwo].setOctaveDivisor (4);   // -2 oct
        for (auto& d : chArr) d.reset();
    }
    mDoublerMix    = 0.0f;
    mDoublerOn     = false;
    mTrackedPeriod = 256.0;
    mMarkSchmittHi = false;
    mMarkPrevMono  = 0.0f;
    mLastMarkAbs   = -1.0e12;
    mMarkClockAbs  = 0.0;
    mYin.prepare (sampleRate);
    mPoly.prepare (sampleRate);
    // #12 (Jeff 2026-07-18, docket pick a): single wide auto range, no
    // instrument selector -- ~4-string bass low E (41 Hz) up through the
    // guitar's high register.  Max notes stays the 6-string default.
    mPoly.setFreqRange (40.0f, 1300.0f);

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
    mPoly.reset();
    mDoublerMix    = 0.0f;
    mDoublerOn     = false;
    mTrackedPeriod = 256.0;
    mMarkSchmittHi = false;
    mMarkPrevMono  = 0.0f;
    mLastMarkAbs   = -1.0e12;
    mMarkClockAbs  = 0.0;
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

    // Floor of 2 channels: the mono-input lockstep path below parks the ch-1
    // doublers' output in the scratch buffers' spare channel.
    auto ensureSize = [n, numCh] (juce::AudioBuffer<float>& b)
    {
        if (b.getNumChannels() < numCh || b.getNumSamples() < n)
            b.setSize (juce::jmax (2, numCh), n, false, false, true);
    };
    ensureSize (mFilteredBuf);
    for (auto& b : mShiftedBuf) ensureSize (b);
    for (auto& b : mDoublerScratch) ensureSize (b);
    if (mMonoScratch.getNumSamples() < n) mMonoScratch.setSize (1, n, false, false, true);

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
        // Mono mix of the Range-filtered input (fundamental dominates) -> both
        // trackers + the mark detector, one pass.
        {
            float* mono = mMonoScratch.getWritePointer (0);
            const float* lp = mFilteredBuf.getReadPointer (0);
            const float* rp = (numCh > 1) ? mFilteredBuf.getReadPointer (1) : lp;
            for (int i = 0; i < n; ++i) mono[i] = 0.5f * (lp[i] + rp[i]);
        }
        const float* mono = mMonoScratch.getReadPointer (0);
        mYin .pushAudio (mono, n);
        mPoly.pushAudio (mono, n);   // #12: real poly detection feeds the granular grain sizing

        // YIN drives the doubler (a mono, single-note engine) -- eligibility +
        // the fractional tracked period.
        const float hz   = mYin.getFrequencyHz();
        const float conf = mYin.getConfidence();
        bool doublerEligible = false;
        if (hz >= PitchTrackerYIN::kMinFreqHz)
        {
            const double P = mSampleRate / (double) hz;
            if (P >= PeriodDoubler::kMinPeriod && P <= PeriodDoubler::kMaxPeriod)
            {
                mTrackedPeriod = P;
                for (auto& chArr : mDoublers) for (auto& d : chArr) d.setTrackedPeriod (P);
                doublerEligible = true;
            }
        }

        // Granular grain size -> period-aligned OLA (kills warble).  #12: on a
        // chord the mono YIN reading is garbage, so size the grain to the
        // LOWEST detected poly note instead of the flailing mono period; a
        // confident single note stays on YIN (mono behavior preserved).
        double grainP = 0.0;
        if (hz >= PitchTrackerYIN::kMinFreqHz && conf > kConfEngage)
        {
            grainP = mSampleRate / (double) hz;                 // confident mono -> YIN
        }
        else
        {
            const auto notes = mPoly.getNotes();
            if (notes.count > 0)
            {
                float lowest = notes.freqs[0];
                for (int k = 1; k < notes.count; ++k)
                    if (notes.freqs[k] > 0.0f) lowest = juce::jmin (lowest, notes.freqs[k]);
                if (lowest > 0.0f) grainP = mSampleRate / (double) lowest;   // chord -> lowest note
            }
            if (grainP <= 0.0 && hz >= PitchTrackerYIN::kMinFreqHz)
                grainP = mSampleRate / (double) hz;             // low-conf mono still beats nothing
        }
        if (grainP > 0.0)
        {
            const int Pi    = juce::jmax (1, (int) std::lround (grainP));
            const int nPer  = juce::jmax (1, (int) std::lround (1024.0 / grainP));   // ~1024-sample target
            const int grain = juce::jlimit (256, GranularShifter::kBufferSize / 2, nPer * Pi);
            for (auto& chArr : mShifters) for (auto& s : chArr) s.setGrainSize (grain);
        }

        // Engine handoff (QA-OctavePedal): doubler vs granular is SWITCHED with
        // a short fade, never summed as a standing blend -- two desynced
        // octave-down engines at half gain comb.  Hysteresis stops the switch
        // flapping around a single confidence threshold.
        if (mDoublerOn) { if (! doublerEligible || conf < kConfRelease) mDoublerOn = false; }
        else            { if (  doublerEligible && conf > kConfEngage)  mDoublerOn = true;  }
        const float mixTarget = mDoublerOn ? 1.0f : 0.0f;
        const float prevMix   = mDoublerMix;
        mDoublerMix += (mixTarget - mDoublerMix) * kConfSmooth;   // per-block one-pole
        if (std::abs (mDoublerMix - mixTarget) < 1.0e-3f) mDoublerMix = mixTarget;

        // Pitch-mark detection, shared across channels: Schmitt rising crossings
        // on the mono Range-filtered input, sub-sample refined at the threshold
        // crossing, validated against the YIN period (>= half a period since the
        // last mark rejects harmonic chatter).  Brief Schmitt dropouts are
        // bridged by synthesizing marks on the tracked-period grid so the
        // doublers never starve mid-note.
        {
            const double P = mTrackedPeriod;
            for (int i = 0; i < n; ++i)
            {
                const float m     = mono[i];
                const bool  wasHi = mMarkSchmittHi;
                if      (m > kSchmittHi) mMarkSchmittHi = true;
                else if (m < kSchmittLo) mMarkSchmittHi = false;
                if (mMarkSchmittHi && ! wasHi)
                {
                    const float denom = m - mMarkPrevMono;
                    const double frac = (denom > 1.0e-9f)
                        ? juce::jlimit (0.0, 1.0, (double) ((kSchmittHi - mMarkPrevMono) / denom))
                        : 0.0;
                    const double c = mMarkClockAbs + (double) (i - 1) + frac;
                    if (c - mLastMarkAbs >= 0.5 * P)
                    {
                        for (auto& chArr : mDoublers) for (auto& d : chArr) d.pushMark (c);
                        mLastMarkAbs = c;
                    }
                }
                mMarkPrevMono = m;
            }
            mMarkClockAbs += (double) n;
            if (doublerEligible && mLastMarkAbs >= 0.0)
            {
                const double gap = mMarkClockAbs - mLastMarkAbs;
                if (gap > 6.0 * P)
                {
                    // Long dropout: re-sync instead of backfilling the whole gap.
                    mLastMarkAbs = mMarkClockAbs - 2.0 * P;
                    for (auto& chArr : mDoublers) for (auto& d : chArr) d.pushMark (mLastMarkAbs);
                }
                else while (mMarkClockAbs - mLastMarkAbs > 2.0 * P)
                {
                    mLastMarkAbs += P;
                    for (auto& chArr : mDoublers) for (auto& d : chArr) d.pushMark (mLastMarkAbs);
                }
            }
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            const int chIdx = juce::jmin (ch, 1);
            const float* src = mFilteredBuf.getReadPointer (ch);

            // +1 oct: period-synced granular shifter.
            mShifters[(size_t) chIdx][kPlusOne].process (
                src, mShiftedBuf[kPlusOne].getWritePointer (ch), n);

            // -1 / -2 oct: both engines always run (rings + clocks stay warm for
            // instant switch-back); only the OUTPUT mix is switched.
            mShifters[(size_t) chIdx][kMinusOne].process (
                src, mShiftedBuf[kMinusOne].getWritePointer (ch), n);
            mShifters[(size_t) chIdx][kMinusTwo].process (
                src, mShiftedBuf[kMinusTwo].getWritePointer (ch), n);
            mDoublers[(size_t) chIdx][kDdMinusOne].process (
                src, mDoublerScratch[0].getWritePointer (ch), n);
            mDoublers[(size_t) chIdx][kDdMinusTwo].process (
                src, mDoublerScratch[1].getWritePointer (ch), n);

            if (prevMix < 1.0f || mDoublerMix < 1.0f)
            {
                mShiftedBuf[kMinusOne].applyGainRamp (ch, 0, n, 1.0f - prevMix, 1.0f - mDoublerMix);
                mShiftedBuf[kMinusTwo].applyGainRamp (ch, 0, n, 1.0f - prevMix, 1.0f - mDoublerMix);
            }
            else
            {
                mShiftedBuf[kMinusOne].clear (ch, 0, n);
                mShiftedBuf[kMinusTwo].clear (ch, 0, n);
            }
            if (prevMix > 0.0f || mDoublerMix > 0.0f)
            {
                mDoublerScratch[0].applyGainRamp (ch, 0, n, prevMix, mDoublerMix);
                mDoublerScratch[1].applyGainRamp (ch, 0, n, prevMix, mDoublerMix);
                mShiftedBuf[kMinusOne].addFrom (ch, 0, mDoublerScratch[0], ch, 0, n);
                mShiftedBuf[kMinusTwo].addFrom (ch, 0, mDoublerScratch[1], ch, 0, n);
            }
        }

        if (numCh == 1)
        {
            // Mono blocks would stall the ch-1 doublers' write clocks and desync
            // every future mark compare against the shared mark clock -- keep
            // them in lockstep by feeding them the same mono input (outputs land
            // in the scratch buffers' spare channel and are discarded).
            const float* src = mFilteredBuf.getReadPointer (0);
            mDoublers[1][kDdMinusOne].process (src, mDoublerScratch[0].getWritePointer (1), n);
            mDoublers[1][kDdMinusTwo].process (src, mDoublerScratch[1].getWritePointer (1), n);
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
                float& lp = mVoiceLpState[v][juce::jmin (ch, 1)];   // state array is 2-wide; extra channels share ch1
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
