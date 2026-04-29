#include "HarmlessEditor.h"
#include "../Standalone/SharedUI.h"   // VKnobAutomation hooks

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr int kW       = 960;
static constexpr int kH       = 620;
static constexpr int kHdrH    = 36;
static constexpr int kGap     = 6;
static constexpr int kKnob    = 44;
static constexpr int kKnobSm  = 32;

// Proportional splits (from reference: top 55%, bottom 45%; 40/10/50, 40/60)
static constexpr float kTopFrac  = 0.55f;
static constexpr float kTLFrac   = 0.40f;
static constexpr float kTMFrac   = 0.10f;
// kTRFrac = 0.50 (remainder)
static constexpr float kBLFrac   = 0.40f;
// kBRFrac = 0.60 (remainder)

// ── Helpers ───────────────────────────────────────────────────────────────────
static void setupRotary (juce::Slider& s)
{
    s.setSliderStyle    (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle   (juce::Slider::NoTextBox, false, 0, 0);
    s.setScrollWheelEnabled (true);
    // 2026-04-19 (S1.5): live value popup on hover/drag, matching the VKnob
    // behaviour in the effects panels. Without this, tooltip shows only the
    // static text and there's no way to see the current numeric value.
    s.setPopupDisplayEnabled (true, true, nullptr);
}

static void setupVertical (juce::Slider& s)
{
    // 2026-04-19 (S1 followup): switched from LinearBarVertical (no thumb,
    // just a colored fill that looked like a "colored line with no fader")
    // to LinearVertical so HarmlessLAF::drawLinearSlider renders a real
    // metallic thumb cap + glowing fill + recessed track.
    s.setSliderStyle    (juce::Slider::LinearVertical);
    s.setTextBoxStyle   (juce::Slider::NoTextBox, false, 0, 0);
    s.setScrollWheelEnabled (true);
    // 2026-04-19 (S1.5): live value popup on hover/drag.
    s.setPopupDisplayEnabled (true, true, nullptr);
}

// Paint a section label + recessed background (1px machined inset border)
static void drawSection (juce::Graphics& g, juce::Rectangle<int> r, const char* title)
{
    if (r.isEmpty()) return;
    const auto rf = r.toFloat();
    g.setColour (juce::Colour (HarmlessLAF::kPanel));
    g.fillRoundedRectangle  (rf, 3.f);
    g.setColour (juce::Colour (HarmlessLAF::kBorder));
    g.drawRoundedRectangle  (rf.reduced (0.5f), 3.f, 1.f);
    g.setColour (juce::Colour (HarmlessLAF::kTextDim));
    g.setFont   (juce::Font  (8.0f, juce::Font::bold));
    g.drawText  (title, r.getX() + 6, r.getY() + 3, r.getWidth() - 8, 12,
                 juce::Justification::centredLeft);
}

// Draw knob label below a component
static void knobLabel (juce::Graphics& g, const juce::Component& c, const char* t)
{
    g.setColour (juce::Colour (HarmlessLAF::kTextDim));
    g.setFont   (juce::Font  (8.0f));
    g.drawText  (t, c.getX() - 4, c.getBottom() + 1,
                 c.getWidth() + 8, 10, juce::Justification::centred);
}

// ── Constructor ───────────────────────────────────────────────────────────────
HarmlessEditor::HarmlessEditor (HarmlessProcessor& p)
    : juce::AudioProcessorEditor (p), mProc (p)
{
    setLookAndFeel (&mLAF);
    setSize (kW, kH);

    auto& apvts = p.apvts;
    auto  pid   = [&p] (const char* n) { return p.pid (n); };

    // ── Waveform buttons ──────────────────────────────────────────────────────
    addAndMakeVisible (mTimbreWavA);
    addAndMakeVisible (mTimbreWavB);
    addAndMakeVisible (mTremWavBtn);
    addAndMakeVisible (mVibWavBtn);

    // ── Hidden shape sliders (APVTS attachment only) ──────────────────────────
    addChildComponent (mTimbreShapeSlider);
    addChildComponent (mPartBShapeSlider);   // S3: Part B shape, interactive via mTimbreWavB
    addChildComponent (mTremShapeSlider);
    addChildComponent (mVibShapeSlider);

    // Sync waveform buttons to hidden sliders
    mTimbreWavA.onChange = [this] (int s) {
        mTimbreShapeSlider.setValue (s, juce::sendNotificationSync);
    };
    mTimbreShapeSlider.onValueChange = [this] {
        mTimbreWavA.setValue (int (mTimbreShapeSlider.getValue()));
    };
    // S3: Part B waveform now interactive - mirrors the Part A wiring.
    mTimbreWavB.onChange = [this] (int s) {
        mPartBShapeSlider.setValue (s, juce::sendNotificationSync);
    };
    mPartBShapeSlider.onValueChange = [this] {
        mTimbreWavB.setValue (int (mPartBShapeSlider.getValue()));
    };
    mTremWavBtn.onChange = [this] (int s) {
        mTremShapeSlider.setValue (s, juce::sendNotificationSync);
    };
    mTremShapeSlider.onValueChange = [this] {
        mTremWavBtn.setValue (int (mTremShapeSlider.getValue()));
    };
    mVibWavBtn.onChange = [this] (int s) {
        mVibShapeSlider.setValue (s, juce::sendNotificationSync);
    };
    mVibShapeSlider.onValueChange = [this] {
        mVibWavBtn.setValue (int (mVibShapeSlider.getValue()));
    };

    // ── Specialized components ────────────────────────────────────────────────
    addAndMakeVisible (mFilter1Row);
    mFilter1Row.attachToApvts (apvts, pid("flt_cutoff"), pid("flt_res"),
                                pid("flt_env_amt"), pid("flt1_kb_track"), pid("flt1_type"));

    addAndMakeVisible (mFilter2Row);
    mFilter2Row.attachToApvts (apvts, pid("flt2_cutoff"), pid("flt2_res"),
                                pid("flt2_env_amt"), pid("flt2_kb_track"), pid("flt2_type"));

    addAndMakeVisible (mRoutingMatrix);
    // T2-F 2026-04-19: wire the 6 routing-matrix sliders to the new APVTS params.
    mRoutingMatrix.attachToApvts (apvts,
        pid("rm_sub"),  pid("rm_prot"), pid("rm_clip"),
        pid("rm_fx"),   pid("rm_vol"),  pid("rm_env"));

    addAndMakeVisible (mXYZPad);
    mXYZPad.attachToApvts (apvts, pid("mod_x"), pid("mod_y"), pid("mod_z"));

    // S4 Batch 3: mod editor now pulls state from the processor's registry.
    // Curve points live per-(target, source, tab) inside HarmlessModRegistry -
    // no editor-local point vector needed.
    addAndMakeVisible (mModEditor);
    mModEditor.setRegistry (&p.getModRegistry());

    // S5 T2-M: central spectrogram visualiser. Bounds set in resized()
    // to the full right column of the bot-left panel (mSpectroTopSec).
    addAndMakeVisible (mSpectrogram);
    mSpectrogram.setSynth (&p.getSynth());

    // S4 Batch 4: install right-click "Modulate envelope..." hooks so any
    // modulatable Harmless knob exposes the item. GlobalAutoRightClick calls
    // sShouldOfferModulate(paramId) to decide whether to show the item, and
    // sOnModulateEnvelope(paramId) on click. Cleared in the destructor.
    {
        auto& reg = p.getModRegistry();
        auto* modEditorPtr = &mModEditor;
        VKnobAutomation::sShouldOfferModulate = [&reg] (const juce::String& pid) -> bool
        {
            return reg.findTarget (pid) != nullptr;
        };
        VKnobAutomation::sOnModulateEnvelope = [modEditorPtr] (const juce::String& pid)
        {
            if (modEditorPtr) modEditorPtr->focusTarget (pid);
        };
    }

    // ── Top-Left knobs ────────────────────────────────────────────────────────
    for (auto* s : { &mTimbreBlend, &mPartALevel, &mPartBLevel, &mBrownian,
                     &mBlurSize, &mBlurTime, &mBlurHarm,            // S2 SLA
                     &mPrismAmt, &mPrismMode,
                     &mTremDepth, &mTremSpeed, &mTremGap,
                     &mVibDepth, &mVibSpeed, &mVibEnv,
                     &mGlideTime, &mLegatoLimit })
        { setupRotary (*s); addAndMakeVisible (*s); }
    // S4 Batch 4 fix: Global LFO re-exposed as macro. Rate+Shape+Tempo knobs
    // in the main editor write to lfo_rate/lfo_shape/lfo_tempo APVTS, which
    // bulk-copies to every target's LFO source in the mod registry. Per-target
    // override still works via the mod editor.
    setupRotary (mLfoRate);   addAndMakeVisible (mLfoRate);
    setupRotary (mLfoShape);  addAndMakeVisible (mLfoShape);
    mLfoRate .setRange (0, 12, 1);
    mLfoShape.setRange (0,  3, 1);
    mLfoRate .setTooltip ("LFO Rate (global). Sets the cycle length for every target's LFO source. Per-target override lives in the mod editor. 1/8 = fastest, 32 beats = slowest.");
    mLfoShape.setTooltip ("LFO Shape (global). Sine / Triangle / Saw / Square. Macro: changes all targets simultaneously.");
    mLfoRate.textFromValueFunction = [] (double idx) -> juce::String
    {
        static const char* kN[] = { "1/8", "1/4", "3/8", "1/2", "5/8", "3/4", "7/8",
                                    "1", "2", "4", "8", "16", "32" };
        const int i = juce::jlimit (0, 12, (int) std::round (idx));
        return juce::String (kN[i]) + juce::String (i == 7 ? " beat" : " beats");
    };
    mLfoShape.textFromValueFunction = [] (double v) -> juce::String
    {
        static const char* kS[] = { "Sine", "Triangle", "Saw", "Square" };
        return kS[juce::jlimit (0, 3, (int) std::round (v))];
    };
    mLfoRate.updateText();  mLfoShape.updateText();
    mLfoTempoBtn.setClickingTogglesState (true);
    mLfoTempoBtn.getProperties().set ("switchToggle", true);
    mLfoTempoBtn.setTooltip ("LFO Tempo Sync (global). On = rate in beats (syncs to BPM). Off = rate in seconds (free-running).");
    addAndMakeVisible (mLfoTempoBtn);

    mLegatoBtn.setClickingTogglesState (true);
    mLegatoBtn.getProperties().set ("switchToggle", true);
    addAndMakeVisible (mLegatoBtn);

    // ── Top-Middle (Unison) ───────────────────────────────────────────────────
    setupRotary (mUnisonVoices);  addAndMakeVisible (mUnisonVoices);
    setupRotary (mUnisonType);    addAndMakeVisible (mUnisonType);
    mUnisonAltBtn.setClickingTogglesState (true);
    mUnisonAltBtn.getProperties().set ("switchToggle", true);
    addAndMakeVisible (mUnisonAltBtn);

    setupVertical (mUnisonPan);   addAndMakeVisible (mUnisonPan);
    setupVertical (mUnisonPitch); addAndMakeVisible (mUnisonPitch);
    setupVertical (mUnisonPhase); addAndMakeVisible (mUnisonPhase);

    // ── Top-Right FX knobs ────────────────────────────────────────────────────
    for (auto* s : { &mPluckDecay, &mPhaserMix, &mPhaserDepth, &mPhaserRate,
                     &mPhaserWidth, &mPhaserOfs,                  // SLA-Impl
                     &mPhaserMaskRate, &mEQMix })
        { setupRotary (*s); addAndMakeVisible (*s); }
    mPluckBlurBtn.setClickingTogglesState (true);                  // SLA-Impl
    mPluckBlurBtn.getProperties().set ("switchToggle", true);
    addAndMakeVisible (mPluckBlurBtn);

    // ── S4 AG-1: Auto-gain toggle ─────────────────────────────────────────────
    mAutoGainBtn.setClickingTogglesState (true);
    mAutoGainBtn.getProperties().set ("switchToggle", true);
    mAutoGainBtn.setTooltip ("Auto-Gain Mode. REL (default): when summed partials exceed clip level, scale them all equally down. Preserves spectral balance. ABS: hard-cap each partial independently at its own ceiling. Preserves per-partial amplitude, may alter balance.");
    {
        auto* modeParam = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid ("auto_gain_mode")));
        const bool initOn = (modeParam != nullptr) && (modeParam->getValue() > 0.5f);
        mAutoGainBtn.setToggleState (initOn, juce::dontSendNotification);
        mAutoGainBtn.setButtonText (initOn ? "AG: ABS" : "AG: REL");
    }
    {
        const juce::String agParamId = pid ("auto_gain_mode");
        mAutoGainBtn.onClick = [this, agParamId]
        {
            const bool on = mAutoGainBtn.getToggleState();
            mAutoGainBtn.setButtonText (on ? "AG: ABS" : "AG: REL");
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (mProc.apvts.getParameter (agParamId)))
                p->setValueNotifyingHost (on ? 1.0f : 0.0f);
        };
    }
    addAndMakeVisible (mAutoGainBtn);

    // ── Pitch group (SLA-Impl) ────────────────────────────────────────────────
    for (auto* s : { &mPitchFreq, &mPitchDetune, &mPitchFreqFrac })
        { setupRotary (*s); addAndMakeVisible (*s); }
    mPitchOctBtn.setClickingTogglesState (true);
    mPitchOctBtn.getProperties().set ("switchToggle", true);
    mPitchHzBtn .setClickingTogglesState (true);
    mPitchHzBtn.getProperties().set ("switchToggle", true);
    addAndMakeVisible (mPitchOctBtn);
    addAndMakeVisible (mPitchHzBtn);

    // ── Bottom-Left knobs ─────────────────────────────────────────────────────
    setupRotary (mPartSel);  addAndMakeVisible (mPartSel);
    setupRotary (mVolume);   addAndMakeVisible (mVolume);
    setupRotary (mPan);      addAndMakeVisible (mPan);
    mPan.getProperties().set (HarmlessLAF::kBipolar, "true");

    mVelLinkBtn.setClickingTogglesState (true);
    mVelLinkBtn.getProperties().set ("switchToggle", true);
    addAndMakeVisible (mVelLinkBtn);

    mCutSelfBtn.setClickingTogglesState (true);
    mCutSelfBtn.getProperties().set ("switchToggle", true);
    mCutSelfBtn.setTooltip ("Cut Self: noteOn cuts any prior voice playing the same note.\n"
                            "Prevents phase stacking on rapid retrigs of the same note.");
    addAndMakeVisible (mCutSelfBtn);
    mPartABtn.setClickingTogglesState (true);
    mPartABtn.getProperties().set ("switchToggle", true);
    mPartBBtn.setClickingTogglesState (true);
    mPartBBtn.getProperties().set ("switchToggle", true);
    addAndMakeVisible (mPartABtn);
    addAndMakeVisible (mPartBBtn);
    // S3 part_sel: A/B buttons act as a 2-way radio group writing to part_sel.
    // The previously-bound mPartSelAtt slider stays attached to APVTS and the
    // buttons just push values to it (the slider is invisible by virtue of
    // never being addAndMakeVisible'd in the new layout). Initial state pulls
    // from APVTS via a syncFromApvts lambda invoked once after attachments.
    mPartABtn.onClick = [this, &p]
    {
        if (auto* pp = dynamic_cast<juce::RangedAudioParameter*> (
                          p.apvts.getParameter (p.pid ("part_sel"))))
            pp->setValueNotifyingHost (pp->getNormalisableRange().convertTo0to1 (0.0f));
        mPartABtn.setToggleState (true,  juce::dontSendNotification);
        mPartBBtn.setToggleState (false, juce::dontSendNotification);
        rebindToPart (0);   // S3.5: swap attachments to Part A's params
    };
    mPartBBtn.onClick = [this, &p]
    {
        if (auto* pp = dynamic_cast<juce::RangedAudioParameter*> (
                          p.apvts.getParameter (p.pid ("part_sel"))))
            pp->setValueNotifyingHost (pp->getNormalisableRange().convertTo0to1 (1.0f));
        mPartABtn.setToggleState (false, juce::dontSendNotification);
        mPartBBtn.setToggleState (true,  juce::dontSendNotification);
        rebindToPart (1);   // S3.5: swap attachments to Part B's params
    };
    // Initial state from APVTS.
    if (auto* pp = p.apvts.getRawParameterValue (p.pid ("part_sel")))
    {
        const bool isB = (pp->load() > 0.5f);
        mPartABtn.setToggleState (! isB, juce::dontSendNotification);
        mPartBBtn.setToggleState (  isB, juce::dontSendNotification);
    }

    for (auto* s : { &mAmpA, &mAmpD, &mAmpS, &mAmpR, &mPhaseStart, &mPhaseRand })
        { setupRotary (*s); addAndMakeVisible (*s); }

    for (auto* s : { &mLfoVel, &mLfoVol, &mLfoPitch })
        { setupVertical (*s); addAndMakeVisible (*s); }

    setupRotary (mStrumDirSlider); addAndMakeVisible (mStrumDirSlider);
    setupRotary (mStrumTime);      addAndMakeVisible (mStrumTime);
    setupRotary (mStrumTns);       addAndMakeVisible (mStrumTns);

    // ── Header ────────────────────────────────────────────────────────────────
    mPresetBtn.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (mPresetBtn);

    // ── APVTS attachments ─────────────────────────────────────────────────────
    // Top-left
    mTimbreShapeAtt  = std::make_unique<SliderAtt> (apvts, pid("timbre_shape"),   mTimbreShapeSlider);
    mTimbreBlendAtt  = std::make_unique<SliderAtt> (apvts, pid("timbre_blend"),   mTimbreBlend);
    // S3: Part B shape attachment (was missing - shape was hardcoded to Square).
    mPartBShapeAtt   = std::make_unique<SliderAtt> (apvts, pid("partB_timbre_shape"), mPartBShapeSlider);
    mPartAAtt        = std::make_unique<SliderAtt> (apvts, pid("partA_level"),    mPartALevel);
    mPartBAtt        = std::make_unique<SliderAtt> (apvts, pid("partB_level"),    mPartBLevel);
    // 2026-04-19 (S3.5): the per-part-able knobs are NOT directly attached
    // here - rebindToPart() below installs the active attachment based on the
    // current part_sel. mBrownianAtt / mBlurSizeAtt / mPrismAmtAtt / etc are
    // intentionally left null; the dual-slider list owns the live attachment.
    mTremShapeAtt    = std::make_unique<SliderAtt> (apvts, pid("trem_shape"),     mTremShapeSlider);
    mTremDepthAtt    = std::make_unique<SliderAtt> (apvts, pid("trem_depth"),     mTremDepth);
    mTremSpeedAtt    = std::make_unique<SliderAtt> (apvts, pid("trem_speed"),     mTremSpeed);
    mTremGapAtt      = std::make_unique<SliderAtt> (apvts, pid("trem_gap"),       mTremGap);
    mVibShapeAtt     = std::make_unique<SliderAtt> (apvts, pid("vib_shape"),      mVibShapeSlider);
    mVibDepthAtt     = std::make_unique<SliderAtt> (apvts, pid("vib_depth"),      mVibDepth);
    mVibSpeedAtt     = std::make_unique<SliderAtt> (apvts, pid("vib_speed"),      mVibSpeed);
    mVibEnvAtt       = std::make_unique<SliderAtt> (apvts, pid("vib_env"),        mVibEnv);
    mGlideTimeAtt    = std::make_unique<SliderAtt> (apvts, pid("glide_time"),     mGlideTime);
    mLegatoLimitAtt  = std::make_unique<SliderAtt> (apvts, pid("legato_limit"),   mLegatoLimit);
    mLegatoAtt       = std::make_unique<ButtonAtt> (apvts, pid("legato"),         mLegatoBtn);
    // Top-middle
    mUnisonVoicesAtt = std::make_unique<SliderAtt> (apvts, pid("unison_voices"),  mUnisonVoices);
    mUnisonTypeAtt   = std::make_unique<SliderAtt> (apvts, pid("unison_type"),    mUnisonType);
    mUnisonAltAtt    = std::make_unique<ButtonAtt> (apvts, pid("unison_alt"),     mUnisonAltBtn);
    mUnisonPanAtt    = std::make_unique<SliderAtt> (apvts, pid("unison_spread"),  mUnisonPan);
    mUnisonPitchAtt  = std::make_unique<SliderAtt> (apvts, pid("unison_detune"),  mUnisonPitch);
    mUnisonPhaseAtt  = std::make_unique<SliderAtt> (apvts, pid("unison_phase"),   mUnisonPhase);
    // Top-right FX
    // S3.5: pluck_decay dual-attached via rebindToPart - leave null here.
    mPhaserMixAtt    = std::make_unique<SliderAtt> (apvts, pid("ophaser_mix"),    mPhaserMix);
    mPhaserDepthAtt  = std::make_unique<SliderAtt> (apvts, pid("ophaser_depth"),  mPhaserDepth);
    mPhaserRateAtt   = std::make_unique<SliderAtt> (apvts, pid("ophaser_rate"),   mPhaserRate);
    // T1a 2026-04-19: oeq_mix param now exists - attachment uncommented.
    mEQMixAtt        = std::make_unique<SliderAtt> (apvts, pid("oeq_mix"),        mEQMix);
    // T2-H 2026-04-19: phaser_mask_rate knob - param + DSP exist; was missing UI.
    // S3.5: phaser_mask_rate dual-attached via rebindToPart - leave null here.
    juce::ignoreUnused (mPhaserMaskRateAtt);
    // SLA-Impl 2026-04-19: Phaser WIDTH + OFS + Pluck blur button.
    mPhaserWidthAtt  = std::make_unique<SliderAtt> (apvts, pid("ophaser_width"),  mPhaserWidth);
    mPhaserOfsAtt    = std::make_unique<SliderAtt> (apvts, pid("ophaser_ofs"),    mPhaserOfs);
    // S3.5: pluck_blur dual-attached via rebindToPart - leave null here.
    juce::ignoreUnused (mPluckBlurAtt);
    // SLA-Impl: Pitch group attachments. mPitchFreq -> pitch_semitones,
    // mPitchDetune -> pitch_cents, mPitchFreqFrac -> pitch_freq_frac.
    mPitchFreqAtt     = std::make_unique<SliderAtt> (apvts, pid("pitch_semitones"), mPitchFreq);
    mPitchDetuneAtt   = std::make_unique<SliderAtt> (apvts, pid("pitch_cents"),     mPitchDetune);
    mPitchFreqFracAtt = std::make_unique<SliderAtt> (apvts, pid("pitch_freq_frac"), mPitchFreqFrac);
    // S2 attachments
    // S3.5: blur_time / blur_harm dual-attached via rebindToPart - leave null here.
    // S4 Batch 4 fix: global LFO attachments restored. Rate/Shape/Tempo act
    // as a macro that copies to every target's LFO source via the processor.
    mLfoRateAtt  = std::make_unique<SliderAtt> (apvts, pid ("lfo_rate"),  mLfoRate);
    mLfoShapeAtt = std::make_unique<SliderAtt> (apvts, pid ("lfo_shape"), mLfoShape);
    mLfoTempoAtt = std::make_unique<ButtonAtt> (apvts, pid ("lfo_tempo"), mLfoTempoBtn);
    // XYZ destination dropdowns removed - routing moved to the mod matrix.
    // oct + Hz are UI-only display toggles (no APVTS attachment) per the SLA
    // decision - they don't change DSP, just toggle the popup-display unit.
    mPitchOctBtn.onClick = [this]
    {
        // Round-trip current pitch_semitones through octaves on toggle.
        // Implementation deferred - for now just toggle visual state.
    };
    // Bottom-left
    mPartSelAtt      = std::make_unique<SliderAtt> (apvts, pid("part_sel"),       mPartSel);
    mVolumeAtt       = std::make_unique<SliderAtt> (apvts, pid("volume"),         mVolume);
    mPanAtt          = std::make_unique<SliderAtt> (apvts, pid("pan"),            mPan);
    mVelLinkAtt      = std::make_unique<ButtonAtt> (apvts, pid("vel_link"),       mVelLinkBtn);
    mCutSelfAtt      = std::make_unique<ButtonAtt> (apvts, pid("cutSelf"),        mCutSelfBtn);
    mAmpAAtt         = std::make_unique<SliderAtt> (apvts, pid("amp_a"),          mAmpA);
    mAmpDAtt         = std::make_unique<SliderAtt> (apvts, pid("amp_d"),          mAmpD);
    mAmpSAtt         = std::make_unique<SliderAtt> (apvts, pid("amp_s"),          mAmpS);
    mAmpRAtt         = std::make_unique<SliderAtt> (apvts, pid("amp_r"),          mAmpR);
    mPhaseStartAtt   = std::make_unique<SliderAtt> (apvts, pid("phase_start"),    mPhaseStart);
    mPhaseRandAtt    = std::make_unique<SliderAtt> (apvts, pid("phase_rand"),     mPhaseRand);
    // S4: lfo_vel/vol/pitch attachments removed with the params.
    mStrumDirAtt     = std::make_unique<SliderAtt> (apvts, pid("strum_dir"),      mStrumDirSlider);
    mStrumTimeAtt    = std::make_unique<SliderAtt> (apvts, pid("strum_time"),     mStrumTime);
    mStrumTnsAtt     = std::make_unique<SliderAtt> (apvts, pid("strum_tns"),      mStrumTns);

    // T1d/T1e 2026-04-19: setComponentID + setTooltip on every attached slider
    // so GlobalAutoRightClick exposes the "Automate: ..." + "Type in value..."
    // menus and hover reveals the param name + units. ASCII-only strings.
    auto wireMeta = [&p] (juce::Slider& s, const char* paramSuffix, const char* tip)
    {
        s.setComponentID (p.pid (paramSuffix));
        s.setTooltip     (tip);
    };
    // Top-Left
    wireMeta (mTimbreBlend,    "timbre_blend",     "Timbre Blend - crossfades Part A toward Part B (0..1)");
    wireMeta (mPartALevel,     "partA_level",      "Part A Level (0..1)");
    wireMeta (mPartBLevel,     "partB_level",      "Part B Level (0..1)");
    wireMeta (mBrownian,       "brownian_amount",  "Brownian rolloff - 0 flat / 1 brown noise (~6 dB/oct)");
    wireMeta (mBlurSize,       "blur_size",        "Blur - spectral smear amount (0..1)");
    wireMeta (mPrismAmt,       "prism_amount",     "Prism - inharmonic spread amount (0..1)");
    wireMeta (mPrismMode,      "prism_mode",       "Prism Mode - 0 stretched / 1 bunched / 2 scattered");
    wireMeta (mTremDepth,      "trem_depth",       "Tremolo Depth (0..1)");
    wireMeta (mTremSpeed,      "trem_speed",       "Tremolo Speed (Hz)");
    wireMeta (mTremGap,        "trem_gap",         "Tremolo Gap - dead-zone around centre (0..1)");
    wireMeta (mVibDepth,       "vib_depth",        "Vibrato Depth (semitones, 0..2)");
    wireMeta (mVibSpeed,       "vib_speed",        "Vibrato Speed (Hz)");
    wireMeta (mVibEnv,         "vib_env",          "Vibrato Envelope - onset delay (0 instant .. 1 slow)");
    wireMeta (mGlideTime,      "glide_time",       "Glide / Portamento Time (seconds, 0..2)");
    wireMeta (mLegatoLimit,    "legato_limit",     "Legato Limit - max glide time cap (seconds)");
    // Top-Middle (Unison)
    wireMeta (mUnisonVoices,   "unison_voices",    "Unison Voices (1..9)");
    wireMeta (mUnisonType,     "unison_type",      "Unison Type (0=Pure, 1=Random, 2=Drifting, 3=Alt-only)");
    wireMeta (mUnisonPan,      "unison_spread",    "Unison Stereo Spread (0..1)");
    wireMeta (mUnisonPitch,    "unison_detune",    "Unison Detune (cents, 0..100)");
    wireMeta (mUnisonPhase,    "unison_phase",     "Unison Phase Offset (0..1)");
    // Top-Right FX
    wireMeta (mPluckDecay,     "pluck_decay",      "Pluck Decay - high-partial decay rate (0..1)");
    wireMeta (mPhaserMix,      "ophaser_mix",      "Output Phaser Mix (0..1)");
    wireMeta (mPhaserDepth,    "ophaser_depth",    "Output Phaser Depth (0..1)");
    wireMeta (mPhaserRate,     "ophaser_rate",     "Output Phaser Rate (Hz)");
    wireMeta (mPhaserMaskRate, "phaser_mask_rate", "Phaser Mask Rate - spectral notch spacing");
    wireMeta (mPhaserWidth,    "ophaser_width",    "Phaser WIDTH - regenerative feedback (0..0.95)");
    wireMeta (mPhaserOfs,      "ophaser_ofs",      "Phaser OFS - centre frequency (200..2000 Hz)");
    wireMeta (mEQMix,          "oeq_mix",          "Output Tilt EQ Mix (0..1)");
    // SLA-Impl Pitch group
    wireMeta (mPitchFreq,      "pitch_semitones",  "Pitch Freq - note offset in semitones (-24..+24)");
    wireMeta (mPitchDetune,    "pitch_cents",      "Pitch Detune - fine cents offset (-100..+100)");
    wireMeta (mPitchFreqFrac,  "pitch_freq_frac",  "Pitch Fraction - 1/1, 1/2, 1/4, 1/8, x2, x4, x8");
    // S2 wires
    wireMeta (mBlurTime,       "blur_time",        "Blur Time - kernel width scale (0..2, default 1)");
    wireMeta (mBlurHarm,       "blur_harm",        "Blur Harm - harmonic-axis bias (0..1)");
    // S4: lfo_rate / lfo_shape wireMeta removed (params ripped).
    // Bottom-Left
    wireMeta (mPartSel,        "part_sel",         "Part Selector (A=0, B=1) - editor-side, no DSP effect today");
    wireMeta (mVolume,         "volume",           "Master Volume (0..1)");
    wireMeta (mPan,            "pan",              "Master Pan (-1..+1)");
    wireMeta (mAmpA,           "amp_a",            "Amp Attack (seconds)");
    wireMeta (mAmpD,           "amp_d",            "Amp Decay (seconds)");
    wireMeta (mAmpS,           "amp_s",            "Amp Sustain (0..1)");
    wireMeta (mAmpR,           "amp_r",            "Amp Release (seconds)");
    wireMeta (mPhaseStart,     "phase_start",      "Phase Start position (0..1)");
    wireMeta (mPhaseRand,      "phase_rand",       "Phase Randomisation amount (0..1)");
    // S4: lfo_vel / vol / pitch wireMeta removed (params ripped).
    wireMeta (mStrumDirSlider, "strum_dir",        "Strum Direction (0 up, 1 down, 2 random)");
    wireMeta (mStrumTime,      "strum_time",       "Strum Time - total stagger (seconds)");
    wireMeta (mStrumTns,       "strum_tns",        "Strum Tension - curve (-1 end / 0 linear / +1 start)");

    // 2026-04-19 (S1.5b): mark discrete-mode multi-selectors as chicken-head
    // visual style. All other knobs use the default Time-effects filmstrip
    // with orange tint per Jeff's "knobs should be the knob from the time
    // effects LAF". Multi-selectors get the chicken-head pointer because
    // they pick from a small fixed set of options (mode dropdowns).
    mPrismMode      .getProperties().set (HarmlessLAF::kKnobVariant, "chickenHead");
    mUnisonType     .getProperties().set (HarmlessLAF::kKnobVariant, "chickenHead");
    mStrumDirSlider .getProperties().set (HarmlessLAF::kKnobVariant, "chickenHead");
    mPitchFreqFrac  .getProperties().set (HarmlessLAF::kKnobVariant, "chickenHead");   // SLA-Impl
    mLfoShape       .getProperties().set (HarmlessLAF::kKnobVariant, "chickenHead");   // S2 T2-B

    // Sync initial waveform button states
    mTimbreWavA.setValue (int (mTimbreShapeSlider.getValue()));
    mTremWavBtn.setValue (int (mTremShapeSlider.getValue()));
    mVibWavBtn .setValue (int (mVibShapeSlider.getValue()));

    // 2026-04-19 (S3.5): build the dual-attachment lists + install initial
    // attachments based on current part_sel (defaults to Part A on cold start).
    // Note: DualSliderPart/DualButtonPart hold a unique_ptr (move-only), so the
    // vector can't be initializer-list-assigned (requires copy). Build via
    // explicit emplace_back instead.
    auto addSlider = [this] (juce::Slider* s, juce::String pa, juce::String pb)
    {
        DualSliderPart d;
        d.slider = s; d.paramA = std::move (pa); d.paramB = std::move (pb);
        mDualSliders.push_back (std::move (d));
    };
    addSlider (&mBrownian,        p.pid("brownian_amount"),  p.pid("partB_brownian_amount"));
    addSlider (&mBlurSize,        p.pid("blur_size"),        p.pid("partB_blur_size"));
    addSlider (&mBlurTime,        p.pid("blur_time"),        p.pid("partB_blur_time"));
    addSlider (&mBlurHarm,        p.pid("blur_harm"),        p.pid("partB_blur_harm"));
    addSlider (&mPrismAmt,        p.pid("prism_amount"),     p.pid("partB_prism_amount"));
    addSlider (&mPrismMode,       p.pid("prism_mode"),       p.pid("partB_prism_mode"));
    addSlider (&mPluckDecay,      p.pid("pluck_decay"),      p.pid("partB_pluck_decay"));
    addSlider (&mPhaserMaskRate,  p.pid("phaser_mask_rate"), p.pid("partB_phaser_mask_rate"));

    DualButtonPart pluckBlurDual;
    pluckBlurDual.button = &mPluckBlurBtn;
    pluckBlurDual.paramA = p.pid ("pluck_blur");
    pluckBlurDual.paramB = p.pid ("partB_pluck_blur");
    mDualButtons.push_back (std::move (pluckBlurDual));
    if (auto* pp = p.apvts.getRawParameterValue (p.pid ("part_sel")))
        mActivePart = (pp->load() > 0.5f) ? 1 : 0;
    rebindToPart (mActivePart);

    // Slider double-click default = current preset value (refreshed every
    // time the APVTS state is replaced via valueTreeRedirected).
    setSliderDoubleClickDefaultsFromApvts (*this, mProc.apvts);
    mProc.apvts.state.addListener (this);
}

