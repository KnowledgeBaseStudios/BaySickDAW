#pragma once

#include <JuceHeader.h>

#include "../RenderTask.h"
#include "../BlockContext.h"

class VibeGraph;
class ISidechainEngine;
class VibeSynthProcessor;
class VibePlayerProcessor;

// CompositeAudioInsertTask
// ------------------------
// 2026-05-07 (QA-0): single render task per audio row that owns BOTH
// previously-conflicting flows at channelId = audioInsert(row):
//
//   1. Clip-engine flow (was ClipPageTask): a sampler-style
//      juce::AudioProcessor that the user assigned to a Clips ribbon tab,
//      driven by piano-roll MIDI from BlockContext::clipPageMidi[row].
//      processBlock writes RAW engine audio into the row's arena slot.
//
//   2. Arrangement-clip flow (was AudioInsertTask): per-row decode of
//      every NON-FilePlay AudioClipPlayer through the shared helper
//      VibeSynthProcessor::renderAudioClipsForRow (mtDest = blockView).
//      Each clip's RAW decoded output is ADDED to blockView (additive).
//
// QA-MultiBlockHazard: both flows sum their RAW output into blockView, then the
// insert chain (processInsert: polarity / preEq / width / rack / postEq / fader
// x mute x solo / PDC / peak) runs exactly ONCE per block on the summed buffer
// -- so a stateful rack advances once per block, not once per source.
//
// Order inside run(): clear blockView once -> clip-engine flow (raw, if engine
// set) -> arrangement-clip flow (raw, if song mode + has clips) -> single
// processInsert (iff >=1 source contributed; N->1 calls, never 0->1).  The
// summed post-chain output lands in mOutputBuffer at audioInsert(row) for the
// row's downstream consumers.
//
// Lifecycle (Strategy 1a):
//   - One instance per audio row, created by
//     VibeSynthProcessor::ensureAudioInsert and registered with the
//     dispatcher under audioInsert(row).
//   - registerClipEngine(pageIdx, eng) sets mClipEngine on the existing
//     instance (no separate task registered, no most-recent-wins).
//   - unregisterClipEngine clears mClipEngine (instance survives).
//   - Instance lives for the project lifetime (matches the existing
//     "no removeAudioInsert hook" pattern in ensureAudioInsert).
class CompositeAudioInsertTask : public RenderTask
{
public:
    CompositeAudioInsertTask (int                 row,
                              int                 channelIdIn,
                              VibeGraph&          graph,
                              VibeSynthProcessor& processor);

    void run() override;

    // Set/clear the optional clip-engine pointer (sampler-style juce::AudioProcessor).
    // Called from VibeSynthProcessor::registerClipEngine /
    // unregisterClipEngine.  setClipEngine(nullptr) is the unregister path.
    // Safe to call from the message thread; the audio thread reads via
    // an acquire-load on the same atomic.
    void setClipEngine (juce::AudioProcessor* engine);

    // Per-task scratch handed to renderAudioClipsForRow when this task's
    // arrangement-clip flow runs.  Per-task rather than a shared processor
    // buffer: rows decode concurrently, and one shared scratch cross-pollutes
    // every strip with every row's clips (the pre-9b race).
    juce::AudioBuffer<float>& getClipScratch (int numChannels, int numSamples);

private:
    int                                mIndex     = 0;
    VibeGraph*                         mGraph     = nullptr;
    VibeSynthProcessor*                mProcessor = nullptr;

    // Clip-engine flow state.
    std::atomic<juce::AudioProcessor*> mClipEngine { nullptr };
    std::atomic<ISidechainEngine*>     mScEngine   { nullptr };
    // QA-ClipPlayback Task 2: cached BaySickPlayer cast of mClipEngine (null when
    // the clip engine isn't a BaySickPlayer, e.g. NAM/IR) so the timeline-WAV
    // decode reads its Player controls without a per-block dynamic_cast.
    std::atomic<VibePlayerProcessor*>  mClipPlayer { nullptr };

    // Arrangement-clip per-task scratch (pre-9b cross-task race fix).
    juce::AudioBuffer<float>           mClipScratch;
};
