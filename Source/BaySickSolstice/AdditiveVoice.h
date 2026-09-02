#pragma once
#include <JuceHeader.h>
#include "../DSP/PanLaw.h"
#include "HarmonicEngine.h"
#include "BaySickSolsticeModRegistry.h"
#include "SynthSound.h"   // in Source/ - on the include path via CMake
#include "BroadcastSynthesiser.h"

// ── AdditiveVoice ─────────────────────────────────────────────────────────────
// One polyphonic voice for BaySickSolsticeSynth.
// Plays up to two HarmonicEngine wavetables (Part A and Part B) mixed together.
// Supports 1–9-voice unison with per-slot detune and stereo spread.
// Applies a per-voice stereo StateVariableTPT filter + amplitude ADSR.
// Also applies tremolo, vibrato, a second filter, and pitch offset.
//
// Zero heap allocations on the audio thread: all arrays pre-allocated for the
// maximum unison count. SmoothedValues prevent parameter-change zipper noise.
// ─────────────────────────────────────────────────────────────────────────────
class AdditiveVoice : public juce::SynthesiserVoice,
                      public GlideStashClearable,
                      public RampTakeoverAcceptor
{
public:
    static constexpr int kMaxUnison = 9;

    AdditiveVoice();

    // ── Engine binding ────────────────────────────────────────────────────────
    // Call once before adding the voice to BaySickSolsticeSynth.
    // partA and partB may point to the same engine (monophonic timbre).
    void setEngines (const HarmonicEngine* partA, const HarmonicEngine* partB);

    // ── Parameter setters (message thread or processBlock preamble) ───────────
    void setUnison       (int numVoices, float detuneCents, float spread);
    void setPartLevels   (float partALevel, float partBLevel);
    void setAmpEnv       (float attackSec, float decaySec, float sustain, float releaseSec);
    void setFilterParams (float cutoffHz, float resonance);
    void setVolume       (float vol);         // 0–1
    void setPan          (float pan);         // −1 (left) .. +1 (right)

    // ── Beta: portamento / legato ─────────────────────────────────────────────
    // glideTimeSec = 0 → no glide (10ms fast snap). legato = true → don't
    // retrigger the envelope when a new note steals this voice.
    void setGlide  (float glideTimeSec);
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
    void setLegato (bool  legatoOn)      noexcept { mLegatoMode = legatoOn; }

    // ── New parameter setters (call from processBlock preamble) ───────────────
    void setPhaseInit      (float start, float rand)                        noexcept;
    void setTremoloParams  (int shape, float depth, float speed, float gap) noexcept;
    void setVibratoParams  (int shape, float depth, float speed, float env) noexcept;
    void setFilter2Params  (float cutoffHz, float res)                      noexcept;
    void setPitchOffset    (float semitones, float cents)                   noexcept;

    // ── 2026-04-19 (S1) newly-wired ghost params + routing matrix hooks ──────
    void setTimbreBlend (float blend)  noexcept;   // 0=all A, 1=all B; mult on partA/B levels
    void setSubOscGain  (float gain)   noexcept;   // T2-F rm_sub: extra octave-down sine
    void setEnvDepth    (float depth)  noexcept;   // T2-F rm_env: amp env effect strength
    void setFilterType  (int type)     noexcept;   // T1c filter 1 LP/HP/BP/Notch
    void setFilter2Type (int type)     noexcept;   // T1c filter 2 LP/HP/BP/Notch
    // 2026-04-19 (SLA-Impl) #19 Pitch fraction selector. Index maps to a
    // multiplier on mNoteHz: 0=1, 1=0.5, 2=0.25, 3=0.125, 4=2, 5=4, 6=8.
    void setPitchFraction (int idx)    noexcept;

    // ── 2026-04-19 (S2) - Filter envelopes + cutoff offset + kb track ─────────
    // T2-A: per-voice ADSR for each filter; envAmt scales the modulation
    // applied to the filter cutoff (-1 closes filter on note, +1 opens).
    void setFilter1Env (float a, float d, float s, float r) noexcept;
    void setFilter2Env (float a, float d, float s, float r) noexcept;
    void setFilter1EnvAmt (float amt) noexcept;
    void setFilter2EnvAmt (float amt) noexcept;
    // SLA #34: cutoff offset in semitones, applied multiplicatively in cutoff freq.
    void setFilter1CutoffOfs (float semis) noexcept;
    void setFilter2CutoffOfs (float semis) noexcept;
    // T2-N: keyboard tracking depth. 0 = cutoff fixed; 1 = cutoff tracks note.
    void setFilter1KbTrack (float depth) noexcept;
    void setFilter2KbTrack (float depth) noexcept;

    // ── 2026-04-19 (S4) - Mod XYZ pad input (dest routing moved to mod matrix)
    // Destination routing is now owned by BaySickSolsticeModRegistry; voices will
    // consume per-target curves in S4 Batch 2. For now mModX/Y/Z are held
    // but do not route anywhere at the voice level.
    void setModXYZ (float x, float y, float z) noexcept;

    // ── 2026-04-19 (S3 T2-C) - Unison Type / Alt / Phase ─────────────────────
    // Type 0 = Pure linear, 1 = Random, 2 = Drifting, 3 = Alt-only
    void setUnisonType  (int type) noexcept;
    void setUnisonAlt   (bool on)  noexcept;
    void setUnisonPhase (float amt) noexcept;   // 0..1 per-slot phase stagger

    // ── 2026-04-19 (S3 T2-N) - vel_link ──────────────────────────────────────
    // When ON (default): velocity scales both Part A + Part B equally.
    // When OFF: velocity only scales Part A; Part B uses fixed level (handy for
    // layered tones where the underneath stays at constant amplitude).
    void setVelLink (bool linkOn) noexcept { mVelLink = linkOn; }

    // ── 2026-04-20 (S4) - Mod registry + project BPM ─────────────────────────
    void setModRegistry (const BaySickSolsticeModRegistry* r) noexcept { mModRegistry = r; }
    void setBeatsPerSecond (double bps) noexcept { mBeatsPerSecond = bps; }

    // Bipolar contribution (-1..+1) for a given registered target index.
    // Used by BaySickSolsticeSynth to aggregate SynthLevel-category mods across voices.
    // Returns 0.0f if the target index is out of range or voice is silent.
    float getTargetContribution (int targetIndex) const noexcept;

    // Current amp-envelope level (0..1). Used by the "loudest voice wins"
    // aggregation rule for SynthLevel targets.
    float getCurrentEnvLevel() const noexcept;

    // S5 T2-M: accumulate this voice's partial amplitudes into `out`,
    // weighted by envelope level and per-part (A/B) level. Used by the
    // central spectrogram to aggregate across all active voices so the
    // visualiser shows the total audible spectrum (Harmor-faithful).
    // GUI-thread polling call; no locks.
    void accumulatePartialAmplitudes (float* out, int numPartials) const noexcept;

    // ── juce::SynthesiserVoice interface ──────────────────────────────────────
    bool canPlaySound  (juce::SynthesiserSound*)                         override;
    void startNote     (int midiNote, float velocity,
                        juce::SynthesiserSound*, int pitchWheelPos)      override;
    void stopNote      (float velocity, bool allowTailOff)               override;
    void renderNextBlock (juce::AudioBuffer<float>&,
                          int startSample, int numSamples)               override;
    void pitchWheelMoved (int newValue)                                  override;
    void controllerMoved (int controllerNumber, int newValue)            override;
    void setCurrentPlaybackSampleRate (double newRate)                   override;

    // ── Cut-self hard cut (QA-CutSelfReview): instant, click-free fade-out via a
    // ~1.5 ms quick-release, then the voice auto-retires.  Driven by BaySickSolsticeSynth.
    void cutFast() noexcept;

private:
    // ── Engine references ─────────────────────────────────────────────────────
    // 2026-04-20 (S4 Option 2): the shared-template engines are now called
    // "template"; per-voice private engines are allocated alongside and used
    // when per-voice modulation of wavetable-build targets is active.
    const HarmonicEngine*           mTemplateA { nullptr };
    const HarmonicEngine*           mTemplateB { nullptr };
    std::unique_ptr<HarmonicEngine> mVoiceEngineA;   // allocated in setCurrentPlaybackSampleRate
    std::unique_ptr<HarmonicEngine> mVoiceEngineB;
    bool                            mUseVoiceEngines { false };  // flipped by mod system (Batch 2b)
    juce::uint64                    mTemplateGenA { 0 };
    juce::uint64                    mTemplateGenB { 0 };

    // Pick the engine to read from this block. Switches between shared template
    // and the voice-private copy based on mUseVoiceEngines. Until Batch 2b
    // wires the mod matrix, this always returns the template.
    const HarmonicEngine* activeEngineA() const noexcept
    {
        return (mUseVoiceEngines && mVoiceEngineA) ? mVoiceEngineA.get() : mTemplateA;
    }
    const HarmonicEngine* activeEngineB() const noexcept
    {
        return (mUseVoiceEngines && mVoiceEngineB) ? mVoiceEngineB.get() : mTemplateB;
    }

    // ── Unison ────────────────────────────────────────────────────────────────
    int   mNumUnison   { 1 };
    float mDetuneCents { 0.0f };
    float mSpread      { 0.0f };

    // Per-slot state - all arrays sized kMaxUnison, no audio-thread allocation.
    float mPhaseA     [kMaxUnison] {};  // wavetable read position [0, kFFTSize)
    float mPhaseB     [kMaxUnison] {};
    float mUnisonMult [kMaxUnison] {};  // frequency ratio from detune (cents→ratio)
    float mPanL       [kMaxUnison] {};  // stereo-spread left gain per slot
    float mPanR       [kMaxUnison] {};  // stereo-spread right gain per slot

    // ── Frequency tracking (manual one-pole IIR - avoids SmoothedValue::reset
    //    clobbering current value, which would break portamento glide) ──────────
    float mCurrentHz  { 440.0f };   // current smoothed frequency (audio thread)
    float mTargetHz   { 440.0f };   // target frequency
    float mFreqCoeff  { 0.997f };   // fast coefficient (~10ms at 44.1kHz)
    float mGlideCoeff { 0.997f };   // portamento coefficient (user-controlled)
    float mGlideTimeSec { 0.0f };   // 0 = no portamento
    bool  mLegatoMode   { false };
    // Bug-L1 (2026-04-19): track whether the voice is in release tail. Legato
    // should only skip ADSR retrigger when the previous note is still HELD
    // (sustaining); if it's already releasing, a fresh noteOn must retrigger
    // the envelope, otherwise the new note plays silent (envelope stuck in release).
    bool  mInRelease    { false };

    // Volume / part-level smoothed values.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mVolSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mPartASmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mPartBSmooth;

    // ── Amplitude ADSR ────────────────────────────────────────────────────────
    juce::ADSR             mAmpADSR;
    juce::ADSR::Parameters mAmpParams { 0.01f, 0.1f, 0.8f, 0.3f };

    // ── Stereo filter 1 (2 channels - preserves unison spread) ──────────────
    // TODO (Beta): add filter ADSR modulation via mFltEnvAmt.
    juce::dsp::StateVariableTPTFilter<float> mFilter;
    float mFilterCutoff { 20000.0f };
    float mFilterRes    { 0.7071f };  // Q=0.7071 (Butterworth) = transparent at 20 kHz
    int   mFilterType   { 0 };        // 0 LP, 1 HP, 2 BP, 3 Notch (BP-subtract)
    // T1c notch helper - separate BP sample so we can compute (input - bp) for notch.
    juce::dsp::StateVariableTPTFilter<float> mFilterNotchHelper;
    juce::dsp::StateVariableTPTFilter<float> mFilter2NotchHelper;

    // ── Voice state ───────────────────────────────────────────────────────────
    double mSampleRate      { 44100.0 };
    float  mNoteHz          { 440.0f };
    float  mPitchWheelSemis { 0.0f };
    float  mPartALevel      { 1.0f };
    float  mPartBLevel      { 0.0f };
    float  mVolume          { 1.0f };
    // Batch E #2 (2026-05-01): per-note filter cutoff offset from CC74.
    // -2..+2 octaves added to filter-cutoff multiplier in renderNextBlock.
    float  mPerNoteCutoffOctaves { 0.0f };
    // QA-H per-note expression: pending stashes written by controllerMoved
    // (roll emits CCs just before each noteOn), consumed into active values at
    // startNote so a still-sounding voice keeps its own values when the next
    // note's CCs land on the channel.
    float  mPendResOffset    { 0.0f };   // CC71: -1..+1 Q offset on both filters
    float  mPendRelScale     { 1.0f };   // CC72: 0.25x..4x release-time scale
    float  mActiveResOffset  { 0.0f };
    float  mActiveRelScale   { 1.0f };
    // CC84 glide source + optional CC5/CC37 glide time (ms).  Per-note glide
    // runs the one-pole smoother with its own coefficient until the pitch
    // lands (~3 time constants inside the requested time), then hands back to
    // the normal fast/glide coefficient selection.
    int    mGlideFromNote    { -1 };
    int    mGlideTimeMsbMs   { 0 };
    int    mGlideTimeLsbMs   { 0 };
    bool   mGlideTimePending { false };
    bool   mPerNoteGlideActive { false };
    float  mPerNoteGlideCoeff  { 0.997f };
    // Engine pan POSITION from setPan(); master + note + mod pan combine as
    // ONE position through the project law at render (QA-TrueLevel SC-4/SC-5).
    float  mMasterPan       { 0.0f };
    baysick::pan::GainCache mPanGains;
    // S-7: per-note pan from CC10 (-1..+1, 0 = center) (Issue 5B: CC10 was
    // emitted, never read).
    float  mNotePan         { 0.0f };
    // S-6(C): RampSlide loudness ramp - CC86 stashes the target velocity, CC85 arms
    // a per-sample ramp of mNoteVelocity from base to slide over the glide time.
    float  mSlideTargetVel  { -1.0f };   // -1 = none pending
    // #11 (G-4): CC89 pan-ramp target (-999 = none pending).  Armed at the RP
    // takeover / RT glide noteOn; mNotePan glides current -> target (mirrors
    // the S-6(C) velocity ramp below).
    float  mPanRampPend     { -999.0f };
    float  mPanRampTarget   { 0.0f };
    float  mPanRampStep     { 0.0f };
    int    mPanRampLeft     { 0 };
    float  mVelRampTarget   { 0.0f };
    float  mVelRampStep     { 0.0f };
    int    mVelRampLeft     { 0 };

    // ── Phase init ────────────────────────────────────────────────────────────
    float mPhaseStart    { 0.0f };  // 0-1: fixed start position (0 = random)
    float mPhaseRand     { 1.0f };  // 0-1: randomisation amount

    // ── Tremolo ───────────────────────────────────────────────────────────────
    int   mTremShape     { 0 };     // 0=sine 1=triangle 2=saw 3=square
    float mTremDepth     { 0.0f };  // 0-1
    float mTremSpeed     { 3.0f };  // Hz
    float mTremGap       { 0.0f };  // dead-zone around centre 0-1
    float mTremPhase     { 0.0f };  // current LFO phase [0, 2π)

    // ── Vibrato ───────────────────────────────────────────────────────────────
    int   mVibShape      { 0 };     // 0=sine 1=triangle 2=saw 3=square
    float mVibDepth      { 0.0f };  // semitones (0-2)
    float mVibSpeed      { 5.0f };  // Hz
    float mVibEnv        { 0.0f };  // onset delay 0-1 (0=instant, 1=slow onset)
    float mVibPhase      { 0.0f };  // current LFO phase [0, 2π)
    float mVibEnvPos     { 0.0f };  // per-note onset ramp 0-1

    // ── Filter 2 ──────────────────────────────────────────────────────────────
    juce::dsp::StateVariableTPTFilter<float> mFilter2;
    float mFilter2Cutoff { 20000.0f };
    float mFilter2Res    { 0.7071f };  // Q=0.7071 (Butterworth) = transparent at 20 kHz
    int   mFilter2Type   { 0 };

    // ── 2026-04-19 (S1) newly-wired ghost params + routing matrix state ──────
    float mTimbreBlend  { 0.0f };   // 0 = unchanged levels; >0 = blend toward Part B
    float mSubOscGain   { 0.0f };   // T2-F rm_sub
    float mEnvDepth     { 1.0f };   // T2-F rm_env (1 = full envelope)
    float mSubPhase     { 0.0f };   // sub-oscillator phase accumulator

    // ── Pitch offset ──────────────────────────────────────────────────────────
    float mPitchSemitones { 0.0f };
    float mPitchCents     { 0.0f };
    // SLA-Impl #19: fraction-multiplier applied to mNoteHz before semitones+cents.
    // Default 1.0 (= idx 0 = 1/1). Set via setPitchFraction(int).
    float mPitchFracMult  { 1.0f };

    // ── 2026-04-19 (S2) - Filter envelopes (T2-A) ────────────────────────────
    juce::ADSR             mFltADSR1;
    juce::ADSR::Parameters mFltParams1 { 0.01f, 0.10f, 0.5f, 0.30f };
    float mFlt1EnvAmt   { 0.0f };
    juce::ADSR             mFltADSR2;
    juce::ADSR::Parameters mFltParams2 { 0.01f, 0.10f, 0.5f, 0.30f };
    float mFlt2EnvAmt   { 0.0f };
    // SLA #34 + T2-N: cutoff offset (semitones) + kb track depth per filter.
    float mFlt1CutoffOfs { 0.0f };
    float mFlt2CutoffOfs { 0.0f };
    float mFlt1KbTrack   { 0.0f };
    float mFlt2KbTrack   { 0.0f };

    // Filter-cutoff control tick.  Both filter envelopes advance once per SAMPLE
    // in renderNextBlock; only the derived cutoff is re-applied on this tick,
    // because a pow() plus four tan()-based coefficient updates per sample per
    // voice is not affordable.  The interval is a sample COUNT derived from the
    // live sample rate in setCurrentPlaybackSampleRate, so the tick is the same
    // wall-clock slice at every rate - a per-block tick would make the control
    // rate (and, before this, the envelope rate itself) a function of the host
    // buffer size.  The countdown deliberately persists across blocks: zeroing
    // it per block would re-tie the tick to the buffer size.
    int   mFltCtrlInterval  { 1 };
    int   mFltCtrlCountdown { 0 };
    // Coefficient values last pushed into the filter objects, so the control
    // tick can skip the setter when nothing moved.  -1 = unknown; any write to
    // the filters outside the tick must call invalidateFilterCoeffCache().
    float mAppliedCutoff1 { -1.0f };
    float mAppliedCutoff2 { -1.0f };
    float mAppliedRes1    { -1.0f };
    float mAppliedRes2    { -1.0f };

    // ── 2026-04-19 (S4) - Mod XYZ pad input values (consumed by mod matrix) ──
    float mModX { 0.0f }, mModY { 0.0f }, mModZ { 0.0f };

    // ── 2026-04-20 (S4 Batch 2b) - Per-voice mod-matrix state ────────────────
    const BaySickSolsticeModRegistry* mModRegistry { nullptr };
    juce::uint64               mModSnapGen  { 0 };
    double                     mBeatsPerSecond { 2.0 };   // 120 BPM default

    struct TargetVoiceState
    {
        bool  active          { false };  // any source for this target has non-zero depth
        float envPhase        { 0.0f };   // 0..1 over the source's LENGTH, then holds at 1
        float lfoPhase        { 0.0f };   // 0..1, wraps
        float capturedVel     { 1.0f };   // 0..1 velocity captured at note-on
        float capturedKey     { 0.5f };   // midi/127 captured at note-on
        float contribution    { 0.0f };   // bipolar -1..+1 (pitch/pan/filter/etc)
        float uniMult         { 1.0f };   // unipolar 0..2 (volume/level targets)
    };
    std::array<TargetVoiceState, (int) ModTargetIndex::NumTargets> mTargets;

    // Gate-held state: true between note-on and note-off. The mod envelope's
    // phase advance keys off it: free-running over the source's LENGTH while
    // held, then covering any remainder at the voice's amp-ADSR release rate.
    bool mModGateHeld { false };

    // Track last-applied contributions for VoiceEngine targets so we only
    // rebuild the voice wavetable when contribution changes materially.
    float mLastPluckContrib { 0.0f };
    float mLastPrismContrib { 0.0f };
    float mLastBlurContrib  { 0.0f };

    // Compute TargetVoiceState::active flags from a fresh snapshot. Called at
    // note-on + whenever mModSnapGen changes.
    void refreshModActiveFlags() noexcept;

    // Advance phases + sample all 7 sources per target; fill contributions.
    // Called once per block from renderNextBlock preamble.
    void updateModContributions (int numSamples) noexcept;

    // Apply VoiceEngine contributions (pluck/prism/blur) to the voice's
    // private engines + rebuild if deltas are above threshold.
    void applyVoiceEngineMods();

    // ── 2026-04-19 (S3 T2-C) - Extended unison state ─────────────────────────
    int   mUnisonType  { 0 };       // 0 Pure, 1 Random, 2 Drifting, 3 Alt-only
    bool  mUnisonAlt   { false };
    float mUnisonPhaseAmount { 0.0f };   // 0..1 per-slot phase stagger

    // ── 2026-04-19 (S3 T2-N) - vel_link + per-part velocity tracking ─────────
    // mVolSmooth no longer pre-multiplies velocity (refactored S3); velocity
    // is applied per-part in the render loop based on mVelLink.
    bool  mVelLink { true };
    float mNoteVelocity { 1.0f };

    // ── Helpers ───────────────────────────────────────────────────────────────
    // Any write to the filter objects made OUTSIDE renderNextBlock's control
    // tick must invalidate the applied-coefficient cache, or the tick's
    // value-changed guard will skip re-applying a value the filter no longer holds.
    void invalidateFilterCoeffCache() noexcept
    {
        mAppliedCutoff1 = -1.0f;
        mAppliedCutoff2 = -1.0f;
        mAppliedRes1    = -1.0f;
        mAppliedRes2    = -1.0f;
    }

    void  recalcUnisonSlots();
    void  updateGlideCoeff();
    float readWavetable (const float* wt, int wtMask, float phase) const noexcept;

    static float midiNoteToHz (int note, float extraSemitones = 0.0f) noexcept;
    static float lfoSample    (int shape, float phase)                noexcept;

    // T1g: per-voice random generator initialised on construction (message
    // thread). Replaces the per-startNote `juce::Random rng (Time::getHighResolutionTicks())`
    // syscall on the audio thread.
    juce::Random mRng;
};
