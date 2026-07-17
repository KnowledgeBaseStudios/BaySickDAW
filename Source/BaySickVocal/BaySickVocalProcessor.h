#pragma once
#include <JuceHeader.h>
#include "../DSP/EngineSidechainHelper.h"
#include "../DSP/PitchCorrectorDSP.h"
#include "../DSP/MonitorPitchShifter.h"
#include "../DSP/BaySickAlignDSP.h"
#include "../DSP/BaySickPitchDSP.h"
#include "../DSP/DenoiseDSP.h"
#include "../EffectRack.h"
#include "../BaySickNAMIR/BaySickNAMIRProcessor.h"
#include "../Standalone/ApvtsDirtyTracker.h"

// ─────────────────────────────────────────────────────────────────────────────
// BaySickVocalProcessor - Phase H-1 skeleton (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────
// Vocal chain processor, peer to BaySickNAMIRProcessor in the Vox chain.
// APVTS prefix `bsv_*`.
//
// Locked signal flow (QA-Fe2 chain order):
//   input -> pitch correction -> gate -> de-reverb -> de-esser -> compressor
//         -> saturation -> limiter -> output
//
// Editor (H-6) - 6 sub-tabs:
//   1. BaySickVocals      (realtime pitch + page-wide controls)
//   2. Vocal Chain        (gate / de-reverb / de-esser / compressor /
//                          saturation / limiter rack)
//   3. BaySickPitch       (offline note-by-note pitch editor)
//   4. BaySickAlign       (offline channel-pair time-alignment editor)
//   5. BaySickNAM/IR      (existing engine hosted as a sub-tab)
//   6. Pre Rack EQ        (strip's existing Pre EQ8 M/S)
//
// QA-Fd 3a/12b: the page-master Bypass (bsv_bypass) is retired -- the chain
// always runs so pitch/align edits are never silenced by a page switch.
// Per-stage Bypass params + the realtime section's own toggle remain the
// bypass surface.
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

    // H-6c (2026-05-01) / QA-Fe2 (2026-07-16): Vocal Chain rack -- 6 locked
    // slots in fixed order:
    //   [0] Gate  [1] De-reverb  [2] De-esser  [3] Compressor
    //   [4] Saturation  [5] Limiter
    // Editor's Vocal Chain sub-tab hosts SlotComponents bound to this rack.
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

    // QA-Fe Task 5: live-monitor mode for this strip (0 = True Dry, 1 = Bypass
    // Pitch Corrector, 2 = With Effect [default since QA-Fe2 docket 2a]).  Set
    // per block from VoxStripTask off the mixer_vox_<n>_monitorMode param.
    // Governs only what the monitor HEARS of the live voice -- the WET
    // recording stays corrected in every mode (the WET tap sits before this
    // split).
    void setMonitorMode (int m) noexcept { mMonitorMode.store (m, std::memory_order_release); }

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

    // QA-Fe2 PDC: the engine-side path latency downstream of the input --
    // vocal chain rack (bypass-aware; De-reverb 2048, spectral De-esser
    // 1024/2048) plus the owned NAM/IR stage's oversampling latency, which
    // was written via setLatencySamples but never read by any aggregator
    // before the full-graph pass.  Consumed by VibeGraph::updateBusLatencies
    // via VibeSynthProcessor's hook.  Message thread.  The realtime pitch
    // corrector's ~48 ms is deliberately NOT in this sum: it runs only while
    // live-monitoring, and folding it in would delay every other path to
    // match the cue mix the performer is singing against (see the QA-Fe2
    // monitor-latency spec call).
    int getChainLatencySamples() const
    {
        int total = mVocalChainRack.getTotalLatencySamples();
        if (mNamIrProc != nullptr)
            total += juce::jmax (0, mNamIrProc->getLatencySamples());
        return total;
    }

    // ── QA-Fe2 De-noise live learners (dual-domain, see DenoiseDSP.h) ────────
    // Enabled from interface-track assignment on (message thread); the raw
    // learner listens pre-corrector, the wet learner post-corrector, because
    // the corrector transforms the noise floor along with the voice -- each
    // cleaned take variant needs its matching-domain profile.
    void setDenoiseLearnersEnabled (bool on)
    {
        mDenoiseLearnEnabled.store (on, std::memory_order_release);
        if (on)  mDenoisePump.startTimerHz (15);
        else     mDenoisePump.stopTimer();
    }
    void resetDenoiseLearners()          // track reassignment: room may differ
    {
        mDenoiseRawLearner.reset();
        mDenoiseWetLearner.reset();
    }
    void getDenoiseProfiles (DenoiseProfile& raw, DenoiseProfile& wet) const
    {
        raw = mDenoiseRawLearner.snapshot();
        wet = mDenoiseWetLearner.snapshot();
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
        // QA-Fd: align-settings generation the map was analyzed under.  A
        // mismatch vs the live counter = a time-map knob (Mode / Fine /
        // Max Shift / Pitch Types) moved since -> stale -> the stop-gated
        // auto re-analyze swaps the map at the next stop ("maps only change
        // while stopped").  Transient: not persisted.
        juce::int64 analyzedSettingsGen { 0 };
        // QA-Fd: the follower's pitch time-map hash at analyze time -- a
        // pitch-tab time edit redefines the performance align matched, so a
        // mismatch marks the analysis stale (pitch is UPSTREAM, locked
        // 5/13a).  Transient: not persisted.
        juce::int64 analyzedPitchMapHash { 0 };
    };
    AlignState mAlignState;

    // QA-Fd: the follower channel's pitch time-map hash right now (0 when
    // no follower / no hook / no edits).  MESSAGE THREAD.
    juce::int64 followerPitchMapHash() const;

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

    // QA-Fd review fix: true while setStateInformation restores.  The align
    // editor's mode-change hook seats the Blend preset via callAsync, which
    // otherwise runs AFTER the load restored the saved Blend and stomps it.
    // Cleared via a queued callAsync so it outlives every hook the restore
    // posted (queue order guarantees the hooks see it still raised).
    bool isRestoringState() const noexcept
        { return mRestoringState->load (std::memory_order_acquire); }

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
    // QA-Fd: the pitch DSP owning a channel's time edits (the align
    // FOLLOWER may be another Vox page's channel; its time map redefines
    // the performance align must match).  Installed by VoxPage.
    std::function<BaySickPitchDSP* (int channelId)>             onPitchDspForChannel;
    // QA-Fd #7: the MAIN transport beat (StandalonePlayHead::getCurrentBeat,
    // UI-safe) -- the pitch editor's playhead follows it INCLUDING the
    // stop-reset seek, replacing the freeze-on-stop FilePlay stamp.
    std::function<double()>                                     onTransportBeat;
    // Seek the MAIN transport (StandalonePlayHead::seekTo, project beats).  The
    // pitch editor's ruler click writes the shared song position; the builder
    // grid reads the same playhead, so the two stay in sync bidirectionally.
    std::function<void(double beat)>                            onTransportSeek;
    // Pitch-editor ruler range <-> the builder-grid SONG time selection (bars).
    // Write pushes the range (endBar <= startBar clears it); the getter reads it
    // back each timer tick so the pitch ruler stays in sync + loops in song mode.
    std::function<void(float startBar, float endBar)>           onSetSongTimeSel;
    std::function<bool(float& startBar, float& endBar)>         onGetSongTimeSel;

    void setOwnChannelId (int id) noexcept { mOwnChannelId = id; }
    int  getOwnChannelId() const noexcept  { return mOwnChannelId; }

    // Resolved picker state: bsa_leader_channel / bsa_follower_channel with
    // -1 meaning "none picked" (leader) / "this page's own channel" (follower).
    int resolveLeaderChannel()   const;
    int resolveFollowerChannel() const;

    // QA-Fd semantics rework (locked 9a/10a/14a/15a): Mode/Fine-Tune ->
    // residual tightness cap R (base Tight 0 / Close 50 / Loose 100 ms,
    // +/-50 ms Fine Tune, sum clamped >= 0).  Matching itself runs inside
    // the DSP's fixed internal window.
    double alignResidualCapSec() const;
    // Per-word movement cap (bsa_align_maxShift, ms -> seconds).
    double alignMaxShiftSec() const;
    // Stretch-ratio bound r for the map builder.  Fixed at Normal (2.0)
    // since the Flexibility picker's removal (2026-07-11); kept as a
    // function so the control can return without touching the DSP.
    double alignFlexRatio() const;

    // QA-Fd 12a: raw anchor delta -> effective pull through the live pitch
    // knobs.  Differences inside the Variation cap are untouched; the excess
    // is scaled by Blend percent.  Used by publishAlignPlayback (live path)
    // and buildWarpedFollower (render/preview) so every consumer agrees.
    float effectiveAlignPitchSemis (float rawSemis) const;

    // QA-Fd: time-map knob generation.  The align editor bumps it on any
    // Mode / Fine / Max Shift / Pitch Types change; isAlignStale() compares
    // against the analyzed generation, and VoxPage's poller folds it into
    // its debounce signature so a knob change re-arms the stop-gated auto
    // re-analyze even at an unchanged grid.
    void bumpAlignSettingsGeneration() noexcept { mAlignSettingsGen.fetch_add (1, std::memory_order_acq_rel); }
    juce::int64 alignSettingsGeneration() const noexcept { return mAlignSettingsGen.load (std::memory_order_acquire); }

    // Analyze the current Leader/Follower pair into mAlignState (composites
    // via onRenderComposite, common-origin padded).  Returns false + fills
    // errorOut on a precondition failure (no channels picked / no clips /
    // hooks missing).  MESSAGE THREAD ONLY.
    bool analyzeAlign (juce::String& errorOut);

    // Render-to-bake: warp the (re-rendered) follower composite through the
    // current map + pitch pass + optional formant shift, write
    // <project>/Aligned/{name}_align_v{N}.wav, append the history entry.
    // Returns the file (invalid on failure + fills errorOut).
    // highRes (QA-Fd 16a): the warp phase runs oversampled (384 kHz-class).
    juce::File renderAlignedTake (juce::String& errorOut, bool highRes = false);

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

    // QA-Fd: source-position stamp (decode layer -> finalize -> applicator).
    // Valid for THIS block only; finalize re-stamps every block (identity
    // when the composed law is not engaged).
    void setFilePlaySourceStamp (double x0, double ratePerSample, bool valid) noexcept
    {
        mFilePlaySrcX0.store (x0, std::memory_order_relaxed);
        mFilePlaySrcRate.store (ratePerSample, std::memory_order_relaxed);
        mFilePlaySrcValid.store (valid, std::memory_order_relaxed);
    }

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

    // ── QA-Fd preview play (shared by scrub-audition + the sub-editor Play
    // button): auditions a span of the note while the main transport is stopped,
    // streamed into the chain pre-rack so it sounds like playback.  QA-Fe: reads
    // the background-baked cache at the EDITED span (no message-thread render);
    // falls back to the raw SOURCE span when the note has no cache (unedited, or a
    // time-only edit that bakes as neutral).  Both spans are composite-relative
    // seconds; they are equal for an untimed note. ─────────────────────────────
    void startPitchPreview (const juce::AudioBuffer<float>& composite, double sr,
                            double srcStartSec, double srcEndSec,
                            double editedStartSec, double editedEndSec, bool loop);
    void stopPitchPreview();
    bool isPitchPreviewPlaying() const noexcept
    {
        auto* pv = mPitchPreview.load (std::memory_order_acquire);
        return pv != nullptr
            && (pv->loop || pv->cursor.load (std::memory_order_relaxed)
                              < pv->buf.getNumSamples());
    }

    // Render/Freeze bake -> <project>/Pitched/{name}_pitch_v{N}.wav +
    // history entry.  Grid placement is a pending owner call (mirrors the
    // Align question); the bake is written + listed either way.
    // QA-Fd Task 8: the bake honors the channel's TIME edits (post-warp
    // through the published time map).
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

    // QA-Fe Task 5: raw live-input stash for the monitor split.  True Dry +
    // Bypass Pitch Corrector re-route the live voice around the corrector for
    // the monitor while the WET tap still captures the corrected stream.
    // Lazy-grow guarded like mDryScratch.
    juce::AudioBuffer<float> mMonitorLiveDry;

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

    // QA-Fe2 De-noise learners.  pushBlock is audio-thread wait-free; the
    // FFT work runs on mDenoisePump (message-thread timer).  Mono fold uses
    // a prepare-time scratch -- no audio-thread allocation.
    struct DenoisePump : juce::Timer
    {
        std::function<void()> onTick;
        void timerCallback() override { if (onTick) onTick(); }
    };
    DenoiseLearner     mDenoiseRawLearner, mDenoiseWetLearner;
    DenoisePump        mDenoisePump;
    std::atomic<bool>  mDenoiseLearnEnabled { false };
    std::vector<float> mDenoiseMonoScratch;

    // QA-Fe Task 5: live-monitor mode (0 TrueDry / 1 BypassCorr / 2 WithEffect
    // [default since QA-Fe2 docket 2a]).  Audio thread reads; VoxStripTask
    // writes per block.
    std::atomic<int> mMonitorMode { 2 };

    // Monitor-swap de-click: a hard mode swap steps the waveform (the corrected
    // voice carries the corrector latency, the raw voice does not), so a mode
    // change equal-gain crossfades over ~10 ms.  Audio-thread-only state.
    int mMonLastMode    { -1 };
    int mMonXfadeFrom   { 1 };
    int mMonXfadeRemain { 0 };
    int mMonXfadeLen    { 0 };

    // QA-Fe2 docket 2a: low-latency corrected monitor.  While realtime
    // correction is live, the With-Effect monitor/mix path swaps the R3
    // stream (~48 ms) for a dual-tap time-domain shift of the RAW voice
    // driven by the same smoothed correction ratio (~12 ms); the R3 stream
    // above the split keeps feeding the WET recorder untouched.
    // mMonShiftFade ramps the substitution over ~40 ms at the
    // correction-toggle edges (mirrors the corrector's own engage fade).
    // Audio-thread-only state.
    MonitorPitchShifter mMonitorShifter;
    std::vector<float>  mMonShiftScratch;
    float mMonShiftFade      { 0.0f };
    float mMonShiftFadeStep  { 1.0f / 1764.0f };
    bool  mMonShiftWasActive { false };

    // QA-Fe Task 4: latency-align the WET take.  The realtime corrector's
    // LiveShifter delays its output by getLatencySamples(); the WET recorder
    // (armed only while realtime pitch is active) captures that delayed stream,
    // so the take would play back that late vs the transport.  Drop the first
    // getLatencySamples() the recorder would write (pre-roll / primed silence)
    // so WET[0] aligns to the record-arm moment, matching the zero-latency DRY
    // take.  Audio-thread-only state; mPrevWetRecorder edge-detects the arm.
    class AudioFileRecorder* mPrevWetRecorder { nullptr };
    int                      mWetLatencySkip { 0 };

    // QA-Fa: FilePlay block-start timeline sample (MT worker writes inside
    // finalizeFilePlayStrip; this engine's processBlock reads in the same
    // call chain -- same-thread within a block, atomic for the ST/MT
    // branch-agnostic contract).
    std::atomic<juce::int64> mFilePlayTimelineSample { 0 };

    // QA-Fd: source-position stamp (same write/read contract as above).
    std::atomic<double> mFilePlaySrcX0    { 0.0 };
    std::atomic<double> mFilePlaySrcRate  { 1.0 };
    std::atomic<bool>   mFilePlaySrcValid { false };

    // QA-Fd preview play: message thread renders + publishes; audio streams
    // + advances the cursor.  Retire ring mirrors the snapshot contract.
    struct PitchPreview
    {
        juce::AudioBuffer<float> buf;   // mono, device rate
        std::atomic<int> cursor { 0 };
        bool loop { false };
    };
    std::atomic<PitchPreview*> mPitchPreview { nullptr };
    std::vector<std::unique_ptr<PitchPreview>> mPreviewRetired;

    // 2026-05-06 (Batch 9c N1): shutdown gate (see setShuttingDown above).
    // Audio thread reads at the top of processBlock; message thread writes
    // from the destructor (and ideally pre-flagged by the owner).
    std::atomic<bool> mShuttingDown { false };

    // QA-Fd review fix (see isRestoringState).  shared_ptr so the deferred
    // clear can never touch a dead processor (the lambda co-owns the flag).
    std::shared_ptr<std::atomic<bool>> mRestoringState
        = std::make_shared<std::atomic<bool>> (false);

    // QA-F Task 3: this page's own mixer channel id (voxInsert(pageIndex)),
    // stamped by VoxPage::setProcessor.  -1 until stamped.
    int mOwnChannelId { -1 };

    // QA-Fd: live time-map knob generation (see bumpAlignSettingsGeneration).
    // Editor writes / message+UI threads read -- atomic for the poller.
    std::atomic<juce::int64> mAlignSettingsGen { 0 };

    // Shared transform behind renderAlignedTake + renderAlignedPreview:
    // re-render the follower composite, re-pad to the analysis origin,
    // compose the follower's pitch TIME map (QA-Fd Task 8 -- renders play
    // what the decode layer plays), warp + pitch + optional formant shift.
    // Empty buffer + errorOut on failure.
    juce::AudioBuffer<float> buildWarpedFollower (juce::String& errorOut,
                                                  bool highRes = false);

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
