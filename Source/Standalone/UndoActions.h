#pragma once
#include <JuceHeader.h>
#include <array>
#include <deque>
#include <functional>
#include <vector>
#include "../PatternManager.h"
#include "../EffectRack.h"
#include "../DSP/BaySickPitchDSP.h"   // QA-Fd: PitchNoteRegion for PitchEditAction

// ── UndoContext ────────────────────────────────────────────────────────────────
// Lightweight token passed to any component that needs to perform undoable
// actions. StandaloneEditor owns one instance; all pages hold a copy by value.
// 'manager' gives canUndo/canRedo; 'perform' routes through the central
// history-label tracker and then calls UndoManager::perform().
// ─────────────────────────────────────────────────────────────────────────────
struct UndoContext
{
    juce::UndoManager* manager { nullptr };

    // Perform an undoable action. Takes ownership. Also updates the global
    // history-label list maintained by StandaloneEditor.
    std::function<bool(juce::UndoableAction*, const juce::String& label)> perform;

    // Undo / redo - ALWAYS use these instead of calling manager->undo/redo() directly.
    // These route through StandaloneEditor::globalUndo/Redo() so mHistoryCursor stays
    // in sync and the history window refreshes correctly.
    std::function<void()> undo;
    std::function<void()> redo;

    // Open the global undo history window (optional - wire where available).
    std::function<void()> showHistory;

    // Jeff ruling 2026-08-06 (fix now): page-lifetime undo entries must
    // survive delete + resurrect cycles.  Resurrection creates a NEW page
    // object, so a SafePointer captured at gesture time goes null and the
    // entry silently skips.  Apply lambdas resolve the LIVE page through
    // this instead: it returns the CURRENT page owning this context's
    // identity (the editor derives kind + pageIndex from the ctx owner key),
    // or nullptr while the tab is genuinely absent -- the history ORDER
    // re-creates it before its rows replay.  Wired by
    // StandaloneEditor::makeUndoContext; null on non-page contexts.
    std::function<juce::Component*()> resolveOwnerPage;

    bool isValid() const { return manager != nullptr && (bool)perform; }
};

// ── PianoRollEditAction ───────────────────────────────────────────────────────
// Full before/after snapshot of piano roll notes for one edit operation.
// ─────────────────────────────────────────────────────────────────────────────
class PianoRollEditAction : public juce::UndoableAction
{
public:
    using NoteVec = std::vector<PianoNote>;
    using ApplyFn = std::function<void(const NoteVec&)>;

    PianoRollEditAction(juce::String label,
                        NoteVec before, NoteVec after,
                        ApplyFn applyFn)
        : mLabel(std::move(label))
        , mBefore(std::move(before))
        , mAfter(std::move(after))
        , mApply(std::move(applyFn))
    {}

    // The change is already applied before commitEdit() calls UndoManager::perform(),
    // so skip the first perform() call (redo still works on subsequent calls).
    bool perform() override
    {
        if (mFirstPerform) { mFirstPerform = false; return true; }
        mApply(mAfter);
        return true;
    }
    bool undo() override { mApply(mBefore); return true; }

    int getSizeInUnits() override
    {
        return (int)((mBefore.size() + mAfter.size()) * sizeof(PianoNote));
    }

private:
    juce::String mLabel;
    NoteVec      mBefore, mAfter;
    ApplyFn      mApply;
    bool         mFirstPerform { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollEditAction)
};

// ── PitchEditAction (QA-Fd 9a: pitch editor joins the global undo) ───────────
// Full before/after snapshot of a channel's pitch note regions for one edit
// gesture.  ApplyFn routes through a Component::SafePointer at the call site
// so an action outliving its editor (tab closed) degrades to a safe no-op.
// ─────────────────────────────────────────────────────────────────────────────
class PitchEditAction : public juce::UndoableAction
{
public:
    using RegionVec = std::vector<PitchNoteRegion>;
    using ApplyFn   = std::function<void(const RegionVec&)>;

    PitchEditAction(juce::String label,
                    RegionVec before, RegionVec after,
                    ApplyFn applyFn)
        : mLabel(std::move(label))
        , mBefore(std::move(before))
        , mAfter(std::move(after))
        , mApply(std::move(applyFn))
    {}

