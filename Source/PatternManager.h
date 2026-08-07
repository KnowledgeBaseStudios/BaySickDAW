#pragma once
#include <JuceHeader.h>
#include "VibesynthConstants.h"
#include "Engine/RetirementQueue.h"   // QA-G3Smoke #30b: roll-snapshot retirement (Batch-9c primitive)
#include <cmath>
#include <limits>
#include <memory>

// QA-Ee (96 PPQ): beats<->ticks converters.  kTicksPerBeat (a plain int) lives
// in VibesynthConstants.h; these need juce::int64 so they live here, where every
// tick consumer already includes PatternManager.h.
inline juce::int64 beatsToTicks (double beats) noexcept
{
    return (juce::int64) std::llround (beats * (double) kTicksPerBeat);
}
inline double ticksToBeats (juce::int64 ticks) noexcept
{
    return (double) ticks / (double) kTicksPerBeat;
}

// ── Automation data ───────────────────────────────────────────────────────────
enum class CurveType { Linear, Stepped, Spline };

struct ControlPoint
{
    float     timeTicks  { 0.f };   // 0..1, fraction of clip length
    float     value01    { 0.5f };  // 0..1, normalized parameter value
    CurveType curveType  { CurveType::Linear };
    float     tension    { 0.f };   // reserved for future Bezier
};

// QA-ModelShell TS2: the ONE control-point evaluator, shared by the live
// engine replay (PluginProcessor) and the offline render replay so the two
// cannot drift.  relPos = 0..1 position within the clip; points assumed
// sorted by timeTicks.
template <typename PointVec>
inline float evalAutomationPointsAt (const PointVec& pts, float relPos)
{
    if (pts.empty()) return 0.5f;
    if ((int) pts.size() == 1)          return pts[0].value01;
    if (relPos <= pts.front().timeTicks) return pts.front().value01;
    if (relPos >= pts.back().timeTicks)  return pts.back().value01;

    for (int pi = 0; pi < (int) pts.size() - 1; ++pi)
    {
        if (relPos >= pts[pi].timeTicks && relPos <= pts[pi + 1].timeTicks)
        {
            if (pts[pi].curveType == CurveType::Stepped)
                return pts[pi].value01;

            const float span = pts[pi + 1].timeTicks - pts[pi].timeTicks;
            const float t    = (span > 0.f) ? (relPos - pts[pi].timeTicks) / span : 0.f;
            return pts[pi].value01 + t * (pts[pi + 1].value01 - pts[pi].value01);
        }
    }
    return pts[0].value01;
}

struct AutomationLane
{
    juce::String              paramId;
    std::vector<ControlPoint> points;
    bool  isLFO   { false };
    int   lfoShape { 0 };   // 0=Sine 1=Triangle 2=Saw 3=Square
    float lfoRate  { 1.f }; // period in beats
    float lfoMin   { 0.f };
    float lfoMax   { 1.f };

    // Display-name system (decoupled from paramId so swapping the effect in a
    // slot updates the shown name without breaking the backend key):
    //  - paramId           : stable key (e.g. "layers_bus_s0_mix"). Never displayed.
    //  - userDisplayName   : user-renamed label. Empty by default. When
    //                        non-empty, takes precedence over the auto name
    //                        everywhere in the UI. "Revert to auto" clears it.
    //  - lastKnownName     : auto-resolved label captured at lane creation,
    //                        while the target still existed.  Deleted-slot
    //                        lanes fall back to it so the row keeps saying
    //                        WHICH effect it drove.  Never goes stale for
    //                        rack lanes: a slot UUID never revives (effect
    //                        swap = old-slot deletion + a fresh UUID).
    // The auto-generated display string is otherwise NOT stored here; it's
    // resolved at display time via
    // StandaloneEditor::resolveAutomationDisplayName(paramId) so effect-swap
    // inside a slot reflects immediately without migration.
    juce::String              userDisplayName;
    juce::String              lastKnownName;

    // Evaluate the lane at normalised clip position pos01 (0 = start, 1 = end).
    // Returns a normalised 0..1 parameter value.
    float evaluateAt(float pos01) const;
};

// ── Piano roll note type ──────────────────────────────────────────────────────
// Values serialize as ints ("t" note attr); new types APPEND so existing indices
// stay stable.  RampSlide = takeover bend (no new attack - the sounding note
// bends to this pitch across this note's length); RetrigSlide = own attack, pitch
// glides in from the previous note's pitch.  Bend (QA-SlideSampler) = a native
// pitch-wheel bend by a set amount, offered on the Guitars/Basses rolls only.
enum class NoteType { Standard, RampSlide, Portamento, RetrigSlide, Bend };

// QA-SlideSampler Task 4: the pitch contour a Bend note follows over its duration
// (user-picked per note).  Serialized as an int ("bsh"); values APPEND.
enum class BendShape { RampHold, RampWhole, UpBack, InstantHold };

// ── Piano roll note ───────────────────────────────────────────────────────────
struct PianoNote
{
    int      midiNote      { 60 };                   // 0–127
    double   startBeat     { 0.0 };                  // from pattern start, in beats
    double   durationBeats { 0.25 };                 // note length in beats
    float    velocity      { 0.8f };                 // 0–1
    float    panning       { 0.0f };                 // -1 (left) to +1 (right)
    float    finePitch     { 0.0f };                 // cents ±100
    NoteType type          { NoteType::Standard };   // Standard / RampSlide / RetrigSlide / Portamento
    bool     muted         { false };                // muted notes play silently
    int      groupId       { -1 };                   // -1 = ungrouped; same id = grouped
    float    filterCutoff  { 0.5f };                 // 0=closed, 1=fully open (control lane)
    float    releaseAmt    { 0.5f };                 // release-time scale, 0.5 = neutral (CC72)
    float    resonance     { 0.5f };                 // filter resonance offset, 0.5 = neutral (CC71)
    // §P4.2 Phase C1: drum slot index (0..15) for drumRoll notes.
    //   -1 = not a drum note (used on layerRoll / bassRoll).
    //   For drum-grid-mode notes created pre-C2 this is redundant with
    //   (51 - midiNote); C2 full-roll mode will use slotIndex independently
    //   from midiNote so a slot can play any pitch.
    int      slotIndex     { -1 };
    // QA-Ee Stage 3b: 96 PPQ tick representation, authoritative on disk (serialized as
    // `st`/`dt`).  startBeat/durationBeats remain the in-memory editing values (kept
    // tick-aligned by the snap engine); `st`/`dt` are derived from them at save, and
    // these fields are filled from `st`/`dt` (or migrated legacy beats) on load.
    // KEEP THESE LAST: positional aggregate-inits elsewhere init the leading fields
    // (PianoNote{ midiNote, startBeat, durationBeats, velocity, ... }); trailing fields
    // just take their defaults there.  Inserting mid-struct breaks those brace-inits.
    // Defaults mirror startBeat 0 / durationBeats 0.25 (24t = 1/16 note).
    juce::int64 startTicks    { 0 };
    juce::int64 durationTicks { 24 };
    // QA-SlideSliceGlide S-4: Portamento glide length in beats.  A Porta note
    // glides over this instead of its own length or the block length; ignored by
    // the other note types.  Kept after the tick fields so positional brace-inits
    // (which stop at `type`, field 7) never reach it.
    double      portaLengthBeats { 1.0 };
    // QA-SlideSampler Task 4: native "Bend" note (Guitars/Basses).  bendSemitones
    // = signed bend amount, gated to the patch's real range at the UI; bendShape =
    // the pitch contour over the note.  Ignored by other note types + engines.
    // KEEP LAST (same brace-init reasoning as portaLengthBeats above).
    double      bendSemitones { 0.0 };
    BendShape   bendShape     { BendShape::RampHold };
};

