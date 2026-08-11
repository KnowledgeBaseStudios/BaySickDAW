#include "HostedPlugin.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)
#include "SandboxedPluginClient.h"
// The bridged path needs the containing window's peer as the parent for the
// plugin's remote surface, and its screen position for the parent-client
// coordinate mapping.
#include "../Standalone/WorkspaceWindow.h"
#include "../MissingFileReport.h"   // refused-plugin restore line (QA-Cleanup)
#include "ScopedPluginDllDirectory.h"   // plugin sibling-DLL resolution (QA-Cleanup)

namespace Hosting
{

namespace
{
    constexpr int kDeadMarkerW = 420;
    constexpr int kDeadMarkerH = 160;

    // Until the helper reports the bridged plugin's declared size.  Deliberately
    // generous: a too-small hole would clip the plugin's UI, and the window
    // shrinks to fit as soon as the real number arrives.
    constexpr int kBridgedDefaultW = 700;
    constexpr int kBridgedDefaultH = 420;

    // Wrapper state tag + attributes.  The plugin's own blob rides as base64 so
    // the whole thing survives inside the rack's / tab's existing XML state,
    // and the full PluginDescription rides as a child element so a restore has
    // something to identify the plugin BY.  Since 2026-08-10 that description is
    // checked against the user's added list before anything is loaded - see
    // descriptionFromState.
    const char* kStateTag    = "HostedPlugin";
    const char* kAttrBridged = "bridged";
    const char* kAttrBlob    = "blob";
}



// ─────────────────────────────────────────────────────────────────────────────
// HostedPluginInstance
// ─────────────────────────────────────────────────────────────────────────────
HostedPluginInstance::HostedPluginInstance (PluginManager& pm, const juce::PluginDescription& d)
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), ! d.isInstrument)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      mPlugins (pm),
      mDesc (d)
{
    mArch = PluginManager::architectureOf (juce::File (d.fileOrIdentifier));
    instantiate();
}

// Out-of-line because SandboxedPluginClient is forward-declared in the header.
HostedPluginInstance::~HostedPluginInstance()
{
    // JUCE requires the plugin's editor to be deleted BEFORE the plugin
    // instance -- VST3PluginInstanceHeadless::cleanup() asserts exactly that.
    // We do not own our editor (the panel window does), and a rack removal
    // destroys this DSP SYNCHRONOUSLY inside EffectRack::packSlotsToTop while
    // that window is still open -- its poll would not notice for another frame.
    // So the editor is told to let go here, on the one path every removal
    // route shares, rather than at each call site.
    if (mLiveEditor != nullptr)
        mLiveEditor->ownerDestroyed();

    mSandbox.reset();
    if (mInner != nullptr) mInner->removeListener (this);
    mInner.reset();
}

void HostedPluginInstance::openBridgedEditor (void* parentWindowHandle, int width, int height)
{
    if (mSandbox != nullptr)
        mSandbox->openEditor (parentWindowHandle, width, height);
}

void HostedPluginInstance::closeBridgedEditor()
{
    if (mSandbox != nullptr)
        mSandbox->closeEditor();
}

void HostedPluginInstance::attachEditor (HostedPluginEditor* e)
{
    mLiveEditor = e;
}

void HostedPluginInstance::detachEditor (HostedPluginEditor* e)
{
    if (mLiveEditor == e)
        mLiveEditor = nullptr;
}

void HostedPluginInstance::instantiate()
{
    if (mInner != nullptr) mInner->removeListener (this);
    mInner.reset();
    mSandbox.reset();
    mLastTouchedIdx.store (-1);   // new instance = new touch history

    // Two tiers, and the forced one is ARCHITECTURE rather than policy: a
    // 64-bit process physically cannot load a 32-bit DLL, so that row has no
    // in-process alternative.  64-bit honours the per-plugin preference.
    const bool wantBridge = isBridgeForced() || mBridgePreferred;

    if (wantBridge)
    {
        auto client = std::make_unique<SandboxedPluginClient>();
        juce::String bridgeErr;

        if (client->start (mDesc, bridgeErr))
        {
            // A crash must stay distinguishable from a deleted slot -- this is
            // what lights the dead marker while the window stays open.
            client->onCrashed = [this] { mState = HostedState::Crashed; };

            // The load result arrives async and used to be INVISIBLE -- a
            // plugin that failed inside the helper looked loaded forever.
            // Marshalled to the message thread by the client.
            client->onLoadResult = [this] (bool ok, juce::String err, bool inst, bool midi)
            {
                juce::ignoreUnused (midi);
                if (! ok)
                {
                    mState = HostedState::FailedToLoad;
                    mError = err.isNotEmpty()
                               ? err
                               : juce::String ("The plugin failed to load in the bridge helper");
                    return;
                }
                // 32-bit metadata correction: the 64-bit scanner cannot open
                // the file, so its description was a filename-only guess.
                if (mDesc.isInstrument != inst)
                {
                    mDesc.isInstrument = inst;
                    mPlugins.refineDescription (mDesc);
                }
            };

            // The bridged editor's real size -- fit the window to the plugin.
            client->onEditorSize = [this] (int w, int h)
            {
                if (mLiveEditor != nullptr)
                    mLiveEditor->remoteEditorSized (w, h);
            };

            // Parameter-list arrival -> lane re-registration (message thread).
            client->onParameterList = [this]
            {
                if (onParamListArrived) onParamListArrived();
            };

            mSandbox = std::move (client);
            mState   = HostedState::Ok;
            mError   = {};
            return;
        }

        // The bridge is the ONLY route for 32-bit, so a failure there is
        // terminal; a 64-bit plugin falls back to running in-process rather
        // than refusing to load over a preference.
        if (isBridgeForced())
        {
            mState = HostedState::NeedsBridge;
            mError = bridgeErr;
            return;
        }
    }

    juce::String err;

   #if JUCE_WINDOWS
    // Must wrap the load itself: the plugin's dependencies are resolved while
    // its module is being brought in, not later.
    const ScopedPluginDllDirectory dllDir { juce::File (mDesc.fileOrIdentifier) };
   #endif

    mInner = mPlugins.formats().createPluginInstance (mDesc,
                                                      getSampleRate() > 0.0 ? getSampleRate() : 44100.0,
                                                      getBlockSize()  > 0   ? getBlockSize()  : 512,
                                                      err);

    if (mInner == nullptr)
    {
        mState = HostedState::FailedToLoad;
        mError = err.isNotEmpty() ? err : juce::String ("The plugin could not be loaded");
        return;
    }

    // Seed the fresh inner with any playhead this object already holds.  A
    // no-op from the constructor (the playhead is null until
    // EngineRig::registerWithProcessor, or HostedPluginEffect's first
    // transport push for a rack slot, calls setPlayHead on this object, which
    // forwards to mInner) -- but instantiate() is ALSO called from
    // setStateInformation's bridge-mode swap, where the transport was attached
    // long ago and neither attach path re-fires for an inner swap
    // (HostedPluginEffect gates on mPlayHeadAttached, the rig sweep on a
    // playhead POINTER change).  Without the seed, a plugin restored from
    // bridged into in-process ran with no transport.
    if (auto* ph = getPlayHead())
        mInner->setPlayHead (ph);

    mInner->addListener (this);   // in-process "last touched" capture
    mState = HostedState::Ok;
    mError = {};
}

