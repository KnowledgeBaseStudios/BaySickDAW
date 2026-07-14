#pragma once
#include <JuceHeader.h>
#include "PitchTrackerYIN.h"
#include "PitchShifters.h"
#include <array>
#include <atomic>

// ─────────────────────────────────────────────────────────────────────────────
// PitchCorrectorDSP - Phase H-5 (2026-05-01), QA-F Task 5 quality pass
// (2026-07-09)
// ─────────────────────────────────────────────────────────────────────────────
// Realtime pitch correction wrapper for BaySickVocal.  Owns:
//   * PitchTrackerYIN for fundamental detection (40-1500 Hz, ~50 ms latency)
//   * PsolaShifter (PitchShifters.h) for the realtime audio path -- period-
//     synchronous OLA at ~2-pitch-period latency, period fed from the YIN
//     tracker.  Chosen over a phase vocoder per Call 2a: PV adds ~40 ms,
//     which confuses a learner monitoring themselves live.
//   * CepstralFormantEngine pair for Formant Preserve + Throat Shift
//     (engaged only while either is active; adds ~20 ms on the wet path).
//
// Algorithm:
//   1. Audio thread pushes input samples into PitchTrackerYIN.
//   2. Read tracker's detected fundamental Hz.
//   3. Convert to MIDI float, snap to nearest note in active Key/Scale --
//      with note-change hysteresis so vibrato doesn't flip-flop targets.
//   4. Compute desired shift ratio = targetHz / detectedHz.
//   5. Smooth toward target shift over RetuneSpeed ms.
//   6. Apply Strength: blend between 1.0 (no correction) and target ratio.
//   7. Add Humanize: small random walk in cents.
//   8. Apply ratio via PSOLA; optionally re-impose the dry spectral
//      envelope (Formant Preserve) / shift it (Throat Shift).
//
// Mode switch (Realtime vs Offline) is a UI/processing choice; the DSP class
// itself runs the realtime path.  BaySickAlign + BaySickPitch render offline
// through BaySickAlignDSP / the PitchShifters trio instead of this wrapper.
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

private:
    // ── Note / scale math ────────────────────────────────────────────────────
    float snapMidiToScale (float midiFloat) const noexcept;
    static float midiToHz (float midi)  noexcept { return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f); }
    static float hzToMidi (float hz)    noexcept { return 12.0f * std::log2 (juce::jmax (hz, 1.0f) / 440.0f) + 69.0f; }

    // ── YIN tracker ──────────────────────────────────────────────────────────
    PitchTrackerYIN mTracker;

    // ── QA-F Task 5: PSOLA shifter + formant machinery (per channel) ────────
    // Replaces the H-5 2-grain granular Shifter: PSOLA's epoch-grid grains
    // stay period-coherent (no comb/warble) and its latency is ~2 pitch
    // periods.  The formant engines only run while Formant Preserve or
    // Throat Shift is active; the engage edge resets them (their ~20 ms
    // latency appears/disappears with the mode).
    std::array<PsolaShifter, 2>          mShifters;   // L and R
    std::array<CepstralFormantEngine, 2> mFormant;    // L and R
    bool mFormantEngaged { false };

    // ── Parameter state ──────────────────────────────────────────────────────
    int   mKey             { 0 };          // 0 = C
    Scale mScale           { Scale::Chromatic };
    float mRetuneSpeedMs   { 50.0f };
    float mStrength        { 1.0f };
    bool  mFormantPreserve { false };       // toggle stored, DSP no-op for H-5
    float mHumanizeCents   { 0.0f };
    float mThroatSemis     { 0.0f };        // toggle stored, DSP no-op for H-5

    // ── Smoothed pitch correction state ─────────────────────────────────────
    float mCurrentShiftRatio { 1.0f };      // smoothed toward target
    float mRetuneCoef        { 0.0f };      // computed from RetuneSpeed + sampleRate
    float mHumanizePhase     { 0.0f };      // for slow Humanize random walk
    // QA-F Task 5: the currently-held target note (MIDI).  -1 = none.  Held
    // until the sung pitch commits to a different note (hysteresis) so
    // vibrato around a boundary doesn't flip-flop the target (the "warble").
    float mCurrentTargetMidi { -1.0f };

    // ── QA-Fd 11/16a engage crossfade ────────────────────────────────────────
    // The shifters' input rings stay warm while bypassed (feedSample copy;
    // zero synthesis, zero added latency).  Engage/disengage runs a ~40 ms
    // equal-power crossfade between the dry tap and the corrected tap, so
    // the shifter's spin-up (and the formant engines' reset) never lands as
    // a click.  0 = fully dry (synthesis skipped once settled), 1 = wet.
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
