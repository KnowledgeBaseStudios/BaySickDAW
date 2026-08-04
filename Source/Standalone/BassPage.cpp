#include "BassPage.h"
#include "../AppPaths.h"
#include "../BaySickBass/BaySickBassProcessor.h"
#include "../BaySickBass/BaySickBassEditor.h"
#include "../Harmless/HarmlessProcessor.h"
#include "../Harmless/HarmlessEditor.h"
#include "../VibePlayer/VibePlayerProcessor.h"
#include "../VibePlayer/VibePlayerEditor.h"
#include "../SampleLibrary.h"
#include "PagePresetIO.h"
#include "StandaloneEditor.h"
using namespace juce;

// ── Constructor / Destructor ──────────────────────────────────────────────────
BassPage::BassPage(VibeSynthProcessor& p, PatternManager& pm, int pageIndex)
    : mProcessor(p), mPM(pm), mPageIndex(juce::jlimit(0, kMaxBassPages - 1, pageIndex))
{
    mPageColor = VC::BassCol[mPageIndex];

    // QA-ModelShell TS1: the tab is a model object from birth (idempotent;
    // name syncs via setTabName; engine attaches at selectEngine).
    mProcessor.engineRig().addTab (TabKind::Bass, mPageIndex, mTabName);

    buildPlayerTab();
    // 2026-04-26 (step 2 commit 3): Piano Roll lives on PianoRollPage now.
    // mPianoRoll stays null; menu-bar pill redirects via the editor.
    // J-6 EQ unification (2026-05-03): EQ sub-tab removed - pre + post EQ
    // for this insert now live exclusively on the Effects page.

    switchTab(0);
    startTimerHz(24);
}

