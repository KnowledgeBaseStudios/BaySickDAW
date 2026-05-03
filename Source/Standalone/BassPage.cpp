#include "BassPage.h"
#include "../BaySickBass/BaySickBassProcessor.h"
#include "../BaySickBass/BaySickBassEditor.h"
#include "../Harmless/HarmlessProcessor.h"
#include "../Harmless/HarmlessEditor.h"
#include "../VibePlayer/VibePlayerProcessor.h"
#include "../VibePlayer/VibePlayerEditor.h"
#include "../SampleLibrary.h"
#include "PagePresetIO.h"
using namespace juce;

// ── Constructor / Destructor ──────────────────────────────────────────────────
BassPage::BassPage(VibeSynthProcessor& p, PatternManager& pm, int pageIndex)
    : mProcessor(p), mPM(pm), mPageIndex(juce::jlimit(0, kMaxBassPages - 1, pageIndex))
{
    mPageColor = VC::BassCol[mPageIndex];

    buildPlayerTab();
    // 2026-04-26 (step 2 commit 3): Piano Roll lives on PianoRollPage now.
    // mPianoRoll stays null; menu-bar pill redirects via the editor.
    buildEQTab();

    switchTab(0);
    startTimerHz(24);
}

BassPage::~BassPage()
{
    stopTimer();

    // D2: drop the dirty-snapshot listener before the engine processor is freed
    unsubscribeFromEngineApvtsState();

    // Unregister from audio thread first, then wait for any in-flight block to finish
    if (mEngineLocked)
    {
        mProcessor.unregisterBassEngine(mPageIndex);
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
void BassPage::switchTab(int idx)
{
    mActiveTab = idx;
    if (mPlayerTab) mPlayerTab->setVisible(idx == 0);
    if (mPianoRoll) mPianoRoll->setVisible(idx == 1);
    if (mEQTab)     mEQTab    ->setVisible(idx == 2);
    if (idx == 1 && mPianoRoll) mPianoRoll->grabKeyboardFocus();
    resized();
    if (onSubTabChanged) onSubTabChanged(idx);
}

void BassPage::setEQMid(bool showMid)
{
    mEQMidActive = showMid;
    if (mEQDisplay) mEQDisplay->setShowMid(showMid);
}

// ── Tab builders ──────────────────────────────────────────────────────────────
void BassPage::buildPlayerTab()
{
    mPlayerTab = std::make_unique<Component>();
    addAndMakeVisible(*mPlayerTab);

    mEngineLabel = std::make_unique<Label>("", "Engine:");
    mEngineLabel->setColour(Label::textColourId, VC::TextDim);
    mEngineLabel->setFont(Font(13.f));
    mPlayerTab->addAndMakeVisible(*mEngineLabel);

    // D1.4-fix (c): LockableCombo intercepts clicks once engine is locked.
    mEngineCombo = std::make_unique<LockableCombo>();
    mEngineCombo->addItem("Harmless",      1);
    mEngineCombo->addItem("BaySickPlayer", 2);
    mEngineCombo->addItem("BaySickBass",   3);
    mEngineCombo->setTextWhenNothingSelected("Select engine...");
    mEngineCombo->onChange = [this]
    {
        if (mEngineLocked) return;   // post-lock click handled by LockableCombo
        int sel = mEngineCombo->getSelectedId();
        if      (sel == 1) selectEngine("Harmless");
        else if (sel == 2) selectEngine("BaySickPlayer");
        else if (sel == 3) selectEngine("BaySickBass");
    };
    mEngineCombo->onLockedClick = [this] { showContextMenu (mEngineCombo.get()); };
    mPlayerTab->addAndMakeVisible(*mEngineCombo);
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

void BassPage::buildEQTab()
{
    mEQTab     = std::make_unique<Component>();
    mEQDisplay = std::make_unique<ParametricEQDisplay>();

    // §P4.3 B7: display starts unbound; selectEngine() binds to the Bass
    // InsertNode's preEq + mixer_bass_<N>_preeq_* APVTS prefix.
    mEQDisplay->setSampleRate(mProcessor.getSampleRate() > 0.0
                              ? mProcessor.getSampleRate() : 44100.0);
    mEQDisplay->showMidSideToggle(false);  // M/S controlled by external buttons
    // 12f: refresh host PDC after anti-cramping toggle.
    mEQDisplay->onLatencyChanged = [this]
    {
        mProcessor.setLatencySamples(mProcessor.mVibeGraph.updateBusLatencies());
    };
    mEQTab->addAndMakeVisible(*mEQDisplay);
    addAndMakeVisible(*mEQTab);
}

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

    mEngineType   = engineName;
    mEngineLocked = true;
    refreshPianoRollContextLabel();

    // Register lazy APVTS params (engine + pre-rack EQ + effect rack)
    mProcessor.registerParamsForTrack(trackId(), engineName);

    if (mEngineEditor && mPlayerTab)
        mPlayerTab->removeChildComponent(mEngineEditor.get());
    mEngineEditor.reset();
    mEngineProcessor.reset();

    double sr        = mProcessor.getSampleRate() > 0.0 ? mProcessor.getSampleRate() : 44100.0;
    int    blockSize = 512;

    // 2026-04-21: string trackId so engines on different pages don't collide.
    const juce::String trackIdStr = "bas_" + juce::String(mPageIndex);
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
    else if (engineName == "BaySickBass")
    {
        auto* proc = new BaySickBassProcessor(trackIdStr);
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
        else if (auto* be = dynamic_cast<BaySickBassEditor*>   (mEngineEditor.get())) be->onPatchLoaded = onPatch;
        else if (auto* pe = dynamic_cast<VibePlayerEditor*>    (mEngineEditor.get())) pe->onPatchLoaded = onPatch;
    }

    // Wire note audition callback (fires on note draw, pitch drag, and key click)
    if (mPianoRoll)
    {
        mPianoRoll->onNoteAudition = [this](int midiNote)
        {
            if (auto* b = dynamic_cast<BaySickBassProcessor*>(mEngineProcessor.get()))
                b->auditionNote(midiNote);
            else if (auto* h = dynamic_cast<HarmlessProcessor*>(mEngineProcessor.get()))
                h->auditionNote(midiNote);
            else if (auto* v = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor.get()))
                v->auditionNote(midiNote);
        };
        mPianoRoll->onNoteAuditionOn = [this](int midiNote)
        {
            if (auto* b = dynamic_cast<BaySickBassProcessor*>(mEngineProcessor.get()))
                b->auditionNoteOn(midiNote);
            else if (auto* h = dynamic_cast<HarmlessProcessor*>(mEngineProcessor.get()))
                h->auditionNoteOn(midiNote);
            else if (auto* v = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor.get()))
                v->auditionNoteOn(midiNote);
        };
        mPianoRoll->onNoteAuditionOff = [this](int midiNote)
        {
            if (auto* b = dynamic_cast<BaySickBassProcessor*>(mEngineProcessor.get()))
                b->auditionNoteOff(midiNote);
            else if (auto* h = dynamic_cast<HarmlessProcessor*>(mEngineProcessor.get()))
                h->auditionNoteOff(midiNote);
            else if (auto* v = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor.get()))
                v->auditionNoteOff(midiNote);
        };
    }

    if (mEngineEditor && mPlayerTab)
        mPlayerTab->addAndMakeVisible(*mEngineEditor);

    // Register with audio thread for rendering in processBlock.
    // registerBassEngine also creates the Bass InsertNode (which owns preEq).
    mProcessor.registerBassEngine(mPageIndex, mEngineProcessor.get());

    // Lock combo so engine can't be changed
    if (mEngineCombo)
        mEngineCombo->locked = true;   // D1.4-fix (c): hijack future clicks → menu

    // §P4.3 B7: bind EQ display to the Bass InsertNode's preEq + the unified
    // mixer_bass_<N>_preeq_* APVTS prefix.
    if (mEQDisplay)
    {
        if (auto* preEq = mProcessor.mVibeGraph.getInsertPreEQ(
                              VibeGraph::InsertKind::Bass, mPageIndex))
        {
            const juce::String mixerPrefix = "mixer_bass_" + juce::String(mPageIndex);
            mEQDisplay->bindMsDSP(preEq, &mProcessor.apvts,
                                  mixerPrefix + "_preeq_mid_eq",
                                  mixerPrefix + "_preeq_side_eq");
            mEQDisplay->setStripContext(mixerPrefix,
                [](int id){ return MixerChannelIds::friendlyName(id); });
        }
        mEQDisplay->setSampleRate(sr);
    }

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
    // EQ display update — DSP sync happens inside ParametricEQDisplay.
    if (mEQDisplay && mEngineLocked)
        mEQDisplay->syncFromDSP();

    // Update piano roll playhead — hidden in Song mode (playhead lives on Builder)
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

    // Show placeholder text in Player tab before engine is selected
    if (mActiveTab == 0 && !mEngineLocked)
    {
        g.setColour(VC::TextDim);
        g.setFont(Font(15.f));
        auto area = getLocalBounds().withTrimmedTop(40);
        g.drawText("Select engine to begin", area, Justification::centred);
    }
}

