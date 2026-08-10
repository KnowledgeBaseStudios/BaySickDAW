#include "PassiveStripTask.h"
#include "../../BaySickGraph.h"
#include "../../PluginProcessor.h"
#include "../SidechainPullHelper.h"   // pullSidechainPredecessorsToGraph
#include "SendSourceRead.h"           // post-fader output vs pre-fader send tap

PassiveStripTask::PassiveStripTask (Kind                kind,
                                    int                 auxOrBusIndex,
                                    int                 channelIdIn,
                                    BaySickGraph&          graph,
                                    BaySickDAWProcessor& processor)
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
    // the per-link gain (main-out = unity, sends = dB → linear).  A pre-fader
    // send reads the source's tap rather than that slot -- see SendSourceRead.h.
    const int nc = juce::jmin (2, blockView.getNumChannels());
    for (const auto& link : mPredecessors)
    {
        if (link.isSc) continue;   // SC handled inside engine tasks, not here

        const auto* srcBuf = SendSourceRead::bufferFor (*mGraph, link);
        if (srcBuf == nullptr) continue;

        const float gain = link.isMainOut
            ? 1.0f
            : juce::Decibels::decibelsToGain (link.gainDb, -60.0f);

        const auto& src = *srcBuf;
        const int srcCh = juce::jmin (nc, src.getNumChannels());
        // Length clamp: the arena slot and every tap are sized to the prepared
        // max block, but a source whose node has not been prepared yet has a
        // zero-length buffer, and addFrom would read past it in release.
        const int srcN  = juce::jmin (n, src.getNumSamples());
        for (int c = 0; c < srcCh; ++c)
            blockView.addFrom (c, 0, src, c, 0, srcN, gain);
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
        mGraph->processInsert (BaySickGraph::InsertKind::Aux, mIndex,
                               blockView, mCtx->bpm, mCtx->anySolo);
    }
    else   // Kind::Bus
    {
        // 2026-05-06 (Batch 9b): unified bus DSP via BaySickGraph::processBus.
        // Predecessors have been summed into blockView above; processBus runs
        // the per-bus chain (preEq → rack → postEq → polarity/width → fader
        // × mute × solo → pan → peak meter) in-place.  Caller is responsible
        // for routing the result downstream - handled by the dispatcher's
        // pull model: consumer tasks read mOutputBuffer directly.
        //
        // QA-Ef: processBus computes the bus-solo gate itself (anyBusSoloed(),
        // all 11 buses); no solo flag is passed from here.
        mGraph->processBus (channelId, blockView, mCtx->bpm, mCtx->panLaw);
        juce::ignoreUnused (mProcessor);
    }
}
