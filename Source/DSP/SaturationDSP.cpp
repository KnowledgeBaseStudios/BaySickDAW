#include "SaturationDSP.h"

namespace
{
    // Smoother ramp times (ms)
    constexpr float kFlowersMs   = 15.0f;
    constexpr float kDabsMs      = 15.0f;
    constexpr float kReliefMs    = 20.0f;
    constexpr float kSensMs      = 20.0f;   // linear-domain
    constexpr float kTonePreMs   = 20.0f;
    constexpr float kTonePostMs  = 20.0f;
    constexpr float kWetMs       = 15.0f;
    constexpr float kOutMs       = 15.0f;
}

// -- Helpers ------------------------------------------------------------------

void SaturationDSP::updateFilters()
{
    if (mSampleRate <= 0.0) return;
    const float sr    = (float) mSampleRate;
    const float twoPi = juce::MathConstants<float>::twoPi;

    // 10 kHz shelf (base rate).
    mShelfCoef = std::exp (-twoPi * 10000.0f / sr);

    // 350 Hz LP (OS rate: sr * oversamplingFactor).
    const int   osFactor = 1 << juce::jlimit (1, 4, mOsLog2);
    const float srOS     = sr * (float) osFactor;
    mBassLPCoef = std::exp (-twoPi * 350.0f / srOS);

    // 9c: sample-rate-aware DC blocker targeting ~5 Hz cutoff.
    mDCCoef = 1.0f - twoPi * 5.0f / sr;

    // Console Dirty LF bloom: 1-pole low-shelf, ~90 Hz corner, +3.5 dB boost
    // (add-gain applied to the low-passed wet signal) -- the Neve low-end bump;
    // ear-tunable.
    mConsoleBloomCoef    = std::exp (-twoPi * 90.0f / sr);
    mConsoleBloomAddGain = juce::Decibels::decibelsToGain (3.5f) - 1.0f;

    // H-7 (2026-05-01): Vocal Body shaping biquads.
    //   Dip: -2 dB peaking @ 250 Hz Q=0.7  (cleans low-mid mud)
    //   Shelf: +2 dB high shelf @ 3 kHz Q=0.7  (vocal presence lift)
    using IIRCoeff = juce::dsp::IIR::Coefficients<float>;
    *mBodyDip  .state = *IIRCoeff::makePeakFilter   (mSampleRate,  250.0, 0.7,
                            juce::Decibels::decibelsToGain (-2.0f));
    *mBodyShelf.state = *IIRCoeff::makeHighShelf    (mSampleRate, 3000.0, 0.7,
                            juce::Decibels::decibelsToGain ( 2.0f));
}

void SaturationDSP::allocateScratch()
{
    if (mMaxBlock <= 0) return;
    mBandBuf.setSize (2, mMaxBlock, false, true, true);
    mBandBuf.clear();

    const size_t n = (size_t) mMaxBlock;
    mFlowersScr     .assign (n, 0.0f);
    mDabsScr        .assign (n, 0.0f);
    mReliefScr      .assign (n, 0.0f);
    mSensGainScr    .assign (n, 1.0f);
    mTonePreGainScr .assign (n, 1.0f);
    mTonePostGainScr.assign (n, 1.0f);
    mWetScr         .assign (n, 0.7f);
    mOutGainScr     .assign (n, 1.0f);
}

void SaturationDSP::snapSmoothedToTargets()
{
    mFlowersSmooth      .setCurrentAndTargetValue (mFlowers);
    mDabsSmooth         .setCurrentAndTargetValue (mDabs);
    mReliefSmooth       .setCurrentAndTargetValue (mBassRelief / 100.0f);
    mSensGainSmooth     .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (mSensitivity));
    mTonePreGainSmooth  .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (mTonePre));
    mTonePostGainSmooth .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (mTonePost));
    mWetSmooth          .setCurrentAndTargetValue (mWet / 100.0f);
    mOutGainSmooth      .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (mOut));
}

// -- DSPBase interface --------------------------------------------------------

void SaturationDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;
    updateFilters();

    // 9a/C2: (re)allocate the oversampler at the current factor.
    mOversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2 /* numChannels */,
        juce::jlimit (1, 4, mOsLog2),
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true /* maxQuality */);
    mOversampler->initProcessing ((size_t) maxBlockSize);
    mOversampler->reset();
    mLatencySamples = (int) std::ceil (mOversampler->getLatencyInSamples());

    allocateScratch();

    // 9d/C1: configure smoothers.
    mFlowersSmooth     .reset (sampleRate, kFlowersMs  * 0.001);
    mDabsSmooth        .reset (sampleRate, kDabsMs     * 0.001);
    mReliefSmooth      .reset (sampleRate, kReliefMs   * 0.001);
    mSensGainSmooth    .reset (sampleRate, kSensMs     * 0.001);
    mTonePreGainSmooth .reset (sampleRate, kTonePreMs  * 0.001);
    mTonePostGainSmooth.reset (sampleRate, kTonePostMs * 0.001);
    mWetSmooth         .reset (sampleRate, kWetMs      * 0.001);
    mOutGainSmooth     .reset (sampleRate, kOutMs      * 0.001);
    snapSmoothedToTargets();

    // H-7: prepare body-shaping biquads at base rate.
    juce::dsp::ProcessSpec bodySpec { sampleRate, (juce::uint32) maxBlockSize, 2 };
    mBodyDip  .prepare (bodySpec);
    mBodyShelf.prepare (bodySpec);
    mBodyDip  .reset();
    mBodyShelf.reset();

    // H-7: prepare Linkwitz-Riley splits for Keep Low / Keep High mode.
    // 4th-order (12 dB/oct each side) -- complementary so summed bands recombine flat.
    mLR250Lo.prepare (bodySpec); mLR250Hi.prepare (bodySpec);
    mLR4kLo .prepare (bodySpec); mLR4kHi .prepare (bodySpec);
    mLR250Lo.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    mLR250Hi.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    mLR4kLo .setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    mLR4kHi .setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    mLR250Lo.setCutoffFrequency (250.0f);
    mLR250Hi.setCutoffFrequency (250.0f);
    mLR4kLo .setCutoffFrequency (4000.0f);
    mLR4kHi .setCutoffFrequency (4000.0f);
    mLR250Lo.reset(); mLR250Hi.reset();
    mLR4kLo .reset(); mLR4kHi .reset();
    mSplitLo.setSize (2, maxBlockSize, false, false, true);
    mSplitHi.setSize (2, maxBlockSize, false, false, true);

    // H-10 (2026-05-02): prepare the Tape sub-engine alongside Tube/Console.
    // tapePrepare configures the Tape-only state (wow/flutter buffers, smoothers,
    // filter coefs); the existing mOversampler is reused for both code paths
    // since both run a 2-channel polyphase IIR oversampler at mOsLog2.
    tapePrepare (sampleRate, maxBlockSize);

    reset();
}

void SaturationDSP::reset()
{
    mPreLP_L = mPreLP_R = mPostLP_L = mPostLP_R = 0.0f;
    mBassLP_L = mBassLP_R = 0.0f;
    mDCx_L = mDCy_L = mDCx_R = mDCy_R = 0.0f;
    mConsoleBloomL = mConsoleBloomR = 0.0f;
    if (mOversampler) mOversampler->reset();
    mBandBuf.clear();
    mBodyDip  .reset();
    mBodyShelf.reset();

    // H-10: clear Tape sub-engine state alongside the Saturation chain.
    tapeReset();
}

// -- Setters (A2: CPU-guarded) ------------------------------------------------