BassPage::~BassPage()
{
    stopTimer();
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
void BassPage::switchTab(int idx)
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
void BassPage::buildPlayerTab()
{
    // QA-Layout T2 (L4): the engine picker row is gone -- the engine is
    // chosen at the "+" menu before the page exists, and the old context
    // menu lives on the title strip's Menu dropdown (showPageActionsMenu).
    mPlayerTab = std::make_unique<Component>();
    addAndMakeVisible(*mPlayerTab);
}

void BassPage::buildPianoRollTab()
{
    mPianoRoll = std::make_unique<PianoRollContainer>();
    mPianoRoll->setData(&mPM.currentPattern().bassRoll[mPageIndex]);
    // C.5b: pattern's intrinsic TS drives the piano roll's bar-line spacing.
    mPianoRoll->setTimeSignature(mPM.currentPattern().tsNum, mPM.currentPattern().tsDen);
    mPianoRoll->setNoteColor(mPageColor);
    addAndMakeVisible(*mPianoRoll);

    if (mTabName.isEmpty())
        mTabName = "Bass " + juce::String(mPageIndex);
    refreshPianoRollContextLabel();
}

// J-6 EQ unification (2026-05-03): buildEQTab removed - pre + post EQ now
// live exclusively on the Effects page (mixer_bass_<N>_preeq_* / mixer_bass_<N>_*).

// ── PlayHead ──────────────────────────────────────────────────────────────────
void BassPage::setPlayHead(StandalonePlayHead* ph)
{
    mPlayHead = ph;
    if (mPianoRoll)
        mPianoRoll->onSeek = [ph](double b) { if (ph) ph->seekTo(b); };
}

void BassPage::setUndoContext(const UndoContext& ctx)
{
    if (mPianoRoll)
    {
        mPianoRoll->setUndoContext(ctx);
        if (ctx.showHistory) mPianoRoll->onShowHistoryWindow = ctx.showHistory;
    }
}

// ── Engine selection ──────────────────────────────────────────────────────────
void BassPage::selectEngine(const juce::String& engineName)
{
    if (mEngineLocked) return;

    HeavyOperationOverlay::ScopedOp busy (StandaloneEditor::busyOverlayFor (this),
                                          "Loading " + engineName + "...", true);

    mEngineType   = engineName;
    mEngineLocked = true;
    refreshPianoRollContextLabel();

    // View teardown only -- any previous engine is rig-owned.
    if (mEngineEditor && mPlayerTab)
        mPlayerTab->removeChildComponent(mEngineEditor.get());
    mEngineEditor.reset();
    mEngineProcessor = nullptr;

    // QA-ModelShell TS1: the model constructs, prepares, and registers the
    // engine (mixer-strip params + InsertNode + render task included).  This
    // page keeps a non-owning view pointer and builds the editor.
    auto& rig = mProcessor.engineRig();
    rig.addTab (TabKind::Bass, mPageIndex, mTabName);
    mEngineProcessor = rig.setEngineType (TabKind::Bass, mPageIndex, engineName);
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
        else if (auto* be = dynamic_cast<BaySickBassEditor*>   (mEngineEditor.get())) be->onPatchLoaded = onPatch;
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
            if (auto* b = dynamic_cast<BaySickBassProcessor*>(mEngineProcessor))
                b->auditionNote(midiNote);
            else if (auto* h = dynamic_cast<HarmlessProcessor*>(mEngineProcessor))
                h->auditionNote(midiNote);
            else if (auto* v = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor))
                v->auditionNote(midiNote);
        };
        mPianoRoll->onNoteAuditionOn = [this](int midiNote)
        {
            if (auto* b = dynamic_cast<BaySickBassProcessor*>(mEngineProcessor))
                b->auditionNoteOn(midiNote);
            else if (auto* h = dynamic_cast<HarmlessProcessor*>(mEngineProcessor))
                h->auditionNoteOn(midiNote);
            else if (auto* v = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor))
                v->auditionNoteOn(midiNote);
        };
        mPianoRoll->onNoteAuditionOff = [this](int midiNote)
        {
            if (auto* b = dynamic_cast<BaySickBassProcessor*>(mEngineProcessor))
                b->auditionNoteOff(midiNote);
            else if (auto* h = dynamic_cast<HarmlessProcessor*>(mEngineProcessor))
                h->auditionNoteOff(midiNote);
            else if (auto* v = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor))
                v->auditionNoteOff(midiNote);
        };
    }

    if (mEngineEditor && mPlayerTab)
        mPlayerTab->addAndMakeVisible(*mEngineEditor);

    // J-6 EQ unification (2026-05-03): page-level EQ display removed; pre-rack
    // EQ is now bound exclusively by EffectsPage (mixer_bass_<N>_preeq_*).
    juce::MessageManager::callAsync([this] { if (isShowing()) resized(); });

    // Notify StandaloneEditor so it can create the mixer channel strip
    if (onEngineSelected) onEngineSelected();

    // D2: subscribe to engine apvts.state so a preset load via the engine
    // editor refreshes the dirty-snapshot.  Initial snapshot now.
    subscribeToEngineApvtsState();
    takeStateSnapshot();
}

// ── Timer ─────────────────────────────────────────────────────────────────────
void BassPage::timerCallback()
{
    // J-6 EQ unification (2026-05-03): page-level EQ display removed; the
    // Effects-page Pre EQ tab handles its own syncFromDSP polling now.

    // Update piano roll playhead - hidden in Song mode (playhead lives on Builder)
    if (mPlayHead && mPianoRoll)
    {
        const bool songMode = mProcessor.isSongMode();
        mPianoRoll->setPlayheadBeat(songMode ? -1.0 : mPlayHead->getCurrentBeat());
    }

    // Refresh piano roll data pointer when pattern changes
    if (mPianoRoll)
    {
        mPianoRoll->setData(&mPM.currentPattern().bassRoll[mPageIndex]);
        mPianoRoll->setTimeSignature(mPM.currentPattern().tsNum, mPM.currentPattern().tsDen);
    }
}

// ── Component overrides ───────────────────────────────────────────────────────
void BassPage::paint(Graphics& g)
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

void BassPage::resized()
{
    auto b = getLocalBounds();

    // ── Content fills full bounds (tabs live in PageMenuBar above) ────────────
    if (mPlayerTab)
    {
        mPlayerTab->setBounds(b);
        if (mEngineEditor && mPlayerTab->getHeight() > 0)
            mEngineEditor->setBounds(mPlayerTab->getLocalBounds());
    }

    if (mPianoRoll) mPianoRoll->setBounds(b);
}

