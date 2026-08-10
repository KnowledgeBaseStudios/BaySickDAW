#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../PatternManager.h"
#include "SharedUI.h"
#include "UndoActions.h"

// ─────────────────────────────────────────────────────────────────────────────
// EEAutomationGrid
// Standalone automation curve editor used by EventEditor.
// Displays and edits an AutomationLane's ControlPoints with teal path + glow,
// hollow draggable nodes, snap to beat subdivision (Alt = override),
// Linear / Stepped / Spline curve types, and LFO waveform preview.
// ─────────────────────────────────────────────────────────────────────────────
class EEAutomationGrid : public juce::Component
{
public:
    enum class EETool { Draw, Paint, Erase, Interpolate, Select, Zoom };

    explicit EEAutomationGrid(juce::UndoManager& um);

    // QA-UndoCoverage Task 5 (12-iii): edits route through the app's undo
    // choke point (labels/dirty/owner semantics) instead of hitting the
    // manager directly.  Falls back to the direct path when unwired.
    void setUndoContext (const UndoContext& ctx) { mCtx = ctx; }

    // Data binding ─────────────────────────────────────────────────────────────
    // Call setBlock() to bind to a live AutomationLane inside PatternManager.
    // All edits snapshot the lane before/after and register AutomationLaneEditActions.
    void setBlock(PatternManager* pm, int blockIdx, float clipLengthBeats = 4.f);

    float getTotalBeats() const { return mTotalBeats; }

    // Tool ─────────────────────────────────────────────────────────────────────
    void setTool(EETool t);
    EETool getTool() const { return mTool; }

    // Snap: subdivisions per beat.
    //   1 = snap to beats, 2 = half-beats, 4 = 1/8-notes, 8 = 1/16-notes, 16 = 1/32-notes
    void setSnapSub(int s)       { mSnapSub = juce::jlimit(1, 32, s); }
    int  getSnapSub() const      { return mSnapSub; }

    // LFO mode: shows generated LFO waveform instead of point-based curve
    void setLFOMode(bool lfo);
    bool isLFOMode() const { return mLFOMode; }

    // Selection helpers
    void selectAll();
    void deleteSelected();

    // Callbacks
    std::function<void()> onChanged;  // called after any lane edit
    // QA-ProjectSave (2026-07-26): fired instead of deleting when the user
    // removes a lane's LAST point -- the host prompts to delete the whole
    // automation rather than leaving an empty block on the Builder grid.
    std::function<void()> onDeleteWholeAutomationRequested;
    // 5F-5: fires whenever the mouse moves over the grid. Beat is in beats,
    // val01 is 0..1 range. Caller uses for hover status bar.
    std::function<void(float beat, float val01)> onHoverChanged;

    // QA-Ed (Problem 1): map a normalised 0..1 point value <-> the param's real
    // units, so the editor reads out e.g. "140 BPM" / "-6.0 dB" and the new
    // right-click "Set Value..." accepts typed real-unit input.  Wired by
    // StandaloneEditor: APVTS lanes use the parameter's own getText()/
    // getValueForText() (identical to its knob + host automation); "global_tempo"
    // is the one non-APVTS lane (its 20..300 BPM mapping).
    std::function<juce::String(const juce::String& paramId, float val01)>      onFormatValue;
    std::function<float(const juce::String& paramId, const juce::String& text)> onParseValue;

    // Resolves a lane's paramId to the 0..1 value "Reset to Default" restores a
    // control point to (StandaloneEditor::automationResetValue).  Null = 0.5.
    std::function<float(const juce::String& paramId)> onResolveResetValue;

