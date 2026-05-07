#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"

// ─────────────────────────────────────────────────────────────────────────────
// WahStyleDSP - Phase I-10 (2026-05-03)
// ─────────────────────────────────────────────────────────────────────────────
// PW Style Wah pedal.  Resonant bandpass filter swept by a Pedal Position
// knob (mappable via I-3 MIDI Learn for expression-pedal control + the
// existing Phase 4D automation lane).  Two modes:
//
//   Vintage -- standard wah sweep.  Single resonant bandpass at log-mapped
//              centre frequency 400 Hz - 2.2 kHz, Q ~ 4.5.
//   Rich    -- Vintage chain + parallel ~200 Hz lowpass blended at fixed
//              level so the bass fundamental survives the bandpass.  Avoids
//              the "thin" character of classic wahs on bass guitar / dropped
//              tunings.
//
// Pedal Position (0..1) maps log-frequency: 0 = heel-down (dark, 400 Hz);
// 1 = toe-down (bright, 2.2 kHz).  Standard CryBaby behavior.
//
// MIDI Learn integration: the panel's Pedal Position knob is a regular
// VKnob.  Right-click -> MIDI Learn binds an expression pedal CC (or pitch
// bend, mod wheel, etc. via the I-3c registry).  Generic VKnobAutomation
// applicator path (I-10a polish) drives the DSP setter from the registry.
// ─────────────────────────────────────────────────────────────────────────────

class WahStyleDSP : public DSPBase
{
public:
    enum class Mode : int
    {
        Vintage = 0,
        Rich    = 1
    };

    WahStyleDSP() = default;
    ~WahStyleDSP() override = default;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    void setPedalPosition (float v01);   // 0..1
    void setMode          (int   m);

    float mPedalPosition { 0.5f };
    Mode  mMode          { Mode::Vintage };

private:
    void updateCutoff();

    juce::dsp::StateVariableTPTFilter<float> mBandpass;
    juce::dsp::StateVariableTPTFilter<float> mBassLpf;   // Rich-mode parallel LP

    juce::AudioBuffer<float> mBassScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WahStyleDSP)
};
