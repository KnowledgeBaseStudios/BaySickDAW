#include "BaySickSolsticeEditor.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)
#include "../AppPaths.h"
#include "../UserFileSave.h"
#include "../Standalone/SharedUI.h"   // VKnobAutomation hooks
#include "../Standalone/UndoBracket.h"

// ── Layout constants ──────────────────────────────────────────────────────────
// QA-Layout T7 (Specific-2): design size = the CONTENT area of Jeff's
// measured BaySickSolstice window minimum (1047x455 window, less the 26px title bar
// and 4px borders = 1039x421).  The old 960x620 was a TALL box -- the layout
// it drove is what collapsed in the real window.
static constexpr int kW       = 1039;
static constexpr int kH       = 421;
static constexpr int kGap     = 6;
// Jeff, 2026-08-04: every knob halved (44/32 -> 22/16).  The old sizes
// are what forced long control rows into tall boxes and produced both the
// overlap and the wasted space at 1047x455.
static constexpr int kKnob    = 22;
static constexpr int kKnobSm  = 16;

// QA-Layout T7 (Specific-2, Jeff's correction 2026-08-04): ground-up
// re-layout for the approved 1047x455 window.  What was wrong with the old
// map, both fixed here:
//   * DEAD SPACE -- a whole blank grid row, a blank bottom-left half and a
//     blank row-C half were reserved as "future space" while real sections
//     were squeezed.  Every cell now carries content.
//   * OVERFLOW -- wide item sets (Output, Timbre, FX) ran past their cell
//     edges.  layoutRow wraps now, and each section is sized to its content.
// Two bands: the top holds the per-voice engine (Output/Routing/mod/Unison/
// filters/Timbre), the bottom the performance + visual surfaces.
// Jeff, 2026-08-04 re-proportion.  Halved knobs and knob-ified faders mean every
// control section needs far less room, so the top band gives height back and the
// bottom columns give width back -- all of it to the Mod Editor, which was asked
// to be at least twice its old size and previously got only the ~22% left over.
static constexpr float kTopBandFrac = 0.46f;   // top band; bottom takes the rest
static constexpr float kTLFrac      = 0.34f;   // top: left column
static constexpr float kTMFrac      = 0.11f;   // top: Unison column; filters take the rest

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

// Paint a section label + recessed background (1px machined inset border)
static void drawSection (juce::Graphics& g, juce::Rectangle<int> r, const char* title)
{
    if (r.isEmpty()) return;
    const auto rf = r.toFloat();
    g.setColour (juce::Colour (BaySickSolsticeLAF::kPanel));
    g.fillRoundedRectangle  (rf, 3.f);
    g.setColour (juce::Colour (BaySickSolsticeLAF::kBorder));
    g.drawRoundedRectangle  (rf.reduced (0.5f), 3.f, 1.f);
    g.setColour (juce::Colour (BaySickSolsticeLAF::kTextDim));
    g.setFont   (juce::Font  (8.0f, juce::Font::bold));
    g.drawText  (title, r.getX() + 6, r.getY() + 3, r.getWidth() - 8, 12,
                 juce::Justification::centredLeft);
}

// Draw knob label below a component
static void knobLabel (juce::Graphics& g, const juce::Component& c, const char* t)
{
    // Jeff, 2026-08-04: the label box used to be the KNOB's width + 8, which at
    // 44px knobs was plenty and at 16px truncated every word past four letters
    // ("VOICES" -> "VOIC...").  Size the box to the TEXT and centre it on the
    // knob instead -- labels overhang their knob into the packing gap, which is
    // free space, rather than the word being cut.
    const juce::Font f (8.0f);
    g.setColour (juce::Colour (BaySickSolsticeLAF::kTextDim));
    g.setFont   (f);
    const int w  = f.getStringWidth (t) + 4;
    const int cx = c.getX() + c.getWidth() / 2;
    g.drawText  (t, cx - w / 2, c.getBottom() + 1, w, 10, juce::Justification::centred);
}

