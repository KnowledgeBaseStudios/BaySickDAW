#pragma once
#include <JuceHeader.h>
#include "../DSP/LoudnessSpec.h"
#include <memory>
#include <vector>

// -- VersionCapture -- QA-ModelShell TS7 §3 -------------------------------------
// A "version" is one playback pass, captured automatically, so the user can
// compare what they just heard against what they heard before WITHOUT having
// planned to record it.  That is the whole point: the comparison a beginner
// needs is almost always the one they did not know they would want.
//
// ANALYSIS is always on; AUDIO is a toggle (§3.1).  The split is a cost
// decision: a five-minute take's analysis is well under 100 KB, while its audio
// is ~16 MB per minute at 24-bit stereo.  The cheap half is the half worth
// having by default.
//
// TWO THINGS THIS DELIBERATELY IS NOT:
//
//  * Not freeze's change detector (§3.3).  Capture taps the POST-FADER master,
//    so a rack knob, a bus EQ, the master limiter or a fader move all change
//    what was captured.  Freeze's per-tab engine-scope stamp would call those
//    passes unchanged and throw them away.  Two detectors, two scopes, on
//    purpose -- see ProjectManager::getChangeStamp.
//
//  * Not a performance archive (§3.7).  A master-tap capture records the master
//    chain AS IT WAS at that moment, so takes compare as OUTPUTS, not as source
//    performances.  Re-listening to take 1 after changing the master limiter
//    plays take 1's audio through the limiter it was rendered with, not the
//    current one.  The UI says so; it is a real limitation, not a footnote.
//
// THREAD MODEL: message thread only.  The audio thread publishes edge counters
// and meter values; this polls them from the editor's timer and never calls
// back into audio.
// ------------------------------------------------------------------------------
class VersionCapture
{
public:
    struct Version
    {
        int          id { 0 };            // 1-based take number within the session
        juce::String label;
        juce::String scopeLabel;          // what was playing

        std::vector<float> lufsCurve;     // Short-Term, sampled at kHistoryHz
        float  integratedLufs { -120.0f };
        float  lraLu          {    0.0f };
        float  truePeakDb     { -144.0f };
        float  maxShortTerm   { -120.0f };
        double durationSec    {    0.0  };

        // §5.3 flagged moments, for CAPTURED takes as well as rendered ones
        // (Jeff, 2026-07-30).  Built through LoudnessViolation::addOrExtend --
        // the same coalescing rule the offline scan uses, so a sustained
        // overshoot reads as one span in both.
        std::vector<LoudnessViolation> violations;
        bool violationsTruncated { false };

        // Empty when audio capture was off for this take.  Nothing ever deletes
        // a take's audio behind the user's back (Jeff, 2026-07-30: no cap) --
        // session-only retention discards the whole temp folder at close, with
        // sweepStaleSessions reclaiming at the next startup what an abnormal
        // exit stranded, and a retained take under <project>\Reports\ is theirs.
        juce::File audioFile;
    };

    VersionCapture() = default;
    ~VersionCapture();

    // ── Settings (§3.1 / §3.4) ───────────────────────────────────────────────
    void setAudioCaptureEnabled (bool on) noexcept { mAudioEnabled = on; }
    bool isAudioCaptureEnabled() const noexcept    { return mAudioEnabled; }

    // false = session-only (temp dir, discarded on close), the default.
    // true  = kept in <project>\Reports\.
    void setRetainInProject (bool on) noexcept     { mRetainInProject = on; }
    bool isRetainInProject() const noexcept        { return mRetainInProject; }

    // ── Hooks the editor supplies ────────────────────────────────────────────
    // Kept as hooks rather than dependencies: this class must not reach into the
    // processor or the recorder.
    //
    // Capture does NOT stand down while a user recording runs -- it does not need
    // to.  The two use separate AudioFileRecorder instances at the same tap
    // (PluginProcessor::startMasterCapture vs startRecording), so they coexist.
    //
    // onBeginAudio returns false if audio could not be started; capture then
    // runs analysis-only for that take instead of pretending it has audio.
    std::function<bool(const juce::File& /*target*/)> onBeginAudio;
    std::function<juce::File()>                       onEndAudio;
    // Where a retained take's audio lives.  Empty File = no project open, which
    // forces session-only regardless of the preference.
    std::function<juce::File()>                       onGetProjectDir;

    // Fires whenever the version list changes, so the analyzer window can
    // refresh without polling it.
    std::function<void()> onVersionsChanged;

