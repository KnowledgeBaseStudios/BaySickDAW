#include "BaySickPitchDSP.h"
#include "BaySickAlignDSP.h"     // estimateF0Track (shared frame-YIN) + AlignPlaySnapshot (time map)
#include "PitchCorrectorDSP.h"   // QA-Fd: shared 13-scale table for the snap target
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPitchDSP - QA-Fa Task 1 (2026-07-10).  See header.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    // Segmentation calibration (QA-Fd rework, locked 6 -- values tuned at
    // the G2 boundary re-listen): a note ends after ~2 unvoiced frames
    // (~93 ms at 44.1k / 2048 hop) or a sustained pitch jump.  NOTHING is
    // silently discarded: onset-glide fragments merge forward into the note
    // they lead into, and (NewTone model) energetic unvoiced material
    // (consonants/breaths) folds into the adjacent note so one pill owns
    // consonant+vowel -- there are no separate slice pills.
    // kGapFrames / kSplitHoldFrames scaled x4 alongside the kF0Hop 2048->512
    // change so the effective sample thresholds (2*2048 == 8*512) are
    // unchanged -- segmentation timing is identical, just finer-resolved.
    constexpr int    kGapFrames        = 8;
    constexpr float  kSplitSemis       = 0.6f;
    constexpr int    kSplitHoldFrames  = 8;
    constexpr double kMinNoteSec       = 0.06;
    constexpr double kEditCarryTolSec  = 0.05;
    // Glide-merge: a run this short that ramps into its neighbor is an
    // onset glide, not a note (fast glides split at the 2-frame jump
    // cadence = ~93 ms chunks, under this cap).
    constexpr double kGlideMergeMaxSec = 0.12;
    constexpr float  kGlideMinRangeSemis = 0.8f;
    // Unvoiced-energy gate: frames louder than 2% of the composite's peak
    // frame RMS (and above an absolute floor) are energetic unvoiced material
    // (consonants/breaths) that folds into the adjacent note; quieter frames
    // are silence and stay uncovered (NewTone model -- no separate slice pills).
    constexpr float  kSliceRelGate = 0.02f;
    constexpr float  kSliceAbsGate = 1.0e-4f;
    // How close (in F0 frames) an energetic-unvoiced span must sit to a note to
    // FOLD into it (else it becomes its own inline slice block): ~2 frames ==
    // ~23 ms at 512-hop/44.1k -- bridges the 1-2 YIN transition frames where a
    // smooth consonant runs into its vowel (fold = word stays whole), but a
    // plosive/hard-stop closure gap (3+ frames) breaks out as a block, NewTone
    // style.  Tunable: lower = more blocks, higher = more folded into words.
    constexpr int    kAttachGapFrames = 2;

    inline float hzToMidiF (float hz)
    {
        return 12.0f * std::log2 (juce::jmax (hz, 1.0f) / 440.0f) + 69.0f;
    }

    float medianOf (std::vector<float> v)
    {
        if (v.empty()) return 0.0f;
        std::sort (v.begin(), v.end());
        return v[v.size() / 2];
    }

    // Linear-interp read of a source-timed envelope at a fractional source index,
    // edge-clamped.
    static float lerpClamp (const std::vector<float>& v, double idx) noexcept
    {
        if (v.empty()) return 0.0f;
        const int n = (int) v.size();
        if (idx <= 0.0)        return v.front();
        if (idx >= n - 1)      return v.back();
        const int i0 = (int) idx; const float fr = (float) (idx - i0);
        return v[(size_t) i0] + (v[(size_t) i0 + 1] - v[(size_t) i0]) * fr;
    }
}

BaySickPitchDSP::BaySickPitchDSP()
{
    mBakeWorker = std::make_unique<BakeWorker> (*this);
    mBakeWorker->startThread();
}

BaySickPitchDSP::~BaySickPitchDSP()
{
    // Stop the bake worker BEFORE tearing down the members it reads.  Owner
    // (BaySickVocalProcessor) raises its shutdown gate before member teardown,
    // so the audio thread cannot be inside processFilePlay here either.
    if (mBakeWorker != nullptr)
    {
        mBakeWorker->signalThreadShouldExit();
        mBakeWorker->notify();
        mBakeWorker->stopThread (3000);
    }
    delete mCacheActive.exchange (nullptr);
    delete mTimeMapActive.exchange (nullptr);
}

void BaySickPitchDSP::prepare (double sampleRate, int /*maxBlockSize*/)
{
    mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
    // ~30 ms crossfade for the cache handoff (a background re-bake swaps the
    // active cache mid-playback -- fade the old->new so the swap can't step the
    // waveform into a click).
    mCacheFadeLen    = (int) juce::jmax (1.0, 0.030 * mSampleRate);
    mCachePlaying    = nullptr;
    mCacheFading     = nullptr;
    mCacheFadeRemain = 0;
}

