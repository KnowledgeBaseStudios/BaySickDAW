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

    // Data binding ─────────────────────────────────────────────────────────────
    // Call setBlock() to bind to a live AutomationLane inside PatternManager.
    // All edits snapshot the lane before/after and register AutomationLaneEditActions.
    void setBlock(PatternManager* pm, int blockIdx, float clipLengthBeats = 4.f);

    // Direct lane pointer (no undo support — for display only)
    void setLaneReadOnly(const AutomationLane* lane, float clipLengthBeats);

    float getTotalBeats() const { return mTotalBeats; }
    void setTotalBeats(float beats);
    void setBeatsPerBar(int bpb) { mBeatsPerBar = bpb; repaint(); }

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

    // Zoom/scroll
    void  setPixelsPerBeat(float ppb);
    float getPixelsPerBeat() const { return mPPBeat; }

    // Selection helpers
    void selectAll();
    void deselectAll() { mSelection.clear(); repaint(); }
    void deleteSelected();

    // Callbacks
    std::function<void()> onChanged;  // called after any lane edit
    // 5F-5: fires whenever the mouse moves over the grid. Beat is in beats,
    // val01 is 0..1 range. Caller uses for hover status bar.
    std::function<void(float beat, float val01)> onHoverChanged;

    void paint   (juce::Graphics&) override;
    void resized ()                override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp  (const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

private:
    juce::UndoManager& mUM;
    PatternManager*    mPM        { nullptr };
    int                mBlockIdx  { -1 };
    float              mTotalBeats{ 4.f };
    int                mBeatsPerBar{ 4 };
    float              mPPBeat    { 80.f };
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

    // Lane accessor (non-const — caller must hold data lock)
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

private:
    struct Row { int blockIdx; juce::String label; };
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
                           private juce::Timer
{
public:
    EventEditorContent(VibeSynthProcessor& p, juce::UndoManager& um);
    ~EventEditorContent() override;

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

    // KeyListener — handles P/B/D/I/E/Z tool shortcuts + Ctrl+Z/Y/A/M
    bool keyPressed(const juce::KeyPress&, juce::Component*) override;

    // Callbacks set by StandaloneEditor after construction
    std::function<juce::StringArray()>                    onGetParamList;
    std::function<int(const juce::String& paramId)>       onCreateAutomation;
    // Resolves an AutomationLane (paramId + userDisplayName) to the label shown
    // in the title bar, browser pane, etc. When null, falls back to raw paramId.
    // EventEditorContent also forwards this to its AutomationBrowserPane.
    std::function<juce::String(const AutomationLane&)>    onResolveDisplayName;

    // Accessor so StandaloneEditor can wire the display-name resolver through
    // to the browser pane after the content has been built.
    AutomationBrowserPane* getBrowserPane() { return mBrowserPane.get(); }

private:
    VibeSynthProcessor& mProcessor;
    juce::UndoManager&  mUM;

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

    // 5F-5: title label (top-left of content — shows current param id)
    std::unique_ptr<juce::Label>           mTitleLabel;
    // 5F-5: delete-automation button (next to "New Automation Clip")
    std::unique_ptr<juce::TextButton>      mDeleteBtn;
    // 5F-5: tool button strip (22px row below the grid area)
    std::array<std::unique_ptr<juce::TextButton>, 6> mToolBtns;
    // 5F-5: footer status bar (20px at bottom — shows hovered beat + value)
    std::unique_ptr<juce::Label>           mStatusBar;

    void timerCallback() override;
    void switchMode(bool lfo);
    void updateTabStyles();
    void updateValueDisplay();
    void onKnobChanged();
    void doImportMidi();
    void doConvertToClip();
    void doSelectAll();
    void doNewAutomation();
    // 5F-5: clears the current lane's points (undoable via AutomationLaneEditAction)
    void doDeleteAutomationPoints();
    // 5F-5: syncs the tool button strip's visual toggle state to the active tool
    void updateToolButtonStates();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EventEditorContent)
};

// ─────────────────────────────────────────────────────────────────────────────
// EventEditor
// Floating DocumentWindow — one per open automation lane.
// Multiple instances can coexist simultaneously.
// ─────────────────────────────────────────────────────────────────────────────
class EventEditor : public juce::DocumentWindow
{
public:
    EventEditor(VibeSynthProcessor& p, juce::UndoManager& um,
                PatternManager* pm, int blockIdx,
                const juce::String& title = "Event Editor");
    ~EventEditor() override;

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
    // Content is owned by DocumentWindow via setContentOwned() — access via getContent()

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EventEditor)
};
