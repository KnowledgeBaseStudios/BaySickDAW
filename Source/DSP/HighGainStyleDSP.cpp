#include "HighGainStyleDSP.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)

namespace
{
    // Fixed pre-clip mid boost: locks the metal honk character.
    // QA-EffectsReview Task 5: steepened 700/+9 -> 1k/+36 to match the reference's
    // aggressive pre-gain (the tanh/clamp stages downstream bound it, so it stays stable).
    constexpr float kFixedMidBoostHz = 1000.0f;
    constexpr float kFixedMidBoostDb = 36.0f;

    // Fixed post-clip gyrator boosts (the real pedal's "V" is the valley between
    // a ~100 Hz low boost and a ~5 kHz high boost -- NOT a mid cut).
    constexpr float kFixedScoopLoHz = 100.0f;
    constexpr float kFixedScoopLoDb = 10.0f;
    constexpr float kFixedScoopLoQ  = 6.0f;
    constexpr float kFixedScoopHiHz = 5000.0f;
    constexpr float kFixedScoopHiDb = 10.0f;
    constexpr float kFixedScoopHiQ  = 3.0f;

    // User 3-band EQ shelf/peak frequencies.
    constexpr float kUserLowShelfHz  = 80.0f;
    constexpr float kUserHighShelfHz = 5000.0f;
}

void HighGainStyleDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;

    mOs.prepare (2, maxBlockSize);

    juce::dsp::ProcessSpec spec { sampleRate,
                                   (juce::uint32) juce::jmax (1, maxBlockSize),
                                   2 };
    mFixedMidBoost.prepare (spec);  mFixedMidBoost.reset();
    mFixedScoopLo .prepare (spec);  mFixedScoopLo .reset();
    mFixedScoopHi .prepare (spec);  mFixedScoopHi .reset();
    mUserLowShelf .prepare (spec);  mUserLowShelf .reset();
    mUserMidPeak  .prepare (spec);  mUserMidPeak  .reset();
    mUserHighShelf.prepare (spec);  mUserHighShelf.reset();

    mDcCoef = 1.0f - (float) (juce::MathConstants<double>::twoPi * 5.0 / sampleRate);
    mDcXL = mDcYL = mDcXR = mDcYR = 0.0f;

    initFixedEqCoefs();
    updateUserEqCoefs();
}

void HighGainStyleDSP::reset()
{
    mOs.reset();
    mFixedMidBoost.reset();
    mFixedScoopLo .reset();
    mFixedScoopHi .reset();
    mUserLowShelf .reset();
    mUserMidPeak  .reset();
    mUserHighShelf.reset();
    mDcXL = mDcYL = mDcXR = mDcYR = 0.0f;
}

void HighGainStyleDSP::initFixedEqCoefs()
{
    if (mSampleRate <= 0.0) return;
    *mFixedMidBoost.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        (double) mSampleRate, kFixedMidBoostHz, 1.2f,
        juce::Decibels::decibelsToGain (kFixedMidBoostDb, -60.0f));
    *mFixedScoopLo.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        (double) mSampleRate, kFixedScoopLoHz, kFixedScoopLoQ,
        juce::Decibels::decibelsToGain (kFixedScoopLoDb, -60.0f));
    *mFixedScoopHi.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        (double) mSampleRate, kFixedScoopHiHz, kFixedScoopHiQ,
        juce::Decibels::decibelsToGain (kFixedScoopHiDb, -60.0f));
}

void HighGainStyleDSP::updateUserEqCoefs()
{
    if (mSampleRate <= 0.0) return;
    *mUserLowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        (double) mSampleRate, kUserLowShelfHz, 0.7071f,
        juce::Decibels::decibelsToGain (mLowDb, -60.0f));
    *mUserMidPeak.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        (double) mSampleRate, juce::jlimit (200.0f, 5000.0f, mMidHz), 1.0f,
        juce::Decibels::decibelsToGain (mMidDb, -60.0f));
    *mUserHighShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        (double) mSampleRate, kUserHighShelfHz, 0.7071f,
        juce::Decibels::decibelsToGain (mHighDb, -60.0f));
}

void HighGainStyleDSP::setDist (float v01)
{
    v01 = juce::jlimit (0.0f, 1.0f, v01);
    if (mDist != v01) mDist = v01;
}

void HighGainStyleDSP::setHighDb (float dB)
{
    dB = juce::jlimit (-15.0f, 15.0f, dB);
    if (mHighDb != dB) { mHighDb = dB; updateUserEqCoefs(); }
}

