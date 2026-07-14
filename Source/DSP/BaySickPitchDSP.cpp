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
    // silently discarded any more: onset-glide fragments merge forward into
    // the note they lead into (the old sub-60 ms discard ate whole first
    // notes on fast rising glides); voiced leftovers and energetic unvoiced
    // spans become slice pills.
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
    // Slice gate: unvoiced frames louder than 2% of the composite's peak
    // frame RMS (and above an absolute floor) become slice pills; quieter
    // frames are silence.
    constexpr float  kSliceRelGate = 0.02f;
    constexpr float  kSliceAbsGate = 1.0e-4f;

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
}

BaySickPitchDSP::BaySickPitchDSP() = default;

BaySickPitchDSP::~BaySickPitchDSP()
{
    // Owner (BaySickVocalProcessor) raises its shutdown gate before member
    // teardown, so the audio thread cannot be inside processFilePlay here.
    delete mActive.exchange (nullptr);
    delete mTimeMapActive.exchange (nullptr);
}

void BaySickPitchDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
    for (auto& sh : mShifters) sh.prepare (mSampleRate, maxBlockSize);
    for (auto& fe : mFormant)  fe.prepare (mSampleRate, maxBlockSize);
    mAppState = {};
    for (auto& sh : mMonShifters) sh.prepare (mSampleRate, maxBlockSize);
    for (auto& fe : mMonFormant)  fe.prepare (mSampleRate, maxBlockSize);
    mMonState = {};
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
    //    survives as its own region (slice pill if under minimum).
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

    // ── Stage 3: materialize regions.  Runs at/over the minimum are pitch
    //    pills (stats from the anchor tail); shorter leftovers become slice
    //    pills -- pitched material never silently drops (locked 6).
    for (const auto& run : runs)
    {
        const double t0 = run.f0 * hopSec;
        const double t1 = juce::jmin (run.f1 * hopSec, mCompositeSec);
        if (t1 - t0 <= 1.0e-6) continue;

        PitchNoteRegion r;
        r.startSec = t0;
        r.endSec   = t1;

        if (t1 - t0 >= kMinNoteSec && ! run.midi.empty())
        {
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
        }
        else
        {
            r.isSlice = true;
        }
        mRegions.push_back (r);
    }

    // ── Stage 4: energetic unvoiced spans -> slice pills.  Frame RMS over
    //    the same hop grid; spans above the gate that no region covers
    //    become time-editable slices (consonants, breaths, percussive
    //    material); quieter frames are silence and stay pill-free.
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
        for (const auto& r : mRegions)
        {
            const int a = juce::jlimit (0, numFrames, (int) (r.startSec / hopSec));
            const int z = juce::jlimit (0, numFrames, (int) std::ceil (r.endSec / hopSec));
            for (int f = a; f < z; ++f) covered[(size_t) f] = true;
        }

        int spanStart = -1;
        auto flushSpan = [&] (int endExclusive)
        {
            if (spanStart < 0) return;
            PitchNoteRegion r;
            r.isSlice  = true;
            r.startSec = spanStart * hopSec;
            r.endSec   = juce::jmin (endExclusive * hopSec, mCompositeSec);
            if (r.endSec - r.startSec > 1.0e-6)
                mRegions.push_back (r);
            spanStart = -1;
        };
        for (int f = 0; f < numFrames; ++f)
        {
            const bool sliceable = ! covered[(size_t) f]
                                && mF0Track[(size_t) f] <= 0.0f
                                && frameRms[(size_t) f] > gate;
            if (sliceable) { if (spanStart < 0) spanStart = f; }
            else             flushSpan (f);
        }
        flushSpan (numFrames);
    }

    std::sort (mRegions.begin(), mRegions.end(),
               [] (const PitchNoteRegion& a, const PitchNoteRegion& b)
               { return a.startSec < b.startSec; });

    // Carry user edits across re-analysis for regions that still line up
    // (same kind only -- a note-to-slice reclassification drops the pitch
    // edits with the pitch identity).  The QA-Fd edit set carries too:
    // popup curves, Variation, and the time edits (FA-9's carry contract --
    // a grid nudge must not silently delete the user's moves).
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

    mAnalyzed = true;
    publishEdits();
}

