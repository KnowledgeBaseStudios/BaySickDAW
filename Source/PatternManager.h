#pragma once
#include <JuceHeader.h>
#include "VibesynthConstants.h"

// ── Automation data ───────────────────────────────────────────────────────────
enum class CurveType { Linear, Stepped, Spline };

struct ControlPoint
{
    float     timeTicks  { 0.f };   // 0..1, fraction of clip length
    float     value01    { 0.5f };  // 0..1, normalized parameter value
    CurveType curveType  { CurveType::Linear };
    float     tension    { 0.f };   // reserved for future Bezier
};

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
    // The auto-generated display string is NOT stored here; it's resolved at
    // display time via StandaloneEditor::resolveAutomationDisplayName(paramId)
    // so effect-swap inside a slot reflects immediately without migration.
    juce::String              userDisplayName;

    // Evaluate the lane at normalised clip position pos01 (0 = start, 1 = end).
    // Returns a normalised 0..1 parameter value.
    float evaluateAt(float pos01) const;
};

// ── Piano roll note type ──────────────────────────────────────────────────────
enum class NoteType { Standard, Slide, Portamento };

// ── Piano roll note ───────────────────────────────────────────────────────────
struct PianoNote
{
    int      midiNote      { 60 };                   // 0–127
    double   startBeat     { 0.0 };                  // from pattern start, in beats
    double   durationBeats { 0.25 };                 // note length in beats
    float    velocity      { 0.8f };                 // 0–1
    float    panning       { 0.0f };                 // -1 (left) to +1 (right)
    float    finePitch     { 0.0f };                 // cents ±100
    NoteType type          { NoteType::Standard };   // Standard / Slide / Portamento
    bool     muted         { false };                // muted notes play silently
    int      groupId       { -1 };                   // -1 = ungrouped; same id = grouped
    float    filterCutoff  { 0.5f };                 // 0=closed, 1=fully open (control lane)
    // §P4.2 Phase C1: drum slot index (0..15) for drumRoll notes.
    //   -1 = not a drum note (used on layerRoll / bassRoll).
    //   For drum-grid-mode notes created pre-C2 this is redundant with
    //   (51 - midiNote); C2 full-roll mode will use slotIndex independently
    //   from midiNote so a slot can play any pitch.
    int      slotIndex     { -1 };
};

struct PianoRollData
{
    std::vector<PianoNote> notes;
    int  numBars       { 2 };
    int  snapDenominator { 32 };   // snap quantization: 1/snapDenominator note
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
    // C.5b (2026-04-30): per-pattern intrinsic time signature (FL-style).
    // Drives the piano roll's bar width + beat sub-divisions for THIS pattern.
    // Default 4/4.  Auto-assigned on first Builder placement (looks up song
    // TS at the placement bar).  Right-click pattern menu lets users override
    // and locks tsLocked = true so subsequent placements don't re-derive.
    int          tsNum       { 4 };
    int          tsDen       { 4 };
    bool         tsLocked    { false };   // true once user-set or first-placed
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

    // Per-page piano roll note data — one slot per Layers page (max 8), one per Bass page (max 4)
    std::array<PianoRollData, 8>            layerRoll;
    std::array<PianoRollData, kMaxBassPages> bassRoll;

    // Legacy single drum roll (pre-D1, kit-shared with slotIndex tagging).
    // Kept for backward compat / migration.  Will be removed in D1.4 cutover.
    PianoRollData drumRoll;

    // D1.1 (2026-04-24): per-drum piano-roll data.  Each drum tab owns its own
    // roll, indexed by drum-tab pageIndex (0..kMaxDrumPages-1).  Notes are
    // standard piano-roll notes — midiNote is the played pitch, no slotIndex
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
    // clipRoll — pageIndex is the corresponding mixer Vox/Inst insert index
    // (0..kMaxVoxPages-1 / 0..kMaxInstPages-1).
    std::array<PianoRollData, kMaxVoxPages>  voxRoll;
    std::array<PianoRollData, kMaxInstPages> instRoll;

