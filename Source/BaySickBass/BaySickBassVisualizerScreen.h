#pragma once
#include "../BaySickSynth/BaySickVisualizerScreen.h"

// ── BaySickBassVisualizerScreen ───────────────────────────────────────────────
// Reuses BaySickVisualizerScreen with the bass neon-green LED colour
// (0xFF33FF88) instead of the synth yellow-green (0xFFA0DB2B).
// ─────────────────────────────────────────────────────────────────────────────
class BaySickBassVisualizerScreen : public BaySickVisualizerScreen
{
public:
    BaySickBassVisualizerScreen()
        : BaySickVisualizerScreen (juce::Colour (0xFF33FF88)) {}
};
