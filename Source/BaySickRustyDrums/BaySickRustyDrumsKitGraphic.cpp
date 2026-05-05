#include "BaySickRustyDrumsKitGraphic.h"
#include "BaySickRustyDrumsProcessor.h"
#include "BaySickAssets.h"
#include <set>

namespace
{
// ── Articulation lookup ───────────────────────────────────────────────────────
// Each label maps to the kit-native MIDI note that gets auditioned when the
// hitbox is clicked.  isHiHatTipFamily / isHiHatShaftFamily flag pieces whose
// MIDI swaps based on the pedal state (closed → these notes, open → the
// alternate variable-key note).  isPedalToggle is the special-case foot
// pedal that toggles CC4 instead of triggering a note.
struct ArticDef
{
    const char* label;
    int         midiNote;
    int         pedalOpenNote;        // when nonzero AND pedal open, send this instead
    bool        isPedalToggle;
    const char* channelName;          // J-8: which kit channel this articulation belongs to —
                                      //       used to grey out the hitbox when the loaded program
                                      //       doesn't include the corresponding piece.  Empty
                                      //       string for "(unassigned)" / non-piece articulations.
};

constexpr ArticDef kArtics[] = {
    { "(unassigned)",                -1,  0, false, ""                },
    { "Kick",                         36, 0, false, "Kick"            },
    { "Snare Center",                 38, 0, false, "Snare"           },
    { "Snare Edge",                   39, 0, false, "Snare"           },
    { "Snare Rim",                    40, 0, false, "Snare"           },
    { "Snare Sidestick",              37, 0, false, "Snare"           },
    { "Tom 14",                       47, 0, false, "Tom 14"          },
    { "Tom 15",                       45, 0, false, "Tom 15"          },
    { "Tom 18",                       43, 0, false, "Tom 18"          },
    { "Tom 22",                       41, 0, false, "Tom 22"          },
    { "Hi-hat Tip",                   42, 46, false, "Hi-hat"         },   // closed → 42, open → 46
    { "Hi-hat Shaft",                 54, 58, false, "Hi-hat"         },   // closed → 54, open → 58
    { "Hi-hat Pedal",                 -2,  0, true,  "Hi-hat"         },
    { "Ride 22 Bow",                  51, 0, false, "Ride 22"         },
    { "Ride 22 Edge Crash",           52, 0, false, "Ride 22"         },
    { "Ride 22 Bell",                 53, 0, false, "Ride 22"         },
    { "Ride Sizzle 19 Bow",           60, 0, false, "Ride Sizzle 19"  },
    { "Ride Sizzle 19 Edge Crash",    61, 0, false, "Ride Sizzle 19"  },
    { "Ride Sizzle 19 Bell",          62, 0, false, "Ride Sizzle 19"  },
    { "Crash 17",                     49, 0, false, "Crash 17"        },
    { "Crash Sizzle 17 Bow",          64, 0, false, "Crash Sizzle 17" },
    { "Crash Sizzle 17 Crash",        65, 0, false, "Crash Sizzle 17" },
    { "Crash Sizzle 17 Bell",         67, 0, false, "Crash Sizzle 17" },
    { "China 18",                     57, 0, false, "China 18"        },
    { "Stack Mid",                    71, 0, false, "Stack"           },
    { "Stack Edge",                   72, 0, false, "Stack"           },
};

const ArticDef* findArtic (const juce::String& label)
{
    for (const auto& a : kArtics)
        if (label == a.label) return &a;
    return nullptr;
}

// ── Default 13-piece layout (calibrated rect → ellipse) ──────────────────────
// Converted from the previous rect calibration: cx = x + w/2, cy = y + h/2,
// rx = w/2, ry = h/2, rotation = 0.  User can refine via Calibrate mode.
struct DefaultEllipse
{
    const char* label;
    float cx, cy, rx, ry, rotDeg, tiltDeg;
};

// Calibrated 2026-05-04 against `Assets/big_rusty_drums.png` via the in-app
// "Calibrate Hitboxes" mode (drag/resize/rotate/tilt + Save Layout export).
// 25 articulations matching the curated middle-ground list from J-8c spec.
constexpr DefaultEllipse kDefaultEllipses[] = {
    { "Crash 17",                  326.1f, 202.9f, 289.6f,  81.7f,  -2.6f, -30.8f },
    { "Stack Mid",                 311.6f, 118.2f,  87.9f,  50.5f,  -2.5f, -56.3f },
    { "Stack Edge",                313.2f, 154.6f, 130.3f,  74.7f,   0.0f, -58.6f },
    { "Crash Sizzle 17 Crash",     735.5f, 280.4f, 151.0f,  45.3f,  -2.0f, -31.6f },
    { "Crash Sizzle 17 Bow",       737.7f, 278.5f, 124.9f,  42.1f,  -1.8f, -47.7f },
    { "Crash Sizzle 17 Bell",      736.8f, 275.6f,  43.1f,  35.1f,  -4.5f, -61.0f },
    { "Ride 22 Edge Crash",       1172.7f, 282.5f, 243.6f,  98.6f,   1.2f, -23.4f },
    { "Ride 22 Bow",              1174.4f, 271.0f, 183.5f, 102.5f,   1.8f, -49.9f },
    { "Ride 22 Bell",             1176.6f, 248.2f,  73.9f,  72.4f,   0.0f, -62.4f },
    { "Ride Sizzle 19 Bow",       1588.3f, 191.9f, 236.6f,  77.6f,   2.6f, -33.9f },
    { "Ride Sizzle 19 Edge Crash",1591.0f, 206.6f, 298.3f, 128.9f,   1.9f, -44.2f },
    { "Ride Sizzle 19 Bell",      1583.8f, 174.1f,  81.7f,  60.0f,   3.9f, -50.3f },
    { "China 18",                 1798.1f, 432.3f, 251.7f, 118.8f,  -2.0f, -32.5f },
    { "Hi-hat Tip",                227.9f, 481.7f, 214.5f,  94.4f,  -3.8f, -33.1f },
    { "Hi-hat Shaft",              231.1f, 464.3f, 162.6f, 116.2f,  -3.1f, -66.2f },
    { "Tom 14",                    543.0f, 377.0f, 135.0f,  60.0f,   0.0f,   0.0f },
    { "Tom 15",                    880.8f, 381.7f, 144.3f,  56.1f,   0.0f,   0.0f },
    { "Snare Center",              645.3f, 596.0f,  91.6f,  59.1f,   0.0f, -42.6f },
    { "Snare Edge",                642.0f, 597.1f, 185.4f,  96.8f,  -1.7f, -47.2f },
    { "Snare Rim",                 645.5f, 604.2f, 202.7f, 154.6f,  -2.1f, -57.1f },
    { "Snare Sidestick",           645.6f, 618.6f, 213.1f, 169.3f,  -1.8f, -52.2f },
    { "Kick",                     1012.3f, 846.3f, 269.9f, 203.2f,   0.0f,   0.0f },
    { "Tom 18",                   1349.6f, 492.4f, 200.5f,  75.2f,   0.0f,   0.0f },
    { "Tom 22",                   1663.8f, 630.7f, 241.7f,  84.0f,   2.5f,   0.0f },
    { "Hi-hat Pedal",              365.9f,1051.2f,  95.0f, 136.1f, -21.1f,   2.4f },
};

// Effective Y-radius after perspective tilt is applied.  Used by paint and
// hit-test so a tilted ellipse renders + responds to clicks consistently.
inline float effectiveRy (const auto& h) noexcept
{
    const float t = juce::degreesToRadians (juce::jlimit (-89.0f, 89.0f, h.tiltDeg));
    return h.ry * std::cos (t);
}

constexpr int kTiltHandleFill = 0xff7fffd4;   // distinct teal so tilt vs rotate are obvious

constexpr int kSelectedOutline    = 0xffffe680;
constexpr int kCalibUnselected    = 0xff4dd2ff;
constexpr int kPressedOutline     = 0xffffaa55;
constexpr int kHandleFill         = 0xffffe680;
constexpr int kHandleOutline      = 0xff202020;
constexpr int kRotationHandleFill = 0xff4dd2ff;
}

