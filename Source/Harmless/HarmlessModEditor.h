#pragma once
#include <JuceHeader.h>
#include <vector>
#include "../Standalone/SharedUI.h"   // VibeSlider
#include "../VibesynthConstants.h"    // unified snap labels / divisions / grid ladders

// ── HarmlessCurvePoint ────────────────────────────────────────────────────────
// Self-contained modulation curve point. No PatternManager dependency.
// curveType: 0=linear, 1=smooth (cosine), 2=sharp (step)
struct HarmlessCurvePoint
{
    float time      { 0.0f };  // 0-1 normalised horizontal position
    float value     { 0.5f };  // 0-1 normalised vertical position
    int   curveType { 1 };     // 0=linear, 1=smooth, 2=sharp
};

class HarmlessModRegistry;
struct ModTarget;
struct ModSourceState;
struct ModCurveState;

// ── HarmlessModEditor (S4 Batch 3 rewrite) ────────────────────────────────────
// Articulator-envelope editor. Operates on a HarmlessModRegistry:
//   - Articulations dropdown: picks the target (1 of 16).
//   - Modulations dropdown:   picks the source (Envelope / LFO / Vel / Key /
//                             ModX / ModY / ModZ).
//   - ENV / IMG tabs:         per-source active tab (IMG enabled only for
//                             the Envelope source; greyed otherwise).
//   - Depth knob:             bipolar (-100%..+100%), center-detent.
//   - Length slider + TEMPO:  envelope duration. TEMPO on = length in beats;
//                             TEMPO off = length in seconds.
//   - SPD / TNS / SKEW / PW:  per-tab curve warps.
//
// Every edit mutates the registry directly (under its edit-lock where a
// vector resize happens) and calls publishSnapshot() so voices observe the
// change on their next block.
//
// Tier 3 T3-PerPartModRouting (logged 2026-04-20): adds a GLOBAL toggle next
// to TEMPO when per-part envelope routing ships. Currently omitted.
class HarmlessModEditor : public juce::Component,
                          private juce::ScrollBar::Listener
{
public:
    HarmlessModEditor();
    ~HarmlessModEditor() override;

    // Call from HarmlessEditor ctor. Fills the Articulations dropdown from
    // the registry's registered targets and snaps focus to target 0.
    void setRegistry (HarmlessModRegistry* r);

    // Focus the editor on a specific target (by APVTS paramId). Called from
    // the right-click "Modulate envelope..." menu on any modulatable knob
    // (Batch 4 integration). No-op if the paramId isn't registered.
    void focusTarget (const juce::String& paramId);

    void paint   (juce::Graphics&) override;
    void resized () override;

    void mouseDown     (const juce::MouseEvent&) override;
    void mouseDrag     (const juce::MouseEvent&) override;
    void mouseUp       (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;   // curveType cycle
    bool keyPressed    (const juce::KeyPress&) override;        // undo / redo
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;

private:
    // ── Data access ───────────────────────────────────────────────────────────
    ModTarget*      currentTarget() const noexcept;
    ModSourceState* currentSource() const noexcept;
    ModCurveState*  currentCurve()  const noexcept;

    // Pull state from registry into the knobs/toggles. Called whenever the
    // focused target / source / tab changes.
    void syncControlsFromState();

    // Refill the Articulations dropdown after setRegistry.
    void rebuildTargetDropdown();

    // Ensure currentSource's activeTab is valid given the current source type
    // (IMG is only meaningful for Envelope; for others it resets to ENV).
    void clampActiveTabForCurrentSource();

    // Publish snapshot so voices observe the edit on next block.
    void publishEdit() noexcept;

    // Sort + dedupe duplicate-time points. Duplicates cause the sampler's
    // bracketing logic to pick one and render the other as a floating dot
    // ("ghost circle"). Called on mouseUp + focus changes when no drag is
    // active (sorting during drag would invalidate mDragIndex).
    void sanitizeCurrentCurveTimes() noexcept;

    // ── Tabs ──────────────────────────────────────────────────────────────────
    // 2026-04-20: IMG tab removed for v1 (image resynthesis is T3-Img). Only
    // ENV exists. Kept the kNumTabs constant + array so per-source state keeps
    // the shape it had, but IMG is never selectable.
    static constexpr int kNumTabs = 1;
    static constexpr const char* kTabNames[] = { "ENV" };
    juce::TextButton mTabBtns[kNumTabs];

    // ── Tool buttons (Batch 5a) ───────────────────────────────────────────────
    juce::TextButton mCurveModeBtn { "CURVE" };
    juce::TextButton mStepModeBtn  { "STEP" };
    juce::TextButton mSnapBtn      { "SNAP" };
    int              mNewPointCurveType { 1 };   // 1 = smooth, 2 = step
    bool             mSnapEnabled      { false };

    // ── Viewport tools (Batch 5c) ─────────────────────────────────────────────
    juce::TextButton mFreezeBtn     { "FREEZE" };
    juce::TextButton mZoomInBtn     { "+" };
    juce::TextButton mZoomOutBtn    { "-" };
    // Snap-resolution dropdown: picks how many divisions SNAP targets across
    // the grid. Grid itself always shows 32 divisions so users see every
    // possible snap position regardless of dropdown setting.
    juce::ComboBox   mDivisionDD;
    // Horizontal scroll bar below the grid. Thumb size = visible phase window
    // (= 1 / mZoomFactor), thumb position = mZoomOffset. When zoomed out to
    // 1x the thumb spans the full track (nothing to scroll).
    juce::ScrollBar  mHScroll { /*isVertical*/ false };
    bool             mFrozen      { false };
    float            mZoomFactor  { 1.0f };    // 1..8
    float            mZoomOffset  { 0.0f };    // 0..(1 - 1/zoom) phase window start
    // Jeff, 2026-08-04: the snap vocabulary is the APP'S unified one now
    // (kUnifiedSnapLabels / snapDivToTicks / gridLadderForSnap in
    // VibesynthConstants.h), triplets included -- this editor used to offer
    // straight 1 / 1/2 / 1/4 / 1/8 / 1/16 / 1/32 only, which was the one place
    // in the app where a triplet could not be snapped to.
    //
    // The TICK SYSTEM itself still does not reach in here, and deliberately:
    // this axis is per-note 0-1 PHASE, not song position (see CLAUDE.md on
    // HarmlessCurvePoint vs PatternManager::ControlPoint).  What is shared is
    // the DIVISION vocabulary -- a division's segment count across one phase
    // cycle is 384 / snapDivToTicks(idx), so the numbers come from the same
    // source of truth without importing the domain.
    int              mSnapDivIdx { 6 };        // unified index; 6 = Step (1/16)
    // Equal segments across ONE phase cycle for the current division.  384 is a
    // bar in ticks, so this is "how many of this division fit in a cycle" --
    // derived from snapDivToTicks so the numbers can never drift from the rest
    // of the app.  Bar -> 1, Beat -> 4, 1/3 Beat -> 12, Step -> 16, 1/6 Step -> 96.
    int snapSegments() const noexcept
    {
        const int t = snapDivToTicks (mSnapDivIdx);
        return t > 0 ? juce::jmax (1, 384 / t) : 16;
    }
    // Grid density follows the SELECTED division's ladder rung, no longer a
    // fixed 32 lines regardless of what is being snapped to.
    static constexpr int kMinGridPx = 6;       // finest rung must clear this

    // ── Undo / Redo (Batch 5b) - per-target curve history ─────────────────────
    // Simple stack of full point-vector snapshots. pushUndo() is called before
    // every mutating edit (add/remove/drag end); Ctrl+Z pops + restores.
    struct UndoFrame {
        juce::String paramId;          // which target
        int          sourceIdx { 0 };  // which source
        int          tabIdx    { 0 };
        std::vector<HarmlessCurvePoint> points;
    };
    std::vector<UndoFrame> mUndoStack;
    std::vector<UndoFrame> mRedoStack;
    void pushUndo();
    void applyFrame (const UndoFrame& f);

    // ── Dropdowns ─────────────────────────────────────────────────────────────
    juce::ComboBox mArticulationsDD;
    juce::ComboBox mModulationsDD;

    // ── Graph area ────────────────────────────────────────────────────────────
    juce::Rectangle<int> mGraphBounds;

    // ── Bottom strip ──────────────────────────────────────────────────────────
    VibeSlider       mDepthKnob;          // bipolar -1..+1, center-detent
    VibeSlider       mLengthSlider;       // 0.01..32 (beats if tempoSync else sec)
    juce::TextButton mTempoBtn { "TEMPO" };
    VibeSlider       mShapeSelector;      // LFO-only, 4 waveforms
    VibeSlider       mSpdKnob, mTnsKnob, mSkewKnob, mPwKnob;

    // ── State ─────────────────────────────────────────────────────────────────
    HarmlessModRegistry* mRegistry       { nullptr };
    int                  mCurrentTargetIdx { -1 };   // index into registry target list
    int                  mDragIndex      { -1 };    // curve point being dragged
    HarmlessCurvePoint   mDragStartPos   {};        // anchor for shift-axis-snap

    static void makeRotary  (juce::Slider& s);
    static void makeBipolar (juce::Slider& s);
    juce::Point<float>  pointToPixel (const HarmlessCurvePoint& p) const noexcept;
    HarmlessCurvePoint  pixelToPoint (float px, float py) const noexcept;
    int                 hitTest       (juce::Point<int> pos) const noexcept;
    void                drawCurve     (juce::Graphics&) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmlessModEditor)
};
