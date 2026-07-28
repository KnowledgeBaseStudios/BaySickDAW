#include "BaySickRustyDrumsPage.h"
#include "../AppPaths.h"
#include "../BaySickRustyDrums/BaySickRustyDrumsProcessor.h"
#include "../BaySickRustyDrums/BaySickRustyDrumsKitGraphic.h"
#include "AriaControlPanel.h"   // K-5 (2026-05-05): moved to Source/Standalone/
#include "../VibeGraph.h"
#include "SampleLibrary.h"
#include "StandaloneEditor.h"

namespace
{
constexpr int kComboItemFull  = 1;
constexpr int kComboItemBasic = 2;

juce::File programSfzPath (BaySickRustyDrumsPage::Program p)
{
    const auto base = SampleLibrary::getCoreLibraryDir()
                          .getChildFile ("Big Rusty Drums")
                          .getChildFile ("Programs");
    switch (p)
    {
        case BaySickRustyDrumsPage::Program::Full:  return base.getChildFile ("01-full.sfz");
        case BaySickRustyDrumsPage::Program::Basic: return base.getChildFile ("02-basic.sfz");
        default:                                    return {};
    }
}

// J-8 stage 2: ARIA GUI XML lives at `<kitRoot>/GUI/<program>.xml`.  We pick
// the matching file for the current program selection.
juce::File programGuiXml (BaySickRustyDrumsPage::Program p)
{
    const auto base = SampleLibrary::getCoreLibraryDir()
                          .getChildFile ("Big Rusty Drums")
                          .getChildFile ("GUI");
    switch (p)
    {
        case BaySickRustyDrumsPage::Program::Full:  return base.getChildFile ("01-full.xml");
        case BaySickRustyDrumsPage::Program::Basic: return base.getChildFile ("02-basic.xml");
        default:                                    return {};
    }
}

const char* programLabel (BaySickRustyDrumsPage::Program p)
{
    switch (p)
    {
        case BaySickRustyDrumsPage::Program::Full:  return "Full";
        case BaySickRustyDrumsPage::Program::Basic: return "Basic";
        default:                                    return "Load Player";
    }
}
}

// QA-E Task 8 NIT-1 (engine-type half): the piano-roll context label's
// engine portion = this string (empty -> "(no engine)").  None returns
// empty (consistent with every other engine pre-load); Full/Basic return
// their label so the label reads "{tab} - Full" / "{tab} - Basic".
juce::String BaySickRustyDrumsPage::getEngineType() const
{
    return mCurrentProgram == Program::None
               ? juce::String()
               : juce::String (programLabel (mCurrentProgram));
}

BaySickRustyDrumsPage::BaySickRustyDrumsPage (VibeSynthProcessor& p)
    : mProcessor (p)
{
    // QA-ProjectSave Task 11: wire dirty-listener pointers (the listener
    // attaches to the engine's apvts.state in loadKit, after the engine exists).
    mDirtyListener.dirtyFlag = &mPageDirty;
    mDirtyListener.suppress  = &mSuppressDirty;

    // Smoke #13 fix: the combo + preset button MUST exist before
    // buildPlayerTab hosts them on the panel title bar -- the old order left
    // both null at hosting time, so the guards skipped and the Load Player
    // dropdown was parentless (invisible everywhere).
    buildProgramCombo();
    buildPlayerPresetButton();
    buildDrumKitTab();
    buildPlayerTab();
    buildPianoRollTab();

    addAndMakeVisible (*mDrumKitTab);
    addChildComponent (*mPlayerTab);
    addChildComponent (*mPianoRollTab);

    // Smoke round 2 (Jeff): land on the PLAYER sub-tab -- the Load Player
    // dropdown lives there now, so opening on the kit-image tab left a new
    // Rusty tab with no visible way to load a program.
    switchTab (1);
    startTimerHz (10);
}

BaySickRustyDrumsPage::~BaySickRustyDrumsPage()
{
    // QA-ProjectSave Task 11: drop the listener before the engine can go away.
    detachDirtyListener();
}

void BaySickRustyDrumsPage::attachDirtyListener()
{
    detachDirtyListener();
    if (auto* engine = mProcessor.getBaySickRustyDrums())
        engine->apvts.state.addListener (&mDirtyListener);
}