// ── Static API: articulation labels for the dropdown ─────────────────────────
juce::StringArray BaySickRustyDrumsKitGraphic::getArticulationLabels()
{
    juce::StringArray a;
    for (const auto& def : kArtics) a.add (def.label);
    return a;
}

// ── Construction ─────────────────────────────────────────────────────────────
BaySickRustyDrumsKitGraphic::BaySickRustyDrumsKitGraphic (BaySickRustyDrumsProcessor* engine)
    : mEngine (engine)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);

    int dataSize = 0;
    if (auto* imgData = BaySickAssets::getNamedResource ("big_rusty_drums_png", dataSize))
        if (dataSize > 0)
            mKitImage = juce::ImageFileFormat::loadFrom (imgData, (size_t) dataSize);

    int sbSize = 0;
    if (auto* sb = BaySickAssets::getNamedResource ("control_tab_png", sbSize))
        if (sbSize > 0)
            mSideBandImage = juce::ImageFileFormat::loadFrom (sb, (size_t) sbSize);

    resetLayoutToDefaults();
}

BaySickRustyDrumsKitGraphic::~BaySickRustyDrumsKitGraphic() = default;

void BaySickRustyDrumsKitGraphic::resetLayoutToDefaults()
{
    mHitboxes.clear();
    mHitboxes.reserve (std::size (kDefaultEllipses));
    for (const auto& d : kDefaultEllipses)
        mHitboxes.push_back ({ d.label, d.cx, d.cy, d.rx, d.ry, d.rotDeg, d.tiltDeg });
    mSelectedIdx = -1;
    mHoverIdx    = -1;
    repaint();
}

void BaySickRustyDrumsKitGraphic::setCalibrationMode (bool enabled)
{
    if (mCalibrationMode == enabled) return;
    mCalibrationMode = enabled;
    if (! enabled)
    {
        mSelectedIdx = -1;
        mDragMode    = DragMode::None;
    }
    setMouseCursor (enabled ? juce::MouseCursor::NormalCursor
                            : juce::MouseCursor::PointingHandCursor);
    repaint();
}

void BaySickRustyDrumsKitGraphic::setKitLoaded (bool loaded)
{
    if (mKitLoaded == loaded) return;
    mKitLoaded = loaded;
    setMouseCursor (loaded ? juce::MouseCursor::PointingHandCursor
                           : juce::MouseCursor::NormalCursor);
    repaint();
}