void HostedPluginInstance::audioProcessorParameterChanged (juce::AudioProcessor*, int idx, float)
{
    // May fire on the plugin's audio thread -- atomic capture only; the
    // id/name resolution happens lazily in getLastTouchedParam.  Host-driven
    // automation writes go through applyParamNorm's setValueNotifyingHost and
    // echo here too; unlike the bridged path there is no cheap way to tell
    // the echo apart, so an in-process "last touched" can briefly follow a
    // playing lane.  Accepted: the menu is read at a right-click, when
    // playback interaction is deliberate.
    const auto nowMs = (std::int64_t) juce::Time::getMillisecondCounter();
    if (nowMs - mStateApplyMs.load (std::memory_order_relaxed) < 1000)
        return;   // state-apply storm (restore / preset load), not a touch

    mLastTouchedIdx.store (idx, std::memory_order_relaxed);
    mTouchCount.fetch_add (1, std::memory_order_relaxed);
}

int HostedPluginInstance::getTouchCount() const noexcept
{
    if (mSandbox != nullptr)
        return mSandbox->getTouchCount();
    return mTouchCount.load (std::memory_order_relaxed);
}

juce::AudioProcessorParameter* HostedPluginInstance::findInnerParam (const juce::String& paramId) const
{
    if (mInner == nullptr)
        return nullptr;

    for (auto* p : mInner->getParameters())
        if (auto* hp = dynamic_cast<juce::AudioPluginInstance::HostedParameter*> (p))
            if (hp->getParameterID() == paramId)
                return p;

    return nullptr;
}

juce::Array<HostedPluginInstance::AutomatableParam> HostedPluginInstance::getAutomatableParams() const
{
    juce::Array<AutomatableParam> out;

    // BRIDGED: the in-process parameter objects do not exist here -- the list
    // came across the wire at load (v3).
    if (mSandbox != nullptr)
    {
        for (const auto& bp : mSandbox->getParameterList())
            out.add ({ bp.id, bp.name });
        return out;
    }

    if (mInner == nullptr)
        return out;

    for (auto* p : mInner->getParameters())
    {
        if (p == nullptr || ! p->isAutomatable())
            continue;

        if (auto* hp = dynamic_cast<juce::AudioPluginInstance::HostedParameter*> (p))
            out.add ({ hp->getParameterID(), p->getName (64) });
    }

    return out;
}

int HostedPluginInstance::getNumKnownParams() const
{
    if (mSandbox != nullptr) return mSandbox->getNumParameters();
    return mInner != nullptr ? mInner->getParameters().size() : 0;
}

bool HostedPluginInstance::applyParamNorm (const juce::String& paramId, float v01)
{
    // BRIDGED: resolve the id to its index in the wire list and send.  The
    // index is the list order -- the same order the helper enumerated.
    if (mSandbox != nullptr)
    {
        const auto params = mSandbox->getParameterList();
        for (size_t i = 0; i < params.size(); ++i)
        {
            if (params[i].id != paramId) continue;
            mSandbox->setParameter ((int) i, juce::jlimit (0.0f, 1.0f, v01));
            return true;
        }
        return false;
    }

    auto* p = findInnerParam (paramId);

    if (p == nullptr)
        return false;

    // setValueNotifyingHost rather than setValue so the plugin's own editor
    // follows an automated move.
    p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v01));
    return true;
}

float HostedPluginInstance::readParamNorm (const juce::String& paramId, float fallback) const
{
    // Bridged parameter VALUES are not cached host-side -- reads fall back,
    // same as the rack adapter has always behaved.
    auto* p = findInnerParam (paramId);
    return p != nullptr ? p->getValue() : fallback;
}

