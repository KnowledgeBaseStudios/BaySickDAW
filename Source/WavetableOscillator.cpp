#include "WavetableOscillator.h"

WavetableOscillator::WavetableOscillator()
{
    // Allocate all large arrays on the heap
    mTable.assign (WAVETABLE_SIZE, 0.0f);

    mUnisonPhases.assign (MAX_UNISON, 0.0f);
    mUnisonDeltas.assign (MAX_UNISON, 0.0f);

    buildSaw();
}

void WavetableOscillator::prepare (double sampleRate)
{
    mSampleRate = sampleRate;
    recalcDeltas();
}

void WavetableOscillator::setFrequency (float hz)
{
    mPhaseDelta = static_cast<float> (hz / mSampleRate);
    recalcDeltas();
}

void WavetableOscillator::setDetune (float semitones)
{
    mDetune = semitones;
    recalcDeltas();
}

void WavetableOscillator::setUnisonVoices (int voices, float spread)
{
    mUnisonVoices = juce::jlimit (1, MAX_UNISON, voices);
    mUnisonSpread = spread;
    recalcDeltas();
}

void WavetableOscillator::reset()
{
    mPhase = 0.0f;
    std::fill (mUnisonPhases.begin(), mUnisonPhases.end(), 0.0f);
}

float WavetableOscillator::getNextSample()
{
    float out = 0.0f;
    for (int v = 0; v < mUnisonVoices; ++v)
    {
        out += sampleTable (mUnisonPhases[v]);
        mUnisonPhases[v] += mUnisonDeltas[v];
        if (mUnisonPhases[v] >= 1.0f) mUnisonPhases[v] -= 1.0f;
    }
    return out / static_cast<float> (mUnisonVoices);
}

void WavetableOscillator::buildSaw()
{
    for (int i = 0; i < WAVETABLE_SIZE; ++i)
        mTable[i] = 2.0f * (static_cast<float>(i) / WAVETABLE_SIZE) - 1.0f;
}

void WavetableOscillator::buildSquare()
{
    for (int i = 0; i < WAVETABLE_SIZE; ++i)
        mTable[i] = (i < WAVETABLE_SIZE / 2) ? 1.0f : -1.0f;
}

void WavetableOscillator::buildSine()
{
    for (int i = 0; i < WAVETABLE_SIZE; ++i)
        mTable[i] = std::sin (2.0f * juce::MathConstants<float>::pi
                              * static_cast<float>(i) / WAVETABLE_SIZE);
}

float WavetableOscillator::sampleTable (float phase) const
{
    float idx  = phase * WAVETABLE_SIZE;
    int   i0   = static_cast<int> (idx) % WAVETABLE_SIZE;
    int   i1   = (i0 + 1) % WAVETABLE_SIZE;
    float frac = idx - static_cast<int> (idx);
    return mTable[i0] * (1.0f - frac) + mTable[i1] * frac;
}

void WavetableOscillator::recalcDeltas()
{
    float detuneRatio = std::pow (2.0f, mDetune / 12.0f);
    float baseDelta   = mPhaseDelta * detuneRatio;

    if (mUnisonVoices == 1)
    {
        mUnisonDeltas[0] = baseDelta;
        return;
    }

    for (int v = 0; v < mUnisonVoices; ++v)
    {
        float spreadFrac = static_cast<float>(v) / (mUnisonVoices - 1) - 0.5f;
        float semOff     = spreadFrac * mUnisonSpread;
        float ratio      = std::pow (2.0f, semOff / 12.0f);
        mUnisonDeltas[v] = baseDelta * ratio;
    }
}
