#include "HostedPluginEffect.h"

namespace Hosting
{

void HostedPluginEffect::setPlugin (const juce::PluginDescription& desc)
{
    auto* pm = PluginManager::getInstance();

    if (pm == nullptr)
        return;

    mHosted = std::make_unique<HostedPluginInstance> (*pm, desc);
    // Fresh instance -- the playhead attaches to the NEW one on the next
    // transport push, not the destroyed one.
    mPlayHeadAttached = false;

    if (mPreparedRate > 0.0)
    {
        mHosted->setRateAndBufferSizeDetails (mPreparedRate, mPreparedBlock);
        mHosted->prepareToPlay (mPreparedRate, mPreparedBlock);
    }
}

juce::String HostedPluginEffect::getPluginName() const
{
    return mHosted != nullptr ? mHosted->getDescription().name : juce::String();
}

void HostedPluginEffect::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;

    mPreparedRate  = sampleRate;
    mPreparedBlock = maxBlockSize;

    mMidiScratch.ensureSize (256);

    if (mHosted != nullptr)
    {
        mHosted->setRateAndBufferSizeDetails (sampleRate, maxBlockSize);
        mHosted->prepareToPlay (sampleRate, maxBlockSize);
    }
}

void HostedPluginEffect::process (juce::AudioBuffer<float>& buffer)
{
    if (mHosted == nullptr)
        return;

    mMidiScratch.clear();
    mHosted->processBlock (buffer, mMidiScratch);
}

void HostedPluginEffect::reset()
{
    if (mHosted != nullptr && mPreparedRate > 0.0)
        mHosted->prepareToPlay (mPreparedRate, mPreparedBlock);
}

// TS7 (2026-07-31).  Called once per block by EffectRack::setHostTransport,
// BEFORE process(), so the position the plugin reads is this block's.
//
// The attach is one-shot and lazy rather than done at setPlugin time, because a
// slot can be loaded before the rack has ever been driven and the instance can
// be replaced underneath us -- checking the flag here is cheaper than finding
// every construction path, and re-attaching after a swap costs one store.
void HostedPluginEffect::setHostTransport (const DSPBase::HostTransport& tp)
{
    if (mHosted == nullptr)
    {
        mPlayHeadAttached = false;
        return;
    }

    // timeInSamples is mandatory -- JUCE's toProcessContext jassert-fails
    // without it.  Everything else is optional to the VST3 context, but a
    // partial transport is exactly the half-fix this change exists to remove.
    mPlayHead.mPos = {};
    mPlayHead.mPos.setBpm (tp.bpm);
    mPlayHead.mPos.setPpqPosition (tp.ppqPosition);
    mPlayHead.mPos.setTimeInSamples (tp.timeInSamples);
    mPlayHead.mPos.setIsPlaying (tp.isPlaying);
    mPlayHead.mPos.setTimeSignature (juce::AudioPlayHead::TimeSignature {
        juce::jmax (1, tp.timeSigNum), juce::jmax (1, tp.timeSigDen) });

    if (! mPlayHeadAttached)
    {
        mHosted->setPlayHead (&mPlayHead);
        mPlayHeadAttached = true;
    }
}

int HostedPluginEffect::getLatencySamples() const
{
    // BLU-301: EffectRack accumulates this and pokes VibeGraph's bus PDC, so a
    // latency-heavy plugin lines up through the path that already exists.
    return mHosted != nullptr ? mHosted->getLatencySamples() : 0;
}

// ── Automation ───────────────────────────────────────────────────────────────

juce::AudioProcessorParameter* HostedPluginEffect::findParam (const juce::String& paramId) const
{
    if (mHosted == nullptr)
        return nullptr;

    auto* inner = mHosted->getInner();

    if (inner == nullptr)
        return nullptr;

    for (auto* p : inner->getParameters())
        if (auto* hp = dynamic_cast<juce::AudioPluginInstance::HostedParameter*>(p))
            if (hp->getParameterID() == paramId)
                return p;

    return nullptr;
}

juce::Array<HostedPluginEffect::AutomatableParam> HostedPluginEffect::getAutomatableParams() const
{
    juce::Array<AutomatableParam> out;

    if (mHosted == nullptr)
        return out;

    auto* inner = mHosted->getInner();

    if (inner == nullptr)
        return out;

    for (auto* p : inner->getParameters())
    {
        if (p == nullptr || ! p->isAutomatable())
            continue;

        if (auto* hp = dynamic_cast<juce::AudioPluginInstance::HostedParameter*>(p))
            out.add ({ hp->getParameterID(), p->getName (64) });
    }

    return out;
}

bool HostedPluginEffect::applyParamNorm (const juce::String& paramId, float v01)
{
    auto* p = findParam (paramId);

    if (p == nullptr)
        return false;

    // setValueNotifyingHost rather than setValue so the plugin's own editor
    // follows an automated move, the same way our panels' display refresh does.
    p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v01));
    return true;
}

float HostedPluginEffect::readParamNorm (const juce::String& paramId, float fallback) const
{
    auto* p = findParam (paramId);
    return p != nullptr ? p->getValue() : fallback;
}

bool HostedPluginEffect::applyParamNorm (DSPBase* dsp, const juce::String& paramId, float v01)
{
    if (auto* self = dynamic_cast<HostedPluginEffect*>(dsp))
        return self->applyParamNorm (paramId, v01);

    return false;
}

float HostedPluginEffect::readParamNorm (DSPBase* dsp, const juce::String& paramId, float fallback)
{
    if (auto* self = dynamic_cast<HostedPluginEffect*>(dsp))
        return self->readParamNorm (paramId, fallback);

    return fallback;
}

void HostedPluginEffect::getStateInformation (juce::MemoryBlock& dest)
{
    if (mHosted != nullptr)
        mHosted->getStateInformation (dest);
    else
        dest.reset();
}

void HostedPluginEffect::setStateInformation (const void* data, int size)
{
    if (data == nullptr || size <= 0)
        return;

    // Rebuild from the description in the blob rather than the added list -- a
    // project that references a plugin the user has since un-added must still
    // load it.
    if (mHosted == nullptr)
    {
        if (auto desc = HostedPluginInstance::descriptionFromState (data, size))
            setPlugin (*desc);
    }

    if (mHosted != nullptr)
        mHosted->setStateInformation (data, size);
}

} // namespace Hosting
