#include "LayersPage.h"
#include "../BaySickSynth/BaySickSynthProcessor.h"
#include "../BaySickSynth/BaySickSynthEditor.h"
#include "../Harmless/HarmlessProcessor.h"
#include "../Harmless/HarmlessEditor.h"
#include "../VibePlayer/VibePlayerProcessor.h"
#include "../VibePlayer/VibePlayerEditor.h"
#include "../SampleLibrary.h"
#include "PagePresetIO.h"
#include "StandaloneEditor.h"
using namespace juce;

// ── Constructor / Destructor ──────────────────────────────────────────────────
LayersPage::LayersPage(VibeSynthProcessor& p, PatternManager& pm, int pageIndex)
    : mProcessor(p), mPM(pm), mPageIndex(juce::jlimit(0, kMaxLayerPages - 1, pageIndex))
{
    mPageColor = VC::LayerCol[mPageIndex];

    buildPlayerTab();
    // 2026-04-26 (step 2 commit 3): Piano Roll lives on the unified
    // PianoRollPage now, not on this engine page.  buildPianoRollTab is left
    // as dead code (mPianoRoll stays null) and every `if (mPianoRoll) ...`
    // guard becomes a no-op.  The Piano Roll sub-tab pill on the menu bar
    // redirects to PianoRollPage via the editor's showPageForTab handler.
    // J-6 EQ unification (2026-05-03): EQ sub-tab removed - pre + post EQ
    // for this insert now live exclusively on the Effects page (uniform with
    // every other strip type).  The pre-EQ DSP + APVTS params still live on
    // the InsertNode unchanged.

    switchTab(0);
    startTimerHz(24);
}

LayersPage::~LayersPage()
{
    stopTimer();

    // D2: drop the dirty-snapshot listener before the engine processor is freed
    unsubscribeFromEngineApvtsState();

    // Unregister from audio thread first, then wait for any in-flight block to finish
    if (mEngineLocked)
    {
        mProcessor.unregisterLayerEngine(mPageIndex);
    }
    juce::Thread::sleep(20);  // outlasts one audio block (~10 ms)

    // Must remove editor from parent before destroying processor
    if (mEngineEditor && mPlayerTab)
        mPlayerTab->removeChildComponent(mEngineEditor.get());
    mEngineEditor.reset();
    mEngineProcessor.reset();

    // Unregister lazy APVTS params
    if (mEngineLocked)
        mProcessor.unregisterParamsForTrack(trackId());
}

// ── Tab switching ─────────────────────────────────────────────────────────────
void LayersPage::switchTab(int idx)
{
    // J-6 EQ unification: 2 sub-tabs only (Player + Piano Roll).
    mActiveTab = juce::jlimit(0, 1, idx);
    if (mPlayerTab) mPlayerTab->setVisible(mActiveTab == 0);
    if (mPianoRoll) mPianoRoll->setVisible(mActiveTab == 1);
    if (mActiveTab == 1 && mPianoRoll) mPianoRoll->grabKeyboardFocus();
    resized();
    if (onSubTabChanged) onSubTabChanged(mActiveTab);
}

// ── Tab builders ──────────────────────────────────────────────────────────────
void LayersPage::buildPlayerTab()
{
    mPlayerTab = std::make_unique<Component>();
    addAndMakeVisible(*mPlayerTab);

    // "Engine:" label
    mEngineLabel = std::make_unique<Label>("", "Engine:");
    mEngineLabel->setColour(Label::textColourId, VC::TextDim);
    mEngineLabel->setFont(Font(13.f));
    mPlayerTab->addAndMakeVisible(*mEngineLabel);

    // Engine selector combo - locks after first selection.
    // D1.4-fix (c): LockableCombo lets us hijack clicks once locked so the
    // per-layer context menu opens instead of the engine dropdown.
    mEngineCombo = std::make_unique<LockableCombo>();
    mEngineCombo->addItem("Harmless",        1);
    mEngineCombo->addItem("BaySickPlayer",   2);
    mEngineCombo->addItem("BaySickSynth",    3);
    mEngineCombo->setTextWhenNothingSelected("Select engine...");
    mEngineCombo->onChange = [this]
    {
        if (mEngineLocked) return;   // post-lock click handled by LockableCombo
        int sel = mEngineCombo->getSelectedId();
        if      (sel == 1) selectEngine("Harmless");
        else if (sel == 2) selectEngine("BaySickPlayer");
        else if (sel == 3) selectEngine("BaySickSynth");
    };
    mEngineCombo->onLockedClick = [this] { showContextMenu (mEngineCombo.get()); };
    mPlayerTab->addAndMakeVisible(*mEngineCombo);
}

