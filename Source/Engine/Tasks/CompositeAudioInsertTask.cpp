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

    // -- Flow A: clip-engine (sampler MIDI trigger) ---------------------
    // Mirrors the old ClipPageTask::run body.  Skipped when no engine is
    // assigned (no Clips ribbon tab for this row).
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

        // Engine writes directly into blockView (cleared above), then
        // processInsert applies the per-row chain in-place.
        clipEngine->processBlock (blockView, *midi);

        mGraph->processInsert (VibeGraph::InsertKind::Audio, mIndex,
                               blockView, mCtx->bpm, mCtx->anySolo);

        // QA-AudioMeters (2026-05-24): no per-flow drain needed -- processInsert
        // exchange-stores the InsertNode peak into VibeGraph's audioInsertPeakDb*
        // per-call, and consecutive processInsert calls within this task
        // (Flow A here + Flow B inside renderAudioClipsForRow below) both
        // contribute via that per-call store, so the published peak captures
        // the maximum across both flows.
    }

    // -- Flow B: arrangement-clip (timeline decode) ---------------------
    // Mirrors the old AudioInsertTask::run body.  Same gate as serial
    // Pass 2: song mode + playing + PatternManager exists.  When mtDest
    // is non-null, renderAudioClipsForRow ADDS each per-clip post-rack
    // output to mtDest -- accumulates on top of the engine-flow output
    // already in blockView.
    if (mCtx->posInfo == nullptr) return;
    if (! mCtx->posInfo->getIsPlaying()) return;
    if (mProcessor->mPatternManager == nullptr) return;
    if (! mProcessor->mSongMode.load (std::memory_order_relaxed)) return;

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

    mProcessor->renderAudioClipsForRow (mIndex, clipCtx, &blockView);
}
