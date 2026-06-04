#pragma once
#include <JuceHeader.h>
#include "../PatternManager.h"
#include "../VibesynthConstants.h"
#include "SharedUI.h"
#include "UndoActions.h"

// ─────────────────────────────────────────────────────────────────────────────
// DrumKitGrid - Drum-Kit-tab "piano roll" rebuilt from PianoRoll.cpp's shell.
// ─────────────────────────────────────────────────────────────────────────────
// Same visual + interaction model as PianoRollGrid, but the rows map to
// per-drum piano-roll data (mPM->currentPattern().drumRolls[0..15]) instead
// of pitch.  All the standard piano-roll tools, keybinds, zoom, scroll,
// undo, selection, marquee, time-selection, copy/paste/duplicate, control
// lane (velocity/pan only), and menu bar are preserved.
//
// Discarded (don't apply to drums):
//   - 88-key piano keyboard widget (replaced by DrumKitSidebar)
//   - Pitch-based scale-snap
//   - Vertical scrollbar (16 rows is fixed)
//   - Pitch-bend control lane mode
//   - Slide / Portamento note types
//   - Row scale-tinting / black-key shading
// ─────────────────────────────────────────────────────────────────────────────

class DrumKitContainer;
class DrumKitMenuBar;

// ── Per-row drum info passed in from the editor ──────────────────────────────
struct DrumKitRowInfo
{
    int          pageIndex { -1 };
    juce::String displayName;
    bool         hasEngine  { false };
    bool         locked     { false };
    bool         isActive   { false };
    juce::Colour color       { 0xff999999 };
};

// ─────────────────────────────────────────────────────────────────────────────
// DrumKitSidebar - left strip of the kit grid, replacement for PianoKeyboard
// ─────────────────────────────────────────────────────────────────────────────
class DrumKitSidebar : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    static constexpr int kNumRows  = 16;
    static constexpr int kHandleW  = 14;
    static constexpr int kPickerW  = 120;
    static constexpr int kMuteW    = 20;
    static constexpr int kSoloW    = 20;
    static constexpr int kKeyW     = 28;
    static constexpr int kWidth    = kHandleW + kPickerW + kMuteW + kSoloW + kKeyW;

    DrumKitSidebar();

    void paint    (juce::Graphics&)                                         override;
    void resized  ()                                                        override;
    void mouseDown(const juce::MouseEvent&)                                 override;
    void mouseDrag(const juce::MouseEvent&)                                 override;
    void mouseUp  (const juce::MouseEvent&)                                 override;
    void mouseWheelMove(const juce::MouseEvent&,
                        const juce::MouseWheelDetails&)                     override;

    void setRowMetrics    (int rulerH, int rowH);
    void refresh();

    void setApvts          (juce::AudioProcessorValueTreeState* a);
    void setKitRowProvider (std::function<std::vector<DrumKitRowInfo>()> fn);
    void setRowClickHandler(std::function<void(int rowIdx, juce::Component*)> fn);
    void setAuditionHandlers(std::function<void(int rowIdx)> onOn,
                             std::function<void(int rowIdx)> onOff);
    void setReorderHandler  (std::function<void(int srcRow, int dstRow)> fn);

    std::function<void(const juce::MouseEvent&,
                       const juce::MouseWheelDetails&)>    onWheel;

    // 2026-04-26: Global Lock/Unlock - fired when the thin ruler-space button
    // above the picker dropdowns is clicked.  Container forwards to DrumPage
    // and ultimately StandaloneEditor for the confirm-prompt + actual toggle.
    std::function<void()> onGlobalLockRequested;

    int rowFromY (int y) const;

