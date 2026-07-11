#pragma once
#include <JuceHeader.h>
#include "PhaseVocoder.h"
#include <array>
#include <vector>
#include <memory>

// ─────────────────────────────────────────────────────────────────────────────
// PitchShifters - QA-F Task 2 (2026-07-09)
// ─────────────────────────────────────────────────────────────────────────────
// Standalone pitch-shifter trio, extracted + generalized to arbitrary ratio
// from OctaveStyleDSP (PeriodDoubler / GranularShifter, octave-only) and
// PitchCorrectorDSP::Shifter (2-grain).  Shared interface:
//
//   prepare (sampleRate, maxBlock) / reset() / float processSample (in, ratio)
//
// ratio = pitch ratio (2.0 = +1 octave, 0.5 = -1 octave), clamped [0.25, 4].
//
//   PsolaShifter    - pitch-synchronous OLA on an externally-fed period
//                     (drive setPeriodSamples from the caller's YIN tracker).
//                     Grains anchor to the analysis-epoch grid so overlapping
//                     windows stay period-coherent (the PSOLA property; a
//                     free-running granular combs instead).  Latency ~2 pitch
//                     periods (~13 ms @ 150 Hz) - the LIVE-path default per
//                     Call 2a (a phase vocoder adds ~40 ms, unusable for a
//                     learner monitoring themselves).
//   GranularShifter - 4-grain 75%-overlap Hann granular, window-sum
//                     normalized.  Low-latency alternative for material with
//                     no reliable single period (chords / unvoiced).
//   PvShifter       - PhaseVocoder wrapper: time-stretch by ratio, then
//                     resample back at the vocoder's EFFECTIVE ratio (the
//                     synthesis hop is integer-rounded; using the requested
//                     ratio instead drifts ~0.1%).  OFFLINE ONLY - FFT
//                     latency + block batching; backs the Algos dropdown and
//                     render/bake paths, never the live monitor path.
//
// THREADING / RT-SAFETY: one instance per channel per owner; all calls on one
// thread.  PsolaShifter + GranularShifter never allocate after prepare() and
// are audio-thread safe.  PvShifter allocates in prepare() only, but its
// latency makes it offline-only regardless.  Ring positions are int64
// absolute counters mapped physical via % ringSize - O(1) per access, no
// unbounded wrap loops, no overflow inside a playable lifetime (the
// PhaseVocoder outReadAbs precedent).
// ─────────────────────────────────────────────────────────────────────────────

// ── PsolaShifter ─────────────────────────────────────────────────────────────
class PsolaShifter
{
public:
    static constexpr int   kRingSize  = 16384;
    static constexpr float kMinPeriod = 24.0f;     // ~1.8 kHz @ 44.1k
    static constexpr float kMaxPeriod = 1024.0f;   // ~43 Hz @ 44.1k

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
        reset();
    }

    void reset()
    {
        mRing.fill (0.0f);
        mWriteAbs     = 0;
        mNextSynthAbs = 0.0;
        for (auto& g : mGrains) g.active = false;
    }

    // Tracked pitch period in samples, from the caller's tracker.  On
    // unvoiced/no-pitch frames keep the last value - the caller fades its
    // ratio toward 1.0 there, and PSOLA at ratio 1 is a near-identity
    // (50%-overlap Hann pair) at any period.
    void setPeriodSamples (float p) { mPeriod = juce::jlimit (kMinPeriod, kMaxPeriod, p); }

    float processSample (float in, float ratio) noexcept
    {
        mRing[(size_t) (mWriteAbs % kRingSize)] = in;
        ++mWriteAbs;

        ratio = juce::jlimit (0.25f, 4.0f, ratio);
        const double P    = (double) mPeriod;
        const double pOut = P / (double) ratio;   // synthesis epoch spacing

        const double outAbs = (double) mWriteAbs;
        // Warmup: need [center-P, center+P) fully written before any grain.
        if (outAbs >= mNextSynthAbs && mWriteAbs > (juce::int64) (2.0 * P + 8.0))
        {
            // Anchor on the k*P analysis grid by snapping DOWN from the
            // write head, so the whole 2P window is already written AND the
            // anchor stays on-grid.  The original round-then-clamp defeated
            // the grid: the scheduler pins mNextSynthAbs at the write head,
            // so the written-window clamp (writeAbs - P - 2, off-grid) won
            // the min() on virtually every epoch -- the analysis step
            // collapsed to the synthesis step and the shifter degenerated to
            // a pure ~2P delay at ANY ratio (G2 boundary: pitch edits
            // audibly inert while diag showed every sample changed).
            const double center = std::floor ((outAbs - P - 2.0) / P) * P;
            if (center >= P)
                spawnGrain (center, P);
            // Schedule the next epoch; the jmax bounds catch-up after long
            // silence at construction (mNextSynthAbs far behind outAbs).
            mNextSynthAbs = juce::jmax (mNextSynthAbs + pOut, outAbs - pOut);
        }

        float sig = 0.0f, wsum = 0.0f;
        for (auto& g : mGrains)
        {
            if (! g.active) continue;
            const float t = (float) g.age / (float) g.len;
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * t);
            sig  += mRing[(size_t) ((g.startAbs + g.age) % kRingSize)] * w;
            wsum += w;
            if (++g.age >= g.len)
                g.active = false;
        }
        // Window-sum normalize: epoch spacing != P/2 exactly, so raw Hann
        // sums ripple; dividing restores unity.  0.05 floor guards the
        // grain-edge/startup region (sig ~= 0 there anyway).
        return (wsum > 0.05f) ? sig / wsum : sig;
    }

