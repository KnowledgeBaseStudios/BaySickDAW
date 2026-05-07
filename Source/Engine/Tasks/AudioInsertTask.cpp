#include "AudioInsertTask.h"
#include "../../PluginProcessor.h"
#include "../../PatternManager.h"

AudioInsertTask::AudioInsertTask (int                 index,
                                  int                 channelIdIn,
                                  VibeSynthProcessor& processor)
    : mIndex (index),
      mProcessor (&processor)
{
    this->channelId = channelIdIn;
}

void AudioInsertTask::run()
{
    if (mOutputBuffer == nullptr || mCtx == nullptr || mProcessor == nullptr)
        return;

    const int n = mCtx->numSamples;
    if (n <= 0)
        return;

    // Per-block view of the arena slot.  Clip outputs accumulate via addFrom
    // inside renderAudioClipsForRow when mtDest is provided.
    float* const* ptrs = mOutputBuffer->getArrayOfWritePointers();
    juce::AudioBuffer<float> blockView (ptrs, mOutputBuffer->getNumChannels(), n);
    blockView.clear();

    // Match the serial code's gate: song mode + playing + PatternManager exists.
    // mCtx->posInfo is set by the dispatcher per block (Batch 8 wiring).
    if (mCtx->posInfo == nullptr) return;
    if (! mCtx->posInfo->getIsPlaying()) return;
    if (mProcessor->mPatternManager == nullptr) return;
    if (! mProcessor->mSongMode.load (std::memory_order_relaxed)) return;

    const double bpm        = mCtx->bpm;
    const double secPerBeat = 60.0 / juce::jmax (20.0, bpm);
    const double beatStart  = mCtx->posInfo->getPpqPosition().orFallback (0.0);

    const juce::int64 projectStart = (juce::int64) (beatStart * secPerBeat * mProcessor->mSampleRate);
    const juce::int64 projectEnd   = projectStart + n;

    // Master gain mirrors processBlock's calculation (apvts knob × mixer master).
    const auto& mx = mProcessor->mPatternManager->getMixer();
    float masterGain = mx.masterLevel;
    if (auto* p = mProcessor->apvts.getRawParameterValue ("masterGain"))
        masterGain *= p->load();

    // Try-lock the audio-clip vector.  In MT mode multiple AudioInsertTasks
    // can race for this lock; if a worker can't acquire it this block, its
    // row produces silence (acceptable degraded state, same shape as serial
    // tryLk skip).  Tracked in deferred notes for Batch 9 redesign.
    juce::SpinLock::ScopedTryLockType tryLk (mProcessor->mAudioClipLock);
    if (! tryLk.isLocked()) return;

    // Per-task scratch is the shared mAudioClipScratch in serial; in MT this
    // would race between rows.  For Batch 5 (dead code) we use the shared
    // member; Batch 9 redesign needs per-task scratch.  Tracked in deferred
    // notes.
    VibeSynthProcessor::AudioClipBlockContext clipCtx;
    clipCtx.bpm           = bpm;
    clipCtx.anySolo       = mCtx->anySolo;
    clipCtx.secPerBeat    = secPerBeat;
    clipCtx.projectStart  = projectStart;
    clipCtx.projectEnd    = projectEnd;
    clipCtx.numSamples    = n;
    clipCtx.numOut        = blockView.getNumChannels();
    clipCtx.masterGain    = masterGain;
    clipCtx.mxState       = &mx;
    clipCtx.clipScratch   = &mProcessor->mAudioClipScratch;

    mProcessor->renderAudioClipsForRow (mIndex, clipCtx, &blockView);
}
