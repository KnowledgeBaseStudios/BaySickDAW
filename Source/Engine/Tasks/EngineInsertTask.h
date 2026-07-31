#pragma once

#include <JuceHeader.h>

#include "../RenderTask.h"
#include "../BlockContext.h"

class VibeGraph;
class ISidechainEngine;
// TS7 §6.3: freeze playback reuses the clip streamer -- SPSC ring, background
// prefetch AND TS2's offline synchronous-read mode, so a frozen tab renders
// correctly inside an export for free.  Forward-declared; only the .cpp reads it.
class AudioClipStreamer;

// QA-ModelShell TS6 (BLU-447): Plugin joins the engine-driven kinds -- a hosted
// VST3 instrument is rendered exactly like a Layer, differing only in which
// per-tab MIDI array it reads.
namespace VibeGraphInsertKindBridge { enum class Kind { Layer, Bass, Drum, Plugin }; }

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

    // TS7 §6.3 freeze: setFrozenSource / getFrozenSource / mFrozenSource moved
    // to RenderTask (2026-07-30) so Vox and Inst strips, which are RenderTasks
    // rather than EngineInsertTasks, can be frozen too.  See the base for the
    // ownership and memory-ordering contract.

private:
    juce::MidiBuffer* resolveMidiBuffer() const noexcept;

    juce::AudioProcessor* mEngine   = nullptr;
    ISidechainEngine*     mScEngine = nullptr;   // cached dynamic_cast
    Kind                  mKind     = Kind::Layer;
    int                   mIndex    = 0;
    VibeGraph*            mGraph    = nullptr;
};
