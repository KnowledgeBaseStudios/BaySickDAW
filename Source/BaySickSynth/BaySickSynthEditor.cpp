#include "BaySickSynthEditor.h"
#include "../AppPaths.h"
#include "../UserFileSave.h"

//==============================================================================
// ── Static UI helpers ─────────────────────────────────────────────────────────

void BaySickSynthEditor::initKnob (juce::Slider& s, const juce::String& tooltip)
{
    s.setSliderStyle      (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle     (juce::Slider::TextBoxBelow, false, 48, 14);
    s.setColour (juce::Slider::textBoxTextColourId,       juce::Colour (0xFFB0B0B0));
    s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xFF141618));
    s.setPopupDisplayEnabled (true, true, nullptr);
    if (tooltip.isNotEmpty()) s.setTooltip (tooltip);
}

void BaySickSynthEditor::initVSlider (juce::Slider& s, const juce::String& tooltip)
{
    s.setSliderStyle  (juce::Slider::LinearVertical);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 14);
    s.setColour (juce::Slider::textBoxTextColourId,       juce::Colour (0xFFB0B0B0));
    s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xFF141618));
    s.setPopupDisplayEnabled (true, true, nullptr);
    if (tooltip.isNotEmpty()) s.setTooltip (tooltip);
}

void BaySickSynthEditor::initLabel (juce::Label& l, const juce::String& text)
{
    l.setText              (text, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setColour            (juce::Label::textColourId, juce::Colour (0xFFB0B0B0));
    l.setFont              (juce::Font (10.0f));
}

void BaySickSynthEditor::initGroup (juce::GroupComponent& grp, const juce::String& name)
{
    grp.setText      (name);
    grp.setTextLabelPosition (juce::Justification::centredTop);
    grp.setColour (juce::GroupComponent::outlineColourId,   juce::Colour (0xFF2E3035));
    grp.setColour (juce::GroupComponent::textColourId,      juce::Colour (0xFF666870));
}

//==============================================================================
// ── BaySickSynthEditor ────────────────────────────────────────────────────────

BaySickSynthEditor::BaySickSynthEditor (BaySickSynthProcessor& p)
    : juce::AudioProcessorEditor (p), mProc (p)
{
    setLookAndFeel (&mSynthLAF);
    setSize (480, 440);

    auto& avts = p.apvts;
    auto  pid  = [&] (const char* s) { return p.pid (s); };
    const juce::Colour ledCol (BaySickSynthLAF::kGreen);

    // ── Header ────────────────────────────────────────────────────────────────
    // QA-A (2026-05-09): unified title bar.
    // QA-Layout T3: no internal header -- the hosting window's title strip
    // shows the name and mounts mPresetBtn (still owned + wired here).
    mPresetBtn.onClick = [this] { showPresetMenu(); };

    // ── Visualizer ────────────────────────────────────────────────────────────
    addAndMakeVisible (mVisualizer);
    mVisualizer.setup (avts, p.getParamPrefix());
    mVisualizer.setEffectiveLfoRateSource (p.getEffectiveLfoRatePtr());

    // ── Tab buttons ───────────────────────────────────────────────────────────
    for (int i = 0; i < 6; ++i)
    {
        mTabBtns[i].setButtonText (kTabNames[i]);
        mTabBtns[i].setClickingTogglesState (false);
        addAndMakeVisible (mTabBtns[i]);
        mTabBtns[i].onClick = [this, i] { setActiveTab (i); };
    }

    // ── OSC deck ──────────────────────────────────────────────────────────────
    initGroup (mWavGroup,      "WAVEFORM");
    initGroup (mVoiceGroup,    "VOICE MODE");
    initGroup (mModWheelGroup, "MOD WHEEL");
    addAndMakeVisible (mWavGroup);
    addAndMakeVisible (mVoiceGroup);
    addAndMakeVisible (mModWheelGroup);

    mWaveformCbo.addItemList ({
        "SAW","SAW+SAW","PULSE","SAW+SQUARE","SQUARE+SQUARE",
        "SUPERSAW","BELL","DEAF SAW","SPREAD OCT","SPREAD 5TH","SINE"
    }, 1);
    addAndMakeVisible (mWaveformCbo);

    mDualOscModeCbo.addItemList ({ "Musical", "Hz Offset", "Absolute Hz" }, 1);
    mDualOscModeCbo.setTooltip ("Dual-osc tuning mode (affects SAW+SAW / SAW+SQUARE / SQUARE+SQUARE only).\n"
                                "Musical: classic detune via Modifier knob.\n"
                                "Hz Offset: Modifier sets osc2 offset in Hz above osc1.\n"
                                "Absolute Hz: Modifier sets osc2 to a specific Hz (try 800 Hz with SQUARE+SQUARE for 808 cowbell).");
    addAndMakeVisible (mDualOscModeCbo);

    mOscSyncBtn.setClickingTogglesState (true);
    mOscSyncBtn.getProperties().set ("switchToggle", true);
    mOscSyncBtn.setTooltip ("Hard sync: osc2 phase restarts when osc1 completes a cycle.\n"
                            "Classic 80s sync-lead sound (Van Halen 'Jump', Final Countdown brass, Moog/ARP).\n"
                            "Active only for SAW+SAW / SAW+SQUARE / SQUARE+SQUARE waveforms.\n"
                            "Sweep osc2's pitch (Modifier with Hz Offset / Absolute Hz mode) for the screaming sync sweep.");
    addAndMakeVisible (mOscSyncBtn);

    mRingModBtn.setClickingTogglesState (true);
    mRingModBtn.getProperties().set ("switchToggle", true);
    mRingModBtn.setTooltip ("Ring modulation: sample = osc1 x osc2.\n"
                            "Metallic / bell / sci-fi character. CS-80 and ARP 2600 signature.\n"
                            "Active only for dual-osc waveforms. Try with Absolute Hz tuning for extreme inharmonics.\n"
                            "Combine with SYNC for further weirdness.");
    addAndMakeVisible (mRingModBtn);

    initKnob (mTransposeKnob, "Transpose (semitones)");
    initKnob (mModifierKnob,  "Modifier - PW / detune / FM depth / spread mix");
    initKnob (mNoiseKnob,     "Noise level mixed into oscillator");
    initKnob (mGlideKnob,     "Glide / portamento time");
    initKnob (mModWheelAmtKnob, "Mod wheel depth");
    initKnob  (mOutVolKnob,    "Master output level for this engine (0..1, default 0.8).");
    for (auto* s : { &mTransposeKnob, &mModifierKnob, &mNoiseKnob,
                     &mGlideKnob, &mOutVolKnob, &mModWheelAmtKnob })
        addAndMakeVisible (*s);

    initLabel (mTransposeLbl,   "TRANSPOSE"); addAndMakeVisible (mTransposeLbl);
    initLabel (mModifierLbl,    "MODIFIER");  addAndMakeVisible (mModifierLbl);
    initLabel (mNoiseLbl,       "NOISE");     addAndMakeVisible (mNoiseLbl);
    initLabel (mGlideLbl,       "SLIDE");     addAndMakeVisible (mGlideLbl);
    initLabel (mOutVolLbl,      "OUT VOL");   addAndMakeVisible (mOutVolLbl);
    initLabel (mModWheelAmtLbl, "AMOUNT");    addAndMakeVisible (mModWheelAmtLbl);

    mVoiceModeLed  = std::make_unique<BssLedRadio> (avts, pid ("voiceMode"),
                         juce::StringArray { "Poly", "Mono", "Lead", "Legato" }, 1, 4, ledCol);

    mCutSelfBtn.setClickingTogglesState (true);
    mCutSelfBtn.getProperties().set ("switchToggle", true);
    mCutSelfBtn.setTooltip ("Cut Self: when on, playing a note already held cuts the prior instance first.\n"
                            "Prevents phase stacking on rapid retrigs of the same note.\n"
                            "Active in Poly mode only (Mono/Lead cut inherently; Legato retargets).");
    addAndMakeVisible (mCutSelfBtn);

    // Cut Self mode toggle (QA-CutSelfReview): Same Pitch vs Cut All.  Label
    // follows the toggle state.  Only bites in Poly (Mono/Lead cut inherently).
    mCutSelfModeBtn.setClickingTogglesState (true);
    mCutSelfModeBtn.setTooltip ("Cut Self mode: Same Pitch cuts only the retriggered note (default);\n"
                                "Cut All cuts every ringing voice on each new note.");
    mCutSelfModeBtn.onStateChange = [this]
    { mCutSelfModeBtn.setButtonText (mCutSelfModeBtn.getToggleState() ? "CUT ALL" : "SAME PITCH"); };
    mCutSelfModeBtn.onStateChange();
    addAndMakeVisible (mCutSelfModeBtn);
    mModWheelDestLed = std::make_unique<BssLedRadio> (avts, pid ("modWheelDest"),
                         juce::StringArray { "Filter", "LFO" }, 1, 2, ledCol);
    addAndMakeVisible (*mVoiceModeLed);
    addAndMakeVisible (*mModWheelDestLed);

    // ── Amp Env deck ──────────────────────────────────────────────────────────
    initVSlider (mAmpASlider, "Amp Attack");
    initVSlider (mAmpDSlider, "Amp Decay");
    initVSlider (mAmpSSlider, "Amp Sustain");
    initVSlider (mAmpRSlider, "Amp Release");
    for (auto* s : { &mAmpASlider, &mAmpDSlider, &mAmpSSlider, &mAmpRSlider })
        addAndMakeVisible (*s);
    initLabel (mAmpALbl, "ATTACK");  addAndMakeVisible (mAmpALbl);
    initLabel (mAmpDLbl, "DECAY");   addAndMakeVisible (mAmpDLbl);
    initLabel (mAmpSLbl, "SUSTAIN"); addAndMakeVisible (mAmpSLbl);
    initLabel (mAmpRLbl, "RELEASE"); addAndMakeVisible (mAmpRLbl);

    initKnob  (mVelAmpKnob, "How much note velocity controls volume (0 = ignore velocity, 1 = full tracking)");
    addAndMakeVisible (mVelAmpKnob);
    initLabel (mVelAmpLbl,  "VEL");
    addAndMakeVisible (mVelAmpLbl);

    // Pitch envelope - second row on OSC ENV tab
    initVSlider (mPEnvASlider, "Pitch Env Attack");
    initVSlider (mPEnvDSlider, "Pitch Env Decay");
    initVSlider (mPEnvSSlider, "Pitch Env Sustain");
    initVSlider (mPEnvRSlider, "Pitch Env Release");
    for (auto* s : { &mPEnvASlider, &mPEnvDSlider, &mPEnvSSlider, &mPEnvRSlider })
        addAndMakeVisible (*s);
    initLabel (mPEnvALbl, "ATTACK");  addAndMakeVisible (mPEnvALbl);
    initLabel (mPEnvDLbl, "DECAY");   addAndMakeVisible (mPEnvDLbl);
    initLabel (mPEnvSLbl, "SUSTAIN"); addAndMakeVisible (mPEnvSLbl);
    initLabel (mPEnvRLbl, "RELEASE"); addAndMakeVisible (mPEnvRLbl);

    initKnob  (mPEnvAmtKnob, "Pitch envelope amount (semitones, bipolar; negative = pitch drops, positive = rises)");
    mPEnvAmtKnob.getProperties().set ("bipolar", true);
    addAndMakeVisible (mPEnvAmtKnob);
    initLabel (mPEnvAmtLbl, "AMOUNT");
    addAndMakeVisible (mPEnvAmtLbl);

    initGroup (mAmpEnvGroup,   "AMP ENV");
    initGroup (mPitchEnvGroup, "PITCH ENV");
    addAndMakeVisible (mAmpEnvGroup);
    addAndMakeVisible (mPitchEnvGroup);

    // ── Filter deck ───────────────────────────────────────────────────────────
    initGroup (mFilterTypeGroup,  "TYPE");
    initGroup (mFilterTrackGroup, "TRACKING");
    addAndMakeVisible (mFilterTypeGroup);
    addAndMakeVisible (mFilterTrackGroup);

    mFilterXYPad = std::make_unique<BssFilterXYPad> (avts, pid ("flt_cutoff"),
                                                      pid ("flt_res"), ledCol);
    addAndMakeVisible (*mFilterXYPad);

    mFilterTypeLed = std::make_unique<BssLedRadio> (avts, pid ("flt_type"),
                         juce::StringArray { "LP", "HP", "BP", "Notch" }, 1, 4, ledCol);
    addAndMakeVisible (*mFilterTypeLed);

    initKnob (mFltKbTrackKnob,  "Keyboard tracking amount");
    initKnob (mFltVelTrackKnob, "Velocity tracking amount");
    addAndMakeVisible (mFltKbTrackKnob);
    addAndMakeVisible (mFltVelTrackKnob);
    initLabel (mFltKbTrackLbl,  "KEYBOARD"); addAndMakeVisible (mFltKbTrackLbl);
    initLabel (mFltVelTrackLbl, "VELOCITY"); addAndMakeVisible (mFltVelTrackLbl);

    // ── Filter Env deck ───────────────────────────────────────────────────────
    initVSlider (mFltASlider, "Filter Env Attack");
    initVSlider (mFltDSlider, "Filter Env Decay");
    initVSlider (mFltSSlider, "Filter Env Sustain");
    initVSlider (mFltRSlider, "Filter Env Release");
    for (auto* s : { &mFltASlider, &mFltDSlider, &mFltSSlider, &mFltRSlider })
        addAndMakeVisible (*s);
    initLabel (mFltALbl, "ATTACK");  addAndMakeVisible (mFltALbl);
    initLabel (mFltDLbl, "DECAY");   addAndMakeVisible (mFltDLbl);
    initLabel (mFltSLbl, "SUSTAIN"); addAndMakeVisible (mFltSLbl);
    initLabel (mFltRLbl, "RELEASE"); addAndMakeVisible (mFltRLbl);

    initGroup (mFltAmtGroup, "AMOUNT");
    addAndMakeVisible (mFltAmtGroup);
    initKnob (mFltEnvAmtKnob, "Filter envelope amount (bipolar)");
    addAndMakeVisible (mFltEnvAmtKnob);
    initLabel (mFltEnvAmtLbl, "AMOUNT");
    addAndMakeVisible (mFltEnvAmtLbl);

    // ── LFO deck ──────────────────────────────────────────────────────────────
    initGroup (mLFOShapeGroup, "SHAPE");
    initGroup (mLFORateGroup,  "RATE");
    initGroup (mLFODestGroup,  "DEST");
    initGroup (mLFOAmtGroup,   "AMOUNT");
    addAndMakeVisible (mLFOShapeGroup);
    addAndMakeVisible (mLFORateGroup);
    addAndMakeVisible (mLFODestGroup);
    addAndMakeVisible (mLFOAmtGroup);

    mLFOShapeLed = std::make_unique<BssLedRadio> (avts, pid ("lfo_shape"),
                       juce::StringArray { "Sine", "Saw", "Square" }, 1, 3, juce::Colour (BaySickSynthLAF::kCyan));
    mLFODestLed  = std::make_unique<BssLedRadio> (avts, pid ("lfo_dest"),
                       juce::StringArray { "Filter", "Pitch", "Osc Mod" }, 1, 3, juce::Colour (BaySickSynthLAF::kCyan));
    addAndMakeVisible (*mLFOShapeLed);
    addAndMakeVisible (*mLFODestLed);

    initKnob (mLFORateKnob, "LFO rate (Hz)");
    initKnob (mLFOAmtKnob,  "LFO modulation amount");
    addAndMakeVisible (mLFORateKnob);
    addAndMakeVisible (mLFOAmtKnob);
    initLabel (mLFORateLbl, "RATE");   addAndMakeVisible (mLFORateLbl);
    initLabel (mLFOAmtLbl,  "AMOUNT"); addAndMakeVisible (mLFOAmtLbl);

    mLFOSyncBtn.setClickingTogglesState (true);
    mLFOSyncBtn.getProperties().set ("switchToggle", true);
    addAndMakeVisible (mLFOSyncBtn);

    mLFODivCbo.addItemList ({ "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" }, 1);
    mLFODivCbo.setTooltip ("LFO tempo-sync division (active when SYNC is on)");
    addAndMakeVisible (mLFODivCbo);

    // ── MOD deck ──────────────────────────────────────────────────────────────
    initGroup (mNoiseGroup, "NOISE");
    addAndMakeVisible (mNoiseGroup);

    mNoiseOnlyBtn.setClickingTogglesState (true);
    mNoiseOnlyBtn.getProperties().set ("switchToggle", true);
    mNoiseOnlyBtn.setTooltip ("When on, the oscillator is muted and noise is the sound source.\n"
                              "Use the NOISE knob on the OSC tab as the level.\n"
                              "Essential for snares, hi-hats, wind/ocean pads, breath attacks.");
    addAndMakeVisible (mNoiseOnlyBtn);

    mNoiseColorCbo.addItemList ({ "White", "Pink", "Brown" }, 1);
    mNoiseColorCbo.setTooltip ("Noise colour (affects the NOISE knob and Noise-Only mode).\n"
                               "White: flat spectrum - bright, hissy, classic analog noise.\n"
                               "Pink: -3 dB/oct - warmer, more musical. Authentic 808 snare, pad air.\n"
                               "Brown: -6 dB/oct - deep, rumbly. Ocean / wind / sub-rumble textures.");
    addAndMakeVisible (mNoiseColorCbo);

    initGroup (mTransientGroup, "TRANSIENT");
    addAndMakeVisible (mTransientGroup);
    initKnob (mTransAmtKnob, "Transient level at note onset (0 = off). Adds a short HPF'd noise 'click' before the envelope.");
    initKnob (mTransDurKnob, "Transient duration in milliseconds (0-20 ms).");
    initKnob (mTransColKnob, "Transient colour - HPF cutoff (200 Hz = thumpy, 10 kHz = bright click).");
    for (auto* s : { &mTransAmtKnob, &mTransDurKnob, &mTransColKnob })
        addAndMakeVisible (*s);
    initLabel (mTransAmtLbl, "AMT");    addAndMakeVisible (mTransAmtLbl);
    initLabel (mTransDurLbl, "DUR");    addAndMakeVisible (mTransDurLbl);
    initLabel (mTransColLbl, "COLOUR"); addAndMakeVisible (mTransColLbl);

    initGroup (mUnisonGroup, "UNISON");
    addAndMakeVisible (mUnisonGroup);
    initKnob (mUniVoicesKnob, "Unison voice count (1-7). 1 = off. 3-5 = classic supersaw stack.");
    initKnob (mUniDetuneKnob, "Unison detune amount (0 = in-tune, 1 = +/-50 cents spread).");
    initKnob (mUniSpreadKnob, "Unison stereo spread (0 = mono, 1 = voices spread hard L-R).");
    for (auto* s : { &mUniVoicesKnob, &mUniDetuneKnob, &mUniSpreadKnob })
        addAndMakeVisible (*s);
    initLabel (mUniVoicesLbl, "VOICES"); addAndMakeVisible (mUniVoicesLbl);
    initLabel (mUniDetuneLbl, "DETUNE"); addAndMakeVisible (mUniDetuneLbl);
    initLabel (mUniSpreadLbl, "SPREAD"); addAndMakeVisible (mUniSpreadLbl);

    initGroup (mDriftGroup, "DRIFT");
    addAndMakeVisible (mDriftGroup);
    initKnob  (mDriftKnob, "Analog drift - slow per-voice pitch wander (up to +/-10 cents at max).\n"
                           "Default 0 = digitally perfect. Raise for Juno/Prophet/CS-80 analog warmth.\n"
                           "Most audible on sustained chords and pads (each voice drifts independently).");
    addAndMakeVisible (mDriftKnob);
    initLabel (mDriftLbl, "DRIFT");
    addAndMakeVisible (mDriftLbl);

    initGroup (mBurstGroup, "BURST ENV");
    addAndMakeVisible (mBurstGroup);
    mBurstModeBtn.setClickingTogglesState (true);
    mBurstModeBtn.getProperties().set ("switchToggle", true);
    mBurstModeBtn.setTooltip ("Multi-burst envelope: fires N quick amplitude pulses at noteOn,\n"
                              "modulating the amp envelope. 808 handclap: 4 bursts @ 20 ms.");
    addAndMakeVisible (mBurstModeBtn);
    initKnob (mBurstCountKnob,   "Number of bursts to fire at noteOn (1-8).");
    initKnob (mBurstSpacingKnob, "Spacing between bursts in milliseconds (1-100).");
    addAndMakeVisible (mBurstCountKnob);
    addAndMakeVisible (mBurstSpacingKnob);
    initLabel (mBurstCountLbl,   "COUNT");   addAndMakeVisible (mBurstCountLbl);
    initLabel (mBurstSpacingLbl, "SPACING"); addAndMakeVisible (mBurstSpacingLbl);

    // ── APVTS Attachments ─────────────────────────────────────────────────────
    // OSC
    mWaveformAtt    = std::make_unique<ComboAtt>  (avts, pid ("waveform"),    mWaveformCbo);
    mDualOscModeAtt = std::make_unique<ComboAtt>  (avts, pid ("dualOscMode"), mDualOscModeCbo);
    mOscSyncAtt     = std::make_unique<ButtonAtt> (avts, pid ("oscSync"),     mOscSyncBtn);
    mRingModAtt     = std::make_unique<ButtonAtt> (avts, pid ("ringMod"),     mRingModBtn);
    mTransposeAtt   = std::make_unique<SliderAtt> (avts, pid ("transpose"),   mTransposeKnob);
    mModifierAtt    = std::make_unique<SliderAtt> (avts, pid ("modifier"),    mModifierKnob);
    mNoiseAtt       = std::make_unique<SliderAtt> (avts, pid ("noise"),       mNoiseKnob);
    mGlideAtt       = std::make_unique<SliderAtt> (avts, pid ("glide"),       mGlideKnob);
    mOutVolAtt      = std::make_unique<SliderAtt> (avts, pid ("outVol"),      mOutVolKnob);
    mModWheelAmtAtt = std::make_unique<SliderAtt> (avts, pid ("modWheelAmt"), mModWheelAmtKnob);
    mCutSelfAtt     = std::make_unique<ButtonAtt> (avts, pid ("cutSelf"),     mCutSelfBtn);
    mCutSelfModeAtt = std::make_unique<ButtonAtt> (avts, pid ("cutSelfMode"), mCutSelfModeBtn);
    // Amp Env
    mAmpAAtt   = std::make_unique<SliderAtt> (avts, pid ("amp_attack"),   mAmpASlider);
    mAmpDAtt   = std::make_unique<SliderAtt> (avts, pid ("amp_decay"),    mAmpDSlider);
    mAmpSAtt   = std::make_unique<SliderAtt> (avts, pid ("amp_sustain"),  mAmpSSlider);
    mAmpRAtt   = std::make_unique<SliderAtt> (avts, pid ("amp_release"),  mAmpRSlider);
    mVelAmpAtt = std::make_unique<SliderAtt> (avts, pid ("velAmpTrack"), mVelAmpKnob);
    // Pitch envelope
    mPEnvAAtt   = std::make_unique<SliderAtt> (avts, pid ("pEnv_attack"),  mPEnvASlider);
    mPEnvDAtt   = std::make_unique<SliderAtt> (avts, pid ("pEnv_decay"),   mPEnvDSlider);
    mPEnvSAtt   = std::make_unique<SliderAtt> (avts, pid ("pEnv_sustain"), mPEnvSSlider);
    mPEnvRAtt   = std::make_unique<SliderAtt> (avts, pid ("pEnv_release"), mPEnvRSlider);
    mPEnvAmtAtt = std::make_unique<SliderAtt> (avts, pid ("pEnv_amt"),     mPEnvAmtKnob);
    // Filter tracking
    mFltKbTrackAtt  = std::make_unique<SliderAtt> (avts, pid ("flt_kbtrack"),  mFltKbTrackKnob);
    mFltVelTrackAtt = std::make_unique<SliderAtt> (avts, pid ("flt_veltrack"), mFltVelTrackKnob);
    // Filter Env
    mFltAAtt      = std::make_unique<SliderAtt> (avts, pid ("flt_attack"),  mFltASlider);
    mFltDAtt      = std::make_unique<SliderAtt> (avts, pid ("flt_decay"),   mFltDSlider);
    mFltSAtt      = std::make_unique<SliderAtt> (avts, pid ("flt_sustain"), mFltSSlider);
    mFltRAtt      = std::make_unique<SliderAtt> (avts, pid ("flt_release"), mFltRSlider);
    mFltEnvAmtAtt = std::make_unique<SliderAtt> (avts, pid ("flt_env_amt"), mFltEnvAmtKnob);
    // LFO
    mLFOSyncAtt = std::make_unique<ButtonAtt> (avts, pid ("lfo_sync"),     mLFOSyncBtn);
    mLFODivAtt  = std::make_unique<ComboAtt>  (avts, pid ("lfo_division"), mLFODivCbo);
    mLFORateAtt = std::make_unique<SliderAtt> (avts, pid ("lfo_rate"),     mLFORateKnob);
    mLFOAmtAtt  = std::make_unique<SliderAtt> (avts, pid ("lfo_amount"),   mLFOAmtKnob);

    // MOD deck
    mNoiseOnlyAtt  = std::make_unique<ButtonAtt> (avts, pid ("noiseOnly"),   mNoiseOnlyBtn);
    mNoiseColorAtt = std::make_unique<ComboAtt>  (avts, pid ("noiseColor"),  mNoiseColorCbo);
    mTransAmtAtt  = std::make_unique<SliderAtt> (avts, pid ("trans_amount"),   mTransAmtKnob);
    mTransDurAtt  = std::make_unique<SliderAtt> (avts, pid ("trans_duration"), mTransDurKnob);
    mTransColAtt  = std::make_unique<SliderAtt> (avts, pid ("trans_colour"),   mTransColKnob);
    mBurstModeAtt    = std::make_unique<ButtonAtt> (avts, pid ("burst_mode"),    mBurstModeBtn);
    mBurstCountAtt   = std::make_unique<SliderAtt> (avts, pid ("burst_count"),   mBurstCountKnob);
    mBurstSpacingAtt = std::make_unique<SliderAtt> (avts, pid ("burst_spacing"), mBurstSpacingKnob);
    mDriftAtt        = std::make_unique<SliderAtt> (avts, pid ("drift"),         mDriftKnob);
    mUniVoicesAtt    = std::make_unique<SliderAtt> (avts, pid ("unison_voices"), mUniVoicesKnob);
    mUniDetuneAtt    = std::make_unique<SliderAtt> (avts, pid ("unison_detune"), mUniDetuneKnob);
    mUniSpreadAtt    = std::make_unique<SliderAtt> (avts, pid ("unison_spread"), mUniSpreadKnob);

    // ── componentID pass ──────────────────────────────────────────────────────
    //   Each attached slider gets its full APVTS paramID as its componentID so
    //   GlobalAutoRightClick + VKnobAutomation resolve the right-click Automate
    //   and Type-in-value menus. Pattern mirrors VibePlayer (see VibePlayerEditor:203).
    // QA-ApvtsAutomation: the stamped id is also the automation registry key.
    // QA-ModelShell TS3: the applicator behind that key is registered model-side
    // when the engine is created, so this pass stamps and nothing else.
    auto wireID = [&p] (juce::Slider& s, const char* paramName)
    {
        const juce::String id = p.pid (paramName);
        s.setComponentID (id);
    };
    wireID (mTransposeKnob,   "transpose");
    wireID (mModifierKnob,    "modifier");
    wireID (mNoiseKnob,       "noise");
    wireID (mGlideKnob,       "glide");
    wireID (mOutVolKnob,      "outVol");
    wireID (mModWheelAmtKnob, "modWheelAmt");
    wireID (mAmpASlider,      "amp_attack");
    wireID (mAmpDSlider,      "amp_decay");
    wireID (mAmpSSlider,      "amp_sustain");
    wireID (mAmpRSlider,      "amp_release");
    wireID (mVelAmpKnob,      "velAmpTrack");
    wireID (mPEnvASlider,     "pEnv_attack");
    wireID (mPEnvDSlider,     "pEnv_decay");
    wireID (mPEnvSSlider,     "pEnv_sustain");
    wireID (mPEnvRSlider,     "pEnv_release");
    wireID (mPEnvAmtKnob,     "pEnv_amt");
    wireID (mTransAmtKnob,    "trans_amount");
    wireID (mTransDurKnob,    "trans_duration");
    wireID (mTransColKnob,    "trans_colour");
    wireID (mBurstCountKnob,  "burst_count");
    wireID (mBurstSpacingKnob,"burst_spacing");
    wireID (mDriftKnob,       "drift");
    wireID (mUniVoicesKnob,   "unison_voices");
    wireID (mUniDetuneKnob,   "unison_detune");
    wireID (mUniSpreadKnob,   "unison_spread");
    wireID (mFltKbTrackKnob,  "flt_kbtrack");
    wireID (mFltVelTrackKnob, "flt_veltrack");
    wireID (mFltASlider,      "flt_attack");
    wireID (mFltDSlider,      "flt_decay");
    wireID (mFltSSlider,      "flt_sustain");
    wireID (mFltRSlider,      "flt_release");
    wireID (mFltEnvAmtKnob,   "flt_env_amt");
    wireID (mLFORateKnob,     "lfo_rate");
    wireID (mLFOAmtKnob,      "lfo_amount");

    // Grey rate knob when sync is on, grey div combo when sync is off
    avts.addParameterListener (pid ("lfo_sync"), this);
    refreshLFOSyncEnableState();

    // Modifier tooltip tracks the dual-osc tuning mode
    avts.addParameterListener (pid ("dualOscMode"), this);
    refreshModifierTooltip();

    setActiveTab (0);

    // Slider double-click returns each param's FACTORY default (the helper reads
    // getDefaultValue) -- standard knob behavior, independent of the loaded patch.
    setSliderDoubleClickDefaultsFromApvts (*this, mProc.apvts);
    mProc.apvts.state.addListener (this);
}

BaySickSynthEditor::~BaySickSynthEditor()
{
    mProc.apvts.state.removeListener (this);
    mProc.apvts.removeParameterListener (mProc.pid ("lfo_sync"),    this);
    mProc.apvts.removeParameterListener (mProc.pid ("dualOscMode"), this);
    setLookAndFeel (nullptr);
}

void BaySickSynthEditor::valueTreeRedirected (juce::ValueTree& tree)
{
    if (tree != mProc.apvts.state) return;
    setSliderDoubleClickDefaultsFromApvts (*this, mProc.apvts);
}

void BaySickSynthEditor::parameterChanged (const juce::String& paramID, float /*newValue*/)
{
    // SafePointer on every posted hop: the destructor removes the listeners, but a
    // lambda already queued cannot be retracted, and this listener runs on whatever
    // thread wrote the parameter (an offline render thread replaying an automation
    // lane), so the post can outlive the editor.
    juce::Component::SafePointer<BaySickSynthEditor> self (this);

    if (paramID == mProc.pid ("lfo_sync"))
        juce::MessageManager::callAsync ([self]
        {
            if (self == nullptr) return;
            self->refreshLFOSyncEnableState();
        });
    else if (paramID == mProc.pid ("dualOscMode"))
        juce::MessageManager::callAsync ([self]
        {
            if (self == nullptr) return;
            self->refreshModifierTooltip();
        });
}

void BaySickSynthEditor::refreshModifierTooltip()
{
    int mode = 0;
    if (auto* p = mProc.apvts.getRawParameterValue (mProc.pid ("dualOscMode")))
        mode = (int) p->load();

    switch (mode)
    {
        case 1: // Hz Offset: 0..2000 Hz
            mModifierKnob.setTooltip (
                "Modifier - Hz offset added to osc2 above osc1 (0 to 2000 Hz).\n"
                "Active for SAW+SAW / SAW+SQUARE / SQUARE+SQUARE waveforms.");
            mModifierKnob.textFromValueFunction = [] (double v)
            {
                return juce::String ((int) std::round (v * 2000.0)) + " Hz";
            };
            mModifierKnob.valueFromTextFunction = [] (const juce::String& t)
            {
                return juce::jlimit (0.0, 1.0, t.getDoubleValue() / 2000.0);
            };
            break;

        case 2: // Absolute Hz: 20..20000 Hz log
            mModifierKnob.setTooltip (
                "Modifier - absolute frequency for osc2 (20 Hz to 20 kHz, log scale).\n"
                "Active for SAW+SAW / SAW+SQUARE / SQUARE+SQUARE waveforms.\n"
                "Classic 808 cowbell: SQUARE+SQUARE, osc2 at ~800 Hz, play A4.");
            mModifierKnob.textFromValueFunction = [] (double v)
            {
                const double hz = 20.0 * std::pow (1000.0, v);
                if (hz >= 1000.0)
                    return juce::String (hz / 1000.0, 2) + " kHz";
                return juce::String ((int) std::round (hz)) + " Hz";
            };
            mModifierKnob.valueFromTextFunction = [] (const juce::String& t)
            {
                double hz = t.getDoubleValue();
                if (t.containsIgnoreCase ("k")) hz *= 1000.0;
                hz = juce::jlimit (20.0, 20000.0, hz);
                return std::log (hz / 20.0) / std::log (1000.0);
            };
            break;

        default: // Musical (0-1 value)
            mModifierKnob.setTooltip (
                "Modifier - context-dependent: pulse width (PULSE), detune (dual-osc),\n"
                "FM depth (BELL), spread mix (SPREAD OCT / SPREAD 5TH).");
            mModifierKnob.textFromValueFunction = nullptr;   // default 0..1 display
            mModifierKnob.valueFromTextFunction = nullptr;
            break;
    }
    mModifierKnob.updateText();
    mModifierKnob.repaint();
}

void BaySickSynthEditor::refreshLFOSyncEnableState()
{
    bool syncOn = false;
    if (auto* p = mProc.apvts.getRawParameterValue (mProc.pid ("lfo_sync")))
        syncOn = p->load() > 0.5f;

    mLFORateKnob.setEnabled (! syncOn);
    mLFODivCbo  .setEnabled (  syncOn);
}

//==============================================================================
void BaySickSynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (BaySickSynthLAF::kBgMain));

    const int deckTop = 32 + 120 + 30;
    g.setColour (juce::Colour (0xFF17191C));
    g.fillRect  (0, deckTop, getWidth(), getHeight() - deckTop);

    g.setColour (juce::Colour (0xFF3A3C40));
    g.drawHorizontalLine (deckTop, 0.f, (float) getWidth());
}

