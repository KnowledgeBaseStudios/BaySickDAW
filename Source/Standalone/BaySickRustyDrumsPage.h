#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "SharedUI.h"

class BaySickRustyDrumsKitGraphic;
class AriaControlPanel;

// ── BaySickRustyDrumsPage ───────────────────────────────────────────────────
// J-8 (2026-05-04) revised: dedicated page for the BaySickRustyDrums singleton
// engine.  Mirrors DrumPage's 3-sub-tab layout: Drum Kit / Player / Piano Roll.
//
// Sub-tabs:
//   Tab 0 "Drum Kit"   - kit-graphic auditioner (clickable hitbox kit photo,
//                        full-bleed; pre-load shows photo dimmed at 50%
//                        opacity with "Pick a program to begin" overlay text)
//   Tab 1 "Player"     - ARIA control panel (knobs/dropdowns/labels rendered
//                        from the kit's GUI XML).  Pre-load state mirrors
//                        Drum Kit's overlay text.
//   Tab 2 "Piano Roll" - nav shortcut to the unified PianoRollPage with the
//                        BaySickRustyDrums engine selected (matches DrumPage's
//                        nav-shortcut pattern; not a real local sub-tab).
//
// Program selector: dropdown right-aligned on the sub-tab bar (rendered by
// PageMenuBar's extra-right area, populated by StandaloneEditor).  Default
// "Load Player" with options "Full" and "Basic"; switching mid-session
// triggers a confirm prompt + tear-down + reload.
//
// No page-level EQ tab: each Rusty mixer strip + the RustyDrums Bus expose
// pre + post EQ on the Effects page.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickRustyDrumsPage : public juce::Component,
                              public juce::Timer
{
public:
    BaySickRustyDrumsPage (VibeSynthProcessor& p);
    ~BaySickRustyDrumsPage() override;

    void paint  (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    // Sub-tab switching.  3 tabs: 0=Drum Kit, 1=Player, 2=Piano Roll.
    // Index 2 is a redirect handled at StandaloneEditor level (selecting
    // it switches the ribbon to PianoRollPage with this engine active);
    // here we just track the requested index.
    void switchTab (int idx);
    int  getActiveTab() const { return mActiveTab; }

    void                setTabName (const juce::String& name);
    const juce::String& getTabName() const { return mTabName; }

    juce::Colour getPageColor() const { return mPageColor; }

    // Fired when the kit's display name changes (program switch / preset load).
    std::function<void(const juce::String&)> onSoundNameChanged;

    // Fired after a successful program load - StandaloneEditor wires this to
    // MixerPage::addRustyChannelAtIndex so the visible strips spawn alongside
    // the audio-graph InsertNodes that PluginProcessor::loadBaySickRustyDrumsKit
    // just created.
    std::function<void()> onKitLoaded;

    // QA-UndoCoverage rulings 1a/3a (2026-08-06): the editor wraps one
    // structural gesture (program switch, player-preset load) as one undo
    // transaction -- composite capture of the engine blob + the pattern slice
    // the teardown clears.  Falls back to a plain run when unwired.
    std::function<void (const juce::String& label, std::function<void()> op)> onWrapStructuralOp;

    // QA-E Task 8 NIT-1 (engine-type half): fired AFTER mCurrentProgram is
    // updated (loadProgram / reloadForProjectRestore) so listeners read the
    // NEW program.  onKitLoaded fires mid-loadKit, BEFORE mCurrentProgram is
    // set, so it can't be used for this (it returns the prior program).
    std::function<void()> onProgramChanged;

    // Fired when the user requests removal of this page (ribbon right-click).
    std::function<void()> onDeleteRequested;

    // QA-ProjectSave Task 11 (docket 16): FND-1 completion.  The delete
    // prompt's "Save Page Preset & Delete" branch chains the J-11 Player
    // Preset save from StandaloneEditor, so both live here in public.
    void savePlayerPresetAs (std::function<void()> onSaved = {});
    bool isPatchDirty() const { return mPageDirty; }

    // Programmatic kit load (file path -> processor).  Returns true on success.
    bool loadKit (const juce::File& sfzPath);

    // J-9 (2026-05-05): full project-restore path.  Identifies the Program
    // from the kit-file name, loads the kit through the wrapper (so the
    // mRustyDrumsActive flag protects sfizz during the load), syncs the
    // program dropdown + mCurrentProgram enum so a future re-pick is a
    // no-op (preventing the kit's set_cc defaults from stomping saved CC
    // values), and renders the ARIA control panel for the program.  Caller
    // is expected to apvts.replaceState the saved engine state AFTER this
    // returns so saved CCs overlay the just-written kit defaults.
    bool reloadForProjectRestore (const juce::File& sfzPath);

    // J-8 Part B: program selector dropdown - owned by this page, exposed to
    // StandaloneEditor which adds it to PageMenuBar's extra-right cluster
    // when this page is visible.  "Load Player" is the placeholder until the
    // user picks a program; subsequent picks of a different program show a
    // confirm prompt + tear-down + reload.
    juce::ComboBox* getProgramCombo() const { return mProgramCombo.get(); }

    // J-11 (2026-05-05): Player Preset dropdown - sits on the PageMenuBar's
    // extras-right cluster next to the Program selector.  Captures kit CC
    // values only (every `brd_cc<N>` + `brd_outVol`); independent of the Page
    // Preset (which also captures mixer strips + racks).  Storage at
    // `Documents/BaySickDAW/Presets/Rusty Player/My Presets/<name>.xml`.
    juce::TextButton* getPlayerPresetButton() const { return mPlayerPresetBtn.get(); }

    enum class Program { None = 0, Full = 1, Basic = 2 };
    Program getCurrentProgram() const noexcept { return mCurrentProgram; }
    // Ruling 1A (2026-08-06): the session's FIRST program pick is undoable --
    // undo returns the page to the empty player.  This is that restore: the
    // switch teardown without a follow-up load, plus UI reset to None.
    void unloadToNone();
    // QA-E Task 8 NIT-1 (engine-type half): mirrors LayersPage::getEngineType()
    // so the piano-roll context label can show "{tab} - Full" / "{tab} -
    // Basic" (empty -> "(no engine)" pre-load, consistent with other engines).
    juce::String getEngineType() const;

private:
    VibeSynthProcessor& mProcessor;

    int          mActiveTab   { 0 };
    juce::String mTabName     { "BaySickRustyDrums" };
    juce::Colour mPageColor   { VC::DrumsCol };

    // ── Tab 0: Drum Kit (auditioner - full-bleed kit graphic) ──────────────
    std::unique_ptr<juce::Component>                mDrumKitTab;
    std::unique_ptr<BaySickRustyDrumsKitGraphic>    mKitGraphic;

    // ── Tab 1: Player (ARIA control panel - full kit GUI XML rendered) ─────
    std::unique_ptr<juce::Component>                mPlayerTab;
    std::unique_ptr<AriaControlPanel>               mAriaPanel;

    // ── Tab 2: Piano Roll (redirect - no local content, handled by editor) ─
    std::unique_ptr<juce::Component>                mPianoRollTab;
    std::unique_ptr<juce::Label>                    mPianoRollPlaceholder;

    // Program selector dropdown (lives on PageMenuBar extras-right when page is visible).
    std::unique_ptr<juce::ComboBox>                 mProgramCombo;
    Program                                         mCurrentProgram { Program::None };

    // J-11: Player Preset dropdown (lives next to mProgramCombo on PageMenuBar).
    std::unique_ptr<juce::TextButton>               mPlayerPresetBtn;

    void buildDrumKitTab();
    void buildPlayerTab();
    void buildPianoRollTab();
    void buildProgramCombo();
    void buildPlayerPresetButton();                  // J-11
    void onProgramComboChanged();
    void promptAndSwitchProgram (Program target);
    bool loadProgram (Program target);
    void tearDownCurrentProgram();
    void loadAriaPanelForProgram (Program target);   // J-8 stage 2

    // J-11 Player Preset helpers.
    void showPlayerPresetMenu();
    void loadPlayerPresetFromFile (const juce::File& xml);
    juce::File playerPresetsDir() const;

    // QA-ProjectSave Task 11: dirty tracking, ported verbatim from the
    // ClipsPage/VoxPage trio -- listener-based so it catches every parameter
    // mutation regardless of how the engine serializes.  Attached to the
    // engine apvts.state on kit load; detached before program teardown.
    // mSuppressDirty is set during preset-apply paths so a freshly-loaded
    // preset doesn't read as dirty.
    struct ApvtsDirtyListener : public juce::ValueTree::Listener
    {
        bool* dirtyFlag { nullptr };
        bool* suppress  { nullptr };
        void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override
        {
            if (suppress && *suppress) return;
            if (dirtyFlag) *dirtyFlag = true;
        }
        void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override {}
        void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}
        void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
        void valueTreeParentChanged (juce::ValueTree&) override {}
        void valueTreeRedirected (juce::ValueTree&) override {}
    };
    bool              mPageDirty { false };
    bool              mSuppressDirty { false };
    ApvtsDirtyListener mDirtyListener;
    void attachDirtyListener();
    void detachDirtyListener();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickRustyDrumsPage)
};
