#include "PluginsPage.h"
#include "../PluginProcessor.h"
#include "../EngineRig.h"
#include "SharedUI.h"   // VC palette
#include "SlotComponent.h"
#include "WorkspaceWindow.h"   // sizeToContent for hosted-plugin surfaces
#include "PagePresetIO.h"

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

juce::String PluginsPage::getDisplayName() const
{
    if (auto* h = getHosted())
    {
        const auto prog = h->getProgramName (h->getCurrentProgram());
        if (prog.isNotEmpty()) return prog;
    }
    return getPluginName();
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
    mLastDisplayName.clear();   // next poll re-learns quietly for the new engine
    mBaselineCaptured = false;  // new engine = new dirty baseline
    mLastParamCount   = -1;     // re-fire onParamListChanged for the new engine

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
        auto* edRaw = ed.get();
        ed->onNaturalSizeChanged = [this, edRaw] (int w, int h)
        {
            auto* win = findParentComponentOfClass<WorkspaceWindow>();
            if (win == nullptr) return;

            const int chromeW = 2 * kEdge;
            const int chromeH = kPickBtnH + kPickGap + 2 * kEdge;

            win->sizeToContent (juce::jmax (240, w + chromeW), h + chromeH);

            // QA-Layout T12: floor from the minimum usable scale, and the
            // CHROME does not scale -- only the plugin surface does, so the
            // picker row and edges keep their full height in the floor.
            const float floorScale = edRaw->canScaleSurface()
                                       ? Hosting::HostedPluginEditor::kMinUsableScale
                                       : 1.0f;
            win->setResizeFloor (juce::jmax (240, (int) ((float) w * floorScale) + chromeW),
                                 (int) ((float) h * floorScale) + chromeH);
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

    // Dirty baseline: one capture per engine, at its first alive poll.
    if (! mBaselineCaptured)
        if (auto* h = getHosted(); h != nullptr && h->isAlive())
        {
            mBaselineTouchCount = h->getTouchCount();
            mBaselineProgram    = getDisplayName();
            mBaselineCaptured   = true;
        }

    // Param-list watch: bridged lists arrive async after load; lane
    // registration re-runs on every count change so a restored project's
    // plugin lanes resolve without user interaction.
    if (auto* h = getHosted())
    {
        const int n = h->getNumKnownParams();
        if (n != mLastParamCount)
        {
            mLastParamCount = n;
            if (onParamListChanged) onParamListChanged();
        }
    }

    // Preset-name linkage: fire the rename cascade only when the display name
    // CHANGES after first being learned.  The first arrival is load / restore
    // state -- renaming on it would stomp a saved custom tab name, which the
    // engine pages' preset flow never does.
    if (getHosted() != nullptr)
    {
        const auto dn = getDisplayName();
        if (dn.isNotEmpty() && dn != mLastDisplayName)
        {
            const bool firstLearn = mLastDisplayName.isEmpty();
            mLastDisplayName = dn;
            if (! firstLearn && onPluginChanged)
                onPluginChanged();
        }
    }
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

void PluginsPage::requestDelete()
{
    juce::Component::SafePointer<PluginsPage> safeThis (this);
    auto fireDelete = [safeThis] {
        if (auto* p = safeThis.getComponent())
            if (p->onDeleteRequested) p->onDeleteRequested();
    };

    const juce::String warning =
        "Deleting this plugin tab removes its Mixer Strip, Effects Rack, "
        "and Piano Roll.\n"
        "The plugin itself stays installed and can be added again.";

    // Save-and-delete is offered only when something CHANGED since the
    // resting baseline: a parameter touch inside the plugin, or a program
    // switch.  An untouched tab gets the plain confirm.
    bool dirty = false;
    if (auto* h = getHosted(); h != nullptr && h->isAlive() && mBaselineCaptured)
        dirty = (h->getTouchCount() != mBaselineTouchCount)
             || (getDisplayName()   != mBaselineProgram);

    if (dirty)
    {
        const juce::String dirtyWarning = warning
            + "\n\n\"Save Page Preset & Delete\" writes the entire page state "
              "(plugin settings + effects rack + strip settings) to disk "
              "first, then deletes the tab.";

        auto* aw = new juce::AlertWindow (
            "Delete Plugin", dirtyWarning, juce::AlertWindow::QuestionIcon);
        aw->addButton ("Save Page Preset & Delete", 1,
                       juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Delete",                    2);
        aw->addButton ("Cancel",                    0,
                       juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create (
            [safeThis, aw, fireDelete] (int r)
            {
                std::unique_ptr<juce::AlertWindow> own (aw);
                if (r == 0 || ! safeThis) return;
                if (r == 1) safeThis->savePagePreset (fireDelete);   // chained
                else if (r == 2) fireDelete();                        // delete only
            }), false);
        return;
    }

    auto* aw = new juce::AlertWindow (
        "Delete Plugin", warning, juce::AlertWindow::WarningIcon);
    aw->addButton ("Delete", 1);
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [aw, fireDelete] (int r)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (r == 1) fireDelete();
        }), false);
}

