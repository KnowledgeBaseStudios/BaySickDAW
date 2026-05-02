#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include <atomic>
#include <memory>
#include <vector>

// ── LimiterDSP ────────────────────────────────────────────────────────────────
// Look-ahead peak limiter with soft-sat pre-stage and 4× oversampled True Peak
// detection. Fruity-Limiter-style architecture:
//
//   input → InputGain → tanh SoftSat(SatThresh,SatCurve) → DelayLine(0–10 ms)
//                                                                ↓
//                    (pre-delay tap) → 4× TP detector (JUCE Oversampling, IIR)
//                                                                ↓
//                                            stereo-linked peak envelope
//                                     (attack/release, 2-stage auto-release,
//                                      linear↔exponential release curve)
//                                                                ↓
//                                      GainComputer: min(1, ceiling/peak)
//                                                                ↓
//                                            applied to delayed audio
//                                                                ↓
//                                                hard ceiling clamp (safety)
//                                                                ↓
//                                                            output
//
// Reports latency via getLatencySamples() = aheadSamples + oversampler latency.
// EffectRack accumulates this for PDC.
// ─────────────────────────────────────────────────────────────────────────────
class LimiterDSP : public DSPBase
{
public:
    LimiterDSP();
    ~LimiterDSP() override = default;

    // DSPBase interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;
    void getStateInformation (juce::MemoryBlock& dest) override;
    void setStateInformation (const void* data, int sz) override;
    float getGainReductionDb() const override { return mGrDb.load(); }
    int   getLatencySamples() const override  { return mLatencySamples; }

    // ── Setters ───────────────────────────────────────────────────────────────
    void setInputGainDb   (float dB);     // -12..+24 dB     (smoothed, 15 ms)
    void setCeilingDb     (float dB);     // -24..0 dB       (smoothed, 15 ms)
    void setSatThresh     (float lin);    // 0..1 linear     (smoothed, 15 ms; 1.0 = off)
    void setSatCurve      (float v01);    // 0..1 knee shape (smoothed, 15 ms)
    void setAttackMs      (float ms);     // 0.1..20 ms
    void setReleaseMs     (float ms);     // 10..1000 ms
    void setAheadMs       (float ms);     // 0..10 ms        (delay line size)
    void setReleaseCurve  (float v01);    // 0..1 (0=linear, 1=exp)
    void setAutoRelease   (bool on);
    // C2: Sidechain HPF cutoff on detector path (20..2000 Hz; 20 = effectively off).
    void setSidechainHPF  (float hz);
    // C.4 Phase 2.1: LimiterDSP consumes external SC for the detector path.
    bool usesSidechain() const noexcept override { return true; }
    // C4: Auto-makeup gain. Boosts output by -ceilingDb so lowering the ceiling
    // doesn't quiet the signal (loudness-maximizer workflow).
    void setAutoMakeup    (bool on);
    // C5: Stereo-link detector. true = single envelope from max(|L|,|R|) drives
    // both channels (default, cleanest stereo image). false = per-channel envelopes.
    void setStereoLink    (bool on);

    // ── Meter accessors (UI thread reads, audio thread writes) ────────────────
    float getCurrentGainReduction() const { return mGrDb.load(); }
    float getInputLevelDb()         const { return mInputDb.load(); }
    float getOutputLevelDb()        const { return mOutputDb.load(); }

    // Public read of target param values (for UI knobs / automation mirroring)
    float getInputGainDb()  const { return mInputGainTargetDb; }
    float getCeilingDb()    const { return mCeilingTargetDb; }
    float getSatThresh()    const { return mSatThreshTarget; }
    float getSatCurve()     const { return mSatCurve; }
    float getAttackMs()     const { return mAttackMs; }
    float getReleaseMs()    const { return mReleaseMs; }
    float getAheadMs()      const { return mAheadMs; }
    float getReleaseCurve() const { return mReleaseCurve; }
    bool  getAutoRelease()  const { return mAutoRelease; }
    float getSidechainHPF() const { return mSidechainHPF; }
    bool  getAutoMakeup()   const { return mAutoMakeup; }
    bool  getStereoLink()   const { return mStereoLink; }

private:
    // ── Param targets (also the authoritative values in serialization) ────────
    float mInputGainTargetDb { 0.0f };
    float mCeilingTargetDb   { -0.3f };
    float mSatThreshTarget   { 1.0f };
    float mSatCurve          { 0.5f };
    float mAttackMs          { 1.0f };
    float mReleaseMs         { 100.0f };
    float mAheadMs           { 2.0f };
    float mReleaseCurve      { 0.5f };
    bool  mAutoRelease       { false };
    float mSidechainHPF      { 20.0f };   // C2: 20 Hz = effectively off
    bool  mAutoMakeup        { false };   // C4
    bool  mStereoLink        { true };    // C5

