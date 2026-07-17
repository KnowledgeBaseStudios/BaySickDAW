#include "LibraryPitchShifters.h"
#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace
{
    // Per-sample envelope read with edge clamp; nullptr => the neutral default.
    inline float envAt (const float* env, int i, int n, float dflt) noexcept
    {
        return (env == nullptr) ? dflt : env[juce::jlimit (0, n - 1, i)];
    }
    inline float ratioToSemis (float ratio) noexcept
    {
        return 12.0f * std::log2 (juce::jmax (1.0e-4f, ratio));
    }

    // Source-position lookup for a fractional EDITED index, with linear identity-
    // slope extrapolation outside [0, nOut) so latency-priming (negative index)
    // and tail-flush (past the end) read sane positions.  sp[j] is monotone
    // within a run; the streaming warp below detects backward steps as cuts.
    inline double srcPosExt (const double* sp, int nOut, double e) noexcept
    {
        if (sp == nullptr || nOut <= 0) return 0.0;
        if (nOut == 1)         return sp[0] + e;
        if (e <= 0.0)          return sp[0]        + e * (sp[1] - sp[0]);
        if (e >= (double) (nOut - 1))
            return sp[nOut - 1] + (e - (double) (nOut - 1)) * (sp[nOut - 1] - sp[nOut - 2]);
        const int    i0 = (int) e;
        const double fr = e - (double) i0;
        return sp[(size_t) i0] * (1.0 - fr) + sp[(size_t) (i0 + 1)] * fr;
    }
}

// ── Rubber Band R3 (BAYSICK_HAS_RUBBERBAND) ──────────────────────────────────
#if BAYSICK_HAS_RUBBERBAND
#include <rubberband/RubberBandStretcher.h>

namespace
{
class RubberBandShifter : public IPitchShifter
{
public:
    void bake (const float* in, float* out, int n, double sr,
               const float* pitchRatio, const float* formantScale) override
    {
        if (n <= 0) return;
        using RB = RubberBand::RubberBandStretcher;
        // Offline mode fixes pitch for the whole pass; time-varying edits need
        // the REAL-TIME engine driven block-by-block.  OptionPitchHighConsistency
        // is the only mode that takes per-block pitch changes without a
        // discontinuity across the 1.0 boundary (must be set at construction).
        const int opts = RB::OptionProcessRealTime
                       | RB::OptionEngineFiner
                       | RB::OptionFormantPreserved
                       | RB::OptionPitchHighConsistency;
        RB rb ((size_t) juce::jmax (8000.0, sr), 1, opts, 1.0,
               (double) juce::jmax (0.0625f, envAt (pitchRatio, 0, n, 1.0f)));
        rb.setFormantScale (formantAuto (formantScale, 0, n));

        const int pad   = (int) rb.getPreferredStartPad();  // input silence to prepend
        const int delay = (int) rb.getStartDelay();         // output frames to discard

        const int block = 1024;
        std::vector<float> zeros ((size_t) block, 0.0f);
        std::vector<float> obuf  ((size_t) block, 0.0f);

        int produced = 0, written = 0, inPos = 0, padFed = 0, tailFed = 0;
        const int tailMax = pad + delay + 4 * block;

        auto drain = [&] () noexcept
        {
            int avail;
            while (written < n && (avail = rb.available()) > 0)
            {
                const int take = juce::jmin (avail, block);
                float* o = obuf.data();
                float* const* op = &o;
                const int got = (int) rb.retrieve (op, (size_t) take);
                for (int k = 0; k < got; ++k)
                    if (produced + k >= delay && written < n)
                        out[written++] = obuf[(size_t) k];
                produced += got;
            }
        };

        while (written < n)
        {
            const float* src; int chunk, envIdx; bool isFinal = false;
            if (padFed < pad)
            {
                chunk = juce::jmin (block, pad - padFed);
                src = zeros.data(); envIdx = 0; padFed += chunk;
            }
            else if (inPos < n)
            {
                chunk = juce::jmin (block, n - inPos);
                src = in + inPos; envIdx = inPos + chunk / 2; inPos += chunk;
            }
            else
            {
                chunk = block; src = zeros.data(); envIdx = n - 1;
                tailFed += chunk; isFinal = (tailFed >= tailMax);
            }

            rb.setPitchScale ((double) juce::jmax (0.0625f, envAt (pitchRatio, envIdx, n, 1.0f)));
            rb.setFormantScale (formantAuto (formantScale, envIdx, n));
            const float* const* ip = &src;
            rb.process (ip, (size_t) chunk, isFinal);
            drain();
            if (isFinal && rb.available() < 0) break;
        }
        while (written < n) out[written++] = 0.0f;   // exact length even on shortfall
    }

