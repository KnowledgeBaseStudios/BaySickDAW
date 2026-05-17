#pragma once
#include <JuceHeader.h>
#include "../Standalone/SharedUI.h"   // ParametricEQDisplay

class VibeSynthProcessor;

// ─────────────────────────────────────────────────────────────────────────────
// ClipsPage - host component for one Clips tab (Phase G-2).
// ─────────────────────────────────────────────────────────────────────────────
// One Clips tab = one BaySickPlayer instance.  Clips is a sampler-style page;
// piano-roll notes trigger the loaded WAV.  No engine picker exists - Clips
// has only one engine choice, so the player auto-instantiates on tab spawn.
// (G-6 cleanup, 2026-04-29: removed BaySickNAM/IR from Clips per Jeff's
// "Clips is essentially a sample player" - NAM/IR belongs on the Inst page.)
//
// The page itself can ONLY be spawned via dragging or uploading a clip onto
// the Builder grid or the Clips empty-state placeholder; the ribbon's Clip
// dropdown is an instance switcher only (no `+ Add` entry).
//
// Sub-tabs mirror Layer/Bass shape - Player / Piano Roll / Pre EQ8 M/S - but
// only Player is locally rendered.  Piano Roll redirects to PianoRollPage
// via the StandaloneEditor's PageMenuBar setTabSlots wiring; Pre EQ8 M/S is
// a placeholder until later polish.
// ─────────────────────────────────────────────────────────────────────────────

class ClipsPage : public juce::Component
{
public:
    // G-6 (2026-04-29): EngineType kept for compatibility with existing call
    // sites (importClipState reads/writes the active type).  Only valid
    // values are None and BaySickPlayer.
    enum class EngineType { None, BaySickPlayer };

    explicit ClipsPage (int pageIndex);
    ~ClipsPage() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    // ── Sub-tab control (driven by StandaloneEditor's PageMenuBar pills) ─────
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
    void         setClipFilePath (const juce::String& p,
                                  const juce::String& libraryPath = {});

    // ── Engine accessors ─────────────────────────────────────────────────────
    // selectEngine remains as the activation entry-point so spawnClipsTabIfMissing
    // can fire onEngineChanged AFTER the editor's callbacks are wired.  Only
    // BaySickPlayer is supported; passing None is a no-op.
    void          selectEngine (EngineType e);
    EngineType    getEngineType() const noexcept { return mEngineType; }
    juce::AudioProcessor* getEngineProcessor() const noexcept;

    // G-3 (2026-04-28): fired BEFORE the active engine swaps so the editor
    // can unregisterClipEngine while the OLD pointer is still valid.
    std::function<void()> onEngineDestroying;
    std::function<void()> onEngineChanged;

    // ── Tab name (for ribbon rename) ─────────────────────────────────────────
    void                setTabName (const juce::String& n) { mTabName = n; repaint(); }
    const juce::String& getTabName () const                 { return mTabName; }

    // ── G-6 (2026-04-29): full-state export/import for Duplicate flow ────────
    // Captures BaySickPlayer's APVTS state (single-engine page now).  Saves
    // the actual engine prefix in the XML so import does proper substitution
    // even though VibePlayerProcessor's prefix format is composite
    // ("tk_<trackId>_bsp_" with double-underscore artifact when trackId
    // already ends in "_").
    juce::String exportClipState() const;
    void         importClipState (const juce::String& xml);

    // ── G-6 (2026-04-29): right-click context menu on the engine picker ──────
    // Wired by StandaloneEditor.  Mirrors the Layers/Bass/Drums right-click
    // context menus on their LockableCombo sound-pickers.
    std::function<void()>                        onDuplicateRequested;
    std::function<void()>                        onRenameRequested;
    std::function<void()>                        onDeleteRequested;
    std::function<void()>                        onLockChanged;        // editor wires to ribbon setTabLocked
    // Choke Group: editor reads/writes the mixer_audio_<pageIdx>_chokeGroup
    // APVTS param on the main VibeSynthProcessor.  ClipsPage doesn't carry
    // a reference to that processor so the menu queries via these callbacks.
    std::function<int()>                         onGetChokeGroup;
    std::function<void(int)>                     onSetChokeGroup;

    // Lock - protects tab from delete (sync'd to ribbon [L] prefix).
    bool isLocked() const noexcept { return mLocked; }
    void setLocked (bool b) { if (b == mLocked) return; mLocked = b; if (onLockChanged) onLockChanged(); repaint(); }

    // J-6 EQ unification (2026-05-03): EQ sub-tab + accessors removed.  Pre-rack
    // EQ for this Audio insert is exclusively edited via the Effects page
    // (mixer_audio_<row>_preeq_*).

    // Save / Load page preset - writes the entire ClipPageState XML to
    // Documents/BaySickDAW/Presets/Clips/My Presets/<name>.xml.  Load Preset
    // walks the same folder + Factory subfolders.
    void savePatchAs();
    void loadPreset (const juce::File& xml);

    // ── G-7 (2026-04-29): Page Preset save/load (full chain) ─────────────────
    // savePagePreset writes engine + strip params + insert rack + post-EQ to
    // Documents/BaySickDAW/Presets/Clip Page/My Presets/.  Requires
    // setProcessor() to have been called with the global VibeSynthProcessor;
    // otherwise falls back to the engine-only savePatchAs.
    void setProcessor (VibeSynthProcessor* p) { mFullProcessor = p; }
    void savePagePreset (std::function<void()> onSaved = {});
    void loadPagePreset (const juce::File& xml);
    void showPageActionsMenu (juce::Component* anchor);
    // G-7: tab-close prompt (replaces the inline AlertWindow that the
    // editor was firing).  Calls onDeleteRequested after confirmation;
    // 3-button "Save Page Preset & Delete" when player state is dirty.
    void requestDelete ();

private:
    void buildEnginePicker();
    void layoutEditor (juce::Rectangle<int> r);
    void showEngineContextMenu();

    // G-6 ComboBox subclass that fires onRightClick instead of opening the
    // dropdown when right-clicked.  Locked to BaySickPlayer (the only option)
    // so left-click is informational only.
    class LockedClipsEngineCombo : public juce::ComboBox
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

    LockedClipsEngineCombo                       mEnginePicker;
    juce::Label                                  mClipFileLabel;
    EngineType                                   mEngineType { EngineType::None };
    std::unique_ptr<juce::AudioProcessor>        mPlayerProc;        // VibePlayerProcessor
    std::unique_ptr<juce::AudioProcessorEditor>  mPlayerEditor;

    // J-6 EQ unification (2026-05-03): mEQDisplay removed; pre-rack EQ on
    // Effects page only.

    // G-7 (2026-04-29): set by StandaloneEditor after construction so we
    // can call PagePresetIO with the global apvts + VibeGraph.
    VibeSynthProcessor*                          mFullProcessor { nullptr };

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

// ─────────────────────────────────────────────────────────────────────────────
// ClipsEmptyState - shown when the Clips ribbon slot is clicked with zero
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
