#include "DrumPage.h"
#include "../AppPaths.h"
#include "../BaySickSynth/BaySickSynthProcessor.h"
#include "../BaySickSynth/BaySickSynthEditor.h"
#include "../VibePlayer/VibePlayerProcessor.h"
#include "../VibePlayer/VibePlayerEditor.h"
#include "../SampleLibrary.h"
#include "PagePresetIO.h"
#include "StandaloneEditor.h"
using namespace juce;

// ── Helper: recursive Core Library menu builder ──────────────────────────────
// Mirrors the legacy BaySickDrumsEditor.cpp::addLibDirToMenu exactly:
//   - subdir → submenu, recurse all the way down
//   - .wav/.aiff/.aif/.flac → individual clickable item (loads single file)
//   - .sfz → clickable item with [SFZ] suffix (loads SFZ instrument)
// No "load folder" group items - the legacy picker drilled into individual
// samples and that's what users expect.
static void addLibDirToMenuDP (juce::PopupMenu& menu,
                                const juce::File& dir,
                                int kLibBase,
                                std::vector<SampleInstrument>& instruments)
{
    static const char* kAudioExts[] = { ".wav", ".aiff", ".aif", ".flac", nullptr };

    // Folders first (alphabetical), then files (alphabetical) so the user
    // sees navigable subfolders at the top of every level.
    juce::Array<juce::File> dirs;
    dir.findChildFiles (dirs, juce::File::findDirectories, false);
    dirs.sort();
    juce::Array<juce::File> files;
    dir.findChildFiles (files, juce::File::findFiles, false);
    files.sort();

    for (const auto& child : dirs)
    {
        juce::PopupMenu sub;
        addLibDirToMenuDP (sub, child, kLibBase, instruments);
        if (sub.getNumItems() > 0)
            menu.addSubMenu (child.getFileName(), sub);
    }

    for (const auto& child : files)
    {
        bool isAudio = false;
        for (int i = 0; kAudioExts[i]; ++i)
            if (child.hasFileExtension (kAudioExts[i])) { isAudio = true; break; }

        if (isAudio)
        {
            const int id = kLibBase + (int) instruments.size();
            menu.addItem (id, child.getFileNameWithoutExtension());
            instruments.push_back ({ child.getFileNameWithoutExtension(), child, false });
        }
        else if (child.hasFileExtension ("sfz;SFZ"))
        {
            const int id = kLibBase + (int) instruments.size();
            menu.addItem (id, child.getFileNameWithoutExtension() + "  [SFZ]");
            instruments.push_back ({ child.getFileNameWithoutExtension(), child, true });
        }
    }
}

