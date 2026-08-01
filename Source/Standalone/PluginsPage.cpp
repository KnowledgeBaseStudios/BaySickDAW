#include "PluginsPage.h"
#include "../PluginProcessor.h"
#include "../EngineRig.h"
#include "SharedUI.h"   // VC palette
#include "SlotComponent.h"
#include "WorkspaceWindow.h"   // sizeToContent for hosted-plugin surfaces

namespace
{
    constexpr int kPickBtnH = 26;
    constexpr int kPickGap  = 6;    // between the picker button and the plugin
    constexpr int kEdge     = 8;    // page inset, matches resized()
}

PluginsPage::PluginsPage (VibeSynthProcessor& p, int pageIndex)
    : mProcessor (p), mPageIndex (pageIndex)
{
    mTabName   = "Plugin " + juce::String (pageIndex + 1);
    mPageColor = VC::Purple;

    // Tab identity is created at page birth (TS1 convention) -- the model owns
    // the tab whether or not a plugin has been picked yet.
    mProcessor.engineRig().addTab (TabKind::Plugins, mPageIndex, mTabName);

    mPickBtn.onClick = [this] { showPicker(); };
    addAndMakeVisible (mPickBtn);

    rebuildEditor();
    startTimerHz (4);
}

// Peer-keyed suspend, the shell's convention (TS4): a page whose window is
// closed has no peer, and its view-sync poll was the one page poll still
// running headless.
void PluginsPage::parentHierarchyChanged()
{
    if (getPeer() != nullptr)
    {
        if (! isTimerRunning()) startTimerHz (4);
    }
    else if (isTimerRunning())
    {
        stopTimer();
    }
}

PluginsPage::~PluginsPage()
{
    stopTimer();
    // The editor dies with the view; the ENGINE does not -- EngineRig owns it
    // and onTabClosed is what removes the tab.
    mEditor.reset();
}

juce::String PluginsPage::getEngineType() const
{
    if (auto* t = mProcessor.engineRig().findTab (TabKind::Plugins, mPageIndex))
        return t->engineType;

    return {};
}

juce::AudioProcessor* PluginsPage::getEngineProcessor() const
{
    return mProcessor.engineRig().engineFor (TabKind::Plugins, mPageIndex);
}

Hosting::HostedPluginInstance* PluginsPage::getHosted() const
{
    return dynamic_cast<Hosting::HostedPluginInstance*> (getEngineProcessor());
}

juce::String PluginsPage::getPluginName() const
{
    if (auto* h = getHosted())
        return h->getDescription().name;

    return {};
}

void PluginsPage::setTabName (const juce::String& n)
{
    mTabName = n;
}

void PluginsPage::showPicker()
{
    auto* pm = Hosting::PluginManager::getInstance();

    juce::PopupMenu m;
    juce::Array<juce::PluginDescription> instruments;

    if (pm != nullptr)
        instruments = pm->getAddedInstruments();   // already alphabetical

    if (instruments.isEmpty())
    {
        m.addSectionHeader ("VST Plugins");
        m.addItem (1, "None added - see Options > Plugins", false, false);
    }
    else
    {
        for (int i = 0; i < instruments.size(); ++i)
            m.addItem (i + 1, instruments.getReference (i).name);
    }

    juce::Component::SafePointer<PluginsPage> safeThis (this);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&mPickBtn),
        [safeThis, instruments] (int result)
        {
            auto* self = safeThis.getComponent();

            if (self == nullptr || result < 1 || result > instruments.size())
                return;

            self->selectPlugin (instruments.getReference (result - 1));
        });
}

void PluginsPage::selectPlugin (const juce::PluginDescription& desc)
{
    const bool wasEmpty = (getEngineProcessor() == nullptr);

    // The identifier string IS the engineType -- construction, registration and
    // teardown all belong to the rig.
    mProcessor.engineRig().setEngineType (TabKind::Plugins, mPageIndex,
                                          desc.createIdentifierString());

    rebuildEditor();

    if (wasEmpty && getEngineProcessor() != nullptr && onEngineSelected)
        onEngineSelected();

    if (onPluginChanged)
        onPluginChanged();
}

void PluginsPage::selectPluginById (const juce::String& identifier)
{
    auto* pm = Hosting::PluginManager::getInstance();

    if (pm == nullptr || identifier.isEmpty())
        return;

    if (auto desc = pm->findAdded (identifier))
        selectPlugin (*desc);
}

void PluginsPage::rebuildEditor()
{
    // Destroy before building: a hosted plugin has exactly ONE editor instance,
    // so an overlapping rebuild would ask for an editor the outgoing one still
    // holds (the same trap closed in EffectSlotWindow::buildPanel).
    mEditor.reset();

    auto* eng = getEngineProcessor();
    mBuiltEngine = eng;
    mBuiltAlive  = false;

    if (eng == nullptr)
    {
        mPickBtn.setButtonText ("Select plugin...");
        resized();
        return;
    }

    if (auto* h = dynamic_cast<Hosting::HostedPluginInstance*> (eng))
        mBuiltAlive = h->isAlive();

    mPickBtn.setButtonText (getPluginName().isNotEmpty() ? getPluginName()
                                                         : juce::String ("Select plugin..."));

    // Built DIRECTLY, not via createEditorIfNeeded: HostedPluginEditor is a
    // plain Component on purpose, because an AudioProcessorEditor of the hosted
    // instance cannot safely outlive it (see HostedPluginInstance::createEditor).
    if (auto* hostedInst = dynamic_cast<Hosting::HostedPluginInstance*> (eng))
    {
        auto ed = std::make_unique<Hosting::HostedPluginEditor> (*hostedInst);

        // Fit the WINDOW to the plugin's own surface rather than leaving dead
        // space around it (Jeff 2026-07-29).  The plugin may also resize itself
        // later, which is why this is a callback and not a one-shot read.
        ed->onNaturalSizeChanged = [this] (int w, int h)
        {
            if (auto* win = findParentComponentOfClass<WorkspaceWindow>())
                win->sizeToContent (juce::jmax (240, w + 2 * kEdge),
                                    h + kPickBtnH + kPickGap + 2 * kEdge);
        };

        mEditor = std::move (ed);
        addAndMakeVisible (*mEditor);
    }

    resized();
}

void PluginsPage::timerCallback()
{
    auto* eng = getEngineProcessor();

    if (eng != mBuiltEngine)
    {
        rebuildEditor();
        return;
    }

    // A crash swaps the plugin's surface for the dead marker without the engine
    // pointer changing, so aliveness is polled separately from identity.
    if (auto* h = dynamic_cast<Hosting::HostedPluginInstance*> (eng))
        if (h->isAlive() != mBuiltAlive)
            rebuildEditor();
}

void PluginsPage::paint (juce::Graphics& g)
{
    g.fillAll (VC::Bg);

    if (mEditor != nullptr)
        return;

    g.setColour (VC::TextDim);
    g.setFont (juce::Font (14.0f));
    g.drawText ("No plugin loaded", getLocalBounds().reduced (12),
                juce::Justification::centred, true);
}

void PluginsPage::resized()
{
    auto b = getLocalBounds().reduced (kEdge);

    mPickBtn.setBounds (b.removeFromTop (kPickBtnH).removeFromLeft (240));
    b.removeFromTop (kPickGap);

    if (mEditor != nullptr)
        mEditor->setBounds (b);
}
