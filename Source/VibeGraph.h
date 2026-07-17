#pragma once
#include <JuceHeader.h>
#include <unordered_map>
#include <functional>
#include "DSP/SpectrumFeed.h"
#include "DSP/EQ8MsDSP.h"
#include "EffectRack.h"
#include "VibesynthConstants.h"

// Forward declarations - full headers included in VibeGraph.cpp only
class BassSynth;
// DrumSynth forward-decl removed - class no longer used in graph (2026-04-25).

// ── Meter latency-compensation toggle (2026-05-02) ───────────────────────────
// When enabled, audio nodes delay their published peak readings by N blocks so
// the visual meter aligns with the sound the user actually hears (output-driver
// latency).  Off by default; toggled from the Mixer hamburger menu.  Audio
// thread reads this atomic per block.  N is the number of audio blocks to
// delay; blocks() helper computes from a milliseconds value + the host SR/block.
namespace MeterLatencyComp
{
    extern std::atomic<int>  gCompensationBlocks;   // 0 = off, >0 = N blocks delay
    extern std::atomic<bool> gEnabled;              // user toggle (UI thread)
    static constexpr int     kRingSize = 16;        // max compensation = 16 blocks
    static constexpr int     kRingMask = kRingSize - 1;

    // Recompute gCompensationBlocks from the current device latency.  Called
    // by host when (a) toggle changes or (b) audio device prepare fires.
    void recomputeFromDevice (double sampleRate, int blockSize, int latencySamples);
}

// ── Channel ID registry (5F-4b B1a) ──────────────────────────────────────────
// Every mixer strip has a unique integer id used for cable routing, APVTS
// _sendTo params, and the RoutingGraph.
namespace MixerChannelIds
{
    constexpr int kOutput    = 0;    // Terminal sink - only Master routes here
    constexpr int kLayersBus = 1;
    constexpr int kBassBus   = 2;
    constexpr int kDrumsBus  = 3;
    constexpr int kMaster    = 4;
    constexpr int kFxBus     = 5;
    constexpr int kClipsBus  = 6;
    constexpr int kVoxBus    = 7;    // R1 (2026-04-23): live-vocal strip bus
    constexpr int kInstBus   = 8;    // R1: live-instrument strip bus
    // G-6 (2026-04-29): secondary buses for splitting Vox/Inst groups
    // (e.g. lead vs backup vocals, guitars vs bass).  Always-allocated audio
    // (cheap pre-process when no inserts route to them) but UI strip is lazy
    // - only rendered on Mixer after user clicks "Add Vox/Inst Bus".
    constexpr int kVoxBus2   = 9;    // G-6: optional 2nd Vox bus
    constexpr int kInstBus2  = 10;   // G-6: optional 2nd Inst bus
    constexpr int kInstBus3  = 11;   // G-6: optional 3rd Inst bus
    constexpr int kRustyDrumsBus = 12; // J-4 (2026-05-03): dedicated bus for BaySickRustyDrums strips
    constexpr int kAuxBase   = 100;  // Aux 0..17 → 100..117 (G-7 polish: 16 → 18)
    constexpr int kLayerBase = 200;  // Layer insert 0..15 → 200..215
    constexpr int kBassBase  = 300;  // Bass insert 0..15 → 300..315
    constexpr int kAudioBase = 400;  // Audio insert 0..49 → 400..449
    constexpr int kDrumBase  = 500;  // Drum insert 0..15 → 500..515
    constexpr int kVoxBase   = 600;  // R1: Vox insert 0..5 → 600..605
    constexpr int kInstBase  = 700;  // R1: Inst insert 0..19 → 700..719 (kMaxInstStrips bumped 6→10 in G-4 2026-04-28, 10→20 in G-6 2026-04-29)
    constexpr int kRustyBase = 800;  // J-4: BaySickRustyDrums insert 0..12 → 800..812

    constexpr int kMaxVoxStrips   = 6;   // R1
    constexpr int kMaxInstStrips  = 20;  // R1; bumped 6 → 10 in G-4 (2026-04-28); 10 → 20 in G-6 (2026-04-29)
    constexpr int kMaxAuxStrips   = 18;  // 5F-4b B2; bumped 16 → 18 in G-7 polish (2026-04-29)
    constexpr int kMaxRustyStrips = 13;  // J-4: 13 sound types per BaySickRustyDrums kit (no doubles)

    inline int layerInsert (int idx) { return kLayerBase + idx; }
    inline int bassInsert  (int idx) { return kBassBase  + idx; }
    inline int drumInsert  (int idx) { return kDrumBase  + idx; }
    inline int audioInsert (int idx) { return kAudioBase + idx; }
    inline int auxStrip    (int idx) { return kAuxBase   + idx; }
    inline int voxInsert   (int idx) { return kVoxBase   + idx; }
    inline int instInsert  (int idx) { return kInstBase  + idx; }
    inline int rustyInsert (int idx) { return kRustyBase + idx; }

    // Derive the APVTS prefix from a channel id (e.g. 200 → "mixer_layer_0").
    inline juce::String prefixFromChannelId (int chId)
    {
        switch (chId)
        {
            case kMaster:    return "mixer_master";
            case kLayersBus: return "mixer_layers";
            case kBassBus:   return "mixer_bass";
            case kDrumsBus:  return "mixer_drums";
            case kFxBus:     return "mixer_fx";
            case kClipsBus:  return "mixer_clipsbus";
            case kVoxBus:    return "mixer_voxbus";
            case kInstBus:   return "mixer_instbus";
            case kVoxBus2:   return "mixer_voxbus2";
            case kInstBus2:  return "mixer_instbus2";
            case kInstBus3:  return "mixer_instbus3";
            case kRustyDrumsBus: return "mixer_rustybus";
        }
        if (chId >= kLayerBase && chId < kLayerBase + 16)           return "mixer_layer_" + juce::String(chId - kLayerBase);
        if (chId >= kBassBase  && chId < kBassBase  + 16)           return "mixer_bass_"  + juce::String(chId - kBassBase);
        if (chId >= kDrumBase  && chId < kDrumBase  + 16)           return "mixer_drum_"  + juce::String(chId - kDrumBase);
        if (chId >= kAudioBase && chId < kAudioBase + 50)           return "mixer_audio_" + juce::String(chId - kAudioBase);
        if (chId >= kAuxBase   && chId < kAuxBase   + kMaxAuxStrips)   return "mixer_aux_"   + juce::String(chId - kAuxBase);
        if (chId >= kVoxBase   && chId < kVoxBase   + kMaxVoxStrips)   return "mixer_vox_"   + juce::String(chId - kVoxBase);
        if (chId >= kInstBase  && chId < kInstBase  + kMaxInstStrips)  return "mixer_inst_"  + juce::String(chId - kInstBase);
        if (chId >= kRustyBase && chId < kRustyBase + kMaxRustyStrips) return "mixer_rusty_" + juce::String(chId - kRustyBase);
        return {};
    }

