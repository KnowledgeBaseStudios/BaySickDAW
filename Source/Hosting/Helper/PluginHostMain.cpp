#include <JuceHeader.h>
#include "../PluginBridgeProtocol.h"
#include "../BridgeSharedMemory.h"

// ── BaySickPluginHost ────────────────────────────────────────────────────────
// QA-ModelShell TS6 (BLU-302): the sandbox helper.  Hosts exactly ONE plugin
// and does nothing else, which is FL's measured shape -- Jeff bridged two
// plugins and got two processes (2026-07-29).
//
// BUILT TWICE, x64 and x86.  A process can only load a plugin of its own
// architecture, and that is the entire reason a 32-bit VST3 needs a helper: our
// 64-bit app physically cannot LoadLibrary it.
//
// Links ONLY juce_audio_processors + juce_events + juce_gui_basics.  It
// deliberately pulls in none of BaySickDAW's own libraries (sfizz, NAM,
// RubberBand, LAME, WORLD, lunasvg) -- every one of those is an x64 build in
// our tree and would otherwise have to be built x86 as well.  Keeping this
// binary tiny is what makes the 32-bit half cheap.
//
// If it crashes, it crashes ALONE: the host sees the connection drop, marks the
// slot HostedState::Crashed, and keeps the plugin's window open showing a dead
// marker rather than closing it.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    const char* kBridgeUid = "BaySickPluginBridge";
}