    // The edit is already applied before commitEdit() calls perform() (the
    // PianoRollEditAction convention) -- skip the first perform.
    bool perform() override
    {
        if (mFirstPerform) { mFirstPerform = false; return true; }
        mApply(mAfter);
        return true;
    }
    bool undo() override { mApply(mBefore); return true; }

    int getSizeInUnits() override
    {
        return (int)((mBefore.size() + mAfter.size()) * sizeof(PitchNoteRegion));
    }

private:
    juce::String mLabel;
    RegionVec    mBefore, mAfter;
    ApplyFn      mApply;
    bool         mFirstPerform { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchEditAction)
};

// ── ArrangementEditAction ─────────────────────────────────────────────────────
// Full before/after snapshot of arrangement blocks + row names.
// ─────────────────────────────────────────────────────────────────────────────
class ArrangementEditAction : public juce::UndoableAction
{
public:
    struct Snapshot
    {
        std::vector<ArrangementBlock> blocks;
        std::vector<juce::String>     rowNames;  // kNumRows entries
        // QA-G: Move/Insert-track edits also mutate per-row state; capture it so
        // undo restores mute/solo/group alongside the blocks (parity with SplitState).
        std::vector<int>              rowGroups;
        std::vector<juce::uint32>     rowColors;
        std::vector<char>             rowMuted;
        std::vector<char>             rowSoloed;
    };
    using ApplyFn = std::function<void(const Snapshot&)>;

    ArrangementEditAction(juce::String label,
                          Snapshot before, Snapshot after,
                          ApplyFn applyFn)
        : mLabel(std::move(label))
        , mBefore(std::move(before))
        , mAfter(std::move(after))
        , mApply(std::move(applyFn))
    {}

    bool perform() override { mApply(mAfter);  return true; }
    bool undo()    override { mApply(mBefore); return true; }

private:
    juce::String mLabel;
    Snapshot     mBefore, mAfter;
    ApplyFn      mApply;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArrangementEditAction)
};

// ── MixerStateAction ──────────────────────────────────────────────────────────
// Full before/after snapshot of MixerState for one fader/mute/solo change.
// ─────────────────────────────────────────────────────────────────────────────
class MixerStateAction : public juce::UndoableAction
{
public:
    using ApplyFn = std::function<void(const MixerState&)>;

    MixerStateAction(juce::String label,
                     MixerState before, MixerState after,
                     ApplyFn applyFn)
        : mLabel(std::move(label))
        , mBefore(std::move(before))
        , mAfter(std::move(after))
        , mApply(std::move(applyFn))
    {}

    bool perform() override { mApply(mAfter);  return true; }
    bool undo()    override { mApply(mBefore); return true; }

private:
    juce::String mLabel;
    MixerState   mBefore, mAfter;
    ApplyFn      mApply;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerStateAction)
};

// ── FloatParamAction ─────────────────────────────────────────────────────────
// Generic single-parameter undo for any float value (knob, pan, etc.).
// The apply function is responsible for updating both the UI control and the DSP.
// ─────────────────────────────────────────────────────────────────────────────
class FloatParamAction : public juce::UndoableAction
{
public:
    using ApplyFn = std::function<void(float)>;

    FloatParamAction(juce::String label, float before, float after, ApplyFn applyFn)
        : mLabel(std::move(label))
        , mBefore(before)
        , mAfter(after)
        , mApply(std::move(applyFn))
    {}

    bool perform() override { mApply(mAfter);  return true; }
    bool undo()    override { mApply(mBefore); return true; }

private:
    juce::String mLabel;
    float        mBefore, mAfter;
    ApplyFn      mApply;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FloatParamAction)
};

