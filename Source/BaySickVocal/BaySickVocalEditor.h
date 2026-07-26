#pragma once
#include <JuceHeader.h>
#include "BaySickVocalProcessor.h"
#include "../Standalone/UndoActions.h"   // QA-Fd 9a: UndoContext plumb-through

// ─────────────────────────────────────────────────────────────────────────────
// BaySickVocalEditor - Phase H-6 (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────
// Stretch-to-fill editor for BaySickVocalProcessor.  Five sub-tabs:
//   1. BaySickVocals  - realtime pitch correction + page-wide controls
//   2. Vocal Chain    - De-esser / Compressor / Saturation / Limiter rack
//   3. BaySickPitch   - offline note-by-note pitch editor     [placeholder]
//   4. BaySickAlign   - offline channel-pair time alignment   [QA-F build-out]
//   5. BaySickNAM/IR  - existing engine hosted as sub-tab     [placeholder; G-9]
//
// J-6 EQ unification (2026-05-03): the former 6th "Pre Rack EQ" tab is
// removed.  Pre + post EQ for this strip live exclusively on the Effects
// page (mixer_vox_<N>_preeq_* / mixer_vox_<N>_*) - same as every other
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

    // QA-ApvtsAutomation: the owning Vox page hands down "vox{N}_".  Vocal param
    // ids ("bsv_*") and the hosted NAM/IR's bare ids are identical across all 6
    // Vox pages, each owning its own processor, so automation registry keys need
    // the page index to stay distinct.  Call once, after construction.
    void setAutomationPrefix (const juce::String& prefix);

    void paint   (juce::Graphics&) override;
    void resized() override;

    // J-6 EQ unification (2026-05-03): setPreRackEQ removed; Pre Rack EQ is
    // now exclusively edited on the Effects page.

    // Driven by the PageMenuBar tab-slot buttons (StandaloneEditor wires the
    // setTabSlots callback to call this).  Default = TabBaySickVocals.
    void setActiveTab (int idx);
    int  getActiveTab() const noexcept { return mActiveTab; }

    // QA-Fd 9a: global undo context, forwarded to the BaySickPitch panel.
    void setUndoContext (const UndoContext& ctx);

private:
    BaySickVocalProcessor& mProc;

    // ── Sub-tab content components (owned directly; one visible at a time) ─
    class BaySickVocalsPanel;
    class VocalChainPanel;
    class PlaceholderPanel;
    class HostPanel;
    class NAMIRHostPanel;

    // QA-ApvtsAutomation: "vox{N}_" from the owning page; empty until set.
    juce::String                        mAutomationPrefix;

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
