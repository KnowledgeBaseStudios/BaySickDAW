#include "BaySickSolsticeFilterRow.h"
#include "BaySickSolsticeLAF.h"

static void makeRotary (juce::Slider& s) {
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setScrollWheelEnabled (true);
    // 2026-04-19 (S1.5): live value popup on hover/drag, matching effects panels.
    s.setPopupDisplayEnabled (true, true, nullptr);
}

BaySickSolsticeFilterRow::BaySickSolsticeFilterRow()
{
    // Type dropdown
    mTypeCombo.addItem ("LP", 1);
    mTypeCombo.addItem ("HP", 2);
    mTypeCombo.addItem ("BP", 3);
    mTypeCombo.addItem ("Notch", 4);
    mTypeCombo.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (mTypeCombo);

    // Knobs
    for (auto* s : { &mEnvKnob, &mFreqKnob, &mResKnob, &mKbTrack })
        { makeRotary (*s); addAndMakeVisible (*s); }
}

BaySickSolsticeFilterRow::~BaySickSolsticeFilterRow() = default;

void BaySickSolsticeFilterRow::attachToApvts (juce::AudioProcessorValueTreeState& apvts,
                                        const juce::String& cutoffId,
                                        const juce::String& resId,
                                        const juce::String& envAmtId,
                                        const juce::String& kbTrackId,
                                        const juce::String& typeId)
{
    if (!typeId.isEmpty())
        mTypeAtt  = std::make_unique<ComboAtt>  (apvts, typeId,    mTypeCombo);
    mFreqAtt      = std::make_unique<SliderAtt> (apvts, cutoffId,  mFreqKnob);
    mResAtt       = std::make_unique<SliderAtt> (apvts, resId,     mResKnob);
    mEnvAtt       = std::make_unique<SliderAtt> (apvts, envAmtId,  mEnvKnob);
    if (!kbTrackId.isEmpty())
        mKbTrackAtt = std::make_unique<SliderAtt>(apvts, kbTrackId, mKbTrack);

    // T1d/T1e 2026-04-19: setComponentID + tooltip on each filter knob so the
    // app-wide right-click "Automate: ..." / "Type in value..." menu works
    // and hover shows the param name + units.
    mFreqKnob .setComponentID (cutoffId);
    mFreqKnob .setTooltip ("Filter Cutoff (Hz, 20..20000)");
    mResKnob  .setComponentID (resId);
    mResKnob  .setTooltip ("Filter Resonance (Q, 0.1..1)");
    mEnvKnob  .setComponentID (envAmtId);
    mEnvKnob  .setTooltip ("Filter Envelope Amount (-1..+1)");
    if (!kbTrackId.isEmpty())
    {
        mKbTrack.setComponentID (kbTrackId);
        mKbTrack.setTooltip ("Filter Keyboard Tracking (0..1)");
    }

    // QA-ModelShell TS3 (2026-07-27): these ids are engine APVTS params, and the
    // model registers every one of them at engine creation
    // (StandaloneEditor::registerModelEngineAutomation), so the widget
    // registrations that used to sit here are gone -- they only ever added a
    // second, view-scoped claim on the same key that died with the row.

    // Task 5 follow-up: the filter TYPE combo is tone state (it selects the
    // filter model), attached but never stamped -- so it was the one control on
    // this row with no Automate menu.
    if (!typeId.isEmpty())
    {
        mTypeCombo.setComponentID (typeId);
        mTypeCombo.setTooltip ("Filter Type");
    }
}

void BaySickSolsticeFilterRow::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (BaySickSolsticeLAF::kPanel));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.f);
    g.setColour (juce::Colour (BaySickSolsticeLAF::kBorder));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.f, 1.f);

    // Labels
    g.setColour (juce::Colour (BaySickSolsticeLAF::kTextDim));
    g.setFont (juce::Font (8.f));
    auto lbl = [&](const juce::Slider& s, const char* t) {
        g.drawText (t, s.getX(), s.getBottom() + 2, s.getWidth(), 10, juce::Justification::centred);
    };
    lbl (mEnvKnob,  "ENV");
    lbl (mFreqKnob, "FREQ");
    lbl (mResKnob,  "RES");
    lbl (mKbTrack,  "KB");
}

void BaySickSolsticeFilterRow::resized()
{
    // 2026-04-20 (S5 layout redesign): distribute 5 controls evenly across
    // the full width with equal gap at start / between / end, instead of
    // left-clustering with fixed gaps. Matches the effect-panel style.
    // Jeff, 2026-08-04: all FOUR knobs are one size -- FREQ and RES used to be
    // 44 against ENV/KB's 32, which read as an accident rather than emphasis --
    // and the whole set is halved along with every other knob in BaySickSolstice.
    // Jeff, 2026-08-04: the type combo was 80px for labels like "LP" / "Notch"
    // -- three times what it needs -- which is most of why this row looked
    // stretched.  Knobs pack at a fixed gap now rather than sharing out every
    // spare pixel between them.
    const int kSmall = 16, kLarge = kSmall, kComboH = 18, kComboW = 56;
    const int kGapPx = 10;
    // Distributed, not packed: equal gap before / between / after, so the row
    // breathes across whatever width the filter box gives it (Jeff, 2026-08-04).
    juce::ignoreUnused (kGapPx);
    const int itemsW = kComboW + kSmall + kLarge + kLarge + kSmall;
    const int gap    = juce::jmax (2, (getWidth() - itemsW) / 6);   // 5 items, 6 gaps
    int x = gap;
    const int y  = juce::jmax (0, (getHeight() - kSmall) / 2 - 4);
    const int cy = getHeight() / 2;
    mTypeCombo.setBounds (x, cy - kComboH / 2, kComboW, kComboH); x += kComboW + gap;
    mEnvKnob  .setBounds (x, y, kSmall, kSmall);                  x += kSmall  + gap;
    mFreqKnob .setBounds (x, y, kLarge, kLarge);                  x += kLarge  + gap;
    mResKnob  .setBounds (x, y, kLarge, kLarge);                  x += kLarge  + gap;
    mKbTrack  .setBounds (x, y, kSmall, kSmall);
}
