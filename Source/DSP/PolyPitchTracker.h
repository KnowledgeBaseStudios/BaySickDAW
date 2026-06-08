#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// PolyPitchTracker - QA-EffectsReview Task 4 (2026-06-06)
// ─────────────────────────────────────────────────────────────────────────────
// Polyphonic pitch tracker for the SY-1 synth's poly mode.  Pipeline:
//   FFT magnitude spectrum -> harmonic-sum scoring over a cent-spaced f0 grid
//   -> greedy iterative spectral subtraction to pull out up to kMaxNotes
//   simultaneous fundamentals (strongest first).
//
// Threading mirrors PitchTrackerYIN exactly:
//   * Audio thread: pushAudio() writes mono samples into a lock-free SPSC ring
//     (juce::AbstractFifo).  Cheap, no analysis.
//   * Background worker: slides a 4096-sample window forward by hop=1024, runs
//     a windowed FFT + the detector, and publishes the NoteSet via a seqlock.
//   * Audio / UI consumers call getNotes() - a wait-free seqlock read.
//
// Reality: real-time polyphonic detection from a summed audio input is
// inherently approximate - solid on sustained, cleanly-played chords/intervals,
// best-effort on fast/dense/percussive playing.  Mono mode (PitchTrackerYIN) is
// preferred for single-note lines + Bass.
// ─────────────────────────────────────────────────────────────────────────────

class PolyPitchTracker
{
public:
    static constexpr int kFftOrder   = 12;             // FFT size = 2^12
    static constexpr int kWindowSize = 1 << kFftOrder; // 4096 (~93 ms @ 44.1k)
    static constexpr int kHopSize    = 1024;
    static constexpr int kMaxNotes   = 6;

    struct NoteSet
    {
        int   count { 0 };
        float freqs[kMaxNotes] { 0, 0, 0, 0, 0, 0 };   // Hz, strongest first
    };

    PolyPitchTracker();
    ~PolyPitchTracker();

    // Start/stop the worker (idempotent restart at the new rate).
    void prepare (double sampleRate);
    void releaseResources();
    void reset() noexcept;

    // Audio-thread: push a mono block into the ring.
    void pushAudio (const float* mono, int numSamples) noexcept;

    // Wait-free read of the latest detected note set.
    NoteSet getNotes() const noexcept;

    // Detection frequency range (Hz); set per instrument (Guitar vs Bass).
    void setFreqRange (float minHz, float maxHz) noexcept
    {
        mMinHz.store (minHz, std::memory_order_relaxed);
        mMaxHz.store (maxHz, std::memory_order_relaxed);
    }
    // Cap simultaneous notes (Bass uses 1-2).
    void setMaxNotes (int n) noexcept
    {
        mMaxNotesReq.store (juce::jlimit (1, kMaxNotes, n), std::memory_order_relaxed);
    }

private:
    class WorkerThread;
    std::unique_ptr<WorkerThread> mWorker;

    static constexpr int kRingSize = kWindowSize * 4;
    juce::AbstractFifo mFifo { kRingSize };
    std::vector<float> mFifoBuf;

    // Seqlock-published note set: worker writes between two odd/even seq bumps;
    // readers retry on a torn read (wait-free; never blocks the audio thread).
    std::atomic<uint32_t> mSeq { 0 };
    NoteSet               mShared;   // guarded by mSeq

    std::atomic<float> mMinHz       { 70.0f };
    std::atomic<float> mMaxHz       { 1200.0f };
    std::atomic<int>   mMaxNotesReq { kMaxNotes };

    double mSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolyPitchTracker)
};
