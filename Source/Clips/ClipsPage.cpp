#include "ClipsPage.h"
#include "../VibePlayer/VibePlayerProcessor.h"
#include "../Standalone/EnginePrefixUtil.h"
#include "../Standalone/PagePresetIO.h"
#include "../PluginProcessor.h"
#include "../SampleLibrary.h"
#include "../VibeGraph.h"           // QA-E Task 4: MixerChannelIds::audioInsert
#include "../PatternManager.h"      // QA-E Task 4: addAudioToLibrary

namespace
{
    constexpr int kHeaderRowH = 36;     // engine picker + filename strip at top of page
    constexpr int kPad        = 12;
    constexpr int kPickerW    = 220;
    constexpr int kFilenameW  = 320;
}

// ─────────────────────────────────────────────────────────────────────────────
// ClipsPage (G-6 cleanup 2026-04-29: BaySickPlayer-only.  NAM/IR removed.)
// ─────────────────────────────────────────────────────────────────────────────

ClipsPage::ClipsPage (int pageIndex)
    : mPageIndex (pageIndex),
      mTabName   ("Clip " + juce::String (pageIndex + 1))
{
    // G-7: wire dirty listener pointers (the listener itself attaches to
    // the engine's apvts.state in selectEngine after engine creation).
    mDirtyListener.dirtyFlag = &mPageDirty;
    mDirtyListener.suppress  = &mSuppressDirty;

    buildEnginePicker();
    // J-6 EQ unification (2026-05-03): EQ sub-tab removed; pre+post EQ on Effects page.

    addAndMakeVisible (mClipFileLabel);
    mClipFileLabel.setJustificationType (juce::Justification::centredLeft);
    mClipFileLabel.setColour (juce::Label::textColourId,        juce::Colour (0xff66ff88));
    mClipFileLabel.setColour (juce::Label::backgroundColourId,  juce::Colour (0xff1a1a1a));
    mClipFileLabel.setColour (juce::Label::outlineColourId,     juce::Colour (0xff333333));
    mClipFileLabel.setBorderSize ({ 2, 6, 2, 6 });
    mClipFileLabel.setText ("(no clip)", juce::dontSendNotification);
    mClipFileLabel.setTooltip ("The audio file this Clips tab is bound to.  "
                                "Set by drag/drop onto Builder or onto the Clips empty-state page.");

    switchTab (0);
}

ClipsPage::~ClipsPage()
{
    // G-7: drop the listener before the engine processor is destroyed.
    detachDirtyListener();
}

void ClipsPage::buildEnginePicker()
{
    // G-6 (2026-04-29): single-engine page (BaySickPlayer only).  The picker
    // is shown for visual consistency with Vox/Inst (and to host the
    // right-click context menu) but locked - left-click opens the dropdown
    // showing the one option, right-click opens the page context menu.
    addAndMakeVisible (mEnginePicker);
    mEnginePicker.addItem ("BaySickPlayer", 1);
    mEnginePicker.setSelectedId (1, juce::dontSendNotification);
    mEnginePicker.setTooltip (
        "Clips uses BaySickPlayer (sample playback) - the only engine for clips.  "
        "Right-click for tab options (Rename / Duplicate / Delete).");
    // Single option - no need for an onChange handler.
    mEnginePicker.onRightClick = [this] { showEngineContextMenu(); };
}

// ─────────────────────────────────────────────────────────────────────────────
// G-6 helpers - preset folder + recursive folder→submenu walker (mirrors the
// addLayerPresetDirToMenu pattern from LayersPage.cpp).
// ─────────────────────────────────────────────────────────────────────────────
// 2026-05-05 consolidation: route every Clip preset through PagePresetIO's
// per-kind directory ("Clip Page/My Presets") so saved files appear in the
// load submenu (the previous `Clips/` literal didn't match where save wrote).
static juce::File clipsPresetsRootDir()
{
    return PagePresetIO::presetsDirForPageKind (PagePresetIO::PageKind::Clip);
}

static juce::File clipsMyPresetsDir()
{
    return PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Clip);
}

