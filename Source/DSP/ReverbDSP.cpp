#include "ReverbDSP.h"
#include <cmath>
#include <algorithm>

//──────────────────────────────────────────────────────────────────────────────
// Static data
//──────────────────────────────────────────────────────────────────────────────
// Prime delay lengths at 44100 Hz.  Scaled by (mRoomSize / 0.6) × (SR / 44100).
const int ReverbDSP::kPrimes[ReverbDSP::kN] = { 149, 211, 263, 293, 353, 389, 421, 443 };

//──────────────────────────────────────────────────────────────────────────────
ReverbDSP::ReverbDSP() {}

//──────────────────────────────────────────────────────────────────────────────
void ReverbDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;

    const int maxPre = static_cast<int>(kMaxPreDelayMs * 0.001 * sampleRate) + 1;
    mPreBufL.assign (maxPre, 0.0f);
    mPreBufR.assign (maxPre, 0.0f);
    mPreWrite = 0;

    // §8b: allocate FDN buffers at MAX size ONCE here. `updateDelayLines()`
    // updates mFDNLen[i] afterwards; no re-allocation on room-size change.
    // Cap SR scaling at kMaxSR so buffers stay bounded.
    const float srCap   = std::min ((float) sampleRate, kMaxSR);
    const float maxCoef = (kMaxRoomSize / 0.6f) * (srCap / 44100.0f);
    for (int i = 0; i < kN; ++i)
    {
        const int maxLen = std::max (16, (int) ((float) kPrimes[i] * maxCoef) + 4);
        mFDNL[i].assign ((size_t) maxLen, 0.0f);
        mFDNR[i].assign ((size_t) maxLen, 0.0f);
    }

    // §8c freeze smoother — 30 ms ramp for smooth engage/disengage
    mFreezeSmooth.reset (sampleRate, 0.030);
    mFreezeSmooth.setCurrentAndTargetValue (mFreeze ? 1.0f : 0.0f);

    updateInputFilters();
    updatePreDelay();
    updateDiffusion();
    updateDelayLines();     // also calls updateFeedback()
    updateWetTone();        // §8g

    // §8a: distribute tail-mod phases across 0..2π so lines don't lockstep
    for (int i = 0; i < kN; ++i)
        mTailModPhase[(size_t) i] =
            juce::MathConstants<float>::twoPi * (float) i / (float) kN;

    // ── H-9 (2026-05-02) Plate allpass cascade + Room ER buffers ─────────
    // Plate: 8 allpass stages at increasing prime delays + 4 parallel
    // feedback combs.  Delays scaled by sample rate; tuned for plate
    // shimmer character.  Comb delays slightly de-tuned to avoid resonant
    // peaks.  Stage B fills the actual algorithm; Stage A allocates
    // buffers so a Mode dropdown switch to Plate doesn't crash.
    {
        // Allpass delay lengths in ms (primes-near pattern, increasing)
        const float apMs[kPlateAllpassStages] = {
            4.7f, 7.3f, 11.4f, 17.9f, 28.1f, 44.2f, 69.5f, 109.3f
        };
        for (int i = 0; i < kPlateAllpassStages; ++i)
        {
            const int len = juce::jmax (16,
                (int) (apMs[i] * 0.001f * (float) sampleRate) + 4);
            mPlateApBufL[(size_t) i].assign ((size_t) len, 0.0f);
            mPlateApBufR[(size_t) i].assign ((size_t) len, 0.0f);
            mPlateApDelay[(size_t) i] = len;
            mPlateApPos[(size_t) i]   = 0;
        }
        // Comb delay lengths in ms (Schroeder-classic 4-comb spread)
        const float combMs[kPlateCombs] = { 29.7f, 37.1f, 41.1f, 43.7f };
        for (int i = 0; i < kPlateCombs; ++i)
        {
            const int len = juce::jmax (16,
                (int) (combMs[i] * 0.001f * (float) sampleRate) + 4);
            mPlateCombBufL[(size_t) i].assign ((size_t) len, 0.0f);
            mPlateCombBufR[(size_t) i].assign ((size_t) len, 0.0f);
            mPlateCombDelay[(size_t) i] = len;
            mPlateCombPos[(size_t) i]   = 0;
        }
    }
    // ── H-9 stage D Chamber: nested allpass blocks (Gardner / Bricasti M7) ─
    // Outer delay times: 25, 33, 41, 53 ms.  Inner delays: 7, 11, 13, 17 ms
    // (primey to avoid resonance buildup).  Outer coef 0.5 / inner coef 0.6
    // give smooth dense diffusion with an inner damping LP per stage.
    {
        const float outerMs[kChamberOuterStages] = { 25.0f, 33.0f, 41.0f, 53.0f };
        const float innerMs[kChamberOuterStages] = {  7.0f, 11.0f, 13.0f, 17.0f };
        for (int i = 0; i < kChamberOuterStages; ++i)
        {
            const int oLen = juce::jmax (16, (int) (outerMs[i] * 0.001f * (float) sampleRate) + 4);
            const int iLen = juce::jmax (16, (int) (innerMs[i] * 0.001f * (float) sampleRate) + 4);
            mChOuterBufL[(size_t) i].assign ((size_t) oLen, 0.0f);
            mChOuterBufR[(size_t) i].assign ((size_t) oLen, 0.0f);
            mChInnerBufL[(size_t) i].assign ((size_t) iLen, 0.0f);
            mChInnerBufR[(size_t) i].assign ((size_t) iLen, 0.0f);
            mChOuterDelay[(size_t) i] = oLen;
            mChInnerDelay[(size_t) i] = iLen;
        }
    }

    // ── H-9 stage E Room: 200 ms ER cloud buffer + 4 short Schroeder combs ─
    // Comb delays: 17, 23, 29, 37 ms (short, primey, no harmonic alignment).
    // 15-tap ER cloud feeds the comb tail; output sums ER + comb-tail.
    {
        const int erLen = juce::jmax (16, (int) (0.200 * sampleRate) + 4);
        mRoomERBufL.assign ((size_t) erLen, 0.0f);
        mRoomERBufR.assign ((size_t) erLen, 0.0f);
        mRoomERPos = 0;
        for (int t = 0; t < kRoomERTaps; ++t)
        {
            int s = (int) (kRoomERTapMs[t] * 0.001f * (float) sampleRate);
            s = juce::jlimit (1, erLen - 1, s);
            mRoomERTapSamples[(size_t) t] = s;
        }
        const float combMs[kRoomCombs] = { 17.0f, 23.0f, 29.0f, 37.0f };
        for (int i = 0; i < kRoomCombs; ++i)
        {
            const int len = juce::jmax (16, (int) (combMs[i] * 0.001f * (float) sampleRate) + 4);
            mRoomCombBufL[(size_t) i].assign ((size_t) len, 0.0f);
            mRoomCombBufR[(size_t) i].assign ((size_t) len, 0.0f);
            mRoomCombDelay[(size_t) i] = len;
        }
    }

    // ── H-9 stage F VocalBooth: tiny 4-line FDN, 7..15 ms delays (no mod) ──
    {
        for (int i = 0; i < kBoothFDN; ++i)
        {
            const int srScaledLen = (int) std::round ((float) kBoothDelaySeeds[i] * (float) sampleRate / 44100.0f);
            const int len = juce::jmax (16, srScaledLen + 4);
            mVbFDNL[(size_t) i].assign ((size_t) len, 0.0f);
            mVbFDNR[(size_t) i].assign ((size_t) len, 0.0f);
            mVbFDNLen[(size_t) i] = len;
        }
    }

    reset();
}

//──────────────────────────────────────────────────────────────────────────────
void ReverbDSP::reset()
{
    std::fill (mPreBufL.begin(), mPreBufL.end(), 0.0f);
    std::fill (mPreBufR.begin(), mPreBufR.end(), 0.0f);
    mPreWrite = 0;

    mInHPStL = mInHPStR = mInHPPrevL = mInHPPrevR = 0.0f;
    mInLPStL = mInLPStR = 0.0f;

    for (int k = 0; k < kDiff; ++k)
    {
        std::fill (mAPBufL[(size_t) k].begin(), mAPBufL[(size_t) k].end(), 0.0f);
        std::fill (mAPBufR[(size_t) k].begin(), mAPBufR[(size_t) k].end(), 0.0f);
        mAPWrL[(size_t) k] = mAPWrR[(size_t) k] = 0;
    }

    for (int i = 0; i < kN; ++i)
    {
        std::fill (mFDNL[(size_t) i].begin(), mFDNL[(size_t) i].end(), 0.0f);
        std::fill (mFDNR[(size_t) i].begin(), mFDNR[(size_t) i].end(), 0.0f);
        mFDNWr[(size_t) i]   = 0;
        mLPStL[(size_t) i]   = mLPStR[(size_t) i]   = 0.0f;
        mBassStL[(size_t) i] = mBassStR[(size_t) i] = 0.0f;
        mTailModSH[(size_t) i] = 0.0f;
        // Tail-mod phases are set in prepare() (per-line spread); don't zero here.
    }

    mWetLoShelf.reset();
    mWetHiShelf.reset();

    // H-9 (2026-05-02): clear duck envelope and per-algorithm transient state
    // so an algorithm switch / reset doesn't leak the previous tail.
    mDuckEnv = 0.0f;
    for (auto& v : mPlateApBufL)  std::fill (v.begin(), v.end(), 0.0f);
    for (auto& v : mPlateApBufR)  std::fill (v.begin(), v.end(), 0.0f);
    for (auto& v : mPlateCombBufL) std::fill (v.begin(), v.end(), 0.0f);
    for (auto& v : mPlateCombBufR) std::fill (v.begin(), v.end(), 0.0f);
    mPlateApPos.fill (0);
    mPlateCombPos.fill (0);
    mPlateCombFiltL.fill (0.0f);
    mPlateCombFiltR.fill (0.0f);
    std::fill (mRoomERBufL.begin(), mRoomERBufL.end(), 0.0f);
    std::fill (mRoomERBufR.begin(), mRoomERBufR.end(), 0.0f);
    mRoomERPos = 0;

    // ── Chamber state (H-9 stage D) ──────────────────────────────────────
    for (int i = 0; i < kChamberOuterStages; ++i)
    {
        std::fill (mChOuterBufL[(size_t) i].begin(), mChOuterBufL[(size_t) i].end(), 0.0f);
        std::fill (mChOuterBufR[(size_t) i].begin(), mChOuterBufR[(size_t) i].end(), 0.0f);
        std::fill (mChInnerBufL[(size_t) i].begin(), mChInnerBufL[(size_t) i].end(), 0.0f);
        std::fill (mChInnerBufR[(size_t) i].begin(), mChInnerBufR[(size_t) i].end(), 0.0f);
        mChOuterPosL[(size_t) i] = mChOuterPosR[(size_t) i] = 0;
        mChInnerPosL[(size_t) i] = mChInnerPosR[(size_t) i] = 0;
        mChInnerLPL [(size_t) i] = mChInnerLPR [(size_t) i] = 0.0f;
    }

    // ── Room state (H-9 stage E) ─────────────────────────────────────────
    for (int i = 0; i < kRoomCombs; ++i)
    {
        std::fill (mRoomCombBufL[(size_t) i].begin(), mRoomCombBufL[(size_t) i].end(), 0.0f);
        std::fill (mRoomCombBufR[(size_t) i].begin(), mRoomCombBufR[(size_t) i].end(), 0.0f);
        mRoomCombPosL[(size_t) i] = mRoomCombPosR[(size_t) i] = 0;
        mRoomCombLPL [(size_t) i] = mRoomCombLPR [(size_t) i] = 0.0f;
    }

    // ── VocalBooth state (H-9 stage F) ───────────────────────────────────
    for (int i = 0; i < kBoothFDN; ++i)
    {
        std::fill (mVbFDNL[(size_t) i].begin(), mVbFDNL[(size_t) i].end(), 0.0f);
        std::fill (mVbFDNR[(size_t) i].begin(), mVbFDNR[(size_t) i].end(), 0.0f);
        mVbFDNWrL[(size_t) i] = mVbFDNWrR[(size_t) i] = 0;
        mVbFDNLPL[(size_t) i] = mVbFDNLPR[(size_t) i] = 0.0f;
    }
}

