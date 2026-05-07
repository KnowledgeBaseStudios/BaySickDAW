#pragma once
#include <JuceHeader.h>

class BaySickVocalProcessor;

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPitchEditor - H-6b (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// Full-page Newtone-clone offline pitch editor.  Layout (top to bottom):
//
//   - Toolbar (~70 px): playback + edit-mode buttons + the 3 global knobs
//                       (CENTER / VARIATION / TRANS) + render + save
//   - Bar/beat ruler (~16 px)
//   - Pitch keyboard (left ~40 px) + Pitch grid (rest):
//                       Y axis = semitones, X axis = time, detected pitch
//                       curve drawn through the canvas, per-note "blobs"
//                       with the waveform drawn INSIDE each blob.
//   - Horizontal scroll bar (~12 px)
//   - Info bar (~22 px): selected note pitch / cents offset / length /
//                        loop region readout
//
// Source = the dry G-9 recording associated with the owning Vox strip.
// G-9 hasn't shipped yet, so H-6b ships the editor in empty-state -- the
// canvas paints the grid + keyboard but renders an empty-state message
// centered until audio is loaded.
//
// Edit lifecycle (Option C - locked in chat):
//   - Edits stored as ValueTree on BaySickVocalProcessor.
//   - Render writes Pitched/{name}_pitch_v{N}.wav and replaces the
//     position on the Builder page; prior version stays in the browser
//     panel.  Both can coexist on Builder via manual drag (manual mute
//     handles A/B in song mode).
//
// Internal transport: BaySickPitch has its own Play/Stop/Pause/Loop
// scoped to the editor (independent of the global app transport).
// Slaved Playback Mode (H key) makes the editor's transport follow the
// global app transport when toggled on.  Auto-Scroll (A key) makes the
// canvas follow the playback cursor.
// ─────────────────────────────────────────────────────────────────────────────

class BaySickPitchEditor : public juce::Component,
                            private juce::ScrollBar::Listener
{
public:
    explicit BaySickPitchEditor (BaySickVocalProcessor& p);
    ~BaySickPitchEditor() override;

    void paint   (juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove (const juce::MouseEvent&,
                          const juce::MouseWheelDetails&) override;

    bool hasAudio() const noexcept { return mHasAudio; }

    // Shared scroll/zoom state accessors (Toolbar + PitchGrid + scrollbars
    // all read from these so they stay in sync).  G-9 wires up the actual
    // audio-length-driven horizontal range; until then the empty-state
    // editor uses kEmptyStateLengthSec as a placeholder file length.
    double pixelsPerSecond() const noexcept { return mPpsX; }
    double scrollSeconds()   const noexcept { return mScrollX; }
    int    topNote()         const noexcept { return mTopNote; }
    int    bottomNote()      const noexcept { return mBottomNote; }

    // Zoom mutators called from the Toolbar +/- buttons + Ctrl/Alt+Wheel.
    // Horizontal = pixels/second (clamp 20..400).  Vertical = visible
    // semitone count (clamp 12..96).  Both keep the visible center fixed.
    void zoomHorizontalBy (double factor);
    void zoomVerticalBy   (double factor);

    // Scroll mutators called from the scrollbars + plain/Shift+Wheel.
    void setScrollSeconds (double secs);
    void setTopNote       (int note);

    // Edit-mode enum (matches Newtone's three modes)
    enum class EditMode { Cut, Advanced, Vibrato };
    EditMode editMode() const noexcept { return mEditMode; }
    void setEditMode (EditMode m);

private:
    void scrollBarMoved (juce::ScrollBar* bar, double newRangeStart) override;
    void syncScrollBarsToState();

    class Toolbar;
    class PitchKeyboard;
    class PitchGrid;
    class InfoBar;

    BaySickVocalProcessor& mProc;

    std::unique_ptr<Toolbar>       mToolbar;
    std::unique_ptr<PitchKeyboard> mKeyboard;
    std::unique_ptr<PitchGrid>     mGrid;
    std::unique_ptr<InfoBar>       mInfoBar;

    juce::ScrollBar mHScroll { false };  // false = horizontal
    juce::ScrollBar mVScroll { true  };  // true  = vertical

    // Shared scroll/zoom state
    double mPpsX       { 100.0 };   // pixels per second
    double mScrollX    { 0.0 };     // seconds offset (left edge)
    int    mTopNote    { 84 };      // top visible MIDI note (C6)
    int    mBottomNote { 36 };      // bottom visible MIDI note (C2)

    static constexpr double kEmptyStateLengthSec = 60.0;
    static constexpr int    kHScrollH            = 12;
    static constexpr int    kVScrollW            = 12;

    bool mPushingToBars { false };   // re-entrancy guard for syncScrollBarsToState

    EditMode mEditMode { EditMode::Advanced };

    bool mHasAudio { false };       // empty-state flag

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickPitchEditor)
};