void HostedPluginInstance::getLastTouchedParam (juce::String& idOut, juce::String& nameOut) const
{
    idOut = {};
    nameOut = {};

    if (mSandbox != nullptr)
    {
        mSandbox->getLastTouchedParam (idOut, nameOut);
        return;
    }

    const int idx = mLastTouchedIdx.load (std::memory_order_relaxed);
    if (mInner == nullptr || idx < 0)
        return;

    const auto& params = mInner->getParameters();
    if (! juce::isPositiveAndBelow (idx, params.size()))
        return;

    auto* p = params[idx];
    if (auto* hp = dynamic_cast<juce::AudioPluginInstance::HostedParameter*> (p))
        idOut = hp->getParameterID();
    else
        idOut = juce::String (idx);
    nameOut = p->getName (64);
}

juce::String HostedPluginInstance::getStateMessage() const
{
    switch (mState)
    {
        case HostedState::Ok:           return {};
        case HostedState::NeedsBridge:  return "32-bit plugin - requires the plugin bridge";
        case HostedState::Crashed:      return "This plugin closed unexpectedly";
        case HostedState::FailedToLoad:
        default:                        return mError.isNotEmpty() ? mError
                                                                   : "The plugin could not be loaded";
    }
}

void HostedPluginInstance::setBridgePreference (bool shouldBridge)
{
    // Forced tier ignores the preference outright rather than storing a value
    // that can never take effect.
    mBridgePreferred = isBridgeForced() ? true : shouldBridge;
}

juce::String HostedPluginInstance::getBridgeLockReason() const
{
    if (isBridgeForced())
        return "32-bit - must run bridged";

    return {};
}

double HostedPluginInstance::getTailLengthSeconds() const
{
    return mInner != nullptr ? mInner->getTailLengthSeconds() : 0.0;
}

int HostedPluginInstance::getNumPrograms()
{
    if (mInner   != nullptr) return mInner->getNumPrograms();
    if (mSandbox != nullptr) return mSandbox->getNumPrograms();
    return 1;
}

int HostedPluginInstance::getCurrentProgram()
{
    if (mInner   != nullptr) return mInner->getCurrentProgram();
    if (mSandbox != nullptr) return mSandbox->getCurrentProgram();
    return 0;
}

void HostedPluginInstance::setCurrentProgram (int i)
{
    // In-process only; no bridged relay -- nothing host-side SETS programs
    // today, the linkage is read-only.
    if (mInner != nullptr) mInner->setCurrentProgram (i);
}

const juce::String HostedPluginInstance::getProgramName (int i)
{
    if (mInner != nullptr) return mInner->getProgramName (i);
    if (mSandbox != nullptr && i == mSandbox->getCurrentProgram())
        return mSandbox->getCurrentProgramName();
    return {};
}

void HostedPluginInstance::prepareToPlay (double sr, int blk)
{
    if (mSandbox != nullptr)
    {
        mSandbox->prepare (sr, blk, getTotalNumOutputChannels());
        setLatencySamples (mSandbox->getLatencySamples());
        return;
    }

    if (mInner == nullptr)
        return;

    // BUS COUNT must match the BUFFER we are going to hand it, not just the
    // channel count of bus 0.
    //
    // setPlayConfigDetails only reshapes bus 0 (AudioProcessor.cpp: it calls
    // setChannelLayoutOfBus for the main input and main output and nothing
    // else).  A MULTI-OUTPUT instrument - Keyscape, Omnisphere, most orchestral
    // libraries - keeps all its other output buses ENABLED through that call.
    // JUCE then reports data.numOutputs = getBusCount(false) into the VST3
    // ProcessData and maps every one of those buses onto the buffer we passed,
    // which only ever has the two channels our wrapper declares.  The plugin
    // receives bus pointers that were never backed by our buffer and writes
    // through them.
    //
    // This is why a plain stereo VST3 was always fine and a big multi-out
    // instrument died the moment audio started flowing.
    // ENABLE every bus, do not disable them.  Disabling was tried and made it
    // WORSE: getBusCount(false) stays at the plugin's real bus count either way
    // (9 for Keyscape), JUCE passes that as data.numOutputs, and a disabled bus
    // then arrives at the plugin with a NULL channel pointer.  A plugin that
    // walks all of its buses writes straight through it -- an access violation
    // at null + a small offset, inside the plugin, on the audio thread.
    //
    // Backed by real storage instead: every bus gets valid pointers, the plugin
    // renders normally, and we take the main pair.  mInnerOutChannels drives the
    // scratch allocation below.
    // A LAYOUT CHANGE IS ILLEGAL WHILE THE PLUGIN IS ACTIVE.  JUCE asserts it
    // outright (canApplyBusesLayout: "someone tried to change the layout while
    // the AudioProcessor is running, call releaseResources first!"), and by the
    // time registerWithProcessor re-prepares, the creation-time prepare has
    // already activated the plugin.  Deactivate, reshape, then prepare.
    mInner->releaseResources();

    // enableAllBuses can legitimately REFUSE - not every plugin supports every
    // bus being on at once.  A refusal is not fatal: we fall back to whatever
    // layout it kept, and the scratch sizing below simply reflects that.
    mInner->enableAllBuses();

    // setRateAndBufferSizeDetails, NOT setPlayConfigDetails.
    //
    // setPlayConfigDetails reshapes bus 0 to the channel count we pass, and it
    // was silently UNDOING enableAllBuses one line above: the diagnostic showed
    // allBusesOn=1 and innerOut=2 in the same breath, which is the layout being
    // opened up and then forced shut again.  We only need the rate and block
    // size here; the layout is settled by the enableAllBuses above.
    mInner->setRateAndBufferSizeDetails (sr, blk);
    mInner->prepareToPlay (sr, blk);

    // Sized HERE, on the message thread.  processBlock must never allocate.
    // Read AFTER the layout is settled and the plugin is prepared - this is the
    // width JUCE will actually address, and the width the scratch must cover.
    mInnerOutChannels = juce::jmax (mInner->getTotalNumOutputChannels(),
                                    mInner->getTotalNumInputChannels());

    if (mInnerOutChannels > getTotalNumOutputChannels())
        mMultiOutScratch.setSize (mInnerOutChannels, juce::jmax (1, blk), false, true, true);
    else
        mMultiOutScratch.setSize (0, 0);

    // SIDE-CHAIN INPUT DISCOVERY.  Bus 0 of the input side is the main input;
    // the first ENABLED bus after it is the side-chain / aux input, and its
    // channels sit at a fixed offset inside the process buffer that JUCE will
    // compute for us.  Resolved here rather than per block because the layout
    // cannot change without another prepare.
    //
    // A plugin with a side-chain always takes the wide-scratch path in
    // processBlock, because mInnerOutChannels is max(out, in) and the side-chain
    // pushes the input count past the 2-channel buffer we are handed.
    mSidechainStartChannel = -1;
    mSidechainChannels     = 0;

    for (int b = 1; b < mInner->getBusCount (true); ++b)
    {
        if (auto* bus = mInner->getBus (true, b); bus != nullptr && bus->isEnabled())
        {
            mSidechainStartChannel = mInner->getChannelIndexInProcessBlockBuffer (true, b, 0);
            mSidechainChannels     = bus->getNumberOfChannels();
            break;
        }
    }


    // BLU-301: the plugin's own reported latency becomes ours, so the rack /
    // bus PDC sweep picks it up through the paths that already exist.
    setLatencySamples (mInner->getLatencySamples());
}