static void addClipsPresetDirToMenu (juce::PopupMenu& menu,
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
        addClipsPresetDirToMenu (subMenu, sub, kPresetBase, presetXmls);
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

void ClipsPage::showEngineContextMenu()
{
    constexpr int kIdLock           = 1;
    constexpr int kIdRename         = 5;
    constexpr int kIdDuplicate      = 12;
    constexpr int kIdSaveAs         = 20;
    constexpr int kIdChokeBase      = 200;     // 200 = None, 201..216 = Group 1..16
    constexpr int kIdLoadPresetBase = 500;
    constexpr int kIdDelete         = 99;

    juce::PopupMenu menu;
    menu.addItem (kIdLock, "Lock", true, mLocked);

    menu.addSeparator();
    menu.addItem (kIdRename,    "Rename...");
    menu.addItem (kIdDuplicate, "Duplicate Clip (new tab)");

    menu.addSeparator();
    // Choke Group submenu - same model as Layers/Bass/Drums.
    {
        const int curGroup = onGetChokeGroup ? juce::jlimit (0, 16, onGetChokeGroup()) : 0;
        juce::PopupMenu chokeSub;
        chokeSub.addItem (kIdChokeBase, "None", true, curGroup == 0);
        for (int g = 1; g <= 16; ++g)
            chokeSub.addItem (kIdChokeBase + g, "Group " + juce::String (g),
                              true, curGroup == g);
        menu.addSubMenu ("Choke Group", chokeSub);
    }

    menu.addSeparator();
    menu.addItem (kIdSaveAs, "Save Current Patch As...", mPlayerProc != nullptr);

    // Load Preset submenu - walks Documents/BaySickDAW/Presets/Clips/.
    juce::Array<juce::File> presetXmls;
    {
        juce::PopupMenu loadSub;
        const auto root = clipsPresetsRootDir();
        if (root.isDirectory())
            addClipsPresetDirToMenu (loadSub, root, kIdLoadPresetBase, presetXmls);
        if (presetXmls.isEmpty())
            loadSub.addItem (-1, "(no presets installed)", false, false);
        menu.addSubMenu ("Load Preset", loadSub);
    }

    menu.addSeparator();
    menu.addItem (kIdDelete, "Delete Clip", ! mLocked);

    juce::Component::SafePointer<ClipsPage> self (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&mEnginePicker),
        [self, presetXmls = std::move (presetXmls), kIdLoadPresetBase, kIdChokeBase] (int r)
        {
            if (! self || r <= 0) return;
            if (r == kIdLock)        { self->setLocked (! self->mLocked); return; }
            if (r == kIdRename    && self->onRenameRequested)    { self->onRenameRequested();    return; }
            if (r == kIdDuplicate && self->onDuplicateRequested) { self->onDuplicateRequested(); return; }
            if (r == kIdSaveAs)      { self->savePagePreset(); return; }   // G-7: full chain
            if (r == kIdDelete)      { self->requestDelete();  return; }   // G-7: prompt before delete
            if (r >= kIdChokeBase && r <= kIdChokeBase + 16)
            {
                if (self->onSetChokeGroup) self->onSetChokeGroup (r - kIdChokeBase);
                return;
            }
            if (r >= kIdLoadPresetBase
                && r <  kIdLoadPresetBase + presetXmls.size())
            {
                self->loadPagePreset (presetXmls[r - kIdLoadPresetBase]);   // G-7: full chain
                return;
            }
        });
}

void ClipsPage::savePatchAs()
{
    auto* aw = new juce::AlertWindow ("Save Clip Preset",
                                       "Enter a name for this Clip preset:",
                                       juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", "My Clip");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<ClipsPage> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, aw] (int r)
        {
            if (r != 1 || ! safeThis) return;
            const juce::String name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;

            auto dir = clipsMyPresetsDir();
            dir.createDirectory();
            auto target = dir.getChildFile (name + ".xml");
            // Auto-suffix on conflict instead of overwriting.
            int n = 2;
            while (target.exists())
                target = dir.getChildFile (name + " (" + juce::String (n++) + ").xml");

            const juce::String xml = safeThis->exportClipState();
            if (xml.isNotEmpty())
                target.replaceWithText (xml);
        }), true);
}

void ClipsPage::loadPreset (const juce::File& xml)
{
    if (! xml.existsAsFile()) return;
    const juce::String contents = xml.loadFileAsString();
    if (contents.isNotEmpty())
        importClipState (contents);
}

