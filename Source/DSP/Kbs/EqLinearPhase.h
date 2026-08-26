// KBS Plugins — linear-phase FIR convolution for the EQ.
//
// The engine this replaced windowed each frame with Hann on the way in and
// again on the way out at 50 % overlap, and multiplied a raw sampled magnitude
// onto the spectrum of an un-padded frame. Three faults followed from that
// construction: Hann-squared at 50 % does not sum flat, so everything through
// it carried a 6 dB tremolo at the frame rate; the multiply was circular
// convolution, so the filter's impulse response wrapped inside the frame and
// smeared narrow curves; and the delay it reported was half the delay it
// imposed. All three are structural, so the structure is different here.
//
// This is overlap-save with a designed FIR:
//
//   1. The magnitude curve is sampled on the FFT grid, made symmetric, and
//      inverse-transformed - that gives the zero-phase impulse response.
//   2. The response is rotated to the centre of the window and shaped by a
//      Kaiser window to exactly `taps` samples. Windowing in time is what the
//      old engine skipped: it trades the sampled grid's infinite ringing for a
//      smooth, finite filter whose response interpolates the grid.
//   3. Frames of `hop` new input samples are zero-padded to N, transformed,
//      multiplied by the FIR's spectrum, and the last `hop` samples of the
//      result - the ones free of wrap - are kept. No analysis window, no
//      synthesis window, nothing to sum flat: overlap-save is exact linear
//      convolution, so a flat curve passes bit-exactly (within float noise).
//
// Latency is hop + taps/2: the frame gather plus the FIR's group delay. It is
// reported by latencySamples() and pinned by a test that measures an impulse,
// because the previous engine's report was wrong precisely where nobody
// measured it.
//
// Rebuilding the curve is cheap enough for a parameter drag: one real FFT
// round trip on scratch that is allocated in prepare(). setMagnitude() may be
// called from the audio thread on the buffers this object already owns.
#pragma once

#include "FFT.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstring>
#include <functional>
#include <memory>
#include <vector>

namespace kbs {

class EqLinearPhase
{
public:
    // Sizes come in powers of two. taps = N/2 + 1, hop = N/2 - taps + 1 + N/2
    // ... spelled plainly: hop = N - taps + 1, and with taps = N/2 + 1 that is
    // N/2, so every frame is half fresh input. Latency = hop + (taps - 1) / 2.
    void prepare (int fftOrder, int channels)
    {
        order = fftOrder;
        n     = 1 << order;
        taps  = n / 2 + 1;
        hop   = n - taps + 1;              // = n/2, kept in this form because
                                           // it is the overlap-save identity
        fft   = std::make_unique<FFT> (order);

        spectrum.assign ((size_t) n, { 0.0f, 0.0f });
        scratch.assign ((size_t) n, { 0.0f, 0.0f });
        design.assign ((size_t) n, { 0.0f, 0.0f });

        // Matrix mode (QA-EqPro SC-3): per-domain routing needs a full 2x2
        // convolution, so the cross spectra and the second channel's transform
        // scratch are sized here - rebuilds run on the audio thread and must
        // not allocate.
        for (auto* v : { &specLL, &specLR, &specRL, &specRR, &scratchB, &outA, &outB })
            v->assign ((size_t) n, { 0.0f, 0.0f });
        matrixOn = false;

        chans.assign ((size_t) std::max (1, channels), {});
        for (auto& c : chans)
        {
            c.ring.assign ((size_t) n, 0.0f);
            c.out.assign ((size_t) hop, 0.0f);
            c.ringPos = 0;
            c.gathered = 0;
            c.outPos = 0;
        }

        // Identity until told otherwise: a flat EQ must pass audio from the
        // first block, not after the first parameter touch.
        setMagnitude ([] (int, int) { return 1.0f; });
        resetStreams();
    }

    bool isReady() const { return fft != nullptr; }

    int latencySamples() const { return hop + (taps - 1) / 2; }
    int fftSize() const { return n; }
    int firTaps() const { return taps; }

    // ── the filter ────────────────────────────────────────────────────────
    //
    // magnitudeAt is sampled at bin frequencies k/N in normalised terms; the
    // caller maps bins to Hz. Zero phase in, linear phase out: the FIR is the
    // symmetric response centred at (taps-1)/2, so its spectrum carries the
    // one delay the whole engine reports.
    void setMagnitude (const std::function<float (int bin, int fftSizeN)>& magnitudeAt)
    {
        if (! isReady()) return;
        designSpectrum (spectrum, magnitudeAt, true);
        matrixOn = false;
    }

