#include "PluginsPage.h"
#include "UndoSnapshotStore.h"   // QA-UndoCoverage ruling 3a: swap snapshots
#include "../PluginProcessor.h"
#include "../EngineRig.h"
#include "../MissingFileReport.h"
#include "../UserFileSave.h"
#include "SharedUI.h"   // VC palette
#include "SlotComponent.h"
#include "WorkspaceWindow.h"   // sizeToContent for hosted-plugin surfaces
#include "PagePresetIO.h"

namespace
{
    constexpr int kPickBtnH = 26;
    constexpr int kPickGap  = 6;    // between the picker button and the plugin
    constexpr int kEdge     = 8;    // page inset, matches resized()

    // One spelling for the dead-plugin marker, shared with the rack slot's
    // (SlotComponent::slotDisplayName) -- the same condition on two surfaces
    // must not read two ways.
    juce::String missingSuffix() { return " (missing)"; }

    juce::String stripMissingMarker (const juce::String& s)
    {
        const auto suffix = missingSuffix();
        return s.endsWith (suffix) ? s.dropLastCharacters (suffix.length()) : s;
    }
}

PluginsPage::PluginsPage (BaySickDAWProcessor& p, int pageIndex)
    : mProcessor (p), mPageIndex (pageIndex)
{
    mTabName   = "Plugin " + juce::String (pageIndex + 1);
    mPageColor = VC::Purple;

    // Tab identity is created at page birth (TS1 convention) -- the model owns
    // the tab whether or not a plugin has been picked yet.
    mProcessor.engineRig().addTab (TabKind::Plugins, mPageIndex);

    // The added-plugin list is the "user put the missing plugin back" signal --
    // see retryDeadPlugin.  Subscribed by the PAGE because page destruction on
    // window close is off, so this object outlives every window its tab opens
    // and is torn down only with the tab itself.
    if (auto* pm = Hosting::PluginManager::getInstance())
        pm->addChangeListener (this);

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
    if (auto* pm = Hosting::PluginManager::getInstance())
        pm->removeChangeListener (this);

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
    auto* h = getHosted();
    if (h == nullptr) return {};

    // Asked LIVE rather than cached: a bridged load result and a crash both land
    // after this name is first rendered, and every surface re-reads it.
    const juce::String name = h->getDescription().name;
    return h->isAlive() ? name : name + missingSuffix();
}

juce::String PluginsPage::getDisplayName() const
{
    auto* h = getHosted();
    if (h == nullptr) return {};

    if (! h->isAlive())
    {
        const juce::String base = mTabName.isNotEmpty() ? mTabName
                                                        : h->getDescription().name;
        return base + missingSuffix();
    }

    // Revival edge: hand back the name the tab carried before the marker went
    // on.  The cascade this rides ends in setTabName, so reporting the program
    // name here would PERSIST it over a name the user chose.
    if (mInMarkerFire && mTabName.isNotEmpty())
        return mTabName;

    const auto prog = h->getProgramName (h->getCurrentProgram());
    return prog.isNotEmpty() ? prog : h->getDescription().name;
}

void PluginsPage::setTabName (const juce::String& n)
{
    // The dead-plugin marker is decoration on a name, never part of one: this is
    // the far end of the rename cascade getDisplayName rides, and it is also what
    // the project persists.  Stripping here is what keeps the marker from being
    // saved into a tab name and then outliving the condition that caused it.
    mTabName = stripMissingMarker (n);
}

void PluginsPage::showPicker()
{
    // One plugin per tab, for the tab's life: swapping the engine under a loaded
    // tab is a feature we do not offer -- closing the tab and opening a new one
    // does the same thing without destroying a live hosted instance under the
    // audio thread.  The button is disabled once a plugin exists; this guard
    // backs it up.  Only the PICKER is gated -- project restore, undo
    // resurrection and page-preset load reach the rig through selectPluginById
    // and are unaffected.
    if (getEngineProcessor() != nullptr)
        return;

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
    selectPluginById (desc.createIdentifierString());
}

void PluginsPage::selectPluginById (const juce::String& identifier)
{
    if (identifier.isEmpty())
        return;

    const bool wasEmpty = (getEngineProcessor() == nullptr);

    // The identifier string IS the engineType -- construction, registration and
    // teardown all belong to the rig.
    //
    // Deliberately NOT gated on the user's added-plugins list: the rig's
    // Plugins factory falls back to the description stashed with the saved tab,
    // which is the only thing that keeps a project (or an undo resurrection)
    // loading its plugin after the user removed it from that list or a rescan
    // dropped it.  Filtering here made that fallback unreachable.  The added
    // list gates showPicker, which is what builds the user's own menu.
    mProcessor.engineRig().setEngineType (TabKind::Plugins, mPageIndex, identifier);

    rebuildEditor();

    if (wasEmpty && getEngineProcessor() != nullptr && onEngineSelected)
        onEngineSelected();

    if (onPluginChanged)
        onPluginChanged();
}