private:
    int mRulerH { 14 };
    int mRowH   { 24 };

    juce::AudioProcessorValueTreeState* mApvts { nullptr };
    std::function<std::vector<DrumKitRowInfo>()>     mProvider;
    std::function<void(int, juce::Component*)>        mRowClickHandler;
    std::function<void(int)>                          mAuditionOn;
    std::function<void(int)>                          mAuditionOff;
    std::function<void(int, int)>                     mReorderHandler;
    std::vector<DrumKitRowInfo>                       mRows;

    std::array<std::unique_ptr<juce::TextButton>,   kNumRows> mPickers;
    std::array<std::unique_ptr<MixerLedButton>,     kNumRows> mMuteBtns;
    std::array<std::unique_ptr<MixerLedButton>,     kNumRows> mSoloBtns;
    // 2026-04-26: thin global lock/unlock toggle in ruler row above pickers.
    std::unique_ptr<juce::TextButton>                         mGlobalLockBtn;
    using BtnAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::array<std::unique_ptr<BtnAtt>, kNumRows>             mMuteAtt;
    std::array<std::unique_ptr<BtnAtt>, kNumRows>             mSoloAtt;

    int  mActiveAuditionRow { -1 };
    int  mDragSourceRow { -1 };
    int  mDragTargetRow { -1 };
    bool mReorderActive { false };

    enum class HitArea { None, DragHandle, AuditionKey };
    HitArea hitArea (juce::Point<int> p, int& outRow) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumKitSidebar)
};

// ─────────────────────────────────────────────────────────────────────────────
// DrumKitGrid - the main note-editing area
// ─────────────────────────────────────────────────────────────────────────────
class DrumKitGrid : public juce::Component
{
public:
    enum class PRTool { Draw, Paint, Delete, Mute, Slice, Select, Zoom };

    struct NoteRef
    {
        int row { -1 };
        int idx { -1 };
        bool operator==(const NoteRef& o) const noexcept
            { return row == o.row && idx == o.idx; }
        bool isValid() const noexcept { return row >= 0 && idx >= 0; }
    };

