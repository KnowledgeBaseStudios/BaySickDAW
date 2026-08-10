#include "SharedUI.h"
#include "UndoBracket.h"
#include "../ProjectManager.h"   // QA-RustyMeter Task 3: getSettingsFile (LUFS mode persistence)
#include "WindowChrome.h"        // TS7 §9.1: shared title-strip look
#include "BaySickTitleBar.h"     // QA-Layout T3: centered engine-name painter

// ── Filmstrip rendering ────────────────────────────────────────────────────────
namespace Filmstrips
{
    static juce::File getDir()
    {
        // exe is at: <root>/build/BaySickDAWStandalone_artefacts/<Config>/BaySickDAW.exe
        // go up 4 levels to reach project root, then into "Files For Claude/Filmstrips"
        return juce::File::getSpecialLocation(juce::File::currentApplicationFile)
            .getParentDirectory()   // Release
            .getParentDirectory()   // BaySickDAWStandalone_artefacts
            .getParentDirectory()   // build
            .getParentDirectory()   // project root
            .getChildFile("Files For Claude/Filmstrips");
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
            // EQ    dB range: -18 .. +18 (bipolar, matches EQ8DSP::setBandGain clamp).
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
            // fader inside ParametricEQDisplay::paint (see mGainReadoutR).
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
    // Locked call 5a applied to desktop windows too: close only.  Minimise and
    // maximise are returned as null so DocumentWindow simply has none -- a
    // maximised satellite covering the app is not a state this shell has a way
    // back out of.
    if (buttonType == juce::DocumentWindow::closeButton)
        return new ChromeCloseButton();
    return nullptr;
}

void BaySickLAF::positionDocumentWindowButtons (juce::DocumentWindow&,
                                             int titleBarX, int titleBarY,
                                             int titleBarW, int titleBarH,
                                             juce::Button* minimise, juce::Button* maximise,
                                             juce::Button* close,
                                             bool /*positionTitleBarButtonsOnLeft*/)
{
    // Square, right-aligned, inset by the same 4px the shell's close button uses.
    if (close != nullptr)
        close->setBounds (juce::Rectangle<int> (titleBarX + titleBarW - titleBarH,
                                                titleBarY, titleBarH, titleBarH)
                              .reduced (4));
    if (minimise != nullptr) minimise->setBounds ({});
    if (maximise != nullptr) maximise->setBounds ({});
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
    addAndMakeVisible(*mHamburgerBtn);

    // QA-Layout T10 (L13): second flat native-style heading -- the strip
    // reads "Menu  Add".  Hidden until a page installs an Add builder.
    mAddBtn = std::make_unique<TitleStripMenuItem>("Add");
    mAddBtn->setTooltip("Add strips and buses");
    mAddBtn->onClick = [this] { if (mAddMenuBuilder) mAddMenuBuilder (mAddBtn.get()); };
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

namespace
{
    // JUCE PopupMenu items carry no tooltip.  Freeze needs one when it is
    // LOCKED -- a greyed item with no explanation is a dead end (Jeff,
    // 2026-08-04: "if it is locked make it so if the user hovers over that
    // freeze option it says something about where to unlock that").  A custom
    // item component is the only hook JUCE gives us; TooltipClient on it is
    // what the tooltip window queries.
    class TooltipMenuItem : public juce::PopupMenu::CustomComponent,
                            public juce::TooltipClient
    {
    public:
        TooltipMenuItem (juce::String text, juce::String tip, bool enabled,
                         juce::Colour textColour)
            // TRUE: let the menu detect the click and invoke the item.  With
            // false the component has to trigger itself, and the item's action
            // would simply never fire.
            : juce::PopupMenu::CustomComponent (true),
              mText (std::move (text)), mTip (std::move (tip)),
              mEnabled (enabled), mColour (textColour)
        {
        }

        juce::String getTooltip() override { return mTip; }

        void getIdealSize (int& w, int& h) override
        {
            w = juce::Font (14.0f, juce::Font::plain).getStringWidth (mText) + 46;
            h = 22;
        }

        void paint (juce::Graphics& g) override
        {
            if (mEnabled && isItemHighlighted())
            {
                g.setColour (VC::Accent.withAlpha (0.30f));
                g.fillRect (getLocalBounds());
            }
            g.setColour (mEnabled ? mColour : mColour.withAlpha (0.38f));
            g.setFont (juce::Font (14.0f, juce::Font::plain));
            g.drawText (mText, getLocalBounds().withTrimmedLeft (12),
                        juce::Justification::centredLeft, true);
        }

    private:
        juce::String mText, mTip;
        bool         mEnabled;
        juce::Colour mColour;
    };
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
    if (mTitle.isNotEmpty() && mTabSlotBtns.empty() && mCenterName.isEmpty())
    {
        g.setColour(VC::TextDim.withAlpha(0.7f));
        g.setFont(juce::Font(10.f, juce::Font::bold));
        g.drawText(mTitle, getLocalBounds(), juce::Justification::centred, false);
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
    std::function<void()>                                                        sOnMidiSaveAsDefault;
    std::function<bool()>                                                        sHasAnyMidiMappings;

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

        // "Save MIDI mappings as global default" -- only if registry has any.
        if (sHasAnyMidiMappings && sHasAnyMidiMappings())
            m.addItem (firstId + 2, "Save MIDI mappings as global default");

        return firstId + 2;
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
        if (result == firstId + 2)
        {
            if (sOnMidiSaveAsDefault) sOnMidiSaveAsDefault();
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

// ============================================================ ParametricEQDisplay
#include "../DSP/EQ8DSP.h"
#include "../DSP/EQ8MsDSP.h"

static const float kEQFreqs[9]       = { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f };
static const float kEQDefaultFreqs[8] = { 40.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 12000.f };
static const float kEQGainLines[] = { -18.f, -12.f, -6.f, 0.f, 6.f, 12.f, 18.f };
static const juce::Colour kBandCols[8] = {
    juce::Colour(0xff8e44ad),  // Band 0: Violet
    juce::Colour(0xff6a1fab),  // Band 1: Indigo-violet
    juce::Colour(0xff2980b9),  // Band 2: Blue
    juce::Colour(0xfff1c40f),  // Band 3: Yellow/Gold
    juce::Colour(0xffe67e22),  // Band 4: Orange-gold
    juce::Colour(0xff27ae60),  // Band 5: Teal-green
    juce::Colour(0xff2ecc71),  // Band 6: Bright teal
    juce::Colour(0xff3498db),  // Band 7: Cyan/Blue
};

ParametricEQDisplay::ParametricEQDisplay()
    : mFFT (kFFTOrder)
{
    // 8 default band positions
    static const int kDefaultTypes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }; // all Bell - flat starting point
    for (int i = 0; i < kNumBands; ++i)
    {
        mBands[i].freq    = kEQDefaultFreqs[i];
        mBands[i].type    = kDefaultTypes[i];
        mBands[i].gainDb  = 0.f;
        mBands[i].q       = 0.707f;
        mBands[i].slope   = 0;
        mBands[i].enabled = true;
        mBands[i].muted   = false;
        mBands[i].soloed  = false;
        mSpareBands[i]    = mBands[i];
    }

    static const char* kTypeNamesShort[] = {
        "Bell", "LP", "HP", "LShelf", "HShelf", "Notch", "Off", "BPass", "Tilt" };

    for (int i = 0; i < kNumBands; ++i)
    {
        auto& c = mControls[i];

        // Type dropdown - select filter type from list
        c.typeCombo = std::make_unique<juce::ComboBox>();
        c.typeCombo->addItem("Bell",   1);
        c.typeCombo->addItem("LP",     2);
        c.typeCombo->addItem("HP",     3);
        c.typeCombo->addItem("LShelf", 4);
        c.typeCombo->addItem("HShelf", 5);
        c.typeCombo->addItem("Notch",  6);
        c.typeCombo->addItem("Off",    7);
        c.typeCombo->addItem("BPass",  8);
        c.typeCombo->addItem("Tilt",   9);
        c.typeCombo->setSelectedId(mBands[i].type + 1, juce::dontSendNotification);
        c.typeCombo->setTooltip("Band " + juce::String(i + 1) + " filter type: Bell, LP, HP, Low Shelf, High Shelf, Notch, Off, Band Pass, Tilt");
        c.typeCombo->onChange = [this, i] {
            if (mSyncing) return;
            const int newType = mControls[i].typeCombo->getSelectedId() - 1;
            mBands[i].type = newType;
            // 12j / Issue 5: zero the band's gain when switching to a type that
            // has no gain parameter (LP/HP/Notch/BP/Off). Otherwise params.gainDb
            // stays at its previous value (e.g. +6 from Peaking) and the fader
            // cap + graph curve both display the stale value.
            const bool nonGainBearing = (newType == 1 || newType == 2
                                      || newType == 5 || newType == 6 || newType == 7);
            if (nonGainBearing && mBands[i].gainDb != 0.f)
                mBands[i].gainDb = 0.f;
            // Refresh control states (fader enabled/disabled, slider positions,
            // type combo selection). Without this the gain fader's enabled state
            // doesn't refresh on type change via combo - only on mouseDrag -
            // which is why the fader stays locked after a swap back to Peaking.
            syncControlsFromBands();
            pushBandToDSP(i);
            beginParamUndoGesture(mBindMode == BindMode::MsDSP && mMsDSPApvts ? mMsDSPApvts->undoManager
                                      : mBindMode == BindMode::APVTS && mAPVTS ? mAPVTS->undoManager : nullptr,
                                  (mBindMode == BindMode::MsDSP ? (mShowMid ? mMsDSPMidPrefix : mMsDSPSidePrefix)
                                                                : "L" + juce::String(mLayerIdx) + "_eq")
                                      + juce::String(i) + "Freq"); // Task 6 (12-iv)
            setAPVTSFromBand(i);
            repaint();
        };
        addAndMakeVisible(*c.typeCombo);

        // Bipolar gain fader (vertical). Tagged as "eqFader" so BaySickLAF renders
        // the metallic mixer-style cap with -18..+18 tick labels, a live dB
        // position pointer, and a numeric readout next to the cap.
        // 2026-04-19: BaySickSlider base swallows right-click so it stays draggable-
        // only on left-click; right-click falls through to the Automate menu.
        c.gainFader = std::make_unique<BaySickSlider>(juce::Slider::LinearVertical,
                                                   juce::Slider::NoTextBox);
        c.gainFader->getProperties().set("eqFader", true);
        c.gainFader->setRange(-18.0, 18.0);
        c.gainFader->setValue(mBands[i].gainDb, juce::dontSendNotification);
        c.gainFader->setDoubleClickReturnValue(true, 0.0);
        c.gainFader->setTooltip("Band " + juce::String(i + 1) + " gain (dB) - double-click to reset to 0 dB");
        c.gainFader->onValueChange = [this, i] { if (!mSyncing) syncBandFromControl(i); };
        addAndMakeVisible(*c.gainFader);

        // Freq knob (rotary, log-mapped via double-click or drag)
        c.freqKnob = std::make_unique<BaySickSlider>(juce::Slider::RotaryVerticalDrag,
                                                  juce::Slider::NoTextBox);
        c.freqKnob->setRange(20.0, 20000.0, 1.0);
        c.freqKnob->setSkewFactorFromMidPoint(1000.0);
        c.freqKnob->setValue(mBands[i].freq, juce::dontSendNotification);
        c.freqKnob->setDoubleClickReturnValue(true, (double)kEQDefaultFreqs[i]);
        c.freqKnob->setTooltip("Band " + juce::String(i + 1) + " frequency (Hz) - drag up/down to adjust, double-click to reset");
        c.freqKnob->onValueChange = [this, i] { if (!mSyncing) syncBandFromControl(i); };
        addAndMakeVisible(*c.freqKnob);

        // Q knob (rotary)
        c.qKnob = std::make_unique<BaySickSlider>(juce::Slider::RotaryVerticalDrag,
                                               juce::Slider::NoTextBox);
        c.qKnob->setRange(0.1, 10.0, 0.01);
        c.qKnob->setSkewFactorFromMidPoint(1.0);
        c.qKnob->setValue(mBands[i].q, juce::dontSendNotification);
        c.qKnob->setDoubleClickReturnValue(true, 0.707);
        c.qKnob->setTooltip("Band " + juce::String(i + 1) + " Q / resonance - scroll wheel on graph handle also works, double-click to reset");
        c.qKnob->onValueChange = [this, i] { if (!mSyncing) syncBandFromControl(i); };
        addAndMakeVisible(*c.qKnob);

        // (Tip: click the colored dot at the top of each column to toggle band on/off)
    }

    // ── Toolbar buttons ────────────────────────────────────────────────────
    auto mkBtn = [&](const juce::String& lbl) {
        auto b = std::make_unique<juce::TextButton>(lbl);
        b->setColour(juce::TextButton::buttonColourId, VC::Surface);
        b->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffce3f8e));
        addAndMakeVisible(*b);
        return b;
    };

    // Options button - opens popup with compare/lock/overlays/mode
    mOptionsBtn = mkBtn("...");
    mOptionsBtn->setTooltip("EQ options - A/B compare, lock bands, heatmap/phase overlays, processing mode");
    mOptionsBtn->onClick = [this] { showEQOptionsMenu(mOptionsBtn.get()); };
    mOptionsBtn->setVisible(false);   // 2026-04-19: superseded by PageMenuBar hamburger

    // 2026-04-19: bank indicator. Lives unparented here; pages inject it into
    // PageMenuBar's extra-right slot via getBankIndicator() on EQ-tab activate.
    mBankIndicator = std::make_unique<BankIndicator>(*this);
    mBankIndicator->setTooltip("Current EQ bank - click to swap A <-> B (or use Page menu A/B Compare)");

    // D.4-Q6: main-level output fader (9th vertical fader on the right of the
    // band column area).  Bipolar, -18..+18 dB, double-click to reset to 0.
    mMainLevelFader = std::make_unique<BaySickSlider> (juce::Slider::LinearVertical,
                                                     juce::Slider::NoTextBox);
    mMainLevelFader->getProperties().set ("eqFader", true);
    mMainLevelFader->setRange (-18.0, 18.0);
    mMainLevelFader->setValue (0.0, juce::dontSendNotification);
    mMainLevelFader->setDoubleClickReturnValue (true, 0.0);
    mMainLevelFader->setTooltip ("EQ main output level (dB) - double-click to reset to 0");
    mMainLevelFader->onValueChange = [this]
    {
        const float v = (float) mMainLevelFader->getValue();
        if (mBindMode == BindMode::DSP && mBoundDSP)
            mBoundDSP->setMainLevel (v);
        else if (mBindMode == BindMode::MsDSP && mBoundMsDsp)
        {
            mBoundMsDsp->mid ().setMainLevel (v);
            mBoundMsDsp->side().setMainLevel (v);
        }
    };
    addAndMakeVisible (*mMainLevelFader);

    // 5F-9 sec.12 Phase 2 (12h): internal MID/SIDE pill removed - M/S view is now
    // driven exclusively by the page-header external MID/SIDE buttons that call
    // setShowMid() / isShowingMid() from outside. Keeping mMidSideBtn nullptr lets
    // existing null-guarded call sites (setShowMid, showMidSideToggle) no-op safely.

    // Zero heatmap buffer
    for (auto& frame : mHeatmapFrames) frame.fill(0.0f);

    // 12j follow-up Q1: shared inline readout editor. Initially hidden.
    mReadoutEditor = std::make_unique<juce::TextEditor>();
    mReadoutEditor->setJustification(juce::Justification::centred);
    mReadoutEditor->setBorder(juce::BorderSize<int>(0));
    mReadoutEditor->setFont(juce::Font(9.5f, juce::Font::bold));
    mReadoutEditor->setColour(juce::TextEditor::backgroundColourId, VC::Surface);
    mReadoutEditor->setColour(juce::TextEditor::textColourId,       VC::Text);
    mReadoutEditor->setColour(juce::TextEditor::outlineColourId,    VC::Highlight);
    mReadoutEditor->setColour(juce::TextEditor::focusedOutlineColourId, VC::Highlight);
    mReadoutEditor->setColour(juce::TextEditor::highlightColourId,  VC::Highlight.withAlpha(0.40f));
    mReadoutEditor->setInputRestrictions(12, "0123456789.+-kKhHzZqQ");
    mReadoutEditor->setVisible(false);
    // Commit on return; cancel on escape (JUCE default).
    mReadoutEditor->onReturnKey = [this] { commitReadoutEdit(); };
    mReadoutEditor->onEscapeKey = [this] { cancelReadoutEdit();  };
    mReadoutEditor->onFocusLost = [this] {
        // Only commit on genuine focus loss; cancelReadoutEdit also clears
        // focus but we check mReadoutEditKind first to avoid recursion.
        if (mReadoutEditKind != ReadoutEditKind::None) commitReadoutEdit();
    };
    addChildComponent(*mReadoutEditor);
}

void ParametricEQDisplay::setBand(int idx, const Band& b)
{
    if (idx < 0 || idx >= kNumBands) return;
    mBands[idx] = b;
    syncControlsFromBands();
    repaint();
}

ParametricEQDisplay::Band ParametricEQDisplay::getBand(int idx) const
{
    if (idx < 0 || idx >= kNumBands) return {};
    return mBands[idx];
}

float ParametricEQDisplay::freqToX(float hz) const
{
    if (mGraphArea.isEmpty()) return 0.f;
    float logMin = std::log10(20.f), logMax = std::log10(20000.f);
    float t = (std::log10(juce::jlimit(20.f, 20000.f, hz)) - logMin) / (logMax - logMin);
    return (float)mGraphArea.getX() + t * (float)mGraphArea.getWidth();
}

float ParametricEQDisplay::gainToY(float db) const
{
    if (mGraphArea.isEmpty()) return 0.f;
    float t = 1.f - (juce::jlimit(-18.f, 18.f, db) + 18.f) / 36.f;
    return (float)mGraphArea.getY() + t * (float)mGraphArea.getHeight();
}

float ParametricEQDisplay::xToFreq(float x) const
{
    if (mGraphArea.isEmpty() || mGraphArea.getWidth() == 0) return 1000.f;
    float t = (x - (float)mGraphArea.getX()) / (float)mGraphArea.getWidth();
    t = juce::jlimit(0.f, 1.f, t);
    float logMin = std::log10(20.f), logMax = std::log10(20000.f);
    return std::pow(10.f, logMin + t * (logMax - logMin));
}

float ParametricEQDisplay::yToGain(float y) const
{
    if (mGraphArea.isEmpty() || mGraphArea.getHeight() == 0) return 0.f;
    float t = 1.f - (y - (float)mGraphArea.getY()) / (float)mGraphArea.getHeight();
    return juce::jlimit(-18.f, 18.f, t * 36.f - 18.f);
}

float ParametricEQDisplay::evalBandDb(int idx, float hz, float gainOverride) const
{
    const auto& b = mBands[idx];
    if (!b.enabled || b.muted || b.type == 6) return 0.f;

    // Slope: each extra section multiplies the roll-off steepness
    const int numSections = [&] {
        static const int kCount[] = { 1, 2, 3, 4, 2, 3, 4 };
        return kCount[juce::jlimit(0, 6, b.slope)];
    }();

    // 12j: effective gain = design gain + current GR when dynamic (animated
    // curve). Ghost-outline calls pass a range-endpoint override explicitly.
    const float effGain = std::isnan(gainOverride)
                            ? (b.gainDb + (b.dynamic ? b.currentGrDb : 0.f))
                            : gainOverride;

    switch (b.type)
    {
        case 0: // Bell
        {
            float logRatio = std::log2(hz / b.freq);
            float bw = 1.f / juce::jmax(0.01f, b.q);
            float db = effGain * std::exp(-0.5f * (logRatio / bw) * (logRatio / bw));
            // Multi-section stacks same peak → narrower & same height (visual approx)
            if (numSections > 1)
                db *= std::exp(-0.4f * (numSections - 1) * (logRatio / bw) * (logRatio / bw));
            return db;
        }
        case 1: // Low Pass - biquad magnitude with Q (resonance at cutoff)
        {
            if (mSampleRateForFFT <= 0.0) return 0.f;
            float sr  = (float)mSampleRateForFFT;
            float w0  = 2.0f * juce::MathConstants<float>::pi * b.freq / sr;
            float cw  = std::cos(w0), sw = std::sin(w0);
            float alp = sw / (2.0f * b.q);
            float a0  = 1.f + alp;
            float nb0 = (1.f - cw) * 0.5f / a0;
            float nb1 = (1.f - cw)         / a0;
            float nb2 = (1.f - cw) * 0.5f / a0;
            float na1 = -2.f * cw          / a0;
            float na2 = (1.f - alp)        / a0;
            float om  = 2.0f * juce::MathConstants<float>::pi * hz / sr;
            float c1 = std::cos(om), s1 = std::sin(om);
            float c2 = std::cos(2.f * om), s2 = std::sin(2.f * om);
            float nRe = nb0 + nb1*c1 + nb2*c2,  nIm = -(nb1*s1 + nb2*s2);
            float dRe = 1.f + na1*c1 + na2*c2,  dIm = -(na1*s1 + na2*s2);
            float magSq = dRe*dRe + dIm*dIm;
            if (magSq < 1e-10f) return -60.f;
            float mag = std::sqrt((nRe*nRe + nIm*nIm) / magSq);
            float db  = juce::Decibels::gainToDecibels(juce::jmax(mag, 1e-6f));
            // Cascade numSections identical filters (approximate for display)
            return juce::jmax(-60.f, db * (float)numSections);
        }
        case 2: // High Pass - biquad magnitude with Q (resonance at cutoff)
        {
            if (mSampleRateForFFT <= 0.0) return 0.f;
            float sr  = (float)mSampleRateForFFT;
            float w0  = 2.0f * juce::MathConstants<float>::pi * b.freq / sr;
            float cw  = std::cos(w0), sw = std::sin(w0);
            float alp = sw / (2.0f * b.q);
            float a0  = 1.f + alp;
            float nb0 =  (1.f + cw) * 0.5f / a0;
            float nb1 = -(1.f + cw)         / a0;
            float nb2 =  (1.f + cw) * 0.5f / a0;
            float na1 = -2.f * cw          / a0;
            float na2 = (1.f - alp)        / a0;
            float om  = 2.0f * juce::MathConstants<float>::pi * hz / sr;
            float c1 = std::cos(om), s1 = std::sin(om);
            float c2 = std::cos(2.f * om), s2 = std::sin(2.f * om);
            float nRe = nb0 + nb1*c1 + nb2*c2,  nIm = -(nb1*s1 + nb2*s2);
            float dRe = 1.f + na1*c1 + na2*c2,  dIm = -(na1*s1 + na2*s2);
            float magSq = dRe*dRe + dIm*dIm;
            if (magSq < 1e-10f) return -60.f;
            float mag = std::sqrt((nRe*nRe + nIm*nIm) / magSq);
            float db  = juce::Decibels::gainToDecibels(juce::jmax(mag, 1e-6f));
            // Cascade numSections identical filters (approximate for display)
            return juce::jmax(-60.f, db * (float)numSections);
        }
        case 3: // Low Shelf - proper RBJ biquad magnitude (Q-dependent, smooth S-curve)
        {
            if (mSampleRateForFFT <= 0.0) return 0.f;
            const float sr    = (float) mSampleRateForFFT;
            const float w0    = 2.0f * juce::MathConstants<float>::pi * b.freq / sr;
            const float cw    = std::cos(w0), sw = std::sin(w0);
            const float A     = std::pow(10.0f, effGain * (1.0f / 40.0f));
            const float alpha = sw / (2.0f * juce::jmax(0.01f, b.q));
            const float twoSqrtAalpha = 2.0f * std::sqrt(A) * alpha;
            // RBJ low-shelf coefficients (pre-normalised by a0).
            const float a0 =        (A + 1.0f) + (A - 1.0f) * cw + twoSqrtAalpha;
            const float b0 =    A * ((A + 1.0f) - (A - 1.0f) * cw + twoSqrtAalpha);
            const float b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cw);
            const float b2 =    A * ((A + 1.0f) - (A - 1.0f) * cw - twoSqrtAalpha);
            const float a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cw);
            const float a2 =        (A + 1.0f) + (A - 1.0f) * cw - twoSqrtAalpha;
            const float nb0 = b0 / a0, nb1 = b1 / a0, nb2 = b2 / a0;
            const float na1 = a1 / a0, na2 = a2 / a0;
            const float om = 2.0f * juce::MathConstants<float>::pi * hz / sr;
            const float c1 = std::cos(om), s1 = std::sin(om);
            const float c2 = std::cos(2.0f * om), s2 = std::sin(2.0f * om);
            const float nRe = nb0 + nb1 * c1 + nb2 * c2, nIm = -(nb1 * s1 + nb2 * s2);
            const float dRe = 1.0f + na1 * c1 + na2 * c2, dIm = -(na1 * s1 + na2 * s2);
            const float magSq = dRe * dRe + dIm * dIm;
            if (magSq < 1.0e-10f) return -60.0f;
            const float mag = std::sqrt((nRe * nRe + nIm * nIm) / magSq);
            return juce::Decibels::gainToDecibels(juce::jmax(mag, 1.0e-6f));
        }
        case 4: // High Shelf - proper RBJ biquad magnitude (Q-dependent, smooth S-curve)
        {
            if (mSampleRateForFFT <= 0.0) return 0.f;
            const float sr    = (float) mSampleRateForFFT;
            const float w0    = 2.0f * juce::MathConstants<float>::pi * b.freq / sr;
            const float cw    = std::cos(w0), sw = std::sin(w0);
            const float A     = std::pow(10.0f, effGain * (1.0f / 40.0f));
            const float alpha = sw / (2.0f * juce::jmax(0.01f, b.q));
            const float twoSqrtAalpha = 2.0f * std::sqrt(A) * alpha;
            // RBJ high-shelf coefficients.
            const float a0 =        (A + 1.0f) - (A - 1.0f) * cw + twoSqrtAalpha;
            const float b0 =    A * ((A + 1.0f) + (A - 1.0f) * cw + twoSqrtAalpha);
            const float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cw);
            const float b2 =    A * ((A + 1.0f) + (A - 1.0f) * cw - twoSqrtAalpha);
            const float a1 =  2.0f * ((A - 1.0f) - (A + 1.0f) * cw);
            const float a2 =        (A + 1.0f) - (A - 1.0f) * cw - twoSqrtAalpha;
            const float nb0 = b0 / a0, nb1 = b1 / a0, nb2 = b2 / a0;
            const float na1 = a1 / a0, na2 = a2 / a0;
            const float om = 2.0f * juce::MathConstants<float>::pi * hz / sr;
            const float c1 = std::cos(om), s1 = std::sin(om);
            const float c2 = std::cos(2.0f * om), s2 = std::sin(2.0f * om);
            const float nRe = nb0 + nb1 * c1 + nb2 * c2, nIm = -(nb1 * s1 + nb2 * s2);
            const float dRe = 1.0f + na1 * c1 + na2 * c2, dIm = -(na1 * s1 + na2 * s2);
            const float magSq = dRe * dRe + dIm * dIm;
            if (magSq < 1.0e-10f) return -60.0f;
            const float mag = std::sqrt((nRe * nRe + nIm * nIm) / magSq);
            return juce::Decibels::gainToDecibels(juce::jmax(mag, 1.0e-6f));
        }
        case 5: // Notch
        {
            float logRatio = std::log2(hz / b.freq);
            float bw = 1.f / juce::jmax(0.01f, b.q);
            return -12.f * std::exp(-0.5f * (logRatio / bw) * (logRatio / bw));
        }
        case 6: // OFF
            return 0.f;
        case 7: // Band Pass - bell-shaped peak (positive dB display)
        {
            float logRatio = std::log2(hz / b.freq);
            float bw = 1.f / juce::jmax(0.01f, b.q);
            return 6.f * std::exp(-0.5f * (logRatio / bw) * (logRatio / bw));
        }
        case 8: // Tilt - low shelf (+gainDb) + high shelf (-gainDb) at same pivot freq
        {
            // 12j: uses effective gain (dynamic-aware when dynamic=true).
            float g = effGain;
            float A_lo = juce::Decibels::decibelsToGain(g * 0.5f);
            float A_hi = juce::Decibels::decibelsToGain(-g * 0.5f);
            juce::ignoreUnused(A_lo, A_hi);
            float ratio = hz / juce::jmax(1.f, b.freq);
            float loDb = g  / (1.f + ratio * ratio);      // 6 dB/oct approximation
            float hiDb = -g / (1.f + 1.f / juce::jmax(1e-6f, ratio * ratio));
            return loDb + hiDb;
        }
        default: return 0.f;
    }
}

