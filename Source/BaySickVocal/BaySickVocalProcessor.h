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

    // ── QA-F Task 3 / QA-Fa recovery: BaySickAlign channel-pair state ────────
    // Analyze / versions / render are MESSAGE THREAD ONLY.  Applied maps
    // reach the audio thread as an immutable AlignPlaySnapshot (atomic swap +
    // retire ring, the BaySickPitchDSP contract): the clip-decode layer in
    // VibeSynthProcessor::decodeFilePlayClip reads it per block and warps the
    // follower channel's FilePlay read position live.  bsa_align_on is the
    // chain switch (decode reads the cached raw-param atomic).
    BaySickAlignDSP mAlign;

    struct AlignRenderEntry
    {
        juce::String file;      // project-relative ("Aligned/x_align_v1.wav")
        juce::String dateIso;   // ISO8601 at render time
        int          version { 1 };
        // QA-Fa recovery: the render's timeline origin (Align = the pair's
        // commonStartBeat, Pitch = the composite startBeat) so Add-From-
        // Export can land the file back at its original position.
        double       startBeat { 0.0 };
    };

    // QA-Fa recovery: one applied state in the per-editor version history.
    // `state` is the same ValueTree shape the project XML persists (Align:
    // the applied-frame tree; Pitch: the PitchState tree), so revert =
    // deserialize + republish.  Signatures power the "(grid changed)" marker.
    struct EditorVersionEntry
    {
        juce::ValueTree state;
        juce::String    dateIso;
        juce::int64     sigA { 0 };   // Align: leader sig / Pitch: own-channel sig
        juce::int64     sigB { 0 };   // Align: follower sig / Pitch: unused
    };

    struct AlignState
    {
        WarpMap                          map;
        std::vector<AlignSyncPoint>      syncPoints;
        std::vector<AlignProtectedArea>  protectedAreas;
        std::vector<AlignRenderEntry>    renders;
        // Channel-clip signatures captured at analyze time; a mismatch on
        // poll = the grid changed under the map -> stale badge.  QA-Fa
        // recovery: staleness now also arms the stop-gated auto re-analyze
        // (VoxPage timer; runs on transport stop, debounced ~1s otherwise).
        juce::int64 analyzedLeaderSig   { 0 };
        juce::int64 analyzedFollowerSig { 0 };
        // Common-origin frame the map was authored in: both composites are
        // front-padded to the earlier of the two start positions, so anchor
        // times are comparable and the bake lands back at this beat.
        double      commonStartBeat     { 0.0 };
        juce::int64 commonStartSample   { 0 };
        juce::int64 leaderPadSamples    { 0 };
        juce::int64 followerPadSamples  { 0 };
        double      analysisSampleRate  { 44100.0 };
        bool        analyzed            { false };
    };
    AlignState mAlignState;

    // ── QA-Fa recovery: per-editor version histories (message thread) ────────
    std::vector<EditorVersionEntry> mAlignVersions;
    std::vector<EditorVersionEntry> mPitchVersions;

    // Append the CURRENT applied state as a new version (called by every
    // successful analyze, and by the Pitch editor's explicit Snapshot
    // action).  FIFO-capped at kMaxEditorVersions.
    static constexpr int kMaxEditorVersions = 20;
    void appendAlignVersion();
    void appendPitchVersion();

    // Revert to versions[index]: deserialize the stored state back into the
    // live fields and republish the playback snapshot.  Reverting to an
    // entry whose signature differs from the current grid is allowed -- the
    // stale badge lights immediately (the entry shows "(grid changed)").
    bool revertAlignToVersion (int index);
    bool revertPitchToVersion (int index);

    // ── QA-Fa recovery: live-playback snapshot publication ──────────────────
    // Rebuild + atomically publish the decode-layer AlignPlaySnapshot from
    // mAlignState (or clear when not analyzed).  MESSAGE THREAD.
    void publishAlignPlayback();

    // AUDIO/MT: one atomic load; pointer valid within the current block
    // (retire ring of 8 -- the BaySickPitchDSP liveness contract).
    const AlignPlaySnapshot* loadAlignPlaySnapshot() const noexcept
        { return mAlignPlayActive.load (std::memory_order_acquire); }

    // AUDIO/MT: bsa_align_on via the cached raw-param atomic.
    bool isAlignChainOn() const noexcept
        { return mAlignOnRaw == nullptr || mAlignOnRaw->load() > 0.5f; }

    // AUDIO/MT: the Pitch box gates, mirroring the bake path's reads --
    // live pitch pull = (anchor semis + transpose) only while pitch_on.
    bool isAlignPitchOn() const noexcept
        { return mAlignPitchOnRaw != nullptr && mAlignPitchOnRaw->load() > 0.5f; }
    float alignTransposeSemis() const noexcept
        { return mAlignTransposeRaw != nullptr ? mAlignTransposeRaw->load() : 0.0f; }

    // Services injected by the owning VoxPage (this engine must not link
    // against VibeSynthProcessor).  All message-thread.
    std::function<juce::AudioBuffer<float> (int channelId, double& outStartBeat,
                                            juce::int64& outStartSample,
                                            double& outSampleRate)> onRenderComposite;
    std::function<juce::int64 (int channelId)>                  onChannelClipSignature;
    std::function<std::vector<std::pair<int, juce::String>>()>  onListCandidateChannels;
    std::function<juce::File()>                                 onGetProjectFolder;
    // True while THIS strip is capturing a take.  The realtime board locks
    // on it (engage-edge toggles mid-take click AND print into the WET
    // capture -- the sound is set before the take, owner call 2026-07-10).
    std::function<bool()>                                       onIsStripRecording;

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

    // ── QA-Fb Option A monitor merge (same-thread, block-scoped) ─────────────
    // VoxStripTask sets this immediately before calling processBlock when the
    // strip is live (armed or listen) with FilePlay clips overlapping; the
    // SAME processBlock call consumes + clears it.  Never set from any other
    // thread -- plain members, no atomics needed, but the pointer must only
    // reference storage that outlives the call (the task's decode scratch).
    //   takes    -> decoded prior-take sum (align warp already applied at the
    //               decode layer).  processBlock applies the channel's note
    //               edits via the monitor-stream applicator (A1), then sums
    //               it in AFTER the corrector + WET tap and BEFORE the rack,
    //               so the chain + NAM process the vocal stack exactly like
    //               post-stop playback does.
    //   timeline -> the takes' block start on the project timeline.
    //   muteLive -> armed && !listen: the live stream is captured (DRY/WET)
    //               but removed from the monitor before the merge.
    void setMonitorMergeForThisBlock (juce::AudioBuffer<float>* takes,
                                      juce::int64 timelineSample,
                                      bool muteLive) noexcept
    {
        mMonitorMerge         = takes;
        mMonitorMergeTimeline = timelineSample;
        mMonitorMuteLive      = muteLive;
    }

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

    // QA-Fb: reusable mono scratch for the WET tap (was a per-block
    // AudioBuffer construction -- audio-thread allocation).  Sized in
    // prepareToPlay; lazy-grow guarded like mDryScratch.
    juce::AudioBuffer<float> mWetMonoScratch;

    // QA-Fb Option A monitor merge -- see setMonitorMergeForThisBlock.
    // Same-thread block-scoped contract; consumed + cleared at the top of
    // processBlock so a bailed block (master bypass / shutdown) can never
    // leave a stale pointer for a later call.
    juce::AudioBuffer<float>* mMonitorMerge { nullptr };
    juce::int64               mMonitorMergeTimeline { 0 };
    bool                      mMonitorMuteLive { false };

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

    // QA-Fa recovery: applied-frame (de)serialization shared by project
    // persistence, the version history, and revert.
    juce::ValueTree alignAppliedToTree() const;
    void alignAppliedFromTree (const juce::ValueTree& v);

    // QA-Fa recovery: decode-layer snapshot (see loadAlignPlaySnapshot).
    // Message thread owns the retired list; audio holds the active pointer
    // only within one block.
    std::atomic<AlignPlaySnapshot*> mAlignPlayActive { nullptr };
    std::vector<std::unique_ptr<AlignPlaySnapshot>> mAlignPlayRetired;

    // Cached raw-param atomics (APVTS owns them; pointers stable for the
    // processor's lifetime).
    std::atomic<float>* mAlignOnRaw        { nullptr };
    std::atomic<float>* mAlignPitchOnRaw   { nullptr };
    std::atomic<float>* mAlignTransposeRaw { nullptr };

    // H-6d (2026-05-02): owned NAM/IR processor (per-Vox-strip instance).
    // unique_ptr so the include only needs the forward declaration in
    // header consumers; full BaySickNAMIRProcessor.h is already included
    // above so we can use make_unique inline.
    std::unique_ptr<BaySickNAMIRProcessor> mNamIrProc;

    // 2026-05-05 dirty-flag wiring.  Declared LAST so apvts is fully constructed.
    ApvtsDirtyTracker mDirtyTracker { apvts };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickVocalProcessor)
};
