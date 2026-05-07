#include "PassiveStripTask.h"
#include "../../VibeGraph.h"
#include "../../PluginProcessor.h"

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
        // for routing the result downstream — handled by the dispatcher's
        // pull model: consumer tasks read mOutputBuffer directly.
        mGraph->processBus (channelId, blockView,
                             mCtx->bpm, mCtx->anySolo, mCtx->panLaw);
        juce::ignoreUnused (mProcessor);
    }
}
