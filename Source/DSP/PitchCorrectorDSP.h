#pragma once
#include <JuceHeader.h>
#include "PitchTrackerYIN.h"
#include <array>
#include <atomic>
#include <memory>
#include <vector>

// QA-Fe Task 4: the real-time pitch engine is Rubber Band's LiveShifter,
// forward-declared here; the concrete header is included only in the .cpp.
// PitchCorrectorDSP is standalone-only, where BaySickRubberBand is always
// linked (BAYSICK_HAS_RUBBERBAND).
namespace RubberBand { class RubberBandLiveShifter; }

// ─────────────────────────────────────────────────────────────────────────────
// PitchCorrectorDSP - realtime vocal pitch correction (QA-Fe Task 4, 2026-07-14)
// ─────────────────────────────────────────────────────────────────────────────
// Realtime auto-tune wrapper for BaySickVocal.  Owns:
//   * PitchTrackerYIN for fundamental detection (40-1500 Hz, ~50 ms latency).
//   * RubberBand::RubberBandLiveShifter for the audio path
//     (OptionFormantPreserved | OptionWindowShort, ~48 ms latency).  Replaces
//     the retired TD-PSOLA + cepstral-formant path: PSOLA's moire warble was
//     worst at the small corrections this stage makes (QA-Fe engine pivot).
//
// Algorithm:
//   1. Audio thread pushes input samples into PitchTrackerYIN.
//   2. Read the tracker's detected fundamental Hz.
//   3. Convert to MIDI float, snap to nearest note in the active Key/Scale --
//      with note-change hysteresis so vibrato doesn't flip-flop targets.
//   4. Compute desired shift ratio = targetHz / detectedHz.
//   5. Smooth toward the target ratio over RetuneSpeed ms.
//   6. Apply Strength: blend between 1.0 (no correction) and the target ratio.
//   7. Add Humanize: small random walk in cents.
//   8. Feed the LiveShifter (formant-preserving); Throat maps to its formant
//      scale.
//
// LiveShifter contract (framework quirk): shift() consumes and returns EXACTLY
// getBlockSize() frames, fixed for the shifter's lifetime -- unrelated to the
// JUCE block -- so the JUCE block is re-partitioned through an input/output
// FIFO.  Total added latency = getStartDelay() + one block (getLatencySamples);
// the engine is cold-started on the correction-engage edge (the ~48 ms delay is
// acquired there, per B3/B6).  Correction is mono (the vocal source is a mono
// mic duplicated L=R; the wet stream is written to every channel), matching the
// editor's mono bake.
//
// Mode switch (Realtime vs Offline) is a UI/processing choice; this DSP class
// runs the realtime path.  BaySickAlign + BaySickPitch render offline through
// BaySickAlignDSP / the LibraryPitchShifters bake seam instead of this wrapper.
// ─────────────────────────────────────────────────────────────────────────────

class PitchCorrectorDSP
{
public:
    // QA-Fd 17b: order + membership = the piano roll's kScaleDefs table
    // (PianoRoll.cpp) so the realtime board and the pitch editor read
    // identically.  The old 0..9 order retires; its Custom slot was dead
    // (no UI or caller ever set the custom mask).  Saved pre-QA-Fd scale
    // picks load shifted -- accepted at spec (17b).
    enum class Scale : int
    {
        Chromatic     = 0,
        Major         = 1,
        Minor         = 2,
        Dorian        = 3,
        Phrygian      = 4,
        Lydian        = 5,
        Mixolydian    = 6,
        Locrian       = 7,
        HarmonicMinor = 8,
        MelodicMinor  = 9,
        PentatonicMaj = 10,
        PentatonicMin = 11,
        Blues         = 12
    };
    static constexpr int kNumScales = 13;

    // QA-Fd 14a/17b: the single scale table shared by the realtime board and
    // the pitch editor's Root/Scale/Snap controls (order + masks = the piano
    // roll's kScaleDefs).
    static const char* scaleName (int idx);
    static const std::array<bool, 12>& scaleMask (int idx);
    // Nearest in-scale note to midiFloat (root pc 0..11); scale 0 = nearest
    // semitone.  Static so the pitch editor + BaySickPitchDSP share it.
    static float snapMidiToScaleStatic (float midiFloat, int rootPc, int scaleIdx);

    PitchCorrectorDSP();
    ~PitchCorrectorDSP();

    // Audio lifecycle.
    void prepare (double sampleRate, int maxBlockSize);
    void releaseResources();
    void reset();

    // Audio-thread call.  In-place processing on the buffer (stereo expected).
    // When bypassed = true, the buffer passes through unchanged (still pushes
    // tracker so UI / future stages stay current if needed).
    void process (juce::AudioBuffer<float>& buffer);

    // ── Parameter setters (CPU-guarded) ──────────────────────────────────────
    void setKey               (int    midiPc);    // 0..11 (C, C#, D, ..., B)
    void setScale             (int    s);         // Scale enum value
    void setRetuneSpeedMs     (float  ms);        // 0..100; lower = robotic, higher = transparent
    void setStrength          (float  unit);      // 0..1
    void setFormantPreserve   (bool   on);        // cepstral envelope re-imposition (QA-F Task 5)
    void setHumanizeCents     (float  cents);     // 0..20
    void setThroatShiftSemis  (float  semis);     // -12..+12 spectral-envelope shift (QA-F Task 5)

    bool bypassed { true };   // H-5: default OFF per locked spec

