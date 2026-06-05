#pragma once
#include <JuceHeader.h>
#include "PatternManager.h"

// ── MidiRecorder ──────────────────────────────────────────────────────────────
// Captures note-on / note-off pairs from the audio thread and converts them
// to PianoNotes when recording stops.
//
// Usage (all calls on audio thread except start/stop):
//   startRecording(currentBeat)             - arm, called from message thread
//   processBlock(midi, numSamples, bps)     - called every block while recording
//   stopRecording()  → vector<PianoNote>    - called from message thread; clears state
// ─────────────────────────────────────────────────────────────────────────────
class MidiRecorder
{
public:
    MidiRecorder() = default;

    // Message thread: arm / disarm
    void startRecording(double startBeat);
    bool isRecording() const { return mRecording.load(); }

    // Audio thread: call every processBlock while armed.
    // numSamples     = this block's length.  The recorder advances its own
    //                  count-in-inclusive beat clock by numSamples*bps each
    //                  block.  Do NOT feed the transport PPQ here -- post-QA-Ed
    //                  the playhead freezes during the count-in (it isn't
    //                  "playing" yet), which dropped the count-in bar out of
    //                  recorded note positions.
    // beatsPerSample = bpm / (60 * sampleRate)
    void processBlock(const juce::MidiBuffer& midi,
                      int numSamples,
                      double beatsPerSample);

    // Message thread: stop and return all completed notes.
    // Notes still held at stop time are given a 1/8-beat duration.
    std::vector<PianoNote> stopRecording();

private:
    std::atomic<bool> mRecording { false };

    // Free-running beat position since startRecording, advanced every block
    // (INCLUDING the count-in).  This is the recorder's OWN clock so note
    // timestamps stay correct even though the transport playhead freezes
    // during the count-in (QA-Ee record-displacement fix).  Audio-thread
    // write; reset on the message thread in startRecording before mRecording
    // is published, same as mActive/mCompleted.
    double mElapsedBeats { 0.0 };

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