// ─────────────────────────────────────────────────────────────────────────────
// G-7 (2026-04-29): Page Preset save/load (full chain).
// ─────────────────────────────────────────────────────────────────────────────
void ClipsPage::takeStateSnapshot()
{
    // G-7: listener-based dirty tracking - just clear the flag.  Subsequent
    // parameter changes (sliders, combos, toggles in the engine editor) flip
    // it back to true via mDirtyListener.
    mPageDirty = false;
}

void ClipsPage::attachDirtyListener()
{
    detachDirtyListener();
    if (auto* vp = dynamic_cast<VibePlayerProcessor*> (mPlayerProc.get()))
        vp->apvts.state.addListener (&mDirtyListener);
}

void ClipsPage::detachDirtyListener()
{
    if (auto* vp = dynamic_cast<VibePlayerProcessor*> (mPlayerProc.get()))
        vp->apvts.state.removeListener (&mDirtyListener);
}

void ClipsPage::savePagePreset (std::function<void()> onSaved)
{
    if (mFullProcessor == nullptr || mPlayerProc == nullptr)
    {
        savePatchAs();   // fallback to engine-only if processor not yet wired
        if (onSaved) onSaved();   // chain anyway so caller can complete
        return;
    }

    auto* aw = new juce::AlertWindow ("Save Page Preset",
                                       "Enter a name for this clip page preset:",
                                       juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", "My Clip");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<ClipsPage> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, aw, onSaved] (int r)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (r != 1 || ! safeThis) return;

            const juce::String name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;

            auto dir = PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Clip);
            dir.createDirectory();
            auto target = dir.getChildFile (name + ".xml");
            int n = 2;
            while (target.exists())
                target = dir.getChildFile (name + " (" + juce::String (n++) + ").xml");

            const juce::String stripPrefix = "mixer_audio_" + juce::String (safeThis->mPageIndex);
            juce::String enginePrefix;
            if (auto* vp = dynamic_cast<VibePlayerProcessor*> (safeThis->mPlayerProc.get()))
                enginePrefix = vp->getParamPrefix();

            const juce::String xml = PagePresetIO::exportPagePreset (
                *safeThis->mFullProcessor,
                PagePresetIO::PageKind::Clip,
                safeThis->mPageIndex,
                stripPrefix,
                safeThis->mPlayerProc.get(),
                "BaySickPlayer",
                enginePrefix);

            if (xml.isEmpty()) return;

            // G-7 (2026-04-29): copy the attached audio file to
            // Documents/BaySickDAW/My Samples (if not already there) and
            // embed a relative path in the preset XML so the file travels
            // with the preset across projects.  Without this, loading a Clip
            // preset on a different project leaves the engine pointed at a
            // path that may not exist there.
            juce::String clipRefRel;
            if (safeThis->mClipPath.isNotEmpty())
            {
                SampleLibrary::ensureUserSamplesDir();
                const juce::File mySamples = SampleLibrary::getUserSamplesDir();
                const juce::File source (safeThis->mClipPath);
                if (source.existsAsFile())
                {
                    juce::File dest = mySamples.getChildFile (source.getFileName());
                    // Auto-suffix on collision so we never overwrite a user
                    // sample (no-file-delete contract).
                    int sfx = 2;
                    while (dest.existsAsFile()
                            && dest.getFullPathName() != source.getFullPathName())
                    {
                        dest = mySamples.getChildFile (
                            source.getFileNameWithoutExtension()
                            + " (" + juce::String (sfx++) + ")"
                            + source.getFileExtension());
                    }
                    if (dest.getFullPathName() == source.getFullPathName()
                            || source.copyFileTo (dest))
                    {
                        clipRefRel = dest.getFileName();   // relative to My Samples
                    }
                }
            }

            // Inject the clip reference into the saved XML before writing.
            if (auto parsed = juce::XmlDocument::parse (xml))
            {
                if (clipRefRel.isNotEmpty())
                    parsed->setAttribute ("clipRef", clipRefRel);
                parsed->writeTo (target, {});
            }
            else
            {
                target.replaceWithText (xml);
            }

            safeThis->takeStateSnapshot();
            if (onSaved) onSaved();   // G-7: chain delete after save completes
        }), false);
}

