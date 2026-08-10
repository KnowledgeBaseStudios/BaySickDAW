#pragma once
#include <JuceHeader.h>

class BaySickDAWProcessor;
class PatternManager;

namespace juce { class Timer; }

// ──────────────────────────────────────────────────────────────────────────────
// ProjectManager (project-persistence Phase P1, 2026-04-23)
//
// Owns the "current project" concept: the folder on disk that holds the user's
// project.xml + Samples/ + any future per-project assets.  This is BaySickDAW's
// equivalent of a .flp / .als / .logicx - a self-contained folder the user
// names once and then saves to forever.
//
// Conventions locked (see blueprint Project Persistence section +
// 2026-04-23 unified-layout revision):
//
//   Documents\BaySickDAW\                ← user-visible, one-folder rule
//   ├── Projects\<name>\
//   │   ├── project.xml                  ← single serialized state doc
//   │   └── Samples\                     (project-bundled audio, copy-on-drop)
//   ├── Recordings\                      (audio captures)
//   ├── Presets\<engine>\                (user-saved + factory presets)
//   ├── Templates\                       (P6 default-template targets)
//   ├── settings.xml                     (recent-projects + global prefs)
//   ├── audio_settings.xml               (machine-scoped audio device state)
//   └── Sample Library.lnk               (P4/P6: shortcut to LocalAppData)
//
//   %LOCALAPPDATA%\BaySickDAW\
//   └── CoreLibrary\<pack>\              (installed sample library)
//
// Each project is a folder - never a single file.  Autosave writes a
// timestamped copy every 15 min into <project>\Backups\ (newest 10 kept), or
// into Documents\BaySickDAW\Backups\Unsaved\ when no project is open so an
// unsaved session can still be recovered after a crash.
// Samples under a stable root (Core Library / My Samples) are REFERENCED as a
// `library:` / `mysamples:` string, not copied - they are reachable from any
// project on this install, so a copy would duplicate factory content per
// project.  Only volatile sources (Downloads, Desktop, USB) land in Samples\.
//
// Phase P1 scope: skeleton class only - methods exist, serialization works
// against BaySickDAWProcessor::serializeProject/deserializeProject, but the UI
// (File menu / New/Open dialogs / autosave timer firing / dirty tracking) lands
// in later phases P2-P5.  Recent-projects list is persisted to
// Documents\BaySickDAW\settings.xml (see getSettingsFile).
// ──────────────────────────────────────────────────────────────────────────────
class ProjectManager : private juce::Timer
{
public:
    ProjectManager (BaySickDAWProcessor& processor);
    ~ProjectManager() override;

    // ── Default paths ────────────────────────────────────────────────────────
    // Lazy-created on first project save.  Matches the
    // Documents/BaySickDAW/Recordings/ convention already in the codebase.
    static juce::File getDefaultProjectsRoot();

    // settings.xml (recent-projects list + future global prefs).  Lives in
    // Documents\BaySickDAW\ alongside Projects / Recordings / Presets /
    // Templates - the unified user-visible folder.
    static juce::File getSettingsFile();

    // ── Project-name validation (Windows filename rules) ─────────────────────
    // Rejects <>:"/\|?* and reserved device names (CON, PRN, AUX, NUL,
    // COM1..9, LPT1..9).  Trims whitespace + trailing dot.  Returns empty
    // string = invalid; caller shows an error.
    static juce::String sanitizeProjectName (const juce::String& raw);
    static bool         isValidProjectName  (const juce::String& name);

    // ── Project lifecycle ────────────────────────────────────────────────────
    // newProject: creates <root>/<name>/ + empty project.xml + Samples/.
    //   Returns true on success.  Fails if name is invalid or the folder
    //   collision couldn't be auto-resolved.  (The old templatePath folder-seed
    //   died with the clone-a-project flow, QA-ProjectSave Task 10 -- templates
    //   load through StandaloneEditor::loadTemplate, never as folder copies.)
    bool newProject (const juce::String& name);

    // openProject: loads an existing project folder.  Accepts either the
    // folder itself or the project.xml inside.
    bool openProject (const juce::File& folderOrXml);

