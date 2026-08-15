#include "MicPlacementView.h"
#include "../Standalone/UndoBracket.h"

namespace
{
    // Parameter ranges, mirrored from BaySickNAMIRProcessor::createLayout.  Kept
    // as named constants rather than read from the range so the drawing maths
    // reads as geometry instead of normalisation.
    constexpr float kMinCm =   1.0f;
    constexpr float kMaxCm = 150.0f;
    constexpr float kMaxDeg = 90.0f;
    constexpr float kMaxHeightCm = 30.0f;

    const juce::Colour kBg      { 0xff15171b };
    const juce::Colour kCab     { 0xff2b2f36 };
    const juce::Colour kCone    { 0xff3a3f48 };
    const juce::Colour kGrid    { 0x22ffffff };
    const juce::Colour kAxis    { 0x3300e5ff };
    const juce::Colour kProx    { 0x22ff5252 };
    const juce::Colour kMic     { 0xffff4d6d };
    const juce::Colour kText    { 0x99ffffff };
}

MicPlacementView::MicPlacementView (juce::AudioProcessorValueTreeState& apvts,
                                    juce::String distanceParamId,
                                    juce::String angleParamId,
                                    juce::String heightParamId,
                                    juce::String polarParamId)
    : mApvts (apvts),
      mDistanceId (std::move (distanceParamId)),
      mAngleId    (std::move (angleParamId)),
      mHeightId   (std::move (heightParamId)),
      mPolarId    (std::move (polarParamId))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);

    // Repaint when the KNOBS move -- the two controls are one placement, so the
    // picture has to follow the knob exactly as the knob follows the picture.
    mApvts.addParameterListener (mDistanceId, this);
    mApvts.addParameterListener (mAngleId,    this);
    mApvts.addParameterListener (mHeightId,   this);
    mApvts.addParameterListener (mPolarId,    this);
}

MicPlacementView::~MicPlacementView()
{
    mApvts.removeParameterListener (mDistanceId, this);
    mApvts.removeParameterListener (mAngleId,    this);
    mApvts.removeParameterListener (mHeightId,   this);
    mApvts.removeParameterListener (mPolarId,    this);
}

void MicPlacementView::setViewMode (ViewMode m)
{
    if (mMode == m) return;
    mMode = m;
    repaint();
}

void MicPlacementView::setActiveLook (bool isActive)
{
    if (mActiveLook == isActive) return;
    mActiveLook = isActive;
    setInterceptsMouseClicks (isActive, isActive);
    repaint();
}

void MicPlacementView::parameterChanged (const juce::String&, float)
{
    // Parameter changes arrive on whichever thread wrote them.
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MicPlacementView> (this)]
    {
        if (safe != nullptr) safe->repaint();
    });
}

float MicPlacementView::readParam (const juce::String& id) const
{
    if (auto* v = mApvts.getRawParameterValue (id)) return v->load();
    return 0.0f;
}

void MicPlacementView::writeParam (const juce::String& id, float value)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (mApvts.getParameter (id)))
        p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (value));
}

// The cab face is the reference the mic is measured from: bottom-centre in the
// side view (looking at the cab edge-on), near-edge-centre in the top view.
juce::Point<float> MicPlacementView::cabOrigin() const
{
    const auto r = getLocalBounds().toFloat();

    // TOP looks down on the cab, so the baffle is the near edge and the mic
    // lives above it.  SIDE looks at the speaker FACE, so the cone is dead
    // centre and the mic can sit above or below it -- which is the whole
    // reason the side view exists now that Height is a real parameter.
    return mMode == ViewMode::SideOn ? r.getCentre()
                                     : juce::Point<float> { r.getCentreX(), r.getBottom() - 22.0f };
}

