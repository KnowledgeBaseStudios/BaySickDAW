#pragma once
#include <JuceHeader.h>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// EngineChainProcessor - Phase G-9 / I-16 (2026-05-03)
// ─────────────────────────────────────────────────────────────────────────────
// Non-owning AudioProcessor wrapper that chains processBlock calls through a
// fixed-order list of stage processors.  Used by InstPage so the audio thread
// drives the full per-page chain (BaySickPedals -> BaySickNAMIR) instead of
// a single stage.
//
// The chain holds raw pointers to existing stage processors -- it does NOT
// own them.  The page (InstPage) keeps owning the stages via its unique_ptrs
// and calls setChain() when those processors are constructed.  prepareToPlay
// forwards to all stages; processBlock applies each stage in order on the
// same buffer.
//
// State is intentionally empty -- per-page state is round-tripped via the
// page's exportInstState path.  This class adds no new save data.
// ─────────────────────────────────────────────────────────────────────────────

class EngineChainProcessor : public juce::AudioProcessor
{
public:
    EngineChainProcessor();
    ~EngineChainProcessor() override = default;

    // Set / clear the stage list.  Pass non-null processor pointers in chain
    // order.  Null entries are skipped at process time.  Safe to call from
    // the message thread; coordinated against the audio thread via spinlock.
    void setChain (std::initializer_list<juce::AudioProcessor*> stages);

    // ── AudioProcessor overrides ──────────────────────────────────────────────
    void prepareToPlay (double sampleRate, int blockSize) override;
    void releaseResources()                                override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
            && layouts.getMainInputChannelSet () == juce::AudioChannelSet::stereo();
    }

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override                      { return false; }

    const juce::String getName() const override          { return "EngineChain"; }
    bool acceptsMidi()  const override                   { return false; }
    bool producesMidi() const override                   { return false; }
    bool isMidiEffect() const override                   { return false; }
    double getTailLengthSeconds() const override         { return 0.0; }

    int  getNumPrograms() override                       { return 1; }
    int  getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override                {}
    const juce::String getProgramName (int) override     { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int)   override {}

private:
    juce::SpinLock                       mLock;
    std::vector<juce::AudioProcessor*>   mStages;
    double                               mSampleRate { 44100.0 };
    int                                  mBlockSize  { 0 };
    bool                                 mPrepared   { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineChainProcessor)
};
