#include "FxRackPresetIO.h"
#include "EffectsPage.h"
#include "../AppPaths.h"
#include "../PluginProcessor.h"

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

bool save (VibeSynthProcessor& proc, int channelId,
           const juce::String& presetName, juce::String& outErr)
{
    if (presetName.trim().isEmpty()) { outErr = "Preset needs a name."; return false; }

    auto* rack = EffectsPage::rackForChannelId (proc.mVibeGraph, channelId);
    if (rack == nullptr) { outErr = "This channel has no effects rack."; return false; }

    juce::XmlElement root ("BaySickFxRackPreset");
    root.setAttribute ("version", kVersion);
    root.setAttribute ("name",    presetName);

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

    auto file = dir.getChildFile (juce::File::createLegalFileName (presetName) + ".xml");
    if (! root.writeTo (file)) { outErr = "Could not write " + file.getFullPathName(); return false; }
    return true;
}

bool load (VibeSynthProcessor& proc, int channelId,
           const juce::File& presetFile, juce::String& outErr)
{
    auto xml = juce::XmlDocument::parse (presetFile);
    if (! xml || ! xml->hasTagName ("BaySickFxRackPreset"))
    { outErr = "Not an FX Rack preset file."; return false; }

    auto* rack = EffectsPage::rackForChannelId (proc.mVibeGraph, channelId);
    if (rack == nullptr) { outErr = "This channel has no effects rack."; return false; }

    if (auto* rackEl = xml->getChildByName ("Rack"))
    {
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (rackEl->getStringAttribute ("data")) && mb.getSize() > 0)
            rack->setStateInformation (mb.getData(), (int) mb.getSize());
    }

    const juce::String destPrefix = EffectsPage::mixerPrefixForChannelId (channelId);
    if (auto* eqEl = xml->getChildByName ("Eq"))
    {
        if (destPrefix.isNotEmpty())
        {
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
