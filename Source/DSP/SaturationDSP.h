#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"

// -- SaturationDSP ------------------------------------------------------------
//
// H-7 (2026-05-01): SaturationType umbrella adds character modes:
//   Tube (default)   -- existing algorithm bit-exact untouched.
//   Console          -- preamp/console-style soft clip + asymmetric transformer
//                       saturation.  Different harmonic signature from Tube;
//                       more transparent on low-mids, gentler harmonic ramp.
// Vocal Body shaping toggle (separate from Type) -- applies a -2 dB peaking
// EQ at 250 Hz + +2 dB high-shelf at 3 kHz post-saturation for vocal presence
// shaping.  Default off.  Existing presets default to Tube + body off,
// preserving identical audio on load.
//
// Algorithm chain (per base-rate sample unless stated):
//   1. Sensitivity  - input gain (-12..+12 dB)
//   2. Tone Pre     - 1-pole high shelf at 10 kHz (-9..+9 dB)
//   ---------- oversampled region (factor = 2^mOsLog2, default 4x) ----------
//   3. Bass split   - 1-pole LP at 350 Hz (OS-rate coef)
//   4. Tube engine  - Flowers (tanh even harmonics) + Dabs (A/B/C odd/even)
//                     per-band (low + high) + optional Transformer
//   5. Relief blend - low_out = relief * low + (1-relief) * processTube(low)
//                     high always goes through tube
//   6. Recombine    - tube_out = low_out + high_out
//   ---------- back to base rate ----------
//   7. DC block     - 5 Hz SR-aware 1-pole HP (9c)
//   8. Tone Post    - 1-pole high shelf at 10 kHz (-9..+9 dB)
//   9. Wet/Dry mix (0-100 %)
//  10. Auto-Gain    - outGain *= 1/sensGain when enabled (9b)
//  11. Out gain     - (-18..+18 dB)
//
// 5F-9 sec.9 quality pass applied:
//   sec.9a  4x oversampling around the tube engine (user-selectable 2x/4x/8x/16x via sec.C2)
//   sec.9b  Auto-Gain compensation toggle
//   sec.9c  Sample-rate-aware DC blocker (5 Hz)
//   sec.9d  SmoothedValue on Flowers/Dabs/Sens/Out (plus C1: Wet/BassRelief/TonePre/TonePost)
//
// Phase A retrospective:
//   A1  juce::ScopedNoDenormals in process()
//   A2  CPU guards on all setters (value-change comparison)
//   A7  processTube Type B: std::pow(|t|, 2/3) -> std::cbrt(t*t)
//
// sec.C2  Oversampling factor is user-selectable via setOversamplingFactor (1..4 = 2x..16x)
// sec.C4  Auto-Gain compensation amount reported via getAutoGainCompDb() for panel LED
// -----------------------------------------------------------------------------
class SaturationDSP : public DSPBase
{
public:
    enum class Type : int
    {
        Tube    = 0,   // existing algorithm bit-exact
        Console = 1,   // H-7: preamp/console-style soft clip + asymmetric xfmr
        Tape    = 2    // H-10 (2026-05-02): folded TapeDSP body -- 4x oversampled
                       //    asymmetric sigmoid + hysteresis + wow/flutter/hiss
                       //    chain.  Bit-exact with the legacy TapeDSP class.
    };

    // H-7 (2026-05-01): Keep Low / Normal / Keep High harmonic-routing mode
    // (Saturation Knob style).  When KeepLow, signal is split at 250 Hz LR4;
    // only the high band is saturated.  When KeepHigh, split at 4 kHz; only
    // the low band is saturated.  Normal saturates the full-band signal.
    // Applies to both Tube and Console Types (and Tape once folded under
    // this umbrella in H-10).
    enum class HarmonicsMode : int
    {
        KeepLow  = 0,
        Normal   = 1,   // default -- preserves prior audio
        KeepHigh = 2
    };

    SaturationDSP()  = default;
    ~SaturationDSP() override = default;

    // DSPBase interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;
    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;
    int  getLatencySamples() const override { return mLatencySamples; }