//==============================================================================
void BaySickSynthEditor::resized()
{
    const int w = getWidth();

    // ── Visualizer (120px) ────────────────────────────────────────────────────
    // QA-Layout T3: the internal 32px title bar is gone -- content starts at 0.
    mVisualizer.setBounds (0, 0, w, 120);

    // ── Tab row (30px) ────────────────────────────────────────────────────────
    const int kTabTop = 120;
    const int kTabW   = w / 6;
    for (int i = 0; i < 6; ++i)
        mTabBtns[i].setBounds (i * kTabW, kTabTop, kTabW, 30);

    // ── Control deck ──────────────────────────────────────────────────────────
    const int kDeckTop = kTabTop + 30;
    const juce::Rectangle<int> deck (0, kDeckTop, w, getHeight() - kDeckTop);

    layoutOscDeck    (deck);
    layoutAmpEnvDeck (deck);
    layoutFilterDeck (deck);
    layoutFltEnvDeck (deck);
    layoutLFODeck    (deck);
    layoutModDeck    (deck);
}

//==============================================================================
// ── Per-deck layouts ──────────────────────────────────────────────────────────

static constexpr int kLblH = 14;
static constexpr int kPad  = 5;
static constexpr int kGrpTop = 16; // space for GroupComponent title
static constexpr int kKnobSz = 80; // target knob size (2026-04-22 layout polish)