    // saveProject: writes current state to <currentFolder>/project.xml.
    // No-op if no project is open.
    bool saveProject();

    // saveProjectAs: duplicates the current project folder to <root>/<newName>/
    // (user doesn't pick a location).  Switches the "current" project to the new
    // folder.  Returns true on success.  A false return is a session no-op: the
    // new folder may exist on disk, but the session is left on the project it
    // was already on, which is what both callers tell the user has happened.
    bool saveProjectAs (const juce::String& newName);

    // deleteProject: moveToTrash the given project folder (Recycle Bin).
    // Caller must confirm first.  Does NOT close the current project if the
    // target happens to be the currently-open one - caller is responsible.
    static bool deleteProject (const juce::File& projectFolder);

    // renameProject: rename the folder on disk.  Only valid on projects NOT
    // currently open (caller must check isCurrentProject).  Returns true on
    // success.
    static bool renameProject (const juce::File& projectFolder,
                               const juce::String& newName);

    // P4: first-launch housekeeping.  Idempotent; safe to call every launch.
    //   - Ensures Documents/BaySickDAW/ exists.
    //   - Creates Sample Library.lnk shortcut to %LOCALAPPDATA%/BaySickDAW/
    //     CoreLibrary/ the first time (guarded by a "shortcutCreated" flag
    //     in settings.xml so users who delete it don't see it come back).
    //   - Checks whether the sound content is actually installed and, if not,
    //     offers to fetch it (content delivery, 2026-08-09).
    void runFirstLaunchHousekeeping();

    // -- Core sound content (content delivery, 2026-08-09) --------------------
    // The offer the startup check raises, also callable on demand so a user who
    // said "not now" (or "don't ask again") has a way back to it.  userAsked
    // means the user reached for this deliberately: it ignores the "don't ask
    // again" preference and reports when nothing is missing, where the startup
    // path stays quiet in both those cases.
    //
    // Never blocks: the check is a directory listing, and the download runs on
    // its own thread behind a progress window with a working Cancel.  An install
    // with no content still opens the app; only the sampled instruments are
    // affected, and they say so.
    //
    // NOT YET REACHABLE with userAsked == true: runFirstLaunchHousekeeping is
    // the only caller and it passes false, so a user who chose "Not Now" or
    // "Don't Ask Again" currently has no way back.  Needs one menu item wired to
    // offerCoreContentDownload (true).
    void offerCoreContentDownload (bool userAsked);

    // "Don't ask again" from the startup offer.  Persisted in settings.xml.
    bool getSkipCoreContentPrompt() const { return mSkipCoreContentPrompt; }
    void setSkipCoreContentPrompt (bool v);

    // duplicateProject: copy the folder to <projects-root>/<newName>/.
    // Collision-safe (appends " (N)" if target exists).  Returns the new
    // folder on success, or juce::File() on failure.  Does NOT switch the
    // current project - caller may openProject(returned) to switch.
    static juce::File duplicateProject (const juce::File& projectFolder,
                                         const juce::String& newName);

    // ── Project browser listing ──────────────────────────────────────────────
    // Enumerates the projects root.  Each entry = {folder, last-modified, size}.
    // Sorted newest-first by default.  Skips non-project subfolders (requires
    // a project.xml to count).
    struct Listing
    {
        juce::File    folder;
        juce::Time    lastModified;
        juce::int64   sizeBytes { 0 };
        juce::String  name;       // folder name for display
    };
    static std::vector<Listing> listProjects();

    // ── Queries ──────────────────────────────────────────────────────────────
    bool         hasProject()       const { return mCurrentFolder != juce::File(); }
    juce::File   getCurrentFolder() const { return mCurrentFolder; }
    juce::String getCurrentName()   const { return mCurrentFolder.getFileName(); }
    juce::File   getSamplesFolder() const
    {
        return hasProject() ? mCurrentFolder.getChildFile("Samples") : juce::File();
    }
    bool isCurrentProject (const juce::File& folder) const
    {
        return hasProject() && folder == mCurrentFolder;
    }

