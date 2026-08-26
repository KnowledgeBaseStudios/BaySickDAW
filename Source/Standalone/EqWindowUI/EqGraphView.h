// BaySickDAW — the EQ graph (QA-EqPro).
//
// Ported from KBS EQ Pro's EqGraph and adapted to the strip world: the page
// IS the graph - log frequency across, gain up, and everything drawn into one
// surface: analyser, spectrogram, per-band fills, the summed curve, handles,
// phase, piano, collision.  Every curve pixel comes from the engine's own
// magnitude and phase queries; there is no drawing-side formula to drift -
// the rule that buried the old display's defect class (four band types drawn
// wrong from a parallel copy).
//
// DAW adaptations over the plugin original:
//  * Bound to a STRIP's EQ point through a live resolver (the node under a
//    window can be rebuilt without the window hearing about it), and to the
//    strip's params through the processor's one id spelling.  Null-safe: an
//    unresolvable EQ draws an empty plot.
//  * THREE DOMAIN VIEWS (SC-5, Jeff's 1b): Stereo / Mid / Side.  The view IS
//    the domain - a band lives in the view that made it, no routing gesture.
//    The other views draw as a live, non-interactive ghost so you always see
//    what the other half is doing.  Left/Right exist only as a picker on
//    Stereo-view bands; a band moves between views from its menu (SC-18).
//  * Our dark look (Jeff's addendum): the current EQ's ground, no brushed
//    plate - and the look is one-way, KBS keeps its own.
//
// One modifier does one thing (the KBS six-pass map, kept):
//   drag                 freq + gain        wheel (on a handle)   Q
//   Shift+drag           fine               Shift+wheel           fine Q
//   Ctrl+drag            gain only          Ctrl+Shift+drag       freq only
//   Alt+click            reset band         double-click empty    add band
//   double-click handle  mute               right-click handle    menu
#pragma once

#include <JuceHeader.h>
#include <fontaudio/fontaudio.h>
#include "EqAnalyser.h"
#include "../../DSP/StripEq.h"
#include "../../PluginProcessor.h"
#include "../UndoBracket.h"
#include "../SharedUI.h"

namespace eqview {

enum class DomainView { stereo = 0, mid, side };

inline DomainView viewOfChannel (kbs::EqChannel ch)
{
    if (ch == kbs::EqChannel::mid)  return DomainView::mid;
    if (ch == kbs::EqChannel::side) return DomainView::side;
    return DomainView::stereo;      // stereo, left, right all live here
}

inline const char* viewName (DomainView v)
{
    switch (v)
    {
        case DomainView::mid:  return "Mid";
        case DomainView::side: return "Side";
        default:               return "Stereo";
    }
}

// The house look: the current EQ's ground and grid, band hues stepped from
// the app's blue so 24 bands stay tellable-apart, curve in white glow.
struct Colors
{
    juce::Colour ground     { VC::EQGridBg };
    juce::Colour groundDeep { VC::EQGridBg.darker (0.35f) };
    juce::Colour outer      { VC::Bg };
    juce::Colour grid       { VC::EQGridLine };
    juce::Colour text       { VC::Text };
    juce::Colour textDim    { VC::TextDim };
    juce::Colour curve      { juce::Colours::white };
    juce::Colour zeroLine   { VC::Green };
    juce::Colour analyserPost { VC::Yellow };
    juce::Colour analyserSc { VC::Purple };
    juce::Colour meter      { VC::Green };
    juce::Colour panel      { VC::Panel };
    juce::Colour panelEdge  { VC::Accent };
    juce::Colour trackDark  { VC::Surface.darker (0.3f) };
    juce::Colour badge      { juce::Colour (0xffe0a030) };

    juce::Colour band (int b) const
    {
        return VC::Blue.withRotatedHue ((float) b * 0.13f)
                       .withSaturation (0.65f).withBrightness (0.85f);
    }
};

class EqGraphView : public juce::Component,
                    public juce::TooltipClient,
                    private juce::Timer
{
public:
    static constexpr int kBands = StripEq::kBands;

    // What the window wires up.
    std::function<StripEq*()> resolveEq;                    // live, per use
    std::function<void (int band)> onSelect;
    std::function<void (int band)> ensureBand;              // SC-2: register 9-24
    std::function<void()> onViewSettingsChanged;

    EqGraphView (BaySickDAWProcessor& processor, juce::String stripPrefixIn, bool preBank)
        : proc (processor), stripPrefix (std::move (stripPrefixIn)),
          bank (preBank ? 1 : 0)
    {
        setWantsKeyboardFocus (false);
        setOpaque (true);
        startTimerHz (30);
    }

    // ── binding ───────────────────────────────────────────────────────────
    StripEq* eq() const { return resolveEq ? resolveEq() : nullptr; }

    kbs::EqBandParams bandParams (int b) const
    {
        if (auto* e = eq()) return e->getBand (b);
        return {};
    }

    // ── view state (persisted in the EQ point's view tree) ────────────────
    juce::var viewProp (const juce::Identifier& id, const juce::var& fallback) const
    {
        if (auto* e = eq())
            if (e->viewTree().hasProperty (id)) return e->viewTree().getProperty (id);
        return fallback;
    }

    void setViewProperty (const juce::Identifier& id, const juce::var& v)
    {
        if (auto* e = eq()) e->viewTree().setProperty (id, v, nullptr);
        spectrogram = {};                    // sizes and modes may have moved
        if (onViewSettingsChanged) onViewSettingsChanged();
        repaint();
    }

    float gainScaleDb() const { return (float) (double) viewProp ("scale", 18.0); }
    bool showAnalyser() const { return (bool) viewProp ("analyser", true); }
    bool showSpectrogram() const { return (bool) viewProp ("spectrogram", false); }
    bool showPhase() const { return (bool) viewProp ("phase", false); }
    bool showPiano() const { return (bool) viewProp ("piano", false); }
    bool analyserPre() const { return (bool) viewProp ("pre", true); }
    bool analyserPost() const { return (bool) viewProp ("post", true); }
    bool analyserSc() const { return (bool) viewProp ("sc", false); }

    DomainView domainView() const
    {
        return (DomainView) juce::jlimit (0, 2, (int) viewProp ("view", 0));
    }

    void setDomainView (DomainView v)
    {
        if (v == domainView()) return;
        setViewProperty ("view", (int) v);
        if (selected >= 0 && viewOfChannel (bandParams (selected).channel) != v)
            selectBand (-1);
        repaint();
    }

    kbs::EqChannel domainOfCurrentView() const
    {
        switch (domainView())
        {
            case DomainView::mid:  return kbs::EqChannel::mid;
            case DomainView::side: return kbs::EqChannel::side;
            default:               return kbs::EqChannel::stereo;
        }
    }

    bool bandInCurrentView (int b) const
    {
        return viewOfChannel (bandParams (b).channel) == domainView();
    }

    int selectedBand() const { return selected; }

    void holdListen (bool down)
    {
        if (auto* e = eq())
            e->engine().setListenBand (down && selected >= 0 ? selected : -1);
        listening = down;
        repaint();
    }

    void setGrabArmed (bool armed)
    {
        grabArmed = armed;
        if (armed) post.beginArmHold();
        else { post.endArmHold(); grabValid = false; }
        repaint();
    }

    bool isGrabArmed() const { return grabArmed; }
    bool isListening() const { return listening; }

    void selectBand (int b)
    {
        if (listenLatched && b != selected)
        {
            listenLatched = false;
            holdListen (false);
        }
        selected = juce::jlimit (-1, kBands - 1, b);
        if (onSelect) onSelect (selected);
        repaint();
    }

    void toggleListenLatch()
    {
        listenLatched = ! listenLatched;
        holdListen (listenLatched);
    }

    void nudgeSelected (float freqSemis, float gainDb)
    {
        if (selected < 0) return;
        const auto p = bandParams (selected);
        if (freqSemis != 0.0f)
            setBandValue (selected, "freq",
                          p.freqHz * std::pow (2.0f, freqSemis / 12.0f));
        if (gainDb != 0.0f && kbs::eqTypeHasGain (p.type))
            setBandValue (selected, "gain", p.gainDb + gainDb);
    }

    void deleteSelected()
    {
        if (selected >= 0) removeBand (selected);
    }

    void cycleSelection (int dir)
    {
        int b = selected;
        for (int step = 0; step < kBands; ++step)
        {
            b = (b + dir + kBands) % kBands;
            if (bandParams (b).on && bandInCurrentView (b)) { selectBand (b); return; }
        }
    }

    juce::String getTooltip() override
    {
        if (grabButtonArea().contains (lastMouse))
            return grabArmed
                ? "Spectrum grab armed: click any found peak marker to drop a "
                  "cut bell on it. Click here again to disarm."
                : "Spectrum grab: arm, then click the resonance the analyser "
                  "finds. Peaks are held steady while armed.";
        if (selected >= 0 && listenButtonArea().contains (lastMouse))
            return "Listen to this band alone. Click to toggle; you can still "
                   "drag the band while listening.";
        return {};
    }

    // ── geometry ──────────────────────────────────────────────────────────
    juce::Rectangle<float> plotArea() const
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        r.removeFromBottom (16.0f);                        // frequency axis
        if (showPiano()) r.removeFromBottom (26.0f);
        return r;
    }

