#include "EngineChainProcessor.h"

EngineChainProcessor::EngineChainProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void EngineChainProcessor::setChain (std::initializer_list<juce::AudioProcessor*> stages)
{
    const juce::SpinLock::ScopedLockType lk (mLock);
    mStages.clear();
    mStages.reserve (stages.size());
    for (auto* s : stages)
        if (s != nullptr) mStages.push_back (s);

    if (mPrepared)
        for (auto* s : mStages)
            s->prepareToPlay (mSampleRate, mBlockSize);
}

void EngineChainProcessor::prepareToPlay (double sampleRate, int blockSize)
{
    const juce::SpinLock::ScopedLockType lk (mLock);
    mSampleRate = sampleRate;
    mBlockSize  = blockSize;
    mPrepared   = true;
    for (auto* s : mStages)
        if (s != nullptr) s->prepareToPlay (sampleRate, blockSize);
}

void EngineChainProcessor::releaseResources()
{
    const juce::SpinLock::ScopedLockType lk (mLock);
    for (auto* s : mStages)
        if (s != nullptr) s->releaseResources();
    mPrepared = false;
}

void EngineChainProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midi)
{
    const juce::SpinLock::ScopedTryLockType tryLk (mLock);
    if (! tryLk.isLocked()) return;

    juce::ScopedNoDenormals noDenormals;

    for (auto* s : mStages)
        if (s != nullptr) s->processBlock (buffer, midi);
}
