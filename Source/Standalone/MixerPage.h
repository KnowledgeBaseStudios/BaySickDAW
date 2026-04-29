#pragma once
#include <JuceHeader.h>
#include <map>
#include "../PluginProcessor.h"
#include "../PatternManager.h"
#include "MixerTrackStrip.h"
#include "UndoActions.h"

// ── MixerPage ─────────────────────────────────────────────────────────────────
// Permanent system tab (Mixer).  Horizontal console layout with horizontal scroll.
//
// Default state: Master (fixed) + 4 Bus strips.
// Instrument channel strips are created lazily:
//   addLayerChannel(tabId, name) — called when a Layers tab is opened
//   addBassChannel (tabId, name) — called when a Bass tab is opened
//   addDrumChannel (slot,  name) — called when a drum slot gets a sound
//
// Strips never get destroyed (preserves effects chain).
//
// Bidirectional name sync for Layer/Bass:
//   mixer strip rename  → onChannelRenamed(tabId, newName) → ribbon renameTab()
//   ribbon tab rename   → renameChannel(tabId, newName)    → strip setTrackName()
//
// Strip order (left → right):
//   [Master] | sep | [Layers Bus][Bass Bus][Drums Bus][FX Bus] | sep |
//   [Layer strips] | sep | [Bass strips] | sep | [Drum strips]
//
// Timer at 30fps: polls PluginProcessor level atomics → updates meters.
// ─────────────────────────────────────────────────────────────────────────────
class MixerPage : public juce::Component,
                  private juce::Timer,
                  private juce::ScrollBar::Listener
{
public:
    MixerPage(VibeSynthProcessor& processor, PatternManager& pm);
    ~MixerPage() override;

    void paint  (juce::Graphics&) override;
    void resized() override;

    // Set by StandaloneEditor — called when any strip's FX button is clicked.
    std::function<void(const juce::String&)> onEffectsTabRequested;

    // Fired when the user renames a Layer or Bass strip in the mixer.
    // StandaloneEditor wires this to mRibbon->renameTab().
    std::function<void(int tabId, const juce::String& newName)> onChannelRenamed;

    // Fired when any audio row strip is renamed — StandaloneEditor rebuilds Effects dropdown.
    std::function<void()> onAudioStripRenamed;

    // Fired when any strip's main-out _sendTo changes — StandaloneEditor rebuilds
    // the Effects dropdown so strips rerouted to Master (Direct Routing) or
    // between buses show up under the correct group.
    std::function<void()> onSendToChanged;

    // R2 (2026-04-23): supplied by StandaloneEditor.  Returns the names of
    // available input channels on the currently-active audio device (e.g.
    // ["Mic 1", "Mic 2", "Inst In"] for an interface with 3 inputs).  Empty
    // when no device is open or device has zero inputs.  Used by the Vox /
    // Inst Arm-LED click handler to populate its input-channel picker.
    std::function<juce::StringArray()> getInputChannelNames;

    // Returns the current display name of an audio row strip (for Effects dropdown).
    juce::String getAudioStripName(int row) const;

    // Aux strip enumeration for the Effects dropdown. Returns aux indices in
    // creation order and the display name for a given index.
    std::vector<int> getAuxStripIndices() const;
    juce::String     getAuxStripName(int idx) const;

    // Drum strip enumeration for the Effects dropdown. Returns drum-slot indices
    // that currently have a strip (i.e. a sound has been assigned to the slot)
    // and the display name for a given slot.
    std::vector<int> getDrumStripIndices() const;
    juce::String     getDrumStripName(int slot) const;

    // ── Lazy channel creation ─────────────────────────────────────────────────
    // Called by StandaloneEditor when a page tab is opened or a sound assigned.
    // Strips persist once created; passing an empty name to addDrumChannel is a no-op.
    void addLayerChannel(int tabId, const juce::String& name);
    void addBassChannel (int tabId, const juce::String& name);
    void addDrumChannel (int slot,  const juce::String& name);
    void addAudioChannel(int row,   const juce::String& name);  // one strip per arrangement row

    // 5F-4b B2: create an aux strip at the next available index (0..15).
    // Called by the "Add Mixer Strip" button. Registers the aux in VibeGraph + APVTS.
    void addAuxChannel();
    // 5F-4b B7: create an aux strip at a specific index (for restoring saved projects).
    void addAuxChannelAtIndex(int idx);
    // Remove the aux strip at the given idx. APVTS params preserved.
    void removeAuxChannel(int idx);

    // R1 (2026-04-23): Vox / Inst strip creation.  Up to kMaxVoxStrips (6)
    // and kMaxInstStrips (6) respectively.  Same pattern as aux.
    void addVoxChannel();
    void addVoxChannelAtIndex(int idx);
    void addInstChannel();
    void addInstChannelAtIndex(int idx);

    // 5F-4b B2: accessor for PageMenuBar injection (StandaloneEditor reparents
    // this button into the menu bar when the Mixer page becomes visible).
    juce::Component* getAddAuxBtn()  const { return mAddAuxBtn.get();  }
    juce::Component* getAddVoxBtn()  const { return mAddVoxBtn.get();  }
    juce::Component* getAddInstBtn() const { return mAddInstBtn.get(); }

    // Called by StandaloneEditor when a ribbon tab is renamed (ribbon → mixer sync).
    void renameChannel(int tabId, const juce::String& newName);

    // Connect to the global undo system.
    void setUndoContext(const UndoContext& ctx);

    // R2 (2026-04-23): refresh the Arm-LED tooltip + visual on a Vox / Inst
    // strip after its `_inputChannelIdx` APVTS changed.  Called immediately
    // after writing the param + on project load.
    void refreshLiveInputStrip(int channelId);

    // 2026-04-24 Cycle 2: push a custom name into an existing aux strip on
    // project load.  No-op if the aux strip doesn't exist yet.
    void setAuxStripName  (int idx, const juce::String& name);
    void setVoxStripName  (int idx, const juce::String& name);
    void setInstStripName (int idx, const juce::String& name);

    // 2026-04-24: wipe every dynamic (non-bus, non-master) strip + reset
    // "next idx" counters.  Called before File > Open / Restore / New so
    // residual strips from the previous session don't bleed through.  Fixed
    // buses (Layers / Bass / Drums / FX / Clips / Vox / Inst / Master) and
    // the scroll content they live in are preserved - only the per-user
    // strips (Layer inserts / Bass inserts / Drum slots / Audio rows / Aux /
    // Vox / Inst) get removed.
    void clearDynamicStrips();

    // 2026-04-24: enumeration + name accessors for Vox / Inst (aux already had
    // equivalents).  Used by serializeUIState to stash custom names.
    std::vector<int> getVoxStripIndices()  const;
    std::vector<int> getInstStripIndices() const;
    juce::String     getVoxStripName  (int idx) const;
    juce::String     getInstStripName (int idx) const;

    // P4 persistence (2026-04-24): horizontal scroll position for save/restore.
    int  getScrollX() const;
    void setScrollX(int x);

private:
    // R2: shared popup-menu helper for Vox + Inst Arm-LED clicks.
    void showInputChannelPicker(int channelId);
    VibeSynthProcessor& mProcessor;
    PatternManager&     mPM;

    // ── Fixed left panel: Master strip ────────────────────────────────────────
    std::unique_ptr<MixerTrackStrip> mMasterStrip;

    // ── Scrollable console area ───────────────────────────────────────────────
    // Horizontal scrollbar is a SEPARATE widget placed at the TOP of the
    // page (above the strips) so the cable overlay never covers it. The
    // Viewport's own scrollbars are hidden; we drive `mViewport` directly
    // via scrollBarMoved() → setViewPosition().
    std::unique_ptr<juce::Viewport>  mViewport;
    std::unique_ptr<juce::ScrollBar> mTopScrollBar;

    // ScrollBar::Listener
    void scrollBarMoved(juce::ScrollBar* sb, double newRangeStart) override;
    void syncTopScrollBar();

    // Neon vertical line drawn on top of the strip row to separate members of
    // a bus group. Bright = between bus strip and first member. Dimmed = between
    // two adjacent members inside the same group.
    struct NeonLine
    {
        int          x;
        int          yStart, yEnd;
        juce::Colour color;
        bool         bright;
    };

    struct ScrollContent : public juce::Component
    {
        void paint(juce::Graphics&) override;
        void paintOverChildren(juce::Graphics&) override;

        std::vector<NeonLine> mNeonLines;
    };
    std::unique_ptr<ScrollContent>   mScrollContent;

    // Bus strips
    std::unique_ptr<MixerTrackStrip> mLayersBusStrip;
    std::unique_ptr<MixerTrackStrip> mBassBusStrip;
    std::unique_ptr<MixerTrackStrip> mDrumsBusStrip;
    std::unique_ptr<MixerTrackStrip> mFXBusStrip;
    std::unique_ptr<MixerTrackStrip> mAudioClipsBusStrip;
    // R1 (2026-04-23): Vox + Inst buses for live-input strip aggregation.
    std::unique_ptr<MixerTrackStrip> mVoxBusStrip;
    std::unique_ptr<MixerTrackStrip> mInstBusStrip;

    // Dynamic instrument strips — keyed by tabId (Layer/Bass) or slot (Drums)
    std::map<int, std::unique_ptr<MixerTrackStrip>> mLayerStrips;
    std::vector<int>                                mLayerTabOrder;

    std::map<int, std::unique_ptr<MixerTrackStrip>> mBassStrips;
    std::vector<int>                                mBassTabOrder;

    std::map<int, std::unique_ptr<MixerTrackStrip>> mDrumStrips;
    std::vector<int>                                mDrumSlotOrder;

    std::map<int, std::unique_ptr<MixerTrackStrip>> mAudioStrips;   // keyed by arrangement row
    std::vector<int>                                mAudioRowOrder;

    // 5F-4b B2: aux strip storage + "+" button
    std::map<int, std::unique_ptr<MixerTrackStrip>> mAuxStrips;
    std::vector<int>                                mAuxOrder;
    std::unique_ptr<juce::TextButton>               mAddAuxBtn;
    int                                             mNextAuxIdx { 0 };

    // R1 (2026-04-23): Vox + Inst strip storage + "+" buttons.
    std::map<int, std::unique_ptr<MixerTrackStrip>> mVoxStrips;
    std::vector<int>                                mVoxOrder;
    std::unique_ptr<juce::TextButton>               mAddVoxBtn;
    int                                             mNextVoxIdx { 0 };

    std::map<int, std::unique_ptr<MixerTrackStrip>> mInstStrips;
    std::vector<int>                                mInstOrder;
    std::unique_ptr<juce::TextButton>               mAddInstBtn;
    int                                             mNextInstIdx { 0 };

    // Direct Routing label — shown between Master and FX Bus when any strip
    // has _sendTo = Master. Small vertical-text panel; visibility driven by
    // layoutScrollContent().
    std::unique_ptr<juce::Component>                mDirectRoutingLabel;

    // Cache of each strip's last-known _sendTo value. The timerCallback
    // compares every tick; any delta triggers layoutScrollContent() so strips
    // visually move when their main-out cable is rerouted.
    std::map<int, int>                              mLastSendToCache;
    // R3.5 (2026-04-23): cable-overlay flicker fix.  Repaint the (now buffered-
    // to-image) overlay only when the viewport scroll position changed.
    int                                             mLastViewportX { -1 };

    // 5F-4b B3+B4: cable overlay — paints green beziers + handles cable drag.
    struct CableOverlay : public juce::Component, private juce::Timer
    {
        MixerPage& owner;

        // Drag state
        bool  mDragging     { false };
        int   mDragSrcId    { -1 };
        juce::Point<float> mDragSrcSocket;
        juce::Point<float> mDragMousePos;

        // Red-flash rejection state
        int   mFlashStripId   { -1 };
        int   mFlashCountdown { 0 };

        // 5F-4b B5: send-placement mode (click "+" → click destination)
        int   mPendingSendSrcId { -1 };

        explicit CableOverlay(MixerPage& o);
        ~CableOverlay() override { stopTimer(); }

        bool hitTest(int x, int y) override;
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void mouseMove(const juce::MouseEvent&) override;   // B5: track cursor in pending mode
        bool keyPressed(const juce::KeyPress&) override;     // B5: Escape cancels
        void timerCallback() override;

        // B5: enter/exit send-placement mode
        void startSendPlacement(int srcChannelId);
        void cancelSendPlacement();

        int findSocketNear(juce::Point<float> pt, float radius, bool skipLocked) const;
        int findStripUnder(juce::Point<float> pt) const;
        bool isRouteAllowed(int srcId, int dstId) const;

        // B5: find the first inactive send slot (0..3) for a strip, or -1 if full.
        int findAvailableSendSlot(const juce::String& prefix) const;

        // B6: right-click cable popup
        // Returns {srcId, dstId, sendSlotIdx} if a cable is near pt; else {-1,-1,-1}.
        struct CableHit { int srcId = -1; int dstId = -1; int sendSlot = -1; bool isMainOut = false; };
        CableHit hitTestCable(juce::Point<float> pt) const;
        void showCablePopup(juce::Point<float> screenPt, const CableHit& hit);
    };
    std::unique_ptr<CableOverlay> mCableOverlay;

    // Find the strip component for a given MixerChannelIds value (or nullptr).
    MixerTrackStrip* findStripByChannelId(int channelId) const;

    // Get the socket position (page coords) for the bottom-center of a strip.
    // Returns {-1,-1} if the strip isn't found or isn't visible.
    juce::Point<float> getSocketPosition(int channelId) const;

    // ── Internal helpers ──────────────────────────────────────────────────────
    void layoutScrollContent();
    void timerCallback() override;
    void syncFromModel();
    // 5F-4a Batch 6: push MixerState into APVTS so the audio path starts with
    // correct values (InsertNode reads level/pan/mute/solo from APVTS).
    void syncApvtsFromMixerState();
    void applyMixerSnapshot(const MixerState& state);

    void wireMasterCallbacks();
    void wireBusCallbacks(MixerTrackStrip* strip, float& levelRef, float& panRef,
                          bool& muteRef, bool& soloRef);

    void drawSectionLabel(juce::Graphics& g, const juce::String& text,
                          juce::Rectangle<int> bounds) const;

    // ── Undo context + pending before-state ───────────────────────────────────
    UndoContext  mUndoCtx;
    MixerState   mMixerStateBefore;   // captured at drag-start

    // Layout constants
    static constexpr int kFixedPanelW = 96 + 4;
    static constexpr int kSepW        = 12;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerPage)
};
