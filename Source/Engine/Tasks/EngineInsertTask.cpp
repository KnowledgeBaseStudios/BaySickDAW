#include "EngineInsertTask.h"
#include "../FrozenSourceRead.h"   // TS7 6.8: scope-matched frozen block read
#include "../../BaySickGraph.h"
#include "../../DSP/EngineSidechainHelper.h"   // ISidechainEngine
#include "../SidechainPullHelper.h"            // pullSidechainPredecessorsToGraph
#include "../../DSP/AudioClipStreamer.h"       // TS7 §6.3: frozen-tab playback

namespace
{
    inline BaySickGraph::InsertKind toInsertKind (EngineInsertTask::Kind k) noexcept
    {
        switch (k)
        {
            case EngineInsertTask::Kind::Layer: return BaySickGraph::InsertKind::Layer;
            case EngineInsertTask::Kind::Bass:  return BaySickGraph::InsertKind::Bass;
            case EngineInsertTask::Kind::Drum:  return BaySickGraph::InsertKind::Drum;
            case EngineInsertTask::Kind::Plugin: return BaySickGraph::InsertKind::Plugin;
        }
        return BaySickGraph::InsertKind::Layer;
    }
}

EngineInsertTask::EngineInsertTask (juce::AudioProcessor* engine,
                                    Kind                  kind,
                                    int                   index,
                                    int                   channelIdIn,
                                    BaySickGraph&            graph)
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
        const BaySickGraph::ScRecvArray scArr = mGraph->getScRecvArray (channelId);
        juce::AudioBuffer<float>* scBufs[BaySickGraph::kMaxScRecvSlots] = {};
        for (int i = 0; i < BaySickGraph::kMaxScRecvSlots; ++i)
            scBufs[i] = scArr[(size_t) i];
        mScEngine->setSidechainBuffers (scBufs, BaySickGraph::kMaxScRecvSlots);
    }

    // ── Engine render, OR the frozen file in its place (TS7 §6.3) ────────────
    //
    // Freeze substitutes the SOURCE and nothing else: the insert chain below
    // runs identically, which is what Jeff's pre-rack "Source Only" ruling means
    // -- a frozen tab keeps its rack, EQ and fader live and editable.
    //
    // Reading here rather than at the InsertNode's dormant `preRenderedSrc` hook
    // is deliberate.  This is where the block's playhead is available (mCtx->
    // posInfo), the streamer's read is stateless per block on exactly that
    // position, and the engine call it replaces sits at the same point -- so the
    // swap is one branch instead of threading a buffer down into the graph.
    //
    // FALLS BACK TO THE LIVE ENGINE when the streamer cannot serve the block
    // (§6.6): a stale or missing freeze plays live until its re-render lands,
    // which is also what covers a project opened with no freeze cache at all.
    //
    // SCOPE-MATCHED (§6.8, 2026-07-31).  Song mode reads the arrangement render
    // at the absolute playhead; pattern mode reads THAT PATTERN's own render at a
    // loop-local position.  This was song-mode-ONLY until the span landed, which
    // meant pattern mode got no freeze at all -- no CPU reclaimed in exactly the
    // place layers are being stacked.  See FrozenSourceRead.h for why the
    // selection lives in one place rather than five copies.
    const bool playedFrozen = FreezeRead::serveBlock (*this, *mCtx, blockView, n);

    juce::MidiBuffer  emptyMidi;
    juce::MidiBuffer* midi = resolveMidiBuffer();
    if (midi == nullptr)
        midi = &emptyMidi;

    if (! playedFrozen)
        mEngine->processBlock (blockView, *midi);

    // ── Insert chain (polarity → preEq → width → rack → postEq → fader …) ────
    mGraph->processInsert (toInsertKind (mKind), mIndex,
                           blockView, mCtx->bpm, mCtx->anySolo);

    // mOutputBuffer's storage now holds this strip's full output for the
    // first n samples. Downstream tasks pull from it via their mPredecessors.
}
