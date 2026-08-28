#include "EffectsPage.h"
#include "EffectEditorPanels.h"
#include "FxRackPresetIO.h"
#include "../PluginProcessor.h"
#include "../MissingFileReport.h"
#include "../DSP/EffectParamMap.h"   // QA-ProjectSave Task 7 step 3
#include "../Hosting/HostedPluginEffect.h"   // QA-ModelShell TS6: hosted plugin lanes

// ── Channel dropdown items (order matches onChannelChanged switch) ─────────────
// ID 1 = Layers Bus, 2 = Bass Bus, 3 = Drums Bus, 4 = Master, 5 = Effects Bus

// ── RackSlotRow — one line of the rack window (QA-ModelShell TS5) ─────────────
// [LED] [ effect name (opens its window) ] [^] [v] [pick] [x]
//
// Everything is painted and hit-tested rather than built from child buttons,
// matching how the slot header has always drawn its own ▲▼× cluster.  The
// glyphs are VECTOR PATHS, not text: a font glyph for a chevron or a cross is
// outside ASCII and renders as a box on a machine without it, and these are the
// only affordance a row has.
class EffectsPage::RackSlotRow : public juce::Component,
                                 public juce::TooltipClient
{
public:
    explicit RackSlotRow (int slotIndex) : mSlotIndex (slotIndex) {}

    std::function<void(int slot)>            onOpen, onPick, onRemove, onBypass;
    std::function<void(int slot, bool up)>   onMove;

    void setState (bool loaded, bool bypassed, juce::String name)
    {
        if (loaded == mLoaded && bypassed == mBypassed && name == mName) return;
        mLoaded = loaded; mBypassed = bypassed; mName = std::move (name);
        repaint();
    }

    juce::String getTooltip() override
    {
        const auto p = getMouseXYRelative();
        if (mLedR   .contains (p)) return mBypassed ? "Bypassed - click to enable"
                                                    : "Active - click to bypass";
        if (mUpR    .contains (p)) return "Move this effect earlier in the chain";
        if (mDownR  .contains (p)) return "Move this effect later in the chain";
        if (mPickR  .contains (p)) return mLoaded ? "Choose a different effect"
                                                  : "Choose an effect for this slot";
        if (mCloseR .contains (p)) return "Remove this effect";
        if (mNameR  .contains (p)) return mLoaded ? "Open this effect in its own window"
                                                  : "Empty slot - click to choose an effect";
        return {};
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (VC::Panel);
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (VC::Accent.withAlpha (mLoaded ? 0.55f : 0.25f));
        g.drawRoundedRectangle (b, 3.0f, 1.0f);

        if (mLoaded)
            EffectBypassLed::paint (g, mLedR, mBypassed);

        // Name plate.  Reads as a button because it is one -- this is the route
        // to the effect's own window.
        auto nameR = mNameR.toFloat().reduced (1.0f);
        g.setColour (mLoaded ? VC::Bg.withAlpha (0.55f) : VC::Bg.withAlpha (0.25f));
        g.fillRoundedRectangle (nameR, 2.0f);
        g.setColour (mLoaded ? VC::Text : VC::TextDim);
        g.setFont (juce::Font (12.0f, mLoaded ? juce::Font::bold : juce::Font::plain));
        g.drawText (mLoaded ? mName : juce::String ("Empty"),
                    mNameR.reduced (6, 0), juce::Justification::centredLeft, true);

        g.setColour (mLoaded ? VC::TextDim : VC::TextDim.withAlpha (0.4f));
        drawTriangle (g, mUpR,   true);
        drawTriangle (g, mDownR, false);
        drawChevron  (g, mPickR);
        if (mLoaded)
        {
            g.setColour (juce::Colour (0xffcc4444));
            drawCross (g, mCloseR);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (3, 2);
        mLedR   = b.removeFromLeft (18);
        b.removeFromLeft (3);
        mCloseR = b.removeFromRight (18);
        mPickR  = b.removeFromRight (20);
        mDownR  = b.removeFromRight (16);
        mUpR    = b.removeFromRight (16);
        b.removeFromRight (4);
        mNameR  = b;

        // QA-ManualPress M-4: every affordance on a row is PAINTED, so the
        // manual's per-affordance callouts declare the rects just laid out.
        // They ride ONE row -- the row above carries the whole-slot callout, and
        // stamping all six rows would collide the dots on an arbitrary one.
        if (mSlotIndex == 1)
        {
            auto rect = [] (const char* id, juce::Rectangle<int> r)
            {
                return juce::String (id) + "@" + juce::String (r.getX())
                     + "," + juce::String (r.getY())
                     + "," + juce::String (r.getWidth())
                     + "," + juce::String (r.getHeight());
            };
            getProperties().set (kDotAnchor,
                                 rect ("FXI-6",  mNameR)
                       + ";"   + rect ("FXI-7",  mUpR.getUnion (mDownR))
                       + ";"   + rect ("FXI-8",  mPickR)
                       + ";"   + rect ("FXI-9",  mLedR)
                       + ";"   + rect ("FXI-10", mCloseR));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.getPosition();
        if (mLoaded && mLedR.contains (p))   { if (onBypass) onBypass (mSlotIndex); return; }
        if (mLoaded && mUpR.contains (p))    { if (onMove)   onMove (mSlotIndex, true);  return; }
        if (mLoaded && mDownR.contains (p))  { if (onMove)   onMove (mSlotIndex, false); return; }
        if (mLoaded && mCloseR.contains (p)) { if (onRemove) onRemove (mSlotIndex); return; }
        if (mPickR.contains (p))             { if (onPick)   onPick (mSlotIndex); return; }
        if (mNameR.contains (p))
        {
            // An empty row's name plate is the picker too: a dead region where
            // the obvious click lands is worse than a second route to the menu.
            if (mLoaded) { if (onOpen) onOpen (mSlotIndex); }
            else         { if (onPick) onPick (mSlotIndex); }
        }
    }

private:
    static void drawTriangle (juce::Graphics& g, juce::Rectangle<int> r, bool pointUp)
    {
        auto a = r.toFloat().reduced (4.0f, 5.0f);
        juce::Path p;
        if (pointUp)
        {
            p.startNewSubPath (a.getCentreX(), a.getY());
            p.lineTo (a.getRight(), a.getBottom());
            p.lineTo (a.getX(),     a.getBottom());
        }
        else
        {
            p.startNewSubPath (a.getX(),       a.getY());
            p.lineTo (a.getRight(),            a.getY());
            p.lineTo (a.getCentreX(),          a.getBottom());
        }
        p.closeSubPath();
        g.fillPath (p);
    }

    static void drawChevron (juce::Graphics& g, juce::Rectangle<int> r)
    {
        auto a = r.toFloat().reduced (5.0f, 6.0f);
        juce::Path p;
        p.startNewSubPath (a.getX(),        a.getY());
        p.lineTo          (a.getCentreX(),  a.getBottom());
        p.lineTo          (a.getRight(),    a.getY());
        g.strokePath (p, juce::PathStrokeType (1.6f));
    }

    static void drawCross (juce::Graphics& g, juce::Rectangle<int> r)
    {
        auto a = r.toFloat().reduced (5.0f);
        g.drawLine (a.getX(), a.getY(), a.getRight(), a.getBottom(), 1.6f);
        g.drawLine (a.getX(), a.getBottom(), a.getRight(), a.getY(), 1.6f);
    }

    const int    mSlotIndex;
    bool         mLoaded   { false };
    bool         mBypassed { false };
    juce::String mName;
    juce::Rectangle<int> mLedR, mNameR, mUpR, mDownR, mPickR, mCloseR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RackSlotRow)
};

// Row pitch for the rack window's fixed-height list.
static constexpr int kRowH = 24;

// ── Ctor ──────────────────────────────────────────────────────────────────────
EffectsPage::EffectsPage(TrackSelectionManager& tsm, BaySickDAWProcessor& processor)
    : mTSM(tsm), mProcessor(processor)
{
    mTSM.addChangeListener(this);

    // The added-plugin list is the "user put the missing plugin back" signal --
    // see retryDeadPluginSlot.  Registered from this page because it is one of
    // the four system tabs, built with the editor and alive for the app's life,
    // so the sweep cannot go unheard while a rack still holds a dead slot.
    if (auto* pm = Hosting::PluginManager::getInstance())
        pm->addChangeListener (this);

    // ── Top bar ───────────────────────────────────────────────────────────────
    mTrackLabel = std::make_unique<juce::Label>();
    mTrackLabel->setText("Channel:", juce::dontSendNotification);
    mTrackLabel->setFont(juce::Font(12.0f));
    mTrackLabel->setColour(juce::Label::textColourId, VC::TextDim);
    addAndMakeVisible(*mTrackLabel);

    mTrackBox = std::make_unique<juce::ComboBox>();
    mTrackBox->setLookAndFeel(&BaySickLAF::get());
    mTrackBox->setTooltip("Select channel to edit effects / EQ");
    mTrackBox->onChange = [this] { onChannelChanged(); };
    addAndMakeVisible(*mTrackBox);

    // Callback wired here; actual dropdown build happens after all components exist (end of ctor)
    mProcessor.mVibeGraph.onInstrChannelListChanged = [this] { rebuildChannelDropdown(); };

    mFxBypassBtn = std::make_unique<MixerLedButton>();
    mFxBypassBtn->setButtonText("FX Bypass");
    mFxBypassBtn->setOnColour(juce::Colour(0xff4488ff));   // blue, matches mixer strip LED
    mFxBypassBtn->setTooltip("Bypass entire effects rack for the channel selected above");
    mFxBypassBtn->setClickingTogglesState(true);
    mFxBypassBtn->onClick = [this]
    {
        // 5F-4a: if the APVTS attachment is active, the button's toggle state
        // is already two-way-synced with the _bypass param and InsertNode will
        // update the rack on the next audio block. Nothing more to do.
        if (mFxBypassAtt != nullptr) return;

        // Fallback (channel has no _bypass param - shouldn't happen after 5F-4a
        // since all strips now get _bypass, but keep as a safety net).
        if (mRack)
            mRack->setRackBypassed(mFxBypassBtn->getToggleState());
    };
    addAndMakeVisible(*mFxBypassBtn);

    // ── EQ entries ────────────────────────────────────────────────────────────
    // Buttons, not tabs: each opens its EQ in its own window, and both can be
    // open at once (Jeff spec 2026-07-29).
    auto makeEqBtn = [this] (std::unique_ptr<juce::TextButton>& btn,
                             const juce::String& label, bool pre)
    {
        btn = std::make_unique<juce::TextButton> (label);
        btn->setLookAndFeel (&BaySickLAF::get());
        btn->setTooltip (pre ? "Open this channel's pre-rack EQ in its own window"
                             : "Open this channel's post-rack EQ in its own window");
        btn->onClick = [this, pre]
        {
            if (onOpenEqWindow) onOpenEqWindow (getCurrentChannelId(), pre);
        };
        addAndMakeVisible (*btn);
    };
    makeEqBtn (mPreEqBtn,  "Pre EQ",  true);
    makeEqBtn (mPostEqBtn, "Post EQ", false);

    // QA-ManualPress M-4: the rack window's own callouts anchor to its controls.
    mTrackBox   ->getProperties().set (kDotAnchor, "FXI-1");
    mFxBypassBtn->getProperties().set (kDotAnchor, "FXI-2");
    mPreEqBtn   ->getProperties().set (kDotAnchor, "FXI-3");
    mPostEqBtn  ->getProperties().set (kDotAnchor, "FXI-4");

    // ── Slot rows ─────────────────────────────────────────────────────────────
    for (int i = 0; i < EffectRack::kNumSlots; ++i)
    {
        auto row = std::make_unique<RackSlotRow> (i);
        row->onOpen   = [this] (int s) { onSlotOpenRequested (s); };
        row->onBypass = [this] (int s) { onSlotBypassToggled (s); };
        row->onRemove = [this] (int s) { onEffectRemoved (s); };
        row->onMove   = [this] (int s, bool up) { onMoveRequested (s, up); };
        row->onPick   = [this] (int s)
        {
            SlotComponent::showEffectPickerMenu (juce::Desktop::getMousePosition(),
                                                 [this, s] (EffectType t) { onEffectChosen (s, t); },
                                                 [this, s] (const juce::PluginDescription& d)
                                                 { onPluginChosen (s, d); });
        };
        // QA-ManualPress M-4: "a rack slot" is the whole row, numbered on the
        // first one; the row below carries the per-affordance callouts.
        if (i == 0) row->getProperties().set (kDotAnchor, "FXI-5");
        addAndMakeVisible (*row);
        mRows[(size_t) i] = std::move (row);
    }

    rebuildChannelDropdown();  // build channel list + point the rows at a rack

    // Timer start is owned by parentHierarchyChanged -- see the comment there.
}

void EffectsPage::parentHierarchyChanged()
{
    // Peer-keyed suspend, matching MixerPage and the meter widgets: a page that
    // is built but not on screen (windows open lazily, and a closed window
    // leaves its page alive) must not keep polling.
    if (getPeer() != nullptr)
    {
        // 10 Hz, down from the 30 the EQ displays needed: this window only has
        // to notice rack changes made elsewhere, and the EQ polling left with
        // the EQs.
        if (! isTimerRunning()) startTimerHz (10);
    }
    else if (isTimerRunning())
    {
        stopTimer();
    }
}

EffectsPage::~EffectsPage()
{
    stopTimer();
    // 2026-04-26 (B-5): no more per-page KeyListener on the top-level - undo
    // / redo route through the global BSCommands manager, so there is no key
    // listener to detach here.
    if (mBypassParamId.isNotEmpty())
        mProcessor.apvts.removeParameterListener(mBypassParamId, this);
    mProcessor.mVibeGraph.onInstrChannelListChanged = nullptr;   // clear before destruction
    mTSM.removeChangeListener(this);
    if (auto* pm = Hosting::PluginManager::getInstance())
        pm->removeChangeListener (this);
    mTrackBox->setLookAndFeel(nullptr);
    // MixerLedButton doesn't use LAF, nothing to clean up here
    if (mPreEqBtn)  mPreEqBtn ->setLookAndFeel(nullptr);
    if (mPostEqBtn) mPostEqBtn->setLookAndFeel(nullptr);
}

// The three sub-tabs (Pre EQ / Rack / Post EQ) and their visible-index
// translation retired with the TS5 rebuild: the rack is the window, and each EQ
// is its own window opened from the two buttons above the rows.  Nothing
// translates an index to a tab kind any more because there are no tabs.

// ── Channel list rebuild ──────────────────────────────────────────────────────
void EffectsPage::rebuildChannelDropdown()
{
    const int prevId = mTrackBox->getSelectedId();
    mTrackBox->clear(juce::dontSendNotification);
    mIdToApvtsPrefix.clear();
    mIdToDisplayName.clear();

    // Attach the colored-section LAF to the dropdown popup so addSectionHeading
    // strings encoded via ColoredSectionLAF::encode() render with a color line.
    mTrackBox->setLookAndFeel(&ColoredSectionLAF::get());

    if (onGetActiveChannels)
    {
        auto channels = onGetActiveChannels();

        // Group strips by their current main-out destination (read _sendTo
        // APVTS for each; fall back to MixerChannelIds::defaultSendTo when absent).
        using namespace MixerChannelIds;
        struct Item { int id; juce::String name; juce::String prefix; };
        std::map<int, std::vector<Item>> buckets;

        auto readSendTo = [&](const juce::String& prefix, int fallback) -> int {
            if (auto* p = mProcessor.apvts.getRawParameterValue(prefix + "_sendTo"))
                return (int) p->load();
            return fallback;
        };

        auto channelToMixerId = [](int dropdownId) -> int {
            // Bus IDs in the dropdown match MixerChannelIds 1-18 directly.
            if (dropdownId >= 1 && dropdownId <= kDrumsBus2) return dropdownId;
            if (dropdownId >= 100 && dropdownId < 200) return kDrumBase + (dropdownId - 100);
            if (dropdownId >= 200 && dropdownId < 216) return kLayerBase + (dropdownId - 200);
            if (dropdownId >= 300 && dropdownId < 316) return kBassBase  + (dropdownId - 300);
            if (dropdownId >= 400 && dropdownId < 500) return kAudioBase + (dropdownId - 400);
            if (dropdownId >= 600 && dropdownId < 600 + (int) kMaxAuxStrips)   return kAuxBase   + (dropdownId - 600);
            if (dropdownId >= 700 && dropdownId < 700 + (int) kMaxVoxStrips)   return kVoxBase   + (dropdownId - 700);
            if (dropdownId >= 800 && dropdownId < 800 + (int) kMaxInstStrips)  return kInstBase  + (dropdownId - 800);
            if (dropdownId >= 900 && dropdownId < 900 + (int) kMaxRustyStrips) return kRustyBase + (dropdownId - 900);
            if (dropdownId >= 1000 && dropdownId < 1000 + (int) kMaxPluginStrips) return kPluginBase + (dropdownId - 1000);
            if (dropdownId >= 1100 && dropdownId < 1100 + (int) kMaxDirectStrips) return kDirectBase + (dropdownId - 1100);
            return dropdownId;
        };

        // Separate buses from inserts - buses are group anchors, inserts go in buckets.
        // The upper bound is the highest bus channel id; anything a bus id is
        // not gets bucketed as an insert, so a bus left out here silently loses
        // its own group anchor and shows up under DIRECT ROUTING instead.
        std::vector<Item> busItems;
        std::vector<Item> insertItems;
        for (auto& [id, name] : channels)
        {
            juce::String prefix = mixerPrefixForChannelId(id);
            Item it { id, name, prefix };
            if (id >= 1 && id <= kDrumsBus2) busItems.push_back(it);
            else                             insertItems.push_back(it);
        }

        for (auto& it : insertItems)
        {
            const int mixerId = channelToMixerId(it.id);
            const int dest = readSendTo(it.prefix, defaultSendTo(mixerId));
            buckets[dest].push_back(it);
        }

        auto findBus = [&](int dropdownId) -> const Item* {
            for (auto& b : busItems) if (b.id == dropdownId) return &b;
            return nullptr;
        };

        auto addItemWithPrefix = [&](const Item& it) {
            mTrackBox->addItem(it.name, it.id);
            if (it.prefix.isNotEmpty()) mIdToApvtsPrefix[it.id] = it.prefix;
            // QA-ModelShell TS5: satellite windows title themselves with the
            // strip they belong to, and they outlive this page's selection --
            // so the name has to be answerable per id, not just for "current".
            mIdToDisplayName[it.id] = it.name;
        };

        // Track whether this is the first group we render - used to inject a
        // blank spacer *before* every group except the first. This keeps items
        // flush under their own colored heading but gives visual breathing
        // room between groups.
        bool firstGroup = true;
        auto addSpacer = [&]() {
            if (firstGroup) { firstGroup = false; return; }
            mTrackBox->addSectionHeading(
                ColoredSectionLAF::encode(juce::Colours::transparentBlack, ""));
        };

        auto addBusAndMembers = [&](int busDropdownId, int busMixerChId,
                                     const juce::String& title, juce::Colour color)
        {
            addSpacer();
            mTrackBox->addSectionHeading(ColoredSectionLAF::encode(color, title));
            if (auto* bus = findBus(busDropdownId)) addItemWithPrefix(*bus);
            if (auto it = buckets.find(busMixerChId); it != buckets.end())
                for (auto& m : it->second) addItemWithPrefix(m);
        };

        // ── Master ───────────────────────────────────────────────────────
        {
            addSpacer();
            mTrackBox->addSectionHeading(
                ColoredSectionLAF::encode(juce::Colour(0xff7b2fbe), "MASTER"));
            if (auto* master = findBus(4)) addItemWithPrefix(*master);
        }

        // ── Direct Routing (strips re-routed to Master) ──────────────────
        if (auto it = buckets.find(kMaster); it != buckets.end() && !it->second.empty())
        {
            addSpacer();
            mTrackBox->addSectionHeading(
                ColoredSectionLAF::encode(VC::Accent, "DIRECT ROUTING"));
            for (auto& m : it->second) addItemWithPrefix(m);
        }

        // ── FX Bus ────────────────────────────────────────────────────────
        addBusAndMembers(5, kFxBus, "FX BUS", juce::Colour(0xffce3f8e));
        // Aux-to-aux chains live visually with FX group
        for (auto& [dst, members] : buckets)
            if (dst >= kAuxBase && dst < kAuxBase + kMaxAuxStrips)
                for (auto& m : members) addItemWithPrefix(m);

        // ── Clips Bus ─────────────────────────────────────────────────────
        addBusAndMembers(6, kClipsBus, "CLIPS BUS", juce::Colour(0xffd4a017));
        addBusAndMembers(16, kClipsBus2, "CLIPS BUS 2", juce::Colour(0xffd4a017));   // T10

        // ── Layers Bus ────────────────────────────────────────────────────
        addBusAndMembers(1, kLayersBus, "LAYERS BUS", VC::LayerCol[0]);
        addBusAndMembers(14, kLayersBus2, "LAYERS BUS 2", VC::LayerCol[0]);   // T10

        // ── Bass Bus ──────────────────────────────────────────────────────
        addBusAndMembers(2, kBassBus, "BASS BUS", VC::BassCol[0]);
        addBusAndMembers(15, kBassBus2, "BASS BUS 2", VC::BassCol[0]);   // T10

        // ── Drums Bus ─────────────────────────────────────────────────────
        addBusAndMembers(3, kDrumsBus, "DRUMS BUS", VC::DrumsCol);
        addBusAndMembers(18, kDrumsBus2, "DRUMS BUS 2", VC::DrumsCol);   // QA-SOUNDNESS

        // J-6 (2026-05-03): EQ unification + missing bus groups.
        // ── RustyDrums Bus (J-5) ─────────────────────────────────────────
        addBusAndMembers(12, kRustyDrumsBus, "RUSTYDRUMS BUS", VC::DrumsCol);

        // ── Plugins Bus (QA-ModelShell TS6, BLU-447) ─────────────────────
        addBusAndMembers(13, kPluginsBus, "PLUGINS BUS", VC::Purple);
        addBusAndMembers(17, kPluginsBus2, "PLUGINS BUS 2", VC::Purple);   // T10

        // ── Vox Bus(es) (R3.5 + G-6) ─────────────────────────────────────
        addBusAndMembers(7, kVoxBus,   "VOX BUS",   juce::Colour(0xFF0FAFA5));
        addBusAndMembers(9, kVoxBus2,  "VOX BUS 2", juce::Colour(0xFF0FAFA5));

        // ── Inst Bus(es) (R3.5 + G-6) ────────────────────────────────────
        addBusAndMembers(8,  kInstBus,  "INST BUS",   juce::Colour(0xFF1C3A8A));
        addBusAndMembers(10, kInstBus2, "INST BUS 2", juce::Colour(0xFF1C3A8A));
        addBusAndMembers(11, kInstBus3, "INST BUS 3", juce::Colour(0xFF1C3A8A));
    }
    else
    {
        // ── Fallback: fixed list (no callback set) ─────────────────────────────
        mTrackBox->addSectionHeading("Bus Channels");
        mTrackBox->addItem("Master",           4);
        mTrackBox->addItem("Layers Bus",       1);
        mTrackBox->addItem("Bass Bus",         2);
        mTrackBox->addItem("Drums Bus",        3);
        mTrackBox->addItem("Effects Bus",      5);
        mTrackBox->addItem("Audio Clips Bus",  6);
    }

    const int newSel = (mTrackBox->indexOfItemId(prevId) >= 0) ? prevId : 4;
    mTrackBox->setSelectedId(newSel, juce::dontSendNotification);
    onChannelChanged();
}

void EffectsPage::selectChannelByApvtsPrefix(const juce::String& apvtsPrefix)
{
    if (apvtsPrefix.isEmpty())
    {
        selectChannelByName("Master");
        return;
    }
    for (const auto& [id, prefix] : mIdToApvtsPrefix)
    {
        if (prefix == apvtsPrefix)
        {
            mTrackBox->setSelectedId(id, juce::sendNotification);
            return;
        }
    }
    // Fallback: try the legacy name-based path (prefix may actually be a name)
    selectChannelByName(apvtsPrefix);
}

// ── Channel switching ─────────────────────────────────────────────────────────
// QA-ProjectSave Task 7 step 3 (2026-07-26): channel id -> rack, lifted out of
// onChannelChanged so automation can resolve a rack WITHOUT the Effects page
// being on that channel (or existing at all).
//
// Deliberately resolved LAZILY at apply time rather than captured at
// registration: racks live inside InsertNodes and die with their tab, so a
// captured EffectRack* would dangle.  A lookup that returns nullptr just
// no-ops, which the dead-lane logging already reports.  Same shape as
// Ardour's automation_control(param, create) -- the lane names a key and the
// key resolves to a live control when one is needed.
void EffectsPage::resolveChannelDsp (BaySickGraph& vg, int id,
                                     EffectRack*& rack, StripEq*& eq)
{
    rack = nullptr;
    eq   = nullptr;
    // IDs 1-18: fixed bus channels (dropdown ids match MixerChannelIds)
    switch (id)
    {
    case 1:  rack = vg.getLayersBusRack();      eq = vg.getLayersBusEQ();     break;
    case 2:  rack = vg.getBassBusRack();        eq = vg.getBassBusEQ();       break;
    case 3:  rack = vg.getDrumsBusRack();       eq = vg.getDrumsBusEQ();      break;
    case 4:  rack = vg.getMasterRack();         eq = vg.getMasterEQ();        break;
    case 5:  rack = vg.getEffectsBusRack();     eq = vg.getEffectsBusEQ();    break;
    case 6:  rack = vg.getAudioClipsBusRack();  eq = vg.getAudioClipsBusEQ(); break;
    case 7:  rack = vg.getVoxBusRack();         eq = vg.getVoxBusEQ();        break;
    case 8:  rack = vg.getInstBusRack();        eq = vg.getInstBusEQ();       break;
    case 9:  rack = vg.getVoxBus2Rack();        eq = vg.getVoxBus2EQ();       break;
    case 10: rack = vg.getInstBus2Rack();       eq = vg.getInstBus2EQ();      break;
    case 11: rack = vg.getInstBus3Rack();       eq = vg.getInstBus3EQ();      break;
    case 12: rack = vg.getRustyDrumsBusRack();  eq = vg.getRustyDrumsBusEQ(); break;
    case 13: rack = vg.getPluginsBusRack();     eq = vg.getPluginsBusEQ();    break;
    case 14: rack = vg.getLayersBus2Rack();     eq = vg.getLayersBus2EQ();    break;   // T10
    case 15: rack = vg.getBassBus2Rack();       eq = vg.getBassBus2EQ();      break;
    case 16: rack = vg.getClipsBus2Rack();      eq = vg.getClipsBus2EQ();     break;
    case 17: rack = vg.getPluginsBus2Rack();    eq = vg.getPluginsBus2EQ();   break;
    case 18: rack = vg.getDrumsBus2Rack();      eq = vg.getDrumsBus2EQ();     break;   // QA-SOUNDNESS
    default:
        if (id >= 200 && id < 200 + kMaxLayerPages)
        {
            // Per-page Layer channels (IDs 200-207). Resolve rack + EQ via the
            // InsertNode registry (matches the per-strip mixer architecture).
            // Same pattern as the drum-strip fix (2026-04-18): both getInsertRack
            // and getInsertEQ share the InsertNode so the post-rack EQ audibly
            // processes the same signal the rack does.
            const int idx = id - 200;
            rack = vg.getInsertRack(BaySickGraph::InsertKind::Layer, idx);
            eq   = vg.getInsertEQ  (BaySickGraph::InsertKind::Layer, idx);
            if (rack == nullptr)
                rack = vg.getLayerPageRack(idx);   // legacy fallback
        }
        else if (id >= 300 && id < 300 + kMaxBassPages)
        {
            // Per-page Bass channels (IDs 300-303), same pattern as Layer above.
            const int idx = id - 300;
            rack = vg.getInsertRack(BaySickGraph::InsertKind::Bass, idx);
            eq   = vg.getInsertEQ  (BaySickGraph::InsertKind::Bass, idx);
            if (rack == nullptr)
                rack = vg.getBassPageRack(idx);    // legacy fallback
        }
        else if (id >= 600 && id < 600 + (int) MixerChannelIds::kMaxAuxStrips)
        {
            // Aux strips (600..615 - dropdown-internal range to avoid collision
            // with drum 100-series and audio 400-series). Route through the
            // InsertNode registry so both rack AND EQ bind to the node the
            // audio path actually processes (same asymmetry as drums/layers/bass
            // had before this sweep).
            const int idx = id - 600;
            rack = vg.getInsertRack(BaySickGraph::InsertKind::Aux, idx);
            eq   = vg.getInsertEQ  (BaySickGraph::InsertKind::Aux, idx);
            if (rack == nullptr)
                rack = vg.getAuxRack(idx);         // legacy fallback
        }
        else if (id >= 400 && id < 500)
        {
            // Per-clip audio row channels (IDs 400+row). InsertNode-first.
            const int row = id - 400;
            rack = vg.getInsertRack(BaySickGraph::InsertKind::Audio, row);
            eq   = vg.getInsertEQ  (BaySickGraph::InsertKind::Audio, row);
            if (rack == nullptr)
            {
                // Legacy InstrChannelNode fallback for stray state-restore paths
                rack = vg.getAudioRowRack(row);
                eq   = vg.getAudioRowEQ  (row);
            }
        }
        else if (id >= 100 && id < 200)
        {
            // Drum strips (IDs 100..115) -> per-slot InsertNode racks.
            // Drums were migrated from legacy InstrChannelNode to InsertKind::Drum
            // during 5F-3; the dropdown enumeration (StandaloneEditor::onGetActiveChannels)
            // already populates drums via MixerPage::getDrumStripIndices with ID=100+slot,
            // so we resolve the rack via BaySickGraph's InsertNode registry here.
            // Fallback to legacy InstrChannelRack for any stray state-restore paths
            // (belt-and-suspenders - mirrors the same pattern in onGetActiveChannels).
            const int slot = id - 100;
            rack = vg.getInsertRack(BaySickGraph::InsertKind::Drum, slot);
            eq   = vg.getInsertEQ  (BaySickGraph::InsertKind::Drum, slot);
            if (rack == nullptr)
            {
                // Legacy fallback for pre-5F-3 state
                rack = vg.getInstrChannelRack(id);
                eq   = vg.getInstrChannelEQ(id);
            }
        }
        else if (id >= 700 && id < 700 + (int) MixerChannelIds::kMaxVoxStrips)
        {
            const int idx = id - 700;
            rack = vg.getInsertRack(BaySickGraph::InsertKind::Vox, idx);
            eq   = vg.getInsertEQ  (BaySickGraph::InsertKind::Vox, idx);
        }
        else if (id >= 800 && id < 800 + (int) MixerChannelIds::kMaxInstStrips)
        {
            const int idx = id - 800;
            rack = vg.getInsertRack(BaySickGraph::InsertKind::Inst, idx);
            eq   = vg.getInsertEQ  (BaySickGraph::InsertKind::Inst, idx);
        }
        else if (id >= 900 && id < 900 + (int) MixerChannelIds::kMaxRustyStrips)
        {
            // J-8 stage 2 (2026-05-04): Rusty per-strip racks + EQ.  Same
            // InsertNode registry pattern as Layer/Bass/Drum - racks bind to
            // the audio-graph node so widget edits hit the audible signal.
            const int idx = id - 900;
            rack = vg.getInsertRack(BaySickGraph::InsertKind::Rusty, idx);
            eq   = vg.getInsertEQ  (BaySickGraph::InsertKind::Rusty, idx);
        }
        else if (id >= 1000 && id < 1000 + (int) MixerChannelIds::kMaxPluginStrips)
        {
            // QA-ModelShell TS6 (BLU-447): hosted VST3 instrument strips get a
            // rack + EQ like every other engine-driven insert.
            const int idx = id - 1000;
            rack = vg.getInsertRack(BaySickGraph::InsertKind::Plugin, idx);
            eq   = vg.getInsertEQ  (BaySickGraph::InsertKind::Plugin, idx);
        }
        else if (id >= 1100 && id < 1100 + (int) MixerChannelIds::kMaxDirectStrips)
        {
            // QA-TrueLevel SC-10: Direct to Master strips carry a rack + EQ
            // like any insert.
            const int idx = id - 1100;
            rack = vg.getInsertRack(BaySickGraph::InsertKind::Direct, idx);
            eq   = vg.getInsertEQ  (BaySickGraph::InsertKind::Direct, idx);
        }
        break;
    }
}

// QA-ProjectSave Task 7 step 3 (2026-07-26): DSP-targeting automation for one
// rack slot.  Everything here resolves lazily from (channelId, slotUuid) so no
// UI object is on the automation path.
void EffectsPage::registerSlotAutomation (int slotIndex)
{
    if (mRack == nullptr || mTrackBox == nullptr) return;
    registerSlotAutomationFor (mProcessor, mTrackBox->getSelectedId(),
                               getChannelPrefix(), *mRack, slotIndex);
}

void EffectsPage::registerSlotAutomationFor (BaySickDAWProcessor& proc, int chId,
                                             const juce::String& channelPrefix,
                                             EffectRack& rackRef, int slotIndex)
{
    auto& vg = proc.mVibeGraph;
    const juce::String uuid   = rackRef.getSlotUuid (slotIndex);
    const EffectType   type   = rackRef.getSlotType (slotIndex);
    // Variant is part of the key: one EffectType can build several panels whose
    // knobs share LABELS but not meaning (Modern vs FET attack, Modern vs Opto
    // gain).  Read it from the DSP so no UI is involved.
    const int          variant = EffectParamMap::variantOf (type, rackRef.getSlotEffect (slotIndex));
    const juce::String base   = channelPrefix + "_" + uuid + "_";
    if (uuid.isEmpty()) return;

    // Resolve rack -> slot index by UUID.  Shared by the slot-gain registration
    // below and the per-parameter ones after it.
    auto resolveSlot = [&vg, chId, uuid] (EffectRack*& outRack) -> int
    {
        outRack = EffectsPage::rackForChannelId (vg, chId);
        if (outRack == nullptr) return -1;
        for (int i = 0; i < EffectRack::kNumSlots; ++i)
            if (outRack->getSlotUuid (i) == uuid) return i;
        return -1;
    };

    // ── Slot output volume ───────────────────────────────────────────────────
    // Every panel inherits this knob from EditorPanelBase, and it is the ONE
    // automatable control that is not a DSP parameter: it writes the RACK's
    // per-slot output gain, not anything inside the effect.  EffectParamMap
    // therefore cannot cover it, which is exactly why Jeff found the compressor's
    // own Gain surviving a channel swap while the outbound volume did not --
    // that knob was still on the old panel-targeting applicator, and still died
    // with its panel.  Range mirrors EditorPanelBase's slider (-24..+12 dB).
    {
        const juce::String pid = base + "output_vol";
        constexpr float kLo = -24.0f, kHi = 12.0f;

        if (VKnobAutomation::sOnRegisterApplicator)
            VKnobAutomation::sOnRegisterApplicator (pid,
                [resolveSlot] (float v01)
                {
                    EffectRack* rack = nullptr;
                    const int slot = resolveSlot (rack);
                    if (rack == nullptr || slot < 0) return;
                    rack->setSlotOutputGain (slot,
                        kLo + juce::jlimit (0.0f, 1.0f, v01) * (kHi - kLo));
                });

        if (VKnobAutomation::sOnRegisterReader)
            VKnobAutomation::sOnRegisterReader (pid,
                [resolveSlot]() -> float
                {
                    EffectRack* rack = nullptr;
                    const int slot = resolveSlot (rack);
                    if (rack == nullptr || slot < 0) return 0.5f;
                    return juce::jlimit (0.0f, 1.0f,
                        (rack->getSlotOutputGain (slot) - kLo) / (kHi - kLo));
                });
    }

    // ── QA-ModelShell TS6: hosted plugin parameters ──────────────────────────
    // A plugin has no EffectParamMap table -- its parameters are DISCOVERED
    // from the instance rather than declared by us -- so this is a parallel
    // loop rather than another table.  Lane suffix is "vst_" + the plugin's own
    // stable parameter id; the "vst_" tag keeps it from ever colliding with a
    // table suffix or with "output_vol", and the offline resolver splits on the
    // uuid so an id containing underscores is fine.
    if (type == EffectType::VST3Plugin)
    {
        auto* hosted = dynamic_cast<Hosting::HostedPluginEffect*>(rackRef.getSlotEffect (slotIndex));

        if (hosted != nullptr)
        {
            // Bridged lists arrive async after load, so this registration can
            // run against an EMPTY list -- which left a restored project's
            // plugin lanes silent until something re-ran it (Jeff,
            // 2026-08-02).  Arm the arrival hook: when the list lands, re-run
            // this whole registration, re-resolving the slot by uuid (it may
            // have been reordered in between).  proc outlives the instance --
            // the graph owns the rack owns the effect owns it.
            if (auto* inst = hosted->getHosted())
            {
                auto* procPtr = &proc;
                inst->onParamListArrived = [procPtr, chId, channelPrefix, uuid]
                {
                    auto* rack = EffectsPage::rackForChannelId (procPtr->mVibeGraph, chId);
                    if (rack == nullptr) return;
                    for (int i = 0; i < EffectRack::kNumSlots; ++i)
                        if (rack->getSlotUuid (i) == uuid)
                        {
                            EffectsPage::registerSlotAutomationFor (*procPtr, chId,
                                                                    channelPrefix, *rack, i);
                            return;
                        }
                };
            }

            auto resolveDsp = [&vg, chId, uuid]() -> DSPBase*
            {
                auto* rack = EffectsPage::rackForChannelId (vg, chId);
                if (rack == nullptr) return nullptr;
                for (int i = 0; i < EffectRack::kNumSlots; ++i)
                    if (rack->getSlotUuid (i) == uuid)
                        return rack->getSlotEffect (i);
                return nullptr;
            };

            for (const auto& p : hosted->getAutomatableParams())
            {
                const juce::String pid     = base + "vst_" + p.id;
                const juce::String paramId = p.id;

                if (VKnobAutomation::sOnRegisterApplicator)
                    VKnobAutomation::sOnRegisterApplicator (pid,
                        [resolveDsp, paramId] (float v01)
                        {
                            Hosting::HostedPluginEffect::applyParamNorm (resolveDsp(), paramId, v01);
                        });

                if (VKnobAutomation::sOnRegisterReader)
                    VKnobAutomation::sOnRegisterReader (pid,
                        [resolveDsp, paramId]() -> float
                        {
                            return Hosting::HostedPluginEffect::readParamNorm (resolveDsp(), paramId, 0.5f);
                        });
            }
        }
    }

    for (const auto& def : EffectParamMap::defsFor (type, variant))
    {
        const juce::String pid    = base + def.suffix;
        const juce::String suffix = def.suffix;
        // Lookahead-class knobs move the DSP's reported latency, and the panel
        // lambda that used to own this write also poked EditorPanelBase::
        // onLatencyChanged -> BaySickGraph::updateBusLatencies.  An automation tick
        // has no panel, so the poke rides the applicator instead; without it a
        // lane could leave bus PDC stale.
        const bool pokeLatency = def.affectsLatency;
        auto* host = &proc;

        // Resolve rack -> slot (by uuid) -> DSP.  Any step failing means the
        // target is genuinely gone, and the applicator no-ops rather than
        // pretending to work.
        auto resolveDsp = [&vg, chId, uuid]() -> DSPBase*
        {
            auto* rack = EffectsPage::rackForChannelId (vg, chId);
            if (rack == nullptr) return nullptr;
            for (int i = 0; i < EffectRack::kNumSlots; ++i)
                if (rack->getSlotUuid (i) == uuid)
                    return rack->getSlotEffect (i);
            return nullptr;
        };

        if (VKnobAutomation::sOnRegisterApplicator)
            VKnobAutomation::sOnRegisterApplicator (pid,
                [resolveDsp, type, variant, suffix, pokeLatency, host] (float v01)
                {
                    if (EffectParamMap::applyNorm (type, variant, resolveDsp(), suffix, v01)
                        && pokeLatency)
                        host->setLatencySamples (host->mVibeGraph.updateBusLatencies());
                });   // rack-scoped: resolves through the graph at apply time

        if (VKnobAutomation::sOnRegisterReader)
            VKnobAutomation::sOnRegisterReader (pid,
                [resolveDsp, type, variant, suffix]() -> float
                {
                    return EffectParamMap::readNorm (type, variant, resolveDsp(), suffix, 0.5f);
                });
    }
}

EffectRack* EffectsPage::rackForChannelId (BaySickGraph& vg, int id)
{
    EffectRack* rack = nullptr;
    StripEq*   eq   = nullptr;
    resolveChannelDsp (vg, id, rack, eq);
    return rack;
}

// The dropdown's channel-id vocabulary (the ids lane paramIds embed): 1-18
// buses, 100+ drums, 200+ layers, 300+ basses, 400+ audio inserts, 600+ aux,
// 700+ vox, 800+ inst, 900+ rusty, 1000+ plugin.  ONE copy of it -- two sweeps
// spelling the ranges out separately would silently miss a channel class the
// day one of them gained a strip type.
static void forEachChannelWithRack (BaySickGraph& vg,
                                    const std::function<void(int, EffectRack&)>& fn)
{
    auto visit = [&vg, &fn] (int chId)
    {
        if (auto* rack = EffectsPage::rackForChannelId (vg, chId))
            fn (chId, *rack);
    };

    for (int id = 1; id <= MixerChannelIds::kDrumsBus2; ++id)            visit (id);
    for (int i = 0; i < kMaxDrumPages; ++i)                              visit (100 + i);
    for (int i = 0; i < kMaxLayerPages; ++i)                             visit (200 + i);
    for (int i = 0; i < kMaxBassPages; ++i)                              visit (300 + i);
    for (int i = 0; i < MixerState::kMaxAudioRows; ++i)                  visit (400 + i);
    for (int i = 0; i < (int) MixerChannelIds::kMaxAuxStrips; ++i)       visit (600 + i);
    for (int i = 0; i < (int) MixerChannelIds::kMaxVoxStrips; ++i)       visit (700 + i);
    for (int i = 0; i < (int) MixerChannelIds::kMaxInstStrips; ++i)      visit (800 + i);
    for (int i = 0; i < (int) MixerChannelIds::kMaxRustyStrips; ++i)     visit (900 + i);
    for (int i = 0; i < (int) MixerChannelIds::kMaxPluginStrips; ++i)    visit (1000 + i);
    for (int i = 0; i < (int) MixerChannelIds::kMaxDirectStrips; ++i)    visit (1100 + i);
}

// QA-ModelShell TS1 (wire-at-load): rack automation used to be registered
// only when the Effects page built a slot editor, so lanes went silent until
// the page was VISITED after every project boundary.  This sweep runs after
// applyPendingRackStates repopulates the racks -- a model trigger, no view
// involved.  The page also re-registers a slot whenever its effect identity
// changes (stampAndRegisterSlotEditor); identical rack-scoped entries overwrite
// by key, so the two paths cannot disagree.
void EffectsPage::registerRackAutomationForAllChannels (BaySickDAWProcessor& proc)
{
    forEachChannelWithRack (proc.mVibeGraph, [&proc] (int chId, EffectRack& rack)
    {
        for (int s = 0; s < EffectRack::kNumSlots; ++s)
        {
            if (rack.getSlotType (s) == EffectType::None) continue;
            registerSlotAutomationFor (proc, chId, channelPrefixForId (chId), rack, s);
        }
    });
}

bool EffectsPage::retryDeadPluginSlot (BaySickDAWProcessor& proc, int chId,
                                       EffectRack& rack, int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= EffectRack::kNumSlots)          return false;
    if (rack.getSlotType (slotIndex) != EffectType::VST3Plugin)       return false;

    auto* hosted = dynamic_cast<Hosting::HostedPluginEffect*> (rack.getSlotEffect (slotIndex));
    if (hosted == nullptr) return false;

    auto* dead = hosted->getHosted();

    // The two dead halves are split rather than merged into one early return,
    // because they fail for opposite reasons and only one of them is a decline
    // this code could ever stop making (header carries the full table).  No
    // instance means the restore blob named no plugin, so the slot's identity is
    // gone from the project itself and there is nothing to ask for -- tearing it
    // down for a rebuild would cost the settle and the dirty fire and still land
    // on an empty slot.  An instance that is not alive is the recoverable half.
    if (dead == nullptr) return false;
    if (dead->isAlive()) return false;

    // Copied out BEFORE the rebuild.  The slot carries the whole description
    // rather than an added-list reference, which is what makes this recoverable
    // at all -- an identifier lookup would depend on the very list the user may
    // still not have the plugin on.
    const juce::PluginDescription desc = dead->getDescription();

    // PROBE BEFORE COMMITTING -- everything below is expensive and unconditional
    // once started: an audio-thread rendezvous (settleAudioThread), a full
    // instance teardown and rebuild, an automation re-registration and a
    // project-dirty fire, all paid PER DEAD SLOT on every added-list change.
    // None of it can succeed while the binary the description names is not on
    // disk: both load routes resolve fileOrIdentifier as a path (in-process
    // through the VST3 format, bridged through the helper), so a missing one
    // fails either way and the slot stays "(missing)" exactly as it was.  The
    // absolute-path test comes first because juce::File asserts on a relative
    // one, and a description with no path at all is left to the load to reject.
    if (juce::File::isAbsolutePath (desc.fileOrIdentifier)
        && ! juce::File (desc.fileOrIdentifier).exists())
        return false;

    // The dead instance answers with the bytes it was last restored with, so the
    // user's settings survive the round trip (see HostedPluginInstance's
    // last-known-state member).
    juce::MemoryBlock state;
    hosted->getStateInformation (state);

    // THREAD SAFETY -- the publication and the state push must be ONE step from
    // the audio thread's point of view, and the shield is what makes them one.
    // loadEffect parks the new DSP in `pending` and the very next block promotes
    // it into `active` and renders through it, so a push made after that lands on
    // the object the audio thread is inside -- and that push is not merely a
    // parameter write: HostedPluginInstance::setStateInformation hands the blob to
    // mInner->setStateInformation, which almost no VST3 tolerates concurrently
    // with its own processBlock, and re-instantiates (freeing the inner plugin
    // outright) whenever the blob's bridge mode differs from the mode the fresh
    // instance came up in -- which it does exactly when the user saved this slot
    // with "Run bridged" on.  Shield raised so processBlock bails to silence,
    // settle so the block already past that check has returned, THEN publish: no
    // promotion can happen until the shield drops, by which point the DSP is fully
    // configured.  Nest-aware save/restore, the same idiom
    // EngineRig::teardownEngine uses and correct for the same reason (the shield
    // is only ever raised from the message thread).
    //
    // It also keeps a user-bridged plugin from ever RENDERING inside the host: the
    // rebuilt instance always comes up unbridged, because mBridgePreferred is
    // per-instance and the fresh one has not been told yet, so without the shield
    // it would process in-process for a block or two before the state flipped it
    // into the sandbox.  What the shield does NOT buy is keeping the module out of
    // the process -- loadEffect instantiates before any blob is read, so a 64-bit
    // plugin the user bridged for stability is still LOADED here and a crash in
    // its own load path still lands in this process.  Closing that needs the
    // preference to reach the build, which loadEffect has no channel for.  A
    // 32-bit plugin is unaffected either way: isBridgeForced makes instantiate
    // refuse the in-process fallback outright.
    //
    // Deliberately NOT hoisted into retryDeadPluginSlots: every early return above
    // runs first, so the mute is paid only for a slot that really has a dead
    // plugin to revive.
    const bool shieldWasUp = proc.isProjectLoadInProgress();
    proc.setProjectLoadInProgress (true);
    if (! shieldWasUp)
        proc.settleAudioThread();

    // Same uuid, so automation lanes and any open window on this slot keep
    // resolving.
    rack.loadEffect (slotIndex, EffectType::VST3Plugin,
                     rack.getSlotUuid (slotIndex), &desc);

    if (auto* revived = rack.getSlotEffect (slotIndex))
        if (state.getSize() > 0)
            revived->setStateInformation (state.getData(), (int) state.getSize());

    proc.setProjectLoadInProgress (shieldWasUp);

    // A new instance means a new parameter list and a new bridged-list-arrival
    // hook, both of which the registration owns.
    registerSlotAutomationFor (proc, chId, channelPrefixForId (chId), rack, slotIndex);
    proc.setLatencySamples (proc.mVibeGraph.updateBusLatencies());

    auto* now = dynamic_cast<Hosting::HostedPluginEffect*> (rack.getSlotEffect (slotIndex));
    auto* inst = now != nullptr ? now->getHosted() : nullptr;
    return inst != nullptr && inst->isAlive();
}

