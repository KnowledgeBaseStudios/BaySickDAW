#include "BaySickSolsticeModEditor.h"
#include "BaySickSolsticeModRegistry.h"
#include "BaySickSolsticeLAF.h"
#include <algorithm>
#include <cmath>

static constexpr float kHitRadius = 8.f;

static const juce::StringArray kSourceNames {
    "Envelope", "LFO", "Velocity", "Keyboard", "Mod X", "Mod Y", "Mod Z"
};

void BaySickSolsticeModEditor::makeRotary (juce::Slider& s)
{
    s.setSliderStyle  (juce::Slider::RotaryVerticalDrag);
    // S4 Batch 4: show a small value-readout below each knob for clarity.
    // Width=0 keeps the text-box hidden but TextBoxBelow triggers the live
    // drag popup via the BaySickLAF default handling; setPopupDisplayEnabled
    // gives the hover + drag value tooltip.
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setPopupDisplayEnabled (true, true, nullptr);
    s.setScrollWheelEnabled (true);
}

void BaySickSolsticeModEditor::makeBipolar (juce::Slider& s)
{
    makeRotary (s);
    s.setRange (-1.0, 1.0, 0.0);
    s.setDoubleClickReturnValue (true, 0.0);   // center-detent via double-click
    s.getProperties().set (BaySickSolsticeLAF::kBipolar, "true");
}

