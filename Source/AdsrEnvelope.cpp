#include "AdsrEnvelope.h"

void AdsrEnvelope::prepare (double sampleRate)
{
    mSampleRate = sampleRate;
    setParameters (mAttack, mDecay, mSustain, mRelease);
}

void AdsrEnvelope::setParameters (float attackSec, float decaySec, float sustain, float releaseSec)
{
    mAttack       = attackSec;
    mDecay        = decaySec;
    mSustain      = juce::jlimit (0.0f, 1.0f, sustain);
    mRelease      = releaseSec;
    mAttackCoef   = calcCoef (attackSec,  mSampleRate);
    mDecayCoef    = calcCoef (decaySec,   mSampleRate);
    mReleaseCoef  = calcCoef (releaseSec, mSampleRate);
}

void AdsrEnvelope::noteOn()
{
    mStage = Stage::Attack;
}

void AdsrEnvelope::noteOff()
{
    if (mStage != Stage::Idle)
        mStage = Stage::Release;
}

void AdsrEnvelope::reset()
{
    mStage = Stage::Idle;
    mLevel = 0.0f;
}

bool AdsrEnvelope::isActive() const
{
    return mStage != Stage::Idle;
}

float AdsrEnvelope::getNextSample()
{
    switch (mStage)
    {
        case Stage::Attack:
            mLevel += mAttackCoef * (1.01f - mLevel);
            if (mLevel >= 1.0f) { mLevel = 1.0f; mStage = Stage::Decay; }
            break;

        case Stage::Decay:
            mLevel += mDecayCoef * (mSustain - 1.01f - mLevel);
            if (mLevel <= mSustain) { mLevel = mSustain; mStage = Stage::Sustain; }
            break;

        case Stage::Sustain:
            mLevel = mSustain;
            break;

        case Stage::Release:
            mLevel += mReleaseCoef * (0.0f - 1.01f - mLevel);
            if (mLevel <= 0.0001f) { mLevel = 0.0f; mStage = Stage::Idle; }
            break;

        case Stage::Idle:
        default:
            mLevel = 0.0f;
            break;
    }
    return mLevel;
}

float AdsrEnvelope::calcCoef (float timeSec, double sampleRate)
{
    if (timeSec <= 0.0f) return 1.0f;
    return static_cast<float> (1.0 - std::exp (-1.0 / (timeSec * sampleRate)));
}