void BaySickSynthEditor::layoutOscDeck (juce::Rectangle<int> deck)
{
    // Three equal columns: Waveform | Voice Mode | Mod Wheel
    const int colW = deck.getWidth() / 3;

    // ── Column 1: Waveform group ──────────────────────────────────────────────
    auto col1 = deck.withWidth (colW).reduced (kPad);
    mWavGroup.setBounds (col1);

    auto wInner = col1.reduced (4, kGrpTop);
    const int cboH    = 24;
    const int dualCboH = 20;
    const int gap     = 3;
    const int syncBtnH = 20;
    mWaveformCbo    .setBounds (wInner.getX(), wInner.getY(),                 wInner.getWidth(), cboH);
    mDualOscModeCbo .setBounds (wInner.getX(), wInner.getY() + cboH + gap,    wInner.getWidth(), dualCboH);

    // SYNC + RING toggles share the bottom row
    const int halfW = wInner.getWidth() / 2;
    mOscSyncBtn.setBounds (wInner.getX(),
                           wInner.getBottom() - syncBtnH,
                           halfW - 2,
                           syncBtnH);
    mRingModBtn.setBounds (wInner.getX() + halfW + 2,
                           wInner.getBottom() - syncBtnH,
                           wInner.getWidth() - halfW - 2,
                           syncBtnH);

    // Three knobs between the top ComboBoxes and the bottom SYNC toggle
    const int stackedH = cboH + gap + dualCboH + kPad;
    const int knobArea = wInner.getHeight() - stackedH - syncBtnH - gap;
    const int kW3 = wInner.getWidth() / 3;
    auto placeKnob = [&] (juce::Slider& s, juce::Label& l, int col)
    {
        const int x   = wInner.getX() + col * kW3;
        const int kh  = juce::jmin (kKnobSz, knobArea - kLblH - 2);
        const int y   = wInner.getY() + stackedH + ((knobArea - kLblH - 2) - kh) / 2;
        s.setBounds (x, y, kW3, kh);
        l.setBounds (x, y + kh, kW3, kLblH);
    };
    placeKnob (mTransposeKnob, mTransposeLbl, 0);
    placeKnob (mModifierKnob,  mModifierLbl,  1);
    placeKnob (mNoiseKnob,     mNoiseLbl,     2);

    // ── Column 2: Voice Mode group ────────────────────────────────────────────
    auto col2 = deck.withX (deck.getX() + colW).withWidth (colW).reduced (kPad);
    mVoiceGroup.setBounds (col2);

    auto vInner = col2.reduced (4, kGrpTop);
    const int ledH   = 26;    // 1×4 button row + padding
    const int cutH   = 22;
    const int vGap   = 4;
    if (mVoiceModeLed) mVoiceModeLed->setBounds (vInner.getX(), vInner.getY(), vInner.getWidth(), ledH);

    // Cut Self button (half width) + Cut Self mode toggle (Same Pitch / Cut All).
    const int cutY     = vInner.getY() + ledH + vGap;
    const int cutHalfW = (vInner.getWidth() - vGap) / 2;
    mCutSelfBtn.setBounds     (vInner.getX(),                   cutY, cutHalfW, cutH);
    mCutSelfModeBtn.setBounds (vInner.getX() + cutHalfW + vGap, cutY, vInner.getWidth() - cutHalfW - vGap, cutH);

    // Slide + Out Vol - split the remaining vertical space horizontally.
    // Two knobs side-by-side, each centered in its half-column with its own label.
    const int knobsTop = vInner.getY() + ledH + vGap + cutH + vGap;
    const int knobsH   = vInner.getBottom() - knobsTop;
    const int vmHalfW  = vInner.getWidth() / 2;
    const int kSlot    = juce::jmin (kKnobSz, juce::jmin (vmHalfW, knobsH - kLblH));

    const int slideX  = vInner.getX() + (vmHalfW - kSlot) / 2;
    const int outVolX = vInner.getX() + vmHalfW + (vmHalfW - kSlot) / 2;
    const int knobsY  = knobsTop + ((knobsH - kLblH) - kSlot) / 2;

    mGlideKnob .setBounds (slideX,  knobsY, kSlot, kSlot);
    mOutVolKnob.setBounds (outVolX, knobsY, kSlot, kSlot);
    mGlideLbl .setBounds (vInner.getX(),           vInner.getBottom() - kLblH, vmHalfW, kLblH);
    mOutVolLbl.setBounds (vInner.getX() + vmHalfW, vInner.getBottom() - kLblH, vmHalfW, kLblH);

    // ── Column 3: Mod Wheel group ─────────────────────────────────────────────
    auto col3 = deck.withX (deck.getX() + colW * 2).withWidth (colW).reduced (kPad);
    mModWheelGroup.setBounds (col3);

    auto mwInner = col3.reduced (4, kGrpTop);
    // ModWheel LED is now a horizontal 1×2 grid (Filter | LFO)
    const int mwLedH = 26;
    if (mModWheelDestLed) mModWheelDestLed->setBounds (mwInner.getX(), mwInner.getY(), mwInner.getWidth(), mwLedH);

    const int amtH    = mwInner.getHeight() - mwLedH - kPad;
    const int amtY    = mwInner.getBottom() - amtH;
    const int aKnobH  = juce::jmin (kKnobSz, amtH - kLblH);
    const int aKnobX  = mwInner.getX() + (mwInner.getWidth() - aKnobH) / 2;
    const int aKnobY  = amtY + ((amtH - kLblH) - aKnobH) / 2;
    mModWheelAmtKnob.setBounds (aKnobX, aKnobY, aKnobH, aKnobH);
    mModWheelAmtLbl .setBounds (mwInner.getX(), amtY + amtH - kLblH, mwInner.getWidth(), kLblH);
}

