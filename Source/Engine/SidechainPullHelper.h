#pragma once

#include <JuceHeader.h>
#include <vector>

#include "RenderTask.h"     // RenderTask, UpstreamLink (transitively via UpstreamLink.h)
#include "../VibeGraph.h"   // VibeGraph::getScRecvBuffer + VibeGraph::kMaxScRecvSlots

// ─────────────────────────────────────────────────────────────────────────────
// pullSidechainPredecessorsToGraph (Batch 9c follow-up, 2026-05-07)
// ─────────────────────────────────────────────────────────────────────────────
//
// Under serial mode, VibeSynthProcessor::routeInsertOutput is called per-source
// after each strip processes; it walks the routing graph's SC edges and copies
// the source's output into every consumer's SC receive slot via
// VibeGraph::getScRecvBuffer(dstId, dstSlot).copyFrom(...).  The consumer's
// processInsert / processBus then calls VibeGraph::pushScArrayToStrip(chId)
// internally, which reads those populated buffers and forwards them to the
// strip's preEq + rack + postEq via setSidechainBuffers.
//
// Under MT, routeInsertOutput sits in serial code below the early `return;`
// at the top of processBlock, so the SC accumulator never gets populated.
// EngineInsertTask / VoxStripTask / etc DO push SC predecessors directly to
// the engine via the ISidechainEngine setSidechainBuffers call -- but that
// only covers ENGINE-level SC.  Compressors, dynamic-EQ bands, and other
// SC-capable DSP that live INSIDE the strip's rack / preEq / postEq read
// from the VibeGraph accumulator that nobody is filling.
//
// This helper is the consumer-side equivalent of routeInsertOutput's SC
// fanout: each task calls it BEFORE its processInsert / processBus, walking
// its own mPredecessors and copyFrom-ing each SC source's mOutputBuffer
// into the strip's SC receive slot.  Mirrors routeInsertOutput's copy-
// replaces semantics -- the encoding contract guarantees at most one source
// per (dst, slot) per block, and clearScRecvBuffers() is called per-block
// at the top of processBlock so the buffers start clean.
//
// channelId is the consumer strip's MixerChannelIds value (e.g.
// MixerChannelIds::layerInsert(idx) for an EngineInsertTask handling a Layer
// strip; MixerChannelIds::kMaster for the master task).  numSamples comes
// from mCtx->numSamples on the calling task.
// ─────────────────────────────────────────────────────────────────────────────
inline void pullSidechainPredecessorsToGraph(
    VibeGraph&                        graph,
    int                                channelId,
    const std::vector<UpstreamLink>&   predecessors,
    int                                numSamples) noexcept
{
    if (numSamples <= 0) return;
    for (const auto& link : predecessors)
    {
        if (! link.isSc) continue;
        if (link.scSlot < 0 || link.scSlot >= VibeGraph::kMaxScRecvSlots) continue;
        if (link.source == nullptr || link.source->mOutputBuffer == nullptr) continue;

        auto* recv = graph.getScRecvBuffer (channelId, link.scSlot);
        if (recv == nullptr) continue;

        const auto& src = *link.source->mOutputBuffer;
        const int nc = juce::jmin (src.getNumChannels(), recv->getNumChannels());
        for (int c = 0; c < nc; ++c)
            recv->copyFrom (c, 0, src, c, 0, numSamples);
    }
}
