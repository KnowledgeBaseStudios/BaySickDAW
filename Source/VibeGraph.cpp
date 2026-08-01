#include "VibeGraph.h"
#include "BassSynth.h"
#include "DSP/LufsMeterDSP.h"   // QA-RustyMeter Task 3: master-bus EBU R128 LUFS
#include "DSP/TruePeakMeter.h"  // QA-ModelShell TS7 CL-044: master-out true peak
// DrumSynth.h removed from graph (2026-04-25) - no longer references the class.
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <optional>

// ═══════════════════════════════════════════════════════════════════════════════
//  QA-InsertMaps (2026-05-24): InsertKind + index -> MixerChannelIds chId helper.
//  Hoisted to the top of the translation unit so every call site sees it,
//  including the XML restore path (restoreInsert at the project-load
//  loadRackStates helper, ~line 2150) which lives BEFORE the per-insert
//  node registry section where the helper was originally added in place of
//  selectInsertMap.  Used by ensureInsertNode / removeInsertNode /
//  getInsertNode / restoreInsert / getInsertChokeGroup for the flat-array
//  lookup, and cached on InsertNode::chId at construction so the audio-
//  thread processInsert path reads it directly (skipping the per-block
//  kind->chId switch).
// ═══════════════════════════════════════════════════════════════════════════════

