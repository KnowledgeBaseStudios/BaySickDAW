#pragma once
#include <JuceHeader.h>
#include "../DSP/BaySickAlignDSP.h"
#include <map>

class BaySickVocalProcessor;

// ─────────────────────────────────────────────────────────────────────────────
// BaySickAlignEditor - QA-F Task 4 full redesign (2026-07-09, sections
// 13a-13g of the phantom-recording-mongoose running notes)
// ─────────────────────────────────────────────────────────────────────────────
// Offline channel-pair alignment editor, hosted in BaySickVocalEditor's
// "BaySickAlign" sub-tab.  Every control is APVTS-attached (bsa_*) -- no
// paint-only widgets.
//
//   - Toolbar: BaySickAlign title, 6-entry preset combo (Loose/Close/Tight
//     x +/-Pitch), Save/Load preset, preset-dirty green dot, Analyze +
//     Render actions, Undo/Redo (local align-edit stack), stale badge.
//   - Shared time ruler above the 3-lane stack.
//   - Lanes: Leader (Bass green) / Follower (Vox teal) / Output (Drums red),
//     channel pickers on the Leader + Follower headers, composite waveforms
//     from VibeSynthProcessor::renderChannelComposite (via processor hooks).
//   - Sync Points strip (between Leader + Follower): click to place, drag
//     either side's handle, right-click delete, "Automatic Sync Points" seed.
//   - Protected Areas strip (between Follower + Output): drag-create,
//     right-click toggles time/pitch protection dimensions.
//   - ViewModeBar: Wave / Pitch (frame-YIN F0 contour) / Energy (RMS).
//   - HistoryScrubber: render-version list (version + date) + Del / + / -.
//   - Right panel, always visible, two boxes:
//       Align: ON, Mode (Loose/Close/Tight), Fine Tune (bipolar +/-50 ms
//              around the Mode base 150/100/50).
//       Pitch: ON, Range (Mode-driven center + travel), Algos (PSOLA /
//              Granular / Phase Vocoder), Transpose, Formant Shift ON +
//              rotary.
//
// All processing is offline/message-thread (the G2-warp design lock): the
// editor drives BaySickVocalProcessor::analyzeAlign / renderAlignedTake /
// renderAlignedPreview and paints the results.
// ─────────────────────────────────────────────────────────────────────────────

class BaySickAlignEditor : public juce::Component,
                           private juce::Timer
{
public:
    explicit BaySickAlignEditor (BaySickVocalProcessor& p);
    ~BaySickAlignEditor() override;

    void paint   (juce::Graphics&) override;
    void resized() override;

    enum class ViewMode { Wave = 0, Pitch = 1, Energy = 2 };

    // Shared horizontal view state (seconds domain, common-origin frame).
    double pixelsPerSecond() const noexcept { return mPps; }
    double scrollSeconds()   const noexcept { return mScroll; }
    void   setView (double pps, double scrollSec);

private:
    // Per-lane composite cache (common-origin seconds domain).
    struct LaneCache
    {
        juce::AudioBuffer<float> audio;      // mono, front-padded to origin
        double                   sampleRate  { 44100.0 };
        double                   durationSec { 0.0 };
        std::vector<float>       f0Hz;       // lazy (Pitch view), 2048 hop
        std::vector<float>       rms;        // lazy (Energy view), 512 hop
        bool                     valid       { false };
        void clear() { audio.setSize (0, 0); f0Hz.clear(); rms.clear();
                       valid = false; durationSec = 0.0; }
    };

    class Ruler;
    class LaneView;
    class SyncStrip;
    class ProtectedStrip;
    class ViewModeBar;
    class HistoryScrubber;
    class RightPanel;
    class Toolbar;

    void timerCallback() override;

    // Actions
    void runAnalyze();
    void runRender();
    void refreshComposites();     // Leader + Follower caches (cheap-ish)
    void refreshOutputPreview();  // Output cache (expensive: full warp render)
    void rebuildChannelCombos();

    // Presets (section 13a)
    void applyFactoryPreset (int presetIdx);   // 0..5
    void snapshotPresetValues();
    bool paramsDivergeFromSnapshot() const;
    void saveUserPreset();
    void loadUserPreset();

    // Local align-edit undo (sync points / protected areas / map)
    struct EditSnapshot
    {
        WarpMap                         map;
        std::vector<AlignSyncPoint>     syncPoints;
        std::vector<AlignProtectedArea> protectedAreas;
        bool                            analyzed { false };
    };
    EditSnapshot captureEdits() const;
    void restoreEdits (const EditSnapshot& s);
    void pushUndo();
    void doUndo();
    void doRedo();

    float paramValue (const char* id, float fallback = 0.0f) const;
    void  setParamValue (const char* id, float v);

    BaySickVocalProcessor& mProc;

    std::unique_ptr<Toolbar>         mToolbar;
    std::unique_ptr<Ruler>           mRuler;
    std::unique_ptr<LaneView>        mLeaderLane;
    std::unique_ptr<SyncStrip>       mSyncStrip;
    std::unique_ptr<LaneView>        mFollowerLane;
    std::unique_ptr<ProtectedStrip>  mProtectedStrip;
    std::unique_ptr<LaneView>        mOutputLane;
    std::unique_ptr<ViewModeBar>     mViewModeBar;
    std::unique_ptr<HistoryScrubber> mHistory;
    std::unique_ptr<RightPanel>      mRightPanel;

    LaneCache mLeaderCache, mFollowerCache, mOutputCache;

    double   mPps    { 60.0 };   // pixels per second
    double   mScroll { 0.0 };    // seconds
    ViewMode mViewMode { ViewMode::Wave };

    std::vector<EditSnapshot> mUndoStack, mRedoStack;
    std::map<juce::String, float> mPresetSnapshot;   // dirty-dot reference
    bool mEditsSinceAnalyze { false };               // sync/protected edited
    int  mSlowTick { 0 };                            // channel-list refresh divider

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickAlignEditor)
};
