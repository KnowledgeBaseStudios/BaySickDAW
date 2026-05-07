#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include "PolyphaseOversampler.h"

// ─────────────────────────────────────────────────────────────────────────────
// BassDriverStyleDSP - Phase I-6 (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// BB Style Bass Driver pedal.  Multi-band drive in the style of the Tech 21
// SansAmp Bass Driver DI.  3-band Linkwitz-Riley split (Low / Mid / High);
// the Low band bypasses the clipper entirely for clean VCA punch on the
// fundamental, while Mid + High bands run through an asymmetric soft-clip
// at 4x oversampled.  Knobs: Level / Blend / Low / High / Drive.
//
// DSP chain:
//   1. LR4 split into 3 bands at 200 Hz and 2 kHz crossovers.
//   2. Low band: gain = mLow (no clipping).
//   3. Mid + High bands (separately, then summed): 4x oversample the band
//      sum, asymmetric soft-clip (tanh(x+0.2)-tanh(0.2)), downsample.
//   4. Per-band trim: High band scaled by mHigh.
//   5. Blend: dry/wet on the clipped Mid+High mix; 0 = all clean, 1 = all
//      clipped.
//   6. Sum all bands.
//   7. 5 Hz DC blocker (asymmetric shaper leaves residual bias at silence).
//   8. Output Level.
// ─────────────────────────────────────────────────────────────────────────────

class BassDriverStyleDSP : public DSPBase
{
public:
    BassDriverStyleDSP() = default;
    ~BassDriverStyleDSP() override = default;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;
    int  getLatencySamples() const override            { return mOs.getLatencySamples(); }

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    void setDrive (float v01);   // 0..1
    void setBlend (float v01);   // 0..1
    void setLow   (float v01);   // 0..1
    void setHigh  (float v01);   // 0..1
    void setLevel (float dB);    // -24..+12

    float mDrive { 0.5f };
    float mBlend { 0.7f };
    float mLow   { 0.7f };
    float mHigh  { 0.7f };
    float mLevel { 0.0f };

private:
    PolyphaseOversampler4x mOs;

    // Linkwitz-Riley 4th-order crossovers.  Two splitters give 3 bands:
    //   mXover1 (200 Hz)  -> low band + (mid + high)
    //   mXover2 (2 kHz)   -> mid band + high band
    juce::dsp::LinkwitzRileyFilter<float> mXover1Lp, mXover1Hp;   // 200 Hz
    juce::dsp::LinkwitzRileyFilter<float> mXover2Lp, mXover2Hp;   // 2 kHz

    juce::AudioBuffer<float> mLowBuf;
    juce::AudioBuffer<float> mMidBuf;
    juce::AudioBuffer<float> mHighBuf;
    juce::AudioBuffer<float> mClippedSumBuf;   // mid+high after clipping
    juce::AudioBuffer<float> mCleanSumBuf;     // mid+high pre-clip (for Blend)

    // 5 Hz DC blocker (asymmetric soft-clip leaves residual bias at silence).
    float mDcXL { 0.0f }, mDcYL { 0.0f };
    float mDcXR { 0.0f }, mDcYR { 0.0f };
    float mDcCoef { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassDriverStyleDSP)
};
