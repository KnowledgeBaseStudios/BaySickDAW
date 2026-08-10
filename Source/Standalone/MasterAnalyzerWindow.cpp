#include "MasterAnalyzerWindow.h"
#include "../PluginProcessor.h"
#include <cmath>
#include <cstring>

namespace
{
    // Limiter.txt §2 palette -- the analyzer is part of the same suite, so a
    // second accent colour would just read as a different tool.
    const juce::Colour kCyan   { 0xff00fff2 };
    const juce::Colour kOrange { 0xffff9100 };
    const juce::Colour kBg     { 0xff101214 };
    const juce::Colour kGrid   { 0xff23282c };
    const juce::Colour kGridLo { 0xff1d2124 };
    const juce::Colour kText   { 0xff8f9aa1 };

    juce::Font monoFont (float h, bool bold = false)
    {
        return juce::Font (juce::Font::getDefaultMonospacedFontName(), h,
                           bold ? juce::Font::bold : juce::Font::plain);
    }

    juce::String fmtDb (float v, float floorAt = -119.0f)
    {
        return v <= floorAt ? juce::String ("--") : juce::String (v, 1);
    }
}

MasterAnalyzerView::MasterAnalyzerView (VibeSynthProcessor& proc)
    : mProc (proc),
      mFft (kFftOrder),
      mWindow ((size_t) kFftSize, juce::dsp::WindowingFunction<float>::hann)
{
    mScratch.assign ((size_t) kFftSize * 2, 0.0f);
    mFeedBuf.assign ((size_t) SpectrumFeed::kSize, 0.0f);
    mAccum  .assign ((size_t) kFftSize, 0.0f);
    mAvgDb .fill (kMinDb);
    mPeakDb.fill (kMinDb);

    setTooltip ("Master analyzer - loudness over time and output spectrum, "
                "tapped after the master fader");

    // ── TS7 §3.6: source picker (Live + every captured take) ─────────────────
    mSourceBox = std::make_unique<juce::ComboBox>();
    mSourceBox->setTextWhenNothingSelected ("Live");
    // §3.7 stated where the user chooses a take, not buried in a manual: a
    // capture is the MASTER OUTPUT as it was at that moment, so an old take
    // still carries the master chain it was recorded through.
    mSourceBox->setTooltip ("Live output, or a captured take.\n"
                            "Takes are recorded from the master output as it "
                            "was at the time, so they compare as finished "
                            "output - not as source performances.");
    mSourceBox->onChange = [this]
    {
        if (mRebuildingList) return;
        mSelectedTakeId = mSourceBox->getSelectedId() - 1;   // id 1 == Live (0)
        applySelection();
    };
    addAndMakeVisible (*mSourceBox);

    mExportBtn = std::make_unique<juce::TextButton> ("Export Take...");
    mExportBtn->setEnabled (false);
    mExportBtn->onClick = [this]
    {
        if (mVersions == nullptr || ! onExportTake) return;
        if (const auto* v = mVersions->find (mSelectedTakeId)) onExportTake (*v);
    };
    addAndMakeVisible (*mExportBtn);

    refreshVersions();
}

void MasterAnalyzerView::resized()
{
    auto r = getLocalBounds().removeFromTop ((int) kControlsH).reduced (4, 3);
    if (mExportBtn) mExportBtn->setBounds (r.removeFromRight (96));
    r.removeFromRight (4);
    if (mSourceBox) mSourceBox->setBounds (r);
}

void MasterAnalyzerView::setVersionSource (VersionCapture* vc)
{
    mVersions = vc;
    refreshVersions();
}

