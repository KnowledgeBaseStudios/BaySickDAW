#include "EffectsPage.h"
#include "EffectEditorPanels.h"
#include "../PluginProcessor.h"

// ── Channel dropdown items (order matches onChannelChanged switch) ─────────────
// ID 1 = Layers Bus, 2 = Bass Bus, 3 = Drums Bus, 4 = Master, 5 = Effects Bus

// ── Ctor ──────────────────────────────────────────────────────────────────────
EffectsPage::EffectsPage(TrackSelectionManager& tsm, VibeSynthProcessor& processor)
    : mTSM(tsm), mProcessor(processor)
{
    mTSM.addChangeListener(this);

    // ── Top bar ───────────────────────────────────────────────────────────────
    mTrackLabel = std::make_unique<juce::Label>();
    mTrackLabel->setText("Channel:", juce::dontSendNotification);
    mTrackLabel->setFont(juce::Font(12.0f));
    mTrackLabel->setColour(juce::Label::textColourId, VC::TextDim);
    addAndMakeVisible(*mTrackLabel);

    mTrackBox = std::make_unique<juce::ComboBox>();
    mTrackBox->setLookAndFeel(&VibeLAF::get());
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

    mMetersBtn = std::make_unique<juce::TextButton>("Meters v");
    mMetersBtn->setLookAndFeel(&VibeLAF::get());
    mMetersBtn->setTooltip("VU meter calibration and display options");
    mMetersBtn->onClick = [this] { showMetersMenu(); };
    addAndMakeVisible(*mMetersBtn);

    buildRackTab();
    buildEQTab();
    buildPreEQTab();   // §P4.3 (B6.2)

    // 2026-04-26: default to Rack explicitly via TabKind - switchTab(int 0)
    // historically resolved to PreEQ for non-player channels because of the
    // 3-tab layout, leading to "ribbon says Rack but Pre EQ shows" mismatches
    // on first open of the page.
    switchTab(TabKind::Rack);
    rebuildChannelDropdown();  // build channel list + sets initial rack/EQ (all components ready now)

    startTimerHz(30);
}

EffectsPage::~EffectsPage()
{
    stopTimer();
    // Defensive: visibilityChanged already removes the listener when the page
    // 2026-04-26 (B-5): no more per-page KeyListener on the top-level - undo
    // / redo route through the global BSCommands manager.
    if (mBypassParamId.isNotEmpty())
        mProcessor.apvts.removeParameterListener(mBypassParamId, this);
    mProcessor.mVibeGraph.onInstrChannelListChanged = nullptr;   // clear before destruction
    mTSM.removeChangeListener(this);
    mTrackBox->setLookAndFeel(nullptr);
    // MixerLedButton doesn't use LAF, nothing to clean up here
    if (mMetersBtn) mMetersBtn->setLookAndFeel(nullptr);
    for (auto& s : mSlots)
        if (s) s->setRack(nullptr);
}

// ── Tab builders ──────────────────────────────────────────────────────────────
void EffectsPage::buildRackTab()
{
    mRackTab = std::make_unique<juce::Component>();

    for (int i = 0; i < 6; ++i)
    {
        mSlots[i] = std::make_unique<SlotComponent>(i);
        mSlots[i]->onEffectChosen  = [this](int idx, EffectType t) { onEffectChosen(idx, t); };
        mSlots[i]->onEffectRemoved = [this](int idx) { onEffectRemoved(idx); };
        mSlots[i]->onMoveRequested = [this](int idx, bool up) { onMoveRequested(idx, up); };
        // QA-EffectsReview Task 1: persist Basic/Advanced choice with the project
        // (setSlotBasicMode fires onSlotsChanged -> markDirty; idempotent if the
        // SlotComponent already wrote it via toggleBasicMode).
        mSlots[i]->onBasicModeChanged = [this](int idx, bool basic) {
            if (mRack) mRack->setSlotBasicMode(idx, basic);
        };
        mRackTab->addAndMakeVisible(*mSlots[i]);
    }

    addAndMakeVisible(*mRackTab);
}

