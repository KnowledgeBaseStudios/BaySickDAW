#pragma once
#include <JuceHeader.h>

// ── DSPBase ───────────────────────────────────────────────────────────────────
// Abstract base for all Phase 2+ standalone DSP effect modules.
// Matches the FXBase calling convention (process(AudioBuffer)) for easy
// integration with EffectRack.
// ─────────────────────────────────────────────────────────────────────────────
class DSPBase
{
public:
    virtual ~DSPBase() = default;

    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void process(juce::AudioBuffer<float>& buffer)   = 0;
    virtual void reset()                                      = 0;

    // Serialization - ValueTree XML stored as raw bytes
    virtual void getStateInformation(juce::MemoryBlock& dest) { (void)dest; }
    virtual void setStateInformation(const void* data, int sz) { (void)data; (void)sz; }

    // BPM sync - only meaningful for time-based effects; default is no-op
    virtual void setHostBPM(double /*bpm*/) {}

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
};
