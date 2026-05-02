#include "InstPage.h"
#include "../VibePlayer/VibePlayerProcessor.h"
#include "../BaySickNAMIR/BaySickNAMIRProcessor.h"
#include "../Standalone/EnginePrefixUtil.h"
#include "../Standalone/PagePresetIO.h"
#include "../PluginProcessor.h"

namespace
{
    constexpr int kHeaderRowH = 36;
    constexpr int kPad        = 12;
    constexpr int kPickerW    = 220;
    constexpr int kFilenameW  = 320;
}

// 2026-04-29 DEBUG: drag-drop hook to test whether the Clips audio-routing
// bug also affects Inst.
bool InstPage::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (files.isEmpty()) return false;
    const juce::File f (files[0]);
    const auto ext = f.getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".mp3" || ext == ".aif" || ext == ".aiff"
        || ext == ".flac" || ext == ".ogg";
}

void InstPage::filesDropped (const juce::StringArray& files, int /*x*/, int /*y*/)
{
    if (files.isEmpty()) return;
    if (mEngineType != EngineType::BaySickPlayer)
        selectEngine (EngineType::BaySickPlayer);
    setClipFilePath (files[0]);
}

InstPage::InstPage (int pageIndex)
    : mPageIndex (pageIndex),
      mTabName   ("Inst " + juce::String (pageIndex + 1))
{
    // G-7: wire dirty-listener pointers (listener attaches in selectEngine).
    mDirtyListener.dirtyFlag = &mPageDirty;
    mDirtyListener.suppress  = &mSuppressDirty;

    buildEnginePicker();
    buildEQTab();

    addAndMakeVisible (mClipFileLabel);
    mClipFileLabel.setJustificationType (juce::Justification::centredLeft);
    mClipFileLabel.setColour (juce::Label::textColourId,        juce::Colour (0xff66bbff));
    mClipFileLabel.setColour (juce::Label::backgroundColourId,  juce::Colour (0xff1a1a1a));
    mClipFileLabel.setColour (juce::Label::outlineColourId,     juce::Colour (0xff333333));
    mClipFileLabel.setBorderSize ({ 2, 6, 2, 6 });
    mClipFileLabel.setText ("(no audio loaded)", juce::dontSendNotification);
    mClipFileLabel.setTooltip (
        "Audio file or recording bound to this Inst tab.  Loaded via the "
        "BaySickPlayer engine's file picker, or auto-bound by Inst recording.");

    switchTab (0);
}

InstPage::~InstPage()
{
    detachDirtyListener();
}

void InstPage::buildEnginePicker()
{
    addAndMakeVisible (mEnginePicker);
    mEnginePicker.setTextWhenNothingSelected ("Pick an engine");
    mEnginePicker.addItem ("BaySickPlayer", 1);
    mEnginePicker.addItem ("BaySickNAM/IR", 2);
    mEnginePicker.setTooltip (
        "Pick how the Inst track plays.  BaySickPlayer = sample playback "
        "(piano-roll triggered).  BaySickNAM/IR = re-amp through a neural "
        "amp model + IR cabinet.");
    mEnginePicker.onChange = [this]()
    {
        const int id = mEnginePicker.getSelectedId();
        if (id == 1)      selectEngine (EngineType::BaySickPlayer);
        else if (id == 2) selectEngine (EngineType::BaySickNAMIR);
    };
    // G-6 (2026-04-29): right-click → page context menu.
    mEnginePicker.onRightClick = [this] { showEngineContextMenu(); };
}

// ─────────────────────────────────────────────────────────────────────────────
// G-6 helpers — Inst preset folder + recursive folder→submenu walker.
// ─────────────────────────────────────────────────────────────────────────────
static juce::File instPresetsRootDir()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
              .getChildFile ("BaySickDAW")
              .getChildFile ("Presets")
              .getChildFile ("Inst");
}

static juce::File instMyPresetsDir()
{
    return instPresetsRootDir().getChildFile ("My Presets");
}