    void bakeWarped (const float* in, int nIn, float* out, int nOut, double sr,
                     const double* srcPos, const float* pitchRatio, const float* formantScale) override
    {
        if (nOut <= 0) return;
        std::fill (out, out + nOut, 0.0f);
        if (nIn <= 0 || srcPos == nullptr) return;
        using RB = RubberBand::RubberBandStretcher;
        const int opts = RB::OptionProcessRealTime | RB::OptionEngineFiner
                       | RB::OptionFormantPreserved | RB::OptionPitchHighConsistency;
        RB rb ((size_t) juce::jmax (8000.0, sr), 1, opts, 1.0,
               (double) juce::jmax (0.0625f, envAt (pitchRatio, 0, nOut, 1.0f)));
        rb.setFormantScale (formantAuto (formantScale, 0, nOut));

        const int pad   = (int) rb.getPreferredStartPad();   // input silence to prepend
        const int delay = (int) rb.getStartDelay();          // output frames to discard
        const int block = 256;
        std::vector<float> ibuf, obuf ((size_t) block);
        std::vector<float> zeros ((size_t) juce::jmax (block, pad), 0.0f);

        // real edited index = outBase + (produced - delay); outBase re-anchors on
        // a detach reset so the post-cut audio lands at the right edited spot.
        int produced = 0, outBase = 0;
        auto drain = [&] () noexcept
        {
            int avail;
            while ((avail = rb.available()) > 0)
            {
                const int take = juce::jmin (avail, block);
                float* o = obuf.data(); float* const* op = &o;
                const int got = (int) rb.retrieve (op, (size_t) take);
                if (got <= 0) break;
                for (int k = 0; k < got; ++k)
                {
                    const int ri = outBase + produced - delay;
                    if (ri >= 0 && ri < nOut) out[(size_t) ri] = obuf[(size_t) k];
                    ++produced;
                }
            }
        };
        auto prime = [&] () noexcept
        {
            if (pad <= 0) return;
            const float* z = zeros.data(); const float* const* zp = &z;
            rb.process (zp, (size_t) pad, false);
            drain();
        };
        prime();

        long long srcCursor = 0;   // integer source samples fed (excludes pad)
        int edited = 0;
        while (edited < nOut)
        {
            const int    B  = juce::jmin (block, nOut - edited);
            const double s0 = srcPosExt (srcPos, nOut, (double) edited);
            const double s1 = srcPosExt (srcPos, nOut, (double) (edited + B));
            long long inN = (long long) std::llround (s1) - srcCursor;
            if (inN <= 0)
            {
                if (s1 < (double) srcCursor - 4.0)   // backward map step = detach cut
                {
                    rb.reset();
                    outBase = edited; produced = 0;   // re-anchor output after the cut
                    srcCursor = (long long) std::llround (juce::jmax (0.0, s0));
                    prime();
                    inN = juce::jmax ((long long) 1, (long long) std::llround (s1) - srcCursor);
                }
                else
                    inN = 1;
            }
            inN = juce::jlimit ((long long) 1, (long long) (16 * block), inN);

            const int em = juce::jlimit (0, nOut - 1, edited + B / 2);
            rb.setTimeRatio   ((double) B / (double) inN);
            rb.setPitchScale  ((double) juce::jmax (0.0625f, envAt (pitchRatio, em, nOut, 1.0f)));
            rb.setFormantScale (formantAuto (formantScale, em, nOut));

            ibuf.resize ((size_t) inN);
            for (long long k = 0; k < inN; ++k)
            {
                const long long idx = srcCursor + k;
                ibuf[(size_t) k] = (idx >= 0 && idx < nIn) ? in[(size_t) idx] : 0.0f;
            }
            const float* ip = ibuf.data(); const float* const* ipp = &ip;
            rb.process (ipp, (size_t) inN, false);
            srcCursor += inN;
            drain();
            edited += B;
        }

        // Flush: feed silence until the tail of real output drains.
        int tail = 0; const int tailMax = pad + delay + 8 * block;
        while (outBase + produced - delay < nOut && tail < tailMax)
        {
            const bool last = (tail + block >= tailMax);
            const float* z = zeros.data(); const float* const* zp = &z;
            rb.process (zp, (size_t) juce::jmin (block, (int) zeros.size()), last);
            tail += block;
            drain();
        }
    }

