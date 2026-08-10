#pragma once

#include <JuceHeader.h>
#include <vector>

#include "RenderTask.h"     // RenderTask, UpstreamLink (transitively via UpstreamLink.h)
#include "../BaySickGraph.h"   // BaySickGraph::getScRecvBuffer + BaySickGraph::kMaxScRecvSlots

// ─────────────────────────────────────────────────────────────────────────────
// pullSidechainPredecessorsToGraph (Batch 9c follow-up, 2026-05-07)
// ─────────────────────────────────────────────────────────────────────────────
//
// A strip's rack / preEq / postEq can contain SC-capable DSP (compressors,
// dynamic-EQ bands, etc.) that read from BaySickGraph's per-strip scRecv buffers
// (via pushScArrayToStrip -> setSidechainBuffers).  The engine-level
// ISidechainEngine push only covers ENGINE SC, not the strip's internal
// rack/EQ DSP -- so something must fill those scRecv buffers.
//
// This helper does it: each task calls it BEFORE its processInsert /
// processBus, walking its own mPredecessors and copyFrom-ing each SC source's
// mOutputBuffer into the strip's SC receive slot.  Copy-replaces semantics --
// the encoding contract guarantees at most one source per (dst, slot) per
// block, and clearScRecvBuffers() runs per-block so the buffers start clean.
//
// channelId is the consumer strip's MixerChannelIds value (e.g.
// MixerChannelIds::layerInsert(idx) for an EngineInsertTask handling a Layer
// strip; MixerChannelIds::kMaster for the master task).  numSamples comes
// from mCtx->numSamples on the calling task.
// ─────────────────────────────────────────────────────────────────────────────
inline void pullSidechainPredecessorsToGraph(
    BaySickGraph&                        graph,
    int                                channelId,
    const std::vector<UpstreamLink>&   predecessors,
    int                                numSamples) noexcept
{
    if (numSamples <= 0) return;
    for (const auto& link : predecessors)
    {
        if (! link.isSc) continue;
        if (link.scSlot < 0 || link.scSlot >= BaySickGraph::kMaxScRecvSlots) continue;
        if (link.source == nullptr || link.source->mOutputBuffer == nullptr) continue;

        auto* recv = graph.getScRecvBuffer (channelId, link.scSlot);
        if (recv == nullptr) continue;

        // QA-Fe2 SC delay-match (docket 1b): prefer the source's
        // PRE-compensation tap so alignment delays never leak into keys;
        // fall back to the post-everything output for tap-less sources
        // (Master, or a stale un-armed edge).
        const juce::AudioBuffer<float>* srcBuf =
            graph.getScSourceTap (link.source->channelId);
        if (srcBuf == nullptr) srcBuf = link.source->mOutputBuffer;

        const int nc = juce::jmin (srcBuf->getNumChannels(), recv->getNumChannels());
        for (int c = 0; c < nc; ++c)
            recv->copyFrom (c, 0, *srcBuf, c, 0,
                            juce::jmin (numSamples, srcBuf->getNumSamples()));

        // Per-(consumer, slot) key alignment: delays the key to meet a LATE
        // consumer point (values solved by updateBusLatencies).
        graph.applyScRecvDelay (channelId, link.scSlot, numSamples);
    }
}
