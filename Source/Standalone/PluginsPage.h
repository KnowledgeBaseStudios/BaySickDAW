#pragma once
#include <JuceHeader.h>
#include "../Hosting/HostedPlugin.h"
#include "UndoActions.h"   // QA-UndoCoverage ruling 3a: UndoContext + StructuralOpAction

class BaySickDAWProcessor;

// ── PluginsPage ──────────────────────────────────────────────────────────────
// QA-ModelShell TS6 (BLU-447): the view for one hosted-VST3-instrument tab.
//
// Deliberately the thinnest page in the app.  Every other page type owns knobs
// and layout because it drives OUR engine; a hosted plugin brings its own UI,
// so this page's whole job is to pick a plugin and then get out of the way of
// the plugin's editor.
//
// A VIEW, per TS1: it never constructs an engine.  `selectPlugin` delegates to
// EngineRig, which owns construction, registration and teardown -- so closing
// this window or deleting the page leaves the plugin playing.
// ─────────────────────────────────────────────────────────────────────────────

class PluginsPage final : public juce::Component,
                          private juce::Timer,
                          private juce::ChangeListener
{
public:
    PluginsPage (BaySickDAWProcessor& p, int pageIndex);
    ~PluginsPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    int getPageIndex() const noexcept { return mPageIndex; }

    juce::Colour getPageColor() const noexcept { return mPageColor; }

    void                setTabName (const juce::String& n);
    const juce::String& getTabName() const noexcept { return mTabName; }

    // The plugin's identifier string -- the same value EngineRig stores as the
    // tab's engineType, so "which plugin" needs no separate persistence.
    juce::String getEngineType() const;

    // Human-readable name for the ribbon / roll label; empty until picked.
    // Carries the dead-plugin marker -- see getDisplayName.
    juce::String getPluginName() const;

    // Preset-name linkage (2026-08-02): the plugin's CURRENT program name when
    // it publishes one (in-process reads live; bridged reads the v4 relay),
    // else the plugin name.  Most modern synths run private preset browsers
    // the host cannot see -- those simply stay on the plugin name.
    //
    // While the hosted instance is NOT alive this instead returns the tab's own
    // name with " (missing)" appended -- the same wording the rack slot uses, so
    // the two surfaces cannot describe one condition two ways.  Built on the TAB
    // name rather than the plugin's on purpose: this string rides a rename
    // cascade, and sourcing it from the plugin would replace a name the user
    // chose.  setTabName strips the marker straight back off, so it is display
    // only and nothing the project persists ever carries it.
    //
    // For the same reason it reports the bare TAB name during a marker refresh
    // -- see mInMarkerFire.
    juce::String getDisplayName() const;

    // Rebuild a hosted instance that is no longer alive, keeping its settings
    // (EngineRig::retryDeadPluginTab), then refresh every surface that cached
    // the old one.  True when the rebuilt instance is alive.
    bool retryDeadPlugin();

    // Delegates to selectPluginById via the description's identifier string.
    void selectPlugin (const juce::PluginDescription&);
    // The one route to EngineRig, also used by project restore, undo
    // resurrection and page-preset load.  Rebuilds the hosted editor
    // afterwards.  The rig owns description resolution (added list first, then
    // the description stashed with the saved tab), so this never filters on the
    // added list itself -- doing so would make that fallback unreachable and
    // strand a restored tab with no engine.
    void selectPluginById (const juce::String& identifier);

    juce::AudioProcessor* getEngineProcessor() const;
    Hosting::HostedPluginInstance* getHosted() const;

    // QA-UndoCoverage ruling 3a: one plugin-state swap gesture (page-preset
    // load) = one structural transaction.  Skips the wrap when no plugin is
    // hosted.  Wired by the page creators.
    void setUndoContext (const UndoContext& ctx) { mUndoCtx = ctx; }
    void performChainSwapGesture (const juce::String& label,
                                  const std::function<void()>& op);

    // Fired when a plugin is first chosen -- StandaloneEditor spawns the mixer
    // strip off this, same as the other page types.
    std::function<void()> onEngineSelected;

    // Fired when the pick changes, so the ribbon / roll labels can follow.
    std::function<void()> onPluginChanged;

    // Rename / Duplicate (Jeff, 2026-08-16): the Plugins Menu carries the same
    // Rename / Replace / Duplicate trio as the other page types.
    std::function<void()> onRenameRequested;
    std::function<void()> onDuplicateRequested;

    // G-7 parity with the other page kinds: confirm prompt (offering a
    // page-preset save when a plugin is loaded), then fire onDeleteRequested.
    void requestDelete();
    std::function<void()> onDeleteRequested;

    // Automate (2026-08-02, ruling 1-c): fired with a stable parameter id
    // when the user picks one from the hamburger's Automate submenu or its
    // last-touched fast path.  StandaloneEditor builds the lane pid,
    // (re)registers the tab's applicators, and opens the event editor.
    std::function<void(const juce::String& paramId)> onAutomateParam;

    // Fired when the plugin's discovered parameter count changes -- bridged
    // lists arrive async after load, and lane applicators can only be
    // registered once the params exist (a restored project's plugin lanes
    // would otherwise stay silent until the user opened the Automate menu).
    std::function<void()> onParamListChanged;

    // Title-strip hamburger: Save / Load Page Preset + Delete.
    void showPageActionsMenu (juce::Component* anchor);
    // QA-Layout T15: the title strip's nav buttons dissolved into the Menu
    // dropdown -- the editor injects the window/view entries at the top of
    // the page-actions popup through this hook.
    std::function<void(juce::PopupMenu&)> onBuildWindowNavMenu;
    void savePagePreset (std::function<void()> onSaved = nullptr);
    void loadPagePreset (const juce::File& xml);

public:
    void parentHierarchyChanged() override;   // peer-keyed poll suspend (TS4)

private:
    UndoContext mUndoCtx;   // QA-UndoCoverage ruling 3a
    void timerCallback() override;
    // The added-plugin list changing is the ONE "the user put the plugin back"
    // signal, and it is edge-triggered by design: a plugin that is gone for good
    // must not be retried on a clock forever.
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void rebuildEditor();
    void refreshNameMarker();

    BaySickDAWProcessor& mProcessor;
    int                 mPageIndex { 0 };
    juce::String        mTabName;
    juce::Colour        mPageColor;


    // The plugin's own editor, or its dead marker.  Owned here because the page
    // is the thing on screen; the INSTANCE is owned by EngineRig.
    // A plain Component: HostedPluginEditor is deliberately not an
    // AudioProcessorEditor of the hosted instance (that back-reference cannot
    // safely outlive the instance -- see HostedPluginInstance::createEditor).
    std::unique_ptr<juce::Component> mEditor;

    // Watches for the hosted instance being swapped or dying underneath us.
    juce::AudioProcessor* mBuiltEngine { nullptr };
    bool                  mBuiltAlive  { false };

    // Last display name the poll saw.  Empty = not yet learned; the first
    // arrival seeds quietly so restore state can't stomp a saved tab name.
    juce::String mLastDisplayName;

    // Aliveness as the NAME surfaces last reported it.  Tracked apart from
    // mBuiltAlive (which drives the editor rebuild) and apart from the display
    // name: a project that restores with its plugin already missing never sees
    // the name CHANGE, so the marker cannot ride the quiet-first-arrival watch
    // above.  Deliberately NOT reset by rebuildEditor -- the revival's dead ->
    // alive edge is the one that takes the marker back off.
    bool mNameAlive { true };

    // Set across the marker's whole rename cascade, which reads the name more
    // than once, and hanging off "this fire is decoration, not an edit": it
    // makes getDisplayName report the tab's own name on the revival edge, where
    // the live branch's PROGRAM name would overwrite the name the user typed --
    // the one thing the marker exists not to do.
    bool mInMarkerFire { false };

    // Dirty baseline for the delete prompt: the touch counter + program name
    // at the tab's resting point.  requestDelete offers save-and-delete only
    // when a parameter was touched or the program changed since.  (State-BLOB
    // compare was the first cut and read dirty on an untouched fresh load --
    // volatile state bytes + bridged capture-before-settle.)
    int          mBaselineTouchCount { 0 };
    juce::String mBaselineProgram;
    bool         mBaselineCaptured { false };

    int mLastParamCount { -1 };   // param-list watch (onParamListChanged)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginsPage)
};