void ClipsPage::loadPagePreset (const juce::File& xml)
{
    if (! xml.existsAsFile()) return;
    if (mFullProcessor == nullptr) { loadPreset (xml); return; }

    if (mPlayerProc == nullptr) selectEngine (EngineType::BaySickPlayer);

    const juce::String stripPrefix = "mixer_audio_" + juce::String (mPageIndex);
    juce::String enginePrefix;
    if (auto* vp = dynamic_cast<VibePlayerProcessor*> (mPlayerProc.get()))
        enginePrefix = vp->getParamPrefix();

    auto noFallback = [] (int) { return true; };

    const juce::String contents = xml.loadFileAsString();
    // G-7: suppress dirty flag during the bulk state restore.
    mSuppressDirty = true;
    PagePresetIO::importPagePreset (*mFullProcessor,
                                     PagePresetIO::PageKind::Clip,
                                     mPageIndex,
                                     stripPrefix,
                                     mPlayerProc.get(),
                                     enginePrefix,
                                     noFallback,
                                     contents);
    mSuppressDirty = false;

    // G-7 (2026-04-29): if the preset embedded a clipRef pointing to a file
    // in My Samples, resolve it and bind the clip path so the page actually
    // plays the original audio.  Falls through silently if the reference is
    // missing (older engine-only preset) or the file vanished.
    if (auto parsed = juce::XmlDocument::parse (contents))
    {
        const juce::String clipRefRel = parsed->getStringAttribute ("clipRef");
        if (clipRefRel.isNotEmpty())
        {
            const juce::File ref =
                SampleLibrary::getUserSamplesDir().getChildFile (clipRefRel);
            if (ref.existsAsFile())
                setClipFilePath (ref.getFullPathName());
        }
    }

    takeStateSnapshot();
}

void ClipsPage::showPageActionsMenu (juce::Component* anchor)
{
    constexpr int kIdSavePagePreset = 100;
    constexpr int kIdLoadBase       = 1000;

    juce::PopupMenu menu;
    menu.addItem (kIdSavePagePreset, "Save Page Preset As...",
                  mPlayerProc != nullptr);

    juce::Array<juce::File> presetXmls;
    {
        juce::PopupMenu loadSub;
        const auto root = PagePresetIO::myPresetsDirForPageKind (PagePresetIO::PageKind::Clip);
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

    juce::Component::SafePointer<ClipsPage> safeThis (this);
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

void ClipsPage::requestDelete()
{
    juce::Component::SafePointer<ClipsPage> safeThis (this);
    auto fireDelete = [safeThis] {
        if (auto* p = safeThis.getComponent())
            if (p->onDeleteRequested) p->onDeleteRequested();
    };

    const juce::String warning =
        "Deleting this clip removes its Player, Mixer Strip, "
        "Effects Rack, and Piano Roll.\n"
        "Audio files in your project's Audio folder will be kept.";

    if (mPlayerProc != nullptr && isPatchDirty())
    {
        const juce::String dirtyWarning = warning
            + "\n\n\"Save Page Preset & Delete\" writes the entire page state "
              "(engine + EQ + effects rack + strip settings, plus a copy of "
              "the attached audio in My Samples) to disk first, then deletes "
              "the tab.";

        auto* aw = new juce::AlertWindow (
            "Delete Clip", dirtyWarning, juce::AlertWindow::QuestionIcon);
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
        "Delete Clip", warning, juce::AlertWindow::WarningIcon);
    aw->addButton ("Delete", 1);
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [aw, fireDelete] (int r)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (r == 1) fireDelete();
        }), false);
}

// J-6 EQ unification (2026-05-03): buildEQTab removed; Audio insert pre-rack EQ
// (mixer_audio_<row>_preeq_*) is exclusively edited via the Effects page.

// ─────────────────────────────────────────────────────────────────────────────
// Engine activation.  Single-engine page (BaySickPlayer only).  selectEngine
// is the entry point spawnClipsTabIfMissing calls AFTER wiring callbacks so
// onEngineDestroying / onEngineChanged fire correctly.  Passing None is a
// no-op; passing BaySickPlayer lazy-creates the processor + editor.
// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessor* ClipsPage::getEngineProcessor() const noexcept
{
    return mPlayerProc.get();
}

