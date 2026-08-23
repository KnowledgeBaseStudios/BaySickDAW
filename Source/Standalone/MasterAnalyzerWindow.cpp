#include "MasterAnalyzerWindow.h"
#include "../PluginProcessor.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    // The app's analyzer palette: the suite's cyan accent over the KBS Meter
    // Suite's dark panel, with its verdict colors (green on target, amber
    // within 3 LU, red beyond) and its red for anything over the ceiling.
    const juce::Colour kCyan   { 0xff00fff2 };
    const juce::Colour kOrange { 0xffff9100 };
    const juce::Colour kBg     { 0xff101214 };
    const juce::Colour kPanel  { 0xff16191c };
    const juce::Colour kEdge   { 0xff2c3036 };
    const juce::Colour kTrack  { 0xff1b1f23 };
    const juce::Colour kGrid   { 0xff23282c };
    const juce::Colour kGridLo { 0xff1d2124 };
    const juce::Colour kText   { 0xffe6e8ea };
    const juce::Colour kDim    { 0xff8f9aa1 };
    const juce::Colour kGreen  { 0xff45c46a };
    const juce::Colour kAmber  { 0xffd8a42a };
    const juce::Colour kRed    { 0xffd85a4a };
    const juce::Colour kCompare{ 0xffd8a42a };

    juce::Font monoFont (float h, bool bold = false)
    {
        return juce::Font (juce::Font::getDefaultMonospacedFontName(), h,
                           bold ? juce::Font::bold : juce::Font::plain);
    }
    juce::Font uiFont (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }
    juce::String fmt (float v, int dp = 1)
    {
        return v <= -119.0f ? juce::String ("--.-") : juce::String (v, dp);
    }
    juce::String timecode (double s)
    {
        if (s < 0.0) s = 0.0;
        const int t = (int) s;
        return juce::String (t / 60).paddedLeft ('0', 2) + ":" + juce::String (t % 60).paddedLeft ('0', 2);
    }

    // EBU Tech 3342 loudness range over a short-term series: -70 absolute gate,
    // -20 LU relative gate, 10th..95th percentile.  Used for the shaded band.
    bool lraBand (const std::vector<float>& st, float& lo, float& hi)
    {
        std::vector<float> abs;
        abs.reserve (st.size());
        for (float v : st) if (v > -70.0f) abs.push_back (v);
        if (abs.size() < 2) return false;
        double sum = 0.0;
        for (float v : abs) sum += std::pow (10.0, (v + 0.691) / 10.0);
        const float mean = (float) (-0.691 + 10.0 * std::log10 (sum / (double) abs.size()));
        std::vector<float> rel;
        for (float v : abs) if (v > mean - 20.0f) rel.push_back (v);
        if (rel.size() < 2) return false;
        std::sort (rel.begin(), rel.end());
        auto q = [&] (double x) { return rel[(size_t) juce::jlimit (0, (int) rel.size() - 1, (int) std::lround (x * (double) (rel.size() - 1)))]; };
        lo = q (0.10); hi = q (0.95);
        return true;
    }
}

MasterAnalyzerView::MasterAnalyzerView (BaySickDAWProcessor& proc)
    : mProc (proc),
      mFft (kFftOrder),
      mWindow ((size_t) kFftSize, juce::dsp::WindowingFunction<float>::hann)
{
    mScratch.assign ((size_t) kFftSize * 2, 0.0f);
    mFeedBuf.assign ((size_t) SpectrumFeed::kSize, 0.0f);
    mAccum  .assign ((size_t) kFftSize, 0.0f);
    mAvgDb .fill (kMinDb);
    mPeakDb.fill (kMinDb);
    mLiveShort.reserve ((size_t) kMaxHistory);
    mLiveMom  .reserve ((size_t) kMaxHistory);

    setTooltip ("Master analyzer - levels, loudness over time and output spectrum, "
                "tapped after the master fader");

    // ── TS7 §3.6: source picker (Live + every captured take) ─────────────────
    mSourceBox = std::make_unique<juce::ComboBox>();
    mSourceBox->setTextWhenNothingSelected ("Live");
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

    // SC-16: a second take laid over the first (A/B).
    mCompareBox = std::make_unique<juce::ComboBox>();
    mCompareBox->setTextWhenNothingSelected ("vs ...");
    mCompareBox->setTooltip ("Overlay another take on the Loudness view for A/B.");
    mCompareBox->onChange = [this]
    {
        if (mRebuildingList) return;
        mCompareCurve.clear();
        mCompareLabel.clear();
        const int id = mCompareBox->getSelectedId() - 1;
        if (mVersions != nullptr && id > 0)
            if (const auto* v = mVersions->find (id))
            {
                mCompareCurve = v->lufsCurve;
                mCompareLabel = v->label;
            }
        repaint();
    };
    addAndMakeVisible (*mCompareBox);

    // QA-TrueLevel SC-15: exports the take's REPORT (every take has one; the
    // audio rides along when it was captured), so the button lights for any
    // selected take instead of only audio-captured ones.
    mExportBtn = std::make_unique<juce::TextButton> ("Export Take...");
    mExportBtn->setEnabled (false);
    mExportBtn->onClick = [this]
    {
        if (mVersions == nullptr || ! onExportTake) return;
        if (const auto* v = mVersions->find (mSelectedTakeId)) onExportTake (*v);
    };
    addAndMakeVisible (*mExportBtn);

    mRemoveBtn = std::make_unique<juce::TextButton> ("Remove Take");
    mRemoveBtn->setEnabled (false);
    mRemoveBtn->onClick = [this]
    {
        if (mVersions == nullptr || ! onRemoveTake || mSelectedTakeId <= 0) return;
        const int id = mSelectedTakeId;
        selectSource (0);
        onRemoveTake (id);
    };
    addAndMakeVisible (*mRemoveBtn);

    // ── The KBS view bar: Levels / Loudness / Spectrum, Reset, + our two ─────
    static const char* kNames[] = { "Levels", "Loudness", "Spectrum" };
    for (int v = 0; v < 3; ++v)
    {
        auto b = std::make_unique<juce::TextButton> (kNames[v]);
        b->setClickingTogglesState (false);
        b->onClick = [this, v] { setViewMode ((ViewMode) v); };
        addAndMakeVisible (*b);
        mViewButtons.push_back (std::move (b));
    }
    mResetBtn = std::make_unique<juce::TextButton> ("Reset");
    mResetBtn->setTooltip ("Clear integrated loudness, history, max true peak, "
                           "over-ceiling count and peak hold");
    mResetBtn->onClick = [this] { resetHistory(); };
    addAndMakeVisible (*mResetBtn);

    mTiltBtn = std::make_unique<juce::TextButton> ("Tilt 0");
    mTiltBtn->setTooltip ("Spectrum slope: 0, 3 or 4.5 dB per octave (pink-noise reads flat at 3)");
    mTiltBtn->onClick = [this]
    {
        mTilt = (mTilt + 1) % 3;
        mTiltBtn->setButtonText (mTilt == 0 ? "Tilt 0" : mTilt == 1 ? "Tilt 3" : "Tilt 4.5");
        repaint();
    };
    addAndMakeVisible (*mTiltBtn);

    mOctBtn = std::make_unique<juce::TextButton> ("1/3 oct");
    mOctBtn->setClickingTogglesState (true);
    mOctBtn->setTooltip ("Third-octave bands instead of the line");
    mOctBtn->onClick = [this] { mThirdOctave = mOctBtn->getToggleState(); repaint(); };
    addAndMakeVisible (*mOctBtn);

    mDbfsMeter = std::make_unique<DBFSMeter>();
    mDbfsMeter->setMeterLayout (DBFSMeter::Layout::Full);
    mDbfsMeter->setTooltip ("Master output level in dBFS - the master strip's meter");
    addChildComponent (*mDbfsMeter);

    refreshVersions();
    setViewMode (ViewMode::Loudness);
}

