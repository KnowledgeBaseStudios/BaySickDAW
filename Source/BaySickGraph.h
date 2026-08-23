#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <unordered_map>
#include <functional>
#include "DSP/SpectrumFeed.h"
#include "DSP/EQ8MsDSP.h"
#include "EffectRack.h"
#include "BaySickConstants.h"

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
    // QA-ModelShell TS6 (BLU-447, 2026-07-29): hosted VST3 instrument strips.
    // Their inserts are deliberately NOT main-out locked -- Jeff's spec is that
    // a VST strip moves under the Layers or Bass bus exactly as those two
    // already move between each other, which is the existing unlocked
    // `_sendTo` behaviour rather than anything new.
    constexpr int kPluginsBus = 13;
    // QA-Layout T10 (L13): optional secondary group buses on the kVoxBus2
    // pattern -- always-allocated audio, lazy UI strip, spawned from the
    // Mixer's "Add" menu.
    constexpr int kLayersBus2 = 14;
    constexpr int kBassBus2   = 15;
    constexpr int kClipsBus2  = 16;
    constexpr int kPluginsBus2 = 17;
    // QA-SOUNDNESS (2026-08-07, Jeff): the Drum Kit grid's "1-16" and "17-32"
    // buttons are two INDEPENDENT kits, not one kit behind a view filter, so
    // each gets its own bus the way BaySickRustyDrums does.  Same
    // always-allocated shape as the T10 group buses above.
    constexpr int kDrumsBus2  = 18;
    constexpr int kAuxBase   = 100;  // Aux 0..17 → 100..117 (G-7 polish: 16 → 18)
    constexpr int kLayerBase = 200;  // Layer insert 0..19 → 200..219
    constexpr int kBassBase  = 300;  // Bass insert 0..9 → 300..309
    constexpr int kAudioBase = 400;  // Audio insert 0..99 → 400..499 (ends flush at kDrumBase)
    constexpr int kDrumBase  = 500;  // Drum insert 0..31 → 500..531
    constexpr int kVoxBase   = 600;  // R1: Vox insert 0..9 → 600..609
    constexpr int kInstBase  = 700;  // R1: Inst insert 0..29 → 700..729
    constexpr int kRustyBase = 800;  // J-4: BaySickRustyDrums insert 0..12 → 800..812
    constexpr int kPluginBase = 900; // TS6: hosted VST3 instrument insert 0..19 → 900..919

    // Per-kind strip caps.  A strip cap IS its page cap -- one page of a kind
    // owns exactly one insert strip, so these are DERIVED from
    // BaySickConstants.h rather than restated.  Raising a page cap therefore
    // raises the strip cap with it; a one-sided raise would leave the extra
    // pages with an empty prefixFromChannelId, i.e. no strip, no EQ bank and no
    // routing entry, silently.  Range checks read THESE, never a bare literal.
    constexpr int kMaxLayerStrips = kMaxLayerPages;
    constexpr int kMaxBassStrips  = kMaxBassPages;
    constexpr int kMaxDrumStrips  = kMaxDrumPages;
    constexpr int kMaxAudioStrips = kMaxClipPages;
    constexpr int kMaxVoxStrips   = kMaxVoxPages;
    constexpr int kMaxInstStrips  = kMaxInstPages;
    constexpr int kMaxPluginStrips = kMaxPluginPages;
    // These two have no page counterpart -- Aux strips are standalone sends and
    // a Rusty kit's strips are its sound types, not tabs.
    constexpr int kMaxAuxStrips   = 18;  // 5F-4b B2; bumped 16 -> 18 in G-7 polish (2026-04-29)
    constexpr int kMaxRustyStrips = 13;  // J-4: 13 sound types per BaySickRustyDrums kit (no doubles)

    // QA-SOUNDNESS: drum pages split into two banks of this size -- pages
    // 0..15 are kit 1, pages 16..31 are kit 2.  This is the ONE place the
    // split is expressed; every drum-routing site resolves through
    // drumBusForPage below rather than repeating the arithmetic.
    constexpr int kDrumPagesPerBank = 16;

    // Which bank does a drum page belong to (0 = kit 1, 1 = kit 2)?
    inline int drumBankForPage (int pageIdx)
    {
        return pageIdx >= kDrumPagesPerBank ? 1 : 0;
    }

    // The drums bus a given drum page's insert feeds.
    inline int drumBusForPage (int pageIdx)
    {
        return drumBankForPage (pageIdx) == 1 ? kDrumsBus2 : kDrumsBus;
    }

    inline int layerInsert (int idx) { return kLayerBase + idx; }
    inline int bassInsert  (int idx) { return kBassBase  + idx; }
    inline int drumInsert  (int idx) { return kDrumBase  + idx; }
    inline int audioInsert (int idx) { return kAudioBase + idx; }
    inline int auxStrip    (int idx) { return kAuxBase   + idx; }
    inline int voxInsert   (int idx) { return kVoxBase   + idx; }
    inline int instInsert  (int idx) { return kInstBase  + idx; }
    inline int rustyInsert (int idx) { return kRustyBase + idx; }
    inline int pluginInsert (int idx) { return kPluginBase + idx; }

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
            case kPluginsBus: return "mixer_pluginbus";
            case kLayersBus2: return "mixer_layersbus2";
            case kBassBus2:   return "mixer_bassbus2";
            case kClipsBus2:  return "mixer_clipsbus2";
            case kPluginsBus2: return "mixer_pluginbus2";
            case kDrumsBus2:  return "mixer_drumsbus2";
        }
        if (chId >= kLayerBase && chId < kLayerBase + kMaxLayerStrips) return "mixer_layer_" + juce::String(chId - kLayerBase);
        if (chId >= kBassBase  && chId < kBassBase  + kMaxBassStrips)  return "mixer_bass_"  + juce::String(chId - kBassBase);
        if (chId >= kDrumBase  && chId < kDrumBase  + kMaxDrumStrips)  return "mixer_drum_"  + juce::String(chId - kDrumBase);
        if (chId >= kAudioBase && chId < kAudioBase + kMaxAudioStrips) return "mixer_audio_" + juce::String(chId - kAudioBase);
        if (chId >= kAuxBase   && chId < kAuxBase   + kMaxAuxStrips)   return "mixer_aux_"   + juce::String(chId - kAuxBase);
        if (chId >= kVoxBase   && chId < kVoxBase   + kMaxVoxStrips)   return "mixer_vox_"   + juce::String(chId - kVoxBase);
        if (chId >= kInstBase  && chId < kInstBase  + kMaxInstStrips)  return "mixer_inst_"  + juce::String(chId - kInstBase);
        if (chId >= kRustyBase && chId < kRustyBase + kMaxRustyStrips) return "mixer_rusty_" + juce::String(chId - kRustyBase);
        if (chId >= kPluginBase && chId < kPluginBase + kMaxPluginStrips) return "mixer_plugin_" + juce::String(chId - kPluginBase);
        return {};
    }

    // Is this channel a bus? (for routing rule enforcement)
    inline bool isBus (int chId)
    {
        return chId == kLayersBus || chId == kBassBus || chId == kDrumsBus
            || chId == kFxBus     || chId == kClipsBus
            || chId == kVoxBus    || chId == kInstBus
            || chId == kVoxBus2   || chId == kInstBus2 || chId == kInstBus3
            || chId == kRustyDrumsBus || chId == kPluginsBus
            || chId == kLayersBus2 || chId == kBassBus2
            || chId == kClipsBus2  || chId == kPluginsBus2
            || chId == kDrumsBus2;
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

    // ── Main-out lines ────────────────────────────────────────────────────────
    // A strip feeds up to four main destinations.  Four matches the aux-send
    // slot count rather than inventing a second number.
    //
    // PERSISTENCE IS ADDITIVE: line 0 stays in `<prefix>_sendTo` with its
    // historical 0..999 range and natural-parent default, so a project saved
    // before this existed restores its single cable untouched.  Lines 1..3 are
    // separate params, -1 = inactive, and simply do not exist in such a file.
    //
    // A main-out-locked strip (Master / buses / Rusty inserts) still has
    // exactly one line: the UI never offers the extra lines, and the routing
    // graph reads whatever is there, which for those strips is nothing.
    inline constexpr int kMaxMainOutsPerStrip = 4;

    inline juce::String mainOutParamId (const juce::String& prefix, int line)
    {
        if (line <= 0) return prefix + "_sendTo";
        return prefix + "_mainOut" + juce::String (line) + "_to";
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
            case kPluginsBus: return "Plugins Bus";
            case kLayersBus2: return "Layers Bus 2";
            case kBassBus2:   return "Bass Bus 2";
            case kClipsBus2:  return "Clips Bus 2";
            case kPluginsBus2: return "Plugins Bus 2";
            case kDrumsBus2:  return "Drums Bus 2";
        }
        if (chId >= kLayerBase && chId < kLayerBase + kMaxLayerStrips) return "Layer " + juce::String(chId - kLayerBase + 1);
        if (chId >= kBassBase  && chId < kBassBase  + kMaxBassStrips)  return "Bass "  + juce::String(chId - kBassBase  + 1);
        if (chId >= kDrumBase  && chId < kDrumBase  + kMaxDrumStrips)  return "Drum "  + juce::String(chId - kDrumBase  + 1);
        if (chId >= kAudioBase && chId < kAudioBase + kMaxAudioStrips) return "Audio " + juce::String(chId - kAudioBase + 1);
        if (chId >= kAuxBase   && chId < kAuxBase   + kMaxAuxStrips)   return "Aux "   + juce::String(chId - kAuxBase   + 1);
        if (chId >= kVoxBase   && chId < kVoxBase   + kMaxVoxStrips)   return "Vox "   + juce::String(chId - kVoxBase   + 1);
        if (chId >= kInstBase  && chId < kInstBase  + kMaxInstStrips)  return "Inst "  + juce::String(chId - kInstBase  + 1);
        if (chId >= kRustyBase && chId < kRustyBase + kMaxRustyStrips) return "Rusty " + juce::String(chId - kRustyBase + 1);
        if (chId >= kPluginBase && chId < kPluginBase + kMaxPluginStrips) return "Plugin " + juce::String(chId - kPluginBase + 1);
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
            case kPluginsBus: return kMaster;
            case kLayersBus2: return kMaster;
            case kBassBus2:   return kMaster;
            case kClipsBus2:  return kMaster;
            case kPluginsBus2: return kMaster;
            case kDrumsBus2:  return kMaster;
        }
        if (channelId >= kLayerBase && channelId < kLayerBase + kMaxLayerStrips) return kLayersBus;
        if (channelId >= kBassBase  && channelId < kBassBase  + kMaxBassStrips)  return kBassBus;
        // QA-SOUNDNESS: bank 1 (pages 0..15) -> Drums Bus, bank 2 (16..31) ->
        // Drums Bus 2.  A pre-change project's saved _sendTo wins over this
        // default, so existing slot-0..15 drums are untouched either way.
        if (channelId >= kDrumBase  && channelId < kDrumBase  + kMaxDrumStrips)  return drumBusForPage (channelId - kDrumBase);
        if (channelId >= kAudioBase && channelId < kAudioBase + kMaxAudioStrips) return kClipsBus;
        if (channelId >= kAuxBase   && channelId < kAuxBase   + kMaxAuxStrips)  return kFxBus;
        if (channelId >= kVoxBase   && channelId < kVoxBase   + kMaxVoxStrips)  return kVoxBus;
        if (channelId >= kInstBase  && channelId < kInstBase  + kMaxInstStrips) return kInstBus;
        if (channelId >= kRustyBase && channelId < kRustyBase + kMaxRustyStrips) return kRustyDrumsBus;
        if (channelId >= kPluginBase && channelId < kPluginBase + kMaxPluginStrips) return kPluginsBus;
        return kMaster;
    }
}

