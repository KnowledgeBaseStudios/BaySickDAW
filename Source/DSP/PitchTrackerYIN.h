#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// PitchTrackerYIN — Phase H-4 (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────
// Autocorrelation-based pitch tracker (YIN by de Cheveigné & Kawahara 2002,
// with the standard CMNDF + threshold + parabolic interpolation refinements).
// Reused by:
//   * H-5 BaySickVocal pitch correction (drives the realtime + offline retune)
//   * I-10 BaySickPedals TU-3 tuner pedal
//
// Threading model:
//   * Audio thread calls pushAudio() each block to feed mono samples into a
//     lock-free SPSC ring buffer (juce::AbstractFifo).  Cheap.  No analysis.
//   * Background worker thread polls the ring at ~5 ms intervals, slides a
//     2048-sample analysis window forward by hop=512 each iteration, runs
//     YIN on the window, atomically publishes the detected fundamental Hz +
//     confidence (0..1).
//   * UI / DSP consumers call getFrequencyHz() / getConfidence() — wait-free
//     atomic reads, no locks.
//
// Latency:
//   Window 2048 / SR 44.1 kHz = ~46 ms analysis window + worker poll interval
//   (~5 ms) = ~50 ms before a fresh pitch reading is published.  Acceptable
//   for tuners (invisible) and for pitch correction where the retune effect
//   itself runs through a faster hop=256 PhaseVocoder (the pitch reading is
//   slightly stale but the correction is sample-accurate within its hop).
//
// Range:
//   kMinTau = 32  -> ~1378 Hz max (above F6 / two octaves above middle C)
//   kMaxTau = 1000 -> ~44 Hz min  (below E1 / well below low bass guitar)
//   At 44.1 kHz; scales with sample rate via prepare().
// ─────────────────────────────────────────────────────────────────────────────

class PitchTrackerYIN
{
public:
    static constexpr int   kWindowSize        = 2048;
    static constexpr int   kHopSize           = 512;
    static constexpr float kYinThreshold      = 0.15f;   // CMNDF detection threshold
    static constexpr float kMinFreqHz         = 40.0f;
    static constexpr float kMaxFreqHz         = 1500.0f;

    PitchTrackerYIN();
    ~PitchTrackerYIN();

    // Set sample rate + start the worker thread.  Idempotent — calling again
    // restarts the worker at the new rate.
    void prepare (double sampleRate);

    // Stop worker + free internal state.  prepare() must be called again
    // before pushAudio() resumes.
    void releaseResources();

    // Audio-thread call.  Pushes a mono block of samples into the ring.
    // For stereo input, callers should average L+R into a mono buffer first.
    void pushAudio (const float* mono, int numSamples) noexcept;

    // Convenience overload — averages stereo L+R to mono internally.
    void pushAudio (const float* left, const float* right, int numSamples) noexcept;

    // Wait-free reads (any thread).  0 Hz published when no pitch detected
    // (silent / unpitched / below-confidence-threshold input).
    float getFrequencyHz()  const noexcept { return mFreqHz.load (std::memory_order_acquire); }
    float getConfidence()   const noexcept { return mConfidence.load (std::memory_order_acquire); }

    // Reset internal state (clears ring buffer + zeroes published values).
    void reset() noexcept;

private:
    // Background worker — pulls samples from the ring, runs YIN on rolling
    // analysis window, publishes results.  See .cpp for run() body.
    class WorkerThread;
    std::unique_ptr<WorkerThread> mWorker;

    // Lock-free ring buffer (audio thread writes, worker thread reads).
    // Sized to 4 * kWindowSize so a slow worker can't backpressure the
    // audio thread; oldest-data-overruns are visible as a momentary stale
    // pitch reading (worker just continues from current ring position).
    static constexpr int kRingSize = kWindowSize * 4;
    juce::AbstractFifo  mFifo { kRingSize };
    std::vector<float>  mFifoBuf;

    // Published outputs — atomic for wait-free cross-thread reads.
    std::atomic<float>  mFreqHz     { 0.0f };
    std::atomic<float>  mConfidence { 0.0f };

    double mSampleRate { 44100.0 };
};
