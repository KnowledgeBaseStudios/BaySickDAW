#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

#include "RenderEngineFlags.h"

class RenderTask;
class VibeThreadPool;
class ChannelBufferArena;
class RoutingGraph;   // VibeGraph.h - full def included only in the .cpp
struct BlockContext;  // BlockContext.h - caller fills before dispatchBlock

// Owns the per-block render graph dispatch logic. Holds non-owning pointers
// to RenderTask instances registered by PluginProcessor as engines / strips
// are created and destroyed.
//
// Phase 1 scaffolding (this commit)
// ---------------------------------
// dispatchBlock() is a stub that clears the host-supplied output buffer.
// Per-strip wrappers ship in Batches 3-8; the full parallel pump (reset
// counters → seed leaves → runUntil → master-publishes) lands in Batch 8.
//
// The class is constructed in PluginProcessor's member init list. It takes
// references to the worker pool and arena, both of which are also value
// members declared earlier in PluginProcessor.h so initialization order is
// guaranteed.
class RenderGraphDispatcher
{
public:
    RenderGraphDispatcher (VibeThreadPool& pool, ChannelBufferArena& arena);
    ~RenderGraphDispatcher();

    RenderGraphDispatcher (const RenderGraphDispatcher&)            = delete;
    RenderGraphDispatcher& operator= (const RenderGraphDispatcher&) = delete;

    // Called from PluginProcessor::prepareToPlay. Resizes the arena to fit
    // the new block size; the worker pool itself is NOT recreated (per the
    // lifetime contract - see VibeThreadPool.h).
    void prepare (double sampleRate, int maxBlockSize);

    // Register a non-owning task pointer. Called from the message thread
    // when an engine is created / a strip is added. The dispatcher hooks
    // up the task's mOutputBuffer to the arena slot for its channelId.
    //
    // Special case: tasks with channelId == -1 (producer-style tasks that
    // drive engine state without publishing to a channel - e.g.
    // RustyDrumsProducerTask) are added to the task list without an arena
    // slot or mTasksByChannel entry.  They participate in the dependency
    // graph via addSyntheticDep().
    void registerTask (RenderTask* task);

    // Remove a task by channel id. Called when an engine is destroyed.
    void unregisterTask (int channelId);

    // Remove a task by pointer. Used for producer tasks that don't have
    // a channelId-based slot.
    void unregisterTask (RenderTask* task);

    // Synthetic dependency between tasks that aren't connected through
    // the routing graph (e.g. RustyDrumsProducerTask → 13 RustyInsertTasks
    // for the engine's internal multi-output).  rebuildLinks walks these
    // alongside routing edges, bumping mInitialDeps + mChildren.
    void addSyntheticDep (RenderTask* producer, RenderTask* consumer);

    // Drop any synthetic deps referencing `task` as either producer or
    // consumer.  Called from unregisterTask paths to keep the list clean.
    void removeSyntheticDepsFor (RenderTask* task);

    // Batch 8 (2026-05-06): expose the dispatcher's done flag so the
    // MasterTask can signal completion of a block.  MasterTask stores the
    // reference at construction time and `release`-stores `true` at the
    // end of its run().  Dispatcher's runUntil polls with `acquire` -
    // the release/acquire pair publishes Master's writes to the arena
    // slot to the main thread.
    std::atomic<bool>& getAllDoneFlag() noexcept { return mAllDone; }

    // Rebuild every registered task's mPredecessors / mChildren / mInitialDeps
    // from the RoutingGraph's current edges + sidechain edges.  Called from
    // PluginProcessor at the top of each block whenever the routing has
    // changed (cheap - just walks two vectors).  Tasks not present in the
    // dispatcher (e.g. an edge references a channel id without a registered
    // RenderTask) are silently skipped; the corresponding edge is dropped
    // from the parallel section.
    //
    // After this call, mDeps for each task still holds its previous value;
    // dispatchBlock resets it to mInitialDeps at the top of every block.
    void rebuildLinks (const RoutingGraph& routing);

    // TS7 §6.9: restrict an OFFLINE freeze render to the tasks the frozen
    // track actually needs.  `target` is the task being frozen; everything
    // upstream of it (audio predecessors, sidechain predecessors, and the
    // synthetic producers that feed it) plus the master task stay live, and
    // every other task is marked skipped -- it still flows through the pool
    // and still decrements its children, it just does no work.
    //
    // Pass nullptr to clear the pruning.  ALWAYS paired with the freeze tap's
    // arm / disarm; this must never be left set when real-time playback
    // resumes or the project falls silent except for one track.
    void setFreezePrune (RenderTask* target);

    // Run one block.  PluginProcessor's processBlock calls this once per
    // block; the dispatcher is the single render path (QA-Ef, 2026-05-21).
    // Caller MUST fully populate ctx
    // (numSamples, bpm, anySolo, posInfo, per-engine MIDI buffer pointers,
    // liveInputSnapshot) before this call - Batch 9a moved that
    // responsibility to the caller so each task sees consistent context
    // built once at the dispatch site rather than rebuilt internally.
    //
    // Pump: reset counters + assign ctx → seed leaves → runUntil(mAllDone)
    // → MasterTask sets done flag with release ordering → main thread
    // copies arena[kMaster] to outputBuffer.
    void dispatchBlock (juce::AudioBuffer<float>& outputBuffer,
                        const BlockContext&       ctx);

private:
    VibeThreadPool&     mPool;
    ChannelBufferArena& mArena;

    // Tasks indexed by channelId for O(1) lookup at register / unregister.
    // Non-owning pointers - task lifetime is managed by PluginProcessor.
    std::array<RenderTask*, RenderEngine::kMaxStripChannels> mTasksByChannel {};

    // Insertion-ordered list for iteration (reset counters, seed leaves).
    std::vector<RenderTask*> mTasks;

    // Batch 6: synthetic (non-routing) deps - pairs (producer, consumer).
    // Re-applied on every rebuildLinks call so consumer.mInitialDeps and
    // producer.mChildren stay coherent when topology changes.
    std::vector<std::pair<RenderTask*, RenderTask*>> mSyntheticDeps;

    // Set true by the master task at the end of each block; runUntil polls
    // this. Stored in dispatcher rather than RenderTask so the master task
    // type can write to it without dispatcher header coupling.
    alignas (64) std::atomic<bool> mAllDone { false };
};