void LayersPage::buildPianoRollTab()
{
    mPianoRoll = std::make_unique<PianoRollContainer>();
    mPianoRoll->setData(&mPM.currentPattern().layerRoll[mPageIndex]);
    // C.5b: pattern's intrinsic TS drives the piano roll's bar-line spacing.
    mPianoRoll->setTimeSignature(mPM.currentPattern().tsNum, mPM.currentPattern().tsDen);
    mPianoRoll->setNoteColor(mPageColor);
    addAndMakeVisible(*mPianoRoll);

    // Initial context label (engine may not be picked yet).
    if (mTabName.isEmpty())
        mTabName = "Layer " + juce::String(mPageIndex);
    refreshPianoRollContextLabel();
}

// ── PlayHead ──────────────────────────────────────────────────────────────────
void LayersPage::setPlayHead(StandalonePlayHead* ph)
{
    mPlayHead = ph;
    if (mPianoRoll)
        mPianoRoll->onSeek = [ph](double b) { if (ph) ph->seekTo(b); };
}

void LayersPage::setUndoContext(const UndoContext& ctx)
{
    if (mPianoRoll)
    {
        mPianoRoll->setUndoContext(ctx);
        if (ctx.showHistory) mPianoRoll->onShowHistoryWindow = ctx.showHistory;
    }
}

// ── Engine selection ──────────────────────────────────────────────────────────
void LayersPage::selectEngine(const juce::String& engineName)
{
    if (mEngineLocked) return;

    HeavyOperationOverlay::ScopedOp busy (StandaloneEditor::busyOverlayFor (this),
                                          "Loading " + engineName + "...", true);

    mEngineType   = engineName;
    mEngineLocked = true;
    refreshPianoRollContextLabel();

    // Register lazy APVTS params (engine + pre-rack EQ + effect rack)
    mProcessor.registerParamsForTrack(trackId(), engineName);

    // Destroy any previous editor before processor (order matters)
    if (mEngineEditor && mPlayerTab)
        mPlayerTab->removeChildComponent(mEngineEditor.get());
    mEngineEditor.reset();
    mEngineProcessor.reset();

    double sr        = mProcessor.getSampleRate() > 0.0 ? mProcessor.getSampleRate() : 44100.0;
    int    blockSize = 512;

    // 2026-04-21: string trackId so engines on different pages don't collide.
    const juce::String trackIdStr = "lay_" + juce::String(mPageIndex);
    if (engineName == "Harmless")
    {
        auto* proc = new HarmlessProcessor(trackIdStr);
        proc->prepareToPlay(sr, blockSize);
        mEngineProcessor.reset(proc);
        mEngineEditor.reset(proc->createEditor());
    }
    else if (engineName == "BaySickPlayer")
    {
        auto* proc = new VibePlayerProcessor(trackIdStr);
        proc->prepareToPlay(sr, blockSize);
        mEngineProcessor.reset(proc);
        mEngineEditor.reset(proc->createEditor());
    }
    else if (engineName == "BaySickSynth")
    {
        auto* proc = new BaySickSynthProcessor(trackIdStr);
        proc->prepareToPlay(sr, blockSize);
        mEngineProcessor.reset(proc);
        mEngineEditor.reset(proc->createEditor());
    }

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
        else if (auto* pe = dynamic_cast<VibePlayerEditor*>    (mEngineEditor.get())) pe->onPatchLoaded = onPatch;
    }

    // Smoke round 2 (Jeff): the SW-3 Swing Mix knob moved OFF the editor
    // title bar onto the PageMenuBar (StandaloneEditor wires it per
    // page-show) so it's visible on every sub-tab.

    // Wire note audition callback (fires on note draw, pitch drag, and key click)
    if (mPianoRoll)
    {
        mPianoRoll->onNoteAudition = [this](int midiNote)
        {
            if (auto* s = dynamic_cast<BaySickSynthProcessor*>(mEngineProcessor.get()))
                s->auditionNote(midiNote);
            else if (auto* h = dynamic_cast<HarmlessProcessor*>(mEngineProcessor.get()))
                h->auditionNote(midiNote);
            else if (auto* v = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor.get()))
                v->auditionNote(midiNote);
        };
        mPianoRoll->onNoteAuditionOn = [this](int midiNote)
        {
            if (auto* s = dynamic_cast<BaySickSynthProcessor*>(mEngineProcessor.get()))
                s->auditionNoteOn(midiNote);
            else if (auto* h = dynamic_cast<HarmlessProcessor*>(mEngineProcessor.get()))
                h->auditionNoteOn(midiNote);
            else if (auto* v = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor.get()))
                v->auditionNoteOn(midiNote);
        };
        mPianoRoll->onNoteAuditionOff = [this](int midiNote)
        {
            if (auto* s = dynamic_cast<BaySickSynthProcessor*>(mEngineProcessor.get()))
                s->auditionNoteOff(midiNote);
            else if (auto* h = dynamic_cast<HarmlessProcessor*>(mEngineProcessor.get()))
                h->auditionNoteOff(midiNote);
            else if (auto* v = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor.get()))
                v->auditionNoteOff(midiNote);
        };
    }

    if (mEngineEditor && mPlayerTab)
        mPlayerTab->addAndMakeVisible(*mEngineEditor);

    // Register with audio thread for rendering in processBlock.
    // registerLayerEngine also creates the Layer InsertNode (which owns preEq).
    mProcessor.registerLayerEngine(mPageIndex, mEngineProcessor.get());

    // Lock combo so engine can't be changed
    if (mEngineCombo)
        mEngineCombo->locked = true;   // D1.4-fix (c): hijack future clicks → menu

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
}

