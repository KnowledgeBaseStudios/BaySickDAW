#include "RibbonTabBar.h"
#include "SharedUI.h"
#include "../Hosting/PluginManager.h"   // QA-ModelShell TS6: added instrument list

// ── Colour helpers ───────────────────────────────────────────────────────────
juce::Colour RibbonTabBar::tabColour(TabType type, bool active)
{
    switch (type)
    {
        case TabType::Mixer:     return active ? juce::Colour(0xff7b2fbe) : juce::Colour(0xff3d1760);
        case TabType::Effects:   return active ? juce::Colour(0xffce3f8e) : juce::Colour(0xff661f47);
        case TabType::Builder:   return active ? juce::Colour(0xffd4a017) : juce::Colour(0xff6b5008);
        // 2026-04-28 (G-2): Clip = mixer Clips-bus amber/gold (VC::Warm).
        // Inactive shade is roughly half-brightness, matching the convention
        // used by Layers / Bass / Drums slots.
        case TabType::Clip:      return active ? juce::Colour(0xffd4a017) : juce::Colour(0xff6a500b);
        // TS6: Plugins match their mixer-bus colour (VC::Purple), same
        // one-channel-identity convention Clips/Vox/Inst use.
        case TabType::Plugins:   return active ? juce::Colour(0xff7b61ff) : juce::Colour(0xff3d3080);
        // 2026-04-28 (G-4): Vox + Inst match their mixer-bus colours so the
        // ribbon tab + page header + mixer strip read as one channel identity
        // (same convention Clips uses with VC::Warm).  Bus colours come from
        // MixerPage.cpp's laidOutBus calls.
        case TabType::Vox:       return active ? juce::Colour(0xff0fafa5) : juce::Colour(0xff075853);   // teal
        case TabType::Inst:      return active ? juce::Colour(0xff1c3a8a) : juce::Colour(0xff0e1d45);   // navy
        case TabType::Layers:    return active ? juce::Colour(0xffe06030) : juce::Colour(0xff703018);
        case TabType::Bass:      return active ? juce::Colour(0xff2e8b57) : juce::Colour(0xff17452b);
        case TabType::Drums:     return active ? juce::Colour(0xffcc2222) : juce::Colour(0xff661111);
        // 2026-04-26: PianoRoll = piano-key black; active brightens slightly
        // so the active-state is still visible against the inactive black.
        case TabType::PianoRoll: return active ? juce::Colour(0xff1a1a1a) : juce::Colour(0xff060606);
        default:                 return active ? VC::Panel : VC::Bg;
    }
}

// ── Static helpers ───────────────────────────────────────────────────────────
bool RibbonTabBar::isRequiredTab (TabType type)
{
    // Locked call: these four are always present regardless of instance count
    // -- they are app surfaces, not instance collections.
    return type == TabType::Builder || type == TabType::Mixer
        || type == TabType::Effects || type == TabType::PianoRoll;
}

std::vector<RibbonTabBar::TabType> RibbonTabBar::visibleSlotTypes() const
{
    // L27 (QA-Layout): the four required app-surface tabs sit together at the
    // left -- Piano Roll moved from last to beside Effects -- then the
    // instance types in their historical order.
    static constexpr TabType order[] = {
        TabType::Builder, TabType::Mixer, TabType::Effects, TabType::PianoRoll,
        TabType::Clip, TabType::Vox, TabType::Inst,
        TabType::Layers, TabType::Bass, TabType::Drums,
        TabType::Plugins
    };

    std::vector<TabType> out;
    for (auto t : order)
        if (isRequiredTab (t) || countTabsOfType (t) > 0)
            out.push_back (t);
    return out;
}

int RibbonTabBar::numSlots() const
{
    return (int) visibleSlotTypes().size() + 1;   // + the trailing "+" slot
}

bool RibbonTabBar::isAddSlot (int slotIndex) const
{
    return slotIndex == numSlots() - 1;
}

RibbonTabBar::TabType RibbonTabBar::slotType(int slotIndex) const
{
    const auto v = visibleSlotTypes();
    if (v.empty()) return TabType::Builder;
    return v[(size_t) juce::jlimit (0, (int) v.size() - 1, slotIndex)];
}

bool RibbonTabBar::hasDropdown(TabType type)
{
    // 2026-04-26 (1a): PianoRoll's engine-picker dropdown lands in step 1b /
    // step 2 once the page actually hosts a DrumKit + per-engine roll list.
    // For 1a the slot is click-only, no chevron.
    if (type == TabType::PianoRoll) return false;
    return type != TabType::Mixer;
}

// ── Ctor ─────────────────────────────────────────────────────────────────────
RibbonTabBar::RibbonTabBar()
{
    // Create the 3 permanent single-instance tabs.
    // Layers/Bass/Drums instances are added via addTab() from StandaloneEditor.
    auto addFixed = [this](TabType t, const char* name)
    {
        Tab tab;
        tab.id   = mNextId++;
        tab.type = t;
        tab.name = name;
        mTabs.add(tab);
    };
    addFixed(TabType::Mixer,     "Mixer");
    addFixed(TabType::Effects,   "Effects");
    addFixed(TabType::Builder,   "Builder");
    // The addFixed ids are handed out in this creation order and persisted in
    // project state -- never reorder these calls.  On-screen slot order is
    // visibleSlotTypes()'s order[], not mTabs order.
    addFixed(TabType::PianoRoll, "Piano Roll");

    mSelectedId = mTabs[0].id;
    if (! mTabs.isEmpty())
        mLastUsedByType[mTabs[0].type] = mTabs[0].id;
    setInterceptsMouseClicks(true, true);
}

// ── Public API ───────────────────────────────────────────────────────────────
int RibbonTabBar::addTab(TabType type, const juce::String& name)
{
    Tab tab;
    tab.id   = mNextId++;
    tab.type = type;
    tab.name = name;
    mTabs.add(tab);
    repaint();
    return tab.id;
}