    const char* name() const noexcept override { return "Rubber Band"; }

private:
    // 0.0 == automatic (== 1/pitchScale under FormantPreserved).  A non-neutral
    // throat value forces an explicit formant scale.
    static double formantAuto (const float* env, int i, int n) noexcept
    {
        const float v = envAt (env, i, n, 1.0f);
        return (std::abs (v - 1.0f) < 1.0e-4f) ? 0.0 : (double) v;
    }
};
} // namespace
#endif

// ── Signalsmith Stretch (BAYSICK_HAS_SIGNALSMITH) ────────────────────────────
#if BAYSICK_HAS_SIGNALSMITH
#include "signalsmith-stretch/signalsmith-stretch.h"

namespace
{
class SignalsmithShifter : public IPitchShifter
{
public:
    void bake (const float* in, float* out, int n, double sr,
               const float* pitchRatio, const float* formantScale) override
    {
        if (n <= 0) return;
        signalsmith::stretch::SignalsmithStretch<float> st (1234L);  // deterministic renders
        st.presetDefault (1, (float) sr);

        const float tonality = 8000.0f / (float) sr;
        const int   totalLat = st.inputLatency() + st.outputLatency();

        // Latency-compensated streaming: output global index tracks input global
        // index 1:1 (inSamps==outSamps => time ratio 1), delayed by totalLat.
        // Feed n real + totalLat silence, discard the first totalLat output.
        const int inTotal = n + totalLat;
        const int block   = 512;
        std::vector<float> cin ((size_t) block, 0.0f), cout ((size_t) block, 0.0f);

        int gi = 0, written = 0;
        while (gi < inTotal && written < n)
        {
            const int cnt = juce::jmin (block, inTotal - gi);
            for (int k = 0; k < cnt; ++k)
                cin[(size_t) k] = (gi + k < n) ? in[gi + k] : 0.0f;

            const int envIdx = gi + cnt / 2;
            st.setTransposeSemitones (ratioToSemis (envAt (pitchRatio, envIdx, n, 1.0f)), tonality);
            st.setFormantSemitones (ratioToSemis (envAt (formantScale, envIdx, n, 1.0f)),
                                    /*compensatePitch*/ true);

            const float* inRow[1]  = { cin.data() };
            float*       outRow[1] = { cout.data() };
            st.process (inRow, cnt, outRow, cnt);

            for (int k = 0; k < cnt; ++k)
                if (gi + k >= totalLat && written < n)
                    out[written++] = cout[(size_t) k];
            gi += cnt;
        }
        while (written < n) out[written++] = 0.0f;
    }

    void bakeWarped (const float* in, int nIn, float* out, int nOut, double sr,
                     const double* srcPos, const float* pitchRatio, const float* formantScale) override
    {
        if (nOut <= 0) return;
        std::fill (out, out + nOut, 0.0f);
        if (nIn <= 0 || srcPos == nullptr) return;
        signalsmith::stretch::SignalsmithStretch<float> st (1234L);
        st.presetDefault (1, (float) sr);
        const float tonality = 8000.0f / (float) sr;
        const int   totalLat = st.inputLatency() + st.outputLatency();

        const int block = 256;
        std::vector<float> cin, cout ((size_t) block);
        const int targetProduced = nOut + totalLat;

        // Latency-compensated streaming warp.  Feed the source so that when
        // `produced` output samples exist, the cumulative source fed is
        // F = srcPos(produced - totalLat) + totalLat -- i.e. output[p] carries
        // the warped source for edited index p - totalLat.  process() emits
        // EXACTLY the requested count, so the accounting is exact.
        long long srcFed = 0;
        int produced = 0;
        while (produced < targetProduced)
        {
            const int    B  = juce::jmin (block, targetProduced - produced);
            const double e0 = (double) produced - totalLat;
            const double e1 = e0 + B;
            const double Fnext = srcPosExt (srcPos, nOut, e1) + (double) totalLat;
            long long inN = (long long) std::llround (Fnext) - srcFed;
            if (inN <= 0)
            {
                if (Fnext < (double) srcFed - 4.0 && e1 > 0.0)   // backward map step = detach cut
                {
                    st.reset();
                    srcFed = (long long) std::llround (juce::jmax (0.0, srcPosExt (srcPos, nOut, e1)));
                }
                inN = 1;
            }
            inN = juce::jlimit ((long long) 1, (long long) (16 * block), inN);

            cin.resize ((size_t) inN);
            for (long long k = 0; k < inN; ++k)
            {
                const long long idx = srcFed + k;
                cin[(size_t) k] = (idx >= 0 && idx < nIn) ? in[(size_t) idx] : 0.0f;
            }
            const int em = juce::jlimit (0, nOut - 1, (int) std::llround ((e0 + e1) * 0.5));
            st.setTransposeSemitones (ratioToSemis (envAt (pitchRatio, em, nOut, 1.0f)), tonality);
            st.setFormantSemitones   (ratioToSemis (envAt (formantScale, em, nOut, 1.0f)), true);

            cout.resize ((size_t) B);
            const float* inRow[1]  = { cin.data() };
            float*       outRow[1] = { cout.data() };
            st.process (inRow, (int) inN, outRow, B);

            for (int k = 0; k < B; ++k)
            {
                const int ri = produced + k - totalLat;
                if (ri >= 0 && ri < nOut) out[(size_t) ri] = cout[(size_t) k];
            }
            produced += B;
            srcFed   += inN;
        }
    }