// ── Constructor ───────────────────────────────────────────────────────────────
BaySickSolsticeEditor::BaySickSolsticeEditor (BaySickSolsticeProcessor& p)
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
    // Curve points live per-(target, source, tab) inside BaySickSolsticeModRegistry -
    // no editor-local point vector needed.
    addAndMakeVisible (mModEditor);
    mModEditor.setRegistry (&p.getModRegistry());

    // S5 T2-M: central spectrogram visualiser.  Bounds set in resized()
    // (bottom band, its own column -- mSpectroSec).
    addAndMakeVisible (mSpectrogram);
    mSpectrogram.setSynth (&p.getSynth());

    // S4 Batch 4: install right-click "Modulate envelope..." hooks so any
    // modulatable BaySickSolstice knob exposes the item. GlobalAutoRightClick calls
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
                     &mGlideTime, &mLegatoLimit,
                     // D.4-Q1+Q2 (2026-05-01): timbre 2x2 stack + filter ADSRs
                     &mFlt1CutoffOfs, &mFlt2CutoffOfs, &mPartAMask, &mPartBMask,
                     &mFlt1A, &mFlt1D, &mFlt1S, &mFlt1R,
                     &mFlt2A, &mFlt2D, &mFlt2S, &mFlt2R })
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

    // Jeff, 2026-08-04: Unison PAN/PITCH/PHASE are knobs, not faders -- the
    // whole reason Unison held a full-height column was fader throw.
    setupRotary (mUnisonPan);   addAndMakeVisible (mUnisonPan);
    setupRotary (mUnisonPitch); addAndMakeVisible (mUnisonPitch);
    setupRotary (mUnisonPhase); addAndMakeVisible (mUnisonPhase);

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
            beginParamUndoGesture (mProc.apvts, agParamId); // Task 6 (12-iv)
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
    mPan.getProperties().set (BaySickSolsticeLAF::kBipolar, "true");

    mVelLinkBtn.setClickingTogglesState (true);
    mVelLinkBtn.getProperties().set ("switchToggle", true);
    addAndMakeVisible (mVelLinkBtn);

    mCutSelfBtn.setClickingTogglesState (true);
    mCutSelfBtn.getProperties().set ("switchToggle", true);
    mCutSelfBtn.setTooltip ("Cut Self: noteOn cuts any prior voice playing the same note.\n"
                            "Prevents phase stacking on rapid retrigs of the same note.");
    addAndMakeVisible (mCutSelfBtn);

    // Cut Self mode toggle (QA-CutSelfReview): Same Pitch vs Cut All.  Label
    // follows the toggle state.  BaySickSolstice is always poly.
    mCutSelfModeBtn.setClickingTogglesState (true);
    mCutSelfModeBtn.setTooltip ("Cut Self mode: Same Pitch cuts only the retriggered note (default);\n"
                                "Cut All cuts every ringing voice on each new note.");
    mCutSelfModeBtn.onStateChange = [this]
    { mCutSelfModeBtn.setButtonText (mCutSelfModeBtn.getToggleState() ? "CUT ALL" : "SAME PITCH"); };
    mCutSelfModeBtn.onStateChange();
    addAndMakeVisible (mCutSelfModeBtn);
    mPartABtn.setClickingTogglesState (true);
    mPartABtn.getProperties().set ("switchToggle", true);
    mPartBBtn.setClickingTogglesState (true);
    mPartBBtn.getProperties().set ("switchToggle", true);
    addAndMakeVisible (mPartABtn);
    addAndMakeVisible (mPartBBtn);
    // S3 part_sel: A/B buttons act as a 2-way radio group writing to part_sel.
    // The previously-bound mPartSelAtt slider stays attached to APVTS and the
    // buttons just push values to it (the slider IS addAndMakeVisible'd but is
    // never given bounds in the new layout, so it never draws). Initial state
    // pulls from APVTS via a syncFromApvts lambda invoked once after attachments.
    mPartABtn.onClick = [this, &p]
    {
        beginParamUndoGesture (p.apvts, p.pid ("part_sel")); // Task 6 (12-iv)
        if (auto* pp = dynamic_cast<juce::RangedAudioParameter*> (
                          p.apvts.getParameter (p.pid ("part_sel"))))
            pp->setValueNotifyingHost (pp->getNormalisableRange().convertTo0to1 (0.0f));
        mPartABtn.setToggleState (true,  juce::dontSendNotification);
        mPartBBtn.setToggleState (false, juce::dontSendNotification);
        rebindToPart (0);   // S3.5: swap attachments to Part A's params
    };
    mPartBBtn.onClick = [this, &p]
    {
        beginParamUndoGesture (p.apvts, p.pid ("part_sel")); // Task 6 (12-iv)
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

    // Jeff, 2026-08-04: the LFO depth faders are KNOBS now -- a fader needs
    // vertical throw, and that height is what the layout could not afford.
    for (auto* s : { &mLfoVel, &mLfoVol, &mLfoPitch })
        { setupRotary (*s); addAndMakeVisible (*s); }

    setupRotary (mStrumDirSlider); addAndMakeVisible (mStrumDirSlider);
    setupRotary (mStrumTime);      addAndMakeVisible (mStrumTime);
    setupRotary (mStrumTns);       addAndMakeVisible (mStrumTns);

    // QA-Layout T3: no internal header -- the hosting window's title strip
    // shows the name and mounts mPresetBtn (still owned + wired here).
    mPresetBtn.onClick = [this] { showPresetMenu(); };

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
    // D.4-Q1+Q2 (2026-05-01): timbre 2x2 stack + filter ADSR attachments.
    mFlt1OfsAtt      = std::make_unique<SliderAtt> (apvts, pid("flt1_cutoff_ofs"), mFlt1CutoffOfs);
    mFlt2OfsAtt      = std::make_unique<SliderAtt> (apvts, pid("flt2_cutoff_ofs"), mFlt2CutoffOfs);
    mPartAMaskAtt    = std::make_unique<SliderAtt> (apvts, pid("filter_mask_cutoff"),    mPartAMask);
    mPartBMaskAtt    = std::make_unique<SliderAtt> (apvts, pid("partB_filter_mask_cutoff"), mPartBMask);
    mFlt1AAtt        = std::make_unique<SliderAtt> (apvts, pid("flt_a"),          mFlt1A);
    mFlt1DAtt        = std::make_unique<SliderAtt> (apvts, pid("flt_d"),          mFlt1D);
    mFlt1SAtt        = std::make_unique<SliderAtt> (apvts, pid("flt_s"),          mFlt1S);
    mFlt1RAtt        = std::make_unique<SliderAtt> (apvts, pid("flt_r"),          mFlt1R);
    mFlt2AAtt        = std::make_unique<SliderAtt> (apvts, pid("flt2_a"),         mFlt2A);
    mFlt2DAtt        = std::make_unique<SliderAtt> (apvts, pid("flt2_d"),         mFlt2D);
    mFlt2SAtt        = std::make_unique<SliderAtt> (apvts, pid("flt2_s"),         mFlt2S);
    mFlt2RAtt        = std::make_unique<SliderAtt> (apvts, pid("flt2_r"),         mFlt2R);
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
    // Batch E #1 (2026-05-01): OCT + Hz are UI-only display toggles for the
    // FREQ pitch knob.  No APVTS attachment - they don't change DSP, just
    // toggle drag-snap and popup-display unit (semitones / octaves / Hz).
    auto applyPitchDisplayMode = [this]
    {
        const bool oct = mPitchOctBtn.getToggleState();
        const bool hz  = mPitchHzBtn .getToggleState();
        // Drag interval: octave snap (12 semis) when OCT is on, free 1-semi steps otherwise.
        mPitchFreq.setRange (-24.0, 24.0, oct ? 12.0 : 1.0);
        // Popup display: Hz mode shows absolute Hz at A4-anchored frequency.
        // OCT mode shows octave count.  Default = semitones with sign.
        mPitchFreq.textFromValueFunction = [oct, hz] (double v)
        {
            if (hz)
            {
                const double freqHz = 440.0 * std::pow (2.0, v / 12.0);
                if (freqHz >= 1000.0)
                    return juce::String (freqHz / 1000.0, 2) + " kHz";
                return juce::String (freqHz, 1) + " Hz";
            }
            if (oct)
            {
                const int oc = (int) std::round (v / 12.0);
                if (oc == 0) return juce::String ("0");
                return (oc > 0 ? juce::String ("+") : juce::String())
                       + juce::String (oc) + " oct";
            }
            const int st = (int) std::round (v);
            return (st >= 0 ? juce::String ("+") : juce::String())
                   + juce::String (st) + " st";
        };
        mPitchFreq.updateText();
    };
    mPitchOctBtn.onClick = applyPitchDisplayMode;
    mPitchHzBtn .onClick = applyPitchDisplayMode;
    // Apply once at construction so the textFromValue lambda is installed
    // before the user touches anything.
    applyPitchDisplayMode();
    // Bottom-left
    mPartSelAtt      = std::make_unique<SliderAtt> (apvts, pid("part_sel"),       mPartSel);
    mVolumeAtt       = std::make_unique<SliderAtt> (apvts, pid("volume"),         mVolume);
    mPanAtt          = std::make_unique<SliderAtt> (apvts, pid("pan"),            mPan);
    mVelLinkAtt      = std::make_unique<ButtonAtt> (apvts, pid("vel_link"),       mVelLinkBtn);
    mCutSelfAtt      = std::make_unique<ButtonAtt> (apvts, pid("cutSelf"),        mCutSelfBtn);
    mCutSelfModeAtt  = std::make_unique<ButtonAtt> (apvts, pid("cutSelfMode"),    mCutSelfModeBtn);
    mAmpAAtt         = std::make_unique<SliderAtt> (apvts, pid("amp_a"),          mAmpA);
    mAmpDAtt         = std::make_unique<SliderAtt> (apvts, pid("amp_d"),          mAmpD);
    mAmpSAtt         = std::make_unique<SliderAtt> (apvts, pid("amp_s"),          mAmpS);
    mAmpRAtt         = std::make_unique<SliderAtt> (apvts, pid("amp_r"),          mAmpR);
    mPhaseStartAtt   = std::make_unique<SliderAtt> (apvts, pid("phase_start"),    mPhaseStart);
    mPhaseRandAtt    = std::make_unique<SliderAtt> (apvts, pid("phase_rand"),     mPhaseRand);
    // 2026-04-30: T2-B LFO depth shortcuts restored after the S4 strip.
    // Three global sliders write LFO source depth on per-voice destinations
    // (vol → Volume target / pitch → Pitch target / vel → note-on velocity
    // scaling).  Bipolar -1..+1; replace semantics override any per-target
    // depth set in the in-player Mod Editor.
    mLfoVelAtt   = std::make_unique<SliderAtt> (apvts, pid ("lfo_vel"),   mLfoVel);
    mLfoVolAtt   = std::make_unique<SliderAtt> (apvts, pid ("lfo_vol"),   mLfoVol);
    mLfoPitchAtt = std::make_unique<SliderAtt> (apvts, pid ("lfo_pitch"), mLfoPitch);
    mStrumDirAtt     = std::make_unique<SliderAtt> (apvts, pid("strum_dir"),      mStrumDirSlider);
    mStrumTimeAtt    = std::make_unique<SliderAtt> (apvts, pid("strum_time"),     mStrumTime);
    mStrumTnsAtt     = std::make_unique<SliderAtt> (apvts, pid("strum_tns"),      mStrumTns);

    // T1d/T1e 2026-04-19: setComponentID + setTooltip on every attached slider
    // so GlobalAutoRightClick exposes the "Automate: ..." + "Type in value..."
    // menus and hover reveals the param name + units. ASCII-only strings.
    // QA-ApvtsAutomation: the stamped id is also the automation registry key.
    // QA-ModelShell TS3: the registration behind that key is made model-side at
    // engine creation, so these controls only stamp.
    auto wireMeta = [&p] (juce::Slider& s, const char* paramSuffix, const char* tip)
    {
        const juce::String id = p.pid (paramSuffix);
        s.setComponentID (id);
        s.setTooltip     (tip);
    };
    // Top-Left
    wireMeta (mTimbreBlend,    "timbre_blend",     "Timbre Blend - crossfades Part A toward Part B (0..1)");
    wireMeta (mPartALevel,     "partA_level",      "Voice A Level (0..1)");
    wireMeta (mPartBLevel,     "partB_level",      "Voice B Level (0..1)");
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
    // D.4-Q1+Q2 (2026-05-01): timbre 2x2 stack + filter ADSRs.
    wireMeta (mFlt1CutoffOfs,  "flt1_cutoff_ofs",          "Filter 1 Cutoff Offset - shifts F1 cutoff up/down by this amount (semitones)");
    wireMeta (mFlt2CutoffOfs,  "flt2_cutoff_ofs",          "Filter 2 Cutoff Offset - shifts F2 cutoff up/down by this amount (semitones)");
    wireMeta (mPartAMask,      "filter_mask_cutoff",       "Part A Timbre Filter Mask Cutoff (Hz) - limits which harmonics pass through Part A's timbre");
    wireMeta (mPartBMask,      "partB_filter_mask_cutoff", "Part B Timbre Filter Mask Cutoff (Hz) - limits which harmonics pass through Part B's timbre");
    wireMeta (mFlt1A,          "flt_a",                    "Filter 1 Envelope Attack (seconds)");
    wireMeta (mFlt1D,          "flt_d",                    "Filter 1 Envelope Decay (seconds)");
    wireMeta (mFlt1S,          "flt_s",                    "Filter 1 Envelope Sustain (0..1)");
    wireMeta (mFlt1R,          "flt_r",                    "Filter 1 Envelope Release (seconds)");
    wireMeta (mFlt2A,          "flt2_a",                   "Filter 2 Envelope Attack (seconds)");
    wireMeta (mFlt2D,          "flt2_d",                   "Filter 2 Envelope Decay (seconds)");
    wireMeta (mFlt2S,          "flt2_s",                   "Filter 2 Envelope Sustain (0..1)");
    wireMeta (mFlt2R,          "flt2_r",                   "Filter 2 Envelope Release (seconds)");
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
    wireMeta (mLfoRate,        "lfo_rate",         "LFO Rate (global) - cycle length for every target's LFO source");
    wireMeta (mLfoShape,       "lfo_shape",        "LFO Shape (global) - Sine / Triangle / Saw / Square");
    // Bottom-Left
    // QA-ApvtsAutomation (Jeff 2026-07-25): part_sel is editor view state -- it
    // picks which part the shared knobs edit and is never read by the DSP, since
    // both parts always render.  Deliberately NOT stamped: a componentID here
    // would advertise an Automate menu whose lane could never do anything
    // (matches docket 5=A, view/display selectors stay local).
    mPartSel.setTooltip ("Part Selector (A=0, B=1) - chooses which part the knobs edit");

    // QA-ApvtsAutomation Task 5 follow-up (Jeff 2026-07-25): the last stragglers.
    // All five are attached but were never stamped, so they offered no Automate
    // menu.  QA-ModelShell TS3: stamping only -- registration is model-side.
    // pluck_blur is deliberately absent: it is a Part A/B dual, stamped by
    // rebindToPart.
    auto wireBtn = [&p] (juce::Button& b, const char* paramName)
    {
        const juce::String id = p.pid (paramName);
        b.setComponentID (id);
    };
    wireBtn (mLegatoBtn,      "legato");
    wireBtn (mUnisonAltBtn,   "unison_alt");
    wireBtn (mVelLinkBtn,     "vel_link");
    wireBtn (mCutSelfBtn,     "cutSelf");
    wireBtn (mCutSelfModeBtn, "cutSelfMode");
    wireMeta (mVolume,         "volume",           "Master Volume (0..1)");
    wireMeta (mPan,            "pan",              "Master Pan (-1..+1)");
    wireMeta (mAmpA,           "amp_a",            "Amp Attack (seconds)");
    wireMeta (mAmpD,           "amp_d",            "Amp Decay (seconds)");
    wireMeta (mAmpS,           "amp_s",            "Amp Sustain (0..1)");
    wireMeta (mAmpR,           "amp_r",            "Amp Release (seconds)");
    wireMeta (mPhaseStart,     "phase_start",      "Phase Start position (0..1)");
    wireMeta (mPhaseRand,      "phase_rand",       "Phase Randomisation amount (0..1)");
    // 2026-04-30: T2-B LFO depth shortcuts restored.
    wireMeta (mLfoVel,         "lfo_vel",          "LFO Vel Depth (-1..+1) - global LFO scaling on note-on velocity");
    wireMeta (mLfoVol,         "lfo_vol",          "LFO Vol Depth (-1..+1) - global LFO depth on Volume target");
    wireMeta (mLfoPitch,       "lfo_pitch",        "LFO Pitch Depth (-1..+1) - global LFO depth on Pitch target");
    wireMeta (mStrumDirSlider, "strum_dir",        "Strum Direction (0 up, 1 down, 2 random)");
    wireMeta (mStrumTime,      "strum_time",       "Strum Time - total stagger (seconds)");
    wireMeta (mStrumTns,       "strum_tns",        "Strum Tension - curve (-1 end / 0 linear / +1 start)");

    // 2026-04-19 (S1.5b): mark discrete-mode multi-selectors as chicken-head
    // visual style. All other knobs use the default Time-effects filmstrip
    // with orange tint per Jeff's "knobs should be the knob from the time
    // effects LAF". Multi-selectors get the chicken-head pointer because
    // they pick from a small fixed set of options (mode dropdowns).
    mPrismMode      .getProperties().set (BaySickSolsticeLAF::kKnobVariant, "chickenHead");
    mUnisonType     .getProperties().set (BaySickSolsticeLAF::kKnobVariant, "chickenHead");
    mStrumDirSlider .getProperties().set (BaySickSolsticeLAF::kKnobVariant, "chickenHead");
    mPitchFreqFrac  .getProperties().set (BaySickSolsticeLAF::kKnobVariant, "chickenHead");   // SLA-Impl
    mLfoShape       .getProperties().set (BaySickSolsticeLAF::kKnobVariant, "chickenHead");   // S2 T2-B

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

    // QA-ApvtsAutomation (Jeff 2026-07-25): Part A and Part B both render at all
    // times -- Part B is a second layer, not an alternate mode -- so each part's
    // param owns an independent automation lane and both can play at once.  That
    // requires targeting the PARAM rather than the shared knob: one knob is
    // time-shared between the two params, so a knob-driven applicator would
    // write whichever part happens to be bound and collapse both lanes onto the
    // same target.
    //
    // QA-ModelShell TS3 (2026-07-27): the registration that guaranteed this
    // moved out of the editor entirely.  The model walks the engine's whole
    // parameter list at creation, so BOTH part ids get param-targeting
    // applicators the same way -- and unlike the pair that used to live here,
    // they are not guarded by a slider that Part A/B rebinding destroys and
    // rebuilds.  The A/B semantics are preserved by construction, not by this
    // function.

    if (auto* pp = p.apvts.getRawParameterValue (p.pid ("part_sel")))
        mActivePart = (pp->load() > 0.5f) ? 1 : 0;
    rebindToPart (mActivePart);

    // QA-ManualPress M-4c: manual callout anchors.  Section-level callouts are
    // painted frames with no container, so they are declared in resized() from
    // the same rects paint() draws; these five name one control each.
    mPartABtn   .getProperties().set (kDotAnchor, "BSSOL-8");
    mTimbreWavA .getProperties().set (kDotAnchor, "BSSOL-9");
    mTimbreBlend.getProperties().set (kDotAnchor, "BSSOL-10");
    mPartALevel .getProperties().set (kDotAnchor, "BSSOL-11");
    mUnisonType .getProperties().set (kDotAnchor, "BSSOL-26");

    // Slider double-click returns each param's FACTORY default (the helper reads
    // getDefaultValue) -- standard knob behavior, independent of the loaded patch.
    setSliderDoubleClickDefaultsFromApvts (*this, mProc.apvts);
    mProc.apvts.state.addListener (this);
}

