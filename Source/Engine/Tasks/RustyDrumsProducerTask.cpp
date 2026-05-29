#include "RustyDrumsProducerTask.h"
#include "../../PluginProcessor.h"
#include "../../BaySickRustyDrums/BaySickRustyDrumsProcessor.h"

RustyDrumsProducerTask::RustyDrumsProducerTask (VibeSynthProcessor& processor)
    : mProcessor (&processor)
{
    // channelId stays at -1 (default) - producer-style task with no arena slot.

    // QA-Sfizz Sub-K Serial Fallback (2026-05-28): pin to the audio thread.
    // The vendored sfizz library isn't safe for the cross-block work-stealing
    // the unified dispatcher currently does on this engine's producer +
    // RustyInsertTask family (mMultiOutScratch read-write race across block
    // boundaries + suspected thread-local-state migration across workers
    // breaking voice continuity on long sustains).  See QA-DispatcherAffinity
    // §9 Forks entry for the proper dispatcher-barrier fix that retires this
    // pin.  Workers will never see this task; only runUntilOrTimeout runs it.
    mAudioThreadOnly = true;
}

void RustyDrumsProducerTask::run()
{
    if (mCtx == nullptr || mProcessor == nullptr) return;

    const int n = mCtx->numSamples;
    if (n <= 0) return;

    // Engine + active flag check.
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
    // QA-C DSP-10 (2026-05-10): peek audition state so a queued audition
    // (e.g. kit-graphic hitbox click) wakes the chain instead of being
    // silently swallowed during idle-suspend.
    const bool auditionPending = engine->isAuditionPending();

    if (midiEmpty && noVoices && ! auditionPending)
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
