#include "InstPage.h"
#include "../BaySickNAMIR/BaySickNAMIRProcessor.h"
#include "../BaySickPedals/BaySickPedalsProcessor.h"
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

    // I-15 (2026-05-03): real BaySickPedals processor + 4x2 rack editor.
    {
        auto pedals = std::make_unique<BaySickPedalsProcessor>();
        pedals->prepareToPlay (44100.0, 512);
        mPedalsProc = std::move (pedals);
        mPedalsEditor.reset (mPedalsProc->createEditor());
        if (mPedalsEditor) addChildComponent (*mPedalsEditor);
    }
    // Placeholder is no longer used post-I-15 but the field stays declared so
    // the layoutContent() guarded path below still compiles.
    (void) mPedalsPlaceholder;

    // I-16 G-9 (2026-05-03): chain wrapper now that both stages exist.
    // registerInstEngine receives this single pointer; its processBlock fans
    // through Pedals -> NAM/IR.  Each stage's prepareToPlay was already
    // called above; setChain() picks up the existing prepared state.
    mChain = std::make_unique<EngineChainProcessor>();
    mChain->setChain ({ mPedalsProc.get(), mNamIrProc.get() });

    // I-15 polish (2026-05-03): BaySickPedals sub-tab header chrome.  Lives
    // in the 36-px page header strip; hidden on other sub-tabs.
    mPedalsHeaderTitle.setText ("BaySickPedals", juce::dontSendNotification);
    mPedalsHeaderTitle.setJustificationType (juce::Justification::centredLeft);
    mPedalsHeaderTitle.setFont (juce::Font (16.0f, juce::Font::bold));
    mPedalsHeaderTitle.setColour (juce::Label::textColourId, juce::Colour (0xffe0e0e0));
    addChildComponent (mPedalsHeaderTitle);

    mPedalsPresetBtn = std::make_unique<juce::TextButton>("Preset...");
    mPedalsPresetBtn->setTooltip (
        "Pedalboard preset library -- save / load the entire 8-slot rack "
        "configuration as a single .xml under Documents/BaySickDAW/Presets/Pedalboards/");
    mPedalsPresetBtn->onClick = [this] { showPedalboardPresetMenu(); };
    addChildComponent (*mPedalsPresetBtn);

    // J-6 EQ unification (2026-05-03): buildEQTab removed; pre-rack EQ on Effects page only.

    // Hook dirty tracker for the live engine.
    attachDirtyListener();

    switchTab (0);
}

