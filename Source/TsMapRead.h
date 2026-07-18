#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <cmath>

// QA-G Task 6 (2026-07-17): the beat-indexed STEPPED time-signature timeline.
//
// A sorted list of segments {startBeat, beatsPerBar (quarter-beats), num, den,
// barIndexAtStart}; barBeatAt(beat) resolves any beat position to its bar
// index / position-in-bar / signature in O(log n).  Grid TS markers are the
// SOLE source (docket #14) — PatternManager publishes on every marker
// mutation + project load/reset.
//
// Positions in the app stay beat-uniform (96 ticks/quarter everywhere); this
// map only answers "which bar is this beat in, and what signature governs it"
// for the metronome, the transport readout, the playhead's reported TS, and
// the Builder ruler's bar lines.
//
// THREADING: mirrors TempoMapRead.h — single writer = message thread
// (PatternManager marker mutations), readers = audio thread (metronome) +
// message thread (readout/ruler).  Same two-fence seqlock publication.
// gCount == 0 means "nothing published yet" — consumers fall back to 4/4.
namespace TsMap
{
    inline constexpr int kMaxSegs = 256;   // base 4/4 + markers; far above any real project

    inline std::atomic<uint32_t> gSeq   { 0 };
    inline std::atomic<int>      gCount { 0 };
    inline std::atomic<double>   gSegBeat[kMaxSegs] {};   // segment start, quarter-beats
    inline std::atomic<double>   gSegBpb [kMaxSegs] {};   // quarter-beats per bar
    inline std::atomic<int>      gSegNum [kMaxSegs] {};
    inline std::atomic<int>      gSegDen [kMaxSegs] {};
    inline std::atomic<int>      gSegBar [kMaxSegs] {};   // cumulative bar index at start

    inline bool isActive() noexcept { return gCount.load (std::memory_order_acquire) > 0; }

    // Message-thread ONLY.  Segments sorted by beat; entry 0 must start at
    // beat 0 (the implicit or bar-1 signature).
    inline void publish (const double* beats, const double* bpbs,
                         const int* nums, const int* dens,
                         const int* bars, int count) noexcept
    {
        count = juce::jlimit (0, kMaxSegs, count);
        const uint32_t s = gSeq.load (std::memory_order_relaxed);
        gSeq.store (s + 1, std::memory_order_relaxed);
        std::atomic_thread_fence (std::memory_order_release);
        for (int i = 0; i < count; ++i)
        {
            gSegBeat[i].store (beats[i], std::memory_order_relaxed);
            gSegBpb [i].store (bpbs[i],  std::memory_order_relaxed);
            gSegNum [i].store (nums[i],  std::memory_order_relaxed);
            gSegDen [i].store (dens[i],  std::memory_order_relaxed);
            gSegBar [i].store (bars[i],  std::memory_order_relaxed);
        }
        gCount.store (count, std::memory_order_relaxed);
        std::atomic_thread_fence (std::memory_order_release);
        gSeq.store (s + 2, std::memory_order_relaxed);
    }

    struct BarBeat
    {
        int    bar          { 0 };     // 0-based bar index
        double beatInBar    { 0.0 };   // quarter-beats into the bar
        double barStartBeat { 0.0 };   // quarter-beat of this bar's downbeat
        double bpb          { 4.0 };   // quarter-beats per bar here
        int    num          { 4 };
        int    den          { 4 };
    };

    // One consistent segment read for `beat` under the seqlock.
    inline void readSegAtBeat (double beat, double& outBeat, double& outBpb,
                               int& outNum, int& outDen, int& outBar) noexcept
    {
        for (;;)
        {
            const uint32_t s1 = gSeq.load (std::memory_order_acquire);
            if ((s1 & 1u) != 0u) continue;
            const int n = juce::jlimit (1, kMaxSegs, gCount.load (std::memory_order_relaxed));
            int lo = 0, hi = n - 1;
            while (lo < hi)
            {
                const int mid = (lo + hi + 1) / 2;
                if (gSegBeat[mid].load (std::memory_order_relaxed) <= beat) lo = mid;
                else hi = mid - 1;
            }
            outBeat = gSegBeat[lo].load (std::memory_order_relaxed);
            outBpb  = gSegBpb [lo].load (std::memory_order_relaxed);
            outNum  = gSegNum [lo].load (std::memory_order_relaxed);
            outDen  = gSegDen [lo].load (std::memory_order_relaxed);
            outBar  = gSegBar [lo].load (std::memory_order_relaxed);
            std::atomic_thread_fence (std::memory_order_acquire);
            if (gSeq.load (std::memory_order_relaxed) == s1) return;
        }
    }

    inline BarBeat barBeatAt (double beat) noexcept
    {
        BarBeat r;
        if (! isActive())
        {
            const double b = juce::jmax (0.0, beat);
            r.bar = (int) std::floor (b / 4.0);
            r.barStartBeat = r.bar * 4.0;
            r.beatInBar = b - r.barStartBeat;
            return r;
        }
        double segBeat = 0.0, bpb = 4.0;
        int num = 4, den = 4, segBar = 0;
        readSegAtBeat (juce::jmax (0.0, beat), segBeat, bpb, num, den, segBar);
        bpb = juce::jmax (1.0e-6, bpb);
        const double into = juce::jmax (0.0, beat) - segBeat;
        const int barsIn  = (int) std::floor (into / bpb);
        r.bar          = segBar + barsIn;
        r.barStartBeat = segBeat + barsIn * bpb;
        r.beatInBar    = juce::jmax (0.0, beat) - r.barStartBeat;
        r.bpb = bpb; r.num = num; r.den = den;
        return r;
    }

    // First TS-segment boundary strictly after `beat`, or -1.0 when the
    // current segment runs to the end.  Lets the metronome split a block
    // into constant-signature spans (mirror of TempoMap::nextBoundaryAfter).
    inline double nextBoundaryAfterBeat (double beat) noexcept
    {
        if (! isActive()) return -1.0;
        for (;;)
        {
            const uint32_t s1 = gSeq.load (std::memory_order_acquire);
            if ((s1 & 1u) != 0u) continue;
            const int n = juce::jlimit (1, kMaxSegs, gCount.load (std::memory_order_relaxed));
            double next = -1.0;
            for (int i = n - 1; i >= 0; --i)
            {
                const double b = gSegBeat[i].load (std::memory_order_relaxed);
                if (b > beat) next = b; else break;
            }
            std::atomic_thread_fence (std::memory_order_acquire);
            if (gSeq.load (std::memory_order_relaxed) == s1) return next;
        }
    }
}
