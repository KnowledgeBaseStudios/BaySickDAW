#pragma once
#include <JuceHeader.h>
#include "DrumKitGrid.h"
#include "PianoRoll.h"
#include "UndoActions.h"

class StandalonePlayHead;

// ─────────────────────────────────────────────────────────────────────────────
// PianoRollPage
// ─────────────────────────────────────────────────────────────────────────────
// 2026-04-26: top-level page that consolidates every piano-roll-style view —
// Drum Kit, per-drum piano rolls, and per-engine (Layers/Bass) piano rolls —
// behind a single ribbon slot.  Single source of truth: PianoRollPage owns
// every PianoRollContainer + the DrumKitContainer.  Engine pages
// (Layers/Bass/Drum) navigate here via a Piano Roll nav button instead of
// hosting their own copies.
// ─────────────────────────────────────────────────────────────────────────────

// 2026-04-28 (Phase G-2): Clip added.  Sampler-style instrument fed by piano-
// roll notes; one Clip engine per ClipsPage.
// 2026-04-28 (Phase G-4): Vox + Inst added.  Same shape as Clip — engine
// picker per page, piano-roll triggering through the unified PianoRollPage.
enum class EngineKind { DrumKit, Layer, Bass, Drum, Clip, Vox, Inst };

struct EngineId
{
    EngineKind kind  { EngineKind::DrumKit };
    int        index { 0 };

    bool operator== (const EngineId& o) const noexcept { return kind == o.kind && index == o.index; }
    bool operator!= (const EngineId& o) const noexcept { return ! (*this == o); }
};

// Bundle of everything needed to wire a fresh PianoRollContainer for a
// given engine.  `dataAccessor` is a closure (NOT a raw pointer) so pattern
// switches stay live: PianoRollPage's timer re-runs it each tick to keep
// the container bound to the right slice of the current pattern.
struct PianoRollConnection
{
    std::function<PianoRollData*()>           dataAccessor;
    juce::Colour                              noteColor;
    juce::String                              displayName;
    std::function<void(int)>                  auditionMomentary;   // brief preview
    std::function<void(int)>                  auditionOn;          // press-and-hold on
    std::function<void(int)>                  auditionOff;         // press-and-hold off
    PianoRollContainer::RollMode              rollMode { PianoRollContainer::RollMode::Standard };
};

class PianoRollPage : public juce::Component,
                      public juce::Timer
{
public:
    PianoRollPage();
    ~PianoRollPage() override;

    void paint   (juce::Graphics& g) override;
    void resized ()                  override;
    void timerCallback ()            override;

    // ── Drum Kit (singleton — always present at the top of the dropdown) ───
    DrumKitContainer* getDrumKitContainer() { return mDrumKit.get(); }

    // ── Engine registry (Layers/Bass/Drum piano rolls) ─────────────────────
    void registerEngine    (EngineId id, PianoRollConnection conn);
    void unregisterEngine  (EngineId id);
    void selectEngine      (EngineId id);
    void setEngineDisplayName (EngineId id, const juce::String& name);
    EngineId getActiveEngineId () const { return mActive; }
    PianoRollContainer* getActivePianoRoll() const;   // null when DrumKit is active
    // Returns the currently visible PianoRollContainer if it's an engine roll
    // (used by Builder loop / time-selection consumers).  Returns null when
    // Drum Kit is active.
    PianoRollContainer* getActivePianoRollForLoop() const { return getActivePianoRoll(); }

    void setPlayHead    (StandalonePlayHead* ph);
    void setUndoContext (const UndoContext& ctx);
    // Editor wires this so the playhead-pump knows when to pass -1 (Song mode).
    std::function<bool()> isSongMode;

    // Editor sets this so PianoRollPage can build its dropdown popup with the
    // ribbon's current Layer/Bass/Drum order.  Returns the engines in the
    // order they should appear AFTER Drum Kit (which is always first).
    struct DropdownEntry { EngineId id; juce::String label; };
    std::function<std::vector<DropdownEntry>()> dropdownEnumerator;

    // Editor wires this to be notified after the user picks an engine in the
    // dropdown — typically to refresh the page-menu-bar pill label.
    std::function<void(EngineId)> onEngineSelected;

    // Builds a juce::PopupMenu of every entry (DrumKit + dropdownEnumerator())
    // and returns the menu.  Caller is responsible for showing the menu.
    juce::PopupMenu buildEngineDropdown();

private:
    struct EngineIdHash
    {
        std::size_t operator() (const EngineId& id) const noexcept
        {
            return ((std::size_t) id.kind << 16) ^ (std::size_t) id.index;
        }
    };

    // Owned containers.  Drum Kit always exists; engine rolls populate as
    // engines are registered.
    std::unique_ptr<DrumKitContainer>                                              mDrumKit;
    std::unordered_map<EngineId, std::unique_ptr<PianoRollContainer>, EngineIdHash> mRolls;
    std::unordered_map<EngineId, PianoRollConnection, EngineIdHash>                 mConns;

    EngineId            mActive { EngineKind::DrumKit, 0 };
    StandalonePlayHead* mPlayHead { nullptr };
    UndoContext         mUndoCtx;

    // Apply mActive's container to bounds; hide the rest.
    void applyActiveVisibility();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRollPage)
};