    // ── Recent projects (persisted across runs) ──────────────────────────────
    // Returns up to 10 most-recently-opened project folders, newest first.
    juce::Array<juce::File> getRecentProjects() const;
    void                    clearRecentProjects();

    // ── P5: dirty tracking + autosave ────────────────────────────────────────
    // markDirty is called from anywhere user edits reach the data model
    // (APVTS listener, undoable actions via StandaloneEditor::doUndoAction).
    // While mIgnoreDirty is true, markDirty is a no-op - used during
    // load/save so we don't self-mark dirty when replacing state.
    // QA-UndoCoverage Task 9: the unconditional-bool model is RETIRED.
    // markDirty now only advances the TS7 change stamp (version capture's
    // detector needs every edit path it always had); DIRTY itself is the
    // transaction-pointer mismatch on the processor's TransactionTracker.
    void markDirty();
    // Save sync: pins saved = current so the asterisk clears at the save
    // point (and returns the moment Ctrl+Z walks past it).
    void markSaved();
    // Load-boundary reset: loads don't transact -- clears the undo history,
    // sweeps the structural-undo snapshot store, resets the pointers, fires
    // clean.  (Former semantics: set the bool false.)
    void clearDirty();
    bool isDirty() const;

    // TS7 §3.3: monotonic edit counter for version capture's change detector.
    // Deliberately keyed on markDirty rather than a new signal: that is already
    // the FULL-SCOPE edit path (main APVTS listener, so rack knobs / bus EQ /
    // master limiter / faders; every engine's dirty tracker; PatternManager
    // mutations; EffectRack lifecycle), which is exactly the scope capture needs
    // because it taps the POST-FADER master.  Freeze's per-tab engine-scope
    // stamp is a SEPARATE detector on purpose -- it would call a fader move
    // "unchanged" and discard a pass that sounds different.
    //
    // Not cleared by save: this asks "has anything changed since", not "are
    // there unsaved edits".
    juce::uint32 getChangeStamp() const noexcept
        { return mChangeStamp.load (std::memory_order_relaxed); }

    // QA-D STATE-01: project-load suppression accessors.  openProject already
    // wraps deserializeProject with mIgnoreDirty, but follow-up load work
    // that runs after openProject returns (e.g.
    // StandaloneEditor::restoreAudioStripsFromArrangement -> applyPendingRack
    // States, which fires EffectRack::clearSlot lifecycle dirty hooks) sits
    // outside that window and needs to gate dirty fires too.  isLoadingProject
    // returns the gate state; setIgnoreDirty flips it -- callers stash the
    // prior value so they nest safely.
    bool isLoadingProject() const noexcept { return mIgnoreDirty; }
    void setIgnoreDirty (bool flag) noexcept { mIgnoreDirty = flag; }

    // onDirtyChanged fires when dirty transitions true<->false.  Wired by
    // StandaloneEditor to update the title-bar asterisk.
    std::function<void()> onDirtyChanged;

    // QA-D STATE-04: fires at the top of openProject() before any load work
    // starts.  Wired by StandaloneEditor to stop the transport / playhead so
    // a project loaded mid-playback halts cleanly instead of streaming silence
    // (or worse, half-torn-down engine state) into the audio thread until the
    // load finishes.  ProjectManager doesn't link against StandalonePlayHead,
    // so the callback indirection keeps the dependency direction one-way.
    std::function<void()> onBeforeOpenProject;

    // ── P6: Default template ─────────────────────────────────────────────────
    // The default template (when set) is a template XML file, loaded through
    // StandaloneEditor::loadTemplate on the same path as any other template pick
    // - nothing is copied.  Empty path = no template; new projects start blank.
    // Persisted in settings.xml so the choice survives across runs.
    juce::File getDefaultTemplate() const   { return mDefaultTemplate; }
    void       setDefaultTemplate (const juce::File& folder);
    void       clearDefaultTemplate()       { setDefaultTemplate ({}); }