void BaySickSynthEditor::layoutAmpEnvDeck (juce::Rectangle<int> deck)
{
    // Side-by-side boxes (2026-04-22 layout polish):
    //   Left half: AMP ENV (4 ADSR verticals + VEL knob)
    //   Right half: PITCH ENV (4 ADSR verticals + AMOUNT knob)
    const int halfW = deck.getWidth() / 2;
    const int lH    = kLblH + 18;

    auto layoutBox = [&] (juce::Rectangle<int> boxArea,
                          juce::GroupComponent& group,
                          juce::Slider* sliders[4], juce::Label* labels[4],
                          juce::Slider& extraKnob, juce::Label& extraLbl)
    {
        group.setBounds (boxArea);
        auto inner  = boxArea.reduced (4, kGrpTop);
        const int knobW  = juce::jmin (kKnobSz + 8, inner.getWidth() / 5);
        const int sW     = (inner.getWidth() - knobW) / 4;

        for (int i = 0; i < 4; ++i)
        {
            auto r = inner.withX (inner.getX() + i * sW).withWidth (sW).reduced (kPad / 2);
            sliders[i]->setBounds (r.getX(), r.getY(), r.getWidth(), r.getHeight() - lH);
            labels [i]->setBounds (r.getX(), r.getBottom() - kLblH, r.getWidth(), kLblH);
        }
        auto knobArea = inner.withX (inner.getRight() - knobW).withWidth (knobW).reduced (kPad / 2);
        const int kh = juce::jmin (kKnobSz, knobArea.getHeight() - kLblH - 2);
        const int kY = knobArea.getY() + ((knobArea.getHeight() - kLblH - 2) - kh) / 2;
        extraKnob.setBounds (knobArea.getX(), kY, knobArea.getWidth(), kh);
        extraLbl .setBounds (knobArea.getX(), knobArea.getBottom() - kLblH, knobArea.getWidth(), kLblH);
    };

    juce::Slider* ampSliders[] = { &mAmpASlider, &mAmpDSlider, &mAmpSSlider, &mAmpRSlider };
    juce::Label*  ampLabels[]  = { &mAmpALbl,    &mAmpDLbl,    &mAmpSLbl,    &mAmpRLbl    };
    juce::Slider* pSliders[]   = { &mPEnvASlider, &mPEnvDSlider, &mPEnvSSlider, &mPEnvRSlider };
    juce::Label*  pLabels[]    = { &mPEnvALbl,    &mPEnvDLbl,    &mPEnvSLbl,    &mPEnvRLbl    };

    auto ampBox   = deck.withWidth (halfW).reduced (kPad);
    auto pitchBox = deck.withTrimmedLeft (halfW).reduced (kPad);

    layoutBox (ampBox,   mAmpEnvGroup,   ampSliders, ampLabels, mVelAmpKnob, mVelAmpLbl);
    layoutBox (pitchBox, mPitchEnvGroup, pSliders,   pLabels,   mPEnvAmtKnob, mPEnvAmtLbl);
}

