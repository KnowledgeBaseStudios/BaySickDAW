#pragma once
#include <JuceHeader.h>
#include "HarmonicEngine.h"
#include "AdditiveVoice.h"
#include "HarmlessModRegistry.h"
#include "SynthSound.h"

// ── HarmlessSynth ─────────────────────────────────────────────────────────────
// Polyphonic synthesiser: 16 AdditiveVoice instances sharing two HarmonicEngines
// (Part A and Part B).
//
// All voices share the same engine wavetables - the wavetables are read-only
// at audio time, so sharing is safe without locks.
// Parameter changes (unison, envelope, filter, etc.) are broadcast to all
// voices in one call and take effect sample-accurately via SmoothedValues.
// ─────────────────────────────────────────────────────────────────────────────
class HarmlessSynth
{
public:
    static constexpr int kNumVoices = 16;

    HarmlessSynth();
    ~HarmlessSynth();

    // ── Audio lifecycle ───────────────────────────────────────────────────────
    void prepare        (double sampleRate, int maxBlockSize);
    void renderNextBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&);
    void allNotesOff();

    // ── Engine access ─────────────────────────────────────────────────────────
    // Used by HarmlessProcessor and (in 3A-Final) HarmlessEditor to edit
    // individual partial amplitudes / trigger a wavetable rebuild.
    HarmonicEngine& partA() { return mPartA; }
    HarmonicEngine& partB() { return mPartB; }
    const HarmonicEngine& partA() const { return mPartA; }
    const HarmonicEngine& partB() const { return mPartB; }

    // ── Voice parameter broadcast ─────────────────────────────────────────────
    // Each method propagates the change to all 16 voices atomically.
    void setUnison       (int numVoices, float detuneCents, float spread);
    void setPartLevels   (float partALevel, float partBLevel);
    void setAmpEnv       (float attackSec, float decaySec, float sustain, float releaseSec);
    void setFilterParams (float cutoffHz, float resonance);
    void setVolume       (float vol);
    void setPan          (float pan);

    // ── Beta: portamento / legato ─────────────────────────────────────────────
    void setGlide  (float glideTimeSec);
    void setLegato (bool legatoOn);

    // ── Session E: Cut self - noteOn cuts prior voices playing the same note
    void setCutSelf (bool on) noexcept { mCutSelf = on; }

    // ── Beta: strum ───────────────────────────────────────────────────────────
    // Staggers simultaneous note-ons by strumTimeSec / (numNotes-1).
    // 0 = off (no staggering).
    void setStrumTime (float strumTimeSec) noexcept { mStrumTimeSec = strumTimeSec; }
    void setStrumDir  (int dir)            noexcept { mStrumDir = dir; }

    // ── Beta: spectral module amounts ─────────────────────────────────────────
    // 2026-04-19 (S3.5): split into per-part setters. The A versions touch
    // mPartA only; the B versions touch mPartB only. Old "both" setters kept
    // for backward compatibility but unused by the processor (they call A+B).
    void setPrismAmount      (float amount);   // back-compat: A + B
    void setPluckDecay       (float amount);   // back-compat: A + B
    void setBlurSize         (float amount);   // back-compat: A + B
    void setFilterMaskAmount (float amount);   // back-compat: A + B
    void setPhaserMaskRate   (float rate);     // back-compat: A + B
    void setBrownianAmount   (float amount);   // back-compat: A + B
    // Per-part DSP setters (S3.5).
    void setPrismAmountA      (float amount);
    void setPrismAmountB      (float amount);
    void setPluckDecayA       (float amount);
    void setPluckDecayB       (float amount);
    void setBlurSizeA         (float amount);
    void setBlurSizeB         (float amount);
    void setFilterMaskAmountA (float amount);
    void setFilterMaskAmountB (float amount);
    void setPhaserMaskRateA   (float rate);
    void setPhaserMaskRateB   (float rate);
    void setBrownianAmountA   (float amount);
    void setBrownianAmountB   (float amount);

    // ── New parameter broadcasts ──────────────────────────────────────────────
    void setPhaseInit      (float start, float rand)                        noexcept;
    void setTremoloParams  (int shape, float depth, float speed, float gap) noexcept;
    void setVibratoParams  (int shape, float depth, float speed, float env) noexcept;
    void setFilter2Params  (float cutoffHz, float res)                      noexcept;
    void setPitchOffset    (float semitones, float cents)                   noexcept;
    void setOutputPhaserParams (float mix, float depth, float rate)         noexcept;
    // 2026-04-19 (SLA-Impl) Phaser WIDTH (-> juce phaser feedback) + OFS
    // (-> juce phaser centre frequency).
    void setOutputPhaserExtras (float width, float ofsHz)                   noexcept;
    // 2026-04-19 (SLA-Impl) Pitch fraction selector (chicken-head). Index 0..6
    // maps to multipliers: 1, 0.5, 0.25, 0.125, 2, 4, 8 - applied to the note
    // fundamental Hz before pitch_semitones + cents.
    void setPitchFraction (int idx)                                          noexcept;

    // ── 2026-04-19 (S2) - Filter envelopes / cutoff ofs / kb track ───────────
    void setFilter1Env       (float a, float d, float s, float r) noexcept;
    void setFilter2Env       (float a, float d, float s, float r) noexcept;
    void setFilter1EnvAmt    (float amt) noexcept;
    void setFilter2EnvAmt    (float amt) noexcept;
    void setFilter1CutoffOfs (float semis) noexcept;
    void setFilter2CutoffOfs (float semis) noexcept;
    void setFilter1KbTrack   (float depth) noexcept;
    void setFilter2KbTrack   (float depth) noexcept;

    // ── 2026-04-19 (S4) - Mod XYZ pad value (destinations removed) ────────────
    // Destination routing moved into HarmlessModRegistry (per-target mod matrix).
    // The X/Y/Z values remain as input to the new mod system via the Registry's
    // ModSource::ModX/Y/Z sources.
    void setModXYZ (float x, float y, float z) noexcept;

    // ── 2026-04-19 (S4 AG-1) - Auto-gain mode ─────────────────────────────────
    // 0 Relative, 1 Absolute. Forwards to both HarmonicEngines.
    void setAutoGainMode (int mode) noexcept;

    // ── 2026-04-20 (S4 Batch 2b) - Mod registry + tempo feed ─────────────────
    // Processor calls this once after construction, forwarding the same
    // HarmlessModRegistry that owns the serialized state chunk. All voices
    // cache this pointer and pull snapshots at note-on.
    void setModRegistry (const HarmlessModRegistry* r) noexcept;

    // Stash the registry pointer so applyGlobalLfoToAllTargets can walk it.
    const HarmlessModRegistry* getModRegistry() const noexcept { return mModRegistry; }

    // Processor calls this per-block with the current playhead BPM (falls back
    // to 120 BPM when no transport). Drives envelope length + LFO rate when
    // tempoSync is on for a given source.
    void setBeatsPerSecond (double bps) noexcept;

    // 2026-04-20 (S4 Batch 4 fix): global LFO knobs act as a MACRO - they
    // copy to every target's LFO source in the mod registry. Per-target
    // override still works via the mod editor. The DSP always reads
    // per-target values. rateIdx = 13-step index, shape 0..3, tempoSync bool.
    void applyGlobalLfoToAllTargets (int rateIdx, int shape, bool tempoSync);

    // 2026-04-30: T2-B LFO depth shortcuts (restored after the S4 strip).
    // Three global slider values that route to per-voice destinations:
    //  - lfo_vol   → ModTarget("volume")'s LFO source depth
    //  - lfo_pitch → ModTarget("pitch_semitones")'s LFO source depth
    //  - lfo_vel   → noteOn velocity scaling (handled internally - there's
    //    no Velocity ModTarget since velocity is a one-shot, not continuous)
    // All three are bipolar -1..+1.  Replace semantics: writing a non-zero
    // depth via these macros clobbers any per-target depth set in the
    // in-player Mod Editor (matches the rate/shape macro pattern).
    void applyGlobalLfoVolDepth   (float depth) noexcept;
    void applyGlobalLfoPitchDepth (float depth) noexcept;

    // lfo_vel is a separate path: stores depth + the global LFO state used
    // to sample the LFO at note-on time.  Voice velocity at note-on is
    // scaled by (1 + LFO * depth * 0.5) for ±50% per-note variation when
    // depth is at full +1 / -1.
    void setGlobalLfoVelDepth     (float depth) noexcept { mGlobalLfoVelDepth = depth; }
    // Called when the global rate / shape / tempoSync params change.  rateLen
    // is in beats when tempoSync is true, seconds otherwise (matches the
    // applyGlobalLfoToAllTargets convention).
    void setGlobalLfoVelRate      (float rateLen, int shape, bool tempoSync) noexcept;
    // Called per block from renderNextBlock to advance the global LFO phase.
    void tickGlobalLfoVel         (int numSamples) noexcept;

    // S5 T2-M: fill `outBuf[0..numPartials-1]` with the sum of partial
    // amplitudes across all active voices (each weighted by that voice's
    // env level + per-part A/B level). Called from the GUI thread at ~30 Hz
    // by the central spectrogram visualiser. Non-blocking.
    void getAggregatedPartialAmplitudes (float* outBuf, int numPartials) const;

    // ── 2026-04-19 (S2 SLA) - Blur extensions on the engine wavetable ─────────
    // S3.5: split into A/B variants.
    void setBlurTime  (float t);   // back-compat: A + B
    void setBlurHarm  (float h);   // back-compat: A + B
    void setBlurTimeA (float t);
    void setBlurTimeB (float t);
    void setBlurHarmA (float h);
    void setBlurHarmB (float h);

    // ── 2026-04-19 (S3 T2-C) - Unison Type / Alt / Phase + Part B shape ──────
    void setUnisonType  (int type);
    void setUnisonAlt   (bool on);
    void setUnisonPhase (float amt);
    void setPartBShape  (int shape);   // mirrors partA().setShape via partB()

    // ── 2026-04-19 (S1) newly-wired previously-ghost params ───────────────────
    // T2-N timbre_blend: 0 = all Part A, 1 = all Part B; multiplied on top of
    //   per-part level knobs (so users can independently mix and crossfade).
    void setTimbreBlend (float blend) noexcept;
    // T2-D prism_mode: 0 = stretched, 1 = bunched, 2 = scattered.
    // S3.5: split into A/B variants.
    void setPrismMode   (int mode);          // back-compat: A + B
    void setPrismModeA  (int mode);
    void setPrismModeB  (int mode);
    // T2-N pluck_blur: ON adds a soft attack curve on top of the pluck decay.
    // S3.5: split into A/B variants.
    void setPluckBlur   (bool blurOn);       // back-compat: A + B
    void setPluckBlurA  (bool blurOn);
    void setPluckBlurB  (bool blurOn);
    // T2-N strum_tns: tension on the strum delay distribution curve.
    //   -1 = bunched at end, 0 = linear, +1 = bunched at start.
    void setStrumTension (float tns) noexcept { mStrumTns = juce::jlimit (-1.f, 1.f, tns); }
    // T2-N vel_link: ON = velocity scales both Part A + B equally; OFF = velocity
    //   only scales Part A (Part B uses fixed level - useful for layered layers).
    // S3 wired: now actually broadcasts to all voices so the per-voice mVelLink
    // takes effect (previously stored on HarmlessSynth only with no propagation).
    void setVelLink (bool linkOn) noexcept
    {
        if (linkOn == mVelLink) return;
        mVelLink = linkOn;
        forEachVoice ([linkOn] (AdditiveVoice& v) { v.setVelLink (linkOn); });
    }
    // T2-N legato_limit: legato glide max time in seconds. Caps the glide_time.
    // S2 wired: re-broadcasts the effective glide on change so the cap takes
    // effect immediately without waiting for a glide_time change.
    void setLegatoLimit (float seconds) noexcept
    {
        const float v = juce::jmax (0.f, seconds);
        if (v == mLegatoLimit) return;
        mLegatoLimit = v;
        // Re-clamp current user glide and rebroadcast.
        const float effective = juce::jmin (mUserGlideTime, mLegatoLimit);
        forEachVoice ([effective] (AdditiveVoice& v2) { v2.setGlide (effective); });
    }
    // T1a oeq_mix: amount of an output tilt EQ applied post-voice. 0 = neutral,
    //   1 = full ±6 dB tilt around 1 kHz.
    void setOutputEQMix (float mix) noexcept;
    // T1c filter type wiring: 0 LP, 1 HP, 2 BP, 3 Notch (BP-subtract).
    void setFilterType  (int t);
    void setFilter2Type (int t);
    // T2-F routing matrix - sub osc gain / nyquist protect / output saturation /
    //   FX wet / output gain trim / env depth on amp curve. See HarmlessProcessor
    //   updateFromApvts comment for per-control semantics.
    void setRoutingMatrix (float sub, float prot, float clip,
                           float fx,  float vol,  float env) noexcept;

