#pragma once
#include <JuceHeader.h>

class BaySickVocalProcessor;

// ─────────────────────────────────────────────────────────────────────────────
// BaySickAlignEditor - H-6c (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// Full-page VocAlign-clone offline alignment editor.  Lives inside the
// BaySickVocalEditor's "BaySickAlign" sub-tab.  Three-lane layout
// matching VocAlign 6 Pro:
//
//   - Toolbar (~36 px): logo + preset selector + modified-indicator +
//                       undo/redo + settings + help
//   - GUIDE lane (yellow waveform) -- captured/loaded from a sidechain
//                                     source picker
//   - SYNC POINTS strip (between Guide + Dub)
//   - DUB lane (orange waveform) -- this Vox channel's recording
//   - PROTECTED AREAS strip (between Dub + Output)
//   - OUTPUT lane (purple waveform) -- shows the alignment result
//   - View-mode buttons (Waveform / Pitch / Energy) at the lane footer
//   - Right edge: 3 panel tabs (clock = Match Timing, wavy = Match Pitch,
//                 sliders = Other) -- open panels to the left
//   - Render-history scrubber strip at the bottom (mini lanes for prior
//                                                  renders + zoom + delete)
//
// Source model (locked):
//   - Dub source = THIS Vox strip's most recent dry G-9 recording.
//   - Guide source = picker over channels currently sidechained INTO
//                    this Vox channel.  User configures sidechain via
//                    the existing routing UI; the picker just lists
//                    those active sources.
//   - Recordings auto-load by default; Capture buttons re-pull current
//                    state if the user re-recorded after opening.
//
// Algorithm (locked):
//   - Cannot run real-time -- needs full Guide + full Dub before
//     computing the warp path.  Algorithm fully built in H-6a
//     (BaySickAlignDSP) + H-4 (YIN) + H-5 (granular shifter) +
//     existing PhaseVocoder for time-stretch.  H-6c is UI only.
//
// Render lifecycle (locked):
//   - Auto-preview Output lane updates on parameter change
//     (lower-quality fast render).  Toolbar toggle to disable.
//   - Manual Render writes Aligned/{name}_align_v{N}.wav and replaces
//     the position on the Builder; prior version stays in the browser
//     panel.  Both can coexist on Builder via manual drag.
//
// H-6c ships the empty-state shell -- all UI elements paint correctly
// and all interaction handlers are stubbed against empty data models.
// G-9 wires the recording loaders + sidechain picker enumeration.
// ─────────────────────────────────────────────────────────────────────────────

class BaySickAlignEditor : public juce::Component
{
public:
    explicit BaySickAlignEditor (BaySickVocalProcessor& p);
    ~BaySickAlignEditor() override;

    void paint   (juce::Graphics&) override;
    void resized() override;

    bool hasGuide() const noexcept { return mHasGuide; }
    bool hasDub()   const noexcept { return mHasDub; }
    bool hasOutput() const noexcept { return mHasOutput; }

    // Shared horizontal scroll/zoom state across all 3 lanes + sync/protected
    // strips + render-history scrubber.  G-9 wires up mutators.
    double pixelsPerSecond() const noexcept { return mPpsX; }
    double scrollSeconds()   const noexcept { return mScrollX; }

    // View modes (matches VocAlign's bottom-left button row)
    enum class ViewMode { Waveform, PitchProfile, EnergyProfile };
    ViewMode viewMode() const noexcept { return mViewMode; }
    void setViewMode (ViewMode m);

    // Right-edge panel toggle -- multiple panels can be open simultaneously
    // (matches VocAlign's "stacked panel" behaviour).
    enum class SidePanel { Timing = 0, Pitch = 1, Other = 2 };
    bool isSidePanelOpen (SidePanel p) const noexcept;
    void toggleSidePanel (SidePanel p);

private:
    class Toolbar;
    class LaneHeader;          // left-side label + Capture/Render button + settings icons
    class WaveformLane;        // Guide / Dub / Output waveform display
    class SyncPointsStrip;     // between Guide + Dub
    class ProtectedAreasStrip; // between Dub + Output
    class ViewModeBar;         // 3 buttons + heat-map strip below the Output lane
    class HistoryScrubber;     // bottom navigator -- prior renders as mini lanes
    class MatchTimingPanel;    // right-side panel
    class MatchPitchPanel;     // right-side panel
    class OtherPanel;          // right-side panel
    class SidePanelTabs;       // 3 vertical icon buttons on the far right edge

    BaySickVocalProcessor& mProc;

    std::unique_ptr<Toolbar>             mToolbar;
    std::unique_ptr<LaneHeader>          mGuideHeader;
    std::unique_ptr<WaveformLane>        mGuideLane;
    std::unique_ptr<SyncPointsStrip>     mSyncStrip;
    std::unique_ptr<LaneHeader>          mDubHeader;
    std::unique_ptr<WaveformLane>        mDubLane;
    std::unique_ptr<ProtectedAreasStrip> mProtectedStrip;
    std::unique_ptr<LaneHeader>          mOutputHeader;
    std::unique_ptr<WaveformLane>        mOutputLane;
    std::unique_ptr<ViewModeBar>         mViewModeBar;
    std::unique_ptr<HistoryScrubber>     mHistoryScrubber;

    std::unique_ptr<MatchTimingPanel>    mTimingPanel;
    std::unique_ptr<MatchPitchPanel>     mPitchPanel;
    std::unique_ptr<OtherPanel>          mOtherPanel;
    std::unique_ptr<SidePanelTabs>       mSidePanelTabs;

    // Shared state
    double mPpsX    { 80.0 };
    double mScrollX { 0.0 };
    ViewMode mViewMode { ViewMode::Waveform };

    bool mSidePanelOpen[3] { true, true, false };  // Timing + Pitch open by default

    // Empty-state flags -- H-6c always false; G-9 toggles when load succeeds.
    bool mHasGuide  { false };
    bool mHasDub    { false };
    bool mHasOutput { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickAlignEditor)
};