BaySickSolsticeModEditor::BaySickSolsticeModEditor()
{
    // ── Tab buttons ────────────────────────────────────────────────────────
    for (int i = 0; i < kNumTabs; ++i)
    {
        auto& btn = mTabBtns[i];
        btn.setButtonText (kTabNames[i]);
        btn.setClickingTogglesState (false);
        btn.onClick = [this, i]
        {
            if (auto* src = currentSource())
            {
                // IMG only valid for Envelope source.
                if (src == nullptr) return;
                if (i == 1 /* IMG */ && currentTarget() != nullptr)
                {
                    const auto& tgt = *currentTarget();
                    if (tgt.activeSource != ModSource::Envelope)
                        return;   // IMG is inert for non-Envelope sources
                }
                src->activeTab = i;
                for (int j = 0; j < kNumTabs; ++j)
                    mTabBtns[j].setToggleState (j == i, juce::dontSendNotification);
                syncControlsFromState();
                publishEdit();
                repaint();
            }
        };
        addAndMakeVisible (btn);
    }
    mTabBtns[0].setToggleState (true, juce::dontSendNotification);

    // ── Tool buttons (Batch 5a) ───────────────────────────────────────────
    mCurveModeBtn.setClickingTogglesState (true);
    mCurveModeBtn.getProperties().set ("switchToggle", true);
    mStepModeBtn .setClickingTogglesState (true);
    mStepModeBtn.getProperties().set ("switchToggle", true);
    mSnapBtn     .setClickingTogglesState (true);
    mSnapBtn.getProperties().set ("switchToggle", true);
    mCurveModeBtn.setToggleState (true, juce::dontSendNotification);   // CURVE default
    mCurveModeBtn.setTooltip ("CURVE mode: newly added points use smooth interpolation between them.");
    mStepModeBtn .setTooltip ("STEP mode: newly added points hold their value until the next point (stair-step).");
    mSnapBtn     .setTooltip ("SNAP: when on, new points and drags snap to the grid on both axes, at the "
                              "division chosen in the dropdown to the right. Hold Shift while dragging to "
                              "axis-lock (horizontal or vertical only).");
    mCurveModeBtn.onClick = [this]
    {
        mNewPointCurveType = 1;   // smooth
        mCurveModeBtn.setToggleState (true,  juce::dontSendNotification);
        mStepModeBtn .setToggleState (false, juce::dontSendNotification);
    };
    mStepModeBtn.onClick = [this]
    {
        mNewPointCurveType = 2;   // step
        mCurveModeBtn.setToggleState (false, juce::dontSendNotification);
        mStepModeBtn .setToggleState (true,  juce::dontSendNotification);
    };
    mSnapBtn.onClick = [this] { mSnapEnabled = mSnapBtn.getToggleState(); };
    for (auto* b : { &mCurveModeBtn, &mStepModeBtn, &mSnapBtn })
        addAndMakeVisible (*b);

    // ── Viewport tools (Batch 5c) ─────────────────────────────────────────
    mFreezeBtn.setClickingTogglesState (true);
    mFreezeBtn.getProperties().set ("switchToggle", true);
    mFreezeBtn.setTooltip ("FREEZE: lock the curve - prevents accidental clicks / drags while you audition the mod.");
    mZoomInBtn.setTooltip ("Zoom IN on the curve (up to 8x). Use the scroll bar below to pan through the full phase range once zoomed.");
    mZoomOutBtn.setTooltip ("Zoom OUT to see more of the curve (back to 1x = full view).");
    auto syncScrollBarToZoom = [this]
    {
        mHScroll.setRangeLimits (0.0, 1.0);
        mHScroll.setCurrentRange (mZoomOffset, 1.0 / mZoomFactor);
    };
    mFreezeBtn.onClick = [this] { mFrozen = mFreezeBtn.getToggleState(); };
    mZoomInBtn.onClick = [this, syncScrollBarToZoom]
    {
        mZoomFactor = juce::jlimit (1.0f, 8.0f, mZoomFactor * 2.0f);
        const float winW = 1.0f / mZoomFactor;
        mZoomOffset = juce::jlimit (0.0f, 1.0f - winW, mZoomOffset);
        syncScrollBarToZoom();
        repaint();
    };
    mZoomOutBtn.onClick = [this, syncScrollBarToZoom]
    {
        mZoomFactor = juce::jmax (1.0f, mZoomFactor * 0.5f);
        const float winW = 1.0f / mZoomFactor;
        mZoomOffset = juce::jlimit (0.0f, 1.0f - winW, mZoomOffset);
        syncScrollBarToZoom();
        repaint();
    };
    for (auto* b : { &mFreezeBtn, &mZoomInBtn, &mZoomOutBtn })
        addAndMakeVisible (*b);

    // Division dropdown - the app's unified snap set (Bar .. 1/6 Step),
    // triplets included.  Item id = unified index + 1, because a ComboBox id of
    // 0 means "nothing selected".  "Off" and "Line" are omitted: the SNAP
    // button already IS the on/off, and "Line" is a zoom-relative song-grid
    // idea that has no meaning on a fixed 0-1 phase axis.
    for (int i = 2; i < kNumUnifiedSnapDivs; ++i)
        mDivisionDD.addItem (kUnifiedSnapLabels[i], i + 1);
    mDivisionDD.setSelectedId (mSnapDivIdx + 1, juce::dontSendNotification);
    mDivisionDD.setTooltip ("Snap resolution, using the same divisions as the Piano Roll and Builder -- "
                            "1/3 Beat, 1/3 Step and 1/6 Step are the triplet targets. The grid draws the "
                            "matching ladder, so every snap position is a line you can see. "
                            "SNAP (to the left) must be on for this to apply.");
    mDivisionDD.onChange = [this]
    {
        const int id = mDivisionDD.getSelectedId();
        if (id >= 3 && id <= kNumUnifiedSnapDivs) { mSnapDivIdx = id - 1; repaint(); }
    };
    addAndMakeVisible (mDivisionDD);

    // Horizontal scroll bar (below the grid, set up in resized()).
    mHScroll.setRangeLimits (0.0, 1.0);
    mHScroll.setCurrentRange (0.0, 1.0);
    mHScroll.setAutoHide (false);
    mHScroll.addListener (this);
    addAndMakeVisible (mHScroll);

    // Undo/redo via keyboard.
    setWantsKeyboardFocus (true);

    // ── Dropdowns ──────────────────────────────────────────────────────────
    mArticulationsDD.setTextWhenNothingSelected ("No target");
    mArticulationsDD.onChange = [this]
    {
        const int idx = mArticulationsDD.getSelectedId() - 1;
        if (idx < 0) return;
        mCurrentTargetIdx = idx;
        syncControlsFromState();
        sanitizeCurrentCurveTimes();
        repaint();
    };
    addAndMakeVisible (mArticulationsDD);

    mModulationsDD.addItemList (kSourceNames, 1);
    mModulationsDD.setSelectedId (1, juce::dontSendNotification);
    mModulationsDD.onChange = [this]
    {
        if (auto* tgt = currentTarget())
        {
            const int newSrc = mModulationsDD.getSelectedId() - 1;
            if (newSrc < 0 || newSrc >= (int) ModSource::NumSources) return;
            tgt->activeSource = (ModSource) newSrc;
            clampActiveTabForCurrentSource();
            syncControlsFromState();
            publishEdit();
            repaint();
        }
    };
    addAndMakeVisible (mModulationsDD);

    // ── Depth knob (bipolar center-detent) ────────────────────────────────
    makeBipolar (mDepthKnob);
    mDepthKnob.setTooltip ("Depth: strength of modulation. Center (0) = no effect. Right (+) applies the curve as drawn. Left (-) inverts the curve (useful for going the opposite direction without redrawing). Double-click to reset.");
    mDepthKnob.onValueChange = [this]
    {
        if (auto* src = currentSource())
        {
            src->depth = (float) mDepthKnob.getValue();
            publishEdit();
            repaint();
        }
    };
    addAndMakeVisible (mDepthKnob);

    // ── Length slider: 13-step discrete selector ──────────────────────────
    // The slider stores an INDEX; the displayed name and the value written to
    // the registry are looked up in BaySickSolsticeModLength (which lives beside the
    // field it writes, because BLU-344's automation lane has to land on the
    // same 13 values this knob does).
    makeRotary (mLengthSlider);
    mLengthSlider.setRange (0.0, (double) (BaySickSolsticeModLength::kNumSteps - 1), 1.0);
    mLengthSlider.setValue (7.0, juce::dontSendNotification);   // index 7 = 1 beat (default)
    mLengthSlider.setTooltip ("Length: how long the envelope plays (0 -> 1). Discrete steps: 1/8, 1/4, 3/8, 1/2, 5/8, 3/4, 7/8, 1, 2, 4, 8, 16, 32 beats. TEMPO on = beats (4 = 1 bar at 4/4), TEMPO off = seconds. Notes shorter than this get the remainder released over the amp release time.");
    mLengthSlider.textFromValueFunction = [this] (double idx) -> juce::String
    {
        const int i = juce::jlimit (0, BaySickSolsticeModLength::kNumSteps - 1, (int) std::round (idx));
        const juce::String name (BaySickSolsticeModLength::kNames[i]);
        auto* src = currentSource();
        const bool beats = src && src->tempoSync;
        if (beats)
        {
            const bool isOne = (BaySickSolsticeModLength::kBeats[i] == 1.0f);
            return name + (isOne ? " beat" : " beats");
        }
        return name + " sec";
    };
    mLengthSlider.updateText();
    mLengthSlider.onValueChange = [this]
    {
        if (auto* src = currentSource())
        {
            const int i = juce::jlimit (0, BaySickSolsticeModLength::kNumSteps - 1,
                                        (int) std::round (mLengthSlider.getValue()));
            src->length = BaySickSolsticeModLength::kBeats[i];
            publishEdit();
        }
    };
    addAndMakeVisible (mLengthSlider);

    // ── SHAPE rotary (LFO-only: sine / triangle / saw / square) ───────────
    makeRotary (mShapeSelector);
    mShapeSelector.setRange (0.0, 3.0, 1.0);
    mShapeSelector.setValue (0.0, juce::dontSendNotification);
    mShapeSelector.setTooltip ("LFO Shape: Sine / Triangle / Saw / Square.");
    mShapeSelector.textFromValueFunction = [] (double v) -> juce::String
    {
        const int i = juce::jlimit (0, 3, (int) std::round (v));
        static const char* kNames[] = { "Sine", "Triangle", "Saw", "Square" };
        return kNames[i];
    };
    mShapeSelector.updateText();
    mShapeSelector.onValueChange = [this]
    {
        if (auto* src = currentSource())
        {
            src->lfoShape = juce::jlimit (0, 3, (int) std::round (mShapeSelector.getValue()));
            publishEdit();
            repaint();
        }
    };
    addAndMakeVisible (mShapeSelector);

    // ── TEMPO toggle ──────────────────────────────────────────────────────
    mTempoBtn.setClickingTogglesState (true);
    mTempoBtn.getProperties().set ("switchToggle", true);
    mTempoBtn.setTooltip ("TEMPO on: envelope length is in beats (syncs to project BPM). TEMPO off: envelope length is in seconds (absolute time).");
    mTempoBtn.onClick = [this]
    {
        if (auto* src = currentSource())
        {
            src->tempoSync = mTempoBtn.getToggleState();
            mLengthSlider.updateText();
            publishEdit();
        }
    };
    addAndMakeVisible (mTempoBtn);

    // S4 Batch 4: no SUSTAIN control here - the WYSIWYG curve plays over LENGTH
    // verbatim and the DSP has no sustain-hold stage. ModCurveState::sustainTime
    // survives for ValueTree round-tripping only (see BaySickSolsticeModRegistry.h);
    // T3-PerPointSustain owns explicit sustain markers.

    // ── Warp knobs (SPD/TNS/SKEW/PW) - per-tab per-source per-target ──────
    for (auto* s : { &mSpdKnob, &mTnsKnob, &mSkewKnob, &mPwKnob })
    {
        makeRotary (*s);
        s->setRange (0.0, 1.0, 0.0);
        s->setDoubleClickReturnValue (true, 0.5);
        s->setValue (0.5, juce::dontSendNotification);
        addAndMakeVisible (*s);
    }
    mSpdKnob .setTooltip ("Speed: warps the envelope advance rate. 0.5 = neutral, lower = slower, higher = faster.");
    mTnsKnob .setTooltip ("Tension: reshapes segments between points. 0.5 = neutral (linear/smooth), lower = ease-in, higher = ease-out.");
    mSkewKnob.setTooltip ("Skew: shifts the curve horizontally. 0.5 = neutral, lower = compressed to start, higher = stretched to end.");
    mPwKnob  .setTooltip ("Pulse Width: asymmetry. 0.5 = symmetric. Remaps the curve so the midpoint lands at PW.");
    // SPD warps phase advance rate (runtime), not the drawn shape - no repaint.
    // TNS / SKEW / PW warp the curve's shape at sample time - repaint the graph.
    mSpdKnob .onValueChange = [this] { if (auto* c = currentCurve()) { c->spd  = (float) mSpdKnob .getValue(); publishEdit(); } };
    mTnsKnob .onValueChange = [this] { if (auto* c = currentCurve()) { c->tns  = (float) mTnsKnob .getValue(); publishEdit(); repaint(); } };
    mSkewKnob.onValueChange = [this] { if (auto* c = currentCurve()) { c->skew = (float) mSkewKnob.getValue(); publishEdit(); repaint(); } };
    mPwKnob  .onValueChange = [this] { if (auto* c = currentCurve()) { c->pw   = (float) mPwKnob  .getValue(); publishEdit(); repaint(); } };

    // QA-ManualPress M-4c: manual callout anchors.  The toolbar and the bottom
    // knob row are runs of loose controls, so each anchors at the leftmost
    // member of its run - which is where the hand-placed dot sat.  The curve
    // canvas is painted, so it declares its rect in resized().
    mArticulationsDD.getProperties().set (kDotAnchor, "BSSOL-20");
    mModulationsDD  .getProperties().set (kDotAnchor, "BSSOL-21");
    mTabBtns[0]     .getProperties().set (kDotAnchor, "BSSOL-22");
    mHScroll        .getProperties().set (kDotAnchor, "BSSOL-24");
    mDepthKnob      .getProperties().set (kDotAnchor, "BSSOL-25");
}