void RibbonTabBar::clearAllDynamicTabs()
{
    for (int i = mTabs.size() - 1; i >= 0; --i)
    {
        const auto t = mTabs[i].type;
        if (t == TabType::Layers || t == TabType::Bass || t == TabType::Drums
         || t == TabType::Clip   || t == TabType::Vox  || t == TabType::Inst
         || t == TabType::Plugins)
            mTabs.remove (i);
    }
    // Drop selection if it pointed at a removed tab.
    bool stillValid = false;
    for (auto& t : mTabs) if (t.id == mSelectedId) { stillValid = true; break; }
    if (! stillValid) mSelectedId = -1;
    repaint();
}

void RibbonTabBar::clearTabsOfType (TabType type)
{
    for (int i = mTabs.size() - 1; i >= 0; --i)
        if (mTabs[i].type == type)
            mTabs.remove (i);
    bool stillValid = false;
    for (auto& t : mTabs) if (t.id == mSelectedId) { stillValid = true; break; }
    if (! stillValid) mSelectedId = -1;
    repaint();
}

void RibbonTabBar::closeTab(int tabId)
{
    for (int i = 0; i < mTabs.size(); ++i)
    {
        if (mTabs[i].id != tabId) continue;

        TabType type = mTabs[i].type;

        // D1.4-fix (c): Layers / Bass / Drums instances can all be closed.
        // G-2 (2026-04-28): Clip instances are also closeable.
        // G-4 (2026-04-28): Vox / Inst follow the same drop-spawn pattern.
        if (type != TabType::Layers && type != TabType::Bass
         && type != TabType::Drums  && type != TabType::Clip
         && type != TabType::Vox    && type != TabType::Inst
         && type != TabType::Plugins) return;

        // QA-ProjectSave docket 18 (2026-07-26): every closeable type may reach
        // zero.  Layers / Bass / Drums used to be pinned at >= 1 from when the
        // app was only those three; that floor meant a saved project or template
        // always carried tabs the user never asked for.  The `force` parameter
        // that existed solely to bypass that floor went with it.

        mTabs.remove(i);

        // If we removed the selected tab, select another of the same type
        if (mSelectedId == tabId)
        {
            for (auto& t : mTabs)
            {
                if (t.type == type)
                {
                    mSelectedId = t.id;
                    mLastUsedByType[t.type] = t.id;
                    if (onTabSelected) onTabSelected(mSelectedId);
                    break;
                }
            }
        }

        repaint();
        if (onTabClosed) onTabClosed(tabId);
        return;
    }
}

void RibbonTabBar::selectTab(int tabId)
{
    for (auto& t : mTabs)
    {
        if (t.id == tabId)
        {
            mSelectedId = tabId;
            mLastUsedByType[t.type] = tabId;   // J-6: per-type last-used cache
            repaint();
            return;
        }
    }
}

void RibbonTabBar::renameTab(int tabId, const juce::String& newName)
{
    for (auto& t : mTabs)
    {
        if (t.id == tabId) { t.name = newName; repaint(); return; }
    }
}

const RibbonTabBar::Tab* RibbonTabBar::getTabById(int id) const
{
    for (auto& t : mTabs)
        if (t.id == id) return &t;
    return nullptr;
}

// ── Internal helpers ─────────────────────────────────────────────────────────
int RibbonTabBar::getActiveTabForType(TabType type) const
{
    // 1. If the currently selected tab is of this type, use it.
    auto* sel = getTabById(mSelectedId);
    if (sel && sel->type == type) return mSelectedId;

    // 2. J-6 (2026-05-03): otherwise prefer the user's last-visited tab of
    //    this type.  Verifies the id still exists in mTabs (covers the case
    //    where the cached tab was closed since last visit).
    if (auto it = mLastUsedByType.find(type); it != mLastUsedByType.end())
        if (auto* cached = getTabById(it->second); cached && cached->type == type)
            return cached->id;

    // 3. Fallback: first tab of this type.
    for (auto& t : mTabs)
        if (t.type == type) return t.id;
    return -1;
}

int RibbonTabBar::countTabsOfType(TabType type) const
{
    int n = 0;
    for (auto& t : mTabs)
        if (t.type == type) ++n;
    return n;
}

int RibbonTabBar::getBadgeCount(TabType type) const
{
    switch (type)
    {
    // QA-A Phase 5 (2026-05-09): Effects + Builder previously showed
    // hardcoded sub-page counters (Effects=2 for Rack/EQ; Builder=3 for
    // Patterns/Audio Clips/Automation) for visual continuity with the
    // instance-count badges on the variable-name slots.  Per Jeff the
    // counters added no information ("the dropdown arrow already says
    // there's a sub-menu"), and the badge region ate ~20 px of slot
    // width that the new variable-width layout could otherwise hand to
    // long-named tabs.  Both cases fall through to default = 0 now.
    case TabType::Clip:    // G-2 (2026-04-28): badge tracks instance count
    case TabType::Vox:     // G-4 (2026-04-28)
    case TabType::Inst:    // G-4 (2026-04-28)
    case TabType::Plugins: // QA-ModelShell TS6
    case TabType::Layers:
    case TabType::Bass:
    case TabType::Drums:   return countTabsOfType(type);
    default:               return 0;
    }
}

bool RibbonTabBar::isSlotSelected(int slotIndex) const
{
    auto* tab = getTabById(mSelectedId);
    return tab && tab->type == slotType(slotIndex);
}

juce::String RibbonTabBar::getSlotDisplayName(int slotIndex) const
{
    TabType type = slotType(slotIndex);
    int activeId = getActiveTabForType(type);
    auto* tab    = getTabById(activeId);
    if (! tab)
    {
        // G-2 (2026-04-28): Clip slot is valid with zero instances (drop-only
        // spawn).  Fall back to a generic label so the empty state is visible.
        // G-4 (2026-04-28): Vox + Inst follow the same pattern.
        // QA-ProjectSave docket 18 (2026-07-26): Layers / Bass / Drums joined
        // them.  Without these three the slots rendered as unlabelled coloured
        // blocks the moment their last tab was deleted -- the label had always
        // come from the active tab's name, which only worked while a tab was
        // guaranteed to exist.
        if (type == TabType::Clip)   return "Clips";
        if (type == TabType::Plugins) return "Plugins";
        if (type == TabType::Vox)    return "Vox";
        if (type == TabType::Inst)   return "Inst";
        if (type == TabType::Layers) return "Layers";
        if (type == TabType::Bass)   return "Bass";
        if (type == TabType::Drums)  return "Drums";
        return {};
    }
    return (tab->locked ? juce::String("[L] ") : juce::String()) + tab->name;
}

