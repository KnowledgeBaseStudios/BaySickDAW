#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include <atomic>

// ─────────────────────────────────────────────────────────────────────────────
// BassCompressorStyleDSP — Phase I-8 (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// BC Style Bass Compressor pedal.  Multi-band feedforward compressor split
// into 3 bands by Linkwitz-Riley 4th-order crossovers (200 Hz, 2 kHz).
// Each band has an independent compressor stage; the macro Comp knob
// lowers thresholds and raises ratios across all 3 in lockstep so a single
// twist gives uniformly heavier compression.
//
// Knobs:
//   Comp    -- 0..1 macro that pushes all 3 bands deeper into compression.
//              At 0 = light per-band 2:1 with -6 dB threshold; at 1 =
//              aggressive 8:1 with -36 dB threshold.
//   Ratio   -- 1..10 fine ratio override (multiplied by macro).
//   Release -- per-band release time, 50..500 ms.
//   Level   -- output dB trim, -24..+12.
//
// Reports gain reduction via getGainReductionDb() (worst case across bands)
// for the panel's LED GR meter.
// ─────────────────────────────────────────────────────────────────────────────

class BassCompressorStyleDSP : public DSPBase
{
public:
    BassCompressorStyleDSP() = default;
    ~BassCompressorStyleDSP() override = default;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    float getGainReductionDb() const override
    {
        return mGainReductionDb.load (std::memory_order_relaxed);
    }

    void setComp     (float v01);   // 0..1 macro
    void setRatio    (float r);     // 1..10
    void setReleaseMs (float ms);   // 50..500
    void setLevel    (float dB);    // -24..+12

    float mComp     { 0.5f };
    float mRatio    { 4.0f };
    float mReleaseMs { 200.0f };
    float mLevel    { 0.0f };

private:
    void recomputeCoefs();

    // Linkwitz-Riley split: two crossovers give 3 bands.
    juce::dsp::LinkwitzRileyFilter<float> mXover1Lp, mXover1Hp;   // 200 Hz
    juce::dsp::LinkwitzRileyFilter<float> mXover2Lp, mXover2Hp;   // 2 kHz

    juce::AudioBuffer<float> mLowBuf, mMidBuf, mHighBuf;

    // Per-band compressor state.  Squared-domain envelope (avoids sqrt per
    // sample); attack fixed fast (10 ms) since fast bass-attack tracking is
    // the BC pedal's character.
    struct BandState
    {
        float envL { 0.0f }, envR { 0.0f };   // squared envelope
    };
    BandState mLow, mMid, mHigh;

    float mAttackCoef  { 0.0f };
    float mReleaseCoef { 0.0f };

    std::atomic<float> mGainReductionDb { 0.0f };
    float mGrDecayDbPerBlock { 0.35f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassCompressorStyleDSP)
};
