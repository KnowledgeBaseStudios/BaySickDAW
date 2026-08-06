#pragma once
#include <JuceHeader.h>
#include "../DSP/EffectVisualFeed.h"
#include "SharedUI.h"   // namespace VC (the app palette)

// ─────────────────────────────────────────────────────────────────────────────
// Effect-panel visuals - QA-Layout T17 (2026-08-05)
// ─────────────────────────────────────────────────────────────────────────────
// The shared half of every effect visual: one clock, one container, one place
// the watcher gate is handled.  Panels supply only what to DRAW.
//
// This exists because the limiter's scrolling display (T13) is the first of TEN
// (T18).  Built bespoke it would be built ten times, drift ten ways, and get
// the audio-side gating subtly wrong in at least one of them -- the gating is
// the part that is easy to omit and impossible to notice, because omitting it
// costs CPU rather than correctness.  Here it is impossible to omit: holding a
// strip IS the watcher registration.
//
// Design note for whoever adds the eleventh: the CONTAINER owns lifecycle,
// timing and gating; CONTENT is a paint callback.  Resist moving per-effect
// drawing in here.  A scrolling history is offered as a built-in because four
// of the ten want exactly that; the rest (LFO scopes, transfer curves, harmonic
// bars, delay grids, reverb envelopes) are parametric draws with no history and
// should just paint.
// ─────────────────────────────────────────────────────────────────────────────

// One timer for every visible visual in the app.
//
// N panels each running their own juce::Timer is N wakeups; a DAW with several
// effect windows open pays that continuously.  One clock ticking a list is one
// wakeup regardless of how many are on screen, and it stops entirely when the
// list empties -- which is the common case, since a visual is only registered
// while it is actually visible.
class EffectVisualClock final : private juce::Timer
{
public:
    static EffectVisualClock& get()
    {
        static EffectVisualClock instance;
        return instance;
    }

    void add (juce::Component* c)
    {
        if (c == nullptr || mClients.contains (c)) return;
        mClients.add (c);
        if (! isTimerRunning())
            startTimerHz (kHz);
    }

    void remove (juce::Component* c)
    {
        mClients.removeAllInstancesOf (c);
        // Nothing visible -> no wakeups at all.  The whole point of the shared
        // clock is that an app with no visual on screen ticks zero times.
        if (mClients.isEmpty())
            stopTimer();
    }

private:
    EffectVisualClock() = default;

    void timerCallback() override
    {
        // Reverse iteration: a repaint can (via a resized/hierarchy change)
        // remove a client mid-loop.
        for (int i = mClients.size(); --i >= 0;)
            if (auto* c = mClients[i])
                c->repaint();
    }

    // 30 Hz, deliberately not 60.  These are diagnostic displays, not
    // animation; doubling the rate doubles a continuous cost to buy motion
    // smoothness nobody is judging.  Revisit only with a measurement.
    static constexpr int kHz = 30;

    juce::Array<juce::Component*> mClients;
};

// The container.  Owning one of these IS the statement "a visual is on screen",
// so the audio side publishes exactly while this exists and is showing.
class EffectVisualStrip final : public juce::Component
{
public:
    // paintContent draws inside the framed content area.  Null is legal and
    // yields an empty frame -- useful while an effect's visual is still being
    // written, and much better than a panel that silently has no strip.
    using PaintFn = std::function<void (juce::Graphics&, juce::Rectangle<float>, EffectVisualFeed*)>;

    EffectVisualStrip() { setInterceptsMouseClicks (false, false); }

    ~EffectVisualStrip() override
    {
        // Both halves must be released here, not left to the parent going away:
        // a dangling clock client repaints freed memory, and a leaked watcher
        // pins the audio-side feed on forever, which is the exact cost this
        // whole design exists to avoid.
        detach();
    }

    // The DSP whose feed this strip displays.  Safe to pass null (effect
    // swapped out, slot cleared) -- the strip simply stops watching.
    void setFeed (EffectVisualFeed* feed)
    {
        if (feed == mFeed) return;
        releaseWatcher();
        mFeed = feed;
        acquireWatcherIfVisible();
    }

    void setPaintFn (PaintFn fn) { mPaint = std::move (fn); }

