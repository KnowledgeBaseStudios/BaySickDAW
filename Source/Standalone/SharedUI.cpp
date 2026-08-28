#include "SharedUI.h"
#include "ShotMenuHook.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)
#include "UndoBracket.h"
#include "../ProjectManager.h"   // QA-RustyMeter Task 3: getSettingsFile (LUFS mode persistence)
#include "WindowChrome.h"        // TS7 §9.1: shared title-strip look
#include "BaySickTitleBar.h"     // QA-Layout T3: centered engine-name painter

// ── Filmstrip rendering ────────────────────────────────────────────────────────
namespace Filmstrips
{
    static juce::File getDir()
    {
        // Exe-relative so it resolves in the build tree AND an installed copy
        // alike: CMake stages repo Resources/ next to the exe post-build, and
        // the installer packages that same folder (G25 - the old dev-tree
        // relative path made every install lose the filmstrips).
        return juce::File::getSpecialLocation(juce::File::currentApplicationFile)
            .getParentDirectory()
            .getChildFile("Resources/Filmstrips");
    }

    static juce::Image loadStrip(const juce::String& filename)
    {
        auto f = getDir().getChildFile(filename);
        if (f.existsAsFile())
            return juce::ImageFileFormat::loadFrom(f);
        return {};
    }

    void drawFrame(juce::Graphics& g, const juce::Image& strip,
                   int frameW, int frameH, int numFrames,
                   float normalizedValue,
                   juce::Rectangle<float> destBounds)
    {
        if (!strip.isValid()) return;
        int frame = juce::jlimit(0, numFrames - 1,
                                 (int)std::round(normalizedValue * float(numFrames - 1)));
        int srcY  = frame * frameH;
        g.drawImage(strip,
                    (int)destBounds.getX(),   (int)destBounds.getY(),
                    (int)destBounds.getWidth(), (int)destBounds.getHeight(),
                    0, srcY, frameW, frameH);
    }

    const juce::Image& dynamics()
    {
        static juce::Image img = loadStrip("Dynamics Group Knobs.png");
        return img;
    }
    const juce::Image& harmonics()
    {
        static juce::Image img = loadStrip("Harmonics Group Knobs.png");
        return img;
    }
    const juce::Image& modulation()
    {
        static juce::Image img = loadStrip("Modulation Group Knobs.png");
        return img;
    }
    const juce::Image& timeBased()
    {
        static juce::Image img = loadStrip("Time Based Group Knobs.png");
        return img;
    }
    const juce::Image& chickenHead()
    {
        static juce::Image img = loadStrip("Chicken Head.png");
        return img;
    }
    const juce::Image& fader()
    {
        static juce::Image img = loadStrip("Fader Slider.png");
        return img;
    }
    const juce::Image& faderInverted()
    {
        static juce::Image img = []()
        {
            auto src = loadStrip("Fader Slider.png");
            if (!src.isValid()) return src;
            auto copy = src.createCopy();
            juce::Image::BitmapData bmp(copy, juce::Image::BitmapData::readWrite);
            for (int py = 0; py < bmp.height; ++py)
                for (int px = 0; px < bmp.width; ++px)
                {
                    auto c = bmp.getPixelColour(px, py);
                    bmp.setPixelColour(px, py,
                        juce::Colour((uint8)(255 - c.getRed()),
                                     (uint8)(255 - c.getGreen()),
                                     (uint8)(255 - c.getBlue()),
                                     c.getAlpha()));
                }
            return copy;
        }();
        return img;
    }
    const juce::Image& vuMeter()
    {
        static juce::Image img = loadStrip("VU Meter.png");
        return img;
    }
    const juce::Image& switchToggle()
    {
        static juce::Image img = loadStrip("Switch Toggle.png");
        return img;
    }
    const juce::Image& volumeBlack()
    {
        static juce::Image img = loadStrip("Volume Black.png");
        return img;
    }
    const juce::Image& volumeWhite()
    {
        static juce::Image img = loadStrip("Volume White.png");
        return img;
    }
}

// ============================================================ BaySickLAF
BaySickLAF::BaySickLAF()
{
    setColour(juce::Slider::thumbColourId,               VC::Highlight);
    setColour(juce::Slider::rotarySliderFillColourId,    VC::Highlight);
    setColour(juce::Slider::rotarySliderOutlineColourId, VC::Accent);
    setColour(juce::Slider::trackColourId,               VC::Accent);
    setColour(juce::Slider::backgroundColourId,          VC::Surface);
    setColour(juce::Label::textColourId,                 VC::Text);
    setColour(juce::ComboBox::backgroundColourId,        VC::Panel);
    setColour(juce::ComboBox::textColourId,              VC::Text);
    setColour(juce::ComboBox::outlineColourId,           VC::Accent);
    setColour(juce::PopupMenu::backgroundColourId,       VC::Panel);
    setColour(juce::PopupMenu::textColourId,             VC::Text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, VC::Highlight);
    setColour(juce::GroupComponent::outlineColourId,     VC::Accent);
    setColour(juce::GroupComponent::textColourId,        VC::TextDim);
    setColour(juce::ToggleButton::textColourId,          VC::Text);
    setColour(juce::ToggleButton::tickColourId,          VC::Highlight);
    setColour(juce::TextButton::buttonColourId,          VC::Accent);
    setColour(juce::TextButton::textColourOffId,         VC::Text);
    setColour(juce::TextButton::buttonOnColourId,        VC::Highlight);
    setColour(juce::TextButton::textColourOnId,          juce::Colours::white);
    setColour(juce::TextEditor::backgroundColourId,      VC::Surface);
    setColour(juce::TextEditor::textColourId,            VC::Text);
    setColour(juce::TextEditor::outlineColourId,         VC::Accent);
    setColour(juce::TabbedButtonBar::tabOutlineColourId, VC::Accent);
    setColour(juce::TabbedButtonBar::frontOutlineColourId, VC::Highlight.withAlpha(0.5f));
}

