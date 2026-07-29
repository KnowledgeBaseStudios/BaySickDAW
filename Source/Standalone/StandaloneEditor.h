#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "StandaloneApp.h"
#include "SharedUI.h"
#include "GlobalTransportBar.h"
#include "RibbonTabBar.h"
#include "WorkspaceWindow.h"   // QA-ModelShell TS4: contained-window shell
#include "TrackSelectionManager.h"
#include "EffectsPage.h"
#include "UndoActions.h"
#include "UndoHistoryWindow.h"
#include "EventEditor.h"
#include "DrumPage.h"   // D2: KitDrumInfo struct used in helper signatures below
#include "PagePresetIO.h"   // G-7: PageKind enum used in helper signatures below
#include "HeavyOperationOverlay.h"

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
class VoxPage;          // 2026-04-28 (G-4: Vox engine page)
class InstPage;         // 2026-04-28 (G-4: Inst engine page)
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
                         public  juce::ApplicationCommandTarget,
                         public  juce::KeyListener
{
public:
    StandaloneEditor(VibeSynthProcessor& p, StandalonePlayHead& ph,
                     juce::AudioDeviceManager& dm);
    ~StandaloneEditor() override;

    // Busy-overlay lookup for page-level heavy ops (engine swaps, sample +
    // kit loads).  Static walk-up so pages don't carry an editor pointer;
    // returns null while the component isn't parented yet (project restore
    // runs selectEngine pre-parent -- the load overlay covers that path).
    static HeavyOperationOverlay* busyOverlayFor (juce::Component* c);

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

    // QA-F (2026-07-09): place a BaySickAlign render-to-bake on the Builder
    // grid (row below the originals + row-mute A/B, alignBake-marked).
    // QA-Fa recovery: DORMANT -- Render is export-only and the automatic
    // caller was retired; kept for future same-channel bake placement.
    void placeAlignedBake (const juce::File& bakeFile, double startBeat,
                           int followerChannelId);

    // QA-Fa recovery: "+ Add New Vox From Export" (Vox ribbon dropdown).
    // listVoxExportEntries returns the project's Aligned/ + Pitched/ wavs
    // (EMPTY when a grey rule holds: no exports / vox cap / unsaved
    // project); addVoxFromExport spawns a new Vox strip via the Mixer-add
    // cascade, places the export at its original timeline position (render-
    // history startBeat lookup; orphans land at beat 0), then prompts to
    // clone the source chain and to mute the source strip.
    std::vector<RibbonTabBar::VoxExportEntry> listVoxExportEntries();
    void addVoxFromExport (const juce::String& fullPath);
    void placeVoxExportClip (const juce::File& exportFile, double startBeat,
                             int routeChannel);
    int  findFreeVoxIndex() const;

    // QA-Fa (2026-07-10): BaySickPitch "Send Notes to..." (section 14b).
    // Targets = the open Layers / Bass / Drums / Clips tabs; only MIDI notes
    // travel (the detected contour quantized to notes), never audio.  The
    // contour arrives in seconds (first note = 0); placement converts to
    // beats at the current transport tempo and appends into the target
    // page's roll in the CURRENT pattern.
    struct PitchNoteTarget { int kind; int pageIndex; juce::String label; };
    struct ContourNote     { double startSec; double endSec; int midiNote; };
    std::vector<PitchNoteTarget> listPitchNoteTargets() const;   // kind: 0=Layer 1=Bass 2=Drum 3=Clips
    void sendPitchNotesToTab (int kind, int pageIndex,
                              const std::vector<ContourNote>& notes);

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
        // QA-ModelShell TS4: the contained window this page is framed in.
        // The page is still owned by `component` above -- the window hosts it
        // non-owningly -- so every existing reach through `component` keeps
        // working while the shell changes around it.
        std::unique_ptr<WorkspaceWindow> window;
        // The page's own index (Layer 2, Drum 5, ...).  Needed to REBUILD the
        // page after destroy-on-close, when the component that used to answer
        // getPageIndex() no longer exists.  -1 for the system pages.
        int pageIndexHint { -1 };
    };

    // Frame `entry`'s page in a contained window and attach it to the
    // workspace.  Called at every page-creation site in place of the old
    // addChildComponent.
    void hostPageInWindow (PageEntry& entry);
    // Stable per-logical-window key for bounds persistence (survives the
    // window object, which destroy-on-close makes short-lived).
    juce::String persistKeyFor (const PageEntry& entry) const;
    // Destroy-on-close: closing a window destroys the WINDOW AND THE PAGE, and
    // reopening rebuilds the page from the model.  The engine is untouched --
    // it is rig-owned since TS1 -- so audio and automation are unaffected.
    void closeWindowForTab (int tabId);
    // Recreate a destroyed page for `entry` and re-bind it to the rig's live
    // engine.  Returns false for the page types whose construction is still
    // entangled with mixer-strip spawning (see the .cpp note).
    bool rebuildPageForTab (PageEntry& entry);
    static bool canRebuildType (RibbonTabBar::TabType t);

    // ── Core helpers ──────────────────────────────────────────────────────────
    void buildDefaultTabs();     // called in ctor: Builder + initial Layers/Bass/Drums
    // QA-ProjectSave docket 18 (2026-07-26): addDefaultDynamicTabs /
    // addDefaultDrumTab removed.  Layers / Bass / Drums no longer get seeded
    // with one instance each -- they open empty and delete down to zero like
    // every other tab type, so a saved project or template never carries tabs
    // the user did not ask for.
    void onAddTabRequest(RibbonTabBar::TabType type);
    void applyEngineToNewestTabOfType (RibbonTabBar::TabType type,
                                       const juce::String& engine);
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
    // QA-ProjectSave Task 2 (2026-07-26): the structural half of the UI state,
    // shared by project save and template save.  A template IS this and nothing
    // more -- serializeUIState wraps it with the session extras (active tab,
    // scroll, selection, metronome, VU calibration, song-loop) that a project
    // skeleton has no business carrying.
    // QA-ProjectSave Task 3 (2026-07-26): teardown scoped to what the caller
    // will restore.  v2 templates + project load replace every tab type;
    // a v1 FACTORY template only restores Layers/Bass/Drums, so it must leave
    // Clips / Vox / Inst / Rusty alone rather than destroying work it cannot
    // put back.
    enum class TabTeardownScope { AllDynamic, LayersBassDrumsOnly };
    void closeDynamicTabs (TabTeardownScope scope);

    void serializeStructuralUIState  (juce::XmlElement& ui);
    void serializeTabsInto           (juce::XmlElement& tabs);
    // QA-ProjectSave Task 5 (2026-07-26, dockets 23/24): after a TEMPLATE's tab
    // walk, adopt every referenced file that is not already under a stable root
    // into My Samples and rewrite the reference to point there.  Project saves
    // do NOT call this -- a project has its own Samples folder, so its refs are
    // already portable with the folder; a template is one loose XML and has to
    // carry its dependencies somewhere durable.
    void adoptTemplateSampleRefs     (juce::XmlElement& tabs);
    void serializeStripNamesAndOrders(juce::XmlElement& ui);
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
    // File > New from Template submenu: files listed at menu-build time,
    // resolved by id at dispatch (rebuilt on every menu open).
    static constexpr int kTemplateMenuLoadBase = 900;
    juce::Array<juce::File> mTemplateMenuFiles;
    void saveTemplateAs ();                            // prompt + write XML
    void loadTemplate (const juce::File& templateXml); // dirty gate -> applyTemplate
    // The actual teardown + rebuild, past the save prompt.  Never call directly:
    // loadTemplate owns the unsaved-changes gate for every entry point.
    void applyTemplate (const juce::File& templateXml);
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
    void doFileSetDefaultTemplate();
    void doFileOpen();
    void doFileQuickOpen();
    void doFileSave();
    // QA-ProjectSave Task 12: optional completion fires after a SUCCESSFUL
    // save only -- Export Project Bundle chains the bundle through it.
    void doFileSaveAs (std::function<void()> onSaved = {});
    void doFileRestoreBackup();
    // QA-Export: settings dialog -> save chooser -> BuilderPage::runExportWithProgress.
    void doExportAudio();
    // QA-Export: zip-or-folder + scope dialog -> ProjectBundler.
    void doExportProjectBundle();
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
    // QA-TransportDisplay (2026-07-08): position readout overlays the bar
    // between the pattern button and the ribbon (same overlay pattern).
    std::unique_ptr<TransportPositionReadout> mPosReadout;
    std::unique_ptr<RibbonTabBar>       mRibbon;
    // QA-ModelShell TS4 (locked call 4a): the page menu is no longer ONE bar
    // under the transport -- each window owns its own inside its title strip.
    // This points at the ACTIVE window's bar, so the ~77 existing
    // `mPageMenuBar->...` configuration sites keep working verbatim.
    //
    // mDetachedPageMenu is a null object, never shown: it keeps the pointer
    // valid when no window is active.  Only 2 of those 77 sites null-check, so
    // a nullable pointer here would be a crash surface, not a design choice.
    PageMenuBar*                        mPageMenuBar { nullptr };
    std::unique_ptr<PageMenuBar>        mDetachedPageMenu;

    // ── D-4 typing-keyboard MIDI (QA-TransportDisplay 2026-07-08) ────────────
    // Editor owns the mode: the bar button and Ctrl+T both route through
    // toggleTypingKeyboard().  Held notes tracked as (keyCode, midiNote) pairs
    // so key-up sends the matching noteOff even across an octave shift.
    bool mTypingKeyboardOn { false };
    int  mTypingOctaveOffset { 0 };                    // PgUp/PgDn steps, clamped [-5..+3]
    std::vector<std::pair<int,int>> mTypingHeldNotes;  // keyCode -> sounding MIDI note
    void toggleTypingKeyboard();
    void sendTypingNote (int midiNote, bool noteOn);
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    // G1 smoke item-12 fix: KeyListener overloads forwarding to the two
    // handlers above.  The editor registers ITSELF as a key listener AFTER
    // the command map's KeyPressMappingSet - JUCE dispatches a component's
    // key listeners in REVERSE registration order, before the component's
    // own keyPressed (see the ctor comment) - so while typing-keyboard mode
    // is on, note keys are consumed BEFORE command dispatch can fire the
    // colliding letter bindings (R = record, bare S = Slip/Stretch, plus
    // anything a stale keymap.xml overlays).  Mode off: both return false
    // instantly and commands work.
    bool keyPressed (const juce::KeyPress& k, juce::Component*) override
    {
        return keyPressed (k);
    }
    bool keyStateChanged (bool isKeyDown, juce::Component*) override
    {
        return keyStateChanged (isKeyDown);
    }

