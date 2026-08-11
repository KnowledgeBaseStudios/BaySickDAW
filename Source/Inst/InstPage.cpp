#include "InstPage.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)
#include "../BaySickNAMIR/BaySickNAMIRProcessor.h"
#include "../BaySickNAMIR/BaySickNAMIREditor.h"   // QA-ApvtsAutomation: setAutomationPrefix
#include "../BaySickPedals/BaySickPedalsProcessor.h"
#include "../BaySickPedals/BaySickPedalsEditor.h"           // QA-A 4.4 (2026-05-09): wire onPedalboardPresetMenu
#include "../BaySickGuitars/BaySickGuitarsProcessor.h"   // K-2: sfizz Guitars front-end
#include "../BaySickBasses/BaySickBassesProcessor.h"     // L-2: sfizz Basses front-end
#include "../Standalone/AriaControlPanel.h"             // K-5: ARIA panel + Binding
#include "../Standalone/PagePresetIO.h"
#include "../MissingFileReport.h"                       // nest-aware drain for the load gestures
#include "../UserFileSave.h"                            // the one write-a-user-named-file path
#include "../SampleLibrary.h"                           // K-5: getCoreLibraryDir
#include "../PluginProcessor.h"
#include "../EngineRig.h"                               // QA-ModelShell TS1: model-side engine owner
#include "../Standalone/EngineChainProcessor.h"

namespace
{
    // QA-A Phase 4.4 (2026-05-09): kHeaderRowH = 36 retired.  The page
    // chrome strip it reserved is gone; BaySickPedalsEditor owns its own
    // title bar (Phase 4.2) and AriaControlPanel hosts its own title bar for
    // Guitars / Basses (Phase 4.3 + per-source-mode wiring below in
    // rebuildPlayerPanel).
    constexpr int kPad        = 12;
    constexpr int kFilenameW  = 320;
}

InstPage::InstPage (BaySickDAWProcessor& proc, int pageIndex)
    : mPageIndex (pageIndex),
      mTabName   ("LiveInst " + juce::String (pageIndex + 1))
{
    // QA-ModelShell TS1: known before the engine-trio bind below.
    // setProcessor stays for spawn-path parity.
    mFullProcessor = &proc;

    // G-7: wire dirty-listener pointers.
    mDirtyListener.dirtyFlag = &mPageDirty;
    mDirtyListener.suppress  = &mSuppressDirty;

    // sfizz program display -- hosted on the AriaControlPanel title bar by
    // rebuildPlayerPanel (QA-Layout T3/L23 removed its live-input strip
    // mount; nothing ever updated it there).
    mClipFileLabel.setJustificationType (juce::Justification::centredLeft);
    mClipFileLabel.setColour (juce::Label::textColourId,        juce::Colour (0xff66bbff));
    mClipFileLabel.setColour (juce::Label::backgroundColourId,  juce::Colour (0xff1a1a1a));
    mClipFileLabel.setColour (juce::Label::outlineColourId,     juce::Colour (0xff333333));
    mClipFileLabel.setBorderSize ({ 2, 6, 2, 6 });
    mClipFileLabel.setText ("(no audio loaded)", juce::dontSendNotification);
    mClipFileLabel.setTooltip (
        "Audio file or recording bound to this Inst tab.  BaySickNAM/IR re-amp "
        "and BaySickPedals consume this once routing lands.");

    // QA-ModelShell TS1: both stages + the chain wrapper are rig-owned
    // ({Inst, pageIndex}, engineType "Chain") -- constructed, prepared, and
    // registered by the model; the chain fans Pedals -> NAM/IR.  This page
    // binds non-owning views and builds the editors.  No engine picker --
    // the trio is permanent for the tab's lifetime.
    {
        auto& rig = proc.engineRig();
        rig.addTab (TabKind::Inst, pageIndex);
        mChain = dynamic_cast<EngineChainProcessor*> (
            rig.setEngineType (TabKind::Inst, pageIndex, "Chain"));
        if (auto* tab = rig.findTab (TabKind::Inst, pageIndex))
        {
            mPedalsProc = tab->pedals;
            mNamIrProc  = tab->namIr;
        }
    }

    // QA-Layout T4 (Window-7/L10): the Pedals + NAM/IR editors are OWNED here
    // but no longer page children -- their contained windows host them
    // non-owned through the accessors.  For a LiveInput tab the PEDALS window
    // IS the player (this page is never framed); sfizz tabs frame this page
    // showing the Aria player.
    if (mNamIrProc != nullptr)
    {
        mNamIrEditor.reset (mNamIrProc->createEditor());
        // QA-ApvtsAutomation: NAM/IR param ids are bare literals shared by every
        // Inst page, so automation keys need this page's index to stay distinct.
        if (auto* ne = dynamic_cast<BaySickNAMIREditor*> (mNamIrEditor.get()))
            ne->setAutomationPrefix ("inst" + juce::String (pageIndex) + "_");
    }

    if (mPedalsProc != nullptr)
    {
        mPedalsEditor.reset (mPedalsProc->createEditor());
        // QA-ApvtsAutomation: per-instance channel prefix; each pedal's lane key
        // is this plus the slot's stable uuid, so lanes survive pedal reordering.
        if (auto* pe = dynamic_cast<BaySickPedalsEditor*> (mPedalsEditor.get()))
            pe->setAutomationPrefix ("inst" + juce::String (pageIndex) + "_pedals");

        // QA-A 4.4 (2026-05-09): hook the pedalboard preset button back to
        // this page's existing showPedalboardPresetMenu() routing (the button
        // itself mounts on the pedals window's title strip -- T3/T4).
        if (auto* pe = dynamic_cast<BaySickPedalsEditor*> (mPedalsEditor.get()))
        {
            pe->onPedalboardPresetMenu = [this] { showPedalboardPresetMenu(); };
            // A per-pedal preset restores that pedal's NAM capture and user IR,
            // so the load can bank a missing file.  Nest-aware on purpose: the
            // same load reached under an outer gesture (a page preset, a project
            // restore) has to leave its entries for that scope, which a direct
            // report would steal and post under this noun.
            pe->onPresetLoaded = []
            {
                MissingFileReport::ScopedGesture gesture ("preset");
            };
        }
    }

    // K-5 (2026-05-05): sfizz player host.  The page's ONLY content now --
    // stays hidden for LiveInput tabs (which are never framed).
    mPlayerTab = std::make_unique<juce::Component>();
    mPlayerTab->setName ("Inst Player Tab");
    addChildComponent (*mPlayerTab);

    AriaControlPanel::Binding emptyBinding{};
    mAriaPanel = std::make_unique<AriaControlPanel> (emptyBinding);
    mPlayerTab->addAndMakeVisible (*mAriaPanel);

    // K-5 / L-4 (2026-05-05): program selector TextButton.  Label is set per
    // source mode in setSource - "Load Guitar" / "Load Bass" / "Load Inst".
    // The initial "Load Program" text only shows briefly before setSource
    // fires when a sfizz Inst tab spawns.
    mProgramButton = std::make_unique<juce::TextButton> ("Load Program");
    mProgramButton->setTooltip ("Pick a program - current selection shown on the file-name label");
    mProgramButton->onClick = [this] { showProgramPickerMenu(); };

    // Task 12 (G-14): cut-self pair, styled to match the synth editors
    // (QA-CutSelfReview).
    mCutSelfBtn.setClickingTogglesState (true);
    mCutSelfBtn.getProperties().set ("switchToggle", true);
    mCutSelfBtn.setTooltip ("Cut Self: when on, a new note cuts what is already sounding\n"
                            "(sfizz voices through their release; slide tails hard-stop).");
    mCutModeBtn.setClickingTogglesState (true);
    mCutModeBtn.setTooltip ("Cut Self mode: Same Pitch cuts only the retriggered note (default);\n"
                            "Cut All cuts every ringing voice on each new note.");
    mCutModeBtn.onStateChange = [this]
    { mCutModeBtn.setButtonText (mCutModeBtn.getToggleState() ? "CUT ALL" : "SAME PITCH"); };
    mCutModeBtn.onStateChange();

    // Jeff, 2026-08-04: the pair lives on the PLAYER's own top-left corner, not
    // on the window title strip -- they act on this engine's voices, so they
    // belong with the engine.  Added after mAriaPanel so they paint over it.
    mPlayerTab->addAndMakeVisible (mCutSelfBtn);
    mPlayerTab->addAndMakeVisible (mCutModeBtn);

    // QA-A 4.4 (2026-05-09): I-15 polish header chrome (mPedalsHeaderTitle +
    // mPedalsPresetBtn) deleted -- the title now lives inside BaySickPedalsEditor's
    // own BaySickTitleBar (Phase 4.2), and the pedalboard preset button migrated
    // into the editor's title-bar trailing area, wired above via onPedalboardPresetMenu.

    // J-6 EQ unification (2026-05-03): buildEQTab removed; pre-rack EQ on Effects page only.

    // Hook dirty tracker for the live engine.
    attachDirtyListener();
}

