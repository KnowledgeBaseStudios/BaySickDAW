#include "VoxStripTask.h"
#include "../../VibeGraph.h"
#include "../../PluginProcessor.h"
#include "../../DSP/EngineSidechainHelper.h"
#include "../../BaySickVocal/BaySickVocalProcessor.h"
#include "../SidechainPullHelper.h"   // pullSidechainPredecessorsToGraph

VoxStripTask::VoxStripTask (juce::AudioProcessor* engine,
                            int                   index,
                            int                   channelIdIn,
                            VibeGraph&            graph,
                            VibeSynthProcessor&   processor)
    : mEngine (engine),
      mScEngine (dynamic_cast<ISidechainEngine*> (engine)),
      mVocalEngine (dynamic_cast<BaySickVocalProcessor*> (engine)),
      mIndex (index),
      mGraph (&graph),
      mProcessor (&processor),
      mPrefix ("mixer_vox_" + juce::String (index))
{
    this->channelId = channelIdIn;
}

void VoxStripTask::run()
{
    if (mEngine == nullptr || mOutputBuffer == nullptr || mCtx == nullptr
        || mGraph == nullptr || mProcessor == nullptr)
        return;

    const int n = mCtx->numSamples;
    if (n <= 0)
        return;

    // Build a "this-block" view of the arena slot's storage at the host's
    // current numSamples, then start by zeroing the output region.
    float* const* ptrs = mOutputBuffer->getArrayOfWritePointers();
    juce::AudioBuffer<float> blockView (ptrs, mOutputBuffer->getNumChannels(), n);
    blockView.clear();

    // 2026-05-06 (Batch 9b Item 9): FilePlay branch.  When audio clips are
    // routed to this Vox engine (player.routeChannel == channelId),
    // QA-MultiBlockHazard (Task 2) decodes every routed clip into a per-task
    // sum (decodeFilePlayClip) then drives the engine + insert chain ONCE on
    // the sum (finalizeFilePlayStrip -> blockView).  The live-input path below
    // is skipped entirely.
    if (mIndex >= 0 && mIndex < (int) mProcessor->mVoxFilePlayActive.size()
        && mProcessor->mVoxFilePlayActive[(size_t) mIndex])
    {
        // Gate on song mode + playing + pattern manager (matches serial Pass 1).
        if (mCtx->posInfo == nullptr) return;
        if (! mCtx->posInfo->getIsPlaying()) return;
        if (mProcessor->mPatternManager == nullptr) return;
        if (! mProcessor->mSongMode.load (std::memory_order_relaxed)) return;

        // 2026-05-06 (Batch 9c B1): try-lock removed.  mCurrentBlockClipSnapshot
        // is captured by VibeSynthProcessor::processBlock at the top of the
        // audio callback BEFORE the dispatcher fires this task; the snapshot
        // is guaranteed alive for the entire block via the RetirementQueue
        // ack protocol.  Visibility is established by the dispatcher's
        // notify/wait release-acquire pair.
        const double bpm        = mCtx->bpm;
        const double secPerBeat = 60.0 / juce::jmax (20.0, bpm);
        const double beatStart  = mCtx->posInfo->getPpqPosition().orFallback (0.0);
        // G1 smoke round 9 FIX: exact integer transport clock - the beat
        // round-trip truncation wobbled projectStart by +-1 sample (see
        // CompositeAudioInsertTask; same seam on Vox FilePlay).
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
        // shared mProcessor->mAudioClipScratch -- the documented race went
        // live once the Task 3 pre-scan fix activated MT FilePlay, producing
        // all-clips-mixed-into-every-strip cross-pollution on playback).
        // Must size before decodeFilePlayClip reads it; the function
        // assumes the caller sized clipScratch (serial Pass 1 does so via
        // mAudioClipScratch at processBlock top).
        mClipScratch.setSize (blockView.getNumChannels(), n, false, false, true);
        clipCtx.clipScratch  = &mClipScratch;

        juce::MidiBuffer  emptyMidi;
        juce::MidiBuffer& engineMidi = (mCtx->voxPageMidi != nullptr)
            ? mCtx->voxPageMidi[mIndex] : emptyMidi;

        // 2026-05-07 (Batch 9c follow-up): populate this strip's SC accumulator
        // so finalizeFilePlayStrip's processInsert sees real SC data.
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
        return;   // FilePlay handled; skip live-input + engine branch below
    }

    // ── APVTS strip params ────────────────────────────────────────────────────
    auto& apvts = mProcessor->apvts;
    const auto* armP    = apvts.getRawParameterValue (mPrefix + "_arm");
    const auto* idxP    = apvts.getRawParameterValue (mPrefix + "_inputChannelIdx");
    const auto* stereoP = apvts.getRawParameterValue (mPrefix + "_inputChannelStereo");
    const auto* listenP = apvts.getRawParameterValue (mPrefix + "_listen");

    const int  chIdx    = (idxP != nullptr) ? (int) idxP->load() : -1;
    const bool isStereo = (stereoP != nullptr) && stereoP->load() > 0.5f;

    const auto* snapshot = mCtx->liveInputSnapshot;
    const int   snapChs  = (snapshot != nullptr) ? snapshot->getNumChannels() : 0;

    const bool channelOK = (chIdx >= 0 && chIdx < snapChs);
    const bool armed     = (armP    != nullptr) && armP   ->load() > 0.5f && channelOK;
    const bool listen    = (listenP != nullptr) && listenP->load() > 0.5f;
    // QA-E Task 5 (2026-05-15): live input flows through the chain whenever
    // either arm OR listen is engaged (with a channel selected).  Prior
    // behavior gated on armed-only, making "monitor without recording"
    // impossible -- surfaced by master-mix-fallback test scenario.
    const bool active    = channelOK && (armed || listen);

    // ── Live-input copy (active) ──────────────────────────────────────────────
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

    // ── Live-mode clears the wet-recorder pitch bypass ───────────────────────
    if (mVocalEngine != nullptr)
        mVocalEngine->setForcePitchBypass (false);

    // ── Sidechain push (pull from SC predecessor outputs) ─────────────────────
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
    juce::MidiBuffer* midi = (mCtx->voxPageMidi != nullptr)
                              ? &mCtx->voxPageMidi[mIndex]
                              : &emptyMidi;

    mEngine->processBlock (blockView, *midi);

    // 2026-05-07 (Batch 9c follow-up): SC accumulator population (live-input
    // branch).  Mirrors the FilePlay branch above + the EngineInsertTask
    // pattern.
    pullSidechainPredecessorsToGraph (*mGraph, channelId, mPredecessors, n);

    mGraph->processInsert (VibeGraph::InsertKind::Vox, mIndex,
                           blockView, mCtx->bpm, mCtx->anySolo);

    // ── Listen gate ───────────────────────────────────────────────────────────
    // Armed && !listen -> kill output (don't route to the bus).  Unarmed
    // pages always route (engine may produce its own audio from MIDI;
    // unarmed-listen-on routes the live input through naturally because the
    // input was copied into blockView by the `active` branch above).
    if (armed && ! listen)
        blockView.clear();
}
