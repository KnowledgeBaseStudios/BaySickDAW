#pragma once

#include <JuceHeader.h>

#include "../RenderTask.h"
#include "../BlockContext.h"

class VibeGraph;
class ISidechainEngine;
class VibeSynthProcessor;

// InstStripTask
// -------------
// Live-input + source-mode-aware instrument strip task. Mirrors the primary
// Inst-engine loop in PluginProcessor::processBlock (lines ~1528-1640):
//
//   1. Skip if FilePlay-active (audio-clip loop drives the engine in that
//      case; AudioInsertTask owns that path in Batch 5).
//   2. Detect sfizz-source slot (mGuitarsActive[index] || mBassesActive[index]).
//   3. Idle suspend (sfizz-source only): if MIDI is empty AND sfizz reports
//      0 active voices for kIdleSuspendBlocks consecutive blocks, skip the
//      whole chain. Wake instantly on next block where any gate fails.
//   4. Read APVTS arm / inputChIdx / inputStereo / listen.  Arm is force-
//      disabled when sfizz-active (sfizz drives audio from MIDI).
//   5. If armed && !sfizz, copy live input from the snapshot into the engine
//      scratch (mono->dual or stereo pair).
//   6. SC predecessor pull -> ISidechainEngine push.
//   7. engine.processBlock(scratch, instPageMidi[index]).
//   8. VibeGraph::processInsert(Inst, index, ...).
//   9. Listen gate (live-input case) -> silence if monitor off.
//
// QA-Ef (2026-05-21): this is the live audio plumbing.  Dry-recorder tap is
// wired via VibeSynthProcessor::tapDryRecorder (see ::run).  QA-MultiBlockHazard
// (Task 2): the FilePlay branch decodes every routed clip into a per-task sum,
// then runs the engine + insert chain ONCE via VibeSynthProcessor::
// decodeFilePlayClip + finalizeFilePlayStrip.
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