void BaySickLAF::drawRotarySlider(juce::Graphics& g, int x,int y,int w,int h,
                                float sp,float sa,float ea,juce::Slider& s)
{
    // Volume knob filmstrip (Black for Dynamics, White for all others)
    {
        auto variant = s.getProperties()["volumeKnob"].toString();
        if (variant.isNotEmpty())
        {
            const auto& strip = (variant == "black") ? Filmstrips::volumeBlack()
                                                     : Filmstrips::volumeWhite();
            if (strip.isValid())
            {
                Filmstrips::drawFrame(g, strip, 70, 70, 100, sp,
                                      juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h));
                return;
            }
        }
    }

    float cx = x + w * 0.5f, cy = y + h * 0.5f;
    float r  = juce::jmin(w, h) * 0.38f;
    bool  active = s.isMouseOverOrDragging();

    // ── Outer hover glow ─────────────────────────────────────────────────────
    if (active)
    {
        g.setColour(VC::Highlight.withAlpha(0.15f));
        g.fillEllipse(cx - r * 1.45f, cy - r * 1.45f, r * 2.9f, r * 2.9f);
    }

    // ── Arc track ────────────────────────────────────────────────────────────
    juce::Path tr;
    tr.addCentredArc(cx, cy, r, r, 0, sa, ea, true);
    g.setColour(VC::Surface);
    g.strokePath(tr, {2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded});

    // ── Tick marks: min / centre / max ───────────────────────────────────────
    auto drawTick = [&](float angle, float len, float thick)
    {
        float a  = angle - juce::MathConstants<float>::halfPi;
        float ix = cx + r * std::cos(a),       iy = cy + r * std::sin(a);
        float ox = cx + (r + len) * std::cos(a), oy = cy + (r + len) * std::sin(a);
        g.setColour(VC::Accent.brighter(0.4f));
        g.drawLine(ix, iy, ox, oy, thick);
    };
    drawTick(sa,            4.f, 1.5f);
    drawTick((sa + ea)*0.5f, 3.f, 1.0f);
    drawTick(ea,            4.f, 1.5f);

    // ── Filled value arc ─────────────────────────────────────────────────────
    if (sp > 0.001f)
    {
        juce::Colour fillCol = active ? VC::Highlight.brighter(0.25f) : VC::Highlight;
        // Subtle glow pass
        juce::Path fr;
        fr.addCentredArc(cx, cy, r, r, 0, sa, sa + sp * (ea - sa), true);
        g.setColour(fillCol.withAlpha(0.3f));
        g.strokePath(fr, {5.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded});
        // Main arc
        g.setColour(fillCol);
        g.strokePath(fr, {2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded});
    }

    // ── Metallic face (radial gradient: dark center → slightly lighter edge) ─
    {
        float faceR = r * 0.62f;
        juce::ColourGradient faceGrad(
            juce::Colour(0xff282830), cx, cy,
            juce::Colour(0xff1e1e26), cx + faceR * 0.7f, cy + faceR * 0.7f, true);
        g.setGradientFill(faceGrad);
        g.fillEllipse(cx - faceR, cy - faceR, faceR * 2.f, faceR * 2.f);
    }

    // ── Beveled ring ─────────────────────────────────────────────────────────
    {
        float ringR = r * 0.62f;
        // Top-left bright highlight
        g.setColour(VC::Chrome.withAlpha(0.35f));
        g.drawEllipse(cx - ringR - 0.5f, cy - ringR - 0.5f, (ringR + 0.5f) * 2.f, (ringR + 0.5f) * 2.f, 1.5f);
        // Bottom-right shadow
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawEllipse(cx - ringR + 0.5f, cy - ringR + 0.5f, (ringR - 0.5f) * 2.f, (ringR - 0.5f) * 2.f, 1.0f);
    }

    // ── Thumb indicator line ─────────────────────────────────────────────────
    {
        float ang    = sa + sp * (ea - sa) - juce::MathConstants<float>::halfPi;
        float faceR  = r * 0.62f;
        float tipR   = faceR * 0.85f;
        float baseR  = faceR * 0.2f;
        float tx1 = cx + tipR  * std::cos(ang), ty1 = cy + tipR  * std::sin(ang);
        float tx2 = cx + baseR * std::cos(ang), ty2 = cy + baseR * std::sin(ang);
        // LRX-6: off-white indicator (slightly warm, less clinical)
        static const juce::Colour kIndicator(0xffF5F0E2);
        g.setColour(kIndicator.withAlpha(0.28f));
        g.drawLine(tx2, ty2, tx1, ty1, 3.f);
        g.setColour(kIndicator.withAlpha(active ? 0.96f : 0.78f));
        g.drawLine(tx2, ty2, tx1, ty1, 1.5f);
    }
}
void BaySickLAF::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
                                float sp, float, float,
                                juce::Slider::SliderStyle style, juce::Slider& s)
{
    bool vert = (style == juce::Slider::LinearVertical);

    if (vert)
    {
        float cx = x + w * 0.5f;
        float fy = (float)y;
        float fh = (float)h;

        // Dynamics panel gets pixel-inverted filmstrip (cream bg - original has white markings).
        // All other panels use the original. Both sit directly on the panel with no backing.
        bool isDynamicsPanel = false;
        if (auto* parent = s.getParentComponent())
            isDynamicsPanel = (dynamic_cast<DynamicsLAF*>(&parent->getLookAndFeel()) != nullptr);

        const juce::Image& faderStrip = isDynamicsPanel ? Filmstrips::faderInverted()
                                                        : Filmstrips::fader();

        // Frame 0 = cap at bottom (min), Frame 30 = cap at top (max).
        // JUCE LinearVertical: sp → fy as value → max, so norm is inverted.
        float norm = juce::jlimit(0.f, 1.f, 1.f - (sp - fy) / fh);

        bool isMixerFader = s.getProperties().contains("mixerFader");
        bool isEqFader    = s.getProperties().contains("eqFader");
        if (isMixerFader || isEqFader)
        {
            // ── Programmatic mixer / EQ fader - metallic dark, HiDPI-safe ─────
            // Mixer dB range: -60 .. +10 (matches MixerTrackStrip constants).
            // EQ    dB range: -18 .. +18 (bipolar).
            // 2026-04-30: mixer fader max dropped +10 → +5.6 dB (FL parity).
            // Was +10 - too much boost, and the fader cap visually pinned past
            // the +10 tick when at max.  +5.6 matches FL's standard mixer cap.
            const float kFMin   = isEqFader ? -18.0f : -60.0f;
            const float kFMax   = isEqFader ?  18.0f :   5.6f;
            const float kFRange = kFMax - kFMin;
            // 2026-04-30: cap shrunk 57 → 35 px (kGuard 28.5 → 17.5).  Reclaims
            // ~22 px of visible track length on every strip and matches FL's
            // slim cap style.  See MixerTrackStrip kFaderBottomGuard which
            // mirrors this so the dB-label C-overlap calculation stays in sync.
            constexpr float kCapH   = 35.0f;
            constexpr float kTrackW = 3.0f;
            constexpr float kTickLen = 5.0f;
            constexpr float kGuard  = kCapH * 0.5f;   // dead zone at track ends (= 17.5)

            float capW  = std::max(11.0f, std::round((float)w * 0.195f) - 3.0f);
            float capX  = std::round(cx - capW * 0.5f);
            // Position cap using norm (same formula as marks) so they always agree.
            // norm=1 → capY=fy (top), norm=0 → capY=fy+fh-kCapH (bottom). No clamping needed.
            float capY  = std::round(fy + (fh - kCapH) * (1.0f - norm));
            float trackX  = std::round(cx - kTrackW * 0.5f);
            float trackY0 = fy + kGuard;
            float trackY1 = fy + fh - kGuard;

            // ── Track groove ─────────────────────────────────────────────────
            g.setColour(juce::Colour(0xff161616));
            g.fillRoundedRectangle(trackX, trackY0, kTrackW, trackY1 - trackY0, 1.5f);
            g.setColour(juce::Colour(0xff383838));
            g.drawLine(cx, trackY0 + 1.0f, cx, trackY1 - 1.0f, 1.0f);

            // ── Glow: tight to track line, from cap centre downward ───────────
            float glowTop = capY + kCapH * 0.5f;
            if (glowTop < trackY1)
            {
                juce::ColourGradient glow(
                    VC::Highlight.withAlpha(0.70f), cx, glowTop,
                    VC::Highlight.withAlpha(0.00f), cx, trackY1, false);
                g.setGradientFill(glow);
                g.fillRoundedRectangle(cx - kTrackW * 0.5f, glowTop,
                                       kTrackW, trackY1 - glowTop, 1.5f);
            }

            // ── dB scale (left of track) ──────────────────────────────────────
            // 2026-04-30: mixer fader range now -60..+5.6 dB (FL-parity max).
            // Marks compressed at the bottom (large jumps -30→-40→-60) and
            // tighter near 0 dB where mixing precision matters.  EQ faders
            // unchanged - symmetric bipolar -18..+18 is independent of mixer
            // changes.
            static const int kMixerMarks[] = { 5, 0, -5, -10, -15, -20, -25, -30, -40, -60 };
            static const int kEqMarks[]    = { 18, 12, 6, 0, -6, -12, -18 };
            const int* dbMarks    = isEqFader ? kEqMarks : kMixerMarks;
            const int  numDbMarks = isEqFader ? (int) (sizeof(kEqMarks)   / sizeof(int))
                                              : (int) (sizeof(kMixerMarks)/ sizeof(int));
            // Smaller font for EQ faders - narrow columns cant fit 14 pt 3-char labels.
            g.setFont(juce::Font(isEqFader ? 9.5f : 14.0f));

            // Same formula as capY: marks span [trackY0, trackY1] matching cap centre travel.
            const float travelTop = fy + kCapH * 0.5f;
            const float travelH   = fh - kCapH;

            for (int idx = 0; idx < numDbMarks; ++idx)
            {
                int db = dbMarks[idx];
                float normDb = ((float)db - kFMin) / kFRange;
                float markY  = travelTop + travelH * (1.0f - normDb);
                if (markY < fy || markY > fy + fh) continue;

                bool isZero = (db == 0);
                juce::Colour col = isZero ? juce::Colour(0xffff4444)
                                          : juce::Colour(0xff888888);
                g.setColour(col);

                // Tick pointing right toward the track
                float tickRX = trackX - 2.0f;
                float tickLX = tickRX - kTickLen;
                g.drawLine(tickLX, markY, tickRX, markY,
                           isZero ? 1.5f : 0.8f);

                // Label right-aligned to left of tick. No "+" prefix on EQ faders -
                // the symmetric -18..+18 scale's position already implies sign, and
                // dropping it saves one character of horizontal space. drawFittedText
                // lets JUCE horizontally squeeze the glyphs when the box is tight
                // rather than clip characters off the end (which is what produced
                // the "-1 / -1" truncation at 14 pt).
                const juce::String lbl = juce::String(db);
                const int lblW = (int)(tickLX - (float)x - 1.0f);
                if (lblW > 4)
                    g.drawFittedText(lbl, x, (int)(markY - 7), lblW, 14,
                                     juce::Justification::centredRight,
                                     1, 0.5f);
            }

            // ── Fader cap (metallic dark) ─────────────────────────────────────
            // Body gradient: lighter at top, darker at bottom
            float cr      = std::min(3.0f, capW * 0.35f);   // corner radius scales with width
            float insetH  = std::max(1.0f, capW * 0.12f);   // bevel/grip inset from sides
            juce::ColourGradient capGrad(
                juce::Colour(0xff585858), cx, capY,
                juce::Colour(0xff242424), cx, capY + kCapH, false);
            g.setGradientFill(capGrad);
            g.fillRoundedRectangle(capX, capY, capW, kCapH, cr);

            // Top bevel highlight
            g.setColour(juce::Colour(0xff6e6e6e));
            g.drawLine(capX + insetH, capY + 1.0f, capX + capW - insetH, capY + 1.0f, 0.75f);

            // Bottom shadow line
            g.setColour(juce::Colour(0xff0e0e0e));
            g.drawLine(capX + insetH, capY + kCapH - 1.5f,
                       capX + capW - insetH, capY + kCapH - 1.5f, 0.75f);

            // Cap border
            g.setColour(juce::Colour(0xff4a4a4a));
            g.drawRoundedRectangle(capX + 0.5f, capY + 0.5f,
                                   capW - 1.0f, kCapH - 1.0f, cr, 0.75f);

            // Centre reference line (bright indicator)
            float midY = capY + kCapH * 0.5f;
            g.setColour(juce::Colour(0xffd0d0d0));
            g.drawLine(capX + insetH, midY, capX + capW - insetH, midY, 1.0f);

            // Grip lines above and below centre
            g.setColour(juce::Colour(0xff3a3a3a));
            for (int i = 1; i <= 2; ++i)
            {
                float gy = midY - (float)i * 3.5f;
                if (gy > capY + 2.5f)
                    g.drawLine(capX + insetH, gy, capX + capW - insetH, gy, 0.6f);
                gy = midY + (float)i * 3.5f;
                if (gy < capY + kCapH - 2.5f)
                    g.drawLine(capX + insetH, gy, capX + capW - insetH, gy, 0.6f);
            }

            // ── EQ faders: live position pointer (track-side) ─────────────────
            // A short horizontal line at the cap's current Y that points from
            // the dB-scale tick zone to the cap's left edge, visually pegging
            // the cap onto the scale. The numeric dB readout lives BELOW the
            // fader inside the old EQ display (retired at QA-EqPro).
            if (isEqFader)
            {
                const float curDb = (float) s.getValue();
                const bool  centred = std::abs(curDb) < 0.05f;
                juce::Colour ptrCol = centred ? juce::Colour(0xffff4444)
                                              : juce::Colour(0xffffcc44);
                g.setColour(ptrCol.withAlpha(0.85f));
                const float ptrLX = trackX - kTickLen - 2.0f;
                const float ptrRX = capX - 1.0f;
                if (ptrRX > ptrLX + 1.0f)
                    g.drawLine(ptrLX, midY, ptrRX, midY, 1.2f);
            }

            // Disabled state: overlay a translucent dim rectangle across the
            // full fader bounds. Matches the VKnob::setLocked grey-out pattern
            // so non-gain-bearing band types (LP / HP / Notch / BP) visibly
            // read as "can't interact."
            if ((isEqFader || isMixerFader) && !s.isEnabled())
            {
                g.setColour(juce::Colour(0xff1a1a1a).withAlpha(0.60f));
                g.fillRect((float) x, fy, (float) w, fh);
            }
        }
        else
        {
            // Non-mixer vertical faders: filmstrip
            Filmstrips::drawFrame(g, faderStrip, 128, 128, 31, norm,
                                  juce::Rectangle<float>((float)x, fy, (float)w, fh));

            // Glow below thumb
            if (sp < fy + fh)
            {
                float trackW = juce::jmax(8.f, (float)w * 0.15f);
                juce::ColourGradient glowGrad(
                    VC::Highlight.withAlpha(0.70f), cx, sp,
                    VC::Highlight.withAlpha(0.f),   cx, fy + fh, false);
                g.setGradientFill(glowGrad);
                g.fillRoundedRectangle(cx - trackW * 0.5f, sp, trackW,
                                       fy + fh - sp, trackW * 0.5f);
            }
        }
    }
    else
    {
        float cy = y + h * 0.5f;

        // ── Groove ──────────────────────────────────────────────────────────
        g.setColour(VC::Bg.darker(0.3f));
        g.fillRoundedRectangle((float)x, cy - 3.5f, (float)w, 7.f, 3.5f);
        juce::ColourGradient grooveShade(juce::Colours::black.withAlpha(0.45f), (float)x, cy,
                                          juce::Colours::transparentBlack, (float)(x + 12), cy, false);
        g.setGradientFill(grooveShade);
        g.fillRoundedRectangle((float)x, cy - 3.5f, (float)w, 7.f, 3.5f);
        g.setColour(VC::Accent.withAlpha(0.45f));
        g.drawRoundedRectangle((float)x, cy - 3.5f, (float)w, 7.f, 3.5f, 1.f);

        // Fill left of thumb
        if (sp > x)
        {
            juce::ColourGradient fillGrad(VC::Highlight.withAlpha(0.3f), (float)x, cy,
                                           VC::Highlight.withAlpha(0.9f), sp, cy, false);
            g.setGradientFill(fillGrad);
            g.fillRoundedRectangle((float)x, cy - 2.5f, sp - (float)x, 5.f, 2.5f);
        }

        // Thumb - vertical cap for horizontal sliders
        {
            float tw = 10.f, th = 20.f;
            float tx = sp - tw * 0.5f, ty = cy - th * 0.5f;

            juce::DropShadow shadow(juce::Colours::black.withAlpha(0.45f), 5, {1, 0});
            juce::Path capPath;
            capPath.addRoundedRectangle(tx, ty, tw, th, 3.f);
            shadow.drawForPath(g, capPath);

            juce::ColourGradient capGrad(
                juce::Colour(0xffD8D8D8), tx, cy,
                juce::Colour(0xff707070), tx + tw, cy, false);
            capGrad.addColour(0.48, juce::Colour(0xffB0B0B0));
            g.setGradientFill(capGrad);
            g.fillRoundedRectangle(tx, ty, tw, th, 3.f);

            // Ribbed grips (vertical lines on horizontal cap)
            g.saveState();
            juce::Path clipPath;
            clipPath.addRoundedRectangle(tx, ty, tw, th, 3.f);
            g.reduceClipRegion(clipPath);
            g.setColour(juce::Colours::black.withAlpha(0.25f));
            for (int r = -1; r <= 1; ++r)
                g.drawVerticalLine((int)(sp + r * 1.8f), ty + 3.f, ty + th - 3.f);
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            for (int r = -1; r <= 1; ++r)
                g.drawVerticalLine((int)(sp + r * 1.8f + 0.5f), ty + 3.f, ty + th - 3.f);
            g.restoreState();

            g.setColour(juce::Colours::white.withAlpha(0.45f));
            g.drawVerticalLine((int)(tx + 1.5f), ty + 3.f, ty + th - 3.f);

            g.setColour(juce::Colour(0xff333333).withAlpha(0.7f));
            g.drawRoundedRectangle(tx, ty, tw, th, 3.f, 0.8f);
        }
    }
    (void)s;
}
void BaySickLAF::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool isOver, bool isDown)
{
    // 2026-04-26: per project rule, switch-style toggle (filmstrip / pill)
    // is reserved for effect-panel toggles, FX-rack slot toggles, mixer
    // pre/post send toggles, and player switch panels.  Other ToggleButtons
    // (e.g. confirm-dialog "Don't show again" checkboxes) opt OUT of switch
    // styling by leaving the "switchToggle" property unset, so they render
    // as the standard JUCE checkbox via LookAndFeel_V4.
    if (! b.getProperties().contains("switchToggle"))
    {
        juce::LookAndFeel_V4::drawToggleButton(g, b, isOver, isDown);
        return;
    }

    bool on = b.getToggleState();
    auto bounds = b.getLocalBounds().toFloat();

    // ── Switch Toggle filmstrip (46x46, 2 frames: 0=OFF, 1=ON) ───────────────
    {
        const auto& strip = Filmstrips::switchToggle();
        if (strip.isValid())
        {
            Filmstrips::drawFrame(g, strip, 46, 46, 2, on ? 1.f : 0.f, bounds);
            return;
        }
    }
    float radius = bounds.getHeight() * 0.4f;

    // Outer trench (recessed housing) - always drawn
    g.setColour(juce::Colour(0xff000000).withAlpha(0.55f));
    g.fillRoundedRectangle(bounds.expanded(1.f), radius + 1.f);
    g.setColour(juce::Colour(0xff000000).withAlpha(0.8f));
    g.drawRoundedRectangle(bounds.expanded(0.5f), radius + 0.5f, 1.f);

    if (on)
    {
        // Pushed in: very dark fill
        g.setColour(VC::Bg.darker(0.4f));
        g.fillRoundedRectangle(bounds, radius);
        // Inner shadow line at top (depth illusion)
        g.setColour(juce::Colour(0xff000000).withAlpha(0.5f));
        g.drawLine(bounds.getX() + radius, bounds.getY() + 1.5f,
                   bounds.getRight() - radius, bounds.getY() + 1.5f, 1.5f);
        // Neon accent bar at top
        g.setColour(VC::Highlight.withAlpha(0.9f));
        g.fillRoundedRectangle(bounds.getX() + 3.f, bounds.getY() + 1.5f,
                               bounds.getWidth() - 6.f, 2.f, 1.f);
        // Subtle glow
        g.setColour(VC::Highlight.withAlpha(0.12f));
        g.fillRoundedRectangle(bounds, radius);
        g.setColour(juce::Colours::white.withAlpha(0.92f));
    }
    else
    {
        // Raised: gradient top-to-bottom, light top edge
        juce::Colour base = isOver ? VC::Accent.brighter(0.15f) : VC::Accent;
        g.setGradientFill(juce::ColourGradient(
            base.brighter(0.12f), 0.f, bounds.getY(),
            base.darker(0.1f),   0.f, bounds.getBottom(), false));
        g.fillRoundedRectangle(bounds, radius);
        // Highlight on top edge
        g.setColour(juce::Colour(0x28ffffff));
        g.drawLine(bounds.getX() + radius, bounds.getY() + 1.f,
                   bounds.getRight() - radius, bounds.getY() + 1.f, 1.f);
        g.setColour(VC::TextDim);
    }
    g.setFont(juce::Font(10, juce::Font::bold));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

void BaySickLAF::drawButtonBackground(juce::Graphics& g, juce::Button& b,
                                    const juce::Colour&, bool isOver, bool isDown)
{
    auto bounds = b.getLocalBounds().toFloat();
    float radius = 3.f;

    const bool isOn             = b.getToggleState();
    const bool isClickToggle    = b.getClickingTogglesState();
    const bool isPlayerOrEffect = b.getProperties().contains("switchToggle");

    // Path A: Switch filmstrip - short clickToggle button + switchToggle property.
    // Reserved for effect-panel + player editor toggles.
    if (isClickToggle && bounds.getHeight() < 44.f && isPlayerOrEffect)
    {
        const auto& strip = Filmstrips::switchToggle();
        if (strip.isValid())
        {
            float sz = bounds.getHeight() - 2.f;
            g.setColour(isOn ? VC::Highlight.withAlpha(0.22f) : juce::Colour(0x55000000));
            g.fillRoundedRectangle(bounds, 3.f);
            g.setColour(VC::Accent.withAlpha(0.55f));
            g.drawRoundedRectangle(bounds.reduced(0.5f), 3.f, 1.f);
            Filmstrips::drawFrame(g, strip, 46, 46, 2, isOn ? 1.f : 0.f,
                                  juce::Rectangle<float>(bounds.getX() + 1.f,
                                                         bounds.getY() + 1.f,
                                                         sz, sz));
            return;
        }
    }

    // Path B: player/effect tall toggle TextButton - preserve existing nav-pill rendering.
    if (isClickToggle && isPlayerOrEffect)
    {
        if (isOn)
        {
            g.setColour(VC::Highlight.withAlpha(0.18f));
            g.fillRoundedRectangle(bounds.expanded(2.f), radius + 2.f);
            juce::ColourGradient onGrad(VC::Highlight.withAlpha(0.75f), 0.f, bounds.getY(),
                                         VC::Highlight.withAlpha(0.45f), 0.f, bounds.getBottom(), false);
            g.setGradientFill(onGrad);
            g.fillRoundedRectangle(bounds, radius);
            juce::ColourGradient insetShadow(juce::Colours::black.withAlpha(0.35f), 0.f, bounds.getY(),
                                              juce::Colours::transparentBlack,        0.f, bounds.getY() + 4.f, false);
            g.setGradientFill(insetShadow);
            g.fillRoundedRectangle(bounds, radius);
            g.setColour(VC::Highlight.brighter(0.3f).withAlpha(0.9f));
            g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.f);
        }
        else
        {
            juce::Colour base = isOver ? VC::Surface.brighter(0.12f) : VC::Surface;
            g.setGradientFill(juce::ColourGradient(
                base.brighter(0.08f), 0.f, bounds.getY(),
                base.darker(0.1f),   0.f, bounds.getBottom(), false));
            g.fillRoundedRectangle(bounds, radius);
            g.setColour(juce::Colours::white.withAlpha(isOver ? 0.18f : 0.10f));
            g.drawLine(bounds.getX() + radius, bounds.getY() + 0.75f,
                       bounds.getRight() - radius, bounds.getY() + 0.75f, 1.f);
            g.setColour(juce::Colours::black.withAlpha(0.22f));
            g.drawLine(bounds.getX() + radius, bounds.getBottom() - 0.75f,
                       bounds.getRight() - radius, bounds.getBottom() - 0.75f, 1.f);
            g.setColour(VC::Accent.withAlpha(0.45f));
            g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.f);
        }
        return;
    }

    // 2026-04-26: page-tab special case - body stays chrome, only an outer
    // accent ring + bloom indicates active.  Triggered by "outlineGlowOnly"
    // property set in setTabSlots.  Rendered via fall-through: don't return
    // here; let the OFF chrome path render the body first, then we add the
    // ring at the end.
    const bool outlineGlowOnly = isOn && b.getProperties().contains("outlineGlowOnly");

    // Path C: ON state - neon glow for active buttons that DON'T use the
    // outline-only style.  Custom buttonOnColourId is honored so transport
    // buttons (Metronome blue, SongMode purple) keep their identity colors;
    // otherwise default neon highlight (cyan).
    if (isOn && ! outlineGlowOnly)
    {
        juce::Colour onCol = b.isColourSpecified(juce::TextButton::buttonOnColourId)
                                 ? b.findColour(juce::TextButton::buttonOnColourId)
                                 : VC::Highlight;
        // Outer bloom glow
        g.setColour(onCol.withAlpha(0.18f));
        g.fillRoundedRectangle(bounds.expanded(2.f), radius + 2.f);
        // Fill
        juce::ColourGradient onGrad(onCol.withAlpha(0.75f), 0.f, bounds.getY(),
                                     onCol.withAlpha(0.45f), 0.f, bounds.getBottom(), false);
        g.setGradientFill(onGrad);
        g.fillRoundedRectangle(bounds, radius);
        // Pushed-in inner shadow (dark at top)
        juce::ColourGradient insetShadow(juce::Colours::black.withAlpha(0.35f), 0.f, bounds.getY(),
                                          juce::Colours::transparentBlack,        0.f, bounds.getY() + 4.f, false);
        g.setGradientFill(insetShadow);
        g.fillRoundedRectangle(bounds, radius);
        // Border
        g.setColour(onCol.brighter(0.3f).withAlpha(0.9f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.f);
        return;
    }

    // Path D: OFF state - chrome action button.  Matches non-toggle siblings so
    // a Snap-OFF or unselected tool button looks identical to the Tools/Zoom buttons
    // beside it.  This is also the path for ordinary non-toggle action buttons.
    if (isDown)
    {
        // Pressed: deep recessed, dark, inner shadow.
        g.setGradientFill(juce::ColourGradient(
            VC::Chrome.darker(0.35f), 0.f, bounds.getY(),
            VC::Chrome.darker(0.20f), 0.f, bounds.getBottom(), false));
        g.fillRoundedRectangle(bounds, radius);
        juce::ColourGradient pressedShadow(juce::Colours::black.withAlpha(0.55f), 0.f, bounds.getY(),
                                            juce::Colours::transparentBlack,        0.f, bounds.getY() + 6.f, false);
        g.setGradientFill(pressedShadow);
        g.fillRoundedRectangle(bounds, radius);
        g.setColour(VC::Bg.withAlpha(0.7f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.f);
        return;
    }

    // Resting OFF: chrome action button (raised slab).
    g.setColour(juce::Colours::black.withAlpha(0.30f));
    g.fillRoundedRectangle(bounds.translated(0.f, 1.5f), radius);
    juce::Colour topCol = isOver ? VC::Chrome.brighter(0.12f) : VC::Chrome;
    juce::Colour botCol = VC::Chrome.darker(0.32f);
    g.setGradientFill(juce::ColourGradient(topCol, 0.f, bounds.getY(),
                                            botCol, 0.f, bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds, radius);
    juce::ColourGradient sideHL(juce::Colours::white.withAlpha(0.20f), bounds.getX(), 0.f,
                                 juce::Colours::transparentWhite,       bounds.getX() + 3.f, 0.f, false);
    g.setGradientFill(sideHL);
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(juce::Colour(0x38ffffff));
    g.drawLine(bounds.getX() + radius, bounds.getY() + 1.f,
               bounds.getRight() - radius, bounds.getY() + 1.f, 1.f);
    g.setColour(juce::Colour(0x30000000));
    g.drawLine(bounds.getX() + radius, bounds.getBottom() - 1.f,
               bounds.getRight() - radius, bounds.getBottom() - 1.f, 1.f);
    g.setColour(VC::Accent.brighter(0.15f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.f);

    // 2026-04-26: outline-only active ring - page sub-tabs (Player/Piano Roll/EQ)
    // keep their chrome body untouched; the ONLY active-state visual is a
    // sharp accent border drawn on the button edge.  No bloom, no fill change.
    if (outlineGlowOnly)
    {
        juce::Colour ringCol = b.isColourSpecified(juce::TextButton::textColourOnId)
                                   ? b.findColour(juce::TextButton::textColourOnId)
                                   : VC::Highlight;
        g.setColour(ringCol.withAlpha(0.95f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.5f);
    }
}
juce::Font BaySickLAF::getLabelFont(juce::Label&) { return juce::Font(11); }

void BaySickLAF::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool /*isOver*/, bool /*isDown*/)
{
    // Small toggle buttons: switch icon is on the left, text goes to the right of it.
    // 2026-04-26: matches drawButtonBackground - opt-in via "switchToggle" property.
    if (b.getClickingTogglesState() && b.getHeight() < 44 && b.getProperties().contains("switchToggle"))
    {
        const auto& strip = Filmstrips::switchToggle();
        if (strip.isValid())
        {
            float sz = (float)b.getHeight() - 2.f;
            auto textR = b.getLocalBounds().withTrimmedLeft((int)(sz + 4.f));
            // Black text on Dynamics (cream) panels, white on all others
            bool darkPanel = (b.getParentComponent() != nullptr &&
                              dynamic_cast<DynamicsLAF*>(&b.getParentComponent()->getLookAndFeel()) != nullptr);
            g.setColour(darkPanel ? juce::Colour(0xff1a1208) : VC::Text);
            g.setFont(juce::Font(9.f, juce::Font::bold));
            g.drawFittedText(b.getButtonText(), textR, juce::Justification::centredLeft, 1);
            return;
        }
    }
    // Default rendering for all other buttons
    juce::LookAndFeel_V4::drawButtonText(g, b, false, false);
}

void BaySickLAF::drawScrollbar(juce::Graphics& g, juce::ScrollBar&,
                             int x, int y, int w, int h,
                             bool isVertical, int thumbStart, int thumbSize,
                             bool isOver, bool isDown)
{
    // LRX-14: Scrollbar - recessed gutter + chrome capsule thumb
    auto track = juce::Rectangle<int>(x, y, w, h).toFloat();

    // Track: deep recessed slot
    g.setColour(juce::Colour(0xff0A0A0A));
    g.fillRoundedRectangle(track, 3.f);
    // Inner shadow at top/left edge (recessed depth)
    juce::ColourGradient trackShadow(
        juce::Colours::black.withAlpha(0.55f), track.getX(), track.getY(),
        juce::Colours::transparentBlack,
        isVertical ? track.getX() : track.getX() + 5.f,
        isVertical ? track.getY() + 5.f : track.getY(), false);
    g.setGradientFill(trackShadow);
    g.fillRoundedRectangle(track, 3.f);

    if (thumbSize > 0)
    {
        auto thumb = isVertical
            ? juce::Rectangle<float>((float)x + 1.f, (float)(y + thumbStart) + 1.f,
                                      (float)w - 2.f, (float)thumbSize - 2.f)
            : juce::Rectangle<float>((float)(x + thumbStart) + 1.f, (float)y + 1.f,
                                      (float)thumbSize - 2.f, (float)h - 2.f);

        float thumbRadius = 3.f;

        // LRX-14: Contact shadow under thumb
        juce::DropShadow contactShadow(juce::Colours::black.withAlpha(0.45f), 3,
                                        isVertical ? juce::Point<int>(0, 1) : juce::Point<int>(1, 0));
        juce::Path thumbPath;
        thumbPath.addRoundedRectangle(thumb, thumbRadius);
        contactShadow.drawForPath(g, thumbPath);

        // Chrome thumb body
        bool bright = isOver || isDown;
        juce::ColourGradient thumbGrad;
        if (isVertical)
            thumbGrad = juce::ColourGradient(
                juce::Colour(bright ? 0xffD0D0D0 : 0xffAAAAAA), thumb.getX(), thumb.getY(),
                juce::Colour(bright ? 0xff606060 : 0xff484848), thumb.getRight(), thumb.getBottom(), false);
        else
            thumbGrad = juce::ColourGradient(
                juce::Colour(bright ? 0xffD0D0D0 : 0xffAAAAAA), thumb.getX(), thumb.getY(),
                juce::Colour(bright ? 0xff606060 : 0xff484848), thumb.getRight(), thumb.getBottom(), false);
        thumbGrad.addColour(0.5, juce::Colour(bright ? 0xffB0B0B0 : 0xff888888));
        g.setGradientFill(thumbGrad);
        g.fillRoundedRectangle(thumb, thumbRadius);

        // LRX-14: Anisotropic highlight (single bright stripe perpendicular to travel)
        if (isVertical)
        {
            float midY = thumb.getCentreY();
            g.setColour(juce::Colours::white.withAlpha(bright ? 0.35f : 0.20f));
            g.drawHorizontalLine((int)midY, thumb.getX() + 2.f, thumb.getRight() - 2.f);
        }
        else
        {
            float midX = thumb.getCentreX();
            g.setColour(juce::Colours::white.withAlpha(bright ? 0.35f : 0.20f));
            g.drawVerticalLine((int)midX, thumb.getY() + 2.f, thumb.getBottom() - 2.f);
        }

        // Fresnel rim
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawRoundedRectangle(thumb, thumbRadius, 0.8f);
    }
}

// ── TS7 §9.1/§9.3: desktop-window chrome ─────────────────────────────────────
// Painted through WindowChrome, the same helpers WorkspaceWindow uses, so the
// contained shell and the windows that cannot be contained wear one title strip.
namespace
{
    // JUCE's own close glyph is an X drawn from the button's colour; ours matches
    // the shell's "x" TextButton without dragging a TextButton into a
    // DocumentWindow's button slot (which expects a Button that paints itself).
    class ChromeCloseButton : public juce::Button
    {
    public:
        ChromeCloseButton() : juce::Button ("close") {}

        void paintButton (juce::Graphics& g, bool isOver, bool isDown) override
        {
            auto b = getLocalBounds().toFloat();
            if (isOver || isDown)
            {
                g.setColour (juce::Colours::white.withAlpha (isDown ? 0.22f : 0.12f));
                g.fillRect (b);
            }
            const auto x = b.reduced (b.getWidth() * 0.33f, b.getHeight() * 0.33f);
            g.setColour (WindowChrome::titleText());
            g.drawLine (x.getX(), x.getY(), x.getRight(), x.getBottom(), 1.4f);
            g.drawLine (x.getX(), x.getBottom(), x.getRight(), x.getY(), 1.4f);
        }
    };

    class ChromeMinimiseButton : public juce::Button
    {
    public:
        ChromeMinimiseButton() : juce::Button ("minimise") {}

        void paintButton (juce::Graphics& g, bool isOver, bool isDown) override
        {
            auto b = getLocalBounds().toFloat();
            if (isOver || isDown)
            {
                g.setColour (juce::Colours::white.withAlpha (isDown ? 0.22f : 0.12f));
                g.fillRect (b);
            }
            const auto x = b.reduced (b.getWidth() * 0.33f, b.getHeight() * 0.33f);
            g.setColour (WindowChrome::titleText());
            g.drawLine (x.getX(), x.getBottom(), x.getRight(), x.getBottom(), 1.4f);
        }
    };

    class ChromeMaximiseButton : public juce::Button
    {
    public:
        ChromeMaximiseButton() : juce::Button ("maximise") {}

        void paintButton (juce::Graphics& g, bool isOver, bool isDown) override
        {
            auto b = getLocalBounds().toFloat();
            if (isOver || isDown)
            {
                g.setColour (juce::Colours::white.withAlpha (isDown ? 0.22f : 0.12f));
                g.fillRect (b);
            }
            const auto x = b.reduced (b.getWidth() * 0.33f, b.getHeight() * 0.33f);
            g.setColour (WindowChrome::titleText());
            g.drawRect (x, 1.4f);
        }
    };
}

void BaySickLAF::drawDocumentWindowTitleBar (juce::DocumentWindow& win, juce::Graphics& g,
                                          int w, int h, int titleSpaceX, int titleSpaceW,
                                          const juce::Image* icon,
                                          bool /*drawTitleTextOnLeft*/)
{
    // "Live" for a desktop window is the peer being the active one -- the shell's
    // mouse-over/content-focus test does not apply to a window the OS focuses.
    const bool live = win.isActiveWindow();
    WindowChrome::paintTitleBar (g, juce::Rectangle<int> (0, 0, w, h), live);

    // L26 (QA-Layout): stock-JUCE placement -- icon + title centred as one
    // unit, clamped into the space JUCE reserved between the buttons so a
    // long title cannot run under the close button.  Reverts TS7's
    // left-align + icon drop; the main frame is the only caller that sets
    // an icon.
    const juce::Font font (13.0f);
    g.setFont (font);

    int textW = font.getStringWidth (win.getName());
    int iconW = 0, iconH = 0;
    if (icon != nullptr && icon->isValid())
    {
        iconH = (int) font.getHeight();
        iconW = icon->getWidth() * iconH / juce::jmax (1, icon->getHeight()) + 4;
    }
    textW = juce::jmin (titleSpaceW, textW + iconW);
    int textX = juce::jmax (titleSpaceX, (w - textW) / 2);
    if (textX + textW > titleSpaceX + titleSpaceW)
        textX = titleSpaceX + titleSpaceW - textW;

    if (iconW > 0)
    {
        g.setOpacity (live ? 1.0f : 0.6f);
        g.drawImageWithin (*icon, textX, (h - iconH) / 2, iconW, iconH,
                           juce::RectanglePlacement::centred, false);
        textX += iconW;
        textW -= iconW;
    }

    g.setColour (WindowChrome::titleText());
    g.drawText (win.getName(), textX, 0, textW, h,
                juce::Justification::centredLeft, true);
}

juce::Button* BaySickLAF::createDocumentWindowButton (int buttonType)
{
    // Locked call 5a still holds for the satellites: their DocumentWindow
    // masks request the close button ONLY, so they never ask for these.
    // Buttons exist here for windows that DO ask -- the manuals window
    // requests all three (Jeff, 2026-08-14).
    if (buttonType == juce::DocumentWindow::closeButton)
        return new ChromeCloseButton();
    if (buttonType == juce::DocumentWindow::minimiseButton)
        return new ChromeMinimiseButton();
    if (buttonType == juce::DocumentWindow::maximiseButton)
        return new ChromeMaximiseButton();
    return nullptr;
}

void BaySickLAF::positionDocumentWindowButtons (juce::DocumentWindow&,
                                             int titleBarX, int titleBarY,
                                             int titleBarW, int titleBarH,
                                             juce::Button* minimise, juce::Button* maximise,
                                             juce::Button* close,
                                             bool /*positionTitleBarButtonsOnLeft*/)
{
    // Square, right-aligned, inset by the same 4px the shell's close button
    // uses; maximise and minimise stack leftward of close when present.
    auto slot = [&] (int i)
    {
        return juce::Rectangle<int> (titleBarX + titleBarW - (i + 1) * titleBarH,
                                     titleBarY, titleBarH, titleBarH)
                   .reduced (4);
    };
    int i = 0;
    if (close    != nullptr) close->setBounds (slot (i++));
    if (maximise != nullptr) maximise->setBounds (slot (i++));
    if (minimise != nullptr) minimise->setBounds (slot (i++));
}

void BaySickLAF::drawGroupComponentOutline(juce::Graphics& g, int w, int h,
                                         const juce::String& text,
                                         const juce::Justification& just,
                                         juce::GroupComponent&)
{
    auto bounds = juce::Rectangle<float>(0.5f, 0.5f, (float)w - 1.f, (float)h - 1.f);
    // Main border
    g.setColour(VC::Accent.brighter(0.3f));
    g.drawRoundedRectangle(bounds, 4.f, 1.f);
    // Subtle top inner highlight
    g.setColour(VC::Highlight.withAlpha(0.12f));
    g.drawLine(6.f, 1.5f, (float)w - 6.f, 1.5f, 1.f);
    // Group label
    if (text.isNotEmpty())
    {
        g.setFont(juce::Font(10, juce::Font::bold));
        g.setColour(VC::TextDim);
        int textW = juce::jmin(w - 16, g.getCurrentFont().getStringWidth(text) + 8);
        float tx = just.testFlags(juce::Justification::right)
                     ? (float)(w - textW - 8)
                     : (just.testFlags(juce::Justification::horizontallyCentred)
                         ? (float)((w - textW) / 2)
                         : 8.f);
        g.fillRect(juce::Rectangle<float>(tx - 2.f, 0.f, (float)textW + 4.f, 14.f).withY(0.f));
        g.setColour(VC::Panel);
        g.fillRect(juce::Rectangle<float>(tx, 2.f, (float)textW, 10.f));
        g.setColour(VC::TextDim);
        g.drawText(text, (int)tx, 2, textW, 11, juce::Justification::centred, false);
    }
}

void BaySickLAF::drawComboBox(juce::Graphics& g, int w, int h, bool isDown,
                             int bx, int by, int bw, int bh, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int>(0, 0, w, h).toFloat();
    g.setColour(VC::Accent);
    g.fillRoundedRectangle(bounds, 3.f);
    g.setColour(isDown ? VC::Highlight : VC::Highlight.withAlpha(0.6f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 3.f, 1.f);
    // Arrow indicator
    float arrowX = (float)(bx + bw * 0.5f);
    float arrowY = (float)(by + bh * 0.4f);
    juce::Path arrow;
    arrow.addTriangle(arrowX - 4.f, arrowY, arrowX + 4.f, arrowY, arrowX, arrowY + 5.f);
    g.setColour(VC::Highlight);
    g.fillPath(arrow);
}

// ── Tooltip drawing ───────────────────────────────────────────────────────────
static const juce::Colour kTooltipBg    { 0xec1c1e21 };   // dark glass, 92% opaque
static const juce::Colour kAutoGreen    { 0xff00ff88 };    // neon green for *Automatable*
static constexpr int      kTipPad       = 6;
static constexpr int      kTipFontSize  = 11;
static constexpr int      kAutoFontSize = 10;

void BaySickLAF::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();

    // Dark glass panel
    g.setColour(kTooltipBg);
    g.fillRoundedRectangle(bounds, 4.f);
    g.setColour(juce::Colour(0xff4a5568).withAlpha(0.8f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.f, 1.f);

    // Split text on "*Automatable*" tag
    bool hasAutoTag  = text.contains("\n*Automatable*");
    juce::String tip = hasAutoTag ? text.upToFirstOccurrenceOf("\n*Automatable*", false, false)
                                  : text;

    int textY = kTipPad;

    // Main text
    g.setColour(juce::Colour(0xffe2e8f0));
    g.setFont(juce::Font(kTipFontSize));
    juce::AttributedString as;
    as.setJustification(juce::Justification::topLeft);
    as.append(tip, juce::Font(kTipFontSize), juce::Colour(0xffe2e8f0));
    as.draw(g, juce::Rectangle<float>((float)kTipPad, (float)textY,
                                       (float)(width - kTipPad * 2),
                                       (float)(height - kTipPad * 2)));

    // *Automatable* tag in neon green (bottom line)
    if (hasAutoTag)
    {
        juce::String autoText = "*Automatable*";
        float autoY = (float)(height - kAutoFontSize - kTipPad - 1);

        // Subtle bloom glow
        g.setColour(kAutoGreen.withAlpha(0.15f));
        g.fillRoundedRectangle((float)(kTipPad - 2), autoY - 1.f,
                               (float)(width - kTipPad * 2 + 4), (float)(kAutoFontSize + 4), 3.f);

        // Text
        g.setColour(kAutoGreen);
        g.setFont(juce::Font(kAutoFontSize, juce::Font::italic));
        g.drawText(autoText, kTipPad, (int)autoY, width - kTipPad * 2, kAutoFontSize,
                   juce::Justification::centredLeft, false);
    }
}

juce::Rectangle<int> BaySickLAF::getTooltipBounds(const juce::String& text,
                                                  juce::Point<int> screenPos,
                                                  juce::Rectangle<int> parentArea)
{
    // Parse text to strip *Automatable* tag before measuring
    bool hasAutoTag = text.contains("\n*Automatable*");
    juce::String tip = hasAutoTag
        ? text.upToFirstOccurrenceOf("\n*Automatable*", false, false) : text;

    // Use TextLayout for accurate measurement of multi-line wrapped text.
    // Max width bumped from 300 -> 460 so longer tooltip lines don't truncate.
    const float maxW = 460.f;
    juce::AttributedString as;
    as.setJustification(juce::Justification::topLeft);
    as.append(tip, juce::Font((float) kTipFontSize), juce::Colour(0xffe2e8f0));
    juce::TextLayout layout;
    layout.createLayout(as, maxW);

    int textW = juce::jmax(100, (int) std::ceil(layout.getWidth())  + kTipPad * 2);
    int tipH  = juce::jmax(22,  (int) std::ceil(layout.getHeight()) + kTipPad * 2);
    if (hasAutoTag) tipH += kAutoFontSize + 4;

    // Position just below and right of mouse, clamped inside parent bounds so
    // the full tooltip is visible (JUCE already handles the x/y shift).
    int x = juce::jlimit(parentArea.getX(), parentArea.getRight()  - textW, screenPos.x + 12);
    int y = juce::jlimit(parentArea.getY(), parentArea.getBottom() - tipH,  screenPos.y + 16);
    return { x, y, textW, tipH };
}

// ══════════════════════════════════════════════════════════════════════════════
// LRX-15 - Realism Technique Quick-Reference Matrix
//
// Component         | Tex  | AO   | Aniso| Frsn | Vign | Asym | Topo | Notes
// ------------------|------|------|------|------|------|------|------|------
// Rotary Knobs      |  ✓   |  ✓   | metal| plas |  -   |  ✓   |  ✓   | per-LAF
// Fader Caps        |  -   |  ✓   | chrm | plas |  -   |  ✓   |  -   |
// Fader Tracks      |  -   |  -   |  -   |  -   |  -   |  ✓   |  -   | variable bevel
// VU Meter          |  -   |  -   | bezl |  ✓   |  -   |  ✓   |  ✓   | glass+screws
// DBFS Meter        |  -   |  -   |  -   |  -   |  -   |  ✓   |  -   | LED segments
// Toggle Buttons    |  -   | L1   |  -   |  -   |  -   |  ✓   |  -   | off-white text
// Action Buttons    |  -   |  ✓   |  ✓   |  -   |  -   |  ✓   |  -   |
// Nav Tabs          |  -   | actv |  -   |  -   |  -   |  ✓   |  -   |
// Mixer Surface     |  -   |  -   |  -   |  -   |  ✓   |  ✓   |  -   |
// Effect Panels     |  ✓   |  -   |  -   |  -   |  -   |  ✓   |  ✓   | screws
// Transport Bar     |  ✓   |  -   |  -   |  -   |  -   |  -   |  -   | brushed alum + LCD BPM
// EQ Dots           |  -   | L1   |  -   |  ✓   |  -   |  -   |  -   | color bleed on hover
// Piano Roll Notes  |  -   | L1   |  -   |  -   |  -   |  ✓   |  -   |
// Builder Clips     |  -   | drop |  -   |  -   |  -   |  -   |  -   |
// Scrollbar Thumbs  |  -   | L1   |  ✓   |  ✓   |  -   |  -   |  -   |
// Displays/Screens  |  -   |  -   |  -   |  ✓   |  -   |  -   |  -   | glass overlay
// Plugin Window     |  -   |  -   |  -   |  -   |  ✓   |  -   |  -   | vignette on top
// Combo Boxes       |  -   |  -   | arw  |  -   |  -   |  ✓   |  -   | off-white text
// JewelIndicator    |  -   | L1   |  -   |  ✓   |  -   |  -   |  -   | faceted glass
// Channel Strips    |  -   |  -   |  -   |  -   |  -   |  ✓   |  -   |
// LCD Displays      |  -   |  -   |  -   |  -   |  -   |  -   |  -   | LCD ghost-segment
//
// Abbreviations: Tex=LRX-1 Texture, AO=LRX-2 Shadow Stack, Aniso=LRX-3 Anisotropic,
//   Frsn=LRX-4 Fresnel, Vign=LRX-5 Vignette, Asym=LRX-6 Asymmetry, Topo=LRX-7 Topography
//   L1=contact shadow only, metal=metal knobs, plas=plastic knobs, chrm=chrome caps,
//   bezl=bezel, actv=active state only, arw=arrow button, drop=drop shadow only
// ══════════════════════════════════════════════════════════════════════════════
// LRX - LRXHelper  (realism drawing utilities)
// ══════════════════════════════════════════════════════════════════════════════
void LRXHelper::drawAO(juce::Graphics& g, const juce::Path& shape,
                       bool withReflection, juce::Colour reflectCol)
{
    // Layer 1: contact AO - very tight black ring (ground contact)
    juce::DropShadow contact(juce::Colours::black.withAlpha(0.92f), 2, {0, 1});
    contact.drawForPath(g, shape);
    // Layer 2: drop shadow - medium, standard elevation
    juce::DropShadow drop(juce::Colours::black.withAlpha(0.72f), 6, {2, 3});
    drop.drawForPath(g, shape);
    // Layer 3: reflection uplighting - faint tinted glow from below
    if (withReflection)
    {
        auto col = reflectCol.isTransparent()
                   ? juce::Colour(0xff203050).withAlpha(0.07f)
                   : reflectCol.withAlpha(0.07f);
        juce::DropShadow reflect(col, 14, {0, -4});
        reflect.drawForPath(g, shape);
    }
}

void LRXHelper::drawWithBloom(juce::Graphics& g, const juce::Path& path,
                              juce::Colour col, float width,
                              float bloomMult, float bloomAlpha)
{
    g.setColour(col.withAlpha(bloomAlpha));
    g.strokePath(path, {width * bloomMult, juce::PathStrokeType::curved,
                         juce::PathStrokeType::rounded});
    g.setColour(col);
    g.strokePath(path, {width, juce::PathStrokeType::curved,
                         juce::PathStrokeType::rounded});
}

void LRXHelper::drawFresnelRim(juce::Graphics& g, juce::Rectangle<float> bounds,
                               juce::Colour rimCol, float thickness)
{
    // Fresnel effect: rim is invisible at top (facing light), bright at bottom (grazing)
    float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    float rx = bounds.getWidth() * 0.5f, ry = bounds.getHeight() * 0.5f;
    juce::Path rim;
    rim.addCentredArc(cx, cy, rx, ry, 0.f, 0.f, juce::MathConstants<float>::twoPi, true);
    juce::ColourGradient rimGrad(rimCol.withAlpha(0.0f),  cx, bounds.getY(),
                                  rimCol.withAlpha(0.48f), cx, bounds.getBottom(), false);
    g.setGradientFill(rimGrad);
    g.strokePath(rim, {thickness, juce::PathStrokeType::curved,
                        juce::PathStrokeType::rounded});
}

void LRXHelper::drawAnisotropicHL(juce::Graphics& g, juce::Rectangle<float> capBounds,
                                  float lightAngleDeg)
{
    auto   centre   = capBounds.getCentre();
    float  radius   = capBounds.getWidth() * 0.5f;
    float  lightRad = juce::degreesToRadians(lightAngleDeg);

    // Hot-spot ellipse at the light-source position on the cap
    float hx = centre.x + radius * 0.22f * std::cos(lightRad - juce::MathConstants<float>::halfPi);
    float hy = centre.y + radius * 0.22f * std::sin(lightRad - juce::MathConstants<float>::halfPi);
    float hsR = radius * 0.20f;
    juce::ColourGradient hs(juce::Colours::white.withAlpha(0.60f), hx, hy,
                             juce::Colours::transparentBlack, hx + hsR * 1.3f, hy + hsR, false);
    g.setGradientFill(hs);
    g.fillEllipse(hx - hsR, hy - hsR, hsR * 2.f, hsR * 2.f);

    // Anisotropic band lines - bright at 135° quarter, dim elsewhere
    {
        juce::Graphics::ScopedSaveState ss(g);
        juce::Path clip; clip.addEllipse(capBounds); g.reduceClipRegion(clip);
        for (int i = 0; i < 5; ++i)
        {
            float t      = (float)i / 4.f;
            float lineY  = capBounds.getY() + capBounds.getHeight() * (0.15f + t * 0.70f);
            float bandA  = 0.03f + 0.07f * std::sin(t * juce::MathConstants<float>::pi);
            g.setColour(juce::Colours::white.withAlpha(bandA));
            g.drawHorizontalLine((int)lineY, capBounds.getX(), capBounds.getRight());
        }
    }
}

void LRXHelper::drawMountingScrews(juce::Graphics& g, juce::Rectangle<int> panel,
                                   int inset, juce::Colour col)
{
    constexpr float r = 3.2f;
    juce::Point<float> corners[4] = {
        { (float)(panel.getX()     + inset), (float)(panel.getY()      + inset) },
        { (float)(panel.getRight() - inset), (float)(panel.getY()      + inset) },
        { (float)(panel.getX()     + inset), (float)(panel.getBottom() - inset) },
        { (float)(panel.getRight() - inset), (float)(panel.getBottom() - inset) }
    };
    for (auto& c : corners)
    {
        // Recessed housing
        g.setColour(juce::Colours::black.withAlpha(0.42f));
        g.fillEllipse(c.x - r, c.y - r, r * 2.f, r * 2.f);
        // Screw cap
        g.setColour(col);
        g.fillEllipse(c.x - r * 0.72f, c.y - r * 0.72f, r * 1.44f, r * 1.44f);
        // Phillips cross
        g.setColour(juce::Colours::black.withAlpha(0.52f));
        g.drawLine(c.x - r * 0.42f, c.y, c.x + r * 0.42f, c.y, 0.7f);
        g.drawLine(c.x, c.y - r * 0.42f, c.x, c.y + r * 0.42f, 0.7f);
        // Specular highlight (top-left of cap)
        g.setColour(juce::Colours::white.withAlpha(0.38f));
        g.fillEllipse(c.x - r * 0.38f, c.y - r * 0.50f, r * 0.40f, r * 0.28f);
    }
}

// HOLD-FOR-GL-RENDERER: disabled 2026-04-21 (CPU-renderer banding); re-enable
// plan T3-LRX5Vignette, Future State BLU-370/BLU-489. Call site was
// StandaloneEditor::paintOverChildren.
void LRXHelper::drawVignette(juce::Graphics& g, juce::Rectangle<int> bounds, float strength)
{
    auto bf = bounds.toFloat();
    // Elliptical radial gradient: center transparent → edges dark
    juce::ColourGradient vignette(
        juce::Colours::transparentBlack, bf.getCentreX(), bf.getCentreY(),
        juce::Colours::black.withAlpha(strength), bf.getX(), bf.getY(), true);
    g.setGradientFill(vignette);
    g.fillRect(bf);
}

// ── PageMenuBar ───────────────────────────────────────────────────────────────
namespace
{
    // QA-Layout L31 (Jeff correction 2026-08-03): the strip's menu entry reads
    // like a NATIVE MENU-BAR HEADING -- flat text the way a main window shows
    // "File", with only a hover/press highlight -- not a chrome button widget.
    // The first cut shipped a TextButton with the standard chassis bezel;
    // wrong read of "text button".
    class TitleStripMenuItem : public juce::TextButton
    {
    public:
        using juce::TextButton::TextButton;

        void paintButton (juce::Graphics& g, bool isOver, bool isDown) override
        {
            if (isOver || isDown)
            {
                g.setColour (juce::Colours::white.withAlpha (isDown ? 0.18f : 0.10f));
                g.fillRect (getLocalBounds());
            }
            g.setColour (WindowChrome::titleText());
            g.setFont (juce::Font (13.0f));
            g.drawText (getButtonText(), getLocalBounds(),
                        juce::Justification::centred, true);
        }
    };
}

PageMenuBar::PageMenuBar()
{
    mHamburgerBtn = std::make_unique<TitleStripMenuItem>("Menu");
    mHamburgerBtn->setTooltip("Page menu");
    mHamburgerBtn->onClick = [this] { showHamburgerMenu(); };
    // QA-ManualPress M-4c: one shared strip serves every page window, so the
    // heading anchors the "window menu" callout on each figure that names it.
    mHamburgerBtn->getProperties().set (kDotAnchor, "EQ-15");
    addAndMakeVisible(*mHamburgerBtn);

    // QA-Layout T10 (L13): second flat native-style heading -- the strip
    // reads "Menu  Add".  Hidden until a page installs an Add builder.
    mAddBtn = std::make_unique<TitleStripMenuItem>("Add");
    mAddBtn->setTooltip("Add strips and buses");
    mAddBtn->onClick = [this] { if (mAddMenuBuilder) mAddMenuBuilder (mAddBtn.get()); };
    mAddBtn->getProperties().set (kDotAnchor, "MIX-1");   // M-4c
    addChildComponent(*mAddBtn);
}

void PageMenuBar::setAddMenuBuilder(MenuBuilder builder)
{
    mAddMenuBuilder = std::move(builder);
    if (mAddBtn) mAddBtn->setVisible(mAddMenuBuilder != nullptr);
    resized();
    repaint();
}

void PageMenuBar::setExtraHeadings (const juce::StringArray& labels,
                                    std::function<void(int, juce::Component*)> onOpen)
{
    clearExtraHeadings();
    for (int i = 0; i < labels.size(); ++i)
    {
        auto btn = std::make_unique<TitleStripMenuItem> (labels[i]);
        auto* raw = btn.get();
        btn->onClick = [onOpen, i, raw] { if (onOpen) onOpen (i, raw); };
        if (labels[i] == "View")
            btn->getProperties().set (kDotAnchor, "BSPDL-1");   // M-4c
        addAndMakeVisible (*btn);
        mExtraHeadings.push_back (std::move (btn));
    }
    resized();
    repaint();
}

void PageMenuBar::setViewMenu (const juce::StringArray& modeNames,
                               std::function<int()>     getMode,
                               std::function<void(int)> setMode)
{
    setExtraHeadings ({ "View" },
        [modeNames, getMode, setMode] (int, juce::Component* anchor)
        {
            juce::PopupMenu m;
            const int cur = getMode ? getMode() : 0;
            for (int i = 0; i < modeNames.size(); ++i)
                m.addItem (modeNames[i], true, i == cur,
                           [setMode, i] { if (setMode) setMode (i); });
            if (shots::maybeCapture (m)) return;
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (anchor));
        });
}

void PageMenuBar::clearExtraHeadings()
{
    for (auto& b : mExtraHeadings) removeChildComponent (b.get());
    mExtraHeadings.clear();
    resized();
}

void PageMenuBar::setPageTitle(const juce::String& t)
{
    mTitle = t;
    repaint();
}

void PageMenuBar::setCenterTitle(const juce::String& name, juce::Colour accent)
{
    if (name == mCenterName && accent == mCenterAccent) return;
    mCenterName   = name;
    mCenterAccent = accent;
    repaint();
}

void PageMenuBar::setMenuBuilder(MenuBuilder builder)
{
    mMenuBuilder = std::move(builder);
}

void PageMenuBar::addExtraRightComponent(juce::Component* c, int width)
{
    if (!c) return;
    addAndMakeVisible(*c);
    mExtraRight.push_back({c, width});
    resized();
}

void PageMenuBar::clearExtraRightComponents()
{
    for (auto& e : mExtraRight)
        if (e.comp) removeChildComponent(e.comp);
    mExtraRight.clear();
    resized();
}

void PageMenuBar::setBankIndicator(juce::Component* indicator)
{
    if (indicator == mBankIndicator) return;   // no-op: avoid stack-on-tab-click bug

    if (mBankIndicator)
    {
        removeChildComponent(mBankIndicator);
        mBankIndicator = nullptr;
    }
    if (indicator)
    {
        mBankIndicator = indicator;
        addAndMakeVisible(*mBankIndicator);
    }
    resized();
}

void PageMenuBar::removeExtraRightComponent(juce::Component* c)
{
    if (!c) return;
    auto it = std::find_if(mExtraRight.begin(), mExtraRight.end(),
                           [c](const ExtraComp& e) { return e.comp == c; });
    if (it == mExtraRight.end()) return;
    removeChildComponent(c);
    mExtraRight.erase(it);
    resized();
}

void PageMenuBar::setTabSlots(const juce::StringArray& labels,
                               std::function<void(int)> onTabClick,
                               int activeIdx,
                               juce::Colour accent)
{
    for (auto& b : mTabSlotBtns) removeChildComponent(b.get());
    mTabSlotBtns.clear();

    for (int i = 0; i < labels.size(); ++i)
    {
        auto btn = std::make_unique<juce::TextButton>(labels[i]);
        const int idx = i;
        btn->onClick = [onTabClick, idx] { if (onTabClick) onTabClick(idx); };
        btn->setToggleState(i == activeIdx, juce::dontSendNotification);
        // QA-ManualPress M-4c: the EQ figure's callout 1 names this pair.
        if (i == 0 && labels[i].startsWith ("Pre EQ"))
            btn->getProperties().set (kDotAnchor, "EQ-1");
        if (accent != juce::Colour())
        {
            // 2026-04-26: page sub-tabs (Player / Piano Roll / EQ) keep their
            // chrome body unchanged when active and only gain an outer glow
            // ring in the page accent.  Body stays legible at all times; the
            // ring is the active-state indicator.  See Path C in
            // BaySickLAF::drawButtonBackground for the rendering logic.
            btn->setColour(juce::TextButton::textColourOnId,   accent);
            btn->setColour(juce::TextButton::textColourOffId,  accent.withAlpha(0.55f));
            btn->getProperties().set("outlineGlowOnly", true);
        }
        addAndMakeVisible(*btn);
        mTabSlotBtns.push_back(std::move(btn));
    }
    resized();
}

void PageMenuBar::updateTabActive(int idx)
{
    for (int i = 0; i < (int)mTabSlotBtns.size(); ++i)
        if (mTabSlotBtns[i])
            mTabSlotBtns[i]->setToggleState(i == idx, juce::dontSendNotification);
}

void PageMenuBar::setTabSlotWidth (int px)
{
    const int w = juce::jmax (18, px);
    if (w == mTabSlotW) return;
    mTabSlotW = w;
    resized();
}

void PageMenuBar::setTabSlotTooltip (int idx, const juce::String& tip)
{
    if (idx >= 0 && idx < (int) mTabSlotBtns.size() && mTabSlotBtns[(size_t) idx])
        mTabSlotBtns[(size_t) idx]->setTooltip (tip);
}

void PageMenuBar::clearTabSlots()
{
    for (auto& b : mTabSlotBtns) removeChildComponent(b.get());
    mTabSlotBtns.clear();
    mTabSlotW = 74;   // a narrow override must not leak to the next page
    if (mMidBtn)  { removeChildComponent(mMidBtn.get());  mMidBtn.reset(); }
    if (mSideBtn) { removeChildComponent(mSideBtn.get()); mSideBtn.reset(); }
    mFxRackAction         = nullptr;
    mFreezeToggle         = nullptr;
    mFreezeState          = nullptr;
    mFreezeDisabledReason = nullptr;
    if (mSwingKnob) { removeChildComponent(mSwingKnob.get()); mSwingKnob.reset(); }
    mMidSideVisible = false;
    clearExtraRightComponents();
    resized();
}

void PageMenuBar::setMidSideSlots(std::function<void()> onMid,
                                   std::function<void()> onSide,
                                   bool midActive)
{
    if (!mMidBtn)
    {
        mMidBtn = std::make_unique<juce::TextButton>("MID");
        mMidBtn->setTooltip("View/edit Mid EQ bands");
        addChildComponent(*mMidBtn);
    }
    if (!mSideBtn)
    {
        mSideBtn = std::make_unique<juce::TextButton>("SIDE");
        mSideBtn->setTooltip("View/edit Side EQ bands");
        addChildComponent(*mSideBtn);
    }
    mMidBtn->onClick = [this, onMid] {
        if (onMid) onMid();
        if (mMidBtn)  mMidBtn ->setToggleState(true,  juce::dontSendNotification);
        if (mSideBtn) mSideBtn->setToggleState(false, juce::dontSendNotification);
    };
    mSideBtn->onClick = [this, onSide] {
        if (onSide) onSide();
        if (mMidBtn)  mMidBtn ->setToggleState(false, juce::dontSendNotification);
        if (mSideBtn) mSideBtn->setToggleState(true,  juce::dontSendNotification);
    };
    mMidBtn ->setToggleState( midActive, juce::dontSendNotification);
    mSideBtn->setToggleState(!midActive, juce::dontSendNotification);
    resized();
}

void PageMenuBar::setFxRackSlot(std::function<void()> onClick)
{
    mFxRackAction = std::move (onClick);
}

void PageMenuBar::setFreezeSlot (std::function<int()> getState,
                                 std::function<void(bool)> onToggle,
                                 std::function<juce::String()> getDisabledReason,
                                 bool isVocal)
{
    mFreezeState          = std::move (getState);
    mFreezeDisabledReason = std::move (getDisabledReason);
    mFreezeToggle         = std::move (onToggle);
    mFreezeIsVocal        = isVocal;
}

// The button carried its own state; a menu item is rebuilt from scratch on
// every open, so there is nothing left to refresh.  Kept as a no-op because the
// freeze driver calls it from its state-change broadcast.
void PageMenuBar::refreshFreezeState() {}

TooltipMenuItem::TooltipMenuItem (juce::String text, juce::String tip, bool enabled,
                                  juce::Colour textColour, bool ticked)
    // TRUE: let the menu detect the click and invoke the item.  With false the
    // component has to trigger itself, and the item's action would simply
    // never fire.
    : juce::PopupMenu::CustomComponent (true),
      mText (std::move (text)), mTip (std::move (tip)),
      mEnabled (enabled), mTicked (ticked), mColour (textColour)
{
}

void TooltipMenuItem::getIdealSize (int& w, int& h)
{
    w = juce::Font (14.0f, juce::Font::plain).getStringWidth (mText) + 46;
    h = 22;
}

void TooltipMenuItem::paint (juce::Graphics& g)
{
    if (mEnabled && isItemHighlighted())
    {
        g.setColour (VC::Accent.withAlpha (0.30f));
        g.fillRect (getLocalBounds());
    }
    g.setColour (mEnabled ? mColour : mColour.withAlpha (0.38f));
    if (mTicked)
    {
        // Same glyph geometry LookAndFeel_V4 draws for a ticked stock item, so
        // a custom row and a stock row read as siblings in one menu.
        const auto r = getLocalBounds().removeFromLeft (22).toFloat().reduced (6.0f, 5.0f);
        juce::Path tick;
        tick.startNewSubPath (r.getX(),            r.getCentreY());
        tick.lineTo          (r.getCentreX() - 1,  r.getBottom());
        tick.lineTo          (r.getRight(),        r.getY());
        g.strokePath (tick, juce::PathStrokeType (2.0f));
    }
    g.setFont (juce::Font (14.0f, juce::Font::plain));
    g.drawText (mText, getLocalBounds().withTrimmedLeft (mTicked ? 24 : 12),
                juce::Justification::centredLeft, true);
}

void PageMenuBar::setVisualSlot (std::function<void()> openVisual,
                                 std::function<bool()> available)
{
    mVisualAction    = std::move (openVisual);
    mVisualAvailable = std::move (available);
}

void PageMenuBar::appendStandardItems (juce::PopupMenu& m)
{
    const bool haveFx     = (bool) mFxRackAction;
    const bool haveFreeze = mFreezeState != nullptr && mFreezeToggle != nullptr;
    // Evaluated LIVE on every menu build, not cached: the effect in this slot
    // changes under an open window (swap, preset load, undo), so a bool captured
    // at configure time is the same staleness that made the locked-Freeze entry
    // ignore its own unlock flag.
    const bool haveVisual = (bool) mVisualAction
                            && (! mVisualAvailable || mVisualAvailable());
    if (! haveFx && ! haveFreeze && ! haveVisual) return;

    m.addSeparator();

    if (haveFx)
        m.addItem ("FX Rack", [cb = mFxRackAction] { if (cb) cb(); });

    // QA-Layout T17: opens this effect's Visual window -- a sub-page window like
    // Pedals or NAM/IR, so closing it is a real teardown (the display's strip
    // dies, its watcher releases, and the DSP stops publishing).  This entry is
    // the way BACK once it has been closed, which is why it is here at all.
    //
    // T20 (Jeff, 2026-08-05): PRESENT or ABSENT, never greyed.  See setVisualSlot.
    if (haveVisual)
        m.addItem ("Visual", [cb = mVisualAction] { if (cb) cb(); });

    if (! haveFreeze) return;

    // Freeze SHOWS even when locked, greyed, with the unlock path in its
    // tooltip (Jeff, 2026-08-04).  A capability the user cannot see is a
    // capability they cannot ask for -- same reasoning as the old disabled
    // button, but the lock now has somewhere to explain itself.
    const juce::String reason = mFreezeDisabledReason ? mFreezeDisabledReason()
                                                      : juce::String();
    const bool  enabled = reason.isEmpty();
    const int   s       = mFreezeState();
    const auto  colour  = s == 2 ? juce::Colour (0xffff9100)
                        : s == 1 ? juce::Colour (0xff00fff2)
                                 : juce::Colours::white.withAlpha (0.85f);

    // §6.9 (Jeff, 2026-07-30): on a VOCAL the warning is not optional.  Freeze
    // prints the WHOLE vocal chain -- gate, de-reverb, de-esser, compressor,
    // saturation, limiter, amp -- plus pitch and alignment, because the capture
    // point is below all of it.  A singer who freezes mid-setup and then reaches
    // for the de-esser would find it dead with nothing explaining why, so the
    // tooltip says what it is FOR: getting CPU back once a sound is settled, not
    // something to leave on while dialing one in.
    const juce::String vocalNote = mFreezeIsVocal
        ? juce::String ("\n\nOn a vocal this prints the WHOLE chain - pitch, "
                        "alignment, gate, de-reverb, de-esser, compressor, "
                        "saturation, limiter and amp. None of them can be "
                        "adjusted while frozen.\n\nUse it to get CPU back once a "
                        "sound is settled, not while you are still setting one up.")
        : juce::String();

    const juce::String tip =
        (! enabled ? reason
         : s == 2  ? juce::String ("Frozen, but its content changed - it plays live "
                                   "until the new freeze finishes rendering (at Stop). "
                                   "Click to unfreeze.")
         : s == 1  ? juce::String ("Frozen - this player's audio is a rendered file, "
                                   "so its engine costs no CPU. Click to unfreeze and "
                                   "edit it again.")
                   : juce::String ("Freeze - render this player to a file so its engine "
                                   "stops costing CPU. Its effects, EQ and fader stay "
                                   "live."))
        + vocalNote;

    const juce::String label = s == 0 ? "Freeze" : "Frozen";
    auto item = std::make_unique<TooltipMenuItem> (label, tip, enabled, colour);
    // Constructed FROM the label, not default-constructed: Item's default ctor
    // leaves itemID at 0, which is PopupMenu's "user picked nothing" sentinel
    // and trips the jassert in addItem.  The String ctor sets -1, the same id
    // every action-lambda item carries (and which the r <= 0 guards skip).
    juce::PopupMenu::Item pmi (label);
    pmi.customComponent = item.release();
    pmi.isEnabled       = enabled;
    if (enabled)
        pmi.action = [getState = mFreezeState, cb = mFreezeToggle]
        {
            // Toggle against the LIVE state rather than a cached copy: auto-freeze
            // and the staleness re-render both change it behind this item's back.
            if (cb) cb (getState ? getState() == 0 : true);
        };
    m.addItem (std::move (pmi));
}

namespace
{
    // Smoke round 2: the per-player Swing Mix knob, moved off the engine
    // title bars onto the always-visible PageMenuBar.  Right-click =
    // Truncate Swing Notes toggle (SW-5); double-click = 1.0 (full global);
    // hover/drag shows the value popup (mixer pan/width convention).
    class PageSwingKnob : public juce::Slider
    {
    public:
        PageSwingKnob (std::function<bool()> getTrunc, std::function<void(bool)> setTrunc)
            : mGetTrunc (std::move (getTrunc)), mSetTrunc (std::move (setTrunc))
        {
            setSliderStyle (juce::Slider::RotaryVerticalDrag);
            setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            setRange (0.0, 1.0, 0.0);
            setDoubleClickReturnValue (true, 1.0);
            setTooltip ("Swing Mix - how much of the global Swing this player follows. "
                        "Right-click: Truncate Swing Notes.");
            setPopupDisplayEnabled (true, true, nullptr);
            textFromValueFunction = [] (double v)
            { return "Swing Mix " + juce::String ((int) std::lround (v * 100.0)) + "%"; };
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                const bool on = mGetTrunc ? mGetTrunc() : false;
                juce::PopupMenu m;
                m.addItem (1, "Truncate Swing Notes", true, on);
                // Review fix: the knob is destroyed on every page switch and
                // JUCE does not dismiss a menu whose target died -- a raw
                // `this` capture would dangle on a keyboard-driven switch
                // with the menu open.
                juce::Component::SafePointer<PageSwingKnob> safe (this);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                                 [safe, on] (int r)
                                 {
                                     if (r == 1 && safe != nullptr && safe->mSetTrunc)
                                         safe->mSetTrunc (! on);
                                 });
                return;
            }
            juce::Slider::mouseDown (e);
        }

    private:
        std::function<bool()>     mGetTrunc;
        std::function<void(bool)> mSetTrunc;
    };
}

void PageMenuBar::setSwingKnobSlot (std::function<float()>     getMix,
                                    std::function<void(float)> setMix,
                                    std::function<bool()>      getTruncate,
                                    std::function<void(bool)>  setTruncate)
{
    if (! getMix)
    {
        if (mSwingKnob) { removeChildComponent (mSwingKnob.get()); mSwingKnob.reset(); }
        resized();
        return;
    }
    // Rebuilt per page-show so the truncate callbacks bind THIS page's params.
    if (mSwingKnob) removeChildComponent (mSwingKnob.get());
    auto k = std::make_unique<PageSwingKnob> (std::move (getTruncate), std::move (setTruncate));
    k->setValue ((double) getMix(), juce::dontSendNotification);
    k->onValueChange = [s = k.get(), setMix] { if (setMix) setMix ((float) s->getValue()); };
    // QA-ManualPress M-4c: ONE knob serves every player window, so it
    // anchors the Swing Mix callout on all seven figures that show it.
    k->getProperties().set (kDotAnchor,
        "BSSBT-2;BSP-8;BSHARM-27;BSGTR-9;BSBAS-11;BSRDMAIN-10;BSPLUG-2");
    addAndMakeVisible (*k);
    mSwingKnob = std::move (k);
    resized();
}

void PageMenuBar::setMidSideVisible(bool show)
{
    mMidSideVisible = show;
    if (mMidBtn)  mMidBtn ->setVisible(show);
    if (mSideBtn) mSideBtn->setVisible(show);
}

void PageMenuBar::updateMidSideActive(bool midActive)
{
    if (mMidBtn)  mMidBtn ->setToggleState( midActive, juce::dontSendNotification);
    if (mSideBtn) mSideBtn->setToggleState(!midActive, juce::dontSendNotification);
}

void PageMenuBar::showHamburgerMenu()
{
    if (mMenuBuilder)
        mMenuBuilder (mHamburgerBtn.get());
}

void PageMenuBar::paint(juce::Graphics& g)
{
    // Dark strip background
    g.setColour(VC::Surface.darker(0.1f));
    g.fillRect(getLocalBounds());

    // Bottom separator line
    g.setColour(VC::Accent.withAlpha(0.55f));
    g.drawHorizontalLine(getHeight() - 1, 0.f, (float)getWidth());

    // Page title.  Jeff's rule (2026-08-04): a window carrying its own logo /
    // engine wordmark shows NO plain title text -- the logo IS the identity.
    // A window without one centres the title instead of pinning it left.
    //
    // Centred in the FREE SPAN, not on the strip -- the same fix the coloured
    // engine name below got on 2026-08-05, which this path never received.  On
    // the strip it slid under the Menu button the moment a window got narrow,
    // and the 180px VU window made that visible as "MenuVU Meter".
    if (mTitle.isNotEmpty() && mTabSlotBtns.empty() && mCenterName.isEmpty())
    {
        g.setColour(VC::TextDim.withAlpha(0.7f));
        g.setFont(juce::Font(10.f, juce::Font::bold));

        const int freeL = juce::jmin (mCenterFreeL, mCenterFreeR);
        const int freeW = juce::jmax (0, mCenterFreeR - freeL);
        g.drawText(mTitle,
                   freeW > 0 ? juce::Rectangle<int> (freeL, 0, freeW, getHeight())
                             : getLocalBounds(),
                   juce::Justification::centred, false);
    }

    // QA-Layout T3 (Window-4/L2): centered colored engine name, bloom style.
    // paintEngineName anchors LEFT within its rect, so center by sizing the
    // rect to the text.
    if (mCenterName.isNotEmpty())
    {
        // Jeff, 2026-08-05: centre the name in the FREE SPAN between the left
        // cluster and the right extras, not on the whole strip.  Centring on
        // the strip put it underneath whatever sat on the left the moment a
        // window got narrow -- visible first on the pedals Compact view, where
        // the NAM/IR button covered it, but latent on every narrow strip.
        const juce::Font f (15.0f, juce::Font::bold);
        const int tw   = f.getStringWidth (mCenterName) + 8;
        const int freeL = juce::jmin (mCenterFreeL, mCenterFreeR);
        const int freeW = juce::jmax (0, mCenterFreeR - freeL);
        auto span = freeW > 0 ? juce::Rectangle<int> (freeL, 0, freeW, getHeight())
                              : getLocalBounds();
        BaySickTitleBar::paintEngineName (g, mCenterName, mCenterAccent,
                                          span.withSizeKeepingCentre (juce::jmin (tw, span.getWidth()),
                                                                      getHeight()),
                                          true, 15.0f);
    }

}

void PageMenuBar::resized()
{
    auto b = getLocalBounds().reduced(2, 2);

    // "Menu" button on left (L31)
    mHamburgerBtn->setBounds(b.removeFromLeft(kMenuBtnW).reduced(0, 1));
    b.removeFromLeft(2);

    // "Add" heading right of Menu (T10/L13) -- only takes width when a page
    // installed an Add builder.
    if (mAddBtn != nullptr && mAddBtn->isVisible())
    {
        mAddBtn->setBounds(b.removeFromLeft(kAddBtnW).reduced(0, 1));
        b.removeFromLeft(2);
    }

    // T16: page-supplied headings (Builder's Edit / View), sized to their text
    // so a long label is not clipped by a fixed slot width.
    for (auto& h : mExtraHeadings)
    {
        const int w = juce::jmax (kAddBtnW,
                                  juce::Font (13.0f).getStringWidth (h->getButtonText()) + 18);
        h->setBounds(b.removeFromLeft(w).reduced(0, 1));
        b.removeFromLeft(2);
    }

    // Swing Mix knob sits immediately right of the Menu heading (Jeff,
    // 2026-08-04) -- it is the one always-live CONTROL on the strip, so it
    // gets the fixed leftmost spot rather than drifting with whatever nav
    // buttons a page happens to mount.
    if (mSwingKnob)
    {
        mSwingKnob->setBounds(b.removeFromLeft(24).reduced(1, 1));
        b.removeFromLeft(4);
    }

    // Tab slot buttons right after hamburger
    for (auto& btn : mTabSlotBtns)
        btn->setBounds(b.removeFromLeft(mTabSlotW).reduced(2, 1));

    // MID/SIDE after tab slots (always positioned; visibility managed separately)
    if (mMidBtn || mSideBtn)
    {
        b.removeFromLeft(4);
        if (mMidBtn)  mMidBtn ->setBounds(b.removeFromLeft(40).reduced(1, 1));
        if (mSideBtn) mSideBtn->setBounds(b.removeFromLeft(46).reduced(1, 1));
    }

    // 2026-04-19: bank indicator pill, immediately to the right of MID/SIDE.
    // Single dedicated slot (not in the right-extras cluster) so it never
    // duplicates on repeated tab clicks and never drifts under other extras.
    if (mBankIndicator)
    {
        b.removeFromLeft(6);
        mBankIndicator->setBounds(b.removeFromLeft(56).reduced(1, 1));
    }

    // FX Rack and Freeze are MENU ITEMS now (Jeff, 2026-08-04), not buttons --
    // see appendStandardItems.  No strip geometry for either.

    // Everything above is the LEFT cluster; whatever is still in `b` when the
    // right-hand extras have been taken is the free span the centered engine
    // name may use.  Captured for paint() -- see mCenterFreeL/R.
    mCenterFreeL = b.getX();

    // Extra right components (Kit, Nav combo, etc.) flush to right.  A dead
    // SafePointer (editor-owned component destroyed on engine swap) is
    // skipped without consuming width.
    for (auto it = mExtraRight.rbegin(); it != mExtraRight.rend(); ++it)
        if (it->comp != nullptr)
            it->comp->setBounds(b.removeFromRight(it->width).reduced(2, 1));

    mCenterFreeR = b.getRight();
}

// ============================================================ VKnobAutomation
namespace VKnobAutomation
{
    std::function<void(const juce::String& paramId)> sOnAutomate;
    std::function<void(const juce::String& paramId,
                       std::function<void(float)>)> sOnRegisterApplicator;
    std::function<void(const juce::String& paramId,
                       std::function<float()>)> sOnRegisterReader;
    std::function<void(const juce::String& slotUuid)>                            sOnUnregisterSlotUuid;
    std::function<juce::String(const juce::String& paramId)>                     sResolveMenuLabel;
    std::function<bool(const juce::String& paramId)>                             sShouldOfferModulate;
    std::function<void(const juce::String& paramId)>                             sOnModulateEnvelope;

    // I-3c (2026-05-02): MIDI Learn callbacks.  Wired by StandaloneEditor.
    std::function<bool(const juce::String& paramId)>                             sIsMidiMapped;
    std::function<bool(const juce::String& paramId)>                             sIsMidiLearningTarget;
    std::function<juce::String(const juce::String& paramId)>                     sDescribeMidiMapping;
    std::function<void(const juce::String& paramId)>                             sOnMidiLearn;
    std::function<void(const juce::String& paramId)>                             sOnMidiForget;

    int appendMidiLearnMenuItems (juce::PopupMenu& m, const juce::String& paramId, int firstId)
    {
        // Skip the section entirely on plugin builds where StandaloneEditor
        // hasn't wired any of the MIDI Learn callbacks.
        if (! sOnMidiLearn) return firstId - 1;
        if (paramId.isEmpty()) return firstId - 1;

        m.addSeparator();

        // "MIDI Learn" -- greyed out when no MIDI input devices are detected
        // (locked spec call 2026-05-02: hot-plug not supported, user must
        // plug a device + reopen the menu).  Re-learn replaces an existing
        // mapping when devices ARE present.
        const bool hasDevices = ! juce::MidiInput::getAvailableDevices().isEmpty();
        const juce::String learnLabel = hasDevices
            ? juce::String ("MIDI Learn")
            : juce::String ("MIDI Learn (no MIDI input devices)");
        m.addItem (firstId, learnLabel, /*enabled*/ hasDevices);

        // "MIDI Forget: <summary>" -- only if there's an existing mapping.
        const bool isMapped = sIsMidiMapped && sIsMidiMapped (paramId);
        if (isMapped)
        {
            juce::String summary;
            if (sDescribeMidiMapping) summary = sDescribeMidiMapping (paramId);
            const juce::String label = summary.isNotEmpty()
                ? juce::String ("MIDI Forget: ") + summary
                : juce::String ("MIDI Forget");
            m.addItem (firstId + 1, label);
        }

        return firstId + 1;
    }

    bool handleMidiLearnMenuResult (int result, int firstId, const juce::String& paramId)
    {
        if (result == firstId)
        {
            if (sOnMidiLearn) sOnMidiLearn (paramId);
            return true;
        }
        if (result == firstId + 1)
        {
            if (sOnMidiForget) sOnMidiForget (paramId);
            return true;
        }
        return false;
    }

    void promptSliderValueEntry(juce::Slider& slider, const juce::String& displayId)
    {
        // Pre-fill with the slider's current display text (matches drag popup).
        // juce::Slider::getTextFromValue returns the same string the popup shows.
        const juce::String currentText = slider.getTextFromValue(slider.getValue());

        // Run the paramId through the display-name resolver so the dialog title
        // matches the right-click menu label ("Channel - Effect - Param" / user
        // rename). Falls back to the raw paramId, then to "Value".
        juce::String title;
        if (sResolveMenuLabel) title = sResolveMenuLabel(displayId);
        if (title.isEmpty())   title = displayId;
        if (title.isEmpty())   title = "Value";

        // Render the range endpoints through the slider's own formatter so units
        // match the drag popup (e.g., "1.0 ms - 2000.0 ms" on Delay Time).
        const juce::String minText = slider.getTextFromValue(slider.getMinimum());
        const juce::String maxText = slider.getTextFromValue(slider.getMaximum());
        const juce::String prompt  = "Enter a new value:\nRange: " + minText + " - " + maxText;

        auto* window = new juce::AlertWindow("Set " + title,
                                             prompt,
                                             juce::AlertWindow::NoIcon);
        window->addTextEditor("value", currentText, {}, false);
        window->addButton("OK",     1, juce::KeyPress(juce::KeyPress::returnKey));
        window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        juce::Component::SafePointer<juce::Slider> safeSlider(&slider);

        window->enterModalState(true, juce::ModalCallbackFunction::create(
            [window, safeSlider](int result)
            {
                if (result == 1)
                {
                    if (auto* s = safeSlider.getComponent())
                    {
                        const juce::String typed = window->getTextEditorContents("value").trim();
                        if (typed.isNotEmpty())
                        {
                            // getValueFromText parses the string using the slider's
                            // own inverse-unit logic (e.g. "500 ms" -> 500.0).
                            // setValue auto-clamps to the slider's range and
                            // triggers onValueChange so the DSP picks it up.
                            const double parsed = s->getValueFromText(typed);
                            s->setValue(parsed, juce::sendNotificationSync);
                        }
                    }
                }
                delete window;
            }), false);
    }
}

// ============================================================ VKnob
VKnob::VKnob(const juce::String& lbl,float def,const juce::String& tip)
{
    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
    slider.setPopupDisplayEnabled(true,true,nullptr);
    slider.setDoubleClickReturnValue(true,(double)def);
    slider.setMouseClickGrabsKeyboardFocus(false);
    slider.setTooltip(tip);
    slider.addListener(this);
    slider.addMouseListener(this, false);
    slider.getProperties().set("vknob_slider", true);  // GlobalAutoRightClick skips this
    addAndMakeVisible(slider);
    label.setText(lbl,juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font(10));
    label.setColour(juce::Label::textColourId,VC::TextDim);
    // Mirror the slider's tooltip onto the label so hovering the knob's name
    // shows the same help text as hovering the rotary itself.
    label.setTooltip(tip);
    addAndMakeVisible(label);

    // Lockout overlay sits on top of the slider when setLocked(true) is called.
    // Starts hidden; setLocked() controls visibility + z-order.
    mStoredTooltip = tip;
    mLockoutOverlay.tip = &mStoredTooltip;
    addChildComponent(mLockoutOverlay);
}
VKnob::~VKnob()
{
    slider.removeMouseListener(this);
    slider.removeListener(this);
}
void VKnob::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        // Use explicit paramId if set, otherwise fall back to the label text
        juce::String id = paramId.isNotEmpty() ? paramId : label.getText();
        if (id.isEmpty()) return;

        // Capture slider via SafePointer so the callback is safe if the VKnob
        // is destroyed (e.g., rack rebuild) while the menu is open.
        juce::Component::SafePointer<juce::Slider> safeSlider(&slider);

        // Resolve paramId -> friendly label for the menu text only; the backend
        // callback still receives the stable `id`.
        juce::String menuLabel;
        if (VKnobAutomation::sResolveMenuLabel)
            menuLabel = VKnobAutomation::sResolveMenuLabel(id);
        if (menuLabel.isEmpty()) menuLabel = id;

        juce::PopupMenu m;
        m.addItem(1, "Automate: " + menuLabel);
        m.addItem(2, "Type in value...");

        // I-3c (2026-05-02): MIDI Learn / Forget / Save-as-default.  IDs
        // start at 100 to leave room for future automation items (1-99).
        constexpr int kMidiFirstId = 100;
        const int midiHighest = VKnobAutomation::appendMidiLearnMenuItems (m, id, kMidiFirstId);
        juce::ignoreUnused (midiHighest);

        m.showMenuAsync(juce::PopupMenu::Options{}, [id, safeSlider](int result)
        {
            if (result == 1 && VKnobAutomation::sOnAutomate)
                VKnobAutomation::sOnAutomate(id);
            else if (result == 2)
            {
                if (auto* s = safeSlider.getComponent())
                    VKnobAutomation::promptSliderValueEntry(*s, id);
            }
            else if (VKnobAutomation::handleMidiLearnMenuResult (result, kMidiFirstId, id))
            {
                // Handled by the MIDI Learn dispatcher.
            }
        });
    }
}
void VKnob::paintOverChildren(juce::Graphics&)
{
    // I-3c (2026-05-02): The dashed-yellow learn outline is now drawn by a
    // generic MidiLearnOutlineOverlay parented on top of the active learn
    // target, so it works for plain juce::Slider controls (mixer faders) too,
    // not just VKnobs.  See MidiLearnUI::beginLearn.  Method retained as a
    // no-op so the override declared in the header still has a definition.
}

