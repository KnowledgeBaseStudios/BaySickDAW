#include "BaySickTitleBar.h"

BaySickTitleBar::BaySickTitleBar (const juce::String& engineName,
                                  juce::Colour accentColor,
                                  bool bloom)
    : mEngineName (engineName)
    , mAccentColor (accentColor)
    , mBloom (bloom)
{
    // Title bar paints + reports geometry; parent owns the trailing widgets.
    // Pass clicks through so trailing widgets sitting on top remain interactive
    // even if the parent makes them children of the bar.
    setInterceptsMouseClicks (false, true);
}

void BaySickTitleBar::setEngineName (const juce::String& name)
{
    if (mEngineName != name)
    {
        mEngineName = name;
        repaint();
    }
}

void BaySickTitleBar::setAccentColor (juce::Colour c)
{
    if (mAccentColor != c)
    {
        mAccentColor = c;
        repaint();
    }
}

void BaySickTitleBar::setBloom (bool enabled)
{
    if (mBloom != enabled)
    {
        mBloom = enabled;
        repaint();
    }
}

juce::Rectangle<int> BaySickTitleBar::getTrailingArea (int trailingWidth) const
{
    const int x = juce::jmax (kPaddingPx,
                              getWidth() - kPaddingPx - trailingWidth);
    return { x, 0, trailingWidth, getHeight() };
}

void BaySickTitleBar::paint (juce::Graphics& g)
{
    // Standardized dark background (matches existing Harmless/VibePlayer tone).
    g.fillAll (juce::Colour (0xFF141618));

    // 1px bottom divider for visual separation against the panel below.
    g.setColour (juce::Colour (0xFF333537));
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    const auto textRect = juce::Rectangle<int> (kPaddingPx, 0,
                                                getWidth() - 2 * kPaddingPx,
                                                getHeight());
    paintEngineName (g, mEngineName, mAccentColor, textRect, mBloom, kFontSizePx);
}

void BaySickTitleBar::resized()
{
    // Parent positions trailing widgets via getTrailingArea(); no internal layout.
}

void BaySickTitleBar::paintEngineName (juce::Graphics&       g,
                                       const juce::String&   engineName,
                                       juce::Colour          accentColor,
                                       juce::Rectangle<int>  rect,
                                       bool                  bloom,
                                       float                 fontSizePx)
{
    if (bloom)
    {
        // True symmetric halo: convert text to path and stroke it before
        // filling the same path for the crisp overlay.  Stroking radiates
        // outward from each glyph contour equally on every side (left, right,
        // top, bottom), avoiding the directional-shadow look that a same-rect
        // or single-direction-shifted larger font would produce when two
        // text rendered via drawText share a left-aligned edge.  Halo + crisp
        // text share the same path so they are guaranteed pixel-aligned.
        juce::Font font (fontSizePx, juce::Font::bold);
        juce::GlyphArrangement glyphs;
        const float baselineY = (float) rect.getCentreY()
                                + (font.getAscent() - font.getDescent()) * 0.5f;
        glyphs.addJustifiedText (font, engineName,
                                 (float) rect.getX(),
                                 baselineY,
                                 (float) rect.getWidth(),
                                 juce::Justification::left);
        juce::Path textPath;
        glyphs.createPath (textPath);

        g.setColour (accentColor.withAlpha (0.30f));
        g.strokePath (textPath, juce::PathStrokeType (2.5f,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

        g.setColour (accentColor);
        g.fillPath (textPath);
    }
    else
    {
        // No-bloom callers render via the platform text rasterizer for the
        // sharpest crisp text.  Path-based rendering is reserved for bloom
        // callers so the halo + crisp text guarantee pixel alignment.
        g.setColour (accentColor);
        g.setFont   (juce::Font (fontSizePx, juce::Font::bold));
        g.drawText  (engineName, rect, juce::Justification::centredLeft, true);
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// BaySickEngineLabel
// ─────────────────────────────────────────────────────────────────────────────

BaySickEngineLabel::BaySickEngineLabel (const juce::String& engineName,
                                        juce::Colour accentColor,
                                        bool bloom)
    : mEngineName (engineName)
    , mAccentColor (accentColor)
    , mBloom (bloom)
{
    // Label-only painter; never intercept clicks so existing toolbar widgets
    // sitting in front of (or near) the label keep working.
    setInterceptsMouseClicks (false, true);
}

void BaySickEngineLabel::setEngineName (const juce::String& name)
{
    if (mEngineName != name)
    {
        mEngineName = name;
        repaint();
    }
}

void BaySickEngineLabel::setAccentColor (juce::Colour c)
{
    if (mAccentColor != c)
    {
        mAccentColor = c;
        repaint();
    }
}

void BaySickEngineLabel::setBloom (bool enabled)
{
    if (mBloom != enabled)
    {
        mBloom = enabled;
        repaint();
    }
}

void BaySickEngineLabel::paint (juce::Graphics& g)
{
    // No background or divider -- caller's chrome is preserved.  Fill the
    // entire local bounds with the engine name (with optional bloom halo).
    BaySickTitleBar::paintEngineName (g, mEngineName, mAccentColor,
                                       getLocalBounds(), mBloom);
}
