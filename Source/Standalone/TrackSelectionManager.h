#pragma once
#include <JuceHeader.h>

// ── TrackSelectionManager ─────────────────────────────────────────────────────
// Simple broadcaster that tracks which mixer track is currently selected for
// the Effects Page. Owned by StandaloneEditor; pointer passed to MixerPage
// (Phase 5) and EffectsPage.
//
// Usage:
//   manager.setTargetTrack(index);   // triggers sendChangeMessage()
//   manager.getCurrentTrack();       // read-only, -1 = Master
// ─────────────────────────────────────────────────────────────────────────────
class TrackSelectionManager : public juce::ChangeBroadcaster
{
public:
    TrackSelectionManager() = default;

    // -1 = Master channel
    void setTargetTrack(int index)
    {
        if (mCurrentTrack != index)
        {
            mCurrentTrack = index;
            sendChangeMessage();
        }
    }

    int getCurrentTrack() const { return mCurrentTrack; }

private:
    int mCurrentTrack { -1 }; // -1 = Master

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackSelectionManager)
};
