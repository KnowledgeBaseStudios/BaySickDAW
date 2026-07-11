#include "BaySickVocalEditor.h"
#include "BaySickPitchEditor.h"
#include "BaySickAlignEditor.h"
#include "../BaySickNAMIR/BaySickNAMIRProcessor.h"
#include "../BaySickNAMIR/BaySickNAMIREditor.h"
#include "../Standalone/SlotComponent.h"
#include "../Standalone/EffectEditorPanels.h"
#include "../Standalone/BaySickTitleBar.h"   // QA-A (2026-05-09)

// H-6c (2026-05-01): createEffectEditor lives in EffectEditorPanels.cpp; the
// VocalChainPanel uses it to materialise per-slot inline editors.  Declaration
// comes from EffectEditorPanels.h (included above) -- no extern needed.
// I-2 (2026-05-02): the function is now 3-arg with a defaulted PanelMode;
// the stale extern with the 2-arg signature here was hiding the new overload
// behind an ambiguous-call error.

// ─────────────────────────────────────────────────────────────────────────────
// BaySickVocalEditor - Phase H-6 (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    constexpr int kMinW = 960;   // matches the smallest player-page footprint
    constexpr int kMinH = 620;

    juce::String pid (const char* suffix) { return juce::String ("bsv_") + suffix; }
}

// ─────────────────────────────────────────────────────────────────────────────
// NAMIRHostPanel - H-6d (2026-05-02)
// Hosts the editor for the BaySickNAMIRProcessor that lives on the owning
// BaySickVocalProcessor (NOT a separately-owned instance, so state save/load
// goes through the parent processor's getStateInformation -> NamIrState
// blob).  Audio routing through the parent's NAM/IR is wired in G-9; for
// H-6d the editor is hosted + reachable but no audio flows through it.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickVocalEditor::NAMIRHostPanel : public juce::Component
{
public:
    explicit NAMIRHostPanel (BaySickNAMIRProcessor& namIrProc)
    {
        mEditor.reset (static_cast<juce::AudioProcessorEditor*> (namIrProc.createEditor()));
        if (mEditor) addAndMakeVisible (*mEditor);
    }

    ~NAMIRHostPanel() override
    {
        // Editor must be destroyed before the parent processor goes away,
        // but we don't own the processor here -- BaySickVocalProcessor does.
        mEditor.reset();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff14161a));
    }

    void resized() override
    {
        if (mEditor)
            mEditor->setBounds (getLocalBounds().reduced (4));
    }

private:
    std::unique_ptr<juce::AudioProcessorEditor> mEditor;
};

// ─────────────────────────────────────────────────────────────────────────────
// HostPanel - wraps an externally-owned child component so the editor can
// inject things like the strip's Pre Rack EQ display into a sub-tab without
// taking ownership.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickVocalEditor::HostPanel : public juce::Component
{
public:
    void setHosted (juce::Component* c)
    {
        if (mHosted == c) return;
        if (mHosted) removeChildComponent (mHosted);
        mHosted = c;
        if (mHosted) addAndMakeVisible (*mHosted);
        resized();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff14161a));
        if (mHosted == nullptr)
        {
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.setFont (juce::Font (14.0f));
            g.drawText ("(Pre Rack EQ - available when this Vox tab has a strip on the mixer)",
                        getLocalBounds(), juce::Justification::centred);
        }
    }

    void resized() override
    {
        if (mHosted) mHosted->setBounds (getLocalBounds().reduced (4));
    }

private:
    juce::Component* mHosted { nullptr };
};

