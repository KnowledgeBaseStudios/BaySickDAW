#pragma once
#include <JuceHeader.h>
#include "../Hosting/HostedPlugin.h"

class VibeSynthProcessor;

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
                          private juce::Timer
{
public:
    PluginsPage (VibeSynthProcessor& p, int pageIndex);
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
    juce::String getPluginName() const;

    // Preset-name linkage (2026-08-02): the plugin's CURRENT program name when
    // it publishes one (in-process reads live; bridged reads the v4 relay),
    // else the plugin name.  Most modern synths run private preset browsers
    // the host cannot see -- those simply stay on the plugin name.
    juce::String getDisplayName() const;

    // Delegates to EngineRig.  Rebuilds the hosted editor afterwards.
    void selectPlugin (const juce::PluginDescription&);
    // Same, from the identifier the ribbon's "+" dropdown carries.  Resolves
    // against the added list; a no-op if the plugin is no longer on it.
    void selectPluginById (const juce::String& identifier);

    juce::AudioProcessor* getEngineProcessor() const;
    Hosting::HostedPluginInstance* getHosted() const;

    // Fired when a plugin is first chosen -- StandaloneEditor spawns the mixer
    // strip off this, same as the other page types.
    std::function<void()> onEngineSelected;

    // Fired when the pick changes, so the ribbon / roll labels can follow.
    std::function<void()> onPluginChanged;

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
    void timerCallback() override;
    void rebuildEditor();
    void showPicker();

    VibeSynthProcessor& mProcessor;
    int                 mPageIndex { 0 };
    juce::String        mTabName;
    juce::Colour        mPageColor;

    juce::TextButton mPickBtn { "Select plugin..." };

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
