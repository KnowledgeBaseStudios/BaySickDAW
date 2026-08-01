#include "VersionCapture.h"

VersionCapture::~VersionCapture()
{
    // Editor-facing callbacks are dropped first: we are inside the editor's
    // teardown, and member destruction order decides whether the surfaces they
    // touch still exist.  onEndAudio stays -- it stops the PROCESSOR-owned
    // recorder, and the processor outlives the editor.
    onVersionsChanged = nullptr;
    onPersistTake     = nullptr;

    // Close the capture BEFORE the folder delete: endTake stops the recorder,
    // so the delete never races a writer holding a file open in the folder --
    // Windows would refuse it and the temp folder would leak.
    endTake();
    discardSessionAudio();
}

juce::File VersionCapture::sessionDir()
{
    if (mSessionDir != juce::File() && mSessionDir.isDirectory())
        return mSessionDir;

    // createTempFile gives a unique NAME; we want a unique FOLDER so
    // discardSessionAudio can delete the whole thing without walking a shared
    // temp dir and guessing which files were ours.
    mSessionDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                      .getChildFile ("BaySickDAW")
                      .getChildFile ("Versions "
                          + juce::String (juce::Time::getCurrentTime()
                                              .toMilliseconds()));
    mSessionDir.createDirectory();
    return mSessionDir;
}

juce::File VersionCapture::nextAudioTarget (int takeId) const
{
    // Timestamped, never just "Take N": the take counter is SESSION-scoped, so
    // in the retained folder a new session's "Take 1.wav" landed on last
    // session's -- and a fresh FileOutputStream APPENDS to an existing file,
    // so the collision did not replace the old take, it corrupted both.
    // Windows-filename-safe stamp, same shape as the recording paths.
    const juce::String stamp = juce::Time::getCurrentTime()
                                   .formatted ("%Y-%m-%d %H-%M-%S");
    const juce::String name  = "Take " + juce::String (takeId)
                             + " - " + stamp + ".wav";

    auto finish = [] (juce::File f)
    {
        // Same-second relaunch edge: the writer must open on a FRESH file.
        if (f.existsAsFile()) f.deleteFile();
        return f;
    };

    if (mRetainInProject && onGetProjectDir)
    {
        const auto proj = onGetProjectDir();
        if (proj != juce::File())
        {
            // §3.4 says <project>\Reports\ and means it.  A "Versions"
            // subfolder was mine and it does not survive its own logic: the
            // reports ARE the versions, so a subfolder would leave the Reports
            // folder holding nothing but a subfolder (Jeff, 2026-07-30).
            auto dir = proj.getChildFile ("Reports");
            dir.createDirectory();
            return finish (dir.getChildFile (name));
        }
    }
    return finish (const_cast<VersionCapture*> (this)->sessionDir().getChildFile (name));
}

void VersionCapture::poll (juce::uint32 playStartEdges,
                           juce::uint32 loopWrapEdges,
                           bool         playing,
                           juce::uint32 changeStamp,
                           float        shortTermLufs,
                           float        truePeakDb,
                           const juce::String& scopeLabel)
{
    // First poll only establishes the baseline.  Without this the counters'
    // initial values would read as a burst of edges and open a phantom take
    // before the user has pressed anything.
    if (! mHaveEdgeBase)
    {
        mLastPlayStart = playStartEdges;
        mLastLoopWrap  = loopWrapEdges;
        mHaveEdgeBase  = true;
        return;
    }

    const bool played  = (playStartEdges != mLastPlayStart);
    const bool wrapped = (loopWrapEdges  != mLastLoopWrap);
    mLastPlayStart = playStartEdges;
    mLastLoopWrap  = loopWrapEdges;

    // §3.2: a play-press ALWAYS starts a version.  The user asked for this pass
    // explicitly, so it is worth keeping even if nothing changed since the last.
    if (played)
    {
        endTake();
        beginTake (scopeLabel, changeStamp);
    }
    else if (wrapped)
    {
        // §3.2: a loop pass starts one ONLY when something changed.  Looping a
        // section untouched for two minutes should not fill the list with
        // twenty identical entries.
        endTake();
        if (! mHaveKept || changeStamp != mLastKeptStamp)
            beginTake (scopeLabel, changeStamp);
    }

    if (mCapturing && ! playing)
    {
        endTake();
        return;
    }

    if (! mCapturing) return;

    // kTimerHz down to kHistoryHz.
    if (++mTickDivider < (kTimerHz / kHistoryHz)) return;
    mTickDivider = 0;

    mCurrent.lufsCurve.push_back (shortTermLufs);
    mCurrent.maxShortTerm = juce::jmax (mCurrent.maxShortTerm, shortTermLufs);
    mLra.push (shortTermLufs);

    // §5.3 for captured takes.  The span is this tick's window: the curve is
    // sampled at kHistoryHz, so a breach is known to within that tick and no
    // finer.  A rendered measurement resolves to the audio block instead, which
    // is why the two paths produce spans of different granularity from the SAME
    // coalescing rule -- worth knowing when comparing a take against an export.
    const double tNow = (double) (mCurrent.lufsCurve.size() - 1) / (double) kHistoryHz;
    const double tEnd = tNow + 1.0 / (double) kHistoryHz;

    if (truePeakDb > mSpec.maxTruePeakDb)
        LoudnessViolation::addOrExtend (mCurrent.violations,
                                        LoudnessViolation::Kind::TruePeak,
                                        tNow, tEnd, truePeakDb,
                                        mCurrent.violationsTruncated);

    if (mSpec.checksShortTerm && shortTermLufs > mSpec.maxShortTermLufs)
        LoudnessViolation::addOrExtend (mCurrent.violations,
                                        LoudnessViolation::Kind::ShortTerm,
                                        tNow, tEnd, shortTermLufs,
                                        mCurrent.violationsTruncated);
}

