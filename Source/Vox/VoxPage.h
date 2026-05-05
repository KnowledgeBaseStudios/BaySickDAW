#pragma once
#include <JuceHeader.h>
#include "../Standalone/SharedUI.h"   // ParametricEQDisplay

class VibeSynthProcessor;
class BaySickVocalEditor;

// ─────────────────────────────────────────────────────────────────────────────
// VoxPage — host component for one Vox tab (Phase G-4).
// ─────────────────────────────────────────────────────────────────────────────
// Mirror of InstPage with Vox-specific colour + naming.  Engine list for V1
// is just BaySickPlayer (sample playback of recorded vocal).  Phase H adds
// `BaySickVocal` (the dedicated vocal channel-strip processor) as the
// preferred default option.  Spawn trigger is the Mixer page's "Add Vox
// Strip" button — ribbon Vox dropdown is an instance switcher only.
// ─────────────────────────────────────────────────────────────────────────────

class VoxPage : public juce::Component,
                public juce::FileDragAndDropTarget
{
public:
    // 2026-04-29 DEBUG: drag-drop a WAV/MP3 onto the page to load it into the
    // BaySickPlayer engine (auto-instantiated if not yet picked).  Lets Jeff
    // test whether the audio-routing bug on Clips also affects Vox/Inst.
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped           (const juce::StringArray& files, int x, int y) override;

    // H-6b (2026-05-01): Vox tabs are always BaySickVocal — no engine picker.
    // EngineType retained for save/load back-compat but only BaySickVocal is
    // valid going forward.  Old projects with BaySickPlayer state silently
    // discard it on load (BaySickPlayer is no longer a Vox option).
    enum class EngineType { None, BaySickPlayer, BaySickVocal };

    explicit VoxPage (int pageIndex);
    ~VoxPage() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    void switchTab    (int idx);
    int  getActiveTab () const noexcept { return mActiveTab; }

    // H-6b (2026-05-01) / J-6 (2026-05-03): tab labels surfaced through the
    // PageMenuBar's setTabSlots.  Pre Rack EQ removed in J-6 EQ unification.
    static juce::StringArray getTabLabels()
    {
        return { "BaySickVocals", "Vocal Chain", "BaySickPitch",
                 "BaySickAlign",  "BaySickNAM/IR" };
    }

    int          getPageIndex() const noexcept { return mPageIndex; }
    // 2026-04-28 (G-4): page accent matches the mixer Vox-bus colour
    // (`0xff0fafa5` teal) so the ribbon tab + Mixer strip + page header
    // all read as the same channel identity.
    juce::Colour getPageColor() const noexcept { return juce::Colour (0xff0fafa5); }

    juce::String getClipFilePath() const                    { return mClipPath; }
    void         setClipFilePath (const juce::String& p);

    // I-16 G-9 (2026-05-03): linked recorded-clip file path (the wet audio
    // that plays back through this page's chain on transport playback).
    // Set by StandaloneEditor::commitRecordingResult after a successful
    // armed Vox take (always the WET file per Option C of the spec).
    juce::String getLinkedClipPath() const                  { return mLinkedClipPath; }
    void         setLinkedClipPath (const juce::String& p)  { mLinkedClipPath = p; }

    // H-6b (2026-05-01): expose the clip-name label so StandaloneEditor can
    // re-parent it into the PageMenuBar's far-right slot when this page is
    // visible.  Label updates via setClipFilePath stay live across reparenting.
    juce::Label* getClipFileLabel() noexcept { return &mClipFileLabel; }

    void          selectEngine (EngineType e);
    EngineType    getEngineType() const noexcept { return mEngineType; }
    juce::AudioProcessor* getEngineProcessor() const noexcept;

    std::function<void()> onEngineDestroying;
    std::function<void()> onEngineChanged;

    void                setTabName (const juce::String& n) { mTabName = n; repaint(); }
    const juce::String& getTabName () const                 { return mTabName; }

    // ── G-6 (2026-04-29): full-state export/import for Duplicate flow ────────
    juce::String exportVoxState() const;
    void         importVoxState (const juce::String& xml);

    // ── G-6 (2026-04-29): right-click engine-picker context menu callbacks ─
    std::function<void()>                        onDuplicateRequested;
    std::function<void()>                        onRenameRequested;
    std::function<void()>                        onDeleteRequested;
    std::function<void()>                        onLockChanged;

    bool isLocked() const noexcept { return mLocked; }
    void setLocked (bool b) { if (b == mLocked) return; mLocked = b; if (onLockChanged) onLockChanged(); repaint(); }

    // J-6 EQ unification (2026-05-03): EQ accessors removed; pre-rack EQ on Effects page only.

    // Save/Load PAGE preset — entire VoxPage state (currently just BaySickPlayer;
    // Phase H adds BaySickVocal alongside).  XML matches exportVoxState format.
    void saveVoxPagePreset();
    void loadVoxPagePreset (const juce::File& xml);

    // ── G-7 (2026-04-29): Page Preset save/load (full chain) ─────────────────
    // Captures engine + strip params + insert rack + post-EQ.  setProcessor
    // must be called by StandaloneEditor with the global VibeSynthProcessor.
    // Bus fallback: if a saved _sendTo references kVoxBus2 and that bus isn't
    // active in the current project, the loader silently substitutes kVoxBus.
    void setProcessor (VibeSynthProcessor* p);
    void setBusActiveQuery (std::function<bool(int channelId)> q) { mBusActiveQuery = std::move (q); }
    void savePagePreset (std::function<void()> onSaved = {});
    void loadPagePreset (const juce::File& xml);
    void showPageActionsMenu (juce::Component* anchor);
    void requestDelete ();

private:
    void buildEnginePicker();
    void layoutEditor (juce::Rectangle<int> r);
    juce::AudioProcessorEditor* activeEditor() const;
    void showEngineContextMenu();

    // G-6: ComboBox subclass that fires onRightClick on right-button click
    // (left-click still opens the dropdown for engine selection).
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
    std::unique_ptr<juce::AudioProcessor>        mPlayerProc;        // VibePlayerProcessor
    std::unique_ptr<juce::AudioProcessor>        mVocalProc;         // BaySickVocal (Phase H — null until then)

    // I-16 G-9 (2026-05-03): file path of the wet recording linked to this
    // page's strip.  Set by commitRecordingResult after a successful take.
    juce::String                                 mLinkedClipPath;
    std::unique_ptr<juce::AudioProcessorEditor>  mPlayerEditor;
    std::unique_ptr<juce::AudioProcessorEditor>  mVocalEditor;

    // J-6 EQ unification (2026-05-03): mEQDisplay removed.

    // G-7: full processor + bus-active query for Page Preset save/load.
    VibeSynthProcessor*                          mFullProcessor { nullptr };
    std::function<bool(int)>                     mBusActiveQuery;

    // G-7 (2026-04-29): listener-based dirty tracking — see ClipsPage for
    // the rationale.  Reliable across engines whose getStateInformation
    // serialization timing differs from byte-comparison expectations.
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxPage)
};

// ─────────────────────────────────────────────────────────────────────────────
// VoxEmptyState — text-only placeholder shown when the Vox ribbon slot is
// clicked with zero instances.  Spawn trigger is "Add Vox Strip" on Mixer.
// ─────────────────────────────────────────────────────────────────────────────
class VoxEmptyState : public juce::Component
{
public:
    VoxEmptyState();
    void paint (juce::Graphics&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxEmptyState)
};
