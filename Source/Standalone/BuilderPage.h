#pragma once
#include "UndoActions.h"
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../PatternManager.h"
#include "SharedUI.h"
#include "StandaloneApp.h"

// ── PatternRowButton ──────────────────────────────────────────────────────────
// TextButton that also fires onRightClick for right-click context menus.
class PatternRowButton : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;
    std::function<void()> onRightClick;

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
            { if (onRightClick) onRightClick(); }
        else
            TextButton::mouseDown(e);
    }
};

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
    enum class Kind { Pattern, Audio, Automation };

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
// CategorizedAudioEntry - one record per file the browser shows in the Audio
// tree.  StandaloneEditor's onEnumerateAudio callback walks mPages and emits
// one entry per ClipsPage / VoxPage / InstPage with a bound file.  Orphan
// audioLibrary entries (no bound page) are skipped - there is NO 4th
// "Imported" / "Library" bucket per Jeff's invariant ("all importable files
// become clips").
struct CategorizedAudioEntry
{
    int          audioLibIdx { -1 };   // index into mPM.audioLibrary for drag descriptor
    juce::String category;             // "Clips" | "Vox" | "Inst"
    juce::String displayName;          // shown as leaf label
    juce::String fullPath;             // for tooltip + Reveal in Explorer
    juce::Colour accent;               // category accent color
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
    juce::var     getDragSourceDescription () override           { return juce::String("audio:") + juce::String(mEntry.audioLibIdx); }
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
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh();

    bool isCollapsed() const { return mCollapsed; }
    void setCollapsed(bool c);

    int  getSelectedPatternIndex() const { return mSelectedPat; }

    // Public alias for switchTab - used by BuilderPage::setBrowserTab + ribbon dropdown.
    void selectTab(int t) { switchTab(t); }

    std::function<void(int)>                  onPatternSelected;
    std::function<void(int)>                  onRenderPattern;   // right-click → Render to WAV
    std::function<void(const juce::String&)>  onImportAudio;     // File → Import Audio (path)
    std::function<void()>                     onArrangementChanged; // delete-block triggers → rebuild
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

private:
    PatternManager&            mPM;
    juce::AudioFormatManager&  mAFM;
    juce::AudioThumbnailCache& mThumbCache;
    bool mCollapsed { false };
    int  mActiveTab { 0 };

    std::unique_ptr<juce::TextButton> mCollapseBtn;
    std::array<std::unique_ptr<juce::TextButton>, 3> mTabBtns;

    std::vector<std::unique_ptr<BrowserItem>> mPatItems;
    std::vector<std::unique_ptr<BrowserItem>> mAudioItems;     // G-5 legacy - kept empty post-tree migration
    std::vector<std::unique_ptr<BrowserItem>> mAutomItems;
    int mSelectedPat { 0 };

    // G-5 (2026-04-29): unified audio tree replacing the flat mAudioItems
    // when Audio tab is active.  Owns a hidden root TreeViewItem populated
    // with 3 category nodes (Clips / Vox / Inst) - leaves come from
    // onEnumerateAudio().  Tree visibility tracks mActiveTab == 1.
    std::unique_ptr<juce::TreeView>           mAudioTree;
    std::unique_ptr<juce::TreeViewItem>       mAudioRoot;
    AudioCategoryItem*                        mClipsCat { nullptr };  // raw - owned by mAudioRoot
    AudioCategoryItem*                        mVoxCat   { nullptr };
    AudioCategoryItem*                        mInstCat  { nullptr };

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
// onGetSnapDiv().  See VibesynthConstants.h kUnifiedSnapLabels / snapDivToTicks.

// ── ArrangementGrid ───────────────────────────────────────────────────────────
// Piano-roll-style arrangement editor.
// Label column is EXTERNAL (TrackHeaderPanel) - grid starts at x=0 for bar 0.
//
// Tools: P=Draw  B=Paint  E=Select  D=Delete  T=Mute  S=SlipEdit  C=Slice
//        Shift+Z=Zoom  Y=PlaySelected
// Zoom:    Ctrl+Scroll (horizontal)
// Pan:     Scroll (horizontal)  |  Viewport scroll (vertical)
// Keys:    Ctrl+Z/Y undo/redo, Ctrl+A/C/V/B, Delete, Shift+Arrows, Escape
// ─────────────────────────────────────────────────────────────────────────────
class ArrangementGrid : public juce::Component,
                        public juce::FileDragAndDropTarget,
                        public juce::DragAndDropTarget,
                        public juce::TooltipClient   // 2026-04-26 (D-2): ruler hover tooltips
{
public:
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

    void setPlayheadBar(double bar) { mPlayheadBar = bar; repaint(); }
    void setPerformanceMode(bool on) { mPerfMode = on; repaint(); }
    void setTimeSignature(TimeSignature ts) { mTimeSig = ts; repaint(); }

    void setSelectedPatternIndex(int idx) { mBrowserSelection = idx; }

    // Ghost clip for Source Picker drag preview
    void setGhostClip(const juce::OptionalScopedPointer<ArrangementBlock>* ghost);
    void clearGhostClip();
    void placeGhostClip();

    // ── Selection ─────────────────────────────────────────────────────────────
    void selectAll     ();
    void clearSelection();
    bool hasSelection  () const { return !mSelection.empty(); }

    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr int kRowH       = 40;
    static constexpr int kRulerH     = 18;
    static constexpr int kLabelW     = 120;   // kept for external use (BuilderPage layout)
    static constexpr int kNumRows    = 50;
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
                       const std::array<juce::String, kNumRows>& rowNames);
    void undo() { if (mUndoCtx.undo) { mUndoCtx.undo(); if (onUndoRedoStateChanged) onUndoRedoStateChanged(); } }
    void redo() { if (mUndoCtx.redo) { mUndoCtx.redo(); if (onUndoRedoStateChanged) onUndoRedoStateChanged(); } }
    void showHistory() { if (mUndoCtx.showHistory) mUndoCtx.showHistory(); }   // 2026-04-26 (D-1b)
    bool canUndo() const { return mUndoCtx.isValid() && mUndoCtx.manager->canUndo(); }
    bool canRedo() const { return mUndoCtx.isValid() && mUndoCtx.manager->canRedo(); }

