#pragma once
#include <JuceHeader.h>
#include "BaySickPedalsProcessor.h"
#include "../Standalone/BaySickTitleBar.h"   // QA-A (2026-05-09)

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPedalsEditor - Phase I-15 (2026-05-03)
// ─────────────────────────────────────────────────────────────────────────────
// 4x2 pedal-rack editor for the BaySickPedalsProcessor.
//
// Layout (left-to-right, top-to-bottom):
//   Row 1:  [Tuner | slot 1 | slot 2 | slot 3]
//   Row 2:  [slot 4 | slot 5 | slot 6 | EQ ]
//
// Slot 0 (Tuner) and slot 7 (EQ) are locked-position; slot 7 has a header
// dropdown to pick between Graphic / Bass Graphic / Pro Parametric EQ.
// Slots 1-6 are user-settable -- right-click for the Change Pedal submenu.
//
// Empty slots paint a dashed-border + plus icon (mirrors empty FX rack
// slots).  Click on an empty slot opens the Change Pedal menu.
//
// Each occupied slot tile contains:
//   * Title strip (effect name + per-pedal "..." preset menu button)
//   * The effect's PanelMode::Pedal panel (simplified knob set)
//   * Bypass LED footswitch (red when on / dim when bypassed)
// ─────────────────────────────────────────────────────────────────────────────

class PedalSlotComponent;

class BaySickPedalsEditor : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit BaySickPedalsEditor (BaySickPedalsProcessor& proc);
    ~BaySickPedalsEditor() override;

    // QA-ApvtsAutomation: the owning page supplies a per-instance channel prefix
    // ("inst{N}_pedals").  Every pedal panel's automation ids are built from it
    // plus the slot's stable uuid, so lanes stay distinct across Inst tabs and
    // survive pedal reordering.  Call once, after construction.
    void setAutomationPrefix (const juce::String& prefix);
    const juce::String& getAutomationPrefix() const noexcept { return mAutomationPrefix; }

    void paint   (juce::Graphics&) override;
    void resized ()                  override;

    // Called by per-tile components when the user picks a new effect type
    // (Change Pedal menu, EQ picker dropdown).
    void onSlotTypeChanged (int slot);

    // I-15 polish (2026-05-03): drag-to-reorder support for slots 1-6.
    // PedalSlotComponent calls these from its mouseDown / mouseDrag / mouseUp
    // when the user drags the tile's title bar.
    int  findSlotIndexAt (juce::Point<int> globalPos) const noexcept;
    void setDropTargetSlot (int slot);    // paint highlight on hovered drop slot
    int  getDropTargetSlot() const noexcept { return mDropTargetSlot; }
    void performMove (int fromSlot, int toSlot);

    // QA-A (2026-05-09): pedalboard preset menu hook.  InstPage (the parent
    // page hosting this editor) sets this callback to its existing
    // showPedalboardPresetMenu() so the migrated preset button in the title
    // bar's trailing area still routes to the right menu.  Phase 4.4 wires
    // the InstPage side; this Phase 4.2 commit just ships the hook.
    std::function<void()> onPedalboardPresetMenu;

    // Fired after a per-pedal preset load succeeds, so the hosting page can
    // drain the process-wide MissingFileReport (a pedal's setStateInformation
    // banks an unresolvable NAM capture / user IR there rather than skipping in
    // silence).  Draining does NOT require the app processor -- reportIfAny and
    // ScopedGesture are free functions in a header-only namespace -- so this
    // hook is a routing choice, not a reachability constraint.  If it is ever
    // left unwired the entries go unreported and surface under an unrelated
    // gesture later; a ScopedGesture on the load itself would not have that
    // failure mode.
    std::function<void()> onPresetLoaded;

    // QA-A Phase 6 (2026-05-09): exposes the title-bar preset button so the
    // parent page (InstPage) can anchor its preset PopupMenu to the button
    // itself instead of the whole editor (which JUCE pins at the editor's
    // top-left -- i.e. the app's top-left, not a button-relative location).
    juce::Component* getPedalboardPresetButton() noexcept { return &mPresetBtn; }

    // QA-Layout T3 (Window-3/4): the internal BaySickTitleBar is dissolved;
    // the hosting window's title strip shows this identity (T4 wires the
    // pedals window's strip to these + the preset button above).
    static juce::String getEngineTitle()  { return "BaySickPedals"; }
    static juce::Colour getEngineAccent() { return juce::Colour (0xFF1C3A8A); }

    // ── View modes (Jeff, 2026-08-05) ─────────────────────────────────────────
    // STANDARD is the 4x2 pedalboard.  COMPACT shows ONE pedal at a time, picked
    // from a dropdown of the eight slots, in a window the size of the Effects
    // window -- a chain of discrete units paginates naturally, which is why this
    // is not a "compact layout" in the T8 sense (those are Future State CL-306).
    //
    // The MECHANISM is meant to be reused: PageMenuBar::setViewMenu installs the
    // View heading, the host owns the window resize, and the editor only has to
    // answer what its modes are and lay itself out.  See CL-307.
    enum class ViewMode { Standard, Compact };
    // notifyHost=false is the RESTORE path: it lays the editor out without
    // firing onViewModeChanged, so restoring the mode cannot resize the window
    // and clobber the bounds the project just restored.  Only a user-initiated
    // switch resizes.
    void     setViewMode (ViewMode m, bool notifyHost = true);
    ViewMode getViewMode() const noexcept { return mViewMode; }
    // Fired when the mode changes so the HOST can resize its window -- the
    // editor does not know or care what window it is in.
    std::function<void(ViewMode)> onViewModeChanged;
    // Window sizes each mode wants, in WINDOW dims.  Compact is the Effects
    // window's WIDTH but taller (Jeff, 2026-08-05: 357x355) -- the Effects
    // window's 268 height does not leave a usable pedal below the picker.
    static juce::Point<int> windowSizeFor (ViewMode m)
    {
        return m == ViewMode::Compact ? juce::Point<int> { 331, 331 }
                                      : juce::Point<int> { 1038, 554 };
    }

private:
    void timerCallback() override;

    BaySickPedalsProcessor& mProc;
    juce::String            mAutomationPrefix;
    std::array<std::unique_ptr<PedalSlotComponent>, BaySickPedalsProcessor::kNumSlots> mTiles;
    EffectType mLastTypes[BaySickPedalsProcessor::kNumSlots] {};
    int        mDropTargetSlot { -1 };

    // Accent = navy / royal blue (#1C3A8A), shared with BaySickGuitars +
    // BaySickBasses (Inst tab active color) since all three engines live
    // inside Inst pages.  The preset button mounts on the hosting window's
    // title strip (QA-Layout T3/T4); the internal title bar is dissolved.
    BaySickPresetButton mPresetBtn { "Preset" };

    // Compact-view state: which slot the single visible tile is showing, and
    // the picker that chooses it.  Both inert in Standard.
    ViewMode        mViewMode { ViewMode::Standard };
    int             mCompactSlot { 0 };
    juce::ComboBox  mSlotPicker;
    void rebuildSlotPicker();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickPedalsEditor)
};