    // The 2x2 form (QA-EqPro SC-3): per-band domain routing (mid / side /
    // left / right) is a matrix operation in the L/R domain, not a pair of
    // independent curves.  With per-bin domain products st, l, r, m, s the
    // caller composes (L/R-domain diagonal first, then the M/S stage folded
    // back through the encode):
    //   LL = ((m+s)/2)*st*l   LR = ((m-s)/2)*st*r
    //   RL = ((m-s)/2)*st*l   RR = ((m+s)/2)*st*r
    // Cross terms are SIGNED - (m-s)/2 goes negative when side exceeds mid -
    // which is still a real even spectrum and a legal zero-phase FIR, so the
    // designer must not clamp them.
    void setMagnitudeMatrix (const std::function<float (int bin, int fftSizeN)>& ll,
                             const std::function<float (int bin, int fftSizeN)>& lr,
                             const std::function<float (int bin, int fftSizeN)>& rl,
                             const std::function<float (int bin, int fftSizeN)>& rr)
    {
        if (! isReady()) return;
        designSpectrum (specLL, ll, false);
        designSpectrum (specLR, lr, false);
        designSpectrum (specRL, rl, false);
        designSpectrum (specRR, rr, false);
        matrixOn = true;
    }

    bool isMatrixOn() const { return matrixOn; }

    // Convenience for a flat curve without building a lambda at the call site.
    void setIdentity() { setMagnitude ([] (int, int) { return 1.0f; }); }

    // Overload taking plain magnitudes-by-bin, the shape the EQ produces.
    void setMagnitude (const std::function<float (float)>& magAtHz, double sampleRate)
    {
        const double binHz = sampleRate / (double) n;
        setMagnitude ([&] (int bin, int) { return magAtHz ((float) (bin * binHz)); });
    }

    // ── audio ─────────────────────────────────────────────────────────────
    void resetStreams()
    {
        for (auto& c : chans)
        {
            std::fill (c.ring.begin(), c.ring.end(), 0.0f);
            std::fill (c.out.begin(), c.out.end(), 0.0f);
            c.ringPos = 0;
            c.gathered = 0;
            c.outPos = 0;
        }
    }

    void process (int channel, float* data, int numSamples)
    {
        if (! isReady() || channel < 0 || channel >= (int) chans.size()) return;
        auto& c = chans[(size_t) channel];

        for (int i = 0; i < numSamples; ++i)
        {
            c.ring[(size_t) c.ringPos] = data[i];
            c.ringPos = (c.ringPos + 1) % n;

            data[i] = c.out[(size_t) c.outPos];
            c.out[(size_t) c.outPos] = 0.0f;

            if (++c.gathered >= hop)
            {
                c.gathered = 0;
                convolveFrame (c);
            }
            c.outPos = (c.outPos + 1) % hop;
        }
    }

    // Stereo entry: the matrix path needs both channels in one call (each
    // output frame mixes both input transforms).  Without a matrix installed
    // it is exactly the two independent per-channel streams.
    void processStereo (float* l, float* r, int numSamples)
    {
        if (! isReady() || chans.size() < 2) return;
        if (! matrixOn)
        {
            process (0, l, numSamples);
            process (1, r, numSamples);
            return;
        }

        auto& c0 = chans[0];
        auto& c1 = chans[1];
        for (int i = 0; i < numSamples; ++i)
        {
            c0.ring[(size_t) c0.ringPos] = l[i];
            c1.ring[(size_t) c1.ringPos] = r[i];
            c0.ringPos = (c0.ringPos + 1) % n;
            c1.ringPos = (c1.ringPos + 1) % n;

            l[i] = c0.out[(size_t) c0.outPos];
            r[i] = c1.out[(size_t) c1.outPos];
            c0.out[(size_t) c0.outPos] = 0.0f;
            c1.out[(size_t) c1.outPos] = 0.0f;

            // The two streams advance in lockstep by construction (this is
            // the only entry that feeds them in matrix mode), so channel 0's
            // gather counter times the shared frame.
            if (++c0.gathered >= hop)
            {
                c0.gathered = c1.gathered = 0;
                convolveMatrixFrame();
            }
            c0.outPos = (c0.outPos + 1) % hop;
            c1.outPos = (c1.outPos + 1) % hop;
        }
    }

private:
    struct Channel
    {
        std::vector<float> ring;   // last n input samples
        std::vector<float> out;    // next hop output samples
        int ringPos = 0, gathered = 0, outPos = 0;
    };

