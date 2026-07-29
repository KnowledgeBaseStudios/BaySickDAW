#include "SandboxedPluginClient.h"
#include "PluginManager.h"

namespace Hosting
{

namespace
{
    // The command-line id both ends agree on (JUCE requires short alphanumeric).
    const char* kBridgeUid = "BaySickPluginBridge";

    // How long the audio thread will wait for the helper to return a block.
    // Deliberately a HARD ceiling rather than "however long the plugin takes":
    // a bridged plugin that stalls must cost its own slot's audio, never the
    // whole app's callback.  Sized well under a 512-frame block at 44.1k (11.6
    // ms) so a miss degrades to silence rather than to a device xrun.
    constexpr int kBlockDeadlineMs = 4;

    // Handshake / load are message-thread operations and may legitimately take
    // a while (a plugin's own scan-time init).
    constexpr int kStartupTimeoutMs = 15000;
}

SandboxedPluginClient::SandboxedPluginClient() = default;

SandboxedPluginClient::~SandboxedPluginClient()
{
    // Best effort: ask nicely, then kill.  The destructor must not block on a
    // helper that is already wedged.
    if (mAlive.load())
        sendFramed (Bridge::MessageType::Shutdown, nullptr, 0);

    killWorkerProcess();
}

juce::File SandboxedPluginClient::helperExecutable (bool want32Bit)
{
    // Ships beside the app binary.  Two builds, because a helper can only load
    // a plugin of its own architecture -- which is the entire reason a 32-bit
    // VST3 needs one.
    const auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                         .getParentDirectory();

    return dir.getChildFile (want32Bit ? "BaySickPluginHost32.exe"
                                       : "BaySickPluginHost64.exe");
}

bool SandboxedPluginClient::start (const juce::PluginDescription& desc, juce::String& errorOut)
{
    const bool want32 = (PluginManager::architectureOf (juce::File (desc.fileOrIdentifier))
                             == PluginArch::X86);
    const auto exe = helperExecutable (want32);

    if (! exe.existsAsFile())
    {
        errorOut = "Plugin bridge helper not found: " + exe.getFileName();
        return false;
    }

    if (! launchWorkerProcess (exe, kBridgeUid, kStartupTimeoutMs))
    {
        errorOut = "Could not start the plugin bridge helper";
        return false;
    }

    mAlive.store (true);

    Bridge::HandshakePayload hs {};
    hs.protocolVersion = Bridge::kProtocolVersion;
    hs.hostArchBits    = (std::uint32_t) (sizeof (void*) * 8);
    sendFramed (Bridge::MessageType::Handshake, &hs, sizeof (hs));

    // The plugin is named by its identifier string, not by a path -- the helper
    // re-resolves it through its own format manager, so a shell plugin with
    // several sub-plugins lands on the right one.
    const auto id = desc.createIdentifierString().toRawUTF8();
    const auto idBytes = (std::uint32_t) std::strlen (id);
    sendFramed (Bridge::MessageType::LoadPlugin, nullptr, 0, id, idBytes);

    return true;
}

bool SandboxedPluginClient::sendFramed (Bridge::MessageType type,
                                        const void* payload, std::uint32_t payloadBytes,
                                        const void* trailer, std::uint32_t trailerBytes)
{
    if (! mAlive.load())
        return false;

    const auto seq = mSequence.fetch_add (1);
    return sendMessageToWorker (Bridge::frame (type, seq, payload, payloadBytes,
                                               trailer, trailerBytes));
}

void SandboxedPluginClient::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    // The shared audio block is sized here, on the message thread, so the audio
    // path never allocates.  Interleaved-by-channel float layout, host writes
    // input then reads output in place.
    mSharedAudio.setSize ((size_t) maxBlockSize * (size_t) numChannels * sizeof (float), true);

    Bridge::PreparePayload p {};
    p.sampleRate   = sampleRate;
    p.maxBlockSize = (std::uint32_t) maxBlockSize;
    p.numChannels  = (std::uint32_t) numChannels;
    sendFramed (Bridge::MessageType::Prepare, &p, sizeof (p));
}

void SandboxedPluginClient::releaseResources()
{
    mSharedAudio.reset();
}