    // Is this channel a bus? (for routing rule enforcement)
    inline bool isBus (int chId)
    {
        return chId == kLayersBus || chId == kBassBus || chId == kDrumsBus
            || chId == kFxBus     || chId == kClipsBus
            || chId == kVoxBus    || chId == kInstBus
            || chId == kVoxBus2   || chId == kInstBus2 || chId == kInstBus3
            || chId == kRustyDrumsBus;
    }

    // Is this channel's main-out locked (cannot be rerouted)?
    // J-5 (2026-05-03): Rusty inserts join Master + buses as locked main-out
    // (each Rusty strip is permanently bound to kRustyDrumsBus; sends can
    // still go out to aux strips via the per-strip "+" send button).
    inline bool isMainOutLocked (int chId)
    {
        return chId == kMaster || isBus(chId)
            || (chId >= kRustyBase && chId < kRustyBase + kMaxRustyStrips);
    }

    // Is this a valid send target from a Bus or Master strip?
    // Rule: buses/master can only send to Aux strips.
    inline bool isValidBusSendTarget (int dstId)
    {
        return dstId >= kAuxBase && dstId < kAuxBase + kMaxAuxStrips;
    }

    // C.4 Phase 1 (2026-04-30): friendly display name for a strip channel id.
    // Used by DynamicParamsPopout / SlotComponent SC source dropdowns to
    // label routed sources ("Layer 1", "Master", "FX Bus", etc.).
    inline juce::String friendlyName (int chId)
    {
        switch (chId)
        {
            case kMaster:    return "Master";
            case kLayersBus: return "Layers Bus";
            case kBassBus:   return "Bass Bus";
            case kDrumsBus:  return "Drums Bus";
            case kFxBus:     return "FX Bus";
            case kClipsBus:  return "Clips Bus";
            case kVoxBus:    return "Vox Bus";
            case kInstBus:   return "Inst Bus";
            case kVoxBus2:   return "Vox Bus 2";
            case kInstBus2:  return "Inst Bus 2";
            case kInstBus3:  return "Inst Bus 3";
            case kRustyDrumsBus: return "RustyDrums Bus";
        }
        if (chId >= kLayerBase && chId < kLayerBase + 16) return "Layer " + juce::String(chId - kLayerBase + 1);
        if (chId >= kBassBase  && chId < kBassBase  + 16) return "Bass "  + juce::String(chId - kBassBase  + 1);
        if (chId >= kDrumBase  && chId < kDrumBase  + 16) return "Drum "  + juce::String(chId - kDrumBase  + 1);
        if (chId >= kAudioBase && chId < kAudioBase + 50) return "Audio " + juce::String(chId - kAudioBase + 1);
        if (chId >= kAuxBase   && chId < kAuxBase   + kMaxAuxStrips)   return "Aux "   + juce::String(chId - kAuxBase   + 1);
        if (chId >= kVoxBase   && chId < kVoxBase   + kMaxVoxStrips)   return "Vox "   + juce::String(chId - kVoxBase   + 1);
        if (chId >= kInstBase  && chId < kInstBase  + kMaxInstStrips)  return "Inst "  + juce::String(chId - kInstBase  + 1);
        if (chId >= kRustyBase && chId < kRustyBase + kMaxRustyStrips) return "Rusty " + juce::String(chId - kRustyBase + 1);
        return "Ch " + juce::String(chId);
    }

    // Default main-out destination for a given channel.
    // Master → kOutput (locked). Buses/Aux → FxBus. Inserts → their parent bus.
    inline int defaultSendTo (int channelId)
    {
        switch (channelId)
        {
            case kMaster:    return kOutput;
            case kLayersBus:
            case kBassBus:
            case kDrumsBus:
            case kFxBus:
            case kClipsBus:
            case kVoxBus:
            case kInstBus:
            case kVoxBus2:
            case kInstBus2:
            case kInstBus3:  return kMaster;
            case kRustyDrumsBus: return kMaster;
        }
        if (channelId >= kLayerBase && channelId < kLayerBase + 16)             return kLayersBus;
        if (channelId >= kBassBase  && channelId < kBassBase  + 16)             return kBassBus;
        if (channelId >= kDrumBase  && channelId < kDrumBase  + 16)             return kDrumsBus;
        if (channelId >= kAudioBase && channelId < kAudioBase + 50)             return kClipsBus;
        if (channelId >= kAuxBase   && channelId < kAuxBase   + kMaxAuxStrips)  return kFxBus;
        if (channelId >= kVoxBase   && channelId < kVoxBase   + kMaxVoxStrips)  return kVoxBus;
        if (channelId >= kInstBase  && channelId < kInstBase  + kMaxInstStrips) return kInstBus;
        if (channelId >= kRustyBase && channelId < kRustyBase + kMaxRustyStrips) return kRustyDrumsBus;
        return kMaster;
    }
}

// ── RoutingGraph (5F-4b B1a) ─────────────────────────────────────────────────
// Resolves dynamic per-strip sendTo + sends[] from APVTS at block rate.
// Cycle detection via Kahn's topo sort. Audio path consumes topoOrder() in B1b.
class RoutingGraph
{
public:
    struct Edge
    {
        int   srcId;
        int   dstId;
        float amountDb;
        bool  prePost;      // true = pre-fader tap from src
        bool  isMainOut;    // true = main-out cable (exactly one per src)
    };

    // C.4 Phase 1 (2026-04-30): sidechain edge.  Source's post-everything
    // tap feeds dst's SC receive line `dstSlot` (0..3).  Cycle check covers
    // SC + main + send edges combined (a strip mustn't sidechain itself
    // through any path that loops back).
    struct ScEdge
    {
        int srcId;
        int dstId;
        int dstSlot;        // 0..3, the receive line index on dst
    };

    static constexpr int kMaxSendsPerStrip   = 4;
    static constexpr int kMaxScRecvsPerStrip = 4;

    // Pre-flight cycle check (message thread). Would adding {src → dst} create
    // a cycle given the current edge list? Called before committing a cable drop.
    // Considers main + send + sidechain edges; SC cables participate in the
    // same DAG so cyclic feedback is impossible.
    bool wouldCreateCycle(int src, int dst) const noexcept;

    // Rebuild edges + topological order from APVTS. Returns true on clean build;
    // false if any cycle was detected (offending edges are dropped).
    bool rebuildFromApvts(juce::AudioProcessorValueTreeState& apvts,
                          const std::vector<std::pair<int, juce::String>>& activeChannels);

    const std::vector<int>&    topoOrder() const noexcept { return mTopoOrder; }
    const std::vector<Edge>&   edges()     const noexcept { return mEdges; }
    const std::vector<ScEdge>& scEdges()   const noexcept { return mScEdges; }

private:
    std::vector<Edge>   mEdges;
    std::vector<ScEdge> mScEdges;
    std::vector<int>    mTopoOrder;