// ─── Analysis ─────────────────────────────────────────────────────────────────
void BaySickPitchDSP::analyzeComposite (const float* mono, int numSamples,
                                        double sampleRate,
                                        double startBeat, juce::int64 startSample)
{
    const auto oldRegions = mRegions;

    mRegions.clear();
    mF0Track.clear();
    mAnalyzed     = false;
    mCompositeSec = 0.0;

    if (mono == nullptr || numSamples <= 0 || sampleRate <= 0.0)
    {
        publishEdits();
        return;
    }

    mStartBeat    = startBeat;
    mStartSample  = startSample;
    mAnalysisSr   = sampleRate;
    mCompositeSec = numSamples / sampleRate;

    BaySickAlignDSP::estimateF0Track (mono, numSamples, sampleRate, kF0Hop, mF0Track);
    const double hopSec = kF0Hop / sampleRate;
    const int    numFrames = (int) mF0Track.size();

    // ── Stage 1: voiced runs (gap + sustained-jump splits), NO discard ──────
    struct Run
    {
        int f0 { 0 }, f1 { 0 };           // frame span [f0, f1)
        std::vector<float> midi;          // per-frame MIDI
        // Frames belonging to the run's own stable tail: glide merges
        // prepend frames but the median/vibrato stats stay anchored here so
        // an onset ramp can't skew the note's center.
        int anchorCount { 0 };
    };
    std::vector<Run> runs;

    std::vector<float> midiFrames;
    int runStart = -1, gapCount = 0, jumpCount = 0;

    auto flushRun = [&] (int endFrameExclusive)
    {
        if (runStart < 0) return;
        if (! midiFrames.empty() && endFrameExclusive > runStart)
        {
            Run run;
            run.f0   = runStart;
            run.f1   = endFrameExclusive;
            run.midi = midiFrames;
            run.anchorCount = (int) midiFrames.size();
            runs.push_back (std::move (run));
        }
        runStart = -1;
        midiFrames.clear();
        gapCount = jumpCount = 0;
    };

    for (int f = 0; f < numFrames; ++f)
    {
        const float hz = mF0Track[(size_t) f];
        if (hz <= 0.0f)
        {
            if (runStart >= 0 && ++gapCount >= kGapFrames)
                flushRun (f - gapCount + 1);
            continue;
        }
        gapCount = 0;
        const float m = hzToMidiF (hz);

        if (runStart < 0)
        {
            runStart = f;
            midiFrames.clear();
            midiFrames.push_back (m);
            continue;
        }

        const float med = medianOf (midiFrames);
        if (std::abs (m - med) > kSplitSemis)
        {
            if (++jumpCount >= kSplitHoldFrames)
            {
                // The jump frames belong to the NEW note.
                const int splitAt = f - jumpCount + 1;
                for (int k = 0; k < jumpCount - 1 && ! midiFrames.empty(); ++k)
                    midiFrames.pop_back();
                flushRun (splitAt);
                runStart = splitAt;
                midiFrames.assign ((size_t) jumpCount, m);
                jumpCount = 0;
            }
            else
                midiFrames.push_back (m);
        }
        else
        {
            jumpCount = 0;
            midiFrames.push_back (m);
        }
    }
    flushRun (numFrames);

    // ── Stage 2: glide-merge (right-to-left so chains collapse into the
    //    stable note they lead into).  Merge when (near-)contiguous AND the
    //    run is either a sub-minimum fragment or a short ramp trending into
    //    the next run's pitch.  A stable short note fails the ramp test and
    //    survives as its own (tiny) note.
    for (int i = (int) runs.size() - 2; i >= 0; --i)
    {
        auto& cur  = runs[(size_t) i];
        auto& next = runs[(size_t) i + 1];
        if (next.f0 - cur.f1 > 1) continue;   // real gap: no merge

        const double curLen = (cur.f1 - cur.f0) * hopSec;
        bool merge = curLen < kMinNoteSec;
        if (! merge && curLen < kGlideMergeMaxSec && ! cur.midi.empty())
        {
            std::vector<float> anchor (next.midi.end() - next.anchorCount,
                                       next.midi.end());
            const float nextMed = medianOf (anchor);
            float lo = cur.midi.front(), hi = cur.midi.front();
            for (float v : cur.midi) { lo = juce::jmin (lo, v); hi = juce::jmax (hi, v); }
            const bool ramps = std::abs (cur.midi.back() - nextMed) + 0.3f
                             < std::abs (cur.midi.front() - nextMed);
            merge = ramps && (hi - lo) > kGlideMinRangeSemis;
        }
        if (! merge) continue;

        next.f0 = cur.f0;
        next.midi.insert (next.midi.begin(), cur.midi.begin(), cur.midi.end());
        runs.erase (runs.begin() + i);
    }

    // ── Stage 3: materialize every voiced run as a NOTE (NewTone model -- no
    //    slice pills).  Stats come from the anchor tail; short runs still become
    //    (tiny) notes so pitched material never silently drops (locked 6).  The
    //    frame span + region index is recorded so Stage 4 can fold the adjacent
    //    unvoiced material (consonants/breaths) into the note it belongs to.
    struct NoteFrame { int f0, f1, idx; };
    std::vector<NoteFrame> noteFrames;
    for (const auto& run : runs)
    {
        const double t0 = run.f0 * hopSec;
        const double t1 = juce::jmin (run.f1 * hopSec, mCompositeSec);
        if (t1 - t0 <= 1.0e-6 || run.midi.empty()) continue;

        PitchNoteRegion r;
        r.startSec = t0;
        r.endSec   = t1;

        std::vector<float> anchor (run.midi.end()
                                       - juce::jmin ((int) run.midi.size(),
                                                     run.anchorCount),
                                   run.midi.end());
        r.midi = medianOf (anchor);
        r.f0Hz = 440.0f * std::pow (2.0f, (r.midi - 69.0f) / 12.0f);

        // Vibrato: deviation from the median over the anchor tail.
        // Depth = sqrt(2)*RMS in cents (sine assumption); rate from
        // deviation zero crossings.  Under 15 cents = no vibrato.
        double rms = 0.0;
        int zc = 0;
        for (size_t k = 0; k < anchor.size(); ++k)
        {
            const float dev = anchor[k] - r.midi;
            rms += (double) dev * dev;
            if (k > 0 && ((anchor[k - 1] - r.midi) < 0.0f) != (dev < 0.0f))
                ++zc;
        }
        rms = std::sqrt (rms / (double) juce::jmax ((size_t) 1, anchor.size()));
        const float depthCents = juce::jlimit (0.0f, 300.0f,
            (float) (rms * std::sqrt (2.0) * 100.0));
        const double anchorSec = anchor.size() * hopSec;
        if (depthCents >= 15.0f)
        {
            r.vibDepthCents = depthCents;
            r.vibRateHz     = juce::jlimit (3.0f, 9.0f,
                (float) (zc / (2.0 * juce::jmax (0.05, anchorSec))));
        }

        noteFrames.push_back ({ run.f0, run.f1, (int) mRegions.size() });
        mRegions.push_back (r);
    }

    // ── Stage 4: fold energetic unvoiced spans into the adjacent note (NewTone
    //    model).  Frame RMS over the hop grid; a contiguous above-gate span that
    //    no note covers is consonant/breath material -- fold it into the
    //    FOLLOWING note (a leading consonant/breath) or, failing that, the
    //    PRECEDING note (a trailing consonant), so one pill owns consonant+vowel.
    //    A span with no note within kAttachGapFrames (a plosive closure / an
    //    isolated breath) becomes its own inline SLICE block at the nearest note's
    //    lane; only quiet frames (true silence) stay uncovered.
    if (! noteFrames.empty())
    {
        std::vector<float> frameRms ((size_t) numFrames, 0.0f);
        float peakRms = 0.0f;
        for (int f = 0; f < numFrames; ++f)
        {
            const int a = f * kF0Hop;
            const int z = juce::jmin (numSamples, a + kF0Hop);
            double acc = 0.0;
            for (int s = a; s < z; ++s) acc += (double) mono[s] * mono[s];
            const float v = (float) std::sqrt (acc / juce::jmax (1, z - a));
            frameRms[(size_t) f] = v;
            peakRms = juce::jmax (peakRms, v);
        }
        const float gate = juce::jmax (kSliceAbsGate, kSliceRelGate * peakRms);

        std::vector<bool> covered ((size_t) numFrames, false);
        for (const auto& nf : noteFrames)
            for (int f = juce::jlimit (0, numFrames, nf.f0);
                 f < juce::jlimit (0, numFrames, nf.f1); ++f)
                covered[(size_t) f] = true;

        // Extend the chosen note over [a, z); leading -> following note's start,
        // trailing -> preceding note's end.  noteFrames is kept in sync so a
        // later span sees the grown boundary.
        auto foldSpan = [&] (int a, int z)
        {
            if (z - a <= 0) return;
            int prevI = -1, nextI = -1;
            for (int k = 0; k < (int) noteFrames.size(); ++k)
            {
                if (noteFrames[(size_t) k].f1 <= a) prevI = k;
                if (nextI < 0 && noteFrames[(size_t) k].f0 >= z) nextI = k;
            }
            if (nextI >= 0 && (noteFrames[(size_t) nextI].f0 - z) <= kAttachGapFrames)
            {
                mRegions[(size_t) noteFrames[(size_t) nextI].idx].startSec = (double) a * hopSec;
                noteFrames[(size_t) nextI].f0 = a;
            }
            else if (prevI >= 0 && (a - noteFrames[(size_t) prevI].f1) <= kAttachGapFrames)
            {
                mRegions[(size_t) noteFrames[(size_t) prevI].idx].endSec
                    = juce::jmin ((double) z * hopSec, mCompositeSec);
                noteFrames[(size_t) prevI].f1 = z;
            }
            else
            {
                // Isolated span (a plosive closure / breath bounded by silence):
                // NewTone shows these as inline unpitched blocks, not a bottom
                // lane.  Materialize a slice at the NEAREST note's pitch lane
                // (midi is placement only -- the applicator skips pitch for slices).
                PitchNoteRegion sl;
                sl.isSlice  = true;
                sl.startSec = (double) a * hopSec;
                sl.endSec   = juce::jmin ((double) z * hopSec, mCompositeSec);
                int lane = -1;
                if (nextI >= 0 && prevI >= 0)
                    lane = ((noteFrames[(size_t) nextI].f0 - z)
                                <= (a - noteFrames[(size_t) prevI].f1)) ? nextI : prevI;
                else if (nextI >= 0) lane = nextI;
                else if (prevI >= 0) lane = prevI;
                if (lane >= 0)
                    sl.midi = mRegions[(size_t) noteFrames[(size_t) lane].idx].midi;
                if (sl.endSec - sl.startSec > 1.0e-6)
                    mRegions.push_back (sl);
            }
        };

        int spanStart = -1;
        for (int f = 0; f < numFrames; ++f)
        {
            const bool energetic = ! covered[(size_t) f]
                                && mF0Track[(size_t) f] <= 0.0f
                                && frameRms[(size_t) f] > gate;
            if (energetic) { if (spanStart < 0) spanStart = f; }
            else if (spanStart >= 0) { foldSpan (spanStart, f); spanStart = -1; }
        }
        if (spanStart >= 0) foldSpan (spanStart, numFrames);
    }

    std::sort (mRegions.begin(), mRegions.end(),
               [] (const PitchNoteRegion& a, const PitchNoteRegion& b)
               { return a.startSec < b.startSec; });

    // Carry user edits across re-analysis for regions that still line up
    // (same kind only -- re-analysis regenerates all-note segmentation, so a
    // manually sliced-off piece does not survive a re-analyze).  The
    // QA-Fd edit set carries too: popup curves, Variation, and the time edits
    // (FA-9's carry contract -- a grid nudge must not silently delete moves).
    for (auto& r : mRegions)
        for (const auto& old : oldRegions)
            if (old.isSlice == r.isSlice
                && std::abs (old.startSec - r.startSec) < kEditCarryTolSec)
            {
                r.shiftSemis   = old.shiftSemis;
                r.formantSemis = old.formantSemis;
                r.vibDepthMult = old.vibDepthMult;
                r.vibRateMult  = old.vibRateMult;
                r.variation    = old.variation;
                r.volShape     = old.volShape;
                r.pitchShape   = old.pitchShape;
                r.dstStartSec  = old.dstStartSec;
                r.dstEndSec    = old.dstEndSec;
                r.detached     = old.detached;
                break;
            }

    mComposite = std::make_shared<const std::vector<float>> (mono, mono + numSamples);
    mAnalyzed = true;
    publishEdits();
}

