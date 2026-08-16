#include "LayersPage.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)
#include "UndoBracket.h"
#include "UndoSnapshotStore.h"   // QA-UndoCoverage: chain-swap snapshots
#include "../AppPaths.h"
#include "../BaySickSynth/BaySickSynthProcessor.h"
#include "../BaySickSynth/BaySickSynthEditor.h"
#include "../Harmless/HarmlessProcessor.h"
#include "../Harmless/HarmlessEditor.h"
#include "../BaySickPlayer/BaySickPlayerProcessor.h"
#include "../BaySickPlayer/BaySickPlayerEditor.h"
#include "../SampleLibrary.h"
#include "../MissingFileReport.h"
#include "../UserFileSave.h"
#include "PagePresetIO.h"
#include "StandaloneEditor.h"
using namespace juce;

// ── Constructor / Destructor ──────────────────────────────────────────────────
LayersPage::LayersPage(BaySickDAWProcessor& p, PatternManager& pm, int pageIndex)
    : mProcessor(p), mPM(pm), mPageIndex(juce::jlimit(0, kMaxLayerPages - 1, pageIndex))
{
    mPageColor = VC::LayerCol[mPageIndex % juce::numElementsInArray (VC::LayerCol)];

    // QA-ModelShell TS1: the tab is a model object from birth -- idempotent,
    // so restore/duplicate paths that pre-created it are fine.  The engine
    // attaches at selectEngine.  The model tab carries no display name --
    // (kind, pageIndex) is its identity and the name belongs to the ribbon.
    mProcessor.engineRig().addTab (TabKind::Layers, mPageIndex);

    buildPlayerTab();
    // 2026-04-26 (step 2 commit 3): Piano Roll lives on the unified
    // PianoRollPage.  QA-Layout T4 (L11) then moved the navigation into the
    // page dropdown's "Pages:" list, so nothing here renders a sub-tab control.
    // Sub-tab index 1 is that redirect and owns no component, which is why
    // switchTab still clamps to 0..1.
    // J-6 EQ unification (2026-05-03): EQ sub-tab removed - pre + post EQ
    // for this insert now live exclusively on the Effects page (uniform with
    // every other strip type).  The pre-EQ DSP + APVTS params still live on
    // the InsertNode unchanged.

    switchTab(0);
}

LayersPage::~LayersPage()
{
    unsubscribeFromEngineApvtsState();

    // QA-ModelShell TS1: the engine is rig-owned and survives this view.
    // Teardown happens in EngineRig::removeTab (tab close) or teardownAll
    // (shutdown).  Only the view-owned editor dies here, before the page --
    // its attachments reference the engine's APVTS.
    if (mEngineEditor && mPlayerTab)
        mPlayerTab->removeChildComponent(mEngineEditor.get());
    mEngineEditor.reset();
    mEngineProcessor = nullptr;
}

// ── Tab switching ─────────────────────────────────────────────────────────────
void LayersPage::switchTab(int idx)
{
    // J-6 EQ unification: 2 sub-tabs only (Player + Piano Roll).
    mActiveTab = juce::jlimit(0, 1, idx);
    if (mPlayerTab) mPlayerTab->setVisible(mActiveTab == 0);
    resized();
}

// ── Tab builders ──────────────────────────────────────────────────────────────
void LayersPage::buildPlayerTab()
{
    // QA-Layout T2 (L4): the engine picker row is gone -- the engine is
    // chosen at the "+" menu before the page exists, and the old context
    // menu lives on the title strip's Menu dropdown (showPageActionsMenu).
    mPlayerTab = std::make_unique<Component>();
    addAndMakeVisible(*mPlayerTab);
}

void LayersPage::setUndoContext(const UndoContext& ctx)
{
    mUndoCtx = ctx;   // QA-UndoCoverage: chain-swap gestures (ruling 3a)
}

// ── Engine selection ──────────────────────────────────────────────────────────
void LayersPage::selectEngine(const juce::String& engineName)
{
    if (mEngineLocked) return;
    selectEngineInternal (engineName);
}

void LayersPage::selectEngineInternal(const juce::String& engineName)
{
    HeavyOperationOverlay::ScopedOp busy (StandaloneEditor::busyOverlayFor (this),
                                          "Loading " + engineName + "...", true);

    mEngineType   = engineName;
    mEngineLocked = true;

    // View teardown only -- any previous engine is rig-owned.
    if (mEngineEditor && mPlayerTab)
        mPlayerTab->removeChildComponent(mEngineEditor.get());
    mEngineEditor.reset();
    mEngineProcessor = nullptr;

    // QA-ModelShell TS1: the model constructs, prepares, and registers the
    // engine (mixer-strip params + InsertNode + render task included).  This
    // page keeps a non-owning view pointer and builds the editor.
    auto& rig = mProcessor.engineRig();
    rig.addTab (TabKind::Layers, mPageIndex);
    mEngineProcessor = rig.setEngineType (TabKind::Layers, mPageIndex, engineName);
    if (mEngineProcessor != nullptr)
        mEngineEditor.reset (mEngineProcessor->createEditor());

    // 2026-04-30: wire engine-internal patch picker -> page tab/strip rename.
    // Each engine editor fires onPatchLoaded(filename) after its internal
    // loadPreset succeeds.  Forward through onSoundNameChanged so ribbon tab,
    // mixer strip, and piano-roll context label all rename to the patch.
    {
        auto onPatch = [this](const juce::String& name)
        {
            if (name.isEmpty()) return;
            setTabName(name);
            if (onSoundNameChanged) onSoundNameChanged(name);
        };
        if      (auto* he = dynamic_cast<HarmlessEditor*>      (mEngineEditor.get())) he->onPatchLoaded = onPatch;
        else if (auto* se = dynamic_cast<BaySickSynthEditor*>  (mEngineEditor.get())) se->onPatchLoaded = onPatch;
        else if (auto* pe = dynamic_cast<BaySickPlayerEditor*>    (mEngineEditor.get())) pe->onPatchLoaded = onPatch;
    }

    // Smoke round 2 (Jeff): the SW-3 Swing Mix knob moved OFF the editor
    // title bar onto the PageMenuBar (StandaloneEditor wires it per
    // page-show) so it's visible on every sub-tab.

    if (mEngineEditor && mPlayerTab)
        mPlayerTab->addAndMakeVisible(*mEngineEditor);

    // J-6 EQ unification (2026-05-03): page-level EQ display removed.  Pre-rack
    // EQ is now bound exclusively by EffectsPage when the user selects this
    // insert there (mixer_layer_<N>_preeq_* APVTS prefix, same params as before).
    juce::MessageManager::callAsync([this] { if (isShowing()) resized(); });

    // Notify StandaloneEditor so it can create the mixer channel strip
    if (onEngineSelected) onEngineSelected();

    // D2: subscribe to the new engine's apvts.state so we refresh the
    // dirty-snapshot every time a preset is loaded via the engine editor's
    // own preset menu (replaceState fires valueTreeRedirected).  Take an
    // initial snapshot now so the just-created engine's default state is
    // the clean baseline.
    subscribeToEngineApvtsState();
    takeStateSnapshot();

    if (onEngineEditorRebuilt) onEngineEditorRebuilt();
}