// ── RoutingGraph (5F-4b B1a) ─────────────────────────────────────────────────
// Resolves each strip's main-out lines + sends[] from APVTS at block rate.
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
        // true = main-out cable.  A src may have up to kMaxMainOutsPerStrip of
        // these, each to a DIFFERENT dst (duplicates are refused); every one
        // carries a full-level copy, the level is not split between them.
        bool  isMainOut;
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
    static constexpr int kMaxMainOutsPerStrip = MixerChannelIds::kMaxMainOutsPerStrip;

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

    // Single fast-path predicate for the pre-fader send tap: true iff at least
    // one surviving send edge asked for it.  Recomputed AFTER computeTopo, so an
    // edge dropped as a cycle cannot leave this reading true and make BaySickGraph
    // fill a tap nobody pulls.
    bool hasPreFaderSends() const noexcept { return mHasPreFaderSend; }

private:
    std::vector<Edge>   mEdges;
    std::vector<ScEdge> mScEdges;
    std::vector<int>    mTopoOrder;
    bool                mHasPreFaderSend { false };

    // Composed APVTS ids per channel, built once per (chId, prefix) pair.
    // rebuildFromApvts runs on the AUDIO THREAD every block, and juce::String
    // has no small-string optimization, so composing these inline cost ~33
    // heap allocations per channel per block -- the dominant allocation source
    // on the block-rate routing path.
    //
    // Only the id TEXT is cached.  Every block still reads the live APVTS
    // atomics through these ids, so a cable drag or an automated send amount
    // lands on the very next block exactly as before.  An id is a pure
    // function of the strip's prefix, so an entry is rebuilt only when that
    // channel's prefix actually changes -- no external invalidation hook is
    // needed, and a lazily-created parameter simply keeps resolving through
    // the same id until it exists.  Entries for channels that go away are
    // retained (the map is keyed by chId, which is a bounded space), so this
    // is a fixed ceiling rather than a ratchet; a chId reused by a different
    // strip re-composes on the prefix mismatch.
    struct ChannelParamIds
    {
        juce::String prefix;
        std::array<juce::String, kMaxMainOutsPerStrip> mainOutTo  {};
        std::array<juce::String, kMaxSendsPerStrip>   sendTo      {};
        std::array<juce::String, kMaxSendsPerStrip>   sendAmount  {};
        std::array<juce::String, kMaxSendsPerStrip>   sendPrePost {};
        std::array<juce::String, kMaxScRecvsPerStrip> scFrom      {};
    };
    std::unordered_map<int, ChannelParamIds> mChannelParamIds;

    // Topo input list, reused so the block-rate path doesn't reallocate it.
    std::vector<int>    mTopoIds;

    bool computeTopo(const std::vector<int>& ids);
};

