#pragma once
#include <JuceHeader.h>
#include "../Standalone/SharedUI.h"   // ParametricEQDisplay
#include "../Standalone/UndoActions.h"   // QA-Fd: UndoContext plumb-through

class VibeSynthProcessor;
class BaySickVocalEditor;

// ─────────────────────────────────────────────────────────────────────────────
// VoxPage - host component for one Vox tab (Phase G-4).
// ─────────────────────────────────────────────────────────────────────────────
// Mirror of InstPage with Vox-specific colour + naming.  Engine list for V1
// is just BaySickPlayer (sample playback of recorded vocal).  Phase H adds
// `BaySickVocal` (the dedicated vocal channel-strip processor) as the
// preferred default option.  Spawn triggers: the Mixer page's "Add Vox
// Strip" button, the ribbon Vox dropdown's "+ Add New Vox" (G-6), and the
// QA-Fa "+ Add New Vox From Export" flow.
//
// QA-Fa recovery: the page owns a 4 Hz timer driving the stop-gated auto
// re-analyze for the engine's Align + Pitch analyses (works with the
// editors closed; dies with the page).
// ─────────────────────────────────────────────────────────────────────────────

class VoxPage : public juce::Component,
                private juce::Timer
{
public:
    // 2026-04-29 DEBUG: drag-drop a WAV/MP3 onto the page to load it into the
    // QA-E Task 4 (2026-05-12): isInterestedInFileDrag + filesDropped
    // overrides removed (debug-only scaffolding from 2026-04-29; never an
    // intended user surface).  Drag-and-drop of audio is the Browser-to-
    // Builder-grid flow handled by ArrangementGrid; library tagging for
    // Vox recordings happens via commitRecordingResult.

    // H-6b (2026-05-01): Vox tabs are always BaySickVocal - no engine picker.
    // EngineType retained for save/load back-compat but only BaySickVocal is
    // valid going forward.  Old projects with BaySickPlayer state silently
    // discard it on load (BaySickPlayer is no longer a Vox option).
    enum class EngineType { None, BaySickPlayer, BaySickVocal };

    // QA-ModelShell TS1: the processor ref is needed at construction because
    // the vocal engine is rig-owned and picked in the ctor (H-6b).
    VoxPage (VibeSynthProcessor& proc, int pageIndex);
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

    // QA-E Task 4 (2026-05-12): getClipFilePath / setClipFilePath / mClipPath
    // + mClipFileLabel + mLinkedClipPath deleted.  Vox file-association now
    // lives in PatternManager's AudioLibrary via pageOwnerChannelId tagging
    // (per §9 17th Forks entry).  Drop handler tags the dropped library entry
    // directly; commitRecordingResult tags recorded library entries.

    void          selectEngine (EngineType e);
    EngineType    getEngineType() const noexcept { return mEngineType; }
    juce::AudioProcessor* getEngineProcessor() const noexcept;
    // 2026-05-05 dirty-flag wiring: editor walks the vocal processor + its
    // embedded NAM/IR sub-processor to install the markDirty hook.
    juce::AudioProcessor* getVocalProcessor() const noexcept { return mVocalProc; }

    std::function<void()> onEngineDestroying;
    std::function<void()> onEngineChanged;

    void                setTabName (const juce::String& n);   // syncs the model tab's name (TS1)
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

    // Save/Load PAGE preset - entire VoxPage state (currently just BaySickPlayer;
    // Phase H adds BaySickVocal alongside).  XML matches exportVoxState format.
    void saveVoxPagePreset();
    void loadVoxPagePreset (const juce::File& xml);

    // ── G-7 (2026-04-29): Page Preset save/load (full chain) ─────────────────
    // Captures engine + strip params + insert rack + post-EQ.  setProcessor
    // must be called by StandaloneEditor with the global VibeSynthProcessor.
    // Bus fallback: if a saved _sendTo references kVoxBus2 and that bus isn't
    // active in the current project, the loader silently substitutes kVoxBus.
    void setProcessor (VibeSynthProcessor* p);
    // QA-Fd 9a: the pitch editor joins the global undo -- StandaloneEditor
    // hands the context here; forwarded into the vocal editor (and re-applied
    // if the editor is ever rebuilt by selectEngine).
    void setUndoContext (const UndoContext& ctx);
    // QA-Fd #7: main-transport beat provider for the pitch editor playhead.
    void setTransportBeatProvider (std::function<double()> fn);
    void setTransportSeekProvider (std::function<void(double)> fn);
    void setSongTimeSelProviders (std::function<void(float,float)> setFn,
                                  std::function<bool(float&,float&)> getFn);
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

    // QA-Fa recovery: stop-gated auto re-analyze poller (see the .cpp).
    void timerCallback() override;
    juce::int64 mAlignAutoLastSig   { 0 };
    juce::int64 mAlignAutoAttempted { 0 };
    int         mAlignAutoStable    { 0 };
    juce::int64 mPitchAutoLastSig   { 0 };
    juce::int64 mPitchAutoAttempted { 0 };
    int         mPitchAutoStable    { 0 };

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
    bool                                         mLocked { false };

    RightClickEngineCombo                        mEnginePicker;
    EngineType                                   mEngineType { EngineType::None };
    std::unique_ptr<juce::AudioProcessor>        mPlayerProc;        // VibePlayerProcessor (H-6b: dead -- never created; enum back-compat only)
    // QA-ModelShell TS1: non-owning view of the rig-owned BaySickVocal
    // ({Vox, pageIndex}); the editor IS view-owned and must die before the
    // page (its attachments reference the engine's APVTS).
    juce::AudioProcessor*                        mVocalProc { nullptr };

    std::unique_ptr<juce::AudioProcessorEditor>  mPlayerEditor;
    std::unique_ptr<juce::AudioProcessorEditor>  mVocalEditor;

    // J-6 EQ unification (2026-05-03): mEQDisplay removed.

    // G-7: full processor + bus-active query for Page Preset save/load.
    VibeSynthProcessor*                          mFullProcessor { nullptr };
    std::function<bool(int)>                     mBusActiveQuery;
    UndoContext                                  mUndoCtx;   // QA-Fd 9a
    std::function<double()>                      mTransportBeat;   // QA-Fd #7
    std::function<void(double)>                  mTransportSeek;   // pitch ruler -> seek
    std::function<void(float,float)>             mSetSongTimeSel;  // pitch ruler range -> builder
    std::function<bool(float&,float&)>           mGetSongTimeSel;  // builder range -> pitch ruler

    // G-7 (2026-04-29): listener-based dirty tracking - see ClipsPage for
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
// VoxEmptyState - text-only placeholder shown when the Vox ribbon slot is
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
