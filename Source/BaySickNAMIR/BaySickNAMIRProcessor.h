#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include "../DSP/EngineSidechainHelper.h"
#include "../DSP/MicSimDSP.h"
#include "../DSP/MicPlacementDSP.h"
#include "../Standalone/ApvtsDirtyTracker.h"

// Forward declaration so we don't drag Eigen into every translation unit that
// includes this header.  Full definition is included only inside the .cpp.
namespace nam { class DSP; }

// ─────────────────────────────────────────────────────────────────────────────
// BaySickNAMIRProcessor - Neural Amp Modeler + IR Cabinet engine (Phase G-1)
// ─────────────────────────────────────────────────────────────────────────────
// Signal chain (Phase G-1.3):
//   Input → Input Gain → Noise Gate → [oversample → NAM → downsample] →
//          Low Cut HPF → High Cut LPF → fork → wet:Convolution →
//          mix(cab_mix) ← dry → Master Output
//
// Active NAM model + IR are picked from one of two slots (A or B) by the
// `ab_slot` parameter.  Each slot owns its own NAM model + Convolution +
// file paths so users can A/B compare two amp/cab combos in place.
//
// G-1.4 adds: editor UI in the existing widget style.
// G-1.5 adds: standalone test exposure.
//
// Threading model:
//   loadNamModel() / loadImpulseResponse() run on the caller's thread (the
//   message thread).  NAM model swap uses a wait-free pattern per slot: the
//   message thread parks the new model in mNamPending[slot] and sets
//   mNamSwapPending[slot]; the audio thread std::swap()s on the next
//   processBlock so old-model dealloc happens later on the message thread
//   (next load) - never on audio.  juce::dsp::Convolution provides its own
//   wait-free swap internally.
//
//   Oversampling factor changes go through `parameterChanged` → message-
//   thread async re-Reset of all loaded NAM models at the new OS-rate
//   (mSampleRate * 2^factor).  RT-safe because the audio-thread swap-pending
//   pattern still gates which model is visible to processBlock.
// ─────────────────────────────────────────────────────────────────────────────

