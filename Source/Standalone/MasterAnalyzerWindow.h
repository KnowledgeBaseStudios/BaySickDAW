#pragma once
#include <JuceHeader.h>
#include "../DSP/SpectrumFeed.h"
#include "../DSP/LoudnessSpec.h"
#include "VersionCapture.h"
#include "SharedUI.h"   // DBFSMeter: the master strip's meter, reused as-is
#include <array>
#include <deque>
#include <vector>

class BaySickDAWProcessor;

// -- MasterAnalyzerView -- CL-044 (QA-ModelShell TS7), rebuilt QA-TrueLevel
// SC-16 (Jeff, 2026-08-22) to the KBS Meter Suite blueprint -----------------
// Three views, because the three questions are different shapes: how loud is
// it RIGHT NOW is a number (Levels); whether it stayed there is a line over
// time (Loudness); what it is made of is a spectrum (Spectrum).
//
// Additions over the plugin panel, which the app can make and a plugin cannot:
// the history is the WHOLE take and zooms (wheel) / pans (drag), the loudness
// range band is shaded on it, PLR / PSR are read out, the spectrum has a
// slope tilt and a 1/3-octave mode, a second take overlays for A/B, moments
// over the ceiling are counted, and the integrated verdict is graded against
// the spec chosen in the export dialog.
//
// Taps the processor's post-fader master feed (seqlock, wait-free on the audio
// thread).  The tap and the timer follow the peer (parentHierarchyChanged), so
// a closed window costs nothing.  60 Hz body-only repaint: the meters are the
// one thing in the app that moves continuously, and 30 is where a falling peak
// reads as steps.
// -----------------------------------------------------------------------------
class MasterAnalyzerView : public juce::Component,
                           public juce::SettableTooltipClient,
                           private juce::Timer
{
public:
    enum class ViewMode { Levels, Loudness, Spectrum };

    explicit MasterAnalyzerView (BaySickDAWProcessor& proc);
    ~MasterAnalyzerView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    // ── TS7 §3.6: source picker (Live + every captured take) ─────────────────
    void setVersionSource (VersionCapture* vc);
    void refreshVersions();
    void selectSource (int takeId);
    int  getSelectedTakeId() const noexcept { return mSelectedTakeId; }
    std::function<void(const VersionCapture::Version&)> onExportTake;
    std::function<void(int /*takeId*/)>                 onRemoveTake;   // QA-TrueLevel SC-14

    void setViewMode (ViewMode m);
    ViewMode getViewMode() const noexcept { return mView; }

    // The scale the panel draws against: target / ceiling from the spec the
    // user works to (the export dialog's pick), never a compiled-in constant.
    void  setTargetLufs (float lufs)      { mTargetLufs = lufs; repaint(); }
    float getTargetLufs() const noexcept  { return mTargetLufs; }
    void  setCeilingDb (float dbtp)       { mCeilingDb = dbtp; repaint(); }
    float getCeilingDb() const noexcept   { return mCeilingDb; }

    // Clears everything that accumulates: history, both true-peak maxima, the
    // over-ceiling count and the spectrum's peak-hold trace.
    void resetHistory();

    // A measured render / a take: the curves stand in for the live history and
    // the summary numbers for the live meters.  momentary may be empty (older
    // reports), in which case only the short-term line draws.
    void showRenderedCurve (const std::vector<float>& lufsCurve,
                            float integrated, float lra, float truePeakDb,
                            const juce::String& label,
                            const std::vector<float>& momentaryCurve = {},
                            int overCeilingCount = -1);
    void showLive();
    bool isShowingRender() const noexcept { return mShowingRender; }

    static constexpr int kFftOrder = 13;
    static constexpr int kFftSize  = 1 << kFftOrder;   // 8192
    static constexpr int kNumBins  = kFftSize / 2;

private:
    void timerCallback() override;
    void setTapActive (bool on);
    void applySelection();
    void rebuildCompareList();

    float xForFrequency (float hz, float w) const;
    juce::Colour verdictColour (float lufs) const;

    void paintLevels   (juce::Graphics&, juce::Rectangle<float>);
    void paintLoudness (juce::Graphics&, juce::Rectangle<float>);
    void paintSpectrum (juce::Graphics&, juce::Rectangle<float>);
    void loudnessRow   (juce::Graphics&, juce::Rectangle<float>, const char* label, float lufs);
    void paintPeaks    (juce::Graphics&, juce::Rectangle<float>);
    void paintCorrelation (juce::Graphics&, juce::Rectangle<float>);

    // The curves the Loudness view draws: live rings or the shown take.
    const std::vector<float>& shortTermSeries() const noexcept;
    const std::vector<float>& momentarySeries() const noexcept;
    juce::Rectangle<float> plotBounds() const;
    juce::Rectangle<float> bodyBounds() const;

    BaySickDAWProcessor& mProc;

    // ── Spectrum machinery ───────────────────────────────────────────────────
    juce::dsp::FFT                      mFft;
    juce::dsp::WindowingFunction<float> mWindow;
    std::vector<float> mScratch;    // 2 * kFftSize, FFT works in place
    std::vector<float> mFeedBuf;    // SpectrumFeed::kSize
    std::vector<float> mAccum;      // kFftSize rolling input ring
    int                mAccumFill { 0 };
    std::array<float, kNumBins> mAvgDb  { };
    std::array<float, kNumBins> mPeakDb { };
    bool mHaveFrame { false };
    int  mTilt       { 0 };       // 0 / 1 / 2 -> 0, 3, 4.5 dB per octave
    bool mThirdOctave { false };

    // ── Loudness history (live) ──────────────────────────────────────────────
    std::vector<float> mLiveShort, mLiveMom;   // kHistoryHz, whole session ring
    int   mOverCeilingLive { 0 };
    float mLastLiveTp { -144.0f };

    // ── Shown take / render ──────────────────────────────────────────────────
    std::vector<float> mRenderCurve, mRenderMom;
    bool         mShowingRender { false };
    juce::String mRenderLabel;
    float mRenderIntegrated { -120.0f };
    float mRenderLra        { 0.0f };
    float mRenderTruePeak   { -144.0f };
    int   mRenderOverCeiling { 0 };

    // ── A/B overlay ──────────────────────────────────────────────────────────
    std::vector<float> mCompareCurve;
    juce::String       mCompareLabel;

    // ── Zoom / pan / hover on the history (seconds) ──────────────────────────
    double mViewX0 { 0.0 }, mViewX1 { 0.0 };   // 0,0 = "show everything"
    double mDragStartX0 { 0.0 }, mDragStartX1 { 0.0 };
    int    mDragStartPx { 0 };
    float  mHoverX { -1.0f };

    ViewMode mView       { ViewMode::Loudness };
    float    mTargetLufs { -14.0f };
    float    mCeilingDb  { -1.0f };
    bool     mTapActive  { false };

    VersionCapture*                   mVersions { nullptr };
    std::unique_ptr<juce::ComboBox>   mSourceBox;
    std::unique_ptr<juce::ComboBox>   mCompareBox;
    std::unique_ptr<juce::TextButton> mExportBtn;
    std::unique_ptr<juce::TextButton> mRemoveBtn;   // QA-TrueLevel SC-14
    std::vector<std::unique_ptr<juce::TextButton>> mViewButtons;
    std::unique_ptr<juce::TextButton> mResetBtn, mTiltBtn, mOctBtn;
    // SC-16: the dBFS meter beside the spectrum is the SAME component the
    // master strip draws, with the same ballistics, fed from the analyzer's own
    // peak window (the strip's window is emptied by the mixer's drain).
    std::unique_ptr<DBFSMeter>        mDbfsMeter;
    int  mSelectedTakeId { 0 };        // 0 = Live
    bool mRebuildingList { false };    // suppresses onChange during repopulate

    static constexpr float kControlsH = 26.0f;
    static constexpr float kViewBarH  = 24.0f;
    static constexpr float kMinDb        = -96.0f;
    static constexpr float kMaxDb        =   6.0f;
    static constexpr int   kTimerHz      =  60;
    static constexpr int   kHistoryHz    =  10;
    static constexpr int   kHistorySecs  = 1800;   // 30 min of whole-session history
    static constexpr int   kMaxHistory   = kHistoryHz * kHistorySecs;
    static constexpr float kAvgAlpha     = 0.35f;
    static constexpr float kPeakDecayDbPerSec = 12.0f;
    static constexpr float kMinHz        =    20.0f;
    static constexpr float kMaxHz        = 20000.0f;
    int mTickCounter { 0 };   // divides kTimerHz down to kHistoryHz

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterAnalyzerView)
};
