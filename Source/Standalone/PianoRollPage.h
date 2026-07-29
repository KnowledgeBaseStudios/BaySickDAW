#pragma once
#include <JuceHeader.h>
#include "DrumKitGrid.h"
#include "PianoRoll.h"
#include "UndoActions.h"

class StandalonePlayHead;

// ─────────────────────────────────────────────────────────────────────────────
// PianoRollPage
// ─────────────────────────────────────────────────────────────────────────────
// 2026-04-26: top-level page that consolidates every piano-roll-style view -
// Drum Kit, per-drum piano rolls, and per-engine (Layers/Bass) piano rolls -
// behind a single ribbon slot.  Single source of truth: PianoRollPage owns
// every PianoRollContainer + the DrumKitContainer.  Engine pages
// (Layers/Bass/Drum) navigate here via a Piano Roll nav button instead of
// hosting their own copies.
// ─────────────────────────────────────────────────────────────────────────────

// 2026-04-28 (Phase G-2): Clip added.  Sampler-style instrument fed by piano-
// roll notes; one Clip engine per ClipsPage.
// 2026-04-28 (Phase G-4): Vox + Inst added.  Same shape as Clip - engine
// picker per page, piano-roll triggering through the unified PianoRollPage.
// 2026-05-03 (Phase J-2): BaySickRustyDrums added - distinct from the
// existing DrumKit value which represents the composited DrumKitGrid view.
// BaySickRustyDrums is the singleton sfizz-driven drum-kit engine that
// auto-spawns N mixer strips (one per kit channel).
// 2026-05-05 (Phase K-1 / L-1): BaySickGuitars + BaySickBasses added -
// per-Inst-page sfizz-driven melodic engines.  Multi-instance (up to 20
// shared with classic live-input Inst pages); EngineId::index = Inst page
// slot index.  Plural BaySickBasses to disambiguate from EngineKind::Bass
// (the BaySickBass synth tab).
// QA-ModelShell TS6 (BLU-447): Plugin APPENDED, and the position is
// load-bearing -- these enumerators' integer values ARE the live-MIDI target
// encoding the processor switches on (1 Layer / 2 Bass / 3 Drum / 4 Clip /
// 7 Guitars / 8 Basses / 9 Rusty), so Plugin must land on 10 and nothing above
// it may be inserted.
enum class EngineKind { DrumKit, Layer, Bass, Drum, Clip, Vox, Inst, BaySickGuitars, BaySickBasses, BaySickRustyDrums, Plugin };

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
    // C.5b: optional pattern-TS provider - returns current pattern's
    // intrinsic (num,den) so the page can refresh bar-line spacing when the
    // active pattern changes or the user right-clicks "Set Time Signature".
    std::function<void(int& outNum, int& outDen)>  patternTimeSigProvider;
    juce::Colour                              noteColor;
    juce::String                              displayName;
    // QA-D STATE-02 follow-on: per-CLAUDE.md 5F-6 design, the piano-roll
    // context label shows "{tabName} - {engineType}" (e.g. "Layer 1 -
    // Harmless", "Bass 2 - (no engine)").  engineType empty -> "(no engine)"
    // is substituted.  StandaloneEditor populates engineType at registration
    // (if engine already picked) and refreshes via PianoRollPage::setEngineType
    // when the user picks/swaps an engine on the Layer/Bass/Drum tab.
    juce::String                              engineType;
    std::function<void(int)>                  auditionMomentary;   // brief preview
    std::function<void(int)>                  auditionOn;          // press-and-hold on
    std::function<void(int)>                  auditionOff;         // press-and-hold off
    PianoRollContainer::RollMode              rollMode { PianoRollContainer::RollMode::Standard };
    // J-7b: optional engine-supplied label for a piano-roll key.  When set,
    // PianoKeyboard renders this string in place of the default "C5" naming
    // (used by BaySickRustyDrums to show "Snare Center", "Hi-hat Tight Closed",
    // etc.).  Returning empty falls back to the default JUCE note name.
    std::function<juce::String(int midiNote)>  noteLabelProvider;
    // QA-SfzGroup Sub-Q (2026-05-27): optional keyswitch-label provider.
    // BaySickPlayer engines hosting an SFZ with sw_lokey/sw_hikey + sw_label
    // opcodes return the human-readable keyswitch label ("C6 Sustain",
    // "C#6 Staccato") for the matching MIDI note.  PianoKeyboard renders
    // these keys with a distinct visual (amber highlight + label) so users
    // can discover the keyswitch range without inspecting the SFZ file.
    // Empty string for non-keyswitch notes or engines without keyswitching.
    std::function<juce::String(int midiNote)>  keyswitchLabelProvider;
    // QA-SlideSampler Task 4: engine-aware note-props context (Guitars/Basses set
    // this to expose the Flat/RP Slide/Bend panel + the patch's real bend range).
    // Empty on other engines -> the normal in-house note-props panel.
    std::function<PianoRollGrid::NoteEditContext()>  noteEditContextProvider;
    // J-7b: top-of-view default MIDI note when this engine is first selected.
    // -1 = no preference (use the page's existing default).
    int                                        defaultTopNote { -1 };
    // J-7b: paint every keyboard row as a white key (drum kit - sharps
    // have no musical meaning, labels need to be readable everywhere).
    bool                                       allKeysWhite { false };
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

    // ── Drum Kit (singleton - always present at the top of the dropdown) ───
    DrumKitContainer* getDrumKitContainer() { return mDrumKit.get(); }

    // ── Engine registry (Layers/Bass/Drum piano rolls) ─────────────────────
    void registerEngine    (EngineId id, PianoRollConnection conn);
    void unregisterEngine  (EngineId id);

    // QA-Ee Stage 3: wire the GLOBAL Unified_PianoRollSnapDiv accessors into every
    // roll + the drum kit (the editor provides the apvts read/write).
    void setSnapAccessors  (std::function<int()> getter, std::function<void(int)> setter);
    // QA-UICleanup Task 4: wire the GLOBAL Unified_QuantizeDiv accessors into every
    // roll + the drum kit (Tools > Quantize Settings writes; the Quantize action reads).
    void setQuantizeAccessors (std::function<int()> getter, std::function<void(int)> setter);
    void selectEngine      (EngineId id);
    void setEngineDisplayName (EngineId id, const juce::String& name);
    // QA-D STATE-02 follow-on: update the engine-type half of the context
    // label ("Layer 1 - <engineType>").  Wired by StandaloneEditor on every
    // onEngineSelected callback for Layer/Bass/Drum.  Pass empty string to
    // revert to the "(no engine)" placeholder.
    void setEngineType        (EngineId id, const juce::String& engineType);
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

    // #30 (QA-G3Smoke): output latency + sample rate for the playhead's visual
    // latency compensation (the page has no processor handle).  Permanent --
    // distinct from the Debug-only g3DiagDeviceInfo below.
    std::function<void(int& latencySamples, double& sampleRate)> deviceInfoProvider;

    // #31 (QA-G3Smoke): song-mode playhead.  Editor maps a SONG beat to the
    // VIEWED pattern's local beat (a block of that pattern whose span contains
    // the beat; local = songBeat - blockStart + contentOffset), or -1 when the
    // viewed pattern isn't playing there.  Pattern mode never calls this.
    std::function<double(double songBeat)> songLocalBeatProvider;

    // #30b regression fix (QA-G3Smoke): every roll/kit note mutation fires
    // this (via each container's onContentEdited) so the editor can
    // republish the scheduler's roll snapshot.  Stored + pushed to the kit
    // and every registered roll (present and future).
    void setContentEditedHook (std::function<void()> fn);

    // Editor wires this to read the processor's held hardware-MIDI notes each
    // timer tick (128-bit mask: lo = notes 0..63, hi = 64..127).  The active
    // roll's keyboard lights those keys so the user sees what they're playing.
    std::function<void(uint64_t&, uint64_t&)> liveHeldNotesProvider;