    // Fires at the start of every take, audio or not.  The editor uses it to
    // clear the master true-peak running max, which must be per-take and lives
    // on the audio side where this class cannot reach.
    std::function<void()> onTakeBegan;

    // §3.4: persist a completed take's ANALYSIS when retention is set to the
    // project.  Fires only in that mode.
    //
    // Analysis is the half §3.1 says is always kept, and it was the half that
    // survived nothing: the version list is in-memory, so with the audio toggle
    // off (the default) "keep takes in the project" retained literally nothing.
    //
    // It is written as one of the EXISTING loudness reports rather than a new
    // sidecar format, which is what makes Jeff's own framing true -- the reports
    // ARE the versions, they land in the same <project>\Reports\, the Files
    // browser already lists them, and the embedded data block already reloads
    // them into this same analyzer.  No second format, no second folder, no
    // second reader.
    std::function<void(const Version&)> onPersistTake;

    // ── Driving (§3.2) ───────────────────────────────────────────────────────
    // Called from the editor's timer at kTimerHz.  playStartEdges / loopWrapEdges
    // are the processor's counters; `playing` is the transport state.
    // truePeakDb is the INSTANTANEOUS master true peak, not the take's running
    // max: a flagged moment has to be timecoded to when it happened, and the max
    // carries no time.
    void poll (juce::uint32 playStartEdges,
               juce::uint32 loopWrapEdges,
               bool         playing,
               juce::uint32 changeStamp,
               float        shortTermLufs,
               float        truePeakDb,
               const juce::String& scopeLabel);

    // The spec a take is judged against.  Capture runs whether or not the export
    // dialog or the analyzer is open, so it holds its own copy rather than
    // reaching into either.
    void setSpec (const LoudnessSpec& s) noexcept { mSpec = s; }
    const LoudnessSpec& getSpec() const noexcept  { return mSpec; }

    // Called at the end of a take to attach the numbers only the meters know.
    // Separate from poll() so the editor supplies them from the processor.
    void setFinalMeters (float integratedLufs, float truePeakDb) noexcept;

    bool isCapturing() const noexcept { return mCapturing; }

    // ── The list (§3.6) ──────────────────────────────────────────────────────
    const std::vector<Version>& versions() const noexcept { return mVersions; }
    const Version* find (int id) const noexcept;

    // Session-only takes are deleted here; retained ones are left on disk.
    void discardSessionAudio();

    // Startup reclaim: deletes session temp folders left behind by instances
    // that never ran their destructor (crash, force-quit).  Skips any folder a
    // running instance still owns.  Call once at startup on the message thread,
    // before any take can open this session's folder.
    static void sweepStaleSessions();

    // Sampling rate of lufsCurve.  Matches EBU Tech 3342's own rate and the
    // analyzer's live history, so a captured curve and a live one are the same
    // kind of line and can be drawn by the same code.
    static constexpr int kHistoryHz = 10;
    static constexpr int kTimerHz   = 30;

private:
    void beginTake (const juce::String& scopeLabel, juce::uint32 changeStamp);
    void endTake();
    juce::File nextAudioTarget (int takeId) const;

    std::vector<Version> mVersions;
    Version              mCurrent;
    bool                 mCapturing { false };
    bool                 mHaveAudio { false };

    LoudnessRangeAccumulator mLra;

    juce::uint32 mLastPlayStart { 0 };
    juce::uint32 mLastLoopWrap  { 0 };
    bool         mHaveEdgeBase  { false };
    // The change stamp as of the last take that was KEPT.  A loop pass whose
    // stamp still matches starts nothing: the version already on the list
    // describes that state (§3.2).
    juce::uint32 mLastKeptStamp { 0 };
    bool         mHaveKept      { false };

    int mTickDivider { 0 };
    int mNextId      { 1 };

    bool mAudioEnabled    { false };
    bool mRetainInProject { false };
    LoudnessSpec mSpec { LoudnessSpec::get (LoudnessSpec::Id::Streaming14) };

    juce::File mSessionDir;   // lazily created temp dir for session-only takes
    juce::File sessionDir();
    static juce::File sessionsParent();

    // Open for as long as mSessionDir is ours.  An open file is what tells
    // another instance's sweep that this folder is still in use, so it must
    // outlive every take and be released before the folder is deleted.
    std::unique_ptr<juce::FileOutputStream> mActiveLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VersionCapture)
};
