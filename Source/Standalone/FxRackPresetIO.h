#pragma once
#include <JuceHeader.h>

class BaySickDAWProcessor;

// ─────────────────────────────────────────────────────────────────────────────
// FxRackPresetIO - QA-ModelShell TS5 (2026-07-29), Jeff's spec
// ─────────────────────────────────────────────────────────────────────────────
// Save / load a WHOLE strip's effects setup as one preset: all six rack slots
// (types, order, uuids, bypass, output gains and every DSP's own state) plus
// the strip's pre-rack and post-rack EQ8 M/S.
//
// Distinct from its two neighbours, which is why it is its own module:
//   * EffectPresetIO  - ONE effect's DSP state.
//   * PagePresetIO    - a whole instrument PAGE (engines + strip params + racks).
//   * this            - the rack + its two EQs, and nothing else, so a preset
//                       saved on a vocal strip can be loaded onto a guitar
//                       strip without dragging that page's engines with it.
//
// Folder layout follows the shipped convention (EffectPresetIO::presetsRoot):
//   Documents/BaySickDAW/Presets/FX Rack/My Presets/{name}.xml
//
// WHY THE RACK IS A BLOB BUT THE EQs ARE PARAMETERS.  EffectRack owns its DSP
// state outright, so its getStateInformation blob IS the truth.  The EQs do not:
// for any strip with a mixer prefix the APVTS parameters are the source of truth
// and processBlock pushes them into the EQ8MsDSP every block -- so restoring the
// DSP's own state would be overwritten within one block.  The EQ half therefore
// captures and restores the strip's `_mid_eq` / `_side_eq` / `_preeq_*`
// parameters, exactly as PagePresetIO does for the same reason.
// ─────────────────────────────────────────────────────────────────────────────
namespace FxRackPresetIO
{
    juce::File presetsDir();     // .../Presets/FX Rack
    juce::File myPresetsDir();   // .../Presets/FX Rack/My Presets

    juce::Array<juce::File> enumeratePresets();

    // channelId is the Effects-page channel vocabulary (1-12 buses, 100+ drums,
    // 200+ layers, ...) -- the same ids the automation lane prefixes embed.
    // presetName is the raw typed string; naming, the collision prompt
    // (Replace / Save a Copy / Cancel) and the write's own failure box are
    // UserFileSave::writeXmlAsync's, so a true return means the save was
    // handed to that helper, not that the file is on disk yet.  outErr is set
    // (with false returned) only for the pre-write failures: no rack on the
    // channel, or the presets folder could not be created.
    bool save (BaySickDAWProcessor& proc, int channelId,
               const juce::String& presetName, juce::String& outErr);

    // Loads onto whatever strip channelId names, rewriting the saved EQ
    // parameter prefix to the destination strip's.  Returns false + outErr when
    // the file is unreadable or the channel has no rack.
    bool load (BaySickDAWProcessor& proc, int channelId,
               const juce::File& presetFile, juce::String& outErr);
}
