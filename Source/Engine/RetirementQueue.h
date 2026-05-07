#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// RetirementQueue<T> — generation-based deferred-destruction queue.
// (Batch 9c B1, 2026-05-06)
// ─────────────────────────────────────────────────────────────────────────────
//
// PROBLEM
//   The audio thread reads a "current snapshot" pointer (RCU-style: load the
//   pointer once at the top of processBlock, use it for the whole block).
//   The message thread can swap a new snapshot in mid-flight.  We need the
//   OLD snapshot to stay alive until the audio thread is demonstrably done
//   with it -- but we DON'T want the audio thread to do the destruction
//   (file-handle close, background-thread join, big-buffer free are all
//   slow operations that cannot land in real-time code).
//
// PATTERN
//   * Producer (message-thread mutator) calls retire(obj, retiredBeforeGen).
//     `retiredBeforeGen` is the generation of the SUCCESSOR snapshot that
//     took `obj`'s place -- NOT obj's own generation.
//   * Consumer (audio thread) calls setInUseGeneration(gen) at the top of
//     every processBlock, where `gen` is the generation of the snapshot it
//     just loaded.
//   * A dedicated background drainer thread (owned by this queue) wakes on
//     each retire() (condvar notify) or every 50 ms (timeout fallback so it
//     still drains when audio is paused / device closed and the consumer
//     isn't ticking the in-use gen) and pops every entry whose
//     retiredBeforeGen <= last-published in-use gen.  Those entries are
//     destroyed off the audio thread, off the producer, off the lock.
//
// WHY NOT std::shared_ptr<T>?
//   shared_ptr keeps an object alive via atomic refcount; whichever thread
//   drops the last reference runs the destructor.  In the audio-thread-
//   reads-snapshot scenario, the audio thread can easily be the last to
//   release a refcount and then the slow destructor lands on the audio
//   thread anyway -- defeating the point.  RetirementQueue routes ALL
//   destruction to the drainer by design, with no per-object atomic
//   refcounting overhead on the audio thread.
//
// THREADING CONTRACT
//   * Producer side (retire()): single-threaded.  Multiple producers would
//     work given external ordering of retiredBeforeGen, but BaySickDAW only
//     ever has one (the message-thread mutator).
//   * Consumer side (setInUseGeneration()): single-threaded, lock-free,
//     called from the audio thread.  release-store paired with the
//     drainer's acquire-load establishes the happens-before relationship
//     for any reads the audio thread did against the snapshot before
//     publishing the gen.
//   * Multiple consumers would need each to publish its own atomic gen
//     and the drainer to take the MIN -- not implemented; not needed.
//   * Producer and consumer are independent and may interleave freely.
//   * On destruction, the drainer is joined and ALL remaining entries are
//     destroyed unconditionally (no further consumer ack expected).
//
// GENERATION CONTRACT
//   * retiredBeforeGen MUST be the gen of the SUCCESSOR snapshot that took
//     `obj`'s place.  Example: snapshot S_A has gen 5.  Mutator builds S_B
//     with gen 6, atomically swaps it in, then retire(S_A, 6).  The drainer
//     destroys S_A once inUseGen >= 6 (consumer has demonstrably loaded
//     S_B or later).
//   * Producers SHOULD call retire() with monotonically non-decreasing
//     retiredBeforeGen values -- the drainer is FIFO and assumes the front
//     has the smallest retiredBeforeGen.  Out-of-order pushes work but may
//     stall later entries until the audio gen catches up to the largest.
//   * setInUseGeneration() may be called with any value; only the most
//     recent matters.
//
// USAGE
//   struct AudioClipSnapshot { ...; uint64_t generation; };
//   RetirementQueue<AudioClipSnapshot> mClipRetirement;
//
//   // Mutator (message thread):
//   auto newSnap = std::make_unique<AudioClipSnapshot>(...);
//   newSnap->generation = mNextGen.fetch_add(1, std::memory_order_relaxed) + 1;
//   auto* oldRaw = mActiveSnap.exchange (newSnap.release(),
//                                        std::memory_order_acq_rel);
//   if (oldRaw)
//       mClipRetirement.retire (std::unique_ptr<AudioClipSnapshot>(oldRaw),
//                                /*retiredBeforeGen=*/ newSnap->generation);
//
//   // Audio thread (top of processBlock):
//   auto* snap = mActiveSnap.load (std::memory_order_acquire);
//   mClipRetirement.setInUseGeneration (snap->generation);
//   // ... use snap->players for the rest of the block ...
// ─────────────────────────────────────────────────────────────────────────────

