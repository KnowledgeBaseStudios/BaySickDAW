#pragma once
#include <JuceHeader.h>
#include "../Standalone/SharedUI.h"   // ParametricEQDisplay

class VibeSynthProcessor;

// ─────────────────────────────────────────────────────────────────────────────
// InstPage — host component for one Inst tab (Phase G-4).
// ─────────────────────────────────────────────────────────────────────────────
// Mirror of ClipsPage with Inst-specific colour + naming.  Engine picker
// offers BaySickPlayer (sampler) + BaySickNAM/IR (re-amp) with full A/B-style
// persistence (both processors stay alive across swaps so each retains APVTS
// state).  The page is spawned ONLY by the Mixer page's "Add Inst Strip"
// button — the ribbon Inst dropdown is an instance switcher only (no `+ Add`).
// Sub-tabs mirror Layer/Bass shape (Player / Piano Roll redirect / Pre EQ8
// M/S stub).
// ─────────────────────────────────────────────────────────────────────────────

class InstPage : public juce::Component,
                 public juce::FileDragAndDropTarget
{
public:
    // 2026-04-29 DEBUG: drag-drop file → BaySickPlayer (auto-pick + load).
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped           (const juce::StringArray& files, int x, int y) override;

    enum class EngineType { None, BaySickPlayer, BaySickNAMIR };

    explicit InstPage (int pageIndex);
    ~InstPage() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    void switchTab    (int idx);
    int  getActiveTab () const noexcept { return mActiveTab; }

    int          getPageIndex() const noexcept { return mPageIndex; }
    // 2026-04-28 (G-4): page accent matches the mixer Inst-bus colour
    // (`0xff1c3a8a` navy) so the ribbon tab + Mixer strip + page header
    // all read as the same channel identity.
    juce::Colour getPageColor() const noexcept { return juce::Colour (0xff1c3a8a); }

    juce::String getClipFilePath() const                    { return mClipPath; }
    void         setClipFilePath (const juce::String& p);

    void          selectEngine (EngineType e);
    EngineType    getEngineType() const noexcept { return mEngineType; }
    juce::AudioProcessor* getEngineProcessor() const noexcept;

    std::function<void()> onEngineDestroying;
    std::function<void()> onEngineChanged;

    void                setTabName (const juce::String& n) { mTabName = n; repaint(); }
    const juce::String& getTabName () const                 { return mTabName; }

    // ── G-6 (2026-04-29): full-state export/import for Duplicate flow ────────
    juce::String exportInstState() const;
    void         importInstState (const juce::String& xml);

    // ── G-6 (2026-04-29): right-click engine-picker context menu callbacks ─
    std::function<void()>                        onDuplicateRequested;
    std::function<void()>                        onRenameRequested;
    std::function<void()>                        onDeleteRequested;
    std::function<void()>                        onLockChanged;

    bool isLocked() const noexcept { return mLocked; }
    void setLocked (bool b) { if (b == mLocked) return; mLocked = b; if (onLockChanged) onLockChanged(); repaint(); }

    // ── G-7 polish (2026-04-29): Pre EQ8 M/S sub-tab (mirrors LayersPage) ─────
    ParametricEQDisplay* getEQDisplay() const { return mEQDisplay.get(); }
    void setEQMid (bool showMid)
    {
        mEQMidActive = showMid;
        if (mEQDisplay) mEQDisplay->setShowMid (showMid);
    }
    bool isEQMidActive() const { return mEQMidActive; }

    // Save/Load PAGE preset — entire InstPage state (BaySickPlayer + BaySickNAM/IR;
    // Phase I adds BaySickPedals).  XML matches exportInstState format.
    void saveInstPagePreset();
    void loadInstPagePreset (const juce::File& xml);

    // ── G-7 (2026-04-29): Page Preset save/load (full chain) ─────────────────
    // Bus fallback: if a saved _sendTo references kInstBus2 / kInstBus3 and
    // those buses aren't active in the current project, the loader silently
    // substitutes kInstBus.
    void setProcessor (VibeSynthProcessor* p) { mFullProcessor = p; }
    void setBusActiveQuery (std::function<bool(int channelId)> q) { mBusActiveQuery = std::move (q); }
    void savePagePreset (std::function<void()> onSaved = {});
    void loadPagePreset (const juce::File& xml);
    void showPageActionsMenu (juce::Component* anchor);
    void requestDelete ();

private:
    void buildEnginePicker();
    void buildEQTab();   // G-7 polish: replaced buildEqStub
    void layoutEditor (juce::Rectangle<int> r);
    juce::AudioProcessorEditor* activeEditor() const;
    void showEngineContextMenu();

    // G-6: ComboBox subclass with right-click → onRightClick callback.
    class RightClickEngineCombo : public juce::ComboBox
    {
    public:
        std::function<void()> onRightClick;
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
            juce::ComboBox::mouseDown (e);
        }
    };

    int                                          mPageIndex { 0 };
    int                                          mActiveTab { 0 };
    juce::String                                 mTabName;
    juce::String                                 mClipPath;
    bool                                         mLocked { false };

    RightClickEngineCombo                        mEnginePicker;
    juce::Label                                  mClipFileLabel;
    EngineType                                   mEngineType { EngineType::None };
    std::unique_ptr<juce::AudioProcessor>        mPlayerProc;
    std::unique_ptr<juce::AudioProcessor>        mNamIrProc;
    std::unique_ptr<juce::AudioProcessorEditor>  mPlayerEditor;
    std::unique_ptr<juce::AudioProcessorEditor>  mNamIrEditor;

    // G-7 polish (2026-04-29): real Pre EQ8 M/S display.
    std::unique_ptr<ParametricEQDisplay>         mEQDisplay;
    bool                                         mEQMidActive { true };

    // G-7: full processor + bus-active query for Page Preset save/load.
    VibeSynthProcessor*                          mFullProcessor { nullptr };
    std::function<bool(int)>                     mBusActiveQuery;

    // G-7 (2026-04-29): listener-based dirty tracking — see ClipsPage for
    // the rationale.
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
    bool                                         mPageDirty { false };
    bool                                         mSuppressDirty { false };
    ApvtsDirtyListener                           mDirtyListener;
    void attachDirtyListener();
    void detachDirtyListener();
    void takeStateSnapshot();
    bool isPatchDirty() const { return mPageDirty; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstPage)
};

// ─────────────────────────────────────────────────────────────────────────────
// InstEmptyState — placeholder shown when the Inst ribbon slot is clicked
// with zero instances.  Unlike ClipsEmptyState there's no drag-drop target;
// the spawn trigger is the "Add Inst Strip" button on the Mixer page.
// ─────────────────────────────────────────────────────────────────────────────
class InstEmptyState : public juce::Component
{
public:
    InstEmptyState();
    void paint (juce::Graphics&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstEmptyState)
};