    float freqToX (double hz) const
    {
        const auto a = plotArea();
        const double t = std::log (juce::jlimit (20.0, 20000.0, hz) / 20.0)
                       / std::log (1000.0);
        return a.getX() + (float) t * a.getWidth();
    }

    double xToFreq (float x) const
    {
        const auto a = plotArea();
        const double t = juce::jlimit (0.0f, 1.0f, (x - a.getX()) / a.getWidth());
        return 20.0 * std::pow (1000.0, t);
    }

    float gainToY (float db) const
    {
        const auto a = plotArea();
        const float s = gainScaleDb();
        return juce::jmap (juce::jlimit (-s, s, db), -s, s, a.getBottom(), a.getY());
    }

    float yToGain (float y) const
    {
        const auto a = plotArea();
        return juce::jmap (juce::jlimit (a.getY(), a.getBottom(), y),
                           a.getBottom(), a.getY(), -gainScaleDb(), gainScaleDb());
    }

    // Handle anchor: gain types sit at their gain, the rest on the zero line.
    // ONE function for drawing, hit-testing and dragging alike (the old
    // display kept four copies and hovered the wrong spot - E5).
    juce::Point<float> handlePos (int b) const
    {
        const auto p = bandParams (b);
        const float y = kbs::eqTypeHasGain (p.type) ? gainToY (p.gainDb) : gainToY (0.0f);
        return { freqToX (p.freqHz), y };
    }

    juce::Colour bandColour (int b) const { return cols.band (b); }
    const Colors& colors() const { return cols; }

    // ── painting ──────────────────────────────────────────────────────────
    void paint (juce::Graphics& g) override
    {
        const auto a = plotArea();
        drawGround (g, a);
        drawGrid (g, a);

        auto* e = eq();
        if (e != nullptr)
        {
            if (showSpectrogram()) drawSpectrogram (g, a);
            if (showAnalyser()) drawAnalyser (g, a, *e);
            if (analyserSc() && e->scFeedAlive.load (std::memory_order_relaxed))
                drawCollision (g, a);

            drawGhostViews (g, a, *e);
            drawBandFills (g, a, *e);
            drawSummedCurve (g, a, *e);
            if (showPhase()) drawPhase (g, a, *e);
            drawHandles (g, *e);
        }
        if (showPiano()) drawPiano (g);
        drawAxis (g, a);

        if (grabValid && ! dragging) drawGrabMarker (g);
        drawGrabModeButton (g);

        if (dragging && selected >= 0) drawDragReadout (g);
        else if (hovered >= 0 && ! dragging) drawHoverPanel (g, hovered);
    }

    // ── interaction ───────────────────────────────────────────────────────
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int hit = bandAt (e.position);

        if (grabButtonArea().contains (e.position) && ! e.mods.isPopupMenu())
        {
            setGrabArmed (! grabArmed);
            return;
        }

        if (listenButtonArea().contains (e.position) && selected >= 0
            && ! e.mods.isPopupMenu())
        {
            // A toggle, not a hold: holding it captured the mouse and made
            // the band undraggable while auditioning - exactly when you want
            // to drag it (KBS test pass five).
            toggleListenLatch();
            return;
        }

        if (e.mods.isPopupMenu())
        {
            if (hit >= 0) { selectBand (hit); showBandMenu (hit); }
            return;
        }

        if (hit < 0 && grabArmed && grabValid)
        {
            // Armed IS the consent: any empty click takes the current marker.
            const int b = firstFreeBand();
            if (b >= 0)
            {
                if (ensureBand) ensureBand (b);
                beginParamUndoGesture (proc.apvts, paramId (b, "on"));
                setBandValue (b, "on", 1.0f);
                setBandValue (b, "type", 0.0f);
                setBandValue (b, "chan", (float) (int) domainOfCurrentView());
                setBandValue (b, "freq", (float) grabPeak.hz);
                setBandValue (b, "gain",
                              -juce::jlimit (2.0f, 12.0f, grabPeak.prominenceDb * 0.6f));
                setBandValue (b, "q", grabPeak.q);
                selectBand (b);
                grabValid = false;
                setGrabArmed (false);     // one grab per arming
                return;
            }
        }

