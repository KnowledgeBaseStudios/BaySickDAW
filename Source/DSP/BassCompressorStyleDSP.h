#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include <atomic>

// ─────────────────────────────────────────────────────────────────────────────
// BassCompressorStyleDSP - Phase I-8 (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// BC Style Bass Compressor pedal (BOSS BC-1X).  Multi-band feedforward
// compressor split into 3 bands by Linkwitz-Riley 4th-order crossovers
// (200 Hz, 2 kHz) -- models the BC-1X's MDP multi-band character.
//
// Knobs (the BC-1X's four discrete controls):
//   Thresh  -- discrete threshold, -48..0 dB (QA-EffectsReview Task 6 split this
//              out of the old 0..1 "Comp" macro that bundled threshold + ratio).
//   Ratio   -- 1..10, applied directly to all bands.
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

    void setThresholdDb (float dB);   // -48..0
    void setRatio       (float r);    // 1..10
    void setReleaseMs   (float ms);   // 50..500
    void setLevel       (float dB);   // -24..+12

    float mThresholdDb { -24.0f };
    float mRatio       {   4.0f };
    float mReleaseMs   { 200.0f };
    float mLevel       {   0.0f };

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
