#include "BaySickSolsticeRoutingMatrix.h"
#include "BaySickSolsticeLAF.h"

static const char* kSliderLabels[] = { "SUB", "PROT", "CLIP", "FX", "VOL", "ENV" };

BaySickSolsticeRoutingMatrix::BaySickSolsticeRoutingMatrix()
{
    for (int i = 0; i < kNumSliders; ++i) {
        auto& s = mSliders[i];
        // Jeff, 2026-08-04: knobs, not faders -- the six faders needed vertical
        // throw, and that height is exactly what this section could not afford.
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRange (0.0, 1.0);
        s.setScrollWheelEnabled (true);
        // 2026-04-19 (S1.5): live value popup on hover/drag.
        s.setPopupDisplayEnabled (true, true, nullptr);
        addAndMakeVisible (s);
    }
    // 2026-04-19 (S1) per Jeff: the 6 LED toggles in the design doc are not
    // wired and not needed - just the 6 faders.  Dropped for good as BLU-630
    // (Future State S2, "Won't do"); the member array that outlived the ruling
    // was removed at QA-Soundness.
}

void BaySickSolsticeRoutingMatrix::attachToApvts (juce::AudioProcessorValueTreeState& apvts,
    const juce::String& subId, const juce::String& protId, const juce::String& clipId,
    const juce::String& fxId, const juce::String& volId, const juce::String& envId)
{
    const juce::String ids[] = { subId, protId, clipId, fxId, volId, envId };
    for (int i = 0; i < kNumSliders; ++i)
        if (ids[i].isNotEmpty())
        {
            mSliderAtts[i] = std::make_unique<SliderAtt> (apvts, ids[i], mSliders[i]);
            // QA-ApvtsAutomation Task 5 follow-up: attached but never stamped, so
            // these six had no Automate menu.  QA-ModelShell TS3: the stamp is
            // all that belongs here -- the applicator behind it is registered
            // model-side against the engine's APVTS, so it outlives this matrix.
            mSliders[i].setComponentID (ids[i]);
        }
}

void BaySickSolsticeRoutingMatrix::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (BaySickSolsticeLAF::kPanel));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.f);
    g.setColour (juce::Colour (BaySickSolsticeLAF::kBorder));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.f, 1.f);

    g.setColour (juce::Colour (BaySickSolsticeLAF::kTextDim));
    g.setFont (juce::Font (7.5f));
    for (int i = 0; i < kNumSliders; ++i) {
        const auto& s = mSliders[i];
        g.drawText (kSliderLabels[i], s.getX() - 2, s.getBottom() + 2,
                    s.getWidth() + 4, 10, juce::Justification::centred);
    }
}

void BaySickSolsticeRoutingMatrix::resized()
{
    // 2026-04-19 (S1): toggles dropped (Jeff's "no LED needed").
    // Jeff, 2026-08-04: one packed row of 16px knobs, matching kKnobSm in the
    // editor, with a label band beneath.  Packed at a fixed gap rather than
    // sharing out all spare width, so the row is as wide as its contents.
    constexpr int kKnobSz = 16, kGapPx = 10, kLblBand = 12;
    const int rowW = kNumSliders * kKnobSz + kGapPx * (kNumSliders - 1);
    const int y    = juce::jmax (0, (getHeight() - kLblBand - kKnobSz) / 2);
    int x = juce::jmax (0, (getWidth() - rowW) / 2);
    for (int i = 0; i < kNumSliders; ++i) {
        mSliders[i].setBounds (x, y, kKnobSz, kKnobSz);
        x += kKnobSz + kGapPx;
    }
}