        if (hit >= 0)
        {
            selectBand (hit);

            if (e.mods.isAltDown()) { resetBand (hit); return; }

            dragging = true;
            dragStart = e.position;
            const auto p = bandParams (hit);
            dragStartFreq = p.freqHz;
            dragStartGain = p.gainDb;
            beginParamUndoGesture (proc.apvts, paramId (hit, "freq"));
            beginGesture (hit, "freq");
            beginGesture (hit, "gain");
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging || selected < 0) return;

        const auto p = bandParams (selected);
        const bool fine = e.mods.isShiftDown() && ! e.mods.isCtrlDown();
        const float k = fine ? 0.2f : 1.0f;

        const bool gainOnly = e.mods.isCtrlDown() && ! e.mods.isShiftDown();
        const bool freqOnly = e.mods.isCtrlDown() && e.mods.isShiftDown();

        if (! gainOnly)
        {
            const float dx = (e.position.x - dragStart.x) * k;
            const auto a = plotArea();
            const double t0 = std::log (dragStartFreq / 20.0) / std::log (1000.0);
            const double t = t0 + dx / a.getWidth();
            setBandValue (selected, "freq", (float) (20.0 * std::pow (1000.0, t)));
        }

        if (! freqOnly && kbs::eqTypeHasGain (p.type))
        {
            const float dy = (e.position.y - dragStart.y) * k;
            const float s = gainScaleDb();
            const auto a = plotArea();
            const float dDb = -dy * (2.0f * s) / a.getHeight();
            setBandValue (selected, "gain", dragStartGain + dDb);
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging && selected >= 0)
        {
            endGesture (selected, "freq");
            endGesture (selected, "gain");
        }
        dragging = false;
        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        lastMouse = e.position;
        const int hit = bandAt (e.position);
        if (hit != hovered) { hovered = hit; repaint(); }

