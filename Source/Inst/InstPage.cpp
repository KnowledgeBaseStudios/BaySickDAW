#include "InstPage.h"
#include "../BaySickNAMIR/BaySickNAMIRProcessor.h"
#include "../Standalone/EnginePrefixUtil.h"
#include "../Standalone/PagePresetIO.h"
#include "../PluginProcessor.h"

namespace
{
    constexpr int kHeaderRowH = 36;
    constexpr int kPad        = 12;
    constexpr int kFilenameW  = 320;
}

// I-0b (2026-05-02): Pedals tab placeholder.  I-15 will replace this with
// the real BaySickPedals editor (4x4 hardware-rack layout).  Until then the
// page renders a centered "BaySickPedals (I-1+)" message so the sub-tab is
// navigable but doesn't surface unfinished UI.
namespace
{
    class PedalsPlaceholder : public juce::Component
    {
    public:
        PedalsPlaceholder() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override
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
                "BaySickPedals (skeleton present; rack UI lands in I-15)",
                bounds.toNearestInt(),
                juce::Justification::centred,
                true);
        }
    };
}

InstPage::InstPage (int pageIndex)
    : mPageIndex (pageIndex),
      mTabName   ("Inst " + juce::String (pageIndex + 1))
{
    // G-7: wire dirty-listener pointers.
    mDirtyListener.dirtyFlag = &mPageDirty;
    mDirtyListener.suppress  = &mSuppressDirty;

    // I-0b: file label retained for header chrome (re-parented into the
    // PageMenuBar's right slot when the page becomes visible -- mirrors Vox).
    // Not addAndMakeVisible'd directly on the page because the menu bar will
    // host it.
    mClipFileLabel.setJustificationType (juce::Justification::centredLeft);
    mClipFileLabel.setColour (juce::Label::textColourId,        juce::Colour (0xff66bbff));
    mClipFileLabel.setColour (juce::Label::backgroundColourId,  juce::Colour (0xff1a1a1a));
    mClipFileLabel.setColour (juce::Label::outlineColourId,     juce::Colour (0xff333333));
    mClipFileLabel.setBorderSize ({ 2, 6, 2, 6 });
    mClipFileLabel.setText ("(no audio loaded)", juce::dontSendNotification);
    mClipFileLabel.setTooltip (
        "Audio file or recording bound to this Inst tab.  BaySickNAM/IR re-amp "
        "and BaySickPedals consume this once routing lands.");

    // I-0b: instantiate both stage processors unconditionally.  Mirrors how
    // Vox instantiates BaySickVocalProcessor in its ctor.  No engine picker
    // -- both stages are permanent on the page.
    {
        auto nam = std::make_unique<BaySickNAMIRProcessor>();
        nam->prepareToPlay (44100.0, 512);
        mNamIrProc = std::move (nam);
        mNamIrEditor.reset (mNamIrProc->createEditor());
        if (mNamIrEditor) addChildComponent (*mNamIrEditor);
    }

    // BaySickPedals: I-1 will instantiate the real processor here.  For I-0b
    // ship a placeholder component for sub-tab 0.
    mPedalsPlaceholder = std::make_unique<PedalsPlaceholder>();
    addChildComponent (*mPedalsPlaceholder);

    buildEQTab();

    // Hook dirty tracker for the live engine.
    attachDirtyListener();

    switchTab (0);
}

InstPage::~InstPage()
{
    detachDirtyListener();
}

// ─────────────────────────────────────────────────────────────────────────────
// Inst page-level preset folder + recursive folder→submenu walker (G-6 helpers,
// retained verbatim).
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
                  mNamIrProc != nullptr);

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
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
        [self, presetXmls = std::move (presetXmls), kIdLoadPresetBase] (int r)
        {
            if (! self || r <= 0) return;
            if (r == kIdLock)        { self->setLocked (! self->mLocked); return; }
            if (r == kIdRename    && self->onRenameRequested)    { self->onRenameRequested();    return; }
            if (r == kIdDuplicate && self->onDuplicateRequested) { self->onDuplicateRequested(); return; }
            if (r == kIdSavePagePreset) { self->savePagePreset();   return; }
            if (r == kIdDelete)         { self->requestDelete();    return; }
            if (r >= kIdLoadPresetBase
                && r <  kIdLoadPresetBase + presetXmls.size())
            {
                self->loadPagePreset (presetXmls[r - kIdLoadPresetBase]);
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
// ─────────────────────────────────────────────────────────────────────────────
void InstPage::takeStateSnapshot()
{
    mPageDirty = false;
}

void InstPage::attachDirtyListener()
{
    detachDirtyListener();
    if (auto* nm = dynamic_cast<BaySickNAMIRProcessor*> (mNamIrProc.get()))
        nm->apvts.state.addListener (&mDirtyListener);
}

void InstPage::detachDirtyListener()
{
    if (auto* nm = dynamic_cast<BaySickNAMIRProcessor*> (mNamIrProc.get()))
        nm->apvts.state.removeListener (&mDirtyListener);
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
            // I-0b: BaySickNAM/IR is the only saved engine in I-0b (BaySickPedals
            // ships its own state in I-1 via a separate <PedalsState> child).
            const juce::String engineType    = "BaySickNAMIR";
            const juce::String enginePrefix  = {};   // NAM/IR uses unprefixed APVTS

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
            if (onSaved) onSaved();
        }), false);
}

