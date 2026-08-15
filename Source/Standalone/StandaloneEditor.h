#pragma once
#include <JuceHeader.h>
#include <optional>   // defaultSizeFor returns nothing when an engine is unbound
#include <set>        // T21 tether stores (closed-by-hand / unlocked visual keys)
#include <tuple>      // failed pattern-fill keys (kind, page, pattern)
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
#include "VersionCapture.h"   // QA-ModelShell TS7 §3: per-playback-pass capture

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
//   - Workspace         (every page lives in its own contained window; all
//                        windows are live at once, selecting a tab fronts one)
//
// Ribbon tab layout:
//   Required tabs (always present): Builder | Mixer | Effects | Piano Roll
//   Instance types (shown once one exists; the "+" slot adds one by picking an
//     engine): Clip | Vox | Inst | Layers | Bass | Drums | Plugins
// ─────────────────────────────────────────────────────────────────────────────
class StandaloneEditor : public  juce::Component,
                         public  juce::MenuBarModel,
                         public  juce::ApplicationCommandTarget,
                         public  juce::KeyListener,
                         public  juce::ChangeListener
{
public:
    StandaloneEditor(BaySickDAWProcessor& p, StandalonePlayHead& ph,
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

    // Public access for audio thread / other components
    PatternManager& getPatternManager();

    // ── Undo dispatch (call instead of UndoManager directly) ─────────────────
    // Takes ownership of action and forwards to the manager under a named
    // transaction ("<owner>|<label>" when the owner key is tab-scoped).  The
    // history-label list is rebuilt from the manager itself via ChangeListener
    // (QA-UndoCoverage Task 3) -- write-side label maintenance is gone.
    bool doUndoAction(juce::UndoableAction* action, const juce::String& label,
                      const juce::String& ownerKey = "app");
    void globalUndo();
    void globalRedo();
    void showHistoryWindow();

    // QA-UndoCoverage Task 3: fires on every UndoManager change (perform /
    // undo / redo / clear) -- rebuilds the history labels from the real stack.
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    // How long a file of `fileSeconds` is in beats when placed at `startBeat`,
    // and the tempo in force THERE.  Every path that puts a WAV on the Builder
    // grid resolves through this: a take recorded across a tempo change is
    // otherwise sized (and stretch-stamped) at whatever the BPM field happened
    // to read, so the same audio lands at two different lengths depending on
    // which door it came in.  Message thread.
    struct ClipTiming { double lengthBeats { 0.0 }; double bpmAtStart { 120.0 }; };
    ClipTiming clipTimingFor (double fileSeconds, double startBeat) const;

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
    // ownerKey tags the page's transactions in the history window (labels
    // only); pass a tab-scoped key ("lay0", "vox1", ...) where the creation
    // site knows it, "app" for global surfaces.
    UndoContext makeUndoContext(const juce::String& ownerKey = "app");

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

    // The 0..1 value the Event Editor's / Builder grid's "Reset to Default"
    // restores an automation point to (Jeff's ruling: a reset returns the point
    // to the parameter's value from before automation touched it, not a
    // hardcoded midpoint).  There is no durable pre-automation stash to read --
    // the two song-mode baselines only exist while song mode is on -- so this
    // resolves the parameter's own default instead, and falls back to the live
    // control for the lane classes that have no default to read.  Wired into
    // both grids as onResolveResetValue.
    float automationResetValue (const juce::String& paramId) const;

    // ── "Most recent" navigation targets ─────────────────────────────────────
    // Session state on purpose: these answer "take me back to where I was",
    // which is a property of this run, not of the project (the piano roll's own
    // engine choice is the one that persists, and it keeps doing that itself).
    //
    // Each reports NONE -- nullptr / -1 -- both before anything qualifies and
    // after the remembered target has been closed: all three resolve through
    // their live registry on read, so none can hand back a stale id or a
    // dangling window.
    WorkspaceWindow* getMostRecentEffectPanel()    const;
    EventEditor*     getMostRecentEventEditor()    const;
    int              getMostRecentPlayerTabId()    const;

    // ── MenuBarModel ──────────────────────────────────────────────────────────
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu   getMenuForIndex(int menuIndex, const juce::String&) override;
    void              menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    // ── ApplicationCommandTarget (Phase A - keymap framework) ────────────────
    juce::ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
    void getAllCommands  (juce::Array<juce::CommandID>& out) override;
    void getCommandInfo  (juce::CommandID id, juce::ApplicationCommandInfo& info) override;
    bool perform         (const InvocationInfo& info) override;

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
    // QA-Layout T7: per-window floor from Jeff's approved sizing map
    // (full-window dims).  Engine-driven families key off the visible engine.
    //
    // Jeff, 2026-08-04: returns NOTHING when the engine has not bound yet.
    // There is deliberately no fallback number -- see the definition.  A caller
    // that gets nullopt installs NO floor rather than a placeholder one.
    std::optional<juce::Point<int>> defaultSizeFor (const PageEntry& entry) const;
    // Re-applies the floor for the window framing `page`, now that its engine
    // is known.  Wired to each engine-driven page's onEngineEditorRebuilt --
    // the only moment the answer exists for a window framed before its engine.
    void refreshWindowDefaultFor (const juce::Component* page);
    // Self-healing sweep for windows still carrying the ctor placeholder floor.
    // onEngineEditorRebuilt is a SINGLE callback slot, so an engine that bound
    // before showPageForTab installed the slot fired into nothing and that
    // window kept 320x200 forever -- intermittent by nature, and likelier on a
    // second player because the engine loads faster once warm (Jeff,
    // 2026-08-04).  A floor that is not yet known is a KNOWN-incomplete state,
    // so poll it instead of depending on catching one event.  Costs nothing
    // once resolved: a window with a real floor is skipped outright.
    void pollPendingWindowDefaults();
    // Stable per-logical-window key for bounds persistence (survives the
    // window object, which destroy-on-close makes short-lived).
    juce::String persistKeyFor (const PageEntry& entry) const;
    // QA-Layout T5: the page's own index resolved from its live component
    // (-1 for the system pages + the Rusty singleton).  One resolver serves
    // persistKeyFor, hostPageInWindow's hint fill, and the load-time reopen
    // matcher -- the old fill cascade covered only Layers/Bass/Drums, so
    // every Clip/Vox/Inst/Plugins window shared a "type:-1" key.
    int  pageIndexOfEntry (const PageEntry& entry) const;
    // Sweep every live window's bounds into the lifetime-1 map.  Resizes
    // have no end-of-gesture hook, so serialization flushes first (this is
    // L16's "timer flush" -- the autosave calls the serializer).
    void flushAllWindowBounds();
    // True while deserializeUIState runs: nothing frames mid-load; the
    // end-of-load pass frames exactly the windows the project saved as open.
    bool mLoadingWindows { false };
    // Window-only close: the WINDOW goes; the page component and the rig-owned
    // engine both stay alive, so audio and every automation lane are untouched.
    // The page may NOT be freed here -- the editor's cached raw page pointers
    // (mMixerPage and friends) are unguarded, so freeing it dangles all of them
    // (crash detail in the .cpp).
    void closeWindowForTab (int tabId);
    // Recreate a destroyed page for `entry` and re-bind it to the rig's live
    // engine.  Returns false for the page types whose construction is still
    // entangled with mixer-strip spawning (see the .cpp note).
    bool rebuildPageForTab (PageEntry& entry);
    static bool canRebuildType (RibbonTabBar::TabType t);

    // ── Satellite windows (QA-ModelShell TS5) ────────────────────────────────
    // Contained windows that are NOT ribbon tabs: the per-effect panels and the
    // two EQs the rack window opens.  Kept in their own registry because every
    // existing window path is keyed to a PageEntry, and these have no tab, no
    // page, and a lifetime driven by the rack's contents instead of the user's
    // navigation.  Keyed by an identity string ("fx:<chId>:<uuid>",
    // "eq:<chId>:pre") so a second request for the same thing raises the window
    // that already exists rather than building a duplicate.
    struct AuxWindow
    {
        juce::String                     key;
        std::unique_ptr<WorkspaceWindow> window;
    };
    // NOTE: the vector itself is declared next to mPages, AFTER mWorkspace --
    // members destruct in reverse declaration order, so anything holding
    // windows must come after the Workspace they are parented to or teardown
    // writes into freed memory (the 2026-07-28 crash).

    WorkspaceWindow* findAuxWindow (const juce::String& key) const;
    // Opens (or raises) a satellite window.  `persistKey` is separate from
    // `key`: identity follows the EFFECT (its uuid), while remembered position
    // follows the SLOT, so swapping the effect in slot 3 reuses slot 3's spot
    // instead of cascading a fresh window.
    WorkspaceWindow* openAuxWindow (const juce::String& key,
                                    const juce::String& persistKey,
                                    const juce::String& title,
                                    std::unique_ptr<juce::Component> content,
                                    int minW, int minH);
    void closeAuxWindow (const juce::String& key);
    void closeAllAuxWindows();

    void openEffectSlotWindow (int channelId, int slotIndex);
    void openEffectEqWindow   (int channelId, bool pre);
    // QA-Layout T17: one visual window PER SLOT (Jeff's call) so a compressor
    // and a limiter can be watched side by side.  Keyed by uuid like the effect
    // window, so it follows the effect through a reorder.
    //
    // T21 `userRequested`: true only for Menu > Visual.  Auto-open passes false,
    // and the difference is load-bearing -- asking for it by hand un-dismisses
    // it, arriving with the effect window must not.
    void openEffectVisualWindow (int channelId, const juce::String& slotUuid,
                                 bool userRequested = false);

    // ── T21 tether stores (project content; replaced on load, not merged) ─────
    // Visual keys the user closed BY HAND.  Auto-open respects this, or the two
    // features cancel: every effect-window open would drag the visual back and
    // the close would never stick.
    std::set<juce::String> mVisualUserClosed;
    // Visual keys whose tether the user UNLOCKED.  Stored as the exception
    // because locked is the default a pair opens in, so an absent entry -- and
    // therefore any older project -- restores tethered.
    std::set<juce::String> mVisualUnlockedKeys;

    // Aux key of the single-effect panel the user opened or raised last.  The
    // registry has no ordering of its own, so recency has to be recorded as it
    // happens; the key is resolved back through findAuxWindow on read, which is
    // what makes a closed panel report none.
    juce::String mLastEffectPanelKey;
    // Wired to EVERY aux window's onBroughtToFront.  The filtering lives here
    // rather than at the wiring site because the same registry also holds the
    // EQ, visual and analyzer windows, and only the effect panels are a
    // navigation target.
    void noteAuxWindowFronted (const juce::String& key);

    // QA-Layout T4 (Window-7): the Vox sub-page windows + the Inst
    // Pedals/NAM-IR windows.  Content hosts the PAGE-OWNED panel non-owned
    // through a per-tick resolver (the EffectWindows satellite discipline).
    // Persist keys are T5's type+instance scheme ("voxsat:<idx>:<kind>" /
    // "instsat:<idx>:<kind>"); stores stay Session until T5 wires the
    // three-lifetime model.
    enum class VoxSat { Chain = 0, Pitch = 1, Align = 2, NamIr = 3 };
    void openVoxSatelliteWindow  (int voxIdx, VoxSat kind);
    // The pedals window doubles as the LIVE-INPUT player window (L10): its
    // strip carries the page Menu, the pedalboard preset button, and a
    // NAM/IR launcher.
    void openInstPedalsWindow (int instIdx);
    void openInstNamIrWindow  (int instIdx);
    // Tab-close hygiene: a dead tab must not leave its satellites open.
    void closeVoxSatellites  (int voxIdx);
    void closeInstSatellites (int instIdx);
    // CL-044 (QA-ModelShell TS7): the floating master spectrum analyzer.  A
    // satellite window like the effect windows -- no ribbon slot, opened from
    // View > Master Analyzer.
    void openMasterAnalyzerWindow();

    // The app's ONE VU meter, in its own window: master OUTPUT level.  Opened
    // from the effects rack menu next to VU Calibration (Jeff, 2026-08-11) -- the
    // per-panel input VUs were removed because they cost 120px on every effect
    // panel.
    void openMasterVuWindow();

    // Installs the Inst nav menu (Pedals / NAM/IR / Piano Roll) on an Inst page.
    // Called for EVERY Inst page -- see the definition for why a live-input tab
    // used to miss it entirely.
    void installInstNavMenu (InstPage& ip,
                             juce::Component::SafePointer<InstPage> safe,
                             juce::Component::SafePointer<PageMenuBar> safeBar);

    // TS7 §6.8: freezes read out of a project file during deserialize, applied
    // after the whole graph exists.  Collected rather than applied inline because
    // the tabs they name do not exist yet at read time.
    // Carries the §6.8 span so restore can tell a file that still matches the
    // arrangement from one rendered against a length or tempo since changed.
    struct PendingFreeze
    {
        TabKind kind; int pageIndex; bool byUser;
        EngineTab::FreezeSpan span;
        // Per-pattern content stamps read back from the project, so each cached
        // pattern render is validated on its own rather than all-or-nothing.
        std::map<int, juce::uint32> patternStamps;
        // Saved while STALE: restore must re-render even on a matching stamp --
        // the stamp cannot see every invalidator, which is how it went stale.
        // Last, defaulted, so the refresh queue's aggregate inits stay valid.
        bool stale = false;
    };
    std::vector<PendingFreeze> mPendingFreezes;
    // §6.8: last pattern whose renders were published to the tasks.  -2 rather
    // than -1 so the first poll always publishes, including for pattern 0.
    int mLastRepublishedPattern { -2 };
    void restorePendingFreezes();

    // TS7 §6.9 / CL-055: smart auto-freeze.  Polled from the existing editor
    // timer rather than its own, and it freezes at most ONE tab per trip over
    // the threshold -- freezing is a blocking offline render, so doing several
    // in one go would stall the message thread exactly when the machine is
    // already struggling.
    void pollAutoFreeze();
    // TS7: names the track in the overlay for the AUTOMATIC freeze paths, which
    // otherwise stall the app for seconds with nothing on screen.  Pair with
    // mHeavyOpOverlay.endOp().
    void showFreezeRenderNotice (TabKind kind, int pageIndex);
    int  mAutoFreezeHoldTicks { 0 };
    // ARM at sustained load, FIRE at Stop (Jeff, 2026-07-31): the flag is what
    // survives the load falling once playback stops -- stopping otherwise
    // cleared the very condition that asked for relief.
    bool mAutoFreezePending { false };
    juce::uint32 mLastSwingStamp { 0 };   // TS7 §6.5 swing invalidator
    // TS7 §6.6: a freeze re-render blocks the message thread for SECONDS, so it
    // must not fire while the user is still editing.  Re-armed by every content
    // change; the refresh queue only drains after this much quiet.
    double mLastContentEditMs { 0.0 };
    static constexpr double kFreezeRefreshQuietMs = 2000.0;
    // Re-renders queued by markEngineContentChanged, drained one per tick for
    // the same reason.
    std::vector<PendingFreeze> mFreezeRefreshQueue;
    // The automatic freeze paths (stale refresh, pattern fill, auto-freeze) fire
    // uninvited and retry on quiet ticks, so a broken Freeze folder would alert
    // forever -- one alert per session, DBG thereafter.
    bool mAutoFreezeFailureShown { false };
    void reportAutomaticFreezeFailure (TabKind kind, int pageIndex, const juce::String& err);
    // A pattern fill that failed is parked until the next content edit:
    // findPendingPatternFreeze returns the same first gap every tick, so
    // retrying would flash the overlay + stall on every quiet tick.
    std::set<std::tuple<TabKind, int, int>> mFailedPatternFreezes;
    // Ruling 6a: push the export dialog's persisted spec choice into version
    // capture (init + every take start).
    void applyCaptureSpecFromPrefs();
    // The take-report write failure alert fires once per session, not per take.
    bool mTakeReportErrorShown { false };
    // A rack changed under us: ask every satellite window on that channel to
    // re-check its target, and close the ones whose effect is gone.
    void closeDeadEffectWindows (int channelId);
    // Save-side backstop for the same records.  False only for an effect-scoped
    // window key ("fx:" / "vis:" / "vispos:" + channel + slot UUID) whose UUID no
    // longer names a live rack slot; every other key passes.  Projects saved
    // before the prune above already carry dead records -- they still LOAD, they
    // just stop being rewritten.
    bool effectWindowKeyStillLive (const juce::String& key) const;

    // ── TS7 §3: version capture ──────────────────────────────────────────────
    // Rides the existing 30 Hz automation timer (VersionCapture::kTimerHz), not
    // a timer of its own: the analysis half is always on, so its wakeup cost is
    // paid for the whole session and a second timer would double it for nothing.
    VersionCapture mVersionCapture;
    // TS7 §6: the per-player Freeze button, wired once from showPageForTab.
    void wireFreezeSlotForVisiblePage();
    bool visiblePageTabIdentity (TabKind& outKind, int& outIndex) const;

    void pollVersionCapture();
    // What is playing, for the take's label.  Read at take start only.
    juce::String currentScopeLabel() const;
    // §3.6: copy a take's audio somewhere the user chooses.  A COPY, not a move
    // -- the take stays selectable in the analyzer afterwards.
    void exportCapturedTake (const VersionCapture::Version& v);

    // ── Core helpers ──────────────────────────────────────────────────────────
    void buildDefaultTabs();     // called in ctor: Builder + initial Layers/Bass/Drums
    // QA-ProjectSave docket 18 (2026-07-26): addDefaultDynamicTabs /
    // addDefaultDrumTab removed.  Layers / Bass / Drums no longer get seeded
    // with one instance each -- they open empty and delete down to zero like
    // every other tab type, so a saved project or template never carries tabs
    // the user did not ask for.
    // navigate=false creates the tab WITHOUT framing a window or selecting it.
    // Used by the Drum Kit's empty-row picker, which must not put a window on
    // screen before a sound has been chosen.  The new tab's id lands in
    // mLastAddedTabId.
    void onAddTabRequest(RibbonTabBar::TabType type, bool navigate = true);

    // Id of the tab the last onAddTabRequest created, or -1 if it made none.
    int  mLastAddedTabId { -1 };
    void applyEngineToNewestTabOfType (RibbonTabBar::TabType type,
                                       const juce::String& engine);
    void onTabSelected(int tabId);
    // The "which page am I on" state every navigation gesture has to refresh.
    // Both routes reach it: a ribbon click through onTabSelected, and a direct
    // click on a contained window through that window's onBroughtToFront.  Held
    // apart from showPageForTab's setup work so the front-brought path can stay
    // honest without re-running it (and without re-entering toFront).
    void updateActiveTabState (int tabId);
    void onTabClosed(int tabId);
    void onSubPageSelected(RibbonTabBar::TabType type, int subPageIndex);
    void showPageForTab(int tabId);
    // Rebuilds the Piano Roll window's title-strip slots (engine pill + the
    // Player Page / FX Rack jumps).  Resolves the bar through the page's OWN
    // window rather than mPageMenuBar, so it is correct no matter which page
    // is "visible" -- contained windows are all live at once, and the engine
    // pill has to follow the dropdown even when another window has focus.
    void refreshPianoRollTabSlots();
    // QA-Layout T4 (L11/D4=c): one function builds the instance dropdown's
    // window-row model AND executes a pick -- pickRow < 0 collects labels,
    // pickRow >= 0 runs that row's action.  Single body keeps the list and
    // the dispatch in lockstep (an index can never mean two things).
    juce::StringArray buildPageWindowRows (int tabId, int pickRow);
    void refreshPatternBox();

    // Create page components for each type
    std::unique_ptr<juce::Component> createLayersPage();
    std::unique_ptr<juce::Component> createBassPage();
    // D1.4: dynamic-drum tab.  QA-SOUNDNESS: allocates inside the ACTIVE
    // bank's index window, never "lowest free of 0..31" -- a drum added while
    // kit 2 is on screen has to be a kit-2 drum (own slot range, own bus).
    std::unique_ptr<juce::Component> createDrumPage();

    // P1+P2 persistence (2026-04-24): deterministic-index page creation used
    // by project load.  Returns null if `idx` is out of range or already in
    // use.  Unlike the index-less variants these do NOT pick the first
    // available slot - they demand the saved pageIndex, so the PatternManager's
    // layerRoll[idx] / bassRoll[idx] notes end up on the right tab.
    std::unique_ptr<juce::Component> createLayersPageAtIndex (int idx);
    std::unique_ptr<juce::Component> createBassPageAtIndex   (int idx);
    std::unique_ptr<juce::Component> createDrumPageAtIndex   (int idx);   // D1.4
    // D1.4-fix (c); QA-UndoCoverage Task 7 generalization: with a forced
    // pageIndex + name these ARE the structural-undo resurrect path (a
    // duplicate is just a resurrect at a fresh identity).  fullPreset = the
    // xml is a full-chain Page Preset (engines + strips + racks) instead of
    // the duplicate clipboard's engine-only blob.  Returns the new ribbon tab
    // id, -1 on failure.
    int spawnDuplicateDrumTab  (const juce::String& clipboardXml, int forcedPageIndex = -1,
                                const juce::String& forcedName = {}, bool fullPreset = false);
    int spawnDuplicateLayerTab (const juce::String& clipboardXml, int forcedPageIndex = -1,
                                const juce::String& forcedName = {}, bool fullPreset = false);
    int spawnDuplicateBassTab  (const juce::String& clipboardXml, int forcedPageIndex = -1,
                                const juce::String& forcedName = {}, bool fullPreset = false);

    // QA-UndoCoverage Task 7: user-facing tab delete with snapshot capture --
    // performs a StructuralOpAction whose undo resurrects the tab (same
    // pageIndex, same chain state) and whose redo re-deletes it.  Kinds
    // without a capture path yet fall back to a plain closeTab.
    void deleteTabWithUndo (int ribbonTabId);
    // The duplicate gesture as one transaction (undo removes the duplicate;
    // redo re-creates it at the same identity from its post-spawn snapshot).
    // QA-SOUNDNESS: `sourcePageIndex` is the DRUM case's bank hint -- a
    // duplicate belongs to the same kit as the drum it copies, which is not
    // necessarily the kit currently on screen.  -1 (every other type, and any
    // caller that has no source index) falls back to the active bank.
    void duplicateTabWithUndo (RibbonTabBar::TabType type, const juce::String& clipboardXml,
                               int sourcePageIndex = -1);

    // QA-UndoCoverage Task 7 pass 2 (Jeff ruling 2a): capture + resurrect for
    // the Clips/Vox/Inst/Plugins/Rusty kinds.  The record mirrors
    // serializeTabsInto's per-kind Tab element; resurrect mirrors the
    // project-restore branches (same creation order, same race-safe loads).
    std::unique_ptr<juce::XmlElement> captureTabRecord (PageEntry& e);
    // resurrectTabFromRecord is the gesture boundary: it rebuilds ONE tab under
    // a MissingFileReport::ScopedGesture.  Impl carries the per-kind branches so
    // every exit funnels through that one scope.
    int resurrectTabFromRecord (const juce::XmlElement& rec);
    int resurrectTabFromRecordImpl (const juce::XmlElement& rec);

    // Review fix (2026-08-06): user-initiated tab ADD as one transaction for
    // the captureTabRecord kinds (undo closes the tab; redo resurrects it from
    // its add-time record).  asRider appends into the CURRENT transaction
    // instead of beginning one (Clips add composes with its library action).
    // Suppression counter: composite adds (Guitars/Basses, Vox/Inst
    // duplicates) spawn a plain page mid-flow through the strip cascade and
    // wrap themselves at the end with the full record instead.
    void wrapTabAddUndo (int ribbonTabId, const juce::String& label,
                         bool asRider = false);
    int  mSuppressAddUndoWrap = 0;

    // ── Tab / mixer-strip rename ─────────────────────────────────────────────
    // Both rename gestures (right-click tab > Rename, and editing a strip's
    // name label) write the SAME four places -- ribbon label, the page's
    // mTabName, the mixer strip label, the piano-roll context label -- so they
    // share one apply body and one transaction.  The transaction is also what
    // marks the project dirty: TransactionTracker derives dirt from undo depth
    // alone, so a rename that opens no transaction is silently discarded at
    // quit.  ONE transaction per gesture, never one per place written.
    //
    // Targets are keyed by page family + the page's own index, never by
    // ribbonTabId: a tab deleted and later resurrected by undo comes back with
    // a fresh ribbon id, so an action recorded against the old id would find
    // nothing to reverse.
    enum class RenameFamily { Layers, Bass, Drums, Inst, Clips, Plugins, Vox, Rusty };

    PageEntry*   findRenameTarget (RenameFamily fam, int pageIndex) const;

    // Retitles a tab's WINDOW.  Used by the hosted-plugin name callbacks, which
    // rename the ribbon tab directly rather than going through a user rename.
    void         setWindowTitleForTab (int ribbonTabId, const juce::String& name);
    bool         renameKeyFor     (const PageEntry& e, RenameFamily& famOut,
                                   int& pageIndexOut) const;
    juce::String tabNameOfPage    (const PageEntry& e) const;
    juce::String uniqueTabNameFor (RenameFamily fam, int pageIndex,
                                   const juce::String& candidate) const;
    void         applyPageRename  (RenameFamily fam, int pageIndex,
                                   const juce::String& name);
    void         performPageRename (RenameFamily fam, int pageIndex,
                                    const juce::String& typedName);

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

    // Which of the two save shapes the tab walk is feeding.  Freeze is the one
    // thing the structural walk emits that is PROJECT data and not skeleton
    // data: the renders live in <project>\Freeze\ and the stamps are the
    // original song's, so a template that carried them either failed to
    // restore (New from Template has no project folder yet) or re-rendered the
    // tab against the template's deliberately blank arrangement and came back
    // frozen to silence.  `locked` stays in both -- that IS structural.
    enum class SaveShape { Project, Template };

    void serializeStructuralUIState  (juce::XmlElement& ui,   SaveShape shape);
    void serializeTabsInto           (juce::XmlElement& tabs, SaveShape shape);
    // QA-ProjectSave Task 5 (2026-07-26, dockets 23/24): after a TEMPLATE's tab
    // walk, adopt every referenced file that is not already under a stable root
    // into My Samples and rewrite the reference to point there.  Project saves
    // do NOT call this -- a project has its own Samples folder, so its refs are
    // already portable with the folder; a template is one loose XML and has to
    // carry its dependencies somewhere durable.
    void adoptTemplateSampleRefs     (juce::XmlElement& tabs,
                                      juce::Array<juce::File>& createdOut);
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
    // KitDrumInfo list with active-tab marked.  Feeds the kit grid's row
    // provider on PianoRollPage.
    std::vector<KitDrumInfo> getKitDrumList() const;
    // Refresh the kit view.  Triggered after add / remove / rename /
    // active-tab change.
    void refreshAllKitViews();
    // Wire a freshly-created DrumPage's play-pitch callback.  Called from
    // every DrumPage creation site.
    void wireDrumPagePlayNote (class DrumPage* dp);
    // Wire the DrumKitContainer hosted on PianoRollPage.  Since the
    // 2026-04-26 unified migration this is the app's only kit view.
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
    // (Harmless / BaySickSynth / BaySickBass / BaySickPlayer / BaySickGuitars /
    // BaySickRustyDrums / BaySickPedals / BaySickNAMIR / BaySickVocal) owns
    // its own APVTS that's invisible to the main PluginProcessor's project-
    // dirty listener.  This helper dynamic_casts to each known type and
    // installs a markDirty hook so any APVTS edit on the engine flips the
    // project dirty bit.  Pass nullptr safe.  No-op for unknown types.
    void wireEngineDirtyHook (juce::AudioProcessor* eng);
    // D2 Batch 4: move drum at row `srcRow` to row `dstRow` in ribbon order.
    // Reorders mPages + mRibbon's tab list + refreshes every kit view.
    void moveDrumTab (int srcRow, int dstRow);

    // ── QA-SOUNDNESS (2026-08-07, Jeff): two independent drum kits ─────────
    // Bank identity is the SLOT RANGE and nothing else -- pages 0..15 are kit
    // 1, 16..31 are kit 2 (MixerChannelIds::drumBankForPage).  No object
    // stores a bank field, so nothing can disagree with the engine rig and a
    // project saved before the split falls into kit 1 for free.  Every add /
    // save / load path resolves the bank through these three.
    int  activeDrumBank() const noexcept { return mActiveDrumBank; }
    void setActiveDrumBank (int bank);            // stores + drives the kit view
    int  firstFreeDrumIndexInBank (int bank) const;   // -1 when that bank is full
    static juce::String drumBankLabel (int bank);     // "1-16" / "17-32"
    void showDrumBankFullMessage (int bank);
    // The kit the user is looking at.  Persisted with the rest of the UI state
    // (it used to reset to kit 1 on every project open).
    int  mActiveDrumBank { 0 };

    // ── Batch 5: Kit save/load ────────────────────────────────────────────
    // Kit XML format: <BaySickKit name="..." version="1">
    //                   <Drum slot="N">
    //                     <DrumPageState .../>     <!-- exportDrumState() XML -->
    //                   </Drum>  ... × non-empty slots
    //                 </BaySickKit>
    // Empty slots (no drum tab in ribbon) are simply omitted.
    // QA-SOUNDNESS: a kit file is ONE bank -- slots are written normalized to
    // 0..15 and remapped into the target bank's range on load, which is what
    // lets a kit saved from kit 2 load into either kit (and every factory kit,
    // all of which ship slots 0-15, load into kit 2).
    static juce::File kitsDir();         // <Documents>/BaySickDAW/Kits
    static juce::File userKitsDir();     // <kitsDir>/My Kits
    static juce::File factoryKitsDir();  // <kitsDir>/Factory
    void showKitMenu (juce::Component* anchor);    // popup with Save/Load
    void saveKitAs ();                              // prompt + write XML
    void loadKit   (const juce::File& kitXml);      // entry: confirm + dispatch
    void loadKitImpl (const juce::File& kitXml, int bank);   // tear-down + rebuild, ONE bank
    // QA-UndoCoverage Task 7: loadKitImpl wrapped as ONE undo transaction.
    void loadKitWithUndo (const juce::File& kitXml, int bank);
    // Lock/Unlock prompt + toggle, scoped to ONE kit (bank 0 = drums 1-16,
    // bank 1 = drums 17-32).  The kits lock independently.
    void showGlobalLockPrompt (int bank);
    void applyGlobalLockToggle (int bank);
    // Undo/redo body for the bank toggle: a list of (drum page index, locked).
    // Page-INDEX keyed, not pointer keyed -- a Drums tab can be closed and
    // resurrected between the do and the undo, so both ends re-resolve through
    // mPages at apply time, the live-page rule the rest of the history follows.
    void applyDrumLockStates (const std::vector<std::pair<int, bool>>& states);
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
    BaySickDAWProcessor&            mProcessor;
    StandalonePlayHead&            mPlayHead;
    juce::AudioDeviceManager&      mDeviceManager;
    juce::AudioIODeviceCallback*   mAudioCallback { nullptr }; // set by StandaloneApp

    // Single pattern manager shared by all pages
    std::unique_ptr<PatternManager> mPM;

    // Project persistence (P1+ - 2026-04-23).  Owns the "current project"
    // folder on disk, serializes/deserializes via BaySickDAWProcessor.
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
    // Update the DocumentWindow title to show the current project name + dirty marker.
    void refreshWindowTitle();

    // P5: if project is dirty, show Save/Don't Save/Cancel confirmation and
    // call continuation on Save (after successful save) or Don't Save.  If
    // not dirty, calls continuation immediately.  Cancel aborts.
    void confirmDiscardChanges (std::function<void()> continuation);
public:
    // P5: called from BaySickDAWWindow::closeButtonPressed to intercept the
    // close-with-unsaved flow.  Returns true if quit should proceed
    // synchronously; returns false when a dialog is shown and the app should
    // wait for the user's choice (continuation calls quit() on accept).
    bool requestAppQuit();
    // Capture every live window's bounds into the lifetime-1 map.  Public so
    // shutdown can run it BEFORE editor teardown: the window destructors were
    // the only thing feeding the exit write, and by then a window is mid-
    // dismantle -- what landed in settings.xml was not where the user left it
    // (Jeff, 2026-08-04: the Mixer kept reopening in the wrong place).
    void flushWindowBoundsNow() { flushAllWindowBounds(); }
private:

    // QA-UndoCoverage: the ONE global undo history lives on the processor
    // (declared there BEFORE apvts -- apvts binds it at construction).  A
    // reference keeps every existing consumer (UndoContext, EventEditor,
    // history window, depth menu) pointed at the processor's manager.
    juce::UndoManager& mUndoManager;
    int               mUndoHistorySize { 100 };  // mirrors current max; JUCE has no getter

    // History label list + cursor: REBUILT from the manager's real stack on
    // every change message (QA-UndoCoverage Task 3) -- the deque/cursor pair
    // survives because UndoHistoryWindow references both.  Never appended to
    // directly.
    std::deque<juce::String>           mHistoryLabels;
    int                                mHistoryCursor { 0 };
    std::unique_ptr<UndoHistoryWindow> mHistoryWindow;
    void rebuildHistoryLabels();
    juce::String historyDisplayFor (const juce::String& transactionName) const;
    juce::String ownerKeyForParamId (const juce::String& paramId) const;
    // QA-UndoCoverage Task 8: previous manager depths for the dirty-tracker
    // event derivation in changeListenerCallback.
    int          mPrevUndoDepth { 0 };
    int          mPrevRedoDepth { 0 };
    juce::String mPrevTopUndoName;
    // QA-UndoCoverage Task 4: editor-side capture/apply for AudioLibraryAction
    // (the browser-panel ops carry their own; these serve the editor lambdas).
    AudioLibrarySnapshot captureAudioLibrarySnapshot (bool withBlocks) const;
    AudioLibraryAction::ApplyFn makeAudioLibraryApply();

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
    // TS6 (BLU-447) plugin roll audition: the note a ONE-SHOT preview left
    // ringing, released when the next preview or a press-and-hold starts.
    // Message thread only.  One value, not one per tab, because only one roll is
    // active at a time -- the same reason the live MIDI target is a single value.
    int mPluginAuditionHeldNote { -1 };
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
    // G1 review fix: public so BaySickDAWWindow can flush held typed notes on
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

    // QA-ModelShell TS5: the per-effect + EQ windows.  Declared AFTER
    // mWorkspace deliberately -- see the note on the AuxWindow struct.
    std::vector<AuxWindow> mAuxWindows;


    // Pointers to single-instance legacy pages (no duplication)
    // These are owned inside mPages[...].component
    LayersPage* mLegacyLayersPage  { nullptr };
    BassPage*   mLegacyBassPage    { nullptr };
    class DrumPage* mLegacyDrumPage { nullptr }; // D1.4 dynamic-drum model (last-created, parallel to LegacyLayers)
    BuilderPage* mBuilderPage      { nullptr };
    MixerPage*  mMixerPage         { nullptr };

    // Tracks which Layers page indices (0–kMaxLayerPages-1) are currently in use.
    // Lets createLayersPage() assign the first free slot and onTabClosed() release it.
    std::array<bool, kMaxLayerPages> mUsedLayerIndices {};
    // QA-ModelShell TS6 (BLU-447): hosted VST3 instrument tabs.
    std::array<bool, kMaxPluginPages> mUsedPluginIndices {};
    std::unique_ptr<juce::Component> createPluginsPage();
    std::unique_ptr<juce::Component> createPluginsPageAtIndex (int idx);

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
    juce::String nextPluginTabName()  { return "Plugin " + juce::String (mNextPluginNameNum++); }
    int mNextPluginNameNum { 1 };
    juce::String nextBassTabName()    { return "Bass "   + juce::String (mNextBassNameNum++); }
    juce::String nextDrumTabName()    { return "Drum "   + juce::String (mNextDrumNameNum++); }
    juce::String nextVoxTabName()     { return "Vox "    + juce::String (mNextVoxNameNum++); }
    juce::String nextInstTabName()    { return "LiveInst " + juce::String (mNextInstNameNum++); }
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

    // Most recently visited PLAYER tab, across every player family.  The
    // ribbon's own last-used cache is per TYPE and so cannot answer "which
    // player was I on"; this rides the same update point, which is what keeps
    // it true for a ribbon click and for a click on a window alike.
    // Player = the instrument families the user would call a player: Layers,
    // Bass, Drums (the Rusty kit is a Drums-type tab), Clip, Vox, Inst,
    // Plugins.  Mixer / Effects / Builder / PianoRoll are utility tabs and
    // never claim it.  Validated against mPages on read, so a deleted tab
    // reports none.
    int mLastPlayerTabId { -1 };
    static bool isPlayerTabType (RibbonTabBar::TabType t) noexcept;

    // R5d (2026-04-24): post-stop routing.  Drops any captured WAVs onto
    // the next free arrangement row (at the beat where Play was pressed),
    // and forwards any captured MIDI notes to the last-accessed piano
    // roll (R5d-midi).  Fire-and-forget - safe to call with an empty
    // result, so Pause / Stop / Record-disarm all use the same path.
    void commitRecordingResult (const struct BaySickDAWProcessor::RecordResult& res);

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
        void timerCallback() override
        {
            owner.pollDenoiseState();
            // TS7 §6.9: rides this existing 5 Hz poll rather than starting a
            // timer of its own -- an auto-freeze check does not need its own
            // wakeup, and a second timer would be pure cost on the machine this
            // feature exists to relieve.
            owner.pollAutoFreeze();
            owner.pollPendingWindowDefaults();
        }
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
    // sourceNoun headlines the missing-file dialog this drains into at its tail.
    // It has to travel with the call: this drain empties the store, so a caller
    // that drains again afterwards under its own noun finds nothing and the
    // user reads whichever noun got here first.
    void restoreAudioStripsFromArrangement (bool isLoadContext = true,
                                            const juce::String& sourceNoun = "project");
    // The name an Audio-row strip is rebuilt with on the next load.  Shared
    // with restoreAudioStripsFromArrangement so the two cannot drift: it is
    // what the mixer snaps a rejected strip rename back to.
    juce::String persistedAudioRowName (int row) const;

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

    // Global tooltip window (styled via BaySickLAF::drawTooltip / getTooltipBounds)
    std::unique_ptr<BaySickTooltip> mTooltipWindow;
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
    juce::Component::SafePointer<juce::Component> mManualsWin;
    void showKeyBindsWindow();
    void showManualsWindow();
    juce::Component::SafePointer<juce::Component> mRustyDrumsMapWin;  // J-7b
    void showPluginsWindow();                                         // QA-ModelShell TS6
    juce::Component::SafePointer<juce::Component> mPluginsWin;
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
    // interactive is handed straight to ClipsPage::setClipFilePath -- false
    // suppresses its per-clip "nothing playable" alert for restore paths, whose
    // misses are already batched into MissingFileReport.
    void spawnClipsTabIfMissing (int audioRow, const juce::String& path,
                                 bool allowDuplicate = false,
                                 bool interactive = true);
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
    // interactive is forwarded through spawnClipsTabIfMissing to the page's
    // setClipFilePath: project restore and undo resurrection pass false so a
    // project with N missing clips raises ONE batched report instead of N
    // stacked modal alerts.  Every user gesture keeps the default.
    void createClipStripAndPage (int row, const juce::String& path,
                                 bool allowDuplicate = false,
                                 bool interactive = true);
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
    // QA-ModelShell TS6 (BLU-447): hosted VST3 instrument tab.
    void registerPluginPianoRoll (int idx, class PluginsPage* pp);

    // K-3 (2026-05-05): wires a sfizz-source Inst page (BaySickGuitars /
    // BaySickBasses) into the unified PianoRollPage.  dataAccessor points at
    // Pattern::instRoll[idx]; audition closures route to the engine's
    // auditionNote.  Caller (K-4 addBaySickGuitarsTab / L-3 addBaySickBassesTab)
    // invokes after the engine is created via BaySickDAWProcessor::loadKit.
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
    // selectAfter controls whether the new tab takes focus.  false = stay put
    // (strip-driven adds, so several can be added without being yanked away on
    // each one).  true = navigate to the new tab, for gestures where the user
    // explicitly asked for the PAGE (ribbon empty-state click).
    // (Said "Mixer 'Add Vox/Inst Strip'" until 2026-08-05 -- T10 replaced those
    // buttons with the Add titled menu.)
    // Return the new tab's ribbon id, or -1 when the tab already existed (or
    // creation failed) -- the strip-cascade add wrap keys on "actually
    // spawned" so resurrection/restore re-entries never re-perform.
    int spawnVoxTabIfMissing  (int voxIdx,  bool selectAfter = true);
    int spawnInstTabIfMissing (int instIdx, bool selectAfter = true);

    // Page-switch command helpers.
    // Find the first tab of the given ribbon type and select it.  No-op when
    // no tab of that type exists.
    void selectFirstTabOfType (RibbonTabBar::TabType type);
    // F10 / View > Show Piano Roll - open the unified Piano Roll page WITHOUT
    // touching which engine it shows; that pick is the roll's own project-
    // persisted state.
    void showUnifiedPianoRoll();
    // F11 / View > Show Drum Kit - the Piano Roll page on its 16-pad kit view.
    void showDrumKitGrid();
    // F7 / View > Show Player - the instrument tab last visited this session,
    // falling back to the first one open.  No-op when none are open.
    void showMostRecentPlayerTab();

    // Pattern-navigation command helpers.
    void showRenamePatternDialog();   // F2
    void jumpToNextEmptyPattern();    // F3
    void createNewPattern();          // F4
    void cyclePattern (int delta);    // + / - (delta = ±1, wraps)
    bool isPatternEmpty (int idx) const;

    // Pattern-list editing command helpers.  Insert / Delete / Move re-point
    // every stored pattern index (arrangement blocks, linked time-signature
    // markers, the selection), so their undo has to carry the marker set as
    // well as the pattern list - hence performPatternTsOp, not the slice-only
    // wrapper Clone can get away with.
    void insertPatternAfterCurrent();   // Shift+Ctrl+Ins
    void duplicateCurrentPattern();     // Alt+C
    // Menu-only, no shortcut: bare Delete belongs to whichever editing surface
    // has focus (Builder grid, Piano Roll, Drum Kit, Event Editor, pitch
    // editor all delete their own selection with it).
    void deleteCurrentPattern();
    void moveCurrentPattern (int delta);  // Shift+Ctrl+Up / Down (delta = -1 / +1)

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
    // Populated by the static APVTS sweep plus the model-side registrations
    // (rig engine-created, sfizz-ready, mixer-strip param materialization,
    // rack slot, pedal board, plugin tab).  NEVER against a view -- a
    // widget-scoped applicator dies with its window and leaves a silent lane.
    std::map<juce::String, std::function<void(float)>> mAutomationApplicators;

    // Maps paramId → function that reads the current 0..1 value from the live control.
    // Used when creating automation blocks to seed the initial control points.
    std::map<juce::String, std::function<float()>>     mAutomationValueReaders;

    // Drops every applicator + reader belonging to one retired rack/pedal slot
    // UUID.  Both maps were insert-only outside the project-boundary clear, so
    // swapping effects in a slot leaked their closures and left their ids in the
    // Event Editor's target list looking like live lanes.  Message thread.
    void unregisterAutomationForSlotUuid (const juce::String& slotUuid);

    // The engine-tab half of the same problem, driven by
    // EngineRig::onEngineDestroying.  Registration was model-side and permanent
    // while removal existed only for rack/pedal slot uuids and the whole-map
    // project-boundary clear, so a deleted tab's ids stayed in the Event
    // Editor's New Automation list AND made the stale-lane detector answer
    // "live" for a lane whose target was gone -- worse once the freed page
    // index was reused by a tab running a different engine.  Message thread.
    void unregisterAutomationForTab (TabKind kind, int pageIndex);

    // Statics = every APVTS param (plus the derived "<prefix>_fader" aliases)
    // + "global_tempo".  Called from the ctor, after resetProjectState()'s full
    // map clear, and from BaySickDAWProcessor::onMixerStripParamsCreated -- a
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

    // 2026-08-02 (ruling 2-b): lanes for the Plugins-tab instrument itself,
    // keyed "plugtab<pageIndex>_vst_<paramId>".  Re-run on every param-count
    // change (bridged lists arrive async after load) via
    // PluginsPage::onParamListChanged; idempotent.
    void registerPluginTabAutomation (int pageIndex);

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
    void registerSfizzEngineAutomation (BaySickDAWProcessor::SfizzEngineKind kind,
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
        void timerCallback() override
        {
            owner.applyAutomationAtCurrentPosition();
            owner.pollVersionCapture();   // TS7 §3.2 (see pollVersionCapture)
        }
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
    // Most recently opened or activated Event Editor.  SafePointer, not a raw
    // pointer: an editor is deleted out from under this the moment its window
    // closes, and the accessor has to report none rather than hand back a dead
    // window.
    juce::Component::SafePointer<EventEditor> mLastEventEditor;
    // EventEditor is a plain DocumentWindow with no front hook of its own, so
    // recency comes from a listener attached at open -- JUCE raises
    // componentBroughtToFront for every activation route a top-level window
    // has (click, toFront, Alt+Tab).
    //
    // DECLARED BEFORE mEventEditors ON PURPOSE.  Members destruct in REVERSE
    // declaration order, so being declared first means outliving the windows
    // that hold this as a listener -- the other order would leave each editor
    // calling into a dead object on its way out.
    struct EventEditorFrontWatcher : public juce::ComponentListener
    {
        StandaloneEditor& owner;
        explicit EventEditorFrontWatcher (StandaloneEditor& o) : owner (o) {}
        void componentBroughtToFront (juce::Component& c) override
        {
            if (auto* ed = dynamic_cast<EventEditor*> (&c)) owner.mLastEventEditor = ed;
        }
    } mEventEditorFrontWatcher { *this };

    // Owned list of open EventEditor windows. Windows remove themselves on close.
    juce::OwnedArray<EventEditor> mEventEditors;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StandaloneEditor)
};
