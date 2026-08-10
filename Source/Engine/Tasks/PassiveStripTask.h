#pragma once

#include <JuceHeader.h>

#include "../RenderTask.h"
#include "../BlockContext.h"

class VibeGraph;
class VibeSynthProcessor;

// PassiveStripTask
// ----------------
// Inner DAG node task for strips that DON'T own an engine - Aux strips and
// the always-on Bus strips - kNumBatch7Buses of them, enumerated by
// kBusChannelIds in prepareToPlay (Master is excluded; it has MasterTask).
// Pull-model:
//
//   1. Iterate mPredecessors → addFrom each upstream's source buffer with
//      the link's gain (main-out = 1.0, sends = decibel-derived) into our
//      own arena slot.  That buffer is the upstream's mOutputBuffer, except
//      for a PRE-FADER send, which reads the upstream's pre-fader tap
//      instead (SendSourceRead.h).  Aux strips are the usual destination of
//      those, so this is the main consumer of the feature.
//   2. Run the strip's DSP chain:
//        Aux → VibeGraph::processInsert(Aux, idx, ...) (existing API).
//        Bus → VibeGraph::processBus(channelId, ...) (Batch 9b).
//   3. mOutputBuffer now holds the strip's final output; downstream tasks
//      pull from it.
//
// Bus DSP migration (Batch 9b, 2026-05-06)
//   processBus is the single bus DSP entry point for every bus strip.
//   Switches on channelId
//   to the right per-bus chain (Layers/Bass/Drums via BusNode helpers,
//   every non-master bus → InstrChannelNode::processChainOnly (CL-301),
//   Master → processMasterBus).
//
// QA-Ef (2026-05-21): this is the live audio plumbing -- the dispatcher is
// the single render path.
class PassiveStripTask : public RenderTask
{
public:
    enum class Kind { Aux, Bus };

    PassiveStripTask (Kind                kind,
                      int                 auxOrBusIndex,
                      int                 channelIdIn,
                      VibeGraph&          graph,
                      VibeSynthProcessor& processor);

    void run() override;

private:
    Kind                mKind;
    int                 mIndex;       // For Aux: 0..17. For Bus: not used (channelId carries the bus id)
    VibeGraph*          mGraph;
    VibeSynthProcessor* mProcessor;
};