juce::String LayersPage::stripEngineTitle() const
{
    if (dynamic_cast<HarmlessEditor*>     (mEngineEditor.get())) return HarmlessEditor::getEngineTitle();
    if (dynamic_cast<BaySickSynthEditor*> (mEngineEditor.get())) return BaySickSynthEditor::getEngineTitle();
    if (dynamic_cast<BaySickPlayerEditor*>   (mEngineEditor.get())) return BaySickPlayerEditor::getEngineTitle();
    return {};
}

juce::Colour LayersPage::stripEngineAccent() const
{
    if (dynamic_cast<HarmlessEditor*>     (mEngineEditor.get())) return HarmlessEditor::getEngineAccent();
    if (dynamic_cast<BaySickSynthEditor*> (mEngineEditor.get())) return BaySickSynthEditor::getEngineAccent();
    if (dynamic_cast<BaySickPlayerEditor*>   (mEngineEditor.get())) return BaySickPlayerEditor::getEngineAccent();
    return {};
}

juce::Component* LayersPage::stripPresetButton() const
{
    if (auto* e = dynamic_cast<HarmlessEditor*>     (mEngineEditor.get())) return e->getTitleStripPresetButton();
    if (auto* e = dynamic_cast<BaySickSynthEditor*> (mEngineEditor.get())) return e->getTitleStripPresetButton();
    if (auto* e = dynamic_cast<BaySickPlayerEditor*>   (mEngineEditor.get())) return e->getTitleStripPresetButton();
    return nullptr;
}

// ── Component overrides ───────────────────────────────────────────────────────
void LayersPage::paint(Graphics& g)
{
    g.fillAll(VC::Bg);

    // Engineless pages can only come from a pre-QA-Layout project save --
    // every add route now names the engine up front (L4).
    if (mActiveTab == 0 && !mEngineLocked)
    {
        g.setColour(VC::TextDim);
        g.setFont(Font(15.f));
        g.drawText("No engine loaded", getLocalBounds(), Justification::centred);
    }
}

