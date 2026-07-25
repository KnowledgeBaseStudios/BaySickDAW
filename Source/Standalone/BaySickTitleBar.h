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
    ~BaySickTitleBar() override;

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

    // ── QA-G3Smoke G-16 / G-14 additions ────────────────────────────────────
    // (The SW-3 Swing Mix knob moved to the PageMenuBar -- smoke round 2.)

    /** Width of the PARENT-managed trailing cluster (getTrailingArea callers,
        e.g. the 88px preset button) so bar-owned widgets sit left of it. */
    void setTrailingWidthHint (int px);

    /** G-14: reserved empty width between hosted widgets and the swing knob
        (the Guitars/Basses CUT SELF + mode toggle slots; Task 12 fills). */
    void setReservedTrailingWidth (int px);

    /** G-16: hosts a parent-owned widget in the bar's right-anchored row
        (right-to-left insertion order, left of the trailing hint).  The bar
        takes the component as a CHILD for layout only -- ownership stays with
        the caller. */
    void addHostedTrailingWidget (juce::Component* c, int width);

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

    // QA-G3Smoke G-16 / G-14 state (see the public setters above).
    struct HostedWidget { juce::Component* comp; int width; };
    std::vector<HostedWidget>        mHosted;
    int                              mTrailingHint     { 0 };
    int                              mReservedTrailing { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickTitleBar)
};


/** Standardized "Preset ▾" button used in BaySickTitleBar trailing areas.

    Paints a label + a path-drawn down-chevron triangle (the same triangle
    technique MetroArrowButton uses in GlobalTransportBar) so we don't depend
    on the platform font being able to render `▾` (U+25BE) at small sizes.
    Earlier engines used a literal lowercase 'v' in the button text as a
    stand-in arrow ("Preset v"); this replaces that across the board.

    The button is a thin juce::TextButton subclass so caller code keeps using
    `onClick`, `setBounds`, `setTooltip`, `setColour` (TextButton::buttonColourId
    / textColourOffId) exactly as before.  Pass a custom label to the
    constructor to override the default "Preset" text. */
class BaySickPresetButton : public juce::TextButton
{
public:
    explicit BaySickPresetButton (const juce::String& label = "Preset");
    ~BaySickPresetButton() override;

    void setLabelText (const juce::String& label);
    juce::String getLabelText() const { return mLabel; }

    void paintButton (juce::Graphics&, bool isMouseOverButton, bool isButtonDown) override;

private:
    juce::String mLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickPresetButton)
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
