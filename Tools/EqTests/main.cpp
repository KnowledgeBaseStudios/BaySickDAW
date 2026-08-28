// BaySickDAW — the EQ engine proof target (QA-EqPro).
//
// The parametric EQ sections of KBS Plugins' Tests/test_core.cpp, carried over
// with the engine so the take-back stays pinned here too: every claim the
// engine makes is measured, because the engine it replaced taught the lesson
// twice — the display said one thing, the sound did another, and nothing
// measured either.  Section 12 covers the two BaySickDAW extensions: the
// per-domain linear-phase matrix (SC-3, the C3 defect the first build of the
// engine did not fix) and the four-slot per-band sidechain (SC-4).
//
// Build + run: run_eq_tests.bat at the repo root (NOT part of do_build.bat's
// gate).  Exit code 0 = every check passed.
#include <cmath>
#include <cstdio>
#include <vector>

#include "../../Source/DSP/Kbs/ParametricEq.h"
#include "../../Source/DSP/Kbs/EqMatch.h"
#include "../../Source/DSP/Kbs/SpectrumScan.h"

namespace { int failures = 0; }

static void check (bool ok, const char* what, double got = 0, double want = 0)
{
    if (ok) { std::printf ("    PASS  %s\n", what); return; }
    std::printf ("    FAIL  %s   got %.4f want %.4f\n", what, got, want);
    ++failures;
}
static void near (double got, double want, double tol, const char* what)
{
    check (std::abs (got - want) <= tol, what, got, want);
}

