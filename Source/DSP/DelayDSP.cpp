#include "DelayDSP.h"
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
DelayDSP::DelayDSP()
{
}

// ─────────────────────────────────────────────────────────────────────────────
void DelayDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;

    const int maxSamples = static_cast<int>(kMaxDelaySeconds * sampleRate) + 1;
    mLineL.assign(maxSamples, 0.0f);
    mLineR.assign(maxSamples, 0.0f);
    mWritePos = 0;

    // Allocate diffusion allpass buffers (each stage at its own max delay time)
    for (int s = 0; s < kDiffStages; ++s)
    {
        const int sz = static_cast<int>(kDiffBaseMs[s] * 0.001 * sampleRate) + 2;
        mDiffusion[s].stateL.assign(sz, 0.0f);
        mDiffusion[s].stateR.assign(sz, 0.0f);
        mDiffusion[s].writePos = 0;
    }

    mLFOPhase  = 0.0f;
    mLoFiPhase = 1.0f;  // forces capture on first sample
    mLoFiHoldL = mLoFiHoldR = 0.0f;

    recalcTargetDelay();
    mCurDelayL = mTargDelayL;
    mCurDelayR = mTargDelayR;

    // TPT feedback filter setup (replaces legacy Biquad2P mFBFilter)
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) juce::jmax(1, maxBlockSize);
    spec.numChannels      = 2;
    mFBTPT.prepare(spec);
    applyFBFilterType();
    mFBTPT.setCutoffFrequency(juce::jlimit(20.0f, 20000.0f, mFBCutoff));
    mFBTPT.setResonance      (juce::jlimit(0.1f, 20.0f,    mFBResonance));
    mFBTPT.reset();

    updateToneFilter();
    updateDiffusion();

    // Reset biquad state (tone filter on output path)
    mToneFilter.reset();

    // Reset DC-blocker state
    mDcBlockXL = mDcBlockYL = 0.0f;
    mDcBlockXR = mDcBlockYR = 0.0f;

    // A2 -- Arm SmoothedValue with spec-mandated 100 ms ramp.
    mDelayMsSmoothed.reset(sampleRate, 0.100);
    mDelayMsSmoothed.setCurrentAndTargetValue(delayMs);
}

// ─────────────────────────────────────────────────────────────────────────────
void DelayDSP::reset()
{
    std::fill(mLineL.begin(), mLineL.end(), 0.0f);
    std::fill(mLineR.begin(), mLineR.end(), 0.0f);
    mWritePos = 0;

    for (int s = 0; s < kDiffStages; ++s)
        mDiffusion[s].reset();

    mFBTPT.reset();
    mToneFilter.reset();

    mDcBlockXL = mDcBlockYL = 0.0f;
    mDcBlockXR = mDcBlockYR = 0.0f;

    mLoFiPhase = 1.0f;
    mLoFiHoldL = mLoFiHoldR = 0.0f;
    mDelayMsSmoothed.setCurrentAndTargetValue(delayMs);
}

// ─────────────────────────────────────────────────────────────────────────────
void DelayDSP::recalcTargetDelay()
{
    float ms = delayMs;

    if (syncBPM && mHostBPM > 0.0)
    {
        const double beatSec  = 60.0 / mHostBPM;
        const double noteFrac = static_cast<double>(syncNumerator) /
                                static_cast<double>(std::max(1, syncDenominator));
        ms = static_cast<float>(noteFrac * beatSec * 1000.0);
    }

    const float maxDelay = static_cast<float>(mLineL.empty() ? 1 : (int)mLineL.size() - 1);
    float samples = juce::jlimit(1.0f, maxDelay, ms * 0.001f * static_cast<float>(mSampleRate));

    // L/R spread: ±half of (stereoSpread * 10% of delay)
    const float spreadHalf = mStereoSpread * samples * 0.05f;
    // Pan offset: -1..1 maps to ±5% of delay
    const float panOffset  = mOffsetPan * samples * 0.05f;

    mTargDelayL = juce::jlimit(1.0f, maxDelay, samples - spreadHalf - panOffset);
    mTargDelayR = juce::jlimit(1.0f, maxDelay, samples + spreadHalf + panOffset);
}

