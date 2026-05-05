#include "AcousticSimulatorStyleDSP.h"

namespace
{
    // Schroeder delay lengths -- offset from AD Style's so the two effects
    // sound distinct when stacked.  All in samples @ 44.1 kHz, scaled at
    // prepare() time.
    constexpr int kCombLensL[4]    = { 1641, 1719, 1481, 1392 };
    constexpr int kCombLensR[4]    = { 1664, 1742, 1504, 1415 };
    constexpr int kAllpassLensL[2] = { 213,  548  };
    constexpr int kAllpassLensR[2] = { 236,  571  };

    // Envelope follower attack 5 ms / release 100 ms.
    float coefFromMs (float ms, double sr)
    {
        if (ms <= 0.0f) return 0.0f;
        return std::exp (-1.0f / (float) (0.001 * ms * sr));
    }
}

AcousticSimulatorStyleDSP::AcousticSimulatorStyleDSP() = default;

void AcousticSimulatorStyleDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;

    juce::dsp::ProcessSpec spec { sampleRate,
                                   (juce::uint32) juce::jmax (1, maxBlockSize),
                                   2 };

    mPickupEQ.prepare (spec);
    mBodyA   .prepare (spec);
    mBodyB   .prepare (spec);
    mTopShelf.prepare (spec);
    mConv    .reset();
    mConv    .prepare (spec);

    rebuildPickupEQ();
    rebuildBodyEQ();
    rebuildTopShelf();

    mEnvAttackCoef  = coefFromMs (5.0f,   sampleRate);
    mEnvReleaseCoef = coefFromMs (100.0f, sampleRate);
    mEnv[0] = mEnv[1] = 0.0f;
    mEnvAvg = 0.0f;

    const double scale = sampleRate / 44100.0;
    for (int i = 0; i < 4; ++i)
    {
        mCombs[i][0].prepare ((int) std::lround (kCombLensL[i] * scale));
        mCombs[i][1].prepare ((int) std::lround (kCombLensR[i] * scale));
    }
    for (int i = 0; i < 2; ++i)
    {
        mAllpasses[i][0].prepare ((int) std::lround (kAllpassLensL[i] * scale));
        mAllpasses[i][1].prepare ((int) std::lround (kAllpassLensR[i] * scale));
    }

    mShelfScratch.setSize (2, juce::jmax (1, maxBlockSize), false, true, true);
    mRevScratch  .setSize (2, juce::jmax (1, maxBlockSize), false, true, true);

    mUserConvDirty = true;
    if (mMode == Mode::User) loadUserIRConvolution();
}

void AcousticSimulatorStyleDSP::reset()
{
    mPickupEQ.reset();
    mBodyA.reset();
    mBodyB.reset();
    mTopShelf.reset();
    mConv.reset();
    mEnv[0] = mEnv[1] = 0.0f;
    mEnvAvg = 0.0f;
    for (auto& row : mCombs)    for (auto& c : row) c.reset();
    for (auto& row : mAllpasses) for (auto& a : row) a.reset();
}

void AcousticSimulatorStyleDSP::rebuildPickupEQ()
{
    // Static -4 dB peak at 2.5 kHz, Q ~0.7.  Flattens electric pickup
    // resonance peak that exists in single-coil + humbucker frequency
    // response and is absent on acoustic body output.
    auto coefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter
                    (mSampleRate, 2500.0f, 0.7f,
                     juce::Decibels::decibelsToGain (-4.0f));
    *mPickupEQ.state = *coefs;
}

void AcousticSimulatorStyleDSP::rebuildTopShelf()
{
    // High-shelf at 4 kHz; gain modulated by envelope (set per sample).
    // We bake the maximum gain here; per-sample modulation rides on top.
    const float maxGainDb = juce::jlimit (-15.0f, 15.0f, mTopDb);
    auto coefs = juce::dsp::IIR::Coefficients<float>::makeHighShelf
                    (mSampleRate, 4000.0f, 0.7f,
                     juce::Decibels::decibelsToGain (maxGainDb));
    *mTopShelf.state = *coefs;
}