// ─── Edit publish -> request a background re-bake + rebuild the time map ───────
void BaySickPitchDSP::publishEdits()
{
    requestBake();
    publishTimeMap();
}

// Message thread: snapshot the edit + knob state into mBakeInput and wake the
// worker.  The worker copies mBakeInput under the mutex, so the message thread
// may keep editing mRegions freely.  Coalesced via mBakeDirty.
void BaySickPitchDSP::requestBake()
{
    Snapshot snap;
    snap.regions     = mRegions;
    snap.sampleRate  = mAnalysisSr;
    snap.startSample = 0;   // composite-relative for the bake envelope
    snap.f0Track     = mF0Track;
    for (const auto& r : mRegions)
    {
        // A pure time move (hasTimeEdit, no pitch/vol edit) must still force a
        // bake -- otherwise anyEdits stays false, neutral wins, the cache stays
        // empty, and the move falls through to the realtime align-decode warp,
        // which hard-seeks at each region boundary (the time-move garble).
        if (r.hasEdits() || r.hasTimeEdit())     snap.anyEdits   = true;
        if (std::abs (r.formantSemis) > 0.01f)   snap.anyFormant = true;
    }
    {
        std::lock_guard<std::mutex> lk (mBakeMutex);
        mBakeInput.snap          = std::move (snap);
        mBakeInput.composite     = mComposite;
        mBakeInput.timeMap       = buildTimeMapSnapshot();   // #1/#2: bake at the edited timeline
        mBakeInput.timelineStart = mStartSample;   // for the playback cache read
        mBakeInput.engine    = mEngine.load  (std::memory_order_relaxed);
        mBakeInput.focus     = mFocus01.load (std::memory_order_relaxed);
        mBakeInput.mod       = mModAmt.load  (std::memory_order_relaxed);
        mBakeInput.speed     = mSpeedMs.load (std::memory_order_relaxed);
        mBakeInput.throat    = mThroat.load  (std::memory_order_relaxed);
        mBakeInput.snapOn    = mSnapOn.load  (std::memory_order_relaxed);
        mBakeInput.rootPc    = mRootPc.load  (std::memory_order_relaxed);
        mBakeInput.scaleIdx  = mScaleIdx.load(std::memory_order_relaxed);
    }
    mBakeDirty.store (true, std::memory_order_release);
    if (mBakeWorker != nullptr) mBakeWorker->notify();
}

