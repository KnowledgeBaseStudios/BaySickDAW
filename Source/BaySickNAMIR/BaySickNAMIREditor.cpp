#include "BaySickNAMIREditor.h"
#include "../ProjectFileResolver.h"
#include "../AppPaths.h"
#include "../Standalone/SharedUI.h"   // VKnobAutomation registration
#include "../Standalone/UndoBracket.h"

// ─────────────────────────────────────────────────────────────────────────────
// BaySickNAMIREditor - Phase G-1.4 implementation.
// ─────────────────────────────────────────────────────────────────────────────

void BaySickNAMIREditor::setAutomationPrefix (const juce::String& prefix)
{
    if (prefix.isEmpty()) return;

    // QA-ModelShell TS3 (2026-07-27): the parameter-list walk that used to
    // register applicators here moved to
    // StandaloneEditor::registerModelEngineAutomation, which does it off the
    // rig's engine-created event under these same prefixed keys.  It had to
    // move: registration here was guarded by THIS editor's lifetime, so the
    // lanes died whenever the page did.
    //
    // The controls' Automate-menu ids must match the registry keys, so the lane
    // a right-click creates is the lane the registry answers to.
    auto knob = [&prefix] (VKnob& k, const char* id)               { k.paramId = prefix + id; };
    auto tog  = [&prefix] (DualLabelToggle& t, const char* id)     { t.btn().setComponentID (prefix + id); };
    auto sel  = [&prefix] (ChickenHeadSelector& s, const char* id) { s.setComponentID (prefix + id); };
    auto cbo  = [&prefix] (juce::ComboBox& c, const char* id)      { c.setComponentID (prefix + id); };

    knob (mInGainKnob,                "input_gain");
    knob (mGateThreshKnob,            "gate_threshold");
    knob (mGateReleaseKnob,           "gate_release");
    knob (mLowCutKnob,                "low_cut");
    knob (mHighCutKnob,               "high_cut");
    knob (mCabMixKnob,                "cab_mix");
    knob (mOutputKnob,                "output");
    knob (mMicSimMixKnob,             "nam_micsim_mix");
    knob (mMicPlacementDistanceKnob,  "nam_placement_distance_cm");
    knob (mMicPlacementAngleKnob,     "nam_placement_angle_deg");
    knob (mMicPlacementMixKnob,       "nam_placement_mix");
    knob (mMicSimMixKnobB,            "nam_micsim_b_mix");
    knob (mMicPlacementDistanceKnobB, "nam_placement_b_distance_cm");
    knob (mMicPlacementAngleKnobB,    "nam_placement_b_angle_deg");
    knob (mMicPlacementMixKnobB,      "nam_placement_b_mix");

    tog  (mNamBypassToggle,           "nam_bypass");
    tog  (mCabBypassToggle,           "cab_bypass");
    tog  (mMicBActiveToggle,          "nam_micb_active");

    sel  (mOSSelector,                "oversampling");
    sel  (mMicSimMode,                "nam_micsim_mode");
    sel  (mMicPlacementPolar,         "nam_placement_polar");
    sel  (mMicSimModeB,               "nam_micsim_b_mode");
    sel  (mMicPlacementPolarB,        "nam_placement_b_polar");

    cbo  (mMicSimModelCombo,          "nam_micsim_model");
    cbo  (mMicSimModelComboB,         "nam_micsim_b_model");
}

namespace
{
    constexpr int kEditorW       = 760;
    constexpr int kEditorH       = 560;   // H-6d: grew from 340 to fit
                                           // Mic Sim + Mic Placement rows.

    // ── H-6d (2026-05-02): default Presets folders for the 3 file pickers ──
    juce::File namIrPresetsRoot()
    {
        return AppPaths::appRoot()
                  .getChildFile ("Presets")
                  .getChildFile ("BaySickNAMIR");
    }
    juce::File namPresetsDir()    { return namIrPresetsRoot().getChildFile ("NAM"); }
    juce::File irPresetsDir()     { return namIrPresetsRoot().getChildFile ("IR"); }
    juce::File micIrPresetsDir()  { return namIrPresetsRoot().getChildFile ("MIC IR"); }

    // Returns the dir if it exists, or creates+returns it.  Falls back to
    // userDocumentsDirectory if creation fails (rare; permissions issue).
    juce::File ensurePresetsDir (const juce::File& dir)
    {
        if (! dir.exists())
            dir.createDirectory();
        return dir.exists()
                  ? dir
                  : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    }
    constexpr int kHeaderH       = BaySickTitleBar::kStandardHeight;   // 32, was 28 (QA-A 2026-05-09)
    constexpr int kFileRowH      = 50;
    constexpr int kKnobsRowH     = 130;
    constexpr int kPad           = 12;
    constexpr int kSlotABtnW     = 28;
    constexpr int kSlotABtnH     = 20;
    // QA-A (2026-05-09): kSlotABtnY removed; slots now vertically centered in
    // the unified title bar via (kHeaderH - kSlotABtnH) / 2 inline below.

    constexpr juce::uint32 kAmpBgARGB    = 0xff222222;
    constexpr juce::uint32 kCabBgARGB    = 0xff181818;
    constexpr juce::uint32 kHeaderBgARGB = 0xff0a0a0a;
    constexpr juce::uint32 kDividerARGB  = 0xff333333;
    constexpr juce::uint32 kAmberARGB    = 0xffffb84d;   // NAM "LCD" amber
    constexpr juce::uint32 kCabGreenARGB = 0xff66ff88;   // IR "LCD" green
    constexpr juce::uint32 kHintARGB     = 0xffffaa33;
    constexpr juce::uint32 kErrARGB      = 0xffff5555;
}

