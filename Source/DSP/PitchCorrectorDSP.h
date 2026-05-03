#pragma once
#include <JuceHeader.h>
#include "PitchTrackerYIN.h"
#include <array>
#include <atomic>

// ─────────────────────────────────────────────────────────────────────────────
// PitchCorrectorDSP — Phase H-5 (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────
// Realtime pitch correction wrapper for BaySickVocal.  Owns:
//   * PitchTrackerYIN for fundamental detection (40-1500 Hz, ~50 ms latency)
//   * Granular time-domain pitch shifter for the realtime audio path
//     (2 overlapping Hann-windowed grains, simple but reliable for vocals
//     at small shift amounts <= 1 semitone)
//
// Algorithm:
//   1. Audio thread pushes input samples into PitchTrackerYIN.
//   2. Read tracker's detected fundamental Hz.
//   3. Convert to MIDI float, snap to nearest note in active Key/Scale.
//   4. Compute desired shift ratio = targetHz / detectedHz.
//   5. Smooth toward target shift over RetuneSpeed ms.
//   6. Apply Strength: blend between 1.0 (no correction) and target ratio.
//   7. Add Humanize: small random walk in cents.
//   8. Apply ratio to audio via granular shifter -> output.
//
// Formant Preserve toggle and Throat Shift APVTS params are wired but the
// formant DSP itself is a follow-up (cepstral envelope swap pass) — for H-5
// the toggle is a no-op pass through.  Knobs are preset-safe to add later.
//
// Mode switch (Realtime vs Offline) is a UI/processing choice; the DSP class
// itself runs the realtime path.  H-6a's BaySickAlign + BaySickPitch use
// PhaseVocoder directly for offline render rather than this wrapper.
// ─────────────────────────────────────────────────────────────────────────────

class PitchCorrectorDSP
{
public:
    enum class Scale : int
    {
        Chromatic     = 0,
        Major         = 1,
        Minor         = 2,
        HarmonicMinor = 3,
        Dorian        = 4,
        Mixolydian    = 5,
        Phrygian      = 6,
        Lydian        = 7,
        Locrian       = 8,
        Custom        = 9
    };

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
    void setFormantPreserve   (bool   on);        // currently no-op pass through
    void setHumanizeCents     (float  cents);     // 0..20
    void setThroatShiftSemis  (float  semis);     // -12..+12 (formant shift; no-op stub)
    void setCustomScaleNotes  (const std::array<bool, 12>& notes);

    bool bypassed { true };   // H-5: default OFF per locked spec

    // ── UI feedback (atomic reads from any thread) ───────────────────────────
    float getDetectedFreqHz()      const noexcept { return mDetectedHz       .load (std::memory_order_acquire); }
    float getTargetFreqHz()        const noexcept { return mTargetHz         .load (std::memory_order_acquire); }
    float getCurrentShiftCents()   const noexcept { return mCurrentShiftCents.load (std::memory_order_acquire); }
    float getDetectionConfidence() const noexcept { return mTracker.getConfidence(); }

private:
    // ── Note / scale math ────────────────────────────────────────────────────
    bool  isNoteInScale (int pc, int rootPc, Scale s) const noexcept;
    int   snapPcToScale (int pc, int rootPc, Scale s) const noexcept;
    float snapMidiToScale (float midiFloat) const noexcept;
    static float midiToHz (float midi)  noexcept { return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f); }
    static float hzToMidi (float hz)    noexcept { return 12.0f * std::log2 (juce::jmax (hz, 1.0f) / 440.0f) + 69.0f; }

    // ── YIN tracker ──────────────────────────────────────────────────────────
    PitchTrackerYIN mTracker;

    // ── Granular pitch shifter (per channel) ────────────────────────────────
    struct Shifter
    {
        std::vector<float>     ring;        // input ring buffer
        int                    writePos { 0 };
        int                    ringSize { 0 };

        // Two grains, 50 % overlap
        struct Grain
        {
            float readPos    { 0.0f };  // floating point read index (linear-interp)
            float outputAge  { 0.0f };  // 0..grainSize - position within grain envelope
            bool  active     { false };
        };
        std::array<Grain, 2>   grains;
        int                    sinceLastGrain { 0 };

        int                    grainSize   { 1024 };  // re-sized in prepare from sampleRate
        int                    grainStride { 512  };  // half of grainSize (50% overlap)

        void prepare (double sampleRate);
        void reset();
        // Process one mono sample: write `in`, read `pitchRatio`-scaled grains, return output.
        float processSample (float in, float pitchRatio) noexcept;
    };
    std::array<Shifter, 2> mShifters;   // L and R

    // ── Parameter state ──────────────────────────────────────────────────────
    int   mKey             { 0 };          // 0 = C
    Scale mScale           { Scale::Chromatic };
    float mRetuneSpeedMs   { 50.0f };
    float mStrength        { 1.0f };
    bool  mFormantPreserve { false };       // toggle stored, DSP no-op for H-5
    float mHumanizeCents   { 0.0f };
    float mThroatSemis     { 0.0f };        // toggle stored, DSP no-op for H-5
    std::array<bool, 12> mCustomScale {};   // all-false = empty until user sets

    // ── Smoothed pitch correction state ─────────────────────────────────────
    float mCurrentShiftRatio { 1.0f };      // smoothed toward target
    float mRetuneCoef        { 0.0f };      // computed from RetuneSpeed + sampleRate
    float mHumanizePhase     { 0.0f };      // for slow Humanize random walk

    // ── UI feedback (atomic published) ──────────────────────────────────────
    std::atomic<float> mDetectedHz        { 0.0f };
    std::atomic<float> mTargetHz          { 0.0f };
    std::atomic<float> mCurrentShiftCents { 0.0f };

    // ── State ────────────────────────────────────────────────────────────────
    double mSampleRate { 44100.0 };

    // ── RNG for Humanize (xorshift32 — fast, deterministic, audio-thread-safe) ─
    juce::uint32 mRngState { 0x12345678u };
    float        nextRandPm1() noexcept;   // returns -1..+1

    void recalcRetuneCoef();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchCorrectorDSP)
};