void ClipsPage::selectEngine (EngineType e)
{
    if (e == mEngineType) return;
    if (e == EngineType::None) return;   // can't deactivate

    if (onEngineDestroying) onEngineDestroying();

    if (e == EngineType::BaySickPlayer && ! mPlayerProc)
    {
        const juce::String prefix = "clip_" + juce::String (mPageIndex) + "_";
        auto vp = std::make_unique<VibePlayerProcessor> (prefix);
        vp->prepareToPlay (44100.0, 512);
        if (mClipPath.isNotEmpty())
            vp->loadSampleFile (juce::File (mClipPath));
        mPlayerProc = std::move (vp);
        mPlayerEditor.reset (mPlayerProc->createEditor());
        if (mPlayerEditor) addChildComponent (*mPlayerEditor);
    }

    mEngineType = e;
    if (mPlayerEditor) mPlayerEditor->setVisible (mActiveTab == 0);
    resized();
    repaint();

    // G-7 polish (2026-04-29): bind Pre EQ8 M/S display to the Audio
    // InsertNode's preEq + mixer_audio_<row>_preeq_(mid|side)_eq APVTS prefix.
    // Audio InsertNode was created by ensureAudioInsert when the clip was
    // first dropped onto Builder; getInsertPreEQ is non-null here.
    //
    // (2026-04-29 BISECT cleared: the routing regression was the strip-create
    // ordering in StandaloneEditor::onAudioClipAdded - addAudioChannel ran
    // before ensureAudioInsert so the strip's APVTS attachments silently
    // failed.  Fixed there + a defensive rebindApvts in
    // VibeGraph::ensureInsertNode.  Pre-EQ binding is back on.)
    // J-6 EQ unification (2026-05-03): page-level EQ display removed; pre-rack
    // EQ is bound exclusively by EffectsPage (mixer_audio_<row>_preeq_*).

    // G-7 (2026-04-29): hook dirty-tracker on the new engine's apvts and
    // reset the flag.  ANY subsequent parameter change in the editor flips
    // the flag, so requestDelete's prompt picks up dirty state reliably.
    attachDirtyListener();
    takeStateSnapshot();

    if (onEngineChanged) onEngineChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// Sub-tab + clip path.
// ─────────────────────────────────────────────────────────────────────────────
void ClipsPage::switchTab (int idx)
{
    // J-6 EQ unification (2026-05-03): EQ tab removed.  Tabs: 0=Player, 1=Piano Roll redirect.
    mActiveTab = juce::jlimit (0, 1, idx);

    if (mPlayerEditor) mPlayerEditor->setVisible (mActiveTab == 0);
    // mActiveTab == 1 = Piano Roll redirect handled by StandaloneEditor before
    // it ever reaches this page.

    resized();
    repaint();
}

void ClipsPage::setClipFilePath (const juce::String& p)
{
    mClipPath = p;
    mClipFileLabel.setText (p.isNotEmpty()
                                ? juce::File (p).getFileName()
                                : juce::String ("(no clip)"),
                            juce::dontSendNotification);

    if (auto* vp = dynamic_cast<VibePlayerProcessor*> (mPlayerProc.get()))
        if (p.isNotEmpty())
            vp->loadSampleFile (juce::File (p));

    // QA-E Task 4 (2026-05-12): in addition to the engine preload above,
    // tag the audio library entry's pageOwnerChannelId so the browser walk
    // groups this file under the Clips category for this page.  mClipPath
    // stays as the "currently preloaded sample for the engine" (deletable
    // post-QA-J Clips routing unification); library tracks ALL N files
    // routed to this page for multi-file browser visibility.
    if (p.isNotEmpty() && mFullProcessor != nullptr)
    {
        if (auto* pm = mFullProcessor->getPatternManager())
        {
            const int ownerCh = MixerChannelIds::audioInsert (mPageIndex);
            pm->addAudioToLibrary (p, {}, ownerCh);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// G-6 (2026-04-29): full-state export/import for Duplicate flow.
// Single-engine page: just BaySickPlayer state.  Saves the actual processor
// prefix (`getParamPrefix()`) so import can substitute correctly - the
// VibePlayerProcessor prefix format is `tk_<trackId>_bsp_` which collapses
// to e.g. `tk_clip_5__bsp_` when trackId already ends in `_`, so we can't
// reconstruct it from page index alone.
// ─────────────────────────────────────────────────────────────────────────────
juce::String ClipsPage::exportClipState() const
{
    juce::XmlElement el ("ClipPageState");
    el.setAttribute ("active",  (int) mEngineType);
    el.setAttribute ("pageIdx", mPageIndex);
    el.setAttribute ("locked",  mLocked ? 1 : 0);

    if (mPlayerProc != nullptr)
    {
        juce::MemoryBlock mb;
        mPlayerProc->getStateInformation (mb);
        auto* sub = el.createNewChildElement ("PlayerState");
        sub->setAttribute ("data", mb.toBase64Encoding());
        if (auto* vp = dynamic_cast<VibePlayerProcessor*> (mPlayerProc.get()))
            sub->setAttribute ("prefix", vp->getParamPrefix());
    }
    return el.toString (juce::XmlElement::TextFormat().singleLine());
}

void ClipsPage::importClipState (const juce::String& xml)
{
    if (xml.isEmpty()) return;
    auto parsed = juce::XmlDocument::parse (xml);
    if (! parsed || ! parsed->hasTagName ("ClipPageState")) return;

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
                // G-7: suppress dirty flag during the bulk state restore so
                // the imported state doesn't immediately register as dirty.
                mSuppressDirty = true;
                mPlayerProc->setStateInformation (mb.getData(), (int) mb.getSize());
                mSuppressDirty = false;
            }
        }
    }

    // Old projects with a NamIrState child (pre-G-6 cleanup) - silently skip.
    // Backward-compat: state still loads, just without the NAM/IR bits.

    // G-6: restore lock state (Duplicate preserves; Load Preset would too).
    setLocked (parsed->getIntAttribute ("locked", 0) != 0);

    // Re-load this page's clip file so the cloned state's saved sample path
    // doesn't override the destination's file.  loadSampleFile updates
    // kLoadPathProp on the processor's apvts state.
    if (mClipPath.isNotEmpty())
        setClipFilePath (mClipPath);
}

// ─────────────────────────────────────────────────────────────────────────────
// Paint + layout.
// ─────────────────────────────────────────────────────────────────────────────
void ClipsPage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181818));
    g.setColour (juce::Colour (0xff0a0a0a));
    g.fillRect (0, 0, getWidth(), kHeaderRowH);
    g.setColour (juce::Colour (0xff333333));
    g.fillRect (0, kHeaderRowH, getWidth(), 1);
}

