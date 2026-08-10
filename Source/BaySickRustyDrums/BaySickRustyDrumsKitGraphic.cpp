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
    const char* channelName;          // J-8: which kit channel this articulation belongs to -
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
// rx = w/2, ry = h/2, rotation = 0.
struct DefaultEllipse
{
    const char* label;
    float cx, cy, rx, ry, rotDeg, tiltDeg;
};

// Calibrated 2026-05-04 against `Assets/big_rusty_drums.png`.
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

constexpr int kOutlineBlue        = 0xff4dd2ff;
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
    mHoverIdx = -1;
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

    // J-8 stage 1: full-bleed layout - no inner padding, no black border.
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
    // loaded program (Basic-mode visual feedback - Crash Sizzle 17, Stack,
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

    // Only show a thin outline while a piece is held pressed.
    // Hover state (mHoverIdx) is still tracked for the tooltip, but no
    // visual ring on hover - keeps the kit photo unobscured during browsing.
    if (mPressedIdx >= 0 && mPressedIdx < (int) mHitboxes.size())
    {
        const auto& h = mHitboxes[(size_t) mPressedIdx];
        paintHitboxOutline (g, h, juce::Colour (kOutlineBlue).withAlpha (0.9f), 1.5f);
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

// ── Mouse interaction ────────────────────────────────────────────────────────
void BaySickRustyDrumsKitGraphic::mouseDown (const juce::MouseEvent& e)
{
    // Audition is disabled when no kit is loaded (overlay state).
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

void BaySickRustyDrumsKitGraphic::mouseUp (const juce::MouseEvent& /*e*/)
{
    if (mPressedIdx >= 0)
    {
        mPressedIdx = -1;
        repaint();
    }
}

void BaySickRustyDrumsKitGraphic::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
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