void SaturationDSP::setFlowers (float v)
{
    const float n = juce::jlimit (0.0f, 10.0f, v);
    if (n != mFlowers) { mFlowers = n; mFlowersSmooth.setTargetValue (n); }
}
void SaturationDSP::setDabs (float v)
{
    const float n = juce::jlimit (0.0f, 10.0f, v);
    if (n != mDabs) { mDabs = n; mDabsSmooth.setTargetValue (n); }
}
void SaturationDSP::setSensitivity (float dB)
{
    const float n = juce::jlimit (-12.0f, 12.0f, dB);
    if (n != mSensitivity)
    {
        mSensitivity = n;
        mSensGainSmooth.setTargetValue (juce::Decibels::decibelsToGain (n));
    }
}
void SaturationDSP::setBassRelief (float v)
{
    const float n = juce::jlimit (0.0f, 100.0f, v);
    if (n != mBassRelief)
    {
        mBassRelief = n;
        mReliefSmooth.setTargetValue (n / 100.0f);
    }
}
void SaturationDSP::setTransformer (bool on)
{
    if (on != mTransformer) mTransformer = on;
}
void SaturationDSP::setConsoleMode (int m)
{
    const auto n = static_cast<ConsoleMode> (juce::jlimit (0, 1, m));
    if (n != mConsoleMode) mConsoleMode = n;
}
void SaturationDSP::setConsoleColor (bool on)
{
    if (on != mConsoleColor) mConsoleColor = on;
}
void SaturationDSP::setTubeType (int t)
{
    const int n = juce::jlimit (0, 2, t);
    if (n != mTubeType) mTubeType = n;
}
void SaturationDSP::setTonePre (float dB)
{
    const float n = juce::jlimit (-9.0f, 9.0f, dB);
    if (n != mTonePre)
    {
        mTonePre = n;
        mTonePreGainSmooth.setTargetValue (juce::Decibels::decibelsToGain (n));
    }
}
void SaturationDSP::setTonePost (float dB)
{
    const float n = juce::jlimit (-9.0f, 9.0f, dB);
    if (n != mTonePost)
    {
        mTonePost = n;
        mTonePostGainSmooth.setTargetValue (juce::Decibels::decibelsToGain (n));
    }
}
void SaturationDSP::setWet (float p)
{
    const float n = juce::jlimit (0.0f, 100.0f, p);
    if (n != mWet) { mWet = n; mWetSmooth.setTargetValue (n / 100.0f); }
}
void SaturationDSP::setOut (float dB)
{
    const float n = juce::jlimit (-18.0f, 18.0f, dB);
    if (n != mOut)
    {
        mOut = n;
        mOutGainSmooth.setTargetValue (juce::Decibels::decibelsToGain (n));
    }
}
void SaturationDSP::setAutoGain (bool on)
{
    if (on != mAutoGain) mAutoGain = on;
}
void SaturationDSP::setOversamplingFactor (int factorLog2)
{
    // C2: 2x=1, 4x=2, 8x=3, 16x=4.
    const int n = juce::jlimit (1, 4, factorLog2);
    if (n == mOsLog2) return;
    mOsLog2 = n;
    // Reallocate oversampler + refresh coefficients (bass split coef uses OS rate).
    if (mSampleRate > 0.0)
        prepare (mSampleRate, mMaxBlock);
}

// ---- Legacy compatibility ---------------------------------------------------
void SaturationDSP::setDrive (float drive) { setFlowers (juce::jlimit (0.0f, 1.0f, drive) * 10.0f); }
void SaturationDSP::setMix   (float mix)   { setWet     (juce::jlimit (0.0f, 1.0f, mix)   * 100.0f); }
void SaturationDSP::setType  (int   type)  { setTubeType (type); }

// ---- H-7 / H-10: umbrella setters -------------------------------------------
void SaturationDSP::setSatType (int t)
{
    // H-10 (2026-05-02): clamp 0..2 (was 0..1) to admit Tape.
    const Type n = static_cast<Type> (juce::jlimit (0, 2, t));
    if (n != mSatType) mSatType = n;
}
void SaturationDSP::setVocalBody (bool on)
{
    if (on != mVocalBody)
    {
        mVocalBody = on;
        // Reset filter state on toggle so a freshly-enabled body shaping
        // doesn't leak stale denormal-y state from a long inactive period.
        mBodyDip  .reset();
        mBodyShelf.reset();
    }
}

void SaturationDSP::setHarmonicsMode (int m)
{
    const HarmonicsMode n = static_cast<HarmonicsMode> (juce::jlimit (0, 2, m));
    if (n != mHarmonicsMode)
    {
        mHarmonicsMode = n;
        // Reset LR filter state on mode change so a fresh activation doesn't
        // start mid-decay.
        mLR250Lo.reset(); mLR250Hi.reset();
        mLR4kLo .reset(); mLR4kHi .reset();
    }
}

// -- Console engine (stateless, takes per-sample smoothed params) -------------
// Clean (SSL-style) vs Dirty (Neve-style) voicings; see the header decl for the
// per-arg meaning.  drive = mFlowers (0..10), color = mDabs (0..10) -- same
// input range as the Tube path so the Type switch keeps muscle memory.
float SaturationDSP::processConsole (float x, float drive, float color,
                                       bool colorOn, bool dirty, bool isLow) noexcept
{
    // Color = an asymmetric input bias -> even (2nd) harmonics.  The tanh(bias)
    // subtraction removes the operating-point DC and the sech^2(bias) slope
    // normalization holds low-level gain at unity, so Color changes harmonic
    // CONTENT not loudness, and it ramps continuously from bias=0 (no on-
    // threshold step -> no click).  bias=0 collapses to the plain soft-clip.
    // (Same gain-compensated asymmetric-shaper approach as the Opto warmth.)
    // Drive/bias coefficients are ear-calibration starting points.
    if (! dirty)
    {
        // Clean (SSL-style): transparent soft-clip; Drive saturates at normal
        // levels, output-compensated so enable doesn't jump the loudness.
        const float driveAmount = 1.0f + drive * 0.60f;          // 1.0..7.0
        const float bias  = colorOn ? color * 0.07f : 0.0f;      // 0..0.7
        const float sb    = std::tanh (bias);
        const float sech2 = 1.0f - sb * sb;                      // sech^2 = 1 - tanh^2
        return (std::tanh (x * driveAmount + bias) - sb) / (driveAmount * sech2);
    }

    // Dirty (Neve-style): low-frequency-weighted saturation -- the low band is
    // driven much harder than the high (transformer LF saturation), with a
    // slightly stronger even-harmonic Color.  Still unity at low level.
    const float bandDrive   = isLow ? 2.2f : 0.8f;
    const float driveAmount = (1.0f + drive * 0.60f) * bandDrive;
    const float bias  = colorOn ? color * 0.08f : 0.0f;          // 0..0.8
    const float sb    = std::tanh (bias);
    const float sech2 = 1.0f - sb * sb;
    return (std::tanh (x * driveAmount + bias) - sb) / (driveAmount * sech2);
}

// -- Tube engine (stateless, takes per-sample smoothed params) ---------------

float SaturationDSP::processTube (float x, float flowers, float dabs,
                                   int tubeType, bool transformer) noexcept
{
    const bool flowersActive = (flowers > 0.001f);
    const bool dabsActive    = (dabs    > 0.001f);

    if (!flowersActive && !dabsActive && !transformer)
        return x;

    // Flowers - tanh even harmonics.
    const float flowers_out = flowersActive ? std::tanh (flowers * x) : 0.0f;

    // Dabs - type-dependent odd/even shapers.
    float dabs_out = 0.0f;
    if (dabsActive)
    {
        switch (tubeType)
        {
            case 0: // Type A - atan + cubic
            {
                const float a = std::atan (dabs * x) / juce::MathConstants<float>::halfPi;
                dabs_out = a + 0.15f * (x * x * x);
                break;
            }
            case 1: // Type B - sign(tanh) * |tanh|^(2/3)
                    // A7: cbrt(t*t) is mathematically identical to |t|^(2/3) but ~3x faster.
            {
                const float t    = std::tanh (dabs * x);
                const float sign = (t >= 0.0f) ? 1.0f : -1.0f;
                dabs_out = sign * std::cbrt (t * t);
                break;
            }
            case 2: // Type C - x/(1+|d*x|) foldback
            {
                dabs_out = x / (1.0f + std::abs (dabs * x));
                break;
            }
            default:
                dabs_out = x;
                break;
        }
    }

    float out = flowers_out + dabs_out;

    if (transformer)
        out += 0.005f * std::tanh (8.0f * x);

    return out;
}

// -- Process block ------------------------------------------------------------

void SaturationDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;

    // H-10 (2026-05-02): Tape branches off to its own dedicated body --
    // bit-exact port of the legacy TapeDSP algorithm.  Tube + Console
    // share the FDN/console body that follows.  Each path has independent
    // state so switching modes never leaks one engine's tail into another.
    if (mSatType == Type::Tape)
    {
        processTape (buffer);
        return;
    }

    juce::ScopedNoDenormals noDenormals;   // A1

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0) return;
    if (numSamples > mMaxBlock) return;
    if (mOversampler == nullptr) return;
    if (mBandBuf.getNumSamples() < numSamples) return;

    const int numCh     = std::min (buffer.getNumChannels(), 2);
    const int numChOS   = 2;   // oversampler is always stereo
    const int kOsNow    = 1 << juce::jlimit (1, 4, mOsLog2);

    const float a_shelf = mShelfCoef;
    const float a_bass  = mBassLPCoef;
    const int   tubeTy  = mTubeType;
    const bool  transf  = mTransformer;

    // Console voicing (constant across the block).
    const bool  consoleDirty   = (mSatType == Type::Console
                                  && mConsoleMode == ConsoleMode::Dirty);
    const bool  consoleColorOn = mConsoleColor;
    const float aBloom         = mConsoleBloomCoef;
    const float bloomAdd       = mConsoleBloomAddGain;

    // H-7 (2026-05-01): Keep Low / Keep High routing.
    // Splits the input into two LR4-complementary bands, saturates only one,
    // sums the kept (un-saturated) band back at the end.  Normal mode
    // bypasses the split entirely.  mSplitLo holds the kept band when active.
    const HarmonicsMode hmode = mHarmonicsMode;
    const bool routeSplit = (hmode != HarmonicsMode::Normal);
    if (routeSplit)
    {
        // Make sure scratch buffers can hold this block.
        if (mSplitLo.getNumSamples() < numSamples)
            mSplitLo.setSize (2, numSamples, false, false, true);
        if (mSplitHi.getNumSamples() < numSamples)
            mSplitHi.setSize (2, numSamples, false, false, true);

        auto& lpFilt = (hmode == HarmonicsMode::KeepLow) ? mLR250Lo : mLR4kLo;
        auto& hpFilt = (hmode == HarmonicsMode::KeepLow) ? mLR250Hi : mLR4kHi;

        // Copy buffer into mSplitLo (then filter it down to lows) and into
        // mSplitHi (then filter it up to highs).
        for (int ch = 0; ch < numCh; ++ch)
        {
            mSplitLo.copyFrom (ch, 0, buffer, ch, 0, numSamples);
            mSplitHi.copyFrom (ch, 0, buffer, ch, 0, numSamples);
        }
        juce::dsp::AudioBlock<float> loBlk (mSplitLo.getArrayOfWritePointers(),
                                             (size_t) numCh, 0, (size_t) numSamples);
        juce::dsp::AudioBlock<float> hiBlk (mSplitHi.getArrayOfWritePointers(),
                                             (size_t) numCh, 0, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> loCtx (loBlk);
        juce::dsp::ProcessContextReplacing<float> hiCtx (hiBlk);
        lpFilt.process (loCtx);
        hpFilt.process (hiCtx);

        // KeepLow: saturate the highs, keep lows untouched.  KeepHigh: opposite.
        // Replace `buffer` with the band that goes through the saturator.
        // We'll sum the kept band back at the very end of process().
        if (hmode == HarmonicsMode::KeepLow)
        {
            for (int ch = 0; ch < numCh; ++ch)
                buffer.copyFrom (ch, 0, mSplitHi, ch, 0, numSamples);
            // mSplitLo holds the kept (untouched) lows.
        }
        else // KeepHigh
        {
            for (int ch = 0; ch < numCh; ++ch)
                buffer.copyFrom (ch, 0, mSplitLo, ch, 0, numSamples);
            // mSplitHi holds the kept (untouched) highs.
        }
    }

    // ---- Phase 0: consume smoothers into scratch arrays --------------------
    for (int n = 0; n < numSamples; ++n)
    {
        mFlowersScr[(size_t) n]      = mFlowersSmooth     .getNextValue();
        mDabsScr[(size_t) n]         = mDabsSmooth        .getNextValue();
        mReliefScr[(size_t) n]       = mReliefSmooth      .getNextValue();
        mSensGainScr[(size_t) n]     = mSensGainSmooth    .getNextValue();
        mTonePreGainScr[(size_t) n]  = mTonePreGainSmooth .getNextValue();
        mTonePostGainScr[(size_t) n] = mTonePostGainSmooth.getNextValue();
        mWetScr[(size_t) n]          = mWetSmooth         .getNextValue();
        mOutGainScr[(size_t) n]      = mOutGainSmooth     .getNextValue();
    }

    // ---- Phase 1: sensitivity + tone pre (base rate) ------------------------
    // Pad mono input to stereo in the scratch band buffer so the oversampler
    // always sees 2 channels (matches the ch-1 read guard at the end of Phase 3).
    for (int ch = 0; ch < numChOS; ++ch)
    {
        float* dst = mBandBuf.getWritePointer (ch);

        if (ch < numCh)
        {
            const float* src = buffer.getReadPointer (ch);
            float& preLP = (ch == 0) ? mPreLP_L : mPreLP_R;
            for (int n = 0; n < numSamples; ++n)
            {
                const float g   = mSensGainScr[(size_t) n];
                const float gPre= mTonePreGainScr[(size_t) n];
                float x = src[n] * g;
                preLP = (1.0f - a_shelf) * x + a_shelf * preLP;
                x = gPre * x + (1.0f - gPre) * preLP;
                dst[n] = x;
            }
        }
        else
        {
            // Mono input: duplicate channel 0 into channel 1's scratch so the
            // oversampler has valid stereo data. Channel 1 state is unused.
            const float* src0 = mBandBuf.getReadPointer (0);
            for (int n = 0; n < numSamples; ++n)
                dst[n] = src0[n];
        }
    }

    // ---- Phase 2: oversampled tube region -----------------------------------
    // Inside the OS domain: split into low/high via 350 Hz LP (OS-rate coef),
    // run tube on each band, apply relief blend on low, recombine, optional
    // Transformer, write back. The BassLP state ticks kOsNow x per base sample.
    juce::dsp::AudioBlock<float> bandBlock (mBandBuf.getArrayOfWritePointers(),
                                             (size_t) numChOS, 0, (size_t) numSamples);
    auto upBlock = mOversampler->processSamplesUp (bandBlock);

    const int upN = (int) upBlock.getNumSamples();
    jassert (upN == numSamples * kOsNow);

    for (int ch = 0; ch < numChOS; ++ch)
    {
        float* up = upBlock.getChannelPointer ((size_t) ch);
        float& bassLP = (ch == 0) ? mBassLP_L : mBassLP_R;

        for (int osN = 0; osN < upN; ++osN)
        {
            const int baseIdx = osN / kOsNow;
            const float flowers = mFlowersScr[(size_t) baseIdx];
            const float dabs    = mDabsScr   [(size_t) baseIdx];
            const float relief  = mReliefScr [(size_t) baseIdx];

            const float x_os = up[osN];

            bassLP = (1.0f - a_bass) * x_os + a_bass * bassLP;
            const float low  = bassLP;
            const float high = x_os - low;

            // H-7 (2026-05-01): branch the inner saturation by Type.  Tube path
            // bit-exact preserved (mSatType default == Tube).  Console path uses
            // the Clean/Dirty voicing with its own Color enable.
            float low_proc, high_proc;
            if (mSatType == Type::Console)
            {
                low_proc  = processConsole (low,  flowers, dabs, consoleColorOn, consoleDirty, true);
                high_proc = processConsole (high, flowers, dabs, consoleColorOn, consoleDirty, false);
            }
            else
            {
                low_proc  = processTube (low,  flowers, dabs, tubeTy, transf);
                high_proc = processTube (high, flowers, dabs, tubeTy, transf);
            }
            // Dirty console drops bass relief so the LF-weighted saturated low
            // dominates (Neve low-end character); Tube + Clean keep the knob.
            const float effRelief = consoleDirty ? 0.0f : relief;
            const float low_out = effRelief * low + (1.0f - effRelief) * low_proc;

            up[osN] = low_out + high_proc;
        }
    }

    mOversampler->processSamplesDown (bandBlock);   // mBandBuf now holds tube_out

    // ---- Phase 3: DC block + tone post + wet/dry + out gain (base rate) -----
    for (int ch = 0; ch < numCh; ++ch)
    {
        float*       samples = buffer.getWritePointer (ch);
        const float* tube    = mBandBuf.getReadPointer (ch);

        float& postLP  = (ch == 0) ? mPostLP_L : mPostLP_R;
        float& dcX     = (ch == 0) ? mDCx_L    : mDCx_R;
        float& dcY     = (ch == 0) ? mDCy_L    : mDCy_R;
        float& bloomLP = (ch == 0) ? mConsoleBloomL : mConsoleBloomR;

        for (int n = 0; n < numSamples; ++n)
        {
            const float dry_in   = samples[n];   // original (for wet/dry)
            const float gPost    = mTonePostGainScr[(size_t) n];
            const float wet      = mWetScr        [(size_t) n];
            const float dry      = 1.0f - wet;
            const float outLin   = mOutGainScr    [(size_t) n];
            // 9b Auto-Gain: multiply 1/sensGain into out gain when enabled.
            // Sens gain is already in mSensGainScr; divide by it to cancel the boost.
            const float autoComp = mAutoGain
                ? (1.0f / std::max (0.001f, mSensGainScr[(size_t) n]))
                : 1.0f;
            const float finalOut = outLin * autoComp;

            float tube_out = tube[n];

            // DC block: y[n] = x[n] - x[n-1] + R * y[n-1]
            const float dc_in = tube_out;
            tube_out = dc_in - dcX + mDCCoef * dcY;
            dcX = dc_in;
            dcY = tube_out;

            // Console Dirty LF bloom: 1-pole low-shelf boost on the wet path.
            if (consoleDirty)
            {
                bloomLP   = (1.0f - aBloom) * tube_out + aBloom * bloomLP;
                tube_out += bloomAdd * bloomLP;
            }

            // Tone post (base-rate shelf)
            postLP = (1.0f - a_shelf) * tube_out + a_shelf * postLP;
            tube_out = gPost * tube_out + (1.0f - gPost) * postLP;

            // Wet/dry + out gain (+ auto-gain compensation)
            samples[n] = (dry * dry_in + wet * tube_out) * finalOut;
        }
    }

    // H-7 (2026-05-01): Vocal Body shaping -- post-saturation peaking dip +
    // high shelf.  Type-agnostic; applies regardless of Tube vs Console mode
    // when mVocalBody is on.  Default off preserves prior audio.
    if (mVocalBody)
    {
        juce::dsp::AudioBlock<float> outBlock (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (outBlock);
        mBodyDip  .process (ctx);
        mBodyShelf.process (ctx);
    }

    // H-7 (2026-05-01): Keep Low / Keep High recombine.  Sum the kept band
    // back so the saturated band joins it for full-range output.
    if (routeSplit)
    {
        const auto& kept = (hmode == HarmonicsMode::KeepLow) ? mSplitLo : mSplitHi;
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* dst = buffer.getWritePointer (ch);
            const float* src = kept.getReadPointer (ch);
            for (int n = 0; n < numSamples; ++n)
                dst[n] += src[n];
        }
    }
}

