#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// PitchTrackerYIN - Phase H-4 (2026-05-01)
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
//   * UI / DSP consumers call getFrequencyHz() / getConfidence() - wait-free
//     atomic reads, no locks.
//
// Latency:
//   Window 2048 / SR 44.1 kHz = ~46 ms analysis window + worker poll interval
//   (~5 ms) = ~50 ms before a fresh pitch reading is published.  Acceptable
//   for tuners (invisible) and for pitch correction where the retune effect
//   itself runs through a faster hop=256 PhaseVocoder (the pitch reading is
//   slightly stale but the correction is sample-accurate within its hop).
//
// Range (at 44.1 kHz; scales with sample rate):
//   kMaxFreqHz 4186 -> tauMin ~10 -> C8 top (global).
//   Analysis window is PER-INSTANCE (mWindowSize): the 2048 default -> ~44 Hz
//   min (Octave / Synth / PitchCorrector, unchanged); the tuner sets 4096 ->
//   ~21.5 Hz min (covers 5-string bass low B ~31 Hz).  The window governs the
//   real minimum.
// ─────────────────────────────────────────────────────────────────────────────

class PitchTrackerYIN
{
public:
    // The analysis window/hop are PER-INSTANCE (mWindowSize/mHopSize +
    // setAnalysisWindow) so the tuner can use a bigger window (bass range)
    // WITHOUT changing the latency of the other consumers (Octave / Synth /
    // PitchCorrector) that share this class and keep the 2048 default.
    static constexpr int   kDefaultWindowSize = 2048;
    static constexpr int   kDefaultHopSize    = 512;
    static constexpr int   kMaxWindowSize     = 4096;    // the tuner's window; the ring is sized for this
    static constexpr float kYinThreshold      = 0.15f;   // CMNDF detection threshold
    static constexpr float kMinFreqHz         = 20.0f;   // global floor; the per-instance window clamps the real min
    static constexpr float kMaxFreqHz         = 4186.0f; // C8 (global; benign for the 2048 consumers)

    PitchTrackerYIN();
    ~PitchTrackerYIN();

    // Set the analysis window + hop BEFORE prepare().  Larger window reaches
    // lower frequencies (bass) at the cost of latency + worker CPU.  Default
    // 2048/512; the tuner uses 4096/1024.  Clamped to kMaxWindowSize.
    void setAnalysisWindow (int windowSize, int hopSize) noexcept;

    // Set sample rate + start the worker thread.  Idempotent - calling again
    // restarts the worker at the new rate.
    void prepare (double sampleRate);

    // Stop worker + free internal state.  prepare() must be called again
    // before pushAudio() resumes.
    void releaseResources();

    // Audio-thread call.  Pushes a mono block of samples into the ring.
    // For stereo input, callers should average L+R into a mono buffer first.
    void pushAudio (const float* mono, int numSamples) noexcept;

    // Convenience overload - averages stereo L+R to mono internally.
    void pushAudio (const float* left, const float* right, int numSamples) noexcept;

    // Wait-free reads (any thread).  0 Hz published when no pitch detected
    // (silent / unpitched / below-confidence-threshold input).
    float getFrequencyHz()  const noexcept { return mFreqHz.load (std::memory_order_acquire); }
    float getConfidence()   const noexcept { return mConfidence.load (std::memory_order_acquire); }

    // Reset internal state (clears ring buffer + zeroes published values).
    void reset() noexcept;

private:
    // Background worker - pulls samples from the ring, runs YIN on rolling
    // analysis window, publishes results.  See .cpp for run() body.
    class WorkerThread;
    std::unique_ptr<WorkerThread> mWorker;

    // Lock-free ring buffer (audio thread writes, worker thread reads).
    // Sized to 4 * kWindowSize so a slow worker can't backpressure the
    // audio thread; oldest-data-overruns are visible as a momentary stale
    // pitch reading (worker just continues from current ring position).
    static constexpr int kRingSize = kMaxWindowSize * 4;
    juce::AbstractFifo  mFifo { kRingSize };
    std::vector<float>  mFifoBuf;

    // Published outputs - atomic for wait-free cross-thread reads.
    std::atomic<float>  mFreqHz     { 0.0f };
    std::atomic<float>  mConfidence { 0.0f };

    double mSampleRate { 44100.0 };
    int    mWindowSize { kDefaultWindowSize };   // per-instance analysis window (<= kMaxWindowSize)
    int    mHopSize    { kDefaultHopSize };
};