void EffectsPage::retryDeadPluginSlots (BaySickDAWProcessor& proc)
{
    forEachChannelWithRack (proc.mVibeGraph, [&proc] (int chId, EffectRack& rack)
    {
        for (int s = 0; s < EffectRack::kNumSlots; ++s)
            retryDeadPluginSlot (proc, chId, rack, s);
    });
}

// QA-ModelShell TS5: the pre-rack EQ resolver, lifted verbatim out of
// onChannelChanged.  It was the one piece of channel resolution that had NOT
// been made static at TS1 -- a second copy of the channel switch living inside
// a view method, which the EQ windows would have had to fork a third time.
StripEq* EffectsPage::preEqForChannelId (BaySickGraph& vg, int id)
{
    switch (id)
    {
        case 1:  return vg.getLayersBusPreEQ();
        case 2:  return vg.getBassBusPreEQ();
        case 3:  return vg.getDrumsBusPreEQ();
        case 4:  return vg.getMasterPreEQ();
        case 5:  return vg.getEffectsBusPreEQ();
        case 6:  return vg.getAudioClipsBusPreEQ();
        case 7:  return vg.getVoxBusPreEQ();
        case 8:  return vg.getInstBusPreEQ();
        case 9:  return vg.getVoxBus2PreEQ();
        case 10: return vg.getInstBus2PreEQ();
        case 11: return vg.getInstBus3PreEQ();
        case 12: return vg.getRustyDrumsBusPreEQ();
        case 13: return vg.getPluginsBusPreEQ();
        case 14: return vg.getLayersBus2PreEQ();    // T10
        case 15: return vg.getBassBus2PreEQ();
        case 16: return vg.getClipsBus2PreEQ();
        case 17: return vg.getPluginsBus2PreEQ();
        case 18: return vg.getDrumsBus2PreEQ();     // QA-SOUNDNESS
        default: break;
    }
    if      (id >= 100 && id < 200)                                              return vg.getInsertPreEQ(BaySickGraph::InsertKind::Drum,  id - 100);
    else if (id >= 200 && id < 200 + kMaxLayerPages)                             return vg.getInsertPreEQ(BaySickGraph::InsertKind::Layer, id - 200);
    else if (id >= 300 && id < 300 + kMaxBassPages)                              return vg.getInsertPreEQ(BaySickGraph::InsertKind::Bass,  id - 300);
    else if (id >= 400 && id < 500)                                              return vg.getInsertPreEQ(BaySickGraph::InsertKind::Audio, id - 400);
    else if (id >= 600 && id < 600 + (int) MixerChannelIds::kMaxAuxStrips)       return vg.getInsertPreEQ(BaySickGraph::InsertKind::Aux,   id - 600);
    else if (id >= 700 && id < 700 + (int) MixerChannelIds::kMaxVoxStrips)       return vg.getInsertPreEQ(BaySickGraph::InsertKind::Vox,   id - 700);
    else if (id >= 800 && id < 800 + (int) MixerChannelIds::kMaxInstStrips)      return vg.getInsertPreEQ(BaySickGraph::InsertKind::Inst,  id - 800);
    else if (id >= 900 && id < 900 + (int) MixerChannelIds::kMaxRustyStrips)     return vg.getInsertPreEQ(BaySickGraph::InsertKind::Rusty, id - 900);
    else if (id >= 1000 && id < 1000 + (int) MixerChannelIds::kMaxPluginStrips)  return vg.getInsertPreEQ(BaySickGraph::InsertKind::Plugin, id - 1000);
    else if (id >= 1100 && id < 1100 + (int) MixerChannelIds::kMaxDirectStrips)  return vg.getInsertPreEQ(BaySickGraph::InsertKind::Direct, id - 1100);
    return nullptr;
}

