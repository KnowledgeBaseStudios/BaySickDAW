#pragma once

#include <JuceHeader.h>

#include "../RenderTask.h"
#include "../BlockContext.h"
#include "IdleSuspendFade.h"

class BaySickGraph;
class ISidechainEngine;
class BaySickDAWProcessor;

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
//      0 active voices for kIdleSuspendBlocks consecutive blocks, fade the
//      strip out over IdleSuspendFade::kFadeOutSeconds and only then skip the
//      whole chain.  Wakes on the next block where any gate fails, ramping
//      back to unity from wherever the fade had got to.
//   4. If active, copy live input from the snapshot (DRY tap fires on the
//      raw snapshot when armed).
//   5. QA-Fb Option A (locked 2026-07-10): live strip over FilePlay clips
//      sums the decoded prior takes with the live DI BEFORE the engine --
//      one chain pass over the stack, bit-identical to post-stop playback;
//      armed && !listen clears the live monitor pre-merge (captured, not
//      monitored).
//   6. SC predecessor pull -> ISidechainEngine push.
//   7. engine.processBlock(blockView, instPageMidi[index]).
//   8. BaySickGraph::processInsert(Inst, index, ...).
//   9. Listen gate (no-overlap live case) -> silence if monitor off.
//
// QA-Ef (2026-05-21): this is the live audio plumbing.  Dry-recorder tap is
// wired via BaySickDAWProcessor::tapDryRecorder (see ::run).
class InstStripTask : public RenderTask
{
public:
    InstStripTask (juce::AudioProcessor* engine,
                   int                   index,
                   int                   channelIdIn,
                   BaySickGraph&            graph,
                   BaySickDAWProcessor&   processor);

    void run() override;

private:
    juce::AudioProcessor* mEngine    = nullptr;
    ISidechainEngine*     mScEngine  = nullptr;   // cached dynamic_cast
    int                   mIndex     = 0;
    BaySickGraph*            mGraph     = nullptr;
    BaySickDAWProcessor*   mProcessor = nullptr;
    juce::String          mPrefix;   // "mixer_inst_<i>"

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
    // See VoxStripTask.h for full rationale -- per-task ownership eliminates
    // the cross-task data race on the previously-shared processor-level
    // scratches that produced all-clips-mixed-into-every-strip on MT playback.
    // Both are grown to a full arena-sized block in the ctor (message thread);
    // run()'s per-block setSize calls then stay inside that allocation.
    juce::AudioBuffer<float> mClipScratch;
    juce::AudioBuffer<float> mEngineScratch;

    // QA-OctavePedal Task 5: live-monitor fork (Dry vs With Effect).  The
    // pre-engine live signal is stashed here so the monitor output can cross-
    // fade back to it in Dry mode without a click.  mMonitorDryGain is the
    // smoothed dry weight (0 = With Effect, 1 = Dry); ramped one block per flip.
    // Owned per-task (audio thread only) -- no cross-thread access.  Pre-sized
    // in the ctor for the same reason as the scratches above.
    juce::AudioBuffer<float> mMonitorDryBuf;
    float                    mMonitorDryGain { 0.0f };

    // Idle-suspend shutdown envelope (see IdleSuspendFade.h).  The suspend
    // skips the engine AND the insert chain, so without this the rack / NAM
    // tail the chain was still rendering is cut to zero in one sample.
    IdleSuspendFade mSuspendFade;
};
