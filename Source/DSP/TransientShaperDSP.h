#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"

// -- TransientShaperDSP -------------------------------------------------------
//
// Signal chain (per base-rate sample):
//   1. Dual-envelope detection (per channel if stereoDetect, else mono-sum)
//      - Fast envelope: peak follower (AttackShape sets fast-env ATTACK time)
//      - Slow envelope: RMS follower (ReleaseShape sets slow-env RELEASE time)
//      - FastRel and SlowAtt are user-settable (C4) - previously hardcoded
//   2. Transient signal = fast - slowRms (difference, not ratio; keeps current
//      tuning feel while switching slow detector to RMS per spec 11e)
//   3. Quadratic gain computation (11a): 1 + attack*t^2 + sustain*s^2
//   4. Linkwitz-Riley 4th-order crossover (11b): per-channel LP + HP split;
//      phase-perfect recombination
//   5. Apply gain to high band, balance-crossfade between dry and shaped high
//   6. Recombine low + high
//   ---------- oversampled region (factor = 2^mOsLog2, default 4x) (11c) ------
//   7. Soft-clip drive: tanh((1+drive)*x) / (1+drive). Always runs through the
//      oversampler at constant latency regardless of drive value.
//   ---------- back to base rate ----------
//   8. Dry/Wet mix (C3) - blend original dry against shaped signal
//   9. Output gain (linear; smoothed directly to avoid per-sample dbToGain)
//
// 5F-9 sec.11 quality pass applied:
//   sec.11a quadratic attack + sustain curves (PRESET-BREAK pre-v1)
//   sec.11b Linkwitz-Riley 4th-order crossover (PRESET-BREAK pre-v1)
//   sec.11c 4x oversampling around drive stage (constant latency)
//   sec.11d SmoothedValue on all continuous user params
//   sec.11e slow envelope uses RMS detection (PRESET-BREAK pre-v1)
//
// Phase A retrospective + C additions:
//   A1 juce::ScopedNoDenormals in process()
//   A2 CPU guards on all setters
//   A4 setAttack dual-range kludge removed (panel always sends -100..100)
//   A7 mSlowL replaced by mSlowMs (mean-square)
//   C1 Oversampling factor chicken-head (2x/4x/8x/16x, default 4x)
//   C2 Stereo envelope detection toggle (default off = mono-sum)
//   C3 Dry/Wet mix knob (default 1.0 = current 100%-wet behavior)
//   C4 FastRel + SlowAtt exposed as knobs (previously hardcoded 10 ms each)
// -----------------------------------------------------------------------------
class TransientShaperDSP : public DSPBase
{
public:
    TransientShaperDSP();
    ~TransientShaperDSP() override = default;

    // DSPBase interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;
    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;
    int  getLatencySamples() const override { return mLatencySamples.load (std::memory_order_relaxed); }

    // C.4: TransientShaperDSP consumes external sidechain for detector path
    bool usesSidechain() const noexcept override { return true; }

    // ---- API ----------------------------------------------------------------
    void setAttack       (float a);         // -100..100 (panel range; stored as -1..1)
    void setSustain      (float s);         // -1..1 (legacy path)
    void setSensitivity  (float s);         // 0..1
    void setRelease      (float rel);       // -100..100; stored as -1..1 in mRelease

    // Attack/release envelope shape presets (affect ONE end of each envelope).
    void setAttackShape  (int shape);       // 0=Sharp, 1=Medium, 2=Soft (fast-env ATTACK time)
    void setReleaseShape (int shape);       // 0=Sharp, 1=Medium, 2=Soft (slow-env RELEASE time)

    // Band-split (11b uses LR4 internally)
    void setSplitFreq    (float hz);        // 5-1000 Hz crossover
    void setSplitBalance (float bal);       // -100=all-low, 0=full-split, +100=all-high

    // Post-processing
    void setDrive        (float drive);     // 0..10 (tanh saturation; oversampled)
    void setGain         (float dB);        // output gain trim

