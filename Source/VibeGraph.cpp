#include "VibeGraph.h"
#include "BassSynth.h"
// DrumSynth.h removed from graph (2026-04-25) — no longer references the class.
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

// ═══════════════════════════════════════════════════════════════════════════════
//  Bus node struct definitions
//  These are nested inside VibeGraph (forward-declared in VibeGraph.h).
//  Each node:
//    • holds non-owning refs to the shared engine and EQ DSP
//    • owns its EffectRack (6 hot-swap slots)
//    • writes a post-processing peak dB to an atomic for the UI meter
// ═══════════════════════════════════════════════════════════════════════════════

// ── Shared solo/mute gain helper ──────────────────────────────────────────────
static float calcBusGain(float gain, bool muted, bool soloed, bool anySolo) noexcept
{
    if (muted || (anySolo && !soloed)) return 0.f;
    return gain;
}

static float bufferPeakDb(const juce::AudioBuffer<float>& buf) noexcept
{
    float peak = buf.getMagnitude(0, buf.getNumSamples());
    return juce::Decibels::gainToDecibels(peak, -60.f);
}

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

// ── Pan law helper (2026-04-29) ──────────────────────────────────────────────
// FL Studio parity.  Maps pan ∈ [-1, +1] + project-level law selector
// (master_pan_law: 0=Circular / 1=Triangular / 2=Square) → (gainL, gainR).
//   Circular   — constant power, -3 dB at center (cos/sin curve, FL default)
//   Triangular — linear, -6 dB at center
//   Square     — 0 dB at center (only attenuates the opposite side)
// Applied per-strip after the fader-gain stage in every Insert/Bus/Master node.
static void applyPanLaw (float pan, int law, float& gainL, float& gainR) noexcept
{
    pan = juce::jlimit (-1.f, 1.f, pan);
    const float normPan = (pan + 1.f) * 0.5f;   // 0 = full L, 0.5 = center, 1 = full R
    switch (law)
    {
        case 1: // Triangular — linear, -6 dB at center
            gainL = 1.f - normPan;
            gainR = normPan;
            break;
        case 2: // Square — 0 dB at center, only attenuates the opposite side
            gainL = (pan <= 0.f) ? 1.f : (1.f - pan);
            gainR = (pan >= 0.f) ? 1.f : (1.f + pan);
            break;
        default: // 0 = Circular — constant power, -3 dB at center (FL default)
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
    EQ8MsDSP               busEq;    // post-rack bus EQ — shown on Effects Page
    CompDelayLine          compDelay;
    std::atomic<float>     peakDb  { -60.f };
    // 2026-04-30: stereo L/R peakDb for the new split DBFSMeter.  Mono
    // peakDb (above) kept for back-compat; written as max(L, R) each block.
    std::atomic<float>     peakDbL { -60.f };
    std::atomic<float>     peakDbR { -60.f };
    // Peak-hold decay (set in prepare) — see InsertNode for rationale.
    float                  peakDecayDbPerBlock { 0.35f };

    juce::Synthesiser&       synth;
    VibeGraph::BusMix&       busMix;
    // 12i: spectrum feeds now live inside each EQ8MsDSP (preFeed/postFeed members).
    // 2026-04-19 cleanup: pageEq reference removed - Layers bus pre-rack EQ had no
    // UI and was deleted. Signal flow on Layers is now: summed-layers -> rack ->
    // busEq -> fader (per-layer pre-rack EQ applied earlier in the per-layer scratch).

    // 5F-4a Batch 6: APVTS pointers for polarity + width
    std::atomic<float>* pPolarity { nullptr };
    std::atomic<float>* pWidth    { nullptr };
    // 2026-04-29: strip-local FX Bypass (mixer_layers_bypass) — bypasses THIS
    // bus's rack only.  Global kill-all (master_fx_bypass) bypasses every
    // rack on every strip; the two are OR-ed together each block.
    std::atomic<float>* pBypass   { nullptr };
    // 2026-04-29: pan (mixer_layers_pan) — applied L/R after fader.
    std::atomic<float>* pPan      { nullptr };
    std::atomic<float>* pPanLaw   { nullptr };
    // 2026-04-30 (audit B.3): read fader / mute / solo direct from APVTS
    // instead of going through busMix (PatternManager) — audio-thread
    // automation now reaches the bus at audio-block rate instead of via
    // the UI's 30 Hz applicator timer.  Cross-bus anySolo computed by
    // reading sibling-bus solo params (2 extra atomic loads per block).
    std::atomic<float>* pLevel       { nullptr };
    std::atomic<float>* pMute        { nullptr };
    std::atomic<float>* pSolo        { nullptr };
    std::atomic<float>* pSiblingBass { nullptr };   // mixer_bass_solo
    std::atomic<float>* pSiblingDrum { nullptr };   // mixer_drums_solo
    // Global "kill-all" FX bypass (master_fx_bypass) — forces rack bypass when true.
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
        constexpr float kDecayDbPerSec = 30.0f;
        peakDecayDbPerBlock = kDecayDbPerSec * (float) blockSize
                              / (float) (sr > 0.0 ? sr : 44100.0);
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
        const bool  thisSolo  = loadParam(pSolo,        0.f) > 0.5f;
        const bool  bassSolo  = loadParam(pSiblingBass, 0.f) > 0.5f;
        const bool  drumSolo  = loadParam(pSiblingDrum, 0.f) > 0.5f;
        const bool  anySolo   = thisSolo || bassSolo || drumSolo;
        const bool  thisMuted = loadParam(pMute, 0.f) > 0.5f;
        const float fadDb     = loadParam(pLevel, 0.f);
        const float fadLin    = juce::Decibels::decibelsToGain(fadDb, -60.f);
        // calcBusGain returns 0 when muted or (anySolo && !soloed); else fadLin.
        const float g = (thisMuted || (anySolo && ! thisSolo)) ? 0.f : fadLin;
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
        // Peak meter with hold+decay — matches InsertNode pattern so transient
        // bus peaks aren't missed between ~30 Hz UI polls.
        {
            // 2026-04-30: stereo L/R peaks for the new split DBFSMeter.
            // Mono atomic kept (= max(L, R)) for legacy readers.
            const auto [thisL, thisR] = bufferPeakDbStereo(buf);
            const float prevL = peakDbL.load(std::memory_order_relaxed);
            const float prevR = peakDbR.load(std::memory_order_relaxed);
            const float decayedL = juce::jmax(-60.0f, prevL - peakDecayDbPerBlock);
            const float decayedR = juce::jmax(-60.0f, prevR - peakDecayDbPerBlock);
            const float newL = juce::jmax(thisL, decayedL);
            const float newR = juce::jmax(thisR, decayedR);
            peakDbL.store(newL, std::memory_order_relaxed);
            peakDbR.store(newR, std::memory_order_relaxed);
            peakDb .store(juce::jmax(newL, newR), std::memory_order_relaxed);
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
    float                  peakDecayDbPerBlock { 0.35f };

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
    // 2026-04-30 (audit B.3): direct APVTS reads — see LayersBusNode header.
    std::atomic<float>* pLevel         { nullptr };
    std::atomic<float>* pMute          { nullptr };
    std::atomic<float>* pSolo          { nullptr };
    std::atomic<float>* pSiblingLayers { nullptr };   // mixer_layers_solo
    std::atomic<float>* pSiblingDrum   { nullptr };   // mixer_drums_solo
    // Global "kill-all" FX bypass (master_fx_bypass) — forces rack bypass when true.
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
        constexpr float kDecayDbPerSec = 30.0f;
        peakDecayDbPerBlock = kDecayDbPerSec * (float) blockSize
                              / (float) (sr > 0.0 ? sr : 44100.0);
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

        // 2026-04-30 (audit B.3): direct APVTS read — see LayersBusNode.
        const bool  thisSolo  = loadParam(pSolo,           0.f) > 0.5f;
        const bool  layerSolo = loadParam(pSiblingLayers,  0.f) > 0.5f;
        const bool  drumSolo  = loadParam(pSiblingDrum,    0.f) > 0.5f;
        const bool  anySolo   = thisSolo || layerSolo || drumSolo;
        const bool  thisMuted = loadParam(pMute, 0.f) > 0.5f;
        const float fadDb     = loadParam(pLevel, 0.f);
        const float fadLin    = juce::Decibels::decibelsToGain(fadDb, -60.f);
        const float g = (thisMuted || (anySolo && ! thisSolo)) ? 0.f : fadLin;
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
            // 2026-04-30: stereo L/R peaks for the new split DBFSMeter.
            // Mono atomic kept (= max(L, R)) for legacy readers.
            const auto [thisL, thisR] = bufferPeakDbStereo(buf);
            const float prevL = peakDbL.load(std::memory_order_relaxed);
            const float prevR = peakDbR.load(std::memory_order_relaxed);
            const float decayedL = juce::jmax(-60.0f, prevL - peakDecayDbPerBlock);
            const float decayedR = juce::jmax(-60.0f, prevR - peakDecayDbPerBlock);
            const float newL = juce::jmax(thisL, decayedL);
            const float newR = juce::jmax(thisR, decayedR);
            peakDbL.store(newL, std::memory_order_relaxed);
            peakDbR.store(newR, std::memory_order_relaxed);
            peakDb .store(juce::jmax(newL, newR), std::memory_order_relaxed);
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
    float                  peakDecayDbPerBlock { 0.35f };

    // 2026-04-25: DrumSynth ref removed — drum bus now ALWAYS uses preRendered
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
    // 2026-04-30 (audit B.3): direct APVTS reads — see LayersBusNode header.
    std::atomic<float>* pLevel         { nullptr };
    std::atomic<float>* pMute          { nullptr };
    std::atomic<float>* pSolo          { nullptr };
    std::atomic<float>* pSiblingLayers { nullptr };   // mixer_layers_solo
    std::atomic<float>* pSiblingBass   { nullptr };   // mixer_bass_solo
    // Global "kill-all" FX bypass (master_fx_bypass) — forces rack bypass when true.
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
        constexpr float kDecayDbPerSec = 30.0f;
        peakDecayDbPerBlock = kDecayDbPerSec * (float) blockSize
                              / (float) (sr > 0.0 ? sr : 44100.0);
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
        // 2026-04-25: legacy DrumSynth fallback removed — buf stays cleared
        // when no preRendered (drum bus is silent until tabs route into it).
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

        // 2026-04-30 (audit B.3): direct APVTS read — see LayersBusNode.
        const bool  thisSolo  = loadParam(pSolo,           0.f) > 0.5f;
        const bool  layerSolo = loadParam(pSiblingLayers,  0.f) > 0.5f;
        const bool  bassSolo  = loadParam(pSiblingBass,    0.f) > 0.5f;
        const bool  anySolo   = thisSolo || layerSolo || bassSolo;
        const bool  thisMuted = loadParam(pMute, 0.f) > 0.5f;
        const float fadDb     = loadParam(pLevel, 0.f);
        const float fadLin    = juce::Decibels::decibelsToGain(fadDb, -60.f);
        const float g = (thisMuted || (anySolo && ! thisSolo)) ? 0.f : fadLin;
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
            // 2026-04-30: stereo L/R peaks for the new split DBFSMeter.
            // Mono atomic kept (= max(L, R)) for legacy readers.
            const auto [thisL, thisR] = bufferPeakDbStereo(buf);
            const float prevL = peakDbL.load(std::memory_order_relaxed);
            const float prevR = peakDbR.load(std::memory_order_relaxed);
            const float decayedL = juce::jmax(-60.0f, prevL - peakDecayDbPerBlock);
            const float decayedR = juce::jmax(-60.0f, prevR - peakDecayDbPerBlock);
            const float newL = juce::jmax(thisL, decayedL);
            const float newR = juce::jmax(thisR, decayedR);
            peakDbL.store(newL, std::memory_order_relaxed);
            peakDbR.store(newR, std::memory_order_relaxed);
            peakDb .store(juce::jmax(newL, newR), std::memory_order_relaxed);
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
    EQ8MsDSP               busEq;    // post-rack master EQ — shown on Effects Page
    std::atomic<float>     peakDb  { -60.f };
    // 2026-04-30: stereo L/R peakDb for the new split DBFSMeter.  Mono
    // peakDb (above) kept for back-compat; written as max(L, R) each block.
    std::atomic<float>     peakDbL { -60.f };
    std::atomic<float>     peakDbR { -60.f };
    float                  peakDecayDbPerBlock { 0.35f };

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
    // 2026-04-29: master strip's own FX Bypass (mixer_master_bypass) — bypasses
    // ONLY the master rack.  Distinct from master_fx_bypass which bypasses
    // every rack on every strip globally.  The two are OR-ed together so
    // either one being on bypasses the master rack.
    std::atomic<float>* pBypass { nullptr };
    // 2026-04-29: master pan + project-level pan law.
    std::atomic<float>* pPan    { nullptr };
    std::atomic<float>* pPanLaw { nullptr };
    // 2026-04-30 (audit B.3): mixer_master_level direct read — was relayed
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
        constexpr float kDecayDbPerSec = 30.0f;
        peakDecayDbPerBlock = kDecayDbPerSec * (float) blockSize
                              / (float) (sr > 0.0 ? sr : 44100.0);
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
        // gain by zero when set — master output silenced regardless of bus
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
            // 2026-04-30: stereo L/R peaks for the new split DBFSMeter.
            // Mono atomic kept (= max(L, R)) for legacy readers.
            const auto [thisL, thisR] = bufferPeakDbStereo(buf);
            const float prevL = peakDbL.load(std::memory_order_relaxed);
            const float prevR = peakDbR.load(std::memory_order_relaxed);
            const float decayedL = juce::jmax(-60.0f, prevL - peakDecayDbPerBlock);
            const float decayedR = juce::jmax(-60.0f, prevR - peakDecayDbPerBlock);
            const float newL = juce::jmax(thisL, decayedL);
            const float newR = juce::jmax(thisR, decayedR);
            peakDbL.store(newL, std::memory_order_relaxed);
            peakDbR.store(newR, std::memory_order_relaxed);
            peakDb .store(juce::jmax(newL, newR), std::memory_order_relaxed);
        }
    }
};