// ─────────────────────────────────────────────────────────────────────────────
// Context label sync
// ─────────────────────────────────────────────────────────────────────────────
void BassPage::setTabName(const juce::String& name)
{
    mTabName = name;
    // QA-ModelShell TS1: every rename path funnels through here -- the one
    // sync point for the model tab's name.
    mProcessor.engineRig().renameTab (TabKind::Bass, mPageIndex, name);
    refreshPianoRollContextLabel();
}

void BassPage::refreshPianoRollContextLabel()
{
    if (!mPianoRoll) return;
    const juce::String engine = mEngineType.isEmpty() ? juce::String("(no engine)")
                                                      : mEngineType;
    mPianoRoll->setContextLabel(mTabName + " - " + engine);
}

// ─────────────────────────────────────────────────────────────────────────────
// D1.4-fix (c) - per-bass right-click context menu + save / delete
// (mirror of LayersPage; see that file for design notes)
// ─────────────────────────────────────────────────────────────────────────────
// G-6 (2026-04-29): sBassClipboard removed (Copy/Paste menu items dropped).

static juce::File bassEnginePresetsDir (const juce::String& engineName)
{
    return AppPaths::appRoot()
               .getChildFile ("Presets")
               .getChildFile (engineName)
               .getChildFile ("My Presets");
}

// 2026-04-25 (Load Preset support).
// Top-level engine presets directory - siblings under <Documents>/BaySickDAW/
// Presets/<engineName>/ are factory bundles; "My Presets" is user-saved.
static juce::File bassEngineRootPresetsDir (const juce::String& engineName)
{
    return AppPaths::appRoot()
               .getChildFile ("Presets")
               .getChildFile (engineName);
}

// Engine paramPrefix accessor - returns this page's local prefix for the
// currently-loaded engine.  Used by loadPreset to rewrite preset XML's param
// IDs so they bind to this tab's track regardless of where saved.
static juce::String bassEngineLocalPrefix (juce::AudioProcessor* proc)
{
    if (auto* bsb = dynamic_cast<BaySickBassProcessor*>(proc))   return bsb->getParamPrefix();
    if (auto* bsp = dynamic_cast<VibePlayerProcessor*>(proc))    return bsp->getParamPrefix();
    if (auto* h   = dynamic_cast<HarmlessProcessor*>(proc))      return h  ->getParamPrefix();
    return {};
}

// Apply a ValueTree (raw apvts state) to whichever engine is loaded.  Caller
// is responsible for any prefix substitution before calling.
static void bassApplyApvtsTree (juce::AudioProcessor* proc, const juce::ValueTree& vt)
{
    if (auto* bsb = dynamic_cast<BaySickBassProcessor*>(proc))   { bsb->apvts.replaceState(vt); return; }
    if (auto* bsp = dynamic_cast<VibePlayerProcessor*>(proc))    { bsp->apvts.replaceState(vt); return; }
    if (auto* h   = dynamic_cast<HarmlessProcessor*>(proc))      { h  ->apvts.replaceState(vt); return; }
}