// ─── Snapshot publish ─────────────────────────────────────────────────────────
void BaySickPitchDSP::publishEdits()
{
    auto snap = std::make_unique<Snapshot>();
    snap->regions     = mRegions;
    snap->sampleRate  = mAnalysisSr;
    snap->startSample = mStartSample;
    snap->f0Track     = mF0Track;
    for (const auto& r : mRegions)
    {
        if (r.hasEdits())                        snap->anyEdits   = true;
        if (std::abs (r.formantSemis) > 0.01f)   snap->anyFormant = true;
    }

    auto* old = mActive.exchange (snap.release(), std::memory_order_acq_rel);
    if (old != nullptr)
        mRetired.push_back (std::unique_ptr<Snapshot> (old));
    // Retain the last 8 retired snapshots: the audio thread only holds a
    // snapshot pointer within one block, so anything 8 publishes old is
    // unreachable by construction.
    while (mRetired.size() > 8)
        mRetired.erase (mRetired.begin());

    publishTimeMap();
}

// ─── QA-Fd time-edit engine ───────────────────────────────────────────────────
// Build the EDITED->SOURCE channel map from the regions' dst spans.  Anchor
// set: identity pin at 0, per-region (dstStart->srcStart)+(dstEnd->srcEnd)
// pairs (gaps between regions map linearly = the gap counter-warp), and a
// slope-1 tail pin.  Detached pills may put a jump between consecutive
// anchors; the strict-monotone nudge below turns it into a steep ramp
// (~the lookup slope rail) and the decode layer treats big jumps as cuts.
void BaySickPitchDSP::publishTimeMap()
{
    std::unique_ptr<AlignPlaySnapshot> snap;

    bool anyTime = false;
    for (const auto& r : mRegions)
        if (r.hasTimeEdit()) { anyTime = true; break; }

    if (mAnalyzed && anyTime)
    {
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

        snap = std::make_unique<AlignPlaySnapshot>();
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
    }

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
        mFocus01.store (v, std::memory_order_relaxed);
}

void BaySickPitchDSP::setModAmount (float v)
{
    v = juce::jlimit (0.0f, 2.0f, v);
    if (v != mModAmt.load (std::memory_order_relaxed))
        mModAmt.store (v, std::memory_order_relaxed);
}

void BaySickPitchDSP::setSpeedMs (float ms)
{
    ms = juce::jlimit (5.0f, 300.0f, ms);
    if (ms != mSpeedMs.load (std::memory_order_relaxed))
        mSpeedMs.store (ms, std::memory_order_relaxed);
}

void BaySickPitchDSP::setChainOn (bool on)
{
    if (on != mChainOn.load (std::memory_order_relaxed))
        mChainOn.store (on, std::memory_order_relaxed);
}

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

