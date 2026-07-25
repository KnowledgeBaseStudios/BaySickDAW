#pragma once
#include <JuceHeader.h>
#include "../Standalone/SharedUI.h"   // TaggedSliderAttachment

// ── HarmlessXYZPad ────────────────────────────────────────────────────────────
// 2D interactive pad for X/Y modulation + a Z knob below.
// Mouse drag sets X/Y in -1..+1 range. onXYChanged(x, y) fires on change.
class HarmlessXYZPad : public juce::Component,
                       private juce::Slider::Listener
{
public:
    std::function<void(float x, float y)> onXYChanged;

    HarmlessXYZPad();

    void setXY (float x, float y);
    float getX() const noexcept { return mX; }
    float getY() const noexcept { return mY; }

    // Attach X, Y and Z to APVTS
    void attachToApvts (juce::AudioProcessorValueTreeState& apvts,
                        const juce::String& xId, const juce::String& yId,
                        const juce::String& zId);

    void paint   (juce::Graphics&) override;
    void resized () override;
    void mouseDown  (const juce::MouseEvent&) override;
    void mouseDrag  (const juce::MouseEvent&) override;

private:
    float mX { 0.f }, mY { 0.f };
    juce::Rectangle<int> mPadBounds;

    VibeSlider mXKnob, mYKnob, mZKnob;
    using SliderAtt = TaggedSliderAttachment;
    std::unique_ptr<SliderAtt> mXAtt, mYAtt, mZAtt;

    void updateFromMouse (const juce::MouseEvent& e);
    // 2026-04-20 (S4 Batch 4 fix): knob -> pad sync. When user drags the X/Y
    // knobs (or APVTS broadcasts a change), pull the new values into mX/mY
    // and repaint the dot position.
    void sliderValueChanged (juce::Slider*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmlessXYZPad)
};
