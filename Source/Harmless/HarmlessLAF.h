#pragma once
#include <JuceHeader.h>
#include <cmath>
#include "../Standalone/SharedUI.h"   // Filmstrips::chickenHead / fader (post-S1.5)

// ── HarmlessLAF - "Ring-Glow Digital Orange" ──────────────────────────────────
// Palette:
//   Main chassis  : #1A1A1C      Accent (glow): #FF6600 (neon amber)
//   Recessed panel: #0D0D0E      Muted gold   : #CC9933
//   Typography    : #B0B0B0      Inactive     : #332211
//
// No filmstrips - everything is custom vector drawn.
//   - Rotary knob: flat dark cap, dim track arc, glowing orange fill arc + bloom
//   - Toggle button: dark flat rect, orange LED line on top edge when active
//   - GroupComponent: near-black fill, 1px crisp inset border
// ─────────────────────────────────────────────────────────────────────────────
class HarmlessLAF : public juce::LookAndFeel_V4
{
public:
    // Colour constants
    static constexpr juce::uint32 kChassis       = 0xFF1A1A1C;
    static constexpr juce::uint32 kPanel         = 0xFF0D0D0E;
    static constexpr juce::uint32 kAccent        = 0xFFFF6600;
    static constexpr juce::uint32 kAccentMuted   = 0xFFCC9933;
    static constexpr juce::uint32 kText          = 0xFFB0B0B0;
    static constexpr juce::uint32 kTextDim       = 0xFF808080;
    static constexpr juce::uint32 kInactive      = 0xFF332211;
    static constexpr juce::uint32 kBorder        = 0xFF252527;
    static constexpr juce::uint32 kCapDark       = 0xFF1D1D1D;
    static constexpr juce::uint32 kCapHover      = 0xFF262626;
    static constexpr juce::uint32 kBtnOff        = 0xFF181818;
    static constexpr juce::uint32 kBtnHover      = 0xFF222222;
    static constexpr juce::uint32 kBtnOnBg       = 0xFF281400;

    HarmlessLAF()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (kChassis));
        setColour (juce::ComboBox::backgroundColourId,        juce::Colour (kBtnOff));
        setColour (juce::ComboBox::outlineColourId,           juce::Colour (0xFF404040));
        setColour (juce::ComboBox::textColourId,              juce::Colour (kText));
        setColour (juce::ComboBox::arrowColourId,             juce::Colour (kAccent));
        setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (kChassis));
        setColour (juce::PopupMenu::textColourId,                  juce::Colour (kText));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (kBtnOnBg));
        setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colour (kAccent));
        setColour (juce::Label::textColourId, juce::Colour (kText));
        setColour (juce::Slider::trackColourId, juce::Colour (kInactive));
    }

    // Property to flag bipolar knobs (arc starts at 12 o'clock)
    inline static const juce::Identifier kBipolar { "bipolar" };
    // 2026-04-19 (S1.5b): chickenHead variant - set on discrete-mode sliders
    // (mode pickers like Prism Mode, Unison Type, Strum Direction). Continuous
    // knobs stay on the default time-based filmstrip rendering.
    inline static const juce::Identifier kKnobVariant { "harmlessKnobVariant" };

    // ── Rotary knob - variant dispatch ────────────────────────────────────────
    // 2026-04-19 (S1.5b) per Jeff: chicken-head visual is for DISCRETE-mode
    // multi-selectors only (Prism Mode, Unison Type, Strum Direction). All
    // other (continuous) knobs use the Time-effects filmstrip with the white
    // indicator tinted orange + an orange under-glow ring beneath the knob.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& s) override
    {
        const auto variant = s.getProperties() [kKnobVariant].toString();
        if (variant == "chickenHead")
        {
            drawChickenHeadKnob (g, x, y, width, height, sliderPos,
                                  rotaryStartAngle, rotaryEndAngle, s);
            return;
        }
        drawTimeStyleKnob (g, x, y, width, height, sliderPos,
                            rotaryStartAngle, rotaryEndAngle, s);
    }