// ── BaySickGraph ─────────────────────────────────────────────────────────────────
// Phase 1A / 1I: Audio bus topology for BaySickDAW.
//
// Fixed bus topology:
//   Layers Bus ─┐
//   Bass Bus   ─┤→ Master → output          (all InstrChannelNode, CL-301)
//   Drums Bus  ─┘
//   FX Bus       (receive bus for aux strips + user-routed sends; driven by
//                 processBus(kFxBus) each block)
//
// Each bus node owns an EffectRack (6 slots) and holds a reference to the
// channel EQ8MsDSP managed by PluginProcessor.  Mixer gain/mute/solo comes
// from BusMix (written on the message thread, read on the audio thread).
// ─────────────────────────────────────────────────────────────────────────────
class BaySickGraph
{
public:
    // ── Spectrum feed (audio thread writes, UI timer reads) ───────────────────
    // Definition moved to DSP/SpectrumFeed.h (shared with EQ8MsDSP per-instance
    // feeds; see 5F-9 §12i). Alias preserves the existing BaySickGraph::SpectrumFeed
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
    BaySickGraph();
    ~BaySickGraph();

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    // prepare() must be called before buildFixedTopology() and again on any
    // sample-rate change.  Safe to call after the topology is built.
    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // Build the fixed bus topology.  Guards itself - no-op after first call so
    // safe to call every prepareToPlay().  All references must remain valid for
    // the lifetime of this BaySickGraph.
    // 12i: SpectrumFeed refs dropped - each EQ8MsDSP owns its own pre/post feeds
    // (populated at its own process() I/O boundary). UI polls eq->preFeed /
    // eq->postFeed directly via ParametricEQDisplay::bindMsDSP.
    // §P4.3 B7 (2026-04-22): all external page-EQ refs dropped.  Every bus now
    // owns its own preEq (pre-rack) and busEq (post-rack) directly.
    // Drums-bus content comes exclusively from per-drum-tab InsertNode outputs.
    void buildFixedTopology(juce::AudioProcessorValueTreeState& apvts);

