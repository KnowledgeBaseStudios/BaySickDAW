#pragma once

#include <JuceHeader.h>

#include "../RenderTask.h"
#include "../BlockContext.h"
#include "IdleSuspendFade.h"

class BaySickRustyDrumsProcessor;
class BaySickDAWProcessor;

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
// Idle suspend: if MIDI is empty AND the engine reports 0 active voices for
// kIdleSuspendBlocks consecutive blocks, fade the rendered strips out over
// IdleSuspendFade::kFadeOutSeconds and only then skip processStrips entirely.
// Wakes on the next block where any gate fails, ramping back to unity from
// wherever the fade had got to.  Counter lives in
// BaySickDAWProcessor::mRustyIdleBlocks.
//
// QA-Ef (2026-05-21): this is the live audio plumbing.
class RustyDrumsProducerTask : public RenderTask
{
public:
    RustyDrumsProducerTask (BaySickDAWProcessor& processor);

    void run() override;

private:
    BaySickDAWProcessor* mProcessor = nullptr;

    // Idle-suspend shutdown envelope (see IdleSuspendFade.h).
    IdleSuspendFade mSuspendFade;
};
