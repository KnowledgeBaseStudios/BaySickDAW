#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include "HarmlessModEditor.h"   // for HarmlessCurvePoint

// ── HarmlessModRegistry ───────────────────────────────────────────────────────
// Per-target modulation registry for Harmless. Implements the S4 mod matrix:
//   17 articulation targets x 7 modulation sources, each with per-tab curves
//   (ENV + IMG) + depth + length + TEMPO/GLOBAL + SPD/TNS/SKEW/PW warp knobs.
//
// Thread model:
//   UI thread  - mutates ModTarget structs directly (via findTarget/getAllTargets).
//                Calls publishSnapshot() after any edit. Cheap; just bumps an
//                atomic generation counter and publishes the live state.
//   Audio thread - at note-on only, calls getCurrentSnapshot() to grab a
//                  read-only pointer to the current published state. Holds the
//                  pointer for the voice's lifetime. Per-sample reads go
//                  through it without locks.
//
// For v1 we skip ABA-safe snapshot retirement entirely: the live ModTarget
// tree is the snapshot. As long as target/source/tab data layout does not
// move (we only mutate leaf fields + vector contents), voices see a coherent
// read. Vector resize during paint is the one hazard - guarded by a
// SpinLock around point vector mutations for safety.
// ─────────────────────────────────────────────────────────────────────────────

enum class ModSource : int {
    Envelope = 0,
    LFO,
    Velocity,
    Keyboard,
    ModX,
    ModY,
    ModZ,
    NumSources
};

// Target index = registration order in HarmlessProcessor::registerModTargets.
// Keep this enum in lock-step with that call sequence. Voices + synth dispatch
// modulation contributions by this index.
//
// Dispatch categories (Option 2 architecture - per-voice engines):
//   VoiceLocal   : applied per-sample on the voice's state (volume/pan/pitch/
//                  filter/timbre/part/unison knobs).
//   VoiceEngine  : applied to the voice's private HarmonicEngine module params;
//                  triggers a per-voice wavetable rebuild at block rate when
//                  contribution changes materially (pluck / prism / blur).
//   SynthLevel   : output-phaser params live post-voice in HarmlessSynth; the
//                  synth aggregates "loudest voice wins" and applies per-block.
enum class ModTargetIndex : int {
    Volume = 0,            // VoiceLocal
    Pan,                   // VoiceLocal
    Pitch,                 // VoiceLocal
    TimbreBlend,           // VoiceLocal
    PluckDecay,            // VoiceEngine
    PrismAmount,           // VoiceEngine
    BlurHarm,              // VoiceEngine
    Filter1Cutoff,         // VoiceLocal
    Filter2Cutoff,         // VoiceLocal
    Filter1Res,            // VoiceLocal
    Filter2Res,            // VoiceLocal
    PhaserMix,             // SynthLevel (output phaser post-voice)
    PhaserWidth,           // SynthLevel (output phaser post-voice)
    UnisonPitch,           // VoiceLocal
    PartALevel,            // VoiceLocal
    PartBLevel,            // VoiceLocal
    NumTargets
};

enum class ModTab : int {
    ENV = 0,
    IMG,
    NumTabs    // IMG is meaningful for Envelope source only.
};

struct ModCurveState
{
    std::vector<HarmlessCurvePoint> points;
    // 2026-04-20 (S4): sustainTime is deprecated / unused in the WYSIWYG
    // model. Kept on the struct for ValueTree compatibility; DSP ignores it.
    // T3-PerPointSustain will re-introduce explicit sustain markers when
    // users need ADSR-style pause behavior.
    float sustainTime { 1.0f };
    float spd  { 0.5f };
    float tns  { 0.5f };
    float skew { 0.5f };
    float pw   { 0.5f };

    // Default WYSIWYG shape: simple ramp-down. Users' drawn shape plays over
    // LENGTH beats verbatim; short notes get release-truncated via amp ADSR.
    static ModCurveState makeDefault() noexcept
    {
        ModCurveState s;
        s.points.push_back ({ 0.0f, 1.0f, 1 });
        s.points.push_back ({ 1.0f, 0.0f, 1 });
        return s;
    }
};

