#pragma once

#include <JuceHeader.h>

/** Shared engine-page title bar.
    Standardized height (32) + font (16pt bold) + left-anchored engine name in
    accent color + standardized dark background + 1px bottom divider.
    Trailing widgets (preset dropdown / A-B toggles / help button) are owned by
    the parent editor; the parent calls `getTrailingArea(width)` to lay them out.

    No LAF coupling - accent is a `juce::Colour` parameter at construction.

    Optional bloom (per-engine opt-in, decided 2026-05-09): paints the engine
    name twice when enabled - 17pt bold underlay at 15% accent opacity, offset
    by (-1, -1), creating a halo around the standard 16pt bold overlay. Used
    today by HarmlessEditor to preserve its long-standing orange-glow signature;
    other engines can opt in later if their visual identity calls for it. */
class BaySickTitleBar : public juce::Component
{
public:
    BaySickTitleBar (const juce::String& engineName,
                     juce::Colour accentColor,
                     bool bloom = false);
    ~BaySickTitleBar() override = default;

    static constexpr int   kStandardHeight = 32;
    static constexpr int   kPaddingPx      = 8;
    static constexpr float kFontSizePx     = 16.0f;

    void setEngineName  (const juce::String& name);
    void setAccentColor (juce::Colour c);
    void setBloom       (bool enabled);

    juce::String getEngineName  () const { return mEngineName; }
    juce::Colour getAccentColor () const { return mAccentColor; }
    bool         getBloom       () const { return mBloom; }

    /** Returns the right-anchored rectangle the parent should use to lay out
        trailing widgets. `trailingWidth` is the pixel width the cluster needs.
        Result is full-height (0..getHeight()), starting at
        `getWidth() - kPaddingPx - trailingWidth`. */
    juce::Rectangle<int> getTrailingArea (int trailingWidth) const;

    void paint   (juce::Graphics& g) override;
    void resized () override;

private:
    juce::String mEngineName;
    juce::Colour mAccentColor;
    bool         mBloom;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickTitleBar)
};