void VersionCapture::beginTake (const juce::String& scopeLabel,
                                juce::uint32 changeStamp)
{
    mCurrent = Version();
    mCurrent.id         = mNextId;
    mCurrent.scopeLabel = scopeLabel;
    mCurrent.label      = "Take " + juce::String (mNextId) + "  -  "
                        + juce::Time::getCurrentTime().toString (false, true, false);
    mLra.reset();
    mTickDivider = 0;
    mCapturing   = true;
    mLastKeptStamp = changeStamp;
    mHaveKept      = true;
    mHaveAudio     = false;

    if (onTakeBegan) onTakeBegan();

    if (mAudioEnabled && onBeginAudio)
    {
        const auto target = nextAudioTarget (mNextId);
        mHaveAudio = onBeginAudio (target);
    }
}

void VersionCapture::endTake()
{
    if (! mCapturing) return;
    mCapturing = false;

    juce::File audio;
    if (mHaveAudio && onEndAudio) audio = onEndAudio();
    mHaveAudio = false;

    // A take with no curve is a play-press that produced no measured audio --
    // a start immediately stopped, or a scrub.  Storing it would put an empty
    // row in the list for something the user would not call a take.
    if (mCurrent.lufsCurve.empty())
    {
        if (audio != juce::File() && audio.existsAsFile()) audio.deleteFile();
        return;
    }

    mCurrent.lraLu       = mLra.lra();
    mCurrent.durationSec = (double) mCurrent.lufsCurve.size() / (double) kHistoryHz;
    mCurrent.audioFile   = audio;

    mVersions.push_back (mCurrent);
    ++mNextId;

    // §3.4: in project-retention mode the analysis is written out too, not just
    // the optional audio.  Uses mVersions.back() rather than mCurrent so the
    // persisted copy is exactly the one on the list.
    if (mRetainInProject && onPersistTake) onPersistTake (mVersions.back());

    if (onVersionsChanged) onVersionsChanged();
}

void VersionCapture::setFinalMeters (float integratedLufs, float truePeakDb) noexcept
{
    // Applies to the take being closed, so the editor calls this immediately
    // before the poll that will end it.  Guarded because a stray call with no
    // take open would otherwise write numbers into a Version that never ships.
    if (! mCapturing) return;
    mCurrent.integratedLufs = integratedLufs;
    mCurrent.truePeakDb     = truePeakDb;
}

const VersionCapture::Version* VersionCapture::find (int id) const noexcept
{
    for (const auto& v : mVersions)
        if (v.id == id) return &v;
    return nullptr;
}

void VersionCapture::clearAll()
{
    discardSessionAudio();
    mVersions.clear();
    mNextId    = 1;
    mHaveKept  = false;
    mCapturing = false;
    if (onVersionsChanged) onVersionsChanged();
}

void VersionCapture::discardSessionAudio()
{
    // Only the session temp folder is removed.  Anything the user chose to
    // retain lives under the project and is theirs, not ours to delete.
    if (mSessionDir != juce::File() && mSessionDir.isDirectory())
        mSessionDir.deleteRecursively();
    mSessionDir = juce::File();
}
