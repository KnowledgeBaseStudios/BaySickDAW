#include "RustyDrumsProducerTask.h"
#include "../../PluginProcessor.h"
#include "../../BaySickRustyDrums/BaySickRustyDrumsProcessor.h"

RustyDrumsProducerTask::RustyDrumsProducerTask (VibeSynthProcessor& processor)
    : mProcessor (&processor)
{
    // channelId stays at -1 (default) - producer-style task with no arena slot.
}

void RustyDrumsProducerTask::run()
{
    if (mCtx == nullptr || mProcessor == nullptr) return;

    const int n = mCtx->numSamples;
    if (n <= 0) return;

    // Engine + active flag check (same gate as the serial path).
    if (! mProcessor->mRustyDrumsActive.load (std::memory_order_acquire)) return;

    juce::SpinLock::ScopedTryLockType lk (mProcessor->mRustyDrumsEngineLock);
    if (! lk.isLocked()) return;

    auto* engine = mProcessor->mRustyDrumsEngine.get();
    if (engine == nullptr) return;

    // Idle suspend: if MIDI is empty AND no active voices for
    // kIdleSuspendBlocks consecutive blocks, skip the entire processStrips
    // call.  Wakes immediately on next block where either gate fails.
    juce::MidiBuffer  emptyMidi;
    juce::MidiBuffer* midi = mCtx->rustyDrumsMidi != nullptr
                              ? mCtx->rustyDrumsMidi
                              : &emptyMidi;

    const bool midiEmpty = midi->getNumEvents() == 0;
    const bool noVoices  = engine->getNumActiveVoices() == 0;

    if (midiEmpty && noVoices)
    {
        if (mProcessor->mRustyIdleBlocks >= VibeSynthProcessor::kIdleSuspendBlocks)
            return;   // suspended this block
        ++mProcessor->mRustyIdleBlocks;
    }
    else
    {
        mProcessor->mRustyIdleBlocks = 0;
    }

    engine->processStrips (n, *midi);
}
