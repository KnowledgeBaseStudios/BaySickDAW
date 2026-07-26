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
    // Creates the folder + Core Library shortcut if missing.  Idempotent -
    // safe to call repeatedly (and on every startup).
    static void       ensureUserSamplesDir();

    // ── Stable-root references (QA-ProjectSave Task 4, 2026-07-26) ───────────
    // A file living under Core Library or My Samples is reachable on any
    // install of this app, so it should be PERSISTED AS A REFERENCE rather than
    // copied into a project or written as an absolute path.  Absolute paths
    // embed the Windows user name and cannot resolve under another account;
    // copies duplicate factory content into every project that touches it.
    //
    // Wire format (matches the pre-existing DrumPage convention this
    // consolidates, so refs written before this batch still load):
    //     "library:<rel>"    - relative to getCoreLibraryDir()
    //     "mysamples:<rel>"  - relative to getUserSamplesDir()
    // <rel> always uses forward slashes regardless of platform.
    //
    // makeStableRef returns an EMPTY string when the file is under neither root
    // -- that is the caller's signal to fall back to copying (importSample) or
    // to an absolute path.  Callers must not invent their own prefix strings;
    // this pair is the single writer/reader so the two can never drift.
    static juce::String makeStableRef    (const juce::File& f);
    static juce::File   resolveStableRef (const juce::String& storedPath);
    static bool         isStableRef      (const juce::String& storedPath);

    // QA-ProjectSave Task 5 (2026-07-26): the pair every persist site uses.
    // refForPersist returns the stable reference when the file is under a
    // stable root and the absolute path otherwise, so a call site never has to
    // decide; resolvePersistedRef reverses it and also accepts the plain
    // absolute paths written before this batch.  Use these rather than
    // getFullPathName() anywhere a file path is written to disk.
    static juce::String refForPersist       (const juce::File& f);
    static juce::File   resolvePersistedRef (const juce::String& storedPath);

    // QA-ProjectSave Task 5 (2026-07-26, dockets 23/24): adopt a volatile file
    // into My Samples and return its "mysamples:" ref, so a TEMPLATE stops
    // depending on wherever the user happened to drag the file in from.
    //
    // A template is one XML with no folder beside it, so a reference to
    // Downloads (or worse, to some OTHER project's Samples folder) silently
    // couples every project made from that template to a path that can vanish.
    //
    // No-ops into a plain ref when the file is ALREADY under a stable root --
    // Core Library content in particular is never copied, since a 555 MB-1 GB
    // sfizz product folder per template is not a trade worth making.  Returns
    // an empty string if the source does not exist or the copy fails.
    // Dedupes on size + last-modified, mirroring ProjectManager::importSample,
    // then auto-suffixes " (2)" so a user sample is never overwritten.
    static juce::String adoptIntoUserSamples (const juce::File& source);

private:
    SampleLibrary() = default;
    JUCE_DECLARE_NON_COPYABLE (SampleLibrary)

    std::vector<SamplePack> mDrumPacks;
    std::vector<SamplePack> mMelodicPacks;
    bool mScanned { false };
};