void EffectsPage::buildEQTab()
{
    mEQTab = std::make_unique<juce::Component>();

    // Prepare the owned M/S DSP with a default sample rate
    // (will snap to real rate if EffectsPage ever receives setSampleRate - Phase 2)
    mEffectsEQDsp.prepare(44100.0, 512);

    mEQDisplay = std::make_unique<ParametricEQDisplay>();
    // Bind to owned DSP so MID/SIDE bands store separate state (no APVTS yet - Phase 2)
    mEQDisplay->bindMsDSP(&mEffectsEQDsp);
    // 12f: refresh host PDC after the user toggles anti-cramping in the popup.
    mEQDisplay->onLatencyChanged = [this]
    {
        mProcessor.setLatencySamples(mProcessor.mVibeGraph.updateBusLatencies());
    };
    mEQTab->addAndMakeVisible(*mEQDisplay);
    // Keep internal pill hidden - MID/SIDE are external buttons in the header row
    mEQDisplay->showMidSideToggle(false);

    addAndMakeVisible(*mEQTab);
}

// §P4.3 (B6.2) Pre EQ tab - mirror of buildEQTab.  Visible only on Aux/Audio/
// Bus channels (player-channel pre-EQ lives on the player page).
void EffectsPage::buildPreEQTab()
{
    mPreEQTab = std::make_unique<juce::Component>();
    mPreEffectsEQDsp.prepare(44100.0, 512);

    mPreEQDisplay = std::make_unique<ParametricEQDisplay>();
    mPreEQDisplay->bindMsDSP(&mPreEffectsEQDsp);   // display-only fallback until channel selected
    mPreEQDisplay->onLatencyChanged = [this]
    {
        mProcessor.setLatencySamples(mProcessor.mVibeGraph.updateBusLatencies());
    };
    mPreEQDisplay->showMidSideToggle(false);
    mPreEQTab->addAndMakeVisible(*mPreEQDisplay);

    addAndMakeVisible(*mPreEQTab);
    mPreEQTab->setVisible(false);   // hidden until tab is selected
}

// §P4.3 (B6.2) - Layer / Bass / Drum-slot channels have their pre-EQ on the
// player page; the mixer Effects page hides the Pre EQ tab for those.
// J-6 (2026-05-03): EQ unification - every channel now exposes Pre + Rack +
// Post on the Effects page (was: only buses + non-player inserts had a Pre
// tab; player inserts had their pre-EQ as a sub-tab on the player page).
// Returning false unconditionally makes the Pre tab visible for every
// channel and the EQ sub-tab on player pages is removed entirely.  Function
// kept for now (not deleted) so any caller that asks "is there a page
// pre-EQ?" gets the consistent "no" answer regardless of channel type.
bool EffectsPage::currentChannelHasPagePreEQ() const
{
    return false;
}

EffectsPage::TabKind EffectsPage::tabKindForVisibleIndex (int visibleIndex) const
{
    // 2-tab layout (player channels): [Rack | Post EQ8 M/S] = idx 0,1
    // 3-tab layout (Aux/Audio/Bus):   [Pre  | Rack | Post]  = idx 0,1,2
    if (currentChannelHasPagePreEQ())
    {
        return visibleIndex == 0 ? TabKind::Rack : TabKind::PostEQ;
    }
    if (visibleIndex == 0) return TabKind::PreEQ;
    if (visibleIndex == 1) return TabKind::Rack;
    return TabKind::PostEQ;
}

int EffectsPage::visibleIndexForTabKind (TabKind kind) const
{
    if (currentChannelHasPagePreEQ())
    {
        // 2-tab layout - PreEQ has no slot, fall back to Rack.
        return (kind == TabKind::PostEQ) ? 1 : 0;
    }
    if (kind == TabKind::PreEQ)  return 0;
    if (kind == TabKind::Rack)   return 1;
    return 2;   // PostEQ
}

