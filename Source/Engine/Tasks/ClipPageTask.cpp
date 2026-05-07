#include "ClipPageTask.h"
#include "../../VibeGraph.h"
#include "../../PluginProcessor.h"
#include "../../DSP/EngineSidechainHelper.h"

#include <atomic>
#include <limits>

ClipPageTask::ClipPageTask (juce::AudioProcessor* engine,
                            int                   index,
                            int                   channelIdIn,
                            VibeGraph&            graph,
                            VibeSynthProcessor&   processor)
    : mEngine (engine),
      mScEngine (dynamic_cast<ISidechainEngine*> (engine)),
      mIndex (index),
      mGraph (&graph),
      mProcessor (&processor)
{
    this->channelId = channelIdIn;
}

void ClipPageTask::run()
{
    if (mEngine == nullptr || mOutputBuffer == nullptr || mCtx == nullptr
        || mGraph == nullptr || mProcessor == nullptr)
        return;

    const int n = mCtx->numSamples;
    if (n <= 0)
        return;

    // Per-block view over the arena slot's storage.
    float* const* ptrs = mOutputBuffer->getArrayOfWritePointers();
    juce::AudioBuffer<float> blockView (ptrs, mOutputBuffer->getNumChannels(), n);
    blockView.clear();

    // ── Sidechain push ────────────────────────────────────────────────────────
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

    // ── Engine + insert chain (Audio kind for clip pages) ─────────────────────
    juce::MidiBuffer  emptyMidi;
    juce::MidiBuffer* midi = (mCtx->clipPageMidi != nullptr)
                              ? &mCtx->clipPageMidi[mIndex]
                              : &emptyMidi;

    mEngine->processBlock (blockView, *midi);
    mGraph->processInsert (VibeGraph::InsertKind::Audio, mIndex,
                           blockView, mCtx->bpm, mCtx->anySolo);

    // ── Audio-row peak drain ──────────────────────────────────────────────────
    // Same drain-and-merge pattern as the serial ClipPage loop: pull the
    // InsertNode's L/R peaks, CAS-max into the audio-row mirrors so the
    // mixer's audio-row meters reflect clip-engine output alongside any
    // arrangement-playback audio on the same row.
    if (mIndex >= 0 && mIndex < VibeSynthProcessor::kMaxAudioRows)
    {
        const auto [pkL, pkR] = mGraph->drainInsertPeakDbStereo (
            VibeGraph::InsertKind::Audio, mIndex);

        auto casMax = [] (std::atomic<float>& a, float v) noexcept
        {
            if (v == -std::numeric_limits<float>::infinity()) return;
            float cur = a.load (std::memory_order_relaxed);
            while (cur < v
                   && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
        };

        casMax (mProcessor->mAudioRowPeakDbLRun[mIndex], pkL);
        casMax (mProcessor->mAudioRowPeakDbRRun[mIndex], pkR);
        casMax (mProcessor->mAudioRowPeakDbRun [mIndex], juce::jmax (pkL, pkR));
    }
}