struct PianoRollData
{
    std::vector<PianoNote> notes;
    int  numBars       { 2 };
    // #20 (QA-G3Smoke): set by Riff Machine's Accept; the panel pre-checks
    // "Work on existing score" on its next open so a re-run refines instead of
    // replacing.  Serialized ("riffUsed"); loader defaults false.
    bool riffMachineUsed { false };
};

// ── Basic sequence step (on-page grid) ───────────────────────────────────────
struct BasicStep
{
    bool  active      { false };
    float velocity    { 0.8f };   // 0-1
    float length      { 1.0f };   // 1.0 = full step, 0.5 = half, etc (drag to resize)
    float fxAmount    { 1.0f };   // 0-1 FX send for this step
};

// ── Simplified AHDSR for basic sequence ──────────────────────────────────────
// Applies to all notes triggered by the basic sequence on this page.
struct BasicEnvelope
{
    float attack  { 0.01f };   // sec
    float hold    { 0.0f };    // sec
    float decay   { 0.2f };    // sec
    float sustain { 0.7f };    // 0-1
    float release_{ 0.3f };    // sec
};

// ── Per-page routing ──────────────────────────────────────────────────────────
struct PageSequenceData
{
    SeqRouting routing { SeqRouting::BasicSequence };

    // Basic sequence grid -- rows depend on page:
    // Layers: up to 8 rows (one per Layers page), Bass: 1 row, Drums: MAX_DRUM_SOUNDS rows
    std::array<std::array<BasicStep, MAX_STEPS_TOTAL>, MAX_DRUM_SOUNDS> basicGrid;
    BasicEnvelope basicEnv;

    // Complex sequence data (A1-style, used when routing == ComplexSequence)
    // Each row is one element (layer / bass sound / drum sound)
    // MAX_DRUM_SOUNDS rows covers all three pages (4 layers, 1 bass, 10 drums)
    struct ComplexStep
    {
        StepType  stepType  { StepType::ShortStep };
        float     velocity  { 0.8f };
        float     fxAmount  { 1.0f };
        int       note      { 60 };
        bool      active    { true };
    };
    std::array<std::array<ComplexStep, MAX_STEPS_TOTAL>, MAX_DRUM_SOUNDS> complexGrid;

    // Complex sequence AHDSR (one per page, not per step)
    struct ComplexEnvelope
    {
        float attack  { 0.01f };
        float hold    { 0.0f };
        float decay   { 0.2f };
        float sustain { 0.7f };
        float release_{ 0.3f };
        float swing   { 0.0f };   // 0-1 swing amount
        bool  triplet { false };
    } complexEnv;

    int  bars        { DEFAULT_BARS };
    int  stepsPerBar { DEFAULT_SPB  };
    int totalSteps() const { return juce::jlimit(1, MAX_STEPS_TOTAL, bars * stepsPerBar); }

    PageSequenceData()
    {
        for (auto& row : basicGrid)   row.fill({});
        for (auto& row : complexGrid) row.fill({});
    }
};

// ── One bar-based pattern ─────────────────────────────────────────────────────
struct Pattern
{
    juce::String name        { "Pattern 1" };
    int          bars        { DEFAULT_BARS };
    int          stepsPerBar { DEFAULT_SPB  };
    // C.5b / QA-G Task 6: per-pattern time signature.  tsNum/tsDen are the
    // EFFECTIVE signature every consumer reads (roll grid, tiling, suffix,
    // pattern-mode metronome).  tsLocked == true -> USER-SET via the type-in
    // popup (the only state that spawns linked grid markers on placement).
    // tsLocked == false -> FOLLOWER: refreshPatternTimeSigs() re-derives the
    // fields live (placed -> marker at earliest block; unplaced -> bound
    // marker -> current-TS selection -> 4/4).  tsBoundMarkerUid = the
    // unplaced-follower binding, stamped at pattern creation from the
    // project's current-TS selection; re-bound to current when its marker
    // dies.  Reset-to-default clears the lock and re-enters the lifecycle.
    int          tsNum       { 4 };
    int          tsDen       { 4 };
    bool         tsLocked    { false };
    int          tsBoundMarkerUid { -1 };
    // Phase F-1 (2026-04-26): per-pattern user colour, shown on Builder grid
    // blocks + Browser pattern items.  Default = light grey.  Persisted to the
    // project XML; loaded patterns without the attribute fall back to the
    // default + the legacy `kBlockCols[idx % 8]` palette in BuilderPage.cpp
    // is no longer consulted for new content.
    juce::Colour color       { 0xffb0b0b0 };
    // Per-pattern tempo removed 2026-04-24 - tempo is a global project value
    // now (PatternManager::mGlobalTempo) and automatable via the "global_tempo"
    // paramId.  Old saved projects with <Pattern tempo="..."/> silently drop
    // the attribute on load.

    // Per-page sequence data
    PageSequenceData layerSeq;
    PageSequenceData bassSeq;
    PageSequenceData drumSeq;

    // Drum grid (legacy basic -- kept for backward compat, mirrors drumSeq.basicGrid row 0-9)
    std::array<std::array<bool, MAX_STEPS_TOTAL>, MAX_DRUM_SOUNDS> drumGrid;

    // Per-row drum sound assignment: drumRowToSlot[row] = DrumType index, -1 = None
    std::array<int, MAX_DRUM_ROWS> drumRowToSlot;