void BaySickRustyDrumsKitGraphic::resized() { repaint(); }

// ── Coordinate conversions ───────────────────────────────────────────────────
juce::Point<float>
BaySickRustyDrumsKitGraphic::imagePointFromComponent (juce::Point<float> p) const
{
    if (mLastDrawArea.isEmpty()) return { 0.f, 0.f };
    const float sx = mLastDrawArea.getWidth()  / kImageW;
    const float sy = mLastDrawArea.getHeight() / kImageH;
    return { (p.x - mLastDrawArea.getX()) / juce::jmax (1e-6f, sx),
             (p.y - mLastDrawArea.getY()) / juce::jmax (1e-6f, sy) };
}

juce::Point<float>
BaySickRustyDrumsKitGraphic::componentPointFromImage (juce::Point<float> p) const
{
    const float sx = mLastDrawArea.getWidth()  / kImageW;
    const float sy = mLastDrawArea.getHeight() / kImageH;
    return { mLastDrawArea.getX() + p.x * sx,
             mLastDrawArea.getY() + p.y * sy };
}

juce::Point<float>
BaySickRustyDrumsKitGraphic::toEllipseLocal (const Hitbox& h, juce::Point<float> ip) const
{
    const float dx = ip.x - h.cx;
    const float dy = ip.y - h.cy;
    const float rad = juce::degreesToRadians (-h.rotDeg);
    const float c = std::cos (rad), s = std::sin (rad);
    return { dx * c - dy * s, dx * s + dy * c };
}

// ── Painting ─────────────────────────────────────────────────────────────────
void BaySickRustyDrumsKitGraphic::paintHitboxOutline (juce::Graphics& g, const Hitbox& h,
                                                       juce::Colour col, float strokeW) const
{
    const float eRy = effectiveRy (h);
    juce::Path p;
    p.addEllipse (-h.rx, -eRy, h.rx * 2, eRy * 2);
    juce::AffineTransform xf;
    xf = xf.rotated (juce::degreesToRadians (h.rotDeg))
           .translated (h.cx, h.cy);

    // Build a component-space transform: image → component
    const float sx = mLastDrawArea.getWidth()  / kImageW;
    const float sy = mLastDrawArea.getHeight() / kImageH;
    juce::AffineTransform finalXf = xf.followedBy (
        juce::AffineTransform::scale (sx, sy)
            .translated (mLastDrawArea.getX(), mLastDrawArea.getY()));

    g.setColour (col);
    g.strokePath (p, juce::PathStrokeType (strokeW), finalXf);
}

void BaySickRustyDrumsKitGraphic::paintHitboxHandles (juce::Graphics& g, const Hitbox& h) const
{
    // Compute the 8 axis-aligned bounding-box handles in ellipse-local space,
    // then transform each through rotation + translation + image→component.
    // Tilt compresses the local Y, so handles use effective_ry.
    const float eRy = effectiveRy (h);
    const float lr[2] = { -h.rx,  h.rx };   // left / right
    const float tb[2] = { -eRy,   eRy   };  // top / bottom
    const float mid = 0.0f;

    const juce::Point<float> centersLocal[8] = {
        { lr[0], tb[0] },                 // NW
        { mid,   tb[0] },                 // N
        { lr[1], tb[0] },                 // NE
        { lr[0], mid   },                 // W
        { lr[1], mid   },                 // E
        { lr[0], tb[1] },                 // SW
        { mid,   tb[1] },                 // S
        { lr[1], tb[1] },                 // SE
    };

    const float rad = juce::degreesToRadians (h.rotDeg);
    const float c = std::cos (rad), s = std::sin (rad);

    for (auto& cl : centersLocal)
    {
        const float ix = cl.x * c - cl.y * s + h.cx;
        const float iy = cl.x * s + cl.y * c + h.cy;
        const auto cp = componentPointFromImage ({ ix, iy });
        const float hs = 7.0f;
        juce::Rectangle<float> hr (cp.x - hs, cp.y - hs, hs * 2, hs * 2);
        g.setColour (juce::Colour (kHandleFill));
        g.fillRect (hr);
        g.setColour (juce::Colour (kHandleOutline));
        g.drawRect (hr, 1.0f);
    }

    // Rotation handle (Z-axis spin): small disc above the ellipse top, on a stalk.
    const float stalkLen = juce::jmax (40.0f, eRy * 0.5f);
    auto localToImage = [&] (juce::Point<float> loc) -> juce::Point<float> {
        return { loc.x * c - loc.y * s + h.cx,
                 loc.x * s + loc.y * c + h.cy };
    };
    const auto topImg = localToImage ({ 0.0f, -eRy });
    const auto rotImg = localToImage ({ 0.0f, -eRy - stalkLen });
    const auto topComp = componentPointFromImage (topImg);
    const auto rcp     = componentPointFromImage (rotImg);

    g.setColour (juce::Colour (kRotationHandleFill).withAlpha (0.75f));
    g.drawLine (topComp.x, topComp.y, rcp.x, rcp.y, 1.5f);
    g.setColour (juce::Colour (kRotationHandleFill));
    g.fillEllipse (rcp.x - 7.0f, rcp.y - 7.0f, 14.0f, 14.0f);
    g.setColour (juce::Colour (kHandleOutline));
    g.drawEllipse (rcp.x - 7.0f, rcp.y - 7.0f, 14.0f, 14.0f, 1.0f);

    // Tilt handle (X-axis perspective tilt): teal disc on the LEFT of the
    // ellipse, on a stalk perpendicular to the rotation handle.  Drag it
    // up/down (in image space, after rotation) to tilt the ellipse forward
    // / backward — compresses the rendered + hit-tested ry.
    const float tiltStalk = juce::jmax (40.0f, h.rx * 0.5f);
    const auto leftImg = localToImage ({ -h.rx, 0.0f });
    const auto tiltImg = localToImage ({ -h.rx - tiltStalk, 0.0f });
    const auto leftComp = componentPointFromImage (leftImg);
    const auto tcp      = componentPointFromImage (tiltImg);

    g.setColour (juce::Colour (kTiltHandleFill).withAlpha (0.75f));
    g.drawLine (leftComp.x, leftComp.y, tcp.x, tcp.y, 1.5f);
    g.setColour (juce::Colour (kTiltHandleFill));
    g.fillEllipse (tcp.x - 7.0f, tcp.y - 7.0f, 14.0f, 14.0f);
    g.setColour (juce::Colour (kHandleOutline));
    g.drawEllipse (tcp.x - 7.0f, tcp.y - 7.0f, 14.0f, 14.0f, 1.0f);
}

