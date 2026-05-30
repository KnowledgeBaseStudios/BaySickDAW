#include "VibeThreadPool.h"
#include "RenderTask.h"
#include "RenderEngineFlags.h"

#include <thread>
#include <vector>

// moodycamel::ConcurrentQueue - vendored single-header library. Drop the
// header into libs/concurrentqueue/concurrentqueue.h before building.
// CMakeLists.txt adds that folder to the include path. See README in the
// folder for download instructions.
#include "concurrentqueue.h"

struct VibeThreadPool::Impl
{
    // Worker-eligible task queue (MPMC).  Workers tryPop this in workerLoop.
    // Audio thread also drains it via runUntilOrTimeout (main-as-worker).
    moodycamel::ConcurrentQueue<RenderTask*>           queue;

    std::vector<std::thread>                           workers;
    std::vector<std::unique_ptr<juce::WaitableEvent>>  wakers;
    std::atomic<int>                                   activeWaiters { 0 };
};

VibeThreadPool::VibeThreadPool (int requestedWorkers)
    : mImpl (std::make_unique<Impl>())
{
    mNumWorkers = juce::jlimit (1, RenderEngine::kMaxWorkers, requestedWorkers);

    mImpl->wakers.reserve ((size_t) mNumWorkers);
    mImpl->workers.reserve ((size_t) mNumWorkers);

    for (int i = 0; i < mNumWorkers; ++i)
        mImpl->wakers.emplace_back (std::make_unique<juce::WaitableEvent> (false));

    for (int i = 0; i < mNumWorkers; ++i)
        mImpl->workers.emplace_back ([this, i] { workerLoop (i); });
}

VibeThreadPool::~VibeThreadPool()
{
    mShutdown.store (true, std::memory_order_release);

    // Wake every sleeping worker so it can observe the shutdown flag.
    for (auto& waker : mImpl->wakers)
        waker->signal();

    for (auto& t : mImpl->workers)
        if (t.joinable())
            t.join();
}

void VibeThreadPool::submit (RenderTask* task) noexcept
{
    if (task == nullptr)
        return;

    // moodycamel::ConcurrentQueue::enqueue() never blocks; it allocates a
    // new block internally if the producer's local block is full. The
    // allocation path is rare (amortized) but it CAN happen - to keep this
    // path RT-safe, the queue's pre-allocated capacity is sized in the pool
    // constructor (default initial capacity is large enough for our task
    // counts).
    mImpl->queue.enqueue (task);

    // If any worker is sleeping, wake one. Signalling all is harmless (signal
    // is sticky) and cheaper than tracking which specific waker to poke. If
    // no worker is sleeping, the signal is wasted but not incorrect - the
    // next worker to enter wait() will return immediately if any signal was
    // queued in the meantime.
    if (mImpl->activeWaiters.load (std::memory_order_acquire) > 0)
        for (auto& waker : mImpl->wakers)
            waker->signal();
}

RenderTask* VibeThreadPool::tryPop() noexcept
{
    RenderTask* task = nullptr;
    if (mImpl->queue.try_dequeue (task))
        return task;
    return nullptr;
}

void VibeThreadPool::runOneTask (RenderTask* task) noexcept
{
    if (task == nullptr)
        return;

    task->run();

    // Decrement each child's dep counter. acq_rel guarantees that this
    // task's writes to mOutputBuffer are visible to the worker that picks
    // up any child whose counter just hit zero.
    for (auto* child : task->mChildren)
    {
        const int prev = child->mDeps.fetch_sub (1, std::memory_order_acq_rel);
        if (prev == 1)
        {
            if (RenderEngine::MtDiagnostic::gCaptureOn.load (std::memory_order_relaxed))
                RenderEngine::MtDiagnostic::gChildSubmits.fetch_add (1, std::memory_order_relaxed);
            submit (child);
        }
    }
}

void VibeThreadPool::runUntil (std::atomic<bool>& done) noexcept
{
    while (! done.load (std::memory_order_acquire))
    {
        if (auto* task = tryPop())
            runOneTask (task);
        else
            std::this_thread::yield();
    }
}