void BaySickRustyDrumsPage::detachDirtyListener()
{
    if (auto* engine = mProcessor.getBaySickRustyDrums())
        engine->apvts.state.removeListener (&mDirtyListener);
}

void BaySickRustyDrumsPage::paint (juce::Graphics& g)
{
    // QA-A 4.5 (2026-05-09): match BaySickTitleBar's bg (0xFF141618) so the
    // page background and the title bar visually fuse.  VC::Bg (0xFF1C1C1E)
    // was a slightly different charcoal that read as a colour mismatch.
    g.fillAll (juce::Colour (0xFF141618));
}

void BaySickRustyDrumsPage::resized()
{
    auto b = getLocalBounds();
    if (mDrumKitTab)   mDrumKitTab  ->setBounds (b);
    if (mPlayerTab)    mPlayerTab   ->setBounds (b);
    if (mPianoRollTab) mPianoRollTab->setBounds (b);

    if (mDrumKitTab && mDrumKitTab->isVisible() && mKitGraphic)
        mKitGraphic->setBounds (b);

    if (mPlayerTab && mPlayerTab->isVisible() && mAriaPanel)
        mAriaPanel->setBounds (b);

    if (mPianoRollTab && mPianoRollTab->isVisible() && mPianoRollPlaceholder)
        mPianoRollPlaceholder->setBounds (b);
}

void BaySickRustyDrumsPage::switchTab (int idx)
{
    // 3 sub-tabs: 0 = Drum Kit, 1 = Player, 2 = Piano Roll (redirect).
    mActiveTab = juce::jlimit (0, 2, idx);
    if (mDrumKitTab)   mDrumKitTab  ->setVisible (mActiveTab == 0);
    if (mPlayerTab)    mPlayerTab   ->setVisible (mActiveTab == 1);
    if (mPianoRollTab) mPianoRollTab->setVisible (mActiveTab == 2);
    resized();
    if (onSubTabChanged) onSubTabChanged (mActiveTab);
}

void BaySickRustyDrumsPage::setTabName (const juce::String& name)
{
    mTabName = name;
}

void BaySickRustyDrumsPage::timerCallback()
{
    // Refresh kit-loaded state on the kit graphic so the dimmed-overlay
    // pre-load mode toggles when a program is loaded / unloaded.
    if (mKitGraphic)
        mKitGraphic->setKitLoaded (mProcessor.hasBaySickRustyDrums());
}

void BaySickRustyDrumsPage::buildDrumKitTab()
{
    mDrumKitTab = std::make_unique<juce::Component>();
    mDrumKitTab->setName ("BaySickRustyDrums Drum Kit Tab");

    // Kit graphic fills the entire sub-tab content area (full-bleed).  Engine
    // pointer rebinds when a program loads via `setEngine`.
    mKitGraphic = std::make_unique<BaySickRustyDrumsKitGraphic> (mProcessor.getBaySickRustyDrums());
    mKitGraphic->setKitLoaded (mProcessor.hasBaySickRustyDrums());
    mDrumKitTab->addAndMakeVisible (*mKitGraphic);
}

