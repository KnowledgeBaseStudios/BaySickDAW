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
void OverdriveDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = juce::jmax (1, maxBlockSize);

    // ── TPT filters (stereo) ─────────────────────────────────────────────────
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) mMaxBlock;
    spec.numChannels      = 2;
    mBPF.prepare (spec);
    mLPF.prepare (spec);
    mBPF.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    mLPF.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    mBPF.setCutoffFrequency (juce::jlimit (20.0f, (float) sampleRate * 0.49f, mColor));
    mBPF.setResonance       (0.5f + (1.0f - mPreBand) * 9.5f);
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
    mLatencySamples = (int) std::ceil (mOversampler->getLatencyInSamples());

    // ── Scratch buffers (stereo) ─────────────────────────────────────────────
    mBandBuf    .setSize (2, mMaxBlock, false, true, true);
    mResidualBuf.setSize (2, mMaxBlock, false, true, true);

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
    mBPF.reset();
    mLPF.reset();
    if (mOversampler) mOversampler->reset();
    mBandBuf.clear();
    mResidualBuf.clear();
    mDcX_L = mDcY_L = 0.0f;
    mDcX_R = mDcY_R = 0.0f;
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
// refreshFilterCoefs() retired in the §6 retrospective pass -- A9 moved coef
// updates inline into the per-sample process loop so fast Color / PostFilter
// sweeps don't step at block boundaries. Function intentionally left defined
// as a no-op in case future code still links against it; nothing in this
// translation unit calls it.
void OverdriveDSP::refreshFilterCoefs (int /*numSamples*/) {}

// ─────────────────────────────────────────────────────────────────────────────
// Setters — CPU-guarded. Coef recomputation moved INTO process() per spec §6c.
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
    const float n = juce::jlimit (-18.0f, 18.0f, dB);
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

void OverdriveDSP::setOversamplingFactor (int factorLog2)
{
    // C5 -- 2x=1, 4x=2, 8x=3, 16x=4.
    const int n = juce::jlimit (1, 4, factorLog2);
    if (n == mOsLog2) return;
    mOsLog2 = n;
    // Reallocate the oversampler for the new factor. prepare() does the work.
    // We only call this from the UI thread; the next process() block will see
    // the new latency via getLatencySamples(). Host-side PDC must be refreshed
    // by the EffectRack (normal pattern when latency changes).
    if (mSampleRate > 0.0)
        prepare (mSampleRate, mMaxBlock);
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
    if (bypassed) return;

    juce::ScopedNoDenormals noDenormals;   // A1

    const int bufCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const int numCh      = std::min (bufCh, 2);
    if (numSamples <= 0 || numCh <= 0)    return;
    if (numSamples > mMaxBlock)           return;   // block-size guard
    if (mOversampler == nullptr)          return;   // not prepared

    // C5 pulls the current OS factor out of the live oversampler (updated on
    // setOversamplingFactor via prepare() reallocation).
    const int kOsNow = 1 << mOsLog2;
    const float nyq  = (float) mSampleRate * 0.49f;

    // ── Step 1: base-rate BPF with per-sample smoothed coef refresh (A9). ────
    // Also split into (band, residual). Writes band into mBandBuf, residual
    // into mResidualBuf. Leaves `buffer` holding original input for the final
    // wet/dry blend.
    //
    // A8: if input is mono, duplicate ch0 into mBandBuf ch1 before the
    // 2-channel oversampler runs so its second-channel state sees valid data.
    for (int n = 0; n < numSamples; ++n)
    {
        // A9 -- advance and push smoothed filter coefs per sample. `setCutoff /
        // setResonance` on TPT SVF is a small trig/div -- ~30 cycles.
        const float color = juce::jlimit (20.0f, nyq, mColorSmooth.getNextValue());
        const float preBd = mPreBandSmooth.getNextValue();
        const float bpfQ  = 0.5f + (1.0f - preBd) * 9.5f;
        mBPF.setCutoffFrequency (color);
        mBPF.setResonance       (bpfQ);

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float x    = buffer.getSample (ch, n);
            const float band = mBPF.processSample (ch, x);
            mBandBuf    .setSample (ch, n, band);
            mResidualBuf.setSample (ch, n, x - band);
        }
        // A8: mono input -- pad ch1 with ch0 so the 2ch oversampler has valid data.
        if (numCh == 1)
        {
            mBandBuf    .setSample (1, n, mBandBuf    .getSample (0, n));
            mResidualBuf.setSample (1, n, mResidualBuf.getSample (0, n));
        }
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
            const float x       = buffer.getSample (ch, n);
            const float shaped  = mBandBuf    .getSample (ch, n);
            const float residual= mResidualBuf.getSample (ch, n);
            float out = shaped + residual;

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
    mPostGain   = (float) xml->getDoubleAttribute ("postGain",   mPostGain);
    mWet        = (float) xml->getDoubleAttribute ("wet",        mWet);
    mBias       = (float) xml->getDoubleAttribute ("bias",       mBias);       // C2
    mParallel   =         xml->getBoolAttribute   ("parallel",   mParallel);   // C4
    const int loadedOs = xml->getIntAttribute ("osLog2", mOsLog2);             // C5
    const bool osChanged = (loadedOs != mOsLog2);
    mOsLog2 = juce::jlimit (1, 4, loadedOs);

    // O5: refresh filter coefs from restored parameters (old code only
    // called setPostFilter/setPostGain which missed BPF updates).
    // O7: reset filter + oversampler + DC-blocker state so no stale transient.
    snapSmoothedToTargets();
    if (mSampleRate > 0.0)
    {
        // C5 -- if OS factor changed in the preset, reallocate the oversampler.
        // prepare() does everything, including filter reset + coef refresh.
        if (osChanged)
        {
            prepare (mSampleRate, mMaxBlock);
            return;
        }
        const float nyq = (float) mSampleRate * 0.49f;
        mBPF.setCutoffFrequency (juce::jlimit (20.0f, nyq, mColor));
        mBPF.setResonance       (0.5f + (1.0f - mPreBand) * 9.5f);
        mLPF.setCutoffFrequency (juce::jlimit (20.0f, nyq, mPostFilter));
        mLPF.setResonance       (juce::MathConstants<float>::sqrt2 * 0.5f);
        mBPF.reset();
        mLPF.reset();
        if (mOversampler) mOversampler->reset();
        mDcX_L = mDcY_L = mDcX_R = mDcY_R = 0.0f;
    }
}
