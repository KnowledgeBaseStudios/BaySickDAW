#include "RustyDrumsProducerTask.h"
#include "../../PluginProcessor.h"
#include "../../BaySickRustyDrums/BaySickRustyDrumsProcessor.h"

RustyDrumsProducerTask::RustyDrumsProducerTask (BaySickDAWProcessor& processor)
    : mProcessor (&processor)
{
    // channelId stays at -1 (default) - producer-style task with no arena slot.
}

void RustyDrumsProducerTask::run()
{
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

    // ── TS7 §6.9: the frozen-kit skip (Jeff, 2026-07-30) ─────────────────────
    // THIS is where a kit freeze actually saves CPU.  One sfizz instance renders
    // 26 channels every block; the 13 strip tasks then read from it.  Freezing
    // the kit substitutes at each strip (RustyInsertTask), so nothing downstream
    // needs the engine's output any more and the whole synthesis can go.
    //
    // SKIP THE WHOLE CALL OR NONE OF IT.  processStrips dispatches every
    // noteOn/noteOff/CC into sfizz BEFORE rendering, and sfizz voices only
    // advance inside renderBlock -- so dispatching MIDI while skipping the
    // render would pile up voices that never age out, permanently poisoning both
    // the engine's own zero-voice gate and the idle-suspend gate below.
    // BOTH scopes (2026-07-31): gating on songMode alone made pattern-mode kit
    // freeze CPU-NEGATIVE -- full synthesis every block PLUS 13 streamer reads.
    // Pattern mode skips only while the CURRENT pattern's renders are the
    // published set, so a pattern switch synthesizes live until the republish
    // lands (the strips' index-matched pointers fall back the same way).
    {
        const bool frozenThisBlock = mCtx->songMode
            ? mProcessor->mRustyKitFrozen.load (std::memory_order_acquire)
            : (mCtx->patternIndex >= 0
               && mProcessor->mRustyFrozenPatternIndex.load (std::memory_order_acquire)
                    == mCtx->patternIndex);
        if (frozenThisBlock)
            return;
    }

    // Idle suspend: if MIDI is empty AND no active voices for
    // kIdleSuspendBlocks consecutive blocks, skip the entire processStrips
    // call.  Wakes immediately on next block where either gate fails.
    //
    // The hold expiring starts a fade rather than an instant stop: the engine
    // keeps rendering at falling gain for IdleSuspendFade::kFadeOutSeconds, and
    // only once that reaches zero does the skip take over.  Same envelope and
    // the same reasoning as InstStripTask - a shutdown that lands in one sample
    // guillotines anything still ringing (see IdleSuspendFade.h).
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

    bool holdExpired = false;
    if (midiEmpty && noVoices && ! auditionPending)
    {
        if (mProcessor->mRustyIdleBlocks >= BaySickDAWProcessor::kIdleSuspendBlocks)
            holdExpired = true;
        else
            ++mProcessor->mRustyIdleBlocks;
    }
    else
    {
        mProcessor->mRustyIdleBlocks = 0;
    }

    const auto suspendRamp = mSuspendFade.advance (holdExpired, n, mProcessor->mSampleRate);
    if (suspendRamp.suspended)
        return;   // faded out already - suspended this block

    engine->processStrips (n, *midi);

    // getStripBuffer hands back a view onto the engine's multi-out scratch, so
    // the ramp lands on the buffers the 13 RustyInsertTasks are about to copy
    // (their synthetic dep on this producer orders them after this write).
    if (! suspendRamp.isUnity())
    {
        const int strips = engine->getStripCount();
        for (int s = 0; s < strips; ++s)
        {
            auto view = engine->getStripBuffer (s, n);
            if (view.getNumChannels() > 0)
                IdleSuspendFade::apply (view, suspendRamp);
        }
    }
}
