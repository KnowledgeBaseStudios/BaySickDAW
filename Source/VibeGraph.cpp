#include "VibeGraph.h"
#include "BassSynth.h"
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

// ── PDC compensation delay line ───────────────────────────────────────────────
// A simple stereo ring buffer. setDelay() resizes and resets it on the message
// thread (safe because processBlock() only runs between prepareToPlay calls or
// when the topology is stable). process() is audio-thread only.
struct CompDelayLine
{
    static constexpr int kMaxSamples = 8192;  // ~186 ms @ 44100

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

// ── LayersBusNode ─────────────────────────────────────────────────────────────
// Renders the polyphonic Layers synth, applies bus EQ, pushes to spectrum feed,
// runs bus EffectRack, then applies channel fader / mute / solo.
struct VibeGraph::LayersBusNode
{
    EQ8MsDSP               preEq;    // §P4.3 pre-rack
    EffectRack             rack;
    EQ8MsDSP               busEq;    // post-rack bus EQ - shown on Effects Page
    CompDelayLine          compDelay;
    std::atomic<float>     peakDb  { -60.f };
    // 2026-04-30: stereo L/R peakDb for the new split DBFSMeter.  Mono
    // peakDb (above) kept for back-compat; written as max(L, R) each block.
    std::atomic<float>     peakDbL { -60.f };
    std::atomic<float>     peakDbR { -60.f };
    // Per-node peak ring for latency-compensated meter publish (2026-05-02).
    // 16 entries = up to ~85 ms of compensation at 256-sample / 48 kHz.
    std::array<float, MeterLatencyComp::kRingSize> peakRingL {}, peakRingR {};
    int                    peakRingIdx { 0 };
    juce::Synthesiser&       synth;
    VibeGraph::BusMix&       busMix;
    // 12i: spectrum feeds now live inside each EQ8MsDSP (preFeed/postFeed members).
    // 2026-04-19 cleanup: pageEq reference removed - Layers bus pre-rack EQ had no
    // UI and was deleted. Signal flow on Layers is now: summed-layers -> rack ->
    // busEq -> fader (per-layer pre-rack EQ applied earlier in the per-layer scratch).

    // 5F-4a Batch 6: APVTS pointers for polarity + width
    std::atomic<float>* pPolarity { nullptr };
    std::atomic<float>* pWidth    { nullptr };
    // 2026-04-29: strip-local FX Bypass (mixer_layers_bypass) - bypasses THIS
    // bus's rack only.  Global kill-all (master_fx_bypass) bypasses every
    // rack on every strip; the two are OR-ed together each block.
    std::atomic<float>* pBypass   { nullptr };
    // 2026-04-29: pan (mixer_layers_pan) - applied L/R after fader.
    std::atomic<float>* pPan      { nullptr };
    std::atomic<float>* pPanLaw   { nullptr };
    // 2026-04-30 (audit B.3): read fader / mute / solo direct from APVTS
    // instead of going through busMix (PatternManager) - audio-thread
    // automation now reaches the bus at audio-block rate instead of via
    // the UI's 30 Hz applicator timer.  Cross-bus anySolo computed by
    // reading sibling-bus solo params (2 extra atomic loads per block).
    std::atomic<float>* pLevel       { nullptr };
    std::atomic<float>* pMute        { nullptr };
    std::atomic<float>* pSolo        { nullptr };
    std::atomic<float>* pSiblingBass { nullptr };   // mixer_bass_solo
    std::atomic<float>* pSiblingDrum { nullptr };   // mixer_drums_solo
    // Global "kill-all" FX bypass (master_fx_bypass) - forces rack bypass when true.
    std::atomic<float>* pGlobalFxBypass { nullptr };

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
        pSiblingBass    = apvts.getRawParameterValue("mixer_bass_solo");
        pSiblingDrum    = apvts.getRawParameterValue("mixer_drums_solo");
        pGlobalFxBypass = apvts.getRawParameterValue("master_fx_bypass");
    }

    LayersBusNode(juce::Synthesiser& s, VibeGraph::BusMix& m)
        : synth(s), busMix(m) {}

    void prepare(double sr, int blockSize)
    {
        preEq.prepare(sr, blockSize);   // §P4.3
        rack .prepare(sr, blockSize);
        busEq.prepare(sr, blockSize);
    }
    void reset()
    {
        preEq.reset();   // §P4.3
        rack .reset();
        busEq.reset();
    }

    void processBlock(juce::AudioBuffer<float>& buf,
                      juce::MidiBuffer& midi, double bpm,
                      juce::AudioBuffer<float>* preRendered = nullptr)
    {
        const int n = buf.getNumSamples();
        buf.clear();
        if (preRendered && preRendered->getNumSamples() == n)
        {
            const int srcCh = juce::jmin(buf.getNumChannels(), preRendered->getNumChannels());
            for (int c = 0; c < srcCh; ++c)
                buf.addFrom(c, 0, *preRendered, c, 0, n);
        }
        else
        {
            synth.renderNextBlock(buf, midi, 0, n);
        }
        // 2026-05-06 (Batch 9b): post-render DSP factored into processChainOnly
        // so VibeGraph::processBus can run the chain on a buffer that already
        // contains the pre-summed input from PassiveStripTask.
        // QA-Ea Part A (2026-05-21): synth-fallback path is dead per the
        // comment at the legacy VibeGraph::processBlock site (deleted by
        // QA-Ef; modern callers always supply an accumulator); pass false
        // here since the caller chain doesn't reach anyBusSoloed().  If this
        // path is ever revived, the caller must pass the real anyBusSoloed()
        // value.
        processChainOnly(buf, bpm, /*anyBusSoloed*/ false);
    }

