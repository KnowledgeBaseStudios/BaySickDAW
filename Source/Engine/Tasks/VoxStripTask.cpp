#include "VoxStripTask.h"
#include "../../VibeGraph.h"
#include "../../PluginProcessor.h"
#include "../../DSP/EngineSidechainHelper.h"
#include "../../BaySickVocal/BaySickVocalProcessor.h"

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

    // 2026-05-06 (Batch 9b Item 9): FilePlay branch.  When an audio clip is
    // routed to this Vox engine (player.routeChannel == channelId),
    // VibeSynthProcessor::renderFilePlayPlayer decodes the clip + drives the
    // engine + processInsert + writes to blockView (mtDest).  The live-input
    // path below is skipped entirely.  Same path the serial Pass 1 loop
    // uses, so the helper is exercised under flag=false.
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
        const juce::int64 projectStart =
            (juce::int64) (beatStart * secPerBeat * mProcessor->mSampleRate);
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
        // mAudioClipScratch is shared across tasks — race in MT mode but flag
        // is constexpr false; future fix tracked alongside the AudioClipPlayer
        // snapshot work in 9c.
        clipCtx.clipScratch  = &mProcessor->mAudioClipScratch;

        juce::MidiBuffer  emptyMidi;
        juce::MidiBuffer& engineMidi = (mCtx->voxPageMidi != nullptr)
            ? mCtx->voxPageMidi[mIndex] : emptyMidi;

        // 2026-05-06 (Batch 9c B1): iterate the audio-thread snapshot.
        for (auto& player : mProcessor->mCurrentBlockClipSnapshot->players)
        {
            if (player.routeChannel != channelId) continue;
            mProcessor->renderFilePlayPlayer (player, clipCtx, engineMidi, &blockView);
        }
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

    const bool armed = (armP != nullptr) && armP->load() > 0.5f
                    && chIdx >= 0
                    && chIdx <  snapChs;

    // ── Live-input copy (armed) ───────────────────────────────────────────────
    if (armed)
    {
        // 2026-05-06 (Batch 9b Item 8): dry-recorder tap (RAW pre-chain mono).
        // Mirrors the serial path's inline loop — captured here so the recorded
        // file is the unprocessed DI; chain runs ONCE on the dry source.
        // Pre-existing race risk between this read of mStripRecorders and
        // message-thread mutation in startRecording / stopRecording is
        // documented at the helper site; not closed in 9b.
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
    mGraph->processInsert (VibeGraph::InsertKind::Vox, mIndex,
                           blockView, mCtx->bpm, mCtx->anySolo);

    // ── Listen gate ───────────────────────────────────────────────────────────
    // Armed && !listen -> kill output (don't route to the bus).  Unarmed
    // pages always route (engine may produce its own audio from MIDI).
    if (armed)
    {
        const bool listen = (listenP != nullptr) && listenP->load() > 0.5f;
        if (! listen)
            blockView.clear();
    }
}