struct ModSourceState
{
    std::array<ModCurveState, (int) ModTab::NumTabs> tabs;
    float depth     { 0.0f };     // bipolar -1..+1
    float length    { 1.0f };     // beats if tempoSync, else seconds. Time for
                                   // the envelope phase to play 0 -> 1 while the
                                   // note gate is held. Notes shorter than LENGTH
                                   // get the remainder released over amp ADSR
                                   // release on note-off; notes longer hold the
                                   // curve's end value.
    bool  tempoSync { true };
    bool  globalAB  { false };
    // LFO-only waveform pick (0 sine, 1 tri, 2 saw, 3 square). LFO cycle
    // duration is carried by `length` above (unified with Envelope source).
    int   lfoShape  { 0 };
    int   activeTab { 0 };

    static ModSourceState makeDefault() noexcept
    {
        ModSourceState s;
        for (auto& t : s.tabs) t = ModCurveState::makeDefault();
        return s;
    }
};

struct ModTarget
{
    juce::String paramId;
    juce::String displayName;
    ModSource    activeSource { ModSource::Envelope };
    std::array<ModSourceState, (int) ModSource::NumSources> sources;

    ModTarget()
    {
        for (auto& s : sources) s = ModSourceState::makeDefault();
    }
};

// ── HarmlessModCurve ──────────────────────────────────────────────────────────
// Pure functions that sample a ModCurveState. Kept static so voice + editor
// share one canonical implementation.
//
// Warps (applied in this order inside sample()):
//   SKEW  - horizontal shift of the input x (exponential mapping).
//   PW    - pulse-width: where x=pw lands on y(0.5) on a symmetric curve.
//   (curve segment interpolation)
//   TNS   - tension: S-curve reshape of the lerp alpha inside each segment.
//
// SPD is NOT applied here - it modifies the phase advance rate and belongs
// wherever phase is incremented (voice per-block).
class HarmlessModCurve
{
public:
    // Remap x in [0,1] by SKEW (0.5 = neutral, <0.5 = left bias, >0.5 = right).
    static float applySkew (float x, float skew) noexcept
    {
        // k range: 2^-2..2^+2 = 0.25..4.0. Upper clamp at 100 is a safety
        // only; typical use sits inside 0.25..4. A prior clamp-at-1.0 bug
        // made skew > 0.5 have no visual effect.
        const float k = juce::jlimit (0.001f, 100.f, std::pow (2.0f, (skew - 0.5f) * 4.0f));
        return std::pow (juce::jlimit (0.f, 1.f, x), k);
    }
    // Remap x so that x=pw lands at y(0.5). Piecewise linear.
    static float applyPw (float x, float pw) noexcept
    {
        const float p = juce::jlimit (0.001f, 0.999f, pw);
        return (x < p) ? (x / (2.0f * p)) : (0.5f + (x - p) / (2.0f * (1.0f - p)));
    }
    // Reshape lerp alpha by TNS. 0.5 = linear; 0 = ease-in (slow start);
    // 1 = ease-out (fast start).
    static float applyTns (float a, float tns) noexcept
    {
        const float t = (tns - 0.5f) * 4.0f;          // range -2..+2
        if (std::abs (t) < 0.001f) return a;
        if (t > 0.0f)
            return std::pow (a, 1.0f / (1.0f + t));   // ease-out
        return std::pow (a, 1.0f + std::abs (t));     // ease-in
    }