    int totalSteps() const { return juce::jlimit(1, MAX_STEPS_TOTAL, bars * stepsPerBar); }
    double stepLengthBeats() const { return 4.0 / juce::jmax(1, stepsPerBar); }

    // Per-page piano roll note data - one slot per Layers page, one per Bass
    // page (QA-Layout T11: layerRoll's literal 8 replaced with the constant;
    // it would have overflowed at the new 20-page cap).
    std::array<PianoRollData, kMaxLayerPages> layerRoll;
    std::array<PianoRollData, kMaxBassPages>  bassRoll;

    // Legacy single drum roll (pre-D1, kit-shared with slotIndex tagging).
    // Kept for backward compat / migration.  Will be removed in D1.4 cutover.
    PianoRollData drumRoll;

    // D1.1 (2026-04-24): per-drum piano-roll data.  Each drum tab owns its own
    // roll, indexed by drum-tab pageIndex (0..kMaxDrumPages-1).  Notes are
    // standard piano-roll notes - midiNote is the played pitch, no slotIndex
    // encoding.  Data populated lazily as drums are added; on project load,
    // legacy `drumRoll` notes are migrated into drumRolls[51 - midiNote].
    std::array<PianoRollData, kMaxDrumPages> drumRolls;

    // G-3 (2026-04-28): per-clip piano-roll data.  Each Clips tab owns its own
    // roll, indexed by clip-tab pageIndex (0..kMaxClipPages-1, which is also
    // the audio-row index since Clips pages map 1:1 to mixer_audio_<row>).
    // Notes are standard piano-roll notes; the active engine
    // (BaySickPlayer = sampler / BaySickNAM/IR = re-amp processor) interprets
    // them per its own semantics.  Data populated lazily as clips are spawned.
    std::array<PianoRollData, kMaxClipPages> clipRoll;

    // G-4 (2026-04-28): per-Vox / per-Inst piano-roll data.  Same shape as
    // clipRoll - pageIndex is the corresponding mixer Vox/Inst insert index
    // (0..kMaxVoxPages-1 / 0..kMaxInstPages-1).
    std::array<PianoRollData, kMaxVoxPages>  voxRoll;
    std::array<PianoRollData, kMaxInstPages> instRoll;
    // QA-ModelShell TS6 (BLU-447): per-tab roll for hosted VST3 instruments.
    std::array<PianoRollData, kMaxPluginPages> pluginRoll;

    // J-7a (2026-05-03): BaySickRustyDrums singleton piano-roll data.  One
    // PianoRollData (no array) - there's only ever zero or one BaySickRustyDrums
    // per project (1-instance lock).  The roll covers the kit's full keymap;
    // J-7b layers a drummer-conventional remap on top of dispatch.
    PianoRollData baySickRustyDrumsRoll;

    Pattern()
    {
        for (auto& arr : drumGrid) arr.fill(false);
        drumRowToSlot.fill(-1);
    }
};

// ── Time markers + per-bar time-signature changes (D-2, 2026-04-26) ─────────
// Project-scope (not per-pattern) - both lists belong to the Builder ruler.
// Markers are user-labelled flags.  TS changes apply from `bar` forward until
// the next change.  Default project tempo signature is 4/4 (encoded as the
// implicit pre-bar-0 state when `mTimeSigChanges` is empty).
struct TimeMarker
{
    int          bar   { 0 };
    juce::String label { };
};

struct TimeSigChange
{
    int bar { 0 };
    int num { 4 };   // beats per bar (numerator)
    int den { 4 };   // note value (denominator - power of 2)
    // QA-G Task 6: stable identity + link ownership.  uid survives sorts /
    // add / remove (referenced by pattern follower bindings + the project's
    // current-TS selection).  linkedPattern >= 0 marks an AUTO-SPAWNED marker
    // owned by that pattern's placement (docket B); -1 = manual.  Editing a
    // linked marker unlinks it (B2a).
    int uid           { -1 };
    int linkedPattern { -1 };
};

// QA-TempoMap (2026-07-08): stepped tempo-change flags on the Builder ruler.
// Bar-authored; beat = bar * 4 (uniform 4 beats/bar - song-level TS markers
// stay decorative for playback, same convention every playback consumer
// already uses).  Tempo applies from `bar` forward until the next change;
// the base tempo (mGlobalTempo) covers bar 1 until the first change.
struct TempoChange
{
    int    bar { 0 };
    double bpm { 120.0 };
};

// ── Clip type ─────────────────────────────────────────────────────────────────
enum class ClipType { Pattern, Audio, Automation };

// ── Arrangement block ─────────────────────────────────────────────────────────
struct ArrangementBlock
{
    // kLengthTicksUnset (-1) = "use lengthBars * 4" (bar-precision length).
    static constexpr juce::int64 kLengthTicksUnset = -1;

    int  trackRow     { 0 };   // which row this block sits on (free-form, user assigns)
    int  patternIndex { 0 };
    // 8A (QA-G3Smoke, G-5): the block START is beats-authoritative -- ONE double
    // in the musical map-position domain, read by scheduler, ruler, playhead and
    // paint alike.  The old startBar(int)+startTicks(int64) pair let the bar-
    // index and map-position domains diverge (they agreed only with no TS
    // marker); the bar index is now DERIVED for display + serialization only.
    // May be negative (slip-edit drag-left exposes pre-roll audio).  XML keeps
    // writing/reading startBar+startTicks -- full precision, no migration.
    double startBeats { 0.0 };
    int    lengthBars { 4 };
    // QA-Ee (96 PPQ): exact length in TICKS for sub-bar precision (audio clips
    // dropped by Record naturally end mid-bar).  kLengthTicksUnset (-1) = use
    // lengthBars * 4.  setLengthBeats() bridges a beats value -> ticks; a user
    // resize wanting bar precision sets lengthTicks back to kLengthTicksUnset.
    juce::int64 lengthTicks { kLengthTicksUnset };

    // Display-only derived bar index (floor; a negative slip-edit start floors
    // to its containing bar).
    int displayStartBar() const noexcept { return (int) std::floor (startBeats / 4.0); }

    void setStartBeats  (double beats) noexcept { startBeats = beats; }
    void setLengthBeats (double beats) noexcept { lengthTicks = (beats > 0.0) ? beatsToTicks (beats) : kLengthTicksUnset; }
    bool layerTrack   { true };

