#include "BaySickPitchDSP.h"
#include "BaySickAlignDSP.h"   // estimateF0Track (shared frame-YIN)
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPitchDSP - QA-Fa Task 1 (2026-07-10).  See header.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    // Segmentation calibration: a note ends after ~2 unvoiced frames
    // (~93 ms at 44.1k / 2048 hop) or a sustained pitch jump; regions
    // shorter than 60 ms are discarded as consonant noise.
    constexpr int    kGapFrames        = 2;
    constexpr float  kSplitSemis       = 0.6f;
    constexpr int    kSplitHoldFrames  = 2;
    constexpr double kMinNoteSec       = 0.06;
    constexpr double kEditCarryTolSec  = 0.05;

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

    // Voiced grouping with gap + sustained-jump splits.
    std::vector<float> midiFrames;
    int runStart = -1, gapCount = 0, jumpCount = 0;

    auto flushRun = [&] (int endFrameExclusive)
    {
        if (runStart < 0) return;
        const double t0 = runStart * hopSec;
        const double t1 = endFrameExclusive * hopSec;
        if (t1 - t0 >= kMinNoteSec && ! midiFrames.empty())
        {
            PitchNoteRegion r;
            r.startSec = t0;
            r.endSec   = juce::jmin (t1, mCompositeSec);
            r.midi     = medianOf (midiFrames);
            r.f0Hz     = 440.0f * std::pow (2.0f, (r.midi - 69.0f) / 12.0f);

            // Vibrato: deviation from the median.  Depth = sqrt(2)*RMS in
            // cents (sine assumption); rate from deviation zero crossings.
            // Under 15 cents = no meaningful vibrato.
            double rms = 0.0;
            int zc = 0;
            for (size_t i = 0; i < midiFrames.size(); ++i)
            {
                const float dev = midiFrames[i] - r.midi;
                rms += (double) dev * dev;
                if (i > 0 && ((midiFrames[i - 1] - r.midi) < 0.0f) != (dev < 0.0f))
                    ++zc;
            }
            rms = std::sqrt (rms / (double) midiFrames.size());
            const float depthCents = juce::jlimit (0.0f, 300.0f,
                (float) (rms * std::sqrt (2.0) * 100.0));
            if (depthCents >= 15.0f)
            {
                r.vibDepthCents = depthCents;
                r.vibRateHz     = juce::jlimit (3.0f, 9.0f,
                    (float) (zc / (2.0 * juce::jmax (0.05, t1 - t0))));
            }
            mRegions.push_back (r);
        }
        runStart = -1;
        midiFrames.clear();
        gapCount = jumpCount = 0;
    };

    for (int f = 0; f < (int) mF0Track.size(); ++f)
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
    flushRun ((int) mF0Track.size());

    // Carry user edits across re-analysis for regions that still line up.
    for (auto& r : mRegions)
        for (const auto& old : oldRegions)
            if (std::abs (old.startSec - r.startSec) < kEditCarryTolSec)
            {
                r.shiftSemis   = old.shiftSemis;
                r.formantSemis = old.formantSemis;
                r.vibDepthMult = old.vibDepthMult;
                r.vibRateMult  = old.vibRateMult;
                r.volShape     = old.volShape;
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
}

