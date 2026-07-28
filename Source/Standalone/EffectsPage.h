#pragma once
#include <JuceHeader.h>
#include "../EffectRack.h"
#include "../DSP/EQ8MsDSP.h"
#include "TrackSelectionManager.h"
#include "SlotComponent.h"
#include "SharedUI.h"
#include "UndoActions.h"

class VibeSynthProcessor;  // forward declaration - full header in .cpp
class VibeGraph;           // QA-ProjectSave Task 7: resolveChannelDsp parameter

// ── EffectsPage ───────────────────────────────────────────────────────────────
// Permanent system tab (Effects). Layout:
//
//   [Channel: ComboBox]                    [FX master bypass toggle]
//   ─────────────────────────────────────────────────────────────────
//   [Tab: Effects Rack (full width)] [Tab: EQ]
//   ─────────────────────────────────────────
//   Slot 1  (empty: shows "+", loaded: header + inline knobs)
//   Slot 2  ...
//   Slot 3
//   Slot 4
//   Slot 5
//   Slot 6
//   ─────── (EQ tab) ──────────────
//   ParametricEQDisplay (Mid/Side toggle)
//
// Signal routing (per plan): input → slot 1 → slot 2 → ... → slot 6 → fader → buss
// ─────────────────────────────────────────────────────────────────────────────
class EffectsPage : public juce::Component,
                    public juce::ChangeListener,
                    public juce::Timer,
                    private juce::AudioProcessorValueTreeState::Listener
{
public:
    EffectsPage(TrackSelectionManager& tsm, VibeSynthProcessor& processor);
    ~EffectsPage() override;

    // 2026-04-26 (B-5): Ctrl+Z / Ctrl+Alt+Z migrated to global BSCommands;
    // the per-page KeyListener-on-top-level dance is gone.

    // Switch the channel dropdown to the named channel and update rack + EQ.
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

    // ── Active channel list callback ──────────────────────────────────────────
    // Returns {effectsPageId, displayName} for every currently-active mixer strip.
    // Set by StandaloneEditor after construction; called by rebuildChannelDropdown().
    std::function<std::vector<std::pair<int,juce::String>>()> onGetActiveChannels;

    // Rebuild the channel dropdown - now public so StandaloneEditor can trigger it
    // when engines are selected or drum slots change.
    void rebuildChannelDropdown();

    // ── Public API for PageMenuBar integration ────────────────────────────────
    int  getActiveTab()   const { return mCurrentTab; }
    void switchTab(int index);
    void setEQMid(bool showMid);
    bool isEQMidActive()  const { return mShowingMid; }

    // Non-owning component pointers for injection into PageMenuBar
    juce::Component* getTrackLabel()  const { return mTrackLabel.get(); }
    juce::Component* getTrackBox()    const { return mTrackBox.get();   }
    juce::Component* getFxBypassBtn() const { return mFxBypassBtn.get();}
    juce::Component* getMetersBtn()   const { return mMetersBtn.get();  }
    // 2026-04-19: PageMenuBar's universal hamburger needs the EQ display so
    // StandaloneEditor's tab-click lambda can install / uninstall its menu.
    ParametricEQDisplay* getEQDisplay() const { return mEQDisplay.get(); }

private:
    TrackSelectionManager& mTSM;
    VibeSynthProcessor&    mProcessor;
    EffectRack*            mRack { nullptr };
    UndoContext            mUndoCtx;

    // ── Top bar ───────────────────────────────────────────────────────────────
    std::unique_ptr<juce::Label>       mTrackLabel;
    std::unique_ptr<juce::ComboBox>    mTrackBox;
    std::unique_ptr<MixerLedButton>    mFxBypassBtn;
    std::unique_ptr<juce::TextButton>  mMetersBtn;   // "Meters v" popup menu

    // §P4.3 (B6.2): TabKind models the 3 possible EffectsPage sub-tabs.  Tab
    // index in setTabSlots is dynamic - Aux/Audio/Bus get all 3 (Pre/Rack/Post),
    // Layer/Bass/Drum-slot get 2 (Rack/Post) since their pre-EQ lives on the
    // player page.  Use TabKind internally to avoid index-meaning ambiguity.
public:
    enum class TabKind { PreEQ, Rack, PostEQ };
    bool currentChannelHasPagePreEQ() const;   // true for Layer/Bass/Drum channels
    ParametricEQDisplay* getPreEQDisplay() const { return mPreEQDisplay.get(); }
    // Switch by TabKind (preferred) or by visible-tab index (StandaloneEditor's
    // tab-callback receives the visible index - switchTab(int) translates).
    void switchTab (TabKind kind);
    TabKind tabKindForVisibleIndex (int visibleIndex) const;
    int     visibleIndexForTabKind (TabKind kind) const;
    // Fired when the visible tab list needs to refresh (channel change altered
    // 2-vs-3 layout).  StandaloneEditor wires this to re-call setTabSlots.
    std::function<void()> onTabsNeedRefresh;
private:

    TabKind mCurrentTabKind { TabKind::Rack };
    int mCurrentTab { 0 };  // legacy raw index - kept in sync with mCurrentTabKind

    // 2026-04-26: per-channel sub-tab persistence.  Keyed by channel ID
    // (1..6 buses, 100+ inserts, 200+ layers, 300+ basses, 400+ audio,
    // 600+ aux).  When the channel selection changes, the previous channel's
    // current TabKind is saved; the new channel restores its saved TabKind
    // (default Rack on first visit).
    std::map<int, TabKind> mLastTabPerChannel;
    int mPrevChannelId { 0 };   // tracks last channel for save-on-change

    // ── Rack tab content (full width) ─────────────────────────────────────────
    std::unique_ptr<juce::Component>               mRackTab;
    std::array<std::unique_ptr<SlotComponent>, 6>  mSlots;

    // ── Post-rack EQ tab content ──────────────────────────────────────────────
    std::unique_ptr<juce::Component>     mEQTab;
    std::unique_ptr<ParametricEQDisplay> mEQDisplay;
    // Owned M/S EQ DSP (display fallback before channel binding lands)
    EQ8MsDSP                             mEffectsEQDsp;
    bool                                 mShowingMid { true };

    // ── Pre-rack EQ tab content (B6.2) ────────────────────────────────────────
    // Visible only on Aux/Audio/Bus channels (player-pre-EQ lives on the page).
    std::unique_ptr<juce::Component>     mPreEQTab;
    std::unique_ptr<ParametricEQDisplay> mPreEQDisplay;
    EQ8MsDSP                             mPreEffectsEQDsp;   // display fallback

    // Called whenever mTrackBox selection changes; wires rack + EQ to selected channel
    void onChannelChanged();

public:
    // QA-ProjectSave Task 7 step 3 (2026-07-26): channel id -> rack / EQ, lifted
    // out of onChannelChanged so automation can resolve a rack WITHOUT this page
    // being on that channel, or existing at all.  Static + lazily called: racks
    // live inside InsertNodes and die with their tab, so an applicator captures
    // the channel ID and looks the rack up per apply rather than holding a
    // pointer that would dangle.
    static void        resolveChannelDsp (VibeGraph& vg, int id,
                                          EffectRack*& rack, EQ8MsDSP*& eq);
    static EffectRack* rackForChannelId  (VibeGraph& vg, int id);

    // QA-ModelShell TS1: dropdown-channel-id -> rack-prefix vocabulary,
    // usable without the dropdown (the sweep below + any model trigger).
    static juce::String channelPrefixForId (int id);

    // QA-ModelShell TS1 (wire-at-load): register every populated rack slot's
    // DSP-targeting automation across ALL channels.  Model-triggered after
    // rack states land at project/template load so lanes apply without the
    // Effects page ever being opened.
    static void registerRackAutomationForAllChannels (VibeSynthProcessor& proc);

private:
    // Registers DSP-targeting applicators/readers for one rack slot's mapped
    // parameters.  Called whenever a slot editor is (re)built, which is also
    // when the slot's effect type can have changed.
    void registerSlotAutomation (int slotIndex);

    // The view-independent body: registrations capture (chId, uuid, type,
    // suffix) and resolve rack -> slot -> DSP at apply time (null-owner,
    // rack-scoped -- survives every panel/page death).
    static void registerSlotAutomationFor (VibeGraph& vg, int chId,
                                           const juce::String& channelPrefix,
                                           EffectRack& rackRef, int slotIndex);

public:

private:


    void buildRackTab();
    void buildEQTab();
    void buildPreEQTab();   // §P4.3 (B6.2)
    void showMetersMenu();   // shows VU calibration popup

    // Slot callbacks
    void onEffectChosen (int slotIndex, EffectType type);
    void onEffectRemoved(int slotIndex);
    void onMoveRequested(int slotIndex, bool up);

    // Rebuild the inline editor for one slot (call after loadEffect/clearSlot/swap)
    void rebuildSlotEditor(int slotIndex);
    void refreshAllSlots();

    // Returns a short prefix string for the current channel, used for automation paramIds.
    // e.g. "layers_bus", "bass_bus", "master", "layer_1", "bass_2"
    juce::String getChannelPrefix() const;

    // 5F-4a: returns the mixer-strip APVTS prefix (e.g. "mixer_layer_0") for the
    // given effects-page channel id, or empty if unknown.
    juce::String getMixerApvtsPrefixForChannel(int effectsPageId) const;

    // 5F-4a: ButtonAttachment for FX Bypass - created/destroyed as channel changes.
    // When active, clicking the button writes to the APVTS _bypass param (which
    // InsertNode reads each audio block). When null (channel has no _bypass
    // param) the direct rack.setRackBypassed fallback in onClick is used.
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mFxBypassAtt;

    // 5F-4a: currently-attached bypass param id (empty when no channel selected
    // or channel has no _bypass param). Used to remove the listener on rebind.
    juce::String mBypassParamId;

    // 5F-4a: listener that mirrors APVTS _bypass changes to rack.setRackBypassed().
    // Without this, clicking the button writes APVTS but the rack stays unchanged
    // until Batch 6 wires InsertNode's per-block read.
    void parameterChanged(const juce::String& paramId, float newValue) override;

    // Map: dropdown item id → APVTS prefix. Rebuilt each rebuildChannelDropdown()
    // so selectChannelByApvtsPrefix() can find the right entry unambiguously.
    std::map<int, juce::String> mIdToApvtsPrefix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectsPage)
};