void EffectsPage::onChannelChanged()
{
    EffectRack* rack = nullptr;
    StripEq*  eq   = nullptr;

    auto& vg  = mProcessor.mVibeGraph;
    const int  id = mTrackBox->getSelectedId();

    resolveChannelDsp (vg, id, rack, eq);

    setRack(rack);

    // The EQs are windows now, and they follow the channel they were OPENED on
    // rather than this page's selection -- so switching channels here does not
    // touch them.  That is the locked behavior: several strips' effects and EQs
    // can be on screen at once.

    if (mFxBypassBtn)
    {
        // 5F-4a: re-attach FX Bypass button + parameter listener to the
        // newly-selected channel. Tear down any prior attachment / listener.
        mFxBypassAtt.reset();
        if (mBypassParamId.isNotEmpty())
            mProcessor.apvts.removeParameterListener(mBypassParamId, this);
        mBypassParamId.clear();

        const juce::String apvtsPrefix = mixerPrefixForChannelId(
            mTrackBox ? mTrackBox->getSelectedId() : 0);
        const bool hasBypassParam = apvtsPrefix.isNotEmpty()
            && mProcessor.apvts.getParameter(apvtsPrefix + "_bypass") != nullptr;

        if (hasBypassParam)
        {
            mBypassParamId = apvtsPrefix + "_bypass";
            mProcessor.apvts.addParameterListener(mBypassParamId, this);

            // Sync the rack to the CURRENT param value once, so the rack's
            // bypass state matches the stored UI state on channel switch.
            if (auto* raw = mProcessor.apvts.getRawParameterValue(mBypassParamId))
                if (rack) rack->setRackBypassed(raw->load() > 0.5f);

            mFxBypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                mProcessor.apvts, mBypassParamId, *mFxBypassBtn);
        }
        else
        {
            mFxBypassBtn->setToggleState(rack ? rack->isRackBypassed() : false,
                                         juce::dontSendNotification);
        }
    }
}

