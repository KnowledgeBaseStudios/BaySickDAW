#pragma once

#include <JuceHeader.h>

#include "../RenderTask.h"
#include "../BlockContext.h"

class BaySickGraph;
class ISidechainEngine;
class BaySickDAWProcessor;
class BaySickRustyDrumsProcessor;

// RustyInsertTask
// ---------------
// Per-strip task for one of the BaySickRustyDrums sfizz engine's outputs.
// One instance per active strip (up to 13 - kit-dependent).  channelId =
// MixerChannelIds::rustyInsert(stripIndex).
//
// Each task has a synthetic dep on RustyDrumsProducerTask (registered in
// PluginProcessor when the producer + inserts are spawned).  When the
// dispatcher runs, the producer fires first (drives processStrips), then
// the 13 inserts can run in parallel - each reads its own strip from the
// engine's internal buffer and runs the per-strip insert chain.
//
// QA-Ef (2026-05-21): this is the live audio plumbing.
class RustyInsertTask : public RenderTask
{
public:
    RustyInsertTask (int                 stripIndex,
                     int                 channelIdIn,
                     BaySickGraph&          graph,
                     BaySickDAWProcessor& processor);

    void run() override;

private:
    int                  mStripIndex = 0;
    BaySickGraph*           mGraph      = nullptr;
    BaySickDAWProcessor*  mProcessor  = nullptr;
    // Audio-thread only.  Last engine render seq this strip copied from; the
    // producer does not run every block (frozen-kit skip, idle suspend), and
    // the scratch then still holds the previous render -- copying it would
    // repeat old drum audio.
    juce::uint32         mLastSeenRenderSeq = 0;
};
