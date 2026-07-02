#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include "PitchTrackerYIN.h"

// ─────────────────────────────────────────────────────────────────────────────
// TunerStyleDSP - Phase I-13 (2026-05-03)
// ─────────────────────────────────────────────────────────────────────────────
// TU Style Tuner.  Wraps the H-4 PitchTrackerYIN.  Audio thread pushes mono-
// summed input to the YIN tracker; published frequency is converted to note
// + cents and exposed via atomics for the UI strobe / LED display.
//
// Modes (chickenhead, 3 options):
//   Chromatic - detects all 12 notes, no snap.  Default.
//   Guitar    - snaps display to nearest standard guitar tuning open string
//               (E2 A2 D3 G3 B3 E4); cents are measured against that target.
//   Bass      - snaps display to nearest standard bass tuning open string
//               (E1 A1 D2 G2).
//
// Reference Hz: 440 / 432 toggle + Trim knob.  Trim range follows the toggle:
//   440 mode -> Trim 436..445 Hz
//   432 mode -> Trim 428..437 Hz
//
// Mute toggle: when engaged, the audio output is silenced so the user can tune
// silently.  Pitch detection still runs.
// ─────────────────────────────────────────────────────────────────────────────

class TunerStyleDSP : public DSPBase
{
public:
    enum class Mode : int
    {
        Chromatic = 0,
        Guitar    = 1,
        Bass      = 2
    };

    TunerStyleDSP();
    ~TunerStyleDSP() override = default;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    void setMode  (int   m);            // 0..2
    void setMuted (bool yes);
    void set432   (bool yes);           // false = 440 mode, true = 432 mode
    void setTrimHz (float hz);          // valid range follows the 432-toggle
    void setFlat  (int semitones);      // 0..6 flat/drop tuning (TU-3: references N semitones below standard)

    bool  isMuted() const { return mMuted; }
    bool  is432()  const { return mIs432; }
    Mode  getMode() const { return mMode; }
    float getTrimHz() const { return mTrimHz; }
    int   getFlat()  const { return mFlatSemitones; }

    // UI thread reads.  Wait-free atomics.
    float getFrequencyHz() const noexcept { return mYin.getFrequencyHz(); }
    float getConfidence()  const noexcept { return mYin.getConfidence(); }

    // Helper returning trim range bounds in Hz for the current 432-toggle.
    void  getTrimRange (float& outMin, float& outMax) const;

    // Helper that converts a frequency to {midi note, cents-deviation,
    // snapped-target-midi-for-mode}.  Returns false if Hz is invalid.
    struct Reading
    {
        int   midiNote;       // nearest chromatic note number (0..127)
        float centsFromNote;  // -50..+50 (relative to chromatic note)
        int   targetNote;     // mode-snapped note (Guitar/Bass) or same as midiNote
        float centsFromTarget;// cents from target (Guitar/Bass mode)
        float frequencyHz;    // raw input Hz (after publishing)
    };
    bool  getReading (Reading& out) const noexcept;

    // Tonic frequency for a given midi note + current reference Hz.
    float midiToHz (int midi) const noexcept;

private:
    PitchTrackerYIN mYin;

    Mode  mMode   { Mode::Chromatic };
    bool  mMuted  { false };
    bool  mIs432  { false };
    float mTrimHz { 440.0f };          // reference Hz including trim
    int   mFlatSemitones { 0 };        // 0..6 flat/drop tuning (references N semitones below standard)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerStyleDSP)
};
