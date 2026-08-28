#pragma once
#include <JuceHeader.h>
#include <map>
#include <unordered_map>
#include "../PluginProcessor.h"
#include "../PatternManager.h"
#include "MixerTrackStrip.h"
#include "UndoActions.h"

// ── MixerPage ─────────────────────────────────────────────────────────────────
// Permanent system tab (Mixer).  Horizontal console layout with horizontal scroll.
//
// Default state: Master (fixed) + the Layers / Bass / Drums / FX / Clips /
// Vox / Inst bus strips.  The RustyDrums, Plugins and Drums Bus 2 strips are
// built at construction but stay hidden until a tab of that family exists.
// Instrument channel strips are created lazily:
//   addLayerChannel(pageIndex, name) - called when a Layers tab is opened
//   addBassChannel (pageIndex, name) - called when a Bass tab is opened
//   addDrumChannel (slot,      name) - called when a drum slot gets a sound
//
// Closing a tab removes its strip WIDGET only -- the InsertNode + APVTS
// params persist, so re-adding the same index restores prior settings.
//
// Name sync between a channel strip and its ribbon tab:
//   mixer strip rename  -> onChannelRenamed(kind, pageIdx, newName) -> ribbon renameTab()
//   ribbon tab rename   -> renameChannel  (kind, pageIdx, newName)  -> strip setTrackName()
// Drum strips are not editable in the mixer, so those only travel ribbon -> strip.
// Both directions land in ONE StandaloneEditor body and ONE undo transaction --
// which is also the project's dirty signal, so neither may be short-circuited
// here into a bare setTrackName.
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
    MixerPage(BaySickDAWProcessor& processor, PatternManager& pm);
    ~MixerPage() override;

    void paint  (juce::Graphics&) override;
    void resized() override;

    // --shot only: runs the real send-menu path so the harness's capture hook
    // (ShotMenuHook.h) can take the menu headless.
    void showSendMenuForShot (int srcChannelId) { onAddCableRequestedFor (srcChannelId); }

    // The title strip's Add menu (L13's ruled seven rows exactly -- Vox/Inst
    // STRIP adds live on the ribbon's "+" flow, not here).  Build-only: the
    // editor's Add heading shows it, the --shot harness renders it.
    juce::PopupMenu buildAddMenu();

    // Set by StandaloneEditor - called when any strip's FX button is clicked.
    std::function<void(const juce::String&)> onEffectsTabRequested;

    // C.4 follow-up (2026-04-30): kind tag added because Layer / Bass / Drum
    // strip maps are all keyed by per-type page index (0..N-1).  The old
    // signature took only an index and searched maps in order Layer -> Bass
    // -> Drum, stopping at first match -- which collided when Bass[0] and
    // Drum[0] coexisted (renaming the Drum hit Bass's strip first).
    // QA-ClipDrop Task 3 (SC-H, 2026-06-03): Audio added so a Clips ribbon-tab
    // rename syncs through to its mixer strip (strips live in mAudioStrips keyed
    // by the owning Clips-page row index) -- parity with Layer/Bass/Drum.
    // TS6 (BLU-447) -- Plugin added TS7 2026-07-30 for the same reason Audio was
    // added above: without an enum entry a plugin tab rename had no route to its
    // mixer strip at all.
    // Vox / Inst added for the REVERSE gap: their strips are renameable and
    // their names persist in <VoxNames> / <InstNames>, but with no enum entry a
    // strip rename never reached the owning tab, so the strip and the ribbon
    // tab drifted into two permanent different names for one channel.
    enum class StripKind { Layer, Bass, Drum, Audio, Plugin, Vox, Inst, Direct };

    // Fired when the user renames a channel strip in the mixer.  StandaloneEditor
    // resolves (kind, pageIdx) to the owning ribbon tab and renames it.  The kind
    // is load-bearing in BOTH directions: without it an index alone cannot name a
    // target, because every strip family numbers its own pages from 0.
    // An Audio row whose Clips tab is gone has nowhere to store the name, so the
    // editor snaps the label back through renameChannel and says why -- do not
    // assume the strip still reads `newName` when this returns.
    std::function<void(StripKind kind, int pageIdx, const juce::String& newName)> onChannelRenamed;

    // Fired when any audio row strip is renamed - StandaloneEditor rebuilds Effects dropdown.
    std::function<void()> onAudioStripRenamed;

    // Fired AFTER a Vox / Inst strip is created, whatever created it -- the
    // Mixer's Add menu, the ribbon "+", or a project-load restore.
    // StandaloneEditor wires these to spawnVoxTabIfMissing /
    // spawnInstTabIfMissing so the matching ribbon page appears alongside the
    // strip; the page is DOWNSTREAM of the strip for these two types, which is
    // why they cannot be rebuilt from the model alone (canRebuildType).
    // idx is the strip's slot index (0..kMaxVoxStrips-1 / 0..kMaxInstStrips-1).
    // (Named the "Add Vox Strip" / "Add Inst Strip" buttons until 2026-08-05;
    // T10 replaced those five buttons with the Add titled menu.)
    std::function<void(int idx)> onVoxStripAdded;
    std::function<void(int idx)> onInstStripAdded;

    // Fired when any strip's main-out _sendTo changes - StandaloneEditor rebuilds
    // the Effects dropdown so strips rerouted to Master (Direct Routing) or
    // between buses show up under the correct group.
    std::function<void()> onSendToChanged;
    // QA-ModelShell TS7 (CL-044): the master strip's repurposed "+" button asks
    // the editor to open the master analyzer window.
    std::function<void()> onAnalyzerRequested;

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

    // QA-Fe2 (2026-07-16, replaces the arm popup): "Builder Grid Default"
    // section in the Vox arm-LED right-click picker -- which recorded take
    // lands on the Builder grid.  -1 = auto (Wet when realtime correction is
    // on, else Dry); 0..3 = user-locked pick (Dry / Dry Cleaned / Wet /
    // Wet Cleaned) held until the project closes.  State lives in
    // StandaloneEditor (it drives commitRecordingResult).
    std::function<int(int voxIdx)>            onGetGridDefault;
    std::function<void(int voxIdx, int pick)> onSetGridDefault;

    // Returns the current display name of an audio row strip (for Effects dropdown).
    juce::String getAudioStripName(int row) const;

    // Aux strip enumeration for the Effects dropdown. Returns aux indices in
    // creation order and the display name for a given index.
    std::vector<int> getAuxStripIndices() const;
    juce::String     getAuxStripName(int idx) const;

    // QA-ModelShell TS2 (stems pick-list): every strip the mixer currently
    // SHOWS, in display-group order, with its MixerChannelIds id, the shown
    // name, and the stems-dialog default -- Master + buses default UNCHECKED
    // per Jeff's per-strip stems spec, everything else CHECKED.  The mixer is
    // the single truth for "active strip" (covers the membership-driven
    // empty-bus hiding and the lazy secondary buses).
    struct StemPickEntry
    {
        int          channelId { -1 };
        juce::String name;
        bool         defaultChecked { true };
    };
    std::vector<StemPickEntry> getStemPickEntries() const;

    // Drum strip enumeration for the Effects dropdown. Returns drum-slot indices
    // that currently have a strip (i.e. a sound has been assigned to the slot)
    // and the display name for a given slot.
    std::vector<int> getDrumStripIndices() const;
    juce::String     getDrumStripName(int slot) const;

    // ── Lazy channel creation ─────────────────────────────────────────────────
    // Called by StandaloneEditor when a page tab is opened or a sound assigned.
    // Strips persist once created; passing an empty name to addDrumChannel is a no-op.
    void addLayerChannel(int pageIndex, const juce::String& name);
    void addPluginChannel(int pageIndex, const juce::String& name);   // TS6 (BLU-447)
    // QA-TrueLevel SC-10: Direct to Master strip -- engine-less, grey, locked to
    // the master.  Mirrors the Layer shape (no arm / monitor).
    void addDirectChannel(int idx, const juce::String& name);
    void removeDirectChannel(int idx);
    void setDirectChannelMissing(int idx, bool missing);   // Task 5: grey + "+" overlay
    void addBassChannel (int pageIndex, const juce::String& name);
    void addDrumChannel (int slot,  const juce::String& name);
    void addAudioChannel(int row,   const juce::String& name);  // one strip per arrangement row

    // 5F-4b B2: create an aux strip at the next available index (0..15).
    // Called by the "Add Mixer Strip" button. Registers the aux in BaySickGraph + APVTS.
    void addAuxChannel();
    // 5F-4b B7: create an aux strip at a specific index (for restoring saved projects).
    void addAuxChannelAtIndex(int idx);
    // Remove the aux strip at the given idx. APVTS params preserved.
    void removeAuxChannel(int idx);

    // 2026-05-05: remove the Inst / Vox / Clip strip at the given idx so the
    // index can be reused after a tab close.  APVTS params preserved (so
    // re-adding at the same idx restores prior fader/pan/sends/etc).  Without
    // these, deleting a tab leaves an orphan mixer strip pinned to the index
    // forever - addInstChannelAtIndex et al silently bail when count(idx)>0.
    void removeInstChannel(int idx);
    void removeVoxChannel(int idx);
    void removeClipChannel(int idx);
    // Same orphan class for the engine-page strips (MIX-05's real cause):
    // Layer/Bass/Drum tab closes never removed their strip, so strips
    // overlapped after re-add and the add-side count(idx) guard blocked
    // reuse.  Same preserve-APVTS convention as the trio above.
    void removeLayerChannel(int pageIndex);
    void removePluginChannel(int pageIndex);   // TS6 (BLU-447)
    void removeBassChannel (int pageIndex);
    void removeDrumChannel (int slot);

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
    void addVoxChannelAtIndex(int idx);
    void addInstChannelAtIndex(int idx);

    // K-2 (2026-05-05): toggle the noLiveInput flag on an existing Inst strip.
    // Call when the corresponding InstPage's source mode changes from LiveInput
    // to BaySickGuitars / BaySickBasses (or back).  Hides arm + listen LEDs.
    // Safe to call before or after the strip's setApvts.  No-op if the strip
    // doesn't exist at the given index.
    void setInstStripNoLiveInput (int idx, bool b);

    // Substituted-kit display marker for a sfizz Inst strip (red name + a
    // tooltip line, never label text).  No-op if the strip doesn't exist.
    void setInstStripKitMissing (int idx, bool b);

    // J-5: BaySickRustyDrums strip add/clear (driven by kit-load lifecycle,
    // NOT user-clicks).  Idempotent - safe to call again with same idx.
    void addRustyChannelAtIndex (int idx, const juce::String& name);
    void clearAllRustyChannels();

    // D.3 (2026-05-01): override strip display order from a saved project.
    // The vector lists indices in left-to-right display order.  Indices not
    // currently registered are dropped silently; currently-registered indices
    // missing from the saved order are appended at the end so we never lose
    // a strip.  Triggers a re-layout.
    enum class OrderKind { Aux, Vox, Inst, Audio };
    void setStripOrder (OrderKind kind, const std::vector<int>& indices);

    // G-6 (2026-04-29): activate a secondary Vox/Inst bus - creates the
    // strip on Mixer + flags the bus active for route-picker / cable
    // filtering.  Idempotent (no-op if already active).  Returns true on
    // success, false if at cap.
    bool activateVoxBus2();
    bool activateInstBus2();
    bool activateInstBus3();
    // QA-Layout T10 (L13): secondary group buses, same activate contract.
    bool activateLayersBus2();
    bool activateBassBus2();
    bool activateClipsBus2();
    bool activatePluginsBus2();
    bool isVoxBus2Active()  const { return mVoxBus2Active; }
    bool isInstBus2Active() const { return mInstBus2Active; }
    bool isInstBus3Active() const { return mInstBus3Active; }
    bool isPluginsBusActive() const { return mPluginsBusActive; }   // TS6 (BLU-447)
    bool isLayersBus2Active()  const { return mLayersBus2Active;  }
    bool isBassBus2Active()    const { return mBassBus2Active;    }
    bool isClipsBus2Active()   const { return mClipsBus2Active;   }
    bool isPluginsBus2Active() const { return mPluginsBus2Active; }
    // L14 lifecycle state, persisted by the editor's <Buses> element.
    bool getBusEverRouted (int chId) const
    {
        auto it = mBusEverRouted.find (chId);
        return it != mBusEverRouted.end() && it->second;
    }
    void setBusEverRouted (int chId, bool v) { mBusEverRouted[chId] = v; }

    // Called by StandaloneEditor when a ribbon tab is renamed (ribbon -> mixer
    // sync); the reverse direction is onChannelRenamed above, which declares the
    // StripKind both share.
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
    // TS6 (BLU-447) -- added TS7 2026-07-30 so the Effects page can enumerate
    // plugin strips as members of its PLUGINS BUS group.
    std::vector<int> getPluginStripIndices() const { return mPluginOrder; }
    juce::String     getPluginStripName (int idx) const;
    std::vector<int> getDirectStripIndices() const { return mDirectOrder; }
    juce::String     getDirectStripName (int idx) const;
    MixerTrackStrip* getDirectStrip (int idx) const
    {
        auto it = mDirectStrips.find (idx);
        return it != mDirectStrips.end() ? it->second.get() : nullptr;
    }

    // P4 persistence (2026-04-24): horizontal scroll position for save/restore.
    int  getScrollX() const;
    void setScrollX(int x);