    bool computeTopo(const std::vector<int>& ids);
};

// ── VibeGraph ─────────────────────────────────────────────────────────────────
// Phase 1A / 1I: Audio bus topology for VibeDAW.
//
// Fixed bus topology:
//   LayersBusNode ─┐
//   BassBusNode   ─┤→ MasterBusNode → output
//   DrumsBusNode  ─┘
//   EffectsBusNode  (FX Bus - receive bus for aux strips + user-routed sends;
//                    driven by VibeGraph::processEffectsBus each block)
//
// Each bus node owns an EffectRack (6 slots) and holds a reference to the
// channel EQ8MsDSP managed by PluginProcessor.  Mixer gain/mute/solo comes
// from BusMix (written on the message thread, read on the audio thread).
//
// Phase-2 instrument nodes: lazy add/remove via addInstrumentNode /
// removeInstrumentNode (uses the embedded AudioProcessorGraph registry).
// ─────────────────────────────────────────────────────────────────────────────
class VibeGraph
{
public:
    // Opaque handle for Phase-2 instrument nodes
    using NodeID = juce::AudioProcessorGraph::NodeID;

    // ── Spectrum feed (audio thread writes, UI timer reads) ───────────────────
    // Definition moved to DSP/SpectrumFeed.h (shared with EQ8MsDSP per-instance
    // feeds; see 5F-9 §12i). Alias preserves the existing VibeGraph::SpectrumFeed
    // name across every caller.
    using SpectrumFeed = ::SpectrumFeed;

    // ── Bus mix state (written by message thread, read by audio thread) ───────
    struct BusMix
    {
        float layersGain  { 1.f };
        float bassGain    { 1.f };
        float drumsGain   { 1.f };
        float masterFader { 1.f };  // from PatternManager master fader
        bool  layersMute  { false };
        bool  bassMute    { false };
        bool  drumsMute   { false };
        bool  layersSolo  { false };
        bool  bassSolo    { false };
        bool  drumsSolo   { false };
    };
    BusMix busMix;   // written from message thread, read in processBlock

    // ── Construction / destruction ────────────────────────────────────────────
    VibeGraph();
    ~VibeGraph();

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    // prepare() must be called before buildFixedTopology() and again on any
    // sample-rate change.  Safe to call after the topology is built.
    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // Build the fixed bus topology.  Guards itself - no-op after first call so
    // safe to call every prepareToPlay().  All references must remain valid for
    // the lifetime of this VibeGraph.
    // 12i: SpectrumFeed refs dropped - each EQ8MsDSP owns its own pre/post feeds
    // (populated at its own process() I/O boundary). UI polls eq->preFeed /
    // eq->postFeed directly via ParametricEQDisplay::bindMsDSP.
    // §P4.3 B7 (2026-04-22): all external page-EQ refs dropped.  Every bus now
    // owns its own preEq (pre-rack) and busEq (post-rack) directly.
    // 2026-04-25: DrumSynth removed.  Drums-bus content now exclusively
    // comes from per-drum-tab InsertNode outputs.
    void buildFixedTopology(juce::Synthesiser&                  synth,
                            BassSynth&                          bass,
                            juce::AudioProcessorValueTreeState& apvts);

    // 2026-05-06 (Batch 9b): unified bus DSP dispatcher used by
    // PassiveStripTask's Bus mode.  `buf` is treated as in/out - caller must
    // have it pre-filled with the bus's input signal (sum of upstream
    // contributions) before calling.  Internally switches on busChId to the
    // right per-bus DSP path: Layers/Bass/Drums delegate to the existing
    // BusNodes via processChainOnly; Clips / Vox / Inst / Vox2 / Inst2 /
    // Inst3 / Rusty run their inline DSP migrated from PluginProcessor; FxBus
    // calls processEffectsBus; Master calls processMasterBus.  `anySolo` and
    // `panLaw` are forwarded as needed -
    // unused for buses that read solo/pan directly via APVTS.  Caller is
    // responsible for routing the processed output downstream - processBus
    // does DSP only.
    void processBus(int busChId, juce::AudioBuffer<float>& buf,
                    double bpm, int panLaw);

    // C.1 (2026-04-30): runs the FX Bus pipeline on its accumulator buffer
    // (preEq -> rack -> postEq -> polarity -> M/S width -> fader x mute x
    // solo -> pan -> peak meter).  Caller must subsequently route the kFxBus
    // result downstream (default = Master).  busAnySolo
    // participates in the receive-group solo gate; panLaw matches the project-
    // level master_pan_law convention used by the Vox/Inst bus loop.
    void processEffectsBus(juce::AudioBuffer<float>& buf, double bpm,
                            bool busAnySolo, int panLaw);

    // ── Bus EffectRack access (Effects Page / Mixer UI) ───────────────────────
    EffectRack* getLayersBusRack();
    EffectRack* getBassBusRack();
    EffectRack* getDrumsBusRack();
    EffectRack* getMasterRack();
    EffectRack* getEffectsBusRack();
    EffectRack* getAudioClipsBusRack();   // ID 6 in Effects dropdown
    EffectRack* getVoxBusRack();          // R3.5: Vox Bus rack
    EffectRack* getInstBusRack();         // R3.5: Inst Bus rack
    // G-6 (2026-04-29): secondary Vox/Inst buses (kVoxBus2 / kInstBus2 / kInstBus3).
    // Always allocated so audio routing works whether or not the user has
    // activated the strip on Mixer.  Inactive buses pre-process silent input
    // (cheap) - the strip activation flag only controls UI presence + route
    // picker filtering.
    EffectRack* getVoxBus2Rack();
    EffectRack* getInstBus2Rack();
    EffectRack* getInstBus3Rack();
    // J-4: dedicated bus for BaySickRustyDrums strips.  Always-allocated
    // so audio routing works regardless of whether the user has added a
    // BaySickRustyDrums instance - sums silence (cheap pre-process) until
    // a kit is loaded and 13 strips spawn at kRustyBase..kRustyBase+12.
    EffectRack* getRustyDrumsBusRack();

    // ── Per-page instrument EffectRacks ────────────────────────────────────────
    // These sit between each engine's pre-rack page EQ and the bus sum.
    // Layer page IDs in Effects dropdown = 200 + pageIdx (0-7)
    // Bass page IDs in Effects dropdown  = 300 + pageIdx (0-3)
    EffectRack* getLayerPageRack(int pageIdx);
    EffectRack* getBassPageRack (int pageIdx);

    // Aux insert rack (nullptr if the aux strip doesn't exist yet)
    EffectRack* getAuxRack      (int auxIdx);