// Recursive XML-preset walker - mirrors addLibDirToMenuDP but for *.xml files.
// Folders become real cascading submenus (matches the sample picker UX).
static void addPresetDirToMenuDP (juce::PopupMenu& menu,
                                   const juce::File& dir,
                                   int kPresetBase,
                                   juce::Array<juce::File>& presetXmls)
{
    juce::Array<juce::File> dirs;
    dir.findChildFiles (dirs, juce::File::findDirectories, false);
    dirs.sort();
    juce::Array<juce::File> files;
    dir.findChildFiles (files, juce::File::findFiles, false, "*.xml");
    files.sort();

    for (const auto& child : dirs)
    {
        juce::PopupMenu sub;
        addPresetDirToMenuDP (sub, child, kPresetBase, presetXmls);
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

juce::File DrumPage::presetsDir()
{
    return AppPaths::appRoot().getChildFile ("Presets")
               .getChildFile ("BaySickDrums");
}

juce::File DrumPage::userPresetsDir()
{
    return presetsDir().getChildFile ("My Presets");
}

// D1.4-fix (c): file-static clipboard for Copy/Paste/Duplicate.  Survives
// across drum tab destroys so copying on one tab and pasting on another
// (or onto a freshly-created tab) works.
// G-6 (2026-04-29): sDrumClipboard removed (Copy/Paste menu items dropped).


DrumPage::DrumPage(VibeSynthProcessor& p, PatternManager& pm, int pageIndex)
    : mProcessor(p), mPM(pm), mPageIndex(juce::jlimit(0, kMaxDrumPages - 1, pageIndex))
{
    mPageColor = VC::DrumCol[mPageIndex];

    // QA-ModelShell TS1: the tab is a model object from birth (idempotent;
    // name syncs via setTabName; engine attaches at selectEngine).
    mProcessor.engineRig().addTab (TabKind::Drums, mPageIndex, mTabName);

    // 2026-04-26 (step 2 commit 3): Drum Kit and per-drum Piano Roll both
    // live on PianoRollPage now.  Skip building those sub-views here -
    // mDrumKitTab + mPianoRoll stay null; sub-tab pills 0 (Drum Kit) and 2
    // (Piano Roll) redirect to PianoRollPage via the editor's showPageForTab
    // click handlers.  Player tab is now the default landing.
    buildPlayerTab();
    // J-6 EQ unification (2026-05-03): EQ sub-tab removed; pre+post EQ live
    // on the Effects page (mixer_drum_<N>_preeq_* / mixer_drum_<N>_*).

    switchTab(1);        // 1 = Player (Drum Kit at index 0 redirects elsewhere)
    startTimerHz(24);
}

DrumPage::~DrumPage()
{
    stopTimer();

    // QA-ModelShell TS1: the engine is rig-owned and survives this view.
    // Teardown happens in EngineRig::removeTab (tab close) or teardownAll
    // (shutdown).  Only the view-owned editor dies here, before the page --
    // its attachments reference the engine's APVTS.
    if (mEngineEditor && mPlayerTab)
        mPlayerTab->removeChildComponent(mEngineEditor.get());
    mEngineEditor.reset();
    mEngineProcessor = nullptr;
}

void DrumPage::switchTab(int idx)
{
    // J-6 EQ unification (2026-05-03): tab order is 0 Drum Kit / 1 Player /
    // 2 Piano Roll (was: 0..3 with EQ as tab 3 - EQ moved to Effects page).
    mActiveTab = juce::jlimit(0, 2, idx);
    if (mDrumKitTab)
    {
        mDrumKitTab->setVisible(mActiveTab == 0);
        if (mActiveTab == 0)
        {
            mDrumKitTab->refreshKitView();
            mDrumKitTab->grabKeyboardFocus();
        }
    }
    if (mPlayerTab) mPlayerTab->setVisible(mActiveTab == 1);
    if (mPianoRoll) mPianoRoll->setVisible(mActiveTab == 2);
    if (mActiveTab == 2 && mPianoRoll) mPianoRoll->grabKeyboardFocus();
    resized();
    if (onSubTabChanged) onSubTabChanged(mActiveTab);
}

// ── D2 Drum Kit hooks (forwarders to mDrumKitTab) ────────────────────────────
// KitDrumInfo (DrumPage's struct, has ribbonTabId) is converted on the fly to
// DrumKitRowInfo (DrumKitGrid's struct, identical fields minus ribbonTabId).
void DrumPage::setKitListProvider (std::function<std::vector<KitDrumInfo>()> fn)
{
    if (! mDrumKitTab) return;
    auto wrapped = [fn = std::move(fn)] {
        std::vector<DrumKitRowInfo> out;
        if (! fn) return out;
        const auto src = fn();
        out.reserve(src.size());
        for (const auto& s : src)
        {
            DrumKitRowInfo r;
            r.pageIndex   = s.pageIndex;
            r.displayName = s.displayName;
            r.hasEngine   = s.hasEngine;
            r.locked      = s.locked;
            r.isActive    = s.isActive;
            r.color       = s.color;
            out.push_back(std::move(r));
        }
        return out;
    };
    mDrumKitTab->setKitRowProvider(std::move(wrapped));
}

void DrumPage::setKitRowClickHandler (std::function<void(int, juce::Component*)> fn)
{
    if (mDrumKitTab) mDrumKitTab->setRowClickHandler (std::move (fn));
}

void DrumPage::setKitAuditionHandlers (std::function<void(int)> onOn,
                                       std::function<void(int)> onOff)
{
    if (mDrumKitTab) mDrumKitTab->setAuditionHandlers (std::move (onOn), std::move (onOff));
}

void DrumPage::setKitReorderHandler (std::function<void(int, int)> fn)
{
    if (mDrumKitTab) mDrumKitTab->setReorderHandler (std::move (fn));
}

void DrumPage::refreshKitView()
{
    if (mDrumKitTab) mDrumKitTab->refreshKitView();
}

void DrumPage::setKitMenuHandler (std::function<void(juce::Component*)> fn)
{
    if (mDrumKitTab) mDrumKitTab->onKitMenuRequested = std::move (fn);
}

void DrumPage::setGlobalLockHandler (std::function<void()> fn)
{
    if (mDrumKitTab) mDrumKitTab->onGlobalLockRequested = std::move (fn);
}

// J-6 EQ unification (2026-05-03): setEQMid removed; pre-rack EQ M/S toggle
// is on the Effects page Pre EQ tab.

void DrumPage::buildDrumKitTab()
{
    mDrumKitTab = std::make_unique<DrumKitContainer>();
    mDrumKitTab->setPatternManager (&mPM);
    mDrumKitTab->setApvts (&mProcessor.apvts);
    addAndMakeVisible(*mDrumKitTab);
}

void DrumPage::buildPlayerTab()
{
    // QA-Layout T2 (L4): the "Pick a sound  v" button + sound label row is
    // gone -- the engine is chosen at the "+" menu, the sound name lives on
    // the tab/title, and the old context menu lives on the title strip's
    // Menu dropdown (showPageActionsMenu).  Sound picking for empty drums
    // stays on the kit grid's per-pad pickers (showSoundPicker).
    mPlayerTab = std::make_unique<Component>();
    addAndMakeVisible(*mPlayerTab);
}

void DrumPage::buildPianoRollTab()
{
    mPianoRoll = std::make_unique<PianoRollContainer>();
    mPianoRoll->setData(&mPM.currentPattern().drumRolls[mPageIndex]);
    // C.5b: pattern's intrinsic TS drives the piano roll's bar-line spacing.
    mPianoRoll->setTimeSignature(mPM.currentPattern().tsNum, mPM.currentPattern().tsDen);
    mPianoRoll->setNoteColor(mPageColor);
    addAndMakeVisible(*mPianoRoll);

    if (mTabName.isEmpty())
        mTabName = "Drum " + juce::String(mPageIndex);
    refreshPianoRollContextLabel();
}

// J-6 EQ unification (2026-05-03): buildEQTab removed.

void DrumPage::setPlayHead(StandalonePlayHead* ph)
{
    mPlayHead = ph;
    if (mPianoRoll)
        mPianoRoll->onSeek = [ph](double b) { if (ph) ph->seekTo(b); };
    if (mDrumKitTab)
        mDrumKitTab->onSeek = [ph](double b) { if (ph) ph->seekTo(b); };
    // Playhead beat is pumped via timerCallback (matches PianoRollContainer flow).
}

void DrumPage::setUndoContext(const UndoContext& ctx)
{
    if (mPianoRoll)
    {
        mPianoRoll->setUndoContext(ctx);
        if (ctx.showHistory) mPianoRoll->onShowHistoryWindow = ctx.showHistory;
    }
    if (mDrumKitTab)
    {
        mDrumKitTab->setUndoContext(ctx);
        if (ctx.showHistory) mDrumKitTab->onShowHistoryWindow = ctx.showHistory;
    }
}

void DrumPage::selectEngine(const juce::String& engineName)
{
    // D1.4-fix: swap-aware.  No-op if same engine already loaded; otherwise
    // tear down old + create new.
    if (engineName == mEngineType && mEngineProcessor) return;

    HeavyOperationOverlay::ScopedOp busy (StandaloneEditor::busyOverlayFor (this),
                                          "Loading " + engineName + "...", true);

    // View teardown only -- the rig tears the old engine down inside
    // setEngineType's swap path.
    if (mEngineProcessor)
    {
        if (mEngineEditor && mPlayerTab)
            mPlayerTab->removeChildComponent(mEngineEditor.get());
        mEngineEditor.reset();
        mEngineProcessor = nullptr;
        mLoadedSampleKind = SampleKind::None;
        mLoadedSamplePath = juce::File();
    }

    mEngineType = engineName;
    refreshPianoRollContextLabel();

    // QA-ModelShell TS1: the model constructs/swaps, prepares, and registers
    // the engine.  This page keeps a non-owning view pointer + the editor.
    auto& rig = mProcessor.engineRig();
    rig.addTab (TabKind::Drums, mPageIndex, mTabName);
    mEngineProcessor = rig.setEngineType (TabKind::Drums, mPageIndex, engineName);
    if (mEngineProcessor != nullptr)
    {
        mEngineEditor.reset (mEngineProcessor->createEditor());
        // 2026-04-26: tell the editor it's in drum context so its preset menu
        // filters to drum-only folders (Hip Hop Drums / EDM Drums) and skips
        // the in-app Core Library (DrumPage's own showSoundPicker handles that).
        // dynamic_cast no-ops for the BaySickSynth editor.
        if (auto* vpe = dynamic_cast<VibePlayerEditor*> (mEngineEditor.get()))
            vpe->setDrumContext (true);
    }

    // Smoke round 2 (Jeff): the SW-3 Swing Mix knob moved OFF the editor
    // title bar onto the PageMenuBar (StandaloneEditor wires it per
    // page-show) so it's visible on every sub-tab.

    if (mPianoRoll)
    {
        mPianoRoll->onNoteAudition = [this](int midiNote)
        {
            if (auto* s = dynamic_cast<BaySickSynthProcessor*>(mEngineProcessor))
                s->auditionNote(midiNote);
            else if (auto* v = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor))
                v->auditionNote(midiNote);
        };
        mPianoRoll->onNoteAuditionOn = [this](int midiNote)
        {
            if (auto* s = dynamic_cast<BaySickSynthProcessor*>(mEngineProcessor))
                s->auditionNoteOn(midiNote);
            else if (auto* v = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor))
                v->auditionNoteOn(midiNote);
        };
        mPianoRoll->onNoteAuditionOff = [this](int midiNote)
        {
            if (auto* s = dynamic_cast<BaySickSynthProcessor*>(mEngineProcessor))
                s->auditionNoteOff(midiNote);
            else if (auto* v = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor))
                v->auditionNoteOff(midiNote);
        };
    }

    if (mEngineEditor && mPlayerTab)
        mPlayerTab->addAndMakeVisible(*mEngineEditor);

    // J-6 EQ unification (2026-05-03): page-level EQ display removed; pre-rack
    // EQ is bound exclusively by EffectsPage (mixer_drum_<N>_preeq_*).
    juce::MessageManager::callAsync([this] { if (isShowing()) resized(); });

    if (onEngineSelected) onEngineSelected();
    if (onEngineEditorRebuilt) onEngineEditorRebuilt();
}

juce::String DrumPage::stripEngineTitle() const
{
    if (dynamic_cast<BaySickSynthEditor*> (mEngineEditor.get())) return BaySickSynthEditor::getEngineTitle();
    if (dynamic_cast<VibePlayerEditor*>   (mEngineEditor.get())) return VibePlayerEditor::getEngineTitle();
    return {};
}

juce::Colour DrumPage::stripEngineAccent() const
{
    if (dynamic_cast<BaySickSynthEditor*> (mEngineEditor.get())) return BaySickSynthEditor::getEngineAccent();
    if (dynamic_cast<VibePlayerEditor*>   (mEngineEditor.get())) return VibePlayerEditor::getEngineAccent();
    return {};
}

juce::Component* DrumPage::stripPresetButton() const
{
    if (auto* e = dynamic_cast<BaySickSynthEditor*> (mEngineEditor.get())) return e->getTitleStripPresetButton();
    if (auto* e = dynamic_cast<VibePlayerEditor*>   (mEngineEditor.get())) return e->getTitleStripPresetButton();
    return nullptr;
}

