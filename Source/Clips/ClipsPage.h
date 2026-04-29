#pragma once
#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
// ClipsPage — host component for one Clips tab (Phase G-2).
// ─────────────────────────────────────────────────────────────────────────────
// One Clips tab = one engine instance.  The engine is picked from a small
// dropdown (BaySickPlayer or BaySickNAM/IR) — same UX pattern as the Layer /
// Bass / Drum tabs, just with a narrower set of options.  The page itself
// can ONLY be spawned via dragging or uploading a clip onto the Builder grid
// or the Clips empty-state placeholder; the ribbon's Clip dropdown is an
// instance switcher only (no `+ Add` entry).
//
// Engine semantics:
//   BaySickPlayer  — sampler-style instrument; piano-roll notes trigger the
//                    loaded clip.  Default on first spawn.
//   BaySickNAM/IR  — re-amp processor; the clip's audio is intended to feed
//                    the amp model's input.  Audio routing for re-amp is
//                    G-3 work; G-2 just hosts the editor.
//
// Sub-tabs mirror Layer/Bass shape — Player / Piano Roll / Pre EQ8 M/S — but
// only Player is locally rendered.  Piano Roll redirects to PianoRollPage
// via the StandaloneEditor's PageMenuBar setTabSlots wiring; Pre EQ8 M/S is
// a placeholder until later polish.
// ─────────────────────────────────────────────────────────────────────────────

class ClipsPage : public juce::Component
{
public:
    enum class EngineType { None, BaySickPlayer, BaySickNAMIR };

    explicit ClipsPage (int pageIndex);
    ~ClipsPage() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    // ── Sub-tab control (driven by StandaloneEditor's PageMenuBar pills) ─────
    void switchTab    (int idx);
    int  getActiveTab () const noexcept { return mActiveTab; }

    // ── Page metadata ────────────────────────────────────────────────────────
    int          getPageIndex() const noexcept { return mPageIndex; }
    // 2026-04-28 (G-2): page accent matches the mixer Clips-bus colour
    // (`VC::Warm` = 0xffd4a017 amber/gold) so the ribbon tab + Mixer strip +
    // page header all read as the same channel identity.
    juce::Colour getPageColor() const noexcept { return juce::Colour (0xffd4a017); }

    // ── Clip path access (set by drop-spawn flow) ────────────────────────────
    juce::String getClipFilePath() const                    { return mClipPath; }
    void         setClipFilePath (const juce::String& p);

    // ── Engine selection ─────────────────────────────────────────────────────
    void          selectEngine (EngineType e);
    EngineType    getEngineType() const noexcept { return mEngineType; }
    // Returns the currently-active engine instance (null if EngineType::None).
    // Both BaySickPlayer + NAM/IR instances are kept alive across swaps so
    // their settings persist — this picker just selects which one is "live".
    juce::AudioProcessor* getEngineProcessor() const noexcept;

    // G-3 (2026-04-28): fired BEFORE the active engine swaps so the editor
    // can unregisterClipEngine while the OLD pointer is still valid (no
    // dangling-pointer crash on audio thread).  Counterpart to onEngineChanged.
    std::function<void()> onEngineDestroying;
    // Fired AFTER the active engine has been swapped to its new value.  The
    // editor wires this to call registerClipEngine with the new active
    // processor so the audio thread starts dispatching MIDI to it.
    std::function<void()> onEngineChanged;

    // ── Tab name (for ribbon rename) ─────────────────────────────────────────
    void                setTabName (const juce::String& n) { mTabName = n; repaint(); }
    const juce::String& getTabName () const                 { return mTabName; }

private:
    void buildEnginePicker();
    void buildEqStub();
    void layoutEditor (juce::Rectangle<int> r);
    juce::AudioProcessorEditor* activeEditor() const;

    int                                          mPageIndex { 0 };
    int                                          mActiveTab { 0 };   // 0 = Player, 2 = EQ stub
    juce::String                                 mTabName;
    juce::String                                 mClipPath;

    juce::ComboBox                               mEnginePicker;
    juce::Label                                  mClipFileLabel;     // small filename strip above editor
    EngineType                                   mEngineType { EngineType::None };
    // G-3 (2026-04-28): dual-engine A/B-style architecture.  Both engines are
    // lazy-created on first selection and KEPT ALIVE across swaps so each
    // retains its own APVTS state (knobs, params, loaded files) — picking
    // BaySickPlayer → BaySickNAM/IR → BaySickPlayer doesn't reset anything.
    // Audio-thread crash fix: registration is unregistered BEFORE swapping
    // the active pointer, never destroyed mid-block.
    std::unique_ptr<juce::AudioProcessor>        mPlayerProc;        // VibePlayerProcessor; null until BaySickPlayer first picked
    std::unique_ptr<juce::AudioProcessor>        mNamIrProc;         // BaySickNAMIRProcessor; null until NAM/IR first picked
    std::unique_ptr<juce::AudioProcessorEditor>  mPlayerEditor;
    std::unique_ptr<juce::AudioProcessorEditor>  mNamIrEditor;

    std::unique_ptr<juce::Label>                 mEqStub;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipsPage)
};

// ─────────────────────────────────────────────────────────────────────────────
// ClipsEmptyState — shown when the Clips ribbon slot is clicked with zero
// instances.  Drop a supported audio file here OR onto the Builder grid to
// spawn the first Clips tab.  G-2 (2026-04-28).
// ─────────────────────────────────────────────────────────────────────────────
class ClipsEmptyState : public juce::Component,
                        public juce::FileDragAndDropTarget
{
public:
    ClipsEmptyState();

    void paint (juce::Graphics&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped           (const juce::StringArray& files, int x, int y) override;

    std::function<void(const juce::String& filePath)> onClipDropped;

private:
    bool mFileHover { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipsEmptyState)
};