        grabValid = false;
        if (hit < 0 && grabArmed && showAnalyser())
        {
            EqAnalyser::Peak pk;
            if (post.findPeakNear (xToFreq (e.position.x), pk, true))
            {
                grabPeak = pk;
                grabValid = true;
            }
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hovered != -1) { hovered = -1; repaint(); }
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int hit = bandAt (e.position);
        if (hit >= 0) { toggleBand (hit, "mute"); return; }   // delete is a menu decision
        addBandAt (e.position);
    }

    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& wheel) override
    {
        // The pointer must be ON a handle - the fallback to the selected band
        // is how a KBS test session ended with two bands at Q 30 and nobody
        // remembering asking for either.
        if (hovered < 0) return;
        const auto p = bandParams (hovered);
        const float step = e.mods.isShiftDown() ? 0.08f : 0.25f;
        setBandValue (hovered, "q", p.q * std::pow (2.0f, wheel.deltaY * step));
    }

    // ── band lifecycle (shared with chips and menus) ──────────────────────
    void addBandAt (juce::Point<float> pos)
    {
        const int b = firstFreeBand();
        if (b < 0) return;
        if (ensureBand) ensureBand (b);

        beginParamUndoGesture (proc.apvts, paramId (b, "on"));
        setBandValue (b, "on", 1.0f);
        setBandValue (b, "type", 0.0f);
        // The view IS the domain (SC-5): a band made in the Side view works
        // the sides, no routing gesture anywhere.
        setBandValue (b, "chan", (float) (int) domainOfCurrentView());
        setBandValue (b, "freq", (float) xToFreq (pos.x));
        setBandValue (b, "gain", juce::jlimit (-30.0f, 30.0f, yToGain (pos.y)));
        setBandValue (b, "q", 0.707f);
        selectBand (b);
    }

    void enableBand (int b)
    {
        if (b < 0 || b >= kBands) return;
        if (ensureBand) ensureBand (b);
        beginParamUndoGesture (proc.apvts, paramId (b, "on"));
        setBandValue (b, "on", 1.0f);
        setBandValue (b, "chan", (float) (int) domainOfCurrentView());
        selectBand (b);
    }

    void removeBand (int b)
    {
        // Delete = off plus a reset to defaults, so re-enabling is a fresh
        // band rather than a ghost of this one.
        beginParamUndoGesture (proc.apvts, paramId (b, "on"));
        resetBand (b, false);
        setBandValue (b, "on", 0.0f);
        if (selected == b) selectBand (-1);
    }

    void resetBand (int b, bool ownTransaction = true)
    {
        if (ownTransaction)
            beginParamUndoGesture (proc.apvts, paramId (b, "gain"));
        const char* fields[] = { "gain", "q", "place", "mute", "iso", "dyn",
                                 "thr", "ratio", "atk", "rel", "relauto",
                                 "range", "scsrc", "slope", "type" };
        for (auto* f : fields)
            if (auto* par = proc.apvts.getParameter (paramId (b, f)))
                par->setValueNotifyingHost (par->getDefaultValue());

        // Reset keeps the band in ITS view: the domain is structure, not a
        // value (SC-5), so "chan" goes back to the view's domain rather than
        // the parameter default.
        setBandValue (b, "chan", (float) (int) viewOfChannel (bandParams (b).channel));

        // The first eight bands have home positions - the classic spread
        // they wake up on.  Later bands were placed by hand: teleporting one
        // to an interpolated default would lose the only meaningful
        // frequency it ever had (KBS test pass six).
        if (b < 8)
            if (auto* par = proc.apvts.getParameter (paramId (b, "freq")))
                par->setValueNotifyingHost (par->getDefaultValue());
        repaint();
    }

    void moveBandToView (int b, DomainView v)
    {
        // SC-18: the move re-domains the band in place, settings kept.
        beginParamUndoGesture (proc.apvts, paramId (b, "chan"));
        const kbs::EqChannel ch = v == DomainView::mid  ? kbs::EqChannel::mid
                                : v == DomainView::side ? kbs::EqChannel::side
                                                        : kbs::EqChannel::stereo;
        setBandValue (b, "chan", (float) (int) ch);
        if (selected == b && v != domainView()) selectBand (-1);
        repaint();
    }

    int firstFreeBand() const
    {
        for (int b = 0; b < kBands; ++b)
            if (! bandParams (b).on) return b;
        return -1;
    }

    // ── the band menu ─────────────────────────────────────────────────────
    void showBandMenu (int b)
    {
        const auto p = bandParams (b);
        juce::PopupMenu m;

        juce::PopupMenu type;
        const char* typeNames[] = { "Bell", "Low Pass", "High Pass", "Low Shelf",
                                    "High Shelf", "Notch", "Band Pass", "Tilt" };
        for (int t = 0; t < 8; ++t)
            type.addItem (100 + t, typeNames[t], true, (int) p.type == t);
        m.addSubMenu ("Type", type);

        if (kbs::eqTypeHasSlope (p.type))
        {
            juce::PopupMenu slope;
            const char* slopeNames[] = { "6 dB/oct", "12 dB/oct", "18 dB/oct",
                                         "24 dB/oct", "36 dB/oct", "48 dB/oct",
                                         "72 dB/oct", "96 dB/oct", "Brickwall" };
            for (int sl = 0; sl < 9; ++sl)
                slope.addItem (200 + sl, slopeNames[sl], true, p.slope == sl);
            m.addSubMenu ("Slope", slope);
        }

        // Channel only in the Stereo view, and only the choices that mean
        // something there: Stereo / Left / Right.  Mid and Side are VIEWS -
        // left/right do not exist inside them, so the picker never lies.
        if (domainView() == DomainView::stereo)
        {
            juce::PopupMenu chan;
            chan.addItem (300, "Stereo", true, p.channel == kbs::EqChannel::stereo);
            chan.addItem (303, "Left",   true, p.channel == kbs::EqChannel::left);
            chan.addItem (304, "Right",  true, p.channel == kbs::EqChannel::right);
            m.addSubMenu ("Channel", chan);
        }

        // SC-18: move between views, settings kept.
        {
            juce::PopupMenu move;
            const DomainView cur = domainView();
            if (cur != DomainView::stereo) move.addItem (310, "Stereo view");
            if (cur != DomainView::mid)    move.addItem (311, "Mid view");
            if (cur != DomainView::side)   move.addItem (312, "Side view");
            m.addSubMenu ("Move to", move);
        }

        if (kbs::eqTypeSupportsDynamic (p.type))
        {
            juce::PopupMenu dyn;
            dyn.addItem (400, "Make Dynamic", true, p.dynamic);
            dyn.addItem (401, "Auto Release", p.dynamic, p.autoRelease);
            m.addSubMenu ("Dynamic", dyn);
        }

        m.addItem (500, "Listen", true, listenLatched && selected == b);
        m.addItem (501, "Isolate", true, p.isolated);
        m.addItem (504, "Mute", true, p.muted);
        m.addSeparator();
        m.addItem (502, "Reset Band");
        m.addItem (503, "Delete Band");

        // At the mouse, not at the component: withTargetComponent alone put
        // the menu at the graph's corner (the same trap as the browser trees).
        const auto at = juce::Rectangle<int> (1, 1)
                            .withPosition (juce::Desktop::getMousePosition());
        m.showMenuAsync (juce::PopupMenu::Options()
                             .withTargetComponent (this)
                             .withTargetScreenArea (at),
            [this, b] (int r)
            {
                if (r == 0) return;
                if (r >= 100 && r < 108)
                {
                    beginParamUndoGesture (proc.apvts, paramId (b, "type"));
                    setBandValue (b, "type", (float) (r - 100));
                }
                else if (r >= 200 && r < 209)
                {
                    beginParamUndoGesture (proc.apvts, paramId (b, "slope"));
                    setBandValue (b, "slope", (float) (r - 200));
                }
                else if (r == 300 || r == 303 || r == 304)
                {
                    beginParamUndoGesture (proc.apvts, paramId (b, "chan"));
                    setBandValue (b, "chan", (float) (r - 300));
                }
                else if (r >= 310 && r <= 312)
                    moveBandToView (b, (DomainView) (r - 310));
                else if (r == 400) toggleDynamic (b);
                else if (r == 401) toggleBand (b, "relauto");
                else if (r == 500) { selectBand (b); toggleListenLatch(); }
                else if (r == 501) toggleBand (b, "iso");
                else if (r == 504) toggleBand (b, "mute");
                else if (r == 502) resetBand (b);
                else if (r == 503) removeBand (b);
            });
    }

    // ── parameter plumbing ────────────────────────────────────────────────
    // Field tokens are the KBS vocabulary; the map below is the ONE place
    // they meet the DAW's suffix spelling.
    juce::String paramId (int b, const char* field) const
    {
        return BaySickDAWProcessor::eqBandParamId (stripPrefix, bank, b,
                                                   suffixFor (field));
    }

    static const char* suffixFor (const char* field)
    {
        struct Pair { const char* f; const char* s; };
        static const Pair map[] = {
            { "freq", "Freq" }, { "gain", "Gain" }, { "q", "Q" },
            { "type", "Type" }, { "on", "On" }, { "slope", "Slope" },
            { "chan", "Channel" }, { "place", "Place" }, { "mute", "Mute" },
            { "iso", "Isolate" }, { "dyn", "Dynamic" }, { "thr", "Threshold" },
            { "ratio", "Ratio" }, { "atk", "Attack" }, { "rel", "Release" },
            { "relauto", "AutoRelease" }, { "range", "Range" },
            { "scsrc", "ScSource" },
        };
        for (const auto& e : map)
            if (std::strcmp (e.f, field) == 0) return e.s;
        jassertfalse;
        return "Freq";
    }

    void setBandValue (int b, const char* field, float natural)
    {
        if (b >= 8 && ensureBand) ensureBand (b);
        if (auto* par = proc.apvts.getParameter (paramId (b, field)))
            par->setValueNotifyingHost (par->convertTo0to1 (natural));
    }

    void toggleBand (int b, const char* field)
    {
        beginParamUndoGesture (proc.apvts, paramId (b, field));
        if (auto* par = proc.apvts.getParameter (paramId (b, field)))
            par->setValueNotifyingHost (par->getValue() >= 0.5f ? 0.0f : 1.0f);
    }

    // The dynamics model is the plugin's (spec'd against Newtone): a
    // direction - compress or expand - and the threshold and ratio do the
    // deciding.  No Range knob; the range parameter carries the direction as
    // its sign at full travel.
    void toggleDynamic (int b)
    {
        const auto p = bandParams (b);
        beginParamUndoGesture (proc.apvts, paramId (b, "dyn"));
        if (! p.dynamic && std::abs (p.rangeDb) < 0.01f)
            setBandValue (b, "range", -30.0f);      // compress until told otherwise
        if (auto* par = proc.apvts.getParameter (paramId (b, "dyn")))
            par->setValueNotifyingHost (par->getValue() >= 0.5f ? 0.0f : 1.0f);
    }

    void setDynamicDirection (int b, bool expand)
    {
        beginParamUndoGesture (proc.apvts, paramId (b, "range"));
        setBandValue (b, "range", expand ? 30.0f : -30.0f);
    }

    void beginGesture (int b, const char* field)
    {
        if (auto* par = proc.apvts.getParameter (paramId (b, field)))
            par->beginChangeGesture();
    }

    void endGesture (int b, const char* field)
    {
        if (auto* par = proc.apvts.getParameter (paramId (b, field)))
            par->endChangeGesture();
    }

    // For the window's shot/test seams and the options menu.
    EqAnalyser& preAnalyser() { return pre; }
    EqAnalyser& postAnalyser() { return post; }
    EqAnalyser& scAnalyser() { return sc; }
    void pollNow() { timerCallback(); }

