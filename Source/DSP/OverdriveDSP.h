#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include <atomic>
#include <memory>

// ── OverdriveDSP - 5F-9 §6 DSP quality pass ──────────────────────────────────
//
// Algorithm chain (stereo, per block) -- QA-EffectsReview Task 5: reworked to the
// reference's in-series topology (the whole signal is driven; no parallel clean band).
//   1. Pre LPF (TPT SVF) - 2nd-order low-pass at Color Hz.  PreBand is the AMOUNT of
//      low-pass blended into the drive input (0 = full bandwidth, 1 = fully low-passed)
//      -- i.e. "amount of low-pass filtering applied before the overdrive".
//   2. Waveshaper - x / (1 + |drive*driveIn + bias|) soft-clip, drive = PreAmp*(x100?100:1).
//                   Processed at 2/4/8/16x via juce::dsp::Oversampling (IIR polyphase,
//                   low-latency). Reduces aliasing on x100 especially.
//   3. Post LPF (TPT SVF) - 2nd-order Butterworth at PostFilter Hz (whole signal).
//   4. PostGain   - attenuate-only trim (-18..0 dB; the reference's "gain reducer").
//   5. Hard clip  - +/-1.0 protection for x100 blow-up.
//   6. 5 Hz DC blocker - strips DC injected by asymmetric shaping at high drive.
//   7. Wet/dry mix (Blend, default) or parallel-add.
//
// Parameters PreAmp / Color / PostFilter / PostGain are smoothed (15-30 ms) to
// kill zipper noise on automation. Coefs refreshed once per process() block.
// Total latency = oversampler group delay (reported via getLatencySamples()).
// ─────────────────────────────────────────────────────────────────────────────
class OverdriveDSP : public DSPBase
{
public:
    // I-4 (2026-05-02): Type umbrella covers two algorithm characters.
    //   Rack  -- the in-series chain documented at the top of this file
    //            (pre LPF + oversampled soft-clip + post LPF).
    //            Default; preserves all old projects bit-exact.
    //   Pedal -- OD Style: 80 Hz split (sub-80 stays clean), 500 Hz pre-clip
    //            notch, oversampled dual cascaded tanh soft-clip, then tone
    //            and level.  Its own branch in process(); the Rack chain is
    //            untouched.
    enum class Type : int
    {
        Rack  = 0,
        Pedal = 1
    };

    OverdriveDSP();
    ~OverdriveDSP() override = default;

    // Type setter (no-op when unchanged).  Mode dropdown calls into this
    // via SlotComponent.  Kept beside the rest of the public param API.
    void setType (int t)
    {
        const Type newType = static_cast<Type> (juce::jlimit (0, 1, t));
        if (newType != mType) mType = newType;
    }
    Type mType { Type::Rack };

    // DSPBase interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;
    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;
    int  getLatencySamples() const override { return mLatencySamples.load (std::memory_order_relaxed); }

    // ── New API ──────────────────────────────────────────────────────────────
    void setPreBand    (float v);    // 0 = no pre-LP (full bandwidth), 1 = full LP at Color (smoothed, A3)
    void setColor      (float hz);   // BPF center frequency (smoothed)
    void setPreAmp     (float v);    // 0-10 drive amount (smoothed)
    void setX100       (bool on);    // x100 gain mode (extreme drive; smoothed scalar 1<->100 via A5)
    void setPostFilter (float hz);   // output LPF cutoff (smoothed)
    void setPostGain   (float dB);   // output trim -18..0 dB, attenuate-only (smoothed)
    void setWet        (float v);    // 0-1 wet/dry mix (smoothed, A4)
    // C2: pre-shaper DC offset injects even harmonics. Post-shaper the same
    // bias is subtracted back out (approximately -- tanh(drive*bias) is the
    // static DC floor removed) so the 5 Hz DC blocker only has LFO drift to
    // catch. Range -1..+1, default 0 (symmetric = current behavior).
    void setBias       (float v);
    // C4: Parallel-add vs blend. false = blend (default, `dry*in + wet*out`).
    // true  = parallel add (`in + wet*out`, dry stays full).
    void setParallel   (bool on);
    // C5: Oversampling factor 2/4/8/16x. Reallocates mOversampler; reports
    // new latency. 4x default matches current behavior.
    void setOversamplingFactor (int factorLog2);   // 1=2x, 2=4x, 3=8x, 4=16x

    // ── Legacy compatibility (old OverdrivePanel still calls these) ──────────
    void setDrive (float drive);   // 0-1 -> preAmp * 10
    void setTone  (float tone);    // 0-1 -> postFilter 500..8000 Hz

    // Getters for panel state-sync
    float getBias()              const { return mBias; }
    bool  getParallel()          const { return mParallel; }
    int   getOversamplingLog2()  const { return mOsLog2; }