    // ── Callbacks ─────────────────────────────────────────────────────────────
    std::function<void(AGTool)>      onToolChanged;
    std::function<void()>            onUndoRedoStateChanged;
    std::function<void(double beat)> onSeek;               // ruler bare-click → seek playhead
    std::function<void()>                              onImportAudioRequested;  // File → Import Audio
    std::function<void()>                              onRowHeightChanged;      // Alt+scroll → notify TrackHeaderPanel
    std::function<void(int)>                           onOpenEventEditor;       // double-click or ctx-menu on Automation clip → open EventEditor
    // 2026-04-28 (G-2): signature extended to also pass the imported audio
    // file path so listeners can spawn per-clip Clips tabs without re-deriving
    // the path from the row.
    std::function<void(int row, const juce::String& rowName, const juce::String& filePath)> onAudioClipAdded;
    std::function<void()>                              onArrangementChanged;    // any block move/resize/delete → rebuild audio clip players
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

    // QA-Ea Task 0c (FL pre-roll record + non-destructive clip trim): three
    // callbacks supporting the slip-edit drag.  Wired by BuilderPage to
    // the transport / processor:
    //   onGetBPM               -> transport's getBPM()
    //   onGetSampleRate        -> processor's getSampleRate()
    //   onRequestRebuildPlayers-> processor's rebuildAudioClipPlayers()
    // (The slip-edit mode read used to be a callback to the editor's
    // mSlipEditMode flag; replaced 2026-05-20 with internal mEditMode.)
    std::function<double()> onGetBPM;
    std::function<double()> onGetSampleRate;
    std::function<void()>   onRequestRebuildPlayers;
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

    // ── Row names (shared with TrackHeaderPanel) ──────────────────────────────
    const std::array<juce::String, kNumRows>& getRowNames() const { return mRowNames; }
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
    float getEffectiveRowH() const { return mEffectiveRowH; }

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

    // ── Public coordinate helpers (used by BuilderPage zoom anchoring) ────
    int   barToX(float bar) const;
    float xToBar(int x)     const;

private:
    PatternManager&            mPM;
    juce::AudioFormatManager&  mAFM;
    juce::AudioThumbnailCache& mThumbCache;

    // Thumbnail cache: filePath → AudioThumbnail (mutable so const draw methods can populate)
    mutable std::map<juce::String, std::unique_ptr<juce::AudioThumbnail>> mThumbnails;

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

    // ── Time signature ────────────────────────────────────────────────────────
    TimeSignature mTimeSig;

    // ── Browser selection ─────────────────────────────────────────────────────
    int mBrowserSelection { 0 };

    // ── Row names ─────────────────────────────────────────────────────────────
    std::array<juce::String, kNumRows> mRowNames;

    // ── Ghost clip (drag preview from Source Picker) ──────────────────────────
    bool           mHasGhost   { false };
    ArrangementBlock mGhostBlock;
    float          mGhostAlpha { 0.5f };

    // ── File drag ghost ────────────────────────────────────────────────────────
    bool         mFileDragActive { false };
    juce::String mFileDragPath   {};
    int          mFileDragX      { 0 };
    int          mFileDragY      { 0 };

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
    int           mSlipEditDragOrigX       { 0 };
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
    int   mResizeIdx      { -1 };
    float mResizeOrigLen  { 0.f };
    float mResizeOrigStart{ 0.f };
    std::vector<ControlPoint> mResizeOrigPoints;  // automation point positions before resize

    // ── Inline automation curve handle drag ───────────────────────────────────
    int   mAutomCurveHandleDrag { -1 };  // original index in lane->points of left point, -1=none
    float mAutomCurveHandleOrigTension { 0.f };

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
    juce::String                       mPendingLabel;

