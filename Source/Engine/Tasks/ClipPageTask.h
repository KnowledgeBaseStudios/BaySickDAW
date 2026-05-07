#pragma once

#include <JuceHeader.h>

#include "../RenderTask.h"
#include "../BlockContext.h"

class VibeGraph;
class ISidechainEngine;
class VibeSynthProcessor;

// ClipPageTask
// ------------
// Sample-trigger clip page (G-3) task. Same shape as EngineInsertTask but:
//   - InsertKind::Audio (clip pages share the audio insert for their row)
//   - channelId = audioInsert(pageIdx)  (1:1 mapping pageIdx <-> audio row)
//   - drains peak meters into VibeSynthProcessor::mAudioRowPeakDb*Run after
//     processInsert, so audio-row meters reflect clip-engine output.
//
// Mirrors PluginProcessor::processBlock's ClipPage loop (the
// `if (mAnyClipPageActive...) { ... }` block).
//
// Phase 5 scaffolding: dead at runtime while kEnableMultiThreadedEngine is
// constexpr false.  When the flag flips in Batch 9, the dispatcher will
// drive these tasks alongside Layer/Bass/Drum/Vox/Inst tasks.
class ClipPageTask : public RenderTask
{
public:
    ClipPageTask (juce::AudioProcessor* engine,
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
};
