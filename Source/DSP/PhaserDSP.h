#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include <vector>

// -- PhaserDSP ----------------------------------------------------------------
// All-pass phaser with selectable LFO wave, feedback, wet/dry mix, and
// runtime-dynamic stage count (1-24). Optional BPM sync with selectable
// note-division (8 positions).
//
// 5F-9 sec.7 quality pass applied:
//   - sec.7a log-scaled LFO -> frequency mapping (per-sample)
//   - sec.7b always-allocated 24-stage buffer (setStages is pure int assign,
//     no click on count change)
//   - sec.7c SmoothedValue on Rate / Feedback / MinDepth / MaxDepth
//   - sec.7d explicit InvertFeedback toggle
//
// Phase A retrospective (2026-04-18):
//   - A1 ScopedNoDenormals in process()
//   - A2 SmoothedValue on Wet
//   - A3 SmoothedValue on Stereo (degrees)
//   - A4 while-wrap on LFO phase (was std::fmod)
//   - A5 SmoothedValue on OutGain (dB)
//   - A6 single-branch wrap on stereo-offset LFO phase
//   - A7 BPM-sync Rate-knob soft-lockout handled by panel
//   - C1 Slow/Fast Range toggle REWIRED to clamp Rate (0-2 Hz vs 0-10 Hz)
//   - C2 Sync-division chicken-head (8 positions; default 1/4 matches v1)
//   - C3 LFO wave selector (Sine/Triangle/Saw/S&H; default Sine matches v1)
//   - C4 Rate knob log-skew handled by panel (UI-only)
//   - C5 Cross-channel feedback knob (0-1, default 0 = current behavior)
// -----------------------------------------------------------------------------
class PhaserDSP : public DSPBase
{
public:
    PhaserDSP()  = default;
    ~PhaserDSP() override = default;

    // DSPBase interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;
    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;
    void setHostBPM          (double bpm)               override;

    // Original setters (preserved for backward compat)
    void setRate     (float hz);    // LFO rate (clamped by current Range)
    void setDepth    (float depth); // 0-1, legacy mapping onto Min/Max
    void setFeedback (float fb);    // -1.2 .. +1.2
    void setWet      (float wet);   // 0-1
    void setSyncBPM  (bool sync);

    // Extended setters
    void setFreqRange      (int range);       // 0=Slow (Rate 0-2 Hz), 1=Fast (Rate 0-10 Hz) -- C1
    void setSweepFreq      (float hz);        // LFO rate (clamped by current Range)
    void setMinDepth       (float hz);
    void setMaxDepth       (float hz);
    void setStereo         (float degrees);   // L/R LFO phase offset (0-360)
    void setStages         (int numStages);   // 1-24 active stage count
    void setOutGain        (float dB);
    void setInvertFeedback (bool on);
    void setLFOWave        (int waveIdx);     // C3: 0=Sine 1=Triangle 2=Saw 3=S&H
    void setSyncDiv        (int divIdx);      // C2: 0..7 index into kSyncDivisions
    void setCrossFB        (float amount);    // C5: 0..1 cross-channel feedback amount

    // Helper used by the panel for C1 UI range mirroring.
    float getRateMaxHz() const noexcept { return mFreqRange == 0 ? 2.0f : 10.0f; }

    // C2 sync-division table (same shape as Flanger).
    struct SyncDiv { const char* label; int num; int den; };
    static constexpr int kNumSyncDivisions = 8;
    static const SyncDiv kSyncDivisions[kNumSyncDivisions];

    float mRate       { 0.5f };
    float mManualRate { 0.5f };   // (c) shadow of last manual rate; restored on un-sync
    float mDepth      { 0.8f };
    float mFeedback   { 0.5f };
    float mWet        { 0.5f };
    bool  mSyncBPM    { false };

    // Extended params
    int   mFreqRange      { 1 };          // C1: 0=Slow (<=2 Hz), 1=Fast (<=10 Hz) -- default Fast matches v1 unrestricted
    float mSweepHz        { 0.5f };
    float mMinDepthHz     { 200.0f };
    float mMaxDepthHz     { 2000.0f };
    float mStereoPhase    { 0.0f };
    int   mNumStages      { 4 };
    float mOutGainDb      { 0.0f };
    bool  mInvertFeedback { false };
    int   mLFOWaveIdx     { 0 };          // C3: default Sine = v1
    int   mSyncDivIdx     { 2 };          // C2: default 1/4 matches v1 hardcoded (BPM/60/4)
    float mCrossFB        { 0.0f };       // C5: default 0 = no cross-feed (v1)

private:
    static constexpr int kMaxStages = 24;
    std::vector<float> mXL, mYL;
    std::vector<float> mXR, mYR;

    float mFbL { 0.0f };
    float mFbR { 0.0f };

    double mPhase   { 0.0 };
    double mHostBPM { 120.0 };

    // C3 S&H state (one held value per channel; resampled on per-channel LFO-phase wrap).
    float  mSHL         { 0.0f };
    float  mSHR         { 0.0f };
    double mLastPhaseL  { 0.0 };
    double mLastPhaseR  { 0.0 };
    juce::Random mSHRng;

    // Smoothers
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mRateSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mFeedbackSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mMinDepthSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mMaxDepthSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mWetSmooth;        // A2
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mStereoSmooth;     // A3 (stored in 0..1 cycle fraction)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mOutGainSmooth;    // A5 (linear gain)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mCrossFBSmooth;    // C5

    void snapSmoothedToTargets();

    // Internal: re-derive mRate from BPM + current sync-division (used by setSyncBPM / setSyncDiv / setHostBPM).
    void reapplyBpmSync();
};