BaySickSolsticeModEditor::~BaySickSolsticeModEditor() = default;

// ── Registry + focus ──────────────────────────────────────────────────────────
void BaySickSolsticeModEditor::setRegistry (BaySickSolsticeModRegistry* r)
{
    mRegistry = r;
    rebuildTargetDropdown();
    if (mRegistry && ! mRegistry->getAllTargets().empty())
    {
        mCurrentTargetIdx = 0;
        mArticulationsDD.setSelectedId (1, juce::dontSendNotification);
        syncControlsFromState();
        sanitizeCurrentCurveTimes();   // clean any leftover duplicate-time points
    }
    repaint();
}

void BaySickSolsticeModEditor::focusTarget (const juce::String& paramId)
{
    if (! mRegistry) return;
    const auto& all = mRegistry->getAllTargets();
    for (int i = 0; i < (int) all.size(); ++i)
    {
        if (all[(size_t) i]->paramId == paramId)
        {
            mCurrentTargetIdx = i;
            mArticulationsDD.setSelectedId (i + 1, juce::dontSendNotification);
            syncControlsFromState();
            sanitizeCurrentCurveTimes();   // clean any leftover duplicate-time points
            repaint();
            return;
        }
    }
}

void BaySickSolsticeModEditor::rebuildTargetDropdown()
{
    mArticulationsDD.clear (juce::dontSendNotification);
    if (! mRegistry) return;
    const auto& all = mRegistry->getAllTargets();
    for (int i = 0; i < (int) all.size(); ++i)
        mArticulationsDD.addItem (all[(size_t) i]->displayName, i + 1);
}

ModTarget* BaySickSolsticeModEditor::currentTarget() const noexcept
{
    if (! mRegistry) return nullptr;
    const auto& all = mRegistry->getAllTargets();
    if (mCurrentTargetIdx < 0 || mCurrentTargetIdx >= (int) all.size()) return nullptr;
    return all[(size_t) mCurrentTargetIdx].get();
}

ModSourceState* BaySickSolsticeModEditor::currentSource() const noexcept
{
    auto* tgt = currentTarget();
    if (! tgt) return nullptr;
    const int si = (int) tgt->activeSource;
    if (si < 0 || si >= (int) tgt->sources.size()) return nullptr;
    return &tgt->sources[(size_t) si];
}

ModCurveState* BaySickSolsticeModEditor::currentCurve() const noexcept
{
    auto* src = currentSource();
    if (! src) return nullptr;
    int ti = src->activeTab;
    // Force ENV for non-Envelope sources (IMG ignored there).
    if (auto* tgt = currentTarget())
        if (tgt->activeSource != ModSource::Envelope) ti = 0;
    ti = juce::jlimit (0, (int) src->tabs.size() - 1, ti);
    return &src->tabs[(size_t) ti];
}

void BaySickSolsticeModEditor::clampActiveTabForCurrentSource()
{
    auto* src = currentSource();
    if (! src) return;
    if (auto* tgt = currentTarget())
        if (tgt->activeSource != ModSource::Envelope)
            src->activeTab = 0;
}