//──────────────────────────────────────────────────────────────────────────────
// Update helpers
//──────────────────────────────────────────────────────────────────────────────
void ReverbDSP::updateInputFilters()
{
    if (mSampleRate <= 0.0) return;
    const float sr  = (float) mSampleRate;
    const float pi2 = juce::MathConstants<float>::twoPi;

    // 1-pole HPF coefficient  y[n] = a*(y[n-1] + x[n] - x[n-1])
    mInHPCoef  = std::exp (-pi2 * std::max (1.0f, mLowCutHz) / sr);

    // 1-pole LPF  y += alpha*(x-y)
    mInLPAlpha = 1.0f - std::exp (-pi2 * std::min (22000.0f, mHighCutHz) / sr);

    // High-damping LP (in feedback path) -- Hall FDN only
    mLPAlpha   = 1.0f - std::exp (-pi2 * std::max (20.0f, mHighDampHz) / sr);
}

void ReverbDSP::updateFeedback()
{
    if (mSampleRate <= 0.0) return;
    // Hall topology only.  Other algorithms own their own decay/feedback in
    // their dedicated process functions.
    const float decayClamped = std::max (0.01f, mDecay);
    const float maxG = 0.9999f / std::max (1.001f, mBassMult);

    for (int i = 0; i < kN; ++i)
    {
        if (mFDNLen[(size_t) i] == 0) { mFeedGain[(size_t) i] = 0.0f; continue; }
        const float delayLen_s = (float) mFDNLen[(size_t) i] / (float) mSampleRate;
        const float g = std::exp (-6.91f * delayLen_s / decayClamped);
        mFeedGain[(size_t) i] = std::min (g, maxG);
    }

    // Bass shelf coefficients
    const float pi2 = juce::MathConstants<float>::twoPi;
    mBassLPCoef  = std::exp (-pi2 * std::max (1.0f, mBassCrossHz) / (float) mSampleRate);
    mBassGainAdd = mBassMult - 1.0f;
}

void ReverbDSP::updateDelayLines()
{
    if (mSampleRate <= 0.0) return;
    // Hall topology = the 8-line FDN.  Other algorithms (Plate / Chamber /
    // Room / VocalBooth) run their own dedicated topologies in separate
    // process functions, so this scale only governs the Hall FDN.
    const float scale = std::max (0.1f, mRoomSize) / 0.6f
                      * (float) (mSampleRate / 44100.0);

    // §8b: no realloc — just update effective length. Wrap the write pointer
    // if it falls outside the new length (handles shrink case). Do NOT zero
    // filter state; the running tail keeps decaying naturally.
    for (int i = 0; i < kN; ++i)
    {
        const int bufCap = (int) mFDNL[(size_t) i].size();
        if (bufCap <= 0) continue;   // not prepared yet
        int newLen = std::max (8, (int) ((float) kPrimes[i] * scale));
        if (newLen > bufCap) newLen = bufCap;   // safety clamp to allocation

        if (newLen != mFDNLen[(size_t) i])
        {
            mFDNLen[(size_t) i] = newLen;
            if (mFDNWr[(size_t) i] >= newLen)
                mFDNWr[(size_t) i] = mFDNWr[(size_t) i] % newLen;
        }
    }
    updateFeedback();
}

void ReverbDSP::updateDiffusion()
{
    if (mSampleRate <= 0.0) return;
    mAPCoef = mDiffusion * 0.7f;

    for (int k = 0; k < kDiff; ++k)
    {
        const int len = std::max (1, (int) (kAPMs[k] * 0.001f * (float) mSampleRate));
        if (len != mAPLen[(size_t) k] || mAPBufL[(size_t) k].empty())
        {
            mAPLen[(size_t) k] = len;
            mAPBufL[(size_t) k].assign ((size_t) len, 0.0f);
            mAPBufR[(size_t) k].assign ((size_t) len, 0.0f);
            mAPWrL[(size_t) k] = mAPWrR[(size_t) k] = 0;
        }
    }
}

// H-9 (2026-05-02): tempo-sync division table -- pattern matches FlangerDSP
// kSyncDivisions.  num/den expresses pre-delay length in whole notes; index
// 2 = 1/4 is the post-load default for new instances.
const ReverbDSP::SyncDiv ReverbDSP::kSyncDivisions[ReverbDSP::kNumSyncDivisions] =
{
    { "1/1",  1, 1  },  // 0
    { "1/2",  1, 2  },  // 1
    { "1/4",  1, 4  },  // 2  (default)
    { "1/8",  1, 8  },  // 3
    { "1/8D", 3, 16 },  // 4  dotted 8th
    { "1/4T", 1, 6  },  // 5  quarter triplet
    { "1/16", 1, 16 },  // 6
    { "1/8T", 1, 12 },  // 7  eighth triplet
};

void ReverbDSP::updatePreDelay()
{
    if (mSampleRate <= 0.0 || mPreBufL.empty()) return;
    float ms = mPreDelayMs;
    if (mTempoSync && mHostBPM > 0.0)
    {
        // H-9: select beat division from kSyncDivisions[mSyncDivIdx].
        // noteFrac is in whole notes; 4 beats per whole note; ms_per_beat = 60000/BPM.
        const int    i        = juce::jlimit (0, kNumSyncDivisions - 1, mSyncDivIdx);
        const double noteFrac = (double) kSyncDivisions[i].num
                              / (double) std::max (1, kSyncDivisions[i].den);
        ms = (float) (noteFrac * 4.0 * (60000.0 / mHostBPM));
    }
    mPreDelaySamples = (int) (ms * 0.001f * (float) mSampleRate);
    mPreDelaySamples = std::min (mPreDelaySamples, (int) mPreBufL.size() - 1);
    mPreDelaySamples = std::max (0, mPreDelaySamples);
}

// RBJ cookbook low-shelf biquad. gainDb > 0 boosts below fc, < 0 cuts.
static void computeLoShelf (ReverbDSP::TiltBiquad& bq, double sr, float fc, float Q, float gainDb)
{
    const double A     = std::pow (10.0, (double) gainDb / 40.0);
    const double w0    = juce::MathConstants<double>::twoPi * (double) fc / sr;
    const double cosw0 = std::cos (w0);
    const double sinw0 = std::sin (w0);
    const double alpha = sinw0 / (2.0 * std::max (0.001, (double) Q));
    const double sqrtA_2a = 2.0 * std::sqrt (A) * alpha;

    const double b0 =  A * ((A + 1.0) - (A - 1.0) * cosw0 + sqrtA_2a);
    const double b1 =  2.0 * A * ((A - 1.0) - (A + 1.0) * cosw0);
    const double b2 =  A * ((A + 1.0) - (A - 1.0) * cosw0 - sqrtA_2a);
    const double a0 =       (A + 1.0) + (A - 1.0) * cosw0 + sqrtA_2a;
    const double a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosw0);
    const double a2 =       (A + 1.0) + (A - 1.0) * cosw0 - sqrtA_2a;

    bq.b0 = (float) (b0 / a0);
    bq.b1 = (float) (b1 / a0);
    bq.b2 = (float) (b2 / a0);
    bq.a1 = (float) (a1 / a0);
    bq.a2 = (float) (a2 / a0);
}

// RBJ cookbook high-shelf biquad. gainDb > 0 boosts above fc, < 0 cuts.
static void computeHiShelf (ReverbDSP::TiltBiquad& bq, double sr, float fc, float Q, float gainDb)
{
    const double A     = std::pow (10.0, (double) gainDb / 40.0);
    const double w0    = juce::MathConstants<double>::twoPi * (double) fc / sr;
    const double cosw0 = std::cos (w0);
    const double sinw0 = std::sin (w0);
    const double alpha = sinw0 / (2.0 * std::max (0.001, (double) Q));
    const double sqrtA_2a = 2.0 * std::sqrt (A) * alpha;

    const double b0 =  A * ((A + 1.0) + (A - 1.0) * cosw0 + sqrtA_2a);
    const double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosw0);
    const double b2 =  A * ((A + 1.0) + (A - 1.0) * cosw0 - sqrtA_2a);
    const double a0 =       (A + 1.0) - (A - 1.0) * cosw0 + sqrtA_2a;
    const double a1 =  2.0 * ((A - 1.0) - (A + 1.0) * cosw0);
    const double a2 =       (A + 1.0) - (A - 1.0) * cosw0 - sqrtA_2a;

    bq.b0 = (float) (b0 / a0);
    bq.b1 = (float) (b1 / a0);
    bq.b2 = (float) (b2 / a0);
    bq.a1 = (float) (a1 / a0);
    bq.a2 = (float) (a2 / a0);
}

void ReverbDSP::updateWetTone()
{
    if (mSampleRate <= 0.0) return;
    // §8g: Pultec-style tilt — complementary low/high shelves around a centre
    // pivot. Positive WetTone = bright: cut lows, boost highs (each ±dB/2).
    // This keeps overall broadband gain close to unity regardless of setting.
    const float halfDb = mWetTiltDb * 0.5f;
    computeLoShelf (mWetLoShelf, mSampleRate, 250.0f, 0.7071f, -halfDb);
    computeHiShelf (mWetHiShelf, mSampleRate, 4000.0f, 0.7071f, +halfDb);
}

//──────────────────────────────────────────────────────────────────────────────
// DSP helpers
//──────────────────────────────────────────────────────────────────────────────

// Schroeder allpass: v[n] = x - g*v[n-D], y[n] = v[n-D] + g*v[n]
float ReverbDSP::processAP (float x, std::vector<float>& buf, int len, int& wp, float g)
{
    const float delayed = buf[(size_t) wp];
    const float v = x - g * delayed;
    buf[(size_t) wp] = v;
    if (++wp >= len) wp = 0;
    return delayed + g * v;
}

// Normalised fast Walsh-Hadamard transform (in-place, N=8).
void ReverbDSP::applyH8 (std::array<float, kN>& v)
{
    float t;
    t=v[0]; v[0]=t+v[1]; v[1]=t-v[1];
    t=v[2]; v[2]=t+v[3]; v[3]=t-v[3];
    t=v[4]; v[4]=t+v[5]; v[5]=t-v[5];
    t=v[6]; v[6]=t+v[7]; v[7]=t-v[7];
    t=v[0]; v[0]=t+v[2]; v[2]=t-v[2];
    t=v[1]; v[1]=t+v[3]; v[3]=t-v[3];
    t=v[4]; v[4]=t+v[6]; v[6]=t-v[6];
    t=v[5]; v[5]=t+v[7]; v[7]=t-v[7];
    t=v[0]; v[0]=t+v[4]; v[4]=t-v[4];
    t=v[1]; v[1]=t+v[5]; v[5]=t-v[5];
    t=v[2]; v[2]=t+v[6]; v[6]=t-v[6];
    t=v[3]; v[3]=t+v[7]; v[7]=t-v[7];
    static constexpr float kNorm = 1.0f / 2.8284271247f;  // 1/sqrt(8)
    for (auto& x : v) x *= kNorm;
}

