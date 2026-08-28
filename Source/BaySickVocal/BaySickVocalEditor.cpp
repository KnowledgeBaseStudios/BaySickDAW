#include "BaySickVocalEditor.h"
#include "BaySickPitchEditor.h"
#include "BaySickAlignEditor.h"
#include "../Standalone/UndoBracket.h"
#include "../BaySickNAMIR/BaySickNAMIRProcessor.h"
#include "../BaySickNAMIR/BaySickNAMIREditor.h"
#include "../Standalone/SlotComponent.h"
#include "../Standalone/EffectEditorPanels.h"

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

    // QA-ApvtsAutomation: forward the owning Vox page's automation prefix to the
    // hosted NAM/IR editor.  Its param ids are bare literals shared with every
    // other NAM/IR instance (Inst pages included), so they need the page key.
    void setAutomationPrefix (const juce::String& prefix)
    {
        if (auto* ne = dynamic_cast<BaySickNAMIREditor*> (mEditor.get()))
            ne->setAutomationPrefix (prefix);
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

// QA-Layout T4: the dormant HostPanel + PlaceholderPanel classes are deleted
// -- every sub-tab they were scaffolding for shipped real content long ago,
// and nothing instantiated either.

// ─────────────────────────────────────────────────────────────────────────────
// BaySickVocalsPanel - realtime pitch correction + page-wide controls
// ─────────────────────────────────────────────────────────────────────────────
class BaySickVocalEditor::BaySickVocalsPanel : public juce::Component,
                                                private juce::Timer
{
public:
    BaySickVocalsPanel (BaySickVocalProcessor& p) : mProc (p)
    {
        // QA-Layout T4: no internal title bar -- the hosting window's title
        // strip shows the centered "BaySickVocals" name (Window-4 treatment,
        // extended here now that this panel IS the Vox window's content).

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

        // QA-Fd 3a/12b: the page-master Bypass button is REMOVED with its
        // param -- pitch/align edits must never be silenced by a page
        // switch.  A/B + Mix cover the compare workflow.
        addAndMakeVisible (mABSlot);
        mABSlot.addItem ("A", 1);
        mABSlot.addItem ("B", 2);
        mABSlot.setSelectedId (1, juce::dontSendNotification);
        mABSlot.setTooltip ("A/B compare slot.  Each slot remembers its own Mix, realtime "
                              "board and vocal chain settings -- build one tone in A, switch "
                              "to B and build another, then flip between the two without "
                              "losing either.  Automatable from the timeline.");
        mABAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                      mProc.apvts, pid ("ab_slot"), mABSlot);

        // ── Realtime pitch correction (bottom half) ────────────────────────
        addAndMakeVisible (mPitchBypassBtn);
        mPitchBypassBtn.setButtonText ("Realtime Pitch ON");
        mPitchBypassBtn.setClickingTogglesState (true);
        mPitchBypassBtn.setTooltip ("Turns on realtime pitch correction.  "
                                      "Tightens vocals to the Root/Scale picked below as they play, "
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

        // Root combo (QA-Fd 17b: "Key" label retired app-wide; param id
        // bsv_pitch_key unchanged).
        addAndMakeVisible (mKeyCombo);
        const char* keyNames[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        for (int i = 0; i < 12; ++i) mKeyCombo.addItem (keyNames[i], i + 1);
        mKeyCombo.setTooltip ("Sets the root note for the realtime correction's snap target.  "
                                "Combined with the Scale below to define which notes are 'in tune'.");
        mKeyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                       mProc.apvts, pid ("pitch_key"), mKeyCombo);
        mKeyLbl.setText ("Root", juce::dontSendNotification);
        mKeyLbl.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (mKeyLbl);

        // Scale combo (QA-Fd 17b: list + order = the piano roll's 13 scales
        // via the shared corrector table -- single source, no literal copy;
        // the dead Custom entry retired with the corrector's old table).
        addAndMakeVisible (mScaleCombo);
        for (int i = 0; i < PitchCorrectorDSP::kNumScales; ++i)
            mScaleCombo.addItem (PitchCorrectorDSP::scaleName (i), i + 1);
        mScaleCombo.setTooltip ("Picks which scale the realtime correction snaps detected pitch to.  "
                                  "Chromatic = nearest semitone (most natural); a named scale (Major, "
                                  "Minor, etc.) tightens to in-key notes only.  Same list as the "
                                  "piano roll's scale picker.");
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

        // QA-ApvtsAutomation: stamp the BARE param id on every automatable control
        // here; BaySickVocalEditor::setAutomationPrefix later re-stamps each one
        // with this page's "vox{N}_" prefix so the right-click Automate menu names
        // the same key the registry answers to.  Without a stamp these controls
        // were reachable only through the Event Editor's parameter browser.
        mABSlot         .setComponentID (pid ("ab_slot"));
        mPitchBypassBtn .setComponentID (pid ("pitch_realtime_bypass"));
        mKeyCombo       .setComponentID (pid ("pitch_key"));
        mScaleCombo     .setComponentID (pid ("pitch_scale"));
        mRetuneSpeed    .setComponentID (pid ("pitch_retuneSpeed"));
        mStrength       .setComponentID (pid ("pitch_strength"));
        mHumanize       .setComponentID (pid ("pitch_humanize"));
        mThroatShift    .setComponentID (pid ("pitch_throatShift"));
        mFormantBtn     .setComponentID (pid ("pitch_formantPreserve"));

        // QA-ManualPress M-4c: manual callout anchors - every BSV callout names
        // one control on this panel, so each dot places itself from the live
        // layout instead of a hand-measured percentage.
        mMixSlider      .getProperties().set (kDotAnchor, "BSV-1");
        mABSlot         .getProperties().set (kDotAnchor, "BSV-2");
        mPitchBypassBtn .getProperties().set (kDotAnchor, "BSV-3");
        mKeyCombo       .getProperties().set (kDotAnchor, "BSV-4");
        mScaleCombo     .getProperties().set (kDotAnchor, "BSV-5");
        mRetuneSpeed    .getProperties().set (kDotAnchor, "BSV-6");
        mStrength       .getProperties().set (kDotAnchor, "BSV-7");
        mHumanize       .getProperties().set (kDotAnchor, "BSV-8");
        mThroatShift    .getProperties().set (kDotAnchor, "BSV-9");
        mFormantBtn     .getProperties().set (kDotAnchor, "BSV-10");
        mPitchRefLbl    .getProperties().set (kDotAnchor, "BSV-11");

        startTimerHz (10);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff14161a));

        // THE SPLIT COMES FROM resized() (Jeff, 2026-08-11).  paint() used to
        // divide at getHeight()/2 while resized() divided elsewhere, so the
        // caption landed on top of the Root / Scale row.  One layout, computed
        // once, read by both.
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawHorizontalLine (mSplitY, 0.0f, (float) getWidth());

        // Descriptive of the section's contents rather than an engine title, so
        // it stays a drawn caption.
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText ("REALTIME PITCH CORRECTION",
                    juce::Rectangle<int> (12, mSplitY + 3, getWidth() - 24, 14),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        // Laid out FROM the panel size (Jeff, 2026-08-11).  This used to be a
        // fixed pixel run that needed ~900px of width and 90px knobs; in a
        // 519x351 window the right-hand half simply fell off the edge and the
        // Mix knob was enormous because it was sized for a window three times
        // as wide.  Everything below derives from getWidth()/getHeight().
        auto all = getLocalBounds().reduced (10, 6);

        const int btnH  = 26;
        const int lblH  = 15;

        // ── Top band: Mix knob + A/B slot ───────────────────────────────────
        auto top = all.removeFromTop (juce::jmax (74, all.getHeight() / 3));
        mSplitY = top.getBottom() + 2;   // published for paint()
        const int mixK = juce::jlimit (44, 64, top.getHeight() - lblH - 4);

        mMixLbl   .setBounds (top.getX(), top.getY(), mixK, lblH);
        mMixSlider.setBounds (top.getX(), top.getY() + lblH, mixK, mixK);

        mABSlot.setBounds (top.getX() + mixK + 14,
                           top.getY() + lblH + (mixK - btnH) / 2, 66, btnH);

        all.removeFromTop (4);

        // ── Bottom band: realtime pitch correction ──────────────────────────
        auto bot = all;
        mPitchRefLbl.setBounds (bot.removeFromBottom (18));
        bot.removeFromTop (20);   // the REALTIME PITCH CORRECTION caption band

        auto row1 = bot.removeFromTop (btnH + lblH);
        {
            auto r = row1;
            mPitchBypassBtn.setBounds (r.removeFromLeft (124)
                                        .withSizeKeepingCentre (124, btnH));
            r.removeFromLeft (8);
            auto keyCol = r.removeFromLeft (62);
            mKeyLbl  .setBounds (keyCol.removeFromTop (lblH));
            mKeyCombo.setBounds (keyCol.removeFromTop (btnH));
            r.removeFromLeft (8);
            auto scaleCol = r.removeFromLeft (juce::jmin (132, r.getWidth()));
            mScaleLbl  .setBounds (scaleCol.removeFromTop (lblH));
            mScaleCombo.setBounds (scaleCol.removeFromTop (btnH));
        }

        bot.removeFromTop (6);

        // Four knobs + the formant toggle share the last row.  Knob width comes
        // from what is actually left, so a narrower window shrinks them instead
        // of pushing the last one off the edge.
        auto knobRow = bot;
        const int formantW = 108;
        const int slotW    = juce::jmax (46, (knobRow.getWidth() - formantW - 8) / 4);
        const int knobK    = juce::jlimit (36, 58, knobRow.getHeight() - lblH - 2);

        auto placeKnob = [&] (juce::Slider& k, juce::Label& l)
        {
            auto col = knobRow.removeFromLeft (slotW);
            l.setBounds (col.getX(), col.getY(), slotW, lblH);
            k.setBounds (col.getX() + (slotW - knobK) / 2, col.getY() + lblH, knobK, knobK);
        };
        placeKnob (mRetuneSpeed, mRetuneSpeedLbl);
        placeKnob (mStrength,    mStrengthLbl);
        placeKnob (mHumanize,    mHumanizeLbl);
        placeKnob (mThroatShift, mThroatShiftLbl);

        knobRow.removeFromLeft (8);
        mFormantBtn.setBounds (knobRow.removeFromLeft (juce::jmin (formantW, knobRow.getWidth()))
                                      .withSizeKeepingCentre (formantW, btnH));
    }

private:
    // Where the page-controls band ends and the realtime-pitch section begins.
    // Computed in resized(), drawn by paint() -- see the note there.
    int mSplitY { 0 };

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
            // A/B is a stepped whole-chain swap -- same click-and-print
            // class mid-take (owner extension, 2026-07-10).  Mix stays live
            // (smooth param).  QA-Fd: the chain Bypass left the gate set
            // with its removal.
            gate (mABSlot);
            mPitchBypassBtn.setTooltip (rec
                ? "Locked while recording - set the realtime sound before the take"
                : "Turns on realtime pitch correction.  "
                  "Tightens vocals to the Root/Scale picked below as they play, "
                  "in both live monitoring and recorded-clip playback.  Use the "
                  "BaySickPitch sub-tab for offline note-by-note pitch editing.");
            mABSlot.setTooltip (rec
                ? "Locked while recording - pick the A/B slot before the take"
                : "A/B compare slot.  Each slot remembers its own Mix, realtime "
                  "board and vocal chain settings -- build one tone in A, switch "
                  "to B and build another, then flip between the two without "
                  "losing either.  Automatable from the timeline.");
        }

        // Note: PitchCorrectorDSP atomic readers aren't exposed via apvts; we
        // read them via the processor's mPitchCorrector member.  H-6 part 1
        // didn't expose accessors, so leave this as a placeholder text update
        // until a follow-up adds getPitchCorrector() or a feedback proxy.
        mPitchRefLbl.setText ("Detected: -- Hz   Target: -- Hz   Shift: -- cents",
                               juce::dontSendNotification);
    }

    BaySickVocalProcessor& mProc;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // Top half (QA-Fd: master Bypass removed with bsv_bypass)
    BaySickSlider       mMixSlider;
    juce::Label      mMixLbl;
    juce::ComboBox   mABSlot;
    std::unique_ptr<SAtt> mMixAtt;
    std::unique_ptr<CAtt> mABAtt;

    // Bottom half
    juce::TextButton mPitchBypassBtn;
    juce::ComboBox   mKeyCombo;
    juce::Label      mKeyLbl;
    juce::ComboBox   mScaleCombo;
    juce::Label      mScaleLbl;
    BaySickSlider       mRetuneSpeed;       juce::Label mRetuneSpeedLbl;
    BaySickSlider       mStrength;          juce::Label mStrengthLbl;
    BaySickSlider       mHumanize;          juce::Label mHumanizeLbl;
    BaySickSlider       mThroatShift;       juce::Label mThroatShiftLbl;
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
// Hosts 6 SlotComponents bound to BaySickVocalProcessor's mVocalChainRack
// (QA-Fe2).  Slots are locked (cannot be swapped / removed / reordered) --
// pinned to: Gate / De-reverb / De-esser / Compressor / Saturation / Limiter.
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
            sc->setVocalChainSlot (true);   // omits Compressor's Pedal mode

            mountSlotEditor (*sc, i);

            // H-7 (2026-05-01): Mode dropdown's onModeChanged callback writes
            // to APVTS so pushApvtsToDsp's per-block push stays consistent
            // with the user-picked Mode.  QA-Fe2 re-slot: Compressor at slot 3
            // -> bsv_comp_type; Saturation at slot 4 -> bsv_sat_type.
            sc->onModeChanged = [this] (int slotIdx, int newType)
            {
                const char* paramId = nullptr;
                if      (slotIdx == 3) paramId = "bsv_comp_type";
                else if (slotIdx == 4) paramId = "bsv_sat_type";
                // TS7: the limiter gained a Mode (Limiter / Maximizer).  Without
                // this row its mode would be written to the DSP and then not
                // persist, because applyChainParams re-pushes from APVTS every
                // block and the chain is APVTS-driven.
                else if (slotIdx == 5) paramId = "bsv_limiter_mode";
                if (paramId == nullptr) return;
                beginParamUndoGesture (mProc.apvts, paramId); // Task 6 (12-iv)
                if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                                  mProc.apvts.getParameter (paramId)))
                    p->setValueNotifyingHost (
                        p->getNormalisableRange().convertTo0to1 ((float) newType));
            };

            // QA-ManualPress M-4c: BSVC-1 is "a chain slot" - one dot for six
            // identical slots, so only the first declares itself.  A stamp on
            // every slot would resolve to the LAST one emitted.
            if (i == 0)
                sc->getProperties().set (kDotAnchor, "BSVC-1");

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

    static constexpr int kNumChainSlots = 6;   // QA-Fe2: Gate + De-reverb added

    BaySickVocalProcessor& mProc;
    std::array<std::unique_ptr<SlotComponent>, kNumChainSlots> mSlots {};
};

