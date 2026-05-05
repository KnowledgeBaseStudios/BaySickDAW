#include "RustyDrumsPagePresetIO.h"
#include "../PluginProcessor.h"
#include "../VibeGraph.h"
#include "../BaySickRustyDrums/BaySickRustyDrumsProcessor.h"

namespace RustyDrumsPagePresetIO
{
    juce::File presetsDir()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("BaySickDAW")
                   .getChildFile ("Presets")
                   .getChildFile ("Rusty Drums Page");
    }

    juce::File myPresetsDir()
    {
        return presetsDir().getChildFile ("My Presets");
    }

    // The bus + every potential per-strip prefix the preset captures.
    // 13 = MixerChannelIds::kMaxRustyStrips (kept inline so this header
    // doesn't need to depend on MixerChannelIds.h).
    static juce::StringArray gatherStripPrefixes()
    {
        juce::StringArray out;
        out.add ("mixer_rustybus");
        for (int i = 0; i < MixerChannelIds::kMaxRustyStrips; ++i)
            out.add ("mixer_rusty_" + juce::String (i));
        return out;
    }

    // Capture every APVTS param whose id starts with "<prefix>_" into a child
    // <Strip prefix="..."> element under `parent`.
    static void captureStripParams (juce::XmlElement& parent,
                                     VibeSynthProcessor& processor,
                                     const juce::String& prefix)
    {
        auto* stripEl = parent.createNewChildElement ("Strip");
        stripEl->setAttribute ("prefix", prefix);

        const juce::String prefixWithSep = prefix + "_";
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

    juce::String exportPreset (VibeSynthProcessor& processor)
    {
        auto* engine = processor.getBaySickRustyDrums();
        if (engine == nullptr) return {};

        juce::XmlElement root ("RustyDrumsPagePreset");
        root.setAttribute ("version", 1);

        // ── 1. Engine state (kit path + APVTS in one MemoryBlock) ──────────
        {
            juce::MemoryBlock mb;
            engine->getStateInformation (mb);
            auto* engineEl = root.createNewChildElement ("Engine");
            engineEl->setAttribute ("data", mb.toBase64Encoding());
        }

        // ── 2 + 3. Bus + per-strip mixer params ────────────────────────────
        {
            auto* mixerEl = root.createNewChildElement ("Mixer");
            for (const auto& pref : gatherStripPrefixes())
                captureStripParams (*mixerEl, processor, pref);
        }

        // ── 4 + 5. RustyDrums Bus rack + per-strip racks ───────────────────
        // saveRackStates dumps the entire graph; we filter to the Rusty bus
        // (kind="BusRack" id="RustyBus") and Rusty inserts (kind="InsertRack"
        // kind property = "Rusty") so we don't bloat the preset with every
        // other bus/strip in the project.
        {
            juce::ValueTree allRacks ("RackStates");
            processor.mVibeGraph.saveRackStates (allRacks);
            auto* racksEl = root.createNewChildElement ("Racks");
            for (int i = 0; i < allRacks.getNumChildren(); ++i)
            {
                auto child = allRacks.getChild (i);
                if (child.hasType ("BusRack")
                    && child.getProperty ("id").toString() == "RustyBus")
                {
                    auto* rec = racksEl->createNewChildElement ("BusRack");
                    rec->setAttribute ("id",   child.getProperty ("id").toString());
                    rec->setAttribute ("rack", child.getProperty ("rack").toString());
                    rec->setAttribute ("eq",   child.getProperty ("eq").toString());
                }
                else if (child.hasType ("InsertRack")
                    && child.getProperty ("kind").toString() == "Rusty")
                {
                    auto* rec = racksEl->createNewChildElement ("InsertRack");
                    rec->setAttribute ("kind",  "Rusty");
                    rec->setAttribute ("index", child.getProperty ("index").toString());
                    rec->setAttribute ("rack",  child.getProperty ("rack").toString());
                    rec->setAttribute ("eq",    child.getProperty ("eq").toString());
                }
            }
        }

        return root.toString (juce::XmlElement::TextFormat().singleLine());
    }

    // Pull the kit path out of the saved engine state without applying it.
    // The engine writes <BaySickRustyDrumsState><APVTS>...</APVTS><KitPath
    // path="..."/></BaySickRustyDrumsState> into the MemoryBlock; we just
    // peek the KitPath child.
    static juce::File extractKitPath (const juce::MemoryBlock& engineMb)
    {
        auto xml = juce::AudioProcessor::getXmlFromBinary (engineMb.getData(),
                                                             (int) engineMb.getSize());
        if (xml == nullptr || ! xml->hasTagName ("BaySickRustyDrumsState"))
            return {};
        if (auto* k = xml->getChildByName ("KitPath"))
            return juce::File (k->getStringAttribute ("path"));
        return {};
    }

    bool importPreset (VibeSynthProcessor& processor, const juce::String& xml)
    {
        if (xml.isEmpty()) return false;
        auto parsed = juce::XmlDocument::parse (xml);
        if (! parsed || ! parsed->hasTagName ("RustyDrumsPagePreset")) return false;

        // ── 1. Decode engine state + force the kit through the wrapper ─────
        // mProcessor.loadBaySickRustyDrumsKit creates the engine + spawns
        // every per-strip InsertNode + ensures the audio thread is rendering.
        // Calling it BEFORE setStateInformation guarantees the InsertNodes
        // exist when the mixer + rack restore pass writes to them.
        juce::MemoryBlock engineMb;
        if (auto* engineEl = parsed->getChildByName ("Engine"))
            engineMb.fromBase64Encoding (engineEl->getStringAttribute ("data"));

        if (engineMb.getSize() == 0) return false;

        const auto kitPath = extractKitPath (engineMb);
        if (! kitPath.existsAsFile())
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Load Page Preset",
                "The saved kit path is missing from this machine:\n"
                + kitPath.getFullPathName()
                + "\n\nInstall the kit, or pick a different preset.",
                "OK");
            return false;
        }

        if (! processor.loadBaySickRustyDrumsKit (kitPath))
            return false;

        // Apply just the APVTS subtree onto the engine the wrapper just
        // created.  Calling engine->setStateInformation here would re-run
        // loadKit WITHOUT the wrapper's mRustyDrumsActive guard — that's a
        // crash path because sfizz's internal hash maps get mutated while
        // the audio thread might be rendering.  replaceState is thread-safe
        // (param listeners forward each brd_cc<N> change through sfizz's
        // message-thread-safe cc() API).
        if (auto* engine = processor.getBaySickRustyDrums())
        {
            if (auto kitXml = juce::AudioProcessor::getXmlFromBinary (
                    engineMb.getData(), (int) engineMb.getSize()))
            {
                if (kitXml->hasTagName ("BaySickRustyDrumsState"))
                {
                    for (auto* child : kitXml->getChildIterator())
                    {
                        auto state = juce::ValueTree::fromXml (*child);
                        if (state.isValid()
                            && state.getType() == engine->apvts.state.getType())
                        {
                            engine->apvts.replaceState (state);
                            break;
                        }
                    }
                }
            }
        }

        // ── 2 + 3. Apply mixer params (bus + every per-strip prefix) ───────
        if (auto* mixerEl = parsed->getChildByName ("Mixer"))
        {
            for (auto* stripEl = mixerEl->getFirstChildElement(); stripEl != nullptr;
                 stripEl = stripEl->getNextElement())
            {
                if (! stripEl->hasTagName ("Strip")) continue;
                for (auto* pe = stripEl->getFirstChildElement(); pe != nullptr;
                     pe = pe->getNextElement())
                {
                    if (! pe->hasTagName ("Param")) continue;
                    const juce::String id      = pe->getStringAttribute ("id");
                    const float        natural = (float) pe->getDoubleAttribute ("v");
                    if (auto* p = processor.apvts.getParameter (id))
                    {
                        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                            rp->setValueNotifyingHost (
                                rp->getNormalisableRange().convertTo0to1 (natural));
                    }
                }
            }
        }

        // ── 4 + 5. Apply rack + EQ states (bus + per-strip) ────────────────
        if (auto* racksEl = parsed->getChildByName ("Racks"))
        {
            juce::ValueTree wrap ("RackStates");
            for (auto* el = racksEl->getFirstChildElement(); el != nullptr;
                 el = el->getNextElement())
            {
                if (el->hasTagName ("BusRack"))
                {
                    juce::ValueTree rec ("BusRack");
                    rec.setProperty ("id",   el->getStringAttribute ("id"),   nullptr);
                    rec.setProperty ("rack", el->getStringAttribute ("rack"), nullptr);
                    rec.setProperty ("eq",   el->getStringAttribute ("eq"),   nullptr);
                    wrap.addChild (rec, -1, nullptr);
                }
                else if (el->hasTagName ("InsertRack"))
                {
                    juce::ValueTree rec ("InsertRack");
                    rec.setProperty ("kind",  el->getStringAttribute ("kind"),  nullptr);
                    rec.setProperty ("index", el->getIntAttribute    ("index"), nullptr);
                    rec.setProperty ("rack",  el->getStringAttribute ("rack"),  nullptr);
                    rec.setProperty ("eq",    el->getStringAttribute ("eq"),    nullptr);
                    wrap.addChild (rec, -1, nullptr);
                }
            }
            processor.mVibeGraph.loadRackStates (wrap);
        }

        return true;
    }
}