    // Phase 4B - clip type + audio/automation fields
    ClipType     clipType           { ClipType::Pattern };
    juce::String audioFilePath      {};                 // Audio clips only
    juce::String displayAlias       {};                 // optional per-clip display name (Browser rename)
    float        pitchSemitones     { 0.f };            // per-clip pitch shift (semitones)
    float        originalBPM        { 120.f };          // original BPM for time-stretch
    bool         stretchMode        { true };           // true=Stretch (pitch locked), false=Resample
    bool         muted              { false };          // muted clips are skipped during playback

    // Phase 4C - automation lane data (only used for ClipType::Automation)
    AutomationLane automationLane;

    // I-16 G-9 (2026-05-03): for audio clips recorded from a Vox/Inst armed
    // strip, this is the originating page's mixer channel id (e.g.,
    // MixerChannelIds::voxInsert(0)).  When non-zero AND clipType==Audio,
    // playback routes the file through that page's chain (BaySickVocals ->
    // BaySickChain -> BaySickNAMIR for Vox; BaySickPedals -> BaySickNAMIR
    // for Inst) instead of through a new Audio row's mixer strip.  Zero
    // (default) = legacy "drop on Audio bus row" behavior.
    int routeChannel { 0 };
    // QA-F (2026-07-09): true = this clip is a BaySickAlign render-to-bake
    // placed by the Align editor.  Plays back like any routed audio clip, but
    // the channel-composite renderer + clip-signature hash SKIP it -- a
    // re-analyze after Render would otherwise composite the bake together
    // with the original takes (the bake feeding its own analysis source).
    bool alignBake { false };
    // QA-E Task 7 (FILE-02): two-level clip properties.  The audio library
    // entry is the SOURCE OF TRUTH for a file's pitch / BPM / stretch-mode /
    // route (AudioLibraryEntry pitchSemitones/originalBPM/stretchMode +
    // pageOwnerChannelId).  isOverride == false (default for newly-created
    // blocks) means "this copy FOLLOWS the library original" for ALL of those
    // -- editing the original via the browser Properties dialog propagates
    // the new values into this block.  isOverride == true means the user
    // customized THIS copy via the grid clip's Properties dialog (one combined
    // flag -- whole-copy detach: changing anything detaches pitch/BPM/mode/
    // route together); the library original no longer drives it.  NOTE: on
    // deserialize the default is true (see PatternManager.cpp) so pre-Task-7
    // projects keep their exact saved values (no silent change on reopen).
    bool isOverride { false };
    // QA-Ea Task 0c (FL pre-roll record + non-destructive clip trim).
    // For ClipType::Audio: file-position offset added to every file-position
    // read in the audio-clip render loop (direct-read filePos, phase-vocoder
    // reference pvRefPos, file-EOF guard fileTotalSamples) so the clip plays
    // from sample N of the file rather than 0.  Set by StandaloneEditor::
    // commitRecordingResult to RecordResult::preRollSamples; movable
    // non-destructively via Ctrl+Alt+Home slip-edit drag on ArrangementGrid
    // (drag-left = decrement + grow lengthBeats; drag-right = increment +
    // shrink lengthBeats).  Right-end of clip + startBar stay fixed.
    // Default 0 = play from file sample 0 (every pre-Task-0c project /
    // every non-count-in recording is backwards-compatible).
    juce::int64 contentStartSamples { 0 };
    // QA-G Task 5 (slice content-offset): pattern-content phase for this
    // block, in grid ticks (384/bar).  The block plays its pattern starting
    // at this offset into the cycle; preview + playback tile from it, so a
    // sliced piece keeps its notes in place instead of restarting.  Only
    // meaningful for ClipType::Pattern (Audio continues via
    // contentStartSamples; Automation lanes are physically split at the cut).
    juce::int64 contentOffsetTicks { 0 };
};

// 2026-04-24: central helper for the "effective length in beats" of an
// arrangement block.  Returns lengthBeats when set (sub-bar precision,
// typically from a recorded audio clip); otherwise falls back to
// lengthBars * 4.  Everywhere that used to compute `lengthBars * 4` directly
// should call this instead so recorded audio clips end at their real length.
inline double effectiveLengthBeats (const ArrangementBlock& b) noexcept
{
    return (b.lengthTicks > 0) ? ticksToBeats (b.lengthTicks)
                               : (double) b.lengthBars * 4.0;
}

// 8A (QA-G3Smoke): startBeats is authoritative; these helpers survive as
// thin accessors so the ~30 read sites keep compiling unchanged.
inline double effectiveStartBeats (const ArrangementBlock& b) noexcept
{
    return b.startBeats;
}

inline double effectiveStartBars (const ArrangementBlock& b) noexcept
{
    return b.startBeats / 4.0;
}

// 2026-04-24: effective length in bars (fractional).  Use for visual width
// + any bar-based math in the arrangement grid.  effectiveLengthBeats / 4.
inline double effectiveLengthBars (const ArrangementBlock& b) noexcept
{
    return effectiveLengthBeats (b) / 4.0;
}

// ── Time signature ─────────────────────────────────────────────────────────────
struct TimeSignature
{
    int numerator   { 4 };
    int denominator { 4 };
};

// ── Mixer state ───────────────────────────────────────────────────────────────
// NOTE (5F-4a, 2026-04-15): MixerState is being migrated to lazy APVTS.
// The per-insert arrays below (drumSlot*, audioRow*) and bus-level fields stay
// for now as a snapshot cache for undo/preset I/O and backward-compat with
// existing save files. Audio thread should read APVTS after migration.
// Batch 2 introduces InsertNode in VibeGraph which reads APVTS directly.
// Batch 3 removes the audio-thread usage of these fields from PluginProcessor.
struct MixerState
{
    float masterLevel { 1.0f };
    float layersLevel { 1.0f };   // section-level (kept for compatibility)
    float bassLevel   { 1.0f };
    float drumsLevel  { 1.0f };   // section-level (kept for compatibility)
    bool  layersMute  { false };
    bool  bassMute    { false };
    bool  drumsMute   { false };
    bool  layersSolo  { false };
    bool  bassSolo    { false };
    bool  drumsSolo   { false };

    // Pan values (-1 = full left, 0 = center, +1 = full right)
    float masterPan  { 0.0f };
    float layersPan  { 0.0f };
    float bassPan    { 0.0f };
    float drumsPan   { 0.0f };

    // Per-drum-row fader levels (rows 0-9, matches drumRowToSlot mapping)
    std::array<float, MAX_DRUM_ROWS> drumSlotLevel;
    std::array<float, MAX_DRUM_ROWS> drumSlotPan;