public:
    // G1 review fix: public so VibeSynthWindow can flush held typed notes on
    // app deactivation (Alt+Tab mid-hold left the key-up with another app and
    // the note droned until refocus).  Idempotent; message thread.
    void releaseAllTypingNotes();

private:

    // QA-TransportDisplay: readout display-mode persistence (settings.xml,
    // same parse-preserve-siblings pattern as the MT pref).
    void loadTransportDisplayPref();
    void saveTransportDisplayPref (bool showTime);

    // QA-TempoMap: rebuild the playhead's tempo timeline from the ruler
    // tempo flags (bar-authored; beat = bar * 4 per the uniform playback grid).
    void pushTempoMarkersToPlayHead();

    // Page entries: parallel to ribbon tabs
    // QA-ModelShell TS4: the region contained windows live in.  Declared BEFORE
    // mPages ON PURPOSE.  Members destruct in REVERSE declaration order, so
    // being declared FIRST means being destroyed LAST -- which is what we need,
    // because every WorkspaceWindow calls mWorkspace->removeWindow(this) from
    // its destructor.  The original order had this backwards (comment claimed
    // one thing, C++ did the other): the Workspace died first and each window
    // teardown then wrote into freed memory.  Jeff hit it as an access
    // violation reading -1 inside Workspace's window array, 2026-07-28.
    std::unique_ptr<Workspace> mWorkspace;

    // False for the duration of the constructor.  While false, hostPageInWindow
    // frames ONLY the windows that are meant to be on screen at launch; every
    // other page waits until its tab is first selected.  Set true just before
    // the startup tab selection, so that selection frames normally.
    bool mStartupComplete { false };

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

    // Heavy-op progress overlay (project load / restore / heavy engine ops).
    // Always-on-top child covering the whole editor; the synchronous ops pump
    // paints through it at step boundaries (see HeavyOperationOverlay.h).
    HeavyOperationOverlay mHeavyOpOverlay;

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

    // ── QA-Fe2 De-noise (2026-07-16) ─────────────────────────────────────
    // Take types written at record stop = File Settings checkboxes UNION the
    // strip's Builder Grid Default (the arm-LED right-click picker section --
    // replaced the arm popup, Jeff's Task-5 call).  -1 = auto rule (Wet when
    // realtime correction is on, else Dry); a user pick LOCKS until the
    // project closes (reset on project load, NOT on track reassignment).
    enum TakeType { kTakeDry = 0, kTakeDryCleaned = 1, kTakeWet = 2, kTakeWetCleaned = 3 };
    struct FileTakeSettings { bool dry, dryCleaned, wet, wetCleaned; int strength; };
    FileTakeSettings readFileTakeSettings() const;
    void showFileSettingsDialog();
    void pollDenoiseState();
    bool regenerateDenoise (const juce::String& relPath, int strength);
    bool renameRecordingGroup (const juce::String& oldBase, const juce::String& newBase);

    static constexpr int kDenoiseMaxVox = 6;    // == MixerChannelIds::kMaxVoxStrips (6)
    std::array<int,   kDenoiseMaxVox> mVoxTakePick;       // ctor-filled -1
    std::array<int,   kDenoiseMaxVox> mVoxInputIdxLast;   // ctor-filled -999
    // QA-Fe2 PDC full-graph pass: last host-reported total; the poll re-solves
    // the whole graph each tick and refreshes the report only on change.
    int mPdcTotalLast { -1 };

    struct DenoisePollTimer : public juce::Timer {
        StandaloneEditor& owner;
        explicit DenoisePollTimer(StandaloneEditor& o) : owner(o) {}
        void timerCallback() override { owner.pollDenoiseState(); }
    } mDenoisePollTimer { *this };

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
            // G1 review fix: Play never edits tempo; the count-in CLICK rate
            // still uses the field value via mMetro.countInBpm above.
            owner.mPlayHead.start();
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
    // QA-ClipDrop Task 3 (SC-G/H, 2026-06-03): create the mixer strip + Clips
    // page for an audio clip at `row`, named from the SAMPLE (not the Builder
    // row label "Track N").  Shared by drag-drop (onAudioClipAdded),
    // "+ Add New Clip" (onAddTabRequest), and project reload.  The strip trio
    // (addAudioRowChannel + ensureAudioInsert + addAudioChannel) is idempotent,
    // so repeat calls at an existing row are safe no-ops.
    // allowDuplicate (QA-EffectsReview side-fix 2026-06-06): forwarded to
    // spawnClipsTabIfMissing so the "New Page"/Duplicate flows route through this
    // canonical helper too -- they used to call spawnClipsTabIfMissing directly
    // and skip the strip trio, yielding a page with no mixer strip.
    void createClipStripAndPage (int row, const juce::String& path, bool allowDuplicate = false);
    // QA-ClipDrop Task 3 (SC-G/J, 2026-06-03): "+ Add New Clip" handler -- copy
    // the picked file into the project (prompting to create one first if there
    // is none, then retrying), register it in the audio library, and spawn its
    // Clips page + strip with NO grid block.
    void addClipPageFromFile (const juce::File& src);
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
    // QA-ProjectSave docket 18 (2026-07-26): Layers / Bass / Drums reach zero
    // now, so they need the same placeholder the other three types have had
    // since G-2/G-4.  One shared component class rather than three near-identical
    // ones -- they differ only in accent colour and prompt text.
    // Six empty states now, so each show* helper hides the other five through
    // here rather than naming them one by one.
    // G-7 (2026-04-29): empty-state hamburger Load Page Preset support.
    // Installs a menu builder on the empty-state page menu bar so users can
    // restore a saved Page Preset which auto-spawns the appropriate tab.
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

    // Session memory of the last Rusty kit (LIFE-02): captured on every kit
    // load + at tab close; re-adding the Rusty tab auto-reloads it.  Not
    // persisted -- project restore carries its own kitPath record.
    juce::File mLastRustyKitFile;

    // ── Automation playback (Phase 4D) ────────────────────────────────────────
    // Maps paramId → function that applies a 0..1 normalised value on the message thread.
    // Populated automatically from APVTS params + registrations from panels/strips.
    std::map<juce::String, std::function<void(float)>> mAutomationApplicators;

    // Maps paramId → function that reads the current 0..1 value from the live control.
    // Used when creating automation blocks to seed the initial control points.
    std::map<juce::String, std::function<float()>>     mAutomationValueReaders;

    // Statics = every APVTS param (plus the derived "<prefix>_fader" aliases)
    // + "global_tempo".  Called from the ctor, after resetProjectState()'s full
    // map clear, and from VibeSynthProcessor::onMixerStripParamsCreated -- a
    // lazily-created strip's params did not exist at the earlier sweeps, and
    // param materialization is the model event that says they do now.
    void registerStaticAutomationHandlers();

    // QA-ModelShell TS1: engine-parameter lane registration keyed to the
    // rig's onEngineCreated model event (fires on pick / restore / template /
    // duplicate).  Null-owner closures resolving through the rig at apply
    // time; TS3 retires the view wrappers these coexist with.
    void registerModelEngineAutomation (struct EngineTab& tab);

    // QA-ModelShell TS3: the BaySickPedals board's per-slot lanes.  Same shape
    // as the FX rack's registerSlotAutomationFor -- resolve board -> slot BY
    // UUID -> DSP at apply time, null owner -- but the board is not an
    // EffectRack on a graph channel, so it needs its own resolver.  Re-run on
    // every slot load/clear/restore via BaySickPedalsProcessor::
    // onSlotAutomationChanged, and once at Inst-tab creation.
    void registerPedalAutomation (int instPageIndex);

    // BLU-344 (QA-ModelShell TS3): the Harmless mod editor's DEPTH and LENGTH.
    // These are the batch's only automation targets that are neither an APVTS
    // parameter nor a rack DSP -- they are fields on HarmlessModRegistry,
    // addressed by (target paramId, source).  Ids are
    // "<targetParamId>_mod<sourceIdx>_depth" / "_length"; no APVTS param can
    // collide with that shape.  Registered for every (target x source) the
    // editor can reach, so a lane works whichever pair the user was on when
    // they created it.
    void registerHarmlessModAutomation (TabKind kind, int pageIndex);

    // QA-ModelShell TS3 fix (2026-07-28): the sfizz trio's lanes.  These
    // engines are processor-owned, not rig-owned, so TS1's model hook never
    // saw them -- and because the Aria panel already offers "Automate: ..." on
    // every kit CC, that gap meant a right-click produced a lane that applied
    // to nothing.  Registers the engine's whole parameter list (outVol, the
    // CC bank, cut-self pair) with applicators that re-resolve the engine
    // through the processor at apply time, so a kit swap or a
    // destroy/recreate cycle cannot strand them.
    void registerSfizzEngineAutomation (VibeSynthProcessor::SfizzEngineKind kind,
                                        int instIdx);

    // ── Applicator lifetime (QA-ModelShell TS3, 2026-07-27) ─────────────────
    // There is none to track any more.  QA-ProjectSave Task 7 built an
    // owning-Component index here because a lane's applicator drove a CONTROL,
    // so the registry had to notice when that control died -- first via a
    // hand-written list of ~17 key prefixes, then via ComponentListener when the
    // list proved unable to see a control that died without its tab closing.
    //
    // TS3 removes the premise instead of the symptom: every registration is now
    // made by the model (engine creation, rack slot, pedal slot, param
    // materialization) and re-resolves its target through the model at APPLY
    // time.  Nothing is keyed to a view, so nothing has to be revoked when a
    // view dies -- which is what makes destroy-on-close windows safe.  This
    // matches the `/architecture` finding (Research Reports, 2026-07-26) that no
    // DAW or plugin framework keeps a UI-keyed applicator map at all.
    //
    // One complaint per paramId per session.  The dispatch runs at the
    // automation tick rate, so an unguarded warning would be a flood.
    juce::StringArray mReportedDeadLanes;

    // Nav: land on the Effects page with the given strip selected -- the same
    // three-line handoff the mixer strips' FX buttons use (mLastFXChannel is
    // consumed by onTabSelected's Effects branch).
    void jumpToFxRackForPrefix (const juce::String& mixerPrefix);
    // Piano-roll menu-bar nav: both derive their target from the roll
    // dropdown's active EngineId.
    void jumpToRollPlayerPage();
    void jumpToRollFxRack();

    // QA-Ed (Problem 3): last beat at which automation was applied.  The 30 Hz
    // timer re-applies automation whenever the playhead beat changes -- playback
    // OR a stopped seek/scrub -- so any param on an active automation snaps to
    // the playhead position.  -1e9 = "not yet applied" so the first tick runs.
    double mLastAutomationBeat { -1.0e9 };
    void applyAutomationAtCurrentPosition();

    // Smoke round 3 (Jeff): applicator-lane (engine params + global_tempo)
    // pre-automation baseline -- the non-APVTS twin of the processor's
    // mAutomationBaseline.  Captured via mAutomationValueReaders at song
    // ENTRY, restored via mAutomationApplicators at EXIT.  Message thread.
    std::vector<std::pair<juce::String, float>> mApplicatorBaseline;

    // 30 Hz timer that drives automation playback application
    struct AutomationTimer : public juce::Timer {
        StandaloneEditor& owner;
        explicit AutomationTimer(StandaloneEditor& o) : owner(o) {}
        void timerCallback() override { owner.applyAutomationAtCurrentPosition(); }
    } mAutomationTimer { *this };

    // Pattern-dropdown label sync.  The current pattern changes from places
    // that can't reach refreshPatternBox directly (Builder browser/grid
    // selection, project load restoring currentPattern), so poll cheaply --
    // refreshPatternBox only touches the button on an actual label change.
    // Same polling idiom as tab detection (no change-listener available).
    struct PatternLabelTimer : public juce::Timer {
        StandaloneEditor& owner;
        explicit PatternLabelTimer(StandaloneEditor& o) : owner(o) {}
        void timerCallback() override { owner.refreshPatternBox(); }
    } mPatternLabelTimer { *this };

    // ── Event Editor windows (Phase 4D) ───────────────────────────────────────
    // Owned list of open EventEditor windows. Windows remove themselves on close.
    juce::OwnedArray<EventEditor> mEventEditors;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StandaloneEditor)
};
