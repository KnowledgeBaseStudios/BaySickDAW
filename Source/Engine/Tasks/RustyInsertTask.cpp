#include "RustyInsertTask.h"
#include "../../VibeGraph.h"
#include "../../PluginProcessor.h"
#include "../../BaySickRustyDrums/BaySickRustyDrumsProcessor.h"
#include "../SidechainPullHelper.h"   // pullSidechainPredecessorsToGraph
#include "../RenderEngineFlags.h"     // QA-DispatcherAffinity TraceScope

RustyInsertTask::RustyInsertTask (int                 stripIndex,
                                  int                 channelIdIn,
                                  VibeGraph&          graph,
                                  VibeSynthProcessor& processor)
    : mStripIndex (stripIndex),
      mGraph (&graph),
      mProcessor (&processor)
{
    this->channelId = channelIdIn;

    // QA-Sfizz Sub-K Serial Fallback (2026-05-28): pin to the audio thread.
    // Inherits the producer's pin rationale (see RustyDrumsProducerTask.cpp).
    // The 13 RustyInsertTasks read their strip from mMultiOutScratch via
    // getStripBuffer; the read-write race across block boundaries with the
    // producer's processStrips write was the Candidate A hypothesis for the
    // MT-mode bit-crusher on long-sustaining cymbals/hi-hats.  Pinning all 14
    // tasks (1 producer + 13 inserts) to the audio thread serializes them
    // sequentially within the dispatcher graph, eliminating both that race
    // and the suspected thread-local-state migration affecting voice
    // continuity.  Retired by QA-DispatcherAffinity.
    //
    // QA-DispatcherAffinity Task 2 Stage B (2026-05-29): gated by the
    // runtime gSubKOverride at VibeThreadPool::submit() time -- see the
    // matching comment on RustyDrumsProducerTask.cpp + the MtDiagnostic
    // namespace in RenderEngineFlags.h.
    this->mAudioThreadOnly = true;
}

void RustyInsertTask::run()
{
    // QA-DispatcherAffinity (2026-05-28): entry+exit timestamp trace.
    // engineInstance = the BaySickRustyDrumsProcessor singleton (matches
    // RustyDrumsProducerTask's trace so the analyzer can correlate the
    // 14 Rusty tasks by engine).
    RenderEngine::MtDiagnostic::TraceScope qaTrace (channelId,
        (mProcessor != nullptr) ? (const void*) mProcessor->mRustyDrumsEngine.get() : nullptr);

    if (mOutputBuffer == nullptr || mCtx == nullptr
        || mGraph == nullptr || mProcessor == nullptr)
        return;

    const int n = mCtx->numSamples;
    if (n <= 0) return;

    if (! mProcessor->mRustyDrumsActive.load (std::memory_order_acquire))
    {
        mOutputBuffer->clear();
        return;
    }

    // The producer (RustyDrumsProducerTask) holds the engine's spin lock
    // briefly while it calls processStrips.  By the time we run, the
    // synthetic dep guarantees the producer has finished.  We still
    // try-lock here defensively in case of a kit-load race; on miss we
    // produce silence rather than risk reading half-loaded engine state.
    juce::SpinLock::ScopedTryLockType lk (mProcessor->mRustyDrumsEngineLock);
    if (! lk.isLocked())
    {
        mOutputBuffer->clear();
        return;
    }

    auto* engine = mProcessor->mRustyDrumsEngine.get();
    if (engine == nullptr)
    {
        mOutputBuffer->clear();
        return;
    }

    if (mStripIndex < 0 || mStripIndex >= engine->getStripCount())
    {
        mOutputBuffer->clear();
        return;
    }

    // Per-block view of the arena slot.
    float* const* ptrs = mOutputBuffer->getArrayOfWritePointers();
    juce::AudioBuffer<float> blockView (ptrs, mOutputBuffer->getNumChannels(), n);
    blockView.clear();

    // Copy this strip's output from the engine's internal buffer.
    auto stripBuf = engine->getStripBuffer (mStripIndex, n);
    if (stripBuf.getNumChannels() >= 2 && blockView.getNumChannels() >= 2)
    {
        blockView.copyFrom (0, 0, stripBuf, 0, 0, n);
        blockView.copyFrom (1, 0, stripBuf, 1, 0, n);
    }
    else if (stripBuf.getNumChannels() >= 1)
    {
        if (blockView.getNumChannels() > 0)
            blockView.copyFrom (0, 0, stripBuf, 0, 0, n);
        if (blockView.getNumChannels() > 1)
            blockView.copyFrom (1, 0, stripBuf, 0, 0, n);   // dual-mono
    }

    // 2026-05-07 (Batch 9c follow-up): SC accumulator population.
    pullSidechainPredecessorsToGraph (*mGraph, channelId, mPredecessors, n);

    // Insert chain (polarity → preEq → width → rack → postEq → fader → ...).
    mGraph->processInsert (VibeGraph::InsertKind::Rusty, mStripIndex,
                           blockView, mCtx->bpm, mCtx->anySolo);
}
