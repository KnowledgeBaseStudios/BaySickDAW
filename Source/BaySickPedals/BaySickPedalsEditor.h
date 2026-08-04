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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickPedalsEditor)
};
