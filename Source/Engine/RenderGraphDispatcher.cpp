#include "RenderGraphDispatcher.h"
#include "RenderTask.h"
#include "BlockContext.h"   // Batch 8: per-block context built inside dispatchBlock
#include "VibeThreadPool.h"
#include "ChannelBufferArena.h"
#include "VibeGraph.h"   // RoutingGraph (Edge / ScEdge / topoOrder)

#include <algorithm>   // std::remove

RenderGraphDispatcher::RenderGraphDispatcher (VibeThreadPool& pool,
                                              ChannelBufferArena& arena)
    : mPool (pool), mArena (arena)
{
    // QA-Ef (2026-05-22): pre-size both task lists so registerTask /
    // addSyntheticDep never reallocate while the audio thread is mid-iteration.
    // dispatchBlock's leaf-seed loop and rebuildLinks both walk these every
    // block; a project load into a freshly-started app appends many tasks at
    // once, and a reallocation there would move the backing store out from
    // under the audio thread's pointer -> use-after-free crash in dispatchBlock
    // (the observed save-file-load crash signature).  channelId space is
    // 0..kMaxStripChannels-1; the +64 headroom covers producer-style tasks
    // (channelId == -1) that don't occupy a channel slot.
    mTasks.reserve (RenderEngine::kMaxStripChannels + 64);
    mSyntheticDeps.reserve (256);
}

RenderGraphDispatcher::~RenderGraphDispatcher()
{
    // Tasks are owned by PluginProcessor. We only clear our non-owning
    // bookkeeping here; the pool itself is destroyed by its own destructor
    // when PluginProcessor goes away.
    mTasks.clear();
    mTasksByChannel.fill (nullptr);
    mSyntheticDeps.clear();
}

void RenderGraphDispatcher::prepare (double sampleRate, int maxBlockSize)
{
    juce::ignoreUnused (sampleRate);
    mArena.prepare (maxBlockSize);
}

void RenderGraphDispatcher::registerTask (RenderTask* task)
{
    if (task == nullptr)
        return;

    const int chId = task->channelId;

    // Producer-style task: no arena slot, no mTasksByChannel binding.
    // Participates in the graph via synthetic deps.
    if (chId == -1)
    {
        mTasks.push_back (task);
        return;
    }

    if (chId < 0 || chId >= RenderEngine::kMaxStripChannels)
    {
        jassertfalse; // channel id out of range
        return;
    }

    // If something was already registered at this id, replace it. The caller
    // is responsible for the lifetime of the previous task.
    //
    // QA-0 (2026-05-07): for normal paths this branch is now unreachable -
    // the per-row Composite at audioInsert(N) is registered exactly once
    // by ensureAudioInsert; registerClipEngine sets a pointer on the
    // existing Composite rather than registering a separate task.  jassert
    // surfaces any future regression where a new task type re-introduces
    // a multi-source channel without going through a composite shape;
    // the silent fallback below stays as a safety net for release builds.
    if (mTasksByChannel[(size_t) chId] != nullptr)
    {
        jassertfalse;
        unregisterTask (chId);
    }

    task->mOutputBuffer = mArena.getStripBuffer (chId);
    mTasksByChannel[(size_t) chId] = task;
    mTasks.push_back (task);
}

void RenderGraphDispatcher::unregisterTask (int channelId)
{
    if (channelId < 0 || channelId >= RenderEngine::kMaxStripChannels)
        return;

    RenderTask* victim = mTasksByChannel[(size_t) channelId];
    if (victim == nullptr)
        return;

    mTasksByChannel[(size_t) channelId] = nullptr;
    mTasks.erase (std::remove (mTasks.begin(), mTasks.end(), victim),
                  mTasks.end());
    removeSyntheticDepsFor (victim);
}

