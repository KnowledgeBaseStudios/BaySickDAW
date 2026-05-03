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

// ── APVTS parameter helper ────────────────────────────────────────────────────
// Creates a ParameterID with version 1
#define VID(id) juce::ParameterID{(id), 1}

class VibeSynthProcessor : public juce::AudioProcessor,
                           private juce::ValueTree::Listener   // §P4.3 perf: dirty-flag EQ sync
{
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

    // PatternManager is owned by StandaloneEditor today — the processor holds
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
    // Creates an Aux strip at the given idx (0..15). No audio source — aux is
    // receive-only; its input is populated by sends from other strips.
    // Default main-out = FX Bus. Safe to call repeatedly for the same idx.
    void ensureAuxInsert(int idx, const juce::String& displayName);

    // R1 (2026-04-23): Vox / Inst strip registration for live-input recording.
    // Up to 6 of each; default main-out = their respective bus (VoxBus / InstBus).
    // R1 registers params + InsertNode only; R2+R3 wire the ASIO input source.
    void ensureVoxInsert  (int idx, const juce::String& displayName);
    void ensureInstInsert (int idx, const juce::String& displayName);

    // R2 (2026-04-23): live-input ASIO channel persistence.  Index lives in
    // APVTS as `<prefix>_inputChannelIdx` (Int -1..127, default -1).  The
    // channel NAME (e.g. "Mic 1") is stored as a non-APVTS attribute on
    // apvts.state via setInputChannelName/getInputChannelName so on device
    // reconnect we can re-resolve by name first, fall back to index.
    void         setInputChannelName (const juce::String& stripPrefix,
                                       const juce::String& name);
    juce::String getInputChannelName (const juce::String& stripPrefix) const;

    // 5F-4b B7: scan APVTS saved state for any mixer_aux_N params and
    // re-register their InsertNodes. Called from setStateInformation.
    void restoreAuxStripsFromState();

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
    // registerLayerEngine — also creates the Drum InsertNode + mixer strip
    // params for that drum's pageIdx.
    void registerDrumEngine  (int pageIdx, juce::AudioProcessor* eng);
    void unregisterDrumEngine(int pageIdx);

    // G-3 (2026-04-28): per-clip-page engine registration.  pageIdx here is
    // the audio-row index for the bound clip (1:1 mapping — each Clips tab
    // claims one of the 50 mixer_audio_<row> inserts on spawn).  Engine
    // output is mixed into THAT row's audio insert during processBlock so
    // arrangement-playback audio + piano-roll-triggered audio share the
    // same EffectRack / EQ / fader chain.  No new mixer strip is created.
    void registerClipEngine  (int pageIdx, juce::AudioProcessor* eng);
    void unregisterClipEngine(int pageIdx);

    // G-4 (2026-04-28): per-Vox / per-Inst page engine registration.  pageIdx
    // is the Vox / Inst insert index (1:1 with mixer_vox_<idx> / mixer_inst_
    // <idx>).  Same shape as registerClipEngine — engine output mixes into
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
    // Peak dB for each mix section — used by MixerPage strip meters.
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
    // C.1 (2026-04-30): FX Bus peak — written by VibeGraph::processEffectsBus
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

    // 2026-05-02: running-max companion atomics.  Audio thread CAS-maxes into
    // these during processBlock; a single end-of-block promotion lifts them
    // into the UI-visible atomics above.  Result: every meter (every bus,
    // every audio row) is end-of-block coherent so a UI vblank firing at any
    // time sees a consistent snapshot across all of them, not "this bus
    // updated, that one not yet".  Default to -inf so a "no audio writes
    // this block" case promotes nothing (skip-on-INF inside the promote
    // helper) and the existing snapshot keeps decaying via UI ballistics.
    static constexpr float kPeakAtomicNegInf = -std::numeric_limits<float>::infinity();
    std::atomic<float> mAudioClipsBusPeakDbRun  { kPeakAtomicNegInf };
    std::atomic<float> mAudioClipsBusPeakDbLRun { kPeakAtomicNegInf };
    std::atomic<float> mAudioClipsBusPeakDbRRun { kPeakAtomicNegInf };
    std::atomic<float> mVoxBusPeakDbRun         { kPeakAtomicNegInf };
    std::atomic<float> mVoxBusPeakDbLRun        { kPeakAtomicNegInf };
    std::atomic<float> mVoxBusPeakDbRRun        { kPeakAtomicNegInf };
    std::atomic<float> mInstBusPeakDbRun        { kPeakAtomicNegInf };
    std::atomic<float> mInstBusPeakDbLRun       { kPeakAtomicNegInf };
    std::atomic<float> mInstBusPeakDbRRun       { kPeakAtomicNegInf };
    std::atomic<float> mFxBusPeakDbRun          { kPeakAtomicNegInf };
    std::atomic<float> mFxBusPeakDbLRun         { kPeakAtomicNegInf };
    std::atomic<float> mFxBusPeakDbRRun         { kPeakAtomicNegInf };
    std::atomic<float> mVoxBus2PeakDbRun        { kPeakAtomicNegInf };
    std::atomic<float> mVoxBus2PeakDbLRun       { kPeakAtomicNegInf };
    std::atomic<float> mVoxBus2PeakDbRRun       { kPeakAtomicNegInf };
    std::atomic<float> mInstBus2PeakDbRun       { kPeakAtomicNegInf };
    std::atomic<float> mInstBus2PeakDbLRun      { kPeakAtomicNegInf };
    std::atomic<float> mInstBus2PeakDbRRun      { kPeakAtomicNegInf };
    std::atomic<float> mInstBus3PeakDbRun       { kPeakAtomicNegInf };
    std::atomic<float> mInstBus3PeakDbLRun      { kPeakAtomicNegInf };
    std::atomic<float> mInstBus3PeakDbRRun      { kPeakAtomicNegInf };

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
        // D3: choke group (0 = none, 1..16).  Copied from the source clip's
        // AudioLibraryEntry at rebuildAudioClipPlayers() time so the audio
        // thread can read it without a library lookup.
        int    chokeGroup     = 0;
        // D3: true once a peer in the same choke group fires during this clip's
        // current playback.  Reset to false each block when the playhead is
        // outside the clip range (so a fresh playthrough starts un-choked).
        bool   mutedByChoke   = false;
        // Disk-streaming reader — background thread pre-fetches, audio thread reads.
        std::unique_ptr<AudioClipStreamer> streamer;
        // Phase vocoder for BPM-aware time stretch (null when stretchMode=false).
        std::unique_ptr<PhaseVocoder> vocoder;
        // Pre-allocated scratch buffers used by the audio thread (no heap alloc in processBlock).
        juce::AudioBuffer<float> pvInBuf;   // raw file samples fed into vocoder
        juce::AudioBuffer<float> pvOutBuf;  // stretched output from vocoder (file SR)
        // Expected next file position — used to detect seeks and reset vocoder.
        int64 expectedFilePos { 0 };
    };
    std::vector<AudioClipPlayer> mAudioClipPlayers;
    juce::SpinLock               mAudioClipLock;

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
        int                                channelId;    // MixerChannelIds
        juce::String                       displayName;  // e.g. "Vox 1"
        juce::File                         file;
        std::unique_ptr<AudioFileRecorder> recorder;
    };

    struct RecordResult
    {
        juce::File                                masterFile;   // only when no strips armed
        std::vector<std::pair<int, juce::File>>   stripFiles;   // per-strip captures
        std::vector<PianoNote>                    midiNotes;    // from MidiRecorder
        double                                    startBeat { 0.0 };
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
    // Pre-allocated scratch buffers for engine rendering (sized in prepareToPlay)
    juce::AudioBuffer<float>                               mLayerEngineSum;
    juce::AudioBuffer<float>                               mLayerEngineScratch;
    juce::AudioBuffer<float>                               mBassEngineBuf;
    juce::AudioBuffer<float>                               mBassEngineScratch;
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
    // Fast-path bypass — true only when at least one DrumPage tab has registered
    // an engine.  Audio thread checks this before doing any D1.2 work.
    std::atomic<bool>                                      mAnyDrumPageActive { false };

    // G-3 (2026-04-28): per-clip-page engine processors (dynamic-clips model).
    // Index = audio-row index (1:1 with mixer_audio_<row>).  Engine output is
    // mixed into that row's audio insert during processBlock so it shares the
    // same EffectRack / EQ / fader as the arrangement-playback audio.
    juce::SpinLock                                         mClipEngineLock;
    std::array<juce::AudioProcessor*, kMaxClipPages>       mClipEngines {};
    juce::AudioBuffer<float>                               mClipEngineScratch;
    // Fast-path bypass — set true the moment ANY Clips tab registers an engine,
    // false when none remain.  Avoids the per-block iteration cost on projects
    // that don't use clips.  Same pattern as mAnyDrumPageActive.
    std::atomic<bool>                                      mAnyClipPageActive { false };

    // G-4 (2026-04-28): per-Vox / per-Inst-page engine processors.  Same
    // pattern as Clips — engine output routes through the existing Vox / Inst
    // InsertNode (created by the Mixer page's "Add Vox/Inst Strip" flow).
    juce::SpinLock                                         mVoxEngineLock;
    std::array<juce::AudioProcessor*, kMaxVoxPages>        mVoxEngines {};
    juce::AudioBuffer<float>                               mVoxEngineScratch;
    std::atomic<bool>                                      mAnyVoxPageActive { false };
    juce::SpinLock                                         mInstEngineLock;
    std::array<juce::AudioProcessor*, kMaxInstPages>       mInstEngines {};
    juce::AudioBuffer<float>                               mInstEngineScratch;
    std::atomic<bool>                                      mAnyInstPageActive { false };

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
    // preEq members — no more page-owned DSPs held here as non-owning ptrs.

    // ── State ─────────────────────────────────────────────────────────────
    std::atomic<int> mDeviceOutputLatency { 0 };
    double          mSampleRate  { 44100.0 };
    int             mBlockSize   { 512 };
    PatternManager* mPatternManager { nullptr };

public:
    // Effective loop length — written by the message thread (10 Hz GlobalTransportBar timer),
    // read on the audio thread in processBlock. Avoids a data race on PatternManager vectors.
    std::atomic<double> mCachedPatternLoopBeats { 4.0 };   // 1 bar default; extended by content

    // Song mode — written by UI thread, read on audio thread.
    // true = play arrangement blocks linearly; false = loop current pattern.
    std::atomic<bool>   mSongMode { false };
    void setSongMode(bool b) { mSongMode.store(b, std::memory_order_relaxed); }
    bool isSongMode() const  { return mSongMode.load(std::memory_order_relaxed); }

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

    // 1M: overload accumulator — audio thread only, no sync needed
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
    // only — Audio/Aux/Vox/Inst chokeGroup APVTS params were registered
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
    // thread), and host-driven setValue calls — all uniformly route through
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VibeSynthProcessor)
};