// §8a: 4-point Catmull-Rom cubic interpolation from a circular buffer.
// Matches the helpers used in ChorusDSP / DelayDSP / FlangerDSP.
float ReverbDSP::cubicInterpCircular (const std::vector<float>& buf, float pos, int size)
{
    if (size <= 0) return 0.0f;
    while (pos < 0.0f)             pos += (float) size;
    while (pos >= (float) size)    pos -= (float) size;

    const int   i1 = (int) pos;
    const float fr = pos - (float) i1;
    auto wrap = [size](int idx) {
        idx %= size;
        return idx < 0 ? idx + size : idx;
    };
    const float y0 = buf[(size_t) wrap (i1 - 1)];
    const float y1 = buf[(size_t) wrap (i1    )];
    const float y2 = buf[(size_t) wrap (i1 + 1)];
    const float y3 = buf[(size_t) wrap (i1 + 2)];

    const float a = -0.5f*y0 + 1.5f*y1 - 1.5f*y2 + 0.5f*y3;
    const float b =        y0 - 2.5f*y1 + 2.0f*y2 - 0.5f*y3;
    const float c = -0.5f*y0 + 0.5f*y2;
    return ((a*fr + b)*fr + c)*fr + y1;
}

// §8a + §8f: LFO value (-1..+1) based on shape selector.
//   shape 0 = sine
//   shape 1 = triangle
//   shape 2 = random S&H (updates `sh` each 2π wrap)
float ReverbDSP::tailLFOVal (int shape, float phaseRad, float& sh)
{
    if (shape == 1)
    {
        // Triangle: -1..+1 over 0..2π
        const float p = phaseRad / juce::MathConstants<float>::twoPi;   // 0..1
        return 4.0f * std::abs (p - std::round (p)) - 1.0f;
    }
    if (shape == 2)
    {
        // Random sample-and-hold: caller resets `sh` to a new random value
        // when phase wraps (handled outside this fn for cheaper branching).
        return sh;
    }
    return std::sin (phaseRad);   // default sine
}

//──────────────────────────────────────────────────────────────────────────────
// Main process
//──────────────────────────────────────────────────────────────────────────────
void ReverbDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;

    // Flush-to-zero for denormals — 8×N FDN delay lines + per-line LPs + bass
    // shelves + pre-delay + allpass + wet-tone LP all hold IIR state that
    // can slowly decay into subnormal territory, where every float op takes
    // ~100× longer and the message thread starves. Keeps the CPU sane.
    juce::ScopedNoDenormals noDenormals;

    // H-9 stages B/D/E/F (2026-05-02): each algorithm has its own dedicated
    // DSP code path (own buffers, own topology, own decay shaping).  Hall is
    // the existing 8-line FDN body that follows below; the others branch
    // out here.  Each per-algorithm function returns true if it produced
    // output; false (zero-size buffers, not yet prepared) falls through to
    // the Hall path so we still emit something.
    //
    // H-9 (2026-05-02): all 5 algorithms live -- Plate (B) + Hall (existing
    // FDN, default) + Chamber (D) + Room (E) + VocalBooth (F).  Each runs
    // its own dedicated DSP code path with its own buffers + topology.
    switch (mAlgorithm)
    {
        case Algorithm::Plate:
            if (processPlate      (buffer)) return;
            break;
        case Algorithm::Chamber:
            if (processChamber    (buffer)) return;
            break;
        case Algorithm::Room:
            if (processRoom       (buffer)) return;
            break;
        case Algorithm::VocalBooth:
            if (processVocalBooth (buffer)) return;
            break;
        case Algorithm::Hall:
        default:
            break;
    }

    const int N  = buffer.getNumSamples();
    const int NC = buffer.getNumChannels();
    if (N == 0 || NC < 2) return;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    const int preSz = (int) mPreBufL.size();
    if (preSz == 0) return;

    // Pre-compute per-block constants
    static constexpr float kInvSqrt8 = 1.0f / 2.8284271247f;
    const float injectScale = kInvSqrt8;
    const float outputScale = kInvSqrt8;
    const float erGainLin   = juce::Decibels::decibelsToGain (mERdB);
    const float sepW        = mStereoSep / 100.0f;

    // §8a tail-mod constants
    const float sr            = (float) mSampleRate;
    const float modPhaseInc   = juce::MathConstants<float>::twoPi * mTailModRateHz / sr;
    const float modDepthSmp   = mTailModDepthMs * 0.001f * sr;
    const bool  modEnabled    = (modDepthSmp > 1e-4f);
    const int   tailShape     = mTailModShape;

    // §8e HF ratio (captured — applied per sample)
    const float hfRatio       = mHFRatio;

    // §8g wet-tone tilt: active only when the user has dialled the knob
    const bool wetTiltActive = (std::abs (mWetTiltDb) > 0.05f);

    // §8d ER tap precompute (24 taps per sample = heavy but only when ER > silence)
    const bool  erActive      = (mERdB > -59.9f);
    const float erNorm        = erGainLin / (float) (kN * kERTaps);

    // Small RNG for §8f random S&H path (audio-thread safe: static = one per call site)
    auto rngNext = []() -> float {
        static uint32_t s = 0x9E3779B9u;
        s = s * 1664525u + 1013904223u;
        return ((float) (s >> 8) / (float) 0x00FFFFFF) * 2.0f - 1.0f;   // -1..+1
    };

    // ── H-9 (2026-05-02) ducking pre-loop constants ─────────────────────────
    // Envelope follower runs on the SC source if a slot is picked, otherwise
    // falls back to self-side-chain (the dry input).  When the trigger
    // crosses threshold, the wet output is attenuated by Amount (the dry
    // signal is unaffected -- duck only pulls the reverb tail down).
    const float duckThreshLin = juce::Decibels::decibelsToGain (mDuckThresholdDb);
    const float duckAmt01     = juce::jlimit (0.0f, 1.0f, mDuckAmount * 0.01f);
    const float duckAtk       = std::exp (-1.0f / std::max (1.0f, mDuckAttackMs  * 0.001f * sr));
    const float duckRel       = std::exp (-1.0f / std::max (1.0f, mDuckReleaseMs * 0.001f * sr));
    auto* scBuf = getActiveSidechain();
    const float* scL = (scBuf && scBuf->getNumChannels() > 0)
                            ? scBuf->getReadPointer (0) : nullptr;
    const float* scR = (scBuf && scBuf->getNumChannels() > 1)
                            ? scBuf->getReadPointer (1) : scL;
    const int    scN = scBuf ? scBuf->getNumSamples() : 0;

    for (int s = 0; s < N; ++s)
    {
        const float origL = L[s], origR = R[s];

        // §8c freeze smoother — 0 (off) .. 1 (full freeze)
        const float frz        = mFreezeSmooth.getNextValue();
        const float inputScale = 1.0f - frz;            // input injection fades out

        // ── Processing mode: choose FDN input ───────────────────────────────
        float fdnInL, fdnInR;
        if (mMode == 1)
        {
            fdnInL = fdnInR = (origL + origR) * 0.5f;
        }
        else if (mMode == 2)
        {
            const float side = (origL - origR) * 0.5f;
            fdnInL =  side;
            fdnInR = -side;
        }
        else
        {
            fdnInL = origL;
            fdnInR = origR;
        }

        // ── Input HPF  y[n] = a*(y[n-1] + x[n] - x[n-1]) ───────────────────
        {
            const float hL = mInHPCoef * (mInHPStL + fdnInL - mInHPPrevL);
            mInHPPrevL = fdnInL; mInHPStL = hL; fdnInL = hL;
            const float hR = mInHPCoef * (mInHPStR + fdnInR - mInHPPrevR);
            mInHPPrevR = fdnInR; mInHPStR = hR; fdnInR = hR;
        }

        // ── Input LPF  y += alpha*(x-y) ─────────────────────────────────────
        mInLPStL += mInLPAlpha * (fdnInL - mInLPStL); fdnInL = mInLPStL;
        mInLPStR += mInLPAlpha * (fdnInR - mInLPStR); fdnInR = mInLPStR;

        // ── Pre-delay ────────────────────────────────────────────────────────
        {
            mPreBufL[(size_t) mPreWrite] = fdnInL;
            mPreBufR[(size_t) mPreWrite] = fdnInR;
            int rp = mPreWrite - mPreDelaySamples;
            if (rp < 0) rp += preSz;
            fdnInL = mPreBufL[(size_t) rp];
            fdnInR = mPreBufR[(size_t) rp];
            if (++mPreWrite >= preSz) mPreWrite = 0;
        }

        // ── Pre-diffusion allpass stages ────────────────────────────────────
        for (int k = 0; k < kDiff; ++k)
        {
            fdnInL = processAP (fdnInL, mAPBufL[(size_t) k], mAPLen[(size_t) k], mAPWrL[(size_t) k], mAPCoef);
            fdnInR = processAP (fdnInR, mAPBufR[(size_t) k], mAPLen[(size_t) k], mAPWrR[(size_t) k], mAPCoef);
        }

        // §8c: input scale folds into the inject-to-FDN scale
        const float scaledInL = fdnInL * injectScale * inputScale;
        const float scaledInR = fdnInR * injectScale * inputScale;

        // ── FDN: read delay outputs (at write-pos = oldest sample) ─────────
        //    §8a: if tail modulation active, read fractional position via
        //    cubic interpolation; otherwise direct integer read (zero-cost).
        std::array<float, kN> zL, zR;
        if (modEnabled)
        {
            for (int i = 0; i < kN; ++i)
            {
                const int lineLen = mFDNLen[(size_t) i];
                if (lineLen <= 0) { zL[(size_t) i] = zR[(size_t) i] = 0.0f; continue; }

                const float lfoVal = tailLFOVal (tailShape, mTailModPhase[(size_t) i], mTailModSH[(size_t) i]);
                const float offset = modDepthSmp * lfoVal;
                const float readPos = (float) mFDNWr[(size_t) i] + offset;

                zL[(size_t) i] = cubicInterpCircular (mFDNL[(size_t) i], readPos, lineLen);
                zR[(size_t) i] = cubicInterpCircular (mFDNR[(size_t) i], readPos, lineLen);

                // Advance mod phase (and refresh S&H on wrap for §8f shape=2)
                mTailModPhase[(size_t) i] += modPhaseInc;
                if (mTailModPhase[(size_t) i] >= juce::MathConstants<float>::twoPi)
                {
                    mTailModPhase[(size_t) i] -= juce::MathConstants<float>::twoPi;
                    if (tailShape == 2) mTailModSH[(size_t) i] = rngNext();
                }
            }
        }
        else
        {
            for (int i = 0; i < kN; ++i)
            {
                zL[(size_t) i] = mFDNL[(size_t) i][(size_t) mFDNWr[(size_t) i]];
                zR[(size_t) i] = mFDNR[(size_t) i][(size_t) mFDNWr[(size_t) i]];
            }
        }

        // §8d: multi-tap early reflections (3 taps per line at prime-ratio
        // positions, natural decay envelope; ER taps stay as integer reads —
        // per spec, early reflections should keep crisp pre-echo character).
        float erL = 0.0f, erR = 0.0f;
        if (erActive)
        {
            for (int i = 0; i < kN; ++i)
            {
                const int lineLen = mFDNLen[(size_t) i];
                if (lineLen <= 0) continue;
                for (int t = 0; t < kERTaps; ++t)
                {
                    const int tapSamples = std::max (1, (int) ((float) lineLen * kERTapFrac[t]));
                    int erRp = mFDNWr[(size_t) i] - tapSamples;
                    if (erRp < 0) erRp += lineLen;
                    erL += mFDNL[(size_t) i][(size_t) erRp] * kERTapGain[t];
                    erR += mFDNR[(size_t) i][(size_t) erRp] * kERTapGain[t];
                }
            }
            erL *= erNorm;
            erR *= erNorm;
        }

        // ── Hadamard mix (feedback path only — output uses raw zL/zR) ───────
        std::array<float, kN> fL = zL, fR = zR;
        applyH8 (fL);
        applyH8 (fR);

        // §8c: when freeze is engaged, neutralize HF damp + bass shelf so the
        // held tail doesn't drift spectrally or pump bass.
        const float effLPAlpha    = mLPAlpha    * inputScale;   // dampen → 0 as freeze → 1
        const float effBassAddGain = mBassGainAdd * inputScale;

        // ── Per-line feedback processing + write back ────────────────────────
        for (int i = 0; i < kN; ++i)
        {
            // Feedback gain (lerped toward 1.0 under freeze — §8c)
            const float effGain = mFeedGain[(size_t) i] + (1.0f - mFeedGain[(size_t) i]) * frz;

            // §8e HF damping + HF ratio — split into LP (lows) + residual (highs).
            //  - Under freeze: HF ratio lerps to 1.0 so the held tail preserves
            //    its full spectrum (otherwise residual·hfRatio would darken the
            //    frozen content over time).
            //  - Runaway safety: residual's loop gain (effGain × effHFRatio)
            //    must stay ≤ 0.9995 or feedback explodes when user dials
            //    hfRatio > 1/effGain. Clamp per-line.
            if (!mHighDampBypass)
            {
                const float hfRatioFrz = hfRatio + (1.0f - hfRatio) * frz;
                const float maxHFRatio = (frz >= 0.999f) ? 1.0f
                                                          : (0.9995f / std::max (0.001f, effGain));
                const float effHFRatio = std::min (hfRatioFrz, maxHFRatio);

                mLPStL[(size_t) i] += effLPAlpha * (fL[(size_t) i] - mLPStL[(size_t) i]);
                const float residualL = fL[(size_t) i] - mLPStL[(size_t) i];
                fL[(size_t) i] = mLPStL[(size_t) i] + residualL * effHFRatio;

                mLPStR[(size_t) i] += effLPAlpha * (fR[(size_t) i] - mLPStR[(size_t) i]);
                const float residualR = fR[(size_t) i] - mLPStR[(size_t) i];
                fR[(size_t) i] = mLPStR[(size_t) i] + residualR * effHFRatio;
            }

            // Bass shelf: 1-pole LP blend amplifies lows (gain scaled by freeze)
            mBassStL[(size_t) i] = mBassStL[(size_t) i] * mBassLPCoef + fL[(size_t) i] * (1.0f - mBassLPCoef);
            fL[(size_t) i]       = fL[(size_t) i] + effBassAddGain * mBassStL[(size_t) i];

            mBassStR[(size_t) i] = mBassStR[(size_t) i] * mBassLPCoef + fR[(size_t) i] * (1.0f - mBassLPCoef);
            fR[(size_t) i]       = fR[(size_t) i] + effBassAddGain * mBassStR[(size_t) i];

            // Apply feedback gain (computed above)
            fL[(size_t) i] *= effGain;
            fR[(size_t) i] *= effGain;

            // Write back with scaled input injection (scaledIn*=frz already zeros when frozen)
            mFDNL[(size_t) i][(size_t) mFDNWr[(size_t) i]] = fL[(size_t) i] + scaledInL;
            mFDNR[(size_t) i][(size_t) mFDNWr[(size_t) i]] = fR[(size_t) i] + scaledInR;

            // Advance write pointer (respects mFDNLen[i], not buffer capacity — §8b)
            if (++mFDNWr[(size_t) i] >= mFDNLen[(size_t) i])
                mFDNWr[(size_t) i] = 0;
        }

        // ── FDN output: sum raw delay taps + early reflections ───────────────
        float wetL = 0.0f, wetR = 0.0f;
        for (int i = 0; i < kN; ++i) { wetL += zL[(size_t) i]; wetR += zR[(size_t) i]; }
        wetL = wetL * outputScale + erL;
        wetR = wetR * outputScale + erR;

        // §8g wet-tone tilt: cascaded low-shelf (250 Hz) + high-shelf (4 kHz)
        // biquad pair. Provably stable for valid coefs (updated via setWetTone).
        if (wetTiltActive)
        {
            wetL = mWetHiShelf.processL (mWetLoShelf.processL (wetL));
            wetR = mWetHiShelf.processR (mWetLoShelf.processR (wetR));
        }

        // ── Stereo separation: M/S width on wet signal ───────────────────────
        {
            const float wetMid  = (wetL + wetR) * 0.5f;
            const float wetSide = (wetL - wetR) * 0.5f * sepW;
            wetL = wetMid + wetSide;
            wetR = wetMid - wetSide;
        }

        // ── Dry + wet blend ──────────────────────────────────────────────────
        // H-9 ducking: SC source if picked, else self-side-chain on the dry
        // input.  Attenuates the wet only -- dry stays at full level so the
        // user hears the lead through the duck.  duckScalar = 1.0 when amount
        // = 0 or trigger is below threshold (back-compat, no audible change).
        const float trigL = (scL && s < scN) ? scL[s] : origL;
        const float trigR = (scR && s < scN) ? scR[s] : origR;
        const float trigPk = std::max (std::abs (trigL), std::abs (trigR));
        const float coef   = (trigPk > mDuckEnv) ? duckAtk : duckRel;
        mDuckEnv = trigPk + (mDuckEnv - trigPk) * coef;
        const float overshoot  = std::max (0.0f, mDuckEnv - duckThreshLin);
        const float duckScalar = (duckAmt01 > 0.0f && overshoot > 0.0f)
                                    ? juce::jmax (0.0f, 1.0f - duckAmt01
                                          * juce::jmin (1.0f, overshoot / duckThreshLin))
                                    : 1.0f;
        L[s] = origL * mDry + wetL * mWet * duckScalar;
        R[s] = origR * mDry + wetR * mWet * duckScalar;
    }
}