void VKnob::sliderDragStarted(juce::Slider* s)
{
    mValueBeforeDrag = (float)s->getValue();
}
void VKnob::sliderDragEnded(juce::Slider* s)
{
    if (onDragEnded) onDragEnded(mValueBeforeDrag, (float)s->getValue());
}
void VKnob::resized()
{
    auto b=getLocalBounds();
    label.setBounds(b.removeFromBottom(14));
    slider.setBounds(b);
    // Overlay sits exactly over the slider (rotary area only; label is
    // outside, so its tooltip still works independently).
    mLockoutOverlay.setBounds(slider.getBounds());
}
void VKnob::setLocked(bool lock)
{
    if (mLocked == lock) return;
    mLocked = lock;
    // Grey visual while keeping hover tooltip reachable. Overlay sits above
    // the slider, silently swallows clicks/drags, and exposes the knob tooltip
    // via its own TooltipClient so hover still shows the help text.
    setAlpha(mLocked ? 0.45f : 1.0f);
    mLockoutOverlay.setVisible(mLocked);
    if (mLocked) mLockoutOverlay.toFront(false);
}

// ============================================================ ChickenHeadSelector
// Discrete N-option rotary selector; renders via shared Chicken Head filmstrip.

ChickenHeadSelector::ChickenHeadSelector()
{
    setWantsKeyboardFocus(false);
}

