// KBS Plugins - oversampling for the nonlinear stages.
//
// A waveshaper generates harmonics above the input's highest frequency. Any of
// them that land above Nyquist do not vanish; they fold back down and appear as
// inharmonic tones underneath the note, which is what makes a cheap distortion
// sound gritty in a way that has nothing to do with the distortion curve. The
// higher the gain, the further up the harmonics reach and the more of them fold.
//
// Running the shaper at a multiple of the rate moves Nyquist out to where the
// harmonics that matter still fit, and the decimation filter removes what is
// left before folding can happen. Everything linear - filters, tone stacks,
// gain - stays at the base rate, because a linear stage generates nothing new
// and oversampling it only costs CPU.
//
// Polyphase, so the zero-stuffed samples are never actually multiplied by zero:
// each output phase uses only the coefficients that would have hit a real
// sample, which is where the factor-of-L saving comes from.
#pragma once

#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

namespace kbs {

template <int Factor>
class Oversampler
{
public:
    static constexpr int kFactor = Factor;
    // Sixteen taps per phase whatever the factor. A fixed 64-tap prototype
    // would leave only eight per phase at 8x, and the transition band would be
    // too wide to stop the fold it was added to prevent.
    static constexpr int kTaps = 16 * Factor;
    static constexpr int kPerPhase = kTaps / Factor;

    static_assert (Factor == 2 || Factor == 4 || Factor == 8, "2x, 4x or 8x");
    static_assert (kTaps % Factor == 0, "prototype must divide evenly into phases");

    void prepare (int maxBlockSize)
    {
        buildPrototype();

        up.assign ((size_t) (maxBlockSize * Factor + 8), 0.0f);
        histUp.assign ((size_t) kPerPhase, 0.0f);
        histDown.assign ((size_t) kTaps, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (histUp.begin(), histUp.end(), 0.0f);
        std::fill (histDown.begin(), histDown.end(), 0.0f);
        std::fill (up.begin(), up.end(), 0.0f);
        posUp = 0;
        posDown = 0;
    }

    // Both filters contribute their group delay at the oversampled rate, so the
    // round trip costs (kTaps - 1) oversampled samples, or that over the factor
    // at the rate the host sees.
    static constexpr int latencySamples() { return (kTaps - 1) / Factor; }

    float* upsampleBuffer() { return up.data(); }

    // Returns the number of oversampled samples written.
    int upsample (const float* in, int numSamples)
    {
        if ((int) up.size() < numSamples * Factor)
            up.assign ((size_t) (numSamples * Factor + 8), 0.0f);

        for (int i = 0; i < numSamples; ++i)
        {
            histUp[(size_t) posUp] = in[i];
            posUp = (posUp + 1) % kPerPhase;

            for (int p = 0; p < Factor; ++p)
            {
                float acc = 0.0f;
                for (int t = 0; t < kPerPhase; ++t)
                {
                    const int h = (posUp - 1 - t + kPerPhase * 2) % kPerPhase;
                    acc += histUp[(size_t) h] * proto[(size_t) (p + t * Factor)];
                }
                // Zero-stuffing divides the signal's energy across L slots, so
                // the interpolator has to put the factor back.
                up[(size_t) (i * Factor + p)] = acc * (float) Factor;
            }
        }
        return numSamples * Factor;
    }

    // Filters the oversampled stream and keeps every Factor-th sample. The
    // filter output only has to be computed once per kept sample, which is the
    // other half of the polyphase saving.
    void downsample (const float* osIn, float* out, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            for (int p = 0; p < Factor; ++p)
            {
                histDown[(size_t) posDown] = osIn[i * Factor + p];
                posDown = (posDown + 1) % kTaps;
            }

            float acc = 0.0f;
            for (int t = 0; t < kTaps; ++t)
            {
                const int h = (posDown - 1 - t + kTaps * 2) % kTaps;
                acc += histDown[(size_t) h] * proto[(size_t) t];
            }
            out[i] = acc;
        }
    }

private:
    // Windowed sinc at the base rate's Nyquist, which is 1/(2*Factor) of the
    // oversampled rate. Kaiser beta 9 puts the stopband near -90 dB. That is
    // the filter's limit, not the system's: a shaper hard enough that its
    // harmonics are still strong above the oversampled Nyquist will fold
    // whatever the stopband is, which is why the most extreme voicings measure
    // worse than the gentle ones no matter how high the factor goes.
    void buildPrototype()
    {
        const double fc = 0.5 / (double) Factor;
        const double centre = (double) (kTaps - 1) * 0.5;
        double sum = 0.0;

        for (int i = 0; i < kTaps; ++i)
        {
            const double x = (double) i - centre;
            const double s = x == 0.0 ? 2.0 * fc
                                      : std::sin (2.0 * kPiD * fc * x) / (kPiD * x);

            const double r = 2.0 * (double) i / (double) (kTaps - 1) - 1.0;
            const double w = besselI0 (9.0 * std::sqrt (std::max (0.0, 1.0 - r * r)))
                           / besselI0 (9.0);

            proto[(size_t) i] = (float) (s * w);
            sum += s * w;
        }

        // Unity DC gain, so oversampling never changes the level - a shaper that
        // sounds louder with oversampling on is impossible to A/B honestly.
        const float g = (float) (1.0 / sum);
        for (auto& c : proto) c *= g;
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

    static constexpr double kPiD = 3.14159265358979323846;

    std::array<float, kTaps> proto {};
    std::vector<float> up, histUp, histDown;
    int posUp = 0, posDown = 0;
};

// ── a shaper that runs oversampled, one sample at a time ───────────────────
//
// The block-at-a-time form above cannot be used inside a feedback loop, where
// each sample depends on the previous output. This does the same job per
// sample: lift one input to Factor, shape each of them, decimate back to one.
// It costs the same arithmetic per sample as the block form and works anywhere.
template <int Factor>
class OversampledShaper
{
public:
    void prepare() { os.prepare (1); }
    void reset() { os.reset(); }

    static constexpr int latencySamples() { return Oversampler<Factor>::latencySamples(); }

    template <typename Fn>
    inline float process (float x, Fn&& shape) noexcept
    {
        os.upsample (&x, 1);
        float* u = os.upsampleBuffer();
        for (int i = 0; i < Factor; ++i)
            u[i] = shape (u[i]);

        float out = 0.0f;
        os.downsample (u, &out, 1);
        return std::isfinite (out) ? out : 0.0f;
    }

private:
    Oversampler<Factor> os;
};

} // namespace kbs