void MasterAnalyzerView::refreshVersions()
{
    if (! mSourceBox) return;

    // Rebuild wholesale rather than appending the new take: reconciling two
    // orderings by hand is how a picker ends up pointing at the wrong take.
    mRebuildingList = true;
    mSourceBox->clear (juce::dontSendNotification);
    mSourceBox->addItem ("Live", 1);   // ids are takeId + 1; 0 is not a legal id

    bool selectionStillExists = (mSelectedTakeId == 0);
    if (mVersions != nullptr)
    {
        for (const auto& v : mVersions->versions())
        {
            juce::String text = v.label;
            if (v.audioFile == juce::File()) text += "   (no audio)";
            mSourceBox->addItem (text, v.id + 1);
            if (v.id == mSelectedTakeId) selectionStillExists = true;
        }
    }

    if (! selectionStillExists) mSelectedTakeId = 0;
    mSourceBox->setSelectedId (mSelectedTakeId + 1, juce::dontSendNotification);
    mRebuildingList = false;

    applySelection();
}

void MasterAnalyzerView::selectSource (int takeId)
{
    mSelectedTakeId = takeId;
    if (mSourceBox)
    {
        mRebuildingList = true;   // do not re-enter through onChange
        mSourceBox->setSelectedId (takeId + 1, juce::dontSendNotification);
        mRebuildingList = false;
    }
    applySelection();
}

void MasterAnalyzerView::applySelection()
{
    const VersionCapture::Version* v =
        (mVersions != nullptr && mSelectedTakeId > 0)
            ? mVersions->find (mSelectedTakeId) : nullptr;

    if (v == nullptr)
    {
        if (mExportBtn) mExportBtn->setEnabled (false);
        showLive();
        return;
    }

    // Only a take captured WITH audio can be exported.  Disabled rather than
    // hidden, so the reason stays visible next to the "(no audio)" the list
    // already shows.
    if (mExportBtn)
        mExportBtn->setEnabled (v->audioFile != juce::File()
                                && v->audioFile.existsAsFile());

    showRenderedCurve (v->lufsCurve, v->integratedLufs, v->lraLu,
                       v->truePeakDb, v->label);
}

MasterAnalyzerView::~MasterAnalyzerView()
{
    stopTimer();
    // Released here as well as in parentHierarchyChanged: a window can be
    // destroyed without losing its peer first, and a stuck flag would keep the
    // audio thread copying for nobody.
    setTapActive (false);
}

void MasterAnalyzerView::setTapActive (bool on)
{
    if (on == mTapActive) return;
    mTapActive = on;
    mProc.setMasterSpectrumActive (on);
}

void MasterAnalyzerView::parentHierarchyChanged()
{
    const bool live = (getPeer() != nullptr);
    if (live)
    {
        setTapActive (true);
        if (! isTimerRunning()) startTimerHz (kTimerHz);
    }
    else
    {
        stopTimer();
        setTapActive (false);
    }
}

void MasterAnalyzerView::resetHistory()
{
    mLufsHistory.clear();
    mPeakDb = mAvgDb;
    repaint();
}

void MasterAnalyzerView::showRenderedCurve (const std::vector<float>& lufsCurve,
                                            float integrated, float lra,
                                            float truePeakDb,
                                            const juce::String& label)
{
    mRenderCurve      = lufsCurve;
    mRenderIntegrated = integrated;
    mRenderLra        = lra;
    mRenderTruePeak   = truePeakDb;
    mRenderLabel      = label;
    mShowingRender    = ! lufsCurve.empty();
    mView             = ViewMode::Loudness;   // a render has no live spectrum
    repaint();
}

void MasterAnalyzerView::showLive()
{
    mShowingRender = false;
    repaint();
}