BaySickNAMIREditor::BaySickNAMIREditor (BaySickNAMIRProcessor& p)
    : juce::AudioProcessorEditor (p),
      processor (p),
      mInGainKnob       ("Input Gain",  0.0f,
        "Pre-NAM gain (-24..+24 dB).  Tweak so the model is hit at the level it was captured at."),
      mGateThreshKnob   ("Gate Thr",   -50.0f,
        "Noise gate threshold (-60..0 dB).  Anything below this gets gated to silence."),
      mGateReleaseKnob  ("Gate Rel",   100.0f,
        "Noise gate release (5..500 ms).  How long the gate takes to close after the signal drops below threshold."),
      mLowCutKnob       ("Low Cut",     20.0f,
        "Pre-cab high-pass (20..500 Hz).  Tightens the low end before the IR."),
      mHighCutKnob      ("High Cut", 20000.0f,
        "Pre-cab low-pass (3k..20k Hz).  Tames fizz and brightness before the IR."),
      mCabMixKnob       ("Cab Mix",    100.0f,
        "Wet / dry mix on the cabinet IR (0..100 %).  100 = full cab; 0 = none."),
      mOutputKnob       ("Output",      0.0f,
        "Master output (-24..+12 dB).  Adjust to match levels with other inserts."),
      mMicSimMixKnob              ("Mix",      100.0f,
        "Mic Sim wet/dry mix (0..100 %)."),
      mMicPlacementDistanceKnob   ("Distance", 30.0f,
        "Virtual mic distance from the source (1..150 cm).  Closer = brighter + proximity bass; farther = darker + quieter."),
      mMicPlacementAngleKnob      ("Angle",     0.0f,
        "Off-axis angle (-90..+90 deg).  0 = on-axis.  Off-axis darkens the high end based on polar pattern."),
      mMicPlacementMixKnob        ("Mix",      100.0f,
        "Mic Placement wet/dry mix (0..100 %)."),
      mMicSimMixKnobB             ("Mix",      100.0f,
        "Mic B Sim wet/dry mix (0..100 %)."),
      mMicPlacementDistanceKnobB  ("Distance", 30.0f,
        "Mic B virtual distance from the source (1..150 cm).  Offset from Mic A's distance for comb-filter coloration."),
      mMicPlacementAngleKnobB     ("Angle",     0.0f,
        "Mic B off-axis angle (-90..+90 deg).  0 = on-axis."),
      mMicPlacementMixKnobB       ("Mix",      100.0f,
        "Mic B Placement wet/dry mix (0..100 %).")
{
    setSize (kEditorW, kEditorH);
    setWantsKeyboardFocus (false);

    // ── Header ───────────────────────────────────────────────────────────────
    // QA-A (2026-05-09): unified title bar replaces juce::Label + custom paint.
    // Original mTitleLabel had a hover tooltip explaining "Neural Amp Modeler
    // + IR cabinet" -- dropped per Jeff (other engines don't have title-hover
    // tooltips either, and adding SettableTooltipClient to BaySickTitleBar
    // correlated with a slider-paint crash in Harmless that wasn't worth
    // chasing).  Engine usage docs cover the acronym explanation.
    addAndMakeVisible (mTitleBar);

    auto wireSlotBtn = [this] (juce::TextButton& btn, int slot,
                               juce::Button::ConnectedEdgeFlags edge,
                               const juce::String& tip)
    {
        addAndMakeVisible (btn);
        btn.setRadioGroupId (1);
        btn.setClickingTogglesState (true);
        btn.setConnectedEdges (edge);
        btn.setTooltip (tip);
        btn.onClick = [this, slot, &btn]()
        {
            if (btn.getToggleState())
                setActiveSlotFromUI (slot);
        };
    };
    wireSlotBtn (mSlotABtn, 0, juce::Button::ConnectedOnRight,
        "Slot A.  Loads A's NAM model + IR.  Click B to A/B compare two amp setups.");
    wireSlotBtn (mSlotBBtn, 1, juce::Button::ConnectedOnLeft,
        "Slot B.  Loads B's NAM model + IR.  Click A to swap back.");

    // ── Section labels ───────────────────────────────────────────────────────
    auto setupSectionLbl = [] (juce::Label& l, const juce::String& txt, const juce::String& tip)
    {
        l.setText (txt, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredLeft);
        l.setColour (juce::Label::textColourId, juce::Colour (0xffd0d0d0));
        l.setTooltip (tip);
    };
    setupSectionLbl (mAmpSectionLbl, "AMP",
        "Amp section - loads a .nam neural-network amp capture.  "
        "Run the dry guitar through here to colour it like the captured amp.");
    setupSectionLbl (mCabSectionLbl, "CAB",
        "Cab section - loads a .wav impulse response of a guitar cabinet.  "
        "The convolution sits after the amp.  Bypass when using a Full-Rig NAM.");
    addAndMakeVisible (mAmpSectionLbl);
    addAndMakeVisible (mCabSectionLbl);

    // ── File pickers + filename labels ───────────────────────────────────────
    addAndMakeVisible (mNamBrowseBtn);
    addAndMakeVisible (mIrBrowseBtn);
    mNamBrowseBtn.setButtonText ("Load .nam...");
    mIrBrowseBtn .setButtonText ("Load .wav IR...");
    mNamBrowseBtn.onClick      = [this]() { browseForNamFile(); };
    mIrBrowseBtn .onClick      = [this]() { browseForIrFile();  };
    mNamBrowseBtn.onRightClick = [this]() { showRecentNamMenu(); };
    mIrBrowseBtn .onRightClick = [this]() { showRecentIrMenu();  };
    mNamBrowseBtn.setTooltip ("Left-click: open file browser.  Right-click: recent files.");
    mIrBrowseBtn .setTooltip ("Left-click: open file browser.  Right-click: recent IRs.");

    addAndMakeVisible (mNamFileLabel);
    addAndMakeVisible (mIrFileLabel);
    auto setupLcd = [] (juce::Label& l, juce::Colour textC, const juce::String& tip)
    {
        l.setJustificationType (juce::Justification::centredLeft);
        l.setColour (juce::Label::textColourId,        textC);
        l.setColour (juce::Label::backgroundColourId,  juce::Colour (0xff1a1a1a));
        l.setColour (juce::Label::outlineColourId,     juce::Colour (0xff333333));
        l.setBorderSize ({ 2, 6, 2, 6 });
        l.setTooltip (tip);
    };
    setupLcd (mNamFileLabel, juce::Colour (kAmberARGB),
        "Currently-loaded .nam model for this slot.  Drag a .nam file here, or use the Browse button.");
    setupLcd (mIrFileLabel,  juce::Colour (kCabGreenARGB),
        "Currently-loaded .wav IR for this slot.  Drag a .wav file here, or use the Browse button.");

    // ── Bypass toggles (Named mode: OFF on top, ON on bottom; white labels)
    addAndMakeVisible (mNamBypassToggle);
    mNamBypassToggle.setupNamed (
        "OFF", "Switch up = NAM bypass OFF.  The neural amp model is processing audio normally.",
        "ON",  "Switch down = NAM bypass ON.  The neural amp model is skipped - dry input passes straight to the filters / cab.");
    mNamBypassToggle.setLabelColour (juce::Colours::white);
    mNamBypassToggle.btn().setTooltip (
        "NAM bypass.  Skips the neural amp model entirely.");
    mNamBypassAtt = std::make_unique<ButtonAtt> (
                        processor.apvts, "nam_bypass", mNamBypassToggle.btn());

    addAndMakeVisible (mCabBypassToggle);
    mCabBypassToggle.setupNamed (
        "OFF", "Switch up = Cab bypass OFF.  The IR cabinet convolution is on.",
        "ON",  "Switch down = Cab bypass ON.  The IR cabinet is skipped - useful when running a Full-Rig NAM that already includes a cab.");
    mCabBypassToggle.setLabelColour (juce::Colours::white);
    mCabBypassToggle.btn().setTooltip (
        "Cab bypass.  Skips the IR cabinet convolution.");
    mCabBypassAtt = std::make_unique<ButtonAtt> (
                        processor.apvts, "cab_bypass", mCabBypassToggle.btn());

    // ── Knobs + APVTS attachments ────────────────────────────────────────────
    auto bindKnob = [&] (VKnob& k, const char* paramId, std::unique_ptr<SliderAtt>& att)
    {
        addAndMakeVisible (k);
        k.paramId = paramId;
        att = std::make_unique<SliderAtt> (processor.apvts, paramId, k.slider);
    };
    bindKnob (mInGainKnob,      "input_gain",     mInGainAtt);
    bindKnob (mGateThreshKnob,  "gate_threshold", mGateThreshAtt);
    bindKnob (mGateReleaseKnob, "gate_release",   mGateReleaseAtt);
    bindKnob (mLowCutKnob,      "low_cut",        mLowCutAtt);
    bindKnob (mHighCutKnob,     "high_cut",       mHighCutAtt);
    bindKnob (mCabMixKnob,      "cab_mix",        mCabMixAtt);
    bindKnob (mOutputKnob,      "output",         mOutputAtt);

    // ── OS selector (chicken head + manual APVTS sync) ───────────────────────
    addAndMakeVisible (mOSSelector);
    mOSSelector.setOptions ({
        { "1", "1x", "No oversampling.  Fastest." },
        { "2", "2x", "2x oversampling.  Reduces aliasing on high-gain models." },
        { "4", "4x", "4x oversampling.  Best quality.  CPU-heavy." },
    });
    mOSSelector.setBodyTooltip (
        "Oversampling factor around the NAM block.  Higher = less aliasing on saturated tones.");
    if (auto* p = processor.apvts.getRawParameterValue ("oversampling"))
        mOSSelector.setSelectedIndex ((int) p->load(), juce::dontSendNotification);
    mOSSelector.onChange = [this] (int idx)
    {
        if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (
                            processor.apvts.getParameter ("oversampling")))
            *prm = idx;
    };

    addAndMakeVisible (mOSSelectorLbl);
    mOSSelectorLbl.setText ("OS", juce::dontSendNotification);
    mOSSelectorLbl.setJustificationType (juce::Justification::centredTop);
    mOSSelectorLbl.setColour (juce::Label::textColourId, juce::Colour (0xffd0d0d0));
    mOSSelectorLbl.setTooltip (
        "Oversampling factor.  1x = no OS (cheapest).  2x / 4x reduce aliasing on saturated tones at the cost of CPU.");

    // ── Status row ───────────────────────────────────────────────────────────
    addAndMakeVisible (mFullRigHint);
    mFullRigHint.setText ({}, juce::dontSendNotification);
    mFullRigHint.setColour (juce::Label::textColourId, juce::Colour (kHintARGB));
    mFullRigHint.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (mErrorLabel);
    mErrorLabel.setText ({}, juce::dontSendNotification);
    mErrorLabel.setColour (juce::Label::textColourId, juce::Colour (kErrARGB));
    mErrorLabel.setJustificationType (juce::Justification::centredRight);

    // ── H-6d: Mic Sim + Mic Placement sections ───────────────────────────────
    auto setupSubSectionLbl = [] (juce::Label& l, const juce::String& txt,
                                    const juce::String& tip)
    {
        l.setText (txt, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredLeft);
        l.setColour (juce::Label::textColourId, juce::Colour (0xffd0d0d0));
        l.setTooltip (tip);
    };
    setupSubSectionLbl (mMicSimSectionLbl, "MIC SIM A",
        "Mic fingerprint stage applied AFTER the cabinet IR.  None / Built-in "
        "(10 generic mic archetypes) / User IR (load your own captured mic IR).");
    setupSubSectionLbl (mMicPlacementSectionLbl, "MIC PLACEMENT A",
        "Virtual mic position model: distance + off-axis angle + polar pattern.  "
        "Models 1/r gain, air absorption, proximity-effect bass, off-axis darkening.");
    setupSubSectionLbl (mMicSimBSectionLbl, "MIC SIM B",
        "Second parallel mic over the same post-cab signal.  Its output SUMS "
        "with Mic A (like two real mics on one source), it does not crossfade.");
    setupSubSectionLbl (mMicPlacementBSectionLbl, "MIC PLACEMENT B",
        "Mic B virtual position.  Offset distance/angle from Mic A for "
        "comb-filter coloration from the path difference.");
    addAndMakeVisible (mMicSimSectionLbl);
    addAndMakeVisible (mMicPlacementSectionLbl);
    addAndMakeVisible (mMicSimBSectionLbl);
    addAndMakeVisible (mMicPlacementBSectionLbl);

    // Mic Sim mode chickenhead (None / Built-in / User IR -- exclusive)
    addAndMakeVisible (mMicSimMode);
    mMicSimMode.setOptions ({
        { "Off",  "None",     "Mic Sim disabled (passthrough)" },
        { "Bln",  "Built-in", "Use one of 10 built-in mic archetypes" },
        { "User", "User IR",  "Load your own captured mic IR (.wav)" },
    });
    mMicSimMode.setBodyTooltip ("Mic Sim mode");
    if (auto* p = processor.apvts.getRawParameterValue ("nam_micsim_mode"))
        mMicSimMode.setSelectedIndex ((int) p->load(), juce::dontSendNotification);
    mMicSimMode.onChange = [this] (int idx)
    {
        beginParamUndoGesture (processor.apvts, "nam_micsim_mode"); // Task 6 (12-iv)
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                          processor.apvts.getParameter ("nam_micsim_mode")))
            p->setValueNotifyingHost (
                p->getNormalisableRange().convertTo0to1 ((float) idx));
        updateMicSimModeUI();
    };
    addAndMakeVisible (mMicSimModeLbl);
    mMicSimModeLbl.setText ("Mode", juce::dontSendNotification);
    mMicSimModeLbl.setJustificationType (juce::Justification::centred);
    mMicSimModeLbl.setColour (juce::Label::textColourId, juce::Colour (0xff909090));

    // Built-in model dropdown
    addAndMakeVisible (mMicSimModelCombo);
    for (int i = 0; i < (int) MicSimDSP::Model::kNumModels; ++i)
        mMicSimModelCombo.addItem (MicSimDSP::modelDisplayName (i), i + 1);
    if (auto* p = processor.apvts.getRawParameterValue ("nam_micsim_model"))
        mMicSimModelCombo.setSelectedId ((int) p->load() + 1, juce::dontSendNotification);
    mMicSimModelCombo.onChange = [this]()
    {
        const int idx = juce::jmax (0, mMicSimModelCombo.getSelectedId() - 1);
        beginParamUndoGesture (processor.apvts, "nam_micsim_model"); // Task 6 (12-iv)
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                          processor.apvts.getParameter ("nam_micsim_model")))
            p->setValueNotifyingHost (
                p->getNormalisableRange().convertTo0to1 ((float) idx));
        updateMicSimModelTooltip();
    };
    updateMicSimModelTooltip();

    addAndMakeVisible (mMicSimModelLbl);
    mMicSimModelLbl.setText ("Model", juce::dontSendNotification);
    mMicSimModelLbl.setJustificationType (juce::Justification::centred);
    mMicSimModelLbl.setColour (juce::Label::textColourId, juce::Colour (0xff909090));

    // User IR file picker
    addAndMakeVisible (mMicSimUserIrBtn);
    mMicSimUserIrBtn.setButtonText ("Load Mic IR...");
    mMicSimUserIrBtn.setTooltip ("Left-click: browse for a .wav mic IR.  "
                                   "Right-click: clear the loaded IR.");
    mMicSimUserIrBtn.onClick      = [this]() { browseForMicUserIr(); };
    mMicSimUserIrBtn.onRightClick = [this]()
    {
        processor.clearUserMicIr();
        mMicSimUserIrLabel.setText ("(no IR loaded)", juce::dontSendNotification);
    };
    addAndMakeVisible (mMicSimUserIrLabel);
    mMicSimUserIrLabel.setJustificationType (juce::Justification::centredLeft);
    mMicSimUserIrLabel.setColour (juce::Label::textColourId, juce::Colour (kCabGreenARGB));
    mMicSimUserIrLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1a1a1a));
    mMicSimUserIrLabel.setColour (juce::Label::outlineColourId,    juce::Colour (0xff333333));
    mMicSimUserIrLabel.setBorderSize ({ 2, 6, 2, 6 });
    {
        const auto path = processor.getMicSim().getUserIrPath();
        mMicSimUserIrLabel.setText (path.isEmpty() ? "(no IR loaded)"
                                                    : ProjectFileResolver::resolve (path).getFileName(),
                                      juce::dontSendNotification);
    }

    // Mic Sim mix knob
    addAndMakeVisible (mMicSimMixKnob);
    mMicSimMixAtt = std::make_unique<SliderAtt> (processor.apvts,
                                                   "nam_micsim_mix",
                                                   mMicSimMixKnob.slider);

    // Mic Placement polar chickenhead
    addAndMakeVisible (mMicPlacementPolar);
    mMicPlacementPolar.setOptions ({
        { "O",    "Omni",          "Equal pickup all directions; no proximity effect" },
        { "Card", "Cardioid",      "Heart-shaped pattern; rejects rear" },
        { "Sup",  "Supercardioid", "Tighter pattern with small rear lobe" },
        { "Hyp",  "Hypercardioid", "Sharper still; pronounced side rejection" },
        { "8",    "Figure-8",      "Bidirectional; equal front + rear, side rejection" },
    });
    mMicPlacementPolar.setBodyTooltip ("Polar pattern");
    if (auto* p = processor.apvts.getRawParameterValue ("nam_placement_polar"))
        mMicPlacementPolar.setSelectedIndex ((int) p->load(), juce::dontSendNotification);
    mMicPlacementPolar.onChange = [this] (int idx)
    {
        beginParamUndoGesture (processor.apvts, "nam_placement_polar"); // Task 6 (12-iv)
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                          processor.apvts.getParameter ("nam_placement_polar")))
            p->setValueNotifyingHost (
                p->getNormalisableRange().convertTo0to1 ((float) idx));
    };
    addAndMakeVisible (mMicPlacementPolarLbl);
    mMicPlacementPolarLbl.setText ("Polar", juce::dontSendNotification);
    mMicPlacementPolarLbl.setJustificationType (juce::Justification::centred);
    mMicPlacementPolarLbl.setColour (juce::Label::textColourId, juce::Colour (0xff909090));

    // Mic Placement knobs
    addAndMakeVisible (mMicPlacementDistanceKnob);
    addAndMakeVisible (mMicPlacementAngleKnob);
    addAndMakeVisible (mMicPlacementMixKnob);
    mMicPlacementDistanceAtt = std::make_unique<SliderAtt> (processor.apvts,
                                                              "nam_placement_distance_cm",
                                                              mMicPlacementDistanceKnob.slider);
    mMicPlacementAngleAtt    = std::make_unique<SliderAtt> (processor.apvts,
                                                              "nam_placement_angle_deg",
                                                              mMicPlacementAngleKnob.slider);
    mMicPlacementMixAtt      = std::make_unique<SliderAtt> (processor.apvts,
                                                              "nam_placement_mix",
                                                              mMicPlacementMixKnob.slider);

    updateMicSimModeUI();

    // ── QA-Fc Mic B column (mirrors the Mic A wiring, `_b_` params) ──────────
    addAndMakeVisible (mMicBActiveToggle);
    mMicBActiveToggle.setupNamed (
        "OFF", "Switch up = Mic B OFF.  Single-mic chain, exactly as before dual-mic.",
        "ON",  "Switch down = Mic B ON.  A second parallel mic processes the same "
               "post-cab signal and sums into the output - like two real mics on one source.");
    mMicBActiveToggle.setLabelColour (juce::Colours::white);
    mMicBActiveToggle.btn().setTooltip (
        "Mic B active.  Adds a second parallel mic path (summed, not blended).");
    mMicBActiveAtt = std::make_unique<ButtonAtt> (
                        processor.apvts, "nam_micb_active", mMicBActiveToggle.btn());

    addAndMakeVisible (mMicSimModeB);
    mMicSimModeB.setOptions ({
        { "Off",  "None",     "Mic B Sim disabled (passthrough)" },
        { "Bln",  "Built-in", "Use one of 10 built-in mic archetypes" },
        { "User", "User IR",  "Load your own captured mic IR (.wav)" },
    });
    mMicSimModeB.setBodyTooltip ("Mic B Sim mode");
    if (auto* p = processor.apvts.getRawParameterValue ("nam_micsim_b_mode"))
        mMicSimModeB.setSelectedIndex ((int) p->load(), juce::dontSendNotification);
    mMicSimModeB.onChange = [this] (int idx)
    {
        beginParamUndoGesture (processor.apvts, "nam_micsim_b_mode"); // Task 6 (12-iv)
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                          processor.apvts.getParameter ("nam_micsim_b_mode")))
            p->setValueNotifyingHost (
                p->getNormalisableRange().convertTo0to1 ((float) idx));
        updateMicSimModeBUI();
    };
    addAndMakeVisible (mMicSimModeBLbl);
    mMicSimModeBLbl.setText ("Mode", juce::dontSendNotification);
    mMicSimModeBLbl.setJustificationType (juce::Justification::centred);
    mMicSimModeBLbl.setColour (juce::Label::textColourId, juce::Colour (0xff909090));

    addAndMakeVisible (mMicSimModelComboB);
    for (int i = 0; i < (int) MicSimDSP::Model::kNumModels; ++i)
        mMicSimModelComboB.addItem (MicSimDSP::modelDisplayName (i), i + 1);
    if (auto* p = processor.apvts.getRawParameterValue ("nam_micsim_b_model"))
        mMicSimModelComboB.setSelectedId ((int) p->load() + 1, juce::dontSendNotification);
    mMicSimModelComboB.onChange = [this]()
    {
        const int idx = juce::jmax (0, mMicSimModelComboB.getSelectedId() - 1);
        beginParamUndoGesture (processor.apvts, "nam_micsim_b_model"); // Task 6 (12-iv)
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                          processor.apvts.getParameter ("nam_micsim_b_model")))
            p->setValueNotifyingHost (
                p->getNormalisableRange().convertTo0to1 ((float) idx));
        updateMicSimModelTooltipB();
    };
    updateMicSimModelTooltipB();

    addAndMakeVisible (mMicSimModelBLbl);
    mMicSimModelBLbl.setText ("Model", juce::dontSendNotification);
    mMicSimModelBLbl.setJustificationType (juce::Justification::centred);
    mMicSimModelBLbl.setColour (juce::Label::textColourId, juce::Colour (0xff909090));

    addAndMakeVisible (mMicSimUserIrBtnB);
    mMicSimUserIrBtnB.setButtonText ("Load Mic IR...");
    mMicSimUserIrBtnB.setTooltip ("Left-click: browse for a .wav mic IR for Mic B.  "
                                    "Right-click: clear the loaded IR.");
    mMicSimUserIrBtnB.onClick      = [this]() { browseForMicUserIrB(); };
    mMicSimUserIrBtnB.onRightClick = [this]()
    {
        processor.clearUserMicIrB();
        mMicSimUserIrLabelB.setText ("(no IR loaded)", juce::dontSendNotification);
    };
    addAndMakeVisible (mMicSimUserIrLabelB);
    mMicSimUserIrLabelB.setJustificationType (juce::Justification::centredLeft);
    mMicSimUserIrLabelB.setColour (juce::Label::textColourId, juce::Colour (kCabGreenARGB));
    mMicSimUserIrLabelB.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1a1a1a));
    mMicSimUserIrLabelB.setColour (juce::Label::outlineColourId,    juce::Colour (0xff333333));
    mMicSimUserIrLabelB.setBorderSize ({ 2, 6, 2, 6 });
    {
        const auto path = processor.getMicSimB().getUserIrPath();
        mMicSimUserIrLabelB.setText (path.isEmpty() ? "(no IR loaded)"
                                                     : ProjectFileResolver::resolve (path).getFileName(),
                                       juce::dontSendNotification);
    }

    addAndMakeVisible (mMicSimMixKnobB);
    mMicSimMixBAtt = std::make_unique<SliderAtt> (processor.apvts,
                                                    "nam_micsim_b_mix",
                                                    mMicSimMixKnobB.slider);

    addAndMakeVisible (mMicPlacementPolarB);
    mMicPlacementPolarB.setOptions ({
        { "O",    "Omni",          "Equal pickup all directions; no proximity effect" },
        { "Card", "Cardioid",      "Heart-shaped pattern; rejects rear" },
        { "Sup",  "Supercardioid", "Tighter pattern with small rear lobe" },
        { "Hyp",  "Hypercardioid", "Sharper still; pronounced side rejection" },
        { "8",    "Figure-8",      "Bidirectional; equal front + rear, side rejection" },
    });
    mMicPlacementPolarB.setBodyTooltip ("Mic B polar pattern");
    if (auto* p = processor.apvts.getRawParameterValue ("nam_placement_b_polar"))
        mMicPlacementPolarB.setSelectedIndex ((int) p->load(), juce::dontSendNotification);
    mMicPlacementPolarB.onChange = [this] (int idx)
    {
        beginParamUndoGesture (processor.apvts, "nam_placement_b_polar"); // Task 6 (12-iv)
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                          processor.apvts.getParameter ("nam_placement_b_polar")))
            p->setValueNotifyingHost (
                p->getNormalisableRange().convertTo0to1 ((float) idx));
    };
    addAndMakeVisible (mMicPlacementPolarBLbl);
    mMicPlacementPolarBLbl.setText ("Polar", juce::dontSendNotification);
    mMicPlacementPolarBLbl.setJustificationType (juce::Justification::centred);
    mMicPlacementPolarBLbl.setColour (juce::Label::textColourId, juce::Colour (0xff909090));

    addAndMakeVisible (mMicPlacementDistanceKnobB);
    addAndMakeVisible (mMicPlacementAngleKnobB);
    addAndMakeVisible (mMicPlacementMixKnobB);
    mMicPlacementDistanceBAtt = std::make_unique<SliderAtt> (processor.apvts,
                                                               "nam_placement_b_distance_cm",
                                                               mMicPlacementDistanceKnobB.slider);
    mMicPlacementAngleBAtt    = std::make_unique<SliderAtt> (processor.apvts,
                                                               "nam_placement_b_angle_deg",
                                                               mMicPlacementAngleKnobB.slider);
    mMicPlacementMixBAtt      = std::make_unique<SliderAtt> (processor.apvts,
                                                               "nam_placement_b_mix",
                                                               mMicPlacementMixKnobB.slider);

    updateMicSimModeBUI();
    updateMicBEnabled();

    // ── A/B + listener - initial sync ────────────────────────────────────────
    processor.apvts.addParameterListener ("ab_slot",       this);
    processor.apvts.addParameterListener ("oversampling",  this);
    processor.apvts.addParameterListener ("nam_micb_active", this);
    // QA-Fc: the mode/model/polar selectors are manual-sync (no APVTS
    // attachment exists for ChickenHeadSelector), so an A/B slot restore
    // that rewrites these params must push the new values back into the
    // widgets -- without these listeners the audio switches but the
    // selectors keep showing the outgoing slot's positions.
    processor.apvts.addParameterListener ("nam_micsim_mode",     this);
    processor.apvts.addParameterListener ("nam_micsim_model",    this);
    processor.apvts.addParameterListener ("nam_placement_polar", this);
    processor.apvts.addParameterListener ("nam_micsim_b_mode",     this);
    processor.apvts.addParameterListener ("nam_micsim_b_model",    this);
    processor.apvts.addParameterListener ("nam_placement_b_polar", this);

    const int slot0 = processor.getActiveSlot();
    mSlotABtn.setToggleState (slot0 == 0, juce::dontSendNotification);
    mSlotBBtn.setToggleState (slot0 == 1, juce::dontSendNotification);

    updateLabels();

    // 2026-05-05 (Bug C fix): subscribe to bulk-restore notifications so the
    // file-name labels refresh after page-preset load / project load.
    juce::Component::SafePointer<BaySickNAMIREditor> safeThis (this);
    processor.onStateRestored = [safeThis]
    {
        if (auto* e = safeThis.getComponent())
            e->updateLabels();
    };
}

