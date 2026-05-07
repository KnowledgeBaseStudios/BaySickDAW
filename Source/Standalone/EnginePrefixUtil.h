#pragma once
#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
// G-6 (2026-04-29): shared APVTS-prefix substitution for binary engine state.
//
// When duplicating a page (Clips / Vox / Inst - and conceptually Layers /
// Bass / Drum, though those have their own auto-detecting helpers), the
// destination page's APVTS uses a different per-tab prefix from the source
// (e.g. "clip_5_" vs "clip_3_").  setStateInformation() matches by id, so
// without rewriting param ids in the binary the cloned state silently fails
// to apply.
//
// This helper round-trips the binary through juce::XmlElement → ValueTree,
// rewrites PARAM ids whose `id` starts with `srcPrefix` to start with
// `dstPrefix`, then re-serializes.  No-op when prefixes are equal.  No-op
// when the binary doesn't decode as APVTS XML.
//
// Caller knows both prefixes (typically `srcPrefix = sourcePage->getParamPrefix()`,
// `dstPrefix = destPage->getParamPrefix()`).  For Layers/Bass/Drum where the
// loaded prefix isn't directly known, those pages have their own auto-
// detecting variants that compute loadedPrefix from the engine tag.
// ─────────────────────────────────────────────────────────────────────────────
inline void substituteApvtsPrefixInBinary (juce::MemoryBlock& mb,
                                           const juce::String& srcPrefix,
                                           const juce::String& dstPrefix)
{
    if (srcPrefix == dstPrefix || mb.getSize() == 0) return;
    if (srcPrefix.isEmpty() || dstPrefix.isEmpty()) return;

    auto xmlEl = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
    if (! xmlEl) return;

    juce::ValueTree loaded = juce::ValueTree::fromXml (*xmlEl);
    if (! loaded.isValid()) return;

    bool changed = false;
    for (int i = 0; i < loaded.getNumChildren(); ++i)
    {
        auto child = loaded.getChild (i);
        if (! child.hasType ("PARAM")) continue;
        juce::String id = child.getProperty ("id").toString();
        if (id.startsWith (srcPrefix))
        {
            id = dstPrefix + id.substring (srcPrefix.length());
            child.setProperty ("id", id, nullptr);
            changed = true;
        }
    }

    if (changed)
    {
        if (auto modifiedXml = loaded.createXml())
        {
            mb.reset();
            juce::AudioProcessor::copyXmlToBinary (*modifiedXml, mb);
        }
    }
}