void LayersPage::resized()
{
    auto b = getLocalBounds();

    // ── Content fills full bounds (tabs live in PageMenuBar above) ────────────
    if (mPlayerTab)
    {
        mPlayerTab->setBounds(b);
        if (mEngineEditor && mPlayerTab->getHeight() > 0)
            mEngineEditor->setBounds(mPlayerTab->getLocalBounds());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Context label sync
// ─────────────────────────────────────────────────────────────────────────────
void LayersPage::setTabName(const juce::String& name)
{
    mTabName = name;
}

// ─────────────────────────────────────────────────────────────────────────────
// D1.4-fix (c) - per-layer right-click context menu + save / delete
// ─────────────────────────────────────────────────────────────────────────────
// G-6 (2026-04-29): sLayerClipboard removed.  Was used by Copy/Paste menu
// items which are now dropped - Duplicate covers all state-clone use cases.

static juce::File layerEnginePresetsDir (const juce::String& engineName)
{
    // Documents/BaySickDAW/Presets/<EngineName>/My Presets
    return AppPaths::appRoot()
               .getChildFile ("Presets")
               .getChildFile (engineName)
               .getChildFile ("My Presets");
}

// 2026-04-25 (Load Preset support).
// Top-level engine presets directory - siblings under <Documents>/BaySickDAW/
// Presets/<engineName>/ are factory bundles; "My Presets" is user-saved.
static juce::File layerEngineRootPresetsDir (const juce::String& engineName)
{
    return AppPaths::appRoot()
               .getChildFile ("Presets")
               .getChildFile (engineName);
}

// Engine paramPrefix accessor - returns this page's local prefix for the
// currently-loaded engine.
static juce::String layerEngineLocalPrefix (juce::AudioProcessor* proc)
{
    if (auto* bss = dynamic_cast<BaySickSynthProcessor*>(proc))  return bss->getParamPrefix();
    if (auto* bsp = dynamic_cast<BaySickPlayerProcessor*>(proc))    return bsp->getParamPrefix();
    if (auto* h   = dynamic_cast<HarmlessProcessor*>(proc))      return h  ->getParamPrefix();
    return {};
}

// Apply a ValueTree (raw apvts state) to whichever engine is loaded.
static void layerApplyApvtsTree (juce::AudioProcessor* proc, const juce::ValueTree& vt)
{
    if (auto* bss = dynamic_cast<BaySickSynthProcessor*>(proc))  { bss->apvts.replaceStateKeepingUndoHistory(vt); return; }
    if (auto* bsp = dynamic_cast<BaySickPlayerProcessor*>(proc))    { bsp->apvts.replaceStateKeepingUndoHistory(vt); return; }
    if (auto* h   = dynamic_cast<HarmlessProcessor*>(proc))      { h  ->apvts.replaceStateKeepingUndoHistory(vt); return; }
}

// G-6 (2026-04-29): rewrite the binary state's PARAM ids so the source
// page's APVTS prefix (tk_lay_<srcIdx>_<engineTag>_) becomes this destination
// page's prefix.  Without this, importLayerState() silently drops every
// param because setStateInformation matches by id.  Mirrors the F-2 fix
// pattern in loadPreset().  Operates in-place on the MemoryBlock - decoded
// XML → ValueTree → substitute → re-encoded back into mb.
static void layerSubstituteEnginePrefixInBinary (juce::AudioProcessor* proc,
                                                 juce::MemoryBlock& mb)
{
    if (proc == nullptr || mb.getSize() == 0) return;

    const juce::String localPrefix = layerEngineLocalPrefix (proc);
    if (localPrefix.isEmpty()) return;

    // Engine tag = segment immediately before the trailing underscore
    // (e.g. "_bss_" out of "tk_lay_1_bss_").
    const int trailingUnder = localPrefix.length() - 1;
    if (trailingUnder < 1) return;
    const int tagStart = localPrefix.substring (0, trailingUnder).lastIndexOfChar ('_');
    if (tagStart < 0) return;
    const juce::String engineTagWithUnders = localPrefix.substring (tagStart, trailingUnder + 1);

    auto xmlEl = SafeXml::parseBinaryBlob (mb.getData(), (int) mb.getSize());
    if (! xmlEl) return;

    juce::ValueTree loaded = juce::ValueTree::fromXml (*xmlEl);
    if (! loaded.isValid()) return;

    // Find the source page's prefix from the first PARAM whose id contains
    // the engine tag.
    juce::String loadedPrefix;
    for (int i = 0; i < loaded.getNumChildren(); ++i)
    {
        auto child = loaded.getChild (i);
        if (! child.hasType ("PARAM")) continue;
        const juce::String id = child.getProperty ("id").toString();
        const int idx = id.indexOf (engineTagWithUnders);
        if (idx > 0 && id.startsWith ("tk_"))
        {
            loadedPrefix = id.substring (0, idx + engineTagWithUnders.length());
            break;
        }
    }

    if (loadedPrefix.isEmpty() || loadedPrefix == localPrefix) return;   // nothing to do

    for (int i = 0; i < loaded.getNumChildren(); ++i)
    {
        auto child = loaded.getChild (i);
        if (! child.hasType ("PARAM")) continue;
        juce::String id = child.getProperty ("id").toString();
        if (id.startsWith (loadedPrefix))
        {
            id = localPrefix + id.substring (loadedPrefix.length());
            child.setProperty ("id", id, nullptr);
        }
    }

    if (auto modifiedXml = loaded.createXml())
    {
        mb.reset();
        juce::AudioProcessor::copyXmlToBinary (*modifiedXml, mb);
    }
}

// Recursive XML-preset walker - folders become real cascading submenus.
// Matches the sample-picker UX (addLibDirToMenuDP in DrumPage.cpp).
// 2026-04-26: skipDrumFolders filters out top-level folders matching
// SampleLibrary::isDrumPack (Hip Hop Drums, EDM Drums, Percussion).  Used
// for BaySickPlayer presets in melodic (Layer/Bass) context.  Always keeps
// "My Presets/".  The flag is only consulted at the top level - once we
// descend into a kept folder, all its contents come in.
static void addLayerPresetDirToMenu (juce::PopupMenu& menu,
                                      const juce::File& dir,
                                      int kPresetBase,
                                      juce::Array<juce::File>& presetXmls,
                                      bool skipDrumFolders = false)
{
    juce::Array<juce::File> dirs;
    dir.findChildFiles (dirs, juce::File::findDirectories, false);
    dirs.sort();
    juce::Array<juce::File> files;
    dir.findChildFiles (files, juce::File::findFiles, false, "*.xml");
    files.sort();

    for (const auto& child : dirs)
    {
        if (skipDrumFolders)
        {
            const auto fname = child.getFileName();
            const bool isUser = fname.equalsIgnoreCase ("My Presets");
            if (! isUser && SampleLibrary::isDrumPack (fname))
                continue;
        }
        juce::PopupMenu sub;
        // Recursing - don't propagate skipDrumFolders, only filter the top level.
        addLayerPresetDirToMenu (sub, child, kPresetBase, presetXmls, false);
        if (sub.getNumItems() > 0)
            menu.addSubMenu (child.getFileName(), sub);
    }
    for (const auto& f : files)
    {
        const int id = kPresetBase + presetXmls.size();
        menu.addItem (id, f.getFileNameWithoutExtension());
        presetXmls.add (f);
    }
}

// QA-Layout T2 (L4/L31): the engine picker died, so its context menu (Lock /
// Polyphony / Rename / Duplicate / Choke / Save Patch / Load Preset / Delete)
// lives here on the title strip's Menu dropdown, merged with the page-preset
// entries.  One Delete only.
void LayersPage::showPageActionsMenu (juce::Component* anchor)
{
    constexpr int kIdLock      = 1;
    constexpr int kIdPolyphony = 2;
    constexpr int kIdRename    = 5;
    constexpr int kIdDuplicate = 12;
    // G-6 (2026-04-29): Copy/Paste menu items dropped per Jeff's spec
    // ("copy and duplicate are the same thing").  Duplicate is the only
    // state-clone operation now - spawns a new tab with cloned settings.
    constexpr int kIdSaveAs    = 20;
    // D3: choke-group submenu - 200 = None, 201..216 = groups 1..16.
    constexpr int kIdChokeBase = 200;
    // 2026-04-25: Load Preset submenu - 500 + i indexes into presetXmls[].
    constexpr int kIdLoadPresetBase = 500;
    constexpr int kIdSavePagePreset = 100;
    // Page-preset load submenu - 1000 + i indexes into pagePresetXmls[].
    constexpr int kIdLoadBase  = 1000;
    constexpr int kIdDelete    = 99;
    // Replace Engine submenu (Jeff, 2026-08-16) - 60 + i indexes kLayerEngines.
    constexpr int kIdReplaceBase = 60;
    static const char* const kLayerEngines[] = { "Harmless", "BaySickSynth", "BaySickPlayer" };
    constexpr int kNumLayerEngines = 3;

    juce::PopupMenu menu;
    if (onBuildWindowNavMenu) { onBuildWindowNavMenu (menu); menu.addSeparator(); }
    menu.addItem (kIdLock, "Lock Layer", true, mLocked);

    // Polyphony - engine-specific param.
    //   BaySickSynth → tk_lay_N_bss_voiceMode  (0=poly, 1=mono)
    //   BaySickPlayer → tk_lay_N_bsp_voiceCap   (1=mono, 8=poly)
    //   Harmless     → polyphonic-only (n/a)
    {
        bool isMono = false;
        bool canToggle = false;
        // MF-5 (QA-Manuals 2026-08-11): these ids live on the ENGINE's own
        // APVTS, not on the main processor's - BaySickSynthProcessor builds its
        // layout with createLayout ("tk_" + trackId + "_bss_") against its own
        // AudioProcessorValueTreeState.  Reading them off mProcessor.apvts
        // returned nullptr every time, so the label was permanently
        // "Polyphonic" and the click did nothing.  DrumPage always did this
        // correctly; resolve through the engine the same way.
        if (auto* bss = dynamic_cast<BaySickSynthProcessor*> (mEngineProcessor))
        {
            // voiceMode raw: 0=Poly, 1=Mono, 2=Legato.  Anything > 0 counts as
            // monophonic for the label.
            if (auto* p = bss->apvts.getRawParameterValue (bss->getParamPrefix() + "voiceMode"))
                isMono = (int) std::round (p->load()) >= 1;
            canToggle = true;
        }
        else if (auto* vp = dynamic_cast<BaySickPlayerProcessor*> (mEngineProcessor))
        {
            if (auto* p = vp->apvts.getRawParameterValue (vp->getParamPrefix() + "voiceCap"))
                isMono = p->load() <= 1.5f;
            canToggle = true;
        }
        const juce::String label = canToggle
            ? (isMono ? juce::String ("Polyphony: Monophonic")
                      : juce::String ("Polyphony: Polyphonic"))
            : juce::String ("Polyphony: (n/a)");
        menu.addItem (kIdPolyphony, label, canToggle);
    }

    menu.addSeparator();
    menu.addItem (kIdRename, "Rename...");
    // Replace Engine (Jeff, 2026-08-16): swap this tab's engine in place -
    // notes, strip, rack and window all stay; no delete prompt because the
    // tab never dies.  Current engine ticked and disabled; locked pages
    // can't swap, same rule as Delete.
    {
        juce::PopupMenu repSub;
        for (int i = 0; i < kNumLayerEngines; ++i)
            repSub.addItem (kIdReplaceBase + i, kLayerEngines[i],
                            mEngineType != kLayerEngines[i],
                            mEngineType == kLayerEngines[i]);
        menu.addSubMenu ("Replace Engine", repSub, ! mLocked && ! mEngineType.isEmpty());
    }
    menu.addItem (kIdDuplicate, "Duplicate Layer (new tab)", ! mEngineType.isEmpty());

    menu.addSeparator();
    // D3: Choke Group submenu - global cross-engine cut bus.
    {
        const juce::String prefix = "mixer_layer_" + juce::String (mPageIndex) + "_chokeGroup";
        int curGroup = 0;
        if (auto* p = mProcessor.apvts.getRawParameterValue (prefix))
            curGroup = juce::jlimit (0, 16, (int) std::round (p->load()));

        juce::PopupMenu chokeSub;
        chokeSub.addItem (kIdChokeBase, "None", true, curGroup == 0);
        for (int g = 1; g <= 16; ++g)
            chokeSub.addItem (kIdChokeBase + g, "Group " + juce::String (g),
                              true, curGroup == g);
        menu.addSubMenu ("Choke Group", chokeSub);
    }

    menu.addSeparator();
    const bool canSave = mEngineProcessor != nullptr && ! mEngineType.isEmpty();
    menu.addItem (kIdSaveAs, "Save Current Patch As...", canSave);

    // ── Load Preset submenu - walks Documents/BaySickDAW/Presets/<engineName>/
    //    with real cascading submenus per folder (matches sample-picker UX).
    juce::Array<juce::File> presetXmls;
    {
        juce::PopupMenu loadSub;
        const auto root = layerEngineRootPresetsDir (mEngineType);
        // 2026-04-26: BaySickPlayer presets include drum-sample subfolders
        // (Hip Hop Drums / EDM Drums) - Layer page is melodic context, so
        // skip them here.  Other engines (BaySickSynth / Harmless) don't
        // have drum subfolders so the flag is a no-op for them.
        const bool skipDrums = (mEngineType == "BaySickPlayer");
        if (root.isDirectory() && ! mEngineType.isEmpty())
            addLayerPresetDirToMenu (loadSub, root, kIdLoadPresetBase, presetXmls, skipDrums);
        if (presetXmls.isEmpty())
            loadSub.addItem (-1, "(no presets installed)", false, false);
        menu.addSubMenu ("Load Preset", loadSub, mEngineProcessor != nullptr);
    }

    menu.addSeparator();
    menu.addItem (kIdSavePagePreset, "Save Page Preset As...",
                  mEngineProcessor != nullptr);
    juce::Array<juce::File> pagePresetXmls;
    {
        juce::PopupMenu loadSub;
        const auto root = PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Layer);
        if (root.isDirectory())
        {
            juce::Array<juce::File> files;
            root.findChildFiles (files, juce::File::findFiles, false, "*.xml");
            files.sort();
            for (auto& f : files)
            {
                const int id = kIdLoadBase + pagePresetXmls.size();
                pagePresetXmls.add (f);
                loadSub.addItem (id, f.getFileNameWithoutExtension());
            }
        }
        if (pagePresetXmls.isEmpty())
            loadSub.addItem (-1, "(no page presets saved)", false, false);
        menu.addSubMenu ("Load Page Preset", loadSub);
    }

    menu.addSeparator();
    menu.addItem (kIdDelete, "Delete Layer", ! mLocked);   // locked layers can't be deleted

    juce::Component::SafePointer<LayersPage> safeThis (this);
    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (anchor),
        [safeThis, presetXmls = std::move (presetXmls),
         pagePresetXmls = std::move (pagePresetXmls),
         kIdLock, kIdPolyphony, kIdRename, kIdDuplicate, kIdChokeBase,
         kIdSaveAs, kIdLoadPresetBase, kIdSavePagePreset, kIdLoadBase,
         kIdDelete, kIdReplaceBase, kNumLayerEngines] (int r) mutable
        {
            if (! safeThis || r <= 0) return;
            auto* lp = safeThis.getComponent();

            // Replace Engine submenu (60 + i -> kLayerEngines[i]).  One
            // structural row; undo restores the old engine with its settings.
            if (r >= kIdReplaceBase && r < kIdReplaceBase + kNumLayerEngines)
            {
                static const char* const kEngines[] = { "Harmless", "BaySickSynth", "BaySickPlayer" };
                const juce::String name = kEngines[r - kIdReplaceBase];
                lp->performChainSwapGesture ("Replace Engine",
                    [lp, name] { lp->selectEngineInternal (name); });
                return;
            }

            // Page-preset load submenu (1000 + i -> pagePresetXmls[i]).
            if (r >= kIdLoadBase && r < kIdLoadBase + pagePresetXmls.size())
            {
                lp->performChainSwapGesture ("Load Page Preset",
                    [lp, f = pagePresetXmls[r - kIdLoadBase]] { lp->loadPagePreset (f); });
                return;
            }

            // Load Preset submenu (500 + i → presetXmls[i]).
            if (r >= kIdLoadPresetBase && r < kIdLoadPresetBase + presetXmls.size())
            {
                lp->loadPreset (presetXmls[r - kIdLoadPresetBase]);
                return;
            }

            // D3: choke-group submenu (200..216 → 0..16).
            if (r >= kIdChokeBase && r <= kIdChokeBase + 16)
            {
                const int newGroup = r - kIdChokeBase;
                const juce::String prefix = "mixer_layer_" + juce::String (lp->mPageIndex) + "_chokeGroup";
                beginParamUndoGesture (lp->mProcessor.apvts, prefix); // Task 6 (12-iv)
                if (auto* p = lp->mProcessor.apvts.getParameter (prefix))
                {
                    const auto& range = p->getNormalisableRange();
                    p->setValueNotifyingHost (range.convertTo0to1 ((float) newGroup));
                }
                return;
            }

            if      (r == kIdLock) lp->toggleLockUndoable();
            else if (r == kIdPolyphony)
            {
                // MF-5: resolve through the engine's own APVTS - see the
                // matching note where the menu label is built.
                if (auto* bss = dynamic_cast<BaySickSynthProcessor*> (lp->mEngineProcessor))
                {
                    // voiceMode is Poly(0) / Mono(1) / Legato(2).  Toggle
                    // Poly <-> Mono only; Legato stays a panel-only pick.
                    if (auto* p = bss->apvts.getParameter (bss->getParamPrefix() + "voiceMode"))
                    {
                        const auto& range = p->getNormalisableRange();
                        const int curRaw  = (int) std::round (range.convertFrom0to1 (p->getValue()));
                        const int nextRaw = (curRaw == 0) ? 1 : 0;
                        beginParamUndoGesture (bss->apvts, bss->getParamPrefix() + "voiceMode"); // Task 6 (12-iv)
                        p->setValueNotifyingHost (range.convertTo0to1 ((float) nextRaw));
                    }
                }
                else if (auto* vp = dynamic_cast<BaySickPlayerProcessor*> (lp->mEngineProcessor))
                {
                    if (auto* p = vp->apvts.getParameter (vp->getParamPrefix() + "voiceCap"))
                    {
                        const auto& range = p->getNormalisableRange();
                        const float curRaw  = range.convertFrom0to1 (p->getValue());
                        const float nextRaw = (curRaw <= 1.5f) ? 8.f : 1.f;
                        beginParamUndoGesture (vp->apvts, vp->getParamPrefix() + "voiceCap"); // Task 6 (12-iv)
                        p->setValueNotifyingHost (range.convertTo0to1 (nextRaw));
                    }
                }
            }
            else if (r == kIdRename)
            {
                if (lp->onRenameRequested) lp->onRenameRequested();
            }
            // G-6: kIdCopy / kIdPaste handlers removed (menu items dropped).
            else if (r == kIdDuplicate)
            {
                const auto state = lp->exportLayerState();
                if (state.isNotEmpty() && lp->onDuplicateRequested)
                    lp->onDuplicateRequested (state);
            }
            else if (r == kIdSaveAs)         lp->savePatchAs();
            else if (r == kIdSavePagePreset) lp->savePagePreset();
            else if (r == kIdDelete)         lp->requestDelete();
        });
}

