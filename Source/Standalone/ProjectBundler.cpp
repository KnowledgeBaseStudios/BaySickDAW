#include "ProjectBundler.h"
#include "../PatternManager.h"
#include "../PluginProcessor.h"
#include "../SampleLibrary.h"
#include <map>

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
                 const BaySickDAWProcessor& proc,
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
    //
    // Every name here is verified against its PRODUCER.  "namPath"/"irPath"
    // shipped in the original list and matched nothing anywhere -- the NAM/IR
    // processor writes nam_filepath / ir_filepath (+ _b for the B slot) in
    // BaySickNAMIRProcessor::getStateInformation -- so those two entries were
    // dead weight while the real captures went unbundled.
    const char* const kPathAttrs[] = {
        "bsp_loadPath",     // BaySickPlayer / BaySickPlayer sample, folder or SFZ
        "kitPath",          // sfizz Guitars / Basses
        "nam_filepath",   "ir_filepath",       // NAM/IR chain, slot A
        "nam_filepath_b", "ir_filepath_b",     // NAM/IR chain, slot B
        "mic_user_ir_path",                    // NAM/IR global mic IR
        "micUserIrPath", "micbUserIrPath",     // NAM/IR per-slot mic IR snapshots
        "modelPath",        // NAM pedal capture (NAMPedalStyleDSP)
        "userIR",           // Acoustic Simulator / Preamp user IR
        "audioFilePath",    // arrangement <Block> (rewrite path only -- the
                            // enumerate walk reads blocks through PatternManager)
    };

    // Tags whose generic "path" attribute names a file.  Generic on purpose:
    // reading "path" on every element would sweep up unrelated attributes.
    const char* const kPathTags[] = { "KitPath", "Sample", "Entry" };

    // A string attribute whose VALUE is a whole XML document (the Inst chain).
    // Not base64, so the blob descent does not catch it.
    constexpr const char* kNestedXmlAttr = "instChainState";

    // Properties whose value is a base64 blob holding MORE persisted state.
    // Saved state nests: an Inst chain's <PedalsState data> holds a pedal board
    // whose every <Slot data> holds one pedal's own state, and a mixer rack's
    // `rack` property holds an EffectRack whose slots do the same.  A file
    // reference can sit at any of those depths.
    //
    // engineData / sfizzEngineData carry a tab's AudioProcessor state, and are
    // the only route to BaySickPlayer's sample path (bsp_loadPath).  They live
    // in this table rather than in a walk of their own because the enumerate
    // and rewrite passes have to descend through exactly the same set: a blob
    // only one of them reaches produces a file that is copied into the bundle
    // and then referenced at its ORIGINAL location, which is the orphaning the
    // rewrite pass exists to prevent.
    const char* const kBlobAttrs[] = {
        "data", "rack", "preEq", "eq", "engineData", "sfizzEngineData"
    };

    // Depth cap: the encoding is self-similar, so a malformed blob that decodes
    // into something blob-shaped could otherwise recurse without bound.  Four
    // covers the deepest real shape (rack -> slot -> chain -> pedal).
    constexpr int kMaxBlobDepth = 4;

    // Two base64 forms are in play and neither is self-describing:
    // MemoryBlock::toBase64Encoding writes a "<size>." prefix (Inst chain
    // children, pedal slots) while juce::Base64::toBase64 writes plain base64
    // (BaySickGraph rack properties, EffectRack slots).  fromBase64Encoding
    // rejects the plain form outright, so a single-form decode silently walked
    // past half the tree.
    enum class B64Form { Prefixed, Plain };

    bool decodeAnyBase64 (const juce::String& b64, juce::MemoryBlock& out, B64Form& form)
    {
        if (b64.isEmpty()) return false;
        if (out.fromBase64Encoding (b64) && out.getSize() > 0)
        {
            form = B64Form::Prefixed;
            return true;
        }
        juce::MemoryOutputStream mos;
        juce::Base64::convertFromBase64 (mos, b64);
        out  = mos.getMemoryBlock();
        form = B64Form::Plain;
        return out.getSize() > 0;
    }

    juce::String encodeBase64 (const juce::MemoryBlock& mb, B64Form form)
    {
        return form == B64Form::Prefixed ? mb.toBase64Encoding()
                                         : juce::Base64::toBase64 (mb.getData(), mb.getSize());
    }

    void walkXmlForPaths (const juce::XmlElement& el,
                          const std::function<void (const juce::String&, const juce::String&)>& found,
                          int depth = 0)
    {
        for (const char* attr : kPathAttrs)
            if (auto v = el.getStringAttribute (attr); v.isNotEmpty())
                found (v, attr);

        for (const char* tag : kPathTags)
            if (el.hasTagName (tag))
                if (auto v = el.getStringAttribute ("path"); v.isNotEmpty())
                    found (v, el.getTagName());

        if (depth < kMaxBlobDepth)
        {
            for (const char* blobAttr : kBlobAttrs)
            {
                juce::MemoryBlock mb;
                B64Form form {};
                if (! decodeAnyBase64 (el.getStringAttribute (blobAttr), mb, form)) continue;
                if (auto inner = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()))
                    walkXmlForPaths (*inner, found, depth + 1);
            }

            if (auto chain = el.getStringAttribute (kNestedXmlAttr); chain.isNotEmpty())
                if (auto parsed = juce::XmlDocument::parse (chain))
                    walkXmlForPaths (*parsed, found, depth + 1);
        }

        for (auto* child : el.getChildIterator())
            walkXmlForPaths (*child, found, depth);
    }

    // Mirror of walkXmlForPaths that REWRITES matched references in place,
    // re-encoding each blob it descended into.  Bundling relocates a file
    // without this, the copy sits in the bundle referenced by nothing.
    // Returns true when anything changed.
    bool rewriteXmlPaths (juce::XmlElement& el,
                          const std::map<juce::String, juce::String>& remap,
                          int depth = 0)
    {
        bool changed = false;

        auto remapAttr = [&] (const char* attr)
        {
            const auto v = el.getStringAttribute (attr);
            if (v.isEmpty()) return;
            if (auto it = remap.find (v); it != remap.end() && it->second != v)
            {
                el.setAttribute (attr, it->second);
                changed = true;
            }
        };

        for (const char* attr : kPathAttrs)
            remapAttr (attr);
        for (const char* tag : kPathTags)
            if (el.hasTagName (tag))
                remapAttr ("path");

        if (depth < kMaxBlobDepth)
        {
            for (const char* blobAttr : kBlobAttrs)
            {
                juce::MemoryBlock mb;
                B64Form form {};
                if (! decodeAnyBase64 (el.getStringAttribute (blobAttr), mb, form)) continue;
                auto inner = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
                if (inner == nullptr) continue;
                if (! rewriteXmlPaths (*inner, remap, depth + 1)) continue;

                juce::MemoryBlock reencoded;
                juce::AudioProcessor::copyXmlToBinary (*inner, reencoded);
                el.setAttribute (blobAttr, encodeBase64 (reencoded, form));
                changed = true;
            }

            if (auto chain = el.getStringAttribute (kNestedXmlAttr); chain.isNotEmpty())
                if (auto parsed = juce::XmlDocument::parse (chain))
                    if (rewriteXmlPaths (*parsed, remap, depth + 1))
                    {
                        el.setAttribute (kNestedXmlAttr,
                                         parsed->toString (juce::XmlElement::TextFormat().singleLine()));
                        changed = true;
                    }
        }

        for (auto* child : el.getChildIterator())
            changed |= rewriteXmlPaths (*child, remap, depth);

        return changed;
    }

    // Rebuilds `src`'s references for the bundle and writes the result to
    // `target` (the same file for a folder bundle, a temp file the zip branch
    // substitutes in).  Returns true only when `target` now holds the rewritten
    // copy.
    //
    // Every way of NOT producing that copy is reported: a bundle whose
    // project.xml still names the exporting machine's paths looks perfect on
    // the exporting machine and reports every copied file missing on the
    // recipient's, so an unreported failure here is invisible until the bundle
    // has already been shipped.  Reported rather than fatal -- the audio is in
    // place, so the bundle is worth writing with a warning attached.
    bool writeRewrittenProjectXml (const juce::File& src,
                                   const juce::File& target,
                                   const std::map<juce::String, juce::String>& remap,
                                   Result& result)
    {
        if (remap.empty()) return false;

        if (! src.existsAsFile())
        {
            result.copyFailed.add ("project.xml: not found, so the bundled copy still "
                                   "points at the original files");
            return false;
        }

        auto parsed = juce::XmlDocument::parse (src);
        if (parsed == nullptr)
        {
            result.copyFailed.add ("project.xml: could not be read, so the bundled copy "
                                   "still points at the original files");
            return false;
        }

        if (! rewriteXmlPaths (*parsed, remap)) return false;

        if (! target.replaceWithText (parsed->toString()))
        {
            result.copyFailed.add ("project.xml: could not update the bundled copy's "
                                   "file references");
            return false;
        }
        return true;
    }

    // What a copied reference becomes inside the bundle.  Forward slash to
    // match how the project already stores its own relative paths.
    juce::String bundleRefFor (const juce::String& kSamplesDir, const juce::String& fileName)
    {
        return kSamplesDir + "/" + fileName;
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
                                  const BaySickDAWProcessor& processor,
                                  const juce::XmlElement* tabsXml,
                                  const juce::XmlElement* rackStatesXml)
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

            // Attributes on the tab record, plus everything nested inside it:
            // the Inst chain XML (a string attribute holding a document), the
            // base64 state blobs within it, and the tab's own engine state.
            // The chain's NAM/IR and pedal state are BLOBS, so the old plain-
            // attribute walk of the chain found nothing -- every NAM capture
            // and user IR on a pedalboard went unbundled and unreported.
            walkXmlForPaths (*rec, found);
        }
    }

    // 4. Mixer rack states.  Effects that hold user files (the two Acoustic
    //    units' user IRs) can sit in any bus / insert rack, which lives under
    //    <Processor> rather than <Tabs> -- a subtree the walk never received,
    //    so those IRs were dropped from every bundle with a clean-looking
    //    result.  Passed in rather than pulled from the processor because
    //    saveRackStates is a non-const snapshot call.
    if (rackStatesXml != nullptr)
    {
        auto found = [&] (const juce::String& stored, const juce::String& what)
        {
            addRef (refs, stored, "Mixer rack (" + what + ")", processor, projectFolder);
        };
        walkXmlForPaths (*rackStatesXml, found);
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

    // storedPath -> where that file lives INSIDE the bundle.  Copying relocates
    // a file; without rewriting the reference the copy ships referenced by
    // nothing and the destination machine reports it missing.
    std::map<juce::String, juce::String> remap;
    const juce::String kProjectXml = "project.xml";

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
            // Basename collision with a DIFFERENT asset (importSample's
            // size+modtime heuristic): uniquify rather than silently aliasing
            // one source's audio onto the other's stored reference.  The
            // renamed copy is reachable only by hand -- the receiving machine
            // reports the reference missing rather than playing wrong audio.
            if (target.existsAsFile()
                && ! (target.getSize() == r.resolved.getSize()
                      && target.getLastModificationTime()
                             == r.resolved.getLastModificationTime()))
            {
                const auto stem = r.resolved.getFileNameWithoutExtension();
                const auto ext  = r.resolved.getFileExtension();
                int n = 2;
                while (samples.getChildFile (stem + " (" + juce::String (n) + ")" + ext).exists())
                    ++n;
                target = samples.getChildFile (stem + " (" + juce::String (n) + ")" + ext);
            }

            bool copied = target.existsAsFile();
            if (! copied)
            {
                copied = r.resolved.copyFileTo (target);
                if (copied) ++result.filesCopied;
                else        result.copyFailed.add (r.origin + ": " + r.storedPath);
            }
            if (copied)
                remap[r.storedPath] = bundleRefFor (kSamplesDir, target.getFileName());
            tick();
        }

        const auto bundledXml = destination.getChildFile (kProjectXml);
        writeRewrittenProjectXml (bundledXml, bundledXml, remap, result);
    }
    else
    {
        destination.deleteFile();
        juce::ZipFile::Builder builder;

        // The copy pass runs FIRST here: its renames decide what the bundled
        // project.xml has to say, and a zip entry cannot be rewritten once added.
        juce::StringArray zipEntryNames, zipEntrySources;
        for (const auto& r : toCopy)
        {
            if (shouldAbort && shouldAbort()) { result.error = "Cancelled."; return result; }

            // Same collision rule as the folder branch: a duplicate basename
            // from the SAME source is already in the archive (skip); from a
            // different source it gets a " (N)" name instead of silently
            // overwriting the first entry.
            juce::String entryName = r.resolved.getFileName();
            const int existing = zipEntryNames.indexOf (entryName, true);
            if (existing >= 0)
            {
                if (zipEntrySources[existing] == r.resolved.getFullPathName())
                {
                    // Two stored references naming one file: it is archived and
                    // counted already, so only the remap wants this second one.
                    // filesCopied counts files, not references -- same as the
                    // folder branch, whose already-present case skips the count.
                    remap[r.storedPath] = bundleRefFor (kSamplesDir, entryName);
                    tick();
                    continue;
                }
                const auto stem = r.resolved.getFileNameWithoutExtension();
                const auto ext  = r.resolved.getFileExtension();
                int n = 2;
                while (zipEntryNames.indexOf (stem + " (" + juce::String (n) + ")" + ext, true) >= 0)
                    ++n;
                entryName = stem + " (" + juce::String (n) + ")" + ext;
            }
            zipEntryNames.add (entryName);
            zipEntrySources.add (r.resolved.getFullPathName());
            remap[r.storedPath] = bundleRefFor (kSamplesDir, entryName);

            builder.addFile (r.resolved, 9, kSamplesDir + "/" + entryName);
            ++result.filesCopied;
            tick();
        }

        // The project folder itself, preserving relative layout -- minus the
        // freeze cache (TS7 §6.8).  project.xml is substituted by the rewritten
        // copy when anything moved; the temp file must outlive writeToStream.
        juce::TemporaryFile rewrittenXml (".xml");
        const bool haveRewritten = writeRewrittenProjectXml (projectFolder.getChildFile (kProjectXml),
                                                             rewrittenXml.getFile(),
                                                             remap, result);

        juce::Array<juce::File> projectFiles;
        projectFolder.findChildFiles (projectFiles, juce::File::findFiles, true);
        for (const auto& f : projectFiles)
        {
            if (isExcludedFromBundle (f, projectFolder)) continue;
            const auto rel = f.getRelativePathFrom (projectFolder);
            if (haveRewritten && f == projectFolder.getChildFile (kProjectXml))
                builder.addFile (rewrittenXml.getFile(), 9, rel);
            else
                builder.addFile (f, 9, rel);
        }
        tick();

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