MasterAnalyzerView::~MasterAnalyzerView()
{
    stopTimer();
    setTapActive (false);
}

void MasterAnalyzerView::resized()
{
    auto r = getLocalBounds().removeFromTop ((int) kControlsH).reduced (4, 3);
    if (mRemoveBtn) { mRemoveBtn->setBounds (r.removeFromRight (96)); r.removeFromRight (4); }
    if (mExportBtn) { mExportBtn->setBounds (r.removeFromRight (96)); r.removeFromRight (4); }
    if (mCompareBox) { mCompareBox->setBounds (r.removeFromRight (juce::jmin (170, r.getWidth() / 3))); r.removeFromRight (4); }
    if (mSourceBox) mSourceBox->setBounds (r);

    auto bar = getLocalBounds().withTrimmedTop ((int) kControlsH).removeFromTop ((int) kViewBarH).reduced (6, 2);
    if (mResetBtn) { mResetBtn->setBounds (bar.removeFromRight (52)); bar.removeFromRight (6); }
    if (mOctBtn)   { mOctBtn  ->setBounds (bar.removeFromRight (58)); bar.removeFromRight (4); }
    if (mTiltBtn)  { mTiltBtn ->setBounds (bar.removeFromRight (58)); bar.removeFromRight (6); }
    const int w = juce::jmin (74, bar.getWidth() / 4);
    for (auto& b : mViewButtons)
    {
        b->setBounds (bar.removeFromLeft (w));
        bar.removeFromLeft (3);
    }

    if (mDbfsMeter)
    {
        auto side = bodyBounds().reduced (6.0f, 6.0f);
        side = side.removeFromRight (54.0f).reduced (4.0f, 0.0f);
        side.removeFromTop (12.0f);
        mDbfsMeter->setBounds (side.toNearestInt());
    }
}

void MasterAnalyzerView::setViewMode (ViewMode m)
{
    mView = m;
    if (m == ViewMode::Spectrum && mShowingRender)
        selectSource (0);   // a render has no live spectrum
    for (int v = 0; v < (int) mViewButtons.size(); ++v)
        mViewButtons[(size_t) v]->setColour (juce::TextButton::buttonColourId,
                                            v == (int) m ? kCyan.withAlpha (0.30f) : kTrack);
    const bool spec = (m == ViewMode::Spectrum);
    if (mTiltBtn) mTiltBtn->setVisible (spec);
    if (mOctBtn)  mOctBtn ->setVisible (spec);
    if (mDbfsMeter) mDbfsMeter->setVisible (spec);
    repaint();
}

// ── Source list ──────────────────────────────────────────────────────────────

void MasterAnalyzerView::setVersionSource (VersionCapture* vc)
{
    mVersions = vc;
    refreshVersions();
}

void MasterAnalyzerView::refreshVersions()
{
    if (! mSourceBox) return;
    mRebuildingList = true;
    mSourceBox->clear (juce::dontSendNotification);
    mSourceBox->addItem ("Live", 1);   // ids are takeId + 1; 0 is not a legal id
    bool selectionStillExists = (mSelectedTakeId == 0);
    if (mVersions != nullptr)
    {
        for (const auto& v : mVersions->versions())
        {
            juce::String text = v.label;
            if (v.fromReport) text += "   (report)";
            else if (v.audioFile == juce::File()) text += "   (no audio)";
            mSourceBox->addItem (text, v.id + 1);
            if (v.id == mSelectedTakeId) selectionStillExists = true;
        }
    }
    if (! selectionStillExists) mSelectedTakeId = 0;
    mSourceBox->setSelectedId (mSelectedTakeId + 1, juce::dontSendNotification);
    rebuildCompareList();
    mRebuildingList = false;
    applySelection();
}

