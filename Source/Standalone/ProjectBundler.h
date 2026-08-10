#pragma once

#include <JuceHeader.h>
#include <vector>

class PatternManager;
class BaySickDAWProcessor;

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

    // QA-ProjectSave docket 22=b (2026-07-26): NEITHER scope copies Core Library
    // content any more.  Anyone who can open a BaySickDAW project has BaySickDAW
    // installed and therefore has the Core Library, so copying it in bought
    // nothing -- and once engine references are walked it would drag 555 MB-1 GB
    // sfizz product folders into every bundle.
    //
    // References     - files the project itself owns (its Samples folder).
    // SelfContained  - additionally copy My Samples + absolute references, i.e.
    //                  everything not guaranteed to exist on the target install.
    enum class Scope { References, SelfContained };

    struct Result
    {
        bool              ok { false };
        juce::String      error;
        juce::StringArray missing;      // stored paths that resolved to nothing
        // What did not make it into the bundle intact: a source that would not
        // copy, or a project.xml whose references could not be updated.
        juce::StringArray copyFailed;
        int               filesCopied { 0 };   // files this call added, not references to them
    };

    // Enumerates the audio references reachable from the pattern data: the
    // project's audio library plus every arrangement block's audioFilePath (the
    // arrangement is project-global, not per-pattern).
    //
    // Every file reference the project depends on.
    //
    // `tabsXml`, when supplied, is the <Tabs> element StandaloneEditor produces
    // for project/template save.  It is how engine-held references get walked:
    // clip paths and sfizz kit paths are plain attributes, NAM captures and user
    // IRs live in the Inst chain XML, and BaySickPlayer sample paths sit inside
    // base64 engine-state blobs that have to be decoded.  Passing nullptr limits
    // the result to PatternManager references (the pre-2026-07-26 behaviour).
    //
    // `rackStatesXml`, when supplied, is a BaySickGraph::saveRackStates snapshot.
    // Effects that hold user files (the Acoustic units' user IRs) can sit in
    // any bus or insert rack, and those live under project.xml's <Processor>
    // rather than <Tabs> -- without this the walk never sees them.
    //
    // Non-const PatternManager only because getBlock() has no const overload;
    // this reads and never mutates.
    std::vector<Reference> enumerate (PatternManager& pm,
                                      const BaySickDAWProcessor& processor,
                                      const juce::XmlElement* tabsXml = nullptr,
                                      const juce::XmlElement* rackStatesXml = nullptr);

    // Total bytes the bundle will copy under this scope, so the user is told the
    // size BEFORE a multi-hundred-MB write rather than after.
    juce::int64 estimateCopyBytes (const std::vector<Reference>& refs, Scope scope);

    // Writes the bundle.  `projectFolder` is the source project directory.
    // Missing files are REPORTED, never silently dropped.
    //
    // Copied files are RE-REFERENCED: the bundle's own project.xml is rewritten
    // so every file this call relocated into Samples/ is referenced at its new
    // location (including a " (N)" rename forced by a basename collision).
    // Without that the copies sit in the bundle referenced by nothing and the
    // destination machine reports them missing -- so a rewrite that could not
    // be completed lands in `copyFailed` rather than passing as a clean export.
    Result write (const std::vector<Reference>& refs,
                  const juce::File& projectFolder,
                  const juce::File& destination,
                  Mode mode,
                  Scope scope,
                  std::function<bool()> shouldAbort = {},
                  std::function<void(double)> onProgress = {});
}
