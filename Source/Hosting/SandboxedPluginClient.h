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
//     and waits on the reply event with a hard deadline; a helper that misses
//     it yields SILENCE for that slot and the block moves on.  Since v3 the
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
    // the deadline -- caller clears the buffer.  Never blocks unbounded.
    bool processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&,
                       const TransportInfo&) noexcept;

    void  setParameter (int index, float value01) noexcept;
    int   getNumParameters() const noexcept { return mNumParameters.load(); }
    int   getLatencySamples() const noexcept { return mLatencySamples.load(); }

    // Forwarded to the helper per block via the control header, so a bridged
    // plugin renders offline exports in offline mode like everything else.
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

    void getState (juce::MemoryBlock& dest);
    void setState (const void* data, int size);

    // Reparents the plugin's editor into our window.  The handle crosses as an
    // integer; the helper does the SetParent on its own side.
    void openEditor (void* parentWindowHandle, int width, int height);
    void closeEditor();

    bool isAlive() const noexcept { return mAlive.load(); }

    // Fired on the MESSAGE THREAD when the helper dies.  HostedPluginInstance
    // turns this into HostedState::Crashed, which is what keeps the window open
    // showing the dead marker instead of closing it (FL's behaviour, and the
    // carve-out TS5's EffectSlotWindow needed).
    std::function<void()> onCrashed;

    // Where the helper binaries live, by architecture.  x86 is what a 32-bit
    // VST3 needs and is the only way it can load at all.
    static juce::File helperExecutable (bool want32Bit);

private:
    void handleMessageFromWorker (const juce::MemoryBlock&) override;
    void handleConnectionLost() override;

    bool sendFramed (Bridge::MessageType, const void* payload, std::uint32_t payloadBytes,
                     const void* trailer = nullptr, std::uint32_t trailerBytes = 0);

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

    void setError (const juce::String& e)
    {
        const juce::ScopedLock sl (mErrorLock);
        mLastError = e;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SandboxedPluginClient)
};

} // namespace Hosting
