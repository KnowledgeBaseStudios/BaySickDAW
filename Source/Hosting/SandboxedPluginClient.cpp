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
    mLoadFailed.store (false);

    Bridge::HandshakePayload hs {};
    hs.protocolVersion = Bridge::kProtocolVersion;
    hs.hostArchBits    = (std::uint32_t) (sizeof (void*) * 8);
    sendFramed (Bridge::MessageType::Handshake, &hs, sizeof (hs));

    // v3 trailer: PATH first, then the identifier, '\n'-separated.  The
    // identifier alone was useless to the helper -- it contains NO path (it is
    // format-name-uid), so the old "parse the file out of it" could never load
    // anything.  The path is what findAllTypesForFile needs; the identifier
    // still disambiguates a shell plugin's sub-plugins.
    //
    // NAMED locals: the old code took toRawUTF8() of a TEMPORARY String and
    // used the pointer after it died.
    const juce::String payload = desc.fileOrIdentifier + "\n"
                               + desc.createIdentifierString();
    const auto* utf8  = payload.toRawUTF8();
    const auto  bytes = (std::uint32_t) std::strlen (utf8);
    sendFramed (Bridge::MessageType::LoadPlugin, nullptr, 0, utf8, bytes);

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
    // The shared audio block is created here, on the message thread, so the
    // audio path never allocates.  Channel-major float layout; the host writes
    // input, the helper processes in place, the host reads output back.
    //
    // A FRESH NAME PER PREPARE.  Re-creating a mapping under a name the helper
    // still has open would hand it the OLD view while we wrote to the new one,
    // which is a silent wrong-audio bug rather than a failure.  The name travels
    // with the Prepare that resizes it, so both ends always agree.
    mSharedBlockSize = maxBlockSize;
    mSharedChannels  = numChannels;
    mSharedAudioName = "Local\\BaySickPluginBridge_" + juce::Uuid().toDashedString();

    const auto bytes = Bridge::SharedAudioBlock::bytesFor (maxBlockSize, numChannels);

    if (! mSharedAudio.create (mSharedAudioName, bytes))
    {
        // No shared block means no audio path; processBlock's isValid() test
        // then yields silence for this slot rather than reading a null view.
        setError ("Could not create the plugin bridge audio block");
        return;
    }

    Bridge::PreparePayload p {};
    p.sampleRate   = sampleRate;
    p.maxBlockSize = (std::uint32_t) maxBlockSize;
    p.numChannels  = (std::uint32_t) numChannels;

    // The mapping NAME rides as the trailer, the same shape LoadPlugin uses for
    // the plugin identifier -- so PreparePayload keeps its asserted layout.
    const auto nameUtf8  = mSharedAudioName.toRawUTF8();
    const auto nameBytes = (std::uint32_t) std::strlen (nameUtf8);
    sendFramed (Bridge::MessageType::Prepare, &p, sizeof (p), nameUtf8, nameBytes);
}

void SandboxedPluginClient::releaseResources()
{
    mSharedAudio.close();
    mSharedBlockSize = 0;
    mSharedChannels  = 0;
}

