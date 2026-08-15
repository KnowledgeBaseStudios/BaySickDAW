#pragma once
#include <JuceHeader.h>
#include "MicPlacementView.h"
#include "BaySickNAMIRProcessor.h"
#include "../Standalone/SharedUI.h"
#include "../Standalone/BaySickTitleBar.h"   // QA-A (2026-05-09)

// ─────────────────────────────────────────────────────────────────────────────
// BaySickNAMIREditor - Phase G-1.4
// ─────────────────────────────────────────────────────────────────────────────
// 760×340 panel, two-tone amp/cab faceplate using existing widget vocabulary
// (VKnob / DualLabelToggle / ChickenHeadSelector).  No skeuomorphic filmstrips
// - those were spec'd in NAM & IR Loader.txt §3.2 but Jeff's house style is
// BaySickLAF.  Picker buttons accept right-click for a recent-files popup; the
// whole component is also a FileDragAndDropTarget so .nam → loadNamModel and
// .wav → loadImpulseResponse just work.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickNAMIREditor : public juce::AudioProcessorEditor,
                           public  juce::FileDragAndDropTarget,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit BaySickNAMIREditor (BaySickNAMIRProcessor&);

    // Jeff, 2026-08-04: this editor is ALWAYS hosted in a window that owns a
    // title strip (the Inst NAM/IR window and the Vox NAM/IR satellite), so the
    // logo renders there and the internal bar carries none -- one name, not a
    // text copy on the strip with the logo directly beneath it.
    static juce::String getEngineTitle()  { return "BaySickNAM/IR"; }
    static juce::Colour getEngineAccent() { return juce::Colour (0xFFE0303F); }

    // QA-ApvtsAutomation: per-instance automation keys.  This engine's param ids
    // are bare literals ("output", "nam_bypass", "oversampling") and are identical
    // across all 20 Inst / 6 Vox pages, each of which owns its own processor and
    // apvts -- so a shared registry key would let the last-built tab win every
    // lane.  The owning page hands down "inst{N}_" / "vox{N}_"; the registry key
    // and each control's Automate-menu id both use the prefixed form while the
    // applicator writes THIS page's own parameter.  Call once, after construction.
    void setAutomationPrefix (const juce::String& prefix);
    ~BaySickNAMIREditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped           (const juce::StringArray& files, int x, int y) override;

