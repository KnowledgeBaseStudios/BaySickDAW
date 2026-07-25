#pragma once
#include <JuceHeader.h>
#include "BaySickBassProcessor.h"
#include "BaySickBassLAF.h"
#include "BaySickBassVisualizerScreen.h"
#include "../BaySickSynth/BssEditorComponents.h"
#include "../Standalone/SharedUI.h"   // TaggedSliderAttachment
#include "../Standalone/BaySickTitleBar.h"   // QA-A (2026-05-09)

// ── BaySickBassEditor ─────────────────────────────────────────────────────────
// 5-tab AudioProcessorEditor for BaySickBassProcessor.
// Mirrors BaySickSynthEditor layout with bass LED colour (0xFF33FF88).
//
// Layout:
//   Header (32px)     : [Preset ▾]
//   Visualizer (120px): BaySickBassVisualizerScreen
//   Tab row (30px)    : [OSC] [OSC ENV] [FILTER] [FLT ENV] [LFO]
//   Deck (remaining)  : controls for the active tab
// ─────────────────────────────────────────────────────────────────────────────
class BaySickBassEditor : public juce::AudioProcessorEditor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::ValueTree::Listener
{
public:
    explicit BaySickBassEditor (BaySickBassProcessor& p);
    ~BaySickBassEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    // 2026-04-30: fired after the engine's internal preset picker loads a
    // patch.  BassPage wires this to onSoundNameChanged so the ribbon tab +
    // mixer strip + piano-roll context label update to the patch filename.
    std::function<void(const juce::String&)> onPatchLoaded;

    // Smoke round 2 (Jeff): the SW-3 Swing Mix knob moved off this title bar
    // onto the PageMenuBar (visible on every sub-tab).

private:
    void parameterChanged   (const juce::String& paramID, float newValue) override;
    void valueTreeRedirected(juce::ValueTree& tree) override;
    void refreshLFOSyncEnableState();
    void refreshModifierTooltip();

    void setActiveTab    (int tab);
    void updateTabButtons();
    void showDeck        (int deck, bool visible);

    // ── Preset helpers ────────────────────────────────────────────────────────
    static juce::File presetsDir();
    void showPresetMenu();
    void savePreset   (const juce::String& name);
    void loadPreset   (const juce::File& f);

    // ── Layout helpers ────────────────────────────────────────────────────────
    void layoutOscDeck    (juce::Rectangle<int> deck);
    void layoutAmpEnvDeck (juce::Rectangle<int> deck);
    void layoutFilterDeck (juce::Rectangle<int> deck);
    void layoutFltEnvDeck (juce::Rectangle<int> deck);
    void layoutLFODeck    (juce::Rectangle<int> deck);
    void layoutModDeck    (juce::Rectangle<int> deck);

    static void initKnob   (juce::Slider& s, const juce::String& tooltip = {});
    static void initVSlider(juce::Slider& s, const juce::String& tooltip = {});
    static void initLabel  (juce::Label& l,  const juce::String& text);
    void        initGroup  (juce::GroupComponent& g, const juce::String& name);

    // ── Look and feel ─────────────────────────────────────────────────────────
    BaySickBassLAF mBassLAF;

    // ── Header ────────────────────────────────────────────────────────────────
    // QA-A (2026-05-09): unified title bar.  Accent = BaySickBassLAF::kGreen
    // (#33FF88 B1 neon green) -- matches STYLE-06's "preset on right + green
    // title logo" spec.
    BaySickTitleBar     mTitleBar  { "BaySickBass",
                                     juce::Colour (BaySickBassLAF::kGreen) };
    BaySickPresetButton mPresetBtn { "Preset" };

    // ── Visualizer ────────────────────────────────────────────────────────────
    BaySickBassVisualizerScreen mVisualizer;

    // ── Tab bar ───────────────────────────────────────────────────────────────
    juce::TextButton mTabBtns[6];
    static constexpr const char* kTabNames[6] = {
        "OSC", "OSC ENV", "FILTER", "FLT ENV", "LFO", "MOD"
    };
    int mActiveTab { 0 };

    // ── OSC deck ──────────────────────────────────────────────────────────────
    juce::GroupComponent mWavGroup, mVoiceGroup, mModWheelGroup;

    juce::ComboBox mWaveformCbo;
    juce::ComboBox mDualOscModeCbo;
    juce::TextButton mOscSyncBtn { "SYNC" };
    juce::TextButton mRingModBtn { "RING" };

    juce::Slider   mTransposeKnob, mModifierKnob, mNoiseKnob;
    juce::Label    mTransposeLbl,  mModifierLbl,   mNoiseLbl;

    juce::Slider   mGlideKnob;
    juce::Label    mGlideLbl;
    juce::Slider   mOutVolKnob;       // 2026-04-25 master out parity (BaySickPlayer)
    juce::Label    mOutVolLbl;
    juce::TextButton mCutSelfBtn { "CUT SELF" };
    juce::TextButton mCutSelfModeBtn { "SAME PITCH" };   // QA-CutSelfReview: Same Pitch / Cut All

    juce::Slider   mModWheelAmtKnob;
    juce::Label    mModWheelAmtLbl;

    std::unique_ptr<BssLedRadio> mVoiceModeLed;
    std::unique_ptr<BssLedRadio> mModWheelDestLed;