void MasterAnalyzerView::timerCallback()
{
    int count = 0;
    const bool got = mProc.pollMasterSpectrum (mFeedBuf.data(), count);
    const float decay = kPeakDecayDbPerSec / (float) kTimerHz;

    if (got && count > 0)
    {
        // Slide the accumulator and append: the feed delivers 1024 at a time and
        // the transform needs kFftSize, so frames are stitched rather than
        // transformed individually.  This is the whole reason the bottom octave
        // exists at all -- a 1024-point FFT's first bin sits near 43 Hz.
        const int n = juce::jmin (count, kFftSize);
        if (n < kFftSize)
            std::memmove (mAccum.data(), mAccum.data() + n,
                          (size_t) (kFftSize - n) * sizeof (float));
        std::copy (mFeedBuf.begin(), mFeedBuf.begin() + n,
                   mAccum.end() - n);
        mAccumFill = juce::jmin (kFftSize, mAccumFill + n);

        if (mAccumFill >= kFftSize)
        {
            std::fill (mScratch.begin(), mScratch.end(), 0.0f);
            std::copy (mAccum.begin(), mAccum.end(), mScratch.begin());
            mWindow.multiplyWithWindowingTable (mScratch.data(), (size_t) kFftSize);
            mFft.performFrequencyOnlyForwardTransform (mScratch.data());

            const float norm = 2.0f / (float) kFftSize;
            for (int i = 0; i < kNumBins; ++i)
            {
                const float db = juce::Decibels::gainToDecibels (
                                     mScratch[(size_t) i] * norm, kMinDb);
                mAvgDb[(size_t) i] = mHaveFrame
                    ? (mAvgDb[(size_t) i] + kAvgAlpha * (db - mAvgDb[(size_t) i]))
                    : db;
                mPeakDb[(size_t) i] = juce::jmax (db, mPeakDb[(size_t) i] - decay);
            }
            mHaveFrame = true;
        }
    }
    else
    {
        for (auto& p : mPeakDb) p = juce::jmax (kMinDb, p - decay);
    }

    // Loudness history at kHistoryHz, divided down from the paint timer.
    if (++mTickCounter >= (kTimerHz / kHistoryHz))
    {
        mTickCounter = 0;
        const float st = mProc.getMasterLufs (1);   // Short-Term
        mLufsHistory.push_back (st);
        while ((int) mLufsHistory.size() > kMaxHistory) mLufsHistory.pop_front();
    }
    repaint();
}

float MasterAnalyzerView::xForFrequency (float hz, float w) const
{
    const float lo = std::log10 (kMinHz);
    const float hi = std::log10 (kMaxHz);
    const float t  = (std::log10 (juce::jlimit (kMinHz, kMaxHz, hz)) - lo) / (hi - lo);
    return t * w;
}

void MasterAnalyzerView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (kBg);
    g.fillRect (b);

    b.removeFromTop (kControlsH);   // TS7 §3.6 source picker (real components)

    auto readouts = b.removeFromTop (34.0f);
    paintReadouts (g, readouts);

    auto plot = b.reduced (4.0f, 4.0f);
    if (plot.getWidth() <= 8.0f || plot.getHeight() <= 8.0f) return;

    if (mView == ViewMode::Loudness) paintLoudness (g, plot);
    else                             paintSpectrum (g, plot);
}