// 2026-04-19 (S3.5): swap the dual attachments to the chosen part. Destroys
// the current attachment + creates a new one against the per-part param ID.
// Slider values auto-sync from the new APVTS values (JUCE attachment behaviour).
void HarmlessEditor::rebindToPart (int part)
{
    mActivePart = juce::jlimit (0, 1, part);
    auto& apvts = mProc.apvts;
    for (auto& d : mDualSliders)
    {
        d.current.reset();
        d.current = std::make_unique<SliderAtt> (
            apvts, mActivePart == 0 ? d.paramA : d.paramB, *d.slider);
    }
    for (auto& d : mDualButtons)
    {
        d.current.reset();
        d.current = std::make_unique<ButtonAtt> (
            apvts, mActivePart == 0 ? d.paramA : d.paramB, *d.button);
    }
}

HarmlessEditor::~HarmlessEditor()
{
    mProc.apvts.state.removeListener (this);
    // Clear the right-click "Modulate envelope..." hooks we installed. Multi-
    // instance caveat (last-registered wins) is deferred to T3-ModMatrixAutomation.
    VKnobAutomation::sShouldOfferModulate = nullptr;
    VKnobAutomation::sOnModulateEnvelope  = nullptr;
    setLookAndFeel (nullptr);
}

void HarmlessEditor::valueTreeRedirected (juce::ValueTree& tree)
{
    if (tree != mProc.apvts.state) return;
    setSliderDoubleClickDefaultsFromApvts (*this, mProc.apvts);
}

