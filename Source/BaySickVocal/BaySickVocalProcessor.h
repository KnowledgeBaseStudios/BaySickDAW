#pragma once
#include <JuceHeader.h>
#include "../DSP/EngineSidechainHelper.h"
#include "../DSP/PitchCorrectorDSP.h"
#include "../EffectRack.h"
#include "../BaySickNAMIR/BaySickNAMIRProcessor.h"
#include "../Standalone/ApvtsDirtyTracker.h"

// ─────────────────────────────────────────────────────────────────────────────
// BaySickVocalProcessor — Phase H-1 skeleton (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────
// Vocal chain processor, peer to BaySickNAMIRProcessor in the Vox chain.
// APVTS prefix `bsv_*`.
//
// Locked signal flow (filled in by subsequent sub-batches):
//   input -> pitch correction -> de-esser -> compressor -> saturation
//         -> limiter -> output
//
// Editor (H-6) — 6 sub-tabs:
//   1. BaySickVocals      (realtime pitch + page-wide controls)
//   2. Vocal Chain        (de-esser / compressor / saturation / limiter rack)
//   3. BaySickPitch       (Newtone-clone offline pitch editor)
//   4. BaySickAlign       (VocAlign-clone offline time-alignment editor)
//   5. BaySickNAM/IR      (existing engine hosted as a sub-tab)
//   6. Pre Rack EQ        (strip's existing Pre EQ8 M/S)
//
// H-1 scope: structural shell only. No DSP yet — processBlock is a passthrough
// honoring the master bypass + mix params. Stage Bypass params registered as
// placeholders so subsequent sub-batches can wire their stages without
// re-touching the param layout.
// ─────────────────────────────────────────────────────────────────────────────