void BaySickRustyDrumsPage::buildPlayerTab()
{
    mPlayerTab = std::make_unique<juce::Component>();
    mPlayerTab->setName ("BaySickRustyDrums Player Tab");

    // J-8 stage 2 (2026-05-04): ARIA control panel.  Pre-load it shows a
    // dimmed background placeholder; after a program loads, it renders the
    // kit's prebuilt GUI XML (knobs + labels + option menus + background art).
    // K-5 (2026-05-05): build the engine-agnostic binding for the panel.
    // Closures captured by value - engine pointer is null until loadKit
    // succeeds, after which the panel re-binds via setEngine() below.
    auto* engine = mProcessor.getBaySickRustyDrums();
    AriaControlPanel::Binding binding;
    if (engine != nullptr)
    {
        binding.apvts        = &engine->apvts;
        binding.ccParamId    = [](int cc) { return juce::String ("brd_cc") + juce::String (cc); };
        binding.kitDefaultCc = [engine](int cc) { return engine->getKitDefaultCc (cc); };
        binding.ccLabel      = [engine](int cc) { return engine->getCcLabel (cc); };
    }
    // QA-A 4.5 (2026-05-09): set engineName + accentColor so AriaControlPanel
    // hosts a "BaySickRustyDrums" title bar at the top (above its internal
    // sub-tab strip).  Drums-tab red (#CC2222) per D7.
    binding.engineName  = "BaySickRustyDrums";
    binding.accentColor = juce::Colour (0xFFCC2222);
    mAriaPanel = std::make_unique<AriaControlPanel> (binding);
    mPlayerTab->addAndMakeVisible (*mAriaPanel);

    // QA-G3Smoke G-16: the Program selector + Player Preset button move off
    // the PageMenuBar onto this title bar.  Smoke round 2 (Jeff): the SW-3
    // Swing Mix knob moved OFF here onto the PageMenuBar (StandaloneEditor
    // wires it per page-show) so it's visible on every sub-tab.
    if (auto* bar = mAriaPanel->getTitleBar())
    {
        if (mPlayerPresetBtn) bar->addHostedTrailingWidget (mPlayerPresetBtn.get(), 110);
        if (mProgramCombo)    bar->addHostedTrailingWidget (mProgramCombo.get(),    160);
    }
}

void BaySickRustyDrumsPage::buildPianoRollTab()
{
    mPianoRollTab = std::make_unique<juce::Component>();
    mPianoRollTab->setName ("BaySickRustyDrums Piano Roll Tab");

    // Piano Roll is a nav-shortcut at the editor level (matches DrumPage):
    // selecting this sub-tab redirects the user to the unified PianoRollPage
    // with EngineKind::BaySickRustyDrums selected.  This placeholder only
    // shows briefly before the redirect fires.
    mPianoRollPlaceholder = std::make_unique<juce::Label> ("pianoRollPlaceholder",
        "Redirecting to Piano Roll...");
    mPianoRollPlaceholder->setJustificationType (juce::Justification::centred);
    mPianoRollPlaceholder->setColour (juce::Label::textColourId, VC::Text.withAlpha (0.5f));
    mPianoRollPlaceholder->setFont (juce::Font (juce::FontOptions (14.f)));
    mPianoRollTab->addAndMakeVisible (*mPianoRollPlaceholder);
}

bool BaySickRustyDrumsPage::loadKit (const juce::File& sfzPath)
{
    HeavyOperationOverlay::ScopedOp busy (StandaloneEditor::busyOverlayFor (this),
                                          "Loading Kit...", true);
    if (! mProcessor.loadBaySickRustyDrumsKit (sfzPath))
        return false;
    if (mKitGraphic) mKitGraphic->setEngine (mProcessor.getBaySickRustyDrums());
    if (mKitGraphic) mKitGraphic->setKitLoaded (true);
    // QA-ProjectSave Task 11: fresh kit = clean page.  loadKit is the single
    // chokepoint every engine (re)creation runs through (program switch,
    // project restore), so the listener re-attaches here.
    attachDirtyListener();
    mPageDirty = false;
    if (onKitLoaded) onKitLoaded();
    if (onSoundNameChanged) onSoundNameChanged (sfzPath.getFileName());
    return true;
}