// Worker thread: bake the whole composite through the selected engine into a new
// CacheSnapshot + publish it (atomic swap, retire the old one).  No edits (+
// neutral knobs) -> publish an empty cache so playback falls back to dry.
void BaySickPitchDSP::bakeToCache (const BakeInput& in)
{
    const bool neutral = ! in.snap.anyEdits
                      && in.focus < 0.001f && std::abs (in.mod - 1.0f) < 0.01f
                      && std::abs (in.throat) < 0.01f;   // QA-Fe Task 6: throat alone must bake

    auto snap = std::make_unique<CacheSnapshot>();
    snap->sampleRate  = in.snap.sampleRate;
    snap->startSample = in.timelineStart;   // timeline origin for the playback read

    if (! neutral && in.composite != nullptr && ! in.composite->empty())
    {
        const auto& comp = *in.composite;
        const int   n    = (int) comp.size();
        auto baked = bakeSpan (comp.data(), n, in.snap.sampleRate, in.snap,
                               (PitchEngine) in.engine, in.focus, in.mod, in.speed, in.throat,
                               in.snapOn, in.rootPc, in.scaleIdx, 0, n, in.timeMap.get());
        snap->audio.assign (baked.getReadPointer (0),
                            baked.getReadPointer (0) + baked.getNumSamples());
    }
    // neutral -> snap->audio stays empty; playback reads dry.

    auto* old = mCacheActive.exchange (snap.release(), std::memory_order_acq_rel);
    if (old != nullptr)
        mCacheRetired.push_back (std::unique_ptr<CacheSnapshot> (old));
    // Trim to the 8-deep bound by erasing the OLDEST caches, but never free one
    // the audio thread still holds across blocks: the last-adopted cache
    // (hazardPlaying, dereferenced at the next swap check) or the one it is
    // crossfading FROM (hazardFading).  A transport pause freezes the audio thread
    // while the user keeps editing (many bakes), so we must SKIP the (<=2) pinned
    // caches and keep trimming the rest -- stopping dead at the first pinned entry
    // would let the ring grow without bound during a stopped tuning session.  The
    // pinned caches are the newest anyway (active + fading), except a frozen
    // hazardPlaying which is kept plus the 7 newest.  (Worker thread only -- no RT
    // constraint here; the audio thread never touches mCacheRetired.)
    auto* hazP = mCacheHazardPlaying.load (std::memory_order_acquire);
    auto* hazF = mCacheHazardFading .load (std::memory_order_acquire);
    for (size_t i = 0; mCacheRetired.size() > 8 && i < mCacheRetired.size(); )
    {
        auto* p = mCacheRetired[i].get();
        if (p == hazP || p == hazF) { ++i; continue; }        // keep pinned caches
        mCacheRetired.erase (mCacheRetired.begin() + (std::ptrdiff_t) i);
    }
}

// ─── QA-Fd time-edit engine ───────────────────────────────────────────────────
// Build the EDITED->SOURCE channel map from the regions' dst spans.  Anchor
// set: identity pin at 0, per-region (dstStart->srcStart)+(dstEnd->srcEnd)
// pairs (gaps between regions map linearly = the gap counter-warp), and a
// slope-1 tail pin.  Detached pills may put a jump between consecutive
// anchors; the strict-monotone nudge below turns it into a steep ramp
// (~the lookup slope rail) and the decode layer treats big jumps as cuts.
// nullptr when nothing is analyzed or no region carries a time edit.
std::unique_ptr<AlignPlaySnapshot> BaySickPitchDSP::buildTimeMapSnapshot() const
{
    bool anyTime = false;
    for (const auto& r : mRegions)
        if (r.hasTimeEdit()) { anyTime = true; break; }

    if (! (mAnalyzed && anyTime)) return nullptr;

    struct A { double dst, src; };
    std::vector<A> a;
    a.reserve (mRegions.size() * 2 + 2);
    a.push_back ({ 0.0, 0.0 });
    for (const auto& r : mRegions)
    {
        a.push_back ({ r.dstStart(), r.startSec });
        a.push_back ({ r.dstEnd(),   r.endSec   });
    }
    if (! mRegions.empty())
    {
        // Tail pin from the region ending LATEST in dst (a detached
        // last-source pill may sit earlier on the edited timeline).
        const PitchNoteRegion* lastDst = &mRegions.front();
        for (const auto& r : mRegions)
            if (r.dstEnd() > lastDst->dstEnd())
                lastDst = &r;
        const double tail = juce::jmax (0.0, mCompositeSec - lastDst->endSec);
        a.push_back ({ lastDst->dstEnd() + tail, mCompositeSec });
    }
    std::sort (a.begin(), a.end(),
               [] (const A& l, const A& r) { return l.dst < r.dst; });
    // Strict dst monotonicity: a detach jump collapses to a ~1 ms dst
    // ramp; the source side is NOT clamped (jumps are legal there).
    for (size_t i = 1; i < a.size(); ++i)
        a[i].dst = juce::jmax (a[i].dst, a[i - 1].dst + 1.0e-3);

    auto snap = std::make_unique<AlignPlaySnapshot>();
    snap->followerChannelId  = 0;   // unused for the time map (index-matched)
    snap->commonStartBeat    = mStartBeat;
    snap->commonStartSample  = mStartSample;
    snap->analysisSampleRate = mAnalysisSr;
    for (const auto& p : a)
    {
        snap->guideSec  .push_back (p.dst);
        snap->dubSec    .push_back (p.src);
        snap->pitchSemis.push_back (0.0f);
    }
    snap->computeTangents();
    if (snap->guideSec.size() < 2)
        snap.reset();
    return snap;
}

// publishEdits helper: rebuild + atomically swap the audio-thread time map.
void BaySickPitchDSP::publishTimeMap()
{
    auto snap = buildTimeMapSnapshot();

    auto* old = mTimeMapActive.exchange (snap.release(), std::memory_order_acq_rel);
    if (old != nullptr)
        mTimeMapRetired.push_back (std::unique_ptr<AlignPlaySnapshot> (old));
    while (mTimeMapRetired.size() > 8)
        mTimeMapRetired.erase (mTimeMapRetired.begin());
}

juce::int64 BaySickPitchDSP::timeMapHash() const
{
    juce::int64 h = 0;
    for (const auto& r : mRegions)
    {
        if (! r.hasTimeEdit()) continue;
        h = h * 1000003
          + (juce::int64) std::llround (r.startSec * 1.0e4);
        h = h * 1000003
          + (juce::int64) std::llround (r.dstStart() * 1.0e4);
        h = h * 1000003
          + (juce::int64) std::llround (r.dstEnd() * 1.0e4);
        h = h * 31 + (r.detached ? 7 : 1);
    }
    return h;
}

double BaySickPitchDSP::srcToEditedSec (double srcSec) const
{
    // Region walk (regions sorted by src start): inside a region -> lerp to
    // its dst span; in a gap -> lerp between the surrounding edges; outside
    // the ends -> offset continuity.
    if (mRegions.empty()) return srcSec;

    double prevSrc = 0.0, prevDst = 0.0;
    for (const auto& r : mRegions)
    {
        if (srcSec < r.startSec)
        {
            const double span = r.startSec - prevSrc;
            const double f = (span > 1.0e-9) ? (srcSec - prevSrc) / span : 0.0;
            return prevDst + f * (r.dstStart() - prevDst);
        }
        if (srcSec < r.endSec)
        {
            const double span = r.endSec - r.startSec;
            const double f = (span > 1.0e-9) ? (srcSec - r.startSec) / span : 0.0;
            return r.dstStart() + f * (r.dstEnd() - r.dstStart());
        }
        prevSrc = r.endSec;
        prevDst = r.dstEnd();
    }
    return prevDst + (srcSec - prevSrc);
}

