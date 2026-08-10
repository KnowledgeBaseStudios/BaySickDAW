#pragma once
#include <JuceHeader.h>

// ── PatternColorPicker ───────────────────────────────────────────────────────
// Floating colour-picker dialog used by the pattern dropdown's "Change Color..."
// action and the Builder browser pattern items' right-click menu.
//
// Built on `juce::ColourSelector` for the standard hue slider + S/V box + RGB /
// hex inputs.  Adds a 10-slot "Recently Used" swatch row that persists in
// `Documents/BaySickDAW/settings.xml` across runs (subclass of ColourSelector
// overrides the swatch hooks).
//
// Live preview: the `onColourChanged` callback fires on every change so the
// pattern's colour updates as the user drags around the picker.  When the
// dialog closes the final colour is added to the recents list.
// ─────────────────────────────────────────────────────────────────────────────
namespace PatternColorPicker
{
    // Recent-colour list (FIFO, capped at kMaxRecents).
    constexpr int kMaxRecents = 10;

    // Load the persisted recent-colours list from settings.xml.
    std::vector<juce::Colour> loadRecents();

    // Save the recent-colours list to settings.xml.  Preserves all other
    // BaySickDAWSettings sections (RecentProjects, defaultTemplate, etc.).
    void saveRecents (const std::vector<juce::Colour>& recents);

    // Insert `c` at the front of the recent list, dedup, cap at kMaxRecents,
    // and persist.  No-op if `c` is already at the front.
    void pushRecent (juce::Colour c);

    // Open the picker dialog.  `onColourChanged` is invoked live every time
    // the user adjusts the colour.  When the dialog closes the final colour
    // is pushed onto the recent list.
    void showAsync (juce::Component* anchor,
                    juce::Colour current,
                    std::function<void(juce::Colour)> onColourChanged);

    // The same selector without a window around it, for hosts that put the
    // color choice inside a larger dialog.  Caller owns it, sizes it, and is
    // responsible for calling pushRecent() with the committed color -- a
    // hosted selector has no close of its own to hang that on.
    std::unique_ptr<juce::ColourSelector> createSelector (juce::Colour current);
}