void BaySickRustyDrumsKitGraphic::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xff0d0d0d));

    if (mKitImage.isNull())
    {
        g.setColour (juce::Colours::grey);
        g.setFont (juce::Font (14.0f));
        g.drawText ("Kit graphic unavailable", bounds, juce::Justification::centred);
        return;
    }

    // J-8 stage 1: full-bleed layout — no inner padding, no black border.
    // The largest 5:3 sub-rectangle that fits inside the component centers
    // and the side bands fill what's left.
    const float aspect = kImageW / kImageH;
    auto drawArea = bounds;
    const float fitW = juce::jmin (drawArea.getWidth(),  drawArea.getHeight() * aspect);
    const float fitH = juce::jmin (drawArea.getHeight(), drawArea.getWidth()  / aspect);
    drawArea = juce::Rectangle<float> (
        drawArea.getCentreX() - fitW * 0.5f,
        drawArea.getCentreY() - fitH * 0.5f,
        fitW, fitH);
    mLastDrawArea = drawArea;

    // When no kit is loaded, render at 50% opacity so the overlay text
    // ("Pick a program to begin") reads clearly against the dimmed photo.
    const float kitAlpha = mKitLoaded ? 1.0f : 0.5f;
    g.setOpacity (kitAlpha);
    g.drawImage (mKitImage, drawArea, juce::RectanglePlacement::stretchToFit);
    g.setOpacity (1.0f);

    // J-8c: fill the side bands (component area not covered by the 5:3 kit
    // photo) with a 90°-rotated copy of the kit's ARIA control panel.
    // Replaces dead-black space with kit-themed art.  Bands are mirrored:
    // left-band rotates 90° CCW, right-band rotates 90° CW, so the original
    // top-of-panel faces inward toward the kit on both sides.
    if (mSideBandImage.isValid())
    {
        const auto compBounds = getLocalBounds().toFloat();
        const float bandLeftW  = drawArea.getX() - compBounds.getX();
        const float bandRightW = compBounds.getRight() - drawArea.getRight();
        const float bandH      = drawArea.getHeight();
        const float bandY      = drawArea.getY();
        const float srcW = (float) mSideBandImage.getWidth();
        const float srcH = (float) mSideBandImage.getHeight();

        g.setOpacity (kitAlpha);

        if (bandLeftW > 1.0f)
        {
            // Left band: rotate 90° CCW.
            const float sx = bandLeftW / srcH;
            const float sy = bandH     / srcW;
            juce::AffineTransform xf =
                juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
                    .translated (0.0f, srcW)
                    .scaled (sx, sy)
                    .translated (compBounds.getX(), bandY);
            g.drawImageTransformed (mSideBandImage, xf);
        }
        if (bandRightW > 1.0f)
        {
            // Right band: rotate 90° CW (mirror of left).
            const float sx = bandRightW / srcH;
            const float sy = bandH      / srcW;
            juce::AffineTransform xf =
                juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi)
                    .translated (srcH, 0.0f)
                    .scaled (sx, sy)
                    .translated (drawArea.getRight(), bandY);
            g.drawImageTransformed (mSideBandImage, xf);
        }

        g.setOpacity (1.0f);
    }

    // J-8 (2026-05-04): grey out hitbox regions whose channel isn't in the
    // loaded program (Basic-mode visual feedback — Crash Sizzle 17, Stack,
    // China 18, Tom 22, Ride Sizzle 19 don't exist in Basic, so we paint a
    // semi-transparent dark overlay over their image areas).  Only runs when
    // a kit IS loaded; pre-load already dims everything via kitAlpha.
    if (mKitLoaded && mEngine != nullptr)
    {
        std::set<juce::String> presentChannels;
        for (const auto& ch : mEngine->getChannels())
            presentChannels.insert (ch.name);

        const float sx = drawArea.getWidth()  / kImageW;
        const float sy = drawArea.getHeight() / kImageH;

        for (const auto& h : mHitboxes)
        {
            const auto* def = findArtic (h.label);
            if (def == nullptr || juce::String (def->channelName).isEmpty()) continue;
            if (presentChannels.count (juce::String (def->channelName)) > 0) continue;

            const float eRy = effectiveRy (h);
            if (h.rx <= 0.0f || eRy <= 0.0f) continue;

            // Same image-space ellipse + rotation + scale-to-component transform
            // chain that paintHitboxOutline uses.  Filled, not stroked.
            juce::Path p;
            p.addEllipse (-h.rx, -eRy, h.rx * 2.0f, eRy * 2.0f);
            juce::AffineTransform xf;
            xf = xf.rotated (juce::degreesToRadians (h.rotDeg))
                   .translated (h.cx, h.cy);
            juce::AffineTransform finalXf = xf.followedBy (
                juce::AffineTransform::scale (sx, sy)
                    .translated (drawArea.getX(), drawArea.getY()));

            g.setColour (juce::Colour (0xff0d0d0d).withAlpha (0.78f));
            g.fillPath (p, finalXf);
        }
    }

    // J-8 stage 1: pre-load overlay text.  Drawn over everything, full-alpha,
    // so it reads cleanly against the dimmed kit photo + side panels.
    if (! mKitLoaded)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        const auto overlay = bounds.withSizeKeepingCentre (
            juce::jmin (bounds.getWidth(),  640.0f),
            juce::jmin (bounds.getHeight(), 90.0f));
        g.fillRoundedRectangle (overlay, 8.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (24.0f).withStyle ("Bold")));
        g.drawText ("Pick a program to begin",
                    overlay, juce::Justification::centred, false);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
    }

    // Calibration mode: draw all ellipses + handles on selected.
    if (mCalibrationMode)
    {
        for (size_t i = 0; i < mHitboxes.size(); ++i)
        {
            const auto& h = mHitboxes[i];
            const bool isSelected = ((int) i == mSelectedIdx);
            const bool unassigned = (h.label == "(unassigned)");

            const auto col = unassigned ? juce::Colour (0xffff5555)
                                        : juce::Colour (isSelected ? kSelectedOutline
                                                                   : kCalibUnselected);
            paintHitboxOutline (g, h, col.withAlpha (isSelected ? 0.95f : 0.55f),
                                isSelected ? 3.0f : 1.5f);

            // Label tag near the ellipse center
            const auto cp = componentPointFromImage ({ h.cx, h.cy });
            const juce::Rectangle<float> lblBg (cp.x - 80.0f, cp.y - 9.0f, 160.0f, 18.0f);
            g.setColour (juce::Colour (0xff000000).withAlpha (0.7f));
            g.fillRect (lblBg);
            g.setColour (juce::Colours::white);
            g.setFont (juce::Font (12.0f));
            g.drawText (h.label, lblBg.reduced (4.0f, 0.0f),
                        juce::Justification::centred, true);

            if (isSelected) paintHitboxHandles (g, h);
        }
        return;
    }

    // Normal mode: only show a thin outline while a piece is held pressed.
    // Hover state (mHoverIdx) is still tracked for the tooltip, but no
    // visual ring on hover — keeps the kit photo unobscured during browsing.
    if (mPressedIdx >= 0 && mPressedIdx < (int) mHitboxes.size())
    {
        const auto& h = mHitboxes[(size_t) mPressedIdx];
        paintHitboxOutline (g, h, juce::Colour (kCalibUnselected).withAlpha (0.9f), 1.5f);
    }

    if (mEngine != nullptr)
    {
        for (const auto& h : mHitboxes)
        {
            const auto* def = findArtic (h.label);
            if (def == nullptr || ! def->isPedalToggle) continue;

            const bool closed = mEngine->isHiHatPedalClosed();
            const auto col = closed ? juce::Colour (0xffe04040) : juce::Colour (0xff4ce06b);
            paintHitboxOutline (g, h, col.withAlpha (0.85f), 2.0f);
            // Pedal state label centered in the ellipse bounding box
            const auto cp = componentPointFromImage ({ h.cx, h.cy });
            g.setColour (juce::Colours::white);
            g.setFont (juce::Font (12.0f));
            g.drawText (closed ? "PEDAL: CLOSED" : "PEDAL: OPEN",
                        juce::Rectangle<float> (cp.x - 80.0f, cp.y - 9.0f, 160.0f, 18.0f),
                        juce::Justification::centred, true);
            break;
        }
    }
}

