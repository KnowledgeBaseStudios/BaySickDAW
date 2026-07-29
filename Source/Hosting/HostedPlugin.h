#pragma once
#include <JuceHeader.h>
#include "PluginManager.h"

// ── Hosting::HostedPluginInstance ────────────────────────────────────────────
// QA-ModelShell TS6: the ONE type every surface talks to when it wants a hosted
// plugin -- the FX rack slot (BLU-300), the Plugins tab engine (BLU-447), the
// editor windows, the state blobs and the automation lanes.
//
// WHY THIS EXISTS AS A SEAM, and why it lands BEFORE its consumers rather than
// after: BLU-302's crash sandbox stopped being optional the moment 32-bit VST3
// came into scope (a 64-bit process cannot load a 32-bit DLL, so the bridge IS
// the 32-bit support).  Written the plan's original way -- every consumer built
// directly against juce::AudioPluginInstance, sandbox bolted on last -- each of
// those surfaces would have had to be re-pointed when the sandbox arrived.
// With this seam in place first, BLU-302 ADDS an implementation behind an
// existing interface instead of refactoring six surfaces.
//
// It is a juce::AudioProcessor deliberately: TS1's generic engine slot already
// stores unique_ptr<juce::AudioProcessor> + ownedStages, so a hosted instrument
// is "just another engine" with no new plumbing -- exactly the shape TS1 task 7
// promised TS6 would consume.  The rack's DSPBase adapter wraps one of these.
// ─────────────────────────────────────────────────────────────────────────────

namespace Hosting
{

// Why a hosted plugin is not currently producing audio.  These stay DISTINCT on
// purpose -- FL's measured behaviour (Jeff, 2026-07-29) keeps a crashed
// plugin's window open with a "plugin closed" message rather than closing it,
// and TS5's EffectSlotWindow closes itself when its target stops resolving.
// Merging "the user deleted this effect" with "this plugin died" would make a
// crash look like a deletion and silently take the window with it.
enum class HostedState
{
    Ok,            // loaded and running
    FailedToLoad,  // the file is there but would not instantiate
    NeedsBridge,   // architecture mismatch -- waiting on BLU-302's helper
    Crashed        // was running, died (BLU-302 reports this over the wire)
};

class HostedPluginInstance final : public juce::AudioProcessor
{
public:
    HostedPluginInstance (PluginManager&, const juce::PluginDescription&);
    ~HostedPluginInstance() override;

    // ── Identity + health ───────────────────────────────────────────────────
    const juce::PluginDescription& getDescription() const noexcept { return mDesc; }
    HostedState  getHostedState()   const noexcept { return mState; }
    juce::String getStateMessage()  const;

    // The dead-plugin carve-out's predicate.  A window asks THIS, not "does my
    // slot still resolve" -- the two answers differ exactly in the crash case.
    bool isAlive() const noexcept { return mState == HostedState::Ok; }

    juce::AudioPluginInstance* getInner() const noexcept { return mInner.get(); }

    // ── Bridging (BLU-302) ──────────────────────────────────────────────────
    // Two tiers, and the forced one is ARCHITECTURE rather than policy: a
    // 64-bit process physically cannot load a 32-bit DLL, so that row has no
    // alternative.  64-bit is unbridged by default with the toggle available.
    PluginArch   getArch()             const noexcept { return mArch; }
    bool         isBridgeForced()      const noexcept { return mArch == PluginArch::X86; }
    bool         getBridgePreference() const noexcept { return mBridgePreferred; }
    void         setBridgePreference (bool);

    // Empty when the toggle is usable.  Non-empty means "show it, disabled,
    // with this text" -- never hide it, or a 32-bit plugin looks broken for no
    // stated reason.
    juce::String getBridgeLockReason() const;

    // ── juce::AudioProcessor ────────────────────────────────────────────────
    void prepareToPlay (double, int) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }

    const juce::String getName() const override          { return mDesc.name; }
    bool   acceptsMidi()  const override                 { return mDesc.isInstrument; }
    bool   producesMidi() const override                 { return false; }
    double getTailLengthSeconds() const override;

    int  getNumPrograms() override                       { return 1; }
    int  getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override                {}
    const juce::String getProgramName (int) override     { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Pulls just the PluginDescription back out of a saved blob, so a restore
    // path that has no instance yet can build one.  The FULL description is
    // stored rather than only its identifier, deliberately: a project must keep
    // loading its plugins even if the user has since removed them from the
    // added list, so restore cannot depend on that list.
    static std::unique_ptr<juce::PluginDescription> descriptionFromState (const void* data, int size);

private:
    void instantiate();

    // Non-null exactly when this instance is running BRIDGED.  The two are
    // mutually exclusive: mInner for in-process, mSandbox for out-of-process.
    std::unique_ptr<class SandboxedPluginClient> mSandbox;

    PluginManager&          mPlugins;
    juce::PluginDescription mDesc;
    PluginArch              mArch  { PluginArch::Unknown };
    HostedState             mState { HostedState::FailedToLoad };
    juce::String            mError;
    bool                    mBridgePreferred { false };

    std::unique_ptr<juce::AudioPluginInstance> mInner;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostedPluginInstance)
};

// Hosts the plugin's own editor, or -- when the plugin is not alive -- draws
// the reason in its place at the same size.  The window stays; only the
// plugin's surface goes away.  That is FL's behaviour and it is the whole
// reason HostedState distinguishes a crash from a deletion.
class HostedPluginEditor final : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit HostedPluginEditor (HostedPluginInstance&);
    ~HostedPluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void childBoundsChanged (juce::Component*) override;

    // Fired whenever the plugin's own surface declares a size -- on mount, and
    // again if the plugin resizes itself (VST3's resizeView).  Hosts use it to
    // fit the WINDOW to the plugin rather than leaving dead space around it
    // (Jeff 2026-07-29).  Also fires for the dead marker, so a crashed plugin's
    // window shrinks to the message instead of keeping the plugin's footprint.
    std::function<void(int, int)> onNaturalSizeChanged;

private:
    void timerCallback() override;
    void buildInner();

    HostedPluginInstance& mOwner;
    std::unique_ptr<juce::AudioProcessorEditor> mInner;
    bool mWasAlive { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostedPluginEditor)
};

} // namespace Hosting