void RibbonTabBar::setTabLocked (int tabId, bool locked)
{
    for (auto& t : mTabs)
    {
        if (t.id == tabId)
        {
            if (t.locked != locked)
            {
                t.locked = locked;
                repaint();
                // 2026-05-05 dirty-flag wiring: lock toggle is project state.
                if (onTabLockChanged) onTabLockChanged (tabId, locked);
            }
            return;
        }
    }
}

bool RibbonTabBar::isTabLocked (int tabId) const
{
    if (auto* t = getTabById (tabId))
        return t->locked;
    return false;
}

void RibbonTabBar::moveTabOfType (TabType type, int srcRowOfType, int dstRowOfType)
{
    std::vector<int> typeIdxs;
    for (int i = 0; i < mTabs.size(); ++i)
        if (mTabs[i].type == type)
            typeIdxs.push_back (i);

    const int n = (int) typeIdxs.size();
    if (srcRowOfType < 0 || srcRowOfType >= n) return;
    if (dstRowOfType < 0 || dstRowOfType >= n) return;
    if (srcRowOfType == dstRowOfType) return;

    mTabs.move (typeIdxs[(size_t) srcRowOfType], typeIdxs[(size_t) dstRowOfType]);
    repaint();
}

// ── Layout helpers (QA-A Phase 5 / STYLE-01, 2026-05-09) ─────────────────────
bool RibbonTabBar::isFixedNameSlot (TabType type)
{
    return type == TabType::Mixer
        || type == TabType::Effects
        || type == TabType::Builder
        || type == TabType::PianoRoll;
}

// Pure measurement: bold-font text width + 16 px padding.  Arrow + badge sit
// on the bottom row (L25) and add no width.  No clamping to min/max here --
// the caller (slotRect) applies the per-slot floor and the cap itself.  Bold
// (the active-state font) is the worst case so a slot never changes width
// when clicked between active and inactive.
int RibbonTabBar::naturalSingleLineWidth (int slotIndex) const
{
    const juce::Font font (12.0f, juce::Font::bold);
    return font.getStringWidth(getSlotDisplayName(slotIndex)) + 16;
}

// QA-A Phase 5 / STYLE-01 (2026-05-09): constraint-based variable-width
// layout with min floors and a max single-line cap that triggers wrap.
//   1. desired_i = clamp(natural_i, minW_i, kMaxSingleLine).
//      -- natural above kMaxSingleLine -> slot caps at the max and
//         drawFittedText shrinks the label in paint().
//      -- natural below the slot's per-type minimum -> floored to that
//         minimum so short labels keep visual presence.
//   2. If sum(desired) <= totalW: each slot gets desired plus an equal
//      share of the leftover slack.
//   3. Else (shrink case): redistribute the excess proportionally to each
//      slot's "shrink room" (desired - minW), so no slot crosses its own
//      floor until every slot has hit its floor together.  If even all-
//      at-min still overflows totalW (very narrow window), fall back to
//      pure proportional scaling -- drawFittedText in paint() picks up
//      the slack on per-slot text fitting.
int RibbonTabBar::addSlotWidth() const
{
    // Same font paint() draws the glyph with, so the slot tracks the symbol.
    const juce::Font f (18.0f, juce::Font::bold);
    return juce::jmax (12, f.getStringWidth ("+") * 2);
}

juce::Rectangle<int> RibbonTabBar::slotRect (int slotIndex) const
{
    const int totalH = getHeight() > 0 ? getHeight() : kTabH;
    const int addW   = addSlotWidth();
    // The "+" is carved off the right edge first; everything below divides the
    // remainder among the TYPE slots only.
    const int totalW = juce::jmax (0, getWidth() - addW);

    if (isAddSlot (slotIndex))
        return { totalW, 0, addW, totalH };

    const int nSlots = juce::jmax (1, numSlots() - 1);
    int desired[kMaxSlots];
    int minW   [kMaxSlots];
    int sumDesired = 0;
    for (int s = 0; s < nSlots; ++s)
    {
        const auto type = slotType(s);
        int natural     = naturalSingleLineWidth(s);
        if (natural > kMaxSingleLine) natural = kMaxSingleLine;

        const int minS = isFixedNameSlot(type) ? kMinFixed : kMinVariable;
        if (natural < minS) natural = minS;

        desired[s] = natural;
        minW   [s] = minS;
        sumDesired += natural;
    }

    int widths[kMaxSlots];
    if (sumDesired <= totalW)
    {
        const int slack      = totalW - sumDesired;
        const int slackPer   = slack / nSlots;
        const int slackExtra = slack - slackPer * nSlots;
        for (int s = 0; s < nSlots; ++s)
            widths[s] = desired[s] + slackPer + (s < slackExtra ? 1 : 0);
    }
    else
    {
        const int excess = sumDesired - totalW;
        int shrinkRoom = 0;
        for (int s = 0; s < nSlots; ++s)
            shrinkRoom += desired[s] - minW[s];

        int sum = 0;
        if (shrinkRoom >= excess && shrinkRoom > 0)
        {
            for (int s = 0; s < nSlots; ++s)
            {
                const int room = desired[s] - minW[s];
                const int shrink = (excess * room + shrinkRoom / 2) / shrinkRoom;
                widths[s] = desired[s] - shrink;
                sum += widths[s];
            }
        }
        else
        {
            // All slots already at min and still overflows -- proportional
            // scale below min as last resort.  drawFittedText handles text.
            for (int s = 0; s < nSlots; ++s)
            {
                widths[s] = (desired[s] * totalW + sumDesired / 2) / sumDesired;
                sum += widths[s];
            }
        }
        widths[nSlots - 1] += (totalW - sum);
    }

    int x = 0;
    for (int s = 0; s < slotIndex; ++s)
        x += widths[s];
    return { x, 0, widths[slotIndex], totalH };
}