private:
    // APVTS listener - keeps the A/B radio + per-slot labels in sync when the
    // ab_slot param changes from anywhere (other UI, automation, project load).
    void parameterChanged (const juce::String& paramID, float newValue) override;

    // ── Helpers ──────────────────────────────────────────────────────────────
    void browseForNamFile ();
    void browseForIrFile  ();
    void showRecentNamMenu();
    void showRecentIrMenu ();
    void updateLabels     ();
    void showError        (const juce::String& msg);
    void setActiveSlotFromUI (int slot);

    // Recents - 10-deep per kind ("nam" / "ir"), persisted in
    // Documents/BaySickDAW/settings.xml under <RecentNAMFiles> / <RecentIRFiles>.
    static juce::File settingsFile();
    juce::StringArray loadRecents (const juce::String& tag);
    void              saveRecents (const juce::String& tag, const juce::StringArray& list);
    void              pushRecent  (const juce::String& tag, const juce::String& path);

    BaySickNAMIRProcessor& processor;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // ── Header ───────────────────────────────────────────────────────────────
    // QA-A (2026-05-09): unified title bar replaces the old juce::Label +
    // custom header paint.  Accent = Mesa red (#E0303F) per Jeff's pick.
    // Nameless since 2026-08-04 -- the bar stays (it hosts the A/B pair and its
    // own chrome), the LOGO moved to the hosting window's strip.
    BaySickTitleBar  mTitleBar { {}, getEngineAccent() };
    juce::TextButton mSlotABtn { "A" };
    juce::TextButton mSlotBBtn { "B" };

    // ── Section labels (left side of file rows) ──────────────────────────────
    juce::Label mAmpSectionLbl;
    juce::Label mCabSectionLbl;

    // ── File rows ────────────────────────────────────────────────────────────
    // Custom subclass swallows the right-button mouseDown to fire the recent-
    // files popup while leaving left-click → onClick semantics untouched.
    struct FilePickerButton : public juce::TextButton
    {
        std::function<void()> onRightClick;
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isRightButtonDown() && onRightClick) { onRightClick(); return; }
            juce::TextButton::mouseDown (e);
        }
    };
    FilePickerButton mNamBrowseBtn;
    FilePickerButton mIrBrowseBtn;
    juce::Label      mNamFileLabel;
    juce::Label      mIrFileLabel;

    // ── Bypass toggles ───────────────────────────────────────────────────────
    DualLabelToggle mNamBypassToggle;
    DualLabelToggle mCabBypassToggle;
    std::unique_ptr<ButtonAtt> mNamBypassAtt, mCabBypassAtt;

    // ── Knobs ────────────────────────────────────────────────────────────────
    VKnob mInGainKnob;
    VKnob mGateThreshKnob;
    VKnob mGateReleaseKnob;
    VKnob mLowCutKnob;
    VKnob mHighCutKnob;
    VKnob mCabMixKnob;
    VKnob mOutputKnob;
    std::unique_ptr<SliderAtt> mInGainAtt, mGateThreshAtt, mGateReleaseAtt,
                               mLowCutAtt, mHighCutAtt, mCabMixAtt, mOutputAtt;

    // ── Selectors (manual APVTS sync - chicken head doesn't have an APVTS
    //    attachment helper) ───────────────────────────────────────────────
    ChickenHeadSelector mOSSelector;
    juce::Label         mOSSelectorLbl;

    // ── Status row ───────────────────────────────────────────────────────────
    juce::Label mFullRigHint;
    juce::Label mErrorLabel;

    // ── H-6d Mic Sim + Mic Placement, Mic A column (QA-Fc: rows split into
    //    Mic A | Mic B columns; Mic B mirrors this set below) ─────────────────
    juce::Label   mMicSimSectionLbl;
    juce::Label   mMicPlacementSectionLbl;

    DualLabelToggle     mMicAActiveToggle;
    std::unique_ptr<ButtonAtt> mMicAActiveAtt;
    juce::ComboBox      mMicSimMode;          // Built-in / User IR
    juce::Label         mMicSimModeLbl;
    juce::ComboBox      mMicSimModelCombo;    // 10 built-in archetypes
    juce::Label         mMicSimModelLbl;
    FilePickerButton    mMicSimUserIrBtn;     // Browse / clear user IR
    juce::Label         mMicSimUserIrLabel;   // Currently-loaded user IR path
    VKnob               mMicSimMixKnob;
    std::unique_ptr<SliderAtt> mMicSimMixAtt;

    ChickenHeadSelector mMicPlacementPolar;   // Omni / Cardioid / Super / Hyper / Fig-8
    juce::Label         mMicPlacementPolarLbl;
    VKnob               mMicPlacementDistanceKnob;
    VKnob               mMicPlacementAngleKnob;
    VKnob               mMicPlacementHeightKnob;
    VKnob               mMicPlacementMixKnob;
    std::unique_ptr<SliderAtt> mMicPlacementDistanceAtt, mMicPlacementAngleAtt,
                               mMicPlacementHeightAtt,
                                 mMicPlacementMixAtt;

    void browseForMicUserIr();
    void updateMicSimModeUI();   // updates the model combo + browse btn visibility
    void updateMicSimModelTooltip();   // refreshes "typical use" tooltip on combo

    // ── QA-Fc Mic B column (parallel mic, summed; bound to `_b_` params) ─────
    juce::Label   mMicSimBSectionLbl;
    juce::Label   mMicPlacementBSectionLbl;
    DualLabelToggle mMicBActiveToggle;
    std::unique_ptr<ButtonAtt> mMicBActiveAtt;

    juce::ComboBox      mMicSimModeB;
    juce::Label         mMicSimModeBLbl;
    juce::ComboBox      mMicSimModelComboB;
    juce::Label         mMicSimModelBLbl;
    FilePickerButton    mMicSimUserIrBtnB;
    juce::Label         mMicSimUserIrLabelB;
    VKnob               mMicSimMixKnobB;
    std::unique_ptr<SliderAtt> mMicSimMixBAtt;

    ChickenHeadSelector mMicPlacementPolarB;
    juce::Label         mMicPlacementPolarBLbl;
    VKnob               mMicPlacementDistanceKnobB;
    VKnob               mMicPlacementAngleKnobB;
    VKnob               mMicPlacementHeightKnobB;

    // The draggable mic pictures (Jeff, 2026-08-11).  One per virtual mic, each
    // bound to that mic's own distance / angle / polar params -- so the knobs
    // above them and the picture are the SAME placement, like the Harmless XYZ
    // pad.  Top / Side is a rendering choice, not a different model: see the
    // header note on MicPlacementView.
    std::unique_ptr<MicPlacementView> mPlacementViewA, mPlacementViewB;
    // ONE TOGGLE PER MIC (Jeff, 2026-08-11) -- the two mics are placed
    // independently, so which view each is looked at in is independent too.
    juce::TextButton                  mPlacementViewToggleA { "Top" };
    juce::TextButton                  mPlacementViewToggleB { "Top" };
    VKnob               mMicPlacementMixKnobB;
    std::unique_ptr<SliderAtt> mMicPlacementDistanceBAtt, mMicPlacementAngleBAtt,
                               mMicPlacementHeightBAtt,
                                 mMicPlacementMixBAtt;

    juce::Rectangle<int> mMicColumnDivider;   // painted A | B separator

    void browseForMicUserIrB();
    void updateMicSimModeBUI();
    void updateMicSimModelTooltipB();
    void updateMicAEnabled();
    void updateMicBEnabled();    // dims/disables the Mic B column when inactive

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickNAMIREditor)
};