bool SandboxedPluginClient::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midi,
                                          const TransportInfo& tp) noexcept
{
    if (! mAlive.load (std::memory_order_acquire))
        return false;

    // No shared block = no audio path.  Returning false yields silence for this
    // slot, which is the same contract as a missed deadline.
    if (! mSharedAudio.isValid())
        return false;

    const int n  = buffer.getNumSamples();
    const int nc = juce::jmin (buffer.getNumChannels(), mSharedChannels);

    auto* ctl = mSharedAudio.control();

    if (n <= 0 || n > mSharedBlockSize || nc <= 0 || ctl == nullptr)
        return false;

    // AUDIO THREAD.  v3: NOTHING here touches the pipe.  The old path framed a
    // MemoryBlock (allocation) and handed it to the pipe write, which blocks up
    // to the pipe TIMEOUT when the helper stalls -- 15 seconds on the audio
    // thread.  Now: raw stores into the mapping, one SetEvent, one bounded
    // WaitForSingleObject.  A miss returns false and the caller clears; it does
    // not retry, because a retry is just a second stall.
    //
    // INPUT IN.  An instrument's buffer is silence here and the copy is wasted,
    // but branching on isInstrument would mean the helper reading whatever the
    // last block left in the mapping -- the same stale-buffer class of bug that
    // bit the freeze render twice this batch.  A memcpy of a few KB is cheaper
    // than that failure mode.
    for (int c = 0; c < nc; ++c)
        if (auto* dst = mSharedAudio.channel (c, mSharedBlockSize))
            std::memcpy (dst, buffer.getReadPointer (c), (size_t) n * sizeof (float));

    // MIDI straight into the control area.  Hand-serialised (int32 samplePos,
    // int32 numBytes, bytes) rather than copying JUCE's MidiBuffer storage,
    // because the helper may be a 32-bit process and this protocol does not
    // trust cross-architecture layout.
    std::uint32_t midiBytes = 0;
    for (const auto meta : midi)
    {
        const auto len = (std::uint32_t) meta.numBytes;
        if (midiBytes + 8 + len > Bridge::kMaxMidiBytesPerBlock)
            break;   // bounded: a runaway buffer must not overrun the fixed area

        const juce::int32 pos = (juce::int32) meta.samplePosition;
        const juce::int32 nb  = (juce::int32) len;
        std::memcpy (ctl->midi + midiBytes,     &pos, 4);
        std::memcpy (ctl->midi + midiBytes + 4, &nb,  4);
        std::memcpy (ctl->midi + midiBytes + 8, meta.data, len);
        midiBytes += 8 + len;
    }

    const auto seq = mSequence.fetch_add (1) + 1;

    ctl->numSamples         = (std::uint32_t) n;
    ctl->numMidiBytes       = midiBytes;
    ctl->nonRealtime        = mNonRealtime.load (std::memory_order_acquire) ? 1u : 0u;
    ctl->bpm                = tp.bpm;
    ctl->ppqPosition        = tp.ppqPosition;
    ctl->timeInSamples      = (std::int64_t) tp.timeInSamples;
    ctl->isPlaying          = tp.isPlaying ? 1u : 0u;
    ctl->timeSigNumerator   = (std::uint32_t) juce::jmax (1, tp.timeSigNum);
    ctl->timeSigDenominator = (std::uint32_t) juce::jmax (1, tp.timeSigDen);
    ctl->replyOk            = 0;
    // Sequence LAST of the control stores, then the event: the helper checks it
    // echoes back, so a reply belonging to an EARLIER block (a late helper
    // waking on a stale request) can never be mistaken for this one -- the
    // desync the old path's ignored sequence field allowed to persist forever.
    ctl->sequence           = seq;

    mSharedAudio.signalRequest();

    if (! mSharedAudio.waitDone (kBlockDeadlineMs))
        return false;

    if (ctl->replyOk == 0 || ctl->replySequence != seq)
        return false;

    // OUTPUT BACK.  Only after the reply -- the helper writes in place, so
    // reading before the rendezvous would race it.  Channels the helper does not
    // cover are cleared rather than left holding this block's INPUT, which for
    // an instrument would echo the strip back at itself.
    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        auto* src = c < nc ? mSharedAudio.channel (c, mSharedBlockSize) : nullptr;

        if (src != nullptr) std::memcpy (buffer.getWritePointer (c), src, (size_t) n * sizeof (float));
        else                buffer.clear (c, 0, n);
    }

    return true;
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
        {
            // HOST-SIDE VERSION CHECK (TS7, 2026-07-31).  The helper already
            // refuses a version it does not know, but this direction was a bare
            // `break` -- so a helper NEWER than the app, or one that skipped its
            // own check, would have been trusted and every message misparsed
            // silently.  A protocol whose layout is asserted rather than trusted
            // should not take the peer's word for its version either.
            if (bodyBytes < sizeof (Bridge::HandshakePayload))
            {
                setError ("Plugin bridge handshake was malformed");
                mAlive.store (false, std::memory_order_release);
                break;
            }

            Bridge::HandshakePayload hs {};
            std::memcpy (&hs, body, sizeof (hs));

            if (hs.protocolVersion != Bridge::kProtocolVersion)
            {
                setError ("Plugin bridge version mismatch - helper reports "
                          + juce::String ((int) hs.protocolVersion) + ", app expects "
                          + juce::String ((int) Bridge::kProtocolVersion));
                mAlive.store (false, std::memory_order_release);
            }
            break;
        }

        case Bridge::MessageType::LoadReply:
        {
            if (bodyBytes < sizeof (Bridge::LoadReplyPayload))
                break;

            Bridge::LoadReplyPayload r {};
            std::memcpy (&r, body, sizeof (r));
            mNumParameters .store ((int) r.numParameters);
            mLatencySamples.store ((int) r.latencySamples);
            // The ok flag was read and DROPPED here -- a plugin that failed in
            // the helper was indistinguishable from one that loaded.
            mLoadFailed.store (r.ok == 0, std::memory_order_release);

            if (onLoadResult)
            {
                // READER THREAD here; the receiver updates UI-facing state.
                juce::MessageManager::callAsync (
                    [cb = onLoadResult, ok = (r.ok != 0), err = getLastError(),
                     inst = (r.isInstrument != 0), midi = (r.acceptsMidi != 0)]
                    { cb (ok, err, inst, midi); });
            }
            break;
        }

        case Bridge::MessageType::EditorOpened:
        {
            // The bridged editor's REAL size.  This reply used to fall into the
            // default arm, so the window could never fit the plugin.
            if (bodyBytes < sizeof (Bridge::EditorPayload))
                break;

            Bridge::EditorPayload e {};
            std::memcpy (&e, body, sizeof (e));

            if (onEditorSize)
                juce::MessageManager::callAsync (
                    [cb = onEditorSize, w = (int) e.width, h = (int) e.height]
                    { cb (w, h); });
            break;
        }

        case Bridge::MessageType::ParameterList:
        {
            // One line per parameter, index implicit by order: id '\t' name.
            std::vector<BridgedParam> parsed;
            juce::StringArray lines;
            lines.addLines (juce::String::fromUTF8 ((const char*) body, (int) bodyBytes));
            for (const auto& line : lines)
            {
                if (line.isEmpty()) continue;
                BridgedParam bp;
                bp.id   = line.upToFirstOccurrenceOf ("\t", false, false);
                bp.name = line.fromFirstOccurrenceOf ("\t", false, false);
                parsed.push_back (std::move (bp));
            }
            const juce::ScopedLock sl (mParamLock);
            mParams = std::move (parsed);
            break;
        }

        case Bridge::MessageType::StateBlob:
            mPendingState.replaceAll (body, bodyBytes);
            mStateReady.signal();
            break;

        case Bridge::MessageType::Error:
            setError (juce::String::fromUTF8 ((const char*) body, (int) bodyBytes));
            break;

        default:
            break;
    }
}

void SandboxedPluginClient::handleConnectionLost()
{
    // The helper died.  The audio thread's rendezvous needs no unsticking --
    // its wait is the 4 ms deadline and a dead helper simply never signals.
    mAlive.store (false, std::memory_order_release);
    mStateReady.signal();

    if (onCrashed)
        onCrashed();
}

} // namespace Hosting