// ── EffectsBusNode ────────────────────────────────────────────────────────────
// Post-master parallel send.  Stub for Phase 1A — EffectRack is present and
// can be loaded via the Effects Page; audio routing is wired in Phase 1E.
struct VibeGraph::EffectsBusNode
{
    EQ8MsDSP           preEq;    // §P4.3 pre-rack
    EffectRack         rack;
    EQ8MsDSP           busEq;    // post-rack Effects Bus EQ — shown on Effects Page
    std::atomic<float> peakDb  { -60.f };
    // 2026-04-30: stereo L/R peakDb for split DBFSMeter (matches all other
    // bus nodes).  EffectsBusNode itself isn't currently driven by audio
    // (its processBlock is a stub), but the atomics are here so the meter
    // rebuild has consistent shape across every bus type.
    std::atomic<float> peakDbL { -60.f };
    std::atomic<float> peakDbR { -60.f };
    float              peakDecayDbPerBlock { 0.35f };

    // 5F-4a Batch 6: APVTS pointers for polarity + width
    std::atomic<float>* pPolarity { nullptr };
    std::atomic<float>* pWidth    { nullptr };
    // Global "kill-all" FX bypass (master_fx_bypass) — forces rack bypass when true.
    std::atomic<float>* pGlobalFxBypass { nullptr };

