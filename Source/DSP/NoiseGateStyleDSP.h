#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"

// ─────────────────────────────────────────────────────────────────────────────
// NoiseGateStyleDSP - Phase I-8 (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// NS Style Noise Gate pedal.  RMS-detector-driven downward expander.  Use
// case: gate the buzz/hum that drive pedals upstream amplify when the player
// isn't picking notes.
//
// Algorithm:
//   1. Detector source: DI mode reads the strip's sidechain (clean signal
//      tapped pre-distortion); Self mode reads the gate's own input.  DI is
//      the default and the only sane choice when this gate sits AFTER
//      drive pedals -- using Self there would gate ON the distortion, which
//      blooms below threshold and never falls quiet enough to close.
//   2. RMS envelope follower with a 5 ms window.  Sample-by-sample the
//      detector signal feeds a one-pole averager whose time constant is
//      5 ms; the envelope is jlimit(envSqrt, threshold, max).
//   3. Compare envelope to threshold (dB).  Below threshold, multiply the
//      audio by a gain-reduction coefficient that ramps to silence (Mute
//      mode) or to a fixed range floor (Reduction mode -- typically -20 dB).
//   4. Decay knob = release time of the gate (how fast it closes after the
//      input falls below threshold).  Range 1..1000 ms.
// ─────────────────────────────────────────────────────────────────────────────

class NoiseGateStyleDSP : public DSPBase
{
public:
    enum class Mode : int
    {
        Reduction = 0,   // -20 dB floor below threshold
        Mute      = 1    // -60 dB floor (effectively silent)
    };

    enum class Source : int
    {
        DI   = 0,        // sidechain bus (clean pre-drive tap)
        Self = 1         // own input
    };

    NoiseGateStyleDSP() = default;
    ~NoiseGateStyleDSP() override = default;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    bool usesSidechain() const noexcept override { return true; }

    void setThresholdDb (float dB);   // -60..0
    void setDecayMs     (float ms);   // 1..1000
    void setMode        (int   m);
    void setSource      (int   s);

    float  mThresholdDb { -40.0f };
    float  mDecayMs     { 100.0f };
    Mode   mMode        { Mode::Reduction };
    Source mSource      { Source::DI };

private:
    void recomputeCoefs();

    float mEnvL { 0.0f }, mEnvR { 0.0f };   // running RMS-of-square envelope
    float mGainL { 1.0f }, mGainR { 1.0f }; // applied gain (smoothed via decay)
    float mRmsCoef    { 0.0f };
    float mDecayCoef  { 0.0f };

    // Cached linear floor for Reduction mode (-20 dB) and Mute mode (-60 dB).
    static constexpr float kReductionFloorDb = -20.0f;
    static constexpr float kMuteFloorDb      = -60.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseGateStyleDSP)
};