    // ── Bus post-rack EQ access (Effects Page EQ tab per channel) ─────────────
    // Each channel has its own independent EQ8MsDSP that runs after the EffectRack.
    // §P4.3 B7: pre-rack EQs now live on the same bus nodes as preEq members
    // (getXxxBusPreEQ accessors).  Pages bind their EQ display to those.
    EQ8MsDSP* getLayersBusEQ();
    EQ8MsDSP* getBassBusEQ();
    EQ8MsDSP* getDrumsBusEQ();
    EQ8MsDSP* getMasterEQ();
    EQ8MsDSP* getEffectsBusEQ();
    EQ8MsDSP* getAudioClipsBusEQ();       // ID 6 in Effects dropdown
    EQ8MsDSP* getVoxBusEQ();              // R3.5
    EQ8MsDSP* getInstBusEQ();             // R3.5
    // G-6 (2026-04-29): post-rack EQs for secondary Vox/Inst buses.
    EQ8MsDSP* getVoxBus2EQ();
    EQ8MsDSP* getInstBus2EQ();
    EQ8MsDSP* getInstBus3EQ();
    EQ8MsDSP* getRustyDrumsBusEQ();        // J-4

    // §P4.3: Pre-rack bus EQs - fresh EQ8MsDSP per bus, runs at the very start
    // of each bus's processBlock chain (input -> preEq -> rack -> postEq -> fader).
    // Bound by the corresponding mixer-strip Effects-page Pre EQ8 M/S tab
    // (NOT by player pages - those use per-insert pre-EQ instead).
    EQ8MsDSP* getLayersBusPreEQ();
    EQ8MsDSP* getBassBusPreEQ();
    EQ8MsDSP* getDrumsBusPreEQ();
    EQ8MsDSP* getMasterPreEQ();
    EQ8MsDSP* getEffectsBusPreEQ();
    EQ8MsDSP* getAudioClipsBusPreEQ();
    EQ8MsDSP* getVoxBusPreEQ();           // R3.5
    EQ8MsDSP* getInstBusPreEQ();          // R3.5
    // G-6 (2026-04-29): pre-rack EQs for secondary Vox/Inst buses.
    EQ8MsDSP* getVoxBus2PreEQ();
    EQ8MsDSP* getInstBus2PreEQ();
    EQ8MsDSP* getInstBus3PreEQ();
    EQ8MsDSP* getRustyDrumsBusPreEQ();     // J-4

    // ── PDC - Plugin Delay Compensation ──────────────────────────────────────
    // Call from message thread after any effect is loaded/removed/bypassed.
    // Recomputes per-bus latency, resizes compensation ring buffers, and
    // updates the total latency exposed to the host via getLatencySamples().
    // Returns the total latency in samples (master rack latency + max bus latency).
    int updateBusLatencies();

    // QA-Fe2 PDC (2026-07-16): per-Vox-strip ENGINE-side chain latency
    // (BaySickVocal rack -- the spectral De-esser and De-reverb report real
    // FFT latency -- plus the strip's NAM/IR oversampling).  Queried by
    // updateBusLatencies so the vocal chain joins the compensation set.
    // Wired once by VibeSynthProcessor; message thread only
    // (updateBusLatencies allocates in setDelay).
    std::function<int(int voxIdx)> onGetVoxStripChainLatency;

    // QA-Fe2 PDC full-graph pass (2026-07-16): Inst analog of the Vox hook.
    // Inst strips host an EngineChainProcessor (sfizz -> Pedals -> NAM/IR);
    // NAM/IR reports oversampling latency via setLatencySamples, which was
    // written but never read by any aggregator before this hook.
    std::function<int(int instIdx)> onGetInstStripEngineLatency;

    // Total algorithmic latency (strip-stage + bus-stage compensation target
    // + master chain).  Read by VibeSynthProcessor::getLatencySamples() and,
    // on the audio thread, by the metronome offset + master-recorder trim.
    std::atomic<int> totalLatencySamples { 0 };

    // ── Rack + bus EQ state serialization ────────────────────────────────────
    // Covers all 5 bus nodes + all instrument channel nodes.
    // loadRackStates is safe to call before buildFixedTopology - state is applied
    // at the end of the first buildFixedTopology call (deferred via ValueTree copy).
    void saveRackStates(juce::ValueTree& parent);
    void loadRackStates(const juce::ValueTree& parent);

    // 2026-04-24: wipe every rack slot across every bus + insert + instr
    // channel.  Needed before loading a fresh project or File > New, because
    // loadRackStates only RESTORES entries present in the tree - any rack
    // without a matching entry would keep its old session's effect chain.
    void clearAllRackStates();

    // ── Dynamic instrument channel registry ───────────────────────────────────
    // Each mixer channel that isn't a bus gets its own rack + post-rack EQ.
    // IDs are assigned by addInstrChannel() and are stable for the session.
    // Audio routing for non-bus channels is wired in Phase 2/3; containers exist now.
    //
    // Initial channels registered in buildFixedTopology:
    //   "Layer 1" (id ~100), "Bass 1" (~101), "Drum Ch 1"–"Drum Ch 14" (~102–115)
    //
    // EffectsPage sets onInstrChannelListChanged to rebuild its dropdown dynamically
    // whenever a new page is opened/closed.
    std::function<void()>   onInstrChannelListChanged;   // fired on add/remove

    int                     addInstrChannel(const juce::String& displayName);
    void                    removeInstrChannel(int channelId);
    juce::String            getInstrChannelName(int channelId) const;
    std::vector<int>        getInstrChannelIds() const;  // stable insertion order
    EffectRack*             getInstrChannelRack(int channelId);
    EQ8MsDSP*               getInstrChannelEQ(int channelId);

    // ── Per-clip audio row channels (IDs 400 + row) ──────────────────────────
    // One rack+EQ per arrangement row, created on clip import.
    // Included in getInstrChannelIds() so they appear in the Effects dropdown.
    void        addAudioRowChannel (int row, const juce::String& displayName);
    EffectRack* getAudioRowRack    (int row);
    EQ8MsDSP*   getAudioRowEQ      (int row);
    bool        hasAudioRowChannel (int row) const;

    // ── 5F-4a: Per-insert audio nodes (new architecture) ─────────────────────
    // Each insert gets its own rack + post-rack EQ + polarity/width/fader path.
    // Driven by APVTS reads in the audio thread (memoized per block).
    // Batch 1 declares the API; Batch 2 defines InsertNode and the process loop.
    enum class InsertKind { Layer, Bass, Drum, Audio, Aux, Vox, Inst, Rusty };

    struct InsertNode;

    // Called on the message thread when a strip is created (engine registered,
    // drum slot assigned, or audio clip imported on a new row).
    // Safe to call repeatedly - no-op if the node already exists.
    // apvtsPrefix is the strip's APVTS prefix (e.g. "mixer_layer_0").
    InsertNode* ensureInsertNode(InsertKind kind, int index,
                                  const juce::String& displayName,
                                  const juce::String& apvtsPrefix);
    void        removeInsertNode(InsertKind kind, int index);
    InsertNode* getInsertNode   (InsertKind kind, int index);

