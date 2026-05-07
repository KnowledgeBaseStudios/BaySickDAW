#pragma once
#include <JuceHeader.h>
#include <functional>

#include "../EffectRack.h"

class DSPBase;

// ─────────────────────────────────────────────────────────────────────────────
// EffectPresetIO - H-9 prep (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// Generic factory + user preset save/load for every effect-rack DSP.  Adds
// a Preset menu next to each SlotComponent's bypass LED so users can:
//   - Save Current Preset    → writes XML to My Presets folder
//   - Load Preset             → walks Factory + My Presets folders
//   - Restore Default         → resets DSP to construction defaults
//   - Save as Default         → user-defined "Init" preset for this effect
//   - Manage Presets          → opens the folder in Explorer
//
// Folder layout:
//   Documents/BaySickDAW/Presets/Effects/{TypeName}/Factory/{name}.xml
//   Documents/BaySickDAW/Presets/Effects/{TypeName}/My Presets/{name}.xml
//   Documents/BaySickDAW/Presets/Effects/{TypeName}/Default.xml   (optional)
//
// Factory presets are SEEDED on first launch via seedFactoryPresets():
// each factory preset is defined as a configure-lambda that takes a fresh
// DSPBase + sets its parameters; the framework runs the lambda on a temp
// DSP, calls getStateInformation, writes the XML to disk.  After seeding
// the Factory folder is read-only by convention -- if the user deletes a
// factory preset, the next app launch re-seeds it.
//
// Preset XML format: just the DSP's getStateInformation blob, base64-encoded
// inside a wrapper <EffectPreset> element with metadata (effectType, name,
// timestamp).  Loading uses setStateInformation so per-DSP state (Type
// umbrella, A/B snapshots, etc.) round-trips faithfully.
// ─────────────────────────────────────────────────────────────────────────────

namespace EffectPresetIO
{
    // Folder names per effect type (used for both filesystem layout and
    // user-facing menu titles).  Keep these stable -- changing them breaks
    // all existing user presets.
    juce::String typeFolderName (EffectType type);

    // Filesystem helpers ----------------------------------------------------
    juce::File   presetsRoot();                         // .../Presets/Effects
    juce::File   typeRoot     (EffectType type);        // .../Presets/Effects/{Type}
    juce::File   factoryDir   (EffectType type);        // .../{Type}/Factory
    juce::File   myPresetsDir (EffectType type);        // .../{Type}/My Presets
    juce::File   defaultFile  (EffectType type);        // .../{Type}/Default.xml

    void ensureFolderTree (EffectType type);   // mkdir-p the three folders above

    // Seeding ---------------------------------------------------------------
    // Run once at app startup -- writes any missing factory preset XMLs
    // to disk based on the static factory-preset table in EffectPresetIO.cpp.
    // Idempotent: existing factory files are not overwritten unless the
    // user deleted them (since we write on missing-file only).
    void seedFactoryPresets();

    // H-10 cutover (2026-05-02): one-time migration of legacy Tape preset
    // folder.  Moves any user-saved XMLs in Tape/My Presets/ into
    // Saturation/My Presets/ (skipping name collisions) so the user's
    // custom Tape presets remain accessible after the fold.  Also wipes
    // the stale Tape/Factory/ directory since those presets now live
    // under Saturation/Factory/ (named "Tape Vintage" etc).  Idempotent:
    // safe to call every launch -- after the first run there's nothing
    // to migrate or clean.  Call BEFORE seedFactoryPresets so the new
    // Saturation tape presets get seeded with the right names on disk.
    void migrateTapeFolderToSaturation();

    // Save / load -----------------------------------------------------------
    // Save a DSP's current state to the named user preset.  Returns true
    // on success; on failure outErr describes why.
    bool savePreset (DSPBase& dsp,
                       EffectType type,
                       const juce::String& presetName,
                       juce::String& outErr);

    // Load a preset XML into the DSP.  Works for both factory and user
    // presets (path is fully qualified).
    bool loadPreset (DSPBase& dsp,
                       const juce::File& presetFile,
                       juce::String& outErr);

    // Restore DSP to its construction-default state (no preset file
    // involved -- just calls DSP's setStateInformation with empty data
    // OR re-instantiates a temp DSP and copies its state).  Implementation
    // uses the "fresh DSP" approach for cleanliness.
    void restoreDefaults (DSPBase& dsp, EffectType type);

    // Save the DSP's current state as the user's "default" preset for
    // this effect type.  Stored at Default.xml in the type root.  When
    // the user adds a new instance of this effect, the Slot factory could
    // read this and apply it (deferred -- not all callers handle this yet).
    bool saveAsDefault (DSPBase& dsp, EffectType type, juce::String& outErr);

    // Enumeration -----------------------------------------------------------
    juce::Array<juce::File> enumerateFactory   (EffectType type);
    juce::Array<juce::File> enumerateMyPresets (EffectType type);
}