    const char* name() const noexcept override { return "Signalsmith"; }
};
} // namespace
#endif

// ── WORLD (BAYSICK_HAS_WORLD) ────────────────────────────────────────────────
#if BAYSICK_HAS_WORLD
#include "world/harvest.h"
#include "world/cheaptrick.h"
#include "world/d4c.h"
#include "world/synthesis.h"

namespace
{
class WorldShifter : public IPitchShifter
{
public:
    void bake (const float* in, float* out, int n, double sr,
               const float* pitchRatio, const float* formantScale) override
    {
        if (n <= 0) return;
        const int fs = (int) juce::jmax (8000.0, sr);

        std::vector<double> x ((size_t) n);
        for (int i = 0; i < n; ++i) x[(size_t) i] = (double) in[i];

        // ANALYZE with the ORIGINAL signal (ordering is load-bearing: CheapTrick
        // + D4C need the unmodified f0; scale f0 only afterward).
        HarvestOption hopt;  InitializeHarvestOption (&hopt);
        hopt.frame_period = 5.0;
        hopt.f0_floor     = 71.0;    // 71 keeps CheapTrick's FFT at 2048; lower forces 4096
        hopt.f0_ceil      = 1100.0;  // widened for high vocals
        const int f0len = GetSamplesForHarvest (fs, n, hopt.frame_period);
        if (f0len <= 0) { std::memset (out, 0, (size_t) n * sizeof (float)); return; }

        std::vector<double> temporal ((size_t) f0len), f0 ((size_t) f0len);
        Harvest (x.data(), n, fs, &hopt, temporal.data(), f0.data());

        CheapTrickOption copt;  InitializeCheapTrickOption (fs, &copt);
        const int fftSize = GetFFTSizeForCheapTrick (fs, &copt);
        const int H = fftSize / 2 + 1;

        std::vector<std::vector<double>> specRows ((size_t) f0len, std::vector<double> ((size_t) H));
        std::vector<std::vector<double>> apRows   ((size_t) f0len, std::vector<double> ((size_t) H));
        std::vector<double*> spec ((size_t) f0len), ap ((size_t) f0len);
        for (int f = 0; f < f0len; ++f)
        {
            spec[(size_t) f] = specRows[(size_t) f].data();
            ap  [(size_t) f] = apRows  [(size_t) f].data();
        }

        CheapTrick (x.data(), n, fs, temporal.data(), f0.data(), f0len, &copt, spec.data());
        D4COption dopt;  InitializeD4COption (&dopt);
        D4C (x.data(), n, fs, temporal.data(), f0.data(), f0len, fftSize, &dopt, ap.data());

        // MODIFY: per-frame pitch scale (== the shift; envelope stays fixed =>
        // formant-preserving).  Optional throat warp on the envelope.
        for (int f = 0; f < f0len; ++f)
        {
            const int si = juce::jlimit (0, n - 1, (int) (temporal[(size_t) f] * fs));
            f0[(size_t) f] *= (double) juce::jmax (1.0e-4f, envAt (pitchRatio, si, n, 1.0f));
            const float fscl = envAt (formantScale, si, n, 1.0f);
            if (std::abs (fscl - 1.0f) > 1.0e-4f)
                warpEnvelope (spec[(size_t) f], H, (double) fscl);
        }

        std::vector<double> y ((size_t) n, 0.0);
        Synthesis (f0.data(), f0len, spec.data(), ap.data(), fftSize,
                   hopt.frame_period, fs, n, y.data());
        writeNormalized (in, n, y.data(), n, out);
    }

