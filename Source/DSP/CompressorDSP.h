#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include <atomic>

// -- CompressorDSP ------------------------------------------------------------
// Full-featured stereo compressor with soft knee, parallel mix, sidechain
// input, look-ahead, peak/RMS detection, and a lock-free GR meter output.
// -----------------------------------------------------------------------------
class CompressorDSP : public DSPBase
{
public:
    CompressorDSP();
    ~CompressorDSP() override = default;

    // DSPBase interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;

    void getStateInformation (juce::MemoryBlock& dest)           override;
    void setStateInformation (const void* data, int sz)          override;

    // setHostBPM is a no-op for compressor
    void setHostBPM (double /*bpm*/) override {}

    // Returns the most-recent gain reduction in dB (negative = reduced)
    float getGainReductionDb() const override;

    // Latency reported to EffectRack for PDC (= look-ahead in samples).
    int getLatencySamples() const override { return mLASamples; }

    // Parameter setters (all CPU-guarded: no-op when unchanged)
    void setThreshold (float dB);        // default -12 dB
    void setRatio     (float ratio);     // 0.4..30; <1 = upward expansion
    void setAttack    (float ms);        // 0..400 ms
    void setRelease   (float ms);        // 1..4000 ms
    void setMakeup    (float dB);        // makeup gain (legacy, clamped -30..+30)
    void setGain      (float dB);        // same as setMakeup, range -30..+30 dB
    void setKnee      (float dB);        // manual soft-knee width override
    void setKneeType  (int type);        // 0=Hard,1=Med,2=Vintage,3=Soft,4-7=/R variants
    void setMix       (float mix);       // 0=dry .. 1=wet, default 1

    // 2a -- Look-ahead delay in ms (0..5). 0 = off, no latency. Changes
    // getLatencySamples(); host must be told (EffectRack calls updateBusLatencies).
    void setLookaheadMs (float ms);
    // 2b -- When true, detector uses max(|L|,|R|) and a single shared envelope
    // drives both channels. When false, each channel is independently compressed.
    void setStereoLink  (bool on);
    // 2c -- When true, makeup is computed each block so output level tracks
    // threshold automatically. Manual Gain ignored.
    void setAutoMakeup  (bool on);

    // C1 -- Enable/disable sidechain detection path. When true and a sidechain
    // buffer is bound via setSidechainBuffer(), detection reads from the SC
    // source instead of the main input. Flag is serialized.
    void setUseSidechain (bool on);

    // C.4 Phase 1 (2026-04-30): SlotComponent header SC dropdown gate -- only
    // shown for effects that actually consume the SC signal.
    bool usesSidechain() const noexcept override { return true; }
    // C2 -- Sidechain high-pass filter cutoff (20..2000 Hz). Applied to the
    // detection source only (internal or external). 20 Hz = effectively off.
    void setSidechainHPF (float hz);
    // C3 -- Detection mode: false = RMS (~Det window, smoothed), true = Peak
    // (|x| with fast attack/slow decay). PRESET-SAFE: default false = current behavior.
    void setPeakDetection (bool on);
    // C4 -- Detection window / smoother time constant in ms (1..100). Applies to
    // RMS mode. Smaller = faster tracking; larger = smoother/musical.
    void setDetectionMs (float ms);

    // Scaffolding for future external-sidechain routing. Stores which mixer
    // channel id feeds the sidechain source (-1 = internal / no external).
    // Serialized so v1 presets future-proof against Tier-3 routing-UI work.
    // DSP reads are no-op today; VibeGraph routing does the actual wiring later.
    void setSidechainSourceId (int channelId);

    // Sidechain: pass a pointer to a buffer whose samples are used for level
    // detection.  The buffer is NOT owned by this object.
    void setSidechainBuffer (juce::AudioBuffer<float>* buf);

    // Public parameter values (read-only externally; use setters to change)
    float threshold  { -12.0f };
    float ratio      {   4.0f };
    float attackMs   {  10.0f };
    float releaseMs  { 100.0f };
    float makeupDb   {   0.0f };
    float kneeDb     {   6.0f };   // set by setKneeType; default Medium=6dB
    float mix        {   1.0f };
    bool  useSidechain { false };
    int   mKneeType  {   1    };   // 0=Hard,1=Med,2=Vintage,3=Soft + /R variants 4-7
    float lookaheadMs{   0.0f };
    bool  stereoLink {   true };
    bool  autoMakeup {  false };
    float sidechainHPF { 20.0f };   // C2: 20 Hz default = effectively off
    bool  peakDetection{ false };   // C3: false = RMS (default), true = peak
    float detectionMs  { 10.0f };   // C4: RMS window in ms
    int   sidechainSourceId { -1 };  // scaffolding: -1 = internal detection path

private:
    // Recalculates attack/release/RMS/TCR coefficients from mSampleRate + stored ms values
    void calcCoefs();
    // Rebuilds sidechain HPF biquad coefficients from sidechainHPF + mSampleRate
    void updateScHpfCoefs();

    // Applies gain-computer curve (with soft/vintage knee) to a level in dB.
    // Returns the gain *change* in dB (typically <= 0).
    float computeGainDb (float levelDb) const noexcept;

    // Per-sample envelopes. When stereoLink=true only mEnvL is used
    // (mEnvR is kept in sync for meter reads); when false, both run independently.
    float mEnvL        { 0.0f };
    float mEnvR        { 0.0f };
    float mAttackCoef  { 0.0f };
    float mReleaseCoef { 0.0f };

    // TCR (/R) mode: slow-envelope of signal level used to accelerate release
    float mTCRLevel    { 0.0f };
    float mTcrAttCoef  { 0.0f };   // cached per sample-rate (was recomputed per block)
    float mTcrRelCoef  { 0.0f };

    // 2d -- Per-sample running mean-square for level detection
    float mRunningRmsL { 0.0f };
    float mRunningRmsR { 0.0f };
    float mRmsCoef     { 0.0f };

    // C3 -- Peak detector state (when peakDetection==true)
    float mPeakL       { 0.0f };
    float mPeakR       { 0.0f };
    float mPeakAttCoef { 0.0f };   // very fast (~0.1 ms)
    float mPeakRelCoef { 0.0f };   // slower (derived from detectionMs)

    // C2 -- Sidechain HPF biquad state (per channel). Simple TPT highpass.
    juce::dsp::StateVariableTPTFilter<float> mScHpfL, mScHpfR;

    // 2a -- Look-ahead delay lines (main audio path only; detector is un-delayed)
    std::vector<float> mLookaheadL, mLookaheadR;
    int                mLAWritePos  { 0 };
    int                mLASamples   { 0 };   // current delay in samples
    static constexpr float kMaxLookaheadMs = 5.0f;

    std::atomic<float> mGainReductionDb { 0.0f };
    // A4 -- Peak-hold decay (dB/block) so transient GR isn't missed between UI polls.
    float mGrDecayDbPerBlock { 0.35f };

    // A2 -- Per-sample smoothed continuous parameters (zipper suppression).
    juce::LinearSmoothedValue<float> mThresholdSmoothed;
    juce::LinearSmoothedValue<float> mRatioSmoothed;
    juce::LinearSmoothedValue<float> mMixSmoothed;
    juce::LinearSmoothedValue<float> mMakeupDbSmoothed;

    juce::AudioBuffer<float>* mSidechainBuffer { nullptr };
};