//──────────────────────────────────────────────────────────────────────────────
// Setters — CPU-guarded (R1). Audit fixes R2 (ER clamp), R7 (mode clamp).
//──────────────────────────────────────────────────────────────────────────────
void ReverbDSP::setProcessingMode (int m)
{
    const int n = juce::jlimit (0, 2, m);   // R7
    if (n != mMode) mMode = n;
}

void ReverbDSP::setTempoSync (bool on)
{
    if (on != mTempoSync) { mTempoSync = on; updatePreDelay(); }
}

void ReverbDSP::setHighDampBypass (bool b)
{
    if (b != mHighDampBypass) mHighDampBypass = b;
}

void ReverbDSP::setDry (float v)
{
    const float n = juce::jlimit (0.0f, 1.0f, v);
    if (n != mDry) mDry = n;
}

void ReverbDSP::setWet (float v)
{
    const float n = juce::jlimit (0.0f, 1.0f, v);
    if (n != mWet) mWet = n;
}

void ReverbDSP::setStereoSep (float p)
{
    const float n = juce::jlimit (0.0f, 200.0f, p);
    if (n != mStereoSep) mStereoSep = n;
}

void ReverbDSP::setER (float d)
{
    const float n = juce::jlimit (-60.0f, 12.0f, d);   // R2
    if (n != mERdB) mERdB = n;
}

void ReverbDSP::setLowCut (float hz)
{
    const float n = std::max (1.0f, hz);
    if (n != mLowCutHz) { mLowCutHz = n; updateInputFilters(); }
}

void ReverbDSP::setHighCut (float hz)
{
    const float n = std::max (100.0f, hz);
    if (n != mHighCutHz) { mHighCutHz = n; updateInputFilters(); }
}

void ReverbDSP::setPreDelay (float ms)
{
    const float n = juce::jlimit (0.0f, kMaxPreDelayMs, ms);
    if (n != mPreDelayMs) { mPreDelayMs = n; updatePreDelay(); }
}

void ReverbDSP::setRoomSize (float v)
{
    const float n = std::max (0.05f, v);
    if (n != mRoomSize) { mRoomSize = n; updateDelayLines(); }
}

void ReverbDSP::setDiffusion (float v)
{
    const float n = juce::jlimit (0.0f, 1.0f, v);
    if (n != mDiffusion) { mDiffusion = n; mAPCoef = mDiffusion * 0.7f; }
}

void ReverbDSP::setDecay (float sec)
{
    const float n = std::max (0.01f, sec);
    if (n != mDecay) { mDecay = n; updateFeedback(); }
}

void ReverbDSP::setBassMult (float v)
{
    const float n = std::max (0.1f, v);
    if (n != mBassMult) { mBassMult = n; mBassGainAdd = mBassMult - 1.0f; updateFeedback(); }
}

void ReverbDSP::setBassCrossover (float hz)
{
    const float n = std::max (20.0f, hz);
    if (n != mBassCrossHz) { mBassCrossHz = n; updateFeedback(); }
}

void ReverbDSP::setHighDamp (float hz)
{
    const float n = std::max (20.0f, hz);
    if (n != mHighDampHz) { mHighDampHz = n; updateInputFilters(); }
}

// ── §8 new setters ────────────────────────────────────────────────────────────
void ReverbDSP::setTailModDepth (float ms)
{
    const float n = juce::jlimit (0.0f, 1.5f, ms);
    if (n != mTailModDepthMs) mTailModDepthMs = n;
}

void ReverbDSP::setTailModRate (float hz)
{
    const float n = juce::jlimit (0.05f, 2.0f, hz);
    if (n != mTailModRateHz) mTailModRateHz = n;
}

void ReverbDSP::setTailModShape (int shape)
{
    const int n = juce::jlimit (0, 2, shape);
    if (n != mTailModShape) mTailModShape = n;
}

void ReverbDSP::setFreeze (bool on)
{
    if (on != mFreeze)
    {
        mFreeze = on;
        mFreezeSmooth.setTargetValue (on ? 1.0f : 0.0f);
    }
}

void ReverbDSP::setHFRatio (float ratio)
{
    const float n = juce::jlimit (0.3f, 2.0f, ratio);
    if (n != mHFRatio) mHFRatio = n;
}

void ReverbDSP::setWetTone (float dB)
{
    const float n = juce::jlimit (-12.0f, 12.0f, dB);
    if (n != mWetTiltDb) { mWetTiltDb = n; updateWetTone(); }
}

void ReverbDSP::setHostBPM (double bpm)
{
    if (bpm == mHostBPM) return;
    mHostBPM = bpm;
    // R3: if sync is on, the pre-delay snap now uses the new BPM — refresh.
    if (mTempoSync) updatePreDelay();
}

// ── Legacy setters ────────────────────────────────────────────────────────────
void ReverbDSP::setSize (float v)
{
    setRoomSize (juce::jlimit (0.05f, 2.0f, v));
}

void ReverbDSP::setDamp (float v)
{
    const float hz = 20000.0f * std::pow (0.008f, juce::jlimit (0.0f, 1.0f, v));
    setHighDamp (hz);
}