// ─────────────────────────────────────────────────────────────────────────────
// PlaceholderPanel - used by the 4 sub-tabs whose content lands in follow-ups
// ─────────────────────────────────────────────────────────────────────────────
class BaySickVocalEditor::PlaceholderPanel : public juce::Component
{
public:
    PlaceholderPanel (juce::String headline, juce::String body)
        : mHeadline (std::move (headline)), mBody (std::move (body))
    {
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff14161a));

        const auto b = getLocalBounds().reduced (40);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::Font (24.0f, juce::Font::bold));
        g.drawText (mHeadline, b.withHeight (40), juce::Justification::centred);

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (juce::Font (14.0f));
        auto bodyArea = b.withTrimmedTop (60);
        g.drawFittedText (mBody, bodyArea, juce::Justification::centredTop, 8, 0.9f);
    }

private:
    juce::String mHeadline;
    juce::String mBody;
};

// ─────────────────────────────────────────────────────────────────────────────
// BaySickVocalsPanel - realtime pitch correction + page-wide controls
// ─────────────────────────────────────────────────────────────────────────────
class BaySickVocalEditor::BaySickVocalsPanel : public juce::Component,
                                                private juce::Timer
{
public:
    BaySickVocalsPanel (BaySickVocalProcessor& p) : mProc (p)
    {
        // QA-A (2026-05-09): unified title bar at top of the panel, replacing
        // the old "PAGE CONTROLS" g.drawText caption per STYLE-03.  Accent =
        // bright teal (#0FAFA5), matching the Vox tab's active ribbon colour.
        addAndMakeVisible (mTopTitleBar);

        // ── Page-wide controls (top half) ───────────────────────────────────
        addAndMakeVisible (mMixSlider);
        addAndMakeVisible (mMixSlider);
        mMixSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        mMixSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        mMixSlider.setPopupDisplayEnabled (true, true, this);
        mMixSlider.setTooltip ("0..100 %.  Blends the dry input against the processed chain output.  "
                                "Lower it to keep the vocal more natural; 100 = fully processed.");
        mMixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                       mProc.apvts, pid ("mix"), mMixSlider);
        mMixLbl.setText ("Mix", juce::dontSendNotification);
        mMixLbl.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (mMixLbl);

        addAndMakeVisible (mBypassBtn);
        mBypassBtn.setButtonText ("Bypass");
        mBypassBtn.setClickingTogglesState (true);
        mBypassBtn.setTooltip ("Bypasses the entire BaySickVocal chain (pitch + de-esser + "
                                "comp + sat + limiter).  Use to compare the processed vocal "
                                "against the raw input at the strip's output.");
        mBypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                          mProc.apvts, pid ("bypass"), mBypassBtn);

        addAndMakeVisible (mABSlot);
        mABSlot.addItem ("A", 1);
        mABSlot.addItem ("B", 2);
        mABSlot.setSelectedId (1, juce::dontSendNotification);
        mABSlot.setTooltip ("A/B compare slot.  Each slot remembers a full set of chain "
                              "settings -- build one tone in A, switch to B and build another, "
                              "then flip between the two without losing either.  Automatable "
                              "from the timeline.");
        mABAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                      mProc.apvts, pid ("ab_slot"), mABSlot);

        // ── Realtime pitch correction (bottom half) ────────────────────────
        addAndMakeVisible (mPitchBypassBtn);
        mPitchBypassBtn.setButtonText ("Realtime Pitch ON");
        mPitchBypassBtn.setClickingTogglesState (true);
        mPitchBypassBtn.setTooltip ("Turns on realtime pitch correction.  "
                                      "Tightens vocals to the Key/Scale picked below as they play, "
                                      "in both live monitoring and recorded-clip playback.  Use the "
                                      "BaySickPitch sub-tab for offline note-by-note pitch editing.");
        // bypass=true => OFF.  We invert visually via getToggleState.
        mPitchBypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                               mProc.apvts, pid ("pitch_realtime_bypass"), mPitchBypassBtn);
        mPitchBypassBtn.onStateChange = [this]
        {
            // Bypass ON = button OFF (text "OFF"); Bypass OFF = button ON (text "ON")
            const bool bypassed = mPitchBypassBtn.getToggleState();
            mPitchBypassBtn.setButtonText (bypassed ? "Realtime Pitch OFF"
                                                    : "Realtime Pitch ON");
        };
        mPitchBypassBtn.onStateChange();

        // Key combo
        addAndMakeVisible (mKeyCombo);
        const char* keyNames[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        for (int i = 0; i < 12; ++i) mKeyCombo.addItem (keyNames[i], i + 1);
        mKeyCombo.setTooltip ("Sets the root note for the realtime correction's snap target.  "
                                "Combined with the Scale below to define which notes are 'in tune'.");
        mKeyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                       mProc.apvts, pid ("pitch_key"), mKeyCombo);
        mKeyLbl.setText ("Key", juce::dontSendNotification);
        mKeyLbl.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (mKeyLbl);

        // Scale combo
        addAndMakeVisible (mScaleCombo);
        const char* scaleNames[] = {
            "Chromatic","Major","Minor","Harmonic Minor","Dorian","Mixolydian",
            "Phrygian","Lydian","Locrian","Custom" };
        for (int i = 0; i < 10; ++i) mScaleCombo.addItem (scaleNames[i], i + 1);
        mScaleCombo.setTooltip ("Picks which scale the realtime correction snaps detected pitch to.  "
                                  "Chromatic = nearest semitone (most natural); a named scale (Major, "
                                  "Minor, etc.) tightens to in-key notes only; Custom lets you pick "
                                  "the allowed notes manually.");
        mScaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                         mProc.apvts, pid ("pitch_scale"), mScaleCombo);
        mScaleLbl.setText ("Scale", juce::dontSendNotification);
        mScaleLbl.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (mScaleLbl);

        // Knob factory
        auto makeKnob = [this](juce::Slider& s, juce::Label& l, const juce::String& text,
                                const juce::String& paramId, const juce::String& tooltip,
                                std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att)
        {
            s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
            s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            s.setPopupDisplayEnabled (true, true, this);
            s.setTooltip (tooltip);
            addAndMakeVisible (s);
            l.setText (text, juce::dontSendNotification);
            l.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (l);
            att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                       mProc.apvts, paramId, s);
        };

        makeKnob (mRetuneSpeed, mRetuneSpeedLbl, "Retune ms", pid ("pitch_retuneSpeed"),
                   "0..100 ms.  Sets how fast vocals snap to the target note.  Low values = "
                   "instant robotic hard-tune sound; high values = transparent natural correction.",
                   mRetuneSpeedAtt);
        makeKnob (mStrength,    mStrengthLbl,    "Strength",  pid ("pitch_strength"),
                   "0..100 %.  Sets how aggressively the correction pulls pitch toward the target.  "
                   "100 = full snap to in-scale notes; 0 = pitch passes through unchanged.  "
                   "60-80 % is typical for natural-sounding correction.",
                   mStrengthAtt);
        makeKnob (mHumanize,    mHumanizeLbl,    "Humanize",  pid ("pitch_humanize"),
                   "0..100 %.  Adds tiny random pitch wobble after correction so corrected "
                   "vocals don't sound mechanical.  Higher = more natural variation.",
                   mHumanizeAtt);
        makeKnob (mThroatShift, mThroatShiftLbl, "Throat",    pid ("pitch_throatShift"),
                   "-100..+100 cents.  Shifts the vocal-tract resonance independently of pitch.  "
                   "Negative = chestier / deeper-sounding voice; positive = brighter / smaller "
                   "voice.  Useful for subtly resexing or reshaping a vocal's character.",
                   mThroatShiftAtt);

        addAndMakeVisible (mFormantBtn);
        mFormantBtn.setButtonText ("Formant Preserve");
        mFormantBtn.setClickingTogglesState (true);
        mFormantBtn.setTooltip ("Keeps the vocal character intact while correction shifts pitch.  "
                                  "On = natural sound at any correction amount; Off = classic "
                                  "pitch-shift artifacts (chipmunk-up, demon-down).");
        mFormantAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                           mProc.apvts, pid ("pitch_formantPreserve"), mFormantBtn);

        // Pitch reference label (live readout from processor's atomic feedback)
        addAndMakeVisible (mPitchRefLbl);
        mPitchRefLbl.setText ("Detected: --   Target: --", juce::dontSendNotification);
        mPitchRefLbl.setJustificationType (juce::Justification::centred);
        mPitchRefLbl.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));
        mPitchRefLbl.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain));

        startTimerHz (10);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff14161a));

        // Top half / bottom half divider line
        const int half = getHeight() / 2;
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawHorizontalLine (half, 0.0f, (float) getWidth());

        // QA-A (2026-05-09): "PAGE CONTROLS" g.drawText caption removed --
        // mTopTitleBar at y=0..32 now owns that role with engine name
        // "BaySickVocals" instead of "PAGE CONTROLS" per STYLE-03.

        // Bottom-half caption preserved -- this is descriptive of the
        // section's contents (realtime correction widgets), not an engine
        // title, so it stays as a g.drawText caption for now.
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText ("REALTIME PITCH CORRECTION", juce::Rectangle<int> (16, half + 4, 280, 18),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        // QA-A (2026-05-09): title bar at the top of the panel.  Top-half
        // content rect skips it via withTrimmedTop, then keeps the original
        // 16-px horizontal margin and 8-px vertical breathing room (was 24
        // before; reduced because the title bar now sits where "PAGE CONTROLS"
        // used to be, providing its own visual chrome).
        mTopTitleBar.setBounds (0, 0, getWidth(), BaySickTitleBar::kStandardHeight);

        const int half = getHeight() / 2;
        auto top = getLocalBounds()
                       .withTrimmedTop (BaySickTitleBar::kStandardHeight)
                       .removeFromTop (half - BaySickTitleBar::kStandardHeight)
                       .reduced (16, 8);
        auto bot = getLocalBounds().withTrimmedTop (half).reduced (16, 24);

        // Top half: Bypass | Mix knob | A/B combo
        const int btnH = 28;
        mBypassBtn  .setBounds (top.removeFromLeft (100).withSizeKeepingCentre (96, btnH));
        top.removeFromLeft (12);

        mMixLbl     .setBounds (top.getX(), top.getY(), 90, 16);
        mMixSlider  .setBounds (top.getX(), top.getY() + 16, 90, 90);
        top.removeFromLeft (102);

        mABSlot     .setBounds (top.removeFromLeft (80).withSizeKeepingCentre (72, btnH));

        // Bottom half: row of pitch correction controls
        // Layout: [Pitch Bypass] [Key] [Scale] [Retune] [Strength] [Humanize] [Throat] [Formant] [Pitch readout]
        const int ctrlW = 96;
        const int rowY  = bot.getY() + 16;

        mPitchBypassBtn.setBounds (bot.getX(),                  rowY + 30, 130, btnH);
        mKeyLbl        .setBounds (bot.getX() + 142,            rowY,      80,  16);
        mKeyCombo      .setBounds (bot.getX() + 142,            rowY + 18, 80,  btnH);
        mScaleLbl      .setBounds (bot.getX() + 230,            rowY,      130, 16);
        mScaleCombo    .setBounds (bot.getX() + 230,            rowY + 18, 130, btnH);

        const int knobsX = bot.getX() + 372;
        const int knobY  = rowY;
        mRetuneSpeedLbl.setBounds (knobsX,             knobY,      ctrlW, 16);
        mRetuneSpeed   .setBounds (knobsX,             knobY + 16, ctrlW, 90);
        mStrengthLbl   .setBounds (knobsX + ctrlW,     knobY,      ctrlW, 16);
        mStrength      .setBounds (knobsX + ctrlW,     knobY + 16, ctrlW, 90);
        mHumanizeLbl   .setBounds (knobsX + ctrlW * 2, knobY,      ctrlW, 16);
        mHumanize      .setBounds (knobsX + ctrlW * 2, knobY + 16, ctrlW, 90);
        mThroatShiftLbl.setBounds (knobsX + ctrlW * 3, knobY,      ctrlW, 16);
        mThroatShift   .setBounds (knobsX + ctrlW * 3, knobY + 16, ctrlW, 90);

        mFormantBtn    .setBounds (knobsX + ctrlW * 4, knobY + 30, 140, btnH);

        // Live pitch readout pinned at bottom
        mPitchRefLbl   .setBounds (bot.removeFromBottom (28));
    }