BaySickNAMIREditor::~BaySickNAMIREditor()
{
    processor.apvts.removeParameterListener ("ab_slot",      this);
    processor.apvts.removeParameterListener ("oversampling", this);
    processor.apvts.removeParameterListener ("nam_micb_active",      this);
    processor.apvts.removeParameterListener ("nam_micsim_mode",      this);
    processor.apvts.removeParameterListener ("nam_micsim_model",     this);
    processor.apvts.removeParameterListener ("nam_placement_polar",  this);
    processor.apvts.removeParameterListener ("nam_micsim_b_mode",     this);
    processor.apvts.removeParameterListener ("nam_micsim_b_model",    this);
    processor.apvts.removeParameterListener ("nam_placement_b_polar", this);
    processor.onStateRestored = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Painting + layout.
// ─────────────────────────────────────────────────────────────────────────────
void BaySickNAMIREditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (kCabBgARGB));   // single dark faceplate

    // QA-A (2026-05-09): header strip + divider moved to BaySickTitleBar::paint().

    g.setColour (juce::Colour (kDividerARGB));
    g.fillRect (mMicColumnDivider);
}

void BaySickNAMIREditor::resized()
{
    // ── Header ───────────────────────────────────────────────────────────────
    // QA-A (2026-05-09): unified title bar.  QA-G3Smoke G-16: the A/B slot
    // toggles moved off the bar to below the OS dial (see the knob row).
    mTitleBar.setBounds (0, 0, getWidth(), BaySickTitleBar::kStandardHeight);

    // ── File rows ────────────────────────────────────────────────────────────
    constexpr int sectionLblW = 36;
    constexpr int browseBtnW  = 110;
    constexpr int bypassW     = 70;
    constexpr int rowGap      = 4;

    auto layoutFileRow = [&] (int y, juce::Label& sectionLbl, juce::Button& browseBtn,
                              juce::Label& fileLbl, DualLabelToggle& bypassTog)
    {
        const int textY = y + 14;
        const int textH = 22;
        sectionLbl.setBounds (kPad, y, sectionLblW, kFileRowH);
        browseBtn .setBounds (kPad + sectionLblW + rowGap, textY, browseBtnW, textH);
        const int fileLblX = kPad + sectionLblW + rowGap + browseBtnW + rowGap;
        const int fileLblW = getWidth() - fileLblX - bypassW - rowGap - kPad;
        fileLbl   .setBounds (fileLblX, textY, fileLblW, textH);
        bypassTog .setBounds (getWidth() - bypassW - kPad, y + 2, bypassW, kFileRowH - 4);
    };
    layoutFileRow (kHeaderH,                 mAmpSectionLbl, mNamBrowseBtn, mNamFileLabel, mNamBypassToggle);
    layoutFileRow (kHeaderH + kFileRowH,     mCabSectionLbl, mIrBrowseBtn,  mIrFileLabel,  mCabBypassToggle);

    // ── Knobs row ────────────────────────────────────────────────────────────
    const int knobsY     = kHeaderH + 2 * kFileRowH + 8;
    const int slotsCount = 8;          // 7 knobs + OS selector
    const int knobsAreaW = getWidth() - 2 * kPad;
    const int slotW      = knobsAreaW / slotsCount;

    int slotX = kPad;
    auto placeKnob = [&] (VKnob& k)
    {
        k.setBounds (slotX, knobsY, slotW, kKnobsRowH);
        slotX += slotW;
    };
    placeKnob (mInGainKnob);
    placeKnob (mGateThreshKnob);
    placeKnob (mGateReleaseKnob);
    placeKnob (mLowCutKnob);
    placeKnob (mHighCutKnob);
    placeKnob (mCabMixKnob);
    placeKnob (mOutputKnob);

    // OS chicken head - center the 66×66 dial inside the last slot, label below.
    const int dialSz   = 66;
    const int dialX    = slotX + (slotW - dialSz) / 2;
    const int dialY    = knobsY + 4;
    mOSSelector  .setBounds (dialX, dialY, dialSz, dialSz);
    mOSSelectorLbl.setBounds (slotX, dialY + dialSz + 4, slotW, 16);

    // QA-G3Smoke G-16: A/B slot toggles under the OS dial (moved off the title
    // bar).  Fits inside kKnobsRowH: 4+66 dial + 16 label + 2 + 20 = 108 < 130.
    const int abY = dialY + dialSz + 4 + 16 + 2;
    const int abX = slotX + (slotW - 2 * kSlotABtnW) / 2;
    mSlotABtn.setBounds (abX,              abY, kSlotABtnW, kSlotABtnH);
    mSlotBBtn.setBounds (abX + kSlotABtnW, abY, kSlotABtnW, kSlotABtnH);

    // ── Status row ───────────────────────────────────────────────────────────
    const int statusY = knobsY + kKnobsRowH + 8;
    mFullRigHint.setBounds (kPad, statusY, getWidth() / 2 - kPad, 22);
    mErrorLabel .setBounds (getWidth() / 2, statusY, getWidth() / 2 - kPad, 22);

    // ── QA-Fc Mic Sim + Mic Placement rows, split Mic A | Mic B ─────────────
    const int colW  = (getWidth() - 3 * kPad) / 2;
    const int colAx = kPad;
    const int colBx = kPad * 2 + colW;

    // Header band is 48 so the Mic B Active DualLabelToggle fits its
    // 12+1+22+1+12 Named layout without clipping the bottom label.
    const int micSimY = statusY + 26;
    const int simHdrH = 48;
    const int simRowY = micSimY + simHdrH;
    mMicSimSectionLbl .setBounds (colAx, micSimY + 15, 120, 18);
    mMicSimBSectionLbl.setBounds (colBx, micSimY + 15, 120, 18);
    mMicBActiveToggle .setBounds (colBx + colW - 70, micSimY, 70, simHdrH);

    auto layoutMicSimCol = [&] (int x, juce::Label& modeLbl, ChickenHeadSelector& mode,
                                juce::Label& modelLbl, juce::ComboBox& combo,
                                FilePickerButton& irBtn, juce::Label& irLbl, VKnob& mixKnob)
    {
        modeLbl.setBounds (x,      simRowY,      80, 14);
        mode   .setBounds (x,      simRowY + 14, 84, 56);
        modelLbl.setBounds (x + 96, simRowY,      150, 14);
        combo   .setBounds (x + 96, simRowY + 18, 150, 22);
        // The user-IR filename shares the combo's slot: the two are
        // mode-exclusive (Built-in shows the combo, User IR shows the label).
        irLbl   .setBounds (x + 96, simRowY + 18, 150, 22);
        irBtn   .setBounds (x + 96, simRowY + 44, 150, 22);
        mixKnob .setBounds (x + colW - 72, simRowY, 72, 70);
    };
    layoutMicSimCol (colAx, mMicSimModeLbl,  mMicSimMode,  mMicSimModelLbl,  mMicSimModelCombo,
                     mMicSimUserIrBtn,  mMicSimUserIrLabel,  mMicSimMixKnob);
    layoutMicSimCol (colBx, mMicSimModeBLbl, mMicSimModeB, mMicSimModelBLbl, mMicSimModelComboB,
                     mMicSimUserIrBtnB, mMicSimUserIrLabelB, mMicSimMixKnobB);

    const int placeY    = simRowY + 70 + 8;
    const int placeRowY = placeY + 22;
    mMicPlacementSectionLbl .setBounds (colAx, placeY, 160, 18);
    mMicPlacementBSectionLbl.setBounds (colBx, placeY, 160, 18);

    auto layoutPlacementCol = [&] (int x, juce::Label& polarLbl, ChickenHeadSelector& polar,
                                   VKnob& dist, VKnob& angle, VKnob& mix)
    {
        polarLbl.setBounds (x,       placeRowY,      104, 14);
        polar   .setBounds (x,       placeRowY + 14, 104, 56);
        dist    .setBounds (x + 116, placeRowY, 72, 70);
        angle   .setBounds (x + 196, placeRowY, 72, 70);
        mix     .setBounds (x + colW - 72, placeRowY, 72, 70);
    };
    layoutPlacementCol (colAx, mMicPlacementPolarLbl,  mMicPlacementPolar,
                        mMicPlacementDistanceKnob,  mMicPlacementAngleKnob,  mMicPlacementMixKnob);
    layoutPlacementCol (colBx, mMicPlacementPolarBLbl, mMicPlacementPolarB,
                        mMicPlacementDistanceKnobB, mMicPlacementAngleKnobB, mMicPlacementMixKnobB);

    mMicColumnDivider = { colAx + colW + kPad / 2 - 1, micSimY,
                          2, (placeRowY + 70) - micSimY };
}