void HostedPluginInstance::releaseResources()
{
    if (mSandbox != nullptr) mSandbox->releaseResources();
    if (mInner   != nullptr) mInner->releaseResources();
}

// TS7 (2026-07-31): THE PLAYHEAD WAS NEVER FORWARDED.  juce::AudioProcessor::
// setPlayHead only stores the pointer on THIS object; `mInner` is a separate
// AudioProcessor whose own getPlayHead() therefore returned null, so a hosted
// plugin had no tempo, no bar/beat and no transport state whatsoever.  That --
// not a controller's MIDI clock -- was why a hosted arpeggiator or synced delay
// had nothing to lock to.
void HostedPluginInstance::setPlayHead (juce::AudioPlayHead* ph)
{
    juce::AudioProcessor::setPlayHead (ph);

    if (mInner != nullptr)
        mInner->setPlayHead (ph);
    // The bridged path cannot take a pointer across a process boundary; it gets
    // the same information as plain values in the shared block's control header.
}

void HostedPluginInstance::setNonRealtime (bool b) noexcept
{
    juce::AudioProcessor::setNonRealtime (b);

    if (mInner   != nullptr) mInner->setNonRealtime (b);
    if (mSandbox != nullptr) mSandbox->setNonRealtime (b);
}

void HostedPluginInstance::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (mSandbox != nullptr)
    {
        // Transport read here rather than stored, so the bridged plugin sees the
        // same position the unbridged one would have read from the playhead.
        SandboxedPluginClient::TransportInfo tp;
        if (auto* ph = getPlayHead())
        {
            if (auto pos = ph->getPosition())
            {
                tp.bpm           = pos->getBpm().orFallback (120.0);
                tp.ppqPosition   = pos->getPpqPosition().orFallback (0.0);
                tp.timeInSamples = pos->getTimeInSamples().orFallback ((juce::int64) 0);
                tp.isPlaying     = pos->getIsPlaying();
                if (auto ts = pos->getTimeSignature())
                {
                    tp.timeSigNum = ts->numerator;
                    tp.timeSigDen = ts->denominator;
                }
            }
        }

        // A bridged plugin that misses its deadline (or has died) yields
        // silence for THIS slot only -- never a stall on the audio callback.
        if (! mSandbox->processBlock (buffer, midi, tp))
            buffer.clear();

        return;
    }

    if (mInner == nullptr)
    {
        // Not loaded: an instrument contributes silence, an effect passes its
        // input through untouched.  Never leave stale buffer content.
        if (mDesc.isInstrument)
            buffer.clear();

        return;
    }

    // MULTI-OUTPUT PLUGINS render through a wider scratch buffer.
    //
    // JUCE passes the plugin data.numOutputs = getBusCount(false) -- the BUS
    // COUNT.  Keyscape reports 9 output buses, and every one of them must be
    // backed by real storage: a bus with no storage arrives as a null channel
    // pointer, and a plugin that walks all of its buses writes through it.  That
    // was the crash - an access violation at null + 0x20, inside the plugin.
    //
    // Diagnosed from the plugin's own numbers rather than inferred: it reported
    // innerOut=2 (so a channel-count check PASSED) while outBuses=9.  Comparing
    // channels missed it entirely; the mismatch was in buses.
    if (mInnerOutChannels > buffer.getNumChannels()
        && mMultiOutScratch.getNumChannels() >= mInnerOutChannels)
    {
        const int n = buffer.getNumSamples();

        if (n <= mMultiOutScratch.getNumSamples())
        {
            mMultiOutScratch.clear();

            // An effect's input rides in on the main bus; an instrument has none.
            for (int ch = 0; ch < juce::jmin (buffer.getNumChannels(),
                                              mMultiOutScratch.getNumChannels()); ++ch)
                mMultiOutScratch.copyFrom (ch, 0, buffer, ch, 0, n);

            // THE SIDE-CHAIN, into the bus the plugin actually reads it from.
            // Without this a hosted compressor / gate / ducker has nothing to key
            // off, which is most of the reason to put one in a rack slot at all.
            // Left as silence when the user has picked no SC source, which is
            // what an unconnected side-chain input should look like.
            if (mSidechain != nullptr && mSidechainStartChannel >= 0)
            {
                const int copyN = juce::jmin (n, mSidechain->getNumSamples());

                for (int ch = 0; ch < juce::jmin (mSidechainChannels,
                                                  mSidechain->getNumChannels()); ++ch)
                {
                    const int dest = mSidechainStartChannel + ch;

                    if (dest < mMultiOutScratch.getNumChannels() && copyN > 0)
                        mMultiOutScratch.copyFrom (dest, 0, *mSidechain, ch, 0, copyN);
                }
            }

            juce::AudioBuffer<float> wide (mMultiOutScratch.getArrayOfWritePointers(),
                                           mMultiOutScratch.getNumChannels(), n);
            mInner->processBlock (wide, midi);

            // The MAIN output bus is bus 0, so its channels are the first ones.
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.copyFrom (ch, 0, mMultiOutScratch, ch, 0, n);

            return;
        }

        // SCRATCH TOO SHORT FOR THIS BLOCK.  Falling through to the narrow call
        // below would hand a multi-out plugin the 2-channel buffer - which IS
        // the access violation this whole path exists to prevent, since the
        // plugin walks buses we did not back with storage.  A block of silence
        // is the only safe answer.
        buffer.clear();
        return;
    }

    // Same reasoning one level up: a plugin that NEEDS the wide buffer must
    // never reach here.  mInnerOutChannels is 0 until prepareToPlay has run, so
    // this also covers a process-before-prepare ordering bug rather than
    // crashing on it.
    if (mInnerOutChannels > buffer.getNumChannels())
    {
        buffer.clear();
        return;
    }

    mInner->processBlock (buffer, midi);
}

