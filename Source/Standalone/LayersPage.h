#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../EngineRig.h"
#include "../PatternManager.h"
#include "../DSP/EQ8MsDSP.h"
#include "SharedUI.h"
#include "PianoRoll.h"
#include "StandaloneApp.h"
#include "UndoActions.h"

// ── LayersPage ────────────────────────────────────────────────────────────────
// One Layers instrument page. Up to 8 instances (kMaxLayerPages).
//
// Two sub-tabs (J-6 EQ unification 2026-05-03 - EQ moved to Effects page):
//   Tab 0 "Player"     - the engine's editor, full page (engine is chosen at
//                        the ribbon "+" menu before the page exists -- L4)
//   Tab 1 "Piano Roll" - PianoRollContainer bound to layerRoll[mPageIndex]
//
// Engine choices: Harmless | VibePlayer | BaySickSynth
// QA-ModelShell TS1 (2026-07-27): the engine is MODEL-owned (EngineRig, keyed
// {Layers, pageIndex}).  This page is a disposable view: it holds a non-owning
// engine pointer plus the editor it creates, and requests engines from the rig
// -- construction, registration, and teardown all happen model-side.
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
    juce::Colour getPageColor() const { return mPageColor; }
    void setUndoContext(const UndoContext& ctx);

    // Sub-tab switching - called by PageMenuBar tab slot buttons
    void switchTab(int idx);

    // Fired AFTER switchTab applies the change. StandaloneEditor wires this to
    // auto-swap transport mode (Pattern when idx==1 piano roll, else Song).
    std::function<void(int idx)> onSubTabChanged;

    // Fired once when the user selects an engine (first pick only, locks after).
    // StandaloneEditor uses this to add the mixer channel strip.
    std::function<void()> onEngineSelected;

    // QA-Layout T3 (Window-3/4): title-strip chrome for the hosted engine --
    // centered colored name + the editor-owned preset button (empty/null when
    // no engine).  onEngineEditorRebuilt fires after selectEngine builds a new
    // editor, so the window's strip can re-mount without waiting for the next
    // page-show (the common add path shows the page BEFORE the engine lands).
    juce::String     stripEngineTitle()  const;
    juce::Colour     stripEngineAccent() const;
    juce::Component* stripPresetButton() const;
    std::function<void()> onEngineEditorRebuilt;

    // Tab name sync - called by StandaloneEditor when the ribbon tab is
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
    juce::AudioProcessor*       getEngineProcessor() const { return mEngineProcessor; }

    // P1+P2 persistence: StandaloneEditor calls this during project load to
    // restore the saved engine before pushing the engine's state back.  The
    // internal combo callback still uses the same method.
    void selectEngine (const juce::String& engineName);

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
    // QA-UndoCoverage Task 7: the same full-chain payload over in-memory XML
    // (the structural-undo snapshot capture/apply rides these).
    juce::String capturePagePresetXml();
    void         applyPagePresetXml (const juce::String& xmlText);
    // Ruling 3a: one chain-swap gesture (page-preset load) = one structural
    // transaction.  Skips the wrap on an engine-less page (nothing to
    // restore) -- runs the op plain.
    void performChainSwapGesture (const juce::String& label,
                                  const std::function<void()>& op);
    void showPageActionsMenu (juce::Component* anchor);
    // QA-Layout T15: the title strip's nav buttons dissolved into the Menu
    // dropdown -- the editor injects the window/view entries at the top of
    // the page-actions popup through this hook.
    std::function<void(juce::PopupMenu&)> onBuildWindowNavMenu;
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
    // 2026-04-26: fired when loadPreset applies a preset - owner renames the
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

    // ── Tab 0: Player ─────────────────────────────────────────────────────────
    // QA-Layout T2 (L4): the engine-picker row (LockableCombo + "Engine:"
    // label) is gone -- the engine is chosen at the "+" menu, and the old
    // context menu lives on the Menu dropdown (showPageActionsMenu).
    std::unique_ptr<juce::Component>            mPlayerTab;
    // Non-owning view of the rig-owned engine ({Layers, pageIndex}); the
    // editor IS view-owned and must be destroyed before the page (its
    // attachments reference the engine's APVTS).
    juce::AudioProcessor*                       mEngineProcessor { nullptr };
    std::unique_ptr<juce::AudioProcessorEditor> mEngineEditor;
    bool         mEngineLocked { false };
    UndoContext  mUndoCtx;   // QA-UndoCoverage: chain-swap gestures (ruling 3a)
    bool         mLocked       { false };  // D1.4-fix (c): protect from kit-replace
    juce::String mEngineType;
    juce::String mTabName;   // defaults to "Layer {pageIndex}"; overridden by ribbon rename

    // ── Tab 1: Piano Roll ─────────────────────────────────────────────────────
    std::unique_ptr<PianoRollContainer>         mPianoRoll;

    // J-6 EQ unification (2026-05-03): EQ tab + display removed; pre-rack EQ
    // is exclusively edited via the Effects page Pre EQ tab (same APVTS
    // params: mixer_layer_<N>_preeq_*).

    // ── Helpers ───────────────────────────────────────────────────────────────
    void buildPlayerTab();
    void buildPianoRollTab();

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
    // §P4.3 B7: midEQPrefix / sideEQPrefix helpers deleted - pre-rack EQ params
    // live on the mixer-strip prefix (mixer_layer_<N>_preeq_*), bound directly
    // by selectEngine().
    juce::String trackId()       const { return "lay_" + juce::String(mPageIndex); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LayersPage)
};