void ParametricEQDisplay::drawGrid(juce::Graphics& g) const
{
    auto& a = mGraphArea;

    // Background
    g.setColour(VC::EQGridBg);
    g.fillRect(a);

    // Horizontal dB lines
    for (float db : kEQGainLines)
    {
        float y = gainToY(db);
        g.setColour(db == 0.f ? VC::EQGridLine.withAlpha(0.9f) : VC::EQGridLine.withAlpha(0.6f));
        g.drawHorizontalLine((int)y, (float)a.getX(), (float)a.getRight());
        if (db == 0.f || std::abs(db) == 12.f || std::abs(db) == 18.f)
        {
            g.setColour(VC::TextDim.withAlpha(0.7f));
            g.setFont(juce::Font(8));
            g.drawText(juce::String((int)db) + " dB",
                       a.getX() + 2, (int)y - 9, 32, 10,
                       juce::Justification::centredLeft, false);
        }
    }

    // Vertical frequency lines
    for (float f : kEQFreqs)
    {
        float x = freqToX(f);
        g.setColour(VC::EQGridLine.withAlpha(0.7f));
        g.drawVerticalLine((int)x, (float)a.getY(), (float)a.getBottom());
        g.setColour(VC::TextDim.withAlpha(0.6f));
        g.setFont(juce::Font(8));
        juce::String label = (f >= 1000.f) ? juce::String((int)(f / 1000.f)) + "k"
                                            : juce::String((int)f);
        g.drawText(label, (int)x - 12, a.getBottom() - 12, 24, 11,
                   juce::Justification::centred, false);
    }
}

void ParametricEQDisplay::drawCurve(juce::Graphics& g) const
{
    auto& a = mGraphArea;
    int w = a.getWidth();
    if (w <= 0) return;

    juce::Path curvePath;
    float zeroY = gainToY(0.f);

    for (int px = 0; px < w; ++px)
    {
        float hz  = xToFreq((float)(a.getX() + px));
        float sum = 0.f;
        for (int b = 0; b < kNumBands; ++b)
            sum += evalBandDb(b, hz);
        sum = juce::jlimit(-18.f, 18.f, sum);
        float y = gainToY(sum);
        if (px == 0) curvePath.startNewSubPath((float)(a.getX() + px), y);
        else         curvePath.lineTo((float)(a.getX() + px), y);
    }

    // Fill below curve with solid white at 15% alpha
    juce::Path fillPath = curvePath;
    fillPath.lineTo((float)a.getRight(), zeroY);
    fillPath.lineTo((float)a.getX(), zeroY);
    fillPath.closeSubPath();
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.fillPath(fillPath);

    // 12j: ghost range outline for dynamic bands. Second curve showing the
    // range-endpoint effective gain (design + signed range). Drawn BEHIND the
    // live animated curve as a faint dashed outline so users see "how far this
    // band CAN move" alongside where it IS right now.
    bool anyDynamic = false;
    for (const auto& b : mBands)
        if (b.enabled && !b.muted && b.dynamic
            && (b.type == 0 || b.type == 3 || b.type == 4 || b.type == 8))
        { anyDynamic = true; break; }

    if (anyDynamic)
    {
        juce::Path ghostPath;
        for (int px = 0; px < w; ++px)
        {
            float hz  = xToFreq((float)(a.getX() + px));
            float sum = 0.f;
            for (int b = 0; b < kNumBands; ++b)
            {
                const auto& band = mBands[b];
                if (band.dynamic
                    && (band.type == 0 || band.type == 3
                     || band.type == 4 || band.type == 8))
                {
                    // 12j follow-up Q2: signed rangeDb directly encodes direction
                    // + amount. endpoint = designGain + rangeDb (positive = expand
                    // upward, negative = compress downward).
                    const float endpoint = band.gainDb + band.rangeDb;
                    sum += evalBandDb(b, hz, endpoint);
                }
                else
                {
                    sum += evalBandDb(b, hz);
                }
            }
            sum = juce::jlimit(-18.f, 18.f, sum);
            float y = gainToY(sum);
            if (px == 0) ghostPath.startNewSubPath((float)(a.getX() + px), y);
            else         ghostPath.lineTo((float)(a.getX() + px), y);
        }
        g.setColour(juce::Colour(0xffff7f33).withAlpha(0.35f));
        juce::PathStrokeType dashed(1.0f);
        float dashLen[] = { 4.0f, 3.0f };
        juce::Path dashedPath;
        dashed.createDashedStroke(dashedPath, ghostPath, dashLen, 2);
        g.strokePath(dashedPath, juce::PathStrokeType(1.0f));
    }

    // Curve line (white) - live effective-gain curve (animated for dynamic bands)
    g.setColour(juce::Colours::white);
    g.strokePath(curvePath, juce::PathStrokeType(1.5f));
}

void ParametricEQDisplay::drawHandles(juce::Graphics& g) const
{
    // Check if any band is soloed
    bool anySolo = false;
    for (int i = 0; i < kNumBands; ++i)
        if (mBands[i].soloed && mBands[i].enabled) { anySolo = true; break; }

    // Session B / Option C: compute this side's default channel. Bands whose
    // channel matches the default are unchanged and get no badge (reduces visual
    // clutter); bands re-routed away from the default get a single-letter badge
    // indicating their actual routing.
    //   MsDSP mid-view  -> default = Mid
    //   MsDSP side-view -> default = Side
    //   DSP (bare)      -> default = Stereo
    const int defaultChan = (mBindMode == BindMode::MsDSP)
                                ? (mShowMid ? 1 /*Mid*/ : 2 /*Side*/)
                                : 0 /*Stereo*/;
    static const char* kChanBadge[5] = { "St", "M", "S", "L", "R" };

    for (int i = 0; i < kNumBands; ++i)
    {
        const auto& b = mBands[i];
        if (!b.enabled) continue;

        const bool dimmed = b.muted || (anySolo && !b.soloed);
        float x = freqToX(b.freq);
        // For types with no gain, anchor at 0dB line
        float y = (b.type == 1 || b.type == 2 || b.type == 5
                   || b.type == 6 || b.type == 7)
                    ? gainToY(0.f) : gainToY(b.gainDb);
        juce::Colour col = kBandCols[i];
        float alpha = dimmed ? 0.35f : 1.0f;

        // LRX-14: Color bleed glow for active/hovered bands
        if (i == mHoveredBand || i == mDragBand)
        {
            juce::DropShadow bleed(col.withAlpha(0.18f), 8, {0, 0});
            juce::Path bleedPath; bleedPath.addEllipse(x - 9.f, y - 9.f, 18.f, 18.f);
            bleed.drawForPath(g, bleedPath);
        }

        // LRX-14: Contact shadow (Layer 1 only - tight, offset {0,1})
        {
            juce::DropShadow contactShadow(juce::Colours::black.withAlpha(0.40f), 2, {0, 1});
            juce::Path shadowPath; shadowPath.addEllipse(x - 6.f, y - 6.f, 12.f, 12.f);
            contactShadow.drawForPath(g, shadowPath);
        }

        // Outer glow
        g.setColour(col.withAlpha(0.20f * alpha));
        g.fillEllipse(x - 9.f, y - 9.f, 18.f, 18.f);
        // Fill
        g.setColour(col.withAlpha(alpha));
        g.fillEllipse(x - 6.f, y - 6.f, 12.f, 12.f);
        // Border: red=muted, yellow=soloed, white=normal
        juce::Colour borderCol = b.muted  ? juce::Colour(0xffcc2222) :
                                 b.soloed ? juce::Colour(0xffffcc00) :
                                            juce::Colours::white.withAlpha(0.6f);
        g.setColour(borderCol);
        g.drawEllipse(x - 6.f, y - 6.f, 12.f, 12.f, 1.2f);
        // LRX-14: Fresnel rim - bright white arc at top-left edge (simplified)
        g.setColour(juce::Colours::white.withAlpha(0.15f * alpha));
        g.drawEllipse(x - 5.5f, y - 5.5f, 11.f, 11.f, 0.7f);
        // Band number
        g.setColour(juce::Colours::white.withAlpha(alpha));
        g.setFont(juce::Font(8.f, juce::Font::bold));
        g.drawText(juce::String(i + 1), (int)x - 5, (int)y - 5, 10, 10,
                   juce::Justification::centred, false);

        // M (mute) and S (solo) chips below handle
        float chipY = y + 9.f;
        if (chipY + 10.f > (float)mGraphArea.getBottom())
            chipY = y - 19.f;
        juce::Colour mCol = b.muted  ? juce::Colour(0xffcc2222) : VC::Accent;
        juce::Colour sCol = b.soloed ? juce::Colour(0xffffcc00) : VC::Accent;
        g.setColour(mCol.withAlpha(0.85f));
        g.fillRoundedRectangle(x - 10.f, chipY, 9.f, 8.f, 2.f);
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.setFont(juce::Font(7.f, juce::Font::bold));
        g.drawText("M", (int)(x - 10.f), (int)chipY, 9, 8, juce::Justification::centred, false);
        g.setColour(sCol.withAlpha(0.85f));
        g.fillRoundedRectangle(x + 1.f, chipY, 9.f, 8.f, 2.f);
        g.setColour(juce::Colours::black.withAlpha(0.9f));
        g.drawText("S", (int)(x + 1.f), (int)chipY, 9, 8, juce::Justification::centred, false);

        // 12j Issue 3: handle-adjacent GR bar removed - GR now lives in the
        // rich hover tooltip's third column as a full-size graphical meter.
        // Keeps the handle region clutter-free.

        // Session B / Option C: channel-routing badge. Only shown when this
        // band is re-routed away from the side's default (reduces clutter;
        // unchanged bands stay clean). Placed top-right of the handle in a
        // small tinted chip with the routing letter (M / S / L / R / St).
        if (b.channel != defaultChan)
        {
            const int chanIdx = juce::jlimit(0, 4, b.channel);
            const juce::String letter = juce::String(kChanBadge[chanIdx]);
            // Chip at top-right of handle, offset so it doesn't overlap the dot.
            const float badgeW = 12.f;
            const float badgeH = 10.f;
            const float bx = x + 5.f;
            const float by = y - 11.f;
            juce::Colour badgeBg = juce::Colour(0xffff9c33).withAlpha(0.90f * alpha);  // amber = re-routed
            g.setColour(badgeBg);
            g.fillRoundedRectangle(bx, by, badgeW, badgeH, 2.f);
            g.setColour(juce::Colours::black.withAlpha(0.92f * alpha));
            g.drawRoundedRectangle(bx + 0.5f, by + 0.5f, badgeW - 1.f, badgeH - 1.f, 2.f, 0.6f);
            g.setColour(juce::Colours::black.withAlpha(alpha));
            g.setFont(juce::Font(7.5f, juce::Font::bold));
            g.drawText(letter, (int) bx, (int) by, (int) badgeW, (int) badgeH,
                       juce::Justification::centred, false);
        }

        // 12j Issue 3: small in-graph Q + slope hover tip removed - all that
        // info now lives in the rich 3-column hover tooltip drawHoverTooltip()
        // renders from paint() when any band is hovered.
    }
}

