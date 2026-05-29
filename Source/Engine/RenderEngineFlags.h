#pragma once

#include <atomic>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>

// Render engine flags + tunables.
//
// 2026-05-07 (Batch 10): the master flag was promoted from `constexpr bool`
// to `std::atomic<bool>`.
//
// QA-Ef (2026-05-21): the serial render path was deleted and the dispatcher
// became the single, unconditional render path.  The flag now gates the
// WORKER THREADS, not the audio thread: workers acquire-load it at the top
// of VibeThreadPool::workerLoop and either run the graph in parallel (true)
// or park immediately (false) so the audio thread drains the entire graph
// itself via runUntilOrTimeout -- genuine serial execution through the
// IDENTICAL dispatcher / task code, no duplicate path.  Flipping at runtime
// is still a hot-swap: toggle from the message thread (release-store from
// the Mixer "Multi-core Rendering" menu) and the very next audio block
// picks the new mode (parallel vs single-core diagnostic) with no glitches
// and no restart.
namespace RenderEngine
{
    // Master switch for render-engine parallelism.  Read by the worker threads
    // (acquire) in VibeThreadPool::workerLoop; written by the message thread
    // (release) from the Mixer "Multi-core Rendering" toggle.
    //
    //   true   -> worker threads run the render graph in parallel (production
    //             default).
    //   false  -> SERIAL-DIAGNOSTIC mode: all workers park and the audio thread
    //             runs the entire graph itself via the dispatcher's
    //             runUntilOrTimeout.  Identical dispatcher + task code, zero
    //             parallelism -- the bisect tool for "parallelism bug vs logic
    //             bug".  NOT a separate serial code path (QA-Ef deleted that
    //             2026-05-21); the dispatcher is always the single render path.
    //
    // Default true: MT validated as the production-quality path during
    // Batch 9c (flag flip + watchdog + meter drain + SC pull + bus solo
    // fix).  Pre-flip blockers (B2 GUI deadlock, N1 BaySickVocal shutdown
    // gate, ~StandaloneEditor teardown, B1 deferred-destruction GC) all
    // cleared in 3b2c85a / fdbe9e1 / 47ba7a2.  Watchdog
    // (kWatchdogTimeoutMillis below) catches any remaining deadlocks
    // loudly instead of letting the audio thread hang.
    //
    // Worker threads read:   .load (std::memory_order_acquire)
    // Message thread writes:  .store (newValue, std::memory_order_release)
    //
    // The acquire/release pair publishes the message thread's preceding state
    // edits before the flip so the workers see a consistent view, and matches
    // the rest of the engine's atomic conventions.
    inline std::atomic<bool> gMultiThreadedEngineEnabled { true };

    // Hard cap on worker threads. The 5950X (16C/32T) saturates well below
    // this for the ~17-task per-block load of a typical heavy session;
    // additional workers thrash cache without throughput gain.
    inline constexpr int kMaxWorkers = 8;

    // Iterations a worker spins on try_dequeue before falling back to its
    // private juce::WaitableEvent. Tuned for ~50 microseconds at 5 GHz -
    // well under typical inter-block idle windows.
    inline constexpr int kWorkerSpinIterations = 256;

    // 2026-05-06 (Batch 9c watchdog): block-timeout deadline for
    // RenderGraphDispatcher::dispatchBlock.  The audio thread bails after
    // this many milliseconds of waiting on mAllDone instead of hanging
    // forever -- catches missed routing-graph cycles, stuck workers, etc.
    // 100 ms is much longer than any healthy block (worst case ~25 ms at
    // 1024 samples / 44.1 kHz with a heavy session) so false positives
    // are very unlikely.  When the watchdog fires it logs the incomplete
    // task list + clears the output buffer for that block.
    inline constexpr double kWatchdogTimeoutMillis = 100.0;

    // Channel id space matches MixerChannelIds (0..999). The ChannelBufferArena
    // pre-allocates one stereo slot per id; most slots are unused at any given
    // moment but the cost is bounded (~8 MB worst case at 1024-sample blocks).
    inline constexpr int kMaxStripChannels = 1000;

    // 2026-05-08 (QA-Md): diagnostic counters for the MT-no-op-in-Debug
    // investigation.  All counters are gated on gCaptureOn so the hot path
    // pays only one relaxed-load per increment site when capture is off.
    // Use std::atomic with relaxed ordering -- these are diagnostic only,
    // no algorithmic dependency.
    //
    // Lifecycle: StandaloneEditor's "Run MT Diagnostic" menu item
    // calls reset() -> sets gCaptureOn=true -> sleeps 2 s -> sets
    // gCaptureOn=false -> snapshot() -> displays AlertWindow.
    namespace MtDiagnostic
    {
        inline std::atomic<bool>      gCaptureOn        { false };

