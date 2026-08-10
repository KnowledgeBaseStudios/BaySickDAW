#include "SandboxedPluginClient.h"
#include "PluginManager.h"

#include <cmath>

namespace Hosting
{

namespace
{
    // The command-line id both ends agree on (JUCE requires short alphanumeric).
    const char* kBridgeUid = "BaySickPluginBridge";

    // How long the audio thread will wait for the helper to return a block.
    // Still a HARD ceiling -- a bridged plugin that stalls must cost its own
    // slot's audio, never the whole app's callback -- but DERIVED rather than
    // constant, because the helper's work scales with the block size while a
    // constant does not.  The old 4 ms was a different rule at every buffer
    // setting: 138% of realtime at 128 frames / 44.1k (Jeff's own setting,
    // where it holds the callback for 1.4 block periods and xruns the DEVICE
    // instead of muting one slot) and 8.6% at 2048, where any ordinary plugin
    // misses it and the export or the strip goes silent.
    //
    // A fraction of the LIVE block period is the invariant that holds at both
    // ends: the helper always gets the same share of realtime, and the ceiling
    // is always under one block period, so a stall costs this slot and never
    // the device.  Three quarters leaves the rest of the app a quarter of the
    // callback, and reproduces the old 4 ms exactly at 256 frames / 44.1k --
    // the smallest buffer at which the old constant's own "well under a block"
    // claim was ever true.
    constexpr double kLiveDeadlineFraction = 0.75;

    // The rendezvous wait takes whole milliseconds (WaitForSingleObject), so
    // under ~1.3 ms of block period the fraction rounds away and this floor is
    // what is actually waited.  At 32 frames / 44.1k that floor is longer than
    // the block period; the integer wait API leaves no way to express less.
    constexpr int kMinLiveDeadlineMs = 1;

    // Bounds the pre-request drain (below).  The helper answers a request at
    // most once, so one stale completion is all there can ever be; the cap is
    // there so no amount of event-signal weirdness can spin the audio thread.
    constexpr int kMaxStaleDrains = 4;

    // OFFLINE ceiling.  An export or freeze renders on a worker with the device
    // suspended, so there is no callback to starve and no reason to hold the
    // live budget at all: offline is allowed to take as long as the plugin
    // takes.  Deliberately NOT derived from the block period -- this is a
    // wedged-helper hang guard, not a per-block budget, so it is a duration
    // that stays the same whatever the render block is.  BOUNDED on purpose
    // rather than infinite: a dead helper must fail the render rather than
    // hang the app with no way out.
    constexpr int kOfflineBlockDeadlineMs = 30000;

    // Handshake / load are message-thread operations and may legitimately take
    // a while (a plugin's own scan-time init).
    constexpr int kStartupTimeoutMs = 15000;
}

SandboxedPluginClient::SandboxedPluginClient() = default;

SandboxedPluginClient::~SandboxedPluginClient()
{
    // mTearingDown goes up before anything disconnects, because killWorkerProcess
    // reaches handleConnectionLost through the same chain a real crash takes and
    // ordinary teardown must not report a crash.
    //
    // THREAD SAFETY: mLife is deliberately NOT reset here.  The IPC reader thread
    // builds weak copies of that same shared_ptr object while this runs, and the
    // standard's atomicity guarantee covers the control block's counts, not the
    // shared_ptr's own pointer members -- a concurrent reset() is a data race that
    // can land a weak-count increment on an already-freed control block.
    // killWorkerProcess -> disconnect joins the reader thread, so the member
    // destruction that follows this body retires the token with no other thread
    // able to touch it, and a marshalled callback still finds it expired: it
    // cannot dispatch until the message thread returns to its queue, which is
    // after the whole destructor.
    mTearingDown.store (true, std::memory_order_release);

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
    // The live rendezvous ceiling is a share of the block PERIOD, so the rate
    // has to be kept: a device change re-prepares here and the deadline moves
    // with it, and a short final block gets a proportionally shorter wait.
    mMsPerSample     = sampleRate > 0.0 ? 1000.0 / sampleRate : 0.0;
    // A fresh mapping is a fresh helper: whatever wedged the last render cannot
    // still be true, so the slot gets to render again.
    mOfflineWedged.store (false, std::memory_order_release);
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
    mMsPerSample     = 0.0;
    // The events die with the mapping, so nothing can be left latched for the
    // next prepare to trip over.
    mNeedsResync.store (false, std::memory_order_release);
    mOfflineWedged.store (false, std::memory_order_release);
}

int SandboxedPluginClient::liveDeadlineMs (int numSamples) const noexcept
{
    return juce::jmax (kMinLiveDeadlineMs,
                       (int) ((double) numSamples * mMsPerSample * kLiveDeadlineFraction));
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
    // thread.  Now: raw stores into the mapping, one SetEvent, and waits that
    // are bounded in total by this block's deadline.  A miss returns false and
    // the caller clears; the request is never re-issued, because a second
    // request for the same block is just a second stall.
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

    // ONE load, feeding both the wire flag and the deadline: if the two could
    // disagree within a block the helper would render in a mode the host is not
    // waiting for.
    const bool offline = mNonRealtime.load (std::memory_order_acquire);

    // Once a render has proved this helper wedged, every later block fails the
    // same way -- so fail them for free instead of paying the hang ceiling again.
    if (offline && mOfflineWedged.load (std::memory_order_acquire))
        return false;

    ctl->numSamples         = (std::uint32_t) n;
    ctl->numMidiBytes       = midiBytes;
    ctl->nonRealtime        = offline ? 1u : 0u;
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

    // RESYNC, and this is what keeps ONE missed block from muting the slot for
    // the rest of the session.  Both rendezvous events are auto-reset and the
    // protocol is strictly one block at a time, so a completion the helper
    // posted AFTER we gave up is still latched when the next block starts: it
    // satisfies the next wait instantly, its echoed sequence cannot match, and
    // every following block fails identically -- silence with no meter and no
    // error until the device is reopened.  Drained BEFORE the request so it can
    // never eat this block's own reply, and bounded so the audio thread cannot
    // spin.
    if (mNeedsResync.exchange (false, std::memory_order_acq_rel))
        for (int i = 0; i < kMaxStaleDrains && mSharedAudio.waitDone (0); ++i)
            {}

    mSharedAudio.signalRequest();

    const int    deadlineMs = offline ? kOfflineBlockDeadlineMs : liveDeadlineMs (n);
    const double startMs    = juce::Time::getMillisecondCounterHiRes();
    bool         answered   = false;

    // The other half of the resync: a late reply that arrives inside THIS
    // block's window is consumed and rejected on its sequence, then the wait
    // RESUMES for whatever budget is left rather than surrendering the block.
    // getMillisecondCounterHiRes is QueryPerformanceCounter -- no allocation,
    // no lock, safe here.
    for (;;)
    {
        const double leftMs = (double) deadlineMs
                            - (juce::Time::getMillisecondCounterHiRes() - startMs);

        if (leftMs <= 0.0 || ! mSharedAudio.waitDone ((int) std::ceil (leftMs)))
            break;

        if (ctl->replyOk != 0 && ctl->replySequence == seq)
        {
            answered = true;
            break;
        }
    }

    if (! answered)
    {
        mNeedsResync.store (true, std::memory_order_release);
        if (offline)
            mOfflineWedged.store (true, std::memory_order_release);
        return false;
    }

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
                // Weak token per the header's marshalled-callback rule: the
                // consumer OWNS this client, so a reply dispatched after it dies
                // would write through a dangling owner pointer.
                juce::MessageManager::callAsync (
                    [cb = onLoadResult, ok = (r.ok != 0), err = getLastError(),
                     inst = (r.isInstrument != 0), midi = (r.acceptsMidi != 0),
                     life = std::weak_ptr<char> (mLife)]
                    {
                        if (life.expired())
                            return;

                        cb (ok, err, inst, midi);
                    });
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

            // READER THREAD here -- weak token per the header's rule.
            if (onEditorSize)
                juce::MessageManager::callAsync (
                    [cb = onEditorSize, w = (int) e.width, h = (int) e.height,
                     life = std::weak_ptr<char> (mLife)]
                    {
                        if (life.expired())
                            return;

                        cb (w, h);
                    });
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
            {
                const juce::ScopedLock sl (mParamLock);
                mParams = std::move (parsed);
            }
            // READER THREAD here -- marshal like onLoadResult, weak token and all.
            if (onParameterList)
                juce::MessageManager::callAsync (
                    [cb = onParameterList, life = std::weak_ptr<char> (mLife)]
                    {
                        if (life.expired())
                            return;

                        cb();
                    });
            break;
        }

