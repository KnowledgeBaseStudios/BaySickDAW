#include "TruePeakMeter.h"
#include <cmath>

namespace
{
    // Zeroth-order modified Bessel function of the first kind -- the Kaiser
    // window's shaping term.  Series form converges in ~20 terms for beta <= 12.
    double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        const double halfSq = (x * 0.5) * (x * 0.5);
        for (int k = 1; k < 40; ++k)
        {
            term *= halfSq / (double) (k * k);
            sum  += term;
            if (term < 1.0e-16 * sum) break;
        }
        return sum;
    }

    // CALIBRATION (Rule 6 cat. 5): cutoff sits at 0.92 of the base-rate Nyquist
    // rather than exactly at it.  With only 12 taps per phase the transition band
    // is wide, and pushing the cutoff to Nyquist trades stopband attenuation for
    // passband that carries no audible content -- 20.3 kHz..22.05 kHz at 44.1 k.
    // 0.92 keeps the response flat through 20 kHz at 44.1/48/96 k while holding
    // the stopband under -80 dB, which is what the true-peak estimate needs.
    constexpr double kCutoffFractionOfNyquist = 0.92;

    // Kaiser beta 8.0 -> ~-80 dB stopband.  Higher beta widens the transition
    // faster than 12 taps per phase can absorb.
    constexpr double kKaiserBeta = 8.0;
}

const TruePeakMeter::Bank& TruePeakMeter::bank()
{
    // Built once, on first use, and const thereafter -- so concurrent readers on
    // the audio and render threads share it without synchronisation.  C++11
    // guarantees the initialisation itself is thread-safe.
    static const Bank b = []
    {
        constexpr int kTotalTaps = kPhases * kTapsPerPhase;   // 48

        // Prototype: windowed sinc lowpass at the 4x rate, gain kPhases so each
        // polyphase branch ends up with unity DC gain.
        std::array<double, kTotalTaps> proto {};
        const double centre = (double) (kTotalTaps - 1) * 0.5;
        // Normalised cutoff as a fraction of the 4x-rate Nyquist.
        const double fc = kCutoffFractionOfNyquist / (double) kPhases;
        const double i0Beta = besselI0 (kKaiserBeta);

        for (int n = 0; n < kTotalTaps; ++n)
        {
            const double t = (double) n - centre;
            const double sinc = (std::abs (t) < 1.0e-12)
                                  ? 1.0
                                  : std::sin (juce::MathConstants<double>::pi * fc * t)
                                      / (juce::MathConstants<double>::pi * fc * t);

            // Kaiser window over the full prototype length.
            const double r = t / centre;                       // -1 .. +1
            const double arg = 1.0 - juce::jlimit (-1.0, 1.0, r) * juce::jlimit (-1.0, 1.0, r);
            const double win = besselI0 (kKaiserBeta * std::sqrt (juce::jmax (0.0, arg))) / i0Beta;

            proto[(size_t) n] = fc * sinc * win;
        }

        // Split into phases and normalise each branch to unity DC gain.  Doing it
        // per branch rather than globally is what stops a constant input from
        // wobbling between phases, which would read as inter-sample peak that is
        // not there.
        Bank out {};
        for (int p = 0; p < kPhases; ++p)
        {
            double sum = 0.0;
            for (int k = 0; k < kTapsPerPhase; ++k)
                sum += proto[(size_t) (k * kPhases + p)];

            const double norm = (std::abs (sum) > 1.0e-12) ? (1.0 / sum) : 1.0;
            for (int k = 0; k < kTapsPerPhase; ++k)
                out[(size_t) p][(size_t) k] = (float) (proto[(size_t) (k * kPhases + p)] * norm);
        }
        return out;
    }();

    return b;
}

void TruePeakMeter::prepare (int numChannels)
{
    const int n = juce::jmax (1, numChannels);
    mHist   .assign ((size_t) n, {});
    mHistPos.assign ((size_t) n, 0);
    mMaxLin = 0.0f;
}

void TruePeakMeter::reset() noexcept
{
    for (auto& h : mHist) h.fill (0.0f);
    for (auto& p : mHistPos) p = 0;
    mMaxLin = 0.0f;
}

// Pushes one input sample into the channel's history and returns the largest
// magnitude across the four interpolated phase outputs.
float TruePeakMeter::pushAndPeak (int ch, float x) noexcept
{
    auto& hist = mHist[(size_t) ch];
    int&  pos  = mHistPos[(size_t) ch];

    hist[(size_t) pos] = x;
    pos = (pos + 1) % kTapsPerPhase;

    const auto& b = bank();
    float peak = 0.0f;

    for (int p = 0; p < kPhases; ++p)
    {
        float acc = 0.0f;
        // Walk history newest-first: index (pos - 1 - k) modulo the ring.
        int idx = pos - 1;
        for (int k = 0; k < kTapsPerPhase; ++k)
        {
            if (idx < 0) idx += kTapsPerPhase;
            acc += b[(size_t) p][(size_t) k] * hist[(size_t) idx];
            --idx;
        }
        peak = juce::jmax (peak, std::abs (acc));
    }
    return peak;
}

void TruePeakMeter::processChannel (int ch, const float* src, int numSamples) noexcept
{
    if (src == nullptr) return;
    if (ch < 0 || ch >= (int) mHist.size()) return;

    float localMax = mMaxLin;
    for (int i = 0; i < numSamples; ++i)
        localMax = juce::jmax (localMax, pushAndPeak (ch, src[i]));
    mMaxLin = localMax;
}

void TruePeakMeter::process (const juce::AudioBuffer<float>& buf) noexcept
{
    const int chans = juce::jmin (buf.getNumChannels(), (int) mHist.size());
    for (int c = 0; c < chans; ++c)
        processChannel (c, buf.getReadPointer (c), buf.getNumSamples());
}