// ── Channel list rebuild ──────────────────────────────────────────────────────
void EffectsPage::rebuildChannelDropdown()
{
    const int prevId = mTrackBox->getSelectedId();
    mTrackBox->clear(juce::dontSendNotification);
    mIdToApvtsPrefix.clear();

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
            // Bus IDs in the dropdown match MixerChannelIds 1-12 directly.
            if (dropdownId >= 1 && dropdownId <= 12) return dropdownId;
            if (dropdownId >= 100 && dropdownId < 200) return kDrumBase + (dropdownId - 100);
            if (dropdownId >= 200 && dropdownId < 216) return kLayerBase + (dropdownId - 200);
            if (dropdownId >= 300 && dropdownId < 316) return kBassBase  + (dropdownId - 300);
            if (dropdownId >= 400 && dropdownId < 450) return kAudioBase + (dropdownId - 400);
            if (dropdownId >= 600 && dropdownId < 600 + (int) kMaxAuxStrips)   return kAuxBase   + (dropdownId - 600);
            if (dropdownId >= 700 && dropdownId < 700 + (int) kMaxVoxStrips)   return kVoxBase   + (dropdownId - 700);
            if (dropdownId >= 800 && dropdownId < 800 + (int) kMaxInstStrips)  return kInstBase  + (dropdownId - 800);
            if (dropdownId >= 900 && dropdownId < 900 + (int) kMaxRustyStrips) return kRustyBase + (dropdownId - 900);
            return dropdownId;
        };

        // Separate buses from inserts - buses are group anchors, inserts go in buckets.
        // J-6 (2026-05-03): bus id range expanded 1-6 → 1-12 to include
        // VoxBus/InstBus/VoxBus2/InstBus2/InstBus3/RustyDrumsBus.
        std::vector<Item> busItems;
        std::vector<Item> insertItems;
        for (auto& [id, name] : channels)
        {
            juce::String prefix = getMixerApvtsPrefixForChannel(id);
            Item it { id, name, prefix };
            if (id >= 1 && id <= 12) busItems.push_back(it);
            else                      insertItems.push_back(it);
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
            if (dst >= kAuxBase && dst < kAuxBase + 16)
                for (auto& m : members) addItemWithPrefix(m);

        // ── Clips Bus ─────────────────────────────────────────────────────
        addBusAndMembers(6, kClipsBus, "CLIPS BUS", juce::Colour(0xffd4a017));

        // ── Layers Bus ────────────────────────────────────────────────────
        addBusAndMembers(1, kLayersBus, "LAYERS BUS", VC::LayerCol[0]);

        // ── Bass Bus ──────────────────────────────────────────────────────
        addBusAndMembers(2, kBassBus, "BASS BUS", VC::BassCol[0]);

        // ── Drums Bus ─────────────────────────────────────────────────────
        addBusAndMembers(3, kDrumsBus, "DRUMS BUS", VC::DrumsCol);

        // J-6 (2026-05-03): EQ unification + missing bus groups.
        // ── RustyDrums Bus (J-5) ─────────────────────────────────────────
        addBusAndMembers(12, kRustyDrumsBus, "RUSTYDRUMS BUS", VC::DrumsCol);

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
void EffectsPage::onChannelChanged()
{
    // 2026-04-26: per-channel sub-tab persistence - save the previous channel's
    // current TabKind, then restore the new channel's last-used TabKind (or
    // default to Rack on first visit).  Done here because onChannelChanged is
    // the single funnel for any channel selection change (dropdown click,
    // selectChannelByName/Prefix, etc.).
    const int newChanId = mTrackBox ? mTrackBox->getSelectedId() : 0;
    if (mPrevChannelId != 0 && mPrevChannelId != newChanId)
        mLastTabPerChannel[mPrevChannelId] = mCurrentTabKind;
    mPrevChannelId = newChanId;

    EffectRack* rack = nullptr;
    EQ8MsDSP*  eq   = nullptr;

    auto& vg  = mProcessor.mVibeGraph;
    const int  id = mTrackBox->getSelectedId();

    // IDs 1-5: fixed bus channels
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
    default:
        if (id >= 200 && id < 200 + kMaxLayerPages)
        {
            // Per-page Layer channels (IDs 200-207). Resolve rack + EQ via the
            // InsertNode registry (matches the per-strip mixer architecture).
            // Same pattern as the drum-strip fix (2026-04-18): both getInsertRack
            // and getInsertEQ share the InsertNode so the post-rack EQ audibly
            // processes the same signal the rack does.
            const int idx = id - 200;
            rack = vg.getInsertRack(VibeGraph::InsertKind::Layer, idx);
            eq   = vg.getInsertEQ  (VibeGraph::InsertKind::Layer, idx);
            if (rack == nullptr)
                rack = vg.getLayerPageRack(idx);   // legacy fallback
        }
        else if (id >= 300 && id < 300 + kMaxBassPages)
        {
            // Per-page Bass channels (IDs 300-303), same pattern as Layer above.
            const int idx = id - 300;
            rack = vg.getInsertRack(VibeGraph::InsertKind::Bass, idx);
            eq   = vg.getInsertEQ  (VibeGraph::InsertKind::Bass, idx);
            if (rack == nullptr)
                rack = vg.getBassPageRack(idx);    // legacy fallback
        }
        else if (id >= 600 && id < 600 + 16)
        {
            // Aux strips (600..615 - dropdown-internal range to avoid collision
            // with drum 100-series and audio 400-series). Route through the
            // InsertNode registry so both rack AND EQ bind to the node the
            // audio path actually processes (same asymmetry as drums/layers/bass
            // had before this sweep).
            const int idx = id - 600;
            rack = vg.getInsertRack(VibeGraph::InsertKind::Aux, idx);
            eq   = vg.getInsertEQ  (VibeGraph::InsertKind::Aux, idx);
            if (rack == nullptr)
                rack = vg.getAuxRack(idx);         // legacy fallback
        }
        else if (id >= 400 && id < 500)
        {
            // Per-clip audio row channels (IDs 400+row). InsertNode-first.
            const int row = id - 400;
            rack = vg.getInsertRack(VibeGraph::InsertKind::Audio, row);
            eq   = vg.getInsertEQ  (VibeGraph::InsertKind::Audio, row);
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
            // so we resolve the rack via VibeGraph's InsertNode registry here.
            // Fallback to legacy InstrChannelRack for any stray state-restore paths
            // (belt-and-suspenders - mirrors the same pattern in onGetActiveChannels).
            const int slot = id - 100;
            rack = vg.getInsertRack(VibeGraph::InsertKind::Drum, slot);
            eq   = vg.getInsertEQ  (VibeGraph::InsertKind::Drum, slot);
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
            rack = vg.getInsertRack(VibeGraph::InsertKind::Vox, idx);
            eq   = vg.getInsertEQ  (VibeGraph::InsertKind::Vox, idx);
        }
        else if (id >= 800 && id < 800 + (int) MixerChannelIds::kMaxInstStrips)
        {
            const int idx = id - 800;
            rack = vg.getInsertRack(VibeGraph::InsertKind::Inst, idx);
            eq   = vg.getInsertEQ  (VibeGraph::InsertKind::Inst, idx);
        }
        else if (id >= 900 && id < 900 + (int) MixerChannelIds::kMaxRustyStrips)
        {
            // J-8 stage 2 (2026-05-04): Rusty per-strip racks + EQ.  Same
            // InsertNode registry pattern as Layer/Bass/Drum - racks bind to
            // the audio-graph node so widget edits hit the audible signal.
            const int idx = id - 900;
            rack = vg.getInsertRack(VibeGraph::InsertKind::Rusty, idx);
            eq   = vg.getInsertEQ  (VibeGraph::InsertKind::Rusty, idx);
        }
        break;
    }

    setRack(rack);

    // Session B: bind EQ display to this channel's own post-rack EQ using the
    // full bindMsDSP overload with APVTS prefix so (a) widget drags propagate to
    // APVTS and become audible, (b) band params are automatable via right-click
    // "Automate: ..." menu. Fall back to stub only if the graph isn't built yet.
    // C.4 Phase 1 (2026-04-30): also push the strip context (mixer prefix +
    // source-name resolver) so the per-band SC dropdown in DynamicParamsPopout
    // can enumerate routed lines and label them.
    auto resolveSrcName = [](int srcChId) -> juce::String
    { return MixerChannelIds::friendlyName(srcChId); };

    if (mEQDisplay)
    {
        const juce::String chanPrefix = getMixerApvtsPrefixForChannel(id);
        if (eq && chanPrefix.isNotEmpty())
        {
            mEQDisplay->bindMsDSP(eq, &mProcessor.apvts,
                                  chanPrefix + "_mid_eq",
                                  chanPrefix + "_side_eq");
        }
        else
        {
            mEQDisplay->bindMsDSP(eq ? eq : &mEffectsEQDsp);
        }
        mEQDisplay->setStripContext(chanPrefix, resolveSrcName);
    }

    // §P4.3 (B6.2): bind the Pre EQ display to this channel's pre-rack EQ
    // using the `_preeq_*` APVTS prefix.  Pre-EQ DSP is on the same node as
    // the post-rack EQ - fetched via getInsertPreEQ / getXxxBusPreEQ.  Note:
    // for player channels (Layer/Bass/Drum-slot) this STILL binds correctly
    // even though the Pre tab itself is hidden - keeps state consistent if the
    // user later switches to a non-player channel.
    if (mPreEQDisplay)
    {
        EQ8MsDSP* preEq = nullptr;
        switch (id)
        {
            case 1:  preEq = vg.getLayersBusPreEQ();     break;
            case 2:  preEq = vg.getBassBusPreEQ();       break;
            case 3:  preEq = vg.getDrumsBusPreEQ();      break;
            case 4:  preEq = vg.getMasterPreEQ();        break;
            case 5:  preEq = vg.getEffectsBusPreEQ();    break;
            case 6:  preEq = vg.getAudioClipsBusPreEQ(); break;
            case 7:  preEq = vg.getVoxBusPreEQ();        break;
            case 8:  preEq = vg.getInstBusPreEQ();       break;
            case 9:  preEq = vg.getVoxBus2PreEQ();       break;
            case 10: preEq = vg.getInstBus2PreEQ();      break;
            case 11: preEq = vg.getInstBus3PreEQ();      break;
            case 12: preEq = vg.getRustyDrumsBusPreEQ(); break;
            default:
                if      (id >= 100 && id < 200)                                    preEq = vg.getInsertPreEQ(VibeGraph::InsertKind::Drum,  id - 100);
                else if (id >= 200 && id < 200 + kMaxLayerPages)                    preEq = vg.getInsertPreEQ(VibeGraph::InsertKind::Layer, id - 200);
                else if (id >= 300 && id < 300 + kMaxBassPages)                     preEq = vg.getInsertPreEQ(VibeGraph::InsertKind::Bass,  id - 300);
                else if (id >= 400 && id < 500)                                     preEq = vg.getInsertPreEQ(VibeGraph::InsertKind::Audio, id - 400);
                else if (id >= 600 && id < 616)                                     preEq = vg.getInsertPreEQ(VibeGraph::InsertKind::Aux,   id - 600);
                else if (id >= 700 && id < 700 + (int) MixerChannelIds::kMaxVoxStrips)   preEq = vg.getInsertPreEQ(VibeGraph::InsertKind::Vox,   id - 700);
                else if (id >= 800 && id < 800 + (int) MixerChannelIds::kMaxInstStrips)  preEq = vg.getInsertPreEQ(VibeGraph::InsertKind::Inst,  id - 800);
                else if (id >= 900 && id < 900 + (int) MixerChannelIds::kMaxRustyStrips) preEq = vg.getInsertPreEQ(VibeGraph::InsertKind::Rusty, id - 900);
                break;
        }
        const juce::String chanPrefix = getMixerApvtsPrefixForChannel(id);
        if (preEq && chanPrefix.isNotEmpty())
        {
            mPreEQDisplay->bindMsDSP(preEq, &mProcessor.apvts,
                                      chanPrefix + "_preeq_mid_eq",
                                      chanPrefix + "_preeq_side_eq");
        }
        else
        {
            mPreEQDisplay->bindMsDSP(preEq ? preEq : &mPreEffectsEQDsp);
        }
        // C.4 Phase 1: same strip context as post-rack EQ above -- the
        // pre-rack EQ8 lives on the same strip, sees the same SC array.
        mPreEQDisplay->setStripContext(chanPrefix, resolveSrcName);
    }

    // 2026-04-26: restore the new channel's last-used sub-tab (default Rack).
    // Player channels never expose PreEQ - clamp PreEQ to Rack if persisted.
    {
        TabKind targetKind = TabKind::Rack;
        auto it = mLastTabPerChannel.find (newChanId);
        if (it != mLastTabPerChannel.end()) targetKind = it->second;
        if (currentChannelHasPagePreEQ() && targetKind == TabKind::PreEQ)
            targetKind = TabKind::Rack;
        switchTab (targetKind);
    }

    // §P4.3 (B6.2): channel kind may have changed Pre-tab visibility - ask
    // StandaloneEditor to re-call setTabSlots with the right 2-vs-3 labels.
    if (onTabsNeedRefresh) onTabsNeedRefresh();

    if (mFxBypassBtn)
    {
        // 5F-4a: re-attach FX Bypass button + parameter listener to the
        // newly-selected channel. Tear down any prior attachment / listener.
        mFxBypassAtt.reset();
        if (mBypassParamId.isNotEmpty())
            mProcessor.apvts.removeParameterListener(mBypassParamId, this);
        mBypassParamId.clear();

        const juce::String apvtsPrefix = getMixerApvtsPrefixForChannel(
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
// Can be called on any thread (APVTS may fire from audio thread); EffectRack's
// setRackBypassed is a plain bool store so this is safe.
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
    for (auto& s : mSlots)
        if (s) s->setRack(rack);

    if (rack)
    {
        if (mFxBypassBtn)
            mFxBypassBtn->setToggleState(rack->isRackBypassed(), juce::dontSendNotification);
        // Build inline editors for any pre-loaded slots
        for (int i = 0; i < EffectRack::kNumSlots; ++i)
            rebuildSlotEditor(i);
    }
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
        // Serialize DSP knob state into the snapshot.
        if (auto* eff = rack->getSlotEffect(i))
            eff->getStateInformation (s.dspState);
    }
    return snaps;
}

// Apply a target slot-snapshot.  Slots whose type+UUID match the current
// state are diff-applied (only bypass / output-gain / DSP-state restored -
// the existing effect instance is preserved).  Slots whose type or UUID
// differ get their effect reloaded with the snapshot's UUID so automation
// lanes survive.  Writes a parallel `outChanged` array indicating which
// slots had their effect instance replaced (so callers can rebuild only the
// affected editor panels - preserves slider SafePointers on untouched ones).
static void applySlotSnapshots(EffectRack* rack,
                               const EffectRackAction::SlotSnapshots& target,
                               std::array<bool, EffectRack::kNumSlots>& outChanged)
{
    outChanged.fill(false);
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
    }
}

