#pragma once
#include <JuceHeader.h>

class BaySickRustyDrumsProcessor;

// ── BaySickRustyDrumsKitGraphic ─────────────────────────────────────────────
// J-8 (2026-05-04): clickable kit graphic for the BaySickRustyDrums Player tab.
//
// J-8c rev: each hitbox is a rotatable ellipse (cx, cy, rx, ry, rotationDeg)
// in the 2000×1200 kit-image coordinate space, plus a string label that
// names which kit articulation it triggers.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickRustyDrumsKitGraphic : public juce::Component,
                                     public juce::SettableTooltipClient
{
public:
    explicit BaySickRustyDrumsKitGraphic (BaySickRustyDrumsProcessor* engine);
    ~BaySickRustyDrumsKitGraphic() override;

    void paint  (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    juce::String getTooltip() override;

    void setEngine (BaySickRustyDrumsProcessor* engine) { mEngine = engine; }

    void resetLayoutToDefaults();

    // J-8 stage 1: kit-loaded state.  When false, the photo + side panels
    // render at 50% opacity with a "Pick a program to begin" overlay text
    // and clicks are no-ops.  When true, full visuals + audition.
    void setKitLoaded (bool loaded);

private:
    struct Hitbox
    {
        juce::String label;             // "(unassigned)" for un-mapped boxes
        // Image-space ellipse: (cx, cy) center, rx/ry radii, rotation in degrees
        // (positive = clockwise).  rotDeg = in-plane (Z-axis) rotation.
        // tiltDeg = perspective tilt around the ellipse's local X-axis,
        // applied AFTER rotDeg.  Range -89..+89; rendered as effective ry
        // = ry * cos(tiltDeg) so tilting up/down compresses the ellipse
        // vertically - useful for cymbals seen from a 3/4 angle.
        float cx, cy, rx, ry;
        float rotDeg;
        float tiltDeg;
    };

    BaySickRustyDrumsProcessor* mEngine { nullptr };
    juce::Image                 mKitImage;
    juce::Image                 mSideBandImage;   // J-8c side panel: ARIA control_tab
    std::vector<Hitbox>         mHitboxes;
    juce::Rectangle<float>      mLastDrawArea;
    int                         mHoverIdx   { -1 };
    int                         mPressedIdx { -1 };

    // Kit-load state (controls the dimmed overlay + click no-op when false).
    bool                        mKitLoaded { false };

    static constexpr float kImageW = 2000.f;
    static constexpr float kImageH = 1200.f;

    int      hitTestPiece    (juce::Point<float> screenPt) const;
    juce::Point<float> imagePointFromComponent (juce::Point<float> componentPt) const;
    juce::Point<float> componentPointFromImage (juce::Point<float> imagePt) const;
    void paintHitboxOutline   (juce::Graphics& g, const Hitbox& h, juce::Colour col, float strokeW) const;

    // Returns the local-space point relative to the ellipse center, with
    // rotation removed.
    juce::Point<float> toEllipseLocal (const Hitbox& h, juce::Point<float> imagePt) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickRustyDrumsKitGraphic)
};
