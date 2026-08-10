#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <cmath>

// QA-TempoMap (2026-07-08): the sample-indexed STEPPED tempo timeline.
//
// A sorted list of segments {startSample, startBeat, bpm}; beat(sample) is
// piecewise-linear through it, so multiple tempo changes sit at exact sample
// positions.  startBeat is the precomputed cumulative integral, which makes
// both directions O(log n) lookups with no accumulation.
//
// Lives as namespace globals (single app-wide timeline) so BOTH
// StandalonePlayHead (Source/Standalone/StandaloneApp.*) and
// PluginProcessor's scheduler can read it without an include cycle -
// PluginProcessor.h cannot include StandaloneApp.h (it is included BY it).
//
// THREADING: single writer = the message thread (StandalonePlayHead's rebuild
// paths).  Readers = audio thread (per block) + render workers + message
// thread (UI cursor).  Publication is the same two-fence SEQLOCK protocol the
// QA-Ed anchor used: writer bumps gSeq odd, stores payload relaxed, bumps
// even.  Readers retry a BOUNDED number of times (detail::kMaxSeqRetries) and
// then bail to a per-thread last-good value - an unbounded seqlock spin lets
// the audio thread wait on a descheduled normal-priority writer for a whole
// scheduler quantum, which is a dropout.  Fixed-capacity atomic arrays - no
// allocation, no locks, no shared_ptr refcount contention on the audio thread.
//
// gCount == 0 means "no timeline published" (e.g. the legacy VST target where
// the host owns tempo) - every consumer falls back to its pre-map linear math.
namespace TempoMap
{
    inline constexpr int kMaxSegs = 512;   // base + markers + one live-write tail; far above any real project

    inline std::atomic<uint32_t> gSeq        { 0 };
    inline std::atomic<int>      gCount      { 0 };
    inline std::atomic<double>   gSampleRate { 44100.0 };
    inline std::atomic<int64_t>  gSegSample[kMaxSegs] {};
    inline std::atomic<double>   gSegBeat  [kMaxSegs] {};
    inline std::atomic<double>   gSegBpm   [kMaxSegs] {};

    inline bool isActive() noexcept { return gCount.load (std::memory_order_acquire) > 0; }

    // Message-thread ONLY.  Arrays must be sorted by sample (and therefore by
    // beat); entry 0 must start at sample 0 / beat 0.
    inline void publish (const int64_t* samples, const double* beats,
                         const double* bpms, int count, double sampleRate) noexcept
    {
        count = juce::jlimit (0, kMaxSegs, count);
        const uint32_t s = gSeq.load (std::memory_order_relaxed);
        gSeq.store (s + 1, std::memory_order_relaxed);              // odd = write in progress
        std::atomic_thread_fence (std::memory_order_release);
        gSampleRate.store (sampleRate, std::memory_order_relaxed);
        for (int i = 0; i < count; ++i)
        {
            gSegSample[i].store (samples[i], std::memory_order_relaxed);
            gSegBeat  [i].store (beats[i],   std::memory_order_relaxed);
            gSegBpm   [i].store (bpms[i],    std::memory_order_relaxed);
        }
        gCount.store (count, std::memory_order_relaxed);
        std::atomic_thread_fence (std::memory_order_release);
        gSeq.store (s + 2, std::memory_order_relaxed);              // even = published
    }

    namespace detail
    {
        // The writer's odd window is a handful of relaxed stores (tens of ns).
        // A reader that loses this many attempts is therefore contending with a
        // PREEMPTED writer, not a busy one, and no amount of further spinning
        // helps - it just burns the audio thread's buffer deadline.  Four is
        // far above the uncontended cost and still bails inside a few hundred
        // nanoseconds.  Matches the SpectrumFeed contract: give up on a torn
        // read rather than retry forever.
        inline constexpr int kMaxSeqRetries = 4;

        struct SegCache { int64_t sample; double beat; double bpm; double sr; };

        // THREAD SAFETY: one cache PER READER THREAD, so the fallback tuple can
        // never be torn by a concurrent reader - each thread only ever writes
        // its own.  Constant-initialized POD, so it lives in the static TLS
        // block the OS allocates at thread creation: no allocation, no lazy-init
        // guard call, no lock on the audio path.  Seed = origin at the default
        // 120 BPM / 44.1 kHz (PatternManager::mGlobalTempo's default), which
        // only shows if a thread's very FIRST read exhausts its retries.
        inline thread_local SegCache gLastGoodSeg { 0, 0.0, 120.0, 44100.0 };

