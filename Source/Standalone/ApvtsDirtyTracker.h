#pragma once
#include <JuceHeader.h>
#include <functional>

// ── ApvtsDirtyTracker (2026-05-05) ───────────────────────────────────────────
// Fires `onAny` on every property change in the tracked apvts.state.
//
// Each per-engine processor (Harmless, BaySickSynth, BaySickBass, VibePlayer,
// BaySickGuitars, BaySickBasses, BaySickRustyDrums, BaySickPedals,
// BaySickNAMIR, BaySickVocal) owns its own `juce::AudioProcessorValueTreeState`
// — separate from the main PluginProcessor's APVTS that ProjectManager's
// dirty hook listens to.  Without this tracker, edits to the engine's own
// state ValueTree never reach the project-dirty listener, so the title-bar
// "*" indicator stays clean even when the user has tweaked dozens of knobs.
//
// Usage: declare `ApvtsDirtyTracker mDirtyTracker { apvts };` AFTER `apvts`
// in the processor's member declarations (so apvts is fully constructed by
// the time the tracker installs its listener).  Expose
// `setOnAnyStateChange(...)` so external code (StandaloneEditor) can wire
// the callback to `ProjectManager::markDirty`.
// ─────────────────────────────────────────────────────────────────────────────
class ApvtsDirtyTracker : public juce::ValueTree::Listener
{
public:
    explicit ApvtsDirtyTracker (juce::AudioProcessorValueTreeState& apvts)
        : mState (apvts.state)
    {
        mState.addListener (this);
    }

    ~ApvtsDirtyTracker() override
    {
        mState.removeListener (this);
    }

    std::function<void()> onAny;

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override
    {
        if (onAny) onAny();
    }
    void valueTreeChildAdded         (juce::ValueTree&, juce::ValueTree&) override {}
    void valueTreeChildRemoved       (juce::ValueTree&, juce::ValueTree&, int) override {}
    void valueTreeChildOrderChanged  (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged      (juce::ValueTree&) override {}
    void valueTreeRedirected         (juce::ValueTree&) override {}

    juce::ValueTree mState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApvtsDirtyTracker)
};