    DrumKitGrid();
    void paint         (juce::Graphics&)                                          override;
    void mouseMove     (const juce::MouseEvent&)                                  override;
    void mouseDown     (const juce::MouseEvent&)                                  override;
    void mouseDrag     (const juce::MouseEvent&)                                  override;
    void mouseUp       (const juce::MouseEvent&)                                  override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&)  override;
    void mouseDoubleClick(const juce::MouseEvent&)                                override;
    bool keyPressed    (const juce::KeyPress&)                                    override;

    void setScrollState   (float ppb, double beatOff, int rowH, int snapDenom);
    void setPatternManager(PatternManager* pm);
    void setKitRowProvider(std::function<std::vector<DrumKitRowInfo>()> fn);
    // Re-fetch the row info from the provider.  Call when the drum list
    // changes (drum added/removed/renamed/active-tab switched).
    void refreshRowsCache ();
    void setPlayheadBeat  (double beat);
    void setTool          (PRTool t);
    PRTool getActiveTool  () const { return mActiveTool; }

    void selectAll      ();
    void clearSelection ();
    void invertSelection();
    bool hasSelection   () const { return !mSelection.empty(); }
    // 2026-04-26 (D-7 sub-4): public wrapper so the DrumKitContainer's lane
    // callback can ask the grid whether a given (row, idx) ref is selected.
    bool isRefSelected (NoteRef ref) const;

    void showToolsMenu();
    void copySelected      ();
    void pasteClipboard    ();
    void deleteSelected    ();
    void duplicateSelected ();
    void toolQuantize      ();
    // 2026-04-26 (D-7): drum-applicable bundle helpers (quickLegato +
    // transposeSelection are NOT mirrored here - drum hits don't legato and
    // drum rows are slot-based, not pitch).
    void quickQuantizeQuarter ();   // Ctrl+Q
    void flamSelected         ();   // Alt+F
    void deleteTimeRegion     ();   // Ctrl+Delete (uses ruler time-range)
    void toggleResizeFromLeftMode ();   // Ctrl+Alt+Home
    void scaleSelectionLevels ();   // Alt+X (modal velocity scale)
    void shiftTimeSelectionLeft ();   // Ctrl+Left
    void shiftTimeSelectionRight ();  // Ctrl+Right

    void setUndoContext(const UndoContext& ctx) { mUndoCtx = ctx; }
    void beginEdit (const juce::String& label = "Edit");
    void commitEdit();

    using DrumKitSnapshot = std::array<std::vector<PianoNote>, kMaxDrumPages>;
    DrumKitSnapshot takeSnapshot() const;
    void            applySnapshot(const DrumKitSnapshot& snap);

    bool canUndo() const { return mUndoCtx.isValid() && mUndoCtx.manager->canUndo(); }
    bool canRedo() const { return mUndoCtx.isValid() && mUndoCtx.manager->canRedo(); }

    void setSnapEnabled(bool b);

    std::function<void(float deltaPPB)>           onZoom;
    std::function<void(float factor, int anchorX)> onZoomAnchored; // QA-Ee: cursor-anchored wheel zoom
    std::function<void(double dBeats)>            onHScroll;
    std::function<void(float factor)>             onVZoom;
    std::function<void()>                         onNotesChanged;
    std::function<void(PRTool)>                   onToolChanged;
    std::function<void()>                         onUndoRedoStateChanged;
    std::function<void(double beatStart,
                       double beatEnd)>           onZoomTo;
    std::function<void()>                         onZoomToggle;
    std::function<int()>                          onGetSnapDiv; // QA-Ee Stage 3: live read of the global snap div
    std::function<void(double beat)>              onSeek;

    std::function<void(int rowIdx)>               onRowAuditionOn;
    std::function<void(int rowIdx)>               onRowAuditionOff;

    bool   hasTimeSelection()     const { return mTimeSelBeatStart >= 0.0; }
    double getTimeSelBeatStart()  const { return mTimeSelBeatStart; }
    double getTimeSelBeatEnd()    const { return mTimeSelBeatEnd; }
    void   clearTimeSelection()         { mTimeSelBeatStart = mTimeSelBeatEnd = -1.0; repaint(); }

    static constexpr int kRulerH      = 14;
    static constexpr int kResizeZone  =  6;
    static constexpr int kDefaultRowH = 24;
    static constexpr int kMinRowH     = 18;
    static constexpr int kMaxRowH     = 60;
    static constexpr int kKitMidiNote = 60;

    void setRowYOffset(int yOff)                  { mRowYOffset = yOff; repaint(); }