    // Return the EffectRack owned by an InsertNode (or nullptr if the node
    // doesn't exist). Lets non-VibeGraph code resolve what effect is loaded
    // in a given insert slot without touching the forward-declared struct.
    EffectRack* getInsertRack   (InsertKind kind, int index);

    // QA-RustyMeter (2026-05-30): UI-thread RMS drain for the split meter's
    // scrolling top half.  exchange-resets the (kind,index) insert node's rms
    // atoms; returns {-inf,-inf} if absent.  No PluginProcessor mirror needed.
    std::pair<float, float> drainInsertNodeRms (InsertKind kind, int index) noexcept;

    // QA-RustyMeter part 2 (2026-05-30): UI-thread RMS drain for a BUS strip's
    // split meter.  exchange-resets the per-bus rms member atoms below (CAS-maxed
    // audio-side by publishRms in processBus/processEffectsBus); returns
    // {-inf,-inf} for kMaster (Full layout, no RMS) or an unknown id.  Direct
    // VibeGraph read -- no PluginProcessor mirror, parallel to drainInsertNodeRms.
    std::pair<float, float> drainBusRms (int busChId) noexcept;

    // QA-RustyMeter Task 3 (2026-05-30): master-bus EBU R128 LUFS readout.
    // getMasterLufs(mode): 0=Momentary, 1=Short-Term, 2=Integrated (-120 floor).
    // resetMasterLufsIntegrated(): clear the gated Integrated accumulation on
    // transport play-from-top / loop (M/S keep running).  Owned by MasterBusNode.
    float getMasterLufs (int mode) const noexcept;
    void  resetMasterLufsIntegrated() noexcept;

    // Return the post-rack EQ owned by an InsertNode (or nullptr). Parallels
    // getInsertRack - same opaque-InsertNode reason.
    EQ8MsDSP*   getInsertEQ     (InsertKind kind, int index);
    EQ8MsDSP*   getInsertPreEQ  (InsertKind kind, int index);   // §P4.3

    // 2026-05-05 dirty-flag wiring: fired from every VibeGraph-owned rack's
    // onSlotsChanged (per-page racks, bus racks, insert racks).  PluginProcessor
    // wires this to its own onAnyStateChange so EffectsPage-driven rack
    // lifecycle (slot type swap, move-up/down, clear, bypass) flips the
    // project dirty bit.  Per-slot APVTS edits already do via the main
    // PluginProcessor's listener.
    std::function<void()> onAnyRackChanged;
    // After setting onAnyRackChanged, call this once to install the wiring on
    // every currently-existing rack (bus + per-page + every insert).  ALSO
    // arms ensureInsertNode to wire any future InsertNode rack the same way.
    void rebindAllRackHooks();

    // QA-AudioMeters (2026-05-24): per-insert peak readers moved to
    // VibeSynthProcessor::drainInsertPeakDbStereo() — UI consumers exchange-
    // reset the per-kind PluginProcessor mirrors (m<Kind>InsertPeakDb*).  The
    // VibeGraph-side per-insert getters / drainers + the peakDbSnap layer are
    // gone (replaced by InsertNode publishPeakReading + processInsert
    // exchange-store + drainMeterAtomicsForUI 8-per-kind G1 drain).

    // QA-AudioMeters (2026-05-24): end-of-audio-block promotion of every rack's
    // slot peak snapshots (one per slot per rack across every InsertNode + every
    // BusNode).  PluginProcessor::processBlock calls this after the render
    // dispatch returns.  Pre-batch this function ALSO promoted per-insert
    // peakDb -> peakDbSnap; the peakDbSnap layer was removed when InsertNode
    // adopted the bus-pattern publishPeakReading + processInsert end-of-call
    // exchange-store, so only the rack-slot promotion remains.
    void promoteAllRackSlotSnapshots();

    // D3: read this insert's choke group (0 = none, 1..16 = group id).
    // Returns 0 if the node doesn't exist or the param isn't bound.
    // Wait-free - reads the cached atomic pChokeGroup pointer.
    int         getInsertChokeGroup (InsertKind kind, int index) const;

    // 5F-4a Batch 6: run an insert node's process loop in-place on buf.
    // Provided as a wrapper so callers don't need the full InsertNode definition.
    // No-op if the node doesn't exist.
    void        processInsert   (InsertKind kind, int index,
                                  juce::AudioBuffer<float>& buf,
                                  double bpm, bool anySolo);

    // Batch 8 (2026-05-06): run the master bus DSP in-place on sumBuf.
    // Called by MasterTask.  Pushes SC array, runs the master node's
    // processBlock, drains peak meters into the VibeGraph-level mirror
    // atomics.  No-op if the master node hasn't been built.
    void        processMasterBus(juce::AudioBuffer<float>& sumBuf, double bpm);

    // 5F-4a Batch 6: cache APVTS pointers inside bus + master nodes. Call after
    // ensureMixerBusAndMasterParams() so the nodes can read polarity/width in
    // their audio processing. Safe to call repeatedly.
    void rebindBusApvts();

    // 5F-4a Batch 6: true if any insert node has its _solo param on.
    // Called once per audio block by PluginProcessor; O(N) over insert nodes.
    bool isAnyInsertSoloed() const noexcept;

    // QA-Ea Part A (2026-05-21): true if any of the 11 bus _solo params is on.
    // GUARDRAIL (per §9 nineteenth Forks): reads BUS _solo ONLY.  MUST NEVER
    // call isAnyInsertSoloed() (strip-level) -- the prior bug muted whole
    // buses when one strip soloed.  Per-strip _solo is a separate axis owned
    // by InsertNodes and
    // is untouched.
    //
    // O(11) cached-atomic loads per call; safe to call from the audio thread.
    // Cached pointers bound in rebindBusApvts() (called from prepareToPlay, so
    // rebound on sample-rate / block-size change).  Pointers stay valid across
    // project state loads by JUCE contract: replaceState swaps the ValueTree
    // but the RangedAudioParameter objects (whose internal atomics these point
    // at) persist for the APVTS lifetime -- so no rebind on load is needed.
    bool anyBusSoloed() const noexcept;

    // 5F-4a Batch 6: apply audio-clips-bus polarity + M/S width in-place on buf.
    // Called by PluginProcessor on the audio thread after the clips bus rack runs.
    void applyAudioClipsBusPolarityWidth(juce::AudioBuffer<float>& buf);
    // R3.5: same shape, applied to Vox / Inst bus accumulators after rack/EQ.
    void applyVoxBusPolarityWidth (juce::AudioBuffer<float>& buf);
    void applyInstBusPolarityWidth(juce::AudioBuffer<float>& buf);
    // G-6 (2026-04-29): polarity/width application for secondary buses.
    void applyVoxBus2PolarityWidth (juce::AudioBuffer<float>& buf);
    void applyInstBus2PolarityWidth(juce::AudioBuffer<float>& buf);
    void applyInstBus3PolarityWidth(juce::AudioBuffer<float>& buf);
    // J-4: BaySickRustyDrums bus polarity/width.
    void applyRustyDrumsBusPolarityWidth(juce::AudioBuffer<float>& buf);