    // 2026-05-06 (Batch 9b): runs the bus DSP chain on `buf` in-place.
    // `buf` must already contain the bus input (PassiveStripTask predecessor
    // sum of upstream Layer InsertNode outputs).  Steps: pre-rack EQ -> rack
    // (with bypass logic) -> post-rack EQ -> fader x mute x in-group solo ->
    // polarity + M/S width -> pan -> latency-compensated comp delay -> peak
    // meter publish.
    //
    // QA-Ea Part A (2026-05-21): anyBusSoloed parameter replaces the legacy
    // 3-bus (L/B/D-only) sibling solo formula.  Caller computes
    // VibeGraph::anyBusSoloed() once per block + passes the same value to
    // every bus's processChainOnly so the solo gate is uniform across all
    // 11 buses.  pSiblingBass / pSiblingDrum members are now dead state
    // (kept for minimal-churn until QA-Ef ST deletion clears the surrounding
    // code).
    void processChainOnly(juce::AudioBuffer<float>& buf, double bpm,
                          bool anyBusSoloed)
    {
        // §P4.3: pre-rack bus EQ (was historically removed from Layers; now
        // restored uniformly across every bus + insert).
        if (buf.getNumChannels() >= 2) preEq.process(buf);   // §P4.3 (identity short-circuit + spectrum feed inside process)
        rack.setHostBPM(bpm);
        // 2026-04-29: respect strip-local FX Bypass (mixer_layers_bypass)
        // OR-ed with global master_fx_bypass kill-all flag.
        {
            const bool stripBypass  = loadParam(pBypass, 0.f) > 0.5f;
            const bool globalBypass = loadParam(pGlobalFxBypass, 0.f) > 0.5f;
            const bool bypass       = stripBypass || globalBypass;
            if (rack.isRackBypassed() != bypass)
                rack.setRackBypassed(bypass);
        }
        rack.process(buf);
        if (buf.getNumChannels() >= 2) busEq.process(buf);   // post-rack bus EQ (identity short-circuit + spectrum feed inside)

        // 2026-04-30 (audit B.3): read fader/mute/solo from APVTS directly
        // so audio-thread automation lands at audio-block rate.  busMix
        // values stay in sync via the strip's onFaderChanged callbacks
        // for legacy code paths but are no longer the audio source of
        // truth for these three params.
        //
        // QA-Ea Part A (2026-05-21): anySolo now comes from the caller's
        // VibeGraph::anyBusSoloed() (all 11 buses) instead of the legacy
        // 3-bus L+B+D sibling sum.  Canonical formula:
        //   silenced = thisMuted || (anyBusSoloed && !thisSolo)
        // applied uniformly across every bus.  Per-strip _solo is a separate
        // axis and is untouched (GUARDRAIL).
        const bool  thisSolo  = loadParam(pSolo, 0.f) > 0.5f;
        const bool  thisMuted = loadParam(pMute, 0.f) > 0.5f;
        const float fadDb     = loadParam(pLevel, 0.f);
        const float fadLin    = juce::Decibels::decibelsToGain(fadDb, -60.f);
        const float g = (thisMuted || (anyBusSoloed && ! thisSolo)) ? 0.f : fadLin;
        if (g != 1.f) buf.applyGain(g);

        // 5F-4a Batch 6: polarity + M/S width
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

        // 2026-04-29: pan applied AFTER fader + width using project-level law.
        applyStereoPan (buf, loadParam(pPan, 0.f), (int) loadParam(pPanLaw, 0.f));

        compDelay.process(buf);
        // Peak meter with hold+decay - matches InsertNode pattern so transient
        // bus peaks aren't missed between ~30 Hz UI polls.
        {
            // 2026-05-02: lock-free max + latency-compensated publish.  The UI
            // exchanges the running-max atomic with -inf on each vsync to start
            // a fresh "max within frame" window; this CAS-loop adds the current
            // block (or, when latency comp is on, an N-block-old block) to the
            // running max safely across that swap.  No more audio-side decay
            // -- ballistics live entirely on the UI thread now.
            publishPeakReading (buf, peakRingL, peakRingR, peakRingIdx,
                                peakDbL, peakDbR, peakDb);
        }
    }
};

// ── BassBusNode ───────────────────────────────────────────────────────────────
struct VibeGraph::BassBusNode
{
    EQ8MsDSP               preEq;    // §P4.3 pre-rack
    EffectRack             rack;
    EQ8MsDSP               busEq;
    CompDelayLine          compDelay;
    std::atomic<float>     peakDb  { -60.f };
    // 2026-04-30: stereo L/R peakDb for the new split DBFSMeter.  Mono
    // peakDb (above) kept for back-compat; written as max(L, R) each block.
    std::atomic<float>     peakDbL { -60.f };
    std::atomic<float>     peakDbR { -60.f };
    // Per-node peak ring for latency-compensated meter publish (2026-05-02).
    // 16 entries = up to ~85 ms of compensation at 256-sample / 48 kHz.
    std::array<float, MeterLatencyComp::kRingSize> peakRingL {}, peakRingR {};
    int                    peakRingIdx { 0 };
    BassSynth&               bass;
    VibeGraph::BusMix&       busMix;
    // 12i: spectrum feeds now live inside each EQ8MsDSP.
    // 2026-04-19 cleanup: pageEq reference removed - Bass bus pre-rack EQ deleted
    // (no UI). Signal flow on Bass: bass-synth -> rack -> busEq -> fader
    // (per-bass pre-rack EQ applied earlier in the per-bass scratch).

    // 5F-4a Batch 6: APVTS pointers for polarity + width
    std::atomic<float>* pPolarity { nullptr };
    std::atomic<float>* pWidth    { nullptr };
    // 2026-04-29: strip-local FX Bypass + pan (see LayersBusNode for rationale).
    std::atomic<float>* pBypass   { nullptr };
    std::atomic<float>* pPan      { nullptr };
    std::atomic<float>* pPanLaw   { nullptr };
    // 2026-04-30 (audit B.3): direct APVTS reads - see LayersBusNode header.
    std::atomic<float>* pLevel         { nullptr };
    std::atomic<float>* pMute          { nullptr };
    std::atomic<float>* pSolo          { nullptr };
    std::atomic<float>* pSiblingLayers { nullptr };   // mixer_layers_solo
    std::atomic<float>* pSiblingDrum   { nullptr };   // mixer_drums_solo
    // Global "kill-all" FX bypass (master_fx_bypass) - forces rack bypass when true.
    std::atomic<float>* pGlobalFxBypass { nullptr };

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
        pSiblingLayers  = apvts.getRawParameterValue("mixer_layers_solo");
        pSiblingDrum    = apvts.getRawParameterValue("mixer_drums_solo");
        pGlobalFxBypass = apvts.getRawParameterValue("master_fx_bypass");
    }

    BassBusNode(BassSynth& b, VibeGraph::BusMix& m)
        : bass(b), busMix(m) {}

    void prepare(double sr, int blockSize)
    {
        preEq.prepare(sr, blockSize);   // §P4.3
        rack .prepare(sr, blockSize);
        busEq.prepare(sr, blockSize);
    }
    void reset()
    {
        preEq.reset();   // §P4.3
        rack .reset();
        busEq.reset();
    }

    void processBlock(juce::AudioBuffer<float>& buf, double bpm,
                      juce::AudioBuffer<float>* preRendered = nullptr)
    {
        const int n = buf.getNumSamples();
        buf.clear();
        if (preRendered && preRendered->getNumSamples() == n)
        {
            const int srcCh = juce::jmin(buf.getNumChannels(), preRendered->getNumChannels());
            for (int c = 0; c < srcCh; ++c)
                buf.addFrom(c, 0, *preRendered, c, 0, n);
        }
        else
        {
            bass.renderNextBlock(buf, 0, n);
        }
        // 2026-05-06 (Batch 9b): post-render DSP factored into processChainOnly
        // so VibeGraph::processBus can run the chain on a buffer that already
        // contains the pre-summed input from PassiveStripTask.
        // QA-Ea Part A (2026-05-21): synth-fallback path is dead; pass false.
        processChainOnly(buf, bpm, /*anyBusSoloed*/ false);
    }

    // 2026-05-06 (Batch 9b): runs the bus DSP chain on `buf` in-place. See
    // LayersBusNode::processChainOnly for the per-step rationale; signal
    // path is identical.  QA-Ea Part A (2026-05-21): unified anyBusSoloed
    // parameter replaces the legacy L+B+D-sibling formula.
    void processChainOnly(juce::AudioBuffer<float>& buf, double bpm,
                          bool anyBusSoloed)
    {
        // §P4.3: pre-rack bus EQ.
        if (buf.getNumChannels() >= 2) preEq.process(buf);   // §P4.3 (identity short-circuit + spectrum feed inside process)
        rack.setHostBPM(bpm);
        // 2026-04-29: strip-local FX Bypass OR global master_fx_bypass.
        {
            const bool stripBypass  = loadParam(pBypass, 0.f) > 0.5f;
            const bool globalBypass = loadParam(pGlobalFxBypass, 0.f) > 0.5f;
            const bool bypass       = stripBypass || globalBypass;
            if (rack.isRackBypassed() != bypass)
                rack.setRackBypassed(bypass);
        }
        rack.process(buf);
        if (buf.getNumChannels() >= 2) busEq.process(buf);   // post-rack bus EQ (identity short-circuit + spectrum feed inside)

        // 2026-04-30 (audit B.3): direct APVTS read - see LayersBusNode.
        // QA-Ea Part A (2026-05-21): unified anyBusSoloed (11 buses) replaces
        // the legacy L+layer+drum sibling sum; canonical formula identical
        // to LayersBusNode + DrumsBusNode.
        const bool  thisSolo  = loadParam(pSolo, 0.f) > 0.5f;
        const bool  thisMuted = loadParam(pMute, 0.f) > 0.5f;
        const float fadDb     = loadParam(pLevel, 0.f);
        const float fadLin    = juce::Decibels::decibelsToGain(fadDb, -60.f);
        const float g = (thisMuted || (anyBusSoloed && ! thisSolo)) ? 0.f : fadLin;
        if (g != 1.f) buf.applyGain(g);

        // 5F-4a Batch 6: polarity + M/S width
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

        // 2026-04-29: pan applied AFTER fader + width using project-level law.
        applyStereoPan (buf, loadParam(pPan, 0.f), (int) loadParam(pPanLaw, 0.f));

        compDelay.process(buf);
        {
            // 2026-05-02: lock-free max + latency-compensated publish.  The UI
            // exchanges the running-max atomic with -inf on each vsync to start
            // a fresh "max within frame" window; this CAS-loop adds the current
            // block (or, when latency comp is on, an N-block-old block) to the
            // running max safely across that swap.  No more audio-side decay
            // -- ballistics live entirely on the UI thread now.
            publishPeakReading (buf, peakRingL, peakRingR, peakRingIdx,
                                peakDbL, peakDbR, peakDb);
        }
    }
};

