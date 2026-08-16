#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../EngineRig.h"
#include "../PatternManager.h"
#include "../DSP/EQ8MsDSP.h"
#include "SharedUI.h"
#include "StandaloneApp.h"
#include "UndoActions.h"

// ── BassPage ──────────────────────────────────────────────────────────────────
// One Bass instrument page. Up to kMaxBassPages (4) instances.
//
// Two sub-tabs (J-6 EQ unification 2026-05-03 - EQ moved to Effects page):
//   Tab 0 "Player"     - the engine's editor, full page (engine is chosen at
//                        the ribbon "+" menu before the page exists -- L4)
//   Tab 1 "Piano Roll" - a REDIRECT, not a view.  The roll itself lives on the
//                        unified PianoRollPage (2026-04-26); this page owns no
//                        roll component.  QA-Layout T4 (L11) moved the row into
//                        the page dropdown's "Pages:" list
//                        (StandaloneEditor::buildPageWindowRows), which navigates
//                        to the Piano Roll tab -- there is no sub-tab pill.
//
// Engine choices: Harmless | BaySickPlayer | BaySickBass
// QA-ModelShell TS1 (2026-07-27): the engine is MODEL-owned (EngineRig, keyed
// {Bass, pageIndex}).  This page is a disposable view: it holds a non-owning
// engine pointer plus the editor it creates, and requests engines from the rig
// -- construction, registration, and teardown all happen model-side.
// ─────────────────────────────────────────────────────────────────────────────
class BassPage : public juce::Component,
                 private juce::ValueTree::Listener
{
public:
    BassPage(BaySickDAWProcessor& p, PatternManager& pm, int pageIndex);
    ~BassPage() override;

    void paint  (juce::Graphics&) override;
    void resized() override;

    int  getPageIndex()   const { return mPageIndex; }
    int  getActiveTab()     const { return mActiveTab; }
    bool isEngineLocked()   const { return mEngineLocked; }
    juce::Colour getPageColor() const { return mPageColor; }
    void setUndoContext(const UndoContext& ctx);

    void switchTab(int idx);

    // Fired once when the user selects an engine.
    std::function<void()> onEngineSelected;

    // Tab name sync from StandaloneEditor.
    void                setTabName(const juce::String& name);
    const juce::String& getTabName() const { return mTabName; }

    // P1+P2 persistence (2026-04-24): same pattern as LayersPage.
    juce::String          getEngineType()      const { return mEngineType; }
    juce::AudioProcessor* getEngineProcessor() const { return mEngineProcessor; }

    // P1+P2 persistence: StandaloneEditor calls this during project load.
    void selectEngine (const juce::String& engineName);
    // Replace Engine (Jeff, 2026-08-16) + cross-engine page-preset loads: the
    // swap path with the one-shot pick gate bypassed (see LayersPage twin).
    void selectEngineInternal (const juce::String& engineName);

    // QA-Layout T3 (Window-3/4): title-strip chrome for the hosted engine --
    // see LayersPage.h for the contract.
    juce::String     stripEngineTitle()  const;
    juce::Colour     stripEngineAccent() const;
    juce::Component* stripPresetButton() const;
    std::function<void()> onEngineEditorRebuilt;

    void savePatchAs      ();
    // 2026-04-25: Load preset (factory + user) for the current engine.
    // Reads the two engine-native shapes savePatchAs writes - raw apvts XML,
    // and the nested <BaySickPlayerState> + <Sample> form for the sample
    // engine - plus the retired <BaySickEnginePreset> wrapper, which no
    // writer produces any more.  Performs prefix substitution so the preset
    // binds to this tab's track prefix regardless of where it was saved.
    void loadPreset       (const juce::File& xml);
    void requestDelete    ();
    juce::String exportBassState() const;
    void         importBassState (const juce::String& xml);

    // ── G-7 (2026-04-29): Page Preset save/load (full chain) ─────────────────
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
    // transaction.  Skips the wrap on an engine-less page.
    void performChainSwapGesture (const juce::String& label,
                                  const std::function<void()>& op);
    void showPageActionsMenu (juce::Component* anchor);
    // QA-Layout T15: the title strip's nav buttons dissolved into the Menu
    // dropdown -- the editor injects the window/view entries at the top of
    // the page-actions popup through this hook.
    std::function<void(juce::PopupMenu&)> onBuildWindowNavMenu;
    bool isLocked() const { return mLocked; }
    void setLocked(bool l);   // D2: fires onLockChanged
    // Every action undoable: the Menu's Lock entry rides one structural
    // transaction.  Skips the wrap when no undo context is wired.
    void toggleLockUndoable();
    std::function<void()>           onDeleteRequested;
    std::function<void(const juce::String& clipboardXml)> onDuplicateRequested;
    std::function<void()>           onLockChanged;
    std::function<void()>           onRenameRequested;
    // 2026-04-26: fired when loadPreset applies a preset - owner renames the
    // ribbon tab to match the preset's filename (mirrors DrumPage).
    std::function<void(const juce::String& newName)> onSoundNameChanged;

private:
    BaySickDAWProcessor& mProcessor;
    PatternManager&     mPM;
    int                 mPageIndex;   // 0-3
    juce::Colour        mPageColor;   // from VC::BassCol[mPageIndex]

    // ── Tab system ────────────────────────────────────────────────────────────
    int  mActiveTab   { 0 };

    // ── Tab 0: Player ─────────────────────────────────────────────────────────
    // QA-Layout T2 (L4): the engine-picker row is gone -- same rationale as
    // LayersPage.h.
    std::unique_ptr<juce::Component>            mPlayerTab;
    // Non-owning view of the rig-owned engine ({Bass, pageIndex}); the editor
    // IS view-owned and must be destroyed before the page (its attachments
    // reference the engine's APVTS).
    juce::AudioProcessor*                       mEngineProcessor { nullptr };
    std::unique_ptr<juce::AudioProcessorEditor> mEngineEditor;
    bool         mEngineLocked { false };
    UndoContext  mUndoCtx;   // QA-UndoCoverage: chain-swap gestures (ruling 3a)
    bool         mLocked       { false };  // D1.4-fix (c): protect from kit-replace
    juce::String mEngineType;
    juce::String mTabName;   // defaults to "Bass {pageIndex}"; overridden by ribbon rename

    // J-6 EQ unification (2026-05-03): EQ tab + display removed; pre-rack EQ
    // is exclusively edited via the Effects page Pre EQ tab.

    // ── Helpers ───────────────────────────────────────────────────────────────
    void buildPlayerTab();

    // D2: dirty-snapshot for the requestDelete prompt + ValueTree listener
    // for engine-editor preset loads.
    juce::MemoryBlock mLoadedStateSnapshot;
    void takeStateSnapshot();
    bool isPatchDirty() const;
    void valueTreeRedirected (juce::ValueTree& tree) override;
    void subscribeToEngineApvtsState();
    void unsubscribeFromEngineApvtsState();

    // 2026-04-21: "bas_" (3-char) prefix matches the engine-processor trackId convention.
    // §P4.3 B7: midEQPrefix / sideEQPrefix helpers deleted - pre-rack EQ params
    // live on the mixer-strip prefix (mixer_bass_<N>_preeq_*).
    juce::String trackId()      const { return "bas_" + juce::String(mPageIndex); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassPage)
};