static void addInstPresetDirToMenu (juce::PopupMenu& menu,
                                    const juce::File& dir,
                                    int kPresetBase,
                                    juce::Array<juce::File>& presetXmls)
{
    juce::Array<juce::File> dirs, files;
    dir.findChildFiles (dirs,  juce::File::findDirectories, false);
    dir.findChildFiles (files, juce::File::findFiles,       false, "*.xml");
    dirs.sort();
    files.sort();
    for (auto& sub : dirs)
    {
        juce::PopupMenu subMenu;
        addInstPresetDirToMenu (subMenu, sub, kPresetBase, presetXmls);
        if (subMenu.getNumItems() > 0)
            menu.addSubMenu (sub.getFileName(), subMenu);
    }
    for (auto& f : files)
    {
        const int id = kPresetBase + presetXmls.size();
        presetXmls.add (f);
        menu.addItem (id, f.getFileNameWithoutExtension());
    }
}

void InstPage::showEngineContextMenu()
{
    constexpr int kIdLock           = 1;
    constexpr int kIdRename         = 5;
    constexpr int kIdDuplicate      = 12;
    constexpr int kIdSavePagePreset = 20;
    constexpr int kIdLoadPresetBase = 500;
    constexpr int kIdDelete         = 99;

    juce::PopupMenu menu;
    menu.addItem (kIdLock, "Lock", true, mLocked);

    menu.addSeparator();
    menu.addItem (kIdRename,    "Rename...");
    menu.addItem (kIdDuplicate, "Duplicate Inst (new tab)");

    menu.addSeparator();
    menu.addItem (kIdSavePagePreset, "Save Page Preset As...",
                  mPlayerProc != nullptr || mNamIrProc != nullptr);

    juce::Array<juce::File> presetXmls;
    {
        juce::PopupMenu loadSub;
        const auto root = instPresetsRootDir();
        if (root.isDirectory())
            addInstPresetDirToMenu (loadSub, root, kIdLoadPresetBase, presetXmls);
        if (presetXmls.isEmpty())
            loadSub.addItem (-1, "(no presets installed)", false, false);
        menu.addSubMenu ("Load Page Preset", loadSub);
    }

    menu.addSeparator();
    menu.addItem (kIdDelete, "Delete Inst", ! mLocked);

    juce::Component::SafePointer<InstPage> self (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&mEnginePicker),
        [self, presetXmls = std::move (presetXmls), kIdLoadPresetBase] (int r)
        {
            if (! self || r <= 0) return;
            if (r == kIdLock)        { self->setLocked (! self->mLocked); return; }
            if (r == kIdRename    && self->onRenameRequested)    { self->onRenameRequested();    return; }
            if (r == kIdDuplicate && self->onDuplicateRequested) { self->onDuplicateRequested(); return; }
            if (r == kIdSavePagePreset) { self->savePagePreset();   return; }   // G-7: full chain
            if (r == kIdDelete)         { self->requestDelete();    return; }   // G-7: prompt before delete
            if (r >= kIdLoadPresetBase
                && r <  kIdLoadPresetBase + presetXmls.size())
            {
                self->loadPagePreset (presetXmls[r - kIdLoadPresetBase]);   // G-7: full chain
                return;
            }
        });
}

void InstPage::saveInstPagePreset()
{
    auto* aw = new juce::AlertWindow ("Save Inst Page Preset",
                                       "Enter a name for this Inst page preset:",
                                       juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", "My Inst");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<InstPage> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, aw] (int r)
        {
            if (r != 1 || ! safeThis) return;
            const juce::String name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;

            auto dir = instMyPresetsDir();
            dir.createDirectory();
            auto target = dir.getChildFile (name + ".xml");
            int n = 2;
            while (target.exists())
                target = dir.getChildFile (name + " (" + juce::String (n++) + ").xml");

            const juce::String xml = safeThis->exportInstState();
            if (xml.isNotEmpty())
                target.replaceWithText (xml);
        }), true);
}

void InstPage::loadInstPagePreset (const juce::File& xml)
{
    if (! xml.existsAsFile()) return;
    const juce::String contents = xml.loadFileAsString();
    if (contents.isNotEmpty())
        importInstState (contents);
}

// ─────────────────────────────────────────────────────────────────────────────
// G-7 (2026-04-29): Page Preset save/load (full chain) + bus fallback.
// Bus fallback handles kInstBus2 / kInstBus3 → kInstBus when secondary
// buses aren't activated in the destination project.
// ─────────────────────────────────────────────────────────────────────────────
void InstPage::takeStateSnapshot()
{
    mPageDirty = false;
}

void InstPage::attachDirtyListener()
{
    detachDirtyListener();
    if (auto* vp = dynamic_cast<VibePlayerProcessor*> (mPlayerProc.get()))
        vp->apvts.state.addListener (&mDirtyListener);
    if (auto* nm = dynamic_cast<BaySickNAMIRProcessor*> (mNamIrProc.get()))
        nm->apvts.state.addListener (&mDirtyListener);
}

