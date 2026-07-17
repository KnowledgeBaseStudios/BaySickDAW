#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// MonitorPitchShifter -- QA-Fe2 monitor-latency fix (docket 2a, 2026-07-16)
// ─────────────────────────────────────────────────────────────────────────────
// Low-latency time-domain pitch shifter for the CORRECTED LIVE MONITOR path.
// Dual-tap delay-line crossfade -- the classic live-tuner / harmonizer-pedal
// algorithm: two read taps sweep a short window behind the write head at the
// shift ratio, each tap gained by a half-sine that peaks mid-window and hits
// zero exactly where that tap wraps, so splices never click.  Effective
// monitoring latency = the gain-weighted mean tap delay = half the window
// (~12 ms at the 24 ms default) vs ~48 ms for the spectral R3 LiveShifter,
// which keeps the WET-recording path untouched.  No formant handling --
// artifacts (slight doubling on large shifts) are negligible at the
// cents-scale corrections this stage applies.
// Mono, audio-thread only, allocation-free after prepare().
class MonitorPitchShifter
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        // 24 ms window: below ~20 ms the splice rate beats against low male
        // fundamentals (~100 Hz); above ~30 ms the tap doubling reads as slap.
        mWindow = juce::jmax (64, (int) std::lround (sampleRate * 0.024));
        int cap = 1;
        while (cap < mWindow * 2 + juce::jmax (1, maxBlockSize) + 8) cap <<= 1;
        mRing.assign ((size_t) cap, 0.0f);
        mMask = cap - 1;
        reset();
    }

    void reset()
    {
        std::fill (mRing.begin(), mRing.end(), 0.0f);
        mWrite = 0;
        mPhase = 0.0;
    }

    int getLatencySamples() const noexcept { return mWindow / 2; }

    // In-place mono shift.  ratio = output pitch / input pitch.
    void process (float* buf, int n, float ratio) noexcept
    {
        if (mRing.empty() || buf == nullptr) return;

        const double drift = (1.0 - (double) ratio) / (double) mWindow;
        for (int i = 0; i < n; ++i)
        {
            mRing[(size_t) mWrite] = buf[i];

            mPhase += drift;
            mPhase -= std::floor (mPhase);                    // wrap to [0,1)
            const double p2 = mPhase + 0.5 >= 1.0 ? mPhase - 0.5 : mPhase + 0.5;

            const float g1 = std::sin ((float) mPhase * juce::MathConstants<float>::pi);
            const float g2 = std::sin ((float) p2     * juce::MathConstants<float>::pi);

            buf[i] = g1 * readTap (mPhase) + g2 * readTap (p2);
            mWrite = (mWrite + 1) & mMask;   // wrap the accumulator itself, not just reads
        }
    }

private:
    float readTap (double phase) const noexcept
    {
        const double pos  = (double) mWrite - phase * (double) mWindow;
        const int    i0   = (int) std::floor (pos);
        const float  frac = (float) (pos - (double) i0);
        // Two's-complement AND wraps negative indices into [0, cap) because
        // mMask is all-ones (power-of-two ring).
        const float a = mRing[(size_t) (i0       & mMask)];
        const float b = mRing[(size_t) ((i0 + 1) & mMask)];
        return a + frac * (b - a);
    }

    std::vector<float> mRing;
    int    mMask   { 0 };
    int    mWrite  { 0 };
    int    mWindow { 1024 };
    double mPhase  { 0.0 };

    JUCE_LEAK_DETECTOR (MonitorPitchShifter)
};
