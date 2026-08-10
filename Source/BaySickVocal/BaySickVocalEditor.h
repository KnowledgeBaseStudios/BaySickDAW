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
//   3. BaySickPitch   - offline note-by-note pitch editor
//   4. BaySickAlign   - offline channel-pair time alignment
//   5. BaySickNAM/IR  - the processor's embedded BaySickNAMIRProcessor
//
// J-6 EQ unification (2026-05-03): the former 6th "Pre Rack EQ" tab is
// removed.  Pre + post EQ for this strip live exclusively on the Effects
// page (mixer_vox_<N>_preeq_* / mixer_vox_<N>_*) - same as every other
// strip type.
// ─────────────────────────────────────────────────────────────────────────────

class BaySickVocalEditor : public juce::AudioProcessorEditor
{
public:
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

    // QA-Layout T4 (Window-7): the in-page tab views are retired -- this
    // editor's own layout is the BaySickVocals main panel ONLY.  The four
    // former sub-tabs live in their own contained windows; the owner reaches
    // the panels through these to host them (non-owned) and wire callbacks.
    // This editor still OWNS all five panels (L9: no sys cost -- they were
    // always-built even as tabs, and the offline analyses they carry must
    // survive a satellite window closing).
    // Defined in the .cpp: the panel types are only complete there, and an
    // inline upcast from a forward-declared type does not compile.
    juce::Component* getVocalChainPanel() const noexcept;
    juce::Component* getPitchPanel()      const noexcept;
    juce::Component* getAlignPanel()      const noexcept;
    juce::Component* getNamIrPanel()      const noexcept;

    // QA-Fd 9a: global undo context, forwarded to the BaySickPitch panel.
    void setUndoContext (const UndoContext& ctx);

private:
    BaySickVocalProcessor& mProc;

    // ── Panel classes (all owned; main panel is the editor's content, the
    //    other four are hosted by their contained windows -- T4) ────────────
    class BaySickVocalsPanel;
    class VocalChainPanel;
    class NAMIRHostPanel;

    // QA-ApvtsAutomation: "vox{N}_" from the owning page; empty until set.
    juce::String                        mAutomationPrefix;

    std::unique_ptr<BaySickVocalsPanel> mPanelBaySickVocals;
    std::unique_ptr<VocalChainPanel>    mPanelVocalChain;
    std::unique_ptr<juce::Component>    mPanelBaySickPitch;   // H-6b: BaySickPitchEditor
    std::unique_ptr<juce::Component>    mPanelBaySickAlign;   // H-6c: BaySickAlignEditor
    std::unique_ptr<juce::Component>    mPanelBaySickNAMIR;   // H-6d: NAMIRHostPanel
    // J-6 EQ unification (2026-05-03): mPanelPreRackEQ removed.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickVocalEditor)
};