private:
    struct Grain
    {
        juce::int64 startAbs = 0;
        int         len      = 0;
        int         age      = 0;
        bool        active   = false;
    };
    // Concurrent grains = 2*ratio (window 2P, spacing P/ratio) -> 8 at the
    // ratio-4 clamp; +2 slack.
    static constexpr int kMaxGrains = 10;

    void spawnGrain (double center, double P) noexcept
    {
        int slot = -1, oldest = -1, oldestAge = -1;
        for (int i = 0; i < kMaxGrains; ++i)
        {
            if (! mGrains[(size_t) i].active) { slot = i; break; }
            if (mGrains[(size_t) i].age > oldestAge)
                { oldestAge = mGrains[(size_t) i].age; oldest = i; }
        }
        if (slot < 0) slot = oldest;

        auto& g = mGrains[(size_t) slot];
        g.len      = juce::jmax (8, 2 * juce::roundToInt (P));
        g.startAbs = juce::jmax ((juce::int64) 0, (juce::int64) std::llround (center - P));
        g.age      = 0;
        g.active   = true;
    }

    std::array<float, kRingSize>     mRing {};
    std::array<Grain, kMaxGrains>    mGrains {};
    juce::int64                      mWriteAbs     { 0 };
    double                           mNextSynthAbs { 0.0 };
    float                            mPeriod       { 256.0f };
    double                           mSampleRate   { 44100.0 };
};

// ── GranularShifter ──────────────────────────────────────────────────────────
class GranularShifter
{
public:
    static constexpr int kRingSize = 16384;

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        const double sr = (sampleRate > 0.0) ? sampleRate : 44100.0;
        // ~25 ms grains (the PitchCorrectorDSP::Shifter calibration: density
        // vs latency).  2048 cap keeps worst-case lag inside the ring:
        // startLag(<=4g) + travel(g) + slack < kRingSize.
        mGrainSize = juce::jlimit (256, 2048, juce::roundToInt (sr * 0.025));
        mStride    = juce::jmax (1, mGrainSize / 4);   // 75% overlap, 4 concurrent
        reset();
    }

    void reset()
    {
        mRing.fill (0.0f);
        mWriteAbs   = 0;
        mSinceSpawn = 0;
        for (auto& g : mGrains) g.active = false;
    }

    void setGrainSize (int g)
    {
        mGrainSize = juce::jlimit (256, 2048, g);
        mStride    = juce::jmax (1, mGrainSize / 4);
    }

    float processSample (float in, float ratio) noexcept
    {
        mRing[(size_t) (mWriteAbs % kRingSize)] = in;
        ++mWriteAbs;

        ratio = juce::jlimit (0.25f, 4.0f, ratio);

        if (++mSinceSpawn >= mStride
            && mWriteAbs > (juce::int64) (5 * mGrainSize + 8))
        {
            mSinceSpawn = 0;
            int slot = -1, oldest = -1, oldestAge = -1;
            for (int i = 0; i < kNumGrains; ++i)
            {
                if (! mGrains[(size_t) i].active) { slot = i; break; }
                if (mGrains[(size_t) i].age > oldestAge)
                    { oldestAge = mGrains[(size_t) i].age; oldest = i; }
            }
            if (slot < 0) slot = oldest;

            // Start lag grows with up-shift so a ratio-speed read can't
            // overtake the write head mid-grain (gap stays >= grainSize).
            const double lag = (double) mGrainSize * juce::jmax (1.0, (double) ratio);
            auto& g    = mGrains[(size_t) slot];
            g.readPos  = (double) mWriteAbs - lag;
            g.age      = 0;
            g.active   = true;
        }

        float sig = 0.0f, wsum = 0.0f;
        for (auto& g : mGrains)
        {
            if (! g.active) continue;
            const juce::int64 ip = (juce::int64) g.readPos;
            // Live-ratio glides can still close the gap - retire early
            // rather than read unwritten samples (normalize masks the loss).
            if (ip + 1 >= mWriteAbs) { g.active = false; continue; }

            const float frac = (float) (g.readPos - (double) ip);
            const float a    = mRing[(size_t) (ip % kRingSize)];
            const float b    = mRing[(size_t) ((ip + 1) % kRingSize)];
            const float t    = (float) g.age / (float) mGrainSize;
            const float w    = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * t);
            sig  += (a + (b - a) * frac) * w;
            wsum += w;

            g.readPos += (double) ratio;   // live ratio: correction stays responsive
            if (++g.age >= mGrainSize)
                g.active = false;
        }
        return (wsum > 0.05f) ? sig / wsum : sig;
    }