void EffectsPage::onEffectChosen(int slotIndex, EffectType type)
{
    if (!mRack) return;
    auto before = captureSlotSnapshots(mRack);
    mRack->loadEffect(slotIndex, type);
    auto after = captureSlotSnapshots(mRack);
    if (mUndoCtx.isValid())
        mUndoCtx.perform(new EffectRackAction("Load Effect", before, after,
            [this](const EffectRackAction::SlotSnapshots& t) {
                std::array<bool, EffectRack::kNumSlots> changed;
                applySlotSnapshots(mRack, t, changed);
                // Only rebuild editors for slots whose DSP was actually
                // replaced - preserves sliders (and hence any queued
                // FloatParamActions' captured SafePointers) on untouched
                // slots. First perform() is skipped by EffectRackAction;
                // this runs only on undo/redo.
                for (int i = 0; i < EffectRack::kNumSlots; ++i)
                    if (changed[i]) rebuildSlotEditor(i);
                refreshAllSlots();
                mProcessor.setLatencySamples(mProcessor.mVibeGraph.updateBusLatencies());
            }), "Load Effect");
    rebuildSlotEditor(slotIndex);
    mSlots[slotIndex]->refresh();
    mProcessor.setLatencySamples(mProcessor.mVibeGraph.updateBusLatencies());
}

