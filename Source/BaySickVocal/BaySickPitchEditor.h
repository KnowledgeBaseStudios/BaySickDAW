#pragma once
#include <JuceHeader.h>
#include "../DSP/BaySickPitchDSP.h"
#include "../Standalone/UndoActions.h"   // QA-Fd 9a: UndoContext + PitchEditAction
#include <set>

class BaySickVocalProcessor;
class BaySickPitchSubEditor;
class BaySickPitchSubEditorWindow;

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPitchEditor - QA-Fd full rework (2026-07-11, snug-orbiting-catmull)
// ─────────────────────────────────────────────────────────────────────────────
// Composite-driven note-level pitch editor (the BaySickAlign sibling), hosted
// in BaySickVocalEditor's "BaySickPitch" sub-tab.  The channel composite
// auto-resolves from the grid clips; analysis segments note regions which
// paint as PILLS (Effects-purple fill, Vox-teal waveform interior; gray slice
// pills in the bottom lane) over the Bass-green as-sung pitch curve.
//
// QA-Fd surface (locked dockets 1-20 + Newtone parity 1-14):
//   - Motion model: 2-axis body drag (vertical pitch 0.1 st / Ctrl 0.01;
//     Snap ON lands on in-scale lanes; horizontal = elastic move with gap
//     counter-warp, pills never cross), edge drag = stretch/squeeze,
//     Ctrl+horizontal = detach.  Time edits publish the channel time map
//     (pitch tab is UPSTREAM of align).
//   - Root/Scale/Snap toolbar controls (shared 13-scale table); Focus pulls
//     toward the snap target and pills RENDER at the corrected pitch while
//     the green curve stays as-sung.
//   - Selection: marquee, Shift+click add, Ctrl+A/C/V/B, Shift+arrows,
//     Delete = Restore to Original State.  Copy/paste = edit-state transfer.
//   - Global undo (9a): every gesture lands as a PitchEditAction through the
//     UndoContext; toolbar Undo/Redo + Ctrl+Z drive the app-wide stack.
//   - Sub-edit system: display-only box (VIB/FRM/VOL/PITCH bars + Edit/Reset
//     buttons) under the selected pill; double-click / Edit opens the popup
//     sub-editor (BaySickPitchSubEditor); slim on-pill corner handles write
//     ramp points into the SAME per-pill curves (single-storage contract).
//   - View: Ctrl wheel h-zoom, Alt wheel v-zoom (variable lane height),
//     Shift wheel h-scroll, bare wheel v-scroll, middle-drag pan,
//     Ctrl+RMB-drag zoom-to-selection with RMB toggle-back; view floor at
//     C0; 30 Hz playhead following the main transport incl. stop-reset.
// ─────────────────────────────────────────────────────────────────────────────