void ChickenHeadSelector::setOptions(const std::vector<Option>& opts)
{
    mOptions = opts;
    // QA-EffectsReview Task 4: raised 10 -> 12 for the BOSS SY-1's 11 TYPE knob.
    // The 10-frame chicken-head asset is just the knob's rotation graphic (drawn
    // from a continuous t = idx/(n-1)), so it does NOT limit the option count;
    // the bezel-letter layout (angleForIndex) already scales to any N.
    if ((int) mOptions.size() > 12) mOptions.resize(12);
    mSelectedIdx = juce::jlimit(0, juce::jmax(0, (int) mOptions.size() - 1), mSelectedIdx);
    repaint();
}

void ChickenHeadSelector::setSelectedIndex(int idx, juce::NotificationType notify)
{
    if (mOptions.empty()) { mSelectedIdx = 0; return; }
    idx = juce::jlimit(0, (int) mOptions.size() - 1, idx);
    if (idx != mSelectedIdx)
    {
        mSelectedIdx = idx;
        repaint();
        if (notify != juce::dontSendNotification && onChange) onChange(mSelectedIdx);
    }
}

juce::Rectangle<float> ChickenHeadSelector::getKnobBounds() const noexcept
{
    // Knob fills the central square; letters arc around outside this.
    auto b = getLocalBounds().toFloat().reduced(2.0f);
    const float letterPad = 13.0f;  // reserved radius for letter marks (QA-EffectsReview Task 4: 10->13 so denser/longer label rings like the SY-1's 11 TYPEs don't overhang the bezel)
    float side = juce::jmin(b.getWidth(), b.getHeight()) - 2.0f * letterPad;
    side = juce::jmax(16.0f, side);
    return juce::Rectangle<float>(0, 0, side, side).withCentre(b.getCentre());
}

float ChickenHeadSelector::angleForIndex(int idx) const noexcept
{
    const int n = (int) mOptions.size();
    if (n <= 1) return 0.0f;
    const float t = (float) idx / (float) (n - 1);
    const float deg = kStartAngleDeg + t * (kEndAngleDeg - kStartAngleDeg);
    return deg * juce::MathConstants<float>::pi / 180.0f;
}

juce::Point<float> ChickenHeadSelector::letterCentre(int idx) const noexcept
{
    const auto kb = getKnobBounds();
    const float r = kb.getWidth() * 0.5f + 8.0f;
    const float ang = angleForIndex(idx);
    // ang = 0 → up. Screen y grows down, so use -cos for "up".
    return { kb.getCentreX() + r * std::sin(ang),
             kb.getCentreY() - r * std::cos(ang) };
}

int ChickenHeadSelector::hitLetter(juce::Point<float> p) const noexcept
{
    constexpr float kHitRadius = 8.0f;
    for (int i = 0; i < (int) mOptions.size(); ++i)
    {
        if (letterCentre(i).getDistanceFrom(p) <= kHitRadius)
            return i;
    }
    return -1;
}

int ChickenHeadSelector::indexFromAngle(float angleRad) const noexcept
{
    const int n = (int) mOptions.size();
    if (n <= 1) return 0;
    // Normalise to [-π, π], then scale into [kStart, kEnd] range.
    float deg = angleRad * 180.0f / juce::MathConstants<float>::pi;
    deg = juce::jlimit(kStartAngleDeg, kEndAngleDeg, deg);
    const float t = (deg - kStartAngleDeg) / (kEndAngleDeg - kStartAngleDeg);
    return juce::jlimit(0, n - 1, (int) std::round(t * (n - 1)));
}

void ChickenHeadSelector::paint(juce::Graphics& g)
{
    if (mOptions.empty()) return;

    const auto kb = getKnobBounds();

    // Draw the filmstrip frame corresponding to the selected option.
    const int n = (int) mOptions.size();
    const float t = (n <= 1) ? 0.0f : (float) mSelectedIdx / (float) (n - 1);
    const auto& strip = Filmstrips::chickenHead();
    if (strip.isValid())
    {
        Filmstrips::drawFrame(g, strip, 66, 66, 10, t, kb);
    }
    else
    {
        // Fallback: circle + beak pointer so the widget still works if the asset is missing.
        g.setColour(juce::Colour(0xff333333));
        g.fillEllipse(kb);
        g.setColour(juce::Colour(0xffcccccc));
        const float ang = angleForIndex(mSelectedIdx);
        const float r   = kb.getWidth() * 0.45f;
        juce::Line<float> beak(kb.getCentre(),
                               { kb.getCentreX() + r * std::sin(ang),
                                 kb.getCentreY() - r * std::cos(ang) });
        g.drawLine(beak, 2.0f);
    }

    // Letter marks around the bezel.
    // Selected  → bright red  (#E03A3A) regardless of panel.
    // Hovered   → duller red  (#B05D5D) - preview of what'd be selected.
    // Default   → mDefaultTextColour (white for dark panels, black for dynamics/cream).
    constexpr juce::uint32 kSelectedRed = 0xffE03A3A;
    constexpr juce::uint32 kHoverRed    = 0xffB05D5D;
    g.setFont(juce::Font(8.5f, juce::Font::bold));
    for (int i = 0; i < n; ++i)
    {
        const auto c = letterCentre(i);
        const bool selected = (i == mSelectedIdx);
        const bool hovered  = (i == mHoverLetter);
        juce::Colour col;
        if (selected)      col = juce::Colour(kSelectedRed);
        else if (hovered)  col = juce::Colour(kHoverRed);
        else               col = mDefaultTextColour;
        g.setColour(col);
        juce::Rectangle<float> r(c.x - 9.0f, c.y - 6.0f, 18.0f, 12.0f);
        const juce::String txt = mOptions[i].letter.isNotEmpty()
            ? mOptions[i].letter
            : mOptions[i].label.substring(0, 1).toUpperCase();
        g.drawText(txt, r, juce::Justification::centred, false);
    }
}

void ChickenHeadSelector::resized() {}

void ChickenHeadSelector::mouseDown(const juce::MouseEvent& e)
{
    if (mLocked || mOptions.empty()) return;  // locked state swallows clicks silently

    // 2026-04-19: right-click opens a popup menu listing all options (current
    // one checkmarked, click to pick directly). When the selector has a
    // componentID (paramId), an "Automate: ..." item shows at the top.
    // Left-click falls through to the normal hit-letter / rotary-drag logic.
    if (e.mods.isRightButtonDown())
    {
        juce::PopupMenu menu;
        const juce::String paramId = getComponentID();
        const bool hasParamId = paramId.isNotEmpty();

        if (hasParamId)
        {
            juce::String menuLabel = paramId;
            if (VKnobAutomation::sResolveMenuLabel)
            {
                auto resolved = VKnobAutomation::sResolveMenuLabel(paramId);
                if (resolved.isNotEmpty()) menuLabel = resolved;
            }
            menu.addItem(1000, "Automate: " + menuLabel);
            menu.addSeparator();
        }

        // List all options; item IDs start at 1 so 0 (menu dismissed) is
        // distinguishable. Current selection gets a check mark.
        for (int i = 0; i < (int) mOptions.size(); ++i)
        {
            const auto& opt = mOptions[i];
            const juce::String display = opt.label.isNotEmpty() ? opt.label
                                                                 : juce::String(i + 1);
            menu.addItem(i + 1, display, true /*enabled*/, i == mSelectedIdx);
        }

        juce::Component::SafePointer<ChickenHeadSelector> safeSelf(this);
        const juce::String id = paramId;
        menu.showMenuAsync(juce::PopupMenu::Options{},
                           [safeSelf, id](int result)
        {
            if (result == 0) return;   // menu dismissed without selection
            if (result == 1000)
            {
                if (VKnobAutomation::sOnAutomate && id.isNotEmpty())
                    VKnobAutomation::sOnAutomate(id);
                return;
            }
            if (safeSelf)
                safeSelf->setSelectedIndex(result - 1, juce::sendNotification);
        });
        return;
    }

    const auto p = e.position;
    const int hit = hitLetter(p);
    if (hit >= 0)
    {
        setSelectedIndex(hit, juce::sendNotification);
        mIsDragging = false;
        return;
    }
    // Start rotary drag.
    mIsDragging     = true;
}

