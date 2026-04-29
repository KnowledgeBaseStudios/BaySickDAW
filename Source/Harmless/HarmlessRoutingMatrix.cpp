#include "HarmlessRoutingMatrix.h"
#include "HarmlessLAF.h"

static const char* kSliderLabels[] = { "SUB", "PROT", "CLIP", "FX", "VOL", "ENV" };
static const char* kToggleLabels[] = { "AUTO", "VEL", "S1", "S2", "S3", "S4" };

HarmlessRoutingMatrix::HarmlessRoutingMatrix()
{
    for (int i = 0; i < kNumSliders; ++i) {
        auto& s = mSliders[i];
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRange (0.0, 1.0);
        s.setScrollWheelEnabled (true);
        // 2026-04-19 (S1.5): live value popup on hover/drag.
        s.setPopupDisplayEnabled (true, true, nullptr);
        addAndMakeVisible (s);
    }
    // 2026-04-19 (S1) per Jeff: the 6 LED toggles in the design doc are not
    // wired and not needed - just the 6 faders. Toggles intentionally NOT
    // added to the component so they don't paint/hit-test/take space.
    juce::ignoreUnused (kToggleLabels);
}

void HarmlessRoutingMatrix::attachToApvts (juce::AudioProcessorValueTreeState& apvts,
    const juce::String& subId, const juce::String& protId, const juce::String& clipId,
    const juce::String& fxId, const juce::String& volId, const juce::String& envId)
{
    const juce::String ids[] = { subId, protId, clipId, fxId, volId, envId };
    for (int i = 0; i < kNumSliders; ++i)
        if (ids[i].isNotEmpty())
            mSliderAtts[i] = std::make_unique<SliderAtt> (apvts, ids[i], mSliders[i]);
}

void HarmlessRoutingMatrix::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (HarmlessLAF::kPanel));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.f);
    g.setColour (juce::Colour (HarmlessLAF::kBorder));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.f, 1.f);

    g.setColour (juce::Colour (HarmlessLAF::kTextDim));
    g.setFont (juce::Font (7.5f));
    for (int i = 0; i < kNumSliders; ++i) {
        const auto& s = mSliders[i];
        g.drawText (kSliderLabels[i], s.getX() - 2, s.getBottom() + 2,
                    s.getWidth() + 4, 10, juce::Justification::centred);
    }
}

void HarmlessRoutingMatrix::resized()
{
    // 2026-04-19 (S1): toggles dropped (Jeff's "no LED needed"). Faders take
    // the full panel height minus a 14 px label band at the bottom.
    const int sliderH = getHeight() - 14;
    const int sliderW = 18;
    const int gap = (getWidth() - kNumSliders * sliderW) / (kNumSliders + 1);
    int x = gap;
    for (int i = 0; i < kNumSliders; ++i) {
        mSliders[i].setBounds (x, 4, sliderW, sliderH);
        x += sliderW + gap;
    }
}