class BaySickVocalProcessor : public juce::AudioProcessor,
                              public ISidechainEngine
{
public:
    BaySickVocalProcessor();
    // 2026-05-06 (Batch 9c N1): no longer defaulted -- body sets the
    // mShuttingDown gate so subsequent processBlock entries bail before
    // dereferencing half-destroyed members.
    ~BaySickVocalProcessor() override;

    // ── AudioProcessor overrides ──────────────────────────────────────────────
    void prepareToPlay (double sampleRate, int maxBlockSize) override;
    void releaseResources()                                  override {}
    void processBlock  (juce::AudioBuffer<float>&,
                        juce::MidiBuffer&)                   override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                          { return true; }

    const juce::String getName() const override              { return "BaySickVocal"; }
    bool acceptsMidi()  const override                       { return false; }
    bool producesMidi() const override                       { return false; }
    bool isMidiEffect() const override                       { return false; }
    double getTailLengthSeconds() const override             { return 0.0; }

    int  getNumPrograms() override                           { return 1; }
    int  getCurrentProgram() override                        { return 0; }
    void setCurrentProgram (int) override                    {}
    const juce::String getProgramName (int) override         { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&)            override;
    void setStateInformation (const void*, int)              override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        // Stereo in / out (mono->stereo upmix done by host on input).
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
            && layouts.getMainInputChannelSet () == juce::AudioChannelSet::stereo();
    }

    // ── Public state access ───────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;

    // 2026-05-05 dirty-flag wiring (see ApvtsDirtyTracker.h).  Fires on every
    // bsv_* APVTS edit (vocals page knobs, vocal-chain rack slot params,
    // pitch correction params).  StandaloneEditor wires it to markDirty;
    // the embedded NAMIR sub-processor has its own tracker wired separately.
    void setOnAnyStateChange (std::function<void()> fn) { mDirtyTracker.onAny = std::move (fn); }

    // H-6c (2026-05-01): Vocal Chain rack -- 4 locked slots in fixed order:
    //   [0] DeEsser  [1] Compressor  [2] Saturation  [3] Limiter
    // Slots 4 + 5 unused (rack always has 6 slots; the trailing two are
    // EffectType::None and pass through silently).  Editor's Vocal Chain
    // sub-tab hosts SlotComponents bound to this rack.
    EffectRack mVocalChainRack;

    // H-6d (2026-05-02): owned BaySickNAMIRProcessor for the BaySickNAM/IR
    // sub-tab.  Owning it here (instead of on the editor) lets the page
    // preset save/load capture its state automatically through
    // BaySickVocalProcessor::getStateInformation.  Audio routing wired in
    // G-9 (2026-05-03): processBlock now calls mNamIrProc->processBlock
    // after the vocal-chain rack.
    BaySickNAMIRProcessor& getNamIrProcessor() noexcept { return *mNamIrProc; }

    // I-16 G-9 (2026-05-03): force-bypass realtime pitch correction.  Set to
    // true during FilePlay so a recording with realtime pitch already baked
    // into the wet file doesn't get corrected twice.  Engine loop in
    // PluginProcessor toggles this per-block based on source-mux state.
    // Independent of the user's pitch-correction toggle (which is the
    // "want correction" intent).  Both must be true for correction to run:
    // user-toggle ON AND not force-bypassed.
    void setForcePitchBypass (bool yes) noexcept { mForcePitchBypass.store (yes, std::memory_order_release); }
    bool isForcePitchBypassed() const noexcept   { return mForcePitchBypass.load (std::memory_order_acquire); }

    // 2026-05-06 (Batch 9c N1): atomic shutdown gate.  Mirrors
    // VibeSynthProcessor::mProjectLoadInProgress.  Owners SHOULD call
    // setShuttingDown(true) ~30 ms before destroying this instance (same
    // pattern StandaloneEditor::closeAllDynamicTabs uses on the main
    // processor: setProjectLoadInProgress + Thread::sleep(30)) so the
    // audio thread sees the flag in advance of member teardown.  The
    // destructor sets it as a final safety net; in either case, processBlock
    // bails out early instead of dereferencing mNamIrProc / mVocalChainRack
    // after their dtors begin (the observed crash was mNamIrProc->processBlock
    // dispatching through a vtable already zeroed by ~BaySickNAMIRProcessor).
    void setShuttingDown (bool b) noexcept
        { mShuttingDown.store (b, std::memory_order_release); }
    bool isShuttingDown() const noexcept
        { return mShuttingDown.load (std::memory_order_acquire); }

    // I-16 G-9 (2026-05-03): wet recording tap.  Plugged in by
    // VibeSynthProcessor::startRecording when this strip is armed.
    // Captures the post-realtime-pitch / pre-vocal-chain signal (Option C
    // from G-9 spec).  Pass nullptr to clear (stopRecording).
    void setWetRecorder (class AudioFileRecorder* recorder) noexcept
    {
        mWetRecorder.store (recorder, std::memory_order_release);
    }

    // ── Sidechain primitive (engine-level, for future ducking stages) ────────
    void setSidechainBuffers (juce::AudioBuffer<float>* const* bufs, int count) noexcept override
    {
        mScHelper.setSidechainBuffers (bufs, count);
    }
    float getSidechainLevel() const noexcept override { return mScHelper.getLevel(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ── Per-block APVTS->DSP push (called at top of processBlock) ───────────
    void pushApvtsToDsp() noexcept;

    // ── Sample-rate / block state ────────────────────────────────────────────
    double mSampleRate = 44100.0;
    int    mMaxBlock   = 512;
    bool   mPrepared   = false;

    EngineSidechainHelper mScHelper;

    // ── DSP chain stages (locked signal flow order) ─────────────────────────
    // input -> pitch correction -> [vocal chain rack: deess->comp->sat->lim]
    //       -> output
    // Pitch correction sits OUTSIDE the rack because it uses YIN tracker
    // backpressure + the realtime LiveASIO/FilePlay gating (G-9.1) doesn't
    // belong inside the slot-swappable rack model.
    PitchCorrectorDSP mPitchCorrector;

    juce::AudioBuffer<float> mDryScratch;   // for global Mix dry/wet crossfade

    // I-16 G-9 (2026-05-03): realtime-bypass + wet recorder hooks.  Atomics
    // because audio thread reads / message thread writes.
    std::atomic<bool> mForcePitchBypass { false };
    std::atomic<class AudioFileRecorder*> mWetRecorder { nullptr };

    // 2026-05-06 (Batch 9c N1): shutdown gate (see setShuttingDown above).
    // Audio thread reads at the top of processBlock; message thread writes
    // from the destructor (and ideally pre-flagged by the owner).
    std::atomic<bool> mShuttingDown { false };

    // H-6d (2026-05-02): owned NAM/IR processor (per-Vox-strip instance).
    // unique_ptr so the include only needs the forward declaration in
    // header consumers; full BaySickNAMIRProcessor.h is already included
    // above so we can use make_unique inline.
    std::unique_ptr<BaySickNAMIRProcessor> mNamIrProc;

    // 2026-05-05 dirty-flag wiring.  Declared LAST so apvts is fully constructed.
    ApvtsDirtyTracker mDirtyTracker { apvts };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickVocalProcessor)
};