    // C.4 Phase 1 (2026-04-30): per-strip SC receive buffer set.  Each strip
    // can hold up to kMaxScRecvsPerStrip stereo SC inputs from upstream
    // sources; consumed by DSP modules on the strip via setSidechainBuffers.
    // Lazy: nullptr until first use.  Cleared (but not freed) at top of each
    // block alongside channel accumulators.  Slot index 0..3 maps directly
    // to _sc_recv{N}_from APVTS / _sc_pick lookups.
    static constexpr int kMaxScRecvSlots = RoutingGraph::kMaxScRecvsPerStrip;
    juce::AudioBuffer<float>* getScRecvBuffer (int channelId, int slotIdx);
    // Returns a pointer to the channel's full SC array (4 stereo buffers).
    // Slots that have never been allocated are nullptr in the returned array.
    // The array itself is owned by VibeGraph; do not free.
    using ScRecvArray = std::array<juce::AudioBuffer<float>*, kMaxScRecvSlots>;
    ScRecvArray getScRecvArray (int channelId);
    // Clear all SC receive buffers (call at the top of each block).
    void clearScRecvBuffers();
    // QA-Fe2 SC delay-match (docket 1b): pre-compensation key tap for an SC
    // SOURCE channel.  nullptr when the channel isn't an armed SC source or
    // has no stash (Master) -- caller falls back to the post-everything
    // output buffer.  Audio thread.
    const juce::AudioBuffer<float>* getScSourceTap (int channelId) const;
    // Apply the per-(consumer, slot) key-alignment delay in place on the SC
    // receive buffer (values solved on the message thread by
    // updateBusLatencies).  Audio thread, allocation-free.
    void applyScRecvDelay (int channelId, int slotIdx, int numSamples);
    // Push the strip's SC array to its preEq + rack + postEq DSP modules.
    // Address-only push -- the actual buffer contents are filled by upstream
    // SC fanout (pullSidechainPredecessorsToGraph under MT), AFTER topo-sorted
    // source strips process.
    // Called by VibeGraph internally for inserts + buses + master, and by
    // PluginProcessor for the Vox/Inst bus loop strips.
    void pushScArrayToStrip (int channelId);

    // Routing graph used by PluginProcessor to look up per-insert main/send
    // destinations. Rebuilt from APVTS each block via rebuildRoutingFromApvts.
    RoutingGraph& getRoutingGraph() noexcept { return mRoutingGraph; }

    // Rebuild the active-channel list + routing edges + topological order from
    // APVTS. Called by PluginProcessor at the top of each processBlock.
    void rebuildRoutingFromApvts();

    // 5F-4b B2: list the ids of currently-registered aux inserts (for UI iter).
    std::vector<int> getAuxIndices() const;

    // QA-Ef #4 (2026-05-22): clear every registered aux InsertNode.  Called on
    // project load via VibeSynthProcessor::clearAllAuxInserts from the three
    // load-entry points (deserializeProject / setStateInformation / doFileNew /
    // loadTemplate) BEFORE restoreAuxStripsFromState rebuilds from the loaded
    // project -- without this, auxes from the prior project's session persist
    // and the load layers the new project's auxes on top of them ("open 16
    // auxes, load another project, all 16 still there").  Safe under the load
    // shield; audio thread bails before iterating tasks.
    void clearAuxInserts();

    // ── Level meters (written by audio thread, read by UI timer) ─────────────
    std::atomic<float> layersPeakDb  { -60.f };
    std::atomic<float> bassPeakDb    { -60.f };
    std::atomic<float> drumsPeakDb   { -60.f };
    std::atomic<float> masterPeakDb  { -60.f };
    // 2026-04-30: stereo L/R for split DBFSMeter.  Mono atomics above kept
    // (= max(L, R)) for back-compat with legacy readers.
    std::atomic<float> layersPeakDbL { -60.f };
    std::atomic<float> layersPeakDbR { -60.f };
    std::atomic<float> bassPeakDbL   { -60.f };
    std::atomic<float> bassPeakDbR   { -60.f };
    std::atomic<float> drumsPeakDbL  { -60.f };
    std::atomic<float> drumsPeakDbR  { -60.f };
    std::atomic<float> masterPeakDbL { -60.f };
    std::atomic<float> masterPeakDbR { -60.f };
    std::atomic<float> fxBusPeakDb   { -60.f };
    std::atomic<float> fxBusPeakDbL  { -60.f };
    std::atomic<float> fxBusPeakDbR  { -60.f };
    std::atomic<float> audioClipsPeakDb  { -60.f };
    std::atomic<float> audioClipsPeakDbL { -60.f };
    std::atomic<float> audioClipsPeakDbR { -60.f };
    std::atomic<float> voxBusPeakDb      { -60.f };
    std::atomic<float> voxBusPeakDbL     { -60.f };
    std::atomic<float> voxBusPeakDbR     { -60.f };
    std::atomic<float> voxBus2PeakDb     { -60.f };
    std::atomic<float> voxBus2PeakDbL    { -60.f };
    std::atomic<float> voxBus2PeakDbR    { -60.f };
    std::atomic<float> instBusPeakDb     { -60.f };
    std::atomic<float> instBusPeakDbL    { -60.f };
    std::atomic<float> instBusPeakDbR    { -60.f };
    std::atomic<float> instBus2PeakDb    { -60.f };
    std::atomic<float> instBus2PeakDbL   { -60.f };
    std::atomic<float> instBus2PeakDbR   { -60.f };
    std::atomic<float> instBus3PeakDb    { -60.f };
    std::atomic<float> instBus3PeakDbL   { -60.f };
    std::atomic<float> instBus3PeakDbR   { -60.f };
    std::atomic<float> rustyDrumsBusPeakDb  { -60.f };
    std::atomic<float> rustyDrumsBusPeakDbL { -60.f };
    std::atomic<float> rustyDrumsBusPeakDbR { -60.f };

