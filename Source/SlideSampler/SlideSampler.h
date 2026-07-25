#pragma once
#include <JuceHeader.h>
#include "SlideRegionMap.h"
#include "SlideSampleCache.h"
#include <array>
#include <vector>
#include <atomic>
#include <cmath>
#include <functional>

// ── SlideSampler ─────────────────────────────────────────────────────────────
// QA-SlideSampler Task 2 (silky-gliding-lynx) + QA-G3Smoke Task 11 (G-1): the
// VOICED blended multi-sample slide DSP for the sfizz-backed Guitars/Basses
// engines.  As the continuous pitch moves it picks the sample at-or-just-below
// and bends it UP <=1 semitone (SL-2), crossfading zones at semitone
// boundaries; every voice now runs the patch's real voicing chain:
//
//   zone sample -> Lagrange resample (keycenter delta + micro-bend + tune +
//   unison detune + LFO pitch + pitcheg) -> gain (volume x velcurve/veltrack
//   x AHDSR x LFO tremolo x crossfade sin) -> keytracked LPF (bass) / 1-pole
//   HPF 250 (unison layers) -> per-voice pan -> stereo sum.
//
// Modulation depths on these patches are almost all `_oncc`-scaled, so the
// audible modulation arrives with the LIVE CC values: setCcProvider() (Task 12
// wires it to the engine's APVTS cc atomics; the kit panel then drives the
// slide for free).  With no provider the routes evaluate at cc=0 -- exactly
// sfizz's behavior with no CC input.
//
// Threading (category 6):
//  - prepare()/setProgram()/setCcProvider() are MESSAGE-thread.  setProgram()
//    runs inside loadKit while the engine's processing gate is OFF; tables are
//    immutable while audio runs.  G-11: it synchronously decodes EVERY
//    articulation's every layer set (zero keyswitch latency).
//  - startSlide()/moveTo()/release()/stopAllNow()/renderNextBlock()/isActive()
//    are AUDIO-thread and alloc/lock-free.  The CC provider is called at
//    block rate and must be lock-free (Task 12 passes an atomics reader).
// ─────────────────────────────────────────────────────────────────────────────
class SlideSampler
{
public:
    SlideSampler() = default;

    void prepare (double sampleRate, int maxBlockSize);

    // Message thread (loadKit, processing gate off): rebuild the voiced zone
    // tables for EVERY articulation's layer stack + decode everything (G-11).
    void setProgram (const SlideRegionMap& map);

    // Task 12 wires the engine's per-CC read (APVTS `<prefix>cc<N>` atomics,
    // 0..127).  Null = every cc reads 0 (routes present but neutral).
    void setCcProvider (std::function<float (int cc)> p) { mCcProvider = std::move (p); }

    // Task 12: keyswitch tracking (audio thread).  A noteOn matching an
    // articulation's sw_last selects that articulation's tables for the NEXT
    // gesture (an active gesture keeps the tables it started on -- they are
    // immutable).  Returns true when the note was a keyswitch.
    bool trySelectArticulation (int note) noexcept;

    // Task 12 (bass cc105 Mono / G-12 middle ground): fade every sounding
    // voice over `seconds` (0.2 s = the patch off_time choke feel).
    void chokeAll (float seconds) noexcept;

    // Task 12: the gesture's current pitch (audio thread; for the cut-self
    // same-pitch match).  Meaningless when !isActive().
    int currentNote() const noexcept { return (int) std::lround (mCurrentPitch); }

    // Audio thread, alloc/lock-free.  NOTE (G-12): startSlide no longer
    // blanket-resets sounding voices -- the OLD tail's fate is the CALLER'S
    // policy (cut-self ON -> stopAllNow first; OFF -> it rings through its
    // release).  Bass cc105 Mono chokes internally when the set is present.
    void startSlide (float startPitchSemis, int velocity, bool reAttack = true);
    void moveTo     (float currentPitchSemis) noexcept;
    // Slide-end: voices enter their AHDSR release stage (the patch
    // ampeg_release -- #4's ring-fix mechanism) + release-triggered layer
    // zones spawn (rt_decay-attenuated by the held time).
    void release()  noexcept;
    // G-12 cut-self-ON / #5 all-notes-off: hard-stop every voice through a
    // short declick ramp (~7 ms) regardless of envelope stage.
    void stopAllNow() noexcept;
    void renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples);
    // Atomic: read by InstStripTask's idle-suspend predicate (worker thread)
    // as well as the audio thread.
    bool isActive() const noexcept { return mAnyActive.load (std::memory_order_acquire); }
    bool hasProgram() const noexcept
    {
        const int a = mActiveArt.load (std::memory_order_acquire);
        return a >= 0 && a < (int) mArts.size()
            && ! mArts[(size_t) a].center.bands.empty();
    }

    // SS-Q5 TUNE knobs (defaults re-based by #6: offset floor up toward the
    // sustain body, crossfade shorter -- see the tuning checklist).
    void setCrossfadeMs    (float ms) noexcept { mCrossfadeMs    = juce::jmax (1.0f, ms); }
    void setAttackOffsetMs (float ms) noexcept { mAttackOffsetMs = juce::jmax (0.0f, ms); }

