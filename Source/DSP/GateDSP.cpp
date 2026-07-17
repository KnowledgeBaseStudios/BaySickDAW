#include "GateDSP.h"

GateDSP::GateDSP()
{
    calcCoefs();
}

void GateDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;
    calcCoefs();
    reset();
}

void GateDSP::reset()
{
    mEnv = 0.0f;
    mGain = 1.0f;
    mOpen = false;
    mHoldRemain = 0;
    mGrDb.store (0.0f, std::memory_order_relaxed);
}

void GateDSP::calcCoefs()
{
    const double sr = juce::jmax (8000.0, mSampleRate);
    auto onePole = [sr] (float ms)
        { return (float) std::exp (-1.0 / (juce::jmax (0.01, (double) ms) * 0.001 * sr)); };
    mAttCoef     = onePole (attackMs);
    mRelCoef     = onePole (releaseMs);
    mDetCoef     = onePole (5.0f);
    mHoldSamples = (int) (holdMs * 0.001 * sr);
}

void GateDSP::setThresholdDb (float dB)
{
    const float n = juce::jlimit (-80.0f, 0.0f, dB);
    if (n != thresholdDb) thresholdDb = n;
}

void GateDSP::setRangeDb (float dB)
{
    const float n = juce::jlimit (-80.0f, 0.0f, dB);
    if (n != rangeDb) rangeDb = n;
}

void GateDSP::setAttackMs (float ms)
{
    const float n = juce::jlimit (0.1f, 100.0f, ms);
    if (n != attackMs) { attackMs = n; calcCoefs(); }
}

void GateDSP::setHoldMs (float ms)
{
    const float n = juce::jlimit (0.0f, 500.0f, ms);
    if (n != holdMs) { holdMs = n; calcCoefs(); }
}

void GateDSP::setReleaseMs (float ms)
{
    const float n = juce::jlimit (5.0f, 2000.0f, ms);
    if (n != releaseMs) { releaseMs = n; calcCoefs(); }
}

void GateDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals nd;
    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh <= 0 || n <= 0) return;

    const float openLin  = juce::Decibels::decibelsToGain (thresholdDb);
    const float closeLin = juce::Decibels::decibelsToGain (thresholdDb - kHystDb);
    const float floorLin = juce::Decibels::decibelsToGain (rangeDb);
    float minGain = 1.0f;

    for (int i = 0; i < n; ++i)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            peak = juce::jmax (peak, std::abs (buffer.getReadPointer (ch)[i]));
        mEnv = (peak > mEnv) ? peak : peak + mDetCoef * (mEnv - peak);

        if (mEnv >= openLin)
        {
            mOpen = true;
            mHoldRemain = mHoldSamples;
        }
        else if (mOpen && mEnv < closeLin)
        {
            if (mHoldRemain > 0) --mHoldRemain;
            else                 mOpen = false;
        }

        const float target = mOpen ? 1.0f : floorLin;
        const float coef   = (target > mGain) ? mAttCoef : mRelCoef;
        mGain = target + coef * (mGain - target);
        minGain = juce::jmin (minGain, mGain);

        for (int ch = 0; ch < numCh; ++ch)
            buffer.getWritePointer (ch)[i] *= mGain;
    }

    mGrDb.store (juce::Decibels::gainToDecibels (juce::jmax (1.0e-4f, minGain)),
                 std::memory_order_relaxed);
}
