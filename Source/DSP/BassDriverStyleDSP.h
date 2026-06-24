#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include "PolyphaseOversampler.h"

// ─────────────────────────────────────────────────────────────────────────────
// BassDriverStyleDSP - Phase I-6 (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// BB Style Bass Driver pedal.  Multi-band, dynamics-adaptive bass drive in the
// style of the BOSS BB-1X Bass Driver (MDP).  The defining BB-1X trait: it keeps
// the LOW END clean and defined while the grind lives in the mids/highs, and the
// distortion ADAPTS to playing dynamics rather than crushing everything equally.
// We approximate that proprietary "MDP" behavior with: (a) a band split that keeps
// the lows clean, (b) an envelope-driven adaptive drive on the mid/high band so the
// grit comes in on harder playing, and (c) a parallel clean Blend.  Knobs: Level /
// Blend / Low / High / Drive.  (QA-EffectsReview Task 5: was modeled on the Tech 21
// SansAmp; re-pointed to the BB-1X per the fidelity matrix + the adaptive-drive build.)
//
// DSP chain:
//   1. LR4 split into 3 bands at 500 Hz and 2 kHz crossovers (lows < 500 Hz clean).
//   2. Low band: gain = mLow (no clipping -- the "solid low end").
//   3a. Envelope follower on the Mid+High sum -> an adaptive drive (soft playing =
//      less drive/cleaner, hard playing = full drive/grind; preserves dynamics).
//   3b. Mid + High sum: 4x oversample, asymmetric soft-clip (tanh(x+0.2)-tanh(0.2))
//      at the per-sample adaptive drive, downsample.  High band trimmed by mHigh.
//   4. Blend: dry/wet on the clipped Mid+High mix; 0 = all clean, 1 = all clipped.
//   5. Sum all bands.
//   6. 5 Hz DC blocker (asymmetric shaper leaves residual bias at silence).
//   7. Output Level.
// ─────────────────────────────────────────────────────────────────────────────

class BassDriverStyleDSP : public DSPBase
{
public:
    BassDriverStyleDSP() = default;
    ~BassDriverStyleDSP() override = default;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;
    int  getLatencySamples() const override            { return mOs.getLatencySamples(); }

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    void setDrive (float v01);   // 0..1
    void setBlend (float v01);   // 0..1
    void setLow   (float v01);   // 0..1
    void setHigh  (float v01);   // 0..1
    void setLevel (float dB);    // -24..+12

    float mDrive { 0.5f };
    float mBlend { 0.7f };
    float mLow   { 0.7f };
    float mHigh  { 0.7f };
    float mLevel { 0.0f };

private:
    PolyphaseOversampler4x mOs;

    // Linkwitz-Riley 4th-order crossovers.  Two splitters give 3 bands:
    //   mXover1 (500 Hz)  -> low band + (mid + high)
    //   mXover2 (2 kHz)   -> mid band + high band
    juce::dsp::LinkwitzRileyFilter<float> mXover1Lp, mXover1Hp;   // 500 Hz
    juce::dsp::LinkwitzRileyFilter<float> mXover2Lp, mXover2Hp;   // 2 kHz

    juce::AudioBuffer<float> mLowBuf;
    juce::AudioBuffer<float> mMidBuf;
    juce::AudioBuffer<float> mHighBuf;
    juce::AudioBuffer<float> mClippedSumBuf;   // mid+high after clipping
    juce::AudioBuffer<float> mCleanSumBuf;     // mid+high pre-clip (for Blend)

    // QA-EffectsReview Task 5: MDP-style dynamics-adaptive drive.  An envelope
    // follower on the Mid+High sum modulates the drive per base sample so the
    // distortion tracks playing intensity (soft = cleaner, hard = grind) instead
    // of crushing everything at a static level.
    juce::AudioBuffer<float> mDriveEnvBuf;     // 1 ch, per-base-sample adaptive drive
    float mEnv        { 0.0f };
    float mEnvAtkCoef { 0.0f };
    float mEnvRelCoef { 0.0f };

    // 5 Hz DC blocker (asymmetric soft-clip leaves residual bias at silence).
    float mDcXL { 0.0f }, mDcYL { 0.0f };
    float mDcXR { 0.0f }, mDcYR { 0.0f };
    float mDcCoef { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassDriverStyleDSP)
};
