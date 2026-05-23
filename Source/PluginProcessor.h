#pragma once
#include <JuceHeader.h>
#include <map>
#include <set>
#include "VibesynthConstants.h"
#include "SynthVoice.h"
#include "SynthSound.h"
// 2026-04-25: DrumSynth.h + DrumVoice + BaySickDrumsProcessor + DrumsPage
// + BaySickDrumsEditor + BaySickDrumsLAF all deleted.  Drums now use the
// dynamic per-tab DrumPage model with BaySickPlayer / BaySickSynth engines.
#include "BassSynth.h"
#include "PatternManager.h"
#include "DSP/EQ8MsDSP.h"
#include "VibeGraph.h"
#include "MidiRecorder.h"
#include "AudioFileRecorder.h"
#include "DSP/AudioClipStreamer.h"
#include "DSP/PhaseVocoder.h"
#include "MidiLearn/MidiLearnRegistry.h"
// Multi-threaded render engine (Phase 1 scaffolding, 2026-05-06).  These
// members live for the lifetime of the processor; per-strip RenderTask
// wrappers ship in Batches 3-8.
#include "Engine/RenderEngineFlags.h"
#include "Engine/ChannelBufferArena.h"
#include "Engine/VibeThreadPool.h"
#include "Engine/RenderGraphDispatcher.h"
#include "Engine/Tasks/EngineInsertTask.h"   // Batch 3: Layer/Bass/Drum task wrappers
#include "Engine/Tasks/VoxStripTask.h"       // Batch 4: Vox live-input strip wrapper
#include "Engine/Tasks/InstStripTask.h"      // Batch 4: Inst source-mode-aware wrapper
#include "Engine/Tasks/CompositeAudioInsertTask.h"  // QA-0 (2026-05-07): per-row composite (replaces ClipPageTask + AudioInsertTask)
#include "Engine/Tasks/RustyDrumsProducerTask.h"   // Batch 6: drives processStrips
#include "Engine/Tasks/RustyInsertTask.h"          // Batch 6: per-strip insert wrapper
#include "Engine/Tasks/PassiveStripTask.h"         // Batch 7: aux + bus accumulator strips
#include "Engine/Tasks/MasterTask.h"               // Batch 8: terminal task (graph sink)
#include "Engine/RetirementQueue.h"                // Batch 9c B1: deferred-destruction GC

// ── APVTS parameter helper ────────────────────────────────────────────────────
// Creates a ParameterID with version 1
#define VID(id) juce::ParameterID{(id), 1}

