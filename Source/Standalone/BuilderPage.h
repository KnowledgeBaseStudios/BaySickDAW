#pragma once
#include "UndoActions.h"
#include <JuceHeader.h>
#include <deque>
#include "../PluginProcessor.h"
#include "../PatternManager.h"
#include "../DSP/LoudnessSpec.h"
#include "SharedUI.h"
#include "StandaloneApp.h"

// ── BrowserItem ───────────────────────────────────────────────────────────────
// A single draggable box in the Browser panel. Used for all three tabs
// (Patterns / Audio / Automation) - same UI, same interactions.
//
//  • Single-click  → onClicked    (e.g. select pattern)
//  • Double-click  → onRenameRequested (panel opens floating rename input)
//  • Right-click   → onContextMenu      (panel shows Rename/Delete/Duplicate)
//  • Drag         → starts a JUCE DragAndDrop with description "kind:index"
//                    so ArrangementGrid::itemDropped can create the clip.
class BrowserItem : public juce::Component
{
public:
    // Audio is NOT a member: since QA-E Task 4's library-driven browser, audio
    // rows are AudioBrowserItem (its own class, its own tree path), and no
    // BrowserItem was ever constructed with an Audio kind again.  The dead
    // enumerator and its five unreachable branches went at QA-Cleanup.
    enum class Kind { Pattern, Automation };

    BrowserItem(Kind k, int index, const juce::String& displayName);

    void paint(juce::Graphics&) override;
    void mouseDown      (const juce::MouseEvent&) override;
    void mouseDrag      (const juce::MouseEvent&) override;
    void mouseUp        (const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    void setSelected(bool s)     { mSelected = s; repaint(); }
    void setAccentColour(juce::Colour c) { mAccent = c; repaint(); }

    const juce::String& getDisplayName() const { return mName; }
    Kind getKind()  const { return mKind; }
    int  getIndex() const { return mIndex; }

    std::function<void()>                   onClicked;
    std::function<void()>                   onRenameRequested;
    std::function<void(juce::Point<int>)>   onContextMenu;

private:
    Kind         mKind;
    int          mIndex;
    juce::String mName;
    juce::Colour mAccent { juce::Colour(0xff6080a0) };
    bool         mSelected { false };
};

// ── G-5 (2026-04-29): Audio browser unified view ─────────────────────────────
// CategorizedAudioEntry - one record per file the browser shows in the Files
// tree.  StandaloneEditor's onEnumerateAudio callback walks mPages and emits
// one entry per ClipsPage / VoxPage / InstPage with a bound file.  Orphan
// audioLibrary entries (no bound page) are skipped - there is NO "Imported" /
// "Library" bucket per Jeff's invariant ("all importable files become clips").
//
// TS7 §11.4 AMENDS THE SCOPE OF THAT INVARIANT, deliberately and on the record.
// It governs IMPORT material: anything importable becomes a clip rather than
// sitting in a library bucket.  The Exports and Reports categories added in TS7
// are OUTPUT - files the user just rendered, listed so they do not have to go
// hunting for them - and they are populated from the project's Exports\ and
// Reports\ folders rather than from audioLibrary.  Nothing is auto-added to the
// grid; right-click routes through the same showItemContextMenu flow clips use.
// The freeze cache is excluded: regenerable, not something the user made.
struct CategorizedAudioEntry
{
    int          audioLibIdx { -1 };   // index into mPM.audioLibrary for drag descriptor
    juce::String category;             // "Clips" | "Vox" | "Inst"
    juce::String displayName;          // shown as leaf label
    juce::String fullPath;             // for tooltip + Reveal in Explorer
    juce::Colour accent;               // category accent color
    juce::String groupName;            // QA-Fe2: manual group ("" = auto/flat)
};

// QA-E Task 7 (FILE-02): one routable target for the "Routes to:" dropdown
// (used by BOTH the per-clip Audio Clip Properties dialog and the browser
// entry Properties dialog).  channelId is a MixerChannelIds id (voxInsert /
// instInsert / audioInsert); displayName is the page's ribbon tab name.
// Declared here (ahead of BrowserPanel + ArrangementGrid) so both can use it.
struct RoutablePageInfo
{
    int          channelId { 0 };
    juce::String displayName;
};

// AudioBrowserItem - TreeViewItem leaf for a single audio file.  Replaces
// the per-file BrowserItem in the flat-list world.  Drag descriptor matches
// the existing format ("audio:<libIdx>") so ArrangementGrid::itemDropped
// keeps working unchanged.
class AudioBrowserItem : public juce::TreeViewItem
{
public:
    AudioBrowserItem (const CategorizedAudioEntry& e);

    bool          mightContainSubItems () override               { return false; }
    int           getItemHeight        () const override         { return 26; }
    juce::String  getUniqueName        () const override         { return mEntry.category + ":" + juce::String(mEntry.audioLibIdx); }
    juce::String  getTooltip           () override               { return mEntry.fullPath; }
    // TS7 §11.5: an Exports/Reports leaf is NOT a library entry, so it has no
    // index to drag.  It carries its PATH instead ("render:<abs path>"), which
    // ArrangementGrid handles by importing first and then running the same
    // route-to-a-tab prompt the context menu uses.  Without this the descriptor
    // would read "audio:-1" and the drop would silently die on a bounds check.
    juce::var     getDragSourceDescription () override
    {
        return mEntry.audioLibIdx >= 0
                 ? juce::String ("audio:")  + juce::String (mEntry.audioLibIdx)
                 : juce::String ("render:") + mEntry.fullPath;
    }
    bool          canBeSelected        () const override         { return true; }
    void          paintItem            (juce::Graphics&, int, int) override;
    void          itemClicked          (const juce::MouseEvent&) override;

    int  getAudioLibIdx () const { return mEntry.audioLibIdx; }
    const juce::String& getDisplayName () const { return mEntry.displayName; }
    const juce::String& getFullPath    () const { return mEntry.fullPath;    }

    // Wired by BrowserPanel so right-click can route into the existing
    // Choke Group / Delete / Rename context flow.
    std::function<void(juce::Point<int>)> onContextMenu;
    std::function<void()>                 onRenameRequested;
    // QA-H Task 8 (#20): fires on plain single click (drop-type tracking).
    std::function<void()>                 onSelected;

private:
    CategorizedAudioEntry mEntry;
};

// AudioRootItem - invisible root holding the 3 category nodes.  Concrete
// subclass needed because juce::TreeViewItem::mightContainSubItems is pure
// virtual.
class AudioRootItem : public juce::TreeViewItem
{
public:
    bool          mightContainSubItems () override               { return true; }
    juce::String  getUniqueName        () const override         { return "audio_root"; }
};

// AudioCategoryItem - non-selectable header node holding audio leaves of one
// kind ("Clips" / "Vox" / "Inst").  Click on the triangle expands / collapses;
// click on the label area also toggles open state for usability.
class AudioCategoryItem : public juce::TreeViewItem
{
public:
    AudioCategoryItem (const juce::String& name, juce::Colour accent);

    bool          mightContainSubItems () override               { return getNumSubItems() > 0; }
    int           getItemHeight        () const override         { return 24; }
    juce::String  getUniqueName        () const override         { return "cat:" + mName; }
    bool          canBeSelected        () const override         { return false; }
    void          paintItem            (juce::Graphics&, int, int) override;
    void          itemClicked          (const juce::MouseEvent&) override;

    // QA-Fe2: right-click on the category header ("Create Group...").
    std::function<void(juce::Point<int>)> onContextMenu;

private:
    juce::String mName;
    juce::Colour mAccent;
};

// QA-Fe2: collapsible group node inside a category.  Two flavors: AUTO
// recording groups (derived from " - DRY/WET [CLEANED]" take-tag file names;
// children show only the tag) and MANUAL groups (user-created via the
// category header; children keep their names).  Rename semantics differ:
// auto = disk-rename every take in the group (StandaloneEditor flow),
// manual = label rename only.
class AudioGroupItem : public juce::TreeViewItem
{
public:
    AudioGroupItem (const juce::String& name, juce::Colour accent);

    bool          mightContainSubItems () override               { return true; }
    int           getItemHeight        () const override         { return 24; }
    juce::String  getUniqueName        () const override         { return "grp:" + mName; }
    bool          canBeSelected        () const override         { return false; }
    void          paintItem            (juce::Graphics&, int, int) override;
    void          itemClicked          (const juce::MouseEvent&) override;

