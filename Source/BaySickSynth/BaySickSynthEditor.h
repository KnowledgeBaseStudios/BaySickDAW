#pragma once
#include <JuceHeader.h>
#include "BaySickSynthProcessor.h"
#include "BaySickSynthLAF.h"
#include "BaySickVisualizerScreen.h"
#include "BssEditorComponents.h"
#include "../Standalone/SharedUI.h"   // TaggedSliderAttachment
#include "../Standalone/BaySickTitleBar.h"   // QA-A (2026-05-09)

// ── BaySickSynthEditor ────────────────────────────────────────────────────────
// 5-tab AudioProcessorEditor for BaySickSynthProcessor.
//
// Layout:
//   Header (32px)     : [Preset ▾]
//   Visualizer (120px): BaySickVisualizerScreen
//   Tab row (30px)    : [OSC] [OSC ENV] [FILTER] [FLT ENV] [LFO]
//   Deck (remaining)  : controls for the active tab
//
// Tab 1 – OSC:
//   Left: Waveform ComboBox + Transpose/Modifier/Noise knobs (Waveform group)
//   Mid : Poly/Mono/Lead LEDs + Glide knob (Voice Mode group)
//   Right: Filter/LFO LEDs + Amount knob (Mod Wheel group)
//
// Tab 2 – Osc Env:   4 vertical sliders (A/D/S/R)
// Tab 3 – Filter:    Left=XY pad, Right=Keyboard+Velocity knobs
// Tab 4 – Filter Env: 4 vertical sliders + Amount knob
// Tab 5 – LFO:       Sine/Saw/Square LEDs | Rate knob | Filter/Pitch/OscMod LEDs | Amount knob
// ─────────────────────────────────────────────────────────────────────────────
class BaySickSynthEditor : public juce::AudioProcessorEditor,
                            private juce::AudioProcessorValueTreeState::Listener,
                            private juce::ValueTree::Listener
{
public:
    explicit BaySickSynthEditor (BaySickSynthProcessor& p);
    ~BaySickSynthEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    // 2026-04-30: fired after the engine's internal preset picker loads a
    // patch.  LayersPage / BassPage wire this to onSoundNameChanged so the
    // ribbon tab + mixer strip + piano-roll context label update to match
    // the patch filename.
    std::function<void(const juce::String&)> onPatchLoaded;

    // Smoke round 2 (Jeff): the SW-3 Swing Mix knob moved off this title bar
    // onto the PageMenuBar (visible on every sub-tab).

private:
    void parameterChanged   (const juce::String& paramID, float newValue) override;
    void valueTreeRedirected(juce::ValueTree& tree) override;
    void refreshLFOSyncEnableState();
    void refreshModifierTooltip();

protected:
    // Subclasses (BaySickBassEditor) can override to customise colours
    virtual BaySickSynthLAF& getLAF() { return mSynthLAF; }

private:
    void setActiveTab    (int tab);
    void updateTabButtons();
    void showDeck        (int deck, bool visible);

    // ── Preset helpers ────────────────────────────────────────────────────────
    virtual juce::File presetsDir() const;
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

    static void initKnob  (juce::Slider& s, const juce::String& tooltip = {});
    static void initVSlider(juce::Slider& s, const juce::String& tooltip = {});
    static void initLabel (juce::Label& l,  const juce::String& text);
    void        initGroup (juce::GroupComponent& g, const juce::String& name);

    // ── Look and feel ─────────────────────────────────────────────────────────
    BaySickSynthLAF mSynthLAF;

    // ── Header ────────────────────────────────────────────────────────────────
    // QA-A (2026-05-09): unified title bar.  Accent = BaySickSynthLAF::kGreen
    // (#A0DB2B FL green).  STYLE-06 spec: preset moves to RIGHT, title is the
    // engine's green accent.
    BaySickTitleBar     mTitleBar  { "BaySickSynth",
                                     juce::Colour (BaySickSynthLAF::kGreen) };
    BaySickPresetButton mPresetBtn { "Preset" };

    // ── Visualizer ────────────────────────────────────────────────────────────
    BaySickVisualizerScreen mVisualizer;

    // ── Tab bar ───────────────────────────────────────────────────────────────
    juce::TextButton mTabBtns[6];
    static constexpr const char* kTabNames[6] = {
        "OSC", "OSC ENV", "FILTER", "FLT ENV", "LFO", "MOD"
    };
    int mActiveTab { 0 };

    // ── OSC deck ──────────────────────────────────────────────────────────────
    juce::GroupComponent mWavGroup, mVoiceGroup, mModWheelGroup;

    juce::ComboBox mWaveformCbo;
    juce::Label    mWaveformLbl;
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

    // LED radio groups (owned via unique_ptr so they can be set up in ctor)
    std::unique_ptr<BssLedRadio> mVoiceModeLed;
    std::unique_ptr<BssLedRadio> mModWheelDestLed;

    // ── OSC Env deck (vertical sliders) ───────────────────────────────────────
    juce::Slider mAmpASlider, mAmpDSlider, mAmpSSlider, mAmpRSlider;
    juce::Label  mAmpALbl,    mAmpDLbl,    mAmpSLbl,    mAmpRLbl;
    juce::Slider mVelAmpKnob;
    juce::Label  mVelAmpLbl;
    // Pitch envelope (P3.1) - second row on OSC ENV tab
    juce::Slider mPEnvASlider, mPEnvDSlider, mPEnvSSlider, mPEnvRSlider;
    juce::Label  mPEnvALbl,    mPEnvDLbl,    mPEnvSLbl,    mPEnvRLbl;
    juce::Slider mPEnvAmtKnob;
    juce::Label  mPEnvAmtLbl;
    juce::GroupComponent mAmpEnvGroup, mPitchEnvGroup;  // side-by-side ADSR boxes

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

    juce::Slider   mLFORateKnob, mLFOAmtKnob;
    juce::Label    mLFORateLbl,  mLFOAmtLbl;
    juce::TextButton mLFOSyncBtn { "SYNC" };
    juce::ComboBox mLFODivCbo;

    // ── APVTS Attachments ─────────────────────────────────────────────────────
    using SliderAtt = TaggedSliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // OSC
    std::unique_ptr<ComboAtt>  mWaveformAtt;
    std::unique_ptr<ComboAtt>  mDualOscModeAtt;
    std::unique_ptr<ButtonAtt> mOscSyncAtt;
    std::unique_ptr<ButtonAtt> mRingModAtt;
    std::unique_ptr<SliderAtt> mTransposeAtt, mModifierAtt, mNoiseAtt;
    std::unique_ptr<SliderAtt> mGlideAtt, mModWheelAmtAtt, mOutVolAtt;
    std::unique_ptr<ButtonAtt> mCutSelfAtt;
    std::unique_ptr<ButtonAtt> mCutSelfModeAtt;
    // Amp Env
    std::unique_ptr<SliderAtt> mAmpAAtt, mAmpDAtt, mAmpSAtt, mAmpRAtt;
    std::unique_ptr<SliderAtt> mVelAmpAtt;
    std::unique_ptr<SliderAtt> mPEnvAAtt, mPEnvDAtt, mPEnvSAtt, mPEnvRAtt;
    std::unique_ptr<SliderAtt> mPEnvAmtAtt;

    // ── MOD deck (Option C layout - P3.3 noise-only is first; later D-sessions fill in the rest) ──
    juce::GroupComponent mNoiseGroup;
    juce::TextButton     mNoiseOnlyBtn { "NOISE ONLY" };
    juce::ComboBox       mNoiseColorCbo;
    std::unique_ptr<ButtonAtt> mNoiseOnlyAtt;
    std::unique_ptr<ComboAtt>  mNoiseColorAtt;

    // Transient injector (P3.5)
    juce::GroupComponent mTransientGroup;
    juce::Slider mTransAmtKnob, mTransDurKnob, mTransColKnob;
    juce::Label  mTransAmtLbl,  mTransDurLbl,  mTransColLbl;
    std::unique_ptr<SliderAtt> mTransAmtAtt, mTransDurAtt, mTransColAtt;

    // Multi-burst envelope (P3.6)
    juce::GroupComponent mBurstGroup;
    juce::TextButton     mBurstModeBtn { "BURST" };
    juce::Slider         mBurstCountKnob, mBurstSpacingKnob;
    juce::Label          mBurstCountLbl, mBurstSpacingLbl;
    std::unique_ptr<ButtonAtt> mBurstModeAtt;
    std::unique_ptr<SliderAtt> mBurstCountAtt, mBurstSpacingAtt;

    // Analog drift (P3.10)
    juce::GroupComponent mDriftGroup;
    juce::Slider         mDriftKnob;
    juce::Label          mDriftLbl;
    std::unique_ptr<SliderAtt> mDriftAtt;

    // Unison (P3.11)
    juce::GroupComponent mUnisonGroup;
    juce::Slider         mUniVoicesKnob, mUniDetuneKnob, mUniSpreadKnob;
    juce::Label          mUniVoicesLbl,  mUniDetuneLbl,  mUniSpreadLbl;
    std::unique_ptr<SliderAtt> mUniVoicesAtt, mUniDetuneAtt, mUniSpreadAtt;
    // Filter (no attachments for cutoff/res - XY pad writes directly)
    std::unique_ptr<SliderAtt> mFltKbTrackAtt, mFltVelTrackAtt;
    // Filter Env
    std::unique_ptr<SliderAtt> mFltAAtt, mFltDAtt, mFltSAtt, mFltRAtt;
    std::unique_ptr<SliderAtt> mFltEnvAmtAtt;
    // LFO
    std::unique_ptr<ButtonAtt> mLFOSyncAtt;
    std::unique_ptr<ComboAtt>  mLFODivAtt;
    std::unique_ptr<SliderAtt> mLFORateAtt, mLFOAmtAtt;

protected:
    BaySickSynthProcessor& mProc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickSynthEditor)
};
