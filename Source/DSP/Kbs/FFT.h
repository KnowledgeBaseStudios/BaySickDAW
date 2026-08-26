// KBS Plugins - a small radix-2 FFT.
//
// JUCE has one, and it is faster. This exists because everything else in Core
// builds and tests without JUCE, and the pitch engine is the one piece of DSP
// in the bundle where a unit test is worth more than a few percent of CPU: a
// phase vocoder that is subtly wrong still produces plausible audio, so ear
// checks do not catch it. Being able to assert that a known sine round-trips to
// the right bin, at the right magnitude, is what makes the rest verifiable.
#pragma once

#include <vector>
#include <complex>
#include <cmath>
#include <cstddef>

namespace kbs {

inline constexpr double kTwoPi = 6.283185307179586476925286766559;

class FFT
{
public:
    explicit FFT (int order) : ord (order), n (1 << order)
    {
        tw.resize ((size_t) n / 2);
        for (int i = 0; i < n / 2; ++i)
        {
            const double a = -kTwoPi * (double) i / (double) n;
            tw[(size_t) i] = { (float) std::cos (a), (float) std::sin (a) };
        }

        rev.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            int x = i, r = 0;
            for (int b = 0; b < ord; ++b) { r = (r << 1) | (x & 1); x >>= 1; }
            rev[(size_t) i] = r;
        }
    }

    int size() const { return n; }

    // In-place, decimation in time. `inverse` conjugates the twiddles and
    // scales by 1/N, so forward-then-inverse is the identity.
    void transform (std::complex<float>* d, bool inverse) const
    {
        for (int i = 0; i < n; ++i)
            if (i < rev[(size_t) i])
                std::swap (d[i], d[rev[(size_t) i]]);

        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2;
            const int step = n / len;
            for (int i = 0; i < n; i += len)
            {
                for (int j = 0; j < half; ++j)
                {
                    auto w = tw[(size_t) (j * step)];
                    if (inverse) w = std::conj (w);
                    const auto u = d[i + j];
                    const auto v = d[i + j + half] * w;
                    d[i + j] = u + v;
                    d[i + j + half] = u - v;
                }
            }
        }

        if (inverse)
        {
            const float s = 1.0f / (float) n;
            for (int i = 0; i < n; ++i) d[i] *= s;
        }
    }

private:
    int ord, n;
    std::vector<std::complex<float>> tw;
    std::vector<int> rev;
};

} // namespace kbs