void BaySickSynthEditor::layoutFilterDeck (juce::Rectangle<int> deck)
{
    // XY pad (50%) on the left, right half split vertically:
    //   TYPE group on top (wider, button strip)
    //   TRACKING group below (2 knobs)
    const int xyW    = deck.getWidth() * 50 / 100;
    const int typeH  = deck.getHeight() * 45 / 100;

    auto xyArea    = deck.withWidth (xyW).reduced (kPad);
    auto rightCol  = deck.withX (deck.getX() + xyW).withWidth (deck.getWidth() - xyW);
    auto typeArea  = rightCol.withHeight (typeH).reduced (kPad);
    auto trackArea = rightCol.withTrimmedTop (typeH).reduced (kPad);

    if (mFilterXYPad) mFilterXYPad->setBounds (xyArea);

    // TYPE group (LP / HP / BP / Notch)
    mFilterTypeGroup.setBounds (typeArea);
    auto typeInner = typeArea.reduced (4, kGrpTop);
    if (mFilterTypeLed) mFilterTypeLed->setBounds (typeInner);

    // TRACKING group (Keyboard + Velocity knobs)
    mFilterTrackGroup.setBounds (trackArea);
    auto tInner = trackArea.reduced (4, kGrpTop);
    const int kW2 = tInner.getWidth() / 2;
    auto placeKnob = [&] (juce::Slider& s, juce::Label& l, int col)
    {
        auto r = tInner.withX (tInner.getX() + col * kW2).withWidth (kW2);
        const int kh = juce::jmin (kKnobSz, r.getHeight() - kLblH - 2);
        const int kY = r.getY() + ((r.getHeight() - kLblH - 2) - kh) / 2;
        s.setBounds (r.getX(), kY, r.getWidth(), kh);
        l.setBounds (r.getX(), r.getBottom() - kLblH, r.getWidth(), kLblH);
    };
    placeKnob (mFltKbTrackKnob,  mFltKbTrackLbl,  0);
    placeKnob (mFltVelTrackKnob, mFltVelTrackLbl, 1);
}

