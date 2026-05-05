#include "OctaveStyleDSP.h"

namespace
{
    constexpr float kRangeMinHz = 300.0f;
    constexpr float kRangeMaxHz = 3000.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// GranularShifter
// ─────────────────────────────────────────────────────────────────────────────
void OctaveStyleDSP::GranularShifter::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    writePos = 0;
    readPos1 = 0.0;
    readPos2 = (double) (kGrainSize / 2);
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
    const float invGrain = 1.0f / (float) kGrainSize;

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
            if (diff < (double) kGrainSize)
                rp -= (double) kGrainSize;            // jump back, lengthen lag
            else if (diff > (double) (kBufferSize - kGrainSize))
                rp += (double) kGrainSize;            // jump forward, shorten lag
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
            const double mod = std::fmod (diff, (double) kGrainSize);
            return (float) (mod * (double) invGrain);
        };

        const float w1 = hann (windowPhase (readPos1));
        const float w2 = hann (windowPhase (readPos2));

        // Sum of windows is ~1.0 since they're 50% offset Hann pairs.
        output[i] = s1 * w1 + s2 * w2;

        readPos1 += (double) ratio;
        readPos2 += (double) ratio;
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

    // Granular shifter pitch ratios per octave path.
    for (auto& chArr : mShifters)
    {
        chArr[kPlusOne ].setPitchRatio (2.0f);
        chArr[kMinusOne].setPitchRatio (0.5f);
        chArr[kMinusTwo].setPitchRatio (0.25f);
        for (auto& s : chArr) s.reset();
    }

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
        for (int ch = 0; ch < numCh; ++ch)
        {
            const int chIdx = juce::jmin (ch, 1);
            const float* src = mFilteredBuf.getReadPointer (ch);
            for (int oct = 0; oct < 3; ++oct)
                mShifters[(size_t) chIdx][(size_t) oct].process (
                    src, mShiftedBuf[oct].getWritePointer (ch), n);
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