void ReverbDSP::setErLevel (float v)
{
    const float clamped = juce::jlimit (0.0f, 1.0f, v);
    setER (clamped > 1e-5f ? juce::Decibels::gainToDecibels (clamped) : -60.0f);
}

//──────────────────────────────────────────────────────────────────────────────
// H-9 (2026-05-02) — Algorithm umbrella + ducking + tempo-sync setters
//──────────────────────────────────────────────────────────────────────────────
void ReverbDSP::setAlgorithm (int algo)
{
    const Algorithm newA = (Algorithm) juce::jlimit (0, 4, algo);
    if (newA == mAlgorithm) return;
    mAlgorithm = newA;
    // Each algorithm runs its own dedicated DSP code path with its own
    // buffers + state.  reset() clears everything so a switch doesn't leak
    // tail energy from the previous algorithm into the new one.
    reset();
}

void ReverbDSP::setDuckAmount (float pct)
{
    const float v = juce::jlimit (0.0f, 100.0f, pct);
    if (juce::approximatelyEqual (v, mDuckAmount)) return;
    mDuckAmount = v;
}

void ReverbDSP::setDuckThresholdDb (float db)
{
    const float v = juce::jlimit (-60.0f, 0.0f, db);
    if (juce::approximatelyEqual (v, mDuckThresholdDb)) return;
    mDuckThresholdDb = v;
}

void ReverbDSP::setDuckAttackMs (float ms)
{
    const float v = juce::jlimit (1.0f, 200.0f, ms);
    if (juce::approximatelyEqual (v, mDuckAttackMs)) return;
    mDuckAttackMs = v;
}

void ReverbDSP::setDuckReleaseMs (float ms)
{
    const float v = juce::jlimit (10.0f, 1000.0f, ms);
    if (juce::approximatelyEqual (v, mDuckReleaseMs)) return;
    mDuckReleaseMs = v;
}

void ReverbDSP::setSyncDivision (int divIdx)
{
    const int v = juce::jlimit (0, kNumSyncDivisions - 1, divIdx);
    if (v == mSyncDivIdx) return;
    mSyncDivIdx = v;
    // H-9 fix (2026-05-02): division change must propagate to the active
    // pre-delay length when tempo-sync is engaged.
    if (mTempoSync) updatePreDelay();
}

//──────────────────────────────────────────────────────────────────────────────
// State serialisation
//──────────────────────────────────────────────────────────────────────────────
void ReverbDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree s ("ReverbDSP2");
    s.setProperty ("mode",            mMode,                nullptr);
    s.setProperty ("lowCut",          mLowCutHz,            nullptr);
    s.setProperty ("highCut",         mHighCutHz,           nullptr);
    s.setProperty ("preDelayMs",      mPreDelayMs,          nullptr);
    s.setProperty ("tempoSync",       (int) mTempoSync,     nullptr);
    s.setProperty ("roomSize",        mRoomSize,            nullptr);
    s.setProperty ("diffusion",       mDiffusion,           nullptr);
    s.setProperty ("decay",           mDecay,               nullptr);
    s.setProperty ("bassMult",        mBassMult,            nullptr);
    s.setProperty ("bassCross",       mBassCrossHz,         nullptr);
    s.setProperty ("highDamp",        mHighDampHz,          nullptr);
    s.setProperty ("highDampByp",     (int) mHighDampBypass,nullptr);
    s.setProperty ("erDB",            mERdB,                nullptr);
    s.setProperty ("stereoSep",       mStereoSep,           nullptr);
    s.setProperty ("dry",             mDry,                 nullptr);
    s.setProperty ("wet",             mWet,                 nullptr);
    s.setProperty ("bypassed",        (int) bypassed,       nullptr);
    // §8 additions
    s.setProperty ("tailModDepthMs",  mTailModDepthMs,      nullptr);
    s.setProperty ("tailModRateHz",   mTailModRateHz,       nullptr);
    s.setProperty ("tailModShape",    mTailModShape,        nullptr);
    s.setProperty ("freeze",          (int) mFreeze,        nullptr);
    s.setProperty ("hfRatio",         mHFRatio,             nullptr);
    s.setProperty ("wetToneDb",       mWetTiltDb,           nullptr);
    // H-9 (2026-05-02): umbrella + ducking + sync index.  Pre-H-9 presets
    // load default to Algorithm::Hall + Duck=0 + SyncDiv=2 (1/4) which
    // preserves the prior FDN behavior bit-exact.
    s.setProperty ("algorithm",       (int) mAlgorithm,     nullptr);
    s.setProperty ("duckAmount",      mDuckAmount,          nullptr);
    s.setProperty ("duckThresholdDb", mDuckThresholdDb,     nullptr);
    s.setProperty ("duckAttackMs",    mDuckAttackMs,        nullptr);
    s.setProperty ("duckReleaseMs",   mDuckReleaseMs,       nullptr);
    s.setProperty ("syncDivIdx",      mSyncDivIdx,          nullptr);

    if (auto xml = std::unique_ptr<juce::XmlElement> (s.createXml()))
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void ReverbDSP::setStateInformation (const void* data, int sz)
{
    auto xml = std::unique_ptr<juce::XmlElement> (juce::AudioProcessor::getXmlFromBinary (data, sz));
    if (!xml) return;

    if (xml->hasTagName ("ReverbDSP2"))
    {
        juce::ValueTree s = juce::ValueTree::fromXml (*xml);
        mMode           = (int)  s.getProperty ("mode",           mMode);
        mLowCutHz       = (float)s.getProperty ("lowCut",         mLowCutHz);
        mHighCutHz      = (float)s.getProperty ("highCut",        mHighCutHz);
        mPreDelayMs     = (float)s.getProperty ("preDelayMs",     mPreDelayMs);
        mTempoSync      = (int)  s.getProperty ("tempoSync",      0) != 0;
        mRoomSize       = (float)s.getProperty ("roomSize",       mRoomSize);
        mDiffusion      = (float)s.getProperty ("diffusion",      mDiffusion);
        mDecay          = (float)s.getProperty ("decay",          mDecay);
        mBassMult       = (float)s.getProperty ("bassMult",       mBassMult);
        mBassCrossHz    = (float)s.getProperty ("bassCross",      mBassCrossHz);
        mHighDampHz     = (float)s.getProperty ("highDamp",       mHighDampHz);
        mHighDampBypass = (int)  s.getProperty ("highDampByp",    0) != 0;
        mERdB           = (float)s.getProperty ("erDB",           mERdB);
        mStereoSep      = (float)s.getProperty ("stereoSep",      mStereoSep);
        mDry            = (float)s.getProperty ("dry",            mDry);
        mWet            = (float)s.getProperty ("wet",            mWet);
        bypassed        = (int)  s.getProperty ("bypassed",       0) != 0;
        // §8 additions — missing attributes default to current (neutral) values
        mTailModDepthMs = (float)s.getProperty ("tailModDepthMs", mTailModDepthMs);
        mTailModRateHz  = (float)s.getProperty ("tailModRateHz",  mTailModRateHz);
        mTailModShape   = (int)  s.getProperty ("tailModShape",   mTailModShape);
        mFreeze         = (int)  s.getProperty ("freeze",         0) != 0;
        mHFRatio        = (float)s.getProperty ("hfRatio",        mHFRatio);
        mWetTiltDb      = (float)s.getProperty ("wetToneDb",      mWetTiltDb);
        // H-9 additions -- default to Hall + duck-disabled for pre-H-9 presets
        mAlgorithm      = (Algorithm) juce::jlimit (0, 4,
                              (int) s.getProperty ("algorithm", (int) mAlgorithm));
        mDuckAmount      = (float) s.getProperty ("duckAmount",       mDuckAmount);
        mDuckThresholdDb = (float) s.getProperty ("duckThresholdDb",  mDuckThresholdDb);
        mDuckAttackMs    = (float) s.getProperty ("duckAttackMs",     mDuckAttackMs);
        mDuckReleaseMs   = (float) s.getProperty ("duckReleaseMs",    mDuckReleaseMs);
        mSyncDivIdx      = (int)   s.getProperty ("syncDivIdx",       mSyncDivIdx);
    }
    else if (xml->hasTagName ("ReverbDSP"))
    {
        juce::ValueTree s = juce::ValueTree::fromXml (*xml);
        setSize      ((float)s.getProperty ("size",       0.6f));
        setDamp      ((float)s.getProperty ("damp",       0.5f));
        setWet       ((float)s.getProperty ("wet",        0.3f));
        setDry       (1.0f - (float)s.getProperty ("wet", 0.3f));
        setPreDelay  ((float)s.getProperty ("preDelayMs", 10.0f));
        setLowCut    ((float)s.getProperty ("lowCutHz",   80.0f));
        setErLevel   ((float)s.getProperty ("erLevel",    0.3f));
        setMsMode    ((int)  s.getProperty ("msMode",     0) != 0);
        bypassed     = (int) s.getProperty ("bypassed",   0) != 0;
    }
    else return;

    // R5: refresh coefs + clear filter state + sync freeze smoother so
    // preset load doesn't pop or glide.
    if (mSampleRate > 0.0)
    {
        updateInputFilters();
        updatePreDelay();
        updateDiffusion();
        updateDelayLines();
        updateWetTone();
        mFreezeSmooth.setCurrentAndTargetValue (mFreeze ? 1.0f : 0.0f);
        reset();   // zeros all FDN / AP / tilt / damp / bass state + preBuf
    }
}