void RenderGraphDispatcher::unregisterTask (RenderTask* task)
{
    if (task == nullptr) return;

    // Clear mTasksByChannel slot too in case this is a regular task.
    if (task->channelId >= 0
        && task->channelId < RenderEngine::kMaxStripChannels)
    {
        if (mTasksByChannel[(size_t) task->channelId] == task)
            mTasksByChannel[(size_t) task->channelId] = nullptr;
    }

    mTasks.erase (std::remove (mTasks.begin(), mTasks.end(), task),
                  mTasks.end());
    removeSyntheticDepsFor (task);
}

void RenderGraphDispatcher::addSyntheticDep (RenderTask* producer,
                                              RenderTask* consumer)
{
    if (producer == nullptr || consumer == nullptr) return;
    mSyntheticDeps.emplace_back (producer, consumer);
}

void RenderGraphDispatcher::removeSyntheticDepsFor (RenderTask* task)
{
    if (task == nullptr) return;
    mSyntheticDeps.erase (
        std::remove_if (mSyntheticDeps.begin(), mSyntheticDeps.end(),
                        [task] (const auto& p)
                        { return p.first == task || p.second == task; }),
        mSyntheticDeps.end());
}

void RenderGraphDispatcher::rebuildLinks (const RoutingGraph& routing)
{
    // Wipe the previous frame's links so we don't accumulate stale edges
    // when topology shrinks (e.g. user deletes a send cable).
    for (auto* t : mTasks)
    {
        if (t == nullptr) continue;
        t->mPredecessors.clear();
        t->mChildren.clear();
        t->mInitialDeps = 0;
    }

    auto lookup = [this] (int chId) -> RenderTask*
    {
        if (chId < 0 || chId >= RenderEngine::kMaxStripChannels)
            return nullptr;
        return mTasksByChannel[(size_t) chId];
    };

    // Audio edges (main-out + sends).
    for (const auto& e : routing.edges())
    {
        RenderTask* src = lookup (e.srcId);
        RenderTask* dst = lookup (e.dstId);
        if (src == nullptr || dst == nullptr)
            continue;   // edge references a strip without a registered task

        UpstreamLink link;
        link.source    = src;
        link.gainDb    = e.amountDb;
        link.prePost   = e.prePost;
        link.isMainOut = e.isMainOut;
        link.isSc      = false;
        link.scSlot    = 0;
        dst->mPredecessors.push_back (link);

        src->mChildren.push_back (dst);
        ++dst->mInitialDeps;
    }

    // Sidechain edges (post-everything tap from src into dst's SC slot).
    for (const auto& sc : routing.scEdges())
    {
        RenderTask* src = lookup (sc.srcId);
        RenderTask* dst = lookup (sc.dstId);
        if (src == nullptr || dst == nullptr)
            continue;

        UpstreamLink link;
        link.source    = src;
        link.gainDb    = 0.0f;
        link.prePost   = false;
        link.isMainOut = false;
        link.isSc      = true;
        link.scSlot    = sc.dstSlot;
        dst->mPredecessors.push_back (link);

        src->mChildren.push_back (dst);
        ++dst->mInitialDeps;
    }

    // Batch 6: synthetic deps (e.g. RustyDrumsProducer → 13 RustyInsert).
    // These are NOT audio-source predecessors (consumer doesn't read producer's
    // mOutputBuffer); they're pure finishing-order constraints, so we bump
    // mInitialDeps + mChildren without adding to mPredecessors.
    for (const auto& [producer, consumer] : mSyntheticDeps)
    {
        if (producer == nullptr || consumer == nullptr) continue;
        producer->mChildren.push_back (consumer);
        ++consumer->mInitialDeps;
    }

    // Sanity: synthetic deps add to mInitialDeps without a matching
    // mPredecessors entry, so the strict equality from Batch 2 relaxes
    // to mInitialDeps >= mPredecessors.size().  Less-than would mean a
    // dep was added to mPredecessors but didn't bump mInitialDeps,
    // which deadlocks the dispatcher.
    for (auto* t : mTasks)
    {
        juce::ignoreUnused (t);
        jassert (t == nullptr || t->mInitialDeps >= (int) t->mPredecessors.size());
    }
}

