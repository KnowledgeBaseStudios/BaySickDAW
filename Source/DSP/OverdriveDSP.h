#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include <memory>

// ── OverdriveDSP — 5F-9 §6 DSP quality pass ──────────────────────────────────
//
// Algorithm chain (stereo, per block):
//   1. Pre BPF (TPT SVF) — 2nd-order bandpass at Color Hz, Q from PreBand.
//      Only the filtered band is distorted; residual stays clean.
//   2. Waveshaper — atan(PreAmp · (x100?100:1) · band) / (π/2)
//                   Processed at 4× via juce::dsp::Oversampling (IIR polyphase,
//                   low-latency). Reduces aliasing on x100 especially.
//   3. Recombine  — shaped_band + clean_residual
//   4. Post LPF (TPT SVF) — 2nd-order Butterworth at PostFilter Hz
//   5. PostGain   — dB trim
//   6. Hard clip  — ±1.0 protection for ×100 blow-up
//   7. 5 Hz DC blocker — strips DC injected by asymmetric shaping at high drive
//   8. Legacy wet/dry mix
//
// Parameters PreAmp / Color / PostFilter / PostGain are smoothed (15-30 ms) to
// kill zipper noise on automation. Coefs refreshed once per process() block.
// Total latency = oversampler group delay (reported via getLatencySamples()).
// ─────────────────────────────────────────────────────────────────────────────
class OverdriveDSP : public DSPBase
{
public:
    OverdriveDSP();
    ~OverdriveDSP() override = default;

    // DSPBase interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;
    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;
    int  getLatencySamples() const override { return mLatencySamples; }

    // ── New API ──────────────────────────────────────────────────────────────
    void setPreBand    (float v);    // 0 = narrow (high Q), 1 = wide (low Q)   (smoothed, A3)
    void setColor      (float hz);   // BPF center frequency (smoothed)
    void setPreAmp     (float v);    // 0-10 drive amount (smoothed)
    void setX100       (bool on);    // x100 gain mode (extreme drive; smoothed scalar 1<->100 via A5)
    void setPostFilter (float hz);   // output LPF cutoff (smoothed)
    void setPostGain   (float dB);   // output gain trim -18..+18 dB (smoothed)
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
    juce::dsp::StateVariableTPTFilter<float> mBPF;
    juce::dsp::StateVariableTPTFilter<float> mLPF;

    // ── 4× oversampler around the shaper (stereo) ────────────────────────────
    std::unique_ptr<juce::dsp::Oversampling<float>> mOversampler;
    juce::AudioBuffer<float> mBandBuf;     // stereo, mMaxBlock — band pre-shaper
    juce::AudioBuffer<float> mResidualBuf; // stereo, mMaxBlock — x - band
    int  mLatencySamples { 0 };

    // ── 5 Hz DC-blocker state (post-clip, pre-wet/dry) ───────────────────────
    float mDcX_L { 0.0f }, mDcY_L { 0.0f };
    float mDcX_R { 0.0f }, mDcY_R { 0.0f };
    float mDcCoef{ 0.0f };   // R = 1 - 2π·5/sr, cached from prepare()

    // ── Helpers ──────────────────────────────────────────────────────────────
    void refreshFilterCoefs(int numSamples);   // called at top of each process()
    void snapSmoothedToTargets();
};
