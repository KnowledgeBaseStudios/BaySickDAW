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

    // ── The feed's time axis (QA-Buffers, 2026-08-09) ────────────────────────
    // A column is a fixed slice of TIME, not one audio block.  Publishing one
    // column per block made the picture a function of the device buffer: the
    // same 600-pixel strip spanned 1.7 s at a 128 buffer and 55.7 s at 4096, and
    // it stretched the harmonic-bar hold in EffectWindows (max of the last 30
    // columns) from 80 ms to 2.8 s.  With a fixed column period a strip's width
    // in pixels IS a width in seconds at every buffer size and sample rate.
    //
    // CALIBRATION: one column = the duration of a 128-sample block at 44100 Hz.
    // That is exactly what these visuals showed before this fix at the buffer
    // size Jeff runs, so the picture there is unchanged and every other buffer
    // size converges onto it.
    static constexpr double kVisualColumnSeconds = 128.0 / 44100.0;

    // For a panel that wants to label its own time axis instead of inferring the
    // column rate from the block period (which is no longer the column period).
    static constexpr double visualColumnSeconds() noexcept { return kVisualColumnSeconds; }

    // Does the AUDIO thread push columns into the feed?  This is a question
    // about publishing, nothing else.  Default false; a DSP that pushes
    // overrides to true.
    virtual bool hasVisualFeed() const { return false; }

    // Does this effect have a Visual window at all?  DELIBERATELY separate from
    // hasVisualFeed() (QA-Layout T20): whether a window exists and whether the
    // audio thread publishes into it are different questions.  Every visual we
    // ship is feed-driven, so this hook exists to keep a parametric-only visual
    // -- one that reads DSP state at paint time and pushes nothing -- buildable
    // without reworking the menu's presence gate.
    //
    // Defaults to hasVisualFeed() so a feed-driven effect needs one override,
    // not two; a parametric visual overrides THIS one alone.
    virtual bool hasVisual() const { return hasVisualFeed(); }

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

    // ── T18 audio-first visual helpers (Jeff, 2026-08-06) ────────────────────
    // The standard column for an in-vs-out visual: hi/lo = output envelope,
    // a = input envelope, both linear so the strip reads as a waveform.  Shared
    // here so six effects cannot drift six ways.  Both are no-ops (one relaxed
    // load) while nothing is watching.
    //
    // Call visualCaptureIn at the TOP of process(), before the buffer is
    // modified; call visualPushInOut at the end of every path that produced
    // output.  A path that early-returns simply pushes nothing that block.
    float visualCaptureIn (const juce::AudioBuffer<float>& b) const noexcept
    {
        return mVisualFeed.isActive() && b.getNumSamples() > 0
                 ? b.getMagnitude (0, b.getNumSamples()) : 0.0f;
    }

    // Samples per display column at the LIVE sample rate (see
    // kVisualColumnSeconds).  Never zero, so the accumulator below always makes
    // progress even if prepare() has not landed yet.
    int visualColumnSamples() const noexcept
    {
        const double sr = mSampleRate > 0.0 ? mSampleRate : 44100.0;
        return juce::jmax (1, juce::roundToInt (kVisualColumnSeconds * sr));
    }

    // QA-Buffers (2026-08-09): emits on ELAPSED TIME, not per block.  A block
    // shorter than one column keeps accumulating its envelope into the pending
    // column; a block longer than one column is sliced so the sub-block detail
    // survives instead of collapsing into a single held peak.  `inPk` is the
    // block's input peak, so it is held across every column that block emits --
    // visualCaptureIn measures the input once, before the buffer is overwritten.
    void visualPushInOut (const juce::AudioBuffer<float>& b, float inPk) noexcept
    {
        const int n = b.getNumSamples();
        if (! mVisualFeed.isActive() || n <= 0)
        {
            // Drop the part-built column rather than splicing it onto whatever
            // arrives when the window is reopened.
            mVisAccumSamples = 0;
            mVisPendOut      = 0.0f;
            mVisPendIn       = 0.0f;
            return;
        }

        const int spc = visualColumnSamples();
        // A sample-rate change between blocks can leave the accumulator past the
        // new column length; restarting is cheaper than carrying a partial
        // column measured against the old rate, and keeps `take` positive.
        if (mVisAccumSamples >= spc)
        {
            mVisAccumSamples = 0;
            mVisPendOut      = 0.0f;
            mVisPendIn       = 0.0f;
        }

        // The input peak accumulates alongside the output one: a block SHORTER
        // than a column (32 samples against a 128-sample column) contributes to
        // a column it does not complete, and taking inPk verbatim would publish
        // only the last such block's peak while the output kept all of them.
        // The ghost-in / solid-out comparison is the whole point of the trace,
        // so the two sides have to be measured over the same span.
        mVisPendIn = juce::jmax (mVisPendIn, juce::jlimit (0.0f, 1.0f, inPk));

        for (int pos = 0; pos < n;)
        {
            const int take = juce::jmin (n - pos, spc - mVisAccumSamples);
            mVisPendOut       = juce::jmax (mVisPendOut, b.getMagnitude (pos, take));
            pos              += take;
            mVisAccumSamples += take;

            if (mVisAccumSamples >= spc)
            {
                const float o = juce::jlimit (0.0f, 1.0f, mVisPendOut);
                mVisualFeed.push (-o, o, mVisPendIn, 0.0f);
                mVisAccumSamples = 0;
                mVisPendOut      = 0.0f;
                mVisPendIn       = 0.0f;
            }
        }
    }

    // Audio thread only (the owning DSP's process()); the UI never reads these.
    int   mVisAccumSamples { 0 };
    float mVisPendOut      { 0.0f };
    float mVisPendIn       { 0.0f };
};
