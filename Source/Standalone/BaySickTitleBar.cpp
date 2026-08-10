#include "BaySickTitleBar.h"
#include "SharedUI.h"  // VC palette (Surface / Accent / Text) for standardized button paint

BaySickTitleBar::BaySickTitleBar (const juce::String& engineName,
                                  juce::Colour accentColor,
                                  bool bloom)
    : mEngineName (engineName)
    , mAccentColor (accentColor)
    , mBloom (bloom)
{
    // Title bar paints only; the parent owns any widgets sitting over it.
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

BaySickTitleBar::~BaySickTitleBar() = default;

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
    // The bar owns no children -- each parent lays out its own chrome over the
    // bar (e.g. the section tab row is placed by AriaControlPanel::resized).
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
// BaySickPresetButton
// ─────────────────────────────────────────────────────────────────────────────
// QA-A Phase 6 (2026-05-09): standardized title-bar preset button.  Earlier
// engines stuck a literal lowercase 'v' in the button text as a fake chevron
// because the UTF-8 ▾ glyph was unreliable at small sizes (same issue
// MetroArrowButton hit; it switched to a path-drawn triangle for the same
// reason).  This class follows MetroArrowButton's path approach -- label
// text on the left, filled triangle on the right -- so we don't depend on
// platform font fallback for the chevron.

BaySickPresetButton::BaySickPresetButton (const juce::String& label)
    : juce::TextButton (""),
      mLabel (label)
{
    setMouseClickGrabsKeyboardFocus (false);
    setClickingTogglesState (false);

    // QA-A Phase 6 (2026-05-09): lock the preset button to VibeLAF (the app-
    // global LAF used by BaySickPedals + the rest of the chassis).  Engine
    // editors that set their own LAF (HarmlessLAF, BaySickSynthLAF,
    // BaySickBassLAF, VibePlayerLAF) propagate that LAF to their child
    // components, which is what made the preset button look black on those
    // engines and grey on Pedals.  Pinning to VibeLAF here means every
    // preset button paints the same way Pedals' did before the propagation,
    // regardless of which engine page hosts it.
    setLookAndFeel (&VibeLAF::get());
}

BaySickPresetButton::~BaySickPresetButton()
{
    // Clear the LAF pointer before the base class destructor runs -- standard
    // JUCE pattern when assigning a non-owned LookAndFeel via setLookAndFeel.
    setLookAndFeel (nullptr);
}


void BaySickPresetButton::paintButton (juce::Graphics& g,
                                        bool isMouseOverButton,
                                        bool isButtonDown)
{
    // Background -- delegate to LookAndFeel (locked to VibeLAF in the ctor)
    // so the button picks up the standard app chassis hover / pressed
    // shading and matches Pedals' look exactly.
    getLookAndFeel().drawButtonBackground (g, *this,
        findColour (juce::TextButton::buttonColourId),
        isMouseOverButton, isButtonDown);

    auto bounds = getLocalBounds().toFloat().reduced (8.0f, 0.0f);

    // Reserve a 14 px column on the right for the chevron triangle, then
    // draw the label in whatever's left.
    constexpr float kArrowAreaW = 14.0f;
    auto arrowArea = bounds.removeFromRight (kArrowAreaW);
    auto textArea  = bounds;

    g.setColour (findColour (juce::TextButton::textColourOffId));
    g.setFont   (juce::Font (12.0f));
    g.drawText  (mLabel, textArea.toNearestInt(),
                 juce::Justification::centredLeft, true);

    // Down-chevron (filled triangle) -- same dimensions MetroArrowButton uses
    // (5 px half-width x 4 px height) so all chevrons across the app match.
    const float cx = arrowArea.getCentreX();
    const float cy = arrowArea.getCentreY();
    const float w  = 5.0f;
    const float h  = 4.0f;
    juce::Path tri;
    tri.addTriangle (cx - w, cy - h * 0.5f,
                     cx + w, cy - h * 0.5f,
                     cx,     cy + h * 0.5f);
    g.setColour (findColour (juce::TextButton::textColourOffId).withAlpha (0.85f));
    g.fillPath (tri);
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