// 5F-4a: APVTS param listener - mirrors _bypass to rack.setRackBypassed().
// Can be called on any thread (APVTS may fire from the audio thread), and the
// audio thread is a second writer of the same flag via BaySickGraph's per-block
// re-sync -- so EffectRack::mRackBypassed is a relaxed std::atomic<bool>, not
// a plain bool.
void EffectsPage::parameterChanged(const juce::String& paramId, float newValue)
{
    if (paramId == mBypassParamId && mRack)
        mRack->setRackBypassed(newValue > 0.5f);
}

void EffectsPage::selectChannelByName(const juce::String& name)
{
    // Try to find the name in the dropdown (handles both bus names and instrument names).
    for (int i = 0; i < mTrackBox->getNumItems(); ++i)
    {
        if (mTrackBox->getItemText(i).equalsIgnoreCase(name))
        {
            mTrackBox->setSelectedId(mTrackBox->getItemId(i), juce::sendNotification);
            return;
        }
    }

    // Fallback: map common aliases used by Mixer strip FX buttons
    int id = 4;   // default: Master
    if      (name.equalsIgnoreCase("Layers Bus") || name.equalsIgnoreCase("Layers"))  id = 1;
    else if (name.equalsIgnoreCase("Bass Bus")   || name.equalsIgnoreCase("Bass"))    id = 2;
    else if (name.equalsIgnoreCase("Drums Bus")  || name.equalsIgnoreCase("Drums"))   id = 3;
    else if (name.equalsIgnoreCase("Master"))                                          id = 4;
    else if (name.equalsIgnoreCase("Effects Bus") || name.equalsIgnoreCase("FX Bus")) id = 5;
    else if (name.equalsIgnoreCase("Audio Clips Bus") || name.equalsIgnoreCase("Clips Bus")) id = 6;
    else
    {
        // "Layer N" and "Bass N" aliases
        for (int i = 0; i < kMaxLayerPages; ++i)
            if (name.equalsIgnoreCase("Layer " + juce::String(i + 1))) { id = 200 + i; break; }
        for (int i = 0; i < kMaxBassPages; ++i)
            if (name.equalsIgnoreCase("Bass " + juce::String(i + 1))) { id = 300 + i; break; }
    }

    mTrackBox->setSelectedId(id, juce::sendNotification);
}

