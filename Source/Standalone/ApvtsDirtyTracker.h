#pragma once
#include <JuceHeader.h>
#include <functional>
#include <atomic>

// ── ApvtsDirtyTracker (2026-05-05) ───────────────────────────────────────────
// Fires `onAny` on every property change in the tracked apvts.state.
//
// Each per-engine processor (Harmless, BaySickSynth, BaySickBass, BaySickPlayer,
// BaySickGuitars, BaySickBasses, BaySickRustyDrums, BaySickPedals,
// BaySickNAMIR, BaySickVocal) owns its own `juce::AudioProcessorValueTreeState`
// - separate from the main PluginProcessor's APVTS that ProjectManager's
// dirty hook listens to.  Without this tracker, edits to the engine's own
// state ValueTree never reach the project-dirty listener, so the title-bar
// "*" indicator stays clean even when the user has tweaked dozens of knobs.
//
// Usage: declare `ApvtsDirtyTracker mDirtyTracker { apvts };` AFTER `apvts`
// in the processor's member declarations (so apvts is fully constructed by
// the time the tracker installs its listener).  Expose
// `setOnAnyStateChange(...)` so external code (StandaloneEditor) can wire
// the callback to `ProjectManager::markDirty`.
//
// QA-EngineApvts (2026-05-30): attach DIRECTLY to `apvts.state` (not a copy)
// so JUCE's `ValueTree::operator=` migrates this listener across `replaceState`
// (preset / project / clone loads) -- the same framework contract the engine
// editors + pages already rely on.  A copy of apvts.state would be orphaned by
// the swap; a direct attachment is re-pointed and gets `valueTreeRedirected`.
// This keeps the per-block dirty flag (`mDirty`) valid across state swaps for
// all 10 engines with zero per-call-site bookkeeping.
// ─────────────────────────────────────────────────────────────────────────────
class ApvtsDirtyTracker : public juce::ValueTree::Listener
{
public:
    explicit ApvtsDirtyTracker (juce::AudioProcessorValueTreeState& apvts)
        : mApvts (apvts)
    {
        mApvts.state.addListener (this);
    }

    ~ApvtsDirtyTracker() override
    {
        mApvts.state.removeListener (this);
    }

    std::function<void()> onAny;

    // QA-EngineApvts: lock-free per-block gate (audio thread).  Returns true and
    // clears if any param changed -- or the state tree was swapped -- since the
    // last call.  Directly attached to apvts.state, so it survives replaceState
    // via JUCE listener migration; no resync bookkeeping needed.
    bool hasChangedSinceLastBlock() noexcept
    {
        return mDirty.exchange (false, std::memory_order_acquire);
    }

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override
    {
        mDirty.store (true, std::memory_order_release);   // QA-EngineApvts perf gate
        if (onAny) onAny();
    }
    void valueTreeChildAdded         (juce::ValueTree&, juce::ValueTree&) override {}
    void valueTreeChildRemoved       (juce::ValueTree&, juce::ValueTree&, int) override {}
    void valueTreeChildOrderChanged  (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged      (juce::ValueTree&) override {}

    // QA-EngineApvts: fires when apvts.replaceState swaps the root tree (preset /
    // project / clone load).  Re-arm the dirty flag so the next block re-reads the
    // loaded params; JUCE auto-migrates this listener onto the new tree.  (onAny is
    // deliberately NOT called here -- a load is not a user edit, and project loads
    // are dirty-suppressed upstream.)
    void valueTreeRedirected (juce::ValueTree&) override
    {
        mDirty.store (true, std::memory_order_release);
    }

    juce::AudioProcessorValueTreeState& mApvts;
    std::atomic<bool>                   mDirty { true };   // armed at ctor so the first block runs updateFromApvts() (initial param sync)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApvtsDirtyTracker)
};