    static float loadParam(const std::atomic<float>* p, float fallback) noexcept
    {
        return p ? p->load(std::memory_order_relaxed) : fallback;
    }
    void rebindApvts(juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix)
    {
        pPolarity       = apvts.getRawParameterValue(prefix + "_polarity");
        pWidth          = apvts.getRawParameterValue(prefix + "_width");
        pGlobalFxBypass = apvts.getRawParameterValue("master_fx_bypass");
    }

    void prepare(double sr, int blockSize)
    {
        preEq.prepare(sr, blockSize);   // §P4.3
        rack .prepare(sr, blockSize);
        busEq.prepare(sr, blockSize);
        constexpr float kDecayDbPerSec = 30.0f;
        peakDecayDbPerBlock = kDecayDbPerSec * (float) blockSize
                              / (float) (sr > 0.0 ? sr : 44100.0);
    }
    void reset()
    {
        preEq.reset();   // §P4.3
        rack .reset();
        busEq.reset();
    }

    void processBlock(juce::AudioBuffer<float>& buf, double bpm)
    {
        // §P4.3 pre-rack Effects Bus EQ.
        if (buf.getNumChannels() >= 2) preEq.process(buf);   // §P4.3 (identity short-circuit + spectrum feed inside process)
        rack.setHostBPM(bpm);
        // Respect global master_fx_bypass (kill-all flag).
        {
            const bool globalBypass = loadParam(pGlobalFxBypass, 0.f) > 0.5f;
            if (rack.isRackBypassed() != globalBypass)
                rack.setRackBypassed(globalBypass);
        }
        rack.process(buf);
        if (buf.getNumChannels() >= 2) busEq.process(buf);

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

        {
            // 2026-04-30: stereo L/R peaks for the new split DBFSMeter.
            // Mono atomic kept (= max(L, R)) for legacy readers.
            const auto [thisL, thisR] = bufferPeakDbStereo(buf);
            const float prevL = peakDbL.load(std::memory_order_relaxed);
            const float prevR = peakDbR.load(std::memory_order_relaxed);
            const float decayedL = juce::jmax(-60.0f, prevL - peakDecayDbPerBlock);
            const float decayedR = juce::jmax(-60.0f, prevR - peakDecayDbPerBlock);
            const float newL = juce::jmax(thisL, decayedL);
            const float newR = juce::jmax(thisR, decayedR);
            peakDbL.store(newL, std::memory_order_relaxed);
            peakDbR.store(newR, std::memory_order_relaxed);
            peakDb .store(juce::jmax(newL, newR), std::memory_order_relaxed);
        }
    }
};

// ── InsertNode (5F-4a Batch 2 full implementation) ───────────────────────────
// Per-insert audio path: polarity → M/S width → rack (bypassable) → EQ →
// fader × mute × solo → PDC compensation → peak meter.
//
// APVTS access pattern: on creation, rebindApvts() caches raw param pointers
// (atomic<float>*). Audio thread reads via plain relaxed load — wait-free.
// Pointers may be null if the corresponding param doesn't exist (Master/Bus
// kinds lack some params); load() helper falls back to a sensible default.
//
// Batch 2 does NOT call processBlock() from anywhere. Batch 3 wires it in.
struct VibeGraph::InsertNode
{
    // ── Identity ──────────────────────────────────────────────────────────────
    juce::String          name;
    juce::String          apvtsPrefix;
    VibeGraph::InsertKind kind { VibeGraph::InsertKind::Layer };
    int                   index { 0 };

    // ── Audio DSP ─────────────────────────────────────────────────────────────
    // §P4.3: pre-rack EQ runs at the very start of the chain, before polarity /
    // width / rack / post-rack EQ.  Fresh EQ8MsDSP using the standard machinery
    // — same registration + APVTS sync as the post-rack `eq` (just under the
    // `_preeq_` prefix so they don't collide).  Bypass-flat by default so
    // existing kits sound identical until the user touches it.
    EQ8MsDSP              preEq;      // §P4.3 pre-rack
    EffectRack            rack;
    EQ8MsDSP              eq;         // post-rack
    CompDelayLine         compDelay;  // per-insert PDC
    // 2026-04-30: stereo L/R peak atomics for the strip's split DBFSMeter.
    // peakDb (mono = max(L, R)) is kept for back-compat with any reader
    // that hasn't switched to the stereo getters yet.
    std::atomic<float>    peakDb  { -60.f };
    std::atomic<float>    peakDbL { -60.f };
    std::atomic<float>    peakDbR { -60.f };
    // Peak-hold decay (set in prepare) so transient hits don't get missed
    // between UI polls (30 Hz) when audio runs at 86+ blocks/sec.
    float                 peakDecayDbPerBlock { 0.35f };

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
    // Global "kill-all" FX bypass (master_fx_bypass) — OR-ed into pBypass each block.
    std::atomic<float>* pGlobalFxBypass { nullptr };
    // 2026-04-29: project-level pan law selector (master_pan_law) — read
    // every block alongside pPan so the user can pick FL-style pan curve.
    std::atomic<float>* pPanLaw { nullptr };

    InsertNode(VibeGraph::InsertKind k, int i,
               juce::String displayName, juce::String prefix)
        : name(std::move(displayName))
        , apvtsPrefix(std::move(prefix))
        , kind(k), index(i) {}

