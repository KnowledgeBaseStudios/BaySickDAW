#pragma once
#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
// MicPlacementView - a draggable picture of one virtual microphone in front of
// the cabinet (Jeff, 2026-08-11).
//
// WHY THIS IS AN HONEST PICTURE RATHER THAN A METAPHOR.  The two parameters it
// drives are ALREADY POLAR and already physical:
//
//     nam_placement_distance_cm   1 .. 150 cm
//     nam_placement_angle_deg   -90 .. +90 deg
//
// so mic-at-radius-and-rotation is a literal plot of the two numbers, not an
// interpretation of them.  Dragging converts the drop point straight back to
// (r, theta) and writes both params; turning either knob moves the mic.  There
// is no translation layer that could drift out of step with what is heard.
//
// THE TWO VIEWS SHOW DIFFERENT THINGS, because HEIGHT exists (Jeff,
// 2026-08-11).  Without it a side view would have been the same plane drawn
// twice.  Now:
//
//   TOP  - looking down on the cab.  Drag sets DISTANCE and ANGLE.
//   SIDE - looking at the speaker face, cone centred in the view with the rings
//          around it.  Drag sets HEIGHT (vertical) and ANGLE (horizontal);
//          distance is not draggable here and stays on its knob.
//
// The engine still takes ONE off-axis angle -- it is rotationally symmetric
// about the speaker axis -- so the processor combines angle and height into the
// true off-axis angle before the DSP sees it.  See combinePlacement in
// BaySickNAMIRProcessor.cpp.
//
// WHAT IT DELIBERATELY DOES NOT SHOW.  Sliding a mic across the speaker face
// from dust cap to cone edge is a POSITION ON THE CONE, and the DSP has no term
// for it (see updateCoefs -- distance and angle only).  A side view looks like
// it should do that; it does not.  Future State CL-312.
// ─────────────────────────────────────────────────────────────────────────────
class MicPlacementView : public juce::Component,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    enum class ViewMode { TopDown, SideOn };

    MicPlacementView (juce::AudioProcessorValueTreeState& apvts,
                      juce::String distanceParamId,
                      juce::String angleParamId,
                      juce::String heightParamId,
                      juce::String polarParamId);

    ~MicPlacementView() override;

    void setViewMode (ViewMode m);
    ViewMode getViewMode() const noexcept { return mMode; }

    // Greys the whole surface and ignores the mouse -- used for Mic B while it
    // is switched off, matching how its knobs already read.
    void setActiveLook (bool isActive);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void parameterChanged (const juce::String& id, float newValue) override;

    // The cabinet face sits at the bottom (side view) or the near edge
    // (top view); the mic hangs off it at (distance, angle).
    juce::Point<float> cabOrigin() const;
    float              pixelsPerCm() const;
    juce::Point<float> micCentre()  const;

    void paintTopDown (juce::Graphics&, float alpha);
    void paintSideOn  (juce::Graphics&, float alpha);

    // Drop point -> the two params THAT VIEW drives, clamped to their ranges.
    void applyFromPoint (juce::Point<float> p, bool startGesture);

    float readParam (const juce::String& id) const;
    void  writeParam (const juce::String& id, float value);

    juce::AudioProcessorValueTreeState& mApvts;
    const juce::String mDistanceId, mAngleId, mHeightId, mPolarId;

    ViewMode mMode { ViewMode::TopDown };
    bool     mActiveLook { true };
    bool     mDragging   { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicPlacementView)
};