// ── Rack connection ───────────────────────────────────────────────────────────
void EffectsPage::setRack(EffectRack* rack)
{
    mRack = rack;

    if (rack && mFxBypassBtn)
        mFxBypassBtn->setToggleState(rack->isRackBypassed(), juce::dontSendNotification);

    // No editors are built here any more -- a panel exists only while its own
    // window is open.  What the page owes the new rack is its rows and, because
    // registration is keyed to (type, variant) and this rack may never have been
    // visited, its automation.
    refreshAllRows();

    if (rack)
        for (int i = 0; i < EffectRack::kNumSlots; ++i)
            if (rack->getSlotType (i) != EffectType::None)
                registerSlotAutomation (i);
}

// ── Slot interaction ──────────────────────────────────────────────────────────
// D.2 (2026-05-01): full-state snapshot - captures type + bypassed +
// outputGainDb + serialized DSP state + slot UUID per slot.  Replaces the
// old type-only captureSlotTypes / applySlotTypes pair so undo/redo
// preserves knob values and keeps slot UUIDs stable (which keeps automation
// lanes pointed at the same paramId across an undo).
static EffectRackAction::SlotSnapshots captureSlotSnapshots(EffectRack* rack)
{
    EffectRackAction::SlotSnapshots snaps;
    for (int i = 0; i < EffectRack::kNumSlots; ++i)
    {
        auto& s = snaps[(size_t) i];
        s.type         = rack->getSlotType(i);
        s.bypassed     = rack->isSlotBypassed(i);
        s.outputGainDb = rack->getSlotOutputGain(i);
        s.uuid         = rack->getSlotUuid(i);
        // Captured for empty slots too: the pick/mode live on the Slot, not on
        // the effect, and survive a clearSlot.
        s.scPick       = rack->getSlotSidechainPick(i);
        s.basicMode    = rack->getSlotBasicMode(i);
        // Serialize DSP knob state into the snapshot.
        if (auto* eff = rack->getSlotEffect(i))
            eff->getStateInformation (s.dspState);
    }
    return snaps;
}