void AcousticSimulatorStyleDSP::rebuildBodyEQ()
{
    // Body modeler -- 2 parallel IIR stages (mBodyA + mBodyB), config varies
    // per Mode.  Body knob (-15..+15 dB) scales the gain of both stages.
    const float depth = juce::jlimit (-15.0f, 15.0f, mBodyDb) / 15.0f;  // -1..+1

    juce::dsp::IIR::Coefficients<float>::Ptr a, b;

    switch (mMode)
    {
        case Mode::Standard:
            // Low-mid scoop @ 600 Hz + neutral high (small upper-mid bump).
            a = juce::dsp::IIR::Coefficients<float>::makePeakFilter
                    (mSampleRate, 600.0f, 1.2f, juce::Decibels::decibelsToGain (-6.0f * depth));
            b = juce::dsp::IIR::Coefficients<float>::makePeakFilter
                    (mSampleRate, 4000.0f, 0.9f, juce::Decibels::decibelsToGain (3.0f * depth));
            break;
        case Mode::Jumbo:
            // Massive low shelf + slight low-mid cut.
            a = juce::dsp::IIR::Coefficients<float>::makeLowShelf
                    (mSampleRate, 120.0f, 0.7f, juce::Decibels::decibelsToGain (8.0f * depth));
            b = juce::dsp::IIR::Coefficients<float>::makePeakFilter
                    (mSampleRate, 500.0f, 1.0f, juce::Decibels::decibelsToGain (-3.0f * depth));
            break;
        case Mode::Enhanced:
            // Hi-fi: high-shelf boost + slight low boost.
            a = juce::dsp::IIR::Coefficients<float>::makeHighShelf
                    (mSampleRate, 6000.0f, 0.7f, juce::Decibels::decibelsToGain (6.0f * depth));
            b = juce::dsp::IIR::Coefficients<float>::makeLowShelf
                    (mSampleRate, 200.0f, 0.7f, juce::Decibels::decibelsToGain (3.0f * depth));
            break;
        case Mode::Piezo:
            // Upper-mid quack @ ~2.5 kHz + tight low cut.
            a = juce::dsp::IIR::Coefficients<float>::makePeakFilter
                    (mSampleRate, 2500.0f, 1.5f, juce::Decibels::decibelsToGain (8.0f * depth));
            b = juce::dsp::IIR::Coefficients<float>::makeHighPass
                    (mSampleRate, 100.0f, 0.7f);
            break;
        case Mode::User:
        default:
            // User mode bypasses the body EQ bank (Convolution handles it).
            // Set both stages to neutral 1.0 gain peak filters.
            a = juce::dsp::IIR::Coefficients<float>::makePeakFilter
                    (mSampleRate, 1000.0f, 0.7f, 1.0f);
            b = juce::dsp::IIR::Coefficients<float>::makePeakFilter
                    (mSampleRate, 1000.0f, 0.7f, 1.0f);
            break;
    }

    if (a) *mBodyA.state = *a;
    if (b) *mBodyB.state = *b;
}

void AcousticSimulatorStyleDSP::loadUserIRConvolution()
{
    mUserConvDirty = false;
    if (mUserIRPath.isNotEmpty())
    {
        juce::File f (mUserIRPath);
        if (f.existsAsFile())
        {
            mConv.loadImpulseResponse (f, juce::dsp::Convolution::Stereo::yes,
                                          juce::dsp::Convolution::Trim::yes,
                                          0,
                                          juce::dsp::Convolution::Normalise::yes);
            return;
        }
    }
    // Fallback: identity 1-sample IR (acts as bypass; user blends with Body knob).
    juce::AudioBuffer<float> empty (2, 1);
    empty.clear();
    empty.setSample (0, 0, 1.0f);
    empty.setSample (1, 0, 1.0f);
    mConv.loadImpulseResponse (std::move (empty), mSampleRate,
                                juce::dsp::Convolution::Stereo::yes,
                                juce::dsp::Convolution::Trim::yes,
                                juce::dsp::Convolution::Normalise::yes);
}

