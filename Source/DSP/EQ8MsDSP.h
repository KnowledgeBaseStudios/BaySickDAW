#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include "EQ8DSP.h"
#include "SpectrumFeed.h"

// ── EQ8MsDSP ──────────────────────────────────────────────────────────────────
// Mid/Side stereo EQ wrapper around two EQ8DSP instances.
//
// 5F-9 sec.12 Phase 2 (12h): per-band M/S routing lives inside EQ8DSP itself
// (Band::channel). The wrapper is kept as the natural 2 x 8 = 16-band container
// for storage, serialisation and UI binding (Option A from 2026-04-19 scoping).
// Its process() now passes the FULL stereo buffer to both inner EQ8DSPs and
// drops the outer M/S encode/decode: each EQ8DSP handles per-band routing on
// its own. Default channel defaults are seeded in the constructor so today's
// behaviour is preserved exactly when no band has been re-routed:
//     mMid's bands default to channel = Mid
//     mSide's bands default to channel = Side
//
// 5F-9 sec.12 Phase 2 (12i): two SpectrumFeed members per instance, populated
// at the stereo-I/O boundary of process(). UI polls them for pre-EQ (grey)
// and post-EQ (yellow) analyser overlays. Feeds live with their EQ so every
// EQ8MsDSP instance - bus-level or insert-level - automatically exposes both.
//
// `showMid` is a UI-hint only; it does not affect DSP. External page-header
// MID/SIDE buttons drive it via ParametricEQDisplay::setShowMid.
// ─────────────────────────────────────────────────────────────────────────────
class EQ8MsDSP : public DSPBase
{
public:
    EQ8MsDSP();
    ~EQ8MsDSP() override = default;

    // DSPBase interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset   ()                                     override;
    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    // ── Access to the two inner EQ instances ──────────────────────────────────
    EQ8DSP& mid()  { return mMid;  }
    EQ8DSP& side() { return mSide; }

    const EQ8DSP& mid()  const { return mMid;  }
    const EQ8DSP& side() const { return mSide; }

    // 12f: anti-cramping pass-through. Both inner EQs run on the same buffer
    // (in series), so the toggle covers both. Latency adds.
    void setAntiCramping(bool on)
    {
        mMid .setAntiCramping(on);
        mSide.setAntiCramping(on);
    }
    bool isAntiCramping() const noexcept { return mMid.isAntiCramping(); }

    // 12g: phase-mode pass-through. Linear modes go through both inner EQs
    // serially, so each inner instance contributes its own linear-phase
    // latency to the wrapper sum below.
    void setPhaseMode(EQ8DSP::PhaseMode mode)
    {
        mMid .setPhaseMode(mode);
        mSide.setPhaseMode(mode);
    }
    EQ8DSP::PhaseMode getPhaseMode() const noexcept { return mMid.getPhaseMode(); }
    bool              isLinearPhaseMode() const noexcept { return mMid.isLinearPhaseMode(); }

    // 12f: PDC. Mid then Side run sequentially on the same buffer, so total
    // latency is the sum of their oversampler latencies.
    int getLatencySamples() const override
    {
        return mMid.getLatencySamples() + mSide.getLatencySamples();
    }

    // 2026-04-22 §P4.3 perf: identity if both inner EQs are identity.
    // Callers wrap process() with this so untouched EQs cost nothing.
    bool isIdentity() const noexcept
    {
        return mMid.isIdentity() && mSide.isIdentity();
    }

    // ── UI hint: which side the display is currently showing ─────────────────
    // Does NOT affect audio - both EQs always run.
    bool showMid { true };

    // ── 12i: per-instance spectrum taps ──────────────────────────────────────
    // Audio thread pushes one block of mono-mixdown samples at entry (preFeed)
    // and at exit (postFeed). UI thread polls at 30 Hz for the analyser overlay.
    SpectrumFeed preFeed;
    SpectrumFeed postFeed;

private:
    EQ8DSP mMid;
    EQ8DSP mSide;

    // Pre-allocated scratch for the mono-mixdown used by the spectrum feeds
    // (keeps process() heap-allocation-free on the audio thread).
    juce::AudioBuffer<float> mFeedScratch;
};
