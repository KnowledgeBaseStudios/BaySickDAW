#pragma once
#include <JuceHeader.h>

class VibeSynthProcessor;

// ─────────────────────────────────────────────────────────────────────────────
// J-8 Part D (2026-05-05): page-preset I/O for the BaySickRustyDrums singleton.
//
// Captures the full Rusty player + mixer setup so the user can save and recall
// a complete kit configuration without losing any custom mix or effect work.
// Captured surfaces:
//
//   1. Engine processor state         (BaySickRustyDrumsProcessor APVTS state
//                                      including the kit path + every brd_cc<N>
//                                      MIDI-CC value + brd_outVol)
//   2. RustyDrums Bus mixer params    (level / pan / mute / solo / polarity /
//                                      width / sends / pre + post EQ8 M/S)
//   3. Per-Rusty-strip mixer params   (level / pan / mute / solo / polarity /
//                                      width / bypass / arm / sends / pre +
//                                      post EQ8 M/S, for every active strip)
//   4. RustyDrums Bus effects rack    (rack + post-EQ DSP state, base64'd)
//   5. Per-Rusty-strip effects racks  (rack + post-EQ DSP state per strip)
//
// NOT captured: the BaySickRustyDrums piano-roll patterns (musical content
// the user is composing, not part of the kit/mix sound design).
//
// Stored under `Documents/BaySickDAW/Presets/Rusty Drums Page/My Presets/<name>.xml`.
// ─────────────────────────────────────────────────────────────────────────────
namespace RustyDrumsPagePresetIO
{
    juce::File presetsDir();        // base folder
    juce::File myPresetsDir();      // user-saved subfolder

    // Capture the full Rusty page chain to an XML string.  Returns empty
    // string when no engine is loaded (nothing to save).
    juce::String exportPreset (VibeSynthProcessor& processor);

    // Restore the full Rusty page chain from saved XML.  Reloads the kit if
    // the saved kit path differs from the currently-loaded one (or no kit is
    // currently loaded).  Returns true on full success, false on any failure
    // (parse, missing kit file, etc.).
    bool importPreset (VibeSynthProcessor& processor, const juce::String& xml);
}
