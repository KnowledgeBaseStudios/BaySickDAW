#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../PatternManager.h"
#include "../DSP/EQ8MsDSP.h"
#include "SharedUI.h"
#include "PianoRoll.h"
#include "StandaloneApp.h"
#include "UndoActions.h"

// ── BassPage ──────────────────────────────────────────────────────────────────
// One Bass instrument page. Up to kMaxBassPages (4) instances.
//
// Three sub-tabs:
//   Tab 0 "Player"     — engine selector ComboBox (locks on first pick) + engine editor
//   Tab 1 "Piano Roll" — PianoRollContainer bound to bassRoll[mPageIndex]
//   Tab 2 "EQ"         — EQ8 M/S (pre-rack). MID/SIDE buttons float in the tab bar.
//
// Engine choices: Harmless | VibePlayer | BaySickBass
// Each page owns its engine processor + editor (created on first engine selection).
// Per-page APVTS params lazily registered via mProcessor.registerParamsForTrack()
// on first engine selection.
// ─────────────────────────────────────────────────────────────────────────────
class BassPage : public juce::Component,
                 public juce::Timer,
                 private juce::ValueTree::Listener
{
public:
    BassPage(VibeSynthProcessor& p, PatternManager& pm, int pageIndex);
    ~BassPage() override;

    void paint  (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    void setPlayHead(StandalonePlayHead* ph);
    int  getPageIndex()   const { return mPageIndex; }
    int  getActiveTab()     const { return mActiveTab; }
    bool isEngineLocked()   const { return mEngineLocked; }
    juce::Colour getPageColor() const { return mPageColor; }
    ParametricEQDisplay* getEQDisplay() const { return mEQDisplay.get(); }   // 2026-04-19: PageMenuBar hamburger wiring
    void setUndoContext(const UndoContext& ctx);

    void switchTab(int idx);
    void setEQMid(bool showMid);
    bool isEQMidActive() const { return mEQMidActive; }

    // Fired AFTER switchTab applies the change.
    std::function<void(int idx)> onSubTabChanged;

    // Fired once when the user selects an engine.
    std::function<void()> onEngineSelected;

    // Tab name sync from StandaloneEditor.
    void                setTabName(const juce::String& name);
    const juce::String& getTabName() const { return mTabName; }

    // Accessor for the piano-roll container (used by StandaloneEditor for
    // time-selection-aware loop + stop-seek behavior).
    PianoRollContainer* getPianoRoll() const { return mPianoRoll.get(); }

    // P1+P2 persistence (2026-04-24): same pattern as LayersPage.
    juce::String          getEngineType()      const { return mEngineType; }
    juce::AudioProcessor* getEngineProcessor() const { return mEngineProcessor.get(); }

    // P1+P2 persistence: StandaloneEditor calls this during project load.
    void selectEngine (const juce::String& engineName);

    // D1.4-fix (c) — per-bass right-click context menu + save / delete.
    void showContextMenu  (juce::Component* anchor);
    void savePatchAs      ();
    // 2026-04-25: Load preset (factory + user) for the current engine.
    // Handles both wrapped (savePatchAs) and raw apvts XML formats.
    // Performs prefix substitution so the preset binds to this tab's
    // track prefix regardless of where it was saved.
    void loadPreset       (const juce::File& xml);
    void requestDelete    ();
    juce::String exportBassState() const;
    void         importBassState (const juce::String& xml);
    bool isLocked() const { return mLocked; }
    void setLocked(bool l);   // D2: fires onLockChanged
    std::function<void()>           onDeleteRequested;
    std::function<void(const juce::String& clipboardXml)> onDuplicateRequested;
    std::function<void()>           onLockChanged;
    std::function<void()>           onRenameRequested;
    // 2026-04-26: fired when loadPreset applies a preset — owner renames the
    // ribbon tab to match the preset's filename (mirrors DrumPage).
    std::function<void(const juce::String& newName)> onSoundNameChanged;

private:
    VibeSynthProcessor& mProcessor;
    PatternManager&     mPM;
    int                 mPageIndex;   // 0-3
    juce::Colour        mPageColor;   // from VC::BassCol[mPageIndex]
    StandalonePlayHead* mPlayHead { nullptr };

    // ── Tab system ────────────────────────────────────────────────────────────
    int  mActiveTab   { 0 };
    bool mEQMidActive { true };

    // ── Tab 0: Player ─────────────────────────────────────────────────────────
    std::unique_ptr<juce::Component>            mPlayerTab;
    // LockableCombo: same pattern as LayersPage — see LayersPage.h.
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
    std::unique_ptr<LockableCombo>              mEngineCombo;
    std::unique_ptr<juce::Label>                mEngineLabel;
    std::unique_ptr<juce::AudioProcessor>       mEngineProcessor;
    std::unique_ptr<juce::AudioProcessorEditor> mEngineEditor;
    bool         mEngineLocked { false };
    bool         mLocked       { false };  // D1.4-fix (c): protect from kit-replace
    juce::String mEngineType;
    juce::String mTabName;   // defaults to "Bass {pageIndex}"; overridden by ribbon rename

    // ── Tab 1: Piano Roll ─────────────────────────────────────────────────────
    std::unique_ptr<PianoRollContainer>         mPianoRoll;

    // ── Tab 2: EQ ─────────────────────────────────────────────────────────────
    // §P4.3 B7: page no longer owns a local EQ DSP.  mEQDisplay binds to the
    // Bass InsertNode's preEq member (via VibeGraph::getInsertPreEQ).
    std::unique_ptr<juce::Component>            mEQTab;
    std::unique_ptr<ParametricEQDisplay>        mEQDisplay;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void buildPlayerTab();
    void buildPianoRollTab();
    void buildEQTab();

    void refreshPianoRollContextLabel();

    // D2: dirty-snapshot for the requestDelete prompt + ValueTree listener
    // for engine-editor preset loads.
    juce::MemoryBlock mLoadedStateSnapshot;
    void takeStateSnapshot();
    bool isPatchDirty() const;
    void valueTreeRedirected (juce::ValueTree& tree) override;
    void subscribeToEngineApvtsState();
    void unsubscribeFromEngineApvtsState();

    // 2026-04-21: "bas_" (3-char) prefix matches the engine-processor trackId convention.
    // §P4.3 B7: midEQPrefix / sideEQPrefix helpers deleted — pre-rack EQ params
    // live on the mixer-strip prefix (mixer_bass_<N>_preeq_*).
    juce::String trackId()      const { return "bas_" + juce::String(mPageIndex); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassPage)
};
