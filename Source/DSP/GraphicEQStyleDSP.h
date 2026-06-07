#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"

// ─────────────────────────────────────────────────────────────────────────────
// GraphicEQStyleDSP - Phase I-12 (2026-05-03)
// ─────────────────────────────────────────────────────────────────────────────
// GE Style Graphic EQ (guitar).  7 fixed biquads (6 peaks + a high-shelf top
// band) at the classic guitar 7-band Boss GE-7 pedal frequencies + master fader.
//
// Bands: 100 / 200 / 400 / 800 / 1.6k / 3.2k / 6.4k Hz
// Q:     1.4 peaks; 6.4k band is a high-shelf (QA-EffectsReview Task 3)
// Gain:  +/-15 dB per band
// Level: +/-15 dB master fader (8th slider; matches GE-7)
// ─────────────────────────────────────────────────────────────────────────────

class GraphicEQStyleDSP : public DSPBase
{
public:
    static constexpr int kNumBands = 7;

    GraphicEQStyleDSP();
    ~GraphicEQStyleDSP() override = default;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    void  setBandDb (int bandIdx, float db);  // -15..+15
    float getBandDb (int bandIdx) const;
    void  setLevelDb (float db);              // -15..+15 (GE-7 Level slider)
    float getLevelDb() const { return mLevelDb; }

    // Frequencies are public + const for the panel to label sliders.
    static constexpr float kFreqs[kNumBands] = {
        100.0f, 200.0f, 400.0f, 800.0f, 1600.0f, 3200.0f, 6400.0f
    };

private:
    void rebuildBand (int idx);

    using IIRDup = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                    juce::dsp::IIR::Coefficients<float>>;

    double mSampleRate { 44100.0 };
    IIRDup mBands[kNumBands];
    float  mGainsDb[kNumBands] { 0,0,0,0,0,0,0 };
    float  mLevelDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphicEQStyleDSP)
};