juce::String LayersPage::exportLayerState() const
{
    if (mEngineType.isEmpty() || mEngineProcessor == nullptr) return {};
    juce::XmlElement el ("LayerPageState");
    el.setAttribute ("engine", mEngineType);
    el.setAttribute ("locked", mLocked ? 1 : 0);
    juce::MemoryBlock mb;
    mEngineProcessor->getStateInformation (mb);
    el.setAttribute ("data", mb.toBase64Encoding());
    return el.toString (juce::XmlElement::TextFormat().singleLine());
}

void LayersPage::importLayerState (const juce::String& xml)
{
    if (xml.isEmpty() || mLocked) return;
    auto parsed = SafeXml::parse (xml);
    if (! parsed || ! parsed->hasTagName ("LayerPageState")) return;

    const juce::String engine = parsed->getStringAttribute ("engine");
    const bool         lock   = parsed->getIntAttribute    ("locked", 0) != 0;
    const juce::String data   = parsed->getStringAttribute ("data");
    if (engine.isEmpty()) return;

    selectEngine (engine);
    if (mEngineProcessor && data.isNotEmpty())
    {
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (data))
        {
            // G-6 (2026-04-29): rewrite the binary's PARAM ids so the source
            // page's prefix becomes this destination page's prefix.  Without
            // this, every param in the cloned state silently fails to apply
            // because setStateInformation matches by id and the per-tab
            // prefixes (tk_lay_<idx>_<engineTag>_) differ between source and
            // destination.  Mirrors the F-2 fix in loadPreset().
            layerSubstituteEnginePrefixInBinary (mEngineProcessor, mb);
            mEngineProcessor->setStateInformation (mb.getData(), (int) mb.getSize());
        }
    }
    // Go through setLocked, not the field: the ribbon's own lock flag ([L]
    // marker + its Delete guard) is only ever written by onLockChanged, so a
    // bare field write leaves the duplicate locked on the page and unlocked in
    // the ribbon.  setLocked early-outs on no change, so the usual
    // false -> false duplicate stays silent.
    setLocked (lock);
}