// ── DrumsBusNode ──────────────────────────────────────────────────────────────
struct VibeGraph::DrumsBusNode
{
    EQ8MsDSP               preEq;    // §P4.3 pre-rack
    EffectRack             rack;
    EQ8MsDSP               busEq;
    CompDelayLine          compDelay;
    std::atomic<float>     peakDb  { -60.f };
    // 2026-04-30: stereo L/R peakDb for the new split DBFSMeter.  Mono
    // peakDb (above) kept for back-compat; written as max(L, R) each block.
    std::atomic<float>     peakDbL { -60.f };
    std::atomic<float>     peakDbR { -60.f };
    // Per-node peak ring for latency-compensated meter publish (2026-05-02).
    // 16 entries = up to ~85 ms of compensation at 256-sample / 48 kHz.
    std::array<float, MeterLatencyComp::kRingSize> peakRingL {}, peakRingR {};
    int                    peakRingIdx { 0 };
    // 2026-04-25: DrumSynth ref removed - drum bus now ALWAYS uses preRendered
    // path (per-drum-tab InsertNode outputs).  Silent fallback if no preRendered.
    VibeGraph::BusMix&       busMix;
    // 12i: spectrum feeds now live inside each EQ8MsDSP.

    // 5F-4a Batch 6: APVTS pointers for polarity + width
    std::atomic<float>* pPolarity { nullptr };
    std::atomic<float>* pWidth    { nullptr };
    // 2026-04-29: strip-local FX Bypass + pan (see LayersBusNode for rationale).
    std::atomic<float>* pBypass   { nullptr };
    std::atomic<float>* pPan      { nullptr };
    std::atomic<float>* pPanLaw   { nullptr };
    // 2026-04-30 (audit B.3): direct APVTS reads - see LayersBusNode header.
    std::atomic<float>* pLevel         { nullptr };
    std::atomic<float>* pMute          { nullptr };
    std::atomic<float>* pSolo          { nullptr };
    std::atomic<float>* pSiblingLayers { nullptr };   // mixer_layers_solo
    std::atomic<float>* pSiblingBass   { nullptr };   // mixer_bass_solo
    // Global "kill-all" FX bypass (master_fx_bypass) - forces rack bypass when true.
    std::atomic<float>* pGlobalFxBypass { nullptr };

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
        pSiblingLayers  = apvts.getRawParameterValue("mixer_layers_solo");
        pSiblingBass    = apvts.getRawParameterValue("mixer_bass_solo");
        pGlobalFxBypass = apvts.getRawParameterValue("master_fx_bypass");
    }

    DrumsBusNode(VibeGraph::BusMix& m)
        : busMix(m) {}

    void prepare(double sr, int blockSize)
    {
        preEq.prepare(sr, blockSize);   // §P4.3
        rack .prepare(sr, blockSize);
        busEq.prepare(sr, blockSize);
    }
    void reset()
    {
        preEq.reset();   // §P4.3
        rack .reset();
        busEq.reset();
    }

    void processBlock(juce::AudioBuffer<float>& buf, double bpm,
                      juce::AudioBuffer<float>* preRendered = nullptr)
    {
        const int n = buf.getNumSamples();
        buf.clear();
        if (preRendered && preRendered->getNumSamples() == n)
        {
            const int srcCh = juce::jmin(buf.getNumChannels(), preRendered->getNumChannels());
            for (int c = 0; c < srcCh; ++c)
                buf.addFrom(c, 0, *preRendered, c, 0, n);
        }
        // 2026-04-25: legacy DrumSynth fallback removed - buf stays cleared
        // when no preRendered (drum bus is silent until tabs route into it).
        // 2026-05-06 (Batch 9b): post-render DSP factored into processChainOnly
        // so VibeGraph::processBus can run the chain on a buffer that already
        // contains the pre-summed input from PassiveStripTask.
        // QA-Ea Part A (2026-05-21): pre-rendered-fallback path passes false.
        processChainOnly(buf, bpm, /*anyBusSoloed*/ false);
    }

    // 2026-05-06 (Batch 9b): runs the bus DSP chain on `buf` in-place.
    // See LayersBusNode::processChainOnly for per-step rationale; signal
    // path is identical.  QA-Ea Part A (2026-05-21): unified anyBusSoloed
    // parameter replaces the legacy L+B+D-sibling formula.
    void processChainOnly(juce::AudioBuffer<float>& buf, double bpm,
                          bool anyBusSoloed)
    {
        // §P4.3 pre-rack EQ (legacy pageEq retired in B7).
        if (buf.getNumChannels() >= 2) preEq.process(buf);   // identity short-circuit + spectrum feed inside process
        rack.setHostBPM(bpm);
        // 2026-04-29: strip-local FX Bypass OR global master_fx_bypass.
        {
            const bool stripBypass  = loadParam(pBypass, 0.f) > 0.5f;
            const bool globalBypass = loadParam(pGlobalFxBypass, 0.f) > 0.5f;
            const bool bypass       = stripBypass || globalBypass;
            if (rack.isRackBypassed() != bypass)
                rack.setRackBypassed(bypass);
        }
        rack.process(buf);
        if (buf.getNumChannels() >= 2) busEq.process(buf);   // post-rack bus EQ (identity short-circuit + spectrum feed inside)

        // 2026-04-30 (audit B.3): direct APVTS read - see LayersBusNode.
        // QA-Ea Part A (2026-05-21): unified anyBusSoloed (11 buses) replaces
        // the legacy L+layer+bass sibling sum; canonical formula identical
        // to LayersBusNode + BassBusNode.
        const bool  thisSolo  = loadParam(pSolo, 0.f) > 0.5f;
        const bool  thisMuted = loadParam(pMute, 0.f) > 0.5f;
        const float fadDb     = loadParam(pLevel, 0.f);
        const float fadLin    = juce::Decibels::decibelsToGain(fadDb, -60.f);
        const float g = (thisMuted || (anyBusSoloed && ! thisSolo)) ? 0.f : fadLin;
        if (g != 1.f) buf.applyGain(g);

        // 5F-4a Batch 6: polarity + M/S width
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

        // 2026-04-29: pan applied AFTER fader + width using project-level law.
        applyStereoPan (buf, loadParam(pPan, 0.f), (int) loadParam(pPanLaw, 0.f));

        compDelay.process(buf);
        {
            // 2026-05-02: lock-free max + latency-compensated publish.  The UI
            // exchanges the running-max atomic with -inf on each vsync to start
            // a fresh "max within frame" window; this CAS-loop adds the current
            // block (or, when latency comp is on, an N-block-old block) to the
            // running max safely across that swap.  No more audio-side decay
            // -- ballistics live entirely on the UI thread now.
            publishPeakReading (buf, peakRingL, peakRingR, peakRingIdx,
                                peakDbL, peakDbR, peakDb);
        }
    }
};