private:
    struct Grain
    {
        double readPos = 0.0;   // absolute fractional sample position
        int    age     = 0;
        bool   active  = false;
    };
    static constexpr int kNumGrains = 6;   // 4 concurrent + slack

    std::array<float, kRingSize>       mRing {};
    std::array<Grain, kNumGrains>      mGrains {};
    juce::int64                        mWriteAbs   { 0 };
    int                                mSinceSpawn { 0 };
    int                                mGrainSize  { 1024 };
    int                                mStride     { 256 };
};

// ── PvShifter ────────────────────────────────────────────────────────────────
// OFFLINE ONLY (render/bake + Algos dropdown).  Latency ~ kFFTSize at the
// working sample rate plus the internal push batch - never the live path.
class PvShifter
{
public:
    void prepare (double sampleRate, int /*maxBlock*/)
    {
        mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
        if (mPv == nullptr)
            mPv = std::make_unique<PhaseVocoder> (1);
        mInChunk.setSize (1, kChunk);
        mPullBuf.setSize (1, 1 << 14);
        mFifo.assign ((size_t) kFifoSize, 0.0f);
        reset();
    }

    void reset()
    {
        if (mPv != nullptr) mPv->reset();
        mInFill       = 0;
        mFifoWriteAbs = 0;
        mReadPos      = 0.0;
        mRatio        = 0.0f;   // force a setStretchRatio on first sample
        mEffRatio     = 1.0;
        std::fill (mFifo.begin(), mFifo.end(), 0.0f);
        mInChunk.clear();
    }

    float processSample (float in, float ratio) noexcept
    {
        ratio = juce::jlimit (0.25f, 4.0f, ratio);
        if (ratio != mRatio)
        {
            mRatio = ratio;
            mPv->setStretchRatio ((double) ratio);
            // Resample at the vocoder's EFFECTIVE stretch (integer-rounded
            // synthesis hop), or the output clock drifts ~0.1%.
            mEffRatio = (double) juce::jmax (1, juce::roundToInt (
                            (double) PhaseVocoder::kHopSize * (double) ratio))
                        / (double) PhaseVocoder::kHopSize;
        }

        mInChunk.setSample (0, mInFill++, in);
        if (mInFill == kChunk)
        {
            mPv->push (mInChunk, 0, kChunk);
            mInFill = 0;
            for (int got = 0;
                 (got = mPv->pull (mPullBuf, 0, mPullBuf.getNumSamples())) > 0;)
            {
                const float* p = mPullBuf.getReadPointer (0);
                for (int i = 0; i < got; ++i)
                    mFifo[(size_t) ((mFifoWriteAbs + i) % kFifoSize)] = p[i];
                mFifoWriteAbs += got;
            }
        }

        // Stretched output read back at effRatio = pitch shift, length held.
        const juce::int64 ip = (juce::int64) mReadPos;
        if (ip + 1 >= mFifoWriteAbs)
            return 0.0f;   // priming (latency window) - hold silence, don't advance
        const float frac = (float) (mReadPos - (double) ip);
        const float a    = mFifo[(size_t) (ip % kFifoSize)];
        const float b    = mFifo[(size_t) ((ip + 1) % kFifoSize)];
        mReadPos += mEffRatio;
        return a + (b - a) * frac;
    }