void BaySickSolsticeModEditor::syncControlsFromState()
{
    auto* tgt  = currentTarget();
    auto* src  = currentSource();
    auto* curv = currentCurve();

    // Modulations dropdown reflects target's activeSource.
    if (tgt)
        mModulationsDD.setSelectedId ((int) tgt->activeSource + 1, juce::dontSendNotification);

    // Tab toggles.
    const bool isEnv = (tgt != nullptr && tgt->activeSource == ModSource::Envelope);
    const int tabIdx = src ? juce::jlimit (0, kNumTabs - 1, src->activeTab) : 0;
    for (int j = 0; j < kNumTabs; ++j)
        mTabBtns[j].setToggleState (j == tabIdx, juce::dontSendNotification);
    juce::ignoreUnused (isEnv);   // IMG tab hidden in v1; reactivates in T3-Img

    // Depth / length / TEMPO visible only when the source uses them.
    // Envelope + LFO have time behavior. Velocity/Keyboard/Mod X/Y/Z do not.
    // SUSTAIN is Envelope-only (LFO wraps forever, no gate).
    const bool hasTimeBehavior = tgt
        && (tgt->activeSource == ModSource::Envelope || tgt->activeSource == ModSource::LFO);
    const bool isLfoSource = tgt && tgt->activeSource == ModSource::LFO;
    mLengthSlider  .setVisible (hasTimeBehavior);
    mTempoBtn      .setVisible (hasTimeBehavior);
    mShapeSelector .setVisible (isLfoSource);

    // Push current state into widgets.
    if (src)
    {
        mDepthKnob.setValue (src->depth, juce::dontSendNotification);
        if (hasTimeBehavior)
            mLengthSlider.setValue ((double) BaySickSolsticeModLength::nearestIndex (src->length),
                                    juce::dontSendNotification);
        mTempoBtn     .setToggleState (src->tempoSync, juce::dontSendNotification);
        mShapeSelector.setValue ((double) juce::jlimit (0, 3, src->lfoShape), juce::dontSendNotification);
    }

    // BLU-344 (QA-ModelShell TS3): DEPTH and LENGTH are automatable, but they
    // are NOT apvts params -- they are fields on the mod registry, addressed by
    // (target, source).  Both are re-stamped here because this function is the
    // one place the visible (target, source) pair changes, so the id under the
    // cursor always names the pair the knob is actually editing.  The
    // applicators behind these ids are registered model-side against the
    // registry (StandaloneEditor::registerBaySickSolsticeModAutomation).
    if (tgt)
    {
        const juce::String base = tgt->paramId + "_mod"
                                + juce::String ((int) tgt->activeSource) + "_";
        mDepthKnob   .setComponentID (base + "depth");
        mLengthSlider.setComponentID (hasTimeBehavior ? base + "length" : juce::String());
    }

    if (curv)
    {
        mSpdKnob .setValue (curv->spd,  juce::dontSendNotification);
        mTnsKnob .setValue (curv->tns,  juce::dontSendNotification);
        mSkewKnob.setValue (curv->skew, juce::dontSendNotification);
        mPwKnob  .setValue (curv->pw,   juce::dontSendNotification);
    }
}

void BaySickSolsticeModEditor::publishEdit() noexcept
{
    if (mRegistry) mRegistry->publishSnapshot();
}

// Sort the current curve's points by time and nudge any duplicates apart so
// every point has a unique time coordinate. Duplicates make the sampler's
// bracketing logic pick one and treat the other as a floating dot.
// Sorting invalidates mDragIndex so callers must ensure no drag is active.
void BaySickSolsticeModEditor::sanitizeCurrentCurveTimes() noexcept
{
    auto* curv = currentCurve();
    if (! curv || curv->points.size() < 2) return;
    if (! mRegistry) return;

    const juce::SpinLock::ScopedLockType lock (mRegistry->getEditLock());
    std::sort (curv->points.begin(), curv->points.end(),
               [] (const BaySickSolsticeCurvePoint& a, const BaySickSolsticeCurvePoint& b)
               { return a.time < b.time; });

    constexpr float kMinGap = 0.002f;
    for (size_t i = 1; i < curv->points.size(); ++i)
    {
        const float minT = curv->points[i - 1].time + kMinGap;
        if (curv->points[i].time < minT)
            curv->points[i].time = juce::jmin (1.0f, minT);
    }
    // Second pass: a clamp to 1.0 may have re-introduced a tie at the tail.
    // Pull later points leftward if needed.
    for (int i = (int) curv->points.size() - 2; i >= 0; --i)
    {
        const float maxT = curv->points[(size_t) i + 1].time - kMinGap;
        if (curv->points[(size_t) i].time > maxT)
            curv->points[(size_t) i].time = juce::jmax (0.0f, maxT);
    }
}

// ── Undo / Redo (Batch 5b) ────────────────────────────────────────────────
// Snapshot the current curve's state before a mutating edit. Keeps the last
// 100 frames; anything older gets dropped from the front of the vector.
void BaySickSolsticeModEditor::pushUndo()
{
    auto* curv = currentCurve();
    auto* tgt  = currentTarget();
    auto* src  = currentSource();
    if (! curv || ! tgt || ! src) return;
    UndoFrame f;
    f.paramId   = tgt->paramId;
    f.sourceIdx = (int) tgt->activeSource;
    f.tabIdx    = src->activeTab;
    f.points    = curv->points;
    mUndoStack.push_back (std::move (f));
    if (mUndoStack.size() > 100)
        mUndoStack.erase (mUndoStack.begin());
    mRedoStack.clear();
}

void BaySickSolsticeModEditor::applyFrame (const UndoFrame& f)
{
    if (! mRegistry) return;
    auto* tgt = mRegistry->findTarget (f.paramId);
    if (! tgt) return;
    auto& src = tgt->sources[(size_t) juce::jlimit (0, 6, f.sourceIdx)];
    auto& tab = src.tabs[(size_t) juce::jlimit (0, (int) src.tabs.size() - 1, f.tabIdx)];
    {
        const juce::SpinLock::ScopedLockType lock (mRegistry->getEditLock());
        tab.points = f.points;
    }
    // Switch focus to the restored target + source so the user sees what
    // changed rather than having the edit applied invisibly.
    for (int i = 0; i < (int) mRegistry->getAllTargets().size(); ++i)
        if (mRegistry->getAllTargets()[(size_t) i].get() == tgt)
        {
            mCurrentTargetIdx = i;
            mArticulationsDD.setSelectedId (i + 1, juce::dontSendNotification);
        }
    tgt->activeSource = (ModSource) f.sourceIdx;
    src.activeTab     = f.tabIdx;
    syncControlsFromState();
    publishEdit();
    repaint();
}