void AcousticSimulatorStyleDSP::setMode (int m)
{
    const Mode newMode = static_cast<Mode> (juce::jlimit (0, 4, m));
    if (mMode != newMode)
    {
        mMode = newMode;
        rebuildBodyEQ();
        if (mMode == Mode::User) mUserConvDirty = true;
    }
}

void AcousticSimulatorStyleDSP::setTopDb (float db)
{
    db = juce::jlimit (-15.0f, 15.0f, db);
    if (! juce::approximatelyEqual (mTopDb, db))
    {
        mTopDb = db;
        rebuildTopShelf();
    }
}

void AcousticSimulatorStyleDSP::setBodyDb (float db)
{
    db = juce::jlimit (-15.0f, 15.0f, db);
    if (! juce::approximatelyEqual (mBodyDb, db))
    {
        mBodyDb = db;
        rebuildBodyEQ();   // user-mode treats this as wet/dry, no rebuild needed,
                           // but harmless to call (returns identity coefs)
    }
}

void AcousticSimulatorStyleDSP::setReverb (float v01)
{
    mReverb01 = juce::jlimit (0.0f, 1.0f, v01);
}

void AcousticSimulatorStyleDSP::setLevelDb (float db)
{
    mLevelDb = juce::jlimit (-24.0f, 12.0f, db);
}

void AcousticSimulatorStyleDSP::loadUserIR (const juce::File& file)
{
    mUserIRPath = file.existsAsFile() ? file.getFullPathName() : juce::String();
    if (mMode == Mode::User) mUserConvDirty = true;
}

void AcousticSimulatorStyleDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    if (mUserConvDirty && mMode == Mode::User) loadUserIRConvolution();

    juce::dsp::AudioBlock<float> blk (buffer);
    auto sub = blk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
    juce::dsp::ProcessContextReplacing<float> ctx (sub);

    // ── 1. Pickup De-Emphasis ───────────────────────────────────────────────
    mPickupEQ.process (ctx);

    // ── 2. Transient Shaper ("Top" knob) ────────────────────────────────────
    // Process the high-shelf into a scratch, then envelope-modulate the wet
    // amount and blend back: out = dry + env * (wet - dry).  For Top<0 the
    // envelope DUCKS the highs (negative = pull pick attack out).
    if (! juce::approximatelyEqual (mTopDb, 0.0f))
    {
        if (mShelfScratch.getNumChannels() < numCh || mShelfScratch.getNumSamples() < n)
            mShelfScratch.setSize (numCh, n, false, false, true);
        for (int ch = 0; ch < numCh; ++ch)
            mShelfScratch.copyFrom (ch, 0, buffer, ch, 0, n);

        juce::dsp::AudioBlock<float> shBlk (mShelfScratch);
        auto shSub = shBlk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> shCtx (shSub);
        mTopShelf.process (shCtx);

        // Envelope follower per sample (stereo summed for tracking, then per-ch blend).
        for (int i = 0; i < n; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                peak = juce::jmax (peak, std::abs (buffer.getReadPointer (ch)[i]));

            // attack/release on instant peak
            const float coef = (peak > mEnv[0]) ? mEnvAttackCoef : mEnvReleaseCoef;
            mEnv[0] = peak + coef * (mEnv[0] - peak);
            // long-term running average
            const float avgCoef = 0.9995f;
            mEnvAvg = mEnv[0] * (1.0f - avgCoef) + mEnvAvg * avgCoef;

            // transient = how much instant exceeds running avg, 0..1
            const float ratio = (mEnvAvg > 1.0e-5f) ? (mEnv[0] / mEnvAvg) : 1.0f;
            const float transient = juce::jlimit (0.0f, 1.0f, (ratio - 1.0f) * 0.5f);

            for (int ch = 0; ch < numCh; ++ch)
            {
                float* dst = buffer.getWritePointer (ch);
                const float* sh = mShelfScratch.getReadPointer (ch);
                dst[i] = dst[i] + transient * (sh[i] - dst[i]);
            }
        }
    }

    // ── 3. Body Modeler ─────────────────────────────────────────────────────
    if (mMode == Mode::User)
    {
        // User mode: convolution with wet/dry from Body knob (-15..+15 dB
        // mapped to 0..1 wet; negative side acts as inverse-blend).
        juce::AudioBuffer<float> wet (numCh, n);
        for (int ch = 0; ch < numCh; ++ch)
            wet.copyFrom (ch, 0, buffer, ch, 0, n);
        juce::dsp::AudioBlock<float> wb (wet);
        auto wsub = wb.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> wctx (wsub);
        mConv.process (wctx);

        const float bodyMix = juce::jlimit (0.0f, 1.0f, (mBodyDb + 15.0f) / 30.0f); // -15..+15 -> 0..1
        const float wetGain = bodyMix;
        const float dryGain = 1.0f - bodyMix * 0.5f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* dst = buffer.getWritePointer (ch);
            const float* w = wet.getReadPointer (ch);
            for (int i = 0; i < n; ++i) dst[i] = dst[i] * dryGain + w[i] * wetGain;
        }
    }
    else
    {
        // Algorithmic mode: parallel-bank EQ via 2 series stages (parallel
        // sum approximated by two cascaded peak/shelf filters w/ depth-scaled
        // gains -- close enough for character emulation, less code).
        mBodyA.process (ctx);
        mBodyB.process (ctx);
    }

    // ── 4. Schroeder Reverb ─────────────────────────────────────────────────
    if (mReverb01 > 0.001f)
    {
        if (mRevScratch.getNumChannels() < numCh || mRevScratch.getNumSamples() < n)
            mRevScratch.setSize (numCh, n, false, false, true);

        for (int ch = 0; ch < numCh; ++ch)
        {
            const int chIdx = juce::jmin (ch, 1);
            float* rv = mRevScratch.getWritePointer (ch);
            const float* in = buffer.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
            {
                const float x = in[i];
                float c = 0.0f;
                for (int k = 0; k < 4; ++k) c += mCombs[k][chIdx].process (x);
                c *= 0.25f;
                float y = mAllpasses[0][chIdx].process (c);
                y       = mAllpasses[1][chIdx].process (y);
                rv[i] = y;
            }
        }

        const float wetGain = mReverb01;
        const float dryGain = 1.0f - mReverb01 * 0.5f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* dst = buffer.getWritePointer (ch);
            const float* rv = mRevScratch.getReadPointer (ch);
            for (int i = 0; i < n; ++i) dst[i] = dst[i] * dryGain + rv[i] * wetGain;
        }
    }

    // ── 5. Level (output gain) ──────────────────────────────────────────────
    if (! juce::approximatelyEqual (mLevelDb, 0.0f))
    {
        const float g = juce::Decibels::decibelsToGain (mLevelDb, -60.0f);
        buffer.applyGain (g);
    }
}

void AcousticSimulatorStyleDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("AcousticSimulatorStyleDSP");
    state.setProperty ("mode",     (int) mMode,    nullptr);
    state.setProperty ("topDb",    mTopDb,         nullptr);
    state.setProperty ("bodyDb",   mBodyDb,        nullptr);
    state.setProperty ("reverb",   mReverb01,      nullptr);
    state.setProperty ("levelDb",  mLevelDb,       nullptr);
    state.setProperty ("userIR",   mUserIRPath,    nullptr);
    state.setProperty ("bypassed", (int) bypassed, nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void AcousticSimulatorStyleDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (! xml || ! xml->hasTagName ("AcousticSimulatorStyleDSP")) return;
    auto state = juce::ValueTree::fromXml (*xml);

    mUserIRPath = state.getProperty ("userIR", juce::String()).toString();
    setMode    ((int)            state.getProperty ("mode",     (int) Mode::Standard));
    setTopDb   ((float)(double)  state.getProperty ("topDb",    0.0));
    setBodyDb  ((float)(double)  state.getProperty ("bodyDb",   0.0));
    setReverb  ((float)(double)  state.getProperty ("reverb",   0.2));
    setLevelDb ((float)(double)  state.getProperty ("levelDb",  0.0));
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;

    if (mMode == Mode::User) mUserConvDirty = true;
}