    // ── Public state (target values; SmoothedValues hold the transient) ──────
    float mPreBand    { 0.5f };
    float mColor      { 1000.0f };
    float mPreAmp     { 5.0f };
    bool  mX100       { false };
    float mPostFilter { 8000.0f };
    float mPostGain   { 0.0f };
    float mWet        { 1.0f };
    float mBias       { 0.0f };    // C2
    bool  mParallel   { false };   // C4
    int   mOsLog2     { 2 };       // C5  (2 -> 4x, matches v1 default)

private:
    // ── SmoothedValues (zipper-noise guards) ─────────────────────────────────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mPreAmpSmooth;     // 0..10
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mColorSmooth;      // Hz
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mPostFilterSmooth; // Hz
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mPostGainSmooth;   // dB
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mPreBandSmooth;    // 0..1  (A3)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mWetSmooth;        // 0..1  (A4)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mX100ScalarSmooth; // 1..100 (A5)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mBiasSmooth;       // -1..+1 (C2)

    // ── TPT SVF filters (stereo) ─────────────────────────────────────────────
    juce::dsp::StateVariableTPTFilter<float> mPreLpf;   // QA-EffectsReview Task 5: pre-LP (was a bandpass)
    juce::dsp::StateVariableTPTFilter<float> mLPF;

    // ── 4× oversampler around the shaper (stereo) ────────────────────────────
    std::unique_ptr<juce::dsp::Oversampling<float>> mOversampler;
    juce::AudioBuffer<float> mBandBuf;      // stereo, mMaxBlock - the in-series drive signal (pre-shaper)
    std::atomic<int> mLatencySamples { 0 };   // audio writes on swap, message thread reads for PDC

    // ── Oversampler hot-swap (mirrors NAMPedalStyleDSP's active/pending pair) ─
    // prepare() must NEVER run on a live DSP -- it frees the oversampler the
    // audio thread is inside.  Message-thread entry points (the OS chicken head,
    // preset loads) BUILD the replacement here instead and publish it with a
    // release-store; the audio thread adopts the whole group in one swap at the
    // top of process().  The retired oversampler is parked back in mOsPending
    // and destructs on the NEXT message-thread stage call, never on audio.
    std::unique_ptr<juce::dsp::Oversampling<float>> mOsPending;
    std::atomic<bool> mOsSwapPending  { false };
    int   mOsPendLog2      { 2 };
    int   mOsPendLatency   { 0 };
    float mOsPendPedalHpfR { 0.0f };
    // Factor the LIVE oversampler was built with.  mOsLog2 above is the user's
    // intent (persisted + read back by the panel); this is what process()
    // indexes the upsampled block with, so it swaps WITH the oversampler and
    // never ahead of it.
    int   mOsLog2Active    { 2 };

    // ── 5 Hz DC-blocker state (post-clip, pre-wet/dry) ───────────────────────
    float mDcX_L { 0.0f }, mDcY_L { 0.0f };
    float mDcX_R { 0.0f }, mDcY_R { 0.0f };
    float mDcCoef{ 0.0f };   // R = 1 - 2π·5/sr, cached from prepare()

    // I-5 (2026-05-02): OD Style Pedal mode -- 80 Hz frequency split (HPF
    // feeds the dual cascaded soft-clip; LPF feeds the clean blend) +
    // separate scratch for the clean sub-bass path.  Filters allocated in
    // prepare(); only walked in process() when mType == Type::Pedal so they
    // cost nothing in Rack mode.
    juce::dsp::StateVariableTPTFilter<float> mPedalSplitHpf;
    juce::dsp::StateVariableTPTFilter<float> mPedalSplitLpf;
    juce::AudioBuffer<float>                 mPedalCleanBuf;

    // QA-EffectsReview Task 5: OD-pedal fidelity -- a 500 Hz pre-clip mid-notch
    // (the pedal's scooped mids) + a 720 Hz 1-pole HPF between the two clip stages
    // (tightens the low end so it stays punchy under chords), plus a higher drive
    // ceiling (set in process()).  Per-channel so the per-sample inter-stage HPF
    // state is valid for L and R independently.
    juce::dsp::IIR::Filter<float> mPedalNotchL, mPedalNotchR;   // 500 Hz, -4.5 dB peak
    float mPedalHpfR { 0.0f };                                   // 720 Hz 1-pole HPF coef (oversampled rate)
    float mPedalHpfX[2] { 0.0f, 0.0f };
    float mPedalHpfY[2] { 0.0f, 0.0f };

    // ── Helpers ──────────────────────────────────────────────────────────────
    void snapSmoothedToTargets();
    void stageOversampler (int factorLog2);    // message thread: build + publish
    void drainOversamplerSwap() noexcept;      // audio thread: top of process()
};