//──────────────────────────────────────────────────────────────────────────────
// H-9 stage B (2026-05-02): Plate algorithm body.
//
// Schroeder allpass-cascade topology -- fundamentally different from the
// FDN.  Architecture:
//
//   input → input HP + LP filters → pre-delay
//         → 8 series allpass stages (dense diffusion = "shimmer")
//         → split to 4 parallel feedback combs (each with damping LPF)
//         → sum combs → wet output
//
// The allpass cascade smears transients into a dense reflection cloud
// without ringing (allpass coef ~0.6).  The 4 parallel combs then build
// up the steady-state tail; their feedback gain is computed from the
// user's Decay knob to hit the target RT60.  Per-comb LPFs scale with
// the user's Damping knob so highs roll off into the tail.
//
// Stereo: left and right channels run independent allpass + comb chains
// using slightly different delay times (the comb buffers are sized
// per-channel from the same prepare() values; we use the SAME tap
// positions but each channel's state is independent so they decorrelate
// naturally over the tail).  Stereo width is applied at the end via
// M/S encoding scaled by mStereoSep.
//
// Returns false if buffers aren't sized (prepare() didn't run yet) so
// process() can fall through to the FDN path as a safety net.
//──────────────────────────────────────────────────────────────────────────────
bool ReverbDSP::processPlate (juce::AudioBuffer<float>& buffer) noexcept
{
    const int N  = buffer.getNumSamples();
    const int NC = buffer.getNumChannels();
    if (N == 0 || NC < 2) return false;
    if (mPlateApBufL[0].empty() || mPlateCombBufL[0].empty()) return false;
    if (mPreBufL.empty()) return false;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    const float sr = (float) mSampleRate;

    // ── Per-comb feedback gain from user Decay ───────────────────────────
    // Schroeder feedback: g = 10^(-3 * delay_sec / RT60).  Plate-style
    // RT60s up to ~6 sec at full Decay.  mDecay is RT60 in seconds so we
    // can use it directly.
    const float rt60 = juce::jmax (0.05f, mDecay);
    std::array<float, kPlateCombs> combFb;
    for (int i = 0; i < kPlateCombs; ++i)
    {
        const float delaySec = (float) mPlateCombDelay[(size_t) i] / sr;
        combFb[(size_t) i] = std::pow (10.0f, -3.0f * delaySec / rt60);
        // clamp under 1.0 so feedback always decays (catch corner cases)
        combFb[(size_t) i] = juce::jmin (0.99f, combFb[(size_t) i]);
    }

    // Allpass coefficient from user Diffusion (0..1).  0.5 = standard
    // Schroeder; allow a bit more range to taste -- 0.3..0.8.
    const float apCoef = juce::jlimit (0.3f, 0.85f, 0.5f + (mDiffusion - 0.5f) * 0.6f);

    // Damping cutoff (per-comb 1-pole LP) -- maps mHighDampHz onto a
    // simple coefficient.  alpha = 1 - exp(-2*pi*fc/sr).
    const float dampHz = juce::jlimit (200.0f, 18000.0f, mHighDampHz);
    const float dampA  = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * dampHz / sr);

    // Pre-delay sample count (snap to integer; existing pre-delay buffer
    // size is large enough for kMaxPreDelayMs).
    const int   preSz   = (int) mPreBufL.size();
    int   preDelaySmp   = (int) (mPreDelayMs * 0.001f * sr);
    preDelaySmp         = juce::jlimit (0, preSz - 1, preDelaySmp);

    // Stereo width
    const float sepW = mStereoSep / 100.0f;

    // ── H-9 ducking constants (per-block) ───────────────────────────────
    const float duckThreshLin = juce::Decibels::decibelsToGain (mDuckThresholdDb);
    const float duckAmt01     = juce::jlimit (0.0f, 1.0f, mDuckAmount * 0.01f);
    const float duckAtk       = std::exp (-1.0f / std::max (1.0f, mDuckAttackMs  * 0.001f * sr));
    const float duckRel       = std::exp (-1.0f / std::max (1.0f, mDuckReleaseMs * 0.001f * sr));
    auto* scBuf = getActiveSidechain();
    const float* scL = (scBuf && scBuf->getNumChannels() > 0)
                            ? scBuf->getReadPointer (0) : nullptr;
    const float* scR = (scBuf && scBuf->getNumChannels() > 1)
                            ? scBuf->getReadPointer (1) : scL;
    const int    scN = scBuf ? scBuf->getNumSamples() : 0;

    for (int s = 0; s < N; ++s)
    {
        const float origL = L[s], origR = R[s];

        // ── Processing-mode input pick (matches FDN path) ───────────────
        float inL, inR;
        if (mMode == 1)
        {
            inL = inR = (origL + origR) * 0.5f;
        }
        else if (mMode == 2)
        {
            const float side = (origL - origR) * 0.5f;
            inL =  side; inR = -side;
        }
        else
        {
            inL = origL; inR = origR;
        }

        // ── Input HPF + LPF (reuse FDN-path filter coefs/state) ─────────
        {
            const float hL = mInHPCoef * (mInHPStL + inL - mInHPPrevL);
            const float hR = mInHPCoef * (mInHPStR + inR - mInHPPrevR);
            mInHPPrevL = inL; mInHPPrevR = inR;
            mInHPStL   = hL;  mInHPStR   = hR;
            inL = hL; inR = hR;

            mInLPStL += mInLPAlpha * (inL - mInLPStL);
            mInLPStR += mInLPAlpha * (inR - mInLPStR);
            inL = mInLPStL; inR = mInLPStR;
        }

        // ── Pre-delay ────────────────────────────────────────────────────
        mPreBufL[(size_t) mPreWrite] = inL;
        mPreBufR[(size_t) mPreWrite] = inR;
        const int preRead = (mPreWrite - preDelaySmp + preSz) % preSz;
        float plL = mPreBufL[(size_t) preRead];
        float plR = mPreBufR[(size_t) preRead];
        mPreWrite = (mPreWrite + 1) % preSz;

        // ── Allpass cascade (8 series stages) ───────────────────────────
        // Standard form: y = -ap*x + d ; d_new = x + ap*y
        for (int k = 0; k < kPlateAllpassStages; ++k)
        {
            const int len  = mPlateApDelay[(size_t) k];
            const int posK = mPlateApPos[(size_t) k];
            // L
            const float wdL = mPlateApBufL[(size_t) k][(size_t) posK];
            const float wL  = plL + apCoef * wdL;
            mPlateApBufL[(size_t) k][(size_t) posK] = wL;
            plL = wdL - apCoef * wL;
            // R
            const float wdR = mPlateApBufR[(size_t) k][(size_t) posK];
            const float wR  = plR + apCoef * wdR;
            mPlateApBufR[(size_t) k][(size_t) posK] = wR;
            plR = wdR - apCoef * wR;

            mPlateApPos[(size_t) k] = (posK + 1) % len;
        }

        // ── Parallel feedback combs (4 in parallel, sum at output) ──────
        float wetL = 0.0f, wetR = 0.0f;
        for (int i = 0; i < kPlateCombs; ++i)
        {
            const int len = mPlateCombDelay[(size_t) i];
            const int pos = mPlateCombPos[(size_t) i];
            // L
            const float dL = mPlateCombBufL[(size_t) i][(size_t) pos];
            mPlateCombFiltL[(size_t) i] += dampA * (dL - mPlateCombFiltL[(size_t) i]);
            const float fbL = mPlateCombFiltL[(size_t) i] * combFb[(size_t) i];
            mPlateCombBufL[(size_t) i][(size_t) pos] = plL + fbL;
            wetL += dL;
            // R
            const float dR = mPlateCombBufR[(size_t) i][(size_t) pos];
            mPlateCombFiltR[(size_t) i] += dampA * (dR - mPlateCombFiltR[(size_t) i]);
            const float fbR = mPlateCombFiltR[(size_t) i] * combFb[(size_t) i];
            mPlateCombBufR[(size_t) i][(size_t) pos] = plR + fbR;
            wetR += dR;

            mPlateCombPos[(size_t) i] = (pos + 1) % len;
        }
        // Normalize comb sum so 4 parallel paths don't blow level
        wetL *= 0.25f;
        wetR *= 0.25f;

        // ── Wet-tone tilt (reuse FDN-path biquads) ──────────────────────
        if (std::abs (mWetTiltDb) > 0.05f)
        {
            wetL = mWetHiShelf.processL (mWetLoShelf.processL (wetL));
            wetR = mWetHiShelf.processR (mWetLoShelf.processR (wetR));
        }

        // ── Stereo width (M/S decode) ───────────────────────────────────
        if (sepW < 0.999f || sepW > 1.001f)
        {
            const float wetMid  = (wetL + wetR) * 0.5f;
            const float wetSide = (wetL - wetR) * 0.5f * sepW;
            wetL = wetMid + wetSide;
            wetR = wetMid - wetSide;
        }

        // ── Ducking ─────────────────────────────────────────────────────
        const float trigL = (scL && s < scN) ? scL[s] : origL;
        const float trigR = (scR && s < scN) ? scR[s] : origR;
        const float trigPk = std::max (std::abs (trigL), std::abs (trigR));
        const float coef   = (trigPk > mDuckEnv) ? duckAtk : duckRel;
        mDuckEnv = trigPk + (mDuckEnv - trigPk) * coef;
        const float overshoot  = std::max (0.0f, mDuckEnv - duckThreshLin);
        const float duckScalar = (duckAmt01 > 0.0f && overshoot > 0.0f)
                                    ? juce::jmax (0.0f, 1.0f - duckAmt01
                                          * juce::jmin (1.0f, overshoot / duckThreshLin))
                                    : 1.0f;

        L[s] = origL * mDry + wetL * mWet * duckScalar;
        R[s] = origR * mDry + wetR * mWet * duckScalar;
    }

    return true;
}

