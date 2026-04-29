#include "SynthFilter.h"

void SynthFilter::prepare (double sampleRate)
{
    mSampleRate = sampleRate;
    calcCoeffs();
}

void SynthFilter::setCutoff (float hz)
{
    mCutoff = juce::jlimit (20.0f, static_cast<float>(mSampleRate * 0.499), hz);
    calcCoeffs();
}

void SynthFilter::setResonance (float q)
{
    mResonance = juce::jlimit (0.1f, 10.0f, q);
    calcCoeffs();
}

void SynthFilter::setMode (FilterMode mode)
{
    mMode = mode;
}

void SynthFilter::reset()
{
    mIC1eq = mIC2eq = 0.0f;
}

float SynthFilter::processSample (float in)
{
    float v3 = in - mIC2eq;
    float v1 = mA1 * mIC1eq + mA2 * v3;
    float v2 = mIC2eq + mA2 * mIC1eq + mA3 * v3;
    mIC1eq = 2.0f * v1 - mIC1eq;
    mIC2eq = 2.0f * v2 - mIC2eq;

    switch (mMode)
    {
        case FilterMode::LowPass:  return v2;
        case FilterMode::HighPass: return in - mK * v1 - v2;
        case FilterMode::BandPass: return v1;
        case FilterMode::Notch:    return in - mK * v1;
        default:                   return v2;
    }
}

void SynthFilter::calcCoeffs()
{
    mG  = std::tan (juce::MathConstants<float>::pi * mCutoff / static_cast<float>(mSampleRate));
    mK  = 1.0f / mResonance;
    mA1 = 1.0f / (1.0f + mG * (mG + mK));
    mA2 = mG * mA1;
    mA3 = mG * mA2;
}