    // QA-RustyMeter part 2 (2026-05-30): per-bus windowed-RMS atoms for the
    // split meter's scrolling top half.  11 non-master buses x L/R (Master keeps
    // a full peak bar, no RMS).  CAS-maxed audio-side by publishRms in processBus
    // / processEffectsBus (never reset there); the UI exchange-resets via
    // drainBusRms.  No mono sibling + no PluginProcessor mirror -- the UI reads
    // these directly off VibeGraph, parallel to the InsertNode rms atoms.  The
    // ~50 ms window smoothing lives UI-side in DBFSMeter::onVBlank.
    std::atomic<float> layersRmsDbL        { -60.f }, layersRmsDbR        { -60.f };
    std::atomic<float> bassRmsDbL          { -60.f }, bassRmsDbR          { -60.f };
    std::atomic<float> drumsRmsDbL         { -60.f }, drumsRmsDbR         { -60.f };
    std::atomic<float> fxBusRmsDbL         { -60.f }, fxBusRmsDbR         { -60.f };
    std::atomic<float> audioClipsRmsDbL    { -60.f }, audioClipsRmsDbR    { -60.f };
    std::atomic<float> voxBusRmsDbL        { -60.f }, voxBusRmsDbR        { -60.f };
    std::atomic<float> voxBus2RmsDbL       { -60.f }, voxBus2RmsDbR       { -60.f };
    std::atomic<float> instBusRmsDbL       { -60.f }, instBusRmsDbR       { -60.f };
    std::atomic<float> instBus2RmsDbL      { -60.f }, instBus2RmsDbR      { -60.f };
    std::atomic<float> instBus3RmsDbL      { -60.f }, instBus3RmsDbR      { -60.f };
    std::atomic<float> rustyDrumsBusRmsDbL { -60.f }, rustyDrumsBusRmsDbR { -60.f };

    // QA-AudioMeters (2026-05-24): per-kind insert peak atomics, parallel to the
    // per-bus atomics above.  InsertNode::process publishes via publishPeakReading;
    // processInsert exchange-stores node->peakDb*/L/R into the per-kind array slot;
    // drainMeterAtomicsForUI drains them into PluginProcessor mirrors that the UI
    // polls.  All 8 InsertKinds adopt this unified G1 pattern (ends the bus-vs-
    // insert architectural split QA-Eg's bus migration left exposed).
    static constexpr int kMaxAudioInserts = 50;  // matches VibeSynthProcessor::kMaxAudioRows + MixerState::kMaxAudioRows (static_assert in .cpp)

    // QA-AudioMeters fix-up (2026-05-24): mono <kind>InsertPeakDb members
    // deleted as dead writes (no UI consumer ever read them; the UI reads L/R
    // only via VibeSynthProcessor::drainInsertPeakDbStereo).
    std::array<std::atomic<float>, kMaxLayerPages>                    layerInsertPeakDbL  {}, layerInsertPeakDbR  {};
    std::array<std::atomic<float>, kMaxBassPages>                     bassInsertPeakDbL   {}, bassInsertPeakDbR   {};
    std::array<std::atomic<float>, kMaxDrumPages>                     drumInsertPeakDbL   {}, drumInsertPeakDbR   {};
    std::array<std::atomic<float>, kMaxAudioInserts>                  audioInsertPeakDbL  {}, audioInsertPeakDbR  {};
    std::array<std::atomic<float>, MixerChannelIds::kMaxAuxStrips>    auxInsertPeakDbL    {}, auxInsertPeakDbR    {};
    std::array<std::atomic<float>, MixerChannelIds::kMaxVoxStrips>    voxInsertPeakDbL    {}, voxInsertPeakDbR    {};
    std::array<std::atomic<float>, MixerChannelIds::kMaxInstStrips>   instInsertPeakDbL   {}, instInsertPeakDbR   {};
    std::array<std::atomic<float>, MixerChannelIds::kMaxRustyStrips>  rustyInsertPeakDbL  {}, rustyInsertPeakDbR  {};

    // ── Phase-2 instrument node registry ─────────────────────────────────────
    // Nodes registered here will be integrated into the processing graph in a
    // future session when the full AudioProcessorGraph path is wired.
    NodeID addInstrumentNode(std::unique_ptr<juce::AudioProcessor> proc, int trackId);
    void   removeInstrumentNode(int trackId);
    bool   hasNode(int trackId) const;
    int    getNodeCount() const { return (int)mTrackNodes.size(); }

    juce::AudioProcessorGraph& getGraph() { return mGraph; }

private:
    // ── Forward-declared nested bus node types (defined in VibeGraph.cpp) ─────
    // unique_ptr with incomplete type - destructor defined in VibeGraph.cpp
    struct LayersBusNode;
    struct BassBusNode;
    struct DrumsBusNode;
    struct MasterBusNode;
    struct EffectsBusNode;
    struct InstrChannelNode;   // generic rack+EQ container for every non-bus channel

    std::unique_ptr<LayersBusNode>  mLayersNode;
    std::unique_ptr<BassBusNode>    mBassNode;
    std::unique_ptr<DrumsBusNode>   mDrumsNode;
    std::unique_ptr<MasterBusNode>  mMasterNode;
    std::unique_ptr<EffectsBusNode> mEffectsBusNode;
    std::unique_ptr<InstrChannelNode> mAudioClipsBusNode;  // rack+EQ for all audio clips (ID 6)

    // QA-Ea Part A (2026-05-21): cached bus _solo atomic pointers for the
    // anyBusSoloed() helper.  Bound in rebindBusApvts().  Order matches
    // kBusSoloPrefixes[11] in VibeGraph.cpp (layers / bass / drums / fx /
    // clipsbus / voxbus / instbus / voxbus2 / instbus2 / instbus3 / rustybus).
    // CPU-safeguarding standing rule: avoid string-keyed getRawParameterValue
    // lookups per audio block; cache the raw atomic ptrs once + reuse.
    std::array<std::atomic<float>*, 11> mBusSoloPtr {};

    // Instrument channel nodes: keyed by the ID returned by addInstrChannel().
    // Insertion order preserved via mInstrChannelOrder for dropdown display.
    std::map<int, std::unique_ptr<InstrChannelNode>> mInstrChannelNodes;
    std::vector<int>                                 mInstrChannelOrder;
    int                                              mNextInstrChannelId { 100 };

    // Per-page instrument EffectRacks - always live, prepared with the graph.
    // Applied after per-page EQ, before the bus sum, in PluginProcessor::processBlock.
    // DEPRECATED 5F-4a: migrated into InsertNode storage (now in mInsertsByChannel
    // below post-QA-InsertMaps 2026-05-24).  Kept as the legacy fallback path used
    // by getLayerPageRack / getBassPageRack when no InsertNode has been ensured at
    // the corresponding chId yet (e.g. on a fresh project before any Layer / Bass
    // strip is auditioned).
    std::array<EffectRack, kMaxLayerPages> mLayerPageRacks;
    std::array<EffectRack, kMaxBassPages>  mBassPageRacks;