    // ---- New API ----------------------------------------------------------
    void setFlowers     (float v);    // 0-10  (even harmonics via tanh)
    void setDabs        (float v);    // 0-10  (odd / even harmonics per TubeType)
    void setSensitivity (float dB);   // -12..+12 dB input gain
    void setBassRelief  (float v);    // 0=lows fully through tube, 100=lows clean
    void setTransformer (bool on);    // always-on base even-harmonic component
    void setTubeType    (int t);      // 0=A 1=B 2=C
    void setTonePre     (float dB);   // -9..+9 dB high shelf before tube
    void setTonePost    (float dB);   // -9..+9 dB high shelf after tube
    void setWet         (float pct);  // 0-100 %
    void setOut         (float dB);   // -18..+18 dB output trim
    void setAutoGain    (bool on);    // 9b: post-output-gain multiplier = 1/sensGain when on
    void setOversamplingFactor (int factorLog2);   // C2: 1=2x, 2=4x (default), 3=8x, 4=16x

    // H-7 (2026-05-01): SaturationType umbrella + Vocal Body shaping toggle.
    // H-10 (2026-05-02): extended to accept Tape (= 2) -- routes process()
    // through processTape() (separate state, bit-exact with old TapeDSP).
    void setSatType        (int t);   // 0 = Tube (default), 1 = Console, 2 = Tape
    void setVocalBody      (bool on); // post-sat -2dB@250Hz peaking + +2dB@3kHz shelf
    void setHarmonicsMode  (int m);   // 0=KeepLow, 1=Normal (default), 2=KeepHigh

    // ── H-10 (2026-05-02): Tape setters -- mirror TapeDSP API ────────────
    //   These only mutate the Tape sub-state; existing Tube/Console params
    //   stay independent.  Engaged by setSatType (Tape).  Bit-exact with
    //   the legacy TapeDSP setters they replace.
    void setTapeVibe         (float v);     // 0..1
    void setTapeSpeed        (float v);     // 0..1
    void setTapeHystAmount   (float v);     // 0..2
    void setTapeWowRate      (float hz);    // 0.1..5
    void setTapeWowDepth     (float d);     // 0..1
    void setTapeFlutterRate  (float hz);    // 1..15
    void setTapeFlutterDepth (float d);     // 0..1
    void setTapeInputGain    (float linGain);
    void setTapeOutputGain   (float linGain);
    void setTapeHiss         (float h);     // 0..1
    void setTapeBias         (float v);     // 0..10  (5 = neutral)
    void setTapePreShelfDb   (float dB);    // -12..+12
    void setTapeDeShelfDb    (float dB);    // -12..+12

    // ── H-10 Tape getters for panel construct-time state-sync ────────────
    int   getTapeOsLog2()    const noexcept { return mOsLog2; }   // shared with Tube/Console
    float getTapeSpeed()     const noexcept { return mTapeSpeed; }

    // ---- Legacy compatibility (old callers still reach these) -------------
    void setDrive (float drive);   // 0-1 -> flowers * 10
    void setMix   (float mix);     // 0-1 -> wet * 100
    void setType  (int   type);    // 0-2 -> tubeType

    // ---- Getters for panel construct-time state-sync + UI readouts --------
    bool  getAutoGain()        const noexcept { return mAutoGain; }
    int   getOversamplingLog2() const noexcept { return mOsLog2; }
    float getAutoGainCompDb()  const noexcept    // C4: compensation amount to show on panel
    {
        return mAutoGain ? -mSensitivity : 0.0f;
    }

    // ---- Public state (matches old field layout; read by editor panels) ----
    float mFlowers     { 3.0f };
    float mDabs        { 3.0f };
    float mSensitivity { 0.0f };
    float mBassRelief  { 30.0f };
    bool  mTransformer { false };
    int   mTubeType    { 0 };
    float mTonePre     { 0.0f };
    float mTonePost    { 0.0f };
    float mWet         { 70.0f };
    float mOut         { 0.0f };
    bool  mAutoGain    { false };   // 9b
    int   mOsLog2      { 2 };       // C2: default 2 = 4x  (shared by Tube/Console + Tape)
    Type  mSatType     { Type::Tube }; // H-7: default Tube preserves prior audio
    bool  mVocalBody   { false };      // H-7: vocal-body EQ post-saturation
    HarmonicsMode mHarmonicsMode { HarmonicsMode::Normal };  // H-7

