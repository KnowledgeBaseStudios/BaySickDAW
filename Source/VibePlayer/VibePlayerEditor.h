#pragma once
#include <JuceHeader.h>
#include "VibePlayerProcessor.h"
#include "VibePlayerLAF.h"
#include "../Standalone/SharedUI.h"   // ChickenHeadSelector (S2 2026-04-21)
#include "../Standalone/BaySickTitleBar.h"   // QA-A (2026-05-09)

// ── VibePlayerEditor (user-facing name: BaySickPlayer) ───────────────────────
// 6-box grid layout (600 × 560):
//
//   Header (36px):  "BAYSICKPLAYER" | [Preset ▾] | [?]
//   Body: 3 cols x 2 rows boxes
//     Row 1: Sample Engine | Pitch & Voicing | Dynamics
//     Row 2: Amp Envelope  | LFO             | Output
// ─────────────────────────────────────────────────────────────────────────────
class VibePlayerEditor : public juce::AudioProcessorEditor,
                          private juce::ValueTree::Listener
{
public:
    explicit VibePlayerEditor (VibePlayerProcessor& p);
    ~VibePlayerEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    // Called by BaySickDrumsEditor after loading a sample into a slot
    void setInfoText (const juce::String& text);

    // 2026-04-23: page-context flag.  When true (set by BaySickDrumsEditor on
    // construction), the in-app Core Library submenu is hidden because the
    // outer drum-slot picker handles library selection (filtered to drum
    // packs).  Default false = melodic context (Layer / Bass page) where the
    // Core Library submenu is shown filtered to melodic packs.
    void setDrumContext (bool isDrum) { mIsDrumContext = isDrum; }

    // 2026-04-30: fired after the engine's internal preset picker loads a
    // patch.  LayersPage / BassPage wire this to onSoundNameChanged so the
    // ribbon tab + mixer strip + piano-roll context label update to the
    // patch filename.
    std::function<void(const juce::String&)> onPatchLoaded;

    // Smoke round 2 (Jeff): the SW-3 Swing Mix knob moved off this title bar
    // onto the PageMenuBar (visible on every sub-tab).

private:
    void valueTreeRedirected (juce::ValueTree& tree) override;

    // ── Preset / file menu ────────────────────────────────────────────────────
    void showPresetMenu();
    void savePreset (const juce::String& name);
    void loadPreset (const juce::File& f);
    static juce::File presetsDir();

    // ── UI helpers ────────────────────────────────────────────────────────────
    static void initKnob  (juce::Slider& s, const juce::String& tooltip = {});
    static void initLabel (juce::Label& l,  const juce::String& text);

    // ── Look and Feel ─────────────────────────────────────────────────────────
    VibePlayerLAF mLAF;

    // ── Header ────────────────────────────────────────────────────────────────
    // QA-A (2026-05-09): unified title bar.  Accent = VC::Warm (#D4A017),
    // matching the Clips + Builder ribbon-tab active colour so the player
    // page reads as one identity with its tab.
    BaySickTitleBar     mTitleBar  { "BaySickPlayer", juce::Colour (0xFFD4A017) };
    BaySickPresetButton mPresetBtn { "Preset" };

    // ── 7-box grid layout (D.4-Q3 2026-05-01 - 7th box "Filter" added):
    //    Row 1: Sample Engine | Pitch & Voicing | Dynamics
    //    Row 2: Amp Envelope  | LFO | Filter | Output  (4-up bottom row)
    juce::Label mBoxHdr[7];

    // ── Box 1: Sample Engine ─────────────────────────────────────────────────
    juce::Slider    mSampleStartKnob, mStretchKnob;
    juce::Label     mSampleStartLbl,  mStretchLbl;
    DualLabelToggle mReverseTog,      mCutSelfTog,      mCutSelfModeTog;

    // ── Box 2: Pitch & Voicing ───────────────────────────────────────────────
    juce::Slider mTuneKnob,          mVoiceCapKnob;
    juce::Slider mUnisonVoicesKnob,  mDetuneKnob;
    juce::Slider mUnisonSpreadKnob;
    juce::Label  mTuneLbl,           mVoiceCapLbl;
    juce::Label  mUnisonVoicesLbl,   mDetuneLbl;
    juce::Label  mUnisonSpreadLbl;
    ChickenHeadSelector mDetuneModeSel;
    juce::Label  mDetuneModeLbl;

    // ── Box 3: Dynamics ──────────────────────────────────────────────────────
    juce::Slider mSensKnob,     mVelVolKnob;
    juce::Slider mVelMuffleKnob, mMuffleKnob;
    juce::Slider mVelHardKnob,   mHardnessKnob;
    juce::Label  mSensLbl,      mVelVolLbl;
    juce::Label  mVelMuffleLbl, mMuffleLbl;
    juce::Label  mVelHardLbl,   mHardnessLbl;

    // ── Box 4: Amp Envelope ──────────────────────────────────────────────────
    juce::Slider mAttackKnob,  mDecayKnob;
    juce::Slider mSustainKnob, mReleaseKnob;
    juce::Label  mAttackLbl,   mDecayLbl;
    juce::Label  mSustainLbl,  mReleaseLbl;

    // ── Box 5: LFO ───────────────────────────────────────────────────────────
    juce::Slider mLfoRateKnob, mLfoAmtKnob;
    juce::Label  mLfoRateLbl,  mLfoAmtLbl;

    // ── Box 6: Filter (D.4-Q3 2026-05-01) ────────────────────────────────────
    // Surfaces 4 previously-hidden APVTS params: cutoff / res / reduct /
    // artic_group.  Sits between LFO (Box 5) and Output (now Box 7).
    juce::Slider mFilterCutoffKnob, mFilterResKnob;
    juce::Slider mFilterReductKnob, mFilterArticKnob;
    juce::Label  mFilterCutoffLbl,  mFilterResLbl;
    juce::Label  mFilterReductLbl,  mFilterArticLbl;

    // ── Box 7: Output (was Box 6 before D.4-Q3) ──────────────────────────────
    juce::Slider mPanKnob,    mStereoKnob;
    juce::Slider mVolumeKnob, mTrebleKnob;
    juce::Slider mDriveKnob;                // Overdrive
    juce::Label  mPanLbl,     mStereoLbl;
    juce::Label  mVolumeLbl,  mTrebleLbl;
    juce::Label  mDriveLbl;
    // Leftover knobs exposed in Box 7 but still driven by their existing APVTS params.

    // ── APVTS attachments ─────────────────────────────────────────────────────
    using SliderAtt = TaggedSliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // Col 1
    std::unique_ptr<SliderAtt> mDecayAtt,   mReleaseAtt;
    std::unique_ptr<SliderAtt> mPanAtt,     mStereoAtt;
    // Col 2
    std::unique_ptr<SliderAtt> mDriveAtt,   mLfoAmtAtt;
    std::unique_ptr<SliderAtt> mTrebleAtt,  mStretchAtt;
    // Col 3
    std::unique_ptr<SliderAtt> mVelMuffleAtt, mMuffleAtt;
    std::unique_ptr<SliderAtt> mVelHardAtt,   mHardnessAtt, mSensAtt;
    // Col 4
    std::unique_ptr<SliderAtt> mTuneAtt,    mDetuneAtt;
    // S2 2026-04-21 attachments
    std::unique_ptr<SliderAtt> mAttackAtt,   mSustainAtt,   mVolumeAtt,   mLfoRateAtt;
    std::unique_ptr<SliderAtt> mVelVolAtt,   mSampleStartAtt;
    std::unique_ptr<SliderAtt> mUnisonVoicesAtt, mUnisonSpreadAtt, mVoiceCapAtt;
    std::unique_ptr<ButtonAtt> mCutSelfAtt,  mReverseAtt,  mCutSelfModeAtt;
    // detuneMode selector is wired manually (ChickenHeadSelector has no juce Attachment):
    std::unique_ptr<juce::ParameterAttachment> mDetuneModeAtt;
    // D.4-Q3 (2026-05-01): Filter box attachments
    std::unique_ptr<SliderAtt> mFilterCutoffAtt, mFilterResAtt, mFilterReductAtt, mFilterArticAtt;

    VibePlayerProcessor& mProc;
    bool mIsDrumContext { false };   // 2026-04-23: page-context flag (see setter)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VibePlayerEditor)
};
