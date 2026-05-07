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
// FilePlay clips (clip routed to a Vox/Inst engine) are NOT handled here -
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

    // 2026-05-06 (Batch 9b Item 10): per-task scratch buffer for clip decode +
    // per-clip rack processing.  Pre-9b, AudioClipBlockContext::clipScratch
    // pointed at the single shared VibeSynthProcessor::mAudioClipScratch, so
    // multiple AudioInsertTasks running in parallel under MT would race on
    // it (decode overwrites in-place; processInsert modifies in-place;
    // corruption window = entire clip rendering loop).  Each task now owns
    // its own buffer; the serial Pass 2 loop in PluginProcessor::processBlock
    // also routes through these per-row buffers so the new ownership pattern
    // is actively exercised under flag=false ("no dead wiring" rule).
    //
    // Caller passes the current block's channel count + sample count; this
    // method resizes-if-needed (avoidReallocating=true → cheap no-op when
    // already the right size) and returns a reference to the buffer.  Audio-
    // thread safe: only the audio thread (serial mode) or this task's worker
    // (MT mode) ever touches its own scratch - no cross-task contention.
    juce::AudioBuffer<float>& getClipScratch (int numChannels, int numSamples);

private:
    int                      mIndex     = 0;
    VibeSynthProcessor*      mProcessor = nullptr;
    juce::AudioBuffer<float> mClipScratch;
};