    // One-shot offline helper: pitch-shift n mono samples by ratio, length
    // preserved.  Pads the vocoder with a pre-roll (primes the OLA ramp-in)
    // and a tail (flushes the settle margin), then maps the span back at the
    // effective ratio - the same recipe as the channel-composite stretch
    // path.  Allocates; message/worker thread only.
    static void shiftBufferMono (const float* in, float* out, int n, double ratio)
    {
        if (n <= 0) return;
        ratio = juce::jlimit (0.25, 4.0, ratio);
        const double effRatio =
            (double) juce::jmax (1, juce::roundToInt (
                (double) PhaseVocoder::kHopSize * ratio))
            / (double) PhaseVocoder::kHopSize;

        PhaseVocoder pv (1);
        pv.setStretchRatio (ratio);

        const int preN  = (int) std::ceil ((double) PhaseVocoder::kFFTSize
                                           / juce::jmin (1.0, ratio));
        const int tailN = (int) std::ceil (2.0 * (double) PhaseVocoder::kFFTSize
                                           / juce::jmin (1.0, ratio));

        std::vector<float> outAccum;
        outAccum.reserve ((size_t) ((double) (preN + n + tailN) * effRatio) + 4096);

        juce::AudioBuffer<float> feed (1, PhaseVocoder::kHopSize);
        juce::AudioBuffer<float> pull (1, 1 << 14);

        // Pre-roll mirrors the head (zero is fine too; the skip discards it).
        juce::int64 fpos = -(juce::int64) preN;
        const juce::int64 feedEnd = (juce::int64) n + tailN;
        while (fpos < feedEnd)
        {
            const int chunk = (int) juce::jmin (
                (juce::int64) PhaseVocoder::kHopSize, feedEnd - fpos);
            feed.clear();
            for (int i = 0; i < chunk; ++i)
            {
                const juce::int64 s = fpos + i;
                if (s >= 0 && s < (juce::int64) n)
                    feed.setSample (0, i, in[s]);
            }
            pv.push (feed, 0, chunk);
            for (int got = 0;
                 (got = pv.pull (pull, 0, pull.getNumSamples())) > 0;)
                outAccum.insert (outAccum.end(),
                                 pull.getReadPointer (0),
                                 pull.getReadPointer (0) + got);
            fpos += chunk;
        }

        const double outSkip = (double) preN * effRatio;
        const double srcLenS = juce::jmax (1.0, (double) n * effRatio);
        for (int j = 0; j < n; ++j)
        {
            const double op = outSkip + ((double) j / (double) n) * srcLenS;
            const int    ip = (int) op;
            if (ip + 1 >= (int) outAccum.size()) { out[j] = 0.0f; continue; }
            const float  fr = (float) (op - (double) ip);
            out[j] = outAccum[(size_t) ip]
                   + (outAccum[(size_t) ip + 1] - outAccum[(size_t) ip]) * fr;
        }
    }

private:
    static constexpr int kChunk    = 64;
    static constexpr int kFifoSize = 1 << 15;

    std::unique_ptr<PhaseVocoder> mPv;
    juce::AudioBuffer<float>      mInChunk;
    juce::AudioBuffer<float>      mPullBuf;
    std::vector<float>            mFifo;
    int                           mInFill       { 0 };
    juce::int64                   mFifoWriteAbs { 0 };
    double                        mReadPos      { 0.0 };
    float                         mRatio        { 0.0f };
    double                        mEffRatio     { 1.0 };
    double                        mSampleRate   { 44100.0 };
};