// Apply a target slot-snapshot.  Slots whose type+UUID match the current
// state are diff-applied (only bypass / output-gain / DSP-state / sidechain
// pick / Basic-mode restored - the existing effect instance is preserved).
// Slots whose type or UUID differ get their effect reloaded with the
// snapshot's UUID so automation lanes survive.  Writes a parallel `outChanged` array indicating which
// slots had their effect instance replaced (so callers can rebuild only the
// affected editor panels - preserves slider SafePointers on untouched ones).
//
// THREAD SAFETY: shielded, and the shield is load-bearing rather than
// defensive.  loadEffect publishes the new DSP into the slot's `pending` with
// swapPending set, and the audio thread promotes and renders it within one
// block - so the setStateInformation below runs against an object audio is
// already inside.  For a VST3 slot that is worse than a data race: no
// PluginDescription is passed here, so the slot is published with mHosted
// null and HostedPluginEffect::setStateInformation then constructs (or on a
// re-apply destroys) the instance through setPlugin, while
// HostedPluginEffect::process reads mHosted as a plain unguarded unique_ptr.
// Reachable by undo/redo of a rack VST3 load during playback.  Bracketing the
// whole loop rather than each slot also makes a multi-slot restore atomic
// with respect to the audio thread, which is what undo of a Move Effect wants.
static void applySlotSnapshots(BaySickDAWProcessor& proc,
                               EffectRack* rack,
                               const EffectRackAction::SlotSnapshots& target,
                               std::array<bool, EffectRack::kNumSlots>& outChanged)
{
    // Filled before every bail: the callers read this array unconditionally and
    // it reaches them default-initialized.
    outChanged.fill(false);

    // A rack lives inside an InsertNode and dies with its tab, so a channel that
    // no longer resolves is the ordinary outcome of undoing past a tab delete --
    // not an invariant break to assert on.
    if (rack == nullptr)
        return;

    // A restored slot that references an external file (a user IR, a VST3 whose
    // DLL is gone) records a miss rather than failing loudly, and undo is a user
    // gesture that owns its own report.  ONE drain for the whole loop, and it
    // outlives the shield restore below because it is declared above it.
    MissingFileReport::ScopedGesture gesture ("undo");

    const bool shieldWasUp = proc.isProjectLoadInProgress();
    proc.setProjectLoadInProgress (true);
    if (! shieldWasUp)
        proc.settleAudioThread();

    for (int i = 0; i < EffectRack::kNumSlots; ++i)
    {
        const auto& tgt        = target[(size_t) i];
        const auto  currType   = rack->getSlotType(i);
        const auto  currUuid   = rack->getSlotUuid(i);

        // Reload effect instance if the type changed OR the UUID changed.
        const bool replaceInstance = (currType != tgt.type) || (currUuid != tgt.uuid);
        if (replaceInstance)
        {
            outChanged[i] = true;
            if (tgt.type == EffectType::None)
                rack->clearSlot(i);
            else
                rack->loadEffect(i, tgt.type, tgt.uuid);
        }

        // Restore DSP knob state, bypass, and output-gain on the (possibly
        // newly-instantiated) slot.
        if (tgt.type != EffectType::None)
        {
            if (auto* eff = rack->getSlotEffect(i))
                if (tgt.dspState.getSize() > 0)
                    eff->setStateInformation (tgt.dspState.getData(),
                                              (int) tgt.dspState.getSize());
            rack->setSlotBypassed   (i, tgt.bypassed);
            rack->setSlotOutputGain (i, tgt.outputGainDb);
        }

        // Outside the type guard: the rack reads scPick every audio block and
        // SlotComponent reads basicMode when it (re)builds the panel, so both
        // must be restored even on a now-empty slot or the next effect loaded
        // there inherits the pick the undo was meant to erase.
        rack->setSlotSidechainPick (i, tgt.scPick);
        rack->setSlotBasicMode     (i, tgt.basicMode);
    }

    proc.setProjectLoadInProgress (shieldWasUp);
}

