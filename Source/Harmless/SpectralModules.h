#pragma once
#include <JuceHeader.h>
#include <array>

// ── SpectralModules ───────────────────────────────────────────────────────────
// Five spectral processors that manipulate the partial amplitude/phase arrays
// BEFORE the IFFT in HarmonicEngine::buildWavetable().
//
// Each module receives working copies of the partial arrays (not the originals),
// so the user's hand-edited partial data is never destroyed by processing.
//
// PrismModule is a special case: it does NOT modify the amps/phases arrays.
// Instead it provides getFrequencyShift(i), which HarmonicEngine uses for
// fractional-bin interpolation during the IFFT input assembly loop.
//
// NOTE: kMaxPartials must match HarmonicEngine::kNumPartials = 516.
// ─────────────────────────────────────────────────────────────────────────────
static constexpr int kMaxPartials = 516;

// ── Base ──────────────────────────────────────────────────────────────────────
class SpectralModule
{
public:
    virtual ~SpectralModule() = default;

    // Modify amplitude and phase arrays in-place.  Arrays are working copies -
    // safe to overwrite.  numPartials ≤ kMaxPartials.
    virtual void processPartials (float* amps, float* phases,
                                   int numPartials) = 0;

    void  setAmount (float a) noexcept { mAmount = juce::jlimit (0.0f, 1.0f, a); }
    float getAmount() const  noexcept { return mAmount; }

protected:
    float mAmount { 0.0f };
};

// ── PrismModule ───────────────────────────────────────────────────────────────
// Introduces inharmonicity by shifting each partial's bin position by a small
// amount proportional to (partialIndex)².  At low amounts this mimics the
// natural inharmonicity of piano strings; at high amounts it creates bell-like
// metallic spectra.
//
// processPartials() is a no-op - the shift is applied inside
// HarmonicEngine::buildWavetable() via getFrequencyShift().
class PrismModule final : public SpectralModule
{
public:
    void processPartials (float*, float*, int) override {}  // handled in buildWavetable

    // 2026-04-19 (S1) T2-D: prism_mode selects the inharmonic-spread shape.
    //   0 = stretched (i^2 - default piano-like inharmonicity)
    //   1 = bunched   (sqrt(i) - softer spread, closer to harmonic series)
    //   2 = scattered (alternating sign per partial - bell/metallic)
    void setMode (int mode) noexcept { mMode = juce::jlimit (0, 2, mode); }

    // Returns the fractional bin offset for partial index i (0-based).
    // i=0 → partial 1 (fundamental).
    float getFrequencyShift (int i) const noexcept
    {
        constexpr float kStretch = 0.0001f;
        const float fi = float (i + 1);
        switch (mMode)
        {
            case 1:  return mAmount * std::sqrt (fi)             * kStretch * 100.f;  // bunched
            case 2:  return mAmount * fi * ((i & 1) ? -1.f : 1.f) * kStretch * 50.f;   // scattered
            default: return mAmount * fi * fi                     * kStretch;          // stretched
        }
    }

private:
    int mMode { 0 };
};

// ── PluckModule ───────────────────────────────────────────────────────────────
// Applies an exponential amplitude decay across partials - higher harmonics
// fade faster, leaving a duller, rounder tone.  Mimics the natural decay of
// plucked / hammered string harmonics.
class PluckModule final : public SpectralModule
{
public:
    // 2026-04-19 (S1) T2-N: pluck_blur softens the pluck transient by widening
    // the decay curve - higher partials still decay but with a gentler roll-off.
    void setBlur (bool blurOn) noexcept { mBlur = blurOn; }

    void processPartials (float* amps, float* /*phases*/,
                           int numPartials) override
    {
        if (mAmount < 1.0e-4f)
            return;

        // decay_rate: at amount=1 partial 100 is attenuated by exp(-1) ≈ 37%.
        // Blur softens the curve by halving the effective rate.
        constexpr float kDecayRate = 0.0001f;
        const float rate = mAmount * kDecayRate * (mBlur ? 0.5f : 1.f);

        for (int i = 0; i < numPartials; ++i)
            amps[i] *= std::exp (-rate * float (i));
    }

private:
    bool mBlur { false };
};