void InstPage::setTabName (const juce::String& n)
{
    mTabName = n;
    repaint();
}

InstPage::~InstPage()
{
    detachDirtyListener();

    // QA-ModelShell TS1: the chain + stages are rig-owned and survive this
    // view.  Teardown happens in EngineRig::removeTab / teardownAll, which
    // unregisters from audio dispatch BEFORE destroying and always destroys
    // the chain before its stages.  Only view-owned widgets die here --
    // editors first, since their child components hold attachments into the
    // engines' APVTS.
    mPedalsEditor.reset();
    mNamIrEditor .reset();
    // K-5: ARIA panel holds SliderParameterAttachments to the sfizz engine
    // APVTS; tear down BEFORE the engine destruction in PluginProcessor (the
    // editor/StandaloneEditor onTabClosed flow calls destroyBaySickGuitars
    // AFTER mPages.remove(i), which destroys this InstPage; ordering matches).
    mAriaPanel   .reset();
    mPlayerTab   .reset();
    mProgramButton.reset();
    mChain      = nullptr;
    mPedalsProc = nullptr;
    mNamIrProc  = nullptr;
}

// Forward declaration so the preset save/load members above can reach the
// unified config builder defined further down.  One config covers every Source
// mode (LiveInput / BaySickGuitars / BaySickBasses).
static PagePresetIO::PageChainConfig
makeInstPresetConfig (BaySickDAWProcessor& processor,
                       int                 pageIndex,
                       InstPage::Source    source,
                       juce::AudioProcessor* pedalsProc,
                       juce::AudioProcessor* namIrProc);

// Every Inst preset routes through PagePresetIO's per-kind directory
// ("Inst Page/My Presets") so saved files appear in the load submenu -- the
// previous `Inst/` literal didn't match where save wrote.
static juce::File instMyPresetsDir()
{
    return PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Inst);
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

            UserFileSave::writeTextAsync (instMyPresetsDir(), name,
                                          safeThis->exportInstState(), {});
        }), true);
}

void InstPage::loadInstPagePreset (const juce::File& xml)
{
    if (! xml.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Load Page Preset",
            "Preset could not be read:\n" + xml.getFullPathName(),
            "OK");
        return;
    }
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
    if (auto* nm = dynamic_cast<BaySickNAMIRProcessor*> (mNamIrProc))
        nm->apvts.state.addListener (&mDirtyListener);
}

void InstPage::detachDirtyListener()
{
    if (auto* nm = dynamic_cast<BaySickNAMIRProcessor*> (mNamIrProc))
        nm->apvts.state.removeListener (&mDirtyListener);
}