// ── Timer ─────────────────────────────────────────────────────────────────────
void LayersPage::timerCallback()
{
    // J-6 EQ unification (2026-05-03): page-level EQ display removed; the
    // Effects-page Pre EQ tab handles its own syncFromDSP polling now.

    // Update piano roll playhead - only visible in Pattern mode. In Song mode
    // the playhead lives on the Builder, so we push -1 here to hide it.
    if (mPlayHead && mPianoRoll)
    {
        const bool songMode = mProcessor.isSongMode();
        mPianoRoll->setPlayheadBeat(songMode ? -1.0 : mPlayHead->getCurrentBeat());
    }

    // Refresh piano roll data pointer when pattern changes
    if (mPianoRoll)
    {
        mPianoRoll->setData(&mPM.currentPattern().layerRoll[mPageIndex]);
        // C.5b: refresh pattern TS too so bar lines re-spaced for new pattern.
        mPianoRoll->setTimeSignature(mPM.currentPattern().tsNum, mPM.currentPattern().tsDen);
    }
}

// ── Component overrides ───────────────────────────────────────────────────────
void LayersPage::paint(Graphics& g)
{
    g.fillAll(VC::Bg);

    // Show placeholder text in Player tab before engine is selected
    if (mActiveTab == 0 && !mEngineLocked)
    {
        g.setColour(VC::TextDim);
        g.setFont(Font(15.f));
        auto area = getLocalBounds().withTrimmedTop(40);
        g.drawText("Select engine to begin", area, Justification::centred);
    }
}

void LayersPage::resized()
{
    auto b = getLocalBounds();

    // ── Content fills full bounds (tabs live in PageMenuBar above) ────────────
    if (mPlayerTab)
    {
        mPlayerTab->setBounds(b);

        // Engine selector row: label + combo across the top
        constexpr int kRowH   = 28;
        constexpr int kPad    = 6;
        constexpr int kLabelW = 52;
        auto pb = mPlayerTab->getLocalBounds();
        auto row = pb.removeFromTop(kRowH + kPad * 2).reduced(kPad);
        if (mEngineLabel) mEngineLabel->setBounds(row.removeFromLeft(kLabelW));
        if (mEngineCombo) mEngineCombo->setBounds(row.removeFromLeft(160));

        if (mEngineEditor && pb.getHeight() > 0)
            mEngineEditor->setBounds(pb);
    }

    if (mPianoRoll) mPianoRoll->setBounds(b);
}

// ─────────────────────────────────────────────────────────────────────────────
// Context label sync
// ─────────────────────────────────────────────────────────────────────────────
void LayersPage::setTabName(const juce::String& name)
{
    mTabName = name;
    refreshPianoRollContextLabel();
}

void LayersPage::refreshPianoRollContextLabel()
{
    if (!mPianoRoll) return;
    const juce::String engine = mEngineType.isEmpty() ? juce::String("(no engine)")
                                                      : mEngineType;
    mPianoRoll->setContextLabel(mTabName + " - " + engine);
}

// ─────────────────────────────────────────────────────────────────────────────
// D1.4-fix (c) - per-layer right-click context menu + save / delete
// ─────────────────────────────────────────────────────────────────────────────
// G-6 (2026-04-29): sLayerClipboard removed.  Was used by Copy/Paste menu
// items which are now dropped - Duplicate covers all state-clone use cases.