// ─────────────────────────────────────────────────────────────────────────────
// BaySickVocalEditor
// ─────────────────────────────────────────────────────────────────────────────

BaySickVocalEditor::BaySickVocalEditor (BaySickVocalProcessor& p)
    : juce::AudioProcessorEditor (&p), mProc (p)
{
    // QA-Layout T4 (Window-7): all five panels are OWNED here (they were
    // always-built even as tabs, and the offline analyses must survive a
    // satellite window closing), but only the BaySickVocals main panel is
    // this editor's CONTENT -- the other four are hosted non-owned by their
    // contained windows through the accessors.
    mPanelBaySickVocals = std::make_unique<BaySickVocalsPanel> (p);
    mPanelVocalChain    = std::make_unique<VocalChainPanel>    (p);
    mPanelBaySickPitch  = std::make_unique<BaySickPitchEditor> (p);   // H-6b
    mPanelBaySickAlign  = std::make_unique<BaySickAlignEditor> (p);   // H-6c
    mPanelBaySickNAMIR  = std::make_unique<NAMIRHostPanel> (p.getNamIrProcessor()); // H-6d
    // J-6 EQ unification (2026-05-03): Pre Rack EQ panel removed.

    addAndMakeVisible (*mPanelBaySickVocals);

    setSize (kMinW, kMinH);
}

