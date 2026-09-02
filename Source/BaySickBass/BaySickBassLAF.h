#pragma once
#include <JuceHeader.h>
#include <cmath>

// ── BaySickBassLAF - "Bass Green / neon low-end" ─────────────────────────────
// Palette:
//   Main bg       : #1C1E21      Visualizer bg  : #0A0B0C
//   Arc / active  : #33FF88  (B1 neon green - bass page color)
//   Decay accent  : #00CED1  (soft cyan - for tab 1 envelope)
//   Inactive      : #2A2C30      Typography     : #E0E0E0
//
// Identical knob/button rendering to BaySickSynthLAF, differentiated by the
// neon-green bass accent color (#33FF88 vs BaySickSynth's #A0DB2B).
// ─────────────────────────────────────────────────────────────────────────────
class BaySickBassLAF : public juce::LookAndFeel_V4
{
public:
    static constexpr uint32_t kGreen    = 0xFF33FF88;   // B1 neon green (bass)
    static constexpr uint32_t kCyan     = 0xFF00CED1;   // envelope/shape accent
    static constexpr uint32_t kBgMain   = 0xFF1C1E21;
    static constexpr uint32_t kInactive = 0xFF2A2C30;
    static constexpr uint32_t kText     = 0xFFE0E0E0;

    BaySickBassLAF()
    {
        setColour (juce::ResizableWindow::backgroundColourId,        juce::Colour (kBgMain));
        setColour (juce::ComboBox::backgroundColourId,               juce::Colour (0xFF181B1F));
        setColour (juce::ComboBox::outlineColourId,                  juce::Colour (0xFF404040));
        setColour (juce::ComboBox::textColourId,                     juce::Colour (kText));
        setColour (juce::ComboBox::arrowColourId,                    juce::Colour (kGreen));
        setColour (juce::PopupMenu::backgroundColourId,              juce::Colour (0xFF1C1E21));
        setColour (juce::PopupMenu::textColourId,                    juce::Colour (kText));
        setColour (juce::PopupMenu::highlightedBackgroundColourId,   juce::Colour (0xFF0A3A20));
        setColour (juce::PopupMenu::highlightedTextColourId,         juce::Colour (kGreen));
        setColour (juce::Label::textColourId,                        juce::Colour (kText));
        setColour (juce::Slider::textBoxTextColourId,                juce::Colour (kText));
        setColour (juce::Slider::textBoxOutlineColourId,             juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId,          juce::Colour (0xFF141618));
    }

    // ── Rotary knob ────────────────────────────────────────────────────────────
    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        const float cx = x + width  * 0.5f;
        const float cy = y + height * 0.5f;
        const float r  = (std::min (width, height) * 0.5f) - 4.0f;
        if (r < 4.0f) return;

        // ── Hex-bolt base ─────────────────────────────────────────────────────
        {
            const float hr = r + 4.0f;
            juce::Path hex;
            for (int k = 0; k < 6; ++k)
            {
                const float a  = juce::MathConstants<float>::pi / 6.0f
                                 + k * (juce::MathConstants<float>::twoPi / 6.0f);
                const float px = cx + hr * std::cos (a);
                const float py = cy + hr * std::sin (a);
                if (k == 0) hex.startNewSubPath (px, py);
                else        hex.lineTo (px, py);
            }
            hex.closeSubPath();
            g.setColour (juce::Colour (0xFF141618));
            g.fillPath  (hex);
            g.setColour (juce::Colour (0xFF2C2E31));
            g.strokePath (hex, juce::PathStrokeType (1.0f));
        }

        // ── Track arc (inactive bg) - lineTo loop per CLAUDE.md ──────────────
        {
            juce::Path track;
            const int kSteps = 96;
            for (int k = 0; k <= kSteps; ++k)
            {
                const float t = (float) k / kSteps;
                const float a = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * t;
                const float px = cx + r * std::sin (a);
                const float py = cy - r * std::cos (a);
                if (k == 0) track.startNewSubPath (px, py);
                else        track.lineTo (px, py);
            }
            g.setColour (juce::Colour (kInactive));
            g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
        }

