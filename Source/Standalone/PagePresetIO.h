#pragma once
#include <JuceHeader.h>
#include <functional>

class BaySickDAWProcessor;

// ─────────────────────────────────────────────────────────────────────────────
// PagePresetIO - unified page-preset save/load for every page type
// ─────────────────────────────────────────────────────────────────────────────
// A "page preset" captures every setting on an instrument-style page - every
// engine state (including pages with multiple sub-tab engines like Inst's
// Pedals + NAM/IR + sfizz Player), every mixer-strip APVTS param, every insert
// effects rack + both pre + post EQ8 M/S, and (for pages that own a dedicated
// bus like Rusty's RustyDrums Bus) the bus rack + its EQs.  Piano-roll notes
// are intentionally excluded - that's musical content, separate from the
// chain's sound design.
//
// 2026-05-05 consolidation: this module now serves every page-type
// (Layers / Bass / Drums / Clips / Vox / Inst / RustyDrums).  Previously the
// kit-aware sfizz engines (Rusty + Guitars + Basses) used a separate
// `AriaPagePresetIO` module - that's been folded into here as the
// `kitLoadCallback` field on `EngineSlot`.  Multi-strip + bus-rack capture
// (Rusty's 13 strips + 1 bus) is now a first-class config option as well.
//
// Saved-file format: version 2 XML, root tag `<BaySickPagePreset>`.
// ─────────────────────────────────────────────────────────────────────────────
namespace PagePresetIO
{
    enum class PageKind { Layer, Bass, Drum, Clip, Vox, Inst, RustyDrums, Plugins };

    // Folder names use a "<Kind> Page" suffix so the directory layout is
    // self-explanatory in Windows Explorer (Documents/BaySickDAW/Presets/
    // Layer Page/My Presets/...).
    juce::File presetsDirForPageKind   (PageKind k);
    juce::File myPresetsDirForPageKind (PageKind k);

    juce::String pageKindLabel (PageKind k);   // "Layer", "Bass", ..., "Rusty Drums"

    // ── Per-engine slot ──────────────────────────────────────────────────────
    // A page can declare one or more engine slots so pages with sub-tab
    // engines (Inst: Pedals + NAM/IR + active sfizz Player) save / restore
    // every slot, not just the active one.  Each slot:
    //   * `engine` / `engineApvts`  - the AudioProcessor + its APVTS, used
    //                                 for getStateInformation on capture and
    //                                 setStateInformation (or replaceState)
    //                                 on restore.
    //   * `engineRootTag`           - top-level XML tag of the engine's
    //                                 getStateInformation blob; consulted
    //                                 only when `kitLoadCallback` is set so
    //                                 the loader can peel a `<KitPath>` child
    //                                 out without applying state.
    //   * `engineLabel`             - short tag stored in the saved XML so
    //                                 the loader knows which slot a record
    //                                 belongs to ("Pedals" / "NamIr" /
    //                                 "Sfizz" / "Engine").  Must round-trip
    //                                 byte-for-byte across save / load.
    //   * `enginePrefix`            - the engine's APVTS id prefix (e.g.
    //                                 "bgg_3_") so the loader can rewrite ids
    //                                 if loading into a different page index.
    //   * `kitLoadCallback`         - for sfizz-backed engines only.  When
    //                                 non-null, the loader extracts the kit
    //                                 path from the saved engine state and
    //                                 calls this BEFORE applying APVTS, so
    //                                 the wrapper's race-safe loadSfzFile
    //                                 path runs (sfizz crashes when a kit is
    //                                 reloaded mid-render without this).  For
    //                                 non-sfizz engines (Harmless, BaySickPlayer,
    //                                 NAM/IR, Pedals, etc.) leave null - the
    //                                 loader falls back to setStateInformation.
    struct EngineSlot
    {
        juce::AudioProcessor*               engine        { nullptr };
        juce::AudioProcessorValueTreeState* engineApvts   { nullptr };
        juce::String                        engineRootTag;
        juce::String                        engineLabel;
        juce::String                        enginePrefix;
        std::function<bool(const juce::File& kitPath)> kitLoadCallback;
    };