    // Sample the warped curve at x in [0,1]. Returns y in [0,1].
    // If the point vector is empty, returns 0.5 (neutral).
    static float sample (const ModCurveState& curve, float x) noexcept
    {
        if (curve.points.empty()) return 0.5f;

        // Apply x-space warps (skew, then pw).
        x = applySkew (juce::jlimit (0.f, 1.f, x), curve.skew);
        x = applyPw   (x, curve.pw);

        // Find the two points bracketing x. Vector order is NOT assumed sorted
        // (editor adds points in click order). lo = latest point with time<=x,
        // hi = earliest point with time>=x, regardless of vector position.
        int lo = -1, hi = -1;
        for (int i = 0; i < (int) curve.points.size(); ++i)
        {
            const float t = curve.points[i].time;
            if (t <= x)
                if (lo < 0 || curve.points[lo].time < t) lo = i;
            if (t >= x)
                if (hi < 0 || curve.points[hi].time > t) hi = i;
        }

        if (lo < 0 && hi >= 0)  return curve.points[hi].value;
        if (lo >= 0 && hi < 0)  return curve.points[lo].value;
        if (lo == hi)           return curve.points[lo].value;

        const auto& p0 = curve.points[lo];
        const auto& p1 = curve.points[hi];
        const float span = p1.time - p0.time;
        float a = (span > 1.0e-6f) ? (x - p0.time) / span : 0.0f;
        a = applyTns (juce::jlimit (0.f, 1.f, a), curve.tns);

        switch (p0.curveType)
        {
            case 0:  return p0.value + a * (p1.value - p0.value);              // linear
            case 2:  return p0.value;                                          // step
            default: { const float s = a * a * (3.f - 2.f * a);                // smooth
                       return p0.value + s * (p1.value - p0.value); }
        }
    }

    // Standard LFO shape helpers. phase in [0,1]. Returns [0,1] (unipolar).
    static float sampleLfoShape (int shape, float phase) noexcept
    {
        phase = phase - std::floor (phase);
        switch (shape)
        {
            case 0:  return 0.5f + 0.5f * std::sin (phase * juce::MathConstants<float>::twoPi); // sine
            case 1:  return (phase < 0.5f) ? (phase * 2.0f) : (2.0f - phase * 2.0f);            // tri
            case 2:  return phase;                                                              // saw up
            case 3:  return (phase < 0.5f) ? 1.0f : 0.0f;                                       // square
            default: return 0.5f;                                                               // s&h fallback
        }
    }

    // Map raw [0,1] curve/LFO output + depth (-1..+1) to bipolar contribution
    // in [-1,+1]: centered at 0.5 of the curve means no modulation.
    static float toBipolar (float raw, float depth) noexcept
    {
        return (raw - 0.5f) * 2.0f * depth;
    }
};

class HarmlessModRegistry
{
public:
    HarmlessModRegistry();

    // Registration (UI thread, at processor startup).
    void registerTarget (juce::String paramId, juce::String displayName);

    // Lookup.
    ModTarget*       findTarget (const juce::String& paramId) noexcept;
    const ModTarget* findTarget (const juce::String& paramId) const noexcept;

    // Returns all registered targets in registration order. Stable for the
    // lifetime of the registry (targets are owned by unique_ptr).
    const std::vector<std::unique_ptr<ModTarget>>& getAllTargets() const noexcept
        { return mTargets; }

    // Snapshot API - audio-thread safe read side.
    // At note-on, voices call getSnapshotGeneration() and cache it; they also
    // cache pointers to the ModTarget structs they consume. Pointers are
    // stable for the registry's lifetime. Leaf-field edits race-safely with
    // voice reads because voices only read per-sample primitives after the
    // initial snapshot; vector resize is bracketed by mEditLock.
    juce::uint64 getSnapshotGeneration() const noexcept
        { return mGeneration.load (std::memory_order_acquire); }

    // Call from UI after any curve/knob edit.
    void publishSnapshot() noexcept
        { mGeneration.fetch_add (1, std::memory_order_release); }

    // Scoped lock for UI-side vector mutations (push/erase of curve points).
    // The audio thread briefly yields if it's mid-read when resize happens.
    juce::SpinLock& getEditLock() noexcept { return mEditLock; }

    // ── Persistence ───────────────────────────────────────────────────────────
    // Serializes the full registry state as a single ValueTree with identifier
    // "harmlessMod". Intended to be attached as a child of apvts.state.
    juce::ValueTree toValueTree() const;

    // Restores target state from a prior toValueTree() output. Unknown
    // target ids are ignored (forward/back compatibility).
    void fromValueTree (const juce::ValueTree&);

    static const juce::Identifier& rootId() noexcept;

private:
    std::vector<std::unique_ptr<ModTarget>> mTargets;
    std::atomic<juce::uint64>               mGeneration { 0 };
    juce::SpinLock                          mEditLock;
};