bool PluginsPage::retryDeadPlugin()
{
    auto* h = getHosted();
    if (h == nullptr || h->isAlive()) return false;

    const bool revived = mProcessor.engineRig().retryDeadPluginTab (mPageIndex);

    // The instance changed identity whether or not it came back, so everything
    // holding the old one is refreshed HERE rather than left to the poll: a page
    // whose window is closed has no poll (peer-keyed suspend), and this can fire
    // from the added-list edge at any time.
    rebuildEditor();
    refreshNameMarker();

    // A new instance means a new parameter list and a new bridged-list-arrival
    // hook, both of which the lane registration owns.
    if (onParamListChanged) onParamListChanged();

    return revived;
}

// The added list is the only edge there is; a rescan or an add/remove elsewhere
// in the app lands here too, which is harmless -- the retry is gated on this
// tab's own instance being dead.
void PluginsPage::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == nullptr || source != Hosting::PluginManager::getInstance())
        return;

    retryDeadPlugin();
}

// The marker is part of the displayed name, so a change in aliveness renames the
// tab / strip / roll -- the dead edge decorating the tab's name, the alive edge
// handing that same name straight back.  Nothing is latched until there is a
// listener to tell: during project restore the page exists before its rename
// cascade is wired, and latching then would consume the only edge the marker
// ever gets.
void PluginsPage::refreshNameMarker()
{
    auto* h = getHosted();
    if (h == nullptr) return;

    const bool alive = h->isAlive();
    if (alive == mNameAlive || ! onPluginChanged) return;

    mNameAlive = alive;

    // Seeded OUTSIDE the fire so the program-name watch holds the plugin's real
    // live name -- seeding it with what the cascade publishes would leave the
    // watch one poll away from renaming the tab back to the program anyway.
    mLastDisplayName = getDisplayName();

    const juce::ScopedValueSetter<bool> markerFire (mInMarkerFire, true);
    onPluginChanged();
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
        mPickBtn.setEnabled (true);
        mPickBtn.setButtonText ("Select plugin...");
        resized();
        return;
    }

    if (auto* h = dynamic_cast<Hosting::HostedPluginInstance*> (eng))
        mBuiltAlive = h->isAlive();

    // The pick button becomes a plain name label once the tab has its plugin --
    // see showPicker for why the swap is gone.
    mPickBtn.setEnabled (false);
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

            // SUPPRESS THE SAVE.  This is a programmatic fit to the plugin, not
            // a size the user chose - and WorkspaceWindow::resized() persists on
            // every settled geometry change.  Without this, each fit overwrote
            // the user's stored window size; while the fit was briefly
            // oscillating it wrote a slightly smaller size every pass, so the
            // window came back smaller on each reopen until it was barely
            // visible.  ScopedSaveSuppress exists for exactly this ("a clamp is
            // not a placement the user chose").
            const WorkspaceWindow::ScopedSaveSuppress noSave;

            win->sizeToContent (juce::jmax (240, w + chromeW), h + chromeH);

            // NO PLUGIN-DERIVED FLOOR (2026-08-11).  It used to be the plugin's
            // size times the minimum usable SCALE, and nothing scales any more.
            // Restoring it as the plugin's own size would be actively wrong:
            // sizeToContent applies the floor AFTER the workspace clamp, so a
            // plugin bigger than the workspace would force a window bigger than
            // the workspace.  The window is free to be smaller than the plugin -
            // it clips, and the plugin's own magnify is the control that fixes
            // that.  0 leaves WorkspaceWindow's anti-degenerate minimum.
            win->setResizeFloor (0, 0);
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

    refreshNameMarker();

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
    // Unmarked: this becomes a FILENAME on disk, and a preset saved while the
    // plugin was missing would otherwise carry the marker for good (the state it
    // writes is the retained last-known blob, which is perfectly valid).
    const juce::String defaultName = stripMissingMarker (getPluginName());
    aw->addTextEditor ("name", defaultName.isNotEmpty() ? defaultName
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

            // The tail lives inside the callback because this save is also the
            // first half of "Save Page Preset & Delete": on anything but
            // Succeeded -- a failed write, or the user answering Cancel to the
            // collision prompt -- nothing below may run, least of all onSaved
            // and least of all the clean-baseline re-capture, which would make
            // the next delete skip its unsaved-work prompt for work that was
            // never written.  safeThis is re-tested because the collision
            // prompt is a second modal, so the page can close while it is up.
            const auto dir = PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Plugins);
            UserFileSave::writeTextAsync (dir, name, xml,
                [safeThis, onSaved] (const UserFileSave::Result& saved)
                {
                    if (! saved || ! safeThis) return;

                    // Saved = clean: the delete prompt goes back to the plain confirm.
                    if (auto* h = safeThis->getHosted(); h != nullptr && h->isAlive())
                    {
                        safeThis->mBaselineTouchCount = h->getTouchCount();
                        safeThis->mBaselineProgram    = safeThis->getDisplayName();
                        safeThis->mBaselineCaptured   = true;
                    }

                    if (onSaved) onSaved();
                },
                UserFileSave::kTabNotDeleted);
        }), false);
}

