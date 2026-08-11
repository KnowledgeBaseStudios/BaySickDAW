#include "DistortionStyleDSP.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)

namespace
{
    // Pre-emphasis: high-shelf around 1 kHz, gain scales with Dist.  At Dist=0
    // the shelf is unity (no boost); at Dist=1 the shelf hits +12 dB.  Sweeping
    // gain on a fixed-frequency shelf gives the classic "tightening as the
    // drive increases" character.
    constexpr float kPreEmphasisHz   = 1000.0f;
    constexpr float kPreEmphasisMaxDb = 12.0f;

    // Tilt-EQ tone control: two parallel paths crossfaded by Tone.
    // QA-EffectsReview Task 5: scoop recentred ~800 -> ~500 Hz to match the reference.
    constexpr float kToneLpfHz = 250.0f;    // dark side of Tone
    constexpr float kToneHpfHz = 1000.0f;   // bright side of Tone

    // Mid-scoop at noon: when Tone ~ 0.5, both paths summed give a notch
    // around 500 Hz (geometric mean of 250 Hz and 1 kHz).  No explicit notch
    // filter -- the LPF + HPF parallel sum naturally produces a scoop because
    // 500 Hz sits in the rolloff of both.
}

void DistortionStyleDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;

    mOs.prepare (2, maxBlockSize);

    juce::dsp::ProcessSpec spec { sampleRate,
                                   (juce::uint32) juce::jmax (1, maxBlockSize),
                                   2 };
    mPreEmphasis.prepare (spec);  mPreEmphasis.reset();
    mToneLpf    .prepare (spec);  mToneLpf    .reset();
    mToneHpf    .prepare (spec);  mToneHpf    .reset();

    mScratchHpf.setSize (2, juce::jmax (1, maxBlockSize), false, false, true);

    mDcCoef = 1.0f - (float) (juce::MathConstants<double>::twoPi * 5.0 / sampleRate);
    mDcXL = mDcYL = mDcXR = mDcYR = 0.0f;

    updatePreEmphasisCoefs();
    updateToneEQCoefs();
}

void DistortionStyleDSP::reset()
{
    mOs.reset();
    mPreEmphasis.reset();
    mToneLpf.reset();
    mToneHpf.reset();
    mDcXL = mDcYL = mDcXR = mDcYR = 0.0f;
}

void DistortionStyleDSP::updatePreEmphasisCoefs()
{
    if (mSampleRate <= 0.0) return;
    const float gainDb  = kPreEmphasisMaxDb * juce::jlimit (0.0f, 1.0f, mDist);
    const float gainLin = juce::Decibels::decibelsToGain (gainDb, -60.0f);
    *mPreEmphasis.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        (double) mSampleRate, kPreEmphasisHz, 0.7071f, gainLin);
}

void DistortionStyleDSP::updateToneEQCoefs()
{
    if (mSampleRate <= 0.0) return;
    *mToneLpf.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (
        (double) mSampleRate, kToneLpfHz);
    *mToneHpf.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (
        (double) mSampleRate, kToneHpfHz);
}

void DistortionStyleDSP::setDist (float v01)
{
    v01 = juce::jlimit (0.0f, 1.0f, v01);
    if (mDist != v01) { mDist = v01; updatePreEmphasisCoefs(); }
}

void DistortionStyleDSP::setTone (float v01)
{
    v01 = juce::jlimit (0.0f, 1.0f, v01);
    if (mTone != v01) mTone = v01;
}

void DistortionStyleDSP::setLevel (float dB)
{
    dB = juce::jlimit (-24.0f, 12.0f, dB);
    if (mLevel != dB) mLevel = dB;
}

void DistortionStyleDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    juce::dsp::AudioBlock<float> block (buffer);

    // 1. Pre-emphasis high-shelf at 1x.
    {
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        mPreEmphasis.process (ctx);
    }

    // Drive scalar -- aggressive (DS-1 max drive is ~+50 dB).
    const float drive = 1.0f + mDist * 49.0f;

    // 2. Upsample, hard-clip at 4x, downsample.
    auto upBlock = mOs.upsample (block);
    const int upN   = (int) upBlock.getNumSamples();
    const int upChs = (int) upBlock.getNumChannels();
    for (int ch = 0; ch < upChs; ++ch)
    {
        float* d = upBlock.getChannelPointer ((size_t) ch);
        for (int i = 0; i < upN; ++i)
            d[i] = std::clamp (d[i] * drive, -1.0f, 1.0f);
    }
    mOs.downsample (block);

    // 3. Big-Muff tilt EQ: parallel LPF + HPF, crossfaded by Tone.  We need
    // the HPF on a copy so we can sum the two paths back together.
    if (mScratchHpf.getNumChannels() < numCh || mScratchHpf.getNumSamples() < n)
        mScratchHpf.setSize (numCh, n, false, false, true);
    for (int ch = 0; ch < numCh; ++ch)
        mScratchHpf.copyFrom (ch, 0, buffer, ch, 0, n);

    {
        juce::dsp::AudioBlock<float> lpfBlock (buffer);
        juce::dsp::ProcessContextReplacing<float> lpfCtx (lpfBlock);
        mToneLpf.process (lpfCtx);
    }
    {
        juce::dsp::AudioBlock<float> hpfBlock (mScratchHpf);
        // Restrict block to first n samples so we don't run the filter on
        // stale tail data when the buffer is smaller than maxBlock.
        juce::dsp::AudioBlock<float> sub = hpfBlock.getSubBlock (0, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> hpfCtx (sub);
        mToneHpf.process (hpfCtx);
    }

    // Crossfade: Tone=0 -> all LPF (dark); Tone=1 -> all HPF (bright); 0.5
    // -> equal mix (mid-scoop).
    const float t   = juce::jlimit (0.0f, 1.0f, mTone);
    const float gLp = 1.0f - t;
    const float gHp = t;
    for (int ch = 0; ch < numCh; ++ch)
    {
        float* lpf = buffer.getWritePointer (ch);
        const float* hpf = mScratchHpf.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
            lpf[i] = lpf[i] * gLp + hpf[i] * gHp;
    }

    // 4. 5 Hz DC blocker (defensive -- pre-emphasis + parallel filter pair
    // can ring residual DC across bypass-ramp transients).
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

    // 5. Output level.
    if (mLevel != 0.0f)
        buffer.applyGain (juce::Decibels::decibelsToGain (mLevel, -60.0f));
}

void DistortionStyleDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("DistortionStyleDSP");
    state.setProperty ("dist",  mDist,  nullptr);
    state.setProperty ("tone",  mTone,  nullptr);
    state.setProperty ("level", mLevel, nullptr);
    state.setProperty ("bypassed", (int) bypassed, nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void DistortionStyleDSP::setStateInformation (const void* data, int sz)
{
    auto xml = SafeXml::parseBinaryBlob (data, sz);
    if (! xml || ! xml->hasTagName ("DistortionStyleDSP")) return;
    auto state = juce::ValueTree::fromXml (*xml);
    setDist  ((float)(double) state.getProperty ("dist",  0.5));
    setTone  ((float)(double) state.getProperty ("tone",  0.5));
    setLevel ((float)(double) state.getProperty ("level", 0.0));
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;
}