// ── MasterBusNode ─────────────────────────────────────────────────────────────
// Receives pre-summed audio.  Runs master EffectRack, then applies
// masterGain (APVTS knob) × masterFader (Mixer master strip).
struct VibeGraph::MasterBusNode
{
    EQ8MsDSP               preEq;    // §P4.3 pre-rack
    EffectRack             rack;
    EQ8MsDSP               busEq;    // post-rack master EQ - shown on Effects Page
    std::atomic<float>     peakDb  { -60.f };
    // 2026-04-30: stereo L/R peakDb for the new split DBFSMeter.  Mono
    // peakDb (above) kept for back-compat; written as max(L, R) each block.
    std::atomic<float>     peakDbL { -60.f };
    std::atomic<float>     peakDbR { -60.f };
    // Per-node peak ring for latency-compensated meter publish (2026-05-02).
    // 16 entries = up to ~85 ms of compensation at 256-sample / 48 kHz.
    std::array<float, MeterLatencyComp::kRingSize> peakRingL {}, peakRingR {};
    int                    peakRingIdx { 0 };
    juce::AudioProcessorValueTreeState& apvts;
    VibeGraph::BusMix&                  busMix;

    // 5F-4a Batch 6: APVTS width pointer (master has no polarity)
    std::atomic<float>* pWidth { nullptr };
    // Global "kill-all" FX bypass (master_fx_bypass).
    std::atomic<float>* pGlobalFxBypass { nullptr };
    // 2026-04-29: master mute (was previously only wired to UI; clicking it
    // updated APVTS but no audio code read the param so master mute did
    // nothing).
    std::atomic<float>* pMute  { nullptr };
    // 2026-04-29: master strip's own FX Bypass (mixer_master_bypass) - bypasses
    // ONLY the master rack.  Distinct from master_fx_bypass which bypasses
    // every rack on every strip globally.  The two are OR-ed together so
    // either one being on bypasses the master rack.
    std::atomic<float>* pBypass { nullptr };
    // 2026-04-29: master pan + project-level pan law.
    std::atomic<float>* pPan    { nullptr };
    std::atomic<float>* pPanLaw { nullptr };
    // 2026-04-30 (audit B.3): mixer_master_level direct read - was relayed
    // through busMix.masterFader (PatternManager) on a 30 Hz UI timer.
    // Now audio thread reads APVTS directly each block.
    std::atomic<float>* pLevel  { nullptr };

    static float loadParam(const std::atomic<float>* p, float fallback) noexcept
    {
        return p ? p->load(std::memory_order_relaxed) : fallback;
    }
    void rebindApvts(juce::AudioProcessorValueTreeState& a, const juce::String& prefix)
    {
        pWidth          = a.getRawParameterValue(prefix + "_width");
        pGlobalFxBypass = a.getRawParameterValue("master_fx_bypass");
        pMute           = a.getRawParameterValue(prefix + "_mute");
        pBypass         = a.getRawParameterValue(prefix + "_bypass");
        pPan            = a.getRawParameterValue(prefix + "_pan");
        pPanLaw         = a.getRawParameterValue("master_pan_law");
        pLevel          = a.getRawParameterValue(prefix + "_level");
    }

    MasterBusNode(juce::AudioProcessorValueTreeState& a, VibeGraph::BusMix& m)
        : apvts(a), busMix(m) {}

    void prepare(double sr, int blockSize)
    {
        preEq.prepare(sr, blockSize);   // §P4.3
        rack .prepare(sr, blockSize);
        busEq.prepare(sr, blockSize);
    }
    void reset()
    {
        preEq.reset();   // §P4.3
        rack .reset();
        busEq.reset();
    }

    void processBlock(juce::AudioBuffer<float>& buf, double bpm)
    {
        // §P4.3 pre-rack master EQ.
        if (buf.getNumChannels() >= 2) preEq.process(buf);   // §P4.3 (identity short-circuit + spectrum feed inside process)
        rack.setHostBPM(bpm);
        // 2026-04-29: master rack bypass = strip-local OR global kill-all.
        // mixer_master_bypass bypasses the master rack only; master_fx_bypass
        // also bypasses every other strip's rack downstream.
        {
            const bool stripBypass  = loadParam(pBypass, 0.f) > 0.5f;
            const bool globalBypass = loadParam(pGlobalFxBypass, 0.f) > 0.5f;
            const bool bypass       = stripBypass || globalBypass;
            if (rack.isRackBypassed() != bypass)
                rack.setRackBypassed(bypass);
        }
        rack.process(buf);
        if (buf.getNumChannels() >= 2) busEq.process(buf);   // post-rack master EQ

        float masterGain = 1.f;
        if (auto* p = apvts.getRawParameterValue("masterGain"))
            masterGain = p->load();

        // 2026-04-29: master mute (mixer_master_mute APVTS toggle).  Multiplies
        // gain by zero when set - master output silenced regardless of bus
        // contributions.  Read via cached pMute pointer (rebindApvts).
        // 2026-04-30 (audit B.3): mixer_master_level read direct from APVTS
        // instead of busMix.masterFader (was 30 Hz UI relay).
        const bool  masterMuted = loadParam(pMute, 0.f) > 0.5f;
        const float masterFadDb = loadParam(pLevel, 0.f);
        const float masterFadLn = juce::Decibels::decibelsToGain(masterFadDb, -60.f);
        float g = masterMuted ? 0.f : (masterGain * masterFadLn);
        if (g != 1.f) buf.applyGain(g);

        // 2026-04-29: master pan applied AFTER master fader using project-level law.
        applyStereoPan (buf, loadParam(pPan, 0.f), (int) loadParam(pPanLaw, 0.f));

        // 5F-4a Batch 6: master M/S width (no polarity on master)
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

        {
            // 2026-05-02: lock-free max + latency-compensated publish.  The UI
            // exchanges the running-max atomic with -inf on each vsync to start
            // a fresh "max within frame" window; this CAS-loop adds the current
            // block (or, when latency comp is on, an N-block-old block) to the
            // running max safely across that swap.  No more audio-side decay
            // -- ballistics live entirely on the UI thread now.
            publishPeakReading (buf, peakRingL, peakRingR, peakRingIdx,
                                peakDbL, peakDbR, peakDb);
        }
    }
};