void MasterAnalyzerView::paintReadouts (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colour (0xff16191c));
    g.fillRect (r);

    const bool rend = mShowingRender;
    const float mom  = rend ? -120.0f            : mProc.getMasterLufs (0);
    const float st   = rend ? -120.0f            : mProc.getMasterLufs (1);
    const float integ= rend ? mRenderIntegrated  : mProc.getMasterLufs (2);
    const float tp   = rend ? mRenderTruePeak    : mProc.getMasterTruePeakDb();

    // §2.2's dBFS PEAK, off the existing master peak atomics.  A render has no
    // sample-peak of its own recorded, so it reads "--" there rather than showing
    // the live meter's value under a render's numbers.
    const float pk = rend
        ? -144.0f
        : juce::jmax (mProc.mMasterPeakDbL.load (std::memory_order_relaxed),
                      mProc.mMasterPeakDbR.load (std::memory_order_relaxed));

    struct Cell { const char* label; juce::String value; juce::Colour col; };
    std::vector<Cell> cells;
    cells.push_back ({ "MOM",  fmtDb (mom),   kText });
    cells.push_back ({ "SHORT",fmtDb (st),    kText });
    cells.push_back ({ "INT",  fmtDb (integ), kCyan });
    // Always present, so the row's cell widths do not reflow when the source
    // changes -- LRA reads "--" on Live rather than vanishing.
    cells.push_back ({ "LRA", rend ? juce::String (mRenderLra, 1)
                                   : juce::String ("--"), kText });
    cells.push_back ({ "PEAK", fmtDb (pk, -143.0f),
                       pk > -0.1f ? kOrange : kText });
    // True peak turns orange inside the last dB -- the margin every spec asks for.
    cells.push_back ({ "TRUE PK", fmtDb (tp, -143.0f),
                       tp > -1.0f ? kOrange : kText });

    const float cw = r.getWidth() / (float) juce::jmax (1, (int) cells.size());
    auto cur = r;
    for (auto& c : cells)
    {
        auto cell = cur.removeFromLeft (cw);
        g.setColour (kText.withAlpha (0.65f));
        g.setFont (monoFont (8.0f));
        g.drawText (c.label, cell.removeFromTop (12.0f).reduced (4.0f, 1.0f),
                    juce::Justification::centredLeft, false);
        g.setColour (c.col);
        g.setFont (monoFont (14.0f, true));
        g.drawText (c.value, cell.reduced (4.0f, 0.0f),
                    juce::Justification::centredLeft, false);
    }

    if (rend)
    {
        g.setColour (kOrange);
        g.setFont (monoFont (8.0f));
        g.drawText ("RENDER: " + mRenderLabel, r.reduced (4.0f, 2.0f),
                    juce::Justification::bottomRight, false);
    }
}

void MasterAnalyzerView::paintLoudness (juce::Graphics& g, juce::Rectangle<float> plot)
{
    const float w = plot.getWidth(), h = plot.getHeight();

    auto yFor = [&] (float lufs)
    {
        const float t = juce::jlimit (0.0f, 1.0f,
                                      (lufs - kLufsMin) / (kLufsMax - kLufsMin));
        return plot.getBottom() - t * h;
    };

    // Horizontal LU gridlines every 6 LU.
    g.setFont (monoFont (8.0f));
    for (float lu = 0.0f; lu >= kLufsMin; lu -= 6.0f)
    {
        const float y = yFor (lu);
        g.setColour (kGridLo);
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (kText.withAlpha (0.5f));
        g.drawText (juce::String ((int) lu), (int) plot.getX() + 2, (int) y - 9, 26, 9,
                    juce::Justification::centredLeft, false);
    }

    // The target bar.  This IS the limit -- moments above it are the flagged
    // instances (no published spec defines a moment-level loudness ceiling, so
    // the user's own target is the bar).
    const float ty = yFor (mTargetLufs);
    g.setColour (kCyan.withAlpha (0.9f));
    {
        const float dash[] = { 4.0f, 3.0f };
        juce::Path line, dashed;
        line.startNewSubPath (plot.getX(), ty);
        line.lineTo (plot.getRight(), ty);
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, line, dash, 2);
        g.fillPath (dashed);
    }
    g.setFont (monoFont (8.0f, true));
    g.drawText (juce::String (mTargetLufs, 1) + " LUFS",
                (int) plot.getRight() - 62, (int) ty - 10, 60, 9,
                juce::Justification::centredRight, false);

    const std::vector<float>* src = nullptr;
    std::vector<float> liveCopy;
    if (mShowingRender) src = &mRenderCurve;
    else { liveCopy.assign (mLufsHistory.begin(), mLufsHistory.end()); src = &liveCopy; }
    if (src->empty()) return;

    // The curve, plus over-target spans drawn in the warning colour so the
    // moments that broke the bar are visible without reading the log.
    const int n = (int) src->size();
    const float dx = (n > 1) ? (w / (float) (n - 1)) : w;

    juce::Path curve;
    bool started = false;
    for (int i = 0; i < n; ++i)
    {
        const float v = (*src)[(size_t) i];
        if (v <= kLufsMin) { started = false; continue; }   // gate: no line through silence
        const float x = plot.getX() + (float) i * dx;
        const float y = yFor (v);
        if (! started) { curve.startNewSubPath (x, y); started = true; }
        else             curve.lineTo (x, y);
    }
    g.setColour (kCyan);
    g.strokePath (curve, juce::PathStrokeType (1.4f));

    g.setColour (kOrange);
    for (int i = 0; i < n; ++i)
    {
        const float v = (*src)[(size_t) i];
        if (v <= mTargetLufs) continue;
        const float x = plot.getX() + (float) i * dx;
        g.fillRect (x, yFor (v), juce::jmax (1.0f, dx), ty - yFor (v));
    }
}

