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

    // G-4 (2026-04-28): fired AFTER a Vox / Inst strip is created via the
    // Mixer page's "Add Vox Strip" / "Add Inst Strip" button (or restored at
    // project load).  StandaloneEditor wires these to spawnVoxTabIfMissing /
    // spawnInstTabIfMissing so the matching ribbon page appears alongside the
    // strip.  idx is the strip's slot index (0..kMaxVoxStrips-1 / 0..kMaxInstStrips-1).
    std::function<void(int idx)> onVoxStripAdded;
    std::function<void(int idx)> onInstStripAdded;

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

    // B2 + B1 (2026-05-04): returns the active audio device's name (e.g.
    // "Tascam Model 24") so the input-picker can apply known stereo-pair
    // profiles for interfaces whose driver names channels without L/R
    // suffixes.  Empty when no device is open.
    std::function<juce::String()> getInputDeviceName;

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

    // 2026-05-05: remove the Inst / Vox / Clip strip at the given idx so the
    // index can be reused after a tab close.  APVTS params preserved (so
    // re-adding at the same idx restores prior fader/pan/sends/etc).  Without
    // these, deleting a tab leaves an orphan mixer strip pinned to the index
    // forever — addInstChannelAtIndex et al silently bail when count(idx)>0.
    void removeInstChannel(int idx);
    void removeVoxChannel(int idx);
    void removeClipChannel(int idx);

    // G-7 (2026-04-29): full delete via right-click → Delete prompt.  Sweeps
    // every strip's send params and resets any pointing at this aux's
    // channel id (sends → inactive; primary _sendTo → natural parent).
    // Then removes the InsertNode + UI strip.  APVTS params for the aux
    // strip itself are left intact so re-creating the aux at the same idx
    // restores prior settings.
    void deleteAuxStrip (int idx, int auxChannelId);

    // G-7: delete a secondary Vox/Inst bus.  Reroutes any strip whose
    // _sendTo or _sendN_to targets this bus → the natural parent bus
    // (kVoxBus / kInstBus).  Hides the UI strip (mVoxBus2Active etc. → false).
    // Audio InsertNode stays allocated (always-allocated in prepare()).
    void deleteSecondaryBus (int channelId);

    // R1 (2026-04-23): Vox / Inst strip creation.  Up to kMaxVoxStrips (6)
    // and kMaxInstStrips (6) respectively.  Same pattern as aux.
    void addVoxChannel();
    void addVoxChannelAtIndex(int idx);
    void addInstChannel();
    void addInstChannelAtIndex(int idx);

    // K-2 (2026-05-05): toggle the noLiveInput flag on an existing Inst strip.
    // Call when the corresponding InstPage's source mode changes from LiveInput
    // to BaySickGuitars / BaySickBasses (or back).  Hides arm + listen LEDs.
    // Safe to call before or after the strip's setApvts.  No-op if the strip
    // doesn't exist at the given index.
    void setInstStripNoLiveInput (int idx, bool b);

    // J-5: BaySickRustyDrums strip add/remove (driven by kit-load lifecycle,
    // NOT user-clicks).  Idempotent — safe to call again with same idx.
    void addRustyChannelAtIndex (int idx, const juce::String& name);
    void removeRustyChannelAtIndex (int idx);
    void clearAllRustyChannels();

    // D.3 (2026-05-01): override strip display order from a saved project.
    // The vector lists indices in left-to-right display order.  Indices not
    // currently registered are dropped silently; currently-registered indices
    // missing from the saved order are appended at the end so we never lose
    // a strip.  Triggers a re-layout.
    enum class OrderKind { Aux, Vox, Inst, Audio };
    void setStripOrder (OrderKind kind, const std::vector<int>& indices);

    // 5F-4b B2: accessor for PageMenuBar injection (StandaloneEditor reparents
    // this button into the menu bar when the Mixer page becomes visible).
    juce::Component* getAddAuxBtn()    const { return mAddAuxBtn.get();    }
    juce::Component* getAddVoxBtn()    const { return mAddVoxBtn.get();    }
    juce::Component* getAddInstBtn()   const { return mAddInstBtn.get();   }
    // G-6 (2026-04-29): Add Vox/Inst BUS buttons (separate from Strip buttons).
    juce::Component* getAddVoxBusBtn() const { return mAddVoxBusBtn.get(); }
    juce::Component* getAddInstBusBtn() const { return mAddInstBusBtn.get(); }

    // G-6 (2026-04-29): activate a secondary Vox/Inst bus — creates the
    // strip on Mixer + flags the bus active for route-picker / cable
    // filtering.  Idempotent (no-op if already active).  Returns true on
    // success, false if at cap.
    bool activateVoxBus2();
    bool activateInstBus2();
    bool activateInstBus3();
    bool isVoxBus2Active()  const { return mVoxBus2Active; }
    bool isInstBus2Active() const { return mInstBus2Active; }
    bool isInstBus3Active() const { return mInstBus3Active; }

    // Called by StandaloneEditor when a ribbon tab is renamed (ribbon → mixer sync).
    // C.4 follow-up (2026-04-30): kind tag added because Layer / Bass / Drum
    // strip maps are all keyed by per-type page index (0..N-1).  The old
    // signature took only an index and searched maps in order Layer -> Bass
    // -> Drum, stopping at first match -- which collided when Bass[0] and
    // Drum[0] coexisted (renaming the Drum hit Bass's strip first).
    enum class StripKind { Layer, Bass, Drum };
    void renameChannel(StripKind kind, int pageIdx, const juce::String& newName);

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
    std::vector<int> getAudioStripIndices() const { return mAudioRowOrder; }
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
    // G-6 (2026-04-29): secondary Vox/Inst bus strips — lazy.  Created on
    // first activate*() call; visibility flag tracked separately so the
    // strip can be hidden/shown without destroying its state.
    std::unique_ptr<MixerTrackStrip> mVoxBus2Strip;
    std::unique_ptr<MixerTrackStrip> mInstBus2Strip;
    std::unique_ptr<MixerTrackStrip> mInstBus3Strip;
    bool                             mVoxBus2Active  { false };
    bool                             mInstBus2Active { false };
    bool                             mInstBus3Active { false };
    // J-5: BaySickRustyDrums dedicated bus strip.  Visible whenever any
    // mRustyStrips entry exists (mRustyDrumsBusActive flag).
    std::unique_ptr<MixerTrackStrip> mRustyDrumsBusStrip;
    bool                             mRustyDrumsBusActive { false };

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
    // G-6 (2026-04-29): Add Vox Bus / Add Inst Bus buttons — sit alongside
    // the existing strip-add buttons.  Greyed out at cap (Vox: 1 extra max,
    // Inst: 2 extra max).
    std::unique_ptr<juce::TextButton>               mAddVoxBusBtn;
    std::unique_ptr<juce::TextButton>               mAddInstBusBtn;
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

    // J-5 (2026-05-03): BaySickRustyDrums strips.  Spawned/destroyed in
    // batches of 13 by PluginProcessor::loadBaySickRustyDrumsKit /
    // destroyBaySickRustyDrums.  No "Add Rusty" UI button — strip lifecycle
    // is engine-driven, not user-driven.
    std::map<int, std::unique_ptr<MixerTrackStrip>> mRustyStrips;
    std::vector<int>                                mRustyOrder;

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
        // C.4 Phase 1 (2026-04-30): sidechain-placement mode (click "+" →
        // pick "Sidechain" from popup → click destination strip).  At most
        // ONE of mPendingSendSrcId / mPendingScSrcId is >= 0 at a time.
        int   mPendingScSrcId   { -1 };

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
        // C.4 Phase 1: enter/exit sidechain-placement mode (white cable).
        void startSidechainPlacement(int srcChannelId);
        void cancelSidechainPlacement();

        int findSocketNear(juce::Point<float> pt, float radius, bool skipLocked) const;
        int findStripUnder(juce::Point<float> pt) const;
        bool isRouteAllowed(int srcId, int dstId) const;

        // B5: find the first inactive send slot (0..3) for a strip, or -1 if full.
        int findAvailableSendSlot(const juce::String& prefix) const;
        // C.4 Phase 1: find the first empty SC receive slot on a TARGET strip
        // (target-side encoding).  Returns -1 if all 4 receive lines are full.
        int findAvailableScRecvSlot(const juce::String& targetPrefix) const;

        // B6: right-click cable popup
        // Returns {srcId, dstId, sendSlotIdx} if a cable is near pt; else {-1,-1,-1}.
        // C.4 Phase 1 extends with sidechain detection: when isSidechain is
        // true, scRecvSlot holds the target's receive line (0..3) and sendSlot
        // is unused.
        struct CableHit {
            int  srcId      = -1;
            int  dstId      = -1;
            int  sendSlot   = -1;
            int  scRecvSlot = -1;
            bool isMainOut  = false;
            bool isSidechain= false;
        };
        CableHit hitTestCable(juce::Point<float> pt) const;
        void showCablePopup(juce::Point<float> screenPt, const CableHit& hit);
    };
    std::unique_ptr<CableOverlay> mCableOverlay;

    // C.4 Phase 1 (2026-04-30): popup-then-dispatch helper for the per-strip
    // "+" Add-Send button.  Shows a Send / Sidechain submenu and routes the
    // user's pick to the corresponding CableOverlay placement mode.  Wired
    // from every onAddSendRequested lambda.
    void onAddCableRequestedFor(int srcChannelId);

    // Find the strip component for a given MixerChannelIds value (or nullptr).
    MixerTrackStrip* findStripByChannelId(int channelId) const;

    // Get the socket position (page coords) for the bottom-center of a strip.
    // Returns {-1,-1} if the strip isn't found or isn't visible.
    juce::Point<float> getSocketPosition(int channelId) const;

    // ── Internal helpers ──────────────────────────────────────────────────────
    void layoutScrollContent();
    void timerCallback() override;
    // 2026-05-02: meter polling moved to a vblank-locked callback so the
    // upstream sync (audio peak atomic -> DBFSMeter) runs in lockstep with
    // the monitor refresh.  The 30 Hz Timer above is kept for the slower
    // page-state polls (cable overlay scroll detection, _sendTo change
    // detection, flash decay) where vsync precision isn't needed.
    void onVBlank();
    std::unique_ptr<juce::VBlankAttachment> mVBlank;
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