// ─────────────────────────────────────────────────────────────────────────────
// H-6d Mic Sim helpers.
// ─────────────────────────────────────────────────────────────────────────────
void BaySickNAMIREditor::browseForMicUserIr()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Load Mic IR (.wav)",
        ensurePresetsDir (micIrPresetsDir()),
        "*.wav");

    auto flags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (flags, [this, chooser] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file == juce::File()) return;

        juce::String err;
        if (processor.loadUserMicIr (file, err))
        {
            mMicSimUserIrLabel.setText (file.getFileName(), juce::dontSendNotification);
            // Auto-switch the Mic Sim mode to User IR after a successful load
            beginParamUndoGesture (processor.apvts, "nam_micsim_mode"); // Task 6 (12-iv)
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                              processor.apvts.getParameter ("nam_micsim_mode")))
                p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (2.0f));
            mMicSimMode.setSelectedIndex (2, juce::dontSendNotification);
            updateMicSimModeUI();
        }
        else
        {
            showError (err);
        }
    });
}

void BaySickNAMIREditor::updateMicSimModeUI()
{
    const int mode = mMicSimMode.getSelectedIndex();
    // Mode 0 = None: hide model + user IR controls (still painted as disabled)
    // Mode 1 = Built-in: show model dropdown, hide user IR
    // Mode 2 = User IR: hide model dropdown, show user IR file picker
    const bool builtIn = (mode == 1);
    const bool userIr  = (mode == 2);
    mMicSimModelLbl   .setVisible (builtIn);
    mMicSimModelCombo .setVisible (builtIn);
    mMicSimUserIrBtn  .setVisible (userIr);
    mMicSimUserIrLabel.setVisible (userIr);
}

