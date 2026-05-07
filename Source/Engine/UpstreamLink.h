#pragma once

class RenderTask;

// One incoming edge into a RenderTask. Built by
// RenderGraphDispatcher::rebuildLinks from RoutingGraph::Edge / ScEdge.
//
// Pull-model design: when a task runs, it iterates its mPredecessors and reads
// each `source->mOutputBuffer` (which has already been written by the upstream
// task - guaranteed by the dependency-counter semantics). This replaces the
// existing serial `routeInsertOutput → addFrom into shared accumulator`
// pattern, which would race under parallel execution.
//
// Encoding rules
//   - Audio (main-out / send) edge: `isSc = false`. Use `gainDb` for level
//     (main-out is always 0 dB, sends carry the user-set send amount).
//     `isMainOut` distinguishes the singular main-out cable from sends so
//     downstream tasks can apply send-only behaviors (pre/post fader tap,
//     etc.) without re-querying the routing graph.
//   - Sidechain edge: `isSc = true`, `scSlot = 0..3`. The destination's
//     `run()` reads `source->mOutputBuffer` into its receive slot. Tap point
//     is post-everything per the C.4 contract - by the time this link fires,
//     `source->mOutputBuffer` contains the source strip's full output after
//     rack/EQ/fader/mute/solo/pan.
struct UpstreamLink
{
    RenderTask* source    = nullptr;
    float       gainDb    = 0.0f;   // user-set send amount (main-out = 0 dB)
    bool        prePost   = false;  // true = pre-fader tap (sends only)
    bool        isMainOut = false;  // true = main-out cable, false = send / SC
    bool        isSc      = false;  // true = sidechain edge (vs audio)
    int         scSlot    = 0;      // 0..3 receive slot on destination
};
