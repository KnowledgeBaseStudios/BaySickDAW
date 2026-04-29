#pragma once
#include <JuceHeader.h>
#include "../EffectRack.h"
#include "SharedUI.h"
#include "EffectEditorPanels.h"

// ── SlotComponent ─────────────────────────────────────────────────────────────
// One slot in the Effects Rack. Two visual states:
//
//   Empty:  dark panel with "+" centered (dashed border).
//           Click anywhere → showAddMenu() → onEffectChosen fires.
//           Menu appears at the mouse cursor position (Change D).
//
//   Loaded: 28px header strip painted directly:
//             ● (bypass, green=active / red=bypassed)  ·  name  ·  ▲  ·  ▼  ·  ×
//           + inline editor component (VKnob panel) fills the rest.
//           All header actions are hit-tested in mouseDown() (Change B).
//
// Signal routing: slot 0 (top) is first in the DSP chain.
// ─────────────────────────────────────────────────────────────────────────────
class SlotComponent : public juce::Component, private juce::Timer
{
public:
    explicit SlotComponent(int slotIndex);
    ~SlotComponent() override;

    void setRack(EffectRack* rack);
    void refresh();  // sync bypass/name from rack without touching editor
    void setEditor(std::unique_ptr<juce::Component> editor);

    // Forward undo context to the inline editor panel (if it is an EditorPanelBase).
    void setEditorUndoContext(const UndoContext& ctx);

    // Callbacks wired by EffectsPage
    std::function<void(int slot, EffectType type)> onEffectChosen;
    std::function<void(int slot)>                  onEffectRemoved;
    std::function<void(int slot, bool up)>         onMoveRequested;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

    // Pretty name for an EffectType enum value (e.g. "Reverb", "Transient Shaper").
    // Public so the automation display-name resolver in StandaloneEditor can use
    // the same labels as the slot UI -- no drift between the two.
    static juce::String effectTypeName(EffectType type);

private:
    int          mSlotIndex;
    EffectRack*  mRack     { nullptr };
    bool         mLoaded   { false };
    bool         mBypassed { false };
    juce::String mEffectName;

    // Hit-test regions for the header strip (only valid when mLoaded)
    juce::Rectangle<int> mBypassRect, mUpRect, mDownRect, mCloseRect;

    // Screen position of last mouse-down (used to position the add-menu popup)
    juce::Point<int> mLastMousePosScreen;

    // Inline editor (owned; replaced on each effect change)
    std::unique_ptr<juce::Component> mEditor;


    void showAddMenu();

    void timerCallback() override;   // feeds levels to meters

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlotComponent)
};