    // 2026-05-06 (Batch 9b): unified bus DSP dispatcher used by
    // PassiveStripTask's Bus mode.  `buf` is treated as in/out - caller must
    // have it pre-filled with the bus's input signal (sum of upstream
    // contributions) before calling.  CL-301: every non-master bus runs the
    // ONE InstrChannelNode::processChainOnly (Master runs processMasterChain
    // via processMasterBus); processBus's remaining per-bus knowledge is
    // node/meter-atomic selection only.  The chain reads the cached
    // master_pan_law pointer itself.  Caller is responsible for routing the
    // processed output downstream - processBus does DSP only.
    void processBus(int busChId, juce::AudioBuffer<float>& buf, double bpm);

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
    // QA-ModelShell TS6 (BLU-447): dedicated bus for hosted VST3 instrument
    // strips.  Always allocated on the same reasoning as the Rusty bus -- audio
    // routing works whether or not any plugin tab exists, and the bus sums
    // silence cheaply until strips spawn at kPluginBase..kPluginBase+19.
    EffectRack* getPluginsBusRack();
    // QA-Layout T10 (L13): secondary group buses -- always allocated on the
    // same reasoning as kVoxBus2 (silent pre-process is cheap; the activation
    // flag only controls UI presence + route-picker filtering).
    EffectRack* getLayersBus2Rack();
    EffectRack* getBassBus2Rack();
    EffectRack* getClipsBus2Rack();
    EffectRack* getPluginsBus2Rack();
    // QA-SOUNDNESS: second drum kit's bus, same always-allocated shape.
    EffectRack* getDrumsBus2Rack();

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
    EQ8MsDSP* getPluginsBusEQ();           // QA-ModelShell TS6
    // QA-Layout T10: secondary group buses.
    EQ8MsDSP* getLayersBus2EQ();
    EQ8MsDSP* getBassBus2EQ();
    EQ8MsDSP* getClipsBus2EQ();
    EQ8MsDSP* getPluginsBus2EQ();
    EQ8MsDSP* getDrumsBus2EQ();            // QA-SOUNDNESS

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
    EQ8MsDSP* getPluginsBusPreEQ();        // QA-ModelShell TS6
    // QA-Layout T10: secondary group buses.
    EQ8MsDSP* getLayersBus2PreEQ();
    EQ8MsDSP* getBassBus2PreEQ();
    EQ8MsDSP* getClipsBus2PreEQ();
    EQ8MsDSP* getPluginsBus2PreEQ();
    EQ8MsDSP* getDrumsBus2PreEQ();         // QA-SOUNDNESS

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
    // Wired once by BaySickDAWProcessor; message thread only
    // (updateBusLatencies allocates in setDelay).
    std::function<int(int voxIdx)> onGetVoxStripChainLatency;

    // QA-Fe2 PDC full-graph pass (2026-07-16): Inst analog of the Vox hook.
    // Inst strips host an EngineChainProcessor (sfizz -> Pedals -> NAM/IR);
    // NAM/IR reports oversampling latency via setLatencySamples, which was
    // written but never read by any aggregator before this hook.
    std::function<int(int instIdx)> onGetInstStripEngineLatency;

    // Total algorithmic latency (strip-stage + bus-stage compensation target
    // + master chain).  Read by BaySickDAWProcessor::getLatencySamples() and,
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

    // ── Instrument channel registry ───────────────────────────────────────────
    // Holds per-audio-row channels ONLY - ids 400 + row, registered by
    // addAudioRowChannel below - each with its own rack + post-rack EQ.
    // Nothing registers into the 100-199 band: Layer / Bass / Drum racks live
    // in the InsertNode storage (see InsertKind below), not here.
    //
    // EffectsPage sets onInstrChannelListChanged to rebuild its dropdown dynamically
    // whenever a new page is opened/closed.
    std::function<void()>   onInstrChannelListChanged;   // fired on add

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

    // ── 5F-4a: Per-insert audio nodes (new architecture) ─────────────────────
    // Each insert gets its own rack + post-rack EQ + polarity/width/fader path.
    // Driven by APVTS reads in the audio thread (memoized per block).
    // Batch 1 declares the API; Batch 2 defines InsertNode and the process loop.
    enum class InsertKind { Layer, Bass, Drum, Audio, Aux, Vox, Inst, Rusty, Plugin };

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
    // doesn't exist). Lets non-BaySickGraph code resolve what effect is loaded
    // in a given insert slot without touching the forward-declared struct.
    EffectRack* getInsertRack   (InsertKind kind, int index);

    // QA-RustyMeter (2026-05-30): UI-thread RMS drain for the split meter's
    // scrolling top half.  exchange-resets the (kind,index) insert node's rms
    // atoms; returns {-inf,-inf} if absent.  No PluginProcessor mirror needed.
    std::pair<float, float> drainInsertNodeRms (InsertKind kind, int index) noexcept;

    // QA-RustyMeter part 2 (2026-05-30): UI-thread RMS drain for a BUS strip's
    // split meter.  exchange-resets the per-bus rms member atoms below (CAS-maxed
    // audio-side by publishRms in processBus); returns
    // {-inf,-inf} for kMaster (Full layout, no RMS) or an unknown id.  Direct
    // BaySickGraph read -- no PluginProcessor mirror, parallel to drainInsertNodeRms.
    std::pair<float, float> drainBusRms (int busChId) noexcept;

