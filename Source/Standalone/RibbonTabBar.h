#pragma once
#include <JuceHeader.h>
#include <vector>

// ── RibbonTabBar ──────────────────────────────────────────────────────────────
// Fixed 10-slot tab bar. Each slot represents a page type:
//
//   Mixer       - no dropdown, no badge
//   Effects     - dropdown (Rack / EQ), badge ②
//   Builder     - dropdown (Patterns / Audio Clips / Automation), badge ③
//   Clip        - dropdown (instance list + sub-pages + rename/delete; NO add).
//                 Instances are ONLY spawned via drag/drop or upload of audio
//                 onto Builder - the ribbon dropdown can't create new ones.
//                 Phase G-2 (2026-04-28).
//   Vox         - dropdown (instance list + sub-pages + rename/delete; NO add).
//                 Instances are ONLY spawned via the "Add Vox Strip" button on
//                 the Mixer page.  Phase G-4 (2026-04-28).
//   Inst        - dropdown (instance list + sub-pages + rename/delete; NO add).
//                 Instances are ONLY spawned via the "Add Inst Strip" button
//                 on the Mixer page.  Phase G-4 (2026-04-28).
//   Layers      - dropdown (instance list + rename/delete/add), dynamic badge
//   Bass        - dropdown (instance list + rename/delete/add), dynamic badge
//   Drums       - dropdown (Sounds / EQ), badge ②
//   PianoRoll   - unified piano-roll page (Drum Kit + every engine's roll),
//                 black/white piano-key palette; dropdown picks the active
//                 engine (Drum Kit always at top of list)
//
// No + button, no overflow, no close X. All tab management through dropdowns.
// ─────────────────────────────────────────────────────────────────────────────

class RibbonTabBar : public juce::Component
{
public:
    enum class TabType { Mixer, Effects, Builder, Clip, Vox, Inst, Layers, Bass, Drums, PianoRoll };

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
    std::function<void(TabType, int subPageIndex)>    onSubPageSelected; // Effects/Builder/Drums
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
    // G-2 (2026-04-28): fired when user clicks the Clip ribbon body and no
    // Clip instances exist yet.  Editor uses this to show the empty-state
    // placeholder ("drop a clip here..." with FileDragAndDropTarget).
    std::function<void()>                             onClipsEmptyStateRequested;
    // G-4 (2026-04-28): same pattern for Vox + Inst.  Empty states tell the
    // user to click the corresponding "Add Strip" button on the Mixer page.
    std::function<void()>                             onVoxEmptyStateRequested;
    std::function<void()>                             onInstEmptyStateRequested;
    // QA-ProjectSave docket 18 (2026-07-26): Layers / Bass / Drums can now sit
    // at zero instances, so they need the same body-click hook.
    std::function<void()>                             onLayersEmptyStateRequested;
    std::function<void()>                             onBassEmptyStateRequested;
    std::function<void()>                             onDrumsEmptyStateRequested;

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
    static constexpr int kNumSlots = 10;  // 2026-04-28: +Vox +Inst (G-4)
    // QA-A Phase 5 (2026-05-09): kTabH bumped 30 -> 40 so each tab fills the
    // full vertical height of the parent transport bar (kBarH = 40 in
    // StandaloneEditor::resized).  Eliminates the empty horizontal strip
    // that previously sat below all tabs.
    static constexpr int kTabH     = 40;
    static constexpr int kArrowW   = 22;   // hit-test width for ▾ region
    static constexpr int kBadgeR   = 8;    // badge circle radius

    // QA-A Phase 5 / STYLE-01 (2026-05-09): variable-width slot constraints.
    //   kMinFixed     -- width floor for fixed-label slots (Mixer / Effects /
    //                     Builder / Piano Roll).  Short labels can shrink
    //                     down to this (they never go below 60 px so the tab
    //                     remains readable in narrow windows).
    //   kMinVariable  -- width floor for variable-label slots (Clip / Vox /
    //                     Inst / Layers / Bass / Drums).  Higher than the
    //                     fixed floor since these slots need room for arrow
    //                     + badge + a few characters of the active label.
    //   kMaxSingleLine -- width cap above which a slot's label wraps to two
    //                     lines instead of growing the slot further.  Tuned
    //                     so every brand-default name (longest is
    //                     "BaySickRustyDrums" at ~208 px natural) stays
    //                     single-line; only user-renamed long custom labels
    //                     trip the wrap.
    static constexpr int kMinFixed      = 60;
    static constexpr int kMinVariable   = 80;
    static constexpr int kMaxSingleLine = 220;

    // Fixed slot order
    static TabType slotType(int slotIndex);

    // Layout helpers
    juce::Rectangle<int> slotRect(int slotIndex) const;
    int hitTestSlot(juce::Point<int> pos, bool& hitArrow) const;

    // QA-A Phase 5 / STYLE-01 (2026-05-09): variable-width + wrap support.
    // - isFixedNameSlot:        true for Mixer / Effects / Builder /
    //                            PianoRoll (slot label is a constant,
    //                            never reflects user-renamed text).
    // - naturalSingleLineWidth: pixel width the slot would need to display
    //                            its current label single-line at 12pt bold,
    //                            including arrow / badge / padding.  Pure
    //                            measurement; no clamping to min/max.
    // - slotWraps:              true when naturalSingleLineWidth exceeds
    //                            kMaxSingleLine -- paint() then renders the
    //                            label wrapped to two lines via JUCE's word
    //                            wrap or (for camelCase brand names with no
    //                            spaces) a manual mid-string split.
    static bool isFixedNameSlot(TabType type);
    int  naturalSingleLineWidth(int slotIndex) const;
    bool slotWraps(int slotIndex) const;

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

    static juce::Colour tabColour(TabType type, bool active);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RibbonTabBar)
};
