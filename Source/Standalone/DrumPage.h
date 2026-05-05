#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../PatternManager.h"
#include "../DSP/EQ8MsDSP.h"
#include "SharedUI.h"
#include "PianoRoll.h"
#include "DrumKitGrid.h"
#include "StandaloneApp.h"
#include "UndoActions.h"

// ── DrumPage ──────────────────────────────────────────────────────────────────
// One Drums instrument page (D1.3, dynamic-drum model).  Up to kMaxDrumPages
// instances.  Functionally identical to LayersPage / BassPage — each drum tab
// owns one independent engine instance + its own piano roll.
//
// Four sub-tabs (D2 added Drum Kit at index 0):
//   Tab 0 "Drum Kit"   — 16-row drum-pad / step-sequencer view (cross-drum)
//   Tab 1 "Player"     — engine selector + engine editor (locks on first pick)
//   Tab 2 "Piano Roll" — PianoRollContainer bound to drumRolls[mPageIndex]
//   Tab 3 "EQ"         — EQ8 M/S (pre-rack)
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
// Per-page APVTS prefix: drm_{N}.  Engine InsertNode: VibeGraph::InsertKind::Drum
// at index pageIndex (mixer_drum_<N>_*).
// ─────────────────────────────────────────────────────────────────────────────
class DrumPage : public juce::Component,
                 public juce::Timer
{
public:
    DrumPage(VibeSynthProcessor& p, PatternManager& pm, int pageIndex);
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
    std::function<void(int idx)> onSubTabChanged;

    // ── D2 Drum Kit hooks ─────────────────────────────────────────────────────
    // StandaloneEditor wires:
    //   getKitListProvider     — returns the cross-drum list in ribbon order
    //   onKitRowClicked        — picker click → activate / add-new / context
    //   onKitAuditionOn / Off  — piano-key press-and-hold (Batch 4)
    //   onKitReorderRow        — drag-handle drop → reorder ribbon + kit (Batch 4)
    void setKitListProvider     (std::function<std::vector<KitDrumInfo>()> fn);
    void setKitRowClickHandler  (std::function<void(int row, juce::Component* anchor)> fn);
    void setKitAuditionHandlers (std::function<void(int row)> onOn,
                                 std::function<void(int row)> onOff);
    void setKitReorderHandler   (std::function<void(int srcRow, int dstRow)> fn);
    // Batch 5: Kit button click — opens Save/Load Kit popup.  StandaloneEditor
    // wires this since kit save/load needs ribbon + DrumPage management access.
    void setKitMenuHandler      (std::function<void(juce::Component* anchor)> fn);
    // 2026-04-26: Global Lock/Unlock button click — wired by StandaloneEditor.
    void setGlobalLockHandler   (std::function<void()> fn);
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

    void                setTabName(const juce::String& name);
    const juce::String& getTabName() const { return mTabName; }

    PianoRollContainer* getPianoRoll() const { return mPianoRoll.get(); }

    juce::String                getEngineType()      const { return mEngineType; }
    juce::AudioProcessor*       getEngineProcessor() const { return mEngineProcessor.get(); }
    bool                        isEngineLocked()  const { return ! mEngineType.isEmpty(); }
    void selectEngine (const juce::String& engineName);

    // D1.4-fix: picker actions.  Picker popup is a single button on the
    // Player tab; selection auto-loads the right engine + applies state.
    void showSoundPicker  (juce::Component* anchor);
    void showContextMenu  (juce::Component* anchor);     // D1.4-fix (c)
    void loadSampleFile   (const juce::File& f);
    void loadSampleFolder (const juce::File& f);
    void loadSampleSFZ    (const juce::File& f);
    void loadSynthPreset  (const juce::File& xml);
    void loadPlayerPreset (const juce::File& xml);   // D1.4-fix (c) BaySickPlayer
    void newBlankPatch    ();
    void savePatchAs      ();
    void clearSound       ();
    void requestDelete    ();   // confirms + (if BaySickSynth tweaked) prompts save first
    static juce::File presetsDir();        // Documents/BaySickDAW/Presets/BaySickDrums
    static juce::File userPresetsDir();    // <presetsDir>/My Presets

    // Lock state — when true, this drum is protected from accidental swap
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

    // XML-string variants used by kit save/load so the full chain
    // (engine + strip params + insert rack + post-EQ) round-trips per drum
    // without writing per-drum temp files.
    juce::String exportPagePresetXml() const;
    void         importPagePresetXml (const juce::String& xml);

private:
    VibeSynthProcessor& mProcessor;
    PatternManager&     mPM;
    int                 mPageIndex;   // 0..kMaxDrumPages-1
    juce::Colour        mPageColor;
    StandalonePlayHead* mPlayHead { nullptr };

    int  mActiveTab   { 0 };

    // Tab 0: Drum Kit (D2 — cross-drum 16-row piano-roll-style view).
    // Same content regardless of which drum tab is active in the ribbon.
    std::unique_ptr<DrumKitContainer>           mDrumKitTab;

    // Tab 1: Player
    std::unique_ptr<juce::Component>            mPlayerTab;
    std::unique_ptr<juce::TextButton>           mPickerBtn;        // "Pick a sound ▾"
    std::unique_ptr<juce::Label>                mSoundLabel;       // current sound name
    std::unique_ptr<juce::AudioProcessor>       mEngineProcessor;
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
    // show — clean patch gets the simple "Delete drum?" confirmation; a
    // dirty patch gets the Save & Delete / Delete Anyway / Cancel three-way
    // so the user can capture their tweaks before deleting.
    juce::MemoryBlock mLoadedStateSnapshot;
    void takeStateSnapshot();
    bool isPatchDirty() const;

    // Tab 2: Piano Roll
    std::unique_ptr<PianoRollContainer>         mPianoRoll;

    // J-6 EQ unification (2026-05-03): EQ tab + display removed; pre-rack EQ
    // is exclusively edited via the Effects page Pre EQ tab.

    void buildDrumKitTab();
    void buildPlayerTab();
    void buildPianoRollTab();
    void refreshPianoRollContextLabel();

    // D1.4-fix (c): per-instance MouseListener that catches right-clicks on
    // the picker button (TextButton consumes left-clicks via onClick, so we
    // need a listener for the popup-menu modifier).
    struct PickerRightClickListener : public juce::MouseListener
    {
        DrumPage* owner { nullptr };
        void mouseDown (const juce::MouseEvent& e) override;
    };
    PickerRightClickListener mPickerRC;

    juce::String trackId() const { return "drm_" + juce::String(mPageIndex); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumPage)
};