    void paint   (juce::Graphics&) override;
    void resized ()                override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp  (const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

private:
    juce::UndoManager& mUM;
    UndoContext        mCtx;
    PatternManager*    mPM        { nullptr };
    int                mBlockIdx  { -1 };
    float              mTotalBeats{ 4.f };
    // C.5b (post-revert): the Builder grid an automation block sits on is
    // uniform 4-beat-per-bar -- song-level time-signature markers are
    // decorative there, and only a PATTERN owns a real signature.  The block's
    // playback window is computed in that same uniform domain
    // (VibeSynthProcessor::processBlock, effectiveStartBars/effectiveLengthBars),
    // so deriving this from a marker would make the ruler, gridlines and snap
    // disagree with where the automation actually plays.
    static constexpr int kBeatsPerBar = 4;
    bool               mLFOMode   { false };
    EETool             mTool      { EETool::Draw };
    int                mSnapSub   { 4 };   // 4 subdivisions per beat = 1/16 note snap

    // Drag state
    int          mDragPt     { -1 };
    bool         mDragging   { false };
    AutomationLane mLaneBefore {};          // snapshot before drag for undo
    bool         mDragIsNew  { false };     // point was just added this drag

    // Hover
    int  mHoverPt           { -1 };
    int  mHoverCurveHandle  { -1 };  // original idx of left-side point, -1=none

    // Curve handle drag state
    int            mCurveHandleDrag       { -1 };   // original idx of left-side point
    AutomationLane mCurveHandleLaneBefore {};        // snapshot for undo

    // Selection (Select tool)
    std::vector<int>  mSelection;
    bool              mMarqueeActive  { false };
    juce::Point<int>  mMarqueeStart;
    juce::Point<int>  mMarqueeCurrent;

    // ── Layout constants ───────────────────────────────────────────────────────
    static constexpr int kRulerH   = 18;   // ruler at top
    static constexpr int kValLabelW = 34;  // value labels on left
    static constexpr float kNodeR  = 4.f;  // node hit radius

    // ── Coordinate helpers ─────────────────────────────────────────────────────
    float beatToX  (float beat) const;
    float xToBeat  (float px)   const;
    float valToY   (float v01)  const;
    float yToVal   (float py)   const;
    float snapBeat (float beat, bool noSnap) const;
    int   hitTestPt         (float px, float py) const;  // control point index or -1
    int   hitTestCurveHandle(float px, float py) const;  // original left-point index or -1

    // Value at the first control point (range-start) for erase resets
    float rangeStartVal() const;

    // Lane accessor (non-const - caller must hold data lock)
    AutomationLane* lanePtr() const;

    // ── Paint helpers ──────────────────────────────────────────────────────────
    void drawRuler       (juce::Graphics&) const;
    void drawGrid        (juce::Graphics&) const;
    void drawCurve       (juce::Graphics&) const;
    void buildCurvePath  (juce::Path&) const;
    void drawNodes       (juce::Graphics&) const;
    void drawCurveHandles(juce::Graphics&) const;
    void drawLFOWaveform (juce::Graphics&) const;
    void drawMarquee     (juce::Graphics&) const;
    void drawValueLabels (juce::Graphics&) const;

    void updateCursor();
    void commitEdit(const juce::String& label, const AutomationLane& before,
                    const AutomationLane& after);

    // QA-Ed (Problem 1): right-click "Set Value..." -> modal text entry that maps
    // typed real-unit input back to the point's 0..1 value via onParseValue.
    void promptSetPointValue(int hit);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EEAutomationGrid)
};

// ─────────────────────────────────────────────────────────────────────────────
// AutomationBrowserPane
// Lists all automation blocks from PatternManager by paramId.
// Clicking a row hot-swaps the EventEditor to that block.
// ─────────────────────────────────────────────────────────────────────────────
class AutomationBrowserPane : public juce::Component,
                              private juce::ListBoxModel
{
public:
    AutomationBrowserPane();

    void resized() override;
    void paint  (juce::Graphics&) override;

    // Repopulate from PatternManager; call whenever blocks may have changed.
    void refresh(PatternManager* pm);

    // Highlight the row for a given block index (without triggering callback).
    void selectBlock(int blockIdx);

    // Fired when user clicks a row: supplies the block index.
    std::function<void(int blockIdx)> onBlockSelected;

    // Optional display-name resolver. When set, refresh() uses it instead of
    // the raw paramId for each row label.
    std::function<juce::String(const AutomationLane&)> onResolveDisplayName;

    // Batch E #3 (2026-05-01): when set, refresh() asks the host whether the
    // lane's target paramId still resolves in APVTS.  Stale rows are marked
    // visually so users can see at a glance which automation is dead-ended.
    std::function<bool(const juce::String& paramId)> onIsParamStale;

private:
    struct Row { int blockIdx; juce::String label; bool stale { false }; };
    std::vector<Row> mRows;
    juce::ListBox    mList;

    int  getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics&, int w, int h, bool sel) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutomationBrowserPane)
};