class BaySickNAMIRProcessor : public juce::AudioProcessor,
                              public ISidechainEngine,
                              private juce::AudioProcessorValueTreeState::Listener
{
public:
    BaySickNAMIRProcessor();
    ~BaySickNAMIRProcessor() override;

    // ── AudioProcessor overrides ──────────────────────────────────────────────
    void prepareToPlay (double sampleRate, int maxBlockSize) override;
    void releaseResources()                                  override {}
    void processBlock  (juce::AudioBuffer<float>&,
                        juce::MidiBuffer&)                   override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                          { return true; }

    const juce::String getName() const override              { return "BaySickNAM/IR"; }
    bool acceptsMidi()  const override                       { return false; }
    bool producesMidi() const override                       { return false; }
    bool isMidiEffect() const override                       { return false; }
    double getTailLengthSeconds() const override             { return 0.0; }

    int  getNumPrograms() override                           { return 1; }
    int  getCurrentProgram() override                        { return 0; }
    void setCurrentProgram (int) override                    {}
    const juce::String getProgramName (int) override         { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&)                          override;
    void setStateInformation (const void*, int)                            override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        // Stereo in / out; mono inference internally.
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
            && layouts.getMainInputChannelSet () == juce::AudioChannelSet::stereo();
    }

    // ── Public state access ───────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;

    // 2026-05-05 dirty-flag wiring (see ApvtsDirtyTracker.h).  NAM file load,
    // IR file load, mic-sim / mic-placement / oversampling toggle, A/B compare
    // edits - every APVTS change fires this.  StandaloneEditor wires it to
    // ProjectManager::markDirty.
    void setOnAnyStateChange (std::function<void()> fn) { mDirtyTracker.onAny = std::move (fn); }

    // ── A/B slot helpers ──────────────────────────────────────────────────────
    // Active slot read from APVTS `ab_slot` (0 = A, 1 = B).  Returns 0 on
    // missing/invalid param so callers don't have to defensively clamp.
    int getActiveSlot() const;

    // ── File loaders (call from message thread) ───────────────────────────────
    // `slot` defaults to -1 = "current slot" (resolved from APVTS).  Pass 0/1
    // explicitly to load into a specific slot regardless of UI selection.
    // Returns true on success; on failure, fills outErr with a human-readable
    // reason and leaves the previously-active model/IR for that slot intact.
    bool loadNamModel         (const juce::String& filePath, juce::String& outErr, int slot = -1);
    bool loadImpulseResponse  (const juce::String& filePath, juce::String& outErr, int slot = -1);
    void clearNamModel        (int slot = -1);
    void clearImpulseResponse (int slot = -1);

    // ── Public state queries (per-slot; -1 = active) ──────────────────────────
    bool         hasNamModel        (int slot = -1) const;
    bool         hasImpulseResponse (int slot = -1) const;
    bool         isFullRig          (int slot = -1) const;
    juce::String getNamErrorMessage () const { return mLastNamError; }
    juce::String getIrErrorMessage  () const { return mLastIrError;  }

    juce::String getNamFilePath (int slot = -1) const;
    juce::String getIrFilePath  (int slot = -1) const;
    void         setNamFilePath (const juce::String& p, int slot = -1);
    void         setIrFilePath  (const juce::String& p, int slot = -1);

    // ── H-6d Mic Sim + Mic Placement (post-IR, pre-master-output) ────────────
    // The two stages live as direct members of this processor so they ride
    // along on every host (Vox sub-tab, Inst sub-tab, FX rack slot).  See
    // MicSimDSP / MicPlacementDSP headers for parameter semantics.
    MicSimDSP&       getMicSim()       noexcept { return mMicSim; }
    MicPlacementDSP& getMicPlacement() noexcept { return mMicPlacement; }
    bool loadUserMicIr (const juce::File& f, juce::String& outErr);
    void clearUserMicIr();

    // ── Per-slot A/B snapshot (full per-slot tone state, locked 2026-05-02) ──
    // When `ab_slot` changes (UI click OR host automation), the processor
    // captures the OUTGOING slot's APVTS values + per-stage state into its
    // SlotSnapshot, then restores the INCOMING slot's snapshot into APVTS.
    // Knobs auto-update via APVTS attachment; Mic Sim user IR reloads if
    // the per-slot path differs from the currently-loaded one.
    //
    // NOT in the snapshot (stays global across slots):
    //   ab_slot itself, oversampling.
    struct SlotSnapshot
    {
        // Knob values
        float inputGain    { 0.0f };
        float output       { 0.0f };
        float gateThresh   { -50.0f };
        float gateRelease  { 100.0f };
        float lowCut       { 20.0f };
        float highCut      { 20000.0f };
        float cabMix       { 100.0f };

        // Bypass toggles
        bool  namBypass    { false };
        bool  cabBypass    { false };

        // Mic Sim
        int   micSimMode   { 0 };
        int   micSimModel  { 0 };
        float micSimMix    { 100.0f };
        juce::String micUserIrPath;

        // Mic Placement
        float placementDistance { 30.0f };
        float placementAngle    { 0.0f };
        int   placementPolar    { 1 };
        float placementMix      { 100.0f };

        juce::ValueTree toValueTree (const juce::Identifier& root) const;
        void             fromValueTree (const juce::ValueTree& v);
    };

    // C.4 Phase 2.2: engine-level SC primitive.
    void setSidechainBuffers (juce::AudioBuffer<float>* const* bufs, int count) noexcept override
    {
        mScHelper.setSidechainBuffers (bufs, count);
    }
    float getSidechainLevel() const noexcept override { return mScHelper.getLevel(); }

    // 2026-05-05 (Bug C fix): editor subscribes to this so a bulk state
    // restore (page preset load, project load) refreshes the file-name
    // labels + slot snapshot UI.  Without this, setStateInformation
    // reloads the NAM model + IR correctly (audio works) but the editor
    // still shows "(no model loaded)" because nothing tells it to call
    // updateLabels.
    std::function<void()> onStateRestored;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ── APVTS listener (oversampling re-prep) ────────────────────────────────
    void parameterChanged (const juce::String& paramID, float newValue) override;
    void reResetNamForOversampling (int factor);   // message-thread

    // ── Helpers ──────────────────────────────────────────────────────────────
    void updateLowCutCoeffs  (float hz);
    void updateHighCutCoeffs (float hz);
    void updateGateCoeffs    (float thresholdDb, float releaseMs);
    int  resolveSlot         (int slot) const;     // -1 → APVTS, clamp to 0/1
    void rebuildOversampling ();                   // alloc 2x + 4x stages, prepare
    int  oversamplingLatencySamples (int factor) const;

    // ── Sample-rate / block state ────────────────────────────────────────────
    double mSampleRate = 44100.0;
    int    mMaxBlock   = 512;
    bool   mPrepared   = false;

    // ── NAM model swap-pending pattern (per slot, audio-thread-safe) ─────────
    // Atomic flags are explicitly initialised to false in the constructor body
    // (std::atomic's value ctor is explicit, so brace-init inside std::array
    // is fragile across MSVC/GCC).
    std::array<std::unique_ptr<nam::DSP>, 2> mNamActive;   // audio thread reads
    std::array<std::unique_ptr<nam::DSP>, 2> mNamPending;  // message thread parks new
    std::array<std::atomic<bool>, 2>         mNamSwapPending;
    std::array<std::atomic<bool>, 2>         mNamLoaded;
    std::array<std::atomic<bool>, 2>         mNamIsFullRig;
    juce::CriticalSection                    mLoadLock;     // serializes msg-thread loads
    juce::String                             mLastNamError;

    // ── IR convolution (zero-latency mode) - per slot ────────────────────────
    std::array<juce::dsp::Convolution, 2>  mIr;
    std::array<std::atomic<bool>, 2>       mIrLoaded;
    juce::String                            mLastIrError;

    // ── Pre/post filters (stereo) ────────────────────────────────────────────
    using StereoIIR = juce::dsp::ProcessorDuplicator<
                          juce::dsp::IIR::Filter<float>,
                          juce::dsp::IIR::Coefficients<float>>;
    StereoIIR mLowCut;
    StereoIIR mHighCut;
    float     mLastLowCutHz  = -1.0f;
    float     mLastHighCutHz = -1.0f;

    // ── Noise gate (stereo peak detect, per-sample envelope follower) ────────
    float mGateEnv            = 0.0f;
    float mGateAttackCoef     = 0.0f;
    float mGateReleaseCoef    = 0.0f;
    float mGateThresholdLin   = 0.0f;
    float mLastGateThreshDb   = std::numeric_limits<float>::quiet_NaN();
    float mLastGateReleaseMs  = std::numeric_limits<float>::quiet_NaN();

    // ── Oversampling around the NAM block (mono) ─────────────────────────────
    // [0] = 2x stage chain (numStages = 1), [1] = 4x stage chain (numStages = 2).
    // 1x bypasses both and runs NAM at host sample rate directly.
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 2> mOversampling;

    // ── Working buffers ──────────────────────────────────────────────────────
    std::vector<double>      mNamMonoIn;     // double - NAM_SAMPLE default is double
    std::vector<double>      mNamMonoOut;
    juce::AudioBuffer<float> mMonoFloatBuf;  // mono scratch fed into Oversampling
    juce::AudioBuffer<float> mDryBuf;        // pre-cab fork copy

    // ── File paths - saved as ValueTree custom string properties ─────────────
    // Index 0 = slot A, index 1 = slot B.
    std::array<juce::String, 2> mNamPaths;
    std::array<juce::String, 2> mIrPaths;

    EngineSidechainHelper mScHelper;   // C.4 Phase 2.2 SC primitive

    // ── H-6d post-IR stages ──────────────────────────────────────────────────
    MicSimDSP        mMicSim;
    MicPlacementDSP  mMicPlacement;
    juce::String     mLastMicIrError;

    // ── Per-slot A/B snapshots (capture-then-restore on ab_slot change) ─────
    std::array<SlotSnapshot, 2> mSnapshots;
    int                          mLastSlot { 0 };

    void captureSnapshotFromCurrent (int slot);
    void applySnapshotToCurrent     (int slot);

    // 2026-05-05 dirty-flag wiring.  Declared LAST so apvts is fully constructed.
    ApvtsDirtyTracker mDirtyTracker { apvts };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickNAMIRProcessor)
};
