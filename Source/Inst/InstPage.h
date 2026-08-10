#pragma once
#include <JuceHeader.h>
#include "../Standalone/SharedUI.h"   // ParametricEQDisplay
#include "../Standalone/EngineChainProcessor.h"   // I-16 G-9: Pedals -> NAM/IR chain
#include "../Standalone/UndoActions.h"            // QA-UndoCoverage Task 7: UndoContext + StructuralOpAction

class BaySickDAWProcessor;
class AriaControlPanel;

// ─────────────────────────────────────────────────────────────────────────────
// InstPage - host component for one Inst tab.
// ─────────────────────────────────────────────────────────────────────────────
// I-0b (2026-05-02): Restructured for Phase I.
//   * BaySickPlayer engine REMOVED entirely (no projects in the wild had it).
// Inst hosts BaySickPedalsProcessor + BaySickNAMIRProcessor DIRECTLY, as two
// rig-owned stages of the page's EngineChainProcessor (order Pedals -> NAM/IR,
// with an optional sfizz front-end for the BaySickGuitars / BaySickBasses
// sources).  There is no engine picker - the trio is permanent for the tab.
// This is NOT the Vox arrangement: VoxPage owns no NAM/IR at all --
// BaySickVocalProcessor embeds one and drives it from inside its own
// processBlock -- so a NAM/IR change on one page does not carry to the other.
//   * Spawned BY its mixer strip: addInstChannelAtIndex fires onInstStripAdded,
//     which spawns this page.  Every entry point -- the Mixer's Add menu, the
//     ribbon "+", a project load -- goes through that one call.
// ─────────────────────────────────────────────────────────────────────────────

class InstPage : public juce::Component
{
public:
    // Back-compat shim: old saves wrote an EngineType int via selectEngine().
    // Phase I: only "None" is meaningful (no engine picker exists).  selectEngine
    // is now a no-op kept for save-format back-compat -- no projects in the wild
    // use this field, but the dispatch site in StandaloneEditor still calls it.
    // Persisted as a raw int: NEVER reorder or insert; append only, with an
    // explicit value.  Same rule as EffectType in EffectRack.h.
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
    InstPage (BaySickDAWProcessor& proc, int pageIndex);
    ~InstPage() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    // QA-Layout T4 (Window-7/L10): sub-tab switching is retired.  A sfizz
    // tab's window shows the Aria player; Pedals + NAM/IR live in contained
    // windows hosted NON-OWNED through these accessors.  A LiveInput tab has
    // NO page window at all -- the pedals window IS its player.
    juce::Component* getPedalsEditorComponent() const noexcept { return mPedalsEditor.get(); }
    juce::Component* getNamIrEditorComponent()  const noexcept { return mNamIrEditor.get(); }

    int          getPageIndex() const noexcept { return mPageIndex; }
    // 2026-04-28 (G-4): page accent matches the mixer Inst-bus colour
    // (`0xff1c3a8a` navy) so the ribbon tab + Mixer strip + page header
    // all read as the same channel identity.
    juce::Colour getPageColor() const noexcept { return juce::Colour (0xff1c3a8a); }

    // QA-E Task 4 (2026-05-12): getClipFilePath / setClipFilePath / mClipPath
    // + mLinkedClipPath deleted.  Inst file-association now lives in
    // PatternManager's AudioLibrary via pageOwnerChannelId tagging (per §9
    // 17th Forks entry).  mClipFileLabel is RETAINED for the sfizz program
    // display on the Aria bar (its live-input strip mount died in QA-Layout
    // T3 / L23).

    // Back-compat stub.  No-op in I-0b -- there's no engine picker; both engines
    // live as permanent sub-tabs.  Old StandaloneEditor save-load code calls this
    // from line 7796; keeping the symbol means we don't have to surgery that path
    // for I-0b.
    void selectEngine (EngineType /*e*/) {}
    EngineType getEngineType() const noexcept { return EngineType::None; }

    // Returns the EngineChainProcessor -- the single processor registered as this
    // tab's engine, whose processBlock drives the whole stage list in order.  A
    // caller that wants one specific stage must use getNamIrProcessor() /
    // getPedalsProcessor(); downcasting this to a stage type yields null.
    juce::AudioProcessor* getEngineProcessor() const noexcept;

    juce::AudioProcessor* getNamIrProcessor() const noexcept { return mNamIrProc; }
    juce::AudioProcessor* getPedalsProcessor() const noexcept { return mPedalsProc; }

