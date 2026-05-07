#include "MicPlacementDSP.h"

// ─────────────────────────────────────────────────────────────────────────────
// MicPlacementDSP - H-6d (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    // Polar response at a given off-axis angle for each pattern.
    // Returns linear gain in [0, 1] for the front-half-plane; figure-8
    // returns negative values past 90 degrees (rear lobe = phase invert).
    float polarResponse (MicPlacementDSP::Polar p, float angleDeg)
    {
        const float a = juce::degreesToRadians (juce::jlimit (-180.0f, 180.0f, angleDeg));
        const float c = std::cos (a);

        switch (p)
        {
            case MicPlacementDSP::Polar::Omni:
                return 1.0f;
            case MicPlacementDSP::Polar::Cardioid:
                // 0.5 + 0.5 cos(angle) -- standard cardioid pickup
                return juce::jmax (0.0f, 0.5f + 0.5f * c);
            case MicPlacementDSP::Polar::Supercardioid:
                // 0.37 + 0.63 cos(angle)
                return juce::jmax (0.0f, 0.37f + 0.63f * c);
            case MicPlacementDSP::Polar::Hypercardioid:
                // 0.25 + 0.75 cos(angle)
                return juce::jmax (0.0f, 0.25f + 0.75f * c);
            case MicPlacementDSP::Polar::Figure8:
                return c;       // pure cosine, can be negative
            default:
                return 1.0f;
        }
    }

    // How much proximity-effect bass lift is appropriate for this pattern
    // at the closest distance (1 cm).  Omni mics have ZERO proximity
    // effect; figure-8 has the most pronounced; cardioids in the middle.
    float proximityScalar (MicPlacementDSP::Polar p)
    {
        switch (p)
        {
            case MicPlacementDSP::Polar::Omni:           return 0.0f;
            case MicPlacementDSP::Polar::Cardioid:       return 1.0f;
            case MicPlacementDSP::Polar::Supercardioid:  return 1.15f;
            case MicPlacementDSP::Polar::Hypercardioid:  return 1.25f;
            case MicPlacementDSP::Polar::Figure8:        return 1.5f;
            default: return 1.0f;
        }
    }
}

const char* MicPlacementDSP::polarDisplayName (int idx)
{
    switch ((Polar) idx)
    {
        case Polar::Omni:          return "Omni";
        case Polar::Cardioid:      return "Cardioid";
        case Polar::Supercardioid: return "Supercardioid";
        case Polar::Hypercardioid: return "Hypercardioid";
        case Polar::Figure8:       return "Figure-8";
        default:                    return "Cardioid";
    }
}

MicPlacementDSP::MicPlacementDSP()  = default;
MicPlacementDSP::~MicPlacementDSP() = default;

void MicPlacementDSP::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    mSampleRate = sampleRate;
    mMaxBlock   = juce::jmax (1, maxBlockSize);
    mNumCh      = juce::jmax (1, numChannels);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) mMaxBlock,
                                   (juce::uint32) mNumCh };
    mAirLp        .prepare (spec); mAirLp        .reset();
    mProximityShelf.prepare (spec); mProximityShelf.reset();
    mOffAxisShelf .prepare (spec); mOffAxisShelf .reset();

    mDryScratch.setSize (mNumCh, mMaxBlock, false, true, true);

    mLastDistanceCm = -1.0f;
    mLastAngleDeg   = 1e9f;
    mLastPolar      = -1;

    updateCoefs();
    mPrepared = true;
}

void MicPlacementDSP::reset()
{
    mAirLp        .reset();
    mProximityShelf.reset();
    mOffAxisShelf .reset();
    mDryScratch.clear();
}

void MicPlacementDSP::setDistanceCm (float cm) noexcept
{
    mDistanceCm = juce::jlimit (1.0f, 150.0f, cm);
}

void MicPlacementDSP::setAngleDeg (float deg) noexcept
{
    mAngleDeg = juce::jlimit (-90.0f, 90.0f, deg);
}

void MicPlacementDSP::setPolar (int idx) noexcept
{
    idx = juce::jlimit (0, (int) Polar::kNumPatterns - 1, idx);
    mPolar = (Polar) idx;
}