void ParametricEQDisplay::syncControlsFromBands()
{
    static const char* kTypeNamesShort[] = {
        "Bell", "LP", "HP", "LShelf", "HShelf", "Notch", "Off", "BPass", "Tilt" };

    mSyncing = true;
    for (int i = 0; i < kNumBands; ++i)
    {
        const auto& b = mBands[i];
        mControls[i].typeCombo ->setSelectedId(juce::jlimit(0, 7, b.type) + 1, juce::dontSendNotification);
        mControls[i].gainFader ->setValue(b.gainDb, juce::dontSendNotification);
        mControls[i].freqKnob  ->setValue(b.freq,   juce::dontSendNotification);
        mControls[i].qKnob     ->setValue(b.q,      juce::dontSendNotification);
        // 12j bundled UX polish: grey out the gain fader for band types that
        // have no gain parameter (LP=1, HP=2, Notch=5, BP=7, Off=6). For these
        // types the DSP setter already short-circuits setBandGain, so the fader
        // would previously snap back to 0 on drag. Disabling the control makes
        // that behaviour explicit + prevents the misleading drag-and-snap UX.
        const bool gainActive = (b.type == 0 || b.type == 3 || b.type == 4 || b.type == 8);
        mControls[i].gainFader ->setEnabled(gainActive);
    }
    mSyncing = false;
}

void ParametricEQDisplay::syncBandFromControl(int idx)
{
    auto& b = mBands[idx];
    auto& c = mControls[idx];
    b.gainDb  = (float)c.gainFader->getValue();
    b.freq    = juce::jlimit(20.f, 20000.f, (float)c.freqKnob->getValue());
    b.q       = juce::jlimit(0.1f, 10.f,    (float)c.qKnob->getValue());
    // b.type is updated directly via typeCombo onChange
    setAPVTSFromBand(idx);
    // Match the graph-handle-drag contract: write directly to DSP immediately so
    // the next syncFromDSP tick (at ~30 Hz) reads the user's value back, not the
    // stale default the DSP still holds until processBlock runs updateXxxEQ.
    // Without this, the timer fires between the slider change and the next audio
    // block, reads the stale default, and snaps the slider back.
    pushBandToDSP(idx);
    repaint();
}

void ParametricEQDisplay::resized()
{
    auto b = getLocalBounds().reduced(2);
    // 2026-04-19: 22-px toolbar row removed - "..." button migrated to the
    // PageMenuBar hamburger and [SPARE] indicator migrated to the BankIndicator
    // component injected into PageMenuBar's right slot. Keep mToolbarArea as
    // an empty rect so any leftover paint references no-op cleanly.
    mToolbarArea = juce::Rectangle<int>();

    // Right panel: 8 color-coded columns beside the graph - target ~25% of total width
    // Each column needs room for: type dropdown + gain fader + freq knob + q knob
    // D.4-Q6: 9th column added on the far right for the EQ main-level fader.
    const int kColW        = juce::jmax(22, (b.getWidth() / 32));
    const int rightPanelW  = kColW * (kNumBands + 1);   // +1 for main fader column
    mRightPanelArea = b.removeFromRight(rightPanelW);
    mGraphArea      = b;

    // ── Right panel: lay out per-band columns ──────────────────────────────
    // From top to bottom in each column:
    //   [color dot - painted, not a component]
    //   enable toggle  16px
    //   type button    22px
    //   gain fader     fills center (~60% of remaining)
    //   freq knob      kColW (square)
    //   q knob         kColW (square)
    if (mRightPanelArea.getWidth() <= 0) return;
    auto panel = mRightPanelArea.reduced(1);
    panel.removeFromTop(10);   // space for color dot header painted in paint()

    // 12i / EQ polish: 10px readout strip reserved below each fader + knob.
    constexpr int kReadoutH = 10;

    for (int i = 0; i < kNumBands; ++i)
    {
        auto col = panel.removeFromLeft(kColW).reduced(1, 0);
        auto& c  = mControls[i];

        // Knobs at bottom: split each square into readout (bottom) + rotary (top).
        auto qBlock    = col.removeFromBottom(kColW);
        mQReadoutR[i]  = qBlock.removeFromBottom(kReadoutH);
        auto qR        = qBlock.reduced(2);

        auto freqBlock    = col.removeFromBottom(kColW);
        mFreqReadoutR[i]  = freqBlock.removeFromBottom(kReadoutH);
        auto freqR        = freqBlock.reduced(2);

        // Type button at top (just below color dot in the 10px header space)
        auto typeR   = col.removeFromTop(22).reduced(1);
        // Gain readout strip sits below the fader. Then translated UP 5 px into
        // the visually-empty region between the fader's last drawn tick label
        // and the readout's nominal position, so the readout breathes away from
        // the freq knob below it. No component resizing - the fader's bounds
        // are unchanged; we're just moving the text-paint rectangle up into
        // space the fader body never paints to.
        mGainReadoutR[i] = col.removeFromBottom(kReadoutH).translated(0, -5);
        // Gain fader takes remaining middle space
        auto faderR  = col.reduced(3, 2);

        if (c.typeCombo)  c.typeCombo ->setBounds(typeR);
        if (c.gainFader && faderR.getHeight() > 8)
                          c.gainFader ->setBounds(faderR);
        if (c.freqKnob)   c.freqKnob  ->setBounds(freqR);
        if (c.qKnob)      c.qKnob     ->setBounds(qR);
    }

    // D.4-Q6: 9th "Main" column - fader spans the same vertical real estate as
    // a band's gain fader, but with no type combo / freq / Q knobs.  The col
    // matches band column geometry so headers and tick rows line up.
    {
        auto col = panel.removeFromLeft(kColW).reduced(1, 0);
        // Skip the bottom Q + freq blocks so the fader has the same vertical
        // bounds as the band gain faders.
        col.removeFromBottom (kColW);   // (Q block area, intentionally blank)
        col.removeFromBottom (kColW);   // (Freq block area, intentionally blank)
        col.removeFromTop (22);         // (Type combo area, intentionally blank)
        mMainReadoutR = col.removeFromBottom (kReadoutH).translated (0, -5);
        auto faderR   = col.reduced (3, 2);
        if (mMainLevelFader && faderR.getHeight() > 8)
            mMainLevelFader->setBounds (faderR);
    }
}

void ParametricEQDisplay::paint(juce::Graphics& g)
{
    // Outer background
    g.setColour(VC::Panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.f);

    // 2026-04-19: toolbar row + [SPARE] indicator both removed. Toolbar
    // migrated to PageMenuBar hamburger; bank indicator migrated to a BankIndicator
    // pill injected into PageMenuBar's extra-right slot.

    drawGrid(g);
    if (mHeatmapEnabled && mHeatmapHasData)
        drawHeatmap(g);
    // 12i: draw if EITHER pre or post spectrum is ready. drawSpectrum handles
    // the per-curve ready flags internally.
    if (mSpectrumReady || mSpectrumPreReady)
        drawSpectrum(g);
    if (mShowPhase) drawPhaseCurve(g);
    drawCurve(g);
    drawHandles(g);

    // ── Right panel background ─────────────────────────────────────────────
    if (mRightPanelArea.getWidth() > 0)
    {
        g.setColour(VC::Bg.withAlpha(0.85f));
        g.fillRect(mRightPanelArea.toFloat());

        // Left border separating graph from panel
        g.setColour(VC::Accent.withAlpha(0.8f));
        g.drawVerticalLine(mRightPanelArea.getX(),
                           (float)mRightPanelArea.getY(),
                           (float)mRightPanelArea.getBottom());

        // Per-column: color dot header + vertical separator.
        // D.4-Q6: 9-column grid (8 bands + 1 main).  Last column uses a
        // "Main" text header instead of a band color dot.
        const int kColW = mRightPanelArea.getWidth() / (kNumBands + 1);
        for (int i = 0; i < kNumBands; ++i)
        {
            // Color dot at top of each column
            float cx = (float)(mRightPanelArea.getX() + i * kColW + kColW / 2);
            float cy = (float)mRightPanelArea.getY() + 5.f;
            bool  bandOn = mBands[i].enabled && !mBands[i].muted;
            float dotAlpha = bandOn ? 0.9f : 0.3f;
            g.setColour(kBandCols[i].withAlpha(dotAlpha));
            g.fillEllipse(cx - 5.f, cy - 5.f, 10.f, 10.f);
            g.setColour(kBandCols[i].brighter(0.3f).withAlpha(dotAlpha));
            g.drawEllipse(cx - 5.f, cy - 5.f, 10.f, 10.f, 1.f);

            // Column separator (skip first)
            if (i > 0)
            {
                g.setColour(VC::Accent.withAlpha(0.4f));
                g.drawVerticalLine(mRightPanelArea.getX() + i * kColW,
                                   (float)mRightPanelArea.getY() + 1.f,
                                   (float)mRightPanelArea.getBottom() - 1.f);
            }

            // Band number label under dot
            g.setColour(kBandCols[i].withAlpha(bandOn ? 0.7f : 0.25f));
            g.setFont(juce::Font(7.f, juce::Font::bold));
            g.drawText(juce::String(i + 1),
                       (int)(cx - 5.f), (int)(cy + 5.f), 10, 8,
                       juce::Justification::centred, false);

            // ── Per-band readouts: gain dB (below fader) / freq / Q ───────────
            // Small numeric strips so users can see the exact value without
            // having to drag and read the tooltip. Kept compact for narrow
            // columns; unit suffix dropped when width is under ~24 px.
            const auto& band = mBands[i];

            // Gain dB readout (below fader).
            if (mGainReadoutR[i].getWidth() > 4)
            {
                const float  db   = band.gainDb;
                const bool   zero = std::abs(db) < 0.05f;
                juce::String txt  = zero ? juce::String("0.0")
                                         : (db > 0 ? ("+" + juce::String(db, 1))
                                                   :       juce::String(db, 1));
                g.setFont(juce::Font(9.5f, juce::Font::bold));
                g.setColour(zero ? juce::Colour(0xffff8080)
                                 : juce::Colour(0xffffe080));
                g.drawText(txt, mGainReadoutR[i], juce::Justification::centred, false);
            }

            // Freq readout (below freq knob): "440", "1.0k", "12k", etc.
            if (mFreqReadoutR[i].getWidth() > 4)
            {
                juce::String txt;
                const float hz = band.freq;
                if (hz >= 10000.f)    txt = juce::String((int) std::round(hz / 1000.f)) + "k";
                else if (hz >= 1000.f) txt = juce::String(hz / 1000.f, 1) + "k";
                else                   txt = juce::String((int) std::round(hz));
                g.setFont(juce::Font(9.0f));
                g.setColour(VC::Text.withAlpha(0.85f));
                g.drawText(txt, mFreqReadoutR[i], juce::Justification::centred, false);
            }

            // Q readout (below Q knob): 2 decimal places, narrow-friendly.
            if (mQReadoutR[i].getWidth() > 4)
            {
                juce::String txt = juce::String(band.q, 2);
                g.setFont(juce::Font(9.0f));
                g.setColour(VC::Text.withAlpha(0.85f));
                g.drawText(txt, mQReadoutR[i], juce::Justification::centred, false);
            }
        }

        // D.4-Q6: 9th column header - "Main" label + separator + readout.
        {
            const int mainColX = mRightPanelArea.getX() + kNumBands * kColW;
            // Separator between band 8 and Main
            g.setColour (VC::Accent.withAlpha (0.4f));
            g.drawVerticalLine (mainColX,
                                (float) mRightPanelArea.getY() + 1.f,
                                (float) mRightPanelArea.getBottom() - 1.f);
            // "Main" header text where the color dot would be
            g.setColour (VC::Text.withAlpha (0.9f));
            g.setFont (juce::Font (8.f, juce::Font::bold));
            g.drawText ("Main",
                        mainColX, mRightPanelArea.getY() + 1, kColW, 12,
                        juce::Justification::centred, false);

            // Main fader readout
            if (mMainLevelFader && mMainReadoutR.getWidth() > 4)
            {
                const float  db   = (float) mMainLevelFader->getValue();
                const bool   zero = std::abs (db) < 0.05f;
                juce::String txt  = zero ? juce::String ("0.0")
                                         : juce::String (db, 1);
                g.setFont (juce::Font (9.0f));
                g.setColour (VC::Text.withAlpha (0.85f));
                g.drawText (txt, mMainReadoutR, juce::Justification::centred, false);
            }
        }
    }

    // 12j Issue 3: rich hover tooltip drawn LAST so it sits on top of the graph
    // + handles + right panel. Only renders when a band is currently hovered.
    if (mHoveredBand >= 0 && mHoveredBand < kNumBands)
        drawHoverTooltip(g);
}