    // ── 2026-04-26: "Don't show again" prompt prefs ──────────────────────────
    // Two confirm-prompts on the kit view that the user can opt out of.
    // Both default to false (prompt shown).
    //
    // The lock opt-out is PER DRUM BANK because the gesture it guards is: the
    // kit view locks 1-16 and 17-32 separately, so a "stop asking me" taken
    // while locking one kit would otherwise silence the confirm for the other
    // kit's drums, which the user never agreed to skip.  bank is clamped 0/1.
    bool getSkipGlobalLockPrompt (int bank) const
        { return mSkipGlobalLockPrompt[(size_t) juce::jlimit (0, 1, bank)]; }
    bool getSkipKitReplacePrompt()  const { return mSkipKitReplacePrompt;  }
    void setSkipGlobalLockPrompt (int bank, bool v);
    void setSkipKitReplacePrompt (bool v);

    // ── Backup discovery + restore ───────────────────────────────────────────
    // Lists backup XML files newest-first.  When a project is open, scans
    // <project>/Backups/.  When no project is open, scans the global
    // Documents\BaySickDAW\Backups\Unsaved\ folder.
    juce::Array<juce::File> listBackups() const;

    // Restores the given backup XML.  Behavior depends on whether a project
    // is currently open:
    //   - Project open: copies the backup over the current project.xml AND
    //     reloads it.  Original project.xml is preserved as
    //     project.xml.before-restore in case the user wants it back.
    //   - No project open: deserializes the backup into the in-memory state
    //     without touching disk.  User then has to File > Save to commit.
    bool restoreBackup (const juce::File& backupXml);

    // ── Sample import (P4) ───────────────────────────────────────────────────
    // Called by Builder when the user drops a WAV onto the arrangement.
    // Copies the file into <projectFolder>/Samples/ (deduped by filename),
    // and returns the relative path ("Samples/<filename>") to store on the
    // arrangement block.  Empty return = no project open / import failed;
    // caller should show "Create a project first" dialog.
    juce::String importSample (const juce::File& externalFile);

    // QA-E Task 7 (FILE-02): ALWAYS make a fresh auto-numbered duplicate of
    // src in Samples/ (never returns an existing path -- unlike importSample,
    // which skips the copy when the asset is already present).  Used by the
    // Properties "Copy" action.  Same " (N)" numbering scheme.  Empty return
    // = no project / copy failed.
    juce::String duplicateSample (const juce::File& src);

private:
    BaySickDAWProcessor& mProcessor;
    juce::File          mCurrentFolder;   // empty = no project open

    // ── Recent-projects list I/O ─────────────────────────────────────────────
    void pushRecentProject (const juce::File& folder);
    void saveSettings();
    void loadSettings();

    juce::Array<juce::File> mRecentProjects;   // cache; persisted on change
    bool                    mShortcutCreated { false };   // one-shot flag
    juce::File              mDefaultTemplate;             // P6, empty = no template
    bool                    mMigratedFromRoaming { false }; // P4b, one-shot
    // 2026-04-26: kit-view prompt opt-outs.  Lock is indexed by drum bank
    // (0 = drums 1-16, 1 = drums 17-32).
    bool                    mSkipGlobalLockPrompt[2] { false, false };
    bool                    mSkipKitReplacePrompt { false };
    bool                    mSkipCoreContentPrompt { false };   // content-delivery opt-out
    bool                    mSettingsWarnShown { false };   // once-per-session write-failure alert gate

    // ── P5 state ─────────────────────────────────────────────────────────────
    // QA-UndoCoverage Task 9: mDirty + setDirtyInternal retired -- see the
    // TransactionTracker on BaySickDAWProcessor.
    std::atomic<juce::uint32> mChangeStamp { 0 };   // TS7 §3.3
    bool                      mIgnoreDirty { false };   // true during load/save
    int               mAutosaveSec { 900 };     // 15 min default
    void timerCallback() override;              // autosave tick
    bool writeBackup();
    // Once-per-session autosave-failure alert gate; reset by the next
    // successful write so a recurring failure can resurface.
    bool              mAutosaveWarnShown { false };

    // The content-download offer is answered by a native message box that can
    // sit open for as long as the user leaves it there, and its callback writes
    // back to this object.  Weak-referenceable so that callback can tell whether
    // the manager is still alive rather than assuming it.
    JUCE_DECLARE_WEAK_REFERENCEABLE (ProjectManager)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectManager)
};
