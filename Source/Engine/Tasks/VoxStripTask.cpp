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

    // ── APVTS strip params ────────────────────────────────────────────────────
    // QA-Fb: hoisted above the FilePlay branch -- armed now selects between
    // pure playback and the capture path below.
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

    // 2026-05-06 (Batch 9b Item 9): FilePlay -- audio clips routed to this Vox
    // engine (player.routeChannel == channelId).  Gates mirror the pre-scan in
    // VibeSynthProcessor::processBlock (song mode + playing + pattern manager).
    // mCurrentBlockClipSnapshot is captured at the top of the audio callback
    // BEFORE the dispatcher fires this task and stays alive for the whole
    // block via the RetirementQueue ack protocol (Batch 9c B1).
    const bool filePlay = mIndex >= 0
        && mIndex < (int) mProcessor->mVoxFilePlayActive.size()
        && mProcessor->mVoxFilePlayActive[(size_t) mIndex]
        && mCtx->posInfo != nullptr
        && mCtx->posInfo->getIsPlaying()
        && mProcessor->mPatternManager != nullptr
        && mProcessor->mSongMode.load (std::memory_order_relaxed);

    // Shared FilePlay decode: sums every routed clip into mEngineScratch.
    // QA-MultiBlockHazard (Task 2): one decode sum per block so a stateful
    // engine + rack advance once per block, not once per FilePlay clip.
    // Used by the pure-playback branch AND the QA-Fb armed-monitor merge.
    VibeSynthProcessor::AudioClipBlockContext clipCtx;
    bool anyClip = false;
    auto decodeRoutedClips = [&]()
    {
        const double bpm        = mCtx->bpm;
        const double secPerBeat = 60.0 / juce::jmax (20.0, bpm);
        const double beatStart  = mCtx->posInfo->getPpqPosition().orFallback (0.0);
        // G1 smoke round 9 FIX: exact integer transport clock - the beat
        // round-trip truncation wobbled projectStart by +-1 sample (see
        // CompositeAudioInsertTask; same seam on Vox FilePlay).
        const juce::int64 projectStart = mCtx->posInfo->getTimeInSamples().orFallback (
            (juce::int64) (beatStart * secPerBeat * mProcessor->mSampleRate));

        const auto& mx = mProcessor->mPatternManager->getMixer();
        float masterGain = mx.masterLevel;
        if (auto* p = mProcessor->apvts.getRawParameterValue ("masterGain"))
            masterGain *= p->load();

        clipCtx.bpm          = bpm;
        clipCtx.anySolo      = mCtx->anySolo;
        clipCtx.secPerBeat   = secPerBeat;
        clipCtx.projectStart = projectStart;
        clipCtx.projectEnd   = projectStart + n;
        clipCtx.numSamples   = n;
        clipCtx.numOut       = blockView.getNumChannels();
        clipCtx.masterGain   = masterGain;
        clipCtx.mxState      = &mx;
        // QA-E Task 3 follow-up (2026-05-12): per-task clip scratch (was
        // shared mProcessor->mAudioClipScratch -- the documented race went
        // live once the Task 3 pre-scan fix activated MT FilePlay, producing
        // all-clips-mixed-into-every-strip cross-pollution on playback).
        // Must size before decodeFilePlayClip reads it.
        mClipScratch.setSize (blockView.getNumChannels(), n, false, false, true);
        clipCtx.clipScratch  = &mClipScratch;

        mEngineScratch.setSize (blockView.getNumChannels(), n, false, false, true);
        mEngineScratch.clear();
        for (auto& player : mProcessor->mCurrentBlockClipSnapshot->players)
        {
            if (player.routeChannel != channelId) continue;
            if (mProcessor->decodeFilePlayClip (player, clipCtx, mEngineScratch))
                anyClip = true;
        }
    };

    if (filePlay && ! active)
    {
        // Pure playback (nobody listening to live input): drive the engine +
        // insert chain ONCE on the decoded sum (finalizeFilePlayStrip ->
        // blockView).  QA-Fb #5 fold-in: the gate is !active (was !armed) so
        // a LIVE strip -- armed OR listen -- takes the monitor-merge path
        // below instead of losing its live input to this branch.
        // 2026-05-07 (Batch 9c follow-up): populate this strip's SC accumulator
        // so finalizeFilePlayStrip's processInsert sees real SC data.
        pullSidechainPredecessorsToGraph (*mGraph, channelId, mPredecessors, n);

        decodeRoutedClips();

        juce::MidiBuffer  emptyMidi;
        juce::MidiBuffer& engineMidi = (mCtx->voxPageMidi != nullptr)
            ? mCtx->voxPageMidi[mIndex] : emptyMidi;
        if (anyClip)
            mProcessor->finalizeFilePlayStrip (channelId, clipCtx, engineMidi, &blockView, mEngineScratch);
        return;
    }

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

    // ── QA-Fb Option A (locked 2026-07-10): live strip over FilePlay clips ───
    // Decode the prior takes now and hand them to the engine as the
    // block-scoped monitor merge: the engine sums them in AFTER its corrector
    // + WET tap (capture = live only) and BEFORE its rack, so chain + NAM
    // process the vocal stack exactly like post-stop playback.  muteLive
    // covers armed && !listen (captured, not monitored).
    if (filePlay)
    {
        decodeRoutedClips();
        if (mVocalEngine != nullptr)
            mVocalEngine->setMonitorMergeForThisBlock (anyClip ? &mEngineScratch : nullptr,
                                                       clipCtx.projectStart,
                                                       armed && ! listen);
    }

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

    // Fallback for a Vox slot whose engine isn't BaySickVocal (cast failed --
    // shouldn't happen): mimic the merge post-engine so takes stay audible.
    if (filePlay && mVocalEngine == nullptr)
    {
        if (armed && ! listen)
            blockView.clear();
        if (anyClip)
        {
            const int nc = juce::jmin (blockView.getNumChannels(),
                                       mEngineScratch.getNumChannels());
            for (int c = 0; c < nc; ++c)
                blockView.addFrom (c, 0, mEngineScratch, c, 0, n);
        }
    }

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
    // QA-Fb: overlap blocks skip this -- the engine's muteLive already
    // removed the live stream from the monitor, and clearing here would kill
    // the merged prior takes.
    if (! filePlay && armed && ! listen)
        blockView.clear();
}