    void prepare(double sr, int blockSize)
    {
        preEq.prepare(sr, blockSize);   // §P4.3
        rack .prepare(sr, blockSize);
        eq   .prepare(sr, blockSize);
        // ~30 dB/sec decay — standard peak-meter ballistics. Scaled to the
        // current block duration so decay feels consistent across SR/BS.
        constexpr float kDecayDbPerSec = 30.0f;
        peakDecayDbPerBlock = kDecayDbPerSec * (float) blockSize
                              / (float) (sr > 0.0 ? sr : 44100.0);
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

        // §P4.3 pre-rack EQ — first DSP stage, before polarity / width / rack.
        // (Identity short-circuit + spectrum feed live inside EQ8MsDSP::process.)
        if (nc >= 2) preEq.process(buf);

        // Polarity flip
        if (load(pPolarity, 0.f) > 0.5f)
            buf.applyGain(-1.f);

        // M/S stereo width — only meaningful on stereo and when width != 1.0
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

        // Rack (bypass synced from APVTS _bypass each block — canonical source).
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

        // Peak meter with hold+decay. Audio runs at ~86 blocks/sec, UI polls
        // at ~30 Hz — without hold, 2 in 3 blocks get missed and transient
        // peaks never reach the UI. max(thisBlockPeak, previousPeak - decay)
        // keeps the meter responsive to transients and decays smoothly over
        // ~1 second to the -60 dB floor.
        // 2026-04-30: stereo L/R peaks for the new split DBFSMeter.  Compute
        // per-channel peaks from the buffer; store mono (max of L+R) for
        // back-compat with legacy readers that haven't switched to the
        // stereo getters yet.
        {
            const auto [thisL, thisR] = bufferPeakDbStereo(buf);
            const float prevL = peakDbL.load(std::memory_order_relaxed);
            const float prevR = peakDbR.load(std::memory_order_relaxed);
            const float decayedL = juce::jmax(-60.0f, prevL - peakDecayDbPerBlock);
            const float decayedR = juce::jmax(-60.0f, prevR - peakDecayDbPerBlock);
            const float newL = juce::jmax(thisL, decayedL);
            const float newR = juce::jmax(thisR, decayedR);
            peakDbL.store(newL, std::memory_order_relaxed);
            peakDbR.store(newR, std::memory_order_relaxed);
            peakDb .store(juce::jmax(newL, newR), std::memory_order_relaxed);
        }
    }
};

// ── InstrChannelNode ──────────────────────────────────────────────────────────
// Generic rack + post-rack EQ container for non-bus mixer channels.
// One instance per entry in the dynamic instrument channel registry.
// Audio routing is wired in Phase 2/3 — containers are live from Phase 1.
struct VibeGraph::InstrChannelNode
{
    juce::String name;
    EQ8MsDSP     preEq;   // §P4.3 pre-rack (used by Audio Clips Bus; processed inline in PluginProcessor)
    EffectRack   rack;
    EQ8MsDSP     eq;

    // 5F-4a Batch 6: polarity + width pointers (used by audio clips bus node)
    std::atomic<float>* pPolarity { nullptr };
    std::atomic<float>* pWidth    { nullptr };

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

    // Per-page racks — always prepared regardless of topology state
    for (auto& r : mLayerPageRacks) r.prepare(sampleRate, maxBlockSize);
    for (auto& r : mBassPageRacks)  r.prepare(sampleRate, maxBlockSize);

    // Audio clips bus — always present, created once
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

    // G-6 (2026-04-29): secondary Vox/Inst buses — always allocated so audio
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

    if (!mTopologyBuilt) return;

    // Re-prepare all nodes (e.g. sample rate change)
    mLayersNode    ->prepare(sampleRate, maxBlockSize);
    mBassNode      ->prepare(sampleRate, maxBlockSize);
    mDrumsNode     ->prepare(sampleRate, maxBlockSize);
    mMasterNode    ->prepare(sampleRate, maxBlockSize);
    mEffectsBusNode->prepare(sampleRate, maxBlockSize);
    for (auto& [id, node] : mInstrChannelNodes)
        node->prepare(sampleRate, maxBlockSize);

    // 5F-4a: per-insert nodes — R3 includes Vox / Inst inserts.
    for (auto* m : { &mLayerInserts, &mBassInserts, &mDrumInserts, &mAudioInserts,
                     &mAuxInserts, &mVoxInserts, &mInstInserts })
        for (auto& [i, node] : *m)
            node->prepare(sampleRate, maxBlockSize);

    // Keep scratch buffers large enough
    const int ch = 2;
    if (mLayersBuf.getNumSamples() < maxBlockSize || mLayersBuf.getNumChannels() < ch)
    {
        mLayersBuf.setSize(ch, maxBlockSize, false, true, true);
        mBassBuf  .setSize(ch, maxBlockSize, false, true, true);
        mDrumsBuf .setSize(ch, maxBlockSize, false, true, true);
        mSumBuf   .setSize(ch, maxBlockSize, false, true, true);
    }

    // 5F-4b B1b: seed / resize accumulator buffers for master + 5 buses.
    // Insert accumulators allocate lazily on first getChannelAccumulator() call.
    using namespace MixerChannelIds;
    for (int id : { kMaster, kLayersBus, kBassBus, kDrumsBus, kFxBus, kClipsBus, kVoxBus, kInstBus })
    {
        auto& buf = mChannelAccum[id];
        if (buf.getNumSamples() < maxBlockSize || buf.getNumChannels() < ch)
            buf.setSize(ch, maxBlockSize, false, true, true);
    }
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
    if (!mTopologyBuilt) return;
    mLayersNode    ->reset();
    mBassNode      ->reset();
    mDrumsNode     ->reset();
    mMasterNode    ->reset();
    mEffectsBusNode->reset();
    for (auto& [id, node] : mInstrChannelNodes)
        node->reset();
    // 5F-4a: per-insert nodes
    for (auto* m : { &mLayerInserts, &mBassInserts, &mDrumInserts, &mAudioInserts, &mAuxInserts })
        for (auto& [i, node] : *m)
            node->reset();
}