void DrumPage::timerCallback()
{
    // J-6 EQ unification (2026-05-03): page-level EQ syncFromDSP removed;
    // Effects-page Pre EQ tab handles its own polling.

    if (mPlayHead)
    {
        const bool songMode = mProcessor.isSongMode();
        const double beat = songMode ? -1.0 : mPlayHead->getCurrentBeat();
        if (mPianoRoll)  mPianoRoll->setPlayheadBeat(beat);
        if (mDrumKitTab) mDrumKitTab->setPlayheadBeat(beat);
    }

    if (mPianoRoll)
    {
        mPianoRoll->setData(&mPM.currentPattern().drumRolls[mPageIndex]);
        mPianoRoll->setTimeSignature(mPM.currentPattern().tsNum, mPM.currentPattern().tsDen);
    }

    // While on the Drum Kit sub-tab, repaint at the timer rate so edits from
    // the per-drum Piano Roll tab show up automatically (the kit view reads
    // each drum's notes on every paint).
    if (mActiveTab == 0 && mDrumKitTab)
        mDrumKitTab->repaint();

    pollTriggerLearn();
}

// ─────────────────────────────────────────────────────────────────────────────
// QA-L-Fix: kit trigger MIDI Learn.  The audio thread captures into
// DrumTriggerMap's lock-free handshake slot; this poll (on the existing 24 Hz
// page timer) commits on the message thread, where prompts + alerts are legal.
// ─────────────────────────────────────────────────────────────────────────────
void DrumPage::beginTriggerLearn()
{
    mProcessor.getDrumTriggerMap().beginLearn (mPageIndex);
    mLearnDeadlineMs = juce::Time::getMillisecondCounter() + 30000;   // 30 s, mirrors MidiLearnUI

    auto* aw = new juce::AlertWindow ("MIDI Learn",
                                      "Hit a pad or key to assign it to this drum.\n"
                                      "Waiting 30 seconds...",
                                      juce::MessageBoxIconType::NoIcon);
    aw->addButton ("Cancel", 0);
    mLearnAlert = aw;
    juce::Component::SafePointer<DrumPage> safeThis (this);
    // The map is captured by reference, NOT reached through safeThis: if this
    // drum tab is closed while the learn window is open, the page dies first and
    // a safeThis-gated disarm would never run, leaving the map armed forever
    // (the next note-on anywhere would then be swallowed as a capture).  The
    // processor outlives every page, so this reference is safe.
    auto& map = mProcessor.getDrumTriggerMap();
    const int myIndex = mPageIndex;
    aw->enterModalState (false,
        juce::ModalCallbackFunction::create ([safeThis, &map, myIndex] (int)
        {
            // Reached on Cancel AND on capture/timeout (both exitModalState).
            // Disarm only if the map is still armed FOR THIS drum -- another
            // page may have started its own learn since, and stomping that is
            // the same class of bug as stealing its capture.
            if (map.getLearnTargetDrum() == myIndex)
                map.cancelLearn();

            if (! safeThis) return;   // page gone; map already disarmed above
            auto* dp = safeThis.getComponent();
            dp->mLearnAlert      = nullptr;
            dp->mLearnDeadlineMs = 0;
        }), true);
}

void DrumPage::pollTriggerLearn()
{
    auto& map = mProcessor.getDrumTriggerMap();

    DrumTriggerMap::Binding captured;
    if (map.takeCapturedBindingFor (mPageIndex, captured))
    {
        map.setBinding (mPageIndex, captured);
        mLearnDeadlineMs = 0;
        // The modal callback does the rest of the cleanup (SafePointer nulls
        // itself when JUCE deletes the window).
        if (mLearnAlert) mLearnAlert->exitModalState (0);

        // D-10: only a NOTE capture offers to become the play pitch.  A CC
        // carries no pitch, so there is nothing to offer.  Also suppressed when
        // the learned note ALREADY equals the play note -- D-10 says "prompt on
        // note capture" without that carve-out, but the prompt would be asking
        // to change a value to itself.
        if (captured.kind == DrumTriggerMap::Kind::Note
            && captured.number != getPlayNote())
        {
            const juce::String noteName =
                juce::MidiMessage::getMidiNoteName (captured.number, true, true, 5);
            juce::Component::SafePointer<DrumPage> safeThis (this);
            const int newNote = captured.number;
            juce::AlertWindow::showOkCancelBox (
                juce::MessageBoxIconType::QuestionIcon,
                "MIDI Learn",
                "Also set this drum's play note to " + noteName + "?",
                "Yes", "No", nullptr,
                juce::ModalCallbackFunction::create ([safeThis, newNote] (int result)
                {
                    if (result == 1 && safeThis)
                        safeThis.getComponent()->setPlayNote (newNote);
                }));
        }
        return;
    }

    // 30 s timeout.  Only the page that armed the learn owns the deadline, and
    // it disarms only if the map is still armed for THIS drum -- a stale
    // deadline must not cancel another page's in-flight learn.
    if (mLearnDeadlineMs != 0
        && juce::Time::getMillisecondCounter() > mLearnDeadlineMs)
    {
        mLearnDeadlineMs = 0;
        if (map.getLearnTargetDrum() == mPageIndex)
            map.cancelLearn();
        // The modal callback does the rest of the cleanup (SafePointer nulls
        // itself when JUCE deletes the window).
        if (mLearnAlert) mLearnAlert->exitModalState (0);
    }
}

void DrumPage::paint(Graphics& g)
{
    g.fillAll(VC::Bg);
    // Empty-drum hint on the Player sub-tab (idx 1).  The kit grid's per-pad
    // picker is the one remaining sound-pick route for an empty drum (L4).
    if (mActiveTab == 1 && mEngineType.isEmpty())
    {
        g.setColour(VC::TextDim);
        g.setFont(Font(15.f));
        g.drawText("No sound loaded - pick one from the Drum Kit",
                   getLocalBounds(), Justification::centred);
    }
}

void DrumPage::resized()
{
    auto b = getLocalBounds();
    if (mDrumKitTab) mDrumKitTab->setBounds(b);
    if (mPlayerTab)
    {
        mPlayerTab->setBounds(b);
        if (mEngineEditor && mPlayerTab->getHeight() > 0)
            mEngineEditor->setBounds(mPlayerTab->getLocalBounds());
    }
    if (mPianoRoll) mPianoRoll->setBounds(b);
}

void DrumPage::setTabName(const juce::String& name)
{
    mTabName = name;
    // QA-ModelShell TS1: every rename path funnels through here -- the one
    // sync point for the model tab's name.
    mProcessor.engineRig().renameTab (TabKind::Drums, mPageIndex, name);
    refreshPianoRollContextLabel();
}

void DrumPage::refreshPianoRollContextLabel()
{
    if (!mPianoRoll) return;
    const juce::String engine = mEngineType.isEmpty() ? juce::String("(no engine)")
                                                      : mEngineType;
    mPianoRoll->setContextLabel(mTabName + " - " + engine);
}


