// KBS EQ Pro — EQ Match: fit bands to the difference between two spectra.
//
// Capture what the track sounds like, capture what it should sound like, and
// place bells until the difference is gone. The capturing is the editor's
// business (averaged analyser frames, or a file); this header is only the
// arithmetic, JUCE-free so test_core can hold it to a number: a known curve,
// matched from its own spectra, must come back within a decibel.
//
// The fit is greedy: smooth the difference, put a bell on the largest
// remaining error with the width the error actually has, subtract that
// bell's true response, repeat. Greedy is right for an EQ - each pass takes
// the most audible error first, so stopping early (fewer bands, or a good
// enough residual) always leaves the best partial fit those bands could
// have made.
#pragma once

#include "ParametricEq.h"

#include <vector>

namespace kbs {

class EqMatch
{
public:
    // The working grid: log-spaced, dense enough that a Q of 10 is seen.
    static constexpr int kPoints = 240;
    static constexpr double kLoHz = 20.0, kHiHz = 20000.0;

    static double hzAt (int i)
    {
        return kLoHz * std::pow (kHiHz / kLoHz, (double) i / (kPoints - 1));
    }

    struct Result
    {
        std::vector<EqBandParams> bands;
        float residualRmsDb = 0.0f;      // what is left after the fit
        float targetRmsDb = 0.0f;        // what the fit was asked to remove
    };

    // referenceDb / currentDb: kPoints of dB, both on hzAt's grid.
    // smoothness 0..1: 0 = follow every wrinkle (1/12 oct), 1 = broad
    // strokes (1.5 oct). maxBands caps how many bells may be spent.
    static Result fit (const float* referenceDb, const float* currentDb,
                       float smoothness, int maxBands, double sampleRate = 48000.0)
    {
        Result out;
        maxBands = std::clamp (maxBands, 1, ParametricEq::kMaxBands);

        // The difference, gain-neutralised: matching overall level is the
        // output trim's job, not a band's. Subtracting the mean keeps the
        // fit spending bands on shape.
        std::vector<double> diff (kPoints);
        double mean = 0.0;
        for (int i = 0; i < kPoints; ++i)
        {
            diff[(size_t) i] = (double) referenceDb[i] - currentDb[i];
            mean += diff[(size_t) i];
        }
        mean /= kPoints;
        for (auto& d : diff) d -= mean;

        // Smooth in the log domain. Sigma in grid points from the octave
        // width asked for: the grid spans log2(1000) ~ 9.97 octaves.
        const double octaves = 1.0 / 12.0 + (1.5 - 1.0 / 12.0) * std::clamp (smoothness, 0.0f, 1.0f);
        const double pointsPerOct = (kPoints - 1) / std::log2 (kHiHz / kLoHz);
        smoothGaussian (diff, octaves * pointsPerOct * 0.5);

        out.targetRmsDb = rms (diff);

        // ── the greedy loop ───────────────────────────────────────────────
        std::vector<double> residual = diff;

        for (int used = 0; used < maxBands; ++used)
        {
            int at = 0;
            double err = 0.0;
            for (int i = 0; i < kPoints; ++i)
                if (std::abs (residual[(size_t) i]) > std::abs (err))
                { err = residual[(size_t) i]; at = i; }

            if (std::abs (err) < 0.5) break;      // inaudible: stop spending

            const double f0 = hzAt (at);

            // The error's own width: walk out to the half-gain points, in
            // grid steps, and read Q off the octave distance between them.
            const double half = err / 2.0;
            int lo = at, hi = at;
            while (lo > 0 && sameSideBeyond (residual[(size_t) (lo - 1)], half)) --lo;
            while (hi < kPoints - 1 && sameSideBeyond (residual[(size_t) (hi + 1)], half)) ++hi;
            const double widthOct = std::max (0.15, (hi - lo) / pointsPerOct);
            const double q = std::clamp (1.6 / widthOct, 0.3, 12.0);

            EqBandParams b;
            b.on = true;
            b.type = EqType::bell;
            b.freqHz = (float) std::clamp (f0, 25.0, 19000.0);
            b.gainDb = (float) std::clamp (err, -18.0, 18.0);
            b.q = (float) q;
            out.bands.push_back (b);

            // Subtract the bell that will actually be run - the RBJ response,
            // not an idealised gaussian - so the next pass sees the truth.
            BiquadCoeffs c = Biquad::peaking (b.freqHz, b.q, b.gainDb, sampleRate);
            for (int i = 0; i < kPoints; ++i)
            {
                double mag, ph;
                eqdesign::biquadResponse (c, 2.0 * kPi * hzAt (i) / sampleRate, mag, ph);
                residual[(size_t) i] -= 20.0 * std::log10 (std::max (1.0e-9, mag));
            }
        }

        out.residualRmsDb = rms (residual);
        return out;
    }

private:
    static bool sameSideBeyond (double v, double half)
    {
        return half > 0.0 ? v > half : v < half;
    }

    static float rms (const std::vector<double>& v)
    {
        double s = 0.0;
        for (double x : v) s += x * x;
        return (float) std::sqrt (s / (double) v.size());
    }

    static void smoothGaussian (std::vector<double>& v, double sigmaPts)
    {
        if (sigmaPts < 0.5) return;
        const int rad = (int) std::ceil (sigmaPts * 3.0);
        std::vector<double> kernel ((size_t) rad * 2 + 1);
        double sum = 0.0;
        for (int i = -rad; i <= rad; ++i)
        {
            const double w = std::exp (-0.5 * (i / sigmaPts) * (i / sigmaPts));
            kernel[(size_t) (i + rad)] = w;
            sum += w;
        }
        for (auto& k : kernel) k /= sum;

        std::vector<double> outV (v.size());
        for (int i = 0; i < (int) v.size(); ++i)
        {
            double acc = 0.0;
            for (int j = -rad; j <= rad; ++j)
            {
                const int idx = std::clamp (i + j, 0, (int) v.size() - 1);
                acc += v[(size_t) idx] * kernel[(size_t) (j + rad)];
            }
            outV[(size_t) i] = acc;
        }
        v = std::move (outV);
    }
};

} // namespace kbs
