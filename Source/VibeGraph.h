#pragma once
#include <JuceHeader.h>
#include <unordered_map>
#include <functional>
#include "DSP/SpectrumFeed.h"
#include "DSP/EQ8MsDSP.h"
#include "EffectRack.h"
#include "VibesynthConstants.h"

// Forward declarations — full headers included in VibeGraph.cpp only
class BassSynth;
// DrumSynth forward-decl removed — class no longer used in graph (2026-04-25).

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
    constexpr int kOutput    = 0;    // Terminal sink — only Master routes here
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
    // — only rendered on Mixer after user clicks "Add Vox/Inst Bus".
    constexpr int kVoxBus2   = 9;    // G-6: optional 2nd Vox bus
    constexpr int kInstBus2  = 10;   // G-6: optional 2nd Inst bus
    constexpr int kInstBus3  = 11;   // G-6: optional 3rd Inst bus
    constexpr int kAuxBase   = 100;  // Aux 0..17 → 100..117 (G-7 polish: 16 → 18)
    constexpr int kLayerBase = 200;  // Layer insert 0..15 → 200..215
    constexpr int kBassBase  = 300;  // Bass insert 0..15 → 300..315
    constexpr int kAudioBase = 400;  // Audio insert 0..49 → 400..449
    constexpr int kDrumBase  = 500;  // Drum insert 0..15 → 500..515
    constexpr int kVoxBase   = 600;  // R1: Vox insert 0..5 → 600..605
    constexpr int kInstBase  = 700;  // R1: Inst insert 0..5 → 700..705

    constexpr int kMaxVoxStrips  = 6;   // R1
    constexpr int kMaxInstStrips = 20;  // R1; bumped 6 → 10 in G-4 (2026-04-28); 10 → 20 in G-6 (2026-04-29)
    constexpr int kMaxAuxStrips  = 18;  // 5F-4b B2; bumped 16 → 18 in G-7 polish (2026-04-29)

    inline int layerInsert (int idx) { return kLayerBase + idx; }
    inline int bassInsert  (int idx) { return kBassBase  + idx; }
    inline int drumInsert  (int idx) { return kDrumBase  + idx; }
    inline int audioInsert (int idx) { return kAudioBase + idx; }
    inline int auxStrip    (int idx) { return kAuxBase   + idx; }
    inline int voxInsert   (int idx) { return kVoxBase   + idx; }
    inline int instInsert  (int idx) { return kInstBase  + idx; }

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
        }
        if (chId >= kLayerBase && chId < kLayerBase + 16)           return "mixer_layer_" + juce::String(chId - kLayerBase);
        if (chId >= kBassBase  && chId < kBassBase  + 16)           return "mixer_bass_"  + juce::String(chId - kBassBase);
        if (chId >= kDrumBase  && chId < kDrumBase  + 16)           return "mixer_drum_"  + juce::String(chId - kDrumBase);
        if (chId >= kAudioBase && chId < kAudioBase + 50)           return "mixer_audio_" + juce::String(chId - kAudioBase);
        if (chId >= kAuxBase   && chId < kAuxBase   + kMaxAuxStrips) return "mixer_aux_"   + juce::String(chId - kAuxBase);
        if (chId >= kVoxBase   && chId < kVoxBase   + kMaxVoxStrips)  return "mixer_vox_"   + juce::String(chId - kVoxBase);
        if (chId >= kInstBase  && chId < kInstBase  + kMaxInstStrips) return "mixer_inst_"  + juce::String(chId - kInstBase);
        return {};
    }

    // Is this channel a bus? (for routing rule enforcement)
    inline bool isBus (int chId)
    {
        return chId == kLayersBus || chId == kBassBus || chId == kDrumsBus
            || chId == kFxBus     || chId == kClipsBus
            || chId == kVoxBus    || chId == kInstBus
            || chId == kVoxBus2   || chId == kInstBus2 || chId == kInstBus3;
    }

    // Is this channel's main-out locked (cannot be rerouted)?
    inline bool isMainOutLocked (int chId)
    {
        return chId == kMaster || isBus(chId);
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
        }
        if (chId >= kLayerBase && chId < kLayerBase + 16) return "Layer " + juce::String(chId - kLayerBase + 1);
        if (chId >= kBassBase  && chId < kBassBase  + 16) return "Bass "  + juce::String(chId - kBassBase  + 1);
        if (chId >= kDrumBase  && chId < kDrumBase  + 16) return "Drum "  + juce::String(chId - kDrumBase  + 1);
        if (chId >= kAudioBase && chId < kAudioBase + 50) return "Audio " + juce::String(chId - kAudioBase + 1);
        if (chId >= kAuxBase   && chId < kAuxBase   + kMaxAuxStrips)  return "Aux "  + juce::String(chId - kAuxBase  + 1);
        if (chId >= kVoxBase   && chId < kVoxBase   + kMaxVoxStrips)  return "Vox "  + juce::String(chId - kVoxBase  + 1);
        if (chId >= kInstBase  && chId < kInstBase  + kMaxInstStrips) return "Inst " + juce::String(chId - kInstBase + 1);
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
        }
        if (channelId >= kLayerBase && channelId < kLayerBase + 16)            return kLayersBus;
        if (channelId >= kBassBase  && channelId < kBassBase  + 16)            return kBassBus;
        if (channelId >= kDrumBase  && channelId < kDrumBase  + 16)            return kDrumsBus;
        if (channelId >= kAudioBase && channelId < kAudioBase + 50)            return kClipsBus;
        if (channelId >= kAuxBase   && channelId < kAuxBase   + kMaxAuxStrips) return kFxBus;
        if (channelId >= kVoxBase   && channelId < kVoxBase   + kMaxVoxStrips)  return kVoxBus;
        if (channelId >= kInstBase  && channelId < kInstBase  + kMaxInstStrips) return kInstBus;
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
//   EffectsBusNode  (FX Bus — receive bus for aux strips + user-routed sends;
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

    // Build the fixed bus topology.  Guards itself — no-op after first call so
    // safe to call every prepareToPlay().  All references must remain valid for
    // the lifetime of this VibeGraph.
    // 12i: SpectrumFeed refs dropped - each EQ8MsDSP owns its own pre/post feeds
    // (populated at its own process() I/O boundary). UI polls eq->preFeed /
    // eq->postFeed directly via ParametricEQDisplay::bindMsDSP.
    // §P4.3 B7 (2026-04-22): all external page-EQ refs dropped.  Every bus now
    // owns its own preEq (pre-rack) and busEq (post-rack) directly.
    // 2026-04-25: DrumSynth removed.  Drums-bus content now exclusively
    // comes from per-drum-tab InsertNode outputs via routeInsertOutput.
    void buildFixedTopology(juce::Synthesiser&                  synth,
                            BassSynth&                          bass,
                            juce::AudioProcessorValueTreeState& apvts);

    // ── Main audio call (replaces direct mixing in PluginProcessor) ───────────
    // midi:  piano-roll MIDI destined for the Layers synth
    // bpm:   current host BPM, forwarded to time-based effects
    void processBlock(juce::AudioBuffer<float>& outputBuffer,
                      juce::MidiBuffer&         midi,
                      double                    bpm,
                      juce::AudioBuffer<float>* layersPreRendered      = nullptr,
                      juce::AudioBuffer<float>* bassPreRendered        = nullptr,
                      juce::AudioBuffer<float>* drumsPreRendered       = nullptr,
                      juce::AudioBuffer<float>* audioClipsPreRendered  = nullptr);

    // C.1 (2026-04-30): runs the FX Bus pipeline on its accumulator buffer
    // (preEq -> rack -> postEq -> polarity -> M/S width -> fader x mute x
    // solo -> pan -> peak meter).  Caller must subsequently routeInsertOutput
    // from kFxBus to fan the result downstream (default = Master).  busAnySolo
    // participates in the receive-group solo gate; panLaw matches the project-
    // level master_pan_law convention used by the Vox/Inst bus loop.
    void processEffectsBus(juce::AudioBuffer<float>& buf, double bpm,
                            bool busAnySolo, int panLaw);

    // C.1 (2026-04-30): post-pipeline FX Bus peak (dBFS), written by
    // processEffectsBus each block.  Returns -60 if EffectsBusNode hasn't
    // been built yet.
    std::pair<float, float> getEffectsBusPeakDbStereo() const;
    // 2026-05-02: drain variant -- exchanges the FxBus node atomics with -inf
    // and returns the running-max pair.  Used by the audio thread per block to
    // reset the per-block window cleanly.  UI should NOT call this -- the
    // const load() variant is the read-only path for UI.
    std::pair<float, float> drainEffectsBusPeakDbStereo();

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
    // (cheap) — the strip activation flag only controls UI presence + route
    // picker filtering.
    EffectRack* getVoxBus2Rack();
    EffectRack* getInstBus2Rack();
    EffectRack* getInstBus3Rack();

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

    // §P4.3: Pre-rack bus EQs — fresh EQ8MsDSP per bus, runs at the very start
    // of each bus's processBlock chain (input -> preEq -> rack -> postEq -> fader).
    // Bound by the corresponding mixer-strip Effects-page Pre EQ8 M/S tab
    // (NOT by player pages — those use per-insert pre-EQ instead).
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

    // ── PDC — Plugin Delay Compensation ──────────────────────────────────────
    // Call from message thread after any effect is loaded/removed/bypassed.
    // Recomputes per-bus latency, resizes compensation ring buffers, and
    // updates the total latency exposed to the host via getLatencySamples().
    // Returns the total latency in samples (master rack latency + max bus latency).
    int updateBusLatencies();

    // Total algorithmic latency (Layers/Bass/Drums compensation + master rack).
    // Read by VibeSynthProcessor::getLatencySamples().
    std::atomic<int> totalLatencySamples { 0 };

    // ── Rack + bus EQ state serialization ────────────────────────────────────
    // Covers all 5 bus nodes + all instrument channel nodes.
    // loadRackStates is safe to call before buildFixedTopology — state is applied
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
    enum class InsertKind { Layer, Bass, Drum, Audio, Aux, Vox, Inst };

    struct InsertNode;

    // Called on the message thread when a strip is created (engine registered,
    // drum slot assigned, or audio clip imported on a new row).
    // Safe to call repeatedly — no-op if the node already exists.
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

    // Return the post-rack EQ owned by an InsertNode (or nullptr). Parallels
    // getInsertRack — same opaque-InsertNode reason.
    EQ8MsDSP*   getInsertEQ     (InsertKind kind, int index);
    EQ8MsDSP*   getInsertPreEQ  (InsertKind kind, int index);   // §P4.3

    // Peak dB atomic for UI (one per slot, per kind). Returns -60 if node doesn't exist.
    float       getInsertPeakDb (InsertKind kind, int index) const;
    // 2026-04-30: stereo L/R peak for split DBFSMeter.  Returns {-60, -60} if
    // node doesn't exist.  Wait-free — both atomics read with relaxed ordering.
    std::pair<float, float> getInsertPeakDbStereo  (InsertKind kind, int index) const;
    // 2026-05-02: drain variant for vblank-locked metering -- exchanges the
    // insert's snapshot atomics with -inf and returns the running max.  UI
    // thread calls this once per vblank; the snapshot is updated by audio
    // ONCE per audio block via promoteAllInsertPeakSnapshots() so UI reads
    // are always consistent across every insert (no mid-block race window).
    std::pair<float, float> drainInsertPeakDbStereo (InsertKind kind, int index);

    // 2026-05-02: end-of-audio-block promotion of all insert peak snapshots.
    // PluginProcessor::processBlock calls this AFTER VibeGraph::processBlock
    // returns, so every insert's snapshot reflects the same just-completed
    // audio block before any UI vblank can read.  Eliminates the layer-vs-
    // bus ping-pong where one meter pulses one frame after the other due to
    // UI sampling between the audio thread's per-node atomic writes.
    void promoteAllInsertPeakSnapshots();

    // D3: read this insert's choke group (0 = none, 1..16 = group id).
    // Returns 0 if the node doesn't exist or the param isn't bound.
    // Wait-free — reads the cached atomic pChokeGroup pointer.
    int         getInsertChokeGroup (InsertKind kind, int index) const;

    // 5F-4a Batch 6: run an insert node's process loop in-place on buf.
    // Provided as a wrapper so callers don't need the full InsertNode definition.
    // No-op if the node doesn't exist.
    void        processInsert   (InsertKind kind, int index,
                                  juce::AudioBuffer<float>& buf,
                                  double bpm, bool anySolo);

    // 5F-4a Batch 6: cache APVTS pointers inside bus + master nodes. Call after
    // ensureMixerBusAndMasterParams() so the nodes can read polarity/width in
    // their audio processing. Safe to call repeatedly.
    void rebindBusApvts();

    // 5F-4a Batch 6: true if any insert node has its _solo param on.
    // Called once per audio block by PluginProcessor; O(N) over insert nodes.
    bool isAnyInsertSoloed() const noexcept;

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

    // 5F-4b B1b: per-channel input accumulator buffers. PluginProcessor sums
    // each InsertNode's output into the correct channel's accumulator based on
    // the strip's _sendTo + _sendN APVTS params. Bus + master nodes then process
    // their accumulator. Keyed by MixerChannelIds. Sized in prepare().
    juce::AudioBuffer<float>* getChannelAccumulator(int channelId);

    // Clear all accumulator buffers to zero at top of each block.
    void clearChannelAccumulators();

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
    // Clear all SC receive buffers (call alongside clearChannelAccumulators).
    void clearScRecvBuffers();
    // Push the strip's SC array to its preEq + rack + postEq DSP modules.
    // Address-only push -- the actual buffer contents are filled by upstream
    // routeInsertOutput SC fanout, AFTER topo-sorted source strips process.
    // Called by VibeGraph internally for inserts + buses + master, and by
    // PluginProcessor for the Vox/Inst bus loop strips.
    void pushScArrayToStrip (int channelId);

    // Routing graph used by PluginProcessor to look up per-insert main/send
    // destinations. Rebuilt from APVTS each block via rebuildRoutingFromApvts.
    RoutingGraph& getRoutingGraph() noexcept { return mRoutingGraph; }

    // Rebuild the active-channel list + routing edges + topological order from
    // APVTS. Called by PluginProcessor at the top of each processBlock.
    void rebuildRoutingFromApvts();

    // 5F-4b B2: process all registered aux inserts. Each aux's input is already
    // summed into its channel accumulator by upstream routeInsertOutput calls.
    // For each aux: run InsertNode::processBlock on the accumulator in-place,
    // then invoke `fanout(auxChannelId, processedBuf)` so the caller can route
    // the aux's output downstream via RoutingGraph edges.
    void processAuxInserts(double bpm, bool anySolo,
                           const std::function<void(int, juce::AudioBuffer<float>&)>& fanout);

    // 5F-4b B2: list the ids of currently-registered aux inserts (for UI iter).
    std::vector<int> getAuxIndices() const;

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
    // unique_ptr with incomplete type — destructor defined in VibeGraph.cpp
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

    // Instrument channel nodes: keyed by the ID returned by addInstrChannel().
    // Insertion order preserved via mInstrChannelOrder for dropdown display.
    std::map<int, std::unique_ptr<InstrChannelNode>> mInstrChannelNodes;
    std::vector<int>                                 mInstrChannelOrder;
    int                                              mNextInstrChannelId { 100 };

    // Per-page instrument EffectRacks — always live, prepared with the graph.
    // Applied after per-page EQ, before the bus sum, in PluginProcessor::processBlock.
    // DEPRECATED 5F-4a: being migrated into InsertNode. Kept compiling through
    // Batch 1–2; Batch 3 removes these and routes to mLayerInserts/mBassInserts.
    std::array<EffectRack, kMaxLayerPages> mLayerPageRacks;
    std::array<EffectRack, kMaxBassPages>  mBassPageRacks;

    // ── 5F-4a: Per-insert node storage (keyed by insert index) ───────────────
    // Created lazily via ensureInsertNode(). Each kind has its own map for O(1)
    // lookup and independent lifecycle. Defined in VibeGraph.cpp alongside
    // InsertNode's full definition.
    std::map<int, std::unique_ptr<InsertNode>> mLayerInserts;
    std::map<int, std::unique_ptr<InsertNode>> mBassInserts;
    std::map<int, std::unique_ptr<InsertNode>> mDrumInserts;
    std::map<int, std::unique_ptr<InsertNode>> mAudioInserts;
    std::map<int, std::unique_ptr<InsertNode>> mAuxInserts;   // 5F-4b B2
    std::map<int, std::unique_ptr<InsertNode>> mVoxInserts;   // R1 (2026-04-23): live-vocal strips
    std::map<int, std::unique_ptr<InsertNode>> mInstInserts;  // R1: live-instrument strips

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

    // Deferred rack state: set by loadRackStates() if topology not yet built;
    // applied at the end of buildFixedTopology().
    juce::ValueTree mPendingRackState;
    void applyRackStates(const juce::ValueTree& parent);

    // ── Pre-allocated scratch buffers (avoid per-block heap allocs) ───────────
    juce::AudioBuffer<float> mLayersBuf;
    juce::AudioBuffer<float> mBassBuf;
    juce::AudioBuffer<float> mDrumsBuf;
    juce::AudioBuffer<float> mSumBuf;

    double mSampleRate    { 44100.0 };
    int    mBlockSize     { 512 };
    bool   mTopologyBuilt { false };

    // APVTS pointer captured at buildFixedTopology() — used by ensureInsertNode()
    // to rebind each insert's cached param pointers on creation.
    juce::AudioProcessorValueTreeState* mApvts { nullptr };

    // Global "kill-all" FX bypass, read by every rack process site each block.
    // Cached in buildFixedTopology() so audio-thread reads are lock-free.
    std::atomic<float>* mGlobalFxBypassPtr { nullptr };

    // 5F-4b B1b: routing state
    RoutingGraph                                       mRoutingGraph;
    std::unordered_map<int, juce::AudioBuffer<float>>  mChannelAccum;

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