void PluginsPage::savePagePreset (std::function<void()> onSaved)
{
    auto* aw = new juce::AlertWindow ("Save Page Preset",
                                       "Enter a name for this plugin page preset:",
                                       juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", getPluginName().isNotEmpty() ? getPluginName()
                                                            : juce::String ("My Plugin"));
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PluginsPage> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, aw, onSaved] (int r)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (r != 1 || ! safeThis) return;

            const juce::String name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;

            auto dir = PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Plugins);
            dir.createDirectory();
            auto target = dir.getChildFile (name + ".xml");
            int n = 2;
            while (target.exists())
                target = dir.getChildFile (name + " (" + juce::String (n++) + ").xml");

            // engineType = the plugin identifier string, so load knows which
            // plugin to instantiate before applying state.  Hosted plugins
            // have no APVTS prefix.
            const juce::String stripPrefix =
                "mixer_plugin_" + juce::String (safeThis->mPageIndex);
            const juce::String xml = PagePresetIO::exportPagePreset (
                safeThis->mProcessor,
                PagePresetIO::PageKind::Plugins,
                safeThis->mPageIndex,
                stripPrefix,
                safeThis->getEngineProcessor(),
                safeThis->getEngineType(),
                {});
            if (xml.isNotEmpty())
                target.replaceWithText (xml);

            // Saved = clean: the delete prompt goes back to the plain confirm.
            if (auto* h = safeThis->getHosted(); h != nullptr && h->isAlive())
            {
                safeThis->mBaselineTouchCount = h->getTouchCount();
                safeThis->mBaselineProgram    = safeThis->getDisplayName();
                safeThis->mBaselineCaptured   = true;
            }

            if (onSaved) onSaved();
        }), false);
}

void PluginsPage::loadPagePreset (const juce::File& xml)
{
    if (! xml.existsAsFile()) return;

    // Two-step, mirroring project restore: instantiate the preset's plugin
    // through the rig FIRST, then apply state (the bridge load path holds
    // early state until the remote instance is ready).
    const juce::String wantedId = PagePresetIO::peekEngineType (xml);
    if (wantedId.isNotEmpty() && getEngineType() != wantedId)
        selectPluginById (wantedId);

    if (wantedId.isNotEmpty() && getEngineType() != wantedId)
    {
        // selectPluginById is a no-op when the plugin isn't on the added list.
        auto* aw = new juce::AlertWindow ("Load Page Preset",
            "This preset uses a plugin that is not in your added list.\n"
            "Add it under Options > Plugins, then load the preset again.",
            juce::AlertWindow::WarningIcon);
        aw->addButton ("OK", 0);
        aw->enterModalState (true, nullptr, true);
        return;
    }

    const juce::String stripPrefix = "mixer_plugin_" + juce::String (mPageIndex);
    PagePresetIO::importPagePreset (mProcessor,
                                     PagePresetIO::PageKind::Plugins,
                                     mPageIndex,
                                     stripPrefix,
                                     getEngineProcessor(),
                                     {},
                                     [] (int) { return true; },
                                     xml.loadFileAsString());

    // A just-loaded page preset is a clean resting point for the delete
    // prompt's dirty check.
    if (auto* h = getHosted(); h != nullptr && h->isAlive())
    {
        mBaselineTouchCount = h->getTouchCount();
        mBaselineProgram    = getDisplayName();
        mBaselineCaptured   = true;
    }
}

