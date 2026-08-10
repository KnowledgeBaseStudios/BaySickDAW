#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// [G3 PLAYHEAD] diagnostic readout (QA-G3Smoke Task 1, spec call G-9).
//
// The roll-side playhead residual (G3 dossier #30) is unexplained: the roll
// already has the pixel-center click fix (LDT-394), yet clicks still land off
// the playhead line.  This is a READING to characterize the residual so the
// fix can be routed (Rule 3) — not a fix.  Logs every roll click (x → raw /
// snapped beat, snap div, playhead beat) and every playhead paint tick (beat,
// output latency samples, sample rate).
//
// Debug build only.  Mirrors the ClipDropDiag / namirLog one-off
// file-logger convention (Documents/BaySickDAW/*.txt) so the lines are
// readable without a debugger.  Rule-4 catalogued in
// Plans & Specs/Running Notes/burly-restringing-bison.md.  Kept alive by
// Jeff's ruling (2026-08-06); the audio-thread half runs through the
// allocation-free ring below rather than writing the file inline.
// ─────────────────────────────────────────────────────────────────────────────

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include "AppPaths.h"

namespace G3PlayheadDiag
{
#if JUCE_DEBUG
    inline void log (const juce::String& line)
    {
        auto f = AppPaths::appRoot()
                     .getChildFile ("g3_playhead_log.txt");
        f.getParentDirectory().createDirectory();
        f.appendText (juce::Time::getCurrentTime().toString (true, true, true, true)
                       + "  [G3 PLAYHEAD] " + line + juce::newLine);
    }

    // ── Audio-thread readout path ────────────────────────────────────────────
    // [G3 BAR1] (smoke General-1): bar-1 note dropout reading.  Records the
    // scheduling windows on blocks that touch beat 0 + every noteOn emitted
    // with absStart < 0.05, so a dropped first note shows as either a window
    // that opened past 0 (scheduler side) or an emitted-but-silent note
    // (engine side).
    //
    // [G3 PAN] (smoke #19 sweep half): pan-ramp arm readout -- one record per
    // RP takeover arm + one per mid-ramp CC10 stomp.  The arm chain reads
    // correct in code, so this reading discriminates the two candidates:
    // short/fallback span vs a channel CC10 stomping the ramp.
    //
    // Both readouts are Debug-only and Rule-4 catalogued.
    //
    // The audio thread stores POD ONLY -- no File, no directory creation, no
    // juce::String -- into this fixed-size ring, and a message-thread drain
    // does the formatting and the write.  Anything else would poison the very
    // Debug-exe ear checks this diagnostic exists to inform.
    //
    // THREAD SAFETY: MULTI-producer, single consumer (the drain timer).  The
    // render dispatcher does NOT serialize the work it schedules -- it submits
    // every ready task to BaySickThreadPool and then joins the pump itself -- so
    // the audio thread and any pool worker can be inside a different engine's
    // processBlock, and pushing, at the same instant.  Each producer therefore
    // CLAIMS its slot with fetch_add and publishes the filled record with that
    // slot's own release stamp; a plain load/store pair would let two pushes
    // interleave their fields into one slot and advance the index once for two
    // records.  A full ring overwrites the OLDEST unread records rather than
    // blocking, because a diagnostic must never stall the render callback.
    // `tag` MUST be a string literal -- the drain reads it long after the
    // producing block returned.
    struct RtRecord
    {
        const char* tag { nullptr };
        double      v[6] {};
        int         numVals { 0 };

        // Publication stamp: the producer stores claimIndex + 1 here with
        // release AFTER the fields above, so 0 can never read as published and
        // the drain can tell WHICH LAP last finished writing this slot.
        //
        // That is the whole of what it proves, and it is NOT a seqlock.  A
        // producer claiming the slot for a later lap does not invalidate the
        // stamp before it starts filling, so a slot the drain reads as ours can
        // be overwritten field by field while the drain is formatting it, and
        // that one line comes out torn.  Closing that would take an
        // in-progress marker published from the render callback (odd/even plus
        // fences) -- more audio-thread stores in a ring that already drops
        // unread records by design, for a Debug-only readout.
        std::atomic<juce::uint32> published { 0 };
    };