void ParametricEQDisplay::mouseDown(const juce::MouseEvent& e)
{
    mDragBand = -1;
    mUserDragging = false;
    mFineAdjust = e.mods.isCtrlDown();

    // 2026-04-19: Lock bands - widget refuses any user-initiated edit while
    // locked. Right-click menu still opens (user may want to invoke Compare /
    // Copy A->B etc) but band-drag interactions short-circuit.
    if (mSpareLocked_btn && ! e.mods.isRightButtonDown()) return;

    auto pos = e.position;

    // 12j follow-up Q1: double-click on any Gain/Freq/Q readout rect -> inline
    // edit. Single-click falls through to the normal drag / right-click path
    // so it doesn't steal focus from regular interaction.
    if (e.getNumberOfClicks() >= 2 && e.mods.isLeftButtonDown())
    {
        const juce::Point<int> pInt { (int) pos.x, (int) pos.y };
        for (int i = 0; i < kNumBands; ++i)
        {
            if (mGainReadoutR[i].contains(pInt))
            { beginReadoutEdit(i, ReadoutEditKind::Gain); return; }
            if (mFreqReadoutR[i].contains(pInt))
            { beginReadoutEdit(i, ReadoutEditKind::Freq); return; }
            if (mQReadoutR[i].contains(pInt))
            { beginReadoutEdit(i, ReadoutEditKind::Q);    return; }
        }
    }

    // ── Colored dot click → toggle band enabled ────────────────────────────
    // The dot is painted at the top of each right-panel column (cy ≈ mRightPanelArea.y + 5)
    if (mRightPanelArea.getWidth() > 0 && e.mods.isLeftButtonDown())
    {
        const int kColW = mRightPanelArea.getWidth() / (kNumBands + 1);   // D.4-Q6: +1 main column
        float dotCy = (float)mRightPanelArea.getY() + 5.f;
        if (pos.y >= dotCy - 8.f && pos.y <= dotCy + 8.f)
        {
            for (int i = 0; i < kNumBands; ++i)
            {
                float dotCx = (float)(mRightPanelArea.getX() + i * kColW + kColW / 2);
                if (std::abs(pos.x - dotCx) < 8.f)
                {
                    mBands[i].enabled = !mBands[i].enabled;
                    syncControlsFromBands();
                    beginParamUndoGesture(mBindMode == BindMode::MsDSP && mMsDSPApvts ? mMsDSPApvts->undoManager
                                              : mBindMode == BindMode::APVTS && mAPVTS ? mAPVTS->undoManager : nullptr,
                                          (mBindMode == BindMode::MsDSP ? (mShowMid ? mMsDSPMidPrefix : mMsDSPSidePrefix)
                                                                        : "L" + juce::String(mLayerIdx) + "_eq")
                                              + juce::String(i) + "Freq"); // Task 6 (12-iv)
                    setAPVTSFromBand(i);
                    pushBandToDSP(i);
                    repaint();
                    return;
                }
            }
        }
    }

    // Find handle under cursor
    int hitBand = -1;
    for (int i = 0; i < kNumBands; ++i)
    {
        if (!mBands[i].enabled) continue;
        float hx = freqToX(mBands[i].freq);
        float hy = (mBands[i].type == 1 || mBands[i].type == 2 || mBands[i].type == 5
                    || mBands[i].type == 6 || mBands[i].type == 7)
                     ? gainToY(0.f) : gainToY(mBands[i].gainDb);
        if (std::abs(pos.x - hx) < 10.f && std::abs(pos.y - hy) < 10.f)
        {
            hitBand = i;
            break;
        }
        // Also check M/S chips
        float chipY = hy + 9.f;
        if (chipY + 10.f > (float)mGraphArea.getBottom()) chipY = hy - 19.f;
        if (pos.y >= chipY && pos.y <= chipY + 8.f)
        {
            if (pos.x >= hx - 10.f && pos.x <= hx - 1.f) {
                // M chip: toggle mute
                mBands[i].muted = !mBands[i].muted;
                beginParamUndoGesture(mBindMode == BindMode::MsDSP && mMsDSPApvts ? mMsDSPApvts->undoManager
                                          : mBindMode == BindMode::APVTS && mAPVTS ? mAPVTS->undoManager : nullptr,
                                      (mBindMode == BindMode::MsDSP ? (mShowMid ? mMsDSPMidPrefix : mMsDSPSidePrefix)
                                                                    : "L" + juce::String(mLayerIdx) + "_eq")
                                          + juce::String(i) + "Freq"); // Task 6 (12-iv)
                setAPVTSFromBand(i);
                pushBandToDSP(i);
                repaint();
                return;
            }
            if (pos.x >= hx + 1.f && pos.x <= hx + 10.f) {
                // S chip: toggle solo
                mBands[i].soloed = !mBands[i].soloed;
                beginParamUndoGesture(mBindMode == BindMode::MsDSP && mMsDSPApvts ? mMsDSPApvts->undoManager
                                          : mBindMode == BindMode::APVTS && mAPVTS ? mAPVTS->undoManager : nullptr,
                                      (mBindMode == BindMode::MsDSP ? (mShowMid ? mMsDSPMidPrefix : mMsDSPSidePrefix)
                                                                    : "L" + juce::String(mLayerIdx) + "_eq")
                                          + juce::String(i) + "Freq"); // Task 6 (12-iv)
                setAPVTSFromBand(i);
                pushBandToDSP(i);
                repaint();
                return;
            }
        }
    }

    if (hitBand < 0) return;

    // Alt+Click: reset band to defaults
    if (e.mods.isAltDown())
    {
        mBands[hitBand].gainDb = 0.f;
        mBands[hitBand].q      = 0.707f;
        mBands[hitBand].slope  = 0;
        mBands[hitBand].muted  = false;
        mBands[hitBand].soloed = false;
        syncControlsFromBands();
        beginParamUndoGesture(mBindMode == BindMode::MsDSP && mMsDSPApvts ? mMsDSPApvts->undoManager
                                  : mBindMode == BindMode::APVTS && mAPVTS ? mAPVTS->undoManager : nullptr,
                              (mBindMode == BindMode::MsDSP ? (mShowMid ? mMsDSPMidPrefix : mMsDSPSidePrefix)
                                                            : "L" + juce::String(mLayerIdx) + "_eq")
                                  + juce::String(hitBand) + "Freq"); // Task 6 (12-iv)
        setAPVTSFromBand(hitBand);
        pushBandToDSP(hitBand);
        repaint();
        return;
    }

    // Right-click: type + slope context menu
    if (e.mods.isRightButtonDown())
    {
        juce::PopupMenu menu;
        juce::PopupMenu typeMenu;
        static const char* kTypeNames[] = {
            "Bell", "Low Pass", "High Pass", "Low Shelf", "Hi Shelf", "Notch", "Off", "Band Pass", "Tilt" };
        for (int t = 0; t < 9; ++t)
            typeMenu.addItem(100 + t, kTypeNames[t], true, mBands[hitBand].type == t);
        menu.addSubMenu("Filter Type", typeMenu);

        juce::PopupMenu slopeMenu;
        static const char* kSlopeNames[] = {
            "Center-2 (12 dB/oct)", "Steep-4 (24 dB/oct)", "Steep-6 (36 dB/oct)",
            "Steep-8 (48 dB/oct)", "Gentle-4 (LR 24)", "Gentle-6 (LR 36)", "Gentle-8 (LR 48)" };
        for (int s = 0; s < 7; ++s)
            slopeMenu.addItem(200 + s, kSlopeNames[s], true, mBands[hitBand].slope == s);
        menu.addSubMenu("Slope / Order", slopeMenu);

        // Session B: per-band Channel routing submenu (12h Stereo/Mid/Side/L/R).
        // 12g (T2a option B): when the EQ instance is in any linear-phase mode,
        // per-band M/S routing is restricted to Stereo (the linear-phase IR is
        // a single combined response on full L/R). Grey the submenu items so
        // the user sees the option still exists but can't toggle it.
        bool linearModeOn = false;
        if (mBindMode == BindMode::DSP && mBoundDSP)
            linearModeOn = mBoundDSP->isLinearPhaseMode();
        else if (mBindMode == BindMode::MsDSP && mBoundMsDsp)
            linearModeOn = mBoundMsDsp->isLinearPhaseMode();

        juce::PopupMenu chanMenu;
        static const char* kChanNames[] = {
            "Stereo", "Mid", "Side", "L Only", "R Only" };
        for (int c = 0; c < 5; ++c)
            chanMenu.addItem(400 + c, kChanNames[c], ! linearModeOn,
                             mBands[hitBand].channel == c);
        const juce::String chanLabel = linearModeOn ? juce::String("Channel  (disabled in Linear modes)")
                                                    : juce::String("Channel");
        menu.addSubMenu(chanLabel, chanMenu);

        // Session B: Automate submenu - per-band paramIds routed through the
        // VKnobAutomation sOnAutomate callback. Only enabled when the widget has
        // an APVTS write-back path (full bindMsDSP). Disabled items still show
        // so users understand what's available; clicking them is a no-op.
        const bool canAutomate = (mBindMode == BindMode::MsDSP && mMsDSPApvts != nullptr);
        juce::PopupMenu autoMenu;
        static const char* kAutoNames[] = {
            "Freq", "Gain", "Q", "Type", "On", "Slope", "Mute", "Solo", "Channel" };
        for (int a = 0; a < 9; ++a)
            autoMenu.addItem(500 + a, kAutoNames[a], canAutomate, false);
        menu.addSubMenu("Automate", autoMenu, canAutomate);

        menu.addSeparator();
        // 12j: Dynamic EQ toggle + popout panel. Only enabled for gain-bearing
        // types (Peaking / LowShelf / HighShelf / Tilt); other types can't be
        // dynamic (no gain to modulate).
        const int t = mBands[hitBand].type;
        const bool dynSupported = (t == 0 || t == 3 || t == 4 || t == 8);
        // 12g (T2b option C): dynamic EQ disabled in linear-phase modes -
        // the static IR cannot follow per-block GR without per-block IR
        // rebuild (deferred to T16). Grey the items so the option remains
        // visible but can't be toggled.
        const bool dynAvailable = dynSupported && ! linearModeOn;
        const juce::String dynLabel = linearModeOn ? juce::String("Make Dynamic  (disabled in Linear modes)")
                                                   : juce::String("Make Dynamic");
        menu.addItem(600, dynLabel, dynAvailable, mBands[hitBand].dynamic);
        menu.addItem(601, "Dynamic Params...",
                     dynAvailable && mBands[hitBand].dynamic && canAutomate, false);
        menu.addSeparator();
        menu.addItem(300, "Reset Band", true, false);

        const int band = hitBand;
        auto opts = juce::PopupMenu::Options()
            .withTargetScreenArea({ e.getScreenX(), e.getScreenY(), 1, 1 });
        menu.showMenuAsync(opts, [this, band](int result)
        {
            if (result >= 100 && result < 108) {
                const int newType = result - 100;
                mBands[band].type = newType;
                // Issue 5 mirror: zero gain when switching to a non-gain-bearing
                // type (LP/HP/Notch/BP/Off). Matches the typeCombo onChange path.
                const bool nonGainBearing = (newType == 1 || newType == 2
                                          || newType == 5 || newType == 6 || newType == 7);
                if (nonGainBearing && mBands[band].gainDb != 0.f)
                    mBands[band].gainDb = 0.f;
                syncControlsFromBands();
                beginParamUndoGesture(mBindMode == BindMode::MsDSP && mMsDSPApvts ? mMsDSPApvts->undoManager
                                          : mBindMode == BindMode::APVTS && mAPVTS ? mAPVTS->undoManager : nullptr,
                                      (mBindMode == BindMode::MsDSP ? (mShowMid ? mMsDSPMidPrefix : mMsDSPSidePrefix)
                                                                    : "L" + juce::String(mLayerIdx) + "_eq")
                                          + juce::String(band) + "Freq"); // Task 6 (12-iv)
                setAPVTSFromBand(band);
                pushBandToDSP(band);
                repaint();
            } else if (result >= 200 && result < 207) {
                mBands[band].slope = result - 200;
                beginParamUndoGesture(mBindMode == BindMode::MsDSP && mMsDSPApvts ? mMsDSPApvts->undoManager
                                          : mBindMode == BindMode::APVTS && mAPVTS ? mAPVTS->undoManager : nullptr,
                                      (mBindMode == BindMode::MsDSP ? (mShowMid ? mMsDSPMidPrefix : mMsDSPSidePrefix)
                                                                    : "L" + juce::String(mLayerIdx) + "_eq")
                                          + juce::String(band) + "Freq"); // Task 6 (12-iv)
                setAPVTSFromBand(band);
                pushBandToDSP(band);
                repaint();
            } else if (result == 300) {
                // Reset Band: restore freq (to this band's default), gain, Q, slope.
                // Type / on / mute / solo / channel are intentionally left alone -
                // only the frequency-response shape resets, not the routing config.
                mBands[band].freq   = kEQDefaultFreqs[band];
                mBands[band].gainDb = 0.f;
                mBands[band].q      = 0.707f;
                mBands[band].slope  = 0;
                syncControlsFromBands();
                beginParamUndoGesture(mBindMode == BindMode::MsDSP && mMsDSPApvts ? mMsDSPApvts->undoManager
                                          : mBindMode == BindMode::APVTS && mAPVTS ? mAPVTS->undoManager : nullptr,
                                      (mBindMode == BindMode::MsDSP ? (mShowMid ? mMsDSPMidPrefix : mMsDSPSidePrefix)
                                                                    : "L" + juce::String(mLayerIdx) + "_eq")
                                          + juce::String(band) + "Freq"); // Task 6 (12-iv)
                setAPVTSFromBand(band);
                pushBandToDSP(band);
                repaint();
            } else if (result >= 400 && result < 405) {
                // Session B: Channel routing change (Stereo/Mid/Side/L/R).
                mBands[band].channel = result - 400;
                beginParamUndoGesture(mBindMode == BindMode::MsDSP && mMsDSPApvts ? mMsDSPApvts->undoManager
                                          : mBindMode == BindMode::APVTS && mAPVTS ? mAPVTS->undoManager : nullptr,
                                      (mBindMode == BindMode::MsDSP ? (mShowMid ? mMsDSPMidPrefix : mMsDSPSidePrefix)
                                                                    : "L" + juce::String(mLayerIdx) + "_eq")
                                          + juce::String(band) + "Freq"); // Task 6 (12-iv)
                setAPVTSFromBand(band);
                pushBandToDSP(band);
                repaint();
            } else if (result >= 500 && result < 509) {
                // Session B: Automate menu - fire VKnobAutomation callback for
                // the chosen paramId. Requires full bindMsDSP with APVTS prefix.
                if (mBindMode != BindMode::MsDSP || !mMsDSPApvts) return;
                static const char* kAutoSuffix[9] = {
                    "Freq", "Gain", "Q", "Type", "On", "Slope", "Mute", "Solo", "Channel" };
                const int idx = result - 500;
                const juce::String prefix = (mShowMid ? mMsDSPMidPrefix
                                                      : mMsDSPSidePrefix)
                                            + juce::String(band);
                const juce::String paramId = prefix + kAutoSuffix[idx];
                if (VKnobAutomation::sOnAutomate)
                    VKnobAutomation::sOnAutomate(paramId);
            } else if (result == 600) {
                // 12j: toggle Make Dynamic on this band.
                // C.4 follow-up (2026-04-30): write only the Dynamic flag (not
                // the full band state, which would clobber APVTS-authoritative
                // values with stale UI defaults).  When toggling FROM off TO
                // on, ALSO reset Range to 0 so the dotted ghost curve sits
                // flat on first enable -- user dials in the modulation
                // direction + amount via the Range slider.  Toggling off
                // leaves Range alone so re-enabling preserves the user's
                // setting (and turning off doesn't strip out their work).
                const bool wasOff = ! mBands[band].dynamic;
                mBands[band].dynamic = !mBands[band].dynamic;
                if (wasOff)
                    mBands[band].rangeDb = 0.f;

                auto writeFlag = [this, band](juce::AudioProcessorValueTreeState* apvts,
                                                const juce::String& prefix)
                {
                    if (apvts == nullptr || prefix.isEmpty()) return;
                    auto setF = [&](const juce::String& id, float val)
                    {
                        if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(apvts->getParameter(id)))
                            p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(val));
                    };
                    const juce::String bp = prefix + juce::String(band);
                    setF(bp + "Dynamic", mBands[band].dynamic ? 1.f : 0.f);
                    if (mBands[band].dynamic) // wasOff is implied when dynamic is now true
                        setF(bp + "Range", mBands[band].rangeDb);   // 0
                };
                beginParamUndoGesture(mBindMode == BindMode::MsDSP && mMsDSPApvts ? mMsDSPApvts->undoManager
                                          : mBindMode == BindMode::APVTS && mAPVTS ? mAPVTS->undoManager : nullptr,
                                      (mBindMode == BindMode::MsDSP ? mMsDSPMidPrefix
                                                                    : "L" + juce::String(mLayerIdx) + "_eq")
                                          + juce::String(band) + "Dynamic"); // Task 6 (12-iv)
                if (mBindMode == BindMode::MsDSP && mMsDSPApvts)
                {
                    writeFlag(mMsDSPApvts, mMsDSPMidPrefix);
                    writeFlag(mMsDSPApvts, mMsDSPSidePrefix);
                }
                else if (mBindMode == BindMode::APVTS && mAPVTS && mLayerIdx >= 0)
                {
                    writeFlag(mAPVTS, "L" + juce::String(mLayerIdx) + "_eq");
                }

                // Tell DSP the new dynamic state too (UI-authoritative for
                // direct-DSP bind modes that don't have APVTS write-back).
                auto pushDspFlag = [this, band](EQ8DSP& d)
                {
                    d.setBandDynamic(band, mBands[band].dynamic);
                    if (mBands[band].dynamic)
                        d.setBandRange(band, mBands[band].rangeDb);   // 0
                };
                if (mBindMode == BindMode::DSP && mBoundDSP)
                    pushDspFlag(*mBoundDSP);
                else if (mBindMode == BindMode::MsDSP && mBoundMsDsp)
                {
                    pushDspFlag(mBoundMsDsp->mid());
                    pushDspFlag(mBoundMsDsp->side());
                }
                repaint();
            } else if (result == 601) {
                // 12j: Open dynamic params popout (CallOutBox with threshold /
                // ratio / attack / release / range knobs + upward toggle +
                // live GR meter).
                openDynamicParamsPopout(band);
            }
        });
        return;
    }

    // Left-click: start drag
    mDragBand    = hitBand;
    mUserDragging = true;
    mDragOrigin  = pos;
}

void ParametricEQDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (mDragBand < 0) return;
    if (mSpareLocked_btn) return;   // bands locked - no edits

    // Fine-adjust: 0.1× sensitivity when Ctrl held
    float scale = mFineAdjust ? 0.1f : 1.0f;
    juce::Point<float> delta = (e.position - mDragOrigin) * scale;
    juce::Point<float> effectivePos = mDragOrigin + delta;

    float newFreq = xToFreq(effectivePos.x);
    float newGain = yToGain(effectivePos.y);

    // LP/HP/BP/Notch: disable Y-axis drag (no gain)
    const int t = mBands[mDragBand].type;
    if (t == 1 || t == 2 || t == 5 || t == 7)
        newGain = mBands[mDragBand].gainDb;  // keep existing

    mBands[mDragBand].freq   = juce::jlimit(20.f, 20000.f, newFreq);
    mBands[mDragBand].gainDb = juce::jlimit(-18.f, 18.f, newGain);
    syncControlsFromBands();
    setAPVTSFromBand(mDragBand);
    pushBandToDSP(mDragBand);
    repaint();
}

void ParametricEQDisplay::mouseUp(const juce::MouseEvent&)
{
    mDragBand     = -1;
    mUserDragging = false;
}

void ParametricEQDisplay::mouseMove(const juce::MouseEvent& e)
{
    int prev = mHoveredBand;
    mHoveredBand = -1;
    for (int i = 0; i < kNumBands; ++i)
    {
        if (!mBands[i].enabled) continue;
        float hx = freqToX(mBands[i].freq);
        float hy = gainToY(mBands[i].gainDb);
        if (std::abs(e.position.x - hx) < 12.f && std::abs(e.position.y - hy) < 12.f)
        {
            mHoveredBand = i;
            break;
        }
    }
    if (mHoveredBand != prev) repaint();
}

void ParametricEQDisplay::mouseWheelMove(const juce::MouseEvent& e,
                                          const juce::MouseWheelDetails& wheel)
{
    // Find which band handle is under cursor
    int band = -1;
    for (int i = 0; i < kNumBands; ++i)
    {
        if (!mBands[i].enabled) continue;
        float hx = freqToX(mBands[i].freq);
        float hy = gainToY(mBands[i].gainDb);
        if (std::abs(e.position.x - hx) < 16.f && std::abs(e.position.y - hy) < 16.f)
        {
            band = i;
            break;
        }
    }
    if (band < 0) return;

    // Wheel up = tighter Q (higher value), wheel down = wider Q (lower value)
    float delta = wheel.deltaY > 0 ? 1.15f : (1.f / 1.15f);
    mBands[band].q = juce::jlimit(0.1f, 10.f, mBands[band].q * delta);
    syncControlsFromBands();   // update Q dial to reflect new value
    setAPVTSFromBand(band);
    pushBandToDSP(band);
    repaint();
    (void)e;
}

// ──────────────────────────── ParametricEQDisplay – new Phase 8B methods ─────

void ParametricEQDisplay::pushSamples (const float* data, int numSamples)
{
    // POST-EQ spectrum (existing path). Accumulate into FIFO; when full, FFT.
    for (int i = 0; i < numSamples; ++i)
    {
        mFifoBuffer[mFifoIndex++] = data[i];
        if (mFifoIndex >= kFFTSize)
        {
            mFifoIndex = 0;
            // Apply Hann window and copy to FFT work buffer
            for (int k = 0; k < kFFTSize; ++k)
            {
                float w = 0.5f * (1.f - std::cos (
                    juce::MathConstants<float>::twoPi * k / (float)(kFFTSize - 1)));
                mFFTData[k] = mFifoBuffer[k] * w;
            }
            std::fill (mFFTData.begin() + kFFTSize, mFFTData.end(), 0.f);
            mFFT.performFrequencyOnlyForwardTransform (mFFTData.data());
            for (int k = 0; k < kFFTSize / 2; ++k)
                mSpectrumDb[k] = juce::Decibels::gainToDecibels (
                    mFFTData[k] / (float)kFFTSize, -100.f);
            mSpectrumReady = true;

            // Write FFT frame into heatmap circular buffer (post-EQ only)
            for (int k = 0; k < kFFTSize / 2; ++k)
                mHeatmapFrames[mHeatmapWritePos][k] = mSpectrumDb[k];
            mHeatmapWritePos = (mHeatmapWritePos + 1) % kNumHeatmapFrames;
            mHeatmapHasData  = true;

            repaint();
        }
    }
}

// 12i: PRE-EQ spectrum path. Same FFT math as pushSamples() but writes to the
// pre-buffers (mFifoBufferPre / mSpectrumDbPre). Shares mFFT since both paths
// are called on the UI thread sequentially. Does NOT feed the heatmap.
void ParametricEQDisplay::pushSamplesPre (const float* data, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        mFifoBufferPre[mFifoIndexPre++] = data[i];
        if (mFifoIndexPre >= kFFTSize)
        {
            mFifoIndexPre = 0;
            for (int k = 0; k < kFFTSize; ++k)
            {
                float w = 0.5f * (1.f - std::cos (
                    juce::MathConstants<float>::twoPi * k / (float)(kFFTSize - 1)));
                mFFTData[k] = mFifoBufferPre[k] * w;
            }
            std::fill (mFFTData.begin() + kFFTSize, mFFTData.end(), 0.f);
            mFFT.performFrequencyOnlyForwardTransform (mFFTData.data());
            for (int k = 0; k < kFFTSize / 2; ++k)
                mSpectrumDbPre[k] = juce::Decibels::gainToDecibels (
                    mFFTData[k] / (float)kFFTSize, -100.f);
            mSpectrumPreReady = true;
            repaint();
        }
    }
}

// 12i helper: build fill + line paths from a spectrum-dB array. Returns true if
// at least one bin was in-range. Caller colours and strokes the paths.
// Path is extended horizontally to the graph's left + right edges using the
// first / last in-range bin's dB value so the curve visually fills the whole
// grid, even when the FFT bin resolution (e.g. ~47 Hz at 48 kHz / 1024-pt FFT)
// means the lowest usable bin is well above the grid's 20 Hz left edge.
static bool buildSpectrumPaths (const float* spectrumDb, int numBins,
                                double sampleRateForFFT,
                                const juce::Rectangle<int>& a,
                                std::function<float(float)> freqToX,
                                juce::Path& fill, juce::Path& line)
{
    const float nyquist = (float)(sampleRateForFFT * 0.5);
    const float gridL   = (float) a.getX();
    const float gridR   = (float) a.getRight();
    bool  started = false;
    float firstY = 0.f, lastY = 0.f;
    for (int bin = 1; bin < numBins; ++bin)
    {
        float hz = bin * nyquist / (float)numBins;
        if (hz < 20.f || hz > 20000.f) continue;
        float x  = freqToX (hz);
        float db = juce::jlimit (-80.f, 0.f, spectrumDb[bin]);
        float ny = (db + 80.f) / 80.f;
        float y  = (float)a.getBottom() - ny * (float)a.getHeight();
        if (!started)
        {
            // Anchor path at grid-left using the first in-range bin's dB value -
            // flat extension across the sub-bin-resolution low end of the grid.
            fill.startNewSubPath (gridL, (float)a.getBottom());
            fill.lineTo (gridL, y);
            line.startNewSubPath (gridL, y);
            if (x > gridL + 0.5f)
            {
                fill.lineTo (x, y);
                line.lineTo (x, y);
            }
            firstY  = y;
            started = true;
        }
        else
        {
            fill.lineTo (x, y);
            line.lineTo (x, y);
        }
        lastY = y;
    }
    if (started)
    {
        // Flat-extend the last in-range bin across to the grid's right edge so
        // the curve fills the high end even when nyquist < 20 kHz.
        fill.lineTo (gridR, lastY);
        line.lineTo (gridR, lastY);
        fill.lineTo (gridR, (float)a.getBottom());
        fill.closeSubPath();
    }
    juce::ignoreUnused (firstY);
    return started;
}

void ParametricEQDisplay::drawSpectrum (juce::Graphics& g) const
{
    auto& a = mGraphArea;
    if (a.isEmpty() || mSampleRateForFFT <= 0.0) return;
    const int numBins = kFFTSize / 2;

    // 12i: PRE-EQ spectrum first (translucent grey, drawn behind the post curve).
    if (mSpectrumPreReady)
    {
        juce::Path pfill, pline;
        if (buildSpectrumPaths (mSpectrumDbPre.data(), numBins, mSampleRateForFFT, a,
                                [this](float hz) { return this->freqToX(hz); }, pfill, pline))
        {
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillPath  (pfill);
            g.setColour (juce::Colours::white.withAlpha (0.22f));
            g.strokePath (pline, juce::PathStrokeType (1.f));
        }
    }

    // POST-EQ spectrum (existing green overlay) on top.
    if (mSpectrumReady)
    {
        juce::Path fill, line;
        if (buildSpectrumPaths (mSpectrumDb.data(), numBins, mSampleRateForFFT, a,
                                [this](float hz) { return this->freqToX(hz); }, fill, line))
        {
            g.setColour (VC::Green.withAlpha (0.09f));
            g.fillPath  (fill);
            g.setColour (VC::Green.withAlpha (0.28f));
            g.strokePath (line, juce::PathStrokeType (1.f));
        }
    }
}