void BaySickSynthEditor::layoutFltEnvDeck (juce::Rectangle<int> deck)
{
    // Left 4 cols: ADSR vertical sliders, Right: Amount group
    const int amtW = 80;
    const int sW   = (deck.getWidth() - amtW) / 4;
    const int lH   = kLblH + 18;

    juce::Slider* sliders[] = { &mFltASlider, &mFltDSlider, &mFltSSlider, &mFltRSlider };
    juce::Label*  labels[]  = { &mFltALbl,    &mFltDLbl,    &mFltSLbl,    &mFltRLbl    };
    for (int i = 0; i < 4; ++i)
    {
        auto r = deck.withX (deck.getX() + i * sW).withWidth (sW).reduced (kPad / 2);
        sliders[i]->setBounds (r.getX(), r.getY(), r.getWidth(), r.getHeight() - lH);
        labels [i]->setBounds (r.getX(), r.getBottom() - kLblH, r.getWidth(), kLblH);
    }

    auto amtArea = deck.withX (deck.getRight() - amtW).withWidth (amtW).reduced (kPad / 2);
    mFltAmtGroup.setBounds (amtArea);
    auto aInner = amtArea.reduced (4, kGrpTop);
    const int kh = juce::jmin (kKnobSz, aInner.getHeight() - kLblH - 2);
    const int kY = aInner.getY() + ((aInner.getHeight() - kLblH - 2) - kh) / 2;
    mFltEnvAmtKnob.setBounds (aInner.getX(), kY, aInner.getWidth(), kh);
    mFltEnvAmtLbl .setBounds (aInner.getX(), aInner.getBottom() - kLblH, aInner.getWidth(), kLblH);
}

