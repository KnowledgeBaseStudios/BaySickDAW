#pragma once

#include <JuceHeader.h>
#include <atomic>

#include "../RenderTask.h"
#include "../BlockContext.h"

class VibeGraph;
class VibeSynthProcessor;

// MasterTask
// ----------
// The terminal task in the render graph.  Sums upstream bus + direct-to-
// master predecessor outputs into its arena slot, runs the master DSP via
// VibeGraph::processMasterBus (extracted Batch 8), and signals the
// dispatcher's mAllDone flag so the parallel pump unblocks.
//
// channelId = MixerChannelIds::kMaster (4).
//
// run() flow:
//   1. Clear the master arena slot.
//   2. Iterate mPredecessors → addFrom each upstream's source buffer with
//      the link's gain (main-out = unity, sends = dB-derived) into the
//      arena slot.  Predecessors include the 11 bus PassiveStripTasks
//      whose _sendTo defaults to kMaster, plus any direct-to-master
//      insert sends configured via the routing graph.  Those bus cables are
//      main-out edges and so always read mOutputBuffer; only a direct
//      SEND here can select the source's pre-fader tap (SendSourceRead.h).
//   3. Call mGraph->processMasterBus(blockView, mCtx->bpm) - runs master
//      rack + EQ + fader + peak drain in-place.
//   4. Set mDoneFlag.store(true, memory_order_release).  This is the
//      signal VibeThreadPool::runUntilOrTimeout is polling; release ordering
//      pairs with the dispatcher's acquire on the same flag and publishes
//      our writes to the arena slot.
//
// QA-Ef (2026-05-21): this is the live render terminal.  The full pump
// (reset counters -> seed leaves -> runUntilOrTimeout -> master signals done
// -> copy to host buffer) is wired in dispatchBlock.
class MasterTask : public RenderTask
{
public:
    MasterTask (VibeGraph&          graph,
                VibeSynthProcessor& processor,
                std::atomic<bool>&  doneFlag);

    void run() override;

private:
    VibeGraph*          mGraph     = nullptr;
    VibeSynthProcessor* mProcessor = nullptr;
    std::atomic<bool>*  mDoneFlag  = nullptr;   // dispatcher's mAllDone
};