static juce::File layerEnginePresetsDir (const juce::String& engineName)
{
    // Documents/BaySickDAW/Presets/<EngineName>/My Presets
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("BaySickDAW")
               .getChildFile ("Presets")
               .getChildFile (engineName)
               .getChildFile ("My Presets");
}

// 2026-04-25 (Load Preset support).
// Top-level engine presets directory - siblings under <Documents>/BaySickDAW/
// Presets/<engineName>/ are factory bundles; "My Presets" is user-saved.
static juce::File layerEngineRootPresetsDir (const juce::String& engineName)
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("BaySickDAW")
               .getChildFile ("Presets")
               .getChildFile (engineName);
}

// Engine paramPrefix accessor - returns this page's local prefix for the
// currently-loaded engine.
static juce::String layerEngineLocalPrefix (juce::AudioProcessor* proc)
{
    if (auto* bss = dynamic_cast<BaySickSynthProcessor*>(proc))  return bss->getParamPrefix();
    if (auto* bsp = dynamic_cast<VibePlayerProcessor*>(proc))    return bsp->getParamPrefix();
    if (auto* h   = dynamic_cast<HarmlessProcessor*>(proc))      return h  ->getParamPrefix();
    return {};
}

// Apply a ValueTree (raw apvts state) to whichever engine is loaded.
static void layerApplyApvtsTree (juce::AudioProcessor* proc, const juce::ValueTree& vt)
{
    if (auto* bss = dynamic_cast<BaySickSynthProcessor*>(proc))  { bss->apvts.replaceState(vt); return; }
    if (auto* bsp = dynamic_cast<VibePlayerProcessor*>(proc))    { bsp->apvts.replaceState(vt); return; }
    if (auto* h   = dynamic_cast<HarmlessProcessor*>(proc))      { h  ->apvts.replaceState(vt); return; }
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

    auto xmlEl = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
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

void LayersPage::showContextMenu (juce::Component* anchor)
{
    if (anchor == nullptr) return;

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
    constexpr int kIdDelete    = 99;

    juce::PopupMenu menu;
    menu.addItem (kIdLock, "Lock Layer", true, mLocked);

    // Polyphony - engine-specific param.
    //   BaySickSynth → tk_lay_N_bss_voiceMode  (0=poly, 1=mono)
    //   BaySickPlayer → tk_lay_N_bsp_voiceCap   (1=mono, 8=poly)
    //   Harmless     → polyphonic-only (n/a)
    {
        bool isMono = false;
        bool canToggle = false;
        const auto trackPrefix = juce::String ("tk_lay_") + juce::String (mPageIndex) + "_";
        if (mEngineType == "BaySickSynth")
        {
            // voiceMode raw: 0=Poly, 1=Mono, 2=Lead, 3=Legato (4-choice)
            if (auto* p = mProcessor.apvts.getRawParameterValue (trackPrefix + "bss_voiceMode"))
                isMono = (int) std::round (p->load()) >= 1;
            canToggle = true;
        }
        else if (mEngineType == "BaySickPlayer")
        {
            if (auto* p = mProcessor.apvts.getRawParameterValue (trackPrefix + "bsp_voiceCap"))
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
    menu.addItem (kIdDelete, "Delete Layer", ! mLocked);   // locked layers can't be deleted

    juce::Component::SafePointer<LayersPage> safeThis (this);
    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (anchor),
        [safeThis, presetXmls = std::move (presetXmls),
         kIdLock, kIdPolyphony, kIdRename, kIdDuplicate,
         kIdChokeBase, kIdSaveAs, kIdLoadPresetBase, kIdDelete] (int r) mutable
        {
            if (! safeThis || r <= 0) return;
            auto* lp = safeThis.getComponent();

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
                if (auto* p = lp->mProcessor.apvts.getParameter (prefix))
                {
                    const auto& range = p->getNormalisableRange();
                    p->setValueNotifyingHost (range.convertTo0to1 ((float) newGroup));
                }
                return;
            }

            if      (r == kIdLock) lp->setLocked (! lp->mLocked);
            else if (r == kIdPolyphony)
            {
                const auto trackPrefix = juce::String ("tk_lay_") + juce::String (lp->mPageIndex) + "_";
                if (lp->mEngineType == "BaySickSynth")
                {
                    // 4-choice voiceMode: toggle Poly(0) <-> Mono(1) only.
                    if (auto* p = lp->mProcessor.apvts.getParameter (trackPrefix + "bss_voiceMode"))
                    {
                        const auto& range = p->getNormalisableRange();
                        const int curRaw  = (int) std::round (range.convertFrom0to1 (p->getValue()));
                        const int nextRaw = (curRaw == 0) ? 1 : 0;
                        p->setValueNotifyingHost (range.convertTo0to1 ((float) nextRaw));
                    }
                }
                else if (lp->mEngineType == "BaySickPlayer")
                {
                    if (auto* p = lp->mProcessor.apvts.getParameter (trackPrefix + "bsp_voiceCap"))
                    {
                        const auto& range = p->getNormalisableRange();
                        const float curRaw  = range.convertFrom0to1 (p->getValue());
                        const float nextRaw = (curRaw <= 1.5f) ? 8.f : 1.f;
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
            else if (r == kIdSaveAs)    lp->savePatchAs();
            else if (r == kIdDelete)    lp->requestDelete();
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
    auto parsed = juce::XmlDocument::parse (xml);
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
            layerSubstituteEnginePrefixInBinary (mEngineProcessor.get(), mb);
            mEngineProcessor->setStateInformation (mb.getData(), (int) mb.getSize());
        }
    }
    mLocked = lock;
    refreshPianoRollContextLabel();
}

void LayersPage::loadPreset (const juce::File& xml)
{
    if (mEngineProcessor == nullptr || ! xml.existsAsFile()) return;

    auto px = juce::XmlDocument::parse (xml);
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
        if (auto x = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()))
            innerXml = std::move (x);
    }
    else
    {
        innerXml = std::move (px);
    }
    if (! innerXml) return;

    juce::ValueTree loaded = juce::ValueTree::fromXml (*innerXml);
    if (! loaded.isValid()) return;

    const juce::String localPrefix = layerEngineLocalPrefix (mEngineProcessor.get());
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

    layerApplyApvtsTree (mEngineProcessor.get(), loaded);

    // 2026-04-26: BaySickPlayer factory presets carry a sibling <Sample>
    // element pointing at an SFZ in the Core Library.  Resolve and load
    // it here.  Unlike DrumPage we DO NOT call normalizeRootNotes - Layers
    // need to preserve the SFZ's natural keymap for melodic playback.
    if (bspSample)
        if (auto* vp = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor.get()))
        {
            const juce::String kind = bspSample->getStringAttribute ("kind", "none");
            const juce::String pathStr = bspSample->getStringAttribute ("path");
            juce::File path;
            if (pathStr.startsWith ("library:"))
                path = SampleLibrary::getCoreLibraryDir().getChildFile (pathStr.substring (8));
            else
                path = juce::File (pathStr);

            // 2026-05-02: route through the processor wrappers so the
            // sample path lives on apvts.state for project-save replay.
            // No normalizeRoot here -- preserve SFZ keymap for melodic play.
            if      (kind == "sfz"    && path.existsAsFile()) vp->loadSampleSFZ    (path);
            else if (kind == "file"   && path.existsAsFile()) vp->loadSampleFile   (path);
            else if (kind == "folder" && path.isDirectory())  vp->loadSampleFolder (path);
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
            auto name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;
            auto* lp = safeThis.getComponent();
            if (lp->mEngineProcessor == nullptr) return;

            auto dir = layerEnginePresetsDir (lp->mEngineType);
            dir.createDirectory();
            auto file = dir.getChildFile (name + ".xml");

            // Generic save - uses the engine's getStateInformation blob.
            // (Shared format across Harmless / BaySickPlayer / BaySickSynth /
            // BaySickBass.  BaySickPlayer's blob already contains its sample
            // reference because the engine serialises that internally.)
            juce::MemoryBlock mb;
            lp->mEngineProcessor->getStateInformation (mb);
            juce::XmlElement root ("BaySickEnginePreset");
            root.setAttribute ("engine", lp->mEngineType);
            root.setAttribute ("data",   mb.toBase64Encoding());
            root.writeTo (file, {});
            lp->takeStateSnapshot();   // saved patch is the new clean baseline
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
    if (auto* h = dynamic_cast<HarmlessProcessor*>     (mEngineProcessor.get())) h->apvts.state.addListener (this);
    else if (auto* s = dynamic_cast<BaySickSynthProcessor*>(mEngineProcessor.get())) s->apvts.state.addListener (this);
    else if (auto* v = dynamic_cast<VibePlayerProcessor*>  (mEngineProcessor.get())) v->apvts.state.addListener (this);
}

void LayersPage::unsubscribeFromEngineApvtsState()
{
    if (auto* h = dynamic_cast<HarmlessProcessor*>     (mEngineProcessor.get())) h->apvts.state.removeListener (this);
    else if (auto* s = dynamic_cast<BaySickSynthProcessor*>(mEngineProcessor.get())) s->apvts.state.removeListener (this);
    else if (auto* v = dynamic_cast<VibePlayerProcessor*>  (mEngineProcessor.get())) v->apvts.state.removeListener (this);
}

// ─────────────────────────────────────────────────────────────────────────────
// G-7 (2026-04-29): Page Preset save/load (full chain - engine + strip +
// insert rack + post-EQ).  Surfaced via the page menu bar's hamburger ≡.
// ─────────────────────────────────────────────────────────────────────────────
static juce::String layerEnginePrefixOf (juce::AudioProcessor* p)
{
    if (auto* h   = dynamic_cast<HarmlessProcessor*>     (p)) return h  ->getParamPrefix();
    if (auto* s   = dynamic_cast<BaySickSynthProcessor*> (p)) return s  ->getParamPrefix();
    if (auto* v   = dynamic_cast<VibePlayerProcessor*>   (p)) return v  ->getParamPrefix();
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
            if (name.isEmpty()) return;

            auto dir = PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Layer);
            dir.createDirectory();
            auto target = dir.getChildFile (name + ".xml");
            int n = 2;
            while (target.exists())
                target = dir.getChildFile (name + " (" + juce::String (n++) + ").xml");

            const juce::String stripPrefix = "mixer_layer_" + juce::String (safeThis->mPageIndex);
            const juce::String enginePrefix = layerEnginePrefixOf (safeThis->mEngineProcessor.get());

            const juce::String xml = PagePresetIO::exportPagePreset (
                safeThis->mProcessor,
                PagePresetIO::PageKind::Layer,
                safeThis->mPageIndex,
                stripPrefix,
                safeThis->mEngineProcessor.get(),
                safeThis->mEngineType,
                enginePrefix);

            if (xml.isNotEmpty())
                target.replaceWithText (xml);

            safeThis->takeStateSnapshot();   // saved page is the new clean baseline

            // G-7: chain the continuation (e.g. requestDelete fires fireDelete
            // here so Save Page Preset & Delete completes the delete after the
            // user has finalised the save name).
            if (onSaved) onSaved();
        }), false);
}

