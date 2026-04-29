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
// (Patterns / Audio / Automation) — same UI, same interactions.
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

// ── BrowserPanel ──────────────────────────────────────────────────────────────
// Collapsible left panel — 3 filter tabs: Patterns | Audio | Automation.
// Each item is a single draggable BrowserItem (no separate text editor).
class BrowserPanel : public juce::Component
{
    // NOTE: BuilderPage is the DragAndDropContainer — not this panel — so
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

    // Public alias for switchTab — used by BuilderPage::setBrowserTab + ribbon dropdown.
    void selectTab(int t) { switchTab(t); }

    std::function<void(int)>                  onPatternSelected;
    std::function<void(int)>                  onRenderPattern;   // right-click → Render to WAV
    std::function<void(const juce::String&)>  onImportAudio;     // File → Import Audio (path)
    std::function<void()>                     onArrangementChanged; // delete-block triggers → rebuild
    // Resolves an AutomationLane to a display label (honors userDisplayName,
    // falls back to the auto-generated "Channel - Effect - Param" format).
    // Set by StandaloneEditor; null on construction.
    std::function<juce::String(const AutomationLane&)> onResolveDisplayName;

private:
    PatternManager&            mPM;
    juce::AudioFormatManager&  mAFM;
    juce::AudioThumbnailCache& mThumbCache;
    bool mCollapsed { false };
    int  mActiveTab { 0 };

    std::unique_ptr<juce::TextButton> mCollapseBtn;
    std::array<std::unique_ptr<juce::TextButton>, 3> mTabBtns;

    std::vector<std::unique_ptr<BrowserItem>> mPatItems;
    std::vector<std::unique_ptr<BrowserItem>> mAudioItems;
    std::vector<std::unique_ptr<BrowserItem>> mAutomItems;
    int mSelectedPat { 0 };

    std::unique_ptr<juce::TextButton> mAddBtn;
    std::unique_ptr<juce::TextButton> mDeleteBtn;

    // Audio file paths keyed by item index (so right-click "Remove" knows the path)
    juce::StringArray mAudioPaths;
    // Automation block indices keyed by item index
    std::vector<int>  mAutomBlockIndices;

    // Diff-guard for refresh() so the timer doesn't blindly destroy items.
    juce::String      mLastRefreshSnapshot;

    void rebuildPatternRows();
    void rebuildAudioRows();
    void rebuildAutomationRows();
    void switchTab(int t);
    void selectPattern(int idx);

    // ── Item interaction helpers ─────────────────────────────────────────
    void openRenamePopup(BrowserItem& item);
    void showItemContextMenu(BrowserItem& item, juce::Point<int> globalPt);

    void renamePatternAt   (int idx, const juce::String& newName);
    void renameAudioAt     (int idx, const juce::String& newName);
    void renameAutomationAt(int idx, const juce::String& newName);
};

// ── SnapMode ──────────────────────────────────────────────────────────────────
enum class SnapMode { Bar, Beat, Cell, Line, Steps, Events, None };

