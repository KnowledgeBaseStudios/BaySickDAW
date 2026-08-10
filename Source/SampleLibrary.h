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

    // Single source of truth for pack classification.  Folder name CONTAINS
    // "Drums" or "Percussion" (case-insensitive substring) -> drum, so the
    // shipped "Hip Hop Drums Package" / "Percussion Package" both match.
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

    // -- Core content install state (content delivery, 2026-08-09) ------------
    // listInstalledPackNames is the SINGLE enumerator for "which packs are on
    // this machine": scan() builds the browser lists from it and the content
    // installer tests completeness against it, so the two can never disagree
    // about what counts as a pack.  A name starting with '.' is installer
    // bookkeeping (marker / manifest), never a pack, and a folder still
    // carrying its in-progress marker is a half-installed pack, never content.
    static juce::StringArray listInstalledPackNames();

    // A pack counts as present only when it holds at least one file at any
    // depth.  An empty directory is exactly what an interrupted fetch leaves
    // behind, so "the folder exists" is not evidence of anything.  Stops at the
    // first file rather than walking a multi-GB tree.
    static bool isPackPopulated (const juce::File& packDir);

    // Planted inside a pack folder before the first file lands in it and
    // removed only once that pack is fully in place.  Same discipline as
    // adoptIntoUserSamples' incomplete-adoption marker, for the same reason: a
    // pack that arrives half-written has to be DETECTABLE, because content the
    // app then treats as complete is worse than content that is simply absent.
    // isPackPopulated cannot answer this on its own - the marker itself is a
    // file, so a folder holding nothing else still reads as populated.
    static juce::File getPackInstallMarkerFile (const juce::File& packDir);
    static bool       isPackInstallIncomplete  (const juce::File& packDir);

    // Planted before the first byte of a content fetch is written and removed
    // only once every pack the app knows about is installed.  Marker present at
    // launch = a fetch died part-way.
    static juce::File getCoreContentMarkerFile();

    // Which release asset produced which folder on this machine.  The two can
    // differ: an archive that carries its own top-level folder installs under
    // THAT name, not under the name derived from the asset.  Without the
    // record, a re-run cannot tell an installed pack from a missing one and
    // would refetch gigabytes it already has.
    static juce::File getCoreContentManifestFile();

    struct CoreContentPack
    {
        juce::String asset;    // release asset file name, e.g. "Keys.Package.zip"
        juce::String folder;   // folder it installed as, under CoreLibrary
    };

    struct CoreContentManifest
    {
        bool                         valid { false };   // false = no manifest on disk
        juce::Array<CoreContentPack> packs;
        juce::String                 sourceUrl;
    };
    static CoreContentManifest readCoreContentManifest();
    static bool                writeCoreContentManifest (const CoreContentManifest& m);

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
    // an empty string -- the caller keeps the ref it already had -- when the
    // source is missing, when it is a bare .sfz (a pointer into a sample tree,
    // deliberately never adopted; see the .cpp), when My Samples is
    // unavailable, or when the copy fails.
    // Dedupes on size + last-modified, mirroring ProjectManager::importSample,
    // against already-suffixed copies as well as the base name, then
    // auto-suffixes " (2)" so a user sample is never overwritten.
    // createdOut, when given, collects the file or folder this call NEWLY
    // copied (reuse and already-stable branches append nothing) -- so a caller
    // whose save can still be cancelled can undo exactly its own copies.  A
    // DIRECTORY copy that fails part-way deletes its own leftovers before
    // returning, and keeps its incomplete marker when the OS refuses that
    // delete, so a later save retries the folder rather than blessing a stump.
    // A part-way FILE copy is left on disk as an orphan and is never
    // referenced: it fails the size + modtime reuse test, so the next save
    // suffixes past it.
    static juce::String adoptIntoUserSamples (const juce::File& source,
                                              juce::Array<juce::File>* createdOut = nullptr);

private:
    SampleLibrary() = default;
    JUCE_DECLARE_NON_COPYABLE (SampleLibrary)

    std::vector<SamplePack> mDrumPacks;
    std::vector<SamplePack> mMelodicPacks;
    bool mScanned { false };
};
