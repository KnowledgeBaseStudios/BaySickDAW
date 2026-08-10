#include "FxRackPresetIO.h"
#include "EffectsPage.h"
#include "../AppPaths.h"
#include "../PluginProcessor.h"
#include "../UserFileSave.h"

namespace FxRackPresetIO
{
namespace
{
    constexpr int kVersion = 1;

    // The four EQ parameter families on a strip.  Spelled out rather than
    // pattern-matched: "<prefix>_mid_eq" and "<prefix>_preeq_mid_eq" are
    // disjoint by construction, and a loose "contains eq" test would sweep up
    // unrelated parameters the day someone adds one.
    juce::StringArray eqSubPrefixes (const juce::String& mixerPrefix)
    {
        return { mixerPrefix + "_mid_eq",
                 mixerPrefix + "_side_eq",
                 mixerPrefix + "_preeq_mid_eq",
                 mixerPrefix + "_preeq_side_eq" };
    }

    bool idIsEqParam (const juce::String& id, const juce::StringArray& subs)
    {
        for (const auto& s : subs)
            if (id.startsWith (s)) return true;
        return false;
    }
}

juce::File presetsDir()
{
    return AppPaths::appRoot().getChildFile ("Presets").getChildFile ("FX Rack");
}

juce::File myPresetsDir()
{
    return presetsDir().getChildFile ("My Presets");
}

juce::Array<juce::File> enumeratePresets()
{
    juce::Array<juce::File> out;
    auto dir = myPresetsDir();
    if (dir.isDirectory())
        for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.xml"))
            out.add (f);
    out.sort();
    return out;
}

bool save (BaySickDAWProcessor& proc, int channelId,
           const juce::String& presetName, juce::String& outErr)
{
    auto* rack = EffectsPage::rackForChannelId (proc.mVibeGraph, channelId);
    if (rack == nullptr) { outErr = "This channel has no effects rack."; return false; }

    juce::XmlElement root ("BaySickFxRackPreset");
    root.setAttribute ("version", kVersion);

    {
        juce::MemoryBlock mb;
        rack->getStateInformation (mb);
        root.createNewChildElement ("Rack")
            ->setAttribute ("data", mb.toBase64Encoding());
    }

    // EQ half: the strip's parameters, saved in natural units so the file stays
    // readable and survives a range change.
    const juce::String mixerPrefix = EffectsPage::mixerPrefixForChannelId (channelId);
    if (mixerPrefix.isNotEmpty())
    {
        auto* eqEl = root.createNewChildElement ("Eq");
        eqEl->setAttribute ("prefix", mixerPrefix);

        const auto subs = eqSubPrefixes (mixerPrefix);
        for (auto* p : proc.getParameters())
        {
            auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
            if (rp == nullptr) continue;
            const juce::String id = rp->getParameterID();
            if (! idIsEqParam (id, subs)) continue;

            auto* pe = eqEl->createNewChildElement ("Param");
            // Store the SUFFIX, not the full id, so the preset can be loaded
            // onto a different strip.
            pe->setAttribute ("id", id.substring (mixerPrefix.length()));
            pe->setAttribute ("v",  rp->convertFrom0to1 (rp->getValue()));
        }
    }

    auto dir = myPresetsDir();
    if (! dir.exists() && ! dir.createDirectory())
    { outErr = "Could not create the FX Rack presets folder."; return false; }

    // Stamped from the typed name's legal stem rather than the final file:
    // the write is async and Save a Copy picks its suffixed path after the
    // element below is deep-copied.  Nothing reads the attribute back.
    root.setAttribute ("name", juce::File::createLegalFileName (presetName).trim());

    // The rack dialog pre-fills the channel's own display name, so re-saving
    // a channel's rack proposes the SAME name every time - exactly the case
    // the Replace / Save a Copy / Cancel prompt exists for.  Naming, that
    // prompt and the write's own failure box are all the helper's; outErr
    // covers only the pre-write failures above, so a true return means the
    // save was handed to the helper, not that the file is on disk.
    UserFileSave::writeXmlAsync (dir, presetName, root, {});
    return true;
}

bool load (BaySickDAWProcessor& proc, int channelId,
           const juce::File& presetFile, juce::String& outErr)
{
    auto xml = juce::XmlDocument::parse (presetFile);
    if (! xml || ! xml->hasTagName ("BaySickFxRackPreset"))
    { outErr = "Not an FX Rack preset file."; return false; }

    auto* rack = EffectsPage::rackForChannelId (proc.mVibeGraph, channelId);
    if (rack == nullptr) { outErr = "This channel has no effects rack."; return false; }

    auto* rackEl = xml->getChildByName ("Rack");
    juce::MemoryBlock mb;
    if (rackEl == nullptr
        || ! mb.fromBase64Encoding (rackEl->getStringAttribute ("data"))
        || mb.getSize() == 0)
    { outErr = "The preset's rack data is missing or corrupt."; return false; }
    rack->setStateInformation (mb.getData(), (int) mb.getSize());

    const juce::String destPrefix = EffectsPage::mixerPrefixForChannelId (channelId);
    if (auto* eqEl = xml->getChildByName ("Eq"))
    {
        if (destPrefix.isNotEmpty())
        {
            // QA-UndoCoverage Task 6: rack-preset EQ restore is programmatic.
            juce::AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spw;
            for (auto* pe = eqEl->getFirstChildElement(); pe != nullptr; pe = pe->getNextElement())
            {
                if (! pe->hasTagName ("Param")) continue;
                const juce::String id = destPrefix + pe->getStringAttribute ("id");
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id)))
                {
                    const float natural = (float) pe->getDoubleAttribute ("v");
                    rp->setValueNotifyingHost (rp->convertTo0to1 (natural));
                }
            }
        }
    }

    return true;
}

} // namespace FxRackPresetIO
