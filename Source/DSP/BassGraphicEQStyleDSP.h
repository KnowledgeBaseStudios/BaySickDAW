#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"

// ─────────────────────────────────────────────────────────────────────────────
// BassGraphicEQStyleDSP — Phase I-12 (2026-05-03)
// ─────────────────────────────────────────────────────────────────────────────
// GEB Style Bass Graphic EQ.  7 fixed peaking biquads at the classic
// MXR M-108-style bass 7-band frequencies + master output fader.
//
// Bands: 50 / 120 / 400 / 500 / 800 / 4.5k / 10k Hz
// Q:     1.4 fixed
// Gain:  +/-15 dB per band
// Level: -inf to +12 dB master fader (8th slider)
// ─────────────────────────────────────────────────────────────────────────────

class BassGraphicEQStyleDSP : public DSPBase
{
public:
    static constexpr int kNumBands = 7;

    BassGraphicEQStyleDSP();
    ~BassGraphicEQStyleDSP() override = default;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    void  setBandDb (int bandIdx, float db);
    float getBandDb (int bandIdx) const;
    void  setLevelDb (float db);
    float getLevelDb() const { return mLevelDb; }

    static constexpr float kFreqs[kNumBands] = {
        50.0f, 120.0f, 400.0f, 500.0f, 800.0f, 4500.0f, 10000.0f
    };

private:
    void rebuildBand (int idx);

    using IIRDup = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                    juce::dsp::IIR::Coefficients<float>>;

    double mSampleRate { 44100.0 };
    IIRDup mBands[kNumBands];
    float  mGainsDb[kNumBands] { 0,0,0,0,0,0,0 };
    float  mLevelDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassGraphicEQStyleDSP)
};