bool BaySickRustyDrumsPage::reloadForProjectRestore (const juce::File& sfzPath)
{
    // Identify which program this kit file represents so the dropdown +
    // mCurrentProgram enum line up.  Falls back to None for unknown paths.
    Program target = Program::None;
    if (sfzPath.getFileName().equalsIgnoreCase ("01-full.sfz"))
        target = Program::Full;
    else if (sfzPath.getFileName().equalsIgnoreCase ("02-basic.sfz"))
        target = Program::Basic;

    // loadKit forwards to mProcessor.loadBaySickRustyDrumsKit (which is the
    // active-flag-protected path) AND fires onKitLoaded so the mixer strips
    // spawn alongside the just-created InsertNodes.  Do NOT call the wrapper
    // directly - that path skips the kit-graphic + onKitLoaded plumbing.
    if (! loadKit (sfzPath))
        return false;

    // Sync program state so a later re-pick of the same program is a no-op
    // (otherwise loadProgram → loadKit re-runs and the kit's set_cc directives
    // stomp the saved CC values that the caller is about to replaceState in).
    mCurrentProgram = target;
    if (onProgramChanged) onProgramChanged();   // QA-E Task 8 NIT-1: new program now current
    if (mProgramCombo)
    {
        const int restoreId = (target == Program::Full)  ? kComboItemFull
                            : (target == Program::Basic) ? kComboItemBasic
                                                          : 0;
        mProgramCombo->setSelectedId (restoreId, juce::dontSendNotification);
    }

    // Render the ARIA control panel for this program (knobs + dropdowns +
    // background art).  Without this the Player sub-tab stays empty until
    // the user manually re-picks the program from the dropdown.
    loadAriaPanelForProgram (target);

    if (onSoundNameChanged)
        onSoundNameChanged (juce::String ("BaySickRustyDrums - ") + programLabel (target));

    return true;
}

void BaySickRustyDrumsPage::loadAriaPanelForProgram (Program target)
{
    if (! mAriaPanel) return;
    // K-5 (2026-05-05): rebuild binding for the (possibly-fresh) engine pointer
    // and hand it to the panel before loading the new program XML.
    auto* engine = mProcessor.getBaySickRustyDrums();
    AriaControlPanel::Binding binding;
    if (engine != nullptr)
    {
        binding.apvts        = &engine->apvts;
        binding.ccParamId    = [](int cc) { return juce::String ("brd_cc") + juce::String (cc); };
        binding.kitDefaultCc = [engine](int cc) { return engine->getKitDefaultCc (cc); };
        binding.ccLabel      = [engine](int cc) { return engine->getCcLabel (cc); };
    }
    // QA-A 4.5 (2026-05-09): preserve the title bar across program reloads.
    binding.engineName  = "BaySickRustyDrums";
    binding.accentColor = juce::Colour (0xFFCC2222);
    mAriaPanel->setEngine (binding);

    const auto kitRoot = SampleLibrary::getCoreLibraryDir().getChildFile ("Big Rusty Drums");
    const auto xml     = programGuiXml (target);
    if (xml.existsAsFile())
        mAriaPanel->loadFromKit (kitRoot, xml);
    else
        mAriaPanel->clear();
}

// ── J-11 Player Preset dropdown ────────────────────────────────────────────
// "Player Preset" captures kit CC values only (every brd_cc<N> + brd_outVol).
// Independent of the Save/Load Page Preset on the page hamburger (which also
// captures the mixer strips + racks).  Apply uses overlay semantics: only
// params present in the preset XML are written; everything else stays put.

juce::File BaySickRustyDrumsPage::playerPresetsDir() const
{
    return AppPaths::appRoot()
               .getChildFile ("Presets")
               .getChildFile ("Rusty Player")
               .getChildFile ("My Presets");
}

void BaySickRustyDrumsPage::buildPlayerPresetButton()
{
    mPlayerPresetBtn = std::make_unique<juce::TextButton> ("Player Preset");
    mPlayerPresetBtn->setTooltip ("Save / load player preset (kit CC values only)");
    mPlayerPresetBtn->setColour (juce::TextButton::buttonColourId, VC::Surface);
    mPlayerPresetBtn->setColour (juce::TextButton::textColourOffId, VC::Text);
    mPlayerPresetBtn->onClick = [this] { showPlayerPresetMenu(); };
}