// ─────────────────────────────────────────────────────────────────────────────
void DelayDSP::computeBiquadCoefs (Biquad2P& bq, double sr, float fc, float q, int type)
{
    if (type < 0 || type > 2 || sr <= 0.0 || fc <= 0.0f)
    {
        bq.a0=1.f; bq.a1=bq.a2=bq.b1=bq.b2=0.f;
        return;
    }

    const double w0    = juce::MathConstants<double>::twoPi * fc / sr;
    const double cosw0 = std::cos(w0);
    const double sinw0 = std::sin(w0);
    const double alpha = sinw0 / (2.0 * std::max(0.001, (double)q));

    double b0, b1, b2, a0, a1, a2;

    switch (type)
    {
        case 0: // LP
            b0 = (1.0 - cosw0) * 0.5;
            b1 =  1.0 - cosw0;
            b2 = (1.0 - cosw0) * 0.5;
            break;
        case 1: // HP
            b0 =  (1.0 + cosw0) * 0.5;
            b1 = -(1.0 + cosw0);
            b2 =  (1.0 + cosw0) * 0.5;
            break;
        default: // BP (0dB peak gain)
            b0 =  alpha;
            b1 =  0.0;
            b2 = -alpha;
            break;
    }

    a0 = 1.0 + alpha;
    a1 = -2.0 * cosw0;
    a2 =  1.0 - alpha;

    bq.a0 = static_cast<float>(b0 / a0);
    bq.a1 = static_cast<float>(b1 / a0);
    bq.a2 = static_cast<float>(b2 / a0);
    bq.b1 = static_cast<float>(a1 / a0);
    bq.b2 = static_cast<float>(a2 / a0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Push mFBFilterType → TPT filter type. Cutoff/resonance are set separately
// (and cutoff is modulated per-sample in process()).
void DelayDSP::applyFBFilterType()
{
    using FT = juce::dsp::StateVariableTPTFilterType;
    switch (mFBFilterType)
    {
        case 0: mFBTPT.setType(FT::lowpass);  break;
        case 1: mFBTPT.setType(FT::highpass); break;
        default: mFBTPT.setType(FT::bandpass); break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void DelayDSP::updateToneFilter()
{
    const float absT = std::abs(mTone);
    if (absT < 0.01f)
    {
        mToneFilter.a0 = 1.f;
        mToneFilter.a1 = mToneFilter.a2 = mToneFilter.b1 = mToneFilter.b2 = 0.f;
        return;
    }
    // Map |tone| 0..1 → cutoff 20kHz..200Hz (exponential)
    const float fc = 20000.0f * std::pow(200.0f / 20000.0f, absT);
    // tone > 0 → LP (cut highs), tone < 0 → HP (cut lows)
    const int type = (mTone > 0.0f) ? 0 : 1;
    computeBiquadCoefs(mToneFilter, mSampleRate, fc, 0.707f, type);
}

// ─────────────────────────────────────────────────────────────────────────────
void DelayDSP::updateDiffusion()
{
    if (mSampleRate <= 0.0) return;

    const float coef = mDiffLevel * 0.7f;

    for (int s = 0; s < kDiffStages; ++s)
    {
        const float stageMs  = kDiffBaseMs[s] * mDiffSpread;
        const int   maxSz    = static_cast<int>(mDiffusion[s].stateL.size());
        const int   delaySmp = juce::jlimit(1, std::max(1, maxSz - 1),
                                  static_cast<int>(stageMs * 0.001f * (float)mSampleRate));
        mDiffusion[s].delaySamples = delaySmp;
        mDiffusion[s].coef         = coef;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Linear interpolation from a circular buffer.
float DelayDSP::linInterp (const std::vector<float>& buf, float rpos, int size)
{
    if (size == 0) return 0.0f;

    const int   i0  = static_cast<int>(rpos) % size;
    const int   i1  = (i0 + 1) % size;
    const float frac = rpos - std::floor(rpos);

    return buf[i0] * (1.0f - frac) + buf[i1] * frac;
}

// ─────────────────────────────────────────────────────────────────────────────
// 4-point Catmull-Rom cubic interpolation from a circular buffer.
// Matches ChorusDSP::cubicInterp.
float DelayDSP::cubicInterp (const std::vector<float>& buf, float rpos, int size)
{
    if (size == 0) return 0.0f;

    const int   i1 = static_cast<int>(rpos);
    const float fr = rpos - static_cast<float>(i1);

    auto wrap = [size](int idx) -> int {
        idx %= size;
        return idx < 0 ? idx + size : idx;
    };

    const float y0 = buf[wrap(i1 - 1)];
    const float y1 = buf[wrap(i1    )];
    const float y2 = buf[wrap(i1 + 1)];
    const float y3 = buf[wrap(i1 + 2)];

    const float a = -0.5f*y0 + 1.5f*y1 - 1.5f*y2 + 0.5f*y3;
    const float b =        y0 - 2.5f*y1 + 2.0f*y2 - 0.5f*y3;
    const float c = -0.5f*y0 + 0.5f*y2;

    return ((a*fr + b)*fr + c)*fr + y1;
}

// ─────────────────────────────────────────────────────────────────────────────
float DelayDSP::softLimit (float x, float knee)
{
    if (knee < 0.001f)
        return juce::jlimit(-1.0f, 1.0f, x);

    const float absX  = std::abs(x);
    if (absX < 1e-9f) return x;

    const float sgn   = x > 0.0f ? 1.0f : -1.0f;
    const float thresh = 1.0f - knee * 0.5f;

    if (absX <= thresh) return x;
    if (absX >= 1.0f)   return sgn;

    // Smooth cubic Hermite in [thresh, 1.0]
    const float t = (absX - thresh) / (1.0f - thresh);
    return sgn * (thresh + (1.0f - thresh) * (t * t * (3.0f - 2.0f * t)));
}

// ─────────────────────────────────────────────────────────────────────────────
float DelayDSP::bitcrush (float x, float bits)
{
    if (bits >= 23.0f) return x;
    const float levels = std::pow(2.0f, bits) - 1.0f;
    if (levels < 1.0f) return x;
    return std::round(x * levels) / levels;
}

// ─────────────────────────────────────────────────────────────────────────────
void DelayDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;

    juce::ScopedNoDenormals noDenormals;  // A1

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels < 1 || mLineL.empty()) return;

    float* L = buffer.getWritePointer(0);
    float* R = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    const int lineSize = static_cast<int>(mLineL.size());

    // LFO increment per sample
    const float lfoInc = static_cast<float>(
        juce::MathConstants<double>::twoPi * mModRate / mSampleRate);

    // Slew coefficient for keep-pitch mode:
    // mSmoothing=0 -> fast (slewCoef~=0), mSmoothing=1 -> slow (~0.9997 at 48kHz)
    const float slewCoef = mKeepPitch
        ? std::exp(-1.0f / std::max(1.0f, mSmoothing * 0.5f * static_cast<float>(mSampleRate)))
        : 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = L[i];
        const float inR = R ? R[i] : inL;

        // ── LFO ─────────────────────────────────────────────────────────────
        const float lfoVal = std::sin(mLFOPhase);
        mLFOPhase += lfoInc;
        // A4 -- while-wrap (was single if; allowed single-frame overshoot at high rate / low SR).
        while (mLFOPhase >= juce::MathConstants<float>::twoPi)
            mLFOPhase -= juce::MathConstants<float>::twoPi;

        // A2 -- Pull smoothed delayMs and rebase targets each sample. The
        // underlying recalcTargetDelay() keeps `mTargDelayL/R` in sync with
        // the non-smoothed `delayMs`, but a knob drag calls setDelayMs()
        // which pushes into mDelayMsSmoothed -- we read the smoothed value
        // and scale the targets accordingly so the ramp is audible.
        const float smoothedMs = mDelayMsSmoothed.getNextValue();
        const float delayMsSafe = std::max(0.001f, delayMs);
        const float smoothScale  = smoothedMs / delayMsSafe;  // 1.0 at rest

        // ── Smooth delay time (keep pitch) ───────────────────────────────────
        const float maxD = static_cast<float>(lineSize - 1);

        float targL = juce::jlimit(1.0f, maxD,
            mTargDelayL * smoothScale + mModTimeMod * lfoVal * static_cast<float>(mSampleRate) * 0.01f);
        float targR = juce::jlimit(1.0f, maxD,
            mTargDelayR * smoothScale + mModTimeMod * lfoVal * static_cast<float>(mSampleRate) * 0.01f);

        if (mKeepPitch)
        {
            mCurDelayL = slewCoef * mCurDelayL + (1.0f - slewCoef) * targL;
            mCurDelayR = slewCoef * mCurDelayR + (1.0f - slewCoef) * targR;
        }
        else
        {
            mCurDelayL = targL;
            mCurDelayR = targR;
        }
        // A3 -- Defensive clamp: slewed values could in principle drift below 1
        // if prior targets hit the floor while mCurDelay was approaching.
        mCurDelayL = juce::jlimit(1.0f, maxD, mCurDelayL);
        mCurDelayR = juce::jlimit(1.0f, maxD, mCurDelayR);

        // ── Read from delay lines ────────────────────────────────────────────
        float rawReadL, rawReadR;
        {
            float rposL = static_cast<float>(mWritePos) - mCurDelayL;
            float rposR = static_cast<float>(mWritePos) - mCurDelayR;
            while (rposL < 0.0f) rposL += static_cast<float>(lineSize);
            while (rposR < 0.0f) rposR += static_cast<float>(lineSize);
            rposL = std::fmod(rposL, static_cast<float>(lineSize));
            rposR = std::fmod(rposR, static_cast<float>(lineSize));

            rawReadL = cubicInterp(mLineL, rposL, lineSize);
            rawReadR = cubicInterp(mLineR, rposR, lineSize);
        }

        // ── Feedback path ────────────────────────────────────────────────────
        float feedL = rawReadL;
        float feedR = rawReadR;

        // 1. Diffusion (allpass chain, only when diffLevel > 0)
        if (mDiffLevel > 0.001f)
        {
            for (int s = 0; s < kDiffStages; ++s)
                mDiffusion[s].processLR(feedL, feedR);
        }

        // 2. Lo-Fi (sample-rate reduction + bit crush)
        {
            mLoFiPhase += mLoFiRate / static_cast<float>(mSampleRate);
            if (mLoFiPhase >= 1.0f)
            {
                mLoFiPhase -= 1.0f;
                mLoFiHoldL = feedL;
                mLoFiHoldR = feedR;
            }
            if (mLoFiBits < 23.0f || mLoFiRate < static_cast<float>(mSampleRate) - 1.0f)
            {
                feedL = bitcrush(mLoFiHoldL, mLoFiBits);
                feedR = bitcrush(mLoFiHoldR, mLoFiBits);
            }
            else
            {
                feedL = mLoFiHoldL;
                feedR = mLoFiHoldR;
            }
        }

        // 3. Feedback filter (TPT SVF LP/HP/BP, per-sample modulated cutoff)
        //    mModCutoffMod scales cutoff by ±(mModCutoffMod) octaves (lfoVal is -1..1)
        {
            const float cutoffMod       = std::exp2(mModCutoffMod * lfoVal);
            const float modulatedCutoff = juce::jlimit(20.0f, 20000.0f, mFBCutoff * cutoffMod);
            mFBTPT.setCutoffFrequency(modulatedCutoff);
            feedL = mFBTPT.processSample(0, feedL);
            feedR = mFBTPT.processSample(1, feedR);
        }

        // 4. Feedback distortion
        if (mFBDistType == 0)
        {
            // Soft limiter
            feedL = softLimit(feedL, mFBDistKnee);
            feedR = softLimit(feedR, mFBDistKnee);
        }
        else
        {
            // Saturation: tanh with optional DC offset for asymmetry.
            // Normalize so small-signal gain == 1.0 (not drive/tanh(drive) which
            // is >1 for drive>0 and causes feedback runaway). Subtracting the
            // pre-computed DC-bias floor also removes the static DC injected by
            // Symmetry>0 in-place (the 5 Hz DC-blocker below catches LFO drift).
            const float dc         = mFBDistSymmetry * 0.3f;
            const float drive      = std::max(0.001f, mFBDistLevel);
            const float dcOutBias  = std::tanh(drive * dc);
            feedL = (std::tanh(drive * (feedL + dc)) - dcOutBias) / drive;
            feedR = (std::tanh(drive * (feedR + dc)) - dcOutBias) / drive;
        }

        // 4b. 5 Hz DC-blocker (post-distortion / pre-feedback-level).
        //     Removes the DC injected by mFBDistSymmetry in Sat mode so it does
        //     not compound per repeat and eat headroom. y[n] = x[n] - x[n-1] + R*y[n-1]
        {
            constexpr float R = 0.9995f;
            const float dcOutL = feedL - mDcBlockXL + R * mDcBlockYL;
            mDcBlockXL = feedL; mDcBlockYL = dcOutL; feedL = dcOutL;

            const float dcOutR = feedR - mDcBlockXR + R * mDcBlockYR;
            mDcBlockXR = feedR; mDcBlockYR = dcOutR; feedR = dcOutR;
        }

        // 5. Feedback level
        feedL *= mFeedbackLevel;
        feedR *= mFeedbackLevel;

        // ── Write to delay lines (depends on model) ──────────────────────────
        const float monoIn = (inL + inR) * 0.5f;

        switch (mDelayModel)
        {
            case 1: // Mono — L+R averaged into both lines
                mLineL[mWritePos] = monoIn * mWetIn + feedL;
                mLineR[mWritePos] = monoIn * mWetIn + feedR;
                break;

            case 2: // PingPong — L input feeds L line + R's feedback, and vice versa
                mLineL[mWritePos] = inL * mWetIn + feedR;
                mLineR[mWritePos] = inR * mWetIn + feedL;
                break;

            case 3: // Off — only feedback, no new input into delay
                mLineL[mWritePos] = feedL;
                mLineR[mWritePos] = feedR;
                break;

            default: // Stereo
                mLineL[mWritePos] = inL * mWetIn + feedL;
                mLineR[mWritePos] = inR * mWetIn + feedR;
                break;
        }

        // ── Output path ──────────────────────────────────────────────────────
        float outL = mToneFilter.processL(rawReadL);
        float outR = mToneFilter.processR(rawReadR);

        // Output limiter (protect when feedback > 1 causes buildup)
        outL = juce::jlimit(-1.0f, 1.0f, outL);
        outR = juce::jlimit(-1.0f, 1.0f, outR);

        // Final mix
        L[i] = inL * mDryOut + outL * mWetOut;
        if (R)
            R[i] = inR * mDryOut + outR * mWetOut;

        mWritePos = (mWritePos + 1) % lineSize;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// setHostBPM
// ─────────────────────────────────────────────────────────────────────────────
void DelayDSP::setHostBPM (double bpm)
{
    if (bpm == mHostBPM) return;
    mHostBPM = bpm;
    if (syncBPM) recalcTargetDelay();
}

// ─────────────────────────────────────────────────────────────────────────────
// New setters
// ─────────────────────────────────────────────────────────────────────────────
void DelayDSP::setWetIn     (float v)
{
    const float n = juce::jlimit(0.0f, 1.0f, v);
    if (n != mWetIn) mWetIn = n;
}
void DelayDSP::setTempoSync (bool en)
{
    if (en != syncBPM) { syncBPM = en; recalcTargetDelay(); }
}
void DelayDSP::setKeepPitch (bool en)
{
    if (en != mKeepPitch) mKeepPitch = en;
}
void DelayDSP::setSmoothing (float v)
{
    const float n = juce::jlimit(0.0f, 1.0f, v);
    if (n != mSmoothing) mSmoothing = n;
}
void DelayDSP::setOffsetPan (float v)
{
    const float n = juce::jlimit(-1.0f, 1.0f, v);
    if (n != mOffsetPan) { mOffsetPan = n; recalcTargetDelay(); }
}
void DelayDSP::setStereoSpread (float v)
{
    const float n = juce::jlimit(0.0f, 1.0f, v);
    if (n != mStereoSpread) { mStereoSpread = n; recalcTargetDelay(); }
}
void DelayDSP::setFeedbackLevel (float v)
{
    const float n = juce::jlimit(0.0f, 1.2f, v);
    if (n != mFeedbackLevel) { mFeedbackLevel = n; feedback = n; }
}
void DelayDSP::setLoFiSampleRate (float hz)
{
    const float n = std::max(100.0f, hz);
    if (n != mLoFiRate) mLoFiRate = n;
}
void DelayDSP::setLoFiBits (float b)
{
    const float n = juce::jlimit(1.0f, 24.0f, b);
    if (n != mLoFiBits) mLoFiBits = n;
}
void DelayDSP::setModRate (float hz)
{
    const float n = juce::jlimit(0.0f, 20.0f, hz);
    if (n != mModRate) mModRate = n;
}
void DelayDSP::setModTimeMod (float v)
{
    const float n = juce::jlimit(0.0f, 1.0f, v);
    if (n != mModTimeMod) mModTimeMod = n;
}
void DelayDSP::setModCutoffMod (float v)
{
    const float n = juce::jlimit(0.0f, 1.0f, v);
    if (n != mModCutoffMod) mModCutoffMod = n;
}
void DelayDSP::setFBDistType (int t)
{
    const int n = (t == 1) ? 1 : 0;
    if (n != mFBDistType) mFBDistType = n;
}
void DelayDSP::setFBDistKnee (float v)
{
    const float n = juce::jlimit(0.0f, 1.0f, v);
    if (n != mFBDistKnee) mFBDistKnee = n;
}
void DelayDSP::setFBDistSymmetry (float v)
{
    const float n = juce::jlimit(0.0f, 1.0f, v);
    if (n != mFBDistSymmetry) mFBDistSymmetry = n;
}
void DelayDSP::setFBDistLevel (float v)
{
    const float n = juce::jlimit(0.0f, 10.0f, v);
    if (n != mFBDistLevel) mFBDistLevel = n;
}
void DelayDSP::setWetOut (float v)
{
    const float n = juce::jlimit(0.0f, 1.0f, v);
    if (n != mWetOut) { mWetOut = n; wet = n; }
}
void DelayDSP::setDryOut (float v)
{
    const float n = juce::jlimit(0.0f, 1.0f, v);
    if (n != mDryOut) mDryOut = n;
}

void DelayDSP::setDelayModel (int m)
{
    const int n = juce::jlimit(0, 3, m);
    if (n != mDelayModel)
    {
        mDelayModel = n;
        pingPong    = (mDelayModel == 2);
    }
}

void DelayDSP::setDelayMs (float ms)
{
    // A5 -- Clamp to documented API range (1..2000 ms).
    const float n = juce::jlimit(1.0f, 2000.0f, ms);
    if (n == delayMs) return;
    delayMs = n;
    recalcTargetDelay();
    // A2 -- Target the smoothed value; process() ramps mTargDelay via smoothScale.
    mDelayMsSmoothed.setTargetValue(n);
    if (!mKeepPitch)
    {
        // Still set the non-smoothed "cur" values so keep-pitch-off reads stay
        // consistent if process() isn't called before the next knob update.
        // The smoother + smoothScale in process() deliver the audible ramp.
        mCurDelayL = mTargDelayL;
        mCurDelayR = mTargDelayR;
    }
}

void DelayDSP::setFeedbackFilterType (int t)
{
    const int n = juce::jlimit(0, 2, t);
    if (n != mFBFilterType)
    {
        mFBFilterType = n;
        applyFBFilterType();
    }
}

void DelayDSP::setFeedbackCutoff (float hz)
{
    const float n = std::max(20.0f, hz);
    if (n != mFBCutoff)
    {
        mFBCutoff = n;
        // Per-sample cutoff is pushed in process(); nothing else to do here.
    }
}

void DelayDSP::setFeedbackResonance (float q)
{
    const float n = std::max(0.1f, q);
    if (n != mFBResonance)
    {
        mFBResonance = n;
        mFBTPT.setResonance(juce::jlimit(0.1f, 20.0f, mFBResonance));
    }
}

void DelayDSP::setDiffusionLevel (float v)
{
    const float n = juce::jlimit(0.0f, 1.0f, v);
    if (n != mDiffLevel)
    {
        mDiffLevel = n;
        updateDiffusion();
    }
}

void DelayDSP::setDiffusionSpread (float v)
{
    const float n = juce::jlimit(0.0f, 1.0f, v);
    if (n != mDiffSpread)
    {
        mDiffSpread = n;
        updateDiffusion();
    }
}

void DelayDSP::setTone (float v)
{
    const float n = juce::jlimit(-1.0f, 1.0f, v);
    if (n != mTone)
    {
        mTone = n;
        updateToneFilter();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Legacy setters (backward compat — map to new parameters)
// ─────────────────────────────────────────────────────────────────────────────
void DelayDSP::setFeedback (float fb)
{
    const float n = juce::jlimit(0.0f, 1.2f, fb);
    if (n != feedback) { feedback = n; mFeedbackLevel = n; }
}

void DelayDSP::setWet (float w)
{
    const float n = juce::jlimit(0.0f, 1.0f, w);
    if (n != wet) { wet = n; mWetOut = n; }
}

void DelayDSP::setHpHz (float hz)
{
    const float n = std::max(1.0f, hz);
    if (n == hpHz && mFBFilterType == 1 && mFBCutoff == n) return;
    hpHz          = n;
    mFBFilterType = 1;   // HP
    mFBCutoff     = n;
    applyFBFilterType();
}

void DelayDSP::setLpHz (float hz)
{
    const float n = std::max(1.0f, hz);
    if (n == lpHz) return;
    lpHz = n;
    // Legacy behaviour: if lpHz < 7000, bias FB filter toward LP at that cutoff.
    if (lpHz < 7000.0f)
    {
        mFBFilterType = 0;   // LP
        mFBCutoff     = lpHz;
        applyFBFilterType();
    }
}

void DelayDSP::setPingPong (bool en)
{
    if (en == pingPong) return;
    pingPong    = en;
    mDelayModel = en ? 2 : 0;
}

void DelayDSP::setSyncBPM (bool en)
{
    if (en == syncBPM) return;
    syncBPM = en;
    recalcTargetDelay();
}

void DelayDSP::setSyncNote (int numerator, int denominator)
{
    const int num = std::max(1, numerator);
    const int den = std::max(1, denominator);
    if (num == syncNumerator && den == syncDenominator) return;
    syncNumerator   = num;
    syncDenominator = den;
    if (syncBPM) recalcTargetDelay();
}

// ─────────────────────────────────────────────────────────────────────────────
// Serialization
// ─────────────────────────────────────────────────────────────────────────────
void DelayDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state("DelayDSP2");

    // Legacy params
    state.setProperty("delayMs",         delayMs,          nullptr);
    state.setProperty("feedback",        feedback,         nullptr);
    state.setProperty("wet",             wet,              nullptr);
    state.setProperty("hpHz",            hpHz,             nullptr);
    state.setProperty("lpHz",            lpHz,             nullptr);
    state.setProperty("pingPong",        (int)pingPong,    nullptr);
    state.setProperty("syncBPM",         (int)syncBPM,     nullptr);
    state.setProperty("syncNumerator",   syncNumerator,    nullptr);
    state.setProperty("syncDenominator", syncDenominator,  nullptr);
    state.setProperty("bypassed",        (int)bypassed,    nullptr);

    // New params
    state.setProperty("wetIn",           mWetIn,           nullptr);
    state.setProperty("keepPitch",       (int)mKeepPitch,  nullptr);
    state.setProperty("smoothing",       mSmoothing,       nullptr);
    state.setProperty("offsetPan",       mOffsetPan,       nullptr);
    state.setProperty("delayModel",      mDelayModel,      nullptr);
    state.setProperty("stereoSpread",    mStereoSpread,    nullptr);
    state.setProperty("feedbackLevel",   mFeedbackLevel,   nullptr);
    state.setProperty("fbFilterType",    mFBFilterType,    nullptr);
    state.setProperty("fbCutoff",        mFBCutoff,        nullptr);
    state.setProperty("fbResonance",     mFBResonance,     nullptr);
    state.setProperty("loFiRate",        mLoFiRate,        nullptr);
    state.setProperty("loFiBits",        mLoFiBits,        nullptr);
    state.setProperty("modRate",         mModRate,         nullptr);
    state.setProperty("modTimeMod",      mModTimeMod,      nullptr);
    state.setProperty("modCutoffMod",    mModCutoffMod,    nullptr);
    state.setProperty("diffLevel",       mDiffLevel,       nullptr);
    state.setProperty("diffSpread",      mDiffSpread,      nullptr);
    state.setProperty("fbDistType",      mFBDistType,      nullptr);
    state.setProperty("fbDistKnee",      mFBDistKnee,      nullptr);
    state.setProperty("fbDistSymmetry",  mFBDistSymmetry,  nullptr);
    state.setProperty("fbDistLevel",     mFBDistLevel,     nullptr);
    state.setProperty("tone",            mTone,            nullptr);
    state.setProperty("wetOut",          mWetOut,          nullptr);
    state.setProperty("dryOut",          mDryOut,          nullptr);

    // Tier-3 scaffolding for future Spectral Delay subsystem. Reserved in
    // state serialization so v1 presets survive its later addition without
    // requiring a preset-migration pass. DSP ignores these today.
    state.setProperty("delayMode",       mDelayMode,        nullptr);
    for (int k = 0; k < (int) mBandDelayMs.size(); ++k)
        state.setProperty(juce::Identifier("bandDelayMs" + juce::String(k)),
                          mBandDelayMs[(size_t) k], nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml) juce::AudioProcessor::copyXmlToBinary(*xml, dest);
}

// ─────────────────────────────────────────────────────────────────────────────
void DelayDSP::setStateInformation (const void* data, int sz)
{
    std::unique_ptr<juce::XmlElement> xml(juce::AudioProcessor::getXmlFromBinary(data, sz));
    if (!xml) return;

    // Accept both old and new format
    if (!xml->hasTagName("DelayDSP") && !xml->hasTagName("DelayDSP2")) return;

    juce::ValueTree state = juce::ValueTree::fromXml(*xml);

    // Legacy params
    delayMs         = state.getProperty("delayMs",         delayMs);
    feedback        = state.getProperty("feedback",        feedback);
    wet             = state.getProperty("wet",             wet);
    hpHz            = state.getProperty("hpHz",            hpHz);
    lpHz            = state.getProperty("lpHz",            lpHz);
    pingPong        = ((int)state.getProperty("pingPong",  0)) != 0;
    syncBPM         = ((int)state.getProperty("syncBPM",   0)) != 0;
    syncNumerator   = state.getProperty("syncNumerator",   syncNumerator);
    syncDenominator = state.getProperty("syncDenominator", syncDenominator);
    bypassed        = ((int)state.getProperty("bypassed",  0)) != 0;

    // New params
    mWetIn          = state.getProperty("wetIn",          mWetIn);
    mKeepPitch      = ((int)state.getProperty("keepPitch", 0)) != 0;
    mSmoothing      = state.getProperty("smoothing",      mSmoothing);
    mOffsetPan      = state.getProperty("offsetPan",      mOffsetPan);
    mDelayModel     = state.getProperty("delayModel",     mDelayModel);
    mStereoSpread   = state.getProperty("stereoSpread",   mStereoSpread);
    mFeedbackLevel  = state.getProperty("feedbackLevel",  mFeedbackLevel);
    mFBFilterType   = state.getProperty("fbFilterType",   mFBFilterType);
    mFBCutoff       = state.getProperty("fbCutoff",       mFBCutoff);
    mFBResonance    = state.getProperty("fbResonance",    mFBResonance);
    mLoFiRate       = state.getProperty("loFiRate",       mLoFiRate);
    mLoFiBits       = state.getProperty("loFiBits",       mLoFiBits);
    mModRate        = state.getProperty("modRate",         mModRate);
    mModTimeMod     = state.getProperty("modTimeMod",      mModTimeMod);
    mModCutoffMod   = state.getProperty("modCutoffMod",    mModCutoffMod);
    mDiffLevel      = state.getProperty("diffLevel",       mDiffLevel);
    mDiffSpread     = state.getProperty("diffSpread",      mDiffSpread);
    mFBDistType     = state.getProperty("fbDistType",      mFBDistType);
    mFBDistKnee     = state.getProperty("fbDistKnee",      mFBDistKnee);
    mFBDistSymmetry = state.getProperty("fbDistSymmetry",  mFBDistSymmetry);
    mFBDistLevel    = state.getProperty("fbDistLevel",     mFBDistLevel);
    mTone           = state.getProperty("tone",            mTone);
    mWetOut         = state.getProperty("wetOut",          mWetOut);
    mDryOut         = state.getProperty("dryOut",          mDryOut);

    // Tier-3 scaffolding load (future Spectral Delay). Missing keys = defaults.
    mDelayMode = (int) state.getProperty("delayMode", mDelayMode);
    for (int k = 0; k < (int) mBandDelayMs.size(); ++k)
        mBandDelayMs[(size_t) k] = (float)(double) state.getProperty(
            juce::Identifier("bandDelayMs" + juce::String(k)),
            (double) mBandDelayMs[(size_t) k]);

    // A9 -- Sync legacy->new ONLY when the new-key was absent in the preset.
    // Previously these three overwrote the new-API values unconditionally, so
    // a mixed preset with both legacy and new keys lost the new-API values.
    if (! state.hasProperty("feedbackLevel"))  mFeedbackLevel = feedback;
    if (! state.hasProperty("wetOut"))         mWetOut        = wet;
    if (! state.hasProperty("delayModel") && pingPong) mDelayModel = 2;

    if (mSampleRate > 0.0)
    {
        recalcTargetDelay();
        mCurDelayL = mTargDelayL;
        mCurDelayR = mTargDelayR;
        applyFBFilterType();
        mFBTPT.setCutoffFrequency(juce::jlimit(20.0f, 20000.0f, mFBCutoff));
        mFBTPT.setResonance      (juce::jlimit(0.1f, 20.0f,    mFBResonance));
        mFBTPT.reset();
        updateToneFilter();
        mToneFilter.reset();   // clear stale biquad state so preset load doesn't pop
        updateDiffusion();
        // DC-blocker state is local to DSP — clear to avoid click on preset load
        mDcBlockXL = mDcBlockYL = 0.0f;
        mDcBlockXR = mDcBlockYR = 0.0f;
        // A2 -- Snap the smoother so state-load doesn't trigger a 100 ms slide.
        mDelayMsSmoothed.setCurrentAndTargetValue(delayMs);
    }
}
