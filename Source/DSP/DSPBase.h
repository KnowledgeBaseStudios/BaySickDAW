#pragma once
#include <JuceHeader.h>
#include "EffectVisualFeed.h"

// ── DSPBase ───────────────────────────────────────────────────────────────────
// Abstract base for all Phase 2+ standalone DSP effect modules.
// Matches the FXBase calling convention (process(AudioBuffer)) for easy
// integration with EffectRack.
// ─────────────────────────────────────────────────────────────────────────────
class DSPBase
{
public:
    virtual ~DSPBase() = default;

    // QA-Layout T17: every effect gets a visual feed for free, and publishing
    // into it is free too while nothing is watching (see EffectVisualFeed).
    // Lives on the base rather than per-effect so a new panel visual needs no
    // new plumbing -- the DSP calls push() in process() and the gate decides.
    EffectVisualFeed&       visualFeed()       noexcept { return mVisualFeed; }
    const EffectVisualFeed& visualFeed() const noexcept { return mVisualFeed; }

    // Does this effect actually publish anything?  Drives the greyed-out state
    // of the panel's View > Visual entry: an effect with no visual still SHOWS
    // the entry (so it is discoverable and recoverable) but cannot be switched
    // to it.  Default false; a DSP that pushes columns overrides to true.
    virtual bool hasVisualFeed() const { return false; }

    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void process(juce::AudioBuffer<float>& buffer)   = 0;
    virtual void reset()                                      = 0;

    // Serialization - ValueTree XML stored as raw bytes
    virtual void getStateInformation(juce::MemoryBlock& dest) { (void)dest; }
    virtual void setStateInformation(const void* data, int sz) { (void)data; (void)sz; }

    // BPM sync - only meaningful for time-based effects; default is no-op
    virtual void setHostBPM(double /*bpm*/) {}

    // ── Full host transport (TS7, 2026-07-31) ────────────────────────────────
    // setHostBPM above carries tempo and nothing else, which is all any of our
    // own time-based effects ever needed.  A HOSTED VST3 needs more: JUCE builds
    // the VST3 ProcessContext from a single AudioPlayHead::getPosition() call
    // (juce_VST3PluginFormatImpl.h toProcessContext), and every field --
    // tempo, ppq, time signature, loop -- gets its validity flag set only if
    // that position exists.  No playhead means a zeroed context with not one
    // valid flag, so nothing in the plugin syncs to anything.
    //
    // timeInSamples is NOT optional: toProcessContext jassert-fails without it
    // ("The time in samples *must* be valid"), so a tempo-only playhead would
    // assert every block in Debug.  That is why this carries the whole transport
    // rather than being folded into setHostBPM.
    //
    // Additive and defaulted: the twelve non-hosted effect types ignore it.
    struct HostTransport
    {
        double      bpm           { 120.0 };
        double      ppqPosition   { 0.0 };
        juce::int64 timeInSamples { 0 };
        bool        isPlaying     { false };
        int         timeSigNum    { 4 };
        int         timeSigDen    { 4 };
    };
    virtual void setHostTransport(const HostTransport& /*tp*/) {}

    // Gain-reduction meter (0 = no reduction, negative dB = compressed)
    virtual float getGainReductionDb() const { return 0.0f; }

    // Algorithmic latency introduced by this effect in samples (look-ahead, FFT, etc.).
    // Does NOT include user-chosen effect parameters (reverb pre-delay, delay time, etc.)
    // Return non-zero only for inherent processing latency the user didn't choose.
    // EffectRack accumulates these for PDC.
    virtual int getLatencySamples() const { return 0; }

    // ── C.4 Phase 1 (2026-04-30): sidechain context ──────────────────────────
    // Strip pushes its 4 SC receive buffers (post-everything tap of upstream
    // sources) to every DSP module on the strip each block.  Modules that
    // consume SC choose which line drives them via their own state:
    //   * Rack effects (Compressor / Limiter / TransientShaper / future NS-2):
    //     setSidechainPick(idx) -- single pick stored in mScPick; use
    //     getActiveSidechain() inside process().
    //   * EQ8DSP: per-band scSourceId picks; module overrides setSidechainBuffers
    //     to store the array and reads buffers[scSourceId] per band.
    // bufs may be nullptr for unused slots; count is always kMaxScRecvSlots.
    virtual void setSidechainBuffers (juce::AudioBuffer<float>* const* bufs, int count) noexcept
    {
        mScBufs  = bufs;
        mScCount = count;
    }
    void setSidechainPick (int pickIdx) noexcept { mScPick = pickIdx; }

    // True for effects whose process() actually consumes the SC signal
    // (Compressor / Limiter / TransientShaper / future NS-2).  SlotComponent
    // shows the SC source dropdown in its header chrome only for these
    // effects so non-consumers (Reverb / Chorus / etc.) don't display a
    // confusing dead control.  Default false.
    virtual bool usesSidechain() const noexcept { return false; }

    bool bypassed { false };

    // App-global transport-running flag.  One processor instance per app, so a
    // static is the right scope: VibeSynthProcessor::processBlock stores it once
    // per block; editor panels poll it (UI timer) to lock latency-changing
    // controls (lookahead / FFT-size toggles) while the transport runs --
    // flipping those mid-play would click and leave bus PDC misaligned.
    inline static std::atomic<bool> sTransportPlaying { false };
    static bool isTransportPlaying() noexcept
    {
        return sTransportPlaying.load (std::memory_order_relaxed);
    }

protected:
    juce::AudioBuffer<float>* getActiveSidechain() const noexcept
    {
        if (mScBufs == nullptr || mScPick < 0 || mScPick >= mScCount) return nullptr;
        return mScBufs[mScPick];
    }

    double mSampleRate  { 44100.0 };
    int    mMaxBlock    { 512 };

    juce::AudioBuffer<float>* const* mScBufs  { nullptr };
    int                              mScCount { 0 };
    int                              mScPick  { -1 };

    // Protected, not private: derived effects push into it from process().
    EffectVisualFeed mVisualFeed;
};