// -- Serialisation ------------------------------------------------------------

void SaturationDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("SaturationDSP");
    state.setProperty ("flowers",     mFlowers,     nullptr);
    state.setProperty ("dabs",        mDabs,        nullptr);
    state.setProperty ("sensitivity", mSensitivity, nullptr);
    state.setProperty ("bassRelief",  mBassRelief,  nullptr);
    state.setProperty ("transformer", mTransformer, nullptr);
    state.setProperty ("tubeType",    mTubeType,    nullptr);
    state.setProperty ("tonePre",     mTonePre,     nullptr);
    state.setProperty ("tonePost",    mTonePost,    nullptr);
    state.setProperty ("wet",         mWet,         nullptr);
    state.setProperty ("out",         mOut,         nullptr);
    state.setProperty ("autoGain",    mAutoGain,    nullptr);   // 9b
    state.setProperty ("osLog2",      mOsLog2,      nullptr);   // C2
    // H-7 (2026-05-01): Type umbrella + Vocal Body shaping toggle.  Default 0
    // (Tube) + false preserve old presets bit-exact on load.
    state.setProperty ("satType",     (int) mSatType, nullptr);
    state.setProperty ("vocalBody",   mVocalBody,   nullptr);
    state.setProperty ("harmonicsMode", (int) mHarmonicsMode, nullptr);
    state.setProperty ("consoleMode",   (int) mConsoleMode,   nullptr);
    state.setProperty ("consoleColor",  mConsoleColor,        nullptr);

    // H-10 (2026-05-02): Tape sub-engine state -- prefixed `tape_*` so old
    // presets (no tape attrs) load with TapeDSP defaults preserved bit-exact.
    state.setProperty ("tape_vibe",         mTapeVibe,         nullptr);
    state.setProperty ("tape_tapeSpeed",    mTapeSpeed,        nullptr);
    state.setProperty ("tape_hystAmount",   mTapeHystAmount,   nullptr);
    state.setProperty ("tape_wowRate",      mTapeWowRate,      nullptr);
    state.setProperty ("tape_wowDepth",     mTapeWowDepth,     nullptr);
    state.setProperty ("tape_flutterRate",  mTapeFlutterRate,  nullptr);
    state.setProperty ("tape_flutterDepth", mTapeFlutterDepth, nullptr);
    state.setProperty ("tape_inputGain",    mTapeInputGain,    nullptr);
    state.setProperty ("tape_outputGain",   mTapeOutputGain,   nullptr);
    state.setProperty ("tape_hiss",         mTapeHiss,         nullptr);
    state.setProperty ("tape_bias",         mTapeBias,         nullptr);
    state.setProperty ("tape_preShelfDb",   mTapePreShelfDb,   nullptr);
    state.setProperty ("tape_deShelfDb",    mTapeDeShelfDb,    nullptr);
    state.setProperty ("tape_lpHz",         mTapeLpHz,         nullptr);
    state.setProperty ("tape_irOn",         mTapeIrOn,         nullptr);
    state.setProperty ("tape_cassetteIdx",  mTapeCassetteIdx,  nullptr);

    auto xml = state.createXml();
    if (xml) juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void SaturationDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (!xml) return;

    // H-10 cutover (2026-05-02): old projects + presets saved before the
    // Tape fold have XML root tag "TapeDSP".  Translate them: force
    // mSatType = Tape, copy each TapeDSP attribute into its mTape* field,
    // then leave Tube/Console params at their defaults.  Saturation-style
    // params (flowers/dabs/etc.) are unset by old TapeDSP saves -- they
    // stay at construction defaults, which matters only if the user later
    // switches Mode to Tube/Console.  For the active Tape body, every
    // tape_* field is explicitly migrated, so the loaded sound is bit-exact.
    if (xml->getTagName() == "TapeDSP")
    {
        mSatType          = Type::Tape;
        mTapeVibe         = (float) xml->getDoubleAttribute ("vibe",         mTapeVibe);
        mTapeSpeed        = (float) xml->getDoubleAttribute ("tapeSpeed",    mTapeSpeed);
        mTapeHystAmount   = (float) xml->getDoubleAttribute ("hystAmount",   mTapeHystAmount);
        mTapeWowRate      = (float) xml->getDoubleAttribute ("wowRate",      mTapeWowRate);
        mTapeWowDepth     = (float) xml->getDoubleAttribute ("wowDepth",     mTapeWowDepth);
        mTapeFlutterRate  = (float) xml->getDoubleAttribute ("flutterRate",  mTapeFlutterRate);
        mTapeFlutterDepth = (float) xml->getDoubleAttribute ("flutterDepth", mTapeFlutterDepth);
        mTapeInputGain    = (float) xml->getDoubleAttribute ("inputGain",    mTapeInputGain);
        mTapeOutputGain   = (float) xml->getDoubleAttribute ("outputGain",   mTapeOutputGain);
        mTapeHiss         = (float) xml->getDoubleAttribute ("hiss",         mTapeHiss);
        mTapeBias         = (float) xml->getDoubleAttribute ("bias",         mTapeBias);
        mTapePreShelfDb   = (float) xml->getDoubleAttribute ("preShelfDb",   mTapePreShelfDb);
        mTapeDeShelfDb    = (float) xml->getDoubleAttribute ("deShelfDb",    mTapeDeShelfDb);
        mTapeIrOn         = false;   // legacy TapeDSP presets predate the cassette IR -> off
        const int loadedOs = xml->getIntAttribute ("osLog2", mOsLog2);
        const bool osChanged = (loadedOs != mOsLog2);
        mOsLog2 = juce::jlimit (1, 4, loadedOs);

        snapSmoothedToTargets();
        tapeSnapSmoothedToTargets();
        if (mSampleRate > 0.0)
        {
            if (osChanged) prepare (mSampleRate, mMaxBlock);
            else { updateFilters(); tapeUpdateFilters(); loadCassetteIR (mTapeCassetteIdx); }
            reset();
        }
        return;
    }

    if (xml->getTagName() != "SaturationDSP") return;

    mFlowers     = (float) xml->getDoubleAttribute ("flowers",     mFlowers);
    mDabs        = (float) xml->getDoubleAttribute ("dabs",        mDabs);
    mSensitivity = (float) xml->getDoubleAttribute ("sensitivity", mSensitivity);
    mBassRelief  = (float) xml->getDoubleAttribute ("bassRelief",  mBassRelief);
    mTransformer =         xml->getBoolAttribute   ("transformer", mTransformer);
    mTubeType    = juce::jlimit (0, 2,
                                 xml->getIntAttribute ("tubeType",    mTubeType));
    mTonePre     = (float) xml->getDoubleAttribute ("tonePre",     mTonePre);
    mTonePost    = (float) xml->getDoubleAttribute ("tonePost",    mTonePost);
    mWet         = (float) xml->getDoubleAttribute ("wet",         mWet);
    mOut         = (float) xml->getDoubleAttribute ("out",         mOut);
    mAutoGain    =         xml->getBoolAttribute   ("autoGain",    mAutoGain);
    // H-7 + H-10: Type umbrella + Vocal Body.  Defaults preserve old preset
    // audio.  Clamp 0..2 for H-10 (Tape).
    mSatType     = static_cast<Type> (juce::jlimit (0, 2,
                                       xml->getIntAttribute ("satType", 0)));
    mVocalBody   =         xml->getBoolAttribute   ("vocalBody",   false);
    // Default 1 = Normal preserves audio for old presets.
    mHarmonicsMode = static_cast<HarmonicsMode> (juce::jlimit (0, 2,
                       xml->getIntAttribute ("harmonicsMode", 1)));
    // Console voicing + Color enable.  Defaults: Clean + Color ON.  Old Console
    // presets (pre-dead-Color-fix) had Color gated dead behind mTransformer, so
    // defaulting ON makes their saved Color amount audible -> re-save.
    mConsoleMode  = static_cast<ConsoleMode> (juce::jlimit (0, 1,
                       xml->getIntAttribute ("consoleMode", 0)));
    mConsoleColor = xml->getBoolAttribute ("consoleColor", true);
    const int loadedOs = xml->getIntAttribute ("osLog2", mOsLog2);
    const bool osChanged = (loadedOs != mOsLog2);
    mOsLog2 = juce::jlimit (1, 4, loadedOs);

    // H-10 (2026-05-02): Tape sub-engine state.  Missing attributes fall
    // back to current member values (which match TapeDSP construction
    // defaults bit-exact), so old non-Tape presets stay audio-identical.
    mTapeVibe         = (float) xml->getDoubleAttribute ("tape_vibe",         mTapeVibe);
    mTapeSpeed        = (float) xml->getDoubleAttribute ("tape_tapeSpeed",    mTapeSpeed);
    mTapeHystAmount   = (float) xml->getDoubleAttribute ("tape_hystAmount",   mTapeHystAmount);
    mTapeWowRate      = (float) xml->getDoubleAttribute ("tape_wowRate",      mTapeWowRate);
    mTapeWowDepth     = (float) xml->getDoubleAttribute ("tape_wowDepth",     mTapeWowDepth);
    mTapeFlutterRate  = (float) xml->getDoubleAttribute ("tape_flutterRate",  mTapeFlutterRate);
    mTapeFlutterDepth = (float) xml->getDoubleAttribute ("tape_flutterDepth", mTapeFlutterDepth);
    mTapeInputGain    = (float) xml->getDoubleAttribute ("tape_inputGain",    mTapeInputGain);
    mTapeOutputGain   = (float) xml->getDoubleAttribute ("tape_outputGain",   mTapeOutputGain);
    mTapeHiss         = (float) xml->getDoubleAttribute ("tape_hiss",         mTapeHiss);
    mTapeBias         = (float) xml->getDoubleAttribute ("tape_bias",         mTapeBias);
    mTapePreShelfDb   = (float) xml->getDoubleAttribute ("tape_preShelfDb",   mTapePreShelfDb);
    mTapeDeShelfDb    = (float) xml->getDoubleAttribute ("tape_deShelfDb",    mTapeDeShelfDb);
    mTapeLpHz         = (float) xml->getDoubleAttribute ("tape_lpHz",         mTapeLpHz);
    // Default false on load so old Tape presets (pre-cassette-IR) preserve
    // their sound; matches the construction default (IR off, SaturationDSP.h).
    mTapeIrOn         =         xml->getBoolAttribute    ("tape_irOn",        false);
    mTapeCassetteIdx  = juce::jlimit (0, kNumCassettes - 1,
                                      xml->getIntAttribute ("tape_cassetteIdx", mTapeCassetteIdx));

    // Snap smoothers to restored values + clear filter state (P4-style safety).
    snapSmoothedToTargets();
    tapeSnapSmoothedToTargets();
    if (mSampleRate > 0.0)
    {
        if (osChanged)
            prepare (mSampleRate, mMaxBlock);   // reallocate oversampler at restored factor
        else
        {
            updateFilters();
            tapeUpdateFilters();
            loadCassetteIR (mTapeCassetteIdx);
        }
        reset();
    }
}