void BaySickNAMIREditor::updateMicSimModelTooltip()
{
    const int idx = juce::jmax (0, mMicSimModelCombo.getSelectedId() - 1);
    juce::String tip = juce::String ("Built-in mic archetype: ")
                        + MicSimDSP::modelDisplayName (idx)
                        + ".\nTypical use: "
                        + MicSimDSP::modelTypicalUse (idx);
    mMicSimModelCombo.setTooltip (tip);
}

// ─────────────────────────────────────────────────────────────────────────────
// QA-Fc Mic B helpers (mirror the Mic A set over the `_b_` params).
// ─────────────────────────────────────────────────────────────────────────────
void BaySickNAMIREditor::browseForMicUserIrB()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Load Mic B IR (.wav)",
        ensurePresetsDir (micIrPresetsDir()),
        "*.wav");

    auto flags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (flags, [this, chooser] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file == juce::File()) return;

        juce::String err;
        if (processor.loadUserMicIrB (file, err))
        {
            mMicSimUserIrLabelB.setText (file.getFileName(), juce::dontSendNotification);
            beginParamUndoGesture (processor.apvts, "nam_micsim_b_mode"); // Task 6 (12-iv)
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                              processor.apvts.getParameter ("nam_micsim_b_mode")))
                p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (2.0f));
            mMicSimModeB.setSelectedIndex (2, juce::dontSendNotification);
            updateMicSimModeBUI();
        }
        else
        {
            showError (err);
        }
    });
}