    // ── H-10 (2026-05-02): Tape sub-engine public knobs.  Defaults match
    //   TapeDSP's defaults bit-exact so any preset round-trip through the
    //   folded engine reproduces the legacy Tape sound exactly when
    //   mSatType = Type::Tape.
    float mTapeVibe         { 0.5f };
    float mTapeSpeed        { 0.5f };
    float mTapeHystAmount   { 1.0f };
    float mTapeWowRate      { 0.5f };
    float mTapeWowDepth     { 0.0f };
    float mTapeFlutterRate  { 5.0f };
    float mTapeFlutterDepth { 0.0f };
    float mTapeInputGain    { 1.0f };
    float mTapeOutputGain   { 1.0f };
    float mTapeHiss         { 0.0f };
    float mTapeBias         { 5.0f };
    float mTapePreShelfDb   { 0.0f };
    float mTapeDeShelfDb    { 0.0f };

private:
    // Tube engine (stateless per sample; takes per-sample smoothed params).
    static float processTube (float x, float flowers, float dabs,
                              int tubeType, bool transformer) noexcept;

    // H-7 (2026-05-01): Console engine -- alternative character.  Gentler
    // soft-clip + asymmetric 2nd-harmonic transformer saturation.  Same
    // input scaling (flowers / dabs / transformer) but different curve.
    static float processConsole (float x, float drive, float xfmr,
                                  bool transformer) noexcept;

    void updateFilters();
    void allocateScratch();

    // Tone-shelf LP state (base rate), per channel
    float mPreLP_L  { 0.0f }, mPreLP_R  { 0.0f };
    float mPostLP_L { 0.0f }, mPostLP_R { 0.0f };

    // Bass-split LP state (OS rate), per channel
    float mBassLP_L { 0.0f }, mBassLP_R { 0.0f };

    // DC blocking state (base rate), per channel
    float mDCx_L { 0.0f }, mDCy_L { 0.0f };
    float mDCx_R { 0.0f }, mDCy_R { 0.0f };

    // Filter coefficients
    float mShelfCoef  { 0.0f };   // base rate, 10 kHz
    float mBassLPCoef { 0.0f };   // OS rate, 350 Hz (depends on mOsLog2)
    float mDCCoef     { 0.9995f }; // 9c: SR-aware, 5 Hz target

    // 9a: oversampler + latency ------------------------------------------------
    std::unique_ptr<juce::dsp::Oversampling<float>> mOversampler;
    int mLatencySamples { 0 };

    // H-7: Vocal Body post-saturation EQ -- one peaking dip + one high shelf
    // applied per channel at base rate when mVocalBody == true.
    using StereoIIR = juce::dsp::ProcessorDuplicator<
                          juce::dsp::IIR::Filter<float>,
                          juce::dsp::IIR::Coefficients<float>>;
    StereoIIR mBodyDip;     // -2 dB peaking @ 250 Hz, Q=0.7
    StereoIIR mBodyShelf;   // +2 dB high shelf @ 3 kHz

    // H-7: Linkwitz-Riley splits for Keep Low / Keep High harmonic-routing.
    // KeepLow path uses mLR250 to split off lows < 250 Hz (saturate highs only).
    // KeepHigh path uses mLR4k  to split off highs > 4 kHz (saturate lows only).
    juce::dsp::LinkwitzRileyFilter<float> mLR250Lo, mLR250Hi;
    juce::dsp::LinkwitzRileyFilter<float> mLR4kLo,  mLR4kHi;
    juce::AudioBuffer<float>              mSplitLo, mSplitHi;

    // Scratch buffers
    juce::AudioBuffer<float> mBandBuf;                // 2ch x maxBlock (post-Phase 1, tube domain)
    std::vector<float>       mFlowersScr, mDabsScr,   // per base-sample smoothed
                             mReliefScr,
                             mSensGainScr, mTonePreGainScr,
                             mTonePostGainScr, mWetScr, mOutGainScr;

    // 9d/C1: smoothers (all consumed at base rate; held constant across OS inner loop)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mFlowersSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mDabsSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mReliefSmooth;        // 0..1
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mSensGainSmooth;      // linear
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mTonePreGainSmooth;   // linear
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mTonePostGainSmooth;  // linear
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mWetSmooth;           // 0..1
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mOutGainSmooth;       // linear

    void snapSmoothedToTargets();