float MicPlacementView::pixelsPerCm() const
{
    // The full 150 cm has to fit the shorter usable half-span, so the mic can
    // never be dragged off the surface at any window size.
    const auto r = getLocalBounds().toFloat();
    if (mMode == ViewMode::SideOn)
    {
        // Side is scaled to HEIGHT, not distance: the vertical half-span has to
        // hold +/-30 cm with the mic fully inside the surface.
        const float usable = juce::jmin (r.getWidth() * 0.5f, r.getHeight() * 0.5f) - 16.0f;
        return juce::jmax (0.2f, usable / kMaxHeightCm);
    }

    const float usable = juce::jmin (r.getWidth() * 0.5f - 10.0f,
                                     r.getHeight() - 34.0f);
    return juce::jmax (0.2f, usable / kMaxCm);
}

juce::Point<float> MicPlacementView::micCentre() const
{
    const auto  o   = cabOrigin();
    const float ppc = pixelsPerCm();
    const float deg = juce::jlimit (-kMaxDeg, kMaxDeg, readParam (mAngleId));

    if (mMode == ViewMode::SideOn)
    {
        // Looking AT the cone: vertical is height off centre, horizontal is the
        // aim.  Distance is not representable here and is not draggable here.
        const float h = juce::jlimit (-kMaxHeightCm, kMaxHeightCm, readParam (mHeightId));
        return { o.x + (deg / kMaxDeg) * (getWidth() * 0.5f - 14.0f),
                 o.y - h * ppc };
    }

    const float cm  = juce::jlimit (kMinCm, kMaxCm, readParam (mDistanceId));
    const float rad = juce::degreesToRadians (deg);
    const float r   = cm * ppc;
    return { o.x + std::sin (rad) * r, o.y - std::cos (rad) * r };
}

void MicPlacementView::applyFromPoint (juce::Point<float> p, bool startGesture)
{
    const auto  o   = cabOrigin();
    const float ppc = pixelsPerCm();

    if (mMode == ViewMode::SideOn)
    {
        // Vertical -> HEIGHT, horizontal -> ANGLE.  Distance is deliberately
        // untouched: this view cannot show it, so it must not change it.
        const float h   = juce::jlimit (-kMaxHeightCm, kMaxHeightCm, (o.y - p.y) / ppc);
        const float deg = juce::jlimit (-kMaxDeg, kMaxDeg,
                              ((p.x - o.x) / juce::jmax (1.0f, getWidth() * 0.5f - 14.0f)) * kMaxDeg);

        if (startGesture)
        {
            beginParamUndoGesture (mApvts, mHeightId);
            beginParamUndoGesture (mApvts, mAngleId);
        }

        writeParam (mHeightId, h);
        writeParam (mAngleId,  deg);
        repaint();
        return;
    }

    const float dx = p.x - o.x;
    const float dy = juce::jmax (1.0f, o.y - p.y);      // never behind the cab

    const float cm  = juce::jlimit (kMinCm, kMaxCm, std::sqrt (dx * dx + dy * dy) / ppc);
    const float deg = juce::jlimit (-kMaxDeg, kMaxDeg,
                                    juce::radiansToDegrees (std::atan2 (dx, dy)));

    if (startGesture)
    {
        beginParamUndoGesture (mApvts, mDistanceId);
        beginParamUndoGesture (mApvts, mAngleId);
    }

    writeParam (mDistanceId, cm);
    writeParam (mAngleId,    deg);
    repaint();
}

void MicPlacementView::mouseDown (const juce::MouseEvent& e)
{
    mDragging = true;
    applyFromPoint (e.position, true);
}

void MicPlacementView::mouseDrag (const juce::MouseEvent& e)
{
    if (mDragging) applyFromPoint (e.position, false);
}

void MicPlacementView::mouseUp (const juce::MouseEvent&)
{
    mDragging = false;
}

// Double-click returns the mic to the parameter defaults -- 30 cm, on axis --
// the same gesture every knob in the app already answers to.
void MicPlacementView::mouseDoubleClick (const juce::MouseEvent&)
{
    beginParamUndoGesture (mApvts, mDistanceId);
    beginParamUndoGesture (mApvts, mAngleId);
    beginParamUndoGesture (mApvts, mHeightId);
    writeParam (mDistanceId, 30.0f);
    writeParam (mAngleId,     0.0f);
    writeParam (mHeightId,    0.0f);
    repaint();
}

