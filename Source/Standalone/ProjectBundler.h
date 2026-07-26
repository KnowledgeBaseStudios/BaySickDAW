#pragma once

#include <JuceHeader.h>
#include <vector>

class PatternManager;
class VibeSynthProcessor;

// ─────────────────────────────────────────────────────────────────────────────
// ProjectBundler - QA-Export Task 4.
//
// Walks every external audio file a project depends on, then writes the project
// plus those files out as a single .zip or a plain folder.
//
// Deliberately a free-standing utility rather than dialog code: QA-ProjectSave's
// "Pack Project" reuses the same walker and writer, and a second copy of this
// logic would drift from this one.
//
// Message thread or a background thread - it does file IO and allocates, so
// never call it from an audio callback.
// ─────────────────────────────────────────────────────────────────────────────
namespace ProjectBundler
{
    // Where a referenced file actually lives.  This drives both what the user is
    // told and what SelfContained has to copy.
    enum class RefKind
    {
        ProjectRelative,   // already inside the project folder - travels for free
        UserSamples,       // Documents\BaySickDAW\My Samples
        CoreLibrary,       // shipped factory content
        Absolute,          // anywhere else on disk
        Missing            // stored path resolves to nothing
    };

    struct Reference
    {
        juce::String storedPath;   // exactly as persisted in the project
        juce::File   resolved;     // absolute location, or File() when Missing
        RefKind      kind { RefKind::Missing };
        juce::String origin;       // human-readable "where this came from"
    };

    enum class Mode  { Zip, Folder };

    // References     - copy what the project cannot find on another machine
    //                  (project-relative + My Samples + absolute), leaving Core
    //                  Library files as references since any install has them.
    // SelfContained  - additionally copy Core Library files in.
    enum class Scope { References, SelfContained };

    struct Result
    {
        bool              ok { false };
        juce::String      error;
        juce::StringArray missing;      // stored paths that resolved to nothing
        int               filesCopied { 0 };
    };

    // Enumerates the audio references reachable from the pattern data: the
    // project's audio library plus every arrangement block's audioFilePath,
    // across every pattern.
    //
    // KNOWN GAP: file references embedded inside ENGINE state (NAM captures, IR
    // files, per-engine sample paths) are not walked - they live inside opaque
    // per-engine state blobs rather than in PatternManager.  QA-Export Task 5
    // covers the NAM case specifically; a general engine-reference walk is not
    // in this batch.  Callers must not treat this list as "every file the
    // project needs".
    // Non-const PatternManager only because getBlock() has no const overload;
    // this reads and never mutates.
    std::vector<Reference> enumerate (PatternManager& pm,
                                      const VibeSynthProcessor& processor);

    // Writes the bundle.  `projectFolder` is the source project directory.
    // Missing files are REPORTED, never silently dropped.
    Result write (const std::vector<Reference>& refs,
                  const juce::File& projectFolder,
                  const juce::File& destination,
                  Mode mode,
                  Scope scope,
                  std::function<bool()> shouldAbort = {},
                  std::function<void(double)> onProgress = {});
}