// ── Hit testing ──────────────────────────────────────────────────────────────
int BaySickRustyDrumsKitGraphic::hitTestPiece (juce::Point<float> screenPt) const
{
    const auto p = imagePointFromComponent (screenPt);
    int best = -1;
    float bestArea = std::numeric_limits<float>::max();
    for (size_t i = 0; i < mHitboxes.size(); ++i)
    {
        const auto& h = mHitboxes[i];
        const float eRy = effectiveRy (h);
        if (h.rx <= 0.0f || eRy <= 0.0f) continue;
        const auto loc = toEllipseLocal (h, p);
        const float nx = loc.x / h.rx;
        const float ny = loc.y / eRy;
        if (nx * nx + ny * ny <= 1.0f)
        {
            const float area = h.rx * eRy;
            if (area < bestArea) { bestArea = area; best = (int) i; }
        }
    }
    return best;
}

BaySickRustyDrumsKitGraphic::DragMode
BaySickRustyDrumsKitGraphic::hitTestHandle (juce::Point<float> screenPt, int boxIdx) const
{
    if (boxIdx < 0 || boxIdx >= (int) mHitboxes.size()) return DragMode::None;
    const auto& h = mHitboxes[(size_t) boxIdx];
    const float eRy = effectiveRy (h);
    const auto p = imagePointFromComponent (screenPt);
    const auto loc = toEllipseLocal (h, p);
    const float hot = kHandleHotImage;

    auto near = [&] (float lx, float ly) {
        return std::abs (loc.x - lx) <= hot && std::abs (loc.y - ly) <= hot;
    };

    // Rotation handle — sits above the ellipse top.
    const float stalkLen = juce::jmax (40.0f, eRy * 0.5f);
    if (near (0.0f, -eRy - stalkLen)) return DragMode::Rotate;

    // Tilt handle — sits to the left of the ellipse.
    const float tiltStalk = juce::jmax (40.0f, h.rx * 0.5f);
    if (near (-h.rx - tiltStalk, 0.0f)) return DragMode::Tilt;

    if (near (-h.rx, -eRy)) return DragMode::ResizeNW;
    if (near ( 0.0f, -eRy)) return DragMode::ResizeN;
    if (near ( h.rx, -eRy)) return DragMode::ResizeNE;
    if (near (-h.rx,  0.0f)) return DragMode::ResizeW;
    if (near ( h.rx,  0.0f)) return DragMode::ResizeE;
    if (near (-h.rx,  eRy)) return DragMode::ResizeSW;
    if (near ( 0.0f,  eRy)) return DragMode::ResizeS;
    if (near ( h.rx,  eRy)) return DragMode::ResizeSE;

    // Inside the ellipse → move
    const float nx = loc.x / juce::jmax (1e-3f, h.rx);
    const float ny = loc.y / juce::jmax (1e-3f, eRy);
    if (nx * nx + ny * ny <= 1.0f) return DragMode::Move;

    return DragMode::None;
}

