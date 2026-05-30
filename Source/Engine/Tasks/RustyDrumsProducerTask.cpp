#include "RustyDrumsProducerTask.h"
#include "../../PluginProcessor.h"
#include "../../BaySickRustyDrums/BaySickRustyDrumsProcessor.h"
#include "../RenderEngineFlags.h"   // QA-DispatcherAffinity TraceScope

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
    //
    // QA-DispatcherAffinity Task 2 Stage B (2026-05-29): the runtime override
    // gSubKOverride (Mixer hamburger "Sub-K Serial Fallback" toggle) gates
    // this pin at VibeThreadPool::submit() time, not at this construction
    // site -- the task stays tagged mAudioThreadOnly=true (the "wants
    // pinning" intent) and the override decides at submit() whether to honor
    // it.  Lets the same session A/B Sub-K-on vs Sub-K-off without a kit
    // reload.  See MtDiagnostic namespace in RenderEngineFlags.h.
    mAudioThreadOnly = true;
}

void RustyDrumsProducerTask::run()
{
    // QA-DispatcherAffinity (2026-05-28): entry+exit timestamp trace.
    // RAII captures entry tick on construction; destructor emits the trace
    // event with the exit tick on every return path.  Zero overhead when
    // gTraceTaskTimestamps is false.  engineInstance = the BaySickRusty-
    // DrumsProcessor singleton (constant across all 14 Rusty tasks; lets
    // the analyzer group them by engine).
    RenderEngine::MtDiagnostic::TraceScope qaTrace (channelId,
        (mProcessor != nullptr) ? (const void*) mProcessor->mRustyDrumsEngine.get() : nullptr);

    if (mCtx == nullptr || mProcessor == nullptr) return;

    const int n = mCtx->numSamples;
    if (n <= 0) return;

    // Engine + active flag check.
    if (! mProcessor->mRustyDrumsActive.load (std::memory_order_acquire)) return;

    // QA-DispatcherAffinity Task 3 (2026-05-29): try-lock REMOVED per Sub-A
    // = (i) resolution.  Same rationale as RustyInsertTask::run() -- the
    // mProjectLoadInProgress shield raised at destroyBaySickRustyDrums +
    // loadBaySickRustyDrumsKit (in this same commit) keeps the engine
    // pointer stable for the audio block.  The producer was the sole
    // upstream of the 13 insert tasks (via synthetic dep) so its lock-free
    // dispatch is the prerequisite for the inserts running lock-free.
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
