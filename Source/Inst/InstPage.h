#pragma once
#include <JuceHeader.h>
#include "../Standalone/SharedUI.h"   // ParametricEQDisplay
#include "../Standalone/EngineChainProcessor.h"   // I-16 G-9: Pedals -> NAM/IR chain

class VibeSynthProcessor;
class AriaControlPanel;

// ─────────────────────────────────────────────────────────────────────────────
// InstPage - host component for one Inst tab.
// ─────────────────────────────────────────────────────────────────────────────
// I-0b (2026-05-02): Restructured for Phase I.
//   * BaySickPlayer engine REMOVED entirely (no projects in the wild had it).
//   * No engine picker - both stage processors are pre-loaded into permanent
//     sub-tabs (mirrors Vox page layout).
//   * Sub-tabs (2 total - J-6 EQ unification 2026-05-03): BaySickPedals | BaySickNAM/IR.
//     - BaySickPedals (sub-tab 0): I-1 will install BaySickPedalsProcessor +
//       editor here.  For I-0b ships a placeholder component.
//     - BaySickNAM/IR (sub-tab 1): hosts the existing BaySickNAMIRProcessor +
//       BaySickNAMIREditor unchanged (same setup as the Vox page's NAM/IR sub-tab).
//   * Spawn trigger remains the Mixer page's "Add Inst Strip" button.
// ─────────────────────────────────────────────────────────────────────────────

class InstPage : public juce::Component
{
public:
    // Back-compat shim: old saves wrote an EngineType int via selectEngine().
    // Phase I: only "None" is meaningful (no engine picker exists).  selectEngine
    // is now a no-op kept for save-format back-compat -- no projects in the wild
    // use this field, but the dispatch site in StandaloneEditor still calls it.
    enum class EngineType { None, BaySickPlayer, BaySickNAMIR };

    // K-2 (2026-05-05): Inst page source mode.  LiveInput is the classic Inst
    // tab - ASIO live input feeds Pedals → NAM/IR.  BaySickGuitars / BaySickBasses
    // (L-2) replace the live-input source with a per-page sfizz engine driven
    // from `Pattern::instRoll[mPageIndex]`.  Source-mode is set by the spawn
    // path (LiveInput for "+ Add Inst" ribbon entry; BaySickGuitars for
    // "+ Add BaySickGuitars" ribbon entry).
    enum class Source { LiveInput, BaySickGuitars, BaySickBasses };

    // QA-ModelShell TS1: the processor ref is needed at construction because
    // the Pedals + NAM/IR + chain trio is rig-owned and created in the ctor.
    InstPage (VibeSynthProcessor& proc, int pageIndex);
    ~InstPage() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    void switchTab    (int idx);
    int  getActiveTab () const noexcept { return mActiveTab; }

    // I-0b: tab labels surfaced through the PageMenuBar's setTabSlots.  Mirrors
    // VoxPage::getTabLabels() pattern.
    // K-3 (2026-05-05): tab list now depends on source mode.  LiveInput shows
    // the classic 2-tab layout (Pedals + NAM/IR).  sfizz sources add a "Piano
    // Roll" tab that nav-redirects to the unified PianoRollPage with this
    // page's engine selected.  K-5 will add a "Player" tab at the front for
    // the ARIA control panel.  Caller in StandaloneEditor uses the label
    // strings (not raw indices) to identify nav-redirect targets, so adding
    // tabs here doesn't require updating index-based dispatch.
    static juce::StringArray getTabLabels()
    {
        return { "BaySickPedals", "BaySickNAM/IR" };
    }
    juce::StringArray getActiveTabLabels() const
    {
        if (mSource == Source::LiveInput)
            return { "BaySickPedals", "BaySickNAM/IR" };
        // K-5 (2026-05-05): sfizz source.  Player sub-tab labeled with the
        // engine name sits leftmost (hosts the ARIA control panel); BaySickPedals
        // and BaySickNAM/IR follow; Piano Roll nav-redirect is the rightmost tab.
        const juce::String engineLabel =
            (mSource == Source::BaySickGuitars) ? "BaySickGuitars" : "BaySickBasses";
        return { engineLabel, "BaySickPedals", "BaySickNAM/IR", "Piano Roll" };
    }

    int          getPageIndex() const noexcept { return mPageIndex; }
    // 2026-04-28 (G-4): page accent matches the mixer Inst-bus colour
    // (`0xff1c3a8a` navy) so the ribbon tab + Mixer strip + page header
    // all read as the same channel identity.
    juce::Colour getPageColor() const noexcept { return juce::Colour (0xff1c3a8a); }

