#pragma once
#include <JuceHeader.h>
#include <vector>

// ── RibbonTabBar ──────────────────────────────────────────────────────────────
// One slot per page type.  The four required app surfaces (Builder / Mixer /
// Effects / Piano Roll) are always shown; instance types (Clip / Vox / Inst /
// Layers / Bass / Drums / Plugins) appear only while they have >= 1 tab and
// return through the trailing "+" slot (QA-ModelShell TS4).
//
// A type slot's dropdown carries: instance list, sub-page nav, rename/delete,
// and engine-named add rows at the bottom -- each names a player this type
// can load and spawns the tab with that player already loaded (QA-Layout T2;
// the engine pickers that used to live on the pages are gone per L4).
//
// No overflow, no close X.
// ─────────────────────────────────────────────────────────────────────────────

class RibbonTabBar : public juce::Component
{
public:
    // QA-ModelShell TS6 (BLU-447): Plugins = hosted VST3 instrument tabs.
    // Appended -- TabType's integer values are persisted in project state.
    enum class TabType { Mixer, Effects, Builder, Clip, Vox, Inst, Layers, Bass, Drums, PianoRoll, Plugins };

    struct Tab
    {
        int          id;
        TabType      type;
        juce::String name;
        bool         locked { false };  // D2: when true, ribbon shows "[L] " prefix and
                                        // refuses Delete from the dropdown.
    };

    RibbonTabBar();

    void paint(juce::Graphics&) override;
    void resized() override {}
    void mouseDown(const juce::MouseEvent&) override;

    // ── Callbacks set by StandaloneEditor ────────────────────────────────────
    std::function<void(int tabId)>                    onTabSelected;
    std::function<void(int tabId)>                    onTabClosed;       // Layers/Bass delete
    std::function<void(TabType)>                      onAddTabRequest;   // Layers/Bass add
    // QA-ModelShell TS4 (Jeff spec 2026-07-28): the "+" menu lists ENGINES, not
    // page types -- the engine you pick decides which tab it lands in.  Engines
    // that can live in more than one tab (BaySickPlayer, Harmless, BaySickSynth)
    // get a side submenu to choose.  This fires with the resolved pair.
    std::function<void(TabType, const juce::String& engineName)> onAddEngineRequest;
    std::function<void(TabType, int subPageIndex)>    onSubPageSelected; // Effects/Builder
    // QA-Layout T4 (L11/D4=c): the instance dropdowns' "Pages:" section is a
    // per-instance WINDOW list built by the editor (incl. the Pre/Post EQ
    // rows).  Labels come from onListPageWindowRows; a pick navigates to the
    // tab first, then routes back by row index.  The editor rebuilds the row
    // model at pick time, so the indices cannot go stale against a page that
    // changed while the menu was open.
    std::function<juce::StringArray(int tabId)>       onListPageWindowRows;
    std::function<void(int tabId, int rowIdx)>        onPageWindowRowPicked;
    // J-6 (2026-05-03): "+ Add BaySickRustyDrums" entry in the Drums dropdown.
    // Singleton - fires only when no instance currently exists.
    std::function<void()>                             onAddBaySickRustyDrumsRequest;
    // J-6: 1-instance lock query - set by StandaloneEditor; the dropdown
    // hides "+ Add BaySickRustyDrums" when this returns true.
    std::function<bool()>                             onIsBaySickRustyDrumsActive;
    // K-4 (2026-05-05): "+ Add BaySickGuitars" entry in the Inst dropdown.
    // Multi-instance; fires when the user picks the entry.  Cap is shared
    // with classic LiveInput Inst pages + BaySickBasses pages - total
    // ≤ kMaxInstPages.  RibbonTabBar greys the entry when onIsInstCapReached()
    // returns true.
    std::function<void()>                             onAddBaySickGuitarsRequest;
    // L-3 (2026-05-05): "+ Add BaySickBasses" entry in the Inst dropdown.
    // Same cap-shared semantics as Guitars.
    std::function<void()>                             onAddBaySickBassesRequest;
    // K-4: shared cap query - true when total Inst-type pages (LiveInput +
    // BaySickGuitars + BaySickBasses) hits kMaxInstPages.
    std::function<bool()>                             onIsInstCapReached;
    // TS7 §6.4: 0 = no frozen tab of this type, 1 = frozen, 2 = frozen but stale
    // (playing live while it re-renders).  Returns the strongest state across the
    // type's instances, because the ribbon shows ONE slot per type.
    std::function<int(TabType)>                       onIsTabFrozen;
    // QA-Fa recovery: "+ Add New Vox From Export" submenu (Vox dropdown
    // only).  The editor returns the Aligned/ + Pitched/ export list --
    // EMPTY when any grey rule holds (no exports / vox cap reached /
    // project unsaved) -- and receives the picked file's full path.
    struct VoxExportEntry
    {
        juce::String folder;     // "Aligned" / "Pitched" (submenu grouping)
        juce::String name;       // display file name
        juce::String fullPath;
    };
    std::function<std::vector<VoxExportEntry>()>      onListVoxExports;
    std::function<void(const juce::String& fullPath)> onAddVoxFromExport;
    std::function<void(int tabId, const juce::String& newName)> onTabRenamed;
    // 2026-05-05 dirty-flag wiring: fired when a tab's lock state actually
    // toggles (no fire on no-op set).  Editor wires to ProjectManager::markDirty.
    std::function<void(int tabId, bool locked)>                  onTabLockChanged;
    // D1.4-fix: editor-side rename intercept.  Return true to suppress the
    // ribbon's default rename dialog (editor handles it).  Used for Drum
    // tabs whose name == "User Patch" → re-routed to Save Patch As.
    std::function<bool(int tabId)>                    onRenameInterceptRequested;
    // D2: dropdown Delete now routes through the page's own requestDelete()
    // so Drums/Layers/Bass all get the same Save & Delete / Delete Anyway /
    // Cancel flow regardless of whether deletion was initiated from the
    // ribbon dropdown or the per-page right-click context menu.  The
    // dropdown only refuses if the tab is locked (with a "Cannot Delete"
    // message); otherwise it fires this so StandaloneEditor can dispatch.
    std::function<void(int tabId)>                    onTabDeleteRequested;
    // QA-ModelShell TS4 (2026-07-28): the six on*EmptyStateRequested callbacks
    // are gone.  They existed so a click on a ZERO-INSTANCE type slot could
    // show a placeholder page -- and a type slot is no longer even drawn at
    // zero instances (visibleSlotTypes), so the click they answered cannot
    // happen.  Adding an instance is the "+" slot's job now.