void BaySickRustyDrumsPage::showPlayerPresetMenu()
{
    constexpr int kIdSave = 1;
    constexpr int kIdLoadBase = 100;

    juce::PopupMenu menu;
    const bool hasEngine = mProcessor.getBaySickRustyDrums() != nullptr;
    menu.addItem (kIdSave, "Save Player Preset As...", hasEngine);

    juce::Array<juce::File> presetXmls;
    {
        const auto root = playerPresetsDir();
        if (root.isDirectory())
        {
            juce::Array<juce::File> files;
            root.findChildFiles (files, juce::File::findFiles, false, "*.xml");
            files.sort();
            for (auto& f : files) presetXmls.add (f);
        }
    }

    menu.addSeparator();
    menu.addSectionHeader ("Load Player Preset");
    if (presetXmls.isEmpty())
    {
        menu.addItem (-1, "(no presets saved)", false, false);
    }
    else
    {
        for (int i = 0; i < presetXmls.size(); ++i)
            menu.addItem (kIdLoadBase + i,
                          presetXmls[i].getFileNameWithoutExtension());
    }

    juce::Component::SafePointer<BaySickRustyDrumsPage> safe (this);
    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (mPlayerPresetBtn.get()),
        [safe, presetXmls] (int r)
        {
            if (! safe || r <= 0) return;
            if (r == kIdSave) { safe->savePlayerPresetAs(); return; }
            if (r >= kIdLoadBase && r < kIdLoadBase + presetXmls.size())
                safe->loadPlayerPresetFromFile (presetXmls[r - kIdLoadBase]);
        });
}

void BaySickRustyDrumsPage::savePlayerPresetAs (std::function<void()> onSaved)
{
    auto* engine = mProcessor.getBaySickRustyDrums();
    if (engine == nullptr) return;

    auto* aw = new juce::AlertWindow (
        "Save Player Preset",
        "Enter a name for this player preset:",
        juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", "My Rusty Player");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<BaySickRustyDrumsPage> safe (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safe, aw, onSaved] (int result)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (result != 1 || ! safe) return;
            const auto name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;

            auto* engine = safe->mProcessor.getBaySickRustyDrums();
            if (engine == nullptr) return;

            juce::XmlElement root ("RustyPlayerPreset");
            root.setAttribute ("version", 1);
            // J-11 fix (2026-05-05): capture which program (Full / Basic) was
            // loaded when the preset was saved.  Without this, loading a
            // preset saved on Full while Basic is active would only apply the
            // CCs Basic happens to share - Full-only knob values would write
            // to APVTS but never reach a visible control, so the user would
            // see "only the Basic controls" with their saved Full-program
            // configuration silently lost.
            {
                auto* progEl = root.createNewChildElement ("Program");
                progEl->setAttribute ("name", programLabel (safe->mCurrentProgram));
            }
            auto* paramsEl = root.createNewChildElement ("Params");
            auto pushParam = [&] (const juce::String& id)
            {
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (
                        engine->apvts.getParameter (id)))
                {
                    const float natural = rp->convertFrom0to1 (rp->getValue());
                    auto* pe = paramsEl->createNewChildElement ("Param");
                    pe->setAttribute ("id", id);
                    pe->setAttribute ("v",  natural);
                }
            };
            // Full registered CC space, not MIDI's 128: kit-author "extended
            // CCs" >= 128 (hi-hat macro CC400/401 etc.) must round-trip too --
            // the old 128 cap silently dropped them from player presets.
            for (int cc = 0; cc < BaySickRustyDrumsProcessor::kCcCount; ++cc)
                pushParam ("brd_cc" + juce::String (cc));
            pushParam ("brd_outVol");

            auto dir = safe->playerPresetsDir();
            dir.createDirectory();
            auto target = dir.getChildFile (name + ".xml");
            int n = 2;
            while (target.exists())
                target = dir.getChildFile (name + " (" + juce::String (n++) + ").xml");
            target.replaceWithText (root.toString (juce::XmlElement::TextFormat()
                                                        .singleLine()));
            // QA-ProjectSave Task 11: completion hook so the delete prompt's
            // "Save Page Preset & Delete" can chain the delete after the write.
            // Cancel/invalid paths return above without firing it.
            if (onSaved) onSaved();
        }), false);
}