void BaySickPitchDSP::clearAllEdits()
{
    for (auto& r : mRegions)
    {
        r.shiftSemis   = 0.0f;
        r.formantSemis = 0.0f;
        r.vibDepthMult = 1.0f;
        r.vibRateMult  = 1.0f;
        r.volShape.clear();
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

// ─── Shared per-sample applicator ─────────────────────────────────────────────
void BaySickPitchDSP::applyEditsToBuffer (float* const* chans, int numCh, int numSamples,
                                          juce::int64 timelineStartSample, double sr,
                                          const Snapshot& snap,
                                          float focus01, float modAmt, float speedMs,
                                          bool chainOn,
                                          PsolaShifter* shifters,
                                          CepstralFormantEngine* formants,
                                          ApplicatorState& st) const noexcept
{
    const auto& regions = snap.regions;
    if (regions.empty() || numCh <= 0) return;

    st.diagInRegion = 0;   // [PITCH DIAG]
    st.diagChanged  = 0;   // [PITCH DIAG]

    const float smoothCoef = 1.0f - std::exp (-1.0f / (float) (speedMs * 0.001 * sr));
    const float gainCoef   = 1.0f - std::exp (-1.0f / (float) (0.005 * sr));

    // snap.startSample is stamped in ANALYSIS-time frames; convert the origin
    // through the analysis rate so a device-rate switch after analysis doesn't
    // re-interpret it at the current rate (seconds are rate-invariant — the
    // align side re-derives its origin the same way).
    const double snapStartSec = (double) snap.startSample / snap.sampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        const double tSec = (double) (timelineStartSample + i) / sr - snapStartSec;

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
        float targetFormant = 0.0f;
        float gain          = 1.0f;

        // Chain OFF keeps the loop running with neutral targets so the
        // smoothing glides everything home; the caller's fast path bails
        // once settled.
        if (inRegion && chainOn)
        {
            const auto& r = regions[(size_t) st.cursor];
            if (st.cursor != st.lastRegion)
            {
                st.lastRegion = st.cursor;
                st.vibPhase   = 0.0;
                const float period = (float) (sr / juce::jmax (40.0f, r.f0Hz));
                for (int ch = 0; ch < numCh; ++ch)
                    shifters[ch].setPeriodSamples (period);
            }

            // Focus pulls the note's detected center to the nearest semitone;
            // the pill drag adds on top.
            targetSemis = r.shiftSemis
                        + (std::round (r.midi) - r.midi) * focus01;

            // Vibrato sub-curve is ADDITIVE synthesis on top of the natural
            // vibrato (mult 1 = nothing added; flattening real vibrato needs
            // phase tracking we don't do -- Focus/retune covers that use).
            const float addMult = (r.vibDepthMult * modAmt) - 1.0f;
            if (std::abs (addMult) > 0.01f)
            {
                const float baseDepth = (r.vibDepthCents > 0.0f) ? r.vibDepthCents : 25.0f;
                const float rate      = r.vibRateHz * r.vibRateMult;
                st.vibPhase += juce::MathConstants<double>::twoPi * rate / sr;
                if (st.vibPhase > juce::MathConstants<double>::twoPi)
                    st.vibPhase -= juce::MathConstants<double>::twoPi;
                targetSemis += (float) std::sin (st.vibPhase)
                             * juce::jlimit (-300.0f, 300.0f, baseDepth * addMult) / 100.0f;
            }

            targetFormant = r.formantSemis;

            const double len = r.endSec - r.startSec;
            if (len > 1.0e-6)
                gain = r.volGainAt ((float) ((tSec - r.startSec) / len));
        }

        st.smoothedSemis   += (targetSemis   - st.smoothedSemis)   * smoothCoef;
        st.smoothedFormant += (targetFormant - st.smoothedFormant) * smoothCoef;
        st.smoothedGain    += (gain          - st.smoothedGain)    * gainCoef;

        const float ratio = std::pow (2.0f, st.smoothedSemis / 12.0f);

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
                                       juce::int64 timelineStartSample) noexcept
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
                        mShifters.data(), mFormant.data(), mAppState);
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
}

// QA-Fb (A1 monitor merge): processFilePlay's twin over the monitor stream
// state.  Same snapshot + knob loads + fast-path gates; the two streams share
// nothing mutable, so the live take's corrector and the prior takes' edits
// advance independently within one block.
void BaySickPitchDSP::processFilePlayMonitor (juce::AudioBuffer<float>& buffer,
                                              juce::int64 timelineStartSample) noexcept
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
                        mMonShifters.data(), mMonFormant.data(), mMonState);
}

// ─── Offline render (Render/Freeze bake) ──────────────────────────────────────
juce::AudioBuffer<float> BaySickPitchDSP::renderOffline (const float* mono, int numSamples,
                                                         double sampleRate) const
{
    juce::AudioBuffer<float> out;
    if (mono == nullptr || numSamples <= 0 || sampleRate <= 0.0)
        return out;

    out.setSize (1, numSamples);
    out.copyFrom (0, 0, mono, numSamples);

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
    // then their delayed output replaces from sample 0).
    const int lat = 600 + (snap.anyFormant
                            ? 2 * CepstralFormantEngine::kSize - CepstralFormantEngine::kHop
                            : 0);
    std::vector<float> shifted ((size_t) numSamples, 0.0f);
    std::vector<float> feed (1, 0.0f);

    for (int j = -lat; j < numSamples; ++j)
    {
        const int src = juce::jlimit (0, numSamples - 1, j + lat);
        feed[0] = mono[src];
        float* chans[1] = { feed.data() };
        // Render bakes the edited state regardless of the bsp_on chain
        // switch (export = the edits, not the monitor state).
        applyEditsToBuffer (chans, 1, 1, (juce::int64) src, sampleRate, snap,
                            focus, mod, speed, true, &shifter, &formant, st);
        if (j >= 0)
            shifted[(size_t) j] = feed[0];
    }
    out.copyFrom (0, 0, shifted.data(), numSamples);
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