    // ── API ──────────────────────────────────────────────────────────────────
    int  addTab(TabType type, const juce::String& name);
    void closeTab(int tabId);
    void selectTab(int tabId);

    // Project-load / File > New helper (2026-04-24): wipes every Layers /
    // Bass / Drums entry in one pass, including types closeTab would refuse
    // outright (system tabs).  StandaloneEditor calls this then follows up
    // with addTab for each saved record.
    void clearAllDynamicTabs();
    // Batch 5 (2026-04-25): same as clearAllDynamicTabs but only for one
    // type - used by Load Kit to wipe Drums without disturbing Layers/Bass.
    void clearTabsOfType (TabType type);
    void renameTab(int tabId, const juce::String& newName);  // programmatic rename (no dialog)
    // D2: opens the rename AlertWindow on the given tab (same dialog the
    // dropdown shows).  Public so per-page right-click menus can route to
    // it without duplicating the dialog logic.
    void startRename(int tabId);

    // D2: lock state mirroring.  When a Layer/Bass/Drum page's lock toggle
    // changes, StandaloneEditor calls this so the ribbon can repaint with
    // the "[L] " prefix and the dropdown's Delete item gets gated.
    void setTabLocked (int tabId, bool locked);
    bool isTabLocked  (int tabId) const;

    // D2 Batch 4: move the N-th tab of `type` to position M (within the type).
    // Used by the kit-tab drag-reorder.  StandaloneEditor mirrors the same
    // move on its own mPages list so the kit view + dropdown order stay in
    // sync.  No-op on out-of-range or src==dst.
    void moveTabOfType (TabType type, int srcRowOfType, int dstRowOfType);

    int  getSelectedTabId() const { return mSelectedId; }
    int  getTabCount()      const { return (int)mTabs.size(); }
    // QA-G (Split by Player Engine): display name of a tab by id ({} if gone).
    juce::String getTabName (int tabId) const
    {
        for (const auto& t : mTabs)
            if (t.id == tabId) return t.name;
        return {};
    }
    const Tab* getTabById(int id) const;

    // Returns true if the tab type shows a ▾ arrow
    static bool hasDropdown(TabType type);

    // D1.4-fix: needed by editor's onSubPageSelected dispatch (active page lookup).
    int  getActiveTabForType(TabType type) const;

    // D1.4-fix (c): true when there's only one tab of `type` left.  Used by
    // the editor to refuse delete-the-last-instance with a friendly notice.
    // isLastOfType retired 2026-07-26 (QA-ProjectSave docket 18) -- its only
    // purpose was the >= 1 floor on Layers / Bass / Drums, which is gone.

private:
    // Upper bound only (11 types + the "+" slot) -- used to size stack arrays
    // in the width solver.  The LIVE count is numSlots().  QA-Layout T1: was
    // 11, which TS6's Plugins type silently outgrew -- with every type visible
    // the solver's stack arrays overflowed by one.
    static constexpr int kMaxSlots = 12;
    // QA-A Phase 5 (2026-05-09): kTabH bumped 30 -> 40 so each tab fills the
    // full vertical height of the parent transport bar (kBarH = 40 in
    // StandaloneEditor::resized).  Eliminates the empty horizontal strip
    // that previously sat below all tabs.
    static constexpr int kTabH     = 40;
    // QA-Layout T1 (L25): two-row slots.  Name on the top row (kNameRowH);
    // badge + dropdown arrow on the remaining bottom row.  Shared by paint()
    // and hitTestSlot() so the drawn arrow and its hit zone stay one rect.
    static constexpr int kNameRowH = 22;
    static constexpr int kArrowW   = 22;   // hit-test width for ▾ region
    static constexpr int kBadgeR   = 8;    // badge circle radius