// ── buildFixedTopology ────────────────────────────────────────────────────────
void VibeGraph::buildFixedTopology(juce::Synthesiser&                  synth,
                                    BassSynth&                          bass,
                                    juce::AudioProcessorValueTreeState& apvts)
{
    if (mTopologyBuilt) return;   // safe to call every prepareToPlay — no-op after first

    mApvts = &apvts;   // 5F-4a: captured for ensureInsertNode / rebindApvts
    // Cache global "kill-all" FX bypass pointer for every rack process site.
    mGlobalFxBypassPtr = apvts.getRawParameterValue("master_fx_bypass");

    // §P4.3 B7: every bus has its own preEq member — no external EQ refs needed.
    mLayersNode     = std::make_unique<LayersBusNode> (synth, busMix);
    mBassNode       = std::make_unique<BassBusNode>   (bass,  busMix);
    mDrumsNode      = std::make_unique<DrumsBusNode>  (busMix);
    mMasterNode     = std::make_unique<MasterBusNode> (apvts, busMix);
    mEffectsBusNode = std::make_unique<EffectsBusNode>();

    // Allocate stereo scratch buffers
    const int ch = 2;
    mLayersBuf.setSize(ch, mBlockSize, false, true, true);
    mBassBuf  .setSize(ch, mBlockSize, false, true, true);
    mDrumsBuf .setSize(ch, mBlockSize, false, true, true);
    mSumBuf   .setSize(ch, mBlockSize, false, true, true);

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

// ── processBlock ──────────────────────────────────────────────────────────────
void VibeGraph::processBlock(juce::AudioBuffer<float>& outputBuf,
                              juce::MidiBuffer&         midi,
                              double                    bpm,
                              juce::AudioBuffer<float>* layersPreRendered,
                              juce::AudioBuffer<float>* bassPreRendered,
                              juce::AudioBuffer<float>* drumsPreRendered,
                              juce::AudioBuffer<float>* audioClipsPreRendered)
{
    if (!mTopologyBuilt)
    {
        outputBuf.clear();
        return;
    }

    const int numSamples = outputBuf.getNumSamples();
    const int numCh      = juce::jmin(2, outputBuf.getNumChannels());

    // Grow scratch buffers if needed (rare — only on unexpected block-size increase)
    if (mLayersBuf.getNumSamples() < numSamples)
    {
        mLayersBuf.setSize(numCh, numSamples, false, true, true);
        mBassBuf  .setSize(numCh, numSamples, false, true, true);
        mDrumsBuf .setSize(numCh, numSamples, false, true, true);
        mSumBuf   .setSize(numCh, numSamples, false, true, true);
    }

    // Create zero-copy sub-view wrappers for the current block
    juce::AudioBuffer<float> layersBuf(mLayersBuf.getArrayOfWritePointers(), numCh, numSamples);
    juce::AudioBuffer<float> bassBuf  (mBassBuf  .getArrayOfWritePointers(), numCh, numSamples);
    juce::AudioBuffer<float> drumsBuf (mDrumsBuf .getArrayOfWritePointers(), numCh, numSamples);
    juce::AudioBuffer<float> sumBuf   (mSumBuf   .getArrayOfWritePointers(), numCh, numSamples);

    // ── Render each bus node ──────────────────────────────────────────────────
    mLayersNode->processBlock(layersBuf, midi, bpm, layersPreRendered);
    layersPeakDb .store(mLayersNode->peakDb .load(), std::memory_order_relaxed);
    layersPeakDbL.store(mLayersNode->peakDbL.load(), std::memory_order_relaxed);
    layersPeakDbR.store(mLayersNode->peakDbR.load(), std::memory_order_relaxed);

    mBassNode->processBlock(bassBuf, bpm, bassPreRendered);
    bassPeakDb .store(mBassNode->peakDb .load(), std::memory_order_relaxed);
    bassPeakDbL.store(mBassNode->peakDbL.load(), std::memory_order_relaxed);
    bassPeakDbR.store(mBassNode->peakDbR.load(), std::memory_order_relaxed);

    mDrumsNode->processBlock(drumsBuf, bpm, drumsPreRendered);
    drumsPeakDb .store(mDrumsNode->peakDb .load(), std::memory_order_relaxed);
    drumsPeakDbL.store(mDrumsNode->peakDbL.load(), std::memory_order_relaxed);
    drumsPeakDbR.store(mDrumsNode->peakDbR.load(), std::memory_order_relaxed);

    // ── Sum buses into master input ───────────────────────────────────────────
    sumBuf.clear();
    for (int c = 0; c < numCh; ++c)
    {
        sumBuf.addFrom(c, 0, layersBuf, c, 0, numSamples);
        sumBuf.addFrom(c, 0, bassBuf,   c, 0, numSamples);
        sumBuf.addFrom(c, 0, drumsBuf,  c, 0, numSamples);
    }
    // Audio clips bus (post-rack, post-fader) summed here so master rack sees it
    if (audioClipsPreRendered != nullptr)
    {
        const int srcCh = juce::jmin(numCh, audioClipsPreRendered->getNumChannels());
        for (int c = 0; c < numCh; ++c)
            sumBuf.addFrom(c, 0, *audioClipsPreRendered, c % srcCh, 0, numSamples);
    }

    // 5F-4b B1b: also sum any signals routed DIRECTLY to Master (insert → Master
    // main-cable drops, or sends targeting kMaster). Accumulator contents are
    // populated by PluginProcessor before this call via routeInsertOutput.
    if (auto* masterExtra = getChannelAccumulator(MixerChannelIds::kMaster))
    {
        for (int c = 0; c < numCh; ++c)
            sumBuf.addFrom(c, 0, *masterExtra, c, 0, numSamples);
    }

    // ── Master bus (rack + masterGain × masterFader) ──────────────────────────
    mMasterNode->processBlock(sumBuf, bpm);
    masterPeakDb .store(mMasterNode->peakDb .load(), std::memory_order_relaxed);
    masterPeakDbL.store(mMasterNode->peakDbL.load(), std::memory_order_relaxed);
    masterPeakDbR.store(mMasterNode->peakDbR.load(), std::memory_order_relaxed);

    // ── Write master output to the output buffer ──────────────────────────────
    outputBuf.clear();
    for (int c = 0; c < numCh; ++c)
        outputBuf.copyFrom(c, 0, sumBuf, c, 0, numSamples);
}

// ── EffectRack getters ────────────────────────────────────────────────────────
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

EffectRack* VibeGraph::getLayerPageRack(int idx)
{
    // 5F-4a Batch 6: prefer the InsertNode's rack; fall back to the legacy array
    if (auto it = mLayerInserts.find(idx); it != mLayerInserts.end())
        return &it->second->rack;
    if (idx < 0 || idx >= kMaxLayerPages) return nullptr;
    return &mLayerPageRacks[idx];
}
EffectRack* VibeGraph::getBassPageRack(int idx)
{
    if (auto it = mBassInserts.find(idx); it != mBassInserts.end())
        return &it->second->rack;
    if (idx < 0 || idx >= kMaxBassPages) return nullptr;
    return &mBassPageRacks[idx];
}

EffectRack* VibeGraph::getAuxRack(int idx)
{
    if (auto it = mAuxInserts.find(idx); it != mAuxInserts.end())
        return &it->second->rack;
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

// §P4.3: Pre-rack bus EQs (NEW — every bus gets one).
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

    auto addNode = [&](const juce::String& id, EffectRack& rack, EQ8MsDSP& eq)
    {
        juce::ValueTree node("BusRack");
        node.setProperty("id", id, nullptr);

        juce::MemoryBlock rackData, eqData;
        rack.getStateInformation(rackData);
        eq  .getStateInformation(eqData);
        node.setProperty("rack", encodeBlock(rackData), nullptr);
        node.setProperty("eq",   encodeBlock(eqData),   nullptr);

        parent.addChild(node, -1, nullptr);
    };

    addNode("LayersBus",  mLayersNode    ->rack, mLayersNode    ->busEq);
    addNode("BassBus",    mBassNode      ->rack, mBassNode      ->busEq);
    addNode("DrumsBus",   mDrumsNode     ->rack, mDrumsNode     ->busEq);
    addNode("Master",     mMasterNode    ->rack, mMasterNode    ->busEq);
    addNode("EffectsBus", mEffectsBusNode->rack, mEffectsBusNode->busEq);

    // 2026-04-24: the three special-bus InstrChannelNode racks (Audio Clips,
    // Vox, Inst) were silently dropped before - they're stored as separate
    // member pointers, not in mInstrChannelNodes, so the loop below missed
    // them.  Save each alongside the fixed buses.
    if (mAudioClipsBusNode) addNode ("ClipsBus", mAudioClipsBusNode->rack, mAudioClipsBusNode->eq);
    if (mVoxBusNode)        addNode ("VoxBus",   mVoxBusNode->rack,        mVoxBusNode->eq);
    if (mInstBusNode)       addNode ("InstBus",  mInstBusNode->rack,       mInstBusNode->eq);
    if (mVoxBus2Node)       addNode ("VoxBus2",  mVoxBus2Node->rack,       mVoxBus2Node->eq);
    if (mInstBus2Node)      addNode ("InstBus2", mInstBus2Node->rack,      mInstBus2Node->eq);
    if (mInstBus3Node)      addNode ("InstBus3", mInstBus3Node->rack,      mInstBus3Node->eq);

    for (int chId : mInstrChannelOrder)
    {
        auto it = mInstrChannelNodes.find(chId);
        if (it == mInstrChannelNodes.end()) continue;
        auto& ch = *it->second;

        juce::ValueTree node("InstrCh");
        node.setProperty("name", ch.name, nullptr);

        juce::MemoryBlock rackData, eqData;
        ch.rack.getStateInformation(rackData);
        ch.eq  .getStateInformation(eqData);
        node.setProperty("rack", encodeBlock(rackData), nullptr);
        node.setProperty("eq",   encodeBlock(eqData),   nullptr);

        parent.addChild(node, -1, nullptr);
    }

    // 2026-04-24: per-insert rack + post-rack EQ state.  Every Layer / Bass /
    // Drum / Audio / Aux / Vox / Inst insert has its own rack - before this,
    // user effect choices on any of those strips were lost on save.
    auto addInsertMap = [&](const char* kindStr,
                             const std::map<int, std::unique_ptr<InsertNode>>& m)
    {
        for (const auto& [idx, node] : m)
        {
            if (node == nullptr) continue;
            juce::ValueTree rec ("InsertRack");
            rec.setProperty ("kind",  kindStr, nullptr);
            rec.setProperty ("index", idx,     nullptr);
            juce::MemoryBlock rackData, eqData;
            node->rack.getStateInformation (rackData);
            node->eq  .getStateInformation (eqData);
            rec.setProperty ("rack", encodeBlock (rackData), nullptr);
            rec.setProperty ("eq",   encodeBlock (eqData),   nullptr);
            parent.addChild (rec, -1, nullptr);
        }
    };
    addInsertMap ("Layer", mLayerInserts);
    addInsertMap ("Bass",  mBassInserts);
    addInsertMap ("Drum",  mDrumInserts);
    addInsertMap ("Audio", mAudioInserts);
    addInsertMap ("Aux",   mAuxInserts);
    addInsertMap ("Vox",   mVoxInserts);
    addInsertMap ("Inst",  mInstInserts);
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

    for (auto& [id, node] : mInstrChannelNodes) if (node) wipe (node->rack);
    for (auto* m : { &mLayerInserts, &mBassInserts, &mDrumInserts,
                     &mAudioInserts, &mAuxInserts,   &mVoxInserts,  &mInstInserts })
        for (auto& [idx, node] : *m) if (node) wipe (node->rack);
}

void VibeGraph::loadRackStates(const juce::ValueTree& parent)
{
    if (!mTopologyBuilt)
    {
        // Topology not built yet — defer until end of buildFixedTopology.
        mPendingRackState = parent.createCopy();
        return;
    }
    applyRackStates(parent);
}

void VibeGraph::applyRackStates(const juce::ValueTree& parent)
{
    auto restoreNode = [&](const juce::String& id, EffectRack& rack, EQ8MsDSP& eq)
    {
        for (int i = 0; i < parent.getNumChildren(); ++i)
        {
            auto child = parent.getChild(i);
            if (!child.hasType("BusRack")) continue;
            if (child.getProperty("id").toString() != id) continue;

            juce::MemoryBlock rackData;
            if (decodeBlock(child.getProperty("rack").toString(), rackData))
                rack.setStateInformation(rackData.getData(), (int)rackData.getSize());

            juce::MemoryBlock eqData;
            if (decodeBlock(child.getProperty("eq").toString(), eqData))
                eq.setStateInformation(eqData.getData(), (int)eqData.getSize());

            return;
        }
    };

    restoreNode("LayersBus",  mLayersNode    ->rack, mLayersNode    ->busEq);
    restoreNode("BassBus",    mBassNode      ->rack, mBassNode      ->busEq);
    restoreNode("DrumsBus",   mDrumsNode     ->rack, mDrumsNode     ->busEq);
    restoreNode("Master",     mMasterNode    ->rack, mMasterNode    ->busEq);
    restoreNode("EffectsBus", mEffectsBusNode->rack, mEffectsBusNode->busEq);
    if (mAudioClipsBusNode) restoreNode ("ClipsBus", mAudioClipsBusNode->rack, mAudioClipsBusNode->eq);
    if (mVoxBusNode)        restoreNode ("VoxBus",   mVoxBusNode->rack,        mVoxBusNode->eq);
    if (mInstBusNode)       restoreNode ("InstBus",  mInstBusNode->rack,       mInstBusNode->eq);
    if (mVoxBus2Node)       restoreNode ("VoxBus2",  mVoxBus2Node->rack,       mVoxBus2Node->eq);
    if (mInstBus2Node)      restoreNode ("InstBus2", mInstBus2Node->rack,      mInstBus2Node->eq);
    if (mInstBus3Node)      restoreNode ("InstBus3", mInstBus3Node->rack,      mInstBus3Node->eq);

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

            juce::MemoryBlock eqData;
            if (decodeBlock(child.getProperty("eq").toString(), eqData))
                it->second->eq.setStateInformation(eqData.getData(), (int)eqData.getSize());

            break;
        }
    }

    // 2026-04-24: per-insert rack / EQ restore.  Match by kind string + index.
    auto restoreInsert = [&](const juce::String& kindStr,
                              std::map<int, std::unique_ptr<InsertNode>>& m)
    {
        for (int i = 0; i < parent.getNumChildren(); ++i)
        {
            auto child = parent.getChild (i);
            if (! child.hasType ("InsertRack")) continue;
            if (child.getProperty ("kind").toString() != kindStr) continue;
            const int idx = (int) child.getProperty ("index", -1);
            auto it = m.find (idx);
            if (it == m.end() || it->second == nullptr) continue;

            juce::MemoryBlock rackData;
            if (decodeBlock (child.getProperty ("rack").toString(), rackData))
                it->second->rack.setStateInformation (rackData.getData(), (int) rackData.getSize());

            juce::MemoryBlock eqData;
            if (decodeBlock (child.getProperty ("eq").toString(), eqData))
                it->second->eq.setStateInformation (eqData.getData(), (int) eqData.getSize());
        }
    };
    restoreInsert ("Layer", mLayerInserts);
    restoreInsert ("Bass",  mBassInserts);
    restoreInsert ("Drum",  mDrumInserts);
    restoreInsert ("Audio", mAudioInserts);
    restoreInsert ("Aux",   mAuxInserts);
    restoreInsert ("Vox",   mVoxInserts);
    restoreInsert ("Inst",  mInstInserts);
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
    // 5F-4a Batch 6: prefer Audio InsertNode; fall back to legacy InstrChannelNode
    if (auto it2 = mAudioInserts.find(row); it2 != mAudioInserts.end())
        return &it2->second->rack;
    auto it = mInstrChannelNodes.find(400 + row);
    return it != mInstrChannelNodes.end() ? &it->second->rack : nullptr;
}

