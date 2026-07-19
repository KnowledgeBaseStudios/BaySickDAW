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
class VibeThreadPool
{
public:
    explicit VibeThreadPool (int requestedWorkers);
    ~VibeThreadPool();

    VibeThreadPool (const VibeThreadPool&)            = delete;
    VibeThreadPool& operator= (const VibeThreadPool&) = delete;

    int getWorkerCount() const noexcept { return mNumWorkers; }

    // Push a ready task into the MPMC queue. Wakes any sleeping workers.
    // RT-safe (no allocation, no locks). Audio-thread callable.
    void submit (RenderTask* task) noexcept;

    // Pop one ready task from the queue. Returns nullptr if empty.
    // RT-safe.
    RenderTask* tryPop() noexcept;

    // Main-thread-as-worker entry. Pops + runs tasks until `done` is set.
    // The dispatcher uses this to keep the audio thread productive instead
    // of spinning idle while workers process the graph. RT-safe.
    void runUntil (std::atomic<bool>& done) noexcept;

    // 2026-05-06 (Batch 9c watchdog): bounded variant of runUntil.  Same as
    // runUntil except it returns false (instead of spinning forever) once
    // juce::Time::getMillisecondCounterHiRes() reaches deadlineMillisHiRes
    // AND the queue is empty.  Returns true if `done` fired normally.
    // The deadline is only checked on idle (queue empty) iterations, so a
    // task that runs forever still hangs the call - this catches deadlocks
    // (workers all blocked waiting on each other, queue empty), not runaway
    // tasks.  RT-safe.
    bool runUntilOrTimeout (std::atomic<bool>& done,
                             double              deadlineMillisHiRes) noexcept;

    // Drain any tasks left in the queue. Safe to call from prepareToPlay.
    // Workers are NOT joined - they continue running and will idle in their
    // wait state until new tasks arrive.
    void clearQueues() noexcept;

    // QA-N (DIAG-02): per-block busy-tick accumulator.  runOneTask (the single
    // funnel for BOTH workers and the audio thread's runUntilOrTimeout pump)
    // adds each task's wall-clock here, so the sum is total parallel render
    // work rather than the audio thread's critical-path wall-clock.  The
    // dispatcher resets it at block start; the DSP meter reads the sum on the
    // MT path.  relaxed everywhere: diagnostic measurement, no algorithmic
    // dependency on the value.
    void        resetBusyTicks() noexcept       { mBusyTicks.store (0, std::memory_order_relaxed); }
    juce::int64 getBusyTicks()   const noexcept { return mBusyTicks.load (std::memory_order_relaxed); }

private:
    void workerLoop (int workerIndex) noexcept;
    void runOneTask (RenderTask* task) noexcept;

    struct Impl;
    std::unique_ptr<Impl> mImpl;

    int               mNumWorkers = 0;
    std::atomic<bool> mShutdown   { false };

    // Cache-line isolated (workers fetch_add it concurrently under MT) so the
    // contended accumulator never false-shares with mShutdown / mNumWorkers.
    alignas (64) std::atomic<juce::int64> mBusyTicks { 0 };
};