void MicPlacementDSP::updateCoefs()
{
    // ── Air-absorption LP cutoff: 20 kHz at 1 cm, ~6 kHz at 150 cm
    //    (linear interpolation in log-frequency).
    const float dist01    = (mDistanceCm - 1.0f) / 149.0f;       // 0..1
    const float airLp_log = juce::jmap (dist01,
                                          std::log (20000.0f),
                                          std::log (6000.0f));
    const float airLpHz   = std::exp (airLp_log);
    auto airC = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (mSampleRate, airLpHz);
    *mAirLp.state = *airC;

    // ── Proximity-effect bass shelf: peaks ~+8 dB at 1 cm cardioid,
    //    fades to 0 dB by ~20 cm; never applies for Omni.
    const float proxScalar  = proximityScalar (mPolar);
    const float proxNear    = juce::jlimit (0.0f, 1.0f, (20.0f - mDistanceCm) / 19.0f);
    const float proxGainDb  = 8.0f * proxNear * proxScalar;
    auto proxC = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
                       mSampleRate, 200.0f, 0.7f,
                       juce::Decibels::decibelsToGain (proxGainDb));
    *mProximityShelf.state = *proxC;

    // ── Off-axis darkening high shelf.  As |angle| grows past ~15
    //    degrees, the shelf cuts the top end progressively (max ~-8 dB
    //    at +/-90 degrees for cardioid).  Omni stays flat.  Figure-8 is
    //    actually brighter directly off-axis at +/-180 but for the +/-90
    //    range we model here it simply darkens like cardioid.
    const float angleAbs = std::abs (mAngleDeg);
    const float angle01  = juce::jlimit (0.0f, 1.0f, (angleAbs - 15.0f) / 75.0f);
    const float darkenMax = (mPolar == Polar::Omni) ? 0.0f : -8.0f;
    const float darkenDb  = darkenMax * angle01;
    auto darkC = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                       mSampleRate, 4000.0f, 0.7f,
                       juce::Decibels::decibelsToGain (darkenDb));
    *mOffAxisShelf.state = *darkC;

    // ── Distance + polar gain attenuation.
    //    Distance follows ~1/r law clamped to a sane near-field range
    //    (1cm = 1.0, 30cm = ~0.6, 150cm = ~0.25).  Polar gain layers on
    //    top.
    const float distGain = juce::jlimit (0.1f, 1.5f,
                                           1.0f / std::sqrt (mDistanceCm / 30.0f));
    const float polarGain = polarResponse (mPolar, mAngleDeg);
    mGainLin = distGain * polarGain;

    mLastDistanceCm = mDistanceCm;
    mLastAngleDeg   = mAngleDeg;
    mLastPolar      = (int) mPolar;
}

void MicPlacementDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (! mPrepared) return;

    const int n  = buffer.getNumSamples();
    const int ch = juce::jmin (buffer.getNumChannels(), mNumCh);
    if (n == 0 || ch == 0) return;

    // Recompute coefs only when something actually changed
    if (! juce::approximatelyEqual (mDistanceCm, mLastDistanceCm)
     || ! juce::approximatelyEqual (mAngleDeg,   mLastAngleDeg)
     || (int) mPolar != mLastPolar)
        updateCoefs();

    // Stash dry for the wet/dry blend
    if (mDryScratch.getNumSamples() < n || mDryScratch.getNumChannels() < ch)
        mDryScratch.setSize (ch, juce::jmax (n, mMaxBlock), false, true, true);
    for (int c = 0; c < ch; ++c)
        mDryScratch.copyFrom (c, 0, buffer, c, 0, n);

    // Apply chain in-place: air LP -> proximity shelf -> off-axis shelf -> gain
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        mAirLp        .process (ctx);
        mProximityShelf.process (ctx);
        mOffAxisShelf .process (ctx);
    }
    if (! juce::approximatelyEqual (mGainLin, 1.0f))
        buffer.applyGain (mGainLin);

    // Wet/dry blend
    const float wet = mMix;
    const float dry = 1.0f - mMix;
    if (juce::approximatelyEqual (wet, 1.0f)) return;
    for (int c = 0; c < ch; ++c)
    {
        float* w = buffer.getWritePointer (c);
        const float* d = mDryScratch.getReadPointer (c);
        for (int i = 0; i < n; ++i)
            w[i] = w[i] * wet + d[i] * dry;
    }
}