// =============================================================================
// H-10 (2026-05-02): Tape sub-engine implementation.
//
// Bit-exact port of the legacy TapeDSP body.  Lives entirely within
// SaturationDSP so picking SaturationType::Tape from the Mode dropdown
// runs this code path instead of the Tube/Console body above.  Reuses
// the existing mOversampler (same 2-channel polyphase IIR, same OS
// factor) but maintains its own scratch / wow / hysteresis / pink-hiss
// state in mTape* members so the two engines never share runtime data.
// =============================================================================

namespace
{
    constexpr float kTapeSmoothMsFast   = 15.0f;
    constexpr float kTapeSmoothMsMed    = 20.0f;
    constexpr float kTapeSmoothMsShelf  = 20.0f;
    constexpr float kTapeSmoothMsVibe   = 30.0f;

    // Hysteresis alpha (per-sample integrator coef for `y += alpha * (shaped - y)`).
    // Spec at base rate: alpha_base = 0.2 + 0.8 * tapeSpeed.  HystAmount scales the
    // memory: 0 = bypass (alpha=1), 1 = spec, 2 = strong.  Result is divided by
    // the OS factor so cutoff stays constant regardless of OS factor (10c spec).
    inline float computeTapeHystAlpha (float tapeSpeed, float hystAmount, int osFactor) noexcept
    {
        if (hystAmount <= 0.001f) return 1.0f;
        const float alphaBase   = 0.2f + 0.8f * juce::jlimit (0.0f, 1.0f, tapeSpeed);
        const float alphaScaled = alphaBase / juce::jmax (0.01f, hystAmount);
        return juce::jlimit (0.001f, 1.0f,
                             alphaScaled / (float) juce::jmax (1, osFactor));
    }
}

// -- cubic circular helper (TapeDSP 10e -- bit-exact port) -------------------

