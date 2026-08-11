#include "BassOverdriveStyleDSP.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)

namespace
{
    constexpr float kLowShelfHz  = 100.0f;
    constexpr float kHighShelfHz = 3000.0f;

    // QA-EffectsReview Task 5: the ODB-3 is never fully clean -- a grit floor keeps
    // a little dirt even at Gain=0 (the diodes are always in-circuit; clips peaks
    // above ~1/kGritFloor), and the gain sweeps from there up to kMaxDrive (it
    // reaches fuzz territory early, well before noon).
    constexpr float kGritFloor = 1.6f;
    constexpr float kMaxDrive  = 50.0f;
}

void BassOverdriveStyleDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;
    mOs.prepare (2, maxBlockSize);

    juce::dsp::ProcessSpec spec { sampleRate,
                                   (juce::uint32) juce::jmax (1, maxBlockSize),
                                   2 };
    mLowShelf .prepare (spec);  mLowShelf .reset();
    mHighShelf.prepare (spec);  mHighShelf.reset();
    updateEqCoefs();

    mDryScratch.setSize (2, juce::jmax (1, maxBlockSize), false, true, true);

    mDcCoef = 1.0f - (float) (juce::MathConstants<double>::twoPi * 5.0 / sampleRate);
    mDcXL = mDcYL = mDcXR = mDcYR = 0.0f;
}

void BassOverdriveStyleDSP::reset()
{
    mOs.reset();
    mLowShelf .reset();
    mHighShelf.reset();
    mDcXL = mDcYL = mDcXR = mDcYR = 0.0f;
}

void BassOverdriveStyleDSP::updateEqCoefs()
{
    if (mSampleRate <= 0.0) return;
    *mLowShelf.state  = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        (double) mSampleRate, kLowShelfHz, 0.7071f,
        juce::Decibels::decibelsToGain (mEqLow,  -60.0f));
    *mHighShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        (double) mSampleRate, kHighShelfHz, 0.7071f,
        juce::Decibels::decibelsToGain (mEqHigh, -60.0f));
}

void BassOverdriveStyleDSP::setGain    (float v01) { v01 = juce::jlimit (0.0f, 1.0f, v01); if (mGain    != v01) mGain    = v01; }
void BassOverdriveStyleDSP::setBalance (float v01) { v01 = juce::jlimit (0.0f, 1.0f, v01); if (mBalance != v01) mBalance = v01; }
void BassOverdriveStyleDSP::setEqLow   (float dB)
{
    dB = juce::jlimit (-15.0f, 15.0f, dB);
    if (mEqLow != dB) { mEqLow = dB; updateEqCoefs(); }
}
void BassOverdriveStyleDSP::setEqHigh  (float dB)
{
    dB = juce::jlimit (-15.0f, 15.0f, dB);
    if (mEqHigh != dB) { mEqHigh = dB; updateEqCoefs(); }
}
void BassOverdriveStyleDSP::setLevel   (float dB)
{
    dB = juce::jlimit (-24.0f, 12.0f, dB);
    if (mLevel != dB) mLevel = dB;
}

void BassOverdriveStyleDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    if (mDryScratch.getNumChannels() < numCh || mDryScratch.getNumSamples() < n)
        mDryScratch.setSize (numCh, n, false, false, true);
    for (int ch = 0; ch < numCh; ++ch)
        mDryScratch.copyFrom (ch, 0, buffer, ch, 0, n);

    // 1 + 2 + 3. Hard clip at 4x oversampled, from a grit floor (never fully clean).
    const float drive = kGritFloor + mGain * (kMaxDrive - kGritFloor);
    juce::dsp::AudioBlock<float> block (buffer);
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

    // 4. Balance: dry/wet blend.  0 = all dry, 1 = all wet.
    {
        const float w = juce::jlimit (0.0f, 1.0f, mBalance);
        const float dGain = 1.0f - w;
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* wet = buffer.getWritePointer (ch);
            const float* dry = mDryScratch.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
                wet[i] = dry[i] * dGain + wet[i] * w;
        }
    }

    // 5. Active Baxandall shelving EQ AFTER the blend.
    {
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        mLowShelf .process (ctx);
        mHighShelf.process (ctx);
    }

    // 6. 5 Hz DC blocker (defensive).
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

    // 7. Output level.
    if (mLevel != 0.0f)
        buffer.applyGain (juce::Decibels::decibelsToGain (mLevel, -60.0f));
}

void BassOverdriveStyleDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("BassOverdriveStyleDSP");
    state.setProperty ("gain",    mGain,    nullptr);
    state.setProperty ("balance", mBalance, nullptr);
    state.setProperty ("eqLow",   mEqLow,   nullptr);
    state.setProperty ("eqHigh",  mEqHigh,  nullptr);
    state.setProperty ("level",   mLevel,   nullptr);
    state.setProperty ("bypassed", (int) bypassed, nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void BassOverdriveStyleDSP::setStateInformation (const void* data, int sz)
{
    auto xml = SafeXml::parseBinaryBlob (data, sz);
    if (! xml || ! xml->hasTagName ("BassOverdriveStyleDSP")) return;
    auto state = juce::ValueTree::fromXml (*xml);
    setGain    ((float)(double) state.getProperty ("gain",    0.5));
    setBalance ((float)(double) state.getProperty ("balance", 0.5));
    setEqLow   ((float)(double) state.getProperty ("eqLow",   0.0));
    setEqHigh  ((float)(double) state.getProperty ("eqHigh",  0.0));
    setLevel   ((float)(double) state.getProperty ("level",   0.0));
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;
}