private:
    float  mPPB        { 80.f };
    double mBeatOff    { 0.0 };
    int    mRowH       { kDefaultRowH };
    int    mSnapDenom  { 16 };
    double mPlayhead   { -1.0 };
    int    mRowYOffset { kRulerH };

    PatternManager* mPM { nullptr };
    std::function<std::vector<DrumKitRowInfo>()> mRowProvider;
    std::vector<DrumKitRowInfo>                  mRowsCache;

    PRTool mActiveTool { PRTool::Draw };

    bool   mDrawing   { false };
    bool   mErasing   { false };
    int    mDrawRow   { -1 };
    double mDrawStart { 0.0 };
    double mDrawEnd   { 0.25 };
    // 2026-04-26 (D-7): FL-style click memory (length only - drum hits don't
    // carry slide / portamento types).  Drag-to-place still wins.
    double mClickMemoryDur { 0.25 };
    bool   mDrawHasDragged { false };

    bool                 mMoving { false };
    juce::Point<int>     mMoveDragOrigin;
    std::vector<NoteRef> mMoveRefs;
    std::vector<int>     mMoveOrigRow;
    std::vector<double>  mMoveOrigBeats;

    int  mAuditionHeldRow { -1 };
    void triggerAudition (int rowIdx);
    void releaseAudition ();

    bool    mResizing        { false };
    NoteRef mResizeRef;
    double  mResizeOrigDur   { 0.0 };
    double  mResizeOrigStart { 0.0 };
    // 2026-04-26 (D-7): Ctrl+Alt+Home flips this - drag-to-resize then grabs
    // the LEFT edge instead of the right.  mResizingFromLeft locks at
    // mouseDown so the gesture's direction stays stable.
    bool    mResizeFromLeftEnabled { false };
    bool    mResizingFromLeft      { false };

    bool                 mMarqueeActive { false };
    juce::Point<int>     mMarqueeStart;
    juce::Rectangle<int> mMarqueeRect;
    bool                 mMarqueeWasCtrl { false };

    bool             mSlicing    { false };
    juce::Point<int> mSliceStart, mSliceEnd;

    bool                 mZoomRectActive { false };
    juce::Point<int>     mZoomRectStart;
    juce::Rectangle<int> mZoomRect;

    double mTimeSelBeatStart  { -1.0 };
    double mTimeSelBeatEnd    { -1.0 };
    double mTimeSelBeatAnchor {  0.0 };
    bool   mTimeSelDragging   { false };

    std::vector<NoteRef> mSelection;

    UndoContext     mUndoCtx;
    DrumKitSnapshot mPendingBefore;
    juce::String    mPendingLabel;

    struct ClipboardCell
    {
        int       relRow      { 0 };
        PianoNote note        {};
    };
    std::vector<ClipboardCell> mClipboard;

    bool                 mSnapEnabled   { true  };
    NoteType             mNewNoteType   { NoteType::Standard };

    double      xToBeat              (int x)        const;
    int         beatToX              (double beat)  const;
    int         rowToY               (int row)      const;
    int         yToRow               (int y)        const;
    double      snapBeat             (double beat)  const;
    double      snapUnitBeats        ()             const;   // QA-Ee Stage 3: snap unit in beats
    NoteRef     noteAtPos            (int x, int y) const;
    NoteRef     noteNearRightEdge    (int x, int y) const;
    void        eraseAt              (int x, int y);
    void        updateCursor         ();

    int         rowToPageIndex (int rowIdx) const;
    int         pageIndexToRow (int pageIdx) const;
    PianoRollData* rollForRow  (int rowIdx) const;

    void finaliseMarquee ();
    bool isSelected      (NoteRef ref) const;
    void toggleSelection (NoteRef ref);

    void nudgeSelection    (int snapUnits, int rowDelta);
    void nudgeSelectionFine(int pixels,    int rowDelta);
    void sliceNotesOnLine  (juce::Point<int> start, juce::Point<int> end);

    void rebuildSelectionFromKeys (const std::vector<std::pair<int,double>>& keys);

    std::vector<NoteRef> getWorkingSet() const;

    void toolChop       (int divisions);
    void toolGlue       ();
    void toolStrum      ();
    void toolRandomize  ();
    void toolArticulate ();

    // ── Per-note context menu (double-click) ─────────────────────────────
    void showNoteContextMenu (NoteRef ref);
    void promptVelocity      (NoteRef ref);
    void promptMidiNote      (NoteRef ref);

    double totalBeats() const;
};

// ─────────────────────────────────────────────────────────────────────────────
// DrumKitControlLane - velocity / panning only
// ─────────────────────────────────────────────────────────────────────────────
class DrumKitControlLane : public juce::Component
{
public:
    enum Mode { Velocity, Panning };

    DrumKitControlLane();
    void paint    (juce::Graphics&)                                       override;
    void mouseDown(const juce::MouseEvent&)                               override;
    void mouseDrag(const juce::MouseEvent&)                               override;
    void mouseUp  (const juce::MouseEvent&)                               override;
    void mouseWheelMove (const juce::MouseEvent&,
                         const juce::MouseWheelDetails&)                  override;

    void setScrollState   (float ppb, double beatOff);
    void setPatternManager(PatternManager* pm);
    void setKitRowProvider(std::function<std::vector<DrumKitRowInfo>()> fn);
    void setMode          (Mode m);

