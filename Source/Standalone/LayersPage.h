#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../PatternManager.h"
#include "../DSP/EQ8MsDSP.h"
#include "SharedUI.h"
#include "PianoRoll.h"
#include "StandaloneApp.h"
#include "UndoActions.h"

// ── LayersPage ────────────────────────────────────────────────────────────────
// One Layers instrument page. Up to 8 instances (kMaxLayerPages).
//
// Three sub-tabs:
//   Tab 0 "Player"     — engine selector ComboBox (locks on first pick) + engine editor
//   Tab 1 "Piano Roll" — PianoRollContainer bound to layerRoll[mPageIndex]
//   Tab 2 "EQ"         — EQ8 M/S (pre-rack). MID/SIDE buttons float in the tab bar.
//
// Engine choices: Harmless | VibePlayer | BaySickSynth
// Each page owns its engine processor + editor (created on first engine selection).
// Per-page APVTS params lazily registered via mProcessor.registerParamsForTrack()
// on first engine selection.
// ─────────────────────────────────────────────────────────────────────────────
class LayersPage : public juce::Component,
                   public juce::Timer,
                   private juce::ValueTree::Listener
{
public:
    LayersPage(VibeSynthProcessor& p, PatternManager& pm, int pageIndex);
    ~LayersPage() override;

    void paint  (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    void setPlayHead(StandalonePlayHead* ph);
    int  getPageIndex()   const { return mPageIndex; }
    int  getActiveTab()     const { return mActiveTab; }
    bool isEngineLocked()   const { return mEngineLocked; }
    ParametricEQDisplay* getEQDisplay() const { return mEQDisplay.get(); }   // 2026-04-19: PageMenuBar hamburger wiring
    juce::Colour getPageColor() const { return mPageColor; }
    void setUndoContext(const UndoContext& ctx);

    // Sub-tab switching — called by PageMenuBar tab slot buttons
    void switchTab(int idx);

    // Fired AFTER switchTab applies the change. StandaloneEditor wires this to
    // auto-swap transport mode (Pattern when idx==1 piano roll, else Song).
    std::function<void(int idx)> onSubTabChanged;

    // EQ MID/SIDE — called by PageMenuBar MID/SIDE slot buttons
    void setEQMid(bool showMid);
    bool isEQMidActive() const { return mEQMidActive; }

    // Fired once when the user selects an engine (first pick only, locks after).
    // StandaloneEditor uses this to add the mixer channel strip.
    std::function<void()> onEngineSelected;

    // Tab name sync — called by StandaloneEditor when the ribbon tab is
    // renamed. Refreshes the piano-roll context label ("{tab} - {engine}").
    void                setTabName(const juce::String& name);
    const juce::String& getTabName() const { return mTabName; }

    // Accessor for the piano-roll container (used by StandaloneEditor for
    // time-selection-aware loop + stop-seek behavior).
    PianoRollContainer* getPianoRoll() const { return mPianoRoll.get(); }

    // P1+P2 persistence (2026-04-24): expose the engine so StandaloneEditor
    // can round-trip the selection + the engine's internal state into the
    // project XML.  No engine selected yet => both return empty/null.
    juce::String                getEngineType()      const { return mEngineType; }
    juce::AudioProcessor*       getEngineProcessor() const { return mEngineProcessor.get(); }

    // P1+P2 persistence: StandaloneEditor calls this during project load to
    // restore the saved engine before pushing the engine's state back.  The
    // internal combo callback still uses the same method.
    void selectEngine (const juce::String& engineName);

    // D1.4-fix (c) — per-layer right-click context menu + save/delete.
    // Mirrors the drum tab pattern.  Lock toggle, Polyphony, Copy / Paste /
    // Duplicate, Choke Group placeholder, Save Patch As, Delete.
    void showContextMenu  (juce::Component* anchor);
    void savePatchAs      ();
    // 2026-04-25: Load preset (factory + user) for the current engine.
    // Handles both wrapped (savePatchAs) and raw apvts XML formats.
    void loadPreset       (const juce::File& xml);
    void requestDelete    ();
    juce::String exportLayerState() const;
    void         importLayerState (const juce::String& xml);

    // ── G-7 (2026-04-29): Page Preset save/load ──────────────────────────────
    // Captures the full chain (engine + strip params + insert rack + post-EQ)
    // and writes / restores it via PagePresetIO.  Distinct from savePatchAs
    // (engine-only).  Surfaced on the page menu bar's hamburger ≡.
    // onSaved fires only on successful save (cancel/empty-name aborts the
    // continuation).  Used by requestDelete's "Save Page Preset & Delete"
    // button to chain delete after save completes.
    void savePagePreset   (std::function<void()> onSaved = {});
    void loadPagePreset   (const juce::File& xml);
    void showPageActionsMenu (juce::Component* anchor);
    bool isLocked() const { return mLocked; }
    void setLocked(bool l);   // D2: fires onLockChanged so ribbon + UI reflect the new state
    std::function<void()>           onDeleteRequested;
    std::function<void(const juce::String& clipboardXml)> onDuplicateRequested;
    // D2: fired whenever mLocked toggles.  StandaloneEditor wires this to
    // mRibbon->setTabLocked so the ribbon shows the "[L] " prefix.
    std::function<void()>           onLockChanged;
    // D2: fired when the user picks Rename from the right-click context menu.
    // StandaloneEditor wires it to mRibbon->startRename.
    std::function<void()>           onRenameRequested;
    // 2026-04-26: fired when loadPreset applies a preset — owner renames the
    // ribbon tab to match the preset's filename (mirrors DrumPage).
    std::function<void(const juce::String& newName)> onSoundNameChanged;

private:
    VibeSynthProcessor& mProcessor;
    PatternManager&     mPM;
    int                 mPageIndex;   // 0-7
    juce::Colour        mPageColor;   // from VC::LayerCol[mPageIndex]
    StandalonePlayHead* mPlayHead { nullptr };

    // ── Tab system ────────────────────────────────────────────────────────────
    int  mActiveTab   { 0 };
    bool mEQMidActive { true };

    // D1.4-fix (c): combo subclass that intercepts clicks when locked so we
    // can route to the per-layer context menu (instead of the engine
    // dropdown popping up).  Pre-lock = normal ComboBox; post-lock = both
    // left-click and right-click invoke onLockedClick.
    struct LockableCombo : public juce::ComboBox
    {
        bool locked { false };
        std::function<void()> onLockedClick;
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (locked)
            {
                if (onLockedClick) onLockedClick();
                return;
            }
            juce::ComboBox::mouseDown (e);
        }
    };

    // ── Tab 0: Player ─────────────────────────────────────────────────────────
    std::unique_ptr<juce::Component>            mPlayerTab;
    std::unique_ptr<LockableCombo>              mEngineCombo;
    std::unique_ptr<juce::Label>                mEngineLabel;   // "Engine:" label
    // Engine processor + editor owned here; base-class pointers so we can hold any type
    std::unique_ptr<juce::AudioProcessor>       mEngineProcessor;
    std::unique_ptr<juce::AudioProcessorEditor> mEngineEditor;
    bool         mEngineLocked { false };
    bool         mLocked       { false };  // D1.4-fix (c): protect from kit-replace
    juce::String mEngineType;
    juce::String mTabName;   // defaults to "Layer {pageIndex}"; overridden by ribbon rename

    // ── Tab 1: Piano Roll ─────────────────────────────────────────────────────
    std::unique_ptr<PianoRollContainer>         mPianoRoll;

    // ── Tab 2: EQ ─────────────────────────────────────────────────────────────
    // §P4.3 B7: page no longer owns a local EQ DSP.  mEQDisplay binds to the
    // Layer InsertNode's preEq member (via VibeGraph::getInsertPreEQ).
    std::unique_ptr<juce::Component>            mEQTab;
    std::unique_ptr<ParametricEQDisplay>        mEQDisplay;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void buildPlayerTab();
    void buildPianoRollTab();
    void buildEQTab();

    // Push "{mTabName} - {mEngineType or (no engine)}" to mPianoRoll.
    void refreshPianoRollContextLabel();

    // D2: dirty-snapshot for the requestDelete prompt.  Fresh snapshot taken
    // on engine creation and on every apvts.replaceState (preset load) via
    // valueTreeRedirected; isPatchDirty compares current engine state
    // against it.  Clean patch → "Delete?" prompt; dirty → save-prompt.
    juce::MemoryBlock mLoadedStateSnapshot;
    void takeStateSnapshot();
    bool isPatchDirty() const;
    void valueTreeRedirected (juce::ValueTree& tree) override;
    void subscribeToEngineApvtsState();
    void unsubscribeFromEngineApvtsState();

    // APVTS track ID for this page.
    // 2026-04-21: unique "lay_{N}" prefix avoids collision with bass / drum
    //   engines at matching integer pageIndex. Matches engine processor prefix.
    // §P4.3 B7: midEQPrefix / sideEQPrefix helpers deleted — pre-rack EQ params
    // live on the mixer-strip prefix (mixer_layer_<N>_preeq_*), bound directly
    // by selectEngine().
    juce::String trackId()       const { return "lay_" + juce::String(mPageIndex); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LayersPage)
};
