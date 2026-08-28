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

// W-5: "A4", "C#2", "Bb3", "C#2+13" (cents) as frequency entry.  Returns the
// Hz, or -1 when the text is not a note name.
inline float parseNoteNameHz (const juce::String& in)
{
    auto s = in.trim().toUpperCase().removeCharacters (" ");
    if (s.isEmpty()) return -1.0f;
    static const int pcOf[7] = { 9, 11, 0, 2, 4, 5, 7 };   // A..G
    const auto c0 = s[0];
    if (c0 < 'A' || c0 > 'G') return -1.0f;
    int pc = pcOf[(int) (c0 - 'A')];
    int i = 1;
    const int len = s.length();
    if (i < len && s[i] == '#') { ++pc; ++i; }
    else if (i + 1 < len && s[i] == 'B'
             && (juce::CharacterFunctions::isDigit (s[i + 1]) || s[i + 1] == '-'))
    { --pc; ++i; }                                          // flat, not the note B
    int j = i;
    if (j < len && s[j] == '-') ++j;
    int k = j;
    while (k < len && juce::CharacterFunctions::isDigit (s[k])) ++k;
    if (k == j) return -1.0f;
    const int oct = s.substring (i, k).getIntValue();
    float cents = 0.0f;
    if (k < len)
    {
        if (s[k] != '+' && s[k] != '-') return -1.0f;
        cents = (float) s.substring (k).getIntValue();
    }
    const float midi = (float) ((oct + 1) * 12 + pc) + cents / 100.0f;
    return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f);
}