// ─── Shared per-sample applicator ─────────────────────────────────────────────
void BaySickPitchDSP::applyEditsToBuffer (float* const* chans, int numCh, int numSamples,
                                          juce::int64 timelineStartSample, double sr,
                                          const Snapshot& snap,
                                          float focus01, float modAmt, float speedMs,
                                          bool chainOn,
                                          PsolaShifter* shifters,
                                          CepstralFormantEngine* formants,
                                          ApplicatorState& st,
                                          double srcX0, double srcRatePerSample,
                                          bool srcValid) const noexcept
{
    const auto& regions = snap.regions;
    if (regions.empty() || numCh <= 0) return;

    st.diagInRegion = 0;   // [PITCH DIAG]
    st.diagChanged  = 0;   // [PITCH DIAG]

    const float smoothCoef = 1.0f - std::exp (-1.0f / (float) (speedMs * 0.001 * sr));
    const float gainCoef   = 1.0f - std::exp (-1.0f / (float) (0.005 * sr));
    // ~4 ms one-pole that de-steps the per-frame median period (below) into a
    // smooth per-sample value -- fast enough to still track vibrato.
    const float periodSlewCoef = 1.0f - std::exp (-1.0f / (float) (0.004 * sr));

    // QA-Fd 14b: the Focus target set (nearest semitone vs the Root/Scale
    // pick).  Relaxed loads once per call -- same thread-contract as the
    // knob params.
    const bool snapScale = mSnapOn.load (std::memory_order_relaxed);
    const int  rootPc    = mRootPc.load (std::memory_order_relaxed);
    const int  scaleIdx  = mScaleIdx.load (std::memory_order_relaxed);

    // snap.startSample is stamped in ANALYSIS-time frames; convert the origin
    // through the analysis rate so a device-rate switch after analysis doesn't
    // re-interpret it at the current rate (seconds are rate-invariant — the
    // align side re-derives its origin the same way).
    const double snapStartSec = (double) snap.startSample / snap.sampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        // QA-Fd source-domain stamps: when the decode layer warped this
        // channel (align and/or time map), resolve the pill at the SOURCE
        // position the audible audio actually came from -- linear timeline
        // resolution applied the wrong pill at any real warp offset (the
        // wrong-syllable hole found in planning).
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
        if (inRegion) ++st.diagInRegion;                    // [PITCH DIAG]
        if (i == numSamples - 1) st.diagLastTSec = tSec;    // [PITCH DIAG]

        float targetSemis   = 0.0f;
        // Per-sample modulation rides POST-smoother: the Speed glide is a
        // ~150 ms one-pole, which low-passes a 5-6 Hz vibrato wiggle, the
        // Variation deviation term, and drawn pitch curves to near-nothing
        // if they route through it.  Only the note CENTER glides.
        float fastSemis     = 0.0f;
        float targetFormant = 0.0f;
        float gain          = 1.0f;

        // Chain OFF keeps the loop running with neutral targets so the
        // smoothing glides everything home; the caller's fast path bails
        // once settled.
        if (inRegion && chainOn)
        {
            const auto& r = regions[(size_t) st.cursor];
            // Slice pills (QA-Fd, locked 6) carry no pitch identity: volume
            // shape applies, pitch/focus/vibrato targets stay neutral (the
            // Speed smoothing glides any prior shift home), and the PSOLA
            // period keeps its last real value.
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
                // Grain-geometry period (2026-07-12).  The grain half-window +
                // hop (hw/pOut) must track the LIVE source period so the Hann
                // zeros land on the neighbor glottal pulses (the Newtone beating
                // fix) -- BUT the raw F0 track octave-glitches at low-energy word
                // boundaries, and feeding those swings straight into the window
                // size yanked the scheduler into a coverage hole (the stutter).
                // So drive the geometry off a CENTERED MEDIAN of up to 5 F0
                // frames (rejects the octave-glitch, passes slow vibrato), and
                // if fewer than 3 frames in the window are voiced (a breath /
                // silence) FREEZE to the last known-good period.  The LPC epoch
                // (glottal-pulse) positions are unaffected -- they still land on
                // the residual peak; only the window/hop period is stabilized.
                {
                    if (! snap.f0Track.empty())
                    {
                        const double hopSec = (double) kF0Hop / snap.sampleRate;
                        const int    nF0    = (int) snap.f0Track.size();
                        const int    f0i    = juce::jlimit (0, nF0 - 1, (int) (tSec / hopSec));
                        if (f0i != st.f0MedFrame)   // recompute the median once per FRAME (RT-cheap)
                        {
                            st.f0MedFrame = f0i;
                            float win[5]; int nv = 0;   // voiced frames, insertion-sorted in place
                            for (int d = -2; d <= 2; ++d)
                            {
                                const int fi = f0i + d;
                                if (fi < 0 || fi >= nF0) continue;
                                const float v = snap.f0Track[(size_t) fi];
                                if (v <= 0.0f) continue;
                                int j = nv - 1;
                                while (j >= 0 && win[j] > v) { win[j + 1] = win[j]; --j; }
                                win[j + 1] = v; ++nv;
                            }
                            if (nv >= 3)   // enough voiced context -> trust the median
                                st.geomTargetPer = (float) (sr / juce::jmax (40.0f, win[nv / 2]));
                            // else: unvoiced / low-confidence -> leave geomTargetPer (freeze)
                        }
                    }
                    else
                        st.geomTargetPer = (float) (sr / juce::jmax (40.0f, r.f0Hz));

                    if (st.geomTargetPer <= 0.0f)   // first-use / still-unvoiced seed
                        st.geomTargetPer = (float) (sr / juce::jmax (40.0f, r.f0Hz));
                    if (st.geomPeriod <= 0.0f)
                        st.geomPeriod = st.geomTargetPer;
                    st.geomPeriod += (st.geomTargetPer - st.geomPeriod) * periodSlewCoef;

                    for (int ch = 0; ch < numCh; ++ch)
                        shifters[ch].setPeriodSamples (st.geomPeriod);
                }

                // Focus pulls the EDITED center (detected + drag) toward the
                // snap target: nearest semitone, or the Root/Scale set when
                // Snap is on (QA-Fd 8/14b).
                const float base   = r.midi + r.shiftSemis;
                const float target = snapScale
                    ? PitchCorrectorDSP::snapMidiToScaleStatic (base, rootPc, scaleIdx)
                    : std::round (base);
                targetSemis = r.shiftSemis + (target - base) * focus01;

                const double len01 = r.endSec - r.startSec;
                const float  t01   = (len01 > 1.0e-6)
                    ? (float) ((tSec - r.startSec) / len01) : 0.0f;

                // QA-Fd sub-edit system: additive per-pill pitch curve.
                if (! r.pitchShape.empty())
                    fastSemis += juce::jlimit (-24.0f, 24.0f, r.pitchSemisAt (t01));

                // QA-Fd 15a Variation: scale the note's own contour wiggle
                // around its center (deviation from the analysis F0 track;
                // unvoiced frames contribute nothing).
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

                // Vib knob is ADDITIVE synthesis on top of the natural
                // vibrato (mult 1 = nothing added); flattening the REAL
                // wiggle is the Variation knob's job (F0-track deviation
                // above).
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

        const float ratio = std::pow (2.0f, (st.smoothedSemis + fastSemis) / 12.0f);

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float dry = chans[ch][i];
            float wet = shifters[ch].processSample (dry, ratio);
            if (snap.anyFormant)
                wet = formants[ch].processSample (dry, wet, false, st.smoothedFormant);
            const float out = wet * st.smoothedGain;
            // [PITCH DIAG]
            if (ch == 0)
            {
                const float aIn = std::abs (dry), aOut = std::abs (out);
                if (aIn  > st.diagPeakIn)  st.diagPeakIn  = aIn;
                if (aOut > st.diagPeakOut) st.diagPeakOut = aOut;
                if (std::abs (out - dry) > 1.0e-4f) ++st.diagChanged;
            }
            chans[ch][i] = out;
        }
    }
}

