#pragma once
#include "DSPBase.h"

// Gate - QA-Fe2 (2026-07-16).  Vocal-channel noise gate, slot 0 of the locked
// vocal chain (distinct from the pedal NoiseGateStyleDSP: channel-strip
// control set, not pedal-grade).  Stereo-linked peak detector, 3 dB Schmitt
// hysteresis, hold stage, per-sample gain smoothing.  Defaults are fully
// transparent (threshold at the floor) so existing projects hear no change.
class GateDSP : public DSPBase
{
public:
    GateDSP();

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    float getGainReductionDb() const noexcept override
        { return mGrDb.load (std::memory_order_relaxed); }

    // Parameter setters (all CPU-guarded: no-op when unchanged)
    void setThresholdDb (float dB);
    void setRangeDb     (float dB);
    void setAttackMs    (float ms);
    void setHoldMs      (float ms);
    void setReleaseMs   (float ms);

    // Parameters are public for UI read; use the setters to change.
    float thresholdDb { -80.0f };
    float rangeDb     { -60.0f };
    float attackMs    { 1.0f };
    float holdMs      { 50.0f };
    float releaseMs   { 100.0f };

private:
    void calcCoefs();

    static constexpr float kHystDb = 3.0f;   // Schmitt gap: open at thr, close at thr-3

    float mAttCoef  { 0.0f };
    float mRelCoef  { 0.0f };
    float mDetCoef  { 0.0f };   // detector release (~5 ms fixed)
    int   mHoldSamples { 0 };

    float mEnv      { 0.0f };   // linear peak follower (stereo max)
    float mGain     { 1.0f };
    bool  mOpen     { false };
    int   mHoldRemain { 0 };

    std::atomic<float> mGrDb { 0.0f };
};
