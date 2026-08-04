#include "VibePlayerEditor.h"
#include "../AppPaths.h"
#include "../SampleLibrary.h"

// ── Constants ─────────────────────────────────────────────────────────────────
static constexpr int   kLblH         = 13;   // knob label height
static constexpr int   kPad          = 6;
static constexpr int   kKnobSz       = 55;   // knob diameter
static constexpr float kArrowSz      = 10.f; // routing arrow size (Box 3)
// 2026-04-21: 6-box grid layout constants
static constexpr int   kOuterMargin  = 10;
static constexpr int   kBoxGap       = 8;
static constexpr int   kBoxTitleH    = 22;
static constexpr int   kInnerPad     = 6;

// ── Helpers ───────────────────────────────────────────────────────────────────
void VibePlayerEditor::initKnob (juce::Slider& s, const juce::String& tooltip)
{
    s.setSliderStyle      (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle     (juce::Slider::NoTextBox, false, 0, 0);
    // 2026-04-21: show current value in a popup while hovering/dragging.
    s.setPopupDisplayEnabled (true, true, nullptr);
    if (tooltip.isNotEmpty()) s.setTooltip (tooltip);
}

void VibePlayerEditor::initLabel (juce::Label& l, const juce::String& text)
{
    l.setText              (text, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont              (juce::Font (10.0f));
    l.setColour            (juce::Label::textColourId, juce::Colour (0xFFB0B0B0));
}

// ── Constructor ───────────────────────────────────────────────────────────────
VibePlayerEditor::VibePlayerEditor (VibePlayerProcessor& p)
    : juce::AudioProcessorEditor (p), mProc (p)
{
    setLookAndFeel (&mLAF);
    // 2026-04-21: 6-box grid layout - 600x560 editor. Drums editor sizes its
    //   embedded child via setBounds; dims here only matter in standalone.
    setSize (600, 560);

    auto& avts = p.apvts;
    auto  pid  = [&] (const char* s) { return p.pid (s); };

    // QA-Layout T3: no internal header -- the hosting window's title strip
    // shows the name and mounts mPresetBtn (still owned + wired here).
    mPresetBtn.onClick = [this] { showPresetMenu(); };

    // ── Box titles ────────────────────────────────────────────────────────────
    const char* kBoxTitles[7] = {
        "SAMPLE ENGINE", "PITCH & VOICING", "DYNAMICS",
        "AMP ENVELOPE",  "LFO",             "FILTER",   "OUTPUT"
    };
    for (int i = 0; i < 7; ++i)
    {
        mBoxHdr[i].setText (kBoxTitles[i], juce::dontSendNotification);
        mBoxHdr[i].setJustificationType (juce::Justification::centred);
        mBoxHdr[i].setFont (juce::Font (11.f, juce::Font::bold));
        mBoxHdr[i].setColour (juce::Label::textColourId, juce::Colour (0xFFCCCCCC));
        addAndMakeVisible (mBoxHdr[i]);
    }

    // ── Knob helper: apply ModulationLAF, tooltip, popup value on hover/drag ──
    auto initModKnob = [&] (juce::Slider& s, const char* tooltip)
    {
        s.setSliderStyle  (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setPopupDisplayEnabled (true, true, nullptr);
        s.setLookAndFeel  (&ModulationLAF::get());
        if (tooltip && *tooltip) s.setTooltip (tooltip);
    };

    // ── Box 1: Sample Engine (2 knobs + 2 toggles) ────────────────────────────
    initModKnob (mSampleStartKnob, "Sample start offset (0-1)");
    initModKnob (mStretchKnob,     "Stretch / playback speed (0.5-2.0)");
    initLabel   (mSampleStartLbl,  "SMPL START");
    initLabel   (mStretchLbl,      "STRETCH");
    for (auto* s : { &mSampleStartKnob, &mStretchKnob }) addAndMakeVisible (*s);
    for (auto* l : { &mSampleStartLbl,  &mStretchLbl  }) addAndMakeVisible (*l);
    mReverseTog.setupOnOff ("REVERSE", "Play samples in reverse");
    mCutSelfTog.setupOnOff ("CUT SELF", "Kill previous notes when a new note starts");
    // Cut Self mode (QA-CutSelfReview): Same Pitch (top/off) vs Cut All (bottom/on).
    mCutSelfModeTog.setupNamed ("SAME PITCH", "Cut only the retriggered note",
                                "CUT ALL",    "Cut every ringing voice on each new note");
    // Match the other VibePlayer labels (0xFFB0B0B0 per initLabel).
    mReverseTog.setLabelColour (juce::Colour (0xFFB0B0B0));
    mCutSelfTog.setLabelColour (juce::Colour (0xFFB0B0B0));
    mCutSelfModeTog.setLabelColour (juce::Colour (0xFFB0B0B0));
    addAndMakeVisible (mReverseTog);
    addAndMakeVisible (mCutSelfTog);
    addAndMakeVisible (mCutSelfModeTog);

    // ── Box 2: Pitch & Voicing (5 knobs + 1 chickenhead) ─────────────────────
    initModKnob (mTuneKnob,          "Tune (semitones)");
    initModKnob (mVoiceCapKnob,      "Max simultaneous voices (1-16)");
    initModKnob (mUnisonVoicesKnob,  "Unison voice count (1-8)");
    initModKnob (mDetuneKnob,        "Detune (cents)");
    initModKnob (mUnisonSpreadKnob,  "Unison detune spread (cents)");
    initLabel   (mTuneLbl,           "TUNE");
    initLabel   (mVoiceCapLbl,       "VOICE CAP");
    initLabel   (mUnisonVoicesLbl,   "UNISON");
    initLabel   (mDetuneLbl,         "DETUNE");
    initLabel   (mUnisonSpreadLbl,   "SPREAD");
    for (auto* s : { &mTuneKnob, &mVoiceCapKnob, &mUnisonVoicesKnob,
                     &mDetuneKnob, &mUnisonSpreadKnob })
        addAndMakeVisible (*s);
    for (auto* l : { &mTuneLbl, &mVoiceCapLbl, &mUnisonVoicesLbl,
                     &mDetuneLbl, &mUnisonSpreadLbl })
        addAndMakeVisible (*l);
    mDetuneModeSel.setOptions ({
        { "S", "Simple: all unison voices get the same detune" },
        { "R", "Random: each voice gets a random detune value" },
        { "P", "Pair: voices spread symmetrically across +/- detune" }
    });
    mDetuneModeSel.setBodyTooltip ("Detune mode");
    mDetuneModeSel.setDefaultLabelColour (juce::Colours::white);
    addAndMakeVisible (mDetuneModeSel);
    initLabel   (mDetuneModeLbl, "DET MODE");
    addAndMakeVisible (mDetuneModeLbl);

    // ── Box 3: Dynamics (6 knobs: Sensitivity, VEL>MASTER, VEL>MUFFLE pair,
    //   VEL>HARDNESS pair) ──────────────────────────────────────────────────────
    initModKnob (mSensKnob,      "Sensitivity (velocity curve)");
    initModKnob (mVelVolKnob,    "Velocity -> Master Volume routing amount");
    initModKnob (mVelMuffleKnob, "Velocity -> Muffle routing amount");
    initModKnob (mMuffleKnob,    "Muffle (LP cutoff reduction target)");
    initModKnob (mVelHardKnob,   "Velocity -> Hardness routing amount");
    initModKnob (mHardnessKnob,  "Hardness (filter resonance target)");
    initLabel   (mSensLbl,       "SENS");
    initLabel   (mVelVolLbl,     "VEL>MASTER");
    initLabel   (mVelMuffleLbl,  "VEL>MUFFLE");
    initLabel   (mMuffleLbl,     "MUFFLE");
    initLabel   (mVelHardLbl,    "VEL>HARD");
    initLabel   (mHardnessLbl,   "HARDNESS");
    for (auto* s : { &mSensKnob,     &mVelVolKnob,   &mVelMuffleKnob,
                     &mMuffleKnob,   &mVelHardKnob,  &mHardnessKnob })
        addAndMakeVisible (*s);
    for (auto* l : { &mSensLbl,      &mVelVolLbl,    &mVelMuffleLbl,
                     &mMuffleLbl,    &mVelHardLbl,   &mHardnessLbl  })
        addAndMakeVisible (*l);

    // ── Box 4: Amp Envelope (ADSR) ───────────────────────────────────────────
    initModKnob (mAttackKnob,   "Attack time (seconds)");
    initModKnob (mDecayKnob,    "Decay time");
    initModKnob (mSustainKnob,  "Sustain level (0-1)");
    initModKnob (mReleaseKnob,  "Release time");
    initLabel   (mAttackLbl,    "ATTACK");
    initLabel   (mDecayLbl,     "DECAY");
    initLabel   (mSustainLbl,   "SUSTAIN");
    initLabel   (mReleaseLbl,   "RELEASE");
    for (auto* s : { &mAttackKnob, &mDecayKnob, &mSustainKnob, &mReleaseKnob })
        addAndMakeVisible (*s);
    for (auto* l : { &mAttackLbl,  &mDecayLbl,  &mSustainLbl,  &mReleaseLbl  })
        addAndMakeVisible (*l);

    // ── Box 5: LFO ───────────────────────────────────────────────────────────
    initModKnob (mLfoRateKnob, "LFO rate (Hz)");
    initModKnob (mLfoAmtKnob,  "LFO / vibrato amount");
    initLabel   (mLfoRateLbl,  "LFO RATE");
    initLabel   (mLfoAmtLbl,   "LFO");
    for (auto* s : { &mLfoRateKnob, &mLfoAmtKnob }) addAndMakeVisible (*s);
    for (auto* l : { &mLfoRateLbl,  &mLfoAmtLbl  }) addAndMakeVisible (*l);

    // ── Box 6: Filter (D.4-Q3 2026-05-01) - 3 hidden APVTS params surfaced ──
    // mFilterArticKnob removed from the UI on 2026-05-01: the bundled Core
    // Library has zero SFZs that use `group=`, so any non-zero value silences
    // the engine.  APVTS param `artic_group` retained so power-user automation
    // + presets-with-non-zero-artic still apply; just no UI knob.
    initModKnob (mFilterCutoffKnob, "Filter cutoff (Hz)");
    initModKnob (mFilterResKnob,    "Filter resonance / Q");
    initModKnob (mFilterReductKnob, "Lo-fi sample-rate reduction (0=off, 1=max grit)");
    initLabel   (mFilterCutoffLbl,  "CUTOFF");
    initLabel   (mFilterResLbl,     "RES");
    initLabel   (mFilterReductLbl,  "REDUCT");
    for (auto* s : { &mFilterCutoffKnob, &mFilterResKnob, &mFilterReductKnob })
        addAndMakeVisible (*s);
    for (auto* l : { &mFilterCutoffLbl,  &mFilterResLbl,  &mFilterReductLbl  })
        addAndMakeVisible (*l);

    // ── Box 7: Output (5 knobs - Master Volume uses white volume filmstrip) ──
    initModKnob (mPanKnob,    "Pan (L/R)");
    initModKnob (mStereoKnob, "Stereo width");
    initModKnob (mTrebleKnob, "Treble shelf (cut/boost at 8kHz)");
    initModKnob (mDriveKnob,  "Overdrive amount");
    initLabel   (mPanLbl,     "PAN");
    initLabel   (mStereoLbl,  "STEREO");
    initLabel   (mTrebleLbl,  "TREBLE");
    initLabel   (mDriveLbl,   "OVERDRIVE");
    for (auto* s : { &mPanKnob, &mStereoKnob, &mTrebleKnob, &mDriveKnob })
        addAndMakeVisible (*s);
    for (auto* l : { &mPanLbl,  &mStereoLbl,  &mTrebleLbl,  &mDriveLbl  })
        addAndMakeVisible (*l);
    // Master Volume knob: VibeLAF with "volumeKnob=white" property -> Volume White filmstrip.
    mVolumeKnob.setSliderStyle     (juce::Slider::RotaryVerticalDrag);
    mVolumeKnob.setTextBoxStyle    (juce::Slider::NoTextBox, false, 0, 0);
    mVolumeKnob.setPopupDisplayEnabled (true, true, nullptr);
    mVolumeKnob.setLookAndFeel     (&VibeLAF::get());
    mVolumeKnob.getProperties().set ("volumeKnob", "white");
    mVolumeKnob.setTooltip         ("Master volume (overall engine gain)");
    initLabel                       (mVolumeLbl, "MASTER VOL");
    addAndMakeVisible              (mVolumeKnob);
    addAndMakeVisible              (mVolumeLbl);

    // ── componentID pass (2026-04-21) ─────────────────────────────────────────
    //   Each attached slider gets its APVTS paramID as its componentID so that
    //   GlobalAutoRightClick + VKnobAutomation resolve the right-click Automate
    //   and Type-in-value menus. Pattern mirrors Harmless (see HarmlessEditor:387).
    // QA-ApvtsAutomation: the stamped id is also the automation registry key.
    // QA-ModelShell TS3: the applicator behind that key is registered model-side
    // when the engine is created, so this pass stamps and nothing else.
    auto wireID = [&p] (juce::Slider& s, const char* paramName)
    {
        const juce::String id = p.pid (paramName);
        s.setComponentID (id);
    };
    wireID (mSampleStartKnob,   "sampleStart");
    wireID (mStretchKnob,       "stretch");
    wireID (mTuneKnob,          "tune");
    wireID (mVoiceCapKnob,      "voiceCap");
    wireID (mUnisonVoicesKnob,  "unisonVoices");
    wireID (mDetuneKnob,        "detune");
    wireID (mUnisonSpreadKnob,  "unisonSpread");
    wireID (mSensKnob,          "sensitivity");
    wireID (mVelVolKnob,        "velToVolume");
    wireID (mVelMuffleKnob,     "velToMuffle");
    wireID (mMuffleKnob,        "muffle");
    wireID (mVelHardKnob,       "velToHardness");
    wireID (mHardnessKnob,      "hardness");
    wireID (mAttackKnob,        "attack");
    wireID (mDecayKnob,         "decay");
    wireID (mSustainKnob,       "sustain");
    wireID (mReleaseKnob,       "release");
    wireID (mLfoRateKnob,       "lfo_rate");
    wireID (mLfoAmtKnob,        "lfoAmt");
    wireID (mPanKnob,           "pan");
    wireID (mStereoKnob,        "stereo");
    wireID (mVolumeKnob,        "volume");
    wireID (mTrebleKnob,        "treble");
    wireID (mDriveKnob,         "drive");
    // D.4-Q3: filter box knob IDs (artic knob hidden - see ctor note)
    wireID (mFilterCutoffKnob,  "cutoff");
    wireID (mFilterResKnob,     "res");
    wireID (mFilterReductKnob,  "reduct");

    // D-CC2: same componentID pattern for non-slider attached components so
    // right-click Automate works on buttons + the detune-mode selector too.
    mReverseTog     .btn().setComponentID (p.pid ("reverse"));
    mCutSelfTog     .btn().setComponentID (p.pid ("cutSelf"));
    // QA-ApvtsAutomation Task 5 (BLU-378): cutSelfMode was attached but never
    // stamped, so it was the one control here with no Automate menu.
    mCutSelfModeTog .btn().setComponentID (p.pid ("cutSelfMode"));
    mDetuneModeSel        .setComponentID (p.pid ("detuneMode"));

    // ── APVTS Attachments ─────────────────────────────────────────────────────
    mSampleStartAtt   = std::make_unique<SliderAtt> (avts, pid ("sampleStart"),  mSampleStartKnob);
    mStretchAtt       = std::make_unique<SliderAtt> (avts, pid ("stretch"),      mStretchKnob);
    mReverseAtt       = std::make_unique<ButtonAtt> (avts, pid ("reverse"),      mReverseTog.btn());
    mCutSelfAtt       = std::make_unique<ButtonAtt> (avts, pid ("cutSelf"),      mCutSelfTog.btn());
    mCutSelfModeAtt   = std::make_unique<ButtonAtt> (avts, pid ("cutSelfMode"),  mCutSelfModeTog.btn());

    mTuneAtt          = std::make_unique<SliderAtt> (avts, pid ("tune"),         mTuneKnob);
    mVoiceCapAtt      = std::make_unique<SliderAtt> (avts, pid ("voiceCap"),     mVoiceCapKnob);
    mUnisonVoicesAtt  = std::make_unique<SliderAtt> (avts, pid ("unisonVoices"), mUnisonVoicesKnob);
    mDetuneAtt        = std::make_unique<SliderAtt> (avts, pid ("detune"),       mDetuneKnob);
    mUnisonSpreadAtt  = std::make_unique<SliderAtt> (avts, pid ("unisonSpread"), mUnisonSpreadKnob);

    mSensAtt          = std::make_unique<SliderAtt> (avts, pid ("sensitivity"),  mSensKnob);
    mVelVolAtt        = std::make_unique<SliderAtt> (avts, pid ("velToVolume"),  mVelVolKnob);
    mVelMuffleAtt     = std::make_unique<SliderAtt> (avts, pid ("velToMuffle"),  mVelMuffleKnob);
    mMuffleAtt        = std::make_unique<SliderAtt> (avts, pid ("muffle"),       mMuffleKnob);
    mVelHardAtt       = std::make_unique<SliderAtt> (avts, pid ("velToHardness"),mVelHardKnob);
    mHardnessAtt      = std::make_unique<SliderAtt> (avts, pid ("hardness"),     mHardnessKnob);

    mAttackAtt        = std::make_unique<SliderAtt> (avts, pid ("attack"),       mAttackKnob);
    mDecayAtt         = std::make_unique<SliderAtt> (avts, pid ("decay"),        mDecayKnob);
    mSustainAtt       = std::make_unique<SliderAtt> (avts, pid ("sustain"),      mSustainKnob);
    mReleaseAtt       = std::make_unique<SliderAtt> (avts, pid ("release"),      mReleaseKnob);

    mLfoRateAtt       = std::make_unique<SliderAtt> (avts, pid ("lfo_rate"),     mLfoRateKnob);
    mLfoAmtAtt        = std::make_unique<SliderAtt> (avts, pid ("lfoAmt"),       mLfoAmtKnob);

    // D.4-Q3: filter box attachments (artic knob hidden - APVTS param kept)
    mFilterCutoffAtt  = std::make_unique<SliderAtt> (avts, pid ("cutoff"),       mFilterCutoffKnob);
    mFilterResAtt     = std::make_unique<SliderAtt> (avts, pid ("res"),          mFilterResKnob);
    mFilterReductAtt  = std::make_unique<SliderAtt> (avts, pid ("reduct"),       mFilterReductKnob);

    mPanAtt           = std::make_unique<SliderAtt> (avts, pid ("pan"),          mPanKnob);
    mStereoAtt        = std::make_unique<SliderAtt> (avts, pid ("stereo"),       mStereoKnob);
    mVolumeAtt        = std::make_unique<SliderAtt> (avts, pid ("volume"),       mVolumeKnob);
    mTrebleAtt        = std::make_unique<SliderAtt> (avts, pid ("treble"),       mTrebleKnob);
    mDriveAtt         = std::make_unique<SliderAtt> (avts, pid ("drive"),        mDriveKnob);

    // ChickenHeadSelector <-> APVTS int param (manual two-way binding)
    if (auto* p = avts.getParameter (pid ("detuneMode")))
    {
        mDetuneModeAtt = std::make_unique<juce::ParameterAttachment> (
            *p,
            [this] (float v) { mDetuneModeSel.setSelectedIndex (juce::jlimit (0, 2, (int) v),
                                                                juce::dontSendNotification); },
            nullptr);
        mDetuneModeAtt->sendInitialUpdate();
        mDetuneModeSel.onChange = [this] (int idx)
        {
            if (mDetuneModeAtt) mDetuneModeAtt->setValueAsCompleteGesture ((float) idx);
        };
    }

    // Slider double-click returns each param's FACTORY default (the helper reads
    // getDefaultValue) -- standard knob behavior, independent of the loaded patch.
    setSliderDoubleClickDefaultsFromApvts (*this, mProc.apvts);
    mProc.apvts.state.addListener (this);
}

VibePlayerEditor::~VibePlayerEditor()
{
    mProc.apvts.state.removeListener (this);
    setLookAndFeel (nullptr);
}

void VibePlayerEditor::valueTreeRedirected (juce::ValueTree& tree)
{
    if (tree != mProc.apvts.state) return;
    setSliderDoubleClickDefaultsFromApvts (*this, mProc.apvts);
}

// ── Box-rect helper (used by both paint and resized) ─────────────────────────
// D.4-Q3 (2026-05-01): 7 boxes - top row has 3 boxes (Sample/Pitch/Dynamics),
// bottom row has 4 boxes (AmpEnv/LFO/Filter/Output).  Top boxes are wider
// than bottom boxes since they share the same total width split into 3 vs 4
// columns.
static juce::Rectangle<int> boxRectFor (int idx, int editorW, int editorH)
{
    const bool topRow = (idx < 3);
    const int  cols   = topRow ? 3 : 4;
    const int  col    = topRow ? idx : (idx - 3);
    const int  row    = topRow ? 0 : 1;
    const int  totalW = editorW - 2 * kOuterMargin - (cols - 1) * kBoxGap;
    const int  boxW   = totalW / cols;
    const int  boxH   = (editorH - 2 * kOuterMargin - kBoxGap) / 2;
    return { kOuterMargin + col * (boxW + kBoxGap),
             kOuterMargin + row * (boxH + kBoxGap),
             boxW, boxH };
}

// ── Paint ─────────────────────────────────────────────────────────────────────
void VibePlayerEditor::paint (juce::Graphics& g)
{
    // Background
    g.fillAll (juce::Colour (0xFF1A1C1F));

    // 6 box section titles only - no box borders, no underlines, no fill.
    //   Kept flat matte to avoid the AA ghost-rings the rounded-rect outlines
    //   were producing against the dark background. Section labels alone
    //   provide the visual grouping.

    // Routing arrows inside Box 2 (Dynamics): VEL>MUFFLE -> MUFFLE (row 1),
    //   VEL>HARD -> HARDNESS (row 2).
    auto drawArrow = [&] (int cx, int cy)
    {
        const float ax = (float) cx - kArrowSz * 0.5f;
        const float ay = (float) cy - 4.f;
        const float aw = kArrowSz;
        const float ah = 8.f;
        juce::Path arrow;
        arrow.startNewSubPath (ax, ay + ah * 0.5f);
        arrow.lineTo (ax + aw * 0.6f, ay + ah * 0.5f);
        arrow.startNewSubPath (ax + aw * 0.35f, ay);
        arrow.lineTo (ax + aw, ay + ah * 0.5f);
        arrow.lineTo (ax + aw * 0.35f, ay + ah);
        g.setColour (juce::Colour (0xFF888A90));
        g.strokePath (arrow, juce::PathStrokeType (1.5f,
                      juce::PathStrokeType::mitered,
                      juce::PathStrokeType::square));
    };

    auto dynBox = boxRectFor (2, getWidth(), getHeight());
    const int contentX = dynBox.getX() + kInnerPad;
    const int contentY = dynBox.getY() + kBoxTitleH + kInnerPad;
    const int contentW = dynBox.getWidth() - 2 * kInnerPad;
    const int contentH = dynBox.getHeight() - kBoxTitleH - 2 * kInnerPad;
    const int cellW    = contentW / 2;
    const int cellH    = contentH / 3;
    const int arrowX   = contentX + cellW;                       // on the col boundary
    // Row 1 knob centre y = row top + knob height/2
    drawArrow (arrowX, contentY + cellH     + kKnobSz / 2);
    drawArrow (arrowX, contentY + cellH * 2 + kKnobSz / 2);
}

// ── Resized ───────────────────────────────────────────────────────────────────
void VibePlayerEditor::resized()
{
    // ── Box layout helpers ───────────────────────────────────────────────────
    auto box = [&] (int i) { return boxRectFor (i, getWidth(), getHeight()); };

    // Per-cell rectangle inside a box (2 cols x 3 rows content area)
    auto cell = [&] (int boxIdx, int col, int row) -> juce::Rectangle<int>
    {
        auto b = box (boxIdx);
        const int cx = b.getX() + kInnerPad;
        const int cy = b.getY() + kBoxTitleH + kInnerPad;
        const int cw = b.getWidth() - 2 * kInnerPad;
        const int ch = b.getHeight() - kBoxTitleH - 2 * kInnerPad;
        return { cx + col * (cw / 2), cy + row * (ch / 3),
                 cw / 2, ch / 3 };
    };

    auto placeKnob = [&] (int boxIdx, int col, int row,
                           juce::Slider& s, juce::Label& l)
    {
        auto c = cell (boxIdx, col, row);
        const int kx = c.getX() + (c.getWidth() - kKnobSz) / 2;
        s.setBounds (kx, c.getY(), kKnobSz, kKnobSz);
        l.setBounds (c.getX(), c.getY() + kKnobSz, c.getWidth(), kLblH);
    };

    // Box-title labels centred across full box width
    for (int i = 0; i < 7; ++i)
    {
        auto b = box (i);
        mBoxHdr[i].setBounds (b.getX(), b.getY() + 2, b.getWidth(), kBoxTitleH - 2);
    }

    // ── Box 0: Sample Engine (2 knobs on row 0, 2 toggles spanning rows 1+2)
    //   The toggles get double cell height so the filmstrip switch + the
    //   TITLE/OFF/switch/ON label stack has proper breathing room.
    placeKnob (0, 0, 0, mSampleStartKnob, mSampleStartLbl);
    placeKnob (0, 1, 0, mStretchKnob,     mStretchLbl);
    {
        // Box 0 toggle row (QA-CutSelfReview): Reverse | CUT SELF | Same Pitch/Cut All,
        // three switches spread evenly across the full box width (rows 1-2, doubleH).
        auto      b0      = box (0);
        auto      rowCell = cell (0, 0, 1);
        const int doubleH = rowCell.getHeight() * 2;
        const int y       = rowCell.getY();
        const int tx      = b0.getX() + kInnerPad;
        const int tw      = b0.getWidth() - 2 * kInnerPad;
        const int third   = tw / 3;
        mReverseTog    .setBounds (tx,             y, third,          doubleH);
        mCutSelfTog    .setBounds (tx + third,     y, third,          doubleH);
        mCutSelfModeTog.setBounds (tx + 2 * third, y, tw - 2 * third, doubleH);
    }

    // ── Box 1: Pitch & Voicing (5 knobs + chickenhead, 2x3) ──────────────────
    placeKnob (1, 0, 0, mTuneKnob,         mTuneLbl);
    placeKnob (1, 1, 0, mVoiceCapKnob,     mVoiceCapLbl);
    placeKnob (1, 0, 1, mUnisonVoicesKnob, mUnisonVoicesLbl);
    placeKnob (1, 1, 1, mDetuneKnob,       mDetuneLbl);
    {
        auto c = cell (1, 0, 2);
        const int selSz = 50;
        const int sx = c.getX() + (c.getWidth() - selSz) / 2;
        mDetuneModeSel.setBounds (sx, c.getY(), selSz, selSz);
        mDetuneModeLbl.setBounds (c.getX(), c.getY() + selSz, c.getWidth(), kLblH);
    }
    placeKnob (1, 1, 2, mUnisonSpreadKnob, mUnisonSpreadLbl);

    // ── Box 2: Dynamics (6 knobs, 2x3 with routing arrows on rows 1 & 2) ─────
    placeKnob (2, 0, 0, mSensKnob,     mSensLbl);
    placeKnob (2, 1, 0, mVelVolKnob,   mVelVolLbl);
    placeKnob (2, 0, 1, mVelMuffleKnob, mVelMuffleLbl);
    placeKnob (2, 1, 1, mMuffleKnob,   mMuffleLbl);
    placeKnob (2, 0, 2, mVelHardKnob,  mVelHardLbl);
    placeKnob (2, 1, 2, mHardnessKnob, mHardnessLbl);

    // ── Box 3: Amp Envelope (4 knobs, 2x2 centred in 2x3 area) ───────────────
    placeKnob (3, 0, 0, mAttackKnob,  mAttackLbl);
    placeKnob (3, 1, 0, mDecayKnob,   mDecayLbl);
    placeKnob (3, 0, 1, mSustainKnob, mSustainLbl);
    placeKnob (3, 1, 1, mReleaseKnob, mReleaseLbl);

    // ── Box 4: LFO (2 knobs - centred in middle row of a 2x3 area) ───────────
    placeKnob (4, 0, 1, mLfoRateKnob, mLfoRateLbl);
    placeKnob (4, 1, 1, mLfoAmtKnob,  mLfoAmtLbl);

    // ── Box 5: Filter (D.4-Q3 - 3 knobs after artic knob removed) ───────────
    // Cutoff + Res on row 0, Reduct centred on row 1 with empty cell beside it.
    placeKnob (5, 0, 0, mFilterCutoffKnob, mFilterCutoffLbl);
    placeKnob (5, 1, 0, mFilterResKnob,    mFilterResLbl);
    placeKnob (5, 0, 1, mFilterReductKnob, mFilterReductLbl);

    // ── Box 6: Output - Master Volume on its own row 2 so it stands out.
    //   Row 0: Pan | Stereo
    //   Row 1: Overdrive | Treble
    //   Row 2: Master Volume | (empty)
    placeKnob (6, 0, 0, mPanKnob,    mPanLbl);
    placeKnob (6, 1, 0, mStereoKnob, mStereoLbl);
    placeKnob (6, 0, 1, mDriveKnob,  mDriveLbl);
    placeKnob (6, 1, 1, mTrebleKnob, mTrebleLbl);
    placeKnob (6, 0, 2, mVolumeKnob, mVolumeLbl);
}

// ── Preset management ─────────────────────────────────────────────────────────
juce::File VibePlayerEditor::presetsDir()
{
    // P4b (2026-04-23): moved Roaming -> Documents per unified-folder layout.
    return AppPaths::appRoot().getChildFile ("Presets/BaySickPlayer");
}

void VibePlayerEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Load Sample");
    // Default all file pickers to the installed Core Library (same as BaySickDrums).
    // Falls back to user Music if the library folder hasn't been created yet.
    auto libRoot = [] () -> juce::File
    {
        const auto d = SampleLibrary::getCoreLibraryDir();
        return d.isDirectory() ? d
                                : juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    };

    // D.4-Q3 follow-up (2026-05-01): direct sample loads now also fire
    // onPatchLoaded so the page's tab + mixer strip + piano-roll context label
    // update to the loaded file's name (was only happening on preset load).
    menu.addItem ("Open Folder...", [this, libRoot]
    {
        auto fc = std::make_shared<juce::FileChooser> (
            "Select Sample Folder", libRoot());
        fc->launchAsync (juce::FileBrowserComponent::openMode
                       | juce::FileBrowserComponent::canSelectDirectories,
            [this, fc] (const juce::FileChooser& ch)
            {
                const auto f = ch.getResult();
                if (f.isDirectory())
                {
                    mProc.loadSampleFolder (f);
                    if (onPatchLoaded) onPatchLoaded (f.getFileName());
                }
            });
    });
    menu.addItem ("Open SFZ...", [this, libRoot]
    {
        auto fc = std::make_shared<juce::FileChooser> (
            "Select SFZ File", libRoot(), "*.sfz");
        fc->launchAsync (juce::FileBrowserComponent::openMode
                       | juce::FileBrowserComponent::canSelectFiles,
            [this, fc] (const juce::FileChooser& ch)
            {
                const auto f = ch.getResult();
                if (f.existsAsFile())
                {
                    mProc.loadSampleSFZ (f);
                    if (onPatchLoaded) onPatchLoaded (f.getFileNameWithoutExtension());
                }
            });
    });
    menu.addItem ("Open Sample...", [this, libRoot]
    {
        auto fc = std::make_shared<juce::FileChooser> (
            "Select Sample File", libRoot(),
            "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        fc->launchAsync (juce::FileBrowserComponent::openMode
                       | juce::FileBrowserComponent::canSelectFiles,
            [this, fc] (const juce::FileChooser& ch)
            {
                const auto f = ch.getResult();
                if (f.existsAsFile())
                {
                    mProc.loadSampleFile (f);
                    if (onPatchLoaded) onPatchLoaded (f.getFileNameWithoutExtension());
                }
            });
    });

    // ── 2026-04-23: in-app Core Library browser, filtered to melodic packs ──
    // Drum packs (Hip Hop Drums / EDM Drums / Percussion) are hidden because
    // VibePlayer is the engine on Layer / Bass pages where drum samples don't
    // belong.  Skipped entirely when this editor is embedded inside a drum
    // slot (mIsDrumContext = true) since the outer BaySickDrumsEditor's slot
    // picker already handles library selection with the drum-pack filter.
    if (! mIsDrumContext)
    {
        const auto root = SampleLibrary::getCoreLibraryDir();
        if (root.isDirectory())
        {
            // Recursive walker (lambda items - each click does its own load).
            std::function<void(juce::PopupMenu&, const juce::File&)> walk;
            walk = [this, &walk] (juce::PopupMenu& m, const juce::File& dir)
            {
                juce::Array<juce::File> children;
                dir.findChildFiles (children, juce::File::findFilesAndDirectories, false);
                children.sort();
                for (const auto& c : children)
                {
                    if (c.isDirectory())
                    {
                        // Folders that DIRECTLY contain audio files become a
                        // single "load folder" item; otherwise recurse.
                        bool hasAudio = false;
                        for (const auto& f : juce::RangedDirectoryIterator (
                                                  c, false, "*.wav;*.aif;*.aiff;*.flac",
                                                  juce::File::findFiles))
                        { juce::ignoreUnused (f); hasAudio = true; break; }
                        if (hasAudio)
                            m.addItem (c.getFileName(),
                                [this, c] {
                                    mProc.loadSampleFolder (c);
                                    if (onPatchLoaded) onPatchLoaded (c.getFileName());
                                });
                        else
                        {
                            juce::PopupMenu sub;
                            walk (sub, c);
                            if (sub.getNumItems() > 0) m.addSubMenu (c.getFileName(), sub);
                        }
                    }
                    else if (c.hasFileExtension ("sfz"))
                    {
                        m.addItem (c.getFileNameWithoutExtension() + "  [SFZ]",
                            [this, c] {
                                mProc.loadSampleSFZ (c);
                                if (onPatchLoaded) onPatchLoaded (c.getFileNameWithoutExtension());
                            });
                    }
                    else if (c.hasFileExtension ("wav;aif;aiff;flac;ogg;mp3"))
                    {
                        m.addItem (c.getFileNameWithoutExtension(),
                            [this, c] {
                                mProc.loadSampleFile (c);
                                if (onPatchLoaded) onPatchLoaded (c.getFileNameWithoutExtension());
                            });
                    }
                }
            };

            juce::PopupMenu libSub;
            juce::Array<juce::File> packDirs;
            root.findChildFiles (packDirs, juce::File::findDirectories, false);
            packDirs.sort();
            for (const auto& packDir : packDirs)
            {
                if (SampleLibrary::isDrumPack (packDir.getFileName())) continue;
                juce::PopupMenu packSub;
                walk (packSub, packDir);
                if (packSub.getNumItems() > 0) libSub.addSubMenu (packDir.getFileName(), packSub);
            }
            if (libSub.getNumItems() > 0)
            {
                menu.addSeparator();
                menu.addSubMenu ("Core Library", libSub);
            }
        }
    }

    menu.addSeparator();
    menu.addSectionHeader ("Presets");

    // 2026-04-26: recursive walk so factory subfolders appear as cascading
    // submenus.  Top-level folders are filtered by drum/melodic context to
    // mirror the sample-pack filter - drum tabs see only Hip Hop / EDM Drums,
    // melodic tabs see Brass / Keys / Strings / Woodwinds.  "My Presets"
    // always shows regardless of context (user content can be either).
    std::function<void (juce::PopupMenu&, const juce::File&)> walkPresets;
    walkPresets = [this, &walkPresets] (juce::PopupMenu& sub, const juce::File& dir)
    {
        if (! dir.isDirectory()) return;
        juce::Array<juce::File> dirs;
        dir.findChildFiles (dirs, juce::File::findDirectories, false);
        dirs.sort();
        for (auto& d : dirs)
        {
            juce::PopupMenu inner;
            walkPresets (inner, d);
            if (inner.getNumItems() > 0)
                sub.addSubMenu (d.getFileName(), inner);
        }
        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
        files.sort();
        for (auto& f : files)
            sub.addItem (f.getFileNameWithoutExtension(), [this, f] { loadPreset (f); });
    };
    {
        const auto dir = presetsDir();
        if (dir.isDirectory())
        {
            juce::Array<juce::File> topLevelDirs;
            dir.findChildFiles (topLevelDirs, juce::File::findDirectories, false);
            topLevelDirs.sort();
            for (const auto& d : topLevelDirs)
            {
                const auto folderName = d.getFileName();
                // Always show user-saved presets folder.
                const bool isUserFolder = folderName.equalsIgnoreCase ("My Presets");
                if (! isUserFolder)
                {
                    const bool isDrumFolder = SampleLibrary::isDrumPack (folderName);
                    if (mIsDrumContext  && ! isDrumFolder) continue;   // drums skip melodic
                    if (! mIsDrumContext &&   isDrumFolder) continue;  // melodic skip drums
                }
                juce::PopupMenu sub;
                walkPresets (sub, d);
                if (sub.getNumItems() > 0)
                    menu.addSubMenu (folderName, sub);
            }
            // Also include any loose XMLs at the engine root (rare).
            auto rootFiles = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
            rootFiles.sort();
            for (const auto& f : rootFiles)
                menu.addItem (f.getFileNameWithoutExtension(), [this, f] { loadPreset (f); });
        }
    }

    menu.addSeparator();
    menu.addItem ("Save preset...", [this]
    {
        auto* dlg = new juce::AlertWindow ("Save Preset", "Enter preset name:",
                                            juce::MessageBoxIconType::NoIcon);
        dlg->addTextEditor ("name", "My BaySickPlayer Preset");
        dlg->addButton ("Save",   1);
        dlg->addButton ("Cancel", 0);
        dlg->enterModalState (true, juce::ModalCallbackFunction::create (
            [this, dlg] (int result)
            {
                if (result == 1) savePreset (dlg->getTextEditorContents ("name"));
                delete dlg;
            }));
    });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (mPresetBtn));
}

void VibePlayerEditor::savePreset (const juce::String& name)
{
    // 2026-04-26: user presets go into "My Presets/" subfolder.
    const auto dir = presetsDir().getChildFile ("My Presets");
    dir.createDirectory();
    if (auto xml = mProc.apvts.copyState().createXml())
        xml->writeTo (dir.getChildFile (name + ".xml"));
}

// D.4-Q3 fix (2026-05-01): preset XML format is the nested page-style format
// written by Layer/Bass/Drum savePatchAs (mirror of DrumPage::loadPlayerPreset):
//   <BaySickPlayerState>
//     <BaySickPlayerState>           ← inner = apvts state
//       <PARAM id=... value=.../> ...
//     </BaySickPlayerState>
//     <Sample kind="sfz|file|folder" path="library:... | absolute"/>
//   </BaySickPlayerState>
// The previous flat-XML parser silently failed on this format (root tag check
// rejected the wrapper element), so presets loaded the filename but never
// updated the apvts and never reloaded the sample.
void VibePlayerEditor::loadPreset (const juce::File& f)
{
    auto parsed = juce::XmlDocument::parse (f);
    if (! parsed || ! parsed->hasTagName ("BaySickPlayerState")) return;

    // 1. Apply the inner apvts state child (rewrite trackId prefix so a patch
    // saved under tk_lay_0_bsp_ loads cleanly under tk_bas_0_bsp_ etc.).
    if (auto* stateEl = parsed->getChildByName (mProc.apvts.state.getType()))
    {
        auto loaded = juce::ValueTree::fromXml (*stateEl);
        const juce::String localPrefix = mProc.getParamPrefix();
        juce::String loadedPrefix;
        for (int i = 0; i < loaded.getNumChildren(); ++i)
        {
            auto child = loaded.getChild (i);
            if (! child.hasType ("PARAM")) continue;
            const juce::String id = child.getProperty ("id").toString();
            const int tagIdx = id.indexOf ("_bsp_");
            if (id.startsWith ("tk_") && tagIdx > 3)
            {
                loadedPrefix = id.substring (0, tagIdx + 5);
                break;
            }
        }
        if (loadedPrefix.isNotEmpty() && loadedPrefix != localPrefix)
        {
            for (int i = 0; i < loaded.getNumChildren(); ++i)
            {
                auto child = loaded.getChild (i);
                if (! child.hasType ("PARAM")) continue;
                juce::String id = child.getProperty ("id").toString();
                if (id.startsWith (loadedPrefix))
                {
                    id = localPrefix + id.substring (loadedPrefix.length());
                    child.setProperty ("id", id, nullptr);
                }
            }
        }
        mProc.apvts.replaceState (loaded);
    }

    // 2. Reload the referenced sample (handles "library:rel/path" + absolute).
    // In drum context, drum-pack samples get root-normalized to MIDI 60 so
    // they play at native pitch when triggered by the drum tab's note 60.
    if (auto* sampleEl = parsed->getChildByName ("Sample"))
    {
        const juce::String kind    = sampleEl->getStringAttribute ("kind", "none");
        const juce::String pathStr = sampleEl->getStringAttribute ("path");
        juce::File path;
        if (pathStr.startsWith ("library:"))
            path = SampleLibrary::getCoreLibraryDir().getChildFile (pathStr.substring (8));
        else
            path = juce::File (pathStr);

        const int normRoot = mIsDrumContext ? 60 : -1;
        if (kind == "file" && path.existsAsFile())
            mProc.loadSampleFile (path, normRoot);
        else if (kind == "folder" && path.isDirectory())
            mProc.loadSampleFolder (path, normRoot);
        else if (kind == "sfz" && path.existsAsFile())
            mProc.loadSampleSFZ (path, normRoot);
    }

    // 2026-04-30: notify page wrapper so Layer/Bass tab + mixer strip get
    // renamed to the patch filename.
    if (onPatchLoaded)
        onPatchLoaded (f.getFileNameWithoutExtension());
}