// 2026-04-19 (S3.5): swap the dual attachments to the chosen part. Destroys
// the current attachment + creates a new one against the per-part param ID.
// Slider values auto-sync from the new APVTS values (JUCE attachment behaviour).
void BaySickSolsticeEditor::rebindToPart (int part)
{
    mActivePart = juce::jlimit (0, 1, part);
    auto& apvts = mProc.apvts;
    for (auto& d : mDualSliders)
    {
        d.current.reset();
        d.current = std::make_unique<SliderAtt> (
            apvts, mActivePart == 0 ? d.paramA : d.paramB, *d.slider);
        // QA-ApvtsAutomation: the componentID tracks the visible part so
        // right-click Automate makes a lane for the part being edited. Both
        // parts stay independently automatable -- applicators for BOTH ids are
        // registered once at construction and are not touched by the rebind.
        d.slider->setComponentID (mActivePart == 0 ? d.paramA : d.paramB);
    }
    for (auto& d : mDualButtons)
    {
        d.current.reset();
        d.current = std::make_unique<ButtonAtt> (
            apvts, mActivePart == 0 ? d.paramA : d.paramB, *d.button);
        d.button->setComponentID (mActivePart == 0 ? d.paramA : d.paramB);
    }
}

BaySickSolsticeEditor::~BaySickSolsticeEditor()
{
    mProc.apvts.state.removeListener (this);
    // Clear the right-click "Modulate envelope..." hooks we installed. Multi-
    // instance caveat (last-registered wins) is deferred to T3-ModMatrixAutomation.
    VKnobAutomation::sShouldOfferModulate = nullptr;
    VKnobAutomation::sOnModulateEnvelope  = nullptr;
    setLookAndFeel (nullptr);
}

