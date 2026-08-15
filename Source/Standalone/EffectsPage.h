#pragma once
#include <JuceHeader.h>
#include "../EffectRack.h"
#include "../DSP/EQ8MsDSP.h"
#include "TrackSelectionManager.h"
#include "SlotComponent.h"
#include "SharedUI.h"
#include "UndoActions.h"

class BaySickDAWProcessor;  // forward declaration - full header in .cpp
class BaySickGraph;           // QA-ProjectSave Task 7: resolveChannelDsp parameter

// ── EffectsPage — the FX rack window ──────────────────────────────────────────
// QA-ModelShell TS5 (2026-07-29), Jeff's spec.  A small window that is a rack
// INDEX, not a rack editor:
//
//   [Channel: v]                            [FX Bypass]
//   [   Pre EQ   ][   Post EQ   ]        <- each opens its own window
//   [*][ Compressor              ][^][v][V][X]   <- one row per slot, six rows
//   [*][ Empty                   ][^][v][V][X]
//   ...
//
// Row anatomy: bypass LED (the same one the slot header has always drawn), the
// effect name as a button that opens THAT EFFECT'S OWN WINDOW, then move-up /
// move-down, the picker, and remove.
//
// WHY NO PANELS HERE.  Pre-TS5 this page stacked all six editor panels at once
// inside three sub-tabs.  The windowed shell makes "one thing at a time" the
// natural unit -- Jeff: "a user can choose what they are editing at a time
// instead of everything all at once" -- so the panels and both EQs became
// windows of their own (EffectWindows.h) and what is left here is the index
// that opens them.
//
// Signal routing is unchanged: input -> slot 1 -> ... -> slot 6 -> fader -> bus.
// ─────────────────────────────────────────────────────────────────────────────
class EffectsPage : public juce::Component,
                    public juce::ChangeListener,
                    public juce::Timer,
                    private juce::AudioProcessorValueTreeState::Listener
{
public:
    EffectsPage(TrackSelectionManager& tsm, BaySickDAWProcessor& processor);
    ~EffectsPage() override;

    // Switch the channel dropdown to the named channel and update the rows.
    // Called by StandaloneEditor when a Mixer strip's FX button is clicked.
    // Accepts bus strip names ("Master", "Layers", "Bass", "Drums", "FX Bus")
    // and instrument strip names ("Layer 1", "Bass 1", etc. - mapped to their bus).
    void selectChannelByName(const juce::String& channelName);

    // Preferred entry point for mixer FX-Rack-button routing: select by the
    // strip's APVTS prefix (e.g. "mixer_layer_0"). Falls back to name lookup
    // if the prefix isn't known. Mapping is rebuilt each rebuildChannelDropdown().
    void selectChannelByApvtsPrefix(const juce::String& apvtsPrefix);

    void setRack(EffectRack* rack);
    void setUndoContext(const UndoContext& ctx) { mUndoCtx = ctx; }

    void paint(juce::Graphics&) override;
    void resized() override;

    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    // Starts/stops the row-refresh poll with this page's on-screen state.
    void parentHierarchyChanged() override;

    // ── Active channel list callback ──────────────────────────────────────────
    // Returns {effectsPageId, displayName} for every currently-active mixer strip.
    // Set by StandaloneEditor after construction; called by rebuildChannelDropdown().
    std::function<std::vector<std::pair<int,juce::String>>()> onGetActiveChannels;

    // Rebuild the channel dropdown - public so StandaloneEditor can trigger it
    // when engines are selected or drum slots change.
    void rebuildChannelDropdown();

    // ── Satellite-window requests ────────────────────────────────────────────
    // StandaloneEditor owns every contained window (it owns the Workspace), so
    // the page ASKS rather than creates.  The channel id travels with the
    // request because a window outlives this page's current selection by
    // design: switching strips here leaves already-open windows alone.
    std::function<void(int channelId, int slotIndex)> onOpenSlotPanel;
    std::function<void(int channelId, bool pre)>      onOpenEqWindow;

    // Opens the master VU meter window (rack menu, next to VU Calibration).
    std::function<void()>                             onOpenVuMeter;
    // A slot's contents changed (loaded / cleared / moved / undone), so any
    // open window on that strip must re-check what it is showing.  The windows
    // also poll for this; the callback is the immediate path, not the only one.
    std::function<void(int channelId)>                onRackContentsChanged;

    // Fills this window's title-bar menu: Save / Load FX Rack Preset, plus the
    // VU calibration submenu that used to be a "Meters" button on the old page.
    void buildTitleMenu (juce::PopupMenu& m);

    int          getCurrentChannelId() const;
    juce::String getChannelDisplayName (int channelId) const;

    // ── Channel resolution — static and view-free ────────────────────────────
    // QA-ProjectSave Task 7 step 3 (2026-07-26): channel id -> rack / EQ, lifted
    // out of onChannelChanged so automation can resolve a rack WITHOUT this page
    // being on that channel, or existing at all.  Static + lazily called: racks
    // live inside InsertNodes and die with their tab, so an applicator captures
    // the channel ID and looks the rack up per apply rather than holding a
    // pointer that would dangle.  TS5's windows resolve through the same calls
    // for the same reason.
    static void        resolveChannelDsp (BaySickGraph& vg, int id,
                                          EffectRack*& rack, EQ8MsDSP*& eq);
    static EffectRack* rackForChannelId  (BaySickGraph& vg, int id);
    // The PRE-rack EQ for a channel.  QA-ModelShell TS5: extracted from
    // onChannelChanged, where it was a second copy of the channel switch --
    // exactly the fork the batch plan says not to make.  The EQ windows and the
    // page now share this one.
    static EQ8MsDSP*   preEqForChannelId (BaySickGraph& vg, int id);

    // QA-ModelShell TS1: dropdown-channel-id -> rack-prefix vocabulary,
    // usable without the dropdown (the sweep below + any model trigger).
    static juce::String channelPrefixForId (int id);
    // 5F-4a: channel id -> mixer-strip APVTS prefix (e.g. "mixer_layer_0"), or
    // empty when the channel has no strip.  Static since TS5 -- the EQ windows
    // need it and they have no page.
    static juce::String mixerPrefixForChannelId (int id);

    // QA-ModelShell TS1 (wire-at-load): register every populated rack slot's
    // DSP-targeting automation across ALL channels.  Model-triggered after
    // rack states land at project/template load so lanes apply without the
    // Effects page ever being opened.
    static void registerRackAutomationForAllChannels (BaySickDAWProcessor& proc);

    // ── Dead hosted plugin recovery ──────────────────────────────────────────
    // A VST3 slot is dead in one of two ways and only one of them is
    // recoverable, so the two are named here rather than left to be inferred
    // from the early returns:
    //
    //   RENDERING half -- the instance exists and reports not alive (the DLL
    //     moved, it would not instantiate, it needs the bridge, or it crashed
    //     mid-session).  It still carries the FULL PluginDescription the project
    //     stored with it, so the slot knows exactly what to ask for and this is
    //     the half the call can rebuild.  It still declines when the binary the
    //     description names is not on disk -- both load routes resolve that path
    //     and would fail, so the rebuild is skipped rather than paid for (see
    //     the body's probe).  A plugin that came back at a DIFFERENT path is
    //     therefore not recovered by this call: the stored path is the only
    //     identity the slot has.  The row and window title render it as
    //     "<name> (missing)".
    //   LOADING half -- no instance was ever built, which happens only when the
    //     restore blob carried no readable description.  The slot's identity is
    //     then absent from the project itself: HostedPluginEffect holds no blob
    //     without an instance, so nothing on disk or in memory still names the
    //     plugin, and neither this call nor a project reload can recover it.
    //     The row falls through to the EffectType name ("VST3 Plugin"), so such
    //     a slot reads as a generic plugin rather than a missing one.  The
    //     retry declines rather than tearing the slot down for a rebuild that
    //     has nothing to rebuild from.
    //
    // Rebuilt through EffectRack::loadEffect with the slot's EXISTING uuid,
    // which keeps every automation lane and open window resolving; the plugin's
    // saved state is pushed back into the revived instance under the
    // project-load shield, so the rebuilt DSP is fully configured before any
    // audio block can reach it (see the body).
    //
    // EDGE-TRIGGERED ONLY -- the added-plugin list changing, or the slot menu's
    // explicit retry.  Instantiating a VST3 is expensive and a permanently
    // missing one would otherwise re-attempt forever on the row poll.
    // Message thread only.  Returns true when the slot came back alive.
    static bool retryDeadPluginSlot  (BaySickDAWProcessor& proc, int chId,
                                      EffectRack& rack, int slotIndex);
    static void retryDeadPluginSlots (BaySickDAWProcessor& proc);

    // The view-independent body: registrations capture (chId, uuid, type,
    // suffix) and resolve rack -> slot -> DSP at apply time (null-owner,
    // rack-scoped -- survives every panel/page death).
    // Takes the processor rather than just the graph because a lookahead-class
    // param has to run the same PDC refresh the panel's onLatencyChanged does,
    // and that is setLatencySamples(updateBusLatencies()) -- both halves.
    static void registerSlotAutomationFor (BaySickDAWProcessor& proc, int chId,
                                           const juce::String& channelPrefix,
                                           EffectRack& rackRef, int slotIndex);

    // Stamp automation paramIds onto a freshly mounted panel and (re)register
    // that slot's applicators.  ONE home for both halves, called from every
    // mount path -- including the ones inside SlotComponent's own menus, which
    // used to rebuild a panel and silently leave the stamps and the
    // variant-keyed registration behind.
    static void stampAndRegisterSlotEditor (BaySickDAWProcessor& proc, int chId,
                                            EffectRack& rack, int slotIndex,
                                            SlotComponent& target);

private:
    TrackSelectionManager& mTSM;
    BaySickDAWProcessor&    mProcessor;
    EffectRack*            mRack { nullptr };
    UndoContext            mUndoCtx;

    // ── Top row ───────────────────────────────────────────────────────────────
    std::unique_ptr<juce::Label>       mTrackLabel;
    std::unique_ptr<juce::ComboBox>    mTrackBox;
    std::unique_ptr<MixerLedButton>    mFxBypassBtn;

    // ── EQ entries ────────────────────────────────────────────────────────────
    std::unique_ptr<juce::TextButton>  mPreEqBtn;
    std::unique_ptr<juce::TextButton>  mPostEqBtn;

    // ── Slot rows ─────────────────────────────────────────────────────────────
    class RackSlotRow;   // defined in the .cpp
    std::array<std::unique_ptr<RackSlotRow>, EffectRack::kNumSlots> mRows;

    // Called whenever mTrackBox selection changes; re-points the rows.
    void onChannelChanged();

    void refreshAllRows();

    // Slot actions, driven from the rows.
    // QA-ModelShell TS6: the plugin entry point is separate because an
    // EffectType cannot name a plugin.  Both funnel into the same body, so undo
    // capture, automation re-registration, PDC refresh and window-open stay in
    // one place.
    void onEffectChosen (int slotIndex, EffectType type,
                         const juce::PluginDescription* pluginDesc = nullptr);
    void onPluginChosen (int slotIndex, const juce::PluginDescription&);
    void onEffectRemoved(int slotIndex);   // prompts first (Jeff spec 2026-07-29)
    void performSlotRemoval (int slotIndex);   // what the prompt's OK runs
    void onMoveRequested(int slotIndex, bool up);
    void onSlotBypassToggled (int slotIndex);
    void onSlotOpenRequested (int slotIndex);

    // The apply body every rack transaction (Load / Remove / Move) shares.
    // Takes the channel id at RECORD time and resolves the rack per apply: an
    // EffectRackAction carries slot snapshots and no channel identity, and this
    // page's selection has moved on by the time an undo arrives.  See the
    // definition for what reading the live selection instead used to destroy.
    EffectRackAction::ApplyFn makeRackApply (int chId);

    // Re-register a slot's automation after its effect identity changed.  The
    // panels no longer live here, so this is where a type/variant change gets
    // its lanes re-keyed for the rack view's own edits.
    void registerSlotAutomation (int slotIndex);
    void notifyRackContentsChanged();

    // Returns a short prefix string for the current channel, used for automation paramIds.
    // e.g. "layers_bus", "bass_bus", "master", "layer_1", "bass_2"
    juce::String getChannelPrefix() const;

    // Rack-wide preset (all six slots + both EQs).  Jeff spec 2026-07-29.
    void saveRackPreset();
    void loadRackPresetMenu (juce::PopupMenu& into);

    // 5F-4a: ButtonAttachment for FX Bypass - created/destroyed as channel changes.
    // When active, clicking the button writes to the APVTS _bypass param (which
    // InsertNode reads each audio block). When null (channel has no _bypass
    // param) the direct rack.setRackBypassed fallback in onClick is used.
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mFxBypassAtt;

    // 5F-4a: currently-attached bypass param id (empty when no channel selected
    // or channel has no _bypass param). Used to remove the listener on rebind.
    juce::String mBypassParamId;

    // 5F-4a: listener that mirrors APVTS _bypass changes to rack.setRackBypassed().
    void parameterChanged(const juce::String& paramId, float newValue) override;

    // Map: dropdown item id → APVTS prefix. Rebuilt each rebuildChannelDropdown()
    // so selectChannelByApvtsPrefix() can find the right entry unambiguously.
    std::map<int, juce::String> mIdToApvtsPrefix;
    // Map: dropdown item id → display name, so a window can title itself with
    // the strip it belongs to long after the page moved to another channel.
    std::map<int, juce::String> mIdToDisplayName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectsPage)
};