    // ── UI feedback (atomic reads from any thread) ───────────────────────────
    float getDetectedFreqHz()      const noexcept { return mDetectedHz       .load (std::memory_order_acquire); }
    float getTargetFreqHz()        const noexcept { return mTargetHz         .load (std::memory_order_acquire); }
    float getCurrentShiftCents()   const noexcept { return mCurrentShiftCents.load (std::memory_order_acquire); }
    float getDetectionConfidence() const noexcept { return mTracker.getConfidence(); }

    // QA-Fe Task 4: total wet-path latency (LiveShifter start delay + one block).
    // 0 when the shifter isn't built in.  Exposed so the record/monitor path can
    // choose to compensate.
    int getLatencySamples() const noexcept { return mLatencySamples; }

private:
    // ── Note / scale math ────────────────────────────────────────────────────
    float snapMidiToScale (float midiFloat) const noexcept;
    static float midiToHz (float midi)  noexcept { return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f); }
    static float hzToMidi (float hz)    noexcept { return 12.0f * std::log2 (juce::jmax (hz, 1.0f) / 440.0f) + 69.0f; }

    // ── YIN tracker ──────────────────────────────────────────────────────────
    PitchTrackerYIN mTracker;

    // ── QA-Fe Task 4: Rubber Band LiveShifter + JUCE-block <-> fixed-block FIFO ─
    // The shifter's block size is fixed at construction and unrelated to the JUCE
    // block, so mInFifo accumulates mono input into whole shift() blocks and
    // mOutFifo buffers the shifted output back out at arbitrary JUCE block sizes.
    // Single-threaded (audio thread only): no locks, preallocated in prepare().
    struct MonoFifo
    {
        std::vector<float> buf;
        int cap { 0 }, head { 0 }, count { 0 };
        void init (int capacity);
        void clear() noexcept;
        int  size() const noexcept { return count; }
        void push (const float* src, int n) noexcept;
        void pushZeros (int n) noexcept;
        void pop  (float* dst, int n) noexcept;
    };

#if BAYSICK_HAS_RUBBERBAND
    std::unique_ptr<RubberBand::RubberBandLiveShifter> mLive;   // null if not yet prepared
#endif
    MonoFifo mInFifo, mOutFifo;
    std::vector<float> mInBlock, mOutBlock;   // shift() de-interleaved scratch (mBlockSize)
    std::vector<float> mMonoIn,  mWetOut;     // per-JUCE-block mono in / wet out (mMaxBlock)
    int  mBlockSize      { 0 };
    int  mMaxBlock       { 0 };
    int  mLatencySamples { 0 };               // getStartDelay() + mBlockSize
    bool mLiveActive     { false };           // running the shifter vs settled-bypass cold
    // Last values pushed to the shifter -- CPU-guard redundant RT-safe setters.
    double mLivePitchScale   { 1.0 };
    double mLiveFormantScale  { 0.0 };        // 0.0 == auto (OptionFormantPreserved)
    bool   mLivePreserve      { true };       // last setFormantOption state

    // ── Parameter state ──────────────────────────────────────────────────────
    int   mKey             { 0 };          // 0 = C
    Scale mScale           { Scale::Chromatic };
    float mRetuneSpeedMs   { 50.0f };
    float mStrength        { 1.0f };
    bool  mFormantPreserve { false };       // -> LiveShifter formant option
    float mHumanizeCents   { 0.0f };
    float mThroatSemis     { 0.0f };        // -> LiveShifter formant scale (Throat)

    // ── Smoothed pitch correction state ─────────────────────────────────────
    float mCurrentShiftRatio { 1.0f };      // smoothed toward target
    float mRetuneCoef        { 0.0f };      // computed from RetuneSpeed + sampleRate
    float mHumanizePhase     { 0.0f };      // for slow Humanize random walk
    // QA-F Task 5: the currently-held target note (MIDI).  -1 = none.  Held
    // until the sung pitch commits to a different note (hysteresis) so
    // vibrato around a boundary doesn't flip-flop the target (the "warble").
    float mCurrentTargetMidi { -1.0f };

    // ── QA-Fd 11/16a engage crossfade (QA-Fe Task 4: cold-start engine) ──────
    // The LiveShifter is cold while settled-bypassed (fast path, no CPU); it is
    // reset + fed on the engage edge.  Engage/disengage runs a ~40 ms equal-power
    // crossfade between the dry tap and the wet tap so the transition never lands
    // as a click.  The wet tap carries the shifter's ~48 ms latency, so a one-time
    // delay jump at the edges is expected (B6).  0 = fully dry, 1 = wet.
    float mEngageFade     { 0.0f };
    // 1/(0.04*sr); prepare() re-derives.  Non-zero default keeps the fade
    // functional even if a block slips in before prepare.
    float mEngageFadeStep { 1.0f / 1764.0f };

    // ── UI feedback (atomic published) ──────────────────────────────────────
    std::atomic<float> mDetectedHz        { 0.0f };
    std::atomic<float> mTargetHz          { 0.0f };
    std::atomic<float> mCurrentShiftCents { 0.0f };

    // ── State ────────────────────────────────────────────────────────────────
    double mSampleRate { 44100.0 };

    // ── RNG for Humanize (xorshift32 - fast, deterministic, audio-thread-safe) ─
    juce::uint32 mRngState { 0x12345678u };
    float        nextRandPm1() noexcept;   // returns -1..+1

    void recalcRetuneCoef();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchCorrectorDSP)
};
