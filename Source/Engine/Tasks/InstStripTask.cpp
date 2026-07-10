#include "InstStripTask.h"
#include "../../VibeGraph.h"
#include "../../PluginProcessor.h"
#include "../../DSP/EngineSidechainHelper.h"
#include "../../BaySickGuitars/BaySickGuitarsProcessor.h"   // getNumActiveVoices
#include "../../BaySickBasses/BaySickBassesProcessor.h"     // getNumActiveVoices
#include "../SidechainPullHelper.h"   // pullSidechainPredecessorsToGraph

InstStripTask::InstStripTask (juce::AudioProcessor* engine,
                              int                   index,
                              int                   channelIdIn,
                              VibeGraph&            graph,
                              VibeSynthProcessor&   processor)
    : mEngine (engine),
      mScEngine (dynamic_cast<ISidechainEngine*> (engine)),
      mIndex (index),
      mGraph (&graph),
      mProcessor (&processor),
      mPrefix ("mixer_inst_" + juce::String (index))
{
    this->channelId = channelIdIn;
}

void InstStripTask::run()
{
    if (mEngine == nullptr || mOutputBuffer == nullptr || mCtx == nullptr
        || mGraph == nullptr || mProcessor == nullptr)
        return;

    const int n = mCtx->numSamples;
    if (n <= 0)
        return;

    // Build per-block view.
    float* const* ptrs = mOutputBuffer->getArrayOfWritePointers();
    juce::AudioBuffer<float> blockView (ptrs, mOutputBuffer->getNumChannels(), n);
    blockView.clear();

    // 2026-05-06 (Batch 9b Item 9): FilePlay branch - see VoxStripTask::run for
    // rationale.  QA-MultiBlockHazard (Task 2): audio clips routed to this Inst
    // engine are decoded into a per-task sum (decodeFilePlayClip) then driven
    // through the engine + insert chain ONCE (finalizeFilePlayStrip).
    if (mIndex >= 0 && mIndex < (int) mProcessor->mInstFilePlayActive.size()
        && mProcessor->mInstFilePlayActive[(size_t) mIndex])
    {
        if (mCtx->posInfo == nullptr) return;
        if (! mCtx->posInfo->getIsPlaying()) return;
        if (mProcessor->mPatternManager == nullptr) return;
        if (! mProcessor->mSongMode.load (std::memory_order_relaxed)) return;

        // 2026-05-06 (Batch 9c B1): try-lock removed.  mCurrentBlockClipSnapshot
        // is captured by VibeSynthProcessor::processBlock at the top of the
        // audio callback BEFORE the dispatcher fires this task.
        const double bpm        = mCtx->bpm;
        const double secPerBeat = 60.0 / juce::jmax (20.0, bpm);
        const double beatStart  = mCtx->posInfo->getPpqPosition().orFallback (0.0);
        // G1 smoke round 9 FIX: exact integer transport clock - the beat
        // round-trip truncation wobbled projectStart by +-1 sample (see
        // CompositeAudioInsertTask; same seam on Inst FilePlay).
        const juce::int64 projectStart = mCtx->posInfo->getTimeInSamples().orFallback (
            (juce::int64) (beatStart * secPerBeat * mProcessor->mSampleRate));
        const juce::int64 projectEnd   = projectStart + n;

        const auto& mx = mProcessor->mPatternManager->getMixer();
        float masterGain = mx.masterLevel;
        if (auto* p = mProcessor->apvts.getRawParameterValue ("masterGain"))
            masterGain *= p->load();

        VibeSynthProcessor::AudioClipBlockContext clipCtx;
        clipCtx.bpm          = bpm;
        clipCtx.anySolo      = mCtx->anySolo;
        clipCtx.secPerBeat   = secPerBeat;
        clipCtx.projectStart = projectStart;
        clipCtx.projectEnd   = projectEnd;
        clipCtx.numSamples   = n;
        clipCtx.numOut       = blockView.getNumChannels();
        clipCtx.masterGain   = masterGain;
        clipCtx.mxState      = &mx;
        // QA-E Task 3 follow-up (2026-05-12): per-task clip scratch (was
        // shared mProcessor->mAudioClipScratch).  See VoxStripTask.cpp for
        // the cross-pollution rationale + the size-before-read requirement.
        mClipScratch.setSize (blockView.getNumChannels(), n, false, false, true);
        clipCtx.clipScratch  = &mClipScratch;

        juce::MidiBuffer  emptyMidi;
        juce::MidiBuffer& engineMidi = (mCtx->instPageMidi != nullptr)
            ? mCtx->instPageMidi[mIndex] : emptyMidi;

        // 2026-05-07 (Batch 9c follow-up): SC accumulator population so
        // finalizeFilePlayStrip's processInsert sees real SC data.
        pullSidechainPredecessorsToGraph (*mGraph, channelId, mPredecessors, n);

        // 2026-05-06 (Batch 9c B1): iterate the audio-thread snapshot.
        // QA-MultiBlockHazard (Task 2): decode every routed clip into the
        // per-task engine-sum buffer, then run the engine + insert chain ONCE on
        // the sum -- so a stateful engine + rack advance once per block, not once
        // per FilePlay clip.
        mEngineScratch.setSize (blockView.getNumChannels(), n, false, false, true);
        mEngineScratch.clear();
        bool anyClip = false;
        for (auto& player : mProcessor->mCurrentBlockClipSnapshot->players)
        {
            if (player.routeChannel != channelId) continue;
            if (mProcessor->decodeFilePlayClip (player, clipCtx, mEngineScratch))
                anyClip = true;
        }
        if (anyClip)
            mProcessor->finalizeFilePlayStrip (channelId, clipCtx, engineMidi, &blockView, mEngineScratch);
        return;
    }

    // ── Source-mode detection ────────────────────────────────────────────────
    const bool guitarsActive = mProcessor->mGuitarsActive[(size_t) mIndex]
                                          .load (std::memory_order_acquire);
    const bool bassesActive  = mProcessor->mBassesActive [(size_t) mIndex]
                                          .load (std::memory_order_acquire);
    const bool sfizzActive   = guitarsActive || bassesActive;

    // ── Idle suspend (sfizz-source only) ─────────────────────────────────────
    // When the tab has no MIDI activity AND sfizz reports 0 active voices,
    // skip the entire chain.  Wakes immediately on next block where any
    // gate fails.  Live-input Inst tabs are NOT suspended (live audio +
    // arm/listen can fire even with no MIDI).
    if (sfizzActive)
    {
        int activeVoices = 0;
        if (auto* g = mProcessor->getBaySickGuitars (mIndex))
            activeVoices = g->getNumActiveVoices();
        if (activeVoices == 0)
            if (auto* b = mProcessor->getBaySickBasses (mIndex))
                activeVoices = b->getNumActiveVoices();

        const bool midiEmpty = mCtx->instPageMidi != nullptr
                            && mCtx->instPageMidi[mIndex].getNumEvents() == 0;
        const bool noVoices  = activeVoices == 0;

        // QA-C DSP-10 (2026-05-10): peek audition state from any sfizz engine
        // bound to this Inst index.  An audition queued via auditionNote(int)
        // must wake the chain so processBlock can mAuditionNote.exchange(-1)
        // and produce sound.  Without this, audition during idle-suspend is
        // silently swallowed.
        bool auditionPending = false;
        if (auto* g = mProcessor->getBaySickGuitars (mIndex))
            auditionPending = g->isAuditionPending();
        if (! auditionPending)
            if (auto* b = mProcessor->getBaySickBasses (mIndex))
                auditionPending = b->isAuditionPending();

        if (midiEmpty && noVoices && ! auditionPending)
        {
            auto& counter = mProcessor->mInstIdleBlocks[(size_t) mIndex];
            if (counter >= VibeSynthProcessor::kIdleSuspendBlocks)
            {
                mOutputBuffer->clear();   // suspended this block
                return;
            }
            ++counter;
        }
        else
        {
            mProcessor->mInstIdleBlocks[(size_t) mIndex] = 0;   // wake
        }
    }

    // ── APVTS strip params (live-input mode) ─────────────────────────────────
    auto& apvts = mProcessor->apvts;
    const auto* armP    = apvts.getRawParameterValue (mPrefix + "_arm");
    const auto* idxP    = apvts.getRawParameterValue (mPrefix + "_inputChannelIdx");
    const auto* stereoP = apvts.getRawParameterValue (mPrefix + "_inputChannelStereo");
    const auto* listenP = apvts.getRawParameterValue (mPrefix + "_listen");

    const int  chIdx    = (idxP != nullptr) ? (int) idxP->load() : -1;
    const bool isStereo = (stereoP != nullptr) && stereoP->load() > 0.5f;

    const auto* snapshot = mCtx->liveInputSnapshot;
    const int   snapChs  = (snapshot != nullptr) ? snapshot->getNumChannels() : 0;

    // sfizz-active slots ignore arm / listen - sfizz is the source, no live input.
    const bool channelOK = (chIdx >= 0 && chIdx < snapChs);
    const bool armed     = ! sfizzActive
                        && (armP    != nullptr) && armP   ->load() > 0.5f && channelOK;
    const bool listen    = ! sfizzActive
                        && (listenP != nullptr) && listenP->load() > 0.5f;
    // QA-E Task 5 (2026-05-15): live input flows through the chain whenever
    // either arm OR listen is engaged (with a channel selected).  Prior
    // behavior gated on armed-only, making "monitor without recording"
    // impossible.
    const bool active    = channelOK && (armed || listen);

    if (active)
    {
        // 2026-05-06 (Batch 9b Item 8): dry-recorder tap (RAW pre-chain mono)
        // - captured here so the recorded file is the unprocessed DI; chain
        // runs ONCE on the dry source.  Only fire when ARMED (monitor-only
        // mode produces no recording).  Pre-existing race risk between this
        // read of mStripRecorders and message-thread mutation in startRecording
        // / stopRecording is documented at the helper site; not closed.
        if (armed)
            mProcessor->tapDryRecorder (channelId,
                                         snapshot->getReadPointer (chIdx),
                                         n);

        const int rightCh = (isStereo && chIdx + 1 < snapChs) ? (chIdx + 1) : chIdx;
        if (blockView.getNumChannels() > 0)
            blockView.copyFrom (0, 0, *snapshot, chIdx,   0, n);
        if (blockView.getNumChannels() > 1)
            blockView.copyFrom (1, 0, *snapshot, rightCh, 0, n);
    }

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

    // ── Engine + insert chain ─────────────────────────────────────────────────
    juce::MidiBuffer  emptyMidi;
    juce::MidiBuffer* midi = (mCtx->instPageMidi != nullptr)
                              ? &mCtx->instPageMidi[mIndex]
                              : &emptyMidi;

    mEngine->processBlock (blockView, *midi);

    // 2026-05-07 (Batch 9c follow-up): SC accumulator population (live-input
    // / sfizz branch).
    pullSidechainPredecessorsToGraph (*mGraph, channelId, mPredecessors, n);

    mGraph->processInsert (VibeGraph::InsertKind::Inst, mIndex,
                           blockView, mCtx->bpm, mCtx->anySolo);

    // ── Listen gate ───────────────────────────────────────────────────────────
    // sfizz-source: always route. Armed live + !listen: kill output.
    // Unarmed + listen: route naturally (input was copied into blockView above).
    // Unarmed + !listen: silent strip, nothing to clear.
    if (armed && ! listen)
        blockView.clear();
}
