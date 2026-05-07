#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include "PolyphaseOversampler.h"

// ─────────────────────────────────────────────────────────────────────────────
// DistortionStyleDSP - Phase I-5 (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// DS Style Distortion pedal.  Aggressive hard-clipping in the style of the
// BOSS DS-1.  Three knobs (Tone / Level / Dist).
//
// DSP chain:
//   1. Pre-emphasis high-shelf at ~1 kHz, +12 dB at max drive.  Tightens the
//      distortion by accenting upper-mids/highs into the clipper.
//   2. 4x oversample.
//   3. Hard clipper: std::clamp (x, -1, +1) at the boosted level so the
//      shaped wave squares off completely.  Symmetrical clipping.
//   4. Downsample.
//   5. Big-Muff-style tilt EQ (Tone): parallel LPF (~400 Hz) and HPF (~2 kHz)
//      paths crossfaded by Tone with a fixed mid-scoop at noon.
//   6. Output level.
// ─────────────────────────────────────────────────────────────────────────────

class DistortionStyleDSP : public DSPBase
{
public:
    DistortionStyleDSP() = default;
    ~DistortionStyleDSP() override = default;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;
    int  getLatencySamples() const override            { return mOs.getLatencySamples(); }

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    void setDist  (float v01);   // 0..1   (drive amount)
    void setTone  (float v01);   // 0..1   (LPF<-->HPF tilt)
    void setLevel (float dB);    // -24..+12

    float mDist  { 0.5f };
    float mTone  { 0.5f };
    float mLevel { 0.0f };

private:
    void updatePreEmphasisCoefs();
    void updateToneEQCoefs();

    PolyphaseOversampler4x mOs;

    using StereoFilter = juce::dsp::ProcessorDuplicator<
                            juce::dsp::IIR::Filter<float>,
                            juce::dsp::IIR::Coefficients<float>>;
    StereoFilter mPreEmphasis;   // High-shelf accentuating the upper-mids
    StereoFilter mToneLpf;       // Big-Muff parallel LPF path (~400 Hz)
    StereoFilter mToneHpf;       // Big-Muff parallel HPF path (~2 kHz)

    juce::AudioBuffer<float> mScratchHpf;   // for the parallel HPF path

    // I-5 polish (2026-05-02): defensive 5 Hz DC blocker.  The hard-clip
    // stage is symmetric so silence in -> silence out, but the pre-emphasis
    // shelf and Big-Muff parallel filter pair can ring residual DC during
    // bypass-ramp transients.  Cheap insurance.
    float mDcXL { 0.0f }, mDcYL { 0.0f };
    float mDcXR { 0.0f }, mDcYR { 0.0f };
    float mDcCoef { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistortionStyleDSP)
};
