#include "PassiveStripTask.h"
#include "../../VibeGraph.h"
#include "../../PluginProcessor.h"
#include "../SidechainPullHelper.h"   // pullSidechainPredecessorsToGraph

PassiveStripTask::PassiveStripTask (Kind                kind,
                                    int                 auxOrBusIndex,
                                    int                 channelIdIn,
                                    VibeGraph&          graph,
                                    VibeSynthProcessor& processor)
    : mKind (kind),
      mIndex (auxOrBusIndex),
      mGraph (&graph),
      mProcessor (&processor)
{
    this->channelId = channelIdIn;
}

void PassiveStripTask::run()
{
    if (mOutputBuffer == nullptr || mCtx == nullptr || mGraph == nullptr)
        return;

    const int n = mCtx->numSamples;
    if (n <= 0) return;

    // Per-block view of the arena slot.
    float* const* ptrs = mOutputBuffer->getArrayOfWritePointers();
    juce::AudioBuffer<float> blockView (ptrs, mOutputBuffer->getNumChannels(), n);
    blockView.clear();

    // ── Pull from predecessors (PULL-model summation) ─────────────────────────
    // Each upstream task wrote to its own arena slot; we sum them here with
    // the per-link gain (main-out = unity, sends = dB → linear).
    const int nc = juce::jmin (2, blockView.getNumChannels());
    for (const auto& link : mPredecessors)
    {
        if (link.isSc) continue;   // SC handled inside engine tasks, not here
        if (link.source == nullptr || link.source->mOutputBuffer == nullptr) continue;

        const float gain = link.isMainOut
            ? 1.0f
            : juce::Decibels::decibelsToGain (link.gainDb, -60.0f);

        const auto& src = *link.source->mOutputBuffer;
        const int srcCh = juce::jmin (nc, src.getNumChannels());
        for (int c = 0; c < srcCh; ++c)
            blockView.addFrom (c, 0, src, c, 0, n, gain);
    }

    // 2026-05-07 (Batch 9c follow-up): SC accumulator population.  Aux
    // strips and buses can both have SC-capable DSP (compressors, dynamic
    // EQ) inside their racks / pre-EQ / post-EQ.
    pullSidechainPredecessorsToGraph (*mGraph, channelId, mPredecessors, n);

    // ── Run strip DSP chain ───────────────────────────────────────────────────
    if (mKind == Kind::Aux)
    {
        // Aux strips reuse the standard insert chain (polarity → preEq →
        // width → rack → postEq → fader × mute × solo → PDC → peak).
        mGraph->processInsert (VibeGraph::InsertKind::Aux, mIndex,
                               blockView, mCtx->bpm, mCtx->anySolo);
    }
    else   // Kind::Bus
    {
        // 2026-05-06 (Batch 9b): unified bus DSP via VibeGraph::processBus.
        // Predecessors have been summed into blockView above; processBus runs
        // the per-bus chain (preEq → rack → postEq → polarity/width → fader
        // × mute × solo → pan → peak meter) in-place.  Caller is responsible
        // for routing the result downstream - handled by the dispatcher's
        // pull model: consumer tasks read mOutputBuffer directly.
        //
        // 2026-05-07 (Batch 9c follow-up): pick the correct solo flag per
        // bus, matching the serial path's contract:
        //   * Layers/Bass/Drums + Master + FxBus -- processBus ignores the
        //     anySolo parameter (delegates to processChainOnly /
        //     processMasterBus / processEffectsBus which read solo state
        //     internally from APVTS).  Pass false to make this explicit.
        //   * ClipsBus + RustyDrumsBus -- processBus computes useGroupSolo
        //     locally inside processBus (overriding the parameter), so the
        //     value passed here is irrelevant.  Pass false (matches
        //     PluginProcessor.cpp:2448 + :2605).
        //   * Vox/Inst/Vox2/Inst2/Inst3 -- processBus uses the parameter
        //     directly for the in-group solo formula.  Pass busAnySolo
        //     (bus-level _solo states only), NOT mCtx->anySolo (strip-
        //     level), otherwise the bus mutes whenever any STRIP is
        //     soloed.  Mirrors PluginProcessor.cpp:2524.
        using namespace MixerChannelIds;
        bool soloFlag = false;
        switch (channelId)
        {
            case kVoxBus:
            case kInstBus:
            case kVoxBus2:
            case kInstBus2:
            case kInstBus3:
                soloFlag = mCtx->busAnySolo;
                break;
            default:
                soloFlag = false;   // L/B/D, FX, Clips, Rusty, Master ignore param
                break;
        }
        mGraph->processBus (channelId, blockView,
                             mCtx->bpm, soloFlag, mCtx->panLaw);
        juce::ignoreUnused (mProcessor);
    }
}