void MasterAnalyzerView::paintSpectrum (juce::Graphics& g, juce::Rectangle<float> plot)
{
    plot.removeFromBottom (12.0f);   // frequency label strip
    const float w = plot.getWidth(), h = plot.getHeight();
    if (w <= 4.0f || h <= 4.0f) return;

    static constexpr float kGridHz[] = { 50.f, 100.f, 200.f, 500.f, 1000.f,
                                         2000.f, 5000.f, 10000.f };
    g.setFont (monoFont (8.0f));
    for (float hz : kGridHz)
    {
        const float x = plot.getX() + xForFrequency (hz, w);
        g.setColour (kGrid);
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (kText.withAlpha (0.6f));
        const juce::String lbl = hz >= 1000.0f
            ? juce::String ((int) (hz / 1000.0f)) + "k"
            : juce::String ((int) hz);
        g.drawText (lbl, (int) x - 14, (int) plot.getBottom() + 1, 28, 11,
                    juce::Justification::centred, false);
    }
    for (float db = 0.0f; db >= kMinDb; db -= 12.0f)
    {
        const float t = (db - kMinDb) / (kMaxDb - kMinDb);
        const float y = plot.getBottom() - t * h;
        g.setColour (db == 0.0f ? kGrid : kGridLo);
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
    }

    if (! mHaveFrame) return;

    const double sr = juce::jmax (8000.0, mProc.getSampleRate() > 0.0
                                              ? mProc.getSampleRate() : 44100.0);
    const float binHz = (float) (sr / (double) kFftSize);

    auto buildTrace = [&] (const std::array<float, kNumBins>& s) -> juce::Path
    {
        juce::Path p;
        bool started = false;
        float lastX = -1.0f, pixMax = kMinDb;
        // Max per pixel rather than last-wins: several bins collapse onto one
        // pixel at the top of a log axis, and last-wins under-reads narrow peaks.
        for (int i = 1; i < kNumBins; ++i)
        {
            const float hz = (float) i * binHz;
            if (hz < kMinHz) continue;
            if (hz > kMaxHz) break;
            const float x = std::floor (xForFrequency (hz, w));
            if (x != lastX && lastX >= 0.0f)
            {
                const float t = juce::jlimit (0.0f, 1.0f,
                                              (pixMax - kMinDb) / (kMaxDb - kMinDb));
                const float y = plot.getBottom() - t * h;
                if (! started) { p.startNewSubPath (plot.getX() + lastX, y); started = true; }
                else             p.lineTo          (plot.getX() + lastX, y);
                pixMax = kMinDb;
            }
            lastX  = x;
            pixMax = juce::jmax (pixMax, s[(size_t) i]);
        }
        return p;
    };

    g.setColour (juce::Colour (0xff4a5a63));
    g.strokePath (buildTrace (mPeakDb), juce::PathStrokeType (1.0f));
    g.setColour (kCyan);
    g.strokePath (buildTrace (mAvgDb), juce::PathStrokeType (1.4f));
}
