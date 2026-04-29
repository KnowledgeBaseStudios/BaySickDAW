#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include <vector>

// -- TapeDSP ------------------------------------------------------------------
//
// Algorithm chain (per base-rate sample unless stated):
//   1. Input gain (linear)
//   2. Pre-emphasis  - 1-pole high shelf @ 5 kHz (default +6 dB, user-adjustable)
//   ---------- oversampled region (factor = 2^mOsLog2, default 4x) ----------
//   3. Asymmetric sigmoid shaper:  f(x) = (x + k*x^2*sgn(x)) / (1+|x|)
//         - x first biased by a static DC offset driven by Bias knob
//           (0..10, 5 = neutral = pure asymmetric shape driven by x^2*sgn(x))
//         - k = 0.3 * mVibe (spec default)
//   4. Hysteresis accumulator (tape magnetic memory):
//         y = y_prev + alphaOS * (shaped - y_prev)
//         - alphaOS SR-aware: time-constant model driven by TapeSpeed
//   ---------- back to base rate ----------
//   5. De-emphasis    - 1-pole high shelf @ 4 kHz (default -8 dB, user-adjustable)
//   6. Wow + Flutter delay (cubic interp reads):
//         - Wow: slow sine LFO (0.1..5 Hz)
//         - Flutter: fast sine (10..20 Hz) + smoothed white noise (~300 Hz LP)
//   7. Pink-filtered hiss + HPF @ 200 Hz (independent L/R streams)
//   8. 5 Hz SR-aware DC blocker
//   9. Output gain (linear)
//
// 5F-9 sec.10 quality pass applied:
//   sec.10a hysteresis state variable (magnetic memory)
//   sec.10b asymmetric sigmoid shaper (replaces tanh; PRESET-BREAK pre-v1)
//   sec.10c 4x oversampling around shaper + hysteresis (OS factor user-selectable 2x..16x)
//   sec.10d separate flutter LFO + smoothed noise
//   sec.10e cubic interpolation on wow/flutter delay reads (replaces linear)
//   sec.10f pre/de-emphasis shelf pair replaces old LP (PRESET-BREAK pre-v1)
//   sec.10g pink-filtered hiss + HPF 200 Hz (replaces raw white noise; PRESET-BREAK pre-v1)
//   sec.10h 5 Hz SR-aware DC blocker
//   sec.10i SmoothedValue on all continuous params
//
// Phase A retrospective + C additions:
//   A1   juce::ScopedNoDenormals in process()
//   A2   CPU guards on all setters
//   A3   while-wrap LFO phases (replaces std::fmod)
//   A5   removed dead maxWowSamples statement
//   A6   hiss formula symmetric between L and R (independent RNG streams)
//   C1   Hysteresis Amount knob (multiplier on alpha)
//   C2   Oversampling factor chicken-head (2x/4x/8x/16x)
//   C3   Pre/de-emphasis gain knobs (user-adjustable dB)
//   C4   Tape speed chicken-head (7.5/15/30 ips wraps mTapeSpeed 0..1)
//   C5   Bias control (0..10, 5=neutral; pre-shaper DC offset injection)
// -----------------------------------------------------------------------------
class TapeDSP : public DSPBase
{
public:
    TapeDSP()  = default;
    ~TapeDSP() override = default;

    // DSPBase interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;
    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;
    int  getLatencySamples() const override { return mLatencySamples; }

    // ---- API -------------------------------------------------------------
    void setVibe         (float v);       // 0..1 - shaper asymmetry coefficient driver
    void setTapeSpeed    (float v);       // 0..1 - 0=slow/squishy, 1=fast/clean
    void setHystAmount   (float v);       // 0..2 - scales final alpha (C1)
    void setWowRate      (float hz);      // 0.1..5 Hz
    void setWowDepth     (float d);       // 0..1
    void setFlutterRate  (float hz);      // 1..15 Hz (10d)
    void setFlutterDepth (float d);       // 0..1 (10d)
    void setInputGain    (float linGain); // linear
    void setOutputGain   (float linGain); // linear
    void setHiss         (float h);       // 0..1
    void setBias         (float v);       // 0..10 (C5) - pre-shaper DC offset driver
    void setPreShelfDb   (float dB);      // -12..+12 (C3) - default +6
    void setDeShelfDb    (float dB);      // -12..+12 (C3) - default -8
    void setOsLog2       (int factorLog2);// 1..4 -> 2x..16x (C2) - default 2 (4x)

    // Panel-facing getters (A9 construct-time state sync)
    int   getOsLog2()       const noexcept { return mOsLog2; }
    float getTapeSpeed()    const noexcept { return mTapeSpeed; }

