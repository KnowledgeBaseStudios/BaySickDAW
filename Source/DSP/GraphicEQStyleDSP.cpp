#include "GraphicEQStyleDSP.h"

constexpr float GraphicEQStyleDSP::kFreqs[GraphicEQStyleDSP::kNumBands];

GraphicEQStyleDSP::GraphicEQStyleDSP() = default;

void GraphicEQStyleDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate,
                                   (juce::uint32) juce::jmax (1, maxBlockSize),
                                   2 };

    for (int i = 0; i < kNumBands; ++i)
    {
        mBands[i].prepare (spec);
        rebuildBand (i);
    }
}

void GraphicEQStyleDSP::reset()
{
    for (auto& b : mBands) b.reset();
}

void GraphicEQStyleDSP::rebuildBand (int idx)
{
    if (idx < 0 || idx >= kNumBands) return;
    const float gain = juce::Decibels::decibelsToGain (mGainsDb[idx]);
    auto coefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter
                    (mSampleRate, kFreqs[idx], 1.4f, gain);
    *mBands[idx].state = *coefs;
}

void GraphicEQStyleDSP::setBandDb (int idx, float db)
{
    if (idx < 0 || idx >= kNumBands) return;
    db = juce::jlimit (-15.0f, 15.0f, db);
    if (! juce::approximatelyEqual (mGainsDb[idx], db))
    {
        mGainsDb[idx] = db;
        rebuildBand (idx);
    }
}

float GraphicEQStyleDSP::getBandDb (int idx) const
{
    if (idx < 0 || idx >= kNumBands) return 0.0f;
    return mGainsDb[idx];
}

void GraphicEQStyleDSP::setLevelDb (float db)
{
    mLevelDb = juce::jlimit (-60.0f, 12.0f, db);
}

void GraphicEQStyleDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    juce::dsp::AudioBlock<float> blk (buffer);
    auto sub = blk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
    juce::dsp::ProcessContextReplacing<float> ctx (sub);

    for (auto& b : mBands) b.process (ctx);

    // Master level (-inf treated as -60 dB floor).
    if (mLevelDb <= -59.99f)
        buffer.clear();
    else if (! juce::approximatelyEqual (mLevelDb, 0.0f))
        buffer.applyGain (juce::Decibels::decibelsToGain (mLevelDb, -60.0f));
}

void GraphicEQStyleDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("GraphicEQStyleDSP");
    for (int i = 0; i < kNumBands; ++i)
        state.setProperty ("band" + juce::String (i), mGainsDb[i], nullptr);
    state.setProperty ("level",    mLevelDb,       nullptr);
    state.setProperty ("bypassed", (int) bypassed, nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void GraphicEQStyleDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (! xml || ! xml->hasTagName ("GraphicEQStyleDSP")) return;
    auto state = juce::ValueTree::fromXml (*xml);
    for (int i = 0; i < kNumBands; ++i)
        setBandDb (i, (float)(double) state.getProperty ("band" + juce::String (i), 0.0));
    setLevelDb ((float)(double) state.getProperty ("level", 0.0));
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;
}
