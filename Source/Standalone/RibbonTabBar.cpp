#include "RibbonTabBar.h"
#include "SharedUI.h"

namespace
{
// QA-A Phase 5 / STYLE-01 (2026-05-09): mid-string camelCase splitter for
// the wrap renderer.  JUCE's `drawFittedText` with `maxLines > 1` only
// breaks at spaces / hyphens; brand names like "BaySickRustyDrums" or
// "BaySickPlayer" have neither, so the engine treats them as a single
// unbreakable word and just shrinks instead of wrapping.  This helper
// finds the capital letter (after the first character) closest to the
// midpoint of the string and inserts a newline there, giving the engine
// a hard break point.  When no capital exists past position 0 (e.g. the
// user typed "supercalifragilistic"), it returns the original string and
// drawFittedText falls back to its single-line shrink behaviour.
juce::String splitCamelCase (const juce::String& s)
{
    if (s.length() < 4) return s;
    const int mid = s.length() / 2;
    int bestCap     = -1;
    int bestDelta   = std::numeric_limits<int>::max();
    for (int i = 1; i < s.length(); ++i)
    {
        if (juce::CharacterFunctions::isUpperCase (s[i]))
        {
            const int d = std::abs (i - mid);
            if (d < bestDelta) { bestDelta = d; bestCap = i; }
        }
    }
    if (bestCap < 0) return s;
    return s.substring (0, bestCap) + "\n" + s.substring (bestCap);
}
} // namespace

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
    // G-8 ordering preserved: Builder leftmost, then Mixer / Effects, the
    // instance types in their historical order, Piano Roll last.
    static constexpr TabType order[] = {
        TabType::Builder, TabType::Mixer, TabType::Effects,
        TabType::Clip, TabType::Vox, TabType::Inst,
        TabType::Layers, TabType::Bass, TabType::Drums,
        TabType::PianoRoll
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
    // 2026-04-26: PianoRoll fixed slot.  Layers/Bass/Drums dynamic instances
    // are inserted between Builder and PianoRoll by StandaloneEditor's
    // create...Page() calls - addTab() pushes them at the end of mTabs but the
    // ribbon's slot ordering uses slotType() and a per-type index lookup,
    // not raw mTabs order.  Adding PianoRoll here keeps the slot fixed at
    // index 6 regardless of how many dynamic tabs exist.
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
         || t == TabType::Clip   || t == TabType::Vox  || t == TabType::Inst)
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
         && type != TabType::Vox    && type != TabType::Inst) return;

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

// Pure measurement: bold-font text width + arrow + badge + 16 px padding.
// No clamping to min/max here -- callers (slotRect / slotWraps) apply the
// per-slot floor and the wrap cap themselves.  Bold (the active-state
// font) is the worst case so a slot never changes width when clicked
// between active and inactive.
int RibbonTabBar::naturalSingleLineWidth (int slotIndex) const
{
    const auto       type = slotType(slotIndex);
    const auto       name = getSlotDisplayName(slotIndex);
    const juce::Font font (12.0f, juce::Font::bold);
    int w = font.getStringWidth(name) + 16;
    if (hasDropdown(type))         w += kArrowW;
    if (getBadgeCount(type) > 0)   w += kBadgeR * 2 + 4;
    return w;
}

bool RibbonTabBar::slotWraps (int slotIndex) const
{
    return naturalSingleLineWidth(slotIndex) > kMaxSingleLine;
}