// 2026-05-05 consolidation: build a unified PagePresetIO config covering every
// Inst Source mode (LiveInput / BaySickGuitars / BaySickBasses).  Engine slots
// list every processor in the page's chain (always Pedals + NAM/IR + the
// active sfizz engine when present) -- rig-owned stages plus the processor-
// owned sfizz front end, all reached here through non-owning views -- so the
// saved preset round-trips the full chain regardless of which sub-tab was
// visible at save.  Single mixer strip +
// Inst-kind insert at this pageIndex; no bus rack since the Inst Bus is
// shared across every Inst tab.
static PagePresetIO::PageChainConfig
makeInstPresetConfig (BaySickDAWProcessor& processor,
                       int                 pageIndex,
                       InstPage::Source    source,
                       juce::AudioProcessor* pedalsProc,
                       juce::AudioProcessor* namIrProc)
{
    PagePresetIO::PageChainConfig cfg;

    cfg.sourceModeLabel = (source == InstPage::Source::LiveInput)      ? "LiveInput"
                        : (source == InstPage::Source::BaySickGuitars) ? "BaySickGuitars"
                        :                                                "BaySickBasses";

    // Pedals (live-input pedalboard) - present on every Inst tab, so this slot is unconditional.
    if (pedalsProc != nullptr)
    {
        PagePresetIO::EngineSlot slot;
        slot.engine        = pedalsProc;
        if (auto* p = dynamic_cast<BaySickPedalsProcessor*> (pedalsProc))
            slot.engineApvts = &p->apvts;
        slot.engineLabel   = "Pedals";
        // Blob root post-QA-Verify; the legacy BaySickPedalsState tag is the
        // APVTS child, not the state root.
        slot.engineRootTag = "BaySickPedalsRoot";
        // Pedals doesn't use a per-page-index APVTS prefix today, so no
        // prefix substitution on cross-page load.
        cfg.engineSlots.add (slot);
    }

    // NAM/IR (live-input amp + cabinet sim).
    if (namIrProc != nullptr)
    {
        PagePresetIO::EngineSlot slot;
        slot.engine        = namIrProc;
        if (auto* n = dynamic_cast<BaySickNAMIRProcessor*> (namIrProc))
            slot.engineApvts = &n->apvts;
        slot.engineLabel   = "NamIr";
        slot.engineRootTag = "BaySickNAMIRState";
        cfg.engineSlots.add (slot);
    }

    // Sfizz engine (BaySickGuitars / BaySickBasses).  Only present when the
    // current Source mode owns a sfizz processor.
    if (source == InstPage::Source::BaySickGuitars)
    {
        if (auto* eng = processor.getBaySickGuitars (pageIndex))
        {
            PagePresetIO::EngineSlot slot;
            slot.engine        = eng;
            slot.engineApvts   = &eng->apvts;
            slot.engineLabel   = "Sfizz";
            slot.engineRootTag = "BaySickGuitarsState";
            slot.enginePrefix  = "bgg_" + juce::String (pageIndex) + "_";
            slot.kitLoadCallback = [&processor, pageIndex] (const juce::File& kitPath)
            {
                return processor.loadBaySickGuitarsKit (pageIndex, kitPath);
            };
            cfg.engineSlots.add (slot);
        }
    }
    else if (source == InstPage::Source::BaySickBasses)
    {
        if (auto* eng = processor.getBaySickBasses (pageIndex))
        {
            PagePresetIO::EngineSlot slot;
            slot.engine        = eng;
            slot.engineApvts   = &eng->apvts;
            slot.engineLabel   = "Sfizz";
            slot.engineRootTag = "BaySickBassesState";
            slot.enginePrefix  = "bbb_" + juce::String (pageIndex) + "_";
            slot.kitLoadCallback = [&processor, pageIndex] (const juce::File& kitPath)
            {
                return processor.loadBaySickBassesKit (pageIndex, kitPath);
            };
            cfg.engineSlots.add (slot);
        }
    }

    cfg.stripPrefixes.add ("mixer_inst_" + juce::String (pageIndex));
    cfg.insertRackKindLabel = "Inst";
    cfg.insertRackIndices.add (pageIndex);   // only this page's strip
    // busRackIds left empty - Inst Bus is shared across every Inst tab.

    return cfg;
}

void InstPage::savePagePreset (std::function<void()> onSaved)
{
    if (mFullProcessor == nullptr)
    {
        saveInstPagePreset();
        if (onSaved) onSaved();
        return;
    }

    // One unified preset path captures Source mode + every owned engine slot
    // (Pedals + NAM/IR + sfizz Player when present) + mixer strip + insert rack
    // + both EQs, which is why showPageActionsMenu enables the entry for a
    // LiveInput tab carrying only pedals or a NAM/IR load.
    auto* aw = new juce::AlertWindow ("Save Page Preset",
                                       "Enter a name for this Inst page preset:",
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

            const auto cfg = makeInstPresetConfig (
                *safeThis->mFullProcessor,
                safeThis->mPageIndex,
                safeThis->mSource,
                safeThis->mPedalsProc,
                safeThis->mNamIrProc);

            const juce::String xml = PagePresetIO::exportPagePreset (
                *safeThis->mFullProcessor, PagePresetIO::PageKind::Inst, cfg);

            // The tail lives inside the callback because this save is also the
            // first half of "Save Page Preset & Delete": on anything but
            // Succeeded -- a failed write, or the user answering Cancel to the
            // collision prompt -- nothing below may run, least of all onSaved.
            // safeThis is re-tested there because the collision prompt is a
            // second modal, so the page can close while it is up.
            UserFileSave::writeTextAsync (instMyPresetsDir(), name, xml,
                [safeThis, onSaved] (const UserFileSave::Result& saved)
                {
                    if (! saved || ! safeThis) return;
                    safeThis->takeStateSnapshot();
                    if (onSaved) onSaved();
                },
                UserFileSave::kTabNotDeleted);
        }), false);
}