//==============================================================================
void HarmlessEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (HarmlessLAF::kChassis));

    // ── Header bar ────────────────────────────────────────────────────────────
    g.setColour (juce::Colour (0xFF111113));
    g.fillRect  (0, 0, getWidth(), kHdrH);
    g.setColour (juce::Colour (0xFF2E2E30));
    g.fillRect  (0, kHdrH - 1, getWidth(), 1);

    // Title with glow
    g.setColour (juce::Colour (HarmlessLAF::kAccent).withAlpha (0.15f));
    g.setFont   (juce::Font (16.0f, juce::Font::bold));
    g.drawText  ("HARMLESS", 13, -1, 140, kHdrH + 2, juce::Justification::centredLeft);
    g.setColour (juce::Colour (HarmlessLAF::kAccent));
    g.setFont   (juce::Font (15.0f, juce::Font::bold));
    g.drawText  ("HARMLESS", 14, 0, 140, kHdrH, juce::Justification::centredLeft);

    // ── Major panel backgrounds (recessed) ────────────────────────────────────
    auto drawPanel = [&] (juce::Rectangle<int> r) {
        g.setColour (juce::Colour (0xFF0E0E10));
        g.fillRoundedRectangle (r.toFloat(), 3.f);
        g.setColour (juce::Colour (HarmlessLAF::kBorder));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 3.f, 1.f);
    };

    drawPanel (mTopLeftBounds);
    drawPanel (mTopMidBounds);
    drawPanel (mTopRightBounds);
    drawPanel (mBotLeftBounds);

    // ── Section labels (2026-04-20 S5 layout redesign) ────────────────────────
    // Top-left: Row A (Output | Routing), Row B (Trem | Vib/Leg),
    // Row C (blank | Strum + XYZ).
    drawSection (g, mGlobalSec,    "OUTPUT");
    drawSection (g, mRoutingSec,   "ROUTING");
    drawSection (g, mTremSec,      "TREMOLO");
    drawSection (g, mVibLegatoSec, "VIBRATO / LEGATO");
    drawSection (g, mStrumSec,     "STRUM");

    // Top-middle: Unison / Pitch / LFO Mod stack.
    drawSection (g, mUnisonSec,    "UNISON");
    drawSection (g, mPitchSec,     "PITCH");
    drawSection (g, mLFOSec,       "LFO MOD");

    // Top-right 5x2 grid: Flt1 | Flt2, Timbre | Blur/Prism, AmpEnv | FX.
    drawSection (g, mFlt1Sec,      "FILTER 1");
    drawSection (g, mFlt2Sec,      "FILTER 2");
    drawSection (g, mTimbreSec,    "TIMBRE");
    drawSection (g, mBlurPrismSec, "BLUR / PRISM");
    drawSection (g, mAmpEnvSec,    "AMP ENV / PHASE");
    drawSection (g, mFXSec,        "FX - PLUCK / PHASER / EQ");

    // Bot-left right column: spectrogram placeholder (populated in L2).
    drawSection (g, mSpectroTopSec, "SPECTROGRAM");
    // mFutureR4L/R, mFutureR5L/R, mFutureBL_TopSec/BotSec, mSpectroBotSec
    // deliberately un-labelled - they're blank-for-future / continuation tiles.

    // ── Knob labels — Top-Left ────────────────────────────────────────────────
    knobLabel (g, mTimbreWavA,   "PART A");
    knobLabel (g, mTimbreWavB,   "PART B");
    knobLabel (g, mTimbreBlend,  "MIX");
    knobLabel (g, mPartALevel,   "A LVL");
    knobLabel (g, mPartBLevel,   "B LVL");
    knobLabel (g, mBrownian,     "BROWN");
    knobLabel (g, mBlurSize,     "BLUR");
    knobLabel (g, mBlurTime,     "TIME");      // S2 SLA #8
    knobLabel (g, mBlurHarm,     "HARM");      // S2 SLA #9
    knobLabel (g, mPrismAmt,     "PRISM");
    knobLabel (g, mPrismMode,    "MODE");
    knobLabel (g, mTremWavBtn,   "WAVE");
    knobLabel (g, mTremDepth,    "DEPTH");
    knobLabel (g, mTremSpeed,    "SPEED");
    knobLabel (g, mTremGap,      "GAP");
    knobLabel (g, mVibWavBtn,    "WAVE");
    knobLabel (g, mVibDepth,     "DEPTH");
    knobLabel (g, mVibSpeed,     "SPEED");
    knobLabel (g, mVibEnv,       "ENV");
    knobLabel (g, mGlideTime,    "GLIDE");
    knobLabel (g, mLegatoLimit,  "LIMIT");

    // ── Unison labels ─────────────────────────────────────────────────────────
    knobLabel (g, mUnisonVoices, "VOICES");
    knobLabel (g, mUnisonType,   "TYPE");
    knobLabel (g, mUnisonPan,    "PAN");
    knobLabel (g, mUnisonPitch,  "PITCH");
    knobLabel (g, mUnisonPhase,  "PHASE");

    // ── FX labels ─────────────────────────────────────────────────────────────
    knobLabel (g, mPluckDecay,   "PLUCK");
    knobLabel (g, mPhaserMix,    "MIX");
    knobLabel (g, mPhaserDepth,  "DEPTH");
    knobLabel (g, mPhaserRate,   "RATE");
    knobLabel (g, mPhaserWidth,  "WIDTH");    // SLA-Impl
    knobLabel (g, mPhaserOfs,    "OFS");      // SLA-Impl
    knobLabel (g, mPhaserMaskRate, "MASK");   // T2-H
    knobLabel (g, mEQMix,        "EQ");
    // SLA-Impl Pitch group labels
    knobLabel (g, mPitchFreq,     "FREQ");
    knobLabel (g, mPitchDetune,   "DETUNE");
    knobLabel (g, mPitchFreqFrac, "FRAC");

    // ── Bottom-left labels ────────────────────────────────────────────────────
    knobLabel (g, mVolume,       "VOL");
    knobLabel (g, mPan,          "PAN");
    knobLabel (g, mAmpA,         "ATK");
    knobLabel (g, mAmpD,         "DEC");
    knobLabel (g, mAmpS,         "SUS");
    knobLabel (g, mAmpR,         "REL");
    knobLabel (g, mPhaseStart,   "START");
    knobLabel (g, mPhaseRand,    "RAND");
    knobLabel (g, mLfoRate,  "RATE");
    knobLabel (g, mLfoShape, "SHAPE");
    knobLabel (g, mStrumDirSlider,"DIR");
    knobLabel (g, mStrumTime,    "TIME");
    knobLabel (g, mStrumTns,     "TNS");
}