// 12j follow-up Q1: begin an inline readout edit. Positions the shared TextEditor
// at the clicked readout rect, pre-fills with current value formatted the same
// way the readout renders, grabs focus + selects all for easy overwrite.
void ParametricEQDisplay::beginReadoutEdit(int band, ReadoutEditKind kind)
{
    if (!mReadoutEditor || band < 0 || band >= kNumBands) return;
    mReadoutEditBand = band;
    mReadoutEditKind = kind;

    juce::Rectangle<int> rect;
    juce::String         text;
    const auto& b = mBands[band];

    switch (kind)
    {
        case ReadoutEditKind::Gain:
            rect = mGainReadoutR[band];
            if (std::abs(b.gainDb) < 0.05f) text = "0.0";
            else if (b.gainDb > 0.f) text = "+" + juce::String(b.gainDb, 1);
            else                     text =       juce::String(b.gainDb, 1);
            break;
        case ReadoutEditKind::Freq:
            rect = mFreqReadoutR[band];
            if (b.freq >= 10000.f)      text = juce::String((int) std::round(b.freq / 1000.f)) + "k";
            else if (b.freq >= 1000.f)  text = juce::String(b.freq / 1000.f, 1) + "k";
            else                        text = juce::String((int) std::round(b.freq));
            break;
        case ReadoutEditKind::Q:
            rect = mQReadoutR[band];
            text = juce::String(b.q, 2);
            break;
        default: return;
    }

    if (rect.getWidth() < 4 || rect.getHeight() < 4) return;
    mReadoutEditor->setBounds(rect);
    mReadoutEditor->setText(text, juce::dontSendNotification);
    mReadoutEditor->setVisible(true);
    mReadoutEditor->toFront(true);
    mReadoutEditor->selectAll();
    mReadoutEditor->grabKeyboardFocus();
}

// Parse the entered text + apply via the same validation path the existing
// right-click "Type in value..." prompt uses. Handles Hz / kHz suffixes on
// freq input; plain numbers elsewhere. Out-of-range values clamp silently.
void ParametricEQDisplay::commitReadoutEdit()
{
    if (!mReadoutEditor || mReadoutEditKind == ReadoutEditKind::None) return;
    const int   band = mReadoutEditBand;
    const auto  kind = mReadoutEditKind;
    // Clear state FIRST so onFocusLost doesn't recurse into commit.
    mReadoutEditKind = ReadoutEditKind::None;
    mReadoutEditBand = -1;

    juce::String txt = mReadoutEditor->getText().trim();
    mReadoutEditor->setVisible(false);

    if (band < 0 || band >= kNumBands) return;
    if (txt.isEmpty()) return;

    switch (kind)
    {
        case ReadoutEditKind::Gain:
        {
            float v = txt.removeCharacters("+").getFloatValue();
            v = juce::jlimit(-18.0f, 18.0f, v);
            mBands[band].gainDb = v;
            break;
        }
        case ReadoutEditKind::Freq:
        {
            // Accept "1k", "1.5k", "440", "12khz", "12kHz" etc.
            juce::String s = txt.toLowerCase().removeCharacters(" ");
            s = s.replace("hz", "");
            float mult = 1.0f;
            if (s.endsWithChar('k')) { mult = 1000.0f; s = s.dropLastCharacters(1); }
            float v = s.getFloatValue() * mult;
            v = juce::jlimit(20.0f, 20000.0f, v);
            mBands[band].freq = v;
            break;
        }
        case ReadoutEditKind::Q:
        {
            float v = txt.removeCharacters("qQ").getFloatValue();
            v = juce::jlimit(0.1f, 10.0f, v);
            mBands[band].q = v;
            break;
        }
        default: return;
    }
    syncControlsFromBands();
    beginParamUndoGesture(mBindMode == BindMode::MsDSP && mMsDSPApvts ? mMsDSPApvts->undoManager
                              : mBindMode == BindMode::APVTS && mAPVTS ? mAPVTS->undoManager : nullptr,
                          (mBindMode == BindMode::MsDSP ? (mShowMid ? mMsDSPMidPrefix : mMsDSPSidePrefix)
                                                        : "L" + juce::String(mLayerIdx) + "_eq")
                              + juce::String(band) + "Freq"); // Task 6 (12-iv)
    setAPVTSFromBand(band);
    pushBandToDSP(band);
    repaint();
}

void ParametricEQDisplay::cancelReadoutEdit()
{
    if (!mReadoutEditor) return;
    mReadoutEditKind = ReadoutEditKind::None;
    mReadoutEditBand = -1;
    mReadoutEditor->setVisible(false);
}

// Bonus Q3: stamp paramIds on the 3 per-band right-panel controls so
// GlobalAutoRightClick can catch right-clicks on them. Called when the widget
// binds or when mShowMid flips (paramIds track the currently-viewed prefix).
void ParametricEQDisplay::stampRightPanelComponentIds()
{
    const juce::String prefixBase =
        (mBindMode == BindMode::MsDSP && mMsDSPApvts)
            ? (mShowMid ? mMsDSPMidPrefix : mMsDSPSidePrefix)
            : juce::String();
    if (prefixBase.isEmpty())
    {
        // Clear componentIDs when unbound so no stale paramIds linger.
        for (int i = 0; i < kNumBands; ++i)
        {
            if (mControls[i].freqKnob)  mControls[i].freqKnob ->setComponentID({});
            if (mControls[i].gainFader) mControls[i].gainFader->setComponentID({});
            if (mControls[i].qKnob)     mControls[i].qKnob    ->setComponentID({});
        }
        return;
    }
    for (int i = 0; i < kNumBands; ++i)
    {
        const juce::String bp = prefixBase + juce::String(i);
        if (mControls[i].freqKnob)  mControls[i].freqKnob ->setComponentID(bp + "Freq");
        if (mControls[i].gainFader) mControls[i].gainFader->setComponentID(bp + "Gain");
        if (mControls[i].qKnob)     mControls[i].qKnob    ->setComponentID(bp + "Q");
    }
}

// 12j Issue 3: rich 3-column hover tooltip. Replaces the JUCE system tooltip
// (getTooltip() returns "") so we have full pixel control.
//
//   Column 1 - static band info (type, freq, gain, Q, channel, state)
//   Column 2 - dynamic knob values (Thr / Ratio / Atk / Rel / Range / Up)
//   Column 3 - graphical GR meter (vertical bar with +/-range centreline;
//              orange = downward compression, green = upward expansion)
//
// Panel is positioned near the hovered handle + clamped to stay inside the
// graph area. Renders immediately on hover (no JUCE idle-in delay) so mouse
// movement across bands updates smoothly.
void ParametricEQDisplay::drawHoverTooltip(juce::Graphics& g) const
{
    if (mHoveredBand < 0 || mHoveredBand >= kNumBands) return;
    const auto& b = mBands[mHoveredBand];

    static const char* kTypeNames[] = {
        "Peaking", "Low Pass", "High Pass", "Low Shelf", "High Shelf",
        "Notch",   "Off",      "Band Pass", "Tilt" };
    static const char* kChanNames[] = {
        "Stereo", "Mid", "Side", "L Only", "R Only" };

    const int typeIdx   = juce::jlimit(0, 8, b.type);
    const int chanIdx   = juce::jlimit(0, 4, b.channel);
    const bool dynSupp  = (b.type == 0 || b.type == 3 || b.type == 4 || b.type == 8);
    const bool dynOn    = b.dynamic && dynSupp;

    // Panel geometry.
    const int panelW  = dynOn ? 360 : 170;   // 3-col when dynamic, 1-col otherwise
    const int panelH  = 110;
    const int colW    = 120;
    const int col1X   = 8;
    const int col2X   = col1X + colW;
    const int col3X   = col2X + colW;
    const int meterW  = 28;

    // Anchor near the hovered handle, offset up + right, clamp to graph bounds.
    const float hx = freqToX(b.freq);
    const float hy = (b.type == 1 || b.type == 2 || b.type == 5
                   || b.type == 6 || b.type == 7) ? gainToY(0.f) : gainToY(b.gainDb);
    float px = hx + 12.f;
    float py = hy - (float) panelH - 6.f;
    if (px + panelW > mGraphArea.getRight() - 2)  px = hx - panelW - 12.f;
    if (px < mGraphArea.getX() + 2)               px = (float) mGraphArea.getX() + 2.f;
    if (py < mGraphArea.getY() + 2)               py = hy + 18.f;
    if (py + panelH > mGraphArea.getBottom() - 2) py = (float) mGraphArea.getBottom() - panelH - 2.f;

    const juce::Rectangle<float> panel((float) px, (float) py,
                                        (float) panelW, (float) panelH);

    // Background + border.
    g.setColour(VC::Panel.withAlpha(0.96f));
    g.fillRoundedRectangle(panel, 4.f);
    g.setColour(kBandCols[mHoveredBand].withAlpha(0.85f));
    g.drawRoundedRectangle(panel.reduced(0.5f), 4.f, 1.0f);

    // ── Column 1: band info ──────────────────────────────────────────────────
    g.setFont(juce::Font(11.f, juce::Font::bold));
    g.setColour(kBandCols[mHoveredBand]);
    g.drawText("Band " + juce::String(mHoveredBand + 1) + " - " + kTypeNames[typeIdx],
               (int)(px + col1X), (int)(py + 4),
               colW * 3, 14,
               juce::Justification::centredLeft, false);

    g.setFont(juce::Font(10.f));
    g.setColour(VC::Text);

    auto drawLine = [&](int colX, int lineIdx, const juce::String& label, const juce::String& value)
    {
        const int lh = 13;
        const int ly = (int)(py + 22 + lineIdx * lh);
        g.setColour(VC::TextDim);
        g.drawText(label, colX + (int)px, ly, 40, lh, juce::Justification::centredLeft, false);
        g.setColour(VC::Text);
        g.drawText(value, colX + (int)px + 42, ly, colW - 44, lh,
                   juce::Justification::centredLeft, false);
    };

    auto fmtHz = [](float hz) -> juce::String {
        if (hz >= 1000.f) return juce::String(hz / 1000.f, 2) + " kHz";
        return juce::String((int) std::round(hz)) + " Hz";
    };
    auto fmtDb = [](float db) -> juce::String {
        if (std::abs(db) < 0.05f) return "0.0 dB";
        return (db > 0.f ? "+" : "") + juce::String(db, 1) + " dB";
    };

    drawLine(col1X, 0, "Freq",    fmtHz(b.freq));
    drawLine(col1X, 1, "Gain",    fmtDb(b.gainDb));
    drawLine(col1X, 2, "Q",       juce::String(b.q, 2));
    drawLine(col1X, 3, "Slope",   juce::String(b.slope));
    drawLine(col1X, 4, "Channel", kChanNames[chanIdx]);
    juce::String state = b.enabled ? (b.muted ? "Muted"
                                     : b.soloed ? "SOLO" : "On")
                                    : "Off";
    drawLine(col1X, 5, "State",   state);

    if (!dynOn) return;

    // ── Column 2: dynamic knob values ────────────────────────────────────────
    // 12j follow-up Q2: direction inferred from signed rangeDb.
    const char* dynLabel = (b.rangeDb > 0.01f)  ? "Dynamic (Expand)"
                         : (b.rangeDb < -0.01f) ? "Dynamic (Compress)"
                                                : "Dynamic (off)";
    const juce::Colour dynHeaderCol = (b.rangeDb > 0.01f) ? juce::Colour(0xff33ffaa)
                                    : (b.rangeDb < -0.01f)? juce::Colour(0xffff7f33)
                                                          : VC::TextDim;
    g.setFont(juce::Font(10.f, juce::Font::bold));
    g.setColour(dynHeaderCol);
    g.drawText(dynLabel,
               (int)(px + col2X), (int)(py + 4),
               colW - 4, 14,
               juce::Justification::centredLeft, false);

    g.setFont(juce::Font(10.f));
    drawLine(col2X, 0, "Thr",     juce::String((int) std::round(b.threshold)) + " dB");
    drawLine(col2X, 1, "Ratio",   juce::String(b.ratio, 1) + ":1");
    drawLine(col2X, 2, "Attack",  juce::String(b.attack,  1) + " ms");
    drawLine(col2X, 3, "Release", juce::String(b.release, 0) + " ms");
    // Range shows signed value with explicit sign for clarity.
    juce::String rngTxt = (b.rangeDb >= 0.f) ? ("+" + juce::String(b.rangeDb, 0))
                                             :       juce::String(b.rangeDb, 0);
    drawLine(col2X, 4, "Range",   rngTxt + " dB");
    drawLine(col2X, 5, "Mode",    (b.rangeDb > 0.f) ? "Expand"
                                                    : (b.rangeDb < 0.f ? "Compress" : "off"));

    // ── Column 3: graphical GR meter ─────────────────────────────────────────
    const float meterX = px + col3X + 32;
    const float meterY = py + 22;
    const float meterH = 82;
    // Track.
    g.setColour(juce::Colour(0xff141414));
    g.fillRect(meterX, meterY, (float) meterW, meterH);
    g.setColour(VC::Accent.withAlpha(0.6f));
    g.drawRect(meterX, meterY, (float) meterW, meterH, 0.7f);

    // Centerline (0 dB).
    const float centreY = meterY + meterH * 0.5f;
    g.setColour(juce::Colour(0xff888888));
    g.drawLine(meterX, centreY, meterX + meterW, centreY, 0.8f);

    // Fill proportional to currentGrDb. maxRange uses |rangeDb| magnitude.
    const float maxRange = juce::jmax(1.f, std::abs(b.rangeDb));
    const float grDb     = juce::jlimit(-maxRange, maxRange, b.currentGrDb);
    if (grDb < 0.f)
    {
        const float h = (meterH * 0.5f) * (-grDb / maxRange);
        g.setColour(juce::Colour(0xffff7f33).withAlpha(0.90f));
        g.fillRect(meterX + 2.f, centreY, (float)(meterW - 4), h);
    }
    else if (grDb > 0.f)
    {
        const float h = (meterH * 0.5f) * (grDb / maxRange);
        g.setColour(juce::Colour(0xff33ffaa).withAlpha(0.90f));
        g.fillRect(meterX + 2.f, centreY - h, (float)(meterW - 4), h);
    }

    // +/- range ticks (absolute magnitude on both sides of 0 dB centreline).
    g.setColour(VC::TextDim);
    g.setFont(juce::Font(8.f));
    const int rngMag = (int) std::round(std::abs(b.rangeDb));
    g.drawText("+" + juce::String(rngMag),
               (int)(meterX + meterW + 2), (int) meterY - 4, 30, 10,
               juce::Justification::centredLeft, false);
    g.drawText(" 0", (int)(meterX + meterW + 2), (int) centreY - 5, 30, 10,
               juce::Justification::centredLeft, false);
    g.drawText("-" + juce::String(rngMag),
               (int)(meterX + meterW + 2), (int) meterY + (int) meterH - 6, 30, 10,
               juce::Justification::centredLeft, false);

    // Header + numeric GR readout.
    g.setFont(juce::Font(10.f, juce::Font::bold));
    g.setColour(VC::Text);
    g.drawText("GR", (int) meterX - 24, (int) meterY, 22, 12,
               juce::Justification::centredRight, false);

    const bool atZero = std::abs(b.currentGrDb) < 0.05f;
    juce::String grText = atZero ? "0.0 dB"
                                 : (b.currentGrDb > 0.f
                                        ? ("+" + juce::String(b.currentGrDb, 1) + " dB")
                                        : (juce::String(b.currentGrDb, 1) + " dB"));
    g.setFont(juce::Font(9.f, juce::Font::bold));
    g.setColour(atZero ? VC::TextDim : juce::Colour(0xffffcc44));
    g.drawText(grText, (int)(px + col3X), (int)(py + panelH - 16),
               colW, 12, juce::Justification::centred, false);
}

void ParametricEQDisplay::syncFromAPVTS()
{
    // Ahead of the early-out: APVTS mode draws no spectrum, but its CURVE is
    // still a biquad magnitude evaluated at the sample rate, so the axis must
    // track the device here too.
    refreshSampleRateFromDevice();

    if (!mAPVTS || mLayerIdx < 0 || mUserDragging || mSyncing) return;

    juce::String p = "L" + juce::String(mLayerIdx) + "_";
    bool changed   = false;

    for (int b = 0; b < kNumBands; ++b)
    {
        juce::String bp = p + "eq" + juce::String(b);
        auto load = [&] (const juce::String& id) -> float
        {
            if (auto* ap = mAPVTS->getRawParameterValue(id)) return ap->load();
            return 0.f;
        };
        float freq   = load(bp + "Freq");
        float gain   = load(bp + "Gain");
        float q      = load(bp + "Q");
        int   type   = (int)load(bp + "Type");
        bool  on     = load(bp + "On")    > 0.5f;
        int   slope  = (int)load(bp + "Slope");
        bool  muted  = load(bp + "Mute")  > 0.5f;
        bool  solo   = load(bp + "Solo")  > 0.5f;
        int   chan   = juce::jlimit(0, 4, (int)load(bp + "Channel"));   // 12h

        if (freq  != mBands[b].freq    || gain  != mBands[b].gainDb  ||
            q     != mBands[b].q       || type  != mBands[b].type    ||
            on    != mBands[b].enabled || slope != mBands[b].slope   ||
            muted != mBands[b].muted   || solo  != mBands[b].soloed  ||
            chan  != mBands[b].channel)
        {
            mBands[b] = { freq, gain, q, type, slope, on, muted, solo, chan };
            changed   = true;
        }
    }
    if (changed)
    {
        syncControlsFromBands();
        repaint();
    }
}

void ParametricEQDisplay::setAPVTSFromBand(int b)
{
    auto writeToAPVTS = [&](juce::AudioProcessorValueTreeState* apvts,
                            const juce::String& bp)
    {
        if (!apvts) return;
        auto setF = [&](const juce::String& id, float val)
        {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(apvts->getParameter(id)))
                p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(val));
        };
        setF(bp + "Freq",      mBands[b].freq);
        setF(bp + "Gain",      mBands[b].gainDb);
        setF(bp + "Q",         mBands[b].q);
        setF(bp + "Type",      (float)mBands[b].type);
        setF(bp + "On",        mBands[b].enabled ? 1.f : 0.f);
        setF(bp + "Slope",     (float)mBands[b].slope);
        setF(bp + "Mute",      mBands[b].muted   ? 1.f : 0.f);
        setF(bp + "Solo",      mBands[b].soloed  ? 1.f : 0.f);
        setF(bp + "Channel",   (float)mBands[b].channel);   // 12h / Session B
        // C.4 follow-up (2026-04-30): dynamic EQ params NO LONGER written from
        // here.  setAPVTSFromBand fires from band-drag / wheel / type-change /
        // any static-param change -- writing the UI mBands' dyn defaults back
        // would clobber whatever the popout's slider attachments set.  Dyn
        // params (Dynamic/Threshold/Ratio/Attack/Release/Range/Upward/ScSource)
        // flow exclusively through the popout's SliderAttachment + APVTS ->
        // updateXxxEQ -> DSP -> syncFromDSP -> mBands.  Make-Dynamic toggle
        // (item 600 in the band right-click menu) writes Dynamic + Range
        // explicitly itself, bypassing this function.
    };

    if (mBindMode == BindMode::APVTS && mAPVTS && mLayerIdx >= 0)
    {
        writeToAPVTS(mAPVTS, "L" + juce::String(mLayerIdx) + "_eq" + juce::String(b));
    }
    else if (mBindMode == BindMode::MsDSP && mMsDSPApvts)
    {
        // Write to mid or side APVTS prefix so processBlock updateXxxEQ() doesn't revert
        juce::String prefix = (mShowMid ? mMsDSPMidPrefix : mMsDSPSidePrefix) + juce::String(b);
        writeToAPVTS(mMsDSPApvts, prefix);
    }
}