void BaySickRustyDrumsKitGraphic::updateCursorForHover (juce::Point<float> screenPt)
{
    if (! mCalibrationMode)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        return;
    }

    DragMode m = DragMode::None;
    if (mSelectedIdx >= 0) m = hitTestHandle (screenPt, mSelectedIdx);
    if (m == DragMode::None && hitTestPiece (screenPt) >= 0) m = DragMode::Move;

    switch (m)
    {
        case DragMode::Move:                                  setMouseCursor (juce::MouseCursor::DraggingHandCursor); break;
        case DragMode::Rotate:
        case DragMode::Tilt:                                  setMouseCursor (juce::MouseCursor::PointingHandCursor); break;
        case DragMode::ResizeN:  case DragMode::ResizeS:      setMouseCursor (juce::MouseCursor::TopEdgeResizeCursor); break;
        case DragMode::ResizeW:  case DragMode::ResizeE:      setMouseCursor (juce::MouseCursor::LeftEdgeResizeCursor); break;
        case DragMode::ResizeNW: case DragMode::ResizeSE:     setMouseCursor (juce::MouseCursor::TopLeftCornerResizeCursor); break;
        case DragMode::ResizeNE: case DragMode::ResizeSW:     setMouseCursor (juce::MouseCursor::TopRightCornerResizeCursor); break;
        default:                                              setMouseCursor (juce::MouseCursor::NormalCursor); break;
    }
}

// ── Mouse interaction ────────────────────────────────────────────────────────
void BaySickRustyDrumsKitGraphic::mouseDown (const juce::MouseEvent& e)
{
    if (mCalibrationMode)
    {
        if (mSelectedIdx >= 0)
        {
            const auto m = hitTestHandle (e.position, mSelectedIdx);
            if (m != DragMode::None)
            {
                mDragMode          = m;
                mDragStartImagePos = imagePointFromComponent (e.position);
                mDragOriginalBox   = mHitboxes[(size_t) mSelectedIdx];
                return;
            }
        }
        const int idx = hitTestPiece (e.position);
        if (idx >= 0)
        {
            mSelectedIdx       = idx;
            mDragMode          = DragMode::Move;
            mDragStartImagePos = imagePointFromComponent (e.position);
            mDragOriginalBox   = mHitboxes[(size_t) idx];
        }
        else
        {
            mSelectedIdx = -1;
            mDragMode    = DragMode::None;
        }
        repaint();
        return;
    }

    // Audition mode — disabled when no kit is loaded (overlay state).
    if (! mKitLoaded) return;
    const int idx = hitTestPiece (e.position);
    if (idx < 0) return;
    mPressedIdx = idx;
    mHoverIdx   = idx;

    const auto& h = mHitboxes[(size_t) idx];
    const auto* def = findArtic (h.label);
    if (def == nullptr) { repaint(); return; }

    if (def->isPedalToggle)
    {
        if (mEngine != nullptr)
            mEngine->setHiHatPedalClosed (! mEngine->isHiHatPedalClosed());
        repaint();
        return;
    }

    if (def->midiNote < 0) { repaint(); return; }   // unassigned, no action

    if (mEngine != nullptr)
    {
        // Velocity from click position within the ellipse + pen pressure
        const auto p = imagePointFromComponent (e.position);
        const auto loc = toEllipseLocal (h, p);
        const float nx = loc.x / juce::jmax (1.0f, h.rx);
        const float ny = loc.y / juce::jmax (1.0f, h.ry);
        const float distNorm = juce::jlimit (0.0f, 1.0f, std::sqrt (nx * nx + ny * ny));
        constexpr float kPosVelMin = 0.45f;
        const float positionFactor = 1.0f - distNorm * (1.0f - kPosVelMin);
        float pressure = e.pressure;
        if (! (pressure > 0.05f && pressure <= 1.0f)) pressure = 1.0f;
        const int velocity = (int) std::round (
            juce::jlimit (1.0f, 127.0f, positionFactor * pressure * 127.0f));

        // Hi-hat pedal-state key swap (Tip / Shaft)
        int midi = def->midiNote;
        if (def->pedalOpenNote > 0 && ! mEngine->isHiHatPedalClosed())
            midi = def->pedalOpenNote;

        mEngine->auditionNote (midi, velocity);
    }
    repaint();
}

