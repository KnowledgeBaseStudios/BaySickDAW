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
// CONTRACT: returns true when the block was fully served IN PLACE OF the live
// engine -- from the freeze file, or as silence while the transport is stopped
// (Jeff's ruling 2026-07-31: a frozen tab is silent when stopped; the playhead
// does not advance, so reading would loop one block as a buzz).  False means
// the caller runs its LIVE engine -- no file, the wrong pattern, a position
// past the end.  That is freeze's existing stale-plays-live rule, not a new
// path.  Pattern scope wraps the loop seam internally; a rate-mismatched file
// is served through the streamer's ratio interpolator (ruling option B).
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

    if (fz == nullptr)
        return false;

    // Jeff's ruling (2026-07-31): a frozen tab is SILENT while the transport is
    // stopped.  The playhead does not advance when stopped, so reading the file
    // here replayed one block forever -- a buzz on every frozen track.  Serving
    // silence rather than returning false keeps the engine skipped, which is
    // the freeze.  Offline renders are unaffected: the OfflineHead publishes
    // isPlaying = true for the whole render.
    if (ctx.posInfo == nullptr || ! ctx.posInfo->getIsPlaying())
    {
        dst.clear (0, numSamples);
        return true;
    }

    const juce::int64 total = fz->getTotalLength();
    if (total <= 0 || pos < 0)
        return false;

    const double fileRate = fz->getFileSampleRate();
    const double sessRate = ctx.sampleRate > 0.0 ? ctx.sampleRate : fileRate;

    if (fileRate == sessRate)
    {
        if (pos >= total) return false;

        // §6.8 seam: in pattern scope the file IS the loop, so the block that
        // straddles the wrap reads tail-then-head.  Falling back to live here
        // clipped the pattern's final samples every iteration AND handed the
        // next iteration's note-ons to an engine that is skipped again one
        // block later -- voices that never receive their note-offs.
        const int tail = (int) juce::jmin ((juce::int64) numSamples, total - pos);
        if (tail < numSamples && ! ctx.songMode)
        {
            if (! fz->readRaw (dst, 0, tail, pos)) return false;
            return fz->readRaw (dst, tail, numSamples - tail, 0);
        }
        return fz->readRaw (dst, 0, numSamples, pos);
    }

    // Jeff's ruling (2026-07-31, option B): a rate-mismatched file is read
    // through the streamer's ratio interpolator, not raw frames -- raw reads at
    // render-domain positions silently corrupted every frozen track in an
    // export at a non-device rate (wrong speed AND wrong material).  The same
    // path covers a device-rate change after freezing.  readAndMix MIXES, so
    // the destination is cleared first; underruns print silence rather than
    // falling back (offline reads block synchronously, so an export is exact).
    const double ratio = fileRate / sessRate;   // file samples per output sample
    double       fpos  = (double) pos * ratio;
    const double fend  = (double) total;
    if (fpos >= fend) return false;

    dst.clear (0, numSamples);

    const int outToEnd = (int) ((fend - fpos) / ratio);
    if (outToEnd < numSamples && ! ctx.songMode)
    {
        fz->readAndMix (dst, 0,        outToEnd,              fpos, ratio,
                        dst.getNumChannels(), 1.0f);
        fz->readAndMix (dst, outToEnd, numSamples - outToEnd, 0.0,  ratio,
                        dst.getNumChannels(), 1.0f);
        return true;
    }
    fz->readAndMix (dst, 0, numSamples, fpos, ratio, dst.getNumChannels(), 1.0f);
    return true;
}

} // namespace FreezeRead