int RibbonTabBar::hitTestSlot(juce::Point<int> pos, bool& hitArrow) const
{
    hitArrow = false;
    const int nSlots = numSlots();
    for (int s = 0; s < nSlots; ++s)
    {
        auto r = slotRect(s);
        if (r.contains(pos))
        {
            // The "+" slot has no dropdown arrow -- the whole slot is one hit.
            // L25: the arrow lives on the bottom row; a name-row click
            // anywhere in the slot navigates.
            if (! isAddSlot (s) && hasDropdown(slotType(s)))
            {
                auto arrowR = r.withTrimmedLeft(r.getWidth() - kArrowW)
                               .withTrimmedTop(kNameRowH);
                hitArrow = arrowR.contains(pos);
            }
            return s;
        }
    }
    return -1;
}

// ── Mouse ────────────────────────────────────────────────────────────────────
void RibbonTabBar::mouseDown(const juce::MouseEvent& e)
{
    bool hitArrow = false;
    int slot = hitTestSlot(e.position.toInt(), hitArrow);
    if (slot < 0) return;

    // QA-ModelShell TS4: the trailing "+" slot owns every add option.
    if (isAddSlot (slot))
    {
        showAddMenu (slotRect (slot));
        return;
    }

    TabType type = slotType(slot);

    if (hitArrow)
    {
        showDropdown(slot);
    }
    else
    {
        // Body click: navigate to the active tab for this type.  A type slot is
        // only VISIBLE while it has >= 1 instance now, so the six empty-state
        // branches that used to live here are gone with the placeholders they
        // drove -- there is no such thing as clicking an empty type tab.
        int tabId = getActiveTabForType(type);
        if (tabId >= 0)
        {
            mSelectedId = tabId;
            if (auto* tab = getTabById(tabId))
                mLastUsedByType[tab->type] = tabId;
            repaint();
            if (onTabSelected) onTabSelected(tabId);
        }
    }
}

// The "+" menu.  Every way to bring a page into existence lives here, including
// the routes that used to be buried in a populated type's dropdown (so they
// were unreachable at zero instances) or on another page entirely.
void RibbonTabBar::showAddMenu (juce::Rectangle<int> slotBounds)
{
    // Jeff spec 2026-07-28: every entry is an ENGINE name, and the engine
    // decides the tab.  Engines that can live in more than one tab get a side
    // submenu; the rest go straight in.  Nothing here is a page-type name and
    // nothing is a disabled "go do it somewhere else" label.
    // QA-Layout L2/L3/L28 (2026-08-03): order + strings are Jeff's locked
    // list; the BaySickDrums entry absorbs the drum routes that used to hide
    // inside the BaySickPlayer / BaySickSynth submenus.

    // Inst is a SHARED cap: BaySickGuitars, BaySickBasses and the live
    // instrument chain all consume Inst slots, so the same gate covers all
    // three.
    const bool instCapped = onIsInstCapReached && onIsInstCapReached();

    juce::PopupMenu m;
    mAddMenuChoices.clear();

    auto pushChoice = [this] (TabType type, const juce::String& engine)
    {
        const int id = kAddEngineBaseId + (int) mAddMenuChoices.size();
        mAddMenuChoices.push_back ({ type, engine });
        return id;
    };

    m.addItem (pushChoice (TabType::Vox,  "BaySickVocal"),    "BaySickVocal");
    m.addItem (pushChoice (TabType::Inst, "BaySickLiveInst"), "BaySickLiveInst", ! instCapped);

    // The sfizz engines keep their own spawn routes (kit load + strip cascade),
    // so they fire their dedicated callbacks rather than the generic one.
    m.addItem (1, "BaySickGuitars", ! instCapped);
    m.addItem (2, "BaySickBasses",  ! instCapped);

    // QA-ModelShell TS6 (BLU-447): hosted VST3 instruments, as a SIDE DROPDOWN
    // off one entry (Jeff's spec) rather than N rows in the top-level list --
    // an added-plugin list can be long and would swamp the engines around it.
    // Stays alphabetical (Jeff, 2026-08-03) -- getAddedInstruments() sorts.
    // No new callback: AddChoice::engine is a juce::String and EngineRig keys a
    // plugin tab on the plugin's identifier string, so these ride the existing
    // onAddEngineRequest path exactly like a built-in engine name.
    {
        juce::Array<juce::PluginDescription> instruments;

        if (auto* pm = Hosting::PluginManager::getInstance())
            instruments = pm->getAddedInstruments();

        juce::PopupMenu sub;

        if (instruments.isEmpty())
            sub.addItem (-99, "None added - see Options > Plugins", false, false);
        else
            for (const auto& d : instruments)
                sub.addItem (pushChoice (TabType::Plugins, d.createIdentifierString()), d.name);

        m.addSubMenu ("VSTPlugin", sub);
    }

    {
        juce::PopupMenu sub;
        sub.addItem (pushChoice (TabType::Layers, "Harmless"), "Layers");
        sub.addItem (pushChoice (TabType::Bass,   "Harmless"), "Bass");
        m.addSubMenu ("Harmless", sub);
    }

    // Flat row: with the drum route under BaySickDrums, Layers is the only
    // tab BaySickSynth can land in, so no submenu.
    m.addItem (pushChoice (TabType::Layers, "BaySickSynth"), "BaySickSynth");

    {
        juce::PopupMenu sub;
        sub.addItem (pushChoice (TabType::Layers, "BaySickPlayer"), "Layers");
        sub.addItem (pushChoice (TabType::Bass,   "BaySickPlayer"), "Bass");
        sub.addItem (pushChoice (TabType::Clip,   "BaySickPlayer"), "Audio Clips");
        m.addSubMenu ("BaySickPlayer", sub);
    }

    m.addItem (pushChoice (TabType::Bass, "BaySickBass"), "BaySickBass");

    // L28: the drum grab is its own top-level entry; the submenu picks the
    // player that backs the new Drums tab.
    {
        juce::PopupMenu sub;
        sub.addItem (pushChoice (TabType::Drums, "BaySickPlayer"), "BaySickPlayer");
        sub.addItem (pushChoice (TabType::Drums, "BaySickSynth"),  "BaySickSynth");
        m.addSubMenu ("BaySickDrums", sub);
    }

    const bool rustyLive = onIsBaySickRustyDrumsActive && onIsBaySickRustyDrumsActive();
    m.addItem (3, "BaySickRustyDrums", ! rustyLive);

    m.showMenuAsync (juce::PopupMenu::Options{}
                        .withTargetComponent (this)
                        .withTargetScreenArea (localAreaToGlobal (slotBounds)),
        [this] (int r)
        {
            if (r <= 0) return;
            if (r == 1) { if (onAddBaySickGuitarsRequest)    onAddBaySickGuitarsRequest();    return; }
            if (r == 2) { if (onAddBaySickBassesRequest)     onAddBaySickBassesRequest();     return; }
            if (r == 3) { if (onAddBaySickRustyDrumsRequest) onAddBaySickRustyDrumsRequest(); return; }

            const int idx = r - kAddEngineBaseId;
            if (idx >= 0 && idx < (int) mAddMenuChoices.size() && onAddEngineRequest)
                onAddEngineRequest (mAddMenuChoices[(size_t) idx].type,
                                    mAddMenuChoices[(size_t) idx].engine);
        });
}

