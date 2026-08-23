#pragma once
#include <JuceHeader.h>
#include "../DSP/PanLaw.h"
#include "../SynthSound.h"
#include "../WavetableOscillator.h"
#include "../SynthFilter.h"
#include "../AdsrEnvelope.h"
#include "../LFO.h"
#include "../BroadcastSynthesiser.h"

// ── Enumerations ──────────────────────────────────────────────────────────────

enum class BssWaveform
{
    Saw = 0, SawSaw, Pulse, SawSquare, SquareSquare,
    Supersaw, Bell, DeafSaw, SpreadOct, SpreadFifth, Sine, kCount
};

enum class BssLFODest  { Filter = 0, Pitch, OscModifier, kCount };
// Lead was removed 2026-08-10 (Jeff's ruling): it fell through to the Mono
// branch, so it had never behaved differently from Mono in any shipped build,
// and the behavior it WOULD have been given is what Legato already does.
//
// WHAT AN OLD PROJECT ACTUALLY DOES, corrected 2026-08-11 (QA-Manuals): this
// comment used to claim AudioParameterChoice persists NORMALIZED and that a
// Lead project therefore reads back as Mono.  Both halves are wrong.  APVTS
// stores the DENORMALISED value (juce_AudioProcessorValueTreeState.cpp:485
// writes convertFrom0to1 (getValue())), and for AudioParameterChoice that is
// the raw index, because its range is { 0, choices.size() - 1 } with interval 1.
// So a project saved on Lead wrote 2, and 2 in the three-entry list is LEGATO -
// which glides instead of retriggering, so it does not sound like the Mono it
// used to play.  Old Poly (0) and Mono (1) still load correctly and old Legato
// (3) clamps to 2, which lands right by luck.  Nothing crashes and nothing goes
// silent, so this is left as-is per the no-backward-compat-pre-v1 rule; it is
// recorded here so the next reader does not repeat the wrong reasoning.
enum class BssVoiceMode { Poly = 0, Mono, Legato, kCount };

// Filter type (unchanged)
enum class BssFilterType { LowPass = 0, HighPass, BandPass, Notch, kCount };

