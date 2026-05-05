#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include "PolyphaseOversampler.h"

// ─────────────────────────────────────────────────────────────────────────────
// BassOverdriveStyleDSP — Phase I-6 (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// ODB Style Bass Overdrive pedal.  Full-range hard-clipping with a heavy
// dry/wet blend and a post-blend Active Baxandall shelving EQ.  Knobs:
// Level / EQ-High / EQ-Low / Balance / Gain.
//
// DSP chain:
//   1. 4x oversample.
//   2. Hard clip via std::clamp at the boosted (Gain knob) level.
//   3. Downsample.
//   4. Balance: dry/wet blend between the un-clipped input and the clipped
//      output.  Active Baxandall pedals classically run at a heavy 50/50
//      mix by default so the bass guitar fundamental survives while the
//      grit lives on top.
//   5. Active Baxandall shelving EQ AFTER the blend (low shelf @ ~100 Hz,
//      high shelf @ ~3 kHz).  This is the "Active EQ" character of the
//      Boss ODB-3 model -- the EQ shapes the post-blend signal, not just
//      the wet path.
//   6. 5 Hz DC blocker (defensive -- hard-clip is symmetric but the active
//      EQ stages can store residual DC).
//   7. Output Level.
// ─────────────────────────────────────────────────────────────────────────────

class BassOverdriveStyleDSP : public DSPBase
{
public:
    BassOverdriveStyleDSP() = default;
    ~BassOverdriveStyleDSP() override = default;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;
    int  getLatencySamples() const override            { return mOs.getLatencySamples(); }

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    void setGain    (float v01);   // 0..1   (drive amount)
    void setBalance (float v01);   // 0..1   (dry/wet)
    void setEqLow   (float dB);    // -15..+15
    void setEqHigh  (float dB);    // -15..+15
    void setLevel   (float dB);    // -24..+12

    float mGain    { 0.5f };
    float mBalance { 0.5f };
    float mEqLow   { 0.0f };
    float mEqHigh  { 0.0f };
    float mLevel   { 0.0f };

private:
    void updateEqCoefs();

    PolyphaseOversampler4x mOs;

    using StereoFilter = juce::dsp::ProcessorDuplicator<
                            juce::dsp::IIR::Filter<float>,
                            juce::dsp::IIR::Coefficients<float>>;
    StereoFilter mLowShelf;    // ~100 Hz
    StereoFilter mHighShelf;   // ~3 kHz

    juce::AudioBuffer<float> mDryScratch;   // dry blend reservoir

    // 5 Hz DC blocker.
    float mDcXL { 0.0f }, mDcYL { 0.0f };
    float mDcXR { 0.0f }, mDcYR { 0.0f };
    float mDcCoef { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassOverdriveStyleDSP)
};