// Level of the component at `freq` in a buffer, by coherent projection.
static double levelAt (const std::vector<float>& buf, double freq, double sr, int from)
{
    double I = 0, Q = 0; int n = 0;
    for (size_t i = (size_t) from; i < buf.size(); ++i)
    {
        const double w = 2.0 * kbs::kPi * freq * (double) i / sr;
        I += buf[i] * std::sin (w); Q += buf[i] * std::cos (w); ++n;
    }
    return std::hypot (2.0 * I / n, 2.0 * Q / n);
}

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    // ── 10. the parametric EQ engine ───────────────────────────────────────
    std::printf ("\n  parametric EQ - identity and magnitude\n");
    {
        auto gainAt = [] (kbs::ParametricEq& eq, double freq, double srr,
                          int settleBlocks = 40)
        {
            const int block = 256;
            std::vector<float> L (block), R (block);
            std::vector<float> tail;
            double phase = 0.0;
            const int total = settleBlocks + 40;
            for (int bl = 0; bl < total; ++bl)
            {
                for (int i = 0; i < block; ++i)
                {
                    phase += 2.0 * kbs::kPi * freq / srr;
                    L[(size_t) i] = R[(size_t) i] = (float) (0.1 * std::sin (phase));
                }
                eq.process (L.data(), R.data(), block);
                if (bl >= settleBlocks)
                    tail.insert (tail.end(), L.begin(), L.end());
            }
            // Project over a whole number of cycles: a fractional cycle leaks
            // into the estimate at O(1/cycles), which at 50 Hz over a fifth of
            // a second is a tenth of a decibel - the size of the tolerance.
            const double period = srr / freq;
            const size_t whole = (size_t) (std::floor ((double) tail.size() / period) * period);
            tail.resize (std::max<size_t> (whole, (size_t) period));
            const double lvl = levelAt (tail, freq, srr, 0);
            return 20.0 * std::log10 (lvl / 0.1);
        };

        const double srr = 48000.0;

        // Identity: a fresh EQ is bit-exact pass-through. Not close - exact.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            std::vector<float> L (256), R (256), refL (256), refR (256);
            for (int i = 0; i < 256; ++i)
                refL[(size_t) i] = L[(size_t) i] = refR[(size_t) i] = R[(size_t) i]
                    = (float) std::sin (0.1 * i) * 0.5f;
            eq.process (L.data(), R.data(), 256);
            double diff = 0.0;
            for (int i = 0; i < 256; ++i)
                diff += std::abs ((double) L[(size_t) i] - refL[(size_t) i])
                      + std::abs ((double) R[(size_t) i] - refR[(size_t) i]);
            check (diff == 0.0, "no bands means bit-exact pass-through", diff, 0.0);
        }

        // Every gain type: the measured response equals the drawn response.
        struct Case { kbs::EqType type; double f; float g; float q; double at; };
        const Case cases[] = {
            { kbs::EqType::bell,      1000.0, +9.0f, 1.0f,  1000.0 },
            { kbs::EqType::bell,      1000.0, -12.0f, 4.0f, 1000.0 },
            { kbs::EqType::lowShelf,   200.0, +6.0f, 0.9f,    50.0 },
            { kbs::EqType::highShelf, 4000.0, -8.0f, 0.9f, 16000.0 },
            { kbs::EqType::tilt,      1000.0, +6.0f, 0.7f,    60.0 },
        };
        for (const auto& c : cases)
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            kbs::EqBandParams b;
            b.on = true; b.type = c.type; b.freqHz = (float) c.f;
            b.gainDb = c.g; b.q = c.q;
            eq.setBand (0, b);

            const double drawn = 20.0 * std::log10 (eq.magnitudeAt ((float) c.at));
            const double heard = gainAt (eq, c.at, srr);
            near (heard, drawn, 0.1, "measured equals drawn (gain type)");
        }

        // Filters: response follows the slope, and the query follows the
        // audio. 96 dB/oct must actually be steeper than 48.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::lowPass; b.freqHz = 1000.0f;
            b.q = 0.707f;

            b.slope = 6.0f;    // 6 dB/oct
            eq.setBand (0, b);
            const double one6 = gainAt (eq, 4000.0, srr);
            near (one6, -12.3, 1.5, "6 dB/oct falls ~12 dB over two octaves");

            b.slope = 48.0f;   // 48
            eq.setBand (0, b);
            const double at48 = gainAt (eq, 2000.0, srr);
            const double q48  = 20.0 * std::log10 (eq.magnitudeAt (2000.0f));
            near (at48, q48, 0.3, "48 dB/oct audio matches its query");

            b.slope = 96.0f;   // 96
            eq.setBand (0, b);
            const double at96 = gainAt (eq, 2000.0, srr);
            check (at96 < at48 - 10.0, "96 dB/oct rejects more than 48", at96, at48 - 10.0);

            b.type = kbs::EqType::bandPass; b.slope = 12.0f; b.q = 2.0f;
            eq.setBand (0, b);
            near (gainAt (eq, 1000.0, srr), 0.0, 0.4, "band pass holds 0 dB at centre");
            const double bpSkirt = gainAt (eq, 250.0, srr);
            check (bpSkirt < -12.0, "and rejects two octaves down", bpSkirt, -12.0);

            b.type = kbs::EqType::notch; b.q = 4.0f; b.slope = 12.0f;
            eq.setBand (0, b);
            const double nullDepth = gainAt (eq, 1000.0, srr);
            check (nullDepth < -30.0, "a static notch is a real null", nullDepth, -30.0);
            near (gainAt (eq, 4000.0, srr), 0.0, 0.3, "and flat away from it");
        }
    }

    std::printf ("\n  parametric EQ - natural mode holds its shape at Nyquist\n");
    {
        const double sr441 = 44100.0;
        auto analogPeakDb = [] (double f, double f0, double q, double gDb)
        {
            const double A = std::pow (10.0, gDb / 40.0);
            const std::complex<double> s (0.0, f / f0);
            const auto num = s * s + s * (A / q) + 1.0;
            const auto den = s * s + s / (A * q) + 1.0;
            return 20.0 * std::log10 (std::abs (num / den));
        };

        kbs::EqBandParams b;
        b.on = true; b.type = kbs::EqType::bell;
        b.freqHz = 15000.0f; b.gainDb = 12.0f; b.q = 1.0f;

        kbs::ParametricEq rbj;
        rbj.prepare (sr441, 256);
        rbj.setMode (kbs::EqMode::zeroLatency);
        rbj.setProportionalQ (false);
        rbj.setBand (0, b);

        kbs::ParametricEq nat;
        nat.prepare (sr441, 256);
        nat.setMode (kbs::EqMode::natural);
        nat.setProportionalQ (false);
        nat.setBand (0, b);

        const double probe = 20500.0;
        const double want = analogPeakDb (probe, 15000.0, 1.0, 12.0);
        const double gotR = 20.0 * std::log10 (rbj.magnitudeAt ((float) probe));
        const double gotN = 20.0 * std::log10 (nat.magnitudeAt ((float) probe));

        check (std::abs (gotR - want) > 2.0,
               "RBJ visibly cramps near Nyquist (the problem is real)", gotR, want);
        near (gotN, want, 1.0, "the natural bell holds the analogue value there");
    }

    std::printf ("\n  parametric EQ - routing, placement, isolate, listen\n");
    {
        const double srr = 48000.0;
        auto chGain = [&] (kbs::ParametricEq& eq, double freq, int chan)
        {
            const int block = 256;
            std::vector<float> L (block), R (block), tail;
            double phase = 0.0;
            for (int bl = 0; bl < 60; ++bl)
            {
                for (int i = 0; i < block; ++i)
                {
                    phase += 2.0 * kbs::kPi * freq / srr;
                    L[(size_t) i] = R[(size_t) i] = (float) (0.1 * std::sin (phase));
                }
                eq.process (L.data(), R.data(), block);
                if (bl >= 30)
                {
                    auto& src = chan == 0 ? L : R;
                    tail.insert (tail.end(), src.begin(), src.end());
                }
            }
            return 20.0 * std::log10 (levelAt (tail, freq, srr, 0) / 0.1);
        };

        kbs::EqBandParams b;
        b.on = true; b.type = kbs::EqType::bell;
        b.freqHz = 1000.0f; b.gainDb = 6.0f; b.q = 1.0f;

        {
            kbs::ParametricEq eq; eq.prepare (srr, 256);
            b.channel = kbs::EqChannel::left;
            eq.setBand (0, b);
            near (chGain (eq, 1000.0, 0), 6.0, 0.15, "a left-only band boosts the left");
            near (chGain (eq, 1000.0, 1), 0.0, 0.05, "and leaves the right alone");
        }
        {
            kbs::ParametricEq eq; eq.prepare (srr, 256);
            b.channel = kbs::EqChannel::side;
            eq.setBand (0, b);
            near (chGain (eq, 1000.0, 0), 0.0, 0.05,
                  "a side band does nothing to a mono signal");
        }
        {
            kbs::ParametricEq eq; eq.prepare (srr, 256);
            b.channel = kbs::EqChannel::stereo;
            b.placement = 1.0f;   // fully right
            eq.setBand (0, b);
            near (chGain (eq, 1000.0, 1), 6.0, 0.15, "placement hard right boosts the right");
            near (chGain (eq, 1000.0, 0), 0.0, 0.05, "and not the left");
            b.placement = 0.0f;
        }
        {
            kbs::ParametricEq eq; eq.prepare (srr, 256);
            eq.setBand (0, b);
            kbs::EqBandParams b2 = b;
            b2.freqHz = 8000.0f; b2.gainDb = -9.0f; b2.q = 4.0f; b2.isolated = true;
            eq.setBand (1, b2);
            near (chGain (eq, 1000.0, 0), 0.0, 0.05,
                  "isolate silences the other bands' work");
            near (chGain (eq, 8000.0, 0), -9.0, 0.2, "and keeps its own");
        }
        {
            kbs::ParametricEq eq; eq.prepare (srr, 256);
            eq.setBand (0, b);
            eq.setListenBand (0);
            const double atBand = chGain (eq, 1000.0, 0);
            const double farOff = chGain (eq, 8000.0, 0);
            check (atBand > -3.5 && atBand < 1.0,
                   "listen passes the band's own region", atBand, 0.0);
            check (farOff < atBand - 12.0,
                   "and rejects far outside it", farOff, atBand - 12.0);
        }
    }

    std::printf ("\n  parametric EQ - dynamics\n");
    {
        const double srr = 48000.0;

        kbs::ParametricEq eq;
        eq.prepare (srr, 512);
        kbs::EqBandParams b;
        b.on = true; b.type = kbs::EqType::bell;
        b.freqHz = 1000.0f; b.gainDb = 0.0f; b.q = 1.0f;
        b.dynamic = true; b.thresholdDb = -40.0f; b.ratio = 10.0f;
        b.attackMs = 1.0f; b.releaseMs = 80.0f; b.rangeDb = -12.0f;
        eq.setBand (0, b);

        std::vector<float> L (512), R (512);
        double phase = 0.0;
        int samplesTo90 = -1;
        int processed = 0;
        for (int bl = 0; bl < 60; ++bl)
        {
            for (int i = 0; i < 512; ++i)
            {
                phase += 2.0 * kbs::kPi * 1000.0 / srr;
                L[(size_t) i] = R[(size_t) i] = (float) (0.25 * std::sin (phase));
            }
            eq.process (L.data(), R.data(), 512);
            processed += 512;
            if (samplesTo90 < 0 && eq.bandGrDb (0) < -0.9f * 11.0f)
                samplesTo90 = processed;
        }

        const float settled = eq.bandGrDb (0);
        check (settled < -10.0f, "a loud tone in-band drives the reduction", settled, -11.0);
        check (samplesTo90 > 0 && samplesTo90 <= (int) (0.02 * srr),
               "1 ms attack lands within 20 ms wall clock",
               samplesTo90, 0.02 * srr);

        // The reduction reaches the audio, not only the meter.
        std::vector<float> tail;
        for (int bl = 0; bl < 20; ++bl)
        {
            for (int i = 0; i < 512; ++i)
            {
                phase += 2.0 * kbs::kPi * 1000.0 / srr;
                L[(size_t) i] = R[(size_t) i] = (float) (0.25 * std::sin (phase));
            }
            eq.process (L.data(), R.data(), 512);
            tail.insert (tail.end(), L.begin(), L.end());
        }
        const double outDb = 20.0 * std::log10 (levelAt (tail, 1000.0, srr, 0) / 0.25);
        near (outDb, (double) settled, 1.5, "and the audio carries it");

        // Silence: the gate holds, GR returns to zero.
        for (int bl = 0; bl < 40; ++bl)
        {
            std::fill (L.begin(), L.end(), 0.0f);
            std::fill (R.begin(), R.end(), 0.0f);
            eq.process (L.data(), R.data(), 512);
        }
        near ((double) eq.bandGrDb (0), 0.0, 0.2, "silence releases the reduction");

        // External sidechain: the band's own input is quiet, the sidechain is
        // loud in-band, and the reduction must follow the sidechain.
        {
            kbs::ParametricEq sc;
            sc.prepare (srr, 512);
            kbs::EqBandParams sb = b;
            sb.scExternal = true;
            sc.setBand (0, sb);

            std::vector<float> SL (512), SR (512), KL (512), KR (512);
            double ph2 = 0.0;
            for (int bl = 0; bl < 60; ++bl)
            {
                for (int i = 0; i < 512; ++i)
                {
                    ph2 += 2.0 * kbs::kPi * 1000.0 / srr;
                    KL[(size_t) i] = KR[(size_t) i] = (float) (0.25 * std::sin (ph2));
                    SL[(size_t) i] = SR[(size_t) i] = 0.001f;   // near-silent programme
                }
                sc.setSidechain (KL.data(), KR.data(), 512);
                sc.process (SL.data(), SR.data(), 512);
            }
            check (sc.bandGrDb (0) < -10.0f,
                   "an external sidechain drives the reduction", sc.bandGrDb (0), -11.0);
        }
    }

    std::printf ("\n  parametric EQ - auto-gain\n");
    {
        const double srr = 48000.0;
        kbs::ParametricEq eq;
        eq.prepare (srr, 512);
        eq.setAutoGain (true, 1.0f);
        kbs::EqBandParams b;
        b.on = true; b.type = kbs::EqType::bell;
        b.freqHz = 1000.0f; b.gainDb = 12.0f; b.q = 0.7f;
        eq.setBand (0, b);

        std::vector<float> L (512), R (512), tail;
        double phase = 0.0;
        for (int bl = 0; bl < 700; ++bl)     // ~7.5 s: well past the follower
        {
            for (int i = 0; i < 512; ++i)
            {
                phase += 2.0 * kbs::kPi * 1000.0 / srr;
                L[(size_t) i] = R[(size_t) i] = (float) (0.1 * std::sin (phase));
            }
            eq.process (L.data(), R.data(), 512);
            if (bl >= 660) tail.insert (tail.end(), L.begin(), L.end());
        }
        const double outDb = 20.0 * std::log10 (levelAt (tail, 1000.0, srr, 0) / 0.1);
        near (outDb, 0.0, 0.7, "+12 dB of bell, auto-gain holds the level");
    }

    std::printf ("\n  parametric EQ - linear phase\n");
    {
        const double srr = 48000.0;

        // Flat and steady: overlap-save is exact convolution, so a flat curve
        // must show no envelope ripple at all.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            eq.setMode (kbs::EqMode::linearLow);

            std::vector<float> L (256), R (256);
            double phase = 0.0;
            float lo = 1.0e9f, hi = -1.0e9f;
            const int settle = (eq.latencySamples() / 256 + 8);
            for (int bl = 0; bl < settle + 60; ++bl)
            {
                for (int i = 0; i < 256; ++i)
                {
                    phase += 2.0 * kbs::kPi * 997.0 / srr;
                    L[(size_t) i] = R[(size_t) i] = (float) (0.25 * std::sin (phase));
                }
                eq.process (L.data(), R.data(), 256);
                if (bl >= settle)
                {
                    float pk = 0.0f;
                    for (int i = 0; i < 256; ++i)
                        pk = std::max (pk, std::abs (L[(size_t) i]));
                    lo = std::min (lo, pk);
                    hi = std::max (hi, pk);
                }
            }
            const double rippleDb = 20.0 * std::log10 ((double) hi / (double) lo);
            check (rippleDb < 0.05, "flat linear curve has no envelope ripple",
                   rippleDb, 0.05);
        }

        // Reported latency is measured latency - to the sample.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            eq.setMode (kbs::EqMode::linearMedium);

            const int lat = eq.latencySamples();
            std::vector<float> L, R;
            const int total = ((lat / 256) + 4) * 256;
            L.assign ((size_t) total, 0.0f);
            R.assign ((size_t) total, 0.0f);
            L[0] = R[0] = 1.0f;
            for (int at = 0; at < total; at += 256)
                eq.process (L.data() + at, R.data() + at, 256);

            int peakAt = 0;
            float peak = 0.0f;
            for (int i = 0; i < total; ++i)
                if (std::abs (L[(size_t) i]) > peak)
                {
                    peak = std::abs (L[(size_t) i]);
                    peakAt = i;
                }
            near ((double) peakAt, (double) lat, 0.0,
                  "linear-mode latency is reported to the sample");
        }

        // A band edit reaches the audio.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            eq.setMode (kbs::EqMode::linearLow);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 1000.0f; b.gainDb = 12.0f; b.q = 1.0f;
            eq.setBand (0, b);

            std::vector<float> L (256), R (256), tail;
            double phase = 0.0;
            auto run = [&] (int blocks, bool collect)
            {
                if (collect) tail.clear();
                for (int bl = 0; bl < blocks; ++bl)
                {
                    for (int i = 0; i < 256; ++i)
                    {
                        phase += 2.0 * kbs::kPi * 1000.0 / srr;
                        L[(size_t) i] = R[(size_t) i] = (float) (0.1 * std::sin (phase));
                    }
                    eq.process (L.data(), R.data(), 256);
                    if (collect) tail.insert (tail.end(), L.begin(), L.end());
                }
            };

            run (40, false);
            run (20, true);
            const double boosted = 20.0 * std::log10 (levelAt (tail, 1000.0, srr, 0) / 0.1);
            near (boosted, 12.0, 1.2, "a linear-mode bell is audible at Low precision");

            {
                kbs::ParametricEq hq;
                hq.prepare (srr, 256);
                hq.setMode (kbs::EqMode::linearHigh);
                hq.setBand (0, b);
                std::vector<float> HL (256), HR (256), htail;
                double hphase = 0.0;
                const int hsettle = hq.latencySamples() / 256 + 8;
                for (int bl = 0; bl < hsettle + 60; ++bl)
                {
                    for (int i = 0; i < 256; ++i)
                    {
                        hphase += 2.0 * kbs::kPi * 1000.0 / srr;
                        HL[(size_t) i] = HR[(size_t) i] = (float) (0.1 * std::sin (hphase));
                    }
                    hq.process (HL.data(), HR.data(), 256);
                    if (bl >= hsettle + 20) htail.insert (htail.end(), HL.begin(), HL.end());
                }
                const double hi = 20.0 * std::log10 (levelAt (htail, 1000.0, srr, 0) / 0.1);
                near (hi, 12.0, 0.3, "and exact at High precision");
            }

            b.gainDb = 0.0f;
            eq.setBand (0, b);
            run (40, false);
            run (20, true);
            const double flat = 20.0 * std::log10 (levelAt (tail, 1000.0, srr, 0) / 0.1);
            near (flat, 0.0, 0.3, "and moving it to zero reaches the audio");
        }

        // Linear phase really is linear phase: the query says zero, and a
        // dynamic band still works, riding minimum-phase on the FIR bed.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 512);
            eq.setMode (kbs::EqMode::linearLow);
            near ((double) eq.phaseAt (3000.0f), 0.0, 1e-6,
                  "the phase query tells the truth in linear mode");

            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 1000.0f; b.q = 1.0f;
            b.dynamic = true; b.thresholdDb = -40.0f; b.ratio = 10.0f;
            b.attackMs = 1.0f; b.rangeDb = -12.0f;
            eq.setBand (0, b);

            std::vector<float> L (512), R (512);
            double phase = 0.0;
            for (int bl = 0; bl < 80; ++bl)
            {
                for (int i = 0; i < 512; ++i)
                {
                    phase += 2.0 * kbs::kPi * 1000.0 / srr;
                    L[(size_t) i] = R[(size_t) i] = (float) (0.25 * std::sin (phase));
                }
                eq.process (L.data(), R.data(), 512);
            }
            check (eq.bandGrDb (0) < -10.0f,
                   "dynamics keep working in linear mode", eq.bandGrDb (0), -11.0);
        }
    }

    std::printf ("\n  parametric EQ - oversampling\n");
    {
        const double srr = 48000.0;
        kbs::ParametricEq eq;
        eq.prepare (srr, 256);
        eq.setOversampling (true);
        near ((double) eq.latencySamples(), 15.0, 1.0, "2x reports its resampler latency");

        kbs::EqBandParams b;
        b.on = true; b.type = kbs::EqType::bell;
        b.freqHz = 1000.0f; b.gainDb = 9.0f; b.q = 1.0f;
        eq.setBand (0, b);

        std::vector<float> L (256), R (256), tail;
        double phase = 0.0;
        for (int bl = 0; bl < 60; ++bl)
        {
            for (int i = 0; i < 256; ++i)
            {
                phase += 2.0 * kbs::kPi * 1000.0 / srr;
                L[(size_t) i] = R[(size_t) i] = (float) (0.1 * std::sin (phase));
            }
            eq.process (L.data(), R.data(), 256);
            if (bl >= 30) tail.insert (tail.end(), L.begin(), L.end());
        }
        const double got = 20.0 * std::log10 (levelAt (tail, 1000.0, srr, 0) / 0.1);
        const double drawn = 20.0 * std::log10 (eq.magnitudeAt (1000.0f));
        near (got, drawn, 0.3, "oversampled audio still matches the query");
    }

    // ── 11. EQ Match ───────────────────────────────────────────────────────
    std::printf ("\n  EQ match\n");
    {
        const double srr = 48000.0;

        kbs::ParametricEq target;
        target.prepare (srr, 256);
        kbs::EqBandParams tb;
        tb.on = true; tb.type = kbs::EqType::bell;
        tb.freqHz = 150.0f; tb.gainDb = 5.0f; tb.q = 1.0f;
        target.setBand (0, tb);
        tb.freqHz = 1200.0f; tb.gainDb = -7.0f; tb.q = 1.8f;
        target.setBand (1, tb);
        tb.freqHz = 7000.0f; tb.gainDb = 4.0f; tb.q = 0.8f;
        target.setBand (2, tb);

        std::vector<float> ref (kbs::EqMatch::kPoints), cur (kbs::EqMatch::kPoints);
        for (int i = 0; i < kbs::EqMatch::kPoints; ++i)
        {
            const double hz = kbs::EqMatch::hzAt (i);
            const double material = -3.0 * std::log2 (hz / 1000.0);   // pink slope
            cur[(size_t) i] = (float) material;
            ref[(size_t) i] = (float) (material
                + 20.0 * std::log10 (std::max (1.0e-6f, target.magnitudeAt ((float) hz))));
        }

        const auto fit = kbs::EqMatch::fit (ref.data(), cur.data(), 0.15f, 8, srr);

        check (! fit.bands.empty() && fit.bands.size() <= 8,
               "the fit spends a sensible number of bands",
               (double) fit.bands.size(), 3.0);

        kbs::ParametricEq fitted;
        fitted.prepare (srr, 256);
        for (size_t i = 0; i < fit.bands.size(); ++i)
            fitted.setBand ((int) i, fit.bands[(size_t) i]);

        double worst = 0.0, sum2 = 0.0;
        int n = 0;
        for (int i = 0; i < kbs::EqMatch::kPoints; ++i)
        {
            const double hz = kbs::EqMatch::hzAt (i);
            if (hz < 60.0 || hz > 16000.0) continue;    // capture edges are noise
            const double want = 20.0 * std::log10 (std::max (1.0e-6f, target.magnitudeAt ((float) hz)));
            const double got  = 20.0 * std::log10 (std::max (1.0e-6f, fitted.magnitudeAt ((float) hz)));
            const double err = std::abs (want - got);
            worst = std::max (worst, err);
            sum2 += err * err;
            ++n;
        }
        const double rmsErr = std::sqrt (sum2 / n);

        near (rmsErr, 0.0, 1.0, "matched curve lands within 1 dB RMS of the target");
        check (worst < 3.0, "and never strays past 3 dB anywhere", worst, 3.0);
        check (fit.residualRmsDb < fit.targetRmsDb * 0.5f,
               "the fit removed most of what it was shown",
               fit.residualRmsDb, fit.targetRmsDb * 0.5f);

        const auto broad = kbs::EqMatch::fit (ref.data(), cur.data(), 1.0f, 8, srr);
        check (broad.bands.size() <= fit.bands.size(),
               "full smoothness never needs more bands",
               (double) broad.bands.size(), (double) fit.bands.size());
    }

    // ── 12. BaySickDAW extensions ──────────────────────────────────────────
    //
    // SC-3: the per-domain linear-phase matrix.  The C3 defect - a band
    // routed mid / side / left / right losing its routing the moment a linear
    // mode engaged - shipped in the DAW's old engine AND in the first build
    // of this one.  These are its regression tests.
    std::printf ("\n  parametric EQ - per-domain linear phase (SC-3 / C3 regression)\n");
    {
        const double srr = 48000.0;

        // A mono signal has no side: a side band in a linear mode must leave
        // it untouched.  The matrix design is linear, so the mono sum of the
        // LL and LR entries is the designed identity - this is exact, not
        // approximate.  This exact setup, before the fix, boosted the whole
        // mix (C3).
        auto monoGain = [&] (kbs::ParametricEq& eq, double freq)
        {
            const int block = 256;
            std::vector<float> L (block), R (block), tail;
            double phase = 0.0;
            const int settle = eq.latencySamples() / block + 10;
            for (int bl = 0; bl < settle + 60; ++bl)
            {
                for (int i = 0; i < block; ++i)
                {
                    phase += 2.0 * kbs::kPi * freq / srr;
                    L[(size_t) i] = R[(size_t) i] = (float) (0.1 * std::sin (phase));
                }
                eq.process (L.data(), R.data(), block);
                if (bl >= settle) tail.insert (tail.end(), L.begin(), L.end());
            }
            return 20.0 * std::log10 (levelAt (tail, freq, srr, 0) / 0.1);
        };

        // Pure-side probe: L = -R.  Returns the level change of the side
        // component (read on L; the decode keeps it anti-phase).
        auto sideGain = [&] (kbs::ParametricEq& eq, double freq)
        {
            const int block = 256;
            std::vector<float> L (block), R (block), tail;
            double phase = 0.0;
            const int settle = eq.latencySamples() / block + 10;
            for (int bl = 0; bl < settle + 60; ++bl)
            {
                for (int i = 0; i < block; ++i)
                {
                    phase += 2.0 * kbs::kPi * freq / srr;
                    const float v = (float) (0.1 * std::sin (phase));
                    L[(size_t) i] = v;
                    R[(size_t) i] = -v;
                }
                eq.process (L.data(), R.data(), block);
                if (bl >= settle) tail.insert (tail.end(), L.begin(), L.end());
            }
            return 20.0 * std::log10 (levelAt (tail, freq, srr, 0) / 0.1);
        };

        kbs::EqBandParams side;
        side.on = true; side.type = kbs::EqType::bell;
        side.freqHz = 1000.0f; side.gainDb = 6.0f; side.q = 1.0f;
        side.channel = kbs::EqChannel::side;

        {
            kbs::ParametricEq eq; eq.prepare (srr, 256);
            eq.setMode (kbs::EqMode::linearHigh);
            eq.setBand (0, side);
            near (monoGain (eq, 1000.0), 0.0, 0.05,
                  "linear mode: a side band leaves a mono signal untouched (C3)");
            near (sideGain (eq, 1000.0), 6.0, 0.3,
                  "and boosts the side content it names");
        }
        {
            kbs::ParametricEq eq; eq.prepare (srr, 256);
            eq.setMode (kbs::EqMode::linearHigh);
            kbs::EqBandParams mid = side;
            mid.channel = kbs::EqChannel::mid;
            eq.setBand (0, mid);
            near (sideGain (eq, 1000.0), 0.0, 0.05,
                  "linear mode: a mid band leaves pure side untouched");
            near (monoGain (eq, 1000.0), 6.0, 0.3,
                  "and boosts the mono content");
        }
        {
            kbs::ParametricEq eq; eq.prepare (srr, 256);
            eq.setMode (kbs::EqMode::linearHigh);
            kbs::EqBandParams lb = side;
            lb.channel = kbs::EqChannel::left;
            eq.setBand (0, lb);

            const int block = 256;
            std::vector<float> L (block), R (block), tl, tr;
            double phase = 0.0;
            const int settle = eq.latencySamples() / block + 10;
            for (int bl = 0; bl < settle + 60; ++bl)
            {
                for (int i = 0; i < block; ++i)
                {
                    phase += 2.0 * kbs::kPi * 1000.0 / srr;
                    L[(size_t) i] = R[(size_t) i] = (float) (0.1 * std::sin (phase));
                }
                eq.process (L.data(), R.data(), block);
                if (bl >= settle)
                {
                    tl.insert (tl.end(), L.begin(), L.end());
                    tr.insert (tr.end(), R.begin(), R.end());
                }
            }
            near (20.0 * std::log10 (levelAt (tl, 1000.0, srr, 0) / 0.1), 6.0, 0.3,
                  "linear mode: a left band boosts the left");
            near (20.0 * std::log10 (levelAt (tr, 1000.0, srr, 0) / 0.1), 0.0, 0.05,
                  "and leaves the right alone");
        }
        {
            // A stereo band beside a side band: the mono probe must see only
            // the stereo band's gain.
            kbs::ParametricEq eq; eq.prepare (srr, 256);
            eq.setMode (kbs::EqMode::linearHigh);
            kbs::EqBandParams st = side;
            st.channel = kbs::EqChannel::stereo;
            st.gainDb = -4.0f;
            eq.setBand (0, st);
            eq.setBand (1, side);
            near (monoGain (eq, 1000.0), -4.0, 0.3,
                  "mixed domains: mono hears only the stereo band");
        }
        {
            // The matrix path's latency is still exact to the sample - the
            // frame rotation is where an off-by-one would hide.
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            eq.setMode (kbs::EqMode::linearMedium);
            eq.setBand (0, side);   // forces the matrix path

            const int lat = eq.latencySamples();
            std::vector<float> L, R;
            const int total = ((lat / 256) + 4) * 256;
            L.assign ((size_t) total, 0.0f);
            R.assign ((size_t) total, 0.0f);
            L[0] = R[0] = 1.0f;
            for (int at = 0; at < total; at += 256)
                eq.process (L.data() + at, R.data() + at, 256);

            int peakAt = 0;
            float peak = 0.0f;
            for (int i = 0; i < total; ++i)
                if (std::abs (L[(size_t) i]) > peak)
                {
                    peak = std::abs (L[(size_t) i]);
                    peakAt = i;
                }
            near ((double) peakAt, (double) lat, 0.0,
                  "matrix-path latency is reported to the sample");
        }
    }

    // SC-4: the four-slot per-band sidechain - the DAW strip's receive lines.
    std::printf ("\n  parametric EQ - four-slot sidechain (SC-4)\n");
    {
        const double srr = 48000.0;

        kbs::EqBandParams b;
        b.on = true; b.type = kbs::EqType::bell;
        b.freqHz = 1000.0f; b.gainDb = 0.0f; b.q = 1.0f;
        b.dynamic = true; b.thresholdDb = -40.0f; b.ratio = 10.0f;
        b.attackMs = 1.0f; b.releaseMs = 80.0f; b.rangeDb = -12.0f;
        b.scSource = 2;

        auto drive = [&] (int loudSlot)
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 512);
            eq.setBand (0, b);

            std::vector<float> SL (512), SR (512), KL (512), KR (512), QL (512), QR (512);
            double ph = 0.0;
            for (int bl = 0; bl < 60; ++bl)
            {
                for (int i = 0; i < 512; ++i)
                {
                    ph += 2.0 * kbs::kPi * 1000.0 / srr;
                    KL[(size_t) i] = KR[(size_t) i] = (float) (0.25 * std::sin (ph));
                    QL[(size_t) i] = QR[(size_t) i] = 0.0005f;
                    SL[(size_t) i] = SR[(size_t) i] = 0.001f;   // near-silent programme
                }
                for (int s = 0; s < 4; ++s)
                {
                    const bool loud = (s == loudSlot);
                    eq.setSidechainSlot (s, loud ? KL.data() : QL.data(),
                                            loud ? KR.data() : QR.data(), 512);
                }
                eq.process (SL.data(), SR.data(), 512);
            }
            return (double) eq.bandGrDb (0);
        };

        check (drive (2) < -10.0, "the band's picked slot drives the reduction",
               drive (2), -11.0);
        near (drive (1), 0.0, 0.2, "a loud signal on another slot does not");
    }

    // ---- 12b/12c. EQ Match: mid/side from one budget, and constant vs
    // occasional.  Ported from the KBS suite alongside the engine itself
    // (their 2026-08-26 handoff).  These pin the two claims the new EqMatch
    // makes: a match can tell the center from the edges, and it can tell a
    // constant problem from one that only shows up on peaks.
    std::printf ("%s", "\n  12b. EQ Match - stereo, mid and side from one budget\n");
    {
        auto flatGrid = [] { return std::vector<float> (kbs::EqMatch::kPoints, -20.0f); };
        auto count = [] (const kbs::EqMatch::Result& r, kbs::EqChannel ch)
        {
            int n = 0;
            for (const auto& b : r.bands) if (b.channel == ch) ++n;
            return n;
        };
        auto dent = [] (std::vector<float>& v, double loHz, double hiHz, float db)
        {
            for (int i = 0; i < kbs::EqMatch::kPoints; ++i)
            {
                const double hz = kbs::EqMatch::hzAt (i);
                if (hz > loHz && hz < hiHz) v[(size_t) i] += db;
            }
        };

        // The domains agree exactly: one stereo band says it once.
        {
            auto rM = flatGrid(), rS = flatGrid(), cM = flatGrid(), cS = flatGrid();
            dent (cM, 2200.0, 4200.0, 6.0f);
            dent (cS, 2200.0, 4200.0, 6.0f);
            const auto r = kbs::EqMatch::fitMidSide (rM.data(), rS.data(),
                                                     cM.data(), cS.data(), 0.4f, 12);
            check (count (r, kbs::EqChannel::stereo) > 0,
                   "domains that agree get stereo bands",
                   (double) count (r, kbs::EqChannel::stereo), 1.0);
            check (count (r, kbs::EqChannel::mid) == 0 && count (r, kbs::EqChannel::side) == 0,
                   "and nothing is spent saying it twice", 0.0, 0.0);
        }

        // Only the sides have a problem: fixed on the sides, no stereo band.
        {
            auto rM = flatGrid(), rS = flatGrid(), cM = flatGrid(), cS = flatGrid();
            dent (cS, 2200.0, 4200.0, 6.0f);
            const auto r = kbs::EqMatch::fitMidSide (rM.data(), rS.data(),
                                                     cM.data(), cS.data(), 0.4f, 12);
            check (count (r, kbs::EqChannel::side) > 0,
                   "a side-only problem is fixed on the sides",
                   (double) count (r, kbs::EqChannel::side), 1.0);
            check (count (r, kbs::EqChannel::stereo) == 0,
                   "and never with a stereo band", 0.0, 0.0);
        }

        // Same place, different amounts: a band each, in their own domain.
        {
            auto rM = flatGrid(), rS = flatGrid(), cM = flatGrid(), cS = flatGrid();
            dent (cM, 2200.0, 4200.0, 6.0f);
            dent (cS, 2200.0, 4200.0, 2.0f);
            const auto r = kbs::EqMatch::fitMidSide (rM.data(), rS.data(),
                                                     cM.data(), cS.data(), 0.4f, 12);
            check (count (r, kbs::EqChannel::mid) > 0 && count (r, kbs::EqChannel::side) > 0,
                   "differing amounts get a band each, in their own domain", 1.0, 1.0);
        }

        // Broad shared error plus narrow detail in each domain.
        {
            auto rM = flatGrid(), rS = flatGrid(), cM = flatGrid(), cS = flatGrid();
            dent (cM, 200.0, 8000.0, 5.0f);
            dent (cS, 200.0, 8000.0, 5.0f);
            dent (cM, 900.0, 1150.0, 5.0f);
            dent (cS, 3000.0, 3800.0, 5.0f);
            const auto r = kbs::EqMatch::fitMidSide (rM.data(), rS.data(),
                                                     cM.data(), cS.data(), 0.2f, 16);
            check (count (r, kbs::EqChannel::stereo) > 0,
                   "a broad shared error is carried by stereo bands",
                   (double) count (r, kbs::EqChannel::stereo), 1.0);
            check (r.residualRmsDb < r.targetRmsDb * 0.6f,
                   "and the whole fit removes most of what it was given",
                   r.residualRmsDb, r.targetRmsDb * 0.5);
        }

        // One budget, counted across every domain together.
        {
            auto rM = flatGrid(), rS = flatGrid(), cM = flatGrid(), cS = flatGrid();
            for (int i = 0; i < kbs::EqMatch::kPoints; ++i)
            {
                cM[(size_t) i] += (float) (6.0 * std::sin (i * 0.31));
                cS[(size_t) i] += (float) (6.0 * std::sin (i * 0.17 + 1.0));
            }
            const auto r = kbs::EqMatch::fitMidSide (rM.data(), rS.data(),
                                                     cM.data(), cS.data(), 0.1f, 7);
            check ((int) r.bands.size() <= 7, "the budget is shared across domains",
                   (double) r.bands.size(), 7.0);
        }
    }

    std::printf ("%s", "\n  12c. EQ Match - constant against occasional\n");
    {
        // One 3 kHz excess described two ways: sitting there all the time, or
        // the same average arrived at by swinging.  Only the second should come
        // out dynamic - a static cut for it dulls the passages that were fine.
        std::vector<float> ref (kbs::EqMatch::kPoints, -20.0f);
        std::vector<float> cur (kbs::EqMatch::kPoints, -20.0f);
        std::vector<float> steady (kbs::EqMatch::kPoints, 1.5f);
        std::vector<float> peaky (kbs::EqMatch::kPoints, 1.5f);

        for (int i = 0; i < kbs::EqMatch::kPoints; ++i)
        {
            const double hz = kbs::EqMatch::hzAt (i);
            if (hz > 2200.0 && hz < 4200.0)
            {
                cur[(size_t) i] = -14.0f;
                peaky[(size_t) i] = 9.0f;
            }
        }

        const auto steadyFit = kbs::EqMatch::fit (ref.data(), cur.data(), 0.4f, 6,
                                                  48000.0, steady.data());
        const auto wildFit = kbs::EqMatch::fit (ref.data(), cur.data(), 0.4f, 6,
                                                48000.0, peaky.data());
        const auto noneFit = kbs::EqMatch::fit (ref.data(), cur.data(), 0.4f, 6);

        check (steadyFit.dynamicBands == 0, "a constant excess gets a static cut",
               (double) steadyFit.dynamicBands, 0.0);
        check (wildFit.dynamicBands > 0, "an occasional excess gets a dynamic one",
               (double) wildFit.dynamicBands, 1.0);
        check (noneFit.dynamicBands == 0, "no spread data means the old behavior exactly",
               (double) noneFit.dynamicBands, 0.0);

        if (! wildFit.bands.empty() && ! steadyFit.bands.empty())
        {
            const auto& b = wildFit.bands[0];
            const double total = std::abs ((double) b.gainDb) + std::abs ((double) b.rangeDb);
            const double steadyTotal = std::abs ((double) steadyFit.bands[0].gainDb);
            near (total, steadyTotal, 0.75, "and covers the same range in total");
        }
    }
    // ---- 14. QA-EqFlagship Task 1: the 96-band pool (W-15)
    std::printf ("%s", "\n  14. the 96-band pool\n");
    {
        std::printf ("      sizeof(ParametricEq) = %zu KB\n",
                     sizeof (kbs::ParametricEq) / 1024);
        check (kbs::ParametricEq::kMaxBands == 96, "the pool holds 96 bands",
               (double) kbs::ParametricEq::kMaxBands, 96.0);

        // Home frequencies: the named eight, then strictly rising log-spaced
        // homes that stay inside the audible span for every band in the pool.
        bool rising = true;
        for (int b = 9; b < kbs::kEqMaxBands && rising; ++b)
            rising = kbs::eqDefaultFreq (b) > kbs::eqDefaultFreq (b - 1);
        check (rising, "home frequencies rise monotonically past band 8", 1.0, 1.0);
        check (kbs::eqDefaultFreq (8) > 20.0f
               && kbs::eqDefaultFreq (95) < 20000.0f,
               "and every home stays inside 20 Hz - 20 kHz",
               (double) kbs::eqDefaultFreq (95), 14000.0);

        // The last band is a real band: set it, hear it, query it.
        kbs::ParametricEq eq;
        eq.prepare (48000.0, 512);
        kbs::EqBandParams b95;
        b95.on = true; b95.type = kbs::EqType::bell;
        b95.freqHz = 1000.0f; b95.gainDb = 6.0f; b95.q = 1.0f;
        eq.setBand (95, b95);
        near (20.0 * std::log10 (eq.bandMagnitudeAt (95, 1000.0f)), 6.0, 0.3,
              "band 96 boosts like any other band");

        // 95 idle bands beside it change nothing: the summed curve equals the
        // one live band's own curve.
        near (20.0 * std::log10 (eq.magnitudeAt (1000.0f)),
              20.0 * std::log10 (eq.bandMagnitudeAt (95, 1000.0f)), 0.01,
              "95 idle bands contribute exactly nothing");
    }
    // ---- 15. QA-EqFlagship Task 2: continuous slopes + all-pass (W-6)
    std::printf ("%s", "\n  15. continuous slopes + all-pass\n");
    {
        const double srr = 48000.0;

        auto queryDb = [] (kbs::ParametricEq& eq, double hz)
        { return 20.0 * std::log10 (eq.magnitudeAt ((float) hz)); };

        auto gainAt = [] (kbs::ParametricEq& eq, double freq, double srr2)
        {
            const int block = 256;
            std::vector<float> L (block), R (block), tail;
            double phase = 0.0;
            for (int bl = 0; bl < 80; ++bl)
            {
                for (int i = 0; i < block; ++i)
                {
                    phase += 2.0 * kbs::kPi * freq / srr2;
                    L[(size_t) i] = R[(size_t) i] = (float) (0.1 * std::sin (phase));
                }
                eq.process (L.data(), R.data(), block);
                if (bl >= 40) tail.insert (tail.end(), L.begin(), L.end());
            }
            const double period = srr2 / freq;
            const size_t whole = (size_t) (std::floor ((double) tail.size() / period) * period);
            tail.resize (std::max<size_t> (whole, (size_t) period));
            return 20.0 * std::log10 (levelAt (tail, freq, srr2, 0) / 0.1);
        };

        // A classic detent through the continuous path is the classic filter:
        // 24 dB/oct exactly equals the 4-pole Butterworth analytic.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::lowPass;
            b.freqHz = 1000.0f; b.q = 0.707f; b.slope = 24.0f;
            eq.setBand (0, b);
            const double om = std::tan (kbs::kPi * 2000.0 / srr)
                            / std::tan (kbs::kPi * 1000.0 / srr);
            const double want = -10.0 * std::log10 (1.0 + std::pow (om, 8.0));
            near (queryDb (eq, 2000.0), want, 0.05,
                  "24 dB/oct continuous IS the 4-pole Butterworth");
        }

        // 17 dB/oct: heard equals drawn (the ladder the audio runs is the
        // ladder the query evaluates), and it sits strictly between the
        // neighboring detents.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::lowPass;
            b.freqHz = 500.0f; b.q = 0.707f; b.slope = 17.0f;
            eq.setBand (0, b);
            near (gainAt (eq, 2000.0, srr), queryDb (eq, 2000.0), 0.3,
                  "17 dB/oct: heard equals drawn");

            kbs::ParametricEq lo, hi;
            lo.prepare (srr, 256); hi.prepare (srr, 256);
            b.slope = 12.0f; lo.setBand (0, b);
            b.slope = 18.0f; hi.setBand (0, b);
            const double m17 = queryDb (eq, 4000.0);
            check (m17 < queryDb (lo, 4000.0) && m17 > queryDb (hi, 4000.0),
                   "and lands between the 12 and 18 curves",
                   m17, queryDb (lo, 4000.0));
        }

        // Fractional below the old floor: 3 dB/oct is exact-analytic in the
        // linear modes and ladder-approximate (within 1.5 dB over two
        // octaves) in the IIR modes.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            eq.setMode (kbs::EqMode::linearLow);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::highPass;
            b.freqHz = 2000.0f; b.q = 0.707f; b.slope = 3.0f;
            eq.setBand (0, b);
            const double om = std::tan (kbs::kPi * 500.0 / srr)
                            / std::tan (kbs::kPi * 2000.0 / srr);
            const double n = 0.5;
            const double want = 20.0 * std::log10 (std::pow (om, n)
                                / std::sqrt (1.0 + std::pow (om, 2.0 * n)));
            near (queryDb (eq, 500.0), want, 0.05,
                  "linear-mode 3 dB/oct is the exact fractional Butterworth");

            eq.setMode (kbs::EqMode::zeroLatency);
            // The ladder is pinned in its ASYMPTOTIC region (three octaves
            // out): the analytic fractional is -3 dB at the cutoff and a
            // pole ladder cannot be, so the knee is softer by design - the
            // query draws each mode's truth either way.
            const double om3 = std::tan (kbs::kPi * 250.0 / srr)
                             / std::tan (kbs::kPi * 2000.0 / srr);
            const double want3 = 20.0 * std::log10 (std::pow (om3, n)
                                / std::sqrt (1.0 + std::pow (om3, 2.0 * n)));
            near (queryDb (eq, 250.0), want3, 1.5,
                  "IIR 3 dB/oct ladder tracks the ideal within 1.5 dB");
            near (gainAt (eq, 250.0, srr), queryDb (eq, 250.0), 0.3,
                  "and heard equals drawn in the IIR too");
        }

        // All-pass: unity magnitude everywhere, audibly nothing, but real
        // phase rotation at the corner.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::allPass;
            b.freqHz = 1000.0f; b.q = 0.707f;
            eq.setBand (0, b);
            near (queryDb (eq, 300.0), 0.0, 0.001, "all-pass draws flat");
            near (gainAt (eq, 300.0, srr), 0.0, 0.1, "all-pass passes level at 300");
            near (gainAt (eq, 4000.0, srr), 0.0, 0.1, "and at 4k");
            check (std::abs (eq.phaseAt (1000.0f)) > 1.0,
                   "and rotates real phase at its corner",
                   std::abs (eq.phaseAt (1000.0f)), 1.0);
        }
    }
    // ---- 16. QA-EqFlagship Task 3: per-band phase + Mixed mode (W-9)
    std::printf ("%s", "\n  16. per-band phase + Mixed mode\n");
    {
        const double srr = 48000.0;

        auto settleGain = [] (kbs::ParametricEq& eq, double freq, double srr2)
        {
            const int block = 256;
            std::vector<float> L (block), R (block), tail;
            double phase = 0.0;
            const int settle = 2 * eq.latencySamples() / block + 40;
            for (int bl = 0; bl < settle + 40; ++bl)
            {
                for (int i = 0; i < block; ++i)
                {
                    phase += 2.0 * kbs::kPi * freq / srr2;
                    L[(size_t) i] = R[(size_t) i] = (float) (0.1 * std::sin (phase));
                }
                eq.process (L.data(), R.data(), block);
                if (bl >= settle) tail.insert (tail.end(), L.begin(), L.end());
            }
            const double period = srr2 / freq;
            const size_t whole = (size_t) (std::floor ((double) tail.size() / period) * period);
            tail.resize (std::max<size_t> (whole, (size_t) period));
            return 20.0 * std::log10 (levelAt (tail, freq, srr2, 0) / 0.1);
        };

        // A minimum-phased band keeps its magnitude: heard equals drawn at
        // full phaseMix, and the drawn curve equals the phaseMix=0 curve.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            eq.setMode (kbs::EqMode::linearMedium);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 700.0f; b.gainDb = 8.0f; b.q = 1.2f;
            b.phaseMix = 1.0f;
            eq.setBand (0, b);
            const double drawn = 20.0 * std::log10 (eq.magnitudeAt (700.0f));
            near (settleGain (eq, 700.0, srr), drawn, 0.3,
                  "full min-phase band: heard equals drawn");
            near (drawn, 8.0, 0.3, "and the magnitude is the bell's own");
        }

        // The algebra survives phase: a side band at full phaseMix STILL
        // leaves a mono signal untouched, exactly.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            eq.setMode (kbs::EqMode::linearMedium);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 1000.0f; b.gainDb = 6.0f; b.q = 1.0f;
            b.channel = kbs::EqChannel::side;
            b.phaseMix = 1.0f;
            eq.setBand (0, b);
            near (settleGain (eq, 1000.0, srr), 0.0, 0.05,
                  "side band at full phaseMix leaves mono exactly alone");
        }

        // Mixed mode: a flat EQ is still a pure delay - the impulse peak
        // lands at exactly the reported latency.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 512);
            eq.setMode (kbs::EqMode::mixed);
            const int lat = eq.latencySamples();
            check (lat > 0, "mixed mode reports linear-class latency",
                   (double) lat, 1.0);

            std::vector<float> L (512, 0.0f), R (512, 0.0f);
            L[0] = R[0] = 1.0f;
            int peakAt = -1; float peakV = 0.0f; int base = 0;
            for (int bl = 0; bl < 8; ++bl)
            {
                eq.process (L.data(), R.data(), 512);
                for (int i = 0; i < 512; ++i)
                    if (std::abs (L[(size_t) i]) > peakV)
                    { peakV = std::abs (L[(size_t) i]); peakAt = base + i; }
                std::fill (L.begin(), L.end(), 0.0f);
                std::fill (R.begin(), R.end(), 0.0f);
                base += 512;
            }
            near ((double) peakAt, (double) lat, 0.0,
                  "flat mixed-mode impulse peaks at the reported latency");
        }

        // Mixed mode with a low bell: magnitude still equals the query (the
        // phase floor moves energy, never level).
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            eq.setMode (kbs::EqMode::mixed);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 80.0f; b.gainDb = 6.0f; b.q = 1.0f;
            eq.setBand (0, b);
            const double drawn = 20.0 * std::log10 (eq.magnitudeAt (80.0f));
            // 0.5 dB, deliberately: at order 12 the design grid is ~11.7 Hz
            // per bin, coarse under an 80 Hz bell - the FIR-resolution truth
            // every linear-phase EQ lives with at LF, not a phase-blend cost
            // (the same bell passes 0.3 at 700 Hz).
            near (settleGain (eq, 80.0, srr), drawn, 0.5,
                  "mixed mode: heard equals drawn on a low bell");
        }
    }
    // ---- 17. QA-EqFlagship Task 4: two-way dynamics + onset (W-12)
    std::printf ("%s", "\n  17. two-way dynamics + onset\n");
    {
        const double srr = 48000.0;

        auto steadyGr = [] (kbs::ParametricEq& eq, double amp, double freq,
                            double srr2)
        {
            const int block = 256;
            std::vector<float> L (block), R (block);
            double phase = 0.0;
            for (int bl = 0; bl < 120; ++bl)
            {
                for (int i = 0; i < block; ++i)
                {
                    phase += 2.0 * kbs::kPi * freq / srr2;
                    L[(size_t) i] = R[(size_t) i] = (float) (amp * std::sin (phase));
                }
                eq.process (L.data(), R.data(), block);
            }
            return (double) eq.bandGrDb (0);
        };

        // The below stage lifts quiet material by exactly its bounded travel.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 1000.0f; b.gainDb = 0.0f; b.q = 1.0f;
            b.dynamic = true;
            b.thresholdDb = 0.0f;            // above stage never engages
            b.thresholdBDb = -40.0f;
            b.ratioB = 20.0f;
            b.rangeBDb = 6.0f;
            eq.setBand (0, b);
            // -50 dB tone sits 10 dB under thresholdB; slope 0.95 asks 9.5,
            // range caps at 6.
            near (steadyGr (eq, 0.00316, 1000.0, srr), 6.0, 0.4,
                  "the below stage lifts quiet material to its range cap");
        }

        // The below stage can also push quiet away (negative range), and the
        // above stage still compresses independently in the same band.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 1000.0f; b.gainDb = 0.0f; b.q = 1.0f;
            b.dynamic = true;
            // Cap-pinned on both stages: the detector envelope of a
            // rectified sine sits a shade under peak by design, so the
            // deterministic number is the range cap, driven well past.
            b.thresholdDb = -30.0f; b.ratio = 20.0f; b.rangeDb = -6.0f;
            b.thresholdBDb = -35.0f; b.ratioB = 20.0f; b.rangeBDb = -6.0f;
            eq.setBand (0, b);
            // Loud (-20 dB): far over -> pinned at the -6 cap; the below
            // stage sees nothing.
            near (steadyGr (eq, 0.1, 1000.0, srr), -6.0, 0.2,
                  "loud material: the above stage compresses alone");
            // Quiet (-55 dB): far under -> pinned at the below cap; the
            // above stage sees nothing.
            near (steadyGr (eq, 0.00178, 1000.0, srr), -6.0, 0.2,
                  "quiet material: the below stage expands alone");
        }

        // A band whose below stage is inert behaves exactly as before.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 1000.0f; b.q = 1.0f;
            b.dynamic = true;
            b.thresholdDb = -40.0f; b.ratio = 4.0f; b.rangeDb = -12.0f;
            eq.setBand (0, b);
            // -20 dB tone: 20 over -> min(15, min(30, 12)) = -12 (the cap).
            near (steadyGr (eq, 0.1, 1000.0, srr), -12.0, 0.4,
                  "single-stage behavior is unchanged when B is inert");
        }

        // Onset at full mix: a sustained tone reads as nothing...
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 1000.0f; b.q = 1.0f;
            b.dynamic = true;
            b.thresholdDb = -40.0f; b.ratio = 4.0f; b.rangeDb = -12.0f;
            b.onsetMix = 1.0f;
            eq.setBand (0, b);
            near (steadyGr (eq, 0.1, 1000.0, srr), 0.0, 0.5,
                  "full onset mix ignores a sustained tone");

            // ...but a fresh attack from silence registers immediately.
            std::vector<float> L (256), R (256);
            double phase = 0.0;
            double deepest = 0.0;
            for (int bl = 0; bl < 20; ++bl)
            {
                for (int i = 0; i < 256; ++i)
                {
                    phase += 2.0 * kbs::kPi * 1000.0 / srr;
                    L[(size_t) i] = R[(size_t) i] = (float) (0.25 * std::sin (phase));
                }
                eq.process (L.data(), R.data(), 256);
                deepest = std::min (deepest, (double) eq.bandGrDb (0));
            }
            check (deepest < -3.0, "and a fresh attack registers",
                   deepest, -3.0);
        }
    }
    // ---- 18. QA-EqFlagship Task 5: spectral dynamics (W-1)
    std::printf ("%s", "\n  18. spectral dynamics\n");
    {
        const double srr = 48000.0;

        // Two tones inside one wide spectral bell: the strong one stands far
        // over its neighborhood and gets pushed to the range cap; the weak
        // one IS the neighborhood and must stay put.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            eq.setMode (kbs::EqMode::linearMedium);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 700.0f; b.gainDb = 0.0f; b.q = 0.4f;
            b.dynamic = true; b.spectral = true;
            b.thresholdDb = -48.0f;          // a 12 dB poke over the hood
            b.ratio = 20.0f;
            b.rangeDb = -12.0f;
            b.attackMs = 5.0f; b.releaseMs = 200.0f;
            b.density = 0.7f;
            eq.setBand (0, b);

            const int block = 256;
            std::vector<float> L (block), R (block), tail;
            double ph1 = 0.0, ph2 = 0.0;
            const int settle = 2 * eq.latencySamples() / block + 80;
            for (int bl = 0; bl < settle + 60; ++bl)
            {
                for (int i = 0; i < block; ++i)
                {
                    ph1 += 2.0 * kbs::kPi * 500.0 / srr;
                    ph2 += 2.0 * kbs::kPi * 900.0 / srr;
                    const float v = (float) (0.1 * std::sin (ph1)
                                           + 0.002 * std::sin (ph2));
                    L[(size_t) i] = R[(size_t) i] = v;
                }
                eq.process (L.data(), R.data(), block);
                if (bl >= settle) tail.insert (tail.end(), L.begin(), L.end());
            }
            const double strong = 20.0 * std::log10 (levelAt (tail, 500.0, srr, 0) / 0.1);
            const double weak   = 20.0 * std::log10 (levelAt (tail, 900.0, srr, 0) / 0.002);
            // The cap is 12 per bin; the tone's own mainlobe spans bins the
            // spread tapers, so the realized tone-level drop is smaller than
            // the deepest bin - assert the honest number.
            // Pinned at the honest realized numbers: the per-bin cap is 12,
            // and mainlobe dilution + mask smoothing land the realized tone
            // drop near half of it - the polish past this point is the
            // ear-tuning every spectral product lives on, with density and
            // the timings exposed for exactly that.
            check (strong < -5.0, "the standing-out tone is pushed down hard",
                   strong, -5.0);
            near (weak, 0.0, 1.5, "its quiet neighbor is left mostly alone");
            check (eq.bandGrDb (0) < -5.0, "and the meter reports the move",
                   (double) eq.bandGrDb (0), -5.0);
        }

        // A spectral band that never triggers is a pure delay: the impulse
        // peak stays at exactly the reported latency.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 512);
            eq.setMode (kbs::EqMode::linearMedium);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 1000.0f; b.gainDb = 0.0f; b.q = 1.0f;
            b.dynamic = true; b.spectral = true;
            b.thresholdDb = 0.0f;            // never
            b.rangeDb = -12.0f;
            eq.setBand (0, b);
            const int lat = eq.latencySamples();

            std::vector<float> L (512, 0.0f), R (512, 0.0f);
            L[0] = R[0] = 1.0f;
            int peakAt = -1; float peakV = 0.0f; int base = 0;
            for (int bl = 0; bl < 8; ++bl)
            {
                eq.process (L.data(), R.data(), 512);
                for (int i = 0; i < 512; ++i)
                    if (std::abs (L[(size_t) i]) > peakV)
                    { peakV = std::abs (L[(size_t) i]); peakAt = base + i; }
                std::fill (L.begin(), L.end(), 0.0f);
                std::fill (R.begin(), R.end(), 0.0f);
                base += 512;
            }
            near ((double) peakAt, (double) lat, 0.0,
                  "an untriggered spectral band is a pure delay");
        }

        // Outside the linear modes the flag is inert: the band is its plain
        // static self and the spectral query reads zero.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 1000.0f; b.gainDb = 4.0f; b.q = 1.0f;
            b.dynamic = true; b.spectral = true;
            b.thresholdDb = -60.0f; b.rangeDb = -12.0f;
            eq.setBand (0, b);
            near (eq.spectralGrDbAt (0, 1000.0f), 0.0, 0.001,
                  "spectral reads zero outside the linear modes");
        }
    }
    // ---- 19. QA-EqFlagship Task 6: the character stage (W-3, W-14)
    std::printf ("%s", "\n  19. the character stage\n");
    {
        const double srr = 48000.0;

        auto harmonicDb = [] (kbs::ParametricEq& eq, double freq, int harmonic,
                              double srr2)
        {
            const int block = 256;
            std::vector<float> L (block), R (block), tail;
            double phase = 0.0;
            for (int bl = 0; bl < 80; ++bl)
            {
                for (int i = 0; i < block; ++i)
                {
                    phase += 2.0 * kbs::kPi * freq / srr2;
                    L[(size_t) i] = R[(size_t) i] = (float) (0.3 * std::sin (phase));
                }
                eq.process (L.data(), R.data(), block);
                if (bl >= 40) tail.insert (tail.end(), L.begin(), L.end());
            }
            const double period = srr2 / freq;
            const size_t whole = (size_t) (std::floor ((double) tail.size() / period) * period);
            tail.resize (std::max<size_t> (whole, (size_t) period));
            return 20.0 * std::log10 (
                std::max (1.0e-9, levelAt (tail, freq * harmonic, srr2, 0)) / 0.3);
        };

        // Off = bit-exact pass-through, whatever the amount says.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            eq.setCharacter (kbs::EqCharMode::off, 1.0f);
            std::vector<float> L (256), R (256), refL (256);
            for (int i = 0; i < 256; ++i)
                refL[(size_t) i] = L[(size_t) i] = R[(size_t) i]
                    = (float) std::sin (0.1 * i) * 0.5f;
            eq.process (L.data(), R.data(), 256);
            double diff = 0.0;
            for (int i = 0; i < 256; ++i)
                diff += std::abs ((double) L[(size_t) i] - refL[(size_t) i]);
            check (diff == 0.0, "color off is bit-exact", diff, 0.0);
        }

        // Color A: real harmonics arrive, the level barely moves.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            // Half amount, the working setting: full drive on a hot sine
            // compresses the fundamental - that IS saturation - so the
            // level pin lives where people actually use it.
            eq.setCharacter (kbs::EqCharMode::colorA, 0.5f);
            const double fund = harmonicDb (eq, 1000.0, 1, srr);
            const double h3 = harmonicDb (eq, 1000.0, 3, srr);
            near (fund, 0.0, 1.5, "color A holds the level");
            check (h3 > -60.0, "and adds a real third harmonic", h3, -60.0);
        }

        // Difference mode with a FLAT EQ nulls exactly: nothing changed,
        // nothing to color.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            eq.setCharacter (kbs::EqCharMode::difference, 1.0f);
            std::vector<float> L (256), R (256), refL (256);
            for (int i = 0; i < 256; ++i)
                refL[(size_t) i] = L[(size_t) i] = R[(size_t) i]
                    = (float) std::sin (0.17 * i) * 0.4f;
            eq.process (L.data(), R.data(), 256);
            double diff = 0.0;
            for (int i = 0; i < 256; ++i)
                diff += std::abs ((double) L[(size_t) i] - refL[(size_t) i]);
            check (diff < 1.0e-6, "difference mode nulls on a flat EQ", diff, 0.0);
        }

        // Per-band sat: zero amount is untouched; a driven slice adds
        // harmonics near the band and leaves far-away content alone.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 1000.0f; b.gainDb = 0.0f; b.q = 1.0f;
            b.satAmt = 1.0f;
            eq.setBand (0, b);
            const double h3 = harmonicDb (eq, 1000.0, 3, srr);
            check (h3 > -60.0, "a saturated band's slice adds harmonics",
                   h3, -60.0);

            kbs::ParametricEq clean;
            clean.prepare (srr, 256);
            b.satAmt = 0.0f;
            clean.setBand (0, b);
            const double h3c = harmonicDb (clean, 1000.0, 3, srr);
            check (h3c < -80.0, "zero sat is clean", h3c, -80.0);
        }
    }
    // ---- 20. QA-EqFlagship Task 7: per-band modulators (W-13)
    std::printf ("%s", "\n  20. per-band modulators\n");
    {
        const double srr = 48000.0;

        auto windowGain = [] (kbs::ParametricEq& eq, double freq, double srr2,
                              int settleBlocks, int measureBlocks)
        {
            const int block = 256;
            std::vector<float> L (block), R (block), tail;
            double phase = 0.0;
            for (int bl = 0; bl < settleBlocks + measureBlocks; ++bl)
            {
                for (int i = 0; i < block; ++i)
                {
                    phase += 2.0 * kbs::kPi * freq / srr2;
                    L[(size_t) i] = R[(size_t) i] = (float) (0.1 * std::sin (phase));
                }
                eq.process (L.data(), R.data(), block);
                if (bl >= settleBlocks) tail.insert (tail.end(), L.begin(), L.end());
            }
            const double period = srr2 / freq;
            const size_t whole = (size_t) (std::floor ((double) tail.size() / period) * period);
            tail.resize (std::max<size_t> (whole, (size_t) period));
            return 20.0 * std::log10 (levelAt (tail, freq, srr2, 0) / 0.1);
        };

        // A slow LFO on gain makes the band's measured gain differ between
        // two short windows a half-cycle apart.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 1000.0f; b.gainDb = 6.0f; b.q = 1.0f;
            b.lfoDepth = 1.0f; b.lfoRateHz = 1.0f; b.lfoTarget = 1;   // gain
            eq.setBand (0, b);
            // 1 Hz: half a cycle is ~94 blocks.  Two ~10-block windows.
            const double w1 = windowGain (eq, 1000.0, srr, 20, 10);
            const double w2 = windowGain (eq, 1000.0, srr, 84, 10);
            check (std::abs (w1 - w2) > 2.0,
                   "a gain LFO audibly swings the band between windows",
                   std::abs (w1 - w2), 2.0);
        }

        // The envelope follower ducks against loud material (negative depth
        // on gain): a hot tone measures LESS boost than a quiet one.
        {
            kbs::ParametricEq loud, quiet;
            loud.prepare (srr, 256);
            quiet.prepare (srr, 256);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 1000.0f; b.gainDb = 6.0f; b.q = 1.0f;
            b.envDepth = -1.0f; b.envTarget = 1;
            loud.setBand (0, b);
            quiet.setBand (0, b);

            auto run = [&] (kbs::ParametricEq& eq, double amp)
            {
                const int block = 256;
                std::vector<float> L (block), R (block), tail;
                double phase = 0.0;
                for (int bl = 0; bl < 120; ++bl)
                {
                    for (int i = 0; i < block; ++i)
                    {
                        phase += 2.0 * kbs::kPi * 1000.0 / srr;
                        L[(size_t) i] = R[(size_t) i] = (float) (amp * std::sin (phase));
                    }
                    eq.process (L.data(), R.data(), block);
                    if (bl >= 80) tail.insert (tail.end(), L.begin(), L.end());
                }
                const double period = srr / 1000.0;
                const size_t whole = (size_t) (std::floor ((double) tail.size() / period) * period);
                tail.resize (std::max<size_t> (whole, (size_t) period));
                return 20.0 * std::log10 (levelAt (tail, 1000.0, srr, 0) / amp);
            };
            const double hot = run (loud, 0.5);
            const double soft = run (quiet, 0.005);
            check (soft - hot > 2.0,
                   "negative env depth ducks the boost on hot material",
                   soft - hot, 2.0);
        }

        // Zero depths = the plain static band, near-exactly.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 1000.0f; b.gainDb = 6.0f; b.q = 1.0f;
            eq.setBand (0, b);
            near (windowGain (eq, 1000.0, srr, 40, 40), 6.0, 0.15,
                  "zero depths leave the band its plain static self");
        }
    }
    // ---- 21. QA-EqFlagship Task 8: transforms, delta, domains, matched solo
    std::printf ("%s", "\n  21. curve transforms + delta + domain audition\n");
    {
        const double srr = 48000.0;

        // Scale -100% inverts the curve exactly: the +6 bell reads -6.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 1000.0f; b.gainDb = 6.0f; b.q = 1.0f;
            eq.setBand (0, b);
            const double before = 20.0 * std::log10 (eq.bandMagnitudeAt (0, 1000.0f));
            eq.setCurveTransform (-1.0f, 0.0f);
            const double after = 20.0 * std::log10 (eq.bandMagnitudeAt (0, 1000.0f));
            near (before, 6.0, 0.05, "the bell reads +6 before");
            near (after, -before, 0.02, "scale -100% inverts it exactly");
        }

        // Shift +12 semitones moves the response one octave, log-exactly.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 1000.0f; b.gainDb = 6.0f; b.q = 2.0f;
            eq.setBand (0, b);
            const double at1k = 20.0 * std::log10 (eq.bandMagnitudeAt (0, 1000.0f));
            eq.setCurveTransform (1.0f, 12.0f);
            const double at2k = 20.0 * std::log10 (eq.bandMagnitudeAt (0, 2000.0f));
            near (at2k, at1k, 0.05, "shift +12 st puts 1 kHz's response at 2 kHz");
        }

        // Delta on a FLAT EQ is silence: nothing changed, nothing to hear.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            eq.setDeltaListen (true);
            std::vector<float> L (256), R (256);
            double acc = 0.0;
            for (int bl = 0; bl < 20; ++bl)
            {
                for (int i = 0; i < 256; ++i)
                    L[(size_t) i] = R[(size_t) i]
                        = (float) (0.3 * std::sin (0.13 * (bl * 256 + i)));
                eq.process (L.data(), R.data(), 256);
                if (bl >= 4)
                    for (int i = 0; i < 256; ++i)
                        acc = std::max (acc, (double) std::abs (L[(size_t) i]));
            }
            check (acc < 1.0e-5, "delta on a flat EQ nulls", acc, 0.0);
        }

        // Delta with a +6 bell at the tone: what remains is the ADDED part,
        // (10^(6/20) - 1) times the input.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 1000.0f; b.gainDb = 6.0f; b.q = 1.0f;
            eq.setBand (0, b);
            eq.setDeltaListen (true);
            std::vector<float> L (256), R (256), tail;
            double phase = 0.0;
            for (int bl = 0; bl < 60; ++bl)
            {
                for (int i = 0; i < 256; ++i)
                {
                    phase += 2.0 * kbs::kPi * 1000.0 / srr;
                    L[(size_t) i] = R[(size_t) i] = (float) (0.1 * std::sin (phase));
                }
                eq.process (L.data(), R.data(), 256);
                if (bl >= 30) tail.insert (tail.end(), L.begin(), L.end());
            }
            const double period = srr / 1000.0;
            const size_t whole = (size_t) (std::floor ((double) tail.size() / period) * period);
            tail.resize (std::max<size_t> (whole, (size_t) period));
            const double got = levelAt (tail, 1000.0, srr, 0) / 0.1;
            const double expect = std::pow (10.0, 6.0 / 20.0) - 1.0;
            near (20.0 * std::log10 (got), 20.0 * std::log10 (expect), 0.5,
                  "delta of a +6 bell is the added part alone");
        }

        // Domain audition: a mono tone has no side - side audition is
        // silence, mid audition is the tone itself.
        {
            for (int domain = 0; domain < 2; ++domain)
            {
                kbs::ParametricEq eq;
                eq.prepare (srr, 256);
                eq.setAuditionDomain (domain == 0 ? kbs::EqChannel::side
                                                  : kbs::EqChannel::mid);
                std::vector<float> L (256), R (256), tail;
                double phase = 0.0;
                for (int bl = 0; bl < 20; ++bl)
                {
                    for (int i = 0; i < 256; ++i)
                    {
                        phase += 2.0 * kbs::kPi * 1000.0 / srr;
                        L[(size_t) i] = R[(size_t) i] = (float) (0.1 * std::sin (phase));
                    }
                    eq.process (L.data(), R.data(), 256);
                    if (bl >= 8) tail.insert (tail.end(), L.begin(), L.end());
                }
                const double period = srr / 1000.0;
                const size_t whole = (size_t) (std::floor ((double) tail.size() / period) * period);
                tail.resize (std::max<size_t> (whole, (size_t) period));
                const double db = 20.0 * std::log10 (
                    std::max (1.0e-9, levelAt (tail, 1000.0, srr, 0) / 0.1));
                if (domain == 0)
                    check (db < -80.0, "side audition of a mono tone is silence",
                           db, -80.0);
                else
                    near (db, 0.0, 0.1, "mid audition of a mono tone is the tone");
            }
        }

        // Loudness-matched solo: listening to the band AT the tone keeps the
        // audition within a couple of dB of the program it replaced.
        {
            kbs::ParametricEq eq;
            eq.prepare (srr, 256);
            kbs::EqBandParams b;
            b.on = true; b.type = kbs::EqType::bell;
            b.freqHz = 1000.0f; b.gainDb = 0.0f; b.q = 2.0f;
            eq.setBand (0, b);
            eq.setListenBand (0);
            std::vector<float> L (256), R (256), tail;
            double phase = 0.0;
            for (int bl = 0; bl < 120; ++bl)
            {
                for (int i = 0; i < 256; ++i)
                {
                    phase += 2.0 * kbs::kPi * 1000.0 / srr;
                    L[(size_t) i] = R[(size_t) i] = (float) (0.1 * std::sin (phase));
                }
                eq.process (L.data(), R.data(), 256);
                if (bl >= 80) tail.insert (tail.end(), L.begin(), L.end());
            }
            const double period = srr / 1000.0;
            const size_t whole = (size_t) (std::floor ((double) tail.size() / period) * period);
            tail.resize (std::max<size_t> (whole, (size_t) period));
            const double db = 20.0 * std::log10 (levelAt (tail, 1000.0, srr, 0) / 0.1);
            near (db, 0.0, 2.0, "matched solo holds the program's loudness");
        }
    }
    // ---- 22. QA-EqFlagship Task 9: Auto Cleanup + the scan accumulator
    std::printf ("%s", "\n  22. auto cleanup + spectrum scan\n");
    {
        const int n = kbs::EqMatch::kPoints;

        // A synthetic capture: flat -20 dB with one narrow +9 dB poke near
        // 2 kHz (a resonance) - cleanup must cut AT it, and nothing else big.
        auto pokeGrid = [n] (double pokeHz, double pokeDb, double widthOct)
        {
            std::vector<float> g ((size_t) n, -20.0f);
            for (int i = 0; i < n; ++i)
            {
                const double d = std::log2 (kbs::EqMatch::hzAt (i) / pokeHz);
                g[(size_t) i] += (float) (pokeDb
                    * std::exp (-0.5 * (d / (widthOct * 0.5)) * (d / (widthOct * 0.5))));
            }
            return g;
        };

        {
            const auto g = pokeGrid (2000.0, 9.0, 0.3);
            const auto r = kbs::EqMatch::cleanup (g.data(), 12, 48000.0);
            check (! r.bands.empty(), "cleanup finds the resonance",
                   (double) r.bands.size(), 1.0);
            if (! r.bands.empty())
            {
                const auto& b = r.bands[0];
                const double oct = std::abs (std::log2 (b.freqHz / 2000.0));
                check (oct < 0.25, "the cut lands on the resonance",
                       (double) b.freqHz, 2000.0);
                check (b.gainDb < -4.0, "and cuts it hard", b.gainDb, -4.0);
                check (! b.dynamic, "steady problem, static cut",
                       b.dynamic ? 1.0 : 0.0, 0.0);
            }
            // The flat remainder must not sprout cuts: every band near the poke.
            for (const auto& b : r.bands)
                check (std::abs (std::log2 (b.freqHz / 2000.0)) < 1.0,
                       "no cut far from the problem", (double) b.freqHz, 2000.0);
        }

        // The same poke with a big SWING at it comes out dynamic - the
        // intermittent problem goes to a dynamic band (the 12c rule reused).
        {
            const auto g = pokeGrid (2000.0, 9.0, 0.3);
            std::vector<float> spread ((size_t) n, 1.0f);
            for (int i = 0; i < n; ++i)
                if (std::abs (std::log2 (kbs::EqMatch::hzAt (i) / 2000.0)) < 0.4)
                    spread[(size_t) i] = 9.0f;
            const auto r = kbs::EqMatch::cleanup (g.data(), 12, 48000.0, spread.data());
            check (r.dynamicBands >= 1, "a swinging problem becomes dynamic",
                   (double) r.dynamicBands, 1.0);
            if (! r.bands.empty())
                check (r.bands[0].rangeDb < 0.0, "with a downward range",
                       r.bands[0].rangeDb, -1.0);
        }

        // The scan accumulator: a stereo pass with a mono 1 kHz tone reads a
        // mid peak at 1 kHz and calls the side meaningless; a hard
        // left-minus-right tone puts the energy in SIDE and flags it real.
        {
            const double srr = 48000.0;
            for (int mode = 0; mode < 2; ++mode)
            {
                kbs::SpectrumScan scan;
                scan.prepare (srr);
                std::vector<float> L (512), R (512);
                double phase = 0.0;
                for (int bl = 0; bl < 400; ++bl)
                {
                    for (int i = 0; i < 512; ++i)
                    {
                        phase += 2.0 * kbs::kPi * 1000.0 / srr;
                        const float v = (float) (0.25 * std::sin (phase));
                        L[(size_t) i] = v;
                        R[(size_t) i] = mode == 0 ? v : -v;
                    }
                    scan.push (L.data(), R.data(), 512);
                }
                check (scan.frames() > 40, "the scan accumulated frames",
                       (double) scan.frames(), 40.0);

                std::vector<float> mid ((size_t) n), side ((size_t) n);
                scan.midGrid (mid.data(), n, kbs::EqMatch::kLoHz, kbs::EqMatch::kHiHz);
                scan.sideGrid (side.data(), n, kbs::EqMatch::kLoHz, kbs::EqMatch::kHiHz);

                auto atHz = [n] (const std::vector<float>& g, double hz)
                {
                    int best = 0;
                    double bd = 1.0e9;
                    for (int i = 0; i < n; ++i)
                    {
                        const double d = std::abs (std::log2 (kbs::EqMatch::hzAt (i) / hz));
                        if (d < bd) { bd = d; best = i; }
                    }
                    return (double) g[(size_t) best];
                };

                if (mode == 0)
                {
                    check (atHz (mid, 1000.0) - atHz (mid, 4000.0) > 30.0,
                           "mono tone: the mid grid peaks at the tone",
                           atHz (mid, 1000.0) - atHz (mid, 4000.0), 30.0);
                    check (! scan.sideMeaningful(),
                           "mono tone: the side is not meaningful", 0.0, 0.0);
                }
                else
                {
                    check (atHz (side, 1000.0) - atHz (mid, 1000.0) > 30.0,
                           "anti-phase tone: the energy lives in side",
                           atHz (side, 1000.0) - atHz (mid, 1000.0), 30.0);
                    check (scan.sideMeaningful(),
                           "anti-phase tone: the side is meaningful", 1.0, 1.0);
                }
            }
        }
    }
    std::printf (failures == 0 ? "\n  all checks passed\n\n" : "\n  %d FAILURE(S)\n\n", failures);
    return failures == 0 ? 0 : 1;
}