void MasterAnalyzerView::rebuildCompareList()
{
    if (! mCompareBox) return;
    const int keep = mCompareBox->getSelectedId();
    mCompareBox->clear (juce::dontSendNotification);
    mCompareBox->addItem ("No overlay", 1);
    bool still = (keep <= 1);
    if (mVersions != nullptr)
        for (const auto& v : mVersions->versions())
        {
            mCompareBox->addItem (v.label, v.id + 1);
            if (v.id + 1 == keep) still = true;
        }
    if (! still) { mCompareCurve.clear(); mCompareLabel.clear(); }
    mCompareBox->setSelectedId (still ? juce::jmax (1, keep) : 1, juce::dontSendNotification);
}

void MasterAnalyzerView::selectSource (int takeId)
{
    mSelectedTakeId = takeId;
    if (mSourceBox)
    {
        mRebuildingList = true;
        mSourceBox->setSelectedId (takeId + 1, juce::dontSendNotification);
        mRebuildingList = false;
    }
    applySelection();
}

void MasterAnalyzerView::applySelection()
{
    const VersionCapture::Version* v =
        (mVersions != nullptr && mSelectedTakeId > 0) ? mVersions->find (mSelectedTakeId) : nullptr;
    if (v == nullptr)
    {
        if (mExportBtn) mExportBtn->setEnabled (false);
        if (mRemoveBtn) mRemoveBtn->setEnabled (false);
        showLive();
        return;
    }
    if (mExportBtn) mExportBtn->setEnabled (true);
    if (mRemoveBtn) mRemoveBtn->setEnabled (true);
    int over = 0;
    for (const auto& viol : v->violations)
        if (viol.kind == LoudnessViolation::Kind::TruePeak) ++over;
    showRenderedCurve (v->lufsCurve, v->integratedLufs, v->lraLu, v->truePeakDb, v->label,
                       v->momentaryCurve, over);
}

// ── Tap / timer ──────────────────────────────────────────────────────────────

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
    mLiveShort.clear();
    mLiveMom.clear();
    mOverCeilingLive = 0;
    mPeakDb = mAvgDb;
    mProc.resetMasterTruePeakMax();
    mProc.mVibeGraph.resetMasterLufsIntegrated();   // the reading the tooltip promises to clear
    mViewX0 = mViewX1 = 0.0;
    repaint();
}

void MasterAnalyzerView::showRenderedCurve (const std::vector<float>& lufsCurve,
                                            float integrated, float lra, float truePeakDb,
                                            const juce::String& label,
                                            const std::vector<float>& momentaryCurve,
                                            int overCeilingCount)
{
    mRenderCurve       = lufsCurve;
    mRenderMom         = momentaryCurve;
    mRenderIntegrated  = integrated;
    mRenderLra         = lra;
    mRenderTruePeak    = truePeakDb;
    mRenderLabel       = label;
    mRenderOverCeiling = juce::jmax (0, overCeilingCount);
    mShowingRender     = ! lufsCurve.empty();
    mViewX0 = mViewX1 = 0.0;
    if (mView == ViewMode::Spectrum) setViewMode (ViewMode::Loudness);
    repaint();
}

void MasterAnalyzerView::showLive()
{
    mShowingRender = false;
    mViewX0 = mViewX1 = 0.0;
    repaint();
}

void MasterAnalyzerView::timerCallback()
{
    int count = 0;
    const bool got = mProc.pollMasterSpectrum (mFeedBuf.data(), count);
    const float decay = kPeakDecayDbPerSec / (float) kTimerHz;
    if (got && count > 0)
    {
        // Frames are stitched into a kFftSize ring: the feed delivers 1024 at a
        // time and an 8192-point transform is what gives the bottom octave.
        const int n = juce::jmin (count, kFftSize);
        if (n < kFftSize)
            std::memmove (mAccum.data(), mAccum.data() + n, (size_t) (kFftSize - n) * sizeof (float));
        std::copy (mFeedBuf.begin(), mFeedBuf.begin() + n, mAccum.end() - n);
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
                const float db = juce::Decibels::gainToDecibels (mScratch[(size_t) i] * norm, kMinDb);
                // Fast up, slow down: transients register and the trace does not
                // flicker on the way back.
                mAvgDb[(size_t) i] = (mHaveFrame && db < mAvgDb[(size_t) i])
                    ? (mAvgDb[(size_t) i] + kAvgAlpha * (db - mAvgDb[(size_t) i])) : db;
                mPeakDb[(size_t) i] = juce::jmax (db, mPeakDb[(size_t) i] - decay);
            }
            mHaveFrame = true;
        }
    }
    else
    {
        for (auto& p : mPeakDb) p = juce::jmax (kMinDb, p - decay);
    }

    if (mDbfsMeter != nullptr && mView == ViewMode::Spectrum)
    {
        const auto [pl, pr] = mProc.drainMasterAnalyzerPeakDb();
        mDbfsMeter->setStereoLevel (pl, pr);
    }

    // Over-ceiling moments, counted on the instantaneous true peak's rising edge.
    const float tp = mProc.getMasterTruePeakDb();
    if (tp > mCeilingDb && mLastLiveTp <= mCeilingDb) ++mOverCeilingLive;
    mLastLiveTp = tp;

    if (++mTickCounter >= (kTimerHz / kHistoryHz))
    {
        mTickCounter = 0;
        mLiveShort.push_back (mProc.getMasterLufs (1));
        mLiveMom  .push_back (mProc.getMasterLufs (0));
        if ((int) mLiveShort.size() > kMaxHistory)
        {
            mLiveShort.erase (mLiveShort.begin(), mLiveShort.begin() + kHistoryHz * 60);
            mLiveMom  .erase (mLiveMom.begin(),   mLiveMom.begin()   + kHistoryHz * 60);
        }
    }
    repaint (bodyBounds().toNearestInt());
}

// ── Geometry + helpers ───────────────────────────────────────────────────────

juce::Rectangle<float> MasterAnalyzerView::bodyBounds() const
{
    return getLocalBounds().toFloat().withTrimmedTop (kControlsH + kViewBarH).reduced (8.0f, 6.0f);
}

