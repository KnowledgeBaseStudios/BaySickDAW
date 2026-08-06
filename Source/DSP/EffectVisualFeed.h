#pragma once
#include <JuceHeader.h>
#include <atomic>

// ─────────────────────────────────────────────────────────────────────────────
// EffectVisualFeed - QA-Layout T17 (2026-08-05)
// ─────────────────────────────────────────────────────────────────────────────
// The audio->UI channel behind every effect-panel visual, and the gate that
// makes an unwatched visual cost nothing.
//
// WHY THE GATE IS THE POINT.  Ten effects each publishing a column per block,
// forever, whether or not a window is open, is a permanent tax paid for a
// picture nobody is looking at -- and no amount of window or view management
// recovers it, because the cost is on the AUDIO thread and the UI's state is
// invisible from there.  `push` therefore opens with one relaxed atomic load
// and returns immediately when the watcher count is zero: the same
// empty-until-activated fast-path bypass used elsewhere in the engine.
//
// A watcher is a VISIBLE visual.  EffectVisualStrip adds one when it goes on
// screen and drops it when it hides or dies, so the DSP is publishing exactly
// while someone can see the result.  Refcounted rather than a bool because a
// rack panel and a pedal view can legitimately watch the same DSP at once.
//
// THREADING.  Single producer (audio thread) writing `mWrite`, single consumer
// (message thread) reading it.  The consumer can occasionally read a column
// mid-write and get a torn value; that is deliberate and matches SpectrumFeed's
// dropped-frame tolerance -- one wrong pixel column on a scrolling display is
// not worth a lock on the audio thread.  Nothing here allocates.
class EffectVisualFeed
{
public:
    // One display column.  `lo`/`hi` are the sample envelope for that slice;
    // `a`/`b` are per-effect overlays (limiter: gain reduction + ceiling;
    // compressor: gain reduction + threshold; transient: pre/post envelope).
    // Kept deliberately generic -- a visual that needs different data reads
    // these as whatever its panel documented, rather than growing a variant
    // per effect here.
    struct Column
    {
        float lo { 0.0f }, hi { 0.0f }, a { 0.0f }, b { 0.0f };
    };

    static constexpr int      kSize = 1024;   // power of two: index wrap is a mask
    static constexpr uint32_t kMask = (uint32_t) kSize - 1;

    // ── Gate ────────────────────────────────────────────────────────────────
    bool isActive() const noexcept   { return mWatchers.load (std::memory_order_relaxed) > 0; }
    void addWatcher() noexcept       { mWatchers.fetch_add (1, std::memory_order_relaxed); }
    void removeWatcher() noexcept
    {
        // Never below zero: a strip destroyed after its DSP was swapped could
        // otherwise leave a negative count that reads as "inactive" forever.
        int cur = mWatchers.load (std::memory_order_relaxed);
        while (cur > 0 && ! mWatchers.compare_exchange_weak (cur, cur - 1,
                                                             std::memory_order_relaxed))
        {
        }
    }

    // ── Audio thread ────────────────────────────────────────────────────────
    void push (float lo, float hi, float a = 0.0f, float b = 0.0f) noexcept
    {
        if (! isActive())
            return;

        const auto w = mWrite.load (std::memory_order_relaxed);
        mRing[w & kMask] = { lo, hi, a, b };
        mWrite.store (w + 1, std::memory_order_release);
    }

    // ── Message thread ──────────────────────────────────────────────────────
    uint32_t writeIndex() const noexcept { return mWrite.load (std::memory_order_acquire); }
    const Column& at (uint32_t absoluteIndex) const noexcept { return mRing[absoluteIndex & kMask]; }

private:
    std::atomic<int>      mWatchers { 0 };
    std::atomic<uint32_t> mWrite    { 0 };
    Column                mRing[kSize] {};
};