        // One consistent read of segment fields under the seqlock.  `search`
        // receives n and returns the segment index to read.
        //
        // THREAD SAFETY: both loop exits are bounded - an odd sequence and a
        // changed sequence each consume one attempt.  On exhaustion the caller
        // gets this thread's last CONSISTENT segment tuple, never a torn one:
        // musically that is one block of map math against the pre-edit tempo
        // segment, which stays inside the map formula (it does NOT fall through
        // to the pre-map linear branch, so the QA-Ed integer-exact loop-seam
        // math is preserved).
        template <typename SearchFn>
        inline void readSeg (SearchFn&& search,
                             int64_t& outSample, double& outBeat,
                             double& outBpm, double& outSr) noexcept
        {
            for (int attempt = 0; attempt < kMaxSeqRetries; ++attempt)
            {
                const uint32_t s1 = gSeq.load (std::memory_order_acquire);
                if ((s1 & 1u) != 0u) continue;
                const int n = juce::jlimit (1, kMaxSegs, gCount.load (std::memory_order_relaxed));
                const int i = juce::jlimit (0, n - 1, search (n));
                outSample = gSegSample[i].load (std::memory_order_relaxed);
                outBeat   = gSegBeat  [i].load (std::memory_order_relaxed);
                outBpm    = gSegBpm   [i].load (std::memory_order_relaxed);
                outSr     = gSampleRate.load  (std::memory_order_relaxed);
                std::atomic_thread_fence (std::memory_order_acquire);
                if (gSeq.load (std::memory_order_relaxed) == s1)
                {
                    gLastGoodSeg = { outSample, outBeat, outBpm, outSr };
                    return;
                }
            }
            outSample = gLastGoodSeg.sample;
            outBeat   = gLastGoodSeg.beat;
            outBpm    = gLastGoodSeg.bpm;
            outSr     = gLastGoodSeg.sr;
        }
    }

    inline double beatAtSample (int64_t sample) noexcept
    {
        int64_t ss; double sb, bpm, sr;
        detail::readSeg ([sample] (int n)
        {
            int lo = 0, hi = n - 1;
            while (lo < hi)
            {
                const int mid = (lo + hi + 1) / 2;
                if (gSegSample[mid].load (std::memory_order_relaxed) <= sample) lo = mid;
                else hi = mid - 1;
            }
            return lo;
        }, ss, sb, bpm, sr);
        return sb + (double) (sample - ss) * bpm / (60.0 * (sr > 0.0 ? sr : 44100.0));
    }

    inline int64_t sampleAtBeat (double beat) noexcept
    {
        int64_t ss; double sb, bpm, sr;
        detail::readSeg ([beat] (int n)
        {
            int lo = 0, hi = n - 1;
            while (lo < hi)
            {
                const int mid = (lo + hi + 1) / 2;
                if (gSegBeat[mid].load (std::memory_order_relaxed) <= beat) lo = mid;
                else hi = mid - 1;
            }
            return lo;
        }, ss, sb, bpm, sr);
        return ss + (int64_t) std::llround ((beat - sb) * 60.0
                                            * (sr > 0.0 ? sr : 44100.0) / juce::jmax (1e-6, bpm));
    }

    inline double bpmAtSample (int64_t sample) noexcept
    {
        int64_t ss; double sb, bpm, sr;
        detail::readSeg ([sample] (int n)
        {
            int lo = 0, hi = n - 1;
            while (lo < hi)
            {
                const int mid = (lo + hi + 1) / 2;
                if (gSegSample[mid].load (std::memory_order_relaxed) <= sample) lo = mid;
                else hi = mid - 1;
            }
            return lo;
        }, ss, sb, bpm, sr);
        juce::ignoreUnused (ss, sb, sr);
        return bpm;
    }

    // First segment boundary strictly after `sample`, or -1 when the current
    // segment runs to the end.  Lets block-loop consumers (metronome) split a
    // block into constant-tempo spans instead of per-sample map lookups.
    //
    // THREAD SAFETY: bounded on both seqlock exits like detail::readSeg, but it
    // must NOT fall back to a cached boundary - callers walk a cursor forward
    // onto the returned value (PluginProcessor.cpp span splits), so a stale
    // boundary at or behind `sample` would spin them forever.  On exhaustion it
    // returns the already-defined -1: the block is treated as one constant-
    // tempo span, which every caller handles.
    inline int64_t nextBoundaryAfter (int64_t sample) noexcept
    {
        for (int attempt = 0; attempt < detail::kMaxSeqRetries; ++attempt)
        {
            const uint32_t s1 = gSeq.load (std::memory_order_acquire);
            if ((s1 & 1u) != 0u) continue;
            const int n = juce::jlimit (1, kMaxSegs, gCount.load (std::memory_order_relaxed));
            int64_t next = -1;
            for (int i = n - 1; i >= 0; --i)
            {
                const int64_t s = gSegSample[i].load (std::memory_order_relaxed);
                if (s > sample) next = s; else break;
            }
            std::atomic_thread_fence (std::memory_order_acquire);
            if (gSeq.load (std::memory_order_relaxed) == s1) return next;
        }
        return -1;
    }
}