InstPage::~InstPage()
{
    detachDirtyListener();

    // I-15 polish (2026-05-03): unregister this page's NAM/IR engine from
    // VibeSynthProcessor's audio-thread routing BEFORE tearing down anything.
    // The TunerStyleDSP -> PitchTrackerYIN worker join can take a few ms during
    // mPedalsProc.reset() below; without this unregister the audio thread can
    // continue calling our (partially destroyed) NAM/IR processor during that
    // window and crash inside MicPlacementDSP.  StandaloneEditor's tab-close
    // path also calls unregisterInstEngine, so on tab-close this is a redundant
    // no-op (unregister is idempotent); on app-shutdown it's load-bearing.
    if (mFullProcessor != nullptr)
        mFullProcessor->unregisterInstEngine (mPageIndex);

    // Explicitly destroy editors before their processors so editor child
    // components don't access dangling APVTS / DSP pointers during teardown.
    // I-16 G-9: chain holds raw pointers into Pedals + NAM/IR -- tear it down
    // first so processBlock can't fire on stale pointers if the audio thread
    // is mid-callback.
    mChain       .reset();
    mPedalsEditor.reset();
    mNamIrEditor .reset();
    mPedalsProc  .reset();
    mNamIrProc   .reset();
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

// J-6 EQ unification (2026-05-03): buildEQTab removed.

juce::AudioProcessor* InstPage::getEngineProcessor() const noexcept
{
    // I-16 G-9 (2026-05-03): return chain wrapper so registerInstEngine sees
    // a single processor whose processBlock drives Pedals -> NAM/IR in order.
    // Pre-G-9 this returned only mNamIrProc (Pedals were bypassed by audio).
    return mChain.get();
}

void InstPage::setProcessor (VibeSynthProcessor* p)
{
    mFullProcessor = p;
    // J-6 EQ unification (2026-05-03): page-level EQ binding removed; pre-rack
    // EQ is bound exclusively by EffectsPage (mixer_inst_<N>_preeq_*).
}

void InstPage::switchTab (int idx)
{
    // I-0b (2026-05-02) / J-6 (2026-05-03): 2 sub-tabs (was 3 before EQ unification).
    //   0 = BaySickPedals (placeholder until I-15 ships the rack UI)
    //   1 = BaySickNAM/IR  (existing BaySickNAMIRProcessor + editor)
    mActiveTab = juce::jlimit (0, 1, idx);

    if (mPedalsPlaceholder) mPedalsPlaceholder->setVisible (mActiveTab == 0);
    if (mPedalsEditor)      mPedalsEditor    ->setVisible (mActiveTab == 0);
    if (mNamIrEditor)       mNamIrEditor     ->setVisible (mActiveTab == 1);

    // I-15 polish: BaySickPedals header chrome only on sub-tab 0.
    mPedalsHeaderTitle.setVisible (mActiveTab == 0);
    if (mPedalsPresetBtn) mPedalsPresetBtn->setVisible (mActiveTab == 0);

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
    // I-15 (2026-05-03): BaySickPedals state export.
    if (mPedalsProc != nullptr)
    {
        juce::MemoryBlock mb;
        mPedalsProc->getStateInformation (mb);
        auto* sub = el.createNewChildElement ("PedalsState");
        sub->setAttribute ("data", mb.toBase64Encoding());
    }

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
    // I-15 (2026-05-03): BaySickPedals state import.
    if (auto* pedalsEl = parsed->getChildByName ("PedalsState"))
    {
        if (mPedalsProc != nullptr)
        {
            juce::MemoryBlock mb;
            if (mb.fromBase64Encoding (pedalsEl->getStringAttribute ("data")))
            {
                mSuppressDirty = true;
                mPedalsProc->setStateInformation (mb.getData(), (int) mb.getSize());
                mSuppressDirty = false;
            }
        }
    }
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

// ─────────────────────────────────────────────────────────────────────────────
// I-15 polish: pedalboard preset library popup -- save/load the entire
// 8-slot rack as Documents/BaySickDAW/Presets/Pedalboards/{name}.xml.
// Distinct from the Page Preset (hamburger menu, full chain) and per-pedal
// presets (the "..." button on each tile).
// ─────────────────────────────────────────────────────────────────────────────
void InstPage::showPedalboardPresetMenu()
{
    auto* pedals = dynamic_cast<BaySickPedalsProcessor*> (mPedalsProc.get());
    if (pedals == nullptr) return;

    auto presets = pedals->enumeratePedalboardPresets();

    juce::PopupMenu m;
    m.addItem (1, "Save Pedalboard As...");
    m.addSeparator();
    if (presets.isEmpty())
    {
        m.addItem (-1, "(no saved pedalboards)", false);
    }
    else
    {
        for (int i = 0; i < presets.size(); ++i)
            m.addItem (1000 + i, presets[i].getFileNameWithoutExtension());
    }
    m.addSeparator();
    m.addItem (2, "Reveal Folder...");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (mPedalsPresetBtn.get()),
        [this, pedals, presets] (int r)
        {
            if (r == 0) return;
            if (r == 1)
            {
                auto* aw = new juce::AlertWindow (
                    "Save Pedalboard Preset",
                    "Pedalboard preset name:",
                    juce::AlertWindow::NoIcon);
                aw->addTextEditor ("name", "");
                aw->addButton ("OK",     1, juce::KeyPress (juce::KeyPress::returnKey));
                aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                aw->enterModalState (true,
                    juce::ModalCallbackFunction::create (
                        [aw, pedals] (int result)
                        {
                            if (result == 1)
                            {
                                const auto name = aw->getTextEditorContents ("name").trim();
                                if (name.isNotEmpty())
                                {
                                    juce::String err;
                                    pedals->savePedalboardPreset (name, err);
                                }
                            }
                            delete aw;
                        }),
                    false);
                return;
            }
            if (r == 2)
            {
                BaySickPedalsProcessor::pedalboardPresetsRoot().revealToUser();
                return;
            }
            if (r >= 1000)
            {
                const int idx = r - 1000;
                if (idx >= 0 && idx < presets.size())
                {
                    juce::String err;
                    pedals->loadPedalboardPreset (presets[idx], err);
                }
            }
        });
}

void InstPage::resized()
{
    auto r = getLocalBounds();

    // I-15 polish (2026-05-03): BaySickPedals header chrome lives in the
    // 36-px page header strip.  Title flush-left, preset button flush-right.
    auto header = r.removeFromTop (kHeaderRowH);
    if (mPedalsPresetBtn && mPedalsPresetBtn->isVisible())
        mPedalsPresetBtn->setBounds (header.removeFromRight (90).reduced (8, 6));
    if (mPedalsHeaderTitle.isVisible())
        mPedalsHeaderTitle.setBounds (header.reduced (12, 4));

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
    // J-6 EQ unification (2026-05-03): mEQDisplay layout removed.
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
