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

class HostedPluginInstance final : public juce::AudioProcessor,
                                   private juce::AudioProcessorListener
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

    // True when this instance is running out-of-process.  Its editor lives in
    // the helper, so the host side supplies a window for the helper to reparent
    // the plugin's UI into rather than creating an editor itself.
    bool isRunningBridged() const noexcept { return mSandbox != nullptr; }
    void openBridgedEditor  (void* parentWindowHandle, int width, int height);
    void closeBridgedEditor();
    // Bridged automation surface (null when in-process): the lanes resolve
    // parameter ids and write values through the client instead of the
    // in-process parameter objects that do not exist on this side.
    class SandboxedPluginClient* getSandboxClient() const noexcept { return mSandbox.get(); }

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

    // Deliberately NO editor of our own.  A juce::AudioProcessorEditor holds a
    // REFERENCE to its processor and calls processor.editorBeingDeleted(this)
    // from its destructor -- so an editor that outlives this instance is a
    // use-after-free, and a rack removal destroys the DSP synchronously while
    // its panel window is still open.  Our surfaces therefore build a
    // HostedPluginEditor (a plain Component) directly instead, and this
    // instance tells it to let go of the plugin's editor before dying.
    juce::AudioProcessorEditor* createEditor() override   { return nullptr; }
    bool hasEditor() const override                      { return false; }

    // Registration so the destructor can reach a live editor.  One at a time --
    // a second editor for the same plugin is what JUCE's VST3 wrapper warns
    // crashes some plugins.
    void attachEditor (class HostedPluginEditor*);
    void detachEditor (class HostedPluginEditor*);

    const juce::String getName() const override          { return mDesc.name; }
    // Forwards to the inner instance -- see the definition; the base class only
    // stores the pointer on THIS object, which left hosted plugins with no
    // transport at all.
    void   setPlayHead (juce::AudioPlayHead*) override;
    // Same forwarding hole as the playhead: the base only flips ITS flag, so a
    // hosted plugin rendered offline exports in realtime mode (and a bridged
    // one raced its wall-clock deadline).  Crosses the seam both ways now.
    void   setNonRealtime (bool) noexcept override;
    bool   acceptsMidi()  const override                 { return mDesc.isInstrument; }
    bool   producesMidi() const override                 { return false; }
    double getTailLengthSeconds() const override;

    // v4: real forwarding on both paths.  In-process reads the inner instance
    // live; bridged reads the client's cache (helper pushes ProgramInfo after
    // load and on every change the plugin reports).  A bridged getProgramName
    // only knows the CURRENT program's name -- that is all the tab/strip
    // naming linkage needs, and a full list sync would be dead weight.
    int  getNumPrograms() override;
    int  getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ── Automation surface (2026-08-02) ─────────────────────────────────────
    // ONE implementation for both hosts of a plugin: the rack adapter
    // (HostedPluginEffect) delegates here, and the Plugins-tab lanes call it
    // directly.  Lanes key on the plugin's OWN stable parameter id, never the
    // index -- index-keyed automation silently repoints every lane when a
    // plugin update inserts or reorders a parameter.
    struct AutomatableParam { juce::String id, name; };
    juce::Array<AutomatableParam> getAutomatableParams() const;
    // Count-only peek for pollers -- getAutomatableParams builds Strings for
    // every parameter, which is not a per-tick cost.
    int getNumKnownParams() const;
    bool  applyParamNorm (const juce::String& paramId, float v01);
    float readParamNorm  (const juce::String& paramId, float fallback) const;

    // "Automate last touched": the parameter the USER moved inside the
    // plugin's own editor (a foreign UI has no right-click hook of ours).
    // In-process: our listener on the inner instance, index captured
    // atomically (plugins may notify from their audio thread) and resolved
    // lazily here.  Bridged: the v5 touch relay.  Empty until a touch.
    void getLastTouchedParam (juce::String& idOut, juce::String& nameOut) const;

    // Monotonic touch counter, the delete prompt's dirty bit: "changed
    // anything" means touches (or a program change) since a baseline
    // snapshot.  State-BLOB compare was tried first and reads dirty on an
    // untouched instance -- many plugins keep volatile bytes (timestamps,
    // editor geometry) in their state.  State-apply storms are excluded on
    // both paths (helper-side window when bridged, mStateApplyMs here).
    int getTouchCount() const noexcept;

    // Fired on the message thread when a BRIDGED plugin's parameter list
    // lands (in-process lists exist at load, so it never fires there).  The
    // lane registrars arm this so a restored project's plugin lanes resolve
    // with no window open and no user interaction -- registration at
    // load-event time structurally saw an empty bridged list.
    std::function<void()> onParamListArrived;

    // Pulls just the PluginDescription back out of a saved blob, so a restore
    // path that has no instance yet can build one.  The FULL description is
    // stored rather than only its identifier, deliberately: a project must keep
    // loading its plugins even if the user has since removed them from the
    // added list, so restore cannot depend on that list.
    static std::unique_ptr<juce::PluginDescription> descriptionFromState (const void* data, int size);