    // ─────────────────────────────────────────────────────────────────────
    // H-10 (2026-05-02): Tape sub-engine private state + helpers.  All
    // members below are touched only by processTape() + its prepare/reset
    // hooks; Tube/Console code paths leave them untouched.  Layout mirrors
    // TapeDSP's private members so the bit-exact port is line-by-line.
    // ─────────────────────────────────────────────────────────────────────

    static constexpr float kTapeMaxWowMs = 50.0f;

    // Asymmetric sigmoid shaper (TapeDSP 10b)
    static float tapeAsymShaper (float x, float k) noexcept
    {
        const float sgn = (x >= 0.0f) ? 1.0f : -1.0f;
        return (x + k * x * x * sgn) / (1.0f + std::abs (x));
    }

    // 4-point cubic (Catmull-Rom) interp on a circular buffer (TapeDSP 10e)
    static float tapeCubicCircular (const std::vector<float>& buf, int writePos,
                                     int iOff, float frac, int size) noexcept;

    void tapePrepare (double sampleRate, int maxBlockSize);
    void tapeReset();
    void tapeUpdateFilters();
    void tapeAllocateScratch();
    void tapeSnapSmoothedToTargets();
    void processTape (juce::AudioBuffer<float>& buffer);

    // Wow + flutter delay buffers
    std::vector<float> mTapeWowBufL, mTapeWowBufR;
    int    mTapeWowWritePos { 0 };
    double mTapeWowPhase    { 0.0 };

    // Flutter LFO state (10d)
    double mTapeFlutterPhase      { 0.0 };
    float  mTapeFlutterNoiseState { 0.0f };

    // Hysteresis state (OS domain) -- 10a
    float mTapeHystL { 0.0f }, mTapeHystR { 0.0f };

    // Pre-emphasis shelf state (base rate) -- 10f
    float mTapePreShelfL { 0.0f }, mTapePreShelfR { 0.0f };

    // De-emphasis shelf state (base rate) -- 10f
    float mTapeDeShelfL { 0.0f }, mTapeDeShelfR { 0.0f };

    // Pink-noise filter state -- 10g (per channel for natural L/R decorrelation)
    struct TapePinkState {
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
    TapePinkState mTapePinkL, mTapePinkR;

    // Hiss HPF @ 200 Hz state (per channel) -- 10g
    float mTapeHissHPx_L { 0.0f }, mTapeHissHPy_L { 0.0f };
    float mTapeHissHPx_R { 0.0f }, mTapeHissHPy_R { 0.0f };

    // DC blocker state (per channel) -- 10h
    float mTapeDcX_L { 0.0f }, mTapeDcY_L { 0.0f };
    float mTapeDcX_R { 0.0f }, mTapeDcY_R { 0.0f };

    // Filter coefficients (recomputed in tapeUpdateFilters)
    float mTapePreShelfCoef    { 0.0f };   // base rate, 5 kHz
    float mTapeDeShelfCoef     { 0.0f };   // base rate, 4 kHz
    float mTapePreShelfGainLin { 2.0f };   // from mTapePreShelfDb
    float mTapeDeShelfGainLin  { 0.4f };   // from mTapeDeShelfDb
    float mTapeHissHpCoef      { 0.0f };   // base rate, 200 Hz
    float mTapeDcCoef          { 0.9995f };// base rate, 5 Hz (10h)
    float mTapeHystAlphaOS     { 0.5f };   // OS-effective alpha (time-constant model)

    juce::Random mTapeRandomL, mTapeRandomR;   // independent streams for stereo decorrelation

    // Scratch (tape uses its own band buffer to avoid contention with Saturation's mBandBuf)
    juce::AudioBuffer<float> mTapeBandBuf;
    std::vector<float>       mTapeVibeScr, mTapeHystAlphaOSScr,
                             mTapeBiasScr, mTapePreGainScr, mTapeDeGainScr,
                             mTapeInGainScr, mTapeOutGainScr,
                             mTapeWowDepthScr, mTapeFlutterDepthScr, mTapeHissScr;

    // Smoothers (10i) -- consumed at base rate; held constant across OS inner loop
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mTapeVibeSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mTapeHystAlphaOSSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mTapeBiasSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mTapePreGainSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mTapeDeGainSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mTapeInGainSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mTapeOutGainSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mTapeWowDepthSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mTapeFlutterDepthSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mTapeHissSmooth;
};