void InstPage::detachDirtyListener()
{
    if (auto* vp = dynamic_cast<VibePlayerProcessor*> (mPlayerProc.get()))
        vp->apvts.state.removeListener (&mDirtyListener);
    if (auto* nm = dynamic_cast<BaySickNAMIRProcessor*> (mNamIrProc.get()))
        nm->apvts.state.removeListener (&mDirtyListener);
}

static juce::String instEnginePrefixOf (juce::AudioProcessor* p)
{
    if (auto* vp = dynamic_cast<VibePlayerProcessor*>   (p)) return vp->getParamPrefix();
    // BaySickNAMIRProcessor doesn't use a per-tab prefix today (single
    // instance), so engine prefix is empty for that branch.
    return {};
}

static juce::String instEngineTypeTag (InstPage::EngineType e)
{
    switch (e)
    {
        case InstPage::EngineType::BaySickPlayer: return "BaySickPlayer";
        case InstPage::EngineType::BaySickNAMIR:  return "BaySickNAMIR";
        case InstPage::EngineType::None:          return {};
    }
    return {};
}

void InstPage::savePagePreset (std::function<void()> onSaved)
{
    if (mFullProcessor == nullptr || getEngineProcessor() == nullptr)
    {
        saveInstPagePreset();
        if (onSaved) onSaved();
        return;
    }

    auto* aw = new juce::AlertWindow ("Save Page Preset",
                                       "Enter a name for this inst page preset:",
                                       juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", "My Inst");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<InstPage> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, aw, onSaved] (int r)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (r != 1 || ! safeThis) return;

            const juce::String name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;

            auto dir = PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Inst);
            dir.createDirectory();
            auto target = dir.getChildFile (name + ".xml");
            int n = 2;
            while (target.exists())
                target = dir.getChildFile (name + " (" + juce::String (n++) + ").xml");

            const juce::String stripPrefix   = "mixer_inst_" + juce::String (safeThis->mPageIndex);
            const juce::String engineType    = instEngineTypeTag (safeThis->mEngineType);
            const juce::String enginePrefix  = instEnginePrefixOf (safeThis->getEngineProcessor());

            const juce::String xml = PagePresetIO::exportPagePreset (
                *safeThis->mFullProcessor,
                PagePresetIO::PageKind::Inst,
                safeThis->mPageIndex,
                stripPrefix,
                safeThis->getEngineProcessor(),
                engineType,
                enginePrefix);

            if (xml.isNotEmpty())
                target.replaceWithText (xml);

            safeThis->takeStateSnapshot();
            if (onSaved) onSaved();   // G-7: chain delete after save completes
        }), false);
}

void InstPage::loadPagePreset (const juce::File& xml)
{
    if (! xml.existsAsFile()) return;
    if (mFullProcessor == nullptr) { loadInstPagePreset (xml); return; }

    // Peek the engineType so we can switch engines first.  If empty, leave
    // the current engine alone.
    auto parsed = juce::XmlDocument::parse (xml.loadFileAsString());
    if (parsed && parsed->hasTagName ("BaySickPagePreset"))
    {
        const juce::String engType = parsed->getStringAttribute ("engineType");
        if      (engType == "BaySickPlayer" && mEngineType != EngineType::BaySickPlayer)
            selectEngine (EngineType::BaySickPlayer);
        else if (engType == "BaySickNAMIR"  && mEngineType != EngineType::BaySickNAMIR)
            selectEngine (EngineType::BaySickNAMIR);
    }

    const juce::String stripPrefix  = "mixer_inst_" + juce::String (mPageIndex);
    const juce::String enginePrefix = instEnginePrefixOf (getEngineProcessor());

    auto query = mBusActiveQuery
                    ? mBusActiveQuery
                    : std::function<bool(int)> ([] (int) { return true; });

    // G-7: suppress dirty during bulk state restore.
    mSuppressDirty = true;
    PagePresetIO::importPagePreset (*mFullProcessor,
                                     PagePresetIO::PageKind::Inst,
                                     mPageIndex,
                                     stripPrefix,
                                     getEngineProcessor(),
                                     enginePrefix,
                                     query,
                                     xml.loadFileAsString());
    mSuppressDirty = false;
    takeStateSnapshot();
}