//==============================================================================
void HarmlessEditor::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop (kHdrH);

    // Header row
    mPresetBtn.setBounds (getWidth() - 92, 4, 86, kHdrH - 8);

    bounds.reduce (kGap, 0);
    bounds.removeFromTop (kGap / 2);

    const int contentH = bounds.getHeight();
    const int contentW = bounds.getWidth();

    // ── Proportional splits ───────────────────────────────────────────────────
    const int topH = int (contentH * kTopFrac);
    const int botH = contentH - topH - kGap;

    const int tlW  = int (contentW * kTLFrac);
    const int tmW  = int (contentW * kTMFrac);
    const int trW  = contentW - tlW - tmW - kGap * 2;

    const int blW  = int (contentW * kBLFrac);
    const int brW  = contentW - blW - kGap;

    // ── Top Row ───────────────────────────────────────────────────────────────
    auto topRow = bounds.removeFromTop (topH);

    mTopLeftBounds  = topRow.removeFromLeft (tlW);
    topRow.removeFromLeft (kGap);
    mTopMidBounds   = topRow.removeFromLeft (tmW);
    topRow.removeFromLeft (kGap);
    mTopRightBounds = topRow;

    bounds.removeFromTop (kGap);

    // ── Bottom Row ────────────────────────────────────────────────────────────
    auto botRow = bounds;
    mBotLeftBounds  = botRow.removeFromLeft (blW);
    botRow.removeFromLeft (kGap);
    mBotRightBounds = botRow;

    // Distribute a list of (component, width, height) across a rect, with
    // equal gaps at start/between/end. Every row below uses this so knobs
    // span the full cell width like the effect panels do, instead of
    // clustering left.
    auto layoutRow = [] (juce::Rectangle<int> r,
                          std::initializer_list<std::tuple<juce::Component*, int, int>> items)
    {
        const int n = (int) items.size();
        if (n <= 0) return;
        int totalW = 0;
        for (auto& it : items) totalW += std::get<1> (it);
        const int gapSpace = juce::jmax (0, r.getWidth() - totalW);
        const int gap = gapSpace / (n + 1);
        int x = r.getX() + gap;
        for (auto& it : items)
        {
            auto* c = std::get<0> (it);
            const int w = std::get<1> (it);
            const int h = std::get<2> (it);
            const int y = r.getCentreY() - h / 2;
            c->setBounds (x, y, w, h);
            x += w + gap;
        }
    };

    // ═════════════════════════════════════════════════════════════════════════
    // S5 Layout redesign (2026-04-20).
    // See blueprint §P1 Harmless layout review for the full map. Quick version:
    //
    //   TOP-LEFT      TOP-MIDDLE      TOP-RIGHT (5x2 grid)
    //   Output|Route  Unison          Filter1 | Filter2
    //   Trem  |VibLeg Pitch           Timbre  | BlurPrism
    //   (merged)      LFO Mod         AmpEnv  | FX
    //   blank|Strum                   blank   | blank
    //        |XYZ                     blank   | blank
    //
    //   BOT-LEFT                      BOT-RIGHT
    //   blank | Spectrogram (top)     Mod Editor (unchanged)
    //   blank | Spectrogram (bottom)
    // ═════════════════════════════════════════════════════════════════════════

    // ─────────────────────────────────────────────────────────────────────────
    // TOP-LEFT panel — Row A (Output | Routing), Row B (Tremolo | Vib/Legato),
    // Row C merged (left blank | right: Strum top-1/4, XYZ pad bot-3/4).
    // ─────────────────────────────────────────────────────────────────────────
    {
        auto r = mTopLeftBounds.reduced (6, 4);
        const int secGap = 3;
        const int halfW  = (r.getWidth() - secGap) / 2;
        const int avail  = r.getHeight();
        // Row A uses current Timbre-box proportion (0.20); Row B uses current
        // Routing-box proportion (0.18); Row C eats the rest (~0.62).
        const int rowAH = int (avail * 0.20f);
        const int rowBH = int (avail * 0.18f);
        // rowCH = remainder

        // Row A: Output (Volume / Pan / VelLink, no A+B) | Routing Matrix
        auto rowA = r.removeFromTop (rowAH);
        mGlobalSec   = rowA.withWidth (halfW);                                 // Output
        mRoutingSec  = rowA.withX (rowA.getX() + halfW + secGap).withWidth (halfW);
        layoutRow (mGlobalSec.reduced (4, 16), {
            { &mVolume,     kKnobSm, kKnobSm },
            { &mPan,        kKnobSm, kKnobSm },
            { &mVelLinkBtn, 34,      18      },
            { &mCutSelfBtn, 56,      18      },
        });
        mRoutingMatrix.setBounds (mRoutingSec.reduced (4, 16));
        r.removeFromTop (secGap);

        // Row B: Tremolo | Vibrato/Legato
        auto rowB = r.removeFromTop (rowBH);
        mTremSec      = rowB.withWidth (halfW);
        mVibLegatoSec = rowB.withX (rowB.getX() + halfW + secGap).withWidth (halfW);
        layoutRow (mTremSec.reduced (4, 16), {
            { &mTremWavBtn, 32,      32      },
            { &mTremDepth,  kKnobSm, kKnobSm },
            { &mTremSpeed,  kKnobSm, kKnobSm },
            { &mTremGap,    kKnobSm, kKnobSm },
        });
        layoutRow (mVibLegatoSec.reduced (4, 16), {
            { &mVibWavBtn,   32,      32      },
            { &mVibDepth,    kKnobSm, kKnobSm },
            { &mVibSpeed,    kKnobSm, kKnobSm },
            { &mVibEnv,      kKnobSm, kKnobSm },
            { &mGlideTime,   kKnobSm, kKnobSm },
            { &mLegatoLimit, kKnobSm, kKnobSm },
            { &mLegatoBtn,   42,      20      },
        });
        r.removeFromTop (secGap);

        // Row C merged: left blank, right split (Strum top 1/4, XYZ bot 3/4).
        // `mBlurPrismSec` + `mPitchSec` now repurposed for future-space and are
        // not rendered (drawSection for them removed in paint()). `mStrumSec`
        // gets the top quarter of the right half; XYZ pad fills bottom 3/4.
        auto rowC = r;
        mBlurPrismSec = rowC.withWidth (halfW);   // blank future-space (left)
        mPitchSec     = juce::Rectangle<int> {};  // unused - Pitch moved to top-middle
        auto rowCright = rowC.withX (rowC.getX() + halfW + secGap).withWidth (halfW);
        const int strumH = rowCright.getHeight() / 4;
        mStrumSec = rowCright.removeFromTop (strumH);
        layoutRow (mStrumSec.reduced (4, 14), {
            { &mStrumDirSlider, kKnobSm, kKnobSm },
            { &mStrumTime,      kKnobSm, kKnobSm },
            { &mStrumTns,       kKnobSm, kKnobSm },
        });
        mXYZPad.setBounds (rowCright.reduced (2));
    }

    // ─────────────────────────────────────────────────────────────────────────
    // TOP-MIDDLE panel — Unison (shortened faders) / Pitch / LFO Mod stack.
    // ─────────────────────────────────────────────────────────────────────────
    {
        auto r = mTopMidBounds.reduced (6, 4);
        const int avail  = r.getHeight();
        // Reserve Pitch + LFO Mod each at the height of Row B in top-left
        // (0.18 × top-area ≈ Vib/Legato-box height). Unison gets the rest.
        const int rowBHeq = int (avail * 0.18f);
        const int pitchH  = rowBHeq;
        const int lfoH    = rowBHeq;
        const int unisonH = avail - pitchH - lfoH - 4;   // - 2 gaps of 2

        // Unison (top). Knob + button row keeps its current size; faders
        // shrink to fit the remaining height.
        auto unisonRect = r.removeFromTop (unisonH);
        mUnisonSec = unisonRect;
        {
            auto ur = unisonRect.reduced (6, 14);
            int x = ur.getX() + (ur.getWidth() - kKnob) / 2;
            mUnisonVoices.setBounds (x, ur.getY(), kKnob, kKnob);
            ur.removeFromTop (kKnob + 8);
            mUnisonType  .setBounds (ur.getX(), ur.getY(), ur.getWidth(), kKnobSm);
            ur.removeFromTop (kKnobSm + 2);
            mUnisonAltBtn.setBounds (ur.getX(), ur.getY(), ur.getWidth(), 18);
            ur.removeFromTop (20);
            // Remaining height -> 3 vertical faders. Will be short in the
            // compressed layout, per design (knobs/button keep their size).
            const int sliderW = (ur.getWidth() - 8) / 3;
            const int sliderH = juce::jmax (10, ur.getHeight() - 2);
            mUnisonPan  .setBounds (ur.getX(),                   ur.getY(), sliderW, sliderH);
            mUnisonPitch.setBounds (ur.getX() + sliderW + 4,     ur.getY(), sliderW, sliderH);
            mUnisonPhase.setBounds (ur.getX() + (sliderW + 4)*2, ur.getY(), sliderW, sliderH);
        }
        r.removeFromTop (2);

        // Pitch (middle): FREQ / DETUNE / fraction chicken-head / OCT + Hz toggles
        mPitchSec = r.removeFromTop (pitchH);
        layoutRow (mPitchSec.reduced (4, 14), {
            { &mPitchFreq,     kKnobSm, kKnobSm },
            { &mPitchDetune,   kKnobSm, kKnobSm },
            { &mPitchFreqFrac, kKnobSm, kKnobSm },
            { &mPitchOctBtn,   24,      18      },
            { &mPitchHzBtn,    20,      18      },
        });
        r.removeFromTop (2);

        // LFO Mod (bottom): RATE / SHAPE / TEMPO — global macro.
        mLFOSec = r;
        layoutRow (mLFOSec.reduced (4, 14), {
            { &mLfoRate,     kKnobSm, kKnobSm },
            { &mLfoShape,    kKnobSm, kKnobSm },
            { &mLfoTempoBtn, 52,      24      },
        });
    }

    // ─────────────────────────────────────────────────────────────────────────
    // TOP-RIGHT panel — 5×2 grid.
    //   R1: Filter 1 | Filter 2
    //   R2: Timbre (with A+B) | Blur/Prism
    //   R3: Amp Env + Phase | FX (Pluck / Phaser / EQ)
    //   R4+R5: blank (future upgrade space)
    // ─────────────────────────────────────────────────────────────────────────
    {
        auto r = mTopRightBounds.reduced (4, 4);
        const int cellGap = 3;
        const int rowH  = (r.getHeight() - cellGap * 4) / 5;
        const int halfW = (r.getWidth()  - cellGap) / 2;

        auto cellAt = [&] (int row, int col) -> juce::Rectangle<int>
        {
            const int y = r.getY() + row * (rowH + cellGap);
            const int x = r.getX() + col * (halfW + cellGap);
            return { x, y, halfW, rowH };
        };

        // R1: Filter 1 | Filter 2
        mFlt1Sec = cellAt (0, 0);
        mFlt2Sec = cellAt (0, 1);
        mFilter1Row.setBounds (mFlt1Sec.reduced (3, 12));
        mFilter2Row.setBounds (mFlt2Sec.reduced (3, 12));

        // R2: Timbre (with A/B) | Blur/Prism
        mTimbreSec    = cellAt (1, 0);
        mBlurPrismSec = cellAt (1, 1);
        layoutRow (mTimbreSec.reduced (3, 12), {
            { &mPartABtn,    22,      18      },
            { &mPartBBtn,    22,      18      },
            { &mTimbreWavA,  28,      28      },
            { &mTimbreWavB,  28,      28      },
            { &mTimbreBlend, kKnobSm, kKnobSm },
            { &mPartALevel,  kKnobSm, kKnobSm },
            { &mPartBLevel,  kKnobSm, kKnobSm },
            { &mBrownian,    kKnobSm, kKnobSm },
            { &mAutoGainBtn, 52,      kKnobSm - 8 },
        });
        layoutRow (mBlurPrismSec.reduced (3, 12), {
            { &mBlurSize,  kKnobSm, kKnobSm },
            { &mBlurTime,  kKnobSm, kKnobSm },
            { &mBlurHarm,  kKnobSm, kKnobSm },
            { &mPrismAmt,  kKnobSm, kKnobSm },
            { &mPrismMode, kKnobSm, kKnobSm },
        });

        // R3: Amp Env + Phase | FX (Pluck / Phaser / EQ)
        mAmpEnvSec = cellAt (2, 0);
        mFXSec     = cellAt (2, 1);
        layoutRow (mAmpEnvSec.reduced (3, 12), {
            { &mAmpA,       kKnobSm, kKnobSm },
            { &mAmpD,       kKnobSm, kKnobSm },
            { &mAmpS,       kKnobSm, kKnobSm },
            { &mAmpR,       kKnobSm, kKnobSm },
            { &mPhaseStart, kKnobSm, kKnobSm },
            { &mPhaseRand,  kKnobSm, kKnobSm },
        });
        layoutRow (mFXSec.reduced (3, 12), {
            { &mPluckDecay,    kKnobSm, kKnobSm },
            { &mPluckBlurBtn,  24,      18      },
            { &mPhaserMix,     kKnobSm, kKnobSm },
            { &mPhaserDepth,   kKnobSm, kKnobSm },
            { &mPhaserRate,    kKnobSm, kKnobSm },
            { &mPhaserWidth,   kKnobSm, kKnobSm },
            { &mPhaserOfs,     kKnobSm, kKnobSm },
            { &mPhaserMaskRate,kKnobSm, kKnobSm },
            { &mEQMix,         kKnobSm, kKnobSm },
        });

        // R4 + R5: blank (future upgrade space).
        mFutureR4LSec = cellAt (3, 0);
        mFutureR4RSec = cellAt (3, 1);
        mFutureR5LSec = cellAt (4, 0);
        mFutureR5RSec = cellAt (4, 1);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // BOTTOM-LEFT panel — left column blank (future space), right column
    // reserved for the spectrogram visualizer (two stacked tiles, populated
    // in S5 batch L2).
    // ─────────────────────────────────────────────────────────────────────────
    {
        auto r = mBotLeftBounds.reduced (6, 4);
        const int halfH = (r.getHeight() - kGap) / 2;
        const int halfW = (r.getWidth()  - kGap) / 2;

        mFutureBL_TopSec = juce::Rectangle<int> (r.getX(), r.getY(), halfW, halfH);
        mFutureBL_BotSec = juce::Rectangle<int> (r.getX(), r.getY() + halfH + kGap, halfW, halfH);
        // Spectrogram spans the ENTIRE right column as one visualizer, not two
        // stacked boxes. mSpectroBotSec kept as a zero rect so drawSection
        // for it is a no-op (it's drawn as part of mSpectroTopSec now).
        mSpectroTopSec = juce::Rectangle<int> (r.getX() + halfW + kGap, r.getY(),
                                                halfW, halfH * 2 + kGap);
        mSpectroBotSec = juce::Rectangle<int> {};
        // Size the visualiser component to fit inside the section, leaving
        // 14 px at the top for the "SPECTROGRAM" header text drawSection draws.
        mSpectrogram.setBounds (mSpectroTopSec.reduced (4, 4).withTrimmedTop (12));
    }

    // ─────────────────────────────────────────────────────────────────────────
    // BOTTOM-RIGHT panel (Mod Editor) — 60% width
    // ─────────────────────────────────────────────────────────────────────────
    mModEditor.setBounds (mBotRightBounds.reduced (2));
}

