#include "VoxPage.h"
#include "../BaySickVocal/BaySickVocalProcessor.h"
#include "../BaySickVocal/BaySickVocalEditor.h"
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

// QA-E Task 4 (2026-05-12): the 2026-04-29 debug `isInterestedInFileDrag`
// + `filesDropped` handlers (originally added to test whether dropping a WAV
// onto a Vox tab routed through the Vox InsertNode) are removed.  Drag-and-
// drop of audio entries is the Browser-panel-to-Builder-grid flow handled by
// ArrangementGrid; per-page tab drag was never an intended user surface.
// Library tagging for Vox recordings happens via commitRecordingResult, and
// re-tagging via Task 7's Properties Routing dropdown (when it lands).

VoxPage::VoxPage (int pageIndex)
    : mPageIndex (pageIndex),
      mTabName   ("Vox " + juce::String (pageIndex + 1))
{
    // G-7: wire dirty-listener pointers (listener attaches to engine
    // apvts.state inside selectEngine after engine creation).
    mDirtyListener.dirtyFlag = &mPageDirty;
    mDirtyListener.suppress  = &mSuppressDirty;

    // J-6 EQ unification (2026-05-03): buildEQTab removed; pre-rack EQ on Effects page only.

    // QA-E Task 4 (2026-05-12): mClipFileLabel deleted.  Browser visibility
    // now driven by PatternManager AudioLibrary entries with pageOwnerChannelId
    // tagging (see §9 17th Forks entry); per-page label is redundant.

    // H-6b (2026-05-01): Vox tabs are always BaySickVocal.  Pick on construction.
    selectEngine (EngineType::BaySickVocal);
    switchTab (0);
}

VoxPage::~VoxPage()
{
    detachDirtyListener();
}

void VoxPage::buildEnginePicker()
{
    // H-6b (2026-05-01): No engine picker on Vox tabs anymore.  BaySickVocal
    // is the only engine; instantiated unconditionally in the ctor.  The
    // page header bar stays minimal - clip file label only.  The right-click
    // page actions menu lives on the ribbon tab itself, not here.
}

// ─────────────────────────────────────────────────────────────────────────────
// G-6 helpers - Vox preset folder + recursive folder→submenu walker.
// ─────────────────────────────────────────────────────────────────────────────
// 2026-05-05 consolidation: route every Vox preset through PagePresetIO's
// per-kind directory ("Vox Page/My Presets") so saved files appear in the
// load submenu (the previous `Vox/` literal didn't match where save wrote).
static juce::File voxPresetsRootDir()
{
    return PagePresetIO::presetsDirForPageKind (PagePresetIO::PageKind::Vox);
}

static juce::File voxMyPresetsDir()
{
    return PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Vox);
}

static void addVoxPresetDirToMenu (juce::PopupMenu& menu,
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
        addVoxPresetDirToMenu (subMenu, sub, kPresetBase, presetXmls);
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

void VoxPage::showEngineContextMenu()
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
    menu.addItem (kIdDuplicate, "Duplicate Vox (new tab)");

    menu.addSeparator();
    menu.addItem (kIdSavePagePreset, "Save Page Preset As...",
                  mVocalProc != nullptr);

    juce::Array<juce::File> presetXmls;
    {
        juce::PopupMenu loadSub;
        const auto root = voxPresetsRootDir();
        if (root.isDirectory())
            addVoxPresetDirToMenu (loadSub, root, kIdLoadPresetBase, presetXmls);
        if (presetXmls.isEmpty())
            loadSub.addItem (-1, "(no presets installed)", false, false);
        menu.addSubMenu ("Load Page Preset", loadSub);
    }

    menu.addSeparator();
    menu.addItem (kIdDelete, "Delete Vox", ! mLocked);

    juce::Component::SafePointer<VoxPage> self (this);
    // H-6b: anchor on the page itself since the engine picker is gone.
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
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

void VoxPage::saveVoxPagePreset()
{
    auto* aw = new juce::AlertWindow ("Save Vox Page Preset",
                                       "Enter a name for this Vox page preset:",
                                       juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", "My Vox");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<VoxPage> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, aw] (int r)
        {
            if (r != 1 || ! safeThis) return;
            const juce::String name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;

            auto dir = voxMyPresetsDir();
            dir.createDirectory();
            auto target = dir.getChildFile (name + ".xml");
            int n = 2;
            while (target.exists())
                target = dir.getChildFile (name + " (" + juce::String (n++) + ").xml");

            const juce::String xml = safeThis->exportVoxState();
            if (xml.isNotEmpty())
                target.replaceWithText (xml);
        }), true);
}