// ── BlurModule ────────────────────────────────────────────────────────────────
// Gaussian blur (approximated by 3 box-blur passes) across the amplitude
// spectrum.  Smears energy from sharp spectral peaks into their neighbours,
// giving a pad-like, diffuse quality.
class BlurModule final : public SpectralModule
{
public:
    // 2026-04-19 (S2 SLA #8): time scale - multiplies the kernel half-width.
    // 1.0 = default; 0 = sharpest; 2 = widest. Affects temporal smearing feel.
    void setTimeScale (float t) noexcept { mTimeScale = juce::jlimit (0.f, 2.f, t); }
    // 2026-04-19 (S2 SLA #9): harm axis - 0 = linear partial-index blur (default
    // behaviour), 1 = harmonic-only blur (skip non-harmonic partials).
    // Implementation: at h=1 the kernel only averages partials whose index is
    // a multiple of the band's harmonic spacing (every 2nd partial here).
    void setHarmAxis (float h) noexcept { mHarmAxis = juce::jlimit (0.f, 1.f, h); }

    void processPartials (float* amps, float* /*phases*/,
                           int numPartials) override
    {
        if (mAmount < 1.0e-4f)
            return;

        // Half-width of the box kernel in partials, scaled by time.
        // At amount=1, time=1 -> 25; at time=2 -> 50; at time=0 -> 1.
        const int halfW = juce::jmax (1, int (mAmount * 25.0f * mTimeScale));

        // Three box-blur passes approximate a Gaussian.
        // Harm-axis: when mHarmAxis > 0.5 we step by 2 instead of 1, so only
        // harmonic-related partials get smeared together. Otherwise standard.
        const int step = (mHarmAxis > 0.5f) ? 2 : 1;
        for (int pass = 0; pass < 3; ++pass)
            boxBlur (amps, numPartials, halfW, step);
    }

private:
    std::array<float, kMaxPartials> mTemp {};
    float mTimeScale { 1.0f };
    float mHarmAxis  { 0.0f };

    void boxBlur (float* amps, int n, int halfW, int step = 1)
    {
        // Prefix-sum sliding window. When step>1, only every step-th partial
        // contributes to the average (harm-axis blur). Off-step partials get
        // their own averaged value at half the strength so they don't pop.
        float sum = 0.0f;
        int   cnt = 0;

        for (int i = 0; i < n; ++i)
        {
            int lo = i - halfW - 1;
            int hi = i + halfW;
            if (hi >= n) hi = n - 1;
            if (lo >= 0 && (lo % step) == 0) { sum -= amps[lo]; --cnt; }
            if (hi < n  && (hi % step) == 0) { sum += amps[hi]; ++cnt; }
            mTemp[i] = (cnt > 0) ? sum / float (cnt) : amps[i];
        }
        std::copy (mTemp.begin(), mTemp.begin() + n, amps);
    }
};

// ── FilterMaskModule ──────────────────────────────────────────────────────────
// Acts as a brick-wall low-pass on the partial spectrum: partials above a
// cutoff index are zeroed.  amount=0 leaves all partials untouched; amount=1
// silences all partials (fully closed).
//
// The APVTS "filter_mask_cutoff" Hz value is mapped externally to a 0–1
// normalised amount before calling setAmount().
class FilterMaskModule final : public SpectralModule
{
public:
    void processPartials (float* amps, float* /*phases*/,
                           int numPartials) override
    {
        if (mAmount < 1.0e-4f)
            return;

        // maxPartial: at amount=0 → all kMaxPartials pass; at amount=1 → 0 pass.
        const int maxPartial = int (float (numPartials) * (1.0f - mAmount));

        for (int i = maxPartial; i < numPartials; ++i)
            amps[i] = 0.0f;
    }
};

// ── PhaserMaskModule ──────────────────────────────────────────────────────────
// Applies a sinusoidal amplitude envelope across partial indices, producing
// evenly-spaced spectral notches similar to a comb / phaser filter.
// amount=0 leaves the spectrum flat; amount=1 creates deep notches at intervals
// of (kFFTSize / (rate * kScale)) bins.
class PhaserMaskModule final : public SpectralModule
{
public:
    // rate: APVTS "phaser_mask_rate" (0.1–10). Controls notch spacing.
    void setRate (float rate) noexcept { mRate = rate; }

    void processPartials (float* amps, float* /*phases*/,
                           int numPartials) override
    {
        if (mAmount < 1.0e-4f)
            return;

        // kScale chosen so rate=1 gives one full notch cycle per ~50 partials.
        constexpr float kScale = juce::MathConstants<float>::twoPi / 50.0f;
        const float depth = mAmount;

        for (int i = 0; i < numPartials; ++i)
        {
            // Envelope: 1 at peaks, (1-depth) at notches.
            float env = 1.0f - depth * 0.5f
                        * (1.0f - std::cos (float (i) * mRate * kScale));
            amps[i] *= env;
        }
    }

private:
    float mRate { 1.0f };
};