    // ── Amp Env deck ──────────────────────────────────────────────────────────
    juce::Slider mAmpASlider, mAmpDSlider, mAmpSSlider, mAmpRSlider;
    juce::Label  mAmpALbl,    mAmpDLbl,    mAmpSLbl,    mAmpRLbl;
    juce::Slider mVelAmpKnob;
    juce::Label  mVelAmpLbl;
    juce::Slider mPEnvASlider, mPEnvDSlider, mPEnvSSlider, mPEnvRSlider;
    juce::Label  mPEnvALbl,    mPEnvDLbl,    mPEnvSLbl,    mPEnvRLbl;
    juce::Slider mPEnvAmtKnob;
    juce::Label  mPEnvAmtLbl;
    juce::GroupComponent mAmpEnvGroup, mPitchEnvGroup;

    // ── Filter deck ───────────────────────────────────────────────────────────
    juce::GroupComponent mFilterTypeGroup, mFilterTrackGroup;
    std::unique_ptr<BssFilterXYPad> mFilterXYPad;
    std::unique_ptr<BssLedRadio>    mFilterTypeLed;

    juce::Slider  mFltKbTrackKnob, mFltVelTrackKnob;
    juce::Label   mFltKbTrackLbl,  mFltVelTrackLbl;

    // ── Filter Env deck ───────────────────────────────────────────────────────
    juce::Slider mFltASlider, mFltDSlider, mFltSSlider, mFltRSlider;
    juce::Label  mFltALbl,    mFltDLbl,    mFltSLbl,    mFltRLbl;

    juce::GroupComponent mFltAmtGroup;
    juce::Slider   mFltEnvAmtKnob;
    juce::Label    mFltEnvAmtLbl;

    // ── LFO deck ──────────────────────────────────────────────────────────────
    juce::GroupComponent mLFOShapeGroup, mLFORateGroup, mLFODestGroup, mLFOAmtGroup;

    std::unique_ptr<BssLedRadio> mLFOShapeLed;
    std::unique_ptr<BssLedRadio> mLFODestLed;

    juce::Slider     mLFORateKnob, mLFOAmtKnob;
    juce::Label      mLFORateLbl,  mLFOAmtLbl;
    juce::TextButton mLFOSyncBtn { "SYNC" };
    juce::ComboBox   mLFODivCbo;

    // ── APVTS Attachments ─────────────────────────────────────────────────────
    using SliderAtt = TaggedSliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboAtt>  mWaveformAtt;
    std::unique_ptr<ComboAtt>  mDualOscModeAtt;
    std::unique_ptr<ButtonAtt> mOscSyncAtt;
    std::unique_ptr<ButtonAtt> mRingModAtt;
    std::unique_ptr<SliderAtt> mTransposeAtt, mModifierAtt, mNoiseAtt;
    std::unique_ptr<SliderAtt> mGlideAtt, mModWheelAmtAtt, mOutVolAtt;
    std::unique_ptr<ButtonAtt> mCutSelfAtt;
    std::unique_ptr<ButtonAtt> mCutSelfModeAtt;

    std::unique_ptr<SliderAtt> mAmpAAtt, mAmpDAtt, mAmpSAtt, mAmpRAtt;
    std::unique_ptr<SliderAtt> mVelAmpAtt;
    std::unique_ptr<SliderAtt> mPEnvAAtt, mPEnvDAtt, mPEnvSAtt, mPEnvRAtt;
    std::unique_ptr<SliderAtt> mPEnvAmtAtt;

    // MOD deck
    juce::GroupComponent mNoiseGroup;
    juce::TextButton     mNoiseOnlyBtn { "NOISE ONLY" };
    juce::ComboBox       mNoiseColorCbo;
    std::unique_ptr<ButtonAtt> mNoiseOnlyAtt;
    std::unique_ptr<ComboAtt>  mNoiseColorAtt;

    juce::GroupComponent mTransientGroup;
    juce::Slider mTransAmtKnob, mTransDurKnob, mTransColKnob;
    juce::Label  mTransAmtLbl,  mTransDurLbl,  mTransColLbl;
    std::unique_ptr<SliderAtt> mTransAmtAtt, mTransDurAtt, mTransColAtt;

    juce::GroupComponent mBurstGroup;
    juce::TextButton     mBurstModeBtn { "BURST" };
    juce::Slider         mBurstCountKnob, mBurstSpacingKnob;
    juce::Label          mBurstCountLbl, mBurstSpacingLbl;
    std::unique_ptr<ButtonAtt> mBurstModeAtt;
    std::unique_ptr<SliderAtt> mBurstCountAtt, mBurstSpacingAtt;

    juce::GroupComponent mDriftGroup;
    juce::Slider         mDriftKnob;
    juce::Label          mDriftLbl;
    std::unique_ptr<SliderAtt> mDriftAtt;

    juce::GroupComponent mUnisonGroup;
    juce::Slider         mUniVoicesKnob, mUniDetuneKnob, mUniSpreadKnob;
    juce::Label          mUniVoicesLbl,  mUniDetuneLbl,  mUniSpreadLbl;
    std::unique_ptr<SliderAtt> mUniVoicesAtt, mUniDetuneAtt, mUniSpreadAtt;

    std::unique_ptr<SliderAtt> mFltKbTrackAtt, mFltVelTrackAtt;

    std::unique_ptr<SliderAtt> mFltAAtt, mFltDAtt, mFltSAtt, mFltRAtt;
    std::unique_ptr<SliderAtt> mFltEnvAmtAtt;

    std::unique_ptr<ButtonAtt> mLFOSyncAtt;
    std::unique_ptr<ComboAtt>  mLFODivAtt;
    std::unique_ptr<SliderAtt> mLFORateAtt, mLFOAmtAtt;

    BaySickBassProcessor& mProc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickBassEditor)
};
