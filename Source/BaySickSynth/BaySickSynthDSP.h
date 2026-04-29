#pragma once
#include <JuceHeader.h>
#include <vector>
#include "BaySickSynthVoice.h"
#include "../SynthSound.h"

// ── BaySickSynthDSP ───────────────────────────────────────────────────────────
// Polyphonic synthesiser wrapper. Owns 16 BaySickSynthVoice instances and a
// juce::Synthesiser for voice allocation / MIDI management.
//
// All parameter setters propagate immediately to every voice.
// renderNextBlock performs mono/lead MIDI pre-processing before passing to
// juce::Synthesiser.
// Called from BaySickSynthProcessor::updateFromApvts() on the audio thread.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickSynthDSP
{
public:
    BaySickSynthDSP();

    void prepare         (double sampleRate, int maxBlockSize);
    void renderNextBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi);

    // ── OSC setters ───────────────────────────────────────────────────────────
    void setWaveform    (BssWaveform w);
    void setTranspose   (int semitones);
    void setModifier    (float m);
    void setDualOscMode (int mode);
    void setOscSync     (bool on);
    void setRingMod     (bool on);
    void setDriftAmount (float amount);

    // ── Unison (P3.11) ────────────────────────────────────────────────────────
    void setUnisonVoices (int voices);
    void setUnisonDetune (float amount);
    void setUnisonSpread (float amount);
    void setNoiseLevel  (float n);
    void setNoiseOnlyMode (bool on);
    void setNoiseColor    (int color);
    void setGlideTime   (float secs);

    // ── Voice mode ────────────────────────────────────────────────────────────
    void setVoiceMode   (BssVoiceMode mode);

    // ── Cut self (Session E retro): cut prior voice playing same note on noteOn
    void setCutSelf     (bool on);

    // ── Mod wheel ─────────────────────────────────────────────────────────────
    void setModWheelDest (int dest);
    void setModWheelAmt  (float amt);

    // ── Amp ADSR ──────────────────────────────────────────────────────────────
    void setAmpEnv (float a, float d, float s, float r);

    // ── Velocity → Amp tracking ───────────────────────────────────────────────
    void setVelAmpTrack (float amount);

    // ── Pitch envelope ────────────────────────────────────────────────────────
    void setPitchEnv    (float a, float d, float s, float r);
    void setPitchEnvAmt (float semitones);

    // ── Transient injector (P3.5) ─────────────────────────────────────────────
    void setTransientAmount   (float amount);
    void setTransientDuration (float ms);
    void setTransientColour   (float hz);

    // ── Multi-burst envelope (P3.6) ───────────────────────────────────────────
    void setBurstMode    (bool on);
    void setBurstCount   (int count);
    void setBurstSpacing (float ms);

    // ── Filter setters ────────────────────────────────────────────────────────
    void setFilterType     (BssFilterType type);
    void setFilterCutoff   (float hz);
    void setFilterRes      (float res);
    void setFilterEnvAmt   (float amount);
    void setFilterKbTrack  (float amount);
    void setFilterVelTrack (float amount);

    // ── Filter ADSR ───────────────────────────────────────────────────────────
    void setFltEnv (float a, float d, float s, float r);

    // ── LFO setters ───────────────────────────────────────────────────────────
    void setLFOShape  (LFOShape shape);
    void setLFODest   (BssLFODest dest);
    void setLFORate   (float hz);
    void setLFOAmount (float amount);

private:
    static constexpr int kNumVoices = 16;

    juce::Synthesiser  mSynth;
    BaySickSynthVoice* mVoices[kNumVoices] { nullptr };  // raw ptrs (owned by mSynth)

    // ── Mono/Lead/Legato state ────────────────────────────────────────────────
    BssVoiceMode mVoiceMode    { BssVoiceMode::Poly };
    int          mLastMonoNote { -1 };
    bool         mCutSelf      { false };   // Session E: Poly-mode same-note cut

    // Legato: last-note-priority stack + currently-playing voice pointer.
    // mLegatoSynthNote = the note number the juce::Synthesiser thinks the voice is
    // playing (the original noteOn we passed through). retargetLegato changes the
    // voice's pitch without updating the synth's internal bookkeeping, so the
    // final noteOff must match mLegatoSynthNote, not the user's last-released note.
    std::vector<int>    mLegatoHeldNotes;
    BaySickSynthVoice*  mLegatoVoice     { nullptr };
    int                 mLegatoSynthNote { -1 };

    void handleLegatoMidi (juce::MidiBuffer& midi);
    BaySickSynthVoice* findVoiceForNote (int note);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickSynthDSP)
};