void HighGainStyleDSP::setLowDb (float dB)
{
    dB = juce::jlimit (-15.0f, 15.0f, dB);
    if (mLowDb != dB) { mLowDb = dB; updateUserEqCoefs(); }
}

void HighGainStyleDSP::setMidHz (float hz)
{
    hz = juce::jlimit (200.0f, 5000.0f, hz);
    if (mMidHz != hz) { mMidHz = hz; updateUserEqCoefs(); }
}

void HighGainStyleDSP::setMidDb (float dB)
{
    dB = juce::jlimit (-15.0f, 15.0f, dB);
    if (mMidDb != dB) { mMidDb = dB; updateUserEqCoefs(); }
}

void HighGainStyleDSP::setLevel (float dB)
{
    dB = juce::jlimit (-24.0f, 12.0f, dB);
    if (mLevel != dB) mLevel = dB;
}

void HighGainStyleDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    juce::dsp::AudioBlock<float> block (buffer);

    // 1. Fixed mid-boost pre-clip (1x).
    {
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        mFixedMidBoost.process (ctx);
    }

    // 2 + 3. Upsample, cascading hard clip at 4x, downsample.
    auto upBlock = mOs.upsample (block);
    const int upN   = (int) upBlock.getNumSamples();
    const int upChs = (int) upBlock.getNumChannels();

    // Drive scalar -- huge at MT-2 max (+60 dB).
    const float drive = 1.0f + mDist * 999.0f;   // up to ~1000x
    constexpr float kStage1Ceiling = 0.85f;
    constexpr float kStage2Ceiling = 1.0f;

    for (int ch = 0; ch < upChs; ++ch)
    {
        float* d = upBlock.getChannelPointer ((size_t) ch);
        for (int i = 0; i < upN; ++i)
        {
            // Stage 1: clamp at 0.85 with a tiny tanh-shaped ramp into the
            // ceiling so the wave doesn't square off perfectly.  Acts as
            // the first transistor stage.
            float x = d[i] * drive;
            x = std::tanh (x * 1.2f) * kStage1Ceiling;

            // Stage 2: hard clamp at +/-1.  Locks the wave fully square.
            x = std::clamp (x, -kStage2Ceiling, kStage2Ceiling);

            d[i] = x;
        }
    }
    mOs.downsample (block);

    // 4. Fixed post-clip gyrator boosts (~100 Hz + ~5 kHz); the V is the valley between.
    {
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        mFixedScoopLo.process (ctx);
        mFixedScoopHi.process (ctx);
    }

    // 5. User 3-band EQ.
    {
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        mUserLowShelf .process (ctx);
        mUserMidPeak  .process (ctx);
        mUserHighShelf.process (ctx);
    }

    // 6. 5 Hz DC blocker (defensive -- 5 biquad stages can store tiny DC
    // across bypass-ramp transients).
    {
        const float R = mDcCoef;
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            float& xPrev = (ch == 0) ? mDcXL : mDcXR;
            float& yPrev = (ch == 0) ? mDcYL : mDcYR;
            for (int i = 0; i < n; ++i)
            {
                const float in = d[i];
                const float y  = in - xPrev + R * yPrev;
                xPrev = in;
                yPrev = y;
                d[i]  = y;
            }
        }
    }

    // 7. Output level.
    if (mLevel != 0.0f)
        buffer.applyGain (juce::Decibels::decibelsToGain (mLevel, -60.0f));
}

void HighGainStyleDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("HighGainStyleDSP");
    state.setProperty ("dist",   mDist,   nullptr);
    state.setProperty ("highDb", mHighDb, nullptr);
    state.setProperty ("lowDb",  mLowDb,  nullptr);
    state.setProperty ("midHz",  mMidHz,  nullptr);
    state.setProperty ("midDb",  mMidDb,  nullptr);
    state.setProperty ("level",  mLevel,  nullptr);
    state.setProperty ("bypassed", (int) bypassed, nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void HighGainStyleDSP::setStateInformation (const void* data, int sz)
{
    auto xml = SafeXml::parseBinaryBlob (data, sz);
    if (! xml || ! xml->hasTagName ("HighGainStyleDSP")) return;
    auto state = juce::ValueTree::fromXml (*xml);
    setDist   ((float)(double) state.getProperty ("dist",   0.5));
    setHighDb ((float)(double) state.getProperty ("highDb", 0.0));
    setLowDb  ((float)(double) state.getProperty ("lowDb",  0.0));
    setMidHz  ((float)(double) state.getProperty ("midHz",  800.0));
    setMidDb  ((float)(double) state.getProperty ("midDb",  0.0));
    setLevel  ((float)(double) state.getProperty ("level",  0.0));
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;
}
