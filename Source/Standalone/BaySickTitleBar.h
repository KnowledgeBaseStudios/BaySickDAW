#pragma once

#include <JuceHeader.h>

/** Shared engine-page title bar.
    Standardized height (32) + font (16pt bold) + left-anchored engine name in
    accent color + standardized dark background + 1px bottom divider.
    Trailing widgets (preset dropdown / A-B toggles / help button) are owned by
    the parent editor; the parent calls `getTrailingArea(width)` to lay them out.

    No LAF coupling - accent is a `juce::Colour` parameter at construction.

    Bloom (default ON, decided 2026-05-09): renders the engine name as a
    glyph path and strokes it with a 2.5px transparent line at 30% accent
    alpha BEFORE filling the same path crisply.  The stroke radiates outward
    from each glyph contour equally on every side, producing a true symmetric
    halo (not a directional shadow).  Path-stroke + path-fill share the same
    path so the halo and crisp text are guaranteed pixel-aligned.  Pass
    `bloom = false` if a future engine wants flat single-pass text. */
class BaySickTitleBar : public juce::Component
{
public:
    BaySickTitleBar (const juce::String& engineName,
                     juce::Colour accentColor,
                     bool bloom = true);
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

    /** Static engine-name painter shared with BaySickEngineLabel (and any
        other caller that wants matching bloom + font + color but supplies its
        own background / chrome). */
    static void paintEngineName (juce::Graphics&        g,
                                 const juce::String&    engineName,
                                 juce::Colour           accentColor,
                                 juce::Rectangle<int>   rect,
                                 bool                   bloom        = true,
                                 float                  fontSizePx   = kFontSizePx);

private:
    juce::String mEngineName;
    juce::Colour mAccentColor;
    bool         mBloom;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickTitleBar)
};


/** Label-only variant of BaySickTitleBar -- paints just the engine name
    (with optional bloom halo) and no background/divider chrome.  Use when an
    existing toolbar / chrome strip already provides the background and just
    needs the engine name painted in matching BaySickTitleBar font + color +
    bloom (e.g. BaySickAlign + BaySickPitch sub-pages of BaySickVocal).
    Shares the static paintEngineName helper with BaySickTitleBar so the
    two visuals stay in lock-step. */
class BaySickEngineLabel : public juce::Component
{
public:
    BaySickEngineLabel (const juce::String& engineName,
                        juce::Colour accentColor,
                        bool bloom = true);
    ~BaySickEngineLabel() override = default;

    void setEngineName  (const juce::String& name);
    void setAccentColor (juce::Colour c);
    void setBloom       (bool enabled);

    juce::String getEngineName  () const { return mEngineName; }
    juce::Colour getAccentColor () const { return mAccentColor; }
    bool         getBloom       () const { return mBloom; }

    void paint (juce::Graphics& g) override;

private:
    juce::String mEngineName;
    juce::Colour mAccentColor;
    bool         mBloom;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickEngineLabel)
};