    // QA-E Task 4 (2026-05-12): getClipFilePath / setClipFilePath / mClipPath
    // + mLinkedClipPath deleted.  Inst file-association now lives in
    // PatternManager's AudioLibrary via pageOwnerChannelId tagging (per §9
    // 17th Forks entry).  mClipFileLabel is RETAINED -- still used for sfizz
    // kit name display by setSource() / updateSfizzKitLabel() paths.

    // Back-compat stub.  No-op in I-0b -- there's no engine picker; both engines
    // live as permanent sub-tabs.  Old StandaloneEditor save-load code calls this
    // from line 7796; keeping the symbol means we don't have to surgery that path
    // for I-0b.
    void selectEngine (EngineType /*e*/) {}
    EngineType getEngineType() const noexcept { return EngineType::None; }

    // Returns the BaySickNAM/IR processor (the only audio-thread-active engine
    // on the page in I-0b; BaySickPedals lands in I-1).  External callers
    // (Page Preset I/O, applyEngineState in state-load) use this.
    juce::AudioProcessor* getEngineProcessor() const noexcept;

    juce::AudioProcessor* getNamIrProcessor() const noexcept { return mNamIrProc; }
    juce::AudioProcessor* getPedalsProcessor() const noexcept { return mPedalsProc; }

    // K-2 (2026-05-05): source mode access + setter.  setSource rebuilds the
    // engine chain (sfizz front-end optional, then Pedals → NAM/IR) and fires
    // onSourceChanged so MixerPage can flip the strip's noLiveInput flag and
    // StandaloneEditor can update sub-tab visibility.  Caller must ensure the
    // PluginProcessor's BaySickGuitars / BaySickBasses engine exists before
    // calling setSource(...) with a non-LiveInput value (typically via
    // VibeSynthProcessor::loadBaySickGuitarsKit) - if the engine pointer is
    // null when the chain is rebuilt, the source effectively produces silence
    // until a kit loads.
    Source getSource() const noexcept { return mSource; }
    void   setSource (Source s);
    std::function<void(Source)> onSourceChanged;

    // K-6 (2026-05-05): per-program state cache serialization for project
    // save/load.  serializeProgramCache writes one <Program filename="..."/>
    // child per cached entry; restoreProgramCacheFromXml reads them back.
    // The cache holds each program's last APVTS state - losing it on save
    // would mean every program reverts to kit defaults on project reload
    // (per-session memory only).
    juce::XmlElement* serializeProgramCacheXml() const;       // caller takes ownership
    void              restoreProgramCacheFromXml (const juce::XmlElement& cacheXml);

    // K-2 (2026-05-05): re-pull the source engine pointer from PluginProcessor
    // and rebuild the chain.  Call after a kit-load creates the engine for the
    // first time so the chain picks up the new pointer.  Idempotent - calling
    // when nothing changed is harmless (just rebuilds the same stage list).
    void notifySourceEngineChanged();

    std::function<void()> onEngineDestroying;
    std::function<void()> onEngineChanged;

    void                setTabName (const juce::String& n);   // syncs the model tab's name (TS1)
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

    // J-6 EQ unification (2026-05-03): EQ accessors removed; pre-rack EQ on Effects page only.

    // Save/Load PAGE preset - entire InstPage state (BaySickNAM/IR currently;
    // I-1 adds BaySickPedals).  XML matches exportInstState format.
    void saveInstPagePreset();
    void loadInstPagePreset (const juce::File& xml);

    // ── G-7 (2026-04-29): Page Preset save/load (full chain) ─────────────────
    // Bus fallback: if a saved _sendTo references kInstBus2 / kInstBus3 and
    // those buses aren't active in the current project, the loader silently
    // substitutes kInstBus.
    void setProcessor (VibeSynthProcessor* p);
    void setBusActiveQuery (std::function<bool(int channelId)> q) { mBusActiveQuery = std::move (q); }
    void savePagePreset (std::function<void()> onSaved = {});
    void loadPagePreset (const juce::File& xml);
    void showPageActionsMenu (juce::Component* anchor);
    void requestDelete ();

private:
    // J-6 EQ unification (2026-05-03): buildEQTab removed.
    void layoutContent (juce::Rectangle<int> r);
    void showEngineContextMenu();
    // K-2: rebuild the EngineChainProcessor stage list based on mSource.
    // For LiveInput the chain is {Pedals, NAMIR}.  For BaySickGuitars it's
    // {Guitars, Pedals, NAMIR} where Guitars is queried from PluginProcessor
    // via getBaySickGuitars(mPageIndex).  Idempotent + safe to call repeatedly.
    void rebuildEngineChain();

    int                                          mPageIndex { 0 };
    int                                          mActiveTab { 0 };
    Source                                       mSource    { Source::LiveInput };
    juce::String                                 mTabName;
    bool                                         mLocked { false };

