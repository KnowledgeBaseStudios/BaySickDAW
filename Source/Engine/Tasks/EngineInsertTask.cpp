#include "EngineInsertTask.h"
#include "../../VibeGraph.h"
#include "../../DSP/EngineSidechainHelper.h"   // ISidechainEngine
#include "../SidechainPullHelper.h"            // pullSidechainPredecessorsToGraph

namespace
{
    inline VibeGraph::InsertKind toInsertKind (EngineInsertTask::Kind k) noexcept
    {
        switch (k)
        {
            case EngineInsertTask::Kind::Layer: return VibeGraph::InsertKind::Layer;
            case EngineInsertTask::Kind::Bass:  return VibeGraph::InsertKind::Bass;
            case EngineInsertTask::Kind::Drum:  return VibeGraph::InsertKind::Drum;
            case EngineInsertTask::Kind::Plugin: return VibeGraph::InsertKind::Plugin;
        }
        return VibeGraph::InsertKind::Layer;
    }
}

EngineInsertTask::EngineInsertTask (juce::AudioProcessor* engine,
                                    Kind                  kind,
                                    int                   index,
                                    int                   channelIdIn,
                                    VibeGraph&            graph)
    : mEngine (engine),
      mScEngine (dynamic_cast<ISidechainEngine*> (engine)),
      mKind (kind),
      mIndex (index),
      mGraph (&graph)
{
    this->channelId = channelIdIn;
}

juce::MidiBuffer* EngineInsertTask::resolveMidiBuffer() const noexcept
{
    if (mCtx == nullptr)
        return nullptr;

    switch (mKind)
    {
        case Kind::Layer: return mCtx->layerPageMidi != nullptr ? &mCtx->layerPageMidi[mIndex] : nullptr;
        case Kind::Bass:  return mCtx->bassPageMidi  != nullptr ? &mCtx->bassPageMidi [mIndex] : nullptr;
        case Kind::Drum:  return mCtx->drumPageMidi  != nullptr ? &mCtx->drumPageMidi [mIndex] : nullptr;
        case Kind::Plugin: return mCtx->pluginPageMidi != nullptr ? &mCtx->pluginPageMidi[mIndex] : nullptr;
    }
    return nullptr;
}

void EngineInsertTask::run()
{
    // Defensive: if any of the wired-state pointers are null (race during
    // engine create/destroy at message-thread teardown vs audio-thread
    // execution), fail safely (silence) rather than crash.
    if (mEngine == nullptr || mOutputBuffer == nullptr || mCtx == nullptr || mGraph == nullptr)
        return;

    const int n = mCtx->numSamples;
    if (n <= 0)
        return;

    // Build a "this-block" view: same float* storage as mOutputBuffer, but
    // reporting numSamples=n so engine processBlocks size their work loops
    // to the host's current block size rather than the arena's max.
    // getArrayOfWritePointers returns float* const* - preserve the const
    // qualifier on the inner pointer; AudioBuffer's constructor accepts it.
    float* const* ptrs = mOutputBuffer->getArrayOfWritePointers();
    juce::AudioBuffer<float> blockView (ptrs, mOutputBuffer->getNumChannels(), n);
    blockView.clear();

    // ── Sidechain fill + push ─────────────────────────────────────────────────
    // QA-Fe2 SC delay-match: fill + delay-match the strip's SC receive
    // buffers FIRST (pre-compensation source taps via the pull helper), then
    // hand those SAME aligned buffers to the engine -- engine-level SC and
    // rack/preEq/postEq SC read identical keys.  (Pre-QA-Fe2 the engine got
    // raw post-compensation predecessor outputs and the pull ran after the
    // engine render.)
    pullSidechainPredecessorsToGraph (*mGraph, channelId, mPredecessors, n);
    if (mScEngine != nullptr)
    {
        const VibeGraph::ScRecvArray scArr = mGraph->getScRecvArray (channelId);
        juce::AudioBuffer<float>* scBufs[VibeGraph::kMaxScRecvSlots] = {};
        for (int i = 0; i < VibeGraph::kMaxScRecvSlots; ++i)
            scBufs[i] = scArr[(size_t) i];
        mScEngine->setSidechainBuffers (scBufs, VibeGraph::kMaxScRecvSlots);
    }

    // ── Engine render ─────────────────────────────────────────────────────────
    juce::MidiBuffer  emptyMidi;
    juce::MidiBuffer* midi = resolveMidiBuffer();
    if (midi == nullptr)
        midi = &emptyMidi;

    mEngine->processBlock (blockView, *midi);

    // ── Insert chain (polarity → preEq → width → rack → postEq → fader …) ────
    mGraph->processInsert (toInsertKind (mKind), mIndex,
                           blockView, mCtx->bpm, mCtx->anySolo);

    // mOutputBuffer's storage now holds this strip's full output for the
    // first n samples. Downstream tasks pull from it via their mPredecessors.
}
