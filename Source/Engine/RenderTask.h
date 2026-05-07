#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

#include "UpstreamLink.h"

// Forward declarations
class RenderGraphDispatcher;
struct BlockContext;

// Base class for one node in the render graph. Typically wraps one mixer
// strip's processing — engine dispatch + insert chain + output publish for a
// leaf, or upstream sum + bus chain + publish for a passive bus.
//
// Memory ordering (CRITICAL — see VibeThreadPool.cpp for enforcement):
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
};
