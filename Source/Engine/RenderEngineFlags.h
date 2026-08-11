#pragma once

#include <algorithm>
#include <atomic>

// Render engine flags + tunables.
//
// 2026-05-07 (Batch 10): the master flag was promoted from `constexpr bool`
// to `std::atomic<bool>`.
//
// QA-Ef (2026-05-21): the serial render path was deleted and the dispatcher
// became the single, unconditional render path.  The flag now gates the
// WORKER THREADS, not the audio thread: workers acquire-load it at the top
// of BaySickThreadPool::workerLoop and either run the graph in parallel (true)
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
    // (acquire) in BaySickThreadPool::workerLoop; written by the message thread
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
    // (watchdogTimeoutMillis below) catches any remaining deadlocks
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
    // this long waiting on mAllDone instead of hanging forever -- it
    // catches missed routing-graph cycles and stuck workers.
    //
    // The watchdog must only ever mean DEADLOCK, never slowness, and the
    // fixed 100 ms it used to be could not mean that at both ends of the
    // supported range: 100 ms is 34 block periods at 128 samples / 44.1 kHz
    // but only 1.08 at 4096 / 44.1 kHz, where any instantaneous load above
    // 108% tripped it.  Firing there is strictly worse than not firing --
    // one 4096 block is 92.9 ms, so bailing at 100 ms cannot save the
    // deadline anyway, it just replaces real audio with a cleared block and
    // hands the next block a desynced graph.  So derive the deadline from
    // the live block period instead (RenderGraphDispatcher::prepare).
    //
    // MAGIC NUMBERS: 32 block periods is far past any plausible transient
    // overload (a Debug build's ~5x slowdown included) while still bounding
    // a genuine hang to under a second.  The clamp floor keeps a tiny buffer
    // from tripping on ordinary scheduler jitter -- 100 ms, which is the old
    // constant, so nothing changes at Jeff's 128 -- and the ceiling keeps a
    // huge buffer from stalling the audio thread for longer than that on a
    // real deadlock.  Same shape as BaySickDAWProcessor::settleAudioThread.
    inline constexpr double kWatchdogBlockPeriods = 32.0;
    inline constexpr double kWatchdogMinMillis    = 100.0;
    inline constexpr double kWatchdogMaxMillis    = 1000.0;

    // Falls back to the floor when the rate/size pair is not known yet, so a
    // dispatch that somehow precedes prepare still behaves as it did before.
    inline double watchdogTimeoutMillis (double sampleRate, int blockSize) noexcept
    {
        if (sampleRate <= 0.0 || blockSize <= 0)
            return kWatchdogMinMillis;

        const double blockPeriodMs = 1000.0 * (double) blockSize / sampleRate;
        return std::clamp (kWatchdogBlockPeriods * blockPeriodMs,
                           kWatchdogMinMillis, kWatchdogMaxMillis);
    }

    // Channel id space matches MixerChannelIds (0..999). The ChannelBufferArena
    // pre-allocates one stereo slot per id; most slots are unused at any given
    // moment but the cost is bounded (~8 MB worst case at 1024-sample blocks).
    inline constexpr int kMaxStripChannels = 1000;
}