void BaySickRustyDrumsKitGraphic::mouseDrag (const juce::MouseEvent& e)
{
    if (! mCalibrationMode || mDragMode == DragMode::None || mSelectedIdx < 0) return;

    const auto cur = imagePointFromComponent (e.position);
    Hitbox h = mDragOriginalBox;

    switch (mDragMode)
    {
        case DragMode::Move:
        {
            h.cx = mDragOriginalBox.cx + (cur.x - mDragStartImagePos.x);
            h.cy = mDragOriginalBox.cy + (cur.y - mDragStartImagePos.y);
            break;
        }
        case DragMode::Rotate:
        {
            // Angle from center to current cursor minus angle from center to drag start.
            const float a0 = std::atan2 (mDragStartImagePos.y - h.cy, mDragStartImagePos.x - h.cx);
            const float a1 = std::atan2 (cur.y - h.cy, cur.x - h.cx);
            const float deltaDeg = juce::radiansToDegrees (a1 - a0);
            h.rotDeg = mDragOriginalBox.rotDeg + deltaDeg;
            break;
        }
        case DragMode::Tilt:
        {
            // Convert drag movement into ellipse-local space; the component
            // of motion along the ellipse's local Y axis maps to tilt.
            // Drag toward the ellipse center → less tilt; drag away → more.
            const auto locStart = toEllipseLocal (mDragOriginalBox, mDragStartImagePos);
            const auto locCur   = toEllipseLocal (mDragOriginalBox, cur);
            // Vertical drag along local Y inversely affects how "edge-on"
            // the ellipse is.  Scale so a few hundred pixels of drag covers
            // the full 0..89° range.
            const float deltaPx = locCur.y - locStart.y;
            const float kPxPerDeg = 4.0f;   // 4 px = 1°
            h.tiltDeg = juce::jlimit (-89.0f, 89.0f,
                                       mDragOriginalBox.tiltDeg + deltaPx / kPxPerDeg);
            break;
        }
        default:
        {
            // Resize: convert drag delta into ellipse-local space, then adjust
            // rx/ry plus shift center so the opposite handle stays anchored.
            const auto locStart = toEllipseLocal (mDragOriginalBox, mDragStartImagePos);
            const auto locCur   = toEllipseLocal (mDragOriginalBox, cur);
            const float ldx = locCur.x - locStart.x;
            const float ldy = locCur.y - locStart.y;

            float dxRx = 0.f, dyRy = 0.f;     // change to rx, ry
            float anchorXLocal = 0.f, anchorYLocal = 0.f;  // anchor (opposite handle) in old-local

            switch (mDragMode)
            {
                case DragMode::ResizeE:  dxRx =  ldx;          anchorXLocal = -mDragOriginalBox.rx; break;
                case DragMode::ResizeW:  dxRx = -ldx;          anchorXLocal =  mDragOriginalBox.rx; break;
                case DragMode::ResizeS:  dyRy =  ldy;          anchorYLocal = -mDragOriginalBox.ry; break;
                case DragMode::ResizeN:  dyRy = -ldy;          anchorYLocal =  mDragOriginalBox.ry; break;
                case DragMode::ResizeSE: dxRx =  ldx; dyRy =  ldy;
                                         anchorXLocal = -mDragOriginalBox.rx; anchorYLocal = -mDragOriginalBox.ry; break;
                case DragMode::ResizeSW: dxRx = -ldx; dyRy =  ldy;
                                         anchorXLocal =  mDragOriginalBox.rx; anchorYLocal = -mDragOriginalBox.ry; break;
                case DragMode::ResizeNE: dxRx =  ldx; dyRy = -ldy;
                                         anchorXLocal = -mDragOriginalBox.rx; anchorYLocal =  mDragOriginalBox.ry; break;
                case DragMode::ResizeNW: dxRx = -ldx; dyRy = -ldy;
                                         anchorXLocal =  mDragOriginalBox.rx; anchorYLocal =  mDragOriginalBox.ry; break;
                default: break;
            }

            constexpr float kMinR = 12.0f;
            h.rx = juce::jmax (kMinR, mDragOriginalBox.rx + dxRx * 0.5f);   // half because radius scales by /2
            h.ry = juce::jmax (kMinR, mDragOriginalBox.ry + dyRy * 0.5f);

            // Re-center so the anchor handle stays where it was on the image
            // (anchorXLocal, anchorYLocal) — translate the center by the
            // half-delta of rx/ry along the rotated axes.
            const float halfDx = (h.rx - mDragOriginalBox.rx) * (anchorXLocal > 0 ? -1.0f : 1.0f);
            const float halfDy = (h.ry - mDragOriginalBox.ry) * (anchorYLocal > 0 ? -1.0f : 1.0f);
            const float rad = juce::degreesToRadians (h.rotDeg);
            const float c = std::cos (rad), s = std::sin (rad);
            h.cx = mDragOriginalBox.cx + (halfDx * c - halfDy * s);
            h.cy = mDragOriginalBox.cy + (halfDx * s + halfDy * c);
            break;
        }
    }

    mHitboxes[(size_t) mSelectedIdx] = h;
    repaint();
}