void ClipsPage::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (kHeaderRowH).reduced (kPad, 6);
    mEnginePicker.setBounds (header.removeFromLeft (kPickerW));
    header.removeFromLeft (kPad);
    mClipFileLabel.setBounds (header.removeFromLeft (juce::jmin (kFilenameW, header.getWidth())));
    layoutEditor (r);
}

void ClipsPage::layoutEditor (juce::Rectangle<int> r)
{
    if (mPlayerEditor && mPlayerEditor->isVisible())
        mPlayerEditor->setBounds (r);
}

// ─────────────────────────────────────────────────────────────────────────────
// ClipsEmptyState - placeholder when zero Clips tabs exist.
// ─────────────────────────────────────────────────────────────────────────────
ClipsEmptyState::ClipsEmptyState()
{
    setInterceptsMouseClicks (true, false);
}

void ClipsEmptyState::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181818));

    auto bounds = getLocalBounds().toFloat().reduced (32.0f);
    juce::Path p;
    p.addRoundedRectangle (bounds, 12.0f);
    juce::Path dashed;
    const float dashes[] = { 8.0f, 6.0f };
    juce::PathStrokeType (2.0f).createDashedStroke (dashed, p, dashes, 2);
    g.setColour (juce::Colour (mFileHover ? 0xffd4a017 : 0xff6a500b));
    g.fillPath (dashed);

    g.setColour (juce::Colour (0xffc0c0c0));
    g.setFont (juce::Font (18.0f, juce::Font::plain));
    g.drawText (
        "Drag an audio file (.wav .mp3 .aiff .flac .ogg .aif) to start a Clips tab",
        bounds.toNearestInt(),
        juce::Justification::centred,
        true);
}

bool ClipsEmptyState::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        const auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".mp3" || ext == ".aiff" || ext == ".aif"
            || ext == ".flac" || ext == ".ogg")
            return true;
    }
    return false;
}

void ClipsEmptyState::filesDropped (const juce::StringArray& files, int, int)
{
    mFileHover = false;
    repaint();
    if (files.isEmpty()) return;
    if (onClipDropped) onClipDropped (files[0]);
}
