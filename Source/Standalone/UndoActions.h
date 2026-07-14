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
// (type + bypassed + outputGainDb + DSP state blob + UUID) so undo/redo of
// Move/Load/Remove preserves the full slot configuration including knob
// values and the slot's UUID (which keeps automation lanes pointed at the
// right paramId after an undo).
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
