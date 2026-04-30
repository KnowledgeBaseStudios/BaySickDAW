#pragma once
#include <JuceHeader.h>

// ── SampleInstrument ──────────────────────────────────────────────────────────
// A single loadable instrument found inside a pack subfolder.
// path is either a directory (sample folder) or a .sfz file.
struct SampleInstrument
{
    juce::String name;
    juce::File   path;
    bool         isSFZ { false };
};

// ── SamplePack ────────────────────────────────────────────────────────────────
// A top-level subfolder of CoreLibrary, containing SampleInstruments.
struct SamplePack
{
    juce::String                  name;
    std::vector<SampleInstrument> instruments;
};

// ── SampleLibrary ─────────────────────────────────────────────────────────────
// Singleton.  Call scan() once at startup.
//
// Root:  %LOCALAPPDATA%\BaySickDAW\CoreLibrary\
//   Each top-level subfolder = one pack.
//   Inside a pack:
//     subdirectory  → sample folder   (loaded via getManager().loadFolder())
//     *.sfz file    → SFZ instrument  (loaded via getManager().loadSFZ())
//
// Routing (2026-04-23): the in-app pack browser filters by page context.
//   Drum-page browser sees ONLY drum packs.
//   Layer / Bass browser sees ONLY melodic packs.
//   A pack is "drum" iff its top-level folder name contains "Drums"
//   OR contains "Percussion" (both case-insensitive substring matches).
//   Real folder names install as "Hip Hop Drums Package", "EDM Drums
//   Package", "Percussion Package" - all three substring-match.  Melodic
//   packs (Layer + Bass): everything else (Brass / Keys / Strings /
//   Woodwinds / etc.).
// ─────────────────────────────────────────────────────────────────────────────
class SampleLibrary
{
public:
    static SampleLibrary& getInstance();

    // Synchronously scans the CoreLibrary folder.  Safe to call on message thread.
    void scan();

    bool isScanned() const { return mScanned; }

    const std::vector<SamplePack>& getDrumPacks()    const { return mDrumPacks; }
    const std::vector<SamplePack>& getMelodicPacks() const { return mMelodicPacks; }

    // Single source of truth for pack classification.  Folder name contains
    // "Drums" (case-insensitive substring) OR equals "Percussion" -> drum.
    // Used by SampleLibrary::scan AND by editor menu builders to filter the
    // top-level pack enumeration based on page context.
    static bool isDrumPack (const juce::String& folderName);

    // Returns %LOCALAPPDATA%\BaySickDAW\CoreLibrary on Windows,
    // or ~/Library/Application Support/BaySickDAW/CoreLibrary on macOS.
    static juce::File getCoreLibraryDir();

    // G-6 (2026-04-29): user-facing samples folder under the main BaySickDAW
    // Documents tree (`Documents/BaySickDAW/My Samples`).  This is where
    // users put samples they want to pull from across projects.  The Clips
    // ribbon `+Add New Clip...` file picker defaults to this folder so users
    // see their own samples first; a Core Library shortcut lives inside so
    // they can drill into factory content without leaving the dialog.
    static juce::File getUserSamplesDir();
    // Creates the folder + Core Library shortcut if missing.  Idempotent —
    // safe to call repeatedly (and on every startup).
    static void       ensureUserSamplesDir();

private:
    SampleLibrary() = default;
    JUCE_DECLARE_NON_COPYABLE (SampleLibrary)

    std::vector<SamplePack> mDrumPacks;
    std::vector<SamplePack> mMelodicPacks;
    bool mScanned { false };
};
