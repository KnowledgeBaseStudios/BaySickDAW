#include "SampleLibrary.h"

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
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
              .getChildFile ("BaySickDAW")
              .getChildFile ("My Samples");
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

    juce::Array<juce::File> packDirs;
    root.findChildFiles (packDirs, juce::File::findDirectories, false);
    packDirs.sort();

    for (const auto& packDir : packDirs)
    {
        SamplePack pack;
        pack.name = packDir.getFileName();

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
        return getCoreLibraryDir().getChildFile (
                   storedPath.substring ((int) juce::String (kLibraryPrefix).length()));
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

juce::String SampleLibrary::adoptIntoUserSamples (const juce::File& source)
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
        // trade for not deep-comparing trees on every save.
        if (dest.isDirectory()) return makeStableRef (dest);
        if (! dest.createDirectory().wasOk()) return {};
        if (! source.copyDirectoryTo (dest)) return {};
        return makeStableRef (dest);
    }

    // Same asset already adopted (matching size + modtime) -> reuse it rather
    // than growing a pile of near-identical copies across repeated saves.
    if (dest.existsAsFile()
        && dest.getSize() == source.getSize()
        && dest.getLastModificationTime() == source.getLastModificationTime())
    {
        return makeStableRef (dest);
    }

    // Different file, same name: auto-suffix.  Never overwrite a user sample.
    int n = 2;
    while (dest.existsAsFile())
    {
        dest = dir.getChildFile (source.getFileNameWithoutExtension()
                                 + " (" + juce::String (n++) + ")"
                                 + source.getFileExtension());
    }

    if (! source.copyFileTo (dest)) return {};
    return makeStableRef (dest);
}