void InstPage::showPageActionsMenu (juce::Component* anchor)
{
    constexpr int kIdSavePagePreset = 100;
    constexpr int kIdLoadBase       = 1000;

    juce::PopupMenu menu;
    menu.addItem (kIdSavePagePreset, "Save Page Preset As...",
                  getEngineProcessor() != nullptr);

    juce::Array<juce::File> presetXmls;
    {
        juce::PopupMenu loadSub;
        const auto root = PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Inst);
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

    juce::Component::SafePointer<InstPage> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (anchor),
        [safeThis, presetXmls = std::move (presetXmls), kIdLoadBase] (int r)
        {
            if (! safeThis || r <= 0) return;
            if (r == kIdSavePagePreset) { safeThis->savePagePreset(); return; }
            if (r >= kIdLoadBase && r < kIdLoadBase + presetXmls.size())
            {
                safeThis->loadPagePreset (presetXmls[r - kIdLoadBase]);
                return;
            }
        });
}

void InstPage::requestDelete()
{
    juce::Component::SafePointer<InstPage> safeThis (this);
    auto fireDelete = [safeThis] {
        if (auto* p = safeThis.getComponent())
            if (p->onDeleteRequested) p->onDeleteRequested();
    };

    const juce::String warning =
        "Deleting this inst tab removes its Player, Mixer Strip, "
        "Effects Rack, and Piano Roll.\n"
        "Audio files in your project's Audio folder will be kept.";

    if (getEngineProcessor() != nullptr && isPatchDirty())
    {
        const juce::String dirtyWarning = warning
            + "\n\n\"Save Page Preset & Delete\" writes the entire page state "
              "(engine + EQ + effects rack + strip settings) to disk first, "
              "then deletes the tab.";

        auto* aw = new juce::AlertWindow (
            "Delete Inst", dirtyWarning, juce::AlertWindow::QuestionIcon);
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
        "Delete Inst", warning, juce::AlertWindow::WarningIcon);
    aw->addButton ("Delete", 1);
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [aw, fireDelete] (int r)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (r == 1) fireDelete();
        }), false);
}

void InstPage::buildEQTab()
{
    // G-7 polish (2026-04-29): real ParametricEQDisplay, mirrors LayersPage.
    mEQDisplay = std::make_unique<ParametricEQDisplay>();
    if (mFullProcessor)
        mEQDisplay->setSampleRate (mFullProcessor->getSampleRate() > 0.0
                                       ? mFullProcessor->getSampleRate() : 44100.0);
    mEQDisplay->showMidSideToggle (false);   // M/S driven by external buttons
    addChildComponent (*mEQDisplay);
}

juce::AudioProcessor* InstPage::getEngineProcessor() const noexcept
{
    return mEngineType == EngineType::BaySickPlayer ? mPlayerProc.get()
         : mEngineType == EngineType::BaySickNAMIR  ? mNamIrProc .get()
         :                                            nullptr;
}

void InstPage::selectEngine (EngineType e)
{
    if (e == mEngineType) return;

    if (onEngineDestroying) onEngineDestroying();

    if (e == EngineType::BaySickPlayer && ! mPlayerProc)
    {
        const juce::String prefix = "inst_" + juce::String (mPageIndex) + "_";
        auto vp = std::make_unique<VibePlayerProcessor> (prefix);
        vp->prepareToPlay (44100.0, 512);
        if (mClipPath.isNotEmpty())
            vp->loadSampleFile (juce::File (mClipPath));
        mPlayerProc = std::move (vp);
        mPlayerEditor.reset (mPlayerProc->createEditor());
        if (mPlayerEditor) addChildComponent (*mPlayerEditor);
    }
    else if (e == EngineType::BaySickNAMIR && ! mNamIrProc)
    {
        auto nam = std::make_unique<BaySickNAMIRProcessor>();
        nam->prepareToPlay (44100.0, 512);
        mNamIrProc = std::move (nam);
        mNamIrEditor.reset (mNamIrProc->createEditor());
        if (mNamIrEditor) addChildComponent (*mNamIrEditor);
    }

    mEngineType = e;
    if (mPlayerEditor) mPlayerEditor->setVisible (e == EngineType::BaySickPlayer && mActiveTab == 0);
    if (mNamIrEditor)  mNamIrEditor ->setVisible (e == EngineType::BaySickNAMIR  && mActiveTab == 0);
    resized();

    const int id = (e == EngineType::BaySickPlayer) ? 1
                 : (e == EngineType::BaySickNAMIR)  ? 2 : 0;
    mEnginePicker.setSelectedId (id, juce::dontSendNotification);

    repaint();

    // G-7 polish (2026-04-29): bind Pre EQ8 M/S to Inst InsertNode's preEq +
    // mixer_inst_<idx>_preeq_(mid|side)_eq APVTS prefix.
    if (mEQDisplay && mFullProcessor)
    {
        if (auto* preEq = mFullProcessor->mVibeGraph.getInsertPreEQ (
                              VibeGraph::InsertKind::Inst, mPageIndex))
        {
            const juce::String mixerPrefix = "mixer_inst_" + juce::String (mPageIndex);
            mEQDisplay->bindMsDSP (preEq, &mFullProcessor->apvts,
                                    mixerPrefix + "_preeq_mid_eq",
                                    mixerPrefix + "_preeq_side_eq");
            mEQDisplay->setStripContext(mixerPrefix,
                [](int id){ return MixerChannelIds::friendlyName(id); });
        }
        const double sr = mFullProcessor->getSampleRate() > 0.0
                              ? mFullProcessor->getSampleRate() : 44100.0;
        mEQDisplay->setSampleRate (sr);
    }

    // G-7 (2026-04-29): hook dirty-tracker on the new engine's apvts and
    // reset the flag.
    attachDirtyListener();
    takeStateSnapshot();

    if (onEngineChanged) onEngineChanged();
}