    // A caption drawn top-left inside the frame.  Present because these
    // displays are teaching tools for people who have never made music -- an
    // unlabelled graph explains nothing.
    void setCaption (juce::String c) { mCaption = std::move (c); repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        g.setColour (VC::Bg.darker (0.35f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (VC::Panel.brighter (0.10f));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);

        auto content = r.reduced (4.0f);

        if (mCaption.isNotEmpty())
        {
            auto capArea = content.removeFromTop (12.0f);
            g.setColour (VC::TextDim);
            g.setFont (10.0f);
            g.drawText (mCaption, capArea, juce::Justification::centredLeft);
        }

        if (mPaint && ! content.isEmpty())
            mPaint (g, content, mFeed);
    }

    // Visibility is the gate, and it has to be observed from BOTH of these:
    // a strip inside a view that is swapped away gets hidden (visibility
    // changed) while a strip in a window that closes loses its peer (hierarchy
    // changed).  Watching only one leaves the other case publishing forever.
    void visibilityChanged()        override { syncWatcher(); }
    void parentHierarchyChanged()   override { syncWatcher(); }

private:
    bool shouldWatch() const { return isShowing(); }

    void syncWatcher()
    {
        if (shouldWatch()) acquireWatcherIfVisible();
        else               detachClockAndWatcher();
    }

    void acquireWatcherIfVisible()
    {
        if (! shouldWatch() || mFeed == nullptr || mWatching) return;
        mFeed->addWatcher();
        mWatching = true;
        EffectVisualClock::get().add (this);
    }

    void releaseWatcher()
    {
        if (mWatching && mFeed != nullptr)
            mFeed->removeWatcher();
        mWatching = false;
    }

    void detachClockAndWatcher()
    {
        EffectVisualClock::get().remove (this);
        releaseWatcher();
    }

    void detach() { detachClockAndWatcher(); }

    EffectVisualFeed* mFeed { nullptr };
    bool              mWatching { false };
    PaintFn           mPaint;
    juce::String      mCaption;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectVisualStrip)
};

// Built-in renderer for the four visuals that ARE a scrolling history (limiter,
// compressor, gate, transient shaper).  Supplied as a free function rather than
// a mode flag on the strip so the parametric visuals never carry its state.
//
// Draws right-to-left: newest column at the right edge, which is the direction
// every meter of this kind moves and therefore the one users already read.
namespace EffectVisualDraw
{
    inline void scrollingHistory (juce::Graphics& g,
                                  juce::Rectangle<float> area,
                                  EffectVisualFeed* feed,
                                  juce::Colour waveColour,
                                  juce::Colour overlayColour,
                                  bool drawOverlayB = false,
                                  juce::Colour overlayBColour = juce::Colours::red)
    {
        if (feed == nullptr || area.getWidth() < 2.0f) return;

        const int      cols  = juce::jmin ((int) area.getWidth(), EffectVisualFeed::kSize);
        const uint32_t write = feed->writeIndex();
        const float    midY  = area.getCentreY();
        const float    halfH = area.getHeight() * 0.5f;

        juce::Path wave, overlay, overlayB;
        bool started = false, startedO = false, startedB = false;

        for (int i = 0; i < cols; ++i)
        {
            // Newest at the right edge.
            const uint32_t idx = write - (uint32_t) (cols - i);
            const auto&    c   = feed->at (idx);
            const float    x   = area.getX() + (float) i;

            g.setColour (waveColour.withAlpha (0.85f));
            const float yTop = midY - juce::jlimit (0.0f, 1.0f, c.hi) * halfH;
            const float yBot = midY + juce::jlimit (0.0f, 1.0f, -c.lo) * halfH;
            g.drawLine (x, yTop, x, yBot, 1.0f);
            started = true;

            const float ay = area.getY() + juce::jlimit (0.0f, 1.0f, c.a) * area.getHeight();
            if (! startedO) { overlay.startNewSubPath (x, ay); startedO = true; }
            else              overlay.lineTo (x, ay);

            if (drawOverlayB)
            {
                const float by = area.getY() + juce::jlimit (0.0f, 1.0f, c.b) * area.getHeight();
                if (! startedB) { overlayB.startNewSubPath (x, by); startedB = true; }
                else              overlayB.lineTo (x, by);
            }
        }

        juce::ignoreUnused (wave, started);

        if (startedO)
        {
            g.setColour (overlayColour);
            g.strokePath (overlay, juce::PathStrokeType (1.4f));
        }

        if (startedB)
        {
            g.setColour (overlayBColour);
            g.strokePath (overlayB, juce::PathStrokeType (1.0f));
        }
    }
}