    // Per-audio-row fader levels and mute (arrangement rows 0..99;
    // QA-Layout T11: 50 -> 100 with the Clips cap)
    static constexpr int kMaxAudioRows = 100;
    std::array<float, kMaxAudioRows> audioRowLevel;
    std::array<bool,  kMaxAudioRows> audioRowMute;

    // Audio Clips Bus - master fader/mute/pan for all audio clips combined
    float audioClipsBusLevel { 1.0f };
    float audioClipsBusPan   { 0.0f };
    bool  audioClipsBusMute  { false };
    bool  audioClipsBusSolo  { false };

    MixerState()
    {
        drumSlotLevel.fill(1.0f);
        drumSlotPan.fill(0.0f);
        audioRowLevel.fill(1.0f);
        audioRowMute.fill(false);
    }
};

// Maximum number of arrangement track rows (matches ArrangementGrid::kNumRows).
static constexpr int kMaxArrangementRows = 500;

// ── QA-G3Smoke #30b (G-6): lock-free scheduler roll snapshots ────────────────
// The audio-thread note scheduler reads roll data through an immutable snapshot
// table: one acquire per block, never a lock, never a discarded note.  The old
// try-locks (drums/clips/vox/inst) silently dropped every note-on in a block
// when the message thread held the lock; layers/bass/rusty read the live
// vectors bare (torn reads under concurrent edits).
//
// Publish protocol (message thread only): notifyContentChanged() republishes
// the CURRENT pattern's entry (every roll edit targets the current pattern --
// the roll UI binds it live); structural mutators (pattern CRUD, load, reset,
// restorePatternList) republish every entry.  Unchanged patterns SHARE their
// per-pattern snapshot across publishes (shared_ptr copy-on-write); the
// swapped-out table retires through the Batch-9c RetirementQueue so a table
// the audio thread may still be reading is never freed under it.  shared_ptr
// refcounts are only ever touched on the message thread (build) and the
// retirement drainer (destroy) -- the audio thread reads through raw pointers.
struct PatternRollsSnapshot
{
    std::array<std::vector<PianoNote>, kMaxLayerPages> layerNotes;
    std::array<std::vector<PianoNote>, kMaxBassPages>  bassNotes;
    std::array<std::vector<PianoNote>, kMaxDrumPages>  drumNotes;
    std::array<std::vector<PianoNote>, kMaxClipPages>  clipNotes;
    std::array<std::vector<PianoNote>, kMaxVoxPages>   voxNotes;
    std::array<std::vector<PianoNote>, kMaxInstPages>  instNotes;
    std::array<std::vector<PianoNote>, kMaxPluginPages> pluginNotes;   // TS6
    std::vector<PianoNote>                             rustyNotes;
    // getPatternContentBeats computed at publish time so the audio thread never
    // walks live note vectors for the content mask (#24 one-pass end).
    double contentBeats { 4.0 };
};

struct SchedulerRollSnapshot
{
    std::vector<std::shared_ptr<const PatternRollsSnapshot>> patterns;
    int           currentPatternIndex { 0 };
    std::uint64_t generation          { 0 };
};

class PatternManager
{
public:
    PatternManager();
    // #30b: frees the active roll-snapshot table (queue-retired tables are
    // handled by the RetirementQueue member's own teardown).
    ~PatternManager();

    // 2026-05-05 dirty-flag wiring: fired from every pattern-side mutation
    // (note edits, pattern CRUD, arrangement add/remove, time-marker
    // add/remove, color change, TS change).  StandaloneEditor wires it to
    // ProjectManager::markDirty.  Note-edit fires arrive via
    // notifyContentChanged(); pattern/arrangement/time-marker mutations call
    // notifyContentChanged() at the end of each mutator.
    std::function<void()> onAnyChange;
    // #30b: republish the current pattern's roll snapshot BEFORE the dirty
    // callback -- every note-edit path already funnels through here, so the
    // audio thread sees fresh rolls the moment an edit lands.
    void notifyContentChanged()
    {
        publishRollSnapshotFor (mCurrentPattern);
        if (onAnyChange) onAnyChange();
    }

    // ── QA-G3Smoke #30b: scheduler roll-snapshot API ─────────────────────
    // Message thread: rebuild one pattern's entry (shares the rest) / all
    // entries, then swap the table in.  Audio thread: acquireRollSnapshot()
    // once per block -- wait-free, stamps the in-use generation for the
    // retirement drainer.  Never returns null after construction.
    void publishRollSnapshotFor (int patternIndex);
    void publishAllRollSnapshots();
    const SchedulerRollSnapshot* acquireRollSnapshot() noexcept
    {
        auto* snap = mActiveRollSnapshot.load (std::memory_order_acquire);
        if (snap != nullptr)
            mRollRetirement.setInUseGeneration (snap->generation);
        return snap;
    }

    // ── Pattern CRUD ──────────────────────────────────────────────────────
    int           addPattern      (const juce::String& name = "");
    int           duplicatePattern (int srcIndex);   // deep-copy; returns new index
    void          removePattern   (int index);
    void          renamePattern   (int index, const juce::String& name);
    Pattern&      getPattern    (int index);
    const Pattern& getPattern   (int index) const;
    int           getNumPatterns() const { return (int)mPatterns.size(); }

    void  setCurrentPattern (int index);
    int   getCurrentPatternIndex() const { return mCurrentPattern; }
    Pattern& currentPattern()            { return mPatterns[mCurrentPattern]; }

    // ── Arrangement ───────────────────────────────────────────────────────
    void              addBlock    (ArrangementBlock block);
    void              removeBlock (int index);
    ArrangementBlock& getBlock    (int index);
    int               getNumBlocks() const { return (int)mArrangement.size(); }
    int               getTotalArrangementBars() const;

    // QA-Export: exact song end in beats -- the furthest block end across EVERY
    // block type (Pattern / Audio / Automation), using effectiveLengthBeats so a
    // recorded clip's sub-bar length drives the end rather than a ceil'd bar
    // count.  Muted blocks still COUNT: mute silences a block, it does not
    // shorten the song (a lone muted 2-bar block is a 2-bar silent song).
    //
    // Shared deliberately: song-mode transport looping and offline export must
    // stop at the SAME beat, and this was previously inline in StandaloneEditor's
    // onGetLoopBeats.  Do NOT substitute getTotalArrangementBars() -- that floors
    // to a 16-bar grid and would render trailing silence or truncate.
    double            getSongEndBeats() const;