void InstPage::loadPagePreset (const juce::File& xml)
{
    if (! xml.existsAsFile()) return;
    if (mFullProcessor == nullptr) { loadInstPagePreset (xml); return; }

    // I-0b: BaySickNAM/IR is the live engine (no engine picker; nothing to
    // switch).  Just hand the saved blob to it via PagePresetIO.
    const juce::String stripPrefix  = "mixer_inst_" + juce::String (mPageIndex);
    const juce::String enginePrefix = {};   // NAM/IR uses unprefixed APVTS

    auto query = mBusActiveQuery
                    ? mBusActiveQuery
                    : std::function<bool(int)> ([] (int) { return true; });

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
        "Deleting this inst tab removes its BaySickPedals, BaySickNAM/IR, "
        "Mixer Strip, and Effects Rack.\n"
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
                if (r == 1) safeThis->savePagePreset (fireDelete);
                else if (r == 2) fireDelete();
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
    mEQDisplay = std::make_unique<ParametricEQDisplay>();
    if (mFullProcessor)
        mEQDisplay->setSampleRate (mFullProcessor->getSampleRate() > 0.0
                                       ? mFullProcessor->getSampleRate() : 44100.0);
    mEQDisplay->showMidSideToggle (false);
    addChildComponent (*mEQDisplay);
}

juce::AudioProcessor* InstPage::getEngineProcessor() const noexcept
{
    return mNamIrProc.get();
}

void InstPage::setProcessor (VibeSynthProcessor* p)
{
    mFullProcessor = p;
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
}

void InstPage::switchTab (int idx)
{
    // I-0b (2026-05-02): 3 sub-tabs.
    //   0 = BaySickPedals (placeholder until I-15 ships the rack UI)
    //   1 = BaySickNAM/IR  (existing BaySickNAMIRProcessor + editor)
    //   2 = Pre EQ8 M/S    (existing ParametricEQDisplay)
    mActiveTab = juce::jlimit (0, 2, idx);

    if (mPedalsPlaceholder) mPedalsPlaceholder->setVisible (mActiveTab == 0);
    if (mPedalsEditor)      mPedalsEditor    ->setVisible (mActiveTab == 0);
    if (mNamIrEditor)       mNamIrEditor     ->setVisible (mActiveTab == 1);
    if (mEQDisplay)         mEQDisplay       ->setVisible (mActiveTab == 2);

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
}

// ─────────────────────────────────────────────────────────────────────────────
// G-6 (2026-04-29): full-state export/import for Duplicate flow.
// I-0b: only BaySickNAM/IR ships state in I-0b.  I-1 will add a <PedalsState>
// child for the BaySickPedals processor when it lands.
// ─────────────────────────────────────────────────────────────────────────────
juce::String InstPage::exportInstState() const
{
    juce::XmlElement el ("InstPageState");
    el.setAttribute ("active",  0);
    el.setAttribute ("pageIdx", mPageIndex);
    el.setAttribute ("locked",  mLocked ? 1 : 0);

    if (mNamIrProc != nullptr)
    {
        juce::MemoryBlock mb;
        mNamIrProc->getStateInformation (mb);
        auto* sub = el.createNewChildElement ("NamIrState");
        sub->setAttribute ("data", mb.toBase64Encoding());
    }
    // I-1: BaySickPedals state export lands when the processor ships.

    return el.toString (juce::XmlElement::TextFormat().singleLine());
}

void InstPage::importInstState (const juce::String& xml)
{
    if (xml.isEmpty()) return;
    auto parsed = juce::XmlDocument::parse (xml);
    if (! parsed || ! parsed->hasTagName ("InstPageState")) return;

    if (auto* namIrEl = parsed->getChildByName ("NamIrState"))
    {
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
    // I-1: BaySickPedals state import lands when the processor ships.
    // I-0b: any old <PlayerState> child from pre-Phase-I saves is silently
    // ignored (per the locked spec call -- no projects in the wild had it).

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
    r.removeFromTop (kHeaderRowH);
    layoutContent (r);
}

void InstPage::layoutContent (juce::Rectangle<int> r)
{
    if (mPedalsPlaceholder && mPedalsPlaceholder->isVisible())
        mPedalsPlaceholder->setBounds (r);
    if (mPedalsEditor && mPedalsEditor->isVisible())
        mPedalsEditor->setBounds (r);
    if (mNamIrEditor && mNamIrEditor->isVisible())
        mNamIrEditor->setBounds (r);
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