// ── CepstralFormantEngine ────────────────────────────────────────────────────
// Shared formant machinery (QA-F Tasks 3 + 5): cepstral spectral-envelope
// extraction -> envelope replacement/scaling, STFT frame-based.
//
//   Formant PRESERVE: impose the DRY (pre-shift) frame's envelope onto the
//   WET (post-shift) frame -- kills the chipmunk/munchkin effect a plain
//   time-domain shifter leaves behind.
//   THROAT SHIFT: scale the target envelope along the frequency axis by
//   2^(semis/12) -- deliberate character change without re-pitching.
//
// Envelope = cepstral liftering (log-magnitude -> IFFT -> keep quefrencies
// below ~1 ms -> FFT back).  The 1 ms cutoff sits under the shortest vocal
// pitch period (~2 ms at a 500 Hz soprano) so harmonics never leak into the
// "envelope".
//
// FFT 512 / hop 128 (75% overlap, Hann analysis + synthesis, (8/3)*Hs/N OLA
// scale -- the PhaseVocoder normalization convention).  Latency = 2*kSize -
// kHop = 896 samples (~20 ms @ 44.1k: one full frame to fire + the OLA
// settle margin) ON TOP of the upstream shifter -- acceptable for an opt-in
// toggle; the default monitor path stays shifter-only (Call 2a).
//
// THREADING: one instance per channel, all calls one thread.  No allocation
// after prepare(); audio-thread safe.
class CepstralFormantEngine
{
public:
    static constexpr int kOrder = 9;
    static constexpr int kSize  = 1 << kOrder;   // 512
    static constexpr int kHop   = kSize / 4;     // 128

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
        if (mFFT == nullptr)
            mFFT = std::make_unique<juce::dsp::FFT> (kOrder);
        for (int i = 0; i < kSize; ++i)
            mWindow[(size_t) i] = 0.5f * (1.0f - std::cos (
                juce::MathConstants<float>::twoPi * (float) i / (float) (kSize - 1)));
        // Quefrency lifter cutoff ~1 ms (see class comment).
        mLifterBins = juce::jlimit (8, kSize / 4, (int) std::round (mSampleRate * 0.001));
        reset();
    }

    void reset()
    {
        mDryRing.fill (0.0f);
        mWetRing.fill (0.0f);
        mOla.fill (0.0f);
        mInAbs = 0;
        mOutReadAbs = 0;
        mOlaWriteAbs = 0;
    }

    // Streaming pair input: dry = pre-shift sample, wet = post-shift sample.
    // Returns the formant-corrected wet stream, 2*kSize-kHop samples late.
    // Caller engages this only while (preserve || throatSemis != 0) and
    // resets on the engage edge (latency appears/disappears with the mode).
    float processSample (float dry, float wet, bool preserve, float throatSemis) noexcept
    {
        mDryRing[(size_t) (mInAbs % kSize)] = dry;
        mWetRing[(size_t) (mInAbs % kSize)] = wet;
        ++mInAbs;

        if (mInAbs >= (juce::int64) kSize && (mInAbs % kHop) == 0)
            processFrame (preserve, throatSemis);

        // Deliver only fully-settled OLA slots (every overlapping frame has
        // contributed) -- the PhaseVocoder getOutputAvailable convention.
        // Stalling during warmup IS the latency.
        float out = 0.0f;
        const juce::int64 settled = mOlaWriteAbs - (juce::int64) (kSize - kHop);
        if (mOutReadAbs < settled)
        {
            const size_t idx = (size_t) (mOutReadAbs % (juce::int64) mOla.size());
            out = mOla[idx];
            mOla[idx] = 0.0f;   // clear-after-read: future OLA lands on silence
            ++mOutReadAbs;
        }
        return out;
    }

    // Offline convenience (Align bake): impose the buffer's own envelope
    // scaled by throatSemis (formant character shift, pitch untouched).
    // Latency-compensated.  Allocates; message/worker thread only.
    static void formantShiftMono (float* buf, int n, double sampleRate, float throatSemis)
    {
        if (n <= 0 || std::abs (throatSemis) < 0.01f) return;
        CepstralFormantEngine eng;
        eng.prepare (sampleRate, 0);
        const int lat = 2 * kSize - kHop;
        std::vector<float> out ((size_t) n, 0.0f);
        for (int i = 0; i < n + lat; ++i)
        {
            const float in = (i < n) ? buf[i] : 0.0f;
            const float v  = eng.processSample (in, in, false, throatSemis);
            const int   j  = i - lat;
            if (j >= 0 && j < n) out[(size_t) j] = v;
        }
        std::copy (out.begin(), out.end(), buf);
    }