bool BaySickSolsticeModEditor::keyPressed (const juce::KeyPress& k)
{
    // Ctrl+Z = undo, Ctrl+Y or Ctrl+Shift+Z = redo. On macOS Cmd is also caught.
    const bool ctrl = k.getModifiers().isCommandDown();
    if (! ctrl) return false;
    if (k.getKeyCode() == 'Z' && ! k.getModifiers().isShiftDown())
    {
        if (mUndoStack.empty()) return true;
        // Snapshot current state before undoing so redo can return to it.
        auto* curv = currentCurve();
        auto* tgt  = currentTarget();
        auto* src  = currentSource();
        if (curv && tgt && src)
        {
            UndoFrame cur;
            cur.paramId   = tgt->paramId;
            cur.sourceIdx = (int) tgt->activeSource;
            cur.tabIdx    = src->activeTab;
            cur.points    = curv->points;
            mRedoStack.push_back (std::move (cur));
        }
        auto f = mUndoStack.back();
        mUndoStack.pop_back();
        applyFrame (f);
        return true;
    }
    if (k.getKeyCode() == 'Y'
        || (k.getKeyCode() == 'Z' && k.getModifiers().isShiftDown()))
    {
        if (mRedoStack.empty()) return true;
        auto* curv = currentCurve();
        auto* tgt  = currentTarget();
        auto* src  = currentSource();
        if (curv && tgt && src)
        {
            UndoFrame cur;
            cur.paramId   = tgt->paramId;
            cur.sourceIdx = (int) tgt->activeSource;
            cur.tabIdx    = src->activeTab;
            cur.points    = curv->points;
            mUndoStack.push_back (std::move (cur));
        }
        auto f = mRedoStack.back();
        mRedoStack.pop_back();
        applyFrame (f);
        return true;
    }
    return false;
}

// ── Paint ─────────────────────────────────────────────────────────────────────
void BaySickSolsticeModEditor::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (BaySickSolsticeLAF::kChassis));
    g.fillAll();
    g.setColour (juce::Colour (BaySickSolsticeLAF::kBorder));
    g.drawRect (getLocalBounds(), 1);

    // Graph background + grid
    const auto gf = mGraphBounds.toFloat();
    g.setColour (juce::Colour (BaySickSolsticeLAF::kPanel));
    g.fillRect  (gf);
    g.setColour (juce::Colour (BaySickSolsticeLAF::kBorder));
    g.drawRect  (gf, 1.f);
    // Jeff, 2026-08-04: the grid FOLLOWS the selected division now.  It used to
    // be a fixed 32 segments whatever was chosen, so picking Beat drew 32 lines
    // and snapped to 4 -- and a triplet division had no lines of its own at all.
    // Same rule the Piano Roll and Builder use: the snap TYPE picks the ladder
    // (straight or triplet), and every rung that clears a pixel threshold is
    // drawn, so a finer selection reveals more lines and each snap target always
    // lands on a visible one.
    {
        int nRungs = 0;
        const int* ladder = gridLadderForSnap (mSnapDivIdx, nRungs);
        const float cycleW = gf.getWidth() * mZoomFactor;   // one phase cycle on screen

        // Coarse -> fine, so finer lines never overdraw the anchors.
        for (int r = 0; r < nRungs; ++r)
        {
            const int   ticks = ladder[r];
            const int   segs  = juce::jmax (1, 384 / juce::jmax (1, ticks));
            if (cycleW / float (segs) < float (kMinGridPx)) continue;   // too dense to read

            g.setColour (juce::Colour (r <= 1 ? 0xFF1E1E20 : 0xFF171718));
            for (int i = 1; i < segs; ++i)
            {
                const float phase = float (i) / float (segs);
                if (phase < mZoomOffset || phase > mZoomOffset + 1.0f / mZoomFactor) continue;
                const float x = gf.getX()
                              + (phase - mZoomOffset) * mZoomFactor * gf.getWidth();
                g.drawVerticalLine (juce::roundToInt (x), gf.getY(), gf.getBottom());
            }
        }

        // Horizontal quarters stay fixed -- the vertical axis is depth, not time.
        g.setColour (juce::Colour (0xFF1E1E20));
        for (int i = 1; i <= 3; ++i)
        {
            const float y = gf.getY() + gf.getHeight() * float (i) / 4.f;
            g.drawHorizontalLine (juce::roundToInt (y), gf.getX(), gf.getRight());
        }
    }
    g.setColour (juce::Colour (BaySickSolsticeLAF::kBorder));
    const float cy = gf.getY() + gf.getHeight() * 0.5f;
    g.drawHorizontalLine (juce::roundToInt (cy), gf.getX(), gf.getRight());

    // Zoom indicator: when zoomed, show a subtle label with the visible window.
    if (mZoomFactor > 1.01f)
    {
        g.setColour (juce::Colour (BaySickSolsticeLAF::kTextDim));
        g.setFont (juce::Font (9.f));
        const juce::String zoomText = juce::String (int (mZoomFactor)) + "x";
        g.drawText (zoomText, mGraphBounds.getRight() - 32, mGraphBounds.getY() + 2,
                    28, 12, juce::Justification::centredRight);
    }

    // Curve
    drawCurve (g);

    // Knob labels.  Jeff, 2026-08-04: size the box to the TEXT and centre it on
    // the knob.  These used to be the knob's width + 4, which was ample at 34px
    // knobs and truncates at 16 -- "DEPTH" and "LENGTH" both came out as "...".
    // Labels overhang into the gap between knobs, which is empty anyway.
    const juce::Font labelFont (8.f);
    g.setColour (juce::Colour (BaySickSolsticeLAF::kTextDim));
    g.setFont (labelFont);

    auto knobLabel = [&] (const juce::Component& c, const char* t)
    {
        const int w  = labelFont.getStringWidth (t) + 4;
        const int cx = c.getX() + c.getWidth() / 2;
        g.drawText (t, cx - w / 2, c.getBottom() + 2, w, 10, juce::Justification::centred);
    };

    const char* kLbls[] = { "SPD", "TNS", "SKEW", "PW" };
    const juce::Slider* kKnobs[] = { &mSpdKnob, &mTnsKnob, &mSkewKnob, &mPwKnob };
    for (int i = 0; i < 4; ++i)
        knobLabel (*kKnobs[i], kLbls[i]);

    // Depth + length labels (sustain is auto-derived, no label needed)
    knobLabel (mDepthKnob, "DEPTH");
    if (mLengthSlider.isVisible())  knobLabel (mLengthSlider,  "LENGTH");
    if (mShapeSelector.isVisible()) knobLabel (mShapeSelector, "SHAPE");
}

