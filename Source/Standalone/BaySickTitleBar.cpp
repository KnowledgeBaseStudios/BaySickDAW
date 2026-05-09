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

    if (mBloom)
    {
        // Halo underlay: 1pt larger font, 15% alpha, offset by (-1, -1).
        // Mirrors the original HarmlessEditor bloom (16pt underlay + 15pt
        // overlay) but pinned to standard 16pt visible text so all engines'
        // crisp glyphs render at the same size.
        g.setColour (mAccentColor.withAlpha (0.15f));
        g.setFont   (juce::Font (kFontSizePx + 1.0f, juce::Font::bold));
        g.drawText  (mEngineName,
                     textRect.translated (-1, -1).withHeight (textRect.getHeight() + 2),
                     juce::Justification::centredLeft, true);
    }

    // Crisp overlay (always).
    g.setColour (mAccentColor);
    g.setFont   (juce::Font (kFontSizePx, juce::Font::bold));
    g.drawText  (mEngineName, textRect, juce::Justification::centredLeft, true);
}

void BaySickTitleBar::resized()
{
    // Parent positions trailing widgets via getTrailingArea(); no internal layout.
}
