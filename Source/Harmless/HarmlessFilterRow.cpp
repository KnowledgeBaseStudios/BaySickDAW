#include "HarmlessFilterRow.h"
#include "HarmlessLAF.h"

static void makeRotary (juce::Slider& s) {
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setScrollWheelEnabled (true);
    // 2026-04-19 (S1.5): live value popup on hover/drag, matching effects panels.
    s.setPopupDisplayEnabled (true, true, nullptr);
}

HarmlessFilterRow::HarmlessFilterRow()
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

HarmlessFilterRow::~HarmlessFilterRow() = default;

void HarmlessFilterRow::attachToApvts (juce::AudioProcessorValueTreeState& apvts,
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
}

void HarmlessFilterRow::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (HarmlessLAF::kPanel));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.f);
    g.setColour (juce::Colour (HarmlessLAF::kBorder));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.f, 1.f);

    // Labels
    g.setColour (juce::Colour (HarmlessLAF::kTextDim));
    g.setFont (juce::Font (8.f));
    auto lbl = [&](const juce::Slider& s, const char* t) {
        g.drawText (t, s.getX(), s.getBottom() + 2, s.getWidth(), 10, juce::Justification::centred);
    };
    lbl (mEnvKnob,  "ENV");
    lbl (mFreqKnob, "FREQ");
    lbl (mResKnob,  "RES");
    lbl (mKbTrack,  "KB");
}

void HarmlessFilterRow::resized()
{
    // 2026-04-20 (S5 layout redesign): distribute 5 controls evenly across
    // the full width with equal gap at start / between / end, instead of
    // left-clustering with fixed gaps. Matches the effect-panel style.
    const int kSmall = 32, kLarge = 44, kComboH = 18;
    const int totalW = 80 + kSmall + kLarge + kLarge + kSmall;   // = 232
    const int avail  = getWidth();
    const int gapSpace = juce::jmax (0, avail - totalW);
    const int gap = gapSpace / 6;   // 5 items => 6 gaps (before+between+after)
    int x = gap;
    const int y = 4;
    const int cy = getHeight() / 2;
    mTypeCombo.setBounds (x, cy - kComboH / 2, 80, kComboH);       x += 80 + gap;
    mEnvKnob  .setBounds (x, y, kSmall, kSmall);                    x += kSmall + gap;
    mFreqKnob .setBounds (x, y, kLarge, kLarge);                    x += kLarge + gap;
    mResKnob  .setBounds (x, y, kLarge, kLarge);                    x += kLarge + gap;
    mKbTrack  .setBounds (x, y, kSmall, kSmall);
}
