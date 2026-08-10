#include "OverdriveDSP.h"
#include <cmath>

namespace
{
    constexpr int   kOsFactor    = 4;       // 4× oversampling ratio
    constexpr float kDcBlockHz   = 5.0f;    // DC-blocker corner (Hz)
    constexpr float kSmoothMs    = 15.0f;   // PreAmp / Color / PostGain ramp
    constexpr float kFilterSmooth = 30.0f;  // PostFilter ramp (slower → no pop)
}

// ─────────────────────────────────────────────────────────────────────────────
OverdriveDSP::OverdriveDSP() = default;

// ─────────────────────────────────────────────────────────────────────────────
// prepare() frees and rebuilds the oversampler, so it is legal ONLY when the
// audio thread cannot be inside process() -- i.e. from EffectRack::prepare,
// which holds mSlotsLock, or before the DSP is published.  Every other
// message-thread entry point (the OS chicken head, preset loads) must go
// through stageOversampler() instead.
void OverdriveDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = juce::jmax (1, maxBlockSize);

    // Drop any staged-but-unadopted oversampler: this rebuild supersedes it.
    mOsSwapPending.store (false, std::memory_order_release);
    mOsPending.reset();

    // ── TPT filters (stereo) ─────────────────────────────────────────────────
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) mMaxBlock;
    spec.numChannels      = 2;
    mPreLpf.prepare (spec);
    mLPF.prepare (spec);
    mPreLpf.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    mLPF.setType    (juce::dsp::StateVariableTPTFilterType::lowpass);
    mPreLpf.setCutoffFrequency (juce::jlimit (20.0f, (float) sampleRate * 0.49f, mColor));
    mPreLpf.setResonance       (juce::MathConstants<float>::sqrt2 * 0.5f);   // Butterworth pre-LP
    mLPF.setCutoffFrequency (juce::jlimit (20.0f, (float) sampleRate * 0.49f, mPostFilter));
    mLPF.setResonance       (juce::MathConstants<float>::sqrt2 * 0.5f);

    // ── Oversampler (stereo, polyphase IIR, factor = 2^mOsLog2) ─────────────
    // C5: factor is now user-selectable (2/4/8/16x). Default mOsLog2=2 -> 4x.
    mOversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2 /* numChannels */,
        juce::jlimit (1, 4, mOsLog2),
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true /* maxQuality */);
    mOversampler->initProcessing ((size_t) mMaxBlock);
    mOsLog2Active = juce::jlimit (1, 4, mOsLog2);
    mLatencySamples.store ((int) std::ceil (mOversampler->getLatencyInSamples()),
                           std::memory_order_relaxed);

    // ── Scratch buffers (stereo) ─────────────────────────────────────────────
    mBandBuf.setSize (2, mMaxBlock, false, true, true);

    // I-5 (2026-05-02): Pedal mode 80 Hz split filters + clean-path scratch.
    mPedalSplitHpf.prepare (spec);
    mPedalSplitLpf.prepare (spec);
    mPedalSplitHpf.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    mPedalSplitLpf.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    mPedalSplitHpf.setCutoffFrequency (80.0f);
    mPedalSplitLpf.setCutoffFrequency (80.0f);
    mPedalSplitHpf.setResonance (juce::MathConstants<float>::sqrt2 * 0.5f);
    mPedalSplitLpf.setResonance (juce::MathConstants<float>::sqrt2 * 0.5f);
    mPedalSplitHpf.reset();
    mPedalSplitLpf.reset();
    mPedalCleanBuf.setSize (2, mMaxBlock, false, true, true);

    // QA-EffectsReview Task 5: OD-pedal 500 Hz mid-notch + 720 Hz inter-stage HPF.
    const auto notchCoefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 500.0f, 1.0f, juce::Decibels::decibelsToGain (-4.5f));
    mPedalNotchL.coefficients = notchCoefs;   // pointer copy (coefficients starts NULL)
    mPedalNotchR.coefficients = notchCoefs;
    mPedalNotchL.reset();
    mPedalNotchR.reset();
    // 1-pole HPF coef at the oversampled rate (the drive path runs inside the OS loop).
    const double osRate = sampleRate * (double) (1 << juce::jlimit (1, 4, mOsLog2));
    mPedalHpfR = (float) (1.0 - (juce::MathConstants<double>::twoPi * 720.0 / osRate));
    mPedalHpfX[0] = mPedalHpfX[1] = mPedalHpfY[0] = mPedalHpfY[1] = 0.0f;

    // ── SmoothedValues ───────────────────────────────────────────────────────
    mPreAmpSmooth    .reset (sampleRate, kSmoothMs     * 0.001);
    mColorSmooth     .reset (sampleRate, kSmoothMs     * 0.001);
    mPostFilterSmooth.reset (sampleRate, kFilterSmooth * 0.001);
    mPostGainSmooth  .reset (sampleRate, kSmoothMs     * 0.001);
    mPreBandSmooth   .reset (sampleRate, kSmoothMs     * 0.001);   // A3
    mWetSmooth       .reset (sampleRate, kSmoothMs     * 0.001);   // A4
    mX100ScalarSmooth.reset (sampleRate, 0.020);                    // A5 20 ms
    mBiasSmooth      .reset (sampleRate, kSmoothMs     * 0.001);   // C2
    snapSmoothedToTargets();

    // ── DC-blocker coef (R form, sample-rate aware) ──────────────────────────
    mDcCoef = 1.0f - (juce::MathConstants<float>::twoPi * kDcBlockHz / (float) sampleRate);

    reset();
}

