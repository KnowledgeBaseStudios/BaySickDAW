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
//   2. Envelope-driven dual-stage waveshaper (QA-EffectsReview Task 5):
//        stage 1 -- asymmetric soft-clip  y1 = tanh(drive*x + 0.2) - 0.19
//                   (even-harmonic-rich, single-ended-tube character),
//        then cross-fade y1 -> a cubic soft-clip (1.5a - 0.5a^3) as |y1| rises,
//        shifting the dominant harmonic 2nd -> 3rd as the player digs in.
//   3. Downsample.
//   4. 1st-order LPF tone stack; cutoff sweeps 500 Hz - 5 kHz from the Tone knob.
//   5. ~100 Hz +3.5 dB body peak (QA-EffectsReview Task 5: the low-mid bump).
//   6. Output level knob (linear gain).
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

    using StereoIIR = juce::dsp::ProcessorDuplicator<
                         juce::dsp::IIR::Filter<float>,
                         juce::dsp::IIR::Coefficients<float>>;
    StereoIIR mToneLpf;
    StereoIIR mBodyPeak;   // QA-EffectsReview Task 5: ~100 Hz +3.5 dB body bump (fixed coefs)

    // 5 Hz DC blocker state (per channel).  The asymmetric tanh(x+0.2)-0.19
    // shaper produces ~0.0074 of static DC at silence; without this blocker
    // the LPF passes that DC through to the output and the dBFS meter
    // registers ~-42 dB even when the input is fully silent.
    float mDcXL { 0.0f }, mDcYL { 0.0f };
    float mDcXR { 0.0f }, mDcYR { 0.0f };
    float mDcCoef { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BluesDriveStyleDSP)
};
