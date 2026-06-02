#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "StandaloneApp.h"
#include "SharedUI.h"
#include "GlobalTransportBar.h"
#include "RibbonTabBar.h"
#include "TrackSelectionManager.h"
#include "EffectsPage.h"
#include "UndoActions.h"
#include "UndoHistoryWindow.h"
#include "EventEditor.h"
#include "DrumPage.h"   // D2: KitDrumInfo struct used in helper signatures below
#include "PagePresetIO.h"   // G-7: PageKind enum used in helper signatures below

class PatternManager;
class ProjectManager;
class LayersPage;
class BassPage;
// DrumsPage removed 2026-04-25 - replaced by per-drum DrumPage class.
class BuilderPage;
class MixerPage;
class PianoRollContainer;
class KeyBindsWindow;   // 2026-04-26 (Phase A - keymap editor popup)
class ClipsPage;        // 2026-04-28 (G-2: Clips engine page)
class ClipsEmptyState;  // 2026-04-28 (G-2: Clips empty-state placeholder)
class VoxPage;          // 2026-04-28 (G-4: Vox engine page)
class VoxEmptyState;    // 2026-04-28 (G-4: Vox empty-state placeholder)
class InstPage;         // 2026-04-28 (G-4: Inst engine page)
class InstEmptyState;   // 2026-04-28 (G-4: Inst empty-state placeholder)
class BaySickRustyDrumsPage;  // J-6 (2026-05-03): singleton drum-kit page