void ChickenHeadSelector::mouseDrag(const juce::MouseEvent& e)
{
    if (mLocked || !mIsDragging || mOptions.empty()) return;
    if (e.mods.isRightButtonDown()) return;   // 2026-04-19: right-click drag ignored
    const auto c = getKnobBounds().getCentre();
    const float ang = std::atan2(e.position.x - c.x, c.y - e.position.y);
    setSelectedIndex(indexFromAngle(ang), juce::sendNotification);
}

void ChickenHeadSelector::setLocked(bool lock)
{
    if (mLocked == lock) return;
    mLocked = lock;
    // Grey visual but KEEP setInterceptsMouseClicks(true) so TooltipWindow
    // still queries getTooltip() on hover. mouseDown/Drag short-circuit above.
    setAlpha(mLocked ? 0.45f : 1.0f);
    setMouseCursor(mLocked ? juce::MouseCursor::NormalCursor
                           : juce::MouseCursor::PointingHandCursor);
    repaint();
}

void ChickenHeadSelector::mouseMove(const juce::MouseEvent& e)
{
    const int hit = hitLetter(e.position);
    if (hit != mHoverLetter)
    {
        mHoverLetter = hit;
        repaint();
    }
}

void ChickenHeadSelector::mouseExit(const juce::MouseEvent&)
{
    if (mHoverLetter != -1) { mHoverLetter = -1; repaint(); }
}

juce::String ChickenHeadSelector::getTooltip()
{
    if (mHoverLetter >= 0 && mHoverLetter < (int) mOptions.size())
    {
        const auto& opt = mOptions[mHoverLetter];
        return opt.label.isNotEmpty() && opt.tooltip.isNotEmpty()
            ? opt.label + ": " + opt.tooltip
            : (opt.tooltip.isNotEmpty() ? opt.tooltip : opt.label);
    }
    return mBodyTooltip;
}

// ============================================================ DualLabelToggle
// Composite widget: 2 or 3 labels + physical switch. See header for layout.

DualLabelToggle::DualLabelToggle()
{
    // Force BaySickLAF on the switch so it uses the shared switch_toggle filmstrip
    // regardless of the containing panel's LAF.
    mBtn.setLookAndFeel(&BaySickLAF::get());
    mBtn.getProperties().set("switchToggle", true);   // intentional switch - opt in to filmstrip

    auto setupLbl = [this](juce::Label& l, float ptSize, bool bold)
    {
        l.setJustificationType(juce::Justification::centred);
        l.setFont(juce::Font(ptSize, bold ? juce::Font::bold : juce::Font::plain));
        l.setColour(juce::Label::textColourId, juce::Colour(0xff2b241a));
        l.setInterceptsMouseClicks(true, false);  // hover for tooltip, click routed via mouseUp below
        l.addMouseListener(this, false);           // forwards clicks to DualLabelToggle::mouseUp
    };
    setupLbl(mTop,    9.0f, true);
    setupLbl(mMidTop, 8.0f, false);
    setupLbl(mMidBot, 8.0f, false);
    setupLbl(mBot,    9.0f, true);

    // All labels start hidden until setupNamed/OnOff is called.
    addChildComponent(mTop);
    addChildComponent(mMidTop);
    addChildComponent(mMidBot);
    addChildComponent(mBot);
    addAndMakeVisible(mBtn);
}

DualLabelToggle::~DualLabelToggle()
{
    mTop   .removeMouseListener(this);
    mMidTop.removeMouseListener(this);
    mMidBot.removeMouseListener(this);
    mBot   .removeMouseListener(this);
    mBtn.setLookAndFeel(nullptr);
}

void DualLabelToggle::setupNamed(const juce::String& topLabel,    const juce::String& topTooltip,
                                 const juce::String& bottomLabel, const juce::String& bottomTooltip)
{
    mMode = Mode::Named;

    mTop.setText(topLabel,    juce::dontSendNotification);
    mBot.setText(bottomLabel, juce::dontSendNotification);
    mTop.setTooltip(topTooltip);
    mBot.setTooltip(bottomTooltip);

    mTop   .setVisible(true);
    mBot   .setVisible(true);
    mMidTop.setVisible(false);
    mMidBot.setVisible(false);
    resized();
}

void DualLabelToggle::setupOnOff(const juce::String& featureName, const juce::String& featureTooltip)
{
    mMode = Mode::OnOff;

    mTop   .setText(featureName, juce::dontSendNotification);
    mMidTop.setText("OFF",        juce::dontSendNotification);
    mMidBot.setText("ON",         juce::dontSendNotification);

    mTop   .setTooltip(featureTooltip);
    mMidTop.setTooltip({});
    mMidBot.setTooltip({});

    mTop   .setVisible(true);
    mMidTop.setVisible(true);
    mMidBot.setVisible(true);
    mBot   .setVisible(false);
    resized();
}

void DualLabelToggle::mouseUp(const juce::MouseEvent& e)
{
    // Right-click is reserved for the Automate menu (see class comment).
    if (e.mods.isPopupMenu()) return;
    if (e.mouseWasDraggedSinceMouseDown()) return;
    // Dispatch based on which label forwarded the event.
    if (mMode == Mode::Named)
    {
        if (e.eventComponent == &mTop) mBtn.setToggleState(false, juce::sendNotification);
        else if (e.eventComponent == &mBot) mBtn.setToggleState(true, juce::sendNotification);
    }
    else if (mMode == Mode::OnOff)
    {
        if (e.eventComponent == &mMidTop) mBtn.setToggleState(false, juce::sendNotification);
        else if (e.eventComponent == &mMidBot) mBtn.setToggleState(true, juce::sendNotification);
    }
}

void DualLabelToggle::setLabelColour(juce::Colour c)
{
    mTop   .setColour(juce::Label::textColourId, c);
    mMidTop.setColour(juce::Label::textColourId, c);
    mMidBot.setColour(juce::Label::textColourId, c);
    mBot   .setColour(juce::Label::textColourId, c);
}

void DualLabelToggle::resized()
{
    auto b = getLocalBounds();
    const int sw = juce::jmin(b.getWidth() - 4, 44);
    const int sh = 22;

    if (mMode == Mode::Named)
    {
        // Layout: [topLbl 12] [switch 22] [botLbl 12]
        mTop.setBounds(b.removeFromTop(12));
        b.removeFromTop(1);
        auto switchArea = b.removeFromTop(sh);
        mBtn.setBounds(switchArea.withSizeKeepingCentre(sw, sh));
        b.removeFromTop(1);
        mBot.setBounds(b.removeFromTop(12));
    }
    else if (mMode == Mode::OnOff)
    {
        // Layout: [featureName 12] [OFF 10] [switch 22] [ON 10]
        mTop   .setBounds(b.removeFromTop(12));
        mMidTop.setBounds(b.removeFromTop(10));
        auto switchArea = b.removeFromTop(sh);
        mBtn.setBounds(switchArea.withSizeKeepingCentre(sw, sh));
        mMidBot.setBounds(b.removeFromTop(10));
    }
    else
    {
        // Unset - still give the switch sensible bounds so it renders.
        mBtn.setBounds(b.withSizeKeepingCentre(sw, sh));
    }
}

// QA-EqPro T6: the ParametricEQDisplay implementation (about 3,000
// lines) is deleted with its class - see SharedUI.h.

// ============================================================ VUMeter
float VUMeter::sCalibrationDb = -18.f;
std::function<void()> VUMeter::sOnCalibrationChanged;

void VUMeter::addCalibrationSubMenu (juce::PopupMenu& parent)
{
    juce::PopupMenu calib;
    for (int db = -18; db <= -14; ++db)
    {
        const bool isCurrent = (getCalibrationDb() == (float) db);
        calib.addItem (juce::String (db) + " dBFS", true, isCurrent,
                       [db] { setCalibrationDb ((float) db); });
    }
    parent.addSubMenu ("VU Calibration (0 VU = ...)", calib);
}

VUMeter::VUMeter(Style style) : mStyle(style)
{
    // 2026-05-02: vblank attachment created lazily in parentHierarchyChanged
    // once the component has a peer.  Old 60 Hz Timer dropped.
}

void VUMeter::setLevel(float rms01)
{
    float clamped = juce::jlimit(0.f, 1.f, rms01);

    // 2026-05-02: CAS-max so multiple per-frame pushes from the audio path
    // capture running max instead of overwriting the last write.  UI vblank
    // exchange-and-resets to start a fresh window each frame.
    auto casMax = [] (std::atomic<float>& a, float v) noexcept
    {
        float cur = a.load (std::memory_order_relaxed);
        while (cur < v && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed))
        {}
    };

    if (mStyle == Horizontal)
    {
        casMax (mLevel, clamped);
        casMax (mPeak,  clamped);
    }
    else
    {
        // Vertical hardware VU path -- vblank handler drives spring-damper
        // ballistics off mLevelRms01.
        casMax (mLevelRms01, clamped);
    }
}

void VUMeter::parentHierarchyChanged()
{
    if (getPeer() != nullptr && mVBlank == nullptr)
    {
        mVBlank = std::make_unique<juce::VBlankAttachment> (
            this, [this] { onVBlank(); });
    }
    else if (getPeer() == nullptr && mVBlank != nullptr)
    {
        mVBlank.reset();
    }
}

void VUMeter::setGainReduction(float grDb)
{
    mGR.store(juce::jlimit(-40.f, 0.f, grDb), std::memory_order_relaxed);
    // repaint() not needed here - timerCallback() repaints on each tick
}

void VUMeter::onVBlank()
{
    if (mStyle == Horizontal)
    {
        // 2026-05-02: drain via exchange-and-reset so the audio thread starts
        // a fresh max window after each frame.  Decay applied in UI thread.
        const float incoming = mLevel.exchange (0.f, std::memory_order_relaxed);
        // Snap up to incoming if higher; otherwise apply fall-off.
        // (mLevel currently sits at 0 post-exchange; no separate display
        // variable needed -- this Style is used by GR-only paths now.)
        if (incoming > 0.f) mLevel.store (incoming, std::memory_order_relaxed);

        if (mPeakHoldFrames > 0) --mPeakHoldFrames;
        else
            mPeak.store(juce::jmax(0.f, mPeak.load(std::memory_order_relaxed) - kFallRate * 0.5f),
                        std::memory_order_relaxed);
        repaint();
        return;
    }

    {
        // 2026-05-02: ditched the ANSI C16.5-1942 spring-damper.  Rationale:
        // a 300 ms rise time literally cannot register a 10 ms snare hit at
        // 60+ Hz vblank cadence, so transient signals showed -16 VU when a
        // continuous identical-level signal pegged the meter.  Replaced with
        // simple snap-up + dB-domain decay -- same ballistics as DBFSMeter,
        // matches what FL / Logic / ProTools "VU-style" meters actually do
        // (none of them implement true ANSI ballistics).  The skeuomorphic
        // gauge UI is unchanged; only the math changed.
        //
        // Decay rate: 25 dB/sec ≈ ANSI fall time perception without the
        // sluggish rise.  Frame interval is computed from real wall clock so
        // 60 / 120 / 144 Hz monitors all see the same dB-per-second fall.
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        const double dt    = juce::jlimit (0.001, 0.100,
                                            (mLastVBlankMs <= 0.0) ? 1.0 / 60.0
                                                                    : (nowMs - mLastVBlankMs) * 0.001);
        mLastVBlankMs = nowMs;
        constexpr float kVuDecayDbPerSec = 25.f;
        const float decayDb = (float) (kVuDecayDbPerSec * dt);

        // Drain the running-max input atomic (audio CAS-maxes between frames;
        // exchange-with-0 starts a fresh window for the next frame).
        const float raw01 = mLevelRms01.exchange (0.f, std::memory_order_relaxed);
        const float dBFS  = (raw01 <= 0.f) ? -100.f : 20.f * std::log10 (raw01);

        // Map to VU dB via the calibration target (0 VU = e.g. -18 dBFS).
        const float calibTarget = VUMeter::getCalibrationDb();
        const float vuDb = juce::jlimit (kVuMin - 1.f, kVuMax + 60.f,
                                          dBFS - calibTarget);

        // Snap-up + decay on the displayed value (in VU dB space).
        if (vuDb > mDisplayLevel) mDisplayLevel = vuDb;
        else                       mDisplayLevel = juce::jmax (kVuMin, mDisplayLevel - decayDb);

        // MAX peak tracking -- hold ~1 s, then decay.  Same wall-clock-based
        // timer so it stays consistent across monitor refresh rates.
        if (vuDb > mPeakDb)
        {
            mPeakDb = vuDb;
            mPeakMaxHoldFrames = 0;
        }
        else
        {
            mPeakMaxHoldFrames++;
            if (mPeakMaxHoldFrames > 60)   // ~1 s @ 60 Hz, slightly less @ higher refresh
                mPeakDb = juce::jmax (kVuMin, mPeakDb - decayDb);
        }

        // mCurrentPos / mVelocity legacy spring state -- compute from
        // mDisplayLevel so the existing paint() code that reads mCurrentPos
        // keeps rendering at the right needle position.
        mCurrentPos = juce::jlimit (0.f, 1.f, (mDisplayLevel - kVuMin) / (kVuMax - kVuMin));
        mVelocity   = 0.f;
    }

    repaint();
}

// ── Horizontal bar-style GR meter (legacy - used in LayersPage glue comp) ─────
void VUMeter::paintHorizontal(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.f);
    float bw = bounds.getWidth(), bh = bounds.getHeight();

    // Snapshot atomics once for consistent paint pass (message thread read)
    const float level = mLevel.load(std::memory_order_relaxed);
    const float peak  = mPeak.load(std::memory_order_relaxed);
    const float gr    = mGR.load(std::memory_order_relaxed);

    // Background groove
    g.setColour(VC::Bg);
    g.fillRoundedRectangle(bounds, 2.f);
    g.setColour(VC::Accent.withAlpha(0.5f));
    g.drawRoundedRectangle(bounds, 2.f, 0.5f);

    // Level bar with green→yellow→red gradient
    if (level > 0.f)
    {
        float fillW = bw * level;
        auto grad = juce::ColourGradient(VC::Green, bounds.getX(), bounds.getCentreY(),
                                         VC::Highlight, bounds.getX() + fillW, bounds.getCentreY(), false);
        grad.addColour(0.7, VC::Yellow);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds.getX(), bounds.getY(), fillW, bh, 2.f);
    }

    // GR overlay (blue, from the right)
    if (gr < -0.5f)
    {
        float grFrac = juce::jlimit(0.f, 1.f, -gr / 40.f);
        float grW    = bw * grFrac;
        g.setColour(VC::Blue.withAlpha(0.55f));
        g.fillRoundedRectangle(bounds.getRight() - grW, bounds.getY(), grW, bh, 2.f);
    }

    // Peak hold indicator
    if (peak > 0.01f)
    {
        g.setColour(peak > 0.9f ? VC::Highlight : juce::Colours::white.withAlpha(0.8f));
        g.fillRect(bounds.getX() + bw * peak - 1.f, bounds.getY(), 2.f, bh);
    }
}

// ── Vertical hardware VU meter recreation ─────────────────────────────────────
void VUMeter::paintVerticalVU(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // ── Filmstrip render (128x128, 100 frames) ────────────────────────────────
    // Stretch the selected frame to fill the full component bounds - JUCE's
    // drawImage scales source to destination, so the image fills width × height.
    {
        const auto& strip = Filmstrips::vuMeter();
        if (strip.isValid())
        {
            // Fill the full component - stretches horizontally to match component width
            Filmstrips::drawFrame(g, strip, 128, 128, 100, mCurrentPos,
                                  getLocalBounds().toFloat());

            // Overlay live CURRENT / MAX LCD boxes on top of the filmstrip
            auto bInner       = getLocalBounds().toFloat().reduced(2.5f);
            const float lcdH  = bInner.getHeight() * 0.18f;
            const float boxW  = bInner.getWidth() * 0.40f;
            const float boxH  = lcdH * 0.55f;
            const float lcdTop   = bInner.getBottom() - lcdH;
            const float labelHt  = lcdH * 0.30f;
            const float gap      = bInner.getWidth() * 0.05f;
            float curBoxX = bInner.getX() + gap;
            float maxBoxX = bInner.getCentreX() + gap * 0.5f;
            float boxY    = lcdTop + labelHt + 1.f;

            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 5.5f, juce::Font::plain));
            g.setColour(juce::Colour(0xff555555));
            g.drawText("CURRENT", juce::Rectangle<float>(curBoxX, lcdTop, boxW, labelHt),
                       juce::Justification::centredBottom, false);
            g.drawText("MAX",     juce::Rectangle<float>(maxBoxX, lcdTop, boxW, labelHt),
                       juce::Justification::centredBottom, false);

            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 7.f, juce::Font::plain));
            g.setColour(juce::Colour(0xffD3D3D3));
            g.fillRoundedRectangle(curBoxX, boxY, boxW, boxH, 2.f);
            g.setColour(juce::Colour(0xff333333));
            g.drawText(juce::String(juce::jlimit(kVuMin, kVuMax, mDisplayLevel), 1),
                       juce::Rectangle<float>(curBoxX + 2.f, boxY + 1.f, boxW - 4.f, boxH - 2.f),
                       juce::Justification::centred, false);
            g.setColour(juce::Colour(0xffD3D3D3));
            g.fillRoundedRectangle(maxBoxX, boxY, boxW, boxH, 2.f);
            g.setColour(juce::Colour(0xff333333));
            g.drawText(juce::String(juce::jlimit(kVuMin, kVuMax, mPeakDb), 1),
                       juce::Rectangle<float>(maxBoxX + 2.f, boxY + 1.f, boxW - 4.f, boxH - 2.f),
                       juce::Justification::centred, false);
            return;
        }
    }

    // LRX-10: Metal bezel - chrome ring drawn before cream plate
    {
        juce::ColourGradient bezel(
            juce::Colour(0xffC0C0C0), b.getX(),      b.getY(),
            juce::Colour(0xff505050), b.getRight(),   b.getBottom(), false);
        bezel.addColour(0.5, juce::Colour(0xffA8A8A8));
        g.setGradientFill(bezel);
        g.fillRoundedRectangle(b, 5.f);
    }
    b = b.reduced(2.5f);  // cream plate inset inside bezel

    // ── 1. CREAM PLATE ────────────────────────────────────────────────────
    g.setColour(juce::Colour(0xffF5F5DC));
    g.fillRoundedRectangle(b, 4.f);

    juce::ColourGradient innerGlow(juce::Colour(0xffFFFFF0), b.getCentreX(), b.getCentreY(),
                                   juce::Colour(0xffE8E8CC), b.getX(), b.getY(), true);
    g.setGradientFill(innerGlow);
    g.fillRoundedRectangle(b.reduced(2.f), 3.f);

    // ── Layout constants ──────────────────────────────────────────────────
    // Reserve bottom 18% for LCD boxes; arc lives above that
    const float lcdH    = b.getHeight() * 0.18f;
    const float arcAreaH = b.getHeight() - lcdH;

    // Pivot: bottom-center of the arc area
    const float pivotX = b.getCentreX();
    const float pivotY = b.getY() + arcAreaH + b.getHeight() * 0.02f;

    // Needle length: 70% of component height
    const float needleLength = b.getHeight() * 0.70f;

    // Arc spans 120 degrees total (-60..+60 deg from vertical up)
    const float arcStartAngle = -juce::MathConstants<float>::pi * 0.333f;  // ~-60 deg from up
    const float arcEndAngle   =  juce::MathConstants<float>::pi * 0.333f;  // ~+60 deg from up

    // Map VU dB to angle
    auto vuToAngle = [&](float vuDb) -> float {
        float t = (vuDb - kVuMin) / (kVuMax - kVuMin);
        t = juce::jlimit(0.f, 1.f, t);
        return arcStartAngle + t * (arcEndAngle - arcStartAngle);
    };

    // Convert polar angle (0=up, positive=clockwise) to cartesian from pivot
    auto polarToXY = [&](float angle, float radius) -> juce::Point<float> {
        return { pivotX + radius * std::sin(angle),
                 pivotY - radius * std::cos(angle) };
    };

    // ── 2. ARC SCALE ─────────────────────────────────────────────────────
    // Draw filled red block for 0..+3VU arc segment
    {
        const float zeroAngle  = vuToAngle(0.f);
        const float maxAngle   = vuToAngle(kVuMax);
        const float innerR     = needleLength * 0.82f;
        const float outerR     = needleLength * 0.95f;

        juce::Path redBlock;
        const int   redSteps = 12;
        for (int i = 0; i <= redSteps; ++i)
        {
            float frac  = (float)i / redSteps;
            float angle = zeroAngle + frac * (maxAngle - zeroAngle);
            auto  pt    = polarToXY(angle, outerR);
            if (i == 0) redBlock.startNewSubPath(pt.x, pt.y);
            else        redBlock.lineTo(pt.x, pt.y);
        }
        for (int i = redSteps; i >= 0; --i)
        {
            float frac  = (float)i / redSteps;
            float angle = zeroAngle + frac * (maxAngle - zeroAngle);
            auto  pt    = polarToXY(angle, innerR);
            redBlock.lineTo(pt.x, pt.y);
        }
        redBlock.closeSubPath();
        g.setColour(juce::Colour(0xffFF2D2D));
        g.fillPath(redBlock);
    }

    // Tick marks and labels along the arc
    struct ScaleMark { float vuDb; const char* label; bool major; };
    static const ScaleMark kMarks[] = {
        { -20.f, "-20", true  },
        { -10.f, "-10", true  },
        {  -7.f,  "-7", true  },
        {  -5.f,  "-5", true  },
        {  -3.f,  "-3", true  },
        {  -1.f,  "-1", true  },  // major - show label
        {   0.f,   "0", true  },
        {  +1.f,  "+1", true  },
        {  +2.f,  "+2", true  },
        {  +3.f,  "+3", true  },
    };

    const float tickInnerR  = needleLength * 0.86f;
    const float tickOuterR  = needleLength * 0.97f;
    const float minorInnerR = needleLength * 0.90f;
    const float labelR      = needleLength * 0.75f;

    for (auto& mk : kMarks)
    {
        bool  isRed = (mk.vuDb >= 0.f);
        float angle = vuToAngle(mk.vuDb);
        auto  inner = polarToXY(angle, mk.major ? tickInnerR : minorInnerR);
        auto  outer = polarToXY(angle, tickOuterR);

        g.setColour(isRed ? juce::Colour(0xffFF2D2D) : juce::Colour(0xff1A1A1A));
        g.drawLine(inner.x, inner.y, outer.x, outer.y, mk.major ? 1.5f : 1.0f);

        if (mk.major && mk.label[0] != '\0')
        {
            auto labelPt = polarToXY(angle, labelR);
            g.setFont(juce::Font(6.5f, juce::Font::bold));
            g.setColour(isRed ? juce::Colour(0xffFF2D2D) : juce::Colour(0xff1A1A1A));
            g.drawText(juce::String(mk.label),
                       juce::Rectangle<float>(labelPt.x - 10.f, labelPt.y - 5.f, 20.f, 10.f),
                       juce::Justification::centred, false);
        }
    }

    // Minor intermediate ticks at -15, -4, -2
    static const float kMinorTicks[] = { -15.f, -4.f, -2.f };
    for (float vuDb : kMinorTicks)
    {
        float angle = vuToAngle(vuDb);
        auto  inner = polarToXY(angle, minorInnerR);
        auto  outer = polarToXY(angle, tickOuterR);
        g.setColour(juce::Colour(0xff1A1A1A));
        g.drawLine(inner.x, inner.y, outer.x, outer.y, 0.8f);
    }

    // ── 3. NEEDLE ─────────────────────────────────────────────────────────
    float needleAngle = vuToAngle(juce::jlimit(kVuMin, kVuMax, mDisplayLevel));

    auto tipPt = polarToXY(needleAngle, needleLength);

    // Drop shadow (drawn offset by {1,1})
    {
        // Perpendicular direction for base width
        float perpSin = std::cos(needleAngle);  // perp of (sin,−cos) is (cos, sin)
        float perpCos = std::sin(needleAngle);
        const float baseHalf = 1.5f;

        juce::Path needleShadow;
        needleShadow.startNewSubPath(pivotX - baseHalf * perpSin + 1.f,
                                     pivotY - baseHalf * perpCos + 1.f);
        needleShadow.lineTo        (pivotX + baseHalf * perpSin + 1.f,
                                    pivotY + baseHalf * perpCos + 1.f);
        needleShadow.lineTo        (tipPt.x + 1.f, tipPt.y + 1.f);
        needleShadow.closeSubPath  ();

        juce::DropShadow shadow(juce::Colours::black.withAlpha(0.35f), 2, {1, 1});
        shadow.drawForPath(g, needleShadow);
    }

    // Needle path proper
    {
        float perpSin = std::cos(needleAngle);
        float perpCos = std::sin(needleAngle);
        const float baseHalf = 1.5f;

        juce::Path needle;
        needle.startNewSubPath(pivotX - baseHalf * perpSin,
                               pivotY - baseHalf * perpCos);
        needle.lineTo          (pivotX + baseHalf * perpSin,
                                pivotY + baseHalf * perpCos);
        needle.lineTo          (tipPt.x, tipPt.y);
        needle.closeSubPath    ();

        g.setColour(juce::Colour(0xff1A1A1A));
        g.fillPath(needle);
    }

    // ── 4. PIVOT ──────────────────────────────────────────────────────────
    g.setColour(juce::Colour(0xff1A1A1A));
    g.fillEllipse(pivotX - 5.f, pivotY - 5.f, 10.f, 10.f);

    // ── 5. LCD BOXES ──────────────────────────────────────────────────────
    const float boxW    = b.getWidth() * 0.40f;
    const float boxH    = lcdH * 0.55f;
    const float lcdTop  = b.getBottom() - lcdH;
    const float labelHt = lcdH * 0.30f;
    const float gap     = b.getWidth() * 0.05f;

    // Compute box rects
    float curBoxX = b.getX() + gap;
    float maxBoxX = b.getCentreX() + gap * 0.5f;
    float boxY    = lcdTop + labelHt + 1.f;

    // CURRENT label
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 5.5f, juce::Font::plain));
    g.setColour(juce::Colour(0xff555555));
    g.drawText("CURRENT",
               juce::Rectangle<float>(curBoxX, lcdTop, boxW, labelHt),
               juce::Justification::centredBottom, false);

    // MAX label
    g.drawText("MAX",
               juce::Rectangle<float>(maxBoxX, lcdTop, boxW, labelHt),
               juce::Justification::centredBottom, false);

    // CURRENT box
    g.setColour(juce::Colour(0xffD3D3D3));
    g.fillRoundedRectangle(curBoxX, boxY, boxW, boxH, 2.f);
    g.setColour(juce::Colour(0xff333333));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 7.f, juce::Font::plain));
    {
        float cur = juce::jlimit(kVuMin, kVuMax, mDisplayLevel);
        juce::String txt = juce::String(cur, 1);
        g.drawText(txt,
                   juce::Rectangle<float>(curBoxX + 2.f, boxY + 1.f, boxW - 4.f, boxH - 2.f),
                   juce::Justification::centred, false);
    }

    // MAX box
    g.setColour(juce::Colour(0xffD3D3D3));
    g.fillRoundedRectangle(maxBoxX, boxY, boxW, boxH, 2.f);
    g.setColour(juce::Colour(0xff333333));
    {
        float pk  = juce::jlimit(kVuMin, kVuMax, mPeakDb);
        juce::String txt = juce::String(pk, 1);
        g.drawText(txt,
                   juce::Rectangle<float>(maxBoxX + 2.f, boxY + 1.f, boxW - 4.f, boxH - 2.f),
                   juce::Justification::centred, false);
    }

    // Outer border
    g.setColour(juce::Colour(0xffA0A080).withAlpha(0.6f));
    g.drawRoundedRectangle(b, 4.f, 1.f);

    // LRX-10: Glass lens overlay - convex reflection sweep top-left corner
    {
        juce::ColourGradient lens(
            juce::Colours::white.withAlpha(0.18f), b.getX() + b.getWidth() * 0.15f, b.getY() + b.getHeight() * 0.08f,
            juce::Colours::transparentWhite,        b.getX() + b.getWidth() * 0.55f, b.getY() + b.getHeight() * 0.40f, false);
        g.setGradientFill(lens);
        g.fillRoundedRectangle(b, 4.f);
        // Bottom-edge internal reflection (grazing light)
        juce::ColourGradient bottomRef(
            juce::Colours::transparentWhite,        b.getCentreX(), b.getBottom() - 6.f,
            juce::Colours::white.withAlpha(0.08f),  b.getCentreX(), b.getBottom(), false);
        g.setGradientFill(bottomRef);
        g.fillRoundedRectangle(b, 4.f);
    }

    // LRX-10: Bezel corner mounting screws (manual - VU meter may be small)
    {
        const float sr = 3.5f;  // screw radius
        const float inset = 5.5f;
        // Restore b to full bezel bounds (undo reduced)
        auto bb = getLocalBounds().toFloat();
        juce::Point<float> corners[4] = {
            { bb.getX() + inset,        bb.getY() + inset        },
            { bb.getRight() - inset,    bb.getY() + inset        },
            { bb.getX() + inset,        bb.getBottom() - inset   },
            { bb.getRight() - inset,    bb.getBottom() - inset   },
        };
        for (auto& c : corners)
        {
            // Housing
            juce::ColourGradient sg(juce::Colour(0xff888888), c.x - sr, c.y - sr,
                                    juce::Colour(0xff333333), c.x + sr, c.y + sr, false);
            g.setGradientFill(sg);
            g.fillEllipse(c.x - sr, c.y - sr, sr * 2, sr * 2);
            // Phillips cross
            g.setColour(juce::Colour(0xff111111).withAlpha(0.9f));
            g.drawLine(c.x - sr * 0.55f, c.y, c.x + sr * 0.55f, c.y, 0.8f);
            g.drawLine(c.x, c.y - sr * 0.55f, c.x, c.y + sr * 0.55f, 0.8f);
            // Specular glint
            g.setColour(juce::Colours::white.withAlpha(0.40f));
            g.fillEllipse(c.x - sr * 0.35f, c.y - sr * 0.45f, sr * 0.4f, sr * 0.4f);
        }
    }
}