void BaySickPitchDSP::clearAllEdits()
{
    // "Clear every pitch edit" = the full QA-Fd edit set too: sub-edit
    // curves, Variation, and the time edits (same semantics as the per-pill
    // Restore to Original State, channel-wide).
    for (auto& r : mRegions)
    {
        r.shiftSemis   = 0.0f;
        r.formantSemis = 0.0f;
        r.vibDepthMult = 1.0f;
        r.vibRateMult  = 1.0f;
        r.variation    = 1.0f;
        r.volShape.clear();
        r.pitchShape.clear();
        r.dstStartSec  = -1.0;
        r.dstEndSec    = -1.0;
        r.detached     = false;
    }
    publishEdits();
}

// ─── Knobs ────────────────────────────────────────────────────────────────────
void BaySickPitchDSP::setFocus01 (float v)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    if (v != mFocus01.load (std::memory_order_relaxed))
    {
        mFocus01.store (v, std::memory_order_relaxed);
        requestBake();
    }
}

void BaySickPitchDSP::setModAmount (float v)
{
    v = juce::jlimit (0.0f, 2.0f, v);
    if (v != mModAmt.load (std::memory_order_relaxed))
    {
        mModAmt.store (v, std::memory_order_relaxed);
        requestBake();
    }
}

void BaySickPitchDSP::setSpeedMs (float ms)
{
    ms = juce::jlimit (5.0f, 300.0f, ms);
    if (ms != mSpeedMs.load (std::memory_order_relaxed))
    {
        mSpeedMs.store (ms, std::memory_order_relaxed);
        requestBake();
    }
}

void BaySickPitchDSP::setThroat (float semis)
{
    semis = juce::jlimit (-12.0f, 12.0f, semis);
    if (semis != mThroat.load (std::memory_order_relaxed))
    {
        mThroat.store (semis, std::memory_order_relaxed);
        requestBake();
    }
}

void BaySickPitchDSP::setChainOn (bool on)
{
    if (on != mChainOn.load (std::memory_order_relaxed))
        mChainOn.store (on, std::memory_order_relaxed);
}

// Root / Scale / Snap only govern the editor drag-wall + the Ctrl+A force after
// the Center/Focus decouple -- the bake no longer reads them, so no re-bake here
// (re-baking on a Snap toggle would also violate "no auto-correction on toggle").
void BaySickPitchDSP::setRoot (int pc)
{
    pc = juce::jlimit (0, 11, pc);
    if (pc != mRootPc.load (std::memory_order_relaxed))
        mRootPc.store (pc, std::memory_order_relaxed);
}

void BaySickPitchDSP::setScaleIdx (int idx)
{
    idx = juce::jlimit (0, PitchCorrectorDSP::kNumScales - 1, idx);
    if (idx != mScaleIdx.load (std::memory_order_relaxed))
        mScaleIdx.store (idx, std::memory_order_relaxed);
}

void BaySickPitchDSP::setSnapOn (bool on)
{
    if (on != mSnapOn.load (std::memory_order_relaxed))
        mSnapOn.store (on, std::memory_order_relaxed);
}

// ─── Per-sample edit -> envelope resolver (bake input) ────────────────────────
void BaySickPitchDSP::computeEnvelopes (int numSamples, juce::int64 timelineStartSample,
                                        double sr, const Snapshot& snap,
                                        float focus01, float modAmt, float speedMs, bool chainOn,
                                        bool snapScale, int rootPc, int scaleIdx,
                                        ApplicatorState& st,
                                        float* outRatio, float* outFormant, float* outGain,
                                        double srcX0, double srcRatePerSample,
                                        bool srcValid) const noexcept
{
    const auto& regions = snap.regions;
    if (regions.empty()) return;

    const float smoothCoef = 1.0f - std::exp (-1.0f / (float) (speedMs * 0.001 * sr));
    const float gainCoef   = 1.0f - std::exp (-1.0f / (float) (0.005 * sr));

    // startSample is composite-relative (0) for the bake; the origin subtraction
    // keeps tSec in composite-relative seconds (rate-invariant).
    const double snapStartSec = (double) snap.startSample / snap.sampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        // QA-Fd source-domain stamps: resolve the pill at the SOURCE position the
        // audible audio came from when the decode layer warped this channel.
        const double tSec = srcValid
            ? (srcX0 + (double) i * srcRatePerSample) / sr - snapStartSec
            : (double) (timelineStartSample + i) / sr - snapStartSec;

        // Monotonic cursor with seek rewind (transport jumps).
        while (st.cursor < (int) regions.size()
               && regions[(size_t) st.cursor].endSec <= tSec)
            ++st.cursor;
        while (st.cursor > 0
               && regions[(size_t) st.cursor - 1].endSec > tSec)
            --st.cursor;

        const bool inRegion = st.cursor < (int) regions.size()
                           && tSec >= regions[(size_t) st.cursor].startSec
                           && tSec <  regions[(size_t) st.cursor].endSec;

        float targetSemis   = 0.0f;
        // Per-sample modulation rides POST-smoother: the Speed glide low-passes a
        // 5-6 Hz vibrato wiggle / Variation deviation / drawn pitch curve to
        // near-nothing if routed through it.  Only the note CENTER glides.
        float fastSemis     = 0.0f;
        float targetFormant = 0.0f;
        float gain          = 1.0f;

        // Chain OFF resolves neutral targets so the smoothing glides everything
        // home; playback gates on chainOn (the bake always bakes the edited state).
        if (inRegion && chainOn)
        {
            const auto& r = regions[(size_t) st.cursor];
            // Slice pieces carry no pitch identity: volume shape applies,
            // pitch/focus/vibrato stay neutral.
            if (r.isSlice)
            {
                const double len = r.endSec - r.startSec;
                if (len > 1.0e-6)
                    gain = r.volGainAt ((float) ((tSec - r.startSec) / len));
            }
            else
            {
                if (st.cursor != st.lastRegion)
                {
                    st.lastRegion = st.cursor;
                    st.vibPhase   = 0.0;
                }

                // Center knob (Newtone model): a micro-tonal pull of the EDITED
                // center toward the nearest semitone CENTER -- always, scale-
                // independent, whether or not Snap is on (Snap only governs the
                // editor drag-wall + the Ctrl+A force, never this per-sample pull).
                const float base   = r.midi + r.shiftSemis;
                const float target = std::round (base);
                targetSemis = r.shiftSemis + (target - base) * focus01;

                const double len01 = r.endSec - r.startSec;
                const float  t01   = (len01 > 1.0e-6)
                    ? (float) ((tSec - r.startSec) / len01) : 0.0f;

                // QA-Fd sub-edit system: additive per-pill pitch curve.
                if (! r.pitchShape.empty())
                    fastSemis += juce::jlimit (-24.0f, 24.0f, r.pitchSemisAt (t01));

                // QA-Fd 15a Variation: scale the note's own contour wiggle around
                // its center (deviation from the analysis F0 track).
                if (std::abs (r.variation - 1.0f) > 0.01f && ! snap.f0Track.empty())
                {
                    const double hopSec = (double) kF0Hop / snap.sampleRate;
                    const int fr = juce::jlimit (0, (int) snap.f0Track.size() - 1,
                                                 (int) (tSec / hopSec));
                    const float hz = snap.f0Track[(size_t) fr];
                    if (hz > 0.0f)
                    {
                        const float devSemis = hzToMidiF (hz) - r.midi;
                        if (std::abs (devSemis) < 6.0f)   // ignore track glitches
                            fastSemis += (r.variation - 1.0f) * devSemis;
                    }
                }

                // Vib knob is ADDITIVE on top of the natural vibrato (mult 1 =
                // nothing added); flattening the REAL wiggle is Variation's job.
                const float addMult = (r.vibDepthMult * modAmt) - 1.0f;
                if (std::abs (addMult) > 0.01f)
                {
                    const float baseDepth = (r.vibDepthCents > 0.0f) ? r.vibDepthCents : 25.0f;
                    const float rate      = r.vibRateHz * r.vibRateMult;
                    st.vibPhase += juce::MathConstants<double>::twoPi * rate / sr;
                    if (st.vibPhase > juce::MathConstants<double>::twoPi)
                        st.vibPhase -= juce::MathConstants<double>::twoPi;
                    fastSemis += (float) std::sin (st.vibPhase)
                               * juce::jlimit (-300.0f, 300.0f, baseDepth * addMult) / 100.0f;
                }

                targetFormant = r.formantSemis;
                gain = r.volGainAt (t01);
            }
        }

        st.smoothedSemis   += (targetSemis   - st.smoothedSemis)   * smoothCoef;
        st.smoothedFormant += (targetFormant - st.smoothedFormant) * smoothCoef;
        st.smoothedGain    += (gain          - st.smoothedGain)    * gainCoef;

        outRatio  [i] = std::pow (2.0f, (st.smoothedSemis + fastSemis) / 12.0f);
        outFormant[i] = st.smoothedFormant;
        outGain   [i] = st.smoothedGain;
    }
}

