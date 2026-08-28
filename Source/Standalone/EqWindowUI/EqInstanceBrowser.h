// BaySickDAW - the EQ instance browser (QA-EqFlagship W-21).
//
// Every EQ point in the project - buses and strips, pre and post - in one
// list: name, a curve thumbnail read straight off the engine's own magnitude
// query, and a live mini-spectrum from the point's post feed.  Click a row
// and THIS window re-points at it (W-8's re-point plumbing); double-click or
// the row menu opens the point in its own window instead.  The row menu also
// hands a point to the Match panel as a reference source and to the graph's
// collision view (W-24 / the existing collision machinery).
//
// SATELLITE DISCIPLINE: rows hold (channelId, pre), never a pointer - every
// resolve goes through the owner's hook per use, because strips die and
// graphs rebuild under open windows.
#pragma once

#include "EqGraphView.h"
#include "../../DSP/Kbs/FFT.h"

namespace eqview {

class EqInstanceBrowser : public juce::Component, private juce::Timer
{
public:
    static constexpr int kPanelW = 320;
    static constexpr int kPanelH = 360;
    static constexpr int kRowH   = 34;

    std::function<std::vector<std::pair<int, juce::String>>()> getChannels;
    std::function<StripEq* (int channelId, bool pre)> resolvePoint;
    std::function<void (int channelId, bool pre)> onRepoint;
    std::function<void (int channelId, bool pre)> onOpenWindow;
    std::function<void (int channelId, bool pre, const juce::String&)> onMatchReference;
    std::function<void (int channelId, bool pre, const juce::String&)> onCollisionReference;
    std::function<bool (int channelId, bool pre)> isCurrent, isMatchRef, isCollisionRef;
    std::function<double()> sampleRate;