private:
    void processFrame (bool preserve, float throatSemis) noexcept
    {
        // Windowed frames end at the current input head.
        const juce::int64 frameStart = mInAbs - kSize;
        for (int i = 0; i < kSize; ++i)
        {
            const size_t src = (size_t) ((frameStart + i) % kSize);
            mFrameWet[(size_t) i] = { mWetRing[src] * mWindow[(size_t) i], 0.0f };
            if (preserve)
                mFrameDry[(size_t) i] = { mDryRing[src] * mWindow[(size_t) i], 0.0f };
        }
        mFFT->perform (mFrameWet.data(), mSpecWet.data(), false);
        if (preserve)
            mFFT->perform (mFrameDry.data(), mSpecDry.data(), false);

        // Log-magnitude envelopes via cepstral lifter.
        computeEnvelope (mSpecWet, mEnvWet);
        if (preserve)
            computeEnvelope (mSpecDry, mEnvDry);

        // Target envelope: dry's (preserve) or wet's own (throat-only),
        // frequency-scaled by the throat ratio.
        const float scale = std::pow (2.0f, -throatSemis / 12.0f);   // sample source bin = k*scale
        auto& srcEnv = preserve ? mEnvDry : mEnvWet;
        for (int k = 0; k <= kSize / 2; ++k)
        {
            const float pos = (float) k * scale;
            const int   k0  = (int) pos;
            float target;
            if (k0 + 1 <= kSize / 2)
            {
                const float fr = pos - (float) k0;
                target = srcEnv[(size_t) k0] * (1.0f - fr) + srcEnv[(size_t) k0 + 1] * fr;
            }
            else
                target = srcEnv[(size_t) (kSize / 2)];

            // Correction in the log domain, clamped +/-24 dB per bin so a
            // near-silent envelope can't blow the frame up.
            const float corr = juce::jlimit (-2.7631f, 2.7631f, target - mEnvWet[(size_t) k]);
            const float g    = std::exp (corr);
            mSpecWet[(size_t) k] *= g;
            if (k > 0 && k < kSize / 2)
                mSpecWet[(size_t) (kSize - k)] = std::conj (mSpecWet[(size_t) k]);
        }

        mFFT->perform (mSpecWet.data(), mFrameWet.data(), true);

        // Hann synthesis window + (8/3)*Hs/N OLA normalization (PhaseVocoder
        // convention: window applied twice, 75% overlap).
        const float olaScale = (8.0f / 3.0f) * (float) kHop / (float) kSize;
        const int olaSize = (int) mOla.size();
        for (int i = 0; i < kSize; ++i)
        {
            const size_t idx = (size_t) ((mOlaWriteAbs + i) % (juce::int64) olaSize);
            mOla[idx] += mFrameWet[(size_t) i].real() * mWindow[(size_t) i] * olaScale;
        }
        mOlaWriteAbs += kHop;
    }

    void computeEnvelope (const std::array<juce::dsp::Complex<float>, kSize>& spec,
                          std::array<float, kSize / 2 + 1>& env) noexcept
    {
        for (int k = 0; k < kSize; ++k)
        {
            const float m = std::abs (spec[(size_t) k]);
            mCepIn[(size_t) k] = { std::log (juce::jmax (m, 1.0e-9f)), 0.0f };
        }
        mFFT->perform (mCepIn.data(), mCepOut.data(), true);   // -> quefrency
        for (int q = 0; q < kSize; ++q)
        {
            const bool keep = (q <= mLifterBins) || (q >= kSize - mLifterBins);
            if (! keep) mCepOut[(size_t) q] = { 0.0f, 0.0f };
        }
        mFFT->perform (mCepOut.data(), mCepIn.data(), false);  // -> log-spectral envelope
        for (int k = 0; k <= kSize / 2; ++k)
            env[(size_t) k] = mCepIn[(size_t) k].real();
    }

    std::unique_ptr<juce::dsp::FFT> mFFT;
    std::array<float, kSize>  mWindow {};
    std::array<float, kSize>  mDryRing {};
    std::array<float, kSize>  mWetRing {};
    // 3x frame size: peak occupancy is write-frontier + kSize minus the
    // settle-lagged read head = exactly 2*kSize -- 3x leaves real margin.
    std::array<float, kSize * 3> mOla {};
    std::array<juce::dsp::Complex<float>, kSize> mFrameWet {}, mFrameDry {};
    std::array<juce::dsp::Complex<float>, kSize> mSpecWet {},  mSpecDry {};
    std::array<juce::dsp::Complex<float>, kSize> mCepIn {},    mCepOut {};
    std::array<float, kSize / 2 + 1> mEnvWet {}, mEnvDry {};
    juce::int64 mInAbs       { 0 };
    juce::int64 mOutReadAbs  { 0 };
    juce::int64 mOlaWriteAbs { 0 };
    int         mLifterBins  { 44 };
    double      mSampleRate  { 44100.0 };
};