// ── Dropdown menus ───────────────────────────────────────────────────────────
void RibbonTabBar::showDropdown(int slotIndex)
{
    TabType type = slotType(slotIndex);
    // Anchor the popup to the ▾ arrow region, not the full tab
    auto r       = slotRect(slotIndex);
    auto arrowR  = r.withTrimmedLeft(r.getWidth() - kArrowW);

    switch (type)
    {
    case TabType::Effects:
    case TabType::Builder:
        showSubPageDropdown(type, arrowR);
        break;
    case TabType::Layers:
    case TabType::Bass:
    case TabType::Drums:
        showInstanceDropdown(type, arrowR);
        break;
    case TabType::Clip:
    case TabType::Vox:
    case TabType::Inst:
    case TabType::Plugins:
        // G-6 (2026-04-29): always show the dropdown - even at 0 instances -
        // so the +Add entry is reachable.  showInstanceDropdown handles the
        // 0-instance case by showing only the +Add (skipping Pages/Rename/
        // Delete which have no active instance to operate on).
        showInstanceDropdown(type, arrowR);
        break;
    default:
        break;
    }
}

void RibbonTabBar::showSubPageDropdown(TabType type, juce::Rectangle<int> tabBounds)
{
    juce::PopupMenu m;

    if (type == TabType::Effects)
    {
        // QA-ModelShell TS5: the single "EQ" entry split in two because the
        // pre- and post-rack EQs are now separate windows that can both be open.
        m.addItem(1, "Rack");
        m.addItem(2, "Pre EQ");
        m.addItem(3, "Post EQ");
    }
    else if (type == TabType::Builder)
    {
        m.addItem(1, "Patterns");
        m.addItem(2, "Audio Clips");
        m.addItem(3, "Automation");
    }
    else if (type == TabType::Drums)
    {
        m.addItem(1, "Sound");
        m.addItem(2, "Piano Roll");
        m.addItem(3, "EQ");
    }

    auto screenArea = localAreaToGlobal(tabBounds);
    m.showMenuAsync(
        juce::PopupMenu::Options().withTargetScreenArea(screenArea),
        [this, type](int result)
        {
            if (result <= 0) return;

            // Navigate to this page type first
            int tabId = getActiveTabForType(type);
            if (tabId >= 0)
            {
                mSelectedId = tabId;
                if (auto* tab = getTabById(tabId))
                    mLastUsedByType[tab->type] = tabId;
                repaint();
                if (onTabSelected) onTabSelected(tabId);
            }
            // Then notify about sub-page selection (0-based index)
            if (onSubPageSelected)
                onSubPageSelected(type, result - 1);
        });
}

