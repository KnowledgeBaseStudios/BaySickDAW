#include "RustyInsertTask.h"
#include "../FrozenSourceRead.h"   // TS7 6.8: scope-matched frozen block read
#include "../../BaySickGraph.h"
#include "../../PluginProcessor.h"
#include "../../BaySickRustyDrums/BaySickRustyDrumsProcessor.h"
#include "../SidechainPullHelper.h"   // pullSidechainPredecessorsToGraph

RustyInsertTask::RustyInsertTask (int                 stripIndex,
                                  int                 channelIdIn,
                                  BaySickGraph&          graph,
                                  BaySickDAWProcessor& processor)
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

    // QA-DispatcherAffinity Task 3 (2026-05-29): try-lock REMOVED per Sub-A
    // = (i) resolution.  The 13 RustyInsertTasks are strict concurrent
    // readers of mMultiOutScratch -- the producer→13-insert synthetic dep
    // guarantees mMultiOutScratch is fully written before any insert reads
    // it, and concurrent reads of a static buffer are safe.  Pre-Task-3 the
    // try-lock here caused B.5 "try-lock-failure strip silencing" under MT
    // execution: 13 inserts racing for the engine spin lock + losers
    // clearing their output buffer = intermittent strip drops audible as
    // the bit-crusher distortion characterized in QA-DispatcherAffinity
    // Stage C analysis.  Lifecycle safety for engine swap / kit load is now
    // provided by the mProjectLoadInProgress shield raised at
    // destroyBaySickRustyDrums + loadBaySickRustyDrumsKit (audit-driven
    // additions in this same commit); the audio thread bails at
    // processBlock top during those mutations + sleeps 30 ms for in-flight
    // blocks to drain, so when this code runs the engine pointer is
    // guaranteed stable for the block.
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

    // ── TS7 §6.9: frozen kit (Jeff, 2026-07-30) ──────────────────────────────
    // The substitution is HERE, at the point the engine hands this strip over --
    // the same place every other track's freeze tap sits, above the insert chain.
    // That is what keeps the whole strip live while frozen: this task still runs
    // its rack, EQs, fader, pan, mute/solo, still publishes its meter and still
    // fills its arena slot, so sidechains fed from this piece keep working.
    //
    // Capturing at the kit BUS instead (my first design) would have baked all 13
    // strips' mixer settings and killed all of that.  See the plan's §6.9 entry.
    // §6.8 scope-matched.  See FrozenSourceRead.h.
    const bool playedFrozen = FreezeRead::serveBlock (*this, *mCtx, blockView, n);

    if (! playedFrozen)
    {
        // Copy this strip's output from the engine's internal buffer -- but
        // ONLY when the producer actually rendered this block.  The producer
        // skips under the frozen-kit gate and under idle suspend, and the
        // scratch then still holds the PREVIOUS render; the seq only advances
        // inside processStrips, so an unchanged seq means the correct fallback
        // is the silence blockView already holds, not a repeat of old audio.
        const juce::uint32 seq = engine->getStripRenderSeq();
        if (seq != mLastSeenRenderSeq)
        {
            mLastSeenRenderSeq = seq;
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
        }
    }

    // 2026-05-07 (Batch 9c follow-up): SC accumulator population.
    pullSidechainPredecessorsToGraph (*mGraph, channelId, mPredecessors, n);

    // Insert chain (polarity → preEq → width → rack → postEq → fader → ...).
    mGraph->processInsert (BaySickGraph::InsertKind::Rusty, mStripIndex,
                           blockView, mCtx->bpm, mCtx->anySolo);
}