    // ── Smoothed values (15 ms linear ramp) ───────────────────────────────────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mInputGainSmooth;   // dB
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mCeilingSmooth;     // dB
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mSatThreshSmooth;   // linear
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mSatCurveSmooth;    // 0..1 (C1)

    // C2: Sidechain HPF (TPT SVF, stereo). Applied to the pre-delay detection
    // signal only; main audio path is untouched.
    juce::dsp::StateVariableTPTFilter<float> mScHpfL, mScHpfR;
    void updateScHpfCoefs();

    // ── Envelope state ────────────────────────────────────────────────────────
    // When mStereoLink == true, only the L variants are used (single envelope
    // drives both channels). When false, each channel runs its own envelope.
    float mEnv         { 0.0f };   // user-release envelope (L / linked)
    float mEnvFast     { 0.0f };   // auto-release fast envelope (~20 ms)
    float mEnvSlow     { 0.0f };   // auto-release slow envelope (~300 ms)
    float mEnvR        { 0.0f };   // C5: R-channel user-release envelope
    float mEnvFastR    { 0.0f };   // C5: R-channel fast envelope
    float mEnvSlowR    { 0.0f };   // C5: R-channel slow envelope
    float mAttackCoef  { 0.0f };
    float mReleaseCoef { 0.0f };
    float mRelFastCoef { 0.0f };
    float mRelSlowCoef { 0.0f };
    float mRelStepPerSample { 0.0f };   // linear-release step size (linear in dB/sample)
    float mRelStepFast      { 0.0f };
    float mRelStepSlow      { 0.0f };

    // ── Look-ahead circular delay (integer-sample only) ───────────────────────
    std::vector<float> mDelayL, mDelayR;
    int   mDelaySize    { 0 };   // buffer capacity (samples)
    int   mWritePos     { 0 };
    int   mAheadSamples { 0 };

    // ── 4× oversampled TP detector (stereo) ───────────────────────────────────
    std::unique_ptr<juce::dsp::Oversampling<float>> mOversampler;
    juce::AudioBuffer<float> mScBuf;       // pre-allocated detector sidechain buffer (stereo, mMaxBlock samples)
    std::vector<float>       mTpPeaks;     // per-sample TP estimate (sized to mMaxBlock) -- linked (max L/R)
    // C5: unlinked mode writes per-channel TP peaks here. Only populated when
    // mStereoLink == false; otherwise left untouched.
    std::vector<float>       mTpPeaksL, mTpPeaksR;
    int   mOsLatencySamples { 0 };
    int   mLatencySamples   { 0 };         // aheadSamples + osLatency

    // ── Meters (atomic; updated once per block, with hold+decay) ──────────────
    // A2/A3 -- hold+decay so transient in/out dBFS and GR are caught between
    // 30 Hz UI polls (audio runs at 86+ blocks/sec so overwrite-only stores
    // miss ~2-in-3 peaks). max(thisBlock, prev +/- decayPerBlock).
    std::atomic<float> mInputDb  { -96.0f };
    std::atomic<float> mOutputDb { -96.0f };
    std::atomic<float> mGrDb     { 0.0f };
    float mLevelDecayDbPerBlock { 0.35f };   // for mInputDb / mOutputDb (falls toward -96)
    float mGrDecayDbPerBlock    { 0.35f };   // for mGrDb (rises toward 0 — GR is <=0)

    // ── Helpers ───────────────────────────────────────────────────────────────
    void recalcCoefs();       // attack/release coefs from times
    void allocateDelay();     // size delay line from mAheadMs
    static float softSat (float x, float drive, float curve);
};
