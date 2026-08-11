#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>

class RenderTask;

// Lock-free MPMC worker pool for the multi-threaded render engine.
//
// Lifetime
// --------
// Instantiated as a value member in PluginProcessor's constructor; destroyed
// when PluginProcessor is destroyed. Worker threads are spawned in the pool
// constructor and joined in the destructor. prepareToPlay does NOT recreate
// the pool - sample-rate / block-size changes only call clearQueues(). This
// avoids dropouts and host crashes that come from joining + spawning threads
// inside prepareToPlay.
//
// Wake protocol (hybrid spin/sleep)
// ---------------------------------
// 1. Worker spins on tryPop() for kWorkerSpinIterations iterations. Tight
//    loop, no yield - typical task gap is microseconds, well under the spin
//    budget.
// 2. On miss, the worker sleeps on its private juce::WaitableEvent.
// 3. Main thread / other workers calling submit() signal the wakers, which
//    is a cheap kernel-level wakeup (~1 microsecond on Windows).
//
// Memory ordering
// ---------------
// - submit() does an enqueue on the moodycamel queue (release-internal) and
//   signals wakers. Workers' tryPop() establishes acquire on the dequeue.
// - When this pool calls a task's children's mDeps.fetch_sub, it uses
//   memory_order_acq_rel so the release pairs with the worker's writes to
//   the task's output buffer, and the acquire pairs with the consuming
//   worker's reads.
//
// Block completion
// ----------------
// mOutstandingTasks counts tasks submitted for the current block that have
// not yet come out the far end of runOneTask.  A block is complete only when
// it is quiescent AND the master task's done flag has fired -- see
// blockComplete().  Waiting on the master flag alone is not sufficient: any
// task that is not upstream of master (a childless producer, a freeze tap, a
// strip routed nowhere) can still be inside run() when master signals, and
// every task holds a BlockContext pointer into the caller's stack frame.
class BaySickThreadPool
{
public:
    explicit BaySickThreadPool (int requestedWorkers);
    ~BaySickThreadPool();

    BaySickThreadPool (const BaySickThreadPool&)            = delete;
    BaySickThreadPool& operator= (const BaySickThreadPool&) = delete;

    int getWorkerCount() const noexcept { return mNumWorkers; }

    // Push a ready task into the MPMC queue. Wakes any sleeping workers.
    // RT-safe (no allocation, no locks). Audio-thread callable.
    void submit (RenderTask* task) noexcept;

    // Pop one ready task from the queue. Returns nullptr if empty.
    // RT-safe.
    RenderTask* tryPop() noexcept;

    // Main-thread-as-worker entry, and the pool's ONLY block pump.  Pops + runs
    // tasks until the block is complete (blockComplete: `done` set AND no task
    // still outstanding), keeping the audio thread productive instead of
    // spinning idle while workers process the graph.
    //
    // 2026-05-06 (Batch 9c watchdog): bounded.  Returns false instead of
    // spinning forever once juce::Time::getMillisecondCounterHiRes() reaches
    // deadlineMillisHiRes AND the queue is empty; true if the block completed
    // normally.  The deadline is only checked on idle (queue empty)
    // iterations, so a task that runs forever still hangs the call - this
    // catches deadlocks (workers all blocked waiting on each other, queue
    // empty), not runaway tasks.  RT-safe.
    bool runUntilOrTimeout (std::atomic<bool>& done,
                             double              deadlineMillisHiRes) noexcept;

    // Drain any tasks left in the queue. Safe to call from prepareToPlay.
    // Workers are NOT joined - they continue running and will idle in their
    // wait state until new tasks arrive.
    void clearQueues() noexcept;

    // Per-block busy-tick accumulator.  runOneTask (the single funnel for BOTH
    // workers and the audio thread's runUntilOrTimeout pump) adds each task's
    // wall-clock here, so the sum is total parallel render work rather than the
    // audio thread's critical-path wall-clock.  The dispatcher resets it at
    // block start.
    //
    // Zero the outstanding-task count at the block boundary.  The dispatcher
    // calls this before seeding the leaves; it is also what absorbs the count
    // left behind by a block that timed out with tasks still in flight.
    void resetOutstandingTasks() noexcept  { mOutstandingTasks.store (0, std::memory_order_relaxed); }

    // Diagnostic read only (the watchdog log).  relaxed: workers are still
    // modifying it in exactly the situation this gets read.
    int  getOutstandingTasks() const noexcept { return mOutstandingTasks.load (std::memory_order_relaxed); }

private:
    void workerLoop (int workerIndex) noexcept;
    void runOneTask (RenderTask* task) noexcept;

    // BOTH halves are load-bearing.  `done` alone fires while non-master tasks
    // may still be running (the stack-frame hazard above).  The counter alone
    // can reach zero without master ever having run -- a routing cycle
    // upstream of master starves it, and the graph goes quiescent anyway --
    // which would publish a stale arena slot instead of timing out.
    //
    // <= 0 rather than == 0: a task still in flight when a block times out
    // decrements during the NEXT block, after that block reset the counter.
    // The count then runs one low, and an equality test would turn a single
    // straggler into a block that can never complete -- whose own tasks then
    // straggle into the block after it.  Undershoot costs one block of the
    // race this counter exists to close; equality costs a watchdog cascade.
    bool blockComplete (std::atomic<bool>& done) const noexcept
    {
        return done.load (std::memory_order_acquire)
            && mOutstandingTasks.load (std::memory_order_acquire) <= 0;
    }

    struct Impl;
    std::unique_ptr<Impl> mImpl;

    int               mNumWorkers = 0;
    std::atomic<bool> mShutdown   { false };

    // Tasks submitted for this block that have not finished runOneTask.
    // Reaches zero ONLY at true graph quiescence: a task submits its
    // newly-ready children BEFORE decrementing itself, so the count already
    // covers them at the moment it drops.  Its own cache line - every worker
    // RMWs it twice per task, so the block's completion signal keeps a line to
    // itself rather than sharing with mShutdown / mNumWorkers.
    alignas (64) std::atomic<int> mOutstandingTasks { 0 };
};
