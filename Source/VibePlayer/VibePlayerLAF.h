#pragma once
#include <JuceHeader.h>
#include "../Standalone/SharedUI.h"   // TextureUtils, LRXHelper, Filmstrips

// ── VibePlayerLAF — "Brushed Aluminum Mini-Player" ───────────────────────────
//
// Palette:
//   Main panel        : #BCC6CC  (brushed aluminum — drawn via TextureUtils)
//   Recessed trench   : #121212  (near-black matte control panel)
//   Active LED / glow : #A0DB2B  (neon green)
//   Knobs / buttons   : #1A1A1A  (matte black)
//   Illustration accent: #40E0D0 / #F1C40F
//
// Knobs  : volumeBlack filmstrip (70×70, 100 frames) — black dome, white indicator
// Faders : custom ultra-slim horizontal track + thin pill thumb
// Buttons: beveled dark rect, neon green glow border when active (A/B/C/D artic)
// ─────────────────────────────────────────────────────────────────────────────
class VibePlayerLAF : public juce::LookAndFeel_V4
{
public:
    VibePlayerLAF()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xFFBCC6CC));
        setColour (juce::ComboBox::backgroundColourId,        juce::Colour (0xFF1A1A1A));
        setColour (juce::ComboBox::outlineColourId,           juce::Colour (0xFF404040));
        setColour (juce::ComboBox::textColourId,              juce::Colour (0xFFCCCCCC));
        setColour (juce::ComboBox::arrowColourId,             juce::Colour (0xFFA0DB2B));
        setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xFF1A1A1A));
        setColour (juce::PopupMenu::textColourId,                  juce::Colour (0xFFCCCCCC));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xFF253400));
        setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colour (0xFFA0DB2B));
        setColour (juce::Label::textColourId, juce::Colour (0xFF606060));
    }

    // ── Rotary knob ────────────────────────────────────────────────────────────
    // Uses volumeBlack filmstrip (black dome, white indicator, 70×70, 100 frames).
    // Falls back to custom-drawn arc knob if filmstrip unavailable.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float /*startAngle*/, float /*endAngle*/,
                           juce::Slider& /*s*/) override
    {
        const auto dest = juce::Rectangle<float> ((float) x, (float) y,
                                                   (float) width, (float) height);
        const auto& strip = Filmstrips::volumeBlack();
        if (strip.isValid())
        {
            Filmstrips::drawFrame (g, strip, 70, 70, 100, sliderPos, dest);
            return;
        }

        // Fallback: custom drawn matte-black dome knob with green arc
        const float cx = x + width  * 0.5f;
        const float cy = y + height * 0.5f;
        const float r  = (std::min (width, height) * 0.5f) - 3.0f;
        if (r < 3.0f) return;

        // Hex bolt base
        g.setColour (juce::Colour (0xFF0A0A0A));
        g.fillEllipse (cx - r - 2.f, cy - r - 2.f, (r + 2.f) * 2.f, (r + 2.f) * 2.f);

        // Dome body
        juce::ColourGradient domeGrad (juce::Colour (0xFF353535), cx - r * 0.3f, cy - r * 0.3f,
                                       juce::Colour (0xFF0E0E0E), cx + r * 0.5f, cy + r * 0.5f,
                                       false);
        g.setGradientFill (domeGrad);
        g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);

        // Thin green arc
        const float startA = juce::MathConstants<float>::pi * 0.75f;
        const float endA   = juce::MathConstants<float>::pi * 2.25f;
        const float valA   = startA + sliderPos * (endA - startA);

        // Track
        {
            juce::Path arc;
            arc.addArc (cx - r, cy - r, r * 2.f, r * 2.f, startA, endA, true);
            g.setColour (juce::Colour (0xFF303030));
            g.strokePath (arc, juce::PathStrokeType (2.f));
        }
        // Value
        if (sliderPos > 0.001f)
        {
            juce::Path arc;
            arc.addArc (cx - r, cy - r, r * 2.f, r * 2.f, startA, valA, true);
            using PST = juce::PathStrokeType;
            LRXHelper::drawWithBloom (g, arc, juce::Colour (0xFFA0DB2B), 2.f);
        }

        // Indicator line
        const float sa = std::sin (valA), ca = std::cos (valA);
        const float ir = r * 0.60f;
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawLine (cx + sa * ir * 0.45f, cy - ca * ir * 0.45f,
                    cx + sa * ir * 0.88f, cy - ca * ir * 0.88f, 1.5f);
    }

    // ── Linear slider (ultra-slim horizontal fader for DRIVE / REDUCT) ──────────
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float /*minPos*/, float /*maxPos*/,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (style != juce::Slider::LinearHorizontal)
        {
            // Delegate non-horizontal to base
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos,
                                                     0.f, 1.f, style, s);
            return;
        }

        const float trackY = y + h * 0.5f;
        const float trackH = 3.f;   // ultra-slim
        const float thumbW = 10.f;
        const float thumbH = h * 0.70f;
        const float thumbX = sliderPos - thumbW * 0.5f;

        // Track background
        const auto trackR = juce::Rectangle<float> ((float) x, trackY - trackH * 0.5f,
                                                     (float) w, trackH);
        g.setColour (juce::Colour (0xFF2A2A2A));
        g.fillRoundedRectangle (trackR, trackH * 0.5f);

        // Active portion (left of thumb) in neon green
        if (sliderPos > (float) x + 1.f)
        {
            const auto activeR = juce::Rectangle<float> ((float) x, trackY - trackH * 0.5f,
                                                          sliderPos - (float) x, trackH);
            g.setColour (juce::Colour (0xFFA0DB2B).withAlpha (0.75f));
            g.fillRoundedRectangle (activeR, trackH * 0.5f);
        }

        // Thumb: slim vertical pill
        const float ty = trackY - thumbH * 0.5f;
        const auto thumbR = juce::Rectangle<float> (thumbX, ty, thumbW, thumbH);

        // Shadow
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (thumbR.translated (0.5f, 1.0f), 3.f);

        // Body gradient
        juce::ColourGradient tg (juce::Colour (0xFF555555), thumbX, ty,
                                  juce::Colour (0xFF1A1A1A), thumbX, ty + thumbH, false);
        g.setGradientFill (tg);
        g.fillRoundedRectangle (thumbR, 3.f);

        // Edge highlight
        g.setColour (juce::Colour (0xFF707070));
        g.drawRoundedRectangle (thumbR.reduced (0.5f), 3.f, 0.8f);
    }

    // ── Button (A/B/C/D articulation push buttons) ───────────────────────────
    void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                const juce::Colour&, bool isOver, bool) override
    {
        const auto bounds = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on     = b.getToggleState();

        // Bevel shadow (bottom-right)
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (bounds.translated (1.f, 1.f), 3.f);

        // Face fill
        juce::ColourGradient face (
            on ? juce::Colour (0xFF1A2800) : (isOver ? juce::Colour (0xFF282828) : juce::Colour (0xFF1A1A1A)),
            bounds.getCentreX(), bounds.getY(),
            on ? juce::Colour (0xFF111A00) : juce::Colour (0xFF101010),
            bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (face);
        g.fillRoundedRectangle (bounds, 3.f);

        // Border — glows green when active
        if (on)
        {
            // Bloom outer ring
            g.setColour (juce::Colour (0xFFA0DB2B).withAlpha (0.20f));
            g.drawRoundedRectangle (bounds.expanded (2.f), 4.f, 3.f);
            g.setColour (juce::Colour (0xFFA0DB2B).withAlpha (0.60f));
            g.drawRoundedRectangle (bounds, 3.f, 1.2f);
        }
        else
        {
            g.setColour (juce::Colour (0xFF404040));
            g.drawRoundedRectangle (bounds, 3.f, 0.8f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const bool on = b.getToggleState();
        g.setColour (on ? juce::Colour (0xFFA0DB2B) : juce::Colour (0xFF909090));
        g.setFont (juce::Font (11.5f, juce::Font::bold));
        g.drawText (b.getButtonText(), b.getLocalBounds(),
                    juce::Justification::centred, false);
    }

    // ── Label ─────────────────────────────────────────────────────────────────
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        if (label.isBeingEdited()) return;
        g.setColour (label.findColour (juce::Label::textColourId));
        g.setFont   (label.getFont());
        g.drawText  (label.getText(),
                     label.getLocalBounds().reduced (2, 0),
                     label.getJustificationType(), true);
    }

    // ── ComboBox ──────────────────────────────────────────────────────────────
    void drawComboBox (juce::Graphics& g, int, int, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        const auto b = box.getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xFF1A1A1A));
        g.fillRoundedRectangle (b, 3.f);
        g.setColour (juce::Colour (0xFF404040));
        g.drawRoundedRectangle (b.reduced (0.5f), 3.f, 1.f);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (11.f); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (5, 1, box.getWidth() - 28, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    // ── PopupMenu background ──────────────────────────────────────────────────
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (juce::Colour (0xFF1A1A1A));
        g.setColour (juce::Colour (0xFF404040));
        g.drawRect (0, 0, w, h, 1);
    }
};
