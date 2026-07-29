#pragma once
#include <JuceHeader.h>
#include "PluginBridgeProtocol.h"

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
//     input into shared memory, rings the doorbell, and waits with a hard
//     deadline; a helper that misses the deadline yields SILENCE for that slot
//     and the block moves on.  A stalled plugin must never become a dropout for
//     the whole app.
//   * Every IPC callback arrives on JUCE's message thread.  State shared with
//     the audio thread is atomic; nothing on the audio path allocates or locks.
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

    // Audio-thread entry.  Returns false when the helper did not answer within
    // the deadline -- caller clears the buffer.  Never blocks unbounded.
    bool processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) noexcept;

    void  setParameter (int index, float value01) noexcept;
    int   getNumParameters() const noexcept { return mNumParameters.load(); }
    int   getLatencySamples() const noexcept { return mLatencySamples.load(); }

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
    std::unique_ptr<juce::InterprocessConnection> mUnusedKeepAlive;   // reserved
    juce::MemoryBlock mSharedAudio;

    // Reply rendezvous for the per-block doorbell.  A WaitableEvent rather than
    // a lock so the audio thread's wait has an explicit timeout.
    juce::WaitableEvent mBlockDone { true };
    std::atomic<bool>   mBlockOk   { false };

    juce::MemoryBlock  mPendingState;
    juce::WaitableEvent mStateReady { true };

    juce::String mLastError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SandboxedPluginClient)
};

} // namespace Hosting