void RibbonTabBar::showInstanceDropdown(TabType type, juce::Rectangle<int> tabBounds)
{
    juce::PopupMenu m;

    int activeId = getActiveTabForType(type);
    int count    = countTabsOfType(type);

    // G-6 (2026-04-29): Pages/Rename/Delete are only meaningful when at least
    // one instance exists.  Skip them at count == 0 so the dropdown shows
    // ONLY the +Add (clean UX - nothing to navigate to or rename otherwise).
    if (count > 0)
    {
        // List all instances - tick mark on the active one, "[L] " prefix if locked
        for (auto& tab : mTabs)
        {
            if (tab.type == type)
            {
                const juce::String label = (tab.locked ? juce::String("[L] ") : juce::String()) + tab.name;
                m.addItem(tab.id, label, true, tab.id == activeId);
            }
        }

        m.addSeparator();

        // QA-Layout T4 (L11/D4=c): the "Pages:" section is the instance's
        // real WINDOW list, built by the editor (incl. Pre/Post EQ rows).
        // Negative IDs reserved for menu actions: -1/-2 (rename/delete),
        // -3..-6 (adds), -99 (headers); rows take -10 - i.
        // Note: PopupMenu has no addSectionHeading; use a disabled item.
        m.addItem(-99, "Pages:", false /* enabled */, false);
        juce::StringArray pageRows;
        if (onListPageWindowRows) pageRows = onListPageWindowRows (activeId);
        for (int i = 0; i < pageRows.size(); ++i)
            m.addItem (-10 - i, "  " + pageRows[i]);

        m.addSeparator();

        // Rename / Delete apply to the currently active instance.
        //
        // QA-ModelShell TS4 (2026-07-28): Delete is ALWAYS enabled.  This used
        // to hold Layer/Bass/Drum at count > 1 on the reasoning that "project
        // must always have at least one of each" -- a floor QA-ProjectSave
        // docket 18 already retired everywhere else, and which this task set
        // depends on being gone: a type tab now VANISHES from the ribbon at
        // zero instances and returns via "+", so refusing to delete the last
        // one left no way to remove a type the user does not want.  Found by
        // Jeff 2026-07-28 -- deleting the empty-state pages removed the
        // placeholder this gate's comment was written around, but the gate
        // itself survived and silently reinstated the old floor.
        m.addItem(-1, "Rename...");
        m.addItem(-2, "Delete", true);

        m.addSeparator();
    }

    // QA-Layout T2 (Jeff map, 2026-08-03): the bottom section names the
    // PLAYERS this type can load -- picking one creates the tab with that
    // player already loaded, mirroring the "+" slot's routes scoped to the
    // type.  The old generic "+ Add New X" rows made an ENGINELESS page,
    // whose picker no longer exists (L4).  Clip keeps the file-picker route
    // (-3): a clip is born from an audio file, and BaySickPlayer is implied.
    mAddMenuChoices.clear();
    auto addEngineRow = [this, &m] (const juce::String& engine,
                                    TabType targetType,
                                    const juce::String& label,
                                    bool enabled = true)
    {
        const int id = kAddEngineBaseId + (int) mAddMenuChoices.size();
        mAddMenuChoices.push_back ({ targetType, engine });
        m.addItem (id, label, enabled);
    };

    if (type == TabType::Layers)
    {
        addEngineRow ("Harmless",      type, "+ Add Harmless");
        addEngineRow ("BaySickPlayer", type, "+ Add BaySickPlayer");
        addEngineRow ("BaySickSynth",  type, "+ Add BaySickSynth");
    }
    else if (type == TabType::Bass)
    {
        addEngineRow ("Harmless",      type, "+ Add Harmless");
        addEngineRow ("BaySickPlayer", type, "+ Add BaySickPlayer");
        addEngineRow ("BaySickBass",   type, "+ Add BaySickBass");
    }
    else if (type == TabType::Drums)
    {
        addEngineRow ("BaySickPlayer", type, "+ Add BaySickPlayer");
        addEngineRow ("BaySickSynth",  type, "+ Add BaySickSynth");
    }
    else if (type == TabType::Vox)
    {
        addEngineRow ("BaySickVocal", type, "+ Add BaySickVocal");
    }
    else if (type == TabType::Inst)
    {
        const bool capReached = onIsInstCapReached && onIsInstCapReached();
        addEngineRow ("BaySickLiveInst", type, "+ Add BaySickLiveInst", ! capReached);
    }
    else if (type == TabType::Clip)
    {
        m.addItem (-3, "+ Add BaySickPlayer...");
    }
    else if (type == TabType::Plugins)
    {
        // Side menu, alphabetical (Jeff, 2026-08-03) -- same sorted
        // getAddedInstruments() source as the "+" slot's VSTPlugin submenu.
        juce::Array<juce::PluginDescription> instruments;
        if (auto* pm = Hosting::PluginManager::getInstance())
            instruments = pm->getAddedInstruments();

        juce::PopupMenu sub;
        if (instruments.isEmpty())
            sub.addItem (-99, "None added - see Options > Plugins", false, false);
        else
            for (const auto& d : instruments)
            {
                const int id = kAddEngineBaseId + (int) mAddMenuChoices.size();
                mAddMenuChoices.push_back ({ TabType::Plugins, d.createIdentifierString() });
                sub.addItem (id, d.name);
            }
        m.addSubMenu ("+ Add VSTPlugin", sub);
    }

    // QA-Fa recovery: "+ Add New Vox From Export" submenu (Vox only; the one
    // sanctioned exception to the old instance-switcher-only convention,
    // owner design 2026-07-10).  Greyed when the editor returns no entries
    // (no exports / vox cap reached / project unsaved).
    if (type == TabType::Vox)
    {
        mVoxExportShown.clear();
        if (onListVoxExports)
            mVoxExportShown = onListVoxExports();
        juce::PopupMenu sub;
        juce::String lastFolder;
        for (int i = 0; i < (int) mVoxExportShown.size(); ++i)
        {
            const auto& e = mVoxExportShown[(size_t) i];
            if (e.folder != lastFolder)
            {
                sub.addItem(-99, e.folder + ":", false, false);
                lastFolder = e.folder;
            }
            sub.addItem(kVoxExportBaseId + i, "  " + e.name);
        }
        m.addSubMenu("+ Add New Vox From Export", sub, ! mVoxExportShown.empty());
    }

    // J-6 (2026-05-03): "+ Add BaySickRustyDrums" entry - only on the Drums
    // dropdown, only when the singleton isn't already spawned (1-instance lock).
    if (type == TabType::Drums)
    {
        const bool brdActive = onIsBaySickRustyDrumsActive
                            && onIsBaySickRustyDrumsActive();
        if (! brdActive)
            m.addItem(-4, "+ Add BaySickRustyDrums");
    }

    // K-4 / L-3 (2026-05-05): "+ Add BaySickGuitars" + "+ Add BaySickBasses"
    // entries - only on the Inst dropdown.  Disabled when the shared 20-page
    // cap is reached (live-input Inst + BaySickGuitars + BaySickBasses
    // combined ≤ kMaxInstPages).
    if (type == TabType::Inst)
    {
        const bool capReached = onIsInstCapReached && onIsInstCapReached();
        m.addItem(-5, "+ Add BaySickGuitars", ! capReached);
        m.addItem(-6, "+ Add BaySickBasses",  ! capReached);
    }

    auto screenArea = localAreaToGlobal(tabBounds);
    m.showMenuAsync(
        juce::PopupMenu::Options().withTargetScreenArea(screenArea),
        [this, type, activeId](int result)
        {
            if (result == 0) return;

            if (result == -1)
            {
                // Rename active instance
                if (activeId >= 0) startRename(activeId);
            }
            else if (result == -2)
            {
                // Delete active instance.
                if (activeId < 0) return;

                // D2: refuse delete when the active tab is locked.  Mirror the
                // per-page right-click context menu, which greys out Delete
                // when locked.
                if (isTabLocked (activeId))
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::AlertWindow::InfoIcon,
                        "Cannot Delete",
                        "This tab is locked. Unlock it first to delete.");
                    return;
                }

                // D2: route through the page's requestDelete() (Save & Delete /
                // Delete Anyway / Cancel for saveable engines).  Falls back to
                // the ribbon's own primitive confirm only if no handler is
                // wired (defensive - StandaloneEditor wires this in ctor).
                if (onTabDeleteRequested)
                {
                    onTabDeleteRequested (activeId);
                }
                else
                {
                    auto* aw = new juce::AlertWindow(
                        "Delete",
                        "This action cannot be undone. Are you sure?",
                        juce::MessageBoxIconType::WarningIcon);
                    aw->addButton("Delete", 1);
                    aw->addButton("Cancel", 0);
                    aw->enterModalState(true,
                        juce::ModalCallbackFunction::create(
                            [this, activeId](int r) {
                                if (r == 1) closeTab(activeId);
                            }),
                        true);
                }
            }
            else if (result == -3)
            {
                // Clip only: opens the OS audio-file picker (editor closure).
                if (onAddTabRequest) onAddTabRequest(type);
            }
            else if (result >= kAddEngineBaseId)
            {
                // Engine-named add row (QA-Layout T2) -- same dispatch as the
                // "+" slot's menu.
                const int idx = result - kAddEngineBaseId;
                if (idx >= 0 && idx < (int) mAddMenuChoices.size() && onAddEngineRequest)
                    onAddEngineRequest (mAddMenuChoices[(size_t) idx].type,
                                        mAddMenuChoices[(size_t) idx].engine);
            }
            else if (result == -4)
            {
                // J-6 (2026-05-03): + Add BaySickRustyDrums (Drums dropdown only).
                if (onAddBaySickRustyDrumsRequest) onAddBaySickRustyDrumsRequest();
            }
            else if (result == -5)
            {
                // K-4 (2026-05-05): + Add BaySickGuitars (Inst dropdown only).
                if (onAddBaySickGuitarsRequest) onAddBaySickGuitarsRequest();
            }
            else if (result == -6)
            {
                // L-3 (2026-05-05): + Add BaySickBasses (Inst dropdown only).
                if (onAddBaySickBassesRequest) onAddBaySickBassesRequest();
            }
            else if (result >= kVoxExportBaseId
                     && result < kVoxExportBaseId + (int) mVoxExportShown.size())
            {
                // QA-Fa recovery: + Add New Vox From Export pick.
                if (onAddVoxFromExport)
                    onAddVoxFromExport (
                        mVoxExportShown[(size_t)(result - kVoxExportBaseId)].fullPath);
            }
            else if (result <= -10 && result > -99)
            {
                // QA-Layout T4: window-list row.  Navigate to the instance
                // first (the row's window belongs to it), then let the editor
                // act on the row -- it rebuilds the row model at pick time so
                // the index cannot go stale.
                int tabId = getActiveTabForType(type);
                if (tabId >= 0)
                {
                    mSelectedId = tabId;
                    if (auto* tab = getTabById(tabId))
                        mLastUsedByType[tab->type] = tabId;
                    repaint();
                    if (onTabSelected) onTabSelected(tabId);
                }
                if (onPageWindowRowPicked) onPageWindowRowPicked (activeId, -10 - result);
            }
            else
            {
                // Instance selected - switch to it
                mSelectedId = result;
                if (auto* tab = getTabById(result))
                    mLastUsedByType[tab->type] = result;
                repaint();
                if (onTabSelected) onTabSelected(result);
            }
        });
}

