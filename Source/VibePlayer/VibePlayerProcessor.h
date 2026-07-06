#pragma once
#include <JuceHeader.h>
#include "VibePlayerDSP.h"
#include "../DSP/EngineSidechainHelper.h"
#include "../Standalone/ApvtsDirtyTracker.h"

// ── VibePlayerProcessor (user-facing name: BaySickPlayer) ────────────────────
// AudioProcessor wrapper for VibeSynth.
// Owns its own APVTS; param IDs follow the consolidated plan convention:
//   tk_{trackId}_bsp_{paramName}
//
// CPU safeguarding:
//   updateFromApvts() guards every setter with a cached last-value comparison.
//   Only calls the setter when the value actually changed.
// ─────────────────────────────────────────────────────────────────────────────
class VibePlayerProcessor : public juce::AudioProcessor,
                            public ISidechainEngine
{
public:
    explicit VibePlayerProcessor (const juce::String& trackId = "lay_0");

    // ── AudioProcessor interface ──────────────────────────────────────────────
    void prepareToPlay    (double sampleRate, int maxBlockSize) override;
    void releaseResources () override {}
    void processBlock     (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor () override;
    bool hasEditor ()           const override { return true; }

    const juce::String getName ()             const override { return "BaySickPlayer"; }
    bool   acceptsMidi ()                     const override { return true; }
    bool   producesMidi ()                    const override { return false; }
    bool   isMidiEffect ()                    const override { return false; }
    double getTailLengthSeconds ()            const override { return 2.0; }

    int  getNumPrograms ()                    override { return 1; }
    int  getCurrentProgram ()                 override { return 0; }
    void setCurrentProgram  (int)             override {}
    const juce::String getProgramName   (int) override { return "Default"; }
    void changeProgramName  (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest) override;
    void setStateInformation (const void* data, int sz) override;

    bool isBusesLayoutSupported (const BusesLayout&) const override;

    // ── Public interface ──────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;

    // 2026-05-05 dirty-flag wiring (see ApvtsDirtyTracker.h).
    void setOnAnyStateChange (std::function<void()> fn) { mDirtyTracker.onAny = std::move (fn); }

    // Engine access for the editor
    VibeSynth& getSynth() { return mSynth; }

    // Param prefix used for this instance
    const juce::String& getParamPrefix() const { return mPrefix; }
    juce::String pid (const char* name)  const { return mPrefix + name; }

    // QA-ClipPlayback close (perf): raw APVTS atom pointers for the timeline-WAV
    // control read (readClipCtl in PluginProcessor), resolved ONCE at construction.
    // That read runs per-row per-block on the render thread; resolving by string id
    // there heap-allocs (juce::String pid concat) + hashes every block.  Written
    // only in the ctor before any audio callback fires; const thereafter.
    struct ClipCtlPtrs
    {
        std::atomic<float>* volume { nullptr };   std::atomic<float>* pan { nullptr };
        std::atomic<float>* cutoff { nullptr };   std::atomic<float>* muffle { nullptr };
        std::atomic<float>* hardness { nullptr }; std::atomic<float>* res { nullptr };
        std::atomic<float>* drive { nullptr };    std::atomic<float>* reduct { nullptr };
        std::atomic<float>* lfoAmt { nullptr };   std::atomic<float>* lfoRate { nullptr };
        std::atomic<float>* treble { nullptr };   std::atomic<float>* stereo { nullptr };
        std::atomic<float>* attack { nullptr };   std::atomic<float>* decay { nullptr };
        std::atomic<float>* sustain { nullptr };  std::atomic<float>* release { nullptr };
        std::atomic<float>* tune { nullptr };     std::atomic<float>* detune { nullptr };
        std::atomic<float>* reverse { nullptr };  std::atomic<float>* sampleStart { nullptr };
        std::atomic<float>* stretch { nullptr };
    };
    const ClipCtlPtrs& clipCtlPtrs() const noexcept { return mClipCtlPtrs; }

    // Thread-safe note audition (UI thread → audio thread via atomic)
    void auditionNote    (int midiNote) { mAuditionNote.store    (midiNote); }
    void auditionNoteOn  (int midiNote) { mAuditionHoldOn.store  (midiNote); }
    void auditionNoteOff (int midiNote) { mAuditionHoldOff.store (midiNote); }

    // P3 persistence (2026-04-24): wrappers around VibeSampleManager's
    // loadFolder / loadSFZ / loadSingleFile that ALSO stash the loaded path
    // on `apvts.state` as non-APVTS properties.  setStateInformation replays
    // whichever one was last used so the samples come back on project load.
    //   loadKind property values: "folder" / "sfz" / "file"
    //   loadPath property value:  absolute path to the loaded asset
    // All editor code paths (drag-drop + Core Library menu + Drums slots)
    // go through these wrappers instead of reaching into the manager.
    //   normalizeRoot = optional MIDI note for VibeSampleManager::normalizeRootNotes
    //   (e.g. 60 for drum-slot conventions).  -1 = don't normalize.
    void loadSampleFolder  (const juce::File& folder,  int normalizeRoot = -1);
    void loadSampleSFZ     (const juce::File& sfzFile, int normalizeRoot = -1);
    void loadSampleFile    (const juce::File& wavFile, int normalizeRoot = -1);
    juce::File getLoadedSampleFile() const;   // returns whatever's stashed (folder/sfz/file)

    // C.4 Phase 2.2: engine-level SC primitive.
    void setSidechainBuffers (juce::AudioBuffer<float>* const* bufs, int count) noexcept override
    {
        mScHelper.setSidechainBuffers (bufs, count);
    }
    float getSidechainLevel() const noexcept override { return mScHelper.getLevel(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout
        createLayout (const juce::String& prefix);

    // CPU-guarded: reads APVTS and pushes to synth only when values changed.
    void updateFromApvts();

    VibeSynth        mSynth;
    juce::String     mPrefix;
    juce::String     mTrackId;
    std::atomic<int> mAuditionNote    { -1 };
    std::atomic<int> mAuditionHoldOn  { -1 };
    std::atomic<int> mAuditionHoldOff { -1 };
    EngineSidechainHelper mScHelper;   // C.4 Phase 2.2 SC primitive

    // Sub-M: scratch MIDI buffer for the keyswitch pre-scan filter.  Reused
    // across blocks - clear() at top of processBlock, addEvent to rebuild,
    // swapWith back into the input buffer.  No per-block allocation in steady
    // state (juce::MidiBuffer reuses capacity after clear()).
    juce::MidiBuffer mKeyswitchFilteredMidi;

    // ── CPU guard cache ───────────────────────────────────────────────────────
    struct ParamCache
    {
        float lfoAmt       { -1.f };
        float cutoff       { -1.f };
        float res          { -1.f };
        float drive        { -1.f };
        float reduct       { -1.f };
        float decay        { -1.f };
        float release      { -1.f };
        float pan          { -1.f };
        float volume       { -1.f };
        float stereo       { 9999.f };   // sentinel outside the bipolar -1..1 range
        float treble       { -1.f };
        float stretch      { -1.f };
        float muffle       { -1.f };
        float velToMuffle  { -1.f };
        float hardness     { -1.f };
        float velToHardness{ -1.f };
        float sensitivity  { -1.f };
        float tune         { 9999.f };
        float detune       { 9999.f };
        int   articGroup   { -1 };
        // S1 2026-04-21 additions
        float attack       { -1.f };
        float sustain      { -1.f };
        float lfoRate      { -1.f };
        float velToVolume  { -1.f };
        float sampleStart  { -1.f };
        int   voiceCap     { -1 };
        int   detuneMode   { -1 };
        // S1 Incr3 cache
        float cutSelf      { -1.f };   // bool as float (apvts bool stores 0/1)
        float reverse      { -1.f };
        int   unisonVoices { -1 };
        float unisonSpread { -1.f };
    } mCache;

    // QA-ClipPlayback close: resolved once in the ctor (see clipCtlPtrs()).
    ClipCtlPtrs mClipCtlPtrs;

    // 2026-05-05 dirty-flag wiring.  Declared LAST so apvts is fully constructed.
    ApvtsDirtyTracker mDirtyTracker { apvts };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VibePlayerProcessor)
};
