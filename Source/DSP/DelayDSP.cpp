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

    // One-pole DC-blocker pole from a corner FREQUENCY, so the corner stays put
    // instead of quadrupling from 44.1 kHz to 192 kHz.
    mDcBlockR = (float) std::exp (-2.0 * juce::MathConstants<double>::pi
                                  * (double) kDcBlockHz / juce::jmax (8000.0, sampleRate));

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
    mFbEnvDisplay.store (0.0f, std::memory_order_relaxed);

    for (int s = 0; s < kDiffStages; ++s)
        mDiffusion[s].reset();

    mFBTPT.reset();
    mToneFilter.reset();

    mDcBlockXL = mDcBlockYL = 0.0f;
    mDcBlockXR = mDcBlockYR = 0.0f;

    mLoFiPhase = 1.0f;
    mLoFiHoldL = mLoFiHoldR = 0.0f;
    mDelayMsSmoothed.setCurrentAndTargetValue(delayMs);

    // H-8: reset VocalDoubler drift phasors + ducking envelope
    mDoubleLfoPhaseL = 0.0f;
    mDoubleLfoPhaseR = 0.27f;
    mDuckEnv         = 0.0f;
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
// (and cutoff is modulated per-sample in process()).  Type 3 = Off leaves the
// TPT untouched -- process() skips the filter entirely for that type.
void DelayDSP::applyFBFilterType()
{
    using FT = juce::dsp::StateVariableTPTFilterType;
    switch (mFBFilterType)
    {
        case 0: mFBTPT.setType(FT::lowpass);  break;
        case 1: mFBTPT.setType(FT::highpass); break;
        case 2: mFBTPT.setType(FT::bandpass); break;
        default: break;   // 3 = Off (bypassed in process())
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

// CL-299 (2): display twin of the feedback distortion in processBlock step 4.
// EDIT BOTH TOGETHER -- see the header note for why they are not one function.
float DelayDSP::shapeFeedbackForDisplay (float x) const
{
    if (mFBDistType == 0)
        return softLimit (x, mFBDistKnee);

    const float dc       = mFBDistSymmetry * 0.3f;
    const float drive    = std::max (0.001f, mFBDistLevel);
    const float hard     = 1.0f - mFBDistKnee;
    const float uBias    = drive * dc;
    const float tanhBias = std::tanh (uBias);
    const float clipBias = juce::jlimit (-1.0f, 1.0f, uBias);
    const float u        = drive * (x + dc);
    return ((1.0f - hard) * (std::tanh (u) - tanhBias)
          +  hard         * (juce::jlimit (-1.0f, 1.0f, u) - clipBias)) / drive;
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

    // H-8: VocalDoubler Type runs an entirely different algorithm (dual
    // detuned taps, no feedback, no diffusion / lo-fi / mod LFO chain).
    if (mType == Type::VocalDoubler)
    {
        // Inline VocalDoubler loop -- short delay taps with per-side LFO
        // drift gives the classic stacked-vocals doubler effect.  No
        // feedback path so the existing Echo machinery is fully bypassed.
        const int numSamples  = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numSamples == 0 || numChannels < 1 || mLineL.empty()) return;

        float* L = buffer.getWritePointer (0);
        float* R = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

        const int lineSize = (int) mLineL.size();
        const float sr = (float) mSampleRate;

        // Drift LFO -- per-side phase increment
        const float lfoInc = (float) (juce::MathConstants<double>::twoPi
                                          * (double) mDoubleRate / mSampleRate);
        const float driftDepthMs = (mDoubleDetune * 0.01f) * 3.0f;   // 0..3 ms

        // Per-sample base delay times (in samples)
        const float baseL = (mDoubleTimeLMs * 0.001f) * sr;
        const float baseR = (mDoubleTimeRMs * 0.001f) * sr;

        // Width 0..1 -- 0 = both taps centered, 1 = full L/R split
        const float w        = juce::jlimit (0.0f, 1.0f, mDoubleWidth * 0.01f);
        const float lTapL    = 0.5f + 0.5f * w;   // L tap pan-left amount
        const float lTapR    = 0.5f - 0.5f * w;   // L tap pan-right amount
        const float rTapL    = 0.5f - 0.5f * w;
        const float rTapR    = 0.5f + 0.5f * w;

        // Ducking setup (also runs for Echo Type below).  External SC source
        // when picked; falls back to the dry input so duck works on a single
        // track without explicit routing.
        const float duckThreshLin = juce::Decibels::decibelsToGain (mDuckThresholdDb);
        const float duckAmt01     = juce::jlimit (0.0f, 1.0f, mDuckAmount * 0.01f);
        const float duckAtk       = std::exp (-1.0f / std::max (1.0f,
                                                                  mDuckAttackMs * 0.001f * sr));
        const float duckRel       = std::exp (-1.0f / std::max (1.0f,
                                                                  mDuckReleaseMs * 0.001f * sr));
        auto* scBuf = getActiveSidechain();
        const float* scL = (scBuf && scBuf->getNumChannels() > 0)
                              ? scBuf->getReadPointer (0) : nullptr;
        const float* scR = (scBuf && scBuf->getNumChannels() > 1)
                              ? scBuf->getReadPointer (1) : scL;
        const int scN = scBuf ? scBuf->getNumSamples() : 0;

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = L[i];
            const float inR = R ? R[i] : inL;

            // Write dry mono-summed input into both lines
            const float monoIn = 0.5f * (inL + inR);
            mLineL[mWritePos] = monoIn * mWetIn;
            mLineR[mWritePos] = monoIn * mWetIn;

            // Per-side drift phasors (decorrelated by initial phase offset)
            const float driftL = std::sin (mDoubleLfoPhaseL) * driftDepthMs * 0.001f * sr;
            const float driftR = std::sin (mDoubleLfoPhaseR) * driftDepthMs * 0.001f * sr;
            mDoubleLfoPhaseL += lfoInc;
            mDoubleLfoPhaseR += lfoInc * 1.13f;   // slight rate offset for natural drift
            while (mDoubleLfoPhaseL >= juce::MathConstants<float>::twoPi)
                mDoubleLfoPhaseL -= juce::MathConstants<float>::twoPi;
            while (mDoubleLfoPhaseR >= juce::MathConstants<float>::twoPi)
                mDoubleLfoPhaseR -= juce::MathConstants<float>::twoPi;

            // Read positions for L + R taps with drift
            const float posL = (float) mWritePos - juce::jmax (1.0f, baseL + driftL);
            const float posR = (float) mWritePos - juce::jmax (1.0f, baseR + driftR);
            const float wrappedL = posL < 0.0f ? posL + (float) lineSize : posL;
            const float wrappedR = posR < 0.0f ? posR + (float) lineSize : posR;
            const float tapL = linInterp (mLineL, wrappedL, lineSize);
            const float tapR = linInterp (mLineR, wrappedR, lineSize);

            // Stereo recombine using width pans
            float wetL = tapL * lTapL + tapR * rTapL;
            float wetR = tapL * lTapR + tapR * rTapR;

            // Ducking on the wet only -- envelope follows external SC if a
            // source is picked, otherwise self-sidechains on the dry input.
            const float trigL = (scL && i < scN) ? scL[i] : inL;
            const float trigR = (scR && i < scN) ? scR[i] : inR;
            const float inEnvSrc = std::max (std::abs (trigL), std::abs (trigR));
            const float coef = (inEnvSrc > mDuckEnv) ? duckAtk : duckRel;
            mDuckEnv = inEnvSrc + (mDuckEnv - inEnvSrc) * coef;
            const float overshoot = std::max (0.0f, mDuckEnv - duckThreshLin);
            const float duckScalar = (duckAmt01 > 0.0f && overshoot > 0.0f)
                                        ? juce::jmax (0.0f, 1.0f - duckAmt01
                                              * juce::jmin (1.0f, overshoot / duckThreshLin))
                                        : 1.0f;
            wetL *= duckScalar;
            wetR *= duckScalar;

            // Final mix (no feedback writeback -- VocalDoubler has no FB path)
            L[i] = inL * mDryOut + wetL * mWetOut;
            if (R) R[i] = inR * mDryOut + wetR * mWetOut;

            mWritePos = (mWritePos + 1) % lineSize;
        }

        return;   // skip the Echo path entirely
    }

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels < 1 || mLineL.empty()) return;

    float* L = buffer.getWritePointer(0);
    float* R = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    const int lineSize = static_cast<int>(mLineL.size());

    // H-8: ducking setup for Echo path -- same envelope follower + threshold
    // logic as the VocalDoubler branch above.  Wet output is attenuated when
    // the SC trigger crosses threshold.  External SC source if picked,
    // otherwise self-sidechains on the dry input.
    const float duckThreshLin = juce::Decibels::decibelsToGain (mDuckThresholdDb);
    const float duckAmt01     = juce::jlimit (0.0f, 1.0f, mDuckAmount * 0.01f);
    const float sr            = (float) mSampleRate;
    const float duckAtk       = std::exp (-1.0f / std::max (1.0f, mDuckAttackMs  * 0.001f * sr));
    const float duckRel       = std::exp (-1.0f / std::max (1.0f, mDuckReleaseMs * 0.001f * sr));
    auto* scBufEcho = getActiveSidechain();
    const float* scL_E = (scBufEcho && scBufEcho->getNumChannels() > 0)
                              ? scBufEcho->getReadPointer (0) : nullptr;
    const float* scR_E = (scBufEcho && scBufEcho->getNumChannels() > 1)
                              ? scBufEcho->getReadPointer (1) : scL_E;
    const int scN_E = scBufEcho ? scBufEcho->getNumSamples() : 0;

    // Off model = reference-delay behavior: no echoes at all -- the FX chain
    // (lo-fi -> FB filter -> FB distortion -> tone) runs instantly on the
    // input, turning the effect into a pure distortion/filter unit.  The
    // delay line is flushed with zeros while Off so a later model switch
    // doesn't replay a stale tail.
    const bool instantFx = (mDelayModel == 3);

    // LFO increment per sample
    const float lfoInc = static_cast<float>(
        juce::MathConstants<double>::twoPi * mModRate / mSampleRate);

    // Lo-Fi sample-and-hold: both the on/off decision and the phase increment
    // are block-invariant, so they are hoisted out of the per-sample loop.
    const bool  loFiSrOn = (mLoFiRate < kLoFiRateMaxHz);
    const float loFiInc  = mLoFiRate / static_cast<float>(juce::jmax (1.0, mSampleRate));

    // Slew coefficient for keep-pitch mode:
    // mSmoothing=0 -> fast (slewCoef~=0), mSmoothing=1 -> slow (~0.9997 at 48kHz)
    const float slewCoef = mKeepPitch
        ? std::exp(-1.0f / std::max(1.0f, mSmoothing * 0.5f * static_cast<float>(mSampleRate)))
        : 0.0f;

    float fbBlockPeak = 0.0f;   // warn-ring source, accumulated at step 5

    // T18: wet accumulated SEPARATELY from the mix -- a post-scan of the output
    // buffer would bury the echoes under the dry signal while it plays, and the
    // echoes are the entire point of the picture.  Hoisted gate: one load per
    // block, three fabs per sample only while a visual is watching.
    const bool visActive = mVisualFeed.isActive();
    float visWetPk = 0.0f, visDryPk = 0.0f;

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
        if (instantFx)
        {
            // Off model: no delayed read -- the input (through the input-wet
            // gain) feeds the FX chain directly.
            rawReadL = inL * mWetIn;
            rawReadR = inR * mWetIn;
        }
        else
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

        // ── Lo-Fi (sample-rate reduction + bit crush) ───────────────────────
        // D.4-Q5 fix (2026-05-01): applied to the delay-line READ (before the
        // feedback / output split) so the lo-fi character is heard on EVERY
        // echo - not just on echo #2+ via feedback-buildup, which was the old
        // placement and made lo-fi inaudible at low feedback levels.
        //
        // The sample-and-hold is OFF at the top of the knob's range, at every
        // device rate.  It used to rely on the hold interval falling below one
        // sample (rate >= device rate) to go transparent, which the knob can
        // only reach at or below 48 kHz: above that, no knob position could
        // disable it and every echo came back with an unmutable HF droop the
        // dry signal did not have.  Bit crush is a separate control and still
        // applies on top.
        if (loFiSrOn)
        {
            mLoFiPhase += loFiInc;
            if (mLoFiPhase >= 1.0f)
            {
                // Full wrap, not a single subtraction: the increment exceeds 1
                // whenever the lo-fi rate is above the device rate, and one
                // subtraction let the phase creep upward without bound.
                mLoFiPhase -= std::floor (mLoFiPhase);
                mLoFiHoldL = rawReadL;
                mLoFiHoldR = rawReadR;
            }
            rawReadL = mLoFiHoldL;
            rawReadR = mLoFiHoldR;
        }
        else
        {
            // Track the input while bypassed so turning the knob back down
            // cannot replay a stale held sample as a click.
            mLoFiHoldL = rawReadL;
            mLoFiHoldR = rawReadR;
        }

        rawReadL = bitcrush(rawReadL, mLoFiBits);
        rawReadR = bitcrush(rawReadR, mLoFiBits);

        // ── Feedback path ────────────────────────────────────────────────────
        float feedL = rawReadL;
        float feedR = rawReadR;

        // 1. Diffusion (allpass chain, only when diffLevel > 0; not part of the
        //    Off model's instant chain -- the reference applies filter / lo-fi /
        //    distortion / tone in Off, not diffusion)
        if (mDiffLevel > 0.001f && ! instantFx)
        {
            for (int s = 0; s < kDiffStages; ++s)
                mDiffusion[s].processLR(feedL, feedR);
        }

        // 3. Feedback filter (TPT SVF LP/HP/BP, per-sample modulated cutoff;
        //    type 3 = Off bypasses the filter entirely)
        //    mModCutoffMod scales cutoff by ±(mModCutoffMod) octaves (lfoVal is -1..1)
        if (mFBFilterType != 3)
        {
            const float cutoffMod       = std::exp2(mModCutoffMod * lfoVal);
            const float modulatedCutoff = juce::jlimit(20.0f, 20000.0f, mFBCutoff * cutoffMod);
            mFBTPT.setCutoffFrequency(modulatedCutoff);
            feedL = mFBTPT.processSample(0, feedL);
            feedR = mFBTPT.processSample(1, feedR);
        }

        // 4. Feedback distortion
        //    (CL-299: shapeFeedbackForDisplay mirrors this branch for the
        //    panel's transfer-curve graph -- edit both together.)
        if (mFBDistType == 0)
        {
            // Soft limiter
            feedL = softLimit(feedL, mFBDistKnee);
            feedR = softLimit(feedR, mFBDistKnee);
        }
        else
        {
            // Saturation: tanh soft curve cross-faded toward a hard clip by
            // Knee (reference behavior: Knee is the Sat-mode character --
            // soft/rounded = valve, sharp = solid-state).  knee=1 -> pure tanh
            // (the pre-Task-9 sound), knee=0 -> hard clip.  Both terms are
            // normalized so small-signal gain == 1.0 (a >1 small-signal gain
            // causes feedback runaway) and both subtract their own DC-bias
            // floor so Symmetry's static DC doesn't leak (the 5 Hz DC-blocker
            // below catches LFO drift).
            const float dc        = mFBDistSymmetry * 0.3f;
            const float drive     = std::max(0.001f, mFBDistLevel);
            const float hard      = 1.0f - mFBDistKnee;
            const float uBias     = drive * dc;
            const float tanhBias  = std::tanh(uBias);
            const float clipBias  = juce::jlimit(-1.0f, 1.0f, uBias);
            const float uL        = drive * (feedL + dc);
            const float uR        = drive * (feedR + dc);
            feedL = ((1.0f - hard) * (std::tanh(uL) - tanhBias)
                   +  hard         * (juce::jlimit(-1.0f, 1.0f, uL) - clipBias)) / drive;
            feedR = ((1.0f - hard) * (std::tanh(uR) - tanhBias)
                   +  hard         * (juce::jlimit(-1.0f, 1.0f, uR) - clipBias)) / drive;
        }

        // 4b. 5 Hz DC-blocker (post-distortion / pre-feedback-level).
        //     Removes the DC injected by mFBDistSymmetry in Sat mode so it does
        //     not compound per repeat and eat headroom. y[n] = x[n] - x[n-1] + R*y[n-1]
        {
            const float R = mDcBlockR;
            const float dcOutL = feedL - mDcBlockXL + R * mDcBlockYL;
            mDcBlockXL = feedL; mDcBlockYL = dcOutL; feedL = dcOutL;

            const float dcOutR = feedR - mDcBlockXR + R * mDcBlockYR;
            mDcBlockXR = feedR; mDcBlockYR = dcOutR; feedR = dcOutR;
        }

        // 5. Feedback level (loop injection only -- in the Off model the
        //    post-chain signal IS the audible output and there is no loop)
        if (! instantFx)
        {
            feedL *= mFeedbackLevel;
            feedR *= mFeedbackLevel;
            // Warn-ring source: peak of what is actually re-entering the loop.
            // Off model skips this on purpose -- no loop, no feedback occurring,
            // ring stays dark.  Cost is two fabs+max on register values.
            fbBlockPeak = juce::jmax (fbBlockPeak, std::abs (feedL), std::abs (feedR));
        }

        // ── Write to delay lines (depends on model) ──────────────────────────
        const float monoIn = (inL + inR) * 0.5f;

        switch (mDelayModel)
        {
            case 1: // Mono - L+R averaged into both lines
                mLineL[mWritePos] = monoIn * mWetIn + feedL;
                mLineR[mWritePos] = monoIn * mWetIn + feedR;
                break;

            case 2: // PingPong - L input feeds L line + R's feedback, and vice versa
                mLineL[mWritePos] = inL * mWetIn + feedR;
                mLineR[mWritePos] = inR * mWetIn + feedL;
                break;

            case 3: // Off - no delay engine; flush at 1x so a later model
                    // switch doesn't replay a stale tail
                mLineL[mWritePos] = 0.0f;
                mLineR[mWritePos] = 0.0f;
                break;

            default: // Stereo
                mLineL[mWritePos] = inL * mWetIn + feedL;
                mLineR[mWritePos] = inR * mWetIn + feedR;
                break;
        }

        // ── Output path ──────────────────────────────────────────────────────
        // Off model taps the post-FX-chain signal (there is no delayed read).
        float outL = mToneFilter.processL(instantFx ? feedL : rawReadL);
        float outR = mToneFilter.processR(instantFx ? feedR : rawReadR);

        // Output limiter (protect when feedback > 1 causes buildup)
        outL = juce::jlimit(-1.0f, 1.0f, outL);
        outR = juce::jlimit(-1.0f, 1.0f, outR);

        // H-8: ducking on the wet output only.  Trigger = external SC source
        // if picked, otherwise self-sidechain on the dry input.  duckScalar
        // = 1.0 when amount = 0 (back-compat, no audible change vs pre-H-8
        // Echo path).
        const float trigL_E = (scL_E && i < scN_E) ? scL_E[i] : inL;
        const float trigR_E = (scR_E && i < scN_E) ? scR_E[i] : inR;
        const float inEnvSrc = std::max (std::abs (trigL_E), std::abs (trigR_E));
        const float coef = (inEnvSrc > mDuckEnv) ? duckAtk : duckRel;
        mDuckEnv = inEnvSrc + (mDuckEnv - inEnvSrc) * coef;
        const float overshoot = std::max (0.0f, mDuckEnv - duckThreshLin);
        const float duckScalar = (duckAmt01 > 0.0f && overshoot > 0.0f)
                                    ? juce::jmax (0.0f, 1.0f - duckAmt01
                                          * juce::jmin (1.0f, overshoot / duckThreshLin))
                                    : 1.0f;
        outL *= duckScalar;
        outR *= duckScalar;

        // Final mix
        L[i] = inL * mDryOut + outL * mWetOut;
        if (R)
            R[i] = inR * mDryOut + outR * mWetOut;

        if (visActive)
        {
            visWetPk = juce::jmax (visWetPk, std::abs (outL * mWetOut),
                                             std::abs (outR * mWetOut));
            visDryPk = juce::jmax (visDryPk, std::abs (inL), std::abs (inR));
        }

        mWritePos = (mWritePos + 1) % lineSize;
    }

    // Warn-ring publish: instant rise to the block peak, ~250 ms decay -- fast
    // enough that a building runaway reads live, slow enough that the ring does
    // not flicker at the repeat rate.
    {
        const float prev  = mFbEnvDisplay.load (std::memory_order_relaxed);
        const float decay = std::exp (-(float) numSamples
                                      / (0.25f * (float) mSampleRate));
        mFbEnvDisplay.store (juce::jmax (fbBlockPeak, prev * decay),
                             std::memory_order_relaxed);
    }

    // T18 column: hi/lo = the WET signal (each echo is a visible bump even
    // while the dry plays), a = the dry input.
    if (visActive)
    {
        mBlockMsDisplay.store ((float) (numSamples * 1000.0 / mSampleRate),
                               std::memory_order_relaxed);
        mVisualFeed.push (-juce::jlimit (0.0f, 1.0f, visWetPk),
                           juce::jlimit (0.0f, 1.0f, visWetPk),
                           juce::jlimit (0.0f, 1.0f, visDryPk), 0.0f);
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
    const float n = juce::jlimit(100.0f, kLoFiRateMaxHz, hz);
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
    const int n = juce::jlimit(0, 3, t);
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
// Legacy setters (backward compat - map to new parameters)
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

    // H-8 (2026-05-02): Type umbrella + VocalDoubler params + ducking.
    // Pre-H-8 presets default to Type=Echo via getProperty fallback.
    state.setProperty ("type",             (int) mType,       nullptr);
    state.setProperty ("doubleTimeLMs",    mDoubleTimeLMs,    nullptr);
    state.setProperty ("doubleTimeRMs",    mDoubleTimeRMs,    nullptr);
    state.setProperty ("doubleDetune",     mDoubleDetune,     nullptr);
    state.setProperty ("doubleWidth",      mDoubleWidth,      nullptr);
    state.setProperty ("doubleRate",       mDoubleRate,       nullptr);
    state.setProperty ("duckAmount",       mDuckAmount,       nullptr);
    state.setProperty ("duckThresholdDb",  mDuckThresholdDb,  nullptr);
    state.setProperty ("duckAttackMs",     mDuckAttackMs,     nullptr);
    state.setProperty ("duckReleaseMs",    mDuckReleaseMs,    nullptr);

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
    // Explicit 0 (Limit) fallback, NOT the member: a save without this key
    // predates the FBDist feature, when Limit was the only behavior -- the
    // member now constructs as 1 (Sat) and must not leak into legacy files.
    mFBDistType     = state.getProperty("fbDistType",      0);
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

    // H-8 (2026-05-02): Type umbrella + VocalDoubler params + ducking.
    // Pre-H-8 presets lack these keys; getProperty fallback hands back our
    // current defaults (Echo, sane VocalDoubler defaults, duck disabled),
    // so existing presets sound exactly the same as before.
    mType            = (Type) (int) state.getProperty ("type",             (int) mType);
    mDoubleTimeLMs   = (float) (double) state.getProperty ("doubleTimeLMs", (double) mDoubleTimeLMs);
    mDoubleTimeRMs   = (float) (double) state.getProperty ("doubleTimeRMs", (double) mDoubleTimeRMs);
    mDoubleDetune    = (float) (double) state.getProperty ("doubleDetune",  (double) mDoubleDetune);
    mDoubleWidth     = (float) (double) state.getProperty ("doubleWidth",   (double) mDoubleWidth);
    mDoubleRate      = (float) (double) state.getProperty ("doubleRate",    (double) mDoubleRate);
    mDuckAmount      = (float) (double) state.getProperty ("duckAmount",    (double) mDuckAmount);
    mDuckThresholdDb = (float) (double) state.getProperty ("duckThresholdDb", (double) mDuckThresholdDb);
    mDuckAttackMs    = (float) (double) state.getProperty ("duckAttackMs",  (double) mDuckAttackMs);
    mDuckReleaseMs   = (float) (double) state.getProperty ("duckReleaseMs", (double) mDuckReleaseMs);

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
        // DC-blocker state is local to DSP - clear to avoid click on preset load
        mDcBlockXL = mDcBlockYL = 0.0f;
        mDcBlockXR = mDcBlockYR = 0.0f;
        // A2 -- Snap the smoother so state-load doesn't trigger a 100 ms slide.
        mDelayMsSmoothed.setCurrentAndTargetValue(delayMs);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// H-8 (2026-05-02): Type umbrella + VocalDoubler + ducking + Slapback preset.
// ─────────────────────────────────────────────────────────────────────────────
void DelayDSP::setType (int t)
{
    const Type newT = (t == (int) Type::VocalDoubler) ? Type::VocalDoubler
                                                      : Type::Echo;
    if (newT == mType) return;
    mType = newT;
    // Reset transient per-Type state so the user doesn't hear leftover
    // tails from the previous Type when switching.
    mDoubleLfoPhaseL = 0.0f;
    mDoubleLfoPhaseR = 0.27f;
}

void DelayDSP::setDoubleTimeLMs (float ms)
{
    const float v = juce::jlimit (5.0f, 50.0f, ms);
    if (juce::approximatelyEqual (v, mDoubleTimeLMs)) return;
    mDoubleTimeLMs = v;
}

void DelayDSP::setDoubleTimeRMs (float ms)
{
    const float v = juce::jlimit (5.0f, 50.0f, ms);
    if (juce::approximatelyEqual (v, mDoubleTimeRMs)) return;
    mDoubleTimeRMs = v;
}

void DelayDSP::setDoubleDetune (float pct)
{
    const float v = juce::jlimit (0.0f, 100.0f, pct);
    if (juce::approximatelyEqual (v, mDoubleDetune)) return;
    mDoubleDetune = v;
}

void DelayDSP::setDoubleWidth (float pct)
{
    const float v = juce::jlimit (0.0f, 100.0f, pct);
    if (juce::approximatelyEqual (v, mDoubleWidth)) return;
    mDoubleWidth = v;
}

void DelayDSP::setDoubleRate (float hz)
{
    const float v = juce::jlimit (0.1f, 2.0f, hz);
    if (juce::approximatelyEqual (v, mDoubleRate)) return;
    mDoubleRate = v;
}

void DelayDSP::setDuckAmount (float pct)
{
    const float v = juce::jlimit (0.0f, 100.0f, pct);
    if (juce::approximatelyEqual (v, mDuckAmount)) return;
    mDuckAmount = v;
}

void DelayDSP::setDuckThresholdDb (float db)
{
    const float v = juce::jlimit (-60.0f, 0.0f, db);
    if (juce::approximatelyEqual (v, mDuckThresholdDb)) return;
    mDuckThresholdDb = v;
}

void DelayDSP::setDuckAttackMs (float ms)
{
    const float v = juce::jlimit (1.0f, 200.0f, ms);
    if (juce::approximatelyEqual (v, mDuckAttackMs)) return;
    mDuckAttackMs = v;
}

void DelayDSP::setDuckReleaseMs (float ms)
{
    const float v = juce::jlimit (10.0f, 1000.0f, ms);
    if (juce::approximatelyEqual (v, mDuckReleaseMs)) return;
    mDuckReleaseMs = v;
}

// Slapback preset = single short delay (~110 ms), low feedback, low wet, no
// diffusion, no mod, slight tone tilt.  Writes Echo-Type values; if user is
// on VocalDoubler, switches Type back to Echo first so the preset audibly
// applies.
void DelayDSP::presetSlapback()
{
    setType ((int) Type::Echo);
    setDelayModel (0);            // Stereo
    setDelayMs (110.0f);
    setStereoSpread (0.0f);
    setOffsetPan (0.0f);
    setFeedbackLevel (0.12f);
    setFeedbackFilterType (0);    // LP
    setFeedbackCutoff (8000.0f);
    setFeedbackResonance (0.7f);
    setLoFiBits (24.0f);
    setLoFiSampleRate (48000.0f);
    setDiffusionLevel (0.0f);
    setDiffusionSpread (0.5f);
    setModRate (0.0f);
    setModTimeMod (0.0f);
    setModCutoffMod (0.0f);
    setFBDistType (0);            // Limit
    setFBDistLevel (1.0f);
    setTone (0.15f);              // mild high-shelf cut for vintage feel
    setWetIn (1.0f);
    setWetOut (0.30f);
    setDryOut (1.0f);
    setKeepPitch (false);
    setSmoothing (0.5f);
    setTempoSync (false);
    // Ducking left where the user had it; Slapback doesn't override it.
}