// ─── Shared bake core (message + worker thread) ───────────────────────────────
juce::AudioBuffer<float> BaySickPitchDSP::bakeSpan (const float* mono, int numSamples, double sr,
                                                    const Snapshot& snap, PitchEngine engine,
                                                    float focus, float mod, float speed, float throat,
                                                    bool snapOn, int rootPc, int scaleIdx,
                                                    juce::int64 spanStart, int spanLen,
                                                    const AlignPlaySnapshot* timeMap) const
{
    juce::AudioBuffer<float> out;
    spanStart = juce::jlimit ((juce::int64) 0, (juce::int64) numSamples, spanStart);
    if (spanLen < 0) spanLen = numSamples - (int) spanStart;
    spanLen = juce::jmin (spanLen, numSamples - (int) spanStart);
    if (mono == nullptr || spanLen <= 0 || sr <= 0.0) return out;

    out.setSize (1, spanLen);
    out.copyFrom (0, 0, mono + spanStart, spanLen);   // dry fallback

    const bool neutral = ! snap.anyEdits && focus < 0.001f && std::abs (mod - 1.0f) < 0.01f
                      && std::abs (throat) < 0.01f;   // QA-Fe Task 6
    if (neutral) return out;

    ApplicatorState st;
    std::vector<float> ratio     ((size_t) spanLen, 1.0f);
    std::vector<float> formSemis ((size_t) spanLen, 0.0f);
    std::vector<float> gain      ((size_t) spanLen, 1.0f);
    computeEnvelopes (spanLen, spanStart, sr, snap, focus, mod, speed, /*chainOn*/ true,
                      snapOn, rootPc, scaleIdx, st,
                      ratio.data(), formSemis.data(), gain.data());

    // QA-Fe Option A (2026-07-15): the chosen engine does BOTH the time-warp and
    // the pitch in ONE native pass (bakeWarped) whenever there is a time edit.
    // Align's applyWarp phase vocoder is no longer in the pitch path -- it
    // wobbled on the pitch editor's sharp per-note map (identity note bodies +
    // hard ratio steps in the gaps).  RubberBand/Signalsmith stretch as they
    // stream; WORLD re-times its own analysis frames (phase-safe).  With NO time
    // edit we do a plain length-preserving pitch bake on the RAW take -- WORLD
    // included: it no longer gets an applyWarp "clean-up" pre-pass, so WORLD's
    // own character shows through (owner call 2026-07-15, the PV was masking it).
    const bool anyTime = (timeMap != nullptr && timeMap->guideSec.size() >= 2);
    auto shifter = makePitchShifter (engine);

    if (anyTime)
    {
        // Edited-timeline length + the per-output-sample SOURCE position map
        // (edited sample j reads source position srcPos[j]).  srcPos + the
        // envelopes are SPAN-RELATIVE (index into the `mono + spanStart` buffer
        // bakeWarped receives, and into computeEnvelopes' span-local envelopes
        // whose element i is composite sample spanStart+i).  spanStart is 0 for
        // every current caller; the subtraction only matters for a future
        // sub-span render and is a no-op at spanStart==0.
        const double edDurSec = timeMap->guideSec.back();
        const int    outLen   = juce::jmax (1, (int) std::llround (edDurSec * sr));

        // QA-Fe2 item 4 (segmentation-aware resample): a detach cut is encoded
        // as a ~1 ms guide-side ramp whose dub side jumps (backward, or far
        // beyond any legal warp ratio).  The Hermite lookup SWEEPS through
        // every intermediate source position across that ramp, so srcPos and
        // the envelope reads pick up unrelated notes' pitch/formant/gain right
        // at the cut.  Snap samples inside a ramp to the nearest side -- the
        // shifters then see a clean instantaneous step (their backward-step
        // detach detection fires at a precise sample instead of mid-sweep).
        struct CutRamp { double g0, g1, d0, d1; };
        std::vector<CutRamp> cuts;
        for (size_t i = 0; i + 1 < timeMap->guideSec.size(); ++i)
        {
            const double dg = timeMap->guideSec[i + 1] - timeMap->guideSec[i];
            const double dd = timeMap->dubSec[i + 1]   - timeMap->dubSec[i];
            // <= 2 ms guide window with a backward or > 50 ms forward dub jump
            // cannot be a legal stretch (ratio rails cap at 3:1) -> detach ramp.
            if (dg <= 2.0e-3 && (dd < -1.0e-9 || dd > 0.05))
                cuts.push_back ({ timeMap->guideSec[i], timeMap->guideSec[i + 1],
                                  timeMap->dubSec[i],   timeMap->dubSec[i + 1] });
        }
        auto snapAcrossCut = [&cuts] (double g, double& dub) noexcept
        {
            for (const auto& c : cuts)
                if (g > c.g0 && g < c.g1)
                {
                    dub = (g - c.g0 <= c.g1 - g) ? c.d0 : c.d1;
                    return;
                }
        };

        std::vector<double> srcPos  ((size_t) outLen);
        std::vector<float>  ratioEd ((size_t) outLen), formEd ((size_t) outLen), gainEd ((size_t) outLen);
        for (int j = 0; j < outLen; ++j)
        {
            double dubSec, slope; float semis;
            const double gSec = (double) j / sr;
            timeMap->lookupAtGuideSec (gSec, dubSec, slope, semis);
            if (! cuts.empty())
                snapAcrossCut (gSec, dubSec);
            const double si = dubSec * sr - (double) spanStart;  // span-relative source index
            srcPos [(size_t) j] = si;
            ratioEd[(size_t) j] = lerpClamp (ratio, si);
            formEd [(size_t) j] = std::pow (2.0f, (lerpClamp (formSemis, si) + throat) / 12.0f);
            gainEd [(size_t) j] = lerpClamp (gain, si);
        }

        std::vector<float> shifted ((size_t) outLen, 0.0f);
        if (shifter != nullptr)
            shifter->bakeWarped (mono + spanStart, spanLen, shifted.data(), outLen, sr,
                                 srcPos.data(), ratioEd.data(), formEd.data());
        else
            for (int j = 0; j < outLen; ++j)   // no engine compiled: timing only, no pitch
                shifted[(size_t) j] = mono[spanStart + juce::jlimit (0, spanLen - 1,
                                                                     (int) std::llround (srcPos[(size_t) j]))];

        for (int j = 0; j < outLen; ++j) shifted[(size_t) j] *= gainEd[(size_t) j];
        out.setSize (1, outLen);
        out.copyFrom (0, 0, shifted.data(), outLen);
        return out;
    }

    // No time edit: length-preserving pitch bake on the raw take.
    std::vector<float> formScale ((size_t) spanLen);
    for (int j = 0; j < spanLen; ++j)
        formScale[(size_t) j] = std::pow (2.0f, (formSemis[(size_t) j] + throat) / 12.0f);

    std::vector<float> shifted ((size_t) spanLen, 0.0f);
    if (shifter != nullptr)
        shifter->bake (mono + spanStart, shifted.data(), spanLen, sr, ratio.data(), formScale.data());
    else
        std::copy (mono + spanStart, mono + spanStart + spanLen, shifted.begin());

    for (int j = 0; j < spanLen; ++j) shifted[(size_t) j] *= gain[(size_t) j];

    out.setSize (1, spanLen);
    out.copyFrom (0, 0, shifted.data(), spanLen);
    return out;
}