void EffectsPage::onEffectRemoved(int slotIndex)
{
    if (!mRack) return;
    auto before = captureSlotSnapshots(mRack);
    mRack->clearSlot(slotIndex);
    mRack->packSlotsToTop();
    auto after = captureSlotSnapshots(mRack);
    if (mUndoCtx.isValid())
        mUndoCtx.perform(new EffectRackAction("Remove Effect", before, after,
            [this](const EffectRackAction::SlotSnapshots& t) {
                std::array<bool, EffectRack::kNumSlots> changed;
                applySlotSnapshots(mRack, t, changed);
                for (int i = 0; i < EffectRack::kNumSlots; ++i)
                    if (changed[i]) rebuildSlotEditor(i);
                refreshAllSlots();
                mProcessor.setLatencySamples(mProcessor.mVibeGraph.updateBusLatencies());
            }), "Remove Effect");
    // Rebuild all 6 editors - slots shifted, so all need updating
    for (int i = 0; i < EffectRack::kNumSlots; ++i)
        rebuildSlotEditor(i);
    refreshAllSlots();
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
            [this](const EffectRackAction::SlotSnapshots& t) {
                std::array<bool, EffectRack::kNumSlots> changed;
                applySlotSnapshots(mRack, t, changed);
                for (int i = 0; i < EffectRack::kNumSlots; ++i)
                    if (changed[i]) rebuildSlotEditor(i);
                refreshAllSlots();
                mProcessor.setLatencySamples(mProcessor.mVibeGraph.updateBusLatencies());
            }), "Move Effect");

    // Rebuild editors for both swapped slots
    int other = up ? slotIndex - 1 : slotIndex + 1;
    if (other >= 0 && other < EffectRack::kNumSlots)
        rebuildSlotEditor(other);
    rebuildSlotEditor(slotIndex);
    refreshAllSlots();
}