float SaturationDSP::tapeCubicCircular (const std::vector<float>& buf, int writePos,
                                         int iOff, float frac, int size) noexcept
{
    const int baseIdx = (writePos - iOff + size) % size;
    const int idxYm1  = (baseIdx + 1) % size;
    const int idxY0   = baseIdx;
    const int idxY1   = (baseIdx - 1 + size) % size;
    const int idxY2   = (baseIdx - 2 + size) % size;
    const float ym1 = buf[(size_t) idxYm1];
    const float y0  = buf[(size_t) idxY0];
    const float y1  = buf[(size_t) idxY1];
    const float y2  = buf[(size_t) idxY2];

    const float c0 = y0;
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

// -- Tape internal helpers ---------------------------------------------------

void SaturationDSP::tapeUpdateFilters()
{
    if (mSampleRate <= 0.0) return;
    const float sr    = (float) mSampleRate;
    const float twoPi = juce::MathConstants<float>::twoPi;

    mTapePreShelfCoef    = std::exp (-twoPi * 5000.0f / sr);
    mTapeDeShelfCoef     = std::exp (-twoPi * 4000.0f / sr);
    mTapePreShelfGainLin = juce::Decibels::decibelsToGain (mTapePreShelfDb);
    mTapeDeShelfGainLin  = juce::Decibels::decibelsToGain (mTapeDeShelfDb);
    mTapeHissHpCoef      = std::exp (-twoPi * 200.0f / sr);
    mTapeDcCoef          = 1.0f - twoPi * 5.0f / sr;
    // coef 0 at the top of the range -> lp = x (truly transparent, no rolloff).
    mTapeLpCoef          = (mTapeLpHz >= 22000.0f) ? 0.0f
                         : std::exp (-twoPi * juce::jlimit (5000.0f, 22000.0f, mTapeLpHz) / sr);

    const int osFactor = 1 << juce::jlimit (1, 4, mOsLog2);
    mTapeHystAlphaOS = computeTapeHystAlpha (mTapeSpeed, mTapeHystAmount, osFactor);
}

void SaturationDSP::tapeAllocateScratch()
{
    if (mMaxBlock <= 0) return;
    mTapeBandBuf.setSize (2, mMaxBlock, false, true, true);
    mTapeBandBuf.clear();

    const int bufSize = (int) std::ceil (kTapeMaxWowMs * 0.001 * mSampleRate) + 16;
    mTapeWowBufL.assign ((size_t) bufSize, 0.0f);
    mTapeWowBufR.assign ((size_t) bufSize, 0.0f);
    mTapeWowWritePos = 0;

    const size_t n = (size_t) mMaxBlock;
    mTapeVibeScr         .assign (n, mTapeVibe);
    mTapeHystAlphaOSScr  .assign (n, mTapeHystAlphaOS);
    mTapeBiasScr         .assign (n, mTapeBias);
    mTapePreGainScr      .assign (n, mTapePreShelfGainLin);
    mTapeDeGainScr       .assign (n, mTapeDeShelfGainLin);
    mTapeInGainScr       .assign (n, mTapeInputGain);
    mTapeOutGainScr      .assign (n, mTapeOutputGain);
    mTapeWowDepthScr     .assign (n, mTapeWowDepth);
    mTapeFlutterDepthScr .assign (n, mTapeFlutterDepth);
    mTapeHissScr         .assign (n, mTapeHiss);
}

void SaturationDSP::tapeSnapSmoothedToTargets()
{
    mTapeVibeSmooth         .setCurrentAndTargetValue (mTapeVibe);
    mTapeHystAlphaOSSmooth  .setCurrentAndTargetValue (mTapeHystAlphaOS);
    mTapeBiasSmooth         .setCurrentAndTargetValue (mTapeBias);
    mTapePreGainSmooth      .setCurrentAndTargetValue (mTapePreShelfGainLin);
    mTapeDeGainSmooth       .setCurrentAndTargetValue (mTapeDeShelfGainLin);
    mTapeInGainSmooth       .setCurrentAndTargetValue (mTapeInputGain);
    mTapeOutGainSmooth      .setCurrentAndTargetValue (mTapeOutputGain);
    mTapeWowDepthSmooth     .setCurrentAndTargetValue (mTapeWowDepth);
    mTapeFlutterDepthSmooth .setCurrentAndTargetValue (mTapeFlutterDepth);
    mTapeHissSmooth         .setCurrentAndTargetValue (mTapeHiss);
}

void SaturationDSP::tapePrepare (double sampleRate, int maxBlockSize)
{
    // Saturation::prepare already sets mSampleRate + mMaxBlock + builds
    // mOversampler.  Tape only needs to compute its own filter coefs +
    // allocate its scratch + reset its smoothers.
    tapeUpdateFilters();
    tapeAllocateScratch();

    mTapeVibeSmooth         .reset (sampleRate, kTapeSmoothMsVibe  * 0.001);
    mTapeHystAlphaOSSmooth  .reset (sampleRate, kTapeSmoothMsVibe  * 0.001);
    mTapeBiasSmooth         .reset (sampleRate, kTapeSmoothMsMed   * 0.001);
    mTapePreGainSmooth      .reset (sampleRate, kTapeSmoothMsShelf * 0.001);
    mTapeDeGainSmooth       .reset (sampleRate, kTapeSmoothMsShelf * 0.001);
    mTapeInGainSmooth       .reset (sampleRate, kTapeSmoothMsFast  * 0.001);
    mTapeOutGainSmooth      .reset (sampleRate, kTapeSmoothMsFast  * 0.001);
    mTapeWowDepthSmooth     .reset (sampleRate, kTapeSmoothMsMed   * 0.001);
    mTapeFlutterDepthSmooth .reset (sampleRate, kTapeSmoothMsMed   * 0.001);
    mTapeHissSmooth         .reset (sampleRate, kTapeSmoothMsMed   * 0.001);
    tapeSnapSmoothedToTargets();

    // Cassette IR convolution + hiss beds.  Conv is re-prepared + re-loaded each
    // prepare (loadImpulseResponse resamples the IR to the host SR); the hiss
    // beds load once (noise -> no resample needed).  Both run on the prepare
    // (message) thread -- never concurrent with processBlock.
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlockSize), 2 };
    mTapeConv.reset();
    mTapeConv.prepare (spec);
    if (mTapeHissSamples.empty()) loadAllCassetteHiss();
    loadCassetteIR (mTapeCassetteIdx);
}

void SaturationDSP::tapeReset()
{
    std::fill (mTapeWowBufL.begin(), mTapeWowBufL.end(), 0.0f);
    std::fill (mTapeWowBufR.begin(), mTapeWowBufR.end(), 0.0f);
    mTapeWowWritePos       = 0;
    mTapeWowPhase          = 0.0;
    mTapeFlutterPhase      = 0.0;
    mTapeFlutterNoiseState = 0.0f;

    mTapeHystL = mTapeHystR = 0.0f;
    mTapePreShelfL = mTapePreShelfR = 0.0f;
    mTapeDeShelfL  = mTapeDeShelfR  = 0.0f;

    mTapePinkL = TapePinkState{};
    mTapePinkR = TapePinkState{};
    mTapeHissHPx_L = mTapeHissHPy_L = mTapeHissHPx_R = mTapeHissHPy_R = 0.0f;
    mTapeDcX_L = mTapeDcY_L = mTapeDcX_R = mTapeDcY_R = 0.0f;
    mTapeLpL = mTapeLpR = 0.0f;
    mTapeConv.reset();
    mTapeHissReadPos = 0;

    mTapeBandBuf.clear();
}

// -- Tape setters (CPU-guarded -- match TapeDSP's behavior bit-exact) --------

void SaturationDSP::setTapeVibe (float v)
{
    const float n = juce::jlimit (0.0f, 1.0f, v);
    if (n != mTapeVibe) { mTapeVibe = n; mTapeVibeSmooth.setTargetValue (n); }
}

void SaturationDSP::setTapeSpeed (float v)
{
    const float n = juce::jlimit (0.0f, 1.0f, v);
    if (n != mTapeSpeed)
    {
        mTapeSpeed = n;
        const int osFactor = 1 << juce::jlimit (1, 4, mOsLog2);
        mTapeHystAlphaOS = computeTapeHystAlpha (mTapeSpeed, mTapeHystAmount, osFactor);
        mTapeHystAlphaOSSmooth.setTargetValue (mTapeHystAlphaOS);
    }
}

void SaturationDSP::setTapeHystAmount (float v)
{
    const float n = juce::jlimit (0.0f, 2.0f, v);
    if (n != mTapeHystAmount)
    {
        mTapeHystAmount = n;
        const int osFactor = 1 << juce::jlimit (1, 4, mOsLog2);
        mTapeHystAlphaOS = computeTapeHystAlpha (mTapeSpeed, mTapeHystAmount, osFactor);
        mTapeHystAlphaOSSmooth.setTargetValue (mTapeHystAlphaOS);
    }
}

void SaturationDSP::setTapeWowRate (float hz)
{
    const float n = juce::jlimit (0.1f, 5.0f, hz);
    if (n != mTapeWowRate) mTapeWowRate = n;
}

void SaturationDSP::setTapeWowDepth (float d)
{
    const float n = juce::jlimit (0.0f, 1.0f, d);
    if (n != mTapeWowDepth) { mTapeWowDepth = n; mTapeWowDepthSmooth.setTargetValue (n); }
}

void SaturationDSP::setTapeFlutterRate (float hz)
{
    const float n = juce::jlimit (1.0f, 15.0f, hz);
    if (n != mTapeFlutterRate) mTapeFlutterRate = n;
}