// 2026-05-06 (Batch 9c watchdog): bounded variant.  We always run any task
// available (greedy) -- the deadline is only checked on idle iterations
// where there's nothing to do.  This biases toward "if work is available,
// finish the block normally; only declare timeout when the system is
// actually stuck waiting."
bool VibeThreadPool::runUntilOrTimeout (std::atomic<bool>& done,
                                          double              deadlineMillisHiRes) noexcept
{
    const bool capture = RenderEngine::MtDiagnostic::gCaptureOn.load (std::memory_order_relaxed);
    while (! done.load (std::memory_order_acquire))
    {
        if (auto* task = tryPop())
        {
            if (capture)
                RenderEngine::MtDiagnostic::gMainThreadTasks.fetch_add (1, std::memory_order_relaxed);
            runOneTask (task);
        }
        else
        {
            if (juce::Time::getMillisecondCounterHiRes() >= deadlineMillisHiRes)
            {
                if (capture)
                    RenderEngine::MtDiagnostic::gWatchdogFires.fetch_add (1, std::memory_order_relaxed);
                return false;
            }
            std::this_thread::yield();
        }
    }
    return true;
}

void VibeThreadPool::clearQueues() noexcept
{
    RenderTask* drained = nullptr;
    while (mImpl->queue.try_dequeue (drained))
        ; // discard
}

void VibeThreadPool::workerLoop (int workerIndex) noexcept
{
    auto& waker = *mImpl->wakers[(size_t) workerIndex];

    while (! mShutdown.load (std::memory_order_acquire))
    {
        // QA-Ef: serial-diagnostic mode. When multi-core rendering is OFF,
        // workers park here so the audio thread (runUntilOrTimeout) runs the
        // whole graph itself -- genuinely serial through the identical
        // dispatcher/task code, no duplicate path. The existing waiter/waker
        // bookkeeping lets submit() wake parked workers when parallel resumes.
        if (! RenderEngine::gMultiThreadedEngineEnabled.load (std::memory_order_acquire))
        {
            mImpl->activeWaiters.fetch_add (1, std::memory_order_release);
            waker.wait (-1);
            mImpl->activeWaiters.fetch_sub (1, std::memory_order_release);
            continue;
        }

        const bool capture = RenderEngine::MtDiagnostic::gCaptureOn.load (std::memory_order_relaxed);

        // Spin phase - tight loop, no yield. Typical inter-task gap is
        // microseconds; we'd rather burn a few cycles than pay a context
        // switch.
        bool gotTask = false;
        for (int i = 0; i < RenderEngine::kWorkerSpinIterations; ++i)
        {
            if (auto* task = tryPop())
            {
                if (capture)
                {
                    RenderEngine::MtDiagnostic::gWorkerSpinFinds.fetch_add (1, std::memory_order_relaxed);
                    RenderEngine::MtDiagnostic::gWorkerTasks    .fetch_add (1, std::memory_order_relaxed);
                }
                runOneTask (task);
                gotTask = true;
                break;
            }
        }
        if (gotTask)
            continue;

        // Sleep phase - register as waiter, recheck the queue once for the
        // submit/spin race, then wait on the event.
        mImpl->activeWaiters.fetch_add (1, std::memory_order_release);

        if (auto* task = tryPop())
        {
            mImpl->activeWaiters.fetch_sub (1, std::memory_order_release);
            if (capture)
            {
                RenderEngine::MtDiagnostic::gWorkerSleepFinds.fetch_add (1, std::memory_order_relaxed);
                RenderEngine::MtDiagnostic::gWorkerTasks     .fetch_add (1, std::memory_order_relaxed);
            }
            runOneTask (task);
            continue;
        }

        // Wait until a submit() signals us, or shutdown wakes everyone.
        // -1 = no timeout. juce::WaitableEvent::signal is sticky, so if a
        // signal raced ahead of this wait it returns immediately.
        if (capture)
            RenderEngine::MtDiagnostic::gWorkerIdleSleeps.fetch_add (1, std::memory_order_relaxed);

        waker.wait (-1);

        if (capture)
            RenderEngine::MtDiagnostic::gWorkerWakes.fetch_add (1, std::memory_order_relaxed);

        mImpl->activeWaiters.fetch_sub (1, std::memory_order_release);
    }
}
