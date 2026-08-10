#pragma once
#include <JuceHeader.h>
#include "HarmlessProcessor.h"
#include "HarmlessLAF.h"
#include "HarmlessWaveformButton.h"
#include "HarmlessFilterRow.h"
#include "HarmlessRoutingMatrix.h"
#include "HarmlessXYZPad.h"
#include "HarmlessModEditor.h"
#include "VisualizerScreen.h"
#include "../Standalone/SharedUI.h"   // TaggedSliderAttachment
#include "../Standalone/BaySickTitleBar.h"   // QA-A (2026-05-09)

// ── HarmlessEditor ────────────────────────────────────────────────────────────
// Full single-view dense layout.  No Basic/Advanced toggle - all controls
// visible.
//
// QA-Layout T7 (Specific-2): four full-height columns, sized for Jeff's
// approved wide-short window (1047x455):
//   col 1: Output/Routing/Trem/VibLegato/Strum/XYZ stack
//   col 2: Unison/Pitch/LFO Mod stack
//   col 3: Filters/Timbre/Blur/AmpEnv/FX grid
//   col 4: Mod Editor over the Spectrogram
// ─────────────────────────────────────────────────────────────────────────────
class HarmlessEditor : public juce::AudioProcessorEditor,
                       private juce::ValueTree::Listener
{
public:
    explicit HarmlessEditor (HarmlessProcessor& p);
    ~HarmlessEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    // 2026-04-30: fired after the engine's internal preset picker loads a patch.
    // LayersPage / BassPage wire this to their onSoundNameChanged callback so
    // the ribbon tab + mixer strip + piano-roll context label all update to
    // the patch's filename.  Pre-2026-04-30 the engine editors loaded presets
    // silently and the page wrappers had no way to know.
    std::function<void(const juce::String&)> onPatchLoaded;

    // QA-Layout T3 (Window-3/4): the internal BaySickTitleBar is dissolved --
    // the hosting window's title strip shows the engine name and mounts the
    // preset button.  The editor still OWNS the button.
    juce::Component* getTitleStripPresetButton() noexcept { return &mPresetBtn; }
    static juce::String getEngineTitle()  { return "Harmless"; }
    static juce::Colour getEngineAccent() { return juce::Colour (HarmlessLAF::kAccent); }

    // Smoke round 2 (Jeff): the SW-3 Swing Mix knob moved off this title bar
    // onto the PageMenuBar (visible on every sub-tab).

private:
    void valueTreeRedirected (juce::ValueTree& tree) override;

    // ── Preset helpers ────────────────────────────────────────────────────────
    static juce::File presetsDir();
    void showPresetMenu();
    void savePreset (const juce::String& name);
    void loadPreset (const juce::File& f);

    // ── Look and Feel ─────────────────────────────────────────────────────────
    HarmlessLAF mLAF;

    // ── Header ────────────────────────────────────────────────────────────────
    // Accent = HarmlessLAF::kAccent (#FF6600 orange).  The preset button
    // mounts on the hosting window's title strip (QA-Layout T3); the internal
    // title bar is dissolved.
    BaySickPresetButton mPresetBtn { "Preset" };

    // ── Waveform buttons (visual selectors, synced to hidden sliders) ─────────
    HarmlessWaveformButton mTimbreWavA;   // Part A shape display/selector
    HarmlessWaveformButton mTimbreWavB;   // Part B shape display (display only)
    HarmlessWaveformButton mTremWavBtn;   // Tremolo waveform selector
    HarmlessWaveformButton mVibWavBtn;    // Vibrato waveform selector

    // ── Specialized panel components ──────────────────────────────────────────
    HarmlessFilterRow     mFilter1Row;
    HarmlessFilterRow     mFilter2Row;
    HarmlessRoutingMatrix mRoutingMatrix;
    HarmlessXYZPad        mXYZPad;
    HarmlessModEditor     mModEditor;
    VisualizerScreen      mSpectrogram;

    // ── Top-Left Panel: Timbre, Routing, Blur, Trem, Vib/Legato ──────────────
    // Timbre
    BaySickSlider      mTimbreShapeSlider;  // hidden, APVTS attachment for timbre_shape
    BaySickSlider      mPartBShapeSlider;   // hidden, APVTS attachment for partB_timbre_shape
    BaySickSlider      mTimbreBlend;
    BaySickSlider      mPartALevel, mPartBLevel;
    BaySickSlider      mBrownian;
    // S4 AG-1: Auto-gain mode 2-state toggle (REL / ABS).  D.4-Q1+Q2
    // (2026-05-01): moved from Timbre to Output cell to free space for the
    // new 2x2 stack of filter offsets + part masks.
    juce::TextButton mAutoGainBtn { "AG: REL" };
    // D.4-Q2 (2026-05-01): 2x2 stack added to Timbre cell.  Top row: filter 1
    // cutoff offset + filter 2 cutoff offset.  Bottom row: part A timbre filter
    // mask + part B timbre filter mask.
    BaySickSlider      mFlt1CutoffOfs;
    BaySickSlider      mFlt2CutoffOfs;
    BaySickSlider      mPartAMask;
    BaySickSlider      mPartBMask;
    // D.4-Q1 (2026-05-01): per-filter ADSR envelopes (8 knobs total) shown in
    // a new 2x2 ADSR panel placed to the right of each filter row.
    BaySickSlider      mFlt1A, mFlt1D, mFlt1S, mFlt1R;
    BaySickSlider      mFlt2A, mFlt2D, mFlt2S, mFlt2R;
    // Blur
    BaySickSlider      mBlurSize;
    BaySickSlider      mBlurTime;
    BaySickSlider      mBlurHarm;
    // Prism
    BaySickSlider      mPrismAmt;
    BaySickSlider      mPrismMode;          // discrete 0-2
    // Tremolo
    BaySickSlider      mTremShapeSlider;    // hidden, APVTS attachment for trem_shape
    BaySickSlider      mTremDepth, mTremSpeed, mTremGap;
    // Vibrato
    BaySickSlider      mVibShapeSlider;     // hidden, APVTS attachment for vib_shape
    BaySickSlider      mVibDepth, mVibSpeed, mVibEnv;
    // Legato
    BaySickSlider      mGlideTime, mLegatoLimit;
    juce::TextButton mLegatoBtn { "LEGATO" };

    // ── Top-Middle Panel: Unison ──────────────────────────────────────────────
    BaySickSlider      mUnisonVoices;       // 1-9 discrete
    BaySickSlider      mUnisonType;         // 0-3 type
    juce::TextButton mUnisonAltBtn { "ALT" };
    BaySickSlider      mUnisonPan;          // unison_spread
    BaySickSlider      mUnisonPitch;        // unison_detune
    BaySickSlider      mUnisonPhase;        // unison_phase

    // ── Top-Right Panel: filters done via HarmlessFilterRow components above
    // Bottom FX row
    BaySickSlider      mPluckDecay;
    juce::TextButton mPluckBlurBtn { "BLUR" };  // SLA-Impl #39: Pluck blur toggle (DSP shipped S1)
    BaySickSlider      mPhaserMix, mPhaserDepth, mPhaserRate;
    BaySickSlider      mPhaserWidth;             // Phaser WIDTH (= feedback)
    BaySickSlider      mPhaserOfs;               // Phaser OFS (= centre freq)
    BaySickSlider      mPhaserMaskRate;          // phaser_mask_rate
    BaySickSlider      mEQMix;

    // ── Pitch group (SLA-Impl #17-21) - new section between BLUR/PRISM and TREMOLO ─
    BaySickSlider       mPitchFreq;        // pitch_semitones - "freq" number input
    BaySickSlider       mPitchDetune;      // pitch_cents - "detune" number input
    BaySickSlider       mPitchFreqFrac;    // pitch_freq_frac - chicken-head selector
    juce::TextButton mPitchOctBtn { "OCT" };   // UI-only display toggle
    juce::TextButton mPitchHzBtn  { "Hz" };    // UI-only display toggle

    // ── Bottom-Left Panel: Global, AmpEnv, LFO, Phase, Strum ─────────────────
    // Global
    juce::TextButton mPartABtn { "A" }, mPartBBtn { "B" };
    BaySickSlider       mPartSel;           // part_sel 0-1
    BaySickSlider       mVolume, mPan;
    juce::TextButton mVelLinkBtn { "VEL" };
    juce::TextButton mCutSelfBtn { "CUT SELF" };
    juce::TextButton mCutSelfModeBtn { "SAME PITCH" };   // QA-CutSelfReview: Same Pitch / Cut All
    // Amp ADSR
    BaySickSlider      mAmpA, mAmpD, mAmpS, mAmpR;
    // Phase (compact in amp row)
    BaySickSlider      mPhaseStart, mPhaseRand;
    // LFO routing
    BaySickSlider      mLfoVel, mLfoVol, mLfoPitch;
    BaySickSlider       mLfoRate;       // global lfo_rate (13-step)
    BaySickSlider       mLfoShape;      // global lfo_shape (4-value chicken)
    juce::TextButton mLfoTempoBtn { "TEMPO" };  // global lfo_tempo bool
    // Strum
    BaySickSlider      mStrumDirSlider;     // 0-2 discrete
    BaySickSlider      mStrumTime, mStrumTns;

    // ── Section layout bounds (for paint) ─────────────────────────────────────
    // QA-Layout T7 (Specific-2): band + column frames, then one rect per
    // section.  The blank "future space" rects are gone with the dead cells
    // that held them.
    juce::Rectangle<int> mTopLeftBounds, mTopMidBounds, mTopRightBounds;
    juce::Rectangle<int> mBotBandBounds;
    // Top-left column: Output / (Tremolo | Routing) / Vibrato-Legato.
    juce::Rectangle<int> mGlobalSec, mTremSec, mRoutingSec, mVibLegatoSec;
    // Top-middle column: Unison (full height).
    juce::Rectangle<int> mUnisonSec;
    // Top-right column: (Filter 1 | its ADSR) / (Filter 2 | its ADSR) / Timbre.
    juce::Rectangle<int> mFlt1Sec, mFlt2Sec, mFlt1AdsrSec, mFlt2AdsrSec, mTimbreSec;
    // Bottom band: Pitch/LFO | Strum/XYZ | Blur-Prism/AmpEnv | FX | Spectrogram | Mod Editor.
    juce::Rectangle<int> mPitchSec, mLFOSec, mStrumSec, mXYZSec;
    juce::Rectangle<int> mBlurPrismSec, mAmpEnvSec, mFXSec, mSpectroSec, mModSec;

    // ── APVTS Attachments ─────────────────────────────────────────────────────
    using SliderAtt = TaggedSliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // Top-left
    std::unique_ptr<SliderAtt> mTimbreShapeAtt, mTimbreBlendAtt;
    std::unique_ptr<SliderAtt> mPartBShapeAtt;   // S3: Part B waveform interactivity
    std::unique_ptr<SliderAtt> mPartAAtt, mPartBAtt, mBrownianAtt;
    // D.4-Q1+Q2 (2026-05-01): timbre 2x2 stack + filter ADSRs.
    std::unique_ptr<SliderAtt> mFlt1OfsAtt, mFlt2OfsAtt;
    std::unique_ptr<SliderAtt> mPartAMaskAtt, mPartBMaskAtt;
    std::unique_ptr<SliderAtt> mFlt1AAtt, mFlt1DAtt, mFlt1SAtt, mFlt1RAtt;
    std::unique_ptr<SliderAtt> mFlt2AAtt, mFlt2DAtt, mFlt2SAtt, mFlt2RAtt;
    std::unique_ptr<SliderAtt> mBlurSizeAtt;
    std::unique_ptr<SliderAtt> mPrismAmtAtt, mPrismModeAtt;
    std::unique_ptr<SliderAtt> mTremShapeAtt, mTremDepthAtt, mTremSpeedAtt, mTremGapAtt;
    std::unique_ptr<SliderAtt> mVibShapeAtt, mVibDepthAtt, mVibSpeedAtt, mVibEnvAtt;
    std::unique_ptr<SliderAtt> mGlideTimeAtt, mLegatoLimitAtt;
    std::unique_ptr<ButtonAtt> mLegatoAtt;
    // Top-middle
    std::unique_ptr<SliderAtt> mUnisonVoicesAtt, mUnisonTypeAtt;
    std::unique_ptr<ButtonAtt> mUnisonAltAtt;
    std::unique_ptr<SliderAtt> mUnisonPanAtt, mUnisonPitchAtt, mUnisonPhaseAtt;
    // Top-right FX
    std::unique_ptr<SliderAtt> mPluckDecayAtt;
    std::unique_ptr<ButtonAtt> mPluckBlurAtt;        // SLA-Impl #39
    std::unique_ptr<SliderAtt> mPhaserMixAtt, mPhaserDepthAtt, mPhaserRateAtt;
    std::unique_ptr<SliderAtt> mPhaserWidthAtt;      // SLA-Impl #43
    std::unique_ptr<SliderAtt> mPhaserOfsAtt;        // SLA-Impl #44
    std::unique_ptr<SliderAtt> mPhaserMaskRateAtt;   // T2-H
    std::unique_ptr<SliderAtt> mEQMixAtt;
    // Pitch group (SLA-Impl #17-19)
    std::unique_ptr<SliderAtt> mPitchFreqAtt;
    std::unique_ptr<SliderAtt> mPitchDetuneAtt;
    std::unique_ptr<SliderAtt> mPitchFreqFracAtt;
    // S2 attachments
    std::unique_ptr<SliderAtt> mBlurTimeAtt, mBlurHarmAtt;
    std::unique_ptr<SliderAtt> mLfoRateAtt;
    std::unique_ptr<SliderAtt> mLfoShapeAtt;
    std::unique_ptr<ButtonAtt> mLfoTempoAtt;
    // Bottom-left
    std::unique_ptr<SliderAtt> mPartSelAtt;
    std::unique_ptr<SliderAtt> mVolumeAtt, mPanAtt;
    std::unique_ptr<ButtonAtt> mVelLinkAtt;
    std::unique_ptr<ButtonAtt> mCutSelfAtt;
    std::unique_ptr<ButtonAtt> mCutSelfModeAtt;
    std::unique_ptr<SliderAtt> mAmpAAtt, mAmpDAtt, mAmpSAtt, mAmpRAtt;
    std::unique_ptr<SliderAtt> mPhaseStartAtt, mPhaseRandAtt;
    std::unique_ptr<SliderAtt> mLfoVelAtt, mLfoVolAtt, mLfoPitchAtt;
    std::unique_ptr<SliderAtt> mStrumDirAtt, mStrumTimeAtt, mStrumTnsAtt;

    // S4 Batch 3: mEnvCurvePoints removed. Curve state now lives inside
    // HarmlessModRegistry (owned by the processor) and the mod editor pulls
    // from it directly.

    HarmlessProcessor& mProc;

    // 2026-04-19 (S3.5): per-part attachment swap. Each entry pairs a slider/
    // button with its A and B paramId; on Part A/B button click the active
    // attachment is recreated against the corresponding param so the visible
    // knob edits one part at a time. Two parallel lists for slider + button.
    struct DualSliderPart {
        juce::Slider* slider;
        juce::String  paramA;
        juce::String  paramB;
        std::unique_ptr<SliderAtt> current;
    };
    struct DualButtonPart {
        juce::Button* button;
        juce::String  paramA;
        juce::String  paramB;
        std::unique_ptr<ButtonAtt> current;
    };
    std::vector<DualSliderPart> mDualSliders;
    std::vector<DualButtonPart> mDualButtons;
    int mActivePart { 0 };   // 0 = A, 1 = B
    void rebindToPart (int part);   // 0 = A, 1 = B

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmlessEditor)
};
