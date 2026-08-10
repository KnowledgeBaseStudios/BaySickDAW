#include "SampleLibrary.h"
#include "AppPaths.h"
#include "MissingFileReport.h"   // content-delivery: diagnose an absent pack once

namespace
{
    // Dot-prefixed so the pack enumeration skips them and the OS file picker
    // hides them.  Both live at the CoreLibrary root, never inside a pack.
    constexpr const char* kCoreContentMarkerName   = ".baysick-content-incomplete";
    constexpr const char* kCoreContentManifestName = ".baysick-content.xml";

    // Lives INSIDE a pack folder, so it travels with the folder when the
    // installer moves it into place.
    constexpr const char* kPackInstallMarkerName   = ".baysick-pack-incomplete";
}

SampleLibrary& SampleLibrary::getInstance()
{
    static SampleLibrary instance;
    return instance;
}

juce::File SampleLibrary::getCoreLibraryDir()
{
#if JUCE_WINDOWS
    const juce::String localAppData =
        juce::SystemStats::getEnvironmentVariable ("LOCALAPPDATA", {});
    if (localAppData.isNotEmpty())
        return juce::File (localAppData).getChildFile ("BaySickDAW/CoreLibrary");
#endif
    // macOS / Linux fallback
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("BaySickDAW/CoreLibrary");
}

juce::File SampleLibrary::getUserSamplesDir()
{
    return AppPaths::appRoot().getChildFile ("My Samples");
}

void SampleLibrary::ensureUserSamplesDir()
{
    auto dir = getUserSamplesDir();
    if (! dir.isDirectory())
        dir.createDirectory();

    // Place a shortcut to the Core Library inside My Samples so the user
    // can navigate from the OS file picker into factory content with one
    // click, without leaving the dialog.  Windows: .lnk; macOS: alias;
    // Linux: not supported by JUCE (silently skipped).
   #if JUCE_WINDOWS
    const juce::String linkName = "Core Library.lnk";
   #else
    const juce::String linkName = "Core Library";   // macOS alias has no extension
   #endif
    const auto shortcut = dir.getChildFile (linkName);
    if (! shortcut.exists())
    {
        const auto coreLib = getCoreLibraryDir();
        if (coreLib.isDirectory())
            coreLib.createShortcut ("BaySickDAW Core Library", shortcut);
    }
}

bool SampleLibrary::isDrumPack (const juce::String& folderName)
{
    // Drum heuristic: substring match (case-insensitive) on either "Drums"
    // or "Percussion".  Substring (not equals) so the actual installed
    // folder names - "Hip Hop Drums Package", "EDM Drums Package",
    // "Percussion Package" - all match.  Layer / Bass-page packs: every
    // other pack (Brass / Keys / Strings / Woodwinds / etc.).
    if (folderName.containsIgnoreCase ("Drums"))      return true;
    if (folderName.containsIgnoreCase ("Percussion")) return true;
    return false;
}

juce::StringArray SampleLibrary::listInstalledPackNames()
{
    juce::StringArray out;

    const auto root = getCoreLibraryDir();
    if (! root.isDirectory()) return out;

    juce::Array<juce::File> packDirs;
    root.findChildFiles (packDirs, juce::File::findDirectories, false);

    for (const auto& d : packDirs)
    {
        const auto name = d.getFileName();
        if (name.startsWithChar ('.')) continue;   // installer bookkeeping
        if (isPackInstallIncomplete (d))  continue;   // half-installed, not content
        out.add (name);
    }

    out.sort (true);
    return out;
}

bool SampleLibrary::isPackPopulated (const juce::File& packDir)
{
    if (! packDir.isDirectory()) return false;

    // First hit wins.  A full recursive listing of a multi-GB pack runs at every
    // launch through the completeness check, and the question here is only
    // "is there anything in here at all".
    for (const auto& it : juce::RangedDirectoryIterator (packDir, true, "*",
                                                          juce::File::findFiles))
    {
        juce::ignoreUnused (it);
        return true;
    }
    return false;
}

juce::File SampleLibrary::getPackInstallMarkerFile (const juce::File& packDir)
{
    return packDir.getChildFile (kPackInstallMarkerName);
}

bool SampleLibrary::isPackInstallIncomplete (const juce::File& packDir)
{
    return getPackInstallMarkerFile (packDir).existsAsFile();
}

juce::File SampleLibrary::getCoreContentMarkerFile()
{
    return getCoreLibraryDir().getChildFile (kCoreContentMarkerName);
}

juce::File SampleLibrary::getCoreContentManifestFile()
{
    return getCoreLibraryDir().getChildFile (kCoreContentManifestName);
}

