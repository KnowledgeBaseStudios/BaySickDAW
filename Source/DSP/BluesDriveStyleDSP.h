#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include "PolyphaseOversampler.h"

// ─────────────────────────────────────────────────────────────────────────────
// BluesDriveStyleDSP - Phase I-5 (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// BD Style Blues Drive pedal.  Asymmetric tube-style overdrive in the style of
// the BOSS BD-2 Blues Driver.  Three knobs (Drive / Tone / Level).
//
// DSP chain:
//   1. 4x oversample (PolyphaseOversampler4x).
//   2. Asymmetric soft-clip waveshaper:  y = tanh(x + 0.2) - 0.19
//      (DC offset 0.2 pre-shaper biases the waveform into the asymmetric
//      region of tanh, then subtracted back out as a constant; net effect is
//      an even-harmonic-rich saturation similar to a single-ended tube
//      stage.)
//   3. Downsample.
//   4. 1st-order LPF tone stack (Fender-style); cutoff sweeps 500 Hz - 5 kHz
//      from the Tone knob.
//   5. Output level knob (linear gain).
// ─────────────────────────────────────────────────────────────────────────────

class BluesDriveStyleDSP : public DSPBase
{
public:
    BluesDriveStyleDSP() = default;
    ~BluesDriveStyleDSP() override = default;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;
    int  getLatencySamples() const override            { return mOs.getLatencySamples(); }

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    // Knob setters (CPU-guarded).
    void setDrive (float v01);   // 0..1   (drive amount)
    void setTone  (float v01);   // 0..1   (tone-stack cutoff sweep)
    void setLevel (float dB);    // -24..+12

    // Public state (read-only externally).
    float mDrive { 0.5f };
    float mTone  { 0.5f };
    float mLevel { 0.0f };

private:
    void updateToneCoefs();

    PolyphaseOversampler4x mOs;

    using StereoLPF = juce::dsp::ProcessorDuplicator<
                         juce::dsp::IIR::Filter<float>,
                         juce::dsp::IIR::Coefficients<float>>;
    StereoLPF mToneLpf;

    // 5 Hz DC blocker state (per channel).  The asymmetric tanh(x+0.2)-0.19
    // shaper produces ~0.0074 of static DC at silence; without this blocker
    // the LPF passes that DC through to the output and the dBFS meter
    // registers ~-42 dB even when the input is fully silent.
    float mDcXL { 0.0f }, mDcYL { 0.0f };
    float mDcXR { 0.0f }, mDcYR { 0.0f };
    float mDcCoef { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BluesDriveStyleDSP)
};
