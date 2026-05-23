#pragma once

#include <JuceHeader.h>

#include "../RenderTask.h"
#include "../BlockContext.h"

class VibeGraph;
class ISidechainEngine;

namespace VibeGraphInsertKindBridge { enum class Kind { Layer, Bass, Drum }; }

// EngineInsertTask
// ----------------
// One mixer-strip-as-task for the simplest engine pattern (Layer / Bass /
// Drum). Wraps:
//
//   1. Sidechain push: pull from any SC predecessor links and feed each
//      source's mOutputBuffer into the engine via ISidechainEngine.
//   2. Engine render: engine.processBlock(blockView, midiBuffer).  Block
//      view is built per-block from mOutputBuffer's storage so getNumSamples
//      reports the host's current block size, not the arena's max.
//   3. Insert chain: VibeGraph::processInsert (polarity → preEq → width →
//      rack → postEq → fader × mute × solo → PDC → peak), in-place on
//      blockView.
//
// After run() returns, mOutputBuffer's storage holds this strip's final
// post-everything output. Downstream tasks pull from it via their
// mPredecessors entries.
//
// QA-Ef (2026-05-21): this is the live audio plumbing.  Tasks register /
// unregister in lockstep with engine create/destroy so every active Layer /
// Bass / Drum has a wrapper.
class EngineInsertTask : public RenderTask
{
public:
    // Match VibeGraph::InsertKind without including VibeGraph.h here.
    // The .cpp casts to the real enum.
    using Kind = VibeGraphInsertKindBridge::Kind;

    EngineInsertTask (juce::AudioProcessor* engine,
                      Kind                  kind,
                      int                   index,
                      int                   channelIdIn,
                      VibeGraph&            graph);

    void run() override;

private:
    juce::MidiBuffer* resolveMidiBuffer() const noexcept;

    juce::AudioProcessor* mEngine   = nullptr;
    ISidechainEngine*     mScEngine = nullptr;   // cached dynamic_cast
    Kind                  mKind     = Kind::Layer;
    int                   mIndex    = 0;
    VibeGraph*            mGraph    = nullptr;
};
