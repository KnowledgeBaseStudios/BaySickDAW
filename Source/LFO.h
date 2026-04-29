#pragma once
#include <JuceHeader.h>

enum class LFOShape { Sine, Triangle, Square, SawDown, SawUp, SampleAndHold };

class LFO
{
public:
    void prepare (double sampleRate);
    void setRate  (float hz);
    void setShape (LFOShape shape);
    void setDepth (float depth);  // 0–1
    void reset();

    float getNextSample();  // returns -depth..+depth

private:
    double   mSampleRate { 44100.0 };
    float    mRate       { 1.0f };
    LFOShape mShape      { LFOShape::Sine };
    float    mDepth      { 1.0f };
    float    mPhase      { 0.0f };
    float    mDelta      { 0.0f };
    float    mSHValue    { 0.0f };

    juce::Random mRandom;
    void recalc();
};