class PluginHostWorker final : public juce::ChildProcessWorker,
                               private juce::AsyncUpdater
{
public:
    PluginHostWorker() { mFormats.addFormat (std::make_unique<juce::VST3PluginFormat>()); }

    ~PluginHostWorker() override
    {
        cancelPendingUpdate();
        closeEditor();
        mPlugin.reset();
    }

    void handleMessageFromCoordinator (const juce::MemoryBlock& mb) override
    {
        if (mb.getSize() < sizeof (Hosting::Bridge::Header))
            return;

        Hosting::Bridge::Header h {};
        std::memcpy (&h, mb.getData(), sizeof (h));

        const auto* body      = static_cast<const std::uint8_t*> (mb.getData()) + sizeof (h);
        const auto  bodyBytes = (std::uint32_t) (mb.getSize() - sizeof (h));

        using MT = Hosting::Bridge::MessageType;

        switch (static_cast<MT> (h.type))
        {
            case MT::Handshake:      onHandshake (body, bodyBytes);   break;
            case MT::LoadPlugin:     onLoad      (body, bodyBytes);   break;
            case MT::Prepare:        onPrepare   (body, bodyBytes);   break;
            case MT::Process:        onProcess   (body, bodyBytes);   break;
            case MT::SetParameter:   onSetParam  (body, bodyBytes);   break;
            case MT::GetState:       onGetState();                    break;
            case MT::SetState:       onSetState  (body, bodyBytes);   break;
            case MT::OpenEditor:     onOpenEditor(body, bodyBytes);   break;
            case MT::CloseEditor:    closeEditor();                   break;
            case MT::Shutdown:       juce::JUCEApplication::quit();   break;
            default: break;
        }
    }

    // The coordinator went away (host quit or crashed).  Nothing to salvage --
    // a helper with no host is a leak, so it exits rather than lingering.
    void handleConnectionLost() override { juce::JUCEApplication::quit(); }

private:
    void reply (Hosting::Bridge::MessageType t, const void* payload, std::uint32_t bytes,
                const void* trailer = nullptr, std::uint32_t trailerBytes = 0)
    {
        sendMessageToCoordinator (Hosting::Bridge::frame (t, 0, payload, bytes,
                                                          trailer, trailerBytes));
    }

    void replyError (const juce::String& text)
    {
        const auto utf8 = text.toRawUTF8();
        reply (Hosting::Bridge::MessageType::Error, nullptr, 0,
               utf8, (std::uint32_t) std::strlen (utf8));
    }

    void onHandshake (const std::uint8_t* body, std::uint32_t bytes)
    {
        if (bytes < sizeof (Hosting::Bridge::HandshakePayload))
            return;

        Hosting::Bridge::HandshakePayload hs {};
        std::memcpy (&hs, body, sizeof (hs));

        // A stale helper binary from an older install would otherwise misparse
        // every message; refusing loudly beats decoding garbage.
        if (hs.protocolVersion != Hosting::Bridge::kProtocolVersion)
        {
            replyError ("Plugin bridge version mismatch");
            juce::JUCEApplication::quit();
            return;
        }

        Hosting::Bridge::HandshakePayload out {};
        out.protocolVersion = Hosting::Bridge::kProtocolVersion;
        out.hostArchBits    = (std::uint32_t) (sizeof (void*) * 8);
        reply (Hosting::Bridge::MessageType::HandshakeReply, &out, sizeof (out));
    }

    void onLoad (const std::uint8_t* body, std::uint32_t bytes)
    {
        const auto identifier = juce::String::fromUTF8 ((const char*) body, (int) bytes);

        // Re-scan for the identifier rather than trusting a path: a shell
        // plugin holds several sub-plugins behind one file, and the identifier
        // is what disambiguates them.
        juce::KnownPluginList list;
        juce::OwnedArray<juce::PluginDescription> found;

        for (auto* fmt : mFormats.getFormats())
            fmt->findAllTypesForFile (found, identifierToFile (identifier));

        const juce::PluginDescription* match = nullptr;

        for (auto* d : found)
            if (d->createIdentifierString() == identifier)
                match = d;

        if (match == nullptr)
        {
            replyError ("Plugin not found in helper: " + identifier);
            sendLoadReply (false);
            return;
        }

        juce::String err;
        mPlugin = mFormats.createPluginInstance (*match, 44100.0, 512, err);

        if (mPlugin == nullptr)
        {
            replyError (err.isNotEmpty() ? err : juce::String ("Plugin failed to load"));
            sendLoadReply (false);
            return;
        }

        sendLoadReply (true);
    }

    // The identifier's leading field is the file path, which is all
    // findAllTypesForFile needs; the uid disambiguation happens above.
    static juce::String identifierToFile (const juce::String& identifier)
    {
        return identifier.upToFirstOccurrenceOf ("-", false, false).trim();
    }

    void sendLoadReply (bool ok)
    {
        Hosting::Bridge::LoadReplyPayload r {};
        r.ok                = ok ? 1u : 0u;
        r.numParameters     = mPlugin != nullptr ? (std::uint32_t) mPlugin->getParameters().size() : 0u;
        r.latencySamples    = mPlugin != nullptr ? (std::uint32_t) mPlugin->getLatencySamples() : 0u;
        r.sharedMemoryBytes = 0;
        reply (Hosting::Bridge::MessageType::LoadReply, &r, sizeof (r));
    }

    void onPrepare (const std::uint8_t* body, std::uint32_t bytes)
    {
        if (mPlugin == nullptr || bytes < sizeof (Hosting::Bridge::PreparePayload))
            return;

        Hosting::Bridge::PreparePayload p {};
        std::memcpy (&p, body, sizeof (p));

        mChannels = (int) juce::jmax (1u, p.numChannels);
        mBlock    = (int) juce::jmax (1u, p.maxBlockSize);
        mScratch.setSize (mChannels, mBlock, false, true, true);

        // TS7: the host names its audio mapping in the trailer.  Before this the
        // helper rendered into mScratch, a buffer only IT could see, so nothing
        // it produced ever reached the app -- a bridged plugin was inaudible by
        // construction.  mScratch survives only as the fallback when the mapping
        // cannot be opened, so a failure is silence rather than a crash.
        mShared.close();
        mChannelPtrs.resize ((size_t) mChannels);

        if (bytes > sizeof (Hosting::Bridge::PreparePayload))
        {
            const auto* nameBytes = body + sizeof (Hosting::Bridge::PreparePayload);
            const auto  nameLen   = (int) (bytes - sizeof (Hosting::Bridge::PreparePayload));
            const auto  name      = juce::String::fromUTF8 ((const char*) nameBytes, nameLen);

            if (name.isNotEmpty())
                mShared.open (name, Hosting::Bridge::SharedAudioBlock::bytesFor (mBlock, mChannels));
        }

        mPlugin->setPlayConfigDetails (mChannels, mChannels, p.sampleRate, mBlock);
        mPlugin->prepareToPlay (p.sampleRate, mBlock);
    }

    void onProcess (const std::uint8_t* body, std::uint32_t bytes)
    {
        if (bytes < sizeof (Hosting::Bridge::ProcessPayload))
            return;

        Hosting::Bridge::ProcessPayload p {};
        std::memcpy (&p, body, sizeof (p));

        if (mPlugin != nullptr)
        {
            const int n = (int) juce::jmin ((std::uint32_t) mBlock, p.numSamples);

            // Render straight into the host's mapping -- the plugin's output IS
            // the shared block, so no copy and no second buffer.  Falls back to
            // the private scratch only when the mapping never opened, which is
            // silence at the host end rather than a crash here.
            float* const* chans = mScratch.getArrayOfWritePointers();

            if (mShared.isValid())
            {
                for (int c = 0; c < mChannels; ++c)
                    if (auto* p2 = mShared.channel (c, mBlock))
                        mChannelPtrs[(size_t) c] = p2;
                chans = mChannelPtrs.data();
            }

            juce::AudioBuffer<float> view (chans, mChannels, n);

            // TS7 (2026-07-31): host transport arrives as plain values because
            // nothing with a vtable can cross a process boundary.  Rebuilt into
            // a real AudioPlayHead here so the plugin's arpeggiators and synced
            // effects see a host exactly as they would unbridged.
            mPlayHead.mPos = {};
            mPlayHead.mPos.setBpm (p.bpm);
            mPlayHead.mPos.setPpqPosition (p.ppqPosition);
            mPlayHead.mPos.setTimeInSamples ((juce::int64) p.timeInSamples);
            mPlayHead.mPos.setIsPlaying (p.isPlaying != 0);
            mPlayHead.mPos.setTimeSignature (juce::AudioPlayHead::TimeSignature {
                (int) p.timeSigNumerator, (int) p.timeSigDenominator });
            mPlugin->setPlayHead (&mPlayHead);

            // MIDI trailer: (int32 samplePos, int32 numBytes, bytes) per event.
            // Previously the host sent none at all, so a bridged INSTRUMENT was
            // silent no matter what was played into it.
            mMidi.clear();
            if (p.numMidiBytes > 0
                && bytes >= sizeof (Hosting::Bridge::ProcessPayload) + p.numMidiBytes)
            {
                const std::uint8_t* m = body + sizeof (Hosting::Bridge::ProcessPayload);
                std::uint32_t off = 0;
                while (off + 8 <= p.numMidiBytes)
                {
                    juce::int32 pos = 0, nb = 0;
                    std::memcpy (&pos, m + off,     4);
                    std::memcpy (&nb,  m + off + 4, 4);
                    off += 8;
                    if (nb <= 0 || off + (std::uint32_t) nb > p.numMidiBytes)
                        break;   // truncated or corrupt -- drop the remainder
                    mMidi.addEvent (m + off, nb, pos);
                    off += (std::uint32_t) nb;
                }
            }

            mPlugin->processBlock (view, mMidi);
        }

        reply (Hosting::Bridge::MessageType::ProcessReply, nullptr, 0);
    }

    void onSetParam (const std::uint8_t* body, std::uint32_t bytes)
    {
        if (mPlugin == nullptr || bytes < sizeof (Hosting::Bridge::SetParameterPayload))
            return;

        Hosting::Bridge::SetParameterPayload p {};
        std::memcpy (&p, body, sizeof (p));

        auto& params = mPlugin->getParameters();

        if (juce::isPositiveAndBelow ((int) p.paramIndex, params.size()))
            params[(int) p.paramIndex]->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p.value01));
    }

    void onGetState()
    {
        juce::MemoryBlock mb;

        if (mPlugin != nullptr)
            mPlugin->getStateInformation (mb);

        reply (Hosting::Bridge::MessageType::StateBlob, nullptr, 0,
               mb.getData(), (std::uint32_t) mb.getSize());
    }

    void onSetState (const std::uint8_t* body, std::uint32_t bytes)
    {
        if (mPlugin != nullptr && bytes > 0)
            mPlugin->setStateInformation (body, (int) bytes);
    }

    void onOpenEditor (const std::uint8_t* body, std::uint32_t bytes)
    {
        if (mPlugin == nullptr || bytes < sizeof (Hosting::Bridge::EditorPayload))
            return;

        std::memcpy (&mPendingEditor, body, sizeof (mPendingEditor));

        // Editor construction is message-thread work; the IPC callback may not
        // be on it, so it is marshalled rather than done inline.
        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        closeEditor();

        if (mPlugin == nullptr || ! mPlugin->hasEditor())
            return;

        mEditor.reset (mPlugin->createEditorIfNeeded());

        if (mEditor == nullptr)
            return;

        // Reparent into the host's window: the handle arrived as an integer and
        // is cast back here, on the side that owns the child window.
        auto* parent = (void*) (juce::pointer_sized_uint) mPendingEditor.parentWindowHandle;
        mEditor->setTopLeftPosition (0, 0);
        mEditor->addToDesktop (juce::ComponentPeer::windowIgnoresKeyPresses, parent);
        mEditor->setVisible (true);

        Hosting::Bridge::EditorPayload out {};
        out.parentWindowHandle = mPendingEditor.parentWindowHandle;
        out.width  = mEditor->getWidth();
        out.height = mEditor->getHeight();
        reply (Hosting::Bridge::MessageType::EditorOpened, &out, sizeof (out));
    }

    void closeEditor()
    {
        if (mEditor != nullptr)
        {
            mEditor->removeFromDesktop();
            mEditor.reset();
        }
    }

    juce::AudioPluginFormatManager mFormats;
    std::unique_ptr<juce::AudioPluginInstance>  mPlugin;
    std::unique_ptr<juce::AudioProcessorEditor> mEditor;
    Hosting::Bridge::EditorPayload mPendingEditor {};

    juce::AudioBuffer<float> mScratch;   // fallback only -- see onPrepare
    juce::MidiBuffer         mMidi;

    // The host's audio mapping, plus a stable channel-pointer array so building
    // the AudioBuffer view per block allocates nothing.
    Hosting::Bridge::SharedAudioBlock mShared;
    std::vector<float*>               mChannelPtrs;

    // TS7: the host's transport, republished to the plugin each block.  A member
    // rather than a local because setPlayHead stores the POINTER -- a stack
    // instance would dangle the moment onProcess returned.
    struct BridgePlayHead : juce::AudioPlayHead
    {
        juce::AudioPlayHead::PositionInfo mPos;
        juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override
        { return mPos; }
    } mPlayHead;
    int mChannels { 2 };
    int mBlock    { 512 };
};

class PluginHostApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "BaySickPluginHost"; }
    const juce::String getApplicationVersion() override { return "1.0"; }

    void initialise (const juce::String& commandLine) override
    {
        mWorker = std::make_unique<PluginHostWorker>();

        // Launched by the host only.  Run standalone and it exits immediately
        // rather than sitting in the background as an orphan.
        if (! mWorker->initialiseFromCommandLine (commandLine, kBridgeUid))
        {
            mWorker.reset();
            quit();
        }
    }

    void shutdown() override { mWorker.reset(); }

private:
    std::unique_ptr<PluginHostWorker> mWorker;
};

START_JUCE_APPLICATION (PluginHostApplication)