    EqInstanceBrowser() : fft (kFftOrder)
    {
        rowsView = std::make_unique<RowList> (*this);
        view.setViewedComponent (rowsView.get(), false);
        view.setScrollBarsShown (true, false);
        addAndMakeVisible (view);

        closeBtn.setButtonText ("Close");
        closeBtn.onClick = [this] { setVisible (false); };
        addAndMakeVisible (closeBtn);

        pollBuf.resize ((size_t) kbs::SpectrumFeed::kSize);
        td.resize ((size_t) kFftSize);

        startTimerHz (10);
        setSize (kPanelW, kPanelH);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (VC::Panel.withAlpha (0.97f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
        g.setColour (VC::Accent);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (VC::Text);
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText ("EQ INSTANCES", 10, 6, getWidth() - 20, 14,
                    juce::Justification::centredLeft);
        g.setColour (VC::TextDim);
        g.setFont (juce::Font (juce::FontOptions (9.5f)));
        g.drawText ("Click re-points this window. Right-click for more.",
                    10, 20, getWidth() - 20, 12, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 6);
        r.removeFromTop (30);
        closeBtn.setBounds (r.removeFromBottom (20));
        r.removeFromBottom (4);
        view.setBounds (r);
        layoutRows();
    }

private:
    struct Row
    {
        int channelId = -1;
        bool pre = false;
        juce::String name;
        std::array<float, 16> bars {};
    };

    static constexpr int kFftOrder = 10;
    static constexpr int kFftSize = 1 << kFftOrder;

    // The list body: all rows, painted flat - a component per row would be
    // 80 children re-created per refresh for no benefit.
    class RowList : public juce::Component
    {
    public:
        explicit RowList (EqInstanceBrowser& o) : owner (o) {}

        void paint (juce::Graphics& g) override
        {
            for (int i = 0; i < (int) owner.rows.size(); ++i)
                owner.paintRow (g, owner.rows[(size_t) i],
                                juce::Rectangle<int> (0, i * kRowH,
                                                      getWidth(), kRowH));
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int i = (int) (e.position.y / (float) kRowH);
            if (i < 0 || i >= (int) owner.rows.size()) return;
            const auto row = owner.rows[(size_t) i];
            if (e.mods.isPopupMenu()) { owner.rowMenu (row); return; }
            if (owner.onRepoint) owner.onRepoint (row.channelId, row.pre);
            repaint();
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int i = (int) (e.position.y / (float) kRowH);
            if (i < 0 || i >= (int) owner.rows.size()) return;
            const auto row = owner.rows[(size_t) i];
            if (owner.onOpenWindow) owner.onOpenWindow (row.channelId, row.pre);
        }

    private:
        EqInstanceBrowser& owner;
    };

    juce::String pointName (const Row& r) const
    {
        return r.name + (r.pre ? " - Pre" : " - Post");
    }

    void rowMenu (const Row& row)
    {
        juce::PopupMenu m;
        m.addItem (1, "Open in Its Own Window");
        m.addSeparator();
        m.addItem (2, "Match Reference", true,
                   isMatchRef && isMatchRef (row.channelId, row.pre));
        m.addItem (3, "Collision Reference", true,
                   isCollisionRef && isCollisionRef (row.channelId, row.pre));
        const auto at = juce::Rectangle<int> (1, 1)
                            .withPosition (juce::Desktop::getMousePosition());
        m.showMenuAsync (juce::PopupMenu::Options()
                             .withTargetComponent (this)
                             .withTargetScreenArea (at),
            [this, row] (int r)
            {
                if (r == 1 && onOpenWindow) onOpenWindow (row.channelId, row.pre);
                else if (r == 2 && onMatchReference)
                    onMatchReference (row.channelId, row.pre, pointName (row));
                else if (r == 3 && onCollisionReference)
                    onCollisionReference (row.channelId, row.pre, pointName (row));
            });
    }

    void paintRow (juce::Graphics& g, const Row& row, juce::Rectangle<int> r)
    {
        const bool cur = isCurrent && isCurrent (row.channelId, row.pre);
        if (cur)
        {
            g.setColour (VC::Yellow.withAlpha (0.12f));
            g.fillRect (r);
        }
        g.setColour (VC::Surface.darker (0.35f));
        g.drawHorizontalLine (r.getBottom() - 1, (float) r.getX(), (float) r.getRight());

        auto area = r.reduced (6, 3);

        // The curve thumbnail: the engine's own magnitude query, +-18 dB.
        auto thumb = area.removeFromLeft (44).toFloat().reduced (0.0f, 3.0f);
        g.setColour (VC::Surface.darker (0.25f));
        g.fillRoundedRectangle (thumb, 2.0f);
        if (auto* e = resolvePoint ? resolvePoint (row.channelId, row.pre) : nullptr)
        {
            juce::Path p;
            for (int i = 0; i < 30; ++i)
            {
                const double hz = 20.0 * std::pow (1000.0, i / 29.0);
                const float db = 20.0f * std::log10 (std::max (1.0e-4f,
                                     e->engine().magnitudeAt ((float) hz)));
                const float y = juce::jmap (juce::jlimit (-18.0f, 18.0f, db),
                                            -18.0f, 18.0f,
                                            thumb.getBottom() - 1.0f, thumb.getY() + 1.0f);
                const float x = thumb.getX() + 1.0f
                              + (thumb.getWidth() - 2.0f) * (float) i / 29.0f;
                if (i == 0) p.startNewSubPath (x, y);
                else        p.lineTo (x, y);
            }
            g.setColour (cur ? VC::Yellow : VC::Accent);
            g.strokePath (p, juce::PathStrokeType (1.2f));
        }

        // The live mini-spectrum on the right.
        auto spec = area.removeFromRight (64).toFloat().reduced (0.0f, 3.0f);
        g.setColour (VC::Surface.darker (0.25f));
        g.fillRoundedRectangle (spec, 2.0f);
        const float bw = (spec.getWidth() - 2.0f) / (float) row.bars.size();
        g.setColour (VC::Green.withAlpha (0.75f));
        for (int i = 0; i < (int) row.bars.size(); ++i)
        {
            const float h = (spec.getHeight() - 2.0f)
                          * juce::jlimit (0.0f, 1.0f, row.bars[(size_t) i]);
            if (h > 0.5f)
                g.fillRect (spec.getX() + 1.0f + bw * (float) i,
                            spec.getBottom() - 1.0f - h,
                            std::max (1.0f, bw - 1.0f), h);
        }

        area.removeFromLeft (6);
        area.removeFromRight (6);
        g.setColour (cur ? VC::Yellow : VC::Text);
        g.setFont (juce::Font (juce::FontOptions (11.0f,
                       cur ? juce::Font::bold : juce::Font::plain)));
        g.drawText (row.name, area.removeFromTop (area.getHeight() / 2 + 2),
                    juce::Justification::bottomLeft, true);
        g.setColour (VC::TextDim);
        g.setFont (juce::Font (juce::FontOptions (8.5f, juce::Font::bold)));
        juce::String tag = row.pre ? "PRE" : "POST";
        if (isMatchRef && isMatchRef (row.channelId, row.pre)) tag << "  MATCH REF";
        if (isCollisionRef && isCollisionRef (row.channelId, row.pre)) tag << "  COLLISION";
        g.drawText (tag, area, juce::Justification::topLeft, true);
    }

    void layoutRows()
    {
        if (rowsView == nullptr) return;
        rowsView->setSize (std::max (10, view.getWidth()
                                             - (view.isVerticalScrollBarShown() ? 8 : 0)),
                           std::max (1, (int) rows.size() * kRowH));
    }

    void timerCallback() override
    {
        if (! isVisible()) return;

        // Re-enumerate each tick: strips are born and die under this list.
        std::vector<Row> fresh;
        if (getChannels && resolvePoint)
            for (const auto& ch : getChannels())
                for (int pre = 0; pre < 2; ++pre)
                    if (resolvePoint (ch.first, pre == 1) != nullptr)
                    {
                        Row r2;
                        r2.channelId = ch.first;
                        r2.pre = pre == 1;
                        r2.name = ch.second;
                        fresh.push_back (r2);
                    }
        bool changed = fresh.size() != rows.size();
        for (size_t i = 0; ! changed && i < rows.size(); ++i)
            changed = fresh[i].channelId != rows[i].channelId
                   || fresh[i].pre != rows[i].pre
                   || fresh[i].name != rows[i].name;
        if (changed)
        {
            for (auto& f : fresh)
                for (const auto& old : rows)
                    if (old.channelId == f.channelId && old.pre == f.pre)
                    { f.bars = old.bars; break; }
            rows = std::move (fresh);
            layoutRows();
        }

        // Mini-spectra for the rows actually on screen.
        const auto visible = view.getViewArea();
        const double sr = sampleRate ? sampleRate() : 48000.0;
        for (int i = 0; i < (int) rows.size(); ++i)
        {
            if ((i + 1) * kRowH < visible.getY() || i * kRowH > visible.getBottom())
                continue;
            updateBars (rows[(size_t) i], sr);
        }
        rowsView->repaint();
    }

    void updateBars (Row& row, double sr)
    {
        auto* e = resolvePoint ? resolvePoint (row.channelId, row.pre) : nullptr;
        if (e == nullptr) { row.bars.fill (0.0f); return; }
        if (! e->postFeed.poll (pollBuf.data())) return;   // torn frame: keep last

        const int off = (int) pollBuf.size() - kFftSize;
        for (int i = 0; i < kFftSize; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (kbs::kTwoPi * (float) i
                                                    / (float) kFftSize);
            td[(size_t) i] = { pollBuf[(size_t) (off + i)] * w, 0.0f };
        }
        fft.transform (td.data(), false);
        const float norm = 4.0f / (float) kFftSize;
        for (int b = 0; b < (int) row.bars.size(); ++b)
        {
            // 40 Hz .. 16 kHz in log-spaced bar bands, peak per band.
            const double lo = 40.0 * std::pow (400.0, (double) b / 16.0);
            const double hi = 40.0 * std::pow (400.0, (double) (b + 1) / 16.0);
            const int kLo = std::max (1, (int) (lo * kFftSize / sr));
            const int kHi = std::min (kFftSize / 2 - 1,
                                      std::max (kLo, (int) (hi * kFftSize / sr)));
            float pk = 0.0f;
            for (int k = kLo; k <= kHi; ++k)
                pk = std::max (pk, std::abs (td[(size_t) k]) * norm);
            const float db = 20.0f * std::log10 (std::max (1.0e-5f, pk));
            const float t = juce::jmap (juce::jlimit (-60.0f, 0.0f, db),
                                        -60.0f, 0.0f, 0.0f, 1.0f);
            auto& bar = row.bars[(size_t) b];
            bar = std::max (t, bar * 0.7f);      // quick up, eased fall
        }
    }

    std::vector<Row> rows;
    juce::Viewport view;
    std::unique_ptr<RowList> rowsView;
    juce::TextButton closeBtn;
    kbs::FFT fft;
    std::vector<float> pollBuf;
    std::vector<std::complex<float>> td;
};

} // namespace eqview