void LayersPage::loadPreset (const juce::File& xml)
{
    // The SFZ reader banks every unresolved sample= line; without a scope of
    // its own this gesture's entries surface later under whatever drains next.
    MissingFileReport::ScopedGesture gesture ("preset");

    if (mEngineProcessor == nullptr || ! xml.existsAsFile()) return;

    auto px = SafeXml::parse (xml);
    if (! px) return;

    // QA-I: context-menu preset path -- the engine is already locked, so
    // selectEngine's overlay never opens; wrap the (possibly multi-second) SFZ
    // load here so the UI shows a busy indicator instead of freezing.
    HeavyOperationOverlay::ScopedOp busy (StandaloneEditor::busyOverlayFor (this),
                                          "Loading Preset...", true);

    // 2026-04-26: detect BaySickPlayer factory wrapper format.  Outer
    // <BaySickPlayerState> contains an inner <BaySickPlayerState> apvts
    // state plus a <Sample kind path/> sibling pointing at the SFZ.
    // Stash the Sample element here; we'll load it after the apvts state
    // is applied below.
    std::unique_ptr<juce::XmlElement> bspSample;
    if (px->hasTagName ("BaySickPlayerState"))
        if (auto* inner = px->getChildByName ("BaySickPlayerState"))
        {
            if (auto* s = px->getChildByName ("Sample"))
                bspSample = std::make_unique<juce::XmlElement> (*s);
            // Replace px with the inner apvts state so the existing logic
            // below treats it as a bare engine state tree.
            px = std::make_unique<juce::XmlElement> (*inner);
        }

    std::unique_ptr<juce::XmlElement> innerXml;
    if (px->hasTagName ("BaySickEnginePreset"))
    {
        const auto data = px->getStringAttribute ("data");
        if (data.isEmpty()) return;
        juce::MemoryBlock mb;
        if (! mb.fromBase64Encoding (data)) return;
        if (auto x = SafeXml::parseBinaryBlob (mb.getData(), (int) mb.getSize()))
            innerXml = std::move (x);
    }
    else
    {
        innerXml = std::move (px);
    }
    if (! innerXml) return;

    juce::ValueTree loaded = juce::ValueTree::fromXml (*innerXml);
    if (! loaded.isValid()) return;

    const juce::String localPrefix = layerEngineLocalPrefix (mEngineProcessor);
    if (localPrefix.isEmpty()) return;

    // F-2 fix (2026-04-26): localPrefix format is `tk_<row>_<idx>_<engineTag>_`
    // (e.g. "tk_lay_1_bss_").  The earlier 3-segment-style computation
    // returned the index segment ("_1_") instead of the engine tag ("_bss_"),
    // so the substring search against loaded PARAM ids - which carry a
    // different page index ("_0_" from the saved slot) - never matched and
    // the substitution silently no-op'd.  Engine sounded default after load.
    // Engine tag = segment immediately before the trailing underscore.
    const int trailingUnder = localPrefix.length() - 1;
    if (trailingUnder < 1) return;
    const int tagStart = localPrefix.substring (0, trailingUnder).lastIndexOfChar ('_');
    if (tagStart < 0) return;
    const juce::String engineTagWithUnders = localPrefix.substring (tagStart, trailingUnder + 1);

    juce::String loadedPrefix;
    for (int i = 0; i < loaded.getNumChildren(); ++i)
    {
        auto child = loaded.getChild (i);
        if (! child.hasType ("PARAM")) continue;
        const juce::String id = child.getProperty ("id").toString();
        const int idx = id.indexOf (engineTagWithUnders);
        if (idx > 0 && id.startsWith ("tk_"))
        {
            loadedPrefix = id.substring (0, idx + engineTagWithUnders.length());
            break;
        }
    }

    if (loadedPrefix.isNotEmpty() && loadedPrefix != localPrefix)
    {
        for (int i = 0; i < loaded.getNumChildren(); ++i)
        {
            auto child = loaded.getChild (i);
            if (! child.hasType ("PARAM")) continue;
            juce::String id = child.getProperty ("id").toString();
            if (id.startsWith (loadedPrefix))
            {
                id = localPrefix + id.substring (loadedPrefix.length());
                child.setProperty ("id", id, nullptr);
            }
        }
    }

    layerApplyApvtsTree (mEngineProcessor, loaded);

    // 2026-04-26: BaySickPlayer factory presets carry a sibling <Sample>
    // element pointing at an SFZ in the Core Library.  Resolve and load
    // it here.  Unlike DrumPage we DO NOT call normalizeRootNotes - Layers
    // need to preserve the SFZ's natural keymap for melodic playback.
    if (bspSample)
        if (auto* vp = dynamic_cast<BaySickPlayerProcessor*>(mEngineProcessor))
        {
            const juce::String kind = bspSample->getStringAttribute ("kind", "none");
            const juce::String pathStr = bspSample->getStringAttribute ("path");
            // resolvePersistedRef, not a local "library:" test: savePatchAs
            // carries bsp_loadPath through verbatim, which refForPersist
            // writes as a stable ref under EITHER library root, so a My
            // Samples sample would otherwise read as missing.
            const juce::File path = SampleLibrary::resolvePersistedRef (pathStr);

            // 2026-05-02: route through the processor wrappers so the
            // sample path lives on apvts.state for project-save replay.
            // No normalizeRoot here -- preserve SFZ keymap for melodic play.
            if (kind != "none")
            {
                bool ok = false;
                if      (kind == "sfz"    && path.existsAsFile()) { vp->loadSampleSFZ    (path); ok = true; }
                else if (kind == "file"   && path.existsAsFile()) { vp->loadSampleFile   (path); ok = true; }
                else if (kind == "folder" && path.isDirectory())  { vp->loadSampleFolder (path); ok = true; }

                if (! ok)
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::MessageBoxIconType::WarningIcon,
                        "Load Preset",
                        "This preset's sample is missing from this machine:\n"
                        + path.getFullPathName()
                        + "\n\nThe preset's settings were applied but it will not make sound.",
                        "OK");
            }
        }

    // Use the preset's filename as the tab's display name.  Bass/Layers
    // don't have a separate "sound name" field like DrumPage - tab name +
    // engine type is the only display surface.
    const juce::String newName = xml.getFileNameWithoutExtension();
    setTabName (newName);
    if (onSoundNameChanged) onSoundNameChanged (newName);
    takeStateSnapshot();
}

