#include "FuzzStyleDSP.h"

void FuzzStyleDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;
    mOs.prepare (2, maxBlockSize);
    mDcCoef = 1.0f - (float) (juce::MathConstants<double>::twoPi * 5.0 / sampleRate);
    mDcXL = mDcYL = mDcXR = mDcYR = 0.0f;
}

void FuzzStyleDSP::reset()
{
    mOs.reset();
    mDcXL = mDcYL = mDcXR = mDcYR = 0.0f;
}

void FuzzStyleDSP::setFuzz (float v01)
{
    v01 = juce::jlimit (0.0f, 1.0f, v01);
    if (mFuzz != v01) mFuzz = v01;
}

void FuzzStyleDSP::setMode (int m)
{
    const Mode newMode = static_cast<Mode> (juce::jlimit (0, 2, m));
    if (mMode != newMode) mMode = newMode;
}

void FuzzStyleDSP::setLevel (float dB)
{
    dB = juce::jlimit (-24.0f, 12.0f, dB);
    if (mLevel != dB) mLevel = dB;
}

void FuzzStyleDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    juce::dsp::AudioBlock<float> block (buffer);
    auto upBlock = mOs.upsample (block);
    const int upN   = (int) upBlock.getNumSamples();
    const int upChs = (int) upBlock.getNumChannels();

    // Drive scales by Fuzz (1..50).  Same range across all modes; the
    // character difference is in the shaper, not the gain.
    const float drive = 1.0f + mFuzz * 49.0f;

    switch (mMode)
    {
        case Mode::M:
        {
            // Bias-starved gated clip.  Positive half clamps tight at +0.5
            // (the "starve") while negative half passes through tanh with
            // a wider knee (the "gated" wasp character).
            for (int ch = 0; ch < upChs; ++ch)
            {
                float* d = upBlock.getChannelPointer ((size_t) ch);
                for (int i = 0; i < upN; ++i)
                {
                    const float x = d[i] * drive;
                    d[i] = (x >= 0.0f) ? std::min (x, 0.5f)
                                        : std::tanh (x * 0.7f) * 1.0f;
                }
            }
            break;
        }
        case Mode::F:
        {
            // Symmetrical soft tanh.  Warm germanium character.
            for (int ch = 0; ch < upChs; ++ch)
            {
                float* d = upBlock.getChannelPointer ((size_t) ch);
                for (int i = 0; i < upN; ++i)
                    d[i] = std::tanh (d[i] * drive);
            }
            break;
        }
        case Mode::O:
        {
            // Octavia: full-wave rectify then hard clip.  |x| produces a
            // doubled fundamental (the upper-octave artefact).  Shift back
            // toward zero-mean by subtracting 0.5 so the rectified stream
            // doesn't sit on a positive DC pedestal.
            for (int ch = 0; ch < upChs; ++ch)
            {
                float* d = upBlock.getChannelPointer ((size_t) ch);
                for (int i = 0; i < upN; ++i)
                {
                    const float r = std::abs (d[i]) * drive - 0.5f;
                    d[i] = std::clamp (r, -1.0f, 1.0f);
                }
            }
            break;
        }
    }

    mOs.downsample (block);

    // 5 Hz DC blocker -- strips the static bias the asymmetric and rectifier
    // shapers leave at silence.  Without this the Octavia mode emits ~-6 dB
    // of DC at rest and bypass-toggle clicks heavily.
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

    if (mLevel != 0.0f)
        buffer.applyGain (juce::Decibels::decibelsToGain (mLevel, -60.0f));
}

void FuzzStyleDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("FuzzStyleDSP");
    state.setProperty ("fuzz",  mFuzz,         nullptr);
    state.setProperty ("mode",  (int) mMode,   nullptr);
    state.setProperty ("level", mLevel,        nullptr);
    state.setProperty ("bypassed", (int) bypassed, nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void FuzzStyleDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (! xml || ! xml->hasTagName ("FuzzStyleDSP")) return;
    auto state = juce::ValueTree::fromXml (*xml);
    setFuzz  ((float)(double) state.getProperty ("fuzz",  0.5));
    setMode  ((int)           state.getProperty ("mode",  (int) Mode::F));
    setLevel ((float)(double) state.getProperty ("level", 0.0));
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;
}