// Session B / 12j Issue 3: JUCE system tooltip is suppressed (empty string) so
// the custom 3-column in-paint hover panel drawn by drawHoverTooltip() is the
// sole on-hover info surface. Gives full pixel control for columns + graphical
// GR meter as per the 12j Issue 3 spec.
juce::String ParametricEQDisplay::getTooltip()
{
    return {};
}

void ParametricEQDisplay::pushBandToDSP(int b)
{
    auto pushAll = [&](EQ8DSP& target)
    {
        target.setBandFreq    (b, mBands[b].freq);
        target.setBandGain    (b, mBands[b].gainDb);
        target.setBandQ       (b, mBands[b].q);
        target.setBandType    (b, mBands[b].type);
        target.setBandSlope   (b, mBands[b].slope);
        target.setBandOn      (b, mBands[b].enabled);
        target.setBandMuted   (b, mBands[b].muted);
        target.setBandSoloed  (b, mBands[b].soloed);
        target.setBandChannel (b, mBands[b].channel);    // 12h / Session B
        // C.4 follow-up (2026-04-30): dynamic EQ params NO LONGER pushed from
        // here -- mBands' UI defaults would clobber whatever the popout
        // sliders set.  Dyn params live in APVTS (via SliderAttachment) and
        // get pushed to DSP exclusively via updateXxxEQ each block.  See
        // setAPVTSFromBand for the matching skip + rationale.
    };
    if (mBindMode == BindMode::DSP && mBoundDSP)
        pushAll(*mBoundDSP);
    else if (mBindMode == BindMode::MsDSP && mBoundMsDsp)
        pushAll(mShowMid ? mBoundMsDsp->mid() : mBoundMsDsp->side());
}

// ── ParametricEQDisplay - new methods (H3) ────────────────────────────────────

void ParametricEQDisplay::bindAPVTS(juce::AudioProcessorValueTreeState& apvts, int layerIdx)
{
    mAPVTS    = &apvts;
    mLayerIdx = layerIdx;
    mBindMode = BindMode::APVTS;
    syncFromAPVTS();
}

void ParametricEQDisplay::bindDSP(EQ8DSP* dsp)
{
    mBoundDSP  = dsp;
    mBindMode  = dsp ? BindMode::DSP : BindMode::None;
    // 12h: internal pill removed (was mMidSideBtn->setVisible(false) here).
    if (dsp) syncFromDSP();
}

void ParametricEQDisplay::bindMsDSP(EQ8MsDSP* msDsp)
{
    mBoundMsDsp = msDsp;
    mBindMode   = msDsp ? BindMode::MsDSP : BindMode::None;
    // 12h: internal pill deleted - external page MID/SIDE buttons drive mShowMid.
    if (msDsp)
    {
        // Default view when binding a new M/S DSP is MID. The external page
        // buttons can flip this afterwards via setShowMid().
        mShowMid = true;
        syncFromDSP();
    }
    // 12i: reset spectrum accumulators so a stale pre-spectrum from a previous
    // binding doesn't bleed into the new one until the first real frame arrives.
    mSpectrumReady    = false;
    mSpectrumPreReady = false;
    mFifoIndex        = 0;
    mFifoIndexPre     = 0;
}

void ParametricEQDisplay::bindMsDSP(EQ8MsDSP* msDsp,
                                     juce::AudioProcessorValueTreeState* apvts,
                                     juce::String midPrefix, juce::String sidePrefix)
{
    mMsDSPApvts      = apvts;
    mMsDSPMidPrefix  = midPrefix;
    mMsDSPSidePrefix = sidePrefix;
    bindMsDSP(msDsp);   // delegate to base overload (resets analyser accumulators, etc.)
    // Bonus Q3: stamp paramIds on per-band right-panel controls (freq/gain/Q)
    // so GlobalAutoRightClick can catch right-clicks and offer the Automate menu.
    stampRightPanelComponentIds();
}

// 12j: dynamic EQ params popout. File-scope Component used inside a CallOutBox.
// Small panel (~340x130 px) with 5 sliders + 1 Upward toggle + live GR meter.
// Each slider is componentID-tagged so GlobalAutoRightClick's "Automate..." /
// "Type in value..." menus work. SliderAttachment bridges to APVTS so dragging
// updates both APVTS and (via processBlock's updateXxxEQ) the DSP.
namespace
{
    class DynamicParamsPopout : public juce::Component,
                                private juce::Timer
    {
    public:
        DynamicParamsPopout(juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& paramPrefix,
                            int bandIdx,
                            EQ8DSP* dsp /* for live GR polling */,
                            juce::String stripMixerPrefix = {},
                            std::function<juce::String(int)> resolveSourceName = {},
                            std::function<void(float)> onRangeChanged     = {},
                            std::function<void(float)> onThresholdChanged = {},
                            std::function<void(float)> onRatioChanged     = {},
                            std::function<void(float)> onAttackChanged    = {},
                            std::function<void(float)> onReleaseChanged   = {})
            : mApvts(apvts), mDsp(dsp), mBandIdx(bandIdx),
              mStripMixerPrefix(std::move(stripMixerPrefix)),
              mResolveSourceName(std::move(resolveSourceName)),
              mOnRangeChanged(std::move(onRangeChanged)),
              mOnThresholdChanged(std::move(onThresholdChanged)),
              mOnRatioChanged(std::move(onRatioChanged)),
              mOnAttackChanged(std::move(onAttackChanged)),
              mOnReleaseChanged(std::move(onReleaseChanged))
        {
            auto makeSlider = [&](BaySickSlider& s, const juce::String& name,
                                  float lo, double skewMid, const juce::String& suffix)
            {
                s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
                s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 14);
                s.setName(name);
                s.setComponentID(paramPrefix + juce::String(bandIdx) + suffix);
                s.setTextValueSuffix("");
                juce::ignoreUnused(lo, skewMid);
                addAndMakeVisible(s);
            };
            makeSlider(mThr,  "Threshold", -60.f, 1.0,  "Threshold");
            makeSlider(mRat,  "Ratio",       1.f, 1.0,  "Ratio");
            makeSlider(mAtt,  "Attack",     0.1f, 30.0, "Attack");
            makeSlider(mRel,  "Release",     1.f, 200.0,"Release");
            // 12j follow-up Q2: Range knob is now BIPOLAR. Negative = downward
            // compression amount, positive = upward expansion amount, zero = off.
            // Double-click snaps to 0 for quick off.
            makeSlider(mRng,  "Range",       0.f, 1.0,  "Range");
            mRng.setDoubleClickReturnValue(true, 0.0);
            mRng.setTooltip("Range: - dB = max compression (cut above threshold), "
                            "+ dB = max expansion (boost below threshold), 0 = off");

            // 12j follow-up Q2: Upward toggle removed - direction encoded in Range sign.

            auto attachS = [&](BaySickSlider& s, const juce::String& suffix) {
                return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                           apvts, paramPrefix + juce::String(bandIdx) + suffix, s);
            };
            mAttThr = attachS(mThr, "Threshold");
            mAttRat = attachS(mRat, "Ratio");
            mAttAtt = attachS(mAtt, "Attack");
            mAttRel = attachS(mRel, "Release");
            mAttRng = attachS(mRng, "Range");

            // C.4 follow-up (2026-04-30): direct path so EVERY dyn slider
            // (Threshold, Ratio, Attack, Release, Range) drives parent's
            // mBands + DSP in the same message-thread tick.  Without this
            // the chain is slider -> APVTS -> processBlock's updateXxxEQ ->
            // DSP -> syncFromDSP's timer-driven poll -> mBands; that has
            // multi-tick latency and depends on the host page's timer
            // actually firing syncFromDSP between each tweak, which led to
            // "ratio/attack/release do nothing" + "dotted line snaps back to
            // 0 on slider release" symptoms.  Slider attachment still writes
            // APVTS (for persistence + automation); the onValueChange below
            // mirrors the same value into the live audio path immediately.
            mRng.onValueChange = [this]
            {
                if (mOnRangeChanged) mOnRangeChanged((float) mRng.getValue());
            };
            mThr.onValueChange = [this]
            {
                if (mOnThresholdChanged) mOnThresholdChanged((float) mThr.getValue());
            };
            mRat.onValueChange = [this]
            {
                if (mOnRatioChanged) mOnRatioChanged((float) mRat.getValue());
            };
            mAtt.onValueChange = [this]
            {
                if (mOnAttackChanged) mOnAttackChanged((float) mAtt.getValue());
            };
            mRel.onValueChange = [this]
            {
                if (mOnReleaseChanged) mOnReleaseChanged((float) mRel.getValue());
            };

            // C.4 Phase 1 (2026-04-30): per-band SC source dropdown.  Click
            // pops a menu listing currently-routed SC lines on the strip.
            // Selection writes mDsp->setBandScSource(band, lineIdx); -1 = Off.
            mScBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3b3b3b));
            mScBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd6d6d6));
            mScBtn.setTooltip("Sidechain source for this band");
            mScBtn.onClick = [this] { showScMenu(); };
            addAndMakeVisible(mScBtn);
            refreshScBtnLabel();

            setSize(380, 168);   // +28 px to fit the SC dropdown row below knobs
            startTimerHz(30);

            // 12j follow-up Q4: listen for right-click events on our own children.
            // CallOutBox is a peer top-level window so the app-wide
            // GlobalAutoRightClick installed on StandaloneEditor never sees our
            // sliders. This self-listener duplicates that behaviour locally.
            addMouseListener(this, true);
        }

        ~DynamicParamsPopout() override { stopTimer(); removeMouseListener(this); }

        // 12j follow-up Q4: right-click on any child slider -> Automate menu.
        // Mirrors GlobalAutoRightClick's logic but local to this popout.
        void mouseDown(const juce::MouseEvent& e) override
        {
            if (!e.mods.isRightButtonDown()) return;
            auto* comp = e.eventComponent;
            if (!comp) return;
            const juce::String id = comp->getComponentID();
            if (id.isEmpty()) return;

            juce::Component::SafePointer<juce::Slider> safeSl(
                dynamic_cast<juce::Slider*>(comp));

            juce::String label = id;
            if (VKnobAutomation::sResolveMenuLabel)
            {
                auto resolved = VKnobAutomation::sResolveMenuLabel(id);
                if (resolved.isNotEmpty()) label = resolved;
            }

            juce::PopupMenu m;
            m.addItem(1, "Automate: " + label);
            if (safeSl != nullptr) m.addItem(2, "Type in value...");
            m.showMenuAsync(juce::PopupMenu::Options{}, [id, safeSl](int result)
            {
                if (result == 1 && VKnobAutomation::sOnAutomate)
                    VKnobAutomation::sOnAutomate(id);
                else if (result == 2)
                    if (auto* s = safeSl.getComponent())
                        VKnobAutomation::promptSliderValueEntry(*s, id);
            });
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(VC::Panel);
            g.setColour(VC::Accent);
            g.drawRect(getLocalBounds(), 1);

            // Header label
            g.setColour(VC::Text);
            g.setFont(juce::Font(12.f, juce::Font::bold));
            g.drawText("Dynamic - Band " + juce::String(mBandIdx + 1),
                       4, 2, getWidth() - 80, 16,
                       juce::Justification::centredLeft, false);

            // GR meter on right side.
            const int mx = getWidth() - 70;
            const int my = 4;
            const int mw = 64;
            const int mh = getHeight() - 8;
            g.setColour(juce::Colour(0xff141414));
            g.fillRect(mx, my, mw, mh);
            g.setColour(VC::Accent);
            g.drawRect(mx, my, mw, mh, 1);
            // Fill bar: 0 dB centreline; downward = below, upward = above.
            const float grDb = mLiveGrDb;
            const float maxRange = 24.f;
            const float clampedGr = juce::jlimit(-maxRange, maxRange, grDb);
            const int centreY = my + mh / 2;
            if (clampedGr < 0.f)
            {
                const int barH = (int)((float)(mh / 2) * (-clampedGr / maxRange));
                g.setColour(juce::Colour(0xffff7f33).withAlpha(0.85f));
                g.fillRect(mx + 2, centreY, mw - 4, barH);
            }
            else if (clampedGr > 0.f)
            {
                const int barH = (int)((float)(mh / 2) * (clampedGr / maxRange));
                g.setColour(juce::Colour(0xff33ffaa).withAlpha(0.85f));
                g.fillRect(mx + 2, centreY - barH, mw - 4, barH);
            }
            // Centreline + readout.
            g.setColour(juce::Colour(0xff888888));
            g.drawLine((float)(mx + 1), (float)centreY, (float)(mx + mw - 1), (float)centreY, 0.6f);
            g.setColour(VC::Text);
            g.setFont(juce::Font(10.f, juce::Font::bold));
            juce::String txt = (std::abs(grDb) < 0.05f) ? "0.0"
                             : (grDb > 0.f ? ("+" + juce::String(grDb, 1))
                                           :       juce::String(grDb, 1));
            g.drawText(txt + " dB", mx, my + mh - 14, mw, 12,
                       juce::Justification::centred, false);

            // Knob labels
            g.setFont(juce::Font(9.f));
            g.setColour(VC::TextDim);
            auto drawLbl = [&](juce::Slider& s, const juce::String& name)
            {
                auto b = s.getBounds();
                g.drawText(name, b.getX(), b.getY() - 11, b.getWidth(), 10,
                           juce::Justification::centred, false);
            };
            drawLbl(mThr, "Thr");
            drawLbl(mRat, "Ratio");
            drawLbl(mAtt, "Atk");
            drawLbl(mRel, "Rel");
            drawLbl(mRng, "Range");
        }

        void resized() override
        {
            const int pad = 6;
            const int knobW = 54;
            const int knobH = 80;
            const int row0y = 24;
            int x = pad;
            mThr.setBounds(x, row0y, knobW, knobH); x += knobW + pad;
            mRat.setBounds(x, row0y, knobW, knobH); x += knobW + pad;
            mAtt.setBounds(x, row0y, knobW, knobH); x += knobW + pad;
            mRel.setBounds(x, row0y, knobW, knobH); x += knobW + pad;
            mRng.setBounds(x, row0y, knobW, knobH); x += knobW + pad;
            // 12j follow-up Q2: Upward toggle removed - Range is bipolar now.

            // C.4 Phase 1: SC source dropdown row beneath the knobs.
            const int scY = row0y + knobH + 8;
            mScBtn.setBounds(pad, scY, getWidth() - pad - 80, 24);
        }

        void showScMenu()
        {
            juce::PopupMenu m;
            const int currentPick = (mDsp != nullptr)
                ? mDsp->getBand(mBandIdx).scSourceId : -1;
            m.addItem(1, "Off", true, currentPick < 0);

            bool anyActive = false;
            if (mStripMixerPrefix.isNotEmpty())
            {
                for (int s = 0; s < 4; ++s)
                {
                    const juce::String pid = mStripMixerPrefix
                        + "_sc_recv" + juce::String(s) + "_from";
                    if (auto* p = mApvts.getRawParameterValue(pid))
                    {
                        const int srcId = (int) p->load();
                        if (srcId < 0) continue;
                        anyActive = true;
                        juce::String srcName = mResolveSourceName ? mResolveSourceName(srcId)
                                                                  : juce::String("Ch ") + juce::String(srcId);
                        if (srcName.isEmpty()) srcName = juce::String("Ch ") + juce::String(srcId);
                        m.addItem(10 + s, srcName, true, currentPick == s);
                    }
                }
            }
            if (! anyActive)
            {
                m.addSeparator();
                m.addItem(99, "(no sidechain cables routed to this strip)", false, false);
            }

            m.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(&mScBtn),
                [this](int r)
                {
                    if (r <= 0 || !mDsp) return;
                    const int pick = (r == 1) ? -1 : (r - 10);
                    mDsp->setBandScSource(mBandIdx, pick);
                    refreshScBtnLabel();
                });
        }

        void refreshScBtnLabel()
        {
            juce::String label = "Sidechain Source: Off";
            if (mDsp != nullptr)
            {
                const int pick = mDsp->getBand(mBandIdx).scSourceId;
                if (pick >= 0 && pick < 4 && mStripMixerPrefix.isNotEmpty())
                {
                    const juce::String pid = mStripMixerPrefix
                        + "_sc_recv" + juce::String(pick) + "_from";
                    if (auto* p = mApvts.getRawParameterValue(pid))
                    {
                        const int srcId = (int) p->load();
                        if (srcId >= 0)
                        {
                            juce::String name = mResolveSourceName ? mResolveSourceName(srcId)
                                                                   : juce::String("Ch ") + juce::String(srcId);
                            if (name.isEmpty()) name = juce::String("Ch ") + juce::String(srcId);
                            label = "Sidechain Source: " + name;
                        }
                    }
                }
            }
            mScBtn.setButtonText(label);
        }

    private:
        void timerCallback() override
        {
            // LIVENESS, not null-ness (QA-Layout T13, 2026-08-05).  This popout
            // holds a raw EQ8DSP* and polls it; a project load rebuilds the
            // graph and destroys that DSP while the callout is still on screen.
            // Same defect class that crashed LimiterPanel -- a null check never
            // catches a DESTROYED object.  The owner's resolver answers whether
            // ours is still the live one.
            if (mResolveDsp && mResolveDsp() != mDsp) return;
            if (!mDsp) return;
            const float gr = mDsp->getBandGrDb(mBandIdx);
            if (std::abs(gr - mLiveGrDb) > 0.05f)
            {
                mLiveGrDb = gr;
                repaint();
            }
        }

    public:
        // Set by the owner after construction: returns the EQ8DSP the model
        // currently vouches for, so the poll above can tell "still live" from
        // "destroyed under us".
        std::function<EQ8DSP*()> mResolveDsp;

    private:
        juce::AudioProcessorValueTreeState& mApvts;
        EQ8DSP* mDsp;
        int     mBandIdx;
        float   mLiveGrDb { 0.f };

        BaySickSlider mThr, mRat, mAtt, mRel, mRng;
        // 12j follow-up Q2: Upward toggle removed - Range is bipolar.

        // C.4 Phase 1: SC source dropdown + strip context.
        juce::TextButton                          mScBtn;
        juce::String                              mStripMixerPrefix;
        std::function<juce::String(int)>          mResolveSourceName;
        // C.4 follow-up (2026-04-30): direct dyn-param -> parent mBands+DSP paths.
        std::function<void(float)>                mOnRangeChanged;
        std::function<void(float)>                mOnThresholdChanged;
        std::function<void(float)>                mOnRatioChanged;
        std::function<void(float)>                mOnAttackChanged;
        std::function<void(float)>                mOnReleaseChanged;

        using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
        std::unique_ptr<SA> mAttThr, mAttRat, mAttAtt, mAttRel, mAttRng;
    };
} // namespace

void ParametricEQDisplay::setStripContext (juce::String mixerPrefix,
                                            std::function<juce::String(int)> resolveSourceName)
{
    mStripMixerPrefix  = std::move(mixerPrefix);
    mResolveSourceName = std::move(resolveSourceName);
}