    Pattern()
    {
        for (auto& arr : drumGrid) arr.fill(false);
        drumRowToSlot.fill(-1);
    }
};

// ── Time markers + per-bar time-signature changes (D-2, 2026-04-26) ─────────
// Project-scope (not per-pattern) — both lists belong to the Builder ruler.
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
    int den { 4 };   // note value (denominator — power of 2)
};

// ── Clip type ─────────────────────────────────────────────────────────────────
enum class ClipType { Pattern, Audio, Automation };

// ── Arrangement block ─────────────────────────────────────────────────────────
struct ArrangementBlock
{
    int  trackRow     { 0 };   // which row this block sits on (free-form, user assigns)
    int  patternIndex { 0 };
    int  startBar     { 0 };
    int  lengthBars   { 4 };
    // 2026-04-24: exact length in beats for sub-bar precision (audio clips
    // dropped by Record naturally end mid-bar).  -1 = use lengthBars * 4.
    // User resize in the arrangement snaps to bars + sets this back to -1.
    float lengthBeats { -1.f };
    bool layerTrack   { true };

    // Phase 4B — clip type + audio/automation fields
    ClipType     clipType           { ClipType::Pattern };
    juce::String audioFilePath      {};                 // Audio clips only
    juce::String displayAlias       {};                 // optional per-clip display name (Browser rename)
    float        pitchSemitones     { 0.f };            // per-clip pitch shift (semitones)
    float        originalBPM        { 120.f };          // original BPM for time-stretch
    bool         stretchMode        { true };           // true=Stretch (pitch locked), false=Resample
    bool         muted              { false };          // muted clips are skipped during playback

    // Phase 4C — automation lane data (only used for ClipType::Automation)
    AutomationLane automationLane;
};