void EffectsPage::rebuildSlotEditor(int slotIndex)
{
    if (!mRack || slotIndex < 0 || slotIndex >= EffectRack::kNumSlots) return;
    const auto& slot = mRack->getSlot(slotIndex);

    if (!mSlots[slotIndex]) return;

    DSPBase* eff = mRack->getSlotEffect(slotIndex);
    if (slot.type == EffectType::None || ! eff)
    {
        mSlots[slotIndex]->setEditor(nullptr);
    }
    else
    {
        auto editor = createEffectEditor(eff, slot.type);

        // Stamp automation paramIds on all knobs before handing off to the slot.
        // C13: keyed by slot UUID, not index, so reorder preserves automation.
        if (auto* base = dynamic_cast<EditorPanelBase*>(editor.get()))
        {
            base->setSlotContext(getChannelPrefix(), mRack->getSlotUuid(slotIndex));
            // QA-EffectsReview Task 1: stamp the panel's Basic/Advanced flag from
            // the slot's persisted state BEFORE setEditor() (which runs the first
            // resized()), so the initial layout matches the saved choice.
            base->mBasicMode = mRack->getSlotBasicMode(slotIndex);
        }

        // C.4 Phase 1 (2026-04-30): push the strip's mixer APVTS prefix +
        // a source-name resolver so the slot's SC dropdown can enumerate
        // routed lines and label them ("Layer 2", "Bass 1", "Master", ...).
        const int chId = mTrackBox ? mTrackBox->getSelectedId() : 0;
        const juce::String mixerPrefix = getMixerApvtsPrefixForChannel(chId);
        mSlots[slotIndex]->setChannelContext(&mProcessor.apvts, mixerPrefix,
                                              [](int id){ return MixerChannelIds::friendlyName(id); });

        mSlots[slotIndex]->setEditor(std::move(editor));
        mSlots[slotIndex]->setEditorUndoContext(mUndoCtx);
    }
}