    std::function<void()>        onChanged;
    std::function<void(Mode)>    onModeChange;
    // 2026-04-26 (D-7 sub-4): selection-aware rendering / editing.
    std::function<bool(DrumKitGrid::NoteRef)> isRefSelected;
    std::function<bool()>                     hasAnySelection;
    std::function<void(const juce::String&)>  onBeginEdit;
    std::function<void()>                     onCommitEdit;
    static constexpr int kHeight  = 240;
    static constexpr int kHeaderH = 16;

private:
    float  mPPB     { 80.f };
    double mBeatOff { 0.0 };
    Mode   mMode    { Velocity };

    PatternManager* mPM { nullptr };
    std::function<std::vector<DrumKitRowInfo>()> mRowProvider;

    DrumKitGrid::NoteRef noteNearX (int x, int y = -1)  const;
    float      getVal     (const PianoNote& n)  const;
    void       setVal     (PianoNote& n, float v);

    DrumKitGrid::NoteRef mDragRef { -1, -1 };
};

// ── Helper: TextButton with right-click callback ─────────────────────────
class DrumKitRightClickButton : public juce::TextButton
{
public:
    std::function<void(const juce::MouseEvent&)> onRightMouseDown;
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            if (onRightMouseDown) onRightMouseDown(e);
            return;
        }
        TextButton::mouseDown(e);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// DrumKitContainer - menu bar + toolbar + sidebar + grid + lane