class VibeSynthProcessor : public juce::AudioProcessor,
                           private juce::ValueTree::Listener   // §P4.3 perf: dirty-flag EQ sync
{
    // Multi-threaded render engine task wrappers (Batches 4+) read internal
    // per-tab state (FilePlay flags, sfizz-active atomics, idle counters,
    // kIdleSuspendBlocks) directly. Friend access avoids cluttering the
    // public API with accessors used only by the engine layer.
    friend class VoxStripTask;
    friend class InstStripTask;
    friend class CompositeAudioInsertTask; // QA-0 (2026-05-07): replaces old ClipPageTask + AudioInsertTask friends
    friend class RustyDrumsProducerTask;  // Batch 6
    friend class RustyInsertTask;         // Batch 6
    friend class PassiveStripTask;        // Batch 7
    friend class MasterTask;              // Batch 8
public:
    VibeSynthProcessor();
    ~VibeSynthProcessor() override;

    // ── AudioProcessor interface ──────────────────────────────────────────
    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BaySickDAW"; }
    bool   acceptsMidi()  const override { return true; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int  getNumPrograms()   override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ── Project persistence (P1, 2026-04-23) ─────────────────────────────────
    // Writes the full project state (APVTS + rack states + PatternManager tree)
    // as children of `root`.  Used by ProjectManager to produce project.xml.
    //   root must start empty; this fills it with <Processor>, <PatternManager>
    //   nodes + a "version" attribute.
    // Non-const because apvts.copyState() + VibeGraph::saveRackStates both
    // require non-const access; conceptually a read-only snapshot.
    void serializeProject  (juce::XmlElement& root);

    // Restores everything serializeProject wrote.  Caller is responsible for
    // syncing UI afterwards.  No-op if root is missing the expected children.
    void deserializeProject (const juce::XmlElement& root);

    // File > New reset (2026-04-24): restore every APVTS param to its
    // default value, clear VibeGraph rack states, and reset PatternManager.
    // Caller is responsible for tearing down + rebuilding dynamic tabs - this
    // method deals only with processor-owned state.
    void resetToBlankState();

    // 2026-04-24: deferred rack state replay.  deserializeProject stashes
    // VibeRackStates into mPendingProjectRackState and skips the immediate
    // apply, because per-insert racks (Layer / Bass / Drum / Audio / Aux /
    // Vox / Inst InsertNodes) don't exist yet - the editor creates them when
    // rebuilding tabs + audio strips.  The editor calls this at the END of
    // its load flow (after restoreAudioStripsFromArrangement), when every
    // InsertNode exists, so effect + EQ state actually reaches its target.
    void applyPendingRackStates();

    // P1+P2 persistence (2026-04-24): StandaloneEditor owns tab + engine state
    // (Harmless / BaySickSynth / VibePlayer / BaySickBass / BaySickDrums are
    // all per-page engine processors with their OWN apvts, not the main
    // VibeSynthProcessor::apvts).  These callbacks let serializeProject /
    // deserializeProject delegate that chunk of persistence up to the editor.
    //   onSerializeUIState  fired inside serializeProject - editor adds its
    //                       <UIState> child to the passed root element.
    //   onDeserializeUIState fired inside deserializeProject after the main
    //                       state has been restored - editor reads <UIState>
    //                       and rebuilds tabs + engines.
    std::function<void(juce::XmlElement&)>       onSerializeUIState;
    std::function<void(const juce::XmlElement&)> onDeserializeUIState;

    // P4: current project folder on disk, used to resolve relative
    // audioFilePath values like "Samples/<name>.wav" for audio-clip playback.
    // Set from ProjectManager after newProject / openProject / saveProjectAs.
    void setCurrentProjectFolder (const juce::File& folder);
    juce::File getCurrentProjectFolder() const;
    // Resolves a block's `audioFilePath`.  Absolute paths pass through (legacy
    // pre-P4 projects stored full paths).  Relative paths like "Samples/x.wav"
    // are resolved against the current project folder.
    juce::File resolveProjectFile (const juce::String& storedPath) const;

    // PatternManager is owned by StandaloneEditor today - the processor holds
    // a pointer so audio-thread scheduling can read it (see mPatternManager).
    // serializeProject/deserializeProject need it too; this setter is already
    // called by StandaloneEditor on startup via setPatternManager (P1 reuses).
    PatternManager* getPatternManager() const { return mPatternManager; }

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // ── Public accessors ──────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;

    BassSynth& getBassSynth() { return mBassSynth; }
    void       allNotesOff()  { mSynth.allNotesOff(1, false); }

    // Request the audio thread to flush every engine's active voices at the
    // start of the next processBlock (sends CC 123 All-Notes-Off + explicit
    // per-note offs for anything in mPRPendingOffs). Thread-safe trigger:
    // set the flag from the UI, clear on the audio thread after handling.
    std::atomic<bool> mFlushAllNotes { false };

    // End-of-song beat (set by UI when song mode is active + no time sel).
    // Audio thread checks crossing and requests transport stop via
    // mRequestStop. 0 = no auto-stop.
    std::atomic<double> mSongEndBeats { 0.0 };
    std::atomic<bool>   mRequestStop  { false };
    // True = loop back to 0 at mSongEndBeats; false = auto-stop at song end.
    // 2026-04-26: default flipped to true so empty Song mode loops the 1-bar
    // audition instead of immediately auto-stopping.  Matches the matching
    // default in `GlobalTransportBar::mSongLoopBtn`.
    std::atomic<bool>   mSongLoopMode { true };

    // Mixer state -- read by processBlock from PatternManager when available
    // Also settable directly for VST mode (no PatternManager)
    struct MixLevels {
        float master { 1.0f };
        float layers { 1.0f };
        float bass   { 1.0f };
        float drums  { 1.0f };
        bool  layersMute { false };
        bool  bassMute   { false };
        bool  drumsMute  { false };
        bool  layersSolo { false };
        bool  bassSolo   { false };
        bool  drumsSolo  { false };
    };
    MixLevels mixLevels;

    // Standalone: inject PatternManager reference so processBlock can
    // read mixer state and sequence data directly.
    void setPatternManager(PatternManager* pm) { mPatternManager = pm; }

    // Rebuild AudioFormatReaders for all audio clips in the arrangement.
    // Call from the message thread after importing audio or changing the arrangement.
    void rebuildAudioClipPlayers();

    // ── 5F-4a: Audio-row mixer strip registration ────────────────────────────
    // Called when an audio clip is first placed on a new arrangement row.
    // Creates the `mixer_audio_{row}` APVTS params (if missing) and the Audio
    // InsertNode in VibeGraph. Safe to call repeatedly.
    void ensureAudioInsert(int row, const juce::String& displayName);

    // ── 5F-4b B2: Aux/Group strip registration ──────────────────────────────
    // Creates an Aux strip at the given idx (0..15). No audio source - aux is
    // receive-only; its input is populated by sends from other strips.
    // Default main-out = FX Bus. Safe to call repeatedly for the same idx.
    void ensureAuxInsert(int idx, const juce::String& displayName);

    // QA-Ef #4 (2026-05-22): tear down EVERY registered aux insert -- unregister
    // each PassiveStripTask from the render dispatcher, reset the task slot, and
    // clear the VibeGraph aux InsertNodes.  Called from the three load-entry
    // points BEFORE restoreAuxStripsFromState rebuilds from the loaded project,
    // so auxes from the prior session don't leak across loads:
    //   - VibeSynthProcessor::deserializeProject (project open)
    //   - VibeSynthProcessor::setStateInformation (VST3 host load)
    //   - StandaloneEditor::doFileNew (File > New)
    //   - StandaloneEditor::loadTemplate (apply template)
    // Each caller raises mProjectLoadInProgress + sleeps 30 ms BEFORE calling
    // this so the audio thread is bailing while we mutate the render-task list
    // (processBlock bails to silence while the shield is up).  Mirrors how
    // onTabClosed tears down per-tab engines; aux strips have no per-tab
    // teardown because they aren't tabs.
    void clearAllAuxInserts();

    // R1 (2026-04-23): Vox / Inst strip registration for live-input recording.
    // Up to 6 of each; default main-out = their respective bus (VoxBus / InstBus).
    // R1 registers params + InsertNode only; R2+R3 wire the ASIO input source.
    void ensureVoxInsert  (int idx, const juce::String& displayName);
    void ensureInstInsert (int idx, const juce::String& displayName);

    // J-5 (2026-05-03): BaySickRustyDrums strip registration.  One strip per
    // sound type (kick / snare / tom / hi-hat / ride / crash / china / stack -
    // 13 total for Big Rusty Drums).  Default main-out = kRustyDrumsBus.
    void ensureRustyInsert (int idx, const juce::String& displayName);
    void removeRustyInsert (int idx);

    // J-5: BaySickRustyDrums singleton lifecycle.  PluginProcessor owns the
    // singleton sfizz-driven engine; only one instance allowed per project.
    bool                          hasBaySickRustyDrums() const noexcept;
    class BaySickRustyDrumsProcessor* getBaySickRustyDrums() noexcept;
    // Creates the singleton if absent + loads the kit at sfzPath.  On success:
    // discovers 13 channels, registers 13 strips at kRustyBase..kRustyBase+12,
    // returns true.  Idempotent - calling twice with same path no-ops cleanly.
    bool loadBaySickRustyDrumsKit (const juce::File& sfzPath);
    // Tear-down: free engine + remove all 13 InsertNodes.  APVTS params
    // persist as zombies (existing pattern - JUCE doesn't allow unregister).
    void destroyBaySickRustyDrums();
    // J-8 Part C (2026-05-04): reset every `mixer_rusty_*` + `mixer_rustybus_*`
    // APVTS param to its registered default value.  Called by the page when
    // the user switches programs - wipes mixer state so the new kit's
    // freshly-spawned strips start clean (no stale level/pan/mute/EQ from
    // the previous program).
    void resetBaySickRustyDrumsMixerState();

    // K-2 (2026-05-05): BaySickGuitars per-instance lifecycle.  Each Inst page
    // whose source = BaySickGuitars owns one slot in the per-instance array.
    // PluginProcessor owns the engines so it can drive them on the audio thread
    // alongside the existing Inst chain dispatch.  The `mGuitarsActive[idx]`
    // atomic gates audio-thread access during kit-load (sfizz hash maps are
    // mutated by loadSfzFile and not safe to read concurrently with renderBlock).
    class BaySickGuitarsProcessor* getBaySickGuitars (int instIdx) noexcept;
    // Creates the engine at slot `instIdx` if absent + loads the kit at sfzPath.
    // Active flag dance: false → drain audio thread → loadKit → true.  Returns
    // true on success.  Idempotent - calling with same path on a loaded slot
    // no-ops cleanly.
    bool loadBaySickGuitarsKit (int instIdx, const juce::File& sfzPath);
    // Tear-down: drop the engine at slot `instIdx` (clears the atomic first).
    // APVTS params persist as zombies (JUCE doesn't allow unregister) - they
    // simply have no listener once the engine is gone.
    void destroyBaySickGuitars (int instIdx);

    // L-2 (2026-05-05): BaySickBasses per-instance lifecycle.  Mirrors the
    // Guitars pattern above - separate per-slot arrays so a single Inst tab
    // can be in only one source mode at a time + the engines coexist when
    // multiple tabs of different source modes are open.
    class BaySickBassesProcessor* getBaySickBasses (int instIdx) noexcept;
    bool loadBaySickBassesKit (int instIdx, const juce::File& sfzPath);
    void destroyBaySickBasses (int instIdx);

    // R2 (2026-04-23): live-input ASIO channel persistence.  Index lives in
    // APVTS as `<prefix>_inputChannelIdx` (Int -1..127, default -1).  The
    // channel NAME (e.g. "Mic 1") is stored as a non-APVTS attribute on
    // apvts.state via setInputChannelName/getInputChannelName so on device
    // reconnect we can re-resolve by name first, fall back to index.
    void         setInputChannelName (const juce::String& stripPrefix,
                                       const juce::String& name);
    juce::String getInputChannelName (const juce::String& stripPrefix) const;

    // 5F-4b B7 / QA-Ef #4 (2026-05-22): scan a SAVED-FILE state tree for
    // mixer_aux_N_level params and re-register their InsertNodes.  The caller
    // MUST pass a deep copy of the saved tree taken BEFORE apvts.replaceState,
    // not apvts.state itself -- replaceState's rebind appends stale empty
    // <PARAM> nodes for any params that were registered in a prior session but
    // are missing from this file (e.g. open AT1 with 3 auxes, then open AT2
    // with 1 -- apvts.state ends up with mixer_aux_1/2_level nodes too because
    // those adapters are still registered).  Scanning apvts.state would
    // recreate phantom auxes; scanning the pre-rebind file copy only finds
    // auxes that were ACTUALLY saved.
    void restoreAuxStripsFromState (const juce::ValueTree& sourceState);

    // ── C.3 (2026-04-30): Hardware MIDI input ────────────────────────────────
    // The MIDI input thread (StandaloneApp::handleIncomingMidiMessage) pushes
    // every message to mLiveMidiCollector.  processBlock drains the collector
    // each block and routes its messages into the engine page-buffer named by
    // mLiveMidiTargetKind/mLiveMidiTargetIndex (the Piano Roll page's currently
    // focused engine, pushed via setLiveMidiTarget when focus changes).
    //
    // Target encoding (matches PianoRollPage::EngineKind ordering):
    //   1 = Layer (mLiveMidiTargetIndex = 0..kMaxLayerPages-1)
    //   2 = Bass  (mLiveMidiTargetIndex = 0..kMaxBassPages-1)
    //   3 = Drum  (mLiveMidiTargetIndex = 0..kMaxDrumPages-1)
    //   anything else (0 DrumKit grid / 4 Clip / 5 Vox / 6 Inst / -1 unset)
    //   = drop incoming messages (no target).  Q3 spec call locks Vox/Inst out.
    juce::MidiMessageCollector& getLiveMidiCollector() noexcept { return mLiveMidiCollector; }
    void setLiveMidiTarget (int engineKind, int index) noexcept
    {
        mLiveMidiTargetKind .store (engineKind, std::memory_order_relaxed);
        mLiveMidiTargetIndex.store (index,      std::memory_order_relaxed);
    }

    // ── I-3b (2026-05-02): MIDI Learn registry ───────────────────────────────
    // App-wide MIDI Learn mapping table.  Audio thread reads it during
    // processBlock to dispatch incoming hardware CC / pitch-bend / aftertouch
    // events to APVTS params.  Message thread (UI from I-3c) mutates via the
    // registry's set/remove/learn methods.  Persisted in project XML as a
    // <MidiCCMappings> child of getStateInformation; also overlay-loaded at
    // app startup from Documents/BaySickDAW/MidiMappings.xml (global defaults).
    MidiLearnRegistry&  getMidiLearnRegistry()  noexcept { return mMidiLearn; }
    MidiLearnEventQueue& getMidiLearnEventQueue() noexcept { return mMidiLearnQueue; }

    // ── Latency ───────────────────────────────────────────────────────────
    // Device output latency (set by StandaloneApp when device initialises or changes).
    void setDeviceOutputLatency (int samples) { mDeviceOutputLatency.store (samples); }
    // 2026-05-02: read-only accessor for the meter latency-compensation toggle
    // (Mixer hamburger menu) -- returns the most recent driver latency in samples.
    int  getDeviceOutputLatency() const noexcept { return mDeviceOutputLatency.load(); }
    // Total latency = PDC (getLatencySamples) + audio device output latency.
    int  getTotalOutputLatency() const
         { return getLatencySamples() + mDeviceOutputLatency.load(); }
    double getSampleRate() const { return mSampleRate; }

    // ── Lazy APVTS registration (Phase 2+) ───────────────────────────────────
    // Called when a Layers/Bass/Drums page is created or destroyed.
    // trackId format: "tk_{index}_{engine}" e.g. "tk_0_Harmless", "tk_2_BaySickBass"
    // Engine types: "Harmless", "BaySickPlayer", "BaySickSynth", "BaySickBass", "BaySickDrums"
    void registerParamsForTrack  (const juce::String& trackId, const juce::String& engineType);
    void unregisterParamsForTrack(const juce::String& trackId);
    bool isTrackRegistered       (const juce::String& trackId) const;

    // ── Engine processor registration (audio thread rendering) ───────────────
    // Called from LayersPage / BassPage on the message thread.
    // The processor must remain alive until unregister is called.
    void registerLayerEngine  (int pageIdx, juce::AudioProcessor* eng);
    void unregisterLayerEngine(int pageIdx);
    void registerBassEngine   (int pageIdx, juce::AudioProcessor* eng);
    void unregisterBassEngine (int pageIdx);
    // 2026-04-25: registerDrumsEngine / unregisterDrumsEngine removed
    // (legacy 16-slot processor deleted).
    // D1.2: per-drum-page engine registration (dynamic-drum model).  Mirrors
    // registerLayerEngine - also creates the Drum InsertNode + mixer strip
    // params for that drum's pageIdx.
    void registerDrumEngine  (int pageIdx, juce::AudioProcessor* eng);
    void unregisterDrumEngine(int pageIdx);

    // G-3 (2026-04-28): per-clip-page engine registration.  pageIdx here is
    // the audio-row index for the bound clip (1:1 mapping - each Clips tab
    // claims one of the 50 mixer_audio_<row> inserts on spawn).  Engine
    // output is mixed into THAT row's audio insert during processBlock so
    // arrangement-playback audio + piano-roll-triggered audio share the
    // same EffectRack / EQ / fader chain.  No new mixer strip is created.
    void registerClipEngine  (int pageIdx, juce::AudioProcessor* eng);
    void unregisterClipEngine(int pageIdx);

    // G-4 (2026-04-28): per-Vox / per-Inst page engine registration.  pageIdx
    // is the Vox / Inst insert index (1:1 with mixer_vox_<idx> / mixer_inst_
    // <idx>).  Same shape as registerClipEngine - engine output mixes into
    // the existing Vox / Inst InsertNode (created when the user clicks "Add
    // Vox/Inst Strip" on the Mixer page).
    void registerVoxEngine   (int pageIdx, juce::AudioProcessor* eng);
    void unregisterVoxEngine (int pageIdx);
    void registerInstEngine  (int pageIdx, juce::AudioProcessor* eng);
    void unregisterInstEngine(int pageIdx);

    // §P4.3 B7 (2026-04-22): per-page pre-rack EQ register/unregister APIs +
    // mDrumsEQDSP / mLayerPageEQs / mBassPageEQs / mDrumsPageEQ members all
    // deleted.  Pre-rack EQs now live on InsertNode / BusNode preEq members
    // inside VibeGraph; pages bind their EQ display to those via
    // VibeGraph::getInsertPreEQ() / getXxxBusPreEQ() + the mixer strip APVTS
    // prefix (mixer_{kind}_<N>_preeq_*).

    // ── EQ spectrum feed type (defined in DSP/SpectrumFeed.h, alias kept for compat) ──
    using EQSpectrumFeed = VibeGraph::SpectrumFeed;

    // ── Level meter feeds (audio thread writes, UI timer reads) ───────────────
    // Peak dB for each mix section - used by MixerPage strip meters.
    // 2026-04-30: stereo L/R atomics added alongside the mono atomics for
    // the new split DBFSMeter.  The mono atomics are still written (max(L,R))
    // for legacy readers.  UI calls the new stereo getters below.
    std::atomic<float> mMasterPeakDb        { -60.0f };
    std::atomic<float> mMasterPeakDbL       { -60.0f };
    std::atomic<float> mMasterPeakDbR       { -60.0f };
    std::atomic<float> mLayersPeakDb        { -60.0f };
    std::atomic<float> mLayersPeakDbL       { -60.0f };
    std::atomic<float> mLayersPeakDbR       { -60.0f };
    std::atomic<float> mBassPeakDb          { -60.0f };
    std::atomic<float> mBassPeakDbL         { -60.0f };
    std::atomic<float> mBassPeakDbR         { -60.0f };
    std::atomic<float> mDrumsPeakDb         { -60.0f };
    std::atomic<float> mDrumsPeakDbL        { -60.0f };
    std::atomic<float> mDrumsPeakDbR        { -60.0f };
    std::atomic<float> mAudioClipsBusPeakDb { -60.0f };
    std::atomic<float> mAudioClipsBusPeakDbL{ -60.0f };
    std::atomic<float> mAudioClipsBusPeakDbR{ -60.0f };
    // R3.5 (2026-04-23): Vox + Inst bus peaks (UI mixer-strip meters).
    std::atomic<float> mVoxBusPeakDb        { -60.0f };
    std::atomic<float> mVoxBusPeakDbL       { -60.0f };
    std::atomic<float> mVoxBusPeakDbR       { -60.0f };
    std::atomic<float> mInstBusPeakDb       { -60.0f };
    std::atomic<float> mInstBusPeakDbL      { -60.0f };
    std::atomic<float> mInstBusPeakDbR      { -60.0f };
    // C.1 (2026-04-30): FX Bus peak - written by VibeGraph::processEffectsBus
    // each block (mirrors the EffectsBusNode internal atomics so MixerPage
    // can read alongside its peers without reaching into VibeGraph internals).
    std::atomic<float> mFxBusPeakDb         { -60.0f };
    std::atomic<float> mFxBusPeakDbL        { -60.0f };
    std::atomic<float> mFxBusPeakDbR        { -60.0f };
    // G-6 (2026-04-29): secondary bus peak meters.
    std::atomic<float> mVoxBus2PeakDb       { -60.0f };
    std::atomic<float> mVoxBus2PeakDbL      { -60.0f };
    std::atomic<float> mVoxBus2PeakDbR      { -60.0f };
    std::atomic<float> mInstBus2PeakDb      { -60.0f };
    std::atomic<float> mInstBus2PeakDbL     { -60.0f };
    std::atomic<float> mInstBus2PeakDbR     { -60.0f };
    std::atomic<float> mInstBus3PeakDb      { -60.0f };
    std::atomic<float> mInstBus3PeakDbL     { -60.0f };
    std::atomic<float> mInstBus3PeakDbR     { -60.0f };
    // J-7b (2026-05-04): RustyDrums Bus peak meter - written by the bus
    // pipeline post-fader/pan, drained at end of block alongside every
    // other bus meter so the UI strip sees signal activity.  Run variants
    // declared below alongside the other *Run atomics.
    std::atomic<float> mRustyDrumsBusPeakDb     { -60.0f };
    std::atomic<float> mRustyDrumsBusPeakDbL    { -60.0f };
    std::atomic<float> mRustyDrumsBusPeakDbR    { -60.0f };

    // 2026-05-02: running-max companion atomics.  Audio thread CAS-maxes into
    // these during processBlock; a single end-of-block promotion lifts them
    // into the UI-visible atomics above.  Result: every meter (every bus,
    // every audio row) is end-of-block coherent so a UI vblank firing at any
    // time sees a consistent snapshot across all of them, not "this bus
    // updated, that one not yet".  Default to -inf so a "no audio writes
    // this block" case promotes nothing (skip-on-INF inside the promote
    // helper) and the existing snapshot keeps decaying via UI ballistics.
    static constexpr float kPeakAtomicNegInf = -std::numeric_limits<float>::infinity();

    // ── 1M: Audio DSP load monitoring (audio thread writes, UI timer reads) ──
    // mAudioDspLoad : smoothed fraction of buffer window used by processBlock (0..1)
    // mDspOverload85: true when load has been >85% for >500 ms (voice steal fired)
    // mDspOverload95: true when current smoothed load >95% (UI red flash)
    std::atomic<float> mAudioDspLoad  { 0.0f };
    std::atomic<bool>  mDspOverload85 { false };
    std::atomic<bool>  mDspOverload95 { false };

    // ── Audio clip playback (song mode) ──────────────────────────────────────
    juce::AudioFormatManager  mAudioFormatManager;
    juce::TimeSliceThread     mAudioFileThread { "AudioClipBG" };

    struct AudioClipPlayer {
        double clipStartBeat  = 0.0;
        double clipEndBeat    = 0.0;
        double fileSampleRate = 44100.0;
        int    trackRow       = 0;
        float  originalBPM    = 120.f;
        bool   stretchMode    = true;
        // I-16 G-9 (2026-05-03): when non-zero AND clipType==Audio, the player
        // routes its samples through the Vox/Inst page identified by this
        // mixer channel id (e.g., MixerChannelIds::voxInsert(0)) instead of
        // through a new Audio row.  Copied from ArrangementBlock at
        // rebuildAudioClipPlayers() time.
        int    routeChannel   = 0;
        // D3: choke group (0 = none, 1..16).  Copied from the source clip's
        // AudioLibraryEntry at rebuildAudioClipPlayers() time so the audio
        // thread can read it without a library lookup.
        int    chokeGroup     = 0;
        // D3: true once a peer in the same choke group fires during this clip's
        // current playback.  Reset to false each block when the playhead is
        // outside the clip range (so a fresh playthrough starts un-choked).
        bool   mutedByChoke   = false;
        // QA-Ea Task 0c (FL pre-roll record + non-destructive clip trim):
        // file-position offset added to every file-position read in the
        // audio-clip render loop (direct-read filePos, phase-vocoder
        // reference pvRefPos, file-EOF guard fileTotalSamples) so the clip
        // plays from sample N of the file rather than 0.  Copied from
        // ArrangementBlock::contentStartSamples at rebuildAudioClipPlayers
        // time.  Default 0 = play from file sample 0 (every pre-Task-0c
        // clip is unaffected; backwards-compatible).
        juce::int64 contentStartSamples { 0 };
        // Disk-streaming reader - background thread pre-fetches, audio thread reads.
        std::unique_ptr<AudioClipStreamer> streamer;
        // Phase vocoder for BPM-aware time stretch (null when stretchMode=false).
        std::unique_ptr<PhaseVocoder> vocoder;
        // Pre-allocated scratch buffers used by the audio thread (no heap alloc in processBlock).
        juce::AudioBuffer<float> pvInBuf;   // raw file samples fed into vocoder
        juce::AudioBuffer<float> pvOutBuf;  // stretched output from vocoder (file SR)
        // Expected next file position - used to detect seeks and reset vocoder.
        int64 expectedFilePos { 0 };
    };
    // 2026-05-06 (Batch 9c B1): deferred-destruction GC pattern.
    //
    // Audio thread reads the active snapshot lock-free at the top of
    // processBlock (mActiveAudioClips.load -> mCurrentBlockClipSnapshot)
    // and uses the same pointer for the entire block (every iteration site
    // + every RenderTask reads mCurrentBlockClipSnapshot->players).
    //
    // Mutator (rebuildAudioClipPlayers, message thread) builds a fresh
    // AudioClipSnapshot, atomic-exchanges it into mActiveAudioClips, and
    // retires the old one to mClipRetirement with retiredBeforeGen set to
    // the new snapshot's generation.  The drainer thread (owned by
    // RetirementQueue) destroys the old snapshot once the audio thread has
    // demonstrably moved past the retire point (mAudioInUseClipGen >=
    // retiredBeforeGen) -- guaranteeing the slow ~AudioClipStreamer (file
    // close + TimeSliceClient unregister) NEVER runs on the audio thread.
    //
    // Replaces the pre-9c per-site try-lock pattern (audio bailed silent
    // for a block under MT contention) and the reverted a19c6e3 "lock for
    // whole block" snapshot (which deadlocked with audio->Win32 GUI calls
    // -- closed by the B2 fix in 3b2c85a).
    struct AudioClipSnapshot
    {
        std::vector<AudioClipPlayer> players;
        std::uint64_t                generation { 0 };
    };

    // Atomically published from the message thread; read once per audio
    // block via load-acquire.  The pointer's target is owned by whoever
    // last took it OUT of the atomic (initial bootstrap in the ctor;
    // mutator's exchange returns the previous value to the retirement
    // queue; ~VibeSynthProcessor explicitly deletes the final value).
    std::atomic<AudioClipSnapshot*> mActiveAudioClips  { nullptr };

    // Audio thread writes (release-store) at the top of every processBlock.
    // RetirementQueue drainer reads (acquire-load) to decide which retired
    // snapshots are safe to destroy.
    std::atomic<std::uint64_t>      mAudioInUseClipGen { 0 };

    // Mutator's monotonic generation counter.  Each rebuild assigns
    // gen = mNextClipGen.fetch_add(1) + 1, so the first published snapshot
    // beyond the bootstrap (gen 0) gets gen 1.
    std::atomic<std::uint64_t>      mNextClipGen       { 0 };

    // GC queue for retired AudioClipSnapshot instances.  Owns its own
    // dedicated drainer thread (see Engine/RetirementQueue.h).
    RetirementQueue<AudioClipSnapshot> mClipRetirement;

    // Audio-thread-only.  Captured at the top of processBlock (under the
    // load-acquire of mActiveAudioClips) and consumed by every iteration
    // site for the rest of that block: FilePlay pre-scan, Pass 2
    // song-mode loop, applyChokeGroupDispatch (both sub-loops), the
    // shared renderAudioClipsForRow / renderFilePlayPlayer helpers, plus
    // the AudioInsertTask / VoxStripTask / InstStripTask MT workers.
    // Visibility to MT workers is established by the dispatcher's
    // notify/wait release-acquire pair (workers wake AFTER the audio
    // thread has written this).
    AudioClipSnapshot* mCurrentBlockClipSnapshot { nullptr };

    // ── Batch 5 (2026-05-06): shared audio-clip render path ──────────────────
    // Per-block context bundle passed into renderAudioClipsForRow.  Keeps the
    // helper signature short while preserving access to all the per-block
    // values the render needs.
    struct AudioClipBlockContext
    {
        double      bpm              = 120.0;
        bool        anySolo          = false;
        double      secPerBeat       = 0.5;
        juce::int64 projectStart     = 0;
        juce::int64 projectEnd       = 0;
        int         numSamples       = 0;
        int         numOut           = 2;
        float       masterGain       = 1.0f;
        const MixerState* mxState    = nullptr;   // PatternManager.h top-level struct
        juce::AudioBuffer<float>* clipScratch = nullptr;   // shared decode buffer
    };

    // Render all non-FilePlay audio clips on `row` through the row's Audio
    // InsertNode, adding each clip's processed output into `mtDest` (the row
    // InsertNode's pull-model output buffer; always non-null).  Called per
    // audio row by CompositeAudioInsertTask::run.  FilePlay clips (clip routed
    // to a Vox/Inst page) are skipped - handled by the FilePlay pass in
    // processBlock.
    void renderAudioClipsForRow (int row,
                                 const AudioClipBlockContext& ctx,
                                 juce::AudioBuffer<float>* mtDest);

    // 2026-05-06 (Batch 9b Item 9): per-clip FilePlay rendering helper.
    // Decodes the clip portion that falls in this block (phase vocoder OR
    // direct-path SR-interp), copies into the matching Vox/Inst engine's
    // scratch, runs eng->processBlock + processInsert + routes the output.
    //
    // 2026-05-06 (Batch 9c B1): caller MUST be iterating over
    // mCurrentBlockClipSnapshot->players (or have already obtained the
    // player reference from there).  The snapshot is captured at the top
    // of processBlock and is guaranteed alive for the duration of the
    // block via the RetirementQueue ack protocol.  Caller MUST also have
    // already filtered the player by FilePlay status (player.routeChannel
    // must be a Vox or Inst id).
    //
    // engineMidi: per-page MIDI buffer for the Vox/Inst page identified by
    // player.routeChannel (e.g. mCtx->voxPageMidi[mIndex] from a VoxStripTask).
    //
    // mtDest: the strip's pull-model output buffer (always non-null).  Engine
    //         output is addFrom'd into it so the task graph's downstream
    //         consumers see it on this strip's mOutputBuffer.
    //
    // Returns true if a clip portion was rendered (engine was driven).
    // Returns false if the clip is out of range / muted / EOF / etc.
    //
    // QA-E Task 3 follow-up (2026-05-12): engineScratch parameter added so
    // the caller provides the engine-input buffer rather than the function
    // touching shared members.  VoxStripTask / InstStripTask each pass their
    // own per-task scratch (no cross-strip race).
    bool renderFilePlayPlayer (AudioClipPlayer&             player,
                                const AudioClipBlockContext& ctx,
                                juce::MidiBuffer&            engineMidi,
                                juce::AudioBuffer<float>*    mtDest,
                                juce::AudioBuffer<float>&    engineScratch);

    // Per-row peak dB for audio strip meters (audio thread writes, UI timer reads).
    // kMaxAudioRows == 50 (matches MixerState::kMaxAudioRows).
    static constexpr int kMaxAudioRows = 50;
    std::atomic<float> mAudioRowPeakDb [kMaxAudioRows];
    // 2026-04-30: stereo L/R for split DBFSMeter (UI reads via mProcessor).
    std::atomic<float> mAudioRowPeakDbL[kMaxAudioRows];
    std::atomic<float> mAudioRowPeakDbR[kMaxAudioRows];
    // 2026-05-02: running-max companion atomics for audio rows.  Audio
    // thread CAS-maxes during processBlock; promotion at end of block lifts
    // them into the UI-visible atomics above so all meters update coherently.
    std::atomic<float> mAudioRowPeakDbRun [kMaxAudioRows];
    std::atomic<float> mAudioRowPeakDbLRun[kMaxAudioRows];
    std::atomic<float> mAudioRowPeakDbRRun[kMaxAudioRows];

    // ── Graph infrastructure (Phase 1A) ───────────────────────────────────────
    VibeGraph mVibeGraph;

    // ── Recording (1G MIDI / 1H Audio) ────────────────────────────────────────
    // R5d (2026-04-24): rewritten to support mode-aware capture.
    //   - Audio mode: if any Vox / Inst strip is armed, each one's pre-rack
    //     input is tapped into its own WAV; otherwise the master output is
    //     captured (fallback).
    //   - MIDI mode: only mMidiRecorder runs; no WAVs.
    // StandaloneEditor owns all target-commit logic (drop WAV on arrangement
    // row, drop notes into the last-accessed piano roll); this class only
    // opens / closes writers and returns a result struct.
    MidiRecorder      mMidiRecorder;

    enum class RecordMode { Audio, Midi };

    struct StripRecorder
    {
        int                                channelId;       // MixerChannelIds
        juce::String                       displayName;     // e.g. "Vox 1"
        juce::File                         file;            // dry file (RAW pre-chain ASIO)
        std::unique_ptr<AudioFileRecorder> recorder;        // dry recorder

        // I-16 G-9 (2026-05-03): Vox-only second writer for the WET tap.
        // BaySickVocalProcessor pushes post-realtime / pre-vocal-chain audio
        // here.  Inst strips leave wetRecorder null (no realtime stage to
        // bake into a wet capture).
        juce::File                         wetFile;
        std::unique_ptr<AudioFileRecorder> wetRecorder;
    };

    struct RecordResult
    {
        juce::File                                masterFile;     // only when no strips armed
        std::vector<std::pair<int, juce::File>>   stripFiles;     // per-strip dry captures
        std::vector<std::pair<int, juce::File>>   stripWetFiles;  // I-16 G-9: per-strip wet (Vox only)
        std::vector<PianoNote>                    midiNotes;      // from MidiRecorder
        double                                    startBeat { 0.0 };
        // QA-Ea Task 0c (FL pre-roll record + non-destructive clip trim):
        // count-in samples captured before transport-start (the FL pre-roll
        // period).  commitRecordingResult uses this to set every resulting
        // Audio block's contentStartSamples + shorten its lengthBeats so the
        // visible clip starts at the song downbeat (not file sample 0) while
        // the WAV still holds the full pre-roll bar.  MIDI commit shifts
        // captured note startBeats by the beat-equivalent and applies the
        // Noodling-discard + Early-Strike-clamp + input-quantize rules.
        // Zero when no count-in fired (existing behavior unchanged).
        juce::int64                               preRollSamples { 0 };
    };

    // Called from StandaloneEditor onPlay when Record is armed.  Allocates
    // writers per mode + per armed strip; safe no-op if already recording.
    //   projectName / samplesFolder come from ProjectManager (empty project
    //   name is handled by caller - it blocks with a save-first prompt).
    void startRecording (RecordMode mode,
                         double startBeat,
                         const juce::String& projectName,
                         const juce::File& samplesFolder);

    // Called on Pause or Stop.  Closes all writers + returns the captured
    // files + MIDI notes + the startBeat for arrangement-row placement.
    RecordResult stopRecording();

    // 2026-05-06 (Batch 9b Item 8): dry-recorder tap helper.  Locates the
    // StripRecorder whose channelId matches and writes one block of mono
    // input (typically a single channel of mLiveInputSnapshot).  Called from
    // VoxStripTask / InstStripTask::run on the audio thread when those strips
    // are armed.  Pre-existing race hazard between iteration here and
    // message-thread mutation in startRecording / stopRecording - documented
    // in 9b, not closed.  Future hardening would either move mStripRecorders
    // to shared_ptr-backed entries or use a small SpinLock around iteration;
    // deferred.
    //
    // monoSource: read-only mono input pointer.  Typical usage:
    //   mProcessor->tapDryRecorder (channelId,
    //                               ctx->liveInputSnapshot->getReadPointer (chIdx),
    //                               n)
    void tapDryRecorder (int channelId, const float* monoSource, int numSamples);

    // 2026-05-07 (Batch 9c follow-up): end-of-block UI-meter atomic drain.
    // Promotes per-node / per-bus / per-row peak atomics into the UI-visible
    // mirror atomics that the editor's timer reads.  Called once per block
    // from processBlock (after dispatchBlock returns) so dBFS / VU / per-
    // effect meters get fed from the worker-thread peak writes.
    void drainMeterAtomicsForUI();

    // 2026-05-07 (Batch 10): DSP-load measurement + overload protection.
    // Called once per block from processBlock after dispatchBlock returns,
    // so mAudioDspLoad reflects the actual cost of running worker tasks via
    // runUntilOrTimeout.  Voice-stealing on sustained 85% overload also
    // lives here.  Caller passes the t0 tick captured at the very top of
    // processBlock.
    void measureDspLoadAndOverload (juce::int64 t0Ticks, int numSamples);

    // 2026-05-18 (QA-Ea Task 0b): post-mix recorders + metronome/count-in.
    // Called once per block from processBlock after dispatchBlock returns;
    // feeds master + MIDI recorders and runs the metronome / count-in.
    // Pre-QA-Ef the 3 hardware-MIDI / master-rec / metronome paths lived
    // only in the deleted serial tail; the extraction landed during QA-Ea
    // Task 0b so the single render path inherits them (Forks #25 close-out).
    // Order preserves the D-5 invariant: MIDI rec -> master rec (the
    // pre-metronome buffer) -> metronome / count-in.  bpm derives from the
    // passed playhead position.
    void applyPostMixRecordAndMetro (juce::AudioBuffer<float>& buffer,
                                     const juce::MidiBuffer& allMidi,
                                     const juce::AudioPlayHead::PositionInfo& pos,
                                     int numSamples);

    bool isRecording() const
    {
        return mMidiRecorder.isRecording() || mMasterRecorder.isRecording()
            || ! mStripRecorders.empty();
    }

private:
    AudioFileRecorder              mMasterRecorder;        // master-output fallback
    std::vector<StripRecorder>     mStripRecorders;        // per-armed-strip WAVs
    std::atomic<RecordMode>        mRecordMode { RecordMode::Audio };
    double                         mRecordStartBeat { 0.0 };
    // QA-Ea Task 0c (FL pre-roll record): count-in samples accumulated
    // during the current Record session.  applyPostMixRecordAndMetro
    // fetch_adds numSamples while isRecording() && mMetro.countInActive;
    // startRecording zeros it; stopRecording exchanges it into
    // RecordResult::preRollSamples.  One global counter applies to master
    // AND every strip block created by the session (Task 0c strip-recorder
    // scope, plan spec line 120).
    std::atomic<juce::int64>       mPreRollSamples { 0 };
public:

private:
    // ── APVTS layout builder ──────────────────────────────────────────────
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ── DSP engines ───────────────────────────────────────────────────────
    juce::Synthesiser mSynth;       // polyphonic synth (piano roll playback)
    BassSynth         mBassSynth;

    // ── Per-page engine processors (message thread sets, audio thread reads) ──
    juce::SpinLock                                         mLayerEngineLock;
    std::array<juce::AudioProcessor*, kMaxLayerPages>      mLayerEngines {};
    juce::SpinLock                                         mBassEngineLock;
    std::array<juce::AudioProcessor*, kMaxBassPages>       mBassEngines {};
    juce::AudioBuffer<float>                               mAudioRowScratch;    // bus accumulation (all clips summed)
    juce::AudioBuffer<float>                               mAudioClipScratch;   // single-clip scratch for per-clip rack

    // D1.2 (2026-04-24): per-drum-page engine processors (dynamic-drum model).
    // Each entry is one independent engine instance owned by a DrumPage tab.
    // 2026-04-25: legacy mDrumsEngine + mDrumsEngineLock + mDrumsEngineBuf +
    // mDrumSlotBufs removed (BaySickDrumsProcessor deleted).
    juce::SpinLock                                         mDrumEngineLock;
    std::array<juce::AudioProcessor*, kMaxDrumPages>       mDrumEngines {};
    juce::AudioBuffer<float>                               mDrumEngineBuf;     // per-drum render scratch
    juce::AudioBuffer<float>                               mDrumEngineScratch; // per-drum sum scratch
    // Fast-path bypass - true only when at least one DrumPage tab has registered
    // an engine.  Audio thread checks this before doing any D1.2 work.
    std::atomic<bool>                                      mAnyDrumPageActive { false };

    // G-3 (2026-04-28): per-clip-page engine processors (dynamic-clips model).
    // Index = audio-row index (1:1 with mixer_audio_<row>).  Engine output is
    // mixed into that row's audio insert during processBlock so it shares the
    // same EffectRack / EQ / fader as the arrangement-playback audio.
    juce::SpinLock                                         mClipEngineLock;
    std::array<juce::AudioProcessor*, kMaxClipPages>       mClipEngines {};
    juce::AudioBuffer<float>                               mClipEngineScratch;
    // Fast-path bypass - set true the moment ANY Clips tab registers an engine,
    // false when none remain.  Avoids the per-block iteration cost on projects
    // that don't use clips.  Same pattern as mAnyDrumPageActive.
    std::atomic<bool>                                      mAnyClipPageActive { false };

    // G-4 (2026-04-28): per-Vox / per-Inst-page engine processors.  Same
    // pattern as Clips - engine output routes through the existing Vox / Inst
    // InsertNode (created by the Mixer page's "Add Vox/Inst Strip" flow).
    juce::SpinLock                                         mVoxEngineLock;
    std::array<juce::AudioProcessor*, kMaxVoxPages>        mVoxEngines {};
    juce::AudioBuffer<float>                               mVoxEngineScratch;
    std::atomic<bool>                                      mAnyVoxPageActive { false };
    juce::SpinLock                                         mInstEngineLock;
    std::array<juce::AudioProcessor*, kMaxInstPages>       mInstEngines {};
    juce::AudioBuffer<float>                               mInstEngineScratch;
    std::atomic<bool>                                      mAnyInstPageActive { false };

    // J-5 (2026-05-03): BaySickRustyDrums singleton engine.  Only one
    // instance per project.  Owned here so PluginProcessor can orchestrate
    // strip lifecycle on kit load/unload.  The class is forward-declared so
    // PluginProcessor.h doesn't pull sfizz headers into every TU.
    juce::SpinLock                                         mRustyDrumsEngineLock;
    std::unique_ptr<class BaySickRustyDrumsProcessor>      mRustyDrumsEngine;
    std::atomic<bool>                                      mRustyDrumsActive { false };
    juce::AudioBuffer<float>                               mRustyDrumsScratch; // J-7a: per-block render target
    juce::MidiBuffer                                       mRustyDrumsMidi;    // J-7a: per-block MIDI feed

    // K-2 (2026-05-05): BaySickGuitars per-instance engines.  Up to kMaxInstPages
    // total instances; one engine per Inst page whose source = BaySickGuitars.
    // Lock array (one SpinLock per slot) avoids cross-instance contention when
    // one tab's kit-load doesn't need to stall another tab's audio thread.
    std::array<juce::SpinLock, kMaxInstPages>                                       mGuitarsEngineLock;
    std::array<std::unique_ptr<class BaySickGuitarsProcessor>, kMaxInstPages>       mGuitarsEngine;
    std::array<std::atomic<bool>, kMaxInstPages>                                    mGuitarsActive {}; // value-init → all false

    // L-2 (2026-05-05): BaySickBasses per-instance engines.  Same pattern as
    // Guitars above; the two source modes share the kMaxInstPages cap (one
    // Inst slot is in exactly one mode at a time, but different tabs can
    // hold different modes).
    std::array<juce::SpinLock, kMaxInstPages>                                       mBassesEngineLock;
    std::array<std::unique_ptr<class BaySickBassesProcessor>, kMaxInstPages>        mBassesEngine;
    std::array<std::atomic<bool>, kMaxInstPages>                                    mBassesActive {};

    // I-16 G-9 (2026-05-03): per-page FilePlay flag.  Pre-scan in processBlock
    // sets these for pages whose linked recorded clip overlaps the current
    // playhead window.  Engine loop skips flagged pages (the audio-clip
    // rendering loop will drive their engine via the FilePlay branch instead).
    std::array<bool, kMaxVoxPages>                         mVoxFilePlayActive {};
    std::array<bool, kMaxInstPages>                        mInstFilePlayActive {};

    // 2026-05-06 Option A: per-tab idle-block counter for sfizz Inst tabs +
    // Rusty.  Increments each block where the tab has no MIDI activity AND
    // sfizz reports 0 active voices AND no audition pending.  When the
    // counter exceeds kIdleSuspendBlocks, the tab's entire chain (sfizz +
    // Pedals + NAMIR + insert rack + EQ) is skipped on this block.  Wakes
    // immediately on the next block where any of those gates fail (MIDI,
    // voice activity, audition).  Audio-thread-only state - no atomics.
    static constexpr int kIdleSuspendBlocks = 9;   // ~200ms at 256/44.1k
    std::array<int, kMaxInstPages> mInstIdleBlocks {};
    int mRustyIdleBlocks { 0 };
    // 2026-05-06: bus-level idle gate REVERTED - the per-block magnitude
    // check added overhead during active playback (where buses are not
    // silent) without offsetting savings, net-negative on busy sessions.
    // Per-tab Option A above remains the meaningful DSP win.

    // R3 (2026-04-23): Live-input audio capture for Vox / Inst strips.
    // mLiveInputSnapshot - non-cleared copy of the input channels before
    // buffer.clear(), populated each block when numInputs > 0.
    // mLiveInputSlotBuf - reused stereo scratch for routing one channel of
    // the snapshot through a single Vox / Inst InsertNode at a time.
    juce::AudioBuffer<float> mLiveInputSnapshot;
    juce::AudioBuffer<float> mLiveInputSlotBuf;

    // F4 (2026-04-24): master-bus Play/Stop declick gain.  Ramps 0 <-> 1 over
    // ~5 ms on transport state change so the output buffer never hard-flips
    // between silence and live audio.  Audio-thread state; no lock.
    float mMasterFadeGain { 0.0f };

    // 2026-04-24 deferred rack-state replay (see applyPendingRackStates doc above).
    juce::ValueTree mPendingProjectRackState;

    // §P4.3 B7 (2026-04-22): per-page pre-rack EQ pointer arrays + SpinLocks
    // deleted.  Pre-rack EQs now live on VibeGraph InsertNode / BusNode
    // preEq members - no more page-owned DSPs held here as non-owning ptrs.

    // ── State ─────────────────────────────────────────────────────────────
    std::atomic<int> mDeviceOutputLatency { 0 };
    double          mSampleRate  { 44100.0 };
    int             mBlockSize   { 512 };
    PatternManager* mPatternManager { nullptr };

public:
    // Effective loop length - written by the message thread (10 Hz GlobalTransportBar timer),
    // read on the audio thread in processBlock. Avoids a data race on PatternManager vectors.
    std::atomic<double> mCachedPatternLoopBeats { 4.0 };   // 1 bar default; extended by content

    // Song mode - written by UI thread, read on audio thread.
    // true = play arrangement blocks linearly; false = loop current pattern.
    std::atomic<bool>   mSongMode { false };
    void setSongMode(bool b) { mSongMode.store(b, std::memory_order_relaxed); }
    bool isSongMode() const  { return mSongMode.load(std::memory_order_relaxed); }

    // 2026-05-06: project-load barrier.  Set true at the start of project
    // open / restoreBackup / closeAllDynamicTabs; the audio thread's
    // processBlock checks this at entry and bails (clears buffer + returns)
    // when set, so teardown of engines + InstPages is safe from concurrent
    // audio-thread access.  Without this, project re-open while audio is
    // running can crash inside a half-destroyed sub-engine (e.g. NAMIR /
    // MicPlacement IIR filter dereferences after the processor was freed).
    //
    // QA-Ef (2026-05-22): the shield now spans the WHOLE load -- teardown AND
    // the tab/engine REBUILD -- via deserializeProject raising it for its full
    // body and closeAllDynamicTabs being nest-aware (leaves it raised when an
    // outer load owns it).  Previously only the teardown half was shielded, so
    // registerTask during rebuild raced the audio thread's render-task-list
    // iteration (use-after-free in RenderGraphDispatcher::dispatchBlock on a
    // save-file load).  Pairs with the dispatcher pre-sizing its task lists.
    std::atomic<bool> mProjectLoadInProgress { false };
    void setProjectLoadInProgress (bool b) noexcept
        { mProjectLoadInProgress.store (b, std::memory_order_release); }
    bool isProjectLoadInProgress() const noexcept
        { return mProjectLoadInProgress.load (std::memory_order_acquire); }

    // P4: current project folder (for resolving relative audioFilePath strings).
    // Guarded by mProjectFolderLock since both message + audio threads read it.
    juce::CriticalSection mProjectFolderLock;
    juce::File            mCurrentProjectFolder;

    // ── Metronome DSP ─────────────────────────────────────────────────────
    struct MetroDSP {
        enum SoundType { Sine = 0, Click, Wood, Bell };

        std::atomic<bool>  enabled      { false };
        std::atomic<float> volume       { 0.7f };
        std::atomic<int>   soundType    { (int)Sine };
        // Count-in (independent of transport):
        std::atomic<bool>   countInActive { false };
        std::atomic<double> countInBpm    { 120.0 };
        // Audio-thread-only state (no atomic needed):
        double lastBeatFloor    { -99999.0 };
        int    clickSampLeft    { 0 };
        bool   clickIsAccent    { false };
        float  clickPhase       { 0.f };
        // Count-in audio-thread state:
        double countInPhase     { 0.0 };   // beats elapsed since count-in started
        bool   countInWasActive { false }; // edge-detect to reset phase on start
        int    countInBeatsFired{ 0 };     // 2026-04-26 (D-5 fix): tracks how many
                                           // count-in clicks have fired so the loop
                                           // doesn't double-fire beat 1 after the
                                           // rising-edge initial trigger.
    } mMetro;

private:

    // Deferred editor init timer (Windows stability fix)
    int mInitCounter { 0 };

    // Step-change tracking for basic sequence triggering (standalone)
    int mLastDrumStep { -1 };
    int mLastBassStep { -1 };

    // 1M: overload accumulator - audio thread only, no sync needed
    int64_t mOverload85Samples { 0 };

    // ── Piano roll note scheduling (standalone) ───────────────────────────
    struct PRPendingOff {
        double beatOff;   // absolute beat when note-off should fire
        int    midiNote;  // note to turn off
        int    target;    // 0..7 = layer page; kBassPRTarget+0..3 (8..11) = bass page index
    };
    std::vector<PRPendingOff> mPRPendingOffs;
    double mPRLastBeatEnd { -1.0 }; // end-of-last-block for jump detection

    // ── Internal helpers ──────────────────────────────────────────────────
    void updateDrumMixLevels();

    // ── D3: choke-group dispatch (audio thread) ───────────────────────────
    // Scan synth note-ons + audio clip starts; if a fire's source has
    // chokeGroup > 0, inject allNotesOff into every other synth insert in the
    // same group AND set mutedByChoke=true on every other audio clip in the
    // same group.  Wait-free; reads atomic param pointers cached on each
    // InsertNode.  Audio clip mutedByChoke is also reset to false here for
    // clips not currently in playback range so a fresh playthrough starts
    // un-choked.
    // 2026-04-30 (audit B.6): Vox + Inst page MIDI buffers added so choke
    // groups also dispatch to / from those engines.  Was Layer/Bass/Drum
    // only - Audio/Aux/Vox/Inst chokeGroup APVTS params were registered
    // but the dispatch loop never scanned or injected them.
    void applyChokeGroupDispatch(
        std::array<juce::MidiBuffer, kMaxLayerPages>& layerMidi,
        std::array<juce::MidiBuffer, kMaxBassPages>&  bassMidi,
        std::array<juce::MidiBuffer, kMaxDrumPages>&  drumMidi,
        std::array<juce::MidiBuffer, kMaxVoxPages>&   voxMidi,
        std::array<juce::MidiBuffer, kMaxInstPages>&  instMidi,
        juce::int64 projectStartSamp,
        int         numSamples,
        double      secPerBeat);
    // §P4.3 B7 (2026-04-22): updateDrumsEQ + updateLayerPageEQsFromApvts +
    // updateBassPageEQsFromApvts removed along with their DSP instances.
    // Pre-rack EQs now sync via updateAllPreRackEQsFromApvts below (unified
    // mixer-strip iteration).
    // Session B: generic update helper for any EQ8MsDSP bound to an APVTS prefix pair.
    // Reads all 9 band params x 8 bands x 2 sides and applies via the standard
    // setBand* setters (CPU-guarded internally). Used for every post-rack EQ
    // (bus + insert) whose params were lazily registered via addParamsForTrackEQ.
    void updateEQFromApvts(EQ8MsDSP* eq,
                           const juce::String& midPrefix,
                           const juce::String& sidePrefix);
    // Iterates every registered mixer strip (6 buses + up to 94 inserts) and
    // calls updateEQFromApvts for each InsertNode/BusNode's EQ. No-op when a
    // given slot has no node (index out of range or not yet registered).
    void updateAllPostRackEQsFromApvts();
    void updateAllPreRackEQsFromApvts();   // §P4.3

    // §P4.3 perf: ValueTree::Listener override.  Marks the EQ-sync dirty flag
    // when ANY APVTS state property changes.  The next processBlock will run
    // updateAllPost+PreRackEQsFromApvts; subsequent blocks skip until the next
    // change.  Catches param edits from UI (message thread), automation (audio
    // thread), and host-driven setValue calls - all uniformly route through
    // ValueTree::setProperty under the hood.
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override
    {
        mEQsDirty.store(true, std::memory_order_relaxed);
        if (onAnyStateChange) onAnyStateChange();   // P5: project dirty tracking
    }
    // Default true: first processBlock always syncs (catches initial state load
    // + factory defaults that may differ from DSP construction defaults).
    std::atomic<bool> mEQsDirty { true };

public:
    // P5: wired by StandaloneEditor to ProjectManager::markDirty.  Fires on
    // any APVTS change (piggybacks on the existing valueTreePropertyChanged
    // subscription).
    std::function<void()> onAnyStateChange;
private:
    void syncMixerFromPatternManager();

    // Lazy registration helpers (called from registerParamsForTrack)
    void addParamsForHarmless    (const juce::String& prefix);
    void addParamsForVibePlayer  (const juce::String& prefix);
    void addParamsForBaySickSynth(const juce::String& prefix);
    void addParamsForBaySickBass (const juce::String& prefix);
    // 2026-04-25: addParamsForBaySickDrums removed.
    void addParamsForTrackEQ     (const juce::String& prefix);
    void addParamsForTrackPreEQ  (const juce::String& prefix);   // §P4.3 pre-rack EQ block
    void addLiveInputParams      (const juce::String& prefix);   // R2: Vox/Inst _inputChannelIdx
    mutable juce::CriticalSection mInputChannelNamesLock;        // R2
    void addParamsForEQBank      (const juce::String& prefix, const juce::String& subPrefix);
    void addParamsForEffectRack  (const juce::String& prefix);

    // ── 5F-4a: Mixer-strip lazy APVTS registration ───────────────────────────
    // Classifies which mixer-strip param family to register.
    // Master: _level, _pan, _width only (no polarity/mute/solo/bypass/arm)
    // Bus:    _level, _pan, _mute, _solo, _polarity, _width
    // Insert: _level, _pan, _mute, _solo, _polarity, _width, _bypass, _arm
public:
    enum class MixerStripKind { Master, Bus, Insert };
private:
    void addParamsForMixerStrip(const juce::String& prefix, MixerStripKind kind,
                                 int defaultSendTo);

public:
    // Ensure mixer-strip params exist for the given prefix. Safe to call repeatedly.
    // Returns true if any new params were registered on this call.
    // Thread: message thread only (APVTS::createAndAddParameter is not RT-safe).
    // defaultSendTo is the main-out destination channel id (see MixerChannelIds).
    bool ensureMixerStripParams(const juce::String& prefix, MixerStripKind kind,
                                 int defaultSendTo);

    // Bulk-register the five bus strips + master once at startup.
    // Called from VibeGraph::buildFixedTopology via a callback wired in the constructor.
    void ensureMixerBusAndMasterParams();

private:

    // trackId → list of registered param IDs (for cleanup tracking)
    std::map<juce::String, juce::StringArray> mRegisteredTrackParams;

    // Track which mixer-strip prefixes have been registered (prevents double-register)
    std::set<juce::String> mRegisteredMixerStrips;

    // C.3 (2026-04-30): hardware MIDI input bridge.  See public getter +
    // setLiveMidiTarget for the contract.
    juce::MidiMessageCollector mLiveMidiCollector;
    std::atomic<int>           mLiveMidiTargetKind  { -1 };   // -1 = unset
    std::atomic<int>           mLiveMidiTargetIndex { 0  };

    // I-3b (2026-05-02): MIDI Learn registry + per-device event queue.
    // The queue is a MIDI-thread -> audio-thread bridge that preserves source
    // device names (the live MIDI collector loses them).  StandaloneApp
    // pushes into it from MidiInputCallback::handleIncomingMidiMessage; the
    // audio thread drains it inside processBlock and feeds events through the
    // registry's dispatch.  Mutators on mMidiLearn run on the message thread.
    MidiLearnRegistry   mMidiLearn;
    MidiLearnEventQueue mMidiLearnQueue;

    // ── Multi-threaded render engine (Phase 1 scaffolding, 2026-05-06) ───────
    // Lifetime = plugin lifetime. Pool spawns its workers in its constructor
    // and joins them in its destructor; prepareToPlay only resizes the arena
    // and clears queues, never touches the threads themselves.
    //
    // Declaration order is critical: dispatcher takes refs to pool + arena,
    // and C++ guarantees member init order matches declaration order.
    static int computeRenderWorkerCount() noexcept;

    VibeThreadPool        mRenderPool       { computeRenderWorkerCount() };
    ChannelBufferArena    mRenderArena;
    RenderGraphDispatcher mRenderDispatcher { mRenderPool, mRenderArena };

    // Batch 3 (2026-05-06): one EngineInsertTask per active Layer/Bass/Drum.
    // Created in lockstep with the engine via registerXxxEngine; destroyed
    // in unregisterXxxEngine. The dispatcher holds non-owning pointers; we
    // own the storage so destruction is well-defined.
    std::array<std::unique_ptr<EngineInsertTask>, kMaxLayerPages> mLayerRenderTasks;
    std::array<std::unique_ptr<EngineInsertTask>, kMaxBassPages>  mBassRenderTasks;
    std::array<std::unique_ptr<EngineInsertTask>, kMaxDrumPages>  mDrumRenderTasks;

    // Batch 4 (2026-05-06): Vox + Inst live-input strip task wrappers.
    std::array<std::unique_ptr<VoxStripTask>,  kMaxVoxPages>  mVoxRenderTasks;
    std::array<std::unique_ptr<InstStripTask>, kMaxInstPages> mInstRenderTasks;

    // Batch 5 (2026-05-06): Clip page + Audio insert task wrappers.
    // QA-0 (2026-05-07): per-row composite owns BOTH arrangement-clip
    // decode + clip-engine MIDI trigger.  mClipRenderTasks is gone --
    // registerClipEngine sets the composite's mClipEngine field on
    // mAudioRenderTasks[pageIdx] instead of registering a separate task.
    std::array<std::unique_ptr<CompositeAudioInsertTask>, kMaxAudioRows> mAudioRenderTasks;

    // Batch 6 (2026-05-06): BaySickRustyDrums producer + per-strip inserts.
    // Producer owned per-engine (only one engine instance ever exists).
    // Inserts are 1:1 with the engine's discovered strip count (up to 13);
    // arrays sized to the max + indexed by stripIndex.
    std::unique_ptr<RustyDrumsProducerTask>                                       mRustyProducerTask;
    std::array<std::unique_ptr<RustyInsertTask>, MixerChannelIds::kMaxRustyStrips> mRustyRenderTasks;

    // Batch 7 (2026-05-06): passive accumulator strip tasks.
    // Aux: created lazily via ensureAuxInsert; up to kMaxAuxStrips (18).
    // Bus: 11 always-on buses registered idempotently in prepareToPlay.
    static constexpr int kNumBatch7Buses = 11;
    std::array<std::unique_ptr<PassiveStripTask>, MixerChannelIds::kMaxAuxStrips> mAuxRenderTasks;
    std::array<std::unique_ptr<PassiveStripTask>, kNumBatch7Buses>                mBusRenderTasks;

    // Batch 8 (2026-05-06): terminal MasterTask.  Always-on, registered
    // idempotently in prepareToPlay alongside the bus tasks.
    std::unique_ptr<MasterTask> mMasterRenderTask;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VibeSynthProcessor)
};
