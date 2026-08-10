#pragma once
#include <atomic>
#include <JuceHeader.h>

// ── BaySickVisualizerScreen ───────────────────────────────────────────────────
// Live animated display in the top section of BaySickSynthEditor.
// Behaviour changes per active tab:
//
//   0 - OSC      : Waveform path (neon green) matching selected waveform + modifier
//   1 - OSC ENV  : Animated ADSR shape (segments expand/contract in real-time)
//   2 - FILTER   : Real-time Bode magnitude plot (cutoff slope + resonance peak)
//   3 - FLT ENV  : Same animated ADSR graph, filter envelope values
//   4 - LFO      : Continuously scrolling LFO waveform (timer 30fps), cyan
//
// APVTS parameter listeners mark the screen dirty; the 30 fps timer issues the
// actual repaint.  The listener fires on whichever thread wrote the parameter,
// and during an offline render that is the render thread -- calling repaint()
// there touches JUCE UI state from a non-message thread.
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

    // THREAD SAFETY: set by parameterChanged on the writer's thread, consumed
    // by timerCallback on the message thread.  The timer already runs
    // unconditionally at 30 fps, so this needs no lifecycle of its own.
    std::atomic<bool>                   mNeedsRepaint       { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickVisualizerScreen)
};