void RenderGraphDispatcher::setFreezePrune (RenderTask* target)
{
    if (target == nullptr)
    {
        for (auto* t : mTasks)
            if (t != nullptr)
                t->setRenderSkipped (false);
        return;
    }

    // MASTER IS NON-NEGOTIABLE.  MasterTask::run is what sets mAllDone, which
    // is the ONLY thing that ends the block -- skip it and every block waits
    // out the full watchdog.
    RenderTask* master = mTasksByChannel[(size_t) MixerChannelIds::kMaster];
    jassert (master != nullptr);

    std::vector<RenderTask*> keep;
    keep.reserve (mTasks.size());

    auto contains = [&keep] (RenderTask* t)
    { return std::find (keep.begin(), keep.end(), t) != keep.end(); };

    auto add = [&keep, &contains] (RenderTask* t)
    { if (t != nullptr && ! contains (t)) { keep.push_back (t); return true; } return false; };

    add (master);
    add (target);

    // Reverse-walk the audio predecessors.  `keep` grows while we iterate it,
    // which is deliberate: the index walk IS the breadth-first closure.  Both
    // normal and sidechain links live in mPredecessors, so one pass covers a
    // frozen track whose compressor is keyed off another strip -- prune that
    // key source and the freeze bakes in the wrong gain reduction.
    for (size_t i = 0; i < keep.size(); ++i)
        for (const auto& link : keep[i]->mPredecessors)
            add (link.source);

    // Synthetic deps are NOT in mPredecessors (no buffer is read across them),
    // so a producer-style task reaches the keep-set only here.  This is the
    // only route by which RustyDrumsProducerTask -- which drives the engine
    // that every RustyInsertTask reads out of -- survives the prune; without
    // it, freezing a Rusty drum renders silence.  Iterate to a fixpoint: a
    // producer can itself have predecessors, and adding it can in turn pull in
    // another producer.
    for (bool grew = true; grew; )
    {
        grew = false;
        for (const auto& [producer, consumer] : mSyntheticDeps)
            if (producer != nullptr && consumer != nullptr && contains (consumer))
                grew |= add (producer);

        for (size_t i = 0; i < keep.size(); ++i)
            for (const auto& link : keep[i]->mPredecessors)
                grew |= add (link.source);
    }

    for (auto* t : mTasks)
        if (t != nullptr)
            t->setRenderSkipped (! contains (t));
}