// G-6 (2026-04-29): rewrite the binary state's PARAM ids so the source page's
// APVTS prefix (tk_bass_<srcIdx>_<engineTag>_) becomes this destination page's
// prefix.  Without this, importBassState() silently drops every param because
// setStateInformation matches by id.  Mirrors the F-2 fix in loadPreset().
static void bassSubstituteEnginePrefixInBinary (juce::AudioProcessor* proc,
                                                juce::MemoryBlock& mb)
{
    if (proc == nullptr || mb.getSize() == 0) return;

    const juce::String localPrefix = bassEngineLocalPrefix (proc);
    if (localPrefix.isEmpty()) return;

    const int trailingUnder = localPrefix.length() - 1;
    if (trailingUnder < 1) return;
    const int tagStart = localPrefix.substring (0, trailingUnder).lastIndexOfChar ('_');
    if (tagStart < 0) return;
    const juce::String engineTagWithUnders = localPrefix.substring (tagStart, trailingUnder + 1);

    auto xmlEl = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
    if (! xmlEl) return;

    juce::ValueTree loaded = juce::ValueTree::fromXml (*xmlEl);
    if (! loaded.isValid()) return;

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

    if (loadedPrefix.isEmpty() || loadedPrefix == localPrefix) return;

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
// SampleLibrary::isDrumPack - used for BaySickPlayer in Bass (melodic) context.
static void addBassPresetDirToMenu (juce::PopupMenu& menu,
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
        addBassPresetDirToMenu (sub, child, kPresetBase, presetXmls, false);
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
void BassPage::showPageActionsMenu (juce::Component* anchor)
{
    constexpr int kIdLock      = 1;
    constexpr int kIdPolyphony = 2;
    constexpr int kIdRename    = 5;
    constexpr int kIdDuplicate = 12;
    // G-6 (2026-04-29): Copy/Paste menu items dropped.
    constexpr int kIdSaveAs    = 20;
    // D3: choke-group submenu - 200 = None, 201..216 = groups 1..16.
    constexpr int kIdChokeBase = 200;
    // 2026-04-25: Load Preset submenu - 500 + i indexes into presetXmls[].
    constexpr int kIdLoadPresetBase = 500;
    constexpr int kIdSavePagePreset = 100;
    // Page-preset load submenu - 1000 + i indexes into pagePresetXmls[].
    constexpr int kIdLoadBase  = 1000;
    constexpr int kIdDelete    = 99;

    juce::PopupMenu menu;
    menu.addItem (kIdLock, "Lock Bass", true, mLocked);

    // Polyphony - engine-specific param.
    //   BaySickBass   → tk_bas_N_bsb_voiceMode  (0=poly, 1=mono)
    //   BaySickPlayer → tk_bas_N_bsp_voiceCap   (1=mono, 8=poly)
    //   Harmless      → polyphonic-only (n/a)
    {
        bool isMono = false;
        bool canToggle = false;
        const auto trackPrefix = juce::String ("tk_bas_") + juce::String (mPageIndex) + "_";
        if (mEngineType == "BaySickBass")
        {
            // voiceMode raw: 0=Poly, 1=Mono, 2=Lead, 3=Legato (4-choice)
            if (auto* p = mProcessor.apvts.getRawParameterValue (trackPrefix + "bsb_voiceMode"))
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
    menu.addItem (kIdDuplicate, "Duplicate Bass (new tab)", ! mEngineType.isEmpty());

    menu.addSeparator();
    // D3: Choke Group submenu - global cross-engine cut bus.
    {
        const juce::String prefix = "mixer_bass_" + juce::String (mPageIndex) + "_chokeGroup";
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
        const auto root = bassEngineRootPresetsDir (mEngineType);
        // 2026-04-26: BaySickPlayer presets include drum subfolders - skip
        // them here (Bass page is melodic context).  Other engines no-op.
        const bool skipDrums = (mEngineType == "BaySickPlayer");
        if (root.isDirectory() && ! mEngineType.isEmpty())
            addBassPresetDirToMenu (loadSub, root, kIdLoadPresetBase, presetXmls, skipDrums);
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
        const auto root = PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Bass);
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
    menu.addItem (kIdDelete, "Delete Bass", ! mLocked);   // locked bass tabs can't be deleted

    juce::Component::SafePointer<BassPage> safeThis (this);
    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (anchor),
        [safeThis, presetXmls = std::move (presetXmls),
         pagePresetXmls = std::move (pagePresetXmls),
         kIdLock, kIdPolyphony, kIdRename, kIdDuplicate, kIdChokeBase,
         kIdSaveAs, kIdLoadPresetBase, kIdSavePagePreset, kIdLoadBase,
         kIdDelete] (int r) mutable
        {
            if (! safeThis || r <= 0) return;
            auto* bp = safeThis.getComponent();

            // Page-preset load submenu (1000 + i -> pagePresetXmls[i]).
            if (r >= kIdLoadBase && r < kIdLoadBase + pagePresetXmls.size())
            {
                bp->loadPagePreset (pagePresetXmls[r - kIdLoadBase]);
                return;
            }

            // Load Preset submenu (500 + i → presetXmls[i]).
            if (r >= kIdLoadPresetBase && r < kIdLoadPresetBase + presetXmls.size())
            {
                bp->loadPreset (presetXmls[r - kIdLoadPresetBase]);
                return;
            }

            // D3: choke-group submenu (200..216 → 0..16).
            if (r >= kIdChokeBase && r <= kIdChokeBase + 16)
            {
                const int newGroup = r - kIdChokeBase;
                const juce::String prefix = "mixer_bass_" + juce::String (bp->mPageIndex) + "_chokeGroup";
                if (auto* p = bp->mProcessor.apvts.getParameter (prefix))
                {
                    const auto& range = p->getNormalisableRange();
                    p->setValueNotifyingHost (range.convertTo0to1 ((float) newGroup));
                }
                return;
            }

            if      (r == kIdLock) bp->setLocked (! bp->mLocked);
            else if (r == kIdPolyphony)
            {
                const auto trackPrefix = juce::String ("tk_bas_") + juce::String (bp->mPageIndex) + "_";
                if (bp->mEngineType == "BaySickBass")
                {
                    // 4-choice voiceMode: toggle Poly(0) <-> Mono(1) only.
                    if (auto* p = bp->mProcessor.apvts.getParameter (trackPrefix + "bsb_voiceMode"))
                    {
                        const auto& range = p->getNormalisableRange();
                        const int curRaw  = (int) std::round (range.convertFrom0to1 (p->getValue()));
                        const int nextRaw = (curRaw == 0) ? 1 : 0;
                        p->setValueNotifyingHost (range.convertTo0to1 ((float) nextRaw));
                    }
                }
                else if (bp->mEngineType == "BaySickPlayer")
                {
                    if (auto* p = bp->mProcessor.apvts.getParameter (trackPrefix + "bsp_voiceCap"))
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
                if (bp->onRenameRequested) bp->onRenameRequested();
            }
            // G-6: kIdCopy / kIdPaste handlers removed.
            else if (r == kIdDuplicate)
            {
                const auto state = bp->exportBassState();
                if (state.isNotEmpty() && bp->onDuplicateRequested)
                    bp->onDuplicateRequested (state);
            }
            else if (r == kIdSaveAs)         bp->savePatchAs();
            else if (r == kIdSavePagePreset) bp->savePagePreset();
            else if (r == kIdDelete)         bp->requestDelete();
        });
}

juce::String BassPage::exportBassState() const
{
    if (mEngineType.isEmpty() || mEngineProcessor == nullptr) return {};
    juce::XmlElement el ("BassPageState");
    el.setAttribute ("engine", mEngineType);
    el.setAttribute ("locked", mLocked ? 1 : 0);
    juce::MemoryBlock mb;
    mEngineProcessor->getStateInformation (mb);
    el.setAttribute ("data", mb.toBase64Encoding());
    return el.toString (juce::XmlElement::TextFormat().singleLine());
}

void BassPage::importBassState (const juce::String& xml)
{
    if (xml.isEmpty() || mLocked) return;
    auto parsed = juce::XmlDocument::parse (xml);
    if (! parsed || ! parsed->hasTagName ("BassPageState")) return;

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
            // this every param is dropped by setStateInformation's id match.
            bassSubstituteEnginePrefixInBinary (mEngineProcessor, mb);
            mEngineProcessor->setStateInformation (mb.getData(), (int) mb.getSize());
        }
    }
    mLocked = lock;
    refreshPianoRollContextLabel();
}

void BassPage::loadPreset (const juce::File& xml)
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
    // Stash the Sample element here; loaded after apvts is applied below.
    std::unique_ptr<juce::XmlElement> bspSample;
    if (px->hasTagName ("BaySickPlayerState"))
        if (auto* inner = px->getChildByName ("BaySickPlayerState"))
        {
            if (auto* s = px->getChildByName ("Sample"))
                bspSample = std::make_unique<juce::XmlElement> (*s);
            px = std::make_unique<juce::XmlElement> (*inner);
        }

    // Extract the inner apvts XML.  Two formats supported:
    //   1. Wrapped <BaySickEnginePreset engine="X" data="base64-of-getStateInformation"/>
    //      (savePatchAs output).  Decode + getXmlFromBinary to get the inner apvts XML.
    //   2. Raw apvts XML (root tag = engine's apvts.state.getType()).
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
        innerXml = std::move (px);   // assume raw apvts XML
    }

    if (! innerXml) return;

    juce::ValueTree loaded = juce::ValueTree::fromXml (*innerXml);
    if (! loaded.isValid()) return;

    // Prefix substitution.  Find any PARAM child whose id contains the
    // current engine's tag (e.g. "_bsb_") and extract its prefix; rewrite
    // every PARAM id whose prefix matches.  Skip if loaded prefix == local.
    const juce::String localPrefix = bassEngineLocalPrefix (mEngineProcessor);
    if (localPrefix.isEmpty()) return;

    // F-2 fix (2026-04-26): localPrefix format is `tk_<row>_<idx>_<engineTag>_`
    // (e.g. "tk_bas_1_bsb_").  The earlier 3-segment-style computation
    // returned the index segment ("_1_") instead of the engine tag ("_bsb_"),
    // so the substring search against loaded PARAM ids - which carry a
    // different page index ("_0_" from the saved slot) - never matched and
    // the substitution silently no-op'd.  Engine sounded default after load.
    const int trailingUnder = localPrefix.length() - 1;
    if (trailingUnder < 1) return;
    const int tagStart = localPrefix.substring (0, trailingUnder).lastIndexOfChar ('_');
    if (tagStart < 0) return;
    const juce::String engineTagWithUnders = localPrefix.substring (tagStart, trailingUnder + 1);  // "_bsb_"

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

    bassApplyApvtsTree (mEngineProcessor, loaded);

    // 2026-04-26: BaySickPlayer factory presets carry a sibling <Sample>
    // element pointing at an SFZ in the Core Library.  Resolve and load
    // it here.  No normalizeRootNotes - preserves SFZ's natural keymap
    // for melodic playback (only DrumPage normalizes to 60).
    if (bspSample)
        if (auto* vp = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor))
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

