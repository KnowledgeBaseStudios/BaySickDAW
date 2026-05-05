#include "WahStyleDSP.h"

namespace
{
    constexpr float kSweepMinHz    = 400.0f;
    constexpr float kSweepMaxHz    = 2200.0f;
    constexpr float kBandpassQ     = 4.5f;       // resonant character
    constexpr float kBassLpfHz     = 200.0f;
    constexpr float kBassLpfBlendDb = -6.0f;     // Rich mode blend level
}

void WahStyleDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;

    juce::dsp::ProcessSpec spec { sampleRate,
                                   (juce::uint32) juce::jmax (1, maxBlockSize),
                                   2 };
    mBandpass.prepare (spec);
    mBandpass.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    mBandpass.setResonance (kBandpassQ);
    mBandpass.reset();

    mBassLpf.prepare (spec);
    mBassLpf.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    mBassLpf.setCutoffFrequency (kBassLpfHz);
    mBassLpf.setResonance (juce::MathConstants<float>::sqrt2 * 0.5f);
    mBassLpf.reset();

    mBassScratch.setSize (2, juce::jmax (1, maxBlockSize), false, true, true);

    updateCutoff();
}

void WahStyleDSP::reset()
{
    mBandpass.reset();
    mBassLpf.reset();
}

void WahStyleDSP::updateCutoff()
{
    if (mSampleRate <= 0.0) return;
    const float p  = juce::jlimit (0.0f, 1.0f, mPedalPosition);
    const float hz = kSweepMinHz * std::pow (kSweepMaxHz / kSweepMinHz, p);
    mBandpass.setCutoffFrequency (juce::jlimit (kSweepMinHz, kSweepMaxHz, hz));
}

void WahStyleDSP::setPedalPosition (float v01)
{
    v01 = juce::jlimit (0.0f, 1.0f, v01);
    if (mPedalPosition != v01) { mPedalPosition = v01; updateCutoff(); }
}

void WahStyleDSP::setMode (int m)
{
    const Mode newMode = static_cast<Mode> (juce::jlimit (0, 1, m));
    if (mMode != newMode) { mMode = newMode; if (mMode == Mode::Rich) mBassLpf.reset(); }
}

void WahStyleDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    // Rich mode: snapshot input into scratch BEFORE the bandpass overwrites it,
    // run LP on scratch, then blend back in after the bandpass stage.
    if (mMode == Mode::Rich)
    {
        if (mBassScratch.getNumChannels() < numCh || mBassScratch.getNumSamples() < n)
            mBassScratch.setSize (numCh, n, false, false, true);
        for (int ch = 0; ch < numCh; ++ch)
            mBassScratch.copyFrom (ch, 0, buffer, ch, 0, n);

        juce::dsp::AudioBlock<float> bassBlk (mBassScratch);
        auto bassSub = bassBlk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> bassCtx (bassSub);
        mBassLpf.process (bassCtx);
    }

    // Bandpass on the main buffer.
    {
        juce::dsp::AudioBlock<float> blk (buffer);
        auto sub = blk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        mBandpass.process (ctx);
    }

    // Rich mode: blend the bass LP back in.
    if (mMode == Mode::Rich)
    {
        const float blend = juce::Decibels::decibelsToGain (kBassLpfBlendDb, -60.0f);
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* wah = buffer.getWritePointer (ch);
            const float* bass = mBassScratch.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
                wah[i] += bass[i] * blend;
        }
    }
}

void WahStyleDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("WahStyleDSP");
    state.setProperty ("position", mPedalPosition, nullptr);
    state.setProperty ("mode",     (int) mMode,    nullptr);
    state.setProperty ("bypassed", (int) bypassed, nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void WahStyleDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (! xml || ! xml->hasTagName ("WahStyleDSP")) return;
    auto state = juce::ValueTree::fromXml (*xml);
    setPedalPosition ((float)(double) state.getProperty ("position", 0.5));
    setMode          ((int)            state.getProperty ("mode",     (int) Mode::Vintage));
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;
}