void BaySickRustyDrumsKitGraphic::mouseUp (const juce::MouseEvent& /*e*/)
{
    if (mCalibrationMode)
    {
        mDragMode = DragMode::None;
        return;
    }
    if (mPressedIdx >= 0)
    {
        mPressedIdx = -1;
        repaint();
    }
}

void BaySickRustyDrumsKitGraphic::mouseMove (const juce::MouseEvent& e)
{
    updateCursorForHover (e.position);
    const int idx = hitTestPiece (e.position);
    if (idx != mHoverIdx)
    {
        mHoverIdx = idx;
        setTooltip (getTooltip());
        repaint();
    }
}

void BaySickRustyDrumsKitGraphic::mouseExit (const juce::MouseEvent& /*e*/)
{
    if (mHoverIdx != -1)
    {
        mHoverIdx = -1;
        setTooltip ({});
        repaint();
    }
}

juce::String BaySickRustyDrumsKitGraphic::getTooltip()
{
    if (mHoverIdx < 0 || mHoverIdx >= (int) mHitboxes.size()) return {};
    return mHitboxes[(size_t) mHoverIdx].label;
}

// ── Calibration: add / delete / label ────────────────────────────────────────
void BaySickRustyDrumsKitGraphic::addHitboxAtCentre()
{
    Hitbox h;
    h.label = "(unassigned)";
    h.cx = kImageW * 0.5f;
    h.cy = kImageH * 0.5f;
    h.rx = 80.0f;
    h.ry = 80.0f;
    h.rotDeg  = 0.0f;
    h.tiltDeg = 0.0f;
    mHitboxes.push_back (h);
    mSelectedIdx = (int) mHitboxes.size() - 1;
    repaint();
}

void BaySickRustyDrumsKitGraphic::deleteSelectedHitbox()
{
    if (mSelectedIdx < 0 || mSelectedIdx >= (int) mHitboxes.size()) return;
    mHitboxes.erase (mHitboxes.begin() + mSelectedIdx);
    mSelectedIdx = -1;
    mDragMode    = DragMode::None;
    repaint();
}

juce::String BaySickRustyDrumsKitGraphic::getSelectedLabel() const
{
    if (mSelectedIdx < 0 || mSelectedIdx >= (int) mHitboxes.size()) return {};
    return mHitboxes[(size_t) mSelectedIdx].label;
}

void BaySickRustyDrumsKitGraphic::setSelectedLabel (const juce::String& label)
{
    if (mSelectedIdx < 0 || mSelectedIdx >= (int) mHitboxes.size()) return;
    mHitboxes[(size_t) mSelectedIdx].label = label;
    repaint();
}

// ── Save layout ──────────────────────────────────────────────────────────────
bool BaySickRustyDrumsKitGraphic::saveLayoutToFile (juce::File dest)
{
    if (dest == juce::File())
        dest = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("BaySickDAW")
                   .getChildFile ("rusty_kit_hitboxes.txt");

    dest.getParentDirectory().createDirectory();

    juce::String s;
    s << "// Updated rusty-kit hitbox table.  Paste into kDefaultEllipses[] in\n"
         "// Source/BaySickRustyDrums/BaySickRustyDrumsKitGraphic.cpp.\n";
    s << "// Format: { label, cx, cy, rx, ry, rotationDegrees, tiltDegrees }\n";
    s << "constexpr DefaultEllipse kDefaultEllipses[] = {\n";
    for (const auto& h : mHitboxes)
    {
        auto fmt = [] (float v) { return juce::String (v, 1).paddedLeft (' ', 8); };
        s << "    { \"" << h.label << "\","
          << " " << fmt (h.cx) << "f,"
          << " " << fmt (h.cy) << "f,"
          << " " << fmt (h.rx) << "f,"
          << " " << fmt (h.ry) << "f,"
          << " " << fmt (h.rotDeg) << "f,"
          << " " << fmt (h.tiltDeg) << "f },\n";
    }
    s << "};\n";

    return dest.replaceWithText (s);
}