    // QA-A Phase 5 / STYLE-01 (2026-05-09): variable-width slot constraints.
    //   kMinFixed     -- width floor for fixed-label slots (Mixer / Effects /
    //                     Builder / Piano Roll).  Short labels can shrink
    //                     down to this (they never go below 60 px so the tab
    //                     remains readable in narrow windows).
    //   kMinVariable  -- width floor for variable-label slots (Clip / Vox /
    //                     Inst / Layers / Bass / Drums / Plugins).  Higher
    //                     than the fixed floor so a few characters of the
    //                     active label always survive.
    //   kMaxSingleLine -- hard width cap; a slot never grows past this.
    //                     Longer labels shrink via drawFittedText's minScale
    //                     (QA-Layout T1 retired the two-line wrap).
    static constexpr int kMinFixed      = 60;
    static constexpr int kMinVariable   = 80;
    static constexpr int kMaxSingleLine = 220;

    // QA-ModelShell TS4: the bar is no longer a fixed 10-slot strip.  The four
    // REQUIRED tabs (Builder / Mixer / Effects / Piano Roll) are always shown;
    // the six instance types appear only while they have >= 1 instance and
    // vanish at zero, returning through the trailing "+" slot.  Slot indices
    // are therefore runtime, not a compile-time map.
    std::vector<TabType> visibleSlotTypes() const;
    // Visible type slots + 1 for the trailing "+".
    int  numSlots() const;
    // True when slotIndex addresses the "+" slot rather than a tab type.
    bool isAddSlot (int slotIndex) const;
    TabType slotType(int slotIndex) const;
    static bool isRequiredTab (TabType type);
    // The "+" menu: every add option, including the ones that used to live in
    // per-type dropdowns and the empty-state placeholders.
    void showAddMenu (juce::Rectangle<int> slotBounds);

    // Layout helpers
    // Jeff, 2026-08-04: the "+" is sized to TWICE its own glyph and nothing
    // more.  It used to be laid out as a normal slot -- floored to kMinFixed /
    // kMinVariable and handed an equal share of every leftover pixel -- so on a
    // wide bar it ballooned into a huge empty block while the real tabs stayed
    // narrow.  Reserved first now; the type slots divide what is left.
    int addSlotWidth() const;
    juce::Rectangle<int> slotRect(int slotIndex) const;
    int hitTestSlot(juce::Point<int> pos, bool& hitArrow) const;

    // QA-A Phase 5 / STYLE-01 (2026-05-09): variable-width support.
    // - isFixedNameSlot:        true for Mixer / Effects / Builder /
    //                            PianoRoll (slot label is a constant,
    //                            never reflects user-renamed text).
    // - naturalSingleLineWidth: pixel width the slot would need to display
    //                            its current label single-line at 12pt bold
    //                            plus padding.  Arrow + badge sit on the
    //                            bottom row (L25) and add no width.  Pure
    //                            measurement; no clamping to min/max.
    static bool isFixedNameSlot(TabType type);
    int  naturalSingleLineWidth(int slotIndex) const;

    // Display helpers
    juce::String getSlotDisplayName(int slotIndex) const;
    int  getBadgeCount(TabType type) const;
    int  countTabsOfType(TabType type) const;
    bool isSlotSelected(int slotIndex) const;

    // Dropdown menus
    void showDropdown(int slotIndex);
    void showSubPageDropdown(TabType type, juce::Rectangle<int> tabBounds);
    void showInstanceDropdown(TabType type, juce::Rectangle<int> tabBounds);
    // (startRename moved to public)

    // ── State ────────────────────────────────────────────────────────────────
    juce::Array<Tab> mTabs;
    int              mSelectedId { -1 };
    int              mNextId     { 1 };
    // J-6 (2026-05-03): per-type "last used" tab id.  Updated every time a
    // tab is selected; consulted by getActiveTabForType when the currently
    // selected tab is of a different type, so clicking a ribbon header
    // re-opens the user's last-visited instance of that type instead of
    // always falling back to the first instance.
    std::map<TabType, int> mLastUsedByType;

    // QA-Fa recovery: the export list shown by the last Vox dropdown --
    // submenu ids index into it inside the menu result callback.
    std::vector<VoxExportEntry> mVoxExportShown;
    static constexpr int kVoxExportBaseId = 100000;
    // "+" menu engine choices, rebuilt each time the menu opens.
    struct AddChoice { TabType type; juce::String engine; };
    std::vector<AddChoice> mAddMenuChoices;
    static constexpr int kAddEngineBaseId = 300000;

    static juce::Colour tabColour(TabType type, bool active);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RibbonTabBar)
};