// The apply body all three rack transactions share, bound to the channel the
// action was RECORDED against.
//
// Binding is the whole point: an EffectRackAction carries slot snapshots and no
// channel identity, while the page's own mRack follows the dropdown -- and moves
// on a plain channel pick, on a mixer FX-button jump, and on any dropdown
// rebuild whose previous selection went away (which falls back to Master).  An
// apply that read mRack therefore wrote one channel's undo into whichever rack
// happened to be showing, replacing that channel's live chain with a foreign
// snapshot.  Resolving by captured id is the same lazy-resolve-by-key rule the
// slot automation applicators already follow, and for the same reason: the rack
// pointer cannot be held across the wait.
EffectRackAction::ApplyFn EffectsPage::makeRackApply (int chId)
{
    // `this` is safe to capture -- the Effects page is a permanent system tab and
    // outlives every transaction.  The RACK is not.
    return [this, chId] (const EffectRackAction::SlotSnapshots& t)
    {
        auto* rack = rackForChannelId (mProcessor.mVibeGraph, chId);
        if (rack == nullptr) return;

        std::array<bool, EffectRack::kNumSlots> changed;
        applySlotSnapshots (mProcessor, rack, t, changed);

        // Re-register only the slots whose DSP was actually replaced.  The first
        // perform() is skipped by EffectRackAction, so this runs on undo/redo
        // only.  Keyed to the captured channel, not the page's selection, for
        // the same reason the rack is.
        for (int i = 0; i < EffectRack::kNumSlots; ++i)
            if (changed[i])
                registerSlotAutomationFor (mProcessor, chId,
                                           channelPrefixForId (chId), *rack, i);

        refreshAllRows();
        if (onRackContentsChanged) onRackContentsChanged (chId);
        mProcessor.setLatencySamples (mProcessor.mVibeGraph.updateBusLatencies());
    };
}

void EffectsPage::onPluginChosen(int slotIndex, const juce::PluginDescription& desc)
{
    onEffectChosen(slotIndex, EffectType::VST3Plugin, &desc);
}

void EffectsPage::onEffectChosen(int slotIndex, EffectType type,
                                 const juce::PluginDescription* pluginDesc)
{
    if (!mRack) return;
    auto before = captureSlotSnapshots(mRack);
    mRack->loadEffect(slotIndex, type, {}, pluginDesc);
    auto after = captureSlotSnapshots(mRack);
    if (mUndoCtx.isValid())
        mUndoCtx.perform(new EffectRackAction("Load Effect", before, after,
                                              makeRackApply (getCurrentChannelId())),
                         "Load Effect");
    registerSlotAutomation(slotIndex);
    refreshAllRows();
    notifyRackContentsChanged();
    mProcessor.setLatencySamples(mProcessor.mVibeGraph.updateBusLatencies());

    // Choosing an effect is a request to work on it, so open its window.
    onSlotOpenRequested (slotIndex);
}

void EffectsPage::onEffectRemoved(int slotIndex)
{
    if (!mRack) return;
    if (mRack->getSlotType (slotIndex) == EffectType::None) return;

    // Jeff spec 2026-07-29: removal prompts.  The row's X is small and sits
    // next to the picker, and a mis-click used to silently drop an effect (and
    // everything below it moved up).
    const juce::String name = SlotComponent::slotDisplayName (mRack, slotIndex);
    juce::NativeMessageBox::showOkCancelBox (
        juce::MessageBoxIconType::QuestionIcon,
        "Remove Effect",
        "Remove " + name + " from this slot?",
        this,
        // SafePointer, not a raw `this`: a modal prompt can outlive its opener
        // (app teardown while it is up), and this codebase already has a family
        // of use-after-frees from callbacks that assumed otherwise.
        juce::ModalCallbackFunction::create (
            [safeThis = juce::Component::SafePointer<EffectsPage> (this), slotIndex] (int result)
        {
            auto* self = safeThis.getComponent();
            if (result == 0 || self == nullptr || self->mRack == nullptr) return;
            self->performSlotRemoval (slotIndex);
        }));
}

void EffectsPage::performSlotRemoval (int slotIndex)
{
    if (mRack == nullptr) return;

    auto before = captureSlotSnapshots(mRack);
    mRack->clearSlot(slotIndex);
    mRack->packSlotsToTop();
    auto after = captureSlotSnapshots(mRack);
    if (mUndoCtx.isValid())
        mUndoCtx.perform(new EffectRackAction("Remove Effect", before, after,
                                              makeRackApply (getCurrentChannelId())),
                         "Remove Effect");

    // Every slot may have shifted (pack-to-top), so re-register all.
    for (int i = 0; i < EffectRack::kNumSlots; ++i)
        if (mRack->getSlotType (i) != EffectType::None)
            registerSlotAutomation(i);
    refreshAllRows();
    notifyRackContentsChanged();
    mProcessor.setLatencySamples(mProcessor.mVibeGraph.updateBusLatencies());
}

void EffectsPage::onMoveRequested(int slotIndex, bool up)
{
    if (!mRack) return;
    auto before = captureSlotSnapshots(mRack);
    if (up) mRack->moveSlotUp  (slotIndex);
    else    mRack->moveSlotDown(slotIndex);
    auto after = captureSlotSnapshots(mRack);
    if (mUndoCtx.isValid())
        mUndoCtx.perform(new EffectRackAction("Move Effect", before, after,
                                              makeRackApply (getCurrentChannelId())),
                         "Move Effect");

    // A move is a swap: both indices now hold a different uuid, so both
    // registrations have to follow.  (Open windows follow their own uuid and
    // re-point themselves.)
    const int other = up ? slotIndex - 1 : slotIndex + 1;
    if (other >= 0 && other < EffectRack::kNumSlots)
        registerSlotAutomation(other);
    registerSlotAutomation(slotIndex);
    refreshAllRows();
    notifyRackContentsChanged();
}

void EffectsPage::onSlotBypassToggled (int slotIndex)
{
    if (mRack == nullptr) return;
    mRack->setSlotBypassed (slotIndex, ! mRack->isSlotBypassed (slotIndex));
    refreshAllRows();
}

void EffectsPage::onSlotOpenRequested (int slotIndex)
{
    if (mRack == nullptr) return;
    if (mRack->getSlotType (slotIndex) == EffectType::None) return;
    if (onOpenSlotPanel) onOpenSlotPanel (getCurrentChannelId(), slotIndex);
}

void EffectsPage::notifyRackContentsChanged()
{
    if (onRackContentsChanged) onRackContentsChanged (getCurrentChannelId());
}

// QA-ModelShell TS5: what rebuildSlotEditor used to do for an inline panel,
// minus the mounting -- panels are mounted by their own windows now.  Both
// halves live here because both have to happen together on EVERY mount path:
// the stamps make the right-click Automate menu name the right lane, and the
// registration is keyed to (type, variant), which a Mode switch changes.
void EffectsPage::stampAndRegisterSlotEditor (BaySickDAWProcessor& proc, int chId,
                                              EffectRack& rack, int slotIndex,
                                              SlotComponent& target)
{
    if (slotIndex < 0 || slotIndex >= EffectRack::kNumSlots) return;

    if (auto* base = dynamic_cast<EditorPanelBase*> (target.getEditor()))
    {
        // C13: keyed by slot UUID, not index, so reorder preserves automation.
        base->setSlotContext (channelPrefixForId (chId), rack.getSlotUuid (slotIndex));

        // Latency-changing panel controls (lookahead) poke a bus-PDC refresh.
        base->onLatencyChanged = [&proc]
        {
            proc.setLatencySamples (proc.mVibeGraph.updateBusLatencies());
        };
    }

    // QA-ProjectSave Task 7 step 3 (2026-07-26): DSP-targeting applicators for
    // this slot's mapped parameters.  These deliberately do NOT capture the
    // panel or its knobs -- they capture the channel id + slot UUID and resolve
    // rack -> slot -> DSP at apply time, so a lane keeps working after the panel
    // is gone (which, with per-effect windows, is most of the time).  Slot
    // lookup is by UUID, never by index, so reordering does not repoint a lane
    // at a different effect (REAPER's positional-addressing failure mode).
    registerSlotAutomationFor (proc, chId, channelPrefixForId (chId), rack, slotIndex);
}

juce::String EffectsPage::getChannelPrefix() const
{
    return channelPrefixForId (mTrackBox ? mTrackBox->getSelectedId() : 0);
}

// Batch E #8 (2026-05-01): added missing channel categories that
// previously fell through to "instr_<id>".  Audio Clips Bus, Vox/Inst
// buses (incl. spawnable extras), Drum / Audio / Vox / Inst inserts
// all now have proper prefixes for slot-context tagging.  Insert ranges
// sourced from MixerChannelIds + BaySickConstants so size bumps stay
// in sync (kMaxInstStrips bumped 6 -> 10 -> 20 over G-4/G-6).
// QA-ModelShell TS1: extracted static so the wire-at-load sweep can derive
// prefixes without the dropdown.
juce::String EffectsPage::channelPrefixForId (int id)
{
    switch (id)
    {
        case 1:  return "layers_bus";
        case 2:  return "bass_bus";
        case 3:  return "drums_bus";
        case 4:  return "master";
        case 5:  return "fx_bus";
        case 6:  return "clips_bus";
        case 7:  return "vox_bus";
        case 8:  return "inst_bus";
        case 9:  return "vox_bus2";
        case 10: return "inst_bus2";
        case 11: return "inst_bus3";
        case 12: return "rusty_bus";
        case 13: return "plugin_bus";   // QA-ModelShell TS6
        case 14: return "layers_bus2";  // QA-Layout T10
        case 15: return "bass_bus2";
        case 16: return "clips_bus2";
        case 17: return "plugin_bus2";
        case 18: return "drums_bus2";   // QA-SOUNDNESS
        default: break;
    }
    if (id >= 100 && id < 200)
        return "drum_" + juce::String(id - 100);
    // Spans come from the constants, not from a count written here: these
    // annotations have gone stale behind a capacity bump twice, and the inverse
    // map in StandaloneEditor::resolveAutomationDisplayName iterates the SAME
    // constants, so a reader who trusts a hardcoded number tightens that loop
    // and un-names every lane above it.
    if (id >= 200 && id < 200 + kMaxLayerPages)            // one-based: 200 -> layer_1
        return "layer_" + juce::String(id - 199);
    if (id >= 300 && id < 300 + kMaxBassPages)             // one-based: 300 -> bass_1
        return "bass_" + juce::String(id - 299);
    if (id >= 400 && id < 400 + MixerState::kMaxAudioRows) // zero-based: 400 -> audio_0
        return "audio_" + juce::String(id - 400);
    // Aux/Vox/Inst/Rusty ranges mirror the J-6 dropdown numbering (Aux 600+ /
    // Vox 700+ / Inst 800+ / Rusty 900+).  These prefixes key the rack-slot
    // automation paramIds that lanes persist, so this table must track the
    // dropdown builder (pre-fix it still held the pre-J-6 600=Vox/700=Inst
    // layout: aux/vox rack pids were stamped with wrong-kind prefixes and
    // inst/rusty dropped to the "fx" fallback).
    if (id >= 600 && id < 600 + (int) MixerChannelIds::kMaxAuxStrips)
        return "aux_" + juce::String(id - 600);
    if (id >= 700 && id < 700 + (int) MixerChannelIds::kMaxVoxStrips)
        return "vox_" + juce::String(id - 700);
    if (id >= 800 && id < 800 + (int) MixerChannelIds::kMaxInstStrips)
        return "inst_" + juce::String(id - 800);
    if (id >= 900 && id < 900 + (int) MixerChannelIds::kMaxRustyStrips)
        return "rusty_" + juce::String(id - 900);
    if (id >= 1000 && id < 1000 + (int) MixerChannelIds::kMaxPluginStrips)
        return "plugin_" + juce::String(id - 1000);
    if (id >= 1100 && id < 1100 + (int) MixerChannelIds::kMaxDirectStrips)
        return "direct_" + juce::String(id - 1100);
    return "fx";
}