private:
    // One block-rate LFO route parsed from a zone's modOps (Task 10 capture).
    struct LfoDef
    {
        int   index = 0;             // lfoNN
        float freqHz = 0.0f;         // static lfoNN_freq
        float delaySec = 0.0f;       // lfoNN_delay (+ _oncc via cc route)
        float fadeSec  = 0.0f;       // lfoNN_fade
        int   wave     = 0;          // 0 sine (patches use sine/tri; parsed, others fall back)
        // cc-scaled target depths: value = depth * cc/127 (curve applied when set).
        struct CcDepth { int cc = -1; float depth = 0.0f; int curve = -1; };
        CcDepth pitchCents;          // lfoNN_pitch_onccK
        CcDepth volumeDb;            // lfoNN_volume_onccK
        CcDepth cutoffCents;         // lfoNN_cutoff_onccK
        CcDepth freqModHz;           // cross-LFO: lfoNN_freq_lfoMM_onccK (applied to lfo MM? --
        int     freqModTarget = -1;  //   parsed as: THIS lfo's freq is modulated by lfo[target])
        CcDepth delayCc;             // lfoNN_delay_onccK (guitar cc116 vibrato delay)
        CcDepth fadeCc;              // bass cc116 fade
    };

    // A zone with its full voicing (copied from SlideSample at setProgram).
    struct Zone
    {
        int rootKey = 60;
        juce::File wav;
        std::shared_ptr<DecodedSlideSample> sample;

        int   offsetFrames = 0;                 // Task 11: captured `offset` now carried
        int   loopStart = -1, loopEnd = -1;     // noise layer loops
        bool  looped = false;
        float tuneCents = 0.0f;
        float volumeDb = 0.0f, amplitudePct = 100.0f, ampVeltrack = 100.0f;
        std::vector<std::pair<int, float>> velcurve;
        float ampegDelay = 0, ampegAttack = 0, ampegHold = 0, ampegDecay = 0;
        float ampegSustain = 100.0f, ampegRelease = 0.25f;
        // Task 12b: ampeg `_oncc` mods (guitar cc70 env, bass cc24 swell /
        // cc107 release).  Attack/hold/decay/sustain latch at trigger;
        // release latches at release() -- sfizz evaluates EG mods per trigger.
        LfoDef::CcDepth envAttackCc, envHoldCc, envDecayCc, envSustainCc, envReleaseCc;
        // Task 12b: pitcheg (guitar attack pitch contour).  Linear A/D toward
        // sustain level; cents = depth x level, cc-scalable depth.
        bool  hasPeg = false;
        float pegAttack = 0.0f, pegDecay = 0.0f, pegSustainPct = 100.0f;
        float pegDepthCents = 0.0f;
        LfoDef::CcDepth pegDepthCc;
        // Task 12c: fileg (bass filter EG) -- same linear A/D shape as
        // pitcheg, cents onto the cutoff.
        bool  hasFileg = false;
        float filegAttack = 0.0f, filegDecay = 0.0f, filegSustainPct = 100.0f;
        float filegDepthCents = 0.0f;
        LfoDef::CcDepth filegDepthCc;
        // Task 12c: gain_cc alias -- volume in dB riding a CC.
        LfoDef::CcDepth volDbCc;
        // Task 12c: ARIA varNN kludge (bass filter): the var's value is the
        // PRODUCT (mod=mult; sum otherwise) of its cc inputs, applied to the
        // cutoff as varNN_cutoff cents.  The shipped kits use var01/var02
        // with 1-2 inputs each; cutoff is the only target evaluated.
        struct VarDef
        {
            int  index = 0;
            bool mult  = true;
            std::array<LfoDef::CcDepth, 4> inputs;
            int  numInputs = 0;
            float cutoffCents = 0.0f;
        };
        std::vector<VarDef> vars;
        float rtDecay = 0.0f;
        bool  unisonHpf = false;                // t-layers: 1-pole HPF 250 (bass fil2)
        // Filter statics (bass): absent cutoff = bypass.
        bool  hasLpf = false;
        float cutoffCents = 0.0f;               // `cutoff` as cents-style value (Hz stored)
        float cutoffHz = 20000.0f;
        float filKeytrack = 0.0f;
        // Task 12b/12c: static filter CC routes (bass cutoff_cc92 in cents,
        // resonance_cc91 in Q offset), block-rate.  fileg_* + var01/var02 are
        // ALSO evaluated (12c) -- see renderVoice.
        LfoDef::CcDepth cutoffCc;
        LfoDef::CcDepth resoCc;
        // CC routes.  gainCc = ANY `amplitude_oncc<K>` (unison cc100, feedback/
        // noise cc29) -- evaluated at BLOCK rate on top of the static
        // `amplitude`, so an amplitude=0 + oncc layer is silent at cc 0 and
        // follows the CC live (Task 12b: this was captured but never applied).
        LfoDef::CcDepth gainCc;
        LfoDef::CcDepth uniPan;                 // pan_oncc101 (signed by layer)
        LfoDef::CcDepth uniTuneCents;           // tune_cc102
        std::vector<LfoDef> lfos;               // parsed block-rate LFO bank
    };
    struct Band
    {
        int loVel = 1, hiVel = 127;
        std::vector<Zone> zones;                // ascending rootKey; immutable in playback
    };
    struct LayerTables { std::vector<Band> bands; };

    // Task 12: EVERY articulation's voiced tables, built at setProgram (all
    // samples already resident per G-11).  The active index switches on a
    // keyswitch noteOn -- an atomic swap over immutable tables, audio-safe.
    struct ArtSet
    {
        int swLast = -1;
        LayerTables center, tUp, tDown, tailpiece;
        std::vector<Zone> releases, noise, feedback;
        bool  hasMonoSet  = false;
        float monoOffTime = 0.2f;
    };

    struct Voice
    {
        bool   active = false;
        bool   killed = false;                  // stopAllNow declick in flight
        const Zone* zone = nullptr;
        const DecodedSlideSample* sample = nullptr;
        int    rootKey = 60;
        int    readIdx = 0;
        double ratio   = 1.0;
        float  baseGain = 1.0f;                 // velocity gain (velcurve path)
        float  fade = 0.0f, fadeStep = 0.0f;    // crossfade sin ramp
        float  pan = 0.0f;                      // -1..1
        // AHDSR state.  Stage lengths are LATCHED at trigger (release at
        // release()) from zone statics + their `_oncc` routes -- Task 12b.
        enum class EnvStage { Delay, Attack, Hold, Decay, Sustain, Release, Done };
        EnvStage envStage = EnvStage::Attack;
        float  envLevel = 0.0f;
        double envTimeSec = 0.0;
        float  envAttackSec = 0.0f, envHoldSec = 0.0f, envDecaySec = 0.0f;
        float  envSustainLvl = 1.0f, envReleaseSec = 0.25f;
        // pitcheg / fileg state (block-rate linear A/D).
        double pegTimeSec   = 0.0;
        double filegTimeSec = 0.0;
        // Review fixes (batch close): gestureSerial scopes the moveTo hop
        // fade to the CURRENT gesture only, so a prior gesture's ring-out
        // tails survive per G-12 OFF; baseCents is the trigger-time
        // root-relative + tune + unison-detune sum (updateVoiceRatio) so
        // block-rate pitch modulation never discards it; lastCutoffHz/lastResQ
        // gate the TPT setters on change (CPU-safeguard rule).
        juce::uint32 gestureSerial = 0;
        double baseCents    = 0.0;
        float  lastCutoffHz = -1.0f;
        float  lastResQ     = -1.0f;
        // Per-voice filters.
        juce::dsp::StateVariableTPTFilter<float> lpf;
        bool   lpfOn = false;
        float  hpfState = 0.0f;                 // 1-pole HPF (unison layers)
        bool   hpfOn = false;
        // Block-rate LFO phases (parallel to zone->lfos).
        std::array<float, 4> lfoPhase   { { 0, 0, 0, 0 } };
        std::array<float, 4> lfoElapsed { { 0, 0, 0, 0 } };
        juce::uint32 age = 0;                   // steal: oldest-quietest
        juce::LagrangeInterpolator interp;
    };

    int    pickBand (const LayerTables& t, int velocity) const noexcept;
    int    pickZone (const LayerTables& t, int bandIdx, float pitchSemis) const noexcept;
    Voice* allocVoice() noexcept;
    void   triggerZone (const LayerTables& t, int bandIdx, int zoneIdx,
                        float pitchSemis, bool firstNote, float panSign) noexcept;
    // Task 12b: spawn a flat-set zone (feedback / noise / cc29 layers) as a
    // self-enveloped sustaining voice at the gesture pitch.
    void   triggerFlatZone (const Zone& z, float pitchSemis) noexcept;
    // Task 12b: cc29-gated feedback + looped-noise spawns at a trigger point.
    void   spawnCc29Layers (const ArtSet& a, float pitchSemis) noexcept;
    void   latchVoiceEnv (Voice& v, const Zone& z) noexcept;
    void   updateVoiceRatio (Voice& v, float pitchSemis) noexcept;
    void   renderVoice (Voice& v, float* L, float* R, int startSample, int numSamples) noexcept;
    float  ccValue (int cc) const noexcept;     // 0..127 via provider (0 when null)
    // Route evaluation: cc 0..1 through the route's optional <curve> table --
    // Task 12b (curves.sfz `_curvecc` support; linear when curve = -1).
    float  ccNorm (const LfoDef::CcDepth& d) const noexcept;
    float  velGainFor (const Zone& z, int velocity) const noexcept;
    static void buildLayer (LayerTables& t, const std::vector<SlideSample>& set,
                            SlideSampleCache& cache, bool unisonHpf);
    static void parseZoneMods (Zone& z, const SlideSample& s);

    // #6 + pool: 2 crossfading center voices x (1 + 2 unison) + tails + headroom.
    static constexpr int kMaxVoices = 20;

    double mSampleRate = 48000.0;
    int    mBlockSize  = 512;

    std::vector<ArtSet> mArts;
    std::atomic<int>    mActiveArt { -1 };
    const ArtSet* art() const noexcept
    {
        const int a = mActiveArt.load (std::memory_order_acquire);
        return (a >= 0 && a < (int) mArts.size()) ? &mArts[(size_t) a] : nullptr;
    }
    // The articulation the CURRENT gesture triggered on.  moveTo/release read
    // THIS (not art()) so a mid-gesture keyswitch can't mismatch band indices
    // against a different articulation's tables.  Pointer into mArts is stable:
    // the vector only mutates in setProgram (processing gate off, voices dead).
    const ArtSet* mGestureArt = nullptr;
    // Task 12b: the gesture's MAIN table -- center, or tailpiece when cc118 is
    // high at gesture start (hi/lo switch semantics; the kit panel sends
    // 0/127).  Pinned like mGestureArt: a mid-gesture cc118 flip applies to
    // the NEXT gesture, matching sfizz's trigger-time locc evaluation.
    const LayerTables* mGestureTables = nullptr;

    // Task 12b: custom <curve> tables (curves.sfz), applied by ccNorm.
    std::vector<SlideCurve> mCurves;

    std::array<Voice, kMaxVoices>  mVoices;

    // Increments per startSlide; voices stamp it at trigger so hop fades and
    // gesture-scoped operations never touch a previous gesture's tails.
    juce::uint32 mGestureSerial = 0;

    // Close review + Jeff (2026-07-24): sfizz-internal pseudo-CC inputs,
    // synthesized per gesture since no panel writer exists -- 131 = note
    // velocity, 133 = note number, 135/136 = per-gesture randoms.  Makes the
    // kits' velocity-scaled depths + random LFO phases work on slides.
    // mRand is a plain LCG member (audio-thread pure math, no shared state);
    // fixed seed = deterministic across runs.
    float        mGestureRand1  = 64.0f;
    float        mGestureRand2  = 64.0f;
    float        mGestureNoteCc = 60.0f;
    juce::Random mRand { 0x51DE5EED };

    int    mActiveBand     = -1;
    int    mActiveZoneRoot = -1000;
    Voice* mActiveVoice    = nullptr;
    std::atomic<bool> mAnyActive { false };
    float  mBaseGain       = 1.0f;
    int    mSlideVelocity  = 100;
    double mHeldSec        = 0.0;               // slide age (rt_decay for releases)
    float  mCurrentPitch   = 60.0f;

    std::function<float (int cc)> mCcProvider;  // message-set, audio-read (block rate)

    // SS-Q5 TUNE-AT-SMOKE knobs.  #6 re-base: offset floor toward the sustain
    // body (~130 ms) hides the re-pluck; crossfade ~28 ms reduces smear.  The
    // hop START now time-aligns to the outgoing voice's elapsed position, so
    // these act as the FLOOR/length, not the whole mechanism.
    float mCrossfadeMs    = 28.0f;   // SS-Q5 TUNE
    float mAttackOffsetMs = 130.0f;  // SS-Q5 TUNE

    std::vector<float> mScratch;

    // G-11 keep-alive (Task 10): every articulation's every layer set decoded.
    std::vector<std::shared_ptr<DecodedSlideSample>> mResidentHandles;

    juce::SharedResourcePointer<SlideSampleCache> mCache;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlideSampler)
};
