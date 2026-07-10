#pragma once

#include <JuceHeader.h>

#include "../RenderTask.h"
#include "../BlockContext.h"

class VibeGraph;
class ISidechainEngine;
class VibeSynthProcessor;

// InstStripTask
// -------------
// Live-input + source-mode-aware instrument strip task -- the only render
// path (QA-Ef: serial fallback deleted).
//
//   1. Detect sfizz-source slot (mGuitarsActive[index] || mBassesActive[index]);
//      read APVTS arm / inputChIdx / inputStereo / listen (arm/listen force-
//      disabled when sfizz-active -- sfizz drives audio from MIDI).
//   2. FilePlay + NOT active: decode every routed clip into a per-task sum,
//      then engine + insert chain ONCE on the sum (decodeFilePlayClip +
//      finalizeFilePlayStrip; QA-MultiBlockHazard Task 2).
//   3. Idle suspend (sfizz-source only): if MIDI is empty AND sfizz reports
//      0 active voices for kIdleSuspendBlocks consecutive blocks, skip the
//      whole chain. Wake instantly on next block where any gate fails.
//   4. If active, copy live input from the snapshot (DRY tap fires on the
//      raw snapshot when armed).
//   5. QA-Fb Option A (locked 2026-07-10): live strip over FilePlay clips
//      sums the decoded prior takes with the live DI BEFORE the engine --
//      one chain pass over the stack, bit-identical to post-stop playback;
//      armed && !listen clears the live monitor pre-merge (captured, not
//      monitored).
//   6. SC predecessor pull -> ISidechainEngine push.
//   7. engine.processBlock(blockView, instPageMidi[index]).
//   8. VibeGraph::processInsert(Inst, index, ...).
//   9. Listen gate (no-overlap live case) -> silence if monitor off.
//
// QA-Ef (2026-05-21): this is the live audio plumbing.  Dry-recorder tap is
// wired via VibeSynthProcessor::tapDryRecorder (see ::run).
class InstStripTask : public RenderTask
{
public:
    InstStripTask (juce::AudioProcessor* engine,
                   int                   index,
                   int                   channelIdIn,
                   VibeGraph&            graph,
                   VibeSynthProcessor&   processor);

    void run() override;

private:
    juce::AudioProcessor* mEngine    = nullptr;
    ISidechainEngine*     mScEngine  = nullptr;   // cached dynamic_cast
    int                   mIndex     = 0;
    VibeGraph*            mGraph     = nullptr;
    VibeSynthProcessor*   mProcessor = nullptr;
    juce::String          mPrefix;   // "mixer_inst_<i>"

    // QA-E Task 3 follow-up (2026-05-12): per-task FilePlay scratch buffers.
    // See VoxStripTask.h for full rationale -- per-task ownership eliminates
    // the cross-task data race on the previously-shared processor-level
    // scratches that produced all-clips-mixed-into-every-strip on MT playback.
    juce::AudioBuffer<float> mClipScratch;
    juce::AudioBuffer<float> mEngineScratch;
};
