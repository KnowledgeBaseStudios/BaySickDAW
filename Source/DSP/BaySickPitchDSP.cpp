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

// ─── Shared per-sample applicator ─────────────────────────────────────────────
void BaySickPitchDSP::applyEditsToBuffer (float* const* chans, int numCh, int numSamples,
                                          juce::int64 timelineStartSample, double sr,
                                          const Snapshot& snap,
                                          float focus01, float modAmt, float speedMs,
                                          PsolaShifter* shifters,
                                          CepstralFormantEngine* formants,
                                          ApplicatorState& st) const noexcept
{
    const auto& regions = snap.regions;
    if (regions.empty() || numCh <= 0) return;

    const float smoothCoef = 1.0f - std::exp (-1.0f / (float) (speedMs * 0.001 * sr));

    for (int i = 0; i < numSamples; ++i)
    {
        const double tSec = (double) (timelineStartSample + i - snap.startSample) / sr;

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
        float targetFormant = 0.0f;
        float gain          = 1.0f;

        if (inRegion)
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

        const float ratio = std::pow (2.0f, st.smoothedSemis / 12.0f);

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float dry = chans[ch][i];
            float wet = shifters[ch].processSample (dry, ratio);
            if (snap.anyFormant)
                wet = formants[ch].processSample (dry, wet, false, st.smoothedFormant);
            chans[ch][i] = wet * gain;
        }
    }
}

// ─── Realtime applicator (audio thread) ───────────────────────────────────────
void BaySickPitchDSP::processFilePlay (juce::AudioBuffer<float>& buffer,
                                       juce::int64 timelineStartSample) noexcept
{
    auto* snap = mActive.load (std::memory_order_acquire);
    if (snap == nullptr) return;

    const float focus = mFocus01.load (std::memory_order_relaxed);
    const float mod   = mModAmt .load (std::memory_order_relaxed);
    // Fast path (lazy-activate): zero-edit channels at neutral knobs cost
    // one atomic load + two float compares per block.
    if (! snap->anyEdits && focus < 0.001f && std::abs (mod - 1.0f) < 0.01f)
        return;

    const int numCh = juce::jmin (2, buffer.getNumChannels());
    applyEditsToBuffer (buffer.getArrayOfWritePointers(), numCh,
                        buffer.getNumSamples(), timelineStartSample,
                        mSampleRate, *snap,
                        focus, mod, mSpeedMs.load (std::memory_order_relaxed),
                        mShifters.data(), mFormant.data(), mAppState);
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
        applyEditsToBuffer (chans, 1, 1, (juce::int64) src, sampleRate, snap,
                            focus, mod, speed, &shifter, &formant, st);
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