    // C1-C4 new setters
    void setOsLog2       (int factorLog2);  // 1..4 -> 2x..16x (default 2 = 4x)
    void setStereoDetect (bool on);         // C2: true = per-channel env, false = mono sum
    void setWet          (float wet);       // C3: 0..1 dry/wet mix, default 1.0
    void setFastRelMs    (float ms);        // C4: fast-env release, 1..50 ms (was hardcoded 10)
    void setSlowAttMs    (float ms);        // C4: slow-env attack, 1..50 ms (was hardcoded 10)

    // Panel-facing getters (A9 construct-time state sync)
    int  getOsLog2()        const noexcept { return mOsLog2; }
    bool getStereoDetect()  const noexcept { return mStereoDetect; }

    // QA-Layout T18: the before/after envelope is drawn from mAttack / mRelease
    // and the two shape selectors below, so it tracks the knobs directly.
    bool hasVisualFeed()    const override { return true; }

    // ---- Public state ----------------------------------------------------
    float mAttack       { 0.0f };      // stored -1..1 (0 = neutral, no attack boost/cut)
    float mRelease      { 0.0f };      // stored -1..1
    float mSensitivity  { 0.5f };
    int   mAttackShape  { 1 };         // 0=Sharp,1=Med,2=Soft
    int   mReleaseShape { 1 };
    float mSplitFreq    { 260.0f };    // default 260 Hz per spec
    float mSplitBalance { 0.0f };      // -100..100
    float mDrive        { 0.0f };      // 0..10
    float mOutGainDb    { 0.0f };      // dB

    // C1-C4
    int   mOsLog2       { 2 };         // 4x default
    bool  mStereoDetect { false };     // default off = current mono-sum behavior
    float mWet          { 1.0f };      // default 1.0 = current 100%-wet behavior
    float mFastRelMs    { 10.0f };     // default 10 ms (matches old hardcoded)
    float mSlowAttMs    { 10.0f };     // default 10 ms (matches old hardcoded)

private:
    void recalcEnvelopeCoefs();

    // Envelope coefficients (recomputed on shape/time-constant change)
    float mFastAttCoef { 0.0f };
    float mFastRelCoef { 0.0f };
    float mSlowAttCoef { 0.0f };
    float mSlowRelCoef { 0.0f };

    // Envelope state
    // Fast envelope (peak follower) - mSlowMs (mean-square accumulator) replaces mSlowL per 11e.
    float mFastL { 0.0f }, mFastR { 0.0f };     // peak; R used only when mStereoDetect
    float mSlowMs  { 0.0f }, mSlowMsR { 0.0f }; // mean-square; R used only when mStereoDetect

    // Linkwitz-Riley 4th-order crossover (11b) replaces manual 1-pole LP.
    juce::dsp::LinkwitzRileyFilter<float> mLR_LP;
    juce::dsp::LinkwitzRileyFilter<float> mLR_HP;

    // Oversampler around drive stage (11c)
    std::unique_ptr<juce::dsp::Oversampling<float>> mOversampler;
    std::atomic<int> mLatencySamples { 0 };   // audio writes on swap, message thread reads for PDC

    // Oversampler hot-swap (mirrors NAMPedalStyleDSP's active/pending pair).
    // prepare() must NEVER run on a live DSP -- it frees the oversampler the
    // audio thread is inside.  Message-thread entry points (the OS chicken head,
    // preset loads) build the replacement here and publish it with a release-
    // store; the audio thread adopts it at the top of process().  The retired
    // oversampler is parked back in mOsPending and destructs on the NEXT
    // message-thread stage call, never on audio.
    std::unique_ptr<juce::dsp::Oversampling<float>> mOsPending;
    std::atomic<bool> mOsSwapPending { false };
    int mOsPendLatency { 0 };

    void stageOversampler (int factorLog2);    // message thread: build + publish
    void drainOversamplerSwap() noexcept;      // audio thread: top of process()

    // Scratch buffers
    juce::AudioBuffer<float> mBandBuf;  // 2ch x maxBlock - holds shaped signal between phases

    // Per-base-sample scratch for values needed in multiple phases
    std::vector<float> mWetScr, mOutGainScr;

    // Smoothers (11d + C3 Wet)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mAttackSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mReleaseSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mSensitivitySmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mSplitFreqSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mBalanceSmooth;   // -100..100 -> 0..1 highWeight
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mDriveSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mOutGainSmooth;   // linear
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mWetSmooth;

    void snapSmoothedToTargets();
};