    // I-0b: page header label (informational).  Was previously parented to
    // mEnginePicker's row; the picker is gone but we keep the label for the
    // file path display.  Re-parented into PageMenuBar's right slot when the
    // page is visible (mirrors Vox).
    juce::Label                                  mClipFileLabel;

public:
    juce::Label* getClipFileLabel() noexcept { return &mClipFileLabel; }

private:
    // K-5 (2026-05-05): ARIA control panel + Player tab host.  Created in the
    // ctor with a null binding; setSource(BaySickGuitars/Basses) re-binds via
    // rebuildPlayerPanel() and loads the kit's GUI XML.  Visible only when
    // source != LiveInput AND active sub-tab == 0.
    std::unique_ptr<juce::Component>             mPlayerTab;
    std::unique_ptr<AriaControlPanel>            mAriaPanel;
    // K-5 UI fix (2026-05-05): replaced ComboBox with a TextButton labeled
    // "Load Guitar".  ComboBox always shows the selected item - Jeff wants
    // a stable "click here to pick a program" affordance with the actual
    // program name surfaced separately on the file-name label.
    std::unique_ptr<juce::TextButton>            mProgramButton;
    juce::String                                 mLastProgramFile; // currently-loaded SFZ filename
    void showProgramPickerMenu();
    // K-5 fix #4 (2026-05-05): per-program APVTS state cache so swapping
    // between programs preserves each one's tweaked CC values for the
    // session.  Keyed by program filename (e.g. "01-green_keyswitch.sfz").
    // Saved on outgoing program; restored on incoming program if present.
    // Project save (K-6) will persist this map alongside the kit path.
    std::map<juce::String, juce::ValueTree>      mProgramStateCache;
    void rebuildPlayerPanel();
    void rebuildProgramCombo();
    void onProgramComboChanged();

    // QA-G3Smoke Task 12 (G-14): CUT SELF + mode toggles hosted in the panel
    // title bar's formerly reserved trailing slots, bound to the sfizz
    // engine's <prefix>cutSelf / <prefix>cutSelfMode params.  Attachments are
    // reset at the top of every rebuildPlayerPanel so a source swap can never
    // leave them listening on a torn-down engine's APVTS.
    juce::TextButton mCutSelfBtn { "CUT SELF" };
    juce::TextButton mCutModeBtn { "SAME PITCH" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mCutSelfAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mCutModeAttach;

public:
    // K-5: PageMenuBar parks this button in extras-right when the page's
    // source is sfizz-driven.  Always reads "Load Guitar" - actual loaded
    // program surfaces on the clip-file label next to it.
    juce::TextButton* getProgramButton() const { return mProgramButton.get(); }

private:

    // QA-ModelShell TS1: the Pedals stage is rig-owned ({Inst, pageIndex}
    // ownedStages); non-owning view pointer here.
    juce::AudioProcessor*                        mPedalsProc { nullptr };
    std::unique_ptr<juce::AudioProcessorEditor>  mPedalsEditor;
    std::unique_ptr<juce::Component>             mPedalsPlaceholder;

    // QA-A Phase 4.4 (2026-05-09): the I-15 polish kHeaderRowH = 36 chrome
    // strip is gone.  mPedalsHeaderTitle + mPedalsPresetBtn deleted along
    // with it; the pedalboard preset button migrated into BaySickPedalsEditor's
    // own title bar (trailing area), wired via onPedalboardPresetMenu callback
    // hooked from this page in the constructor.  showPedalboardPresetMenu()
    // stays as the popup-builder; the BaySickPedalsEditor preset button just
    // routes back into it via the callback.
    void                                         showPedalboardPresetMenu();

    // QA-ModelShell TS1: the NAM/IR stage is rig-owned; non-owning view.
    juce::AudioProcessor*                        mNamIrProc { nullptr };
    std::unique_ptr<juce::AudioProcessorEditor>  mNamIrEditor;

    // I-16 G-9 (2026-05-03): chain wrapper -- the registered engine; its
    // processBlock fans the buffer through Pedals -> NAM/IR.  QA-ModelShell
    // TS1: rig-owned (the tab's registered engine); non-owning view here.
    EngineChainProcessor*                        mChain { nullptr };

    // J-6 EQ unification (2026-05-03): mEQDisplay removed.
    // QA-E Task 4 (2026-05-12): mLinkedClipPath removed (was scaffold for
    // commitRecordingResult; never had any callers).  Replaced by library-
    // driven ownership via AudioLibraryEntry.pageOwnerChannelId.

    // G-7: full processor + bus-active query for Page Preset save/load.
    VibeSynthProcessor*                          mFullProcessor { nullptr };
    std::function<bool(int)>                     mBusActiveQuery;

    // G-7 (2026-04-29): listener-based dirty tracking - see ClipsPage for
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