void MicPlacementView::paintTopDown (juce::Graphics& g, float alpha)
{
    const auto  r   = getLocalBounds().toFloat();
    const auto  o   = cabOrigin();
    const float ppc = pixelsPerCm();

    // Distance rings, every 30 cm.
    g.setColour (kGrid.withMultipliedAlpha (alpha));
    for (int cm = 30; cm <= (int) kMaxCm; cm += 30)
    {
        const float rr = cm * ppc;
        g.drawEllipse (o.x - rr, o.y - rr, rr * 2.0f, rr * 2.0f, 1.0f);
    }

    // Inside +/-15 deg the DSP applies NO off-axis darkening at all, so it is
    // worth drawing as the "bright" zone.
    {
        juce::Path axisCone;
        const float len = kMaxCm * ppc;
        axisCone.startNewSubPath (o);
        axisCone.lineTo (o.x + std::sin (juce::degreesToRadians (-15.0f)) * len,
                         o.y - std::cos (juce::degreesToRadians (-15.0f)) * len);
        axisCone.lineTo (o.x + std::sin (juce::degreesToRadians ( 15.0f)) * len,
                         o.y - std::cos (juce::degreesToRadians ( 15.0f)) * len);
        axisCone.closeSubPath();
        g.setColour (kAxis.withMultipliedAlpha (alpha));
        g.fillPath (axisCone);
    }

    // The bass shelf fades out by 20 cm, so that ring is a real boundary
    // rather than decoration.
    {
        const float pr = 20.0f * ppc;
        g.setColour (kProx.withMultipliedAlpha (alpha));
        g.fillEllipse (o.x - pr, o.y - pr, pr * 2.0f, pr * 2.0f);
    }

    // Seen from above: the baffle is a bar, the cone a shallow arc on it.
    const float w = juce::jmin (r.getWidth() - 16.0f, 132.0f);
    g.setColour (kCab.withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (o.x - w * 0.5f, o.y, w, 16.0f, 3.0f);
    g.setColour (kCone.withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (o.x - w * 0.22f, o.y - 3.0f, w * 0.44f, 6.0f, 3.0f);
}

void MicPlacementView::paintSideOn (juce::Graphics& g, float alpha)
{
    const auto  o     = cabOrigin();
    const float ppc   = pixelsPerCm();
    const float dist  = juce::jlimit (kMinCm, kMaxCm, readParam (mDistanceId));
    const float halfW = juce::jmax (1.0f, getWidth() * 0.5f - 14.0f);

    // The cab seen face-on: a 1x12 is roughly 50 cm square with a 30 cm cone.
    g.setColour (kCab.withMultipliedAlpha (alpha));
    {
        const float cw = 25.0f * ppc;
        g.fillRoundedRectangle (o.x - cw, o.y - cw, cw * 2.0f, cw * 2.0f, 6.0f);
    }
    {
        const float cr = 15.0f * ppc;
        g.setColour (kCone.withMultipliedAlpha (alpha));
        g.fillEllipse (o.x - cr, o.y - cr, cr * 2.0f, cr * 2.0f);
        const float dc = 5.0f * ppc;
        g.setColour (kCab.withMultipliedAlpha (alpha));
        g.fillEllipse (o.x - dc, o.y - dc, dc * 2.0f, dc * 2.0f);
    }

    // Height rings every 10 cm, centred on the cone -- the vertical axis of
    // this view IS height, so the rings read off it directly.
    g.setColour (kGrid.withMultipliedAlpha (alpha));
    for (int cm = 10; cm <= (int) kMaxHeightCm; cm += 10)
    {
        const float rr = cm * ppc;
        g.drawEllipse (o.x - rr, o.y - rr, rr * 2.0f, rr * 2.0f, 1.0f);
    }

    // Proximity is a function of TRUE distance sqrt(d^2 + height^2), so face-on
    // it is a horizontal band that closes up as the mic backs off -- and
    // vanishes entirely past 20 cm, which is the honest picture.
    if (dist < 20.0f)
    {
        const float yCm = std::sqrt (20.0f * 20.0f - dist * dist);
        g.setColour (kProx.withMultipliedAlpha (alpha));
        g.fillRect (0.0f, o.y - yCm * ppc, (float) getWidth(), yCm * ppc * 2.0f);
    }

    // The no-darkening zone.  Off-axis angle here combines aim and height
    // (cos eff = cos h * cos v), so the 15 deg region is a lens, not a wedge:
    // swept column by column straight off that identity.
    {
        const float cos15 = std::cos (juce::degreesToRadians (15.0f));
        juce::Path zone;
        bool started = false;
        std::vector<juce::Point<float>> lower;

        for (float x = 0.0f; x <= (float) getWidth(); x += 2.0f)
        {
            const float h  = juce::degreesToRadians (((x - o.x) / halfW) * kMaxDeg);
            const float ch = std::cos (h);
            if (ch <= cos15) continue;

            const float vMax = std::acos (juce::jlimit (-1.0f, 1.0f, cos15 / ch));
            const float dy   = dist * std::tan (vMax) * ppc;

            if (! started) { zone.startNewSubPath (x, o.y - dy); started = true; }
            else            zone.lineTo (x, o.y - dy);

            lower.push_back ({ x, o.y + dy });
        }

        if (started)
        {
            for (auto it = lower.rbegin(); it != lower.rend(); ++it) zone.lineTo (*it);
            zone.closeSubPath();
            g.setColour (kAxis.withMultipliedAlpha (alpha));
            g.fillPath (zone);
        }
    }
}

void MicPlacementView::paint (juce::Graphics& g)
{
    const float alpha = mActiveLook ? 1.0f : 0.35f;
    const auto  o     = cabOrigin();

    g.fillAll (kBg);

    if (mMode == ViewMode::TopDown) paintTopDown (g, alpha);
    else                            paintSideOn  (g, alpha);

    // -- The mic ------------------------------------------------------------
    const auto m = micCentre();
    {
        // Body points back down the line to the cone, so it visibly aims rather
        // than just sliding -- and that one expression is right in both views
        // because it is derived from where the mic actually landed.
        const float rot = std::atan2 (m.x - o.x, juce::jmax (0.001f, o.y - m.y));
        juce::Path body;
        body.addRoundedRectangle (-4.5f, -13.0f, 9.0f, 26.0f, 4.0f);

        g.setColour (kMic.withMultipliedAlpha (alpha));
        g.fillPath (body, juce::AffineTransform::rotation (rot).translated (m.x, m.y));
        g.setColour (juce::Colours::white.withAlpha (0.85f * alpha));
        g.fillEllipse (m.x - 3.0f, m.y - 3.0f, 6.0f, 6.0f);

        g.setColour (kMic.withAlpha (0.45f * alpha));
        g.drawLine (o.x, o.y, m.x, m.y, 1.0f);
    }

    // -- Readout ------------------------------------------------------------
    g.setColour (kText.withMultipliedAlpha (alpha));
    g.setFont (juce::Font (10.0f));

    juce::String read = juce::String ((int) readParam (mDistanceId)) + " cm   "
                          + juce::String ((int) readParam (mAngleId)) + " deg";
    if (mMode == ViewMode::SideOn)
        read += "   " + juce::String ((int) readParam (mHeightId)) + " cm H";

    g.drawText (read, getLocalBounds().reduced (4).removeFromBottom (14),
                juce::Justification::centredRight, false);

    g.drawText (mMode == ViewMode::TopDown ? "TOP" : "SIDE",
                getLocalBounds().reduced (4).removeFromBottom (14),
                juce::Justification::centredLeft, false);
}