void HostedPluginInstance::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement xml (kStateTag);
    xml.setAttribute (kAttrBridged, mBridgePreferred);

    if (auto desc = mDesc.createXml())
        xml.addChildElement (desc.release());

    juce::MemoryBlock inner;

    if (mSandbox != nullptr)      mSandbox->getState (inner);
    else if (mInner != nullptr)   mInner->getStateInformation (inner);

    // A dead instance (missing DLL, failed load, crashed or timed-out helper)
    // returns nothing; falling back to the retained blob is what stops a save
    // from wiping the plugin's settings out of the project.
    if (inner.getSize() == 0)
        inner = mLastKnownState;
    else
        mLastKnownState = inner;

    if (inner.getSize() > 0)
        xml.setAttribute (kAttrBlob, inner.toBase64Encoding());

    copyXmlToBinary (xml, dest);
}

std::unique_ptr<juce::PluginDescription> HostedPluginInstance::descriptionFromState (const void* data, int size)
{
    auto xml = SafeXml::parseBinaryBlob (data, size);

    if (xml == nullptr || ! xml->hasTagName (kStateTag))
        return {};

    auto* descXml = xml->getChildByName ("PLUGIN");
    if (descXml == nullptr)
        return {};

    auto desc = std::make_unique<juce::PluginDescription>();
    if (! desc->loadFromXml (*descXml))
        return {};

    // SECURITY (QA-Cleanup 2026-08-10).  What this returns is handed to
    // createPluginInstance, and PluginDescription::fileOrIdentifier is a full
    // filesystem path that came out of a PROJECT FILE -- a thing users trade.
    // Rebuilding from the blob instead of the added list was a deliberate
    // choice (so a project referencing a since-un-added plugin still opens);
    // accepting an arbitrary path was not, it was simply never looked at.  A
    // project could name any .vst3, including one on a UNC share, and it would
    // load and run silently with the user's privileges.
    //
    // Note the bridge is NOT a mitigation: "sandboxed" there means x86-vs-x64,
    // the helper is a plain CreateProcess with no job object or token
    // restriction, and a 64-bit Plugins tab runs in-process regardless.
    //
    // The check lives HERE, in the one parser, rather than at the three restore
    // call sites -- the two missed SFZ clamps in this same batch are what a
    // per-call-site guard looks like when it goes wrong.
    if (auto* pm = PluginManager::getInstance())
    {
        for (const auto& known : pm->getAddedPlugins())
            if (known.fileOrIdentifier.equalsIgnoreCase (desc->fileOrIdentifier))
                return desc;
    }

    // Refused, not silently dropped: this rides the same dialog a moved-DLL
    // restore already feeds, so the user is told which plugin did not load and
    // can add it deliberately if they want it.
    MissingFileReport::add ("Plugin not in your added list - not loaded",
                            desc->fileOrIdentifier);
    return {};
}

void HostedPluginInstance::setStateInformation (const void* data, int size)
{
    auto xml = SafeXml::parseBinaryBlob (data, size);

    if (xml == nullptr || ! xml->hasTagName (kStateTag))
        return;

    // In-process restore/preset storms are not user touches -- see the
    // parameter listener's suppression window.
    mStateApplyMs.store ((std::int64_t) juce::Time::getMillisecondCounter(),
                         std::memory_order_relaxed);

    const bool wasBridged = (mSandbox != nullptr);
    setBridgePreference (xml->getBoolAttribute (kAttrBridged, false));

    // "Applies on next load" (the locked ruling) -- and THIS restore is the
    // load.  The constructor's instantiate() runs before this state arrives,
    // so without re-instantiating here the saved preference could never take
    // effect on ANY path: the 64-bit bridge toggle was a stored value nothing
    // ever read at a moment it could act.
    const bool wantBridge = isBridgeForced() || mBridgePreferred;
    if (wantBridge != wasBridged)
    {
        instantiate();
        if (getSampleRate() > 0.0 && getBlockSize() > 0)
            prepareToPlay (getSampleRate(), getBlockSize());
    }

    juce::MemoryBlock inner;

    if (! inner.fromBase64Encoding (xml->getStringAttribute (kAttrBlob))
        || inner.getSize() == 0)
        return;

    // Retained BEFORE the dispatch, so a plugin that never came up still carries
    // its saved bytes through to the next save.
    mLastKnownState = inner;

    if (mSandbox != nullptr)
        mSandbox->setState (inner.getData(), (int) inner.getSize());
    else if (mInner != nullptr)
        mInner->setStateInformation (inner.getData(), (int) inner.getSize());
}