// QA-UndoCoverage ruling 3a: plugin-state swap gesture as one transaction.
void PluginsPage::performChainSwapGesture (const juce::String& label,
                                           const std::function<void()>& op)
{
    auto encode = [] (juce::AudioProcessor* eng) -> juce::String
    {
        if (eng == nullptr) return {};
        juce::MemoryBlock mb;
        eng->getStateInformation (mb);
        return mb.toBase64Encoding();
    };

    const juce::String before = encode (getEngineProcessor());
    if (! mUndoCtx.isValid() || before.isEmpty()) { op(); return; }

    op();
    const juce::String after = encode (getEngineProcessor());
    if (before == after) return;

    const juce::File beforeF = UndoSnapshotStore::writeNew (before);
    const juce::File afterF  = UndoSnapshotStore::writeNew (after);
    // Jeff ruling 2026-08-06: resolve the LIVE page at apply time (a
    // delete+resurrect cycle replaces this object; SafePointer = fallback).
    auto resolveSelf = mUndoCtx.resolveOwnerPage;
    auto sp = juce::Component::SafePointer<PluginsPage> (this);
    auto apply = [resolveSelf, sp] (const juce::File& f)
    {
        auto* pp = resolveSelf ? dynamic_cast<PluginsPage*> (resolveSelf())
                               : sp.getComponent();
        if (pp == nullptr || ! f.existsAsFile()) return;
        auto* eng = pp->getEngineProcessor();
        if (eng == nullptr) return;
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (f.loadFileAsString()))
            eng->setStateInformation (mb.getData(), (int) mb.getSize());
    };
    mUndoCtx.perform (new StructuralOpAction ([apply, beforeF] { apply (beforeF); },
                                              [apply, afterF]  { apply (afterF); },
                                              { beforeF, afterF }),
                      label);
}

void PluginsPage::loadPagePreset (const juce::File& xml)
{
    if (! xml.existsAsFile()) return;

    // The preset restores this tab's insert rack, whose slots can hold effects
    // that reference an external file (a user IR, a VST3 whose DLL is gone) and
    // record a miss rather than failing loudly.  The scope reports that under
    // THIS gesture's name; an entry left banked surfaces later attached to an
    // unrelated load, naming a file that has nothing to do with what the user
    // is looking at then.
    MissingFileReport::ScopedGesture gesture ("preset");

    // Two-step, mirroring project restore: instantiate the preset's plugin
    // through the rig FIRST, then apply state (the bridge load path holds
    // early state until the remote instance is ready).
    const juce::String wantedId = PagePresetIO::peekEngineType (xml);
    if (wantedId.isNotEmpty() && getEngineType() != wantedId)
        selectPluginById (wantedId);

    if (wantedId.isNotEmpty() && getEngineType() != wantedId)
    {
        // setEngineType clears the tab's engineType when the factory can't
        // build the plugin.  A page preset carries no stashed description
        // (that's the project/undo restore path), so the only resolver left is
        // the added list.
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
    constexpr int kIdRetryPlugin    = 103;
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

    // The automatic recovery watches the added-plugin list, and putting a plugin
    // back on that list is not the gesture every user reaches for -- so the retry
    // is also here, by hand.  Shown only while the plugin is not alive: retrying
    // a working one means nothing.  Same wording as the rack slot's retry.
    if (auto* h = getHosted(); h != nullptr && ! h->isAlive())
    {
        menu.addSeparator();
        menu.addItem (kIdRetryPlugin, "Retry Loading Plugin");
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
            if (r == kIdRetryPlugin)    { safeThis->retryDeadPlugin(); return; }
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
            {
                auto* pp = safeThis.getComponent();
                pp->performChainSwapGesture ("Load Page Preset",
                    [pp, f = presetXmls[r - kIdLoadBase]] { pp->loadPagePreset (f); });
            }
        });
}