void LayersPage::savePatchAs()
{
    if (mEngineProcessor == nullptr || mEngineType.isEmpty()) return;

    auto* aw = new juce::AlertWindow ("Save Patch As",
                                       "Enter a name to save this layer as a preset:",
                                       juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", "My Patch");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<LayersPage> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, aw] (int r)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (r != 1 || ! safeThis) return;
            const auto name = aw->getTextEditorContents ("name").trim();
            auto* lp = safeThis.getComponent();
            if (lp->mEngineProcessor == nullptr) return;

            // The engine editor's preset button on this page's title strip
            // enumerates the same My Presets folder, and that editor's loader
            // gates on the engine's own root tag - so the file must be
            // written in the engine's native format, not a page-side wrapper.
            // Flat state XML for the synth engines (getStateInformation keeps
            // Harmless's mod registry aboard); BaySickPlayer gets the nested
            // <BaySickPlayerState> + <Sample> pair every player reader
            // expects, mirroring BaySickPlayerEditor::savePreset.
            juce::MemoryBlock mb;
            lp->mEngineProcessor->getStateInformation (mb);
            auto stateXml = SafeXml::parseBinaryBlob (
                                mb.getData(), (int) mb.getSize());
            if (! stateXml) return;

            auto onWritten = [safeThis] (const UserFileSave::Result& saved)
            {
                // A collision prompt can hold this open long enough for the
                // page to be closed, so the SafePointer is re-tested here
                // rather than trusted from the naming callback.
                if (! saved || ! safeThis) return;
                safeThis->takeStateSnapshot();   // saved patch is the new clean baseline
            };

            const auto dir = layerEnginePresetsDir (lp->mEngineType);

            if (dynamic_cast<BaySickPlayerProcessor*> (lp->mEngineProcessor) != nullptr)
            {
                // bsp_loadKind / bsp_loadPath are the processor's persistence
                // contract, stamped onto apvts.state on every sample load and
                // carried into the state XML as root attributes.
                const juce::String kind = stateXml->getStringAttribute ("bsp_loadKind");
                const juce::String path = stateXml->getStringAttribute ("bsp_loadPath");

                juce::XmlElement root ("BaySickPlayerState");
                root.addChildElement (stateXml.release());

                // A kind with no path behind it is what makes the reader name
                // a blank file in its "sample is missing" box, so the pair is
                // written or neither is.
                const bool haveSample = kind.isNotEmpty() && kind != "none"
                                        && path.isNotEmpty();
                auto* sampleEl = root.createNewChildElement ("Sample");
                sampleEl->setAttribute ("kind", haveSample ? kind
                                                           : juce::String ("none"));
                if (haveSample)
                    sampleEl->setAttribute ("path", path);

                UserFileSave::writeXmlAsync (dir, name, root, onWritten,
                                             "The patch is still unsaved.");
            }
            else
            {
                UserFileSave::writeXmlAsync (dir, name, *stateXml, onWritten,
                                             "The patch is still unsaved.");
            }
        }), false);
}

