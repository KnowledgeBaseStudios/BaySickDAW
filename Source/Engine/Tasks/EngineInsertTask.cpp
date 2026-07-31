#include "EngineInsertTask.h"
#include "../../VibeGraph.h"
#include "../../DSP/EngineSidechainHelper.h"   // ISidechainEngine
#include "../SidechainPullHelper.h"            // pullSidechainPredecessorsToGraph
#include "../../DSP/AudioClipStreamer.h"       // TS7 §6.3: frozen-tab playback

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
    // SONG MODE ONLY (2026-07-30, found by Jeff asking whether freeze helps in
    // pattern mode).  The freeze file is the SONG arrangement rendered from bar
    // 1, but in pattern mode the transport wraps inside the pattern loop -- so
    // reading it at the playhead would play the opening bars of the SONG, looped,
    // instead of the pattern.  Wrong audio WITH the CPU saving, which is worse
    // than no saving.  Falls back to the live engine, which is freeze's existing
    // stale-plays-live behaviour rather than a new path.
    //
    // Pattern-mode freeze needs a pattern-SCOPED render, which is precisely the
    // span §6.8 never saves.
    bool playedFrozen = false;
    if (mCtx->songMode)
    if (auto* fz = mFrozenSource.load (std::memory_order_acquire))
    {
        juce::int64 filePos = 0;
        if (mCtx->posInfo != nullptr)
            filePos = mCtx->posInfo->getTimeInSamples().orFallback ((juce::int64) 0);

        if (filePos >= 0 && filePos < fz->getTotalLength())
            playedFrozen = fz->readRaw (blockView, 0, n, filePos);
    }

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
