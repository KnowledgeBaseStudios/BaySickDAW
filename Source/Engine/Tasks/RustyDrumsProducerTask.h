#pragma once

#include <JuceHeader.h>

#include "../RenderTask.h"
#include "../BlockContext.h"

class BaySickRustyDrumsProcessor;
class VibeSynthProcessor;

// RustyDrumsProducerTask
// ----------------------
// Drives the singleton BaySickRustyDrums sfizz engine's processStrips() once
// per block, which fills the engine's internal per-strip output buffers
// (one per Rusty channel - up to 13 in the loaded kit).  This task does NOT
// publish to a channel-id arena slot; channelId stays -1.  The 13
// RustyInsertTasks consume the engine's internal buffers via getStripBuffer
// and have a synthetic dep on this producer registered with the dispatcher
// so they finish-after the producer.
//
// Idle suspend (mirrors serial behavior): if MIDI is empty AND the engine
// reports 0 active voices for kIdleSuspendBlocks consecutive blocks, skip
// processStrips entirely.  Wakes immediately on next block where any gate
// fails.  Counter lives in VibeSynthProcessor::mRustyIdleBlocks.
//
// Phase 6 scaffolding: dead at runtime while kEnableMultiThreadedEngine
// is constexpr false.
class RustyDrumsProducerTask : public RenderTask
{
public:
    RustyDrumsProducerTask (VibeSynthProcessor& processor);

    void run() override;

private:
    VibeSynthProcessor* mProcessor = nullptr;
};
