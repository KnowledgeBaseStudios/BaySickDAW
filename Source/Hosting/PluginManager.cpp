#include "PluginManager.h"
#include "OutOfProcessScanner.h"   // helper-process plugin scanning (QA-Cleanup)
#include "SandboxedPluginClient.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)
#include "../AppPaths.h"

namespace Hosting
{

// ── Persistence ──────────────────────────────────────────────────────────────
// Global preference, not project data -- the user's installed plugins do not
// change when they open a different song.  Sits beside settings.xml /
// audio_settings.xml / ui_prefs.xml at the app root rather than inside
// settings.xml, deliberately: settings.xml is re-parsed and rewritten whole on
// every window close (the smell the layout batch exists to remove) and adding
// a plugin database to that path would make the cleanup worse.

juce::File PluginManager::dataFile()
{
    return AppPaths::appRoot().getChildFile ("plugins.xml");
}

juce::File PluginManager::deadMansPedalFile()
{
    // JUCE writes the plugin currently under scan here and clears it on
    // success, so anything left behind is something that took the app down
    // mid-scan.  applyBlacklistingsFromDeadMansPedal turns that into a real
    // blacklist on the next run -- which is the whole crash-blacklist
    // requirement, already implemented upstream.
    return AppPaths::appRoot().getChildFile ("plugins_scan_crashes.txt");
}

// ── Construction ─────────────────────────────────────────────────────────────

PluginManager::PluginManager()
    : juce::Thread ("BaySick Plugin Scan")
{
    // Hard-coded rather than addDefaultFormats(): VST3 is the entire licensed
    // format scope (CL-303), and addDefaultFormats() would silently widen it
    // the moment another JUCE_PLUGINHOST_* flag is turned on for any reason.
    mFormats.addFormat (new juce::VST3PluginFormat());

    loadFromDisk();
    sInstance.store (this);
}

PluginManager* PluginManager::getInstance() noexcept
{
    return sInstance.load();
}

PluginManager::~PluginManager()
{
    auto* self = this;
    sInstance.compare_exchange_strong (self, nullptr);

    // Both bases outlive this destructor body, and both need shutting down
    // from here: juce::Thread asserts if it is still running when destroyed,
    // and a queued AsyncUpdater callback would fire into a half-dead object.
    signalThreadShouldExit();
    stopThread (4000);
    cancelPendingUpdate();
}

juce::AudioPluginFormat* PluginManager::vst3Format() const noexcept
{
    for (auto* f : mFormats.getFormats())
        if (f != nullptr && f->getName() == "VST3")
            return f;

    return nullptr;
}

// ── Section 1 — scan folders ─────────────────────────────────────────────────

juce::FileSearchPath PluginManager::defaultScanFolders()
{
    juce::VST3PluginFormat fmt;
    return fmt.getDefaultLocationsToSearch();
}

juce::FileSearchPath PluginManager::getScanFolders() const
{
    const juce::ScopedLock sl (mLock);
    return mScanFolders;
}

bool PluginManager::addScanFolder (const juce::File& f)
{
    if (! f.isDirectory())
        return false;

    {
        const juce::ScopedLock sl (mLock);

        for (int i = 0; i < mScanFolders.getNumPaths(); ++i)
            if (mScanFolders[i] == f)
                return false;

        mScanFolders.add (f);
    }

    saveToDisk();
    return true;
}

void PluginManager::removeScanFolder (int index)
{
    {
        const juce::ScopedLock sl (mLock);

        if (index < 0 || index >= mScanFolders.getNumPaths())
            return;

        mScanFolders.remove (index);
    }

    saveToDisk();
}

void PluginManager::resetScanFoldersToDefaults()
{
    {
        const juce::ScopedLock sl (mLock);
        mScanFolders = defaultScanFolders();
    }

    saveToDisk();
}

// ── Section 2 — the added list ───────────────────────────────────────────────

static void sortByName (juce::Array<juce::PluginDescription>& list)
{
    std::sort (list.begin(), list.end(),
               [] (const juce::PluginDescription& a, const juce::PluginDescription& b)
               {
                   const int byName = a.name.compareIgnoreCase (b.name);
                   return byName != 0 ? byName < 0
                                      : a.createIdentifierString() < b.createIdentifierString();
               });
}

juce::Array<juce::PluginDescription> PluginManager::getAddedPlugins() const
{
    juce::Array<juce::PluginDescription> out;

    {
        const juce::ScopedLock sl (mLock);
        out = mAdded.getTypes();
    }

    sortByName (out);
    return out;
}

juce::Array<juce::PluginDescription> PluginManager::getAddedEffects() const
{
    juce::Array<juce::PluginDescription> out;

    for (const auto& d : getAddedPlugins())
        if (! d.isInstrument)
            out.add (d);

    return out;
}

juce::Array<juce::PluginDescription> PluginManager::getAddedInstruments() const
{
    juce::Array<juce::PluginDescription> out;

    for (const auto& d : getAddedPlugins())
        if (d.isInstrument)
            out.add (d);

    return out;
}

void PluginManager::addToAddedList (const juce::Array<juce::PluginDescription>& toAdd)
{
    if (toAdd.isEmpty())
        return;

    {
        const juce::ScopedLock sl (mLock);

        for (const auto& d : toAdd)
            mAdded.addType (d);

        // Anything just added stops being an addable scan result, so section 3
        // reflects the move without needing a rescan.
        for (int i = mScanResults.size(); --i >= 0;)
        {
            const auto id = mScanResults.getReference (i).createIdentifierString();

            for (const auto& d : toAdd)
            {
                if (d.createIdentifierString() == id)
                {
                    mScanResults.remove (i);
                    break;
                }
            }
        }
    }

    saveToDisk();

    if (onAddedListChanged)
        onAddedListChanged();

    sendChangeMessage();
}

void PluginManager::removeFromAddedList (const juce::PluginDescription& d)
{
    {
        const juce::ScopedLock sl (mLock);
        mAdded.removeType (d);
    }

    saveToDisk();

    if (onAddedListChanged)
        onAddedListChanged();

    sendChangeMessage();
}

std::unique_ptr<juce::PluginDescription> PluginManager::findAdded (const juce::String& identifier) const
{
    const juce::ScopedLock sl (mLock);

    for (const auto& d : mAdded.getTypes())
        if (d.createIdentifierString() == identifier)
            return std::make_unique<juce::PluginDescription> (d);

    return {};
}

// ── Section 3 — scanning ─────────────────────────────────────────────────────

bool  PluginManager::isScanning()      const noexcept { return isThreadRunning(); }
bool  PluginManager::hasScanned()      const noexcept { return mHasScanned.load(); }
float PluginManager::getScanProgress() const noexcept { return mProgress.load(); }

juce::String PluginManager::getScanStatusText() const
{
    const juce::ScopedLock sl (mLock);
    return mStatusText;
}

juce::Array<juce::PluginDescription> PluginManager::getScanResults() const
{
    juce::Array<juce::PluginDescription> out;

    {
        const juce::ScopedLock sl (mLock);
        out = mScanResults;
    }

    sortByName (out);
    return out;
}

juce::Array<SkippedPlugin> PluginManager::getSkipped() const
{
    const juce::ScopedLock sl (mLock);
    return mSkipped;
}

void PluginManager::startScan()
{
    if (isThreadRunning())
        return;

    {
        const juce::ScopedLock sl (mLock);
        mScanResults.clear();
        mSkipped.clear();
        mStatusText = "Starting scan...";
    }

    mProgress.store (0.0f);
    startThread();
}

void PluginManager::cancelScan()
{
    signalThreadShouldExit();
}

PluginArch PluginManager::architectureOf (const juce::File& pluginPath)
{
    if (pluginPath.isDirectory())
    {
        // VST3 bundle: the real binary lives under Contents/<arch>-win/.  A
        // bundle may ship BOTH, in which case a 64-bit host loads the 64-bit
        // one -- so x86_64 is tested first and wins.
        const auto contents = pluginPath.getChildFile ("Contents");

        if (contents.getChildFile ("x86_64-win").isDirectory()) return PluginArch::X64;
        if (contents.getChildFile ("x86-win")   .isDirectory()) return PluginArch::X86;

        return PluginArch::Unknown;
    }

    juce::FileInputStream in (pluginPath);

    if (! in.openedOk())
        return PluginArch::Unknown;

    // PE/COFF layout, all little-endian: 'MZ' at 0; the PE header's file offset
    // is a 32-bit value at 0x3C; that header is "PE\0\0" followed immediately
    // by IMAGE_FILE_HEADER, whose first field is the 2-byte Machine id.
    // 0x014c = IMAGE_FILE_MACHINE_I386, 0x8664 = IMAGE_FILE_MACHINE_AMD64.
    if (in.readShort() != (short) 0x5a4d)
        return PluginArch::Unknown;

    if (! in.setPosition (0x3c))
        return PluginArch::Unknown;

    const auto peOffset = in.readInt();

    if (peOffset <= 0 || ! in.setPosition (peOffset))
        return PluginArch::Unknown;

    if (in.readInt() != 0x00004550)
        return PluginArch::Unknown;

    switch ((juce::uint16) in.readShort())
    {
        case 0x014c: return PluginArch::X86;
        case 0x8664: return PluginArch::X64;
        default:     return PluginArch::Unknown;
    }
}

void PluginManager::refineDescription (const juce::PluginDescription& d)
{
    bool changed = false;

    {
        const juce::ScopedLock sl (mLock);

        for (const auto& existing : mAdded.getTypes())
        {
            if (existing.fileOrIdentifier != d.fileOrIdentifier)
                continue;

            if (existing.isInstrument != d.isInstrument || existing.name != d.name)
            {
                mAdded.removeType (existing);
                mAdded.addType (d);
                changed = true;
            }
            break;
        }
    }

    if (changed)
    {
        saveToDisk();
        if (onAddedListChanged) onAddedListChanged();
        sendChangeMessage();
    }
}

void PluginManager::run()
{
    juce::FileSearchPath folders;
    juce::StringArray    alreadyAdded;

    {
        const juce::ScopedLock sl (mLock);
        folders = mScanFolders;

        for (const auto& d : mAdded.getTypes())
            alreadyAdded.add (d.createIdentifierString());
    }

    // The persisted list keeps folders that do not currently resolve
    // (loadFromDisk), so a scan filters them out here.  isDirectory() can block
    // for seconds on a dead network path, which is why it runs after mLock is
    // released rather than under it -- every public getter takes that lock.
    {
        juce::FileSearchPath available;

        for (int i = 0; i < folders.getNumPaths(); ++i)
            if (folders[i].isDirectory())
                available.add (folders[i]);

        folders = available;
    }

    auto* fmt = vst3Format();

    if (fmt == nullptr)
        return;

    juce::Array<SkippedPlugin> skipped;

    auto publish = [this] (const juce::String& status, float progress)
    {
        {
            const juce::ScopedLock sl (mLock);
            mStatusText = status;
        }

        mProgress.store (progress);
        triggerAsyncUpdate();
    };

    // ── Pass 1: VST2 candidates ─────────────────────────────────────────────
    // VST3PluginFormat only matches *.vst3, so a VST2 .dll is INVISIBLE to it
    // rather than failed -- it would never reach getFailedFiles() and the user
    // would get silence.  A .dll inside a folder the user nominated as a
    // plugin folder is a VST2 plugin for reporting purposes, which is exactly
    // the case Jeff called out.  RangedDirectoryIterator rather than
    // findChildFiles so a scan over a large tree stays cancellable.
    publish ("Looking for plugins...", 0.0f);

    for (int i = 0; i < folders.getNumPaths(); ++i)
    {
        if (threadShouldExit())
            return;

        for (const auto& entry : juce::RangedDirectoryIterator (folders[i], true, "*.dll",
                                                                juce::File::findFiles))
        {
            if (threadShouldExit())
                return;

            skipped.add ({ entry.getFile().getFullPathName(),
                           "Skipped: VST2 is not supported" });
        }
    }

    // ── Pass 2: split VST3 candidates by architecture BEFORE loading ────────
    const auto candidates = fmt->searchPathsForPlugins (folders, true, false);

    juce::StringArray loadable;
    juce::Array<juce::PluginDescription> bridged32;

    for (const auto& c : candidates)
    {
        if (threadShouldExit())
            return;

        if (architectureOf (juce::File (c)) == PluginArch::X86)
        {
            // INTAKE, not a skip (Jeff's ruling, 2026-07-31): skipping here
            // meant no 32-bit plugin could ever reach the added list, which
            // left the forced-bridge tier -- the bridge's whole reason to
            // exist -- with no entry point at all.  This 64-bit process cannot
            // open the file, so the description is minimal (name from the
            // filename); the first bridged load corrects it via LoadReply ->
            // refineDescription.
            const juce::File f (c);
            juce::PluginDescription d;
            d.name             = f.getFileNameWithoutExtension();
            d.descriptiveName  = d.name + " (32-bit, bridged)";
            d.pluginFormatName = "VST3";
            d.manufacturerName = "32-bit plugin";
            d.fileOrIdentifier = c;
            d.lastFileModTime  = f.getLastModificationTime();
            d.isInstrument     = false;
            bridged32.add (d);
        }
        else
            loadable.add (c);
    }

    // ── Pass 3: load the ones this process can actually load ────────────────
    juce::KnownPluginList found;
    juce::PluginDirectoryScanner::applyBlacklistingsFromDeadMansPedal (found, deadMansPedalFile());

    // Empty search path on purpose: the ctor would otherwise walk the folders a
    // SECOND time, and setFilesOrIdentifiersToScan replaces the result anyway.
    // OUT-OF-PROCESS SCAN (2026-08-10).  Every file below is loaded by a helper
    // process, not by us.  Two reasons, both real:
    //
    //   * JUCE asserts the VST3 scan path must run on the MESSAGE thread
    //     (juce_VST3PluginFormatImpl.h:299).  This function is a Thread::run,
    //     so we were breaking that on every scan - silently, because the assert
    //     compiles out in Release.  In the helper the scan runs on ITS message
    //     thread, which is where the contract is satisfied.
    //   * scanning LOADS the plugin, so one bad plugin used to take the whole
    //     DAW down and lose the user's work.  Now it takes down a helper we are
    //     happy to lose, JUCE's dead-man's pedal blacklists the file, and the
    //     scan carries on to the next one.
    //
    // The helper is chosen by ARCHITECTURE: a 32-bit plugin can only be scanned
    // by the 32-bit helper, the same reason it can only be loaded by one.
    found.setCustomScanner (std::make_unique<BridgedPluginScanner> (
        [] (const juce::File& pluginFile)
        {
            return SandboxedPluginClient::helperExecutable (
                       architectureOf (pluginFile) == PluginArch::X86);
        }));

    juce::PluginDirectoryScanner scanner (found, *fmt, juce::FileSearchPath(), true,
                                          deadMansPedalFile());
    scanner.setFilesOrIdentifiersToScan (loadable);

    juce::String nameBeingScanned;

    while (! threadShouldExit())
    {
        const auto next = scanner.getNextPluginFileThatWillBeScanned();

        // No DLL-directory scope here any more: the load happens in the helper
        // now, and the helper applies its own (PluginHostMain::onScanFile).
        if (! scanner.scanNextFile (true, nameBeingScanned))
            break;

        publish ("Scanning: " + (next.isNotEmpty() ? juce::File (next).getFileName() : nameBeingScanned),
                 scanner.getProgress());
    }

    if (threadShouldExit())
        return;

    for (const auto& f : scanner.getFailedFiles())
        skipped.add ({ f, "Skipped: failed to load" });

    for (const auto& b : found.getBlacklistedFiles())
        skipped.add ({ b, "Skipped: crashed during a previous scan" });

    // ── Publish ─────────────────────────────────────────────────────────────
    juce::Array<juce::PluginDescription> results;

    for (const auto& d : found.getTypes())
        if (! alreadyAdded.contains (d.createIdentifierString()))
            results.add (d);

    for (const auto& d : bridged32)
        if (! alreadyAdded.contains (d.createIdentifierString()))
            results.add (d);

    {
        const juce::ScopedLock sl (mLock);
        mScanResults = results;
        mSkipped     = skipped;
        mStatusText  = "Scan complete - " + juce::String (results.size()) + " found, "
                     + juce::String (skipped.size()) + " skipped";
    }

    mProgress.store (1.0f);
    mHasScanned.store (true);
    mPendingFinish.store (true);
    triggerAsyncUpdate();
}

void PluginManager::handleAsyncUpdate()
{
    if (onScanProgress)
        onScanProgress();

    if (mPendingFinish.exchange (false) && onScanFinished)
        onScanFinished();
}

// ── Disk ─────────────────────────────────────────────────────────────────────

void PluginManager::loadFromDisk()
{
    const juce::ScopedLock sl (mLock);

    mScanFolders = defaultScanFolders();

    const auto f = dataFile();

    if (! f.existsAsFile())
        return;

    const auto xml = SafeXml::parse (f);

    if (xml == nullptr)
        return;

    if (auto* folders = xml->getChildByName ("ScanFolders"))
    {
        juce::FileSearchPath restored;

        for (auto* e : folders->getChildWithTagNameIterator ("Folder"))
        {
            const auto path = e->getStringAttribute ("path");

            if (path.isNotEmpty())
                restored.add (juce::File (path));
        }

        // A folder that does not resolve at launch is KEPT.  An external drive
        // or network share that happens to be offline is still the user's
        // folder, and this list is not display-only: every add/remove gesture
        // rewrites the whole thing, so filtering here would let the next
        // unrelated gesture delete it from disk permanently.  run() drops the
        // unavailable ones for the duration of a scan instead.
        //
        // A saved-but-now-empty list stays empty: the user may have removed the
        // defaults on purpose, and re-seeding would undo that on every launch.
        // Only a file with no <ScanFolders> element at all keeps the default
        // seed above (saveToDisk always writes the element, so absence means
        // never-configured, not emptied).
        mScanFolders = restored;
    }

    if (auto* known = xml->getChildByName ("KNOWNPLUGINS"))
        mAdded.recreateFromXml (*known);
}

void PluginManager::saveToDisk() const
{
    juce::XmlElement root ("BaySickDAWPlugins");

    {
        const juce::ScopedLock sl (mLock);

        auto* folders = root.createNewChildElement ("ScanFolders");

        for (int i = 0; i < mScanFolders.getNumPaths(); ++i)
            folders->createNewChildElement ("Folder")
                   ->setAttribute ("path", mScanFolders[i].getFullPathName());

        if (auto known = mAdded.createXml())
            root.addChildElement (known.release());
    }

    dataFile().getParentDirectory().createDirectory();

    if (! root.writeTo (dataFile()) && ! mWarnedSaveFailure.exchange (true))
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
            "Plugin List Not Saved",
            "Could not save the plugin list to:\n" + dataFile().getFullPathName()
                + "\n\nAdded plugins and scan folders may be lost on the next launch.");
}

} // namespace Hosting