void VUMeter::paint(juce::Graphics& g)
{
    if (mStyle == Horizontal)
        paintHorizontal(g);
    else
        paintVerticalVU(g);
}

// ============================================================ LufsReadoutBox
LufsReadoutBox::LufsReadoutBox()
{
    // Load the persisted mode from settings.xml (preserve other sections).
    auto f = ProjectManager::getSettingsFile();
    if (f.existsAsFile())
        if (auto xml = SafeXml::parse (f))
            if (auto* node = xml->getChildByName ("MasterLufsMode"))
                mMode = juce::jlimit (0, 2, node->getIntAttribute ("mode", 0));
    refreshTooltip();
}

void LufsReadoutBox::setValues (float momentary, float shortTerm, float integrated)
{
    if (mVals[0] == momentary && mVals[1] == shortTerm && mVals[2] == integrated)
        return;
    mVals[0] = momentary; mVals[1] = shortTerm; mVals[2] = integrated;
    refreshTooltip();
    repaint();
}

juce::String LufsReadoutBox::modeName (int mode)
{
    return mode == 1 ? "Short Term" : mode == 2 ? "Integrated" : "Momentary";
}

void LufsReadoutBox::refreshTooltip()
{
    const float v = mVals[juce::jlimit (0, 2, mMode)];
    const juce::String vs = (v <= -100.f) ? juce::String ("--") : juce::String (v, 1);
    setTooltip (modeName (mMode) + " loudness: " + vs + " LUFS  (click to change mode)");
}

void LufsReadoutBox::applyMode (int mode, bool persist)
{
    mMode = juce::jlimit (0, 2, mode);
    if (persist)
    {
        // Preserve other sections written by ProjectManager / other UI.
        auto f = ProjectManager::getSettingsFile();
        f.getParentDirectory().createDirectory();
        std::unique_ptr<juce::XmlElement> root;
        if (f.existsAsFile())
            root = SafeXml::parse (f);
        if (root == nullptr)
            root = std::make_unique<juce::XmlElement> ("BaySickDAWSettings");
        root->removeChildElement (root->getChildByName ("MasterLufsMode"), true);
        root->createNewChildElement ("MasterLufsMode")->setAttribute ("mode", mMode);
        root->writeTo (f);
    }
    refreshTooltip();
    repaint();
}

void LufsReadoutBox::mouseDown (const juce::MouseEvent&)
{
    juce::PopupMenu m;
    m.addItem (1, "Momentary",  true, mMode == 0);
    m.addItem (2, "Short Term", true, mMode == 1);
    m.addItem (3, "Integrated", true, mMode == 2);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                     [this] (int r) { if (r > 0) applyMode (r - 1, true); });
}

void LufsReadoutBox::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Recessed housing (matches the dark LUFS / meter panel palette).
    g.setColour (juce::Colour (0xff0A0C0E));
    g.fillRoundedRectangle (b, 2.0f);
    g.setColour (juce::Colour (0xff2A2E30));
    g.drawRoundedRectangle (b.reduced (0.5f), 2.0f, 1.0f);

    auto inner = b.reduced (3.0f, 2.0f);

    // Top row: the LUFS value (prominent), with a small down-caret on the right.
    auto valueRow = inner.removeFromTop (inner.getHeight() * 0.56f);
    {
        auto caretArea = valueRow.removeFromRight (9.0f);
        auto cr = caretArea.withSizeKeepingCentre (7.0f, 4.0f);
        juce::Path tri;
        tri.addTriangle (cr.getX(), cr.getY(),
                         cr.getRight(), cr.getY(),
                         cr.getCentreX(), cr.getBottom());
        g.setColour (juce::Colour (0xff9AA0A2));
        g.fillPath (tri);
    }
    const float v = mVals[juce::jlimit (0, 2, mMode)];
    const juce::String valueStr = (v <= -100.f) ? juce::String ("--") : juce::String (v, 1);
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold));
    g.drawText (valueStr, valueRow, juce::Justification::centred, false);

    // Bottom row: the full mode title underneath (spec #1).  drawFittedText so
    // "Short Term" / "Integrated" never clip in the ~44 px column.
    g.setColour (juce::Colour (0xff9AA0A2));
    g.setFont (juce::Font (10.0f, juce::Font::plain));
    g.drawFittedText (modeName (mMode), inner.toNearestInt(), juce::Justification::centred, 1);
}

// ============================================================ DBFSMeter
DBFSMeter::DBFSMeter()
{
    // QA-RustyMeter close fix (2026-05-30, /review-batch NEEDS-FIX): pre-fill the
    // RMS history ring to kFloor.  Value-init left it at 0.0f (= 0 dB ~= 0.925
    // deflection), so on a Split meter the not-yet-written slots painted a
    // misleading near-full-width "loud" band for the first ~4 s (until the ring
    // fills) on every non-master strip at launch.  kFloor renders flat (silent).
    mRmsHistL.fill (kFloor);
    mRmsHistR.fill (kFloor);
    // Vblank attachment is created in parentHierarchyChanged once the
    // component is on a peer.  Until then nothing fires; safe.
}

juce::String DBFSMeter::getTooltip()
{
    auto fmt = [] (float dB) -> juce::String
    {
        if (dB <= kFloor + 0.5f) return juce::String ("-inf");
        return juce::String (dB, 1) + " dB";
    };
    return "L: " + fmt (mDisplayDbL) + "  |  R: " + fmt (mDisplayDbR);
}

// 2026-05-02: setLevel/setStereoLevel are CAS-loop max writes -- audio thread
// raises the running max if the new value is bigger, otherwise no-ops.  The
// UI's vblank callback exchanges these with -inf to start a fresh "max within
// frame" window, so transient peaks between vblanks aren't lost.
namespace
{
    inline void casMaxLevel (std::atomic<float>& a, float v) noexcept
    {
        float cur = a.load (std::memory_order_relaxed);
        while (cur < v && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed))
        {}
    }
}

void DBFSMeter::setLevel(float dBFS)
{
    casMaxLevel (mLevelDbL, dBFS);
    casMaxLevel (mLevelDbR, dBFS);
}

void DBFSMeter::setStereoLevel(float dBFS_L, float dBFS_R)
{
    casMaxLevel (mLevelDbL, dBFS_L);
    casMaxLevel (mLevelDbR, dBFS_R);
}

void DBFSMeter::parentHierarchyChanged()
{
    // Create / destroy the vblank attachment based on whether the meter is
    // currently attached to a top-level peer.  JUCE only fires VBlank
    // callbacks while the host window is on screen; the attachment itself
    // costs nothing when the component isn't visible.
    if (getPeer() != nullptr && mVBlank == nullptr)
    {
        mVBlank = std::make_unique<juce::VBlankAttachment> (
            this, [this] { onVBlank(); });
        mLastVBlankMs = juce::Time::getMillisecondCounterHiRes();
    }
    else if (getPeer() == nullptr && mVBlank != nullptr)
    {
        mVBlank.reset();
    }
}

void DBFSMeter::onVBlank()
{
    // Exchange-and-reset reads the running-max written by the audio thread
    // since the last vblank and starts a fresh window.  -inf is the sentinel:
    // any new audio sample's dB will be larger and CAS will replace it.
    constexpr float kNegInf = -std::numeric_limits<float>::infinity();
    const float incomingL = mLevelDbL.exchange (kNegInf, std::memory_order_relaxed);
    const float incomingR = mLevelDbR.exchange (kNegInf, std::memory_order_relaxed);

    // Convert -inf to the visible floor so the dB-domain ballistics math
    // doesn't try to subtract decay from -inf.  Anything below kFloor is
    // already fully-empty visually.
    const float clampedL = (incomingL <= kFloor) ? kFloor : incomingL;
    const float clampedR = (incomingR <= kFloor) ? kFloor : incomingR;

    // Delta-time ballistics -- both attack and release scale with frame
    // interval so 60/120/144 Hz monitors all see the same time constants.
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    const double dt    = juce::jlimit (0.001, 0.100, (nowMs - mLastVBlankMs) * 0.001);
    mLastVBlankMs = nowMs;
    const float decayDb = (float) (kDecayDbPerSec * dt);

    // QA-Eg: smooth attack via exponential filter (FL-style visual low-pass).
    // Time constant ~20 ms = fast enough to register transients clearly, slow
    // enough that bursty audio doesn't strobe the bar (or any cables routed
    // through getCurrentDisplayedDb()).  Peak-hold marker keeps INSTANT attack
    // below so the per-channel peak indicator still snaps to actual peaks.
    constexpr float kAttackTimeConstSec = 0.020f;
    const float alphaAttack = juce::jlimit (0.0f, 1.0f,
        1.0f - std::exp (-(float)dt / kAttackTimeConstSec));

    auto step = [&] (float incoming, float& display, float& peak, double& holdUntil)
    {
        if (incoming > display)
            display += (incoming - display) * alphaAttack;
        else
            display = juce::jmax (kFloor, display - decayDb);

        if (incoming >= peak) { peak = incoming; holdUntil = nowMs + kPeakHoldMs; }
        else if (nowMs > holdUntil)
        {
            peak = juce::jmax (kFloor, peak - decayDb);
        }
    };

    step (clampedL, mDisplayDbL, mPeakDbL, mPeakHoldUntilL);
    step (clampedR, mDisplayDbR, mPeakDbR, mPeakHoldUntilR);

    // QA-RustyMeter: EMA-smooth the incoming per-frame RMS (~50 ms window, #5b)
    // then push it into the scrolling history ring (Split layout only).  Newest
    // at mRmsHead-1; paintRmsWaveform reads back.  The audio thread publishes a
    // fixed 5 ms windowed RMS (BaySickGraph's MeterRmsWindow), CAS-maxed so several
    // windows landing inside one UI frame merge to their max; this EMA is the
    // display ballistic on top of it, and stays correct for multi-call InsertKinds.
    {
        const float alphaRms = juce::jlimit (0.f, 1.f,
            1.f - std::exp (-(float) dt / kRmsTimeConstSec));
        const float inL = (mRmsInL <= kFloor) ? kFloor : mRmsInL;
        const float inR = (mRmsInR <= kFloor) ? kFloor : mRmsInR;
        mRmsDispL += (inL - mRmsDispL) * alphaRms;
        mRmsDispR += (inR - mRmsDispR) * alphaRms;
        if (mLayout == Layout::Split)
        {
            mRmsHistL[(size_t) mRmsHead] = mRmsDispL;
            mRmsHistR[(size_t) mRmsHead] = mRmsDispR;
            mRmsHead = (mRmsHead + 1) % kRmsHist;
        }
    }

    repaint();
}

// 2026-04-30: piecewise-linear log-style mapping.  Bottom 70 % of the bar
// covers -60..-18 dB; top 30 % covers -18..+6 dB.  Aggressive enough to put
// the action near 0 dB on screen without obliterating the lower range.
float DBFSMeter::dbToNorm(float dB) noexcept
{
    if (dB <= kFloor)   return 0.f;
    if (dB >= kCeiling) return 1.f;
    if (dB <= kBreakDb)
        return juce::jmap(dB, kFloor, kBreakDb, 0.f, kBreakNorm);
    return juce::jmap(dB, kBreakDb, kCeiling, kBreakNorm, 1.f);
}

// timerCallback removed (2026-05-02) -- replaced by onVBlank above.  The new
// callback runs on the monitor's vsync interval (60/120/144 Hz on matching
// monitors) instead of JUCE's coalesced 60 Hz timer, so meter updates stay
// in lockstep with the display refresh and never drop frames.

// Paint one bar (L or R half).  drawLabels=true on the LEFT half so labels
// appear once across the whole meter, naturally covered by lit segments.
void DBFSMeter::paintBar(juce::Graphics& g, juce::Rectangle<float> r,
                          float displayDb, float peakDb, bool drawLabels) const
{
    const float zeroY  = r.getY();
    const float floorY = r.getBottom();
    const float totalH = floorY - zeroY;

    const float displayNorm = dbToNorm(displayDb);
    const float peakNorm    = dbToNorm(peakDb);

    // LED segment geometry - same look as the legacy meter.
    const float gap   = 1.5f;
    const float segH  = 4.5f;
    const int numSegs = juce::jmax(1, (int)(totalH / (segH + gap)));

    const float segW = r.getWidth() - 1.f;
    const float segX = r.getX() + 0.5f;

    const float peakY   = floorY - peakNorm * totalH;
    const int   peakSeg = (int)((floorY - peakY) / (segH + gap));

    // PASS 1 - paint every segment as DIM so tick labels overlay onto the
    // unlit background.  Lit segments paint over them in PASS 3.
    for (int i = 0; i < numSegs; ++i)
    {
        const float segTop = floorY - (i + 1) * (segH + gap) + gap;
        const float norm   = (float)(i + 1) / (float)numSegs;

        // Pick the segment's nominal color (dim version).
        juce::Colour col;
        if      (norm > 0.94f) col = juce::Colour(0xffFF2020);   // clip red (above 0 dB)
        else if (norm > 0.85f) col = juce::Colour(0xffFF6020);   // hot red (-3..0)
        else if (norm > 0.72f) col = juce::Colour(0xffFFCC00);   // yellow
        else                   col = juce::Colour(0xff22EE44);   // green

        g.setColour(col.withAlpha(0.09f));
        g.fillRect(segX, segTop, segW, segH);
    }

    // PASS 2 - tick labels INSIDE the meter (only on the left half so they
    // appear once across the whole bar).  These get covered by the lit segs
    // in pass 3 wherever the meter is filled - exactly the FL behaviour.
    if (drawLabels)
    {
        static const int kLabels[] = { 6, 0, -3, -6, -9, -12, -18, -24, -36, -48 };
        g.setColour(juce::Colour(0xff7A7E80));   // dim grey, readable on dim segs
        g.setFont(juce::Font(7.5f, juce::Font::plain));
        // Labels live in a tiny strip on the LEFT inside the half.  We let
        // them spill across the half-width so the digits aren't crushed.
        for (int db : kLabels)
        {
            const float n = dbToNorm((float) db);
            const float y = floorY - n * totalH;
            // Center the text vertically on the tick line.
            g.drawText(juce::String(db), (int)(r.getX() + 1), (int)(y - 4),
                       (int)(r.getWidth() * 2.f), 8,
                       juce::Justification::centredLeft);
        }
    }

    // PASS 3 - lit segments (and the white peak-hold marker).  These paint
    // ON TOP of the dim+labels layers; wherever a segment is lit the labels
    // beneath it disappear (covered by the bright fill).
    for (int i = 0; i < numSegs; ++i)
    {
        const float segTop = floorY - (i + 1) * (segH + gap) + gap;
        const float norm   = (float)(i + 1) / (float)numSegs;

        const bool lit    = (norm <= displayNorm);
        const bool isPeak = (i == peakSeg && peakNorm > 0.01f);
        if (! lit && ! isPeak) continue;

        juce::Colour col;
        if      (norm > 0.94f) col = juce::Colour(0xffFF2020);   // clip red (above 0)
        else if (norm > 0.85f) col = juce::Colour(0xffFF6020);   // hot red
        else if (norm > 0.72f) col = juce::Colour(0xffFFCC00);   // yellow
        else                   col = juce::Colour(0xff22EE44);   // green
        if (isPeak) col = juce::Colours::white;

        juce::ColourGradient litGrad(
            col.withAlpha(0.95f), segX, segTop,
            col.withAlpha(0.60f), segX, segTop + segH, false);
        g.setGradientFill(litGrad);
        g.fillRect(segX, segTop, segW, segH);

        // Specular highlight on top edge.
        g.setColour(juce::Colours::white.withAlpha(isPeak ? 0.55f : 0.22f));
        g.fillRect(segX, segTop, segW, 1.f);
    }
}

// QA-RustyMeter: paint the L/R peak bars into rect r (extracted from paint() so
// the Split layout can render them in the bottom half).
void DBFSMeter::paintBars (juce::Graphics& g, juce::Rectangle<float> r) const
{
    const float halfW  = (r.getWidth() - 1.f) * 0.5f;
    const auto  leftR  = juce::Rectangle<float> (r.getX(),               r.getY(), halfW, r.getHeight());
    const auto  rightR = juce::Rectangle<float> (r.getX() + halfW + 1.f, r.getY(), halfW, r.getHeight());
    paintBar (g, leftR,  mDisplayDbL, mPeakDbL, /*drawLabels*/ true);
    paintBar (g, rightR, mDisplayDbR, mPeakDbR, /*drawLabels*/ false);
    // Center gutter - thin black line so the L|R split reads.
    g.setColour (juce::Colour (0xff000000));
    g.fillRect (r.getX() + halfW, r.getY(), 1.f, r.getHeight());
}

// QA-RustyMeter: centered scrolling RMS-history waveform for the top half of a
// Split meter.  L deflects left of centre, R deflects right; smooth dBFS-palette
// gradient by deflection (green centre -> red edge, same colours/thresholds as
// the peak bar); newest at top, scrolling down as the ring advances each vblank.
void DBFSMeter::paintRmsWaveform (juce::Graphics& g, juce::Rectangle<float> r) const
{
    if (r.getWidth() < 2.f || r.getHeight() < 2.f) return;

    g.setColour (juce::Colour (0xff0A0C0E));   // recessed housing for the RMS zone
    g.fillRect (r);

    const float cx   = r.getCentreX();
    const float half = r.getWidth() * 0.5f - 1.f;
    const int   rows = juce::jmax (1, (int) r.getHeight());

    auto deflect = [&] (const std::array<float, (size_t) kRmsHist>& h, int row)
    {
        const int back = (int) ((float) row / (float) rows * (float) kRmsHist);
        const int j    = (mRmsHead - 1 - back + 2 * kRmsHist) % kRmsHist;
        return dbToNorm (h[(size_t) j]);          // 0..1 deflection (newest at top)
    };

    // Filled stereo waveform: R deflects right of centre, L deflects left.  The
    // band always spans the centre line, so it reads as one filled waveform
    // rather than two thin traces (Jeff 2026-05-30).
    juce::Path fill;
    for (int row = 0; row < rows; ++row)                 // right boundary, top -> bottom
    {
        const float x = cx + deflect (mRmsHistR, row) * half;
        const float y = r.getY() + (float) row;
        row == 0 ? fill.startNewSubPath (x, y) : fill.lineTo (x, y);
    }
    for (int row = rows - 1; row >= 0; --row)            // left boundary, bottom -> top
        fill.lineTo (cx - deflect (mRmsHistL, row) * half, r.getY() + (float) row);
    fill.closeSubPath();

    // Symmetric dBFS-palette gradient across the width: green at the centre line,
    // through yellow/orange, to red at both outer edges (#8, smooth gradient).
    const float yMid = r.getCentreY();
    juce::ColourGradient grad (juce::Colour (0xffFF2020), r.getX(),     yMid,
                               juce::Colour (0xffFF2020), r.getRight(), yMid, false);
    grad.addColour (0.12, juce::Colour (0xffFF6020));
    grad.addColour (0.26, juce::Colour (0xffFFCC00));
    grad.addColour (0.50, juce::Colour (0xff22EE44));
    grad.addColour (0.74, juce::Colour (0xffFFCC00));
    grad.addColour (0.88, juce::Colour (0xffFF6020));
    g.setGradientFill (grad);
    g.fillPath (fill);
}

void DBFSMeter::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Recessed housing background.
    {
        juce::ColourGradient housing(
            juce::Colour(0xff0A0C0E), b.getX(), b.getY(),
            juce::Colour(0xff161A1C), b.getRight(), b.getBottom(), false);
        g.setGradientFill(housing);
        g.fillRect(b);
        juce::ColourGradient topShadow(juce::Colours::black.withAlpha(0.7f), 0.f, b.getY(),
                                       juce::Colours::transparentBlack, 0.f, b.getY() + 5.f, false);
        g.setGradientFill(topShadow);
        g.fillRect(b.withBottom(b.getY() + 5.f));
    }

    // QA-RustyMeter: Split layout (non-master) = scrolling RMS waveform on the
    // top 35% + the L/R peak bars on the bottom 65%.  Full layout
    // (master) = the peak bars across the whole height (the LUFS box carries
    // loudness for the master strip instead).
    if (mLayout == Layout::Split)
    {
        const float splitY = b.getY() + b.getHeight() * kRmsTopFrac;
        paintRmsWaveform (g, b.withBottom (splitY));   // top 35% = RMS wave
        paintBars        (g, b.withTop    (splitY));   // bottom 65% = peak bar
    }
    else
    {
        paintBars (g, b);
    }

    // Outer frame / bezel.
    g.setColour(juce::Colour(0xff2A2E30));
    g.drawRect(b, 1.f);
}

// ── QuadrantButton ────────────────────────────────────────────────────────────
const juce::Colour QuadrantButton::kQuads[4] = {
    VC::LayerCol[0],   // L1 bright orange
    VC::LayerCol[1],   // L2 deep orange
    VC::LayerCol[2],   // L3 amber orange
    VC::LayerCol[3],   // L4 burnt orange
};

QuadrantButton::QuadrantButton(const juce::String& label)
    : juce::TextButton(label) {}

void QuadrantButton::paintButton(juce::Graphics& g, bool /*highlighted*/, bool /*down*/)
{
    auto b  = getLocalBounds().toFloat().reduced(1.f);
    float mx = b.getCentreX();
    float my = b.getCentreY();
    bool  active = getToggleState();
    float alpha  = active ? 1.0f : 0.28f;

    // Four quadrants: top-left, top-right, bottom-left, bottom-right
    g.setColour(kQuads[0].withAlpha(alpha));
    g.fillRect(b.getX(), b.getY(), b.getWidth() * 0.5f, b.getHeight() * 0.5f);
    g.setColour(kQuads[1].withAlpha(alpha));
    g.fillRect(mx,       b.getY(), b.getWidth() * 0.5f, b.getHeight() * 0.5f);
    g.setColour(kQuads[2].withAlpha(alpha));
    g.fillRect(b.getX(), my,       b.getWidth() * 0.5f, b.getHeight() * 0.5f);
    g.setColour(kQuads[3].withAlpha(alpha));
    g.fillRect(mx,       my,       b.getWidth() * 0.5f, b.getHeight() * 0.5f);

    // Border
    g.setColour(active ? VC::Text.withAlpha(0.6f) : VC::Accent);
    g.drawRect(b, 1.f);

    // Centered text label
    g.setColour(VC::Text);
    g.setFont(juce::Font(11.f, juce::Font::bold));
    g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred, false);
}

// ── ModulationLAF ─────────────────────────────────────────────────────────────