private:
    // ── drawing pieces ────────────────────────────────────────────────────
    void drawGround (juce::Graphics& g, juce::Rectangle<float> a)
    {
        // Our dark look (Jeff): the current EQ's ground, flat chassis around
        // it - no brushed plate, that stays KBS-side.
        g.fillAll (cols.outer);
        g.setGradientFill (juce::ColourGradient (cols.groundDeep, 0.0f, a.getY(),
                                                 cols.ground, 0.0f, a.getBottom(),
                                                 false));
        g.fillRect (a);
        g.setGradientFill (juce::ColourGradient (
            juce::Colours::black.withAlpha (0.35f), 0.0f, a.getY(),
            juce::Colours::transparentBlack, 0.0f, a.getY() + 14.0f, false));
        g.fillRect (a.withHeight (14.0f));
    }

    void drawDottedVertical (juce::Graphics& g, float x,
                             juce::Rectangle<float> a, float alpha)
    {
        // Dots read as air, lines read as a cage.
        g.setColour (cols.grid.brighter (0.6f).withAlpha (alpha));
        for (float y = a.getY() + 3.0f; y < a.getBottom(); y += 5.0f)
            g.fillRect (x, y, 1.0f, 1.5f);
    }

    void drawGrid (juce::Graphics& g, juce::Rectangle<float> a)
    {
        const double minors[] = { 30, 40, 60, 70, 80, 90, 200, 300, 400, 600,
                                  700, 800, 900, 2000, 3000, 4000, 6000, 7000,
                                  8000, 9000, 15000 };
        for (double hz : minors)
            drawDottedVertical (g, freqToX (hz), a, 0.10f);

        for (double hz : { 50.0, 100.0, 500.0, 1000.0, 5000.0, 10000.0 })
            drawDottedVertical (g, freqToX (hz), a, 0.25f);

        const float sc2 = gainScaleDb();
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        for (float db = -sc2; db <= sc2 + 0.01f; db += sc2 / 3.0f)
        {
            const bool zero = std::abs (db) < 0.01f;
            const float y = gainToY (db);
            if (zero)
            {
                g.setColour (cols.zeroLine.withAlpha (0.30f));
                g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
            }
            else
            {
                g.setColour (cols.grid.brighter (0.5f).withAlpha (0.12f));
                for (float x = a.getX() + 3.0f; x < a.getRight(); x += 5.0f)
                    g.fillRect (x, y, 1.5f, 1.0f);
            }

            g.setColour (cols.textDim.withAlpha (0.75f));
            g.drawText ((db > 0 ? "+" : "") + juce::String ((int) std::round (db)),
                        (int) a.getX() + 3, (int) y - 11, 26, 10,
                        juce::Justification::left);
        }
    }

    void drawAxis (juce::Graphics& g, juce::Rectangle<float> a)
    {
        g.setColour (cols.textDim);
        g.setFont (juce::Font (juce::FontOptions (9.5f)));
        for (double hz : { 30.0, 50.0, 100.0, 200.0, 500.0, 1000.0,
                           2000.0, 5000.0, 10000.0, 20000.0 })
        {
            const auto label = hz >= 1000.0 ? juce::String (hz / 1000.0, 0) + "k"
                                            : juce::String ((int) hz);
            g.drawText (label, (int) freqToX (hz) - 18, (int) a.getBottom() + 3,
                        36, 11, juce::Justification::centred);
        }
    }

    void drawAnalyser (juce::Graphics& g, juce::Rectangle<float> a, StripEq& e)
    {
        juce::ignoreUnused (e);
        auto xToHz = [this] (float x) { return xToFreq (x); };

        if (analyserPre())
        {
            g.setGradientFill (juce::ColourGradient (
                cols.textDim.withAlpha (0.12f), 0.0f, a.getY(),
                cols.textDim.withAlpha (0.02f), 0.0f, a.getBottom(), false));
            g.fillPath (pre.buildPath (a, -80.0f, xToHz));
        }
        if (analyserPost())
        {
            const auto path = post.buildPath (a, -80.0f, xToHz);
            g.setGradientFill (juce::ColourGradient (
                cols.analyserPost.withAlpha (0.28f), 0.0f, a.getY(),
                cols.analyserPost.withAlpha (0.04f), 0.0f, a.getBottom(), false));
            g.fillPath (path);
            g.setColour (cols.analyserPost.withAlpha (0.45f));
            g.strokePath (path, juce::PathStrokeType (1.0f));
        }
        if (analyserSc())
        {
            g.setColour (cols.analyserSc.withAlpha (0.40f));
            g.strokePath (sc.buildPath (a, -80.0f, xToHz),
                          juce::PathStrokeType (1.0f));
        }
        if (post.peakHold)
        {
            g.setColour (cols.text.withAlpha (0.35f));
            g.strokePath (post.buildPath (a, -80.0f, xToHz, true),
                          juce::PathStrokeType (1.0f));
        }
    }

    void drawSpectrogram (juce::Graphics& g, juce::Rectangle<float> a)
    {
        if (! spectrogram.isValid()
            || spectrogram.getWidth() != (int) a.getWidth()
            || spectrogram.getHeight() != (int) a.getHeight())
            spectrogram = juce::Image (juce::Image::ARGB,
                                       juce::jmax (16, (int) a.getWidth()),
                                       juce::jmax (16, (int) a.getHeight()), true);
        g.drawImageAt (spectrogram, (int) a.getX(), (int) a.getY());
    }

    void drawCollision (juce::Graphics& g, juce::Rectangle<float> a)
    {
        // Masking view: where the input and the sidechain both carry energy,
        // tint the band between the two outlines.
        g.setColour (juce::Colour (0xffcc4444).withAlpha (0.30f));
        const int w = (int) a.getWidth();
        for (int px = 0; px < w; px += 3)
        {
            const double hz = xToFreq (a.getX() + (float) px);
            const float inDb = post.dbAt (hz);
            const float scDb = sc.dbAt (hz);
            if (juce::jmin (inDb, scDb) < -55.0f) continue;
            const float y1 = juce::jmap (juce::jlimit (-80.0f, 0.0f, inDb),
                                         -80.0f, 0.0f, a.getBottom(), a.getY());
            const float y2 = juce::jmap (juce::jlimit (-80.0f, 0.0f, scDb),
                                         -80.0f, 0.0f, a.getBottom(), a.getY());
            g.fillRect ((float) a.getX() + px, juce::jmin (y1, y2),
                        3.0f, std::abs (y1 - y2) + 1.0f);
        }
    }

    // A view's summed curve: the product of ITS bands' own responses - each
    // pixel still the engine's arithmetic, multiplied display-side.  The
    // views are separate pictures by design (Jeff): the Mid view shows the
    // mid processing, not the whole EQ.
    float viewCurveDb (StripEq& e, DomainView v, double hz) const
    {
        double m = 1.0;
        for (int b = 0; b < kBands; ++b)
        {
            const auto p = e.getBand (b);
            if (! p.on || p.muted) continue;
            if (viewOfChannel (p.channel) != v) continue;
            m *= e.engine().bandMagnitudeAt (b, (float) hz);
        }
        return 20.0f * (float) std::log10 (juce::jmax (1.0e-6, m));
    }

    // The other two views, ghosted: live, dimmed, never interactive - so the
    // half you are not editing stays visible while you work (Jeff).
    void drawGhostViews (juce::Graphics& g, juce::Rectangle<float> a, StripEq& e)
    {
        const int w = (int) a.getWidth();
        for (int vi = 0; vi < 3; ++vi)
        {
            const auto v = (DomainView) vi;
            if (v == domainView()) continue;

            bool any = false;
            for (int b = 0; b < kBands && ! any; ++b)
            {
                const auto p = e.getBand (b);
                any = p.on && ! p.muted && viewOfChannel (p.channel) == v;
            }
            if (! any) continue;

            juce::Path path;
            for (int px = 0; px <= w; px += 3)
            {
                const double hz = xToFreq (a.getX() + (float) px);
                const float y = gainToY (viewCurveDb (e, v, hz));
                if (px == 0) path.startNewSubPath (a.getX(), y);
                else path.lineTo (a.getX() + (float) px, y);
            }
            g.setColour (cols.text.withAlpha (0.16f));
            g.strokePath (path, juce::PathStrokeType (1.2f));

            // Their dots, faint and small - landmarks, not targets.
            for (int b = 0; b < kBands; ++b)
            {
                const auto p = e.getBand (b);
                if (! p.on || viewOfChannel (p.channel) != v) continue;
                const auto pos = handlePos (b);
                g.setColour (bandColour (b).withAlpha (0.25f));
                g.fillEllipse (pos.x - 4.0f, pos.y - 4.0f, 8.0f, 8.0f);
            }
        }
    }

    void drawBandFills (juce::Graphics& g, juce::Rectangle<float> a, StripEq& e)
    {
        const float zeroY = gainToY (0.0f);
        for (int b = 0; b < kBands; ++b)
        {
            const auto p = e.getBand (b);
            if (! p.on || p.muted) continue;
            if (viewOfChannel (p.channel) != domainView()) continue;

            juce::Path curve;
            juce::Path path;
            path.startNewSubPath (a.getX(), zeroY);
            const int w = (int) a.getWidth();
            for (int px = 0; px <= w; px += 2)
            {
                const double hz = xToFreq (a.getX() + (float) px);
                const float db = 20.0f * std::log10 (juce::jmax (1.0e-6f,
                    e.engine().bandMagnitudeAt (b, (float) hz)));
                const float y = gainToY (db);
                path.lineTo (a.getX() + (float) px, y);
                if (px == 0) curve.startNewSubPath (a.getX(), y);
                else curve.lineTo (a.getX() + (float) px, y);
            }
            path.lineTo (a.getRight(), zeroY);
            path.closeSubPath();

            const bool sel = b == selected;
            const auto col = bandColour (b);
            const auto bounds = path.getBounds();
            if (bounds.getHeight() > 1.0f)
            {
                const bool boost = bounds.getY() < zeroY - 1.0f;
                g.setGradientFill (juce::ColourGradient (
                    col.withAlpha (sel ? 0.34f : 0.13f),
                    0.0f, boost ? bounds.getY() : bounds.getBottom(),
                    col.withAlpha (0.02f), 0.0f, zeroY, false));
                g.fillPath (path);
            }
            if (sel)
            {
                g.setColour (col.withAlpha (0.85f));
                g.strokePath (curve, juce::PathStrokeType (1.4f));
            }

            // The dashed ghost is the EXTENT: the furthest the band can
            // travel, drawn from the same expression that bounds the gain
            // computer - the live curve cannot pass it.
            if (sel && p.dynamic)
            {
                juce::Path ghost;
                for (int px = 0; px <= w; px += 3)
                {
                    const double hz = xToFreq (a.getX() + (float) px);
                    const float db = 20.0f * std::log10 (juce::jmax (1.0e-6f,
                        e.engine().bandExtentMagnitudeAt (b, (float) hz)));
                    const float y = gainToY (db);
                    if (px == 0) ghost.startNewSubPath (a.getX(), y);
                    else ghost.lineTo (a.getX() + (float) px, y);
                }
                const float dashes[] = { 4.0f, 4.0f };
                juce::PathStrokeType (1.2f).createDashedStroke (ghost, ghost, dashes, 2);
                g.setColour (col.withAlpha (0.55f));
                g.fillPath (ghost);
            }
        }
    }

    void drawSummedCurve (juce::Graphics& g, juce::Rectangle<float> a, StripEq& e)
    {
        const int w = (int) a.getWidth();

        juce::Path path;
        path.preallocateSpace (w * 2 + 8);
        for (int px = 0; px <= w; px += 1)
        {
            const double hz = xToFreq (a.getX() + (float) px);
            const float y = gainToY (viewCurveDb (e, domainView(), hz));
            if (px == 0) path.startNewSubPath (a.getX(), y);
            else path.lineTo (a.getX() + (float) px, y);
        }

        {
            juce::Path fill (path);
            fill.lineTo (a.getRight(), gainToY (0.0f));
            fill.lineTo (a.getX(), gainToY (0.0f));
            fill.closeSubPath();
            g.setColour (cols.curve.withAlpha (0.10f));
            g.fillPath (fill);
        }

        // Glow: the same path three times, wide and faint underneath, tight
        // and bright on top - most of what makes the curve read as lit.
        g.setColour (cols.curve.withAlpha (0.08f));
        g.strokePath (path, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        g.setColour (cols.curve.withAlpha (0.25f));
        g.strokePath (path, juce::PathStrokeType (3.6f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        g.setColour (cols.curve.withAlpha (0.95f));
        g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    void drawPhase (juce::Graphics& g, juce::Rectangle<float> a, StripEq& e)
    {
        juce::Path path;
        const float zeroY = a.getCentreY();
        const float scale = a.getHeight() * 0.5f / juce::MathConstants<float>::pi;
        const int w = (int) a.getWidth();
        for (int px = 0; px <= w; px += 2)
        {
            const double hz = xToFreq (a.getX() + (float) px);
            float ph = e.engine().phaseAt ((float) hz);
            while (ph > juce::MathConstants<float>::pi) ph -= juce::MathConstants<float>::twoPi;
            while (ph < -juce::MathConstants<float>::pi) ph += juce::MathConstants<float>::twoPi;
            const float y = juce::jlimit (a.getY(), a.getBottom(), zeroY - ph * scale);
            if (px == 0) path.startNewSubPath (a.getX(), y);
            else path.lineTo (a.getX() + (float) px, y);
        }
        g.setColour (juce::Colour (0xffff8800).withAlpha (0.6f));
        g.strokePath (path, juce::PathStrokeType (1.2f));
    }

    juce::String typeGlyph (kbs::EqType t) const
    {
        switch (t)
        {
            case kbs::EqType::bell:      return fontaudio::FilterBell;
            case kbs::EqType::lowPass:   return fontaudio::FilterLowpass;
            case kbs::EqType::highPass:  return fontaudio::FilterHighpass;
            case kbs::EqType::lowShelf:  return fontaudio::FilterShelvingLo;
            case kbs::EqType::highShelf: return fontaudio::FilterShelvingHi;
            case kbs::EqType::notch:     return fontaudio::FilterNotch;
            case kbs::EqType::bandPass:  return fontaudio::FilterBandpass;
            default:                     return {};         // Tilt: drawn
        }
    }

    void drawHandles (juce::Graphics& g, StripEq& e)
    {
        for (int b = 0; b < kBands; ++b)
        {
            const auto p = e.getBand (b);
            if (! p.on) continue;
            if (viewOfChannel (p.channel) != domainView()) continue;

            const auto pos = handlePos (b);
            const bool sel = b == selected;
            const float r = sel ? 10.0f : 8.0f;
            auto col = bandColour (b);
            if (p.muted) col = col.withSaturation (0.15f).withAlpha (0.6f);

            // Dynamic bands wear a mini meter beside the handle: fixed
            // 3 x 22 px, |GR| against 24 dB - the rail's meter in miniature,
            // never scaled by the graph's own axis.
            if (p.dynamic)
            {
                const float gr = e.engine().bandGrDb (b);
                const auto track = juce::Rectangle<float> (pos.x + r + 4.0f,
                                                           pos.y - 11.0f, 3.0f, 22.0f);
                g.setColour (cols.trackDark.withAlpha (0.8f));
                g.fillRoundedRectangle (track, 1.5f);
                const float t = juce::jlimit (0.0f, 1.0f, std::abs (gr) / 24.0f);
                if (t > 0.01f)
                {
                    auto fill = track.reduced (0.5f);
                    fill = gr < 0 ? fill.removeFromTop (fill.getHeight() * t)
                                  : fill.removeFromBottom (fill.getHeight() * t);
                    g.setColour (cols.meter);
                    g.fillRoundedRectangle (fill, 1.5f);
                }
            }

            if (sel)
            {
                g.setColour (col.withAlpha (0.25f));
                g.fillEllipse (pos.x - r - 5.0f, pos.y - r - 5.0f,
                               (r + 5.0f) * 2.0f, (r + 5.0f) * 2.0f);
            }

            g.setColour (sel ? col : cols.ground.withAlpha (0.85f));
            g.fillEllipse (pos.x - r, pos.y - r, r * 2, r * 2);
            g.setColour (col);
            g.drawEllipse (pos.x - r, pos.y - r, r * 2, r * 2, sel ? 2.0f : 1.5f);

            g.setColour (sel ? cols.ground : col);
            g.setFont (juce::Font (juce::FontOptions (r * 1.05f, juce::Font::bold)));
            g.drawText (juce::String (b + 1),
                        juce::Rectangle<float> (pos.x - r, pos.y - r, r * 2, r * 2),
                        juce::Justification::centred, false);

            if (p.dynamic)
            {
                g.setColour (col.withAlpha (0.6f));
                g.drawEllipse (pos.x - r - 3.0f, pos.y - r - 3.0f,
                               (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f, 1.0f);
            }

            if (const auto glyph = typeGlyph (p.type); glyph.isNotEmpty())
            {
                g.setColour (col.brighter (0.3f));
                g.setFont (icons->getFont (11.0f));
                g.drawText (glyph, (int) (pos.x - 8), (int) (pos.y - r - 15.0f),
                            16, 12, juce::Justification::centred, false);
            }
            else if (p.type == kbs::EqType::tilt)
            {
                g.setColour (col.brighter (0.3f));
                g.drawLine (pos.x - 5.0f, pos.y - r - 7.0f,
                            pos.x + 5.0f, pos.y - r - 13.0f, 1.6f);
            }

            // L/R badge - the one routing that exists inside the Stereo view.
            if (p.channel == kbs::EqChannel::left || p.channel == kbs::EqChannel::right)
            {
                g.setColour (cols.badge);
                g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
                g.drawText (p.channel == kbs::EqChannel::left ? "L" : "R",
                            (int) (pos.x + r + 3.0f), (int) (pos.y - r - 12.0f), 12, 10,
                            juce::Justification::left);
            }
            if (p.isolated)
            {
                g.setColour (cols.analyserPost);
                g.setFont (icons->getFont (10.0f));
                g.drawText (fontaudio::Solo,
                            (int) (pos.x - 6), (int) (pos.y + r + 3), 12, 11,
                            juce::Justification::centred, false);
            }

            if (sel)
            {
                const auto lb = listenButtonArea();
                g.setColour (listening ? cols.analyserPost.withAlpha (0.9f)
                                       : cols.ground.withAlpha (0.8f));
                g.fillEllipse (lb);
                g.setColour (listening ? cols.analyserPost : col.withAlpha (0.7f));
                g.drawEllipse (lb.reduced (0.5f), 1.2f);
                g.setColour (listening ? cols.ground : col);
                g.setFont (icons->getFont (11.0f));
                g.drawText (fontaudio::Headphones, lb.toNearestInt(),
                            juce::Justification::centred, false);
            }
        }
    }

    void drawPiano (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        r.removeFromBottom (16.0f);
        auto strip = r.removeFromBottom (26.0f);

        for (int midi = 16; midi <= 135; ++midi)
        {
            const double hz = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
            if (hz < 20.0 || hz > 20000.0) continue;
            const double hzNext = hz * std::pow (2.0, 1.0 / 12.0);
            const float x0 = freqToX (hz), x1 = freqToX (hzNext);
            const int pc = midi % 12;
            const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            g.setColour (black ? cols.groundDeep.darker (0.5f) : cols.panel);
            g.fillRect (x0, strip.getY() + 1.0f, x1 - x0 - 0.5f, strip.getHeight() - 2.0f);
        }
        // Every octave's C is named: a map, not an instrument.
        g.setColour (cols.text.withAlpha (0.8f));
        g.setFont (juce::Font (juce::FontOptions (8.0f, juce::Font::bold)));
        for (int oct = 1; oct <= 9; ++oct)
        {
            const int midi = 12 * (oct + 1);
            const double hz = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
            if (hz < 22.0 || hz > 19000.0) continue;
            g.drawText ("C" + juce::String (oct),
                        (int) freqToX (hz), (int) strip.getY() + 2, 22, 10,
                        juce::Justification::left);
        }

        g.setColour (cols.panelEdge);
        g.drawRect (strip, 1.0f);
    }

    juce::Rectangle<float> grabButtonArea() const
    {
        const auto a = plotArea();
        return { a.getRight() - 30.0f, a.getY() + 6.0f, 24.0f, 24.0f };
    }

    juce::Rectangle<float> listenButtonArea() const
    {
        if (selected < 0) return {};
        const auto pos = handlePos (selected);
        return { pos.x - 30.0f, pos.y - 11.0f, 18.0f, 18.0f };
    }

    void drawGrabModeButton (juce::Graphics& g)
    {
        const auto r = grabButtonArea();
        g.setColour (grabArmed ? cols.analyserPost.withAlpha (0.85f)
                               : cols.trackDark.withAlpha (0.7f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (grabArmed ? cols.analyserPost : cols.panelEdge.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

        // A crosshair-on-peak mark, drawn: fontaudio has no magnet.
        const auto c = r.getCentre();
        g.setColour (grabArmed ? cols.ground : cols.textDim);
        g.drawEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f, 1.4f);
        g.drawLine (c.x, c.y - 8.0f, c.x, c.y - 3.0f, 1.4f);
        g.drawLine (c.x, c.y + 3.0f, c.x, c.y + 8.0f, 1.4f);
        g.drawLine (c.x - 8.0f, c.y, c.x - 3.0f, c.y, 1.4f);
        g.drawLine (c.x + 3.0f, c.y, c.x + 8.0f, c.y, 1.4f);
    }

    void drawGrabMarker (juce::Graphics& g)
    {
        const auto a = plotArea();
        const float x = freqToX (grabPeak.hz);
        const float y = juce::jmap (juce::jlimit (-80.0f, 0.0f, grabPeak.db),
                                    -80.0f, 0.0f, a.getBottom(), a.getY());
        g.setColour (cols.analyserPost.withAlpha (0.9f));
        g.drawEllipse (x - 6.0f, y - 6.0f, 12.0f, 12.0f, 1.5f);
        g.drawVerticalLine ((int) x, y + 8.0f, a.getBottom());
        drawReadoutBox (g, formatHz ((float) grabPeak.hz) + "  grab", { x, y });
    }

    void drawDragReadout (juce::Graphics& g)
    {
        const auto p = bandParams (selected);
        const auto pos = handlePos (selected);
        juce::String text = formatHz (p.freqHz) + "   " + noteName (p.freqHz);
        if (kbs::eqTypeHasGain (p.type))
            text += "   " + juce::String (p.gainDb, 1) + " dB";
        text += "   Q " + juce::String (p.q, 2);

        drawReadoutBox (g, text, pos);
    }

    void drawHoverPanel (juce::Graphics& g, int b)
    {
        const auto p = bandParams (b);
        juce::String text = "Band " + juce::String (b + 1) + "   "
                          + formatHz (p.freqHz);
        if (kbs::eqTypeHasGain (p.type))
            text += "   " + juce::String (p.gainDb, 1) + " dB";
        if (p.dynamic)
            if (auto* e = eq())
                text += "   GR " + juce::String (e->engine().bandGrDb (b), 1) + " dB";
        drawReadoutBox (g, text, handlePos (b));
    }

    void drawReadoutBox (juce::Graphics& g, const juce::String& text,
                         juce::Point<float> at)
    {
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        const int w = (int) std::ceil (juce::GlyphArrangement::getStringWidth (
                          g.getCurrentFont(), text)) + 14;
        auto box = juce::Rectangle<float> (at.x - w * 0.5f, at.y - 34.0f,
                                           (float) w, 18.0f);
        box = box.constrainedWithin (getLocalBounds().toFloat().reduced (2.0f));
        g.setColour (cols.panel.withAlpha (0.92f));
        g.fillRoundedRectangle (box, 3.0f);
        g.setColour (cols.panelEdge);
        g.drawRoundedRectangle (box, 3.0f, 1.0f);
        g.setColour (cols.text);
        g.drawText (text, box, juce::Justification::centred, false);
    }

    static juce::String formatHz (float hz)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz"
                             : juce::String (hz, 1) + " Hz";
    }

    static juce::String noteName (float hz)
    {
        const double midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
        const int n = (int) std::lround (midi);
        const int cents = (int) std::lround ((midi - n) * 100.0);
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F",
                                       "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1)
             + (cents == 0 ? juce::String()
                           : (cents > 0 ? " +" : " ") + juce::String (cents) + "c");
    }

    int bandAt (juce::Point<float> pos) const
    {
        // Only the current view's bands answer: ghosts are landmarks.
        int best = -1;
        float bestD = 14.0f;
        for (int b = 0; b < kBands; ++b)
        {
            const auto p = bandParams (b);
            if (! p.on || viewOfChannel (p.channel) != domainView()) continue;
            const float d = pos.getDistanceFrom (handlePos (b));
            if (d < bestD) { bestD = d; best = b; }
        }
        return best;
    }

    void timerCallback() override
    {
        auto* e = eq();
        if (e == nullptr) { repaint(); return; }

        const double sr = proc.getSampleRate();
        if (sr > 0) { pre.setSampleRate (sr); post.setSampleRate (sr); sc.setSampleRate (sr); }

        bool any = false;
        if (showAnalyser() || showSpectrogram())
        {
            if (analyserPre()) any |= pre.analyse (e->preFeed);
            any |= post.analyse (e->postFeed);
            if (analyserSc() && e->scFeedAlive.load (std::memory_order_relaxed))
                any |= sc.analyse (e->scFeed);
        }

        if (showSpectrogram() && any && spectrogram.isValid())
            post.pushSpectrogramColumn (spectrogram, -80.0f,
                [] (float y01) { return 20.0 * std::pow (1000.0, 1.0 - y01); });

        repaint();
    }

    BaySickDAWProcessor& proc;
    juce::String stripPrefix;
    int bank = 0;
    Colors cols;

    EqAnalyser pre, post, sc;
    juce::Image spectrogram;

    int selected = -1, hovered = -1;
    EqAnalyser::Peak grabPeak;
    bool grabValid = false, grabArmed = false, listening = false;
    bool dragging = false, listenLatched = false;
    juce::Point<float> dragStart, lastMouse;
    float dragStartFreq = 1000.0f, dragStartGain = 0.0f;

    juce::SharedResourcePointer<fontaudio::IconHelper> icons;
};

} // namespace eqview