void BaySickSolsticeEditor::valueTreeRedirected (juce::ValueTree& tree)
{
    if (tree != mProc.apvts.state) return;
    setSliderDoubleClickDefaultsFromApvts (*this, mProc.apvts);
}

//==============================================================================
void BaySickSolsticeEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (BaySickSolsticeLAF::kChassis));

    // QA-A (2026-05-09): header bar paint moved to BaySickTitleBar::paint()
    // (background + divider + the bloomed engine title via the bloom flag).

    // ── Major panel backgrounds (recessed) ────────────────────────────────────
    auto drawPanel = [&] (juce::Rectangle<int> r) {
        g.setColour (juce::Colour (0xFF0E0E10));
        g.fillRoundedRectangle (r.toFloat(), 3.f);
        g.setColour (juce::Colour (BaySickSolsticeLAF::kBorder));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 3.f, 1.f);
    };

    drawPanel (mTopLeftBounds);
    drawPanel (mTopMidBounds);
    drawPanel (mTopRightBounds);
    drawPanel (mBotBandBounds);

    // ── Section labels (QA-Layout T7 map) ─────────────────────────────────────
    // Top-left column.
    drawSection (g, mGlobalSec,    "OUTPUT");
    drawSection (g, mTremSec,      "TREMOLO");
    drawSection (g, mRoutingSec,   "ROUTING");
    drawSection (g, mVibLegatoSec, "VIBRATO / LEGATO");

    // Top-middle column.
    drawSection (g, mUnisonSec,    "UNISON");

    // Top-right column.
    // T16: one box per filter, its ADSR included -- mFlt*AdsrSec is emptied by
    // resized() so these two draw nothing.
    drawSection (g, mFlt1Sec,      "FILTER 1 + ADSR");
    drawSection (g, mFlt1AdsrSec,  "FILTER 1 ADSR");
    drawSection (g, mFlt2Sec,      "FILTER 2 + ADSR");
    drawSection (g, mFlt2AdsrSec,  "FILTER 2 ADSR");
    drawSection (g, mTimbreSec,    "TIMBRE");

    // Bottom band.  The XYZ pad and Mod Editor paint their own frames, so
    // they take no section header here.
    drawSection (g, mPitchSec,     "PITCH");
    drawSection (g, mLFOSec,       "LFO MOD");
    drawSection (g, mStrumSec,     "STRUM");
    drawSection (g, mBlurPrismSec, "BLUR / PRISM");
    drawSection (g, mAmpEnvSec,    "AMP ENV / PHASE");
    drawSection (g, mFXSec,        "FX - PLUCK / PHASER / EQ");
    drawSection (g, mSpectroSec,   "SPECTROGRAM");

    // ── Knob labels - Top-Left ────────────────────────────────────────────────
    knobLabel (g, mTimbreWavA,   "PART A");
    knobLabel (g, mTimbreWavB,   "PART B");
    knobLabel (g, mTimbreBlend,  "MIX");
    knobLabel (g, mPartALevel,   "VOICE A");
    knobLabel (g, mPartBLevel,   "VOICE B");
    knobLabel (g, mBrownian,     "BROWN");
    // D.4-Q1+Q2 (2026-05-01): filter-offset / part-mask + filter ADSR labels.
    knobLabel (g, mFlt1CutoffOfs, "F1 OFS");
    knobLabel (g, mFlt2CutoffOfs, "F2 OFS");
    knobLabel (g, mPartAMask,     "A MASK");
    knobLabel (g, mPartBMask,     "B MASK");
    knobLabel (g, mFlt1A,         "ATK");
    knobLabel (g, mFlt1D,         "DEC");
    knobLabel (g, mFlt1S,         "SUS");
    knobLabel (g, mFlt1R,         "REL");
    knobLabel (g, mFlt2A,         "ATK");
    knobLabel (g, mFlt2D,         "DEC");
    knobLabel (g, mFlt2S,         "SUS");
    knobLabel (g, mFlt2R,         "REL");
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
    // 2026-04-30: T2-B LFO depth shortcuts - labels under each slider, same
    // knobLabel convention as everything else in the editor.
    knobLabel (g, mLfoVel,   "VEL");
    knobLabel (g, mLfoVol,   "VOL");
    knobLabel (g, mLfoPitch, "PITCH");
    knobLabel (g, mStrumDirSlider,"DIR");
    knobLabel (g, mStrumTime,    "TIME");
    knobLabel (g, mStrumTns,     "TNS");
}

