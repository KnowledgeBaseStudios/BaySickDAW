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