void BaySickSynthEditor::layoutLFODeck (juce::Rectangle<int> deck)
{
    // Four equal panels: Shape | Rate | Dest | Amount
    const int pW = deck.getWidth() / 4;

    // ── Shape ─────────────────────────────────────────────────────────────────
    auto shape = deck.withWidth (pW).reduced (kPad);
    mLFOShapeGroup.setBounds (shape);
    if (mLFOShapeLed) mLFOShapeLed->setBounds (shape.reduced (4, kGrpTop));

    // ── Rate ──────────────────────────────────────────────────────────────────
    auto rate = deck.withX (deck.getX() + pW).withWidth (pW).reduced (kPad);
    mLFORateGroup.setBounds (rate);
    auto rInner = rate.reduced (4, kGrpTop);
    // Stack: Rate knob (top, capped at kKnobSz, centered) | RATE | DIV | SYNC
    const int syncH     = 22;
    const int divH      = 20;
    const int gap       = 3;
    const int knobArea  = rInner.getHeight() - kLblH - divH - syncH - (gap * 2);
    const int knobH     = juce::jmin (kKnobSz, knobArea);
    const int knobX     = rInner.getX() + (rInner.getWidth() - knobH) / 2;
    int y = rInner.getY() + (knobArea - knobH) / 2;
    mLFORateKnob.setBounds (knobX, y, knobH, knobH); y = rInner.getY() + knobArea;
    mLFORateLbl .setBounds (rInner.getX(), y, rInner.getWidth(), kLblH); y += kLblH + gap;
    mLFODivCbo  .setBounds (rInner.getX(), y, rInner.getWidth(), divH);  y += divH + gap;
    mLFOSyncBtn .setBounds (rInner.getX(), y, rInner.getWidth(), syncH);

    // ── Dest ──────────────────────────────────────────────────────────────────
    auto dest = deck.withX (deck.getX() + pW * 2).withWidth (pW).reduced (kPad);
    mLFODestGroup.setBounds (dest);
    if (mLFODestLed) mLFODestLed->setBounds (dest.reduced (4, kGrpTop));

    // ── Amount ────────────────────────────────────────────────────────────────
    auto amt = deck.withX (deck.getX() + pW * 3).withWidth (pW).reduced (kPad);
    mLFOAmtGroup.setBounds (amt);
    auto aInner = amt.reduced (4, kGrpTop);
    const int kh = juce::jmin (kKnobSz, aInner.getHeight() - kLblH - 2);
    const int kY = aInner.getY() + ((aInner.getHeight() - kLblH - 2) - kh) / 2;
    mLFOAmtKnob.setBounds (aInner.getX(), kY, aInner.getWidth(), kh);
    mLFOAmtLbl .setBounds (aInner.getX(), aInner.getBottom() - kLblH, aInner.getWidth(), kLblH);
}

void BaySickSynthEditor::layoutModDeck (juce::Rectangle<int> deck)
{
    // MOD deck (Option C) - 8-column grid.
    //   Col 1:   NOISE (toggle + colour)             - P3.3 / P3.9
    //   Col 2-3: TRANSIENT (3 knobs)                  - P3.5
    //   Col 4-5: BURST ENV (toggle + 2 knobs)         - P3.6
    //   Col 6:   DRIFT (1 knob)                       - P3.10
    //   Col 7-8: UNISON (3 knobs)                     - P3.11
    const int colW = deck.getWidth() / 8;

    // Col 1: NOISE group (toggle + colour selector)
    auto noiseArea = deck.withWidth (colW).reduced (kPad);
    mNoiseGroup.setBounds (noiseArea);
    auto nInner = noiseArea.reduced (4, kGrpTop);
    const int btnH    = 26;
    const int cboH    = 20;
    const int nGap    = 6;
    mNoiseOnlyBtn.setBounds (nInner.getX(),
                             nInner.getY() + 8,
                             nInner.getWidth(),
                             btnH);
    mNoiseColorCbo.setBounds (nInner.getX(),
                              nInner.getY() + 8 + btnH + nGap,
                              nInner.getWidth(),
                              cboH);

    // Col 2-3: TRANSIENT group (3 knobs)
    auto transArea = deck.withX (deck.getX() + colW).withWidth (colW * 2).reduced (kPad);
    mTransientGroup.setBounds (transArea);
    auto tInner = transArea.reduced (4, kGrpTop);
    const int kW3 = tInner.getWidth() / 3;
    auto placeKnob = [&] (juce::Slider& s, juce::Label& l, int col, juce::Rectangle<int> area, int slotW)
    {
        const int x  = area.getX() + col * slotW;
        const int kh = juce::jmin (kKnobSz, area.getHeight() - kLblH - 2);
        const int kY = area.getY() + ((area.getHeight() - kLblH - 2) - kh) / 2;
        s.setBounds (x, kY, slotW, kh);
        l.setBounds (x, area.getBottom() - kLblH, slotW, kLblH);
    };
    placeKnob (mTransAmtKnob, mTransAmtLbl, 0, tInner, kW3);
    placeKnob (mTransDurKnob, mTransDurLbl, 1, tInner, kW3);
    placeKnob (mTransColKnob, mTransColLbl, 2, tInner, kW3);

    // Col 4-5: BURST ENV - button on top, 2 knobs below
    auto burstArea = deck.withX (deck.getX() + colW * 3).withWidth (colW * 2).reduced (kPad);
    mBurstGroup.setBounds (burstArea);
    auto bInner = burstArea.reduced (4, kGrpTop);
    const int burstBtnH = 22;
    mBurstModeBtn.setBounds (bInner.getX(), bInner.getY(), bInner.getWidth(), burstBtnH);
    auto bKnobRow = bInner.withTrimmedTop (burstBtnH + kPad);
    const int burstKW = bKnobRow.getWidth() / 2;
    placeKnob (mBurstCountKnob,   mBurstCountLbl,   0, bKnobRow, burstKW);
    placeKnob (mBurstSpacingKnob, mBurstSpacingLbl, 1, bKnobRow, burstKW);

    // Col 6: DRIFT
    auto driftArea = deck.withX (deck.getX() + colW * 5).withWidth (colW).reduced (kPad);
    mDriftGroup.setBounds (driftArea);
    auto dInner = driftArea.reduced (4, kGrpTop);
    placeKnob (mDriftKnob, mDriftLbl, 0, dInner, dInner.getWidth());

    // Col 7-8: UNISON (3 knobs)
    auto uniArea = deck.withX (deck.getX() + colW * 6).withWidth (colW * 2).reduced (kPad);
    mUnisonGroup.setBounds (uniArea);
    auto uInner = uniArea.reduced (4, kGrpTop);
    const int uniKW = uInner.getWidth() / 3;
    placeKnob (mUniVoicesKnob, mUniVoicesLbl, 0, uInner, uniKW);
    placeKnob (mUniDetuneKnob, mUniDetuneLbl, 1, uInner, uniKW);
    placeKnob (mUniSpreadKnob, mUniSpreadLbl, 2, uInner, uniKW);
}

//==============================================================================
// ── Tab switching ─────────────────────────────────────────────────────────────

