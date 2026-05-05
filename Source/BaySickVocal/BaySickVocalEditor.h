#pragma once
#include <JuceHeader.h>
#include "BaySickVocalProcessor.h"

// ─────────────────────────────────────────────────────────────────────────────
// BaySickVocalEditor — Phase H-6 (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────
// Stretch-to-fill editor for BaySickVocalProcessor.  Five sub-tabs:
//   1. BaySickVocals  — realtime pitch correction + page-wide controls
//   2. Vocal Chain    — De-esser / Compressor / Saturation / Limiter rack
//   3. BaySickPitch   — Newtone-clone offline pitch editor    [placeholder]
//   4. BaySickAlign   — VocAlign-clone offline alignment      [placeholder]
//   5. BaySickNAM/IR  — existing engine hosted as sub-tab     [placeholder; G-9]
//
// J-6 EQ unification (2026-05-03): the former 6th "Pre Rack EQ" tab is
// removed.  Pre + post EQ for this strip live exclusively on the Effects
// page (mixer_vox_<N>_preeq_* / mixer_vox_<N>_*) — same as every other
// strip type.
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
        // J-6 EQ unification (2026-05-03): TabPreRackEQ removed.
        kNumTabs
    };

    explicit BaySickVocalEditor (BaySickVocalProcessor& p);
    ~BaySickVocalEditor() override = default;

    void paint   (juce::Graphics&) override;
    void resized() override;

    // J-6 EQ unification (2026-05-03): setPreRackEQ removed; Pre Rack EQ is
    // now exclusively edited on the Effects page.

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
    // J-6 EQ unification (2026-05-03): mPanelPreRackEQ removed.

    int mActiveTab { TabBaySickVocals };

    juce::Component* panelForTab (int idx) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickVocalEditor)
};