    // ── Time markers (D-2, 2026-04-26) ───────────────────────────────────
    int                 getNumTimeMarkers() const { return (int) mTimeMarkers.size(); }
    const TimeMarker&   getTimeMarker (int idx) const { return mTimeMarkers[(size_t) idx]; }
    TimeMarker&         getTimeMarker (int idx)       { return mTimeMarkers[(size_t) idx]; }
    void                addTimeMarker (int bar, const juce::String& label);
    void                removeTimeMarker (int idx);
    void                renameTimeMarker (int idx, const juce::String& label);
    int                 findTimeMarkerNearBar (float bar, float tolerance = 0.5f) const;
    // QA-UndoCoverage Task 4: bulk restore seam for MarkerSetAction.
    void                restoreTimeMarkers (const std::vector<TimeMarker>& markers);

    // ── Time-signature changes (D-2; QA-G Task 6: the SOLE played source) ──
    int                  getNumTimeSigChanges() const { return (int) mTimeSigChanges.size(); }
    const TimeSigChange& getTimeSigChange (int idx) const { return mTimeSigChanges[(size_t) idx]; }
    TimeSigChange&       getTimeSigChange (int idx)       { return mTimeSigChanges[(size_t) idx]; }
    void                 addTimeSigChange (int bar, int num, int den, int linkedPattern = -1);
    void                 removeTimeSigChange (int idx);
    int                  findTimeSigChangeAtBar (int bar) const;   // exact-bar match, -1 otherwise
    int                  findTimeSigChangeByUid (int uid) const;   // index, -1 otherwise
    // Project-level current-TS selection: the marker newly-created patterns
    // bind to (docket #4 revision).  Auto = the sole marker while exactly one
    // exists; user-picked via prompt / transport dropdown at 2+.
    int                  getCurrentTsMarkerUid() const { return mCurrentTsMarkerUid; }
    void                 setCurrentTsMarkerUid (int uid);
    // QA-UndoCoverage Task 4: bulk restore seam for MarkerSetAction (list +
    // current-uid together -- removeTimeSigChange re-picks the uid, so the
    // pair restores atomically).  Restored uids stay valid: mNextTsUid only
    // climbs, so re-adds never collide with restored markers.
    void                 restoreTimeSigState (const std::vector<TimeSigChange>& changes,
                                              int currentUid);
    // Fired after any change that can alter effective pattern signatures
    // (marker mutations, current-selection change, lifecycle refresh).
    // StandaloneEditor refreshes rolls / dropdown / Builder from it.
    std::function<void()> onTimeSigStateChanged;

    // ── Pattern TS lifecycle (QA-G Task 6) ───────────────────────────────
    // Re-derives every FOLLOWER pattern's effective tsNum/tsDen (placed ->
    // marker at earliest block; unplaced -> binding -> current -> 4/4),
    // re-binding dead bindings to the current selection.  Cheap; called by
    // every marker / block / lifecycle mutation path.
    void refreshPatternTimeSigs();
    // Reset-to-default: clears the user-set lock, removes the pattern's
    // still-linked auto-markers, re-enters the follower lifecycle.
    void resetPatternTimeSig (int patternIndex);
    // Removes linked markers whose (pattern, bar) no longer has a block
    // starting there (block delete / undo / redo).  Spawns nothing — marker
    // creation is placement-event-driven in the Builder grid.
    void cleanupLinkedMarkers();
    bool patternHasNotes (int patternIndex) const;
    // Publish the marker timeline to the audio-side TsMap seqlock.
    void publishTimeSigMap() const;
    // QA-G (Split by Player Engine): raw pattern-list restore for the split's
    // undo snapshot.  No per-pattern side effects -- just swap the list,
    // clamp the current index, refresh follower caches, notify.
    void restorePatternList (const std::vector<Pattern>& patterns, int currentIndex);

    // ── Tempo changes (QA-TempoMap, 2026-07-08) ──────────────────────────
    int                getNumTempoChanges() const { return (int) mTempoChanges.size(); }
    const TempoChange& getTempoChange (int idx) const { return mTempoChanges[(size_t) idx]; }
    void               addTempoChange (int bar, double bpm);       // replaces an existing same-bar change
    void               removeTempoChange (int idx);
    int                findTempoChangeNearBar (float bar, float tolerance = 0.5f) const;
    // QA-UndoCoverage Task 4: bulk restore seam for MarkerSetAction.
    void               restoreTempoChanges (const std::vector<TempoChange>& changes);

    // C.5 (2026-04-30): time-signature-aware beat/bar conversion.
    // The DAW beat is one quarter note (PPQ).  For a time signature N/D, one
    // bar contains N * (4/D) PPQ beats - so 4/4 = 4 beats, 3/4 = 3, 6/8 = 3,
    // 7/8 = 3.5, etc.  When mTimeSigChanges is empty, defaults to 4/4.
    // Audio thread safe (read-only, no allocations).
    double getBeatsPerBarAtBar  (int bar) const;
    double getBeatsPerBarAtBeat (double beat) const;
    // Returns the (num,den) effective at the given bar.  Defaults to {4,4}.
    TimeSigChange getEffectiveTimeSigAtBar (int bar) const;
    // Convert PPQ beat position to (bar, beatInBar) honoring TS changes.
    // beatInBar is in PPQ beats from the bar's start (so a 6/8 bar runs 0..3).
    void   beatToBarAndBeatInBar (double beat, int& outBar, double& outBeatInBar) const;
    // Convert bar count to PPQ beat using TS changes (start of bar).
    double barStartBeat (int bar) const;

    // Per-pattern beats-per-bar (Pattern.tsNum/tsDen).  PPQ-beat (quarter note)
    // basis: 4/4 = 4, 3/4 = 3, 6/8 = 3, 5/4 = 5, 7/8 = 3.5.  Defaults 4/4.
    double getPatternBeatsPerBar (int patternIndex) const;
    // User TS setter (type-in popup / Lock Previous TS).  Locks the pattern
    // (user-set) + updates its still-linked markers' signatures.  Spawns no
    // markers — spawning is placement-event-driven.
    void   setPatternTimeSig (int patternIndex, int num, int den);

    // ── Audio file library (persists independently of blocks) ────────────
    // Items here survive deletion of all blocks that reference them so the
    // Browser's Audio tab doesn't empty when the user clears the arrangement.
    //
    // QA-E Task 4 (2026-05-12): pageOwnerChannelId tags each entry to a Vox /
    // Inst / Clips page so the browser groups by ownerChannelId range instead
    // of round-tripping through per-page mClipPath fields.  Default 0 = generic
    // Audio category (master capture, untagged drops).  See §9 17th Forks entry.
    // D3 (2026-04-25): chokeGroup is 0 = none, 1..16 = group id.  QA-E Task 4:
    // pageOwnerChannelId tags the entry to its owning page.  QA-E Task 7
    // (FILE-02): the entry is the SOURCE OF TRUTH for a file's clip
    // properties (pitch/BPM/stretch); followers inherit.  QA-Fe2: groupName
    // is manual browser-group membership ("" = ungrouped).
    // Public since QA-UndoCoverage Task 4 -- AudioLibraryAction snapshots the
    // whole list by value.
    struct AudioLibraryEntry {
        juce::String path;
        juce::String alias;
        int chokeGroup         { 0 };
        int pageOwnerChannelId { 0 };
        float pitchSemitones   { 0.f };
        float originalBPM      { 120.f };
        bool  stretchMode      { true };
        juce::String groupName;
    };

