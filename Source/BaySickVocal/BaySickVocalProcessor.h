#pragma once
#include <JuceHeader.h>
#include "../DSP/EngineSidechainHelper.h"
#include "../DSP/PitchCorrectorDSP.h"
#include "../DSP/BaySickAlignDSP.h"
#include "../DSP/BaySickPitchDSP.h"
#include "../EffectRack.h"
#include "../BaySickNAMIR/BaySickNAMIRProcessor.h"
#include "../Standalone/ApvtsDirtyTracker.h"

// ─────────────────────────────────────────────────────────────────────────────
// BaySickVocalProcessor - Phase H-1 skeleton (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────
// Vocal chain processor, peer to BaySickNAMIRProcessor in the Vox chain.
// APVTS prefix `bsv_*`.
//
// Locked signal flow (filled in by subsequent sub-batches):
//   input -> pitch correction -> de-esser -> compressor -> saturation
//         -> limiter -> output
//
// Editor (H-6) - 6 sub-tabs:
//   1. BaySickVocals      (realtime pitch + page-wide controls)
//   2. Vocal Chain        (de-esser / compressor / saturation / limiter rack)
//   3. BaySickPitch       (offline note-by-note pitch editor)
//   4. BaySickAlign       (offline channel-pair time-alignment editor)
//   5. BaySickNAM/IR      (existing engine hosted as a sub-tab)
//   6. Pre Rack EQ        (strip's existing Pre EQ8 M/S)
//
// H-1 scope: structural shell only. No DSP yet - processBlock is a passthrough
// honoring the master bypass + mix params. Stage Bypass params registered as
// placeholders so subsequent sub-batches can wire their stages without
// re-touching the param layout.
// ─────────────────────────────────────────────────────────────────────────────