void SaturationDSP::setTapeFlutterDepth (float d)
{
    const float n = juce::jlimit (0.0f, 1.0f, d);
    if (n != mTapeFlutterDepth) { mTapeFlutterDepth = n; mTapeFlutterDepthSmooth.setTargetValue (n); }
}

void SaturationDSP::setTapeInputGain (float linGain)
{
    const float n = juce::jmax (0.0f, linGain);
    if (n != mTapeInputGain) { mTapeInputGain = n; mTapeInGainSmooth.setTargetValue (n); }
}

void SaturationDSP::setTapeOutputGain (float linGain)
{
    const float n = juce::jmax (0.0f, linGain);
    if (n != mTapeOutputGain) { mTapeOutputGain = n; mTapeOutGainSmooth.setTargetValue (n); }
}

void SaturationDSP::setTapeHiss (float h)
{
    const float n = juce::jlimit (0.0f, 1.0f, h);
    if (n != mTapeHiss) { mTapeHiss = n; mTapeHissSmooth.setTargetValue (n); }
}

void SaturationDSP::setTapeBias (float v)
{
    const float n = juce::jlimit (0.0f, 10.0f, v);
    if (n != mTapeBias) { mTapeBias = n; mTapeBiasSmooth.setTargetValue (n); }
}

void SaturationDSP::setTapePreShelfDb (float dB)
{
    const float n = juce::jlimit (-12.0f, 12.0f, dB);
    if (n != mTapePreShelfDb)
    {
        mTapePreShelfDb = n;
        mTapePreShelfGainLin = juce::Decibels::decibelsToGain (n);
        mTapePreGainSmooth.setTargetValue (mTapePreShelfGainLin);
    }
}

void SaturationDSP::setTapeDeShelfDb (float dB)
{
    const float n = juce::jlimit (-12.0f, 12.0f, dB);
    if (n != mTapeDeShelfDb)
    {
        mTapeDeShelfDb = n;
        mTapeDeShelfGainLin = juce::Decibels::decibelsToGain (n);
        mTapeDeGainSmooth.setTargetValue (mTapeDeShelfGainLin);
    }
}
void SaturationDSP::setTapeLpHz (float hz)
{
    const float n = juce::jlimit (5000.0f, 22000.0f, hz);
    if (n != mTapeLpHz)
    {
        mTapeLpHz = n;
        if (mSampleRate > 0.0)
            mTapeLpCoef = (n >= 22000.0f) ? 0.0f
                        : std::exp (-juce::MathConstants<float>::twoPi * n / (float) mSampleRate);
    }
}
void SaturationDSP::setTapeIrOn (bool on)
{
    if (on != mTapeIrOn) mTapeIrOn = on;
}
void SaturationDSP::setTapeCassette (int idx)
{
    const int i = juce::jlimit (0, kNumCassettes - 1, idx);
    if (i != mTapeCassetteIdx)
    {
        mTapeCassetteIdx = i;
        if (mSampleRate > 0.0) loadCassetteIR (i);   // hiss switches via the audio-thread index read
    }
}

juce::File SaturationDSP::cassetteResourceDir()
{
    // Runtime override dir staged next to the exe by the build (mirrors the
    // AcousticPreampStyleDSP IR-resolution convention).
    return juce::File::getSpecialLocation (juce::File::currentExecutableFile)
              .getParentDirectory().getChildFile ("Resources").getChildFile ("Tape");
}

void SaturationDSP::loadCassetteIR (int idx)
{
    const int i = juce::jlimit (0, kNumCassettes - 1, idx);
    const auto f = cassetteResourceDir().getChildFile ("IRs")
                       .getChildFile ("cassette tape_" + juce::String (i + 1) + ".wav");
    if (f.existsAsFile())
    {
        mTapeConv.loadImpulseResponse (f,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::yes, 0,
            juce::dsp::Convolution::Normalise::yes);
    }
    else
    {
        // Identity (unit-impulse) IR -> the convolution passes through unchanged.
        juce::AudioBuffer<float> identity (1, 1);
        identity.setSample (0, 0, 1.0f);
        mTapeConv.loadImpulseResponse (std::move (identity),
            mSampleRate > 0.0 ? mSampleRate : 44100.0,
            juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::no,
            juce::dsp::Convolution::Normalise::no);
    }
}

void SaturationDSP::loadAllCassetteHiss()
{
    mTapeHissSamples.assign (kNumCassettes, juce::AudioBuffer<float>());
    mTapeHissLoaded = false;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    const auto dir = cassetteResourceDir().getChildFile ("Samples");
    for (int i = 0; i < kNumCassettes; ++i)
    {
        const auto f = dir.getChildFile ("cassette tape_" + juce::String (i + 1) + "_noise.wav");
        if (! f.existsAsFile()) continue;
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
        if (reader == nullptr || reader->lengthInSamples <= 0) continue;
        const int ch  = (int) juce::jmin ((juce::uint32) 2, reader->numChannels);
        const int len = (int) juce::jmin ((juce::int64) (1 << 23), reader->lengthInSamples);
        mTapeHissSamples[(size_t) i].setSize (juce::jmax (1, ch), len);
        reader->read (&mTapeHissSamples[(size_t) i], 0, len, 0, true, ch > 1);
        mTapeHissLoaded = true;
    }
}

// -- Tape process body (bit-exact port of TapeDSP::process) ------------------