void BassPage::resized()
{
    auto b = getLocalBounds();

    // ── Content fills full bounds (tabs live in PageMenuBar above) ────────────
    if (mPlayerTab)
    {
        mPlayerTab->setBounds(b);

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

    if (mEQTab && mEQDisplay)
    {
        mEQTab->setBounds(b);
        mEQDisplay->setBounds(mEQTab->getLocalBounds().reduced(4));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Context label sync
// ─────────────────────────────────────────────────────────────────────────────
void BassPage::setTabName(const juce::String& name)
{
    mTabName = name;
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
// D1.4-fix (c) — per-bass right-click context menu + save / delete
// (mirror of LayersPage; see that file for design notes)
// ─────────────────────────────────────────────────────────────────────────────
// G-6 (2026-04-29): sBassClipboard removed (Copy/Paste menu items dropped).

static juce::File bassEnginePresetsDir (const juce::String& engineName)
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("BaySickDAW")
               .getChildFile ("Presets")
               .getChildFile (engineName)
               .getChildFile ("My Presets");
}

// 2026-04-25 (Load Preset support).
// Top-level engine presets directory — siblings under <Documents>/BaySickDAW/
// Presets/<engineName>/ are factory bundles; "My Presets" is user-saved.
static juce::File bassEngineRootPresetsDir (const juce::String& engineName)
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("BaySickDAW")
               .getChildFile ("Presets")
               .getChildFile (engineName);
}

// Engine paramPrefix accessor — returns this page's local prefix for the
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

// Recursive XML-preset walker — folders become real cascading submenus.
// Matches the sample-picker UX (addLibDirToMenuDP in DrumPage.cpp).
// 2026-04-26: skipDrumFolders filters out top-level folders matching
// SampleLibrary::isDrumPack — used for BaySickPlayer in Bass (melodic) context.
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

void BassPage::showContextMenu (juce::Component* anchor)
{
    if (anchor == nullptr) return;

    constexpr int kIdLock      = 1;
    constexpr int kIdPolyphony = 2;
    constexpr int kIdRename    = 5;
    constexpr int kIdDuplicate = 12;
    // G-6 (2026-04-29): Copy/Paste menu items dropped.
    constexpr int kIdSaveAs    = 20;
    // D3: choke-group submenu — 200 = None, 201..216 = groups 1..16.
    constexpr int kIdChokeBase = 200;
    // 2026-04-25: Load Preset submenu — 500 + i indexes into mPresetXmls[].
    constexpr int kIdLoadPresetBase = 500;
    constexpr int kIdDelete    = 99;

    juce::PopupMenu menu;
    menu.addItem (kIdLock, "Lock Bass", true, mLocked);

    // Polyphony — engine-specific param.
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
    // D3: Choke Group submenu — global cross-engine cut bus.
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

    // ── Load Preset submenu — walks Documents/BaySickDAW/Presets/<engineName>/
    //    with real cascading submenus per folder (matches sample-picker UX).
    juce::Array<juce::File> presetXmls;
    {
        juce::PopupMenu loadSub;
        const auto root = bassEngineRootPresetsDir (mEngineType);
        // 2026-04-26: BaySickPlayer presets include drum subfolders — skip
        // them here (Bass page is melodic context).  Other engines no-op.
        const bool skipDrums = (mEngineType == "BaySickPlayer");
        if (root.isDirectory() && ! mEngineType.isEmpty())
            addBassPresetDirToMenu (loadSub, root, kIdLoadPresetBase, presetXmls, skipDrums);
        if (presetXmls.isEmpty())
            loadSub.addItem (-1, "(no presets installed)", false, false);
        menu.addSubMenu ("Load Preset", loadSub, mEngineProcessor != nullptr);
    }

    menu.addSeparator();
    menu.addItem (kIdDelete, "Delete Bass", ! mLocked);   // locked bass tabs can't be deleted

    juce::Component::SafePointer<BassPage> safeThis (this);
    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (anchor),
        [safeThis, presetXmls = std::move (presetXmls),
         kIdLock, kIdPolyphony, kIdRename, kIdDuplicate,
         kIdChokeBase, kIdSaveAs, kIdLoadPresetBase, kIdDelete] (int r) mutable
        {
            if (! safeThis || r <= 0) return;
            auto* bp = safeThis.getComponent();

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
            else if (r == kIdSaveAs)    bp->savePatchAs();
            else if (r == kIdDelete)    bp->requestDelete();
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
            bassSubstituteEnginePrefixInBinary (mEngineProcessor.get(), mb);
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
    const juce::String localPrefix = bassEngineLocalPrefix (mEngineProcessor.get());
    if (localPrefix.isEmpty()) return;

    // F-2 fix (2026-04-26): localPrefix format is `tk_<row>_<idx>_<engineTag>_`
    // (e.g. "tk_bas_1_bsb_").  The earlier 3-segment-style computation
    // returned the index segment ("_1_") instead of the engine tag ("_bsb_"),
    // so the substring search against loaded PARAM ids — which carry a
    // different page index ("_0_" from the saved slot) — never matched and
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

    bassApplyApvtsTree (mEngineProcessor.get(), loaded);

    // 2026-04-26: BaySickPlayer factory presets carry a sibling <Sample>
    // element pointing at an SFZ in the Core Library.  Resolve and load
    // it here.  No normalizeRootNotes — preserves SFZ's natural keymap
    // for melodic playback (only DrumPage normalizes to 60).
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
    // don't have a separate "sound name" field like DrumPage — tab name +
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
            const juce::String enginePrefix = bassEnginePrefixOf (safeThis->mEngineProcessor.get());

            const juce::String xml = PagePresetIO::exportPagePreset (
                safeThis->mProcessor,
                PagePresetIO::PageKind::Bass,
                safeThis->mPageIndex,
                stripPrefix,
                safeThis->mEngineProcessor.get(),
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
    const juce::String enginePrefix = bassEnginePrefixOf (mEngineProcessor.get());
    auto noFallback = [] (int) { return true; };

    PagePresetIO::importPagePreset (mProcessor,
                                     PagePresetIO::PageKind::Bass,
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

void BassPage::showPageActionsMenu (juce::Component* anchor)
{
    constexpr int kIdSavePagePreset = 100;
    constexpr int kIdLoadBase       = 1000;

    juce::PopupMenu menu;
    menu.addItem (kIdSavePagePreset, "Save Page Preset As...",
                  mEngineProcessor != nullptr);

    juce::Array<juce::File> presetXmls;
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
                const int id = kIdLoadBase + presetXmls.size();
                presetXmls.add (f);
                loadSub.addItem (id, f.getFileNameWithoutExtension());
            }
        }
        if (presetXmls.isEmpty())
            loadSub.addItem (-1, "(no page presets saved)", false, false);
        menu.addSubMenu ("Load Page Preset", loadSub);
    }

    juce::Component::SafePointer<BassPage> safeThis (this);
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
    if (auto* h = dynamic_cast<HarmlessProcessor*>     (mEngineProcessor.get())) h->apvts.state.addListener (this);
    else if (auto* b = dynamic_cast<BaySickBassProcessor*> (mEngineProcessor.get())) b->apvts.state.addListener (this);
    else if (auto* v = dynamic_cast<VibePlayerProcessor*>  (mEngineProcessor.get())) v->apvts.state.addListener (this);
}

void BassPage::unsubscribeFromEngineApvtsState()
{
    if (auto* h = dynamic_cast<HarmlessProcessor*>     (mEngineProcessor.get())) h->apvts.state.removeListener (this);
    else if (auto* b = dynamic_cast<BaySickBassProcessor*> (mEngineProcessor.get())) b->apvts.state.removeListener (this);
    else if (auto* v = dynamic_cast<VibePlayerProcessor*>  (mEngineProcessor.get())) v->apvts.state.removeListener (this);
}
