#include "BaySickRustyDrumsPage.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)
#include "../AppPaths.h"
#include "../BaySickRustyDrums/BaySickRustyDrumsProcessor.h"
#include "../BaySickRustyDrums/BaySickRustyDrumsKitGraphic.h"
#include "AriaControlPanel.h"   // K-5 (2026-05-05): moved to Source/Standalone/
#include "../UserFileSave.h"
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

BaySickRustyDrumsPage::BaySickRustyDrumsPage (BaySickDAWProcessor& p)
    : mProcessor (p)
{
    // QA-ProjectSave Task 11: wire dirty-listener pointers (the listener
    // attaches to the engine's apvts.state in loadKit, after the engine exists).
    mDirtyListener.dirtyFlag = &mPageDirty;
    mDirtyListener.suppress  = &mSuppressDirty;

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
    // Jeff, 2026-08-06: engineName stays EMPTY (the kit artwork carries Rusty's
    // own logo) and the BAND stays -- but it now hosts the section TAB ROW
    // (Main / Kick / Snare / ...), placed there by AriaControlPanel::resized.
    // The Program + Player Preset controls moved UP to the WINDOW title strip
    // (StandaloneEditor's Rusty page-show branch mounts them via
    // addExtraRightComponent, which lays out INSIDE the menu bar -- the mount
    // whose right edge already excludes the window chrome, unlike the T3
    // attempt that ended up behind it).
    binding.hostTitleBar = true;
    mAriaPanel = std::make_unique<AriaControlPanel> (binding);
    mPlayerTab->addAndMakeVisible (*mAriaPanel);
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
    // mCurrentProgram enum line up.
    Program target = Program::None;
    if (sfzPath.getFileName().equalsIgnoreCase ("01-full.sfz"))
        target = Program::Full;
    else if (sfzPath.getFileName().equalsIgnoreCase ("02-basic.sfz"))
        target = Program::Basic;

    // This page's whole control surface -- program dropdown, ARIA panel,
    // piano-roll engine label, program-tagged player presets -- is defined
    // over Full and Basic only, so a kit outside that pair has no state the
    // page can show or re-pick.  Loading it anyway used to leave the combo on
    // "Load Player" and the roll on "(no engine)" while the kit played, so
    // refuse it instead and let the caller name the file in the missing-files
    // report.  Widening this means widening Program + the dropdown with it.
    if (target == Program::None)
        return false;

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
    // engineName stays empty across program reloads too, and the band
    // persists (hostTitleBar) -- it hosts the section tab row (Jeff
    // 2026-08-06; the Program + Player Preset controls live on the WINDOW
    // title strip now).
    binding.hostTitleBar = true;
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
    // "Presets" (Jeff, 2026-08-12).  The full "Player Preset" was wide enough
    // to squeeze the strip's centred engine name into an ellipsis; the tooltip
    // still says which kind of preset this is.
    mPlayerPresetBtn = std::make_unique<juce::TextButton> ("Presets");
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
            // Ruling 3a + regression fix (2026-08-06): the load wraps at its
            // TERMINALS inside loadPlayerPresetFromFile (the cross-program
            // branch finishes in an async confirm callback the old outer
            // wrap could not see).
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

            const juce::String xml = root.toString (juce::XmlElement::TextFormat()
                                                        .singleLine());
            // QA-ProjectSave Task 11: completion hook so the delete prompt's
            // "Save Page Preset & Delete" can chain the delete after the write.
            // Cancel, invalid, cancelled-at-the-collision-prompt and
            // failed-write paths all leave it unfired.
            UserFileSave::writeTextAsync (safe->playerPresetsDir(), name, xml,
                [safe, onSaved] (const UserFileSave::Result& saved)
                {
                    // A collision prompt can hold this open long enough for the
                    // page to be closed, so the SafePointer is re-tested here
                    // rather than trusted from the naming callback.
                    if (! saved || ! safe) return;
                    if (onSaved) onSaved();
                },
                UserFileSave::kTabNotDeleted);
        }), false);
}