#if JUCE_DEBUG
    // [G3 PLAYHEAD] G-9 reading (QA-G3Smoke Task 1): the page has no processor
    // handle, so the editor supplies output latency + sample rate for the
    // per-tick readout.  Debug-only; stripped with the diagnostic at batch close.
    std::function<void(int& latencySamples, double& sampleRate)> g3DiagDeviceInfo;
#endif

    // Editor sets this so PianoRollPage can build its dropdown popup with the
    // ribbon's current Layer/Bass/Drum order.  Returns the engines in the
    // order they should appear AFTER Drum Kit (which is always first).
    struct DropdownEntry { EngineId id; juce::String label; };
    std::function<std::vector<DropdownEntry>()> dropdownEnumerator;

    // Editor wires this to be notified after the user picks an engine in the
    // dropdown - typically to refresh the page-menu-bar pill label.
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
    std::function<void()> mContentEditedHook;   // #30b regression fix: pushed to kit + every roll
    std::function<int()>     mSnapGetter;   // QA-Ee Stage 3: global snap read
    std::function<void(int)> mSnapSetter;   // QA-Ee Stage 3: global snap write
    std::function<int()>     mQuantizeGetter;   // QA-UICleanup Task 4: global quantize read
    std::function<void(int)> mQuantizeSetter;   // QA-UICleanup Task 4: global quantize write

    EngineId            mActive { EngineKind::DrumKit, 0 };
    StandalonePlayHead* mPlayHead { nullptr };
    UndoContext         mUndoCtx;
    // QA-H Task 7: last ghost set pushed to the active roll (change guard).
    std::vector<std::pair<const PianoRollData*, juce::Colour>> mLastGhosts;

    // Apply mActive's container to bounds; hide the rest.
    void applyActiveVisibility();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRollPage)
};
