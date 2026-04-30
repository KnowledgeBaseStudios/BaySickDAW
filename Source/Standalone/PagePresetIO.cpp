#include "PagePresetIO.h"
#include "../PluginProcessor.h"
#include "../VibeGraph.h"
#include "EnginePrefixUtil.h"

namespace PagePresetIO
{
    juce::String pageKindLabel (PageKind k)
    {
        switch (k)
        {
            case PageKind::Layer: return "Layer";
            case PageKind::Bass:  return "Bass";
            case PageKind::Drum:  return "Drum";
            case PageKind::Clip:  return "Clip";
            case PageKind::Vox:   return "Vox";
            case PageKind::Inst:  return "Inst";
        }
        return "Layer";
    }

    juce::File presetsDirForPageKind (PageKind k)
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("BaySickDAW")
                   .getChildFile ("Presets")
                   .getChildFile (pageKindLabel (k) + " Page");
    }

    juce::File myPresetsDirForPageKind (PageKind k)
    {
        return presetsDirForPageKind (k).getChildFile ("My Presets");
    }

    static juce::String insertKindString (PageKind k)
    {
        // Maps to the "kind" string used by VibeGraph::saveRackStates /
        // applyRackStates for InsertRack records.  Clip pages are bound
        // to Audio inserts, so they share the "Audio" kind.
        switch (k)
        {
            case PageKind::Layer: return "Layer";
            case PageKind::Bass:  return "Bass";
            case PageKind::Drum:  return "Drum";
            case PageKind::Clip:  return "Audio";
            case PageKind::Vox:   return "Vox";
            case PageKind::Inst:  return "Inst";
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

    juce::String exportPagePreset (VibeSynthProcessor& processor,
                                    PageKind kind,
                                    int pageIndex,
                                    const juce::String& stripApvtsPrefix,
                                    juce::AudioProcessor* engineProc,
                                    const juce::String& engineType,
                                    const juce::String& enginePrefix)
    {
        juce::XmlElement root ("BaySickPagePreset");
        root.setAttribute ("version",   1);
        root.setAttribute ("pageType",  pageKindLabel (kind));
        root.setAttribute ("engineType", engineType);
        root.setAttribute ("pageIndex", pageIndex);

        // ── 1. Engine state ─────────────────────────────────────────────
        if (engineProc != nullptr)
        {
            juce::MemoryBlock mb;
            engineProc->getStateInformation (mb);

            auto* engineEl = root.createNewChildElement ("Engine");
            engineEl->setAttribute ("data",   mb.toBase64Encoding());
            engineEl->setAttribute ("prefix", enginePrefix);
            engineEl->setAttribute ("type",   engineType);
        }

        // ── 2. Strip APVTS params ───────────────────────────────────────
        // Iterate every parameter on the global apvts — capture those
        // whose id starts with the strip prefix (level/pan/mute/solo/
        // width/polarity/bypass/arm + per-strip EQ + sends).  The id is
        // saved verbatim; the loader rewrites the prefix on import.
        auto* stripEl = root.createNewChildElement ("StripParams");
        stripEl->setAttribute ("prefix", stripApvtsPrefix);

        const juce::String prefixWithSep = stripApvtsPrefix + "_";
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

        // ── 3. Insert rack + post-EQ ────────────────────────────────────
        // saveRackStates writes the entire graph; we filter to the single
        // InsertRack entry whose kind+index matches this page.
        juce::ValueTree allRacks ("RackStates");
        processor.mVibeGraph.saveRackStates (allRacks);

        const juce::String kindStr = insertKindString (kind);
        for (int i = 0; i < allRacks.getNumChildren(); ++i)
        {
            auto child = allRacks.getChild (i);
            if (! child.hasType ("InsertRack")) continue;
            if (child.getProperty ("kind").toString() != kindStr) continue;
            if ((int) child.getProperty ("index", -1) != pageIndex) continue;

            auto* rackEl = root.createNewChildElement ("Rack");
            rackEl->setAttribute ("rack", child.getProperty ("rack").toString());
            rackEl->setAttribute ("eq",   child.getProperty ("eq").toString());
            break;
        }

        return root.toString (juce::XmlElement::TextFormat().singleLine());
    }

    juce::String importPagePreset (VibeSynthProcessor& processor,
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
        if (! parsed || ! parsed->hasTagName ("BaySickPagePreset")) return {};

        const juce::String engineType = parsed->getStringAttribute ("engineType");

        // ── 1. Engine state (with prefix substitution) ──────────────────
        if (auto* engineEl = parsed->getChildByName ("Engine"))
        {
            if (engineProc != nullptr)
            {
                juce::MemoryBlock mb;
                if (mb.fromBase64Encoding (engineEl->getStringAttribute ("data")))
                {
                    const juce::String savedPrefix = engineEl->getStringAttribute ("prefix");
                    if (savedPrefix.isNotEmpty() && enginePrefix.isNotEmpty()
                            && savedPrefix != enginePrefix)
                    {
                        substituteApvtsPrefixInBinary (mb, savedPrefix, enginePrefix);
                    }
                    engineProc->setStateInformation (mb.getData(), (int) mb.getSize());
                }
            }
        }

        // ── 2. Strip APVTS params (with prefix sub + bus fallback) ──────
        if (auto* stripEl = parsed->getChildByName ("StripParams"))
        {
            const juce::String savedPrefix = stripEl->getStringAttribute ("prefix");

            for (auto* pe = stripEl->getFirstChildElement(); pe != nullptr;
                 pe = pe->getNextElement())
            {
                if (! pe->hasTagName ("Param")) continue;

                juce::String id      = pe->getStringAttribute ("id");
                float        natural = (float) pe->getDoubleAttribute ("v");

                // Rewrite "<savedPrefix>_..." → "<currentPrefix>_..." so
                // a Layer 0 preset can be loaded onto Layer 3.
                if (savedPrefix.isNotEmpty() && id.startsWith (savedPrefix + "_"))
                    id = stripApvtsPrefix + id.substring (savedPrefix.length());

                // Bus fallback: if this is a send destination targeting a
                // secondary Vox/Inst bus that isn't active, fall back to
                // the natural parent bus.
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

                    if (newChId != chId)
                        natural = (float) newChId;
                }

                if (auto* p = processor.apvts.getParameter (id))
                {
                    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                    {
                        const float normalized = rp->getNormalisableRange()
                                                     .convertTo0to1 (natural);
                        rp->setValueNotifyingHost (normalized);
                    }
                }
            }
        }

        // ── 3. Insert rack + post-EQ ────────────────────────────────────
        // Wrap the saved <Rack> in a single-child ValueTree shaped like
        // saveRackStates' output and pass to loadRackStates.  Index is
        // overridden to the destination page so a Layer 0 preset lands
        // on whatever insert called us.
        if (auto* rackEl = parsed->getChildByName ("Rack"))
        {
            juce::ValueTree wrap ("RackStates");
            juce::ValueTree rec  ("InsertRack");
            rec.setProperty ("kind",  insertKindString (kind),                 nullptr);
            rec.setProperty ("index", pageIndex,                                 nullptr);
            rec.setProperty ("rack",  rackEl->getStringAttribute ("rack"),       nullptr);
            rec.setProperty ("eq",    rackEl->getStringAttribute ("eq"),         nullptr);
            wrap.addChild (rec, -1, nullptr);
            processor.mVibeGraph.loadRackStates (wrap);
        }

        return engineType;
    }

    juce::String peekEngineType (const juce::File& xml)
    {
        if (! xml.existsAsFile()) return {};
        auto parsed = juce::XmlDocument::parse (xml);
        if (! parsed || ! parsed->hasTagName ("BaySickPagePreset")) return {};
        return parsed->getStringAttribute ("engineType");
    }
}