// ── EffectsBusNode ────────────────────────────────────────────────────────────
// FX Bus - receive bus for aux strip output (default destination), and any
// other strip whose user-cable points here.  Pipeline matches the Vox/Inst
// bus loop in PluginProcessor (preEq -> rack -> postEq -> polarity -> width
// -> fader x mute x solo -> pan -> peak).  Driven each block by
// VibeGraph::processEffectsBus, called from PluginProcessor after the
// Vox/Inst bus loop so every upstream send has fanned in by then.  Output
// fans downstream via the routing graph (default _sendTo = Master).
// Pre-C.1 (2026-04-30) this struct existed but its processBlock was never
// invoked -- the FX Bus was completely silent.
struct VibeGraph::EffectsBusNode
{
    EQ8MsDSP           preEq;    // §P4.3 pre-rack
    EffectRack         rack;
    EQ8MsDSP           busEq;    // post-rack Effects Bus EQ - shown on Effects Page
    std::atomic<float> peakDb  { -60.f };
    // 2026-04-30: stereo L/R peakDb for split DBFSMeter, written each block
    // inside processBlock once the FX Bus pipeline runs.  Mirrored into
    // PluginProcessor's mFxBusPeakDb* atomics so MixerPage can read alongside
    // its peers.
    std::atomic<float> peakDbL { -60.f };
    std::atomic<float> peakDbR { -60.f };
    // Per-node peak ring for latency-compensated meter publish (2026-05-02).
    std::array<float, MeterLatencyComp::kRingSize> peakRingL {}, peakRingR {};
    int                peakRingIdx { 0 };
    // 5F-4a Batch 6: APVTS pointers for polarity + width
    std::atomic<float>* pPolarity { nullptr };
    std::atomic<float>* pWidth    { nullptr };
    // Global "kill-all" FX bypass (master_fx_bypass) - forces rack bypass when true.
    std::atomic<float>* pGlobalFxBypass { nullptr };
    // C.1 (2026-04-30): full strip param set so FX Bus participates in the
    // mixer's fader / mute / solo / pan / strip-bypass behaviour.  Pre-C.1
    // these were registered + UI-bound but never read by audio.
    std::atomic<float>* pStripBypass { nullptr };
    std::atomic<float>* pLevel       { nullptr };
    std::atomic<float>* pPan         { nullptr };
    std::atomic<float>* pMute        { nullptr };
    std::atomic<float>* pSolo        { nullptr };

    static float loadParam(const std::atomic<float>* p, float fallback) noexcept
    {
        return p ? p->load(std::memory_order_relaxed) : fallback;
    }
    void rebindApvts(juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix)
    {
        pPolarity       = apvts.getRawParameterValue(prefix + "_polarity");
        pWidth          = apvts.getRawParameterValue(prefix + "_width");
        pGlobalFxBypass = apvts.getRawParameterValue("master_fx_bypass");
        pStripBypass    = apvts.getRawParameterValue(prefix + "_bypass");
        pLevel          = apvts.getRawParameterValue(prefix + "_level");
        pPan            = apvts.getRawParameterValue(prefix + "_pan");
        pMute           = apvts.getRawParameterValue(prefix + "_mute");
        pSolo           = apvts.getRawParameterValue(prefix + "_solo");
    }

    void prepare(double sr, int blockSize)
    {
        preEq.prepare(sr, blockSize);   // §P4.3
        rack .prepare(sr, blockSize);
        busEq.prepare(sr, blockSize);
    }
    void reset()
    {
        preEq.reset();   // §P4.3
        rack .reset();
        busEq.reset();
    }

    // C.1 (2026-04-30): full FX Bus pipeline - preEq → rack (with strip OR
    // global bypass) → postEq → polarity → M/S width → fader × mute × solo
    // → pan → peak meter.  busAnySolo participates in the receive-group solo
    // gate.  panLaw matches PluginProcessor's bus-loop convention (0=Circular,
    // 1=Triangular, 2=Square).
    void processBlock(juce::AudioBuffer<float>& buf, double bpm,
                       bool busAnySolo, int panLaw)
    {
        // §P4.3 pre-rack Effects Bus EQ.
        if (buf.getNumChannels() >= 2) preEq.process(buf);   // §P4.3 (identity short-circuit + spectrum feed inside process)
        rack.setHostBPM(bpm);
        // Strip-local FX Bypass OR global master_fx_bypass (kill-all flag).
        {
            const bool stripBypass  = loadParam(pStripBypass, 0.f) > 0.5f;
            const bool globalBypass = loadParam(pGlobalFxBypass, 0.f) > 0.5f;
            const bool bypass       = stripBypass || globalBypass;
            if (rack.isRackBypassed() != bypass)
                rack.setRackBypassed(bypass);
        }
        rack.process(buf);
        if (buf.getNumChannels() >= 2) busEq.process(buf);

        // 5F-4a Batch 6: polarity + M/S width
        if (loadParam(pPolarity, 0.f) > 0.5f) buf.applyGain(-1.f);
        const float width = loadParam(pWidth, 1.f);
        const int   n     = buf.getNumSamples();
        if (buf.getNumChannels() >= 2 && std::abs(width - 1.f) > 1.0e-4f)
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

        // Fader × mute × in-group solo.  Matches PluginProcessor BusSet pattern.
        const float dB     = loadParam(pLevel, 0.0f);
        const bool  muted  = loadParam(pMute, 0.f) > 0.5f;
        const bool  soloed = loadParam(pSolo, 0.f) > 0.5f;
        const bool  silenced = muted || (busAnySolo && ! soloed);
        const float gain   = silenced ? 0.0f : juce::Decibels::decibelsToGain(dB, -60.0f);
        if (gain != 1.0f) buf.applyGain(gain);

        // Pan (project-level law).  Inline copy of PluginProcessor's bus-loop pan
        // application so we don't reach into VibeGraph's private helper.
        const float pan = loadParam(pPan, 0.f);
        if (buf.getNumChannels() >= 2 && std::abs(pan) > 1.0e-4f)
        {
            float gL = 1.f, gR = 1.f;
            const float p  = juce::jlimit(-1.f, 1.f, pan);
            const float np = (p + 1.f) * 0.5f;
            switch (panLaw)
            {
                case 1: gL = 1.f - np;          gR = np;             break;
                case 2: gL = (p <= 0.f ? 1.f : 1.f - p);
                        gR = (p >= 0.f ? 1.f : 1.f + p);             break;
                default: { const float a = np * juce::MathConstants<float>::halfPi;
                           gL = std::cos(a); gR = std::sin(a); }     break;
            }
            buf.applyGain(0, 0, n, gL);
            buf.applyGain(1, 0, n, gR);
        }

        {
            // 2026-05-02: lock-free max + latency-compensated publish.  The UI
            // exchanges the running-max atomic with -inf on each vsync to start
            // a fresh "max within frame" window; this CAS-loop adds the current
            // block (or, when latency comp is on, an N-block-old block) to the
            // running max safely across that swap.  No more audio-side decay
            // -- ballistics live entirely on the UI thread now.
            publishPeakReading (buf, peakRingL, peakRingR, peakRingIdx,
                                peakDbL, peakDbR, peakDb);
        }
    }
};

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
        rack.setHostBPM(bpm);
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
struct VibeGraph::InstrChannelNode
{
    juce::String name;
    EQ8MsDSP     preEq;   // §P4.3 pre-rack (used by Audio Clips Bus; processed inline in PluginProcessor)
    EffectRack   rack;
    EQ8MsDSP     eq;

    // 5F-4a Batch 6: polarity + width pointers (used by audio clips bus node)
    std::atomic<float>* pPolarity { nullptr };
    std::atomic<float>* pWidth    { nullptr };

