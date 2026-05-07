#include "EngineInsertTask.h"
#include "../../VibeGraph.h"
#include "../../DSP/EngineSidechainHelper.h"   // ISidechainEngine

namespace
{
    inline VibeGraph::InsertKind toInsertKind (EngineInsertTask::Kind k) noexcept
    {
        switch (k)
        {
            case EngineInsertTask::Kind::Layer: return VibeGraph::InsertKind::Layer;
            case EngineInsertTask::Kind::Bass:  return VibeGraph::InsertKind::Bass;
            case EngineInsertTask::Kind::Drum:  return VibeGraph::InsertKind::Drum;
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
    }
    return nullptr;
}

void EngineInsertTask::run()
{
    // Defensive: while kEnableMultiThreadedEngine is false this is dead code,
    // but once the flag flips we want any partially-wired state to fail safely
    // (silence) rather than crash.
    if (mEngine == nullptr || mOutputBuffer == nullptr || mCtx == nullptr || mGraph == nullptr)
        return;

    const int n = mCtx->numSamples;
    if (n <= 0)
        return;

    // Build a "this-block" view: same float* storage as mOutputBuffer, but
    // reporting numSamples=n so engine processBlocks size their work loops
    // to the host's current block size rather than the arena's max.
    // getArrayOfWritePointers returns float* const* — preserve the const
    // qualifier on the inner pointer; AudioBuffer's constructor accepts it.
    float* const* ptrs = mOutputBuffer->getArrayOfWritePointers();
    juce::AudioBuffer<float> blockView (ptrs, mOutputBuffer->getNumChannels(), n);
    blockView.clear();

    // ── Sidechain push ────────────────────────────────────────────────────────
    // Pull from each SC predecessor's mOutputBuffer (already written by the
    // upstream task — guaranteed by mDeps reaching zero before we run). Feed
    // into the engine via ISidechainEngine the same way the serial path does.
    if (mScEngine != nullptr)
    {
        juce::AudioBuffer<float>* scBufs[VibeGraph::kMaxScRecvSlots] = {};
        for (const auto& link : mPredecessors)
        {
            if (! link.isSc) continue;
            if (link.scSlot < 0 || link.scSlot >= VibeGraph::kMaxScRecvSlots) continue;
            if (link.source == nullptr || link.source->mOutputBuffer == nullptr) continue;
            scBufs[link.scSlot] = link.source->mOutputBuffer;
        }
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
