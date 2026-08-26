// BaySickDAW — the EQ window's band chips and right rail (QA-EqPro).
//
// Ported from KBS EQ Pro's EqRail (the third-pass layout that provably fits
// the smallest window WITH the dynamics section) and adapted to the strip
// world: no tier gates, the DAW's dark look, params through the graph's one
// id spelling, and two structural changes ruled by Jeff:
//  * The channel row offers ST / L / R and only in the Stereo view - Mid and
//    Side are VIEWS (SC-5), so the picker never lies.
//  * The A/B control sits beside the chip row's "+" (SC-16): click swaps the
//    banks, right-click offers Copy A to B and Lock.
// The EXT toggle became the sidechain-source picker: the strip has four
// receive lines, so EXT opens a menu of them instead of flipping one bool.
#pragma once

#include "EqGraphView.h"

namespace eqview {

// ── a draggable number field ───────────────────────────────────────────────
class DragNumber : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    std::function<float()> get;
    std::function<void (float)> set;
    std::function<juce::String (float)> format;
    float dragScale = 1.0f;
    bool logDrag = false;

    explicit DragNumber (const juce::String& labelText)
    {
        label.setText (labelText, juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (9.0f)));
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, VC::TextDim);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);

        value.setJustificationType (juce::Justification::centred);
        value.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        value.setColour (juce::Label::textColourId, VC::Text);
        value.setEditable (false, true, false);
        value.onTextChange = [this]
        {
            if (set && get)
                set (value.getText().retainCharacters ("0123456789.-").getFloatValue());
            refresh();
        };
        value.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (value);
        setRepaintsOnMouseActivity (true);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }

    void refresh()
    {
        if (get && format)
            value.setText (format (get()), juce::dontSendNotification);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (11));
        value.setBounds (r);
    }

    void paint (juce::Graphics& g) override
    {
        auto box = getLocalBounds().toFloat().withTrimmedTop (11.0f);
        g.setColour (VC::Surface.darker (0.3f).withAlpha (0.6f));
        g.fillRoundedRectangle (box, 3.0f);
        g.setColour (VC::Accent.withAlpha (isMouseOver() ? 0.9f : 0.45f));
        g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.0f);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (get) dragStartValue = get();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! get || ! set) return;
        const float k = e.mods.isShiftDown() ? 0.2f : 1.0f;
        const float d = (float) -e.getDistanceFromDragStartY() * k;
        set (logDrag ? dragStartValue * std::pow (2.0f, d * dragScale)
                     : dragStartValue + d * dragScale);
        refresh();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override { value.showEditor(); }

private:
    juce::Label label, value;
    float dragStartValue = 0.0f;
};

