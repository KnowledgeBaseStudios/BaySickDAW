#include "BluesDriveStyleDSP.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)

namespace
{
    // Tone-stack sweep: 500 Hz (knob = 0) -> 5 kHz (knob = 1), log-mapped.
    constexpr float kToneMinHz = 500.0f;
    constexpr float kToneMaxHz = 5000.0f;

    // Pre-shaper drive scalar mapping.  Drive = 0 -> ~1.0 (clean unity);
    // Drive = 1 -> ~30 (deep saturation, comparable to BD-2 max position).
    inline float driveAmount (float drive01) noexcept
    {
        const float d = juce::jlimit (0.0f, 1.0f, drive01);
        return 1.0f + d * 29.0f;
    }
}

void BluesDriveStyleDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;

    // 4x oversampler - stereo so we can process the buffer in place.
    mOs.prepare (2, maxBlockSize);

    juce::dsp::ProcessSpec spec { sampleRate,
                                   (juce::uint32) juce::jmax (1, maxBlockSize),
                                   2 };
    mToneLpf.prepare (spec);
    mToneLpf.reset();
    updateToneCoefs();

    // QA-EffectsReview Task 5: fixed ~100 Hz +3.5 dB body peak (the low-mid bump).
    mBodyPeak.prepare (spec);
    mBodyPeak.reset();
    *mBodyPeak.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        (double) sampleRate, 100.0f, 0.7071f, juce::Decibels::decibelsToGain (3.5f));

    // 5 Hz DC blocker: y[n] = x[n] - x[n-1] + R*y[n-1].
    // R = 1 - 2*pi*5/sr ≈ 0.999 at 44.1 kHz.
    mDcCoef = 1.0f - (float) (juce::MathConstants<double>::twoPi * 5.0 / sampleRate);
    mDcXL = mDcYL = mDcXR = mDcYR = 0.0f;
}

void BluesDriveStyleDSP::reset()
{
    mOs.reset();
    mToneLpf.reset();
    mBodyPeak.reset();
    mDcXL = mDcYL = mDcXR = mDcYR = 0.0f;
}

void BluesDriveStyleDSP::updateToneCoefs()
{
    if (mSampleRate <= 0.0) return;
    // Log-frequency sweep (musical mapping).
    const float t   = juce::jlimit (0.0f, 1.0f, mTone);
    const float hz  = kToneMinHz * std::pow (kToneMaxHz / kToneMinHz, t);
    *mToneLpf.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (
        (double) mSampleRate, hz);
}

void BluesDriveStyleDSP::setDrive (float v01)
{
    v01 = juce::jlimit (0.0f, 1.0f, v01);
    if (mDrive != v01) mDrive = v01;
}

void BluesDriveStyleDSP::setTone (float v01)
{
    v01 = juce::jlimit (0.0f, 1.0f, v01);
    if (mTone != v01) { mTone = v01; updateToneCoefs(); }
}

void BluesDriveStyleDSP::setLevel (float dB)
{
    dB = juce::jlimit (-24.0f, 12.0f, dB);
    if (mLevel != dB) mLevel = dB;
}

void BluesDriveStyleDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    juce::dsp::AudioBlock<float> block (buffer);

    // 1. Upsample to 4x.
    auto upBlock = mOs.upsample (block);

    // 2. Envelope-driven dual-stage shaper at 4x (QA-EffectsReview Task 5).
    //    Stage 1: asymmetric tanh (pre-add 0.2 DC, post-subtract 0.19 -- the
    //    0.01 residual is the intentional bias floor; the DC blocker strips it).
    //    Then cross-fade stage 1 -> a cubic soft-clip (1.5a - 0.5a^3) by |s1|,
    //    so the dominant harmonic shifts 2nd -> 3rd as the player digs in.
    const float drive = driveAmount (mDrive);
    const int upN     = (int) upBlock.getNumSamples();
    const int upChs   = (int) upBlock.getNumChannels();
    for (int ch = 0; ch < upChs; ++ch)
    {
        float* d = upBlock.getChannelPointer ((size_t) ch);
        for (int i = 0; i < upN; ++i)
        {
            const float s1  = std::tanh (d[i] * drive + 0.2f) - 0.19f;   // asym (2nd)
            const float a   = juce::jlimit (-1.0f, 1.0f, s1);
            const float cub = 1.5f * a - 0.5f * a * a * a;               // cubic (3rd)
            const float mix = juce::jlimit (0.0f, 1.0f, std::abs (s1));  // louder -> more cubic
            d[i] = s1 + mix * (cub - s1);
        }
    }

    // 3. Downsample back to 1x in-place.
    mOs.downsample (block);

    // 4. Tone-stack LPF at 1x (cheap; no aliasing concern post-shaper).
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    mToneLpf.process (ctx);

    // 5. ~100 Hz +3.5 dB body peak (QA-EffectsReview Task 5: the low-mid bump).
    mBodyPeak.process (ctx);

    // 6. 5 Hz DC blocker -- strips the static 0.0074 bias the shaper leaves
    // at silence.  Without this the dBFS meter registers ~-42 dB at rest
    // and toggling bypass clicks because the bypassed signal is 0 but the
    // un-bypassed signal sits on a DC pedestal.
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

void BluesDriveStyleDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("BluesDriveStyleDSP");
    state.setProperty ("drive", mDrive, nullptr);
    state.setProperty ("tone",  mTone,  nullptr);
    state.setProperty ("level", mLevel, nullptr);
    state.setProperty ("bypassed", (int) bypassed, nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void BluesDriveStyleDSP::setStateInformation (const void* data, int sz)
{
    auto xml = SafeXml::parseBinaryBlob (data, sz);
    if (! xml || ! xml->hasTagName ("BluesDriveStyleDSP")) return;
    auto state = juce::ValueTree::fromXml (*xml);
    setDrive ((float)(double) state.getProperty ("drive", 0.5));
    setTone  ((float)(double) state.getProperty ("tone",  0.5));
    setLevel ((float)(double) state.getProperty ("level", 0.0));
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;
}