void ModulationLAF::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
    float sliderPos, float startAngle, float endAngle, juce::Slider& s)
{
    // ── Filmstrip render (70x70, 101 frames) ──────────────────────────────────
    {
        const auto& strip = Filmstrips::modulation();
        if (strip.isValid())
        {
            Filmstrips::drawFrame(g, strip, 70, 70, 101, sliderPos,
                                  juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h));
            return;
        }
    }

    auto bounds = juce::Rectangle<float>(x, y, w, h).reduced(4.f);
    auto centre = bounds.getCentre();
    float radius = bounds.getWidth() * 0.5f;
    float angle  = startAngle + sliderPos * (endAngle - startAngle);

    // LRX-2: 3-layer AO shadow stack
    {
        juce::Path skirtPath; skirtPath.addEllipse(bounds);
        LRXHelper::drawAO(g, skirtPath, true, juce::Colour(0xff0030ff));
    }

    // SKIRT - glossy black radial dome
    juce::ColourGradient skirtGrad(juce::Colour(0xff2A2A2A),
                                    centre.x, centre.y - radius * 0.4f,
                                    juce::Colour(0xff000000),
                                    centre.x, centre.y + radius, true);
    g.setGradientFill(skirtGrad);
    g.fillEllipse(bounds);

    // KNURLING - 40 fine lines around outer 10% of skirt
    {
        const int ridges = 40;
        for (int i = 0; i < ridges; ++i)
        {
            float a = juce::MathConstants<float>::twoPi * i / ridges;
            float innerR = radius * 0.875f, outerR = radius * 0.975f;
            g.setColour(juce::Colour(0xff3A3A3A));
            g.drawLine(centre.x + innerR * std::sin(a),
                       centre.y - innerR * std::cos(a),
                       centre.x + outerR * std::sin(a),
                       centre.y - outerR * std::cos(a), 0.6f);
        }
    }

    // INDICATOR LINE - on skirt, rotates with knob
    {
        juce::Graphics::ScopedSaveState ss(g);
        g.addTransform(juce::AffineTransform::rotation(angle, centre.x, centre.y));
        g.setColour(juce::Colours::white);
        g.drawLine(centre.x, centre.y - radius * 0.55f,
                   centre.x, centre.y - radius * 0.88f, 1.5f);
    }

    // CAP - brushed silver, STATIC (does not rotate)
    float capR = radius * 0.52f;
    juce::Rectangle<float> capB(centre.x - capR, centre.y - capR, capR * 2, capR * 2);

    g.setColour(juce::Colour(0xffBCC6CC));
    g.fillEllipse(capB);

    // Concentric brushed-metal texture: 32 thin rings
    for (int i = 0; i < 32; ++i)
    {
        float t  = (float)i / 32.f;
        float rr = capR * (0.08f + 0.88f * t);
        float alpha = 0.03f + 0.07f * std::abs(std::sin(t * juce::MathConstants<float>::pi * 4));
        g.setColour(juce::Colour(0xff000000).withAlpha(alpha));
        g.drawEllipse(centre.x - rr, centre.y - rr, rr * 2, rr * 2, 0.5f);
    }

    // Cap rim highlight (lit from top-left)
    juce::ColourGradient capHL(juce::Colours::white.withAlpha(0.45f),
                                centre.x - capR * 0.3f, centre.y - capR * 0.7f,
                                juce::Colours::transparentBlack, centre.x, centre.y, false);
    g.setGradientFill(capHL);
    g.fillEllipse(capB);

    // LRX-3: Anisotropic highlight streaks on brushed silver cap
    LRXHelper::drawAnisotropicHL(g, capB, 135.f);
}

void ModulationLAF::drawButtonBackground(juce::Graphics& g, juce::Button& b,
    const juce::Colour&, bool, bool isDown)
{
    auto bounds = b.getLocalBounds().toFloat().reduced(1.f);

    // Housing trench (slightly darker than panel)
    g.setColour(juce::Colour(0xff111111));
    g.fillRoundedRectangle(bounds, 2.f);

    if (b.getToggleState() || isDown)
    {
        // RECESSED: inner shadow on top edge - "pushed in"
        g.setColour(juce::Colour(0xff080808));
        g.fillRoundedRectangle(bounds.reduced(1.f), 1.5f);
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth(), 2.f);
    }
    else
    {
        // RAISED: highlight on top edge - "popped out"
        g.setColour(juce::Colour(0xff2A2A2A));
        g.fillRoundedRectangle(bounds.reduced(1.f), 1.5f);
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth(), 1.5f);
    }
}

// ── DynamicsLAF ───────────────────────────────────────────────────────────────

const juce::Identifier DynamicsLAF::kKnobVariant("knobVariant");

void DynamicsLAF::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
    float sliderPos, float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<float>(x, y, w, h).reduced(4.f);
    auto centre = bounds.getCentre();
    float radius = bounds.getWidth() * 0.5f;
    float angle  = startAngle + sliderPos * (endAngle - startAngle);
    bool isActive = s.isMouseOverOrDragging();

    auto variant = s.getProperties()[kKnobVariant].toString();
    if (variant == "chickenHead")
    {
        // ── Chicken Head filmstrip (66x66, 10 frames) ─────────────────────────
        const auto& strip = Filmstrips::chickenHead();
        if (strip.isValid())
        {
            Filmstrips::drawFrame(g, strip, 66, 66, 10, sliderPos,
                                  juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h));
            return;
        }
        drawChickenHead(g, centre, radius, angle);
    }
    else
    {
        // ── Dynamics knob filmstrip (96x96, 31 frames) ───────────────────────
        const auto& strip = Filmstrips::dynamics();
        if (strip.isValid())
        {
            Filmstrips::drawFrame(g, strip, 96, 96, 31, sliderPos,
                                  juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h));
            return;
        }
        if (variant == "dualLayerAluminum")
            drawDualLayerAluminum(g, bounds, angle, isActive);
        else
            drawModernAnalog(g, bounds, angle, isActive);
    }
}

void DynamicsLAF::drawModernAnalog(juce::Graphics& g, juce::Rectangle<float> bounds,
                                    float angle, bool isActive)
{
    auto centre = bounds.getCentre();
    float radius = bounds.getWidth() * 0.5f;

    // LRX-2: 3-layer AO shadow stack
    { juce::Path p; p.addEllipse(bounds); LRXHelper::drawAO(g, p, false); }

    // Radial dome: center #1A1A1A → rim #000000
    juce::ColourGradient grad(juce::Colour(0xff1A1A1A),
                               centre.x, centre.y - radius * 0.5f,
                               juce::Colour(0xff000000),
                               centre.x, centre.y + radius, true);
    g.setGradientFill(grad);
    g.fillEllipse(bounds);

    // Specular spot top-left: white 20% alpha, small ellipse
    float specR = radius * 0.25f;
    juce::ColourGradient spec(juce::Colours::white.withAlpha(0.20f),
                               centre.x - radius * 0.3f, centre.y - radius * 0.45f,
                               juce::Colours::transparentBlack,
                               centre.x, centre.y, false);
    g.setGradientFill(spec);
    g.fillEllipse(centre.x - radius * 0.55f, centre.y - radius * 0.6f,
                   specR * 2.f, specR * 1.5f);

    // LRX-4: Fresnel rim - cool silver (LA-2A style)
    LRXHelper::drawFresnelRim(g, bounds, juce::Colour(0xff99AABB), 1.4f);

    // White indicator line (thin, high-visibility)
    {
        juce::Graphics::ScopedSaveState ss(g);
        g.addTransform(juce::AffineTransform::rotation(angle, centre.x, centre.y));
        g.setColour(juce::Colours::white);
        g.drawLine(centre.x, centre.y - radius * 0.45f,
                   centre.x, centre.y - radius * 0.88f, 1.5f);
    }
}

void DynamicsLAF::drawDualLayerAluminum(juce::Graphics& g, juce::Rectangle<float> bounds,
                                         float angle, bool isActive)
{
    auto centre = bounds.getCentre();
    float radius = bounds.getWidth() * 0.5f;

    // LRX-2: 3-layer AO shadow stack
    { juce::Path p; p.addEllipse(bounds); LRXHelper::drawAO(g, p, false); }

    // Glossy black skirt
    juce::ColourGradient skirtGrad(juce::Colour(0xff2A2A2A),
                                    centre.x, centre.y - radius * 0.4f,
                                    juce::Colour(0xff000000),
                                    centre.x, centre.y + radius, true);
    g.setGradientFill(skirtGrad);
    g.fillEllipse(bounds);

    // Knurling - 40 lines
    for (int i = 0; i < 40; ++i)
    {
        float a = juce::MathConstants<float>::twoPi * i / 40;
        float innerR = radius * 0.875f, outerR = radius * 0.975f;
        g.setColour(juce::Colour(0xff3A3A3A));
        g.drawLine(centre.x + innerR * std::sin(a), centre.y - innerR * std::cos(a),
                   centre.x + outerR * std::sin(a), centre.y - outerR * std::cos(a), 0.6f);
    }

    // Indicator line on skirt (rotates)
    {
        juce::Graphics::ScopedSaveState ss(g);
        g.addTransform(juce::AffineTransform::rotation(angle, centre.x, centre.y));
        g.setColour(juce::Colours::white);
        g.drawLine(centre.x, centre.y - radius * 0.55f,
                   centre.x, centre.y - radius * 0.88f, 1.5f);
    }

    // Static brushed-silver cap
    float capR = radius * 0.52f;
    juce::Rectangle<float> capB(centre.x - capR, centre.y - capR, capR * 2, capR * 2);
    g.setColour(juce::Colour(0xffBCC6CC));
    g.fillEllipse(capB);
    for (int i = 0; i < 32; ++i)
    {
        float t  = (float)i / 32.f;
        float rr = capR * (0.08f + 0.88f * t);
        float alpha = 0.03f + 0.07f * std::abs(std::sin(t * juce::MathConstants<float>::pi * 4));
        g.setColour(juce::Colour(0xff000000).withAlpha(alpha));
        g.drawEllipse(centre.x - rr, centre.y - rr, rr * 2, rr * 2, 0.5f);
    }
    juce::ColourGradient capHL(juce::Colours::white.withAlpha(0.45f),
                                centre.x - capR * 0.3f, centre.y - capR * 0.7f,
                                juce::Colours::transparentBlack, centre.x, centre.y, false);
    g.setGradientFill(capHL);
    g.fillEllipse(capB);

    // LRX-3: Anisotropic highlights on brushed silver cap
    LRXHelper::drawAnisotropicHL(g, capB, 135.f);
}

void DynamicsLAF::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height).reduced(4.f);
    auto centre = bounds.getCentre();
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

    // Determine selected index (0-based) and total items
    int selectedId = box.getSelectedId();   // 1-based
    int numItems   = box.getNumItems();
    int idx        = juce::jmax(0, selectedId - 1);  // 0-based

    // Arc parameters: same as the drawRotarySlider arc
    float startAngle = juce::MathConstants<float>::pi * 1.25f;  // ~225 degrees (bottom-left)
    float endAngle   = juce::MathConstants<float>::pi * 2.75f;  // ~495 degrees (bottom-right)

    // Map selected index to angle
    float angle = (numItems > 1)
                  ? startAngle + (float)idx / (numItems - 1) * (endAngle - startAngle)
                  : (startAngle + endAngle) * 0.5f;

    // Draw the chicken head pointer at the computed angle
    drawChickenHead(g, centre, radius * 0.72f, angle);

    // Draw labels around the outside
    static const char* kneeLabels[] = { "H", "M", "V", "S", "H/R", "M/R", "V/R", "S/R" };
    float labelR = radius * 0.92f;
    g.setFont(juce::Font(7.5f, juce::Font::bold));

    for (int i = 0; i < numItems && i < 8; ++i)
    {
        float a  = startAngle + (float)i / (numItems - 1) * (endAngle - startAngle);
        float lx = centre.x + labelR * std::sin(a);
        float ly = centre.y - labelR * std::cos(a);

        bool isSelected = (i == idx);
        g.setColour(isSelected ? juce::Colour(0xffD1D1D1) : juce::Colour(0xff888888));
        g.drawText(kneeLabels[i],
                   juce::Rectangle<float>(lx - 12.f, ly - 6.f, 24.f, 12.f),
                   juce::Justification::centred, false);
    }
}

void DynamicsLAF::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    // Hide the text label - the chicken head shows the selection visually
    label.setBounds(0, 0, 0, 0);
}

void DynamicsLAF::drawChickenHead(juce::Graphics& g, juce::Point<float> centre,
                                   float radius, float angle)
{
    // 1. HEX-BOLT BASE (static)
    {
        juce::Path hex;
        float hexR = radius * 0.28f;
        for (int i = 0; i < 6; ++i)
        {
            float a = juce::MathConstants<float>::twoPi * i / 6.f
                      - juce::MathConstants<float>::pi / 6.f;
            auto p = juce::Point<float>(centre.x + hexR * std::cos(a),
                                         centre.y + hexR * std::sin(a));
            if (i == 0) hex.startNewSubPath(p); else hex.lineTo(p);
        }
        hex.closeSubPath();
        juce::ColourGradient hexGrad(juce::Colour(0xffCCCCCC), centre.x, centre.y - hexR,
                                      juce::Colour(0xff888888), centre.x, centre.y + hexR, false);
        g.setGradientFill(hexGrad);
        g.fillPath(hex);
        g.setColour(juce::Colour(0xff555555));
        g.strokePath(hex, juce::PathStrokeType(0.5f));
    }

    // 2. CHICKEN HEAD POINTER (rotates)
    {
        juce::Path pointer;
        pointer.startNewSubPath(centre.x, centre.y - radius * 0.92f);
        pointer.lineTo(centre.x - radius * 0.11f, centre.y - radius * 0.38f);
        pointer.lineTo(centre.x - radius * 0.24f, centre.y + radius * 0.12f);
        // Counterweight arc - manual lineTo keeps path continuous (addArc starts a new subpath,
        // leaving the right side disconnected and the fill missing on that side)
        {
            float cwR  = radius * 0.28f;
            float cwRy = cwR * 0.9f;                              // elliptical y-radius
            float arcCx = centre.x;
            float arcCy = centre.y + radius * 0.08f + cwRy;
            const int numSegs = 10;
            for (int seg = 0; seg <= numSegs; ++seg)
            {
                float t = (float)seg / numSegs;
                float a = juce::MathConstants<float>::pi * (1.0f - t); // pi→0: left→bottom→right
                pointer.lineTo(arcCx + cwR  * std::cos(a),
                               arcCy + cwRy * std::sin(a));
            }
        }
        pointer.lineTo(centre.x + radius * 0.11f, centre.y - radius * 0.38f);
        pointer.closeSubPath();
        pointer.applyTransform(juce::AffineTransform::rotation(angle, centre.x, centre.y));

        g.setColour(juce::Colour(0xff050505));
        g.fillPath(pointer);

        // Specular highlight on beak edge
        juce::Path beakEdge;
        beakEdge.startNewSubPath(centre.x, centre.y - radius * 0.92f);
        beakEdge.lineTo(centre.x - radius * 0.08f, centre.y - radius * 0.5f);
        beakEdge.applyTransform(juce::AffineTransform::rotation(angle, centre.x, centre.y));
        g.setColour(juce::Colour(0xff505050).withAlpha(0.45f));  // dark - readable on white LA-2A panel
        g.strokePath(beakEdge, juce::PathStrokeType(1.8f));

        // Dark indicator line on beak - near-black so it reads on cream/white panel
        juce::Path indicator;
        indicator.startNewSubPath(centre.x, centre.y - radius * 0.35f);
        indicator.lineTo(centre.x, centre.y - radius * 0.82f);
        indicator.applyTransform(juce::AffineTransform::rotation(angle, centre.x, centre.y));
        g.setColour(juce::Colour(0xff1A1A1A));
        g.strokePath(indicator, juce::PathStrokeType(1.5f));
    }
}

void DynamicsLAF::paintLA2APanel(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // LA-2A cream/white VU panel background
    g.setColour(juce::Colour(0xffF5F0E8));  // warm cream
    g.fillRect(bounds);

    // Subtle inner gradient (backlighting effect - lighter center)
    juce::ColourGradient innerGlow(
        juce::Colour(0xffFFFDF8), bounds.getCentreX(), bounds.getCentreY(),
        juce::Colour(0xffE8E2D8), bounds.getX(), bounds.getY(), true);
    g.setGradientFill(innerGlow);
    g.fillRect(bounds.reduced(0));

    // Very subtle panel grain texture (minimal - LA-2A is smooth, not rough)
    juce::Random rng(99);  // fixed seed
    for (int i = 0; i < bounds.getWidth() * bounds.getHeight() / 200; ++i)
    {
        float px = bounds.getX() + rng.nextFloat() * bounds.getWidth();
        float py = bounds.getY() + rng.nextFloat() * bounds.getHeight();
        float sz = rng.nextFloat() * 0.8f + 0.2f;
        float alpha = rng.nextFloat() * 0.06f + 0.01f;
        g.setColour(juce::Colours::black.withAlpha(alpha));
        g.fillEllipse(px, py, sz, sz);
    }

    // 1px dark border around panel edges (bezel)
    g.setColour(juce::Colour(0xff8A8070));
    g.drawRect(bounds, 1);

    // LRX-7: mounting screws - light ivory against cream panel
    LRXHelper::drawMountingScrews(g, bounds, 8, juce::Colour(0xffD8D0C0));
}

// ── EffectBypassLed (QA-ModelShell TS5) ───────────────────────────────────────
// Lifted verbatim out of SlotComponent::paint so the rack row and the panel
// window show the identical LED rather than a look-alike.
void EffectBypassLed::paint (juce::Graphics& g, juce::Rectangle<int> area, bool bypassed)
{
    g.setFont (juce::Font (16.0f));
    g.setColour (bypassed ? juce::Colour (0xffcc2222) : juce::Colour (0xff22cc44));
    g.drawText (juce::String::fromUTF8 ("\xe2\x97\x8f"),   // filled circle
                area, juce::Justification::centred);
}

// ── JewelIndicator ────────────────────────────────────────────────────────────
void JewelIndicator::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(2.f);
    auto centre = b.getCentre();
    float r = b.getWidth() * 0.5f;

    // PANEL BLEED GLOW - extends beyond component bounds when active
    if (isActive)
    {
        juce::DropShadow glow(juce::Colour(0xffCC0000).withAlpha(0.55f), 14, {0, 0});
        juce::Path p; p.addEllipse(b);
        glow.drawForPath(g, p);
    }

    // FACETED GLASS LENS - radial gradient
    juce::ColourGradient lens(
        isActive ? juce::Colour(0xffFF3333) : juce::Colour(0xff661111),
        centre.x, centre.y - r * 0.35f,
        juce::Colour(0xff220000), centre.x, centre.y + r, true);
    g.setGradientFill(lens);
    g.fillEllipse(b);

    // DIAMOND FACET LINES - 4 crossing lines for "cut glass" effect
    if (isActive)
    {
        g.setColour(juce::Colours::white.withAlpha(0.22f));
        for (int i = 0; i < 4; ++i)
        {
            float a = juce::MathConstants<float>::pi * i / 4.f;
            g.drawLine(centre.x + r * 0.62f * std::cos(a), centre.y + r * 0.62f * std::sin(a),
                       centre.x - r * 0.62f * std::cos(a), centre.y - r * 0.62f * std::sin(a), 0.6f);
        }
    }

    // SPECULAR SPOT - small bright reflection top-left
    juce::ColourGradient spec(juce::Colours::white.withAlpha(isActive ? 0.65f : 0.25f),
                               centre.x - r * 0.28f, centre.y - r * 0.35f,
                               juce::Colours::transparentBlack, centre.x, centre.y, false);
    g.setGradientFill(spec);
    g.fillEllipse(centre.x - r * 0.55f, centre.y - r * 0.55f, r, r);

    // DARK RIM
    g.setColour(juce::Colour(0xff110000));
    g.drawEllipse(b, 1.f);
}

// ── TimeLAF ───────────────────────────────────────────────────────────────────
// CL-299 (1): the additive-feedback warning ring, BOTH halves.  Opt-in per
// slider via `kWarnRingFrom` (normalized position where the warning zone
// starts; Delay's Feed knob runs 0..1.2, so 100 % sits at 0.833).
//
// The ring is a live METER, not a knob decoration (Jeff, 2026-08-05: "actually
// show you hitting red when it was causing the extreme clipping" -- the first
// build colored purely off the knob position and showed nothing about the
// audio).  Two layers:
//   * SETTING track (thin): where the knob is -- green through the safe range,
//     a dim outline through the over-unity zone, so the runaway RANGE stays
//     visible before anything sounds.
//   * LIVE arc (thick): the feedback level actually circulating, fed by the
//     owning panel's timer via `kWarnRingLive`.  Its lit head IS the current
//     level; the color runs green -> orange as the loop approaches unity and
//     red only when the shaper is genuinely clamping (the clipping Jeff
//     described).  Silence draws nothing.
void TimeLAF::drawWarnRing (juce::Graphics& g, juce::Rectangle<float> area,
                            float sliderPos, float warnFrom, float liveNorm,
                            float startAngle, float endAngle)
{
    const auto centre = area.getCentre();

    // The stroke is CENTRED on the radius, so the arc's outer edge sits half a
    // thickness beyond it.  Use the SMALLER dimension and leave room for the
    // fattest stroke this function draws (Jeff, 2026-08-05).  The kWarnRing*
    // calibration fits the ellipse to the knob face -- see their declaration.
    constexpr float kMaxStroke = 2.8f;
    const float baseR = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f
                          - (kMaxStroke * 0.5f) - 1.0f;
    const float rx = baseR * kWarnRingScaleX;
    const float ry = baseR * kWarnRingScaleY;
    if (rx <= 1.0f || ry <= 1.0f) return;
    const float rot = kWarnRingRotDeg * juce::MathConstants<float>::pi / 180.0f;

    const float valAngle = startAngle + sliderPos * (endAngle - startAngle);

    auto arcTo = [&] (float from, float to, juce::Colour c, float thickness)
    {
        if (to <= from) return;
        juce::Path p;
        // addCentredArc with startAsNewSubPath -- addArc's habit of starting a
        // new subpath is a documented trap for FILLED paths; this one is stroked.
        p.addCentredArc (centre.x, centre.y, rx, ry, rot, from, to, true);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    };

    // ── Setting track (thin) ─────────────────────────────────────────────────
    const float warnAngle = startAngle + juce::jlimit (0.0f, 1.0f, warnFrom) * (endAngle - startAngle);
    arcTo (startAngle, juce::jmin (valAngle, warnAngle),
           juce::Colour (0xff22cc44).withAlpha (0.55f), 1.4f);
    if (sliderPos > warnFrom)
        arcTo (warnAngle, valAngle, juce::Colour (0xffff9100).withAlpha (0.35f), 1.4f);

    // ── Live meter (thick) ───────────────────────────────────────────────────
    const float live = juce::jlimit (0.0f, 1.0f, liveNorm);
    if (live > 0.005f)
    {
        const float liveAngle = startAngle + live * (endAngle - startAngle);
        juce::Colour c;
        if (live < warnFrom)
        {
            // Approach: green heating toward orange as the loop nears unity.
            const float t = live / juce::jmax (0.001f, warnFrom);
            c = juce::Colour (0xff22cc44).interpolatedWith (juce::Colour (0xffff9100),
                                                            t * t);
        }
        else
        {
            // Past unity: the step-4 shaper is clamping -- this is the red.
            const float t = (live - warnFrom) / juce::jmax (0.001f, 1.0f - warnFrom);
            c = juce::Colour (0xffff9100).interpolatedWith (juce::Colour (0xffee2222), t);
        }
        arcTo (startAngle, liveAngle, c, 2.8f);
    }
}

void TimeLAF::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
    float sliderPos, float startAngle, float endAngle, juce::Slider& s)
{
    const bool hasWarnRing = s.getProperties().contains (kWarnRingFrom);
    const auto  fullArea   = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h);
    // Jeff, 2026-08-06: knob at FULL size; the ring fits the knob face via the
    // kWarnRing* ellipse calibration instead of shrinking the knob.
    const auto  knobArea   = fullArea;

    // ── Filmstrip render (64x64, 101 frames) ──────────────────────────────────
    {
        const auto& strip = Filmstrips::timeBased();
        if (strip.isValid())
        {
            Filmstrips::drawFrame(g, strip, 64, 64, 101, sliderPos, knobArea);
            if (hasWarnRing)
                drawWarnRing (g, fullArea,
                              sliderPos, (float) s.getProperties()[kWarnRingFrom],
                              (float) s.getProperties().getWithDefault (kWarnRingLive, 0.0f),
                              startAngle, endAngle);
            return;
        }
    }

    auto bounds = knobArea.reduced(3.f);
    auto centre = bounds.getCentre();
    float radius = bounds.getWidth() * 0.5f;
    float angle  = startAngle + sliderPos * (endAngle - startAngle);

    // LRX-2: 3-layer AO shadow stack
    { juce::Path p; p.addEllipse(bounds); LRXHelper::drawAO(g, p, false); }

    // MATTE BLACK CYLINDER BASE
    juce::ColourGradient base(juce::Colour(0xff383838), centre.x, centre.y - radius * 0.6f,
                               juce::Colour(0xff1A1A1A), centre.x, centre.y + radius, false);
    g.setGradientFill(base);
    g.fillEllipse(bounds);

    // 8-FLUTED STAR GRIP - rotates with knob
    {
        const int numFlutes = 8;
        float fluteOuter = radius * 0.70f;
        float fluteInner = radius * 0.52f;
        juce::Path star;
        for (int i = 0; i < numFlutes * 2; ++i)
        {
            float a = angle + juce::MathConstants<float>::twoPi * i / (numFlutes * 2);
            float fr = (i % 2 == 0) ? fluteOuter : fluteInner;
            auto p = juce::Point<float>(centre.x + fr * std::sin(a), centre.y - fr * std::cos(a));
            if (i == 0) star.startNewSubPath(p); else star.lineTo(p);
        }
        star.closeSubPath();
        g.setColour(juce::Colour(0xff2C2C2C));
        g.fillPath(star);
        g.setColour(juce::Colour(0xff484848));
        g.strokePath(star, juce::PathStrokeType(0.7f));
    }

    // INDICATOR LINE - on base wall between flute ring and outer edge
    {
        float innerR = radius * 0.76f, outerR = radius * 0.96f;
        g.setColour(juce::Colours::white);
        g.drawLine(centre.x + innerR * std::sin(angle),
                   centre.y - innerR * std::cos(angle),
                   centre.x + outerR * std::sin(angle),
                   centre.y - outerR * std::cos(angle), 2.5f);
    }

    if (hasWarnRing)
        drawWarnRing (g, fullArea,
                      sliderPos, (float) s.getProperties()[kWarnRingFrom],
                      (float) s.getProperties().getWithDefault (kWarnRingLive, 0.0f),
                      startAngle, endAngle);
}

