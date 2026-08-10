#pragma once
#include <JuceHeader.h>
#include "PluginBridgeProtocol.h"
#include "BridgeSharedMemory.h"

// ── Hosting::SandboxedPluginClient ──────────────────────────────────────────
// QA-ModelShell TS6 (BLU-302): the host end of the plugin bridge.  Owns one
// helper process hosting exactly one plugin, which is FL's measured shape
// (Jeff, 2026-07-29: bridging a second plugin produced a second process).
//
// Slots in behind HostedPluginInstance rather than beside it -- that seam
// landed at step 3 precisely so this class adds an implementation instead of
// forcing a rewrite of the rack slot, the tab engine, the editor windows and
// the automation lanes.
//
// THREADING, and it is the whole risk surface of this class:
//   * The audio thread MUST NOT block on the helper.  `processBlock` writes the
//     block into the shared mapping's control header, signals a named event,
//     and waits on the reply event with a deadline DERIVED from the live block
//     period (a fixed one is a different rule at every buffer size); a helper
//     that misses it yields SILENCE for that slot and the block moves on, and
//     the next block resynchronises rather than inheriting the miss.  Since v3 the
//     audio path touches NO pipe at all -- the pipe send allocated a frame and
//     could block for the pipe timeout, both forbidden on the audio thread.
//   * IPC callbacks arrive on the CONNECTION'S READER THREAD, not the message
//     thread (the earlier claim here was false).  Cross-thread state is atomic
//     or lock-guarded, and user-facing callbacks are marshalled to the message
//     thread before they fire.
// ─────────────────────────────────────────────────────────────────────────────

namespace Hosting
{

class SandboxedPluginClient final : private juce::ChildProcessCoordinator
{
public:
    SandboxedPluginClient();
    ~SandboxedPluginClient() override;

    // Launches the architecture-matched helper and loads the plugin.  Returns
    // false with `errorOut` set if the helper is missing, the handshake fails,
    // or the plugin will not load in the helper.
    bool start (const juce::PluginDescription&, juce::String& errorOut);

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void releaseResources();

    // Host transport handed across per block.  Deliberately NOT a juce::
    // AudioPlayHead: the helper is a separate PROCESS (and may be a different
    // architecture), so nothing with a vtable or a pointer can cross.  Plain
    // values only; the helper rebuilds a playhead from them on its side.
    struct TransportInfo
    {
        double       bpm           { 120.0 };
        double       ppqPosition   { 0.0 };
        juce::int64  timeInSamples { 0 };
        bool         isPlaying     { false };
        int          timeSigNum    { 4 };
        int          timeSigDen    { 4 };
    };

    // Audio-thread entry.  Returns false when the helper did not answer within
    // the deadline -- caller clears the buffer.  Never blocks unbounded: the
    // deadline is a fixed share of THIS block's period, or the far larger
    // (still bounded) offline one while setNonRealtime is on.
    bool processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&,
                       const TransportInfo&) noexcept;

    void  setParameter (int index, float value01) noexcept;
    int   getNumParameters() const noexcept { return mNumParameters.load(); }
    int   getLatencySamples() const noexcept { return mLatencySamples.load(); }

    // Forwarded to the helper per block via the control header, so a bridged
    // plugin renders offline exports in offline mode like everything else, and
    // it also selects processBlock's rendezvous deadline -- an offline render
    // has no device callback to starve, and the live ceiling is far too tight
    // for a render-sized block.
    void  setNonRealtime (bool b) noexcept { mNonRealtime.store (b, std::memory_order_release); }

    // The load result used to be INVISIBLE: LoadReply.ok was read and dropped,
    // and mLastError had no reader -- a plugin that failed in the helper looked
    // exactly like one that loaded.  Both surfaced now.
    bool loadFailed() const noexcept { return mLoadFailed.load (std::memory_order_acquire); }
    juce::String getLastError() const
    {
        const juce::ScopedLock sl (mErrorLock);
        return mLastError;
    }