template <typename T>
class RetirementQueue
{
public:
    RetirementQueue()
    {
        mDrainer = std::thread ([this]
        {
            juce::Thread::setCurrentThreadName ("RetirementQueue");
            drainerLoop();
        });
    }

    ~RetirementQueue()
    {
        {
            std::lock_guard<std::mutex> lk (mMutex);
            mStopping = true;
        }
        mCv.notify_all();
        if (mDrainer.joinable())
            mDrainer.join();

        // Drainer is joined; producer side is no longer using us.
        // Whatever's still pending gets destroyed here unconditionally --
        // by this point there's no consumer to ack the in-use gen and no
        // producer to push more entries.  Destructors run on whichever
        // thread destroyed the queue (typically the message thread); if
        // that's a problem the owner should drain explicitly first.
        std::lock_guard<std::mutex> lk (mMutex);
        mEntries.clear();
    }

    RetirementQueue (const RetirementQueue&)            = delete;
    RetirementQueue& operator= (const RetirementQueue&) = delete;
    RetirementQueue (RetirementQueue&&)                 = delete;
    RetirementQueue& operator= (RetirementQueue&&)      = delete;

    // Producer side (single thread; message thread in our case).  Hands
    // ownership of `obj` to the queue, stamped with the gen at which the
    // successor took over.  Wakes the drainer (notify_one is enough --
    // single drainer thread).
    void retire (std::unique_ptr<T> obj, std::uint64_t retiredBeforeGen)
    {
        if (! obj) return;
        {
            std::lock_guard<std::mutex> lk (mMutex);
            mEntries.push_back ({ std::move (obj), retiredBeforeGen });
        }
        mCv.notify_one();
    }

    // Consumer side (audio thread, top of every processBlock).  Lock-free.
    // release-store so the drainer's acquire-load establishes happens-
    // before with whatever reads the consumer did against the snapshot
    // before publishing this gen.
    void setInUseGeneration (std::uint64_t gen) noexcept
    {
        mInUseGen.store (gen, std::memory_order_release);
    }

    // For tests / introspection.  Takes the mutex briefly; do not call
    // from the audio thread.
    std::size_t pendingCount() const
    {
        std::lock_guard<std::mutex> lk (mMutex);
        return mEntries.size();
    }

private:
    struct Entry
    {
        std::unique_ptr<T> obj;
        std::uint64_t      retiredBeforeGen;
    };

    void drainerLoop()
    {
        std::unique_lock<std::mutex> lk (mMutex);
        while (! mStopping)
        {
            // Wake on push (notify_one), or 50 ms timeout so we still make
            // progress when audio is paused / device closed and the audio
            // thread isn't ticking mInUseGen.  Spurious wakeups + the
            // empty-queue case are handled by the predicate.
            mCv.wait_for (lk, std::chrono::milliseconds (50),
                          [this] { return mStopping
                                          || ! mEntries.empty(); });

            if (mStopping) break;

            const std::uint64_t inUse =
                mInUseGen.load (std::memory_order_acquire);

            // Pop a batch of ready entries off the front.  Producer is
            // single-threaded and increments retiredBeforeGen monotonically,
            // so a single forward sweep suffices -- once we hit an entry
            // whose retiredBeforeGen > inUse, every later entry is also
            // not ready (FIFO + monotonic gen).
            std::vector<std::unique_ptr<T>> toDestroy;
            while (! mEntries.empty()
                   && mEntries.front().retiredBeforeGen <= inUse)
            {
                toDestroy.push_back (std::move (mEntries.front().obj));
                mEntries.pop_front();
            }

            // Drop the lock during destruction -- destructors may be slow
            // (file close, thread joins, large buffer free) and we don't
            // want producers blocking on retire() while we tear down.
            lk.unlock();
            toDestroy.clear();
            lk.lock();
        }
    }

    mutable std::mutex         mMutex;
    std::condition_variable    mCv;
    std::deque<Entry>          mEntries;
    std::atomic<std::uint64_t> mInUseGen { 0 };
    bool                       mStopping { false };  // guarded by mMutex
    std::thread                mDrainer;
};
