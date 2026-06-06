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
class SlotComponent : public juce::Component
{
public:
    explicit SlotComponent(int slotIndex);
    ~SlotComponent() override;

    void setRack(EffectRack* rack);
    void refresh();  // sync bypass/name from rack without touching editor
    void setEditor(std::unique_ptr<juce::Component> editor);

    // H-8 (2026-05-02): re-mount the inline editor for the current slot's
    // DSP.  Called from a panel when the DSP's Type-driven layout changes
    // outside the Mode dropdown (e.g. clicking a preset button that flips
    // Type internally) so the user sees the right panel for the new state.
    void remountEditor();

    // H-6c (2026-05-01): when locked, the slot's effect can't be swapped,
    // moved, or removed.  Bypass + sidechain dropdown still work normally.
    // Used by BaySickVocal's Vocal Chain rack where slots are pinned to
    // specific effect types (De-esser / Compressor / Saturation / Limiter).
    void setLocked (bool b) { mLocked = b; repaint(); }
    bool isLocked() const noexcept { return mLocked; }

    // Forward undo context to the inline editor panel (if it is an EditorPanelBase).
    void setEditorUndoContext(const UndoContext& ctx);

    // Callbacks wired by EffectsPage
    std::function<void(int slot, EffectType type)> onEffectChosen;
    std::function<void(int slot)>                  onEffectRemoved;
    std::function<void(int slot, bool up)>         onMoveRequested;

    // H-7 (2026-05-01): Mode-dropdown callback fired when the user picks a
    // character mode for an effect that supports one (Compressor: Modern/
    // FET/Opto; Saturation: Tube/Console).  Host wires this to drive the
    // DSP directly (regular FX rack) or to write APVTS (BaySickVocal).
    // newType is the int value of the effect's Type enum.
    std::function<void(int slot, int newType)>     onModeChanged;

    // QA-EffectsReview Task 1: fired when the user flips Basic/Advanced on a
    // rack slot.  EffectsPage wires this to EffectRack::setSlotBasicMode so the
    // choice persists with the project.  basic == true means Basic mode.
    std::function<void(int slot, bool basic)>      onBasicModeChanged;

    // C.4 Phase 1 (2026-04-30): SC source dropdown context.  EffectsPage wires
    // this when a slot's editor is rebuilt.  channelMixerPrefix is the strip's
    // mixer APVTS prefix (e.g. "mixer_layer_0") -- the dropdown scans
    // <prefix>_sc_recv{0..3}_from to enumerate active SC lines.
    // resolveSourceName(int channelId) returns a display name for a source
    // strip's channel id (e.g. "Layer 1", "Bass 1", "Master").
    void setChannelContext (juce::AudioProcessorValueTreeState* apvts,
                             const juce::String& channelMixerPrefix,
                             std::function<juce::String(int)> resolveSourceName);

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
    bool         mLocked   { false };   // H-6c: BaySickVocal locks chain slots
    juce::String mEffectName;

    // Hit-test regions for the header strip (only valid when mLoaded)
    juce::Rectangle<int> mBypassRect, mUpRect, mDownRect, mCloseRect;

    // Screen position of last mouse-down (used to position the add-menu popup)
    juce::Point<int> mLastMousePosScreen;

    // Inline editor (owned; replaced on each effect change)
    std::unique_ptr<juce::Component> mEditor;

    // C.4 Phase 1 (2026-04-30): SC dropdown in the header chrome.  Only
    // visible when the loaded effect's usesSidechain() returns true.  Items
    // populated each click from APVTS scan + resolveSourceName callback.
    std::unique_ptr<juce::TextButton>                  mScBtn;
    juce::AudioProcessorValueTreeState*                mApvts { nullptr };
    juce::String                                       mChannelMixerPrefix;
    std::function<juce::String(int)>                   mResolveSourceName;

    // H-7 (2026-05-01): Mode dropdown in the header chrome.  Visible only
    // for effects with character-mode umbrellas (Compressor: Modern/FET/
    // Opto; Saturation: Tube/Console).  Sits beside mScBtn for Compressor;
    // takes the SC slot's position for Saturation since Saturation has no
    // sidechain.
    std::unique_ptr<juce::TextButton>                  mModeBtn;

    // H-9 prep (2026-05-02): preset menu button.  Sits at the LEFT of the
    // slot header next to the bypass LED.  Visible whenever a non-empty
    // effect is loaded.  Pop-up menu offers Save / Load / Restore / etc.
    std::unique_ptr<juce::TextButton>                  mPresetBtn;

    // QA-EffectsReview Task 1: Basic/Advanced disclosure toggle.  Sits in the
    // header immediately LEFT of mPresetBtn.  Visible ONLY when the loaded panel
    // reports hasAdvancedControls() == true (a reference-only panel -- e.g. a
    // Wah loaded into the rack -- gets no button).  Label reads "Basic" or
    // "Advanced"; click flips the panel's mBasicMode + the slot's persisted
    // basicMode, then re-lays-out the inline editor in place.
    std::unique_ptr<juce::TextButton>                  mBasicBtn;

    void showAddMenu();
    void showScMenu();
    void showModeMenu();
    void showPresetMenu();
    void refreshScBtnLabel();
    void refreshModeBtnLabel();
    void toggleBasicMode();        // QA-EffectsReview Task 1
    void refreshBasicBtnLabel();   // QA-EffectsReview Task 1

    // 2026-05-02: vblank-locked level feed.  Replaces the old 30 Hz Timer so
    // every effect panel's input VU + output dBFS update in lockstep with
    // the monitor refresh.  The vblank attachment is created lazily once the
    // component is on a peer (parentHierarchyChanged hook).
    void parentHierarchyChanged() override;
    void onVBlank();
    std::unique_ptr<juce::VBlankAttachment> mVBlank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlotComponent)
};