    // Fired on the MESSAGE THREAD (marshalled off the reader thread).
    // onLoadResult: ok, error text, isInstrument, acceptsMidi -- the last two
    // correct a 32-bit plugin's filename-only scan description on first load.
    std::function<void (bool, juce::String, bool, bool)> onLoadResult;
    // The helper's parameter list just landed (arrives AFTER LoadReply).
    // Lane registration hangs off this: registering at load-event time saw an
    // empty list for every bridged plugin, leaving restored lanes silent.
    std::function<void()> onParameterList;
    // The bridged editor's real size, reported by the helper after it opens.
    std::function<void (int, int)> onEditorSize;

    // v3: the helper's parameter list (index implicit by order), so automation
    // can target bridged parameters -- the in-process parameter objects the
    // lanes used to require do not exist on this side of the boundary.
    struct BridgedParam { juce::String id, name; };
    std::vector<BridgedParam> getParameterList() const
    {
        const juce::ScopedLock sl (mParamLock);
        return mParams;
    }

    // v4: current program info, pushed by the helper after load and on every
    // program change the plugin reports.  Only the CURRENT program's name is
    // relayed -- the naming linkage needs nothing else.
    int getNumPrograms() const noexcept    { return mNumPrograms.load(); }
    int getCurrentProgram() const noexcept { return mCurrentProgram.load(); }
    juce::String getCurrentProgramName() const
    {
        const juce::ScopedLock sl (mProgramLock);
        return mProgramName;
    }

    // v5: the parameter the user last moved inside the plugin's own editor
    // (id + display name).  Empty until a touch arrives.
    void getLastTouchedParam (juce::String& idOut, juce::String& nameOut) const
    {
        const juce::ScopedLock sl (mTouchLock);
        idOut   = mTouchedId;
        nameOut = mTouchedName;
    }

    // Monotonic touch counter -- the delete prompt's dirty bit compares this
    // against a baseline snapshot instead of comparing state blobs (state
    // blobs carry volatile bytes on many plugins, so blob-compare reads
    // dirty on an untouched instance).
    int getTouchCount() const noexcept { return mTouchCount.load(); }

    void getState (juce::MemoryBlock& dest);
    void setState (const void* data, int size);

    // Reparents the plugin's editor into our window.  The handle crosses as an
    // integer; the helper does the SetParent on its own side.
    void openEditor (void* parentWindowHandle, int width, int height);
    void closeEditor();

    bool isAlive() const noexcept { return mAlive.load(); }

    // Fired on the MESSAGE THREAD when the helper dies -- marshalled, because
    // the notice itself lands on whichever thread noticed.  NOT fired for a
    // deliberate teardown: destroying this client runs the same disconnect the
    // crash path does, and reporting that as a crash would paint a healthy
    // plugin dead on every slot removal and every rebridge.
    // HostedPluginInstance turns this into HostedState::Crashed, which is what
    // keeps the window open showing the dead marker instead of closing it
    // (FL's behavior, and the carve-out TS5's EffectSlotWindow needed).
    std::function<void()> onCrashed;

    // Where the helper binaries live, by architecture.  x86 is what a 32-bit
    // VST3 needs and is the only way it can load at all.
    static juce::File helperExecutable (bool want32Bit);

private:
    void handleMessageFromWorker (const juce::MemoryBlock&) override;
    void handleConnectionLost() override;

    bool sendFramed (Bridge::MessageType, const void* payload, std::uint32_t payloadBytes,
                     const void* trailer = nullptr, std::uint32_t trailerBytes = 0);

    // Live rendezvous ceiling for a block of `numSamples`, in whole ms.  Reads
    // the prepared rate, so it tracks a device change with no extra plumbing.
    int liveDeadlineMs (int numSamples) const noexcept;

