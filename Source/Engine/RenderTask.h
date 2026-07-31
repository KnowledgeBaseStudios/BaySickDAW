#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

#include "UpstreamLink.h"

// Forward declarations
class RenderGraphDispatcher;
class AudioClipStreamer;   // TS7 §6.3 freeze source
struct BlockContext;

// Base class for one node in the render graph. Typically wraps one mixer
// strip's processing - engine dispatch + insert chain + output publish for a
// leaf, or upstream sum + bus chain + publish for a passive bus.
//
// Memory ordering (CRITICAL - see VibeThreadPool.cpp for enforcement):
//   - When a worker finishes a task and decrements each child's mDeps, the
//     decrement uses memory_order_acq_rel. The release half publishes this
//     task's writes to mOutputBuffer; the acquire half pairs with downstream
//     tasks' reads of the counter.
//   - When a worker picks up a task whose mDeps just hit zero, the load uses
//     acquire ordering, which guarantees visibility of upstream output buffer
//     writes.
//
// Layout
//   alignas(64) on the class pads the struct to a cache line, preventing the
//   atomic dependency counter from sharing a cache line with adjacent tasks
//   in any container that holds them by value or contiguously.
//
// Lifetime
//   Concrete subclasses are owned by PluginProcessor (typically inside the
//   per-engine creation/destruction paths). The dispatcher only holds
//   non-owning pointers via registerTask / unregisterTask. Tasks must outlive
//   any block in which the dispatcher might run them; in practice this means
//   teardown happens during the project-load barrier or in releaseResources
//   when the dispatcher is guaranteed quiescent.
class alignas (64) RenderTask
{
public:
    virtual ~RenderTask() = default;

    // Strip identity. Matches MixerChannelIds for diagnostics + bookkeeping.
    int channelId = -1;

    // Runtime-mutable: count of upstream tasks not yet finished. Reset to
    // mInitialDeps at the top of each block by the dispatcher.
    alignas (64) std::atomic<int> mDeps { 0 };

    // Snapshot computed at graph build time. The dispatcher stores this and
    // restores mDeps to it at the top of every block.
    int mInitialDeps = 0;

    // Downstream tasks that depend on this one. When this task finishes, the
    // dispatcher / pool decrements each child's mDeps and pushes any that hit
    // zero into the ready queue.
    std::vector<RenderTask*> mChildren;

    // Upstream tasks this one consumes. PULL-model design: this task's run()
    // iterates mPredecessors and reads each source's mOutputBuffer. Built by
    // RenderGraphDispatcher::rebuildLinks from RoutingGraph::edges() +
    // scEdges() whenever topology changes. Stable for the duration of a
    // block (rebuilt only between blocks).
    std::vector<UpstreamLink> mPredecessors;

    // Output buffer view. Owned by ChannelBufferArena; populated by the
    // dispatcher when the task is registered. Each strip writes ONLY to this
    // buffer; downstream tasks pull from it.
    juce::AudioBuffer<float>* mOutputBuffer = nullptr;

    // Per-block context (sample count, BPM, position info, MIDI buffers).
    // Set by the dispatcher before submitting any task. Concrete tasks read
    // from this in run().
    const BlockContext* mCtx = nullptr;

    // The actual work. Implementations must be RT-safe:
    //   - zero memory allocation
    //   - zero locks
    //   - zero OS calls (no DBG, no file I/O)
    //
    // Contract:
    //   1. Read mNumSamples from mCtx, plus any input the task needs.
    //   2. Write `mNumSamples` samples into mOutputBuffer.
    //   3. Return. The pool handles dependency propagation.
    virtual void run() = 0;

    // ── TS7 §6.3: freeze source ───────────────────────────────────────────────
    // Non-null means this strip plays a cached file instead of doing its own
    // work.  Nothing is destroyed -- the engine (or the clip decode) is left in
    // place and simply not called, so unfreeze is a store of nullptr and no
    // state was ever lost.
    //
    // ON THE BASE, not on EngineInsertTask (moved 2026-07-30, Jeff: every tab
    // must be freezable).  Vox and Inst strips are RenderTasks, NOT
    // EngineInsertTasks, so while this lived on the derived class
    // `renderTaskForTab` could not even express them in its return type and
    // freeze was structurally shut out of both -- which is why the excuse
    // recorded next to it ("live-input chains, freezing the input would freeze
    // nothing") went unexamined for so long.  It was never about live input:
    // recorded takes on those strips replay through the strip's own chain and
    // are exactly what freeze should be substituting.
    //
    // ATOMIC because the message thread sets it (freeze / unfreeze / a
    // re-render completing) while the audio thread reads it every block.  The
    // streamer is owned by the freeze driver and outlives any block that could
    // be reading it: it is cleared here FIRST, and the owner only destroys it
    // once the dispatcher has been through a block with the pointer already null.
    void setFrozenSource (AudioClipStreamer* s) noexcept
        { mFrozenSource.store (s, std::memory_order_release); }
    AudioClipStreamer* getFrozenSource() const noexcept
        { return mFrozenSource.load (std::memory_order_acquire); }

    // ── TS7 §6.9: freeze-render pruning ───────────────────────────────────────
    // Set only by RenderGraphDispatcher::setFreezePrune, only for the duration
    // of an offline freeze render, and only on tasks OUTSIDE the keep-set (the
    // frozen target, everything upstream of it, and the master).  A freeze
    // render of one track otherwise pays for the whole project every block.
    //
    // WHY A FLAG AND NOT "DON'T SEED THE TASK":  the dispatcher's completion
    // signal is MasterTask::run setting mAllDone, and master only runs when its
    // dependency counter reaches zero -- which requires EVERY predecessor to
    // have been through the pool.  Drop a task from the seed set and master
    // starves, mAllDone never sets, and the block burns the full 100ms watchdog
    // instead of ~2ms: measured at 2.15x realtime, TWENTY TIMES SLOWER than the
    // unpruned render it was meant to speed up.  So the task must still flow
    // through the pool and still decrement its children -- it just must not do
    // any WORK.  The dependency graph is load-bearing; only run() is optional.
    void setRenderSkipped (bool s) noexcept
        { mRenderSkipped.store (s, std::memory_order_release); }
    bool isRenderSkipped() const noexcept
        { return mRenderSkipped.load (std::memory_order_acquire); }

    // Stands in for run() when skipped.  The arena buffer is recycled across
    // blocks and is never zeroed on its own, so leaving it alone would feed
    // last block's audio (or, on the first block, uninitialised memory) into
    // whatever sums it downstream.
    // Whole buffer, not just mCtx->numSamples: BlockContext is only forward
    // declared here, and the arena slot is one max-block allocation anyway.
    void clearOnSkip() noexcept
    {
        if (mOutputBuffer != nullptr)
            mOutputBuffer->clear();
    }

protected:
    std::atomic<AudioClipStreamer*> mFrozenSource { nullptr };
    std::atomic<bool>               mRenderSkipped { false };
};