// ── StandaloneEditor ──────────────────────────────────────────────────────────
// Top-level UI component. Owns:
//   - MenuBarComponent  (title bar menus: FILE/EDIT/PATTERNS/VIEW/OPTIONS/HELP)
//   - Header strip      (title + pattern selector + level dials)
//   - GlobalTransportBar
//   - RibbonTabBar      (Chrome-style dynamic tabs)
//   - Content area      (switches between tab pages)
//
// Phase 1 tab layout:
//   System tabs (permanent): Mixer | Effects | Builder
//   Dynamic tabs:            Layers | Bass | Drums  (closeable, multiple allowed
//                            except Drums which is limited to 1)
// ─────────────────────────────────────────────────────────────────────────────
class StandaloneEditor : public  juce::Component,
                         public  juce::MenuBarModel,
                         public  juce::ApplicationCommandTarget
{
public:
    StandaloneEditor(VibeSynthProcessor& p, StandalonePlayHead& ph,
                     juce::AudioDeviceManager& dm);
    ~StandaloneEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

    // handleCommandMessage: pages use this to request tab switches
    void handleCommandMessage(int commandId) override;

    // Public access for audio thread / other components
    PatternManager& getPatternManager();
    juce::UndoManager& getUndoManager() { return mUndoManager; }

    // ── Undo dispatch (call instead of UndoManager directly) ─────────────────
    // Takes ownership of action, updates history list, then forwards to manager.
    bool doUndoAction(juce::UndoableAction* action, const juce::String& label);
    void globalUndo();
    void globalRedo();
    void showHistoryWindow();

    // Build a UndoContext token pointing at this editor's undo infrastructure.
    UndoContext makeUndoContext();

    // Audio transport
    void startPlayback(double bpm);
    void stopPlayback();
    // QA-Ea Task 0b (2026-05-18): shared transport-halt + recording-finalize.
    // Called by the manual Stop button AND the song-end auto-stop path so
    // play-through end finalizes the recording exactly like pressing Stop
    // (was stopPlayback-only -> recorder kept writing silence until manual
    // Stop).  Forks #25.
    void stopTransportAndFinalizeRecording();

    // Called by StandaloneApp after construction so the audio settings dialog
    // can safely unregister/re-register the callback around device switches.
    void setAudioCallback(juce::AudioIODeviceCallback* cb) { mAudioCallback = cb; }

    // ── Event Editor (Phase 4D) ───────────────────────────────────────────────
    // Open a floating EventEditor for the given ArrangementBlock index.
    // Multiple windows can be open simultaneously; re-activates if already open.
    void openEventEditor(int blockIdx);

    // Find or create an Automation block for paramId, then open EventEditor.
    // If an Automation block already exists with this paramId, opens that one.
    // Otherwise creates a new block (row=next free, bar=0, length=4) and opens it.
    void openEventEditorForParam(const juce::String& paramId);

    // Create (or find existing) automation block for paramId. Returns block index, or -1.
    int  createAutomationBlock(const juce::String& paramId);

    // Resolve an automation-lane paramId into a human-readable display label.
    // Format: "Channel - Effect - Param" (e.g. "Layers Bus - Reverb - Mix"),
    // falling back to just "Channel - Param" when there's no effect slot,
    // or to the raw paramId when the prefix can't be parsed.
    // Recomputed on demand so effect swaps inside a slot reflect immediately.
    juce::String resolveAutomationDisplayName(const juce::String& paramId) const;

    // Convenience: respects an AutomationLane's userDisplayName override.
    // Returns userDisplayName if non-empty, otherwise the auto-resolved name.
    juce::String displayNameFor(const AutomationLane& lane) const;

    // ── MenuBarModel ──────────────────────────────────────────────────────────
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu   getMenuForIndex(int menuIndex, const juce::String&) override;
    void              menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    // ── ApplicationCommandTarget (Phase A - keymap framework) ────────────────
    juce::ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
    void getAllCommands  (juce::Array<juce::CommandID>& out) override;
    void getCommandInfo  (juce::CommandID id, juce::ApplicationCommandInfo& info) override;
    bool perform         (const InvocationInfo& info) override;

    // Public access to the command manager (used by Help > Key Binds popup).
    juce::ApplicationCommandManager& getCommandManager() noexcept { return mCmdMgr; }

private:
    // ── Internal types ────────────────────────────────────────────────────────
    // Each open tab in the ribbon owns one PageEntry.
    struct PageEntry
    {
        int             ribbonTabId { -1 };
        RibbonTabBar::TabType type;
        std::unique_ptr<juce::Component> component; // the page content component
    };

    // ── Core helpers ──────────────────────────────────────────────────────────
    void buildDefaultTabs();     // called in ctor: Builder + initial Layers/Bass/Drums
    // 2026-04-24 File > New reset: adds just the three default Layers / Bass /
    // Drums tabs.  Split out of buildDefaultTabs so File > New can rebuild
    // them without also re-adding the system (Mixer / Effects / Builder)
    // tabs that persist across project changes.
    void addDefaultDynamicTabs();
    void onAddTabRequest(RibbonTabBar::TabType type);
    void onTabSelected(int tabId);
    void onTabClosed(int tabId);
    void onSubPageSelected(RibbonTabBar::TabType type, int subPageIndex);
    void showPageForTab(int tabId);
    void refreshPatternBox();

    // Create page components for each type
    std::unique_ptr<juce::Component> createLayersPage();
    std::unique_ptr<juce::Component> createBassPage();
    std::unique_ptr<juce::Component> createDrumPage();    // D1.4: dynamic-drum tab

    // P1+P2 persistence (2026-04-24): deterministic-index page creation used
    // by project load.  Returns null if `idx` is out of range or already in
    // use.  Unlike the index-less variants these do NOT pick the first
    // available slot - they demand the saved pageIndex, so the PatternManager's
    // layerRoll[idx] / bassRoll[idx] notes end up on the right tab.
    std::unique_ptr<juce::Component> createLayersPageAtIndex (int idx);
    std::unique_ptr<juce::Component> createBassPageAtIndex   (int idx);
    std::unique_ptr<juce::Component> createDrumPageAtIndex   (int idx);   // D1.4
    void spawnDuplicateDrumTab  (const juce::String& clipboardXml);        // D1.4-fix (c)
    void spawnDuplicateLayerTab (const juce::String& clipboardXml);        // D1.4-fix (c)
    void spawnDuplicateBassTab  (const juce::String& clipboardXml);        // D1.4-fix (c)

    // Project-load orchestration: tear down every dynamic (Layers / Bass /
    // Drums) tab + rebuild from a <UIState> element.  Safe to call repeatedly.
    void serializeUIState   (juce::XmlElement& root);
    void deserializeUIState (const juce::XmlElement& root);
    void closeAllDynamicTabs();
    std::unique_ptr<juce::Component> createBuilderPage();
    std::unique_ptr<juce::Component> createMixerPage();
    std::unique_ptr<juce::Component> createEffectsPage();
    // 2026-04-26 (1a): unified Piano Roll page.  Stub for step 1a; hosts the
    // Drum Kit container in step 1b and full engine list in step 2.
    std::unique_ptr<juce::Component> createPianoRollPage();

    // Returns the piano-roll container of the currently-visible instrument
    // page (Layers/Bass/Drums), or nullptr if no such page is visible / the
    // visible page's Piano Roll sub-tab is not currently selected.
    PianoRollContainer* getActivePianoRollForLoop() const;

    // ── D2 Drum Kit support ───────────────────────────────────────────────
    // Walk mPages, filter to TabType::Drums in ribbon order, build a
    // KitDrumInfo list with active-tab marked.  Used by every DrumPage's
    // DrumKitContainer via its provider callback.
    std::vector<KitDrumInfo> getKitDrumList() const;
    // Iterate all DrumPage instances and call refreshKitView() on each.
    // Triggered after add / remove / rename / active-tab change.
    void refreshAllKitViews();
    // Wire a freshly-created DrumPage's kit-view callbacks (provider + click
    // handler).  Called from every DrumPage creation site.
    void wireDrumPageKitView (class DrumPage* dp);
    // 2026-04-26 (1b): same wiring as wireDrumPageKitView, applied directly
    // to the DrumKitContainer hosted on PianoRollPage.  Both views share the
    // same `Pattern::drumRolls[]` data so edits propagate either way.
    void wirePianoRollPageKitView (class PianoRollPage* prp);

    // 2026-04-26 (step 2 commit 2): register a freshly-created engine page
    // with PianoRollPage so its piano roll lives on the unified page.  Each
    // helper builds a closure-based PianoRollConnection (see PianoRollPage.h)
    // capturing the engine page raw ptr - audition reads getEngineProcessor()
    // per call so engine swaps survive without re-registering.
    void registerLayerPianoRoll (class LayersPage* lp);
    void registerBassPianoRoll  (class BassPage*   bp);
    void registerDrumPianoRoll  (class DrumPage*   dp);
    void registerBaySickRustyDrumsPianoRoll();   // J-7a (2026-05-03)

    // 2026-05-05 dirty-flag wiring helper.  Each per-engine processor
    // (Harmless / BaySickSynth / BaySickBass / VibePlayer / BaySickGuitars /
    // BaySickRustyDrums / BaySickPedals / BaySickNAMIR / BaySickVocal) owns
    // its own APVTS that's invisible to the main PluginProcessor's project-
    // dirty listener.  This helper dynamic_casts to each known type and
    // installs a markDirty hook so any APVTS edit on the engine flips the
    // project dirty bit.  Pass nullptr safe.  No-op for unknown types.
    void wireEngineDirtyHook (juce::AudioProcessor* eng);
    // D2 Batch 4: move drum at row `srcRow` to row `dstRow` in ribbon order.
    // Reorders mPages + mRibbon's tab list + refreshes every kit view.
    void moveDrumTab (int srcRow, int dstRow);

    // ── Batch 5: Kit save/load ────────────────────────────────────────────
    // Kit XML format: <BaySickKit name="..." version="1">
    //                   <Drum slot="N">
    //                     <DrumPageState .../>     <!-- exportDrumState() XML -->
    //                   </Drum>  ... × non-empty slots
    //                 </BaySickKit>
    // Empty slots (no drum tab in ribbon) are simply omitted.
    static juce::File kitsDir();         // <Documents>/BaySickDAW/Kits
    static juce::File userKitsDir();     // <kitsDir>/My Kits
    static juce::File factoryKitsDir();  // <kitsDir>/Factory
    void showKitMenu (juce::Component* anchor);    // popup with Save/Load
    void saveKitAs ();                              // prompt + write XML
    void loadKit   (const juce::File& kitXml);      // entry: confirm + dispatch
    void loadKitImpl (const juce::File& kitXml);    // actual tear-down + rebuild
    // 2026-04-26: Global Lock/Unlock prompt + cross-slot toggle.
    void showGlobalLockPrompt ();
    void applyGlobalLockToggle ();
    // 2026-04-26: Templates (kit + 8 layers + 4 basses bundle).
    static juce::File templatesDir();         // <Documents>/BaySickDAW/Templates
    static juce::File factoryTemplatesDir();  // <templatesDir>/Factory
    static juce::File userTemplatesDir();     // <templatesDir>/My Templates
    void showTemplateMenu (juce::Component* anchor);  // popup with Save/Load
    void saveTemplateAs ();                            // prompt + write XML
    void loadTemplate (const juce::File& templateXml); // tear down + rebuild
    // Helper used by loadTemplate to instantiate a layer/bass tab and apply a preset.
    juce::Component* spawnLayerTabFromTemplate (const juce::String& engine,
                                                 const juce::File& presetFile,
                                                 bool locked);
    juce::Component* spawnBassTabFromTemplate  (const juce::String& engine,
                                                 const juce::File& presetFile,
                                                 bool locked);
    std::unique_ptr<juce::FileChooser> mTemplateChooser;   // for Set Default Template

    // ── Members ───────────────────────────────────────────────────────────────
    VibeSynthProcessor&            mProcessor;
    StandalonePlayHead&            mPlayHead;
    juce::AudioDeviceManager&      mDeviceManager;
    juce::AudioIODeviceCallback*   mAudioCallback { nullptr }; // set by StandaloneApp

    // Single pattern manager shared by all pages
    std::unique_ptr<PatternManager> mPM;

    // Project persistence (P1+ - 2026-04-23).  Owns the "current project"
    // folder on disk, serializes/deserializes via VibeSynthProcessor.
    std::unique_ptr<ProjectManager> mProjectManager;

    // ── File menu handlers (P2) ───────────────────────────────────────────────
    // Shown asynchronously via AlertWindow; all take the user through
    // project-name validation and recover gracefully on cancel.
    void doFileNew();
    void doFileNewFromTemplate();
    void doFileSetDefaultTemplate();
    void doFileOpen();
    void doFileSave();
    void doFileSaveAs();
    void doFileRestoreBackup();
    // Prompts the user for a project name.  Shown the first time they try to
    // save (explicit Save or - later in P4 - a Builder audio drop) when no
    // project folder exists yet.  Returns true if a project was created.
    bool promptCreateProject (const juce::String& reasonExplanation = {});
    // Update the DocumentWindow title to show the current project name + dirty marker.
    void refreshWindowTitle();

    // P5: if project is dirty, show Save/Don't Save/Cancel confirmation and
    // call continuation on Save (after successful save) or Don't Save.  If
    // not dirty, calls continuation immediately.  Cancel aborts.
    void confirmDiscardChanges (std::function<void()> continuation);
public:
    // P5: called from VibeSynthWindow::closeButtonPressed to intercept the
    // close-with-unsaved flow.  Returns true if quit should proceed
    // synchronously; returns false when a dialog is shown and the app should
    // wait for the user's choice (continuation calls quit() on accept).
    bool requestAppQuit();
private:

    // Undo manager - owned here, passed by pointer to sub-systems
    juce::UndoManager mUndoManager;           // initialised in ctor with default 100 steps
    int               mUndoHistorySize { 100 };  // mirrors current max; JUCE has no getter

    // Parallel history label list (juce::UndoManager doesn't expose its list)
    std::deque<juce::String>           mHistoryLabels;
    int                                mHistoryCursor { 0 };
    std::unique_ptr<UndoHistoryWindow> mHistoryWindow;

    // ── UI layers (top to bottom) ─────────────────────────────────────────────
    // Menu bar (JUCE component, sits at very top)
    std::unique_ptr<juce::MenuBarComponent> mMenuBar;

    // Header row (title + pattern selector + level dials)
    std::unique_ptr<juce::Label>      mTitleLabel;
    // Pattern dropdown button - replaces old ComboBox + Add + hidden TextEditor
    std::unique_ptr<juce::TextButton> mPatternBtn;

    // Transport bar, ribbon, and page menu bar
    std::unique_ptr<GlobalTransportBar> mTransport;
    std::unique_ptr<RibbonTabBar>       mRibbon;
    std::unique_ptr<PageMenuBar>        mPageMenuBar;

    // Page entries: parallel to ribbon tabs
    juce::OwnedArray<PageEntry> mPages;

    // Pointers to single-instance legacy pages (no duplication)
    // These are owned inside mPages[...].component
    LayersPage* mLegacyLayersPage  { nullptr };
    BassPage*   mLegacyBassPage    { nullptr };
    class DrumPage* mLegacyDrumPage { nullptr }; // D1.4 dynamic-drum model (last-created, parallel to LegacyLayers)
    BuilderPage* mBuilderPage      { nullptr };
    MixerPage*  mMixerPage         { nullptr };

    // Tracks which Layers page indices (0–kMaxLayerPages-1) are currently in use.
    // Lets createLayersPage() assign the first free slot and onTabClosed() release it.
    std::array<bool, 8> mUsedLayerIndices {};

    // Tracks which Bass page indices (0–kMaxBassPages-1) are currently in use.
    std::array<bool, kMaxBassPages> mUsedBassIndices {};

    // D1.4: tracks which Drum page indices (0..kMaxDrumPages-1) are in use.
    std::array<bool, kMaxDrumPages> mUsedDrumIndices {};

    // QA-D STATE-02: monotonic tab-name counters.  Unlike the slot-index
    // trackers above (which reuse freed slots), these increment monotonically
    // across the lifetime of a project so the user-visible numbering never
    // reuses a deleted number ("delete Layer 2, add new" -> "Layer 4", not
    // "Layer 2 again").  resetProjectState() zeroes them all back to 1 when
    // closeAllDynamicTabs runs at project teardown;
    // advanceCountersFromRestoredTabs() advances each past
    // max(parsed-name-suffix) after deserializeUIState restores saved tabs,
    // so the next +Add doesn't collide with a restored tab number.
    int mNextLayerNameNum   { 1 };
    int mNextBassNameNum    { 1 };
    int mNextDrumNameNum    { 1 };
    int mNextVoxNameNum     { 1 };
    int mNextInstNameNum    { 1 };
    int mNextGuitarNameNum  { 1 };
    int mNextBassesNameNum  { 1 };
    int mNextClipNameNum    { 1 };

    // Helper accessors: each increments its counter and returns the next
    // user-visible tab-name string (singular form, 1-based per QA-D Sub-A/B).
    // "Basses" (plural) for the BaySickBasses-source Inst tabs disambiguates
    // from "Bass" Bass-slot tabs per QA-D Sub-D.
    juce::String nextLayerTabName()   { return "Layer "  + juce::String (mNextLayerNameNum++); }
    juce::String nextBassTabName()    { return "Bass "   + juce::String (mNextBassNameNum++); }
    juce::String nextDrumTabName()    { return "Drum "   + juce::String (mNextDrumNameNum++); }
    juce::String nextVoxTabName()     { return "Vox "    + juce::String (mNextVoxNameNum++); }
    juce::String nextInstTabName()    { return "Inst "   + juce::String (mNextInstNameNum++); }
    juce::String nextGuitarTabName()  { return "Guitar " + juce::String (mNextGuitarNameNum++); }
    juce::String nextBassesTabName()  { return "Basses " + juce::String (mNextBassesNameNum++); }
    juce::String nextClipTabName()    { return "Clip "   + juce::String (mNextClipNameNum++); }

    // QA-D STATE-02 lifecycle hooks (definitions in StandaloneEditor.cpp).
    // resetProjectState: called from closeAllDynamicTabs after the existing
    // teardown loop -- zeroes all 8 counters back to 1.
    // advanceCountersFromRestoredTabs: called from end of deserializeUIState
    // -- scans mPages for restored tabs, parses trailing numbers from each
    // tab's display name, sets each counter to max(found-numbers) + 1.
    void resetProjectState();
    void advanceCountersFromRestoredTabs();

    // Currently visible page component
    juce::Component* mVisiblePage { nullptr };

    // (mHasDrumsTab removed - Drums is now a permanent slot in the ribbon)

    // ── Effects infrastructure ────────────────────────────────────────────────
    TrackSelectionManager          mTrackSel;
    EffectsPage*                   mEffectsPage { nullptr }; // owned in mPages
    // 2026-04-26 (step 2): unified Piano Roll page raw ptr (owned in mPages
    // at id=4).  Captured during buildDefaultTabs so engine create / delete
    // paths can call registerEngine / unregisterEngine / setEngineDisplayName.
    class PianoRollPage*           mPianoRollPage { nullptr };

    // Last channel that requested the Effects tab via FX button.
    // Phase 1E will use this to pre-select the matching dropdown entry.
    juce::String mLastFXChannel { "Master" };

    // 2026-04-26 (D-5): recording precount.  When `mPrecountEnabled && mRecordArmed`
    // at Play-press, transport plays a 1-bar (4-beat) lead-in click before
    // recording engages on bar 1.  Disabled by default; toggle via Ctrl+P or
    // the metronome panel's Precount checkbox.
    bool   mPrecountEnabled { false };
    double mCountInPendingBpm { 120.0 };

    // R5b (2026-04-23): Record button arms recording; capture only begins
    // when the user presses Play.  startPlayback() checks this flag and
    // routes to mProcessor.startRecording on the way through.  stopPlayback()
    // commits any active recording.
    bool mRecordArmed       { false };
    bool mRecordingActive   { false };   // true between actual capture start + stop

    // QA-Ea Task 0c (2026-05-20): mSlipEditMode + isSlipEditModeOn() removed.
    // Replaced by ArrangementGrid::mEditMode (Slip / Stretch dropdown).
    // The 'S' command in the ApplicationCommandManager
    // (cmdToggleSlipStretchMode) now calls into BuilderPage to toggle the
    // dropdown directly; no editor-level state needed.

    // R5d-midi (2026-04-24): last-accessed piano roll tracking.  Updated
    // whenever the user switches to a Layers / Bass / Drums tab.  Used in
    // MIDI record mode so captured notes land in whichever roll the user
    // was last editing.  Kind::None means the user hasn't touched a piano
    // roll yet this session - MIDI Record + Play blocks with "No Active
    // Piano Roll Selected" alert.
    enum class LastRollKind { None, Layer, Bass, Drums };
    LastRollKind mLastRollKind  { LastRollKind::None };
    int          mLastRollIndex { -1 };   // page index for Layer/Bass; unused for Drums

    // R5d (2026-04-24): post-stop routing.  Drops any captured WAVs onto
    // the next free arrangement row (at the beat where Play was pressed),
    // and forwards any captured MIDI notes to the last-accessed piano
    // roll (R5d-midi).  Fire-and-forget - safe to call with an empty
    // result, so Pause / Stop / Record-disarm all use the same path.
    void commitRecordingResult (const struct VibeSynthProcessor::RecordResult& res);

    // R5d follow-up (2026-04-24): after any project load / backup restore,
    // scan the arrangement for ClipType::Audio blocks and recreate the mixer
    // strip + InsertNode + routing edges for each row.  Without this, loaded
    // projects with audio clips play silently (no routing) + have no mixer
    // strip for the row.  Safe to call repeatedly; ensureAudioInsert +
    // addAudioChannel are both idempotent.
    // QA-E Task 8 NIT-2 (QA-D carry-forward): isLoadContext defaults true so
    // the 5 existing (all load-path) callers are unchanged; a future non-load
    // caller passes false to skip the end-of-restore clearDirty (defensive
    // guard so it can never wrongly clear a user's unsaved-edit state).
    void restoreAudioStripsFromArrangement (bool isLoadContext = true);

    // Single-shot timer that fires when the count-in period ends and starts the transport
    struct CountInTimer : public juce::Timer {
        StandaloneEditor& owner;
        explicit CountInTimer(StandaloneEditor& o) : owner(o) {}
        void timerCallback() override {
            stopTimer();
            owner.mProcessor.mMetro.countInActive.store(false, std::memory_order_relaxed);
            owner.mPlayHead.start(owner.mCountInPendingBpm);
        }
    } mCountInTimer { *this };

    // Global tooltip window (styled via VibeLAF::drawTooltip / getTooltipBounds)
    std::unique_ptr<VibeTooltip> mTooltipWindow;
    GlobalAutoRightClick         mAutoRightClick;  // global right-click → automate

    // ── Keymap framework (Phase A - 2026-04-26) ───────────────────────────────
    // ApplicationCommandManager owns the KeyPressMappingSet, dispatches keys to
    // perform() via this target.  Help > Key Binds opens KeyBindsWindow which
    // edits the same mapping set live; saves to keymap.xml on every change.
    juce::ApplicationCommandManager        mCmdMgr;
    // Holds the popup (DocumentWindow → Component).  Concrete KeyBindsWindow
    // type only known in the .cpp - using base Component avoids needing the
    // full include here.
    juce::Component::SafePointer<juce::Component> mKeyBindsWin;
    void showKeyBindsWindow();
    juce::Component::SafePointer<juce::Component> mRustyDrumsMapWin;  // J-7b
    void showRustyDrumsMapWindow();

    // J-6 (2026-05-03): BaySickRustyDrums singleton tab spawn.  Triggered by
    // the "+ Add BaySickRustyDrums" entry in the Drums▾ ribbon dropdown.
    // No-op when the singleton already exists (1-instance lock).
    void addBaySickRustyDrumsTab();

    // ── G-2 (2026-04-28): Clips ribbon tab.  Empty-state placeholder shown
    //    when the user clicks the Clip ribbon slot before any clips have
    //    been imported; same component is also a FileDragAndDropTarget so
    //    audio files dropped here route through the existing Builder import
    //    flow (which fires onAudioClipAdded → spawns the Clips tab).
    std::unique_ptr<ClipsEmptyState> mClipsEmptyState;
    void showClipsEmptyState();
    // Idempotent - does nothing if a Clips tab already exists for `path`.
    // pageIdx = audioRow (1:1 with mixer_audio_<row>), so the engine output
    // routes through that same audio insert.  Called from the onAudioClipAdded
    // hook (Builder drop) and from the empty-state drop handler (which routes
    // through Builder's importAudioFile path internally).
    // G-6 (2026-04-29): allowDuplicate=true skips the "is there already a
    // ClipsPage for this file?" dedup check.  Used by the picker's Duplicate
    // flow to spawn a second ClipsPage on the same WAV at a different audioRow.
    // Default false preserves drag-drop import semantics (one page per file).
    void spawnClipsTabIfMissing (int audioRow, const juce::String& path,
                                 bool allowDuplicate = false);
    // G-3 (2026-04-28): wires a Clips tab into the unified PianoRollPage so
    // its piano roll appears in the engine dropdown alongside layers / bass /
    // drums.  The PianoRollConnection's dataAccessor closure points at
    // Pattern::clipRoll[idx] so pattern switches stay live.
    void registerClipPianoRoll (int idx, ClipsPage* cp);

    // K-3 (2026-05-05): wires a sfizz-source Inst page (BaySickGuitars /
    // BaySickBasses) into the unified PianoRollPage.  dataAccessor points at
    // Pattern::instRoll[idx]; audition closures route to the engine's
    // auditionNote.  Caller (K-4 addBaySickGuitarsTab / L-3 addBaySickBassesTab)
    // invokes after the engine is created via VibeSynthProcessor::loadKit.
    void registerInstSourcePianoRoll (class InstPage* ip);
    void unregisterInstSourcePianoRoll (class InstPage* ip);

    // K-4 (2026-05-05): triggered by the "+ Add BaySickGuitars" entry on the
    // Inst▾ ribbon dropdown.  Finds the lowest free Inst slot, spawns the
    // mixer strip + InstPage, switches the page's source to BaySickGuitars,
    // auto-loads the default kit (`Black&Green Guitars/Programs/01-green_keyswitch.sfz`),
    // splices the Guitars engine into the chain, hides arm/listen LEDs on
    // the strip, registers with PianoRollPage, and renames the tab "Guitar N".
    void addBaySickGuitarsTab();

    // L-3 (2026-05-05): same flow for BaySickBasses.  Default kit
    // `Black&Blue Basses/Programs/01-darkblack_keysw.sfz`; tab named "Bass N".
    void addBaySickBassesTab();

    // ── G-4 (2026-04-28): Vox + Inst empty-state placeholders + spawn flow.
    //    Mirrors the Clips structure exactly - pages spawn from the Mixer
    //    page's "Add Vox/Inst Strip" buttons (NOT drag/drop).  Engine
    //    register / unregister wires through mProcessor.registerVoxEngine /
    //    registerInstEngine for audio-thread routing.  No piano-roll
    //    registration - Vox + Inst are live-input / recorded-audio
    //    destinations, not MIDI-triggered engines.
    std::unique_ptr<VoxEmptyState>  mVoxEmptyState;
    std::unique_ptr<InstEmptyState> mInstEmptyState;
    void showVoxEmptyState  ();
    void showInstEmptyState ();
    // G-7 (2026-04-29): empty-state hamburger Load Page Preset support.
    // Installs a menu builder on the empty-state page menu bar so users can
    // restore a saved Page Preset which auto-spawns the appropriate tab.
    void installEmptyStatePagePresetMenu (PagePresetIO::PageKind kind);
    void spawnAndLoadFromEmptyState      (PagePresetIO::PageKind kind,
                                           const juce::File& presetFile);
    // G-6 (2026-04-29): right-click "Duplicate" on a ClipsPage's engine
    // picker spawns a new ClipsPage at the next free audio row bound to the
    // SAME WAV file as the source, then applies the source's full state
    // (engine + APVTS).  Distinct from the Builder browser tree's
    // Duplicate which COPIES the WAV first.
    void spawnDuplicateClipsTab (class ClipsPage* sourceCp);
    void spawnDuplicateVoxTab   (class VoxPage*   sourceVp);
    void spawnDuplicateInstTab  (class InstPage*  sourceIp);
    // G-6 (2026-04-29): selectAfter controls whether the new tab takes focus.
    // false = stay on current page (used by Mixer "Add Vox/Inst Strip" so the
    // user can add multiple without being yanked away on each add).  true =
    // navigate to the new tab (used by ribbon empty-state click + ribbon "+Add"
    // entry where the user explicitly asked for the page).
    void spawnVoxTabIfMissing  (int voxIdx,  bool selectAfter = true);
    void spawnInstTabIfMissing (int instIdx, bool selectAfter = true);

    // Phase B-1 helpers (page-switch commands).
    // Find the first tab of the given ribbon type and select it.  No-op when
    // no tab of that type exists.
    void selectFirstTabOfType (RibbonTabBar::TabType type);
    // F11 - switch to the last-used Layer/Bass/Drum tab and land on its
    // Piano Roll sub-tab.  Falls back to the first Layers tab + Piano Roll
    // when no piano roll has been visited yet this session.
    void showLastUsedPianoRoll();

    // Phase B-2 helpers (pattern navigation).
    void showRenamePatternDialog();   // F2
    void jumpToNextEmptyPattern();    // F3
    void createNewPattern();          // F4
    void cyclePattern (int delta);    // + / - (delta = ±1, wraps)
    bool isPatternEmpty (int idx) const;

    // I-3c (2026-05-02): MIDI Learn UI controller.  Owns the 30s learn timer
    // and the Escape-cancels keyboard hook.  Wires all VKnobAutomation
    // sOnMidi* callbacks in the StandaloneEditor constructor.
    std::unique_ptr<class MidiLearnUI> mMidiLearnUI;

    // ── Automation playback (Phase 4D) ────────────────────────────────────────
    // Maps paramId → function that applies a 0..1 normalised value on the message thread.
    // Populated automatically from APVTS params + registrations from panels/strips.
    std::map<juce::String, std::function<void(float)>> mAutomationApplicators;

    // Maps paramId → function that reads the current 0..1 value from the live control.
    // Used when creating automation blocks to seed the initial control points.
    std::map<juce::String, std::function<float()>>     mAutomationValueReaders;

    // QA-Ed (Problem 3): last beat at which automation was applied.  The 30 Hz
    // timer re-applies automation whenever the playhead beat changes -- playback
    // OR a stopped seek/scrub -- so any param on an active automation snaps to
    // the playhead position.  -1e9 = "not yet applied" so the first tick runs.
    double mLastAutomationBeat { -1.0e9 };
    void applyAutomationAtCurrentPosition();

    // 30 Hz timer that drives automation playback application
    struct AutomationTimer : public juce::Timer {
        StandaloneEditor& owner;
        explicit AutomationTimer(StandaloneEditor& o) : owner(o) {}
        void timerCallback() override { owner.applyAutomationAtCurrentPosition(); }
    } mAutomationTimer { *this };

    // ── Event Editor windows (Phase 4D) ───────────────────────────────────────
    // Owned list of open EventEditor windows. Windows remove themselves on close.
    juce::OwnedArray<EventEditor> mEventEditors;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StandaloneEditor)
};