// ── EffectRackAction ──────────────────────────────────────────────────────────
// Before/after snapshot of the 6 rack slot types for load/remove/swap.
// D.2 (2026-05-01): snapshot expanded from type-only to a full SlotSnapshot
// (type + bypassed + outputGainDb + DSP state blob + UUID + sidechain pick +
// Basic/Advanced mode) so undo/redo of Move/Load/Remove preserves the full
// slot configuration including knob values and the slot's UUID (which keeps
// automation lanes pointed at the right paramId after an undo).
// ─────────────────────────────────────────────────────────────────────────────
class EffectRackAction : public juce::UndoableAction
{
public:
    struct SlotSnapshot
    {
        EffectType        type         { EffectType::None };
        bool              bypassed     { false };
        float             outputGainDb { 0.0f };
        juce::MemoryBlock dspState;
        juce::String      uuid;
        // Slot-level config the DSP blob cannot carry: EffectRack pushes scPick
        // onto the effect on every audio block, so a restored dspState always
        // loses to the Slot's own value, and basicMode is UI-only state the DSP
        // never sees.  Both are snapshotted for EVERY slot including empty ones,
        // so an undo cannot leave a stale pick behind for the next load.
        int               scPick       { -1 };
        bool              basicMode    { true };
    };
    using SlotSnapshots = std::array<SlotSnapshot, EffectRack::kNumSlots>;
    using ApplyFn       = std::function<void(const SlotSnapshots&)>;

    // Legacy alias kept for now - type-only snapshot.  D.2 extends call sites
    // to use SlotSnapshots directly.
    using SlotTypes = std::array<EffectType, EffectRack::kNumSlots>;

    EffectRackAction(juce::String label,
                     SlotSnapshots before, SlotSnapshots after,
                     ApplyFn applyFn)
        : mLabel(std::move(label))
        , mBefore(std::move(before))
        , mAfter(std::move(after))
        , mApply(std::move(applyFn))
    {}

    // The slot change was already applied by the caller (EffectsPage) before
    // this action was passed to UndoManager::perform(). Skip the first
    // perform() so we don't re-apply and destroy-then-recreate every DSP
    // (which would leave every panel's cached DSP* pointer dangling). Future
    // perform() calls (redo) do apply the change.
    bool perform() override
    {
        if (mFirstPerform) { mFirstPerform = false; return true; }
        mApply(mAfter);
        return true;
    }
    bool undo() override { mApply(mBefore); return true; }

private:
    juce::String  mLabel;
    SlotSnapshots mBefore, mAfter;
    ApplyFn       mApply;
    bool          mFirstPerform { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectRackAction)
};

// ── AutomationLaneEditAction ──────────────────────────────────────────────────
// Before/after snapshot of one AutomationLane (inside an ArrangementBlock).
// Used by EventEditor for all point edits (add, move, erase, curve-type change).
// ─────────────────────────────────────────────────────────────────────────────
class AutomationLaneEditAction : public juce::UndoableAction
{
public:
    using ApplyFn = std::function<void(const AutomationLane&)>;

    AutomationLaneEditAction(juce::String label,
                             AutomationLane before, AutomationLane after,
                             ApplyFn applyFn)
        : mLabel(std::move(label))
        , mBefore(std::move(before))
        , mAfter(std::move(after))
        , mApply(std::move(applyFn))
    {}

    bool perform() override { mApply(mAfter);  return true; }
    bool undo()    override { mApply(mBefore); return true; }
    int  getSizeInUnits() override
    {
        return (int)((mBefore.points.size() + mAfter.points.size()) * sizeof(ControlPoint));
    }

private:
    juce::String   mLabel;
    AutomationLane mBefore, mAfter;
    ApplyFn        mApply;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutomationLaneEditAction)
};

// ── StructuralOpAction (QA-UndoCoverage Task 7) ───────────────────────────────
// One structural user gesture (tab add / delete / duplicate, engine pick or
// swap, kit load) as one undoable transaction.  The op has ALREADY happened
// when this action is performed (skip-first convention); undoFn reverses it
// and redoFn re-applies it.  Snapshot temp files referenced by the lambdas
// are owned here -- deleted when the action dies (depth-cap drop, history
// clear, manager teardown), which is what keeps the UndoSnapshots store
// bounded by the history depth.
// ─────────────────────────────────────────────────────────────────────────────
class StructuralOpAction : public juce::UndoableAction
{
public:
    StructuralOpAction (std::function<void()> undoFn,
                        std::function<void()> redoFn,
                        juce::Array<juce::File> ownedSnapshots = {})
        : mUndo (std::move (undoFn))
        , mRedo (std::move (redoFn))
        , mSnapshots (std::move (ownedSnapshots))
    {}

    ~StructuralOpAction() override
    {
        for (const auto& f : mSnapshots)
            f.deleteFile();
    }