        // ── Active arc with bloom ─────────────────────────────────────────────
        {
            const bool bipolar = (slider.getMinimum() < 0.0);
            float arcStart, arcEnd;

            if (bipolar)
            {
                const float mid = (rotaryStartAngle + rotaryEndAngle) * 0.5f;
                const float val = rotaryStartAngle
                                  + (rotaryEndAngle - rotaryStartAngle) * sliderPos;
                arcStart = std::min (mid, val);
                arcEnd   = std::max (mid, val);
            }
            else
            {
                arcStart = rotaryStartAngle;
                arcEnd   = rotaryStartAngle
                           + (rotaryEndAngle - rotaryStartAngle) * sliderPos;
            }

            if (std::abs (arcEnd - arcStart) > 0.002f)
            {
                const int kSteps = 64;
                auto makeArcPath = [&]() -> juce::Path
                {
                    juce::Path p;
                    for (int k = 0; k <= kSteps; ++k)
                    {
                        const float t  = (float) k / kSteps;
                        const float a  = arcStart + (arcEnd - arcStart) * t;
                        const float px = cx + r * std::sin (a);
                        const float py = cy - r * std::cos (a);
                        if (k == 0) p.startNewSubPath (px, py);
                        else        p.lineTo (px, py);
                    }
                    return p;
                };

                const juce::Path active = makeArcPath();
                g.setColour (juce::Colour (kGreen).withAlpha (0.15f));
                g.strokePath (active, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded));
                g.setColour (juce::Colour (kGreen).withAlpha (0.30f));
                g.strokePath (active, juce::PathStrokeType (5.5f, juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded));
                g.setColour (juce::Colour (kGreen));
                g.strokePath (active, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded));
            }
        }

        // ── Knob body ─────────────────────────────────────────────────────────
        g.setColour (juce::Colour (0xFF1C1F22));
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colour (0xFF3A3C40));
        g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);

        // ── Pointer tick ──────────────────────────────────────────────────────
        {
            const float angle = rotaryStartAngle
                                + (rotaryEndAngle - rotaryStartAngle) * sliderPos;
            const float pLen  = r * 0.55f;
            g.setColour (juce::Colour (0xFFD0D0D0));
            g.drawLine (cx, cy,
                        cx + pLen * std::sin (angle),
                        cy - pLen * std::cos (angle), 2.0f);
        }
    }

    // ── Button background (BaySickSolstice VEL-style for toggles, 2026-04-22) ────────
    void drawButtonBackground (juce::Graphics& g, juce::Button& btn,
                                const juce::Colour& /*bgColor*/,
                                bool highlighted, bool down) override
    {
        const auto  b  = btn.getLocalBounds().toFloat().reduced (0.5f);
        const bool  on = btn.getToggleState();

        if (btn.getClickingTogglesState())
        {
            const juce::Colour offFill (kInactive);
            const juce::Colour onFill  = juce::Colour (kGreen).withAlpha (0.18f)
                                            .overlaidWith (offFill);
            g.setColour (on ? onFill
                            : (down        ? offFill.brighter (0.12f)
                              : highlighted ? offFill.brighter (0.06f)
                                            : offFill));
            g.fillRoundedRectangle (b, 2.0f);

            g.setColour (on ? juce::Colour (kGreen).withAlpha (0.4f)
                            : juce::Colour (0xFF404040));
            g.drawRoundedRectangle (b, 2.0f, 1.0f);

            if (on)
            {
                g.setColour (juce::Colour (kGreen));
                g.fillRoundedRectangle (b.getX() + 2.0f, b.getY(),
                                         b.getWidth() - 4.0f, 2.0f, 1.0f);
            }
            return;
        }

        // Non-toggle push style (tab bar, preset menu) - unchanged.
        juce::Colour fill;
        if      (on)          fill = juce::Colour (kGreen);
        else if (down)        fill = juce::Colour (kInactive).brighter (0.12f);
        else if (highlighted) fill = juce::Colour (kInactive).brighter (0.06f);
        else                  fill = juce::Colour (kInactive);

        g.setColour (fill);
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (juce::Colour (on ? 0xFF1A8844u : 0xFF3A3C40u));
        g.drawRoundedRectangle (b, 3.0f, 1.0f);
    }

    // ── Button text ───────────────────────────────────────────────────────────
    void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                          bool /*highlighted*/, bool /*down*/) override
    {
        const bool on = btn.getToggleState();
        if (btn.getClickingTogglesState())
        {
            g.setColour (on ? juce::Colour (kGreen) : juce::Colour (0xFF909090));
            g.setFont   (juce::Font (10.0f, juce::Font::bold));
            g.drawFittedText (btn.getButtonText(),
                              btn.getLocalBounds().reduced (2, 0),
                              juce::Justification::centred, 1);
            return;
        }

        g.setColour (on ? juce::Colour (0xFF0A1A10) : juce::Colour (kText));
        g.setFont   (juce::Font (11.5f, juce::Font::bold));
        g.drawFittedText (btn.getButtonText(),
                          btn.getLocalBounds().reduced (2, 0),
                          juce::Justification::centred, 1);
    }

    // ── Label - force text color ──────────────────────────────────────────────
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        g.fillAll (label.findColour (juce::Label::backgroundColourId));
        if (!label.isBeingEdited())
        {
            const float alpha = label.isEnabled() ? 1.0f : 0.5f;
            g.setColour (label.findColour (juce::Label::textColourId).withAlpha (alpha));
            g.setFont   (juce::Font (10.5f));
            g.drawFittedText (label.getText(), label.getLocalBounds(),
                              juce::Justification::centredTop, 1);
        }
    }

    // ── ComboBox border ───────────────────────────────────────────────────────
    void drawComboBox (juce::Graphics& g, int width, int height,
                       bool, int, int, int, int,
                       juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<float> (0.f, 0.f, (float)width, (float)height);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);

        // Arrow
        const float ax = width - 14.0f;
        const float ay = height * 0.5f;
        juce::Path arrow;
        arrow.startNewSubPath (ax,        ay - 3.0f);
        arrow.lineTo          (ax + 5.0f, ay - 3.0f);
        arrow.lineTo          (ax + 2.5f, ay + 3.0f);
        arrow.closeSubPath();
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath  (arrow);
    }

    // ── Tooltip sizing (wider than JUCE default to fit multi-line text) ────────
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText,
                                            juce::Point<int> screenPos,
                                            juce::Rectangle<int> parentArea) override
    {
        const int maxTextW = 420;
        juce::TextLayout layout;
        juce::AttributedString s (tipText);
        s.setJustification (juce::Justification::centredLeft);
        s.setFont (juce::Font (13.0f));
        s.setColour (juce::Colour (kText));
        layout.createLayout (s, (float) maxTextW);

        const int textW = (int) std::ceil (layout.getWidth());
        const int textH = (int) std::ceil (layout.getHeight());
        const int w = juce::jmax (80, textW + 16);
        const int h = juce::jmax (22, textH + 10);

        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                      screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6)  : screenPos.y + 6,
                                      w, h).constrainedWithin (parentArea);
    }

    void drawTooltip (juce::Graphics& g,
                      const juce::String& text,
                      int width, int height) override
    {
        g.fillAll (juce::Colour (0xFF181B1F));
        g.setColour (juce::Colour (0xFF404040));
        g.drawRect (0, 0, width, height, 1);

        juce::AttributedString s (text);
        s.setJustification (juce::Justification::centredLeft);
        s.setFont (juce::Font (13.0f));
        s.setColour (juce::Colour (kText));

        juce::TextLayout layout;
        layout.createLayout (s, (float) (width - 16));
        layout.draw (g, juce::Rectangle<float> (8.0f, 5.0f,
                                                 (float) (width - 16),
                                                 (float) (height - 10)));
    }
};