// ─────────────────────────────────────────────────────────────────────────────
void OverdriveDSP::reset()
{
    mPreLpf.reset();
    mLPF.reset();
    if (mOversampler) mOversampler->reset();
    mBandBuf.clear();
    mDcX_L = mDcY_L = 0.0f;
    mDcX_R = mDcY_R = 0.0f;
    mPedalNotchL.reset();
    mPedalNotchR.reset();
    mPedalHpfX[0] = mPedalHpfX[1] = mPedalHpfY[0] = mPedalHpfY[1] = 0.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
void OverdriveDSP::snapSmoothedToTargets()
{
    mPreAmpSmooth    .setCurrentAndTargetValue (mPreAmp);
    mColorSmooth     .setCurrentAndTargetValue (mColor);
    mPostFilterSmooth.setCurrentAndTargetValue (mPostFilter);
    mPostGainSmooth  .setCurrentAndTargetValue (mPostGain);
    mPreBandSmooth   .setCurrentAndTargetValue (mPreBand);
    mWetSmooth       .setCurrentAndTargetValue (mWet);
    mX100ScalarSmooth.setCurrentAndTargetValue (mX100 ? 100.0f : 1.0f);
    mBiasSmooth      .setCurrentAndTargetValue (mBias);
}

// ─────────────────────────────────────────────────────────────────────────────
// Setters - CPU-guarded. Coef recomputation moved INTO process() per spec §6c.
// ─────────────────────────────────────────────────────────────────────────────
void OverdriveDSP::setPreBand (float v)
{
    // A3 -- smoothed Q so knob drag doesn't step BPF resonance at block boundaries.
    const float n = juce::jlimit (0.0f, 1.0f, v);
    if (n != mPreBand) { mPreBand = n; mPreBandSmooth.setTargetValue (n); }
}

void OverdriveDSP::setColor (float hz)
{
    const float n = juce::jlimit (20.0f, 20000.0f, hz);
    if (n != mColor) { mColor = n; mColorSmooth.setTargetValue (n); }
}

void OverdriveDSP::setPreAmp (float v)
{
    const float n = juce::jlimit (0.0f, 10.0f, v);
    if (n != mPreAmp) { mPreAmp = n; mPreAmpSmooth.setTargetValue (n); }
}

void OverdriveDSP::setX100 (bool on)
{
    // A5 -- instead of an instantaneous 40 dB jump on toggle, ramp the scalar
    // 1 <-> 100 via a 20 ms SmoothedValue so the transition is click-free.
    if (on != mX100)
    {
        mX100 = on;
        mX100ScalarSmooth.setTargetValue (on ? 100.0f : 1.0f);
    }
}

void OverdriveDSP::setPostFilter (float hz)
{
    const float n = juce::jlimit (200.0f, 20000.0f, hz);
    if (n != mPostFilter) { mPostFilter = n; mPostFilterSmooth.setTargetValue (n); }
}

void OverdriveDSP::setPostGain (float dB)
{
    // QA-EffectsReview Task 5: attenuate-only (the reference's "gain reducer"; max = unity).
    const float n = juce::jlimit (-18.0f, 0.0f, dB);
    if (n != mPostGain) { mPostGain = n; mPostGainSmooth.setTargetValue (n); }
}

void OverdriveDSP::setWet (float v)
{
    // A4 -- smoothed Wet so blend drags don't zipper.
    const float n = juce::jlimit (0.0f, 1.0f, v);
    if (n != mWet) { mWet = n; mWetSmooth.setTargetValue (n); }
}

void OverdriveDSP::setBias (float v)
{
    // C2 -- asymmetric-bias knob; pre-shaper DC offset injects even harmonics.
    const float n = juce::jlimit (-1.0f, 1.0f, v);
    if (n != mBias) { mBias = n; mBiasSmooth.setTargetValue (n); }
}

void OverdriveDSP::setParallel (bool on)
{
    if (on != mParallel) mParallel = on;
}

void OverdriveDSP::stageOversampler (int factorLog2)
{
    // MESSAGE THREAD ONLY.  Builds the replacement oversampler and its OS-rate-
    // derived scalars off to the side, then publishes the whole group with one
    // release-store.  Never touches mOversampler -- the audio thread owns it.
    const int n = juce::jlimit (1, 4, factorLog2);

    auto os = std::make_unique<juce::dsp::Oversampling<float>> (
        2 /* numChannels */,
        n,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true /* maxQuality */);
    os->initProcessing ((size_t) mMaxBlock);

    const double osRate = mSampleRate * (double) (1 << n);
    mOsPendLog2      = n;
    mOsPendLatency   = (int) std::ceil (os->getLatencyInSamples());
    mOsPendPedalHpfR = (float) (1.0 - (juce::MathConstants<double>::twoPi * 720.0 / osRate));
    // Assigning here also destructs the PREVIOUSLY retired oversampler -- on
    // this thread, which is the point of the pattern.
    mOsPending = std::move (os);
    mOsSwapPending.store (true, std::memory_order_release);
}

void OverdriveDSP::drainOversamplerSwap() noexcept
{
    // AUDIO THREAD.  Adopts oversampler + factor + latency + the OS-rate pedal
    // HPF coef together, so process() can never index the upsampled block with
    // a factor the live oversampler was not built for.
    if (! mOsSwapPending.load (std::memory_order_acquire)) return;
    std::swap (mOversampler, mOsPending);
    mOsLog2Active = mOsPendLog2;
    mLatencySamples.store (mOsPendLatency, std::memory_order_relaxed);
    mPedalHpfR    = mOsPendPedalHpfR;
    mOsSwapPending.store (false, std::memory_order_release);
}

void OverdriveDSP::setOversamplingFactor (int factorLog2)
{
    // C5 -- 2x=1, 4x=2, 8x=3, 16x=4.
    const int n = juce::jlimit (1, 4, factorLog2);
    if (n == mOsLog2) return;
    mOsLog2 = n;
    // Stage rather than prepare(): this runs from the UI thread while the audio
    // thread may be inside processSamplesUp/Down on the current oversampler.
    // Host-side PDC still has to be refreshed by the EffectRack once the swap
    // lands (normal pattern when latency changes).
    if (mSampleRate > 0.0)
        stageOversampler (n);
}

// ── Legacy compatibility ──────────────────────────────────────────────────────
void OverdriveDSP::setDrive (float drive)
{
    setPreAmp (juce::jlimit (0.0f, 1.0f, drive) * 10.0f);
}

void OverdriveDSP::setTone (float tone)
{
    setPostFilter (500.0f + juce::jlimit (0.0f, 1.0f, tone) * 7500.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Process block
// ─────────────────────────────────────────────────────────────────────────────
void OverdriveDSP::process (juce::AudioBuffer<float>& buffer)
{
    // Ahead of the bypass early-out: a staged oversampler must be adopted even
    // on a block this effect otherwise does nothing with.
    drainOversamplerSwap();

    if (bypassed) return;

    juce::ScopedNoDenormals noDenormals;   // A1

    const int bufCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const int numCh      = std::min (bufCh, 2);
    if (numSamples <= 0 || numCh <= 0)    return;
    if (numSamples > mMaxBlock)           return;   // block-size guard
    if (mOversampler == nullptr)          return;   // not prepared

    // I-5 (2026-05-02): OD Style Pedal-mode algorithm.  When mType==Pedal,
    // run the pedal-specific frequency-split + dual cascaded soft-clip path
    // and return early -- the existing Rack chain stays untouched.
    //
    // Algorithm:
    //   1. Split input at 80 Hz: HPF feeds the clipping path, LPF feeds the
    //      clean blend (sub-80 keeps the low end intact, classic OD-3
    //      character that "doesn't get muddy under heavy chords").
    //   2. 4x oversample the HPF path.
    //   3. Dual cascaded tanh soft-clip (two stages, second hits harder).
    //   4. Downsample.
    //   5. Sum drive + clean.
    //   6. Tone LPF (PostFilter Hz, user knob).
    //   7. PostGain (Level, user knob).
    if (mType == Type::Pedal)
    {
        // Snapshot HPF input into the clean buf BEFORE we overwrite buffer
        // with the HPF result.  Then run an LPF copy on the clean buf to get
        // the sub-80 component.
        if (mPedalCleanBuf.getNumChannels() < numCh
         || mPedalCleanBuf.getNumSamples() < numSamples)
            mPedalCleanBuf.setSize (numCh, numSamples, false, false, true);

        for (int ch = 0; ch < numCh; ++ch)
            mPedalCleanBuf.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        // HPF the main buffer (drive path).
        {
            juce::dsp::AudioBlock<float> blk (buffer);
            auto sub = blk.getSubBlock (0, (size_t) numSamples)
                          .getSubsetChannelBlock (0, (size_t) numCh);
            juce::dsp::ProcessContextReplacing<float> ctx (sub);
            mPedalSplitHpf.process (ctx);
        }
        // LPF the clean buf (clean blend path).
        {
            juce::dsp::AudioBlock<float> blk (mPedalCleanBuf);
            auto sub = blk.getSubBlock (0, (size_t) numSamples)
                          .getSubsetChannelBlock (0, (size_t) numCh);
            juce::dsp::ProcessContextReplacing<float> ctx (sub);
            mPedalSplitLpf.process (ctx);
        }

        // QA-EffectsReview Task 5: 500 Hz mid-notch on the drive path, pre-clip (1x).
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            auto&  notch = (ch == 0) ? mPedalNotchL : mPedalNotchR;
            for (int i = 0; i < numSamples; ++i)
                d[i] = notch.processSample (d[i]);
        }

        // Dual cascaded soft-clip on the drive path (oversampled).  QA-EffectsReview
        // Task 5: drive ceiling raised 1+preAmp*4 -> 1+preAmp*13 (~+43 dB) and a
        // 720 Hz 1-pole HPF inserted between the two clip stages.
        const float drive = 1.0f + juce::jlimit (0.0f, 10.0f, mPreAmp) * 13.0f;
        juce::dsp::AudioBlock<float> driveBlock (buffer);
        auto driveSub = driveBlock.getSubBlock (0, (size_t) numSamples)
                                   .getSubsetChannelBlock (0, (size_t) numCh);
        auto upBlock = mOversampler->processSamplesUp (driveSub);
        const int upN   = (int) upBlock.getNumSamples();
        const int upChs = (int) upBlock.getNumChannels();
        for (int ch = 0; ch < upChs; ++ch)
        {
            float* d = upBlock.getChannelPointer ((size_t) ch);
            const int hch = juce::jmin (ch, 1);
            for (int i = 0; i < upN; ++i)
            {
                // Stage 1: gentle pre-clip (warms the upper-mids).
                const float s1 = std::tanh (d[i] * drive);
                // 720 Hz 1-pole HPF between stages -- strips low-end mud so the
                // second cascade stays tight under chords.
                const float hy = mPedalHpfR * (mPedalHpfY[hch] + s1 - mPedalHpfX[hch]);
                mPedalHpfX[hch] = s1;
                mPedalHpfY[hch] = hy;
                // Stage 2: harder cascade (the pedal's punchy attack).
                d[i] = std::tanh (hy * 1.6f);
            }
        }
        mOversampler->processSamplesDown (driveSub);

        // Sum drive + clean back into the main buffer.
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* drv = buffer.getWritePointer (ch);
            const float* cln = mPedalCleanBuf.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                drv[i] += cln[i];
        }

        // Tone (PostFilter LPF) + Level (PostGain) at 1x.
        const float toneHz = juce::jlimit (200.0f, 8000.0f, mPostFilter);
        mLPF.setCutoffFrequency (toneHz);
        {
            juce::dsp::AudioBlock<float> blk (buffer);
            auto sub = blk.getSubBlock (0, (size_t) numSamples)
                          .getSubsetChannelBlock (0, (size_t) numCh);
            juce::dsp::ProcessContextReplacing<float> ctx (sub);
            mLPF.process (ctx);
        }
        if (mPostGain != 0.0f)
            buffer.applyGain (juce::Decibels::decibelsToGain (mPostGain, -60.0f));

        return;   // Pedal mode is done; skip the Rack chain below.
    }

    // C5 -- the factor the LIVE oversampler was built with (adopted together
    // with it in drainOversamplerSwap), not the user-intent mOsLog2.
    const int kOsNow = 1 << mOsLog2Active;
    const float nyq  = (float) mSampleRate * 0.49f;

    // ── Step 1: in-series pre-LP. The whole signal is driven (no parallel clean ──
    // band). PreBand is the AMOUNT of low-pass blended into the drive input
    // (0 = full bandwidth, 1 = fully low-passed at Color). Writes the drive input
    // into mBandBuf; leaves `buffer` holding the original input for the wet/dry blend.
    //
    // A8: if input is mono, duplicate ch0 into mBandBuf ch1 before the 2-channel
    // oversampler runs so its second-channel state sees valid data.
    for (int n = 0; n < numSamples; ++n)
    {
        // A9 -- per-sample smoothed cutoff (Color). Pre-LP Q is fixed Butterworth.
        const float color = juce::jlimit (20.0f, nyq, mColorSmooth.getNextValue());
        const float amt   = mPreBandSmooth.getNextValue();   // 0..1 low-pass amount
        mPreLpf.setCutoffFrequency (color);

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float x  = buffer.getSample (ch, n);
            const float lp = mPreLpf.processSample (ch, x);
            // Blend dry -> low-passed by the PreBand amount, then drive in series.
            mBandBuf.setSample (ch, n, x + amt * (lp - x));
        }
        // A8: mono input -- pad ch1 with ch0 so the 2ch oversampler has valid data.
        if (numCh == 1)
            mBandBuf.setSample (1, n, mBandBuf.getSample (0, n));
    }

    // ── Step 2: oversample band -> soft-clip sigmoid -> downsample ──────────
    // Always present the oversampler with 2 channels (A8); we read back only
    // the channels we'll write out.
    juce::dsp::AudioBlock<float> bandBlock (mBandBuf.getArrayOfWritePointers(),
                                            (size_t) 2, 0, (size_t) numSamples);
    auto upBlock = mOversampler->processSamplesUp (bandBlock);

    const int upN = (int) upBlock.getNumSamples();
    jassert (upN == numSamples * kOsNow);

    for (int baseN = 0; baseN < numSamples; ++baseN)
    {
        // Consume ONE smoothed drive + x100-scalar + bias per base sample, reuse
        // across the kOsNow upsampled sub-samples. A5: x100 ramps smoothly
        // instead of snapping -- no pop on toggle.
        const float preAmp   = mPreAmpSmooth    .getNextValue();
        const float x100s    = mX100ScalarSmooth.getNextValue();
        const float bias     = mBiasSmooth      .getNextValue();
        const float drive    = preAmp * x100s;

        // C1 -- swap atan for x/(1+|x|) soft-clip sigmoid (spec formula).
        // C2 -- pre-shaper bias adds asymmetric even harmonics; post-shaper
        // subtract the DC equivalent so the static DC floor is removed (the
        // 5 Hz DC-blocker downstream catches any residual drift).
        const float biasOffset = bias * 0.3f;                                // scaled so knob feels musical
        const float biasFloor  = (biasOffset) / (1.0f + std::abs (biasOffset));   // f(bias) with drive=1

        for (int j = 0; j < kOsNow; ++j)
        {
            const int upIdx = baseN * kOsNow + j;
            if (upIdx >= upN) break;
            for (int ch = 0; ch < 2; ++ch)
            {
                const float s       = upBlock.getSample (ch, upIdx);
                const float driven  = drive * s + biasOffset;
                // Soft-clip sigmoid: x/(1+|x|). Saturates to +/-1 smoothly.
                const float shaped  = driven / (1.0f + std::abs (driven));
                // Remove the static DC the bias would otherwise inject.
                upBlock.setSample (ch, upIdx, shaped - biasFloor);
            }
        }
    }

    mOversampler->processSamplesDown (bandBlock);   // mBandBuf now holds SHAPED band

    // ── Step 3: recombine + post-LPF + gain + clip + DC-block + wet/dry ─────
    const float R       = mDcCoef;
    const bool parallel = mParallel;   // C4

    for (int n = 0; n < numSamples; ++n)
    {
        // A9 -- per-sample LPF coef refresh (same reasoning as BPF above).
        const float pfilter = juce::jlimit (20.0f, nyq, mPostFilterSmooth.getNextValue());
        mLPF.setCutoffFrequency (pfilter);
        // LPF Q constant (Butterworth) -- no need to set per sample, but leave
        // it on the cheap path. setResonance is trivial.
        mLPF.setResonance (juce::MathConstants<float>::sqrt2 * 0.5f);

        const float postGain = juce::Decibels::decibelsToGain (mPostGainSmooth.getNextValue());
        const float wet      = mWetSmooth.getNextValue();   // A4 smoothed
        const float dry      = 1.0f - wet;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float x   = buffer.getSample (ch, n);
            // In-series: the shaped band IS the whole driven signal (no clean residual).
            float out       = mBandBuf.getSample (ch, n);

            out = mLPF.processSample (ch, out);
            out *= postGain;
            out = juce::jlimit (-1.0f, 1.0f, out);

            // 5 Hz DC-blocker (R form) -- separate state per channel
            if (ch == 0)
            {
                const float y = out - mDcX_L + R * mDcY_L;
                mDcX_L = out; mDcY_L = y; out = y;
            }
            else
            {
                const float y = out - mDcX_R + R * mDcY_R;
                mDcX_R = out; mDcY_R = y; out = y;
            }

            // C4 -- Parallel mode: out = x + wet*processed (dry stays full).
            // Blend mode (default): out = dry*x + wet*processed.
            const float mixed = parallel
                ? (x + wet * out)
                : (dry * x + wet * out);
            buffer.setSample (ch, n, mixed);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Serialisation
// ─────────────────────────────────────────────────────────────────────────────
void OverdriveDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("OverdriveDSP");
    state.setProperty ("preBand",    mPreBand,    nullptr);
    state.setProperty ("color",      mColor,      nullptr);
    state.setProperty ("preAmp",     mPreAmp,     nullptr);
    state.setProperty ("x100",       mX100,       nullptr);
    state.setProperty ("postFilter", mPostFilter, nullptr);
    state.setProperty ("postGain",   mPostGain,   nullptr);
    state.setProperty ("wet",        mWet,        nullptr);
    state.setProperty ("bias",       mBias,       nullptr);   // C2
    state.setProperty ("parallel",   mParallel,   nullptr);   // C4
    state.setProperty ("osLog2",     mOsLog2,     nullptr);   // C5
    state.setProperty ("type",       (int) mType, nullptr);   // I-4 (2026-05-02)
    auto xml = state.createXml();
    if (xml) juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void OverdriveDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (!xml || xml->getTagName() != "OverdriveDSP") return;

    mPreBand    = (float) xml->getDoubleAttribute ("preBand",    mPreBand);
    mColor      = (float) xml->getDoubleAttribute ("color",      mColor);
    mPreAmp     = (float) xml->getDoubleAttribute ("preAmp",     mPreAmp);
    mX100       =         xml->getBoolAttribute   ("x100",       mX100);
    mPostFilter = (float) xml->getDoubleAttribute ("postFilter", mPostFilter);
    // QA-EffectsReview Task 5: enforce attenuate-only on legacy presets that saved a boost.
    mPostGain   = juce::jlimit (-18.0f, 0.0f, (float) xml->getDoubleAttribute ("postGain", mPostGain));
    mWet        = (float) xml->getDoubleAttribute ("wet",        mWet);
    mBias       = (float) xml->getDoubleAttribute ("bias",       mBias);       // C2
    mParallel   =         xml->getBoolAttribute   ("parallel",   mParallel);   // C4
    // I-4 (2026-05-02): Type umbrella; defaults to Rack for old projects.
    mType = static_cast<Type> (juce::jlimit (0, 1,
        xml->getIntAttribute ("type", (int) Type::Rack)));
    const int loadedOs = xml->getIntAttribute ("osLog2", mOsLog2);             // C5
    const bool osChanged = (loadedOs != mOsLog2);
    mOsLog2 = juce::jlimit (1, 4, loadedOs);

    // O5: refresh filter coefs from restored parameters (old code only
    // called setPostFilter/setPostGain which missed BPF updates).
    // O7: reset filter + oversampler + DC-blocker state so no stale transient.
    snapSmoothedToTargets();
    if (mSampleRate > 0.0)
    {
        // C5 -- if OS factor changed in the preset, STAGE the new oversampler.
        // The preset menu hands this call the LIVE DSP the audio thread is
        // processing, so prepare() here would free the oversampler out from
        // under it.
        if (osChanged)
            stageOversampler (mOsLog2);

        const float nyq = (float) mSampleRate * 0.49f;
        mPreLpf.setCutoffFrequency (juce::jlimit (20.0f, nyq, mColor));
        mPreLpf.setResonance       (juce::MathConstants<float>::sqrt2 * 0.5f);
        mLPF.setCutoffFrequency (juce::jlimit (20.0f, nyq, mPostFilter));
        mLPF.setResonance       (juce::MathConstants<float>::sqrt2 * 0.5f);
        mPreLpf.reset();
        mLPF.reset();
        if (mOversampler) mOversampler->reset();
        mDcX_L = mDcY_L = mDcX_R = mDcY_R = 0.0f;
    }
}
