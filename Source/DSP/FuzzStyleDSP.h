#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include "PolyphaseOversampler.h"

// ─────────────────────────────────────────────────────────────────────────────
// FuzzStyleDSP — Phase I-5 (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// FZ Style Fuzz pedal.  Three-mode multi-character fuzz.  Knobs: Level / Mode /
// Fuzz (Drive).
//
// Modes:
//   M  -- Maestro FZ-1A bias-starved.  Asymmetric gated clip: positive half
//         clamps hard at +0.5; negative half passes much more freely with a
//         soft tanh shape.  Produces the squelchy, gated, wasp-like fuzz.
//   F  -- Fuzz Face warm germanium.  Symmetric soft tanh with a gentle knee.
//         Smoother than the bias-starved mode, rounder mid character.
//   O  -- Octavia (upper-octave fuzz).  Full-wave rectifier (|x|) followed by
//         hard clip.  Creates a doubled-frequency upper octave artefact.
//
// All modes are 4x oversampled.
// ─────────────────────────────────────────────────────────────────────────────

class FuzzStyleDSP : public DSPBase
{
public:
    enum class Mode : int
    {
        M = 0,   // Maestro FZ-1A bias-starved
        F = 1,   // Fuzz Face germanium
        O = 2    // Octavia upper-octave
    };

    FuzzStyleDSP() = default;
    ~FuzzStyleDSP() override = default;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;
    int  getLatencySamples() const override            { return mOs.getLatencySamples(); }

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    void setFuzz  (float v01);   // 0..1   (drive amount)
    void setMode  (int   m);     // 0..2   (M / F / O)
    void setLevel (float dB);    // -24..+12

    float mFuzz  { 0.5f };
    Mode  mMode  { Mode::F };
    float mLevel { 0.0f };

private:
    PolyphaseOversampler4x mOs;

    // 5 Hz DC blocker per channel.  Octavia (Mode O) is the worst case --
    // its full-wave-rectifier path produces a constant -0.5 pedestal at
    // silence (after the static -0.5 offset).  The bias-starved Mode M also
    // injects asymmetric DC.  Without this blocker the dBFS meter registers
    // strong levels at silence and bypass-toggle clicks.
    float mDcXL { 0.0f }, mDcYL { 0.0f };
    float mDcXR { 0.0f }, mDcYR { 0.0f };
    float mDcCoef { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FuzzStyleDSP)
};