// ─────────────────────────────────────────────────────────────────────────────
// HostedPluginEditor
// ─────────────────────────────────────────────────────────────────────────────
HostedPluginEditor::HostedPluginEditor (HostedPluginInstance& owner)
    : mOwner (owner)
{
    setOpaque (true);
    mOwner.attachEditor (this);
    buildInner();

    // Watches for the plugin dying under us.  Cheap: one bool compare.
    startTimerHz (4);
}

HostedPluginEditor::~HostedPluginEditor()
{
    stopTimer();

    if (mRemoteOpened && ! mOwnerGone)
        mOwner.closeBridgedEditor();

    mRemoteHost.reset();

    // Skipped when the owner went first -- it already cleared its own pointer
    // and touching it here would be the use-after-free this design avoids.
    if (! mOwnerGone)
        mOwner.detachEditor (this);
}

void HostedPluginEditor::ownerDestroyed()
{
    stopTimer();

    // Bridged: tell the helper to drop the plugin's window before we lose the
    // ability to talk to it, then discard our end of the hole.
    if (mRemoteOpened)
    {
        mOwner.closeBridgedEditor();
        mRemoteOpened = false;
    }

    mRemoteHost.reset();

    // Order is the whole point: the plugin's editor is deleted while the plugin
    // instance is STILL ALIVE, which is what JUCE's cleanup() assert demands.
    mInner.reset();

    mOwnerGone     = true;
    mWasAlive      = false;
    mMarkerMessage = "This effect was removed";

    setSize (kDeadMarkerW, kDeadMarkerH);
    repaint();
}

void HostedPluginEditor::buildInner()
{
    if (mOwnerGone)
        return;

    mInner.reset();
    mWasAlive = mOwner.isAlive();

    // BRIDGED: the plugin's editor lives in the helper process, so there is no
    // AudioProcessorEditor to create here.  We hand the helper a window and it
    // reparents the plugin's own UI into it.
    if (mWasAlive && mOwner.isRunningBridged())
    {
        mMarkerTitle   = mOwner.getDescription().name;
        mMarkerMessage = mOwner.getStateMessage();

        if (mRemoteHost == nullptr)
        {
            mRemoteHost = std::make_unique<juce::Component> ("BridgedPluginSurface");
            addAndMakeVisible (*mRemoteHost);
        }

        // Provisional until the helper reports the plugin's real size.
        setSize (kBridgedDefaultW, kBridgedDefaultH);
        attachRemoteHost();
        return;
    }

    // Cached now so paint() never dereferences mOwner -- by repaint time the
    // instance may have been destroyed under us.
    mMarkerTitle   = mOwner.getDescription().name;
    mMarkerMessage = mOwner.getStateMessage();

    auto* inner = mOwner.getInner();

    // ONE editor per instance, by contract: createEditorIfNeeded() hands back
    // an EXISTING editor if one is already open, and taking ownership of it
    // twice would double-delete.  Our windowing gives each slot / tab exactly
    // one panel window, and JUCE's own VST3 wrapper warns that a second editor
    // instance crashes some plugins anyway -- so an editor that already exists
    // means someone else owns it, and we show the marker rather than steal it.
    if (mWasAlive && inner != nullptr && inner->hasEditor() && inner->getActiveEditor() == nullptr)
    {
        mInner.reset (inner->createEditorIfNeeded());

        if (mInner != nullptr)
        {
            addAndMakeVisible (*mInner);
            mNaturalW = mInner->getWidth();
            mNaturalH = mInner->getHeight();
            setSize (mNaturalW, mNaturalH);

            if (onNaturalSizeChanged)
                onNaturalSizeChanged (mNaturalW, mNaturalH);

            return;
        }
    }

    // The dead marker is its own natural size -- without this the frame would
    // keep scaling against the size of a plugin that is no longer there.
    mNaturalW = kDeadMarkerW;
    mNaturalH = kDeadMarkerH;
    setSize (kDeadMarkerW, kDeadMarkerH);

    if (onNaturalSizeChanged)
        onNaturalSizeChanged (kDeadMarkerW, kDeadMarkerH);
}

void HostedPluginEditor::timerCallback()
{
    if (mOwnerGone)
        return;

    if (mOwner.isAlive() != mWasAlive)
        buildInner();
}

void HostedPluginEditor::remoteEditorSized (int w, int h)
{
    if (mOwnerGone || mRemoteHost == nullptr || w <= 0 || h <= 0)
        return;

    // SECURITY (QA-Cleanup 2026-08-10): this number comes from the HELPER, and
    // it does not stop at the window -- onNaturalSizeChanged feeds
    // setResizeFloor, which PERSISTS as a minimum and deliberately overrides
    // the workspace clamp.  So an absurd report survives the first (clamped,
    // harmless-looking) resize and wins on the next one.  8K a side is past any
    // real plugin editor.
    constexpr int kMaxEditorEdge = 8192;
    w = juce::jmin (w, kMaxEditorEdge);
    h = juce::jmin (h, kMaxEditorEdge);

    mNaturalW = w;
    mNaturalH = h;
    setSize (w, h);

    if (onNaturalSizeChanged)
        onNaturalSizeChanged (w, h);
}