// ─── Playback: read the pre-baked cache (audio thread, RT-safe) ───────────────
// Replaces the strip audio with the background-baked pitched composite.  The
// cache is now baked on the EDITED (performance) timeline -- the pitch tab's own
// time-move is baked in, so it is read at the ALIGN-ONLY position (srcX0/srcRate
// carry only the align warp when a map is applied; a plain linear timeline read
// when there is no align).  Lock-free: one atomic load + array reads, no PSOLA,
// no per-sample DSP.  Outside the composite span the buffer is left dry, and with
// no cache yet (no edits, or a bake in flight before the first publish) playback
// is dry -- so an analyzed-but-untouched take plays through unchanged.
void BaySickPitchDSP::processFilePlay (juce::AudioBuffer<float>& buffer,
                                       juce::int64 timelineStartSample,
                                       double srcX0, double srcRatePerSample,
                                       bool srcValid) noexcept
{
    if (! mChainOn.load (std::memory_order_relaxed))
        return;   // chain OFF -> pass the dry strip audio through

    auto* cur = mCacheActive.load (std::memory_order_acquire);
    // Cache-handoff crossfade: a fresh bake is a NEW pointer, so cur != the last
    // one adopted means the worker just published -> fade old->new over ~30 ms so
    // the swap can't step the waveform (a click after an edit during playback).
    // Lock-free; the retiring old cache stays alive through the short fade.
    if (cur != mCachePlaying)
    {
        if (mCachePlaying != nullptr && ! mCachePlaying->audio.empty()
            && cur != nullptr && ! cur->audio.empty() && mCacheFadeLen > 0)
        {
            mCacheFading     = mCachePlaying;
            mCacheFadeRemain = mCacheFadeLen;
            mCacheHazardFading.store (mCacheFading, std::memory_order_release);   // pin through the fade
        }
        mCachePlaying = cur;
        // Keep the hazard == mCachePlaying at all times so the worker never frees
        // the cache we deref/latch on the NEXT swap check (survives a paused fade).
        mCacheHazardPlaying.store (cur, std::memory_order_release);
    }

    if (cur == nullptr || cur->audio.empty())
    {
        mCacheFadeRemain = 0; mCacheFading = nullptr;
        mCacheHazardFading.store (nullptr, std::memory_order_release);
        return;   // no bake -> dry
    }

    const std::vector<float>& a = cur->audio;
    const int    N  = (int) a.size();
    const double cr = cur->sampleRate;                          // cache (analysis) rate
    const double sr = mSampleRate;                              // device rate
    const double startSec = (double) cur->startSample / cr;     // composite timeline origin

    const bool haveFade = (mCacheFadeRemain > 0 && mCacheFading != nullptr
                           && ! mCacheFading->audio.empty());
    const std::vector<float>* fadeA = haveFade ? &mCacheFading->audio : nullptr;
    const int    fN  = fadeA ? (int) fadeA->size() : 0;
    const double fcr = haveFade ? mCacheFading->sampleRate : cr;
    const double fStartSec = haveFade ? (double) mCacheFading->startSample / fcr : startSec;

    auto readAt = [] (const std::vector<float>& v, double idx, int n) noexcept -> float
    {
        const int   i0 = (int) idx;
        const float fr = (float) (idx - (double) i0);
        return (i0 + 1 < n) ? v[(size_t) i0] + (v[(size_t) (i0 + 1)] - v[(size_t) i0]) * fr
                            : v[(size_t) i0];
    };

    const int numCh = buffer.getNumChannels();
    const int nSamp = buffer.getNumSamples();
    for (int i = 0; i < nSamp; ++i)
    {
        const double srcPos = srcValid ? (srcX0 + (double) i * srcRatePerSample)
                                       : (double) (timelineStartSample + i);
        // Advance the fade per output sample (wall-clock), before any early-out.
        float fadeT = 1.0f;
        if (mCacheFadeRemain > 0)
        {
            fadeT = 1.0f - (float) mCacheFadeRemain / (float) mCacheFadeLen;   // 0 -> 1
            --mCacheFadeRemain;
        }

        const double idx = (srcPos / sr - startSec) * cr;
        if (idx < 0.0 || idx > (double) (N - 1)) continue;      // outside composite -> leave dry
        float v = readAt (a, idx, N);

        if (fadeA != nullptr && fadeT < 1.0f)
        {
            const double idxO = (srcPos / sr - fStartSec) * fcr;
            if (idxO >= 0.0 && idxO <= (double) (fN - 1))
            {
                const float vOld = readAt (*fadeA, idxO, fN);
                v = vOld + (v - vOld) * fadeT;   // old -> new across the fade
            }
        }

        // Composite (and thus the cache) is mono -- write it to every channel.
        for (int ch = 0; ch < numCh; ++ch)
            buffer.setSample (ch, i, v);
    }

    if (mCacheFadeRemain <= 0)
    {
        mCacheFading = nullptr;   // fade done -> release the faded-out cache ref
        mCacheHazardFading.store (nullptr, std::memory_order_release);
    }
}