void BaySickNAMIREditor::updateMicSimModeBUI()
{
    const int mode = mMicSimModeB.getSelectedIndex();
    const bool builtIn = (mode == 1);
    const bool userIr  = (mode == 2);
    mMicSimModelBLbl   .setVisible (builtIn);
    mMicSimModelComboB .setVisible (builtIn);
    mMicSimUserIrBtnB  .setVisible (userIr);
    mMicSimUserIrLabelB.setVisible (userIr);
}

void BaySickNAMIREditor::updateMicSimModelTooltipB()
{
    const int idx = juce::jmax (0, mMicSimModelComboB.getSelectedId() - 1);
    juce::String tip = juce::String ("Built-in mic archetype: ")
                        + MicSimDSP::modelDisplayName (idx)
                        + ".\nTypical use: "
                        + MicSimDSP::modelTypicalUse (idx);
    mMicSimModelComboB.setTooltip (tip);
}

void BaySickNAMIREditor::updateMicBEnabled()
{
    bool on = false;
    if (auto* p = processor.apvts.getRawParameterValue ("nam_micb_active"))
        on = p->load() > 0.5f;

    const float alpha = on ? 1.0f : 0.4f;
    juce::Component* comps[] = { &mMicSimBSectionLbl, &mMicPlacementBSectionLbl,
                                 &mMicSimModeBLbl, &mMicSimModelBLbl, &mMicSimModelComboB,
                                 &mMicSimUserIrBtnB, &mMicSimUserIrLabelB, &mMicSimMixKnobB,
                                 &mMicPlacementPolarBLbl,
                                 &mMicPlacementDistanceKnobB, &mMicPlacementAngleKnobB,
                                 &mMicPlacementMixKnobB };
    for (auto* c : comps)
    {
        c->setEnabled (on);
        c->setAlpha (alpha);
    }
    // Selectors keep hover/tooltips while locked (setLocked greys + ignores
    // clicks) so the user can still read what the disabled controls are.
    mMicSimModeB       .setLocked (! on);
    mMicPlacementPolarB.setLocked (! on);
    mMicSimModeB       .setAlpha (alpha);
    mMicPlacementPolarB.setAlpha (alpha);
}