    void                 addAudioToLibrary     (const juce::String& path,
                                                const juce::String& alias = {},
                                                int                 pageOwnerChannelId = 0);
    void                 removeAudioFromLibrary(const juce::String& path);

    // QA-E Task 5 (2026-05-15): library lookup helpers for drag dedupe +
    // last-file-out delete cascade.
    // findAudioLibraryIndexByPath: exact string match on stored path.
    //   Returns -1 if not found.
    // countAudioLibraryEntriesForChannel: how many entries the given page
    //   channel owns (used to detect "deleting the LAST file -> cascade
    //   page close").
    int                  findAudioLibraryIndexByPath        (const juce::String& path) const;
    int                  countAudioLibraryEntriesForChannel (int channelId) const;
    // QA-E Task 5: remove a specific entry by index (precise: multiple
    // entries may share a path post-schema-change).  removeAudioFromLibrary
    // (by path) still removes the FIRST matching entry for legacy callers.
    void                 removeAudioFromLibraryAt           (int idx);

    int                  getNumAudioLibrary    () const { return (int)mAudioLibrary.size(); }
    const juce::String&  getAudioLibraryPath   (int idx) const { return mAudioLibrary[idx].path; }
    const juce::String&  getAudioLibraryAlias  (int idx) const { return mAudioLibrary[idx].alias; }
    int                  getAudioLibraryChokeGroup (int idx) const
        { return (idx >= 0 && idx < (int) mAudioLibrary.size()) ? mAudioLibrary[idx].chokeGroup : 0; }
    int                  getAudioLibraryPageOwner (int idx) const
        { return (idx >= 0 && idx < (int) mAudioLibrary.size()) ? mAudioLibrary[idx].pageOwnerChannelId : 0; }
    // QA-E Task 7 (FILE-02): source-of-truth clip props (browser Properties
    // edits these; followers inherit).  Safe defaults if idx out of range.
    float                getAudioLibraryPitch      (int idx) const
        { return (idx >= 0 && idx < (int) mAudioLibrary.size()) ? mAudioLibrary[idx].pitchSemitones : 0.f; }
    float                getAudioLibraryBPM        (int idx) const
        { return (idx >= 0 && idx < (int) mAudioLibrary.size()) ? mAudioLibrary[idx].originalBPM : 120.f; }
    bool                 getAudioLibraryStretchMode (int idx) const
        { return (idx >= 0 && idx < (int) mAudioLibrary.size()) ? mAudioLibrary[idx].stretchMode : true; }
    void                 setAudioLibraryAlias  (int idx, const juce::String& alias);
    void                 setAudioLibraryChokeGroup (int idx, int group);
    void                 setAudioLibraryPageOwner (int idx, int channelId);
    // QA-E Task 7 (FILE-02): set the source-of-truth pitch / BPM / stretch
    // (route source-of-truth stays setAudioLibraryPageOwner).
    void                 setAudioLibraryClipDefaults (int idx, float pitch,
                                                      float bpm, bool stretchMode);

    // ── QA-Fe2 browser groups ────────────────────────────────────────────
    juce::String         getAudioLibraryGroup (int idx) const
        { return (idx >= 0 && idx < (int) mAudioLibrary.size()) ? mAudioLibrary[idx].groupName : juce::String(); }
    void                 setAudioLibraryGroup (int idx, const juce::String& g)
        { if (idx >= 0 && idx < (int) mAudioLibrary.size()) mAudioLibrary[idx].groupName = g; }
    void                 addManualAudioGroup    (int category, const juce::String& name);
    void                 renameManualAudioGroup (int category, const juce::String& oldName,
                                                 const juce::String& newName);
    juce::StringArray    getManualAudioGroups   (int category) const;
    // Rewrites a stored audio path EVERYWHERE it is referenced: library
    // entries + every ArrangementBlock.audioFilePath in every pattern.
    // Returns the number of references rewritten.  (Recording-group rename.)
    int                  replaceAudioPath (const juce::String& oldPath,
                                           const juce::String& newPath);

    // QA-UndoCoverage Task 4: bulk snapshot/restore seams for AudioLibraryAction
    // (mirrors the restorePatternList precedent).  Restore swaps both lists and
    // notifies; callers refresh their own views.
    const std::vector<AudioLibraryEntry>&           getAudioLibraryEntries() const { return mAudioLibrary; }
    const std::vector<std::pair<int, juce::String>>& getManualAudioGroupsRaw() const { return mManualAudioGroups; }
    void restoreAudioLibrary (const std::vector<AudioLibraryEntry>& entries,
                              const std::vector<std::pair<int, juce::String>>& manualGroups);

    // ── Automation template library (persists independently of blocks) ───
    void                    addAutomationTemplate   (const AutomationLane& lane);
    void                    removeAutomationTemplate(int idx);
    int                     getNumAutomationTemplates() const { return (int)mAutomationTemplates.size(); }
    const AutomationLane&   getAutomationTemplate   (int idx) const { return mAutomationTemplates[idx]; }
    void                    renameAutomationTemplate(int idx, const juce::String& newParamId);

    // Set the template's user-facing display name (empty = revert to auto).
    // Does NOT touch paramId (keeps backend applicator bindings stable).
    void                    setAutomationTemplateUserName(int idx, const juce::String& userName);

    // QA-UndoCoverage Task 4: bulk snapshot/restore seam for
    // AutomationTemplateAction (restorePatternList precedent).
    const std::vector<AutomationLane>& getAutomationTemplatesRaw() const { return mAutomationTemplates; }
    void restoreAutomationTemplates (const std::vector<AutomationLane>& templates);

    // ── Drum sounds ───────────────────────────────────────────────────────
    static const char* kDrumNames[MAX_DRUM_SOUNDS];
    void enableDrum   (int slot, bool enabled);
    bool isDrumEnabled(int slot) const { return mDrumEnabled[slot]; }
    int  getNumEnabledDrums() const;