    // ── Page chain config ────────────────────────────────────────────────────
    struct PageChainConfig
    {
        // For Inst: "LiveInput" / "BaySickGuitars" / "BaySickBasses" so
        // restore can switch to the right Source mode before applying engine
        // states.  Empty for pages that don't have multiple source modes.
        juce::String sourceModeLabel;

        // One or more engine processors to capture.  Order doesn't matter on
        // disk (each record carries its own `engineLabel`), but pages
        // typically list slots in pipeline order for readability.
        juce::Array<EngineSlot> engineSlots;

        // APVTS prefixes for every mixer strip the preset should capture.
        //   Layer: ["mixer_layer_<idx>"]
        //   Inst:  ["mixer_inst_<idx>"]
        //   Rusty: ["mixer_rustybus", "mixer_rusty_0", ..., "mixer_rusty_12"]
        juce::StringArray stripPrefixes;

        // BaySickGraph InsertRack "kind" label to capture (matches saveRackStates'
        // "kind" property).  e.g. "Layer", "Bass", "Drum", "Audio", "Vox",
        // "Inst", "Rusty".
        juce::String insertRackKindLabel;

        // Empty = capture every insert of `insertRackKindLabel`.  Non-empty =
        // only capture the listed indices (typical: [pageIndex]).
        juce::Array<int> insertRackIndices;

        // BaySickGraph BusRack ids to capture.  Used for pages that own a
        // dedicated bus (Rusty's RustyBus).  Empty for every other page -
        // shared buses don't belong in per-page presets.
        juce::StringArray busRackIds;
    };

    // Capture the page chain to an XML string.  Returns empty if the config
    // has no engine slots (nothing to save).
    juce::String exportPagePreset (BaySickDAWProcessor& processor,
                                    PageKind kind,
                                    const PageChainConfig& cfg);

    // Restore a page from saved XML.  Returns true on success.  On failure
    // (parse error, damaged engine blob, missing kit on disk, kitLoadCallback
    // returns false) returns false and stops at the failing point -- this is NOT
    // atomic.  On a multi-slot page (Inst: Pedals + NAM/IR + sfizz) every slot
    // applied before the failing one keeps its new state and there is no
    // rollback; strip params and racks are reached only after every slot applies.
    bool importPagePreset (BaySickDAWProcessor& processor,
                           PageKind kind,
                           const PageChainConfig& cfg,
                           const juce::String& xml);

    // Read the sourceMode field from a preset file without applying state.
    // Lets Inst page set its Source mode before instantiating the engine.
    // Returns empty string on parse failure or if the preset has no source
    // mode label (every preset other than Inst).
    juce::String peekSourceMode (const juce::File& xml);

    // ─────────────────────────────────────────────────────────────────────────
    // Legacy single-engine + single-strip API (kept for the 5 simpler pages
    // until they're migrated to the richer config struct).  Captures the same
    // surfaces but with a flatter signature.  New code should use the
    // `PageChainConfig`-based API above.
    // ─────────────────────────────────────────────────────────────────────────
    juce::String exportPagePreset (BaySickDAWProcessor& processor,
                                    PageKind kind,
                                    int pageIndex,
                                    const juce::String& stripApvtsPrefix,
                                    juce::AudioProcessor* engineProc,
                                    const juce::String& engineType,
                                    const juce::String& enginePrefix);

    juce::String importPagePreset (BaySickDAWProcessor& processor,
                                    PageKind kind,
                                    int pageIndex,
                                    const juce::String& stripApvtsPrefix,
                                    juce::AudioProcessor* engineProc,
                                    const juce::String& enginePrefix,
                                    std::function<bool (int channelId)> isChannelActive,
                                    const juce::String& xml);

    juce::String peekEngineType (const juce::File& xml);
    // QA-UndoCoverage Task 7: same peek over in-memory XML text.
    juce::String peekEngineTypeFromXml (const juce::String& xmlText);
}
