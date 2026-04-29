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

    // High-damping LP (in feedback path)
    mLPAlpha   = 1.0f - std::exp (-pi2 * std::max (20.0f, mHighDampHz) / sr);
}

void ReverbDSP::updateFeedback()
{
    if (mSampleRate <= 0.0) return;
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

void ReverbDSP::updatePreDelay()
{
    if (mSampleRate <= 0.0 || mPreBufL.empty()) return;
    float ms = mPreDelayMs;
    if (mTempoSync && mHostBPM > 0.0)
    {
        const float sixteenthMs = (float) (60000.0 / mHostBPM / 4.0);
        ms = std::round (ms / sixteenthMs) * sixteenthMs;
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
        L[s] = origL * mDry + wetL * mWet;
        R[s] = origR * mDry + wetR * mWet;
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