// ─────────────────────────────────────────────────────────────────────────────
// APVTS listener.
// ─────────────────────────────────────────────────────────────────────────────
void BaySickNAMIREditor::parameterChanged (const juce::String& paramID, float newValue)
{
    // SafePointer on every posted hop: the destructor removes the listeners,
    // but a lambda already queued can still run after the editor is torn down
    // (page delete / project-load param storms).
    juce::Component::SafePointer<BaySickNAMIREditor> self (this);

    if (paramID == "ab_slot")
    {
        const int slot = juce::jlimit (0, 1, (int) newValue);
        juce::MessageManager::callAsync ([self, slot]()
        {
            if (self == nullptr) return;
            self->mSlotABtn.setToggleState (slot == 0, juce::dontSendNotification);
            self->mSlotBBtn.setToggleState (slot == 1, juce::dontSendNotification);
            self->updateLabels();
        });
    }
    else if (paramID == "oversampling")
    {
        const int idx = juce::jlimit (0, 2, (int) newValue);
        juce::MessageManager::callAsync ([self, idx]()
        {
            if (self == nullptr) return;
            self->mOSSelector.setSelectedIndex (idx, juce::dontSendNotification);
        });
    }
    else if (paramID == "nam_micb_active")
    {
        juce::MessageManager::callAsync ([self]()
        {
            if (self != nullptr) self->updateMicBEnabled();
        });
    }
    // QA-Fc: manual-sync selector resync.  A/B slot restores rewrite these
    // params behind the widgets' backs (ChickenHeadSelector has no APVTS
    // attachment); push the new value back into the UI.
    else if (paramID == "nam_micsim_mode" || paramID == "nam_micsim_b_mode")
    {
        const bool isB = (paramID == "nam_micsim_b_mode");
        const int  idx = juce::jlimit (0, 2, (int) newValue);
        juce::MessageManager::callAsync ([self, isB, idx]()
        {
            if (self == nullptr) return;
            auto& sel = isB ? self->mMicSimModeB : self->mMicSimMode;
            if (sel.getSelectedIndex() != idx)
            {
                sel.setSelectedIndex (idx, juce::dontSendNotification);
                if (isB) self->updateMicSimModeBUI();
                else     self->updateMicSimModeUI();
            }
        });
    }
    else if (paramID == "nam_micsim_model" || paramID == "nam_micsim_b_model")
    {
        const bool isB = (paramID == "nam_micsim_b_model");
        const int  idx = juce::jlimit (0, (int) MicSimDSP::Model::kNumModels - 1,
                                          (int) newValue);
        juce::MessageManager::callAsync ([self, isB, idx]()
        {
            if (self == nullptr) return;
            auto& combo = isB ? self->mMicSimModelComboB : self->mMicSimModelCombo;
            if (combo.getSelectedId() != idx + 1)
            {
                combo.setSelectedId (idx + 1, juce::dontSendNotification);
                if (isB) self->updateMicSimModelTooltipB();
                else     self->updateMicSimModelTooltip();
            }
        });
    }
    else if (paramID == "nam_placement_polar" || paramID == "nam_placement_b_polar")
    {
        const bool isB = (paramID == "nam_placement_b_polar");
        const int  idx = juce::jlimit (0, 4, (int) newValue);
        juce::MessageManager::callAsync ([self, isB, idx]()
        {
            if (self == nullptr) return;
            auto& sel = isB ? self->mMicPlacementPolarB : self->mMicPlacementPolar;
            if (sel.getSelectedIndex() != idx)
                sel.setSelectedIndex (idx, juce::dontSendNotification);
        });
    }
}

void BaySickNAMIREditor::setActiveSlotFromUI (int slot)
{
    if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (
                        processor.apvts.getParameter ("ab_slot")))
        *prm = juce::jlimit (0, 1, slot);
    updateLabels();
}

void BaySickNAMIREditor::updateLabels()
{
    const int slot = processor.getActiveSlot();

    const juce::String namPath = processor.getNamFilePath (slot);
    const juce::String irPath  = processor.getIrFilePath  (slot);

    // The remembered path survives a failed load so the file can relink later,
    // so the LCDs read the loaded flags rather than the strings: never present a
    // name we did not actually load (same rule as NAMPedalStyleDSP::getModelName).
    const bool namLive = processor.hasNamModel (slot);
    const bool irLive  = processor.hasIr       (slot);

    const bool namMissing = namPath.isNotEmpty() && ! namLive;
    const bool irMissing  = irPath .isNotEmpty() && ! irLive;

    mNamFileLabel.setColour (juce::Label::textColourId,
                             juce::Colour (namMissing ? kErrARGB : kAmberARGB));
    mIrFileLabel .setColour (juce::Label::textColourId,
                             juce::Colour (irMissing  ? kErrARGB : kCabGreenARGB));

    juce::String namText ("(no model loaded)");
    if (namPath.isNotEmpty())
    {
        namText = juce::File (namPath).getFileName();
        if (namMissing) namText += " (missing)";
    }

    juce::String irText ("(no IR loaded)");
    if (irPath.isNotEmpty())
    {
        irText = juce::File (irPath).getFileName();
        if (irMissing) irText += " (missing)";
    }

    mNamFileLabel.setText (namText, juce::dontSendNotification);
    mIrFileLabel .setText (irText,  juce::dontSendNotification);

    if (processor.hasNamModel (slot) && processor.isFullRig (slot))
        mFullRigHint.setText (
            "Full-rig model - cabinet already included.  Consider bypassing the IR.",
            juce::dontSendNotification);
    else
        mFullRigHint.setText ({}, juce::dontSendNotification);

    // H-6d: per-slot Mic IR label tracks the active slot's loaded user IR.
    // RESOLVE before naming: getUserIrPath returns a persisted REFERENCE, and a
    // bare juce::File built from "mysamples:<rel>" is not a valid absolute path.
    const juce::String micPath = processor.getMicSim().getUserIrPath (slot);
    mMicSimUserIrLabel.setText (micPath.isEmpty()
                                    ? juce::String ("(no IR loaded)")
                                    : ProjectFileResolver::resolve (micPath).getFileName(),
                                  juce::dontSendNotification);

    const juce::String micPathB = processor.getMicSimB().getUserIrPath (slot);
    mMicSimUserIrLabelB.setText (micPathB.isEmpty()
                                     ? juce::String ("(no IR loaded)")
                                     : ProjectFileResolver::resolve (micPathB).getFileName(),
                                   juce::dontSendNotification);
}