private:
    void timerCallback() override
    {
        // Realtime board locks while THIS strip captures a take (owner call
        // 2026-07-10): engage-edge toggles mid-take click AND print into the
        // WET file -- the sound is set before the take.  Monitoring without
        // recording stays editable (that's the setup flow).
        const bool rec = mProc.onIsStripRecording && mProc.onIsStripRecording();
        if (rec != mRecGated)
        {
            mRecGated = rec;
            auto gate = [rec] (juce::Component& c)
            {
                c.setEnabled (! rec);
                c.setAlpha (rec ? 0.4f : 1.0f);
            };
            gate (mPitchBypassBtn);
            gate (mKeyCombo);    gate (mKeyLbl);
            gate (mScaleCombo);  gate (mScaleLbl);
            gate (mRetuneSpeed); gate (mRetuneSpeedLbl);
            gate (mStrength);    gate (mStrengthLbl);
            gate (mHumanize);    gate (mHumanizeLbl);
            gate (mThroatShift); gate (mThroatShiftLbl);
            gate (mFormantBtn);
            // Chain Bypass + A/B are stepped whole-chain swaps -- same
            // click-and-print class mid-take (owner extension, 2026-07-10).
            // Mix stays live (smooth param).
            gate (mBypassBtn);
            gate (mABSlot);
            mPitchBypassBtn.setTooltip (rec
                ? "Locked while recording - set the realtime sound before the take"
                : "Turns on realtime pitch correction.  "
                  "Tightens vocals to the Key/Scale picked below as they play, "
                  "in both live monitoring and recorded-clip playback.  Use the "
                  "BaySickPitch sub-tab for offline note-by-note pitch editing.");
            mBypassBtn.setTooltip (rec
                ? "Locked while recording - set the chain before the take"
                : "Bypasses the entire BaySickVocal chain (pitch + de-esser + "
                  "comp + sat + limiter).  Use to compare the processed vocal "
                  "against the raw input at the strip's output.");
            mABSlot.setTooltip (rec
                ? "Locked while recording - pick the A/B slot before the take"
                : "A/B compare slot.  Each slot remembers a full set of chain "
                  "settings -- build one tone in A, switch to B and build another, "
                  "then flip between the two without losing either.  Automatable "
                  "from the timeline.");
        }

        // Note: PitchCorrectorDSP atomic readers aren't exposed via apvts; we
        // read them via the processor's mPitchCorrector member.  H-6 part 1
        // didn't expose accessors, so leave this as a placeholder text update
        // until a follow-up adds getPitchCorrector() or a feedback proxy.
        mPitchRefLbl.setText ("Detected: -- Hz   Target: -- Hz   Shift: -- cents",
                               juce::dontSendNotification);
    }

    BaySickVocalProcessor& mProc;

    // QA-A (2026-05-09): unified title bar replaces the old "PAGE CONTROLS"
    // g.drawText caption.  Accent = bright teal (#0FAFA5) -- same as the Vox
    // tab's active ribbon colour at RibbonTabBar.cpp:20.
    BaySickTitleBar mTopTitleBar { "BaySickVocals", juce::Colour (0xFF0FAFA5) };

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // Top half
    juce::Slider     mMixSlider;
    juce::Label      mMixLbl;
    juce::TextButton mBypassBtn;
    juce::ComboBox   mABSlot;
    std::unique_ptr<SAtt> mMixAtt;
    std::unique_ptr<BAtt> mBypassAtt;
    std::unique_ptr<CAtt> mABAtt;

    // Bottom half
    juce::TextButton mPitchBypassBtn;
    juce::ComboBox   mKeyCombo;
    juce::Label      mKeyLbl;
    juce::ComboBox   mScaleCombo;
    juce::Label      mScaleLbl;
    juce::Slider     mRetuneSpeed;       juce::Label mRetuneSpeedLbl;
    juce::Slider     mStrength;          juce::Label mStrengthLbl;
    juce::Slider     mHumanize;          juce::Label mHumanizeLbl;
    juce::Slider     mThroatShift;       juce::Label mThroatShiftLbl;
    juce::TextButton mFormantBtn;
    juce::Label      mPitchRefLbl;

    std::unique_ptr<BAtt> mPitchBypassAtt;
    std::unique_ptr<CAtt> mKeyAtt, mScaleAtt;
    std::unique_ptr<SAtt> mRetuneSpeedAtt, mStrengthAtt, mHumanizeAtt, mThroatShiftAtt;
    std::unique_ptr<BAtt> mFormantAtt;
    bool mRecGated { false };
};

