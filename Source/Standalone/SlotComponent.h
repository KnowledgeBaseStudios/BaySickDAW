#pragma once
#include <JuceHeader.h>
#include "../EffectRack.h"
#include "SharedUI.h"
#include "EffectEditorPanels.h"

// ── SlotComponent ─────────────────────────────────────────────────────────────
// One slot in the Effects Rack. Two visual states:
//
//   Empty:  dark panel with "+" centered (dashed border).
//           Click anywhere -> showAddMenu().
//           Menu appears at the mouse cursor position (Change D).
//
//   Loaded: 28px header strip painted directly:
//             ● (bypass, green=active / red=bypassed)  ·  name  ·  ▲  ·  ▼  ·  ×
//           + inline editor component (VKnob panel) fills the rest.
//           mouseDown() hit-tests the bypass LED; the Mode / SC / Preset /
//           Basic buttons are child components with their own onClick.
//
// Signal routing: slot 0 (top) is first in the DSP chain.
// ─────────────────────────────────────────────────────────────────────────────
class SlotComponent : public juce::Component
{
public:
    // QA-ModelShell TS5: the same slot is now shown two ways.
    //   Inline    - header strip + editor underneath.  The original, and what
    //               BaySickVocal's locked Vocal Chain still uses.
    //   PanelOnly - editor ONLY, filling the component.  The per-effect window
    //               in the new Effects shell: its bypass LED and its
    //               Basic / Mode / SC / Preset actions live on the WINDOW's
    //               title strip, so the header strip would be a second copy of
    //               chrome that is already one row higher.
    // Everything else -- the menus, the level feed, the undo + output-gain
    // wiring, the Basic-mode stamping -- is shared, which is the point of
    // making this a presentation rather than a second class.
    enum class Presentation { Inline, PanelOnly };
    void setPresentation (Presentation p);

    explicit SlotComponent(int slotIndex);
    ~SlotComponent() override;

    void setRack(EffectRack* rack);
    EffectRack* getRack() const noexcept { return mRack; }
    void refresh();  // sync bypass/name from rack without touching editor

    // QA-ModelShell TS5: which rack slot this component drives.  Fixed for the
    // life of an inline slot, but a panel window follows ONE EFFECT by uuid and
    // that effect changes index whenever the rack is reordered or a removal
    // packs the slots up -- so the window re-points its SlotComponent instead
    // of showing the effect that moved into the old index.
    int  getSlotIndex() const noexcept { return mSlotIndex; }
    void setSlotIndex (int idx);
    void setEditor(std::unique_ptr<juce::Component> editor);
    juce::Component* getEditor() const noexcept { return mEditor.get(); }

    // QA-ModelShell TS5: fired whenever a NEW panel has been mounted -- the
    // first build, a Mode switch, or a preset load that changed the Type.
    //
    // This exists because remountEditor() rebuilt the panel and nothing else:
    // the fresh panel came up WITHOUT its automation paramId stamps, and the
    // slot's registered applicators kept the variant captured at registration.
    // So switching a Compressor from Modern to FET left every lane applying
    // through the Modern table -- the exact collision class TS3's (type,
    // variant) key exists to prevent.  The host re-stamps and re-registers
    // here, which makes every mount path equivalent.
    std::function<void()> onEditorMounted;

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

    // H-7 (2026-05-01): Mode-dropdown callback fired when the user picks a
    // character mode for an effect that supports one (Compressor: Modern/
    // FET/Opto; Saturation: Tube/Console).  Host wires this to drive the
    // DSP directly (regular FX rack) or to write APVTS (BaySickVocal).
    // newType is the int value of the effect's Type enum.
    std::function<void(int slot, int newType)>     onModeChanged;

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

    // QA-ModelShell TS6: prefer this wherever the rack + slot are in hand -- a
    // hosted plugin slot is named by its PLUGIN, which the EffectType cannot
    // supply (one ordinal covers them all).
    static juce::String slotDisplayName (const EffectRack* rack, int slot);

    // QA-ModelShell TS5: the effect picker, callable without a SlotComponent.
    // The rack window's slot rows are not SlotComponents but must offer the
    // identical grouped list, and a second copy of that list would drift the
    // first time an effect is added.
    // QA-ModelShell TS6: plugin picks come back through a SEPARATE callback
    // because an EffectType cannot name a plugin -- one ordinal covers them all.
    // Defaulted so the vocal chain's caller is untouched (its locked slots never
    // offer plugins anyway).
    static void showEffectPickerMenu (juce::Point<int> screenPos,
                                      std::function<void(EffectType)> onPick,
                                      std::function<void(const juce::PluginDescription&)> onPickPlugin = {});

    // ── Chrome actions, driven from the panel window's title-bar menu ─────────
    // These were private because the only caller was this component's own
    // header.  In PanelOnly presentation the header is gone and the window's
    // menu drives them instead, so they are part of the surface now.
    void showModeMenu();
    void showPresetMenu();
    void showScMenu();
    void toggleBasicMode();        // QA-EffectsReview Task 1
    void showAddMenu();

    // What that menu should OFFER for the currently-loaded effect.  The header
    // decided this by showing / hiding buttons; a menu needs to ask.
    bool hasModeMenu()  const;     // character-mode umbrella (Compressor, ...)
    bool hasScMenu()    const;     // effect declares usesSidechain()
    bool hasBasicMode() const;     // panel reports hasAdvancedControls()
    bool isBasicMode()  const;
    juce::String modeLabel() const;
    juce::String scLabel()   const;

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

    void refreshScBtnLabel();
    void refreshModeBtnLabel();
    void refreshBasicBtnLabel();   // QA-EffectsReview Task 1

    Presentation mPresentation { Presentation::Inline };
    // Header chrome exists only in Inline presentation; PanelOnly hides every
    // button and skips the header strip entirely.
    void applyPresentationToChrome();

    // 2026-05-02: vblank-locked level feed.  Replaces the old 30 Hz Timer so
    // every effect panel's input VU + output dBFS update in lockstep with
    // the monitor refresh.  The vblank attachment is created lazily once the
    // component is on a peer (parentHierarchyChanged hook).
    void parentHierarchyChanged() override;
    void onVBlank();
    std::unique_ptr<juce::VBlankAttachment> mVBlank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlotComponent)
};