    // ── 5F-4a: Per-insert node storage (keyed by insert index) ───────────────
    // QA-InsertMaps (2026-05-24): flat-array storage by ChannelId.  Replaces
    // the 8 per-kind `std::map<int, std::unique_ptr<InsertNode>>` tables
    // (Layer / Bass / Drum / Audio / Aux / Vox / Inst / Rusty).  Lookups
    // become single-pointer indirection -- the selectInsertMap kind-switch +
    // red-black-tree walks die.  Sparsely populated (chId range 0..812
    // currently used; ~187 nullptr slots at full capacity).  Companion
    // mLiveInsertChannels list is iterated by every site that used to walk
    // the per-kind maps (addInsertMap XML save, restoreInsert XML restore,
    // walkInserts, promoteRacksInMap, isAnyInsertSoloed,
    // rebuildRoutingFromApvts, prepare / reset sweeps).  ChId computed via
    // `computeChannelId(kind, index)` helper in VibeGraph.cpp; cached on
    // `InsertNode::chId` at construction so the audio-thread `processInsert`
    // path reads it directly.  See §5 QA-InsertMaps + §9 thirty-third Forks.
    static constexpr int kMaxStripChannels = 1000;
    std::array<std::unique_ptr<InsertNode>, kMaxStripChannels> mInsertsByChannel;
    std::vector<int>                                            mLiveInsertChannels;

    // R1: new bus nodes for live-input strip routing.  Same shape as existing
    // Effects/Clips bus nodes - Fx rack + pre/post EQ + fader + meter.  Vox
    // strips (600..605) sum into mVoxBusNode; Inst strips (700..705) sum into
    // mInstBusNode; both buses route to Master.
    std::unique_ptr<InstrChannelNode> mVoxBusNode;
    std::unique_ptr<InstrChannelNode> mInstBusNode;
    // G-6 (2026-04-29): secondary buses (kVoxBus2 / kInstBus2 / kInstBus3).
    std::unique_ptr<InstrChannelNode> mVoxBus2Node;
    std::unique_ptr<InstrChannelNode> mInstBus2Node;
    std::unique_ptr<InstrChannelNode> mInstBus3Node;
    // J-4 (2026-05-03): dedicated bus for BaySickRustyDrums.  Always allocated
    // (matches Vox/Inst pattern) so audio routing works whether or not a
    // BaySickRustyDrums instance exists - bus sums silence cheaply until a
    // kit's 13 strips are registered via ensureRustyInsertNode.
    std::unique_ptr<InstrChannelNode> mRustyDrumsBusNode;
    // mRustyInserts std::map removed by QA-InsertMaps 2026-05-24 (flattened into
    // mInsertsByChannel above; chId range 800..812 for Rusty kit strips).

    // Deferred rack state: set by loadRackStates() if topology not yet built;
    // applied at the end of buildFixedTopology().
    juce::ValueTree mPendingRackState;
    void applyRackStates(const juce::ValueTree& parent);

    double mSampleRate    { 44100.0 };
    int    mBlockSize     { 512 };
    bool   mTopologyBuilt { false };

    // APVTS pointer captured at buildFixedTopology() - used by ensureInsertNode()
    // to rebind each insert's cached param pointers on creation.
    juce::AudioProcessorValueTreeState* mApvts { nullptr };

    // Global "kill-all" FX bypass, read by every rack process site each block.
    // Cached in buildFixedTopology() so audio-thread reads are lock-free.
    std::atomic<float>* mGlobalFxBypassPtr { nullptr };

    // ── PDC compensation delay line ──────────────────────────────────────────
    // A simple stereo ring buffer. setDelay() resizes and resets it on the
    // message thread (safe because processBlock() only runs between
    // prepareToPlay calls or when the topology is stable). process() is
    // audio-thread only.  Nested here (moved from the .cpp at QA-Fe2) so the
    // SC key-alignment array below can hold it by value.
    struct CompDelayLine
    {
        // 32768 (~680 ms @ 48k) headroom: a vocal chain (De-reverb 2048 +
        // spectral De-esser 2048 + limiter lookahead) stacked with an
        // HQ-Linear strip EQ (2x ~2050 mid+side) can pass 8192.  setDelay
        // allocates the FULL cap (256 KB stereo) for any line it activates
        // and a line returned to 0 keeps it; only never-activated lines cost
        // nothing -- a few MB total across a fully latent session.  Beyond
        // the cap the delay silently clamps (misaligned but stable).
        static constexpr int kMaxSamples = 32768;

        void setDelay(int delaySamples, int numChannels)
        {
            mDelay = juce::jlimit(0, kMaxSamples, delaySamples);
            mNumCh = numChannels;
            mBuf.assign((size_t)(mNumCh * kMaxSamples), 0.f);
            mWrite = 0;
        }

        // In-place: reads delayed sample, writes current sample, advances pointer.
        void process(juce::AudioBuffer<float>& buf)
        {
            if (mDelay == 0 || mBuf.empty()) return;

            const int n = buf.getNumSamples();
            for (int s = 0; s < n; ++s)
            {
                int readPos = (mWrite - mDelay + kMaxSamples) % kMaxSamples;
                for (int ch = 0; ch < mNumCh; ++ch)
                {
                    float* chBuf = mBuf.data() + ch * kMaxSamples;
                    float delayed = chBuf[readPos];
                    chBuf[mWrite]  = buf.getSample(ch, s);
                    buf.setSample(ch, s, delayed);
                }
                mWrite = (mWrite + 1) % kMaxSamples;
            }
        }

        int mDelay { 0 };
        int mNumCh { 2 };
        int mWrite  { 0 };
        std::vector<float> mBuf;
    };

    // QA-Fe2 SC delay-match (docket 1b): per-(consumer, slot) key-alignment
    // delay lines.  Fixed array (not inside ScSet) because mScRecv is
    // audio-thread-owned (rebuildRoutingFromApvts inserts at block rate) and
    // the message-thread solver must never race the map; empty lines cost
    // ~48 bytes each, setDelay allocates only on active receives.
    std::array<std::array<CompDelayLine, (size_t) kMaxScRecvSlots>,
               (size_t) kMaxStripChannels> mScRecvDelays;

    // Audio thread (block rate, from rebuildRoutingFromApvts): flag every
    // SC-edge source node so its process stashes the pre-compensation tap.
    // Allocation-free (relaxed atomic bools on the nodes).
    void armScSourceTaps();

    // 5F-4b B1b: routing state
    RoutingGraph                                       mRoutingGraph;

    // C.4 Phase 1: per-channel SC receive buffer set (4 stereo bufs per strip).
    // Each ScSet is allocated lazily on first ensureScRecvBuffers() / getScRecvBuffer.
    struct ScSet { std::array<juce::AudioBuffer<float>, kMaxScRecvSlots> bufs; };
    std::unordered_map<int, ScSet>                     mScRecv;
    std::vector<std::pair<int, juce::String>>          mActiveChannels;

    // ── Phase-2 node registry (AudioProcessorGraph, not yet driving audio) ────
    juce::AudioProcessorGraph mGraph;
    std::map<int, NodeID>     mTrackNodes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VibeGraph)
};