// ── Layout ────────────────────────────────────────────────────────────────────
void BaySickSolsticeModEditor::resized()
{
    const int kTopH     = 22;
    const int kTabH     = 22;
    const int kBotH     = 50;
    // Jeff, 2026-08-04: matches kKnobSm in BaySickSolsticeEditor -- these were the
    // last knobs in BaySickSolstice still at their old size, so the mod editor's row
    // read as oversized next to every other section.
    const int kKnobSz   = 16;
    const int kGap      = 4;
    auto bounds = getLocalBounds();

    // Top row: Articulations + Modulations dropdowns split evenly.
    auto top = bounds.removeFromTop (kTopH).reduced (2, 2);
    const int halfW = top.getWidth() / 2 - 2;
    mArticulationsDD.setBounds (top.removeFromLeft (halfW));
    top.removeFromLeft (4);
    mModulationsDD  .setBounds (top.removeFromLeft (halfW));

    // Tab row + tool buttons + viewport buttons -- ONE row, always.
    //
    // Jeff, 2026-08-04: at its natural widths this row wants 398px and the mod
    // editor is ~387px, so the tail overflowed and the division dropdown got
    // clipped -- which is what rendered its value as "...".  The buttons SHRINK
    // to fit.  Wrapping to a second row was the previous attempt and it is
    // wrong: it steals height from the envelope graph, which is the one thing
    // this box exists to make bigger.
    //
    // The dropdown is excluded from the shrink and floored at a width that
    // holds the longest label ("1/2 Beat") plus its arrow -- it is the control
    // that was unreadable, so it is the one that must not give ground.
    auto tabRow = bounds.removeFromTop (kTabH);
    {
        constexpr int kDivW = 72;
        const int gaps    = 8 + 2 + 2 + 8 + 2 + 2 + 4;
        const int btnNat  = kNumTabs * 48 + 52 + 48 + 48 + 56 + 28 + 28;
        const int btnRoom = juce::jmax (1, tabRow.getWidth() - gaps - kDivW);
        const double k    = juce::jmin (1.0, (double) btnRoom / (double) btnNat);
        auto sc = [k] (int v) { return juce::jmax (14, (int) std::floor (v * k)); };

        for (int i = 0; i < kNumTabs; ++i)
            mTabBtns[i].setBounds (tabRow.removeFromLeft (sc (48)));
        tabRow.removeFromLeft (8);
        // Edit tools: CURVE / STEP / SNAP.
        mCurveModeBtn.setBounds (tabRow.removeFromLeft (sc (52)));
        tabRow.removeFromLeft (2);
        mStepModeBtn .setBounds (tabRow.removeFromLeft (sc (48)));
        tabRow.removeFromLeft (2);
        mSnapBtn     .setBounds (tabRow.removeFromLeft (sc (48)));
        tabRow.removeFromLeft (8);
        // Viewport tools: FREEZE / + / - / Division dropdown.
        mFreezeBtn .setBounds (tabRow.removeFromLeft (sc (56)));
        tabRow.removeFromLeft (2);
        mZoomInBtn .setBounds (tabRow.removeFromLeft (sc (28)));
        tabRow.removeFromLeft (2);
        mZoomOutBtn.setBounds (tabRow.removeFromLeft (sc (28)));
        tabRow.removeFromLeft (4);
        // Whatever is left, but never less than the dropdown's floor.
        mDivisionDD.setBounds (tabRow.removeFromLeft (juce::jmax (kDivW, tabRow.getWidth())));
    }

    // Bottom strip.
    auto bot = bounds.removeFromBottom (kBotH);
    const int ky = bot.getY() + 4;
    int bx = bot.getX() + 4;

    // Jeff, 2026-08-04: TEMPO clipped the LENGTH label.  Labels are sized to
    // their TEXT now and centre on a 16px knob, so "LENGTH" overhangs its knob
    // by ~6px each side -- more than the 4px gap that followed it.  kLblClear
    // is the room a label needs beyond its knob; anything sitting next to a
    // labelled knob leaves that much.
    constexpr int kLblClear = 14;
    mDepthKnob    .setBounds (bx, ky, kKnobSz, kKnobSz); bx += kKnobSz + kLblClear;
    mLengthSlider .setBounds (bx, ky, kKnobSz, kKnobSz); bx += kKnobSz + kLblClear;
    mTempoBtn     .setBounds (bx, ky + 8, 52, 22);       bx += 52 + kGap;
    // Shape lives in the same column whether visible or not (LFO-only).
    mShapeSelector.setBounds (bx, ky, kKnobSz, kKnobSz); bx += kKnobSz + kLblClear;
    mSpdKnob      .setBounds (bx, ky, kKnobSz, kKnobSz); bx += kKnobSz + kLblClear;
    mTnsKnob     .setBounds (bx, ky, kKnobSz, kKnobSz); bx += kKnobSz + kLblClear;
    mSkewKnob    .setBounds (bx, ky, kKnobSz, kKnobSz); bx += kKnobSz + kLblClear;
    mPwKnob      .setBounds (bx, ky, kKnobSz, kKnobSz);

    // Horizontal scroll bar sits just above the bottom strip for panning
    // when zoomed in. 14 px tall; spans full editor width.
    const int kScrollH = 14;
    auto scrollArea = bounds.removeFromBottom (kScrollH);
    mHScroll.setBounds (scrollArea.reduced (2, 2));

    // Remaining area = graph.
    mGraphBounds = bounds.reduced (2, 0);

    // QA-ManualPress M-4c: the curve canvas is a painted rect, not a child, so
    // its callout dot anchors to the same rect drawCurve() draws into.
    getProperties().set (kDotAnchor,
                         juce::String ("BSSOL-23@")
                           + juce::String (mGraphBounds.getX())      + ","
                           + juce::String (mGraphBounds.getY())      + ","
                           + juce::String (mGraphBounds.getWidth())  + ","
                           + juce::String (mGraphBounds.getHeight()));
}

// ── Geometry helpers ──────────────────────────────────────────────────────────
juce::Point<float> BaySickSolsticeModEditor::pointToPixel (const BaySickSolsticeCurvePoint& p) const noexcept
{
    const auto gf = mGraphBounds.toFloat();
    // Zoom: only a window [mZoomOffset, mZoomOffset + 1/mZoomFactor] of the
    // phase range is visible. Points outside map off-screen (caller still
    // gets a valid float; they just won't be visible).
    const float t = (p.time - mZoomOffset) * mZoomFactor;
    return { gf.getX() + t * gf.getWidth(),
             gf.getBottom() - p.value * gf.getHeight() };
}

BaySickSolsticeCurvePoint BaySickSolsticeModEditor::pixelToPoint (float px, float py) const noexcept
{
    const auto gf = mGraphBounds.toFloat();
    BaySickSolsticeCurvePoint pt;
    const float tw = (px - gf.getX()) / gf.getWidth();
    pt.time  = juce::jlimit (0.f, 1.f, mZoomOffset + tw / mZoomFactor);
    pt.value = juce::jlimit (0.f, 1.f, (gf.getBottom() - py) / gf.getHeight());
    return pt;
}

