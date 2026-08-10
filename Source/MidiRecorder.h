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

    // Audio thread: call every processBlock while armed.  Allocation-free for
    // the take's first 8192 notes / 128 simultaneously-held notes -- that
    // capacity is reserved by startRecording on the message thread and
    // published by its arming store, so do not remove those reserves.
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

    // THREAD SAFETY: audio-thread liveness gate over mActive / mCompleted --
    // the same shape as BaySickDAWProcessor's mStripTapsLive gate over
    // mStripRecorders.  processBlock raises this BEFORE it tests mRecording
    // and lowers it on every exit path; start/stopRecording store
    // mRecording=false and then wait here before touching the vectors.
    // Both halves are seq_cst because they are a store-load handshake: if
    // either side's store may sink past its load, one of the two directions
    // reopens (the audio thread can enter unseen, or the message thread can
    // read "idle" while a block is between its gate test and its first
    // push_back).
    std::atomic<bool> mBlockInFlight { false };

    // Message thread only.  Blocks until any processBlock that already passed
    // the mRecording gate has left, which is what makes the gate a handshake
    // rather than a hint.  Bounded, like BaySickDAWProcessor::settleAudioThread
    // (which MidiRecorder cannot reach from here): a block that never returns
    // means the audio thread is already wedged, and freezing the UI on it
    // forever would be worse than the race this closes.  Costs nothing when no
    // block is in flight -- which is also the device-closed / suspended case,
    // so no separate early-out is needed.
    void settleAudioBlock() noexcept;

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

    // THREAD SAFETY: written from the audio thread in processBlock, drained on
    // the message thread in stopRecording.  Storing mRecording=false is NOT on
    // its own sufficient -- it only stops the NEXT block from entering; a block
    // already past the gate keeps push_backing here while stopRecording
    // iterates mActive and moves mCompleted's buffer away.  The mBlockInFlight
    // settle above is what closes that window, and it must run before any
    // access on this side.
    std::vector<ActiveNote>  mActive;     // notes currently held
    std::vector<PianoNote>   mCompleted;  // note-on + note-off matched

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiRecorder)
};
