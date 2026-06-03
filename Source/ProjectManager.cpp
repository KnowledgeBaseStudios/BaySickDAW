#include "ProjectManager.h"
#include "PluginProcessor.h"
#include "ClipDropDiag.h"            // QA-ClipDrop: diagnostic trap (2026-06-02)

// ──────────────────────────────────────────────────────────────────────────────
// Path helpers
// ──────────────────────────────────────────────────────────────────────────────
juce::File ProjectManager::getDefaultProjectsRoot()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("BaySickDAW")
               .getChildFile ("Projects");
}

juce::File ProjectManager::getSettingsFile()
{
    // 2026-04-23: moved from Roaming APPDATA to Documents\BaySickDAW\ per the
    // unified user-folder layout - every user-visible artifact (projects,
    // recordings, presets, settings) lives under Documents\BaySickDAW\. Only
    // the CoreLibrary sample bundle stays in %LOCALAPPDATA% (too heavy to
    // round-trip through user-browsable paths + no reason to back it up with
    // user work).  audio_settings.xml still lives in Roaming APPDATA (written
    // by StandaloneApp::saveAudioSettings); that move is a separate task.
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("BaySickDAW")
               .getChildFile ("settings.xml");
}

// ──────────────────────────────────────────────────────────────────────────────
// Name validation (Windows filename rules + reserved device names)
// ──────────────────────────────────────────────────────────────────────────────
namespace
{
    static const juce::StringArray kReservedNames { "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };

    static const juce::String kIllegalChars = "<>:\"/\\|?*";
}

juce::String ProjectManager::sanitizeProjectName (const juce::String& raw)
{
    auto trimmed = raw.trim();
    // Trailing dots/spaces are illegal on Windows
    while (trimmed.isNotEmpty()
           && (trimmed.getLastCharacter() == '.' || trimmed.getLastCharacter() == ' '))
        trimmed = trimmed.dropLastCharacters (1);
    return trimmed;
}

bool ProjectManager::isValidProjectName (const juce::String& name)
{
    if (name.isEmpty()) return false;
    // Any illegal character rejects
    for (int i = 0; i < kIllegalChars.length(); ++i)
        if (name.containsChar (kIllegalChars[i])) return false;
    // Control characters
    for (int i = 0; i < name.length(); ++i)
        if (name[i] < 32) return false;
    // Reserved device names (case-insensitive, with or without extension)
    const auto upper = name.upToFirstOccurrenceOf (".", false, false).toUpperCase();
    if (kReservedNames.contains (upper)) return false;
    // Windows rejects names with length > 255
    if (name.length() > 255) return false;
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// Construction
// ──────────────────────────────────────────────────────────────────────────────
ProjectManager::ProjectManager (VibeSynthProcessor& processor)
    : mProcessor (processor)
{
    loadSettings();
    // Kick off autosave timer (15 min default).  Timer runs on message thread;
    // file I/O on project.xml is small + only fires when dirty so no real risk.
    if (mAutosaveSec > 0)
        startTimer (mAutosaveSec * 1000);
}

ProjectManager::~ProjectManager()
{
    stopTimer();
}

void ProjectManager::setAutosaveIntervalSeconds (int seconds)
{
    mAutosaveSec = juce::jmax (0, seconds);
    stopTimer();
    if (mAutosaveSec > 0) startTimer (mAutosaveSec * 1000);
}

void ProjectManager::setDirtyInternal (bool flag)
{
    const bool prev = mDirty.exchange (flag, std::memory_order_relaxed);
    if (prev != flag && onDirtyChanged) onDirtyChanged();
}

void ProjectManager::markDirty()
{
    if (mIgnoreDirty) return;
    setDirtyInternal (true);
}

void ProjectManager::clearDirty()
{
    setDirtyInternal (false);
}

juce::Array<juce::File> ProjectManager::listBackups() const
{
    juce::File dir;
    if (mCurrentFolder != juce::File())
        dir = mCurrentFolder.getChildFile ("Backups");
    else
        dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                  .getChildFile ("BaySickDAW")
                  .getChildFile ("Backups")
                  .getChildFile ("Unsaved");

    juce::Array<juce::File> out;
    if (! dir.isDirectory()) return out;
    for (const auto& f : juce::RangedDirectoryIterator (dir, false, "*.xml",
                                                          juce::File::findFiles))
        out.add (f.getFile());
    std::sort (out.begin(), out.end(),
               [](const juce::File& a, const juce::File& b)
               { return a.getLastModificationTime() > b.getLastModificationTime(); });
    return out;
}

bool ProjectManager::restoreBackup (const juce::File& backupXml)
{
    if (! backupXml.existsAsFile()) return false;
    auto parsed = juce::XmlDocument::parse (backupXml);
    if (parsed == nullptr) return false;

    if (hasProject())
    {
        // Preserve the current project.xml as a safety net before overwriting.
        auto live = mCurrentFolder.getChildFile ("project.xml");
        auto safety = mCurrentFolder.getChildFile ("project.xml.before-restore");
        if (live.existsAsFile()) live.copyFileTo (safety);
        // Copy the backup XML to project.xml + reload from disk so any path
        // resolution that happens during deserialize uses the new content.
        backupXml.copyFileTo (live);
    }

    // Replay state into memory.  mIgnoreDirty so the propertyChanged storm
    // during replaceState doesn't mark the freshly-restored project dirty.
    mIgnoreDirty = true;
    mProcessor.deserializeProject (*parsed);
    mIgnoreDirty = false;
    clearDirty();
    if (onDirtyChanged) onDirtyChanged();
    return true;
}

void ProjectManager::timerCallback()
{
    // Autosave on schedule regardless of dirty state - matches FL Studio
    // convention.  The dirty flag is still used for the close-with-unsaved
    // prompt + title-bar asterisk, just not as a gate on backup writes.
    // hasProject==false branch lands the backup under
    // Documents\BaySickDAW\Backups\Unsaved\ so unsaved sessions can also
    // recover from a crash.
    writeBackup();
}

bool ProjectManager::writeBackup()
{
    // Two paths: project open -> backup goes inside that project's Backups/.
    // No project open -> backup goes under Documents\BaySickDAW\Backups\Unsaved\
    // so the user can recover from an unsaved session after a crash.
    juce::File backupsDir;
    juce::String stem;

    if (hasProject())
    {
        backupsDir = mCurrentFolder.getChildFile ("Backups");
        stem = mCurrentFolder.getFileName();

        // Migrate legacy: any old `project.xml.bak` at the project root from
        // pre-2026-04-23 builds is silently removed (it was never user-loadable).
        auto legacy = mCurrentFolder.getChildFile ("project.xml.bak");
        if (legacy.existsAsFile()) legacy.deleteFile();
    }
    else
    {
        backupsDir = juce::File::getSpecialLocation (
                          juce::File::userDocumentsDirectory)
                          .getChildFile ("BaySickDAW")
                          .getChildFile ("Backups")
                          .getChildFile ("Unsaved");
        stem = "Untitled";
    }
    backupsDir.createDirectory();

    // Build filename: "<stem>_backup_YYYY-MM-DD_HH-MM.xml"
    const auto now = juce::Time::getCurrentTime();
    const juce::String stamp = now.formatted ("%Y-%m-%d_%H-%M");
    auto target = backupsDir.getChildFile (stem + "_backup_" + stamp + ".xml");

    // Same-minute collision: append " (N)".
    if (target.existsAsFile())
    {
        int n = 2;
        while (backupsDir.getChildFile (
                   stem + "_backup_" + stamp
                   + " (" + juce::String (n) + ").xml").existsAsFile())
            ++n;
        target = backupsDir.getChildFile (
                     stem + "_backup_" + stamp
                     + " (" + juce::String (n) + ").xml");
    }

    juce::XmlElement root ("BaySickDAWProject");
    mProcessor.serializeProject (root);
    if (! target.replaceWithText (root.toString())) return false;

    // Rolling retention: keep newest 10, delete the rest.
    juce::Array<juce::File> backups;
    for (const auto& f : juce::RangedDirectoryIterator (backupsDir, false, "*.xml",
                                                          juce::File::findFiles))
        backups.add (f.getFile());
    std::sort (backups.begin(), backups.end(),
               [](const juce::File& a, const juce::File& b)
               { return a.getLastModificationTime() > b.getLastModificationTime(); });
    for (int i = 10; i < backups.size(); ++i) backups[i].deleteFile();

    // Don't clearDirty - backups are a safety net; the user's real Save is
    // still pending.  Dirty remains until they explicitly Save.
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// Project lifecycle
// ──────────────────────────────────────────────────────────────────────────────
bool ProjectManager::newProject (const juce::String& name,
                                  const juce::File& templatePath)
{
    const auto sanitized = sanitizeProjectName (name);
    if (! isValidProjectName (sanitized)) return false;

    auto root = getDefaultProjectsRoot();
    root.createDirectory();

    // Collision handling: if <name>/ exists, append " (2)", " (3)", etc.
    auto folder = root.getChildFile (sanitized);
    if (folder.exists())
    {
        int suffix = 2;
        while (root.getChildFile (sanitized + " (" + juce::String (suffix) + ")").exists())
            ++suffix;
        folder = root.getChildFile (sanitized + " (" + juce::String (suffix) + ")");
    }

    if (! folder.createDirectory().wasOk()) return false;
    folder.getChildFile ("Samples").createDirectory();

    // Optional template seed - copy template's project.xml + Samples/ as start.
    if (templatePath.isDirectory() && templatePath != folder)
    {
        auto srcXml = templatePath.getChildFile ("project.xml");
        if (srcXml.existsAsFile())
            srcXml.copyFileTo (folder.getChildFile ("project.xml"));
        auto srcSamples = templatePath.getChildFile ("Samples");
        if (srcSamples.isDirectory())
            srcSamples.copyDirectoryTo (folder.getChildFile ("Samples"));
    }

    mCurrentFolder = folder;
    mProcessor.setCurrentProjectFolder (folder);
    pushRecentProject (folder);
    clearDirty();
    // Don't call saveProject here - caller decides when the first real save
    // happens.  An empty folder with just Samples/ is valid.
    return true;
}

bool ProjectManager::openProject (const juce::File& folderOrXml)
{
    // QA-D STATE-04: stop the transport before any load work begins so a
    // project opened mid-playback halts cleanly.  Hook is wired by
    // StandaloneEditor; no-op when null (e.g. headless tests).
    if (onBeforeOpenProject) onBeforeOpenProject();

    juce::File folder = folderOrXml.isDirectory()
                          ? folderOrXml
                          : folderOrXml.getParentDirectory();
    auto xml = folder.getChildFile ("project.xml");
    if (! xml.existsAsFile()) return false;

    auto parsed = juce::XmlDocument::parse (xml);
    if (parsed == nullptr) return false;

    mCurrentFolder = folder;
    mProcessor.setCurrentProjectFolder (folder);   // set BEFORE deserialize so
    // any path resolution that happens during state restore has the folder ready.
    mIgnoreDirty = true;
    mProcessor.deserializeProject (*parsed);
    mIgnoreDirty = false;
    pushRecentProject (folder);
    clearDirty();
    return true;
}

bool ProjectManager::saveProject()
{
    if (! hasProject()) return false;

    juce::XmlElement root ("BaySickDAWProject");
    mProcessor.serializeProject (root);

    auto xml = mCurrentFolder.getChildFile ("project.xml");
    const bool ok = xml.replaceWithText (root.toString());
    if (ok) clearDirty();
    return ok;
}

bool ProjectManager::saveProjectAs (const juce::String& newName)
{
    const auto sanitized = sanitizeProjectName (newName);
    if (! isValidProjectName (sanitized)) return false;

    auto root = getDefaultProjectsRoot();
    root.createDirectory();

    auto target = root.getChildFile (sanitized);
    if (target.exists())
    {
        int suffix = 2;
        while (root.getChildFile (sanitized + " (" + juce::String (suffix) + ")").exists())
            ++suffix;
        target = root.getChildFile (sanitized + " (" + juce::String (suffix) + ")");
    }

    // If a project is already open, duplicate its folder (preserves Samples/).
    if (hasProject())
    {
        if (! mCurrentFolder.copyDirectoryTo (target)) return false;
    }
    else
    {
        target.createDirectory();
        target.getChildFile ("Samples").createDirectory();
    }

    mCurrentFolder = target;
    mProcessor.setCurrentProjectFolder (target);
    // Write current in-memory state as the new folder's project.xml (may
    // differ from the copied file if the user made unsaved edits).
    saveProject();   // clears dirty
    pushRecentProject (target);
    return true;
}

bool ProjectManager::deleteProject (const juce::File& projectFolder)
{
    if (! projectFolder.isDirectory()) return false;
    return projectFolder.moveToTrash();
}

bool ProjectManager::renameProject (const juce::File& projectFolder,
                                     const juce::String& newName)
{
    const auto sanitized = sanitizeProjectName (newName);
    if (! isValidProjectName (sanitized)) return false;
    auto target = projectFolder.getParentDirectory().getChildFile (sanitized);
    if (target.exists()) return false;  // collision - caller retries with new name
    return projectFolder.moveFileTo (target);
}

void ProjectManager::runFirstLaunchHousekeeping()
{
    // Ensure Documents\BaySickDAW\ exists.
    auto docsRoot = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                        .getChildFile ("BaySickDAW");
    docsRoot.createDirectory();

    // P4b (2026-04-23): one-shot migration from Roaming APPDATA to Documents
    // for any user data that predates the unified-folder layout.  Guarded by
    // mMigratedFromRoaming so it only runs once per user.
    if (! mMigratedFromRoaming)
    {
        auto roamingRoot = juce::File::getSpecialLocation (
                              juce::File::userApplicationDataDirectory)
                              .getChildFile ("BaySickDAW");

        // Migrate audio_settings.xml (small file, simple move).
        auto oldAudio = roamingRoot.getChildFile ("audio_settings.xml");
        auto newAudio = docsRoot.getChildFile ("audio_settings.xml");
        if (oldAudio.existsAsFile() && ! newAudio.existsAsFile())
            oldAudio.moveFileTo (newAudio);

        // Migrate Presets/ folder (Roaming -> Documents).  Each engine
        // subfolder (BaySickSynth, BaySickBass, BaySickDrums, BaySickPlayer,
        // Harmless) gets moved together.
        auto oldPresets = roamingRoot.getChildFile ("Presets");
        auto newPresets = docsRoot.getChildFile ("Presets");
        if (oldPresets.isDirectory() && ! newPresets.isDirectory())
        {
            // copyDirectoryTo to preserve in case the move fails partway, then
            // delete original on success.
            if (oldPresets.copyDirectoryTo (newPresets))
                oldPresets.deleteRecursively();
        }

        mMigratedFromRoaming = true;
        saveSettings();
    }

    // Sample Library.lnk shortcut (one-shot).  Skip if the flag's already
    // set - respects users who chose to delete it.
    if (! mShortcutCreated)
    {
        auto link = docsRoot.getChildFile ("Sample Library.lnk");
        auto target = juce::File::getSpecialLocation (
                          juce::File::windowsLocalAppData)
                          .getChildFile ("BaySickDAW")
                          .getChildFile ("CoreLibrary");
        // Create target dir preemptively so the shortcut lands on a real path
        // even if the installer hasn't populated CoreLibrary/ yet.
        target.createDirectory();
        if (! link.existsAsFile())
            target.createShortcut ("BaySickDAW Sample Library", link);
        mShortcutCreated = true;
        saveSettings();
    }
}

juce::File ProjectManager::duplicateProject (const juce::File& projectFolder,
                                              const juce::String& newName)
{
    const auto sanitized = sanitizeProjectName (newName);
    if (! isValidProjectName (sanitized)) return {};
    auto root = projectFolder.getParentDirectory();
    auto target = root.getChildFile (sanitized);
    if (target.exists())
    {
        int n = 2;
        while (root.getChildFile (sanitized + " (" + juce::String (n) + ")").exists())
            ++n;
        target = root.getChildFile (sanitized + " (" + juce::String (n) + ")");
    }
    if (! projectFolder.copyDirectoryTo (target)) return {};
    return target;
}

std::vector<ProjectManager::Listing> ProjectManager::listProjects()
{
    std::vector<Listing> out;
    auto root = getDefaultProjectsRoot();
    if (! root.isDirectory()) return out;

    for (const auto& child : juce::RangedDirectoryIterator (root, false, "*",
                                                              juce::File::findDirectories))
    {
        auto folder = child.getFile();
        auto xml    = folder.getChildFile ("project.xml");
        if (! xml.existsAsFile()) continue;   // skip non-project folders

        Listing e;
        e.folder       = folder;
        e.name         = folder.getFileName();
        e.lastModified = xml.getLastModificationTime();
        // Folder size = sum of every contained file (Samples/ + project.xml).
        juce::int64 sz = 0;
        for (const auto& f : juce::RangedDirectoryIterator (folder, true, "*",
                                                              juce::File::findFiles))
            sz += f.getFile().getSize();
        e.sizeBytes = sz;
        out.push_back (std::move (e));
    }

    // Newest-first default.
    std::sort (out.begin(), out.end(),
               [](const Listing& a, const Listing& b) { return a.lastModified > b.lastModified; });
    return out;
}

// ──────────────────────────────────────────────────────────────────────────────
// Sample import (P4 - defined here so the API surface is complete in P1)
// ──────────────────────────────────────────────────────────────────────────────
juce::String ProjectManager::importSample (const juce::File& externalFile)
{
    if (! hasProject())                { ClipDropDiag::log ("importSample BAIL", "no project open; src=" + externalFile.getFullPathName()); return {}; }
    if (! externalFile.existsAsFile()) { ClipDropDiag::log ("importSample BAIL", "src does not exist; src=" + externalFile.getFullPathName()); return {}; }

    auto samplesDir = getSamplesFolder();
    samplesDir.createDirectory();

    auto target = samplesDir.getChildFile (externalFile.getFileName());
    // If same path, no copy needed.
    if (target == externalFile)
    {
        ClipDropDiag::log ("importSample OK", "src already in Samples/; rel=Samples/" + externalFile.getFileName());
        return "Samples/" + externalFile.getFileName();
    }

    // Filename collision: if the existing file has identical size + modtime,
    // assume it's the same asset and skip the copy.  Otherwise append " (N)".
    if (target.exists())
    {
        if (target.getSize() == externalFile.getSize()
            && target.getLastModificationTime() == externalFile.getLastModificationTime())
        {
            ClipDropDiag::log ("importSample OK", "collision, same size+modtime, reuse; rel=Samples/" + externalFile.getFileName());
            return "Samples/" + externalFile.getFileName();
        }

        const auto stem = externalFile.getFileNameWithoutExtension();
        const auto ext  = externalFile.getFileExtension();
        int n = 2;
        while (samplesDir.getChildFile (stem + " (" + juce::String (n) + ")" + ext).exists())
            ++n;
        target = samplesDir.getChildFile (stem + " (" + juce::String (n) + ")" + ext);
    }

    if (! externalFile.copyFileTo (target))
    {
        ClipDropDiag::log ("importSample BAIL", "copyFileTo FAILED; src=" + externalFile.getFullPathName()
                            + " target=" + target.getFullPathName()
                            + " samplesDir=" + samplesDir.getFullPathName()
                            + " samplesDirExists=" + (samplesDir.isDirectory() ? juce::String ("Y") : juce::String ("N")));
        return {};
    }
    ClipDropDiag::log ("importSample OK", "copied to Samples/; rel=Samples/" + target.getFileName());
    return "Samples/" + target.getFileName();
}

// QA-E Task 7 (FILE-02): force a fresh auto-numbered duplicate.  Unlike
// importSample, this never short-circuits to an existing path -- the
// Properties "Copy" action must always produce a NEW distinct file (so its
// own library entry is a separate source of truth, no identical-path dupes).
juce::String ProjectManager::duplicateSample (const juce::File& src)
{
    if (! hasProject())       return {};
    if (! src.existsAsFile()) return {};

    auto samplesDir = getSamplesFolder();
    samplesDir.createDirectory();

    const auto stem = src.getFileNameWithoutExtension();
    const auto ext  = src.getFileExtension();
    juce::File target;
    for (int n = 2; n < 100000; ++n)
    {
        target = samplesDir.getChildFile (stem + " (" + juce::String (n) + ")" + ext);
        if (! target.exists()) break;
    }
    if (target == juce::File() || target.exists()) return {};
    if (! src.copyFileTo (target))                 return {};
    return "Samples/" + target.getFileName();
}

// ──────────────────────────────────────────────────────────────────────────────
// Recent projects
// ──────────────────────────────────────────────────────────────────────────────
juce::Array<juce::File> ProjectManager::getRecentProjects() const
{
    return mRecentProjects;
}

void ProjectManager::clearRecentProjects()
{
    mRecentProjects.clear();
    saveSettings();
}

void ProjectManager::pushRecentProject (const juce::File& folder)
{
    mRecentProjects.removeAllInstancesOf (folder);
    mRecentProjects.insert (0, folder);
    while (mRecentProjects.size() > 10) mRecentProjects.removeLast();
    saveSettings();
}

void ProjectManager::loadSettings()
{
    auto f = getSettingsFile();
    if (! f.existsAsFile()) return;
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr) return;
    if (auto* recent = xml->getChildByName ("RecentProjects"))
    {
        for (auto* e : recent->getChildIterator())
        {
            if (! e->hasTagName ("Project")) continue;
            const auto path = e->getStringAttribute ("path");
            if (path.isNotEmpty()) mRecentProjects.add (juce::File (path));
        }
    }
    mShortcutCreated     = xml->getBoolAttribute ("shortcutCreated", false);
    mMigratedFromRoaming = xml->getBoolAttribute ("migratedFromRoaming", false);
    mSkipGlobalLockPrompt = xml->getBoolAttribute ("skipGlobalLockPrompt", false);
    mSkipKitReplacePrompt = xml->getBoolAttribute ("skipKitReplacePrompt", false);
    const auto tpl = xml->getStringAttribute ("defaultTemplate");
    if (tpl.isNotEmpty()) mDefaultTemplate = juce::File (tpl);
}

void ProjectManager::saveSettings()
{
    auto f = getSettingsFile();
    f.getParentDirectory().createDirectory();

    // Preserve any non-RecentProjects sections that other subsystems wrote.
    std::unique_ptr<juce::XmlElement> root;
    if (f.existsAsFile())
        root = juce::XmlDocument::parse (f);
    if (root == nullptr)
        root = std::make_unique<juce::XmlElement> ("BaySickDAWSettings");

    root->removeChildElement (root->getChildByName ("RecentProjects"), true);
    auto* recent = root->createNewChildElement ("RecentProjects");
    for (const auto& p : mRecentProjects)
    {
        auto* e = recent->createNewChildElement ("Project");
        e->setAttribute ("path", p.getFullPathName());
    }
    root->setAttribute ("shortcutCreated",     mShortcutCreated);
    root->setAttribute ("migratedFromRoaming", mMigratedFromRoaming);
    root->setAttribute ("skipGlobalLockPrompt", mSkipGlobalLockPrompt);
    root->setAttribute ("skipKitReplacePrompt", mSkipKitReplacePrompt);
    if (mDefaultTemplate != juce::File())
        root->setAttribute ("defaultTemplate", mDefaultTemplate.getFullPathName());
    else
        root->removeAttribute ("defaultTemplate");
    f.replaceWithText (root->toString());
}

void ProjectManager::setDefaultTemplate (const juce::File& folder)
{
    mDefaultTemplate = folder;
    saveSettings();
}

void ProjectManager::setSkipGlobalLockPrompt (bool v)
{
    if (mSkipGlobalLockPrompt == v) return;
    mSkipGlobalLockPrompt = v;
    saveSettings();
}

void ProjectManager::setSkipKitReplacePrompt (bool v)
{
    if (mSkipKitReplacePrompt == v) return;
    mSkipKitReplacePrompt = v;
    saveSettings();
}