// ─────────────────────────────────────────────────────────────────────────────
// D1.4-fix: sound picker (Sample / Synth Patch / Save / New)
// Single popup menu with two top-level submenus.  Picking an item swaps the
// engine to BaySickPlayer (sample) or BaySickSynth (synth patch) and applies
// the chosen state.  Engine type is invisible to the user.
// ─────────────────────────────────────────────────────────────────────────────
void DrumPage::showSoundPicker (juce::Component* anchor)
{
    if (anchor == nullptr) return;

    juce::PopupMenu menu;

    // Item ID layout (avoid colliding ranges):
    //   1                = Clear (None)
    //   2                = New Patch (Blank synth)
    //   3                = Save Current Patch As...
    //   100              = Browse sample folder...
    //   101              = Load SFZ file...
    //   1000..           = Core Library sample items
    //   10000..          = Synth preset XMLs
    constexpr int kIdNone        = 1;
    constexpr int kIdNewPatch    = 2;
    constexpr int kIdSaveAs      = 3;
    constexpr int kIdBrowseSmp   = 100;
    constexpr int kIdLoadSFZ     = 101;
    constexpr int kLibBase       = 1000;
    constexpr int kPresetBase    = 10000;

    std::vector<SampleInstrument> libInstruments;
    // Shared preset-XML list; used by both Sample and Synth Patch submenus
    // so picks from either route to the same load-by-index dispatch below.
    juce::Array<juce::File> presetXmls;

    // ── Sample submenu ───────────────────────────────────────────────────────
    juce::PopupMenu sampleSub;
    sampleSub.addItem (kIdBrowseSmp, "Browse sample folder...");
    sampleSub.addItem (kIdLoadSFZ,   "Load SFZ file...");
    {
        const auto libDir = SampleLibrary::getCoreLibraryDir();
        if (libDir.isDirectory())
        {
            juce::PopupMenu libSub;
            juce::Array<juce::File> packDirs;
            libDir.findChildFiles (packDirs, juce::File::findDirectories, false);
            packDirs.sort();
            for (const auto& packDir : packDirs)
            {
                if (! SampleLibrary::isDrumPack (packDir.getFileName())) continue;
                juce::PopupMenu packSub;
                addLibDirToMenuDP (packSub, packDir, kLibBase, libInstruments);
                if (packSub.getNumItems() > 0)
                    libSub.addSubMenu (packDir.getFileName(), packSub);
            }
            if (libSub.getNumItems() > 0)
            {
                sampleSub.addSeparator();
                sampleSub.addSubMenu ("Core Library", libSub);
            }
        }
        // 2026-04-26: factory BaySickPlayer drum presets - XML wrappers around
        // the Hip Hop / EDM .wav samples with friendly names ("Hip Hop Kick 01"
        // etc.) and per-preset envelope/attack defaults.  Lives in Sample
        // submenu since they're sample-based, not synth-based.
        const auto bspRoot = AppPaths::appRoot().getChildFile ("Presets/BaySickPlayer");
        if (bspRoot.isDirectory())
        {
            juce::Array<juce::File> bspSubs;
            bspRoot.findChildFiles (bspSubs, juce::File::findDirectories, false);
            bspSubs.sort();
            bool anyAdded = false;
            for (const auto& sub : bspSubs)
            {
                if (! SampleLibrary::isDrumPack (sub.getFileName())) continue;
                juce::PopupMenu inner;
                addPresetDirToMenuDP (inner, sub, kPresetBase, presetXmls);
                if (inner.getNumItems() > 0)
                {
                    if (! anyAdded) { sampleSub.addSeparator(); anyAdded = true; }
                    sampleSub.addSubMenu (sub.getFileName(), inner);
                }
            }
        }
    }
    menu.addSubMenu ("Sample", sampleSub);

    // ── Synth Patch submenu - synthesized drum patches only (BaySickDrums) ──
    juce::PopupMenu synthSub;
    synthSub.addItem (kIdNewPatch, "+ New Patch (Blank)");
    synthSub.addSeparator();
    {
        const auto presetDir = presetsDir();
        if (presetDir.isDirectory())
            addPresetDirToMenuDP (synthSub, presetDir, kPresetBase, presetXmls);
        if (presetXmls.isEmpty())
            synthSub.addItem (-1, "(no presets installed)", false, false);

        synthSub.addSeparator();
        const bool canSave = mEngineProcessor != nullptr
                              && (mEngineType == "BaySickSynth" || mEngineType == "BaySickPlayer");
        synthSub.addItem (kIdSaveAs, "Save Current Patch As...", canSave, false);
    }
    menu.addSubMenu ("Synth Patch", synthSub);

    // ── None (clear) - only show if we have something loaded ─────────────────
    if (! mEngineType.isEmpty())
    {
        menu.addSeparator();
        menu.addItem (kIdNone, "None (clear)");
    }

    juce::Component::SafePointer<DrumPage> safeThis (this);
    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (anchor),
        [safeThis,
         libInstruments = std::move (libInstruments),
         presetXmls     = std::move (presetXmls),
         kIdNone, kIdNewPatch, kIdSaveAs, kIdBrowseSmp, kIdLoadSFZ,
         kLibBase, kPresetBase] (int result) mutable
        {
            if (! safeThis || result <= 0) return;
            auto* dp = safeThis.getComponent();

            if (result == kIdNone)            { dp->clearSound();    return; }
            if (result == kIdNewPatch)        { dp->newBlankPatch(); return; }
            if (result == kIdSaveAs)          { dp->savePatchAs();   return; }

            if (result == kIdBrowseSmp)
            {
                const auto libDir = SampleLibrary::getCoreLibraryDir();
                const auto startDir = libDir.isDirectory() ? libDir
                    : juce::File::getSpecialLocation (juce::File::userMusicDirectory);
                dp->mFileChooser = std::make_unique<juce::FileChooser> (
                    "Select sample folder for drum", startDir, "*", true);
                const int flags = juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectDirectories
                                | juce::FileBrowserComponent::canSelectFiles;
                dp->mFileChooser->launchAsync (flags, [safeThis] (const juce::FileChooser& fc)
                {
                    if (auto* d = safeThis.getComponent())
                    {
                        const auto chosen = fc.getResult();
                        if (chosen.isDirectory())          d->loadSampleFolder (chosen);
                        else if (chosen.existsAsFile())    d->loadSampleFile   (chosen);
                    }
                });
                return;
            }

            if (result == kIdLoadSFZ)
            {
                const auto libDir = SampleLibrary::getCoreLibraryDir();
                const auto startDir = libDir.isDirectory() ? libDir
                    : juce::File::getSpecialLocation (juce::File::userMusicDirectory);
                dp->mFileChooser = std::make_unique<juce::FileChooser> (
                    "Select SFZ for drum", startDir, "*.sfz;*.SFZ", true);
                const int flags = juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles;
                dp->mFileChooser->launchAsync (flags, [safeThis] (const juce::FileChooser& fc)
                {
                    if (auto* d = safeThis.getComponent())
                    {
                        const auto chosen = fc.getResult();
                        if (chosen.existsAsFile()) d->loadSampleSFZ (chosen);
                    }
                });
                return;
            }

            if (result >= kLibBase && result < kLibBase + (int) libInstruments.size())
            {
                const auto& inst = libInstruments[result - kLibBase];
                if      (inst.isSFZ)             dp->loadSampleSFZ    (inst.path);
                else if (inst.path.isDirectory()) dp->loadSampleFolder (inst.path);
                else                              dp->loadSampleFile   (inst.path);
                return;
            }

            if (result >= kPresetBase && result < kPresetBase + presetXmls.size())
            {
                // Detect engine from preset's root tag - BaySickPlayerState
                // routes to the player loader, BaySickSynthState to the synth.
                const auto& xml = presetXmls[result - kPresetBase];
                if (auto px = juce::XmlDocument::parse (xml))
                {
                    if (px->hasTagName ("BaySickPlayerState"))
                        dp->loadPlayerPreset (xml);
                    else
                        dp->loadSynthPreset (xml);
                }
                return;
            }
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// Sound load helpers
// ─────────────────────────────────────────────────────────────────────────────
void DrumPage::loadSampleFile (const juce::File& f)
{
    HeavyOperationOverlay::ScopedOp busy (StandaloneEditor::busyOverlayFor (this),
                                          "Loading Sample...", true);
    selectEngine ("BaySickPlayer");
    if (auto* vp = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor))
    {
        // 2026-05-02: route through the processor wrapper so the path/kind
        // properties get stamped onto apvts.state.  Direct mgr.load* calls
        // skip that step and the project-load reload path then has nothing
        // to replay (silent engine after open until user re-picks).
        vp->loadSampleFile (f);
        mLoadedSampleKind = SampleKind::File;
        mLoadedSamplePath = f;
        mSoundName = f.getFileNameWithoutExtension();
        refreshPianoRollContextLabel();
        if (onSoundNameChanged) onSoundNameChanged (mSoundName);
        takeStateSnapshot();
    }
}

void DrumPage::loadSampleFolder (const juce::File& f)
{
    HeavyOperationOverlay::ScopedOp busy (StandaloneEditor::busyOverlayFor (this),
                                          "Loading Samples...", true);
    selectEngine ("BaySickPlayer");
    if (auto* vp = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor))
    {
        // 2026-05-02: see loadSampleFile note above.  Drums always trigger
        // at MIDI 60 -- pass 60 as normalizeRoot.
        vp->loadSampleFolder (f, 60);
        mLoadedSampleKind = SampleKind::Folder;
        mLoadedSamplePath = f;
        mSoundName = f.getFileName();
        refreshPianoRollContextLabel();
        if (onSoundNameChanged) onSoundNameChanged (mSoundName);
        takeStateSnapshot();
    }
}

