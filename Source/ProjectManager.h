#pragma once
#include <JuceHeader.h>

class VibeSynthProcessor;
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
//   ├── audio_settings.xml               (TODO: move from Roaming APPDATA)
//   └── Sample Library.lnk               (P4/P6: shortcut to LocalAppData)
//
//   %LOCALAPPDATA%\BaySickDAW\
//   └── CoreLibrary\<pack>\              (installed sample library)
//
// Each project is a folder - never a single file.  project.xml.bak is an
// autosave sidecar written every 15 min (P5).  No path-reference mode - v1
// is bundled-only for beginner safety.
//
// Phase P1 scope: skeleton class only - methods exist, serialization works
// against VibeSynthProcessor::serializeProject/deserializeProject, but the UI
// (File menu / New/Open dialogs / autosave timer firing / dirty tracking) lands
// in later phases P2-P5.  Recent-projects list is persisted to
// %APPDATA%/BaySickDAW/settings.xml.
// ──────────────────────────────────────────────────────────────────────────────
class ProjectManager : private juce::Timer
{
public:
    ProjectManager (VibeSynthProcessor& processor);
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
    //   If templatePath is a valid project folder, its project.xml + Samples/
    //   get copied as the seed.  Returns true on success.  Fails if name is
    //   invalid or folder collision couldn't be auto-resolved.
    bool newProject (const juce::String& name,
                     const juce::File& templatePath = {});

    // openProject: loads an existing project folder.  Accepts either the
    // folder itself or the project.xml inside.
    bool openProject (const juce::File& folderOrXml);

    // saveProject: writes current state to <currentFolder>/project.xml.
    // No-op if no project is open.
    bool saveProject();

    // saveProjectAs: duplicates the current project folder to <root>/<newName>/
    // (pure GarageBand-style - user doesn't pick a location).  Switches the
    // "current" project to the new folder.  Returns true on success.
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
    void runFirstLaunchHousekeeping();

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
    void markDirty();
    void clearDirty();
    bool isDirty() const { return mDirty; }

    // onDirtyChanged fires when dirty transitions true<->false.  Wired by
    // StandaloneEditor to update the title-bar asterisk.
    std::function<void()> onDirtyChanged;

    // Autosave interval in seconds.  Default 15 minutes (900s) per the
    // project-persistence lock.  Setting 0 disables autosave entirely.
    void setAutosaveIntervalSeconds (int seconds);
    int  getAutosaveIntervalSeconds() const { return mAutosaveSec; }

    // ── P6: Default template ─────────────────────────────────────────────────
    // The default template (when set) is a project folder whose contents
    // (project.xml + Samples/) are copied as the seed for every new project.
    // Empty path = no template; new projects start blank.  Persisted in
    // settings.xml so the choice survives across runs.
    juce::File getDefaultTemplate() const   { return mDefaultTemplate; }
    void       setDefaultTemplate (const juce::File& folder);
    void       clearDefaultTemplate()       { setDefaultTemplate ({}); }

    // ── 2026-04-26: "Don't show again" prompt prefs ──────────────────────────
    // Two confirm-prompts on the kit view that the user can opt out of.
    // Both default to false (prompt shown).
    bool getSkipGlobalLockPrompt()  const { return mSkipGlobalLockPrompt;  }
    bool getSkipKitReplacePrompt()  const { return mSkipKitReplacePrompt;  }
    void setSkipGlobalLockPrompt (bool v);
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

private:
    VibeSynthProcessor& mProcessor;
    juce::File          mCurrentFolder;   // empty = no project open

    // ── Recent-projects list I/O ─────────────────────────────────────────────
    void pushRecentProject (const juce::File& folder);
    void saveSettings();
    void loadSettings();

    juce::Array<juce::File> mRecentProjects;   // cache; persisted on change
    bool                    mShortcutCreated { false };   // one-shot flag
    juce::File              mDefaultTemplate;             // P6, empty = no template
    bool                    mMigratedFromRoaming { false }; // P4b, one-shot
    bool                    mSkipGlobalLockPrompt { false };// 2026-04-26: kit-view prompt opt-outs
    bool                    mSkipKitReplacePrompt { false };

    // ── P5 state ─────────────────────────────────────────────────────────────
    std::atomic<bool> mDirty       { false };
    bool              mIgnoreDirty { false };   // true during load/save
    int               mAutosaveSec { 900 };     // 15 min default
    void setDirtyInternal (bool flag);
    void timerCallback() override;              // autosave tick
    bool writeBackup();                         // writes project.xml.bak

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectManager)
};