// ── BaySickSynthVoice ─────────────────────────────────────────────────────────
// A single polyphonic voice for BaySickSynth (and, via BaySickSynthDSP, for
// BaySickBass -- there is no separate bass voice class).
// Owns: two WavetableOscillators (main + dual-osc), SynthFilter, three ADSR
// envelopes (amp + filter + pitch), one LFO.  EVERY one of those is rate
// dependent and must be prepared from setCurrentPlaybackSampleRate.
//
// All setters are called from BaySickSynthDSP before renderNextBlock.
// renderNextBlock executes per-sample processing entirely in the inner loop.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickSynthVoice : public juce::SynthesiserVoice,
                          public GlideStashClearable,
                          public RampTakeoverAcceptor
{
public:
    BaySickSynthVoice();

    bool canPlaySound (juce::SynthesiserSound* sound) override;
    void startNote    (int midiNote, float velocity,
                       juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote     (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newValue) override;
    void controllerMoved (int cc, int value) override;   // CC1 = mod wheel
    // GlideStashClearable (QA-H); #36 extended to the full one-shot set (the
    // CC85 branch tail that used to clear these now lives in the synth's wipe).
    void clearGlideStash() override
    {
        mGlideFromNote    = -1;
        mGlideTimePending = false;
        mSlideTargetVel   = -1.0f;
        mPanRampPend      = -999.0f;
    }
    // #36: first-match CC85 takeover (dispatched by BroadcastSynthesiser).
    bool tryRampTakeover (int targetNote) override;
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
    void setVoiceMode   (BssVoiceMode mode);  // informational; every mode is enforced at DSP level

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
    // DSP calls this in Legato mode when a new note arrives while the
    // previous one is still held.
    void retargetLegato (int midiNote);
    bool isInRelease() const { return mInRelease; }

    // ── Cut-self hard cut (QA-CutSelfReview): instant, click-free fade-out then
    // retire.  Driven by BaySickSynthDSP for the Same Pitch / Cut All modes.
    void cutFast() noexcept;

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
    // One-pole smoothing coefficient for the drift glide, re-derived per sample
    // rate so the wander takes the same wall-clock time on every device.
    float       mDriftSmoothCoef { 0.0005f };

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
    // The published Kellett pink poles/gains and the brown integrator pole are a
    // 44.1 kHz design.  setCurrentPlaybackSampleRate re-derives them for the live
    // rate so the noise keeps the same color and the same level on every device;
    // the defaults here are the published values, which the derivation reproduces
    // exactly at 44.1 kHz.
    float  mPinkPole[6]  { 0.99886f,   0.99332f,   0.96900f,
                           0.86650f,   0.55000f,  -0.7616f };
    float  mPinkGain[6]  { 0.0555179f, 0.0750759f, 0.1538520f,
                           0.3104856f, 0.5329522f, -0.0168980f };
    float  mBrownPole    { 0.998f };
    float  mBrownGain    { 0.02f };
    // White noise has no pole to re-derive, but the raw LCG output is a fixed
    // per-sample variance spread flat over 0..rate/2, so its density inside the
    // audible band falls as 1 / rate and white got quieter than pink and brown
    // once those were made rate-invariant.  sqrt (rate / 44100) restores equal
    // in-band energy and is exactly 1 at the 44.1 kHz design rate.  Applied to
    // the white output only -- the whiteN that FEEDS the pink and brown sections
    // must stay unscaled or their already-held levels would move with it.
    float  mWhiteRateGain { 1.0f };

    // ── Voice mode ────────────────────────────────────────────────────────────
    BssVoiceMode mVoiceMode { BssVoiceMode::Poly };

    // ── Mod wheel state ───────────────────────────────────────────────────────
    int   mModWheelDest  { 0 };     // 0=Filter, 1=LFO
    float mModWheelAmt   { 0.0f };
    float mModWheelValue { 0.0f };  // current CC1 value 0..1
    // Batch E #2 (2026-05-01): per-note filter cutoff offset from CC74.
    // -2..+2 octaves applied multiplicatively to effCutoff in renderNextBlock.
    float mPerNoteCutoffOctaves { 0.0f };

    // S-7: per-note pan from CC10 (-1 left .. +1 right, 0 = center).  Channel-wide
    // live value like the cutoff above (broadcast reaches idle voices); applied as
    // a center-preserving balance in the render output so a centered note is
    // unchanged.  Fixes app-wide panning (Issue 5B: CC10 was emitted, never read).
    float mNotePan { 0.0f };
    baysick::pan::GainCache mPanGains;   // note pan -> gains via the project law

    // S-6(C): RampSlide loudness ramp.  CC86 stashes the target velocity; CC85
    // arms a per-sample ramp of mCurrentVelocity from the base note's value to the
    // slide's over the glide, so amplitude + vel-tracked filter both interpolate.
    float mSlideTargetVel { -1.0f };   // -1 = none pending
    float mVelRampTarget  { 0.0f };
    float mVelRampStep    { 0.0f };
    int   mVelRampLeft    { 0 };
    // #11 (G-4): CC89 pan-ramp target (-999 = none pending).  Armed at the RP
    // takeover / RT glide noteOn; mNotePan glides current -> target over the
    // glide span (mirrors the S-6(C) velocity ramp above).
    float mPanRampPend    { -999.0f };
    float mPanRampTarget  { 0.0f };
    float mPanRampStep    { 0.0f };
    int   mPanRampLeft    { 0 };

    // QA-H per-note expression: pending stashes are written by controllerMoved
    // (the roll emits the CCs just before each noteOn) and consumed into the
    // active values at startNote, so a still-sounding voice keeps its own
    // resonance/release when the NEXT note's CCs arrive on the channel.
    float mPendResOffset   { 0.0f };   // CC71: -0.5..+0.5 on the 0-1 res scale
    float mPendRelScale    { 1.0f };   // CC72: 0.25x..4x release-time scale
    float mActiveResOffset { 0.0f };
    float mActiveRelScale  { 1.0f };
    // CC84 glide source note + optional CC5/CC37 14-bit glide time (ms).
    // Consumed one-shot at startNote; time absent = porta (engine glide time,
    // 60 ms fallback when the glide param is 0), present = slide (spans note).
    int   mGlideFromNote    { -1 };
    int   mGlideTimeMsbMs   { 0 };
    int   mGlideTimeLsbMs   { 0 };
    bool  mGlideTimePending { false };
    // Base amp ADSR as last set by the DSP (setters only fire on user change,
    // so startNote re-applies base * active release scale itself each note).
    float mAmpA { 0.01f }, mAmpD { 0.1f }, mAmpS { 0.8f }, mAmpR { 0.3f };

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

    // Cut-self fast fade-out (QA-CutSelfReview): ~1 ms linear ramp to 0, then the
    // voice retires.  Gives an instant but click-free hard cut.
    bool  mCutFadeActive { false };
    float mCutFadeGain   { 1.0f };
    float mCutFadeStep   { 0.0f };

    // ── Helpers ───────────────────────────────────────────────────────────────
    static float midiToHz (int note, float extraSemis = 0.0f) noexcept;
    void rebuildOscWavetable();
    void updateFilterType();
    void applyEffectiveFilterRes();   // base res + per-note CC71 offset -> Q

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickSynthVoice)
};
