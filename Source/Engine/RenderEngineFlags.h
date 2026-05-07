#pragma once

#include <atomic>

// Multi-threaded render engine flags + tunables.
//
// 2026-05-07 (Batch 10): the master flag was promoted from `constexpr bool`
// to `std::atomic<bool>`.  The audio thread now reads it once at the top of
// every processBlock (acquire) and routes either to the MT dispatcher or
// the serial path.  Both branches live in the compiled binary now -- the
// compiler can no longer elide the unused side -- so flipping the flag at
// runtime is a hot-swap: toggle from the message thread (release-store
// from the Mixer hamburger menu) and the very next audio block picks the
// new path with no glitches and no restart.
namespace RenderEngine
{
    // Master switch for the multi-threaded render path.
    //
    //   false  -> PluginProcessor::processBlock runs the serial loop.
    //   true   -> dispatcher takes over the block.  ALL strips must have
    //             RenderTask wrappers registered, otherwise audio is silent
    //             or partial.  (rebuildLinks fires every block regardless,
    //             so toggling from false->true sees fresh predecessor /
    //             child / mInitialDeps state on the very next block.)
    //
    // Default true: MT validated as the production-quality path during
    // Batch 9c (flag flip + watchdog + meter drain + SC pull + bus solo
    // fix).  Pre-flip blockers (B2 GUI deadlock, N1 BaySickVocal shutdown
    // gate, ~StandaloneEditor teardown, B1 deferred-destruction GC) all
    // cleared in 3b2c85a / fdbe9e1 / 47ba7a2.  Watchdog
    // (kWatchdogTimeoutMillis below) catches any remaining deadlocks
    // loudly instead of letting the audio thread hang.
    //
    // Audio thread reads:    .load (std::memory_order_acquire)
    // Message thread writes: .store (newValue, std::memory_order_release)
    //
    // The acquire/release pair publishes any preceding state-edits the
    // message thread did before flipping the flag (e.g. a settings UI
    // change) so the audio thread sees a consistent view.  In practice
    // there's nothing else for the message thread to publish here -- the
    // dispatcher's tasks + arena are always live regardless of flag --
    // but using release/acquire keeps the protocol future-proof and
    // matches the rest of the engine's atomic conventions.
    inline std::atomic<bool> gMultiThreadedEngineEnabled { true };

    // Hard cap on worker threads. The 5950X (16C/32T) saturates well below
    // this for the ~17-task per-block load of a typical heavy session;
    // additional workers thrash cache without throughput gain.
    inline constexpr int kMaxWorkers = 8;

    // Iterations a worker spins on try_dequeue before falling back to its
    // private juce::WaitableEvent. Tuned for ~50 microseconds at 5 GHz —
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
}
