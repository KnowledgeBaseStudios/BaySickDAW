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
    // tryLk skip).  Tracked in deferred notes for the snapshot-pattern
    // backlog (Batch 9b).
    juce::SpinLock::ScopedTryLockType tryLk (mProcessor->mAudioClipLock);
    if (! tryLk.isLocked()) return;

    // 2026-05-06 (Batch 9b Item 10): per-task scratch buffer eliminates the
    // cross-task race that existed when every AudioInsertTask pointed at
    // the shared VibeSynthProcessor::mAudioClipScratch.  Each task now
    // owns its own buffer (lazy-resized on first/changing block).  The
    // serial Pass 2 loop in PluginProcessor::processBlock also threads
    // through these per-row buffers so the new ownership pattern is
    // actively exercised under flag=false.
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
    clipCtx.clipScratch   = &getClipScratch (blockView.getNumChannels(), n);

    mProcessor->renderAudioClipsForRow (mIndex, clipCtx, &blockView);
}

juce::AudioBuffer<float>& AudioInsertTask::getClipScratch (int numChannels, int numSamples)
{
    // setSize with avoidReallocating=true is a no-op when already at the
    // requested geometry — cheap to call every block.  keepExistingContent
    // is false because the buffer is rewritten by every clip decode.
    if (mClipScratch.getNumChannels() != numChannels
        || mClipScratch.getNumSamples() < numSamples)
    {
        mClipScratch.setSize (numChannels, numSamples,
                              /*keepExistingContent*/ false,
                              /*clearExtraSpace*/    false,
                              /*avoidReallocating*/  true);
    }
    return mClipScratch;
}