//──────────────────────────────────────────────────────────────────────────────
// H-9 stage D (2026-05-02): Chamber algorithm body.
//
// Dattorro / Bricasti M7-style nested allpass chain.  4 series stages;
// each stage is an inner allpass whose damped output feeds an outer allpass.
// The nesting + per-stage HF damping produces the dense, smooth chamber
// character that an FDN cannot fake via parameter tuning.  Independent
// L/R buffers + LP states for natural stereo.
//
// Decay control: mDecay maps onto outer-allpass coefficient (longer RT60
// = higher coef = longer ringing).  Damping pulls the per-stage LP down.
// No tail modulation; the nested-allpass topology generates its own
// decorrelation via inner-vs-outer phase relationships.
//
// Returns false if buffers aren't sized.
//──────────────────────────────────────────────────────────────────────────────
bool ReverbDSP::processChamber (juce::AudioBuffer<float>& buffer) noexcept
{
    const int N  = buffer.getNumSamples();
    const int NC = buffer.getNumChannels();
    if (N == 0 || NC < 2) return false;
    if (mChOuterBufL[0].empty() || mChInnerBufL[0].empty()) return false;
    if (mPreBufL.empty()) return false;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    const float sr = (float) mSampleRate;

    // Map user Decay (0.05..6 sec) to outer coefficient (0.40..0.72).
    // Outer coef dominates RT60 in nested-allpass chamber topology.
    const float decayClamped = juce::jmax (0.05f, mDecay);
    const float decayNorm    = juce::jlimit (0.0f, 1.0f, (decayClamped - 0.05f) / 5.95f);
    const float outerCoef    = 0.40f + 0.32f * decayNorm;
    // Inner coef stays moderate (Dattorro's preferred ~0.5..0.65 range)
    const float innerCoef    = juce::jlimit (0.30f, 0.70f, 0.50f + (mDiffusion - 0.5f) * 0.40f);

    // Damping LP coefficient from mHighDampHz
    const float dampHz = juce::jlimit (200.0f, 18000.0f, mHighDampHz);
    const float dampA  = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * dampHz / sr);

    // Pre-delay
    const int preSz = (int) mPreBufL.size();
    int preDelaySmp = (int) (mPreDelayMs * 0.001f * sr);
    preDelaySmp     = juce::jlimit (0, preSz - 1, preDelaySmp);

    // Stereo width
    const float sepW = mStereoSep / 100.0f;

    // Ducking
    const float duckThreshLin = juce::Decibels::decibelsToGain (mDuckThresholdDb);
    const float duckAmt01     = juce::jlimit (0.0f, 1.0f, mDuckAmount * 0.01f);
    const float duckAtk       = std::exp (-1.0f / std::max (1.0f, mDuckAttackMs  * 0.001f * sr));
    const float duckRel       = std::exp (-1.0f / std::max (1.0f, mDuckReleaseMs * 0.001f * sr));
    auto* scBuf = getActiveSidechain();
    const float* scL = (scBuf && scBuf->getNumChannels() > 0) ? scBuf->getReadPointer (0) : nullptr;
    const float* scR = (scBuf && scBuf->getNumChannels() > 1) ? scBuf->getReadPointer (1) : scL;
    const int    scN = scBuf ? scBuf->getNumSamples() : 0;

    for (int s = 0; s < N; ++s)
    {
        const float origL = L[s], origR = R[s];

        // Mode pick
        float inL, inR;
        if (mMode == 1)      { inL = inR = (origL + origR) * 0.5f; }
        else if (mMode == 2) { const float side = (origL - origR) * 0.5f; inL = side; inR = -side; }
        else                 { inL = origL; inR = origR; }

        // Input HP+LP (reuse FDN coefs)
        {
            const float hL = mInHPCoef * (mInHPStL + inL - mInHPPrevL);
            const float hR = mInHPCoef * (mInHPStR + inR - mInHPPrevR);
            mInHPPrevL = inL; mInHPPrevR = inR;
            mInHPStL = hL;    mInHPStR = hR;
            inL = hL; inR = hR;
            mInLPStL += mInLPAlpha * (inL - mInLPStL);
            mInLPStR += mInLPAlpha * (inR - mInLPStR);
            inL = mInLPStL; inR = mInLPStR;
        }

        // Pre-delay
        mPreBufL[(size_t) mPreWrite] = inL;
        mPreBufR[(size_t) mPreWrite] = inR;
        const int preRead = (mPreWrite - preDelaySmp + preSz) % preSz;
        float chL = mPreBufL[(size_t) preRead];
        float chR = mPreBufR[(size_t) preRead];
        mPreWrite = (mPreWrite + 1) % preSz;

        // ── Nested allpass chain (4 stages) ────────────────────────────
        for (int k = 0; k < kChamberOuterStages; ++k)
        {
            const int oLen = mChOuterDelay[(size_t) k];
            const int iLen = mChInnerDelay[(size_t) k];

            // Inner allpass (L)
            {
                const float innerOut = mChInnerBufL[(size_t) k][(size_t) mChInnerPosL[(size_t) k]];
                const float innerIn  = chL - innerCoef * innerOut;
                const float innerY   = innerOut + innerCoef * innerIn;
                mChInnerBufL[(size_t) k][(size_t) mChInnerPosL[(size_t) k]] = innerIn;
                mChInnerPosL[(size_t) k] = (mChInnerPosL[(size_t) k] + 1) % iLen;

                // Damping LP on inner output
                mChInnerLPL[(size_t) k] += dampA * (innerY - mChInnerLPL[(size_t) k]);
                const float innerDamped = mChInnerLPL[(size_t) k];

                // Outer allpass
                const float outerOut = mChOuterBufL[(size_t) k][(size_t) mChOuterPosL[(size_t) k]];
                const float outerIn  = innerDamped - outerCoef * outerOut;
                const float outerY   = outerOut + outerCoef * outerIn;
                mChOuterBufL[(size_t) k][(size_t) mChOuterPosL[(size_t) k]] = outerIn;
                mChOuterPosL[(size_t) k] = (mChOuterPosL[(size_t) k] + 1) % oLen;
                chL = outerY;
            }
            // Inner allpass (R)
            {
                const float innerOut = mChInnerBufR[(size_t) k][(size_t) mChInnerPosR[(size_t) k]];
                const float innerIn  = chR - innerCoef * innerOut;
                const float innerY   = innerOut + innerCoef * innerIn;
                mChInnerBufR[(size_t) k][(size_t) mChInnerPosR[(size_t) k]] = innerIn;
                mChInnerPosR[(size_t) k] = (mChInnerPosR[(size_t) k] + 1) % iLen;

                mChInnerLPR[(size_t) k] += dampA * (innerY - mChInnerLPR[(size_t) k]);
                const float innerDamped = mChInnerLPR[(size_t) k];

                const float outerOut = mChOuterBufR[(size_t) k][(size_t) mChOuterPosR[(size_t) k]];
                const float outerIn  = innerDamped - outerCoef * outerOut;
                const float outerY   = outerOut + outerCoef * outerIn;
                mChOuterBufR[(size_t) k][(size_t) mChOuterPosR[(size_t) k]] = outerIn;
                mChOuterPosR[(size_t) k] = (mChOuterPosR[(size_t) k] + 1) % oLen;
                chR = outerY;
            }
        }

        float wetL = chL, wetR = chR;

        // Wet-tone tilt
        if (std::abs (mWetTiltDb) > 0.05f)
        {
            wetL = mWetHiShelf.processL (mWetLoShelf.processL (wetL));
            wetR = mWetHiShelf.processR (mWetLoShelf.processR (wetR));
        }

        // Stereo width
        if (sepW < 0.999f || sepW > 1.001f)
        {
            const float wetMid  = (wetL + wetR) * 0.5f;
            const float wetSide = (wetL - wetR) * 0.5f * sepW;
            wetL = wetMid + wetSide;
            wetR = wetMid - wetSide;
        }

        // Ducking
        const float trigL = (scL && s < scN) ? scL[s] : origL;
        const float trigR = (scR && s < scN) ? scR[s] : origR;
        const float trigPk = std::max (std::abs (trigL), std::abs (trigR));
        const float coef   = (trigPk > mDuckEnv) ? duckAtk : duckRel;
        mDuckEnv = trigPk + (mDuckEnv - trigPk) * coef;
        const float overshoot  = std::max (0.0f, mDuckEnv - duckThreshLin);
        const float duckScalar = (duckAmt01 > 0.0f && overshoot > 0.0f)
                                    ? juce::jmax (0.0f, 1.0f - duckAmt01
                                          * juce::jmin (1.0f, overshoot / duckThreshLin))
                                    : 1.0f;

        L[s] = origL * mDry + wetL * mWet * duckScalar;
        R[s] = origR * mDry + wetR * mWet * duckScalar;
    }

    return true;
}

//──────────────────────────────────────────────────────────────────────────────
// H-9 stage E (2026-05-02): Room algorithm body.
//
// Pure dense ER cloud (15 discrete reflections in a 7..151ms spread) +
// 4 parallel short Schroeder combs (17/23/29/37 ms) with per-comb damping
// LP in the feedback path.  No FDN.  Topology:
//
//   pre-delayed input → ER cloud (15 fixed taps, declining gain envelope)
//                     → 4 parallel damped Schroeder combs
//                     → wet = ER * 0.55 + combTail * 0.45
//
// Comb feedback gain set from user Decay so combs reach the user's RT60.
// Damping LP cutoff from user mHighDampHz.  ER provides the small-room
// pre-echo geometry that an FDN smears into late-reverb mush; the comb
// tail adds a short stochastic-decay layer behind it.
//
// Returns false if buffers aren't sized.
//──────────────────────────────────────────────────────────────────────────────
bool ReverbDSP::processRoom (juce::AudioBuffer<float>& buffer) noexcept
{
    const int N  = buffer.getNumSamples();
    const int NC = buffer.getNumChannels();
    if (N == 0 || NC < 2) return false;
    if (mRoomERBufL.empty() || mRoomCombBufL[0].empty()) return false;
    if (mPreBufL.empty()) return false;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    const float sr = (float) mSampleRate;

    // Per-comb feedback gain from user Decay -- aim for the user's RT60
    // across each comb's natural delay length.
    const float rt60 = juce::jmax (0.05f, mDecay);
    std::array<float, kRoomCombs> combFb;
    for (int i = 0; i < kRoomCombs; ++i)
    {
        const float delaySec = (float) mRoomCombDelay[(size_t) i] / sr;
        combFb[(size_t) i] = juce::jmin (0.95f, std::pow (10.0f, -3.0f * delaySec / rt60));
    }

    // Damping LP coefficient for combs
    const float dampHz = juce::jlimit (200.0f, 18000.0f, mHighDampHz);
    const float dampA  = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * dampHz / sr);

    // ER tap-sum normalization (sum of kRoomERTapGain values ≈ 7.61)
    constexpr float kERNorm = 1.0f / 7.61f;

    // Mix between ER cloud and comb tail (Diffusion knob biases toward tail)
    const float diffN     = juce::jlimit (0.0f, 1.0f, mDiffusion);
    const float erWeight  = 0.55f - 0.20f * (diffN - 0.5f);    // 0.65..0.45
    const float tailWeight = 0.45f + 0.20f * (diffN - 0.5f);   // 0.35..0.55

    const int preSz = (int) mPreBufL.size();
    int preDelaySmp = (int) (mPreDelayMs * 0.001f * sr);
    preDelaySmp     = juce::jlimit (0, preSz - 1, preDelaySmp);

    const float sepW = mStereoSep / 100.0f;

    // Ducking
    const float duckThreshLin = juce::Decibels::decibelsToGain (mDuckThresholdDb);
    const float duckAmt01     = juce::jlimit (0.0f, 1.0f, mDuckAmount * 0.01f);
    const float duckAtk       = std::exp (-1.0f / std::max (1.0f, mDuckAttackMs  * 0.001f * sr));
    const float duckRel       = std::exp (-1.0f / std::max (1.0f, mDuckReleaseMs * 0.001f * sr));
    auto* scBuf = getActiveSidechain();
    const float* scL = (scBuf && scBuf->getNumChannels() > 0) ? scBuf->getReadPointer (0) : nullptr;
    const float* scR = (scBuf && scBuf->getNumChannels() > 1) ? scBuf->getReadPointer (1) : scL;
    const int    scN = scBuf ? scBuf->getNumSamples() : 0;

    const int erBufLen = (int) mRoomERBufL.size();

    for (int s = 0; s < N; ++s)
    {
        const float origL = L[s], origR = R[s];

        // Mode pick
        float inL, inR;
        if (mMode == 1)      { inL = inR = (origL + origR) * 0.5f; }
        else if (mMode == 2) { const float side = (origL - origR) * 0.5f; inL = side; inR = -side; }
        else                 { inL = origL; inR = origR; }

        // Input HP+LP
        {
            const float hL = mInHPCoef * (mInHPStL + inL - mInHPPrevL);
            const float hR = mInHPCoef * (mInHPStR + inR - mInHPPrevR);
            mInHPPrevL = inL; mInHPPrevR = inR;
            mInHPStL = hL;    mInHPStR = hR;
            inL = hL; inR = hR;
            mInLPStL += mInLPAlpha * (inL - mInLPStL);
            mInLPStR += mInLPAlpha * (inR - mInLPStR);
            inL = mInLPStL; inR = mInLPStR;
        }

        // Pre-delay
        mPreBufL[(size_t) mPreWrite] = inL;
        mPreBufR[(size_t) mPreWrite] = inR;
        const int preRead = (mPreWrite - preDelaySmp + preSz) % preSz;
        const float roomInL = mPreBufL[(size_t) preRead];
        const float roomInR = mPreBufR[(size_t) preRead];
        mPreWrite = (mPreWrite + 1) % preSz;

        // ── ER cloud: write current, sum 15 taps with declining gain ───
        mRoomERBufL[(size_t) mRoomERPos] = roomInL;
        mRoomERBufR[(size_t) mRoomERPos] = roomInR;
        float erL = 0.0f, erR = 0.0f;
        for (int t = 0; t < kRoomERTaps; ++t)
        {
            int rp = mRoomERPos - mRoomERTapSamples[(size_t) t];
            if (rp < 0) rp += erBufLen;
            erL += mRoomERBufL[(size_t) rp] * kRoomERTapGain[t];
            erR += mRoomERBufR[(size_t) rp] * kRoomERTapGain[t];
        }
        if (++mRoomERPos >= erBufLen) mRoomERPos = 0;
        erL *= kERNorm;
        erR *= kERNorm;

        // ── 4-comb Schroeder tail (parallel; ER feeds them as input) ────
        float tailL = 0.0f, tailR = 0.0f;
        for (int i = 0; i < kRoomCombs; ++i)
        {
            const int len = mRoomCombDelay[(size_t) i];
            // L
            const float dL = mRoomCombBufL[(size_t) i][(size_t) mRoomCombPosL[(size_t) i]];
            mRoomCombLPL[(size_t) i] += dampA * (dL - mRoomCombLPL[(size_t) i]);
            mRoomCombBufL[(size_t) i][(size_t) mRoomCombPosL[(size_t) i]] =
                erL + mRoomCombLPL[(size_t) i] * combFb[(size_t) i];
            mRoomCombPosL[(size_t) i] = (mRoomCombPosL[(size_t) i] + 1) % len;
            tailL += dL;
            // R
            const float dR = mRoomCombBufR[(size_t) i][(size_t) mRoomCombPosR[(size_t) i]];
            mRoomCombLPR[(size_t) i] += dampA * (dR - mRoomCombLPR[(size_t) i]);
            mRoomCombBufR[(size_t) i][(size_t) mRoomCombPosR[(size_t) i]] =
                erR + mRoomCombLPR[(size_t) i] * combFb[(size_t) i];
            mRoomCombPosR[(size_t) i] = (mRoomCombPosR[(size_t) i] + 1) % len;
            tailR += dR;
        }
        tailL *= 0.25f;
        tailR *= 0.25f;

        float wetL = erL * erWeight + tailL * tailWeight;
        float wetR = erR * erWeight + tailR * tailWeight;

        // Wet-tone tilt
        if (std::abs (mWetTiltDb) > 0.05f)
        {
            wetL = mWetHiShelf.processL (mWetLoShelf.processL (wetL));
            wetR = mWetHiShelf.processR (mWetLoShelf.processR (wetR));
        }

        // Stereo width
        if (sepW < 0.999f || sepW > 1.001f)
        {
            const float wetMid  = (wetL + wetR) * 0.5f;
            const float wetSide = (wetL - wetR) * 0.5f * sepW;
            wetL = wetMid + wetSide;
            wetR = wetMid - wetSide;
        }

        // Ducking
        const float trigL = (scL && s < scN) ? scL[s] : origL;
        const float trigR = (scR && s < scN) ? scR[s] : origR;
        const float trigPk = std::max (std::abs (trigL), std::abs (trigR));
        const float coef   = (trigPk > mDuckEnv) ? duckAtk : duckRel;
        mDuckEnv = trigPk + (mDuckEnv - trigPk) * coef;
        const float overshoot  = std::max (0.0f, mDuckEnv - duckThreshLin);
        const float duckScalar = (duckAmt01 > 0.0f && overshoot > 0.0f)
                                    ? juce::jmax (0.0f, 1.0f - duckAmt01
                                          * juce::jmin (1.0f, overshoot / duckThreshLin))
                                    : 1.0f;

        L[s] = origL * mDry + wetL * mWet * duckScalar;
        R[s] = origR * mDry + wetR * mWet * duckScalar;
    }

    return true;
}

