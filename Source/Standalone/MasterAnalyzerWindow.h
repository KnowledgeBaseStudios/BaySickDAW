#pragma once
#include <JuceHeader.h>
#include "../DSP/SpectrumFeed.h"
#include "../DSP/LoudnessSpec.h"
#include "VersionCapture.h"
#include <array>
#include <deque>
#include <vector>

class VibeSynthProcessor;

// ── MasterAnalyzerView — CL-044 + CL-227 (QA-ModelShell TS7) ─────────────────
// The master analyzer AND the loudness-report face are ONE window, not two
// features (Jeff, 2026-07-29).  It carries two VIEWS over two SOURCES:
//
//   View   : Loudness (a Youlean-style LUFS-over-time graph) or Spectrum.
//   Source : Live (the running master output) or a captured/rendered version.
//
// Readouts are always on: Momentary / Short-Term / Integrated LUFS, LRA, output
// dBFS peak, and true peak.  The target line drawn across the loudness graph is the
// user's own LUFS target -- moments above it are the flagged instances, and the
// integrated full-song average is the headline number.  No published spec
// defines a moment-level loudness limit, so the user's target IS the bar.
//
// Tap point: post fader / pan / width, the same place the master LUFS meter
// reads, so the analyzer shows what actually leaves the app and the master fader
// moves the trace.
//
// COST DISCIPLINE, two independent gates: the audio-side push sits behind an
// atomic flag (closed window = one relaxed load per block, no copy), and the flag
// is driven from parentHierarchyChanged() keyed on getPeer() != nullptr -- the
// peer-keyed suspend convention the shell uses for MixerPage's vblank, the
// Effects poll and the Builder animation.
// ─────────────────────────────────────────────────────────────────────────────
class MasterAnalyzerView : public juce::Component,
                           public juce::SettableTooltipClient,
                           private juce::Timer
{
public:
    enum class ViewMode { Loudness, Spectrum };

    explicit MasterAnalyzerView (VibeSynthProcessor& proc);
    ~MasterAnalyzerView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

    // ── TS7 §3.6: the version list ───────────────────────────────────────────
    // The window does not OWN capture -- capture runs for the whole session
    // whether this window exists or not -- so it is handed a pointer to read.
    // Null is normal (no capture wired yet) and must stay harmless.
    void setVersionSource (VersionCapture* vc);
    void refreshVersions();
    // THE one way to change source, so the title-strip menu and the in-view combo
    // cannot disagree.  0 = Live.  Previously the menu called showLive() directly,
    // which left the combo displaying a take name over a live trace and let the
    // next refreshVersions() re-apply the stale take.
    void selectSource (int takeId);
    int  getSelectedTakeId() const noexcept { return mSelectedTakeId; }
    // Export the selected take's audio.  Wired by the editor because a file
    // chooser is not this component's business.
    std::function<void(const VersionCapture::Version&)> onExportTake;

    // Driven from the window's title-strip menu.
    void setViewMode (ViewMode m)      { mView = m; repaint(); }
    ViewMode getViewMode() const noexcept { return mView; }
    void setTargetLufs (float lufs)    { mTargetLufs = lufs; repaint(); }
    float getTargetLufs() const noexcept { return mTargetLufs; }
    void resetHistory();

    // A rendered result shown in place of the live trace (CL-227's face).  An
    // empty curve returns the view to Live.
    void showRenderedCurve (const std::vector<float>& lufsCurve,
                            float integrated, float lra, float truePeakDb,
                            const juce::String& label);
    void showLive();
    bool isShowingRender() const noexcept { return mShowingRender; }

    // ~5.4 Hz bins at 44.1k.  The feed pushes 1024 at a time and this
    // accumulates 8 of them, because a 1024-point FFT cannot produce a bin below
    // ~43 Hz -- which left the bottom octave of the display permanently empty.
    static constexpr int kFftOrder = 13;
    static constexpr int kFftSize  = 1 << kFftOrder;   // 8192
    static constexpr int kNumBins  = kFftSize / 2;

private:
    void timerCallback() override;
    void setTapActive (bool on);
    float xForFrequency (float hz, float w) const;
    void paintLoudness (juce::Graphics&, juce::Rectangle<float>);
    void paintSpectrum (juce::Graphics&, juce::Rectangle<float>);
    void paintReadouts (juce::Graphics&, juce::Rectangle<float>);

    VibeSynthProcessor& mProc;

    // juce::dsp::FFT is not default-constructible -- must be in the ctor init
    // list (CLAUDE.md gotcha).
    juce::dsp::FFT                      mFft;
    juce::dsp::WindowingFunction<float> mWindow;

    std::vector<float> mScratch;    // 2 * kFftSize, FFT works in place
    std::vector<float> mFeedBuf;    // SpectrumFeed::kSize
    std::vector<float> mAccum;      // kFftSize rolling input ring
    int                mAccumFill { 0 };

    std::array<float, kNumBins> mAvgDb  { };
    std::array<float, kNumBins> mPeakDb { };
    bool mHaveFrame { false };

    // Loudness history, sampled at kHistoryHz.  Deque rather than a ring because
    // a render's curve replaces it wholesale and lengths differ.
    std::deque<float> mLufsHistory;
    std::vector<float> mRenderCurve;
    bool         mShowingRender { false };
    juce::String mRenderLabel;
    float mRenderIntegrated { -120.0f };
    float mRenderLra        { 0.0f };
    float mRenderTruePeak   { -144.0f };

    ViewMode mView       { ViewMode::Loudness };
    float    mTargetLufs { -14.0f };
    bool     mTapActive  { false };

    // TS7 §3.6.  mSelectedTakeId is held separately from the combo's index
    // because the list is rebuilt whenever a take is added, and an index would
    // silently point at a different take afterwards.
    VersionCapture*                   mVersions { nullptr };
    std::unique_ptr<juce::ComboBox>   mSourceBox;
    std::unique_ptr<juce::TextButton> mExportBtn;
    int  mSelectedTakeId { 0 };        // 0 = Live
    bool mRebuildingList { false };    // suppresses onChange during repopulate
    void applySelection();
    static constexpr float kControlsH = 26.0f;

    // CALIBRATION (Rule 6 cat. 5).  Spectrum display range -96..+6 dB covers a
    // mastered mix's floor through an overshoot.  The loudness graph runs -40..0
    // LUFS, which spans every target in LoudnessSpec (-14 through ATSC's -24)
    // with room to read a quiet passage.  History at 10 Hz over 120 s matches
    // EBU Tech 3342's sampling rate and gives a readable two-minute window.
    static constexpr float kMinDb        = -96.0f;
    static constexpr float kMaxDb        =   6.0f;
    static constexpr float kLufsMin      = -40.0f;
    static constexpr float kLufsMax      =   0.0f;
    static constexpr int   kTimerHz      =  30;
    static constexpr int   kHistoryHz    =  10;
    static constexpr int   kHistorySecs  = 120;
    static constexpr int   kMaxHistory   = kHistoryHz * kHistorySecs;
    static constexpr float kAvgAlpha     = 0.35f;
    static constexpr float kPeakDecayDbPerSec = 12.0f;
    static constexpr float kMinHz        =    20.0f;
    static constexpr float kMaxHz        = 20000.0f;

    int mTickCounter { 0 };   // divides kTimerHz down to kHistoryHz

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterAnalyzerView)
};
