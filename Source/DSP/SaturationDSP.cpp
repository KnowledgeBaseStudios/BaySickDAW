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

    reset();
}

void SaturationDSP::reset()
{
    mPreLP_L = mPreLP_R = mPostLP_L = mPostLP_R = 0.0f;
    mBassLP_L = mBassLP_R = 0.0f;
    mDCx_L = mDCy_L = mDCx_R = mDCy_R = 0.0f;
    if (mOversampler) mOversampler->reset();
    mBandBuf.clear();
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

            const float low_proc  = processTube (low,  flowers, dabs, tubeTy, transf);
            const float high_proc = processTube (high, flowers, dabs, tubeTy, transf);
            const float low_out   = relief * low + (1.0f - relief) * low_proc;

            up[osN] = low_out + high_proc;
        }
    }

    mOversampler->processSamplesDown (bandBlock);   // mBandBuf now holds tube_out

    // ---- Phase 3: DC block + tone post + wet/dry + out gain (base rate) -----
    for (int ch = 0; ch < numCh; ++ch)
    {
        float*       samples = buffer.getWritePointer (ch);
        const float* tube    = mBandBuf.getReadPointer (ch);

        float& postLP = (ch == 0) ? mPostLP_L : mPostLP_R;
        float& dcX    = (ch == 0) ? mDCx_L    : mDCx_R;
        float& dcY    = (ch == 0) ? mDCy_L    : mDCy_R;

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

            // Tone post (base-rate shelf)
            postLP = (1.0f - a_shelf) * tube_out + a_shelf * postLP;
            tube_out = gPost * tube_out + (1.0f - gPost) * postLP;

            // Wet/dry + out gain (+ auto-gain compensation)
            samples[n] = (dry * dry_in + wet * tube_out) * finalOut;
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
    auto xml = state.createXml();
    if (xml) juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void SaturationDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (!xml || xml->getTagName() != "SaturationDSP") return;

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
    const int loadedOs = xml->getIntAttribute ("osLog2", mOsLog2);
    const bool osChanged = (loadedOs != mOsLog2);
    mOsLog2 = juce::jlimit (1, 4, loadedOs);

    // Snap smoothers to restored values + clear filter state (P4-style safety).
    snapSmoothedToTargets();
    if (mSampleRate > 0.0)
    {
        if (osChanged)
            prepare (mSampleRate, mMaxBlock);   // reallocate oversampler at restored factor
        else
            updateFilters();
        reset();
    }
}