        inline std::atomic<long long> gBlockCount       { 0 };  // dispatchBlock invocations
        inline std::atomic<long long> gLeavesSubmitted  { 0 };  // pool.submit calls from dispatcher leaf seeding
        inline std::atomic<long long> gChildSubmits     { 0 };  // pool.submit calls from runOneTask child cascade
        inline std::atomic<long long> gWatchdogFires    { 0 };  // runUntilOrTimeout returned false (block timed out)

        inline std::atomic<long long> gMainThreadTasks  { 0 };  // tasks runOneTask'd from runUntilOrTimeout
        inline std::atomic<long long> gWorkerTasks      { 0 };  // tasks runOneTask'd from workerLoop (any worker)
        inline std::atomic<long long> gWorkerSpinFinds  { 0 };  // workerLoop spin-phase tryPop hits
        inline std::atomic<long long> gWorkerSleepFinds { 0 };  // workerLoop sleep-phase tryPop hits (race resolution)
        inline std::atomic<long long> gWorkerIdleSleeps { 0 };  // workerLoop entered waker.wait
        inline std::atomic<long long> gWorkerWakes      { 0 };  // workerLoop returned from waker.wait

        struct Snapshot
        {
            long long blockCount;
            long long leavesSubmitted;
            long long childSubmits;
            long long watchdogFires;
            long long mainThreadTasks;
            long long workerTasks;
            long long workerSpinFinds;
            long long workerSleepFinds;
            long long workerIdleSleeps;
            long long workerWakes;
        };

        inline void reset()
        {
            gBlockCount      .store (0);
            gLeavesSubmitted .store (0);
            gChildSubmits    .store (0);
            gWatchdogFires   .store (0);
            gMainThreadTasks .store (0);
            gWorkerTasks     .store (0);
            gWorkerSpinFinds .store (0);
            gWorkerSleepFinds.store (0);
            gWorkerIdleSleeps.store (0);
            gWorkerWakes     .store (0);
        }

        inline Snapshot snapshot()
        {
            return {
                gBlockCount      .load(),
                gLeavesSubmitted .load(),
                gChildSubmits    .load(),
                gWatchdogFires   .load(),
                gMainThreadTasks .load(),
                gWorkerTasks     .load(),
                gWorkerSpinFinds .load(),
                gWorkerSleepFinds.load(),
                gWorkerIdleSleeps.load(),
                gWorkerWakes     .load()
            };
        }

        // QA-DispatcherAffinity (2026-05-28): per-task entry+exit timestamp
        // trace.  When gTraceTaskTimestamps is true, instrumented task::run()
        // bodies (the 3 sfizz-driven engine task families:
        // RustyDrumsProducerTask + RustyInsertTask + sfizz-engine
        // InstStripTask) record entry + exit nanosecond timestamps + thread
        // ID hash + engine-instance pointer + per-block index into the
        // lock-free ring gTraceRing.  Audio-thread-safe -- only an atomic
        // fetch_add + struct-by-value write per event; no allocation, no
        // lock, no OS calls.  Drained from the message thread (Mixer
        // hamburger "QA-DispatcherAffinity Trace" toggle off) into
        // Documents/BaySickDAW/qa-dispatcheraffinity-trace.log via the
        // dump code inlined in StandaloneEditor's menu handler.  Stripped
        // at QA-DispatcherAffinity Task 4 close (if Task 3 cure-verify
        // passes; else batch close).  The dual entry+exit timestamps are
        // the load-bearing improvement over QA-Sfizz Task 4's Sub-F=(e)
        // entry-only trace, which couldn't discriminate concurrent vs
        // sequential overlap and cost two failed empirical fix cycles
        // (Sub-G + Sub-I).

        struct TraceEvent
        {
            std::uint64_t entryNs       { 0 };
            std::uint64_t exitNs        { 0 };
            int           channelId     { -1 };
            const void*   engineInstance { nullptr };
            std::uint32_t threadIdHash  { 0 };
            std::uint32_t blockIndex    { 0 };
        };