void BaySickRustyDrumsPage::loadPlayerPresetFromFile (const juce::File& xml)
{
    if (! xml.existsAsFile()) return;

    auto parsed = juce::XmlDocument::parse (xml);
    if (! parsed || ! parsed->hasTagName ("RustyPlayerPreset")) return;

    // J-11 fix (2026-05-05): read the program the preset was saved on (if
    // recorded) so we can switch to it before applying CCs.  Older presets
    // (pre-fix) won't have this element - we apply on the current program.
    Program presetProgram = Program::None;
    if (auto* pe = parsed->getChildByName ("Program"))
    {
        const auto name = pe->getStringAttribute ("name");
        if      (name.equalsIgnoreCase ("Full"))  presetProgram = Program::Full;
        else if (name.equalsIgnoreCase ("Basic")) presetProgram = Program::Basic;
    }

    // Hoist param data out of the XML so it survives across the async program
    // switch.  Each entry is (paramId, naturalValue).
    auto paramPairs = std::make_shared<std::vector<std::pair<juce::String, float>>>();
    if (auto* paramsEl = parsed->getChildByName ("Params"))
    {
        for (auto* pe = paramsEl->getFirstChildElement(); pe != nullptr;
             pe = pe->getNextElement())
        {
            if (! pe->hasTagName ("Param")) continue;
            paramPairs->emplace_back (pe->getStringAttribute ("id"),
                                      (float) pe->getDoubleAttribute ("v"));
        }
    }

    auto applyParams = [this, paramPairs]
    {
        auto* engine = mProcessor.getBaySickRustyDrums();
        if (engine == nullptr) return;
        // QA-ProjectSave Task 11: a freshly-applied preset is the new clean
        // state, not an edit -- suppress the dirty listener for the apply.
        mSuppressDirty = true;
        for (const auto& [id, natural] : *paramPairs)
        {
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (
                    engine->apvts.getParameter (id)))
            {
                rp->setValueNotifyingHost (
                    rp->getNormalisableRange().convertTo0to1 (natural));
            }
        }
        mSuppressDirty = false;
        mPageDirty     = false;
    };

    // Same program (or preset has no program tag): apply CCs immediately.
    if (presetProgram == Program::None || presetProgram == mCurrentProgram)
    {
        applyParams();
        return;
    }

    // Preset specifies a different program.  If nothing is loaded yet, switch
    // silently (no destructive state to warn about).
    if (mCurrentProgram == Program::None)
    {
        if (loadProgram (presetProgram))
            applyParams();
        return;
    }

    // Active program differs - same destructive switch as picking a new
    // program from the dropdown.  Confirm before wiping mixer + piano roll.
    juce::Component::SafePointer<BaySickRustyDrumsPage> safe (this);
    juce::AlertWindow::showOkCancelBox (
        juce::AlertWindow::WarningIcon,
        "Load Player Preset?",
        juce::String ("This preset was saved on '") + programLabel (presetProgram)
            + "' and will switch the player from '" + programLabel (mCurrentProgram)
            + "'.  Switching will reset all mixer settings and clear the piano "
              "roll across every pattern.  This cannot be undone.  Continue?",
        "Yes, switch + load", "Cancel", nullptr,
        juce::ModalCallbackFunction::create (
            [safe, presetProgram, applyParams] (int r)
            {
                if (! safe || r != 1) return;
                if (safe->loadProgram (presetProgram))
                    applyParams();
            }));
}

// ── Program selector + switching ────────────────────────────────────────────
void BaySickRustyDrumsPage::buildProgramCombo()
{
    mProgramCombo = std::make_unique<juce::ComboBox> ("rustyProgram");
    mProgramCombo->setTextWhenNothingSelected ("Load Player");
    mProgramCombo->addItem ("Full",  kComboItemFull);
    mProgramCombo->addItem ("Basic", kComboItemBasic);
    mProgramCombo->setColour (juce::ComboBox::backgroundColourId, VC::Surface);
    mProgramCombo->setColour (juce::ComboBox::textColourId,       VC::Text);
    mProgramCombo->onChange = [this] { onProgramComboChanged(); };
}

void BaySickRustyDrumsPage::onProgramComboChanged()
{
    if (! mProgramCombo) return;
    const int sel = mProgramCombo->getSelectedId();
    Program target = Program::None;
    if      (sel == kComboItemFull)  target = Program::Full;
    else if (sel == kComboItemBasic) target = Program::Basic;
    else return;

    if (target == mCurrentProgram) return;

    if (mCurrentProgram == Program::None)
    {
        // First load - no confirm prompt, nothing to lose.
        loadProgram (target);
        return;
    }

    // Subsequent switch - confirm prompt; revert dropdown on cancel.
    promptAndSwitchProgram (target);
}