juce::Rectangle<float> MasterAnalyzerView::plotBounds() const
{
    auto b = bodyBounds();
    b.removeFromRight (96.0f);   // the side cells
    return b.reduced (2.0f, 0.0f);
}

float MasterAnalyzerView::xForFrequency (float hz, float w) const
{
    const float lo = std::log10 (kMinHz);
    const float hi = std::log10 (kMaxHz);
    const float t  = (std::log10 (juce::jlimit (kMinHz, kMaxHz, hz)) - lo) / (hi - lo);
    return t * w;
}

juce::Colour MasterAnalyzerView::verdictColour (float lufs) const
{
    if (lufs <= -119.0f) return kCyan;
    const float d = std::abs (lufs - mTargetLufs);
    return d <= 1.0f ? kGreen : d <= 3.0f ? kAmber : kRed;
}

const std::vector<float>& MasterAnalyzerView::shortTermSeries() const noexcept
{
    return mShowingRender ? mRenderCurve : mLiveShort;
}
const std::vector<float>& MasterAnalyzerView::momentarySeries() const noexcept
{
    return mShowingRender ? mRenderMom : mLiveMom;
}

// ── Mouse: zoom / pan / hover on the history ─────────────────────────────────

void MasterAnalyzerView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& d)
{
    if (mView != ViewMode::Loudness) return;
    const auto plot = plotBounds();
    if (! plot.contains (e.position)) return;
    const double dur = (double) shortTermSeries().size() / (double) kHistoryHz;
    if (dur <= 0.0) return;
    double x0 = mViewX0, x1 = mViewX1;
    if (x1 <= x0) { x0 = 0.0; x1 = dur; }
    const double fx = (e.position.x - plot.getX()) / plot.getWidth();
    const double t  = x0 + fx * (x1 - x0);
    const double k  = d.deltaY < 0.0f ? 1.25 : 0.8;
    double w = juce::jlimit (0.5, dur, (x1 - x0) * k);
    x0 = t - fx * w; x1 = x0 + w;
    if (x0 < 0.0) { x0 = 0.0; x1 = w; }
    if (x1 > dur) { x1 = dur; x0 = juce::jmax (0.0, dur - w); }
    mViewX0 = x0; mViewX1 = x1;
    repaint();
}

void MasterAnalyzerView::mouseDown (const juce::MouseEvent& e)
{
    mDragStartX0 = mViewX0; mDragStartX1 = mViewX1; mDragStartPx = e.x;
}

void MasterAnalyzerView::mouseDrag (const juce::MouseEvent& e)
{
    if (mView != ViewMode::Loudness) return;
    const double dur = (double) shortTermSeries().size() / (double) kHistoryHz;
    if (dur <= 0.0 || mDragStartX1 <= mDragStartX0) return;
    const auto plot = plotBounds();
    const double dt = (double) (e.x - mDragStartPx) / plot.getWidth() * (mDragStartX1 - mDragStartX0);
    double x0 = mDragStartX0 - dt, x1 = mDragStartX1 - dt;
    const double w = x1 - x0;
    if (x0 < 0.0) { x0 = 0.0; x1 = w; }
    if (x1 > dur) { x1 = dur; x0 = juce::jmax (0.0, dur - w); }
    mViewX0 = x0; mViewX1 = x1;
    repaint();
}

void MasterAnalyzerView::mouseDoubleClick (const juce::MouseEvent&)
{
    mViewX0 = mViewX1 = 0.0;
    repaint();
}

void MasterAnalyzerView::mouseMove (const juce::MouseEvent& e)
{
    mHoverX = plotBounds().contains (e.position) ? e.position.x : -1.0f;
}

void MasterAnalyzerView::mouseExit (const juce::MouseEvent&) { mHoverX = -1.0f; }

// ── Paint ────────────────────────────────────────────────────────────────────

void MasterAnalyzerView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (kPanel);
    g.fillRect (b);
    const auto body = bodyBounds();
    g.setColour (kBg);
    g.fillRoundedRectangle (body, 4.0f);
    g.setColour (kEdge);
    g.drawRoundedRectangle (body, 4.0f, 1.0f);
    if (body.getWidth() <= 40.0f || body.getHeight() <= 40.0f) return;

    switch (mView)
    {
        case ViewMode::Levels:   paintLevels   (g, body.reduced (10.0f, 8.0f)); break;
        case ViewMode::Loudness: paintLoudness (g, body.reduced (6.0f, 6.0f));  break;
        case ViewMode::Spectrum: paintSpectrum (g, body.reduced (6.0f, 6.0f));  break;
    }

    if (mShowingRender)
    {
        g.setColour (kOrange);
        g.setFont (monoFont (8.5f, true));
        g.drawText ("TAKE: " + mRenderLabel, body.reduced (6.0f, 2.0f),
                    juce::Justification::topRight, false);
    }
}

// ── View 1: the numbers ──────────────────────────────────────────────────────

void MasterAnalyzerView::paintLevels (juce::Graphics& g, juce::Rectangle<float> body)
{
    const bool  rend  = mShowingRender;
    const float lufsI = rend ? mRenderIntegrated : mProc.getMasterLufs (2);
    const float lufsM = rend ? -120.0f : mProc.getMasterLufs (0);
    const float lufsS = rend ? -120.0f : mProc.getMasterLufs (1);

    auto top = body.removeFromTop (juce::jmax (44.0f, body.getHeight() * 0.38f));
    g.setColour (kDim);
    g.setFont (uiFont (10.0f));
    g.drawText ("INTEGRATED", top.removeFromTop (12.0f), juce::Justification::centred);
    g.setColour (verdictColour (lufsI));
    g.setFont (uiFont (juce::jlimit (22.0f, 44.0f, top.getHeight() - 14.0f), true));
    g.drawText (fmt (lufsI), top.removeFromTop (top.getHeight() - 12.0f), juce::Justification::centred);
    g.setColour (kDim);
    g.setFont (uiFont (9.5f));
    g.drawText ("LUFS   target " + juce::String (mTargetLufs, 1)
                + (lufsI > -119.0f ? "   (" + juce::String (lufsI - mTargetLufs, 1) + " LU)" : ""),
                top, juce::Justification::centred);

    body.removeFromTop (6.0f);
    auto bars = body.removeFromTop (juce::jmax (30.0f, body.getHeight() * 0.42f));
    const float rowH = bars.getHeight() * 0.5f;
    loudnessRow (g, bars.removeFromTop (rowH), "M", lufsM);
    loudnessRow (g, bars, "S", lufsS);

    body.removeFromTop (6.0f);
    auto peaks = body.removeFromLeft (body.getWidth() * 0.62f);
    paintPeaks (g, peaks);
    body.removeFromLeft (8.0f);
    paintCorrelation (g, body);
}