void BaySickVocalEditor::setAutomationPrefix (const juce::String& prefix)
{
    if (prefix.isEmpty()) return;
    mAutomationPrefix = prefix;

    if (auto* np = dynamic_cast<NAMIRHostPanel*> (mPanelBaySickNAMIR.get()))
        np->setAutomationPrefix (prefix);

    // Re-stamp every control that carries a bare param id with this page's prefix,
    // so right-click Automate names the same key the registry answers to.  Done as
    // a tree walk rather than a per-control list: the controls live in private
    // nested panel classes (VocalChainPanel + the Align / Pitch sub-editors), and
    // a walk picks up any stamp added there later without touching this function.
    // Skips the NAM/IR subtree -- it was just prefixed by its own editor above.
    std::function<void (juce::Component&)> restamp = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (child == nullptr) continue;
            if (child == mPanelBaySickNAMIR.get()) continue;
            const juce::String id = child->getComponentID();
            if (id.isNotEmpty() && ! id.startsWith (prefix))
                child->setComponentID (prefix + id);
            restamp (*child);
        }
    };
    restamp (*this);

    // QA-ModelShell TS3 (2026-07-27): registration used to happen here, walking
    // mProc.getParameters() and binding each one through a Component lifetime
    // guard.  That guard was this editor -- so every lane on this page died with
    // the page, which destroy-on-close windows turn from a latent bug into the
    // normal case.  StandaloneEditor::registerModelEngineAutomation now does the
    // same walk off the rig's engine-created event, under the same "vox<N>_"
    // keys, and carries the capture-lock veto with it
    // (BaySickVocalProcessor::isCaptureGated).  Stamping ids stays here, because
    // the right-click Automate menu reads them off the clicked component.
}

juce::Component* BaySickVocalEditor::getVocalChainPanel() const noexcept { return mPanelVocalChain   .get(); }
juce::Component* BaySickVocalEditor::getPitchPanel()      const noexcept { return mPanelBaySickPitch .get(); }
juce::Component* BaySickVocalEditor::getAlignPanel()      const noexcept { return mPanelBaySickAlign .get(); }
juce::Component* BaySickVocalEditor::getNamIrPanel()      const noexcept { return mPanelBaySickNAMIR .get(); }

void BaySickVocalEditor::setUndoContext (const UndoContext& ctx)
{
    if (auto* pe = dynamic_cast<BaySickPitchEditor*> (mPanelBaySickPitch.get()))
        pe->setUndoContext (ctx);
}

// J-6 EQ unification (2026-05-03): setPreRackEQ removed.

void BaySickVocalEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0f12));
}

void BaySickVocalEditor::resized()
{
    if (mPanelBaySickVocals)
        mPanelBaySickVocals->setBounds (getLocalBounds());
}