bool SandboxedPluginClient::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midi) noexcept
{
    if (! mAlive.load (std::memory_order_acquire))
        return false;

    // AUDIO THREAD.  No allocation, no lock, and a bounded wait -- see the
    // threading note in the header.  A miss returns false and the caller
    // clears; it does not retry, because a retry is just a second stall.
    mBlockOk.store (false, std::memory_order_relaxed);
    mBlockDone.reset();

    Bridge::ProcessPayload p {};
    p.numSamples   = (std::uint32_t) buffer.getNumSamples();
    p.numMidiBytes = 0;
    juce::ignoreUnused (midi);

    if (! sendFramed (Bridge::MessageType::Process, &p, sizeof (p)))
        return false;

    if (! mBlockDone.wait (kBlockDeadlineMs))
        return false;

    return mBlockOk.load (std::memory_order_acquire);
}

void SandboxedPluginClient::setParameter (int index, float value01) noexcept
{
    Bridge::SetParameterPayload p {};
    p.paramIndex = (std::uint32_t) index;
    p.value01    = value01;
    sendFramed (Bridge::MessageType::SetParameter, &p, sizeof (p));
}

void SandboxedPluginClient::getState (juce::MemoryBlock& dest)
{
    dest.reset();

    if (! mAlive.load())
        return;

    mStateReady.reset();
    sendFramed (Bridge::MessageType::GetState, nullptr, 0);

    // Message-thread call; a bounded wait is fine here and a dead helper simply
    // yields empty state rather than hanging a project save.
    if (mStateReady.wait (2000))
        dest = mPendingState;
}

void SandboxedPluginClient::setState (const void* data, int size)
{
    if (data == nullptr || size <= 0)
        return;

    sendFramed (Bridge::MessageType::SetState, nullptr, 0, data, (std::uint32_t) size);
}

void SandboxedPluginClient::openEditor (void* parentWindowHandle, int width, int height)
{
    Bridge::EditorPayload p {};
    // Never a pointer on the wire -- the helper casts it back on its side.
    p.parentWindowHandle = (std::uint64_t) (juce::pointer_sized_uint) parentWindowHandle;
    p.width  = (std::int32_t) width;
    p.height = (std::int32_t) height;
    sendFramed (Bridge::MessageType::OpenEditor, &p, sizeof (p));
}

void SandboxedPluginClient::closeEditor()
{
    sendFramed (Bridge::MessageType::CloseEditor, nullptr, 0);
}

void SandboxedPluginClient::handleMessageFromWorker (const juce::MemoryBlock& mb)
{
    if (mb.getSize() < sizeof (Bridge::Header))
        return;

    Bridge::Header h {};
    std::memcpy (&h, mb.getData(), sizeof (h));

    const auto* body = static_cast<const std::uint8_t*> (mb.getData()) + sizeof (h);
    const auto  bodyBytes = (std::uint32_t) (mb.getSize() - sizeof (h));

    switch (static_cast<Bridge::MessageType> (h.type))
    {
        case Bridge::MessageType::HandshakeReply:
            break;

        case Bridge::MessageType::LoadReply:
        {
            if (bodyBytes < sizeof (Bridge::LoadReplyPayload))
                break;

            Bridge::LoadReplyPayload r {};
            std::memcpy (&r, body, sizeof (r));
            mNumParameters .store ((int) r.numParameters);
            mLatencySamples.store ((int) r.latencySamples);
            break;
        }

        case Bridge::MessageType::ProcessReply:
            mBlockOk.store (true, std::memory_order_release);
            mBlockDone.signal();
            break;

        case Bridge::MessageType::StateBlob:
            mPendingState.replaceAll (body, bodyBytes);
            mStateReady.signal();
            break;

        case Bridge::MessageType::Error:
            mLastError = juce::String::fromUTF8 ((const char*) body, (int) bodyBytes);
            break;

        default:
            break;
    }
}

void SandboxedPluginClient::handleConnectionLost()
{
    // The helper died.  Release anything the audio thread may be waiting on
    // FIRST, so a block in flight fails fast instead of burning its deadline.
    mAlive.store (false, std::memory_order_release);
    mBlockOk.store (false, std::memory_order_release);
    mBlockDone.signal();
    mStateReady.signal();

    if (onCrashed)
        onCrashed();
}

} // namespace Hosting