// ─── Realtime applicator (audio thread) ───────────────────────────────────────
void BaySickPitchDSP::processFilePlay (juce::AudioBuffer<float>& buffer,
                                       juce::int64 timelineStartSample,
                                       double srcX0, double srcRatePerSample,
                                       bool srcValid) noexcept
{
    auto* snap = mActive.load (std::memory_order_acquire);
    // [PITCH DIAG] G2 boundary (Rule 4, Remove at close): relaxed counters,
    // negligible cost, no branches added to the hot loop itself.
    mDiag.blocks.fetch_add (1, std::memory_order_relaxed);
    if (snap == nullptr)
    {
        mDiag.snapNull.fetch_add (1, std::memory_order_relaxed);
        return;
    }
    mDiag.regionCount.store ((int) snap->regions.size(), std::memory_order_relaxed);

    const bool  on    = mChainOn.load (std::memory_order_relaxed);
    const float focus = mFocus01.load (std::memory_order_relaxed);
    const float mod   = mModAmt .load (std::memory_order_relaxed);
    // Fast path (lazy-activate): zero-edit channels at neutral knobs cost
    // one atomic load + two float compares per block.  Chain OFF bails only
    // once the glide has settled (no hard switch, rule 5).
    const bool settled = std::abs (mAppState.smoothedSemis)   < 0.002f
                      && std::abs (mAppState.smoothedFormant) < 0.002f
                      && std::abs (mAppState.smoothedGain - 1.0f) < 0.002f;
    if (! on && settled)
    {
        mDiag.bailOff.fetch_add (1, std::memory_order_relaxed);
        return;
    }
    if (on && settled
        && ! snap->anyEdits && focus < 0.001f && std::abs (mod - 1.0f) < 0.01f)
    {
        mDiag.bailNeutral.fetch_add (1, std::memory_order_relaxed);
        return;
    }

    const int numCh = juce::jmin (2, buffer.getNumChannels());
    applyEditsToBuffer (buffer.getArrayOfWritePointers(), numCh,
                        buffer.getNumSamples(), timelineStartSample,
                        mSampleRate, *snap,
                        focus, mod, mSpeedMs.load (std::memory_order_relaxed),
                        on,
                        mShifters.data(), mFormant.data(), mAppState,
                        srcX0, srcRatePerSample, srcValid);
    mDiag.applied.fetch_add (1, std::memory_order_relaxed);
    mDiag.inRegion.store (mAppState.diagInRegion, std::memory_order_relaxed);
    mDiag.lastTSec.store ((float) mAppState.diagLastTSec, std::memory_order_relaxed);
    mDiag.changed.store (mAppState.diagChanged, std::memory_order_relaxed);
    if (mAppState.diagPeakIn > mDiag.peakIn.load (std::memory_order_relaxed))
        mDiag.peakIn.store (mAppState.diagPeakIn, std::memory_order_relaxed);
    if (mAppState.diagPeakOut > mDiag.peakOut.load (std::memory_order_relaxed))
        mDiag.peakOut.store (mAppState.diagPeakOut, std::memory_order_relaxed);
    const float curAbs = std::abs (mAppState.smoothedSemis);
    if (curAbs > mDiag.maxSemis.load (std::memory_order_relaxed))
        mDiag.maxSemis.store (curAbs, std::memory_order_relaxed);
    // [PITCH DIAG] fold in the shifter's per-block silence-gap counters (ch 0),
    // then clear them for the next block.
    mDiag.floorHits.fetch_add ((juce::int64) mShifters[0].diagFloorHits(),
                               std::memory_order_relaxed);
    // grainMin = THIS block's fewest concurrent grains (not sticky -- warmup
    // would otherwise pin it low forever).  0 while playing = a real gap.
    mDiag.grainMin.store (mShifters[0].diagGrainMin(), std::memory_order_relaxed);
    mDiag.shPeriod.store (mShifters[0].diagPeriod(), std::memory_order_relaxed);
    mShifters[0].diagClear();
}

