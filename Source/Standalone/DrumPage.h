#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../EngineRig.h"
#include "../PatternManager.h"
#include "../DSP/EQ8MsDSP.h"
#include "SharedUI.h"
#include "PianoRoll.h"
#include "DrumKitGrid.h"
#include "StandaloneApp.h"
#include "UndoActions.h"

// ── DrumPage ──────────────────────────────────────────────────────────────────
// One Drums instrument page (D1.3, dynamic-drum model).  Up to kMaxDrumPages
// instances.  Functionally identical to LayersPage / BassPage - each drum tab
// owns one independent engine instance + its own piano roll.
//
// Three sub-tabs:
//   Tab 0 "Drum Kit"   - 16-row drum-pad / step-sequencer view (cross-drum)
//   Tab 1 "Player"     - the engine's editor, full page (engine is chosen at
//                        the ribbon "+" menu; sound picking is on the kit pads)
//   Tab 2 "Piano Roll" - PianoRollContainer bound to drumRolls[mPageIndex]
//
// D2 Drum Kit data model
// ─────────────────────────
// Per-row info shown in the kit view, in ribbon order:
struct KitDrumInfo
{
    int          ribbonTabId  { -1 };
    int          pageIndex    { -1 };   // index into drumRolls[] / mDrumEngines[]
    juce::String displayName;            // sound name (or "Pick a sound  v" if empty)
    bool         hasEngine    { false };
    bool         locked       { false };
    bool         isActive     { false };
    juce::Colour color;                  // drum's accent color (for active-row border)
};
//
// Engine choices: BaySickPlayer | BaySickSynth | BaySickBass | Harmless
// Per-page APVTS prefix: drm_{N}.  Engine InsertNode: BaySickGraph::InsertKind::Drum
// at index pageIndex (mixer_drum_<N>_*).
// ─────────────────────────────────────────────────────────────────────────────
class DrumPage : public juce::Component,
                 public juce::Timer
{
public:
    DrumPage(BaySickDAWProcessor& p, PatternManager& pm, int pageIndex);
    ~DrumPage() override;

    void paint  (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    void setPlayHead(StandalonePlayHead* ph);
    int  getPageIndex()    const { return mPageIndex; }
    int  getActiveTab()    const { return mActiveTab; }
    juce::Colour getPageColor() const { return mPageColor; }
    void setUndoContext(const UndoContext& ctx);

    void switchTab(int idx);

    // ── D2 Drum Kit hooks ─────────────────────────────────────────────────────
    // StandaloneEditor wires:
    //   getKitListProvider     - returns the cross-drum list in ribbon order
    //   onKitRowClicked        - picker click → activate / add-new / context
    //   onKitAuditionOn / Off  - piano-key press-and-hold (Batch 4)
    //   onKitReorderRow        - drag-handle drop → reorder ribbon + kit (Batch 4)
    void setKitListProvider     (std::function<std::vector<KitDrumInfo>()> fn);
    void setKitRowClickHandler  (std::function<void(int row, juce::Component* anchor)> fn);
    void setKitAuditionHandlers (std::function<void(int row)> onOn,
                                 std::function<void(int row)> onOff);
    void setKitReorderHandler   (std::function<void(int srcRow, int dstRow)> fn);
    // Batch 5: Kit button click - opens Save/Load Kit popup.  StandaloneEditor
    // wires this since kit save/load needs ribbon + DrumPage management access.
    void setKitMenuHandler      (std::function<void(juce::Component* anchor)> fn);
    // Lock/Unlock button click - wired by StandaloneEditor.  The argument is
    // the kit the grid is showing; the toggle applies to that kit alone.
    void setGlobalLockHandler   (std::function<void(int bank)> fn);
    void refreshKitView ();

    std::function<void()> onEngineSelected;
    // D1.4-fix: drum tab name auto-update.  Fired whenever mSoundName changes
    // (preset load, sample load, new patch, clear).  StandaloneEditor wires
    // this to ribbon->renameTab + mixer->renameChannel.
    std::function<void(const juce::String& newName)> onSoundNameChanged;
    // D2: fired whenever mLocked toggles (context-menu Lock Drum / serialized
    // import).  StandaloneEditor wires this to refreshAllKitViews so kit-tab
    // row labels reflect the new lock state immediately.
    std::function<void()> onLockChanged;
    // D2: fired when the user picks Rename from the per-drum right-click
    // context menu.  StandaloneEditor wires this to mRibbon->startRename
    // so the same rename dialog shown by the ribbon dropdown is used.
    std::function<void()> onRenameRequested;

    // D1.4-fix (c): right-click context menu hooks.  Editor wires these to
    // perform the actual tab-level operations (delete needs to remove the
    // ribbon entry, free indices, etc.; duplicate needs to spawn a new tab).
    std::function<void()>           onDeleteRequested;     // delete THIS drum tab
    std::function<void(const juce::String& clipboardXml)> onDuplicateRequested;
        // editor receives serialized drum state, creates new tab, applies it

    // QA-L-Fix (D-6): the user changed this drum's play pitch from the kit
    // menu.  The kit grid owns the undo stack for `drumRolls`, so re-pitching
    // the drum's existing hits is delegated up to whoever wired the kit view.
    std::function<void(int pageIdx, int oldNote, int newNote)> onPlayNoteChanged;

    void                setTabName(const juce::String& name);
    const juce::String& getTabName() const { return mTabName; }

    PianoRollContainer* getPianoRoll() const { return mPianoRoll.get(); }

    juce::String                getEngineType()      const { return mEngineType; }
    juce::AudioProcessor*       getEngineProcessor() const { return mEngineProcessor; }
    bool                        isEngineLocked()  const { return ! mEngineType.isEmpty(); }
    void selectEngine (const juce::String& engineName);

    // QA-Layout T3 (Window-3/4): title-strip chrome for the hosted engine --
    // see LayersPage.h for the contract.  Drums matter most here: a kit-pad
    // pick can SWAP the engine while the page is visible, and the rebuild
    // callback is what re-mounts the strip.
    juce::String     stripEngineTitle()  const;
    juce::Colour     stripEngineAccent() const;
    juce::Component* stripPresetButton() const;
    std::function<void()> onEngineEditorRebuilt;

    // D1.4-fix: sound picker popup.  QA-Layout T2: anchored from the kit
    // grid's per-pad pickers (the page's own picker button is gone);
    // selection auto-loads the right engine + applies state.
    void showSoundPicker  (juce::Component* anchor);
    // QA-L-Fix (D-2): `fromKit` gates the MIDI items (kit-only -- the mapping
    // exists to play the kit off a pad controller).  QA-Layout T2: !fromKit
    // adds the page-preset entries instead -- it is the Menu dropdown's shape
    // (showPageActionsMenu forwards here).
    void showContextMenu  (juce::Component* anchor, bool fromKit);   // D1.4-fix (c)

    // QA-L-Fix (D-4/D-6): the drum's play pitch -- the note it sounds at.
    // Setting it re-pitches this drum's existing hits that sit at the OLD
    // play note (via onPlayNoteChanged); hits deliberately placed at other
    // pitches stay put.
    int  getPlayNote () const;
    void setPlayNote (int midiNote);

    // QA-L-Fix (D-7/D-10): arm MIDI Learn for this drum's kit trigger.  The
    // capture handshake is polled from the existing page timer.
    void beginTriggerLearn();
    void loadSampleFile   (const juce::File& f);
    void loadSampleFolder (const juce::File& f);
    void loadSampleSFZ    (const juce::File& f);
    // Both preset loaders return a one-line failure description, empty on
    // success.  A damaged preset or a missing sample still leaves the drum
    // named and selectable -- the string is what keeps a silent slot from
    // reading as a fully loaded sound.
    juce::String loadSynthPreset  (const juce::File& xml);
    juce::String loadPlayerPreset (const juce::File& xml);   // D1.4-fix (c) BaySickPlayer
    void newBlankPatch    ();
    void savePatchAs      ();
    void clearSound       ();
    // QA-UndoCoverage Task 7: one drum-sound swap gesture = one structural
    // undo transaction (before/after full-chain snapshots).  Public so the
    // sound-picker menu callback wraps its terminal loads.
    void performSoundSwapGesture (const juce::String& label,
                                  const std::function<void()>& op);
    void requestDelete    ();   // confirms + (if BaySickSynth tweaked) prompts save first
    static juce::File presetsDir();        // Documents/BaySickDAW/Presets/BaySickDrums
    static juce::File userPresetsDir();    // <presetsDir>/My Presets

    // Lock state - when true, this drum is protected from accidental swap
    // (picker shows current sound name only; right-click delete still works).
    // Used by future "Random Kit" / "Replace Kit" actions to skip locked drums.
    bool isLocked() const { return mLocked; }
    void setLocked(bool l)
    {
        if (mLocked == l) return;
        mLocked = l;
        refreshPianoRollContextLabel();
        if (onLockChanged) onLockChanged();
    }

    // Drum state snapshot for Copy/Paste/Duplicate.  Exports the engine type
    // + engine state + sound name + lock status as a serialized XML string.
    juce::String exportDrumState() const;
    void         importDrumState (const juce::String& xml);

    // ── G-7 (2026-04-29): Page Preset save/load (full chain) ─────────────────
    // onSaved fires only on successful save (cancel/empty-name aborts the
    // continuation).  Used by requestDelete's "Save Page Preset & Delete"
    // button to chain delete after save completes.
    void savePagePreset   (std::function<void()> onSaved = {});
    void loadPagePreset   (const juce::File& xml);
    void showPageActionsMenu (juce::Component* anchor);
    // QA-Layout T15: the title strip's nav buttons dissolved into the Menu
    // dropdown -- the editor injects the window/view entries at the top of
    // the page-actions popup (showContextMenu, !fromKit only) through this hook.
    std::function<void(juce::PopupMenu&)> onBuildWindowNavMenu;

    // XML-string variants used by kit save/load so the full chain
    // (engine + strip params + insert rack + post-EQ) round-trips per drum
    // without writing per-drum temp files.
    juce::String exportPagePresetXml() const;
    void         importPagePresetXml (const juce::String& xml);

private:
    // QA-L-Fix: message-thread half of the trigger-learn handshake.  Polled
    // from timerCallback; commits the capture + runs the D-10 prompt.
    void pollTriggerLearn();
    // Weak: the AlertWindow is modal with deleteWhenDismissed, so JUCE owns it.
    // A unique_ptr here would double-free when the window self-deletes.
    juce::Component::SafePointer<juce::AlertWindow> mLearnAlert;
    // 0 = not learning.  Absolute ms deadline for the 30 s auto-cancel.
    juce::uint32 mLearnDeadlineMs { 0 };

    BaySickDAWProcessor& mProcessor;
    PatternManager&     mPM;
    int                 mPageIndex;   // 0..kMaxDrumPages-1
    juce::Colour        mPageColor;
    StandalonePlayHead* mPlayHead { nullptr };

    int  mActiveTab   { 0 };

    // Tab 0: Drum Kit (D2 - cross-drum 16-row piano-roll-style view).
    // Same content regardless of which drum tab is active in the ribbon.
    std::unique_ptr<DrumKitContainer>           mDrumKitTab;

    // Tab 1: Player.  QA-Layout T2 (L4): the picker button + sound-name
    // label row is gone -- the engine editor fills the tab.
    std::unique_ptr<juce::Component>            mPlayerTab;
    // QA-ModelShell TS1: non-owning view of the rig-owned engine ({Drums,
    // pageIndex}); the editor IS view-owned and must die before the page
    // (its attachments reference the engine's APVTS).
    juce::AudioProcessor*                       mEngineProcessor { nullptr };
    std::unique_ptr<juce::AudioProcessorEditor> mEngineEditor;
    std::unique_ptr<juce::FileChooser>          mFileChooser;      // outlives async picker callbacks
    juce::String mEngineType;     // "" = no engine yet
    juce::String mSoundName;      // current sound display name
    juce::String mTabName;
    bool         mLocked { false };  // D1.4-fix (c): protect from kit-replace

    // D1.4-fix (c): track loaded sample for BaySickPlayer preset save.
    // mLoadedSampleKind lets savePatchAs reconstruct the correct loadX call
    // on preset reload.  Cleared on engine swap.
    enum class SampleKind { None, File, Folder, SFZ };
    SampleKind mLoadedSampleKind { SampleKind::None };
    juce::File mLoadedSamplePath;

    // D2: snapshot of engine state taken after every preset / sample load
    // and after a successful Save Patch As.  requestDelete() compares the
    // current engine state against this snapshot to decide which prompt to
    // show - clean patch gets the simple "Delete drum?" confirmation; a
    // dirty patch gets the Save & Delete / Delete Anyway / Cancel three-way
    // so the user can capture their tweaks before deleting.
    juce::MemoryBlock mLoadedStateSnapshot;
    void takeStateSnapshot();
    bool isPatchDirty() const;

    // QA-UndoCoverage Task 7: undo context (structural sound-swap gesture) +
    // the unwrapped clear body the undo-apply path uses.
    UndoContext mUndoCtx;
    void clearSoundInternal();
    // mLocked has no APVTS parameter behind it, so the page menu's Lock item
    // needs its own transaction to satisfy the every-action-undoable rule.
    void toggleLockWithUndo();

    // Tab 2: Piano Roll
    std::unique_ptr<PianoRollContainer>         mPianoRoll;

    // J-6 EQ unification (2026-05-03): EQ tab + display removed; pre-rack EQ
    // is exclusively edited via the Effects page Pre EQ tab.

    void buildDrumKitTab();
    void buildPlayerTab();
    void buildPianoRollTab();
    void refreshPianoRollContextLabel();

    juce::String trackId() const { return "drm_" + juce::String(mPageIndex); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumPage)
};
