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

    // FilePlay branch: serial code skips this engine and lets the audio-
    // clip loop drive it. In the parallel path, AudioInsertTask (Batch 5)
    // will own that case. Until then write silence so any partial flag-flip
    // doesn't double-render.
    if (mIndex >= 0 && mIndex < (int) mProcessor->mVoxFilePlayActive.size()
        && mProcessor->mVoxFilePlayActive[(size_t) mIndex])
    {
        mOutputBuffer->clear();
        return;
    }

    // Build a "this-block" view of the arena slot's storage at the host's
    // current numSamples, then start by zeroing the output region.
    float* const* ptrs = mOutputBuffer->getArrayOfWritePointers();
    juce::AudioBuffer<float> blockView (ptrs, mOutputBuffer->getNumChannels(), n);
    blockView.clear();

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
