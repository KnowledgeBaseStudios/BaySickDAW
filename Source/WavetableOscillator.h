#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>

static constexpr int WAVETABLE_SIZE  = 2048;
static constexpr int MAX_UNISON      = 7;

class WavetableOscillator
{
public:
    WavetableOscillator();

    void prepare (double sampleRate);
    void setFrequency (float hz);
    void setDetune (float semitones);
    void setUnisonVoices (int voices, float spread);
    void reset();

    float getNextSample();

    void buildSaw();
    void buildSquare();
    void buildSine();

private:
    // Heap-allocated - no stack overflow risk
    std::vector<float> mTable;
    std::vector<float> mUnisonPhases;
    std::vector<float> mUnisonDeltas;

    double mSampleRate  { 44100.0 };
    float  mPhase       { 0.0f };
    float  mPhaseDelta  { 0.0f };
    float  mDetune      { 0.0f };
    int    mUnisonVoices{ 1 };
    float  mUnisonSpread{ 0.0f };

    float sampleTable (float phase) const;
    void  recalcDeltas();
};
