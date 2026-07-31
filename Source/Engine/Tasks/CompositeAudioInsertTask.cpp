#include "CompositeAudioInsertTask.h"
#include "../FrozenSourceRead.h"   // TS7 6.8: scope-matched frozen block read
#include "../../PluginProcessor.h"
#include "../../PatternManager.h"
#include "../../VibeGraph.h"
#include "../../DSP/EngineSidechainHelper.h"
#include "../../VibePlayer/VibePlayerProcessor.h"   // QA-ClipPlayback Task 2: complete type for the dynamic_cast in setClipEngine
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
    mClipPlayer.store (dynamic_cast<VibePlayerProcessor*> (engine),
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

    // ── TS7 §6.9: frozen Clips row (Jeff, 2026-07-30) ────────────────────────
    // Substitutes BOTH flows at once, which is the point: this task owns the
    // arrangement-clip decode AND the clip-engine MIDI trigger, so a frozen row
    // skips the BaySickPlayer engine as well as the decode.  Freezing a Clips
    // page is not "rendering a file to copy a file" -- the engine behind that
    // file runs every block, and that is the cost being reclaimed.
    // §6.8 scope-matched: the song render at the absolute playhead, or this
    // pattern's own render at a loop-local one.  See FrozenSourceRead.h.
    // Falls through to live decode + engine when neither can serve this block --
    // §6.6's stale-plays-live rule.
    if (FreezeRead::serveBlock (*this, *mCtx, blockView, n))
    {
        mGraph->processInsert (VibeGraph::InsertKind::Audio, mIndex,
                               blockView, mCtx->bpm, mCtx->anySolo);
        return;
    }

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
        // QA-Fe2 SC delay-match: hand the engine the SAME aligned receive
        // buffers the pull above filled + delay-matched (pre-compensation
        // source taps), so engine SC and rack/EQ SC read identical keys.
        if (auto* sc = mScEngine.load (std::memory_order_acquire))
        {
            const VibeGraph::ScRecvArray scArr = mGraph->getScRecvArray (channelId);
            juce::AudioBuffer<float>* scBufs[VibeGraph::kMaxScRecvSlots] = {};
            for (int i = 0; i < VibeGraph::kMaxScRecvSlots; ++i)
                scBufs[i] = scArr[(size_t) i];
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

        // G1 smoke round 9 FIX (the fizz): use the transport's EXACT integer
        // sample clock.  The old beat round-trip (integer clock -> ppq double
        // -> truncating cast back) landed +-1 sample on FP rounding luck, so
        // projectStart wobbled block-to-block and every clip rendered a
        // one-sample seam wherever it flipped (trace-proven: paired +-1
        // steps).  Beat math stays for hosts that don't supply samples.
        const juce::int64 projectStart = mCtx->posInfo->getTimeInSamples().orFallback (
            (juce::int64) (beatStart * secPerBeat * mProcessor->mSampleRate));
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
        clipCtx.clipPlayer    = mClipPlayer.load (std::memory_order_acquire);

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