// ── Rename ───────────────────────────────────────────────────────────────────
void RibbonTabBar::startRename(int tabId)
{
    // D1.4-fix: let editor intercept (e.g. Drum "User Patch" → savePatchAs).
    if (onRenameInterceptRequested && onRenameInterceptRequested (tabId))
        return;

    for (auto& t : mTabs)
    {
        if (t.id != tabId) continue;

        auto* aw = new juce::AlertWindow(
            "Rename", "Enter a new name:",
            juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor("name", t.name);
        aw->addButton("OK", 1);
        aw->addButton("Cancel", 0);
        aw->enterModalState(true,
            juce::ModalCallbackFunction::create(
                [this, tabId, aw](int result) {
                    if (result == 1)
                    {
                        auto newName = aw->getTextEditorContents("name");
                        if (newName.isNotEmpty())
                        {
                            for (auto& tab : mTabs)
                            {
                                if (tab.id == tabId)
                                {
                                    tab.name = newName;
                                    if (onTabRenamed) onTabRenamed(tabId, newName);
                                    repaint();
                                    break;
                                }
                            }
                        }
                    }
                }),
            true);
        return;
    }
}

// ── Paint ────────────────────────────────────────────────────────────────────
void RibbonTabBar::paint(juce::Graphics& g)
{
    // No background fill - the parent transport bar's brushed-aluminum shows through.

    const int nSlots = numSlots();
    for (int s = 0; s < nSlots; ++s)
    {
        // QA-ModelShell TS4: the trailing slot is the "+" add button, not a tab.
        if (isAddSlot (s))
        {
            const auto r = slotRect (s);
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.fillRect (r.reduced (2));
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.setFont (juce::Font (18.0f, juce::Font::bold));
            g.drawText ("+", r, juce::Justification::centred);
            continue;
        }
        TabType type = slotType(s);
        bool    sel  = isSlotSelected(s);
        auto    r    = slotRect(s);

        // ── Tab background (rounded top corners) ─────────────────────────────
        juce::Path tabPath;
        float cornerR = 5.0f;
        tabPath.addRoundedRectangle(
            (float)r.getX(), (float)r.getY(),
            (float)r.getWidth(), (float)r.getHeight() + 2.0f,
            cornerR, cornerR, true, true, false, false);

        // G-8 (2026-04-29): Builder slot uses an 8-color tie-dye gradient
        // matching the mixer-strip palette in mixer order
        // (Master / FX / Clips / Vox / Inst / Layers / Bass / Drums) so
        // the Builder tab visually echoes the channels it arranges.  All
        // other slots keep their flat single-colour fill.
        if (type == TabType::Builder)
        {
            // Sourced from MixerPage.cpp's actual strip-construction colors:
            //   Master  = kMixerTabPurple    (0xff7b2fbe)
            //   FX      = kEffectsTabPink    (0xffce3f8e)
            //   Clips   = VC::Warm           (0xffd4a017)
            //   Vox     = teal               (0xff0fafa5)
            //   Inst    = navy               (0xff1c3a8a)
            //   Layers  = VC::LayerCol[0]    (0xffff8833)
            //   Bass    = VC::BassCol[0]     (0xff33ff88)
            //   Drums   = VC::DrumsCol       (0xffff4444)
            const juce::Colour stops[8] = {
                juce::Colour (0xff7b2fbe),   // Master (purple)
                juce::Colour (0xffce3f8e),   // FX (pink)
                juce::Colour (0xffd4a017),   // Clips (gold)
                juce::Colour (0xff0fafa5),   // Vox (teal)
                juce::Colour (0xff1c3a8a),   // Inst (navy)
                juce::Colour (0xffff8833),   // Layers (orange)
                juce::Colour (0xff33ff88),   // Bass (neon green)
                juce::Colour (0xffff4444)    // Drums (red)
            };
            const float bright = sel ? 1.0f : 0.55f;

            juce::ColourGradient grad (stops[0].withMultipliedBrightness (bright),
                                         (float) r.getX(),     (float) r.getCentreY(),
                                         stops[7].withMultipliedBrightness (bright),
                                         (float) r.getRight(), (float) r.getCentreY(),
                                         false);
            for (int i = 1; i < 7; ++i)
                grad.addColour ((double) i / 7.0,
                                 stops[i].withMultipliedBrightness (bright));
            g.setGradientFill (grad);
            g.fillPath (tabPath);
        }
        else
        {
            g.setColour(tabColour(type, sel));
            g.fillPath(tabPath);
        }

        // Active tab: bright top stripe
        if (sel)
        {
            g.setColour(VC::Highlight);
            g.fillRect(r.getX() + 1, r.getY(), r.getWidth() - 2, 2);
        }

        int badge = getBadgeCount(type);

        // ── L25 (QA-Layout): two-row slot ────────────────────────────────────
        // Name across the top row at full slot width; badge + arrow on the
        // bottom row.  drawFittedText's 0.75f minScale shrinks a long label
        // before anything clips.
        const juce::Rectangle<int> nameR   = r.withHeight (kNameRowH);
        const juce::Rectangle<int> bottomR = r.withTrimmedTop (kNameRowH);

        g.setColour(juce::Colours::white.withAlpha(sel ? 1.0f : 0.75f));
        g.setFont(juce::Font(12.0f, sel ? juce::Font::bold : 0));
        g.drawFittedText (getSlotDisplayName(s), nameR.reduced (8, 0),
                          juce::Justification::centredLeft, 1, 0.75f);

        // ── Badge circle (bottom row, left of the arrow) ─────────────────────
        if (badge > 0)
        {
            int bx = r.getRight() - (hasDropdown(type) ? kArrowW : 0) - kBadgeR * 2 - 2;
            int by = bottomR.getY() + (bottomR.getHeight() - kBadgeR * 2) / 2;

            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.fillEllipse((float)bx, (float)by,
                          (float)(kBadgeR * 2), (float)(kBadgeR * 2));

            g.setColour(juce::Colour(0xff1a1a2e));
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText(juce::String(badge),
                       juce::Rectangle<int>(bx, by, kBadgeR * 2, kBadgeR * 2),
                       juce::Justification::centred);
        }

        // ── TS7 §6.4: frozen indicator ───────────────────────────────────────
        // A small mark on the tab's left edge (bottom row, clear of the name),
        // drawn rather than a glyph: a snowflake character renders as a box on
        // a machine without the font, and this is the only signal that a tab
        // is playing cached audio.
        //
        // Two states, because "frozen" and "frozen but out of date" mean
        // different things to the user: solid = playing its file, hollow = stale
        // and currently back on the live engine while it re-renders.
        if (onIsTabFrozen)
        {
            const int frozenState = onIsTabFrozen (type);   // 0 none, 1 frozen, 2 stale
            if (frozenState > 0)
            {
                const float d  = 7.0f;
                const float fx = (float) r.getX() + 4.0f;
                const float fy = (float) bottomR.getY()
                               + ((float) bottomR.getHeight() - d) * 0.5f;
                const juce::Colour cyan (0xff00fff2);
                if (frozenState == 1)
                {
                    g.setColour (cyan.withAlpha (0.95f));
                    g.fillEllipse (fx, fy, d, d);
                }
                else
                {
                    g.setColour (cyan.withAlpha (0.75f));
                    g.drawEllipse (fx, fy, d, d, 1.4f);
                }
            }
        }

        // ── ▾ arrow (bottom row, rightmost region) ───────────────────────────
        if (hasDropdown(type))
        {
            auto arrowR = bottomR.withTrimmedLeft(bottomR.getWidth() - kArrowW);
            g.setColour(juce::Colours::white.withAlpha(sel ? 0.9f : 0.55f));
            g.setFont(juce::Font(16.0f));
            g.drawText(juce::CharPointer_UTF8("\xe2\x96\xbe"), arrowR,
                       juce::Justification::centred);   // ▾
        }

        // ── Separator between slots ──────────────────────────────────────────
        if (s + 1 < nSlots)
        {
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            g.fillRect(r.getRight() - 1, r.getY() + 4, 1, r.getHeight() - 8);
        }
    }
}