    // ── Mixer ─────────────────────────────────────────────────────────────
    MixerState& getMixer() { return mMixer; }

    // ── Per-track mute / solo (arrangement playback gate) ─────────────────
    // Independent from the mixer-strip mute. Applies to every ClipType on the
    // given track row (Pattern, Audio, Automation). Audio-thread safe: reads
    // use relaxed atomic loads, no allocations.
    void setRowMuted  (int row, bool m);
    void setRowSoloed (int row, bool s);
    bool isRowMuted   (int row) const;
    bool isRowSoloed  (int row) const;
    bool isRowAudible (int row) const;
    bool anyRowSoloed () const { return mAnyRowSoloed.load(std::memory_order_relaxed); }

    // ── Per-track row names + visual groups (QA-G) ────────────────────────
    // Builder-row PROJECT data serialized in the RowState node; the
    // ArrangementGrid reads/writes through these instead of owning a parallel
    // copy.  Groups are visual-only in V1 (header band + tint); group id 0 =
    // ungrouped, ids 1+ come from allocateRowGroupId().  UI-thread only (no
    // audio-thread reads), hence non-atomic unlike mute/solo above.
    const std::array<juce::String, kMaxArrangementRows>& getRowNames() const { return mRowNames; }
    void setRowName (int row, const juce::String& name);
    juce::String defaultRowName (int row) const { return "Track " + juce::String (row + 1); }
    int          getRowGroup      (int row) const;
    void         setRowGroup      (int row, int groupId);      // 0 = ungrouped
    juce::uint32 getRowGroupColor (int row) const;
    void         setRowGroupColor (int row, juce::uint32 argb);
    int          allocateRowGroupId () const;                  // max used + 1 (>= 1)

    // ── Effective playback loop length ────────────────────────────────────
    // Returns the loop length in beats for the current pattern, following this priority:
    //   1. If the pattern has blocks on the builder that extend beyond 1 bar,
    //      the loop = the longest block length placed for this pattern (in beats).
    //   2. If notes exist beyond bar 1, the loop = end of the last bar containing a note.
    //   3. Default = 1 bar = 4 beats.
    double getEffectivePatternLoopBeats() const;
    // B-1: content length in beats for ANY pattern (not just the current one) -
    // furthest note/step end, bar-ceiled at the pattern's bpb, min 1 bar.  Drives
    // Builder tiling + song-mode tiling so blocks loop at their real content length.
    double getPatternContentBeats (int patternIndex) const;

    // ── Check if complex sequence is active on any page ───────────────────
    // Used by the UI to grey out / enable the Sequencer tab.
    bool isComplexSequenceActive() const;

    // ── Serialisation ─────────────────────────────────────────────────────
    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& vt);

    // File > New reset (2026-04-24): wipe patterns, arrangement, mixer, row
    // state, drum-enabled flags, automation templates.  Leaves one empty
    // default pattern so the app has somewhere to put new notes.
    void reset();

    // P4-follow-up (2026-04-24): global project tempo.  FL-style - tempo is
    // a single project-level value, NOT per-pattern.  Patterns play at
    // whatever this is set to; tempo automation clips target this via the
    // "global_tempo" paramId.  Per-pattern Pattern::tempo field is gone.
    double getGlobalTempo() const        { return mGlobalTempo; }
    void   setGlobalTempo (double bpm)   { mGlobalTempo = juce::jlimit (20.0, 300.0, bpm); }

    // ── Audio clip library registration (persistent browser entries) ──────
    // Forward declaration - the real addAudioToLibrary lives below.  Exposed
    // here for clarity: commitRecordingResult registers recorded WAVs so they
    // show up in the Builder's Audio tab on reload.

private:
    // ── QA-G3Smoke #30b: roll-snapshot internals (message thread builds,
    // audio thread acquires, drainer destroys) ───────────────────────────
    std::shared_ptr<const PatternRollsSnapshot> buildPatternRollsSnapshot (int patternIndex) const;
    void republishRollTable();   // swap a new table in from mPatternSnapCache + retire the old
    std::vector<std::shared_ptr<const PatternRollsSnapshot>> mPatternSnapCache;
    std::atomic<SchedulerRollSnapshot*>    mActiveRollSnapshot { nullptr };
    RetirementQueue<SchedulerRollSnapshot> mRollRetirement;
    std::uint64_t                          mRollSnapGeneration { 0 };

    std::vector<Pattern>          mPatterns;
    std::vector<ArrangementBlock> mArrangement;
    // D-2 (2026-04-26): time-markers + time-signature-changes - project scope.
    std::vector<TimeMarker>       mTimeMarkers;
    std::vector<TimeSigChange>    mTimeSigChanges;
    // QA-G Task 6: current-TS selection (uid, -1 = none) + monotonic uid source.
    int mCurrentTsMarkerUid { -1 };
    int mNextTsUid          { 1 };
    // Shared tail of every TS-affecting mutation: refresh follower caches,
    // republish the audio-side map, dirty the project, notify the UI.
    void tsStateChanged();
    // QA-TempoMap (2026-07-08): ruler tempo flags - project scope.
    std::vector<TempoChange>      mTempoChanges;
    int                           mCurrentPattern { 0 };
    double                        mGlobalTempo    { 120.0 };   // 2026-04-24
    std::array<bool, MAX_DRUM_SOUNDS> mDrumEnabled;
    MixerState                    mMixer;

    // Per-track-row mute/solo state (arrangement gate).
    std::array<std::atomic<bool>, kMaxArrangementRows> mRowMuted {};
    std::array<std::atomic<bool>, kMaxArrangementRows> mRowSoloed {};
    std::atomic<bool>                                  mAnyRowSoloed { false };

    // Builder row names + visual group state (QA-G).  UI-thread only.
    std::array<juce::String, kMaxArrangementRows> mRowNames;
    std::array<int,          kMaxArrangementRows> mRowGroupId    {};  // 0 = ungrouped
    std::array<juce::uint32, kMaxArrangementRows> mRowGroupColor {};

    // ── Browser library storage (persists across block delete) ──────────
    // Entry type is public (QA-UndoCoverage Task 4); storage stays here.
    std::vector<AudioLibraryEntry> mAudioLibrary;
    std::vector<AutomationLane>    mAutomationTemplates;

    // QA-Fe2: user-created (possibly empty) browser groups, per category
    // (0 = Clips, 1 = Vox, 2 = Inst).  Auto recording-groups are NOT stored
    // here -- they are derived from take-tag file names at tree build.
    std::vector<std::pair<int, juce::String>> mManualAudioGroups;
};
