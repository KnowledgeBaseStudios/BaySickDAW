#include "PagePresetIO.h"
#include "../AppPaths.h"
#include "../PluginProcessor.h"
#include "../SampleLibrary.h"
#include "../BaySickGraph.h"
#include "EnginePrefixUtil.h"

namespace PagePresetIO
{
    juce::String pageKindLabel (PageKind k)
    {
        switch (k)
        {
            case PageKind::Layer:      return "Layer";
            case PageKind::Bass:       return "Bass";
            case PageKind::Drum:       return "Drum";
            case PageKind::Clip:       return "Clip";
            case PageKind::Vox:        return "Vox";
            case PageKind::Inst:       return "Inst";
            case PageKind::RustyDrums: return "Rusty Drums";
            case PageKind::Plugins:    return "Plugin";
        }
        return "Layer";
    }

    juce::File presetsDirForPageKind (PageKind k)
    {
        return AppPaths::appRoot()
                   .getChildFile ("Presets")
                   .getChildFile (pageKindLabel (k) + " Page");
    }

    juce::File myPresetsDirForPageKind (PageKind k)
    {
        return presetsDirForPageKind (k).getChildFile ("My Presets");
    }

    static juce::String insertKindString (PageKind k)
    {
        // Maps to the "kind" string used by BaySickGraph::saveRackStates /
        // applyRackStates for InsertRack records.  Clip pages are bound
        // to Audio inserts, so they share the "Audio" kind.
        switch (k)
        {
            case PageKind::Layer:      return "Layer";
            case PageKind::Bass:       return "Bass";
            case PageKind::Drum:       return "Drum";
            case PageKind::Clip:       return "Audio";
            case PageKind::Vox:        return "Vox";
            case PageKind::Inst:       return "Inst";
            case PageKind::RustyDrums: return "Rusty";
            case PageKind::Plugins:    return "Plugin";
        }
        return "Layer";
    }

    static bool paramIdIsSendDestination (const juce::String& id)
    {
        // Matches "_sendTo" (main-out destination) and "_sendN_to" for
        // N in 0..3 (additional sends).  Doesn't match "_sendN_amount"
        // or "_sendN_prepost".
        if (id.endsWith ("_sendTo")) return true;
        for (int n = 0; n < 4; ++n)
            if (id.endsWith ("_send" + juce::String (n) + "_to")) return true;
        return false;
    }

    // `recorded` is what separates "this preset never had a kit" from "this
    // preset's kit is gone": the sfizz engines omit the <KitPath> child
    // entirely when no kit is loaded, so an unresolvable path and an absent
    // one both arrive as an empty juce::File and cannot be told apart from it.
    struct SavedKitRef
    {
        bool         recorded { false };
        juce::String storedRef;
        juce::File   resolved;
    };