void TimeLAF::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
    float sliderPos, float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    float leverW = w * 0.55f, leverH = h * 0.75f;
    float lx = x + (w - leverW) * 0.5f;
    float ly = y + (h - leverH) * sliderPos;

    // Drop shadow (heavy - Pultec style: light from upper-right)
    juce::DropShadow levShadow(juce::Colours::black.withAlpha(0.85f), 8, {-4, 5});
    juce::Path levPath;
    levPath.addRectangle(lx, ly, leverW, leverH);
    levShadow.drawForPath(g, levPath);

    // Black lever body
    g.setColour(juce::Colour(0xff1A1A1A));
    g.fillRect(lx, ly, leverW, leverH);

    // Silver metallic strip on top of lever
    juce::ColourGradient strip(juce::Colour(0xffD0D0D0), lx, ly,
                                juce::Colour(0xff909090), lx, ly + leverH * 0.15f, false);
    g.setGradientFill(strip);
    g.fillRect(lx, ly, leverW, leverH * 0.18f);

    // Lever edges: highlight left, shadow right
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawLine(lx, ly, lx, ly + leverH, 1.f);
}

// ── Shared helper: draw label with a forced text colour ──────────────────────
static void drawLabelForcedColour(juce::Graphics& g, juce::Label& label, juce::Colour col)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));
    if (!label.isBeingEdited())
    {
        float alpha = label.isEnabled() ? 1.0f : 0.5f;
        g.setColour(col.withAlpha(alpha));
        g.setFont(label.getFont());
        auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
        g.drawFittedText(label.getText(), textArea,
                         label.getJustificationType(),
                         juce::jmax(1, (int)(textArea.getHeight() / label.getFont().getHeight())),
                         label.getMinimumHorizontalScale());
    }
    g.setColour(label.findColour(juce::Label::outlineColourId).withAlpha(0.f));
    g.drawRect(label.getLocalBounds());
}

void TimeLAF::drawLabel(juce::Graphics& g, juce::Label& label)
{
    // Pultec Radio Gray panel - white text
    drawLabelForcedColour(g, label, juce::Colours::white);
}

void DynamicsLAF::drawLabel(juce::Graphics& g, juce::Label& label)
{
    // LA-2A cream/white panel - dark warm text (matches chicken head labels)
    drawLabelForcedColour(g, label, juce::Colour(0xff1a1208));
}

void HarmonicLAF::drawLabel(juce::Graphics& g, juce::Label& label)
{
    // Olive Hammerite dark panel - white text
    drawLabelForcedColour(g, label, juce::Colours::white);
}

void ModulationLAF::drawLabel(juce::Graphics& g, juce::Label& label)
{
    // Dark modulation panels - white text
    drawLabelForcedColour(g, label, juce::Colours::white);
}

void TimeLAF::paintPultecPanel(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Radio Gray / Air Force Blue base
    g.setColour(juce::Colour(0xff5B747E));
    g.fillRect(bounds);

    // Aged metal noise grain texture (fixed seed - does not change every repaint)
    juce::Random rng(73);
    int totalPix = bounds.getWidth() * bounds.getHeight();
    for (int i = 0; i < totalPix / 60; ++i)
    {
        float px = bounds.getX() + rng.nextFloat() * bounds.getWidth();
        float py = bounds.getY() + rng.nextFloat() * bounds.getHeight();
        float sz = rng.nextFloat() * 1.2f + 0.2f;
        float alpha = rng.nextFloat() * 0.12f + 0.03f;
        bool bright = rng.nextBool();
        g.setColour((bright ? juce::Colours::white : juce::Colours::black).withAlpha(alpha));
        g.fillEllipse(px, py, sz, sz);
    }

    // LRX-7: mounting screws - steel on Radio Gray panel
    LRXHelper::drawMountingScrews(g, bounds, 9, juce::Colour(0xff8AABB8));
}

// ── HarmonicLAF ───────────────────────────────────────────────────────────────
void HarmonicLAF::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
    float sliderPos, float startAngle, float endAngle, juce::Slider&)
{
    // ── Filmstrip render (128x128, 200 frames) ────────────────────────────────
    {
        const auto& strip = Filmstrips::harmonics();
        if (strip.isValid())
        {
            Filmstrips::drawFrame(g, strip, 128, 128, 200, sliderPos,
                                  juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h));
            return;
        }
    }

    auto bounds = juce::Rectangle<float>(x, y, w, h).reduced(3.f);
    auto centre = bounds.getCentre();
    float radius = bounds.getWidth() * 0.5f;
    float angle  = startAngle + sliderPos * (endAngle - startAngle);

    // LRX-2: 3-layer AO shadow stack
    { juce::Path body; body.addEllipse(bounds); LRXHelper::drawAO(g, body, true, juce::Colour(0xff553311)); }

    // Matte charcoal cylinder body
    juce::ColourGradient bodyGrad(juce::Colour(0xff2A2A2A), centre.x, centre.y - radius * 0.5f,
                                   juce::Colour(0xff0E0E0E), centre.x, centre.y + radius, false);
    g.setGradientFill(bodyGrad);
    g.fillEllipse(bounds);

    // Vertical groove fluting - 24 rectangles around perimeter (Bakelite style)
    {
        const int numGrooves = 24;
        for (int i = 0; i < numGrooves; ++i)
        {
            float a = juce::MathConstants<float>::twoPi * i / numGrooves;
            float grooveW = radius * 0.10f;
            float grooveH = radius * 0.28f;
            juce::Rectangle<float> groove(0.f, 0.f, grooveW, grooveH);
            groove = groove.withCentre(juce::Point<float>(
                centre.x + radius * 0.82f * std::sin(a),
                centre.y - radius * 0.82f * std::cos(a)));
            juce::Path gp; gp.addRectangle(groove);
            gp.applyTransform(juce::AffineTransform::rotation(a, centre.x, centre.y));
            g.setColour(juce::Colour(0xff0A0A0A));
            g.fillPath(gp);
        }
    }

    // Hollow triangular beak pointer - cream/gold stroke, transparent fill, rotates
    {
        float tipR = radius * 0.90f, baseHW = radius * 0.14f, baseR = radius * 0.40f;
        juce::Path beak;
        beak.startNewSubPath(centre.x, centre.y - tipR);
        beak.lineTo(centre.x - baseHW, centre.y - baseR);
        beak.lineTo(centre.x + baseHW, centre.y - baseR);
        beak.closeSubPath();
        beak.applyTransform(juce::AffineTransform::rotation(angle, centre.x, centre.y));

        g.setColour(juce::Colours::transparentBlack);
        g.fillPath(beak);
        g.setColour(juce::Colour(0xffC5B358));
        g.strokePath(beak, juce::PathStrokeType(1.8f));
    }

    // Brass center screw cap
    {
        float capR = radius * 0.14f;
        juce::ColourGradient brassGrad(juce::Colour(0xffE8C86E), centre.x, centre.y - capR,
                                        juce::Colour(0xff8B6914), centre.x, centre.y + capR, false);
        g.setGradientFill(brassGrad);
        g.fillEllipse(centre.x - capR, centre.y - capR, capR * 2, capR * 2);
        g.setColour(juce::Colour(0xff5A4500).withAlpha(0.8f));
        g.drawLine(centre.x - capR * 0.7f, centre.y, centre.x + capR * 0.7f, centre.y, 1.f);
    }

    // LRX-4: Fresnel rim - warm amber Bakelite
    LRXHelper::drawFresnelRim(g, bounds, juce::Colour(0xffCC8833), 1.3f);
}

void HarmonicLAF::paintHammeritePanel(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Olive Hammerite base
    g.setColour(juce::Colour(0xff4B4E3B));
    g.fillRect(bounds);

    // Noise texture: fixed seed so it doesn't change every repaint
    juce::Random rng(42);
    for (int i = 0; i < bounds.getWidth() * bounds.getHeight() / 80; ++i)
    {
        float px = bounds.getX() + rng.nextFloat() * bounds.getWidth();
        float py = bounds.getY() + rng.nextFloat() * bounds.getHeight();
        float sz = rng.nextFloat() * 1.5f + 0.3f;
        float alpha = rng.nextFloat() * 0.18f + 0.04f;
        g.setColour(juce::Colours::black.withAlpha(alpha));
        g.fillEllipse(px, py, sz, sz);
    }

    // LRX-7: mounting screws - dull brass on olive Hammerite panel
    LRXHelper::drawMountingScrews(g, bounds, 8, juce::Colour(0xff7A6A3A));
}

// ─────────────────────────────────────────────────────────────────────────────
// ColoredSectionLAF - colored horizontal line for ComboBox popup section headings
// ─────────────────────────────────────────────────────────────────────────────
ColoredSectionLAF& ColoredSectionLAF::get()
{
    static ColoredSectionLAF instance;
    return instance;
}

juce::String ColoredSectionLAF::encode(juce::Colour c, const juce::String& title)
{
    // Format: "\xc2\xa7#RRGGBB\xc2\xa7TITLE"  (sentinel byte is 0xA7 = section-sign '§')
    return juce::String(juce::CharPointer_UTF8("\xc2\xa7"))
         + "#" + c.toDisplayString(false)
         + juce::String(juce::CharPointer_UTF8("\xc2\xa7"))
         + title;
}

bool ColoredSectionLAF::decode(const juce::String& s,
                                juce::Colour& outColor,
                                juce::String& outTitle)
{
    const juce::String marker(juce::CharPointer_UTF8("\xc2\xa7"));
    if (!s.startsWith(marker)) return false;
    int first = marker.length();
    int second = s.indexOf(first, marker);
    if (second < 0) return false;
    juce::String hex = s.substring(first, second);        // "#RRGGBB"
    if (!hex.startsWithChar('#') || hex.length() < 7) return false;
    const auto rgb = (juce::uint32)hex.substring(1).getHexValue32();
    outColor = juce::Colour(0xff000000 | rgb);
    outTitle = s.substring(second + marker.length());
    return true;
}

juce::PopupMenu::Options ColoredSectionLAF::getOptionsForComboBoxPopupMenu(
    juce::ComboBox& box, juce::Label& label)
{
    // Anchor the popup to the combo box (so it opens from the box, not some
    // arbitrary screen corner) and let it grow up to 3 columns if the content
    // would otherwise overflow vertically. `withTargetComponent` keeps the
    // popup in the free-floating layer that isn't clipped by parent viewports,
    // which fixes the post-navigation truncation bug.
    return juce::LookAndFeel_V4::getOptionsForComboBoxPopupMenu(box, label)
               .withTargetComponent(&box)
               .withMinimumWidth(box.getWidth())
               .withStandardItemHeight(label.getHeight())
               .withMinimumNumColumns(1)
               .withMaximumNumColumns(3);
}

void ColoredSectionLAF::drawPopupMenuSectionHeader(juce::Graphics& g,
                                                    const juce::Rectangle<int>& area,
                                                    const juce::String& sectionName)
{
    juce::Colour  lineColor = juce::Colour(0xff808080);
    juce::String  title     = sectionName;
    decode(sectionName, lineColor, title);   // no-op if no prefix

    // Empty title → render as pure blank spacer (used between dropdown groups)
    if (title.isEmpty())
        return;

    auto b = area.reduced(8, 4).toFloat();

    // Colored glowing line (2 px with a soft halo)
    const float lineY = b.getCentreY();
    g.setColour(lineColor.withAlpha(0.25f));
    g.drawLine(b.getX(), lineY, b.getRight(), lineY, 5.f);
    g.setColour(lineColor);
    g.drawLine(b.getX(), lineY, b.getRight(), lineY, 2.f);

    // Title text above the line (if any), small caps, dim white
    if (title.isNotEmpty())
    {
        g.setColour(VC::Text.withAlpha(0.85f));
        g.setFont(juce::Font(10.f, juce::Font::bold));
        g.drawText(title.toUpperCase(),
                   area.reduced(12, 0).removeFromTop(area.getHeight() / 2),
                   juce::Justification::centredLeft, false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// GRMeter (SSL-style gain-reduction indicator)
// ─────────────────────────────────────────────────────────────────────────────
GRMeter::GRMeter()
{
    setTooltip("Gain reduction (dB)");
    startTimerHz(kTimerHz);
}

void GRMeter::setGainReduction(float grDb)
{
    mTargetDb.store(juce::jlimit(kMinDb, kMaxDb, grDb), std::memory_order_relaxed);
}

void GRMeter::timerCallback()
{
    const float target = mTargetDb.load(std::memory_order_relaxed);
    mDisplayDb += (target - mDisplayDb) * kSmoothAlpha;
    repaint();
}

void GRMeter::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Chrome bezel
    {
        juce::ColourGradient bezel(
            juce::Colour(0xffC0C0C0), b.getX(),      b.getY(),
            juce::Colour(0xff505050), b.getRight(),  b.getBottom(), false);
        bezel.addColour(0.5, juce::Colour(0xffA8A8A8));
        g.setGradientFill(bezel);
        g.fillRoundedRectangle(b, 5.f);
    }
    b = b.reduced(2.5f);

    // Cream plate (SSL-ish warm off-white)
    g.setColour(juce::Colour(0xffF2EEDC));
    g.fillRoundedRectangle(b, 4.f);
    juce::ColourGradient warm(juce::Colour(0xffFFFBEA), b.getCentreX(), b.getCentreY(),
                              juce::Colour(0xffE6E0C8), b.getX(), b.getY(), true);
    g.setGradientFill(warm);
    g.fillRoundedRectangle(b.reduced(2.f), 3.f);

    // Layout: bottom 18% reserved for "GR dB" label + numeric LCD
    const float lcdH     = b.getHeight() * 0.18f;
    const float arcAreaH = b.getHeight() - lcdH;
    const float pivotX   = b.getCentreX();
    const float pivotY   = b.getY() + arcAreaH + b.getHeight() * 0.02f;
    // Cap needleLen by available width so the needle tip + tick marks at
    // ±60° stay inside the plate. Horizontal extent at 60° = sin(60°)·len.
    // Tick outer radius = 0.97·len + a ~12 px label box. Use ~0.42·width as
    // the width-constrained upper bound so labels don't clip either edge.
    const float needleLen = juce::jmin(b.getHeight() * 0.70f,
                                        b.getWidth()  * 0.42f);

    // 120° arc. Needle rests at RIGHT (0 dB); swings LEFT as GR deepens.
    const float arcLeftAngle  = -juce::MathConstants<float>::pi * 0.333f;  // -60° (min dB)
    const float arcRightAngle =  juce::MathConstants<float>::pi * 0.333f;  // +60° (0 dB)

    auto dbToAngle = [&](float db) {
        float t = (db - kMinDb) / (kMaxDb - kMinDb);
        t = juce::jlimit(0.f, 1.f, t);
        return arcLeftAngle + t * (arcRightAngle - arcLeftAngle);
    };
    auto polarToXY = [&](float angle, float radius) {
        return juce::Point<float>{ pivotX + radius * std::sin(angle),
                                   pivotY - radius * std::cos(angle) };
    };

    // Red zone from -20..-15 (filled arc wedge outside tick ring)
    {
        const float innerR = needleLen * 0.82f;
        const float outerR = needleLen * 0.95f;
        const float a0 = dbToAngle(kMinDb);
        const float a1 = dbToAngle(kRedDb);
        juce::Path wedge;
        const int steps = 12;
        for (int i = 0; i <= steps; ++i) {
            float a = a0 + (a1 - a0) * (float)i / steps;
            auto  pt = polarToXY(a, outerR);
            if (i == 0) wedge.startNewSubPath(pt.x, pt.y);
            else        wedge.lineTo(pt.x, pt.y);
        }
        for (int i = steps; i >= 0; --i) {
            float a = a0 + (a1 - a0) * (float)i / steps;
            auto  pt = polarToXY(a, innerR);
            wedge.lineTo(pt.x, pt.y);
        }
        wedge.closeSubPath();
        g.setColour(juce::Colour(0xffFF2D2D));
        g.fillPath(wedge);
    }

    // Tick marks + labels (0 at right, -5, -10, -15, -20 going left)
    struct Mark { float db; const char* label; };
    static const Mark kMarks[] = {
        {   0.f,  "0"  },
        {  -5.f,  "5"  },
        { -10.f, "10" },
        { -15.f, "15" },
        { -20.f, "20" },
    };
    const float tickInner = needleLen * 0.86f;
    const float tickOuter = needleLen * 0.97f;
    const float labelR    = needleLen * 0.75f;

    g.setFont(juce::Font(9.f, juce::Font::bold));
    for (auto& m : kMarks) {
        float a  = dbToAngle(m.db);
        auto  p0 = polarToXY(a, tickInner);
        auto  p1 = polarToXY(a, tickOuter);
        g.setColour(juce::Colour(0xff202020));
        g.drawLine(p0.x, p0.y, p1.x, p1.y, 1.8f);

        auto  lp = polarToXY(a, labelR);
        g.setColour(m.db <= kRedDb ? juce::Colour(0xffCC0000) : juce::Colour(0xff202020));
        g.drawText(m.label, juce::Rectangle<float>(lp.x - 10.f, lp.y - 6.f, 20.f, 12.f),
                   juce::Justification::centred, false);
    }

    // Needle
    {
        const float a = dbToAngle(mDisplayDb);
        auto        tip  = polarToXY(a, needleLen * 0.92f);
        auto        base = juce::Point<float>(pivotX, pivotY);
        juce::Path needle;
        needle.startNewSubPath(base);
        needle.lineTo(tip);
        g.setColour(juce::Colour(0xffCC1A1A));
        g.strokePath(needle, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        // Needle center cap (brass-ish)
        g.setColour(juce::Colour(0xff8B6F3E));
        g.fillEllipse(pivotX - 3.5f, pivotY - 3.5f, 7.f, 7.f);
        g.setColour(juce::Colour(0xff3A2E1A));
        g.drawEllipse(pivotX - 3.5f, pivotY - 3.5f, 7.f, 7.f, 1.f);
    }

    // Bottom LCD: "GR dB" label + current reading
    {
        const float lcdTop = b.getBottom() - lcdH;
        const float labelH = lcdH * 0.30f;
        const float boxW   = b.getWidth() * 0.55f;
        const float boxH   = lcdH * 0.55f;
        const float boxX   = b.getCentreX() - boxW * 0.5f;
        const float boxY   = lcdTop + labelH + 1.f;

        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 5.5f, juce::Font::plain));
        g.setColour(juce::Colour(0xff555555));
        g.drawText("GR dB", juce::Rectangle<float>(b.getX(), lcdTop, b.getWidth(), labelH),
                   juce::Justification::centredBottom, false);

        g.setColour(juce::Colour(0xffD3D3D3));
        g.fillRoundedRectangle(boxX, boxY, boxW, boxH, 2.f);
        g.setColour(juce::Colour(0xff333333));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 7.f, juce::Font::plain));
        g.drawText(juce::String(mDisplayDb, 1),
                   juce::Rectangle<float>(boxX + 2.f, boxY + 1.f, boxW - 4.f, boxH - 2.f),
                   juce::Justification::centred, false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// GateGRMeter - see SharedUI.h (QA-Fe2 gate sibling of GRMeter)
// ─────────────────────────────────────────────────────────────────────────────
GateGRMeter::GateGRMeter()
{
    setTooltip("Gate attenuation (dB)");
    startTimerHz(kTimerHz);
}

void GateGRMeter::setGainReduction(float grDb)
{
    mTargetDb.store(juce::jlimit(kMinDb, kMaxDb, grDb), std::memory_order_relaxed);
}

void GateGRMeter::timerCallback()
{
    const float target = mTargetDb.load(std::memory_order_relaxed);
    mDisplayDb += (target - mDisplayDb) * kSmoothAlpha;
    repaint();
}

void GateGRMeter::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Chrome bezel (identical chassis to GRMeter)
    {
        juce::ColourGradient bezel(
            juce::Colour(0xffC0C0C0), b.getX(),      b.getY(),
            juce::Colour(0xff505050), b.getRight(),  b.getBottom(), false);
        bezel.addColour(0.5, juce::Colour(0xffA8A8A8));
        g.setGradientFill(bezel);
        g.fillRoundedRectangle(b, 5.f);
    }
    b = b.reduced(2.5f);

    g.setColour(juce::Colour(0xffF2EEDC));
    g.fillRoundedRectangle(b, 4.f);
    juce::ColourGradient warm(juce::Colour(0xffFFFBEA), b.getCentreX(), b.getCentreY(),
                              juce::Colour(0xffE6E0C8), b.getX(), b.getY(), true);
    g.setGradientFill(warm);
    g.fillRoundedRectangle(b.reduced(2.f), 3.f);

    const float lcdH     = b.getHeight() * 0.18f;
    const float arcAreaH = b.getHeight() - lcdH;
    const float pivotX   = b.getCentreX();
    const float pivotY   = b.getY() + arcAreaH + b.getHeight() * 0.02f;
    const float needleLen = juce::jmin(b.getHeight() * 0.70f,
                                        b.getWidth()  * 0.42f);

    const float arcLeftAngle  = -juce::MathConstants<float>::pi * 0.333f;
    const float arcRightAngle =  juce::MathConstants<float>::pi * 0.333f;

    auto dbToAngle = [&](float db) {
        float t = (db - kMinDb) / (kMaxDb - kMinDb);
        t = juce::jlimit(0.f, 1.f, t);
        return arcLeftAngle + t * (arcRightAngle - arcLeftAngle);
    };
    auto polarToXY = [&](float angle, float radius) {
        return juce::Point<float>{ pivotX + radius * std::sin(angle),
                                   pivotY - radius * std::cos(angle) };
    };

    // Red zone at the OPEN end (kRedDb..0): closed-is-normal for a gate, so
    // the warning band lives toward 0 (owner call, 2026-07-16).
    {
        const float innerR = needleLen * 0.82f;
        const float outerR = needleLen * 0.95f;
        const float a0 = dbToAngle(kRedDb);
        const float a1 = dbToAngle(kMaxDb);
        juce::Path wedge;
        const int steps = 12;
        for (int i = 0; i <= steps; ++i) {
            float a = a0 + (a1 - a0) * (float)i / steps;
            auto  pt = polarToXY(a, outerR);
            if (i == 0) wedge.startNewSubPath(pt.x, pt.y);
            else        wedge.lineTo(pt.x, pt.y);
        }
        for (int i = steps; i >= 0; --i) {
            float a = a0 + (a1 - a0) * (float)i / steps;
            auto  pt = polarToXY(a, innerR);
            wedge.lineTo(pt.x, pt.y);
        }
        wedge.closeSubPath();
        g.setColour(juce::Colour(0xffFF2D2D));
        g.fillPath(wedge);
    }

    // Tick marks + labels (0 at right, deepening left to -80)
    struct Mark { float db; const char* label; };
    static const Mark kMarks[] = {
        {   0.f,  "0"  },
        { -20.f, "20" },
        { -40.f, "40" },
        { -60.f, "60" },
        { -80.f, "80" },
    };
    const float tickInner = needleLen * 0.86f;
    const float tickOuter = needleLen * 0.97f;
    const float labelR    = needleLen * 0.75f;

    g.setFont(juce::Font(9.f, juce::Font::bold));
    for (auto& m : kMarks) {
        float a  = dbToAngle(m.db);
        auto  p0 = polarToXY(a, tickInner);
        auto  p1 = polarToXY(a, tickOuter);
        g.setColour(juce::Colour(0xff202020));
        g.drawLine(p0.x, p0.y, p1.x, p1.y, 1.8f);

        auto  lp = polarToXY(a, labelR);
        g.setColour(m.db >= kRedDb ? juce::Colour(0xffCC0000) : juce::Colour(0xff202020));
        g.drawText(m.label, juce::Rectangle<float>(lp.x - 10.f, lp.y - 6.f, 20.f, 12.f),
                   juce::Justification::centred, false);
    }

    // Needle + brass cap (identical to GRMeter)
    {
        const float a = dbToAngle(mDisplayDb);
        auto        tip  = polarToXY(a, needleLen * 0.92f);
        auto        base = juce::Point<float>(pivotX, pivotY);
        juce::Path needle;
        needle.startNewSubPath(base);
        needle.lineTo(tip);
        g.setColour(juce::Colour(0xffCC1A1A));
        g.strokePath(needle, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xff8B6F3E));
        g.fillEllipse(pivotX - 3.5f, pivotY - 3.5f, 7.f, 7.f);
        g.setColour(juce::Colour(0xff3A2E1A));
        g.drawEllipse(pivotX - 3.5f, pivotY - 3.5f, 7.f, 7.f, 1.f);
    }

    // Bottom LCD: "GATE dB" label + current reading
    {
        const float lcdTop = b.getBottom() - lcdH;
        const float labelH = lcdH * 0.30f;
        const float boxW   = b.getWidth() * 0.55f;
        const float boxH   = lcdH * 0.55f;
        const float boxX   = b.getCentreX() - boxW * 0.5f;
        const float boxY   = lcdTop + labelH + 1.f;

        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 5.5f, juce::Font::plain));
        g.setColour(juce::Colour(0xff555555));
        g.drawText("GATE dB", juce::Rectangle<float>(b.getX(), lcdTop, b.getWidth(), labelH),
                   juce::Justification::centredBottom, false);

        g.setColour(juce::Colour(0xffD3D3D3));
        g.fillRoundedRectangle(boxX, boxY, boxW, boxH, 2.f);
        g.setColour(juce::Colour(0xff333333));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 7.f, juce::Font::plain));
        g.drawText(juce::String(mDisplayDb, 1),
                   juce::Rectangle<float>(boxX + 2.f, boxY + 1.f, boxW - 4.f, boxH - 2.f),
                   juce::Justification::centred, false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// setSliderDoubleClickDefaultsFromApvts - see SharedUI.h
// ─────────────────────────────────────────────────────────────────────────────
void setSliderDoubleClickDefaultsFromApvts (juce::Component& root,
                                            juce::AudioProcessorValueTreeState& apvts)
{
    for (int i = 0; i < root.getNumChildComponents(); ++i)
    {
        auto* c = root.getChildComponent (i);
        if (c == nullptr) continue;

        if (auto* s = dynamic_cast<juce::Slider*> (c))
        {
            const auto pid = s->getProperties().getWithDefault ("apvtsId", juce::var()).toString();
            if (pid.isNotEmpty())
                if (auto* param = apvts.getParameter (pid))
                    s->setDoubleClickReturnValue (true,
                        (double) param->convertFrom0to1 (param->getDefaultValue()));
        }

        setSliderDoubleClickDefaultsFromApvts (*c, apvts);
    }
}