void SaturationDSP::processTape (juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0) return;
    if (numSamples > mMaxBlock) return;
    if (mOversampler == nullptr) return;
    if (mTapeWowBufL.empty()) return;
    if (mTapeBandBuf.getNumSamples() < numSamples) return;

    const int numCh   = std::min (buffer.getNumChannels(), 2);
    const int numChOS = 2;
    const int kOsNow  = 1 << juce::jlimit (1, 4, mOsLog2);
    const int wowSize = (int) mTapeWowBufL.size();

    const float sr        = (float) mSampleRate;
    const float a_preC    = mTapePreShelfCoef;
    const float a_deC     = mTapeDeShelfCoef;
    const float a_hissHp  = mTapeHissHpCoef;
    const float a_dc      = mTapeDcCoef;
    const float a_lp      = mTapeLpCoef;

    // Sampled hiss: the active cassette bed (clamped index), or synthetic pink if
    // no bed is loaded.  Read pointers are bounded each sample in the loop below.
    const int  hissIdx        = juce::jlimit (0, kNumCassettes - 1, mTapeCassetteIdx);
    const bool useSampledHiss = mTapeHissLoaded
        && hissIdx < (int) mTapeHissSamples.size()
        && mTapeHissSamples[(size_t) hissIdx].getNumSamples() > 0;
    const int    hissLen = useSampledHiss ? mTapeHissSamples[(size_t) hissIdx].getNumSamples() : 0;
    const float* hissCh0 = useSampledHiss ? mTapeHissSamples[(size_t) hissIdx].getReadPointer (0) : nullptr;
    const float* hissCh1 = (useSampledHiss && mTapeHissSamples[(size_t) hissIdx].getNumChannels() > 1)
                           ? mTapeHissSamples[(size_t) hissIdx].getReadPointer (1) : hissCh0;

    // Phase 0: consume smoothers into scratch arrays.
    for (int n = 0; n < numSamples; ++n)
    {
        mTapeVibeScr[(size_t) n]         = mTapeVibeSmooth        .getNextValue();
        mTapeHystAlphaOSScr[(size_t) n]  = mTapeHystAlphaOSSmooth .getNextValue();
        mTapeBiasScr[(size_t) n]         = mTapeBiasSmooth        .getNextValue();
        mTapePreGainScr[(size_t) n]      = mTapePreGainSmooth     .getNextValue();
        mTapeDeGainScr[(size_t) n]       = mTapeDeGainSmooth      .getNextValue();
        mTapeInGainScr[(size_t) n]       = mTapeInGainSmooth      .getNextValue();
        mTapeOutGainScr[(size_t) n]      = mTapeOutGainSmooth     .getNextValue();
        mTapeWowDepthScr[(size_t) n]     = mTapeWowDepthSmooth    .getNextValue();
        mTapeFlutterDepthScr[(size_t) n] = mTapeFlutterDepthSmooth.getNextValue();
        mTapeHissScr[(size_t) n]         = mTapeHissSmooth        .getNextValue();
    }

    // Phase 1: input gain + pre-emphasis (base rate).
    for (int ch = 0; ch < numChOS; ++ch)
    {
        float* dst = mTapeBandBuf.getWritePointer (ch);
        if (ch < numCh)
        {
            const float* src = buffer.getReadPointer (ch);
            float& preLP = (ch == 0) ? mTapePreShelfL : mTapePreShelfR;
            for (int n = 0; n < numSamples; ++n)
            {
                float x = src[n] * mTapeInGainScr[(size_t) n];
                preLP = (1.0f - a_preC) * x + a_preC * preLP;
                const float gPre = mTapePreGainScr[(size_t) n];
                x = gPre * x + (1.0f - gPre) * preLP;
                dst[n] = x;
            }
        }
        else
        {
            // Mono: pad ch1 from ch0's processed stream.
            const float* src0 = mTapeBandBuf.getReadPointer (0);
            for (int n = 0; n < numSamples; ++n)
                dst[n] = src0[n];
        }
    }

    // Phase 2: oversampled shaper + hysteresis.
    juce::dsp::AudioBlock<float> bandBlock (mTapeBandBuf.getArrayOfWritePointers(),
                                             (size_t) numChOS, 0, (size_t) numSamples);
    auto upBlock = mOversampler->processSamplesUp (bandBlock);
    const int upN = (int) upBlock.getNumSamples();
    jassert (upN == numSamples * kOsNow);

    for (int ch = 0; ch < numChOS; ++ch)
    {
        float* up = upBlock.getChannelPointer ((size_t) ch);
        float& hyst = (ch == 0) ? mTapeHystL : mTapeHystR;
        for (int osN = 0; osN < upN; ++osN)
        {
            const int   baseIdx  = osN / kOsNow;
            const float vibeNow  = mTapeVibeScr[(size_t) baseIdx];
            const float biasNow  = mTapeBiasScr[(size_t) baseIdx];
            const float alphaNow = mTapeHystAlphaOSScr[(size_t) baseIdx];

            const float x        = up[osN];
            const float biasedX  = x + (biasNow - 5.0f) * 0.02f;
            const float k        = 0.3f * vibeNow;
            const float shaped   = tapeAsymShaper (biasedX, k);
            hyst += alphaNow * (shaped - hyst);
            up[osN] = hyst;
        }
    }
    mOversampler->processSamplesDown (bandBlock);

    // Phase 3: de-emphasis + wow/flutter + hiss + DC + output.
    float* Lout = numCh > 0 ? buffer.getWritePointer (0) : nullptr;
    float* Rout = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    const float* processedL = mTapeBandBuf.getReadPointer (0);
    const float* processedR = mTapeBandBuf.getReadPointer (1);

    const float maxWowSamples = (float) (kTapeMaxWowMs * 0.5 * 0.001 * mSampleRate);
    const double centreDelay  = kTapeMaxWowMs * 0.5 * 0.001 * mSampleRate;

    for (int n = 0; n < numSamples; ++n)
    {
        float sampL = processedL[n];
        float sampR = (numCh > 1) ? processedR[n] : sampL;

        // De-emphasis (base-rate shelf).
        const float gDe = mTapeDeGainScr[(size_t) n];
        mTapeDeShelfL = (1.0f - a_deC) * sampL + a_deC * mTapeDeShelfL;
        sampL         = gDe * sampL + (1.0f - gDe) * mTapeDeShelfL;
        mTapeDeShelfR = (1.0f - a_deC) * sampR + a_deC * mTapeDeShelfR;
        sampR         = gDe * sampR + (1.0f - gDe) * mTapeDeShelfR;

        // Cassette low-pass (HF rolloff) on the program signal.
        mTapeLpL = (1.0f - a_lp) * sampL + a_lp * mTapeLpL;  sampL = mTapeLpL;
        mTapeLpR = (1.0f - a_lp) * sampR + a_lp * mTapeLpR;  sampR = mTapeLpR;

        // Write into wow/flutter delay buffer.
        mTapeWowBufL[(size_t) mTapeWowWritePos] = sampL;
        mTapeWowBufR[(size_t) mTapeWowWritePos] = sampR;

        // LFO update.
        const float wowLfo     = (float) std::sin (juce::MathConstants<double>::twoPi * mTapeWowPhase);
        const float flutterSin = (float) std::sin (juce::MathConstants<double>::twoPi * mTapeFlutterPhase);
        const float rawNoise   = mTapeRandomL.nextFloat() * 2.0f - 1.0f;
        mTapeFlutterNoiseState = 0.999f * mTapeFlutterNoiseState + 0.001f * rawNoise;
        const float flutter    = 0.9f * flutterSin + 0.1f * mTapeFlutterNoiseState;

        const float wowDepthNow     = mTapeWowDepthScr[(size_t) n];
        const float flutterDepthNow = mTapeFlutterDepthScr[(size_t) n];
        const float totalModSmp =
            maxWowSamples * (wowDepthNow * wowLfo + flutterDepthNow * flutter * 0.3f);

        const float readOffsetF = juce::jlimit (0.0f, (float) (wowSize - 4),
                                                (float) centreDelay + totalModSmp);
        const int   iOff = (int) readOffsetF;
        const float frac = readOffsetF - (float) iOff;

        // Cubic-interp delay reads.
        float wowL = tapeCubicCircular (mTapeWowBufL, mTapeWowWritePos, iOff, frac, wowSize);
        float wowR = tapeCubicCircular (mTapeWowBufR, mTapeWowWritePos, iOff, frac, wowSize);

        // Pink-filtered hiss + 200 Hz HPF (independent L/R RNG streams).
        const float hissNow = mTapeHissScr[(size_t) n];
        if (useSampledHiss)
        {
            // Bounded looping read: clamp before (the cassette may have just
            // switched to a shorter bed), wrap after advancing.
            if (mTapeHissReadPos >= hissLen) mTapeHissReadPos = 0;
            wowL += hissNow * hissCh0[mTapeHissReadPos];
            wowR += hissNow * hissCh1[mTapeHissReadPos];
            if (++mTapeHissReadPos >= hissLen) mTapeHissReadPos = 0;
        }
        else
        {
            // Synthetic pink + 200 Hz HPF fallback (no real hiss bed loaded).
            const float whiteL = mTapeRandomL.nextFloat() * 2.0f - 1.0f;
            const float whiteR = mTapeRandomR.nextFloat() * 2.0f - 1.0f;
            const float pinkL  = mTapePinkL.process (whiteL);
            const float pinkR  = mTapePinkR.process (whiteR);
            const float hpL    = a_hissHp * (mTapeHissHPy_L + pinkL - mTapeHissHPx_L);
            const float hpR    = a_hissHp * (mTapeHissHPy_R + pinkR - mTapeHissHPx_R);
            mTapeHissHPx_L = pinkL; mTapeHissHPy_L = hpL;
            mTapeHissHPx_R = pinkR; mTapeHissHPy_R = hpR;
            wowL += hissNow * hpL * 0.1f;
            wowR += hissNow * hpR * 0.1f;
        }

        // DC blocker (5 Hz SR-aware).
        const float dcInL = wowL;
        wowL = dcInL - mTapeDcX_L + a_dc * mTapeDcY_L;
        mTapeDcX_L = dcInL; mTapeDcY_L = wowL;
        const float dcInR = wowR;
        wowR = dcInR - mTapeDcX_R + a_dc * mTapeDcY_R;
        mTapeDcX_R = dcInR; mTapeDcY_R = wowR;

        // Output gain.
        const float outLin = mTapeOutGainScr[(size_t) n];
        if (Lout) Lout[n] = wowL * outLin;
        if (Rout) Rout[n] = wowR * outLin;

        mTapeWowWritePos = (mTapeWowWritePos + 1) % wowSize;

        // LFO phase advance (positive deltas; while-wrap).
        mTapeWowPhase     += (double) mTapeWowRate     / (double) sr;
        while (mTapeWowPhase     >= 1.0) mTapeWowPhase     -= 1.0;
        mTapeFlutterPhase += (double) mTapeFlutterRate / (double) sr;
        while (mTapeFlutterPhase >= 1.0) mTapeFlutterPhase -= 1.0;
    }

    // Cassette IR: zero-latency convolution on the tape output when engaged.
    if (mTapeIrOn)
    {
        juce::dsp::AudioBlock<float> blk (buffer);
        auto sub = blk.getSubBlock (0, (size_t) numSamples).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        mTapeConv.process (ctx);
    }

    juce::ignoreUnused (sr);
}