void LayersPage::requestDelete()
{
    // G-7 (2026-04-29): close-prompt with dirty-aware buttons.  Dirty patch
    // gets the 3-button [Save Page Preset & Delete / Delete / Cancel] flow
    // (Save chains savePagePreset's modal → fireDelete on success).  Clean
    // patch (or no engine) gets the simpler 2-button [Delete / Cancel] -
    // nothing to save.
    juce::Component::SafePointer<LayersPage> safeThis (this);
    auto fireDelete = [safeThis] {
        if (auto* lp = safeThis.getComponent())
            if (lp->onDeleteRequested) lp->onDeleteRequested();
    };

    const juce::String warning =
        "Deleting this layer removes its Player, Mixer Strip, "
        "Effects Rack, and Piano Roll.";

    if (mEngineProcessor != nullptr && isPatchDirty())
    {
        const juce::String dirtyWarning = warning
            + "\n\n\"Save Page Preset & Delete\" writes the entire page state "
              "(engine + EQ + effects rack + strip settings) to disk first, "
              "then deletes the tab.";

        auto* aw = new juce::AlertWindow (
            "Delete Layer", dirtyWarning, juce::AlertWindow::QuestionIcon);
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
        "Delete Layer", warning, juce::AlertWindow::WarningIcon);
    aw->addButton ("Delete", 1);
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [aw, fireDelete] (int r)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (r == 1) fireDelete();
        }), false);
}

// ── D2 lock + dirty-snapshot helpers ─────────────────────────────────────────
void LayersPage::setLocked (bool l)
{
    if (mLocked == l) return;
    mLocked = l;
    if (onLockChanged) onLockChanged();
}

void LayersPage::toggleLockUndoable()
{
    const bool before = mLocked;
    const bool after  = ! before;
    setLocked (after);

    if (! mUndoCtx.isValid()) return;

    // Jeff ruling 2026-08-06: resolve the LIVE page at apply time (a
    // delete+resurrect cycle replaces this object; SafePointer = fallback).
    auto resolveSelf = mUndoCtx.resolveOwnerPage;
    auto sp = juce::Component::SafePointer<LayersPage> (this);
    auto livePage = [resolveSelf, sp]() -> LayersPage*
    {
        if (resolveSelf) return dynamic_cast<LayersPage*> (resolveSelf());
        return sp.getComponent();
    };
    mUndoCtx.perform (new StructuralOpAction (
                          [livePage, before] { if (auto* lp = livePage()) lp->setLocked (before); },
                          [livePage, after]  { if (auto* lp = livePage()) lp->setLocked (after);  }),
                      after ? "Lock Layer" : "Unlock Layer");
}

void LayersPage::takeStateSnapshot()
{
    mLoadedStateSnapshot.reset();
    if (mEngineProcessor)
        mEngineProcessor->getStateInformation (mLoadedStateSnapshot);
}

bool LayersPage::isPatchDirty() const
{
    if (mEngineProcessor == nullptr) return false;
    if (mLoadedStateSnapshot.getSize() == 0) return false;
    juce::MemoryBlock current;
    mEngineProcessor->getStateInformation (current);
    return current != mLoadedStateSnapshot;
}

void LayersPage::valueTreeRedirected (juce::ValueTree& tree)
{
    // Engine apvts.state was replaced (preset load via engine editor) - refresh
    // baseline so the just-loaded preset becomes the new clean state.
    juce::ignoreUnused (tree);
    takeStateSnapshot();
}

void LayersPage::subscribeToEngineApvtsState()
{
    if (auto* h = dynamic_cast<HarmlessProcessor*>     (mEngineProcessor)) h->apvts.state.addListener (this);
    else if (auto* s = dynamic_cast<BaySickSynthProcessor*>(mEngineProcessor)) s->apvts.state.addListener (this);
    else if (auto* v = dynamic_cast<BaySickPlayerProcessor*>  (mEngineProcessor)) v->apvts.state.addListener (this);
}

void LayersPage::unsubscribeFromEngineApvtsState()
{
    if (auto* h = dynamic_cast<HarmlessProcessor*>     (mEngineProcessor)) h->apvts.state.removeListener (this);
    else if (auto* s = dynamic_cast<BaySickSynthProcessor*>(mEngineProcessor)) s->apvts.state.removeListener (this);
    else if (auto* v = dynamic_cast<BaySickPlayerProcessor*>  (mEngineProcessor)) v->apvts.state.removeListener (this);
}