SampleLibrary::CoreContentManifest SampleLibrary::readCoreContentManifest()
{
    CoreContentManifest m;

    const auto f = getCoreContentManifestFile();
    if (! f.existsAsFile()) return m;

    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("BaySickCoreContent")) return m;

    m.sourceUrl = xml->getStringAttribute ("sourceUrl");

    for (auto* e : xml->getChildIterator())
    {
        if (! e->hasTagName ("Pack")) continue;

        CoreContentPack p;
        p.asset  = e->getStringAttribute ("asset");
        p.folder = e->getStringAttribute ("folder");
        if (p.asset.isNotEmpty() && p.folder.isNotEmpty()) m.packs.add (p);
    }

    m.valid = true;
    return m;
}

bool SampleLibrary::writeCoreContentManifest (const CoreContentManifest& m)
{
    juce::XmlElement xml ("BaySickCoreContent");
    xml.setAttribute ("sourceUrl", m.sourceUrl);

    for (const auto& p : m.packs)
    {
        auto* e = xml.createNewChildElement ("Pack");
        e->setAttribute ("asset",  p.asset);
        e->setAttribute ("folder", p.folder);
    }

    const auto f = getCoreContentManifestFile();
    f.getParentDirectory().createDirectory();
    return f.replaceWithText (xml.toString());
}

void SampleLibrary::scan()
{
    mDrumPacks.clear();
    mMelodicPacks.clear();
    mScanned = false;

    const auto root = getCoreLibraryDir();
    if (!root.isDirectory())
    {
        mScanned = true;
        return;
    }

    for (const auto& packName : listInstalledPackNames())
    {
        const auto packDir = root.getChildFile (packName);

        SamplePack pack;
        pack.name = packName;

        // Each child: subdirectory → sample folder,  *.sfz → SFZ instrument
        juce::Array<juce::File> items;
        packDir.findChildFiles (items, juce::File::findFilesAndDirectories, false);
        items.sort();

        for (const auto& item : items)
        {
            if (item.isDirectory())
            {
                pack.instruments.push_back ({ item.getFileName(), item, false });
            }
            else if (item.hasFileExtension ("sfz"))
            {
                pack.instruments.push_back ({ item.getFileNameWithoutExtension(), item, true });
            }
        }

        if (isDrumPack (pack.name))
            mDrumPacks.push_back (std::move (pack));
        else
            mMelodicPacks.push_back (std::move (pack));
    }

    mScanned = true;
}

// ── Stable-root references (QA-ProjectSave Task 4, 2026-07-26) ───────────────
namespace
{
    constexpr const char* kLibraryPrefix   = "library:";
    constexpr const char* kMySamplesPrefix = "mysamples:";

    // Forward slashes on the wire so a ref written on one platform reads on any
    // other, and so the stored string is stable across JUCE separator handling.
    juce::String relativeUnder (const juce::File& f, const juce::File& root)
    {
        if (! root.isDirectory() || ! f.isAChildOf (root)) return {};
        return f.getRelativePathFrom (root).replaceCharacter ('\\', '/');
    }
}

juce::String SampleLibrary::makeStableRef (const juce::File& f)
{
    if (f == juce::File()) return {};

    if (auto rel = relativeUnder (f, getCoreLibraryDir()); rel.isNotEmpty())
        return kLibraryPrefix + rel;
    if (auto rel = relativeUnder (f, getUserSamplesDir()); rel.isNotEmpty())
        return kMySamplesPrefix + rel;

    return {};   // not under a stable root - caller copies or stores absolute
}

juce::File SampleLibrary::resolveStableRef (const juce::String& storedPath)
{
    if (storedPath.startsWith (kLibraryPrefix))
    {
        const auto rel = storedPath.substring ((int) juce::String (kLibraryPrefix).length());
        const auto f   = getCoreLibraryDir().getChildFile (rel);

        // Every caller already reports the FILE it failed to load; none of them
        // can say WHY, so a machine with no content packs installed shows the
        // user forty missing-file lines and no cause.  When the pack the
        // reference points into is not installed at all, bank one extra line
        // naming the pack.  MissingFileReport dedupes on (what, path), so forty
        // references across three absent packs add three lines, not forty.
        //
        // Costs one stat per library reference.  Every call site is state
        // restore or editor code (never the audio thread), and the load that
        // follows dwarfs it.
        if (! f.exists())
        {
            const auto pack = rel.upToFirstOccurrenceOf ("/", false, false);
            if (pack.isNotEmpty() && ! getCoreLibraryDir().getChildFile (pack).isDirectory())
                MissingFileReport::add ("Content pack not installed", pack);
        }

        return f;
    }
    if (storedPath.startsWith (kMySamplesPrefix))
        return getUserSamplesDir().getChildFile (
                   storedPath.substring ((int) juce::String (kMySamplesPrefix).length()));
    return {};
}

bool SampleLibrary::isStableRef (const juce::String& storedPath)
{
    return storedPath.startsWith (kLibraryPrefix)
        || storedPath.startsWith (kMySamplesPrefix);
}

juce::String SampleLibrary::refForPersist (const juce::File& f)
{
    if (f == juce::File()) return {};
    if (auto ref = makeStableRef (f); ref.isNotEmpty()) return ref;
    return f.getFullPathName();
}

