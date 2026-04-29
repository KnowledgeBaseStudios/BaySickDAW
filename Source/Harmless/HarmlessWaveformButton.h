#pragma once
#include <JuceHeader.h>

// ── HarmlessWaveformButton ────────────────────────────────────────────────────
// Draws a waveform icon (0=sine, 1=saw, 2=square, 3=triangle) in a dark box.
// Single-click cycles through shapes. Double-click resets to 0.
// Call onChange when the shape changes.
class HarmlessWaveformButton : public juce::Component
{
public:
    std::function<void(int)> onChange;  // called with new shape index

    HarmlessWaveformButton() = default;

    void setValue (int shape)
    {
        mShape = juce::jlimit (0, 3, shape);
        repaint();
    }
    int getValue() const noexcept { return mShape; }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (0.5f);
        // Background
        g.setColour (juce::Colour (0xFF0D0D0E));
        g.fillRoundedRectangle (b, 3.f);
        g.setColour (juce::Colour (0xFF2A2A2C));
        g.drawRoundedRectangle (b, 3.f, 1.f);

        // Draw waveform in orange
        g.setColour (juce::Colour (0xFFFF6600));
        const float mx = b.getX() + 3.f, mw = b.getWidth() - 6.f;
        const float my = b.getCentreY(), mh = (b.getHeight() - 6.f) * 0.5f;

        juce::Path wave;
        if (mShape == 0) {  // Sine
            for (int i = 0; i <= 32; ++i) {
                const float t = float(i) / 32.f;
                const float px = mx + t * mw;
                const float py = my - std::sin (t * juce::MathConstants<float>::twoPi) * mh;
                if (i == 0) wave.startNewSubPath (px, py);
                else        wave.lineTo (px, py);
            }
        } else if (mShape == 1) {  // Saw
            wave.startNewSubPath (mx, my + mh);
            wave.lineTo (mx + mw * 0.5f, my - mh);
            wave.lineTo (mx + mw * 0.5f, my + mh);
            wave.lineTo (mx + mw, my - mh);
        } else if (mShape == 2) {  // Square
            wave.startNewSubPath (mx, my - mh);
            wave.lineTo (mx + mw * 0.5f, my - mh);
            wave.lineTo (mx + mw * 0.5f, my + mh);
            wave.lineTo (mx + mw, my + mh);
        } else {  // Triangle
            wave.startNewSubPath (mx, my);
            wave.lineTo (mx + mw * 0.25f, my - mh);
            wave.lineTo (mx + mw * 0.75f, my + mh);
            wave.lineTo (mx + mw, my);
        }
        g.strokePath (wave, juce::PathStrokeType (1.5f));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown()) {
            mShape = (mShape + 1) % 4;
            repaint();
            if (onChange) onChange (mShape);
        }
    }

private:
    int mShape { 0 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmlessWaveformButton)
};
