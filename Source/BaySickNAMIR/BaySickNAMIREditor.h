#pragma once
#include <JuceHeader.h>
#include "BaySickNAMIRProcessor.h"
#include "../Standalone/SharedUI.h"

// ─────────────────────────────────────────────────────────────────────────────
// BaySickNAMIREditor - Phase G-1.4
// ─────────────────────────────────────────────────────────────────────────────
// 760×340 panel, two-tone amp/cab faceplate using existing widget vocabulary
// (VKnob / DualLabelToggle / ChickenHeadSelector).  No skeuomorphic filmstrips
// - those were spec'd in NAM & IR Loader.txt §3.2 but Jeff's house style is
// VibeLAF.  Picker buttons accept right-click for a recent-files popup; the
// whole component is also a FileDragAndDropTarget so .nam → loadNamModel and
// .wav → loadImpulseResponse just work.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickNAMIREditor : public juce::AudioProcessorEditor,
                           public  juce::FileDragAndDropTarget,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit BaySickNAMIREditor (BaySickNAMIRProcessor&);
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
    juce::Label      mTitleLabel;
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

    // ── H-6d Mic Sim + Mic Placement (only visible when editor is taller
    //    than the standalone 760x340 size -- i.e. when hosted in a Vox/Inst
    //    sub-tab that gives us extra vertical room).  ─────────────────────────
    juce::Label   mMicSimSectionLbl;
    juce::Label   mMicPlacementSectionLbl;

    ChickenHeadSelector mMicSimMode;          // None / Built-in / User IR
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
    VKnob               mMicPlacementMixKnob;
    std::unique_ptr<SliderAtt> mMicPlacementDistanceAtt, mMicPlacementAngleAtt,
                                 mMicPlacementMixAtt;

    void browseForMicUserIr();
    void updateMicSimModeUI();   // updates the model combo + browse btn visibility
    void updateMicSimModelTooltip();   // refreshes "typical use" tooltip on combo

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickNAMIREditor)
};
