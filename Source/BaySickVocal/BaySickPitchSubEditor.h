#pragma once
#include <JuceHeader.h>
#include "../DSP/BaySickPitchDSP.h"
#include "../Standalone/SharedUI.h"   // VibeSlider

class BaySickPitchEditor;

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPitchSubEditor - QA-Fd Task 7 (locked 10/15a; Jeff's design verbatim)
// ─────────────────────────────────────────────────────────────────────────────
// Popup automation-style editor for ONE pill's sub-edits, overlaid on the
// pitch editor:
//   - Lane spans the pill's length with the pill waveform ghosted behind.
//   - Editable points + linear curves with the EventEditor gesture set:
//     click empty = add point, drag = move, right-click a point = delete.
//   - Volume / Pitch lane toggle (volume gain 0..2 around unity; pitch
//     additive semitones +-12 around 0).
//   - Pill browser panel on the right listing pills in order (filtered to
//     the selection when entered from a multi-select) -- click to switch.
//   - Own Play button previewing the pill span through the current edits;
//     SPACE = play/stop while the popup has focus.
//   - Per-pill bipolar Vib + Frm knobs + the Variation knob (15a) on top.
//
// Single-storage contract (locked 19a): these lanes edit the SAME
// volShape/pitchShape points the on-pill corner handles write -- the popup
// shows handle-made ramps, handles sit at popup-made fade edges.
// All edits land on the GLOBAL undo through the owner (9a).
// ─────────────────────────────────────────────────────────────────────────────

class BaySickPitchSubEditor : public juce::Component
{
public:
    explicit BaySickPitchSubEditor (BaySickPitchEditor& owner);
    ~BaySickPitchSubEditor() override;

    // Open for a pill; browserIdxs = the pills listed in the side panel
    // (selection-filtered when opened from a multi-select).
    void openFor (int regionIdx, std::vector<int> browserIdxs);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void visibilityChanged() override;

    // Lane/knob edits route the global undo through the owner (one action
    // per gesture).  Public for the nested Lane class.
    void beginLaneEdit();
    void commitLaneEdit (const juce::String& label);

    // QA-Layout T4: hosting window wires this to its own title setter --
    // replaces the findParentComponentOfClass<DocumentWindow> escape so the
    // sub-editor never assumes what frames it.
    std::function<void (const juce::String&)> onTitleChanged;

private:
    class Lane;
    class Browser;

    PitchNoteRegion* region() const;
    void refreshFromRegion();
    void togglePlay();
    void switchToPill (int idx);

    // Friendship with BaySickPitchEditor does NOT extend to the nested
    // classes -- these accessors are their bridge.
    std::vector<PitchNoteRegion>& regionsRef() const;
    const juce::AudioBuffer<float>* compCache() const;
    double compSr() const;

    BaySickPitchEditor& mOwner;
    int mRegionIdx { -1 };
    std::vector<int> mBrowserIdxs;
    bool mShowPitchLane { false };

    std::unique_ptr<Lane>    mLane;
    std::unique_ptr<Browser> mBrowser;
    juce::TextButton mVolBtn, mPitchBtn, mPlayBtn;
    VibeSlider mVibKnob, mFrmKnob, mVarKnob;
    bool mSyncingKnobs { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickPitchSubEditor)
};

// ─────────────────────────────────────────────────────────────────────────────
// Floating host window -- the Event Editor's DocumentWindow chrome verbatim
// (title-bar color 0xff1a1c1e, resizable, screen-centered, close = X or Esc)
// so every popup editor shares one box style (owner call, 2026-07-11).
// Owned by BaySickPitchEditor via unique_ptr; closeButtonPressed routes to
// the owner, which resets the pointer (deletes the window).
// ─────────────────────────────────────────────────────────────────────────────
class BaySickPitchSubEditorWindow : public juce::DocumentWindow
{
public:
    explicit BaySickPitchSubEditorWindow (BaySickPitchEditor& owner);

    void closeButtonPressed() override;
    BaySickPitchSubEditor* content();

private:
    BaySickPitchEditor& mOwner;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickPitchSubEditorWindow)
};