    // QA-RustyMeter Task 3 (2026-05-30): master-bus EBU R128 LUFS readout.
    // getMasterLufs(mode): 0=Momentary, 1=Short-Term, 2=Integrated (-120 floor).
    // resetMasterLufsIntegrated(): clear the gated Integrated accumulation on
    // transport play-from-top / loop (M/S keep running).  Owned by the master
    // node (InstrChannelNode::mLufs; only the master chain processes it).
    float getMasterLufs (int mode) const noexcept;
    void  resetMasterLufsIntegrated() noexcept;

    // CL-044 (QA-ModelShell TS7): master-out spectrum feed, tapped at the same
    // point as the LUFS meter (post fader/pan/width) so the analyzer shows what
    // actually leaves the app.
    //
    // The ACTIVE FLAG lives here rather than on the master node deliberately: the
    // graph outlives any node, and the node holds a POINTER to this one flag
    // (installed by buildFixedTopology and re-pointed by every setter).  Since
    // §3.1 the tap is effectively ALWAYS live -- version capture's always-on
    // analysis want ORs with the analyzer window's -- so closing that window
    // changes which client holds the tap up, not whether it runs.
    void setMasterSpectrumActive (bool on) noexcept;
    // TS7 §3.1: version capture's own want on the same tap.  Analysis is always
    // on, so it cannot depend on the analyzer window being open; the tap runs if
    // EITHER client wants it.
    void setMasterAnalysisActive (bool on) noexcept;
    // UI thread: copies one frame.  Returns false if audio was mid-write (frame
    // dropped, which is correct for a visualiser) or if the tap is inactive.
    bool pollMasterSpectrum (float* dest, int& outCount) noexcept;
    // Master-out TRUE PEAK (BLU-108), measured at the same point.  Returns -144
    // when the tap is inactive rather than a stale value.
    float getMasterTruePeakDb() const noexcept;
    // TS7 §3.1: running max since the last reset, for a capture take's true peak.
    // Sampling the per-block value from the UI timer would miss the block the
    // overshoot lands in.
    float getMasterTruePeakMaxDb() const noexcept;
    void  resetMasterTruePeakMax() noexcept;

    // Return the post-rack EQ owned by an InsertNode (or nullptr). Parallels
    // getInsertRack - same opaque-InsertNode reason.
    // TS7 §6.2 (freeze, pre-rack "Source Only" tap).  Captures an insert's signal
    // at the TOP of its chain -- before preEq / polarity / width / rack / eq /
    // fader -- which is what lets a frozen tab keep its rack live and editable.
    // NOT the arena slot getStripOutputForTap returns: that is POST-chain (stems).
    // One insert armed at a time; arming clears any previous arm.
    // ── TS7 per-block host transport ─────────────────────────────────────────
    // Published once per block by PluginProcessor before the dispatcher runs,
    // read by every node's rack push.  Exists so a hosted VST3 in a RACK SLOT
    // gets a real playhead: JUCE builds the whole VST3 ProcessContext from one
    // AudioPlayHead::getPosition(), so without this a tempo-synced delay or an
    // arpeggiator in a rack slot had no host to follow.
    //
    // STATIC, matching AudioClipStreamer::sOfflineRender and
    // RenderEngine::gMultiThreadedEngineEnabled: the nodes are nested structs
    // with no back-pointer to the graph, and threading one through would have
    // meant changing processInsert's signature at every task call site for a
    // value that is identical across the whole block.  One processor per app
    // (standalone only), so a single snapshot is the whole truth.
    //
    // Written on the audio thread before task submission; the pool's
    // release/acquire on submit is what publishes it to the workers.
    static void setBlockTransport (const DSPBase::HostTransport& tp) noexcept
    { sBlockTransport = tp; }
    // bpm comes from the CALLER because each node is already handed the block's
    // tempo; everything else rides the snapshot.
    static DSPBase::HostTransport blockTransport (double bpm) noexcept
    { auto tp = sBlockTransport; tp.bpm = bpm; return tp; }

    void armFreezeTap (InsertKind kind, int index);
    void disarmFreezeTap();
    // OFFLINE USE ONLY -- same contract as getStripOutputForTap.
    juce::AudioBuffer<float>* getFreezeTapBuffer() noexcept;
    // Advances only when the tap ACTUALLY copied this block.  The tapped node's
    // processBlock does not run every block, so without this the render cannot
    // distinguish a fresh capture from the previous block's leftovers.
    juce::uint32 getFreezeTapSeq() const noexcept;

    EQ8MsDSP*   getInsertEQ     (InsertKind kind, int index);
    EQ8MsDSP*   getInsertPreEQ  (InsertKind kind, int index);   // §P4.3

    // 2026-05-05 dirty-flag wiring: fired from every BaySickGraph-owned rack's
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
    // BaySickDAWProcessor::drainInsertPeakDbStereo() — UI consumers exchange-
    // reset the per-kind PluginProcessor mirrors (m<Kind>InsertPeakDb*).  The
    // BaySickGraph-side per-insert getters / drainers + the peakDbSnap layer are
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

    // ONE walk over every rack in the graph, shared by both sweeps below.  A
    // second hand-maintained copy of that rack list is precisely the two-lists-
    // drift defect a new bus rack would fall through.
    void forEachRack (const std::function<void (EffectRack&)>& fn);

    // Offline-render edges: propagate the non-realtime state into hosted VST3
    // plugins living in rack slots, which the processor's engine sweep cannot
    // reach (they are DSPBase entries inside an EffectRack, not rig engines).
    void setAllRackSlotsNonRealtime (bool offline);

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
    // processBlock, drains peak meters into the BaySickGraph-level mirror
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