void RenderGraphDispatcher::dispatchBlock (juce::AudioBuffer<float>& outputBuffer,
                                            const BlockContext&       ctx)
{
    const int n = ctx.numSamples;
    if (n <= 0 || mTasks.empty())
    {
        outputBuffer.clear();
        return;
    }

    if (RenderEngine::MtDiagnostic::gCaptureOn.load (std::memory_order_relaxed))
        RenderEngine::MtDiagnostic::gBlockCount.fetch_add (1, std::memory_order_relaxed);

    // QA-N (DIAG-02): reset the pool's per-block busy accumulator at the block
    // boundary.  runOneTask (workers + audio-thread pump) adds to it below;
    // measureDspLoadAndOverload reads the sum after this dispatch returns.
    mPool.resetBusyTicks();

    // ── Reset dep counters + assign ctx for every task ──────────────────────
    // Release ordering on the counter store pairs with the worker's acquire
    // when picking up a child whose deps just hit zero - guarantees the
    // worker sees the fresh counter value.
    for (auto* t : mTasks)
    {
        if (t == nullptr) continue;
        t->mDeps.store (t->mInitialDeps, std::memory_order_release);
        t->mCtx = &ctx;
    }

    mAllDone.store (false, std::memory_order_release);

    // ── Seed leaves ─────────────────────────────────────────────────────────
    // Tasks with mInitialDeps == 0 are ready immediately.  In a typical
    // graph these are the engine inserts (no upstream predecessors).
    {
        const bool capture = RenderEngine::MtDiagnostic::gCaptureOn.load (std::memory_order_relaxed);
        for (auto* t : mTasks)
            if (t != nullptr && t->mInitialDeps == 0)
            {
                if (capture)
                    RenderEngine::MtDiagnostic::gLeavesSubmitted.fetch_add (1, std::memory_order_relaxed);
                mPool.submit (t);
            }
    }

    // ── Main thread participates as worker until master signals done ────────
    // This is the parallel pump.  Workers process tasks from the pool's
    // queue; main thread joins as a worker (no dedicated spin-wait).  Loop
    // exits when MasterTask sets mAllDone via release; main thread's
    // acquire on mAllDone publishes Master's writes to the arena slot.
    //
    // 2026-05-06 (Batch 9c watchdog): bounded by kWatchdogTimeoutMillis so a
    // missed routing-graph cycle / stuck worker / unfired Master flag
    // doesn't hang the audio thread forever.  On timeout we log the
    // incomplete task list and clear the output buffer for this block --
    // audio glitches once instead of locking up the whole DAW.
    const double tStart   = juce::Time::getMillisecondCounterHiRes();
    const double deadline = tStart + RenderEngine::kWatchdogTimeoutMillis;
    const bool   completed = mPool.runUntilOrTimeout (mAllDone, deadline);

    if (! completed)
    {
        // Build a one-line-per-incomplete-task diagnostic.  Logging from the
        // audio thread is exceptional here -- we already lost real-time
        // guarantees by timing out, so a synchronous Logger::writeToLog is
        // acceptable.  Heap allocations inside juce::String are also OK
        // for the same reason.  Read mDeps with relaxed: workers may still
        // be modifying it (race), but a stale value is fine for diagnostics.
        int incomplete = 0;
        for (auto* t : mTasks)
            if (t != nullptr && t->mDeps.load (std::memory_order_relaxed) > 0)
                ++incomplete;

        const double elapsed = juce::Time::getMillisecondCounterHiRes() - tStart;
        juce::String msg;
        msg << "[RenderGraphDispatcher] BLOCK TIMEOUT after "
            << juce::String (elapsed, 2) << "ms -- "
            << incomplete << "/" << (int) mTasks.size()
            << " tasks incomplete, mAllDone="
            << (mAllDone.load (std::memory_order_relaxed) ? "true" : "false")
            << "\n";
        for (auto* t : mTasks)
        {
            if (t == nullptr) continue;
            const int d = t->mDeps.load (std::memory_order_relaxed);
            if (d <= 0) continue;
            msg << "  ch=" << t->channelId
                << " deps=" << d << "/" << t->mInitialDeps
                << " preds=" << (int) t->mPredecessors.size()
                << " children=" << (int) t->mChildren.size()
                << "\n";
        }
        juce::Logger::writeToLog (msg);

        // Clear the host buffer so we emit silence for this block instead
        // of whatever stale arena data might be there.  Any in-flight tasks
        // continue running and will decrement child deps as they finish --
        // those decrements race against the next block's mDeps reset,
        // potentially desyncing the graph for one or two blocks before it
        // settles.  Acceptable: the alternative is a permanent hang.
        outputBuffer.clear();
        return;
    }

    // ── Copy master arena slot to host output buffer ────────────────────────
    if (auto* masterBuf = mArena.getStripBuffer (MixerChannelIds::kMaster))
    {
        const int nc = juce::jmin (masterBuf->getNumChannels(),
                                   outputBuffer.getNumChannels());
        for (int c = 0; c < nc; ++c)
            outputBuffer.copyFrom (c, 0, *masterBuf, c, 0, n);
        // Zero any remaining output channels (host may request more than 2).
        for (int c = nc; c < outputBuffer.getNumChannels(); ++c)
            outputBuffer.clear (c, 0, n);
    }
    else
    {
        outputBuffer.clear();
    }
}
