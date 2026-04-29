#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include <vector>

// ── FlangerDSP ────────────────────────────────────────────────────────────────
// Stereo flanger with sine LFO, feedback (positive or negative for through-zero
// flanging), optional BPM sync, and wet/dry mix.
// ─────────────────────────────────────────────────────────────────────────────
class FlangerDSP : public DSPBase
{
public:
    FlangerDSP()  = default;
    ~FlangerDSP() override = default;

    // DSPBase interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;
    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;
    void setHostBPM          (double bpm)               override;

    // Parameter setters
    void setRate           (float hz);        // LFO rate, 0.05-5 Hz
    void setDepth          (float ms);        // max delay sweep depth, 0-10 ms
    void setDelay          (float ms);        // base (min) delay, 0-20 ms
    void setFeedback       (float fb);        // -1 to 1
    void setFeed           (float pct);       // alias for setFeedback, 0-100 pct mapped -1..1
    void setWet            (float wet);       // 0-1
    void setSyncBPM        (bool sync);       // lock LFO to BPM (1/8 note by default)
    void setSyncDivision   (int divIdx);      // C4: 0..7 index into kSyncDivisions table
    void setPhase          (float degrees);   // stereo R-channel LFO offset, 0-360
    void setDampHz         (float hz);        // C3: feedback-damp LPF cutoff, 200..20000 Hz
    void setShape          (float s);         // 0=sine, 1=triangle (morph)
    void setInvertFeedback (bool b);          // negate feedback signal
    void setInvertWet      (bool b);          // negate wet signal
    void setDryLevel       (float dB);        // dry output level in dB
    void setWetLevel       (float dB);        // wet output level in dB
    void setCrossLevel     (float dB);        // cross-channel wet mix (L->R, R->L) in dB

    float mRate           { 0.5f };
    float mDepth          { 5.0f };
    float mDelay          { 0.0f };   // base delay ms
    float mFeedback       { 0.5f };
    float mWet            { 0.5f };
    bool  mSyncBPM        { false };
    int   mSyncDivIdx     { 3 };      // C4: default index 3 = 1/8 (preserves v1 behavior)
    float mStereoPhase    { 0.0f };   // degrees (0-360) of R-channel LFO offset
    float mShape          { 0.0f };   // 0=sine, 1=triangle
    float mDampHz         { 20000.0f }; // C3: feedback-damp LPF cutoff in Hz (20 kHz = off)
    bool  mInvertFeedback { false };
    bool  mInvertWet      { false };
    float mDryLevelDb     { 0.0f };   // dB, 0 = unity
    float mWetLevelDb     { 0.0f };   // dB, 0 = unity
    float mCrossLevelDb   { -96.0f }; // dB, -96 = off

    // C4: sync-division table (num/den of a whole note per LFO cycle).
    // Index 3 = 1/8 (preserves v1 hardcoded-1/8 behavior).
    struct SyncDiv { const char* label; int num; int den; };
    static constexpr int kNumSyncDivisions = 8;
    static const SyncDiv kSyncDivisions[kNumSyncDivisions];

private:
    void allocateBuffers();

    // Max buffer = base delay (up to 20ms) + sweep depth (up to 10ms) + headroom
    static constexpr float kMaxDelayMs = 40.0f;

    std::vector<float> mBufL, mBufR;
    int    mWritePos      { 0 };
    double mPhase         { 0.0 };
    float  mFbL           { 0.0f };
    float  mFbR           { 0.0f };
    double mHostBPM       { 120.0 };
    // 1-pole LP damp filter state
    float  mDampStateL    { 0.0f };
    float  mDampStateR    { 0.0f };

    // Zipper-noise guards (4c + A2/A3/A4): ~20 ms linear ramp to target on each setter
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mRateSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mDepthSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mFeedbackSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mWetSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mDelaySmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mShapeSmooth;

    // C3: damp LPF 1-pole state-feedback coefficient derived from mDampHz.
    // Standard 1-pole LP: y = a*y_prev + (1-a)*x, where a = exp(-2*pi*fc/sr).
    // High cutoff (20 kHz) -> a ~= 0 -> output ~= input (transparent / no damping).
    // Low cutoff (200 Hz)  -> a close to 1 -> heavy LP (strong damping).
    float  mDampAlpha     { 0.0f };
    void   recomputeDampAlpha();

    // 4-point Catmull-Rom cubic read from circular buffer, reading BACK from writePos
    static float cubicReadBack (const std::vector<float>& buf, float dSamp,
                                int writePos, int bufSize);
};
