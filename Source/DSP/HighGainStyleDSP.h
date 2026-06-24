#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include "PolyphaseOversampler.h"

// ─────────────────────────────────────────────────────────────────────────────
// HighGainStyleDSP - Phase I-5 (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// MT Style High-Gain pedal.  Modern metal stack in the style of the BOSS
// MT-2 Metal Zone.  Knobs: Level / Dist / High / Low / Mid Hz / Mid dB.
// (Six knobs, no concentrics per locked spec call -- Mid is split into two
// separate knobs.)
//
// DSP chain:
//   1. Fixed mid-boost biquad pre-clip (locks the metal "honk" character).
//   2. 4x oversample.
//   3. Cascading hard clip: two clamp stages with a small smoothing curve
//      between them so the "stacked transistor" sound lands.
//   4. Downsample.
//   5. Two fixed post-clip gyrator boosts (~100 Hz + ~5 kHz); the V/scoop is the
//      valley between them (the real pedal boosts the extremes, not cuts the mids).
//   6. User 3-band EQ: Low shelf (~80 Hz), parametric Mid (Hz + dB), High
//      shelf (~5 kHz).
//   7. Output level.
// ─────────────────────────────────────────────────────────────────────────────

class HighGainStyleDSP : public DSPBase
{
public:
    HighGainStyleDSP() = default;
    ~HighGainStyleDSP() override = default;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;
    int  getLatencySamples() const override            { return mOs.getLatencySamples(); }

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    void setDist  (float v01);   // 0..1
    void setHighDb (float dB);   // -15..+15
    void setLowDb  (float dB);   // -15..+15
    void setMidHz  (float hz);   // 200..5000
    void setMidDb  (float dB);   // -15..+15
    void setLevel  (float dB);   // -24..+12

    float mDist   { 0.5f };
    float mHighDb { 0.0f };
    float mLowDb  { 0.0f };
    float mMidHz  { 800.0f };
    float mMidDb  { 0.0f };
    float mLevel  { 0.0f };

private:
    void updateUserEqCoefs();
    void initFixedEqCoefs();   // pre-clip mid-boost + post-clip mid-scoop

    PolyphaseOversampler4x mOs;

    using StereoFilter = juce::dsp::ProcessorDuplicator<
                            juce::dsp::IIR::Filter<float>,
                            juce::dsp::IIR::Coefficients<float>>;

    // Fixed character (don't move with knobs):
    StereoFilter mFixedMidBoost;   // pre-clip, locks ~1 kHz +36 dB (QA-EffectsReview Task 5)
    // QA-EffectsReview Task 5: the real pedal's post-clip "V" is NOT a mid cut -- it's
    // two fixed gyrator BOOSTS (~100 Hz + ~5 kHz); the scoop is the valley between them.
    StereoFilter mFixedScoopLo;    // post-clip boost, ~100 Hz
    StereoFilter mFixedScoopHi;    // post-clip boost, ~5 kHz (the fixed treble spike)

    // User 3-band EQ:
    StereoFilter mUserLowShelf;    // ~80 Hz, +/- mLowDb
    StereoFilter mUserMidPeak;     // mMidHz, +/- mMidDb
    StereoFilter mUserHighShelf;   // ~5 kHz, +/- mHighDb

    // I-5 polish (2026-05-02): defensive 5 Hz DC blocker.  Cascading tanh
    // and clamp stages are symmetric so silence in -> silence out, but the
    // five biquad stages (mid-boost / mid-scoop / 3-band user EQ) can store
    // tiny DC during bypass-ramp transients that would otherwise leak.
    float mDcXL { 0.0f }, mDcYL { 0.0f };
    float mDcXR { 0.0f }, mDcYR { 0.0f };
    float mDcCoef { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HighGainStyleDSP)
};