        // Power of 2 (2^16 = 65536).  At ~14 sfizz tasks per block and ~170
        // blocks/sec (1024 samples @ 44.1 kHz wall time) the ring covers
        // ~27 seconds of trace history before wrapping -- comfortably above
        // the ~6-second 6-cymbal crash MT-on reproducer.  Sized at compile
        // time as std::array -- ~2.5 MB BSS allocation; zero runtime alloc.
        static constexpr std::size_t kTraceRingCapacity = 65536;
        static_assert ((kTraceRingCapacity & (kTraceRingCapacity - 1)) == 0,
                       "kTraceRingCapacity must be a power of 2 for mask-based slot indexing");

        inline std::atomic<bool>          gTraceTaskTimestamps { false };
        inline std::atomic<std::uint64_t> gTraceWriteIndex     { 0 };
        inline std::atomic<std::uint32_t> gBlockIndex          { 0 };
        inline std::array<TraceEvent, kTraceRingCapacity> gTraceRing {};

        // Steady-clock nanoseconds since epoch.  Used as the trace
        // timestamp source -- monotonic, no NTP adjustment, sub-microsecond
        // precision on Windows.  Not wall-clock time; only deltas matter
        // for race-window analysis.
        inline std::uint64_t traceNowNs() noexcept
        {
            return (std::uint64_t) std::chrono::duration_cast<std::chrono::nanoseconds> (
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }

        // Lower 32 bits of std::hash<std::thread::id>{}(current).  Sufficient
        // to discriminate the ~9 entities that can run a task (8 workers +
        // audio thread) without collisions; full 64-bit hash would inflate
        // the trace event size.
        inline std::uint32_t traceThreadHash() noexcept
        {
            return (std::uint32_t) (std::hash<std::thread::id> {} (std::this_thread::get_id()) & 0xFFFFFFFFu);
        }

        inline void recordTraceEvent (int           channelId,
                                       const void*   engineInst,
                                       std::uint64_t entryNs,
                                       std::uint64_t exitNs,
                                       std::uint32_t blockIndex) noexcept
        {
            const std::uint64_t idx = gTraceWriteIndex.fetch_add (1, std::memory_order_relaxed);
            const std::size_t   slot = (std::size_t) (idx & (kTraceRingCapacity - 1));
            TraceEvent& ev = gTraceRing[slot];
            ev.entryNs        = entryNs;
            ev.exitNs         = exitNs;
            ev.channelId      = channelId;
            ev.engineInstance = engineInst;
            ev.threadIdHash   = traceThreadHash();
            ev.blockIndex     = blockIndex;
        }

        // Reset the trace ring.  Called by the Mixer menu handler before
        // flipping gTraceTaskTimestamps to true -- gives a fresh capture
        // window with a known starting index.  Doesn't zero the ring slots
        // themselves (fresh writes overwrite); the wrap-vs-no-wrap branch
        // in the dump path handles uninitialized tail slots after a fresh
        // reset.
        inline void resetTrace() noexcept
        {
            gTraceWriteIndex.store (0, std::memory_order_relaxed);
            gBlockIndex     .store (0, std::memory_order_relaxed);
        }

        // RAII helper for task::run() entry/exit timestamp capture.
        // Construct at the top of run() with the task's channelId + engine
        // instance pointer; destructor emits the trace event with the
        // elapsed exit timestamp + current block index.  When
        // gTraceTaskTimestamps is false, construction is trivial (one
        // relaxed atomic load + 3 stack assignments) and the destructor is
        // a single branch-and-return.  Hot path overhead is negligible.
        // Pass shouldTrace=false to opt a specific run() invocation out
        // (e.g. InstStripTask with a non-sfizz engine -- only sfizz
        // instances are in scope for the Candidate B investigation).
        struct TraceScope
        {
            bool          trace;
            int           channelId;
            const void*   engineInst;
            std::uint64_t entryNs;

            TraceScope (int ch, const void* eng, bool shouldTrace = true) noexcept
                : trace        (shouldTrace && gTraceTaskTimestamps.load (std::memory_order_relaxed)),
                  channelId    (ch),
                  engineInst   (eng),
                  entryNs      (trace ? traceNowNs() : 0)
            {}

            ~TraceScope() noexcept
            {
                if (! trace) return;
                recordTraceEvent (channelId, engineInst, entryNs, traceNowNs(),
                                  gBlockIndex.load (std::memory_order_relaxed));
            }

            TraceScope (const TraceScope&) = delete;
            TraceScope& operator= (const TraceScope&) = delete;
        };
    }
}
