#include "BaySickRustyDrumsPage.h"
#include "../BaySickRustyDrums/BaySickRustyDrumsProcessor.h"
#include "../BaySickRustyDrums/BaySickRustyDrumsKitGraphic.h"
#include "../BaySickRustyDrums/AriaControlPanel.h"
#include "../VibeGraph.h"
#include "SampleLibrary.h"

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

BaySickRustyDrumsPage::BaySickRustyDrumsPage (VibeSynthProcessor& p)
    : mProcessor (p)
{
    buildDrumKitTab();
    buildPlayerTab();
    buildPianoRollTab();
    buildProgramCombo();

    addAndMakeVisible (*mDrumKitTab);
    addChildComponent (*mPlayerTab);
    addChildComponent (*mPianoRollTab);

    switchTab (0);
    startTimerHz (10);
}

BaySickRustyDrumsPage::~BaySickRustyDrumsPage() = default;

void BaySickRustyDrumsPage::paint (juce::Graphics& g)
{
    g.fillAll (VC::Bg);
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
    mAriaPanel = std::make_unique<AriaControlPanel> (mProcessor.getBaySickRustyDrums());
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
    if (! mProcessor.loadBaySickRustyDrumsKit (sfzPath))
        return false;
    if (mKitGraphic) mKitGraphic->setEngine (mProcessor.getBaySickRustyDrums());
    if (mKitGraphic) mKitGraphic->setKitLoaded (true);
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
    // directly — that path skips the kit-graphic + onKitLoaded plumbing.
    if (! loadKit (sfzPath))
        return false;

    // Sync program state so a later re-pick of the same program is a no-op
    // (otherwise loadProgram → loadKit re-runs and the kit's set_cc directives
    // stomp the saved CC values that the caller is about to replaceState in).
    mCurrentProgram = target;
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
    mAriaPanel->setEngine (mProcessor.getBaySickRustyDrums());

    const auto kitRoot = SampleLibrary::getCoreLibraryDir().getChildFile ("Big Rusty Drums");
    const auto xml     = programGuiXml (target);
    if (xml.existsAsFile())
        mAriaPanel->loadFromKit (kitRoot, xml);
    else
        mAriaPanel->clear();
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
        // First load — no confirm prompt, nothing to lose.
        loadProgram (target);
        return;
    }

    // Subsequent switch — confirm prompt; revert dropdown on cancel.
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
    loadAriaPanelForProgram (target);   // J-8 stage 2: render the ARIA control surface
    if (onSoundNameChanged) onSoundNameChanged (juce::String ("BaySickRustyDrums - ") + programLabel (target));
    return true;
}

void BaySickRustyDrumsPage::tearDownCurrentProgram()
{
    // Clear baySickRustyDrumsRoll on every pattern (full reset across the
    // project, since the new program may have a different kit).
    if (auto* pm = mProcessor.getPatternManager())
    {
        for (int i = 0; i < pm->getNumPatterns(); ++i)
            pm->getPattern (i).baySickRustyDrumsRoll.notes.clear();
    }

    // Reset Rusty mixer-strip + bus APVTS params to default.  PluginProcessor
    // exposes a helper that walks all `mixer_rusty_*` and `mixer_rustybus_*`
    // params and sets each to its registered default value.
    mProcessor.resetBaySickRustyDrumsMixerState();

    // J-8 stage 2 (2026-05-04): clear UI widgets BEFORE the engine is freed.
    // The ARIA panel's AriaKnob / AriaOptionMenu widgets hold SliderParameter
    // Attachments rooted in the engine's APVTS — destructing them after the
    // engine is gone is a use-after-free.  Same logic for the kit graphic
    // (which holds a raw engine pointer for hi-hat pedal state).
    if (mKitGraphic) { mKitGraphic->setEngine (nullptr); mKitGraphic->setKitLoaded (false); }
    if (mAriaPanel)  { mAriaPanel ->clear(); mAriaPanel->setEngine (nullptr); }

    // Tear down all Rusty InsertNodes (PluginProcessor's destroy method) and
    // mark the engine inactive.  The engine itself is rebuilt in loadKit().
    mProcessor.destroyBaySickRustyDrums();
}