void BaySickRustyDrumsPage::promptAndSwitchProgram (Program target)
{
    juce::AlertWindow::showOkCancelBox (
        juce::AlertWindow::WarningIcon,
        "Switch Rusty Drums program?",
        "Switching will reset all mixer settings, clear the piano roll across "
        "every pattern, and reload the kit.  This cannot be undone.  Continue?",
        "Yes, switch", "Cancel", nullptr,
        juce::ModalCallbackFunction::create ([this, target] (int result)
        {
            if (result == 1)
            {
                loadProgram (target);
            }
            else
            {
                // Revert dropdown to the currently-loaded program.
                if (mProgramCombo)
                {
                    const int restoreId = (mCurrentProgram == Program::Full)  ? kComboItemFull
                                        : (mCurrentProgram == Program::Basic) ? kComboItemBasic
                                                                              : 0;
                    mProgramCombo->setSelectedId (restoreId, juce::dontSendNotification);
                }
            }
        }));
}

bool BaySickRustyDrumsPage::loadProgram (Program target)
{
    const auto sfzPath = programSfzPath (target);
    if (! sfzPath.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Big Rusty Drums",
            "Could not find program SFZ file:\n" + sfzPath.getFullPathName(),
            "OK");
        // Revert dropdown
        if (mProgramCombo)
            mProgramCombo->setSelectedId (mCurrentProgram == Program::Full ? kComboItemFull
                                        : mCurrentProgram == Program::Basic ? kComboItemBasic
                                        : 0,
                                          juce::dontSendNotification);
        return false;
    }

    // Tear down any existing program first.
    if (mCurrentProgram != Program::None)
        tearDownCurrentProgram();

    if (! loadKit (sfzPath))
        return false;

    mCurrentProgram = target;
    if (onProgramChanged) onProgramChanged();   // QA-E Task 8 NIT-1: new program now current
    loadAriaPanelForProgram (target);   // J-8 stage 2: render the ARIA control surface
    if (onSoundNameChanged) onSoundNameChanged (juce::String ("BaySickRustyDrums - ") + programLabel (target));
    return true;
}

void BaySickRustyDrumsPage::tearDownCurrentProgram()
{
    // QA-ProjectSave Task 11: the engine dies below -- detach first.
    detachDirtyListener();

    // QA-DispatcherAffinity Task 3 (2026-05-29): the piano-roll clear that
    // was previously inline here moved into
    // VibeSynthProcessor::destroyBaySickRustyDrums (called below via
    // mProcessor.destroyBaySickRustyDrums()).  Single source of truth for
    // "destroy Rusty completely" so the tab-delete path inherits the same
    // behavior automatically -- previously the tab-delete path's
    // "Delete BaySickRustyDrums?" onDeleteRequested confirmation dialog in
    // StandaloneEditor.cpp promised the clear but it was only performed here
    // on program change.  Surfaced by Jeff at Task 3 Verify 2 (kit-swap test).

    // Reset Rusty mixer-strip + bus APVTS params to default.  PluginProcessor
    // exposes a helper that walks all `mixer_rusty_*` and `mixer_rustybus_*`
    // params and sets each to its registered default value.
    mProcessor.resetBaySickRustyDrumsMixerState();

    // J-8 stage 2 (2026-05-04): clear UI widgets BEFORE the engine is freed.
    // The ARIA panel's AriaKnob / AriaOptionMenu widgets hold SliderParameter
    // Attachments rooted in the engine's APVTS - destructing them after the
    // engine is gone is a use-after-free.  Same logic for the kit graphic
    // (which holds a raw engine pointer for hi-hat pedal state).
    if (mKitGraphic) { mKitGraphic->setEngine (nullptr); mKitGraphic->setKitLoaded (false); }
    if (mAriaPanel)  { mAriaPanel ->clear(); mAriaPanel->setEngine (AriaControlPanel::Binding{}); }

    // Tear down all Rusty InsertNodes (PluginProcessor's destroy method) and
    // mark the engine inactive.  The engine itself is rebuilt in loadKit().
    mProcessor.destroyBaySickRustyDrums();
}
