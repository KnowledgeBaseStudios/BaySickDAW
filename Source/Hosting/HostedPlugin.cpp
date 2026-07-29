#include "HostedPlugin.h"
#include "SandboxedPluginClient.h"

namespace Hosting
{

namespace
{
    constexpr int kDeadMarkerW = 420;
    constexpr int kDeadMarkerH = 160;

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
HostedPluginInstance::~HostedPluginInstance() = default;

void HostedPluginInstance::instantiate()
{
    mInner.reset();
    mSandbox.reset();

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

    mState = HostedState::Ok;
    mError = {};
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

void HostedPluginInstance::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (mSandbox != nullptr)
    {
        // A bridged plugin that misses its deadline (or has died) yields
        // silence for THIS slot only -- never a stall on the audio callback.
        if (! mSandbox->processBlock (buffer, midi))
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

juce::AudioProcessorEditor* HostedPluginInstance::createEditor()
{
    return new HostedPluginEditor (*this);
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

    setBridgePreference (xml->getBoolAttribute (kAttrBridged, false));

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
    : juce::AudioProcessorEditor (owner), mOwner (owner)
{
    setOpaque (true);
    buildInner();

    // Watches for the plugin dying under us.  Cheap: one bool compare.
    startTimerHz (4);
}

HostedPluginEditor::~HostedPluginEditor() = default;

void HostedPluginEditor::buildInner()
{
    mInner.reset();
    mWasAlive = mOwner.isAlive();

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
            setSize (mInner->getWidth(), mInner->getHeight());

            if (onNaturalSizeChanged)
                onNaturalSizeChanged (mInner->getWidth(), mInner->getHeight());

            return;
        }
    }

    setSize (kDeadMarkerW, kDeadMarkerH);

    if (onNaturalSizeChanged)
        onNaturalSizeChanged (kDeadMarkerW, kDeadMarkerH);
}

void HostedPluginEditor::timerCallback()
{
    if (mOwner.isAlive() != mWasAlive)
        buildInner();
}

void HostedPluginEditor::resized()
{
    if (mInner != nullptr)
        mInner->setBounds (getLocalBounds());
}

void HostedPluginEditor::childBoundsChanged (juce::Component* child)
{
    // A plugin can resize its own editor at any time (VST3's resizeView).  When
    // it does, the frame follows it rather than clipping -- our resized() would
    // otherwise immediately squash it back to the old size.
    if (child != mInner.get() || mInner == nullptr)
        return;

    if (mInner->getWidth() != getWidth() || mInner->getHeight() != getHeight())
    {
        setSize (mInner->getWidth(), mInner->getHeight());

        if (onNaturalSizeChanged)
            onNaturalSizeChanged (mInner->getWidth(), mInner->getHeight());
    }
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

    g.setColour (juce::Colour (0xffe0e0e8));
    g.setFont (juce::Font (15.0f, juce::Font::bold));
    g.drawText (mOwner.getDescription().name,
                area.removeFromTop (24), juce::Justification::centred, true);

    g.setColour (juce::Colour (0xff808090));
    g.setFont (juce::Font (13.0f));
    g.drawFittedText (mOwner.getStateMessage(), area, juce::Justification::centred, 3);
}

} // namespace Hosting
