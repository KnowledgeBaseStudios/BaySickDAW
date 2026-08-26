#pragma once
#include <JuceHeader.h>
#include "../Standalone/SharedUI.h"

class BaySickDAWProcessor;

// ─────────────────────────────────────────────────────────────────────────────
// ClipsPage - host component for one Clips tab (Phase G-2).
// ─────────────────────────────────────────────────────────────────────────────
// One Clips tab = one BaySickPlayer instance.  Clips is a sampler-style page;
// piano-roll notes trigger the loaded WAV.  No engine picker exists - Clips
// has only one engine choice, so the player auto-instantiates on tab spawn.
// (G-6 cleanup, 2026-04-29: removed BaySickNAM/IR from Clips per Jeff's
// "Clips is essentially a sample player" - NAM/IR belongs on the Inst page.)
//
// Spawn routes: a clip dropped or imported onto the Builder grid, the ribbon
// "+" slot (BaySickPlayer > Audio Clips), or the Clip dropdown's
// "+ Add BaySickPlayer..." row.  Both ribbon routes open the same audio-file
// picker -- a Clips page is born from a file, never from an engine pick.
//
// Views mirror Layer/Bass shape - Player / Piano Roll - but only Player is
// locally rendered.  Piano Roll redirects to PianoRollPage via the Menu
// dropdown's nav entries (QA-Layout T15; the strip buttons are gone).
// ─────────────────────────────────────────────────────────────────────────────

class ClipsPage : public juce::Component
{
public:
    // G-6 (2026-04-29): EngineType kept for compatibility with existing call
    // sites (importClipState reads/writes the active type).  Only valid
    // values are None and BaySickPlayer.
    //
    // Ordinals are pinned explicitly because this int is persisted: the project
    // writer stores (int) getEngineType() and the loader casts the saved int
    // straight back, and selectEngine consumes the value in live control flow
    // (an unrecognized ordinal tears the engine down and never rebuilds it,
    // leaving a silent Clips tab).  NEVER reorder or insert; append only, with
    // an explicit value.  Same rule as EffectType in EffectRack.h.
    enum class EngineType { None = 0, BaySickPlayer = 1 };

    explicit ClipsPage (int pageIndex);
    ~ClipsPage() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    // ── Sub-tab control (driven by the window Menu dropdown's nav entries) ───
    void switchTab    (int idx);
    int  getActiveTab () const noexcept { return mActiveTab; }

    // ── Page metadata ────────────────────────────────────────────────────────
    int          getPageIndex() const noexcept { return mPageIndex; }
    juce::Colour getPageColor() const noexcept { return juce::Colour (0xffd4a017); }

    // ── Clip path access (set by drop-spawn flow) ────────────────────────────
    juce::String getClipFilePath() const                    { return mClipPath; }
    // QA-E Task 7 (FILE-02) root-cause fix: the engine must load from a
    // RESOLVED ABSOLUTE path (p), but the audio-library tag must use the
    // STORED/RELATIVE path so it matches every other library entry (blocks,
    // browser walk, PluginProcessor all compare stored paths; addAudioTo
    // Library dedups by exact string).  libraryPath is that stored form;
    // when empty it falls back to p (preserves behavior for callers that
    // already pass a stored path).
    //
    // interactive == false suppresses the per-clip "nothing playable" alert:
    // the project-restore path already routes both the unresolvable and the
    // present-but-undecodable case into MissingFileReport, which drains once
    // per load into a single batched dialog.  A restore that also alerted here
    // would stack one box per clip on top of that batch.  Every user gesture
    // (drop, spawn, preset apply) keeps the default and alerts immediately.
    void         setClipFilePath (const juce::String& p,
                                  const juce::String& libraryPath = {},
                                  bool interactive = true);

    // ── Engine accessors ─────────────────────────────────────────────────────
    // selectEngine remains as the activation entry-point so spawnClipsTabIfMissing
    // can fire onEngineChanged AFTER the editor's callbacks are wired.  Only
    // BaySickPlayer is supported; passing None is a no-op.
    void          selectEngine (EngineType e);
    EngineType    getEngineType() const noexcept { return mEngineType; }
    juce::AudioProcessor* getEngineProcessor() const noexcept;

    // QA-Layout T3 (Window-3/4): title-strip chrome for the hosted engine --
    // see LayersPage.h for the contract.
    juce::String     stripEngineTitle()  const;
    juce::Colour     stripEngineAccent() const;
    juce::Component* stripPresetButton() const;
    std::function<void()> onEngineEditorRebuilt;

    std::function<void()> onEngineChanged;

    // ── Tab name (for ribbon rename) ─────────────────────────────────────────
    void                setTabName (const juce::String& n);
    const juce::String& getTabName () const                 { return mTabName; }

