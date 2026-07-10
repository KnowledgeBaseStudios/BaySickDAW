#pragma once
#include <JuceHeader.h>
#include "../DSP/BaySickPitchDSP.h"
#include <map>

class BaySickVocalProcessor;

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPitchEditor - QA-Fa Task 2 full redesign (2026-07-10, sections
// 14a-14g of the phantom-recording-mongoose running notes)
// ─────────────────────────────────────────────────────────────────────────────
// Composite-driven note-level pitch editor (the BaySickAlign sibling), hosted
// in BaySickVocalEditor's "BaySickPitch" sub-tab.  The channel composite
// auto-resolves from the grid clips (no manual load -- Call 4a killed
// drag-drop import); analysis segments note regions which paint as PILLS
// (Effects-purple fill, Vox-teal waveform interior) over the Bass-green
// detected pitch curve.  Every control is APVTS-attached (bsp_*).
//
//   - Toolbar: BaySickPitch title, LENGTH readout (X bars / M:SS.f, SEL
//     variant), 3-mode preset combo + Save/Load + dirty dot, Slice/Edit
//     mode buttons, Reset, Render, "Send Notes to..." popup, Undo/Redo,
//     Auto-Scroll (A), stale RE-ANALYZE badge, Focus/Mod/Speed knobs.
//   - Canvas: MIDI keyboard (left) + bar/beat ruler + grid + pills +
//     pitch curve + playhead (FilePlay position) + per-note sub-curves
//     (Vibrato / Formant / Volume) under the selected pill.
//   - InfoBar: monospace Pitch / Cents / Length at hover/selection.
//
// Edits live on BaySickVocalProcessor::mPitch (BaySickPitchDSP) and flow to
// playback through the realtime applicator -- no bake required; Render is
// the opt-in bake to <project>/Pitched/.
// ─────────────────────────────────────────────────────────────────────────────

class BaySickPitchEditor : public juce::Component,
                           private juce::Timer
{
public:
    explicit BaySickPitchEditor (BaySickVocalProcessor& p);
    ~BaySickPitchEditor() override;

    void paint   (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed (const juce::KeyPress& k) override;

    // Shared view state (seconds domain, composite-relative).
    double pixelsPerSecond() const noexcept { return mPps; }
    double scrollSeconds()   const noexcept { return mScroll; }
    int    topNote()         const noexcept { return mTopNote; }
    double noteLaneH()       const noexcept { return mLaneH; }
    void   setView (double pps, double scrollSec);
    void   setTopNote (int note);

private:
    class Toolbar;
    class PitchKeyboard;
    class PitchCanvas;
    class InfoBar;

    void timerCallback() override;

    void updateInfoBarFor (int regionIdx);
    void runAnalyzeIfNeeded (bool force);
    void runRender();
    void runReset();
    void refreshComposite();          // waveform cache for the pills
    void showSendNotesMenu();
    // QA-Fa recovery: version-history restore points (bundle item 3).
    void runSnapshotVersion();
    void showVersionsMenu();

    // Presets (section 14f: 3 mode presets drive the knobs)
    void applyFactoryPreset (int idx);   // 0 Loose / 1 Close / 2 Tight
    void snapshotPresetValues();
    bool paramsDivergeFromSnapshot() const;
    void saveUserPreset();
    void loadUserPreset();

    // Local edit undo (region edits + slices)
    void pushUndo();
    void doUndo();
    void doRedo();

    float paramValue (const char* id, float fallback = 0.0f) const;
    void  setParamValue (const char* id, float v);

    BaySickVocalProcessor& mProc;

    std::unique_ptr<Toolbar>       mToolbar;
    std::unique_ptr<PitchKeyboard> mKeyboard;
    std::unique_ptr<PitchCanvas>   mCanvas;
    std::unique_ptr<InfoBar>       mInfoBar;

    // Composite waveform cache (mono, analysis SR) for pill interiors.
    juce::AudioBuffer<float> mCompCache;
    double                   mCompSr { 44100.0 };
    bool                     mCompValid { false };

    // View state
    double mPps     { 100.0 };   // pixels per second
    double mScroll  { 0.0 };     // seconds at the canvas left edge
    int    mTopNote { 84 };      // top visible MIDI note
    double mLaneH   { 10.0 };    // pixels per semitone lane

    int  mSelectedRegion { -1 };
    bool mAutoScroll     { true };

    // Playhead (FilePlay position; hidden when it stops advancing)
    juce::int64 mLastPlaySample  { -1 };
    juce::uint32 mLastPlayMoveMs { 0 };

    std::vector<std::vector<PitchNoteRegion>> mUndoStack, mRedoStack;
    std::map<juce::String, float> mPresetSnapshot;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickPitchEditor)
};