void BassPage::savePatchAs()
{
    if (mEngineProcessor == nullptr || mEngineType.isEmpty()) return;

    auto* aw = new juce::AlertWindow ("Save Patch As",
                                       "Enter a name to save this bass as a preset:",
                                       juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", "My Patch");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<BassPage> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, aw] (int r)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (r != 1 || ! safeThis) return;
            auto name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;
            auto* bp = safeThis.getComponent();
            if (bp->mEngineProcessor == nullptr) return;

            auto dir = bassEnginePresetsDir (bp->mEngineType);
            dir.createDirectory();
            auto file = dir.getChildFile (name + ".xml");

            juce::MemoryBlock mb;
            bp->mEngineProcessor->getStateInformation (mb);
            juce::XmlElement root ("BaySickEnginePreset");
            root.setAttribute ("engine", bp->mEngineType);
            root.setAttribute ("data",   mb.toBase64Encoding());
            root.writeTo (file, {});
            bp->takeStateSnapshot();   // saved patch is the new clean baseline
        }), false);
}

void BassPage::requestDelete()
{
    // G-7 (2026-04-29): close-prompt with dirty-aware buttons (matches LayersPage).
    juce::Component::SafePointer<BassPage> safeThis (this);
    auto fireDelete = [safeThis] {
        if (auto* bp = safeThis.getComponent())
            if (bp->onDeleteRequested) bp->onDeleteRequested();
    };

    const juce::String warning =
        "Deleting this bass removes its Player, Mixer Strip, "
        "Effects Rack, and Piano Roll.";

    if (mEngineProcessor != nullptr && isPatchDirty())
    {
        const juce::String dirtyWarning = warning
            + "\n\n\"Save Page Preset & Delete\" writes the entire page state "
              "(engine + EQ + effects rack + strip settings) to disk first, "
              "then deletes the tab.";

        auto* aw = new juce::AlertWindow (
            "Delete Bass", dirtyWarning, juce::AlertWindow::QuestionIcon);
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
        "Delete Bass", warning, juce::AlertWindow::WarningIcon);
    aw->addButton ("Delete", 1);
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [aw, fireDelete] (int r)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (r == 1) fireDelete();
        }), false);
}

