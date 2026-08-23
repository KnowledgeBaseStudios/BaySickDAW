#include "DirectFileTask.h"
#include "../../BaySickGraph.h"
#include "../SidechainPullHelper.h"
#include "../../DSP/AudioClipStreamer.h"

DirectFileTask::DirectFileTask (int index, int channelIdIn, BaySickGraph& graph,
                                std::unique_ptr<AudioClipStreamer> streamer)
    : mIndex (index), mGraph (&graph), mStreamer (std::move (streamer))
{
    this->channelId = channelIdIn;
}

DirectFileTask::~DirectFileTask() = default;

void DirectFileTask::run()
{
    if (mOutputBuffer == nullptr || mCtx == nullptr || mGraph == nullptr)
        return;
    const int n = mCtx->numSamples;
    if (n <= 0)
        return;

    float* const* ptrs = mOutputBuffer->getArrayOfWritePointers();
    juce::AudioBuffer<float> blockView (ptrs, mOutputBuffer->getNumChannels(), n);
    blockView.clear();

    pullSidechainPredecessorsToGraph (*mGraph, channelId, mPredecessors, n);

    // Transport-locked from bar 1: the file's position IS the transport's
    // sample position (scaled by the file's rate), so play / stop / seek /
    // loop all follow the transport with no state of their own.  Past the end
    // of the file, or stopped, the strip is silent but its chain still runs
    // (tails through the rack keep ringing).
    auto* fz = mStreamer.get();
    if (fz != nullptr && mCtx->posInfo != nullptr && mCtx->posInfo->getIsPlaying())
    {
        const juce::int64 pos   = mCtx->posInfo->getTimeInSamples().orFallback ((juce::int64) 0);
        const juce::int64 total = fz->getTotalLength();
        if (total > 0 && pos >= 0)
        {
            // readAndMix rather than readRaw even at matching rates: it mixes a
            // mono file into both channels and lands on integer positions
            // exactly when the ratio is 1.0.
            const double fileRate = fz->getFileSampleRate();
            const double sessRate = mCtx->sampleRate > 0.0 ? mCtx->sampleRate : fileRate;
            const double ratio    = fileRate / sessRate;
            const double fpos     = (double) pos * ratio;
            if (fpos < (double) total)
                fz->readAndMix (blockView, 0, n, fpos, ratio, blockView.getNumChannels(), 1.0f);
        }
    }

    mGraph->processInsert (BaySickGraph::InsertKind::Direct, mIndex,
                           blockView, mCtx->bpm, mCtx->anySolo);
}
