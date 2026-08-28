// KBS Plugins - moving data from the audio thread to the screen.
//
// A visualiser needs more than a number per frame: a spectrum needs a block of
// samples, and a scrolling history needs everything that has happened for the
// last minute. Neither can be passed through a mutex, because the audio thread
// must never wait for the UI.
//
// Both structures below solve that the same way - by being allowed to fail. A
// dropped frame in a meter is invisible; a blocked audio thread is a click.
#pragma once

#include <atomic>
#include <array>
#include <algorithm>
#include <cstdint>

namespace kbs {

// ── a block of samples, for the spectrum ───────────────────────────────────
//
// Seqlock. The writer bumps a counter to odd before writing and to even after,
// so a reader that sees an odd count, or a different count either side of its
// copy, knows it read while a write was in progress and discards the frame.
// No locks, no allocation, and the audio thread never waits for anything.
class SpectrumFeed
{
public:
    // 16384 samples is a 341 ms window at 48 kHz. That is a long window, but
    // the ring always holds the most recent block and the UI re-transforms it
    // every frame, so the display slides continuously rather than updating once
    // per window. Frequency resolution is what a spectrum analyser is for; time
    // resolution is what the level meters are for.
    static constexpr int kSize = 16384;

    // Audio thread.
    void push (const float* left, const float* right, int n) noexcept
    {
        seq.fetch_add (1, std::memory_order_release);

        for (int i = 0; i < n; ++i)
        {
            // Mid is what a spectrum analyser shows. Side rides along
            // because EQ Match needs to know how the two domains differ -
            // one mono sum cannot tell it whether a problem lives in the
            // centre or at the edges.
            data[(size_t) write] = 0.5f * (left[i] + right[i]);
            side[(size_t) write] = 0.5f * (left[i] - right[i]);
            write = (write + 1) & (kSize - 1);
        }
        filled = std::min (kSize, filled + n);

        seq.fetch_add (1, std::memory_order_release);
    }

    // UI thread. Returns false if the audio thread was mid-write.
    bool pollSide (float* dest) const noexcept { return pollFrom (side, dest); }

    bool poll (float* dest) const noexcept { return pollFrom (data, dest); }

    bool pollFrom (const std::array<float, kSize>& src, float* dest) const noexcept
    {
        const uint32_t a = seq.load (std::memory_order_acquire);
        if (a & 1u) return false;

        // Oldest first, so the caller gets a contiguous window in time order.
        const int start = write;
        for (int i = 0; i < kSize; ++i)
            dest[i] = src[(size_t) ((start + i) & (kSize - 1))];

        return a == seq.load (std::memory_order_acquire) && filled >= kSize;
    }

private:
    mutable std::atomic<uint32_t> seq { 0 };
    std::array<float, kSize> data {}, side {};
    int write = 0, filled = 0;
};

// ── a scrolling history, for the loudness graph ────────────────────────────
//
// One sample per slot at a fixed rate; the reader walks backwards from the
// write index. A torn read costs one point out of hundreds, which is not
// visible, so this needs no sequence counter at all.
template <int Points>
class HistoryRing
{
public:
    static constexpr int kPoints = Points;

    void prepare (double sampleRate, double hz)
    {
        interval = std::max (1, (int) (sampleRate / hz));
        reset();
    }

    void reset() noexcept
    {
        a.fill (-120.0f);
        b.fill (-120.0f);
        write.store (0, std::memory_order_relaxed);
        counter = 0;
    }

    // Audio thread. Call once per sample; it stores on its own schedule.
    inline void advance (int samples, float valueA, float valueB) noexcept
    {
        counter += samples;
        while (counter >= interval)
        {
            counter -= interval;
            const int w = write.load (std::memory_order_relaxed);
            a[(size_t) w] = valueA;
            b[(size_t) w] = valueB;
            write.store ((w + 1) % Points, std::memory_order_release);
        }
    }

    // UI thread. Fills oldest-to-newest.
    void read (float* outA, float* outB) const noexcept
    {
        const int w = write.load (std::memory_order_acquire);
        for (int i = 0; i < Points; ++i)
        {
            const int idx = (w + i) % Points;
            outA[i] = a[(size_t) idx];
            outB[i] = b[(size_t) idx];
        }
    }

private:
    std::array<float, Points> a {}, b {};
    std::atomic<int> write { 0 };
    int interval = 2400, counter = 0;
};

} // namespace kbs