juce::File SampleLibrary::resolvePersistedRef (const juce::String& storedPath)
{
    if (storedPath.isEmpty()) return {};
    if (isStableRef (storedPath)) return resolveStableRef (storedPath);
    return juce::File (storedPath);   // absolute, incl. everything written pre-batch
}

namespace
{
    // Planted inside a My Samples folder while its copy is in flight, removed
    // once the copy completes.  juce::File::copyDirectoryTo abandons its walk at
    // the first child it cannot copy and leaves the earlier ones on disk, and a
    // crash mid-copy leaves the same debris - so "the folder exists" cannot mean
    // "the folder is a complete adoption".  This marker is the O(1) stand-in for
    // the deep tree compare adoptIntoUserSamples deliberately refuses to do.
    constexpr const char* kAdoptInProgressMarker = ".baysick-adopt-incomplete";

    // juce::File::deleteRecursively ANDs its result across every child instead of
    // stopping at the first failure, so a payload the OS refuses to remove leaves
    // a partial tree behind while the marker - our own short-path, default-
    // attribute file - deletes cleanly.  Win32 CopyFile propagates
    // FILE_ATTRIBUTE_READONLY from source to destination and juce::File::deleteFile
    // is a bare DeleteFile with no attribute clear, so a source library off
    // read-only media reproduces that split every time.  Re-planting the marker
    // keeps the stump flagged incomplete rather than letting the next save read it
    // as a finished adoption.
    bool wipeIncompleteAdoption (const juce::File& dest, const juce::File& marker)
    {
        if (dest.deleteRecursively())
            return true;

        marker.create();
        return false;
    }
}

juce::String SampleLibrary::adoptIntoUserSamples (const juce::File& source,
                                                  juce::Array<juce::File>* createdOut)
{
    if (source == juce::File() || ! source.exists()) return {};

    // Already reachable from any project on this install - reference it as-is.
    // This is the branch that keeps sfizz kits (all Core Library) out of the copy.
    if (auto existing = makeStableRef (source); existing.isNotEmpty())
        return existing;

    // A bare .sfz is a POINTER into a sample tree, not a self-contained asset:
    // its `sample=` opcodes resolve relative to its own folder, so copying the
    // file alone yields a reference to nothing.  Adopting it properly would mean
    // dragging the whole surrounding library, which is the exact reason docket 24
    // excludes sfizz kits.  Same reasoning, same answer - left as an absolute
    // reference and reported by the bundler if it ever goes missing.
    if (source.hasFileExtension ("sfz")) return {};

    ensureUserSamplesDir();
    const auto dir = getUserSamplesDir();
    if (! dir.isDirectory()) return {};

    auto dest = dir.getChildFile (source.getFileName());

    if (source.isDirectory())
    {
        // BaySickPlayer folder load.  A directory of the same name is treated as
        // the same asset so repeated template saves do not pile up copies; two
        // genuinely different folders sharing a name would collide, which is the
        // trade for not deep-comparing trees on every save.  That trade does not
        // extend to a folder THIS function abandoned part-way - the marker tells
        // the two apart, so an incomplete copy is redone rather than blessed as
        // a complete adoption and referenced by a template.
        const auto marker = dest.getChildFile (kAdoptInProgressMarker);

        if (dest.isDirectory())
        {
            if (! marker.existsAsFile()) return makeStableRef (dest);

            // A directory that could not be emptied would mix leftovers from the
            // abandoned copy into the new one, so the retry is refused outright.
            if (! wipeIncompleteAdoption (dest, marker)) return {};
        }

        if (! dest.createDirectory().wasOk()) return {};

        // Marker before the first child copied, cleanup before every failure
        // exit: anything left in My Samples while returning {} is untracked by
        // createdOut, and without the marker the next save reads it as a
        // finished adoption.
        if (! marker.create().wasOk() || ! source.copyDirectoryTo (dest))
        {
            wipeIncompleteAdoption (dest, marker);
            return {};
        }

        marker.deleteFile();
        if (createdOut != nullptr) createdOut->add (dest);
        return makeStableRef (dest);
    }

    // Same asset already adopted (matching size + modtime) -> reuse it rather
    // than growing a pile of near-identical copies across repeated saves.  The
    // test has to run against every suffixed candidate, not just the base name:
    // existence alone treats the copy the LAST save made as an unrelated
    // occupant, so each re-save clones the audio again under the next free
    // " (N)" and orphans the previous one with no reclaim path.
    // Different file, same name: auto-suffix.  Never overwrite a user sample.
    int n = 2;
    while (dest.existsAsFile())
    {
        if (dest.getSize() == source.getSize()
            && dest.getLastModificationTime() == source.getLastModificationTime())
        {
            return makeStableRef (dest);
        }

        dest = dir.getChildFile (source.getFileNameWithoutExtension()
                                 + " (" + juce::String (n++) + ")"
                                 + source.getFileExtension());
    }

    if (! source.copyFileTo (dest)) return {};
    if (createdOut != nullptr) createdOut->add (dest);
    return makeStableRef (dest);
}
