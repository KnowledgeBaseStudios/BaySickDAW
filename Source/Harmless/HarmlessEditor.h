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

// ── HarmlessEditor ────────────────────────────────────────────────────────────
// Full single-view dense layout: 960 × 620
// No Basic/Advanced toggle — all controls visible.
//
// Layout:
//   Header bar (36 px) — preset button, title
//   Top row  (304 px) — [Timbre/Mod|Unison|Filter+FX]
//   Bottom row (268 px) — [Global/AmpEnv/XYZ/Strum | ModEditor]
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
    juce::TextButton mPresetBtn { "Preset v" };

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
    juce::Slider    mTimbreShapeSlider;  // hidden, APVTS attachment for timbre_shape
    juce::Slider    mPartBShapeSlider;   // S3: hidden, APVTS attachment for partB_timbre_shape
    juce::Slider    mTimbreBlend;
    juce::Slider    mPartALevel, mPartBLevel;
    juce::Slider    mBrownian;
    // S4 AG-1: Auto-gain mode 2-state toggle (REL / ABS).  D.4-Q1+Q2
    // (2026-05-01): moved from Timbre to Output cell to free space for the
    // new 2x2 stack of filter offsets + part masks.
    juce::TextButton mAutoGainBtn { "AG: REL" };
    // D.4-Q2 (2026-05-01): 2x2 stack added to Timbre cell.  Top row: filter 1
    // cutoff offset + filter 2 cutoff offset.  Bottom row: part A timbre filter
    // mask + part B timbre filter mask.
    juce::Slider    mFlt1CutoffOfs;
    juce::Slider    mFlt2CutoffOfs;
    juce::Slider    mPartAMask;
    juce::Slider    mPartBMask;
    // D.4-Q1 (2026-05-01): per-filter ADSR envelopes (8 knobs total) shown in
    // a new 2x2 ADSR panel placed to the right of each filter row.
    juce::Slider    mFlt1A, mFlt1D, mFlt1S, mFlt1R;
    juce::Slider    mFlt2A, mFlt2D, mFlt2S, mFlt2R;
    // Blur
    juce::Slider    mBlurSize;
    juce::Slider    mBlurTime;     // S2 SLA #8
    juce::Slider    mBlurHarm;     // S2 SLA #9
    // Prism
    juce::Slider    mPrismAmt;
    juce::Slider    mPrismMode;          // discrete 0-2
    // Tremolo
    juce::Slider    mTremShapeSlider;    // hidden, APVTS attachment for trem_shape
    juce::Slider    mTremDepth, mTremSpeed, mTremGap;
    // Vibrato
    juce::Slider    mVibShapeSlider;     // hidden, APVTS attachment for vib_shape
    juce::Slider    mVibDepth, mVibSpeed, mVibEnv;
    // Legato
    juce::Slider    mGlideTime, mLegatoLimit;
    juce::TextButton mLegatoBtn { "LEGATO" };

    // ── Top-Middle Panel: Unison ──────────────────────────────────────────────
    juce::Slider    mUnisonVoices;       // 1-9 discrete
    juce::Slider    mUnisonType;         // 0-3 type
    juce::TextButton mUnisonAltBtn { "ALT" };
    juce::Slider    mUnisonPan;          // unison_spread
    juce::Slider    mUnisonPitch;        // unison_detune
    juce::Slider    mUnisonPhase;        // unison_phase

    // ── Top-Right Panel: filters done via HarmlessFilterRow components above
    // Bottom FX row
    juce::Slider    mPluckDecay;
    juce::TextButton mPluckBlurBtn { "BLUR" };  // SLA-Impl #39: Pluck blur toggle (DSP shipped S1)
    juce::Slider    mPhaserMix, mPhaserDepth, mPhaserRate;
    juce::Slider    mPhaserWidth;             // SLA-Impl #43: Phaser WIDTH (= feedback)
    juce::Slider    mPhaserOfs;               // SLA-Impl #44: Phaser OFS  (= centre freq)
    juce::Slider    mPhaserMaskRate;          // T2-H: phaser_mask_rate (was UI-missing)
    juce::Slider    mEQMix;

    // ── Pitch group (SLA-Impl #17-21) - new section between BLUR/PRISM and TREMOLO ─
    juce::Slider     mPitchFreq;        // pitch_semitones - "freq" number input
    juce::Slider     mPitchDetune;      // pitch_cents - "detune" number input
    juce::Slider     mPitchFreqFrac;    // pitch_freq_frac - chicken-head selector
    juce::TextButton mPitchOctBtn { "OCT" };   // UI-only display toggle
    juce::TextButton mPitchHzBtn  { "Hz" };    // UI-only display toggle

    // ── Bottom-Left Panel: Global, AmpEnv, LFO, Phase, Strum ─────────────────
    // Global
    juce::TextButton mPartABtn { "A" }, mPartBBtn { "B" };
    juce::Slider     mPartSel;           // part_sel 0-1
    juce::Slider     mVolume, mPan;
    juce::TextButton mVelLinkBtn { "VEL" };
    juce::TextButton mCutSelfBtn { "CUT SELF" };
    // Amp ADSR
    juce::Slider    mAmpA, mAmpD, mAmpS, mAmpR;
    // Phase (compact in amp row)
    juce::Slider    mPhaseStart, mPhaseRand;
    // LFO routing
    juce::Slider    mLfoVel, mLfoVol, mLfoPitch;
    juce::Slider     mLfoRate;       // global lfo_rate (13-step)
    juce::Slider     mLfoShape;      // global lfo_shape (4-value chicken)
    juce::TextButton mLfoTempoBtn { "TEMPO" };  // global lfo_tempo bool
    // Mod XYZ destination dropdowns (S2 T2-E)
    juce::ComboBox  mModXDest, mModYDest, mModZDest;
    // Strum
    juce::Slider    mStrumDirSlider;     // 0-2 discrete
    juce::Slider    mStrumTime, mStrumTns;

    // ── Section layout bounds (for paint) ─────────────────────────────────────
    juce::Rectangle<int> mTopLeftBounds, mTopMidBounds, mTopRightBounds;
    juce::Rectangle<int> mBotLeftBounds, mBotRightBounds;
    // Repurposed by S5 layout redesign: see resized() for the new placement.
    // Top-left: Row A (mGlobalSec|mRoutingSec), Row B (mTremSec|mVibLegatoSec),
    // Row C (mBlurPrismSec blank | mStrumSec + XYZ).
    juce::Rectangle<int> mTimbreSec, mRoutingSec, mBlurPrismSec, mPitchSec, mTremSec, mVibLegatoSec;
    // Top-middle: Unison / Pitch / LFO Mod stack.
    juce::Rectangle<int> mUnisonSec;
    // Top-right: 5x2 grid. R1 = Flt1 | Flt2, R2 = Timbre | BlurPrism, R3 = AmpEnv | FX,
    // R4 + R5 = blank (future upgrade space).
    juce::Rectangle<int> mFlt1Sec, mFlt2Sec, mFXSec;
    juce::Rectangle<int> mFlt1AdsrSec, mFlt2AdsrSec;   // D.4-Q1 (2026-05-01)
    juce::Rectangle<int> mFutureR4LSec, mFutureR4RSec, mFutureR5LSec, mFutureR5RSec;
    // Bottom-left: left column blank; right column reserved for spectrogram (S5 L2).
    juce::Rectangle<int> mGlobalSec, mAmpEnvSec, mLFOSec, mStrumSec;
    juce::Rectangle<int> mFutureBL_TopSec, mFutureBL_BotSec;
    juce::Rectangle<int> mSpectroTopSec, mSpectroBotSec;

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
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<ComboAtt>  mModXDestAtt, mModYDestAtt, mModZDestAtt;
    // Bottom-left
    std::unique_ptr<SliderAtt> mPartSelAtt;
    std::unique_ptr<SliderAtt> mVolumeAtt, mPanAtt;
    std::unique_ptr<ButtonAtt> mVelLinkAtt;
    std::unique_ptr<ButtonAtt> mCutSelfAtt;
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
