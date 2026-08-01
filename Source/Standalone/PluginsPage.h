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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginsPage)
};