// ─────────────────────────────────────────────────────────────────────────────
// File browse + drag-drop.
// ─────────────────────────────────────────────────────────────────────────────
void BaySickNAMIREditor::browseForNamFile()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Choose a .nam model",
        ensurePresetsDir (namPresetsDir()),
        "*.nam");

    auto flags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectFiles;
    chooser->launchAsync (flags, [this, chooser] (const juce::FileChooser& fc)
    {
        const juce::File f = fc.getResult();
        if (! f.existsAsFile()) return;
        juce::String err;
        if (processor.loadNamModel (f.getFullPathName(), err))
        {
            pushRecent ("nam", f.getFullPathName());
            mErrorLabel.setText ({}, juce::dontSendNotification);
            updateLabels();
        }
        else
        {
            mErrorLabel.setText (err, juce::dontSendNotification);
            showError ("Couldn't load NAM model:\n" + err);
        }
    });
}

void BaySickNAMIREditor::browseForIrFile()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Choose a .wav IR",
        ensurePresetsDir (irPresetsDir()),
        "*.wav");

    auto flags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectFiles;
    chooser->launchAsync (flags, [this, chooser] (const juce::FileChooser& fc)
    {
        const juce::File f = fc.getResult();
        if (! f.existsAsFile()) return;
        juce::String err;
        if (processor.loadImpulseResponse (f.getFullPathName(), err))
        {
            pushRecent ("ir", f.getFullPathName());
            mErrorLabel.setText ({}, juce::dontSendNotification);
            updateLabels();
        }
        else
        {
            mErrorLabel.setText (err, juce::dontSendNotification);
            showError ("Couldn't load IR:\n" + err);
        }
    });
}

void BaySickNAMIREditor::showError (const juce::String& msg)
{
    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                            "BaySickNAM/IR", msg, "OK");
}

bool BaySickNAMIREditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".nam") || f.endsWithIgnoreCase (".wav"))
            return true;
    return false;
}

void BaySickNAMIREditor::filesDropped (const juce::StringArray& files, int /*x*/, int /*y*/)
{
    juce::String lastErr;
    for (const auto& f : files)
    {
        if (f.endsWithIgnoreCase (".nam"))
        {
            juce::String err;
            if (processor.loadNamModel (f, err))
                pushRecent ("nam", f);
            else
                lastErr = err;
        }
        else if (f.endsWithIgnoreCase (".wav"))
        {
            juce::String err;
            if (processor.loadImpulseResponse (f, err))
                pushRecent ("ir", f);
            else
                lastErr = err;
        }
    }
    if (lastErr.isNotEmpty())
    {
        mErrorLabel.setText (lastErr, juce::dontSendNotification);
        showError ("Couldn't load dropped file:\n" + lastErr);
    }
    else
    {
        mErrorLabel.setText ({}, juce::dontSendNotification);
    }
    updateLabels();
}

// ─────────────────────────────────────────────────────────────────────────────
// Recent files menu.
// ─────────────────────────────────────────────────────────────────────────────
juce::File BaySickNAMIREditor::settingsFile()
{
    auto dir = AppPaths::appRoot();
    if (! dir.exists()) dir.createDirectory();
    return dir.getChildFile ("settings.xml");
}

juce::StringArray BaySickNAMIREditor::loadRecents (const juce::String& tag)
{
    juce::StringArray out;
    const juce::File f = settingsFile();
    if (! f.existsAsFile()) return out;

    if (auto xml = juce::XmlDocument::parse (f))
    {
        const juce::String childTag = (tag == "nam") ? "RecentNAMFiles" : "RecentIRFiles";
        if (auto* root = xml->getChildByName (childTag))
            for (auto* item = root->getChildByName ("File"); item != nullptr;
                 item = item->getNextElementWithTagName ("File"))
                out.add (item->getStringAttribute ("path"));
    }
    return out;
}

void BaySickNAMIREditor::saveRecents (const juce::String& tag, const juce::StringArray& list)
{
    const juce::File f = settingsFile();
    std::unique_ptr<juce::XmlElement> root;
    if (f.existsAsFile())
        root = juce::XmlDocument::parse (f);
    if (! root)
        root = std::make_unique<juce::XmlElement> ("Settings");

    const juce::String childTag = (tag == "nam") ? "RecentNAMFiles" : "RecentIRFiles";
    if (auto* existing = root->getChildByName (childTag))
        root->removeChildElement (existing, true);

    auto* section = root->createNewChildElement (childTag);
    for (const auto& p : list)
    {
        auto* item = section->createNewChildElement ("File");
        item->setAttribute ("path", p);
    }

    root->writeTo (f);
}

void BaySickNAMIREditor::pushRecent (const juce::String& tag, const juce::String& path)
{
    juce::StringArray list = loadRecents (tag);
    list.removeString (path, true);     // dedupe (case-insensitive on Windows is fine)
    list.insert (0, path);
    while (list.size() > 10) list.remove (list.size() - 1);
    saveRecents (tag, list);
}

void BaySickNAMIREditor::showRecentNamMenu()
{
    juce::PopupMenu m;
    auto recents = loadRecents ("nam");
    if (recents.isEmpty())
    {
        m.addItem (1, "(no recent .nam files)", false);
    }
    else
    {
        for (int i = 0; i < recents.size(); ++i)
            m.addItem (100 + i, juce::File (recents[i]).getFileName());
        m.addSeparator();
        m.addItem (1, "Clear recent");
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (mNamBrowseBtn),
        [this, recents] (int result)
        {
            if (result == 1)
            {
                saveRecents ("nam", {});
            }
            else if (result >= 100 && result < 100 + recents.size())
            {
                juce::String err;
                const juce::String path = recents[result - 100];
                if (processor.loadNamModel (path, err))
                {
                    pushRecent ("nam", path);
                    mErrorLabel.setText ({}, juce::dontSendNotification);
                    updateLabels();
                }
                else
                {
                    mErrorLabel.setText (err, juce::dontSendNotification);
                    showError ("Couldn't load NAM model:\n" + err);
                }
            }
        });
}

void BaySickNAMIREditor::showRecentIrMenu()
{
    juce::PopupMenu m;
    auto recents = loadRecents ("ir");
    if (recents.isEmpty())
    {
        m.addItem (1, "(no recent IRs)", false);
    }
    else
    {
        for (int i = 0; i < recents.size(); ++i)
            m.addItem (100 + i, juce::File (recents[i]).getFileName());
        m.addSeparator();
        m.addItem (1, "Clear recent");
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (mIrBrowseBtn),
        [this, recents] (int result)
        {
            if (result == 1)
            {
                saveRecents ("ir", {});
            }
            else if (result >= 100 && result < 100 + recents.size())
            {
                juce::String err;
                const juce::String path = recents[result - 100];
                if (processor.loadImpulseResponse (path, err))
                {
                    pushRecent ("ir", path);
                    mErrorLabel.setText ({}, juce::dontSendNotification);
                    updateLabels();
                }
                else
                {
                    mErrorLabel.setText (err, juce::dontSendNotification);
                    showError ("Couldn't load IR:\n" + err);
                }
            }
        });
}
