#pragma once

#include <JuceHeader.h>

#include "../RenderTask.h"
#include "../BlockContext.h"

class BaySickGraph;
class ISidechainEngine;
class BaySickDAWProcessor;
class BaySickVocalProcessor;

// VoxStripTask
// ------------
// Vocal strip task -- the only render path (QA-Ef: serial fallback deleted).
//
//   1. FilePlay + NOT active (no arm, no listen): decode every routed clip
//      into a per-task sum, then engine + insert chain ONCE on the sum
//      (decodeFilePlayClip + finalizeFilePlayStrip; QA-MultiBlockHazard
//      Task 2).  Live input skipped.
//   2. Otherwise: read APVTS arm / inputChIdx / inputStereo / listen; if
//      active, copy live input from the snapshot (DRY tap fires pre-chain
//      when armed) and clear setForcePitchBypass (live mode).
//   3. QA-Fb Option A (locked 2026-07-10): live strip over FilePlay clips
//      decodes the prior takes and hands them to the engine as the
//      block-scoped monitor merge (setMonitorMergeForThisBlock) -- the
//      engine sums them in after its corrector + WET tap and before its
//      rack, so chain + NAM process the vocal stack exactly like post-stop
//      playback; muteLive covers armed && !listen.
//   4. SC predecessor pull -> ISidechainEngine push.
//   5. engine.processBlock(blockView, voxPageMidi[index]) -- corrector +
//      WET tap on the live stream, then the monitor merge, then rack + NAM.
//   6. BaySickGraph::processInsert(Vox, index, ...).
//   7. No-overlap tail: armed && !listen writes silence to mOutputBuffer
//      (capture happened; don't route).  Overlap blocks skip this -- the
//      engine's muteLive already handled it.
class VoxStripTask : public RenderTask
{
public:
    VoxStripTask (juce::AudioProcessor* engine,
                  int                   index,
                  int                   channelIdIn,
                  BaySickGraph&            graph,
                  BaySickDAWProcessor&   processor);

    void run() override;

private:
    juce::AudioProcessor*  mEngine        = nullptr;
    ISidechainEngine*      mScEngine      = nullptr;   // cached dynamic_cast
    BaySickVocalProcessor* mVocalEngine   = nullptr;   // cached dynamic_cast
    int                    mIndex         = 0;
    BaySickGraph*             mGraph         = nullptr;
    BaySickDAWProcessor*    mProcessor     = nullptr;
    juce::String           mPrefix;   // "mixer_vox_<i>"

    // Strip param pointers, resolved LAZILY on first successful lookup -- never
    // in the ctor.  These ids are created by a different message-thread path
    // (addLiveInputParams) than the one that builds this task, and nothing
    // orders the two, so a ctor-time resolve could pin nullptr permanently and
    // silently kill arm / listen / channel select on the strip.  The address is
    // stable for the APVTS lifetime (adapterTable is only ever emplaced into),
    // and plain pointers need no extra synchronization: they follow the same
    // cross-block publication the task already relies on for mCtx.
    std::atomic<float>* mArmP     = nullptr;
    std::atomic<float>* mIdxP     = nullptr;
    std::atomic<float>* mStereoP  = nullptr;
    std::atomic<float>* mListenP  = nullptr;
    std::atomic<float>* mMonModeP = nullptr;

    // QA-E Task 3 follow-up (2026-05-12): per-task FilePlay scratch buffers.
    // Replaces the previously-shared mProcessor->mAudioClipScratch +
    // mProcessor->mVoxEngineScratch.  Pre-fix MT FilePlay was a no-op (flag
    // never set); the Task 3 pre-scan move activated it AND surfaced the
    // long-acknowledged race documented at the now-stale "race in MT mode
    // but flag is constexpr false" comment.  Per-task ownership eliminates
    // the cross-task data race that produced all-3-clips-mixed-into-every-
    // strip on playback.
    // Both are grown to a full arena-sized block in the ctor (message thread);
    // run()'s per-block setSize calls then stay inside that allocation.
    juce::AudioBuffer<float> mClipScratch;
    juce::AudioBuffer<float> mEngineScratch;
};