//──────────────────────────────────────────────────────────────────────────────
// H-9 stage F (2026-05-02): VocalBooth algorithm body.
//
// Tiny dedicated 4-line FDN.  Very short delays (309/397/487/661 samples
// at 44.1k → 7..15 ms scaled by SR), Hadamard 4×4 mix matrix in the
// feedback path, aggressive HF damping LP per line, NO tail modulation
// (vocal-safe -- pitch correction needs stable phase).  Independent
// buffers from the main 8-line Hall FDN.
//
// Decay control: per-line feedback gain from user mDecay.  Damping LP
// fixed-aggressive at ~mVbDampAlpha (low-cut in feedback so the tail
// stays dark and "small-room"-like).
//
// Returns false if buffers aren't sized.
//──────────────────────────────────────────────────────────────────────────────
bool ReverbDSP::processVocalBooth (juce::AudioBuffer<float>& buffer) noexcept
{
    const int N  = buffer.getNumSamples();
    const int NC = buffer.getNumChannels();
    if (N == 0 || NC < 2) return false;
    if (mVbFDNL[0].empty()) return false;
    if (mPreBufL.empty()) return false;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    const float sr = (float) mSampleRate;

    // Per-line feedback gain from user Decay
    const float rt60 = juce::jmax (0.05f, mDecay);
    std::array<float, kBoothFDN> feedGain;
    for (int i = 0; i < kBoothFDN; ++i)
    {
        const float delaySec = (float) mVbFDNLen[(size_t) i] / sr;
        feedGain[(size_t) i] = juce::jmin (0.95f, std::pow (10.0f, -3.0f * delaySec / rt60));
    }

    // Aggressive HF damping LP -- coefficient roughly tracks user's
    // mHighDampHz but biased lower (booth = closed, dark, eats HF).
    const float dampHz = juce::jlimit (1500.0f, 9000.0f, mHighDampHz * 0.55f);
    const float dampA  = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * dampHz / sr);

    const int preSz = (int) mPreBufL.size();
    int preDelaySmp = (int) (mPreDelayMs * 0.001f * sr);
    preDelaySmp     = juce::jlimit (0, preSz - 1, preDelaySmp);

    const float sepW = mStereoSep / 100.0f;

    // Ducking
    const float duckThreshLin = juce::Decibels::decibelsToGain (mDuckThresholdDb);
    const float duckAmt01     = juce::jlimit (0.0f, 1.0f, mDuckAmount * 0.01f);
    const float duckAtk       = std::exp (-1.0f / std::max (1.0f, mDuckAttackMs  * 0.001f * sr));
    const float duckRel       = std::exp (-1.0f / std::max (1.0f, mDuckReleaseMs * 0.001f * sr));
    auto* scBuf = getActiveSidechain();
    const float* scL = (scBuf && scBuf->getNumChannels() > 0) ? scBuf->getReadPointer (0) : nullptr;
    const float* scR = (scBuf && scBuf->getNumChannels() > 1) ? scBuf->getReadPointer (1) : scL;
    const int    scN = scBuf ? scBuf->getNumSamples() : 0;

    // Inject scale -- 4-line FDN normalization
    constexpr float kInject = 0.5f;   // 1/sqrt(4)

    for (int s = 0; s < N; ++s)
    {
        const float origL = L[s], origR = R[s];

        // Mode pick
        float inL, inR;
        if (mMode == 1)      { inL = inR = (origL + origR) * 0.5f; }
        else if (mMode == 2) { const float side = (origL - origR) * 0.5f; inL = side; inR = -side; }
        else                 { inL = origL; inR = origR; }

        // Input HP+LP
        {
            const float hL = mInHPCoef * (mInHPStL + inL - mInHPPrevL);
            const float hR = mInHPCoef * (mInHPStR + inR - mInHPPrevR);
            mInHPPrevL = inL; mInHPPrevR = inR;
            mInHPStL = hL;    mInHPStR = hR;
            inL = hL; inR = hR;
            mInLPStL += mInLPAlpha * (inL - mInLPStL);
            mInLPStR += mInLPAlpha * (inR - mInLPStR);
            inL = mInLPStL; inR = mInLPStR;
        }

        // Pre-delay
        mPreBufL[(size_t) mPreWrite] = inL;
        mPreBufR[(size_t) mPreWrite] = inR;
        const int preRead = (mPreWrite - preDelaySmp + preSz) % preSz;
        const float bIn_L = mPreBufL[(size_t) preRead] * kInject;
        const float bIn_R = mPreBufR[(size_t) preRead] * kInject;
        mPreWrite = (mPreWrite + 1) % preSz;

        // ── Read 4 delay-line outputs (raw delayed samples = wet output) ─
        std::array<float, kBoothFDN> vL, vR;
        for (int i = 0; i < kBoothFDN; ++i)
        {
            vL[(size_t) i] = mVbFDNL[(size_t) i][(size_t) mVbFDNWrL[(size_t) i]];
            vR[(size_t) i] = mVbFDNR[(size_t) i][(size_t) mVbFDNWrR[(size_t) i]];
        }

        // ── Hadamard 4x4 mix on feedback path ────────────────────────────
        // H4 * v = [(v0+v1+v2+v3), (v0-v1+v2-v3), (v0+v1-v2-v3), (v0-v1-v2+v3)] * 0.5
        std::array<float, kBoothFDN> uL, uR;
        uL[0] = (vL[0] + vL[1] + vL[2] + vL[3]) * 0.5f;
        uL[1] = (vL[0] - vL[1] + vL[2] - vL[3]) * 0.5f;
        uL[2] = (vL[0] + vL[1] - vL[2] - vL[3]) * 0.5f;
        uL[3] = (vL[0] - vL[1] - vL[2] + vL[3]) * 0.5f;
        uR[0] = (vR[0] + vR[1] + vR[2] + vR[3]) * 0.5f;
        uR[1] = (vR[0] - vR[1] + vR[2] - vR[3]) * 0.5f;
        uR[2] = (vR[0] + vR[1] - vR[2] - vR[3]) * 0.5f;
        uR[3] = (vR[0] - vR[1] - vR[2] + vR[3]) * 0.5f;

        // ── Per-line: damp LP + feedback gain + input inject ─────────────
        for (int i = 0; i < kBoothFDN; ++i)
        {
            mVbFDNLPL[(size_t) i] += dampA * (uL[(size_t) i] - mVbFDNLPL[(size_t) i]);
            mVbFDNLPR[(size_t) i] += dampA * (uR[(size_t) i] - mVbFDNLPR[(size_t) i]);
            const float fbL = mVbFDNLPL[(size_t) i] * feedGain[(size_t) i];
            const float fbR = mVbFDNLPR[(size_t) i] * feedGain[(size_t) i];
            mVbFDNL[(size_t) i][(size_t) mVbFDNWrL[(size_t) i]] = bIn_L + fbL;
            mVbFDNR[(size_t) i][(size_t) mVbFDNWrR[(size_t) i]] = bIn_R + fbR;
            mVbFDNWrL[(size_t) i] = (mVbFDNWrL[(size_t) i] + 1) % mVbFDNLen[(size_t) i];
            mVbFDNWrR[(size_t) i] = (mVbFDNWrR[(size_t) i] + 1) % mVbFDNLen[(size_t) i];
        }

        // Wet output: sum 4 raw delay-tap outputs
        float wetL = (vL[0] + vL[1] + vL[2] + vL[3]) * 0.5f;   // 1/sqrt(4)
        float wetR = (vR[0] + vR[1] + vR[2] + vR[3]) * 0.5f;

        // Wet-tone tilt
        if (std::abs (mWetTiltDb) > 0.05f)
        {
            wetL = mWetHiShelf.processL (mWetLoShelf.processL (wetL));
            wetR = mWetHiShelf.processR (mWetLoShelf.processR (wetR));
        }

        // Stereo width
        if (sepW < 0.999f || sepW > 1.001f)
        {
            const float wetMid  = (wetL + wetR) * 0.5f;
            const float wetSide = (wetL - wetR) * 0.5f * sepW;
            wetL = wetMid + wetSide;
            wetR = wetMid - wetSide;
        }

        // Ducking
        const float trigL = (scL && s < scN) ? scL[s] : origL;
        const float trigR = (scR && s < scN) ? scR[s] : origR;
        const float trigPk = std::max (std::abs (trigL), std::abs (trigR));
        const float coef   = (trigPk > mDuckEnv) ? duckAtk : duckRel;
        mDuckEnv = trigPk + (mDuckEnv - trigPk) * coef;
        const float overshoot  = std::max (0.0f, mDuckEnv - duckThreshLin);
        const float duckScalar = (duckAmt01 > 0.0f && overshoot > 0.0f)
                                    ? juce::jmax (0.0f, 1.0f - duckAmt01
                                          * juce::jmin (1.0f, overshoot / duckThreshLin))
                                    : 1.0f;

        L[s] = origL * mDry + wetL * mWet * duckScalar;
        R[s] = origR * mDry + wetR * mWet * duckScalar;
    }

    return true;
}