    bool perform() override
    {
        if (mFirstPerform) { mFirstPerform = false; return true; }
        if (mRedo) mRedo();
        return true;
    }
    bool undo() override
    {
        if (mUndo) mUndo();
        return true;
    }
    int getSizeInUnits() override { return (int) sizeof (*this); }

private:
    std::function<void()>  mUndo, mRedo;
    juce::Array<juce::File> mSnapshots;
    bool                   mFirstPerform { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StructuralOpAction)
};

// ── PatternListAction (QA-UndoCoverage Task 4) ────────────────────────────────
// Project-slice snapshot for pattern-list ops (add / duplicate / remove).
// The slice is {patterns, currentPattern, blocks, row state} because
// PatternManager::removePattern cascades: blocks of the dead pattern are
// erased and every higher patternIndex is re-indexed -- a single-Pattern
// payload cannot restore that.  The slice itself carries NO markers, so any
// gesture that also destroys or re-points linked TS markers must bank a
// MarkerSetAction into the same transaction (see performPatternTsOp).
// ─────────────────────────────────────────────────────────────────────────────
struct PatternListSnapshot
{
    std::vector<Pattern>          patterns;
    int                           currentPattern { 0 };
    std::vector<ArrangementBlock> blocks;
    std::vector<juce::String>     rowNames;
    std::vector<int>              rowGroups;
    std::vector<juce::uint32>     rowColors;
    std::vector<char>             rowMuted, rowSoloed;
};

class PatternListAction : public juce::UndoableAction
{
public:
    using ApplyFn = std::function<void(const PatternListSnapshot&)>;

    PatternListAction(PatternListSnapshot before, PatternListSnapshot after,
                      ApplyFn applyFn)
        : mBefore(std::move(before))
        , mAfter(std::move(after))
        , mApply(std::move(applyFn))
    {}

    bool perform() override { mApply(mAfter);  return true; }
    bool undo()    override { mApply(mBefore); return true; }
    int  getSizeInUnits() override
    {
        return (int)((mBefore.patterns.size() + mAfter.patterns.size()) * 1024
                     + (mBefore.blocks.size() + mAfter.blocks.size()) * sizeof(ArrangementBlock));
    }

private:
    PatternListSnapshot mBefore, mAfter;
    ApplyFn             mApply;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternListAction)
};

// ── PatternRenameAction / PatternColorAction ──────────────────────────────────
// Light single-field pattern edits -- the full slice above would copy every
// pattern's note content for a name/color write.
// ─────────────────────────────────────────────────────────────────────────────
class PatternRenameAction : public juce::UndoableAction
{
public:
    using ApplyFn = std::function<void(int index, const juce::String& name)>;

    PatternRenameAction(int index, juce::String before, juce::String after,
                        ApplyFn applyFn)
        : mIndex(index), mBefore(std::move(before)), mAfter(std::move(after))
        , mApply(std::move(applyFn))
    {}

    bool perform() override { mApply(mIndex, mAfter);  return true; }
    bool undo()    override { mApply(mIndex, mBefore); return true; }
    int  getSizeInUnits() override { return (int)sizeof(*this); }

private:
    int          mIndex;
    juce::String mBefore, mAfter;
    ApplyFn      mApply;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternRenameAction)
};

class PatternColorAction : public juce::UndoableAction
{
public:
    using ApplyFn = std::function<void(int index, juce::Colour colour)>;

    PatternColorAction(int index, juce::Colour before, juce::Colour after,
                       ApplyFn applyFn)
        : mIndex(index), mBefore(before), mAfter(after)
        , mApply(std::move(applyFn))
    {}

    bool perform() override { mApply(mIndex, mAfter);  return true; }
    bool undo()    override { mApply(mIndex, mBefore); return true; }
    int  getSizeInUnits() override { return (int)sizeof(*this); }

private:
    int          mIndex;
    juce::Colour mBefore, mAfter;
    ApplyFn      mApply;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternColorAction)
};