void MasterAnalyzerView::loudnessRow (juce::Graphics& g, juce::Rectangle<float> r, const char* label, float lufs)
{
    r = r.reduced (0.0f, 2.0f);
    g.setColour (kDim);
    g.setFont (uiFont (10.5f, true));
    g.drawText (label, r.removeFromLeft (16.0f), juce::Justification::centredLeft);
    g.setColour (kText);
    g.setFont (uiFont (11.5f));
    g.drawText (fmt (lufs), r.removeFromRight (50.0f), juce::Justification::centredRight);

    auto track = r.reduced (4.0f, 0.0f);
    g.setColour (kTrack);
    g.fillRoundedRectangle (track, 2.0f);
    constexpr float range = 30.0f;
    const float lo = mTargetLufs - range * 0.5f;
    if (lufs > -119.0f)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (lufs - lo) / range);
        g.setColour (kCyan.withAlpha (0.85f));
        g.fillRoundedRectangle (track.withWidth (track.getWidth() * t), 2.0f);
    }
    // The target sits at the center of the scale: "on target" is the middle
    // of the bar rather than a number to remember.
    const float mid = track.getX() + track.getWidth() * 0.5f;
    g.setColour (kText.withAlpha (0.5f));
    g.drawLine (mid, track.getY(), mid, track.getBottom(), 1.0f);
}

void MasterAnalyzerView::paintPeaks (juce::Graphics& g, juce::Rectangle<float> r)
{
    const bool rend = mShowingRender;
    g.setColour (kDim);
    g.setFont (uiFont (9.5f));
    auto head = r.removeFromTop (11.0f);
    g.drawText ("TRUE PEAK", head, juce::Justification::centredLeft);

    const float v[2] = { rend ? mRenderTruePeak : mProc.getMasterTruePeakDbChannel (0),
                         rend ? mRenderTruePeak : mProc.getMasterTruePeakDbChannel (1) };
    const float mx[2] = { rend ? mRenderTruePeak : mProc.getMasterTruePeakMaxDbChannel (0),
                          rend ? mRenderTruePeak : mProc.getMasterTruePeakMaxDbChannel (1) };
    const int over = rend ? mRenderOverCeiling : mOverCeilingLive;
    g.setColour (over > 0 ? kRed : kDim);
    g.drawText (juce::String (over) + (over == 1 ? " moment over " : " moments over ")
                + juce::String (mCeilingDb, 1) + " dBTP", head, juce::Justification::centredRight);

    const float rowH = juce::jmax (9.0f, r.getHeight() * 0.5f - 3.0f);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto row = r.removeFromTop (rowH);
        const bool isOver = v[ch] > mCeilingDb;
        g.setColour (isOver ? kRed : kText);
        g.setFont (uiFont (10.5f));
        g.drawText (fmt (v[ch]) + "  max " + fmt (mx[ch]), row.removeFromRight (110.0f), juce::Justification::centredRight);
        auto track = row.reduced (0.0f, 2.0f);
        g.setColour (kTrack);
        g.fillRoundedRectangle (track, 2.0f);
        const float t = juce::jlimit (0.0f, 1.0f, (v[ch] + 40.0f) / 40.0f);
        g.setColour (isOver ? kRed : kCyan.withAlpha (0.85f));
        g.fillRoundedRectangle (track.withWidth (track.getWidth() * t), 2.0f);
        const float cx = track.getX() + track.getWidth() * (mCeilingDb + 40.0f) / 40.0f;
        g.setColour (kRed.withAlpha (0.8f));
        g.drawLine (cx, track.getY(), cx, track.getBottom(), 1.0f);
        r.removeFromTop (2.0f);
    }

    // PLR (peak to loudness) and PSR (peak to short-term): dynamics at a glance.
    const float integ = rend ? mRenderIntegrated : mProc.getMasterLufs (2);
    const float st    = rend ? -120.0f : mProc.getMasterLufs (1);
    const float tpMax = rend ? mRenderTruePeak : juce::jmax (mx[0], mx[1]);
    const float tpNow = rend ? mRenderTruePeak : juce::jmax (v[0], v[1]);
    juce::String dyn;
    if (integ > -119.0f && tpMax > -119.0f) dyn << "PLR " << juce::String (tpMax - integ, 1);
    if (st > -119.0f && tpNow > -119.0f)    dyn << (dyn.isEmpty() ? "" : "     ") << "PSR " << juce::String (tpNow - st, 1);
    if (dyn.isNotEmpty())
    {
        g.setColour (kDim);
        g.setFont (uiFont (9.5f));
        g.drawText (dyn, r, juce::Justification::centredLeft);
    }
}