    inline constexpr int kRtRingSize = 512;
    inline std::array<RtRecord, (size_t) kRtRingSize> gRtRing {};

    // Next slot to CLAIM, not the published count -- see RtRecord::published.
    inline std::atomic<juce::uint32>                  gRtWrite { 0 };
    inline juce::uint32                               gRtRead { 0 };   // drain thread only

    inline void pushRT (const char* tag, int numVals,
                        double a = 0.0, double b = 0.0, double c = 0.0,
                        double d = 0.0, double e = 0.0, double f = 0.0) noexcept
    {
        const auto w = gRtWrite.fetch_add (1, std::memory_order_relaxed);
        auto& r = gRtRing[(size_t) (w % (juce::uint32) kRtRingSize)];
        r.tag = tag;
        r.v[0] = a; r.v[1] = b; r.v[2] = c;
        r.v[3] = d; r.v[4] = e; r.v[5] = f;
        r.numVals = juce::jlimit (0, 6, numVals);
        r.published.store (w + 1, std::memory_order_release);
    }

    // Message thread only.  Formats every record produced since the last call
    // and appends them in one write.
    inline void drainRT()
    {
        const auto claimed = gRtWrite.load (std::memory_order_acquire);
        if (claimed == gRtRead) return;

        // Producers lapped us: skip the records they already overwrote.
        if ((juce::uint32) (claimed - gRtRead) > (juce::uint32) kRtRingSize)
            gRtRead = claimed - (juce::uint32) kRtRingSize;

        const auto stamp = juce::Time::getCurrentTime().toString (true, true, true, true);
        juce::String out;
        for (; gRtRead != claimed; ++gRtRead)
        {
            const auto& r    = gRtRing[(size_t) (gRtRead % (juce::uint32) kRtRingSize)];
            const auto  want = gRtRead + 1;
            const auto  pub  = r.published.load (std::memory_order_acquire);

            if (pub != want)
            {
                // Signed difference so uint32 wrap cannot read as "ahead".
                // Ahead: a later lap has FINISHED writing this slot, so our
                // record is gone -- skip it.  Behind (including the
                // never-published 0): the producer has claimed the slot and is
                // still filling it, so leave gRtRead here and collect it on the
                // next tick.
                if ((juce::int32) (pub - want) > 0)
                    continue;
                break;
            }

            // pub == want says only that the last COMPLETED write to this slot
            // was ours; a later lap may be mid-fill over it right now, in which
            // case this line comes out torn.  See RtRecord::published.
            out << stamp << "  " << (r.tag != nullptr ? r.tag : "") << " =";
            for (int i = 0; i < r.numVals; ++i)
                out << " " << juce::String (r.v[i], 4);
            out << juce::newLine;
        }

        if (out.isEmpty()) return;

        auto file = AppPaths::appRoot().getChildFile ("g3_playhead_log.txt");
        file.getParentDirectory().createDirectory();
        file.appendText (out);
    }
#else
    inline void log (const juce::String&) {}
    inline void pushRT (const char*, int, double = 0.0, double = 0.0, double = 0.0,
                        double = 0.0, double = 0.0, double = 0.0) noexcept {}
    inline void drainRT() {}
#endif
}

// Owns the message-thread drain for the audio-thread readout ring above.  Held
// by BaySickDAWProcessor so it is created and destroyed on the message thread
// while the MessageManager is alive -- a static-lifetime Timer would be torn
// down after JUCE's own shutdown.  Compiles to an empty object in Release.
class G3PlayheadDiagDrainer
#if JUCE_DEBUG
    : private juce::Timer
#endif
{
public:
   #if JUCE_DEBUG
    G3PlayheadDiagDrainer()           { startTimerHz (4); }
    ~G3PlayheadDiagDrainer() override { stopTimer(); G3PlayheadDiag::drainRT(); }
   #endif

private:
   #if JUCE_DEBUG
    void timerCallback() override     { G3PlayheadDiag::drainRT(); }
   #endif
};