void BaySickPitchDSP::processFilePlayMonitor (juce::AudioBuffer<float>& buffer,
                                              juce::int64 timelineStartSample,
                                              double srcX0, double srcRatePerSample,
                                              bool srcValid) noexcept
{
    // QA-Fe: with the pre-baked cache there is no separate monitor DSP state --
    // both streams read the same published cache at their own source position.
    processFilePlay (buffer, timelineStartSample, srcX0, srcRatePerSample, srcValid);
}

// ─── Offline render (Render/Freeze bake + span preview) ───────────────────────
juce::AudioBuffer<float> BaySickPitchDSP::renderOffline (const float* mono, int numSamples,
                                                         double sampleRate,
                                                         juce::int64 spanStart,
                                                         int spanLen) const
{
    // Render/Freeze bake: build a composite-relative edit snapshot from the live
    // message-thread state and run the shared bake core.  Passes the time map so
    // the export WARPS-THEN-BAKES exactly like playback (bakeToCache) -- the old
    // pitch-then-post-hoc-warp order in renderPitchedTake time-stretched already-
    // pitched audio and destroyed phase coherence (engine-independent garble).
    Snapshot snap;
    snap.regions     = mRegions;
    snap.sampleRate  = sampleRate;
    snap.startSample = 0;   // composite-relative render
    for (const auto& r : mRegions)
    {
        // Match requestBake's neutral gate: a pure time move (hasTimeEdit, no
        // pitch/vol edit) must still bake in the EXPORT too, or renders drop the
        // move entirely (bakeSpan's neutral check returns the dry input).
        if (r.hasEdits() || r.hasTimeEdit())   snap.anyEdits   = true;
        if (std::abs (r.formantSemis) > 0.01f) snap.anyFormant = true;
    }
    auto tm = buildTimeMapSnapshot();   // nullptr when no time edit
    return bakeSpan (mono, numSamples, sampleRate, snap,
                     (PitchEngine) mEngine.load (std::memory_order_relaxed),
                     mFocus01.load (std::memory_order_relaxed),
                     mModAmt.load  (std::memory_order_relaxed),
                     mSpeedMs.load (std::memory_order_relaxed),
                     mThroat.load  (std::memory_order_relaxed),
                     mSnapOn.load  (std::memory_order_relaxed),
                     mRootPc.load  (std::memory_order_relaxed),
                     mScaleIdx.load(std::memory_order_relaxed),
                     spanStart, spanLen, tm.get());
}

// THREADING: called on the MESSAGE thread (scrub preview).  mCacheActive holds an
// immutable published snapshot kept alive by the worker's 8-deep retire ring, so a
// fast load+copy here can never observe a freed snapshot (the worker needs 8 more
// bakes -- seconds -- to erase it; the copy is sub-ms).  No render, no lock.
juce::AudioBuffer<float> BaySickPitchDSP::copyCacheSpan (double startEditedSec,
                                                         double endEditedSec) const
{
    juce::AudioBuffer<float> out;
    auto* cache = mCacheActive.load (std::memory_order_acquire);
    if (cache == nullptr || cache->audio.empty() || endEditedSec <= startEditedSec)
        return out;   // no bake / empty span -> caller auditions the dry source
    const double cr = cache->sampleRate;
    const int    N  = (int) cache->audio.size();
    // Cache is the pitched EDITED-timeline composite (audio[0] = edited t0), so an
    // edited-composite second maps to sample sec*cr.
    const int i0 = juce::jlimit (0, N, (int) std::llround (startEditedSec * cr));
    const int i1 = juce::jlimit (0, N, (int) std::llround (endEditedSec   * cr));
    if (i1 - i0 <= 0) return out;
    out.setSize (1, i1 - i0);
    out.copyFrom (0, 0, cache->audio.data() + i0, i1 - i0);
    return out;
}

// ─── Persistence ──────────────────────────────────────────────────────────────
juce::ValueTree BaySickPitchDSP::stateToValueTree() const
{
    juce::ValueTree v ("PitchState");
    v.setProperty ("analyzed",  mAnalyzed ? 1 : 0, nullptr);
    v.setProperty ("startBeat", mStartBeat,        nullptr);
    v.setProperty ("startSamp", mStartSample,      nullptr);
    v.setProperty ("sr",        mAnalysisSr,       nullptr);
    v.setProperty ("compSec",   mCompositeSec,     nullptr);
    for (const auto& r : mRegions)
        v.appendChild (r.toValueTree(), nullptr);
    return v;
}

void BaySickPitchDSP::stateFromValueTree (const juce::ValueTree& v)
{
    mRegions.clear();
    mF0Track.clear();
    if (! v.hasType ("PitchState"))
    {
        mAnalyzed = false;
        publishEdits();
        return;
    }
    mAnalyzed     = ((int) v.getProperty ("analyzed", 0)) != 0;
    mStartBeat    = (double) v.getProperty ("startBeat", 0.0);
    mStartSample  = (juce::int64) v.getProperty ("startSamp", 0);
    mAnalysisSr   = (double) v.getProperty ("sr", 44100.0);
    mCompositeSec = (double) v.getProperty ("compSec", 0.0);
    for (int i = 0; i < v.getNumChildren(); ++i)
        if (v.getChild (i).hasType ("N"))
            mRegions.push_back (PitchNoteRegion::fromValueTree (v.getChild (i)));

    // QA-Fe migration: old projects saved auto-detected slice pills (the bottom-
    // lane consonant/breath regions).  The model retired them -- a stationary
    // slice beside a moved note IS the old time-map garble.  Drop the ones the
    // user never touched (no work to lose; re-analysis re-folds the audio into
    // the notes), but keep any slice that was volume-shaped or moved as an inline
    // unpitched piece so no user work is lost.
    mRegions.erase (std::remove_if (mRegions.begin(), mRegions.end(),
        [] (const PitchNoteRegion& r)
        { return r.isSlice && ! r.hasEdits() && ! r.hasTimeEdit(); }),
        mRegions.end());

    publishEdits();
}