// ── a row of exclusive segments, each with its own tooltip ─────────────────
class SegmentRow : public juce::Component,
                   public juce::TooltipClient
{
public:
    struct Seg { juce::String glyph, text, tip; };
    std::vector<Seg> segs;
    int columns = 0;                 // 0 = one row; else a grid

    std::function<int()> get;
    std::function<void (int)> set;

    juce::Rectangle<float> cellRect (int i) const
    {
        const int cols = columns > 0 ? columns : (int) segs.size();
        const int rows = ((int) segs.size() + cols - 1) / cols;
        const float w = (float) getWidth() / (float) cols;
        const float h = (float) getHeight() / (float) rows;
        return juce::Rectangle<float> ((i % cols) * w, (i / cols) * h, w, h)
                   .reduced (1.0f, 1.0f);
    }

    void paint (juce::Graphics& g) override
    {
        if (segs.empty()) return;
        const int cur = get ? get() : 0;

        for (int i = 0; i < (int) segs.size(); ++i)
        {
            auto cell = cellRect (i);
            const bool sel = i == cur;

            g.setColour (sel ? VC::Yellow.withAlpha (0.85f)
                             : VC::Surface.darker (0.3f).withAlpha (0.55f));
            g.fillRoundedRectangle (cell, 3.0f);
            g.setColour (sel ? VC::Yellow : VC::Accent.withAlpha (0.5f));
            g.drawRoundedRectangle (cell.reduced (0.5f), 3.0f, 1.0f);

            g.setColour (sel ? VC::Bg : VC::Text);
            const auto& sg = segs[(size_t) i];
            if (sg.glyph.isNotEmpty())
            {
                // Sized to the CELL WIDTH as well as its height: drawText
                // with ellipsis off drops a glyph wider than its box whole -
                // the KBS empty-squares trap.
                g.setFont (icons->getFont (juce::jmin (cell.getWidth() - 4.0f,
                                                       cell.getHeight() * 0.85f, 20.0f)));
                g.drawText (sg.glyph, cell.toNearestInt(),
                            juce::Justification::centred, false);
            }
            else
            {
                g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
                g.drawText (sg.text, cell, juce::Justification::centred, false);
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { hoverAt = e.position; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (segs.empty() || ! set) return;
        set (indexAt (e.position));
        repaint();
    }

    juce::String getTooltip() override
    {
        if (segs.empty()) return {};
        return segs[(size_t) indexAt (hoverAt)].tip;
    }

private:
    int indexAt (juce::Point<float> pos) const
    {
        for (int i = 0; i < (int) segs.size(); ++i)
            if (cellRect (i).expanded (1.0f).contains (pos)) return i;
        return 0;
    }

    juce::Point<float> hoverAt;
    juce::SharedResourcePointer<fontaudio::IconHelper> icons;
};

// ── a vertical gain-reduction meter with a scale ───────────────────────────
class GrMeter : public juce::Component,
                public juce::SettableTooltipClient
{
public:
    std::function<float()> read;    // dB, signed

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto track = r.withTrimmedBottom (12.0f).reduced (r.getWidth() * 0.5f - 3.0f, 0.0f);

        g.setColour (VC::Surface.darker (0.3f));
        g.fillRoundedRectangle (track, 2.5f);

        const float gr = read ? read() : 0.0f;
        const float t = juce::jlimit (0.0f, 1.0f, std::abs (gr) / 24.0f);
        if (t > 0.004f)
        {
            auto fill = track.reduced (1.0f);
            fill = fill.removeFromTop (fill.getHeight() * t);
            g.setGradientFill (juce::ColourGradient (
                VC::Green.brighter (0.2f), 0.0f, fill.getY(),
                VC::Green.darker (0.2f), 0.0f, fill.getBottom(), false));
            g.fillRoundedRectangle (fill, 2.0f);
        }

        g.setColour (VC::TextDim.withAlpha (0.7f));
        g.setFont (juce::Font (juce::FontOptions (7.5f)));
        for (int db : { 6, 12, 18 })
        {
            const float y = track.getY() + track.getHeight() * (float) db / 24.0f;
            g.drawHorizontalLine ((int) y, track.getX() - 4.0f, track.getX() - 1.0f);
            g.drawText (juce::String (db), (int) track.getX() - 15, (int) y - 5,
                        10, 9, juce::Justification::right);
        }

        g.setColour (VC::Text.withAlpha (0.9f));
        g.setFont (juce::Font (juce::FontOptions (8.5f)));
        g.drawText (juce::String (gr, 1), r.removeFromBottom (11.0f).toNearestInt(),
                    juce::Justification::centred);
    }
};

// ── the chip row (24 chips, "+", and the A/B control per SC-16) ────────────
class BandChipRow : public juce::Component,
                    public juce::TooltipClient,
                    private juce::Timer
{
public:
    BandChipRow (EqGraphView& graphRef) : graph (graphRef)
    {
        startTimerHz (10);
    }

    // SC-16: the window owns the swap (it pushes the swapped bank to the
    // params); the row only reports state and clicks.
    std::function<void()> onAbSwap;
    std::function<void()> onAbCopy;
    std::function<void()> onAbLock;
    std::function<bool()> isViewingB;
    std::function<bool()> isAbLocked;

    void paint (juce::Graphics& g) override
    {
        const float w = chipWidth();

        for (int b = 0; b < EqGraphView::kBands; ++b)
        {
            const auto p = graph.bandParams (b);
            auto cell = juce::Rectangle<float> (b * w, 0.0f, w, (float) getHeight())
                            .reduced (2.0f, 4.0f);

            auto col = graph.bandColour (b);
            if (! p.on) col = col.withAlpha (0.25f);
            if (p.muted) col = col.withSaturation (0.15f);

            g.setColour (col);
            if (p.on) g.fillRoundedRectangle (cell, cell.getHeight() * 0.5f);
            else      g.drawRoundedRectangle (cell, cell.getHeight() * 0.5f, 1.0f);

            if (b == graph.selectedBand())
            {
                g.setColour (VC::Text);
                g.drawRoundedRectangle (cell.expanded (1.5f),
                                        cell.getHeight() * 0.5f + 1.5f, 1.6f);
            }

            g.setColour (p.on ? VC::Bg : VC::TextDim);
            g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
            g.drawText (juce::String (b + 1), cell, juce::Justification::centred);

            // A chip living in another view shows a tiny domain tick so the
            // row still maps the whole pool.
            const auto v = viewOfChannel (p.channel);
            if (p.on && v != graph.domainView())
            {
                g.setColour (VC::Bg.withAlpha (0.9f));
                g.setFont (juce::Font (juce::FontOptions (6.5f, juce::Font::bold)));
                g.drawText (v == DomainView::mid ? "M" : v == DomainView::side ? "S" : "",
                            cell.translated (0.0f, cell.getHeight() * 0.28f),
                            juce::Justification::centred);
            }
        }

        auto add = addArea();
        g.setColour (VC::TextDim);
        g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        g.drawText ("+", add, juce::Justification::centred);

        // The A/B pill (SC-16): lit while the B bank is in view.
        const bool onB = isViewingB && isViewingB();
        auto ab = abArea();
        g.setColour (onB ? VC::Yellow.withAlpha (0.85f)
                         : VC::Surface.darker (0.2f));
        g.fillRoundedRectangle (ab, ab.getHeight() * 0.5f);
        g.setColour (onB ? VC::Yellow : VC::Accent);
        g.drawRoundedRectangle (ab.reduced (0.5f), ab.getHeight() * 0.5f, 1.0f);
        g.setColour (onB ? VC::Bg : VC::Text);
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        g.drawText (onB ? "B" : "A", ab, juce::Justification::centred);
        if (isAbLocked && isAbLocked())
        {
            g.setFont (icons->getFont (8.0f));
            g.setColour (VC::TextDim);
            g.drawText (fontaudio::Lock, ab.translated (0.0f, -1.0f).toNearestInt(),
                        juce::Justification::centredRight, false);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { hoverAt = e.position; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (abArea().contains (e.position))
        {
            if (e.mods.isPopupMenu())
            {
                juce::PopupMenu m;
                m.addItem (1, "Copy A to B");
                m.addItem (2, "Lock banks", true, isAbLocked && isAbLocked());
                const auto at = juce::Rectangle<int> (1, 1)
                                    .withPosition (juce::Desktop::getMousePosition());
                m.showMenuAsync (juce::PopupMenu::Options()
                                     .withTargetComponent (this)
                                     .withTargetScreenArea (at),
                    [this] (int r)
                    {
                        if (r == 1 && onAbCopy) onAbCopy();
                        else if (r == 2 && onAbLock) onAbLock();
                    });
                return;
            }
            if (onAbSwap) onAbSwap();
            return;
        }

        if (addArea().contains (e.position))
        {
            if (! e.mods.isPopupMenu())
                graph.addBandAt ({ graph.freqToX (1000.0), graph.gainToY (0.0f) });
            return;
        }

        const int b = chipAt (e.position);
        if (b < 0) return;

        // Right-click on an active chip is the band's own menu - the chip is
        // the dot's twin, so it answers the same way.
        if (e.mods.isPopupMenu())
        {
            if (graph.bandParams (b).on)
            {
                graph.selectBand (b);
                graph.showBandMenu (b);
            }
            return;
        }

        graph.selectBand (b);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int b = chipAt (e.position);
        if (b < 0) return;

        if (graph.bandParams (b).on)
        {
            // Same gesture as the dot: mute, never delete.
            graph.toggleBand (b, "mute");
            return;
        }
        graph.enableBand (b);
    }

    juce::String getTooltip() override
    {
        if (abArea().contains (hoverAt))
            return "A/B compare: click swaps the two setups, right-click "
                   "copies or locks them.";
        if (addArea().contains (hoverAt)) return "Add a band";
        const int b = chipAt (hoverAt);
        if (b < 0) return {};
        if (! graph.bandParams (b).on)
            return "Band " + juce::String (b + 1)
                 + " - off. Double-click to switch it on.";
        return "Band " + juce::String (b + 1)
             + " - click selects, double-click mutes, right-click for the menu.";
    }

private:
    static constexpr int kRightControls = 54;   // "+" plus the A/B pill

    int chipAt (juce::Point<float> pos) const
    {
        const int b = (int) (pos.x / chipWidth());
        return b >= 0 && b < EqGraphView::kBands ? b : -1;
    }

    float chipWidth() const
    {
        return (float) (getWidth() - kRightControls) / (float) EqGraphView::kBands;
    }

    juce::Rectangle<float> addArea() const
    {
        return { (float) getWidth() - 52.0f, 2.0f, 24.0f, (float) getHeight() - 4.0f };
    }

    juce::Rectangle<float> abArea() const
    {
        return { (float) getWidth() - 26.0f, 4.0f, 24.0f, (float) getHeight() - 8.0f };
    }

    void timerCallback() override { repaint(); }

    EqGraphView& graph;
    juce::Point<float> hoverAt;
    juce::SharedResourcePointer<fontaudio::IconHelper> icons;
};

// ── the rail ───────────────────────────────────────────────────────────────
class EqRailView : public juce::Component, private juce::Timer
{
public:
    static constexpr int kWidth = 150;
    static constexpr int kCollapsedWidth = 14;

    std::function<void (bool)> onCollapse;
    // EXT is the strip's four receive lines here, not one bool: the window
    // owns the menu (it knows the strip's routing names).
    std::function<void()> onPickScSource;
    bool collapsed = false;

    explicit EqRailView (EqGraphView& graphRef)
        : graph (graphRef), freq ("FREQ"), q ("Q")
    {
        auto initKnob = [this] (juce::Slider& k, const juce::String& tip)
        {
            k.setSliderStyle (juce::Slider::RotaryVerticalDrag);
            k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            k.setTooltip (tip);
            addAndMakeVisible (k);
        };

        initKnob (gain, "This band's gain");
        gain.setRange (-30.0, 30.0, 0.0);
        gain.setDoubleClickReturnValue (true, 0.0);
        gain.onValueChange = [this]
        {
            if (const int b = graph.selectedBand(); b >= 0)
                graph.setBandValue (b, "gain", (float) gain.getValue());
        };

        initKnob (pan, "Pans this band's effect across the stereo image: "
                       "hard left and the boost or cut lands only on the left "
                       "channel. Not an audio pan - the band's, and only on "
                       "gain types in the Stereo view.");
        pan.setRange (-1.0, 1.0, 0.0);
        pan.setDoubleClickReturnValue (true, 0.0);
        pan.onValueChange = [this]
        {
            if (const int b = graph.selectedBand(); b >= 0)
                graph.setBandValue (b, "place", (float) pan.getValue());
        };

        wireNumber (freq, "freq", true, 0.006f,
                    [] (float v) { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + "k"
                                                       : juce::String (v, 1); });
        freq.setTooltip ("The band's frequency. Drag, or double-click to type.");
        wireNumber (q, "q", true, 0.008f,
                    [] (float v) { return juce::String (v, 2); });
        q.setTooltip ("Width. Higher is narrower. The wheel over the band's "
                      "dot changes it too.");

        type.segs = {
            { fontaudio::FilterBell, {}, "Bell" },
            { fontaudio::FilterLowpass, {}, "Low Pass" },
            { fontaudio::FilterHighpass, {}, "High Pass" },
            { fontaudio::FilterShelvingLo, {}, "Low Shelf" },
            { fontaudio::FilterShelvingHi, {}, "High Shelf" },
            { fontaudio::FilterNotch, {}, "Notch" },
            { fontaudio::FilterBandpass, {}, "Band Pass" },
            { {}, "T", "Tilt - low shelf up, high shelf down, one pivot" },
        };
        type.columns = 4;              // 2 rows of 4: glyphs get real room
        wireSegment (type, "type");
        addAndMakeVisible (type);

        // ST / L / R - and only in the Stereo view (SC-5): Mid and Side are
        // views, and left/right do not exist inside them.
        chan.segs = {
            { {}, "ST", "Stereo - both channels" },
            { {}, "L", "Left channel only" },
            { {}, "R", "Right channel only" },
        };
        chan.get = [this]
        {
            const int b = graph.selectedBand();
            if (b < 0) return 0;
            const auto ch = graph.bandParams (b).channel;
            return ch == kbs::EqChannel::left ? 1 : ch == kbs::EqChannel::right ? 2 : 0;
        };
        chan.set = [this] (int v)
        {
            const int b = graph.selectedBand();
            if (b < 0) return;
            const int ch = v == 1 ? (int) kbs::EqChannel::left
                         : v == 2 ? (int) kbs::EqChannel::right
                                  : (int) kbs::EqChannel::stereo;
            graph.setBandValue (b, "chan", (float) ch);
        };
        addAndMakeVisible (chan);

        slope.addItemList ({ "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct",
                             "36 dB/oct", "48 dB/oct", "72 dB/oct", "96 dB/oct",
                             "Brickwall" }, 1);
        slope.setTooltip ("How steep the filter falls. Brickwall exists in "
                          "the linear-phase modes.");
        slope.onChange = [this]
        {
            if (syncing) return;
            const int b = graph.selectedBand();
            if (b < 0) return;
            graph.setBandValue (b, "slope", (float) (slope.getSelectedId() - 1));
        };
        addAndMakeVisible (slope);

        dynT.setButtonText ("DYN");
        dynT.setTooltip ("Make this band dynamic: it moves with the material. "
                         "Enabling seeds the direction to compress so it does "
                         "something immediately.");
        dynT.onClick = [this]
        {
            if (const int b = graph.selectedBand(); b >= 0)
                graph.toggleDynamic (b);
        };
        addAndMakeVisible (dynT);

        autoT.setButtonText ("AUTO");
        autoT.setTooltip ("Programme-dependent release: fast on transients, "
                          "slow on sustained material.");
        autoT.onClick = [this]
        {
            if (const int b = graph.selectedBand(); b >= 0)
                graph.toggleBand (b, "relauto");
        };
        addAndMakeVisible (autoT);

        extT.setButtonText ("EXT");
        extT.setTooltip ("Detect from one of this strip's sidechain receive "
                         "lines instead of the band's own input - duck this "
                         "band when that source plays. Click to pick the line.");
        extT.onClick = [this] { if (onPickScSource) onPickScSource(); };
        addAndMakeVisible (extT);

        auto initDynKnob = [this, &initKnob] (juce::Slider& k, const char* field,
                                              double lo, double hi, double skew,
                                              const juce::String& tip)
        {
            initKnob (k, tip);
            k.setRange (lo, hi, 0.0);
            if (skew > 0.0) k.setSkewFactor (skew);
            k.onValueChange = [this, &k, field]
            {
                if (const int b = graph.selectedBand(); b >= 0)
                    graph.setBandValue (b, field, (float) k.getValue());
            };
        };
        initDynKnob (thrK, "thr", -60.0, 0.0, 0.0,
                     "Where the band engages, and how far it can go: at 0 dB "
                     "nothing ever crosses, so nothing moves; at -60 everything "
                     "does, pinned to the dotted line. The dotted line IS this "
                     "knob.");
        thrK.setDoubleClickReturnValue (true, 0.0);
        initDynKnob (ratK, "ratio", 1.0, 20.0, 0.5, "How hard past the threshold");
        ratK.setDoubleClickReturnValue (true, 2.0);
        initDynKnob (atkK, "atk", 0.1, 500.0, 0.35, "Detector attack");
        atkK.setDoubleClickReturnValue (true, 10.0);
        initDynKnob (relK, "rel", 1.0, 2000.0, 0.35, "Detector release");
        relK.setDoubleClickReturnValue (true, 100.0);

        // Direction: compress pulls the band down past the threshold, expand
        // pushes it up.  No Range knob - the range parameter carries the
        // direction as its sign at full travel.
        direction.segs = {
            { {}, "DOWN", "Compress: the band cuts when its signal crosses "
                          "the threshold" },
            { {}, "UP", "Expand: the band boosts when its signal crosses "
                        "the threshold" },
        };
        direction.get = [this]
        {
            const int b = graph.selectedBand();
            return b >= 0 && graph.bandParams (b).rangeDb > 0.0f ? 1 : 0;
        };
        direction.set = [this] (int v)
        {
            if (const int b = graph.selectedBand(); b >= 0)
                graph.setDynamicDirection (b, v == 1);
        };
        addAndMakeVisible (direction);

        grMeter.setTooltip ("Gain reduction: how far the band has moved right now");
        grMeter.read = [this]
        {
            const int b = graph.selectedBand();
            if (b < 0) return 0.0f;
            if (auto* e = graph.eq()) return e->engine().bandGrDb (b);
            return 0.0f;
        };
        addAndMakeVisible (grMeter);

        startTimerHz (15);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (VC::Bg);
        g.setColour (VC::Panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f, 1.0f), 4.0f);

        g.setColour (VC::Accent.withAlpha (0.6f));
        g.fillRoundedRectangle (3.0f, getHeight() * 0.5f - 14.0f, 3.0f, 28.0f, 1.5f);

        const int b = graph.selectedBand();
        g.setColour (VC::TextDim);
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawText (b >= 0 ? "BAND " + juce::String (b + 1) : "NO BAND",
                    14, 6, getWidth() - 28, 12, juce::Justification::centredLeft);

        if (b >= 0)
        {
            g.setColour (graph.bandColour (b));
            g.fillEllipse ((float) getWidth() - 22.0f, 7.0f, 9.0f, 9.0f);
        }

        auto caption = [&] (juce::Component& c, const char* text)
        {
            if (! c.isVisible()) return;
            g.setColour (VC::TextDim);
            g.setFont (juce::Font (juce::FontOptions (8.5f)));
            g.drawText (text, c.getX(), c.getBottom() + 1, c.getWidth(), 9,
                        juce::Justification::centred);
        };
        caption (gain, "GAIN");
        caption (pan, "PAN");

        if (b >= 0 && kbs::eqTypeSupportsDynamic (graph.bandParams (b).type))
        {
            g.setColour (VC::Accent.withAlpha (0.5f));
            g.drawHorizontalLine (dynTop - 6, 12.0f, (float) getWidth() - 12.0f);
            g.setColour (VC::TextDim);
            g.setFont (juce::Font (juce::FontOptions (8.5f, juce::Font::bold)));
            g.drawText ("DYNAMICS", 14, dynTop - 16, 80, 10,
                        juce::Justification::left);
        }

        caption (thrK, "THR");
        caption (ratK, "RATIO");
        caption (atkK, "ATK");
        caption (relK, "REL");

        auto readout = [&] (juce::Slider& k, const juce::String& text)
        {
            if (! k.isVisible()) return;
            g.setColour (VC::Text.withAlpha (0.85f));
            g.setFont (juce::Font (juce::FontOptions (8.5f)));
            g.drawText (text, k.getX() - 4, k.getBottom() + 10, k.getWidth() + 8, 9,
                        juce::Justification::centred);
        };
        if (b >= 0)
        {
            const auto p = graph.bandParams (b);
            readout (gain, juce::String (p.gainDb, 1) + " dB");
            readout (pan, std::abs (p.placement) < 0.02f ? juce::String ("C")
                          : (p.placement < 0 ? "L" : "R")
                              + juce::String ((int) std::round (std::abs (p.placement) * 100)));
            readout (thrK, juce::String (p.thresholdDb, 0));
            readout (ratK, juce::String (p.ratio, 1) + ":1");
            readout (atkK, juce::String (p.attackMs, p.attackMs < 10 ? 1 : 0));
            readout (relK, p.autoRelease ? juce::String ("auto")
                                         : juce::String (p.releaseMs, 0));
        }
    }

    void resized() override { layout(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.position.x < 9.0f)
        {
            collapsed = ! collapsed;
            if (onCollapse) onCollapse (collapsed);
        }
    }

    void pollNow() { timerCallback(); }

private:
    void wireNumber (DragNumber& d, const char* field, bool logDrag, float scale,
                     std::function<juce::String (float)> fmt)
    {
        d.logDrag = logDrag;
        d.dragScale = scale;
        d.format = std::move (fmt);
        d.get = [this, field]
        {
            const int b = graph.selectedBand();
            if (b < 0) return 0.0f;
            const auto p = graph.bandParams (b);
            return std::strcmp (field, "freq") == 0 ? p.freqHz : p.q;
        };
        d.set = [this, field] (float v)
        {
            if (const int b = graph.selectedBand(); b >= 0)
                graph.setBandValue (b, field, v);
        };
        addAndMakeVisible (d);
    }

    void wireSegment (SegmentRow& row, const char* field)
    {
        row.get = [this, field]
        {
            const int b = graph.selectedBand();
            if (b < 0) return 0;
            const auto p = graph.bandParams (b);
            return std::strcmp (field, "type") == 0 ? (int) p.type : p.slope;
        };
        row.set = [this, field] (int v)
        {
            if (const int b = graph.selectedBand(); b >= 0)
                graph.setBandValue (b, field, (float) v);
        };
    }

    void layout()
    {
        // Budgeted to fit the smallest window WITH the dynamics section and
        // its meter (the KBS third-pass arithmetic, kept).
        auto r = getLocalBounds().reduced (12, 5);
        r.removeFromTop (15);

        auto knobs = r.removeFromTop (34);
        gain.setBounds (knobs.removeFromLeft (knobs.getWidth() / 2).reduced (14, 0));
        pan.setBounds (knobs.reduced (14, 0));
        r.removeFromTop (20);                              // caption + readout

        auto row = r.removeFromTop (27);
        freq.setBounds (row.removeFromLeft (row.getWidth() / 2 - 2));
        row.removeFromLeft (4);
        q.setBounds (row);
        r.removeFromTop (4);

        type.setBounds (r.removeFromTop (34));
        r.removeFromTop (3);
        chan.setBounds (r.removeFromTop (16));
        r.removeFromTop (3);
        slope.setBounds (r.removeFromTop (17));
        r.removeFromTop (12);

        dynTop = r.getY();
        auto tr = r.removeFromTop (16);
        const int tw = tr.getWidth() / 3;
        dynT.setBounds (tr.removeFromLeft (tw).reduced (1, 0));
        autoT.setBounds (tr.removeFromLeft (tw).reduced (1, 0));
        extT.setBounds (tr.reduced (1, 0));
        r.removeFromTop (4);

        direction.setBounds (r.removeFromTop (16));
        r.removeFromTop (4);

        auto body = r.removeFromTop (100);
        auto meterCol = body.removeFromRight (22);
        grMeter.setBounds (meterCol.reduced (0, 1));

        auto k1 = body.removeFromTop (30);
        const int kw = k1.getWidth() / 2;
        thrK.setBounds (k1.removeFromLeft (kw).reduced (8, 0));
        ratK.setBounds (k1.reduced (8, 0));
        body.removeFromTop (20);                           // captions + values
        auto k2 = body.removeFromTop (30);
        atkK.setBounds (k2.removeFromLeft (kw).reduced (8, 0));
        relK.setBounds (k2.reduced (8, 0));
    }

    void syncFromParams()
    {
        const int b = graph.selectedBand();
        if (b < 0) return;
        syncing = true;
        slope.setSelectedId (graph.bandParams (b).slope + 1, juce::dontSendNotification);
        syncing = false;
    }

    void timerCallback() override
    {
        const int b = graph.selectedBand();
        const bool show = b >= 0;
        const auto p = show ? graph.bandParams (b) : kbs::EqBandParams();

        // Component ids carry the param spelling so the app's global
        // right-click (Automate / Type in value / MIDI Learn) reaches these
        // knobs like any other stamped control.
        if (show && b != stampedBand)
        {
            stampedBand = b;
            gain.setComponentID (graph.paramId (b, "gain"));
            pan.setComponentID (graph.paramId (b, "place"));
            thrK.setComponentID (graph.paramId (b, "thr"));
            ratK.setComponentID (graph.paramId (b, "ratio"));
            atkK.setComponentID (graph.paramId (b, "atk"));
            relK.setComponentID (graph.paramId (b, "rel"));
        }

        gain.setVisible (show && kbs::eqTypeHasGain (p.type));
        if (gain.isVisible() && ! gain.isMouseButtonDown())
            gain.setValue (p.gainDb, juce::dontSendNotification);

        pan.setVisible (show && kbs::eqTypeHasGain (p.type)
                        && p.channel == kbs::EqChannel::stereo);
        if (pan.isVisible() && ! pan.isMouseButtonDown())
            pan.setValue (p.placement, juce::dontSendNotification);

        slope.setVisible (show && kbs::eqTypeHasSlope (p.type));
        chan.setVisible (show && graph.domainView() == DomainView::stereo);

        const bool dynOk = show && kbs::eqTypeSupportsDynamic (p.type);
        dynT.setVisible (dynOk);
        autoT.setVisible (dynOk && p.dynamic);
        extT.setVisible (dynOk && p.dynamic);

        const bool dyn = dynOk && p.dynamic;
        for (auto* k : { &thrK, &ratK, &atkK, &relK })
            k->setVisible (dyn);
        relK.setVisible (dyn && ! p.autoRelease);
        direction.setVisible (dyn);
        grMeter.setVisible (dyn);

        if (dyn)
        {
            auto quiet = [] (juce::Slider& k, double v)
            {
                if (! k.isMouseButtonDown())
                    k.setValue (v, juce::dontSendNotification);
            };
            quiet (thrK, p.thresholdDb);
            quiet (ratK, p.ratio);
            quiet (atkK, p.attackMs);
            quiet (relK, p.releaseMs);
            direction.repaint();
        }

        auto tint = [] (juce::TextButton& t, bool on)
        {
            t.setColour (juce::TextButton::buttonColourId,
                         on ? VC::Yellow.withAlpha (0.85f)
                            : VC::Surface.darker (0.2f));
            t.setColour (juce::TextButton::textColourOffId,
                         on ? VC::Bg : VC::Text.withAlpha (0.8f));
        };
        tint (dynT, p.dynamic);
        tint (autoT, p.autoRelease);
        tint (extT, p.scSource >= 0);

        for (auto* d : { &freq, &q })
            if (d->isVisible()) d->refresh();
        type.setVisible (show);
        if (show) syncFromParams();

        repaint();
    }

    EqGraphView& graph;

    juce::Slider gain, pan, thrK, ratK, atkK, relK;
    SegmentRow direction;
    DragNumber freq, q;
    SegmentRow type, chan;
    juce::ComboBox slope;
    juce::TextButton dynT, autoT, extT;
    GrMeter grMeter;
    int dynTop = 300;
    int stampedBand = -1;
    bool syncing = false;

    juce::SharedResourcePointer<fontaudio::IconHelper> icons;
};

} // namespace eqview