void ParametricEQDisplay::openDynamicParamsPopout(int bandIdx)
{
    if (bandIdx < 0 || bandIdx >= kNumBands) return;
    if (mBindMode != BindMode::MsDSP || !mBoundMsDsp || !mMsDSPApvts) return;

    // Pick the inner EQ (mid or side) matching the current view so GR readings
    // reflect the side the user is editing.
    EQ8DSP* dsp = mShowMid ? &mBoundMsDsp->mid() : &mBoundMsDsp->side();
    const juce::String prefix = mShowMid ? mMsDSPMidPrefix : mMsDSPSidePrefix;

    // C.4 Phase 1 (2026-04-30): hand the popout the strip's SC enumeration
    // context so its per-band SC dropdown can list routed source lines.
    // C.4 follow-up: also direct dyn-param -> mBands + DSP callbacks for all
    // five sliders.  Without these the slider attachment writes only APVTS,
    // and the chain APVTS -> processBlock::updateXxxEQ -> DSP -> timer-driven
    // syncFromDSP -> mBands has multi-tick latency and is gated on host-page
    // timer cadence; that caused the "ratio/attack/release do nothing" and
    // "dotted line snaps back on slider release" symptoms.
    juce::Component::SafePointer<ParametricEQDisplay> safeSelf(this);

    auto pushDspBoth = [safeSelf, bandIdx](void (EQ8DSP::*fn)(int, float), float v)
    {
        if (auto* self = safeSelf.getComponent())
        {
            if (self->mBindMode == BindMode::MsDSP && self->mBoundMsDsp)
            {
                (self->mBoundMsDsp->mid()  .*fn)(bandIdx, v);
                (self->mBoundMsDsp->side() .*fn)(bandIdx, v);
            }
            else if (self->mBindMode == BindMode::DSP && self->mBoundDSP)
            {
                (self->mBoundDSP->*fn)(bandIdx, v);
            }
        }
    };
    auto onRangeChanged = [safeSelf, bandIdx, pushDspBoth](float v)
    {
        if (auto* self = safeSelf.getComponent())
        {
            if (bandIdx >= 0 && bandIdx < kNumBands)
            {
                self->mBands[bandIdx].rangeDb = v;
                pushDspBoth(&EQ8DSP::setBandRange, v);
                self->repaint();
            }
        }
    };
    auto onThresholdChanged = [safeSelf, bandIdx, pushDspBoth](float v)
    {
        if (auto* self = safeSelf.getComponent())
        {
            if (bandIdx >= 0 && bandIdx < kNumBands)
            {
                self->mBands[bandIdx].threshold = v;
                pushDspBoth(&EQ8DSP::setBandThreshold, v);
            }
        }
    };
    auto onRatioChanged = [safeSelf, bandIdx, pushDspBoth](float v)
    {
        if (auto* self = safeSelf.getComponent())
        {
            if (bandIdx >= 0 && bandIdx < kNumBands)
            {
                self->mBands[bandIdx].ratio = v;
                pushDspBoth(&EQ8DSP::setBandRatio, v);
            }
        }
    };
    auto onAttackChanged = [safeSelf, bandIdx, pushDspBoth](float v)
    {
        if (auto* self = safeSelf.getComponent())
        {
            if (bandIdx >= 0 && bandIdx < kNumBands)
            {
                self->mBands[bandIdx].attack = v;
                pushDspBoth(&EQ8DSP::setBandAttack, v);
            }
        }
    };
    auto onReleaseChanged = [safeSelf, bandIdx, pushDspBoth](float v)
    {
        if (auto* self = safeSelf.getComponent())
        {
            if (bandIdx >= 0 && bandIdx < kNumBands)
            {
                self->mBands[bandIdx].release = v;
                pushDspBoth(&EQ8DSP::setBandRelease, v);
            }
        }
    };
    auto content = std::make_unique<DynamicParamsPopout>(*mMsDSPApvts, prefix, bandIdx, dsp,
                                                          mStripMixerPrefix, mResolveSourceName,
                                                          std::move(onRangeChanged),
                                                          std::move(onThresholdChanged),
                                                          std::move(onRatioChanged),
                                                          std::move(onAttackChanged),
                                                          std::move(onReleaseChanged));
    // The popout outlives nothing, but a project load CAN rebuild the graph
    // while it is open -- so it asks the display for the live DSP rather than
    // trusting the pointer it was handed at construction.
    content->mResolveDsp = [safeSelf]() -> EQ8DSP*
    {
        auto* self = safeSelf.getComponent();
        return self != nullptr ? self->mBoundDSP : nullptr;
    };
    // Anchor the CallOutBox to the band handle's screen rect so the arrow points
    // at the band it's editing.
    const float hx = freqToX(mBands[bandIdx].freq);
    const float hy = gainToY(mBands[bandIdx].gainDb);
    const auto anchor = localAreaToGlobal(juce::Rectangle<int>((int)hx - 6, (int)hy - 6, 12, 12));
    juce::CallOutBox::launchAsynchronously(std::move(content), anchor, nullptr);
}

void ParametricEQDisplay::setShowMid(bool showMid)
{
    // 12h: called by external page-header MID/SIDE buttons. Flips which inner
    // EQ8DSP (mMid / mSide) the display is editing. Internal pill is gone.
    mShowMid = showMid;
    // Bonus Q3: re-stamp right-panel paramIds so automation hits the now-viewed
    // side's prefix (mMsDSPMidPrefix vs mMsDSPSidePrefix).
    stampRightPanelComponentIds();
    syncFromDSP();
    repaint();
}

void ParametricEQDisplay::refreshSampleRateFromDevice()
{
    const double live = getLiveSampleRate();
    if (live > 0.0 && std::abs (mSampleRateForFFT - live) > 1.0e-9)
    {
        setSampleRate (live);
        repaint();   // every bin's frequency AND the whole drawn curve moved
    }
}

void ParametricEQDisplay::syncFromDSP()
{
    // The axis has to be right before the frame that arrives below is mapped
    // onto it, so this leads the poll.
    refreshSampleRateFromDevice();

    // 12i: feed poll is ORDER-INDEPENDENT of the band-value sync below and must
    // run even during drag / mid-sync, or the analyser freezes while the user is
    // holding a handle. Kept above the mSyncing / mUserDragging guard.
    if (mBindMode == BindMode::MsDSP && mBoundMsDsp)
    {
        int got = 0;
        if (mBoundMsDsp->preFeed .poll(mFeedPollBuf, got) && got > 0)
            pushSamplesPre(mFeedPollBuf, got);
        if (mBoundMsDsp->postFeed.poll(mFeedPollBuf, got) && got > 0)
            pushSamples   (mFeedPollBuf, got);
    }

    // 12g: keep mPhaseMode in sync with the bound DSP so the popup checkmark
    // is correct after preset load / external mode change. Cheap (one int read).
    //
    // Same treatment for the A/B bank: the DSP owns which bank is live (swap
    // goes through swapWithSpare) and now restores it from saved state, so the
    // pill and the compare-menu direction must be read back or they invert
    // after a project load or a window reopen. APVTS-only mode keeps its own
    // flag - there is no DSP to ask - so it is deliberately not touched here.
    bool dspViewingSpare = mViewingSpare;
    if (mBindMode == BindMode::DSP && mBoundDSP)
    {
        mPhaseMode      = (int) mBoundDSP->getPhaseMode();
        dspViewingSpare = mBoundDSP->isViewingSpare();
    }
    else if (mBindMode == BindMode::MsDSP && mBoundMsDsp)
    {
        mPhaseMode      = (int) mBoundMsDsp->getPhaseMode();
        dspViewingSpare = mBoundMsDsp->mid().isViewingSpare();
    }

    if (dspViewingSpare != mViewingSpare)
    {
        mViewingSpare = dspViewingSpare;
        refreshBankIndicator();
    }

    // D.4-Q6: sync the main-level fader from DSP without firing onValueChange
    // (avoid the DSP->UI->DSP loop).  Only when not actively dragging the
    // fader so user input wins.
    if (mMainLevelFader != nullptr && ! mMainLevelFader->isMouseButtonDown())
    {
        float dspMainDb = 0.0f;
        if (mBindMode == BindMode::DSP && mBoundDSP)
            dspMainDb = mBoundDSP->getMainLevel();
        else if (mBindMode == BindMode::MsDSP && mBoundMsDsp)
            dspMainDb = mBoundMsDsp->mid().getMainLevel();
        const float curUi = (float) mMainLevelFader->getValue();
        if (std::abs (dspMainDb - curUi) > 0.01f)
            mMainLevelFader->setValue (dspMainDb, juce::dontSendNotification);
    }

    // Band-value sync: SKIP during drag / nested sync so the DSP->UI poll doesn't
    // fight the user's drag input, and so we don't recurse from pushBandToDSP.
    if (mSyncing || mUserDragging) return;

    // 12h: SKIP band-sync when the widget is bound to an MsDSP without an APVTS
    // write-back (EffectsPage's simple bind path). The Layers / Bass / Drums bus
    // APVTS params are authoritative for their EQs - processBlock's updateXxxEQ
    // constantly reads them back onto the DSP. If our pushBandToDSP writes to
    // DSP without also writing to APVTS, the next processBlock resets the DSP to
    // the APVTS default, and pulling that back into mBands would snap the slider
    // to its default every few ms. Without the APVTS bridge, mBands remains the
    // user-authoritative source and must not be overwritten from the DSP side.
    if (mBindMode == BindMode::MsDSP && mMsDSPApvts == nullptr) return;

    EQ8DSP* src = nullptr;
    if (mBindMode == BindMode::DSP)
        src = mBoundDSP;
    else if (mBindMode == BindMode::MsDSP && mBoundMsDsp)
        src = mShowMid ? &mBoundMsDsp->mid() : &mBoundMsDsp->side();

    if (!src) return;

    bool changed = false;
    for (int b = 0; b < kNumBands; ++b)
    {
        EQ8DSP::Band eb = src->getBand(b);
        Band& mb = mBands[b];
        if (eb.freq != mb.freq || eb.gainDb != mb.gainDb || eb.q != mb.q ||
            eb.type != mb.type || eb.slope  != mb.slope  || eb.on != mb.enabled ||
            eb.muted != mb.muted || eb.soloed != mb.soloed ||
            eb.channel != mb.channel ||
            eb.dynamic != mb.dynamic || eb.threshold != mb.threshold ||
            eb.ratio != mb.ratio || eb.attack != mb.attack ||
            eb.release != mb.release || eb.rangeDb != mb.rangeDb ||
            eb.upward != mb.upward || eb.scSourceId != mb.scSourceId)
        {
            mb.freq      = eb.freq;
            mb.gainDb    = eb.gainDb;
            mb.q         = eb.q;
            mb.type      = eb.type;
            mb.slope     = eb.slope;
            mb.enabled   = eb.on;
            mb.muted     = eb.muted;
            mb.soloed    = eb.soloed;
            mb.channel   = eb.channel;      // 12h / Session B
            mb.dynamic   = eb.dynamic;      // 12j
            mb.threshold = eb.threshold;
            mb.ratio     = eb.ratio;
            mb.attack    = eb.attack;
            mb.release   = eb.release;
            mb.rangeDb   = eb.rangeDb;
            mb.upward    = eb.upward;
            mb.scSourceId= eb.scSourceId;
            changed = true;
        }
        // 12j: live GR readout - polled every tick regardless of param changes.
        mb.currentGrDb = src->getBandGrDb(b);
    }
    if (changed) { syncControlsFromBands(); }
    // Always repaint so the animated curve + GR meter track the live envelope.
    repaint();
}

// 2026-04-19: A/B compare reworked per Jeff's option A. Pure swap, no auto-
// save - the original auto-save-then-swap pattern destroyed the user's B bank
// every other compare flip. Spare is only seeded by the explicit "Copy A -> B"
// menu item (triggerCopyAToB below). On a fresh EQ the first compare flip will
// reveal the factory-default flat spare; users explicitly Copy A -> B once
// they've dialled in their A and want a starting point for B.
//
// 2026-04-19 followup: APVTS write-back fix. APVTS holds a single set of params
// (no spare-bank concept). For full-bindMsDSP EQs (all bus EQs etc), processBlock
// constantly reads APVTS -> DSP via updateXxxEQ. Without rewriting APVTS to
// reflect the just-swapped-in DSP state, the next processBlock would overwrite
// the DSP's new bank with the OLD bank's APVTS values - exactly the "swap shows
// briefly then rewrites" symptom. Fix: after every swap, push every band's
// new DSP-side value back into APVTS so APVTS = currently-viewed bank.
// Subsequent edits modify APVTS -> DSP normally; subsequent swap re-syncs.
void ParametricEQDisplay::triggerCompare()
{
    if (mBindMode == BindMode::DSP && mBoundDSP)
    {
        mBoundDSP->swapWithSpare();
        mViewingSpare = mBoundDSP->isViewingSpare();
        syncFromDSP();
    }
    else if (mBindMode == BindMode::MsDSP && mBoundMsDsp)
    {
        // Both inner EQs swap together (same bank semantics for mid + side).
        mBoundMsDsp->mid() .swapWithSpare();
        mBoundMsDsp->side().swapWithSpare();
        mViewingSpare = mBoundMsDsp->mid().isViewingSpare();
        // Pull the new DSP state into mBands so the widget reflects the swap.
        syncFromDSP();
        // Push the new (just-swapped-in) state of BOTH inner EQs back into
        // BOTH APVTS prefixes. setAPVTSFromBand only writes the side currently
        // being VIEWED (mid OR side based on mShowMid), so the other side
        // would stay stale and get clobbered by processBlock's next pass.
        // Read each inner EQ's bands directly + push to its prefix.
        if (mMsDSPApvts && ! mMsDSPMidPrefix.isEmpty()
                       && ! mMsDSPSidePrefix.isEmpty())
        {
            beginParamUndoGesture (*mMsDSPApvts, mMsDSPMidPrefix + "0Freq"); // Task 6 (12-iv)
            pushInnerDSPBandsToAPVTS (mBoundMsDsp->mid (), mMsDSPMidPrefix);
            pushInnerDSPBandsToAPVTS (mBoundMsDsp->side(), mMsDSPSidePrefix);
        }
    }
    else
    {
        // APVTS-only mode (no DSP attached): swap mBands <-> mSpareBands
        // locally. mHasSpare just tracks whether mSpareBands has been
        // initialised; first swap clones the defaults so the swap is
        // well-defined.
        if (!mHasSpare)
        {
            mSpareBands = mBands;
            mHasSpare   = true;
        }
        std::swap(mBands, mSpareBands);
        mViewingSpare = !mViewingSpare;
        syncControlsFromBands();
        // Same APVTS-write-back guard as above so processBlock can't
        // overwrite the swap.
        if (mMsDSPApvts && ! mMsDSPMidPrefix.isEmpty())
        {
            beginParamUndoGesture (*mMsDSPApvts, mMsDSPMidPrefix + "0Freq"); // Task 6 (12-iv)
            for (int i = 0; i < kNumBands; ++i)
                setAPVTSFromBand(i);
        }
        repaint();
    }
    refreshBankIndicator();
}

// 2026-04-19: explicit "Copy A -> B" action. Copies the currently-VIEWED bank
// (whether main or spare) into the OTHER bank so a future compare flip starts
// from this state. Honors the lock - if Lock bands is on, both banks are
// frozen and copy is blocked.
void ParametricEQDisplay::triggerCopyAToB()
{
    if (mSpareLocked_btn) return;   // both banks frozen for A/B comparison
    if (mBindMode == BindMode::DSP && mBoundDSP)
    {
        mBoundDSP->saveToSpare();   // copies current main -> spare
        // No swap - we just seeded the spare to match. View remains where it was.
    }
    else
    {
        mSpareBands = mBands;
        mHasSpare   = true;
    }
    refreshBankIndicator();
}

// 2026-04-19: Lock now means "freeze BOTH banks for A/B comparison" - widget
// refuses user edits (drags, slider tweaks, right-click commands that would
// change band state) and the spare bank is also write-protected. Compare swap
// still works so the user can flip A <-> B freely while locked. APVTS-driven
// changes (preset load, automation playback) bypass the lock - it's a UI
// gesture, not a DSP bypass.
void ParametricEQDisplay::triggerLock()
{
    mSpareLocked_btn = !mSpareLocked_btn;
    mSpareLocked     = mSpareLocked_btn;
    if (mBindMode == BindMode::DSP && mBoundDSP)
        mBoundDSP->lockSpare(mSpareLocked);

    // Grey per-band controls so the user sees what's blocked. Drag handlers
    // in mouseDown/mouseDrag check mSpareLocked_btn before mutating state.
    for (auto& c : mControls)
    {
        if (c.gainFader) c.gainFader->setEnabled(! mSpareLocked_btn);
        if (c.freqKnob)  c.freqKnob ->setEnabled(! mSpareLocked_btn);
        if (c.qKnob)     c.qKnob    ->setEnabled(! mSpareLocked_btn);
        if (c.typeCombo) c.typeCombo->setEnabled(! mSpareLocked_btn);
    }
    repaint();
}

// 2026-04-19: A/B compare APVTS sync helper. Reads each band's current
// DSP-side state and writes it back to the corresponding APVTS prefix so
// processBlock's next pass doesn't overwrite the swap.
void ParametricEQDisplay::pushInnerDSPBandsToAPVTS (EQ8DSP& innerEq,
                                                     const juce::String& apvtsPrefix)
{
    if (! mMsDSPApvts) return;
    auto setF = [this] (const juce::String& id, float val)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(mMsDSPApvts->getParameter (id)))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (val));
    };
    for (int i = 0; i < kNumBands; ++i)
    {
        auto b = innerEq.getBand (i);
        const juce::String bp = apvtsPrefix + juce::String (i);
        setF (bp + "Freq",      b.freq);
        setF (bp + "Gain",      b.gainDb);
        setF (bp + "Q",         b.q);
        setF (bp + "Type",      (float) b.type);
        setF (bp + "On",        b.on     ? 1.f : 0.f);
        setF (bp + "Slope",     (float) b.slope);
        setF (bp + "Mute",      b.muted  ? 1.f : 0.f);
        setF (bp + "Solo",      b.soloed ? 1.f : 0.f);
        setF (bp + "Channel",   (float) b.channel);
        setF (bp + "Dynamic",   b.dynamic ? 1.f : 0.f);
        setF (bp + "Threshold", b.threshold);
        setF (bp + "Ratio",     b.ratio);
        setF (bp + "Attack",    b.attack);
        setF (bp + "Release",   b.release);
        setF (bp + "Range",     b.rangeDb);
        setF (bp + "Upward",    b.upward  ? 1.f : 0.f);
        setF (bp + "ScSource",  (float) b.scSourceId);
    }
}

// 2026-04-19: BankIndicator paint + click. Green pill for A, red for B.
// Click swaps banks (alias for triggerCompare). Lives in PageMenuBar's
// extra-right slot via owner->getBankIndicator() injection.
void ParametricEQDisplay::BankIndicator::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(2.f, 3.f);
    const bool isB = mViewingSpare;
    const juce::Colour bg = isB ? juce::Colour(0xffcc2222) : juce::Colour(0xff229944);
    g.setColour(bg.withAlpha(0.85f));
    g.fillRoundedRectangle(b, 3.f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(11.f, juce::Font::bold));
    g.drawText(isB ? juce::String("B Bank") : juce::String("A Bank"), b.toNearestInt(),
               juce::Justification::centred);
    // Subtle border for legibility on dark bars
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.drawRoundedRectangle(b, 3.f, 1.f);
}

void ParametricEQDisplay::BankIndicator::mouseUp(const juce::MouseEvent&)
{
    mOwner.triggerCompare();
}

juce::Component* ParametricEQDisplay::getBankIndicator()
{
    return mBankIndicator.get();
}

void ParametricEQDisplay::refreshBankIndicator()
{
    if (! mBankIndicator) return;
    if (mBankIndicator->mViewingSpare != mViewingSpare)
    {
        mBankIndicator->mViewingSpare = mViewingSpare;
        mBankIndicator->repaint();
    }
}

// ── EQ options menu ───────────────────────────────────────────────────────────

// 2026-04-19: install/uninstall to PageMenuBar's universal hamburger. Pages
// call install on EQ-tab activation and uninstall on switching to other tabs.
void ParametricEQDisplay::installPageMenu(PageMenuBar& bar)
{
    bar.setMenuBuilder ([this] (juce::Component* anchor)
    {
        showEQOptionsMenu (anchor);
    });
}