// QA-A Phase 5 / STYLE-01 (2026-05-09): constraint-based variable-width
// layout with min floors and a max single-line cap that triggers wrap.
//   1. desired_i = clamp(natural_i, minW_i, kMaxSingleLine).
//      -- natural above kMaxSingleLine -> slot caps at the max and the
//         label will paint as wrapped two-line text in paint().
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
juce::Rectangle<int> RibbonTabBar::slotRect (int slotIndex) const
{
    const int totalW = getWidth();
    const int totalH = getHeight() > 0 ? getHeight() : kTabH;

    const int nSlots = numSlots();
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
            if (! isAddSlot (s) && hasDropdown(slotType(s)))
            {
                auto arrowR = r.withTrimmedLeft(r.getWidth() - kArrowW);
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
    struct Target { const char* label; TabType type; };

    // (engine, where it can live) -- taken from each page's own engine list:
    // LayersPage offers Harmless / BaySickPlayer / BaySickSynth, BassPage offers
    // Harmless / BaySickPlayer / BaySickBass, DrumPage's picker is sample
    // (BaySickPlayer) or synth patch (BaySickSynth), Clips are BaySickPlayer.
    struct EngineRow { const char* engine; std::vector<Target> targets; bool enabled { true }; };

    // Inst is a SHARED cap: BaySickGuitars, BaySickBasses and the live
    // instrument chain all consume Inst slots, so the same gate covers all
    // three.  Computed here because the table below needs it.
    const bool instCapped = onIsInstCapReached && onIsInstCapReached();

    const std::vector<EngineRow> kEngines = {
        { "Harmless",       { { "Layers", TabType::Layers }, { "Bass", TabType::Bass } } },
        { "BaySickPlayer",  { { "Layers", TabType::Layers }, { "Bass", TabType::Bass },
                              { "Drums",  TabType::Drums  }, { "Audio Clips", TabType::Clip } } },
        { "BaySickSynth",   { { "Layers", TabType::Layers }, { "Drums", TabType::Drums } } },
        { "BaySickBass",    { { "Bass",  TabType::Bass } } },
        { "BaySickVocal",   { { "Vox",   TabType::Vox  } } },
        // Jeff 2026-07-28: the LIVE instrument route was missing entirely.  The
        // menu offered the two sfizz Inst engines but no way to make a plain
        // live-input Inst tab, which is the direct counterpart of BaySickVocal
        // -> Vox.  Named for its engine like every other row.
        { "BaySickPedals",  { { "Inst",  TabType::Inst } }, ! instCapped },
    };

    juce::PopupMenu m;
    mAddMenuChoices.clear();

    for (const auto& row : kEngines)
    {
        if (row.targets.size() == 1)
        {
            const int id = kAddEngineBaseId + (int) mAddMenuChoices.size();
            mAddMenuChoices.push_back ({ row.targets[0].type, row.engine });
            m.addItem (id, row.engine, row.enabled);
        }
        else
        {
            juce::PopupMenu sub;
            for (const auto& t : row.targets)
            {
                const int id = kAddEngineBaseId + (int) mAddMenuChoices.size();
                mAddMenuChoices.push_back ({ t.type, row.engine });
                sub.addItem (id, t.label);
            }
            m.addSubMenu (row.engine, sub);
        }
    }

    // The sfizz engines keep their own spawn routes (kit load + strip cascade),
    // so they fire their dedicated callbacks rather than the generic one.
    m.addItem (1, "BaySickGuitars", ! instCapped);
    m.addItem (2, "BaySickBasses",  ! instCapped);
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

        // Sub-page navigation - opens the active instance and switches to that sub-tab
        // Negative IDs reserved for menu actions: -1/-2/-3 (rename/delete/add) and
        // -10..-13 (sub-page items) so we can disambiguate from instance IDs.
        // Drums has 4 sub-tabs (Drum Kit added in D2); Layers/Bass have 3.
        // Note: PopupMenu has no addSectionHeading; use a disabled "Pages:" item.
        m.addItem(-99, "Pages:", false /* enabled */, false);
        // J-6 EQ unification (2026-05-03): EQ sub-page item removed from
        // Drums/Layers/Bass/Clip dropdowns - pre + post EQ for every strip
        // now live exclusively on the Effects page.  Vox/Inst keep "EQ"
        // because BaySickVocalEditor still hosts the Pre Rack EQ as one of
        // its internal tabs (deferred clean-up).
        if (type == TabType::Drums)
        {
            m.addItem(-10, "  Drum Kit");
            m.addItem(-11, "  Player");
            m.addItem(-12, "  Piano Roll");
        }
        else if (type == TabType::Vox || type == TabType::Inst)
        {
            m.addItem(-10, "  Player");
            m.addItem(-11, "  EQ");
        }
        else
        {
            m.addItem(-10, "  Player");
            m.addItem(-11, "  Piano Roll");
        }

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

    // G-6 (2026-04-29): all multi-instance types now have a +Add entry,
    // including Clip.  Clip's +Add opens an OS file picker (handled by the
    // editor's onAddTabRequest closure) so the user can add a clip without
    // dragging from elsewhere.
    juce::String addLabel = (type == TabType::Layers) ? "+ Add New Layers"
                          : (type == TabType::Bass)   ? "+ Add New Bass"
                          : (type == TabType::Vox)    ? "+ Add New Vox"
                          : (type == TabType::Inst)   ? "+ Add New Inst"
                          : (type == TabType::Clip)   ? "+ Add New Clip..."
                          :                             "+ Add New Drum";
    m.addItem(-3, addLabel);

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
                // Add new instance
                if (onAddTabRequest) onAddTabRequest(type);
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
            else if (result <= -10 && result >= (type == TabType::Drums ? -12
                                                : (type == TabType::Vox || type == TabType::Inst) ? -11
                                                : -11))
            {
                // Sub-page item: open the active instance, then notify the
                // editor to switch its sub-tab.
                // Drums (D2): -10=DrumKit(0), -11=Player(1), -12=PianoRoll(2), -13=EQ(3).
                // Vox/Inst (G-4): -10=Player(0), -11=EQ(1) - no Piano Roll.
                // Layers/Bass/Clip: -10=Player(0), -11=PianoRoll(1), -12=EQ(2).
                int tabId = getActiveTabForType(type);
                if (tabId >= 0)
                {
                    mSelectedId = tabId;
                    if (auto* tab = getTabById(tabId))
                        mLastUsedByType[tab->type] = tabId;
                    repaint();
                    if (onTabSelected) onTabSelected(tabId);
                }
                if (onSubPageSelected) onSubPageSelected(type, -10 - result);
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

        // ── Compute badge count first (affects text area) ─────────────────
        int badge = getBadgeCount(type);

        // ── Tab text ─────────────────────────────────────────────────────────
        // Layout from right edge: [arrow 24px] [badge 20px if present] [text fills rest]
        juce::Rectangle<int> textR = r;
        if (hasDropdown(type))
            textR = textR.withTrimmedRight(kArrowW);
        if (badge > 0)
            textR = textR.withTrimmedRight(kBadgeR * 2 + 4);

        juce::String name = getSlotDisplayName(s);
        g.setColour(juce::Colours::white.withAlpha(sel ? 1.0f : 0.75f));
        g.setFont(juce::Font(12.0f, sel ? juce::Font::bold : 0));
        // QA-A Phase 5 / STYLE-01 (2026-05-09): single-line by default; if the
        // slot's natural width exceeds kMaxSingleLine the layout caps the
        // slot's allocated width and we render the label wrapped to two
        // lines.  JUCE's `drawFittedText` word-wrap breaks at spaces /
        // hyphens, so user-typed names with spaces wrap on their own.  For
        // brand names without spaces ("BaySickRustyDrums" / "BaySickPlayer"),
        // splitCamelCase() in the anonymous namespace at the top of this
        // file injects a hard newline at the capital letter closest to the
        // string midpoint so the text still wraps cleanly.  drawFittedText's
        // 0.75f minScaleFactor handles the residual narrow-window case
        // where even the wrapped two lines need a slight shrink.
        const bool wraps = slotWraps(s);
        const juce::String renderName =
            (wraps && ! name.containsAnyOf (" -")) ? splitCamelCase (name) : name;
        const int maxLines = wraps ? 2 : 1;
        g.drawFittedText (renderName,
                          textR.reduced (8, 0),
                          juce::Justification::centredLeft,
                          maxLines, 0.75f);

        // ── Badge circle (between text and arrow) ────────────────────────────
        if (badge > 0)
        {
            int bx = r.getRight() - kArrowW - kBadgeR * 2 - 2;
            int by = r.getY() + (kTabH - kBadgeR * 2) / 2;

            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.fillEllipse((float)bx, (float)by,
                          (float)(kBadgeR * 2), (float)(kBadgeR * 2));

            g.setColour(juce::Colour(0xff1a1a2e));
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText(juce::String(badge),
                       juce::Rectangle<int>(bx, by, kBadgeR * 2, kBadgeR * 2),
                       juce::Justification::centred);
        }

        // ── ▾ arrow (rightmost region) ───────────────────────────────────────
        if (hasDropdown(type))
        {
            auto arrowR = r.withTrimmedLeft(r.getWidth() - kArrowW)
                           .withSizeKeepingCentre(kArrowW, kTabH);
            g.setColour(juce::Colours::white.withAlpha(sel ? 0.9f : 0.55f));
            g.setFont(juce::Font(26.0f));
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
