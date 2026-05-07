#include "RustyInsertTask.h"
#include "../../VibeGraph.h"
#include "../../PluginProcessor.h"
#include "../../BaySickRustyDrums/BaySickRustyDrumsProcessor.h"

RustyInsertTask::RustyInsertTask (int                 stripIndex,
                                  int                 channelIdIn,
                                  VibeGraph&          graph,
                                  VibeSynthProcessor& processor)
    : mStripIndex (stripIndex),
      mGraph (&graph),
      mProcessor (&processor)
{
    this->channelId = channelIdIn;
}

void RustyInsertTask::run()
{
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

    // Insert chain (polarity → preEq → width → rack → postEq → fader → ...).
    mGraph->processInsert (VibeGraph::InsertKind::Rusty, mStripIndex,
                           blockView, mCtx->bpm, mCtx->anySolo);
}
