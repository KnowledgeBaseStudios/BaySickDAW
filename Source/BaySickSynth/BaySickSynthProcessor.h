#pragma once
#include <JuceHeader.h>
#include "BaySickSynthDSP.h"
#include "../DSP/EngineSidechainHelper.h"
#include "../Standalone/ApvtsDirtyTracker.h"

// ── BaySickSynthProcessor ─────────────────────────────────────────────────────
// AudioProcessor wrapper for BaySickSynthDSP.
// Owns its own APVTS; param IDs follow the plan convention:
//   tk_{trackId}_bss_{paramName}
//
// CPU safeguarding: updateFromApvts() guards every setter with a cached
// last-value comparison.  Only calls the setter when value actually changed.
// processBlock fires hundreds of times per second - keep setters cheap.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickSynthProcessor : public juce::AudioProcessor,
                              public ISidechainEngine
{
public:
    explicit BaySickSynthProcessor (const juce::String& trackId = "lay_0");

    // ── AudioProcessor interface ──────────────────────────────────────────────
    void prepareToPlay    (double sampleRate, int maxBlockSize) override;
    void releaseResources () override {}
    void processBlock     (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName()        const override { return "BaySickSynth"; }
    bool   acceptsMidi()                const override { return true; }
    bool   producesMidi()               const override { return false; }
    bool   isMidiEffect()               const override { return false; }
    double getTailLengthSeconds()       const override { return 1.0; }

    int  getNumPrograms()                override { return 1; }
    int  getCurrentProgram()             override { return 0; }
    void setCurrentProgram  (int)        override {}
    const juce::String getProgramName   (int)          override { return "Default"; }
    void changeProgramName  (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest) override;
    void setStateInformation (const void* data, int sz) override;

    bool isBusesLayoutSupported (const BusesLayout&) const override;

    // ── Public interface ──────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;

    // 2026-05-05 dirty-flag wiring (see ApvtsDirtyTracker.h).
    void setOnAnyStateChange (std::function<void()> fn) { mDirtyTracker.onAny = std::move (fn); }

    BaySickSynthDSP& getSynth() { return mSynth; }

    const juce::String& getParamPrefix() const { return mPrefix; }
    juce::String pid (const char* name)  const { return mPrefix + name; }

    // Thread-safe note audition (UI thread → audio thread via atomic)
    void auditionNote    (int midiNote) { mAuditionNote.store    (midiNote); }
    void auditionNoteOn  (int midiNote) { mAuditionHoldOn.store  (midiNote); }
    void auditionNoteOff (int midiNote) { mAuditionHoldOff.store (midiNote); }

    // Visualizer reads this to drive its LFO animation at the sync-aware rate.
    std::atomic<float>* getEffectiveLfoRatePtr() { return &mEffectiveLfoRate; }

    // C.4 Phase 2.2: engine-level SC primitive.
    void setSidechainBuffers (juce::AudioBuffer<float>* const* bufs, int count) noexcept override
    {
        mScHelper.setSidechainBuffers (bufs, count);
    }
    float getSidechainLevel() const noexcept override { return mScHelper.getLevel(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout
    createLayout (const juce::String& prefix);

    void updateFromApvts();

    BaySickSynthDSP      mSynth;
    juce::String         mPrefix;
    juce::String         mTrackId;
    std::atomic<int>     mAuditionNote       { -1 };
    std::atomic<int>     mAuditionHoldOn     { -1 };
    std::atomic<int>     mAuditionHoldOff    { -1 };
    float                mHostBPM            { 120.0f };
    float                mLastSyncedBpm      { -1.0f };   // QA-EngineApvts: tempo-change detect for the gate
    std::atomic<float>   mEffectiveLfoRate   { 1.0f };
    EngineSidechainHelper mScHelper;   // C.4 Phase 2.2 SC primitive

    // ── CPU guard cache ───────────────────────────────────────────────────────
    struct ParamCache
    {
        float outVol        { -1.f };   // 2026-04-25: master out (parity with player)
        int   waveform      { -1 };
        int   transpose     { -9999 };
        float modifier      { -1.f };
        int   dualOscMode   { -1 };
        int   oscSync       { -1 };
        int   ringMod       { -1 };
        float drift         { -1.f };
        int   unisonVoices  { -1 };
        float unisonDetune  { -1.f };
        float unisonSpread  { -1.f };
        float noise         { -1.f };
        int   noiseOnly     { -1 };
        int   noiseColor    { -1 };
        int   voiceMode     { -1 };
        float glide         { -1.f };
        int   cutSelf       { -1 };
        int   modWheelDest  { -1 };
        float modWheelAmt   { -1.f };

        float ampA          { -1.f };
        float ampD          { -1.f };
        float ampS          { -1.f };
        float ampR          { -1.f };
        float velAmpTrack   { -1.f };

        float pEnvA         { -1.f };
        float pEnvD         { -1.f };
        float pEnvS         { -1.f };
        float pEnvR         { -1.f };
        float pEnvAmt       { -99.f };  // sentinel; valid range -24..+24

        float transAmt      { -1.f };
        float transDur      { -1.f };
        float transCol      { -1.f };

        int   burstMode     { -1 };
        int   burstCount    { -1 };
        float burstSpacing  { -1.f };

        int   fltType       { -1 };
        float fltCutoff     { -1.f };
        float fltRes        { -1.f };
        float fltEnvAmt     { -1.f };
        float fltKbTrack    { -1.f };
        float fltVelTrack   { -1.f };

        float fltA          { -1.f };
        float fltD          { -1.f };
        float fltS          { -1.f };
        float fltR          { -1.f };

        int   lfoShape      { -1 };
        int   lfoDest       { -1 };
        float lfoRate       { -1.f };
        float lfoAmount     { -1.f };
        int   lfoSync       { -1 };
        int   lfoDivision   { -1 };
        float lfoHostBPM    { -1.f };
    } mCache;

    // 2026-05-05 dirty-flag wiring.  Declared LAST so apvts is fully constructed.
    ApvtsDirtyTracker mDirtyTracker { apvts };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickSynthProcessor)
};
