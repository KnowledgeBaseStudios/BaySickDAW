#pragma once
#include <JuceHeader.h>

// ── BaySickVisualizerScreen ───────────────────────────────────────────────────
// Live animated display in the top section of BaySickSynthEditor.
// Behaviour changes per active tab:
//
//   0 — OSC      : Waveform path (neon green) matching selected waveform + modifier
//   1 — OSC ENV  : Animated ADSR shape (segments expand/contract in real-time)
//   2 — FILTER   : Real-time Bode magnitude plot (cutoff slope + resonance peak)
//   3 — FLT ENV  : Same animated ADSR graph, filter envelope values
//   4 — LFO      : Continuously scrolling LFO waveform (timer 30fps), cyan
//
// All graphs update via APVTS parameter listeners that trigger repaint().
// The LFO tab additionally uses a juce::Timer for continuous scroll animation.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickVisualizerScreen : public juce::Component,
                                 private juce::AudioProcessorValueTreeState::Listener,
                                 private juce::Timer
{
public:
    // ledColour overrides the neon green used for waveform/env; LFO tab uses cyan.
    explicit BaySickVisualizerScreen (juce::Colour ledColour = juce::Colour (0xFFA0DB2B));
    ~BaySickVisualizerScreen() override;

    // Call once after construction.  Subscribes to all BaySickSynth params.
    void setup (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);

    // Optional: point the LFO visualizer at the processor's sync-aware effective
    // rate atomic. When non-null, the scrolling LFO animation uses this instead
    // of reading `lfo_rate` directly, so the visual tracks tempo-sync correctly.
    void setEffectiveLfoRateSource (std::atomic<float>* src) { mEffectiveLfoRate = src; }

    // Called by editor when the user clicks a tab button.
    void setActiveTab (int tab);  // 0-4

    void paint (juce::Graphics&) override;

private:
    void parameterChanged (const juce::String&, float) override;
    void timerCallback() override;

    // ── Per-tab paint helpers ─────────────────────────────────────────────────
    void paintOscTab    (juce::Graphics&, juce::Rectangle<float> bounds);
    void paintEnvTab    (juce::Graphics&, juce::Rectangle<float> bounds,
                         float a, float d, float s, float r);
    void paintFilterTab (juce::Graphics&, juce::Rectangle<float> bounds);
    void paintLFOTab    (juce::Graphics&, juce::Rectangle<float> bounds);

    static void drawADSR (juce::Graphics&, juce::Rectangle<float> bounds,
                          float a, float d, float s, float r,
                          juce::Colour colour);

    static float svfMagnitude (float u, float q, int filterType);
    static float lfoSample    (int shape, float phase);

    juce::AudioProcessorValueTreeState* mApvts             { nullptr };
    juce::String                        mPrefix;
    int                                 mActiveTab          { 0 };
    float                               mLFOPhase           { 0.0f };
    juce::Colour                        mLedColour;
    std::atomic<float>*                 mEffectiveLfoRate   { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickVisualizerScreen)
};