private:
    void instantiate();

    // juce::AudioProcessorListener (in-process touch capture; the bridged
    // path's equivalent lives in the helper).
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override;
    // Qualified: both bases (AudioProcessor + AudioProcessorListener) expose
    // a ChangeDetails name, so the bare token is ambiguous here.
    void audioProcessorChanged (juce::AudioProcessor*,
                                const juce::AudioProcessorListener::ChangeDetails&) override {}

    juce::AudioProcessorParameter* findInnerParam (const juce::String& paramId) const;

    // Non-null exactly when this instance is running BRIDGED.  The two are
    // mutually exclusive: mInner for in-process, mSandbox for out-of-process.
    std::unique_ptr<class SandboxedPluginClient> mSandbox;

    std::atomic<int> mLastTouchedIdx { -1 };
    std::atomic<int> mTouchCount     { 0 };
    std::atomic<std::int64_t> mStateApplyMs { 0 };

    PluginManager&          mPlugins;
    juce::PluginDescription mDesc;
    PluginArch              mArch  { PluginArch::Unknown };
    HostedState             mState { HostedState::FailedToLoad };
    juce::String            mError;
    bool                    mBridgePreferred { false };

    std::unique_ptr<juce::AudioPluginInstance> mInner;

    // Non-owning.  The panel window owns the editor; this is only so the
    // destructor can tell it to release the plugin's editor first.
    class HostedPluginEditor* mLiveEditor { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostedPluginInstance)
};

// Hosts the plugin's own editor, or -- when the plugin is not alive -- draws
// the reason in its place at the same size.  The window stays; only the
// plugin's surface goes away.  That is FL's behaviour and it is the whole
// reason HostedState distinguishes a crash from a deletion.
// A plain Component, NOT an AudioProcessorEditor -- see the note on
// HostedPluginInstance::createEditor.  It hosts the plugin's own editor as a
// child, and when the instance is destroyed under it (a rack removal does that
// synchronously) it drops that child while the instance is still alive, then
// goes inert.  Everything it paints after that comes from cached strings, so it
// never touches a dead owner.
class HostedPluginEditor final : public juce::Component,
                                 private juce::Timer
{
public:
    explicit HostedPluginEditor (HostedPluginInstance&);
    ~HostedPluginEditor() override;

    // Called by ~HostedPluginInstance BEFORE it releases the plugin: deletes the
    // plugin's editor (which JUCE requires to happen first) and leaves this
    // component showing a "removed" marker with no live owner reference.
    void ownerDestroyed();

    void paint (juce::Graphics&) override;
    void resized() override;
    void childBoundsChanged (juce::Component*) override;

    // Fired whenever the plugin's own surface declares a size -- on mount, and
    // again if the plugin resizes itself (VST3's resizeView).  Hosts use it to
    // fit the WINDOW to the plugin rather than leaving dead space around it
    // (Jeff 2026-07-29).  Also fires for the dead marker, so a crashed plugin's
    // window shrinks to the message instead of keeping the plugin's footprint.
    std::function<void(int, int)> onNaturalSizeChanged;

    // BRIDGED path: the helper reported the plugin editor's real size (message
    // thread).  This reply used to be dropped, so a bridged window could only
    // ever show the provisional default hole.
    void remoteEditorSized (int w, int h);

    void parentHierarchyChanged() override;
    void moved() override;

    // ── Stretch (QA-Layout T12 / L17) ────────────────────────────────────────
    // Three cases, and they are genuinely different rather than one behaviour
    // with edge cases:
    //
    //   RESIZABLE, in-process  - the frame size is pushed through the plugin's
    //                            OWN resize path.  Its constrainer decides what
    //                            it accepts and we follow whatever it settles on.
    //   FIXED-SIZE, in-process - the plugin snaps any bounds change back, so the
    //                            surface is SCALED by an AffineTransform instead.
    //                            Aspect preserved and centred, so the result is
    //                            letterbox bars rather than a stretched or
    //                            silently clipped UI.
    //   BRIDGED                - CANNOT scale.  The surface is a native child
    //                            peer holding another process's window; an
    //                            AffineTransform on a JUCE component does not
    //                            reach a peer, which is positioned in raw
    //                            parent-client pixels.  It is centred at its
    //                            natural size and clipped to the frame.
    //
    // The floor exists so "scaled down" never becomes "unusable": below this the
    // window refuses to shrink further instead of rendering an unreadable UI.
    static constexpr float kMinUsableScale = 0.5f;

    // False for the bridged case -- hosts use it to explain the narrowing rather
    // than leaving a window that just refuses to scale with no reason given.
    bool canScaleSurface() const noexcept   { return mRemoteHost == nullptr; }

    // The plugin's OWN declared size, tracked across self-resizes.  Window
    // minimums derive from this * kMinUsableScale.
    int getNaturalWidth()  const noexcept   { return mNaturalW; }
    int getNaturalHeight() const noexcept   { return mNaturalH; }

private:
    void timerCallback() override;
    void buildInner();
    void attachRemoteHost();
    void updateRemoteHostBounds();

    // BRIDGED path only.  A bare Component given its OWN native child peer, so
    // the helper process has a real HWND to reparent the plugin's window into.
    // The plugin's UI is not ours to draw in this mode -- we only supply and
    // position the hole it lives in.
    std::unique_ptr<juce::Component> mRemoteHost;
    bool mRemoteOpened { false };

    HostedPluginInstance& mOwner;
    std::unique_ptr<juce::AudioProcessorEditor> mInner;
    bool mWasAlive   { false };
    bool mOwnerGone  { false };
    // Cached at every rebuild so paint() never dereferences mOwner -- it may be
    // gone by the time we repaint.
    juce::String mMarkerTitle, mMarkerMessage;

    // The plugin's own declared size.  Kept SEPARATE from our bounds because
    // scaling makes the two deliberately differ -- "child size != frame size"
    // used to be the test for a plugin self-resize, and that test is wrong the
    // moment a transform is in play.
    int  mNaturalW { 0 }, mNaturalH { 0 };
    // Guards the resized() -> setBounds -> childBoundsChanged -> host resize
    // -> resized() feedback loop that a resizable plugin would otherwise spin.
    bool mInLayout { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostedPluginEditor)
};

} // namespace Hosting