private:
    // ── Continuous-value knob: Time-effects filmstrip + orange tint overlay ──
    // Time-effects filmstrip = 64x64, 101 frames, dark matte cylinder with a
    // white indicator line. We render the filmstrip then overdraw the indicator
    // in orange to honour Jeff's "fill the white lines on the knob with our
    // orange color", and add an orange arc-glow underneath as the value-position
    // hint that's visible against the black chassis.
    void drawTimeStyleKnob (juce::Graphics& g, int x, int y, int width, int height,
                             float sliderPos, float rotaryStartAngle,
                             float rotaryEndAngle, juce::Slider& s)
    {
        const float cx = x + width  * 0.5f;
        const float cy = y + height * 0.5f;
        const float r  = (std::min (width, height) * 0.5f) - 2.0f;
        if (r < 4.0f) return;

        const bool bipolar = s.getProperties()[kBipolar].toString() == "true";
        const float valueAngle = rotaryStartAngle
                               + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // ── Under-glow arc (drawn FIRST, behind the knob, hint of position) ──
        {
            const float underR = r * 1.05f;
            float arcStart, arcEnd;
            if (bipolar)
            {
                const float centerAngle = (rotaryStartAngle + rotaryEndAngle) * 0.5f;
                arcStart = std::min (centerAngle, valueAngle);
                arcEnd   = std::max (centerAngle, valueAngle);
            }
            else
            {
                arcStart = rotaryStartAngle;
                arcEnd   = valueAngle;
            }
            if (arcEnd - arcStart > 0.01f)
            {
                juce::Path arc;
                arc.addArc (cx - underR, cy - underR, underR * 2.f, underR * 2.f,
                            arcStart, arcEnd, true);
                using PST = juce::PathStrokeType;
                g.setColour (juce::Colour (kAccent).withAlpha (0.10f));
                g.strokePath (arc, PST (8.0f, PST::curved, PST::rounded));
                g.setColour (juce::Colour (kAccent).withAlpha (0.55f));
                g.strokePath (arc, PST (3.0f, PST::curved, PST::rounded));
            }
        }

        // ── Time-based filmstrip (64x64, 101 frames) ──────────────────────────
        const auto& strip = Filmstrips::timeBased();
        if (strip.isValid())
        {
            Filmstrips::drawFrame (g, strip, 64, 64, 101, sliderPos,
                                    juce::Rectangle<float> ((float) x, (float) y,
                                                             (float) width, (float) height));
        }
        else
        {
            // Filmstrip unavailable - draw a flat dark cylinder fallback.
            g.setColour (juce::Colour (0xFF1A1A1C));
            g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);
            g.setColour (juce::Colour (0xFF101012));
            g.drawEllipse (cx - r, cy - r, r * 2.f, r * 2.f, 0.8f);
        }

        // ── Orange indicator line OVERLAY on top of the filmstrip's white tick.
        // Same position + width so it dominates the white pixel underneath.
        {
            const float innerR = r * 0.50f;
            const float outerR = r * 0.94f;
            juce::Path tip;
            tip.startNewSubPath (cx + std::sin (valueAngle) * innerR,
                                 cy - std::cos (valueAngle) * innerR);
            tip.lineTo          (cx + std::sin (valueAngle) * outerR,
                                 cy - std::cos (valueAngle) * outerR);
            // Soft glow underneath for emphasis
            g.setColour (juce::Colour (kAccent).withAlpha (0.45f));
            g.strokePath (tip, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
            // Solid amber line on top
            g.setColour (juce::Colour (kAccent));
            g.strokePath (tip, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }
    }

    // ── Discrete multi-selector: chicken-head pointer (smooth vector) ────────
    void drawChickenHeadKnob (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider&)
    {
        const float cx = x + width  * 0.5f;
        const float cy = y + height * 0.5f;
        const float r  = (std::min (width, height) * 0.5f) - 2.0f;
        if (r < 4.0f) return;

        const float valueAngle = rotaryStartAngle
                               + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const float bodyR = r * 0.84f;

        // Hex base
        {
            juce::Path hex;
            const float hexR = bodyR * 0.36f;
            for (int i = 0; i < 6; ++i)
            {
                const float a = juce::MathConstants<float>::twoPi * i / 6.f
                              - juce::MathConstants<float>::pi / 6.f;
                const auto p = juce::Point<float> (cx + hexR * std::cos (a),
                                                    cy + hexR * std::sin (a));
                if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
            }
            hex.closeSubPath();
            juce::ColourGradient hexGrad (juce::Colour (0xFF3A3A3C), cx, cy - hexR,
                                          juce::Colour (0xFF202022), cx, cy + hexR, false);
            g.setGradientFill (hexGrad);
            g.fillPath (hex);
            g.setColour (juce::Colour (0xFF101012));
            g.strokePath (hex, juce::PathStrokeType (0.8f));
        }

        // Pointer body (rotates)
        juce::Path pointer;
        pointer.startNewSubPath (cx, cy - bodyR * 0.92f);
        pointer.lineTo (cx - bodyR * 0.13f, cy - bodyR * 0.38f);
        pointer.lineTo (cx - bodyR * 0.26f, cy + bodyR * 0.12f);
        {
            const float cwR  = bodyR * 0.30f;
            const float cwRy = cwR * 0.9f;
            const float arcCx = cx;
            const float arcCy = cy + bodyR * 0.08f + cwRy;
            constexpr int numSegs = 10;
            for (int seg = 0; seg <= numSegs; ++seg)
            {
                const float t = (float) seg / numSegs;
                const float a = juce::MathConstants<float>::pi * (1.0f - t);
                pointer.lineTo (arcCx + cwR  * std::cos (a),
                                arcCy + cwRy * std::sin (a));
            }
        }
        pointer.lineTo (cx + bodyR * 0.13f, cy - bodyR * 0.38f);
        pointer.closeSubPath();
        pointer.applyTransform (juce::AffineTransform::rotation (valueAngle, cx, cy));
        g.setColour (juce::Colour (0xFF1A1A1C));
        g.fillPath (pointer);
        g.setColour (juce::Colour (0xFF050506));
        g.strokePath (pointer, juce::PathStrokeType (0.6f));

        // Orange tip indicator
        juce::Path tip;
        tip.startNewSubPath (cx, cy - bodyR * 0.34f);
        tip.lineTo          (cx, cy - bodyR * 0.88f);
        tip.applyTransform (juce::AffineTransform::rotation (valueAngle, cx, cy));
        g.setColour (juce::Colour (kAccent).withAlpha (0.45f));
        g.strokePath (tip, juce::PathStrokeType (3.5f));
        g.setColour (juce::Colour (kAccent));
        g.strokePath (tip, juce::PathStrokeType (1.6f));
    }

public:

    // ── Linear vertical fader - standard fader-bar filmstrip + amber underglow
    // 2026-04-19 (S1.5b) per Jeff: "no fader bar like on every other fader in
    // the app and we have no under glow". Switched to the same pattern BaySickLAF
    // uses for non-mixer/non-EQ vertical faders: Filmstrips::fader() (128x128,
    // 31 frames - a real fader-bar visual with a metallic cap + grip lines)
    // plus an amber under-glow gradient from cap-bottom to track-bottom.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minPos, float maxPos,
                           const juce::Slider::SliderStyle style,
                           juce::Slider& s) override
    {
        if (style == juce::Slider::LinearVertical
         || style == juce::Slider::LinearBarVertical)
        {
            // QA-A (2026-05-09): guard against zero-sized slider bounds.
            // A slider that hasn't been laid out yet (bounds = 0) propagates
            // NaN through the norm calc (fh == 0 -> divide-by-zero) and
            // Direct2D asserts on the resulting NaN-coord rounded rects.
            // Latent bug surfaced by the Phase 3.1 kHdrH 36->32 change which
            // shifted body layout in HarmlessEditor.
            if (width <= 0 || height <= 0)
                return;

            const float fy = (float) y;
            const float fh = (float) height;
            const float cx = x + width * 0.5f;

            // Filmstrip frame index expects 0..1 with 1 = top. JUCE's sliderPos
            // for LinearVertical is the absolute Y of the thumb (decreases as
            // value increases), so normalise back.
            float norm;
            if (style == juce::Slider::LinearVertical)
                norm = juce::jlimit (0.f, 1.f, 1.f - (sliderPos - fy) / fh);
            else
                norm = (sliderPos - minPos) / (maxPos - minPos);

            const auto& strip = Filmstrips::fader();
            if (strip.isValid())
            {
                // Render the standard fader-cap-on-track filmstrip.
                Filmstrips::drawFrame (g, strip, 128, 128, 31, norm,
                                        juce::Rectangle<float> ((float) x, fy,
                                                                 (float) width, fh));

                // Amber under-glow from current thumb Y down to track bottom.
                const float thumbY = fy + (1.0f - norm) * fh;
                if (thumbY < fy + fh)
                {
                    const float trackW = juce::jmax (8.f, (float) width * 0.18f);
                    juce::ColourGradient glow (
                        juce::Colour (kAccent).withAlpha (0.65f), cx, thumbY,
                        juce::Colour (kAccent).withAlpha (0.0f),  cx, fy + fh, false);
                    g.setGradientFill (glow);
                    g.fillRoundedRectangle (cx - trackW * 0.5f, thumbY,
                                             trackW, fy + fh - thumbY,
                                             trackW * 0.5f);
                }
                return;
            }

            // Filmstrip unavailable - vector fallback (slim track + cap).
            const float trackW = juce::jmin ((float) width * 0.28f, 6.0f);
            const auto  track  = juce::Rectangle<float> (cx - trackW * 0.5f, fy, trackW, fh);
            g.setColour (juce::Colour (0xFF101012));
            g.fillRoundedRectangle (track, trackW * 0.5f);

            const float thumbY = fy + (1.0f - norm) * fh;
            // Glow below thumb
            juce::ColourGradient glow (
                juce::Colour (kAccent).withAlpha (0.65f), cx, thumbY,
                juce::Colour (kAccent).withAlpha (0.0f),  cx, fy + fh, false);
            g.setGradientFill (glow);
            g.fillRoundedRectangle (cx - trackW, thumbY, trackW * 2.f, fy + fh - thumbY,
                                     trackW * 0.5f);

            // Cap
            const float capW = (float) width * 0.85f;
            const float capH = juce::jmin (14.0f, fh * 0.18f);
            const auto cap   = juce::Rectangle<float> (cx - capW * 0.5f,
                                                        thumbY - capH * 0.5f, capW, capH);
            juce::ColourGradient capGrad (juce::Colour (0xFF3A3A3C), cap.getCentreX(), cap.getY(),
                                           juce::Colour (0xFF1C1C1E), cap.getCentreX(), cap.getBottom(),
                                           false);
            g.setGradientFill (capGrad);
            g.fillRoundedRectangle (cap, 2.5f);
            g.setColour (juce::Colour (0xFF050506));
            g.drawRoundedRectangle (cap, 2.5f, 0.8f);
            g.setColour (juce::Colour (kAccent));
            g.fillRect (cap.getX() + 2.f, cap.getCentreY() - 0.5f, cap.getWidth() - 4.f, 1.0f);
            return;
        }

        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height,
                                                  sliderPos, minPos, maxPos, style, s);
    }

    // ── Toggle / push button - Micro-Toggle with LED ──────────────────────────
    void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                const juce::Colour&, bool isOver, bool) override
    {
        const auto bounds = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on     = b.getToggleState();

        if (b.getClickingTogglesState())
        {
            // Flat toggle button
            g.setColour (on ? juce::Colour (kBtnOnBg)
                            : (isOver ? juce::Colour (kBtnHover) : juce::Colour (kBtnOff)));
            g.fillRoundedRectangle (bounds, 2.0f);

            // Border
            g.setColour (on ? juce::Colour (kAccent).withAlpha (0.4f)
                            : juce::Colour (0xFF404040));
            g.drawRoundedRectangle (bounds, 2.0f, 1.0f);

            // LED line on top edge when active
            if (on)
            {
                g.setColour (juce::Colour (kAccent));
                g.fillRoundedRectangle (bounds.getX() + 2.f, bounds.getY(),
                                         bounds.getWidth() - 4.f, 2.0f, 1.0f);
            }
            return;
        }

        // Non-toggle push button
        g.setColour (isOver ? juce::Colour (kBtnHover) : juce::Colour (kBtnOff));
        g.fillRoundedRectangle (bounds, 2.0f);
        g.setColour (juce::Colour (0xFF404040));
        g.drawRoundedRectangle (bounds, 2.0f, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const bool on = b.getToggleState();
        g.setColour (on ? juce::Colour (kAccent) : juce::Colour (0xFF909090));
        g.setFont   (juce::Font (10.0f, juce::Font::bold));
        g.drawText  (b.getButtonText(), b.getLocalBounds(),
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
        g.setColour (juce::Colour (kBtnOff));
        g.fillRoundedRectangle (b, 2.0f);
        g.setColour (juce::Colour (0xFF404040));
        g.drawRoundedRectangle (b.reduced (0.5f), 2.0f, 1.0f);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (5, 1, box.getWidth() - 28, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return juce::Font (10.0f);
    }

    // ── PopupMenu ─────────────────────────────────────────────────────────────
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (juce::Colour (kChassis));
        g.setColour (juce::Colour (0xFF404040));
        g.drawRect (0, 0, w, h, 1);
    }

    // ── GroupComponent outline ─────────────────────────────────────────────────
    void drawGroupComponentOutline (juce::Graphics& g, int w, int h,
                                    const juce::String& text,
                                    const juce::Justification&,
                                    juce::GroupComponent&) override
    {
        const float indent = 5.0f;
        const float textH  = 13.0f;
        auto bounds = juce::Rectangle<float> (
            indent, textH * 0.5f,
            float (w) - indent * 2.0f,
            float (h) - textH * 0.5f - indent);

        g.setColour (juce::Colour (kPanel));
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (juce::Colour (kBorder));
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

        g.setColour (juce::Colour (kTextDim));
        g.setFont   (juce::Font (9.0f, juce::Font::bold));
        g.drawText  (text, int (indent) + 6, 0,
                     juce::jmin (w - 12, 120), int (textH),
                     juce::Justification::centredLeft, false);
    }
};