    void bakeWarped (const float* in, int nIn, float* out, int nOut, double sr,
                     const double* srcPos, const float* pitchRatio, const float* formantScale) override
    {
        if (nOut <= 0) return;
        std::memset (out, 0, (size_t) nOut * sizeof (float));
        if (nIn <= 0 || srcPos == nullptr) return;
        const int fs = (int) juce::jmax (8000.0, sr);

        std::vector<double> x ((size_t) nIn);
        for (int i = 0; i < nIn; ++i) x[(size_t) i] = (double) in[i];

        // ANALYZE the ORIGINAL (source) signal -- same ordering rule as bake().
        HarvestOption hopt;  InitializeHarvestOption (&hopt);
        hopt.frame_period = 5.0;  hopt.f0_floor = 71.0;  hopt.f0_ceil = 1100.0;
        const int f0len = GetSamplesForHarvest (fs, nIn, hopt.frame_period);
        if (f0len <= 1) { std::memset (out, 0, (size_t) nOut * sizeof (float)); return; }

        std::vector<double> temporal ((size_t) f0len), f0 ((size_t) f0len);
        Harvest (x.data(), nIn, fs, &hopt, temporal.data(), f0.data());

        CheapTrickOption copt;  InitializeCheapTrickOption (fs, &copt);
        const int fftSize = GetFFTSizeForCheapTrick (fs, &copt);
        const int H = fftSize / 2 + 1;

        std::vector<std::vector<double>> specRows ((size_t) f0len, std::vector<double> ((size_t) H));
        std::vector<std::vector<double>> apRows   ((size_t) f0len, std::vector<double> ((size_t) H));
        std::vector<double*> spec ((size_t) f0len), ap ((size_t) f0len);
        for (int f = 0; f < f0len; ++f)
        {
            spec[(size_t) f] = specRows[(size_t) f].data();
            ap  [(size_t) f] = apRows  [(size_t) f].data();
        }
        CheapTrick (x.data(), nIn, fs, temporal.data(), f0.data(), f0len, &copt, spec.data());
        D4COption dopt;  InitializeD4COption (&dopt);
        D4C (x.data(), nIn, fs, temporal.data(), f0.data(), f0len, fftSize, &dopt, ap.data());

        // RE-TIME the analysis frames onto the EDITED timeline: edited synthesis
        // frame g (edited sample g*framePeriod) reads the SOURCE frame the map
        // points to.  WORLD rebuilds phase per frame, so a remap -- even across a
        // detach jump -- is a clean splice, no phase-vocoder wobble.  f0 uses the
        // NEAREST source frame (never interpolate across a voiced/unvoiced 0);
        // the smooth envelope + aperiodicity interpolate linearly.
        const double fpSec  = hopt.frame_period * 0.001;
        const int edFrames = GetSamplesForHarvest (fs, nOut, hopt.frame_period);
        if (edFrames <= 0) { std::memset (out, 0, (size_t) nOut * sizeof (float)); return; }

        std::vector<double> f0Ed ((size_t) edFrames);
        std::vector<std::vector<double>> specEdR ((size_t) edFrames, std::vector<double> ((size_t) H));
        std::vector<std::vector<double>> apEdR   ((size_t) edFrames, std::vector<double> ((size_t) H));
        std::vector<double*> specEd ((size_t) edFrames), apEd ((size_t) edFrames);
        for (int g = 0; g < edFrames; ++g)
        {
            specEd[(size_t) g] = specEdR[(size_t) g].data();
            apEd  [(size_t) g] = apEdR  [(size_t) g].data();
        }

        for (int g = 0; g < edFrames; ++g)
        {
            const int    edSamp = juce::jlimit (0, nOut - 1, (int) std::llround ((double) g * fpSec * fs));
            const double srcSamp = srcPos[(size_t) edSamp];
            const double srcFrameF = juce::jlimit (0.0, (double) (f0len - 1), (srcSamp / fs) / fpSec);
            const int    sf0 = (int) srcFrameF;
            const int    sf1 = juce::jmin (f0len - 1, sf0 + 1);
            const double fr  = srcFrameF - (double) sf0;

            const int nf = (fr < 0.5 ? sf0 : sf1);
            const double pr = (double) juce::jmax (1.0e-4f, envAt (pitchRatio, edSamp, nOut, 1.0f));
            f0Ed[(size_t) g] = f0[(size_t) nf] * pr;

            for (int k = 0; k < H; ++k)
            {
                specEdR[(size_t) g][(size_t) k] = specRows[(size_t) sf0][(size_t) k] * (1.0 - fr)
                                                + specRows[(size_t) sf1][(size_t) k] * fr;
                apEdR  [(size_t) g][(size_t) k] = apRows  [(size_t) sf0][(size_t) k] * (1.0 - fr)
                                                + apRows  [(size_t) sf1][(size_t) k] * fr;
            }
            const float fscl = envAt (formantScale, edSamp, nOut, 1.0f);
            if (std::abs (fscl - 1.0f) > 1.0e-4f)
                warpEnvelope (specEd[(size_t) g], H, (double) fscl);
        }

        std::vector<double> y ((size_t) nOut, 0.0);
        Synthesis (f0Ed.data(), edFrames, specEd.data(), apEd.data(), fftSize,
                   hopt.frame_period, fs, nOut, y.data());
        writeNormalized (in, nIn, y.data(), nOut, out);
    }