    // CL-301: the applyXxxBusPolarityWidth wrapper family is gone -- polarity
    // + width run inside InstrChannelNode::processChainOnly for every bus.

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
    // The array itself is owned by BaySickGraph; do not free.
    using ScRecvArray = std::array<juce::AudioBuffer<float>*, kMaxScRecvSlots>;
    ScRecvArray getScRecvArray (int channelId);
    // Clear all SC receive buffers (call at the top of each block).
    void clearScRecvBuffers();
    // QA-Fe2 SC delay-match (docket 1b): pre-compensation key tap for an SC
    // SOURCE channel.  nullptr when the channel isn't an armed SC source or
    // has no stash (Master) -- caller falls back to the post-everything
    // output buffer.  Audio thread.
    const juce::AudioBuffer<float>* getScSourceTap (int channelId) const;

    // ── Pre-fader send tap ───────────────────────────────────────────────────
    // The signal a strip carries at the point BEFORE its fader stage -- i.e.
    // before fader gain, before the mute gate and before the solo gate, which
    // this codebase applies as one combined gain (InsertNode::processBlock /
    // InstrChannelNode::processChainOnly).  Pan sits after that gain in both
    // chains, so the tap is pre-pan for free.
    //
    // A send edge with prePost == true reads THIS instead of the source's
    // mOutputBuffer; prePost == false keeps reading mOutputBuffer, and sidechain
    // edges keep reading their own getScSourceTap stash.  nullptr means the
    // channel is not an armed pre-fader source (or has no node), and the caller
    // falls back to mOutputBuffer.
    //
    // Audio thread.  Filled by the source strip's own chain pass, which the
    // dispatcher already orders before every consumer via the send edge's
    // mPredecessors / mChildren / mInitialDeps entry -- the same acquire on the
    // dependency counter that publishes mOutputBuffer publishes this.
    const juce::AudioBuffer<float>* getPreFaderTap (int channelId) const;
    // Writable handle on the same buffer, for the handful of strip tasks that
    // modify their output AFTER the chain pass (listen gate, idle-suspend fade)
    // and must apply the identical change to the tap.  nullptr when unarmed.
    juce::AudioBuffer<float>*       getPreFaderTapBuffer (int channelId);

    // Apply the per-(consumer, slot) key-alignment delay in place on the SC
    // receive buffer (values solved on the message thread by
    // updateBusLatencies).  Audio thread, allocation-free.
    void applyScRecvDelay (int channelId, int slotIdx, int numSamples);
    // Push the strip's SC array to its preEq + rack + postEq DSP modules.
    // Address-only push -- the actual buffer contents are filled by upstream
    // SC fanout (pullSidechainPredecessorsToGraph under MT), AFTER topo-sorted
    // source strips process.
    // Called by BaySickGraph internally for inserts + buses + master, and by
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
    // project load via BaySickDAWProcessor::clearAllAuxInserts from the three
    // load-entry points (deserializeProject / setStateInformation / doFileNew /
    // loadTemplate) BEFORE restoreAuxStripsFromState rebuilds from the loaded
    // project -- without this, auxes from the prior project's session persist
    // and the load layers the new project's auxes on top of them ("open 16
    // auxes, load another project, all 16 still there").  Safe under the load
    // shield; audio thread bails before iterating tasks.
    void clearAuxInserts();

    // ── Level meters (written by audio thread, read by UI timer) ─────────────
    // 2026-04-30: stereo L/R for the split DBFSMeter.
    std::atomic<float> layersPeakDbL { -60.f };
    std::atomic<float> layersPeakDbR { -60.f };
    std::atomic<float> bassPeakDbL   { -60.f };
    std::atomic<float> bassPeakDbR   { -60.f };
    std::atomic<float> drumsPeakDbL  { -60.f };
    std::atomic<float> drumsPeakDbR  { -60.f };
    std::atomic<float> masterPeakDbL { -60.f };
    std::atomic<float> masterPeakDbR { -60.f };
    std::atomic<float> fxBusPeakDbL  { -60.f };
    std::atomic<float> fxBusPeakDbR  { -60.f };
    std::atomic<float> audioClipsPeakDbL { -60.f };
    std::atomic<float> audioClipsPeakDbR { -60.f };
    std::atomic<float> voxBusPeakDbL     { -60.f };
    std::atomic<float> voxBusPeakDbR     { -60.f };
    std::atomic<float> voxBus2PeakDbL    { -60.f };
    std::atomic<float> voxBus2PeakDbR    { -60.f };
    std::atomic<float> instBusPeakDbL    { -60.f };
    std::atomic<float> instBusPeakDbR    { -60.f };
    std::atomic<float> instBus2PeakDbL   { -60.f };
    std::atomic<float> instBus2PeakDbR   { -60.f };
    std::atomic<float> instBus3PeakDbL   { -60.f };
    std::atomic<float> instBus3PeakDbR   { -60.f };
    std::atomic<float> rustyDrumsBusPeakDbL { -60.f };
    std::atomic<float> rustyDrumsBusPeakDbR { -60.f };
    std::atomic<float> pluginsBusPeakDbL    { -60.f };
    std::atomic<float> pluginsBusPeakDbR    { -60.f };
    // QA-Layout T10: secondary group buses.
    std::atomic<float> layersBus2PeakDbL    { -60.f };
    std::atomic<float> layersBus2PeakDbR    { -60.f };
    std::atomic<float> bassBus2PeakDbL      { -60.f };
    std::atomic<float> bassBus2PeakDbR      { -60.f };
    std::atomic<float> clipsBus2PeakDbL     { -60.f };
    std::atomic<float> clipsBus2PeakDbR     { -60.f };
    std::atomic<float> pluginsBus2PeakDbL   { -60.f };
    std::atomic<float> pluginsBus2PeakDbR   { -60.f };
    // QA-SOUNDNESS: second drum kit's bus.
    std::atomic<float> drumsBus2PeakDbL     { -60.f };
    std::atomic<float> drumsBus2PeakDbR     { -60.f };