// QA-Fb (A1 monitor merge): processFilePlay's twin over the monitor stream
// state.  Same snapshot + knob loads + fast-path gates; the two streams share
// nothing mutable, so the live take's corrector and the prior takes' edits
// advance independently within one block.
void BaySickPitchDSP::processFilePlayMonitor (juce::AudioBuffer<float>& buffer,
                                              juce::int64 timelineStartSample,
                                              double srcX0, double srcRatePerSample,
                                              bool srcValid) noexcept
{
    auto* snap = mActive.load (std::memory_order_acquire);
    if (snap == nullptr) return;

    const bool  on    = mChainOn.load (std::memory_order_relaxed);
    const float focus = mFocus01.load (std::memory_order_relaxed);
    const float mod   = mModAmt .load (std::memory_order_relaxed);
    const bool settled = std::abs (mMonState.smoothedSemis)   < 0.002f
                      && std::abs (mMonState.smoothedFormant) < 0.002f
                      && std::abs (mMonState.smoothedGain - 1.0f) < 0.002f;
    if (! on && settled)
        return;
    if (on && settled
        && ! snap->anyEdits && focus < 0.001f && std::abs (mod - 1.0f) < 0.01f)
        return;

    const int numCh = juce::jmin (2, buffer.getNumChannels());
    applyEditsToBuffer (buffer.getArrayOfWritePointers(), numCh,
                        buffer.getNumSamples(), timelineStartSample,
                        mSampleRate, *snap,
                        focus, mod, mSpeedMs.load (std::memory_order_relaxed),
                        on,
                        mMonShifters.data(), mMonFormant.data(), mMonState,
                        srcX0, srcRatePerSample, srcValid);
}