juce::AudioProcessorEditor* InstPage::activeEditor() const
{
    return mEngineType == EngineType::BaySickPlayer ? mPlayerEditor.get()
         : mEngineType == EngineType::BaySickNAMIR  ? mNamIrEditor .get()
         :                                            nullptr;
}

void InstPage::switchTab (int idx)
{
    // G-4 (2026-04-28): Inst has 2 sub-tabs — 0 = Player, 1 = Pre EQ8 M/S.
    // Piano Roll removed; Inst is a live-input / recorded-audio destination
    // (BaySickPlayer for sample playback, BaySickNAM/IR for re-amping).
    mActiveTab = juce::jlimit (0, 1, idx);

    if (mPlayerEditor) mPlayerEditor->setVisible (mEngineType == EngineType::BaySickPlayer && mActiveTab == 0);
    if (mNamIrEditor)  mNamIrEditor ->setVisible (mEngineType == EngineType::BaySickNAMIR  && mActiveTab == 0);
    if (mEQDisplay)    mEQDisplay   ->setVisible (mActiveTab == 1);

    resized();
    repaint();
}

void InstPage::setClipFilePath (const juce::String& p)
{
    mClipPath = p;
    mClipFileLabel.setText (p.isNotEmpty()
                                ? juce::File (p).getFileName()
                                : juce::String ("(no audio loaded)"),
                            juce::dontSendNotification);

    if (auto* vp = dynamic_cast<VibePlayerProcessor*> (mPlayerProc.get()))
        if (p.isNotEmpty())
            vp->loadSampleFile (juce::File (p));
}

// ─────────────────────────────────────────────────────────────────────────────
// G-6 (2026-04-29): full-state export/import for Duplicate flow.
// Mirrors ClipsPage's dual-engine pattern (BaySickPlayer + BaySickNAMIR).
// VibePlayer state gets prefix-substituted (inst_<idx>_*); NAM/IR uses
// unprefixed params so direct setStateInformation works.
// ─────────────────────────────────────────────────────────────────────────────
juce::String InstPage::exportInstState() const
{
    juce::XmlElement el ("InstPageState");
    el.setAttribute ("active",  (int) mEngineType);
    el.setAttribute ("pageIdx", mPageIndex);
    el.setAttribute ("locked",  mLocked ? 1 : 0);

    if (mPlayerProc != nullptr)
    {
        juce::MemoryBlock mb;
        mPlayerProc->getStateInformation (mb);
        auto* sub = el.createNewChildElement ("PlayerState");
        sub->setAttribute ("data", mb.toBase64Encoding());
        // G-6 fix (2026-04-29): save actual processor prefix (see ClipsPage
        // for rationale).
        if (auto* vp = dynamic_cast<VibePlayerProcessor*> (mPlayerProc.get()))
            sub->setAttribute ("prefix", vp->getParamPrefix());
    }
    if (mNamIrProc != nullptr)
    {
        juce::MemoryBlock mb;
        mNamIrProc->getStateInformation (mb);
        auto* sub = el.createNewChildElement ("NamIrState");
        sub->setAttribute ("data", mb.toBase64Encoding());
    }
    // BaySickPedals state export lands when Phase I ships.

    return el.toString (juce::XmlElement::TextFormat().singleLine());
}