    // QA-RustyMeter part 2 (2026-05-30): per-bus windowed-RMS atoms for the
    // split meter's scrolling top half.  Every non-master bus x L/R (Master keeps
    // a full peak bar, no RMS).  CAS-maxed audio-side by publishRms in processBus
    // (never reset there); the UI exchange-resets via
    // drainBusRms.  No mono sibling + no PluginProcessor mirror -- the UI reads
    // these directly off BaySickGraph, parallel to the InsertNode rms atoms.  The
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
    std::atomic<float> pluginsBusRmsDbL    { -60.f }, pluginsBusRmsDbR    { -60.f };  // TS6
    // QA-Layout T10: secondary group buses.
    std::atomic<float> layersBus2RmsDbL    { -60.f }, layersBus2RmsDbR    { -60.f };
    std::atomic<float> bassBus2RmsDbL      { -60.f }, bassBus2RmsDbR      { -60.f };
    std::atomic<float> clipsBus2RmsDbL     { -60.f }, clipsBus2RmsDbR     { -60.f };
    std::atomic<float> pluginsBus2RmsDbL   { -60.f }, pluginsBus2RmsDbR   { -60.f };
    // QA-SOUNDNESS: second drum kit's bus.
    std::atomic<float> drumsBus2RmsDbL     { -60.f }, drumsBus2RmsDbR     { -60.f };

    // QA-AudioMeters (2026-05-24): per-kind insert peak atomics, parallel to the
    // per-bus atomics above.  InsertNode::process publishes via publishPeakReading;
    // processInsert exchange-stores node->peakDb*/L/R into the per-kind array slot;
    // drainMeterAtomicsForUI drains them into PluginProcessor mirrors that the UI
    // polls.  All 8 InsertKinds adopt this unified G1 pattern (ends the bus-vs-
    // insert architectural split QA-Eg's bus migration left exposed).
    static constexpr int kMaxAudioInserts = 100; // matches BaySickDAWProcessor::kMaxAudioRows + MixerState::kMaxAudioRows (static_assert in .cpp); T11: 50 -> 100

    // QA-AudioMeters fix-up (2026-05-24): mono <kind>InsertPeakDb members
    // deleted as dead writes (no UI consumer ever read them; the UI reads L/R
    // only via BaySickDAWProcessor::drainInsertPeakDbStereo).
    std::array<std::atomic<float>, kMaxLayerPages>                    layerInsertPeakDbL  {}, layerInsertPeakDbR  {};
    std::array<std::atomic<float>, kMaxBassPages>                     bassInsertPeakDbL   {}, bassInsertPeakDbR   {};
    std::array<std::atomic<float>, kMaxDrumPages>                     drumInsertPeakDbL   {}, drumInsertPeakDbR   {};
    std::array<std::atomic<float>, kMaxAudioInserts>                  audioInsertPeakDbL  {}, audioInsertPeakDbR  {};
    std::array<std::atomic<float>, MixerChannelIds::kMaxAuxStrips>    auxInsertPeakDbL    {}, auxInsertPeakDbR    {};
    std::array<std::atomic<float>, MixerChannelIds::kMaxVoxStrips>    voxInsertPeakDbL    {}, voxInsertPeakDbR    {};
    std::array<std::atomic<float>, MixerChannelIds::kMaxInstStrips>   instInsertPeakDbL   {}, instInsertPeakDbR   {};
    std::array<std::atomic<float>, MixerChannelIds::kMaxRustyStrips>  rustyInsertPeakDbL  {}, rustyInsertPeakDbR  {};
    std::array<std::atomic<float>, MixerChannelIds::kMaxPluginStrips> pluginInsertPeakDbL {}, pluginInsertPeakDbR {};

    // TS7 per-block transport snapshot -- see setBlockTransport above.
    static DSPBase::HostTransport sBlockTransport;

private:
    // ── Forward-declared nested bus node type (defined in BaySickGraph.cpp) ──────
    // unique_ptr with incomplete type - destructor defined in BaySickGraph.cpp.
    // CL-301 (QA-ModelShell TS1, 2026-07-27): the five hand-written bus structs
    // (Layers/Bass/Drums/Master/Effects) are folded into this ONE type -- every
    // bus shares the same implementation; the master chain is a method,
    // not a type.
    struct InstrChannelNode;   // the one bus/channel node type (rack + EQs + chain)

    std::unique_ptr<InstrChannelNode> mLayersNode;
    std::unique_ptr<InstrChannelNode> mBassNode;
    std::unique_ptr<InstrChannelNode> mDrumsNode;
    std::unique_ptr<InstrChannelNode> mMasterNode;
    // TS7 §6.2: the currently armed freeze-tap insert, or null.  Raw because the
    // node is owned elsewhere and this is cleared by disarmFreezeTap; the render
    // that arms it is the only thing that reads it, and it runs with the device
    // suspended.
    InsertNode*                       mFreezeTapNode { nullptr };
    // CL-044: owned HERE rather than on the node.  NOTE the original rationale
    // ("a topology rebuild would reset it") does not hold -- buildFixedTopology
    // early-returns on mTopologyBuilt, so mMasterNode is built exactly once.  It
    // stays here anyway because the flag is now the OR of two clients (below) and
    // that arbitration is graph-level state, not node state.
    std::atomic<bool>                 mMasterSpecActive { false };
    // TS7 §3.1: the two independent wants behind mMasterSpecActive (analyzer
    // window / version capture).  updateMasterTapFlag folds them together --
    // neither client may write the effective flag, or closing the window would
    // silently stop capture's analysis.
    std::atomic<bool>                 mMasterSpecWanted     { false };
    std::atomic<bool>                 mMasterAnalysisWanted { false };
    void updateMasterTapFlag() noexcept;
    std::unique_ptr<InstrChannelNode> mEffectsBusNode;
    std::unique_ptr<InstrChannelNode> mAudioClipsBusNode;  // rack+EQ for all audio clips (ID 6)

