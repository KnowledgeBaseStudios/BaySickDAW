#include "LFO.h"

void LFO::prepare (double sampleRate)
{
    mSampleRate = sampleRate;
    recalc();
}

void LFO::setRate (float hz)
{
    mRate = hz;
    recalc();
}

void LFO::setShape (LFOShape shape) { mShape = shape; }
void LFO::setDepth (float depth)   { mDepth = juce::jlimit (0.0f, 1.0f, depth); }

void LFO::reset() { mPhase = 0.0f; }

float LFO::getNextSample()
{
    float out = 0.0f;
    switch (mShape)
    {
        case LFOShape::Sine:
            out = std::sin (juce::MathConstants<float>::twoPi * mPhase);
            break;

        case LFOShape::Triangle:
            out = (mPhase < 0.5f) ? (4.0f * mPhase - 1.0f) : (3.0f - 4.0f * mPhase);
            break;

        case LFOShape::Square:
            out = (mPhase < 0.5f) ? 1.0f : -1.0f;
            break;

        case LFOShape::SawDown:
            out = 1.0f - 2.0f * mPhase;   // +1 → -1
            break;

        case LFOShape::SawUp:
            out = 2.0f * mPhase - 1.0f;   // -1 → +1
            break;

        case LFOShape::SampleAndHold:
        {
            float prevPhase = mPhase - mDelta;
            if (prevPhase < 0.0f || mPhase < prevPhase)  // crossed zero
                mSHValue = mRandom.nextFloat() * 2.0f - 1.0f;
            out = mSHValue;
            break;
        }
    }

    mPhase += mDelta;
    if (mPhase >= 1.0f) mPhase -= 1.0f;

    return out * mDepth;
}

void LFO::recalc()
{
    mDelta = static_cast<float> (mRate / mSampleRate);
}
