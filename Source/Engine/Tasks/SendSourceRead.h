#pragma once

#include <JuceHeader.h>

#include "../RenderTask.h"
#include "../UpstreamLink.h"
#include "../../BaySickGraph.h"

// Which buffer a summing task should read for one incoming audio edge.
//
// Default is the source's mOutputBuffer -- the post-everything strip output.
// A SEND edge whose prePost flag is set reads the source's pre-fader tap
// instead (BaySickGraph::getPreFaderTap), which is the strip's signal before its
// fader x mute x solo gain and before its pan.  Main-out edges and sidechain
// edges never take that branch: main-out is the strip's actual output by
// definition, and sidechain has its own documented tap (getScSourceTap, pulled
// by SidechainPullHelper) with different semantics.
//
// The tap falls back to mOutputBuffer whenever the source is not an armed
// pre-fader node, so a routing edge that outlives its arming by a block
// degrades to the old post-fader behavior rather than to silence.
//
// ORDERING: none of its own.  The send edge that sets prePost is the same edge
// that made the source a predecessor of this task, so the dispatcher's
// mPredecessors / mChildren / mInitialDeps bookkeeping already guarantees the
// source's chain pass finished before this runs, and the acquire on the
// dependency counter that publishes mOutputBuffer publishes the tap with it.
namespace SendSourceRead
{
    inline const juce::AudioBuffer<float>* bufferFor (BaySickGraph& graph,
                                                       const UpstreamLink& link) noexcept
    {
        if (link.source == nullptr)
            return nullptr;

        if (link.prePost && ! link.isMainOut && ! link.isSc)
            if (const auto* tap = graph.getPreFaderTap (link.source->channelId))
                return tap;

        return link.source->mOutputBuffer;
    }
}