    // QA-Eg: G1-pattern peak fields (parallel to L/B/D/Master/EffectsBusNode).
    // publishPeakReading writes peakDb/L/R + ring; processBus exchange-stores
    // into VibeGraph member atomics; drainMeterAtomicsForUI G1 loop drains
    // those into PluginProcessor snapshot mirrors.  S6: NO peakDecayDbPerBlock
    // (dead carry-over on the 5 existing G1 nodes; stripped in Task 7).
    std::atomic<float> peakDb  { -60.f };
    std::atomic<float> peakDbL { -60.f };
    std::atomic<float> peakDbR { -60.f };
    std::array<float, MeterLatencyComp::kRingSize> peakRingL {}, peakRingR {};
    int peakRingIdx { 0 };

    explicit InstrChannelNode(const juce::String& displayName) : name(displayName) {}

    void prepare(double sr, int blockSize)
    {
        preEq.prepare(sr, blockSize);   // §P4.3
        rack.prepare(sr, blockSize);
        eq  .prepare(sr, blockSize);
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
        pPolarity = apvts.getRawParameterValue(prefix + "_polarity");
        pWidth    = apvts.getRawParameterValue(prefix + "_width");
    }

    // 5F-4a Batch 6: apply polarity flip + M/S width in-place on buf.
    // Called by VibeGraph::applyAudioClipsBusPolarityWidth on the audio thread.
    void applyPolarityWidth(juce::AudioBuffer<float>& buf)
    {
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
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
//  VibeGraph implementation
// ═══════════════════════════════════════════════════════════════════════════════

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
    // includes Vox / Inst / Rusty for symmetric end state.  NOTE: VibeGraph::
    // reset() itself is currently dead code -- no caller in the source tree;
    // future-batch routing via §9 Forks 2026-05-24 entry to wire it up to
    // transport Stop.
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
    mLayersNode     = std::make_unique<LayersBusNode> (synth, busMix);
    mBassNode       = std::make_unique<BassBusNode>   (bass,  busMix);
    mDrumsNode      = std::make_unique<DrumsBusNode>  (busMix);
    mMasterNode     = std::make_unique<MasterBusNode> (apvts, busMix);
    mEffectsBusNode = std::make_unique<EffectsBusNode>();


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
    mMasterNode->processBlock(sumBuf, bpm);
    // 2026-05-02: drain via exchange-with--inf so the node atomic's
    // running-max window resets per block.
    masterPeakDb .store(mMasterNode->peakDb .exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
    masterPeakDbL.store(mMasterNode->peakDbL.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
    masterPeakDbR.store(mMasterNode->peakDbR.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
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

    // Pre-existing helpers handle Master + FxBus.  These already push SC,
    // run their own DSP chain, and drain peaks; processBus just delegates.
    // QA-Ea Part A (2026-05-21): pass anyBus to processEffectsBus instead of
    // the now-dead caller anySolo so FxBus gates uniformly with the other 10.
    if (busChId == kMaster)  { processMasterBus(buf, bpm);                       return; }
    if (busChId == kFxBus)
    {
        processEffectsBus(buf, bpm, anyBus, panLaw);
        if (mEffectsBusNode != nullptr)
        {
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

    // Remaining buses (Clips / Vox / Inst / Vox2 / Inst2 / Inst3 / Rusty) all
    // share the same DSP shape, ported from PluginProcessor inline code:
    //   preEq -> rack (with bypass) -> postEq -> polarity/width -> fader x mute
    //   x in-group solo (where applicable) -> pan -> peak meter.
    // Per-bus state: prefix string + EQ/rack getters + polarity/width method
    // + in-group solo formula.
    if (mApvts == nullptr)        return;
    if (buf.getNumChannels() < 2) return;
    // QA-InsertMaps Task 5 close (2026-05-25): pre-existing unused `const int
    // n = buf.getNumSamples();` removed (C4189 cleanup; processBus uses buf
    // directly via preEq/rack/postEq/applyXxxPolarityWidth/applyStereoPan/
    // publishPeakReading and never needed n).

    EQ8MsDSP*  preEq  = nullptr;
    EffectRack* rack   = nullptr;
    EQ8MsDSP*  postEq = nullptr;
    InstrChannelNode* node = nullptr;
    juce::String prefix;
    // QA-RustyMeter part 2 (2026-05-30): per-bus split-meter RMS target atoms,
    // set alongside node in the switch below; publishRms CAS-maxes into them
    // after publishPeakReading (parallel to the peak exchange-store).
    std::atomic<float>* rmsL = nullptr;
    std::atomic<float>* rmsR = nullptr;
    // QA-Ea Part A (2026-05-21): inGroupSolo + useGroupSolo retired.  Every
    // bus shares the unified canonical formula `silenced = muted || (anyBus
    // && !soloed)`.  Previously: 6-bus subsets (different per-bus depending
    // on Clips override / Rusty standalone) excluded L/B/D + FX inconsistently
    // -- the root cause of "solo dead on 7 of 11 buses".  Now: anyBus is
    // computed once at top of processBus via anyBusSoloed() (all 11 buses).

    switch (busChId)
    {
        case kClipsBus:
            preEq  = getAudioClipsBusPreEQ();
            rack   = getAudioClipsBusRack();
            postEq = getAudioClipsBusEQ();
            node   = mAudioClipsBusNode.get();
            prefix = "mixer_clipsbus";
            rmsL = &audioClipsRmsDbL; rmsR = &audioClipsRmsDbR;
            break;
        case kVoxBus:
            preEq  = getVoxBusPreEQ();   rack = getVoxBusRack();   postEq = getVoxBusEQ();
            node   = mVoxBusNode.get();
            prefix = "mixer_voxbus";
            rmsL = &voxBusRmsDbL; rmsR = &voxBusRmsDbR;
            break;
        case kInstBus:
            preEq  = getInstBusPreEQ();  rack = getInstBusRack();  postEq = getInstBusEQ();
            node   = mInstBusNode.get();
            prefix = "mixer_instbus";
            rmsL = &instBusRmsDbL; rmsR = &instBusRmsDbR;
            break;
        case kVoxBus2:
            preEq  = getVoxBus2PreEQ();  rack = getVoxBus2Rack();  postEq = getVoxBus2EQ();
            node   = mVoxBus2Node.get();
            prefix = "mixer_voxbus2";
            rmsL = &voxBus2RmsDbL; rmsR = &voxBus2RmsDbR;
            break;
        case kInstBus2:
            preEq  = getInstBus2PreEQ(); rack = getInstBus2Rack(); postEq = getInstBus2EQ();
            node   = mInstBus2Node.get();
            prefix = "mixer_instbus2";
            rmsL = &instBus2RmsDbL; rmsR = &instBus2RmsDbR;
            break;
        case kInstBus3:
            preEq  = getInstBus3PreEQ(); rack = getInstBus3Rack(); postEq = getInstBus3EQ();
            node   = mInstBus3Node.get();
            prefix = "mixer_instbus3";
            rmsL = &instBus3RmsDbL; rmsR = &instBus3RmsDbR;
            break;
        case kRustyDrumsBus:
            preEq  = getRustyDrumsBusPreEQ(); rack = getRustyDrumsBusRack(); postEq = getRustyDrumsBusEQ();
            node   = mRustyDrumsBusNode.get();
            prefix = "mixer_rustybus";
            rmsL = &rustyDrumsBusRmsDbL; rmsR = &rustyDrumsBusRmsDbR;
            break;
        default:
            jassertfalse;
            return;
    }

    // Pre-rack EQ.
    if (preEq) preEq->process(buf);

    // Rack with bypass = strip-local OR global kill-all.
    if (rack)
    {
        const auto* bypP = mApvts->getRawParameterValue (prefix + "_bypass");
        const bool stripBypass  = bypP && bypP->load() > 0.5f;
        const bool globalBypass = (mGlobalFxBypassPtr != nullptr)
                                  && (mGlobalFxBypassPtr->load() > 0.5f);
        const bool bypass = stripBypass || globalBypass;
        if (rack->isRackBypassed() != bypass)
            rack->setRackBypassed(bypass);
        rack->process(buf);
    }

    // Post-rack EQ.
    if (postEq) postEq->process(buf);

    // Polarity + M/S width via the per-bus helper (so all knob bindings stay routed).
    switch (busChId)
    {
        case kClipsBus:        applyAudioClipsBusPolarityWidth(buf); break;
        case kVoxBus:          applyVoxBusPolarityWidth(buf);        break;
        case kInstBus:         applyInstBusPolarityWidth(buf);       break;
        case kVoxBus2:         applyVoxBus2PolarityWidth(buf);       break;
        case kInstBus2:        applyInstBus2PolarityWidth(buf);      break;
        case kInstBus3:        applyInstBus3PolarityWidth(buf);      break;
        case kRustyDrumsBus:   applyRustyDrumsBusPolarityWidth(buf); break;
        default: break;
    }

    // Fader * mute * unified bus-solo gate.
    // QA-Ea Part A (2026-05-21): canonical formula across every bus:
    //   silenced = muted || (anyBus && !soloed)
    // anyBus = VibeGraph::anyBusSoloed() (all 11 bus _solo params).
    // Identical formula to L/B/D BusNode::processChainOnly above.
    {
        const auto* lvlP   = mApvts->getRawParameterValue (prefix + "_level");
        const auto* muteP  = mApvts->getRawParameterValue (prefix + "_mute");
        const auto* soloP  = mApvts->getRawParameterValue (prefix + "_solo");
        const float dB     = lvlP ? lvlP->load() : 0.0f;
        const bool  muted  = muteP && muteP->load() > 0.5f;
        const bool  soloed = soloP && soloP->load() > 0.5f;
        const bool  silenced = muted || (anyBus && ! soloed);
        const float gain   = silenced ? 0.0f : juce::Decibels::decibelsToGain (dB, -60.0f);
        if (gain != 1.0f) buf.applyGain (gain);
    }

    // Pan - applyStereoPan is the file-static helper used by every BusNode.
    if (const auto* panP = mApvts->getRawParameterValue (prefix + "_pan"))
    {
        const float pan = panP->load();
        if (std::abs (pan) > 1.0e-4f)
            applyStereoPan (buf, pan, panLaw);
    }

    // Peak meter.  Each bus's InstrChannelNode (or BusNode for L/B/D/Master/FX)
    // owns the G1 peak fields; publishPeakReading writes them inside this
    // block, and the exchange-store at the end of this function lifts the
    // node-internal peak atomics into VibeGraph member atomics parallel to
    // the L/B/D pattern above.
    if (node != nullptr)
    {
        publishPeakReading (buf,
                            node->peakRingL, node->peakRingR, node->peakRingIdx,
                            node->peakDbL, node->peakDbR, node->peakDb);
        // QA-RustyMeter part 2 (2026-05-30): bus split-meter RMS feed.  rmsL/rmsR
        // were set alongside node in the switch above, so node != nullptr implies
        // they're valid for every generic bus.
        if (rmsL != nullptr && rmsR != nullptr)
            publishRms (buf, *rmsL, *rmsR);
    }

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

// C.1 (2026-04-30): public wrapper - drives the FX Bus pipeline once per
// block.  Called from PluginProcessor after the Vox/Inst bus loop, i.e.
// after every upstream send / aux / bus has had a chance to fan into the
// FX Bus accumulator.  Pre-C.1 this was never
// invoked anywhere - the accumulator filled but nothing read it back.
void VibeGraph::processEffectsBus(juce::AudioBuffer<float>& buf, double bpm,
                                   bool busAnySolo, int panLaw)
{
    if (mEffectsBusNode)
    {
        // C.4 Phase 1: push SC array to FX Bus chain before processing.
        pushScArrayToStrip(MixerChannelIds::kFxBus);
        mEffectsBusNode->processBlock(buf, bpm, busAnySolo, panLaw);
    }
}

EffectRack* VibeGraph::getAudioClipsBusRack() { return mAudioClipsBusNode ? &mAudioClipsBusNode->rack : nullptr; }
EffectRack* VibeGraph::getVoxBusRack()        { return mVoxBusNode        ? &mVoxBusNode       ->rack : nullptr; }
EffectRack* VibeGraph::getInstBusRack()       { return mInstBusNode       ? &mInstBusNode      ->rack : nullptr; }
EffectRack* VibeGraph::getVoxBus2Rack()       { return mVoxBus2Node       ? &mVoxBus2Node      ->rack : nullptr; }
EffectRack* VibeGraph::getInstBus2Rack()      { return mInstBus2Node      ? &mInstBus2Node     ->rack : nullptr; }
EffectRack* VibeGraph::getInstBus3Rack()      { return mInstBus3Node      ? &mInstBus3Node     ->rack : nullptr; }
EffectRack* VibeGraph::getRustyDrumsBusRack() { return mRustyDrumsBusNode ? &mRustyDrumsBusNode->rack : nullptr; }

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
int VibeGraph::updateBusLatencies()
{
    if (!mTopologyBuilt) return 0;

    const int ch = 2;

    // Per-bus latency = rack + post-rack bus EQ. Bus EQ latency was previously
    // omitted (assumed always 0); 5F-9 sec.12 12f added opt-in 2x oversampling
    // to EQ8DSP, so EQ8MsDSP::getLatencySamples() can now report a non-zero
    // value when AC is on. Sum both so PDC stays accurate.
    int layersLat  = mLayersNode    ->rack.getTotalLatencySamples()
                   + mLayersNode    ->busEq.getLatencySamples();
    int bassLat    = mBassNode      ->rack.getTotalLatencySamples()
                   + mBassNode      ->busEq.getLatencySamples();
    int drumsLat   = mDrumsNode     ->rack.getTotalLatencySamples()
                   + mDrumsNode     ->busEq.getLatencySamples();
    int masterLat  = mMasterNode    ->rack.getTotalLatencySamples()
                   + mMasterNode    ->busEq.getLatencySamples();

    // Each source bus is compensated to the longest among them before the master sum.
    int maxBusLat = juce::jmax(layersLat, bassLat, drumsLat);

    mLayersNode->compDelay.setDelay(maxBusLat - layersLat, ch);
    mBassNode  ->compDelay.setDelay(maxBusLat - bassLat,   ch);
    mDrumsNode ->compDelay.setDelay(maxBusLat - drumsLat,  ch);

    // Total latency seen by the host = aligned source buses + master rack
    int total = maxBusLat + masterLat;
    totalLatencySamples.store(total, std::memory_order_relaxed);
    return total;
}

// ── EQ getters (post-rack bus EQs, one per channel) ──────────────────────────
EQ8MsDSP* VibeGraph::getLayersBusEQ()     { return mLayersNode       ? &mLayersNode      ->busEq : nullptr; }
EQ8MsDSP* VibeGraph::getBassBusEQ()       { return mBassNode         ? &mBassNode        ->busEq : nullptr; }
EQ8MsDSP* VibeGraph::getDrumsBusEQ()      { return mDrumsNode        ? &mDrumsNode       ->busEq : nullptr; }
EQ8MsDSP* VibeGraph::getMasterEQ()        { return mMasterNode       ? &mMasterNode      ->busEq : nullptr; }
EQ8MsDSP* VibeGraph::getEffectsBusEQ()    { return mEffectsBusNode   ? &mEffectsBusNode  ->busEq : nullptr; }
EQ8MsDSP* VibeGraph::getAudioClipsBusEQ() { return mAudioClipsBusNode ? &mAudioClipsBusNode->eq  : nullptr; }
EQ8MsDSP* VibeGraph::getVoxBusEQ()        { return mVoxBusNode        ? &mVoxBusNode       ->eq  : nullptr; }
EQ8MsDSP* VibeGraph::getInstBusEQ()       { return mInstBusNode       ? &mInstBusNode      ->eq  : nullptr; }
EQ8MsDSP* VibeGraph::getVoxBus2EQ()       { return mVoxBus2Node       ? &mVoxBus2Node      ->eq  : nullptr; }
EQ8MsDSP* VibeGraph::getInstBus2EQ()      { return mInstBus2Node      ? &mInstBus2Node     ->eq  : nullptr; }
EQ8MsDSP* VibeGraph::getInstBus3EQ()      { return mInstBus3Node      ? &mInstBus3Node     ->eq  : nullptr; }
EQ8MsDSP* VibeGraph::getRustyDrumsBusEQ() { return mRustyDrumsBusNode ? &mRustyDrumsBusNode->eq  : nullptr; }

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

    addNode("LayersBus",  mLayersNode    ->rack, mLayersNode    ->preEq, mLayersNode    ->busEq);
    addNode("BassBus",    mBassNode      ->rack, mBassNode      ->preEq, mBassNode      ->busEq);
    addNode("DrumsBus",   mDrumsNode     ->rack, mDrumsNode     ->preEq, mDrumsNode     ->busEq);
    addNode("Master",     mMasterNode    ->rack, mMasterNode    ->preEq, mMasterNode    ->busEq);
    addNode("EffectsBus", mEffectsBusNode->rack, mEffectsBusNode->preEq, mEffectsBusNode->busEq);

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

    restoreNode("LayersBus",  mLayersNode    ->rack, mLayersNode    ->preEq, mLayersNode    ->busEq);
    restoreNode("BassBus",    mBassNode      ->rack, mBassNode      ->preEq, mBassNode      ->busEq);
    restoreNode("DrumsBus",   mDrumsNode     ->rack, mDrumsNode     ->preEq, mDrumsNode     ->busEq);
    restoreNode("Master",     mMasterNode    ->rack, mMasterNode    ->preEq, mMasterNode    ->busEq);
    restoreNode("EffectsBus", mEffectsBusNode->rack, mEffectsBusNode->preEq, mEffectsBusNode->busEq);
    if (mAudioClipsBusNode) restoreNode ("ClipsBus", mAudioClipsBusNode->rack, mAudioClipsBusNode->preEq, mAudioClipsBusNode->eq);
    if (mVoxBusNode)        restoreNode ("VoxBus",   mVoxBusNode       ->rack, mVoxBusNode       ->preEq, mVoxBusNode       ->eq);
    if (mInstBusNode)       restoreNode ("InstBus",  mInstBusNode      ->rack, mInstBusNode      ->preEq, mInstBusNode      ->eq);
    if (mVoxBus2Node)       restoreNode ("VoxBus2",  mVoxBus2Node      ->rack, mVoxBus2Node      ->preEq, mVoxBus2Node      ->eq);
    if (mInstBus2Node)      restoreNode ("InstBus2", mInstBus2Node     ->rack, mInstBus2Node     ->preEq, mInstBus2Node     ->eq);
    if (mInstBus3Node)      restoreNode ("InstBus3", mInstBus3Node     ->rack, mInstBus3Node     ->preEq, mInstBus3Node     ->eq);
    if (mRustyDrumsBusNode) restoreNode ("RustyBus", mRustyDrumsBusNode->rack, mRustyDrumsBusNode->preEq, mRustyDrumsBusNode->eq);

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
        // public-member array for the (kind, index) slot.  CAS-max is required
        // because processInsert is called MULTIPLE TIMES PER BLOCK for some
        // InsertKinds: Audio (Flow A clip-engine in CompositeAudioInsertTask +
        // per-clip Flow B inside renderAudioClipsForRow), Vox + Inst (per-
        // FilePlay-player loop in VoxStripTask / InstStripTask).  A plain
        // store would overwrite each prior call's peak with the latest; CAS-
        // max preserves the maximum across all calls within a single audio
        // block.  drainMeterAtomicsForUI's per-kind G1 loop drains the
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
        }
    }
}

EffectRack* VibeGraph::getInsertRack(InsertKind kind, int index)
{
    if (auto* node = getInsertNode(kind, index))
        return &node->rack;
    return nullptr;
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
// publishRms in processBus/processEffectsBus); returns "max RMS since the last
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
    if (mMasterNode)        mMasterNode        ->rebindApvts(*mApvts, "mixer_master");

    // QA-Ea Part A (2026-05-21): cache bus _solo atomic pointers for the
    // anyBusSoloed() helper.  Order matches mBusSoloPtr[11] declaration in
    // VibeGraph.h.  CPU-safeguarding standing rule: cache the raw atomic
    // ptrs once + reuse, avoid 11 string-keyed getRawParameterValue lookups
    // per audio block.  Master is excluded -- it has no _solo param + no
    // sibling to solo against.
    static constexpr const char* kBusSoloPrefixes[11] = {
        "mixer_layers", "mixer_bass", "mixer_drums", "mixer_fx", "mixer_clipsbus",
        "mixer_voxbus", "mixer_instbus", "mixer_voxbus2", "mixer_instbus2",
        "mixer_instbus3", "mixer_rustybus"
    };
    for (int i = 0; i < 11; ++i)
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

void VibeGraph::applyAudioClipsBusPolarityWidth(juce::AudioBuffer<float>& buf)
{
    if (mAudioClipsBusNode)
        mAudioClipsBusNode->applyPolarityWidth(buf);
}
void VibeGraph::applyVoxBusPolarityWidth(juce::AudioBuffer<float>& buf)
{
    if (mVoxBusNode) mVoxBusNode->applyPolarityWidth(buf);
}
void VibeGraph::applyInstBusPolarityWidth(juce::AudioBuffer<float>& buf)
{
    if (mInstBusNode) mInstBusNode->applyPolarityWidth(buf);
}
// G-6 (2026-04-29): polarity/width application for secondary buses.
void VibeGraph::applyVoxBus2PolarityWidth(juce::AudioBuffer<float>& buf)
{
    if (mVoxBus2Node) mVoxBus2Node->applyPolarityWidth(buf);
}
void VibeGraph::applyInstBus2PolarityWidth(juce::AudioBuffer<float>& buf)
{
    if (mInstBus2Node) mInstBus2Node->applyPolarityWidth(buf);
}
void VibeGraph::applyInstBus3PolarityWidth(juce::AudioBuffer<float>& buf)
{
    if (mInstBus3Node) mInstBus3Node->applyPolarityWidth(buf);
}
// J-4 (2026-05-03): polarity/width for the BaySickRustyDrums bus.
void VibeGraph::applyRustyDrumsBusPolarityWidth(juce::AudioBuffer<float>& buf)
{
    if (mRustyDrumsBusNode) mRustyDrumsBusNode->applyPolarityWidth(buf);
}

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
// processBlock / processEffectsBus do this internally for the strips they
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