void BaySickRustyDrumsPage::loadPlayerPresetFromFile (const juce::File& xml)
{
    if (! xml.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Big Rusty Drums",
            "Could not load player preset '" + xml.getFileNameWithoutExtension()
                + "' - the file is missing or not a valid player preset.",
            "OK");
        return;
    }

    auto parsed = SafeXml::parse (xml);
    if (! parsed || ! parsed->hasTagName ("RustyPlayerPreset"))
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Big Rusty Drums",
            "Could not load player preset '" + xml.getFileNameWithoutExtension()
                + "' - the file is missing or not a valid player preset.",
            "OK");
        return;
    }

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
        // QA-UndoCoverage Task 6: and keep it out of the undo history too.
        juce::AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spw;
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

    // Regression fix (2026-08-06): the wrap moved from the MENU callback to
    // each terminal here.  The old shape wrapped this whole function, but the
    // cross-program branch does its real work in an ASYNC confirm callback --
    // the wrap closed before anything changed, the before==after guard ate
    // the empty transaction, and the actual switch ran unwrapped.
    auto wrapOp = [this] (std::function<void()> op)
    {
        if (onWrapStructuralOp) onWrapStructuralOp ("Load Player Preset", std::move (op));
        else                    op();
    };

    // Same program (or preset has no program tag): apply CCs immediately.
    if (presetProgram == Program::None || presetProgram == mCurrentProgram)
    {
        wrapOp (applyParams);
        return;
    }

    // Preset specifies a different program.  If nothing is loaded yet, switch
    // silently (no destructive state to warn about).  Ruling 1A: undoable,
    // undo returns to the empty player.
    if (mCurrentProgram == Program::None)
    {
        wrapOp ([this, presetProgram, applyParams]
        {
            if (loadProgram (presetProgram))
                applyParams();
        });
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
              "roll across every pattern.  Continue?",
        "Yes, switch + load", "Cancel", nullptr,
        juce::ModalCallbackFunction::create (
            [safe, presetProgram, applyParams] (int r)
            {
                if (! safe || r != 1) return;
                auto op = [safe, presetProgram, applyParams]
                {
                    if (safe != nullptr && safe->loadProgram (presetProgram))
                        applyParams();
                };
                if (safe->onWrapStructuralOp)
                    safe->onWrapStructuralOp ("Load Player Preset", op);
                else
                    op();
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
        // First load - no confirm prompt.  Ruling 1A (2026-08-06): still ONE
        // undoable transaction; undo returns to the empty player (the wrap's
        // restore recognizes the engine-less before-record and unloads).
        auto doIt = [this, target] { loadProgram (target); };
        if (onWrapStructuralOp) onWrapStructuralOp ("Load Rusty Program", doIt);
        else                    doIt();
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
        "every pattern, and reload the kit.  Continue?",
        "Yes, switch", "Cancel", nullptr,
        juce::ModalCallbackFunction::create ([this, target] (int result)
        {
            if (result == 1)
            {
                // Jeff ruling 1a (2026-08-06): the switch is ONE undoable
                // transaction (composite capture: engine blob + the pattern
                // slice the teardown clears).  "This cannot be undone."
                // retired from the warning above with it.
                auto doIt = [this, target] { loadProgram (target); };
                if (onWrapStructuralOp) onWrapStructuralOp ("Switch Rusty Program", doIt);
                else                    doIt();
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
    {
        // The teardown above really did destroy the running program, so None
        // is the honest state -- leaving mCurrentProgram on the old value
        // would make BOTH dropdown entries dead (re-picking the new one is
        // inert because the ComboBox id never changed, re-picking the old one
        // dies on the target == mCurrentProgram early-out) with a silent,
        // kit-less page behind them.  Inlined rather than calling
        // unloadToNone(): that would re-run tearDownCurrentProgram against an
        // engine that no longer exists and re-clear the pattern rolls.
        mCurrentProgram = Program::None;
        if (mProgramCombo) mProgramCombo->setSelectedId (0, juce::dontSendNotification);
        if (onProgramChanged) onProgramChanged();
        if (onSoundNameChanged) onSoundNameChanged ("BaySickRustyDrums");

        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Big Rusty Drums",
            "Could not load program:\n" + sfzPath.getFullPathName(),
            "OK");
        return false;
    }

    mCurrentProgram = target;
    if (onProgramChanged) onProgramChanged();   // QA-E Task 8 NIT-1: new program now current
    loadAriaPanelForProgram (target);   // J-8 stage 2: render the ARIA control surface
    if (onSoundNameChanged) onSoundNameChanged (juce::String ("BaySickRustyDrums - ") + programLabel (target));
    return true;
}

void BaySickRustyDrumsPage::unloadToNone()
{
    if (mCurrentProgram == Program::None) return;
    tearDownCurrentProgram();
    mCurrentProgram = Program::None;
    if (mProgramCombo) mProgramCombo->setSelectedId (0, juce::dontSendNotification);
    if (onProgramChanged) onProgramChanged();
    if (onSoundNameChanged) onSoundNameChanged ("BaySickRustyDrums");
}

void BaySickRustyDrumsPage::tearDownCurrentProgram()
{
    // QA-ProjectSave Task 11: the engine dies below -- detach first.
    detachDirtyListener();

    // QA-DispatcherAffinity Task 3 (2026-05-29): the piano-roll clear that
    // was previously inline here moved into
    // BaySickDAWProcessor::destroyBaySickRustyDrums (called below via
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