// ─────────────────────────────────────────────────────────────────────────────
// G-7 (2026-04-29): Page Preset save/load (full chain).
// ─────────────────────────────────────────────────────────────────────────────
static juce::String bassEnginePrefixOf (juce::AudioProcessor* p)
{
    if (auto* bsb = dynamic_cast<BaySickBassProcessor*> (p))   return bsb->getParamPrefix();
    if (auto* h   = dynamic_cast<HarmlessProcessor*>    (p))   return h  ->getParamPrefix();
    if (auto* v   = dynamic_cast<VibePlayerProcessor*>  (p))   return v  ->getParamPrefix();
    return {};
}

void BassPage::savePagePreset (std::function<void()> onSaved)
{
    auto* aw = new juce::AlertWindow ("Save Page Preset",
                                       "Enter a name for this bass page preset:",
                                       juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", "My Bass");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<BassPage> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, aw, onSaved] (int r)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (r != 1 || ! safeThis) return;

            const juce::String name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;

            auto dir = PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Bass);
            dir.createDirectory();
            auto target = dir.getChildFile (name + ".xml");
            int n = 2;
            while (target.exists())
                target = dir.getChildFile (name + " (" + juce::String (n++) + ").xml");

            const juce::String stripPrefix = "mixer_bass_" + juce::String (safeThis->mPageIndex);
            const juce::String enginePrefix = bassEnginePrefixOf (safeThis->mEngineProcessor);

            const juce::String xml = PagePresetIO::exportPagePreset (
                safeThis->mProcessor,
                PagePresetIO::PageKind::Bass,
                safeThis->mPageIndex,
                stripPrefix,
                safeThis->mEngineProcessor,
                safeThis->mEngineType,
                enginePrefix);

            if (xml.isNotEmpty())
                target.replaceWithText (xml);

            safeThis->takeStateSnapshot();
            if (onSaved) onSaved();   // G-7: chain delete after save completes
        }), false);
}

