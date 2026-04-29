#include "FMOscillator.h"

void FMOscillator::prepare (double sampleRate)
{
    mSampleRate = sampleRate;
    recalc();
}

void FMOscillator::setCarrierFrequency (float hz)
{
    mCarrierFreq = hz;
    recalc();
}

void FMOscillator::setRatio (float ratio)
{
    mRatio = ratio;
    recalc();
}

void FMOscillator::setIndex (float index)
{
    mIndex = index;
}

void FMOscillator::reset()
{
    mCarrierPhase   = 0.0f;
    mModulatorPhase = 0.0f;
}

float FMOscillator::getNextSample()
{
    float modSample = std::sin (juce::MathConstants<float>::twoPi * mModulatorPhase);
    float carrierInput = juce::MathConstants<float>::twoPi * mCarrierPhase
                         + mIndex * modSample;
    float out = std::sin (carrierInput);

    mCarrierPhase   += mCarrierDelta;
    mModulatorPhase += mModDelta;
    if (mCarrierPhase   >= 1.0f) mCarrierPhase   -= 1.0f;
    if (mModulatorPhase >= 1.0f) mModulatorPhase -= 1.0f;

    return out;
}

void FMOscillator::recalc()
{
    mCarrierDelta = static_cast<float> (mCarrierFreq / mSampleRate);
    mModDelta     = static_cast<float> ((mCarrierFreq * mRatio) / mSampleRate);
}