// ─────────────────────────────────────────────────────────────────────────────
// VocalChainPanel - H-6c (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────
// Hosts 4 SlotComponents bound to BaySickVocalProcessor's mVocalChainRack.
// Slots are locked (cannot be swapped / removed / reordered) -- they're
// pinned to: De-esser / Compressor / Saturation / Limiter in that order.
// SlotComponent's bypass + sidechain dropdown still work normally.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickVocalEditor::VocalChainPanel : public juce::Component
{
public:
    VocalChainPanel (BaySickVocalProcessor& p) : mProc (p)
    {
        for (int i = 0; i < kNumChainSlots; ++i)
        {
            auto sc = std::make_unique<SlotComponent> (i);
            sc->setRack (&mProc.mVocalChainRack);
            sc->setLocked (true);

            mountSlotEditor (*sc, i);

            // H-7 (2026-05-01): Mode dropdown's onModeChanged callback writes
            // to APVTS so pushApvtsToDsp's per-block push stays consistent
            // with the user-picked Mode.  Compressor at slot 1 -> bsv_comp_type;
            // Saturation at slot 2 -> bsv_sat_type.
            sc->onModeChanged = [this] (int slotIdx, int newType)
            {
                const char* paramId = nullptr;
                if      (slotIdx == 1) paramId = "bsv_comp_type";
                else if (slotIdx == 2) paramId = "bsv_sat_type";
                if (paramId == nullptr) return;
                if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                                  mProc.apvts.getParameter (paramId)))
                    p->setValueNotifyingHost (
                        p->getNormalisableRange().convertTo0to1 ((float) newType));
            };

            addAndMakeVisible (*sc);
            mSlots[i] = std::move (sc);
        }

        // QA-F chain-wiring fix (2026-07-10): re-mount the slot editors after
        // a state restore -- panel knobs sync from DSP state at construction
        // and would otherwise display pre-restore values.
        mProc.onChainStateRestored = [this]
        {
            for (int i = 0; i < kNumChainSlots; ++i)
                if (mSlots[i])
                    mountSlotEditor (*mSlots[i], i);
            resized();
        };
    }

    ~VocalChainPanel() override
    {
        mProc.onChainStateRestored = nullptr;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff14161a));
    }

    void resized() override
    {
        // Slots stack vertically (matches the Effects rack page layout).
        auto b = getLocalBounds().reduced (8);
        const int gap = 6;
        const int rowH = juce::jmax (60, (b.getHeight() - gap * (kNumChainSlots - 1)) / kNumChainSlots);
        for (int i = 0; i < kNumChainSlots; ++i)
        {
            if (! mSlots[i]) continue;
            mSlots[i]->setBounds (b.removeFromTop (rowH));
            b.removeFromTop (gap);
        }
    }

