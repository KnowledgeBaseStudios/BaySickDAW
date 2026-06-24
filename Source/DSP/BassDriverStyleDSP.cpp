#include "BassDriverStyleDSP.h"

namespace
{
    constexpr float kXover1Hz = 500.0f;    // low/mid split -- lows < 500 Hz stay clean (BB-1X)
    constexpr float kXover2Hz = 2000.0f;   // mid/high split

    // tanh(0.2) static bias removed post-shaper (same approach as
    // BluesDriveStyleDSP for the asymmetric character).
    constexpr float kAsymBias       = 0.2f;
    const     float kAsymBiasOffset = std::tanh (kAsymBias);

    // QA-EffectsReview Task 5: MDP-style adaptive drive shaping.  kDriveFloor = the
    // fraction of the Drive knob applied at silence (soft playing stays cleaner);
    // the envelope lifts it toward the full knob value as the player digs in.
    constexpr float kDriveFloor = 0.4f;
    constexpr float kEnvSens    = 3.0f;    // envelope -> 0..1 drive-lift sensitivity
}

void BassDriverStyleDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;
    mOs.prepare (2, maxBlockSize);

    juce::dsp::ProcessSpec spec { sampleRate,
                                   (juce::uint32) juce::jmax (1, maxBlockSize),
                                   2 };
    auto setupLP = [&] (juce::dsp::LinkwitzRileyFilter<float>& f, float hz)
    {
        f.prepare (spec);
        f.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        f.setCutoffFrequency (hz);
        f.reset();
    };
    auto setupHP = [&] (juce::dsp::LinkwitzRileyFilter<float>& f, float hz)
    {
        f.prepare (spec);
        f.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        f.setCutoffFrequency (hz);
        f.reset();
    };
    setupLP (mXover1Lp, kXover1Hz);
    setupHP (mXover1Hp, kXover1Hz);
    setupLP (mXover2Lp, kXover2Hz);
    setupHP (mXover2Hp, kXover2Hz);

    const int n = juce::jmax (1, maxBlockSize);
    mLowBuf       .setSize (2, n, false, true, true);
    mMidBuf       .setSize (2, n, false, true, true);
    mHighBuf      .setSize (2, n, false, true, true);
    mClippedSumBuf.setSize (2, n, false, true, true);
    mCleanSumBuf  .setSize (2, n, false, true, true);

    mDcCoef = 1.0f - (float) (juce::MathConstants<double>::twoPi * 5.0 / sampleRate);
    mDcXL = mDcYL = mDcXR = mDcYR = 0.0f;

    // QA-EffectsReview Task 5: adaptive-drive envelope follower (fast attack, slow
    // release) + the per-base-sample drive scratch.
    mEnvAtkCoef = 1.0f - std::exp (-1.0f / (float) (0.003 * sampleRate));   // ~3 ms
    mEnvRelCoef = 1.0f - std::exp (-1.0f / (float) (0.080 * sampleRate));   // ~80 ms
    mEnv = 0.0f;
    mDriveEnvBuf.setSize (1, juce::jmax (1, maxBlockSize), false, true, true);
}

void BassDriverStyleDSP::reset()
{
    mOs.reset();
    mXover1Lp.reset();  mXover1Hp.reset();
    mXover2Lp.reset();  mXover2Hp.reset();
    mDcXL = mDcYL = mDcXR = mDcYR = 0.0f;
    mEnv = 0.0f;
}

void BassDriverStyleDSP::setDrive (float v01) { v01 = juce::jlimit (0.0f, 1.0f, v01); if (mDrive != v01) mDrive = v01; }
void BassDriverStyleDSP::setBlend (float v01) { v01 = juce::jlimit (0.0f, 1.0f, v01); if (mBlend != v01) mBlend = v01; }
void BassDriverStyleDSP::setLow   (float v01) { v01 = juce::jlimit (0.0f, 1.0f, v01); if (mLow   != v01) mLow   = v01; }
void BassDriverStyleDSP::setHigh  (float v01) { v01 = juce::jlimit (0.0f, 1.0f, v01); if (mHigh  != v01) mHigh  = v01; }
void BassDriverStyleDSP::setLevel (float dB)
{
    dB = juce::jlimit (-24.0f, 12.0f, dB);
    if (mLevel != dB) mLevel = dB;
}

void BassDriverStyleDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    // Lazy resize scratch buffers if the host block grew.
    auto ensureSize = [n, numCh] (juce::AudioBuffer<float>& b)
    {
        if (b.getNumChannels() < numCh || b.getNumSamples() < n)
            b.setSize (numCh, n, false, false, true);
    };
    ensureSize (mLowBuf);
    ensureSize (mMidBuf);
    ensureSize (mHighBuf);
    ensureSize (mClippedSumBuf);
    ensureSize (mCleanSumBuf);

    // Copy input into the working buffers.  We need the input in two places:
    // the low-pass branch (-> mLowBuf) and the high-pass branch (-> midHigh).
    // Use mLowBuf as the LP destination and a local highpass into mMidBuf
    // (which gets further split into Mid + High).
    for (int ch = 0; ch < numCh; ++ch)
    {
        mLowBuf.copyFrom  (ch, 0, buffer, ch, 0, n);
        mMidBuf.copyFrom  (ch, 0, buffer, ch, 0, n);  // shared starting point
    }

    // 1a. Xover1 split @ 500 Hz: mLowBuf <- LP, mMidBuf <- HP (rest).
    {
        juce::dsp::AudioBlock<float> lowBlk (mLowBuf);
        juce::dsp::AudioBlock<float> midBlk (mMidBuf);
        auto lowSub = lowBlk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        auto midSub = midBlk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> lowCtx (lowSub);
        juce::dsp::ProcessContextReplacing<float> midCtx (midSub);
        mXover1Lp.process (lowCtx);
        mXover1Hp.process (midCtx);
    }

    // 1b. Xover2 split @ 2 kHz: mMidBuf has both Mid + High after Xover1Hp;
    //     mHighBuf <- HP of Xover2 (the high band); mMidBuf <- LP of Xover2
    //     (the mid band).
    for (int ch = 0; ch < numCh; ++ch)
        mHighBuf.copyFrom (ch, 0, mMidBuf, ch, 0, n);
    {
        juce::dsp::AudioBlock<float> midBlk  (mMidBuf);
        juce::dsp::AudioBlock<float> highBlk (mHighBuf);
        auto midSub  = midBlk .getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        auto highSub = highBlk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> midCtx  (midSub);
        juce::dsp::ProcessContextReplacing<float> highCtx (highSub);
        mXover2Lp.process (midCtx);
        mXover2Hp.process (highCtx);
    }

    // 2. Sum Mid + High into the "to-be-clipped" path.  Apply per-band trims
    //    (mHigh on the high band) before summing -- the spec exposes Low and
    //    High as gain knobs but Mid is whatever's left.
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* mid  = mMidBuf .getReadPointer (ch);
        const float* high = mHighBuf.getReadPointer (ch);
        float* clean      = mCleanSumBuf.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            clean[i] = mid[i] + high[i] * mHigh;
    }

    // 3a. MDP-style dynamics-adaptive drive (QA-EffectsReview Task 5).  An envelope
    // follower on the Mid+High sum sets a per-base-sample drive: soft playing gets
    // kDriveFloor of the Drive knob (stays cleaner / keeps dynamics), digging in
    // lifts it toward the full knob value (the grind comes in on harder playing).
    const float baseDrive = 1.0f + mDrive * 14.0f;
    if (mDriveEnvBuf.getNumSamples() < n)
        mDriveEnvBuf.setSize (1, n, false, false, true);
    {
        float* env = mDriveEnvBuf.getWritePointer (0);
        for (int i = 0; i < n; ++i)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                mono += mCleanSumBuf.getSample (ch, i);
            mono = std::abs (mono / (float) numCh);
            const float coef = (mono > mEnv) ? mEnvAtkCoef : mEnvRelCoef;
            mEnv += (mono - mEnv) * coef;
            const float lift = std::tanh (mEnv * kEnvSens);   // 0..~1
            env[i] = baseDrive * (kDriveFloor + (1.0f - kDriveFloor) * lift);
        }
    }

    // 3b. Asymmetric soft-clip on the Mid+High sum at 4x oversampled, at the
    // adaptive drive (mapped per base sample).  Base scalar 1..15 (bass drivers
    // are gentler than guitar overdrives).
    for (int ch = 0; ch < numCh; ++ch)
        mClippedSumBuf.copyFrom (ch, 0, mCleanSumBuf, ch, 0, n);
    {
        juce::dsp::AudioBlock<float> clipBlk (mClippedSumBuf);
        auto clipSub = clipBlk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        auto upBlock = mOs.upsample (clipSub);
        const int upN     = (int) upBlock.getNumSamples();
        const int upChs   = (int) upBlock.getNumChannels();
        const int osRatio = (n > 0) ? juce::jmax (1, upN / n) : 4;
        const float* env  = mDriveEnvBuf.getReadPointer (0);
        for (int ch = 0; ch < upChs; ++ch)
        {
            float* d = upBlock.getChannelPointer ((size_t) ch);
            for (int i = 0; i < upN; ++i)
            {
                const int baseIdx = juce::jmin (n - 1, i / osRatio);
                const float x = d[i] * env[baseIdx] + kAsymBias;
                d[i] = std::tanh (x) - kAsymBiasOffset;
            }
        }
        mOs.downsample (clipSub);
    }

    // 4. Blend clean Mid+High (mCleanSumBuf) with clipped Mid+High
    //    (mClippedSumBuf) per the Blend knob, then write into buffer along
    //    with the clean Low band.  buffer = low*mLow + (clean*(1-blend) +
    //    clipped*blend).
    const float blend = juce::jlimit (0.0f, 1.0f, mBlend);
    const float gC    = 1.0f - blend;
    const float gW    = blend;
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* low     = mLowBuf       .getReadPointer (ch);
        const float* cleanMH = mCleanSumBuf  .getReadPointer (ch);
        const float* wetMH   = mClippedSumBuf.getReadPointer (ch);
        float* out           = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            out[i] = low[i] * mLow + cleanMH[i] * gC + wetMH[i] * gW;
    }

    // 5. 5 Hz DC blocker (asymmetric shaper leaves bias at silence).
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

    // 6. Output level.
    if (mLevel != 0.0f)
        buffer.applyGain (juce::Decibels::decibelsToGain (mLevel, -60.0f));
}

void BassDriverStyleDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("BassDriverStyleDSP");
    state.setProperty ("drive", mDrive, nullptr);
    state.setProperty ("blend", mBlend, nullptr);
    state.setProperty ("low",   mLow,   nullptr);
    state.setProperty ("high",  mHigh,  nullptr);
    state.setProperty ("level", mLevel, nullptr);
    state.setProperty ("bypassed", (int) bypassed, nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void BassDriverStyleDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (! xml || ! xml->hasTagName ("BassDriverStyleDSP")) return;
    auto state = juce::ValueTree::fromXml (*xml);
    setDrive ((float)(double) state.getProperty ("drive", 0.5));
    setBlend ((float)(double) state.getProperty ("blend", 0.7));
    setLow   ((float)(double) state.getProperty ("low",   0.7));
    setHigh  ((float)(double) state.getProperty ("high",  0.7));
    setLevel ((float)(double) state.getProperty ("level", 0.0));
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;
}
