#include "InstStripTask.h"
#include "../FrozenSourceRead.h"   // TS7 6.8: scope-matched frozen block read
#include "../ChannelBufferArena.h"   // kChannelsPerStrip (scratch pre-size)
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

    // Scratch buffers are grown to a whole arena-sized block HERE, on the
    // message thread.  run() executes on pool workers, and its per-block setSize
    // calls only skip the allocator while allocatedBytes already covers the
    // request (juce::AudioBuffer::setSize, avoidReallocating branch) -- from a
    // 0x0 buffer the first block that serves a recorded take, a freeze file or a
    // Dry monitor flip would malloc on the render path instead.
    const int maxBlock = juce::jmax (1, processor.mBlockSize);
    mClipScratch   .setSize (ChannelBufferArena::kChannelsPerStrip, maxBlock, false, true, false);
    mEngineScratch .setSize (ChannelBufferArena::kChannelsPerStrip, maxBlock, false, true, false);
    mMonitorDryBuf .setSize (ChannelBufferArena::kChannelsPerStrip, maxBlock, false, true, false);
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

    // ── Source-mode detection ────────────────────────────────────────────────
    const bool guitarsActive = mProcessor->mGuitarsActive[(size_t) mIndex]
                                          .load (std::memory_order_acquire);
    const bool bassesActive  = mProcessor->mBassesActive [(size_t) mIndex]
                                          .load (std::memory_order_acquire);
    const bool sfizzActive   = guitarsActive || bassesActive;

    // ── APVTS strip params ────────────────────────────────────────────────────
    // QA-Fb: hoisted above the FilePlay branch -- active now selects between
    // pure playback and the capture/monitor path below.  sfizz-active slots
    // ignore arm / listen - sfizz is the source, no live input.
    // run() is on a pool worker (and on the audio callback itself via the
    // main-as-worker pump), so building each id with juce::String every block is
    // a heap allocation per param per strip.  Resolve once, then read through
    // the cached pointers; an unresolved strip simply retries next block.
    auto& apvts = mProcessor->apvts;
    if (mArmP    == nullptr) mArmP    = apvts.getRawParameterValue (mPrefix + "_arm");
    if (mIdxP    == nullptr) mIdxP    = apvts.getRawParameterValue (mPrefix + "_inputChannelIdx");
    if (mStereoP == nullptr) mStereoP = apvts.getRawParameterValue (mPrefix + "_inputChannelStereo");
    if (mListenP == nullptr) mListenP = apvts.getRawParameterValue (mPrefix + "_listen");
    // QA-OctavePedal Task 5: live-monitor mode (0 = Dry raw input, 1 = With
    // Effect processed; default 1 = With Effect per Jeff 2026-07-18).  A
    // monitor-only fork applied around the engine render below.
    if (mMonModeP == nullptr) mMonModeP = apvts.getRawParameterValue (mPrefix + "_monitorMode");

    const auto* armP        = mArmP;
    const auto* idxP        = mIdxP;
    const auto* stereoP     = mStereoP;
    const auto* listenP     = mListenP;
    const auto* monModeP    = mMonModeP;
    const int   monitorMode = (monModeP != nullptr) ? (int) monModeP->load() : 1;

    const int  chIdx    = (idxP != nullptr) ? (int) idxP->load() : -1;
    const bool isStereo = (stereoP != nullptr) && stereoP->load() > 0.5f;

    const auto* snapshot = mCtx->liveInputSnapshot;
    const int   snapChs  = (snapshot != nullptr) ? snapshot->getNumChannels() : 0;

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

    // 2026-05-06 (Batch 9b Item 9): FilePlay -- audio clips routed to this
    // Inst engine.  Gates mirror the pre-scan in VibeSynthProcessor::
    // processBlock; mCurrentBlockClipSnapshot is block-stable via the
    // RetirementQueue ack protocol (Batch 9c B1).
    const bool filePlay = mIndex >= 0
        && mIndex < (int) mProcessor->mInstFilePlayActive.size()
        && mProcessor->mInstFilePlayActive[(size_t) mIndex]
        && mCtx->posInfo != nullptr
        && mCtx->posInfo->getIsPlaying()
        && mProcessor->mPatternManager != nullptr
        && mProcessor->mSongMode.load (std::memory_order_relaxed);

    // Shared FilePlay decode: sums every routed clip into mEngineScratch.
    // QA-MultiBlockHazard (Task 2): one decode sum per block so a stateful
    // engine + rack advance once per block, not once per FilePlay clip.
    VibeSynthProcessor::AudioClipBlockContext clipCtx;
    bool anyClip = false;
    auto decodeRoutedClips = [&]()
    {
        const double bpm        = mCtx->bpm;
        const double secPerBeat = 60.0 / juce::jmax (20.0, bpm);
        const double beatStart  = mCtx->posInfo->getPpqPosition().orFallback (0.0);
        // G1 smoke round 9 FIX: exact integer transport clock - the beat
        // round-trip truncation wobbled projectStart by +-1 sample (see
        // CompositeAudioInsertTask; same seam on Inst FilePlay).
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
        // QA-E Task 3 follow-up (2026-05-12): per-task clip scratch -- see
        // VoxStripTask.cpp for the cross-pollution rationale + the
        // size-before-read requirement.
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

    // ── TS7 §6.9: frozen playback (Jeff, 2026-07-30) ─────────────────────────
    // Tested before the route split for the same reason as Vox: the playback
    // route only fires when a clip overlaps THIS block, so substituting inside
    // it would leave the chain (pedals + NAM/IR, the expensive stage) running
    // over silence on every other block and give back almost nothing.
    //
    // GATED ON !active -- Jeff's (a): freeze STANDS ASIDE the moment the strip
    // is armed or monitoring.  It has to.  Unlike Vox, this task merges the
    // recorded takes into the live buffer BEFORE the engine ("QA-Fb Option A",
    // locked 2026-07-10, so live and prior takes get one identical chain pass),
    // and frozen audio already carries that chain -- joining it there would
    // process it twice.  Standing aside costs nothing: the strip simply plays
    // live exactly as it does today, which is freeze's existing fall-back
    // behaviour rather than a special case invented here.
    // §6.8 scope-matched (song render, or this pattern's own render at a
    // loop-local position).  Still gated on `! active` for the reason above.
    if (! active && FreezeRead::serveBlock (*this, *mCtx, blockView, n))
    {
        pullSidechainPredecessorsToGraph (*mGraph, channelId, mPredecessors, n);
        mGraph->processInsert (VibeGraph::InsertKind::Inst, mIndex,
                               blockView, mCtx->bpm, mCtx->anySolo);
        return;
    }

    if (filePlay && ! active)
    {
        // Pure playback (nobody listening to live input): drive the engine +
        // insert chain ONCE on the decoded sum (finalizeFilePlayStrip ->
        // blockView).  QA-Fb #5 fold-in: the gate is !active so a LIVE strip
        // -- armed OR listen -- takes the pre-engine merge path below instead
        // of losing its live input to this branch.  sfizz strips always land
        // here when clips overlap (arm/listen forced off above).
        pullSidechainPredecessorsToGraph (*mGraph, channelId, mPredecessors, n);

        decodeRoutedClips();

        juce::MidiBuffer  emptyMidi;
        juce::MidiBuffer& engineMidi = (mCtx->instPageMidi != nullptr)
            ? mCtx->instPageMidi[mIndex] : emptyMidi;
        if (anyClip)
            mProcessor->finalizeFilePlayStrip (channelId, clipCtx, engineMidi, &blockView, mEngineScratch);
        return;
    }

    // ── Idle suspend (sfizz-source only) ─────────────────────────────────────
    // When the tab has no MIDI activity AND sfizz reports 0 active voices,
    // stop rendering the whole chain.  Wakes on the next block where any gate
    // fails.  Live-input Inst tabs are NOT suspended (live audio + arm/listen
    // can fire even with no MIDI).
    //
    // The skip takes out the insert chain as well as the engine, so it cannot
    // be instantaneous: whatever the rack / NAM cab is still ringing would be
    // cut off in one sample.  The hold expiring starts a fade instead, the
    // chain keeps running at falling gain for its duration, and only then does
    // the cheap cleared path take over (IdleSuspendFade.h).  Stays unity on
    // every non-sfizz strip, where isUnity() below skips the ramp outright.
    IdleSuspendFade::Ramp suspendRamp;
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

        // QA-G3Smoke Task 12: SlideSampler voices (gesture + ring-out tails)
        // don't count in sfizz's active-voice total -- without this term the
        // chain suspends mid-slide and the tail freezes.
        bool slideActive = false;
        if (auto* g = mProcessor->getBaySickGuitars (mIndex))
            slideActive = g->isSlideActive();
        if (! slideActive)
            if (auto* b = mProcessor->getBaySickBasses (mIndex))
                slideActive = b->isSlideActive();

        bool holdExpired = false;
        if (midiEmpty && noVoices && ! auditionPending && ! slideActive)
        {
            auto& counter = mProcessor->mInstIdleBlocks[(size_t) mIndex];
            if (counter >= VibeSynthProcessor::kIdleSuspendBlocks)
                holdExpired = true;
            else
                ++counter;
        }
        else
        {
            mProcessor->mInstIdleBlocks[(size_t) mIndex] = 0;   // wake
        }

        suspendRamp = mSuspendFade.advance (holdExpired, n, mProcessor->mSampleRate);
        if (suspendRamp.suspended)
        {
            mOutputBuffer->clear();   // faded out already - suspended this block
            return;
        }
    }

    if (active)
    {
        // 2026-05-06 (Batch 9b Item 8): dry-recorder tap (RAW pre-chain mono)
        // - captured here so the recorded file is the unprocessed DI; chain
        // runs ONCE on the dry source.  Only fire when ARMED (monitor-only
        // mode produces no recording).  tapDryRecorder gates its own read of
        // mStripRecorders on the mStripTapsLive flag the message thread lowers
        // before mutating that vector.
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

    // ── QA-Fb Option A (locked 2026-07-10): live strip over FilePlay clips ───
    // Prior takes sum with the live DI BEFORE the engine, so the pedals/amp
    // chain processes the stack in one pass -- bit-identical to how post-stop
    // playback runs the summed decode through the same chain.  The DRY tap
    // above reads the raw input snapshot directly, so capture never sees the
    // merge.  armed && !listen: live is captured but cleared from the monitor
    // before the takes join.
    if (filePlay)
    {
        decodeRoutedClips();
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

    // ── Sidechain fill + push ─────────────────────────────────────────────────
    // QA-Fe2 SC delay-match: fill + delay-match the strip's SC receive
    // buffers FIRST (pre-compensation source taps via the pull helper), then
    // hand those SAME aligned buffers to the engine -- engine-level SC and
    // rack/preEq/postEq SC read identical keys.  (Pre-QA-Fe2 the engine got
    // raw post-compensation predecessor outputs and the pull ran after the
    // engine render.)
    pullSidechainPredecessorsToGraph (*mGraph, channelId, mPredecessors, n);
    if (mScEngine != nullptr)
    {
        const VibeGraph::ScRecvArray scArr = mGraph->getScRecvArray (channelId);
        juce::AudioBuffer<float>* scBufs[VibeGraph::kMaxScRecvSlots] = {};
        for (int i = 0; i < VibeGraph::kMaxScRecvSlots; ++i)
            scBufs[i] = scArr[(size_t) i];
        mScEngine->setSidechainBuffers (scBufs, VibeGraph::kMaxScRecvSlots);
    }

    // ── Engine + insert chain ─────────────────────────────────────────────────
    juce::MidiBuffer  emptyMidi;
    juce::MidiBuffer* midi = (mCtx->instPageMidi != nullptr)
                              ? &mCtx->instPageMidi[mIndex]
                              : &emptyMidi;

    // QA-OctavePedal Task 5: stash the pre-engine live signal so the monitor
    // can cross-fade back to it in Dry mode.  Live-input strips only -- for
    // sfizz sources `active` is already false (arm/listen forced off), and pure
    // FilePlay playback returned above, so this only fires for real monitoring.
    // Gated so steady "With Effect" (the default) pays ZERO extra work: the
    // fork engages only while Dry is selected or a flip is still fading out.
    const bool monitorFork = active && (monitorMode == 0 || mMonitorDryGain > 0.0f);
    if (monitorFork)
    {
        mMonitorDryBuf.setSize (blockView.getNumChannels(), n, false, false, true);
        for (int ch = 0; ch < blockView.getNumChannels(); ++ch)
            mMonitorDryBuf.copyFrom (ch, 0, blockView, ch, 0, n);
    }

    mEngine->processBlock (blockView, *midi);

    // Monitor-only fork: With Effect (default) keeps the processed output; Dry
    // cross-fades back to the raw pre-engine signal (~15 ms ramp per flip =
    // click-free).  The recorded take (raw DI tap above) + playback (DI ->
    // engine) are byte-identical in both modes -- this re-weights the MONITOR
    // path only.
    if (monitorFork)
    {
        const float target = (monitorMode == 0) ? 1.0f : 0.0f;   // 1 = Dry, 0 = With Effect
        const float step   = 1.0f / juce::jmax (1.0f, 0.015f * (float) mProcessor->mSampleRate);
        const int   nc     = juce::jmin (blockView.getNumChannels(), mMonitorDryBuf.getNumChannels());
        float gEnd = mMonitorDryGain;
        for (int ch = 0; ch < nc; ++ch)
        {
            float*       out = blockView.getWritePointer (ch);
            const float* dry = mMonitorDryBuf.getReadPointer (ch);
            float g = mMonitorDryGain;
            for (int i = 0; i < n; ++i)
            {
                out[i] += (dry[i] - out[i]) * g;             // out*(1-g) + dry*g
                g += juce::jlimit (-step, step, target - g);
            }
            gEnd = g;
        }
        mMonitorDryGain = gEnd;
    }

    mGraph->processInsert (VibeGraph::InsertKind::Inst, mIndex,
                           blockView, mCtx->bpm, mCtx->anySolo);

    // ── Listen gate ───────────────────────────────────────────────────────────
    // sfizz-source: always route. Armed live + !listen: kill output.
    // Unarmed + listen: route naturally (input was copied into blockView above).
    // Unarmed + !listen: silent strip, nothing to clear.
    // QA-Fb: overlap blocks skip this -- live was already cleared pre-merge
    // and clearing here would kill the monitored prior takes.
    //
    // The two edits below land AFTER the chain pass, which is where the
    // pre-fader tap was filled, so each has to be applied to the tap as well or
    // a pre-fader send would carry signal this strip is not actually producing.
    // Null unless this strip is a pre-fader send source, so it costs one
    // relaxed load otherwise.
    auto* preFaderTap = mGraph->getPreFaderTapBuffer (channelId);

    if (! filePlay && armed && ! listen)
    {
        blockView.clear();
        if (preFaderTap != nullptr) preFaderTap->clear();
    }

    // Idle-suspend envelope, applied last so it covers the whole strip output
    // the suspend is about to stop producing - the insert chain's tail, not
    // just the sampler's own voices.
    IdleSuspendFade::apply (blockView, suspendRamp);
    if (preFaderTap != nullptr)
        IdleSuspendFade::apply (*preFaderTap, suspendRamp);
}