class BaySickPitchEditor : public juce::Component,
                           private juce::Timer,
                           private juce::ScrollBar::Listener
{
public:
    explicit BaySickPitchEditor (BaySickVocalProcessor& p);
    ~BaySickPitchEditor() override;

    void paint   (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed (const juce::KeyPress& k) override;

    // QA-Fd 9a: global undo plumb (VoxPage -> BaySickVocalEditor -> here).
    void setUndoContext (const UndoContext& ctx) { mUndoCtx = ctx; }

    // Shared view state (seconds domain, composite-relative).
    double pixelsPerSecond() const noexcept { return mPps; }
    double scrollSeconds()   const noexcept { return mScroll; }
    int    topNote()         const noexcept { return mTopNote; }
    double noteLaneH()       const noexcept { return mLaneH; }
    void   setView (double pps, double scrollSec);
    void   setTopNote (int note);
    void   setLaneHeight (double laneH, double anchorMidi, int anchorY);

private:
    class Toolbar;
    class PitchKeyboard;
    class PitchCanvas;
    class InfoBar;
    friend class BaySickPitchSubEditor;
    friend class BaySickPitchSubEditorWindow;   // closeButtonPressed -> closeSubEditor

    void timerCallback() override;

    void updateInfoBarFor (int regionIdx);
    void runAnalyzeIfNeeded (bool force);
    void runRender();
    void runReset();
    void refreshComposite();          // waveform cache for the pills
    void showSendNotesMenu();
    // QA-Fe: WORLD-is-offline notice (once, until "do not show again").  Fired
    // from the toolbar engine dropdown when the user picks WORLD.
    void showWorldOfflineNotice();
    // QA-Fa recovery: version-history restore points (bundle item 3).
    void runSnapshotVersion();
    void showVersionsMenu();

    // User presets (Focus/Mod/Speed; the factory preset combo retired, 7a)
    void saveUserPreset();
    void loadUserPreset();

    // ── QA-Fd selection model (Task 6) ───────────────────────────────────────
    std::set<int> mSelection;
    int  mFocusRegion { -1 };            // last-clicked member (copy source)
    bool isSelected (int idx) const { return mSelection.count (idx) > 0; }
    void selectOnly (int idx);
    void toggleSelect (int idx);
    void clearSelection();
    std::vector<int> selectionOrFocus() const;   // selection, or focus as a 1-vector

    // ── QA-Fd global undo (9a) ───────────────────────────────────────────────
    // beginEdit captures the before-state; commitEdit publishes + lands the
    // PitchEditAction (edit already applied -- first-perform skip).
    UndoContext mUndoCtx;
    std::vector<PitchNoteRegion> mPendingBefore;
    bool mEditPending { false };
    void beginEdit();
    void cancelEdit();
    void commitEdit (const juce::String& label);
    void applyRegionsFromUndo (const std::vector<PitchNoteRegion>& regions);

    // ── QA-Fd gestures / actions ─────────────────────────────────────────────
    void showPillMenu (int idx);
    void restoreToOriginal (const std::vector<int>& idxs);   // 15b full reset
    void snapToSemitone (const std::vector<int>& idxs);      // 18a menu item
    void toggleSliceExcluded (int idx);   // include/exclude a piece from correction
    // #3: force out-of-scale notes onto the nearest in-key lane center; in-key
    // notes keep their natural cents (Root/Scale-driven; no-op on Chromatic).
    void forceSelectionToScale (const std::vector<int>& idxs);
    void mergeSelection();                                    // merge/join
    void batchRepitchTo (int midiNote);                       // keyboard click
    void copySelection();                                     // 20a
    void pasteToSelection();
    void nudgeSelection (float semis);
    void nudgeSelectionTime (double dSec);
    void deleteSelection();                                   // = restore
    void resetBoxedParams (const std::vector<int>& idxs);     // display-box Reset
    void openSubEditor (int idx);
    void closeSubEditor();

    // Preview play (shared scrub-audition + popup Play path)
    void startRegionPreview (int regionIdx, bool loop);
    void stopPreview();

    // ── QA-Fd 4a analysis UX state ───────────────────────────────────────────
    enum class AnalysisState { Idle, Analyzing, Deferred, Failed };
    AnalysisState mAnalysisState { AnalysisState::Idle };
    juce::String  mAnalysisError;
    bool          mAnalyzeQueued { false };   // one deferred analysis in flight
    void setAnalysisState (AnalysisState s, const juce::String& err);

    float paramValue (const char* id, float fallback = 0.0f) const;
    void  setParamValue (const char* id, float v);

    BaySickVocalProcessor& mProc;

    std::unique_ptr<Toolbar>       mToolbar;
    std::unique_ptr<PitchKeyboard> mKeyboard;
    std::unique_ptr<PitchCanvas>   mCanvas;
    std::unique_ptr<InfoBar>       mInfoBar;
    // Sub-editor popup (Task 7), rehosted 2026-07-11 in the Event Editor's
    // floating-DocumentWindow chrome (owner uniformity call).
    std::unique_ptr<BaySickPitchSubEditorWindow> mSubWin;

    // Composite waveform cache (mono, analysis SR) for pill interiors +
    // preview rendering.
    juce::AudioBuffer<float> mCompCache;
    double                   mCompSr { 44100.0 };
    bool                     mCompValid { false };

    // View state
    double mPps     { 100.0 };   // pixels per second
    double mScroll  { 0.0 };     // seconds at the canvas left edge
    int    mTopNote { 84 };      // top visible MIDI note
    double mLaneH   { 10.0 };    // pixels per semitone lane (Alt-wheel zooms)
    // Zoom-to-selection return slot (family Ctrl+RMB-drag / RMB-back idiom).
    bool   mHasSavedView { false };
    double mSavedPps { 100.0 }, mSavedScroll { 0.0 };
    int    mSavedTopNote { 84 };
    double mSavedLaneH { 10.0 };
    void   zoomToSelection();
    void   restoreSavedView();

    bool mAutoScroll { true };

    // Scroll bars (piano-roll parity).  mHScroll drives mScroll (seconds), mVScroll
    // drives mTopNote (MIDI note, inverted).  mPushingToBars guards the funnel
    // (bars written with dontSendNotification) from re-entering scrollBarMoved.
    static constexpr int kScrollBarSz = 12;
    std::unique_ptr<juce::ScrollBar> mHScroll, mVScroll;
    bool mPushingToBars { false };
    void scrollBarMoved (juce::ScrollBar* sb, double newStart) override;
    void pushScrollStateToBars();

    // Playhead: follows the MAIN transport (incl. stop-reset) at 30 Hz.
    double mPlayheadSec { -1.0 };   // composite seconds; < 0 = hidden
    int    mSlowTick    { 0 };      // 30 Hz timer -> ~2 Hz slow path
    float  mRenderSig   { -1.0f };  // Focus/Snap/Root/Scale/mode repaint watch

    // Multi-select reset prompt's never-show-again checkbox (owned so the
    // async AlertWindow can host it without lifetime games).
    std::unique_ptr<juce::ToggleButton> mResetPromptCheck;
    // QA-Fe: WORLD-offline notice's do-not-show-again checkbox (async-owned).
    std::unique_ptr<juce::ToggleButton> mWorldPromptCheck;

    // Clipboard: edit-state transfer (locked 20a) -- audio never relocates.
    struct EditClipboard
    {
        bool  valid { false };
        float shiftSemis { 0 }, formantSemis { 0 };
        float vibDepthMult { 1 }, vibRateMult { 1 }, variation { 1 };
        std::vector<juce::Point<float>> volShape, pitchShape;
    };
    EditClipboard mClipboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickPitchEditor)
};