private:
    void applyStrum (juce::MidiBuffer& midi, int numSamples);

    HarmonicEngine mPartA;
    HarmonicEngine mPartB;

    // juce::Synthesiser manages voice stealing and MIDI routing.
    juce::Synthesiser mSynth;

    double mSampleRate    { 44100.0 };
    float  mStrumTimeSec  { 0.0f };
    int    mStrumDir      { 0 };    // 0=up 1=down 2=random
    bool   mCutSelf       { false };  // Session E

    // Cached pointer to the registry so global-LFO macro writes can reach it.
    const HarmlessModRegistry* mModRegistry { nullptr };

    // 2026-04-30: global LFO state for the lfo_vel note-on velocity scaling.
    // Phase advances per block in tickGlobalLfoVel and is sampled at each
    // noteOn message (intercepted in renderNextBlock).  Volume + pitch
    // depths route through the mod registry instead, so they don't need
    // any state here.
    // Cycle is stored as raw rate length + tempo-sync flag and converted
    // to seconds at tick time using the current bps - that way BPM changes
    // mid-session adjust the LFO period without any explicit recompute.
    float mGlobalLfoVelDepth        { 0.0f };   // -1..+1 from lfo_vel slider
    float mGlobalLfoPhase01         { 0.0f };   // 0..1, advances per block
    float mGlobalLfoRateLen         { 1.0f };   // beats (if tempoSync) or sec
    bool  mGlobalLfoTempoSync       { true  };
    int   mGlobalLfoShape           { 0 };      // 0=sine 1=tri 2=saw 3=square
    double mGlobalLfoBps            { 2.0  };   // mirrors mSampleRate-side bps

    // S5 T2-P: background thread for off-audio-thread wavetable rebuilds.
    // mPartA + mPartB register as TimeSliceClients; the thread polls their
    // `consumeRebuildRequest()` and runs `buildWavetable()` off-thread when
    // a UI change flipped the request. Voice-mod rebuilds (rare, sparse)
    // stay on the audio thread via applyVoiceEngineMods per Option A.
    struct WavetableRebuildClient : public juce::TimeSliceClient
    {
        HarmonicEngine* engine { nullptr };
        int useTimeSlice() override
        {
            if (engine && engine->consumeRebuildRequest())
                engine->buildWavetable();
            return 25;   // ms sleep between polls = 40 Hz rebuild rate
        }
    };
    WavetableRebuildClient mRebuildClientA;
    WavetableRebuildClient mRebuildClientB;
    juce::TimeSliceThread  mBackgroundThread { "HarmlessRebuild" };

    // Output phaser (applied after Synthesiser render)
    juce::dsp::Phaser<float>  mOutputPhaser;
    float mOutputPhaserMix   { 0.0f };
    bool  mOutputPhaserReady { false };

    // 2026-04-19 (S1): newly-wired ghost-param state.
    float mStrumTns       { 0.0f };          // T2-N strum tension curve
    bool  mVelLink        { false };         // T2-N
    float mLegatoLimit    { 0.5f };          // T2-N seconds
    float mUserGlideTime  { 0.0f };          // S2: pre-clamp glide value from setGlide
    float mOutputEqMix    { 0.0f };          // T1a tilt EQ amount

    // T2-F routing matrix state. Defaults preserve current behaviour.
    float mRmSub  { 0.0f };
    float mRmProt { 0.0f };
    float mRmClip { 0.0f };
    float mRmFx   { 1.0f };
    float mRmVol  { 1.0f };
    float mRmEnv  { 1.0f };

    // T1a tilt EQ filters: low-shelf + high-shelf with opposite-sign gain.
    juce::dsp::IIR::Filter<float> mTiltLoL, mTiltLoR, mTiltHiL, mTiltHiR;
    bool mTiltReady { false };
    void rebuildTiltEQ();

    // Convenience - iterates all voices and casts.
    template <typename Fn>
    void forEachVoice (Fn&& fn)
    {
        for (int i = 0; i < mSynth.getNumVoices(); ++i)
            if (auto* v = dynamic_cast<AdditiveVoice*> (mSynth.getVoice (i)))
                fn (*v);
    }
};