// ─── Offline render (Render/Freeze bake + span preview) ───────────────────────
juce::AudioBuffer<float> BaySickPitchDSP::renderOffline (const float* mono, int numSamples,
                                                         double sampleRate,
                                                         juce::int64 spanStart,
                                                         int spanLen) const
{
    juce::AudioBuffer<float> out;
    if (mono == nullptr || numSamples <= 0 || sampleRate <= 0.0)
        return out;

    spanStart = juce::jlimit ((juce::int64) 0, (juce::int64) numSamples, spanStart);
    if (spanLen < 0) spanLen = numSamples - (int) spanStart;
    spanLen = juce::jmin (spanLen, numSamples - (int) spanStart);
    if (spanLen <= 0) return out;

    out.setSize (1, spanLen);
    out.copyFrom (0, 0, mono + spanStart, spanLen);

    Snapshot snap;
    snap.regions     = mRegions;
    snap.sampleRate  = sampleRate;
    snap.startSample = 0;   // composite-relative render
    for (const auto& r : mRegions)
    {
        if (r.hasEdits())                      snap.anyEdits   = true;
        if (std::abs (r.formantSemis) > 0.01f) snap.anyFormant = true;
    }

    const float focus = mFocus01.load (std::memory_order_relaxed);
    const float mod   = mModAmt .load (std::memory_order_relaxed);
    const float speed = mSpeedMs.load (std::memory_order_relaxed);
    if (! snap.anyEdits && focus < 0.001f && std::abs (mod - 1.0f) < 0.01f)
        return out;   // nothing to bake -- identical copy

    PsolaShifter          shifter;
    CepstralFormantEngine formant;
    shifter.prepare (sampleRate, 512);
    formant.prepare (sampleRate, 512);
    ApplicatorState st;

    // Streaming pass with latency-compensating lead-in: PSOLA ~2 periods
    // plus the formant engine when engaged (both consume real audio first,
    // then their delayed output replaces from sample 0).  Span renders read
    // real pre-span audio for the lead-in (clamped at the composite edge).
    const int lat = 600 + (snap.anyFormant
                            ? 2 * CepstralFormantEngine::kSize - CepstralFormantEngine::kHop
                            : 0);
    std::vector<float> shifted ((size_t) spanLen, 0.0f);
    std::vector<float> feed (1, 0.0f);

    for (int j = -lat; j < spanLen; ++j)
    {
        const int src = juce::jlimit (0, numSamples - 1, (int) spanStart + j + lat);
        feed[0] = mono[src];
        float* chans[1] = { feed.data() };
        // Render bakes the edited state regardless of the bsp_on chain
        // switch (export = the edits, not the monitor state).
        applyEditsToBuffer (chans, 1, 1, (juce::int64) src, sampleRate, snap,
                            focus, mod, speed, true, &shifter, &formant, st);
        if (j >= 0)
            shifted[(size_t) j] = feed[0];
    }
    out.copyFrom (0, 0, shifted.data(), spanLen);
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
    publishEdits();
}