    std::function<void(juce::Point<int>)> onContextMenu;

private:
    juce::String mName;
    juce::Colour mAccent;
};

// ── BrowserPanel ──────────────────────────────────────────────────────────────
// Collapsible left panel - 3 filter tabs: Patterns | Audio | Automation.
// Each item is a single draggable BrowserItem (no separate text editor).
class BrowserPanel : public juce::Component
{
    // NOTE: BuilderPage is the DragAndDropContainer - not this panel - so
    // drags started from BrowserItems can be routed to ArrangementGrid (a
    // sibling, not a descendant of this panel). JUCE only routes drags to
    // targets that share the SAME container ancestor.
public:
    explicit BrowserPanel(PatternManager& pm,
                          juce::AudioFormatManager& afm,
                          juce::AudioThumbnailCache& thumbCache);
    // QA-H Task 8 (#17): mAudioTree is declared before mAudioRoot, so
    // reverse-order member destruction kills the root while the TreeView
    // still points at it - drop the root reference first (shutdown UAF).
    ~BrowserPanel() override { if (mAudioTree) mAudioTree->setRootItem (nullptr); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh();

    bool isCollapsed() const { return mCollapsed; }
    void setCollapsed(bool c);

    // Jeff, 2026-08-06: collapsed = width zero.  The 5px edge grip is the only
    // affordance in both states -- drag past the magnetic floor to collapse,
    // drag the (chevron-marked) grip back out to reopen.  The 28px click-strip
    // and its onExpandRequest are gone.

    // Public alias for switchTab - used by BuilderPage::setBrowserTab + ribbon dropdown.
    void selectTab(int t) { switchTab(t); }

    std::function<void(int)>                  onPatternSelected;
    // QA-H Task 8 (#20): last-clicked browser entry -> the grid's active
    // drop type.  tab: 0 = pattern, 1 = audio, 2 = automation; refIdx is
    // the pattern index / audio library index / automation template index
    // (-1 = none picked yet on that tab).
    std::function<void(int tab, int refIdx)>  onDropTypeChanged;
    std::function<void(int)>                  onRenderPattern;   // right-click → Render to WAV
    std::function<void(int)>                  onSplitPattern;    // right-click → Split by Player Engine (QA-G)
    std::function<void()>                     onArrangementChanged; // delete-block triggers → rebuild
    // LIFETIME: a deleted library entry's baked waveform image + AudioThumbnail
    // would otherwise sit in the grid's caches for the rest of the session.
    // Wired by BuilderPage to ArrangementGrid::purgeWaveformCacheFor.
    std::function<void(const juce::String& path)> onPurgeWaveformCache;
    // QA-E Task 5 (2026-05-15): fires when the user deletes the LAST library
    // entry owned by a page (Clips / Vox / Inst).  StandaloneEditor walks
    // mPages to find the owning page by channelId and closes its ribbon tab.
    // Unwired -> no page close happens; entry + blocks already removed.
    std::function<void(int channelId)>        onClosePageForChannelId;
    // Resolves an AutomationLane to a display label (honors userDisplayName,
    // falls back to the auto-generated "Channel - Effect - Param" format).
    // Set by StandaloneEditor; null on construction.
    std::function<juce::String(const AutomationLane&)> onResolveDisplayName;
    // G-5 (2026-04-29): page-walk based enumeration of audio files for the
    // unified Audio tree.  StandaloneEditor wires this with a closure that
    // walks mPages and emits one CategorizedAudioEntry per ClipsPage /
    // VoxPage / InstPage with a bound file.  Returning empty causes the tree
    // to render with empty Clips / Vox / Inst categories (which is normal
    // before any pages exist).
    std::function<std::vector<CategorizedAudioEntry>()> onEnumerateAudio;

    // QA-Fe2 De-noise: re-clean a "* CLEANED" take at a new strength
    // (0 = Light, 1 = Strong).  Returns false on failure (caller shows why).
    std::function<bool(const juce::String& relPath, int strength)> onRegenerateDenoise;

    // QA-Fe2: recording-group rename = disk-rename every take in the group
    // (base swapped, take tags kept) + rewrite all clip/library references.
    // StandaloneEditor owns the flow (file moves need the project folder +
    // player rebuild ordering).  Returns false on failure (flow shows why).
    std::function<bool(const juce::String& oldBase, const juce::String& newBase)>
        onRenameRecordingGroup;
    // G-6 (2026-04-29): right-click "Duplicate..." on an audio tree leaf has
    // already (a) prompted for a name + resolved any filename conflict and
    // (b) physically copied the WAV.  This callback hands BOTH the source
    // path (so the editor can find the source page and capture its full
    // state) AND the new copy's absolute path to StandaloneEditor, which
    // spawns a fresh ClipsPage / VoxPage / InstPage bound to the copy and
    // clones the source page's full state (active engine, all knob values,
    // both engines' APVTS state in the dual-engine A/B case).
    std::function<void(const juce::String& sourceAbsPath,
                       const juce::String& copiedAbsPath)> onDuplicateClipSpawn;
    // QA-E Task 7 (FILE-02): browser-entry "Properties..." (full props box).
    // onEnumerateRoutablePages / onCreateRoutablePage mirror ArrangementGrid's
    // (StandaloneEditor wires both from the same logic so the per-clip and
    // per-library dialogs list identical targets).  onApplyLibraryProperties
    // commits a source-of-truth edit on the library entry: it writes pitch /
    // BPM / stretch-mode + route onto the entry AND propagates all four into
    // every grid copy still following the original (isOverride == false).
    std::function<std::vector<RoutablePageInfo>()>                      onEnumerateRoutablePages;
    std::function<int(int /*kind*/, const juce::String& /*audioPath*/)> onCreateRoutablePage;
    std::function<void(int /*libIdx*/, float /*pitch*/, float /*bpm*/,
                       bool /*stretchMode*/, int /*newRoute*/)>         onApplyLibraryProperties;
    // QA-E Task 7 (FILE-02): "Copy" action.  Physically duplicates srcStored
    // (ProjectManager::importSample auto-numbered de-conflict), adds a new
    // library entry tagged to targetChannel carrying pitch/bpm/stretch, and
    // returns the new stored path ({} on failure).
    //
    // Split into two so "Copy to a new Clip Page" doesn't double-register
    // (the Clips-page spawn auto-adds an entry for its bound file; binding
    // it to the duplicate makes that the single entry, and the explicit
    // tag below dedups to a no-op):
    //   onDuplicateFileForCopy(src) -> np : physical auto-numbered copy
    //     only, NO library entry, NO page.  {} on failure.
    //   onTagCopiedEntry(np, targetChannel, pitch, bpm, stretch) :
    //     addAudioToLibrary(np, targetChannel) [dedup-safe] + clip defaults
    //     + notifyArrangementChanged.
    std::function<juce::String(const juce::String& /*src*/)>            onDuplicateFileForCopy;
    std::function<void(const juce::String& /*np*/, int /*targetChannel*/,
                       float /*pitch*/, float /*bpm*/, bool /*stretch*/)> onTagCopiedEntry;

    // QA-UndoCoverage Task 4: browser pattern/library/template gestures wrap
    // in undo actions.  Wired by BuilderPage::setUndoContext.
    void setUndoContext (const UndoContext& ctx) { mUndoCtx = ctx; }
    // Public rebuild hooks for undo-apply paths that live outside this class
    // (the New Automation Clip rider + the slice-apply wiring in BuilderPage).
    void refreshAutomationTab();
    void refreshPatternTab();
    // Jeff 2026-08-06: master-capture takes land in the Exports folder at
    // record-commit; the editor pokes this so the new file shows immediately.
    void refreshRenderRows();

    // Pattern-list ops cascade into blocks (removePattern re-indexes), so the
    // slice capture/apply lives on the ArrangementGrid; BuilderPage wires
    // these across.
    std::function<PatternListSnapshot()>            onCapturePatternSlice;
    std::function<void(const PatternListSnapshot&)> onApplyPatternSlice;
    // Deleting a pattern also destroys its linked TS markers, which the slice
    // does not carry, so that gesture borrows the grid's marker-aware wrapper
    // instead.  BuilderPage wires this to ArrangementGrid::performPatternTsOp.
    std::function<void(const juce::String& label, const std::function<void()>& op)>
        onPerformPatternTsOp;

private:
    // Wrap one browser gesture as one undo transaction (capture before, run
    // the op, capture after, perform).  Falls back to bare op() when the
    // context or slice hooks are unwired.
    void performPatternSliceOp (const juce::String& label, const std::function<void()>& op);
    // Slice + linked markers in ONE transaction.  Falls back to the slice-only
    // wrapper when the hook is unwired.
    void performPatternTsOp (const juce::String& label, const std::function<void()>& op);
    void performLibraryOp      (const juce::String& label, bool withBlocks,
                                const std::function<void()>& op);
    void performTemplateOp     (const juce::String& label, bool withBlocks,
                                const std::function<void()>& op);

    UndoContext                mUndoCtx;
    PatternManager&            mPM;
    juce::AudioFormatManager&  mAFM;
    juce::AudioThumbnailCache& mThumbCache;
    bool mCollapsed { false };
    int  mActiveTab { 0 };

    std::array<std::unique_ptr<juce::TextButton>, 3> mTabBtns;

    std::vector<std::unique_ptr<BrowserItem>> mPatItems;
    std::vector<std::unique_ptr<BrowserItem>> mAudioItems;     // G-5 legacy - kept empty post-tree migration
    std::vector<std::unique_ptr<BrowserItem>> mAutomItems;
    int mSelectedPat { 0 };
    // QA-H Task 8 (#20): last-clicked audio library idx / automation
    // template idx (per-tab memory for the drop-type callback).
    int mLastAudioSel { -1 };
    int mLastAutomSel { -1 };
    void selectAutomationItem (int itemIdx);

    // G-5 (2026-04-29): unified audio tree replacing the flat mAudioItems
    // when Audio tab is active.  Owns a hidden root TreeViewItem populated
    // with 3 category nodes (Clips / Vox / Inst) - leaves come from
    // onEnumerateAudio().  Tree visibility tracks mActiveTab == 1.
    std::unique_ptr<juce::TreeView>           mAudioTree;
    std::unique_ptr<juce::TreeViewItem>       mAudioRoot;
    AudioCategoryItem*                        mClipsCat { nullptr };  // raw - owned by mAudioRoot
    AudioCategoryItem*                        mVoxCat   { nullptr };
    AudioCategoryItem*                        mInstCat  { nullptr };
    // TS7 §11.2/§11.3: what the user has rendered.  Populated from the project's
    // Exports\ and Reports\ folders, NOT from the audio library -- they have no
    // bound page and are not clips.
    AudioCategoryItem*                        mExportsCat { nullptr };
    AudioCategoryItem*                        mReportsCat { nullptr };
    void rebuildRenderRows();

public:
    // TS7 §11: { <project>\Exports\, <project>\Reports\ }.  Supplied by the
    // editor -- the panel does not know where a project lives.
    std::function<std::pair<juce::File, juce::File>()> onGetRenderFolders;
    // §11.6: open a saved report in the analyzer window.
    std::function<void(const juce::File&)>             onOpenReport;
    // §11.5/§11.5a: add a rendered file to the project.  Not a callback -- the
    // whole flow is reachable from here (mPM plus the routing callbacks the
    // Properties dialog already uses), and routing it through the editor would
    // just be a second copy of that logic waiting to drift from it.
    //
    // Registers the file IN PLACE: every other import copies into Samples\
    // first, and a render must not, because the point of the Exports row is
    // that the file the user just made is the file that plays.
    //
    // onRouted fires with the resulting library index so the grid-drop caller
    // can place the block where it landed.  Pass nullptr for the browser
    // gesture, which places nothing.
    void beginAddRenderToProject (const juce::File& f,
                                  std::function<void(int /*libIdx*/)> onRouted);

    // TS7 (Jeff, 2026-07-29): how the ITEMS INSIDE each group are ordered --
    // Clips, Vox, Inst, Exports and Reports alike, plus the leaves inside a
    // manual or auto group.
    //
    // It does NOT reorder the groups or the categories themselves: those carry
    // meaning the user assigned (category, manual group name, take-suffix
    // family), and shuffling them would destroy that.  Only the leaves move.
    enum class ItemSort { NewestFirst, OldestFirst, Alphabetical };
    void setItemSort (ItemSort s);
private:
    ItemSort mItemSort { ItemSort::NewestFirst };
    // Orders a leaf list in place by the current mode.  ONE implementation, so
    // every category and every group sorts identically.
    void sortEntries (std::vector<CategorizedAudioEntry>& v) const;
    std::unique_ptr<juce::TextButton> mSortBtn;
    void showSortMenu();

    std::unique_ptr<juce::TextButton> mAddBtn;
    std::unique_ptr<juce::TextButton> mDeleteBtn;

    // Audio file paths keyed by item index (so right-click "Remove" knows the path)
    juce::StringArray mAudioPaths;
    // Automation block indices keyed by item index
    std::vector<int>  mAutomBlockIndices;

    // Diff-guard for refresh() so the timer doesn't blindly destroy items.
    juce::String      mLastRefreshSnapshot;

    void rebuildPatternRows();
    void rebuildAudioRows();         // G-5: now rebuilds the tree, not the flat list

    // QA-E Task 5 (2026-05-15): browser Delete -> confirmation prompt +
    // last-file-out cascade.  Shared between the tree right-click handler
    // (showAudioTreeContextMenu) and the flat-list right-click handler.
    // Prompts the user, on confirm cascade-removes matching blocks, removes
    // the specific library entry by index, and (if this was the page's last
    // file) fires onClosePageForChannelId so StandaloneEditor closes the
    // owning Clips / Vox / Inst tab.
    void confirmAndDeleteLibraryEntry (int libIdx);
    // QA-E Task 7 (FILE-02): browser-entry "Properties..." dialog -- the SAME
    // full Audio Properties box as the per-clip grid dialog (Pitch / BPM /
    // Mode + Routes to:, built via the shared buildAudioPropsControls helper).
    // Editing it is the source-of-truth edit: writes pitch/BPM/mode/route onto
    // the library entry and propagates all four to every grid copy still
    // following (via onApplyLibraryProperties).  Mirrors
    // confirmAndDeleteLibraryEntry's shape (SafePointer + modal callback).
    void showLibraryPropertiesDialog (int libIdx);
    void rebuildAutomationRows();
    void switchTab(int t);
    void selectPattern(int idx);

    // ── Item interaction helpers ─────────────────────────────────────────
    void openRenamePopup(BrowserItem& item);
    void showItemContextMenu(BrowserItem& item, juce::Point<int> globalPt);

    // G-5 (2026-04-29): right-click on an Audio tree leaf - same context-menu
    // shape as the legacy flat-list audio item (Rename / Choke Group / Delete
    // + new Reveal in Explorer).  audioLibIdx is the global library index for
    // direct lookup into mPM.audioLibrary.
    void showAudioTreeContextMenu(AudioBrowserItem& item, juce::Point<int> globalPt);

    // ── QA-Fe2 browser groups ────────────────────────────────────────────
    // Category header right-click -> "Create Group..." (manual, may sit
    // empty).  Group header right-click -> "Rename Group..." (auto groups
    // route through onRenameRecordingGroup; manual rename is label-only).
    // maybePromptGroupAssign fires after a Properties move/copy lands on a
    // Vox/Inst target: offers that page's recording groups + the category's
    // manual groups as a destination ("(none)" default).
    void showCategoryContextMenu (int category, juce::Point<int> globalPt);
    void showGroupContextMenu    (int category, const juce::String& name,
                                  bool isAuto, juce::Point<int> globalPt);
    void maybePromptGroupAssign  (int libIdx, int targetChannel);

    // G-6 (2026-04-29): "Duplicate..." right-click flow.  Shows the name
    // prompt, resolves conflicts (Overwrite / Cancel / Rename re-prompt),
    // copies the WAV to the same folder, and fires onDuplicateClipSpawn so
    // StandaloneEditor can spawn a new ClipsPage on the copy + clone the
    // source page's full state.  defaultName is pre-populated ("<original>
    // Duplicate" on first call; on Rename, the user's last typed value is
    // fed back in).  Recursive on Rename.
    void runAudioDuplicateFlow(const juce::String& sourceAbsPath,
                               const juce::String& defaultName);

    void renamePatternAt   (int idx, const juce::String& newName);
    void renameAudioAt     (int idx, const juce::String& newName);
    void renameAutomationAt(int idx, const juce::String& newName);
};

// ── SnapMode ──────────────────────────────────────────────────────────────────
// QA-Ee Stage 2: SnapMode enum removed -- Builder snap is now the unified Int
// 0..10 scheme (param Unified_BuilderSnapDiv), read live by the grid via
// onGetSnapDiv().  See BaySickConstants.h kUnifiedSnapLabels / snapDivToTicks.

// ── ArrangementGrid ───────────────────────────────────────────────────────────
// Piano-roll-style arrangement editor.
// Label column is EXTERNAL (TrackHeaderPanel) - grid starts at x=0 for bar 0.
//
// Tools: P=Draw  B=Paint  E=Select  D=Delete  T=Mute  S=SlipEdit  C=Slice
//        Shift+Z=Zoom  Y=PlaySelected
// Zoom:    Ctrl+Scroll (horizontal)
// Pan:     Shift+Scroll / bottom scrollbar (horizontal, virtual mBarOff)
//          |  Viewport scroll (vertical)
// Keys:    Ctrl+Z/Y undo/redo, Ctrl+A/C/V/B, Delete, Shift+Arrows, Escape
// ─────────────────────────────────────────────────────────────────────────────
class ArrangementGrid : public juce::Component,
                        public juce::FileDragAndDropTarget,
                        public juce::DragAndDropTarget,
                        public juce::TooltipClient   // 2026-04-26 (D-2): ruler hover tooltips
{
public:
    // QA-ProjectSave (2026-07-26): removing an automation's LAST point leaves a
    // block that controls nothing, so both delete routes ask here whether to
    // drop the whole block -- this grid's own right-click, and the Event
    // Editor's via StandaloneEditor.  Public because the Event Editor route
    // calls in from outside.
    void promptDeleteWholeAutomation (int blockIdx);

    // QA-Ea Task 0c (2026-05-20): SlipEdit removed from the AGTool enum.
    // It used to be a mutually-exclusive tool in the toolbar tool group; now
    // it lives as a separate Slip/Stretch dropdown (see EditMode below) so
    // the user can be in any tool AND have the slip / stretch mode set
    // independently.  All references to AGTool::SlipEdit were removed
    // alongside the tool button + 'S' keybind reassignment.
    enum class AGTool {
        Draw, Paint, Select, Delete, Mute,
        Slice, Zoom, PlaySelected
    };

    // QA-Ea Task 0c (2026-05-20): edge-drag behavior mode.  Determines what
    // a left or right edge drag on an Audio clip does.  Slip = our Option A
    // semantics (left edge moves on timeline, contentStart shifts, right
    // edge mirrors -- "slip-out").  Stretch = time-stretch the clip's
    // content to fit (QA-Ec full impl; for Task 0c, stretch mode is dormant
    // on the LEFT edge and falls back to the existing length-only resize on
    // the RIGHT edge so today's right-edge resize UX is preserved).
    // Pattern and Automation blocks are NEVER affected by this mode -- the
    // dropdown's behavior is gated on clipType == Audio.  Default Stretch
    // (matches today's pre-Task-0c right-edge resize UX).
    enum class EditMode { Slip, Stretch };

    explicit ArrangementGrid(PatternManager& pm,
                             juce::AudioFormatManager& afm,
                             juce::AudioThumbnailCache& thumbCache);
    ~ArrangementGrid() override;

    void paint         (juce::Graphics&)                                         override;
    void resized       ()                                                         override;
    void mouseMove     (const juce::MouseEvent&)                                 override;
    void mouseDown     (const juce::MouseEvent&)                                 override;
    void mouseDrag     (const juce::MouseEvent&)                                 override;
    void mouseUp       (const juce::MouseEvent&)                                 override;
    void mouseDoubleClick(const juce::MouseEvent&)                               override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed    (const juce::KeyPress&)                                   override;
    void modifierKeysChanged(const juce::ModifierKeys&)                          override;
    // B-4: apply the Slice drag-line - split every block the line crosses at its
    // per-row X (one undo edit).  sliceOneBlock does the split for a single block
    // (caller wraps begin/commitEdit); returns true if it split.
    void applySliceLine (juce::Point<int> start, juce::Point<int> end);
    bool sliceOneBlock  (int blockIdx, double cutBars);

    // FileDragAndDropTarget - accept audio files dropped from OS
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void fileDragMove(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray&) override;

    // DragAndDropTarget - accept BrowserItem drags from the BrowserPanel.
    // Description format: "kind:index" (kind = "pattern" / "audio" / "auto").
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDragEnter(const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped  (const SourceDetails&) override;

    void setTool(AGTool t);
    AGTool getTool() const { return mActiveTool; }

    // (setSnapMode/getSnapMode removed QA-Ee Stage 2 -- the grid reads the snap
    //  index live via onGetSnapDiv(); the combo writes it via onSnapDivChanged.)

    void setPlayheadBar(double bar)
    {
        if (bar == mPlayheadBar) return;
        // Residual (b) fix: dirty-rect playhead repaint (see PianoRollGrid) --
        // covers the ruler flag + mast column; the perf-mode pulse keeps its
        // own full repaint in the page timer.
        const double old = mPlayheadBar;
        mPlayheadBar = bar;
        if (old >= 0.0) repaint(barToX((float) old) - 2, 0, 14, getHeight());
        if (bar >= 0.0) repaint(barToX((float) bar) - 2, 0, 14, getHeight());
    }
    void setPerformanceMode(bool on) { mPerfMode = on; repaint(); }

    void setSelectedPatternIndex(int idx)
    {
        mBrowserSelection = idx;
        mClickMemoryLenBeats = -1.0;   // #26: fresh browser pick clears the copy memory
    }

    // QA-H Task 8 (#20): what an empty-grid Draw click places = the last
    // browser entry clicked (piano-roll last-type parity).  Pattern rides
    // mBrowserSelection; Audio/Automation carry their own ref index.
    enum class BrowserDropKind { Pattern, Audio, Automation };
    void setActiveDropKind (BrowserDropKind k, int refIdx)
    {
        mDropKind = k;
        mClickMemoryLenBeats = -1.0;   // #26: fresh browser pick clears the copy memory
        if      (k == BrowserDropKind::Audio)      mDropAudioIdx = refIdx;
        else if (k == BrowserDropKind::Automation) mDropAutomIdx = refIdx;
        else if (refIdx >= 0)                      mBrowserSelection = refIdx;
    }

    // ── Selection ─────────────────────────────────────────────────────────────
    void selectAll     ();
    void clearSelection();
    bool hasSelection  () const { return !mSelection.empty(); }

    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr int kRowH       = 40;
    static constexpr int kRulerH     = 18;
    static constexpr int kLabelW     = 120;   // kept for external use (BuilderPage layout)
    // Owner call 2026-07-17: FL-parity 500 tracks (supersedes marathon-5's
    // "keep 50 stock rows" -- 50 was an arbitrary early constant, not a spec).
    static constexpr int kNumRows    = 500;
    // Alt-zoom limits.  Jeff, 2026-08-04: these are ABSOLUTE PIXELS now.  They
    // used to be derived from the viewport height ((vpH - kRulerH) / 50 and
    // / 8), which tied the zoom range to the window: the same Alt+scroll
    // bottomed out at a readable ~12 px row in a full-screen Builder and at a
    // ~4 px row in a small one, so a contained window crushed every row into
    // its neighbour.  Fixed bounds make one gesture mean one thing at any
    // window size, and the floor is above the tallest fixed-height thing drawn
    // in a row, so rows can no longer overlap at all.
    static constexpr int kMinRowH = 16;
    static constexpr int kMaxRowH = 96;
    static constexpr int kResizeZone = 8;
    // QA-Ea Task 0c (2026-05-20 update): dynamic negative-bar viewport limit.
    // Replaces the earlier hardcoded -8 floor.  Scans all Audio blocks for
    // the max contentStartSamples > 0, converts to bars at project BPM, and
    // returns that many bars of negative-viewport allowance.  If no clip has
    // pre-roll captured, returns 0 (no negative scroll allowed).  Used by
    // every user-initiated viewport scroll site (wheel / zoom anchor /
    // centre zoom).  Auto-fit operations keep their own bar-0 floors.
    double maxRevealableNegativeBars() const;

    // QA-Ee Stage 2: content-bound dynamic zoom (single source of truth for the
    // zoom clamp -- every zoom path calls these).  minZoomPPBar expands as
    // content grows; maxZoomPPBar is the tick-level zoom-in ceiling.
    float  contentMaxBars() const;
    float  minZoomPPBar (float vpW) const;
    float  maxZoomPPBar (float vpW) const;

    // ── Undo / Redo ───────────────────────────────────────────────────────────
    void setUndoContext(const UndoContext& ctx) { mUndoCtx = ctx; }
    void beginEdit (const juce::String& label = "Edit");
    void commitEdit();
    void applySnapshot(const std::vector<ArrangementBlock>& blocks,
                       const std::array<juce::String, kNumRows>& rowNames,
                       const std::vector<int>& rowGroups,
                       const std::vector<juce::uint32>& rowColors,
                       const std::vector<char>& rowMuted,
                       const std::vector<char>& rowSoloed);
    // QA-UndoCoverage Task 4: the pattern-list slice (patterns + blocks +
    // per-row state), promoted from the Split-by-Engine local lambdas so
    // browser pattern ops and the transport dropdown share one capture/apply.
    PatternListSnapshot capturePatternSlice() const;
    void                applyPatternSlice (const PatternListSnapshot& s);
    // Wrap one pattern-list gesture (add / duplicate / remove) as one
    // PatternListAction transaction.  Public: the transport dropdown and
    // keybind paths in StandaloneEditor delegate here.
    void performPatternSliceOp (const juce::String& label, const std::function<void()>& op);
    // Wrap one marker/TS/tempo gesture as one MarkerSetAction transaction.
    void performMarkerSetOp (const juce::String& label, const std::function<void()>& op);
    // The marker/TS/tempo snapshot pair, shared by every path that banks a
    // MarkerSetAction (the marker wrapper, the pattern+TS wrapper, and
    // Split by Player Engine, whose removePattern destroys the original
    // pattern's linked markers).
    MarkerSetSnapshot captureMarkerSet() const;
    void              applyMarkerSet (const MarkerSetSnapshot& s);
    // Wrap a gesture that writes pattern state AND marker lists - the pattern
    // TS setters, and the pattern-list edits that re-point every linked
    // marker's pattern index (insert / move / remove).  A MarkerSetAction
    // rides the slice action inside ONE transaction.
    void performPatternTsOp (const juce::String& label, const std::function<void()>& op);
    // For rider actions appended into the transaction commitEdit just opened
    // (perform WITHOUT beginNewTransaction = same ActionSet, one Ctrl+Z).
    juce::UndoManager* riderManager() const { return mUndoCtx.manager; }

    void undo() { if (mUndoCtx.undo) { mUndoCtx.undo(); if (onUndoRedoStateChanged) onUndoRedoStateChanged(); } }
    void redo() { if (mUndoCtx.redo) { mUndoCtx.redo(); if (onUndoRedoStateChanged) onUndoRedoStateChanged(); } }
    void showHistory() { if (mUndoCtx.showHistory) mUndoCtx.showHistory(); }   // 2026-04-26 (D-1b)
    bool canUndo() const { return mUndoCtx.isValid() && mUndoCtx.manager->canUndo(); }
    bool canRedo() const { return mUndoCtx.isValid() && mUndoCtx.manager->canRedo(); }

    // ── Callbacks ─────────────────────────────────────────────────────────────
    std::function<void(AGTool)>      onToolChanged;
    std::function<void()>            onUndoRedoStateChanged;
    std::function<void(double beat)> onSeek;               // ruler bare-click → seek playhead
    std::function<void()>                              onRowHeightChanged;      // Alt+scroll → notify TrackHeaderPanel
    std::function<void(int)>                           onOpenEventEditor;       // double-click or ctx-menu on Automation clip → open EventEditor
    // 2026-04-28 (G-2): signature extended to also pass the imported audio
    // file path so listeners can spawn per-clip Clips tabs without re-deriving
    // the path from the row.
    std::function<void(int row, const juce::String& rowName, const juce::String& filePath)> onAudioClipAdded;
    std::function<void()>                              onArrangementChanged;    // any block move/resize/delete → rebuild audio clip players
    // QA-TempoMap (2026-07-08): fires after any ruler tempo-flag add/edit/
    // delete; StandaloneEditor pushes the marker set into the playhead's
    // timeline + marks the project dirty.
    std::function<void()>                              onTempoMapChanged;
    // QA-E Task 7 (FILE-02): "Routes to:" dropdown support.  Both wired by
    // StandaloneEditor (mirrors onArrangementChanged / onAudioClipAdded).
    // onEnumerateRoutablePages → one RoutablePageInfo per Vox/Inst/Clips page
    // (channelId = MixerChannelIds id, displayName = ribbon tab name).
    std::function<std::vector<RoutablePageInfo>()>     onEnumerateRoutablePages;
    // onCreateRoutablePage → create a new routable page of the given kind
    // (0=Clip, 1=Vox, 2=Inst), backed by audioPath (used for Clips; ignored
    // for Vox/Inst), navigate to it, and return its channelId (or -1 on
    // failure / no free slot).
    std::function<int(int /*kind*/, const juce::String& /*audioPath*/)> onCreateRoutablePage;
    // QA-E Task 7 (FILE-02): "Copy" action for the per-clip Properties menu.
    // Split so "Copy to a new Clip Page" doesn't double-register (see
    // BrowserPanel decl above for the rationale).  onDuplicateFileForCopy
    // makes the auto-numbered physical copy only; onTagCopiedEntry registers
    // it on the target (dedup-safe) + sets clip defaults + notifies.  The
    // acted-on grid block is then repointed to the duplicate.
    std::function<juce::String(const juce::String& /*src*/)>            onDuplicateFileForCopy;
    std::function<void(const juce::String& /*np*/, int /*targetChannel*/,
                       float /*pitch*/, float /*bpm*/, bool /*stretch*/)> onTagCopiedEntry;
    // TS7 §11.5: an Exports/Reports file dragged onto the grid.  Routed to the
    // SAME import-then-prompt handler the browser's "Add to Project..." uses, so
    // dragging and right-clicking cannot end up meaning different things.  row /
    // bar are where it landed, so the import can place it there.
    std::function<void(const juce::String& /*fullPath*/, int /*row*/, float /*bar*/)>
                                                                        onRenderFileDropped;
    // Owner call 2026-07-11: per-clip Properties "Move" mirrors the browser
    // dialog exactly by sharing its lambda -- relocate the library entry's
    // owner + props and propagate to every FOLLOWING copy (isOverride==false),
    // so the grid clip and its browser entry move as one (linked).  Wired in
    // StandaloneEditor from `panel->onApplyLibraryProperties`.
    std::function<void(int /*libIdx*/, float /*pitch*/, float /*bpm*/,
                       bool /*stretchMode*/, int /*newRoute*/)>          onApplyLibraryProperties;
    // copies it into the current project's Samples/ folder and returns the
    // relative string to store (e.g. "Samples/kick.wav"), or returns {} to
    // reject the drop (e.g. no project open - caller also shows the user an
    // explanation dialog).  When this callback is unset, imports fall back to
    // storing the absolute path (pre-P4 behavior).
    std::function<juce::String(const juce::File& externalFile)> onImportSampleRequest;
    // P4: resolves a stored path (possibly relative) to an absolute file.
    // When unset, falls back to juce::File(stored) which treats the string
    // as an absolute path (pre-P4 behavior).
    std::function<juce::File(const juce::String& stored)> onResolveStoredPath;

    // QA-Ea Task 0c (FL pre-roll record + non-destructive clip trim): two
    // callbacks supporting the slip-edit drag.  Wired by BuilderPage to
    // the transport / processor:
    //   onGetBPM               -> transport's getBPM()
    //   onGetSampleRate        -> processor's getSampleRate()
    // (The slip-edit mode read used to be a callback to the editor's
    // mSlipEditMode flag; replaced 2026-05-20 with internal mEditMode.)
    std::function<double()> onGetBPM;
    std::function<double()> onGetSampleRate;
    // QA-Ee Stage 2 (Builder snap): the grid reads the unified snap-division
    // index (0..10) LIVE from the Unified_BuilderSnapDiv APVTS param via this
    // getter (used by snapBar/snapBarAlt + drawGrid) and writes it via the setter
    // when the snap combo changes.  Both wired in StandaloneEditor.
    std::function<int()>     onGetSnapDiv;
    std::function<void(int)> onSnapDivChanged;

    // QA-Ea Task 0c (2026-05-20): edit-mode state + accessors + callbacks.
    // mEditMode determines what a left or right edge drag does on an Audio
    // clip (see EditMode enum above).  Default Stretch (preserves the
    // pre-Task-0c right-edge resize UX).  setEditMode fires onEditModeChanged
    // so the toolbar dropdown can update its button label in lockstep.
    EditMode getEditMode() const noexcept { return mEditMode; }
    void setEditMode (EditMode m)
    {
        if (m == mEditMode) return;
        mEditMode = m;
        if (onEditModeChanged) onEditModeChanged (m);
    }
    void toggleEditMode()
    {
        setEditMode (mEditMode == EditMode::Slip ? EditMode::Stretch
                                                  : EditMode::Slip);
    }
    std::function<void(EditMode)> onEditModeChanged;
    // P4: called when onImportSampleRequest rejects the drop because no
    // project is open.  Caller prompts for a new project name (async),
    // creates it, then re-invokes importAudioFile with the original args so
    // the drop completes after project creation.
    std::function<void(const juce::File& src, int row, float bar)> onDropWithoutProject;

    // QA-E Task 5 (2026-05-15): fires when a disk drop's file is already in
    // the audio library.  StandaloneEditor shows a "Use existing routing /
    // New page and strip / Cancel" prompt and either calls back into
    // placeAudioLibraryEntry (Existing) or spawns a forced-new Clips page
    // and adds a block routed to it (New).  Unwired = fall back to the
    // default importAudioFile path (existing pre-Task-5 behavior).
    std::function<void(const juce::File& dropped, int libIdx, int row, float bar)>
        onDuplicateFileDrop;
    // Resolves an AutomationLane to a display label. Used by drawAutomationClip
    // so on-grid blocks show "Channel - Effect - Param" (or the user's rename)
    // instead of the raw paramId. Null = fall back to paramId.
    std::function<juce::String(const AutomationLane&)> onResolveDisplayName;
    // Resolves a lane's paramId to the 0..1 value "Reset to Default" restores a
    // control point to (StandaloneEditor::automationResetValue).  Null = 0.5.
    std::function<float(const juce::String& paramId)> onResolveResetValue;

    // ── Row names (shared with TrackHeaderPanel) ──────────────────────────────
    // QA-G: row names live in PatternManager (project data, serialized in
    // RowState) -- the grid is a pass-through view, no parallel copy.
    static_assert(kNumRows == kMaxArrangementRows,
                  "Builder grid rows must match PatternManager row-state arrays");
    const std::array<juce::String, kNumRows>& getRowNames() const { return mPM.getRowNames(); }
    void setRowName(int row, const juce::String& name);

    // ── View state (public - BuilderPage accesses these directly) ────────────
    float  mPPBar        { 80.f };   // pixels per bar
    float  mEffectiveRowH{ 40.f };   // current row height (Alt+scroll adjusts)
    double mPlayheadBar  { -1.0 };   // <0 = not playing
    float  mBarOff       { 0.f  };   // horizontal scroll offset in bars
    float  mPulsePhi     { 0.f  };   // performance-mode ruler pulse phase

    // ── Time selection accessors (for doNewAutomationClip) ────────────────
    bool  hasTimeSelection() const { return mTimeSelStart >= 0.f; }
    float getTimeSelStart()  const { return mTimeSelStart; }
    float getTimeSelEnd()    const { return mTimeSelEnd; }
    void  clearTimeSelection()     { mTimeSelStart = mTimeSelEnd = -1.f; repaint(); }
    // P4 persistence (2026-04-24): restore a saved time selection on load.
    void  setTimeSelection (float startBar, float endBar)
    {
        mTimeSelStart = startBar;
        mTimeSelEnd   = endBar;
        repaint();
    }

    // ── Public operations (called from BuilderPage) ───────────────────────────
    // QA-E Task 4 (2026-05-12): routeChannel param (default 0 = generic
    // Audio behavior).  When non-zero, the dropped block is routed through
    // the Vox/Inst/Clips page identified by that channel id, AND the
    // onAudioClipAdded callback (which spawns a Clips strip) is skipped
    // -- Vox/Inst-routed clips play through the page's chain, not an
    // Audio strip.  Mirrors commitRecordingResult's dropWavAsClip pattern.
    void importAudioFile(const juce::String& path, int targetRow, float targetBar, int routeChannel = 0);

    // QA-E Task 5 (2026-05-15): "place existing library entry on grid" helper.
    // Used by:
    //   * itemDropped browser->grid drop ("audio" kind) -- always uses this
    //     path since the file is by definition already in the library.
    //   * filesDropped disk-drop "Existing routing" prompt callback.
    // SKIPS importSample (no copy), addAudioToLibrary (entry exists), and
    // onAudioClipAdded (routed clip, page exists).  Just resolves the path,
    // reads metadata, creates a routed block.
    void placeAudioLibraryEntry(int libIdx, int targetRow, float targetBar);

    // ── Split by Player Engine (QA-G, owner docket 5/5a/5b 2026-07-17) ────
    // One new pattern per tab entry with MIDI data (Layers/Bass/Drums/Clips/
    // Inst), named "Original - <tab name>"; original destroyed; placed blocks
    // replaced in place with siblings stacked directly below; ONE undo step.
    void splitPatternByEngine (int patternIndex);
    // Tab display names for the split ("<tab name>" auto-populates from
    // presets); kind: 0=Layer 1=Bass 2=Drum 3=Clip 4=Inst.  Fallback =
    // generic "Layer N" style when unwired/empty.
    std::function<juce::String(int kind, int engineIndex)> onGetEngineTabName;
    // Row-insertion primitives (shared by the split + Insert Track Above):
    // whole-row blank test, bottom-capacity check, and the shift-down insert.
    bool rowIsBlank (int row) const;
    bool canInsertRows (int count) const;
    bool insertBlankRowsAt (int startRow, int count);

    // ── Public coordinate helpers (BuilderPage zoom anchoring + scrollbar
    //    sync; TrackHeaderPanel row alignment) ───────────────────────────────
    int   barToX(float bar) const;
    float xToBar(int x)     const;
    float xToBarF(float x)  const;   // all-float anchor math (toolbar centre zoom)
    int   rowToY (int row)  const;
    int   yToRow (int y)    const;
    int   rowHeightPx(int row) const;   // rowToY(row+1) - rowToY(row)
    int   totalVisibleBars() const;

    // LIFETIME: drops one file's baked waveform image AND its AudioThumbnail.
    // Message thread ONLY.  No getOrCreateThumbnail result may be live on the
    // stack when this runs: ArrangementGrid::drawAudioClip holds the raw
    // AudioThumbnail* for the duration of the draw, and the slip-edit drag
    // read in mouseDrag holds it across its clamp math, so erasing under
    // either would dangle it.  Callers are the library-delete cascades, which
    // run from menu callbacks.
    void purgeWaveformCacheFor (const juce::String& path);

private:
    PatternManager&            mPM;
    juce::AudioFormatManager&  mAFM;
    juce::AudioThumbnailCache& mThumbCache;

    // Thumbnail cache: filePath → AudioThumbnail (mutable so const draw methods can populate)
    // LIFETIME: same ceiling reasoning as mWaveformImages below -- the grid is
    // a default tab that lives the whole session, and nothing evicts on clip
    // delete, arrangement clear or project switch, so without a ceiling every
    // file ever scrolled into view (including takes from projects closed hours
    // ago) holds its level array until the process exits.  Evicted through the
    // same helper as the image cache so the two cannot drift apart.
    // MAGIC NUMBER: 256 entries.  A 3-minute 48k stereo file costs ~66 KB of
    // min/max levels at the grid's 512 samples-per-thumb-sample, so this is a
    // ~17 MB ceiling -- parity with the image cache, and far above any visible
    // working set (drawBlocks culls to the on-screen rect).
    static constexpr size_t kMaxThumbnails = 256;
    mutable std::map<juce::String, std::unique_ptr<juce::AudioThumbnail>> mThumbnails;
    mutable std::deque<juce::String> mThumbnailOrder;

    // QA-Ea Task 0c (Rule 5 -- buttery-smooth slip-edit drag): cached
    // off-screen Image per file path.  Bakes the FULL file waveform once
    // into a fixed-size juce::Image (2048x64 ARGB, white-on-transparent
    // so the brush-color tint works at draw time via
    // fillAlphaChannelWithCurrentBrush).  Drawing computes a source sub-rect
    // for [contentStartSamples, contentStartSamples + visibleLengthBeats]
    // in file-time terms and lets JUCE scale-blit it into the clip's visible
    // rect -- no per-frame drawChannels work during a slip-drag.  Cache
    // invalidates when the AudioThumbnail finishes loading (partial -> full
    // re-bake).  Path key avoids needing zoom-axis invalidation -- the blit
    // scales the cached image at draw time.
    struct WaveformImageCacheEntry
    {
        juce::Image image;
        bool        wasFullyLoadedWhenBaked { false };
    };
    mutable std::map<juce::String, WaveformImageCacheEntry> mWaveformImages;
    // LIFETIME: the grid is a default tab that lives for the whole session, so
    // the bake cache needs its own ceiling -- without one every file ever drawn
    // (including deleted takes) holds its image until the process exits.  The
    // deque is insertion order; the oldest entries are dropped once the map
    // passes kMaxWaveformImages.  A dropped path simply re-bakes when it next
    // scrolls into view.
    // MAGIC NUMBER: 32 entries x 2048x64 ARGB = a 16 MB ceiling.
    static constexpr size_t kMaxWaveformImages = 32;
    mutable std::deque<juce::String> mWaveformImageOrder;
    const juce::Image* getOrBakeWaveformImage (const juce::String& path,
                                                juce::AudioThumbnail* thumb) const;

    // ── Tool + snap ───────────────────────────────────────────────────────────
    AGTool   mActiveTool    { AGTool::Draw };
    // QA-Ea Task 0c (2026-05-20): default snap = Steps (1/16) instead of
    // Bar.  Sub-bar precision is the norm for builder grid edits (drag,
    // slip, stretch all honor snap mode); whole-bar was too coarse for
    // typical workflows.  User can change via the snap-mode picker; new
    // projects start at Steps.
    // mSnapMode removed (QA-Ee Stage 2): the grid reads the unified snap index
    // live via onGetSnapDiv() (param Unified_BuilderSnapDiv, default 1 = Line).
    bool     mAltSnapActive { false };  // Alt held = snap override to None

    // ── Performance mode ──────────────────────────────────────────────────────
    bool         mPerfMode  { false };

    // ── Browser selection ─────────────────────────────────────────────────────
    int mBrowserSelection { 0 };
    BrowserDropKind mDropKind     { BrowserDropKind::Pattern };
    int             mDropAudioIdx { -1 };
    int             mDropAutomIdx { -1 };
    // #26 (QA-G3Smoke, G-15): click-copy memory.  Clicking a block primes the
    // "brush" with WHAT that block is (identity via the fields above) plus its
    // length + content offset, so an empty-space click places a faithful copy.
    // A fresh browser pick resets to content-length defaults.  lenBeats < 0 =
    // no memory.  Nothing else carries over (velocity is #12, roll-only).
    double          mClickMemoryLenBeats    { -1.0 };
    juce::int64     mClickMemoryOffsetTicks { 0 };            // Pattern copies
    juce::int64     mClickMemoryContentStartSamples { 0 };    // Audio copies
    void primeClickMemoryFrom (int blockIdx);

    // ── Ghost clip (drag preview from Source Picker) ──────────────────────────
    bool           mHasGhost   { false };
    ArrangementBlock mGhostBlock;
    float          mGhostAlpha { 0.5f };

    // ── File drag ghost ────────────────────────────────────────────────────────
    bool         mFileDragActive { false };

    // ── Interaction state ─────────────────────────────────────────────────────
    // Draw / resize
    bool  mDrawing    { false };
    int   mDrawRow    { -1 };
    float mDrawStart  { 0.f };
    float mDrawEnd    { 1.f };

    // Paint (continuous draw without gaps)
    bool  mPainting   { false };
    int   mPaintRow   { -1 };
    float mPaintLastBar { 0.f };
    // #25 (QA-G3Smoke): paint stamp size in bars (the selected pattern's
    // content length, captured at paint start) -- drives stamp advance too.
    int   mPaintLenBars { 1 };

    // B-4: Slice tool drag-line (two-dot preview; applied on mouseUp).  Cuts every
    // block whose row the line's y-span crosses, at the line's X for that row
    // (Shift = vertical, snapped X).  Mirrors the piano-roll slice line.
    bool  mSlicing    { false };
    juce::Point<int> mSliceStart, mSliceEnd;

    // QA-Ea Task 0c (2026-05-20): legacy AGTool::SlipEdit stub state
    // (mSlipping/mSlipIdx/mSlipDragX + the never-functional "Slip edit:
    // visual only (stub - no audio data shift yet)" mouseDrag branch) is
    // removed.  Replaced by the EditMode dropdown + mSlipEditing drag state
    // (which does real work, not a stub).

    // QA-Ea Task 0c (2026-05-20): persistent edit-mode for edge drags.
    // Default Stretch (matches pre-Task-0c right-edge resize UX).  Toggled
    // by 'S' (ApplicationCommandManager cmdToggleSlipStretchMode) or via
    // the toolbar dropdown.  Audio-clip edge drags consult this; Pattern
    // and Automation clips ignore it.
    EditMode mEditMode { EditMode::Stretch };

    // QA-Ea Task 0c (FL pre-roll record): slip-edit drag state.  Fires
    // when EditMode == Slip AND the user clicks an Audio clip's edge.
    // Drag adjusts contentStartSamples + lengthBeats; LEFT-edge slip:
    // startBar + startBeats move, right-end stays fixed (Option A).
    // RIGHT-edge slip: lengthBeats moves, contentStart + startBeats stay
    // fixed (slip-out mirror).  Pattern + Automation clips bypass this
    // entirely.  See running-notes 2026-05-19 design lock-in + 2026-05-20
    // dropdown-mode redesign.
    enum class SlipEdge { Left, Right };
    bool          mSlipEditing             { false };
    int           mSlipEditIdx             { -1 };
    SlipEdge      mSlipEditEdge            { SlipEdge::Left };
    juce::int64   mSlipEditOrigContent     { 0 };
    float         mSlipEditOrigLengthBeats { -1.f };
    // QA-Ea Task 0c (2026-05-20 - Option A): origin start in beats (sub-bar
    // precision; can be negative).  Drag delta is applied vs this snapshot.
    float         mSlipEditOrigStartBeats  { 0.f };
    // QA-Ea Task 0c (Option ii auto-scroll): origin mouse position in bars
    // at mouseDown (xToBar(e.x)).  Drag uses bar-delta from this snapshot
    // so the auto-scroll-during-drag correctly moves the edge with the
    // viewport instead of being pinned to fixed component pixels.
    float         mSlipEditOrigMouseBar    { 0.f };

    // Move
    bool              mMoving         { false };
    std::vector<int>  mMoveIndices;
    juce::Point<int>  mMoveDragOrigin;
    std::vector<float> mMoveOrigBars;
    std::vector<int>  mMoveOrigRows;

    // Resize (also handles Shift+drag = time-stretch for audio clips)
    bool  mResizing       { false };
    bool  mStretching     { false };  // Shift+drag right edge on audio clip
    // QA-Ec (2026-07-08): exact beat length at stretch-drag start.  The
    // re-fit ratio needs BEATS - mResizeOrigLen is whole bars, which is
    // wrong for sub-bar clips (an imported 2.3-beat one-shot reads 1 bar).
    double mStretchOrigBeats { 0.0 };
    int   mResizeIdx      { -1 };
    float mResizeOrigLen  { 0.f };
    float mResizeOrigStart{ 0.f };
    std::vector<ControlPoint> mResizeOrigPoints;  // automation point positions before resize

    // ── Inline automation curve handle drag ───────────────────────────────────
    int   mAutomCurveHandleDrag { -1 };  // original index in lane->points of left point, -1=none

    // Marquee
    bool                 mMarqueeActive { false };
    juce::Point<int>     mMarqueeStart;
    juce::Rectangle<int> mMarqueeRect;

    // Zoom-rect drag (Ctrl+RClick drag - D-1 2026-04-26)
    bool                 mZoomRectActive { false };
    juce::Point<int>     mZoomRectStart;
    juce::Rectangle<int> mZoomRect;

    // ── Selection ─────────────────────────────────────────────────────────────
    std::vector<int> mSelection;

    // ── Undo / Redo ───────────────────────────────────────────────────────────
    UndoContext                        mUndoCtx;
    std::vector<ArrangementBlock>      mPendingBlocks;
    std::array<juce::String, kNumRows> mPendingRowNames;
    std::vector<int>                   mPendingRowGroups;   // QA-G: per-row state for undo
    std::vector<juce::uint32>          mPendingRowColors;
    std::vector<char>                  mPendingRowMuted;
    std::vector<char>                  mPendingRowSoloed;
    juce::String                       mPendingLabel;
    // Marker/TS/tempo state as of beginEdit.  commitEdit calls
    // cleanupLinkedMarkers, which PERMANENTLY erases orphaned linked TS
    // markers, and the ArrangementEditAction payload carries no markers -- so
    // the before-side has to be banked here or an edit that orphans a marker
    // destroys it with no way back.
    MarkerSetSnapshot                  mPendingMarkers;

    // ── Clipboard ─────────────────────────────────────────────────────────────
    std::vector<ArrangementBlock> mClipboard;

    // ── Automation point editing (Draw tool on Automation clips) ──────────────
    int          mAutomEditBlock { -1 };   // block index being edited
    int          mAutomDragPoint { -1 };   // point index being dragged (-1 = none)
    bool         mAutomDragging  { false };

    // ── Time selection (Ctrl+drag on ruler) ────────────────────────────────
    float mTimeSelStart    { -1.f };   // start bar (-1 = no selection)
    float mTimeSelEnd      { -1.f };   // end bar
    float mTimeSelAnchor   {  0.f };   // anchor bar (drag origin)
    bool  mTimeSelDragging { false };

    // ── Coordinate helpers (bar/row mappings declared public above) ───────────
    int   rulerPinY() const;   // pinned-ruler band top in grid-local coords
    float snapBar  (float bar) const;
    float snapBarAlt(float bar) const; // snap ignoring Alt override

    // ── Hit testing ───────────────────────────────────────────────────────────
    int  blockAtPos     (int x, int y) const;
    bool nearRightEdge  (int blockIdx, int x) const;
    // QA-Ea Task 0c (FL pre-roll record): mirror of nearRightEdge for the
    // slip-edit mode's left-edge hit test.  Used by mouseDown + updateCursor
    // when mEditMode == Slip.
    bool nearLeftEdge   (int blockIdx, int x) const;

    // ── Selection helpers ─────────────────────────────────────────────────────
    bool isSelected (int idx) const;
    void finaliseMarquee();

    // ── Operation helpers ─────────────────────────────────────────────────────
    void deleteSelected();
    void copySelected();
    void pasteClipboard();
    void duplicateSelected();
    void nudgeSelection(int dBars, int dRows);
    void muteSelected(bool mute);
    // 2026-04-26 (D-7): Ctrl+Delete - close the gap left behind by deleting
    // every block ENTIRELY inside the time span.  Source: ruler time-range
    // first, with a fall-back to the bounding span of the current selection.
    void deleteTimeRegion();
    // 2026-04-26 (D-7 sub-3): Ctrl+Left/Right shifts the ruler time-
    // selection box by its own length.  No-op when no ruler range is set;
    // clamps t0 to bar 0 on left shift.
    void shiftTimeSelectionLeft();
    void shiftTimeSelectionRight();
private:
    void shiftTimeSelectionByLength (int direction);   // direction: -1 or +1
public:
    void showClipContextMenu(int blockIdx);
    void showAudioClipProperties(int blockIdx);
    // 2026-04-26 (D-1): right-click modifier helpers.
    void fitBlockToViewport(int blockIdx);   // Ctrl+Shift+RClick
    void showQuantizePopup();                // Alt+RClick

    // 2026-04-26 (D-2): time-marker + time-signature ruler menu + hover tooltip.
    void showRulerContextMenu(int xPx);
    void promptAddTimeMarker(int bar);
    void promptRenameTimeMarker(int idx);
    void promptAddTimeSigChange(int bar);
    void promptEditTimeSigChange(int idx);
    // QA-G Task 6: linked auto-marker spawn (fires at placement events of
    // USER-SET patterns only, docket B), the current-TS picker (docket #4
    // revision), and the follower move-across-TS prompt state.
    void afterPatternBlockPlaced (const ArrangementBlock& b);
    void showCurrentTsPicker();
    std::vector<std::array<int, 3>> mMovePreTs;   // {patternIndex, num, den} at move start
    void performSplitByEngine (int patternIndex,
                               const std::vector<std::pair<int,int>>& parts,
                               bool joinGroups);
    juce::String engineTabName (int kind, int index) const;
    // QA-TempoMap (2026-07-08): ruler tempo-flag prompts.
    void promptAddTempoChange(int bar);
    void promptEditTempoChange(int idx);
    juce::String getTooltip() override;   // TooltipClient - shows marker label / TS info on ruler hover

    // ── Paint helpers ─────────────────────────────────────────────────────────
    void drawRuler          (juce::Graphics&) const;
    void drawRowBgs         (juce::Graphics&) const;
    void drawGrid           (juce::Graphics&) const;
    void drawBlocks         (juce::Graphics&) const;
    void drawPatternClip    (juce::Graphics&, const ArrangementBlock&, int x, int y, int w, int h, bool sel) const;
    void drawAudioClip      (juce::Graphics&, const ArrangementBlock&, int x, int y, int w, int h, bool sel) const;
    void drawAutomationClip (juce::Graphics&, const ArrangementBlock&, int x, int y, int w, int h, bool sel) const;
    void drawMidiShading    (juce::Graphics&, const ArrangementBlock&, int x, int y, int w, int h) const;
    void drawMarquee        (juce::Graphics&) const;
    void drawPreviewBlock   (juce::Graphics&) const;
    void drawGhostClip      (juce::Graphics&) const;
    void drawPlayheadOverlay(juce::Graphics&) const;
    void drawPerformanceOverlays(juce::Graphics&) const;

    juce::Colour blockColour(const ArrangementBlock& b) const;
    juce::AudioThumbnail* getOrCreateThumbnail(const juce::String& path) const;

    // ── Automation helpers ────────────────────────────────────────────────────
    int  hitTestAutomPoint(int blockIdx, int x, int y) const;

    void updateCursor();
};

// ── TrackHeaderPanel ──────────────────────────────────────────────────────────
// Fixed left column (kLabelW wide) showing track row labels.
// Syncs vertical scroll with the main grid viewport.
// Right-click → track context menu (rename, delete, color, etc.).
class TrackHeaderPanel : public juce::Component
{
public:
    explicit TrackHeaderPanel(ArrangementGrid& grid, PatternManager& pm);

    void paint  (juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    // Called by BuilderPage::timerCallback() to keep labels in sync with grid scroll
    void setViewportYOffset(int yPixels);

    // TS7 §7.1: "Render Track to WAV..." on an AUDIO row.  BuilderPage supplies
    // this; the panel itself owns no render machinery.
    std::function<void(int row)> onRenderTrackRow;

    // LED hit-test geometry (shared with paint + mouseDown)
    static constexpr int kLedMuteX  = 4;
    static constexpr int kLedSoloX  = 20;
    static constexpr int kLedSize   = 12;
    static constexpr int kLabelXOff = 38;   // label starts after both LEDs

private:
    ArrangementGrid& mGrid;
    PatternManager&  mPM;
    int              mYOffset { 0 };  // pixel offset matching viewport vertical scroll

    void showTrackContextMenu(int row);
    // #21/#22 (QA-G3Smoke): contiguous run of rows sharing row's group id
    // (a lone ungrouped row = itself) -- Move Up/Down hops whole spans.
    void groupSpan (int row, int& first, int& last) const;
    int  yToRow(int y) const;

    // Mirrors BuilderPage::notifyArrangementChanged for header-panel block
    // mutations (audio-clip players re-derive trackRow; project dirty flag).
    void notifyArrangementChanged();

    // Returns row the LED was hit on, or -1. kind: 0 = mute, 1 = solo, -1 = none.
    int  hitTestLed(int x, int y, int& kind) const;
};

// ── ArrangementToolbar ────────────────────────────────────────────────────────
// Single-row toolbar above the grid: tool buttons + snap selector + zoom
class ArrangementToolbar : public juce::Component
{
public:
    ArrangementToolbar();

    // QA-Ee Stage 2: set the snap combo's selection silently (no onChange fire),
    // for initial + project-load sync from the Unified_BuilderSnapDiv param.
    // G1 boundary: the magnet button carries no value - the menu reads the
    // live div via onGetSnapDiv - so restore only re-syncs the highlight.
    void setSnapDivIndex (int idx) { if (mSnapBtn) mSnapBtn->setToggleState (idx != 0, juce::dontSendNotification); }

    std::function<void(ArrangementGrid::AGTool)> onToolSelected;
    std::function<void(int)>                     onSnapChanged;   // QA-Ee Stage 2: snap-div index 0..10
    std::function<int()>                         onGetSnapDiv;    // G1 boundary: live div for the magnet menu's tick
    std::function<void(float factor)>            onZoom;
    std::function<void()>                        onUndo;
    std::function<void()>                        onRedo;
    std::function<void()>                        onShowHistory;   // 2026-04-26 (D-1b)
    // QA-Ea Task 0c (2026-05-20): Slip/Stretch dropdown callback (fires
    // when user picks a mode from the popup).  Wired by BuilderPage to
    // ArrangementGrid::setEditMode.  setEditModeLabel is called by
    // BuilderPage on ArrangementGrid::onEditModeChanged so the button
    // label stays in sync with the grid's mEditMode.
    std::function<void(ArrangementGrid::EditMode)> onEditModeRequested;
    void setEditModeLabel (ArrangementGrid::EditMode m);

    void setActiveTool(ArrangementGrid::AGTool t);
    void setUndoEnabled(bool e) { if (mUndoBtn) mUndoBtn->setEnabled(e); }
    void setRedoEnabled(bool e) { if (mRedoBtn) mRedoBtn->setEnabled(e); }

    // Context label (right-aligned). Builder pushes "Playlist > {pattern name}".
    void setContextText(const juce::String& text);

    void paint  (juce::Graphics&) override;
    void resized() override;

    static constexpr int kHeight = 30;

private:
    // Tool buttons (8 tools post-Task-0c; SlipEdit moved out to a dropdown)
    static constexpr int kNumTools = 8;
    std::array<std::unique_ptr<juce::TextButton>, kNumTools> mToolBtns;

    // QA-Ea Task 0c (2026-05-20): Slip/Stretch dropdown button placed
    // flush after Play(Y).  Click pops a menu (Slip / Stretch); label
    // reflects current mode + 'S' keybind hint, e.g. "Slip (S) v" or
    // "Stretch (S) v".  Callback + label-setter are public (see top of
    // class) so BuilderPage can wire them.
    std::unique_ptr<juce::TextButton> mEditModeBtn;

    std::unique_ptr<juce::TextButton> mUndoBtn, mRedoBtn, mHistoryBtn;
    std::unique_ptr<juce::TextButton> mZoomInBtn, mZoomOutBtn;

    std::unique_ptr<juce::TextButton> mSnapBtn;   // magnet-style: click = 11-value menu; lit = snap active

    // Context label (e.g. "Playlist > Pattern 1")
    std::unique_ptr<juce::Label>    mContextLabel;
};


// ── BuilderPage ───────────────────────────────────────────────────────────────
class BuilderPage : public juce::Component,
                    public juce::Timer,
                    public juce::KeyListener,
                    public juce::DragAndDropContainer,
                    public juce::ScrollBar::Listener
{
public:
    BuilderPage(BaySickDAWProcessor& p, PatternManager& pm);
    ~BuilderPage() override;
    void paint             (juce::Graphics&)       override;
    void resized           ()                      override;
    void timerCallback     ()                      override;
    void visibilityChanged ()                      override;
    // Starts/stops the 30 Hz animation poll with this page's on-screen state.
    void parentHierarchyChanged ()                 override;

    // KeyListener - intercepts key events from the top-level window
    bool keyPressed        (const juce::KeyPress&, juce::Component*) override;

    // ── QA-Export: offline render ────────────────────────────────────────────
    // One harness: every entry point (song, section, pattern, per-track) fills
    // a RenderOptions and expresses itself over renderToFile.
    struct RenderOptions
    {
        enum class Format { Wav, Ogg, Mp3 };
        enum class Scope  { Pattern, Song, Section };

        // Tail::Included renders PAST the content end until the sound actually
        // decays to near-silence, rather than a fixed guess -- a typed "2.0 s"
        // truncates a long reverb and pads a dry mix.  Tail::Cut stops dead at
        // the content end.  Applies to Song and Section alike (owner call).
        enum class Tail { Included, Cut };

        Scope      scope        { Scope::Song };
        Tail       tail         { Tail::Included };
        int        patternIndex { -1 };        // Scope::Pattern only
        double     startBeats   { 0.0 };       // Scope::Section only
        double     endBeats     { 0.0 };       // Scope::Section only
        Format     format       { Format::Wav };
        double     sampleRate   { 44100.0 };
        int        bitDepth     { 24 };        // WAV only
        int        oggQuality   { 6 };         // OGG only, 0..10
        int        mp3Kbps      { 256 };       // MP3 only, CBR
        juce::File destination;

        // QA-ModelShell TS2 stems (Jeff's per-mixer-strip spec, 2026-07-27):
        // when non-empty, ONE extra file per entry -- that strip's post-chain
        // output tapped during the SAME single render pass, so sends are
        // separate stems and sidechain-driven behavior (a bass comp keyed by
        // the kick) stays in the stem by construction.  channelId is the
        // MixerChannelIds strip id (the arena slot).  Files land beside
        // `destination` as "<destBase> - <name>.<ext>", same format options.
        struct StemTarget { int channelId { -1 }; juce::String name; };
        std::vector<StemTarget> stems;

        // QA-ModelShell TS7 (§7.1 track render, §7.2 pattern render):
        //
        // mixTapChannels -- when non-empty, the MAIN file is fed by SUMMING these
        // strips' post-chain taps instead of by the master output.  One entry is a
        // single track consolidated to one WAV (the track-head render, and each
        // file of a Per Track pattern render); several entries is "Full Mix",
        // which per Jeff 2026-07-29 has ALWAYS meant stems summed and never the
        // master out -- so no master chain is on it in either case, and the two
        // are one operation over a different track set.
        //
        // writeMainFile -- false renders stems only.  Per Track needs N files and
        // no combined one, and writing a main file the user did not ask for would
        // be worse than an extra option.
        //
        // Both ride the SAME single offline pass as stems, never a pass per
        // target: muting everything but one track would kill sidechain keys and
        // strip the ducking that made the track sound the way it does.
        std::vector<int> mixTapChannels;
        bool             writeMainFile { true };

        // QA-ModelShell TS2 riders:
        // CL-043: SELECTABLE dither on 16-bit WAV writes (main file + stems
        // alike).  The entry names POW-r as a third option; it is a licensed,
        // trademarked algorithm from the POW-r Consortium and is neither ours
        // to implement nor ours to name (same class as the VST2 finding), so
        // the third slot is our own noise-shaped design -- Jeff's ruling (a),
        // 2026-07-31.
        enum class Dither { Off, Flat, NoiseShaped };
        Dither dither     { Dither::Off };
        // CL-045: LUFS-target normalization -- measure-then-gain, BOTH
        // directions, with any boost capped so the (estimated) true peak
        // stays under ceilingDbTp.  postGainDb is INTERNAL: the normalize
        // flow measures first, then re-renders with this gain applied at
        // the writers (uniformly to main + stems, so the stem sum still
        // matches the mix).
        bool  normalize   { false };
        float lufsTarget  { -14.0f };
        float ceilingDbTp { -1.0f };
        float postGainDb  { 0.0f };
    };

    // QA-ModelShell TS2 (CL-227 backend + CL-045 measure pass): the render
    // loop with METERS instead of writers -- same offline drive, same clock,
    // same lane replay, no files.
    //
    // TS7 / BLU-108: truePeakDb is now a real ITU-R BS.1770-4-geometry measurement
    // (TruePeakMeter's 4x polyphase FIR), not the 4x Lagrange ESTIMATE this
    // shipped with at TS2.  CL-045's export boost cap consumes this value, so the
    // cap tightened to the true number in the same change.
    //
    // TS7 / CL-227: the loudness-conformance fields.  `violations` is timecoded
    // and COALESCED -- consecutive breaches of the same kind inside
    // kViolationGapSeconds merge into one span, so a sustained overshoot reads as
    // one row rather than several hundred.
    struct MeasureResult
    {
        float  integratedLufs   { -120.f };
        float  truePeakDb       { -120.f };
        float  lraLu            {    0.f };   // EBU Tech 3342 loudness range
        float  maxShortTermLufs { -120.f };
        float  maxMomentaryLufs { -120.f };
        double durationSeconds  {    0.0 };

        // Hoisted to LoudnessSpec.h so the live capture poll and this offline
        // scan share ONE coalescing rule (Jeff, 2026-07-30).  The alias keeps
        // every existing MeasureResult::Violation call site working.
        using Violation = LoudnessViolation;
        std::vector<Violation> violations;
        // True when the row budget was hit.  Surfaced in the report, because a
        // silent cap reads as "only kMaxViolationRows problems exist".
        bool violationsTruncated { false };

        // TS7 §2.7: the Short-Term loudness curve, sampled at 10 Hz (EBU Tech
        // 3342's rate).  Without this the render could be SUMMARISED but not
        // DRAWN -- and the analyzer window is CL-227's face, so the same graphs
        // have to render live data and render data.  It is also what the HTML
        // report's curve is built from, so one capture serves both.
        std::vector<float> lufsCurve;
        static constexpr int kCurveHz = 10;

        // §5.1's SPECTRUM SNAPSHOT.  Averaged over the whole render, in dB, one
        // value per log-spaced band across 20 Hz - 20 kHz.
        //
        // BANDS, not raw FFT bins: a 2048-point transform gives ~1000 usable
        // bins, which as comma-separated floats inside the HTML would add
        // hundreds of KB to every report for a curve nobody reads bin-by-bin.
        // 96 log bands is the resolution the eye gets from a 900px-wide plot.
        std::vector<float> spectrumDb;
        static constexpr int kSpectrumBands = 96;
        static constexpr int kSpectrumOrder = 11;   // 2048-point
        static constexpr float kSpectrumMinHz = 20.0f;
        static constexpr float kSpectrumMaxHz = 20000.0f;

        // Summary verdicts against the spec the measurement ran with.
        LoudnessSpec::Id specId          { LoudnessSpec::Id::Streaming14 };
        bool             integratedInSpec { true };
        bool             truePeakInSpec   { true };
        // §4.2: only meaningful when specId == Custom.  Carried on the RESULT so
        // a report reloaded later reads against the number that measurement
        // actually used.
        float            customTargetLufs { -14.0f };

        // The one way to get this measurement's spec.  Never LoudnessSpec::get
        // (specId) at a call site -- that skips the Custom override.
        LoudnessSpec resolvedSpec() const noexcept
            { return LoudnessSpec::resolved (specId, customTargetLufs); }
    };

    // Aliases: the values live with the coalescing rule in LoudnessSpec.h now
    // that live capture shares it, so there is exactly one of each.
    static constexpr double kViolationGapSeconds = LoudnessViolation::kGapSeconds;
    static constexpr int    kMaxViolationRows    = LoudnessViolation::kMaxRows;

    // ── Offline session, MESSAGE THREAD ONLY (Jeff's ruling B, 2026-08-22) ──
    // Entering/leaving offline mode activates and deactivates every hosted
    // VST3, which the VST3 spec requires on the message thread.  Callers that
    // drive a render from a worker open the session BEFORE starting the worker
    // and close it after it finishes, so the render thread never waits on the
    // message thread (the export dialog joins its worker FROM the message
    // thread, so such a wait deadlocks).  Every runOfflineLoop consumer needs
    // a session open around it.
    bool enterOfflineRender (double sampleRate, juce::String& outErr);
    void leaveOfflineRender();

    bool measureRender (const RenderOptions& opts,
                        MeasureResult& out,
                        juce::String& outErr,
                        std::function<bool()> shouldAbort = {},
                        std::function<void(double)> onProgress = {},
                        LoudnessSpec::Id spec = LoudnessSpec::Id::Streaming14,
                        float customLufs = -14.0f);

    // Tail::Included safety ceiling.  Some content never decays -- a frozen
    // reverb, a self-oscillating filter, runaway feedback -- and without a cap
    // the render would run until the disk filled.  This is a MAXIMUM, not a
    // duration: a normal tail ends when it decays, usually seconds.
    static constexpr double kMaxTailSeconds = 60.0;

    // CL-056: the offline render block.  Markedly faster renders than the live
    // 512 while the per-block lane replay (~46 ms at 44.1k) stays in the same
    // class as the live 30 Hz applicator tick, so automation granularity in the
    // file matches what you hear.  ONE home: runOfflineLoop drives with it and
    // renderToFile sizes its tap-sum scratch to it.
    static constexpr int kOfflineBlock = 2048;

    // Renders synchronously on the CALLING thread -- never call from the message
    // thread for a full song (that is what runExportWithProgress is for).
    // `shouldAbort` is polled per block; returning true deletes the partial file.
    // `onProgress` receives 0..1.  Returns false + fills outErr on failure.
    bool renderToFile (const RenderOptions& opts,
                       juce::String& outErr,
                       std::function<bool()> shouldAbort = {},
                       std::function<void(double)> onProgress = {});

    // QA-ModelShell TS2: fired (true) as the offline drive takes the live
    // processor and (false) after the restore set is back -- EVERY exit path.
    // StandaloneEditor wires it to pause/resume its 30 Hz automation timer
    // (the editor marshals to the message thread; this fires on the render
    // thread).
    std::function<void(bool active)> onOfflineRenderActive;

    // The two pattern entries in the Clips menu run editor-level dialogs that
    // the page has no other route to - the same bodies the F2 / F3 commands
    // invoke.  The grid carries no local handlers for either key.
    std::function<void()> onRenamePatternRequest;
    std::function<void()> onFindNextEmptyPatternRequest;

    // QA-ModelShell TS2: offline lane replay (render thread; called per block
    // by the offline loop).  Covers the lane classes the in-processBlock
    // replay cannot: engine-APVTS lanes via the rig, rack lanes via
    // EffectParamMap + slot uuid, the rack output gain, and the legacy
    // "_fader" spelling.
    void applyOfflineAutomationAt (double songBeat);
    void applyOfflineLaneValue   (const juce::String& pid, float v01);

    // TS7 §6.1: freeze — runOfflineLoop's THIRD consumer (renderToFile and
    // measureRender are the other two; the loop is never copied).  Renders one
    // insert's PRE-RACK output to `dest` at the project rate, so the frozen tab's
    // rack, EQ and fader stay live and editable.
    //
    // Renders SONG scope: a freeze has to line up with the arrangement it
    // replaces, so it spans the same timeline rather than the tab's own content.
    //
    // `target` is the same strip expressed as a RenderTask, used ONLY to prune
    // the render graph down to it.  Null is legal and simply renders the whole
    // project as before.
    // `patternIndex` < 0 renders the SONG arrangement; otherwise just that
    // pattern, so pattern mode reads audio of the right length at a loop-local
    // position instead of the song's opening bars.
    bool renderFreezeFile (BaySickGraph::InsertKind kind, int index,
                           RenderTask* target,
                           int patternIndex,
                           const juce::File& dest,
                           juce::String& outErr,
                           std::function<bool()> shouldAbort = {},
                           std::function<void(double)> onProgress = {});

    // TS7 §6.9: every kit strip captured in ONE offline pass.  Bypasses the
    // single-arm freeze tap deliberately -- see the implementation.
    bool renderKitFreezeFiles (const std::vector<juce::File>& dests,
                               RenderTask* target,
                               int patternIndex,
                               juce::String& outErr,
                               std::function<bool()> shouldAbort = {},
                               std::function<void(double)> onProgress = {});

    // TS7 stopwatch split: offline enter/leave cost, filled by runOfflineLoop and
    // read by renderFreezeFile.  FIXED per render regardless of length -- a full
    // prepareToPlay of every engine (NAM oversampling rebuild + model prewarm,
    // sfizz, graph reset, arena resize) at both ends -- so it must be separable
    // from the per-block loop or a short render reads as catastrophically slow.
    double mFreezeSetupMs    { 0.0 };
    double mFreezeTeardownMs { 0.0 };

    // QA-ModelShell TS2: the ONE offline render loop -- span/scope math, the
    // offline drive (begin/endOfflineRender + restores), the lane-aware
    // clock, per-block lane replay, tail-decay handling.  Consumers differ
    // only in what they do with each rendered block: renderToFile writes
    // sinks (+ stem arena taps), measureRender feeds meters, TS7's freeze
    // taps one strip.  consumeBlock returns false to abort with an IO error.
    bool runOfflineLoop (const RenderOptions& opts,
                         juce::String& outErr,
                         std::function<bool()> shouldAbort,
                         std::function<void(double)> onProgress,
                         const std::function<bool (const juce::AudioBuffer<float>&, int)>& consumeBlock);

    // Message-thread entry: runs renderToFile on a background thread behind a
    // progress window with a working Cancel.
    void runExportWithProgress (const RenderOptions& opts);

    // TS7 §7.1: consolidate one arrangement row to a single WAV (audio rows only).
    void renderTrackRowToWav (int row);

    // TS7 §7.2: one tab that carries notes in a pattern.  `channelId` is the
    // strip whose post-chain tap becomes that tab's audio.
    struct PatternTrackEntry { int channelId { -1 }; juce::String name; };
    // Tabs with content in `patternIndex`, in display order.  Empty rolls are
    // skipped -- rendering a silent file per idle tab is noise, not completeness.
    std::vector<PatternTrackEntry> getPatternTracks (int patternIndex) const;
    // The options popup (Per Track / Full Mix / Select Tracks) and its two tails.
    void showPatternRenderOptions (int patternIndex);
    void showPatternTrackPicker   (int patternIndex);
    // channelIds empty == every track in the pattern; perTrack picks N files vs
    // one summed file.  One entry point so the three menu picks cannot diverge.
    void startPatternRender (int patternIndex, std::vector<int> channelIds, bool perTrack);
    void setPlayHead(StandalonePlayHead* ph);
    void setUndoContext(const UndoContext& ctx);

    // Time selection accessors (used by StandaloneEditor for loop range)
    bool  hasTimeSelection() const { return mGrid && mGrid->hasTimeSelection(); }
    float getTimeSelStartBars() const { return mGrid ? mGrid->getTimeSelStart() : 0.f; }
    float getTimeSelEndBars()   const { return mGrid ? mGrid->getTimeSelEnd()   : 0.f; }
    // The pitch-editor ruler range feeds this SONG time selection, so it loops
    // (song mode, via onGetLoopBeats) and shows on the builder ruler -- kept in
    // sync like the shared playhead.  Bars domain (4 beats/bar).
    void  setTimeSelectionBars (float startBar, float endBar) { if (mGrid) mGrid->setTimeSelection (startBar, endBar); }
    void  clearTimeSelectionBars ()                           { if (mGrid) mGrid->clearTimeSelection(); }

    // Called from toolbar ≡ menu / keyboard shortcuts
    void doImportAudio();
    void doFindNextEmptyPattern();
    void doRenamePattern();
    void doPerformanceModeToggle();
    void doZoom(float factor);
    void doNewAutomationClip();

    // Grid accessor (used by StandaloneEditor to wire EventEditor callback)
    ArrangementGrid* getGrid() { return mGrid.get(); }
    BrowserPanel*    getBrowserPanel() { return mBrowser.get(); }

    // QA-Ee Stage 2: push the current Unified_BuilderSnapDiv param value into the
    // snap combo's display (combo state isn't auto-refreshed).  Call after the
    // grid's onGetSnapDiv is wired + on project load so the combo reflects the
    // restored snap.  Snap itself always reads the param live; this is display-only.
    void syncSnapComboFromParam() { if (mGrid && mGrid->onGetSnapDiv && mToolbar) mToolbar->setSnapDivIndex (mGrid->onGetSnapDiv()); }

    // G-7 (2026-04-29): public hook for callers (StandaloneEditor) that
    // mutate the arrangement directly (e.g. closing a Clips tab sweeps the
    // blocks pointing to that clip).  Repaints the grid and fires the grid's
    // internal onArrangementChanged so audio-clip players rebuild.  Mirrors
    // the BrowserPanel::onArrangementChanged callback path.
    void notifyArrangementChanged();

    // Switch the browser pane to one of its 3 tabs (0=Patterns, 1=Audio Clips,
    // 2=Automation). Wired to the ribbon Builder dropdown.
    void setBrowserTab(int idx);

    // QA-Layout T16 (Jeff, 2026-08-04): the page's own 20px Edit/Tools/Clips/View
    // row is gone -- Clips folds into the window's Menu dropdown, Edit and View
    // become title-strip headings, and Tools was deleted outright because every
    // one of its eight entries duplicated a toolbar button already on screen.
    // Items carry action lambdas so they compose into any host menu.
    void buildClipsMenu (juce::PopupMenu& m);
    void buildEditMenu  (juce::PopupMenu& m);
    void buildViewMenu  (juce::PopupMenu& m);

private:
    BaySickDAWProcessor& mProcessor;
    PatternManager&     mPM;
    StandalonePlayHead* mPlayHead { nullptr };

    juce::AudioFormatManager   mAFM;
    juce::AudioThumbnailCache  mThumbCache { 64 };

    bool mPerfMode { false };

    std::unique_ptr<BrowserPanel>         mBrowser;

    // QA-Fe2: draggable browser right edge.  Default == minimum == the
    // pre-feature fixed width (kBrowserDefaultW); max = 3x.  Session-local
    // (not persisted) per Jeff's S9 call.
    static constexpr int kBrowserDefaultW = 180;
    int mBrowserWidth { kBrowserDefaultW };
    struct BrowserEdgeGrip : juce::Component
    {
        std::function<int()>     getWidth;
        std::function<void(int)> setWidth;
        std::function<bool()>    isCollapsed;   // Jeff 2026-08-06: divider stays live when collapsed
        int mStartW { 0 };
        BrowserEdgeGrip() { setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); }
        void mouseDown (const juce::MouseEvent&) override
        {
            // Collapsed: seed from zero so the drag-out travel is measured
            // from the closed edge, not the stale pre-collapse width.
            mStartW = (isCollapsed && isCollapsed()) ? 0
                                                     : (getWidth ? getWidth() : 0);
        }
        void mouseDrag (const juce::MouseEvent& e) override
            { if (setWidth) setWidth (mStartW + e.getDistanceFromDragStartX()); }
        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xff303640));
            if (isCollapsed && isCollapsed())
            {
                // Small right-pointing chevron: "drag me back out".
                const float cx = (float) getWidth() * 0.5f;
                const float cy = (float) getHeight() * 0.5f;
                juce::Path p;
                p.startNewSubPath (cx - 1.5f, cy - 5.0f);
                p.lineTo          (cx + 1.5f, cy);
                p.lineTo          (cx - 1.5f, cy + 5.0f);
                g.setColour (VC::Text.withAlpha (0.75f));
                g.strokePath (p, juce::PathStrokeType (1.5f));
            }
        }
    };
    std::unique_ptr<BrowserEdgeGrip>      mBrowserGrip;

    std::unique_ptr<TrackHeaderPanel>     mTrackHeader;
    std::unique_ptr<ArrangementToolbar>   mToolbar;
    std::unique_ptr<juce::Viewport>       mGridViewport;
    std::unique_ptr<ArrangementGrid>      mGrid;

    // External horizontal scrollbar under the grid: the ONE horizontal scroll
    // mechanism (drives the grid's virtual mBarOff; the viewport scrolls
    // vertical only).  Range recomputes on the UI timer so it tracks content
    // + the current view dynamically (shrinks back when the view returns from
    // empty territory).
    std::unique_ptr<juce::ScrollBar>      mGridHScroll;
    void scrollBarMoved(juce::ScrollBar*, double newRangeStart) override;
    void syncGridHScroll();

    void syncToolbar();
};