class BaySickVocalProcessor : public juce::AudioProcessor,
                              public ISidechainEngine
{
public:
    BaySickVocalProcessor();
    // 2026-05-06 (Batch 9c N1): no longer defaulted -- body sets the
    // mShuttingDown gate so subsequent processBlock entries bail before
    // dereferencing half-destroyed members.
    ~BaySickVocalProcessor() override;

    // ── AudioProcessor overrides ──────────────────────────────────────────────
    void prepareToPlay (double sampleRate, int maxBlockSize) override;
    void releaseResources()                                  override {}
    void processBlock  (juce::AudioBuffer<float>&,
                        juce::MidiBuffer&)                   override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                          { return true; }

    const juce::String getName() const override              { return "BaySickVocal"; }
    bool acceptsMidi()  const override                       { return false; }
    bool producesMidi() const override                       { return false; }
    bool isMidiEffect() const override                       { return false; }
    double getTailLengthSeconds() const override             { return 0.0; }

    int  getNumPrograms() override                           { return 1; }
    int  getCurrentProgram() override                        { return 0; }
    void setCurrentProgram (int) override                    {}
    const juce::String getProgramName (int) override         { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&)            override;
    void setStateInformation (const void*, int)              override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        // Stereo in / out (mono->stereo upmix done by host on input).
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
            && layouts.getMainInputChannelSet () == juce::AudioChannelSet::stereo();
    }

    // ── Public state access ───────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;

    // 2026-05-05 dirty-flag wiring (see ApvtsDirtyTracker.h).  Fires on every
    // bsv_* APVTS edit (vocals page knobs, vocal-chain rack slot params,
    // pitch correction params).  StandaloneEditor wires it to markDirty;
    // the embedded NAMIR sub-processor has its own tracker wired separately.
    void setOnAnyStateChange (std::function<void()> fn) { mDirtyTracker.onAny = std::move (fn); }

    // H-6c (2026-05-01): Vocal Chain rack -- 4 locked slots in fixed order:
    //   [0] DeEsser  [1] Compressor  [2] Saturation  [3] Limiter
    // Slots 4 + 5 unused (rack always has 6 slots; the trailing two are
    // EffectType::None and pass through silently).  Editor's Vocal Chain
    // sub-tab hosts SlotComponents bound to this rack.
    EffectRack mVocalChainRack;

    // QA-F chain-wiring fix (2026-07-10): fired at the end of
    // setStateInformation after the per-slot chain-DSP blobs are restored
    // (message thread), so the Vocal Chain sub-tab can re-mount its slot
    // editors -- their knobs sync from DSP state at construction and would
    // otherwise display pre-restore values.  Installed by VocalChainPanel;
    // cleared in its destructor.
    std::function<void()> onChainStateRestored;

    // H-6d (2026-05-02): owned BaySickNAMIRProcessor for the BaySickNAM/IR
    // sub-tab.  Owning it here (instead of on the editor) lets the page
    // preset save/load capture its state automatically through
    // BaySickVocalProcessor::getStateInformation.  Audio routing wired in
    // G-9 (2026-05-03): processBlock now calls mNamIrProc->processBlock
    // after the vocal-chain rack.
    BaySickNAMIRProcessor& getNamIrProcessor() noexcept { return *mNamIrProc; }

    // I-16 G-9 (2026-05-03): force-bypass realtime pitch correction.  Set to
    // true during FilePlay so a recording with realtime pitch already baked
    // into the wet file doesn't get corrected twice.  Engine loop in
    // PluginProcessor toggles this per-block based on source-mux state.
    // Independent of the user's pitch-correction toggle (which is the
    // "want correction" intent).  Both must be true for correction to run:
    // user-toggle ON AND not force-bypassed.
    void setForcePitchBypass (bool yes) noexcept { mForcePitchBypass.store (yes, std::memory_order_release); }
    bool isForcePitchBypassed() const noexcept   { return mForcePitchBypass.load (std::memory_order_acquire); }

    // 2026-05-06 (Batch 9c N1): atomic shutdown gate.  Mirrors
    // VibeSynthProcessor::mProjectLoadInProgress.  Owners SHOULD call
    // setShuttingDown(true) ~30 ms before destroying this instance (same
    // pattern StandaloneEditor::closeAllDynamicTabs uses on the main
    // processor: setProjectLoadInProgress + Thread::sleep(30)) so the
    // audio thread sees the flag in advance of member teardown.  The
    // destructor sets it as a final safety net; in either case, processBlock
    // bails out early instead of dereferencing mNamIrProc / mVocalChainRack
    // after their dtors begin (the observed crash was mNamIrProc->processBlock
    // dispatching through a vtable already zeroed by ~BaySickNAMIRProcessor).
    void setShuttingDown (bool b) noexcept
        { mShuttingDown.store (b, std::memory_order_release); }
    bool isShuttingDown() const noexcept
        { return mShuttingDown.load (std::memory_order_acquire); }

    // I-16 G-9 (2026-05-03): wet recording tap.  Plugged in by
    // VibeSynthProcessor::startRecording when this strip is armed.
    // Captures the post-realtime-pitch / pre-vocal-chain signal (Option C
    // from G-9 spec).  Pass nullptr to clear (stopRecording).
    void setWetRecorder (class AudioFileRecorder* recorder) noexcept
    {
        mWetRecorder.store (recorder, std::memory_order_release);
    }

    // ── Sidechain primitive (engine-level, for future ducking stages) ────────
    void setSidechainBuffers (juce::AudioBuffer<float>* const* bufs, int count) noexcept override
    {
        mScHelper.setSidechainBuffers (bufs, count);
    }
    float getSidechainLevel() const noexcept override { return mScHelper.getLevel(); }

    // ── QA-F Task 3: BaySickAlign channel-pair state ─────────────────────────
    // MESSAGE THREAD ONLY end to end -- the whole Align pipeline is offline
    // (analyze -> preview -> render-to-bake per the G2-warp design lock); no
    // bsa_* param ever reaches the audio thread, so there is no per-block
    // APVTS push for this block of state.
    BaySickAlignDSP mAlign;

    struct AlignRenderEntry
    {
        juce::String file;      // project-relative ("Aligned/x_align_v1.wav")
        juce::String dateIso;   // ISO8601 at render time
        int          version { 1 };
    };

    struct AlignState
    {
        WarpMap                          map;
        std::vector<AlignSyncPoint>      syncPoints;
        std::vector<AlignProtectedArea>  protectedAreas;
        std::vector<AlignRenderEntry>    renders;
        // Channel-clip signatures captured at analyze time; a mismatch on
        // poll = the grid changed under the map -> stale badge, manual
        // re-analyze (never auto -- G2-warp lock).
        juce::int64 analyzedLeaderSig   { 0 };
        juce::int64 analyzedFollowerSig { 0 };
        // Common-origin frame the map was authored in: both composites are
        // front-padded to the earlier of the two start positions, so anchor
        // times are comparable and the bake lands back at this beat.
        double      commonStartBeat     { 0.0 };
        juce::int64 leaderPadSamples    { 0 };
        juce::int64 followerPadSamples  { 0 };
        double      analysisSampleRate  { 44100.0 };
        bool        analyzed            { false };
    };
    AlignState mAlignState;

    // Services injected by the owning VoxPage (this engine must not link
    // against VibeSynthProcessor).  All message-thread.
    std::function<juce::AudioBuffer<float> (int channelId, double& outStartBeat,
                                            juce::int64& outStartSample,
                                            double& outSampleRate)> onRenderComposite;
    std::function<juce::int64 (int channelId)>                  onChannelClipSignature;
    std::function<std::vector<std::pair<int, juce::String>>()>  onListCandidateChannels;
    std::function<juce::File()>                                 onGetProjectFolder;
    // Bake placement is a pending owner spec call (what happens to the
    // original follower clips when a render lands) -- the hook stays
    // uninstalled until it is answered; render still writes the file +
    // history entry either way.
    std::function<void (const juce::File& bake, double startBeat)> onPlaceBakedClip;

    void setOwnChannelId (int id) noexcept { mOwnChannelId = id; }
    int  getOwnChannelId() const noexcept  { return mOwnChannelId; }

    // Resolved picker state: bsa_leader_channel / bsa_follower_channel with
    // -1 meaning "none picked" (leader) / "this page's own channel" (follower).
    int resolveLeaderChannel()   const;
    int resolveFollowerChannel() const;

    // Mode/Fine-Tune -> pairing tolerance seconds (section 13e: base
    // 150/100/50 ms for Loose/Close/Tight, +/-50 ms Fine Tune).
    double alignToleranceSec() const;

    // Analyze the current Leader/Follower pair into mAlignState (composites
    // via onRenderComposite, common-origin padded).  Returns false + fills
    // errorOut on a precondition failure (no channels picked / no clips /
    // hooks missing).  MESSAGE THREAD ONLY.
    bool analyzeAlign (juce::String& errorOut);

    // Render-to-bake: warp the (re-rendered) follower composite through the
    // current map + pitch pass + optional formant shift, write
    // <project>/Aligned/{name}_align_v{N}.wav, append the history entry.
    // Returns the file (invalid on failure + fills errorOut).
    juce::File renderAlignedTake (juce::String& errorOut);

    // Warped preview of the follower composite for the Output lane (same
    // transform as the bake, no file write).  Empty when not analyzed.
    juce::AudioBuffer<float> renderAlignedPreview();

    // True when the leader/follower clip layout changed since analyze
    // (polled by the editor timer; cheap hash compare via hook).
    bool isAlignStale() const;

    // ── QA-Fa: BaySickPitch channel state ────────────────────────────────────
    // Same offline/message-thread shape as the Align block above, except the
    // realtime applicator (Mode C) DOES run on the audio thread during
    // FilePlay -- processBlock hands it the strip audio + the timeline
    // position stamped by finalizeFilePlayStrip.
    BaySickPitchDSP mPitch;
    std::vector<AlignRenderEntry> mPitchRenders;   // Pitched/ bake history
    juce::int64 mPitchAnalyzedSig { 0 };

    // Stamped per block by finalizeFilePlayStrip (next to the force-bypass
    // set) so the applicator can map strip audio to composite note regions.
    void setFilePlayTimelineSample (juce::int64 s) noexcept
        { mFilePlayTimelineSample.store (s, std::memory_order_relaxed); }
    juce::int64 getFilePlayTimelineSample() const noexcept
        { return mFilePlayTimelineSample.load (std::memory_order_relaxed); }

    // Analyze the OWN channel's composite into mPitch (auto-resolve, section
    // 14b -- no manual load).  MESSAGE THREAD ONLY.
    bool analyzePitch (juce::String& errorOut);

    // Render/Freeze bake -> <project>/Pitched/{name}_pitch_v{N}.wav +
    // history entry.  Grid placement is a pending owner call (mirrors the
    // Align question); the bake is written + listed either way.
    juce::File renderPitchedTake (juce::String& errorOut);

    bool isPitchStale() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ── Per-block APVTS->DSP push (called at top of processBlock) ───────────
    void pushApvtsToDsp() noexcept;

    // ── Sample-rate / block state ────────────────────────────────────────────
    double mSampleRate = 44100.0;
    int    mMaxBlock   = 512;
    bool   mPrepared   = false;

    EngineSidechainHelper mScHelper;

    // ── DSP chain stages (locked signal flow order) ─────────────────────────
    // input -> pitch correction -> [vocal chain rack: deess->comp->sat->lim]
    //       -> output
    // Pitch correction sits OUTSIDE the rack because it uses YIN tracker
    // backpressure + the realtime LiveASIO/FilePlay gating (G-9.1) doesn't
    // belong inside the slot-swappable rack model.
    PitchCorrectorDSP mPitchCorrector;

    juce::AudioBuffer<float> mDryScratch;   // for global Mix dry/wet crossfade

    // I-16 G-9 (2026-05-03): realtime-bypass + wet recorder hooks.  Atomics
    // because audio thread reads / message thread writes.
    std::atomic<bool> mForcePitchBypass { false };
    std::atomic<class AudioFileRecorder*> mWetRecorder { nullptr };

    // QA-Fa: FilePlay block-start timeline sample (MT worker writes inside
    // finalizeFilePlayStrip; this engine's processBlock reads in the same
    // call chain -- same-thread within a block, atomic for the ST/MT
    // branch-agnostic contract).
    std::atomic<juce::int64> mFilePlayTimelineSample { 0 };

    // 2026-05-06 (Batch 9c N1): shutdown gate (see setShuttingDown above).
    // Audio thread reads at the top of processBlock; message thread writes
    // from the destructor (and ideally pre-flagged by the owner).
    std::atomic<bool> mShuttingDown { false };

    // QA-F Task 3: this page's own mixer channel id (voxInsert(pageIndex)),
    // stamped by VoxPage::setProcessor.  -1 until stamped.
    int mOwnChannelId { -1 };

    // Shared transform behind renderAlignedTake + renderAlignedPreview:
    // re-render the follower composite, re-pad to the analysis origin, warp
    // + pitch + optional formant shift.  Empty buffer + errorOut on failure.
    juce::AudioBuffer<float> buildWarpedFollower (juce::String& errorOut);

    // H-6d (2026-05-02): owned NAM/IR processor (per-Vox-strip instance).
    // unique_ptr so the include only needs the forward declaration in
    // header consumers; full BaySickNAMIRProcessor.h is already included
    // above so we can use make_unique inline.
    std::unique_ptr<BaySickNAMIRProcessor> mNamIrProc;

    // 2026-05-05 dirty-flag wiring.  Declared LAST so apvts is fully constructed.
    ApvtsDirtyTracker mDirtyTracker { apvts };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickVocalProcessor)
};