private:
    // Materialize (or re-materialize after a state restore) the inline
    // editor for the locked slot's effect, then opt it into the two-way
    // bsv_ param binding (QA-F chain-wiring fix, 2026-07-10).
    void mountSlotEditor (SlotComponent& sc, int i)
    {
        const auto& slot = mProc.mVocalChainRack.getSlot (i);
        if (auto* eff = mProc.mVocalChainRack.getSlotEffect (i))
        {
            auto ed = createEffectEditor (eff, slot.type);
            auto* base = dynamic_cast<EditorPanelBase*> (ed.get());
            sc.setEditor (std::move (ed));
            if (base)
                base->bindToApvts (mProc.apvts, "bsv_");
        }
    }

    static constexpr int kNumChainSlots = 4;

    BaySickVocalProcessor& mProc;
    std::array<std::unique_ptr<SlotComponent>, kNumChainSlots> mSlots {};
};

// ─────────────────────────────────────────────────────────────────────────────
// BaySickVocalEditor
// ─────────────────────────────────────────────────────────────────────────────

BaySickVocalEditor::BaySickVocalEditor (BaySickVocalProcessor& p)
    : juce::AudioProcessorEditor (&p), mProc (p)
{
    // H-6b (2026-05-01): tabs are PageMenuBar buttons (set by StandaloneEditor
    // via mPageMenuBar->setTabSlots) -- this editor just owns 6 content panes
    // and switches which one is visible based on setActiveTab.
    mPanelBaySickVocals = std::make_unique<BaySickVocalsPanel> (p);
    mPanelVocalChain    = std::make_unique<VocalChainPanel>    (p);
    mPanelBaySickPitch  = std::make_unique<BaySickPitchEditor> (p);   // H-6b
    mPanelBaySickAlign  = std::make_unique<BaySickAlignEditor> (p);   // H-6c
    mPanelBaySickNAMIR  = std::make_unique<NAMIRHostPanel> (p.getNamIrProcessor()); // H-6d
    // J-6 EQ unification (2026-05-03): Pre Rack EQ panel removed.

    addChildComponent (*mPanelBaySickVocals);
    addChildComponent (*mPanelVocalChain);
    addChildComponent (*mPanelBaySickPitch);
    addChildComponent (*mPanelBaySickAlign);
    addChildComponent (*mPanelBaySickNAMIR);

    setActiveTab (TabBaySickVocals);
    setSize (kMinW, kMinH);
}

juce::Component* BaySickVocalEditor::panelForTab (int idx) const noexcept
{
    switch (idx)
    {
        case TabBaySickVocals: return mPanelBaySickVocals.get();
        case TabVocalChain:    return mPanelVocalChain   .get();
        case TabBaySickPitch:  return mPanelBaySickPitch .get();
        case TabBaySickAlign:  return mPanelBaySickAlign .get();
        case TabBaySickNAMIR:  return mPanelBaySickNAMIR .get();
        // J-6 EQ unification (2026-05-03): TabPreRackEQ removed.
        default:               return nullptr;
    }
}

void BaySickVocalEditor::setActiveTab (int idx)
{
    mActiveTab = juce::jlimit (0, (int) kNumTabs - 1, idx);
    for (int i = 0; i < (int) kNumTabs; ++i)
        if (auto* c = panelForTab (i))
            c->setVisible (i == mActiveTab);
    resized();
}

// J-6 EQ unification (2026-05-03): setPreRackEQ removed.

void BaySickVocalEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0f12));
}

void BaySickVocalEditor::resized()
{
    if (auto* c = panelForTab (mActiveTab))
        c->setBounds (getLocalBounds());
}
