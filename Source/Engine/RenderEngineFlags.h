#pragma once

// Multi-threaded render engine — Phase 1 scaffolding (2026-05-06).
//
// Phase 1 adds the lock-free task-graph dispatcher infrastructure but does
// NOT wire any per-strip RenderTask wrappers yet. The flag below stays false
// through Phase 1; flipping it in this build will produce silence (the
// dispatcher returns a cleared buffer until per-strip integration ships).
//
// Promotion path: constexpr → runtime atomic + UI toggle in Batch 10.
namespace RenderEngine
{
    // Master switch for the multi-threaded render path.
    //
    //   false  → PluginProcessor::processBlock runs the existing serial loop
    //            unchanged. The MT branch is elided by the compiler
    //            (if constexpr).
    //   true   → dispatcher takes over the block. ALL strips must have
    //            RenderTask wrappers registered, otherwise audio is silent
    //            or partial.
    //
    // Do NOT flip to true until Batches 2-8 have shipped per-strip wrappers
    // for every active strip type.
    inline constexpr bool kEnableMultiThreadedEngine = false;

    // Hard cap on worker threads. The 5950X (16C/32T) saturates well below
    // this for the ~17-task per-block load of a typical heavy session;
    // additional workers thrash cache without throughput gain.
    inline constexpr int kMaxWorkers = 8;

    // Iterations a worker spins on try_dequeue before falling back to its
    // private juce::WaitableEvent. Tuned for ~50 microseconds at 5 GHz —
    // well under typical inter-block idle windows.
    inline constexpr int kWorkerSpinIterations = 256;

    // Channel id space matches MixerChannelIds (0..999). The ChannelBufferArena
    // pre-allocates one stereo slot per id; most slots are unused at any given
    // moment but the cost is bounded (~8 MB worst case at 1024-sample blocks).
    inline constexpr int kMaxStripChannels = 1000;
}