void InstPage::importInstState (const juce::String& xml)
{
    if (xml.isEmpty()) return;
    auto parsed = juce::XmlDocument::parse (xml);
    if (! parsed || ! parsed->hasTagName ("InstPageState")) return;

    const auto sourceActive = (EngineType) parsed->getIntAttribute (
                                  "active", (int) EngineType::None);

    if (auto* playerEl = parsed->getChildByName ("PlayerState"))
    {
        if (mPlayerProc == nullptr) selectEngine (EngineType::BaySickPlayer);
        if (mPlayerProc != nullptr)
        {
            juce::MemoryBlock mb;
            if (mb.fromBase64Encoding (playerEl->getStringAttribute ("data")))
            {
                const juce::String srcPrefix = playerEl->getStringAttribute ("prefix");
                juce::String dstPrefix;
                if (auto* vp = dynamic_cast<VibePlayerProcessor*> (mPlayerProc.get()))
                    dstPrefix = vp->getParamPrefix();
                substituteApvtsPrefixInBinary (mb, srcPrefix, dstPrefix);
                // G-7: suppress dirty during bulk state restore.
                mSuppressDirty = true;
                mPlayerProc->setStateInformation (mb.getData(), (int) mb.getSize());
                mSuppressDirty = false;
            }
        }
    }
    if (auto* namIrEl = parsed->getChildByName ("NamIrState"))
    {
        if (mNamIrProc == nullptr) selectEngine (EngineType::BaySickNAMIR);
        if (mNamIrProc != nullptr)
        {
            juce::MemoryBlock mb;
            if (mb.fromBase64Encoding (namIrEl->getStringAttribute ("data")))
            {
                mSuppressDirty = true;
                mNamIrProc->setStateInformation (mb.getData(), (int) mb.getSize());
                mSuppressDirty = false;
            }
        }
    }

    if (sourceActive != EngineType::None && sourceActive != mEngineType)
        selectEngine (sourceActive);

    // G-6: restore lock state.
    setLocked (parsed->getIntAttribute ("locked", 0) != 0);

    if (mClipPath.isNotEmpty())
        setClipFilePath (mClipPath);
}

void InstPage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181818));
    g.setColour (juce::Colour (0xff0a0a0a));
    g.fillRect (0, 0, getWidth(), kHeaderRowH);
    g.setColour (juce::Colour (0xff333333));
    g.fillRect (0, kHeaderRowH, getWidth(), 1);
}

void InstPage::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (kHeaderRowH).reduced (kPad, 6);
    mEnginePicker.setBounds (header.removeFromLeft (kPickerW));
    header.removeFromLeft (kPad);
    mClipFileLabel.setBounds (header.removeFromLeft (juce::jmin (kFilenameW, header.getWidth())));
    layoutEditor (r);
}

void InstPage::layoutEditor (juce::Rectangle<int> r)
{
    if (auto* ed = activeEditor(); ed && ed->isVisible())
        ed->setBounds (r);
    if (mEQDisplay && mEQDisplay->isVisible())
        mEQDisplay->setBounds (r.reduced (4));
}

// ─────────────────────────────────────────────────────────────────────────────
// InstEmptyState — text-only placeholder.  Mixer button is the spawn trigger.
// ─────────────────────────────────────────────────────────────────────────────
InstEmptyState::InstEmptyState()
{
    setInterceptsMouseClicks (false, false);
}

void InstEmptyState::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181818));

    auto bounds = getLocalBounds().toFloat().reduced (32.0f);
    juce::Path p;
    p.addRoundedRectangle (bounds, 12.0f);
    juce::Path dashed;
    const float dashes[] = { 8.0f, 6.0f };
    juce::PathStrokeType (2.0f).createDashedStroke (dashed, p, dashes, 2);
    g.setColour (juce::Colour (0xff1c3a8a).withAlpha (0.6f));
    g.fillPath (dashed);

    g.setColour (juce::Colour (0xffc0c0c0));
    g.setFont (juce::Font (18.0f, juce::Font::plain));
    g.drawText (
        "Click 'Add Inst Strip' on the Mixer page to start an Inst page",
        bounds.toNearestInt(),
        juce::Justification::centred,
        true);
}