    // ── Clipboard ─────────────────────────────────────────────────────────────
    std::vector<ArrangementBlock> mClipboard;

    // ── Automation point editing (Draw tool on Automation clips) ──────────────
    int          mAutomEditBlock { -1 };   // block index being edited
    int          mAutomDragPoint { -1 };   // point index being dragged (-1 = none)
    bool         mAutomDragging  { false };
    ControlPoint mAutomDragOrig  {};       // state before drag (for commitEdit)

    // ── Time selection (Ctrl+drag on ruler) ────────────────────────────────
    float mTimeSelStart    { -1.f };   // start bar (-1 = no selection)
    float mTimeSelEnd      { -1.f };   // end bar
    float mTimeSelAnchor   {  0.f };   // anchor bar (drag origin)
    bool  mTimeSelDragging { false };

    // ── Coordinate helpers (barToX/xToBar declared public above) ──────────────
    int   rowToY   (int row)   const;
    int   yToRow   (int y)     const;
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
    int  totalVisibleBars() const;
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
    int  yToRow(int y) const;

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
    void setSnapDivIndex (int idx) { if (mSnapCombo) mSnapCombo->setSelectedId (idx + 1, juce::dontSendNotification); }

    std::function<void(ArrangementGrid::AGTool)> onToolSelected;
    std::function<void(int)>                     onSnapChanged;   // QA-Ee Stage 2: snap-div index 0..10
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

    std::unique_ptr<juce::Label>    mSnapLabel;
    std::unique_ptr<juce::ComboBox> mSnapCombo;

    // Context label (e.g. "Playlist > Pattern 1")
    std::unique_ptr<juce::Label>    mContextLabel;
};

class BuilderMenuBar;   // defined after BuilderPage

// ── BuilderPage ───────────────────────────────────────────────────────────────
class BuilderPage : public juce::Component,
                    public juce::Timer,
                    public juce::KeyListener,
                    public juce::DragAndDropContainer
{
public:
    BuilderPage(VibeSynthProcessor& p, PatternManager& pm);
    ~BuilderPage() override;
    void paint             (juce::Graphics&)       override;
    void resized           ()                      override;
    void timerCallback     ()                      override;
    void visibilityChanged ()                      override;

    // KeyListener - intercepts key events from the top-level window
    bool keyPressed        (const juce::KeyPress&, juce::Component*) override;

    void renderPatternToWav(int patternIndex);
    void setPlayHead(StandalonePlayHead* ph);
    void setUndoContext(const UndoContext& ctx);

    // Time selection accessors (used by StandaloneEditor for loop range)
    bool  hasTimeSelection() const { return mGrid && mGrid->hasTimeSelection(); }
    float getTimeSelStartBars() const { return mGrid ? mGrid->getTimeSelStart() : 0.f; }
    float getTimeSelEndBars()   const { return mGrid ? mGrid->getTimeSelEnd()   : 0.f; }

    // Called from toolbar ≡ menu / keyboard shortcuts
    void doImportAudio();
    void doNew();
    void doSave();
    void doOpen();
    void doExport();
    void doFindNextEmptyPattern();
    void doRenamePattern();
    void doPerformanceModeToggle();
    void doZoom(float factor);
    void doToggleBrowser();
    void doNavigatePage(int pageIndex);  // 0=Layers, 1=Bass, 2=Drums, 3=Builder
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

private:
    VibeSynthProcessor& mProcessor;
    PatternManager&     mPM;
    StandalonePlayHead* mPlayHead { nullptr };

    juce::AudioFormatManager   mAFM;
    juce::AudioThumbnailCache  mThumbCache { 64 };

    bool mPerfMode { false };

    std::unique_ptr<BrowserPanel>         mBrowser;
    std::unique_ptr<TrackHeaderPanel>     mTrackHeader;
    std::unique_ptr<ArrangementToolbar>   mToolbar;
    std::unique_ptr<juce::Viewport>       mGridViewport;
    std::unique_ptr<ArrangementGrid>      mGrid;

    // Menu bar (sits above toolbar, replaces hamburger popup).
    // QA-D Task 4 / QA-0a finding #8: model declared FIRST so it outlives
    // the MenuBarComponent during reverse-order destruction.
    std::unique_ptr<BuilderMenuBar>         mMenuBarModel;
    std::unique_ptr<juce::MenuBarComponent> mMenuBar;
    static constexpr int kMenuBarH = 20;

    friend class BuilderMenuBar;
    void syncToolbar();
};

// ── Builder menu bar model ────────────────────────────────────────────────────
class BuilderMenuBar : public juce::MenuBarModel
{
public:
    explicit BuilderMenuBar(BuilderPage& owner) : mOwner(owner) {}
    ~BuilderMenuBar() override { setApplicationCommandManagerToWatch(nullptr); }

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu   getMenuForIndex (int index, const juce::String& name) override;
    void              menuItemSelected(int itemId, int topLevelIndex) override;

private:
    BuilderPage& mOwner;
};
