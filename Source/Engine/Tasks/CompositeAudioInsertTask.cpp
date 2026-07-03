#include "CompositeAudioInsertTask.h"
#include "../../PluginProcessor.h"
#include "../../PatternManager.h"
#include "../../VibeGraph.h"
#include "../../DSP/EngineSidechainHelper.h"
#include "../SidechainPullHelper.h"

#include <atomic>
#include <limits>

CompositeAudioInsertTask::CompositeAudioInsertTask (int                 row,
                                                    int                 channelIdIn,
                                                    VibeGraph&          graph,
                                                    VibeSynthProcessor& processor)
    : mIndex (row),
      mGraph (&graph),
      mProcessor (&processor)
{
    this->channelId = channelIdIn;
}

void CompositeAudioInsertTask::setClipEngine (juce::AudioProcessor* engine)
{
    mClipEngine.store (engine, std::memory_order_release);
    mScEngine .store (dynamic_cast<ISidechainEngine*> (engine),
                      std::memory_order_release);
}

juce::AudioBuffer<float>& CompositeAudioInsertTask::getClipScratch (int numChannels,
                                                                    int numSamples)
{
    if (mClipScratch.getNumChannels() != numChannels
        || mClipScratch.getNumSamples() < numSamples)
    {
        mClipScratch.setSize (numChannels, numSamples,
                              /*keepExistingContent*/ false,
                              /*clearExtraSpace*/    false,
                              /*avoidReallocating*/  true);
    }
    return mClipScratch;
}

void CompositeAudioInsertTask::run()
{
    if (mProcessor == nullptr || mGraph == nullptr
        || mOutputBuffer == nullptr || mCtx == nullptr)
        return;

    const int n = mCtx->numSamples;
    if (n <= 0)
        return;

    // Per-block view over the arena slot.  Cleared once; both flows
    // accumulate.
    float* const* ptrs = mOutputBuffer->getArrayOfWritePointers();
    juce::AudioBuffer<float> blockView (ptrs, mOutputBuffer->getNumChannels(), n);
    blockView.clear();

    // SC accumulator population for the row's rack / preEq / postEq.
    // Done once; both flows share the same SC inputs since they target
    // the same row InsertNode.
    pullSidechainPredecessorsToGraph (*mGraph, channelId, mPredecessors, n);

    // QA-MultiBlockHazard (Task 1): both flows sum their RAW output into
    // blockView; the insert chain (processInsert) runs exactly ONCE per block on
    // the summed buffer below, so a stateful rack advances once per block instead
    // of once per source.  anySource preserves the old gating -- the chain runs
    // iff >=1 source contributed (N->1 calls, never 0->1).
    bool anySource = false;

    // -- Flow A: clip-engine (sampler MIDI trigger) ---------------------
    // Skipped when no engine is assigned (no Clips ribbon tab for this row).
    juce::AudioProcessor* clipEngine
        = mClipEngine.load (std::memory_order_acquire);
    if (clipEngine != nullptr)
    {
        // Push SC buffers via setSidechainBuffers (same as old ClipPageTask).
        if (auto* sc = mScEngine.load (std::memory_order_acquire))
        {
            juce::AudioBuffer<float>* scBufs[VibeGraph::kMaxScRecvSlots] = {};
            for (const auto& link : mPredecessors)
            {
                if (! link.isSc) continue;
                if (link.scSlot < 0 || link.scSlot >= VibeGraph::kMaxScRecvSlots) continue;
                if (link.source == nullptr || link.source->mOutputBuffer == nullptr) continue;
                scBufs[link.scSlot] = link.source->mOutputBuffer;
            }
            sc->setSidechainBuffers (scBufs, VibeGraph::kMaxScRecvSlots);
        }

        juce::MidiBuffer  emptyMidi;
        juce::MidiBuffer* midi = (mCtx->clipPageMidi != nullptr)
                                  ? &mCtx->clipPageMidi[mIndex]
                                  : &emptyMidi;

        // Engine writes its RAW output directly into blockView (cleared above);
        // the insert chain is applied once, after Flow B, below.
        clipEngine->processBlock (blockView, *midi);
        anySource = true;
    }

    // -- Flow B: arrangement-clip (timeline decode) ---------------------
    // Same gate as serial Pass 2: song mode + playing + PatternManager exists.
    // renderAudioClipsForRow ADDS each clip's RAW decoded output on top of the
    // engine-flow output already in blockView.  Guarded block (not early-return)
    // so the single processInsert below still runs in the Flow-A-only case.
    if (mCtx->posInfo != nullptr
        && mCtx->posInfo->getIsPlaying()
        && mProcessor->mPatternManager != nullptr
        && mProcessor->mSongMode.load (std::memory_order_relaxed))
    {
        const double bpm        = mCtx->bpm;
        const double secPerBeat = 60.0 / juce::jmax (20.0, bpm);
        const double beatStart  = mCtx->posInfo->getPpqPosition().orFallback (0.0);

        const juce::int64 projectStart = (juce::int64) (beatStart * secPerBeat * mProcessor->mSampleRate);
        const juce::int64 projectEnd   = projectStart + n;

        const auto& mx = mProcessor->mPatternManager->getMixer();
        float masterGain = mx.masterLevel;
        if (auto* p = mProcessor->apvts.getRawParameterValue ("masterGain"))
            masterGain *= p->load();

        VibeSynthProcessor::AudioClipBlockContext clipCtx;
        clipCtx.bpm           = bpm;
        clipCtx.anySolo       = mCtx->anySolo;
        clipCtx.secPerBeat    = secPerBeat;
        clipCtx.projectStart  = projectStart;
        clipCtx.projectEnd    = projectEnd;
        clipCtx.numSamples    = n;
        clipCtx.numOut        = blockView.getNumChannels();
        clipCtx.masterGain    = masterGain;
        clipCtx.mxState       = &mx;
        clipCtx.clipScratch   = &getClipScratch (blockView.getNumChannels(), n);

        if (mProcessor->renderAudioClipsForRow (mIndex, clipCtx, &blockView))
            anySource = true;
    }

    // -- Single insert-chain pass on the summed sources ----------------
    // polarity -> preEq -> width -> rack -> postEq -> fader x mute x solo ->
    // PDC -> peak.  One processInsert per block now (was per-flow + per-clip);
    // see the CAS-max note at VibeGraph::processInsert.
    if (anySource)
        mGraph->processInsert (VibeGraph::InsertKind::Audio, mIndex,
                               blockView, mCtx->bpm, mCtx->anySolo);
}