void ParametricEQDisplay::uninstallPageMenu(PageMenuBar& bar)
{
    bar.setMenuBuilder (nullptr);
}

void ParametricEQDisplay::showEQOptionsMenu(juce::Component* anchor)
{
    if (anchor == nullptr) anchor = mOptionsBtn.get();
    juce::PopupMenu menu;

    // Reset
    menu.addItem(5, "Reset All Bands to Default", true, false);
    menu.addSeparator();

    // 2026-04-19: A/B compare reworked. Item 1 = swap. Item 7 = explicit
    // Copy A -> B (seeds the spare from current bank). Item 2 = lock both
    // banks for safe A/B comparison. Compare label flips to show what the
    // click WILL DO (swap to the other bank), not what bank you're on
    // (the bank indicator in the page menu bar shows that).
    const juce::String compareLabel = mViewingSpare ? juce::String("A/B Compare  (swap to A bank)")
                                                    : juce::String("A/B Compare  (swap to B bank)");
    menu.addItem(1, compareLabel,                          true, false);
    menu.addItem(7, "Copy A -> B  (seed spare bank)",      ! mSpareLocked_btn, false);
    menu.addItem(2, "Lock both banks (freeze A and B)",    true, mSpareLocked_btn);
    menu.addSeparator();

    // Spectrum overlays
    menu.addItem(3, "Heatmap overlay (frequency heat over time)", true, mHeatmapEnabled);
    menu.addItem(4, "Phase curve overlay (phase shift vs freq)",  true, mShowPhase);
    menu.addSeparator();

    // 12f: anti-cramping (2x oversampling). Only meaningful when a DSP is
    // attached - APVTS-only mode (no live DSP) hides the item.
    bool acAvailable = false;
    bool acOn        = false;
    if (mBindMode == BindMode::DSP && mBoundDSP)
    {
        acAvailable = true;
        acOn        = mBoundDSP->isAntiCramping();
    }
    else if (mBindMode == BindMode::MsDSP && mBoundMsDsp)
    {
        acAvailable = true;
        acOn        = mBoundMsDsp->isAntiCramping();
    }
    if (acAvailable)
    {
        menu.addItem(6, "Anti-cramping (2x OS)", true, acOn);
        menu.addSeparator();
    }

    // Processing mode sub-menu (radio-style). 12g: items now act on the bound
    // DSP and append a live latency readout (T2e) so the user sees the cost
    // of each mode at the current sample rate before picking. Per-mode FFT
    // latency contribution + AC's ~5 samples are summed in the label.
    static const char* kModeLabels[] = {
        "Standard (minimum-phase)",
        "Linear Phase (zero phase shift, more CPU)",
        "HQ+ (oversampled)",
        "HQ Linear (oversampled + linear-phase)",
        "HQ Extended (low-latency linear)"
    };

    // D.4-Q6 (2026-05-01): expose three previously-hidden EQ8 settings in this
    // menu (Jeff confirmed these are options, not toolbar knobs).  All three
    // apply only when a live DSP is bound (DSP mode or MsDSP mode); APVTS-only
    // mode hides them.  Resolved up here because the per-mode latency readout
    // below needs the bound DSP's Linear Phase Precision.
    EQ8DSP* directDsp = (mBindMode == BindMode::DSP) ? mBoundDSP : nullptr;
    EQ8MsDSP* msDsp   = (mBindMode == BindMode::MsDSP) ? mBoundMsDsp : nullptr;

    // APVTS-only mode has no DSP to ask, so it falls back to the default
    // precision - which is the FFT size spec 12g fixed Linear at anyway.
    const int curPrec = directDsp ? directDsp->getLinearPhasePrecision()
                      : msDsp     ? msDsp->mid().getLinearPhasePrecision()
                                  : EQ8DSP::kDefaultLinearPrec;

    // Latency contribution per mode. Mirrors EQ8DSP::linearFftSize +
    // EQ8DSP::getLatencySamples (Mid + Side serial in MsDSP).
    auto modeLatencySamples = [this, curPrec] (int m) -> int
    {
        const int linLat = EQ8DSP::linearFftSize ((EQ8DSP::PhaseMode) m, curPrec) / 2;
        const bool acOn  = (m == 2 || m == 3);
        const int acLat  = acOn ? 8 : 0;   // approx IIR halfband 1-stage
        const int perInner = linLat + acLat;
        // M/S wrapper runs Mid then Side -> 2x the per-instance latency.
        if (mBindMode == BindMode::MsDSP) return 2 * perInner;
        return perInner;
    };

    juce::PopupMenu modeMenu;
    for (int m = 0; m < 5; ++m)
    {
        const int sm = modeLatencySamples (m);
        juce::String label = kModeLabels[m];
        if (sm > 0)
            label += "  [+" + juce::String (sm) + " sp]";
        modeMenu.addItem(10 + m, label, true, mPhaseMode == m);
    }
    menu.addSubMenu("Processing Mode", modeMenu);

    if (directDsp != nullptr || msDsp != nullptr)
    {
        menu.addSeparator();

        // Linear Phase Precision: 5-position radio picking the FFT size for the
        // plain Linear Phase mode.  HQ Linear and HQ Extended keep their own
        // spec'd sizes, so the submenu name says which mode this governs rather
        // than offering a setting that silently does nothing in three of five
        // modes.
        juce::PopupMenu precMenu;
        static const char* kPrecLabels[] = { "256 (low CPU)", "512", "1024", "2048 (default)", "4096 (high)" };
        for (int p = 0; p < EQ8DSP::kNumLinearPrecisions; ++p)
            precMenu.addItem (20 + p, kPrecLabels[p], true, curPrec == p);
        menu.addSubMenu ("Linear Phase Precision (Linear Phase mode)", precMenu);

        // IIR Mod Speed: 5-position radio (smoother ramp time).
        const float curSpeed = directDsp ? directDsp->getIIRModSpeed()
                                         : msDsp->mid().getIIRModSpeed();
        auto speedIdx = [] (float v) -> int {
            if (v < 0.125f) return 0;
            if (v < 0.375f) return 1;
            if (v < 0.625f) return 2;
            if (v < 0.875f) return 3;
            return 4;
        };
        const int curSpeedIdx = speedIdx (curSpeed);
        juce::PopupMenu speedMenu;
        static const char* kSpeedLabels[] = { "Instant (~1 ms)", "Fast", "Medium", "Slow", "Slowest (~50 ms)" };
        for (int s = 0; s < 5; ++s)
            speedMenu.addItem (25 + s, kSpeedLabels[s], true, curSpeedIdx == s);
        menu.addSubMenu ("IIR Mod Speed", speedMenu);

        // Proportional Q toggle.
        const bool propQ = directDsp ? directDsp->getProportionalQ()
                                     : msDsp->mid().getProportionalQ();
        menu.addItem (30, "Proportional Q (analog console feel)", true, propQ);
    }

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(anchor),
        [this](int result)
        {
            switch (result)
            {
                case 1:  triggerCompare();                          break;
                case 2:  triggerLock();                             break;
                case 7:  triggerCopyAToB();                         break;
                case 3:  mHeatmapEnabled = !mHeatmapEnabled; repaint(); break;
                case 4:  mShowPhase      = !mShowPhase;      repaint(); break;
                case 6:
                {
                    // 12f: toggle anti-cramping on the bound DSP, then refresh
                    // host PDC so the new latency reaches the host.
                    if (mBindMode == BindMode::DSP && mBoundDSP)
                        mBoundDSP->setAntiCramping(! mBoundDSP->isAntiCramping());
                    else if (mBindMode == BindMode::MsDSP && mBoundMsDsp)
                        mBoundMsDsp->setAntiCramping(! mBoundMsDsp->isAntiCramping());
                    if (onLatencyChanged) onLatencyChanged();
                    repaint();
                    break;
                }
                case 5:
                    // Reset all bands to default values
                    beginParamUndoGesture(mBindMode == BindMode::MsDSP && mMsDSPApvts ? mMsDSPApvts->undoManager
                                              : mBindMode == BindMode::APVTS && mAPVTS ? mAPVTS->undoManager : nullptr,
                                          (mBindMode == BindMode::MsDSP ? (mShowMid ? mMsDSPMidPrefix : mMsDSPSidePrefix)
                                                                        : "L" + juce::String(mLayerIdx) + "_eq")
                                              + juce::String(0) + "Freq"); // Task 6 (12-iv)
                    for (int b = 0; b < kNumBands; ++b)
                    {
                        mBands[b].freq    = kEQDefaultFreqs[b];
                        mBands[b].gainDb  = 0.f;
                        mBands[b].q       = 0.707f;
                        mBands[b].type    = 0;
                        mBands[b].slope   = 0;
                        mBands[b].muted   = false;
                        mBands[b].soloed  = false;
                        mBands[b].enabled = true;
                        setAPVTSFromBand(b);
                        pushBandToDSP(b);
                    }
                    syncControlsFromBands();
                    repaint();
                    break;
                default:
                    if (result >= 10 && result < 15)
                    {
                        mPhaseMode = result - 10;
                        // 12g: push to bound DSP + refresh PDC. PRESET-SAFE
                        // (phaseMode now serialised inside EQ8DSP state).
                        const auto pm = (EQ8DSP::PhaseMode) mPhaseMode;
                        if (mBindMode == BindMode::DSP && mBoundDSP)
                            mBoundDSP->setPhaseMode(pm);
                        else if (mBindMode == BindMode::MsDSP && mBoundMsDsp)
                            mBoundMsDsp->setPhaseMode(pm);
                        if (onLatencyChanged) onLatencyChanged();
                        repaint();
                    }
                    // D.4-Q6: Linear Phase Precision (20..24)
                    else if (result >= 20 && result <= 24)
                    {
                        const int prec = result - 20;
                        if (mBindMode == BindMode::DSP && mBoundDSP)
                            mBoundDSP->setLinearPhasePrecision (prec);
                        else if (mBindMode == BindMode::MsDSP && mBoundMsDsp)
                        {
                            mBoundMsDsp->mid ().setLinearPhasePrecision (prec);
                            mBoundMsDsp->side().setLinearPhasePrecision (prec);
                        }
                        if (onLatencyChanged) onLatencyChanged();
                        repaint();
                    }
                    // D.4-Q6: IIR Mod Speed (25..29)
                    else if (result >= 25 && result <= 29)
                    {
                        static const float kSpeeds[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
                        const float v = kSpeeds[result - 25];
                        if (mBindMode == BindMode::DSP && mBoundDSP)
                            mBoundDSP->setIIRModSpeed (v);
                        else if (mBindMode == BindMode::MsDSP && mBoundMsDsp)
                        {
                            mBoundMsDsp->mid ().setIIRModSpeed (v);
                            mBoundMsDsp->side().setIIRModSpeed (v);
                        }
                    }
                    // D.4-Q6: Proportional Q toggle (30)
                    else if (result == 30)
                    {
                        bool curOn = false;
                        if (mBindMode == BindMode::DSP && mBoundDSP)
                            curOn = mBoundDSP->getProportionalQ();
                        else if (mBindMode == BindMode::MsDSP && mBoundMsDsp)
                            curOn = mBoundMsDsp->mid().getProportionalQ();
                        const bool next = ! curOn;
                        if (mBindMode == BindMode::DSP && mBoundDSP)
                            mBoundDSP->setProportionalQ (next);
                        else if (mBindMode == BindMode::MsDSP && mBoundMsDsp)
                        {
                            mBoundMsDsp->mid ().setProportionalQ (next);
                            mBoundMsDsp->side().setProportionalQ (next);
                        }
                        repaint();
                    }
                    break;
            }
        });
}

void ParametricEQDisplay::showMidSideToggle(bool show)
{
    // 12h: internal pill removed. Retained as a no-op so existing page-level
    // call sites (EffectsPage / LayersPage / BassPage / DrumsPage ctors) keep
    // compiling. External page-header MID/SIDE buttons drive setShowMid() now.
    juce::ignoreUnused (show);
}

// ── Heatmap ───────────────────────────────────────────────────────────────────

void ParametricEQDisplay::drawHeatmap(juce::Graphics& g) const
{
    auto& a = mGraphArea;
    if (a.isEmpty() || !mHeatmapHasData || mSampleRateForFFT <= 0.0) return;

    const float nyquist = (float)(mSampleRateForFFT * 0.5);
    const int   numBins = kFFTSize / 2;
    const int   h       = a.getHeight();
    const int   rows    = kNumHeatmapFrames;

    // Render older frames near bottom, newer near top (scrolling spectrogram)
    for (int row = 0; row < rows; ++row)
    {
        int frameIdx = (mHeatmapWritePos - 1 - row + rows) % rows;
        const auto& frame = mHeatmapFrames[frameIdx];
        float fy = (float)a.getBottom() - (row + 1.f) * h / (float)rows;
        float rowH = h / (float)rows;

        for (int bin = 1; bin < numBins; ++bin)
        {
            float hz = bin * nyquist / (float)numBins;
            if (hz < 20.f || hz > 20000.f) continue;
            float x1 = freqToX(hz);
            float x0 = freqToX((bin - 1) * nyquist / (float)numBins);
            float db = juce::jlimit(-80.f, 0.f, frame[bin]);
            float t  = (db + 80.f) / 80.f;  // 0..1

            // Viridis-inspired color: blue→cyan→green→yellow→red
            juce::Colour col;
            if (t < 0.25f)       col = juce::Colours::darkblue.interpolatedWith(juce::Colours::cyan,   t / 0.25f);
            else if (t < 0.5f)   col = juce::Colours::cyan.interpolatedWith    (juce::Colours::green,  (t - 0.25f) / 0.25f);
            else if (t < 0.75f)  col = juce::Colours::green.interpolatedWith   (juce::Colours::yellow, (t - 0.5f)  / 0.25f);
            else                  col = juce::Colours::yellow.interpolatedWith  (juce::Colours::red,    (t - 0.75f) / 0.25f);

            g.setColour(col.withAlpha(0.55f));
            g.fillRect((float)x0, fy, x1 - x0 + 1.f, rowH + 1.f);
        }
    }
}

// ── Phase curve ───────────────────────────────────────────────────────────────

float ParametricEQDisplay::evalPhaseRad(int idx, float hz) const
{
    const auto& b = mBands[idx];
    if (!b.enabled || b.muted || b.type == 6 || b.type == 7) return 0.0f;

    const float sr = (float)mSampleRateForFFT;
    if (sr <= 0.f) return 0.0f;
    const float f   = juce::jlimit(20.0f, sr * 0.499f, b.freq);
    const float q   = juce::jlimit(0.05f, 20.0f, b.q);
    const float g   = b.gainDb;

    float w0  = 2.0f * juce::MathConstants<float>::pi * f / sr;
    float cw  = std::cos(w0), sw = std::sin(w0);
    float alp = sw / (2.0f * q);

    float b0, b1, b2, a0, a1, a2;

    switch (b.type)
    {
        case 0: { // Bell/Peak
            float A = juce::Decibels::decibelsToGain(g * 0.5f);
            b0 = 1.f + alp * A;  b1 = -2.f * cw;  b2 = 1.f - alp * A;
            a0 = 1.f + alp / A;  a1 = -2.f * cw;  a2 = 1.f - alp / A;
            break;
        }
        case 1: { // LP
            b0 = (1.f - cw) * 0.5f;  b1 = 1.f - cw;  b2 = (1.f - cw) * 0.5f;
            a0 = 1.f + alp;          a1 = -2.f * cw;  a2 = 1.f - alp;
            break;
        }
        case 2: { // HP
            b0 = (1.f + cw) * 0.5f;  b1 = -(1.f + cw);  b2 = (1.f + cw) * 0.5f;
            a0 = 1.f + alp;           a1 = -2.f * cw;     a2 = 1.f - alp;
            break;
        }
        case 3: { // LowShelf
            float A  = juce::Decibels::decibelsToGain(g * 0.5f);
            float sq = 2.f * std::sqrt(A) * alp;
            b0 = A * ((A + 1) - (A - 1) * cw + sq);
            b1 = 2.f * A * ((A - 1) - (A + 1) * cw);
            b2 = A * ((A + 1) - (A - 1) * cw - sq);
            a0 = (A + 1) + (A - 1) * cw + sq;
            a1 = -2.f * ((A - 1) + (A + 1) * cw);
            a2 = (A + 1) + (A - 1) * cw - sq;
            break;
        }
        case 4: { // HiShelf
            float A  = juce::Decibels::decibelsToGain(g * 0.5f);
            float sq = 2.f * std::sqrt(A) * alp;
            b0 = A * ((A + 1) + (A - 1) * cw + sq);
            b1 = -2.f * A * ((A - 1) + (A + 1) * cw);
            b2 = A * ((A + 1) + (A - 1) * cw - sq);
            a0 = (A + 1) - (A - 1) * cw + sq;
            a1 = 2.f * ((A - 1) - (A + 1) * cw);
            a2 = (A + 1) - (A - 1) * cw - sq;
            break;
        }
        case 5: { // Notch
            b0 = 1.f;  b1 = -2.f * cw;  b2 = 1.f;
            a0 = 1.f + alp;  a1 = -2.f * cw;  a2 = 1.f - alp;
            break;
        }
        default: return 0.0f;
    }

    // Normalise
    b0 /= a0;  b1 /= a0;  b2 /= a0;
    a1 /= a0;  a2 /= a0;

    // Evaluate H(e^jω) at target freq
    const float omega = 2.0f * juce::MathConstants<float>::pi * hz / sr;
    const float c1 = std::cos(omega), s1 = std::sin(omega);
    const float c2 = std::cos(2.f * omega), s2 = std::sin(2.f * omega);

    const float nRe = b0 + b1 * c1 + b2 * c2;
    const float nIm = -(b1 * s1 + b2 * s2);
    const float dRe = 1.f + a1 * c1 + a2 * c2;
    const float dIm = -(a1 * s1 + a2 * s2);

    const float magSq = dRe * dRe + dIm * dIm;
    if (magSq < 1e-10f) return 0.0f;

    const float re = (nRe * dRe + nIm * dIm) / magSq;
    const float im = (nIm * dRe - nRe * dIm) / magSq;
    return std::atan2(im, re);
}

void ParametricEQDisplay::drawPhaseCurve(juce::Graphics& g) const
{
    auto& a = mGraphArea;
    int w = a.getWidth();
    if (w <= 0 || mSampleRateForFFT <= 0.0) return;

    juce::Path phasePath;
    const float zeroY  = gainToY(0.f);
    const float scale  = (float)a.getHeight() / (2.f * juce::MathConstants<float>::pi);  // ±π → ±halfHeight

    for (int px = 0; px < w; ++px)
    {
        float hz    = xToFreq((float)(a.getX() + px));
        float phase = 0.f;
        for (int b = 0; b < kNumBands; ++b)
            phase += evalPhaseRad(b, hz);

        // Map phase ±π to ±half-graph-height (same scale as ±18dB)
        float y = zeroY - phase * scale;
        y = juce::jlimit((float)a.getY(), (float)a.getBottom(), y);
        if (px == 0) phasePath.startNewSubPath((float)(a.getX() + px), y);
        else         phasePath.lineTo         ((float)(a.getX() + px), y);
    }

    g.setColour(juce::Colour(0xffff8800).withAlpha(0.65f));  // orange
    g.strokePath(phasePath, juce::PathStrokeType(1.2f));
}

// ── pushSamples - also update heatmap buffer ──────────────────────────────────
// (Kept as the existing implementation; the heatmap frame push happens in the
//  existing pushSamples block.  We patch the end of the FFT completion block.)

// ============================================================ VUMeter
float VUMeter::sCalibrationDb = -18.f;
std::function<void()> VUMeter::sOnCalibrationChanged;

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
        if (auto xml = juce::XmlDocument::parse (f))
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
            root = juce::XmlDocument::parse (f);
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