    // ── Marshalled-callback lifetime ─────────────────────────────────────────
    // EVERY callback this client posts with callAsync -- onLoadResult,
    // onEditorSize, onParameterList and onCrashed -- must capture mLife weakly
    // and bail on expired().  The consumer OWNS this client (HostedPluginInstance
    // holds it in a unique_ptr), the posts originate on the IPC reader thread,
    // and callAsync has no cancellation: a reply posted while the client is
    // healthy can still be sitting in the message queue when the user removes
    // the slot, deletes the tab or opens another project, and dispatching it
    // then writes through a freed owner.  The token retires with this member at
    // the end of the destructor -- NOT through an explicit reset in the body,
    // which would race the reader thread's own weak_ptr construction off the same
    // shared_ptr object.  killWorkerProcess joins that thread first, and a queued
    // lambda cannot dispatch until the message thread returns to its queue, so it
    // still finds the token expired and drops itself.
    //
    // onCrashed needs a SECOND guard the other three do not, because
    // handleConnectionLost arrives on three different threads: the IPC reader
    // thread (pipe closed), the message thread (the ping thread's timeout), and
    // SYNCHRONOUSLY inside ~SandboxedPluginClient, because killWorkerProcess ->
    // disconnect -> connectionLost is the very chain a real crash takes.
    //   mTearingDown -- raised before the destructor disconnects, so ordinary
    //     teardown never reports a crash.  An atomic rather than simply clearing
    //     onCrashed, because the reader thread reads that std::function and
    //     mutating it from here would be a data race.
    std::atomic<bool>     mTearingDown { false };
    std::shared_ptr<char> mLife { std::make_shared<char> ('\0') };

    std::atomic<bool> mAlive          { false };
    std::atomic<int>  mNumParameters  { 0 };
    std::atomic<int>  mLatencySamples { 0 };
    std::atomic<std::uint32_t> mSequence { 0 };

    // Audio transport.  Deliberately NOT the IPC pipe: copying every block
    // through the pipe would put an allocation and a syscall on the audio path.
    //
    // Was a juce::MemoryBlock that nothing ever read or wrote -- and a
    // MemoryBlock is process-local, so the helper could not have seen it even if
    // something had.  Now a real named mapping (TS7, 2026-07-31).
    Bridge::SharedAudioBlock mSharedAudio;
    juce::String             mSharedAudioName;
    int                      mSharedBlockSize { 0 };
    int                      mSharedChannels  { 0 };
    // 1000 / prepared sample rate.  Written in prepare (message thread), read
    // on the audio thread, exactly like the two ints above -- a prepare only
    // happens with the device stopped.
    double                   mMsPerSample     { 0.0 };

    // Raised when a block gave up on the helper, so the NEXT block drains the
    // completion that helper is about to post late.  Without it that stale
    // signal satisfies every subsequent wait instantly and the slot is silent
    // for good -- a transient overload became an unrecoverable dead slot.
    std::atomic<bool> mNeedsResync { false };

    // A WEDGED helper must cost a render ONE ceiling, not one per block.  The
    // offline deadline is a hang detector, not a performance budget, so it is
    // measured in tens of seconds -- and runOfflineLoop drives blocks back to
    // back with no per-render bail, so re-paying it every block turns an export
    // into an unbounded hang (a 3-minute render at 48k/1024 is ~8,400 blocks).
    // A helper that is DEAD rather than wedged is already short-circuited by
    // handleConnectionLost clearing mAlive; this covers the live-but-stuck case
    // the guard actually exists for.  Cleared by prepare / releaseResources.
    std::atomic<bool> mOfflineWedged { false };

    std::atomic<bool> mNonRealtime { false };
    std::atomic<bool> mLoadFailed  { false };

    juce::MemoryBlock  mPendingState;
    juce::WaitableEvent mStateReady { true };

    // Written on the reader thread, read from the message thread -- a bare
    // String here was a data race.
    mutable juce::CriticalSection mErrorLock;
    juce::String mLastError;

    mutable juce::CriticalSection mParamLock;
    std::vector<BridgedParam>     mParams;

    std::atomic<int> mNumPrograms    { 1 };
    std::atomic<int> mCurrentProgram { 0 };
    mutable juce::CriticalSection mProgramLock;
    juce::String mProgramName;

    mutable juce::CriticalSection mTouchLock;
    juce::String mTouchedId, mTouchedName;
    std::atomic<int> mTouchCount { 0 };

    void setError (const juce::String& e)
    {
        const juce::ScopedLock sl (mErrorLock);
        mLastError = e;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SandboxedPluginClient)
};

} // namespace Hosting