void InstPage::loadPagePreset (const juce::File& xml)
{
    // Gesture boundary: the preset restores the sfizz kit, the pedalboard's NAM
    // captures and the NAM/IR files, every one of which can bank a missing-file
    // entry.  Scoped rather than drained at the tail because the kit-verify
    // block below returns early, and nest-aware so the same load driven from an
    // outer gesture reports under that gesture's noun instead of stealing it.
    MissingFileReport::ScopedGesture gesture ("preset");

    if (! xml.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Load Page Preset",
            "Preset could not be read:\n" + xml.getFullPathName(),
            "OK");
        return;
    }
    if (mFullProcessor == nullptr) { loadInstPagePreset (xml); return; }

    const juce::String xmlText = xml.loadFileAsString();

    // 2026-05-05 consolidation: read the saved Source mode first so we can
    // switch to it BEFORE applying engine state - setSource() flips mSource
    // and rebuilds the chain pointers, but the sfizz processor itself is only
    // spawned by `loadBaySickGuitarsKit` on first kit load.  So if we're
    // loading a sfizz preset onto a LiveInput tab, we then have to peel the
    // saved kit path out of the saved engine blob and invoke the race-safe
    // loader to spawn the engine + load the kit, BEFORE building the config
    // (which captures engine pointers for the import).  Without this step,
    // makeInstPresetConfig sees a null engine pointer, silently drops the
    // Sfizz slot, and the user ends up on a sfizz tab with no kit loaded.
    const juce::String savedSourceMode = PagePresetIO::peekSourceMode (xml);
    if (savedSourceMode.isNotEmpty())
    {
        const Source target = (savedSourceMode == "BaySickGuitars") ? Source::BaySickGuitars
                            : (savedSourceMode == "BaySickBasses")  ? Source::BaySickBasses
                            :                                          Source::LiveInput;
        if (target != mSource)
            setSource (target);
    }

    // Spawn the sfizz engine + load the kit when needed.  Walks the saved
    // engine blob (v2: <Engines>/<Engine label="Sfizz">; legacy K-7: top-level
    // <Engine>), base64-decodes its data, looks for <KitPath path=...> inside
    // the engine state's root, and hands that path to the page's loader.
    // Falls through silently for LiveInput presets (no engine blob).  A
    // missing kit file or failed kit load alerts here and aborts the import:
    // PagePresetIO::importPagePreset can't cover this case because with no
    // engine spawned, makeInstPresetConfig drops the Sfizz slot entirely.
    if (mSource == Source::BaySickGuitars
        || mSource == Source::BaySickBasses)
    {
        if (auto parsed = SafeXml::parse (xmlText))
        {
            const auto rootTag = parsed->getTagName();
            const bool acceptedRoot = rootTag == "BaySickPagePreset"
                                       || rootTag == "GuitarsPagePreset"
                                       || rootTag == "BassesPagePreset";
            if (acceptedRoot)
            {
                // v2: look for <Engines>/<Engine label="Sfizz">.  Legacy K-7:
                // single top-level <Engine>.  Either way the engine blob lives
                // in `data="base64..."`.
                juce::XmlElement* engineEl = nullptr;
                if (auto* enginesEl = parsed->getChildByName ("Engines"))
                {
                    for (auto* e = enginesEl->getFirstChildElement(); e != nullptr;
                         e = e->getNextElement())
                    {
                        if (e->hasTagName ("Engine")
                            && e->getStringAttribute ("label") == "Sfizz")
                        {
                            engineEl = e; break;
                        }
                    }
                }
                if (engineEl == nullptr)
                    engineEl = parsed->getChildByName ("Engine");

                if (engineEl != nullptr)
                {
                    juce::MemoryBlock mb;
                    if (mb.fromBase64Encoding (engineEl->getStringAttribute ("data"))
                        && mb.getSize() > 0)
                    {
                        const juce::String engineRootTag =
                            engineEl->hasAttribute ("rootTag")
                                ? engineEl->getStringAttribute ("rootTag")
                                : (mSource == Source::BaySickGuitars
                                      ? juce::String ("BaySickGuitarsState")
                                      : juce::String ("BaySickBassesState"));

                        if (auto stateXml = SafeXml::parseBinaryBlob (
                                mb.getData(), (int) mb.getSize()))
                        {
                            if (engineRootTag.isEmpty()
                                || stateXml->hasTagName (engineRootTag))
                            {
                                if (auto* kitEl = stateXml->getChildByName ("KitPath"))
                                {
                                    // The engine writes this as a stable-root
                                    // ref when the kit is under Core Library;
                                    // resolvePersistedRef passes plain absolute
                                    // paths through, so older blobs still load.
                                    const juce::File kitPath =
                                        SampleLibrary::resolvePersistedRef (
                                            kitEl->getStringAttribute ("path"));
                                    if (! kitPath.existsAsFile())
                                    {
                                        juce::AlertWindow::showMessageBoxAsync (
                                            juce::MessageBoxIconType::WarningIcon,
                                            "Load Page Preset",
                                            "The saved kit path is missing from this machine:\n"
                                            + kitPath.getFullPathName()
                                            + "\n\nInstall the kit, or pick a different preset.",
                                            "OK");
                                        return;
                                    }
                                    const bool kitOk = (mSource == Source::BaySickGuitars)
                                        ? mFullProcessor->loadBaySickGuitarsKit (mPageIndex, kitPath)
                                        : mFullProcessor->loadBaySickBassesKit  (mPageIndex, kitPath);
                                    if (! kitOk)
                                    {
                                        juce::AlertWindow::showMessageBoxAsync (
                                            juce::MessageBoxIconType::WarningIcon,
                                            "Load Page Preset",
                                            "The kit could not be loaded:\n"
                                            + kitPath.getFullPathName()
                                            + "\n\nThe file may be damaged.",
                                            "OK");
                                        return;
                                    }
                                    // Also covers makeInstPresetConfig's
                                    // slot.kitLoadCallback below -- it reloads
                                    // this same, already-verified kit path.
                                    setKitMissing (false);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    const auto cfg = makeInstPresetConfig (*mFullProcessor, mPageIndex, mSource,
                                            mPedalsProc, mNamIrProc);

    mSuppressDirty = true;
    PagePresetIO::importPagePreset (*mFullProcessor, PagePresetIO::PageKind::Inst,
                                     cfg, xmlText);
    mSuppressDirty = false;

    // 2026-05-05 fix: rebuild the engine chain AFTER the engine state has been
    // restored.  setSource (called above) ran rebuildEngineChain when the
    // sfizz processor didn't exist yet, so the chain wrapper was built with
    // only [Pedals, NAM/IR] - no sfizz front stage.  By the time
    // importPagePreset finishes, the engine + kit + APVTS are all live; this
    // call now sees the engine pointer and splices it into the chain so
    // audio actually flows from the player.  rebuildPlayerPanel re-runs the
    // ARIA panel binding against the just-restored engine state.
    notifySourceEngineChanged();
    takeStateSnapshot();
}

// Lock / Rename / Duplicate belong HERE, not in a separate engine context menu.
// Inst's old one was caller-less, which left Lock unsettable -- so the
// page-locked delete guard could never engage for an Inst tab -- and left
// Duplicate Inst with no route in the shipping UI at all.  Every sibling page
// carries these in this same menu; one Delete only.  Rename additionally has an
// independent route through the ribbon tab's right-click.
void InstPage::showPageActionsMenu (juce::Component* anchor)
{
    constexpr int kIdLock           = 1;
    constexpr int kIdRename         = 5;
    constexpr int kIdDuplicate      = 12;
    constexpr int kIdSavePagePreset = 100;
    constexpr int kIdDeleteTab      = 101;
    constexpr int kIdLoadBase       = 1000;

    juce::PopupMenu menu;
    if (onBuildWindowNavMenu) { onBuildWindowNavMenu (menu); menu.addSeparator(); }
    menu.addItem (kIdLock, "Lock", true, mLocked);

    menu.addSeparator();
    menu.addItem (kIdRename,    "Rename...");
    menu.addItem (kIdDuplicate, "Duplicate Inst (new tab)");

    menu.addSeparator();
    // 2026-05-05 consolidation: Save enabled when the page has anything in its
    // chain - sfizz Player picked, NAM/IR loaded, or any pedal slot wired up.
    // Load is always enabled (even on a blank tab) so the user can spawn a
    // saved chain on a fresh tab.
    const bool canSavePreset = (getEngineProcessor() != nullptr)
                                || (mNamIrProc  != nullptr)
                                || (mPedalsProc != nullptr);
    menu.addItem (kIdSavePagePreset, "Save Page Preset As...", canSavePreset);

    juce::Array<juce::File> presetXmls;
    {
        juce::PopupMenu loadSub;
        // 2026-05-05 consolidation: every Inst preset lives in the unified
        // Inst Page folder regardless of which source mode was active at save.
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

    // Delete on the hamburger too (Jeff, 2026-08-02) -- see LayersPage.
    menu.addSeparator();
    menu.addItem (kIdDeleteTab, "Delete Inst", ! mLocked);

    juce::Component::SafePointer<InstPage> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (anchor),
        [safeThis, presetXmls = std::move (presetXmls), kIdLoadBase] (int r)
        {
            if (! safeThis || r <= 0) return;
            if (r == kIdLock)           { safeThis->toggleLockUndoable(); return; }
            if (r == kIdRename    && safeThis->onRenameRequested)    { safeThis->onRenameRequested();    return; }
            if (r == kIdDuplicate && safeThis->onDuplicateRequested) { safeThis->onDuplicateRequested(); return; }
            if (r == kIdSavePagePreset) { safeThis->savePagePreset(); return; }
            if (r == kIdDeleteTab)      { safeThis->requestDelete();  return; }
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

    // QA-E Task 5 (2026-05-15): tab close now cascades library cleanup --
    // every recording made on this Inst tab gets removed from the audio
    // library alongside the page.  Physical WAVs in the project's Samples
    // folder stay on disk so the user can drag them back in later.
    const juce::String warning =
        "Deleting this inst tab removes its BaySickPedals, BaySickNAM/IR, "
        "Mixer Strip, Effects Rack, and the audio library entries for "
        "every recording made on this tab.\n"
        "The audio files in your project's Samples folder stay on disk.";

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
    // K-2 (2026-05-05): chain may also have a sfizz front-end stage when
    // source = BaySickGuitars / BaySickBasses (rebuilt via setSource).
    return mChain;
}

// K-2 (2026-05-05): rebuild the EngineChainProcessor stage list based on the
// current source mode.  LiveInput = {Pedals, NAMIR}.  BaySickGuitars (or
// BaySickBasses, L-2) = {Guitars, Pedals, NAMIR} - sfizz front-end fills the
// buffer with engine output, then Pedals + NAMIR process from there.
//
// The Guitars / Basses engine pointer is queried from PluginProcessor every
// time this runs.  If the engine doesn't exist yet (kit not loaded), the chain
// degrades to {Pedals, NAMIR} and the buffer arrives empty - silent until the
// kit loads, at which point a follow-up call rebuilds with the engine spliced
// in.  K-4 calls setSource → loadKit → setSource (or just setSource after the
// load) to ensure the chain sees the engine.
void InstPage::rebuildEngineChain()
{
    if (! mChain) return;

    juce::AudioProcessor* sfizzFront = nullptr;
    if (mFullProcessor != nullptr)
    {
        if (mSource == Source::BaySickGuitars)
            sfizzFront = mFullProcessor->getBaySickGuitars (mPageIndex);
        else if (mSource == Source::BaySickBasses)
            sfizzFront = mFullProcessor->getBaySickBasses (mPageIndex);
    }

    if (sfizzFront != nullptr)
        mChain->setChain ({ sfizzFront, mPedalsProc, mNamIrProc });
    else
        mChain->setChain ({ mPedalsProc, mNamIrProc });
}

void InstPage::setSource (Source s)
{
    if (s == mSource) return;
    mSource = s;
    // A source change replaces the tab's whole engine identity, so any
    // substituted-kit marker from the previous source no longer describes it.
    // Both restore paths call setSource BEFORE setting the marker.
    setKitMissing (false);
    rebuildEngineChain();
    rebuildPlayerPanel();
    // L-3 (2026-05-05): keep the program-picker button label in sync with
    // the active source mode.  "Load Guitar" / "Load Bass" / "Load Inst"
    // makes the affordance unambiguous in the page-menu-bar extras row.
    if (mProgramButton)
    {
        const juce::String label =
            (s == Source::BaySickGuitars) ? "Load Guitar"
          : (s == Source::BaySickBasses)  ? "Load Bass"
          :                                  "Load Inst";
        const juce::String tip =
            (s == Source::BaySickGuitars) ? "Pick a guitar program - current selection shown on the file-name label"
          : (s == Source::BaySickBasses)  ? "Pick a bass program - current selection shown on the file-name label"
          :                                  "Pick a program";
        mProgramButton->setButtonText (label);
        mProgramButton->setTooltip (tip);
    }
    // QA-Layout T4: the sfizz player is the page's only content -- visible
    // for sfizz sources, hidden for LiveInput (whose tab is never framed;
    // the pedals window is its player per L10).
    if (mPlayerTab)
    {
        mPlayerTab->setVisible (mSource != Source::LiveInput);
        resized();
    }
}

void InstPage::notifySourceEngineChanged()
{
    rebuildEngineChain();
    rebuildPlayerPanel();
}

// K-6 (2026-05-05): serialize the per-program APVTS cache for project save.
// Returns a new <ProgramStateCache> XmlElement with one <Program> child per
// cached entry.  Caller owns the returned pointer.  Empty cache → empty
// element (still returned so deserialize can detect the cache existed).
juce::XmlElement* InstPage::serializeProgramCacheXml() const
{
    auto* root = new juce::XmlElement ("ProgramStateCache");
    for (const auto& [filename, tree] : mProgramStateCache)
    {
        auto* prog = root->createNewChildElement ("Program");
        prog->setAttribute ("filename", filename);
        if (auto treeXml = tree.createXml())
            prog->addChildElement (treeXml.release());
    }
    return root;
}

void InstPage::restoreProgramCacheFromXml (const juce::XmlElement& cacheXml)
{
    mProgramStateCache.clear();
    for (auto* prog : cacheXml.getChildWithTagNameIterator ("Program"))
    {
        if (prog == nullptr) continue;
        const auto filename = prog->getStringAttribute ("filename");
        if (filename.isEmpty()) continue;
        if (auto* treeXml = prog->getFirstChildElement())
        {
            auto tree = juce::ValueTree::fromXml (*treeXml);
            if (tree.isValid())
                mProgramStateCache[filename] = tree;
        }
    }
}

// K-5 (2026-05-05): build a fresh AriaControlPanel::Binding for the current
// source-mode engine and load the kit's Player GUI XML.  When the engine
// pointer is null (kit not loaded yet, or LiveInput source), the panel falls
// through to its placeholder ("Loading control surface...").
void InstPage::rebuildPlayerPanel()
{
    if (! mAriaPanel) return;

    // G-14: unbind the cut-self attachments FIRST -- the engine this rebuild
    // re-binds to may not be the one they currently listen on.
    mCutSelfAttach.reset();
    mCutModeAttach.reset();

    AriaControlPanel::Binding binding{};

    juce::File kitRoot;
    juce::File programXml;

    if (mFullProcessor != nullptr)
    {
        // L-4 (2026-05-05): Guitars + Basses share the binding-build path; the
        // engine type's APVTS prefix differs (`bgg_<idx>_cc` vs `bbb_<idx>_cc`)
        // but the closures reach into the engine's getApvtsPrefix() / getCc*
        // accessors which already know their own prefix.
        auto bindToSfizzEngine = [&binding, &kitRoot, &programXml] (auto* engine)
        {
            if (engine == nullptr) return;
            binding.apvts        = &engine->apvts;
            binding.ccParamId    = [pfx = engine->getApvtsPrefix() + "cc"](int cc)
                                      { return pfx + juce::String (cc); };
            binding.kitDefaultCc = [engine](int cc) { return engine->getKitDefaultCc (cc); };
            binding.ccLabel      = [engine](int cc) { return engine->getCcLabel (cc); };

            kitRoot = engine->getCurrentKitPath().getParentDirectory()
                                                .getParentDirectory();
            const auto stem = engine->getCurrentKitPath().getFileNameWithoutExtension();
            if (stem.isNotEmpty())
                programXml = kitRoot.getChildFile ("GUI").getChildFile (stem + ".xml");
        };

        if (mSource == Source::BaySickGuitars)
            bindToSfizzEngine (mFullProcessor->getBaySickGuitars (mPageIndex));
        else if (mSource == Source::BaySickBasses)
            bindToSfizzEngine (mFullProcessor->getBaySickBasses  (mPageIndex));
        // QA-Layout T15: binding.engineName stays empty -- the internal
        // AriaControlPanel title bar is dissolved; the engine name renders
        // centered on the hosting window's title strip and the widgets the
        // bar hosted (program label/button + CUT SELF pair) mount there too,
        // wired by StandaloneEditor per page-show.
    }

    mAriaPanel->setEngine (binding);

    // G-14: bind the cut-self pair to whichever sfizz engine this page
    // drives.  No engine (mid-teardown) -> attachments stay cleared.
    if (mSource != Source::LiveInput && mFullProcessor != nullptr)
    {
        juce::AudioProcessorValueTreeState* engApvts = nullptr;
        juce::String engPrefix;
        if (mSource == Source::BaySickGuitars)
        {
            if (auto* e = mFullProcessor->getBaySickGuitars (mPageIndex))
            {
                engApvts  = &e->apvts;
                engPrefix = e->getApvtsPrefix();
            }
        }
        else if (mSource == Source::BaySickBasses)
        {
            if (auto* e = mFullProcessor->getBaySickBasses (mPageIndex))
            {
                engApvts  = &e->apvts;
                engPrefix = e->getApvtsPrefix();
            }
        }
        if (engApvts != nullptr)
        {
            mCutSelfAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                *engApvts, engPrefix + "cutSelf", mCutSelfBtn);
            mCutModeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                *engApvts, engPrefix + "cutSelfMode", mCutModeBtn);
        }
    }

    if (programXml.existsAsFile())
        mAriaPanel->loadFromKit (kitRoot, programXml);
    else
        mAriaPanel->clear();

    // K-5 fix #UI (2026-05-05): refresh the loaded-program label.  For sfizz
    // sources, the file label that's parked in PageMenuBar shows the pretty
    // program name (e.g. "Green Keyswitch") instead of the live-input clip
    // path.  Empty kit → "(no program loaded)".
    if (mSource != Source::LiveInput)
    {
        juce::File currentKit;
        if (mFullProcessor != nullptr)
        {
            if (mSource == Source::BaySickGuitars)
                if (auto* engine = mFullProcessor->getBaySickGuitars (mPageIndex))
                    currentKit = engine->getCurrentKitPath();
            if (mSource == Source::BaySickBasses)
                if (auto* engine = mFullProcessor->getBaySickBasses (mPageIndex))
                    currentKit = engine->getCurrentKitPath();
        }

        if (currentKit == juce::File())
        {
            mClipFileLabel.setText ("(no program loaded)", juce::dontSendNotification);
            mLastProgramFile.clear();
        }
        else
        {
            // Strip "NN-" numeric prefix + title-case, mirroring the popup labels.
            auto stem = currentKit.getFileNameWithoutExtension();
            if (stem.length() > 3 && stem[2] == '-'
                && juce::CharacterFunctions::isDigit (stem[0])
                && juce::CharacterFunctions::isDigit (stem[1]))
                stem = stem.substring (3);
            stem = stem.replaceCharacter ('_', ' ');
            juce::String pretty;
            bool atWordStart = true;
            for (auto ch : stem)
            {
                if (ch == ' ') { pretty += ch; atWordStart = true; continue; }
                pretty += atWordStart
                            ? juce::String::charToString (juce::CharacterFunctions::toUpperCase (ch))
                            : juce::String::charToString (ch);
                atWordStart = false;
            }
            mClipFileLabel.setText (pretty, juce::dontSendNotification);
            mLastProgramFile = currentKit.getFileName();
        }
    }
}

// K-5 / L-4 (2026-05-05): program-picker button click - pop a menu listing
// every *.sfz in the kit's Programs folder.  Selecting an item swaps programs
// preserving each one's CC tweaks for the session (mProgramStateCache).
// First visit to a program shows kit-author defaults.  No confirm dialog -
// swap is non-destructive.  Source-aware: queries either the Guitars or
// Basses engine via mFullProcessor based on mSource.
// Program display name: strips a leading "NN-" track number, underscores to
// spaces, Title Case.  Shared by the picker menu, the sound-name linkage,
// and the add-tab autoload path in StandaloneEditor.
juce::String InstPage::prettyProgramName (const juce::File& f)
{
    auto stem = f.getFileNameWithoutExtension();
    if (stem.length() > 3 && stem[2] == '-'
        && juce::CharacterFunctions::isDigit (stem[0])
        && juce::CharacterFunctions::isDigit (stem[1]))
        stem = stem.substring (3);
    stem = stem.replaceCharacter ('_', ' ');
    juce::String pretty;
    bool atWordStart = true;
    for (auto ch : stem)
    {
        if (ch == ' ') { pretty += ch; atWordStart = true; continue; }
        pretty += atWordStart
                    ? juce::String::charToString (juce::CharacterFunctions::toUpperCase (ch))
                    : juce::String::charToString (ch);
        atWordStart = false;
    }
    return pretty;
}

void InstPage::showProgramPickerMenu()
{
    if (mFullProcessor == nullptr || mSource == Source::LiveInput) return;

    // Closure unifies Guitars / Basses engine fetch into one APVTS-bearing
    // accessor that returns the current kit path (sfizz is the only audio
    // processor type that owns kit-path state - neither Pedals nor NAM/IR).
    auto getCurrentKit = [&] () -> juce::File
    {
        if (mSource == Source::BaySickGuitars)
            if (auto* eng = mFullProcessor->getBaySickGuitars (mPageIndex))
                return eng->getCurrentKitPath();
        if (mSource == Source::BaySickBasses)
            if (auto* eng = mFullProcessor->getBaySickBasses (mPageIndex))
                return eng->getCurrentKitPath();
        return {};
    };

    const auto currentKit = getCurrentKit();
    if (currentKit == juce::File()) return;

    const auto programsDir = currentKit.getParentDirectory();
    juce::Array<juce::File> sfzFiles;
    programsDir.findChildFiles (sfzFiles, juce::File::findFiles, false, "*.sfz");
    sfzFiles.sort();

    juce::PopupMenu m;
    for (int i = 0; i < sfzFiles.size(); ++i)
    {
        const bool isCurrent = (sfzFiles[i] == currentKit);
        m.addItem (i + 1, prettyProgramName (sfzFiles[i]), /*enabled=*/true, /*ticked=*/isCurrent);
    }

    juce::Component::SafePointer<InstPage> safe (this);
    auto picksCopy = sfzFiles;
    m.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (mProgramButton.get()),
        [safe, picksCopy] (int result)
        {
            if (! safe || result <= 0 || result > picksCopy.size()) return;
            auto* p = safe.getComponent();
            if (p == nullptr || p->mFullProcessor == nullptr) return;
            p->switchSfizzProgramWithUndo (picksCopy[result - 1]);
        });
}

// QA-UndoCoverage Task 7: the program switch, callable from the menu pick AND
// from undo/redo.  The session program-state cache carries each program's CC
// tweaks, so no snapshot files are needed -- switching back restores the
// prior program's state through the same cache the pick always used.
bool InstPage::switchSfizzProgram (const juce::File& target)
{
    if (mFullProcessor == nullptr) return false;

    auto sfizzEngine = [&] () -> juce::AudioProcessor*
    {
        if (mSource == Source::BaySickGuitars)
            return mFullProcessor->getBaySickGuitars (mPageIndex);
        if (mSource == Source::BaySickBasses)
            return mFullProcessor->getBaySickBasses  (mPageIndex);
        return nullptr;
    };
    auto sfizzApvts = [&] () -> juce::AudioProcessorValueTreeState*
    {
        if (mSource == Source::BaySickGuitars)
            if (auto* eng = mFullProcessor->getBaySickGuitars (mPageIndex))
                return &eng->apvts;
        if (mSource == Source::BaySickBasses)
            if (auto* eng = mFullProcessor->getBaySickBasses (mPageIndex))
                return &eng->apvts;
        return nullptr;
    };
    auto sfizzKitPath = [&] () -> juce::File
    {
        if (mSource == Source::BaySickGuitars)
            if (auto* eng = mFullProcessor->getBaySickGuitars (mPageIndex))
                return eng->getCurrentKitPath();
        if (mSource == Source::BaySickBasses)
            if (auto* eng = mFullProcessor->getBaySickBasses (mPageIndex))
                return eng->getCurrentKitPath();
        return {};
    };

    auto* eng = sfizzEngine();
    auto* apv = sfizzApvts();
    if (eng == nullptr || apv == nullptr) return false;
    const auto current = sfizzKitPath();
    if (target == current) return false;

    // 1) Save outgoing program's APVTS state.
    const auto outgoingKey = current.getFileName();
    if (outgoingKey.isNotEmpty())
        mProgramStateCache[outgoingKey] = apv->copyState();

    // 2) Race-safe kit load (source-aware wrapper).
    const bool ok = (mSource == Source::BaySickGuitars)
                      ? mFullProcessor->loadBaySickGuitarsKit (mPageIndex, target)
                      : mFullProcessor->loadBaySickBassesKit  (mPageIndex, target);
    if (! ok)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Load Program",
            "Could not load program:\n" + target.getFullPathName(),
            "OK");
        return false;
    }

    // The tab now plays what it says it plays.  Cleared here rather than off
    // loadBaySickGuitarsKit/loadBaySickBassesKit returning true: the restore
    // substitution IS a successful load of an existing file, so a load-success
    // hook would wipe the marker two statements after it was set.
    setKitMissing (false);

    // 3) Restore incoming program's saved state if cached.  Walk the
    //    cached PARAM tree directly and call setValueNotifyingHost on
    //    each one - replaceState alone doesn't reliably fire the
    //    parameterChanged listener for every changed param across all
    //    JUCE versions, which would leave sfizz out of sync with the
    //    restored APVTS state for any CC the kit just reset to 0.
    const auto incomingKey = target.getFileName();
    if (auto it = mProgramStateCache.find (incomingKey);
        it != mProgramStateCache.end())
    {
        auto* apv2 = sfizzApvts();
        auto* eng2 = sfizzEngine();
        if (apv2 != nullptr && eng2 != nullptr)
        {
            // QA-UndoCoverage Task 6: restore force-fires are programmatic.
            juce::AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spw;
            apv2->replaceStateKeepingUndoHistory (it->second);
            for (auto* param : eng2->getParameters())
            {
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                    ranged->setValueNotifyingHost (ranged->getValue());
            }
        }
    }

    // 4) Refresh chain + ARIA panel + file label.
    notifySourceEngineChanged();

    // 5) Program-name linkage: tab + strip + roll follow the loaded
    //    program (interactive picks only -- restore never runs this).
    if (onSoundNameChanged)
        onSoundNameChanged (prettyProgramName (target));
    return true;
}

void InstPage::switchSfizzProgramWithUndo (const juce::File& target)
{
    // Capture the prior program BEFORE the switch (the switch caches its CC
    // state under this key, which is exactly what undo re-enters through).
    juce::File prior;
    if (mSource == Source::BaySickGuitars)
    {
        if (auto* eng = mFullProcessor ? mFullProcessor->getBaySickGuitars (mPageIndex) : nullptr)
            prior = eng->getCurrentKitPath();
    }
    else if (mSource == Source::BaySickBasses)
    {
        if (auto* eng = mFullProcessor ? mFullProcessor->getBaySickBasses (mPageIndex) : nullptr)
            prior = eng->getCurrentKitPath();
    }

    if (! switchSfizzProgram (target)) return;

    if (! mUndoCtx.isValid() || prior == juce::File()) return;

    // Jeff ruling 2026-08-06: resolve the LIVE page at apply time (a
    // delete+resurrect cycle replaces this object; SafePointer = fallback).
    auto resolveSelf = mUndoCtx.resolveOwnerPage;
    auto sp = juce::Component::SafePointer<InstPage> (this);
    auto livePage = [resolveSelf, sp]() -> InstPage*
    {
        if (resolveSelf) return dynamic_cast<InstPage*> (resolveSelf());
        return sp.getComponent();
    };
    mUndoCtx.perform (new StructuralOpAction (
                          [livePage, prior]  { if (auto* ip = livePage()) ip->switchSfizzProgram (prior); },
                          [livePage, target] { if (auto* ip = livePage()) ip->switchSfizzProgram (target); }),
                      "Program " + prettyProgramName (target));
}

void InstPage::toggleLockUndoable()
{
    const bool before = mLocked;
    const bool after  = ! before;
    setLocked (after);

    if (! mUndoCtx.isValid()) return;

    // Jeff ruling 2026-08-06: resolve the LIVE page at apply time (a
    // delete+resurrect cycle replaces this object; SafePointer = fallback).
    auto resolveSelf = mUndoCtx.resolveOwnerPage;
    auto sp = juce::Component::SafePointer<InstPage> (this);
    auto livePage = [resolveSelf, sp]() -> InstPage*
    {
        if (resolveSelf) return dynamic_cast<InstPage*> (resolveSelf());
        return sp.getComponent();
    };
    mUndoCtx.perform (new StructuralOpAction (
                          [livePage, before] { if (auto* ip = livePage()) ip->setLocked (before); },
                          [livePage, after]  { if (auto* ip = livePage()) ip->setLocked (after);  }),
                      after ? "Lock Inst" : "Unlock Inst");
}

void InstPage::setProcessor (BaySickDAWProcessor* p)
{
    mFullProcessor = p;
    // J-6 EQ unification (2026-05-03): page-level EQ binding removed; pre-rack
    // EQ is bound exclusively by EffectsPage (mixer_inst_<N>_preeq_*).
}


// QA-E Task 4 (2026-05-12): setClipFilePath deleted.  Inst file-association
// lives in PatternManager AudioLibrary via pageOwnerChannelId tagging.
// mClipFileLabel is RETAINED -- still driven by setSource() / updateSfizz
// paths for kit name display.

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
    auto parsed = SafeXml::parse (xml);
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

    // QA-E Task 4 (2026-05-12): removed dead `if (mClipPath.isNotEmpty())
    // setClipFilePath(mClipPath)` -- mClipPath was never serialized so the
    // field was always empty at this point; the call was dead.
}

void InstPage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181818));
    // QA-A 4.4 (2026-05-09): page header chrome (kHeaderRowH = 36 dark fill +
    // 1 px divider) deleted.  Each engine UI now owns its own title bar at
    // y = 0 of its content area, so the page only paints the body fill.
}