private:
    // R2: shared popup-menu helper for Vox + Inst Arm-LED clicks.
    void showInputChannelPicker(int channelId);
    BaySickDAWProcessor& mProcessor;
    PatternManager&     mPM;

    // ── Fixed left panel: Master strip ────────────────────────────────────────
    std::unique_ptr<MixerTrackStrip> mMasterStrip;

    // ── Scrollable console area ───────────────────────────────────────────────
    // Horizontal scrollbar is a SEPARATE widget placed below the strips so the
    // cable overlay never covers it. The Viewport's own scrollbars are hidden;
    // we drive `mViewport` directly via scrollBarMoved() → setViewPosition().
    std::unique_ptr<juce::Viewport>  mViewport;
    std::unique_ptr<juce::ScrollBar> mHScrollBar;

    // ScrollBar::Listener
    void scrollBarMoved(juce::ScrollBar* sb, double newRangeStart) override;
    void syncHScrollBar();

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
    // QA-SOUNDNESS (2026-08-07): kit 2's own drums bus.  Deliberately NOT one
    // of the flag-gated secondary buses -- it is a system bus like Drums Bus,
    // so it carries no activation flag, no Add-menu entry and no delete: it is
    // always constructed and laidOutBus shows it exactly while a bank-2 drum
    // strip routes to it, which is the same rule Drums Bus itself follows.
    std::unique_ptr<MixerTrackStrip> mDrumsBus2Strip;
    std::unique_ptr<MixerTrackStrip> mFXBusStrip;
    std::unique_ptr<MixerTrackStrip> mAudioClipsBusStrip;
    // R1 (2026-04-23): Vox + Inst buses for live-input strip aggregation.
    std::unique_ptr<MixerTrackStrip> mVoxBusStrip;
    std::unique_ptr<MixerTrackStrip> mInstBusStrip;
    // G-6 (2026-04-29): secondary Vox/Inst bus strips - lazy.  Created on
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
    // QA-ModelShell TS6 (BLU-447): hosted VST3 instrument bus + its strips.
    std::unique_ptr<MixerTrackStrip> mPluginsBusStrip;
    bool                             mPluginsBusActive { false };
    // QA-Layout T10 (L13): secondary group buses -- lazy strips + activation
    // flags on the kVoxBus2 pattern, spawned from the Mixer's "Add" menu.
    std::unique_ptr<MixerTrackStrip> mLayersBus2Strip;
    std::unique_ptr<MixerTrackStrip> mBassBus2Strip;
    std::unique_ptr<MixerTrackStrip> mClipsBus2Strip;
    std::unique_ptr<MixerTrackStrip> mPluginsBus2Strip;
    bool                             mLayersBus2Active  { false };
    bool                             mBassBus2Active    { false };
    bool                             mClipsBus2Active   { false };
    bool                             mPluginsBus2Active { false };
    // L14: per-secondary-bus has-ever-had-route flags (project-persisted via
    // the editor's <Buses> element).  A freshly added bus with no members
    // stays visible; once it HAS had members and empties again, the layout
    // pass auto-deactivates it.
    std::map<int, bool>              mBusEverRouted;
    // Shared body for the T10 secondary-bus activations (kVoxBus2 pattern).
    bool activateGroupBus (std::unique_ptr<MixerTrackStrip>& strip, bool& activeFlag,
                           const juce::String& name, juce::Colour color,
                           const char* prefix, int chId,
                           const juce::String& deleteTitle,
                           const juce::String& parentName);
    // L14: the seven user-added secondary buses (Vox2/Inst2/Inst3 + the four
    // T10 group buses) -- the has-ever-had-route lifecycle applies to these.
    static bool isSecondaryBus (int chId);
    // Flag+visibility drop WITHOUT the param sweep (bus already empty).
    void deactivateBusFlagOnly (int chId);

    // Dynamic instrument strips - keyed by tabId (Layer/Bass) or slot (Drums)
    std::map<int, std::unique_ptr<MixerTrackStrip>> mLayerStrips;
    std::vector<int>                                mLayerTabOrder;

    std::map<int, std::unique_ptr<MixerTrackStrip>> mBassStrips;
    std::vector<int>                                mBassTabOrder;

    std::map<int, std::unique_ptr<MixerTrackStrip>> mDrumStrips;
    std::vector<int>                                mDrumSlotOrder;

    std::map<int, std::unique_ptr<MixerTrackStrip>> mAudioStrips;   // keyed by arrangement row
    std::vector<int>                                mAudioRowOrder;

    // 5F-4b B2: aux strip storage.  QA-Layout T10 (L13): the five add BUTTONS
    // are gone -- every add action lives on the strip's "Add" titled menu.
    std::map<int, std::unique_ptr<MixerTrackStrip>> mAuxStrips;
    std::vector<int>                                mAuxOrder;
    int                                             mNextAuxIdx { 0 };

    // R1 (2026-04-23): Vox + Inst strip storage.
    std::map<int, std::unique_ptr<MixerTrackStrip>> mVoxStrips;
    std::vector<int>                                mVoxOrder;
    int                                             mNextVoxIdx { 0 };

    std::map<int, std::unique_ptr<MixerTrackStrip>> mInstStrips;
    std::vector<int>                                mInstOrder;
    int                                             mNextInstIdx { 0 };


    // J-5 (2026-05-03): BaySickRustyDrums strips.  Spawned/destroyed in
    // batches of 13 by PluginProcessor::loadBaySickRustyDrumsKit /
    // destroyBaySickRustyDrums.  No "Add Rusty" UI button - strip lifecycle
    // is engine-driven, not user-driven.
    std::map<int, std::unique_ptr<MixerTrackStrip>> mRustyStrips;
    std::map<int, std::unique_ptr<MixerTrackStrip>> mPluginStrips;   // TS6
    std::map<int, std::unique_ptr<MixerTrackStrip>> mDirectStrips;   // QA-TrueLevel SC-10
    std::vector<int>                                mDirectOrder;
    std::vector<int>                                mRustyOrder;
    std::vector<int>                                mPluginOrder;   // TS6

    // Direct Routing label - shown between Master and FX Bus when any strip
    // has _sendTo = Master. Small vertical-text panel; visibility driven by
    // layoutScrollContent().
    std::unique_ptr<juce::Component>                mDirectRoutingLabel;

    // Change key folded from ALL of a strip's main-out lines (not a routing
    // value anyone reads). The timerCallback compares every tick; any delta
    // triggers layoutScrollContent() so strips visually move when a main-out
    // cable is rerouted and buses appear / disappear with the extra lines.
    std::map<int, int>                              mLastSendToCache;
    // R3.5 (2026-04-23): cable-overlay scroll-detection cache.  Drives a
    // repaint when the viewport scroll position changed since the last
    // timerCallback tick (cable sockets follow strips that scrolled).
    int                                             mLastViewportX { -1 };

    // QA-Eg (2026-05-24): cable-overlay idle-flicker fix.  Snapshot of each
    // strip's displayed peak dB from the previous vblank, used to gate the
    // overlay's repaint -- only dirty the overlay when at least one value
    // changed by > kCableRepaintEpsilonDb (0.1 dB).  On idle every meter
    // sits at the floor (-60 dB) so the snapshot stays identical frame-to-
    // frame and no repaint fires; on playback values move every frame so
    // the overlay repaints every vblank as before.  mPeakSnapshotScratch is
    // the scratch buffer onVBlank fills each frame -- swapped with
    // mLastPeakSnapshot at end so both vectors retain their capacity (no
    // per-frame heap alloc, perf-audit M1).
    std::vector<float>                              mLastPeakSnapshot;
    std::vector<float>                              mPeakSnapshotScratch;

    // QA-Eg fix-up (perf-audit H2): channelId -> strip cache.  Replaces a
    // linear scan of 12 bus pointers + 8 std::map containers on every
    // findStripByChannelId call.  Lazily rebuilt when the dirty flag is
    // set (every strip add / remove / clear).  Mutable so the cache can be
    // populated from a const member function.
    mutable std::unordered_map<int, MixerTrackStrip*> mStripByChannelId;
    mutable bool                                      mStripCacheDirty { true };

    // 5F-4b B3+B4: cable overlay - paints the patch-bay beziers.  QA-Layout
    // T10 (L12): the click-to-place send/SC modes and the main-out socket
    // drag are RETIRED (targets are picked from the per-strip "+" menu);
    // what remains is cable painting + the right-click property popup.
    struct CableOverlay : public juce::Component
    {
        MixerPage& owner;

        explicit CableOverlay(MixerPage& o);

        bool hitTest(int x, int y) override;
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;

        bool isRouteAllowed(int srcId, int dstId) const;

        // B5: find the first inactive send slot (0..3) for a strip, or -1 if full.
        int findAvailableSendSlot(const juce::String& prefix) const;
        // C.4 Phase 1: find the first empty SC receive slot on a TARGET strip
        // (target-side encoding).  Returns -1 if all 4 receive lines are full.
        int findAvailableScRecvSlot(const juce::String& targetPrefix) const;

        // ── Main-out lines ────────────────────────────────────────────────────
        // Current destination of one main-out line (line 0 = the strip's
        // permanent output; 1..3 return -1 when inactive).
        int  mainOutDest (const juce::String& prefix, int line) const;
        // First inactive extra line (1..3), or -1 when all four are in use.
        int  findAvailableMainOutLine (const juce::String& prefix) const;
        // Is dstId already fed by any of this strip's main-out lines?  Two
        // lines to one destination would sum the strip in twice, so the menu
        // refuses it.
        bool isMainOutDestInUse (const juce::String& prefix, int dstId) const;

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
            // Which of the source's main-out lines this cable is (0..3), when
            // isMainOut.  Line 0 is the strip's permanent output and has no
            // delete affordance; 1..3 can be removed from the cable popup.
            int  mainLine   = -1;
        };
        CableHit hitTestCable(juce::Point<float> pt) const;
        // QA-Eg: returns ALL cables within hit-zone of pt, in paint order
        // (SC first, then mains + sends).  Right-click uses this to pop a
        // chooser menu when multiple cables overlap; hitTestCable is the
        // convenience that returns just the first.
        std::vector<CableHit> hitTestCablesAll(juce::Point<float> pt) const;
        void showCablePopup(juce::Point<float> screenPt, const CableHit& hit);

        // QA-Eg: deep dual-stub cable path - 200 px base drop + 0.4 *
        // horizontal-distance multiplier so the cable's middle section
        // descends below the visible page area, producing a patch-bay
        // aesthetic where each end visually dangles down and slightly
        // toward the other end.  Used by every cable + ghost-cable site
        // in paint() so the math stays in one place.
        juce::Path getMixerCablePath(float startX, float startY,
                                     float destX,  float destY) const;
    };
    std::unique_ptr<CableOverlay> mCableOverlay;

    // QA-Layout T10 (L12): the per-strip "+" menu -- Send... / Sidechain... /
    // Move Output... submenus enumerating concrete legal targets.  Wired from
    // every onAddSendRequested lambda.
    void onAddCableRequestedFor(int srcChannelId);

    // Find the strip component for a given MixerChannelIds value (or nullptr).
    MixerTrackStrip* findStripByChannelId(int channelId) const;
    // QA-Eg fix-up (perf-audit H2): rebuild the channelId -> strip cache from
    // current data structures.  Called lazily on cache-miss when the dirty
    // flag is set.  Cheap: walks 12 bus pointers + 8 std::map containers once.
    void rebuildStripCache() const;

    // Get the socket position (page coords) for the bottom-center of a strip.
    // Returns {-1,-1} if the strip isn't found or isn't visible.
    juce::Point<float> getSocketPosition(int channelId) const;

    // QA-ProjectSave docket 18 (2026-07-26): empty bus strips are hidden by
    // layoutScrollContent straight off its `buckets` map -- see laidOutBus.
    // Cable sockets and strip hit-testing already reject invisible strips, so
    // that visibility flag is the only gate needed.

    // ── Internal helpers ──────────────────────────────────────────────────────
    void layoutScrollContent();
    // QA-RustyMeter Task 5 (2026-05-30): bus collapse/expand.  isBusCollapsed
    // reads the per-bus _collapsed APVTS bool; onBusCollapseToggled (fired by a
    // bus strip's arrow) flips it + relayouts.
    bool isBusCollapsed (int channelId) const;
    void onBusCollapseToggled (int channelId);
    void timerCallback() override;
    // Starts/stops the 30 Hz poll and the vblank meter feed with this page's
    // on-screen state.  Mirrors VUMeter / DBFSMeter / SlotComponent, which
    // already key their own vblank attachments off getPeer().
    void parentHierarchyChanged() override;
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

    // ── Undo context + pending before-state ───────────────────────────────────
    UndoContext  mUndoCtx;
    MixerState   mMixerStateBefore;   // captured at drag-start

    // Layout constants
    static constexpr int kFixedPanelW = 96 + 4;
    static constexpr int kSepW        = 12;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerPage)
};
