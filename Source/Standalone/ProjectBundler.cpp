#include "ProjectBundler.h"
#include "../PatternManager.h"
#include "../PluginProcessor.h"
#include "../SampleLibrary.h"

namespace ProjectBundler
{
namespace
{
    bool isUnder (const juce::File& f, const juce::File& dir)
    {
        return dir != juce::File() && f != juce::File() && f.isAChildOf (dir);
    }

    RefKind classify (const juce::File& resolved, const juce::File& projectFolder)
    {
        if (resolved == juce::File() || ! resolved.existsAsFile()) return RefKind::Missing;
        if (isUnder (resolved, projectFolder))                     return RefKind::ProjectRelative;
        if (isUnder (resolved, SampleLibrary::getUserSamplesDir()))return RefKind::UserSamples;
        if (isUnder (resolved, SampleLibrary::getCoreLibraryDir()))return RefKind::CoreLibrary;
        return RefKind::Absolute;
    }

    void addRef (std::vector<Reference>& out,
                 const juce::String& storedPath,
                 const juce::String& origin,
                 const VibeSynthProcessor& proc,
                 const juce::File& projectFolder)
    {
        if (storedPath.isEmpty()) return;

        // Dedupe on the stored string: the same file referenced by twenty blocks
        // should be copied once and reported once.
        for (const auto& r : out)
            if (r.storedPath == storedPath)
                return;

        Reference ref;
        ref.storedPath = storedPath;
        ref.resolved   = proc.resolveProjectFile (storedPath);
        ref.kind       = classify (ref.resolved, projectFolder);
        ref.origin     = origin;
        out.push_back (std::move (ref));
    }

    // Files the destination machine will not have unless we bring them.
    // Docket 22=b: Core Library NEVER copies -- any install that can open the
    // project already has it, and copying it would mean GB-scale bundles once
    // engine references (sfizz kits) are part of the walk.
    bool needsCopying (RefKind k, Scope scope)
    {
        switch (k)
        {
            case RefKind::UserSamples:
            case RefKind::Absolute:        return scope == Scope::SelfContained;
            case RefKind::CoreLibrary:     return false;
            case RefKind::ProjectRelative: return false;   // inside the folder already
            case RefKind::Missing:         return false;
        }
        return false;
    }

    // ── Engine-held references (QA-ProjectSave Task 6, docket 21) ────────────
    // Attributes that name a file in engine state.  Kept explicit rather than
    // "any attribute that looks like a path": a generic guess would report
    // false positives as Missing and train the user to ignore the warning.
    const char* const kPathAttrs[] = {
        "bsp_loadPath",     // VibePlayer / BaySickPlayer sample, folder or SFZ
        "kitPath",          // sfizz Guitars / Basses
        "namPath", "irPath",
        "micUserIrPath", "micbUserIrPath",
    };

    void walkXmlForPaths (const juce::XmlElement& el,
                          const std::function<void (const juce::String&, const juce::String&)>& found)
    {
        for (const char* attr : kPathAttrs)
            if (auto v = el.getStringAttribute (attr); v.isNotEmpty())
                found (v, attr);

        // <KitPath path="..."/> and <Sample path="..."/> use a generic "path",
        // which is only safe to read on those specific tags.
        if (el.hasTagName ("KitPath") || el.hasTagName ("Sample"))
            if (auto v = el.getStringAttribute ("path"); v.isNotEmpty())
                found (v, el.getTagName());

        for (auto* child : el.getChildIterator())
            walkXmlForPaths (*child, found);
    }

    // engineData / sfizzEngineData are base64 of AudioProcessor::getStateInformation,
    // which is copyXmlToBinary -- decode back to XML and walk it.
    void walkEngineBlob (const juce::String& base64,
                         const std::function<void (const juce::String&, const juce::String&)>& found)
    {
        if (base64.isEmpty()) return;
        juce::MemoryBlock mb;
        if (! mb.fromBase64Encoding (base64)) return;
        if (auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()))
            walkXmlForPaths (*xml, found);
    }
}

juce::int64 estimateCopyBytes (const std::vector<Reference>& refs, Scope scope)
{
    juce::int64 total = 0;
    for (const auto& r : refs)
        if (needsCopying (r.kind, scope) && r.resolved.existsAsFile())
            total += r.resolved.getSize();
    return total;
}