    // K-2 (2026-05-05): source mode access + setter.  setSource rebuilds the
    // engine chain (sfizz front-end optional, then Pedals -> NAM/IR).  Caller
    // must ensure the PluginProcessor's BaySickGuitars / BaySickBasses engine
    // exists before calling setSource(...) with a non-LiveInput value
    // (typically via BaySickDAWProcessor::loadBaySickGuitarsKit) - if the engine
    // pointer is null when the chain is rebuilt, the source effectively
    // produces silence until a kit loads.
    Source getSource() const noexcept { return mSource; }
    void   setSource (Source s);

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

    // QA-UndoCoverage Task 7: program pick = one structural transaction
    // (undo re-enters the prior program through the session CC-state cache).
    void setUndoContext (const UndoContext& ctx) { mUndoCtx = ctx; }
    void switchSfizzProgramWithUndo (const juce::File& target);

    void                setTabName (const juce::String& n);
    const juce::String& getTabName () const                 { return mTabName; }

    // Jeff, 2026-08-05: the pedals window's Standard/Compact view lives on the
    // PAGE, not the editor -- the editor is destroyed and rebuilt (source swap,
    // window close) and the mode has to outlive it, and the project save needs
    // somewhere to read it from when no window is open at all.
    bool isPedalsCompact() const noexcept   { return mPedalsCompact; }
    void setPedalsCompact (bool c) noexcept { mPedalsCompact = c; }

    // ── G-6 (2026-04-29): full-state export/import for Duplicate flow ────────
    juce::String exportInstState() const;
    void         importInstState (const juce::String& xml);

    // ── G-6 (2026-04-29): right-click engine-picker context menu callbacks ─
    std::function<void()>                        onDuplicateRequested;
    std::function<void()>                        onRenameRequested;
    std::function<void()>                        onDeleteRequested;
    std::function<void()>                        onLockChanged;

    // Fired when the substituted-kit display marker flips.  StandaloneEditor
    // mirrors it onto the ribbon tab + the mixer strip.  Unlike onLockChanged
    // this has NO persistence hook -- the marker is not project state.
    std::function<void()>                        onKitMissingChanged;

    // Program-name linkage (2026-08-02): fired with the loaded program's
    // display name from the INTERACTIVE paths only (picker + the add-tab
    // default-kit autoload) -- project restore never fires it, so a saved
    // tab name survives the reload.
    std::function<void(const juce::String&)>     onSoundNameChanged;

    // Program display name ("03-Clean_Chorus.sfz" -> "Clean Chorus").  Public
    // so the add-tab autoload path can name the tab from the default kit.
    static juce::String prettyProgramName (const juce::File&);

    bool isLocked() const noexcept { return mLocked; }
    void setLocked (bool b) { if (b == mLocked) return; mLocked = b; if (onLockChanged) onLockChanged(); repaint(); }
    // Every action undoable: the Menu's Lock entry rides one structural
    // transaction.  Skips the wrap when no undo context is wired.
    void toggleLockUndoable();

    // Display-only: the project's saved kit was gone at restore and a default
    // kit was substituted (or the kit failed to load), so the tab is labeled
    // one instrument while playing another.  RUNTIME ONLY -- deliberately
    // absent from exportInstState / serializeTabsInto / captureTabRecord: a
    // persisted marker would survive reinstalling the kit forever.
    bool isKitMissing() const noexcept { return mKitMissing; }
    void setKitMissing (bool b)
    {
        if (b == mKitMissing) return;
        mKitMissing = b;
        if (onKitMissingChanged) onKitMissingChanged();
        repaint();
    }

    // J-6 EQ unification (2026-05-03): EQ accessors removed; pre-rack EQ on Effects page only.

    // Save/Load PAGE preset - entire InstPage state (BaySickNAM/IR currently;
    // I-1 adds BaySickPedals).  XML matches exportInstState format.
    void saveInstPagePreset();
    void loadInstPagePreset (const juce::File& xml);

