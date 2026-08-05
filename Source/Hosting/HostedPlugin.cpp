#include "HostedPlugin.h"
#include "SandboxedPluginClient.h"
// The bridged path needs the containing window's peer as the parent for the
// plugin's remote surface, and its screen position for the parent-client
// coordinate mapping.
#include "../Standalone/WorkspaceWindow.h"

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
    // and the full PluginDescription rides as a child element so a restore can
    // rebuild the instance without consulting the added list.
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

    // NO PLAYHEAD SEED HERE, deliberately.  A first cut seeded one from
    // getPlayHead() at this point; it could never fire, because instantiate() is
    // only ever called from the constructor and an AudioProcessor's playhead is
    // null until someone sets it.  Dead code that read as covering the case.
    // The transport arrives instead when EngineRig::registerWithProcessor (or
    // HostedPluginEffect's first transport push, for a rack slot) calls
    // setPlayHead on this object, which forwards to mInner.
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

    // Same order the JUCE plugin host uses: force the channel config the
    // wrapper actually presents, THEN prepare.
    mInner->setPlayConfigDetails (getTotalNumInputChannels(),
                                  getTotalNumOutputChannels(), sr, blk);
    mInner->prepareToPlay (sr, blk);

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

    if (inner.getSize() > 0)
        xml.setAttribute (kAttrBlob, inner.toBase64Encoding());

    copyXmlToBinary (xml, dest);
}

std::unique_ptr<juce::PluginDescription> HostedPluginInstance::descriptionFromState (const void* data, int size)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, size);

    if (xml == nullptr || ! xml->hasTagName (kStateTag))
        return {};

    if (auto* descXml = xml->getChildByName ("PLUGIN"))
    {
        auto desc = std::make_unique<juce::PluginDescription>();

        if (desc->loadFromXml (*descXml))
            return desc;
    }

    return {};
}

void HostedPluginInstance::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);

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

    if (mInner->isResizable())
    {
        // Push the frame size through the plugin's OWN path.  Its constrainer
        // rejects or clamps as it likes and childBoundsChanged follows the
        // result, so a plugin with a minimum simply stops shrinking.
        mInner->setTransform ({});
        mInner->setBounds (getLocalBounds());
        return;
    }

    // Fixed-size: bounds changes get snapped back by the plugin, so scale.
    const int nw = juce::jmax (1, mNaturalW);
    const int nh = juce::jmax (1, mNaturalH);

    mInner->setTransform ({});
    mInner->setBounds (0, 0, nw, nh);

    const float fit = juce::jmin ((float) getWidth()  / (float) nw,
                                  (float) getHeight() / (float) nh);
    const float scale = juce::jmax (kMinUsableScale, fit);

    // Centre the scaled surface -- the leftover is symmetrical letterboxing,
    // which reads as deliberate framing instead of an off-centre crop.
    const float dx = ((float) getWidth()  - (float) nw * scale) * 0.5f;
    const float dy = ((float) getHeight() - (float) nh * scale) * 0.5f;

    mInner->setTransform (juce::AffineTransform::scale (scale).translated (dx, dy));
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
    // A plugin can resize its own editor at any time (VST3's resizeView), and
    // the frame follows it rather than clipping.
    //
    // The test is against the plugin's LAST DECLARED size, not against our own
    // bounds.  Scaling makes child-size and frame-size differ on purpose, so
    // the old "differs from the frame" test fired on every scaled layout and
    // would have dragged the window back to the plugin's natural size the
    // instant anyone resized it.  mInLayout additionally excludes the bounds
    // WE just set on a resizable plugin, which is our change, not the
    // plugin's.
    if (mInLayout || child != mInner.get() || mInner == nullptr)
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