    // QA-Ea Part A (2026-05-21): cached bus _solo atomic pointers for the
    // anyBusSoloed() helper.  Bound in rebindBusApvts(); the array length must
    // stay equal to kBusSoloPrefixes[] in BaySickGraph.cpp, which is the list this
    // mirrors (Master is excluded -- no _solo param, no sibling to solo against).
    // CPU-safeguarding standing rule: avoid string-keyed getRawParameterValue
    // lookups per audio block; cache the raw atomic ptrs once + reuse.
    static constexpr int kNumSoloableBuses = 17;   // QA-SOUNDNESS: +Drums Bus 2
    std::array<std::atomic<float>*, kNumSoloableBuses> mBusSoloPtr {};

    // Instrument channel nodes: keyed by 400 + arrangement row, the id
    // addAudioRowChannel assigns.  Insertion order preserved via
    // mInstrChannelOrder for dropdown display.
    std::map<int, std::unique_ptr<InstrChannelNode>> mInstrChannelNodes;
    std::vector<int>                                 mInstrChannelOrder;

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
    // `computeChannelId(kind, index)` helper in BaySickGraph.cpp; cached on
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
    // QA-ModelShell TS6 (BLU-447): hosted VST3 instrument bus, same
    // always-allocated shape as the Rusty bus above.
    std::unique_ptr<InstrChannelNode> mPluginsBusNode;
    // QA-Layout T10 (L13): secondary group buses, kVoxBus2 shape.
    std::unique_ptr<InstrChannelNode> mLayersBus2Node;
    std::unique_ptr<InstrChannelNode> mBassBus2Node;
    std::unique_ptr<InstrChannelNode> mClipsBus2Node;
    std::unique_ptr<InstrChannelNode> mPluginsBus2Node;
    // QA-SOUNDNESS (2026-08-07): bus for the second drum kit (pages 16..31),
    // same always-allocated shape.
    std::unique_ptr<InstrChannelNode> mDrumsBus2Node;
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

        // Zero the history without touching the delay length or the allocation.
        // Audio-thread safe (no allocation: the vector is already at size), and
        // used when a line resumes after a stretch of not being processed -- its
        // ring would otherwise emit up to mDelay samples of whatever it last saw.
        void clearRing() noexcept
        {
            std::fill(mBuf.begin(), mBuf.end(), 0.f);
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

    // The bus node behind a fixed-bus channel id, or nullptr for anything else
    // (insert strips live in mInsertsByChannel; kMaster is deliberately NOT
    // here -- getScSourceTap's contract is that master has no stash because it
    // has no comp delay, and master is the terminal node so it is never a live
    // send source either).
    InstrChannelNode* busNodeForChannel (int chId) const noexcept;

    // Audio thread (block rate, from rebuildRoutingFromApvts): arm the pre-fader
    // tap on exactly those nodes that are the source of at least one pre-fader
    // send this block, disarm every other node, and clear each armed tap so a
    // strip that does not run this block (idle-suspended, pruned by a freeze
    // render, a Clips row between clips) feeds silence into its send instead of
    // repeating the previous block.  Allocation-free.
    void armPreFaderTaps();

    // Single fast-path check for the whole feature (the mAnyXActive pattern):
    // with no pre-fader send anywhere, armPreFaderTaps returns on one relaxed
    // load and no node ever copies anything.  Holds LAST block's answer so the
    // sweep that disarms the final tap still runs once.
    std::atomic<bool> mAnyPreFaderSend { false };
    // Scratch for that sweep: chId -> is a pre-fader send source this block.
    // Audio thread only; fixed size so it never allocates.
    std::array<bool, (size_t) kMaxStripChannels> mPreFaderSrcFlags {};

    // 5F-4b B1b: routing state
    RoutingGraph                                       mRoutingGraph;

    // C.4 Phase 1: per-channel SC receive buffer set (4 stereo bufs per strip).
    // Each ScSet is allocated lazily on first ensureScRecvBuffers() / getScRecvBuffer.
    struct ScSet { std::array<juce::AudioBuffer<float>, kMaxScRecvSlots> bufs; };
    std::unordered_map<int, ScSet>                     mScRecv;

    // Channel set handed to RoutingGraph::rebuildFromApvts every block.  The
    // leading mFixedBusChannelCount entries are the always-registered buses;
    // they are built ONCE by buildFixedBusChannels() from the constructor and
    // never rebuilt, because juce::String has no small-string optimization and
    // composing them from literals per block cost one StringHolder malloc per
    // bus, plus the matching frees, on the audio thread.  The per-block rebuild
    // only truncates back to that count and re-appends the live-insert tail.
    // Capacity covers the
    // whole chId space so the append can never realloc on the audio thread
    // either (same reasoning as the mLiveInsertChannels reserve).
    std::vector<std::pair<int, juce::String>>          mActiveChannels;
    std::size_t                                        mFixedBusChannelCount { 0 };
    void buildFixedBusChannels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BaySickGraph)
};