void BaySickSynthEditor::showDeck (int deck, bool visible)
{
    switch (deck)
    {
        case 0: // OSC
            mWavGroup.setVisible (visible);       mVoiceGroup.setVisible (visible);
            mModWheelGroup.setVisible (visible);
            mWaveformCbo.setVisible (visible);
            mDualOscModeCbo.setVisible (visible);
            mOscSyncBtn.setVisible (visible);
            mRingModBtn.setVisible (visible);
            mTransposeKnob.setVisible (visible);  mTransposeLbl.setVisible (visible);
            mModifierKnob.setVisible (visible);   mModifierLbl.setVisible (visible);
            mNoiseKnob.setVisible (visible);      mNoiseLbl.setVisible (visible);
            mGlideKnob.setVisible (visible);      mGlideLbl.setVisible (visible);
            mOutVolKnob.setVisible (visible);     mOutVolLbl.setVisible (visible);
            mCutSelfBtn.setVisible (visible);
            mCutSelfModeBtn.setVisible (visible);
            mModWheelAmtKnob.setVisible (visible); mModWheelAmtLbl.setVisible (visible);
            mVoiceModeLed->setVisible (visible);
            mModWheelDestLed->setVisible (visible);
            break;

        case 1: // AMP ENV
            mAmpEnvGroup.setVisible (visible); mPitchEnvGroup.setVisible (visible);
            mAmpASlider.setVisible (visible); mAmpALbl.setVisible (visible);
            mAmpDSlider.setVisible (visible); mAmpDLbl.setVisible (visible);
            mAmpSSlider.setVisible (visible); mAmpSLbl.setVisible (visible);
            mAmpRSlider.setVisible (visible); mAmpRLbl.setVisible (visible);
            mVelAmpKnob.setVisible (visible); mVelAmpLbl.setVisible (visible);
            mPEnvASlider.setVisible (visible); mPEnvALbl.setVisible (visible);
            mPEnvDSlider.setVisible (visible); mPEnvDLbl.setVisible (visible);
            mPEnvSSlider.setVisible (visible); mPEnvSLbl.setVisible (visible);
            mPEnvRSlider.setVisible (visible); mPEnvRLbl.setVisible (visible);
            mPEnvAmtKnob.setVisible (visible); mPEnvAmtLbl.setVisible (visible);
            break;

        case 2: // FILTER
            mFilterTypeGroup.setVisible (visible);
            mFilterTrackGroup.setVisible (visible);
            mFilterXYPad->setVisible (visible);
            if (mFilterTypeLed) mFilterTypeLed->setVisible (visible);
            mFltKbTrackKnob.setVisible (visible);  mFltKbTrackLbl.setVisible (visible);
            mFltVelTrackKnob.setVisible (visible);  mFltVelTrackLbl.setVisible (visible);
            break;

        case 3: // FLT ENV
            mFltASlider.setVisible (visible); mFltALbl.setVisible (visible);
            mFltDSlider.setVisible (visible); mFltDLbl.setVisible (visible);
            mFltSSlider.setVisible (visible); mFltSLbl.setVisible (visible);
            mFltRSlider.setVisible (visible); mFltRLbl.setVisible (visible);
            mFltAmtGroup.setVisible (visible);
            mFltEnvAmtKnob.setVisible (visible); mFltEnvAmtLbl.setVisible (visible);
            break;

        case 4: // LFO
            mLFOShapeGroup.setVisible (visible); mLFOShapeLed->setVisible (visible);
            mLFORateGroup.setVisible  (visible);
            mLFORateKnob.setVisible   (visible); mLFORateLbl.setVisible (visible);
            mLFODivCbo.setVisible     (visible);
            mLFOSyncBtn.setVisible    (visible);
            mLFODestGroup.setVisible  (visible); mLFODestLed->setVisible (visible);
            mLFOAmtGroup.setVisible   (visible);
            mLFOAmtKnob.setVisible    (visible); mLFOAmtLbl.setVisible (visible);
            break;

        case 5: // MOD
            mNoiseGroup.setVisible   (visible);
            mNoiseOnlyBtn.setVisible (visible);
            mNoiseColorCbo.setVisible (visible);
            mTransientGroup.setVisible (visible);
            mTransAmtKnob.setVisible (visible); mTransAmtLbl.setVisible (visible);
            mTransDurKnob.setVisible (visible); mTransDurLbl.setVisible (visible);
            mTransColKnob.setVisible (visible); mTransColLbl.setVisible (visible);
            mBurstGroup.setVisible (visible);
            mBurstModeBtn.setVisible (visible);
            mBurstCountKnob.setVisible (visible);   mBurstCountLbl.setVisible (visible);
            mBurstSpacingKnob.setVisible (visible); mBurstSpacingLbl.setVisible (visible);
            mDriftGroup.setVisible (visible);
            mDriftKnob.setVisible (visible); mDriftLbl.setVisible (visible);
            mUnisonGroup.setVisible (visible);
            mUniVoicesKnob.setVisible (visible); mUniVoicesLbl.setVisible (visible);
            mUniDetuneKnob.setVisible (visible); mUniDetuneLbl.setVisible (visible);
            mUniSpreadKnob.setVisible (visible); mUniSpreadLbl.setVisible (visible);
            break;

        default: break;
    }
}

void BaySickSynthEditor::setActiveTab (int tab)
{
    mActiveTab = juce::jlimit (0, 5, tab);
    for (int i = 0; i < 6; ++i) showDeck (i, false);
    showDeck (mActiveTab, true);
    updateTabButtons();
    // MOD tab isn't in the visualizer's rotation - clamp to 4 (LFO) so the
    // scrolling LFO display stays active there too.
    mVisualizer.setActiveTab (juce::jlimit (0, 4, mActiveTab));
    resized();
    repaint();
}

void BaySickSynthEditor::updateTabButtons()
{
    for (int i = 0; i < 6; ++i)
    {
        mTabBtns[i].setToggleState (i == mActiveTab, juce::dontSendNotification);
        mTabBtns[i].repaint();
    }
}

//==============================================================================
// ── Preset management ─────────────────────────────────────────────────────────

juce::File BaySickSynthEditor::presetsDir() const
{
    // P4b (2026-04-23): moved Roaming -> Documents per unified-folder layout.
    return AppPaths::appRoot().getChildFile ("Presets/BaySickSynth");
}

// Recursive XML-preset walker - folders become real cascading submenus.
static void addSynthPresetDirToMenu (juce::PopupMenu& menu,
                                      const juce::File& dir,
                                      std::function<void(const juce::File&)> onSelect)
{
    juce::Array<juce::File> dirs;
    dir.findChildFiles (dirs, juce::File::findDirectories, false);
    dirs.sort();
    juce::Array<juce::File> files;
    dir.findChildFiles (files, juce::File::findFiles, false, "*.xml");
    files.sort();

    for (const auto& child : dirs)
    {
        juce::PopupMenu sub;
        addSynthPresetDirToMenu (sub, child, onSelect);
        if (sub.getNumItems() > 0)
            menu.addSubMenu (child.getFileName(), sub);
    }
    for (const auto& f : files)
        menu.addItem (f.getFileNameWithoutExtension(), [onSelect, f] { onSelect (f); });
}

void BaySickSynthEditor::showPresetMenu()
{
    juce::PopupMenu menu;

    // 2026-04-25: real cascading submenus per folder (matches sample-picker UX).
    const auto dir = presetsDir();
    if (dir.exists())
        addSynthPresetDirToMenu (menu, dir, [this] (const juce::File& f) { loadPreset (f); });

    menu.addSeparator();
    menu.addItem ("Save preset...", [this]
    {
        auto* dlg = new juce::AlertWindow ("Save Preset", "Enter preset name:",
                                            juce::MessageBoxIconType::NoIcon);
        dlg->addTextEditor ("name", "My Preset");
        dlg->addButton ("Save",   1);
        dlg->addButton ("Cancel", 0);
        dlg->enterModalState (true, juce::ModalCallbackFunction::create (
            [this, dlg] (int result) {
                if (result == 1) savePreset (dlg->getTextEditorContents ("name"));
                delete dlg;
            }));
    });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (mPresetBtn));
}

void BaySickSynthEditor::savePreset (const juce::String& name)
{
    // 2026-04-26: user presets go into "My Presets/" subfolder so they're
    // grouped separately from the factory category folders in the picker.
    const auto dir = presetsDir().getChildFile ("My Presets");
    auto state = mProc.apvts.copyState();
    if (auto xml = state.createXml())
        UserFileSave::writeXmlAsync (dir, name, *xml, {});
}

void BaySickSynthEditor::loadPreset (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (! xml || ! xml->hasTagName (mProc.apvts.state.getType()))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Load Preset",
                                                "That preset file could not be read.",
                                                "OK");
        return;
    }

    auto loaded = juce::ValueTree::fromXml (*xml);

    // P4.1 trackId-portability fix: presets are saved with the saving instance's
    // full trackId-prefixed param IDs (e.g. tk_lay_0_bss_waveform).  Loading
    // into a different instance (e.g. drum slot tk_drm_0_s5_bss_*) requires
    // rewriting the IDs, otherwise replaceState applies values to a non-matching
    // tree and the bound parameters silently keep their old values.
    const juce::String localPrefix = mProc.getParamPrefix();   // e.g. "tk_drm_0_s5_bss_"
    juce::String loadedPrefix;
    for (int i = 0; i < loaded.getNumChildren(); ++i)
    {
        auto child = loaded.getChild (i);
        if (! child.hasType ("PARAM")) continue;
        const juce::String id = child.getProperty ("id").toString();
        const int tagIdx = id.indexOf ("_bss_");
        if (id.startsWith ("tk_") && tagIdx > 3)
        {
            loadedPrefix = id.substring (0, tagIdx + 5);   // include "_bss_"
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

    mProc.apvts.replaceStateKeepingUndoHistory (loaded, "Load Preset");   // ruling 3a

    // 2026-04-30: notify page wrapper so Layer/Bass tab + mixer strip get
    // renamed to the patch filename (matches DrumPage's auto-rename).
    if (onPatchLoaded)
        onPatchLoaded (f.getFileNameWithoutExtension());
}
