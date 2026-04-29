#pragma once
#include <JuceHeader.h>

// Simple 2-operator FM: carrier frequency modulated by a sine modulator.
// Output = sin(2π·fc·t + index·sin(2π·fm·t))
class FMOscillator
{
public:
    void prepare (double sampleRate);
    void setCarrierFrequency (float hz);
    void setRatio    (float ratio);   // modulator = carrier * ratio
    void setIndex    (float index);   // modulation depth (0–10)
    void reset();

    float getNextSample();

private:
    double mSampleRate      { 44100.0 };
    float  mCarrierFreq     { 440.0f };
    float  mRatio           { 2.0f };
    float  mIndex           { 1.0f };
    float  mCarrierPhase    { 0.0f };
    float  mModulatorPhase  { 0.0f };
    float  mCarrierDelta    { 0.0f };
    float  mModDelta        { 0.0f };

    void recalc();
};