void BassPage::loadPagePreset (const juce::File& xml)
{
    if (! xml.existsAsFile()) return;

    const juce::String savedEngineType = PagePresetIO::peekEngineType (xml);
    if (savedEngineType.isNotEmpty() && savedEngineType != mEngineType)
        selectEngine (savedEngineType);

    const juce::String stripPrefix = "mixer_bass_" + juce::String (mPageIndex);
    const juce::String enginePrefix = bassEnginePrefixOf (mEngineProcessor);
    auto noFallback = [] (int) { return true; };

    PagePresetIO::importPagePreset (mProcessor,
                                     PagePresetIO::PageKind::Bass,
                                     mPageIndex,
                                     stripPrefix,
                                     mEngineProcessor,
                                     enginePrefix,
                                     noFallback,
                                     xml.loadFileAsString());

    const juce::String newName = xml.getFileNameWithoutExtension();
    setTabName (newName);
    if (onSoundNameChanged) onSoundNameChanged (newName);
    takeStateSnapshot();
}

// ── D2 lock + dirty-snapshot helpers ─────────────────────────────────────────
void BassPage::setLocked (bool l)
{
    if (mLocked == l) return;
    mLocked = l;
    if (onLockChanged) onLockChanged();
}

void BassPage::takeStateSnapshot()
{
    mLoadedStateSnapshot.reset();
    if (mEngineProcessor)
        mEngineProcessor->getStateInformation (mLoadedStateSnapshot);
}

bool BassPage::isPatchDirty() const
{
    if (mEngineProcessor == nullptr) return false;
    if (mLoadedStateSnapshot.getSize() == 0) return false;
    juce::MemoryBlock current;
    mEngineProcessor->getStateInformation (current);
    return current != mLoadedStateSnapshot;
}

void BassPage::valueTreeRedirected (juce::ValueTree& tree)
{
    juce::ignoreUnused (tree);
    takeStateSnapshot();
}

void BassPage::subscribeToEngineApvtsState()
{
    if (auto* h = dynamic_cast<HarmlessProcessor*>     (mEngineProcessor)) h->apvts.state.addListener (this);
    else if (auto* b = dynamic_cast<BaySickBassProcessor*> (mEngineProcessor)) b->apvts.state.addListener (this);
    else if (auto* v = dynamic_cast<VibePlayerProcessor*>  (mEngineProcessor)) v->apvts.state.addListener (this);
}

void BassPage::unsubscribeFromEngineApvtsState()
{
    if (auto* h = dynamic_cast<HarmlessProcessor*>     (mEngineProcessor)) h->apvts.state.removeListener (this);
    else if (auto* b = dynamic_cast<BaySickBassProcessor*> (mEngineProcessor)) b->apvts.state.removeListener (this);
    else if (auto* v = dynamic_cast<VibePlayerProcessor*>  (mEngineProcessor)) v->apvts.state.removeListener (this);
}