juce::String EffectsPage::getChannelPrefix() const
{
    // Batch E #8 (2026-05-01): added missing channel categories that
    // previously fell through to "instr_<id>".  Audio Clips Bus, Vox/Inst
    // buses (incl. spawnable extras), Drum / Audio / Vox / Inst inserts
    // all now have proper prefixes for slot-context tagging.  Insert ranges
    // sourced from MixerChannelIds + VibesynthConstants so size bumps stay
    // in sync (kMaxInstStrips bumped 6 -> 10 -> 20 over G-4/G-6).
    const int id = mTrackBox ? mTrackBox->getSelectedId() : 0;
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
        default: break;
    }
    if (id >= 100 && id < 200)
        return "drum_" + juce::String(id - 100);
    if (id >= 200 && id < 200 + kMaxLayerPages)         // 8 layers (200..207)
        return "layer_" + juce::String(id - 199);
    if (id >= 300 && id < 300 + kMaxBassPages)          // 4 basses (300..303)
        return "bass_" + juce::String(id - 299);
    if (id >= 400 && id < 400 + MixerState::kMaxAudioRows) // 50 audio inserts
        return "audio_" + juce::String(id - 400);
    if (id >= 600 && id < 600 + MixerChannelIds::kMaxVoxStrips)  // 6 vox inserts
        return "vox_" + juce::String(id - 600);
    if (id >= 700 && id < 700 + MixerChannelIds::kMaxInstStrips) // 20 inst inserts
        return "inst_" + juce::String(id - 700);
    return "fx";
}

// 5F-4a: maps effects-page channel id → mixer-strip APVTS prefix.
// Returns empty string if the channel has no corresponding mixer strip.
juce::String EffectsPage::getMixerApvtsPrefixForChannel(int id) const
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

    return {};
}

void EffectsPage::refreshAllSlots()
{
    for (auto& s : mSlots)
        if (s) s->refresh();
}

// ── Keyboard ──────────────────────────────────────────────────────────────────
// 2026-04-26 (B-5): keyPressed + visibilityChanged removed.  Ctrl+Z / Ctrl+Alt+Z
// migrated to global BSCommands; the manager fires from any focus location.

// ── ChangeListener ────────────────────────────────────────────────────────────
void EffectsPage::changeListenerCallback(juce::ChangeBroadcaster*)
{
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
    auto b = getLocalBounds();

    // Header row removed - all controls live in PageMenuBar above.
    // Content fills full bounds.
    if (mRackTab)   mRackTab  ->setBounds(b);
    if (mEQTab)     mEQTab    ->setBounds(b);
    if (mPreEQTab)  mPreEQTab ->setBounds(b);   // §P4.3 (B6.2)

    // Layout within rack tab (slots)
    if (mRackTab && mRackTab->isVisible())
    {
        auto rack = mRackTab->getLocalBounds().reduced(4);
        int slotH = rack.getHeight() / 6;
        for (auto& s : mSlots)
            if (s) s->setBounds(rack.removeFromTop(slotH).reduced(0, 2));
    }

    // Layout within Post EQ tab
    if (mEQTab && mEQTab->isVisible() && mEQDisplay)
        mEQDisplay->setBounds(mEQTab->getLocalBounds().reduced(4));

    // Layout within Pre EQ tab (§P4.3 B6.2)
    if (mPreEQTab && mPreEQTab->isVisible() && mPreEQDisplay)
        mPreEQDisplay->setBounds(mPreEQTab->getLocalBounds().reduced(4));
}