    // Pull <KitPath path="..."/> out of the engine's getStateInformation blob
    // without applying it.  Sfizz engines write
    //   <RootTag><APVTS .../><KitPath path="..."/></RootTag>
    // so we just look for the KitPath child by name.
    //
    // The stored string is whatever SampleLibrary::refForPersist produced -
    // a "library:<rel>" stable reference for shipped content, an absolute
    // path otherwise - so it has to come back through resolvePersistedRef.
    // Reading it as a raw juce::File turns every stable ref into a bogus
    // relative file that fails existsAsFile().
    static SavedKitRef extractKitPath (const juce::MemoryBlock& engineMb,
                                        const juce::String& rootTag)
    {
        SavedKitRef out;
        if (rootTag.isEmpty()) return out;
        auto xml = juce::AudioProcessor::getXmlFromBinary (engineMb.getData(),
                                                            (int) engineMb.getSize());
        if (xml == nullptr || ! xml->hasTagName (rootTag))
            return out;
        if (auto* k = xml->getChildByName ("KitPath"))
        {
            out.storedRef = k->getStringAttribute ("path");
            // A blank attribute is nothing to go on, so it counts as "not
            // recorded" -- that keeps every message below naming a real path.
            out.recorded  = out.storedRef.isNotEmpty();
            out.resolved  = SampleLibrary::resolvePersistedRef (out.storedRef);
        }
        return out;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // EngineSlot capture / apply helpers (used by both the new struct API and
    // the legacy single-engine wrapper at the bottom of the file).
    // ─────────────────────────────────────────────────────────────────────────
    static void captureEngineSlot (juce::XmlElement& parent,
                                    const EngineSlot& slot)
    {
        if (slot.engine == nullptr) return;
        juce::MemoryBlock mb;
        slot.engine->getStateInformation (mb);
        auto* el = parent.createNewChildElement ("Engine");
        el->setAttribute ("label",   slot.engineLabel);
        el->setAttribute ("rootTag", slot.engineRootTag);
        el->setAttribute ("prefix",  slot.enginePrefix);
        el->setAttribute ("data",    mb.toBase64Encoding());
    }

    // Returns false when the slot is sfizz-backed (kitLoadCallback present)
    // and the saved state records no kit at all, or records one that is
    // missing from disk or fails to load - the alert is shown here and the
    // caller aborts the rest of the import.  Other engine types and
    // missing-data cases return true (silently skipped).
    static bool applyEngineSlotFromXml (const juce::XmlElement& engineEl,
                                         const EngineSlot& slot)
    {
        if (slot.engine == nullptr) return true;

        // ABSENT data stays silently skipped (the documented back-compat
        // tolerance: an older preset simply lacks this engine's state).
        // PRESENT-but-unreadable data is a different thing and now fails
        // loudly -- it used to report success while applying nothing, so a
        // damaged preset looked like it loaded.  The magic-number check is
        // what actually catches corruption: in-attribute damage usually still
        // base64-decodes, and setStateInformation would then no-op silently.
        const auto rawData = engineEl.getStringAttribute ("data");
        if (rawData.isEmpty()) return true;

        juce::MemoryBlock mb;
        const bool decoded = mb.fromBase64Encoding (rawData) && mb.getSize() > 0;
        if (! decoded
            || juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()) == nullptr)
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Load Page Preset",
                "This preset's engine settings are damaged and could not be "
                "applied.", "OK");
            return false;
        }

        const juce::String savedPrefix = engineEl.getStringAttribute ("prefix");
        if (savedPrefix.isNotEmpty() && slot.enginePrefix.isNotEmpty()
            && savedPrefix != slot.enginePrefix)
        {
            substituteApvtsPrefixInBinary (mb, savedPrefix, slot.enginePrefix);
        }

        if (slot.kitLoadCallback)
        {
            // Sfizz path: peel kit path → load via wrapper → replaceState
            // the APVTS subtree onto the engine the wrapper just created.
            // Calling engine->setStateInformation here would re-run loadKit
            // WITHOUT the wrapper's active-flag guard - that's a crash path
            // because sfizz's internal hash maps get mutated while the
            // audio thread might be rendering.
            const auto savedKit = extractKitPath (mb, slot.engineRootTag);
            if (! savedKit.recorded)
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "Load Page Preset",
                    "This preset was saved with no kit loaded, so it has no "
                    "sound to restore.\n\nPick a kit for this tab, or choose a "
                    "different preset.",
                    "OK");
                return false;
            }

            const auto kitPath = savedKit.resolved;
            if (! kitPath.existsAsFile())
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "Load Page Preset",
                    "The saved kit is missing from this machine:\n"
                    + kitPath.getFullPathName()
                    + "\n\nInstall the kit, or pick a different preset.",
                    "OK");
                return false;
            }

            if (! slot.kitLoadCallback (kitPath))
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "Load Page Preset",
                    "The kit could not be loaded:\n"
                    + kitPath.getFullPathName()
                    + "\n\nThe file may be damaged.",
                    "OK");
                return false;
            }

            if (slot.engineApvts != nullptr)
            {
                if (auto kitXml = juce::AudioProcessor::getXmlFromBinary (
                        mb.getData(), (int) mb.getSize()))
                {
                    if (kitXml->hasTagName (slot.engineRootTag))
                    {
                        for (auto* child : kitXml->getChildIterator())
                        {
                            auto state = juce::ValueTree::fromXml (*child);
                            if (state.isValid()
                                && state.getType() == slot.engineApvts->state.getType())
                            {
                                // 2026-05-05 fix: walk saved <PARAM> children
                                // and explicitly setValueNotifyingHost each
                                // one BEFORE replaceState.  The kit-load just
                                // above set every CC to its set_cc default
                                // (e.g. CC25=80); the saved values differ
                                // (e.g. CC25=50).  setValueNotifyingHost with
                                // the saved value forces a real delta vs the
                                // current state, which guarantees the APVTS
                                // listener fires + dispatches the CC to
                                // sfizz.  Previous force-fire code called
                                // setValueNotifyingHost(getValue()) - same-
                                // value calls were silently no-op'd by the
                                // listener-short-circuit on some JUCE builds,
                                // leaving sfizz holding kit defaults.
                                // QA-UndoCoverage Task 6: preset-import param
                                // walks are programmatic -- never history.
                                juce::AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spw;
                                for (int i = 0; i < state.getNumChildren(); ++i)
                                {
                                    auto paramNode = state.getChild (i);
                                    if (! paramNode.hasType ("PARAM")) continue;
                                    const auto id = paramNode.getProperty ("id").toString();
                                    if (id.isEmpty()) continue;
                                    if (auto* p = slot.engineApvts->getParameter (id))
                                    {
                                        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                                        {
                                            const float natural = (float) (double)
                                                paramNode.getProperty ("value", 0.0);
                                            rp->setValueNotifyingHost (
                                                rp->getNormalisableRange().convertTo0to1 (natural));
                                        }
                                    }
                                }
                                // After PARAM dispatch, replaceState ensures
                                // any non-PARAM children (e.g. cached state
                                // properties) round-trip too.  Param values
                                // are already at the saved targets, so this
                                // is a no-op for the listener path.
                                slot.engineApvts->replaceStateKeepingUndoHistory (state);
                                break;
                            }
                        }
                    }
                }
            }
            return true;
        }

        // Non-sfizz path: setStateInformation is safe (no async kit load).
        slot.engine->setStateInformation (mb.getData(), (int) mb.getSize());
        return true;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Strip params capture / apply
    // ─────────────────────────────────────────────────────────────────────────
    static void captureStripParams (juce::XmlElement& parent,
                                     BaySickDAWProcessor& processor,
                                     const juce::String& stripPrefix)
    {
        auto* stripEl = parent.createNewChildElement ("Strip");
        stripEl->setAttribute ("prefix", stripPrefix);

        const juce::String prefixWithSep = stripPrefix + "_";
        for (auto* p : processor.getParameters())
        {
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                const juce::String id = rp->getParameterID();
                if (! id.startsWith (prefixWithSep)) continue;

                const float natural = rp->convertFrom0to1 (rp->getValue());
                auto* pe = stripEl->createNewChildElement ("Param");
                pe->setAttribute ("id", id);
                pe->setAttribute ("v",  natural);
            }
        }
    }

    // Apply <Strip prefix="...">/<Param id v>... onto the global APVTS,
    // rewriting the saved prefix to the destination prefix so a Layer 0
    // preset can be loaded onto Layer 3.  Single-strip pages pass a one-
    // entry array; multi-strip pages (Rusty) pass the full list and the
    // loader matches each <Strip> to its prefix-mapped destination.
    static void applyStripParams (BaySickDAWProcessor& processor,
                                   const juce::XmlElement& stripsEl,
                                   const juce::StringArray& destStripPrefixes,
                                   std::function<bool (int channelId)> isChannelActive)
    {
        for (auto* stripEl = stripsEl.getFirstChildElement(); stripEl != nullptr;
             stripEl = stripEl->getNextElement())
        {
            if (! stripEl->hasTagName ("Strip")) continue;
            const juce::String savedPrefix = stripEl->getStringAttribute ("prefix");

            // Find the matching destination prefix.  Single-strip pages
            // accept any saved prefix and remap to the one entry.  Multi-
            // strip pages (Rusty) keep the prefix the same on save / load,
            // so we look up by exact match first; if nothing matches and
            // there's exactly one destination, fall back to remap-to-it.
            juce::String destPrefix = savedPrefix;
            if (! destStripPrefixes.contains (savedPrefix))
            {
                if (destStripPrefixes.size() == 1)
                    destPrefix = destStripPrefixes[0];
                else
                    continue;   // unknown saved strip, skip
            }

            for (auto* pe = stripEl->getFirstChildElement(); pe != nullptr;
                 pe = pe->getNextElement())
            {
                if (! pe->hasTagName ("Param")) continue;

                juce::String id      = pe->getStringAttribute ("id");
                float        natural = (float) pe->getDoubleAttribute ("v");

                if (savedPrefix.isNotEmpty() && id.startsWith (savedPrefix + "_"))
                    id = destPrefix + id.substring (savedPrefix.length());

                // Bus fallback: secondary Vox/Inst buses that aren't active
                // in the current project route back to their natural parent.
                if (paramIdIsSendDestination (id))
                {
                    using namespace MixerChannelIds;
                    const int chId = (int) natural;
                    int newChId = chId;

                    if      (chId == kVoxBus2  && isChannelActive && ! isChannelActive (chId))
                        newChId = kVoxBus;
                    else if (chId == kInstBus2 && isChannelActive && ! isChannelActive (chId))
                        newChId = kInstBus;
                    else if (chId == kInstBus3 && isChannelActive && ! isChannelActive (chId))
                        newChId = kInstBus;
                    // QA-Layout T10: secondary group buses fall back the same way.
                    else if (chId == kLayersBus2 && isChannelActive && ! isChannelActive (chId))
                        newChId = kLayersBus;
                    else if (chId == kBassBus2 && isChannelActive && ! isChannelActive (chId))
                        newChId = kBassBus;
                    else if (chId == kClipsBus2 && isChannelActive && ! isChannelActive (chId))
                        newChId = kClipsBus;
                    else if (chId == kPluginsBus2 && isChannelActive && ! isChannelActive (chId))
                        newChId = kPluginsBus;
                    // A drum strip's MAIN out is bank-bound: the mixer refuses
                    // to route a kit-1 strip into Drums Bus 2 or the reverse
                    // (MixerPage::isRouteAllowed), because that would run one
                    // kit's audio through the other kit's inserts.  A preset
                    // carries the bus id of the bank it was saved on, so the
                    // destination page's own bank decides here.  Aux sends
                    // target aux strips and are bank-agnostic, hence the
                    // "_sendTo" test rather than paramIdIsSendDestination.
                    else if ((chId == kDrumsBus || chId == kDrumsBus2)
                             && id.endsWith ("_sendTo")
                             && destPrefix.startsWith ("mixer_drum_"))
                    {
                        newChId = drumBusForPage (
                            destPrefix.substring (juce::String ("mixer_drum_").length())
                                      .getIntValue());
                    }

                    if (newChId != chId)
                        natural = (float) newChId;
                }

                if (auto* p = processor.apvts.getParameter (id))
                {
                    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                    {
                        // Task 6: preset strip-param restore is programmatic.
                        juce::AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spw;
                        const float normalized = rp->getNormalisableRange()
                                                     .convertTo0to1 (natural);
                        rp->setValueNotifyingHost (normalized);
                    }
                }
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Insert + bus rack capture / apply
    // ─────────────────────────────────────────────────────────────────────────
    static void captureRacks (juce::XmlElement& parent,
                               BaySickDAWProcessor& processor,
                               const PageChainConfig& cfg)
    {
        juce::ValueTree allRacks ("RackStates");
        processor.mVibeGraph.saveRackStates (allRacks);

        const juce::String kindStr = cfg.insertRackKindLabel;

        for (int i = 0; i < allRacks.getNumChildren(); ++i)
        {
            auto child = allRacks.getChild (i);

            if (child.hasType ("BusRack"))
            {
                const auto id = child.getProperty ("id").toString();
                if (! cfg.busRackIds.contains (id)) continue;

                auto* rec = parent.createNewChildElement ("BusRack");
                rec->setAttribute ("id",    id);
                rec->setAttribute ("rack",  child.getProperty ("rack") .toString());
                rec->setAttribute ("preEq", child.getProperty ("preEq").toString());
                rec->setAttribute ("eq",    child.getProperty ("eq")   .toString());
            }
            else if (child.hasType ("InsertRack")
                     && child.getProperty ("kind").toString() == kindStr)
            {
                const int idx = (int) child.getProperty ("index", -1);
                if (! cfg.insertRackIndices.isEmpty()
                    && ! cfg.insertRackIndices.contains (idx))
                    continue;

                auto* rec = parent.createNewChildElement ("InsertRack");
                rec->setAttribute ("kind",  kindStr);
                rec->setAttribute ("index", idx);
                rec->setAttribute ("rack",  child.getProperty ("rack") .toString());
                rec->setAttribute ("preEq", child.getProperty ("preEq").toString());
                rec->setAttribute ("eq",    child.getProperty ("eq")   .toString());
            }
        }
    }

    static void applyRacks (BaySickDAWProcessor& processor,
                             const juce::XmlElement& racksEl,
                             const PageChainConfig& cfg,
                             int destInsertIndexOverride /* -1 = use saved index */)
    {
        juce::ValueTree wrap ("RackStates");

        for (auto* el = racksEl.getFirstChildElement(); el != nullptr;
             el = el->getNextElement())
        {
            if (el->hasTagName ("BusRack"))
            {
                juce::ValueTree rec ("BusRack");
                rec.setProperty ("id",   el->getStringAttribute ("id"),   nullptr);
                rec.setProperty ("rack", el->getStringAttribute ("rack"), nullptr);
                rec.setProperty ("eq",   el->getStringAttribute ("eq"),   nullptr);
                if (el->hasAttribute ("preEq"))
                    rec.setProperty ("preEq", el->getStringAttribute ("preEq"), nullptr);
                wrap.addChild (rec, -1, nullptr);
            }
            else if (el->hasTagName ("InsertRack"))
            {
                juce::ValueTree rec ("InsertRack");
                rec.setProperty ("kind", el->getStringAttribute ("kind"), nullptr);
                const int idx = (destInsertIndexOverride >= 0)
                                  ? destInsertIndexOverride
                                  : el->getIntAttribute ("index");
                rec.setProperty ("index", idx,                           nullptr);
                rec.setProperty ("rack",  el->getStringAttribute ("rack"), nullptr);
                rec.setProperty ("eq",    el->getStringAttribute ("eq"),   nullptr);
                if (el->hasAttribute ("preEq"))
                    rec.setProperty ("preEq", el->getStringAttribute ("preEq"), nullptr);
                wrap.addChild (rec, -1, nullptr);
            }
        }

        processor.mVibeGraph.loadRackStates (wrap);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Public new struct-based API
    // ─────────────────────────────────────────────────────────────────────────
    juce::String exportPagePreset (BaySickDAWProcessor& processor,
                                    PageKind kind,
                                    const PageChainConfig& cfg)
    {
        juce::XmlElement root ("BaySickPagePreset");
        root.setAttribute ("version",  2);
        root.setAttribute ("pageType", pageKindLabel (kind));
        if (cfg.sourceModeLabel.isNotEmpty())
            root.setAttribute ("sourceMode", cfg.sourceModeLabel);

        // ── 1. Engine slots (Pedals + NAM/IR + sfizz Player for Inst, etc.) ──
        if (! cfg.engineSlots.isEmpty())
        {
            auto* enginesEl = root.createNewChildElement ("Engines");
            for (const auto& slot : cfg.engineSlots)
                captureEngineSlot (*enginesEl, slot);
        }

        // ── 2. Per-strip mixer params ────────────────────────────────────────
        if (! cfg.stripPrefixes.isEmpty())
        {
            auto* stripsEl = root.createNewChildElement ("Strips");
            for (const auto& sp : cfg.stripPrefixes)
                captureStripParams (*stripsEl, processor, sp);
        }

        // ── 3 + 4. Bus rack(s) + per-strip insert racks (with both EQs) ─────
        auto* racksEl = root.createNewChildElement ("Racks");
        captureRacks (*racksEl, processor, cfg);

        return root.toString (juce::XmlElement::TextFormat().singleLine());
    }

    bool importPagePreset (BaySickDAWProcessor& processor,
                            PageKind kind,
                            const PageChainConfig& cfg,
                            const juce::String& xml)
    {
        if (xml.isEmpty()) return false;

        auto parsed = juce::XmlDocument::parse (xml);
        if (parsed == nullptr)
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Load Page Preset",
                "That preset file could not be read.",
                "OK");
            return false;
        }

        // 2026-05-05 backward-compat: accept three preset formats.
        //   1. v2 (consolidated)         - root <BaySickPagePreset version=2>
        //                                   wrappers <Engines>/<Strips>/<Racks>
        //   2. v1 PagePresetIO           - root <BaySickPagePreset version=1>
        //                                   single <Engine>/<StripParams>/<Rack>
        //   3. K-7 Aria/RustyDrums shim  - root <RustyDrumsPagePreset> /
        //                                       <GuitarsPagePreset>
        //                                   single <Engine>, <Mixer>/<Strip>,
        //                                   <Racks>/<BusRack>+<InsertRack>
        // The detector below normalises old shapes onto the v2 in-memory layout
        // so the apply path stays single-form.
        const auto rootTag = parsed->getTagName();
        const bool isModernRoot   = rootTag == "BaySickPagePreset";
        const bool isK7AriaRoot   = rootTag == "RustyDrumsPagePreset"
                                       || rootTag == "GuitarsPagePreset"
                                       || rootTag == "BassesPagePreset";
        if (! isModernRoot && ! isK7AriaRoot)
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Load Page Preset",
                "That preset file could not be read.",
                "OK");
            return false;
        }

        // Helper: when no <Engines> wrapper exists, wrap a top-level <Engine>
        // child into a synthetic <Engines> so the v2 apply path can iterate it.
        // Same for <Strips> ↔ <StripParams>/<Mixer>, <Racks> ↔ <Rack>.  All
        // synthesised wrappers are owned by this lambda's local stash so they
        // persist for the lifetime of `parsed`.
        std::vector<std::unique_ptr<juce::XmlElement>> legacyOwned;

        auto ensureEnginesWrapper = [&]() -> juce::XmlElement*
        {
            if (auto* existing = parsed->getChildByName ("Engines")) return existing;

            // Old <Engine> at top level (PagePresetIO v1 + K-7 Aria).  Promote
            // it into a synthesised <Engines> child.  Label defaults to the
            // first config slot's label ("Engine" for legacy callers, "Sfizz"
            // for K-7 Aria saves) so the v2 matcher finds it.
            if (auto* legacyEngine = parsed->getChildByName ("Engine"))
            {
                auto wrapper = std::make_unique<juce::XmlElement> ("Engines");
                auto* clone = new juce::XmlElement (*legacyEngine);

                // K-7 / v1 didn't have a `label` attribute.  Pick the best
                // match from the config: K-7 Aria roots → "Sfizz" slot;
                // PagePresetIO v1 root → "Engine" slot (legacy wrapper).
                if (! clone->hasAttribute ("label"))
                {
                    if (isK7AriaRoot)
                        clone->setAttribute ("label", "Sfizz");
                    else if (! cfg.engineSlots.isEmpty())
                        clone->setAttribute ("label", cfg.engineSlots[0].engineLabel);
                }
                // K-7 Aria saves stored the engine root tag inline in the
                // single-engine pattern; bring it forward.
                if (! clone->hasAttribute ("rootTag")
                    && ! cfg.engineSlots.isEmpty())
                {
                    clone->setAttribute ("rootTag",
                                          cfg.engineSlots[0].engineRootTag);
                }
                wrapper->addChildElement (clone);
                auto* raw = wrapper.get();
                legacyOwned.push_back (std::move (wrapper));
                return raw;
            }
            return nullptr;
        };

        auto ensureStripsWrapper = [&]() -> juce::XmlElement*
        {
            if (auto* existing = parsed->getChildByName ("Strips")) return existing;

            // K-7 Aria saves used <Mixer> as the wrapper (with <Strip> children).
            if (auto* mixerEl = parsed->getChildByName ("Mixer"))
            {
                auto wrapper = std::make_unique<juce::XmlElement> ("Strips");
                for (auto* s = mixerEl->getFirstChildElement(); s != nullptr;
                     s = s->getNextElement())
                {
                    if (s->hasTagName ("Strip"))
                        wrapper->addChildElement (new juce::XmlElement (*s));
                }
                auto* raw = wrapper.get();
                legacyOwned.push_back (std::move (wrapper));
                return raw;
            }
            // PagePresetIO v1 used <StripParams> at top level (single strip).
            // Promote it to <Strips><Strip prefix><Param/></Strip></Strips>.
            if (auto* stripParamsEl = parsed->getChildByName ("StripParams"))
            {
                auto wrapper = std::make_unique<juce::XmlElement> ("Strips");
                auto* stripChild = new juce::XmlElement ("Strip");
                stripChild->setAttribute ("prefix",
                    stripParamsEl->getStringAttribute ("prefix"));
                for (auto* p = stripParamsEl->getFirstChildElement(); p != nullptr;
                     p = p->getNextElement())
                {
                    if (p->hasTagName ("Param"))
                        stripChild->addChildElement (new juce::XmlElement (*p));
                }
                wrapper->addChildElement (stripChild);
                auto* raw = wrapper.get();
                legacyOwned.push_back (std::move (wrapper));
                return raw;
            }
            return nullptr;
        };

        auto ensureRacksWrapper = [&]() -> juce::XmlElement*
        {
            if (auto* existing = parsed->getChildByName ("Racks")) return existing;

            // PagePresetIO v1 used a single top-level <Rack rack eq/> - wrap
            // into <Racks><InsertRack kind index rack eq/></Racks> so the v2
            // apply path treats it as a one-item insert-rack list.  Kind +
            // index come from the config (single-strip page).
            if (auto* legacyRack = parsed->getChildByName ("Rack"))
            {
                auto wrapper = std::make_unique<juce::XmlElement> ("Racks");
                auto* insertEl = wrapper->createNewChildElement ("InsertRack");
                insertEl->setAttribute ("kind", cfg.insertRackKindLabel);
                const int idx = (cfg.insertRackIndices.size() == 1)
                                   ? cfg.insertRackIndices[0]
                                   : 0;
                insertEl->setAttribute ("index", idx);
                insertEl->setAttribute ("rack", legacyRack->getStringAttribute ("rack"));
                insertEl->setAttribute ("eq",   legacyRack->getStringAttribute ("eq"));
                if (legacyRack->hasAttribute ("preEq"))
                    insertEl->setAttribute ("preEq", legacyRack->getStringAttribute ("preEq"));
                auto* raw = wrapper.get();
                legacyOwned.push_back (std::move (wrapper));
                return raw;
            }
            return nullptr;
        };

        auto* enginesEl = ensureEnginesWrapper();
        auto* stripsEl  = ensureStripsWrapper();
        auto* racksEl   = ensureRacksWrapper();

        // ── 1. Engine slots - match by `engineLabel` so save/load order
        // doesn't have to align.  Slots in the config but not in the saved
        // XML are simply not touched (engine keeps its current state).
        if (enginesEl != nullptr)
        {
            for (auto* engineEl = enginesEl->getFirstChildElement(); engineEl != nullptr;
                 engineEl = engineEl->getNextElement())
            {
                if (! engineEl->hasTagName ("Engine")) continue;
                const juce::String label = engineEl->getStringAttribute ("label");
                for (const auto& slot : cfg.engineSlots)
                {
                    if (slot.engineLabel == label)
                    {
                        if (! applyEngineSlotFromXml (*engineEl, slot))
                            return false;
                        break;
                    }
                }
            }
        }

        // ── 2. Per-strip mixer params ────────────────────────────────────────
        if (stripsEl != nullptr)
        {
            applyStripParams (processor, *stripsEl, cfg.stripPrefixes,
                              [&processor](int chId) -> bool
                              {
                                  // No reliable "is this channel active?" for
                                  // arbitrary buses today - keep all routes
                                  // unless the page passes its own test via
                                  // the legacy API.  TODO: wire this through.
                                  juce::ignoreUnused (processor, chId);
                                  return true;
                              });
        }

        // ── 3 + 4. Racks ────────────────────────────────────────────────────
        if (racksEl != nullptr)
        {
            // For multi-insert pages (Rusty) we keep the saved indices.  For
            // single-insert pages we override with the one configured index
            // so a Layer 0 preset lands on whatever destination called us.
            const int overrideIdx = (cfg.insertRackIndices.size() == 1)
                                       ? cfg.insertRackIndices[0]
                                       : -1;
            applyRacks (processor, *racksEl, cfg, overrideIdx);
        }

        return true;
    }

    juce::String peekSourceMode (const juce::File& xml)
    {
        if (! xml.existsAsFile()) return {};
        auto parsed = juce::XmlDocument::parse (xml);
        if (parsed == nullptr) return {};
        const auto rootTag = parsed->getTagName();
        if (rootTag != "BaySickPagePreset"
            && rootTag != "RustyDrumsPagePreset"
            && rootTag != "GuitarsPagePreset"
            && rootTag != "BassesPagePreset")
            return {};
        // v2 saves carry sourceMode explicitly.  K-7 saves don't, but the
        // root tag pins the source: GuitarsPagePreset → BaySickGuitars, etc.
        // PagePresetIO v1 saves on Inst were always LiveInput.
        if (parsed->hasAttribute ("sourceMode"))
            return parsed->getStringAttribute ("sourceMode");
        if (rootTag == "GuitarsPagePreset") return "BaySickGuitars";
        if (rootTag == "BassesPagePreset")  return "BaySickBasses";
        // RustyDrumsPagePreset isn't loaded on Inst, so its mapping is moot.
        // Pre-consolidation BaySickPagePreset on Inst was always live-input.
        if (rootTag == "BaySickPagePreset"
            && parsed->getStringAttribute ("pageType") == "Inst")
            return "LiveInput";
        return {};
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Legacy single-engine + single-strip API.  Implemented as a thin wrapper
    // around the new struct-based API so both write the same XML format.
    // ─────────────────────────────────────────────────────────────────────────
    juce::String exportPagePreset (BaySickDAWProcessor& processor,
                                    PageKind kind,
                                    int pageIndex,
                                    const juce::String& stripApvtsPrefix,
                                    juce::AudioProcessor* engineProc,
                                    const juce::String& engineType,
                                    const juce::String& enginePrefix)
    {
        PageChainConfig cfg;

        if (engineProc != nullptr)
        {
            EngineSlot slot;
            slot.engine        = engineProc;
            slot.engineLabel   = "Engine";
            slot.engineRootTag = engineType;   // legacy callers pass type label here
            slot.enginePrefix  = enginePrefix;
            cfg.engineSlots.add (slot);
        }

        cfg.stripPrefixes.add (stripApvtsPrefix);
        cfg.insertRackKindLabel = insertKindString (kind);
        cfg.insertRackIndices.add (pageIndex);

        // Stash the legacy engine type tag onto the XML so peekEngineType()
        // can round-trip it for callers that switch engines on load.
        auto xml = exportPagePreset (processor, kind, cfg);
        if (engineProc != nullptr)
        {
            auto parsed = juce::XmlDocument::parse (xml);
            if (parsed != nullptr)
            {
                parsed->setAttribute ("engineType", engineType);
                xml = parsed->toString (juce::XmlElement::TextFormat().singleLine());
            }
        }
        return xml;
    }

    juce::String importPagePreset (BaySickDAWProcessor& processor,
                                    PageKind kind,
                                    int pageIndex,
                                    const juce::String& stripApvtsPrefix,
                                    juce::AudioProcessor* engineProc,
                                    const juce::String& enginePrefix,
                                    std::function<bool (int channelId)> isChannelActive,
                                    const juce::String& xml)
    {
        if (xml.isEmpty()) return {};

        auto parsed = juce::XmlDocument::parse (xml);
        if (parsed == nullptr)
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Load Page Preset",
                "That preset file could not be read.",
                "OK");
            return {};
        }
        const auto rootTag = parsed->getTagName();
        if (rootTag != "BaySickPagePreset"
            && rootTag != "RustyDrumsPagePreset"
            && rootTag != "GuitarsPagePreset"
            && rootTag != "BassesPagePreset")
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Load Page Preset",
                "That preset file could not be read.",
                "OK");
            return {};
        }

        // Map root-tag → engine type when the saved file is K-7 era and has
        // no inline `engineType` attribute.  v1 PagePresetIO + v2 saves carry
        // `engineType` directly so the attribute path is preferred.
        const juce::String engineType =
            parsed->hasAttribute ("engineType")
                ? parsed->getStringAttribute ("engineType")
                : (rootTag == "RustyDrumsPagePreset" ? juce::String ("BaySickRustyDrums")
                  : rootTag == "GuitarsPagePreset"   ? juce::String ("BaySickGuitars")
                  : rootTag == "BassesPagePreset"    ? juce::String ("BaySickBasses")
                  : juce::String());

        PageChainConfig cfg;
        if (engineProc != nullptr)
        {
            EngineSlot slot;
            slot.engine        = engineProc;
            slot.engineLabel   = "Engine";
            slot.engineRootTag = engineType;
            slot.enginePrefix  = enginePrefix;
            cfg.engineSlots.add (slot);
        }

        cfg.stripPrefixes.add (stripApvtsPrefix);
        cfg.insertRackKindLabel = insertKindString (kind);
        cfg.insertRackIndices.add (pageIndex);

        // Run the new importer to apply engine + strip + rack state.  We
        // can't re-use applyStripParams's bus-fallback-by-isChannelActive
        // closure path through the new API yet (TODO above), so re-run the
        // strip block here with the page's `isChannelActive` query.  The
        // new importer auto-promotes legacy <StripParams> / <Mixer> wrappers
        // into <Strips> for back-compat, but this short-circuit only sees
        // the modern wrapper - old saves go through the recursive call below.
        if (auto* stripsEl = parsed->getChildByName ("Strips"))
            applyStripParams (processor, *stripsEl, cfg.stripPrefixes, isChannelActive);

        // Apply engines + racks via the new path (which itself re-applies
        // strips with a no-op fallback - harmless duplicate, last write wins).
        importPagePreset (processor, kind, cfg, xml);

        return engineType;
    }

    juce::String peekEngineType (const juce::File& xml)
    {
        if (! xml.existsAsFile()) return {};
        return peekEngineTypeFromXml (xml.loadFileAsString());
    }

    // QA-UndoCoverage Task 7: string-input peek for the structural-undo
    // snapshots (they live as in-memory XML / temp files, not preset files).
    juce::String peekEngineTypeFromXml (const juce::String& xmlText)
    {
        auto parsed = juce::XmlDocument::parse (xmlText);
        if (parsed == nullptr) return {};
        const auto rootTag = parsed->getTagName();
        if (rootTag != "BaySickPagePreset"
            && rootTag != "RustyDrumsPagePreset"
            && rootTag != "GuitarsPagePreset"
            && rootTag != "BassesPagePreset")
            return {};
        // Prefer the legacy `engineType` attribute when present (single-
        // engine v1 saves).  v2 saves use <Engines>/<Engine label="..."> so
        // there's no single answer; return the first slot's `rootTag`.
        if (parsed->hasAttribute ("engineType"))
            return parsed->getStringAttribute ("engineType");
        if (auto* enginesEl = parsed->getChildByName ("Engines"))
            if (auto* first = enginesEl->getFirstChildElement())
                if (first->hasTagName ("Engine"))
                    return first->getStringAttribute ("rootTag");
        // K-7 Aria root + single top-level <Engine> with no `rootTag` attr -
        // map root tag → engine type so `selectEngine` on the page can pick
        // the right processor before applying state.
        if (rootTag == "RustyDrumsPagePreset") return "BaySickRustyDrums";
        if (rootTag == "GuitarsPagePreset")    return "BaySickGuitars";
        if (rootTag == "BassesPagePreset")     return "BaySickBasses";
        return {};
    }
}