void MasterAnalyzerView::paintCorrelation (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (kDim);
    g.setFont (uiFont (9.5f));
    g.drawText ("CORR", r.removeFromTop (11.0f), juce::Justification::centredLeft);
    const float c = mShowingRender ? 0.0f : mProc.getMasterCorrelation();
    auto track = r.removeFromTop (juce::jmax (9.0f, r.getHeight() * 0.5f)).reduced (0.0f, 2.0f);
    g.setColour (kTrack);
    g.fillRoundedRectangle (track, 2.0f);
    // Fills outward from the center: right for correlated, left for the
    // out-of-phase content that vanishes on a mono system.
    const float mid = track.getX() + track.getWidth() * 0.5f;
    const float w   = track.getWidth() * 0.5f * std::abs (c);
    g.setColour (c < -0.05f ? kRed : kCyan.withAlpha (0.85f));
    g.fillRoundedRectangle ({ c < 0 ? mid - w : mid, track.getY(), w, track.getHeight() }, 2.0f);
    g.setColour (kText.withAlpha (0.5f));
    g.drawLine (mid, track.getY(), mid, track.getBottom(), 1.0f);
    g.setColour (kText);
    g.setFont (uiFont (10.5f));
    g.drawText (mShowingRender ? juce::String ("--") : juce::String (c, 2), r, juce::Justification::centredLeft);
}

// ── View 2: loudness over time ───────────────────────────────────────────────

void MasterAnalyzerView::paintLoudness (juce::Graphics& g, juce::Rectangle<float> body)
{
    auto side = body.removeFromRight (96.0f);
    auto plot = body.reduced (2.0f, 0.0f);

    const auto& st = shortTermSeries();
    const auto& mo = momentarySeries();
    const double dur = (double) juce::jmax (st.size(), mo.size()) / (double) kHistoryHz;
    double x0 = mViewX0, x1 = mViewX1;
    if (x1 <= x0) { x0 = 0.0; x1 = juce::jmax (1.0, dur); }

    constexpr float range = 32.0f;
    // Weighted below the target: material sits under it far more often than over.
    const float lo = mTargetLufs - range * 0.75f;
    const float hi = mTargetLufs + range * 0.25f;
    auto yFor = [&] (float lufs) { return plot.getBottom() - plot.getHeight() * juce::jlimit (0.0f, 1.0f, (lufs - lo) / (hi - lo)); };
    auto xFor = [&] (double secs) { return (float) (plot.getX() + plot.getWidth() * (secs - x0) / (x1 - x0)); };

    g.setColour (kTrack);
    g.fillRoundedRectangle (plot, 3.0f);

    // LRA band: 10th..95th percentile of the visible short-term, shaded.
    {
        const int i0 = juce::jlimit (0, (int) st.size(), (int) std::floor (x0 * kHistoryHz));
        const int i1 = juce::jlimit (0, (int) st.size(), (int) std::ceil  (x1 * kHistoryHz));
        if (i1 > i0)
        {
            std::vector<float> vis (st.begin() + i0, st.begin() + i1);
            float blo = 0.0f, bhi = 0.0f;
            if (lraBand (vis, blo, bhi))
            {
                g.setColour (kAmber.withAlpha (0.10f));
                g.fillRect (juce::Rectangle<float> (plot.getX(), yFor (bhi), plot.getWidth(), juce::jmax (1.0f, yFor (blo) - yFor (bhi))));
            }
        }
    }

    g.setFont (monoFont (8.5f));
    for (float v = std::ceil (lo / 6.0f) * 6.0f; v <= hi; v += 6.0f)
    {
        const float y = yFor (v);
        g.setColour (kGrid.withAlpha (0.7f));
        g.drawLine (plot.getX(), y, plot.getRight(), y, 1.0f);
        g.setColour (kDim.withAlpha (0.7f));
        g.drawText (juce::String ((int) v), (int) plot.getX() + 3, (int) y - 9, 26, 10, juce::Justification::centredLeft);
    }
    const double span = x1 - x0;
    const double step = span > 600.0 ? 120.0 : span > 120.0 ? 30.0 : span > 40.0 ? 10.0 : span > 10.0 ? 5.0 : 1.0;
    for (double t = std::ceil (x0 / step) * step; t <= x1; t += step)
    {
        const float x = xFor (t);
        g.setColour (kGridLo);
        g.drawLine (x, plot.getY(), x, plot.getBottom(), 1.0f);
        g.setColour (kDim.withAlpha (0.6f));
        g.drawText (timecode (t), (int) x + 3, (int) plot.getBottom() - 11, 40, 10, juce::Justification::centredLeft);
    }

    auto drawSeries = [&] (const std::vector<float>& s, bool filled, juce::Colour col, float stroke)
    {
        if (s.empty()) return;
        const int i0 = juce::jlimit (0, (int) s.size() - 1, (int) std::floor (x0 * kHistoryHz));
        const int i1 = juce::jlimit (0, (int) s.size() - 1, (int) std::ceil  (x1 * kHistoryHz));
        if (filled)
        {
            juce::Path fill;
            fill.startNewSubPath (xFor ((double) i0 / kHistoryHz), plot.getBottom());
            for (int i = i0; i <= i1; ++i) fill.lineTo (xFor ((double) i / kHistoryHz), yFor (s[(size_t) i]));
            fill.lineTo (xFor ((double) i1 / kHistoryHz), plot.getBottom());
            fill.closeSubPath();
            g.setColour (col);
            g.fillPath (fill);
            return;
        }
        juce::Path line;
        bool started = false;
        for (int i = i0; i <= i1; ++i)
        {
            const float v = s[(size_t) i];
            if (v <= -119.0f) { started = false; continue; }
            const float x = xFor ((double) i / kHistoryHz), y = yFor (v);
            if (! started) { line.startNewSubPath (x, y); started = true; }
            else             line.lineTo (x, y);
        }
        g.setColour (col);
        g.strokePath (line, juce::PathStrokeType (stroke));
    };
    // Short-term as a filled area, momentary as a line over it: the fill reads
    // as where it has been, the line as where it is.
    drawSeries (st, true,  kCyan.withAlpha (0.28f), 0.0f);
    drawSeries (mo, false, kCyan, 1.3f);
    if (! mCompareCurve.empty())
        drawSeries (mCompareCurve, false, kCompare.withAlpha (0.9f), 1.0f);

    // The target last, so it sits on top of both.
    {
        const float y = yFor (mTargetLufs);
        g.setColour (kGreen.withAlpha (0.95f));
        const float dash[2] { 5.0f, 4.0f };
        g.drawDashedLine ({ plot.getX(), y, plot.getRight(), y }, dash, 2, 1.4f);
        g.setFont (monoFont (8.5f, true));
        g.drawText ("TARGET " + juce::String (mTargetLufs, 1), (int) plot.getRight() - 90, (int) y - 11, 88, 10, juce::Justification::centredRight);
    }

    // Hover readout.
    if (mHoverX >= 0.0f && plot.getWidth() > 0.0f)
    {
        const double t = x0 + (mHoverX - plot.getX()) / plot.getWidth() * (x1 - x0);
        const int i = (int) std::lround (t * kHistoryHz);
        if (i >= 0 && (i < (int) st.size() || i < (int) mo.size()))
        {
            g.setColour (kText.withAlpha (0.35f));
            g.drawLine (mHoverX, plot.getY(), mHoverX, plot.getBottom(), 1.0f);
            juce::String txt = timecode (t) + "  S " + fmt (i < (int) st.size() ? st[(size_t) i] : -120.0f)
                             + "  M " + fmt (i < (int) mo.size() ? mo[(size_t) i] : -120.0f);
            g.setFont (monoFont (9.0f));
            const float tw = g.getCurrentFont().getStringWidthFloat (txt) + 10.0f;
            const float bx = juce::jmin (mHoverX + 8.0f, plot.getRight() - tw - 4.0f);
            g.setColour (kPanel.withAlpha (0.92f));
            g.fillRect (bx, plot.getY() + 6.0f, tw, 15.0f);
            g.setColour (kText);
            g.drawText (txt, (int) bx + 5, (int) plot.getY() + 6, (int) tw, 15, juce::Justification::centredLeft);
        }
    }

    g.setColour (kEdge);
    g.drawRoundedRectangle (plot, 3.0f, 1.0f);
    g.setColour (kDim.withAlpha (0.7f));
    g.setFont (monoFont (8.5f));
    g.drawText (x1 - x0 >= dur - 0.01 ? "all " + timecode (dur) : timecode (x0) + " - " + timecode (x1),
                (int) plot.getX() + 3, (int) plot.getBottom() - 11, 120, 10, juce::Justification::centredLeft);
    if (! mCompareLabel.isEmpty())
    {
        g.setColour (kCompare);
        g.drawText ("vs " + mCompareLabel, (int) plot.getX() + 3, (int) plot.getY() + 3, (int) plot.getWidth() - 6, 10, juce::Justification::centredLeft);
    }

    // Side cells.
    side = side.reduced (5.0f, 0.0f);
    const float cellH = side.getHeight() / 5.0f;
    auto cell = [&] (const char* label, const juce::String& v, juce::Colour c)
    {
        auto row = side.removeFromTop (cellH);
        g.setColour (kDim);
        g.setFont (uiFont (8.0f));
        g.drawText (label, row.removeFromTop (10.0f), juce::Justification::centredLeft);
        g.setColour (c);
        g.setFont (uiFont (13.0f, true));
        g.drawText (v, row, juce::Justification::centredLeft);
    };
    const bool rend = mShowingRender;
    const float integ = rend ? mRenderIntegrated : mProc.getMasterLufs (2);
    cell ("INTEGRATED", fmt (integ), verdictColour (integ));
    cell ("SHORT-TERM", fmt (rend ? (st.empty() ? -120.0f : st.back()) : mProc.getMasterLufs (1)), kText);
    cell ("MOMENTARY",  fmt (rend ? (mo.empty() ? -120.0f : mo.back()) : mProc.getMasterLufs (0)), kText);
    cell ("LRA", rend ? juce::String (mRenderLra, 1) + " LU" : juce::String ("--"), kText);
    {
        const float l = rend ? mRenderTruePeak : mProc.getMasterTruePeakMaxDbChannel (0);
        const float r2 = rend ? mRenderTruePeak : mProc.getMasterTruePeakMaxDbChannel (1);
        auto row = side.removeFromTop (cellH);
        g.setColour (kDim);
        g.setFont (uiFont (8.0f));
        g.drawText ("MAX TP  L / R", row.removeFromTop (10.0f), juce::Justification::centredLeft);
        g.setFont (uiFont (11.0f, true));
        g.setColour (l > mCeilingDb ? kRed : kText);
        g.drawText (fmt (l), row.removeFromLeft (row.getWidth() * 0.5f), juce::Justification::centredLeft);
        g.setColour (r2 > mCeilingDb ? kRed : kText);
        g.drawText (fmt (r2), row, juce::Justification::centredLeft);
    }
}