    // ---- Public state ----------------------------------------------------
    float mVibe         { 0.5f };
    float mTapeSpeed    { 0.5f };      // 0..1 (C4 chicken-head drives this in 3 steps)
    float mHystAmount   { 1.0f };      // C1 - 0=no hysteresis, 1=spec default, 2=strong
    float mWowRate      { 0.5f };
    float mWowDepth     { 0.0f };      // default off - users opt into wow as a creative add
    float mFlutterRate  { 5.0f };      // 10d
    float mFlutterDepth { 0.0f };      // 10d - default off, same reasoning as WowDepth
    float mInputGain    { 1.0f };
    float mOutputGain   { 1.0f };
    float mHiss         { 0.0f };      // default off - users opt into hiss as a creative add
    float mBias         { 5.0f };      // C5 - 0..10, 5=neutral (zero DC offset)
    float mPreShelfDb   { 0.0f };      // C3 - default off (flat); user raises for emphasis character
    float mDeShelfDb    { 0.0f };      // C3 - default off (flat); user lowers for de-emphasis character
    int   mOsLog2       { 2 };         // C2 - default 2 = 4x

private:
    static constexpr float kMaxWowMs = 50.0f;

    // Asymmetric sigmoid shaper (10b)
    static float asymShaper (float x, float k) noexcept
    {
        const float sgn = (x >= 0.0f) ? 1.0f : -1.0f;
        return (x + k * x * x * sgn) / (1.0f + std::abs (x));
    }

    // 4-point cubic (Catmull-Rom) interp on a circular buffer (10e)
    static float cubicCircular (const std::vector<float>& buf, int writePos,
                                 int iOff, float frac, int size) noexcept;

    void updateFilters();
    void allocateScratch();
    void snapSmoothedToTargets();

    // Wow + flutter delay buffers
    std::vector<float> mWowBufL, mWowBufR;
    int    mWowWritePos { 0 };
    double mWowPhase    { 0.0 };

    // Flutter LFO state (10d)
    double mFlutterPhase      { 0.0 };
    float  mFlutterNoiseState { 0.0f };

    // Hysteresis state (OS domain) - 10a
    float mHystL { 0.0f }, mHystR { 0.0f };

    // Pre-emphasis shelf state (base rate) - 10f
    float mPreLP_L { 0.0f }, mPreLP_R { 0.0f };

    // De-emphasis shelf state (base rate) - 10f
    float mDeLP_L { 0.0f }, mDeLP_R { 0.0f };

    // Pink-noise filter state - 10g (per channel for natural L/R decorrelation)
    struct PinkState {
        float b0{0}, b1{0}, b2{0}, b3{0}, b4{0}, b5{0}, b6{0};
        float process (float white) noexcept
        {
            b0 = 0.99886f * b0 + white * 0.0555179f;
            b1 = 0.99332f * b1 + white * 0.0750759f;
            b2 = 0.96900f * b2 + white * 0.1538520f;
            b3 = 0.86650f * b3 + white * 0.3104856f;
            b4 = 0.55000f * b4 + white * 0.5329522f;
            b5 = -0.7616f  * b5 - white * 0.0168980f;
            const float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
            b6 = white * 0.115926f;
            return pink * 0.11f;
        }
    };
    PinkState mPinkL, mPinkR;

    // Hiss HPF @ 200 Hz state (per channel) - 10g
    float mHissHPx_L { 0.0f }, mHissHPy_L { 0.0f };
    float mHissHPx_R { 0.0f }, mHissHPy_R { 0.0f };

    // DC blocker state (per channel) - 10h
    float mDcX_L { 0.0f }, mDcY_L { 0.0f };
    float mDcX_R { 0.0f }, mDcY_R { 0.0f };

    // Filter coefficients (recomputed in updateFilters)
    float mPreShelfCoef    { 0.0f };   // base rate, 5 kHz
    float mDeShelfCoef     { 0.0f };   // base rate, 4 kHz
    float mPreShelfGainLin { 2.0f };   // from mPreShelfDb
    float mDeShelfGainLin  { 0.4f };   // from mDeShelfDb
    float mHissHpCoef      { 0.0f };   // base rate, 200 Hz
    float mDcCoef          { 0.9995f };// base rate, 5 Hz (9c-style)
    float mHystAlphaOS     { 0.5f };   // OS-effective alpha (time-constant-based)

    juce::Random mRandomL, mRandomR;   // independent streams for stereo decorrelation

    // Oversampler (10c)
    std::unique_ptr<juce::dsp::Oversampling<float>> mOversampler;
    int mLatencySamples { 0 };

    // Scratch buffers (OS band buffer + per-base-sample smoother scratch)
    juce::AudioBuffer<float> mBandBuf;                    // 2ch x maxBlock
    std::vector<float>       mVibeScr, mHystAlphaOSScr,
                             mBiasScr, mPreGainScr, mDeGainScr,
                             mInGainScr, mOutGainScr,
                             mWowDepthScr, mFlutterDepthScr, mHissScr;

    // Smoothers (10i) - consumed at base rate; held constant across OS inner loop
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mVibeSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mHystAlphaOSSmooth;   // derived
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mBiasSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mPreGainSmooth;       // linear
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mDeGainSmooth;        // linear
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mInGainSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mOutGainSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mWowDepthSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mFlutterDepthSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mHissSmooth;
};