void VoxPage::loadVoxPagePreset (const juce::File& xml)
{
    if (! xml.existsAsFile()) return;
    const juce::String contents = xml.loadFileAsString();
    if (contents.isNotEmpty())
        importVoxState (contents);
}

// ─────────────────────────────────────────────────────────────────────────────
// G-7 (2026-04-29): Page Preset save/load (full chain) + bus fallback.
// ─────────────────────────────────────────────────────────────────────────────
void VoxPage::takeStateSnapshot()
{
    mPageDirty = false;
}

void VoxPage::attachDirtyListener()
{
    detachDirtyListener();
    // H-6b (2026-05-01): listener attaches to BaySickVocal's apvts instead.
    if (auto* bv = dynamic_cast<BaySickVocalProcessor*> (mVocalProc.get()))
        bv->apvts.state.addListener (&mDirtyListener);
}

void VoxPage::detachDirtyListener()
{
    if (auto* bv = dynamic_cast<BaySickVocalProcessor*> (mVocalProc.get()))
        bv->apvts.state.removeListener (&mDirtyListener);
}

void VoxPage::savePagePreset (std::function<void()> onSaved)
{
    if (mFullProcessor == nullptr || getEngineProcessor() == nullptr)
    {
        saveVoxPagePreset();
        if (onSaved) onSaved();
        return;
    }

    auto* aw = new juce::AlertWindow ("Save Page Preset",
                                       "Enter a name for this vox page preset:",
                                       juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", "My Vox");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<VoxPage> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, aw, onSaved] (int r)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (r != 1 || ! safeThis) return;

            const juce::String name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;

            auto dir = PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Vox);
            dir.createDirectory();
            auto target = dir.getChildFile (name + ".xml");
            int n = 2;
            while (target.exists())
                target = dir.getChildFile (name + " (" + juce::String (n++) + ").xml");

            const juce::String stripPrefix = "mixer_vox_" + juce::String (safeThis->mPageIndex);
            // H-6b: always BaySickVocal.  Engine prefix is the constant `bsv_`.
            juce::String engineType   = "BaySickVocal";
            juce::String enginePrefix = "bsv_";

            const juce::String xml = PagePresetIO::exportPagePreset (
                *safeThis->mFullProcessor,
                PagePresetIO::PageKind::Vox,
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

void VoxPage::loadPagePreset (const juce::File& xml)
{
    if (! xml.existsAsFile()) return;
    if (mFullProcessor == nullptr) { loadVoxPagePreset (xml); return; }

    // H-6b (2026-05-01): always BaySickVocal.
    if (mVocalProc == nullptr) selectEngine (EngineType::BaySickVocal);

    const juce::String stripPrefix = "mixer_vox_" + juce::String (mPageIndex);
    // BaySickVocal's engine prefix is the constant `bsv_` (no per-strip
    // suffix today).  PagePresetIO uses this only for save/load XML keying.
    const juce::String enginePrefix = "bsv_";

    // Bus fallback: query MixerPage for kVoxBus2 activation.  If the query
    // isn't installed (e.g. page constructed without StandaloneEditor wiring),
    // assume all buses active so existing behavior is preserved.
    auto query = mBusActiveQuery
                    ? mBusActiveQuery
                    : std::function<bool(int)> ([] (int) { return true; });

    // G-7: suppress dirty during bulk state restore.
    mSuppressDirty = true;
    PagePresetIO::importPagePreset (*mFullProcessor,
                                     PagePresetIO::PageKind::Vox,
                                     mPageIndex,
                                     stripPrefix,
                                     getEngineProcessor(),
                                     enginePrefix,
                                     query,
                                     xml.loadFileAsString());
    mSuppressDirty = false;
    takeStateSnapshot();
}

void VoxPage::showPageActionsMenu (juce::Component* anchor)
{
    constexpr int kIdSavePagePreset = 100;
    constexpr int kIdLoadBase       = 1000;

    juce::PopupMenu menu;
    menu.addItem (kIdSavePagePreset, "Save Page Preset As...",
                  getEngineProcessor() != nullptr);

    juce::Array<juce::File> presetXmls;
    {
        juce::PopupMenu loadSub;
        const auto root = PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Vox);
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

    juce::Component::SafePointer<VoxPage> safeThis (this);
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

void VoxPage::requestDelete()
{
    juce::Component::SafePointer<VoxPage> safeThis (this);
    auto fireDelete = [safeThis] {
        if (auto* p = safeThis.getComponent())
            if (p->onDeleteRequested) p->onDeleteRequested();
    };

    // QA-E Task 5 (2026-05-15): tab close now cascades library cleanup --
    // every recording (dry + wet) made on this Vox tab gets removed from
    // the audio library alongside the page.  Physical WAVs in the project's
    // Samples folder stay on disk so the user can drag them back in later.
    const juce::String warning =
        "Deleting this vox tab removes its Player, Mixer Strip, "
        "Effects Rack, Piano Roll, and the audio library entries for "
        "every recording made on this tab.\n"
        "The audio files in your project's Samples folder stay on disk.";

    if (getEngineProcessor() != nullptr && isPatchDirty())
    {
        const juce::String dirtyWarning = warning
            + "\n\n\"Save Page Preset & Delete\" writes the entire page state "
              "(engine + EQ + effects rack + strip settings) to disk first, "
              "then deletes the tab.";

        auto* aw = new juce::AlertWindow (
            "Delete Vox", dirtyWarning, juce::AlertWindow::QuestionIcon);
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
        "Delete Vox", warning, juce::AlertWindow::WarningIcon);
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

juce::AudioProcessor* VoxPage::getEngineProcessor() const noexcept
{
    // H-6b: Vox tabs are always BaySickVocal.  BaySickPlayer kept in the enum
    // for save-state back-compat only - never returned as the active engine.
    return mVocalProc.get();
}

void VoxPage::selectEngine (EngineType e)
{
    // H-6b (2026-05-01): coerce any caller's ask to BaySickVocal.  The picker
    // is gone; this method is now mostly a "ensure the vocal processor is
    // live" helper called from the constructor + state-load paths.
    juce::ignoreUnused (e);
    e = EngineType::BaySickVocal;

    if (e == mEngineType && mVocalProc != nullptr)
        return;

    if (onEngineDestroying) onEngineDestroying();

    if (! mVocalProc)
    {
        auto vp = std::make_unique<BaySickVocalProcessor>();
        vp->prepareToPlay (44100.0, 512);
        mVocalProc = std::move (vp);
        // J-6 EQ unification (2026-05-03): cast-fixed editor pointer (originally
        // for setPreRackEQ injection; that hookup is removed).
        auto* ed = static_cast<BaySickVocalEditor*> (mVocalProc->createEditor());
        mVocalEditor.reset (ed);
        if (mVocalEditor) addChildComponent (*mVocalEditor);
        // J-6 EQ unification (2026-05-03): Pre Rack EQ injection removed -
        // pre-rack EQ is exclusively edited via the Effects page.
    }

    mEngineType = e;
    if (mVocalEditor) mVocalEditor->setVisible (true);
    resized();

    repaint();

    // H-6b (2026-05-01): EQ binding moved into setProcessor() so it fires
    // when the StandaloneEditor hands the full processor to this page (which
    // happens AFTER the constructor's selectEngine call).

    // G-7 (2026-04-29): hook dirty-tracker on the new engine's apvts and
    // reset the flag.
    attachDirtyListener();
    takeStateSnapshot();

    if (onEngineChanged) onEngineChanged();
}

juce::AudioProcessorEditor* VoxPage::activeEditor() const
{
    return mVocalEditor.get();
}

void VoxPage::switchTab (int idx)
{
    // H-6b (2026-05-01) / J-6 (2026-05-03): forward outer tab-slot click to
    // BaySickVocalEditor's setActiveTab.  Tab labels:
    //   0 BaySickVocals, 1 Vocal Chain, 2 BaySickPitch, 3 BaySickAlign,
    //   4 BaySickNAM/IR  (Pre Rack EQ tab removed in J-6 EQ unification)
    mActiveTab = juce::jlimit (0, 4, idx);
    if (auto* ed = dynamic_cast<BaySickVocalEditor*> (mVocalEditor.get()))
        ed->setActiveTab (mActiveTab);
    if (mVocalEditor) mVocalEditor->setVisible (true);
    resized();
    repaint();
}

// H-6b (2026-05-01): setProcessor was inline-set previously; promoted to a
// real method so the EQ-binding logic can fire when the StandaloneEditor
// hands the full processor over (constructor's selectEngine fires before
// setProcessor in the new spawn order).
void VoxPage::setProcessor (VibeSynthProcessor* p)
{
    mFullProcessor = p;
    // J-6 EQ unification (2026-05-03): page-level EQ display removed; pre-rack
    // EQ is bound exclusively by EffectsPage (mixer_vox_<N>_preeq_*).

    // QA-F Task 3: inject the BaySickAlign services (composite renderer +
    // clip signatures + channel list + project folder) into the vocal
    // engine.  The engine never links against VibeSynthProcessor; these
    // hooks are its only reach into the timeline.  All message-thread.
    if (auto* bv = dynamic_cast<BaySickVocalProcessor*> (mVocalProc.get()))
    {
        bv->setOwnChannelId (MixerChannelIds::voxInsert (mPageIndex));

        VibeSynthProcessor* full = mFullProcessor;
        bv->onRenderComposite = [full] (int channelId, double& outStartBeat,
                                        juce::int64& outStartSample, double& outSr)
            -> juce::AudioBuffer<float>
        {
            outStartBeat = 0.0; outStartSample = 0; outSr = 44100.0;
            if (full == nullptr) return {};
            outSr = (full->getSampleRate() > 0.0) ? full->getSampleRate() : 44100.0;
            return full->renderChannelComposite (channelId, outStartBeat, outStartSample);
        };
        bv->onChannelClipSignature = [full] (int channelId) -> juce::int64
        {
            return (full != nullptr) ? full->channelClipSignature (channelId) : 0;
        };
        bv->onListCandidateChannels = [full]()
            -> std::vector<std::pair<int, juce::String>>
        {
            return (full != nullptr) ? full->listAudioClipChannels()
                                     : std::vector<std::pair<int, juce::String>> {};
        };
        bv->onGetProjectFolder = [full]() -> juce::File
        {
            if (full == nullptr) return {};
            const juce::ScopedLock lk (full->mProjectFolderLock);
            return full->mCurrentProjectFolder;
        };
        {
            const int chId = MixerChannelIds::voxInsert (mPageIndex);
            bv->onIsStripRecording = [full, chId]() -> bool
            {
                return full != nullptr && full->isStripRecording (chId);
            };
        }
        // QA-Fa recovery: render is EXPORT ONLY -- the QA-F onPlaceBakedClip
        // install is retired (re-import goes through the Vox ribbon's
        // "+ Add New Vox From Export" flow).

        // QA-Fa recovery: a project-restored applied map may have published
        // before setOwnChannelId resolved the follower id -- republish now
        // that the id is real.
        bv->publishAlignPlayback();

        // QA-Fa recovery: stop-gated auto re-analyze poller (works with the
        // editors closed; dies with the page).
        startTimerHz (4);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// QA-Fa recovery: stop-gated auto re-analyze (locked bundle item 4).
// Grid/tempo changes on the analyzed channel(s) re-run the analysis
// automatically: transport STOPPED -> debounced ~1 s after the signature
// stops moving; transport PLAYING -> pending (the editors' badges show it),
// runs at the next stop.  The version history appended by every analyze is
// the safety net that makes auto re-analysis safe.  Failures are silent
// (badge stays lit); a failed signature is not retried until it changes.
// ─────────────────────────────────────────────────────────────────────────────
void VoxPage::timerCallback()
{
    auto* bv = dynamic_cast<BaySickVocalProcessor*> (mVocalProc.get());
    if (bv == nullptr || mFullProcessor == nullptr) return;

    auto pollOne = [&] (bool stale, juce::int64 curSig, juce::int64& lastSeen,
                        int& stableTicks, juce::int64& lastAttempt,
                        auto&& runAnalyze)
    {
        if (! stale) { stableTicks = 0; lastSeen = curSig; return; }
        if (curSig != lastSeen)
        {
            lastSeen    = curSig;
            stableTicks = 0;
            return;
        }
        if (curSig == lastAttempt) return;   // this exact state already failed
        if (++stableTicks < 4) return;       // ~1 s at 4 Hz
        if (DSPBase::isTransportPlaying()) return;   // stop-gated
        lastAttempt = curSig;
        stableTicks = 0;
        runAnalyze();
    };

    // Align: only re-runs an analysis that exists; combined leader+follower
    // signature so either channel's change (or a tempo edit) trips it.
    if (bv->mAlignState.analyzed)
    {
        const int leader   = bv->resolveLeaderChannel();
        const int follower = bv->resolveFollowerChannel();
        const juce::int64 sig =
            (leader >= 0 ? mFullProcessor->channelClipSignature (leader) : 0)
            ^ (follower >= 0
               ? (mFullProcessor->channelClipSignature (follower) * 31) : 0);
        pollOne (bv->isAlignStale(), sig, mAlignAutoLastSig, mAlignAutoStable,
                 mAlignAutoAttempted,
                 [&] { juce::String err; bv->analyzeAlign (err); });
    }

    if (bv->mPitch.isAnalyzed())
    {
        const juce::int64 sig =
            mFullProcessor->channelClipSignature (bv->getOwnChannelId());
        pollOne (bv->isPitchStale(), sig, mPitchAutoLastSig, mPitchAutoStable,
                 mPitchAutoAttempted,
                 [&] { juce::String err; bv->analyzePitch (err); });
    }
}

// QA-E Task 4 (2026-05-12): setClipFilePath deleted.  Vox file-association
// lives in PatternManager AudioLibrary via pageOwnerChannelId tagging.

// ─────────────────────────────────────────────────────────────────────────────
// G-6 (2026-04-29): full-state export/import for Duplicate flow.
// BaySickVocal slot reserved for Phase H - only BaySickPlayer is cloned for
// now.  Same prefix-substitution shape as ClipsPage (vox_<idx>_* prefix).
// ─────────────────────────────────────────────────────────────────────────────
juce::String VoxPage::exportVoxState() const
{
    juce::XmlElement el ("VoxPageState");
    el.setAttribute ("active",  (int) mEngineType);
    el.setAttribute ("pageIdx", mPageIndex);
    el.setAttribute ("locked",  mLocked ? 1 : 0);

    // H-6b (2026-05-01): export BaySickVocal state.  BaySickPlayer state on
    // Vox tabs no longer exists -- old projects with PlayerState chunks load
    // via importVoxState which silently discards them.
    if (mVocalProc != nullptr)
    {
        juce::MemoryBlock mb;
        mVocalProc->getStateInformation (mb);
        auto* sub = el.createNewChildElement ("VocalState");
        sub->setAttribute ("data",   mb.toBase64Encoding());
        sub->setAttribute ("prefix", "bsv_");
    }

    return el.toString (juce::XmlElement::TextFormat().singleLine());
}

void VoxPage::importVoxState (const juce::String& xml)
{
    if (xml.isEmpty()) return;
    auto parsed = juce::XmlDocument::parse (xml);
    if (! parsed || ! parsed->hasTagName ("VoxPageState")) return;

    // H-6b: BaySickVocal state restore.
    if (auto* vocalEl = parsed->getChildByName ("VocalState"))
    {
        if (mVocalProc == nullptr) selectEngine (EngineType::BaySickVocal);
        if (mVocalProc != nullptr)
        {
            juce::MemoryBlock mb;
            if (mb.fromBase64Encoding (vocalEl->getStringAttribute ("data")))
            {
                mSuppressDirty = true;
                mVocalProc->setStateInformation (mb.getData(), (int) mb.getSize());
                mSuppressDirty = false;
            }
        }
    }
    // Old PlayerState chunks from pre-H-6b projects are silently ignored.

    // G-6: restore lock state.
    setLocked (parsed->getIntAttribute ("locked", 0) != 0);

    // QA-E Task 4 (2026-05-12): removed dead `if (mClipPath.isNotEmpty())
    // setClipFilePath(mClipPath)` -- mClipPath was never serialized so
    // the field was always empty at this point; the call was dead.
}

void VoxPage::paint (juce::Graphics& g)
{
    // H-6b (2026-05-01): page is just a thin host for BaySickVocalEditor;
    // editor paints its own background.  No header bar anymore.
    g.fillAll (juce::Colour (0xff181818));
}

void VoxPage::resized()
{
    // H-6b (2026-05-01): no in-page header.  Engine picker is gone, clip
    // file label moved to PageMenuBar's right slot.  BaySickVocalEditor
    // fills the entire VoxPage.
    layoutEditor (getLocalBounds());
}

void VoxPage::layoutEditor (juce::Rectangle<int> r)
{
    if (auto* ed = activeEditor(); ed && ed->isVisible())
        ed->setBounds (r);
    // J-6 EQ unification (2026-05-03): mEQDisplay removed; no outer layout needed.
}

// ─────────────────────────────────────────────────────────────────────────────
// VoxEmptyState - text-only placeholder.  Mixer button is the spawn trigger.
// ─────────────────────────────────────────────────────────────────────────────
VoxEmptyState::VoxEmptyState()
{
    setInterceptsMouseClicks (false, false);
}

void VoxEmptyState::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181818));

    auto bounds = getLocalBounds().toFloat().reduced (32.0f);
    juce::Path p;
    p.addRoundedRectangle (bounds, 12.0f);
    juce::Path dashed;
    const float dashes[] = { 8.0f, 6.0f };
    juce::PathStrokeType (2.0f).createDashedStroke (dashed, p, dashes, 2);
    g.setColour (juce::Colour (0xff0fafa5).withAlpha (0.6f));
    g.fillPath (dashed);

    g.setColour (juce::Colour (0xffc0c0c0));
    g.setFont (juce::Font (18.0f, juce::Font::plain));
    g.drawText (
        "Click 'Add Vox Strip' on the Mixer page to start a Vox page",
        bounds.toNearestInt(),
        juce::Justification::centred,
        true);
}
