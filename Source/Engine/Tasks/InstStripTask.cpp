#include "InstStripTask.h"
#include "../../VibeGraph.h"
#include "../../PluginProcessor.h"
#include "../../DSP/EngineSidechainHelper.h"
#include "../../BaySickGuitars/BaySickGuitarsProcessor.h"   // getNumActiveVoices
#include "../../BaySickBasses/BaySickBassesProcessor.h"     // getNumActiveVoices

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

    // FilePlay branch: Batch 5 territory.  Until then, silence.
    if (mIndex >= 0 && mIndex < (int) mProcessor->mInstFilePlayActive.size()
        && mProcessor->mInstFilePlayActive[(size_t) mIndex])
    {
        mOutputBuffer->clear();
        return;
    }

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

        if (midiEmpty && noVoices)
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

    // sfizz-active slots ignore arm — sfizz is the source.
    const bool armed = ! sfizzActive
                    && (armP != nullptr) && armP->load() > 0.5f
                    && chIdx >= 0
                    && chIdx <  snapChs;

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
    mGraph->processInsert (VibeGraph::InsertKind::Inst, mIndex,
                           blockView, mCtx->bpm, mCtx->anySolo);

    // ── Listen gate ───────────────────────────────────────────────────────────
    // sfizz-source: always route. Armed live: gate on _listen. Unarmed: route.
    if (armed)
    {
        const bool listen = (listenP != nullptr) && listenP->load() > 0.5f;
        if (! listen)
            blockView.clear();
    }
}