// 2026-04-24: central helper for the "effective length in beats" of an
// arrangement block.  Returns lengthBeats when set (sub-bar precision,
// typically from a recorded audio clip); otherwise falls back to
// lengthBars * 4.  Everywhere that used to compute `lengthBars * 4` directly
// should call this instead so recorded audio clips end at their real length.
inline double effectiveLengthBeats (const ArrangementBlock& b) noexcept
{
    return (b.lengthBeats > 0.f) ? (double) b.lengthBeats
                                 : (double) b.lengthBars * 4.0;
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

    // Per-audio-row fader levels and mute (arrangement rows 0..49)
    static constexpr int kMaxAudioRows = 50;
    std::array<float, kMaxAudioRows> audioRowLevel;
    std::array<bool,  kMaxAudioRows> audioRowMute;

    // Audio Clips Bus — master fader/mute/pan for all audio clips combined
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
static constexpr int kMaxArrangementRows = 50;

class PatternManager
{
public:
    PatternManager();

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

    // ── Time markers (D-2, 2026-04-26) ───────────────────────────────────
    int                 getNumTimeMarkers() const { return (int) mTimeMarkers.size(); }
    const TimeMarker&   getTimeMarker (int idx) const { return mTimeMarkers[(size_t) idx]; }
    TimeMarker&         getTimeMarker (int idx)       { return mTimeMarkers[(size_t) idx]; }
    void                addTimeMarker (int bar, const juce::String& label);
    void                removeTimeMarker (int idx);
    void                renameTimeMarker (int idx, const juce::String& label);
    int                 findTimeMarkerNearBar (float bar, float tolerance = 0.5f) const;

    // ── Time-signature changes (D-2) ─────────────────────────────────────
    int                  getNumTimeSigChanges() const { return (int) mTimeSigChanges.size(); }
    const TimeSigChange& getTimeSigChange (int idx) const { return mTimeSigChanges[(size_t) idx]; }
    TimeSigChange&       getTimeSigChange (int idx)       { return mTimeSigChanges[(size_t) idx]; }
    void                 addTimeSigChange (int bar, int num, int den);
    void                 removeTimeSigChange (int idx);
    int                  findTimeSigChangeAtBar (int bar) const;   // exact-bar match, -1 otherwise

    // C.5 (2026-04-30): time-signature-aware beat/bar conversion.
    // The DAW beat is one quarter note (PPQ).  For a time signature N/D, one
    // bar contains N * (4/D) PPQ beats — so 4/4 = 4 beats, 3/4 = 3, 6/8 = 3,
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
    // Auto-derive a pattern's intrinsic TS from the song-level TS at the
    // given placement bar.  No-op if pattern.tsLocked is already true.
    // Sets pattern.tsLocked = true after derive.  Returns true if changed.
    bool   autoDerivePatternTimeSig (int patternIndex, int placementBar);
    // Manual TS setter (right-click override).  Always locks.
    void   setPatternTimeSig (int patternIndex, int num, int den);

    // ── Audio file library (persists independently of blocks) ────────────
    // Items here survive deletion of all blocks that reference them so the
    // Browser's Audio tab doesn't empty when the user clears the arrangement.
    void                 addAudioToLibrary     (const juce::String& path,
                                                const juce::String& alias = {});
    void                 removeAudioFromLibrary(const juce::String& path);
    int                  getNumAudioLibrary    () const { return (int)mAudioLibrary.size(); }
    const juce::String&  getAudioLibraryPath   (int idx) const { return mAudioLibrary[idx].path; }
    const juce::String&  getAudioLibraryAlias  (int idx) const { return mAudioLibrary[idx].alias; }
    int                  getAudioLibraryChokeGroup (int idx) const
        { return (idx >= 0 && idx < (int) mAudioLibrary.size()) ? mAudioLibrary[idx].chokeGroup : 0; }
    void                 setAudioLibraryAlias  (int idx, const juce::String& alias);
    void                 setAudioLibraryChokeGroup (int idx, int group);

    // ── Automation template library (persists independently of blocks) ───
    void                    addAutomationTemplate   (const AutomationLane& lane);
    void                    removeAutomationTemplate(int idx);
    int                     getNumAutomationTemplates() const { return (int)mAutomationTemplates.size(); }
    const AutomationLane&   getAutomationTemplate   (int idx) const { return mAutomationTemplates[idx]; }
    void                    renameAutomationTemplate(int idx, const juce::String& newParamId);

    // Set the template's user-facing display name (empty = revert to auto).
    // Does NOT touch paramId (keeps backend applicator bindings stable).
    void                    setAutomationTemplateUserName(int idx, const juce::String& userName);

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

    // ── Effective playback loop length ────────────────────────────────────
    // Returns the loop length in beats for the current pattern, following this priority:
    //   1. If the pattern has blocks on the builder that extend beyond 1 bar,
    //      the loop = the longest block length placed for this pattern (in beats).
    //   2. If notes exist beyond bar 1, the loop = end of the last bar containing a note.
    //   3. Default = 1 bar = 4 beats.
    double getEffectivePatternLoopBeats() const;

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
    std::vector<Pattern>          mPatterns;
    std::vector<ArrangementBlock> mArrangement;
    // D-2 (2026-04-26): time-markers + time-signature-changes — project scope.
    std::vector<TimeMarker>       mTimeMarkers;
    std::vector<TimeSigChange>    mTimeSigChanges;
    int                           mCurrentPattern { 0 };
    double                        mGlobalTempo    { 120.0 };   // 2026-04-24
    std::array<bool, MAX_DRUM_SOUNDS> mDrumEnabled;
    MixerState                    mMixer;

    // Per-track-row mute/solo state (arrangement gate).
    std::array<std::atomic<bool>, kMaxArrangementRows> mRowMuted {};
    std::array<std::atomic<bool>, kMaxArrangementRows> mRowSoloed {};
    std::atomic<bool>                                  mAnyRowSoloed { false };

    // ── Browser library storage (persists across block delete) ──────────
    // D3 (2026-04-25): chokeGroup is 0 = none, 1..16 = group id.  When this
    // clip starts playback, it chokes any other insert (synth or audio) on the
    // same group.  Set via the browser's right-click "Choke Group" submenu.
    struct AudioLibraryEntry { juce::String path; juce::String alias; int chokeGroup { 0 }; };
    std::vector<AudioLibraryEntry> mAudioLibrary;
    std::vector<AutomationLane>    mAutomationTemplates;
};