int BaySickSolsticeModEditor::hitTest (juce::Point<int> pos) const noexcept
{
    auto* curv = currentCurve();
    if (! curv) return -1;
    for (int i = 0; i < (int) curv->points.size(); ++i)
    {
        const auto px = pointToPixel (curv->points[(size_t) i]);
        if (pos.toFloat().getDistanceFrom (px) < kHitRadius)
            return i;
    }
    return -1;
}

// ── Curve render ──────────────────────────────────────────────────────────────
void BaySickSolsticeModEditor::drawCurve (juce::Graphics& g) const
{
    auto* curv = currentCurve();
    auto* tgt  = currentTarget();
    auto* src  = currentSource();
    if (! curv) return;

    // LFO source with an unedited curve: render the selected waveform shape
    // (one cycle across the grid) so the graph matches what the DSP plays.
    // DSP has the same "<=2 points = use sampleLfoShape" fallback, so visual
    // and audio stay in sync.
    const bool useLfoWave = (tgt != nullptr && src != nullptr
                             && tgt->activeSource == ModSource::LFO
                             && curv->points.size() <= 2);

    // 2026-04-20 (S4 Batch 4 fix): DEPTH does NOT scale the visual curve -
    // at depth=0 (default) that would collapse everything to a flat line and
    // hide the shape users are editing. Graph shows the raw curve; DEPTH
    // knob position communicates effective modulation strength on its own.
    juce::ignoreUnused (src);

    if (useLfoWave)
    {
        const auto gf = mGraphBounds.toFloat();
        const int kSteps = 256;
        juce::Path wavePath;
        const float skew = curv->skew;
        const float pw   = curv->pw;
        // Zoom-aware: sample the visible phase window.
        const float winStart = mZoomOffset;
        const float winW     = 1.0f / mZoomFactor;
        for (int step = 0; step <= kSteps; ++step)
        {
            const float screenT = float (step) / float (kSteps);
            const float t_raw = winStart + screenT * winW;
            float       tw    = BaySickSolsticeModCurve::applySkew (t_raw, skew);
            tw                = BaySickSolsticeModCurve::applyPw   (tw, pw);
            const float v = BaySickSolsticeModCurve::sampleLfoShape (src->lfoShape, tw);
            const float px = gf.getX() + screenT * gf.getWidth();
            const float py = gf.getBottom() - v * gf.getHeight();
            if (step == 0) wavePath.startNewSubPath (px, py);
            else           wavePath.lineTo (px, py);
        }
        g.setColour (juce::Colour (BaySickSolsticeLAF::kAccent).withAlpha (0.15f));
        g.strokePath (wavePath, juce::PathStrokeType (6.f));
        g.setColour (juce::Colour (BaySickSolsticeLAF::kAccent));
        g.strokePath (wavePath, juce::PathStrokeType (1.5f));
        return;   // skip point-drawing; user sees the waveform, not draggable dots
    }

    if (curv->points.empty()) return;

    // Segment-aware drawing: walk the SORTED points and emit a path that
    // passes through each point EXACTLY, with the segment between each
    // consecutive pair sampled by the DSP's BaySickSolsticeModCurve::sample. This
    // guarantees every visible point is on the line regardless of how close
    // points are to each other - no more floating ghost dots from
    // sampling-resolution mismatches.
    juce::Path curvePath;
    bool pathStarted = false;
    const auto gf = mGraphBounds.toFloat();
    const float winStart = mZoomOffset;
    const float winW     = 1.0f / mZoomFactor;

    std::vector<BaySickSolsticeCurvePoint> sortedPts = curv->points;
    std::sort (sortedPts.begin(), sortedPts.end(),
               [] (const BaySickSolsticeCurvePoint& a, const BaySickSolsticeCurvePoint& b)
               { return a.time < b.time; });

    auto tToScreen = [&] (float t) -> float
    {
        return gf.getX() + ((t - winStart) / winW) * gf.getWidth();
    };
    auto valToScreen = [&] (float v) -> float
    {
        return gf.getBottom() - v * gf.getHeight();
    };
    auto addPathPoint = [&] (float px, float py)
    {
        if (! pathStarted) { curvePath.startNewSubPath (px, py); pathStarted = true; }
        else               curvePath.lineTo (px, py);
    };

    // Left edge: if the first point is past winStart, draw a horizontal line
    // at that point's value from winStart to its position (holds value).
    const float firstT = sortedPts.front().time;
    if (firstT > winStart + 1.0e-5f)
    {
        addPathPoint (tToScreen (winStart), valToScreen (sortedPts.front().value));
        addPathPoint (tToScreen (firstT),   valToScreen (sortedPts.front().value));
    }

    // Each segment between two consecutive points gets sampled finely to
    // capture SKEW/PW/TNS warp shapes.
    for (size_t i = 0; i < sortedPts.size(); ++i)
    {
        const auto& p = sortedPts[i];
        const float pT = p.time;
        // Always include the point itself as a path vertex.
        addPathPoint (tToScreen (pT), valToScreen (p.value));

        if (i + 1 >= sortedPts.size()) break;
        const auto& pNext = sortedPts[i + 1];

        // Sample intermediate steps between p and pNext via the full sample()
        // function - that applies SKEW/PW/TNS warps consistently.
        constexpr int kSegSteps = 32;
        for (int s = 1; s < kSegSteps; ++s)
        {
            const float frac = float (s) / float (kSegSteps);
            const float t = pT + frac * (pNext.time - pT);
            const float v = BaySickSolsticeModCurve::sample (*curv, t);
            addPathPoint (tToScreen (t), valToScreen (v));
        }
    }

    // Right edge: if the last point is before winEnd, hold its value to the edge.
    const float winEnd = winStart + winW;
    const float lastT  = sortedPts.back().time;
    if (lastT < winEnd - 1.0e-5f)
        addPathPoint (tToScreen (winEnd), valToScreen (sortedPts.back().value));

    // Glow + stroke
    g.setColour (juce::Colour (BaySickSolsticeLAF::kAccent).withAlpha (0.15f));
    g.strokePath (curvePath, juce::PathStrokeType (6.f));
    g.setColour (juce::Colour (BaySickSolsticeLAF::kAccent));
    g.strokePath (curvePath, juce::PathStrokeType (1.5f));

    // Control points - drawn from the same sorted list used for the line so
    // every dot is guaranteed to be on the curve path.
    for (const auto& pt : sortedPts)
    {
        const auto px = pointToPixel (pt);
        g.setColour (juce::Colour (BaySickSolsticeLAF::kAccent).withAlpha (0.3f));
        g.fillEllipse (px.x - 6.f, px.y - 6.f, 12.f, 12.f);
        g.setColour (juce::Colour (BaySickSolsticeLAF::kAccent));
        g.drawEllipse (px.x - 5.f, px.y - 5.f, 10.f, 10.f, 1.5f);
    }
}