void HostedPluginEditor::resized()
{
    // Everything below can move the child, and moving the child re-enters here
    // through childBoundsChanged; the flag keeps a self-resize (real, from the
    // plugin) distinguishable from our own layout (not a size change at all).
    const juce::ScopedValueSetter<bool> inLayout (mInLayout, true);

    if (mRemoteHost != nullptr)
    {
        // Bridged: no scaling is possible -- see the header note.  Centre at
        // natural size; updateRemoteHostBounds clips to the frame.
        updateRemoteHostBounds();
        return;
    }

    if (mInner == nullptr)
        return;

    // ── NO SCALING.  Read this before adding any back. ───────────────────────
    //
    // A host cannot scale a hosted VST3's UI with an AffineTransform.  It is not
    // that the transform fails to reach the plugin - it reaches it as a SIZE
    // CHANGE, twice, in a loop.  JUCE's own wrapper resolves the plugin's
    // geometry through the transformed coordinate space
    // (juce_VST3PluginFormat.cpp):
    //
    //   componentToVST3Rect  ->  localAreaToGlobal (r) * dpi
    //   vst3ToComponentRect  ->  getLocalArea (nullptr, r / dpi)
    //
    // Both walk the parent chain and apply every ancestor transform.  So with a
    // scale of s on an ancestor:
    //
    //   * componentMovedOrResized tells the plugin, via view->onSize(), that it
    //     is s * its own pixels.  It re-lays out smaller while its native child
    //     window keeps the old rect - the clipping and tearing.
    //   * resizeToFit() reads view->getSize() back through the INVERSE, so the
    //     editor's logical size becomes natural / s.
    //   * our next fit then computes s' = frame / (natural / s) = s * s.
    //
    // The scale squares itself on every pass.  It survived window close because
    // the plugin INSTANCE outlives the window (engines are model-owned), so
    // view->getSize() reopened at the already-shrunken size and squared again -
    // the surface halving on every reopen with the window size unchanged.
    //
    // IPlugViewContentScaleSupport is not an escape hatch either: it is a DPI
    // hint, plugins that do not implement it ignore it silently, and after
    // resizeToFit() a factor below 1 makes the editor LOGICALLY LARGER, which is
    // the opposite of fitting.  The plugin's own magnify is the only real
    // control, and it belongs to the user.
    //
    // So: the plugin sits at its own size and the window wraps it.
    if (mInner->isResizable())
    {
        // Push the frame size through the plugin's OWN path.  Its constrainer
        // rejects or clamps as it likes, so a plugin with a minimum simply stops
        // shrinking.
        //
        // Not during the layout our own fit triggered - the plugin already told
        // us what it wants and this pass exists to honour it, not to argue with
        // it.
        if (! mRefitting)
            mInner->setBounds (getLocalBounds());

        // READ BACK what the plugin actually accepted.  "Resizable" does not
        // mean "any size": a plugin still has a constrainer and can clamp to its
        // own minimum rather than filling a larger frame.  childBoundsChanged
        // cannot report this - mInLayout is true for the whole of resized() and
        // that callback early-returns on it, deliberately, so OUR layout is
        // never mistaken for a plugin self-resize.
        if (mInner->getWidth() != mNaturalW || mInner->getHeight() != mNaturalH)
        {
            mNaturalW = mInner->getWidth();
            mNaturalH = mInner->getHeight();
            requestWindowFit();
        }
    }
    else
    {
        // Fixed size: the plugin snaps any bounds change back, so it is never
        // told anything.  The window was already fitted to this size when the
        // plugin declared it (buildInner / childBoundsChanged), so there is
        // nothing to negotiate here and nothing that can oscillate.
        mInner->setSize (juce::jmax (1, mNaturalW), juce::jmax (1, mNaturalH));
    }

    positionSurface();
}

void HostedPluginEditor::positionSurface()
{
    if (mInner == nullptr)
        return;

    // Centred while it FITS, so the leftover reads as framing rather than as a
    // broken offset.
    //
    // TOP-LEFT when it does not.  The plugin's UI is a native child window, and
    // Windows clips a child window to its top-level parent's client rect and to
    // NOTHING in between - not to the JUCE components it nominally sits inside.
    // A centred overflow therefore spills upward over the page's menu row and
    // the plugin picker.  Anchoring top-left puts every overflowing edge against
    // a real window edge, where the clip is the one we want.
    const int x = mInner->getWidth()  <= getWidth()
                    ? (getWidth()  - mInner->getWidth())  / 2 : 0;
    const int y = mInner->getHeight() <= getHeight()
                    ? (getHeight() - mInner->getHeight()) / 2 : 0;

    mInner->setTopLeftPosition (x, y);
}

void HostedPluginEditor::requestWindowFit()
{
    if (onNaturalSizeChanged == nullptr)
        return;

    // ASYNC on purpose: this fires the window's sizeToContent, which re-enters
    // resized().  Inline would recurse through the layout we are inside.
    juce::Component::SafePointer<HostedPluginEditor> safe (this);
    const int w = mNaturalW, h = mNaturalH;

    juce::MessageManager::callAsync ([safe, w, h]
    {
        if (safe == nullptr || safe->onNaturalSizeChanged == nullptr)
            return;

        // Suppress the frame-push for the layout THIS fit causes.  Without it a
        // clamping plugin loops: it clamps -> we fit the window -> resized()
        // pushes the new frame back at it -> it clamps again.
        const juce::ScopedValueSetter<bool> noPush (safe->mRefitting, true);
        safe->onNaturalSizeChanged (w, h);
    });
}

