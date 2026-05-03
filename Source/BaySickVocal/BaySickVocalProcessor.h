#pragma once
#include <JuceHeader.h>
#include "../DSP/EngineSidechainHelper.h"
#include "../DSP/PitchCorrectorDSP.h"
#include "../EffectRack.h"
#include "../BaySickNAMIR/BaySickNAMIRProcessor.h"

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
    ~BaySickVocalProcessor() override = default;

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

    // H-6c (2026-05-01): Vocal Chain rack -- 4 locked slots in fixed order:
    //   [0] DeEsser  [1] Compressor  [2] Saturation  [3] Limiter
    // Slots 4 + 5 unused (rack always has 6 slots; the trailing two are
    // EffectType::None and pass through silently).  Editor's Vocal Chain
    // sub-tab hosts SlotComponents bound to this rack.
    EffectRack mVocalChainRack;

    // H-6d (2026-05-02): owned BaySickNAMIRProcessor for the BaySickNAM/IR
    // sub-tab.  Owning it here (instead of on the editor) lets the page
    // preset save/load capture its state automatically through
    // BaySickVocalProcessor::getStateInformation.  Audio routing through
    // it is wired in G-9.
    BaySickNAMIRProcessor& getNamIrProcessor() noexcept { return *mNamIrProc; }

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

    // H-6d (2026-05-02): owned NAM/IR processor (per-Vox-strip instance).
    // unique_ptr so the include only needs the forward declaration in
    // header consumers; full BaySickNAMIRProcessor.h is already included
    // above so we can use make_unique inline.
    std::unique_ptr<BaySickNAMIRProcessor> mNamIrProc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickVocalProcessor)
};
