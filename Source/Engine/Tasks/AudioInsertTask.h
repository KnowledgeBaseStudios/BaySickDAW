#pragma once

#include <JuceHeader.h>

#include "../RenderTask.h"
#include "../BlockContext.h"

class VibeSynthProcessor;

// AudioInsertTask
// ---------------
// Per-row audio clip render task. Owns one channel id (audioInsert(row)) and
// is responsible for decoding all NON-FilePlay clips on that row through the
// shared helper VibeSynthProcessor::renderAudioClipsForRow.
//
// Modified Option A (Jeff, 2026-05-06): the helper is the same one the serial
// audio-clip loop calls, so the code path is actively tested today instead
// of waiting for Batch 9.  When kEnableMultiThreadedEngine flips, the task's
// run() invokes the helper with mtDest = mOutputBuffer so the pull-model
// dispatcher can consume the row's summed clip output downstream.
//
// FilePlay clips (clip routed to a Vox/Inst engine) are NOT handled here —
// the helper skips them silently and the inline FilePlay pass in
// PluginProcessor::processBlock owns that path.  See deferred notes in
// the recovery doc Batch 5 entry.
//
// Phase 5 scaffolding: dead at runtime while kEnableMultiThreadedEngine
// is constexpr false.  Compile + register-task lifecycle are exercised; the
// helper call inside run() is gated.
class AudioInsertTask : public RenderTask
{
public:
    AudioInsertTask (int                 index,
                     int                 channelIdIn,
                     VibeSynthProcessor& processor);

    void run() override;

private:
    int                 mIndex     = 0;
    VibeSynthProcessor* mProcessor = nullptr;
};
