// KBS EQ Pro / BaySickDAW - offline spectrum accumulator (QA-EqFlagship W-22).
//
// The Match panel's live capture averages analyser frames while the user
// plays.  A track SCAN renders the timeline offline and pushes the strip's
// pre-EQ audio through this instead: same outputs the live capture produces
// (mid + side averages on the match grid, plus the mid capture's per-point
// swing for the static-against-dynamic split), fed at render speed with no
// feed ring and no dropped frames.  JUCE-free so the test target can hold it
// to a number.
#pragma once

#include "FFT.h"

#include <algorithm>
#include <complex>
#include <vector>

namespace kbs {

class SpectrumScan
{
public:
    static constexpr int kOrder = 12;              // 4096-point frames
    static constexpr int kSize  = 1 << kOrder;
    static constexpr int kHop   = kSize / 2;
    static constexpr int kBins  = kSize / 2;

    SpectrumScan() : fft (kOrder)
    {
        window.resize ((size_t) kSize);
        for (int i = 0; i < kSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) kSize);
        ringL.resize ((size_t) kSize, 0.0f);
        ringR.resize ((size_t) kSize, 0.0f);
        tdM.resize ((size_t) kSize);
        tdS.resize ((size_t) kSize);
        reset();
    }

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        reset();
    }

    void reset()
    {
        std::fill (ringL.begin(), ringL.end(), 0.0f);
        std::fill (ringR.begin(), ringR.end(), 0.0f);
        sumM.assign ((size_t) kBins, 0.0);
        sqM.assign ((size_t) kBins, 0.0);
        sumS.assign ((size_t) kBins, 0.0);
        fill = 0;
        sinceHop = 0;
        frameCount = 0;
        primed = false;
    }

    void push (const float* l, const float* r, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            ringL[(size_t) fill] = l[i];
            ringR[(size_t) fill] = r != nullptr ? r[i] : l[i];
            fill = (fill + 1) % kSize;
            if (++sinceHop >= kHop)
            {
                sinceHop = 0;
                if (primed) accumulateFrame();
                else primed = true;        // the first hop only half-fills
            }
        }
    }

    int frames() const { return frameCount; }

    bool midGrid (float* out, int points, double loHz, double hiHz) const
        { return gridOf (sumM, out, points, loHz, hiHz, 2); }

    bool sideGrid (float* out, int points, double loHz, double hiHz) const
        { return gridOf (sumS, out, points, loHz, hiHz, 2); }

    // Per-point swing of the MID capture (std dev of dB across frames) - the
    // same statistic EqAnalyser::averagedSpreadGrid feeds the match fitter.
    bool spreadGrid (float* out, int points, double loHz, double hiHz) const
    {
        if (frameCount < 4) return false;
        for (int i = 0; i < points; ++i)
        {
            const int k = binFor (i, points, loHz, hiHz);
            const double mean = sumM[(size_t) k] / frameCount;
            const double var = std::max (0.0, sqM[(size_t) k] / frameCount
                                              - mean * mean);
            out[i] = (float) std::sqrt (var);
        }
        return true;
    }

    // A mono source's side curve is near-silence, not a description of
    // anything - the same >= 6 dB dynamic-range rule the file loader uses.
    bool sideMeaningful() const
    {
        if (frameCount < 2) return false;
        double lo = 1.0e9, hi = -1.0e9;
        for (int k = 2; k < kBins - 2; ++k)
        {
            const double v = sumS[(size_t) k] / frameCount;
            lo = std::min (lo, v);
            hi = std::max (hi, v);
        }
        return (hi - lo) >= 6.0;
    }

private:
    void accumulateFrame()
    {
        // The ring's oldest sample is at `fill` (just wrapped past it).
        for (int i = 0; i < kSize; ++i)
        {
            const int idx = (fill + i) % kSize;
            const float lm = ringL[(size_t) idx];
            const float rm = ringR[(size_t) idx];
            const float w = window[(size_t) i];
            tdM[(size_t) i] = { 0.5f * (lm + rm) * w, 0.0f };
            tdS[(size_t) i] = { 0.5f * (lm - rm) * w, 0.0f };
        }
        fft.transform (tdM.data(), false);
        fft.transform (tdS.data(), false);
        const float norm = 4.0f / (float) kSize;
        for (int k = 0; k < kBins; ++k)
        {
            const double dbM = 20.0 * std::log10 (
                std::max (1.0e-7f, std::abs (tdM[(size_t) k]) * norm));
            const double dbS = 20.0 * std::log10 (
                std::max (1.0e-7f, std::abs (tdS[(size_t) k]) * norm));
            sumM[(size_t) k] += dbM;
            sqM[(size_t) k]  += dbM * dbM;
            sumS[(size_t) k] += dbS;
        }
        ++frameCount;
    }

    int binFor (int i, int points, double loHz, double hiHz) const
    {
        const double hz = loHz * std::pow (hiHz / loHz,
                                           (double) i / (double) (points - 1));
        const double bin = hz * kSize / sr;
        return bin < 1.0 ? 1 : bin > kBins - 2 ? kBins - 2 : (int) bin;
    }

    bool gridOf (const std::vector<double>& sum, float* out, int points,
                 double loHz, double hiHz, int minFrames) const
    {
        if (frameCount < minFrames) return false;
        for (int i = 0; i < points; ++i)
            out[i] = (float) (sum[(size_t) binFor (i, points, loHz, hiHz)]
                              / frameCount);
        return true;
    }

    FFT fft;
    double sr = 48000.0;
    std::vector<float> window, ringL, ringR;
    std::vector<std::complex<float>> tdM, tdS;
    std::vector<double> sumM, sqM, sumS;
    int fill = 0, sinceHop = 0, frameCount = 0;
    bool primed = false;
};

} // namespace kbs