    // Overlap-save: transform the last n inputs, multiply, keep the final hop
    // samples of the result - the region a taps-long FIR cannot wrap into.
    void convolveFrame (Channel& c)
    {
        for (int i = 0; i < n; ++i)
        {
            const int src = (c.ringPos + i) % n;    // oldest first
            scratch[(size_t) i] = { c.ring[(size_t) src], 0.0f };
        }

        fft->transform (scratch.data(), false);
        for (int i = 0; i < n; ++i)
            scratch[(size_t) i] *= spectrum[(size_t) i];
        fft->transform (scratch.data(), true);

        // The last hop samples are valid linear convolution. The read head has
        // already consumed out[outPos] this tick and will advance before the
        // next read, so the oldest valid sample goes at outPos + 1 - the first
        // position the head meets - and the newest lands back on outPos, to be
        // read a full hop from now. One position of rotation, and exactly the
        // kind of off-by-one the latency test exists to catch.
        for (int i = 0; i < hop; ++i)
            c.out[(size_t) ((c.outPos + 1 + i) % hop)] = scratch[(size_t) (n - hop + i)].real();
    }

    // The one design pipeline: sample the curve on the grid, make it
    // symmetric about Nyquist (real even spectrum -> real zero-phase impulse
    // response), rotate the centre of symmetry to (taps-1)/2, shape by a
    // Kaiser (beta 8: about -80 dB sidelobes, transition narrow enough that a
    // 24 dB/oct curve survives visibly intact on the smallest size), and
    // transform the windowed FIR into its spectrum.  clampNonNegative is for
    // plain magnitude curves; matrix cross terms carry legitimate signs.
    void designSpectrum (std::vector<std::complex<float>>& dest,
                         const std::function<float (int bin, int fftSizeN)>& at,
                         bool clampNonNegative)
    {
        for (int k = 0; k <= n / 2; ++k)
        {
            float m = at (k, n);
            if (clampNonNegative) m = std::max (0.0f, m);
            design[(size_t) k] = { m, 0.0f };
            if (k > 0 && k < n / 2)
                design[(size_t) (n - k)] = { m, 0.0f };
        }

        fft->transform (design.data(), true);   // -> impulse response at t=0

        const int half = (taps - 1) / 2;
        std::fill (scratch.begin(), scratch.end(), std::complex<float> { 0.0f, 0.0f });

        for (int i = 0; i < taps; ++i)
        {
            const int src = (i - half + n) % n;       // ..., n-2, n-1, 0, 1, ...
            const double r = (double) (i - half) / (double) std::max (1, half);
            const double w = besselI0 (8.0 * std::sqrt (std::max (0.0, 1.0 - r * r)))
                           / besselI0 (8.0);
            scratch[(size_t) i] = { design[(size_t) src].real() * (float) w, 0.0f };
        }

        fft->transform (scratch.data(), false);
        std::copy (scratch.begin(), scratch.end(), dest.begin());
    }

    // The 2x2 frame: both input transforms feed both output frames.  Same
    // overlap-save discipline as convolveFrame - keep the last hop, same
    // one-position output rotation.
    void convolveMatrixFrame()
    {
        auto& c0 = chans[0];
        auto& c1 = chans[1];

        for (int i = 0; i < n; ++i)
        {
            scratch [(size_t) i] = { c0.ring[(size_t) ((c0.ringPos + i) % n)], 0.0f };
            scratchB[(size_t) i] = { c1.ring[(size_t) ((c1.ringPos + i) % n)], 0.0f };
        }

        fft->transform (scratch.data(),  false);   // X_L
        fft->transform (scratchB.data(), false);   // X_R

        for (int i = 0; i < n; ++i)
        {
            const auto xl = scratch[(size_t) i];
            const auto xr = scratchB[(size_t) i];
            outA[(size_t) i] = specLL[(size_t) i] * xl + specLR[(size_t) i] * xr;
            outB[(size_t) i] = specRL[(size_t) i] * xl + specRR[(size_t) i] * xr;
        }

        fft->transform (outA.data(), true);
        fft->transform (outB.data(), true);

        for (int i = 0; i < hop; ++i)
        {
            c0.out[(size_t) ((c0.outPos + 1 + i) % hop)] = outA[(size_t) (n - hop + i)].real();
            c1.out[(size_t) ((c1.outPos + 1 + i) % hop)] = outB[(size_t) (n - hop + i)].real();
        }
    }

    static double besselI0 (double x)
    {
        double s = 1.0, t = 1.0;
        for (int i = 1; i < 24; ++i)
        {
            t *= (x / (2.0 * i)) * (x / (2.0 * i));
            s += t;
            if (t < 1.0e-13 * s) break;
        }
        return s;
    }

    std::unique_ptr<FFT> fft;
    int order = 0, n = 0, taps = 0, hop = 0;

    std::vector<std::complex<float>> spectrum, scratch, design;
    std::vector<std::complex<float>> specLL, specLR, specRL, specRR;   // matrix mode
    std::vector<std::complex<float>> scratchB, outA, outB;
    bool matrixOn = false;
    std::vector<Channel> chans;
};

} // namespace kbs