//==============================================================================
void BaySickSolsticeEditor::resized()
{
    // QA-Layout T3: the internal 32px title bar is gone -- content starts at 0.
    auto bounds = getLocalBounds();
    bounds.reduce (kGap, 0);
    bounds.removeFromTop (kGap / 2);

    const int contentW = bounds.getWidth();

    // ── QA-Layout T7 (Specific-2) region map ─────────────────────────────────
    //   TOP band (52%)
    //     LEFT (34%)        MID (13%)   RIGHT (rest)
    //     Output            Unison      Filter 1   | Filter 1 ADSR
    //     Tremolo | Routing (full ht)   Filter 2   | Filter 2 ADSR
    //     Vibrato / Legato              Timbre (full width)
    //   BOTTOM band (rest), six columns
    //     Pitch      | Strum | Blur-Prism | FX | Spectrogram | Mod Editor
    //     LFO Mod    | XYZ   | Amp Env    |    |             |
    auto topBand = bounds.removeFromTop (int (bounds.getHeight() * kTopBandFrac));
    bounds.removeFromTop (kGap);
    mBotBandBounds = bounds;

    mTopLeftBounds  = topBand.removeFromLeft (int (contentW * kTLFrac));
    topBand.removeFromLeft (kGap);
    mTopMidBounds   = topBand.removeFromLeft (int (contentW * kTMFrac));
    topBand.removeFromLeft (kGap);
    mTopRightBounds = topBand;

    // Distribute a list of (component, width, height) across a rect, with
    // equal gaps at start/between/end.
    //
    // QA-Layout T7 (Specific-2): the single-row version ran wide item sets
    // straight past the cell edge (Output, Timbre and FX all overflowed at
    // the approved 1047x455 window -- the visible "doesn't fit" symptom).
    // Items now WRAP into as many rows as the cell width needs and the block
    // centres vertically, so a section fits its cell at any width.
    auto layoutRow = [] (juce::Rectangle<int> r,
                          std::initializer_list<std::tuple<juce::Component*, int, int>> items)
    {
        const int n = (int) items.size();
        if (n <= 0 || r.getWidth() <= 0) return;
        const auto* it = items.begin();
        constexpr int kMinGap = 4;

        // Greedy wrap: start a new row when the next item would not fit.
        std::vector<std::pair<int, int>> rows;   // (firstIdx, count)
        {
            int first = 0, used = 0;
            for (int i = 0; i < n; ++i)
            {
                const int iw   = std::get<1> (it[i]);
                const int need = (i > first ? kMinGap : 0) + iw;
                if (i > first && used + need > r.getWidth())
                {
                    rows.emplace_back (first, i - first);
                    first = i;
                    used  = iw;
                }
                else used += need;
            }
            rows.emplace_back (first, n - first);
        }

        std::vector<int> rowH;
        rowH.reserve (rows.size());
        int itemsH = 0;
        for (auto& rw : rows)
        {
            int h = 0;
            for (int i = rw.first; i < rw.first + rw.second; ++i)
                h = juce::jmax (h, std::get<2> (it[i]));
            rowH.push_back (h);
            itemsH += h;
        }
        const int gapsH = kMinGap * ((int) rows.size() - 1);

        // Jeff, 2026-08-04: wrapping alone was not enough.  A block taller than
        // its cell used to CENTRE and spill past both edges, which is what put
        // FILTER 1 on top of FILTER 2 and pushed the Amp Env RAND row into its
        // neighbour.  Scale the whole block down to fit instead.  Width scales
        // with height so the knobs stay round -- and since the factor is <= 1,
        // items only get narrower, so the wrap decided above stays valid.
        const double avail = juce::jmax (1, r.getHeight() - gapsH);
        const double scale = itemsH > 0 ? juce::jmin (1.0, avail / (double) itemsH) : 1.0;
        auto sc = [scale] (int v) { return juce::jmax (1, (int) std::lround (v * scale)); };

        int blockH = gapsH;
        for (auto& h : rowH) { h = sc (h); blockH += h; }

        int y = r.getY() + juce::jmax (0, (r.getHeight() - blockH) / 2);
        for (size_t ri = 0; ri < rows.size(); ++ri)
        {
            const auto& rw = rows[ri];
            int itemsW = 0;
            for (int i = rw.first; i < rw.first + rw.second; ++i)
                itemsW += sc (std::get<1> (it[i]));

            // Jeff, 2026-08-04: DISTRIBUTE across the cell -- equal gap before,
            // between and after -- so knobs breathe inside their box.
            //
            // This is deliberately NOT the packing an earlier pass used.  The
            // reason packing was reached for (halving the knobs just widened
            // every gap and bought no space) no longer applies: a section's
            // WIDTH is now decided by its content via the natural() sizing in
            // the bottom band, so spreading inside that width costs nothing and
            // the box does not grow.  Boxes with their own layout -- Routing,
            // the XYZ/MOD pad, the Mod Editor's knob row -- do not come through
            // here and keep their packed spacing.
            const int gap = juce::jmax (0, r.getWidth() - itemsW) / (rw.second + 1);
            int x = r.getX() + gap;
            for (int i = rw.first; i < rw.first + rw.second; ++i)
            {
                auto* c = std::get<0> (it[i]);
                const int w = sc (std::get<1> (it[i]));
                const int h = sc (std::get<2> (it[i]));
                c->setBounds (x, y + (rowH[ri] - h) / 2, w, h);
                x += w + gap;
            }
            y += rowH[ri] + kMinGap;
        }
    };

    // ═════════════════════════════════════════════════════════════════════════
    // S5 section internals (2026-04-20), rehung on the T7 four-column frame:
    //
    //   COL 1           COL 2      COL 3 (grid)       COL 4
    //   Output|Route    Unison     Filter1 | Filter2  Mod Editor
    //   Trem  |VibLeg   Pitch      Timbre  | BlurPrsm ----------
    //   blank |Strum    LFO Mod    AmpEnv  | FX       Spectrogram
    //         |XYZ
    // ═════════════════════════════════════════════════════════════════════════

    // ─────────────────────────────────────────────────────────────────────────
    // TOP-LEFT column - Output (full width) / Tremolo | Routing / Vib-Legato
    // (full width).  T7: the old half-width Output cell overflowed its six
    // fixed-width items; full-width rows fit them with room at the approved
    // size.  Strum + XYZ moved to the bottom band.
    // ─────────────────────────────────────────────────────────────────────────
    {
        auto r = mTopLeftBounds.reduced (6, 4);
        const int secGap = 3;
        const int rowH   = (r.getHeight() - 2 * secGap) / 3;

        // Row A: Output, full width.
        mGlobalSec = r.removeFromTop (rowH);
        layoutRow (mGlobalSec.reduced (4, 16), {
            { &mVolume,      kKnobSm, kKnobSm },
            { &mPan,         kKnobSm, kKnobSm },
            { &mVelLinkBtn,  34,      18      },
            { &mCutSelfBtn,  56,      18      },
            { &mCutSelfModeBtn, 74,   18      },   // QA-CutSelfReview: Same Pitch / Cut All
            { &mAutoGainBtn, 52,      18      },   // D.4-Q1+Q2: moved here from Timbre cell
        });
        r.removeFromTop (secGap);

        // Row B: Tremolo | Routing Matrix.
        auto rowB = r.removeFromTop (rowH);
        const int halfW = (rowB.getWidth() - secGap) / 2;
        mTremSec    = rowB.withWidth (halfW);
        mRoutingSec = rowB.withX (rowB.getX() + halfW + secGap)
                          .withWidth (rowB.getWidth() - halfW - secGap);
        layoutRow (mTremSec.reduced (4, 16), {
            { &mTremWavBtn, 32,      32      },
            { &mTremDepth,  kKnobSm, kKnobSm },
            { &mTremSpeed,  kKnobSm, kKnobSm },
            { &mTremGap,    kKnobSm, kKnobSm },
        });
        mRoutingMatrix.setBounds (mRoutingSec.reduced (4, 16));
        r.removeFromTop (secGap);

        // Row C: Vibrato / Legato, full width.
        mVibLegatoSec = r;
        layoutRow (mVibLegatoSec.reduced (4, 16), {
            { &mVibWavBtn,   32,      32      },
            { &mVibDepth,    kKnobSm, kKnobSm },
            { &mVibSpeed,    kKnobSm, kKnobSm },
            { &mVibEnv,      kKnobSm, kKnobSm },
            { &mGlideTime,   kKnobSm, kKnobSm },
            { &mLegatoLimit, kKnobSm, kKnobSm },
            { &mLegatoBtn,   42,      20      },
        });
    }

    // ─────────────────────────────────────────────────────────────────────────
    // TOP-MIDDLE column - Unison, alone at full band height.  T7: Unison is
    // the one section whose controls are inherently VERTICAL (three faders
    // under the voices knob), so it gets the height rather than sharing a
    // stack with Pitch + LFO Mod (which are horizontal rows and moved to the
    // bottom band).  The faders keep real throw instead of the ~75%-of-a-
    // third they had, and the derived "savings" arithmetic that produced the
    // old heights is gone with it.
    // ─────────────────────────────────────────────────────────────────────────
    {
        mUnisonSec = mTopMidBounds.reduced (6, 4);
        auto ur = mUnisonSec.reduced (6, 10);
        constexpr int kLblBand = 11;
        // VOICES + the type combo + ALT, then PAN/PITCH/PHASE as a knob row.
        auto r1 = ur.removeFromTop (kKnobSm + kLblBand);
        layoutRow (r1.withTrimmedBottom (kLblBand), { { &mUnisonVoices, kKnobSm, kKnobSm } });
        mUnisonType  .setBounds (ur.getX(), ur.getY(), ur.getWidth(), 18);
        ur.removeFromTop (20);
        mUnisonAltBtn.setBounds (ur.getX(), ur.getY(), ur.getWidth(), 18);
        ur.removeFromTop (20);
        layoutRow (ur.withTrimmedBottom (kLblBand), {
            { &mUnisonPan,   kKnobSm, kKnobSm },
            { &mUnisonPitch, kKnobSm, kKnobSm },
            { &mUnisonPhase, kKnobSm, kKnobSm },
        });
    }

    // ─────────────────────────────────────────────────────────────────────────
    // TOP-RIGHT column - 3 rows, no blanks (T7).
    //   D.4-Q1+Q2 (2026-05-01): Filter 2 sits BELOW Filter 1, each with its
    //   ADSR box to the right.
    //   R1: Filter 1 | Filter 1 ADSR
    //   R2: Filter 2 | Filter 2 ADSR
    //   R3: Timbre (A/B strip + the 2x2 offsets/masks stack), FULL width --
    //       the widest control set in the editor, and the old half-width cell
    //       is exactly what overflowed.  Blur/Prism, Amp Env and FX moved to
    //       the bottom band; the two blank "future" rows are gone.
    // ─────────────────────────────────────────────────────────────────────────
    {
        auto r = mTopRightBounds.reduced (4, 4);
        const int cellGap = 3;
        const int rowH  = (r.getHeight() - cellGap * 2) / 3;
        const int halfW = (r.getWidth()  - cellGap) / 2;

        auto cellAt = [&] (int row, int col) -> juce::Rectangle<int>
        {
            const int y = r.getY() + row * (rowH + cellGap);
            const int x = r.getX() + col * (halfW + cellGap);
            return { x, y, halfW, rowH };
        };

        // ADSR-cell layout helper: single row of 4 A/D/S/R knobs (matches
        // the existing layoutRow style used by Amp Env elsewhere).
        auto layoutAdsr = [&] (juce::Rectangle<int> cell, juce::Slider& a,
                                juce::Slider& d, juce::Slider& s, juce::Slider& rk)
        {
            layoutRow (cell.reduced (3, 12), {
                { &a,  kKnobSm, kKnobSm },
                { &d,  kKnobSm, kKnobSm },
                { &s,  kKnobSm, kKnobSm },
                { &rk, kKnobSm, kKnobSm },
            });
        };

        // Jeff, 2026-08-04: ONE box per filter.  Splitting each filter from its
        // own ADSR into two boxes doubled the chrome and the padding for eight
        // knobs that belong together, and left both halves half-empty.  The
        // filter row (combo + 4 knobs) takes the left, its ADSR the right, in a
        // single section spanning the full width.  mFlt*AdsrSec is emptied so
        // paint() draws no second frame.
        auto filterCell = [&] (int row, juce::Rectangle<int>& sec,
                               BaySickSolsticeFilterRow& fltRow,
                               juce::Slider& a, juce::Slider& d,
                               juce::Slider& s, juce::Slider& rk)
        {
            sec = cellAt (row, 0).withWidth (r.getWidth());
            auto inner = sec.reduced (3, 12);
            // The filter row needs the combo plus four knobs; the ADSR four
            // more.  Split by content, not down the middle.
            fltRow.setBounds (inner.removeFromLeft (inner.getWidth() * 9 / 16));
            layoutRow (inner, {
                { &a,  kKnobSm, kKnobSm },
                { &d,  kKnobSm, kKnobSm },
                { &s,  kKnobSm, kKnobSm },
                { &rk, kKnobSm, kKnobSm },
            });
        };

        filterCell (0, mFlt1Sec, mFilter1Row, mFlt1A, mFlt1D, mFlt1S, mFlt1R);
        filterCell (1, mFlt2Sec, mFilter2Row, mFlt2A, mFlt2D, mFlt2S, mFlt2R);
        mFlt1AdsrSec = {};
        mFlt2AdsrSec = {};

        // R3: Timbre, FULL width -- the widest set in the editor, and the old
        // half cell is what overflowed.  T7 also DISSOLVES the cramped 2x2
        // stack that held the D.4-Q1+Q2 filter-offset + part-mask knobs: with
        // the full row available they sit inline at full knob size instead of
        // shrinking to ~12px in a half-height sub-cell.
        mTimbreSec = cellAt (2, 0).withWidth (r.getWidth());
        layoutRow (mTimbreSec.reduced (3, 12), {
            { &mPartABtn,       22,      18      },
            { &mPartBBtn,       22,      18      },
            { &mTimbreWavA,     28,      28      },
            { &mTimbreWavB,     28,      28      },
            { &mTimbreBlend,    kKnobSm, kKnobSm },
            { &mPartALevel,     kKnobSm, kKnobSm },
            { &mPartBLevel,     kKnobSm, kKnobSm },
            { &mBrownian,       kKnobSm, kKnobSm },
            { &mFlt1CutoffOfs,  kKnobSm, kKnobSm },
            { &mFlt2CutoffOfs,  kKnobSm, kKnobSm },
            { &mPartAMask,      kKnobSm, kKnobSm },
            { &mPartBMask,      kKnobSm, kKnobSm },
        });
    }

    // ─────────────────────────────────────────────────────────────────────────
    // BOTTOM band - six columns, every one carrying content (T7).
    //   Pitch      | Strum | Blur/Prism | FX | Spectrogram | Mod Editor
    //   LFO Mod    | XYZ   | Amp Env    |    |             |
    // The horizontal-row sections (Pitch, LFO Mod, Blur/Prism, Amp Env, FX)
    // moved here off the top band; XYZ + Spectrogram + Mod Editor are the
    // surfaces that actually want area, so they get full column height.
    // ─────────────────────────────────────────────────────────────────────────
    {
        auto band = mBotBandBounds;
        constexpr int kColGap  = 3;

        // Jeff, 2026-08-04: HORIZONTAL STRIPS, not narrow vertical columns.
        // Every one of these sections is a single row of knobs, and squeezing
        // them into ~13%-wide columns forced layoutRow to wrap them into two or
        // three cramped lines -- the overlap.  Each is now a full-width strip in
        // a left stack, so a row stays a row.  The two surfaces that genuinely
        // want AREA (the XYZ pad and the Spectrogram) share a column beside the
        // stack, and the Mod Editor keeps the right-hand half.
        auto right    = band.removeFromRight (int (band.getWidth() * 0.38f));
        band.removeFromRight (kGap);
        // Jeff, 2026-08-04: the Spectrogram is a VISUAL surface and the last
        // pass starved it.  This column is wider now, and the split favours the
        // Spectrogram over the XYZ pad rather than halving them.
        auto surfaces = band.removeFromRight (int (band.getWidth() * 0.30f));
        band.removeFromRight (kGap);

        // Right: Mod Editor, full height of the band.
        mModSec = right;
        mModEditor.setBounds (mModSec.reduced (2));

        // Middle: Spectrogram ALONE, full column height (Jeff, 2026-08-04).  The
        // XYZ/MOD pad moved into the left stack, so nothing shares this column
        // and the scope gets the whole of it.
        mSpectroSec = surfaces;
        mSpectrogram.setBounds (mSpectroSec.reduced (4, 4).withTrimmedTop (12));

        // Left: strips SIZED TO CONTENT, so two short sections share one row
        // rather than each burning a whole row (Jeff, 2026-08-04).  A section's
        // natural width is its packed row plus the frame padding; a row splits
        // between its sections in proportion to those widths.
        {
            constexpr int kPackGap = 10;
            constexpr int kSecPad  = 16;
            auto natural = [&] (std::initializer_list<int> widths)
            {
                int w = 0, n = 0;
                for (int v : widths) { w += v; ++n; }
                return w + kPackGap * juce::jmax (0, n - 1) + kSecPad;
            };

            const int nRows  = 4;
            const int stripH = (band.getHeight() - kColGap * (nRows - 1)) / nRows;
            auto takeStrip = [&] () -> juce::Rectangle<int>
            {
                auto s = band.removeFromTop (stripH);
                band.removeFromTop (kColGap);
                return s;
            };
            auto splitByContent = [&] (juce::Rectangle<int> strip, int wA, int wB,
                                       juce::Rectangle<int>& a, juce::Rectangle<int>& b)
            {
                const int avail = strip.getWidth() - kColGap;
                a = strip.removeFromLeft (juce::jmax (40, avail * wA / juce::jmax (1, wA + wB)));
                strip.removeFromLeft (kColGap);
                b = strip;
            };

            // Row 1: PITCH + LFO MOD.
            {
                auto strip = takeStrip();
                const int wP = natural ({ kKnobSm, kKnobSm, kKnobSm, 24, 20 });
                const int wL = natural ({ kKnobSm, kKnobSm, 52, kKnobSm, kKnobSm, kKnobSm });
                splitByContent (strip, wP, wL, mPitchSec, mLFOSec);

                layoutRow (mPitchSec.reduced (4, 12), {
                    { &mPitchFreq,     kKnobSm, kKnobSm },
                    { &mPitchDetune,   kKnobSm, kKnobSm },
                    { &mPitchFreqFrac, kKnobSm, kKnobSm },
                    { &mPitchOctBtn,   24,      18      },
                    { &mPitchHzBtn,    20,      18      },
                });
                layoutRow (mLFOSec.reduced (4, 12), {
                    { &mLfoRate,     kKnobSm, kKnobSm },
                    { &mLfoShape,    kKnobSm, kKnobSm },
                    { &mLfoTempoBtn, 52,      20      },
                    { &mLfoVel,      kKnobSm, kKnobSm },
                    { &mLfoVol,      kKnobSm, kKnobSm },
                    { &mLfoPitch,    kKnobSm, kKnobSm },
                });
            }

            // Row 2: STRUM + FX (Jeff, 2026-08-04 -- FX and Blur/Prism swapped
            // places, so the nine-control set shares this row and Blur/Prism
            // drops into the half-width stack below).
            {
                auto strip = takeStrip();
                const int wS = natural ({ kKnobSm, kKnobSm, kKnobSm });
                const int wF = natural ({ kKnobSm, 24, kKnobSm, kKnobSm, kKnobSm,
                                          kKnobSm, kKnobSm, kKnobSm, kKnobSm });
                splitByContent (strip, wS, wF, mStrumSec, mFXSec);

                layoutRow (mStrumSec.reduced (4, 12), {
                    { &mStrumDirSlider, kKnobSm, kKnobSm },
                    { &mStrumTime,      kKnobSm, kKnobSm },
                    { &mStrumTns,       kKnobSm, kKnobSm },
                });
                layoutRow (mFXSec.reduced (4, 12), {
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
            }

            // Rows 3+4 (Jeff, 2026-08-04): AMP ENV / PHASE and BLUR / PRISM take
            // only HALF the width they had and stack in the left half; the half
            // that frees up carries the XYZ/MOD pad across both rows, which is
            // what let the Spectrogram claim its whole column.
            {
                auto block = band;                      // whatever height remains
                auto leftHalf = block.removeFromLeft ((block.getWidth() - kColGap) / 2);
                block.removeFromLeft (kColGap);

                mXYZSec = block;                        // pad draws its own frame
                mXYZPad.setBounds (mXYZSec.reduced (2));

                mAmpEnvSec = leftHalf.removeFromTop ((leftHalf.getHeight() - kColGap) / 2);
                leftHalf.removeFromTop (kColGap);
                mBlurPrismSec = leftHalf;

                layoutRow (mAmpEnvSec.reduced (4, 12), {
                    { &mAmpA,       kKnobSm, kKnobSm },
                    { &mAmpD,       kKnobSm, kKnobSm },
                    { &mAmpS,       kKnobSm, kKnobSm },
                    { &mAmpR,       kKnobSm, kKnobSm },
                    { &mPhaseStart, kKnobSm, kKnobSm },
                    { &mPhaseRand,  kKnobSm, kKnobSm },
                });
                layoutRow (mBlurPrismSec.reduced (4, 12), {
                    { &mBlurSize,  kKnobSm, kKnobSm },
                    { &mBlurTime,  kKnobSm, kKnobSm },
                    { &mBlurHarm,  kKnobSm, kKnobSm },
                    { &mPrismAmt,  kKnobSm, kKnobSm },
                    { &mPrismMode, kKnobSm, kKnobSm },
                });
            }
        }
    }

    // QA-ManualPress M-4c: manual callout anchors for the SECTION callouts.  A
    // section here is a painted frame, not a component, so the dot anchors to
    // the same rect paint() draws and is re-declared whenever the layout moves.
    {
        juce::String anchors;
        auto sec = [&anchors] (const char* code, juce::Rectangle<int> r)
        {
            if (r.isEmpty()) return;
            if (anchors.isNotEmpty()) anchors << ";";
            anchors << code << "@" << r.getX() << "," << r.getY() << ","
                    << r.getWidth() << "," << r.getHeight();
        };
        sec ("BSSOL-1",  mGlobalSec);
        sec ("BSSOL-2",  mTremSec);
        sec ("BSSOL-3",  mRoutingSec);
        sec ("BSSOL-4",  mVibLegatoSec);
        sec ("BSSOL-5",  mUnisonSec);
        sec ("BSSOL-6",  mFlt1Sec);
        sec ("BSSOL-7",  mFlt2Sec);
        sec ("BSSOL-12", mPitchSec);
        sec ("BSSOL-13", mLFOSec);
        sec ("BSSOL-14", mStrumSec);
        sec ("BSSOL-15", mFXSec);
        sec ("BSSOL-16", mAmpEnvSec);
        sec ("BSSOL-17", mBlurPrismSec);
        sec ("BSSOL-18", mXYZSec);
        sec ("BSSOL-19", mSpectroSec);
        getProperties().set (kDotAnchor, anchors);
    }
}

//==============================================================================
juce::File BaySickSolsticeEditor::presetsDir()
{
    // P4b (2026-04-23): moved Roaming -> Documents per unified-folder layout.
    return AppPaths::appRoot().getChildFile ("Presets/BaySickSolstice");
}

// 2026-04-26: recursive XML-preset walker - folders become real cascading
// submenus.  Mirrors the synth/bass/drum picker UX.  Used by showPresetMenu.
static void addBaySickSolsticePresetDirToMenu (juce::PopupMenu& menu,
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
        addBaySickSolsticePresetDirToMenu (sub, child, presetXmls, kPresetBase);
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

void BaySickSolsticeEditor::showPresetMenu()
{
    presetsDir().createDirectory();

    juce::PopupMenu menu;

    // 2026-04-26: factory presets now live in genre subfolders
    // (Modern Hip-Hop / Psytrance / etc).  Walk recursively so each folder
    // becomes a cascading submenu instead of being flattened.
    juce::Array<juce::File> presetXmls;
    addBaySickSolsticePresetDirToMenu (menu, presetsDir(), presetXmls, /*kPresetBase=*/1);

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
                        if (r == 1) savePreset (aw->getTextEditorContents ("name"));
                        delete aw;
                    }), true);
            }
            else if (result == 1001)
            {
                auto freshTree = juce::ValueTree (mProc.apvts.state.getType());
                mProc.apvts.replaceStateKeepingUndoHistory (freshTree, "Init Patch");   // ruling 3a
            }
        });
}

void BaySickSolsticeEditor::savePreset (const juce::String& name)
{
    // 2026-04-26: user presets go into "My Presets/" subfolder.
    const auto dir = presetsDir().getChildFile ("My Presets");
    auto state = mProc.apvts.copyState();
    if (auto xml = state.createXml())
        UserFileSave::writeXmlAsync (dir, name, *xml, {});
}

void BaySickSolsticeEditor::loadPreset (const juce::File& f)
{
    bool ok = false;
    if (auto xml = SafeXml::parse (f))
        if (xml->hasTagName (mProc.apvts.state.getType()))
        {
            mProc.apvts.replaceStateKeepingUndoHistory (juce::ValueTree::fromXml (*xml),
                                                        "Load Preset");   // ruling 3a
            ok = true;
        }

    if (! ok)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Load Preset",
                                                "That preset file could not be read.",
                                                "OK");
        return;
    }

    // 2026-04-30: notify page wrapper so Layer/Bass tab + mixer strip get
    // renamed to the patch filename (matches DrumPage's sound-pick auto-rename).
    if (onPatchLoaded)
        onPatchLoaded (f.getFileNameWithoutExtension());
}