// ── ArrangementGrid ───────────────────────────────────────────────────────────
// Piano-roll-style arrangement editor.
// Label column is EXTERNAL (TrackHeaderPanel) — grid starts at x=0 for bar 0.
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
    enum class AGTool {
        Draw, Paint, Select, Delete, Mute,
        SlipEdit, Slice, Zoom, PlaySelected
    };

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

    // FileDragAndDropTarget — accept audio files dropped from OS
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void fileDragMove(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray&) override;

    // DragAndDropTarget — accept BrowserItem drags from the BrowserPanel.
    // Description format: "kind:index" (kind = "pattern" / "audio" / "auto").
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDragEnter(const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped  (const SourceDetails&) override;

    void setTool(AGTool t);
    AGTool getTool() const { return mActiveTool; }

    void setSnapMode(SnapMode s) { mSnapMode = s; }
    SnapMode getSnapMode() const { return mSnapMode; }

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
    // P4: copy-on-drop.  Called with the external source file; caller either
    // copies it into the current project's Samples/ folder and returns the
    // relative string to store (e.g. "Samples/kick.wav"), or returns {} to
    // reject the drop (e.g. no project open — caller also shows the user an
    // explanation dialog).  When this callback is unset, imports fall back to
    // storing the absolute path (pre-P4 behavior).
    std::function<juce::String(const juce::File& externalFile)> onImportSampleRequest;
    // P4: resolves a stored path (possibly relative) to an absolute file.
    // When unset, falls back to juce::File(stored) which treats the string
    // as an absolute path (pre-P4 behavior).
    std::function<juce::File(const juce::String& stored)> onResolveStoredPath;
    // P4: called when onImportSampleRequest rejects the drop because no
    // project is open.  Caller prompts for a new project name (async),
    // creates it, then re-invokes importAudioFile with the original args so
    // the drop completes after project creation.
    std::function<void(const juce::File& src, int row, float bar)> onDropWithoutProject;
    // Resolves an AutomationLane to a display label. Used by drawAutomationClip
    // so on-grid blocks show "Channel - Effect - Param" (or the user's rename)
    // instead of the raw paramId. Null = fall back to paramId.
    std::function<juce::String(const AutomationLane&)> onResolveDisplayName;

    // ── Row names (shared with TrackHeaderPanel) ──────────────────────────────
    const std::array<juce::String, kNumRows>& getRowNames() const { return mRowNames; }
    void setRowName(int row, const juce::String& name);

    // ── View state (public — BuilderPage accesses these directly) ────────────
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
    void importAudioFile(const juce::String& path, int targetRow, float targetBar);

    // ── Public coordinate helpers (used by BuilderPage zoom anchoring) ────
    int   barToX(float bar) const;
    float xToBar(int x)     const;

private:
    PatternManager&            mPM;
    juce::AudioFormatManager&  mAFM;
    juce::AudioThumbnailCache& mThumbCache;

    // Thumbnail cache: filePath → AudioThumbnail (mutable so const draw methods can populate)
    mutable std::map<juce::String, std::unique_ptr<juce::AudioThumbnail>> mThumbnails;

    // ── Tool + snap ───────────────────────────────────────────────────────────
    AGTool   mActiveTool    { AGTool::Draw };
    SnapMode mSnapMode      { SnapMode::Bar };
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

    // Slip edit (shift audio content without moving block boundaries)
    bool  mSlipping     { false };
    int   mSlipIdx      { -1 };
    int   mSlipDragX    { 0 };

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

    // Zoom-rect drag (Ctrl+RClick drag — D-1 2026-04-26)
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
    // 2026-04-26 (D-7): Ctrl+Delete — close the gap left behind by deleting
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
    juce::String getTooltip() override;   // TooltipClient — shows marker label / TS info on ruler hover

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

    std::function<void(ArrangementGrid::AGTool)> onToolSelected;
    std::function<void(SnapMode)>                onSnapChanged;
    std::function<void(float factor)>            onZoom;
    std::function<void()>                        onUndo;
    std::function<void()>                        onRedo;
    std::function<void()>                        onShowHistory;   // 2026-04-26 (D-1b)

    void setActiveTool(ArrangementGrid::AGTool t);
    void setUndoEnabled(bool e) { if (mUndoBtn) mUndoBtn->setEnabled(e); }
    void setRedoEnabled(bool e) { if (mRedoBtn) mRedoBtn->setEnabled(e); }

    // Context label (right-aligned). Builder pushes "Playlist > {pattern name}".
    void setContextText(const juce::String& text);

    void paint  (juce::Graphics&) override;
    void resized() override;

    static constexpr int kHeight = 30;

private:
    // Tool buttons (9 tools)
    static constexpr int kNumTools = 9;
    std::array<std::unique_ptr<juce::TextButton>, kNumTools> mToolBtns;

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

    // KeyListener — intercepts key events from the top-level window
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

    // Menu bar (sits above toolbar, replaces ≡ popup)
    std::unique_ptr<juce::MenuBarComponent> mMenuBar;
    std::unique_ptr<BuilderMenuBar>         mMenuBarModel;
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