// ── Scroll bar ────────────────────────────────────────────────────────────────
void BaySickSolsticeModEditor::scrollBarMoved (juce::ScrollBar* sb, double newRangeStart)
{
    if (sb != &mHScroll) return;
    mZoomOffset = juce::jlimit (0.0f, juce::jmax (0.0f, 1.0f - 1.0f / mZoomFactor),
                                (float) newRangeStart);
    repaint();
}

// ── Mouse handlers ────────────────────────────────────────────────────────────
// Snap point values to the current Division dropdown resolution when SNAP is on.
static float snapToGridN (float v, bool enabled, int divisions) noexcept
{
    if (! enabled) return v;
    const float cells = juce::jmax (1, divisions);
    return juce::jlimit (0.0f, 1.0f, std::round (v * cells) / cells);
}

void BaySickSolsticeModEditor::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();   // so Ctrl+Z reaches keyPressed
    auto* curv = currentCurve();
    if (! curv) return;
    if (mFrozen) return;   // FREEZE locks all edits
    if (! mGraphBounds.contains (e.getPosition())) return;

    mDragIndex = hitTest (e.getPosition());

    if (mDragIndex < 0 && e.mods.isLeftButtonDown())
    {
        pushUndo();
        if (mRegistry)
        {
            const juce::SpinLock::ScopedLockType lock (mRegistry->getEditLock());
            auto newPt = pixelToPoint (float (e.x), float (e.y));
            newPt.time      = snapToGridN (newPt.time,  mSnapEnabled, snapSegments());
            newPt.value     = snapToGridN (newPt.value, mSnapEnabled, snapSegments());
            newPt.curveType = mNewPointCurveType;

            // Prevent duplicate-time points. The sampler's bracketing logic
            // only picks one point when multiple share a time coordinate, so
            // extras would render as "disconnected dots" that don't drive
            // the line. Instead: replace the existing same-time point's
            // value + curveType with what the user clicked.
            constexpr float kTimeEps = 0.0005f;
            int replaceIdx = -1;
            for (int i = 0; i < (int) curv->points.size(); ++i)
                if (std::abs (curv->points[(size_t) i].time - newPt.time) < kTimeEps)
                    { replaceIdx = i; break; }

            if (replaceIdx >= 0)
            {
                curv->points[(size_t) replaceIdx].value     = newPt.value;
                curv->points[(size_t) replaceIdx].curveType = newPt.curveType;
                mDragIndex = replaceIdx;
            }
            else
            {
                curv->points.push_back (newPt);
                mDragIndex = int (curv->points.size()) - 1;
            }
        }
        publishEdit();
        repaint();
    }
    else if (mDragIndex >= 0 && e.mods.isRightButtonDown())
    {
        // Protect the two boundary anchors (leftmost + rightmost by time) so
        // the curve always has at least 2 points. User can delete any middle
        // point freely.
        bool isBoundary = false;
        if (! curv->points.empty())
        {
            float minT = 1e9f, maxT = -1e9f;
            int   minI = -1,   maxI = -1;
            for (int i = 0; i < (int) curv->points.size(); ++i)
            {
                const float t = curv->points[(size_t) i].time;
                if (t < minT) { minT = t; minI = i; }
                if (t > maxT) { maxT = t; maxI = i; }
            }
            if (mDragIndex == minI || mDragIndex == maxI) isBoundary = true;
        }
        if (! isBoundary)
        {
            pushUndo();
            if (mRegistry)
            {
                const juce::SpinLock::ScopedLockType lock (mRegistry->getEditLock());
                curv->points.erase (curv->points.begin() + mDragIndex);
            }
            publishEdit();
            repaint();
        }
        mDragIndex = -1;
    }
    else if (mDragIndex >= 0)
    {
        // Drag on existing point - push undo at start, not per-tick.
        pushUndo();
    }
    // Capture drag-start position for shift-axis-snap anchoring. Covers both
    // paths (new-point-added and existing-point-drag) since both set mDragIndex.
    if (mDragIndex >= 0 && mDragIndex < (int) curv->points.size())
        mDragStartPos = curv->points[(size_t) mDragIndex];
}

void BaySickSolsticeModEditor::mouseDrag (const juce::MouseEvent& e)
{
    auto* curv = currentCurve();
    if (! curv || mDragIndex < 0) return;
    if (mFrozen) return;
    if (mDragIndex >= (int) curv->points.size()) return;

    auto pt = pixelToPoint (float (e.x), float (e.y));

    // Shift-axis-snap: lock drag to the axis with the larger offset from the
    // drag-start anchor (captured in mouseDown). Using the point's current
    // position as anchor was a bug - it updated each tick, killing the lock.
    if (e.mods.isShiftDown())
    {
        const float dx = std::abs (pt.time  - mDragStartPos.time);
        const float dy = std::abs (pt.value - mDragStartPos.value);
        if (dx > dy) pt.value = mDragStartPos.value;   // horizontal-lock
        else         pt.time  = mDragStartPos.time;    // vertical-lock
    }

    pt.time      = snapToGridN (pt.time,  mSnapEnabled, snapSegments());
    pt.value     = snapToGridN (pt.value, mSnapEnabled, snapSegments());
    pt.curveType = curv->points[(size_t) mDragIndex].curveType;   // preserve
    curv->points[(size_t) mDragIndex] = pt;
    publishEdit();
    repaint();
}

void BaySickSolsticeModEditor::mouseUp (const juce::MouseEvent&)
{
    mDragIndex = -1;
    // Full sort + dedupe after any drag. Catches multi-point pile-ups that
    // the old single-collision nudge missed.
    sanitizeCurrentCurveTimes();
    publishEdit();
    repaint();
}

void BaySickSolsticeModEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (mFrozen) return;
    auto* curv = currentCurve();
    if (! curv) return;
    const int idx = hitTest (e.getPosition());
    if (idx < 0 || idx >= (int) curv->points.size()) return;
    pushUndo();
    auto& p = curv->points[(size_t) idx];
    p.curveType = (p.curveType + 1) % 3;   // 0 linear -> 1 smooth -> 2 step -> 0
    publishEdit();
    repaint();
}