// ── Tab switching ─────────────────────────────────────────────────────────────
// §P4.3 (B6.2): legacy entry point - interprets `index` as a VISIBLE-tab index
// (0..1 for player channels, 0..2 for Aux/Audio/Bus) and dispatches to the
// TabKind-aware overload.
void EffectsPage::switchTab(int index)
{
    switchTab(tabKindForVisibleIndex(index));
}

void EffectsPage::switchTab(TabKind kind)
{
    mCurrentTabKind = kind;
    mCurrentTab     = visibleIndexForTabKind(kind);
    if (mPreEQTab) mPreEQTab->setVisible(kind == TabKind::PreEQ);
    if (mRackTab)  mRackTab ->setVisible(kind == TabKind::Rack);
    if (mEQTab)    mEQTab   ->setVisible(kind == TabKind::PostEQ);

    // 2026-04-26: persist this tab kind for the current channel so re-entry
    // restores it.  Single source of truth - every tab change funnels through
    // here, so onChannelChanged's restore lookup will always find the most
    // recent value.  Skipped before any channel is selected (mPrevChannelId==0
    // during construction-time defaults).
    if (mPrevChannelId != 0)
        mLastTabPerChannel[mPrevChannelId] = kind;

    resized();
}

void EffectsPage::setEQMid(bool showMid)
{
    mShowingMid = showMid;
    if (mEQDisplay)    mEQDisplay   ->setShowMid(showMid);
    if (mPreEQDisplay) mPreEQDisplay->setShowMid(showMid);   // §P4.3 (B6.2)
}

// ── Meters menu ───────────────────────────────────────────────────────────────
void EffectsPage::showMetersMenu()
{
    // Build "VU Calibration (0 VU = ...)" sub-menu with -18 to -14 dBFS options
    // Item IDs 1000–1004 map to -18, -17, -16, -15, -14 dBFS respectively.
    // TODO: persist to settings.xml (Phase 5F)
    juce::PopupMenu calibMenu;
    const int kCalibBase = 1000;
    for (int db = -18; db <= -14; ++db)
    {
        bool isCurrent = (VUMeter::getCalibrationDb() == static_cast<float>(db));
        calibMenu.addItem(kCalibBase + (db + 18),
                          juce::String(db) + " dBFS",
                          true,   // enabled
                          isCurrent);
    }

    juce::PopupMenu metersMenu;
    metersMenu.addSubMenu("VU Calibration (0 VU = ...)", calibMenu);

    metersMenu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(mMetersBtn.get()),
        [this](int result)
        {
            if (result >= 1000 && result <= 1004)
            {
                int db = (result - 1000) - 18;  // maps 1000→-18, 1001→-17, … 1004→-14
                VUMeter::setCalibrationDb(static_cast<float>(db));
            }
        });
}

// ── Timer ─────────────────────────────────────────────────────────────────────
void EffectsPage::timerCallback()
{
    // 5F-4a: Button state is now driven by ButtonAttachment → APVTS.
    // The old force-sync from rack.isRackBypassed() fought with the attachment
    // and caused the click-flashes-and-reverts bug.

    // 12i: drive the EQ display so pre/post spectrum feeds are polled from the
    // bound EQ8MsDSP. Also pulls band-handle changes back into the widget when
    // processBlock's updateXxxEQ runs against an APVTS-backed bus EQ.
    if (mEQDisplay)    mEQDisplay   ->syncFromDSP();
    // C.4 follow-up (2026-04-30): pre-rack EQ display ALSO needs polling --
    // without this, the Pre EQ8 M/S tab's dynamic-band dotted curve never
    // updates when the user moves Range/Threshold/etc. in the popout
    // (the slider attachment writes APVTS, processBlock pushes APVTS to DSP,
    // but mBands never syncs back from DSP -> the cached UI band state
    // stays stale -> ghost curve doesn't follow live state).  Same fix
    // shape as the post-rack EQ above.
    if (mPreEQDisplay) mPreEQDisplay->syncFromDSP();
}
