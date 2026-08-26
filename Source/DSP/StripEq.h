#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include "Kbs/ParametricEq.h"
#include "Kbs/Feeds.h"

// ── StripEq ───────────────────────────────────────────────────────────────────
// QA-EqPro: the strip EQ wrapper around ONE kbs::ParametricEq.  Replaces
// EQ8MsDSP's two-engine mid+side pair (SC-1): per-band domain routing lives on
// each band (EqChannel), so one 24-band engine covers what the pair covered,
// with half the instances and half the linear-mode latency (the pair ran its
// halves in series).
//
// Threading contract: the engine is single-threaded by design.  Every band
// and value setter is applied on the AUDIO thread by the processor's
// APVTS-cache sweep (pushBand); the reallocating configuration actions
// (setMode / setOversampling) are message-thread calls that the caller runs
// under the project-load shield with a settle (SC-15) - same idiom as the
// rest of the load boundaries.  setListenBand is the engine's one
// any-thread setter and is forwarded raw.
//
// A/B (SC-16): the spare bank lives here, DSP-side, and serializes with the
// EQ point - swap on the message thread is a params-only exchange the next
// audio-thread sweep materializes (the UI pushes the swapped bank to the
// APVTS immediately after, exactly like the old EQ8DSP flow).
// ─────────────────────────────────────────────────────────────────────────────
class StripEq : public DSPBase
{
public:
    static constexpr int kBands = kbs::ParametricEq::kMaxBands;

    StripEq() = default;
    ~StripEq() override = default;

    // DSPBase interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset   ()                                     override;
    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;
    int  getLatencySamples() const override { return mEq.latencySamples(); }

    // The engine, for the display's queries (magnitudeAt family, bandGrDb)
    // and for setListenBand.  UI reads are coherent because every write goes
    // through the audio-thread sweep or the shielded config path.
    kbs::ParametricEq&       engine()       noexcept { return mEq; }
    const kbs::ParametricEq& engine() const noexcept { return mEq; }

    // Audio-thread band sync: compare against the last pushed params and
    // forward only on change, so an untouched band costs a struct compare.
    void pushBand (int i, const kbs::EqBandParams& p);
    kbs::EqBandParams getBand (int i) const;

    // Audio-thread global sync (cheap, engine setters self-guard).
    void pushGlobals (bool propQ, bool autoGain, float agAmount01,
                      float outGainDb, bool polarity);

    // Configuration actions - message thread, under the shield (SC-15).
    void setMode (kbs::EqMode m);
    void setOversampling (bool on);
    kbs::EqMode getMode() const noexcept { return mEq.getMode(); }
    bool getOversampling() const noexcept { return mEq.getOversampling(); }

    // Load-boundary slate: every band off/default, spare reseeded, globals
    // and mode back to defaults.  Caller holds the shield.
    void resetToDefaults();

    // A/B spare bank (SC-16).
    void saveToSpare();
    void swapWithSpare();
    void lockSpare (bool locked) noexcept { mSpareLocked = locked; }
    bool isViewingSpare() const noexcept { return mViewingSpare; }
    bool isSpareLocked() const noexcept { return mSpareLocked; }
    kbs::EqBandParams getSpareBand (int i) const;

    // Untouched EQs must cost nothing (72 idle instances at startup), but a
    // latency-bearing configuration must NEVER short-circuit: a flat EQ in a
    // linear mode has to impose its reported delay or the strip plays early
    // against the PDC solve (the old B2 defect).
    bool isIdentity() const noexcept;

    void setSidechainBuffers (juce::AudioBuffer<float>* const* bufs, int count) noexcept override;

    // Spectrum taps at the wrapper's stereo boundary; the analyser polls them.
    // scFeed carries ONE of the strip's four receive lines (scFeedSlot; -1 =
    // first connected) for the analyser overlay + collision view.
    kbs::SpectrumFeed preFeed, postFeed, scFeed;
    std::atomic<int>  scFeedSlot { -1 };
    std::atomic<bool> scFeedAlive { false };

    // Per-EQ-point display preferences (scale, analyser toggles, the domain
    // view), serialized inside the state blob so they travel with the project
    // - the plugin's viewTree pattern, per EQ point.
    juce::ValueTree& viewTree() noexcept { return mViewTree; }

private:
    static juce::ValueTree bandToTree (int index, const kbs::EqBandParams& p);
    static void bandFromTree (const juce::XmlElement& e, kbs::EqBandParams& p);

    kbs::ParametricEq mEq;
    juce::ValueTree mViewTree { "View" };
    std::array<kbs::EqBandParams, kBands> mCached {};   // last pushed, audio thread
    std::array<kbs::EqBandParams, kBands> mSpare {};
    bool mViewingSpare { false };
    bool mSpareLocked  { false };

    // Wrapper mirrors of engine values the engine has no getters for.
    float mOutGainDb { 0.0f };
    bool  mPolarity  { false };
    bool  mAutoGain  { false };
    float mAgAmount  { 1.0f };
};