std::vector<Reference> enumerate (PatternManager& pm,
                                  const VibeSynthProcessor& processor,
                                  const juce::XmlElement* tabsXml)
{
    std::vector<Reference> refs;
    const juce::File projectFolder = processor.getCurrentProjectFolder();

    // 1. The project's audio library (the Browser's clip list).
    for (int i = 0; i < pm.getNumAudioLibrary(); ++i)
        addRef (refs, pm.getAudioLibraryPath (i), "Audio library", processor, projectFolder);

    // 2. Every arrangement block carrying an audio path.  The arrangement is
    //    project-global, not per-pattern.  Both passes are needed: a clip can
    //    sit in the arrangement without being in the library, and vice versa.
    for (int i = 0; i < pm.getNumBlocks(); ++i)
    {
        const auto& blk = pm.getBlock (i);
        addRef (refs, blk.audioFilePath, "Arrangement", processor, projectFolder);
    }

    // 3. Engine-held references (QA-ProjectSave Task 6, docket 21).  Before this
    //    the walk stopped at PatternManager, so a bundle silently shipped without
    //    NAM captures, user IRs or engine-loaded sample folders and reported
    //    nothing missing -- it looked like a clean export.
    if (tabsXml != nullptr)
    {
        for (auto* rec : tabsXml->getChildWithTagNameIterator ("Tab"))
        {
            const auto tabName = rec->getStringAttribute ("name",
                                     rec->getStringAttribute ("type", "Tab"));

            auto found = [&] (const juce::String& stored, const juce::String& what)
            {
                addRef (refs, stored, tabName + " (" + what + ")", processor, projectFolder);
            };

            // Plain attributes on the tab record itself.
            walkXmlForPaths (*rec, found);

            // The Inst chain XML is stored as a string attribute, not a child.
            if (auto chain = rec->getStringAttribute ("instChainState"); chain.isNotEmpty())
                if (auto parsed = juce::XmlDocument::parse (chain))
                    walkXmlForPaths (*parsed, found);

            // Base64 engine state - BaySickPlayer sample paths live here, and
            // nowhere else reachable.
            walkEngineBlob (rec->getStringAttribute ("engineData"),      found);
            walkEngineBlob (rec->getStringAttribute ("sfizzEngineData"), found);
        }
    }

    return refs;
}

Result write (const std::vector<Reference>& refs,
              const juce::File& projectFolder,
              const juce::File& destination,
              Mode mode,
              Scope scope,
              std::function<bool()> shouldAbort,
              std::function<void(double)> onProgress)
{
    Result result;

    if (projectFolder == juce::File() || ! projectFolder.isDirectory())
    {
        result.error = "This project has not been saved to disk yet, so there is nothing to bundle.";
        return result;
    }
    if (destination == juce::File())
    {
        result.error = "No destination was chosen.";
        return result;
    }

    for (const auto& r : refs)
        if (r.kind == RefKind::Missing)
            result.missing.add (r.storedPath);

    // Everything lands under Samples/ inside the bundle, matching where the
    // project already keeps its own audio.
    const juce::String kSamplesDir = "Samples";
    // TS7 §6.8: the freeze cache is regenerable and excluded from bundles.
    const juce::String kFreezeDir  = "Freeze";

    // True for anything the bundle deliberately leaves behind.  Matches on the
    // path RELATIVE to the project folder, so a user's own "Freeze" folder
    // nested somewhere else is not caught by accident.
    auto isExcludedFromBundle = [&kFreezeDir] (const juce::File& f,
                                               const juce::File& projectFolder)
    {
        const juce::String rel = f.getRelativePathFrom (projectFolder);
        return rel.startsWith (kFreezeDir + juce::File::getSeparatorString())
            || rel.startsWith (kFreezeDir + "/");
    };

    std::vector<Reference> toCopy;
    for (const auto& r : refs)
        if (needsCopying (r.kind, scope))
            toCopy.push_back (r);

    const double totalSteps = (double) juce::jmax (1, (int) toCopy.size() + 1);
    double       step       = 0.0;
    auto tick = [&] { step += 1.0; if (onProgress) onProgress (step / totalSteps); };

    if (mode == Mode::Folder)
    {
        destination.createDirectory();
        if (! destination.isDirectory())
        {
            result.error = "Could not create " + destination.getFullPathName();
            return result;
        }

        if (! projectFolder.copyDirectoryTo (destination))
        {
            result.error = "Could not copy the project folder.";
            return result;
        }
        // TS7 §6.8: copyDirectoryTo takes everything, so the freeze cache is
        // removed after the fact.  It is regenerable audio -- roughly 16 MB per
        // minute per frozen track -- and the receiving machine re-renders it on
        // load, so shipping it would bloat every bundle for nothing.
        destination.getChildFile (kFreezeDir).deleteRecursively();
        tick();

        auto samples = destination.getChildFile (kSamplesDir);
        samples.createDirectory();

        for (const auto& r : toCopy)
        {
            if (shouldAbort && shouldAbort()) { result.error = "Cancelled."; return result; }

            auto target = samples.getChildFile (r.resolved.getFileName());
            if (! target.existsAsFile() && r.resolved.copyFileTo (target))
                ++result.filesCopied;
            tick();
        }
    }
    else
    {
        destination.deleteFile();
        juce::ZipFile::Builder builder;

        // The project folder itself, preserving relative layout -- minus the
        // freeze cache (TS7 §6.8).
        juce::Array<juce::File> projectFiles;
        projectFolder.findChildFiles (projectFiles, juce::File::findFiles, true);
        for (const auto& f : projectFiles)
        {
            if (isExcludedFromBundle (f, projectFolder)) continue;
            builder.addFile (f, 9, f.getRelativePathFrom (projectFolder));
        }
        tick();

        for (const auto& r : toCopy)
        {
            if (shouldAbort && shouldAbort()) { result.error = "Cancelled."; return result; }
            builder.addFile (r.resolved, 9,
                             kSamplesDir + "/" + r.resolved.getFileName());
            tick();
        }

        auto os = destination.createOutputStream();
        if (os == nullptr)
        {
            result.error = "Could not write to " + destination.getFullPathName();
            return result;
        }

        double zipProgress = 0.0;
        if (! builder.writeToStream (*os, &zipProgress))
        {
            os.reset();
            destination.deleteFile();
            result.error = "Writing the zip failed.";
            return result;
        }
    }

    result.ok = true;
    return result;
}
}
