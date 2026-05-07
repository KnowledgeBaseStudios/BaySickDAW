#pragma once
#include <JuceHeader.h>

class BaySickRustyDrumsProcessor;

// ── BaySickRustyDrumsKitGraphic ─────────────────────────────────────────────
// J-8 (2026-05-04): clickable kit graphic for the BaySickRustyDrums Player tab.
//
// J-8c rev: each hitbox is a rotatable ellipse (cx, cy, rx, ry, rotationDeg)
// in the 2000×1200 kit-image coordinate space, plus a string label that
// names which kit articulation it triggers.  Calibration mode lets the
// user move / resize / rotate / add / delete hitboxes and pick the label
// from a 25-articulation dropdown.  Save Layout dumps the table to a text
// file the user pastes back to me; I bake those into the source defaults.
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
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    juce::String getTooltip() override;

    void setEngine (BaySickRustyDrumsProcessor* engine) { mEngine = engine; }

    // J-8 calibration mode.
    void setCalibrationMode (bool enabled);
    bool isInCalibrationMode() const noexcept { return mCalibrationMode; }
    bool saveLayoutToFile (juce::File dest = {});
    void resetLayoutToDefaults();

    // J-8 stage 1: kit-loaded state.  When false, the photo + side panels
    // render at 50% opacity with a "Pick a program to begin" overlay text
    // and clicks are no-ops.  When true, full visuals + audition.
    void setKitLoaded (bool loaded);
    bool isKitLoaded() const noexcept { return mKitLoaded; }

    // J-8c additions (calibration mode actions).
    void addHitboxAtCentre();           // spawns a new "(unassigned)" ellipse
    void deleteSelectedHitbox();        // erases mSelectedIdx; clamps selection
    int  getSelectedHitboxIndex() const noexcept { return mSelectedIdx; }
    juce::String getSelectedLabel() const;
    void setSelectedLabel (const juce::String& label);

    // List of every articulation label the dropdown should expose.  Used by
    // BaySickRustyDrumsPage to populate the ComboBox.
    static juce::StringArray getArticulationLabels();

private:
    struct Hitbox
    {
        juce::String label;             // "(unassigned)" for new + un-mapped boxes
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

    enum class DragMode {
        None,
        Move,
        ResizeNW, ResizeN, ResizeNE,
        ResizeW,             ResizeE,
        ResizeSW, ResizeS, ResizeSE,
        Rotate,         // in-plane Z spin (handle above ellipse)
        Tilt            // perspective tilt around local X (handle left of ellipse)
    };

    BaySickRustyDrumsProcessor* mEngine { nullptr };
    juce::Image                 mKitImage;
    juce::Image                 mSideBandImage;   // J-8c side panel: ARIA control_tab
    std::vector<Hitbox>         mHitboxes;
    juce::Rectangle<float>      mLastDrawArea;
    int                         mHoverIdx   { -1 };
    int                         mPressedIdx { -1 };

    // Calibration state
    bool                        mCalibrationMode { false };
    int                         mSelectedIdx     { -1 };
    DragMode                    mDragMode        { DragMode::None };
    juce::Point<float>          mDragStartImagePos;
    Hitbox                      mDragOriginalBox {};

    // Kit-load state (controls the dimmed overlay + click no-op when false).
    bool                        mKitLoaded { false };

    static constexpr float kImageW = 2000.f;
    static constexpr float kImageH = 1200.f;
    static constexpr float kHandleHotImage = 32.f;   // image-space hit slop for handles

    int      hitTestPiece    (juce::Point<float> screenPt) const;
    DragMode hitTestHandle   (juce::Point<float> screenPt, int boxIdx) const;
    juce::Point<float> imagePointFromComponent (juce::Point<float> componentPt) const;
    juce::Point<float> componentPointFromImage (juce::Point<float> imagePt) const;
    void updateCursorForHover (juce::Point<float> screenPt);
    void paintHitboxOutline   (juce::Graphics& g, const Hitbox& h, juce::Colour col, float strokeW) const;
    void paintHitboxHandles   (juce::Graphics& g, const Hitbox& h) const;

    // Returns the local-space point relative to the ellipse center, with
    // rotation removed.  Used for both hit-tests and drag math.
    juce::Point<float> toEllipseLocal (const Hitbox& h, juce::Point<float> imagePt) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickRustyDrumsKitGraphic)
};
