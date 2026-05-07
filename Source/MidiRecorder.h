#pragma once
#include <JuceHeader.h>
#include "PatternManager.h"

// ── MidiRecorder ──────────────────────────────────────────────────────────────
// Captures note-on / note-off pairs from the audio thread and converts them
// to PianoNotes when recording stops.
//
// Usage (all calls on audio thread except start/stop):
//   startRecording(currentBeat)             - arm, called from message thread
//   processBlock(midi, beatStart, bps)      - called every block while recording
//   stopRecording()  → vector<PianoNote>    - called from message thread; clears state
// ─────────────────────────────────────────────────────────────────────────────
class MidiRecorder
{
public:
    MidiRecorder() = default;

    // Message thread: arm / disarm
    void startRecording(double startBeat);
    bool isRecording() const { return mRecording.load(); }

    // Audio thread: call every processBlock while armed
    // beatStart = PPQ position at the start of this block
    // beatsPerSample = bpm / (60 * sampleRate)
    void processBlock(const juce::MidiBuffer& midi,
                      double beatStart,
                      double beatsPerSample);

    // Message thread: stop and return all completed notes.
    // Notes still held at stop time are given a 1/8-beat duration.
    std::vector<PianoNote> stopRecording();

private:
    std::atomic<bool> mRecording { false };

    // Pending note (waiting for matching note-off)
    struct ActiveNote
    {
        int    midiNote;
        double startBeat;
        float  velocity;
    };

    // Written from audio thread, read on message thread after mRecording=false.
    // Access is safe because processBlock only writes while mRecording==true,
    // and stopRecording() sets mRecording=false before reading.
    std::vector<ActiveNote>  mActive;     // notes currently held
    std::vector<PianoNote>   mCompleted;  // note-on + note-off matched

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiRecorder)
};