        case Bridge::MessageType::StateBlob:
            mPendingState.replaceAll (body, bodyBytes);
            mStateReady.signal();
            break;

        case Bridge::MessageType::ParamTouched:
        {
            // v5: the user moved a control inside the plugin's own editor.
            // Cached for the "Automate last touched" menu; poll-read.
            if (bodyBytes < sizeof (Bridge::ParamTouchedPayload))
                break;

            const auto* txt = body + sizeof (Bridge::ParamTouchedPayload);
            const auto  len = bodyBytes - (std::uint32_t) sizeof (Bridge::ParamTouchedPayload);
            const auto  both = juce::String::fromUTF8 ((const char*) txt, (int) len);

            {
                const juce::ScopedLock sl (mTouchLock);
                mTouchedId   = both.upToFirstOccurrenceOf ("\t", false, false);
                mTouchedName = both.fromFirstOccurrenceOf ("\t", false, false);
            }
            mTouchCount.fetch_add (1);
            break;
        }

        case Bridge::MessageType::ProgramInfo:
        {
            // v4: payload + the CURRENT program's name as trailer.  Cached for
            // poll-reads (the Plugins page compares per tick); no marshalled
            // callback needed.
            if (bodyBytes < sizeof (Bridge::ProgramInfoPayload))
                break;

            Bridge::ProgramInfoPayload p {};
            std::memcpy (&p, body, sizeof (p));
            mNumPrograms   .store ((int) p.numPrograms);
            mCurrentProgram.store ((int) p.currentProgram);

            const auto* nameBytes = body + sizeof (p);
            const auto  nameLen   = bodyBytes - (std::uint32_t) sizeof (p);
            const juce::ScopedLock sl (mProgramLock);
            mProgramName = juce::String::fromUTF8 ((const char*) nameBytes, (int) nameLen);
            break;
        }

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
    // its wait is the block deadline and a dead helper simply never signals.
    // These two must run on WHATEVER thread got here: the processBlock
    // short-circuit and getState's bounded wait both depend on them.
    mAlive.store (false, std::memory_order_release);
    mStateReady.signal();

    // Deliberate teardown is not a crash.
    if (mTearingDown.load (std::memory_order_acquire))
        return;

    // MARSHALLED: this arrives on whichever thread noticed -- the IPC reader
    // thread when the pipe closes, the message thread when the ping times out --
    // while the consumer flips UI-facing state.  The weak token is the lifetime
    // guard: this client is a MEMBER of that consumer, so a callback that
    // outlives it would write through a dangling owner pointer.
    if (onCrashed)
        juce::MessageManager::callAsync (
            [cb = onCrashed, life = std::weak_ptr<char> (mLife)]
            {
                if (! life.expired())
                    cb();
            });
}

} // namespace Hosting
