#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"

// -- SaturationDSP ------------------------------------------------------------
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
    int   mOsLog2      { 2 };       // C2: default 2 = 4x

private:
    // Tube engine (stateless per sample; takes per-sample smoothed params).
    static float processTube (float x, float flowers, float dabs,
                              int tubeType, bool transformer) noexcept;

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
};