void DrumPage::loadSampleSFZ (const juce::File& f)
{
    HeavyOperationOverlay::ScopedOp busy (StandaloneEditor::busyOverlayFor (this),
                                          "Loading SFZ...", true);
    selectEngine ("BaySickPlayer");
    if (auto* vp = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor))
    {
        // 2026-05-02: see loadSampleFile note above.
        vp->loadSampleSFZ (f, 60);
        mLoadedSampleKind = SampleKind::SFZ;
        mLoadedSamplePath = f;
        mSoundName = f.getFileNameWithoutExtension();
        refreshPianoRollContextLabel();
        if (onSoundNameChanged) onSoundNameChanged (mSoundName);
        takeStateSnapshot();
    }
}

void DrumPage::loadSynthPreset (const juce::File& xml)
{
    HeavyOperationOverlay::ScopedOp busy (StandaloneEditor::busyOverlayFor (this),
                                          "Loading Preset...", true);
    selectEngine ("BaySickSynth");
    auto* bss = dynamic_cast<BaySickSynthProcessor*>(mEngineProcessor);
    if (bss == nullptr) return;

    auto px = juce::XmlDocument::parse (xml);
    if (! px || ! px->hasTagName (bss->apvts.state.getType())) return;

    auto loaded = juce::ValueTree::fromXml (*px);

    // trackId substitution - preset XMLs were saved with a different prefix
    // (e.g. tk_lay_0_bss_*); rewrite to this drum's prefix (tk_drm_N_bss_*)
    // so the params bind into our engine instead of leaking elsewhere.
    const juce::String localPrefix = bss->getParamPrefix();
    juce::String loadedPrefix;
    for (int i = 0; i < loaded.getNumChildren(); ++i)
    {
        auto child = loaded.getChild (i);
        if (! child.hasType ("PARAM")) continue;
        const juce::String id = child.getProperty ("id").toString();
        const int tagIdx = id.indexOf ("_bss_");
        if (id.startsWith ("tk_") && tagIdx > 3)
        {
            loadedPrefix = id.substring (0, tagIdx + 5);
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
    bss->apvts.replaceState (loaded);

    // Synth preset → no sample reference.
    mLoadedSampleKind = SampleKind::None;
    mLoadedSamplePath = juce::File();

    mSoundName = xml.getFileNameWithoutExtension();
    refreshPianoRollContextLabel();
    if (onSoundNameChanged) onSoundNameChanged (mSoundName);
    takeStateSnapshot();
}

void DrumPage::newBlankPatch()
{
    selectEngine ("BaySickSynth");
    mLoadedSampleKind = SampleKind::None;
    mLoadedSamplePath = juce::File();
    // Engine starts at APVTS defaults - nothing to apply.
    mSoundName = "User Patch";
    refreshPianoRollContextLabel();
    if (onSoundNameChanged) onSoundNameChanged (mSoundName);
    takeStateSnapshot();
}

void DrumPage::clearSound()
{
    // QA-ModelShell TS1: view dies first (editor attachments reference the
    // engine APVTS), then the rig tears the engine down keeping the tab.
    if (mEngineEditor && mPlayerTab)
        mPlayerTab->removeChildComponent (mEngineEditor.get());
    mEngineEditor.reset();
    mEngineProcessor = nullptr;
    mProcessor.engineRig().clearEngine (TabKind::Drums, mPageIndex);
    mEngineType.clear();
    mSoundName.clear();
    mLoadedSampleKind = SampleKind::None;
    mLoadedSamplePath = juce::File();
    refreshPianoRollContextLabel();
    // Restore default tab name on clear so the ribbon doesn't keep the cleared sound's label.
    const juce::String defaultName = "Drum " + juce::String (mPageIndex + 1);
    if (onSoundNameChanged) onSoundNameChanged (defaultName);
    mLoadedStateSnapshot.reset();
    juce::MessageManager::callAsync ([this] { if (isShowing()) resized(); });
}

// D2: snapshot helpers for the dirty-state Delete prompt.
void DrumPage::takeStateSnapshot()
{
    mLoadedStateSnapshot.reset();
    if (mEngineProcessor)
        mEngineProcessor->getStateInformation (mLoadedStateSnapshot);
}

bool DrumPage::isPatchDirty() const
{
    if (mEngineProcessor == nullptr) return false;
    if (mLoadedStateSnapshot.getSize() == 0) return false;   // no baseline → treat as clean
    juce::MemoryBlock current;
    mEngineProcessor->getStateInformation (current);
    return current != mLoadedStateSnapshot;
}

void DrumPage::savePatchAs()
{
    if (mEngineProcessor == nullptr) return;
    const bool isSynth  = (mEngineType == "BaySickSynth");
    const bool isPlayer = (mEngineType == "BaySickPlayer");
    if (! isSynth && ! isPlayer) return;

    const bool isUserPatch = (mSoundName == "User Patch");
    const juce::String message = isUserPatch
        ? juce::String ("Renaming this drum saves it as a preset.\nEnter a name:")
        : juce::String ("Enter a name to save this drum as a preset:");
    auto* aw = new juce::AlertWindow ("Save Patch As",
                                       message,
                                       juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", isUserPatch ? juce::String ("My Patch") : mSoundName);
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<DrumPage> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, aw] (int r)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (r != 1 || ! safeThis) return;
            auto name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;
            auto* dp = safeThis.getComponent();
            if (dp->mEngineProcessor == nullptr) return;

            auto dir = userPresetsDir();
            dir.createDirectory();
            auto file = dir.getChildFile (name + ".xml");

            if (auto* bss = dynamic_cast<BaySickSynthProcessor*>(dp->mEngineProcessor))
            {
                // BaySickSynth: just the apvts state.
                auto state = bss->apvts.copyState();
                if (auto xml = state.createXml())
                    xml->writeTo (file, {});
            }
            else if (auto* vp = dynamic_cast<VibePlayerProcessor*>(dp->mEngineProcessor))
            {
                // BaySickPlayer: wrap apvts state + sample reference.
                // Sample path is stored relative to Core Library when the file
                // lives there ("library:relative/path"), absolute otherwise.
                juce::XmlElement root ("BaySickPlayerState");

                auto state = vp->apvts.copyState();
                if (auto stateXml = state.createXml())
                    root.addChildElement (stateXml.release());

                auto* sampleEl = root.createNewChildElement ("Sample");
                const char* kindStr = "none";
                switch (dp->mLoadedSampleKind)
                {
                    case SampleKind::File:   kindStr = "file";   break;
                    case SampleKind::Folder: kindStr = "folder"; break;
                    case SampleKind::SFZ:    kindStr = "sfz";    break;
                    case SampleKind::None:   kindStr = "none";   break;
                }
                sampleEl->setAttribute ("kind", kindStr);
                if (dp->mLoadedSampleKind != SampleKind::None)
                {
                    const auto libDir = SampleLibrary::getCoreLibraryDir();
                    const auto absPath = dp->mLoadedSamplePath.getFullPathName();
                    if (libDir.isDirectory()
                        && dp->mLoadedSamplePath.isAChildOf (libDir))
                    {
                        const auto rel = dp->mLoadedSamplePath
                                            .getRelativePathFrom (libDir)
                                            .replaceCharacter ('\\', '/');
                        sampleEl->setAttribute ("path", "library:" + rel);
                    }
                    else
                    {
                        sampleEl->setAttribute ("path", absPath);
                    }
                }

                root.writeTo (file, {});
            }

            dp->mSoundName = name;
            dp->refreshPianoRollContextLabel();
            if (dp->onSoundNameChanged) dp->onSoundNameChanged (name);
            dp->takeStateSnapshot();   // saved patch is the new clean baseline
        }), false);
}