namespace {
    static inline int computeChannelId(VibeGraph::InsertKind kind, int index) noexcept
    {
        using namespace MixerChannelIds;
        switch (kind)
        {
            case VibeGraph::InsertKind::Layer: return layerInsert(index);
            case VibeGraph::InsertKind::Bass:  return bassInsert (index);
            case VibeGraph::InsertKind::Drum:  return drumInsert (index);
            case VibeGraph::InsertKind::Audio: return audioInsert(index);
            case VibeGraph::InsertKind::Aux:   return auxStrip   (index);
            case VibeGraph::InsertKind::Vox:   return voxInsert  (index);
            case VibeGraph::InsertKind::Inst:  return instInsert (index);
            case VibeGraph::InsertKind::Rusty: return rustyInsert(index);
            case VibeGraph::InsertKind::Plugin: return pluginInsert(index);
        }
        return -1;  // unreachable; defensive
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Bus node struct definitions
//  These are nested inside VibeGraph (forward-declared in VibeGraph.h).
//  Each node:
//    • holds non-owning refs to the shared engine and EQ DSP
//    • owns its EffectRack (6 hot-swap slots)
//    • writes a post-processing peak dB to an atomic for the UI meter
// ═══════════════════════════════════════════════════════════════════════════════

// QA-InsertMaps Task 5 close (2026-05-25): deleted dead static helpers
// `calcBusGain` + `bufferPeakDb` (C4505 cleanup).  Both orphaned by earlier
// refactors -- `calcBusGain` by QA-Ea Part A (unified bus-solo via
// `anyBusSoloed()` cached-atomic helper replaced the muted/soloed/anySolo
// per-block-arg shape) + `bufferPeakDb` (mono) by QA-Eg / QA-AudioMeters
// G1 publish chain (every publish site now uses `bufferPeakDbStereo` below
// + `publishPeakReading` with the L/R-aware peak ring).

// 2026-04-30: per-channel peak helper for stereo L/R split meters.  Returns
// {peakDbL, peakDbR}.  Mono buffers fan to L=R; >2-channel buffers only
// expose channels 0 and 1 (matches the strip's L/R display).
static std::pair<float, float> bufferPeakDbStereo(const juce::AudioBuffer<float>& buf) noexcept
{
    const int n  = buf.getNumSamples();
    const int nc = buf.getNumChannels();
    if (nc <= 0 || n <= 0) return { -60.f, -60.f };

    const float pL = buf.getMagnitude (0, 0, n);
    const float pR = (nc >= 2) ? buf.getMagnitude (1, 0, n) : pL;
    return { juce::Decibels::gainToDecibels (pL, -60.f),
             juce::Decibels::gainToDecibels (pR, -60.f) };
}

// ── Meter-latency-compensation globals (2026-05-02) ──────────────────────────
// gEnabled is the user's hamburger-menu toggle.  gCompensationBlocks is the
// number of audio blocks to delay the published peak by (so the visual meter
// matches the sound the user actually hears, given the audio device's output
// latency).  Recomputed on toggle change OR audio-device prepare.
namespace MeterLatencyComp
{
    std::atomic<int>  gCompensationBlocks { 0 };
    std::atomic<bool> gEnabled            { false };

    void recomputeFromDevice (double sampleRate, int blockSize, int latencySamples)
    {
        juce::ignoreUnused (sampleRate);
        if (! gEnabled.load (std::memory_order_relaxed)
            || blockSize <= 0 || latencySamples <= 0)
        {
            gCompensationBlocks.store (0, std::memory_order_relaxed);
            return;
        }
        // Round to nearest block.  Clamp so the compensated read stays inside
        // the per-node ring buffer (kRingMask = 15 blocks max).
        const int blocks = juce::jlimit (0, kRingMask,
                                          (latencySamples + blockSize / 2) / blockSize);
        gCompensationBlocks.store (blocks, std::memory_order_relaxed);
    }
}

// ── Lock-free max-peak publisher (2026-05-02) ────────────────────────────────
// Replaces the legacy "max + per-block decay" pattern in every bus / insert /
// master node.  The audio thread:
//   1. computes the current block's L/R peak in dB,
//   2. writes it into a small per-node ring buffer (16 entries),
//   3. reads back from the ring at offset = MeterLatencyComp::gCompensationBlocks
//      so the published value matches what the user is currently hearing,
//   4. CAS-loops the latency-compensated peak into the running-max atomics.
//
// The CAS loop is the lock-free counterpart to the UI's exchange-and-reset:
// the UI thread atomically swaps the running-max with -inf each frame to
// start a fresh "max within frame" window.  Audio's CAS retries safely if
// it races with that swap, so no peak is lost across the boundary.
//
// peakRingL/R are passed in as references so each node owns its own ring
// (peaks are inherently per-node).  Audio thread is the only writer of the
// ring; UI thread never reads it directly (only the atomics).
namespace
{
    inline void publishPeakReading (const juce::AudioBuffer<float>& buf,
                                     std::array<float, MeterLatencyComp::kRingSize>& ringL,
                                     std::array<float, MeterLatencyComp::kRingSize>& ringR,
                                     int& writeIdx,
                                     std::atomic<float>& peakDbL,
                                     std::atomic<float>& peakDbR,
                                     std::atomic<float>& peakDbMono) noexcept
    {
        const auto [thisL, thisR] = bufferPeakDbStereo (buf);
        ringL[(size_t) (writeIdx & MeterLatencyComp::kRingMask)] = thisL;
        ringR[(size_t) (writeIdx & MeterLatencyComp::kRingMask)] = thisR;
        const int compBlocks =
            juce::jlimit (0, MeterLatencyComp::kRingMask,
                           MeterLatencyComp::gCompensationBlocks.load (std::memory_order_relaxed));
        const int rIdx = (writeIdx - compBlocks) & MeterLatencyComp::kRingMask;
        const float dispL = ringL[(size_t) rIdx];
        const float dispR = ringR[(size_t) rIdx];
        ++writeIdx;

        // CAS-max: each audio block, raise the running max in the node's
        // atomic if this block's compensated peak is louder.  The atomic is
        // drained by the PluginProcessor mirror sync once per block:
        //   - buses: BusNode peakDb -> VibeGraph member atomic (exchange-store
        //     in processBus) -> PluginProcessor mirror (drainAndMerge in
        //     drainMeterAtomicsForUI) -> UI poll (exchange-and-reset).
        //   - inserts (QA-AudioMeters 2026-05-24): InsertNode peakDb -> VibeGraph
        //     per-kind public-member array (exchange-store in processInsert) ->
        //     PluginProcessor m<Kind>InsertPeakDb* mirror (drainAndMerge in
        //     drainMeterAtomicsForUI) -> UI poll via VibeSynthProcessor::
        //     drainInsertPeakDbStereo (exchange-and-reset).
        auto casMax = [] (std::atomic<float>& a, float v) noexcept
        {
            float cur = a.load (std::memory_order_relaxed);
            while (cur < v
                   && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed))
            {}
        };
        casMax (peakDbL,    dispL);
        casMax (peakDbR,    dispR);
        casMax (peakDbMono, juce::jmax (dispL, dispR));
    }

    // QA-RustyMeter (2026-05-30): per-block RMS publish for the split meter's
    // scrolling top half.  Computes sqrt(mean-square) in dB and CAS-maxes it
    // into the node's rms atoms.  The audio thread NEVER resets these (unlike
    // peak, which processInsert exchange-stores) -- the UI exchange-resets them
    // via VibeGraph::drainInsertNodeRms, so the value is "max RMS since the last
    // UI read".  That makes it multi-call safe (Audio/Vox/Inst call processInsert
    // several times per block) exactly like publishPeakReading.  The ~50 ms
    // window smoothing lives UI-side in DBFSMeter::onVBlank.
    inline void publishRms (const juce::AudioBuffer<float>& buf,
                            std::atomic<float>& rmsDbL,
                            std::atomic<float>& rmsDbR) noexcept
    {
        const int n  = buf.getNumSamples();
        const int nc = buf.getNumChannels();
        if (n <= 0 || nc <= 0) return;
        const float* L = buf.getReadPointer (0);
        const float* R = (nc >= 2) ? buf.getReadPointer (1) : L;
        double sL = 0.0, sR = 0.0;
        for (int s = 0; s < n; ++s) { sL += (double) L[s] * L[s]; sR += (double) R[s] * R[s]; }
        const float dbL = juce::Decibels::gainToDecibels ((float) std::sqrt (sL / (double) n), -60.f);
        const float dbR = juce::Decibels::gainToDecibels ((float) std::sqrt (sR / (double) n), -60.f);
        auto casMax = [] (std::atomic<float>& a, float v) noexcept
        {
            float cur = a.load (std::memory_order_relaxed);
            while (cur < v && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
        };
        casMax (rmsDbL, dbL);
        casMax (rmsDbR, dbR);
    }
}

// ── Pan law helper (2026-04-29) ──────────────────────────────────────────────
// FL Studio parity.  Maps pan ∈ [-1, +1] + project-level law selector
// (master_pan_law: 0=Circular / 1=Triangular / 2=Square) → (gainL, gainR).
//   Circular   - constant power, -3 dB at center (cos/sin curve, FL default)
//   Triangular - linear, -6 dB at center
//   Square     - 0 dB at center (only attenuates the opposite side)
// Applied per-strip after the fader-gain stage in every Insert/Bus/Master node.
static void applyPanLaw (float pan, int law, float& gainL, float& gainR) noexcept
{
    pan = juce::jlimit (-1.f, 1.f, pan);
    const float normPan = (pan + 1.f) * 0.5f;   // 0 = full L, 0.5 = center, 1 = full R
    switch (law)
    {
        case 1: // Triangular - linear, -6 dB at center
            gainL = 1.f - normPan;
            gainR = normPan;
            break;
        case 2: // Square - 0 dB at center, only attenuates the opposite side
            gainL = (pan <= 0.f) ? 1.f : (1.f - pan);
            gainR = (pan >= 0.f) ? 1.f : (1.f + pan);
            break;
        default: // 0 = Circular - constant power, -3 dB at center (FL default)
        {
            const float a = normPan * juce::MathConstants<float>::halfPi;
            gainL = std::cos (a);
            gainR = std::sin (a);
            break;
        }
    }
}

// Apply pan in-place to a stereo buffer using the given pan + law.  No-op for
// near-center pan (avoids the cost of two applyGain calls when the user hasn't
// touched the pan knob).
static void applyStereoPan (juce::AudioBuffer<float>& buf, float pan, int law) noexcept
{
    if (buf.getNumChannels() < 2 || std::abs (pan) < 1.0e-4f) return;
    float gL = 1.f, gR = 1.f;
    applyPanLaw (pan, law, gL, gR);
    buf.applyGain (0, 0, buf.getNumSamples(), gL);
    buf.applyGain (1, 0, buf.getNumSamples(), gR);
}

// CL-301 (2026-07-27): LayersBusNode / BassBusNode / DrumsBusNode /
// MasterBusNode / EffectsBusNode deleted -- folded into the unified
// InstrChannelNode below (see its header comment for the divergence history).



// ── InsertNode (5F-4a Batch 2 full implementation) ───────────────────────────
// Per-insert audio path: polarity → M/S width → rack (bypassable) → EQ →
// fader × mute × solo → PDC compensation → peak meter.
//
// APVTS access pattern: on creation, rebindApvts() caches raw param pointers
// (atomic<float>*). Audio thread reads via plain relaxed load - wait-free.
// Pointers may be null if the corresponding param doesn't exist (Master/Bus
// kinds lack some params); load() helper falls back to a sensible default.
//
// Batch 2 does NOT call processBlock() from anywhere. Batch 3 wires it in.
struct VibeGraph::InsertNode
{
    // ── Identity ──────────────────────────────────────────────────────────────
    juce::String          name;
    juce::String          apvtsPrefix;
    VibeGraph::InsertKind kind  { VibeGraph::InsertKind::Layer };
    int                   index { 0 };
    // QA-InsertMaps (2026-05-24): cached MixerChannelIds chId; set by
    // ensureInsertNode via computeChannelId(kind, index).  Lets processInsert
    // skip the per-block kind->chId switch (the switch dies as a natural
    // consequence of the flat-array migration).
    int                   chId  { -1 };

    // ── Audio DSP ─────────────────────────────────────────────────────────────
    // §P4.3: pre-rack EQ runs at the very start of the chain, before polarity /
    // width / rack / post-rack EQ.  Fresh EQ8MsDSP using the standard machinery
    // - same registration + APVTS sync as the post-rack `eq` (just under the
    // `_preeq_` prefix so they don't collide).  Bypass-flat by default so
    // existing kits sound identical until the user touches it.
    EQ8MsDSP              preEq;      // §P4.3 pre-rack
    EffectRack            rack;
    EQ8MsDSP              eq;         // post-rack
    CompDelayLine         compDelay;  // per-insert PDC
    // QA-AudioMeters (2026-05-24): G1-pattern peak fields (parallel to L/B/D/
    // Master/FX/AudioClips/Vox/Inst/Rusty BusNodes).  InsertNode::processBlock
    // calls publishPeakReading which CAS-maxes into peakDb/L/R via the
    // latency-comp ring; VibeGraph::processInsert exchange-stores these into
    // its per-kind public-member arrays at end of every processInsert call;
    // drainMeterAtomicsForUI drains those into PluginProcessor mirrors that
    // the UI polls.  peakDbSnap layer + peakDecayDbPerBlock field removed --
    // the per-block decay used to be applied here; ballistic decay now lives
    // entirely on the UI side (DBFSMeter widget).
    std::atomic<float>    peakDb  { -60.f };
    std::atomic<float>    peakDbL { -60.f };
    std::atomic<float>    peakDbR { -60.f };
    std::array<float, MeterLatencyComp::kRingSize> peakRingL {}, peakRingR {};
    int                   peakRingIdx { 0 };

    // QA-RustyMeter (2026-05-30): per-block RMS accumulator for the split meter.
    // CAS-maxed by publishRms (audio thread); exchange-reset by the UI via
    // VibeGraph::drainInsertNodeRms.  Not touched by processInsert (unlike peak).
    std::atomic<float>    rmsDbL { -60.f };
    std::atomic<float>    rmsDbR { -60.f };

    // F5 (2026-04-24): per-insert fader-gain smoothing.  Starts at 1.0 after
    // prepare; processBlock applies applyGainRamp(mLastFaderGain -> newGain)
    // so fader moves + mute toggles don't zipper.
    float                 mLastFaderGain { 1.0f };

    // ── Cached APVTS raw value pointers (rebindApvts sets these) ──────────────
    std::atomic<float>* pLevel    { nullptr };
    std::atomic<float>* pPan      { nullptr };
    std::atomic<float>* pMute     { nullptr };
    std::atomic<float>* pSolo     { nullptr };
    std::atomic<float>* pPolarity { nullptr };
    std::atomic<float>* pWidth    { nullptr };
    std::atomic<float>* pBypass   { nullptr };
    std::atomic<float>* pArm      { nullptr };
    // D3: choke group (0 = none, 1..16 = group id).  Audio thread reads this
    // via load(); ChokeBus dispatch uses it to find peer inserts to silence.
    std::atomic<float>* pChokeGroup { nullptr };
    // Global "kill-all" FX bypass (master_fx_bypass) - OR-ed into pBypass each block.
    std::atomic<float>* pGlobalFxBypass { nullptr };
    // 2026-04-29: project-level pan law selector (master_pan_law) - read
    // every block alongside pPan so the user can pick FL-style pan curve.
    std::atomic<float>* pPanLaw { nullptr };

    // QA-Fe2 SC delay-match stash (see InstrChannelNode).
    juce::AudioBuffer<float> scTap;
    std::atomic<bool>        scTapArmed { false };

    InsertNode(VibeGraph::InsertKind k, int i, int channelId,
               juce::String displayName, juce::String prefix)
        : name(std::move(displayName))
        , apvtsPrefix(std::move(prefix))
        , kind(k), index(i), chId(channelId) {}

    void prepare(double sr, int blockSize)
    {
        preEq.prepare(sr, blockSize);   // §P4.3
        rack .prepare(sr, blockSize);
        eq   .prepare(sr, blockSize);
        scTap.setSize(2, juce::jmax(1, blockSize), false, true, false);
        // TS7 §6.2: sized here so the audio-thread tap is a pure copy.
        freezeTapBuf.setSize(2, juce::jmax(1, blockSize), false, true, false);
    }
    void reset()
    {
        preEq.reset();   // §P4.3
        rack .reset();
        eq   .reset();
    }

    // Message thread: called after ensureMixerStripParams has created the params.
    void rebindApvts(juce::AudioProcessorValueTreeState& apvts)
    {
        pLevel          = apvts.getRawParameterValue(apvtsPrefix + "_level");
        pPan            = apvts.getRawParameterValue(apvtsPrefix + "_pan");
        pMute           = apvts.getRawParameterValue(apvtsPrefix + "_mute");
        pSolo           = apvts.getRawParameterValue(apvtsPrefix + "_solo");
        pPolarity       = apvts.getRawParameterValue(apvtsPrefix + "_polarity");
        pWidth          = apvts.getRawParameterValue(apvtsPrefix + "_width");
        pBypass         = apvts.getRawParameterValue(apvtsPrefix + "_bypass");
        pArm            = apvts.getRawParameterValue(apvtsPrefix + "_arm");
        pChokeGroup     = apvts.getRawParameterValue(apvtsPrefix + "_chokeGroup");
        pGlobalFxBypass = apvts.getRawParameterValue("master_fx_bypass");
        pPanLaw         = apvts.getRawParameterValue("master_pan_law");
    }

    static float load(const std::atomic<float>* p, float fallback) noexcept
    {
        return p ? p->load(std::memory_order_relaxed) : fallback;
    }

    // 5F-4a Batch 6: exposed for anySolo computation on the audio thread.
    bool isSoloed() const noexcept { return load(pSolo, 0.f) > 0.5f; }

    // ── TS7 §6.2: pre-rack "Source Only" freeze tap ───────────────────────────
    // Armed by VibeGraph::armFreezeTap for ONE insert during a freeze render.
    // The copy is taken at the very TOP of processBlock -- before preEq, and
    // therefore before the whole chain (preEq -> polarity -> width -> rack -> eq
    // -> fader -> pan).  That is what makes a frozen tab keep its rack live and
    // editable, which is the entire point of Jeff's Source Only ruling.
    //
    // Deliberately NOT the arena slot getStripOutputForTap returns: that is the
    // POST-chain output (what stems are), so it could not serve this.
    std::atomic<bool>        freezeTapArmed { false };
    juce::AudioBuffer<float> freezeTapBuf;
    // Bumped on every actual copy.  The render reads the buffer BETWEEN blocks
    // and cannot otherwise tell a fresh capture from last block's leftovers --
    // and this node's processBlock does NOT run every block (a Clips row with a
    // gap between clips skips it, an idle-suspended engine skips it).  Without
    // this, those gaps were written as a REPEAT of the previous block instead of
    // silence: a stutter baked into the freeze file.
    std::atomic<juce::uint32> freezeTapSeq { 0 };

    // Audio thread: process buf in-place.
    // If preRenderedSrc is non-null and has matching sample count, copied into buf first.
    // anySolo: global solo-active flag (computed by caller once per block).
    void processBlock(juce::AudioBuffer<float>& buf, double bpm, bool anySolo,
                      const juce::AudioBuffer<float>* preRenderedSrc = nullptr)
    {
        const int n  = buf.getNumSamples();
        const int nc = buf.getNumChannels();

        if (preRenderedSrc != nullptr && preRenderedSrc->getNumSamples() == n)
        {
            const int srcCh = juce::jmin(nc, preRenderedSrc->getNumChannels());
            for (int c = 0; c < srcCh; ++c)
                buf.copyFrom(c, 0, *preRenderedSrc, c, 0, n);
        }

        // TS7 §6.2: the Source Only tap, taken BEFORE any chain stage runs.
        // Sized at prepare, so this is a copy with no allocation.
        if (freezeTapArmed.load (std::memory_order_relaxed)
            && freezeTapBuf.getNumSamples() >= n)
        {
            const int tc = juce::jmin (nc, freezeTapBuf.getNumChannels());
            for (int c = 0; c < tc; ++c)
                freezeTapBuf.copyFrom (c, 0, buf, c, 0, n);
            freezeTapSeq.fetch_add (1, std::memory_order_release);
        }

        // §P4.3 pre-rack EQ - first DSP stage, before polarity / width / rack.
        // (Identity short-circuit + spectrum feed live inside EQ8MsDSP::process.)
        if (nc >= 2) preEq.process(buf);

        // Polarity flip
        if (load(pPolarity, 0.f) > 0.5f)
            buf.applyGain(-1.f);

        // M/S stereo width - only meaningful on stereo and when width != 1.0
        const float width = load(pWidth, 1.f);
        if (nc >= 2 && std::abs(width - 1.f) > 1.0e-4f)
        {
            float* L = buf.getWritePointer(0);
            float* R = buf.getWritePointer(1);
            for (int s = 0; s < n; ++s)
            {
                const float m    = 0.5f * (L[s] + R[s]);
                const float side = 0.5f * (L[s] - R[s]) * width;
                L[s] = m + side;
                R[s] = m - side;
            }
        }

        // Rack (bypass synced from APVTS _bypass each block - canonical source).
        // OR-in the global master_fx_bypass kill-all flag.
        const bool stripBypass  = load(pBypass, 0.f) > 0.5f;
        const bool globalBypass = load(pGlobalFxBypass, 0.f) > 0.5f;
        const bool bypass       = stripBypass || globalBypass;
        if (rack.isRackBypassed() != bypass)
            rack.setRackBypassed(bypass);
        rack.setHostTransport (VibeGraph::blockTransport (bpm));
        rack.process(buf);

        // Post-rack EQ (identity short-circuit + spectrum feed live inside).
        if (nc >= 2) eq.process(buf);

        // Fader × mute × solo.  F5 (2026-04-24): gain-ramp from previous block's
        // final gain -> this block's target so fader moves / mute toggles
        // don't zipper.  When g is stable (common case), applyGainRamp is
        // equivalent to applyGain and the per-sample cost is marginal.
        const bool muted  = load(pMute, 0.f) > 0.5f;
        const bool soloed = load(pSolo, 0.f) > 0.5f;
        float g = 0.f;
        if (!muted && !(anySolo && !soloed))
        {
            const float db = load(pLevel, 0.f);
            g = juce::Decibels::decibelsToGain(db, -60.f);
        }
        if (g != 1.f || mLastFaderGain != 1.f)
        {
            for (int c = 0; c < nc; ++c)
                buf.applyGainRamp(c, 0, n, mLastFaderGain, g);
        }
        mLastFaderGain = g;

        // 2026-04-29: per-strip pan applied AFTER fader using project-level
        // pan law (master_pan_law: 0=Circular / 1=Triangular / 2=Square).
        // applyStereoPan no-ops when pan is at center, so this costs ~nothing
        // for the common case.
        applyStereoPan (buf, load(pPan, 0.f), (int) load(pPanLaw, 0.f));

        // QA-Fe2 SC delay-match: stash the pre-compensation output for SC
        // consumers before the alignment delay lands on this strip.
        if (scTapArmed.load(std::memory_order_relaxed))
            for (int c = 0, tc = juce::jmin(nc, scTap.getNumChannels()); c < tc; ++c)
                scTap.copyFrom(c, 0, buf, c, 0, juce::jmin(n, scTap.getNumSamples()));

        // Per-insert PDC alignment
        compDelay.process(buf);

        // QA-AudioMeters (2026-05-24): unified G1 publish via publishPeakReading
        // (CAS-max + latency-comp ring) -- same helper every BusNode uses.
        // Per-block decay used to live here as an open-coded load-decay-max-store;
        // ballistic decay now happens UI-side in DBFSMeter.  processInsert
        // exchange-stores peakDb/L/R into VibeGraph's per-kind public-member
        // arrays at end of every processInsert call; drainMeterAtomicsForUI
        // drains those into PluginProcessor mirrors that UI polls.
        publishPeakReading (buf, peakRingL, peakRingR, peakRingIdx,
                            peakDbL, peakDbR, peakDb);
        publishRms (buf, rmsDbL, rmsDbR);   // QA-RustyMeter split-meter RMS feed
    }
};

// ── InstrChannelNode ──────────────────────────────────────────────────────────
// Generic rack + post-rack EQ container for non-bus mixer channels.
// One instance per entry in the dynamic instrument channel registry.
// Audio routing is wired in Phase 2/3 - containers are live from Phase 1.
// CL-301 (QA-ModelShell TS1, 2026-07-27): the ONE bus/channel node type.  The
// five hand-written structs (LayersBusNode / BassBusNode / DrumsBusNode /
// MasterBusNode / EffectsBusNode) are folded in here so all 11 mixer buses
// share a single implementation -- the split caused three on-record divergence
// incidents (scattered solo formulas, hand-ported meter pattern, PDC lines
// missing on the generic 7) plus a fourth found at fold time (the generic 7
// never received setHostBPM, so tempo-synced rack effects on those buses ran
// at default BPM).  Special-case content survives as members/methods, not
// types: the master chain (terminal -- masterGain x fader, no polarity, no
// comp delay, LUFS meter) is processMasterChain; every other bus runs
// processChainOnly.  The L/B/D synth-render fallback paths died with the fold
// (dead since QA-Ea Part A; zero callers).
struct VibeGraph::InstrChannelNode
{
    juce::String name;
    EQ8MsDSP     preEq;   // §P4.3 pre-rack
    EffectRack   rack;
    EQ8MsDSP     eq;      // post-rack bus EQ - shown on Effects Page
    // QA-Fe2 PDC: bus-stage alignment (post-pan, pre-meter).  The master
    // chain deliberately never processes it (terminal node -- nothing
    // downstream to align against).
    CompDelayLine compDelay;

    // QA-RustyMeter Task 3: EBU R128 loudness (Momentary/Short-Term/
    // Integrated).  Processed by the MASTER chain only, post fader/pan/width;
    // sibling buses carry the member unprocessed (prepare-time cost only).
    LufsMeterDSP mLufs;

    // CL-044 (QA-ModelShell TS7): master-out spectrum tap.  Written by the MASTER
    // chain only; sibling buses never touch either member.  `specFeedActive` is
    // owned by VibeGraph (not the node) so its lifetime is independent of the
    // node's.  Since §3.1 the tap is effectively ALWAYS live -- version
    // capture's always-on analysis want ORs with the analyzer window's -- so
    // the flag mostly records which clients hold it up.  Null on every
    // non-master node, which is also the gate.
    SpectrumFeed        specFeed;
    std::atomic<bool>*  specFeedActive { nullptr };
    // Pre-allocated: the mono sum cannot allocate on the audio thread.
    std::vector<float>  specMonoScratch;

    // CL-044 / BLU-108: master-out TRUE PEAK, measured at the same point as the
    // loudness meter so the analyzer's three numbers all describe one signal.
    // Gated by the same flag as the spectrum tap -- a closed analyzer pays for
    // neither.  resetPeak (not reset) per block keeps filter history across the
    // boundary, so there is no seam in the measurement.
    TruePeakMeter       specTp;
    std::atomic<float>  masterTpDb { -144.0f };
    // TS7 §3.1: the running MAX across a capture take, beside the per-block
    // value the readout shows.  A take's true peak cannot be sampled by the UI
    // timer -- at ~43 blocks/sec against a 30 Hz poll, the one block carrying
    // the overshoot is exactly what gets missed.  Audio keeps the max; the UI
    // only reads and clears it.
    std::atomic<float>  masterTpMaxDb { -144.0f };

    // QA-Eg: G1-pattern peak fields.  publishPeakReading writes peakDb/L/R +
    // ring; processBus exchange-stores into VibeGraph member atomics;
    // drainMeterAtomicsForUI drains those into PluginProcessor mirrors.
    std::atomic<float> peakDb  { -60.f };
    std::atomic<float> peakDbL { -60.f };
    std::atomic<float> peakDbR { -60.f };
    std::array<float, MeterLatencyComp::kRingSize> peakRingL {}, peakRingR {};
    int peakRingIdx { 0 };

    // QA-Fe2 SC delay-match: pre-compensation key stash.  armScSourceTaps
    // flags SC-edge sources at block rate; the copy runs right before
    // compDelay so keys never carry the alignment delay.
    juce::AudioBuffer<float> scTap;
    std::atomic<bool>        scTapArmed { false };

    // Cached APVTS raw pointers (rebindBusApvts binds per-bus prefixes).
    // CPU-safeguarding standing rule: no string-keyed lookups per block.
    std::atomic<float>* pPolarity       { nullptr };
    std::atomic<float>* pWidth          { nullptr };
    std::atomic<float>* pBypass         { nullptr };
    std::atomic<float>* pPan            { nullptr };
    std::atomic<float>* pPanLaw         { nullptr };
    std::atomic<float>* pLevel          { nullptr };
    std::atomic<float>* pMute           { nullptr };
    std::atomic<float>* pSolo           { nullptr };
    std::atomic<float>* pGlobalFxBypass { nullptr };
    std::atomic<float>* pMasterGain     { nullptr };   // master chain only

    explicit InstrChannelNode(const juce::String& displayName) : name(displayName) {}

    void prepare(double sr, int blockSize)
    {
        preEq.prepare(sr, blockSize);   // §P4.3
        rack.prepare(sr, blockSize);
        eq  .prepare(sr, blockSize);
        scTap.setSize(2, juce::jmax(1, blockSize), false, true, false);
        mLufs.prepareToPlay(sr);        // derive K-weighting + bin ring
        // CL-044: sized to the feed's own capacity, not the block size, so a
        // later block-size increase cannot outrun it before the next prepare.
        specMonoScratch.assign((size_t) SpectrumFeed::kSize, 0.0f);
        specTp.prepare(2);
    }
    void reset()
    {
        preEq.reset();   // §P4.3
        rack.reset();
        eq  .reset();
    }

    static float loadParam(const std::atomic<float>* p, float fallback) noexcept
    {
        return p ? p->load(std::memory_order_relaxed) : fallback;
    }
    void rebindApvts(juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix)
    {
        pPolarity       = apvts.getRawParameterValue(prefix + "_polarity");
        pWidth          = apvts.getRawParameterValue(prefix + "_width");
        pBypass         = apvts.getRawParameterValue(prefix + "_bypass");
        pPan            = apvts.getRawParameterValue(prefix + "_pan");
        pPanLaw         = apvts.getRawParameterValue("master_pan_law");
        pLevel          = apvts.getRawParameterValue(prefix + "_level");
        pMute           = apvts.getRawParameterValue(prefix + "_mute");
        pSolo           = apvts.getRawParameterValue(prefix + "_solo");
        pGlobalFxBypass = apvts.getRawParameterValue("master_fx_bypass");
        pMasterGain     = apvts.getRawParameterValue("masterGain");
    }

    // Runs the standard bus DSP chain on `buf` in-place.  `buf` must already
    // contain the bus input (PassiveStripTask predecessor sum).  Steps:
    // pre-rack EQ -> rack (strip bypass OR global kill-all) -> post-rack EQ
    // -> fader x mute x unified bus-solo gate -> polarity + M/S width -> pan
    // -> SC stash -> latency-compensated comp delay -> peak publish.
    // anyBusSoloed = VibeGraph::anyBusSoloed(), computed once per block by the
    // caller and shared by every bus (QA-Ea Part A canonical formula:
    // silenced = muted || (anyBusSoloed && !soloed)).
    void processChainOnly(juce::AudioBuffer<float>& buf, double bpm,
                          bool anyBusSoloed)
    {
        if (buf.getNumChannels() >= 2) preEq.process(buf);   // §P4.3 (identity short-circuit + spectrum feed inside)
        rack.setHostTransport (VibeGraph::blockTransport (bpm));
        {
            const bool stripBypass  = loadParam(pBypass, 0.f) > 0.5f;
            const bool globalBypass = loadParam(pGlobalFxBypass, 0.f) > 0.5f;
            const bool bypass       = stripBypass || globalBypass;
            if (rack.isRackBypassed() != bypass)
                rack.setRackBypassed(bypass);
        }
        rack.process(buf);
        if (buf.getNumChannels() >= 2) eq.process(buf);   // post-rack bus EQ

        const bool  thisSolo  = loadParam(pSolo, 0.f) > 0.5f;
        const bool  thisMuted = loadParam(pMute, 0.f) > 0.5f;
        const float fadDb     = loadParam(pLevel, 0.f);
        const float fadLin    = juce::Decibels::decibelsToGain(fadDb, -60.f);
        const float g = (thisMuted || (anyBusSoloed && ! thisSolo)) ? 0.f : fadLin;
        if (g != 1.f) buf.applyGain(g);

        if (loadParam(pPolarity, 0.f) > 0.5f) buf.applyGain(-1.f);
        const float width = loadParam(pWidth, 1.f);
        if (buf.getNumChannels() >= 2 && std::abs(width - 1.f) > 1.0e-4f)
        {
            float* L = buf.getWritePointer(0);
            float* R = buf.getWritePointer(1);
            const int n = buf.getNumSamples();
            for (int s = 0; s < n; ++s)
            {
                const float m    = 0.5f * (L[s] + R[s]);
                const float side = 0.5f * (L[s] - R[s]) * width;
                L[s] = m + side;
                R[s] = m - side;
            }
        }

        applyStereoPan (buf, loadParam(pPan, 0.f), (int) loadParam(pPanLaw, 0.f));

        if (scTapArmed.load(std::memory_order_relaxed))
            for (int c = 0, tc = juce::jmin(buf.getNumChannels(), scTap.getNumChannels()); c < tc; ++c)
                scTap.copyFrom(c, 0, buf, c, 0, juce::jmin(buf.getNumSamples(), scTap.getNumSamples()));

        compDelay.process(buf);
        // Lock-free max + latency-compensated publish; ballistics live
        // entirely on the UI thread (it exchange-resets per vsync).
        publishPeakReading (buf, peakRingL, peakRingR, peakRingIdx,
                            peakDbL, peakDbR, peakDb);
    }

    // The terminal master chain: masterGain (APVTS knob) x master fader,
    // mute only (no solo -- nothing to solo against), pan BEFORE width
    // (kept verbatim from the former MasterBusNode), no polarity, no comp
    // delay / SC stash (terminal node), LUFS metering on the final sum.
    void processMasterChain(juce::AudioBuffer<float>& buf, double bpm)
    {
        if (buf.getNumChannels() >= 2) preEq.process(buf);   // §P4.3
        rack.setHostTransport (VibeGraph::blockTransport (bpm));
        // mixer_master_bypass bypasses the master rack only; master_fx_bypass
        // also bypasses every other strip's rack.  OR-ed together.
        {
            const bool stripBypass  = loadParam(pBypass, 0.f) > 0.5f;
            const bool globalBypass = loadParam(pGlobalFxBypass, 0.f) > 0.5f;
            const bool bypass       = stripBypass || globalBypass;
            if (rack.isRackBypassed() != bypass)
                rack.setRackBypassed(bypass);
        }
        rack.process(buf);
        if (buf.getNumChannels() >= 2) eq.process(buf);   // post-rack master EQ

        const float masterGain  = loadParam(pMasterGain, 1.f);
        const bool  masterMuted = loadParam(pMute, 0.f) > 0.5f;
        const float masterFadDb = loadParam(pLevel, 0.f);
        const float masterFadLn = juce::Decibels::decibelsToGain(masterFadDb, -60.f);
        const float g = masterMuted ? 0.f : (masterGain * masterFadLn);
        if (g != 1.f) buf.applyGain(g);

        applyStereoPan (buf, loadParam(pPan, 0.f), (int) loadParam(pPanLaw, 0.f));

        const float width = loadParam(pWidth, 1.f);
        if (buf.getNumChannels() >= 2 && std::abs(width - 1.f) > 1.0e-4f)
        {
            float* L = buf.getWritePointer(0);
            float* R = buf.getWritePointer(1);
            const int n = buf.getNumSamples();
            for (int s = 0; s < n; ++s)
            {
                const float m    = 0.5f * (L[s] + R[s]);
                const float side = 0.5f * (L[s] - R[s]) * width;
                L[s] = m + side;
                R[s] = m - side;
            }
        }

        // EBU R128 on the final master sum (post fader/pan/width); M/S/I
        // atoms read by the UI LUFS box.
        mLufs.process (buf);

        // CL-044 (QA-ModelShell TS7): master-out spectrum tap, at the SAME point
        // as the loudness meter -- post fader/pan/width -- so the analyzer shows
        // what leaves the app rather than a pre-fader version of it.  The push is
        // wait-free (seqlock) and gated on a UI-set flag, so a closed analyzer
        // window costs one relaxed atomic load per block.
        if (specFeedActive != nullptr
            && specFeedActive->load (std::memory_order_relaxed))
        {
            // Mono sum: a spectrum analyzer wants the programme's magnitude
            // response, and two independent traces would just overdraw.
            const int n = juce::jmin (buf.getNumSamples(), SpectrumFeed::kSize);
            if (buf.getNumChannels() >= 2)
            {
                const float* specL = buf.getReadPointer (0);
                const float* specR = buf.getReadPointer (1);
                for (int s = 0; s < n; ++s)
                    specMonoScratch[(size_t) s] = 0.5f * (specL[s] + specR[s]);
                specFeed.push (specMonoScratch.data(), n);
            }
            else
            {
                specFeed.push (buf.getReadPointer (0), n);
            }

            // BLU-108 at the master: the real inter-sample peak of what leaves
            // the app, for the analyzer's True Peak readout.
            specTp.resetPeak();
            specTp.process (buf);
            const float tpDb = specTp.truePeakDb();
            masterTpDb.store (tpDb, std::memory_order_relaxed);

            // Plain load/compare/store, not a CAS loop: this is the only writer
            // (one audio thread), and the UI's clear racing a store can at worst
            // lose one block of a take that is being restarted anyway.
            if (tpDb > masterTpMaxDb.load (std::memory_order_relaxed))
                masterTpMaxDb.store (tpDb, std::memory_order_relaxed);
        }

        publishPeakReading (buf, peakRingL, peakRingR, peakRingIdx,
                            peakDbL, peakDbR, peakDb);
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
//  VibeGraph implementation
// ═══════════════════════════════════════════════════════════════════════════════

// TS7 per-block transport snapshot.  See VibeGraph.h for why this is static.
DSPBase::HostTransport VibeGraph::sBlockTransport {};

VibeGraph::VibeGraph()  = default;

// Destructor defined here so unique_ptr<incomplete-type> compiles in the header
VibeGraph::~VibeGraph()
{
    mGraph.clear();
    // unique_ptrs are destroyed in declaration order after this body
}

// ── prepare ───────────────────────────────────────────────────────────────────
void VibeGraph::prepare(double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mBlockSize  = maxBlockSize;

    // Per-page racks - always prepared regardless of topology state
    for (auto& r : mLayerPageRacks) r.prepare(sampleRate, maxBlockSize);
    for (auto& r : mBassPageRacks)  r.prepare(sampleRate, maxBlockSize);

    // Audio clips bus - always present, created once
    if (!mAudioClipsBusNode)
        mAudioClipsBusNode = std::make_unique<InstrChannelNode>("Audio Clips Bus");
    mAudioClipsBusNode->prepare(sampleRate, maxBlockSize);

    // R3 (2026-04-23): Vox + Inst buses for live-input strip aggregation.
    // Same shape as the audio clips bus (InstrChannelNode = pre-EQ + rack +
    // post-EQ + fader + meter).
    if (!mVoxBusNode)  mVoxBusNode  = std::make_unique<InstrChannelNode>("Vox Bus");
    mVoxBusNode->prepare(sampleRate, maxBlockSize);
    if (!mInstBusNode) mInstBusNode = std::make_unique<InstrChannelNode>("Inst Bus");
    mInstBusNode->prepare(sampleRate, maxBlockSize);

    // G-6 (2026-04-29): secondary Vox/Inst buses - always allocated so audio
    // routing works whether or not the user has activated the strip on
    // Mixer.  Same shape as the primary buses (InstrChannelNode = pre-EQ +
    // rack + post-EQ + fader + meter).  Pre-process is cheap when no
    // inserts route to them (silent buffer).
    if (!mVoxBus2Node)  mVoxBus2Node  = std::make_unique<InstrChannelNode>("Vox Bus 2");
    mVoxBus2Node->prepare(sampleRate, maxBlockSize);
    if (!mInstBus2Node) mInstBus2Node = std::make_unique<InstrChannelNode>("Inst Bus 2");
    mInstBus2Node->prepare(sampleRate, maxBlockSize);
    if (!mInstBus3Node) mInstBus3Node = std::make_unique<InstrChannelNode>("Inst Bus 3");
    mInstBus3Node->prepare(sampleRate, maxBlockSize);

    // J-4 (2026-05-03): dedicated bus for BaySickRustyDrums.  Always allocated
    // so audio routing works whether or not a BaySickRustyDrums instance
    // exists; bus sums silence cheaply until 13 strips at kRustyBase..kRustyBase+12
    // are registered via ensureRustyInsertNode (J-5).
    if (!mRustyDrumsBusNode) mRustyDrumsBusNode = std::make_unique<InstrChannelNode>("RustyDrums Bus");
    mRustyDrumsBusNode->prepare(sampleRate, maxBlockSize);

    // QA-ModelShell TS6 (BLU-447): hosted VST3 instrument bus, same
    // always-allocated shape -- routing works before any plugin tab exists.
    if (!mPluginsBusNode) mPluginsBusNode = std::make_unique<InstrChannelNode>("Plugins Bus");
    mPluginsBusNode->prepare(sampleRate, maxBlockSize);

    if (!mTopologyBuilt) return;

    // Re-prepare all nodes (e.g. sample rate change)
    mLayersNode    ->prepare(sampleRate, maxBlockSize);
    mBassNode      ->prepare(sampleRate, maxBlockSize);
    mDrumsNode     ->prepare(sampleRate, maxBlockSize);
    mMasterNode    ->prepare(sampleRate, maxBlockSize);
    mEffectsBusNode->prepare(sampleRate, maxBlockSize);
    for (auto& [id, node] : mInstrChannelNodes)
        node->prepare(sampleRate, maxBlockSize);

    // 5F-4a: per-insert nodes.  QA-InsertMaps (2026-05-24): single sweep over
    // mLiveInsertChannels covers all 8 InsertKinds (the original pointer-init-
    // list at this site excluded Rusty -- Finding A in QA-InsertMaps Task 1
    // inventory; defensive since ensureInsertNode already calls prepare() at
    // construction when mSampleRate > 0.0, but symmetric end state).
    for (int chId : mLiveInsertChannels)
        if (auto* n = mInsertsByChannel[(size_t) chId].get())
            n->prepare(sampleRate, maxBlockSize);

    // QA-AudioMeters (2026-05-24): initialise the per-kind public-member peak
    // arrays to -60 dB.  std::atomic<float> default-construction leaves value
    // unspecified pre-C++20; explicit init here matches the existing bus atomic
    // pattern (which uses in-class { -60.f } initializers).
    auto initArray = [] (auto& arr) noexcept
    {
        for (auto& a : arr) a.store (-60.f, std::memory_order_relaxed);
    };
    initArray (layerInsertPeakDbL);  initArray (layerInsertPeakDbR);
    initArray (bassInsertPeakDbL);   initArray (bassInsertPeakDbR);
    initArray (drumInsertPeakDbL);   initArray (drumInsertPeakDbR);
    initArray (audioInsertPeakDbL);  initArray (audioInsertPeakDbR);
    initArray (auxInsertPeakDbL);    initArray (auxInsertPeakDbR);
    initArray (voxInsertPeakDbL);    initArray (voxInsertPeakDbR);
    initArray (instInsertPeakDbL);   initArray (instInsertPeakDbR);
    initArray (rustyInsertPeakDbL);  initArray (rustyInsertPeakDbR);
    initArray (pluginInsertPeakDbL); initArray (pluginInsertPeakDbR);
}

// ── reset ─────────────────────────────────────────────────────────────────────
void VibeGraph::reset()
{
    mGraph.reset();
    for (auto& r : mLayerPageRacks) r.reset();
    for (auto& r : mBassPageRacks)  r.reset();
    if (mAudioClipsBusNode) mAudioClipsBusNode->reset();
    if (mVoxBusNode)        mVoxBusNode->reset();
    if (mInstBusNode)       mInstBusNode->reset();
    if (mVoxBus2Node)       mVoxBus2Node->reset();
    if (mInstBus2Node)      mInstBus2Node->reset();
    if (mInstBus3Node)      mInstBus3Node->reset();
    if (mRustyDrumsBusNode) mRustyDrumsBusNode->reset();
    if (mPluginsBusNode)    mPluginsBusNode->reset();
    if (!mTopologyBuilt) return;
    mLayersNode    ->reset();
    mBassNode      ->reset();
    mDrumsNode     ->reset();
    mMasterNode    ->reset();
    mEffectsBusNode->reset();
    for (auto& [id, node] : mInstrChannelNodes)
        node->reset();
    // 5F-4a: per-insert nodes.  QA-InsertMaps (2026-05-24): single sweep over
    // mLiveInsertChannels covers all 8 InsertKinds (the original pointer-init-
    // list at this site only covered 5 kinds -- Finding B in QA-InsertMaps
    // Task 1 inventory; pre-existing oversight when R1 / J-4 extended
    // InsertKind without updating this sweep; Sub-G Option 2 resolution
    // includes Vox / Inst / Rusty for symmetric end state.  QA-ModelShell TS2
    // (2026-07-27): reset() gained its first callers -- the offline render
    // drive's wet-tail hygiene (both sides of the render).  The transport-
    // Stop wiring from the §9 Forks 2026-05-24 entry remains future work.
    for (int chId : mLiveInsertChannels)
        if (auto* n = mInsertsByChannel[(size_t) chId].get())
            n->reset();
}

// ── buildFixedTopology ────────────────────────────────────────────────────────
void VibeGraph::buildFixedTopology(juce::Synthesiser&                  synth,
                                    BassSynth&                          bass,
                                    juce::AudioProcessorValueTreeState& apvts)
{
    if (mTopologyBuilt) return;   // safe to call every prepareToPlay - no-op after first

    mApvts = &apvts;   // 5F-4a: captured for ensureInsertNode / rebindApvts
    // Cache global "kill-all" FX bypass pointer for every rack process site.
    mGlobalFxBypassPtr = apvts.getRawParameterValue("master_fx_bypass");

    // §P4.3 B7: every bus has its own preEq member - no external EQ refs needed.
    // CL-301: one node type for all five (the synth/bass refs died with the
    // dead render-fallback paths; masterGain reads via the cached pointer
    // rebindBusApvts binds).  The signature keeps synth/bass for API
    // stability with the PluginProcessor caller.
    juce::ignoreUnused (synth, bass);
    mLayersNode     = std::make_unique<InstrChannelNode> ("Layers Bus");
    mBassNode       = std::make_unique<InstrChannelNode> ("Bass Bus");
    mDrumsNode      = std::make_unique<InstrChannelNode> ("Drums Bus");
    mMasterNode     = std::make_unique<InstrChannelNode> ("Master");
    mEffectsBusNode = std::make_unique<InstrChannelNode> ("FX Bus");


    // Prepare all nodes with the current sample rate (may already be set)
    if (mSampleRate > 0.0)
    {
        mLayersNode    ->prepare(mSampleRate, mBlockSize);
        mBassNode      ->prepare(mSampleRate, mBlockSize);
        mDrumsNode     ->prepare(mSampleRate, mBlockSize);
        mMasterNode    ->prepare(mSampleRate, mBlockSize);
        mEffectsBusNode->prepare(mSampleRate, mBlockSize);
        for (auto& r : mLayerPageRacks) r.prepare(mSampleRate, mBlockSize);
        for (auto& r : mBassPageRacks)  r.prepare(mSampleRate, mBlockSize);
    }

    mTopologyBuilt = true;

    // The fresh master node's tap pointer must be installed HERE, not only in
    // the two setters: analysis is always on since §3.1, and a node built
    // after those setters last ran would push nothing until one of them
    // happened to be called again.
    updateMasterTapFlag();

    // Apply any rack state that arrived before the topology was built (VST3 host order).
    if (mPendingRackState.isValid())
    {
        applyRackStates(mPendingRackState);
        mPendingRackState = juce::ValueTree{};
    }
}

void VibeGraph::processMasterBus(juce::AudioBuffer<float>& sumBuf, double bpm)
{
    if (mMasterNode == nullptr) return;

    constexpr float kBusNegInf = -std::numeric_limits<float>::infinity();
    // C.4 Phase 1: push SC array to master chain before processing.
    pushScArrayToStrip(MixerChannelIds::kMaster);
    mMasterNode->processMasterChain(sumBuf, bpm);
    // 2026-05-02: drain via exchange-with--inf so the node atomic's
    // running-max window resets per block.
    masterPeakDb .store(mMasterNode->peakDb .exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
    masterPeakDbL.store(mMasterNode->peakDbL.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
    masterPeakDbR.store(mMasterNode->peakDbR.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
}

// QA-RustyMeter Task 3 (2026-05-30): UI read of the master LUFS (mode 0=Momentary
// / 1=Short-Term / 2=Integrated).  Returns -120 if the master node isn't built.
float VibeGraph::getMasterLufs (int mode) const noexcept
{
    if (mMasterNode == nullptr) return -120.f;
    switch (mode)
    {
        case 1:  return mMasterNode->mLufs.shortTerm();
        case 2:  return mMasterNode->mLufs.integrated();
        default: return mMasterNode->mLufs.momentary();
    }
}

// QA-RustyMeter Task 3 (2026-05-30): clear the Integrated accumulation (Momentary
// / Short-Term keep tracking).  Called by PluginProcessor on transport
// play-from-top / loop-start.  Audio-thread safe (histogram fill, no alloc).
void VibeGraph::resetMasterLufsIntegrated() noexcept
{
    if (mMasterNode != nullptr) mMasterNode->mLufs.resetIntegrated();
}

// CL-044 (QA-ModelShell TS7): master-out spectrum tap control + drain.
void VibeGraph::setMasterSpectrumActive (bool on) noexcept
{
    mMasterSpecWanted.store (on, std::memory_order_relaxed);
    updateMasterTapFlag();
}

// TS7 §3.1: version capture is the SECOND client of this tap, and its analysis
// half is always on -- so the tap can no longer be owned by whether the analyzer
// window happens to be open.  Two independent wants, OR'd, rather than one
// window's suspend hook writing the flag directly.
void VibeGraph::setMasterAnalysisActive (bool on) noexcept
{
    mMasterAnalysisWanted.store (on, std::memory_order_relaxed);
    updateMasterTapFlag();
}

float VibeGraph::getMasterTruePeakMaxDb() const noexcept
{
    if (mMasterNode == nullptr) return -144.0f;
    return mMasterNode->masterTpMaxDb.load (std::memory_order_relaxed);
}

void VibeGraph::resetMasterTruePeakMax() noexcept
{
    if (mMasterNode != nullptr)
        mMasterNode->masterTpMaxDb.store (-144.0f, std::memory_order_relaxed);
}

void VibeGraph::updateMasterTapFlag() noexcept
{
    mMasterSpecActive.store (
        mMasterSpecWanted.load (std::memory_order_relaxed)
            || mMasterAnalysisWanted.load (std::memory_order_relaxed),
        std::memory_order_relaxed);
    // Re-point on every call AND from buildFixedTopology: the pointer must be
    // installed whenever a master node exists, whichever came first -- the
    // build or the last setter call.
    if (mMasterNode != nullptr)
        mMasterNode->specFeedActive = &mMasterSpecActive;
}

bool VibeGraph::pollMasterSpectrum (float* dest, int& outCount) noexcept
{
    outCount = 0;
    if (dest == nullptr || mMasterNode == nullptr) return false;
    if (! mMasterSpecActive.load (std::memory_order_relaxed)) return false;
    return mMasterNode->specFeed.poll (dest, outCount);
}

// -144 when the tap is inactive: reporting a stale peak would be worse than
// reporting none, since the analyzer cannot tell the difference.
float VibeGraph::getMasterTruePeakDb() const noexcept
{
    if (mMasterNode == nullptr) return -144.0f;
    if (! mMasterSpecActive.load (std::memory_order_relaxed)) return -144.0f;
    return mMasterNode->masterTpDb.load (std::memory_order_relaxed);
}

void VibeGraph::processBus(int busChId, juce::AudioBuffer<float>& buf,
                            double bpm, int panLaw)
{
    using namespace MixerChannelIds;

    // QA-Ea Part A (2026-05-21): unified bus-solo gate.  Computed ONCE here
    // and shared across every bus's chain (L/B/D + FxBus + generic Clips/Vox/
    // Inst/Vox2/Inst2/Inst3/Rusty).  Replaces the 3 prior scattered formulas:
    // (1) per-BusNode L+B+D-sibling sums in processChainOnly,
    // (2) the generic-path `useGroupSolo` 6-bus formula at the Clips/Vox/
    //     Inst switch below + the ClipsBus 6-bus override,
    // (3) PluginProcessor.cpp's receive-group busAnySolo that excluded L/B/D.
    // GUARDRAIL: anyBusSoloed reads BUS _solo ONLY -- never strip-level.
    // Per-strip _solo is a separate axis (InsertNode's own gate); untouched.
    const bool anyBus = anyBusSoloed();
    constexpr float kBusNegInf = -std::numeric_limits<float>::infinity();

    // Master keeps its dedicated helper (terminal chain + SC push inside).
    // CL-301: FxBus runs the same unified chain as every other bus (its old
    // processEffectsBus delegate is gone; the SC push it did is kept here).
    if (busChId == kMaster)  { processMasterBus(buf, bpm);                       return; }
    if (busChId == kFxBus)
    {
        if (mEffectsBusNode != nullptr)
        {
            pushScArrayToStrip(kFxBus);
            mEffectsBusNode->processChainOnly(buf, bpm, anyBus);
            fxBusPeakDb .store(mEffectsBusNode->peakDb .exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
            fxBusPeakDbL.store(mEffectsBusNode->peakDbL.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
            fxBusPeakDbR.store(mEffectsBusNode->peakDbR.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
            publishRms (buf, fxBusRmsDbL, fxBusRmsDbR);   // QA-RustyMeter split-meter RMS feed
        }
        return;
    }

    // Push SC array before any DSP so source-side fanout reaches consumer DSP.
    pushScArrayToStrip(busChId);

    // Layers / Bass / Drums delegate to BusNode::processChainOnly + drain peaks.
    // Caller is responsible for ensuring `buf` already contains the pre-summed
    // input (PassiveStripTask predecessor sum of upstream Layer/Bass/Drum
    // InsertNode outputs).
    if (busChId == kLayersBus)
    {
        if (mLayersNode == nullptr) return;
        mLayersNode->processChainOnly(buf, bpm, anyBus);
        layersPeakDb .store(mLayersNode->peakDb .exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        layersPeakDbL.store(mLayersNode->peakDbL.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        layersPeakDbR.store(mLayersNode->peakDbR.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        publishRms (buf, layersRmsDbL, layersRmsDbR);   // QA-RustyMeter split-meter RMS feed
        return;
    }
    if (busChId == kBassBus)
    {
        if (mBassNode == nullptr) return;
        mBassNode->processChainOnly(buf, bpm, anyBus);
        bassPeakDb .store(mBassNode->peakDb .exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        bassPeakDbL.store(mBassNode->peakDbL.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        bassPeakDbR.store(mBassNode->peakDbR.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        publishRms (buf, bassRmsDbL, bassRmsDbR);   // QA-RustyMeter split-meter RMS feed
        return;
    }
    if (busChId == kDrumsBus)
    {
        if (mDrumsNode == nullptr) return;
        mDrumsNode->processChainOnly(buf, bpm, anyBus);
        drumsPeakDb .store(mDrumsNode->peakDb .exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        drumsPeakDbL.store(mDrumsNode->peakDbL.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        drumsPeakDbR.store(mDrumsNode->peakDbR.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        publishRms (buf, drumsRmsDbL, drumsRmsDbR);   // QA-RustyMeter split-meter RMS feed
        return;
    }

    // Remaining buses (Clips / Vox / Inst / Vox2 / Inst2 / Inst3 / Rusty /
    // Plugins -- eight since BLU-447 added the Plugins bus).
    // CL-301: the inline chain that lived here (string-keyed APVTS lookups per
    // block + the applyXxxPolarityWidth helper switch) is gone -- these buses
    // run the SAME InstrChannelNode::processChainOnly as L/B/D/FX, with
    // cached param pointers bound in rebindBusApvts.  processBus's remaining
    // per-bus knowledge is node + RMS-atomic selection.
    if (mApvts == nullptr)        return;
    if (buf.getNumChannels() < 2) return;

    InstrChannelNode* node = nullptr;
    std::atomic<float>* rmsL = nullptr;
    std::atomic<float>* rmsR = nullptr;

    switch (busChId)
    {
        case kClipsBus:
            node = mAudioClipsBusNode.get();
            rmsL = &audioClipsRmsDbL; rmsR = &audioClipsRmsDbR;
            break;
        case kVoxBus:
            node = mVoxBusNode.get();
            rmsL = &voxBusRmsDbL; rmsR = &voxBusRmsDbR;
            break;
        case kInstBus:
            node = mInstBusNode.get();
            rmsL = &instBusRmsDbL; rmsR = &instBusRmsDbR;
            break;
        case kVoxBus2:
            node = mVoxBus2Node.get();
            rmsL = &voxBus2RmsDbL; rmsR = &voxBus2RmsDbR;
            break;
        case kInstBus2:
            node = mInstBus2Node.get();
            rmsL = &instBus2RmsDbL; rmsR = &instBus2RmsDbR;
            break;
        case kInstBus3:
            node = mInstBus3Node.get();
            rmsL = &instBus3RmsDbL; rmsR = &instBus3RmsDbR;
            break;
        case kRustyDrumsBus:
            node = mRustyDrumsBusNode.get();
            rmsL = &rustyDrumsBusRmsDbL; rmsR = &rustyDrumsBusRmsDbR;
            break;
        case kPluginsBus:
            node = mPluginsBusNode.get();
            rmsL = &pluginsBusRmsDbL; rmsR = &pluginsBusRmsDbR;
            break;
        default:
            jassertfalse;
            return;
    }

    if (node == nullptr) return;

    node->processChainOnly (buf, bpm, anyBus);
    publishRms (buf, *rmsL, *rmsR);   // QA-RustyMeter split-meter RMS feed

    // Exchange-store the migrated buses' node-internal peak atomics into
    // VibeGraph member atomics (parallel to L/B/D pattern at the top of this
    // function).  Tasks 4-6 extend with else-if branches for the remaining
    // InstrChannelNode-backed buses.
    if (busChId == kClipsBus && mAudioClipsBusNode != nullptr)
    {
        audioClipsPeakDb .store(mAudioClipsBusNode->peakDb .exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        audioClipsPeakDbL.store(mAudioClipsBusNode->peakDbL.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        audioClipsPeakDbR.store(mAudioClipsBusNode->peakDbR.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
    }
    else if (busChId == kVoxBus && mVoxBusNode != nullptr)
    {
        voxBusPeakDb .store(mVoxBusNode->peakDb .exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        voxBusPeakDbL.store(mVoxBusNode->peakDbL.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        voxBusPeakDbR.store(mVoxBusNode->peakDbR.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
    }
    else if (busChId == kVoxBus2 && mVoxBus2Node != nullptr)
    {
        voxBus2PeakDb .store(mVoxBus2Node->peakDb .exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        voxBus2PeakDbL.store(mVoxBus2Node->peakDbL.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        voxBus2PeakDbR.store(mVoxBus2Node->peakDbR.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
    }
    else if (busChId == kInstBus && mInstBusNode != nullptr)
    {
        instBusPeakDb .store(mInstBusNode->peakDb .exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        instBusPeakDbL.store(mInstBusNode->peakDbL.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        instBusPeakDbR.store(mInstBusNode->peakDbR.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
    }
    else if (busChId == kInstBus2 && mInstBus2Node != nullptr)
    {
        instBus2PeakDb .store(mInstBus2Node->peakDb .exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        instBus2PeakDbL.store(mInstBus2Node->peakDbL.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        instBus2PeakDbR.store(mInstBus2Node->peakDbR.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
    }
    else if (busChId == kInstBus3 && mInstBus3Node != nullptr)
    {
        instBus3PeakDb .store(mInstBus3Node->peakDb .exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        instBus3PeakDbL.store(mInstBus3Node->peakDbL.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        instBus3PeakDbR.store(mInstBus3Node->peakDbR.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
    }
    else if (busChId == kRustyDrumsBus && mRustyDrumsBusNode != nullptr)
    {
        rustyDrumsBusPeakDb .store(mRustyDrumsBusNode->peakDb .exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        rustyDrumsBusPeakDbL.store(mRustyDrumsBusNode->peakDbL.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        rustyDrumsBusPeakDbR.store(mRustyDrumsBusNode->peakDbR.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
    }
    else if (busChId == kPluginsBus && mPluginsBusNode != nullptr)
    {
        pluginsBusPeakDb .store(mPluginsBusNode->peakDb .exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        pluginsBusPeakDbL.store(mPluginsBusNode->peakDbL.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        pluginsBusPeakDbR.store(mPluginsBusNode->peakDbR.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
    }
}

// ── EffectRack getters ────────────────────────────────────────────────────────
// 2026-05-05 dirty-flag wiring: walk every currently-known rack and chain
// its onSlotsChanged into VibeGraph::onAnyRackChanged.  Called by
// PluginProcessor after it sets the onAnyRackChanged callback.  ensureInsertNode
// also wires per-insert racks at creation time, so this only needs to cover
// pre-built ones (bus + per-page + already-existing inserts).
void VibeGraph::rebindAllRackHooks()
{
    auto fire = [this]() { if (onAnyRackChanged) onAnyRackChanged(); };

    auto chain = [&fire] (EffectRack* r) { if (r) r->onSlotsChanged = fire; };

    chain (getLayersBusRack());
    chain (getBassBusRack());
    chain (getDrumsBusRack());
    chain (getMasterRack());
    chain (getEffectsBusRack());
    chain (getAudioClipsBusRack());
    chain (getVoxBusRack());
    chain (getInstBusRack());
    chain (getVoxBus2Rack());
    chain (getInstBus2Rack());
    chain (getInstBus3Rack());
    chain (getRustyDrumsBusRack());
    chain (getPluginsBusRack());

    for (int i = 0; i < (int) mLayerPageRacks.size(); ++i)  chain (&mLayerPageRacks[(size_t) i]);
    for (int i = 0; i < (int) mBassPageRacks.size();  ++i)  chain (&mBassPageRacks[(size_t) i]);

    // QA-InsertMaps (2026-05-24): single sweep over mLiveInsertChannels
    // replaces the walkInserts lambda + 8 per-kind calls.
    for (int chId : mLiveInsertChannels)
        if (auto* n = mInsertsByChannel[(size_t) chId].get())
            chain (&n->rack);
}

EffectRack* VibeGraph::getLayersBusRack()     { return mLayersNode       ? &mLayersNode      ->rack : nullptr; }
EffectRack* VibeGraph::getBassBusRack()       { return mBassNode         ? &mBassNode        ->rack : nullptr; }
EffectRack* VibeGraph::getDrumsBusRack()      { return mDrumsNode        ? &mDrumsNode       ->rack : nullptr; }
EffectRack* VibeGraph::getMasterRack()        { return mMasterNode       ? &mMasterNode      ->rack : nullptr; }
EffectRack* VibeGraph::getEffectsBusRack()    { return mEffectsBusNode   ? &mEffectsBusNode  ->rack : nullptr; }

EffectRack* VibeGraph::getAudioClipsBusRack() { return mAudioClipsBusNode ? &mAudioClipsBusNode->rack : nullptr; }
EffectRack* VibeGraph::getVoxBusRack()        { return mVoxBusNode        ? &mVoxBusNode       ->rack : nullptr; }
EffectRack* VibeGraph::getInstBusRack()       { return mInstBusNode       ? &mInstBusNode      ->rack : nullptr; }
EffectRack* VibeGraph::getVoxBus2Rack()       { return mVoxBus2Node       ? &mVoxBus2Node      ->rack : nullptr; }
EffectRack* VibeGraph::getInstBus2Rack()      { return mInstBus2Node      ? &mInstBus2Node     ->rack : nullptr; }
EffectRack* VibeGraph::getInstBus3Rack()      { return mInstBus3Node      ? &mInstBus3Node     ->rack : nullptr; }
EffectRack* VibeGraph::getRustyDrumsBusRack() { return mRustyDrumsBusNode ? &mRustyDrumsBusNode->rack : nullptr; }
EffectRack* VibeGraph::getPluginsBusRack()    { return mPluginsBusNode    ? &mPluginsBusNode   ->rack : nullptr; }

EffectRack* VibeGraph::getLayerPageRack(int idx)
{
    // 5F-4a Batch 6: prefer the InsertNode's rack; fall back to the legacy array.
    // QA-InsertMaps (2026-05-24): InsertNode lookup via flat array.
    if (auto* node = getInsertNode(InsertKind::Layer, idx))
        return &node->rack;
    if (idx < 0 || idx >= kMaxLayerPages) return nullptr;
    return &mLayerPageRacks[idx];
}
EffectRack* VibeGraph::getBassPageRack(int idx)
{
    // QA-InsertMaps (2026-05-24): InsertNode lookup via flat array.
    if (auto* node = getInsertNode(InsertKind::Bass, idx))
        return &node->rack;
    if (idx < 0 || idx >= kMaxBassPages) return nullptr;
    return &mBassPageRacks[idx];
}

EffectRack* VibeGraph::getAuxRack(int idx)
{
    // QA-InsertMaps (2026-05-24): InsertNode lookup via flat array.
    if (auto* node = getInsertNode(InsertKind::Aux, idx))
        return &node->rack;
    return nullptr;
}

// ── PDC ───────────────────────────────────────────────────────────────────────
// QA-Fe2 PDC full-graph pass (2026-07-16): two-stage minimal-latency solve
// replacing the single-max model (which measured only the 4 L/B/D/Master
// rack+postEq pairs + the Vox engine chains -- ~130 per-insert racks, the FX
// bus, and the 7 InstrChannelNode buses were invisible, and preEq was omitted
// everywhere).
//   Stage 1 (strip): each live insert's own path latency (engine chain via
//   the Vox/Inst hooks + preEq + rack + postEq) aligns WITHIN its actual
//   main-out bus (_sendTo, natural parent fallback) through the per-insert
//   delay:  want = A(bus) - own,  A(bus) = max own over that bus's members.
//   Stage 2 (bus): each bus's natural output latency A(bus) + busChain aligns
//   cross-path through the bus delay:  want = T - natural,  T = longest path.
// Aux strips align to each other (auxStage) so their summed FX-bus feed stays
// coherent; the FX input reference is maxA + auxStage, which lands the most
// latent source's send return exactly on the aligned mix.
// Deliberate residuals (per-NODE delays cannot express per-EDGE timing):
// sends from different buses into one aux mix at A(b)-relative offsets;
// bus->bus / bus->aux sends and re-cabled aux main-outs arrive a stage off;
// exact per-edge alignment is future work.
// Message thread only -- setDelay allocates + clears, so no-op updates are
// skipped to keep the periodic poll from glitching a running mix.  The audio
// thread consumes totalLatencySamples atomically (metronome offset,
// master-recorder trim).
int VibeGraph::updateBusLatencies()
{
    if (!mTopologyBuilt) return 0;

    using namespace MixerChannelIds;
    const int ch = 2;

    auto chainLat = [](EQ8MsDSP& pre, EffectRack& rack, EQ8MsDSP& post)
    {
        return juce::jmax(0, pre .getLatencySamples())
             + juce::jmax(0, rack.getTotalLatencySamples())
             + juce::jmax(0, post.getLatencySamples());
    };

    // Stage-2 table: the 10 strip-fed buses.  Index order is load-bearing for
    // busIndexFor below only; FX and Master are handled apart (FX is send-fed,
    // Master is the terminal sum).
    struct BusSlot { CompDelayLine* delay; int chain; int maxOwn; };
    std::array<BusSlot, 11> buses {};
    auto busIndexFor = [](int chId) -> int
    {
        switch (chId)
        {
            case kLayersBus:     return 0;
            case kBassBus:       return 1;
            case kDrumsBus:      return 2;
            case kClipsBus:      return 3;
            case kVoxBus:        return 4;
            case kInstBus:       return 5;
            case kVoxBus2:       return 6;
            case kInstBus2:      return 7;
            case kInstBus3:      return 8;
            case kRustyDrumsBus: return 9;
            case kPluginsBus:    return 10;   // QA-ModelShell TS6
        }
        return -1;
    };
    buses[0] = { &mLayersNode->compDelay, chainLat(mLayersNode->preEq, mLayersNode->rack, mLayersNode->eq), 0 };
    buses[1] = { &mBassNode  ->compDelay, chainLat(mBassNode  ->preEq, mBassNode  ->rack, mBassNode  ->eq), 0 };
    buses[2] = { &mDrumsNode ->compDelay, chainLat(mDrumsNode ->preEq, mDrumsNode ->rack, mDrumsNode ->eq), 0 };
    auto instrBus = [&chainLat](InstrChannelNode* n) -> BusSlot
    {
        if (n == nullptr) return { nullptr, 0, 0 };
        return { &n->compDelay, chainLat(n->preEq, n->rack, n->eq), 0 };
    };
    buses[3] = instrBus(mAudioClipsBusNode.get());
    buses[4] = instrBus(mVoxBusNode.get());
    buses[5] = instrBus(mInstBusNode.get());
    buses[6] = instrBus(mVoxBus2Node.get());
    buses[7] = instrBus(mInstBus2Node.get());
    buses[8] = instrBus(mInstBus3Node.get());
    buses[9] = instrBus(mRustyDrumsBusNode.get());
    buses[10] = instrBus(mPluginsBusNode.get());

    const int fxChain = mEffectsBusNode != nullptr
        ? chainLat(mEffectsBusNode->preEq, mEffectsBusNode->rack, mEffectsBusNode->eq) : 0;
    const int masterChain =
        chainLat(mMasterNode->preEq, mMasterNode->rack, mMasterNode->eq);

    // Stage-1 sweep: classify every live insert by its actual main-out target.
    // ownByCh / engineByCh feed the SC key-alignment solve below (-1 own =
    // channel not live).
    enum class Dest { Bus, FxFeed, MasterDirect, AuxStage };
    struct StripRec { CompDelayLine* delay; int own; Dest dest; int busIdx; };
    std::vector<StripRec> strips;
    strips.reserve(mLiveInsertChannels.size());
    std::vector<int> ownByCh    ((size_t) kMaxStripChannels, -1);
    std::vector<int> engineByCh ((size_t) kMaxStripChannels, 0);
    int auxStage = 0, fxFeedMaxOwn = 0, masterDirectMaxOwn = 0;

    for (int chId : mLiveInsertChannels)
    {
        auto* node = mInsertsByChannel[(size_t) chId].get();
        if (node == nullptr) continue;

        int engineLat = 0;
        if (chId >= kVoxBase && chId < kVoxBase + kMaxVoxStrips && onGetVoxStripChainLatency)
            engineLat = juce::jmax(0, onGetVoxStripChainLatency(chId - kVoxBase));
        else if (chId >= kInstBase && chId < kInstBase + kMaxInstStrips && onGetInstStripEngineLatency)
            engineLat = juce::jmax(0, onGetInstStripEngineLatency(chId - kInstBase));
        int own = chainLat(node->preEq, node->rack, node->eq) + engineLat;
        ownByCh   [(size_t) chId] = own;
        engineByCh[(size_t) chId] = engineLat;

        // Aux strips receive sends tapped POST-compensation, so they carry no
        // cross-path delta of their own -- only the relative auxStage align.
        if (chId >= kAuxBase && chId < kAuxBase + kMaxAuxStrips)
        {
            auxStage = juce::jmax(auxStage, own);
            strips.push_back({ &node->compDelay, own, Dest::AuxStage, -1 });
            continue;
        }

        int dst = defaultSendTo(chId);
        if (mApvts != nullptr)
            if (auto* p = mApvts->getRawParameterValue(node->apvtsPrefix + "_sendTo"))
                dst = (int) p->load();

        int bi = busIndexFor(dst);
        if (bi >= 0 && buses[(size_t) bi].delay == nullptr) bi = -1;
        if (bi < 0 && dst != kFxBus && dst != kMaster
            && ! (dst >= kAuxBase && dst < kAuxBase + kMaxAuxStrips))
        {
            // Unroutable target (e.g. a strip-to-strip cable the render graph
            // does not pull) -- fall back to the natural parent bucket so the
            // strip still aligns with its siblings.
            bi = busIndexFor(defaultSendTo(chId));
            if (bi >= 0 && buses[(size_t) bi].delay == nullptr) bi = -1;
        }

        if (bi >= 0)
        {
            buses[(size_t) bi].maxOwn = juce::jmax(buses[(size_t) bi].maxOwn, own);
            strips.push_back({ &node->compDelay, own, Dest::Bus, bi });
        }
        else if (dst == kMaster)
        {
            masterDirectMaxOwn = juce::jmax(masterDirectMaxOwn, own);
            strips.push_back({ &node->compDelay, own, Dest::MasterDirect, -1 });
        }
        else
        {
            fxFeedMaxOwn = juce::jmax(fxFeedMaxOwn, own);
            strips.push_back({ &node->compDelay, own, Dest::FxFeed, -1 });
        }
    }

    // Strip-stage reference: send taps sit at their bus's A(b), so the FX
    // input reference tracks the largest strip stage in the graph.
    int maxA = fxFeedMaxOwn;
    for (auto& b : buses) maxA = juce::jmax(maxA, b.maxOwn);

    // Cross-path target T = longest natural path at the master input.
    int T = juce::jmax(masterDirectMaxOwn, maxA + auxStage + fxChain);
    for (auto& b : buses) T = juce::jmax(T, b.maxOwn + b.chain);

    auto setDelayGuarded = [ch](CompDelayLine* d, int want)
    {
        if (d == nullptr) return;
        want = juce::jmax(0, want);
        if (d->mDelay != want) d->setDelay(want, ch);
    };

    for (const auto& s : strips)
    {
        int want = 0;
        switch (s.dest)
        {
            case Dest::Bus:          want = buses[(size_t) s.busIdx].maxOwn - s.own; break;
            case Dest::FxFeed:       want = maxA - s.own;                            break;
            case Dest::MasterDirect: want = T - s.own;                               break;
            case Dest::AuxStage:     want = auxStage - s.own;                        break;
        }
        setDelayGuarded(s.delay, want);
    }

    for (auto& b : buses)
        setDelayGuarded(b.delay, T - b.maxOwn - b.chain);
    if (mEffectsBusNode != nullptr)
        setDelayGuarded(&mEffectsBusNode->compDelay, T - (maxA + auxStage + fxChain));

    // QA-Fe2 SC delay-match (docket 1b): each SC receive line delays its key
    // by (consumer chain-input position - source natural position), so the
    // alignment delays above never skew keying.  Sources tap PRE-compensation
    // (armScSourceTaps + the per-node scTap stash), which makes "natural" the
    // source's real path latency.  A source naturally LATER than its consumer
    // (vocal chain keying a drum gate) stays late -- only per-edge graph PDC
    // could fix that direction.  Engine-internal SC readers sit deeper than
    // the chain input; the engine-stage term dominates and the intra-chain
    // offset is accepted.
    auto srcNaturalFor = [&](int src) -> int
    {
        const int bi = busIndexFor(src);
        if (bi >= 0)        return buses[(size_t) bi].maxOwn + buses[(size_t) bi].chain;
        if (src == kFxBus)  return maxA + auxStage + fxChain;
        if (src == kMaster) return T + masterChain;
        if (src >= 0 && src < kMaxStripChannels && ownByCh[(size_t) src] >= 0)
            return (src >= kAuxBase && src < kAuxBase + kMaxAuxStrips)
                     ? maxA + ownByCh[(size_t) src] : ownByCh[(size_t) src];
        return -1;
    };
    auto consumerPosFor = [&](int dst) -> int
    {
        const int bi = busIndexFor(dst);
        if (bi >= 0)        return buses[(size_t) bi].maxOwn;
        if (dst == kFxBus)  return maxA + auxStage;
        if (dst == kMaster) return T;
        if (dst >= 0 && dst < kMaxStripChannels && ownByCh[(size_t) dst] >= 0)
            return (dst >= kAuxBase && dst < kAuxBase + kMaxAuxStrips)
                     ? maxA : engineByCh[(size_t) dst];
        return 0;
    };
    auto solveScFor = [&](int dst, const juce::String& prefix)
    {
        for (int s = 0; s < kMaxScRecvSlots; ++s)
        {
            int want = 0;
            if (mApvts != nullptr)
                if (auto* p = mApvts->getRawParameterValue(prefix + "_sc_recv"
                                                           + juce::String(s) + "_from"))
                {
                    const int src = (int) p->load();
                    if (src >= 0 && src != dst)
                    {
                        const int nat = srcNaturalFor(src);
                        if (nat >= 0)
                            want = juce::jmax(0, consumerPosFor(dst) - nat);
                    }
                }
            setDelayGuarded(&mScRecvDelays[(size_t) dst][(size_t) s], want);
        }
    };
    for (int busCh : { kMaster, kLayersBus, kBassBus, kDrumsBus, kFxBus,
                       kClipsBus, kVoxBus, kInstBus, kVoxBus2, kInstBus2,
                       kInstBus3, kRustyDrumsBus, kPluginsBus })
        solveScFor(busCh, prefixFromChannelId(busCh));
    for (int chId : mLiveInsertChannels)
        if (auto* node = mInsertsByChannel[(size_t) chId].get())
            solveScFor(chId, node->apvtsPrefix);

    const int total = T + masterChain;
    totalLatencySamples.store(total, std::memory_order_relaxed);
    return total;
}

// ── EQ getters (post-rack bus EQs, one per channel) ──────────────────────────
EQ8MsDSP* VibeGraph::getLayersBusEQ()     { return mLayersNode       ? &mLayersNode      ->eq : nullptr; }
EQ8MsDSP* VibeGraph::getBassBusEQ()       { return mBassNode         ? &mBassNode        ->eq : nullptr; }
EQ8MsDSP* VibeGraph::getDrumsBusEQ()      { return mDrumsNode        ? &mDrumsNode       ->eq : nullptr; }
EQ8MsDSP* VibeGraph::getMasterEQ()        { return mMasterNode       ? &mMasterNode      ->eq : nullptr; }
EQ8MsDSP* VibeGraph::getEffectsBusEQ()    { return mEffectsBusNode   ? &mEffectsBusNode  ->eq : nullptr; }
EQ8MsDSP* VibeGraph::getAudioClipsBusEQ() { return mAudioClipsBusNode ? &mAudioClipsBusNode->eq  : nullptr; }
EQ8MsDSP* VibeGraph::getVoxBusEQ()        { return mVoxBusNode        ? &mVoxBusNode       ->eq  : nullptr; }
EQ8MsDSP* VibeGraph::getInstBusEQ()       { return mInstBusNode       ? &mInstBusNode      ->eq  : nullptr; }
EQ8MsDSP* VibeGraph::getVoxBus2EQ()       { return mVoxBus2Node       ? &mVoxBus2Node      ->eq  : nullptr; }
EQ8MsDSP* VibeGraph::getInstBus2EQ()      { return mInstBus2Node      ? &mInstBus2Node     ->eq  : nullptr; }
EQ8MsDSP* VibeGraph::getInstBus3EQ()      { return mInstBus3Node      ? &mInstBus3Node     ->eq  : nullptr; }
EQ8MsDSP* VibeGraph::getRustyDrumsBusEQ() { return mRustyDrumsBusNode ? &mRustyDrumsBusNode->eq  : nullptr; }
EQ8MsDSP* VibeGraph::getPluginsBusEQ()    { return mPluginsBusNode    ? &mPluginsBusNode   ->eq  : nullptr; }

// §P4.3: Pre-rack bus EQs (NEW - every bus gets one).
EQ8MsDSP* VibeGraph::getLayersBusPreEQ()     { return mLayersNode       ? &mLayersNode       ->preEq : nullptr; }
EQ8MsDSP* VibeGraph::getBassBusPreEQ()       { return mBassNode         ? &mBassNode         ->preEq : nullptr; }
EQ8MsDSP* VibeGraph::getDrumsBusPreEQ()      { return mDrumsNode        ? &mDrumsNode        ->preEq : nullptr; }
EQ8MsDSP* VibeGraph::getMasterPreEQ()        { return mMasterNode       ? &mMasterNode       ->preEq : nullptr; }
EQ8MsDSP* VibeGraph::getEffectsBusPreEQ()    { return mEffectsBusNode   ? &mEffectsBusNode   ->preEq : nullptr; }
EQ8MsDSP* VibeGraph::getAudioClipsBusPreEQ() { return mAudioClipsBusNode ? &mAudioClipsBusNode->preEq : nullptr; }
EQ8MsDSP* VibeGraph::getVoxBusPreEQ()        { return mVoxBusNode        ? &mVoxBusNode       ->preEq : nullptr; }
EQ8MsDSP* VibeGraph::getInstBusPreEQ()       { return mInstBusNode       ? &mInstBusNode      ->preEq : nullptr; }
EQ8MsDSP* VibeGraph::getVoxBus2PreEQ()       { return mVoxBus2Node       ? &mVoxBus2Node      ->preEq : nullptr; }
EQ8MsDSP* VibeGraph::getInstBus2PreEQ()      { return mInstBus2Node      ? &mInstBus2Node     ->preEq : nullptr; }
EQ8MsDSP* VibeGraph::getInstBus3PreEQ()      { return mInstBus3Node      ? &mInstBus3Node     ->preEq : nullptr; }
EQ8MsDSP* VibeGraph::getRustyDrumsBusPreEQ() { return mRustyDrumsBusNode ? &mRustyDrumsBusNode->preEq : nullptr; }
EQ8MsDSP* VibeGraph::getPluginsBusPreEQ()    { return mPluginsBusNode    ? &mPluginsBusNode   ->preEq : nullptr; }

// ── Rack + bus EQ state serialization ────────────────────────────────────────

namespace {
    juce::String encodeBlock(juce::MemoryBlock& block)
    {
        return juce::Base64::toBase64(block.getData(), block.getSize());
    }
    bool decodeBlock(const juce::String& b64, juce::MemoryBlock& out)
    {
        if (b64.isEmpty()) return false;
        juce::MemoryOutputStream mos;
        juce::Base64::convertFromBase64(mos, b64);
        out = mos.getMemoryBlock();
        return out.getSize() > 0;
    }
}

void VibeGraph::saveRackStates(juce::ValueTree& parent)
{
    if (!mTopologyBuilt) return;

    // 2026-05-05: every node with a §P4.3 pre-rack EQ8 M/S now writes a
    // `preEq` property alongside the existing post-rack `eq`.  Older saves
    // without `preEq` round-trip cleanly - applyRackStates only restores
    // the property when it's present.
    auto addNode = [&](const juce::String& id, EffectRack& rack,
                        EQ8MsDSP& preEq, EQ8MsDSP& eq)
    {
        juce::ValueTree node("BusRack");
        node.setProperty("id", id, nullptr);

        juce::MemoryBlock rackData, preEqData, eqData;
        rack .getStateInformation(rackData);
        preEq.getStateInformation(preEqData);
        eq   .getStateInformation(eqData);
        node.setProperty("rack",  encodeBlock(rackData),  nullptr);
        node.setProperty("preEq", encodeBlock(preEqData), nullptr);
        node.setProperty("eq",    encodeBlock(eqData),    nullptr);

        parent.addChild(node, -1, nullptr);
    };

    addNode("LayersBus",  mLayersNode    ->rack, mLayersNode    ->preEq, mLayersNode    ->eq);
    addNode("BassBus",    mBassNode      ->rack, mBassNode      ->preEq, mBassNode      ->eq);
    addNode("DrumsBus",   mDrumsNode     ->rack, mDrumsNode     ->preEq, mDrumsNode     ->eq);
    addNode("Master",     mMasterNode    ->rack, mMasterNode    ->preEq, mMasterNode    ->eq);
    addNode("EffectsBus", mEffectsBusNode->rack, mEffectsBusNode->preEq, mEffectsBusNode->eq);

    // 2026-04-24: the three special-bus InstrChannelNode racks (Audio Clips,
    // Vox, Inst) were silently dropped before - they're stored as separate
    // member pointers, not in mInstrChannelNodes, so the loop below missed
    // them.  Save each alongside the fixed buses.
    if (mAudioClipsBusNode) addNode ("ClipsBus", mAudioClipsBusNode->rack, mAudioClipsBusNode->preEq, mAudioClipsBusNode->eq);
    if (mVoxBusNode)        addNode ("VoxBus",   mVoxBusNode       ->rack, mVoxBusNode       ->preEq, mVoxBusNode       ->eq);
    if (mInstBusNode)       addNode ("InstBus",  mInstBusNode      ->rack, mInstBusNode      ->preEq, mInstBusNode      ->eq);
    if (mVoxBus2Node)       addNode ("VoxBus2",  mVoxBus2Node      ->rack, mVoxBus2Node      ->preEq, mVoxBus2Node      ->eq);
    if (mInstBus2Node)      addNode ("InstBus2", mInstBus2Node     ->rack, mInstBus2Node     ->preEq, mInstBus2Node     ->eq);
    if (mInstBus3Node)      addNode ("InstBus3", mInstBus3Node     ->rack, mInstBus3Node     ->preEq, mInstBus3Node     ->eq);
    if (mRustyDrumsBusNode) addNode ("RustyBus", mRustyDrumsBusNode->rack, mRustyDrumsBusNode->preEq, mRustyDrumsBusNode->eq);
    if (mPluginsBusNode)    addNode ("PluginsBus", mPluginsBusNode->rack, mPluginsBusNode->preEq, mPluginsBusNode->eq);

    for (int chId : mInstrChannelOrder)
    {
        auto it = mInstrChannelNodes.find(chId);
        if (it == mInstrChannelNodes.end()) continue;
        auto& ch = *it->second;

        juce::ValueTree node("InstrCh");
        node.setProperty("name", ch.name, nullptr);

        juce::MemoryBlock rackData, preEqData, eqData;
        ch.rack .getStateInformation(rackData);
        ch.preEq.getStateInformation(preEqData);
        ch.eq   .getStateInformation(eqData);
        node.setProperty("rack",  encodeBlock(rackData),  nullptr);
        node.setProperty("preEq", encodeBlock(preEqData), nullptr);
        node.setProperty("eq",    encodeBlock(eqData),    nullptr);

        parent.addChild(node, -1, nullptr);
    }

    // 2026-04-24: per-insert rack + post-rack EQ state.  Every Layer / Bass /
    // Drum / Audio / Aux / Vox / Inst insert has its own rack - before this,
    // user effect choices on any of those strips were lost on save.
    // 2026-05-05: also captures the §P4.3 pre-rack EQ (`preEq`) so per-page
    // presets round-trip both EQs.
    // QA-InsertMaps (2026-05-24): single sweep over mLiveInsertChannels with
    // node->kind dispatch to the XML kind label.  Replaces the per-kind
    // addInsertMap lambda + 8 calls.  XML tag labels MUST be preserved for
    // project load compat (restoreInsert below matches against them).
    auto kindString = [] (InsertKind k) -> const char*
    {
        switch (k)
        {
            case InsertKind::Layer: return "Layer";
            case InsertKind::Bass:  return "Bass";
            case InsertKind::Drum:  return "Drum";
            case InsertKind::Audio: return "Audio";
            case InsertKind::Aux:   return "Aux";
            case InsertKind::Vox:   return "Vox";
            case InsertKind::Inst:  return "Inst";
            case InsertKind::Rusty: return "Rusty";   // J-9 (2026-05-05)
            case InsertKind::Plugin: return "Plugin"; // QA-ModelShell TS6
        }
        return "Unknown";   // defensive; unreachable for any valid kind
    };
    for (int chId : mLiveInsertChannels)
    {
        auto* node = mInsertsByChannel[(size_t) chId].get();
        if (node == nullptr) continue;
        juce::ValueTree rec ("InsertRack");
        rec.setProperty ("kind",  kindString(node->kind), nullptr);
        rec.setProperty ("index", node->index,            nullptr);
        juce::MemoryBlock rackData, preEqData, eqData;
        node->rack .getStateInformation (rackData);
        node->preEq.getStateInformation (preEqData);
        node->eq   .getStateInformation (eqData);
        rec.setProperty ("rack",  encodeBlock (rackData),  nullptr);
        rec.setProperty ("preEq", encodeBlock (preEqData), nullptr);
        rec.setProperty ("eq",    encodeBlock (eqData),    nullptr);
        parent.addChild (rec, -1, nullptr);
    }
}

void VibeGraph::clearAllRackStates()
{
    auto wipe = [](EffectRack& r)
    {
        for (int s = 0; s < EffectRack::kNumSlots; ++s)
            r.clearSlot (s);
    };

    if (mLayersNode)        wipe (mLayersNode      ->rack);
    if (mBassNode)          wipe (mBassNode        ->rack);
    if (mDrumsNode)         wipe (mDrumsNode       ->rack);
    if (mMasterNode)        wipe (mMasterNode      ->rack);
    if (mEffectsBusNode)    wipe (mEffectsBusNode  ->rack);
    if (mAudioClipsBusNode) wipe (mAudioClipsBusNode->rack);
    if (mVoxBusNode)        wipe (mVoxBusNode      ->rack);
    if (mInstBusNode)       wipe (mInstBusNode     ->rack);
    if (mVoxBus2Node)       wipe (mVoxBus2Node     ->rack);
    if (mInstBus2Node)      wipe (mInstBus2Node    ->rack);
    if (mInstBus3Node)      wipe (mInstBus3Node    ->rack);
    if (mRustyDrumsBusNode) wipe (mRustyDrumsBusNode->rack);
    if (mPluginsBusNode)    wipe (mPluginsBusNode->rack);

    for (auto& [id, node] : mInstrChannelNodes) if (node) wipe (node->rack);
    // QA-InsertMaps (2026-05-24): single sweep over mLiveInsertChannels.
    for (int chId : mLiveInsertChannels)
        if (auto* n = mInsertsByChannel[(size_t) chId].get())
            wipe (n->rack);
}

void VibeGraph::loadRackStates(const juce::ValueTree& parent)
{
    if (!mTopologyBuilt)
    {
        // Topology not built yet - defer until end of buildFixedTopology.
        mPendingRackState = parent.createCopy();
        return;
    }
    applyRackStates(parent);
}

void VibeGraph::applyRackStates(const juce::ValueTree& parent)
{
    // 2026-05-05: when the saved record carries a `preEq` property (added in
    // the matching saveRackStates change above), restore it onto the node's
    // §P4.3 pre-rack EQ.  Older saves without `preEq` skip this step - the
    // missing-property check below keeps backward compatibility.
    auto restoreEqs = [&](const juce::ValueTree& rec, EQ8MsDSP& preEq, EQ8MsDSP& eq)
    {
        if (rec.hasProperty ("preEq"))
        {
            juce::MemoryBlock pe;
            if (decodeBlock (rec.getProperty ("preEq").toString(), pe))
                preEq.setStateInformation (pe.getData(), (int) pe.getSize());
        }
        juce::MemoryBlock eqData;
        if (decodeBlock (rec.getProperty ("eq").toString(), eqData))
            eq.setStateInformation (eqData.getData(), (int) eqData.getSize());
    };

    auto restoreNode = [&](const juce::String& id, EffectRack& rack,
                            EQ8MsDSP& preEq, EQ8MsDSP& eq)
    {
        for (int i = 0; i < parent.getNumChildren(); ++i)
        {
            auto child = parent.getChild(i);
            if (!child.hasType("BusRack")) continue;
            if (child.getProperty("id").toString() != id) continue;

            juce::MemoryBlock rackData;
            if (decodeBlock(child.getProperty("rack").toString(), rackData))
                rack.setStateInformation(rackData.getData(), (int)rackData.getSize());

            restoreEqs (child, preEq, eq);

            return;
        }
    };

    restoreNode("LayersBus",  mLayersNode    ->rack, mLayersNode    ->preEq, mLayersNode    ->eq);
    restoreNode("BassBus",    mBassNode      ->rack, mBassNode      ->preEq, mBassNode      ->eq);
    restoreNode("DrumsBus",   mDrumsNode     ->rack, mDrumsNode     ->preEq, mDrumsNode     ->eq);
    restoreNode("Master",     mMasterNode    ->rack, mMasterNode    ->preEq, mMasterNode    ->eq);
    restoreNode("EffectsBus", mEffectsBusNode->rack, mEffectsBusNode->preEq, mEffectsBusNode->eq);
    if (mAudioClipsBusNode) restoreNode ("ClipsBus", mAudioClipsBusNode->rack, mAudioClipsBusNode->preEq, mAudioClipsBusNode->eq);
    if (mVoxBusNode)        restoreNode ("VoxBus",   mVoxBusNode       ->rack, mVoxBusNode       ->preEq, mVoxBusNode       ->eq);
    if (mInstBusNode)       restoreNode ("InstBus",  mInstBusNode      ->rack, mInstBusNode      ->preEq, mInstBusNode      ->eq);
    if (mVoxBus2Node)       restoreNode ("VoxBus2",  mVoxBus2Node      ->rack, mVoxBus2Node      ->preEq, mVoxBus2Node      ->eq);
    if (mInstBus2Node)      restoreNode ("InstBus2", mInstBus2Node     ->rack, mInstBus2Node     ->preEq, mInstBus2Node     ->eq);
    if (mInstBus3Node)      restoreNode ("InstBus3", mInstBus3Node     ->rack, mInstBus3Node     ->preEq, mInstBus3Node     ->eq);
    if (mRustyDrumsBusNode) restoreNode ("RustyBus", mRustyDrumsBusNode->rack, mRustyDrumsBusNode->preEq, mRustyDrumsBusNode->eq);
    if (mPluginsBusNode)    restoreNode ("PluginsBus", mPluginsBusNode->rack, mPluginsBusNode->preEq, mPluginsBusNode->eq);

    for (int i = 0; i < parent.getNumChildren(); ++i)
    {
        auto child = parent.getChild(i);
        if (!child.hasType("InstrCh")) continue;

        juce::String name = child.getProperty("name").toString();

        for (int chId : mInstrChannelOrder)
        {
            auto it = mInstrChannelNodes.find(chId);
            if (it == mInstrChannelNodes.end() || it->second->name != name) continue;

            juce::MemoryBlock rackData;
            if (decodeBlock(child.getProperty("rack").toString(), rackData))
                it->second->rack.setStateInformation(rackData.getData(), (int)rackData.getSize());

            restoreEqs (child, it->second->preEq, it->second->eq);

            break;
        }
    }

    // 2026-04-24: per-insert rack / EQ restore.  Match by kind string + index.
    // 2026-05-05: also restores the §P4.3 pre-rack EQ when `preEq` is present.
    // QA-InsertMaps (2026-05-24): single walk through XML children with
    // kindFromString helper.  Replaces the per-kind restoreInsert lambda + 8
    // calls (the original walked parent.getNumChildren() 8 times, once per
    // kind; this version walks it once and dispatches on each child's kind
    // label).  XML tag labels must match those produced by addInsertMap above.
    auto kindFromString = [] (const juce::String& s) -> std::optional<InsertKind>
    {
        if (s == "Layer") return InsertKind::Layer;
        if (s == "Bass")  return InsertKind::Bass;
        if (s == "Drum")  return InsertKind::Drum;
        if (s == "Audio") return InsertKind::Audio;
        if (s == "Aux")   return InsertKind::Aux;
        if (s == "Vox")   return InsertKind::Vox;
        if (s == "Inst")  return InsertKind::Inst;
        if (s == "Rusty") return InsertKind::Rusty;   // J-9 (2026-05-05)
        if (s == "Plugin") return InsertKind::Plugin; // QA-ModelShell TS6
        // QA-InsertMaps Task 5 close (2026-05-25): unknown kind label -> nullopt
        // -> skipped at call site.  Restores pre-batch drop-unknown behavior
        // (pre-batch's 8 per-kind iterations dropped unrecognised labels by
        // never matching them); prevents Layer[idx] data corruption when a
        // future version adds a 9th InsertKind that older versions don't
        // recognise on project load.
        return std::nullopt;
    };
    for (int i = 0; i < parent.getNumChildren(); ++i)
    {
        auto child = parent.getChild (i);
        if (! child.hasType ("InsertRack")) continue;
        const juce::String kindStr = child.getProperty ("kind").toString();
        const int idx = (int) child.getProperty ("index", -1);
        if (idx < 0) continue;

        const auto kind = kindFromString(kindStr);
        if (! kind) continue;   // unknown kind label; drop (pre-batch parity)

        const int chId = computeChannelId(*kind, idx);
        if (chId < 0 || chId >= kMaxStripChannels) continue;
        auto* node = mInsertsByChannel[(size_t) chId].get();
        if (node == nullptr) continue;

        juce::MemoryBlock rackData;
        if (decodeBlock (child.getProperty ("rack").toString(), rackData))
            node->rack.setStateInformation (rackData.getData(), (int) rackData.getSize());

        restoreEqs (child, node->preEq, node->eq);
    }
}

// ── Instrument channel registry (dynamic, one rack+EQ per non-bus channel) ───
int VibeGraph::addInstrChannel(const juce::String& displayName)
{
    int id = mNextInstrChannelId++;
    auto node = std::make_unique<InstrChannelNode>(displayName);
    if (mSampleRate > 0.0)
        node->prepare(mSampleRate, mBlockSize);
    mInstrChannelNodes[id] = std::move(node);
    mInstrChannelOrder.push_back(id);
    if (onInstrChannelListChanged) onInstrChannelListChanged();
    return id;
}

void VibeGraph::removeInstrChannel(int channelId)
{
    mInstrChannelNodes.erase(channelId);
    auto it = std::find(mInstrChannelOrder.begin(), mInstrChannelOrder.end(), channelId);
    if (it != mInstrChannelOrder.end()) mInstrChannelOrder.erase(it);
    if (onInstrChannelListChanged) onInstrChannelListChanged();
}

juce::String VibeGraph::getInstrChannelName(int channelId) const
{
    auto it = mInstrChannelNodes.find(channelId);
    return it != mInstrChannelNodes.end() ? it->second->name : juce::String{};
}

std::vector<int> VibeGraph::getInstrChannelIds() const
{
    return mInstrChannelOrder;
}

EffectRack* VibeGraph::getInstrChannelRack(int channelId)
{
    auto it = mInstrChannelNodes.find(channelId);
    return it != mInstrChannelNodes.end() ? &it->second->rack : nullptr;
}

EQ8MsDSP* VibeGraph::getInstrChannelEQ(int channelId)
{
    auto it = mInstrChannelNodes.find(channelId);
    return it != mInstrChannelNodes.end() ? &it->second->eq : nullptr;
}

// ── Per-clip audio row channels (IDs 400 + row) ──────────────────────────────
void VibeGraph::addAudioRowChannel(int row, const juce::String& displayName)
{
    const int id = 400 + row;
    if (mInstrChannelNodes.count(id) > 0) return;
    auto node = std::make_unique<InstrChannelNode>(displayName);
    if (mSampleRate > 0.0) node->prepare(mSampleRate, mBlockSize);
    mInstrChannelNodes[id] = std::move(node);
    mInstrChannelOrder.push_back(id);   // included so Effects dropdown sees it
    if (onInstrChannelListChanged) onInstrChannelListChanged();
}

EffectRack* VibeGraph::getAudioRowRack(int row)
{
    // 5F-4a Batch 6: prefer Audio InsertNode; fall back to legacy InstrChannelNode.
    // QA-InsertMaps (2026-05-24): InsertNode lookup via flat array.
    if (auto* node = getInsertNode(InsertKind::Audio, row))
        return &node->rack;
    auto it = mInstrChannelNodes.find(400 + row);
    return it != mInstrChannelNodes.end() ? &it->second->rack : nullptr;
}

EQ8MsDSP* VibeGraph::getAudioRowEQ(int row)
{
    // 5F-4a Batch 6 migration: prefer the Audio InsertNode's EQ; fall back to
    // legacy InstrChannelNode. Mirrors the getAudioRowRack dual-path pattern.
    // Previously legacy-only, which caused the audio-row EQ tab in EffectsPage
    // to bind null when the node lived in the new InsertNode registry.
    // QA-InsertMaps (2026-05-24): InsertNode lookup via flat array.
    if (auto* node = getInsertNode(InsertKind::Audio, row))
        return &node->eq;
    auto it = mInstrChannelNodes.find(400 + row);
    return it != mInstrChannelNodes.end() ? &it->second->eq : nullptr;
}

bool VibeGraph::hasAudioRowChannel(int row) const
{
    return mInstrChannelNodes.count(400 + row) > 0;
}

// ── Phase-2 instrument node registry ─────────────────────────────────────────
VibeGraph::NodeID VibeGraph::addInstrumentNode(std::unique_ptr<juce::AudioProcessor> proc,
                                                int trackId)
{
    jassert(!hasNode(trackId));
    jassert((int)mTrackNodes.size() < 100);   // plan: max 100 instrument nodes

    if (mSampleRate > 0.0)
        proc->prepareToPlay(mSampleRate, mBlockSize);

    auto  node   = mGraph.addNode(std::move(proc));
    auto  nodeId = node->nodeID;
    mTrackNodes[trackId] = nodeId;
    return nodeId;
}

void VibeGraph::removeInstrumentNode(int trackId)
{
    auto it = mTrackNodes.find(trackId);
    if (it != mTrackNodes.end())
    {
        mGraph.removeNode(it->second);
        mTrackNodes.erase(it);
    }
}

bool VibeGraph::hasNode(int trackId) const
{
    return mTrackNodes.count(trackId) > 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  5F-4a: Per-insert node registry
//  QA-InsertMaps (2026-05-24): the `selectInsertMap` anon-namespace helper
//  that lived here pre-batch has been deleted; replaced by `computeChannelId`
//  defined near the top of this file (hoisted so the XML restore path can
//  see it).  See the comment block at the top of the file for the helper +
//  rationale.
// ═══════════════════════════════════════════════════════════════════════════════

VibeGraph::InsertNode*
VibeGraph::ensureInsertNode(InsertKind kind, int index,
                             const juce::String& displayName,
                             const juce::String& apvtsPrefix)
{
    // QA-InsertMaps (2026-05-24): flat-array lookup by ChannelId.  ChId is
    // computed once here for both the lookup AND the cached `node->chId`
    // field (see L9/Sub-D); processInsert reads node->chId directly so the
    // audio-thread kind->chId switch dies.
    const int chId = computeChannelId(kind, index);
    jassert(chId >= 0 && chId < kMaxStripChannels);
    if (chId < 0 || chId >= kMaxStripChannels) return nullptr;

    if (auto& slot = mInsertsByChannel[(size_t) chId]; slot)
    {
        // 2026-04-29: defensive re-bind.  If the node was first created before
        // mApvts was set (i.e. before buildFixedTopology), or before the strip's
        // params were registered, pLevel/pMute/etc. would be NULL and stay NULL
        // forever - strip fader/mute/meter would silently no-op.  Re-binding on
        // every ensure call keeps pointers fresh.
        if (mApvts != nullptr)
            slot->rebindApvts(*mApvts);
        return slot.get();
    }

    auto node = std::make_unique<InsertNode>(kind, index, chId, displayName, apvtsPrefix);
    if (mSampleRate > 0.0)
        node->prepare(mSampleRate, mBlockSize);
    if (mApvts != nullptr)
        node->rebindApvts(*mApvts);

    // 2026-05-05 dirty-flag wiring: route this freshly-created insert rack's
    // onSlotsChanged into VibeGraph::onAnyRackChanged → PluginProcessor →
    // editor's markDirty.  Without this, slot type swaps via the Effects page
    // wouldn't flip the project dirty bit.
    node->rack.onSlotsChanged = [this] { if (onAnyRackChanged) onAnyRackChanged(); };

    auto* raw = node.get();
    mInsertsByChannel[(size_t) chId] = std::move(node);
    mLiveInsertChannels.push_back(chId);
    return raw;
}

void VibeGraph::removeInsertNode(InsertKind kind, int index)
{
    // QA-InsertMaps (2026-05-24): flat-array reset + companion-list erase.
    const int chId = computeChannelId(kind, index);
    if (chId < 0 || chId >= kMaxStripChannels) return;
    mInsertsByChannel[(size_t) chId].reset();
    auto it = std::find(mLiveInsertChannels.begin(), mLiveInsertChannels.end(), chId);
    if (it != mLiveInsertChannels.end())
        mLiveInsertChannels.erase(it);
}

VibeGraph::InsertNode*
VibeGraph::getInsertNode(InsertKind kind, int index)
{
    // QA-InsertMaps (2026-05-24): flat-array lookup by ChannelId.
    const int chId = computeChannelId(kind, index);
    if (chId < 0 || chId >= kMaxStripChannels) return nullptr;
    return mInsertsByChannel[(size_t) chId].get();
}

void VibeGraph::processInsert(InsertKind kind, int index,
                               juce::AudioBuffer<float>& buf,
                               double bpm, bool anySolo)
{
    if (auto* node = getInsertNode(kind, index))
    {
        // C.4 Phase 1: push SC array to this strip's preEq + rack + postEq
        // before processing.  Topo order ensures any SOURCE feeding this
        // strip's SC has already populated the receive buffers via
        // the upstream SC fanout.
        // QA-InsertMaps (2026-05-24): chId is cached on the node at
        // construction; the per-block kind->chId switch that used to live
        // here died with selectInsertMap.
        if (node->chId >= 0) pushScArrayToStrip(node->chId);
        node->processBlock(buf, bpm, anySolo);

        // QA-AudioMeters fix-up (2026-05-24): CAS-max MERGE (not plain store) of
        // the InsertNode's freshly published peak into VibeGraph's per-kind
        // public-member array for the (kind, index) slot.
        // QA-MultiBlockHazard (2026-07-02): processInsert is now called ONCE per
        // block for every InsertKind -- Audio / Vox / Inst were collapsed to a
        // single summed-sources pass (previously Flow A + per-clip Flow B on
        // Audio, and a per-FilePlay-player loop on Vox / Inst).  The CAS-max is
        // retained as a harmless single-call max -- safe if a future path ever
        // re-introduces multiple calls -- but no longer compensates for a live
        // multi-call.  drainMeterAtomicsForUI's per-kind G1 loop drains the
        // accumulated mirror into the PluginProcessor mirror that the UI
        // polls.  Mono branch removed -- the m<Kind>InsertPeakDb mono mirrors
        // have no UI consumer (per-batch dead-write cleanup); InsertNode->
        // peakDb mono is still written by publishPeakReading (shared with
        // BusNodes) but is no longer extracted here.
        constexpr float kNI = -std::numeric_limits<float>::infinity();
        auto storeAxes = [&] (std::atomic<float>& dL,
                              std::atomic<float>& dR) noexcept
        {
            auto casMax = [] (std::atomic<float>& a, float v) noexcept
            {
                if (v == kNI) return;
                float cur = a.load (std::memory_order_relaxed);
                while (cur < v
                       && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed))
                {}
            };
            casMax (dL, node->peakDbL.exchange (kNI, std::memory_order_relaxed));
            casMax (dR, node->peakDbR.exchange (kNI, std::memory_order_relaxed));
        };
        switch (kind)
        {
            case InsertKind::Layer:
                if (index >= 0 && index < kMaxLayerPages)
                    storeAxes (layerInsertPeakDbL [index], layerInsertPeakDbR [index]);
                break;
            case InsertKind::Bass:
                if (index >= 0 && index < kMaxBassPages)
                    storeAxes (bassInsertPeakDbL  [index], bassInsertPeakDbR  [index]);
                break;
            case InsertKind::Drum:
                if (index >= 0 && index < kMaxDrumPages)
                    storeAxes (drumInsertPeakDbL  [index], drumInsertPeakDbR  [index]);
                break;
            case InsertKind::Audio:
                if (index >= 0 && index < kMaxAudioInserts)
                    storeAxes (audioInsertPeakDbL [index], audioInsertPeakDbR [index]);
                break;
            case InsertKind::Aux:
                if (index >= 0 && index < MixerChannelIds::kMaxAuxStrips)
                    storeAxes (auxInsertPeakDbL   [index], auxInsertPeakDbR   [index]);
                break;
            case InsertKind::Vox:
                if (index >= 0 && index < MixerChannelIds::kMaxVoxStrips)
                    storeAxes (voxInsertPeakDbL   [index], voxInsertPeakDbR   [index]);
                break;
            case InsertKind::Inst:
                if (index >= 0 && index < MixerChannelIds::kMaxInstStrips)
                    storeAxes (instInsertPeakDbL  [index], instInsertPeakDbR  [index]);
                break;
            case InsertKind::Rusty:
                if (index >= 0 && index < MixerChannelIds::kMaxRustyStrips)
                    storeAxes (rustyInsertPeakDbL [index], rustyInsertPeakDbR [index]);
                break;
            case InsertKind::Plugin:
                if (index >= 0 && index < MixerChannelIds::kMaxPluginStrips)
                    storeAxes (pluginInsertPeakDbL [index], pluginInsertPeakDbR [index]);
                break;
        }
    }
}

EffectRack* VibeGraph::getInsertRack(InsertKind kind, int index)
{
    if (auto* node = getInsertNode(kind, index))
        return &node->rack;
    return nullptr;
}

// ── TS7 §6.2: the pre-rack "Source Only" freeze tap ──────────────────────────
// Exactly ONE insert is armed at a time -- a freeze render targets one tab, and
// allowing several would mean several destinations to disambiguate for no gain.
// Arming clears any previous arm so a cancelled render cannot leave a node
// copying into a buffer nobody reads.
void VibeGraph::armFreezeTap (InsertKind kind, int index)
{
    disarmFreezeTap();
    if (auto* node = getInsertNode (kind, index))
    {
        mFreezeTapNode = node;
        node->freezeTapArmed.store (true, std::memory_order_relaxed);
    }
}

void VibeGraph::disarmFreezeTap()
{
    if (mFreezeTapNode != nullptr)
        mFreezeTapNode->freezeTapArmed.store (false, std::memory_order_relaxed);
    mFreezeTapNode = nullptr;
}

// OFFLINE USE ONLY, same contract as getStripOutputForTap: valid on the render
// thread between a processBlock return and the next call, while the device is
// suspended.
juce::AudioBuffer<float>* VibeGraph::getFreezeTapBuffer() noexcept
{
    return mFreezeTapNode != nullptr ? &mFreezeTapNode->freezeTapBuf : nullptr;
}

juce::uint32 VibeGraph::getFreezeTapSeq() const noexcept
{
    return mFreezeTapNode != nullptr
             ? mFreezeTapNode->freezeTapSeq.load (std::memory_order_acquire) : 0;
}

// QA-RustyMeter (2026-05-30): UI-thread drain of an insert node's RMS for the
// split meter's scrolling top half.  exchange-resets the node's rms atoms (the
// audio thread only CAS-maxes them via publishRms), so this returns "max RMS
// since the last call".  Unlike the peak path there is no PluginProcessor mirror
// + no audio-thread snapshot -- the RMS is a current value the UI reads off the
// node directly.  Called from MixerPage::onVBlank (message thread); insert nodes
// are created/destroyed on the message thread too, so getInsertNode can't race a
// destroy here.
std::pair<float, float> VibeGraph::drainInsertNodeRms (InsertKind kind, int index) noexcept
{
    constexpr float kNI = -std::numeric_limits<float>::infinity();
    if (auto* node = getInsertNode (kind, index))
        return { node->rmsDbL.exchange (kNI, std::memory_order_relaxed),
                 node->rmsDbR.exchange (kNI, std::memory_order_relaxed) };
    return { kNI, kNI };
}

// QA-RustyMeter part 2 (2026-05-30): UI-thread RMS drain for a bus strip's split
// meter.  exchange-resets the per-bus rms member atoms (CAS-maxed audio-side by
// publishRms in processBus); returns "max RMS since the last
// call".  Master + unknown ids return {-inf,-inf} (Master is Full-layout, no RMS
// top).  Direct VibeGraph read mirrored from drainInsertNodeRms -- no mirror.
std::pair<float, float> VibeGraph::drainBusRms (int busChId) noexcept
{
    using namespace MixerChannelIds;
    constexpr float kNI = -std::numeric_limits<float>::infinity();
    std::atomic<float>* l = nullptr;
    std::atomic<float>* r = nullptr;
    switch (busChId)
    {
        case kLayersBus:     l = &layersRmsDbL;        r = &layersRmsDbR;        break;
        case kBassBus:       l = &bassRmsDbL;          r = &bassRmsDbR;          break;
        case kDrumsBus:      l = &drumsRmsDbL;         r = &drumsRmsDbR;         break;
        case kFxBus:         l = &fxBusRmsDbL;         r = &fxBusRmsDbR;         break;
        case kClipsBus:      l = &audioClipsRmsDbL;    r = &audioClipsRmsDbR;    break;
        case kVoxBus:        l = &voxBusRmsDbL;        r = &voxBusRmsDbR;        break;
        case kVoxBus2:       l = &voxBus2RmsDbL;       r = &voxBus2RmsDbR;       break;
        case kInstBus:       l = &instBusRmsDbL;       r = &instBusRmsDbR;       break;
        case kInstBus2:      l = &instBus2RmsDbL;      r = &instBus2RmsDbR;      break;
        case kInstBus3:      l = &instBus3RmsDbL;      r = &instBus3RmsDbR;      break;
        case kRustyDrumsBus: l = &rustyDrumsBusRmsDbL; r = &rustyDrumsBusRmsDbR; break;
        case kPluginsBus:    l = &pluginsBusRmsDbL;    r = &pluginsBusRmsDbR;    break;
        default:             return { kNI, kNI };   // kMaster (Full) + anything else
    }
    return { l->exchange (kNI, std::memory_order_relaxed),
             r->exchange (kNI, std::memory_order_relaxed) };
}

EQ8MsDSP* VibeGraph::getInsertEQ(InsertKind kind, int index)
{
    if (auto* node = getInsertNode(kind, index))
        return &node->eq;
    return nullptr;
}

// §P4.3: Pre-rack EQ on every InsertNode (Layer/Bass/Drum/Audio/Aux).
EQ8MsDSP* VibeGraph::getInsertPreEQ(InsertKind kind, int index)
{
    if (auto* node = getInsertNode(kind, index))
        return &node->preEq;
    return nullptr;
}

// QA-AudioMeters (2026-05-24): getInsertPeakDb / getInsertPeakDbStereo /
// drainInsertPeakDbStereo (the per-insert UI peak readers) are gone.  The UI
// drain now lives on VibeSynthProcessor::drainInsertPeakDbStereo reading the
// per-kind PluginProcessor mirrors (m<Kind>InsertPeakDb*L/R[index]).  Audio
// publishes via InsertNode::process -> publishPeakReading -> peakDb/L/R, then
// processInsert exchange-stores into VibeGraph's per-kind public-member arrays
// (see above), and drainMeterAtomicsForUI drains those into the PluginProcessor
// mirrors at end-of-block.

// 2026-05-02: end-of-audio-block snapshot promotion.  Called once per audio
// block from PluginProcessor::processBlock AFTER all VibeGraph processing
// completes.  QA-AudioMeters (2026-05-24): the per-insert peakDb -> peakDbSnap
// promotion half was removed (peakDbSnap layer deleted -- InsertNode now uses
// the bus-pattern publishPeakReading + processInsert end-of-call exchange-
// store + drainMeterAtomicsForUI 8-per-kind G1 drain, same single boundary
// point as buses).  The rack-slot promotion half stays -- effect-panel
// DBFSMeter + VU input meters on every slot in every rack across every node
// (InsertNode + BusNode) still rely on this end-of-block promotion to update
// coherently with the rest of the meter chain.
void VibeGraph::promoteAllRackSlotSnapshots()
{
    auto promoteRack = [] (EffectRack* r)
    {
        if (r) r->promoteSlotPeakSnapshots();
    };
    // QA-InsertMaps (2026-05-24): single sweep over mLiveInsertChannels
    // replaces the promoteRacksInMap lambda + 8 per-kind calls.
    for (int chId : mLiveInsertChannels)
        if (auto* n = mInsertsByChannel[(size_t) chId].get())
            promoteRack (&n->rack);
    promoteRack (getLayersBusRack());
    promoteRack (getBassBusRack());
    promoteRack (getDrumsBusRack());
    promoteRack (getMasterRack());
    promoteRack (getEffectsBusRack());
    promoteRack (getAudioClipsBusRack());
    promoteRack (getVoxBusRack());
    promoteRack (getInstBusRack());
    promoteRack (getVoxBus2Rack());
    promoteRack (getInstBus2Rack());
    promoteRack (getInstBus3Rack());
    promoteRack (getRustyDrumsBusRack());
    promoteRack (getPluginsBusRack());
    // Deprecated per-page racks (5F-4a migration target was InsertNode racks
    // above; these still exist in the source tree).  Promote them too so any
    // legacy code path that still routes through them shows coherent meters.
    for (int i = 0; i < kMaxLayerPages; ++i)
        promoteRack (getLayerPageRack (i));
    for (int i = 0; i < kMaxBassPages; ++i)
        promoteRack (getBassPageRack (i));
}

// D3: read the insert's choke group (0 = none).  Wait-free.
// QA-InsertMaps (2026-05-24): flat-array lookup via computeChannelId.  The
// 2nd selectInsertMap-style switch that used to live here died with
// selectInsertMap itself.
int VibeGraph::getInsertChokeGroup(InsertKind kind, int index) const
{
    const int chId = computeChannelId(kind, index);
    if (chId < 0 || chId >= kMaxStripChannels) return 0;
    if (auto* node = mInsertsByChannel[(size_t) chId].get())
        if (auto* p = node->pChokeGroup)
            return juce::jlimit(0, 16, (int) std::round(p->load(std::memory_order_relaxed)));
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  5F-4a Batch 6: Bus + Master APVTS rebind + any-solo helper
// ═══════════════════════════════════════════════════════════════════════════════

void VibeGraph::rebindBusApvts()
{
    if (mApvts == nullptr) return;

    if (mLayersNode)        mLayersNode        ->rebindApvts(*mApvts, "mixer_layers");
    if (mBassNode)          mBassNode          ->rebindApvts(*mApvts, "mixer_bass");
    if (mDrumsNode)         mDrumsNode         ->rebindApvts(*mApvts, "mixer_drums");
    if (mEffectsBusNode)    mEffectsBusNode    ->rebindApvts(*mApvts, "mixer_fx");
    if (mAudioClipsBusNode) mAudioClipsBusNode ->rebindApvts(*mApvts, "mixer_clipsbus");
    if (mVoxBusNode)        mVoxBusNode        ->rebindApvts(*mApvts, "mixer_voxbus");
    if (mInstBusNode)       mInstBusNode       ->rebindApvts(*mApvts, "mixer_instbus");
    if (mVoxBus2Node)       mVoxBus2Node       ->rebindApvts(*mApvts, "mixer_voxbus2");
    if (mInstBus2Node)      mInstBus2Node      ->rebindApvts(*mApvts, "mixer_instbus2");
    if (mInstBus3Node)      mInstBus3Node      ->rebindApvts(*mApvts, "mixer_instbus3");
    if (mRustyDrumsBusNode) mRustyDrumsBusNode ->rebindApvts(*mApvts, "mixer_rustybus");
    if (mPluginsBusNode)    mPluginsBusNode    ->rebindApvts(*mApvts, "mixer_pluginbus");
    if (mMasterNode)        mMasterNode        ->rebindApvts(*mApvts, "mixer_master");

    // QA-Ea Part A (2026-05-21): cache bus _solo atomic pointers for the
    // anyBusSoloed() helper.  Order matches mBusSoloPtr[11] declaration in
    // VibeGraph.h.  CPU-safeguarding standing rule: cache the raw atomic
    // ptrs once + reuse, avoid 11 string-keyed getRawParameterValue lookups
    // per audio block.  Master is excluded -- it has no _solo param + no
    // sibling to solo against.
    static constexpr const char* kBusSoloPrefixes[12] = {
        "mixer_layers", "mixer_bass", "mixer_drums", "mixer_fx", "mixer_clipsbus",
        "mixer_voxbus", "mixer_instbus", "mixer_voxbus2", "mixer_instbus2",
        "mixer_instbus3", "mixer_rustybus", "mixer_pluginbus"
    };
    for (int i = 0; i < 12; ++i)
        mBusSoloPtr[(size_t) i] = mApvts->getRawParameterValue (
            juce::String (kBusSoloPrefixes[i]) + "_solo");
}

bool VibeGraph::isAnyInsertSoloed() const noexcept
{
    // QA-InsertMaps (2026-05-24): single sweep over mLiveInsertChannels with
    // early-return on first soloed insert.  Replaces the pointer-init-list
    // range-for over all 8 maps + nested for-each.
    for (int chId : mLiveInsertChannels)
        if (auto* n = mInsertsByChannel[(size_t) chId].get())
            if (n->isSoloed())
                return true;
    return false;
}

// QA-Ea Part A (2026-05-21): unified bus-solo helper.
//
// GUARDRAIL: reads BUS _solo params ONLY via the cached mBusSoloPtr array.
// MUST NEVER call isAnyInsertSoloed() (strip-level) -- the prior serial bug
// muted whole buses when one strip soloed, warned at
// PluginProcessor.cpp:1876-1885 + recorded in §9 nineteenth Forks.  Per-strip
// _solo is a separate axis owned by InsertNodes and is untouched.
//
// Called at audio rate (every block in every bus's processChainOnly path);
// cached-atomic loads keep it O(11) per call with no per-block string-keyed
// APVTS lookups.
bool VibeGraph::anyBusSoloed() const noexcept
{
    for (auto* p : mBusSoloPtr)
        if (p != nullptr && p->load (std::memory_order_relaxed) > 0.5f)
            return true;
    return false;
}

// CL-301: the applyXxxBusPolarityWidth wrapper family is deleted -- polarity
// + width run inside InstrChannelNode::processChainOnly for every bus.

// ═══════════════════════════════════════════════════════════════════════════════
//  5F-4b B1a: RoutingGraph
// ═══════════════════════════════════════════════════════════════════════════════

bool RoutingGraph::wouldCreateCycle(int src, int dst) const noexcept
{
    // DFS from dst: if we can reach src from dst via existing edges,
    // adding src → dst would complete a cycle.
    // C.4 Phase 1 (2026-04-30): SC edges participate in the same DAG, so
    // a sidechain cable can't sneak past the cycle check via an audio path.
    std::unordered_set<int> visited;
    std::vector<int> stack { dst };
    while (! stack.empty())
    {
        int node = stack.back();
        stack.pop_back();
        if (node == src) return true;
        if (! visited.insert(node).second) continue;
        for (const auto& e : mEdges)
            if (e.srcId == node)
                stack.push_back(e.dstId);
        for (const auto& e : mScEdges)
            if (e.srcId == node)
                stack.push_back(e.dstId);
    }
    return false;
}

bool RoutingGraph::rebuildFromApvts(juce::AudioProcessorValueTreeState& apvts,
                                     const std::vector<std::pair<int, juce::String>>& activeChannels)
{
    mEdges.clear();
    mEdges.reserve(activeChannels.size() * (1 + kMaxSendsPerStrip));
    mScEdges.clear();
    mScEdges.reserve(activeChannels.size() * kMaxScRecvsPerStrip);

    auto loadInt = [&](const juce::String& id, int fallback)
    {
        if (auto* p = apvts.getRawParameterValue(id)) return (int) p->load();
        return fallback;
    };
    auto loadFloat = [&](const juce::String& id, float fallback)
    {
        if (auto* p = apvts.getRawParameterValue(id)) return p->load();
        return fallback;
    };
    auto loadBool = [&](const juce::String& id, bool fallback)
    {
        if (auto* p = apvts.getRawParameterValue(id)) return p->load() > 0.5f;
        return fallback;
    };

    for (const auto& [chId, prefix] : activeChannels)
    {
        const int mainDst = loadInt(prefix + "_sendTo", MixerChannelIds::defaultSendTo(chId));
        if (mainDst != chId)
        {
            Edge e; e.srcId = chId; e.dstId = mainDst;
            e.amountDb = 0.f; e.prePost = false; e.isMainOut = true;
            mEdges.push_back(e);
        }
        for (int s = 0; s < kMaxSendsPerStrip; ++s)
        {
            const juce::String sp = prefix + "_send" + juce::String(s);
            const int dst = loadInt(sp + "_to", -1);
            if (dst < 0 || dst == chId) continue;
            Edge e; e.srcId = chId; e.dstId = dst;
            e.amountDb = loadFloat(sp + "_amount", 0.f);
            e.prePost  = loadBool (sp + "_prepost", false);
            e.isMainOut = false;
            mEdges.push_back(e);
        }

        // C.4 Phase 1 (2026-04-30): SC receive lines.  Each strip reads up
        // to 4 _sc_recv{N}_from params (channel id of the source, -1 = empty).
        // Build SC edges as src -> this strip on receive line N.
        for (int s = 0; s < kMaxScRecvsPerStrip; ++s)
        {
            const juce::String sp = prefix + "_sc_recv" + juce::String(s);
            const int src = loadInt(sp + "_from", -1);
            if (src < 0 || src == chId) continue;
            ScEdge e; e.srcId = src; e.dstId = chId; e.dstSlot = s;
            mScEdges.push_back(e);
        }
    }

    std::vector<int> ids;
    ids.reserve(activeChannels.size());
    for (const auto& [chId, _] : activeChannels) ids.push_back(chId);
    return computeTopo(ids);
}

bool RoutingGraph::computeTopo(const std::vector<int>& ids)
{
    // Kahn's algorithm. Drops cycle edges on retry if needed.
    // C.4 Phase 1 (2026-04-30): SC edges add a "src must process before dst"
    // constraint just like main/send edges (the src's post-everything tap is
    // read into dst's SC receive buffer that block, so src needs to have
    // produced its output already).
    std::unordered_map<int, int> inDegree;
    for (int id : ids) inDegree[id] = 0;
    for (const auto& e : mEdges)
        if (inDegree.count(e.dstId))
            ++inDegree[e.dstId];
    for (const auto& e : mScEdges)
        if (inDegree.count(e.dstId))
            ++inDegree[e.dstId];

    std::vector<int> queue;
    for (auto& [id, deg] : inDegree)
        if (deg == 0) queue.push_back(id);

    mTopoOrder.clear();
    mTopoOrder.reserve(ids.size());

    while (! queue.empty())
    {
        int node = queue.back(); queue.pop_back();
        mTopoOrder.push_back(node);
        for (const auto& e : mEdges)
        {
            if (e.srcId != node) continue;
            if (inDegree.count(e.dstId) && --inDegree[e.dstId] == 0)
                queue.push_back(e.dstId);
        }
        for (const auto& e : mScEdges)
        {
            if (e.srcId != node) continue;
            if (inDegree.count(e.dstId) && --inDegree[e.dstId] == 0)
                queue.push_back(e.dstId);
        }
    }

    if (mTopoOrder.size() == ids.size()) return true;

    // Cycle present - drop all edges between unresolved nodes and retry.
    std::unordered_set<int> unresolved;
    for (auto& [id, deg] : inDegree) if (deg > 0) unresolved.insert(id);
    mEdges.erase(std::remove_if(mEdges.begin(), mEdges.end(),
        [&](const Edge& e) { return unresolved.count(e.srcId) > 0 && unresolved.count(e.dstId) > 0; }),
        mEdges.end());

    inDegree.clear();
    for (int id : ids) inDegree[id] = 0;
    for (const auto& e : mEdges) if (inDegree.count(e.dstId)) ++inDegree[e.dstId];
    queue.clear();
    for (auto& [id, deg] : inDegree) if (deg == 0) queue.push_back(id);
    mTopoOrder.clear();
    while (! queue.empty())
    {
        int node = queue.back(); queue.pop_back();
        mTopoOrder.push_back(node);
        for (const auto& e : mEdges)
        {
            if (e.srcId != node) continue;
            if (inDegree.count(e.dstId) && --inDegree[e.dstId] == 0)
                queue.push_back(e.dstId);
        }
    }
    return false;
}

// C.4 Phase 1 (2026-04-30): per-strip SC receive buffer accessors.  Lazy
// allocation matches the channel-accumulator pattern.  Slot 0..3 maps to
// _sc_recv{N}_from APVTS / _sc_pick lookups in DSP modules.
juce::AudioBuffer<float>* VibeGraph::getScRecvBuffer (int channelId, int slotIdx)
{
    if (slotIdx < 0 || slotIdx >= kMaxScRecvSlots) return nullptr;
    auto& set = mScRecv[channelId];   // creates if missing
    auto& buf = set.bufs[(size_t) slotIdx];
    if (buf.getNumChannels() < 2 || buf.getNumSamples() < mBlockSize)
    {
        const int blockSize = mBlockSize > 0 ? mBlockSize : 512;
        buf.setSize(2, blockSize, false, true, false);
    }
    return &buf;
}

// QA-Fe2 SC delay-match (docket 1b): pre-compensation key tap lookup.
// Mirrors the pushScArrayToStrip channel map; Master has no comp delay so
// its output IS the natural tap (nullptr -> caller falls back).
const juce::AudioBuffer<float>* VibeGraph::getScSourceTap (int channelId) const
{
    using namespace MixerChannelIds;
    auto tapOf = [](const auto* node) -> const juce::AudioBuffer<float>*
    {
        if (node == nullptr) return nullptr;
        return node->scTapArmed.load (std::memory_order_relaxed) ? &node->scTap
                                                                 : nullptr;
    };
    switch (channelId)
    {
        case kLayersBus:     return tapOf (mLayersNode.get());
        case kBassBus:       return tapOf (mBassNode.get());
        case kDrumsBus:      return tapOf (mDrumsNode.get());
        case kFxBus:         return tapOf (mEffectsBusNode.get());
        case kClipsBus:      return tapOf (mAudioClipsBusNode.get());
        case kVoxBus:        return tapOf (mVoxBusNode.get());
        case kInstBus:       return tapOf (mInstBusNode.get());
        case kVoxBus2:       return tapOf (mVoxBus2Node.get());
        case kInstBus2:      return tapOf (mInstBus2Node.get());
        case kInstBus3:      return tapOf (mInstBus3Node.get());
        case kRustyDrumsBus: return tapOf (mRustyDrumsBusNode.get());
        case kPluginsBus:    return tapOf (mPluginsBusNode.get());
        default: break;
    }
    if (channelId >= 0 && channelId < kMaxStripChannels)
        return tapOf (mInsertsByChannel[(size_t) channelId].get());
    return nullptr;
}

// QA-Fe2 SC delay-match: run the (consumer, slot) key-alignment delay in
// place on the receive buffer.  Values are solved by updateBusLatencies on
// the message thread; this path is audio-thread + allocation-free.
void VibeGraph::applyScRecvDelay (int channelId, int slotIdx, int numSamples)
{
    if (slotIdx < 0 || slotIdx >= kMaxScRecvSlots) return;
    if (channelId < 0 || channelId >= kMaxStripChannels) return;
    if (numSamples <= 0) return;

    auto& d = mScRecvDelays[(size_t) channelId][(size_t) slotIdx];
    if (d.mDelay == 0) return;

    auto it = mScRecv.find (channelId);
    if (it == mScRecv.end()) return;
    auto& buf = it->second.bufs[(size_t) slotIdx];
    // CompDelayLine::process iterates its own mNumCh (2) -- require a full
    // stereo receive buffer, not just one channel.
    if (buf.getNumChannels() < 2 || buf.getNumSamples() < numSamples) return;

    juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(),
                                   buf.getNumChannels(), numSamples);
    d.process (view);
}

// QA-Fe2 SC delay-match: flag SC-edge source nodes so their process stashes
// the pre-compensation tap.  Block rate from rebuildRoutingFromApvts (audio
// thread) -- relaxed stores only, no allocation.
void VibeGraph::armScSourceTaps()
{
    using namespace MixerChannelIds;
    auto armCh = [this](int chId, bool on)
    {
        auto arm = [on](auto* node)
        {
            if (node != nullptr)
                node->scTapArmed.store (on, std::memory_order_relaxed);
        };
        switch (chId)
        {
            case kLayersBus:     arm (mLayersNode.get());       return;
            case kBassBus:       arm (mBassNode.get());         return;
            case kDrumsBus:      arm (mDrumsNode.get());        return;
            case kFxBus:         arm (mEffectsBusNode.get());   return;
            case kClipsBus:      arm (mAudioClipsBusNode.get()); return;
            case kVoxBus:        arm (mVoxBusNode.get());       return;
            case kInstBus:       arm (mInstBusNode.get());      return;
            case kVoxBus2:       arm (mVoxBus2Node.get());      return;
            case kInstBus2:      arm (mInstBus2Node.get());     return;
            case kInstBus3:      arm (mInstBus3Node.get());     return;
            case kRustyDrumsBus: arm (mRustyDrumsBusNode.get()); return;
            case kPluginsBus:    arm (mPluginsBusNode.get()); return;
            default: break;
        }
        if (chId >= 0 && chId < kMaxStripChannels)
            arm (mInsertsByChannel[(size_t) chId].get());
    };

    for (int busCh : { kLayersBus, kBassBus, kDrumsBus, kFxBus, kClipsBus,
                       kVoxBus, kInstBus, kVoxBus2, kInstBus2, kInstBus3,
                       kRustyDrumsBus, kPluginsBus })
        armCh (busCh, false);
    for (int chId : mLiveInsertChannels)
        armCh (chId, false);

    for (const auto& sce : mRoutingGraph.scEdges())
        armCh (sce.srcId, true);
}

VibeGraph::ScRecvArray VibeGraph::getScRecvArray (int channelId)
{
    ScRecvArray out {};
    auto it = mScRecv.find(channelId);
    if (it == mScRecv.end()) return out;
    for (int i = 0; i < kMaxScRecvSlots; ++i)
    {
        auto& b = it->second.bufs[(size_t) i];
        out[(size_t) i] = (b.getNumChannels() >= 2) ? &b : nullptr;
    }
    return out;
}

void VibeGraph::clearScRecvBuffers()
{
    // C.4 follow-up (2026-04-30): only clear SC receive buffers whose
    // (dstId, slot) pair is NOT currently the destination of an active
    // scEdge.  For active edges we leave the previous block's source data
    // in place -- this gives cross-order SC (where the source strip's
    // hardcoded process order is AFTER the target's) a one-block-latency
    // signal rather than silence.  Same-order SC (source processes before
    // target) overwrites the buffer in this block via the upstream
    // SC fan, so the latency window only applies when the audio order
    // can't satisfy the SC dependency directly (e.g. Drum -> Bass duck).
    //
    // For inactive edges (cable just removed, or never wired), the buffer
    // gets cleared so stale data from a deleted cable doesn't linger.
    const auto& scEdges = mRoutingGraph.scEdges();

    auto isActive = [&scEdges](int dstId, int slot) -> bool
    {
        for (const auto& e : scEdges)
            if (e.dstId == dstId && e.dstSlot == slot)
                return true;
        return false;
    };

    for (auto& [id, set] : mScRecv)
        for (int s = 0; s < (int) set.bufs.size(); ++s)
            if (set.bufs[(size_t) s].getNumChannels() >= 2 && ! isActive(id, s))
                set.bufs[(size_t) s].clear();
}

// C.4 Phase 1 (2026-04-30): push the strip's SC array to all SC-capable DSP
// modules on the strip (preEq + rack + postEq).  Caller is responsible for
// invoking before that strip's processBlock; VibeGraph::processInsert /
// processBlock / processBus do this internally for the strips they
// own, and PluginProcessor's bus loop calls it for the Vox/Inst secondary
// buses.  Address-only push so the call is cheap (4 pointer copies + maybe
// 8 forwards into preEq/rack/postEq members).
void VibeGraph::pushScArrayToStrip (int channelId)
{
    using namespace MixerChannelIds;

    const ScRecvArray arr = getScRecvArray(channelId);
    juce::AudioBuffer<float>* bufs[kMaxScRecvSlots];
    for (int i = 0; i < kMaxScRecvSlots; ++i) bufs[i] = arr[(size_t) i];

    auto push3 = [bufs](EQ8MsDSP* preEq, EffectRack* rack, EQ8MsDSP* postEq)
    {
        if (preEq)  preEq ->setSidechainBuffers(bufs, kMaxScRecvSlots);
        if (rack)   rack  ->setSidechainBuffers(bufs, kMaxScRecvSlots);
        if (postEq) postEq->setSidechainBuffers(bufs, kMaxScRecvSlots);
    };

    switch (channelId)
    {
        case kMaster:    return push3(getMasterPreEQ(),       getMasterRack(),        getMasterEQ());
        case kLayersBus: return push3(getLayersBusPreEQ(),    getLayersBusRack(),     getLayersBusEQ());
        case kBassBus:   return push3(getBassBusPreEQ(),      getBassBusRack(),       getBassBusEQ());
        case kDrumsBus:  return push3(getDrumsBusPreEQ(),     getDrumsBusRack(),      getDrumsBusEQ());
        case kFxBus:     return push3(getEffectsBusPreEQ(),   getEffectsBusRack(),    getEffectsBusEQ());
        case kClipsBus:  return push3(getAudioClipsBusPreEQ(),getAudioClipsBusRack(), getAudioClipsBusEQ());
        case kVoxBus:    return push3(getVoxBusPreEQ(),       getVoxBusRack(),        getVoxBusEQ());
        case kInstBus:   return push3(getInstBusPreEQ(),      getInstBusRack(),       getInstBusEQ());
        case kVoxBus2:   return push3(getVoxBus2PreEQ(),      getVoxBus2Rack(),       getVoxBus2EQ());
        case kInstBus2:  return push3(getInstBus2PreEQ(),     getInstBus2Rack(),      getInstBus2EQ());
        case kInstBus3:  return push3(getInstBus3PreEQ(),     getInstBus3Rack(),      getInstBus3EQ());
        case kRustyDrumsBus: return push3(getRustyDrumsBusPreEQ(), getRustyDrumsBusRack(), getRustyDrumsBusEQ());
        case kPluginsBus:    return push3(getPluginsBusPreEQ(), getPluginsBusRack(), getPluginsBusEQ());
    }

    // Insert channels: route to per-kind getters.
    auto pushInsert = [&](InsertKind kind, int idx)
    {
        push3(getInsertPreEQ(kind, idx), getInsertRack(kind, idx), getInsertEQ(kind, idx));
    };
    if (channelId >= kLayerBase && channelId < kLayerBase + 16)              return pushInsert(InsertKind::Layer, channelId - kLayerBase);
    if (channelId >= kBassBase  && channelId < kBassBase  + 16)              return pushInsert(InsertKind::Bass,  channelId - kBassBase);
    if (channelId >= kDrumBase  && channelId < kDrumBase  + 16)              return pushInsert(InsertKind::Drum,  channelId - kDrumBase);
    if (channelId >= kAudioBase && channelId < kAudioBase + 50)              return pushInsert(InsertKind::Audio, channelId - kAudioBase);
    if (channelId >= kAuxBase   && channelId < kAuxBase   + kMaxAuxStrips)   return pushInsert(InsertKind::Aux,   channelId - kAuxBase);
    if (channelId >= kVoxBase   && channelId < kVoxBase   + kMaxVoxStrips)   return pushInsert(InsertKind::Vox,   channelId - kVoxBase);
    if (channelId >= kInstBase  && channelId < kInstBase  + kMaxInstStrips)  return pushInsert(InsertKind::Inst,  channelId - kInstBase);
    if (channelId >= kRustyBase && channelId < kRustyBase + kMaxRustyStrips) return pushInsert(InsertKind::Rusty, channelId - kRustyBase);
    if (channelId >= kPluginBase && channelId < kPluginBase + kMaxPluginStrips) return pushInsert(InsertKind::Plugin, channelId - kPluginBase);
}

void VibeGraph::rebuildRoutingFromApvts()
{
    if (mApvts == nullptr) return;

    using namespace MixerChannelIds;
    mActiveChannels.clear();
    // QA-InsertMaps (2026-05-24): reserve 12 buses (Master + 11 other buses:
    // Layers / Bass / Drums / Fx / Clips / Vox / Inst / Vox2 / Inst2 / Inst3 /
    // RustyDrums) + live-insert count.  ChId already known on the node so the
    // per-kind chId helpers (layerInsert / bassInsert / ...) aren't needed in
    // the per-insert loop -- node->chId is the source of truth.  (Task 5
    // close cleanup: comment + reserve hint previously said "13 buses (master
    // + 12 buses)" which double-counted Master.)
    mActiveChannels.reserve(12 + mLiveInsertChannels.size());

    mActiveChannels.emplace_back(kMaster,    juce::String("mixer_master"));
    mActiveChannels.emplace_back(kLayersBus, juce::String("mixer_layers"));
    mActiveChannels.emplace_back(kBassBus,   juce::String("mixer_bass"));
    mActiveChannels.emplace_back(kDrumsBus,  juce::String("mixer_drums"));
    mActiveChannels.emplace_back(kFxBus,     juce::String("mixer_fx"));
    mActiveChannels.emplace_back(kClipsBus,  juce::String("mixer_clipsbus"));
    mActiveChannels.emplace_back(kVoxBus,    juce::String("mixer_voxbus"));
    mActiveChannels.emplace_back(kInstBus,   juce::String("mixer_instbus"));
    // G-6 (2026-04-29): secondary buses included always so the routing
    // graph picks up their _sendTo (set when active strip's main-out cable
    // is dragged) regardless of whether the UI strip is currently visible.
    mActiveChannels.emplace_back(kVoxBus2,   juce::String("mixer_voxbus2"));
    mActiveChannels.emplace_back(kInstBus2,  juce::String("mixer_instbus2"));
    mActiveChannels.emplace_back(kInstBus3,  juce::String("mixer_instbus3"));
    // J-5 (2026-05-03): RustyDrums Bus.  Always included so its _sendTo (to
    // Master) is part of the graph even when no Rusty strips are spawned;
    // also so SC cycle-check operations don't drop neighboring edges due
    // to an incomplete channel set.
    mActiveChannels.emplace_back(kRustyDrumsBus, juce::String("mixer_rustybus"));
    // QA-ModelShell TS6: Plugins Bus, included for the same reasons -- its
    // _sendTo belongs in the graph before any plugin strip exists, and an
    // incomplete channel set makes the SC cycle-check drop neighbouring edges.
    mActiveChannels.emplace_back(kPluginsBus, juce::String("mixer_pluginbus"));

    // QA-InsertMaps (2026-05-24): single sweep over mLiveInsertChannels
    // replaces 8 per-kind emplace_back blocks (Layer / Bass / Drum / Audio /
    // Aux / Vox / Inst / Rusty -- see commit history for each block's
    // historical comment context).  All 8 InsertKinds register their
    // _sendTo / _sendN_to APVTS values into the routing graph through this
    // unified pass; Vox / Inst / Rusty inclusion is preserved (was added
    // R1 + J-5 with their own comment blocks; consolidated here).
    for (int chId : mLiveInsertChannels)
        if (auto* n = mInsertsByChannel[(size_t) chId].get())
            mActiveChannels.emplace_back(chId, n->apvtsPrefix);

    mRoutingGraph.rebuildFromApvts(*mApvts, mActiveChannels);

    // C.4 Phase 1 (2026-04-30): pre-allocate SC receive buffers for every
    // (dst, slot) pair the routing graph references this block.  Pulls
    // allocation off the audio thread; the audio loop's getScRecvBuffer
    // calls then hit existing buffers and just .clear() them.
    for (const auto& sce : mRoutingGraph.scEdges())
        getScRecvBuffer(sce.dstId, sce.dstSlot);

    // QA-Fe2 SC delay-match: re-flag SC-edge source nodes from the fresh
    // edge list so their process stashes the pre-compensation key tap.
    armScSourceTaps();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  5F-4b B2: Aux insert processing
std::vector<int> VibeGraph::getAuxIndices() const
{
    // QA-InsertMaps (2026-05-24): filter mLiveInsertChannels to Aux range
    // (kAuxBase..kAuxBase+kMaxAuxStrips); return per-Aux idx (chId - base).
    using namespace MixerChannelIds;
    std::vector<int> result;
    for (int chId : mLiveInsertChannels)
        if (chId >= kAuxBase && chId < kAuxBase + kMaxAuxStrips)
            result.push_back(chId - kAuxBase);
    return result;
}

void VibeGraph::clearAuxInserts()
{
    // QA-InsertMaps (2026-05-24): reset every Aux slot in the flat array +
    // erase the corresponding chIds from the companion live-list.
    using namespace MixerChannelIds;
    for (auto it = mLiveInsertChannels.begin(); it != mLiveInsertChannels.end(); )
    {
        if (*it >= kAuxBase && *it < kAuxBase + kMaxAuxStrips)
        {
            mInsertsByChannel[(size_t) *it].reset();
            it = mLiveInsertChannels.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