void HostedPluginEditor::moved()
{
    // A child peer does not follow its logical parent automatically -- it is
    // positioned in the parent window's client space, so any move of ours has
    // to be pushed onto it explicitly.
    updateRemoteHostBounds();
}

void HostedPluginEditor::parentHierarchyChanged()
{
    // The peer we need as a parent does not exist while this component is being
    // built inside a window that is not yet on the desktop, which is the same
    // deferred-attach problem TS4 hit -- so attaching is retried here.
    if (mRemoteHost != nullptr && ! mRemoteOpened)
        attachRemoteHost();
}

void HostedPluginEditor::attachRemoteHost()
{
    if (mRemoteHost == nullptr || mOwnerGone)
        return;

    auto* win = findParentComponentOfClass<WorkspaceWindow>();

    if (win == nullptr)
        return;

    auto* parentPeer = win->getPeer();

    if (parentPeer == nullptr)
        return;   // retried from parentHierarchyChanged

    if (mRemoteHost->getPeer() == nullptr)
        mRemoteHost->addToDesktop (juce::ComponentPeer::windowIgnoresKeyPresses,
                                   parentPeer->getNativeHandle());

    updateRemoteHostBounds();

    auto* peer = mRemoteHost->getPeer();

    if (peer == nullptr || mRemoteOpened)
        return;

    mOwner.openBridgedEditor (peer->getNativeHandle(),
                              mRemoteHost->getWidth(), mRemoteHost->getHeight());
    mRemoteOpened = true;
}

void HostedPluginEditor::updateRemoteHostBounds()
{
    if (mRemoteHost == nullptr || mRemoteHost->getPeer() == nullptr)
        return;

    auto* win = findParentComponentOfClass<WorkspaceWindow>();

    if (win == nullptr)
        return;

    // LETTERBOX, not stretch (QA-Layout T12).  The hole cannot scale its
    // contents -- it is another process's window reparented into a native child
    // peer, and a JUCE AffineTransform does not reach a peer.  Giving it the
    // whole frame would just enlarge the hole and leave the plugin drawn at
    // natural size in its top-left, with the growth showing as dead space on
    // two sides.  Centring it puts the same leftover on all four sides, which
    // reads as framing instead of misalignment.
    //
    // Centring is right HERE and wrong in positionSurface() because the
    // intersection below shrinks the PEER itself, so an oversized bridged plugin
    // is genuinely clipped to the frame.  The in-process editor's native window
    // is clipped only by the top-level peer, so it has to anchor top-left or its
    // overflow spills over the page's own chrome.
    auto area = getLocalBounds();

    if (mNaturalW > 0 && mNaturalH > 0)
        area = juce::Rectangle<int> (mNaturalW, mNaturalH)
                   .withCentre (area.getCentre())
                   // A frame smaller than the plugin clips rather than
                   // overhanging into the window's chrome.
                   .getIntersection (getLocalBounds());

    // Child peers are positioned in PARENT-CLIENT space -- the contract written
    // into WorkspaceWindow.h.  A child peer has no non-client area, so its
    // parent's client origin IS the window's top-left on screen; converting
    // through the screen is therefore the whole of the mapping.
    const auto screenArea = localAreaToGlobal (area);
    mRemoteHost->setBounds (screenArea - win->getScreenPosition());
}

void HostedPluginEditor::childBoundsChanged (juce::Component* child)
{
    // A plugin can resize its own editor at any time (VST3's resizeView) - the
    // magnify control does exactly this - and the window follows it rather than
    // clipping.
    //
    // The test is against the plugin's LAST DECLARED size, not against our own
    // bounds: a plugin bigger than the workspace gets a frame smaller than
    // itself on purpose, so "differs from the frame" would fire forever.
    // mInLayout additionally excludes the bounds WE just set on a resizable
    // plugin, which is our change, not the plugin's.
    if (mInLayout || mInner == nullptr || child != mInner.get())
        return;

    const int w = mInner->getWidth();
    const int h = mInner->getHeight();

    if (w <= 0 || h <= 0 || (w == mNaturalW && h == mNaturalH))
        return;

    mNaturalW = w;
    mNaturalH = h;

    setSize (w, h);

    if (onNaturalSizeChanged)
        onNaturalSizeChanged (w, h);
}

void HostedPluginEditor::paint (juce::Graphics& g)
{
    // ALWAYS fill, even when the plugin's surface exists.
    //
    // The constructor calls setOpaque(true), which PROMISES JUCE that this
    // component paints every pixel of its bounds - so JUCE does not clear
    // behind it.  Returning early here was safe only while the plugin's surface
    // covered the whole frame.  A plugin that clamps its size, or that the user
    // magnifies smaller, leaves an uncovered band, and with nothing painted
    // there the band showed stale buffer content: visible tearing in the empty
    // space around the plugin.
    g.fillAll (juce::Colour (0xff1c1c1e));

    if (mInner != nullptr)
        return;

    // The dead marker.  Deliberately says WHICH plugin and WHY -- the failure
    // this replaces is a surface silently vanishing with nothing to reload
    // into.
    g.fillAll (juce::Colour (0xff1c1c1e));

    auto area = getLocalBounds().reduced (16);

    // Cached strings only -- mOwner may already be destroyed.
    g.setColour (juce::Colour (0xffe0e0e8));
    g.setFont (juce::Font (15.0f, juce::Font::bold));
    g.drawText (mMarkerTitle, area.removeFromTop (24), juce::Justification::centred, true);

    g.setColour (juce::Colour (0xff808090));
    g.setFont (juce::Font (13.0f));
    g.drawFittedText (mMarkerMessage, area, juce::Justification::centred, 3);
}

} // namespace Hosting
