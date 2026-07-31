#pragma once

#include "RenderTask.h"
#include "BlockContext.h"
#include "../DSP/AudioClipStreamer.h"

// ── §6.8: the ONE place a frozen block is served ─────────────────────────────
// Five task types substitute frozen audio (EngineInsert, VoxStrip, InstStrip,
// CompositeAudioInsert, RustyInsert).  Each had its own copy of the read, all
// gated on `songMode`, which is how pattern mode ended up with no freeze at all:
// the song render is the arrangement from bar 1, and reading it at a wrapping
// pattern-loop playhead played the song's opening bars instead of the pattern.
//
// Scope selection is subtle enough that five copies is five chances to get it
// wrong -- and the copies had already drifted once (the freeze-tap staleness fix
// had to be applied twice, in two shapes).  One function, five callers.
//
// CONTRACT: returns true only when the block was fully served from a freeze
// file.  False means the caller runs its LIVE engine -- which covers no file,
// the wrong pattern, a position past the end, and a short read.  That is
// freeze's existing stale-plays-live rule, not a new path.
namespace FreezeRead
{

// Is there anything to serve this block?  Two atomic loads and no buffer work.
//
// For callers that write straight into their output buffer, serveBlock's own
// null check is enough.  A caller that must PREPARE a buffer first (VoxStripTask
// stages through a scratch it has to size and clear) needs this: without it the
// preparation happens on every block of every strip whether or not anything is
// frozen, which is real audio-thread cost bought for nothing.
inline bool hasSourceFor (const RenderTask& task, const BlockContext& ctx) noexcept
{
    if (ctx.songMode)
        return task.getFrozenSource() != nullptr;

    return ctx.patternIndex >= 0
        && task.getFrozenPatternSource (ctx.patternIndex) != nullptr;
}

inline bool serveBlock (const RenderTask& task,
                        const BlockContext& ctx,
                        juce::AudioBuffer<float>& dst,
                        int numSamples) noexcept
{
    AudioClipStreamer* fz  = nullptr;
    juce::int64        pos = 0;

    if (ctx.songMode)
    {
        fz = task.getFrozenSource();
        if (ctx.posInfo != nullptr)
            pos = ctx.posInfo->getTimeInSamples().orFallback ((juce::int64) 0);
    }
    else if (ctx.patternIndex >= 0)
    {
        // Index-matched: the pointer is republished on a pattern change, and
        // until it lands the block's pattern will not match, so we fall through
        // to live rather than playing the previous pattern's render.
        fz  = task.getFrozenPatternSource (ctx.patternIndex);
        pos = ctx.patternLocalSamples;
    }

    if (fz == nullptr || pos < 0 || pos >= fz->getTotalLength())
        return false;

    return fz->readRaw (dst, 0, numSamples, pos);
}

} // namespace FreezeRead