void LayersPage::loadPagePreset (const juce::File& xml)
{
    if (! xml.existsAsFile()) return;

    // First peek the engineType so we can swap engines if needed.
    const juce::String savedEngineType = PagePresetIO::peekEngineType (xml);
    if (savedEngineType.isNotEmpty() && savedEngineType != mEngineType)
        selectEngine (savedEngineType);

    const juce::String stripPrefix = "mixer_layer_" + juce::String (mPageIndex);
    const juce::String enginePrefix = layerEnginePrefixOf (mEngineProcessor.get());

    // Layer pages don't have secondary buses, so isChannelActive can return
    // true unconditionally - bus fallback is a no-op.
    auto noFallback = [] (int) { return true; };

    PagePresetIO::importPagePreset (mProcessor,
                                     PagePresetIO::PageKind::Layer,
                                     mPageIndex,
                                     stripPrefix,
                                     mEngineProcessor.get(),
                                     enginePrefix,
                                     noFallback,
                                     xml.loadFileAsString());

    const juce::String newName = xml.getFileNameWithoutExtension();
    setTabName (newName);
    if (onSoundNameChanged) onSoundNameChanged (newName);
    takeStateSnapshot();
}

void LayersPage::showPageActionsMenu (juce::Component* anchor)
{
    constexpr int kIdSavePagePreset = 100;
    constexpr int kIdLoadBase       = 1000;

    juce::PopupMenu menu;
    menu.addItem (kIdSavePagePreset, "Save Page Preset As...",
                  mEngineProcessor != nullptr);

    juce::Array<juce::File> presetXmls;
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
                const int id = kIdLoadBase + presetXmls.size();
                presetXmls.add (f);
                loadSub.addItem (id, f.getFileNameWithoutExtension());
            }
        }
        if (presetXmls.isEmpty())
            loadSub.addItem (-1, "(no page presets saved)", false, false);
        menu.addSubMenu ("Load Page Preset", loadSub);
    }

    juce::Component::SafePointer<LayersPage> safeThis (this);
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
