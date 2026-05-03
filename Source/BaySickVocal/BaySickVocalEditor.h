#pragma once
#include <JuceHeader.h>
#include "BaySickVocalProcessor.h"

// ─────────────────────────────────────────────────────────────────────────────
// BaySickVocalEditor — Phase H-6 (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────
// Stretch-to-fill editor for BaySickVocalProcessor.  Six sub-tabs:
//   1. BaySickVocals  — realtime pitch correction + page-wide controls
//   2. Vocal Chain    — De-esser / Compressor / Saturation / Limiter rack
//   3. BaySickPitch   — Newtone-clone offline pitch editor    [placeholder]
//   4. BaySickAlign   — VocAlign-clone offline alignment      [placeholder]
//   5. BaySickNAM/IR  — existing engine hosted as sub-tab     [placeholder; G-9]
//   6. Pre Rack EQ    — strip's existing Pre EQ8 M/S          [placeholder; G-9]
// ─────────────────────────────────────────────────────────────────────────────

class BaySickVocalEditor : public juce::AudioProcessorEditor
{
public:
    // Tab indices map 1:1 to the page-menu-bar slot order.
    enum TabIdx
    {
        TabBaySickVocals = 0,
        TabVocalChain    = 1,
        TabBaySickPitch  = 2,
        TabBaySickAlign  = 3,
        TabBaySickNAMIR  = 4,
        TabPreRackEQ     = 5,
        kNumTabs
    };

    explicit BaySickVocalEditor (BaySickVocalProcessor& p);
    ~BaySickVocalEditor() override = default;

    void paint   (juce::Graphics&) override;
    void resized() override;

    // H-6b (2026-05-01): inject the strip's Pre Rack EQ display into the
    // "Pre Rack EQ" sub-tab.  VoxPage owns the ParametricEQDisplay (it knows
    // which strip's InsertNode the EQ binds to) and hands it here for visual
    // hosting.  Pass nullptr to clear.
    void setPreRackEQ (juce::Component* eq);

    // Driven by the PageMenuBar tab-slot buttons (StandaloneEditor wires the
    // setTabSlots callback to call this).  Default = TabBaySickVocals.
    void setActiveTab (int idx);
    int  getActiveTab() const noexcept { return mActiveTab; }

private:
    BaySickVocalProcessor& mProc;

    // ── Sub-tab content components (owned directly; one visible at a time) ─
    class BaySickVocalsPanel;
    class VocalChainPanel;
    class PlaceholderPanel;
    class HostPanel;
    class NAMIRHostPanel;

    std::unique_ptr<BaySickVocalsPanel> mPanelBaySickVocals;
    std::unique_ptr<VocalChainPanel>    mPanelVocalChain;
    std::unique_ptr<juce::Component>    mPanelBaySickPitch;   // H-6b: BaySickPitchEditor
    std::unique_ptr<juce::Component>    mPanelBaySickAlign;   // H-6c: BaySickAlignEditor
    std::unique_ptr<juce::Component>    mPanelBaySickNAMIR;   // H-6d: NAMIRHostPanel
    std::unique_ptr<HostPanel>          mPanelPreRackEQ;

    int mActiveTab { TabBaySickVocals };

    juce::Component* panelForTab (int idx) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickVocalEditor)
};