// ── View 3: the spectrum ─────────────────────────────────────────────────────

void MasterAnalyzerView::paintSpectrum (juce::Graphics& g, juce::Rectangle<float> body)
{
    auto side = body.removeFromRight (54.0f);
    auto plot = body.reduced (2.0f, 0.0f);
    const float w = plot.getWidth(), h = plot.getHeight();

    g.setColour (kTrack);
    g.fillRoundedRectangle (plot, 3.0f);

    constexpr float kTopDb = 6.0f, kBotDb = -96.0f;
    auto yFor = [&] (float db) { return plot.getBottom() - h * juce::jlimit (0.0f, 1.0f, (db - kBotDb) / (kTopDb - kBotDb)); };

    g.setFont (monoFont (8.0f));
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = plot.getX() + xForFrequency (hz, w);
        g.setColour (kGrid.withAlpha (0.5f));
        g.drawLine (x, plot.getY(), x, plot.getBottom(), 1.0f);
        g.setColour (kDim.withAlpha (0.6f));
        g.drawText (hz >= 1000.0f ? juce::String ((int) (hz / 1000)) + "k" : juce::String ((int) hz),
                    (int) x - 14, (int) plot.getBottom() - 11, 28, 10, juce::Justification::centred);
    }
    for (float db = -84.0f; db < kTopDb; db += 12.0f)
    {
        const float y = yFor (db);
        g.setColour (kGrid.withAlpha (0.35f));
        g.drawLine (plot.getX(), y, plot.getRight(), y, 1.0f);
    }

    if (mHaveFrame)
    {
        const float sr    = (float) juce::jmax (1.0, mProc.getSampleRate());
        const float binHz = sr / (float) kFftSize;
        const float tiltDbPerOct = mTilt == 0 ? 0.0f : mTilt == 1 ? 3.0f : 4.5f;
        auto tilt = [&] (float hz) { return tiltDbPerOct * std::log2 (juce::jmax (hz, 1.0f) / 1000.0f); };
        const float logLo = std::log10 (kMinHz), logHi = std::log10 (kMaxHz);

        if (mThirdOctave)
        {
            // 1/3-octave bands, 20 Hz .. 20 kHz: the energy in each band as a bar.
            for (float lo = 20.0f; lo < kMaxHz; lo *= std::cbrt (2.0f))
            {
                const float hiHz = lo * std::cbrt (2.0f);
                const int k0 = juce::jmax (1, (int) std::floor (lo / binHz));
                const int k1 = juce::jmin (kNumBins - 1, (int) std::ceil (hiHz / binHz));
                float sum = 0.0f; int n = 0;
                for (int k = k0; k <= k1; ++k) { sum += std::pow (10.0f, mAvgDb[(size_t) k] * 0.1f); ++n; }
                if (n == 0) continue;
                const float db = 10.0f * std::log10 (juce::jmax (sum / (float) n, 1.0e-12f)) + tilt (std::sqrt (lo * hiHz));
                const float xa = plot.getX() + xForFrequency (lo, w), xb = plot.getX() + xForFrequency (hiHz, w);
                g.setColour (kCyan.withAlpha (0.55f));
                g.fillRect (juce::Rectangle<float> (xa + 1.0f, yFor (db), juce::jmax (1.0f, xb - xa - 2.0f), plot.getBottom() - yFor (db)));
            }
        }
        else
        {
            // One column per pixel: average for the trace, max for the hold.
            // Above a few kHz one pixel spans dozens of bins and the loudest is
            // always an outlier; below 500 Hz a bin spans many pixels, so
            // interpolate between neighbours rather than stair-stepping.
            const int px = juce::jmax (2, (int) w);
            juce::Path curve, hold;
            for (int i = 0; i < px; ++i)
            {
                const float f0 = std::pow (10.0f, logLo + (logHi - logLo) * (float) i / (float) px);
                const float f1 = std::pow (10.0f, logLo + (logHi - logLo) * (float) (i + 1) / (float) px);
                const float pos0 = f0 / binHz, pos1 = f1 / binHz;
                const int k0 = (int) std::floor (pos0), k1 = (int) std::floor (pos1);
                float m, hd;
                if (k1 > k0)
                {
                    float sum = 0.0f, mx = -140.0f; int count = 0;
                    for (int k = juce::jmax (1, k0); k <= juce::jmin (kNumBins - 1, k1); ++k)
                    { sum += mAvgDb[(size_t) k]; mx = juce::jmax (mx, mPeakDb[(size_t) k]); ++count; }
                    m  = count > 0 ? sum / (float) count : -140.0f;
                    hd = mx;
                }
                else
                {
                    const float center = 0.5f * (pos0 + pos1);
                    const int ka = juce::jlimit (1, kNumBins - 2, (int) center);
                    const float frac = juce::jlimit (0.0f, 1.0f, center - (float) ka);
                    m  = mAvgDb[(size_t) ka]  + (mAvgDb[(size_t) (ka + 1)]  - mAvgDb[(size_t) ka])  * frac;
                    hd = mPeakDb[(size_t) ka] + (mPeakDb[(size_t) (ka + 1)] - mPeakDb[(size_t) ka]) * frac;
                }
                const float tt = tilt (std::sqrt (f0 * f1));
                const float x = plot.getX() + (float) i;
                if (i == 0) { curve.startNewSubPath (x, yFor (m + tt)); hold.startNewSubPath (x, yFor (hd + tt)); }
                else        { curve.lineTo (x, yFor (m + tt));          hold.lineTo (x, yFor (hd + tt)); }
            }
            auto filled = curve;
            filled.lineTo (plot.getRight(), plot.getBottom());
            filled.lineTo (plot.getX(), plot.getBottom());
            filled.closeSubPath();
            g.setColour (kCyan.withAlpha (0.30f));
            g.fillPath (filled);
            g.setColour (kCyan);
            g.strokePath (curve, juce::PathStrokeType (1.2f));
            g.setColour (kText.withAlpha (0.45f));
            g.strokePath (hold, juce::PathStrokeType (1.0f));
        }
    }

    g.setColour (kEdge);
    g.drawRoundedRectangle (plot, 3.0f, 1.0f);

    // Per channel, because a peak on one side is a different problem from a
    // peak on both, and Reset clears them.
    const float tpL = mProc.getMasterTruePeakMaxDbChannel (0), tpR = mProc.getMasterTruePeakMaxDbChannel (1);
    g.setFont (monoFont (9.0f, true));
    g.setColour (kDim);
    g.drawText ("MAX TP", (int) plot.getX() + 7, (int) plot.getY() + 4, 48, 11, juce::Justification::centredLeft);
    g.setColour (tpL > mCeilingDb ? kRed : kText);
    g.drawText ("L " + fmt (tpL), (int) plot.getX() + 54, (int) plot.getY() + 4, 62, 11, juce::Justification::centredLeft);
    g.setColour (tpR > mCeilingDb ? kRed : kText);
    g.drawText ("R " + fmt (tpR), (int) plot.getX() + 112, (int) plot.getY() + 4, 62, 11, juce::Justification::centredLeft);

    // The meter itself is the DBFSMeter child (resized() places it); only its label is ours.
    g.setColour (kDim);
    g.setFont (uiFont (8.0f));
    g.drawText ("dBFS", side.reduced (4.0f, 0.0f).removeFromTop (11.0f), juce::Justification::centred);
}