//==============================================================================
juce::File HarmlessEditor::presetsDir()
{
    // P4b (2026-04-23): moved Roaming -> Documents per unified-folder layout.
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile ("BaySickDAW/Presets/Harmless");
}

// 2026-04-26: recursive XML-preset walker — folders become real cascading
// submenus.  Mirrors the synth/bass/drum picker UX.  Used by showPresetMenu.
static void addHarmlessPresetDirToMenu (juce::PopupMenu& menu,
                                         const juce::File& dir,
                                         juce::Array<juce::File>& presetXmls,
                                         int kPresetBase)
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
        addHarmlessPresetDirToMenu (sub, child, presetXmls, kPresetBase);
        if (sub.getNumItems() > 0)
            menu.addSubMenu (child.getFileName(), sub);
    }
    for (const auto& f : files)
    {
        const int id = kPresetBase + presetXmls.size();
        menu.addItem (id, f.getFileNameWithoutExtension());
        presetXmls.add (f);
    }
}

void HarmlessEditor::showPresetMenu()
{
    presetsDir().createDirectory();

    juce::PopupMenu menu;

    // 2026-04-26: factory presets now live in genre subfolders
    // (Modern Hip-Hop / Psytrance / etc).  Walk recursively so each folder
    // becomes a cascading submenu instead of being flattened.
    juce::Array<juce::File> presetXmls;
    addHarmlessPresetDirToMenu (menu, presetsDir(), presetXmls, /*kPresetBase=*/1);

    if (presetXmls.isEmpty())
        menu.addItem (-1, "(no presets installed)", false);
    menu.addSeparator();

    const int kSaveId = 1000, kInitId = 1001;
    menu.addItem (kSaveId, "Save preset...");
    menu.addItem (kInitId, "Init (reset to default)");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (mPresetBtn),
        [this, presetXmls] (int result)
        {
            if (result > 0 && result < 1000)
                loadPreset (presetXmls[result - 1]);
            else if (result == 1000)
            {
                auto* aw = new juce::AlertWindow ("Save Preset", "Enter a name:",
                                                  juce::AlertWindow::NoIcon);
                aw->addTextEditor ("name", "My Preset");
                aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
                aw->addButton ("Cancel", 0);
                aw->enterModalState (true, juce::ModalCallbackFunction::create (
                    [this, aw] (int r) {
                        if (r == 1) {
                            auto name = aw->getTextEditorContents ("name").trim();
                            if (name.isNotEmpty()) savePreset (name);
                        }
                        delete aw;
                    }), true);
            }
            else if (result == 1001)
            {
                auto freshTree = juce::ValueTree (mProc.apvts.state.getType());
                mProc.apvts.replaceState (freshTree);
            }
        });
}

void HarmlessEditor::savePreset (const juce::String& name)
{
    // 2026-04-26: user presets go into "My Presets/" subfolder.
    const auto dir = presetsDir().getChildFile ("My Presets");
    dir.createDirectory();
    const auto f = dir.getChildFile (name + ".xml");
    auto state   = mProc.apvts.copyState();
    if (auto xml = state.createXml()) xml->writeTo (f);
}

void HarmlessEditor::loadPreset (const juce::File& f)
{
    if (auto xml = juce::XmlDocument::parse (f))
        if (xml->hasTagName (mProc.apvts.state.getType()))
            mProc.apvts.replaceState (juce::ValueTree::fromXml (*xml));
}