// ─────────────────────────────────────────────────────────────────────────────
// I-15 polish: pedalboard preset library popup -- save/load the entire
// 8-slot rack as Documents/BaySickDAW/Presets/Pedalboards/{name}.xml.
// Distinct from the Page Preset (hamburger menu, full chain) and per-pedal
// presets (the "..." button on each tile).
// ─────────────────────────────────────────────────────────────────────────────
void InstPage::showPedalboardPresetMenu()
{
    auto* pedals = dynamic_cast<BaySickPedalsProcessor*> (mPedalsProc);
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

    // QA-A Phase 6 (2026-05-09): anchor the menu to the title-bar preset
    // button instead of the whole editor.  Targeting `mPedalsEditor.get()`
    // pinned the menu at the editor's top-left = the app's top-left;
    // targeting the button drops it from immediately below the click site.
    // mPedalsEditor is typed as juce::AudioProcessorEditor* on this side, so
    // the BaySickPedalsEditor-specific accessor needs a dynamic_cast --
    // matches the existing pattern at InstPage.cpp:102.
    auto* pedalsEditor = dynamic_cast<BaySickPedalsEditor*> (mPedalsEditor.get());
    auto* presetBtn = pedalsEditor != nullptr
                        ? pedalsEditor->getPedalboardPresetButton()
                        : nullptr;
    auto opts = juce::PopupMenu::Options();
    if (presetBtn != nullptr)
        opts = opts.withTargetComponent (presetBtn);
    else
        opts = opts.withTargetComponent (mPedalsEditor.get());
    m.showMenuAsync (opts,
        [pedals, presets] (int r)
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
                                // No empty-name pre-check: savePedalboardPreset
                                // reports that case itself, and guarding here
                                // swallowed the message so pressing Save on the
                                // empty field did nothing at all.
                                const auto name = aw->getTextEditorContents ("name").trim();
                                juce::String err;
                                if (! pedals->savePedalboardPreset (name, err))
                                    juce::AlertWindow::showMessageBoxAsync (
                                        juce::MessageBoxIconType::WarningIcon,
                                        "Save Pedalboard Preset", err, "OK");
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
                    // A whole-board load restores all eight pedal DSPs, NAM
                    // captures and user IRs included, so it reaches
                    // MissingFileReport::add through them -- and a load that
                    // reports an error can still have banked entries on its way
                    // there, which a success-only drain left behind.
                    MissingFileReport::ScopedGesture gesture ("preset");

                    juce::String err;
                    if (! pedals->loadPedalboardPreset (presets[idx], err))
                        juce::AlertWindow::showMessageBoxAsync (
                            juce::MessageBoxIconType::WarningIcon,
                            "Load Pedalboard Preset", err, "OK");
                }
            }
        });
}

void InstPage::resized()
{
    // QA-A 4.4 (2026-05-09): no header chrome to lay out anymore -- the
    // engine UIs fill the full page area.  Each engine owns its own title
    // bar at y = 0 of its content rect.
    layoutContent (getLocalBounds());
}

void InstPage::layoutContent (juce::Rectangle<int> r)
{
    // QA-Layout T4: Pedals + NAM/IR live in their own contained windows --
    // the sfizz player is the page's only content.
    if (mPlayerTab && mPlayerTab->isVisible())
    {
        mPlayerTab->setBounds (r);
        if (mAriaPanel) mAriaPanel->setBounds (mPlayerTab->getLocalBounds());
        // Overlaid on the player's top-left corner (Jeff, 2026-08-04).
        auto cut = mPlayerTab->getLocalBounds().removeFromTop (22).reduced (6, 2);
        mCutSelfBtn.setBounds (cut.removeFromLeft (62));
        cut.removeFromLeft (4);
        mCutModeBtn.setBounds (cut.removeFromLeft (78));
    }
}