// 5F-4a: maps effects-page channel id → mixer-strip APVTS prefix.
// Returns empty string if the channel has no corresponding mixer strip.
juce::String EffectsPage::mixerPrefixForChannelId(int id)
{
    if (id <= 0) return {};

    switch (id)
    {
        case 1:  return "mixer_layers";
        case 2:  return "mixer_bass";
        case 3:  return "mixer_drums";
        case 4:  return "mixer_master";
        case 5:  return "mixer_fx";
        case 6:  return "mixer_clipsbus";
        case 7:  return "mixer_voxbus";       // R3.5
        case 8:  return "mixer_instbus";      // R3.5
        case 9:  return "mixer_voxbus2";      // G-6
        case 10: return "mixer_instbus2";     // G-6
        case 11: return "mixer_instbus3";     // G-6
        case 12: return "mixer_rustybus";     // J-6
        case 13: return "mixer_pluginbus";    // QA-ModelShell TS6
        case 14: return "mixer_layersbus2";   // QA-Layout T10
        case 15: return "mixer_bassbus2";
        case 16: return "mixer_clipsbus2";
        case 17: return "mixer_pluginbus2";
        case 18: return "mixer_drumsbus2";    // QA-SOUNDNESS
        default: break;
    }

    // Drum inserts - legacy InstrChannelNode IDs (100-199). Map to mixer_drum_N.
    if (id >= 100 && id < 200)
        return "mixer_drum_" + juce::String(id - 100);
    if (id >= 200 && id < 200 + kMaxLayerPages)
        return "mixer_layer_" + juce::String(id - 200);
    if (id >= 300 && id < 300 + kMaxBassPages)
        return "mixer_bass_" + juce::String(id - 300);
    // Audio inserts: 400+row.
    if (id >= 400 && id < 500)
        return "mixer_audio_" + juce::String(id - 400);
    // Aux strips: dropdown 600..617 (re-mapped from MixerChannelIds kAuxBase 100+).
    if (id >= 600 && id < 600 + (int) MixerChannelIds::kMaxAuxStrips)
        return "mixer_aux_" + juce::String(id - 600);
    // J-6 (2026-05-03): Vox/Inst/Rusty insert dropdown ranges.  Disambiguated
    // from each other AND from Aux by giving each kind its own dropdown range.
    if (id >= 700 && id < 700 + (int) MixerChannelIds::kMaxVoxStrips)
        return "mixer_vox_" + juce::String(id - 700);
    if (id >= 800 && id < 800 + (int) MixerChannelIds::kMaxInstStrips)
        return "mixer_inst_" + juce::String(id - 800);
    if (id >= 900 && id < 900 + (int) MixerChannelIds::kMaxRustyStrips)
        return "mixer_rusty_" + juce::String(id - 900);
    if (id >= 1000 && id < 1000 + (int) MixerChannelIds::kMaxPluginStrips)
        return "mixer_plugin_" + juce::String(id - 1000);
    if (id >= 1100 && id < 1100 + (int) MixerChannelIds::kMaxDirectStrips)
        return "mixer_direct_" + juce::String(id - 1100);

    return {};
}

void EffectsPage::refreshAllRows()
{
    for (int i = 0; i < EffectRack::kNumSlots; ++i)
    {
        auto& row = mRows[(size_t) i];
        if (! row) continue;

        if (mRack == nullptr)
        {
            row->setState (false, false, {});
            continue;
        }

        const auto type = mRack->getSlotType (i);
        const bool loaded = (type != EffectType::None);
        row->setState (loaded,
                       loaded && mRack->isSlotBypassed (i),
                       loaded ? SlotComponent::slotDisplayName (mRack, i) : juce::String());
    }
}

int EffectsPage::getCurrentChannelId() const
{
    return mTrackBox != nullptr ? mTrackBox->getSelectedId() : 0;
}

juce::String EffectsPage::getChannelDisplayName (int channelId) const
{
    auto it = mIdToDisplayName.find (channelId);
    return it != mIdToDisplayName.end() ? it->second : juce::String();
}

// ── Keyboard ──────────────────────────────────────────────────────────────────
// 2026-04-26 (B-5): keyPressed + visibilityChanged removed.  Ctrl+Z / Ctrl+Alt+Z
// migrated to global BSCommands; the manager fires from any focus location.

// ── ChangeListener ────────────────────────────────────────────────────────────
void EffectsPage::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    // Two broadcasters land here.  The plugin manager's is the edge that lets a
    // slot whose plugin went missing try again; the track selection's is the
    // repaint it has always been.
    if (source != nullptr && source == Hosting::PluginManager::getInstance())
    {
        retryDeadPluginSlots (mProcessor);
        refreshAllRows();
        return;
    }

    repaint();
}

// ── Paint ─────────────────────────────────────────────────────────────────────
void EffectsPage::paint(juce::Graphics& g)
{
    g.fillAll(VC::Bg);
}

// ── Layout ────────────────────────────────────────────────────────────────────
void EffectsPage::resized()
{
    auto b = getLocalBounds().reduced (4);

    // Channel picker + rack-wide bypass.
    {
        auto top = b.removeFromTop (kRowH);
        if (mTrackLabel) mTrackLabel->setBounds (top.removeFromLeft (54));
        if (mFxBypassBtn) mFxBypassBtn->setBounds (top.removeFromRight (86).reduced (0, 1));
        top.removeFromRight (4);
        if (mTrackBox) mTrackBox->setBounds (top.reduced (0, 1));
    }

    b.removeFromTop (4);

    // The two EQ entries, side by side.
    {
        auto eqRow = b.removeFromTop (kRowH);
        const int half = eqRow.getWidth() / 2;
        if (mPreEqBtn)  mPreEqBtn ->setBounds (eqRow.removeFromLeft (half).reduced (1));
        if (mPostEqBtn) mPostEqBtn->setBounds (eqRow.reduced (1));
    }

    b.removeFromTop (6);

    // Six rows, fixed height: this window is an index, so the rows do not grow
    // to fill it -- extra height is empty space rather than six fat rows.
    for (auto& row : mRows)
    {
        if (! row) continue;
        row->setBounds (b.removeFromTop (kRowH));
        b.removeFromTop (2);
    }
}

// ── Title-bar menu ────────────────────────────────────────────────────────────
void EffectsPage::buildTitleMenu (juce::PopupMenu& m)
{
    const int chId = getCurrentChannelId();

    m.addItem ("Save FX Rack Preset...", [this] { saveRackPreset(); });

    juce::PopupMenu loadMenu;
    loadRackPresetMenu (loadMenu);
    m.addSubMenu ("Load FX Rack Preset", loadMenu, loadMenu.getNumItems() > 0);

    m.addSeparator();
    m.addItem ("Open Presets Folder", [] {
        auto dir = FxRackPresetIO::myPresetsDir();
        dir.createDirectory();
        dir.startAsProcess();
    });

    // VU calibration -- app-wide, and this was the old page's "Meters" button.
    // It moves into the menu rather than costing a button on a small window.
    m.addSeparator();
    VUMeter::addCalibrationSubMenu (m);

    // The one VU in the app: master output, its own window (Jeff, 2026-08-11).
    m.addItem ("VU Meter", [this] { if (onOpenVuMeter) onOpenVuMeter(); });

    juce::ignoreUnused (chId);
}

void EffectsPage::saveRackPreset()
{
    const int chId = getCurrentChannelId();
    if (rackForChannelId (mProcessor.mVibeGraph, chId) == nullptr) return;

    auto* aw = new juce::AlertWindow ("Save FX Rack Preset",
                                      "Saves all six slots and both EQs for this channel.",
                                      juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", getChannelDisplayName (chId) + " Rack", "Preset name:");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis = juce::Component::SafePointer<EffectsPage> (this), aw, chId] (int r)
    {
        std::unique_ptr<juce::AlertWindow> owned (aw);
        auto* self = safeThis.getComponent();
        if (r != 1 || self == nullptr) return;

        juce::String err;
        if (! FxRackPresetIO::save (self->mProcessor, chId, aw->getTextEditorContents ("name"), err))
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                         "Save FX Rack Preset", err);
    }), false);
}

void EffectsPage::loadRackPresetMenu (juce::PopupMenu& into)
{
    const int chId = getCurrentChannelId();
    for (const auto& f : FxRackPresetIO::enumeratePresets())
    {
        into.addItem (f.getFileNameWithoutExtension(), [this, f, chId]
        {
            // Effects that reference an external file (a NAM capture, a user IR)
            // record a miss rather than failing loudly, so this load owns the
            // report -- an undrained entry otherwise surfaces later attached to
            // an unrelated load.
            MissingFileReport::ScopedGesture gesture ("preset");

            juce::String err;
            if (! FxRackPresetIO::load (mProcessor, chId, f, err))
            {
                juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                             "Load FX Rack Preset", err);
                return;
            }
            // The preset carries its own slot uuids, so every lane on this
            // channel now points at effects that no longer exist -- re-register
            // against what actually landed, and let the open windows notice
            // their target is gone.
            if (auto* rack = rackForChannelId (mProcessor.mVibeGraph, chId))
                for (int i = 0; i < EffectRack::kNumSlots; ++i)
                    if (rack->getSlotType (i) != EffectType::None)
                        registerSlotAutomationFor (mProcessor, chId, channelPrefixForId (chId), *rack, i);

            refreshAllRows();
            notifyRackContentsChanged();
            mProcessor.setLatencySamples (mProcessor.mVibeGraph.updateBusLatencies());
        });
    }
}

// ── Timer ─────────────────────────────────────────────────────────────────────
void EffectsPage::timerCallback()
{
    // 5F-4a: Button state is driven by ButtonAttachment -> APVTS.  The old
    // force-sync from rack.isRackBypassed() fought with the attachment and
    // caused the click-flashes-and-reverts bug.
    //
    // The rows, though, have to follow changes this page did not make: undo /
    // redo, a project load, a preset drop, or a bypass flipped from an effect's
    // own window.  setState is a no-op when nothing moved, so this costs a
    // comparison per row.
    refreshAllRows();
}
