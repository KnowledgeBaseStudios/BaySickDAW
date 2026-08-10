#pragma once

class RenderTask;

// One incoming edge into a RenderTask. Built by
// RenderGraphDispatcher::rebuildLinks from RoutingGraph::Edge / ScEdge.
//
// Pull-model design: when a task runs, it iterates its mPredecessors and reads
// one buffer per link, chosen by the flags below -- normally the upstream
// task's `source->mOutputBuffer`, which has already been written (guaranteed by
// the dependency-counter semantics).  A push-model (writing into a shared
// accumulator) would race under parallel execution.
//
// Encoding rules
//   - Audio (main-out / send) edge: `isSc = false`. Use `gainDb` for level
//     (main-out is always 0 dB, sends carry the user-set send amount).
//     `isMainOut` distinguishes a main-out cable from a send so downstream
//     tasks can apply send-only behaviors without re-querying the routing
//     graph -- `prePost` is the one that exists today.  A source may hold up
//     to MixerChannelIds::kMaxMainOutsPerStrip main-out cables, each to a
//     different destination and each carrying a full-level copy; from any one
//     destination's side that is still a single link in its predecessor list.
//   - Sidechain edge: `isSc = true`, `scSlot = 0..3`. The destination's
//     `run()` reads `source->mOutputBuffer` into its receive slot. Tap point
//     is post-everything per the C.4 contract - by the time this link fires,
//     `source->mOutputBuffer` contains the source strip's full output after
//     rack/EQ/fader/mute/solo/pan.
struct UpstreamLink
{
    RenderTask* source    = nullptr;
    float       gainDb    = 0.0f;   // user-set send amount (main-out = 0 dB)
    // SENDS ONLY (ignored when isMainOut or isSc).  True means the destination
    // sums the source's PRE-FADER TAP -- the strip's signal before its fader x
    // mute x solo gain and before its pan -- instead of mOutputBuffer.  The tap
    // lives on the source's BaySickGraph node and is resolved through
    // BaySickGraph::getPreFaderTap; see Tasks/SendSourceRead.h for the one place
    // that choice is made, and BaySickGraph.h for the tap's contract.
    bool        prePost   = false;
    bool        isMainOut = false;  // true = main-out cable, false = send / SC
    bool        isSc      = false;  // true = sidechain edge (vs audio)
    int         scSlot    = 0;      // 0..3 receive slot on destination
};