// ── a draggable number field ───────────────────────────────────────────────
class DragNumber : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    std::function<float()> get;
    std::function<void (float)> set;
    std::function<juce::String (float)> format;
    std::function<float (const juce::String&)> parse;   // typed-text override
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
            {
                float v = parse ? parse (value.getText()) : -1.0f;
                if (v <= 0.0f)
                    v = value.getText().retainCharacters ("0123456789.-").getFloatValue();
                set (v);
            }
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

    void setLabelText (const juce::String& t)
    {
        if (t != label.getText())
            label.setText (t, juce::dontSendNotification);
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
                   public juce::TooltipClient,
                   private juce::Timer
{
public:
    struct Seg { juce::String glyph, text, tip; };
    std::vector<Seg> segs;
    int columns = 0;                 // 0 = one row; else a grid

    std::function<int()> get;
    std::function<void (int)> set;
    // W-23: fires (index, true) after the press has been held ~a third of a
    // second, (index, false) on release.  A quick click never triggers it,
    // so the plain switch gesture is untouched.
    std::function<void (int, bool)> onHold;

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
        const int idx = indexAt (e.position);
        set (idx);
        if (onHold) { heldIdx = idx; startTimer (300); }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        stopTimer();
        if (holdActive && onHold) onHold (heldIdx, false);
        holdActive = false;
        heldIdx = -1;
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

    void timerCallback() override
    {
        stopTimer();
        if (onHold && heldIdx >= 0) { holdActive = true; onHold (heldIdx, true); }
    }

    juce::Point<float> hoverAt;
    int heldIdx = -1;
    bool holdActive = false;
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

// ── the chip row: pages of 24 chips, "+", and the A/B control ──────────────
//
// QA-EqFlagship W-15: the pool is 96 bands, shown 24 at a time.  Pages are
// cycled by the two arrows at the row's left; a page only EXISTS once there
// is a reason for it (the previous page filled, or a band already lives on
// it), so a beginner sees exactly the 24-chip row QA-EqPro shipped and the
// arrows never appear.  Pages are numbered - never lettered, "A/B" already
// means the setup pill on this same row.
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
        const int lastPage = lastVisiblePage();
        page = juce::jlimit (0, lastPage, page);
        const float w = chipWidth();
        const float x0 = chipX0();

        if (lastPage > 0)
        {
            auto arrow = [&] (juce::Rectangle<float> r, bool up, bool usable)
            {
                juce::Path t;
                const float cx = r.getCentreX(), cy = r.getCentreY();
                if (up) t.addTriangle (cx - 4.0f, cy + 2.5f, cx + 4.0f, cy + 2.5f, cx, cy - 3.0f);
                else    t.addTriangle (cx - 4.0f, cy - 2.5f, cx + 4.0f, cy - 2.5f, cx, cy + 3.0f);
                g.setColour (usable ? VC::Text.withAlpha (0.75f)
                                    : VC::Surface.darker (0.1f));
                g.fillPath (t);
            };
            arrow (arrowUpArea(),   true,  page > 0);
            arrow (arrowDownArea(), false, page < lastPage);
        }

        for (int i = 0; i < kPageSize; ++i)
        {
            const int b = page * kPageSize + i;
            if (b >= EqGraphView::kBands) break;
            const auto p = graph.bandParams (b);
            // The side inset shrinks with the chip: at a narrow window those
            // 2px each side are the difference between "24" fitting and not.
            auto cell = juce::Rectangle<float> (x0 + i * w, 0.0f, w, (float) getHeight())
                            .reduced (juce::jmin (2.0f, w * 0.12f), 4.0f);

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
            // FITTED, not drawn: drawText drops a string wider than its box
            // whole, which is why every chip past 9 went blank once the window
            // was narrow enough to squeeze two digits.
            g.drawFittedText (juce::String (b + 1), cell.toNearestInt(),
                              juce::Justification::centred, 1, 0.5f);

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

        // W-20: the morph strip - a track with a spring-back thumb.
        {
            auto mr = morphArea();
            auto track = mr.withSizeKeepingCentre (mr.getWidth() - 8.0f, 3.0f);
            g.setColour (VC::Surface.darker (0.3f));
            g.fillRoundedRectangle (track, 1.5f);
            const float tx = track.getX() + track.getWidth()
                                * juce::jlimit (0.0f, 1.0f, morphT);
            g.setColour (morphActive ? VC::Yellow : VC::Accent.withAlpha (0.8f));
            g.fillEllipse (tx - 4.0f, mr.getCentreY() - 4.0f, 8.0f, 8.0f);
            g.setColour (VC::TextDim.withAlpha (0.8f));
            g.setFont (juce::Font (juce::FontOptions (6.5f, juce::Font::bold)));
            g.drawText ("A", mr.removeFromLeft (7.0f), juce::Justification::centredLeft);
            g.drawText ("B", mr.removeFromRight (7.0f), juce::Justification::centredRight);
        }

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
        if (morphArea().contains (e.position) && ! e.mods.isPopupMenu())
        {
            morphBegin();
            morphActive = true;
            morphDragTo (e.position.x);
            return;
        }

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

        if (lastVisiblePage() > 0)
        {
            if (arrowUpArea().contains (e.position))
            { page = juce::jmax (0, page - 1); repaint(); return; }
            if (arrowDownArea().contains (e.position))
            { page = juce::jmin (lastVisiblePage(), page + 1); repaint(); return; }
        }

        if (addArea().contains (e.position))
        {
            // W-15: "+" fills the page you are LOOKING at first, then the
            // pool - and the row follows the band it just made.
            if (! e.mods.isPopupMenu())
            {
                int f = -1;
                for (int i = 0; i < kPageSize && f < 0; ++i)
                {
                    const int b = page * kPageSize + i;
                    if (b < EqGraphView::kBands && ! graph.bandParams (b).on) f = b;
                }
                if (f < 0)
                    for (int b = 0; b < EqGraphView::kBands && f < 0; ++b)
                        if (! graph.bandParams (b).on) f = b;
                if (f >= 0)
                {
                    graph.enableBand (f);
                    graph.selectBand (f);
                    page = f / kPageSize;
                    repaint();
                }
            }
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

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (morphActive) morphDragTo (e.position.x);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! morphActive) return;
        morphActive = false;
        morphT = 0.0f;             // spring back: what you hear stays
        repaint();
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
        if (morphArea().contains (hoverAt))
            return "Morph: drag the thumb to blend this setup toward the "
                   "other bank. Release keeps what you hear; Undo takes the "
                   "whole morph back.";
        if (addArea().contains (hoverAt)) return "Add a band";
        if (lastVisiblePage() > 0
            && (arrowUpArea().contains (hoverAt) || arrowDownArea().contains (hoverAt)))
            return "Band pages: 24 chips at a time. A new page appears when "
                   "this one fills.";
        const int b = chipAt (hoverAt);
        if (b < 0) return {};
        if (! graph.bandParams (b).on)
            return "Band " + juce::String (b + 1)
                 + " - off. Double-click to switch it on.";
        return "Band " + juce::String (b + 1)
             + " - click selects, double-click mutes, right-click for the menu.";
    }

private:
    static constexpr int kRightControls = 104;  // morph strip, "+", A/B pill
    static constexpr int kPageSize = 24;
    static constexpr int kArrowW = 14;

    // The last page a user can reach: the highest page holding an ON band,
    // plus one more once that page is FULL - so there is always somewhere to
    // grow, and never an empty page to wander into.
    int lastVisiblePage() const
    {
        const int maxPage = (EqGraphView::kBands + kPageSize - 1) / kPageSize - 1;
        int last = 0;
        for (int b = 0; b < EqGraphView::kBands; ++b)
            if (graph.bandParams (b).on) last = juce::jmax (last, b / kPageSize);
        if (last < maxPage)
        {
            bool full = true;
            for (int i = 0; i < kPageSize && full; ++i)
                full = graph.bandParams (last * kPageSize + i).on;
            if (full) ++last;
        }
        return last;
    }

    float chipX0() const
    {
        return lastVisiblePage() > 0 ? (float) kArrowW : 0.0f;
    }

    juce::Rectangle<float> arrowUpArea() const
    {
        return { 0.0f, 0.0f, (float) kArrowW, getHeight() * 0.5f };
    }

    juce::Rectangle<float> arrowDownArea() const
    {
        return { 0.0f, getHeight() * 0.5f, (float) kArrowW, getHeight() * 0.5f };
    }

    int chipAt (juce::Point<float> pos) const
    {
        if (pos.x < chipX0()) return -1;
        const int i = (int) ((pos.x - chipX0()) / chipWidth());
        const int b = page * kPageSize + i;
        return i >= 0 && i < kPageSize && b < EqGraphView::kBands ? b : -1;
    }

    float chipWidth() const
    {
        return (float) (getWidth() - kRightControls - (int) chipX0())
             / (float) kPageSize;
    }

    juce::Rectangle<float> addArea() const
    {
        return { (float) getWidth() - 52.0f, 2.0f, 24.0f, (float) getHeight() - 4.0f };
    }

    juce::Rectangle<float> morphArea() const
    {
        return { (float) getWidth() - 102.0f, 4.0f, 46.0f, (float) getHeight() - 8.0f };
    }

    // W-20: one gesture morphs the live bank toward the spare.  The drag
    // starts from a SNAPSHOT of both banks, writes interpolated parameters
    // through the graph's one id spelling (one undo step), and the thumb
    // springs back on release - the morphed state IS the new A.
    void morphBegin()
    {
        auto* e = graph.eq();
        if (e == nullptr) return;
        morphBands.clear(); morphSrc.clear(); morphDst.clear();
        for (int b = 0; b < EqGraphView::kBands; ++b)
        {
            const auto a = e->getBand (b);
            const auto s = e->getSpareBand (b);
            if (! a.on && ! s.on) continue;
            const bool differ = a.on != s.on || a.type != s.type
                || a.channel != s.channel
                || std::abs (a.freqHz - s.freqHz) > 0.01f
                || std::abs (a.gainDb - s.gainDb) > 0.001f
                || std::abs (a.q - s.q) > 0.0001f
                || std::abs (a.slope - s.slope) > 0.01f
                || std::abs (a.placement - s.placement) > 0.001f;
            if (! differ) continue;
            morphBands.push_back (b);
            morphSrc.push_back (a);
            morphDst.push_back (s);
        }
        if (! morphBands.empty())
            beginParamUndoGesture (graph.processor().apvts,
                                   graph.paramId (morphBands[0], "gain"));
    }

    void morphApply (float t)
    {
        for (size_t i = 0; i < morphBands.size(); ++i)
        {
            const int b = morphBands[i];
            const auto& a = morphSrc[i];
            const auto& s = morphDst[i];
            const auto& d = t < 0.5f ? a : s;   // discrete fields snap halfway
            graph.setBandValue (b, "on",   d.on ? 1.0f : 0.0f);
            graph.setBandValue (b, "type", (float) (int) d.type);
            graph.setBandValue (b, "chan", (float) (int) d.channel);
            graph.setBandValue (b, "freq",
                a.freqHz * std::pow (s.freqHz / std::max (1.0f, a.freqHz), t));
            graph.setBandValue (b, "gain", a.gainDb + (s.gainDb - a.gainDb) * t);
            graph.setBandValue (b, "q",
                a.q * std::pow (s.q / std::max (0.05f, a.q), t));
            graph.setBandValue (b, "slope", a.slope + (s.slope - a.slope) * t);
            graph.setBandValue (b, "place",
                a.placement + (s.placement - a.placement) * t);
        }
    }

    juce::Rectangle<float> abArea() const
    {
        return { (float) getWidth() - 26.0f, 4.0f, 24.0f, (float) getHeight() - 8.0f };
    }

    void timerCallback() override
    {
        const int sel = graph.selectedBand();
        if (sel >= 0 && sel != lastSelected)
            page = sel / kPageSize;
        lastSelected = sel;
        repaint();
    }

    void morphDragTo (float x)
    {
        const auto mr = morphArea();
        morphT = juce::jlimit (0.0f, 1.0f,
                               (x - (mr.getX() + 4.0f)) / (mr.getWidth() - 8.0f));
        morphApply (morphT);
        repaint();
    }

    EqGraphView& graph;
    int page = 0, lastSelected = -1;
    juce::Point<float> hoverAt;
    bool morphActive = false;
    float morphT = 0.0f;
    std::vector<int> morphBands;
    std::vector<kbs::EqBandParams> morphSrc, morphDst;
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

    // Two separate wishes.  `collapsed` is the user's, set by clicking the
    // rail's left edge and never overwritten; `forcedCollapsed` is the
    // window's, set when there is not enough width left to show a graph
    // worth looking at.  Growing the window back restores the user's choice
    // instead of whatever the squeeze left behind.
    bool collapsed = false;
    bool forcedCollapsed = false;
    bool isCollapsed() const noexcept { return collapsed || forcedCollapsed; }

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

        auto gestureOnDragStart = [this] (juce::Slider& k, const char* field)
        {
            k.onDragStart = [this, field]
            {
                if (const int b = graph.selectedBand(); b >= 0)
                    beginParamUndoGesture (graph.processor().apvts,
                                           graph.paramId (b, field));
            };
        };

        initKnob (gain, "This band's gain");
        gain.setRange (-30.0, 30.0, 0.0);
        gain.setDoubleClickReturnValue (true, 0.0);
        gestureOnDragStart (gain, "gain");
        gain.onValueChange = [this]
        {
            if (const int b = graph.selectedBand(); b >= 0)
                graph.setBandValue (b, "gain", (float) gain.getValue());
        };

        initKnob (satK, "Saturate just this band's region: the slice at the "
                        "band's frequency and width is softened and folded "
                        "back. Zero = clean.");
        satK.setRange (0.0, 1.0, 0.0);
        satK.setDoubleClickReturnValue (true, 0.0);
        gestureOnDragStart (satK, "sat");
        satK.onValueChange = [this]
        {
            if (const int b = graph.selectedBand(); b >= 0)
                graph.setBandValue (b, "sat", (float) satK.getValue());
        };

        initKnob (pan, "Pans this band's effect across the stereo image: "
                       "hard left and the boost or cut lands only on the left "
                       "channel. Not an audio pan - the band's, and only on "
                       "gain types in the Stereo view.");
        pan.setRange (-1.0, 1.0, 0.0);
        pan.setDoubleClickReturnValue (true, 0.0);
        gestureOnDragStart (pan, "place");
        pan.onValueChange = [this]
        {
            if (const int b = graph.selectedBand(); b >= 0)
                graph.setBandValue (b, "place", (float) pan.getValue());
        };

        wireNumber (freq, "freq", true, 0.006f,
                    [] (float v) { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + "k"
                                                       : juce::String (v, 1); });
        freq.parse = [] (const juce::String& t) { return parseNoteNameHz (t); };
        freq.setTooltip ("The band's frequency. Drag, or double-click to type "
                         "- a number, or a note name like A4 or C#2+13.");
        wireNumber (q, "q", true, 0.008f,
                    [] (float v) { return juce::String (v, 2); });
        q.setTooltip ("Width. Higher is narrower - and on Low Pass and High "
                      "Pass it is RESONANCE: a peak right at the cutoff, any "
                      "slope. The wheel over the band's dot changes it too.");

        type.segs = {
            { fontaudio::FilterBell, {}, "Bell" },
            { fontaudio::FilterLowpass, {}, "Low Pass" },
            { fontaudio::FilterHighpass, {}, "High Pass" },
            { fontaudio::FilterShelvingLo, {}, "Low Shelf" },
            { fontaudio::FilterShelvingHi, {}, "High Shelf" },
            { fontaudio::FilterNotch, {}, "Notch" },
            { fontaudio::FilterBandpass, {}, "Band Pass" },
            { {}, "T", "Tilt - low shelf up, high shelf down, one pivot" },
            { {}, "AP", "All Pass - changes phase only, for aligning layered "
                        "sounds. Zero-latency modes; linear phase flattens it." },
        };
        type.columns = 3;              // 3 rows of 3: nine types, real room
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

        // W-6: slope is CONTINUOUS - drag any dB/oct, double-click to type
        // one, and the very top is Brickwall.  The band menu keeps the nine
        // classic detents.
        wireNumber (slope, "slope", false, 0.5f,
                    [] (float v)
                    {
                        if (kbs::eqSlopeIsBrickwall (v)) return juce::String ("Brick");
                        return juce::String (v, v < 10.0f ? 1 : 0) + "/oct";
                    });
        slope.setTooltip ("How steep the filter falls - any dB per octave, "
                          "drag or double-click to type. All the way up is "
                          "Brickwall, which exists in the linear-phase modes.");
        addAndMakeVisible (slope);

        // W-9: per-band phase, linear modes only.  0% = the mode's own
        // phase; 100% = this band minimum-phase (its pre-ring gone).
        wireNumber (phaseN, "phase", false, 0.004f,
                    [] (float v)
                    { return juce::String ((int) std::round (v * 100.0f)) + "%"; });
        phaseN.setTooltip ("This band's phase in the linear modes: 0% rides "
                           "the mode's linear phase, 100% makes just this "
                           "band minimum-phase - its pre-ring disappears.");
        addAndMakeVisible (phaseN);

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
        const std::pair<juce::Slider*, const char*> dynGestures[] = {
            { &thrK, "thr" }, { &ratK, "ratio" }, { &atkK, "atk" }, { &relK, "rel" },
            { &thrBK, "thrb" }, { &ratBK, "ratb" }, { &rngBK, "rngb" },
            { &onsK, "onset" }, { &denK, "dens" },
            { &lfoRateK, "lforate" }, { &lfoDepthK, "lfodepth" },
            { &envDepthK, "envdepth" } };
        for (const auto& kf : dynGestures)
            gestureOnDragStart (*kf.first, kf.second);
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

        // W-12: the below-threshold stage + onset-selective detection.
        initDynKnob (thrBK, "thrb", -60.0, 0.0, 0.0,
                     "The BELOW stage: the band moves when its signal drops "
                     "under this. All the way down = never.");
        thrBK.setDoubleClickReturnValue (true, -60.0);
        initDynKnob (ratBK, "ratb", 1.0, 20.0, 0.5, "How hard under the "
                     "below-threshold");
        ratBK.setDoubleClickReturnValue (true, 2.0);
        initDynKnob (rngBK, "rngb", -30.0, 30.0, 0.0,
                     "The below stage's travel and direction: up lifts quiet "
                     "detail, down pushes it away. Zero = the stage is off.");
        rngBK.setDoubleClickReturnValue (true, 0.0);
        initDynKnob (onsK, "onset", 0.0, 1.0, 0.0,
                     "Onset: how much the detector listens to attacks only. "
                     "Full right, a sustained note reads as nothing and only "
                     "hits register.");
        onsK.setDoubleClickReturnValue (true, 0.0);

        // W-1: how surgical a SPECTRAL band is about which bins it moves.
        // W-13: the modulator pair.  Lives outside the dynamics gate -
        // a static bell can wobble too.
        initDynKnob (lfoRateK, "lforate", 0.02, 20.0, 0.3, "LFO speed");
        lfoRateK.setDoubleClickReturnValue (true, 2.0);
        initDynKnob (lfoDepthK, "lfodepth", 0.0, 1.0, 0.0,
                     "How far the LFO swings this band. Zero = off.");
        lfoDepthK.setDoubleClickReturnValue (true, 0.0);
        initDynKnob (envDepthK, "envdepth", -1.0, 1.0, 0.0,
                     "The band follows its own level: up rides with the "
                     "material, down ducks against it. Zero = off.");
        envDepthK.setDoubleClickReturnValue (true, 0.0);

        lfoTgt.segs = {
            { {}, "F", "LFO moves the frequency" },
            { {}, "G", "LFO moves the gain" },
            { {}, "Q", "LFO moves the width" },
        };
        wireSegment (lfoTgt, "lfotgt");
        addAndMakeVisible (lfoTgt);
        envTgt.segs = {
            { {}, "F", "Envelope moves the frequency" },
            { {}, "G", "Envelope moves the gain" },
            { {}, "Q", "Envelope moves the width" },
        };
        wireSegment (envTgt, "envtgt");
        addAndMakeVisible (envTgt);

        initDynKnob (denK, "dens", 0.0, 1.0, 0.0,
                     "Spectral density: left, only real spikes over their "
                     "neighborhood register; right, individual resonances "
                     "are picked apart.");
        denK.setDoubleClickReturnValue (true, 0.5);

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

        // The collapse grip: two bars, the back one dim, so it reads as
        // something that slides rather than a decorative line.  White, because
        // the old single accent-tinted bar was near invisible against the
        // panel it sits on.
        const float gripY = getHeight() * 0.5f - 14.0f;
        g.setColour (VC::Text.withAlpha (0.28f));
        g.fillRoundedRectangle (7.5f, gripY, 2.0f, 28.0f, 1.0f);
        g.setColour (VC::Text.withAlpha (0.9f));
        g.fillRoundedRectangle (3.0f, gripY, 3.0f, 28.0f, 1.5f);

        const int b = graph.selectedBand();

        // The band's colour dot lives INSIDE the collapsed strip, flush to its
        // right edge: parked out at the full width it was clipped to a one
        // pixel sliver on collapse, which is worse than not drawing it.  Here
        // the collapsed rail still says which band is selected.
        if (b >= 0)
        {
            g.setColour (graph.bandColour (b));
            g.fillEllipse ((float) kCollapsedWidth - 10.0f, 7.0f, 9.0f, 9.0f);
        }

        // Nothing below this fits in 14px, and drawing it into a negative-width
        // box is how the label passes end up as rows of empty squares.
        if (isCollapsed()) return;

        g.setColour (VC::TextDim);
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawText (b >= 0 ? "BAND " + juce::String (b + 1) : "NO BAND",
                    18, 6, getWidth() - 32, 12, juce::Justification::centredLeft);

        auto caption = [&] (juce::Component& c, const char* text)
        {
            if (! c.isVisible()) return;
            g.setColour (VC::TextDim);
            g.setFont (juce::Font (juce::FontOptions (8.5f)));
            g.drawText (text, c.getX(), c.getBottom() + 1, c.getWidth(), 9,
                        juce::Justification::centred);
        };
        caption (gain, "GAIN");
        caption (satK, "SAT");
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

        if (mMaxScroll > 0)
        {
            const float track = (float) getHeight() - 26.0f;
            const float frac = (float) getHeight() / (float) (getHeight() + mMaxScroll);
            const float h = juce::jmax (18.0f, track * frac);
            const float t = (float) mScrollY / (float) mMaxScroll;
            g.setColour (VC::Accent.withAlpha (0.8f));
            g.fillRoundedRectangle ((float) getWidth() - 5.5f,
                                    20.0f + t * (track - h), 2.5f, h, 1.25f);
        }

        caption (thrK, "THR");
        caption (ratK, "RATIO");
        caption (atkK, "ATK");
        caption (relK, "REL");
        caption (thrBK, "THR-B");
        caption (ratBK, "RAT-B");
        caption (rngBK, "RNG-B");
        caption (onsK, "ONSET");
        caption (denK, "DENSE");
        caption (lfoRateK, "RATE");
        caption (lfoDepthK, "LFO");
        caption (envDepthK, "ENV");

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
            readout (satK, juce::String ((int) std::round (p.satAmt * 100)) + "%");
            readout (pan, std::abs (p.placement) < 0.02f ? juce::String ("C")
                          : (p.placement < 0 ? "L" : "R")
                              + juce::String ((int) std::round (std::abs (p.placement) * 100)));
            readout (thrK, juce::String (p.thresholdDb, 0));
            readout (ratK, juce::String (p.ratio, 1) + ":1");
            readout (atkK, juce::String (p.attackMs, p.attackMs < 10 ? 1 : 0));
            readout (relK, p.autoRelease ? juce::String ("auto")
                                         : juce::String (p.releaseMs, 0));
            readout (thrBK, juce::String (p.thresholdBDb, 0));
            readout (ratBK, juce::String (p.ratioB, 1) + ":1");
            readout (rngBK, std::abs (p.rangeBDb) < 0.05f ? juce::String ("off")
                            : (p.rangeBDb > 0 ? "+" : "")
                                + juce::String (p.rangeBDb, 0));
            readout (onsK, juce::String ((int) std::round (p.onsetMix * 100)) + "%");
            readout (denK, juce::String ((int) std::round (p.density * 100)) + "%");
            readout (lfoRateK, juce::String (p.lfoRateHz, p.lfoRateHz < 1.0f ? 2 : 1));
            readout (lfoDepthK, juce::String ((int) std::round (p.lfoDepth * 100)) + "%");
            readout (envDepthK, (p.envDepth > 0 ? "+" : "")
                                + juce::String ((int) std::round (p.envDepth * 100)) + "%");
        }
    }

    void resized() override { layout(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.position.x < 11.0f)
        {
            // A deliberate click outvotes the width-driven collapse: without
            // this the edge silently did nothing at any size narrow enough to
            // have folded the rail on its own.
            forcedCollapsed = false;
            collapsed = ! collapsed;
            if (onCollapse) onCollapse (collapsed);
        }
    }

    void mouseWheelMove (const juce::MouseEvent&,
                         const juce::MouseWheelDetails& w) override
    {
        if (mMaxScroll <= 0) return;
        mScrollY = juce::jlimit (0, mMaxScroll,
                                 mScrollY - juce::roundToInt (w.deltaY * 60.0f));
        layout();
        repaint();
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
            return std::strcmp (field, "freq") == 0 ? p.freqHz
                 : std::strcmp (field, "slope") == 0 ? p.slope
                 : std::strcmp (field, "phase") == 0 ? p.phaseMix : p.q;
        };
        d.set = [this, field] (float v)
        {
            if (const int b = graph.selectedBand(); b >= 0)
            {
                beginParamUndoGesture (graph.processor().apvts,
                                       graph.paramId (b, field));
                graph.setBandValue (b, field, v);
            }
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
            if (std::strcmp (field, "type") == 0) return (int) p.type;
            if (std::strcmp (field, "lfotgt") == 0) return p.lfoTarget;
            if (std::strcmp (field, "envtgt") == 0) return p.envTarget;
            return (int) p.slope;
        };
        row.set = [this, field] (int v)
        {
            if (const int b = graph.selectedBand(); b >= 0)
            {
                beginParamUndoGesture (graph.processor().apvts,
                                       graph.paramId (b, field));
                graph.setBandValue (b, field, (float) v);
            }
        };
    }

    void layout()
    {
        // Budgeted to fit the smallest window WITH the dynamics section and
        // its meter (the KBS third-pass arithmetic, kept), but only RESERVED
        // when that section is actually on screen: the block is 140 of the
        // rail's 335 px and stays hidden for every band that is not dynamic,
        // which is most bands most of the time.  Whatever still does not fit
        // scrolls instead of clipping, so the rail stays usable at any height
        // rather than at a threshold.
        if (isCollapsed()) return;

        auto full = getLocalBounds().reduced (12, 5);
        full.removeFromTop (15);                    // the header stays pinned

        // Read the same visibility the timer set - two predicates that could
        // disagree is how a layout starts reserving space for nothing.
        const bool showDynRow  = dynT.isVisible();
        const bool showDynBody = direction.isVisible();
        const bool showSlope   = slope.isVisible();
        const bool showPhase   = phaseN.isVisible();
        // Base rows (knobs through channel) + each optional part it actually
        // shows + the MOD block, which is ALWAYS placed: it belongs to any
        // band, dynamic or not - leaving it un-laid-out on a non-dynamic band
        // parked the MOD knobs at stale bounds over the rows above.
        const int need = 155 + (showSlope ? 26 : 0) + (showPhase ? 26 : 0)
                             + (showDynRow  ? 12 + 16 : 0)
                             + (showDynBody ? 4 + 16 + 4 + 200 : 0)
                             + 150;

        mMaxScroll = juce::jmax (0, need - full.getHeight());
        mScrollY   = juce::jlimit (0, mMaxScroll, mScrollY);

        auto r = full.withY (full.getY() - mScrollY).withHeight (need);

        auto knobs = r.removeFromTop (34);
        const int kw3 = knobs.getWidth() / 3;
        gain.setBounds (knobs.removeFromLeft (kw3).reduced (8, 0));
        satK.setBounds (knobs.removeFromLeft (kw3).reduced (8, 0));
        pan.setBounds (knobs.reduced (8, 0));
        r.removeFromTop (20);                              // caption + readout

        auto row = r.removeFromTop (27);
        freq.setBounds (row.removeFromLeft (row.getWidth() / 2 - 2));
        row.removeFromLeft (4);
        q.setBounds (row);
        r.removeFromTop (4);

        type.setBounds (r.removeFromTop (48));
        r.removeFromTop (3);
        chan.setBounds (r.removeFromTop (16));
        r.removeFromTop (3);
        if (showSlope) slope.setBounds (r.removeFromTop (26));
        if (showPhase) phaseN.setBounds (r.removeFromTop (26));

        if (showDynRow)
        {
            r.removeFromTop (12);
            dynTop = r.getY();
            auto tr = r.removeFromTop (16);
            const int tw = tr.getWidth() / 3;
            dynT.setBounds (tr.removeFromLeft (tw).reduced (1, 0));
            autoT.setBounds (tr.removeFromLeft (tw).reduced (1, 0));
            extT.setBounds (tr.reduced (1, 0));
        }

        if (showDynBody)
        {
            r.removeFromTop (4);
            direction.setBounds (r.removeFromTop (16));
            r.removeFromTop (4);

            auto body = r.removeFromTop (200);
            auto meterCol = body.removeFromRight (22);
            grMeter.setBounds (meterCol.reduced (0, 1));

            auto k1 = body.removeFromTop (30);
            const int kw = k1.getWidth() / 2;
            thrK.setBounds (k1.removeFromLeft (kw).reduced (8, 0));
            ratK.setBounds (k1.reduced (8, 0));
            body.removeFromTop (20);                       // captions + values
            auto k2 = body.removeFromTop (30);
            atkK.setBounds (k2.removeFromLeft (kw).reduced (8, 0));
            relK.setBounds (k2.reduced (8, 0));
            body.removeFromTop (20);
            auto k3 = body.removeFromTop (30);
            thrBK.setBounds (k3.removeFromLeft (kw).reduced (8, 0));
            ratBK.setBounds (k3.reduced (8, 0));
            body.removeFromTop (20);
            auto k4 = body.removeFromTop (30);
            rngBK.setBounds (k4.removeFromLeft (kw).reduced (8, 0));
            onsK.setBounds (k4.reduced (8, 0));
            body.removeFromTop (20);
            auto k5 = body.removeFromTop (30);
            denK.setBounds (k5.removeFromLeft (kw).reduced (8, 0));
        }

        // W-13 MOD: any band can modulate, so this lays out whether or not
        // the dynamics body is open.
        r.removeFromTop (12);
        auto m1 = r.removeFromTop (30);
        const int mw = m1.getWidth() / 2;
        lfoRateK.setBounds (m1.removeFromLeft (mw).reduced (8, 0));
        lfoDepthK.setBounds (m1.reduced (8, 0));
        r.removeFromTop (20);
        lfoTgt.setBounds (r.removeFromTop (16));
        r.removeFromTop (6);
        auto m2 = r.removeFromTop (30);
        envDepthK.setBounds (m2.removeFromLeft (mw).reduced (8, 0));
        r.removeFromTop (20);
        envTgt.setBounds (r.removeFromTop (16));
    }

    void syncFromParams()
    {
        const int b = graph.selectedBand();
        if (b < 0) return;
        slope.refresh();
        phaseN.refresh();
    }

    void timerCallback() override
    {
        const int b = graph.selectedBand();
        const bool show = b >= 0;
        const auto p = show ? graph.bandParams (b) : kbs::EqBandParams();

        // W-5: the FREQ caption doubles as the live note name.
        freq.setLabelText (show ? EqGraphView::noteName (p.freqHz)
                                : juce::String ("FREQ"));

        // Component ids carry the param spelling so the app's global
        // right-click (Automate / Type in value / MIDI Learn) reaches these
        // knobs like any other stamped control.
        if (show && b != stampedBand)
        {
            stampedBand = b;
            gain.setComponentID (graph.paramId (b, "gain"));
            satK.setComponentID (graph.paramId (b, "sat"));
            pan.setComponentID (graph.paramId (b, "place"));
            thrK.setComponentID (graph.paramId (b, "thr"));
            ratK.setComponentID (graph.paramId (b, "ratio"));
            atkK.setComponentID (graph.paramId (b, "atk"));
            relK.setComponentID (graph.paramId (b, "rel"));
            thrBK.setComponentID (graph.paramId (b, "thrb"));
            ratBK.setComponentID (graph.paramId (b, "ratb"));
            rngBK.setComponentID (graph.paramId (b, "rngb"));
            onsK.setComponentID (graph.paramId (b, "onset"));
            denK.setComponentID (graph.paramId (b, "dens"));
            lfoRateK.setComponentID (graph.paramId (b, "lforate"));
            lfoDepthK.setComponentID (graph.paramId (b, "lfodepth"));
            envDepthK.setComponentID (graph.paramId (b, "envdepth"));
        }

        gain.setVisible (show && kbs::eqTypeHasGain (p.type));
        if (gain.isVisible() && ! gain.isMouseButtonDown())
            gain.setValue (p.gainDb, juce::dontSendNotification);

        pan.setVisible (show && kbs::eqTypeHasGain (p.type)
                        && p.channel == kbs::EqChannel::stereo);
        if (pan.isVisible() && ! pan.isMouseButtonDown())
            pan.setValue (p.placement, juce::dontSendNotification);

        satK.setVisible (show && kbs::eqTypeHasGain (p.type));
        if (satK.isVisible() && ! satK.isMouseButtonDown())
            satK.setValue (p.satAmt, juce::dontSendNotification);

        slope.setVisible (show && kbs::eqTypeHasSlope (p.type));
        phaseN.setVisible (show && graph.eq() != nullptr
                           && kbs::eqModeIsLinear (graph.eq()->getMode()));
        chan.setVisible (show && graph.domainView() == DomainView::stereo);

        const bool dynOk = show && kbs::eqTypeSupportsDynamic (p.type);
        dynT.setVisible (dynOk);
        autoT.setVisible (dynOk && p.dynamic);
        extT.setVisible (dynOk && p.dynamic);

        const bool dyn = dynOk && p.dynamic;
        for (auto* k : { &thrK, &ratK, &atkK, &relK, &thrBK, &ratBK, &rngBK, &onsK })
            k->setVisible (dyn);
        denK.setVisible (dyn && p.spectral);
        for (auto* k : { &lfoRateK, &lfoDepthK, &envDepthK })
            k->setVisible (show);
        lfoTgt.setVisible (show);
        envTgt.setVisible (show);
        relK.setVisible (dyn && ! p.autoRelease);
        direction.setVisible (dyn);

        const bool bandChanged = b != lastLaidOutBand;
        const bool slopeVis = slope.isVisible(), phaseVis = phaseN.isVisible();
        if (bandChanged) { mScrollY = 0; lastLaidOutBand = b; }
        if (bandChanged || dynOk != lastDynRow || dyn != lastDynBody
            || slopeVis != lastSlopeRow || phaseVis != lastPhaseRow)
        {
            lastDynRow   = dynOk;
            lastDynBody  = dyn;
            lastSlopeRow = slopeVis;
            lastPhaseRow = phaseVis;
            layout();
            repaint();
        }
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

    juce::Slider gain, satK, pan, thrK, ratK, atkK, relK, thrBK, ratBK, rngBK, onsK, denK;
    juce::Slider lfoRateK, lfoDepthK, envDepthK;
    SegmentRow lfoTgt, envTgt;
    SegmentRow direction;
    DragNumber freq, q;
    SegmentRow type, chan;
    DragNumber slope { "SLOPE" };
    DragNumber phaseN { "PHASE" };
    juce::TextButton dynT, autoT, extT;
    GrMeter grMeter;
    int dynTop = 300;
    int mScrollY = 0, mMaxScroll = 0;
    bool lastDynRow = false, lastDynBody = false;
    bool lastSlopeRow = false, lastPhaseRow = false;
    int lastLaidOutBand = -2;
    int stampedBand = -1;
    bool syncing = false;

    juce::SharedResourcePointer<fontaudio::IconHelper> icons;
};

} // namespace eqview