    // ── G-6 (2026-04-29): full-state export/import for Duplicate flow ────────
    // Captures BaySickPlayer's APVTS state (single-engine page now).  Saves
    // the actual engine prefix in the XML so import does proper substitution
    // even though BaySickPlayerProcessor's prefix format is composite
    // ("tk_<trackId>_bsp_" with double-underscore artifact when trackId
    // already ends in "_").
    juce::String exportClipState() const;
    void         importClipState (const juce::String& xml);

    // ── Menu-dropdown action callbacks (wired by StandaloneEditor) ───────────
    // QA-Layout T2: these served the picker's right-click context menu; the
    // picker is gone and the same entries live on showPageActionsMenu.
    std::function<void()>                        onDuplicateRequested;
    std::function<void()>                        onRenameRequested;
    std::function<void()>                        onDeleteRequested;
    std::function<void()>                        onLockChanged;        // editor wires to ribbon setTabLocked
    // Choke Group: editor reads/writes the mixer_audio_<pageIdx>_chokeGroup
    // APVTS param on the main BaySickDAWProcessor.  ClipsPage doesn't carry
    // a reference to that processor so the menu queries via these callbacks.
    std::function<int()>                         onGetChokeGroup;
    std::function<void(int)>                     onSetChokeGroup;

    // Lock - protects tab from delete (sync'd to ribbon [L] prefix).
    // setLocked is the raw applier: restore paths (project load, importClipState,
    // page preset) and undo/redo replay use it so they bank no transaction.
    // setLockedUndoable is the USER gesture - it applies and banks one.
    bool isLocked() const noexcept { return mLocked; }
    void setLocked (bool b) { if (b == mLocked) return; mLocked = b; if (onLockChanged) onLockChanged(); repaint(); }
    void setLockedUndoable (bool wantLocked);

    // J-6 EQ unification (2026-05-03): EQ sub-tab + accessors removed.  Pre-rack
    // EQ for this Audio insert is exclusively edited via the Effects page
    // (mixer_audio_<row>_preeq_*).

    // Save / Load page preset - writes the entire ClipPageState XML to
    // Documents/BaySickDAW/Presets/Clips/My Presets/<name>.xml.  Load Preset
    // walks the same folder + Factory subfolders.
    void savePatchAs (std::function<void()> onSaved = {});
    void loadPreset (const juce::File& xml);

    // ── G-7 (2026-04-29): Page Preset save/load (full chain) ─────────────────
    // savePagePreset writes engine + strip params + insert rack + post-EQ to
    // Documents/BaySickDAW/Presets/Clip Page/My Presets/.  Requires
    // setProcessor() to have been called with the global BaySickDAWProcessor;
    // otherwise falls back to the engine-only savePatchAs.
    void setProcessor (BaySickDAWProcessor* p);   // also creates the model tab (TS1)
    void savePagePreset (std::function<void()> onSaved = {});
    void loadPagePreset (const juce::File& xml);
    void showPageActionsMenu (juce::Component* anchor);
    // QA-Layout T15: the title strip's nav buttons dissolved into the Menu
    // dropdown -- the editor injects the window/view entries at the top of
    // the page-actions popup through this hook.
    std::function<void(juce::PopupMenu&)> onBuildWindowNavMenu;
    // G-7: tab-close prompt (replaces the inline AlertWindow that the
    // editor was firing).  Calls onDeleteRequested after confirmation;
    // 3-button "Save Page Preset & Delete" when player state is dirty.
    void requestDelete ();

private:
    void layoutEditor (juce::Rectangle<int> r);

    int                                          mPageIndex { 0 };
    int                                          mActiveTab { 0 };
    juce::String                                 mTabName;
    juce::String                                 mClipPath;
    bool                                         mLocked { false };

    EngineType                                   mEngineType { EngineType::None };
    // QA-ModelShell TS1: non-owning view of the rig-owned BaySickPlayerProcessor
    // ({Clips, pageIndex}); the editor IS view-owned and must die before the
    // page (its attachments reference the engine's APVTS).
    juce::AudioProcessor*                        mPlayerProc { nullptr };
    std::unique_ptr<juce::AudioProcessorEditor>  mPlayerEditor;

    // J-6 EQ unification (2026-05-03): mEQDisplay removed; pre-rack EQ on
    // Effects page only.

    // G-7 (2026-04-29): set by StandaloneEditor after construction so we
    // can call PagePresetIO with the global apvts + BaySickGraph.
    BaySickDAWProcessor*                          mFullProcessor { nullptr };

    // G-7 dirty tracking - listener-based instead of byte comparison so it
    // catches every parameter mutation reliably regardless of how the
    // engine's getStateInformation is implemented.  Attached to the engine
    // apvts.state when the engine is created; detached on engine destroy.
    // mSuppressDirty is set during setStateInformation paths so import flows
    // don't mark the freshly-loaded state as dirty.
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
    void takeStateSnapshot();   // resets mPageDirty
    bool isPatchDirty() const { return mPageDirty; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipsPage)
};

