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
    bool needsCopying (RefKind k, Scope scope)
    {
        switch (k)
        {
            case RefKind::UserSamples:
            case RefKind::Absolute:        return true;
            case RefKind::CoreLibrary:     return scope == Scope::SelfContained;
            case RefKind::ProjectRelative: return false;   // inside the folder already
            case RefKind::Missing:         return false;
        }
        return false;
    }
}

std::vector<Reference> enumerate (PatternManager& pm,
                                  const VibeSynthProcessor& processor)
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

        // The project folder itself, preserving relative layout.
        juce::Array<juce::File> projectFiles;
        projectFolder.findChildFiles (projectFiles, juce::File::findFiles, true);
        for (const auto& f : projectFiles)
            builder.addFile (f, 9, f.getRelativePathFrom (projectFolder));
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
