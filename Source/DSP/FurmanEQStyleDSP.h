#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"

// ─────────────────────────────────────────────────────────────────────────────
// FurmanEQStyleDSP — Phase I-12 (2026-05-03)
// ─────────────────────────────────────────────────────────────────────────────
// EQFH Style Pro Parametric EQ.  Models the Furman PQ-3 hardware:
//   * Master Input Volume (preamp gain stage with sigmoid soft-clip,
//     emulating the +86 dB total hardware gain range).
//   * 3 fully parametric bands (Low / Mid / High) using IIR peak filters.
//   * Global EQ Bypass switch -- bypasses the 3 bands ONLY; preamp saturation
//     stays active (per locked spec).  This is distinct from the slot-bypass
//     LED (DSPBase::bypassed) which bypasses the WHOLE effect.
//
// Per-band ranges:
//   Low  Frequency:  25 Hz - 500 Hz
//   Mid  Frequency:  150 Hz - 2.5 kHz
//   High Frequency:  600 Hz - 10 kHz
//   Boost (each):    -inf dB to +20 dB (skewed; 0 dB sits at noon)
//   Bandwidth (Q):   0.1 - 10 (default 0.7)
//
// Master Input Volume: 0 dB to +86 dB.  Above ~+12 dB the sigmoid soft-clip
// stage (`std::tanh`) starts shaving peaks; at the upper end it acts as a
// hard limiter with audible analog character.
// ─────────────────────────────────────────────────────────────────────────────

class FurmanEQStyleDSP : public DSPBase
{
public:
    static constexpr int kNumBands = 3;

    FurmanEQStyleDSP();
    ~FurmanEQStyleDSP() override = default;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    // Per-band setters (idx 0=Low, 1=Mid, 2=High).
    void  setFreq      (int idx, float hz);
    void  setBoostDb   (int idx, float db);   // -60..+20 (-60 acts as -inf)
    void  setQ         (int idx, float q);    // 0.1..10
    float getFreq      (int idx) const;
    float getBoostDb   (int idx) const;
    float getQ         (int idx) const;

    void  setInputVolDb (float db);   // 0..+86
    float getInputVolDb() const { return mInputVolDb; }

    void  setEqBypassed (bool yes);
    bool  isEqBypassed() const { return mEqBypassed; }

    // Per-band valid frequency ranges.
    static constexpr float kFreqMin[kNumBands] = {  25.0f, 150.0f, 600.0f };
    static constexpr float kFreqMax[kNumBands] = { 500.0f, 2500.0f, 10000.0f };

    // Default frequencies (Furman PQ-3 hardware defaults: ~80 Hz / 1 kHz / 5 kHz).
    static constexpr float kFreqDef[kNumBands] = { 80.0f, 1000.0f, 5000.0f };

private:
    void rebuildBand (int idx);

    using IIRDup = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                    juce::dsp::IIR::Coefficients<float>>;

    double mSampleRate { 44100.0 };
    IIRDup mBands[kNumBands];

    float mFreq[kNumBands]   { kFreqDef[0], kFreqDef[1], kFreqDef[2] };
    float mBoost[kNumBands]  { 0.0f, 0.0f, 0.0f };
    float mQ[kNumBands]      { 0.7f, 0.7f, 0.7f };

    float mInputVolDb { 0.0f };
    bool  mEqBypassed { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FurmanEQStyleDSP)
};