// ─────────────────────────────────────────────────────────────────────────────
// G-7 (2026-04-29): Page Preset save/load (full chain - engine + strip +
// insert rack + post-EQ).  Surfaced via the page menu bar's hamburger ≡.
// ─────────────────────────────────────────────────────────────────────────────
static juce::String layerEnginePrefixOf (juce::AudioProcessor* p)
{
    if (auto* h   = dynamic_cast<HarmlessProcessor*>     (p)) return h  ->getParamPrefix();
    if (auto* s   = dynamic_cast<BaySickSynthProcessor*> (p)) return s  ->getParamPrefix();
    if (auto* v   = dynamic_cast<BaySickPlayerProcessor*>   (p)) return v  ->getParamPrefix();
    return {};
}

void LayersPage::savePagePreset (std::function<void()> onSaved)
{
    auto* aw = new juce::AlertWindow ("Save Page Preset",
                                       "Enter a name for this layer page preset:",
                                       juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", "My Layer");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<LayersPage> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, aw, onSaved] (int r)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (r != 1 || ! safeThis) return;

            const juce::String name = aw->getTextEditorContents ("name").trim();

            const juce::String stripPrefix = "mixer_layer_" + juce::String (safeThis->mPageIndex);
            const juce::String enginePrefix = layerEnginePrefixOf (safeThis->mEngineProcessor);

            const juce::String xml = PagePresetIO::exportPagePreset (
                safeThis->mProcessor,
                PagePresetIO::PageKind::Layer,
                safeThis->mPageIndex,
                stripPrefix,
                safeThis->mEngineProcessor,
                safeThis->mEngineType,
                enginePrefix);

            UserFileSave::writeTextAsync (
                PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Layer),
                name, xml,
                [safeThis, onSaved] (const UserFileSave::Result& saved)
                {
                    // A collision prompt can hold this open long enough for the
                    // page to be closed, so the SafePointer is re-tested here
                    // rather than trusted from the naming callback.
                    if (! saved || ! safeThis) return;

                    safeThis->takeStateSnapshot();   // saved page is the new clean baseline

                    // G-7: chain the continuation (e.g. requestDelete fires
                    // fireDelete here so Save Page Preset & Delete completes the
                    // delete after the user has finalized the save name).
                    if (onSaved) onSaved();
                },
                UserFileSave::kTabNotDeleted);
        }), false);
}

void LayersPage::loadPagePreset (const juce::File& xml)
{
    if (! xml.existsAsFile()) return;

    applyPagePresetXml (xml.loadFileAsString());

    const juce::String newName = xml.getFileNameWithoutExtension();
    setTabName (newName);
    if (onSoundNameChanged) onSoundNameChanged (newName);
}

void LayersPage::performChainSwapGesture (const juce::String& label,
                                          const std::function<void()>& op)
{
    const juce::String before = capturePagePresetXml();
    if (! mUndoCtx.isValid() || before.isEmpty()) { op(); return; }

    op();
    const juce::String after = capturePagePresetXml();
    if (before == after) return;

    const juce::File beforeF = UndoSnapshotStore::writeNew (before);
    const juce::File afterF  = UndoSnapshotStore::writeNew (after);
    // Jeff ruling 2026-08-06: resolve the LIVE page at apply time (a
    // delete+resurrect cycle replaces this object; SafePointer = fallback).
    auto resolveSelf = mUndoCtx.resolveOwnerPage;
    auto sp = juce::Component::SafePointer<LayersPage> (this);
    auto apply = [resolveSelf, sp] (const juce::File& f)
    {
        auto* lp = resolveSelf ? dynamic_cast<LayersPage*> (resolveSelf())
                               : sp.getComponent();
        if (lp != nullptr && f.existsAsFile())
            lp->applyPagePresetXml (f.loadFileAsString());
    };
    mUndoCtx.perform (new StructuralOpAction ([apply, beforeF] { apply (beforeF); },
                                              [apply, afterF]  { apply (afterF); },
                                              { beforeF, afterF }),
                      label);
}

// QA-UndoCoverage Task 7: the Save/Load Page Preset payload over in-memory
// XML -- the structural-undo snapshot capture/apply rides these.
juce::String LayersPage::capturePagePresetXml()
{
    if (mEngineProcessor == nullptr) return {};
    const juce::String stripPrefix  = "mixer_layer_" + juce::String (mPageIndex);
    const juce::String enginePrefix = layerEnginePrefixOf (mEngineProcessor);
    return PagePresetIO::exportPagePreset (mProcessor,
                                           PagePresetIO::PageKind::Layer,
                                           mPageIndex,
                                           stripPrefix,
                                           mEngineProcessor,
                                           mEngineType,
                                           enginePrefix);
}

void LayersPage::applyPagePresetXml (const juce::String& xmlText)
{
    if (xmlText.isEmpty()) return;

    // RAII rather than a drain at the tail: a bare drain inside an outer
    // gesture (an undo that resurrects several tabs) takes that gesture's
    // entries and posts them under this noun.  Only the outermost scope
    // reports, so nesting keeps the noun the user reads correct.
    MissingFileReport::ScopedGesture gesture ("preset");

    // First peek the engineType so we can swap engines if needed.  Through the
    // INTERNAL route: the public selectEngine no-ops once the "+"-time pick has
    // locked, which left this swap dead - a cross-engine page preset imported
    // the saved blob into the OLD engine (found 2026-08-16 while building
    // Replace Engine, which shares this path).
    const juce::String savedEngineType = PagePresetIO::peekEngineTypeFromXml (xmlText);
    if (savedEngineType.isNotEmpty() && savedEngineType != mEngineType)
        selectEngineInternal (savedEngineType);

    const juce::String stripPrefix = "mixer_layer_" + juce::String (mPageIndex);
    const juce::String enginePrefix = layerEnginePrefixOf (mEngineProcessor);

    // Layer pages don't have secondary buses, so isChannelActive can return
    // true unconditionally - bus fallback is a no-op.
    auto noFallback = [] (int) { return true; };

    PagePresetIO::importPagePreset (mProcessor,
                                     PagePresetIO::PageKind::Layer,
                                     mPageIndex,
                                     stripPrefix,
                                     mEngineProcessor,
                                     enginePrefix,
                                     noFallback,
                                     xmlText);
    takeStateSnapshot();
}

