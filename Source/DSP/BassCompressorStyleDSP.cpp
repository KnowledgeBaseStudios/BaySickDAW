#include "BassCompressorStyleDSP.h"

namespace
{
    constexpr float kXover1Hz   = 200.0f;
    constexpr float kXover2Hz   = 2000.0f;
    constexpr float kAttackMs   = 10.0f;

    // Macro mapping: Comp 0..1 maps to threshold -6..-36 dB and ratio 2..8x
    // (multiplied with the user's Ratio knob to reach the final ratio).
    constexpr float kCompMinThreshDb = -6.0f;
    constexpr float kCompMaxThreshDb = -36.0f;
    constexpr float kCompMinRatioMul = 1.0f;
    constexpr float kCompMaxRatioMul = 2.5f;

    // Per-sample squared-domain compressor: envSq follows |x|^2 with attack/
    // release coefficients.  Returns the linear gain factor that should be
    // applied to the audio sample given the current envelope.
    inline float computeBandGain (float envSq, float threshSq, float ratio)
    {
        if (envSq <= threshSq || ratio <= 1.0f) return 1.0f;
        // env above threshold -> apply (1 - 1/ratio) of the over-threshold dB
        // back as gain reduction.
        const float envDb     = 0.5f * juce::Decibels::gainToDecibels (envSq, -120.0f);
        const float threshDb  = 0.5f * juce::Decibels::gainToDecibels (threshSq, -120.0f);
        const float overDb    = envDb - threshDb;
        const float reductDb  = -overDb * (1.0f - 1.0f / ratio);
        return juce::Decibels::decibelsToGain (reductDb, -60.0f);
    }
}

void BassCompressorStyleDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;

    juce::dsp::ProcessSpec spec { sampleRate,
                                   (juce::uint32) juce::jmax (1, maxBlockSize),
                                   2 };
    auto setup = [&] (juce::dsp::LinkwitzRileyFilter<float>& f,
                       juce::dsp::LinkwitzRileyFilterType type, float hz)
    {
        f.prepare (spec);
        f.setType (type);
        f.setCutoffFrequency (hz);
        f.reset();
    };
    setup (mXover1Lp, juce::dsp::LinkwitzRileyFilterType::lowpass,  kXover1Hz);
    setup (mXover1Hp, juce::dsp::LinkwitzRileyFilterType::highpass, kXover1Hz);
    setup (mXover2Lp, juce::dsp::LinkwitzRileyFilterType::lowpass,  kXover2Hz);
    setup (mXover2Hp, juce::dsp::LinkwitzRileyFilterType::highpass, kXover2Hz);

    const int n = juce::jmax (1, maxBlockSize);
    mLowBuf .setSize (2, n, false, true, true);
    mMidBuf .setSize (2, n, false, true, true);
    mHighBuf.setSize (2, n, false, true, true);

    recomputeCoefs();

    // GR meter decay matches CompressorDSP's pattern.
    constexpr float kGrDecayDbPerSec = 30.0f;
    mGrDecayDbPerBlock = kGrDecayDbPerSec * (float) maxBlockSize
                          / (float) (sampleRate > 0.0 ? sampleRate : 44100.0);

    reset();
}

void BassCompressorStyleDSP::reset()
{
    mXover1Lp.reset();  mXover1Hp.reset();
    mXover2Lp.reset();  mXover2Hp.reset();
    mLow = mMid = mHigh = {};
    mGainReductionDb.store (0.0f, std::memory_order_relaxed);
}

void BassCompressorStyleDSP::recomputeCoefs()
{
    if (mSampleRate <= 0.0) return;
    mAttackCoef  = (float) std::exp (-1.0 / (kAttackMs * 0.001 * mSampleRate));
    mReleaseCoef = (float) std::exp (-1.0 / (mReleaseMs * 0.001 * mSampleRate));
}

void BassCompressorStyleDSP::setComp (float v01)
{
    v01 = juce::jlimit (0.0f, 1.0f, v01);
    if (mComp != v01) mComp = v01;
}

void BassCompressorStyleDSP::setRatio (float r)
{
    r = juce::jlimit (1.0f, 10.0f, r);
    if (mRatio != r) mRatio = r;
}

void BassCompressorStyleDSP::setReleaseMs (float ms)
{
    ms = juce::jlimit (50.0f, 500.0f, ms);
    if (mReleaseMs != ms) { mReleaseMs = ms; recomputeCoefs(); }
}

void BassCompressorStyleDSP::setLevel (float dB)
{
    dB = juce::jlimit (-24.0f, 12.0f, dB);
    if (mLevel != dB) mLevel = dB;
}

void BassCompressorStyleDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    auto ensureSize = [n, numCh] (juce::AudioBuffer<float>& b)
    {
        if (b.getNumChannels() < numCh || b.getNumSamples() < n)
            b.setSize (numCh, n, false, false, true);
    };
    ensureSize (mLowBuf);
    ensureSize (mMidBuf);
    ensureSize (mHighBuf);

    // Split: copy input into Low and Mid, then HP the Mid into High.
    for (int ch = 0; ch < numCh; ++ch)
    {
        mLowBuf.copyFrom (ch, 0, buffer, ch, 0, n);
        mMidBuf.copyFrom (ch, 0, buffer, ch, 0, n);
    }
    {
        juce::dsp::AudioBlock<float> lowBlk (mLowBuf);
        auto sub = lowBlk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        mXover1Lp.process (ctx);
    }
    {
        juce::dsp::AudioBlock<float> midBlk (mMidBuf);
        auto sub = midBlk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        mXover1Hp.process (ctx);
    }
    for (int ch = 0; ch < numCh; ++ch)
        mHighBuf.copyFrom (ch, 0, mMidBuf, ch, 0, n);
    {
        juce::dsp::AudioBlock<float> midBlk (mMidBuf);
        auto sub = midBlk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        mXover2Lp.process (ctx);
    }
    {
        juce::dsp::AudioBlock<float> highBlk (mHighBuf);
        auto sub = highBlk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        mXover2Hp.process (ctx);
    }

    // Compressor params after macro mapping.
    const float comp01      = juce::jlimit (0.0f, 1.0f, mComp);
    const float threshDb    = juce::jmap (comp01, 0.0f, 1.0f,
                                            kCompMinThreshDb, kCompMaxThreshDb);
    const float threshLin   = juce::Decibels::decibelsToGain (threshDb, -120.0f);
    const float threshSq    = threshLin * threshLin;
    const float ratioMul    = juce::jmap (comp01, 0.0f, 1.0f,
                                            kCompMinRatioMul, kCompMaxRatioMul);
    const float effRatio    = mRatio * ratioMul;

    // Per-band per-sample compress, then sum.  Track worst-case GR for the
    // meter (most-negative value across bands and channels).
    float worstGrDb = 0.0f;

    auto compressBand = [&] (juce::AudioBuffer<float>& bandBuf, BandState& st)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* d = bandBuf.getWritePointer (ch);
            float& env = (ch == 0) ? st.envL : st.envR;
            for (int i = 0; i < n; ++i)
            {
                const float xSq = d[i] * d[i];
                // Asymmetric envelope: fast attack, slow release.
                const float coef = (xSq > env) ? mAttackCoef : mReleaseCoef;
                env = coef * env + (1.0f - coef) * xSq;
                const float g = computeBandGain (env, threshSq, effRatio);
                d[i] *= g;
                if (g < 1.0f)
                {
                    const float grDb = juce::Decibels::gainToDecibels (g, -60.0f);
                    if (grDb < worstGrDb) worstGrDb = grDb;
                }
            }
        }
    };

    compressBand (mLowBuf,  mLow);
    compressBand (mMidBuf,  mMid);
    compressBand (mHighBuf, mHigh);

    // Sum bands back into buffer.
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* lo = mLowBuf .getReadPointer (ch);
        const float* mi = mMidBuf .getReadPointer (ch);
        const float* hi = mHighBuf.getReadPointer (ch);
        float* out      = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            out[i] = lo[i] + mi[i] + hi[i];
    }

    // Output level.
    if (mLevel != 0.0f)
        buffer.applyGain (juce::Decibels::decibelsToGain (mLevel, -60.0f));

    // GR meter hold + decay (same pattern as CompressorDSP).
    const float prevGr    = mGainReductionDb.load (std::memory_order_relaxed);
    const float decayedGr = juce::jmin (0.0f, prevGr + mGrDecayDbPerBlock);
    mGainReductionDb.store (juce::jmin (worstGrDb, decayedGr),
                              std::memory_order_relaxed);
}

void BassCompressorStyleDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("BassCompressorStyleDSP");
    state.setProperty ("comp",    mComp,          nullptr);
    state.setProperty ("ratio",   mRatio,         nullptr);
    state.setProperty ("release", mReleaseMs,     nullptr);
    state.setProperty ("level",   mLevel,         nullptr);
    state.setProperty ("bypassed", (int) bypassed, nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void BassCompressorStyleDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (! xml || ! xml->hasTagName ("BassCompressorStyleDSP")) return;
    auto state = juce::ValueTree::fromXml (*xml);
    setComp      ((float)(double) state.getProperty ("comp",    0.5));
    setRatio     ((float)(double) state.getProperty ("ratio",   4.0));
    setReleaseMs ((float)(double) state.getProperty ("release", 200.0));
    setLevel     ((float)(double) state.getProperty ("level",   0.0));
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;
}