    // ── G-7 (2026-04-29): Page Preset save/load (full chain) ─────────────────
    void setProcessor (BaySickDAWProcessor* p);
    // NOT CONSULTED TODAY.  The bus fallback this query was registered for
    // (saved _sendTo names kInstBus2 / kInstBus3, those buses are not active,
    // loader substitutes kInstBus) only exists in PagePresetIO's legacy
    // per-strip overload.  InstPage::loadPagePreset goes through the
    // PageChainConfig overload instead, which has no isChannelActive field and
    // hardcodes "every channel is active", so no substitution happens on an
    // Inst preset load.  Nothing is broken by that -- MixerPage self-activates
    // a secondary bus it finds a strip routed to -- but do not build Inst
    // routing behavior on this query until it is either threaded through the
    // config or removed outright.
    void setBusActiveQuery (std::function<bool(int channelId)> q) { mBusActiveQuery = std::move (q); }
    void savePagePreset (std::function<void()> onSaved = {});
    void loadPagePreset (const juce::File& xml);
    void showPageActionsMenu (juce::Component* anchor);
    // QA-Layout T15: the title strip's nav buttons dissolved into the Menu
    // dropdown -- the editor injects the window/view entries at the top of
    // the page-actions popup through this hook.
    std::function<void(juce::PopupMenu&)> onBuildWindowNavMenu;
    void requestDelete ();

private:
    // J-6 EQ unification (2026-05-03): buildEQTab removed.
    void layoutContent (juce::Rectangle<int> r);
    // K-2: rebuild the EngineChainProcessor stage list based on mSource.
    // For LiveInput the chain is {Pedals, NAMIR}.  For BaySickGuitars it's
    // {Guitars, Pedals, NAMIR} where Guitars is queried from PluginProcessor
    // via getBaySickGuitars(mPageIndex).  Idempotent + safe to call repeatedly.
    void rebuildEngineChain();

    int                                          mPageIndex { 0 };
    Source                                       mSource    { Source::LiveInput };
    juce::String                                 mTabName;
    bool                                         mLocked { false };
    bool                                         mKitMissing { false };

    // sfizz program display -- setSource()/updateSfizz paths drive it, and it
    // sits on the AriaControlPanel title bar.  QA-Layout T3 (L23): the
    // live-input PageMenuBar mount is gone (nothing ever updated it there,
    // so it read "(no audio loaded)" forever).
    juce::Label                                  mClipFileLabel;

private:
    // K-5 (2026-05-05): ARIA control panel + Player tab host.  Created in the
    // ctor with a null binding; setSource(BaySickGuitars/Basses) re-binds via
    // rebuildPlayerPanel() and loads the kit's GUI XML.  QA-Layout T4: the
    // page's only content -- visible whenever source != LiveInput.
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
    UndoContext                                  mUndoCtx;
    bool switchSfizzProgram (const juce::File& target);
    void rebuildPlayerPanel();

    // QA-G3Smoke Task 12 (G-14): CUT SELF + mode toggles hosted in the panel
    // title bar's formerly reserved trailing slots, bound to the sfizz
    // engine's <prefix>cutSelf / <prefix>cutSelfMode params.  Attachments are
    // reset at the top of every rebuildPlayerPanel so a source swap can never
    // leave them listening on a torn-down engine's APVTS.
    bool mPedalsCompact { false };   // see isPedalsCompact
    juce::TextButton mCutSelfBtn { "CUT SELF" };
    juce::TextButton mCutModeBtn { "SAME PITCH" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mCutSelfAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mCutModeAttach;

public:
    // K-5: PageMenuBar parks this button in extras-right when the page's
    // source is sfizz-driven.  Label tracks the source mode ("Load Guitar" /
    // "Load Bass" / "Load Inst"); the actual loaded program surfaces on the
    // clip-file label next to it.
    juce::TextButton* getProgramButton() const { return mProgramButton.get(); }

    // QA-Layout T15: the sfizz AriaControlPanel title bar is dissolved.  The
    // program button + clip-file label it hosted mount on the hosting window's
    // title strip (StandaloneEditor wires them per page-show); the cut-self
    // pair mounts on the player itself (Jeff, 2026-08-04).
    juce::Label*      getClipFileLabel() { return &mClipFileLabel; }

private:

    // QA-ModelShell TS1: the Pedals stage is rig-owned ({Inst, pageIndex}
    // ownedStages); non-owning view pointer here.
    juce::AudioProcessor*                        mPedalsProc { nullptr };
    std::unique_ptr<juce::AudioProcessorEditor>  mPedalsEditor;

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

    // G-7: full processor for Page Preset save/load.  mBusActiveQuery is
    // registered by the editor and read by nothing -- see setBusActiveQuery.
    BaySickDAWProcessor*                          mFullProcessor { nullptr };
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

