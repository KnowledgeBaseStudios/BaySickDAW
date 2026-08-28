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

    // How much a point has to swing before its error counts as occasional
    // rather than constant. Measured on programme material, a steady band
    // sits near 3 dB of spread and a band driven by peaks runs well past it.
    static constexpr double kOccasionalDb = 4.5;

    static double hzAt (int i)
    {
        return kLoHz * std::pow (kHiHz / kLoHz, (double) i / (kPoints - 1));
    }

    struct Result
    {
        std::vector<EqBandParams> bands;
        float residualRmsDb = 0.0f;      // what is left after the fit
        float targetRmsDb = 0.0f;        // what the fit was asked to remove
        int dynamicBands = 0;            // how many of them came out dynamic
    };

    // referenceDb / currentDb: kPoints of dB, both on hzAt's grid.
    // smoothness 0..1: 0 = follow every wrinkle (1/12 oct), 1 = broad
    // strokes (1.5 oct). maxBands caps how many bells may be spent.
    // spreadDb is optional: kPoints of how far the CURRENT capture swings
    // around its own average at each point, from
    // EqAnalyser::averagedSpreadGrid. Given it, the fit distinguishes a
    // constant problem from an occasional one - a 3 kHz excess that only
    // appears on the loud moments is not something a static cut should
    // solve, because the static cut dulls the other ninety percent. Absent,
    // every band is static and the result is bit-identical to before.
    static Result fit (const float* referenceDb, const float* currentDb,
                       float smoothness, int maxBands, double sampleRate = 48000.0,
                       const float* spreadDb = nullptr)
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

            // A cut whose frequency swings a lot in the source is a problem
            // that comes and goes. Hand the constant part to static gain and
            // the rest to a dynamic range, so quiet passages keep their tone.
            // Boosts stay static: an occasional hole is not a thing dynamics
            // can fill without pumping.
            if (spreadDb != nullptr && err < -0.5)
            {
                const double swing = spreadDb[at];
                if (swing > kOccasionalDb)
                {
                    const double share = std::clamp ((swing - kOccasionalDb) / kOccasionalDb,
                                                     0.0, 0.75);
                    const double dynPart = std::abs (b.gainDb) * share;
                    b.gainDb = (float) (b.gainDb + dynPart);     // toward zero
                    b.dynamic = true;
                    b.rangeDb = (float) -dynPart;
                    b.thresholdDb = -12.0f;      // programme-relative, like every dynamic here
                    b.ratio = 4.0f;
                    b.attackMs = 5.0f;
                    b.releaseMs = 120.0f;
                    out.dynamicBands++;
                }
            }

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

    // W-2: Auto Cleanup - resonance and problem detection with NO reference.
    // The capture is matched against its own broad-stroke self (a ~1-octave
    // smoothing of itself): whatever stands narrowly over its own
    // neighborhood is a resonance, whatever the neighborhood already is is
    // the tone - so only pokes get bands, and only CUTS are emitted.  The
    // same spread rule fit() uses sends intermittent pokes to dynamic bands
    // so the quiet ninety percent keeps its tone.
    static Result cleanup (const float* currentDb, int maxBands,
                           double sampleRate = 48000.0,
                           const float* spreadDb = nullptr)
    {
        Result out;
        maxBands = std::clamp (maxBands, 1, ParametricEq::kMaxBands);
        const double pointsPerOct = (kPoints - 1) / std::log2 (kHiHz / kLoHz);

        std::vector<double> cur (kPoints), broad (kPoints);
        for (int i = 0; i < kPoints; ++i)
            cur[(size_t) i] = broad[(size_t) i] = (double) currentDb[i];
        smoothGaussian (broad, 1.0 * pointsPerOct * 0.5);

        // Negative where the capture pokes above its neighborhood; the light
        // 1/12-oct pass keeps single-bin noise from earning a band.
        std::vector<double> residual (kPoints);
        for (int i = 0; i < kPoints; ++i)
            residual[(size_t) i] = broad[(size_t) i] - cur[(size_t) i];
        smoothGaussian (residual, (1.0 / 12.0) * pointsPerOct * 0.5);
        out.targetRmsDb = rms (residual);

        for (int used = 0; used < maxBands; ++used)
        {
            int at = 0;
            double err = 0.0;
            for (int i = 0; i < kPoints; ++i)
                if (residual[(size_t) i] < err) { err = residual[(size_t) i]; at = i; }

            // Under 2 dB of poke is tone, not trouble.
            if (err > -2.0) break;

            const double half = err / 2.0;
            int lo = at, hi = at;
            while (lo > 0 && sameSideBeyond (residual[(size_t) (lo - 1)], half)) --lo;
            while (hi < kPoints - 1 && sameSideBeyond (residual[(size_t) (hi + 1)], half)) ++hi;
            const double widthOct = std::max (0.15, (hi - lo) / pointsPerOct);
            const double q = std::clamp (1.6 / widthOct, 1.0, 12.0);

            EqBandParams b;
            b.on = true;
            b.type = EqType::bell;
            b.freqHz = (float) std::clamp (hzAt (at), 40.0, 16000.0);
            b.gainDb = (float) std::clamp (err, -18.0, 0.0);
            b.q = (float) q;

            if (spreadDb != nullptr)
            {
                const double swing = spreadDb[at];
                if (swing > kOccasionalDb)
                {
                    const double share = std::clamp ((swing - kOccasionalDb) / kOccasionalDb,
                                                     0.0, 0.75);
                    const double dynPart = std::abs (b.gainDb) * share;
                    b.gainDb = (float) (b.gainDb + dynPart);     // toward zero
                    b.dynamic = true;
                    b.rangeDb = (float) -dynPart;
                    b.thresholdDb = -12.0f;
                    b.ratio = 4.0f;
                    b.attackMs = 5.0f;
                    b.releaseMs = 120.0f;
                    out.dynamicBands++;
                }
            }

            out.bands.push_back (b);

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

public:
    // ── mid and side, from one operation ──────────────────────────────
    //
    // Four spectra in: what the track is and what it should be, each split
    // into mid and side. Two differences out, and bands placed across all
    // three domains from ONE budget - because 24 bands is what there is,
    // and the music decides how they are spent, not a quota.
    //
    // Stereo against mid+side is decided per band by the owner's rule: take
    // the common part with a candidate stereo band, then count how many
    // domains still need a band there.
    //
    //   neither      -> stereo. The domains agreed; one band instead of two.
    //   both         -> stereo. It is doing what two bands would, and the
    //                   mid and side bands beside it are the augmentation.
    //   exactly one  -> NOT stereo. Mid and side each get their own band.
    //                   "Stereo plus mid" spends the same two bands while
    //                   hiding why either exists, and a single Q cannot be
    //                   right for two errors of different widths.
    static Result fitMidSide (const float* refMidDb, const float* refSideDb,
                              const float* curMidDb, const float* curSideDb,
                              float smoothness, int maxBands,
                              double sampleRate = 48000.0,
                              const float* spreadMidDb = nullptr,
                              const float* spreadSideDb = nullptr)
    {
        Result out;
        maxBands = std::clamp (maxBands, 1, ParametricEq::kMaxBands);

        const double octaves = 1.0 / 12.0 + (1.5 - 1.0 / 12.0) * std::clamp (smoothness, 0.0f, 1.0f);
        const double pointsPerOct = (kPoints - 1) / std::log2 (kHiHz / kLoHz);

        auto prepare = [&] (const float* ref, const float* cur)
        {
            std::vector<double> d (kPoints);
            double mean = 0.0;
            for (int i = 0; i < kPoints; ++i)
            {
                d[(size_t) i] = (double) ref[i] - cur[i];
                mean += d[(size_t) i];
            }
            mean /= kPoints;
            for (auto& v : d) v -= mean;
            smoothGaussian (d, octaves * pointsPerOct * 0.5);
            return d;
        };

        std::vector<double> resMid  = prepare (refMidDb,  curMidDb);
        std::vector<double> resSide = prepare (refSideDb, curSideDb);

        {
            std::vector<double> both (kPoints);
            for (int i = 0; i < kPoints; ++i)
                both[(size_t) i] = 0.5 * (resMid[(size_t) i] + resSide[(size_t) i]);
            out.targetRmsDb = rms (both);
        }

        // Q from the error's own width, the same way the single-domain fit
        // reads it: walk out to the half-gain points and measure the octaves
        // between them.
        auto widthAt = [&] (const std::vector<double>& res, int at)
        {
            const double err = res[(size_t) at];
            const double half = err / 2.0;
            int lo = at, hi = at;
            while (lo > 0 && sameSideBeyond (res[(size_t) (lo - 1)], half)) --lo;
            while (hi < kPoints - 1 && sameSideBeyond (res[(size_t) (hi + 1)], half)) ++hi;
            return std::max (0.15, (hi - lo) / pointsPerOct);
        };

        auto qOf = [] (double widthOct) { return std::clamp (1.6 / widthOct, 0.3, 12.0); };

        // Subtract a band's TRUE response, so the next pass sees what is
        // really left rather than an idealised shape.
        auto subtract = [&] (std::vector<double>& res, const EqBandParams& b)
        {
            BiquadCoeffs c = Biquad::peaking (b.freqHz, b.q, b.gainDb, sampleRate);
            for (int i = 0; i < kPoints; ++i)
            {
                double mag, ph;
                eqdesign::biquadResponse (c, 2.0 * kPi * hzAt (i) / sampleRate, mag, ph);
                res[(size_t) i] -= 20.0 * std::log10 (std::max (1.0e-9, mag));
            }
        };

        auto makeBand = [&] (double f0, double gain, double q, EqChannel ch,
                             const float* spread, int at)
        {
            EqBandParams b;
            b.on = true;
            b.type = EqType::bell;
            b.freqHz = (float) std::clamp (f0, 25.0, 19000.0);
            b.gainDb = (float) std::clamp (gain, -18.0, 18.0);
            b.q = (float) q;
            b.channel = ch;

            // The same constant-against-occasional decision the single-domain
            // fit makes, per domain.
            if (spread != nullptr && gain < -0.5)
            {
                const double swing = spread[at];
                if (swing > kOccasionalDb)
                {
                    const double share = std::clamp ((swing - kOccasionalDb) / kOccasionalDb,
                                                     0.0, 0.75);
                    const double dynPart = std::abs (b.gainDb) * share;
                    b.gainDb = (float) (b.gainDb + dynPart);
                    b.dynamic = true;
                    b.rangeDb = (float) -dynPart;
                    b.thresholdDb = -12.0f;
                    b.ratio = 4.0f;
                    b.attackMs = 5.0f;
                    b.releaseMs = 120.0f;
                    out.dynamicBands++;
                }
            }
            return b;
        };

        // Does this domain still want a band anywhere near here?
        auto stillNeeds = [&] (const std::vector<double>& res, int at)
        {
            const int span = (int) std::ceil (pointsPerOct);
            for (int i = std::max (0, at - span); i <= std::min (kPoints - 1, at + span); ++i)
                if (std::abs (res[(size_t) i]) >= 0.5) return true;
            return false;
        };

        while ((int) out.bands.size() < maxBands)
        {
            // The biggest remaining error across BOTH domains: one budget,
            // spent where the music hurts most.
            int at = 0;
            double err = 0.0;
            for (int i = 0; i < kPoints; ++i)
            {
                if (std::abs (resMid[(size_t) i]) > std::abs (err))
                { err = resMid[(size_t) i]; at = i; }
                if (std::abs (resSide[(size_t) i]) > std::abs (err))
                { err = resSide[(size_t) i]; at = i; }
            }
            if (std::abs (err) < 0.5) break;

            const double f0 = hzAt (at);
            const double mErr = resMid[(size_t) at];
            const double sErr = resSide[(size_t) at];

            // The common part exists only where the two agree in direction.
            const bool agree = mErr * sErr > 0.0;
            const double common = agree ? (std::abs (mErr) < std::abs (sErr) ? mErr : sErr)
                                        : 0.0;

            bool useStereo = false;
            EqBandParams stereoBand;

            if (agree && std::abs (common) >= 0.5)
            {
                const double wM = widthAt (resMid, at);
                const double wS = widthAt (resSide, at);
                const double widthRatio = std::max (wM, wS) / std::max (0.15, std::min (wM, wS));

                // A stereo band carries one Q. If the two errors are shaped
                // differently that Q is wrong for at least one of them, so
                // the domains keep their own bands whatever the amounts say.
                if (widthRatio < 1.5)
                {
                    stereoBand = makeBand (f0, common, qOf (std::min (wM, wS)),
                                           EqChannel::stereo, spreadMidDb, at);

                    std::vector<double> tryMid = resMid, trySide = resSide;
                    subtract (tryMid, stereoBand);
                    subtract (trySide, stereoBand);

                    const bool needM = stillNeeds (tryMid, at);
                    const bool needS = stillNeeds (trySide, at);

                    // Both or neither: the stereo band is worth its slot.
                    // Exactly one: it would be the second half of a story
                    // nobody can read, so mid and side go their own way.
                    useStereo = (needM == needS);
                }
            }

            if (useStereo)
            {
                subtract (resMid, stereoBand);
                subtract (resSide, stereoBand);
                out.bands.push_back (stereoBand);
                continue;
            }

            // Separate domains. The one that hurts most goes first so the
            // budget is always spent on the loudest problem; its partner
            // comes back around on the next pass if it still needs one.
            const bool midFirst = std::abs (mErr) >= std::abs (sErr);
            const double gain = midFirst ? mErr : sErr;
            if (std::abs (gain) < 0.5) break;

            auto& res = midFirst ? resMid : resSide;
            const auto band = makeBand (f0, gain, qOf (widthAt (res, at)),
                                        midFirst ? EqChannel::mid : EqChannel::side,
                                        midFirst ? spreadMidDb : spreadSideDb, at);
            subtract (res, band);
            out.bands.push_back (band);
        }

        std::vector<double> left (kPoints);
        for (int i = 0; i < kPoints; ++i)
            left[(size_t) i] = 0.5 * (resMid[(size_t) i] + resSide[(size_t) i]);
        out.residualRmsDb = rms (left);
        return out;
    }

};

} // namespace kbs
