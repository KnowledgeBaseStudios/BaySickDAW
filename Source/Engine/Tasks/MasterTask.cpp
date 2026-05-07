#include "MasterTask.h"
#include "../../VibeGraph.h"
#include "../../PluginProcessor.h"

MasterTask::MasterTask (VibeGraph&          graph,
                        VibeSynthProcessor& processor,
                        std::atomic<bool>&  doneFlag)
    : mGraph (&graph),
      mProcessor (&processor),
      mDoneFlag (&doneFlag)
{
    this->channelId = MixerChannelIds::kMaster;
}

void MasterTask::run()
{
    // Defensive: should never run without context, but flag-flip safety.
    if (mOutputBuffer == nullptr || mCtx == nullptr
        || mGraph == nullptr || mDoneFlag == nullptr)
    {
        if (mDoneFlag) mDoneFlag->store (true, std::memory_order_release);
        return;
    }

    const int n = mCtx->numSamples;
    if (n <= 0)
    {
        mDoneFlag->store (true, std::memory_order_release);
        return;
    }

    // Per-block view of the master arena slot.
    float* const* ptrs = mOutputBuffer->getArrayOfWritePointers();
    juce::AudioBuffer<float> blockView (ptrs, mOutputBuffer->getNumChannels(), n);
    blockView.clear();

    // ── Sum predecessors (the 11 bus PassiveStripTasks + any direct-to-
    //    master insert sends) into our slot.  Acquire ordering on the dep
    //    counter (handled by VibeThreadPool::runOneTask) guarantees we see
    //    each upstream's published mOutputBuffer.
    const int nc = juce::jmin (2, blockView.getNumChannels());
    for (const auto& link : mPredecessors)
    {
        if (link.isSc) continue;   // SC is a side-channel, never sums into Master
        if (link.source == nullptr || link.source->mOutputBuffer == nullptr) continue;

        const float gain = link.isMainOut
            ? 1.0f
            : juce::Decibels::decibelsToGain (link.gainDb, -60.0f);

        const auto& src = *link.source->mOutputBuffer;
        const int srcCh = juce::jmin (nc, src.getNumChannels());
        for (int c = 0; c < srcCh; ++c)
            blockView.addFrom (c, 0, src, c, 0, n, gain);
    }

    // ── Run master DSP (rack + EQ + fader + peak drain) in-place ─────────────
    mGraph->processMasterBus (blockView, mCtx->bpm);

    // ── Signal the dispatcher's runUntil loop ────────────────────────────────
    // Release ordering publishes our writes to blockView (== arena slot) to
    // anyone who acquires this flag (the dispatcher's runUntil loop).
    mDoneFlag->store (true, std::memory_order_release);
}
