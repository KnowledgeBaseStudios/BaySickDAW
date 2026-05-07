#include "EngineChainProcessor.h"

EngineChainProcessor::EngineChainProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void EngineChainProcessor::setChain (std::initializer_list<juce::AudioProcessor*> stages)
{
    // K-5 fix (2026-05-05): build the new (non-null) stage list first and
    // compare to what's already there.  If identical, skip both the swap and
    // the prepareToPlay broadcast - repeatedly calling prepareToPlay on a
    // freshly-loaded sfizz engine triggers sfizz->setSampleRate which can
    // reset just-pushed CC defaults and silence the just-loaded kit.
    std::vector<juce::AudioProcessor*> next;
    next.reserve (stages.size());
    for (auto* s : stages)
        if (s != nullptr) next.push_back (s);

    {
        const juce::SpinLock::ScopedLockType lk (mLock);
        if (next == mStages) return;
        mStages = std::move (next);
    }

    if (mPrepared)
    {
        const juce::SpinLock::ScopedLockType lk (mLock);
        for (auto* s : mStages)
            if (s != nullptr) s->prepareToPlay (mSampleRate, mBlockSize);
    }
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