EQ8MsDSP* VibeGraph::getAudioRowEQ(int row)
{
    // 5F-4a Batch 6 migration: prefer the Audio InsertNode's EQ; fall back to
    // legacy InstrChannelNode. Mirrors the getAudioRowRack dual-path pattern.
    // Previously legacy-only, which caused the audio-row EQ tab in EffectsPage
    // to bind null when the node lived in the new InsertNode registry.
    if (auto it2 = mAudioInserts.find(row); it2 != mAudioInserts.end())
        return &it2->second->eq;
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
// ═══════════════════════════════════════════════════════════════════════════════

namespace {
    std::map<int, std::unique_ptr<VibeGraph::InsertNode>>*
    selectInsertMap(VibeGraph::InsertKind kind,
                    std::map<int, std::unique_ptr<VibeGraph::InsertNode>>& layerMap,
                    std::map<int, std::unique_ptr<VibeGraph::InsertNode>>& bassMap,
                    std::map<int, std::unique_ptr<VibeGraph::InsertNode>>& drumMap,
                    std::map<int, std::unique_ptr<VibeGraph::InsertNode>>& audioMap,
                    std::map<int, std::unique_ptr<VibeGraph::InsertNode>>& auxMap,
                    std::map<int, std::unique_ptr<VibeGraph::InsertNode>>& voxMap,
                    std::map<int, std::unique_ptr<VibeGraph::InsertNode>>& instMap)
    {
        switch (kind)
        {
            case VibeGraph::InsertKind::Layer: return &layerMap;
            case VibeGraph::InsertKind::Bass:  return &bassMap;
            case VibeGraph::InsertKind::Drum:  return &drumMap;
            case VibeGraph::InsertKind::Audio: return &audioMap;
            case VibeGraph::InsertKind::Aux:   return &auxMap;
            case VibeGraph::InsertKind::Vox:   return &voxMap;
            case VibeGraph::InsertKind::Inst:  return &instMap;
        }
        return &layerMap;
    }
}

VibeGraph::InsertNode*
VibeGraph::ensureInsertNode(InsertKind kind, int index,
                             const juce::String& displayName,
                             const juce::String& apvtsPrefix)
{
    auto* m = selectInsertMap(kind, mLayerInserts, mBassInserts, mDrumInserts, mAudioInserts, mAuxInserts, mVoxInserts, mInstInserts);

    if (auto it = m->find(index); it != m->end())
    {
        // 2026-04-29: defensive re-bind.  If the node was first created before
        // mApvts was set (i.e. before buildFixedTopology), or before the strip's
        // params were registered, pLevel/pMute/etc. would be NULL and stay NULL
        // forever — strip fader/mute/meter would silently no-op.  Re-binding on
        // every ensure call keeps pointers fresh.
        if (mApvts != nullptr)
            it->second->rebindApvts(*mApvts);
        return it->second.get();
    }

    auto node = std::make_unique<InsertNode>(kind, index, displayName, apvtsPrefix);
    if (mSampleRate > 0.0)
        node->prepare(mSampleRate, mBlockSize);
    if (mApvts != nullptr)
        node->rebindApvts(*mApvts);

    auto* raw = node.get();
    (*m)[index] = std::move(node);
    return raw;
}

void VibeGraph::removeInsertNode(InsertKind kind, int index)
{
    auto* m = selectInsertMap(kind, mLayerInserts, mBassInserts, mDrumInserts, mAudioInserts, mAuxInserts, mVoxInserts, mInstInserts);
    m->erase(index);
}

VibeGraph::InsertNode*
VibeGraph::getInsertNode(InsertKind kind, int index)
{
    auto* m = selectInsertMap(kind, mLayerInserts, mBassInserts, mDrumInserts, mAudioInserts, mAuxInserts, mVoxInserts, mInstInserts);
    if (auto it = m->find(index); it != m->end())
        return it->second.get();
    return nullptr;
}

void VibeGraph::processInsert(InsertKind kind, int index,
                               juce::AudioBuffer<float>& buf,
                               double bpm, bool anySolo)
{
    if (auto* node = getInsertNode(kind, index))
        node->processBlock(buf, bpm, anySolo);
}

EffectRack* VibeGraph::getInsertRack(InsertKind kind, int index)
{
    if (auto* node = getInsertNode(kind, index))
        return &node->rack;
    return nullptr;
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

float VibeGraph::getInsertPeakDb(InsertKind kind, int index) const
{
    // Local const-ptr lookup (can't reuse selectInsertMap because of const)
    const std::map<int, std::unique_ptr<InsertNode>>* m = nullptr;
    switch (kind)
    {
        case InsertKind::Layer: m = &mLayerInserts; break;
        case InsertKind::Bass:  m = &mBassInserts;  break;
        case InsertKind::Drum:  m = &mDrumInserts;  break;
        case InsertKind::Audio: m = &mAudioInserts; break;
        case InsertKind::Aux:   m = &mAuxInserts;   break;
        case InsertKind::Vox:   m = &mVoxInserts;   break;
        case InsertKind::Inst:  m = &mInstInserts;  break;
    }
    if (m)
        if (auto it = m->find(index); it != m->end())
            return it->second->peakDb.load(std::memory_order_relaxed);
    return -60.f;
}

// 2026-04-30: stereo peak getter for the new split DBFSMeter.  Returns
// {peakDbL, peakDbR}.  Falls back to {-60, -60} when the insert doesn't
// exist (matches mono getter behaviour).
std::pair<float, float> VibeGraph::getInsertPeakDbStereo(InsertKind kind, int index) const
{
    const std::map<int, std::unique_ptr<InsertNode>>* m = nullptr;
    switch (kind)
    {
        case InsertKind::Layer: m = &mLayerInserts; break;
        case InsertKind::Bass:  m = &mBassInserts;  break;
        case InsertKind::Drum:  m = &mDrumInserts;  break;
        case InsertKind::Audio: m = &mAudioInserts; break;
        case InsertKind::Aux:   m = &mAuxInserts;   break;
        case InsertKind::Vox:   m = &mVoxInserts;   break;
        case InsertKind::Inst:  m = &mInstInserts;  break;
    }
    if (m)
        if (auto it = m->find(index); it != m->end())
            return { it->second->peakDbL.load(std::memory_order_relaxed),
                     it->second->peakDbR.load(std::memory_order_relaxed) };
    return { -60.f, -60.f };
}

// D3: read the insert's choke group (0 = none).  Wait-free.
int VibeGraph::getInsertChokeGroup(InsertKind kind, int index) const
{
    const std::map<int, std::unique_ptr<InsertNode>>* m = nullptr;
    switch (kind)
    {
        case InsertKind::Layer: m = &mLayerInserts; break;
        case InsertKind::Bass:  m = &mBassInserts;  break;
        case InsertKind::Drum:  m = &mDrumInserts;  break;
        case InsertKind::Audio: m = &mAudioInserts; break;
        case InsertKind::Aux:   m = &mAuxInserts;   break;
        case InsertKind::Vox:   m = &mVoxInserts;   break;
        case InsertKind::Inst:  m = &mInstInserts;  break;
    }
    if (m)
        if (auto it = m->find(index); it != m->end())
            if (auto* p = it->second->pChokeGroup)
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
    if (mMasterNode)        mMasterNode        ->rebindApvts(*mApvts, "mixer_master");
}

bool VibeGraph::isAnyInsertSoloed() const noexcept
{
    for (const auto* m : { &mLayerInserts, &mBassInserts, &mDrumInserts, &mAudioInserts, &mAuxInserts, &mVoxInserts, &mInstInserts })
        for (const auto& [i, node] : *m)
            if (node->isSoloed())
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

// ═══════════════════════════════════════════════════════════════════════════════
//  5F-4b B1a: RoutingGraph
// ═══════════════════════════════════════════════════════════════════════════════

bool RoutingGraph::wouldCreateCycle(int src, int dst) const noexcept
{
    // DFS from dst: if we can reach src from dst via existing edges,
    // adding src → dst would complete a cycle.
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
    }
    return false;
}

bool RoutingGraph::rebuildFromApvts(juce::AudioProcessorValueTreeState& apvts,
                                     const std::vector<std::pair<int, juce::String>>& activeChannels)
{
    mEdges.clear();
    mEdges.reserve(activeChannels.size() * (1 + kMaxSendsPerStrip));

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
    }

    std::vector<int> ids;
    ids.reserve(activeChannels.size());
    for (const auto& [chId, _] : activeChannels) ids.push_back(chId);
    return computeTopo(ids);
}

bool RoutingGraph::computeTopo(const std::vector<int>& ids)
{
    // Kahn's algorithm. Drops cycle edges on retry if needed.
    std::unordered_map<int, int> inDegree;
    for (int id : ids) inDegree[id] = 0;
    for (const auto& e : mEdges)
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
    }

    if (mTopoOrder.size() == ids.size()) return true;

    // Cycle present — drop all edges between unresolved nodes and retry.
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

// ═══════════════════════════════════════════════════════════════════════════════
//  5F-4b B1b: per-channel accumulator + routing orchestration
// ═══════════════════════════════════════════════════════════════════════════════

juce::AudioBuffer<float>* VibeGraph::getChannelAccumulator(int channelId)
{
    auto it = mChannelAccum.find(channelId);
    if (it == mChannelAccum.end())
    {
        // Lazy-allocate with the current block size (prepared or default).
        auto& buf = mChannelAccum[channelId];
        const int blockSize = mBlockSize > 0 ? mBlockSize : 512;
        buf.setSize(2, blockSize, false, true, false);
        return &buf;
    }
    return &it->second;
}

void VibeGraph::clearChannelAccumulators()
{
    for (auto& [id, buf] : mChannelAccum)
        buf.clear();
}

void VibeGraph::rebuildRoutingFromApvts()
{
    if (mApvts == nullptr) return;

    using namespace MixerChannelIds;
    mActiveChannels.clear();
    mActiveChannels.reserve(6 + mLayerInserts.size() + mBassInserts.size()
                           + mDrumInserts.size() + mAudioInserts.size()
                           + mAuxInserts.size() + mVoxInserts.size()
                           + mInstInserts.size());

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

    for (auto& [idx, node] : mLayerInserts)
        mActiveChannels.emplace_back(layerInsert(idx), node->apvtsPrefix);
    for (auto& [idx, node] : mBassInserts)
        mActiveChannels.emplace_back(bassInsert(idx), node->apvtsPrefix);
    for (auto& [idx, node] : mDrumInserts)
        mActiveChannels.emplace_back(drumInsert(idx), node->apvtsPrefix);
    for (auto& [idx, node] : mAudioInserts)
        mActiveChannels.emplace_back(audioInsert(idx), node->apvtsPrefix);
    for (auto& [idx, node] : mAuxInserts)
        mActiveChannels.emplace_back(auxStrip(idx), node->apvtsPrefix);
    // R1 (2026-04-23): Vox + Inst inserts must be in active channels so the
    // routing graph picks up their _sendN_to APVTS values when sends are
    // placed on aux destinations.  Without this, send cables visually fail
    // to attach because the edge never enters the graph.
    for (auto& [idx, node] : mVoxInserts)
        mActiveChannels.emplace_back(voxInsert(idx), node->apvtsPrefix);
    for (auto& [idx, node] : mInstInserts)
        mActiveChannels.emplace_back(instInsert(idx), node->apvtsPrefix);

    mRoutingGraph.rebuildFromApvts(*mApvts, mActiveChannels);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  5F-4b B2: Aux insert processing
// ═══════════════════════════════════════════════════════════════════════════════

void VibeGraph::processAuxInserts(double bpm, bool anySolo,
                                   const std::function<void(int, juce::AudioBuffer<float>&)>& fanout)
{
    for (auto& [idx, node] : mAuxInserts)
    {
        const int chId = MixerChannelIds::auxStrip(idx);
        if (auto* buf = getChannelAccumulator(chId))
        {
            // Input already summed into accumulator by upstream routeInsertOutput.
            // InsertNode::processBlock processes in-place; no preRendered copy.
            node->processBlock(*buf, bpm, anySolo);
            fanout(chId, *buf);
        }
    }
}

std::vector<int> VibeGraph::getAuxIndices() const
{
    std::vector<int> result;
    result.reserve(mAuxInserts.size());
    for (const auto& [idx, node] : mAuxInserts)
        result.push_back(idx);
    return result;
}