// D1.4-fix (c) - BaySickPlayer preset load.  Mirror of loadSynthPreset.
// XML format: <BaySickPlayerState><BaySickPlayerState-apvts/><Sample kind=... path=.../></...>
//   (the apvts state's root tag is BaySickPlayerState too - JUCE convention)
// Sample path may be "library:rel/path" (resolved via SampleLibrary) or absolute.
void DrumPage::loadPlayerPreset (const juce::File& xml)
{
    auto parsed = juce::XmlDocument::parse (xml);
    if (! parsed || ! parsed->hasTagName ("BaySickPlayerState")) return;

    HeavyOperationOverlay::ScopedOp busy (StandaloneEditor::busyOverlayFor (this),
                                          "Loading Preset...", true);
    selectEngine ("BaySickPlayer");
    auto* vp = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor);
    if (vp == nullptr) return;

    // 1. Apply the apvts state child (rewrite trackId prefix).
    if (auto* stateEl = parsed->getChildByName (vp->apvts.state.getType()))
    {
        auto loaded = juce::ValueTree::fromXml (*stateEl);
        const juce::String localPrefix = vp->getParamPrefix();
        juce::String loadedPrefix;
        for (int i = 0; i < loaded.getNumChildren(); ++i)
        {
            auto child = loaded.getChild (i);
            if (! child.hasType ("PARAM")) continue;
            const juce::String id = child.getProperty ("id").toString();
            const int tagIdx = id.indexOf ("_bsp_");
            if (id.startsWith ("tk_") && tagIdx > 3)
            {
                loadedPrefix = id.substring (0, tagIdx + 5);
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
        vp->apvts.replaceState (loaded);
    }

    // 2. Reload sample reference.
    if (auto* sampleEl = parsed->getChildByName ("Sample"))
    {
        const juce::String kind = sampleEl->getStringAttribute ("kind", "none");
        const juce::String pathStr = sampleEl->getStringAttribute ("path");
        juce::File path;
        if (pathStr.startsWith ("library:"))
            path = SampleLibrary::getCoreLibraryDir().getChildFile (pathStr.substring (8));
        else
            path = juce::File (pathStr);

        // 2026-05-02: route through the processor wrappers so the sample
        // path is stamped into apvts.state (project-save needs it for the
        // reload-on-open replay path).
        if (kind == "file" && path.existsAsFile())
        {
            vp->loadSampleFile (path);
            mLoadedSampleKind = SampleKind::File;
            mLoadedSamplePath = path;
        }
        else if (kind == "folder" && path.isDirectory())
        {
            vp->loadSampleFolder (path, 60);
            mLoadedSampleKind = SampleKind::Folder;
            mLoadedSamplePath = path;
        }
        else if (kind == "sfz" && path.existsAsFile())
        {
            vp->loadSampleSFZ (path, 60);
            mLoadedSampleKind = SampleKind::SFZ;
            mLoadedSamplePath = path;
        }
        // Sample missing or kind == none → leave engine with empty sample slot.
    }

    mSoundName = xml.getFileNameWithoutExtension();
    refreshPianoRollContextLabel();
    if (onSoundNameChanged) onSoundNameChanged (mSoundName);
    takeStateSnapshot();
}

int DrumPage::getPlayNote () const
{
    const juce::String pid = "mixer_drum_" + juce::String (mPageIndex) + "_playNote";
    if (auto* p = mProcessor.apvts.getRawParameterValue (pid))
        return juce::jlimit (0, 127, (int) std::round (p->load()));
    return 60;
}

void DrumPage::setPlayNote (int midiNote)
{
    const int newNote = juce::jlimit (0, 127, midiNote);
    const int oldNote = getPlayNote();
    if (newNote == oldNote) return;

    const juce::String pid = "mixer_drum_" + juce::String (mPageIndex) + "_playNote";
    if (auto* p = mProcessor.apvts.getParameter (pid))
    {
        const auto& range = p->getNormalisableRange();
        p->setValueNotifyingHost (range.convertTo0to1 ((float) newNote));
    }

    if (onPlayNoteChanged) onPlayNoteChanged (mPageIndex, oldNote, newNote);
}

void DrumPage::showContextMenu (juce::Component* anchor, bool fromKit)
{
    if (anchor == nullptr) return;

    constexpr int kIdLock      = 1;
    constexpr int kIdPolyphony = 2;
    constexpr int kIdRename    = 5;
    constexpr int kIdDuplicate = 12;
    // G-6 (2026-04-29): Copy/Paste menu items dropped.
    constexpr int kIdSaveAs    = 20;
    // D3: choke-group submenu - 200 = None, 201..216 = groups 1..16.
    constexpr int kIdChokeBase = 200;   // 200 = None
    // Per-drum play pitch - 298 = disabled "Assigned:" status row,
    // 300 + n = MIDI note n (0..127).
    constexpr int kIdNoteHeader   = 298;
    constexpr int kIdNoteBase     = 300;
    // Per-drum kit trigger binding (QA-L-Fix).
    constexpr int kIdMidiLearn    = 30;
    constexpr int kIdMidiForget   = 31;
    constexpr int kIdDelete    = 99;

    juce::PopupMenu menu;
    // T15: nav entries only on the Menu-dropdown shape -- a kit pad's
    // per-drum context menu is not a window menu.
    if (! fromKit && onBuildWindowNavMenu) { onBuildWindowNavMenu (menu); menu.addSeparator(); }
    menu.addItem (kIdLock, "Lock Drum", true, mLocked);

    // Polyphony toggle - engine-specific param dispatch.
    //   BaySickSynth → _bss_voiceMode (0 = poly, 1 = mono)
    //   BaySickPlayer → _bsp_voiceCap (1 = mono, 8 = poly default)
    //   Harmless / others → polyphonic-only, no toggle
    {
        bool isMono = false;
        bool canToggle = false;
        if (auto* bss = dynamic_cast<BaySickSynthProcessor*>(mEngineProcessor))
        {
            // voiceMode raw: 0=Poly, 1=Mono, 2=Lead, 3=Legato.  We treat
            // anything > 0 as "monophonic-ish" for label purposes.
            if (auto* p = bss->apvts.getRawParameterValue (bss->getParamPrefix() + "voiceMode"))
                isMono = (int) std::round (p->load()) >= 1;
            canToggle = true;
        }
        else if (auto* vp = dynamic_cast<VibePlayerProcessor*>(mEngineProcessor))
        {
            if (auto* p = vp->apvts.getRawParameterValue (vp->getParamPrefix() + "voiceCap"))
                isMono = p->load() <= 1.5f;
            canToggle = true;
        }
        const juce::String label = canToggle
            ? (isMono ? juce::String ("Polyphony: Monophonic")
                      : juce::String ("Polyphony: Polyphonic"))
            : juce::String ("Polyphony: (n/a)");
        menu.addItem (kIdPolyphony, label, canToggle, false);
    }

    menu.addSeparator();
    menu.addItem (kIdRename, "Rename...");
    menu.addItem (kIdDuplicate, "Duplicate Drum (new tab)", ! mEngineType.isEmpty());

    menu.addSeparator();
    // D3: Choke Group submenu - global cross-engine cut bus (1..16, 0 = none).
    {
        const juce::String prefix = "mixer_drum_" + juce::String (mPageIndex) + "_chokeGroup";
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

    // QA-L-Fix (D-2/D-4/D-5): kit-only.  This is the drum's PLAY PITCH -- the
    // note it sounds at -- not an input filter.  Every drum starts assigned to
    // C5 (60).  Octave submenus; labels use the roll's FL-style octave
    // numbering (C5 = 60, note/12).
    if (fromKit)
    {
        const juce::String pid = "mixer_drum_" + juce::String (mPageIndex) + "_playNote";
        int curNote = 60;
        if (auto* p = mProcessor.apvts.getRawParameterValue (pid))
            curNote = juce::jlimit (0, 127, (int) std::round (p->load()));

        juce::PopupMenu noteSub;
        noteSub.addItem (kIdNoteHeader,
                         "Assigned: "
                             + juce::MidiMessage::getMidiNoteName (curNote, true, true, 5),
                         false, false);
        noteSub.addSeparator();
        for (int oct = 0; oct * 12 < 128; ++oct)
        {
            const int lo = oct * 12;
            const int hi = juce::jmin (127, lo + 11);
            juce::PopupMenu octSub;
            for (int n = lo; n <= hi; ++n)
                octSub.addItem (kIdNoteBase + n,
                                juce::MidiMessage::getMidiNoteName (n, true, true, 5)
                                    + "  (" + juce::String (n) + ")",
                                true, curNote == n);
            noteSub.addSubMenu (juce::MidiMessage::getMidiNoteName (lo, true, true, 5)
                                    + " - "
                                    + juce::MidiMessage::getMidiNoteName (hi, true, true, 5),
                                octSub);
        }
        menu.addSubMenu ("MIDI Note", noteSub);

        // D-3: MIDI Learn sits directly below MIDI Note.  Label carries the
        // current binding so the kit menu doubles as the binding readout.
        const juce::String bind = mProcessor.getDrumTriggerMap().describeBinding (mPageIndex);
        menu.addItem (kIdMidiLearn,
                      bind.isEmpty() ? juce::String ("MIDI Learn")
                                     : "MIDI Learn: " + bind);
        if (bind.isNotEmpty())
            menu.addItem (kIdMidiForget, "MIDI Forget: " + bind);
    }

    menu.addSeparator();
    const bool canSave = mEngineProcessor != nullptr
                          && (mEngineType == "BaySickSynth" || mEngineType == "BaySickPlayer");
    menu.addItem (kIdSaveAs, "Save Current Patch As...", canSave);

    // QA-Layout T2: the page-scope route (the Menu dropdown) also carries the
    // page-preset entries the old separate showPageActionsMenu held; the kit
    // route keeps its per-drum shape without them.
    constexpr int kIdSavePagePreset = 40;
    constexpr int kIdPageLoadBase   = 1000;
    juce::Array<juce::File> pagePresetXmls;
    if (! fromKit)
    {
        menu.addSeparator();
        menu.addItem (kIdSavePagePreset, "Save Page Preset As...",
                      mEngineProcessor != nullptr);
        juce::PopupMenu loadSub;
        const auto root = PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Drum);
        if (root.isDirectory())
        {
            juce::Array<juce::File> files;
            root.findChildFiles (files, juce::File::findFiles, false, "*.xml");
            files.sort();
            for (auto& f : files)
            {
                const int id = kIdPageLoadBase + pagePresetXmls.size();
                pagePresetXmls.add (f);
                loadSub.addItem (id, f.getFileNameWithoutExtension());
            }
        }
        if (pagePresetXmls.isEmpty())
            loadSub.addItem (-1, "(no page presets saved)", false, false);
        menu.addSubMenu ("Load Page Preset", loadSub);
    }

    menu.addSeparator();
    menu.addItem (kIdDelete, "Delete Drum", ! mLocked);   // locked drums can't be deleted

    juce::Component::SafePointer<DrumPage> safeThis (this);
    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (anchor),
        [safeThis, pagePresetXmls = std::move (pagePresetXmls),
         kIdLock, kIdPolyphony, kIdRename, kIdDuplicate,
         kIdChokeBase, kIdNoteBase, kIdMidiLearn, kIdMidiForget,
         kIdSaveAs, kIdSavePagePreset, kIdPageLoadBase, kIdDelete] (int r) mutable
        {
            if (! safeThis || r <= 0) return;
            auto* dp = safeThis.getComponent();

            // Page-preset load submenu (1000 + i -> pagePresetXmls[i]).
            if (r >= kIdPageLoadBase && r < kIdPageLoadBase + pagePresetXmls.size())
            {
                dp->loadPagePreset (pagePresetXmls[r - kIdPageLoadBase]);
                return;
            }

            // Per-drum play pitch (300 + n = note n).
            if (r >= kIdNoteBase && r < kIdNoteBase + 128)
            {
                dp->setPlayNote (r - kIdNoteBase);
                return;
            }

            if (r == kIdMidiLearn)  { dp->beginTriggerLearn(); return; }
            if (r == kIdMidiForget)
            {
                dp->mProcessor.getDrumTriggerMap().clearBinding (dp->mPageIndex);
                return;
            }

            // D3: choke-group submenu (200..216 → 0..16).
            if (r >= kIdChokeBase && r <= kIdChokeBase + 16)
            {
                const int newGroup = r - kIdChokeBase;
                const juce::String prefix = "mixer_drum_" + juce::String (dp->mPageIndex) + "_chokeGroup";
                if (auto* p = dp->mProcessor.apvts.getParameter (prefix))
                {
                    const auto& range = p->getNormalisableRange();
                    p->setValueNotifyingHost (range.convertTo0to1 ((float) newGroup));
                }
                return;
            }

            if (r == kIdLock)         dp->setLocked (! dp->mLocked);
            else if (r == kIdPolyphony)
            {
                if (auto* bss = dynamic_cast<BaySickSynthProcessor*>(dp->mEngineProcessor))
                {
                    // voiceMode is a 4-choice param: Poly(0) / Mono(1) / Lead(2) / Legato(3).
                    // Toggle Poly <-> Mono ONLY (Lead/Legato are advanced; user wants binary).
                    if (auto* p = bss->apvts.getParameter (bss->getParamPrefix() + "voiceMode"))
                    {
                        const auto& range = p->getNormalisableRange();
                        const int curRaw = (int) std::round (range.convertFrom0to1 (p->getValue()));
                        const int nextRaw = (curRaw == 0) ? 1 : 0;   // Poly=0, Mono=1
                        p->setValueNotifyingHost (range.convertTo0to1 ((float) nextRaw));
                    }
                }
                else if (auto* vp = dynamic_cast<VibePlayerProcessor*>(dp->mEngineProcessor))
                {
                    if (auto* p = vp->apvts.getParameter (vp->getParamPrefix() + "voiceCap"))
                    {
                        // Toggle 1 ↔ 8.  voiceCap range is 1..16, normalized 0..1.
                        const auto& range = p->getNormalisableRange();
                        const float curRaw = range.convertFrom0to1 (p->getValue());
                        const float nextRaw = (curRaw <= 1.5f) ? 8.f : 1.f;
                        p->setValueNotifyingHost (range.convertTo0to1 (nextRaw));
                    }
                }
            }
            else if (r == kIdRename)
            {
                if (dp->onRenameRequested) dp->onRenameRequested();
            }
            // G-6: kIdCopy / kIdPaste handlers removed.
            else if (r == kIdDuplicate)
            {
                const auto state = dp->exportDrumState();
                if (state.isNotEmpty() && dp->onDuplicateRequested)
                    dp->onDuplicateRequested (state);
            }
            else if (r == kIdSaveAs)         dp->savePatchAs();
            else if (r == kIdSavePagePreset) dp->savePagePreset();
            else if (r == kIdDelete)         dp->requestDelete();
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// Drum-state import/export (Copy/Paste/Duplicate)
// ─────────────────────────────────────────────────────────────────────────────

// G-6 (2026-04-29): rewrite the binary state's PARAM ids so the source page's
// APVTS prefix (tk_drm_<srcIdx>_<engineTag>_) becomes this destination page's
// prefix.  Without this importDrumState() silently drops every param because
// setStateInformation matches by id.  Mirrors the F-2 fix in loadPreset()
// pattern from LayersPage / BassPage.
static juce::String drumEngineLocalPrefix (juce::AudioProcessor* proc)
{
    if (auto* bss = dynamic_cast<BaySickSynthProcessor*>(proc))  return bss->getParamPrefix();
    if (auto* bsp = dynamic_cast<VibePlayerProcessor*>(proc))    return bsp->getParamPrefix();
    return {};
}

static void drumSubstituteEnginePrefixInBinary (juce::AudioProcessor* proc,
                                                juce::MemoryBlock& mb)
{
    if (proc == nullptr || mb.getSize() == 0) return;

    const juce::String localPrefix = drumEngineLocalPrefix (proc);
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

juce::String DrumPage::exportDrumState() const
{
    if (mEngineType.isEmpty() || mEngineProcessor == nullptr) return {};

    juce::XmlElement el ("DrumPageState");
    el.setAttribute ("engine",  mEngineType);
    el.setAttribute ("name",    mSoundName);
    el.setAttribute ("locked",  mLocked ? 1 : 0);

    juce::MemoryBlock mb;
    mEngineProcessor->getStateInformation (mb);
    el.setAttribute ("data", mb.toBase64Encoding());

    return el.toString (juce::XmlElement::TextFormat().singleLine());
}

void DrumPage::importDrumState (const juce::String& xml)
{
    if (xml.isEmpty()) return;
    if (mLocked)       return;

    auto parsed = juce::XmlDocument::parse (xml);
    if (! parsed || ! parsed->hasTagName ("DrumPageState")) return;

    const juce::String engine = parsed->getStringAttribute ("engine");
    const juce::String name   = parsed->getStringAttribute ("name");
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
            // this every param is silently dropped by setStateInformation's
            // id match.
            drumSubstituteEnginePrefixInBinary (mEngineProcessor, mb);
            mEngineProcessor->setStateInformation (mb.getData(), (int) mb.getSize());
        }
    }

    mSoundName = name;
    mLocked    = lock;
    refreshPianoRollContextLabel();
    if (onSoundNameChanged) onSoundNameChanged (mSoundName);
}

// ─────────────────────────────────────────────────────────────────────────────
// Delete with confirmation (and optional Save Patch As prompt)
// ─────────────────────────────────────────────────────────────────────────────
void DrumPage::requestDelete()
{
    // G-7 (2026-04-29): close-prompt with dirty-aware buttons (matches Layer/Bass).
    juce::Component::SafePointer<DrumPage> safeThis (this);

    auto fireDelete = [safeThis] {
        if (auto* dp = safeThis.getComponent())
            if (dp->onDeleteRequested) dp->onDeleteRequested();
    };

    const juce::String warning =
        "Deleting this drum removes its Player, Mixer Strip, "
        "Effects Rack, and Piano Roll.";

    const bool isSaveable = mEngineProcessor != nullptr
                             && (mEngineType == "BaySickSynth" || mEngineType == "BaySickPlayer");
    if (isSaveable && isPatchDirty())
    {
        const juce::String dirtyWarning = warning
            + "\n\n\"Save Page Preset & Delete\" writes the entire page state "
              "(engine + EQ + effects rack + strip settings) to disk first, "
              "then deletes the tab.";

        auto* aw = new juce::AlertWindow (
            "Delete Drum", dirtyWarning, juce::AlertWindow::QuestionIcon);
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
        "Delete Drum", warning, juce::AlertWindow::WarningIcon);
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
static juce::String drumEnginePrefixOf (juce::AudioProcessor* p)
{
    if (auto* s = dynamic_cast<BaySickSynthProcessor*>(p)) return s->getParamPrefix();
    if (auto* v = dynamic_cast<VibePlayerProcessor*>  (p)) return v->getParamPrefix();
    return {};
}

void DrumPage::savePagePreset (std::function<void()> onSaved)
{
    auto* aw = new juce::AlertWindow ("Save Page Preset",
                                       "Enter a name for this drum page preset:",
                                       juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", "My Drum");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<DrumPage> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, aw, onSaved] (int r)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (r != 1 || ! safeThis) return;

            const juce::String name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;

            auto dir = PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Drum);
            dir.createDirectory();
            auto target = dir.getChildFile (name + ".xml");
            int n = 2;
            while (target.exists())
                target = dir.getChildFile (name + " (" + juce::String (n++) + ").xml");

            const juce::String stripPrefix = "mixer_drum_" + juce::String (safeThis->mPageIndex);
            const juce::String enginePrefix = drumEnginePrefixOf (safeThis->mEngineProcessor);

            const juce::String xml = PagePresetIO::exportPagePreset (
                safeThis->mProcessor,
                PagePresetIO::PageKind::Drum,
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

void DrumPage::loadPagePreset (const juce::File& xml)
{
    if (! xml.existsAsFile()) return;

    const juce::String savedEngineType = PagePresetIO::peekEngineType (xml);
    if (savedEngineType.isNotEmpty() && savedEngineType != mEngineType)
        selectEngine (savedEngineType);

    const juce::String stripPrefix = "mixer_drum_" + juce::String (mPageIndex);
    const juce::String enginePrefix = drumEnginePrefixOf (mEngineProcessor);
    auto noFallback = [] (int) { return true; };

    PagePresetIO::importPagePreset (mProcessor,
                                     PagePresetIO::PageKind::Drum,
                                     mPageIndex,
                                     stripPrefix,
                                     mEngineProcessor,
                                     enginePrefix,
                                     noFallback,
                                     xml.loadFileAsString());

    // 2026-05-05 fix: sync the DrumPage's sample tracking state from the
    // engine that just had its state restored.  selectEngine creates a
    // fresh engine with mLoadedSampleKind=None / mLoadedSamplePath=empty,
    // and importPagePreset restores the engine's INTERNAL apvts (which
    // includes bsp_loadKind / bsp_loadPath for VibePlayer + the sample
    // re-load).  But DrumPage's tracking is NOT in apvts - without syncing
    // it here, mSoundName stays empty and savePatchAs cannot reconstruct
    // the load call even though the engine has a kit loaded.
    if (auto* vp = dynamic_cast<VibePlayerProcessor*> (mEngineProcessor))
    {
        const auto kind = vp->apvts.state.getProperty ("bsp_loadKind", juce::String()).toString();
        const auto path = vp->apvts.state.getProperty ("bsp_loadPath", juce::String()).toString();
        if (path.isNotEmpty())
        {
            const juce::File f (path);
            if      (kind == "folder") { mLoadedSampleKind = SampleKind::Folder; mLoadedSamplePath = f; mSoundName = f.getFileName(); }
            else if (kind == "sfz")    { mLoadedSampleKind = SampleKind::SFZ;    mLoadedSamplePath = f; mSoundName = f.getFileNameWithoutExtension(); }
            else if (kind == "file")   { mLoadedSampleKind = SampleKind::File;   mLoadedSamplePath = f; mSoundName = f.getFileNameWithoutExtension(); }
        }
    }
    else if (mEngineProcessor != nullptr)
    {
        // BaySickSynth / Harmless / etc. - no sample reference; the patch is
        // pure APVTS state.  Use the preset filename as the sound label.
        mLoadedSampleKind = SampleKind::None;
        mLoadedSamplePath = juce::File();
        mSoundName = xml.getFileNameWithoutExtension();
    }

    refreshPianoRollContextLabel();

    const juce::String newName = xml.getFileNameWithoutExtension();
    setTabName (newName);
    if (onSoundNameChanged) onSoundNameChanged (newName);
    takeStateSnapshot();
}

// G-7 (2026-04-29): XML-string variants for kit save/load.  Lets saveKitAs
// embed each drum's full Page Preset (engine + strip params + insert rack +
// post-EQ) per drum slot - the kit XML now carries the entire chain rather
// than just engine state.
juce::String DrumPage::exportPagePresetXml() const
{
    if (mEngineProcessor == nullptr || mEngineType.isEmpty()) return {};

    const juce::String stripPrefix = "mixer_drum_" + juce::String (mPageIndex);
    const juce::String enginePrefix = drumEnginePrefixOf (mEngineProcessor);

    return PagePresetIO::exportPagePreset (
        mProcessor,
        PagePresetIO::PageKind::Drum,
        mPageIndex,
        stripPrefix,
        mEngineProcessor,
        mEngineType,
        enginePrefix);
}

void DrumPage::importPagePresetXml (const juce::String& xml)
{
    if (xml.isEmpty()) return;

    const juce::String savedEngineType = [&xml]
    {
        auto parsed = juce::XmlDocument::parse (xml);
        if (! parsed || ! parsed->hasTagName ("BaySickPagePreset")) return juce::String();
        return parsed->getStringAttribute ("engineType");
    }();

    if (savedEngineType.isNotEmpty() && savedEngineType != mEngineType)
        selectEngine (savedEngineType);

    const juce::String stripPrefix = "mixer_drum_" + juce::String (mPageIndex);
    const juce::String enginePrefix = drumEnginePrefixOf (mEngineProcessor);
    auto noFallback = [] (int) { return true; };

    PagePresetIO::importPagePreset (mProcessor,
                                     PagePresetIO::PageKind::Drum,
                                     mPageIndex,
                                     stripPrefix,
                                     mEngineProcessor,
                                     enginePrefix,
                                     noFallback,
                                     xml);
    takeStateSnapshot();
}

void DrumPage::showPageActionsMenu (juce::Component* anchor)
{
    // QA-Layout T2: the Menu dropdown IS the page-scope context menu; the
    // kit pads keep their per-drum shape via fromKit=true (MIDI Note / MIDI
    // Learn rows, no page-preset entries).
    showContextMenu (anchor, /*fromKit*/ false);
}
