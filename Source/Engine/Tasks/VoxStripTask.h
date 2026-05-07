#pragma once

#include <JuceHeader.h>

#include "../RenderTask.h"
#include "../BlockContext.h"

class VibeGraph;
class ISidechainEngine;
class VibeSynthProcessor;
class BaySickVocalProcessor;

// VoxStripTask
// ------------
// Live-input vocal strip task. Mirrors the primary Vox-engine loop in
// PluginProcessor::processBlock (lines ~1446-1525):
//
//   1. Skip if FilePlay-active (the audio-clip loop drives the engine in
//      that case; see the FilePlay branch in the audio-clip rendering loop).
//   2. Read APVTS arm / inputChIdx / inputStereo / listen for this strip.
//   3. If armed and inputChIdx is in range, copy live input from the snapshot
//      into the engine scratch (mono->dual or stereo pair).
//   4. Clear setForcePitchBypass on BaySickVocalProcessor (live mode).
//   5. SC predecessor pull -> ISidechainEngine push.
//   6. engine.processBlock(scratch, voxPageMidi[index]).
//   7. VibeGraph::processInsert(Vox, index, ...).
//   8. If armed && !listen, write silence to mOutputBuffer (do not route).
//      Else mOutputBuffer's storage holds the strip's final output.
//
// Phase 4 scaffolding: code is dead at runtime while
// kEnableMultiThreadedEngine is false.
//
// Deferred to Batch 5/9:
//   - Dry-recorder tap (mStripRecorders): the serial path writes raw mono
//     input to recorders during armed playback. Reproducing this in the
//     task requires touching mStripRecorders from a worker thread. Will be
//     re-evaluated when AudioInsertTask ships in Batch 5 and recording is
//     end-to-end re-validated for the parallel path.
//   - FilePlay routing (clip drives engine): handled by AudioInsertTask in
//     Batch 5. Until then, the task writes silence when FilePlay is active.
class VoxStripTask : public RenderTask
{
public:
    VoxStripTask (juce::AudioProcessor* engine,
                  int                   index,
                  int                   channelIdIn,
                  VibeGraph&            graph,
                  VibeSynthProcessor&   processor);

    void run() override;

private:
    juce::AudioProcessor*  mEngine        = nullptr;
    ISidechainEngine*      mScEngine      = nullptr;   // cached dynamic_cast
    BaySickVocalProcessor* mVocalEngine   = nullptr;   // cached dynamic_cast
    int                    mIndex         = 0;
    VibeGraph*             mGraph         = nullptr;
    VibeSynthProcessor*    mProcessor     = nullptr;
    juce::String           mPrefix;   // "mixer_vox_<i>"
};