    const char* name() const noexcept override { return "WORLD"; }

private:
    // WORLD's Synthesis does not preserve absolute level -- it runs a bit hot vs
    // the analyzed input (~1.14x on vocal here), unlike RubberBand/Signalsmith
    // which are level-neutral.  Match the output RMS back to the input so WORLD
    // sits at the same level as the other engines.  Gain capped to guard
    // near-silence.  QA-Fe2 Task 4: the former buzz-fix helpers (AP floor /
    // 11 kHz de-emphasis / peak tanh) are gone for good -- the buzz/water was
    // the take's own noise layer re-rendered F0-gated by Synthesis, fixed at
    // the source by De-noise; stock WORLD + a cleaned take is the
    // ear-validated configuration ("cleantake", running notes 2026-07-16).
    static void writeNormalized (const float* in, int nIn, const double* y, int nOut, float* out) noexcept
    {
        double inSum = 0.0, outSum = 0.0;
        for (int i = 0; i < nIn;  ++i) { const double v = (double) in[i]; inSum += v * v; }
        for (int i = 0; i < nOut; ++i) outSum += y[i] * y[i];
        const double inRms  = std::sqrt (inSum  / (double) juce::jmax (1, nIn));
        const double outRms = std::sqrt (outSum / (double) juce::jmax (1, nOut));
        const double g = (outRms > 1.0e-9) ? juce::jmin (4.0, inRms / outRms) : 1.0;
        for (int i = 0; i < nOut; ++i) out[i] = (float) (y[i] * g);
    }

    // Warp the power-spectrum envelope frequency axis: dst[k] = src(k/ratio).
    // ratio > 1 raises formants (smaller/tighter throat), < 1 lowers (bigger).
    // Reads overlap writes, so warp into scratch then copy back.
    static void warpEnvelope (double* row, int H, double ratio) noexcept
    {
        std::vector<double> scratch ((size_t) H);
        for (int k = 0; k < H; ++k)
        {
            double pos  = (double) k / ratio;
            int    i0   = (int) std::floor (pos);
            double frac = pos - i0;
            int    i1   = i0 + 1;
            if (i0 < 0)     { i0 = 0; i1 = 0; frac = 0.0; }
            if (i1 > H - 1) { i1 = H - 1; if (i0 > H - 1) i0 = H - 1; }
            scratch[(size_t) k] = row[(size_t) i0] * (1.0 - frac) + row[(size_t) i1] * frac;
        }
        std::memcpy (row, scratch.data(), (size_t) H * sizeof (double));
    }
};
} // namespace
#endif

// ── Factory ──────────────────────────────────────────────────────────────────
std::unique_ptr<IPitchShifter> makePitchShifter (PitchEngine engine)
{
    switch (engine)
    {
       #if BAYSICK_HAS_RUBBERBAND
        case PitchEngine::RubberBand:  return std::make_unique<RubberBandShifter>();
       #endif
       #if BAYSICK_HAS_SIGNALSMITH
        case PitchEngine::Signalsmith: return std::make_unique<SignalsmithShifter>();
       #endif
       #if BAYSICK_HAS_WORLD
        case PitchEngine::World:       return std::make_unique<WorldShifter>();
       #endif
        default: break;
    }
    return nullptr;
}
