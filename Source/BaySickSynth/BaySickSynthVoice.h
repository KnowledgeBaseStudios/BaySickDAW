#pragma once
#include <JuceHeader.h>
#include "../SynthSound.h"
#include "../WavetableOscillator.h"
#include "../SynthFilter.h"
#include "../AdsrEnvelope.h"
#include "../LFO.h"

// ── Enumerations ──────────────────────────────────────────────────────────────

enum class BssWaveform
{
    Saw = 0, SawSaw, Pulse, SawSquare, SquareSquare,
    Supersaw, Bell, DeafSaw, SpreadOct, SpreadFifth, Sine, kCount
};

enum class BssLFODest  { Filter = 0, Pitch, OscModifier, kCount };
enum class BssVoiceMode { Poly = 0, Mono, Lead, Legato, kCount };

// Filter type (unchanged)
enum class BssFilterType { LowPass = 0, HighPass, BandPass, Notch, kCount };

// ── BaySickSynthVoice ─────────────────────────────────────────────────────────
// A single polyphonic voice for BaySickSynth.
// Owns: two WavetableOscillators (main + dual-osc), SynthFilter, two ADSR
// envelopes (amp + filter), one LFO.
//
// All setters are called from BaySickSynthDSP before renderNextBlock.
// renderNextBlock executes per-sample processing entirely in the inner loop.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickSynthVoice : public juce::SynthesiserVoice
{
public:
    BaySickSynthVoice();

    bool canPlaySound (juce::SynthesiserSound* sound) override;
    void startNote    (int midiNote, float velocity,
                       juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote     (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newValue) override;
    void controllerMoved (int cc, int value) override;   // CC1 = mod wheel
    void renderNextBlock (juce::AudioBuffer<float>& buf,
                          int startSample, int numSamples) override;
    void setCurrentPlaybackSampleRate (double newRate) override;

    // ── OSC ───────────────────────────────────────────────────────────────────
    void setWaveform    (BssWaveform w);
    void setTranspose   (int semitones);      // -24..24
    void setModifier    (float m);            // 0-1, context-dependent
    void setDualOscMode (int mode);           // 0=Musical (current), 1=Hz Offset, 2=Absolute Hz

    // ── Hard sync (P3.7): osc2 phase restarts when osc1 completes a cycle ─────
    void setOscSync (bool on);                // false = normal dual-osc behaviour

    // ── Ring modulation (P3.8): sample = osc1 * osc2 (metallic / bell) ────────
    void setRingMod (bool on);                // false = normal dual-osc sum

    // ── Per-voice analog drift (P3.10): slow random pitch wander for warmth ───
    void setDriftAmount (float amount);       // 0 = off, 1 = max drift (~ ±10 cents)

    // ── Unison (P3.11): detuned saw stack with stereo spread ──────────────────
    void setUnisonVoices (int voices);        // 1-7 (1 = unison off)
    void setUnisonDetune (float amount);      // 0 = in tune, 1 = ±50 cents
    void setUnisonSpread (float amount);      // 0 = mono, 1 = full stereo
    void setNoiseLevel  (float n);            // 0-1
    void setNoiseOnlyMode (bool on);          // true = osc muted, noise is the sound source
    void setNoiseColor   (int color);         // 0=White, 1=Pink, 2=Brown
    void setGlideTime   (float secs);         // 0-2

    // ── Voice mode ────────────────────────────────────────────────────────────
    void setVoiceMode   (BssVoiceMode mode);  // informational; Mono enforced at DSP level

    // ── Mod wheel ─────────────────────────────────────────────────────────────
    void setModWheelDest (int dest);          // 0=Filter, 1=LFO
    void setModWheelAmt  (float amt);         // 0-1

    // ── Amp ADSR ──────────────────────────────────────────────────────────────
    void setAmpEnv (float a, float d, float s, float r);

    // ── Velocity → Amp tracking (0 = no velocity, 1 = full velocity) ──────────
    void setVelAmpTrack (float amount);

    // ── Pitch envelope (one-shot ADSR on pitch, bipolar amount) ───────────────
    void setPitchEnv    (float a, float d, float s, float r);
    void setPitchEnvAmt (float semitones);     // -24..+24

    // ── Transient injector (P3.5): short noise burst on noteOn ────────────────
    void setTransientAmount   (float amount);   // 0 = off, 1 = full
    void setTransientDuration (float ms);       // 0-20 ms
    void setTransientColour   (float hz);       // HPF cutoff 200-10000 Hz

    // ── Multi-burst envelope (P3.6): N attack pulses modulate amp env ─────────
    void setBurstMode    (bool on);             // false = plain ADSR
    void setBurstCount   (int count);           // 1-8 bursts
    void setBurstSpacing (float ms);            // 1-100 ms between bursts

    // ── Legato support ────────────────────────────────────────────────────────
    // Re-pitch to midiNote without retriggering envelopes / LFO / phases.
    // DSP calls this in Legato mode when a new note arrives while previous still held.
    void retargetLegato (int midiNote);
    bool isInRelease() const { return mInRelease; }

    // ── Filter ────────────────────────────────────────────────────────────────
    void setFilterType     (BssFilterType type);
    void setFilterCutoff   (float hz);        // 20-20000
    void setFilterRes      (float res);       // 0-1
    void setFilterEnvAmt   (float amount);    // -1..+1
    void setFilterKbTrack  (float amount);    // 0-1
    void setFilterVelTrack (float amount);    // 0-1

    // ── Filter ADSR ───────────────────────────────────────────────────────────
    void setFltEnv (float a, float d, float s, float r);

    // ── LFO ───────────────────────────────────────────────────────────────────
    void setLFOShape  (LFOShape shape);
    void setLFODest   (BssLFODest dest);
    void setLFORate   (float hz);
    void setLFOAmount (float amount);         // 0-1

private:
    // ── DSP objects ───────────────────────────────────────────────────────────
    WavetableOscillator mOsc;    // main oscillator
    WavetableOscillator mOsc2;   // second oscillator (dual-osc modes)
    SynthFilter         mFilter;
    AdsrEnvelope        mAmpEnv;
    AdsrEnvelope        mFltEnv;
    AdsrEnvelope        mPitchEnv;
    LFO                 mLFO;

    // ── OSC state ─────────────────────────────────────────────────────────────
    BssWaveform mWaveform    { BssWaveform::Saw };
    int         mTranspose   { 0 };           // semitones (-24..24)
    float       mModifier    { 0.5f };        // 0-1, meaning depends on waveform
    int         mDualOscMode { 0 };           // 0=Musical, 1=Hz Offset, 2=Absolute Hz
    bool        mOscSync     { false };       // P3.7 hard sync
    bool        mRingMod     { false };       // P3.8 ring mod
    double      mSyncPhase1  { 0.0 };
    double      mSyncPhase2  { 0.0 };

    // P3.10 analog drift - slow per-voice pitch wander (±cents) for warmth.
    float       mDriftAmount     { 0.0f };
    float       mDriftTargetCents { 0.0f };
    float       mDriftCurrentCents { 0.0f };
    int         mDriftCounter    { 0 };
    uint32_t    mDriftRngState   { 0x8F1BBCDCu };

    // P3.11 unison - up to 6 detuned saw copies (voices 2-7) panned across stereo.
    int         mUnisonVoices    { 1 };
    float       mUnisonDetune    { 0.2f };
    float       mUnisonSpread    { 0.8f };
    double      mUniPhase[6]     { 0, 0, 0, 0, 0, 0 };
    float       mNoiseLevel     { 0.0f };     // 0-1 (mix-in amount normally; level when noise-only)
    bool        mNoiseOnlyMode  { false };    // true = osc muted, noise IS the sound
    float       mGlideTime   { 0.0f };

    // ── Inline phase accumulators (for Pulse, SpreadOct, SpreadFifth, DeafSaw)
    double mPhase1 { 0.0 };
    double mPhase2 { 0.0 };
    double mPhase3 { 0.0 };

    // ── FM synthesis state (Bell) ─────────────────────────────────────────────
    double mFMCarrierPhase { 0.0 };
    double mFMModPhase     { 0.0 };

    // ── DeafSaw one-pole LP state ─────────────────────────────────────────────
    float mDeafSawState { 0.0f };

    // ── LCG noise state ───────────────────────────────────────────────────────
    uint32_t mNoiseSeed { 0x12345678u };

    // ── Noise colour (P3.9): 0=White, 1=Pink (Kellett filter), 2=Brown (integrator)
    int    mNoiseColor   { 0 };
    float  mPinkB[7]     { 0, 0, 0, 0, 0, 0, 0 };   // Kellett pink-noise filter state
    float  mBrownState   { 0.0f };                  // Leaky integrator for brown

    // ── Voice mode ────────────────────────────────────────────────────────────
    BssVoiceMode mVoiceMode { BssVoiceMode::Poly };

    // ── Mod wheel state ───────────────────────────────────────────────────────
    int   mModWheelDest  { 0 };     // 0=Filter, 1=LFO
    float mModWheelAmt   { 0.0f };
    float mModWheelValue { 0.0f };  // current CC1 value 0..1
    // Batch E #2 (2026-05-01): per-note filter cutoff offset from CC74.
    // -2..+2 octaves applied multiplicatively to effCutoff in renderNextBlock.
    float mPerNoteCutoffOctaves { 0.0f };

    // ── Filter state ──────────────────────────────────────────────────────────
    BssFilterType mFilterType       { BssFilterType::LowPass };
    float         mFilterBaseCutoff { 20000.0f };
    float         mFilterRes        { 0.0f };
    float         mFilterEnvAmt     { 0.0f };
    float         mFilterKbTrack    { 0.0f };
    float         mFilterVelTrack   { 0.0f };
    float         mLastEffCutoff    { 20000.0f };

    // ── LFO state ─────────────────────────────────────────────────────────────
    BssLFODest mLFODest   { BssLFODest::Filter };
    float      mLFOAmount { 0.0f };

    // ── Glide ─────────────────────────────────────────────────────────────────
    float mCurrentFreqHz { 0.0f };
    float mTargetFreqHz  { 0.0f };
    float mGlideRatio    { 1.0f };

    // ── MIDI state ────────────────────────────────────────────────────────────
    int   mCurrentNote     { -1 };
    float mCurrentVelocity { 1.0f };
    float mPitchWheelSemis { 0.0f };
    float mVelAmpTrack     { 1.0f };   // 0 = amp ignores vel, 1 = full vel tracking
    float mPitchEnvAmt     { 0.0f };   // -24..+24 semitones; default 0 = no pitch mod

    // Transient injector state (P3.5)
    float mTransientAmount     { 0.0f };    // 0 = off
    float mTransientDurationMs { 5.0f };
    float mTransientColourHz   { 5000.0f };
    int   mTransientSamplesTotal     { 0 };
    int   mTransientSamplesRemaining { 0 };
    float mTransientHPFState { 0.0f };
    float mTransientPrevInput { 0.0f };
    uint32_t mTransientNoiseSeed { 0x9E3779B9u };

    // Multi-burst envelope state (P3.6)
    bool  mBurstMode         { false };
    int   mBurstCount        { 4 };
    float mBurstSpacingMs    { 20.0f };
    int   mBurstSamplesElapsed { 0 };
    bool  mInRelease       { false };  // true while amp env tail plays after noteOff

    // Declick fade-in ramp applied at every startNote so a hard voice
    // termination (e.g. Legato's forced V0 stop) can't produce an audible click.
    float mDeclickGain     { 1.0f };
    float mDeclickStep     { 1.0f };

    // ── Helpers ───────────────────────────────────────────────────────────────
    static float midiToHz (int note, float extraSemis = 0.0f) noexcept;
    void rebuildOscWavetable();
    void updateFilterType();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickSynthVoice)
};