// ─────────────────────────────────────────────────────────────────────────────
class DrumKitContainer : public juce::Component,
                         private juce::ScrollBar::Listener
{
public:
    DrumKitContainer();
    ~DrumKitContainer() override;   // QA-D Task 4: defensive MenuBarComponent teardown
    void resized()                                                          override;
    void paint   (juce::Graphics&)                                          override;
    bool keyPressed(const juce::KeyPress& key)                              override;
    void focusGained(juce::Component::FocusChangeType)                      override;

    void setPatternManager (PatternManager* pm);
    void setApvts          (juce::AudioProcessorValueTreeState* a);
    void setKitRowProvider (std::function<std::vector<DrumKitRowInfo>()> fn);
    void setPlayheadBeat   (double beat);
    void setContextLabel   (const juce::String& text);

    void setRowClickHandler  (std::function<void(int rowIdx, juce::Component*)> fn);
    void setAuditionHandlers (std::function<void(int rowIdx)> onOn,
                              std::function<void(int rowIdx)> onOff);
    void setReorderHandler   (std::function<void(int srcRow, int dstRow)> fn);
    void refreshKitView();

    void setUndoContext(const UndoContext& ctx);
    std::function<void()> onShowHistoryWindow;
    void undoRoll();
    void redoRoll();

    std::function<void(double beat)> onSeek;

    // 2026-04-25 (Batch 5): Kit button click - wired by StandaloneEditor to
    // open the Save Kit As / Load Kit popup.  Anchor passed for menu positioning.
    std::function<void(juce::Component* anchor)> onKitMenuRequested;

    // 2026-04-26: Global Lock/Unlock button click - wired up the chain to
    // StandaloneEditor where the confirm-prompt + cross-slot lock toggle live.
    std::function<void()> onGlobalLockRequested;

    void setActiveTool (DrumKitGrid::PRTool t);
    void applyZoom     (float factor);
    void applyZoomAnchored (float factor, int anchorX);   // QA-Ee: cursor-anchored wheel zoom
    void applyVZoom    (float factor);

    // QA-Ee Stage 3: GLOBAL snap accessors (provided by PianoRollPage).
    void setSnapAccessors (std::function<int()> getter, std::function<void(int)> setter);
    // QA-Ee Stage 2: content-bound dynamic zoom limits (scans all drum rows' notes).
    float contentMaxBars() const;
    float minZoomPPB (float vpW) const;
    float maxZoomPPB (float vpW) const;

    void scrollToPlayhead       ();
    void setLaneVisible         (bool v);
    void setSnapDenomAndQuantize(int denom);

    bool isLaneVisible()    const { return mLaneVisible;   }
    DrumKitGrid::PRTool getActiveTool() const { return mActiveTool; }

    bool   hasTimeSelection()    const { return mGrid && mGrid->hasTimeSelection(); }
    double getTimeSelBeatStart() const { return mGrid ? mGrid->getTimeSelBeatStart() : 0.0; }
    double getTimeSelBeatEnd()   const { return mGrid ? mGrid->getTimeSelBeatEnd()   : 0.0; }

    static constexpr int kMenuBarH    = 20;
    static constexpr int kToolbarH    = 28;
    static constexpr int kLaneH       = DrumKitControlLane::kHeight;
    static constexpr int kScrollBarSz = 12;

private:
    std::unique_ptr<DrumKitSidebar>      mSidebar;
    std::unique_ptr<DrumKitGrid>         mGrid;
    std::unique_ptr<DrumKitControlLane>  mLane;

    std::unique_ptr<juce::Label>         mContextLabel;

    std::unique_ptr<juce::ScrollBar>     mHScroll;
    bool mPushingToBars { false };
    void scrollBarMoved(juce::ScrollBar* sb, double newRangeStart) override;

    std::array<std::unique_ptr<juce::TextButton>, 7> mToolBtns;
    std::unique_ptr<juce::TextButton>                mUndoBtn, mRedoBtn, mHistoryBtn;
    std::unique_ptr<juce::TextButton>                mWrenchBtn;
    std::unique_ptr<DrumKitRightClickButton>         mMagnetBtn;
    std::unique_ptr<juce::TextButton>                mZoomInBtn, mZoomOutBtn;
    // Batch 5: Kit button - opens the Save Kit As / Load Kit popup.
    std::unique_ptr<juce::TextButton>                mKitBtn;

    PatternManager* mPM { nullptr };
    UndoContext     mUndoCtx;

    float  mPPB        { 80.f };
    float  mRowHScale  { 1.f };
    double mBeatOff    { 0.0 };
    int    mSnapDenom  { 16 };
    bool   mSnapEnabled { true };
    std::function<int()>     mOnGetSnapDiv;   // QA-Ee Stage 3: global snap read
    std::function<void(int)> mOnSetSnapDiv;   // QA-Ee Stage 3: global snap write
    int                      mLastSnapDiv { 1 };  // QA-Ee Stage 3: restore-to div for the on/off toggle
    DrumKitGrid::PRTool mActiveTool { DrumKitGrid::PRTool::Draw };

    float  mPreZoomPPB     { 80.f };
    double mPreZoomBeatOff { 0.0 };
    bool   mZoomedIn       { false };

    friend class DrumKitMenuBar;

    // QA-D Task 4 / QA-0a finding #8: model declared FIRST so it outlives
    // the MenuBarComponent during reverse-order destruction.
    std::unique_ptr<DrumKitMenuBar>         mMenuBarModel;
    std::unique_ptr<juce::MenuBarComponent> mMenuBar;

    bool mLaneVisible { true };

    double mPlayheadBeat { -1.0 };

    void updateUndoRedoBtns();
    void syncScrollState   ();
    void onHScrollDelta    (double dBeats);
    void pushScrollStateToBars();
};

// ─────────────────────────────────────────────────────────────────────────────
// DrumKitMenuBar - menu bar above the toolbar (Edit / Tools / View)
// ─────────────────────────────────────────────────────────────────────────────
class DrumKitMenuBar : public juce::MenuBarModel
{
public:
    explicit DrumKitMenuBar(DrumKitContainer& owner) : mOwner(owner) {}
    ~DrumKitMenuBar() override { setApplicationCommandManagerToWatch(nullptr); }

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu   getMenuForIndex (int index, const juce::String& name) override;
    void              menuItemSelected(int itemId, int topLevelIndex) override;

private:
    DrumKitContainer& mOwner;
};