// ─────────────────────────────────────────────────────────────────────────────
// EventEditorContent
// Main content component of an EventEditor window.
// Contains: menu bar, Auto/LFO tab buttons, LFO knobs, value display,
//           EEAutomationGrid (left), AutomationBrowserPane (right).
// ─────────────────────────────────────────────────────────────────────────────
class EventEditorContent : public juce::Component,
                           public juce::MenuBarModel,
                           public juce::KeyListener,
                           private juce::ChangeListener,
                           private juce::Timer
{
public:
    EventEditorContent(VibeSynthProcessor& p, juce::UndoManager& um);
    ~EventEditorContent() override;

    // QA-UndoCoverage Task 5 (12-iii): route lane edits + undo/redo through
    // the app choke point; forwards to the grid.
    void setUndoContext (const UndoContext& ctx);

    // Bind to a specific ArrangementBlock (must be ClipType::Automation)
    void setBlock(PatternManager* pm, int blockIdx);

    // Tool forwarding
    void setTool(EEAutomationGrid::EETool t);
    EEAutomationGrid::EETool getTool() const;

    // ── MenuBarModel ───────────────────────────────────────────────────────────
    juce::StringArray getMenuBarNames()                                      override;
    juce::PopupMenu   getMenuForIndex(int menuIndex, const juce::String&)    override;
    void              menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    void resized() override;
    void paint  (juce::Graphics&) override;

    // KeyListener - handles P/B/D/I/E/Z tool shortcuts + Ctrl+Z/Y/A/M
    bool keyPressed(const juce::KeyPress&, juce::Component*) override;

    // Callbacks set by StandaloneEditor after construction
    std::function<juce::StringArray()>                    onGetParamList;
    std::function<int(const juce::String& paramId)>       onCreateAutomation;
    // Resolves an AutomationLane (paramId + userDisplayName) to the label shown
    // in the title bar, browser pane, etc. When null, falls back to raw paramId.
    // EventEditorContent also forwards this to its AutomationBrowserPane.
    std::function<juce::String(const AutomationLane&)>    onResolveDisplayName;
    // QA-ProjectSave (2026-07-26): fired after ANY lane edit so the host can
    // redraw surfaces that render the same automation -- the Builder grid's
    // arrangement block in particular, which previously kept its old shape until
    // the user navigated away and back.
    std::function<void()>                                 onLaneEdited;

    // Accessor so StandaloneEditor can wire the display-name resolver through
    // to the browser pane after the content has been built.
    AutomationBrowserPane* getBrowserPane() { return mBrowserPane.get(); }

    // Accessor so StandaloneEditor can wire the value format/parse hooks (QA-Ed
    // Problem 1) onto the grid after the content has been built.
    EEAutomationGrid* getGrid() { return mGrid.get(); }

private:
    VibeSynthProcessor& mProcessor;
    juce::UndoManager&  mUM;
    UndoContext         mCtx;
    void performViaCtx (const juce::String& label, juce::UndoableAction* action);
    void doUndo();
    void doRedo();

    PatternManager* mPM       { nullptr };
    int             mBlockIdx { -1 };

    // ── Menu bar ───────────────────────────────────────────────────────────────
    std::unique_ptr<juce::MenuBarComponent> mMenuBar;

    // ── Mode tabs ──────────────────────────────────────────────────────────────
    std::unique_ptr<juce::TextButton> mAutoTab;
    std::unique_ptr<juce::TextButton> mLFOTab;
    bool mInLFOMode { false };

    // ── LFO control knobs (visible in LFO mode only) ───────────────────────────
    std::unique_ptr<VKnob> mMinKnob;
    std::unique_ptr<VKnob> mMaxKnob;
    std::unique_ptr<VKnob> mTimeKnob;   // LFO rate in beats

    // ── Value display label (bg: #7B241C deep red) ─────────────────────────────
    std::unique_ptr<juce::Label> mValueDisplay;

    // ── Snap control ───────────────────────────────────────────────────────────
    std::unique_ptr<juce::ComboBox> mSnapCombo;
    std::unique_ptr<juce::Label>    mSnapLabel;

    // ── Main panels ────────────────────────────────────────────────────────────
    std::unique_ptr<EEAutomationGrid>      mGrid;
    std::unique_ptr<AutomationBrowserPane> mBrowserPane;
    std::unique_ptr<juce::TextButton>      mNewAutoBtn;
    int                                    mLastBlockCount { 0 };

    // 5F-5: title label (top-left of content - shows current param id)
    std::unique_ptr<juce::Label>           mTitleLabel;
    // 5F-5: delete-automation button (next to "New Automation Clip")
    // 5F-5: tool button strip (22px row below the grid area)
    std::array<std::unique_ptr<juce::TextButton>, 6> mToolBtns;
    // 5F-5: footer status bar (20px at bottom - shows hovered beat + value)
    std::unique_ptr<juce::Label>           mStatusBar;

    void timerCallback() override;
    // UndoManager broadcast: undo/redo may have rewritten the bound lane
    // from this window or the main one - repaint the grid (QA-Fd fix).
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void switchMode(bool lfo);
    void updateTabStyles();
    void updateValueDisplay();
    void onKnobChanged();
    void doImportMidi();
    void doConvertToClip();
    void doSelectAll();
    void doNewAutomation();
    // 5F-5: clears the current lane's points (undoable via AutomationLaneEditAction)
    // 5F-5: syncs the tool button strip's visual toggle state to the active tool
    void updateToolButtonStates();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EventEditorContent)
};

// ─────────────────────────────────────────────────────────────────────────────
// EventEditor
// Floating DocumentWindow - one per open automation lane.
// Multiple instances can coexist simultaneously.
// ─────────────────────────────────────────────────────────────────────────────
class EventEditor : public juce::DocumentWindow
{
public:
    EventEditor(VibeSynthProcessor& p, juce::UndoManager& um,
                PatternManager* pm, int blockIdx,
                const juce::String& title = "Event Editor");
    ~EventEditor() override;

    // QA-UndoCoverage Task 5: forwarded to the content + grid.
    void setUndoContext (const UndoContext& ctx);

    void closeButtonPressed() override;

    EventEditorContent* getContent()
    {
        return dynamic_cast<EventEditorContent*>(getContentComponent());
    }
    int getBlockIdx() const { return mBlockIdx; }

    // Called from outside to set the active tool on the grid
    void setTool(EEAutomationGrid::EETool t);

    // Callback: fired when the window is closed (so owner can remove it from list)
    std::function<void(EventEditor*)> onClosed;

private:
    int mBlockIdx;
    // Content is owned by DocumentWindow via setContentOwned() - access via getContent()

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EventEditor)
};
