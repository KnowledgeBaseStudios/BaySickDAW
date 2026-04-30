#pragma once
#include <JuceHeader.h>

class VibeSynthProcessor;

// ─────────────────────────────────────────────────────────────────────────────
// G-7 (2026-04-29): unified Page Preset save / load.
//
// A "page preset" captures every setting on an instrument-style page
// (Layer / Bass / Drum / Clip / Vox / Inst) so the user can recreate the
// exact sound in another tab or another project.  Captured surfaces:
//
//   1. Engine processor state           (BaySickPlayer / Harmless /
//                                        BaySickSynth / BaySickBass /
//                                        BaySickVocal slot reserved)
//   2. Mixer strip APVTS params         (level / pan / mute / solo / width /
//                                        polarity / FX bypass / arm,
//                                        plus per-strip EQ + sends)
//   3. Insert Effects Rack              (slot types + per-slot APVTS)
//   4. Insert post-rack EQ8 M/S         (8 bands × 2 channels)
//
// NOT captured: piano-roll patterns (song-specific musical content) and
// the full mixer routing graph (project-specific cable topology).
//
// Bus fallback: when a Vox / Inst preset references a secondary bus
// (kVoxBus2 / kInstBus2 / kInstBus3) that is not active in the current
// project, the loader silently falls back to the natural parent bus
// (kVoxBus / kInstBus) so the strip stays audible instead of ending up
// routed to a phantom channel.
// ─────────────────────────────────────────────────────────────────────────────
namespace PagePresetIO
{
    enum class PageKind { Layer, Bass, Drum, Clip, Vox, Inst };

    // Folder names use a "<Kind> Page" suffix so the directory layout is
    // self-explanatory in Windows Explorer (Documents/BaySickDAW/Presets/
    // Layer Page/My Presets/...).  Distinct from per-engine "BaySickPlayer
    // / Harmless / ..." preset folders, which keep capturing engine-only
    // state via savePatchAs.
    juce::File presetsDirForPageKind   (PageKind k);
    juce::File myPresetsDirForPageKind (PageKind k);

    juce::String pageKindLabel (PageKind k);   // "Layer", "Bass", ...

    // Capture the page chain to an XML string.  Caller passes:
    //   processor          — global VibeSynthProcessor (for apvts +
    //                        VibeGraph::saveRackStates)
    //   kind / pageIndex   — identifies the InsertNode + strip prefix
    //   stripApvtsPrefix   — e.g. "mixer_layer_0" or "mixer_audio_5"
    //   engineProc         — engine AudioProcessor (may be null for a no-
    //                        engine page; engine state is then omitted)
    //   engineType         — short tag stored in XML, e.g. "Harmless" /
    //                        "BaySickPlayer" / "BaySickSynth"
    //   enginePrefix       — engine's APVTS id prefix (e.g.
    //                        "tk_lay_0_harm_") so the loader can rewrite
    //                        ids if loading into a different page index
    juce::String exportPagePreset (VibeSynthProcessor& processor,
                                    PageKind kind,
                                    int pageIndex,
                                    const juce::String& stripApvtsPrefix,
                                    juce::AudioProcessor* engineProc,
                                    const juce::String& engineType,
                                    const juce::String& enginePrefix);

    // Restore a page from saved XML.  Returns the engineType recorded in
    // the file (the page may need to instantiate a different engine before
    // calling this).  isChannelActive is queried for kVoxBus2 / kInstBus2 /
    // kInstBus3 so the loader can fall back to natural parent buses.  Pass
    // a callback that always returns true if your page type doesn't have
    // secondary buses — fallback then becomes a no-op.
    juce::String importPagePreset (VibeSynthProcessor& processor,
                                    PageKind kind,
                                    int pageIndex,
                                    const juce::String& stripApvtsPrefix,
                                    juce::AudioProcessor* engineProc,
                                    const juce::String& enginePrefix,
                                    std::function<bool (int channelId)> isChannelActive,
                                    const juce::String& xml);

    // Read the engineType field from a preset file without applying state.
    // Lets pages instantiate the right engine before calling
    // importPagePreset.  Returns empty string on parse failure.
    juce::String peekEngineType (const juce::File& xml);
}
