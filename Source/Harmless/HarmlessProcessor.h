#pragma once
#include <JuceHeader.h>
#include "HarmlessSynth.h"
#include "HarmlessModRegistry.h"
#include "../DSP/EngineSidechainHelper.h"
#include "../Standalone/ApvtsDirtyTracker.h"

// ── HarmlessProcessor ─────────────────────────────────────────────────────────
// AudioProcessor wrapper for HarmlessSynth.
// Owns its own APVTS; param IDs follow the consolidated plan convention:
//   tk_{trackId}_harm_{paramName}
//
// CPU safeguarding:
//   updateFromApvts() guards every setter with a cached last-value comparison.
//   Only calls the setter when the value actually changed.
//
// Phase 3A-Final TODO:
//   createEditor() returns the HarmlessEditor (Basic/Advanced UI).
//   Timbre-shape changes will trigger partA/B.setShape() on a background thread.
// ─────────────────────────────────────────────────────────────────────────────
class HarmlessProcessor : public juce::AudioProcessor,
                          public ISidechainEngine
{
public:
    explicit HarmlessProcessor (const juce::String& trackId = "lay_0");

    // ── AudioProcessor interface ──────────────────────────────────────────────
    void prepareToPlay   (double sampleRate, int maxBlockSize) override;
    void releaseResources() override {}
    void processBlock    (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName()        const override { return "Harmless"; }
    bool   acceptsMidi()                const override { return true; }
    bool   producesMidi()               const override { return false; }
    bool   isMidiEffect()               const override { return false; }
    double getTailLengthSeconds()       const override { return 2.0; }

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

    // 2026-05-05 dirty-flag wiring (see ApvtsDirtyTracker.h).  Fires on every
    // tk_<id>_harm_* edit (full Harmless internal state - partial values,
    // filter / LFO / envelope / mod-matrix / etc).  StandaloneEditor wires it
    // to ProjectManager::markDirty.
    void setOnAnyStateChange (std::function<void()> fn) { mDirtyTracker.onAny = std::move (fn); }

    // Engine access for the Phase 3A-Final editor.
    HarmlessSynth& getSynth() { return mSynth; }

    // Param prefix used for this instance ("tk_0_harm_", "tk_1_harm_", …).
    const juce::String& getParamPrefix() const { return mPrefix; }

    // Thread-safe note audition (UI thread → audio thread via atomic)
    void auditionNote    (int midiNote) { mAuditionNote.store    (midiNote); }
    void auditionNoteOn  (int midiNote) { mAuditionHoldOn.store  (midiNote); }
    void auditionNoteOff (int midiNote) { mAuditionHoldOff.store (midiNote); }

    // Build param ID for this instance.
    juce::String pid (const char* name) const { return mPrefix + name; }

    // S4: per-target modulation registry (envelope / LFO / velocity / keyboard /
    // mod X / Y / Z per articulation target). Serialized into apvts.state as a
    // child ValueTree alongside the APVTS parameters. Lifetime = processor's.
    HarmlessModRegistry& getModRegistry() noexcept { return mModRegistry; }
    const HarmlessModRegistry& getModRegistry() const noexcept { return mModRegistry; }

    // C.4 Phase 2.2: engine-level SC primitive.  PluginProcessor's render
    // loop pushes the host InsertNode's SC array before processBlock.
    void setSidechainBuffers (juce::AudioBuffer<float>* const* bufs, int count) noexcept override
    {
        mScHelper.setSidechainBuffers (bufs, count);
    }
    float getSidechainLevel() const noexcept override { return mScHelper.getLevel(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout
    createLayout (const juce::String& prefix);

    // Reads APVTS values and pushes them to the synth.
    // CPU-guarded: only calls setters when values differ from cached state.
    // Called from processBlock - must be audio-thread safe.
    void updateFromApvts();

    HarmlessSynth       mSynth;
    juce::String        mPrefix;
    juce::String        mTrackId;
    std::atomic<int>    mAuditionNote    { -1 };
    std::atomic<int>    mAuditionHoldOn  { -1 };
    std::atomic<int>    mAuditionHoldOff { -1 };
    HarmlessModRegistry mModRegistry;
    EngineSidechainHelper mScHelper;   // C.4 Phase 2.2 SC primitive
    void registerModTargets();   // called once in ctor

    // ── CPU guard cache ───────────────────────────────────────────────────────
    struct ParamCache
    {
        // Existing fields
        int   timbreShape       { -1 };
        float partALevel        { -1.f };
        float partBLevel        { -1.f };
        float fltCutoff         { -1.f };
        float fltRes            { -1.f };
        float ampA              { -1.f };
        float ampD              { -1.f };
        float ampS              { -1.f };
        float ampR              { -1.f };
        int   unisonVoices      { -1 };
        float unisonDetune      { -1.f };
        float unisonSpread      { -1.f };
        float volume            { -1.f };
        float pan               { -1.f };
        // Beta additions
        float glideTime         { -1.f };
        int   legato            { -1 };
        int   cutSelf           { -1 };
        int   cutSelfMode       { -1 };
        float strumTime         { -1.f };
        float prismAmount       { -1.f };
        float pluckDecay        { -1.f };
        float blurSize          { -1.f };
        float filterMaskCutoff  { -1.f };
        float phaserMaskRate    { -1.f };
        float brownianAmount    { -1.f };

        // New 5F fields
        float phaseStart    { -1.f };
        float phaseRand     { -1.f };
        int   tremShape     { -1 };
        float tremDepth     { -1.f };
        float tremSpeed     { -1.f };
        float tremGap       { -1.f };
        int   vibShape      { -1 };
        float vibDepth      { -1.f };
        float vibSpeed      { -1.f };
        float vibEnv        { -1.f };
        float flt2Cutoff    { -1.f };
        float flt2Res       { -1.f };
        float pitchSemitones{ -1.f };
        float pitchCents    { -1.f };
        float ophaserMix    { -1.f };
        float ophaserDepth  { -1.f };
        float ophaserRate   { -1.f };
        int   strumDir      { -1 };
        float strumTns      { -1.f };
        // 2026-04-19 (S1): caches for newly-wired ghost params + new routing matrix.
        float timbreBlend   { -1.f };
        int   prismMode     { -1 };
        int   pluckBlur     { -1 };
        int   velLink       { -1 };
        float legatoLimit   { -1.f };
        float oeqMix        { -1.f };
        int   flt1Type      { -1 };
        int   flt2Type      { -1 };
        // S1's flt2KbTrack stub removed; S2 wires it via mCache.flt2KbTrack below.
        // Routing matrix
        float rmSub  { -1.f };
        float rmProt { -1.f };
        float rmClip { -1.f };
        float rmFx   { -1.f };
        float rmVol  { -1.f };
        float rmEnv  { -1.f };
        // 2026-04-19 (SLA-Impl) - Pitch fraction selector + Phaser extras.
        int   pitchFreqFrac { -1 };
        float ophaserWidth  { -1.f };
        float ophaserOfs    { -1.f };
        // 2026-04-19 (S2) - Filter envelopes + ofs + kb track + LFO + Mod XYZ + Blur ext.
        float fltA { -1.f }, fltD { -1.f }, fltS { -1.f }, fltR { -1.f };
        float fltEnvAmt { -2.f };
        float flt2A { -1.f }, flt2D { -1.f }, flt2S { -1.f }, flt2R { -1.f };
        float flt2EnvAmt { -2.f };
        float flt1CutoffOfs { -100.f };
        float flt2CutoffOfs { -100.f };
        float flt1KbTrack   { -1.f };
        float flt2KbTrack   { -1.f };
        // S4: S2 LFO + XYZ destination cache fields removed with params.
        float modX { -100.f }, modY { -100.f }, modZ { -100.f };
        int   autoGainMode { -1 };   // S4 AG-1
        // S4 Batch 4 fix: global LFO cache.
        int   lfoRateIdx { -1 };
        int   lfoShape   { -1 };
        int   lfoTempo   { -1 };
        // 2026-04-30: T2-B LFO depth cache (restored after S4 strip).
        // Sentinels init at -100 so the first block always pushes a value.
        float lfoVel    { -100.f };
        float lfoVol    { -100.f };
        float lfoPitch  { -100.f };
        float blurTime { -1.f };
        float blurHarm { -1.f };
        // 2026-04-19 (S3) - Unison Type / Alt / Phase + Part B shape.
        int   unisonTypeS3 { -1 };
        int   unisonAltS3  { -1 };
        float unisonPhaseS3 { -1.f };
        int   partBShape   { -1 };
        // 2026-04-19 (S3.5) - Per-part wavetable-domain twins.
        float pBBrownian { -1.f };
        float pBBlurSize { -1.f };
        float pBBlurTime { -1.f };
        float pBBlurHarm { -1.f };
        float pBPrismAmount { -2.f };
        int   pBPrismMode   { -1 };
        float pBPluckDecay  { -1.f };
        int   pBPluckBlur   { -1 };
        float pBFilterMaskCutoff { -1.f };
        float pBPhaserMaskRate   { -1.f };
    } mCache;

    // 2026-05-05 dirty-flag wiring.  Declared LAST so apvts is fully constructed.
    ApvtsDirtyTracker mDirtyTracker { apvts };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmlessProcessor)
};
