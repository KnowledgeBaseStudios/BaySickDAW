#pragma once
#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
// MicPlacementDSP - H-6d (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// Virtual mic placement model.  Lives on BaySickNAMIRProcessor after
// MicSimDSP and before the master output.  Models three things:
//
//   Distance (1cm..150cm)
//     - 1/r law gain attenuation as distance grows
//     - 1st-order LP "air absorption" rolloff that moves down with
//       distance (close = brighter, far = darker)
//     - proximity-effect bass shelf that lifts at close distances and
//       fades out past ~20 cm (typical cardioid behaviour)
//
//   Off-axis Angle (-90..+90 degrees)
//     - cos(angle) gain rolloff scaled by polar-pattern strength
//       (Omni = no rolloff, Cardioid = typical rear rejection,
//       Hypercardioid = sharper, Fig-8 = +/-90 null + rear-lobe
//       phase invert)
//     - high-shelf darkening as angle moves off-axis (most LDC mics
//       get noticeably darker at >30 degrees)
//
//   Polar pattern (Omni / Cardioid / Supercardioid / Hypercardioid /
//                  Figure-8) - selects the rolloff curve + how much
//   off-axis darkening + whether proximity effect applies.
//
// Mix knob = wet/dry blend (default 100% wet).
//
// Threading: setters run on the message thread; process runs on audio.
// All filter coef updates are guarded by approximate-equal checks so
// only actual changes trigger recomputation.
// ─────────────────────────────────────────────────────────────────────────────

class MicPlacementDSP
{
public:
    enum class Polar : int
    {
        Omni          = 0,
        Cardioid      = 1,
        Supercardioid = 2,
        Hypercardioid = 3,
        Figure8       = 4,
        kNumPatterns  = 5
    };

    MicPlacementDSP();
    ~MicPlacementDSP();

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void process (juce::AudioBuffer<float>& buffer);

    void setDistanceCm (float cm)        noexcept;
    void setAngleDeg   (float deg)       noexcept;
    void setPolar      (int  patternIdx) noexcept;
    void setMix        (float mix01)     noexcept { mMix = juce::jlimit (0.0f, 1.0f, mix01); }

    float getDistanceCm() const noexcept { return mDistanceCm; }
    float getAngleDeg()   const noexcept { return mAngleDeg; }
    int   getPolar()      const noexcept { return (int) mPolar; }


private:
    using StereoIIR = juce::dsp::ProcessorDuplicator<
                          juce::dsp::IIR::Filter<float>,
                          juce::dsp::IIR::Coefficients<float>>;

    void updateCoefs();   // recomputes air-LP, proximity-shelf, off-axis-shelf

    double mSampleRate { 44100.0 };
    int    mMaxBlock   { 512 };
    int    mNumCh      { 2 };
    bool   mPrepared   { false };

    float mDistanceCm { 30.0f };
    float mAngleDeg   { 0.0f };
    Polar mPolar      { Polar::Cardioid };
    float mMix        { 1.0f };

    StereoIIR mAirLp;          // 1st-order LP for air absorption
    StereoIIR mProximityShelf; // low shelf -- lifts bass at close distances
    StereoIIR mOffAxisShelf;   // high shelf -- darkens off-axis

    float mGainLin { 1.0f };   // computed from distance + polar response

    // Last-applied values for change detection (avoid recomputing coefs
    // every block when nothing has moved).
    float mLastDistanceCm { -1.0f };
    float mLastAngleDeg   { 1e9f };
    int   mLastPolar      { -1 };

    juce::AudioBuffer<float> mDryScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicPlacementDSP)
};