void PluginsPage::showPageActionsMenu (juce::Component* anchor)
{
    constexpr int kIdSavePagePreset = 100;
    constexpr int kIdDeleteTab      = 101;
    constexpr int kIdAutomateLast   = 102;
    constexpr int kIdLoadBase       = 1000;
    constexpr int kIdAutoBase       = 20000;

    juce::PopupMenu menu;
    if (onBuildWindowNavMenu) { onBuildWindowNavMenu (menu); menu.addSeparator(); }
    menu.addItem (kIdSavePagePreset, "Save Page Preset As...", getHosted() != nullptr);

    juce::Array<juce::File> presetXmls;
    {
        juce::PopupMenu loadSub;
        const auto root = PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Plugins);
        if (root.isDirectory())
        {
            juce::Array<juce::File> files;
            root.findChildFiles (files, juce::File::findFiles, false, "*.xml");
            files.sort();
            for (auto& f : files)
            {
                const int id = kIdLoadBase + presetXmls.size();
                presetXmls.add (f);
                loadSub.addItem (id, f.getFileNameWithoutExtension());
            }
        }
        if (presetXmls.isEmpty())
            loadSub.addItem (-1, "(no page presets saved)", false, false);
        menu.addSubMenu ("Load Page Preset", loadSub);
    }

    // Automate (2026-08-02, ruling 1-c): last-touched fast path + the full
    // discovered parameter list.  A hosted plugin's editor is a foreign
    // window with no right-click hook of ours, so this menu IS the creation
    // surface for its lanes.  Chunked so thousand-parameter synths stay
    // navigable.
    juce::Array<Hosting::HostedPluginInstance::AutomatableParam> autoParams;
    {
        juce::PopupMenu autoSub;
        if (auto* h = getHosted())
        {
            juce::String lastId, lastName;
            h->getLastTouchedParam (lastId, lastName);
            autoSub.addItem (kIdAutomateLast,
                             lastId.isNotEmpty()
                                 ? "Last Touched: " + lastName
                                 : juce::String ("Last Touched (move a control in the plugin first)"),
                             lastId.isNotEmpty());
            autoSub.addSeparator();

            autoParams = h->getAutomatableParams();
            constexpr int kChunk = 30;
            if (autoParams.size() <= kChunk)
            {
                for (int i = 0; i < autoParams.size(); ++i)
                    autoSub.addItem (kIdAutoBase + i, autoParams[i].name);
            }
            else
            {
                for (int start = 0; start < autoParams.size(); start += kChunk)
                {
                    const int end = juce::jmin (start + kChunk, autoParams.size());
                    juce::PopupMenu chunk;
                    for (int i = start; i < end; ++i)
                        chunk.addItem (kIdAutoBase + i, autoParams[i].name);
                    autoSub.addSubMenu (juce::String (start + 1) + " - " + juce::String (end),
                                        chunk);
                }
            }
            if (autoParams.isEmpty())
                autoSub.addItem (-1, "(no parameters reported)", false, false);
        }
        else
        {
            autoSub.addItem (-1, "(no plugin loaded)", false, false);
        }
        menu.addSubMenu ("Automate", autoSub);
    }

    // Delete on the hamburger too (Jeff, 2026-08-02) -- see LayersPage.
    menu.addSeparator();
    menu.addItem (kIdDeleteTab, "Delete Plugin");

    juce::Component::SafePointer<PluginsPage> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (anchor),
        [safeThis, presetXmls = std::move (presetXmls),
         autoParams = std::move (autoParams), kIdLoadBase, kIdAutoBase] (int r)
        {
            if (! safeThis || r <= 0) return;
            if (r == kIdSavePagePreset) { safeThis->savePagePreset(); return; }
            if (r == kIdDeleteTab)      { safeThis->requestDelete();  return; }
            if (r == kIdAutomateLast)
            {
                if (auto* h = safeThis->getHosted())
                {
                    juce::String id, nm;
                    h->getLastTouchedParam (id, nm);
                    if (id.isNotEmpty() && safeThis->onAutomateParam)
                        safeThis->onAutomateParam (id);
                }
                return;
            }
            if (r >= kIdAutoBase && r < kIdAutoBase + autoParams.size())
            {
                if (safeThis->onAutomateParam)
                    safeThis->onAutomateParam (autoParams[r - kIdAutoBase].id);
                return;
            }
            if (r >= kIdLoadBase && r < kIdLoadBase + presetXmls.size())
                safeThis->loadPagePreset (presetXmls[r - kIdLoadBase]);
        });
}