// ── MarkerSetAction (QA-UndoCoverage Task 4) ──────────────────────────────────
// Before/after snapshot of the three ruler lists (time markers, time-sig
// changes, tempo changes) + the current-TS uid.  Cheap: the lists are small
// POD-ish vectors.  Ops that ALSO write a pattern's TS fields
// (setPatternTimeSig / resetPatternTimeSig) ride a PatternListAction in the
// same transaction.
// ─────────────────────────────────────────────────────────────────────────────
struct MarkerSetSnapshot
{
    std::vector<TimeMarker>    timeMarkers;
    std::vector<TimeSigChange> timeSigChanges;
    std::vector<TempoChange>   tempoChanges;
    int                        currentTsUid { -1 };
};

class MarkerSetAction : public juce::UndoableAction
{
public:
    using ApplyFn = std::function<void(const MarkerSetSnapshot&)>;

    MarkerSetAction(MarkerSetSnapshot before, MarkerSetSnapshot after,
                    ApplyFn applyFn)
        : mBefore(std::move(before))
        , mAfter(std::move(after))
        , mApply(std::move(applyFn))
    {}

    bool perform() override { mApply(mAfter);  return true; }
    bool undo()    override { mApply(mBefore); return true; }
    int  getSizeInUnits() override { return (int)sizeof(*this); }

private:
    MarkerSetSnapshot mBefore, mAfter;
    ApplyFn           mApply;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MarkerSetAction)
};

// ── AudioLibraryAction (QA-UndoCoverage Task 4) ───────────────────────────────
// Before/after snapshot of the Builder audio library (entries + manual
// groups) plus the arrangement blocks, because library ops cascade into
// blocks (entry delete removes its clips; alias rename rewrites block
// displayAlias).  Blocks stay empty-empty for pure-library ops.
// ─────────────────────────────────────────────────────────────────────────────
struct AudioLibrarySnapshot
{
    std::vector<PatternManager::AudioLibraryEntry>  entries;
    std::vector<std::pair<int, juce::String>>       manualGroups;
    std::vector<ArrangementBlock>                   blocks;
    bool                                            includesBlocks { false };
};

class AudioLibraryAction : public juce::UndoableAction
{
public:
    using ApplyFn = std::function<void(const AudioLibrarySnapshot&)>;

    AudioLibraryAction(AudioLibrarySnapshot before, AudioLibrarySnapshot after,
                       ApplyFn applyFn)
        : mBefore(std::move(before))
        , mAfter(std::move(after))
        , mApply(std::move(applyFn))
    {}

    bool perform() override { mApply(mAfter);  return true; }
    bool undo()    override { mApply(mBefore); return true; }
    int  getSizeInUnits() override
    {
        return (int)((mBefore.entries.size() + mAfter.entries.size()) * 128
                     + (mBefore.blocks.size() + mAfter.blocks.size()) * sizeof(ArrangementBlock));
    }

private:
    AudioLibrarySnapshot mBefore, mAfter;
    ApplyFn              mApply;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioLibraryAction)
};

// ── AutomationTemplateAction (QA-UndoCoverage Task 4) ─────────────────────────
// Before/after snapshot of the automation-template list, optionally with the
// arrangement blocks (template delete cascades into its clips; template
// rename rewrites block lane display names).
// ─────────────────────────────────────────────────────────────────────────────
struct AutomationTemplateSnapshot
{
    std::vector<AutomationLane>   templates;
    std::vector<ArrangementBlock> blocks;
    bool                          includesBlocks { false };
};

class AutomationTemplateAction : public juce::UndoableAction
{
public:
    using ApplyFn = std::function<void(const AutomationTemplateSnapshot&)>;

    AutomationTemplateAction(AutomationTemplateSnapshot before,
                             AutomationTemplateSnapshot after,
                             ApplyFn applyFn)
        : mBefore(std::move(before))
        , mAfter(std::move(after))
        , mApply(std::move(applyFn))
    {}

    bool perform() override { mApply(mAfter);  return true; }
    bool undo()    override { mApply(mBefore); return true; }
    int  getSizeInUnits() override
    {
        return (int)((mBefore.templates.size() + mAfter.templates.size()) * 256
                     + (mBefore.blocks.size() + mAfter.blocks.size()) * sizeof(ArrangementBlock));
    }

private:
    AutomationTemplateSnapshot mBefore, mAfter;
    ApplyFn                    mApply;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutomationTemplateAction)
};
