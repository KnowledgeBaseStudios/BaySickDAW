#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

// -- TruePeakMeter --------------------------------------------------------------
// ITU-R BS.1770-4 Annex 2 style true-peak (inter-sample peak) measurement:
// a 4x polyphase FIR interpolator, 48 taps arranged as 4 phases of 12, with the
// peak taken as max|y| across every phase output.  Sample-peak metering misses
// inter-sample overshoot entirely, which is what clips a consumer DAC or a
// lossy transcode even when the file itself reads -0.1 dBFS.
//
// COEFFICIENTS ARE DESIGNED HERE, NOT TRANSCRIBED.  BS.1770-4 Annex 2 publishes
// a specific 48-tap table; this builds an equivalent filter to the same geometry
// (4 phases x 12 taps) from a Kaiser-windowed sinc, because a mis-keyed
// coefficient in a hand-copied table degrades the meter silently and there is no
// way to tell from the output that it happened.  A designed filter is
// self-consistent and its response is checkable from the recipe.  Spec parity:
// flat to 20 kHz at every supported base rate, stopband below -80 dB.
//
// Thread model: one instance per consumer.  process*() is audio/render-thread
// safe and allocation-free after prepare().  The running maximum is a plain
// float -- read it from the SAME thread that feeds it, or via the atomic mirror
// the owner publishes (LimiterDSP does the latter).
// ------------------------------------------------------------------------------
class TruePeakMeter
{
public:
    static constexpr int kPhases       = 4;    // 4x oversampling
    static constexpr int kTapsPerPhase = 12;   // 48 taps total (BS.1770-4 geometry)

    // Setup (message/prepare thread).  Sizes the per-channel history and clears
    // the running maximum.  Safe to call repeatedly.
    void prepare (int numChannels);

    // Clears history + the running maximum without reallocating.
    void reset() noexcept;

    // Clears ONLY the running maximum -- keeps filter history, so a metering
    // window can restart mid-stream without a discontinuity at the seam.
    void resetPeak() noexcept { mMaxLin = 0.0f; }

    // Feed a block.  Accumulates into the running maximum across all channels
    // present (up to the prepared count).
    void process (const juce::AudioBuffer<float>& buf) noexcept;

    float truePeakLinear() const noexcept { return mMaxLin; }
    float truePeakDb()     const noexcept
        { return juce::Decibels::gainToDecibels (mMaxLin, -144.0f); }

private:
    // Feed one channel.  Private because process() is the only caller -- a
    // per-channel entry point with no user would be dead surface.
    void processChannel (int ch, const float* src, int numSamples) noexcept;

    // Polyphase coefficient bank, built once on first use.  [phase][tap].
    using Bank = std::array<std::array<float, kTapsPerPhase>, kPhases>;
    static const Bank& bank();

    // Per-channel input history ring (kTapsPerPhase deep).
    std::vector<std::array<float, kTapsPerPhase>> mHist;
    std::vector<int>                              mHistPos;
    float                                         mMaxLin { 0.0f };

    float pushAndPeak (int ch, float x) noexcept;
};
