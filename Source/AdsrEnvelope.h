#pragma once
#include <JuceHeader.h>

class AdsrEnvelope
{
public:
    void prepare (double sampleRate);
    void setParameters (float attackSec, float decaySec, float sustain, float releaseSec);
    void noteOn();
    void noteOff();
    void reset();
    bool isActive() const;

    float getNextSample();

private:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    double mSampleRate { 44100.0 };
    Stage  mStage      { Stage::Idle };
    float  mLevel      { 0.0f };
    float  mAttack     { 0.01f };
    float  mDecay      { 0.1f };
    float  mSustain    { 0.8f };
    float  mRelease    { 0.3f };

    float mAttackCoef  { 0.0f };
    float mDecayCoef   { 0.0f };
    float mReleaseCoef { 0.0f };

    static float calcCoef (float timeSec, double sampleRate);
};
