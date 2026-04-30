#pragma once
#include <JuceHeader.h>

// ── RibbonTabBar ──────────────────────────────────────────────────────────────
// Fixed 10-slot tab bar. Each slot represents a page type:
//
//   Mixer       — no dropdown, no badge
//   Effects     — dropdown (Rack / EQ), badge ②
//   Builder     — dropdown (Patterns / Audio Clips / Automation), badge ③
//   Clip        — dropdown (instance list + sub-pages + rename/delete; NO add).
//                 Instances are ONLY spawned via drag/drop or upload of audio
//                 onto Builder — the ribbon dropdown can't create new ones.
//                 Phase G-2 (2026-04-28).
//   Vox         — dropdown (instance list + sub-pages + rename/delete; NO add).
//                 Instances are ONLY spawned via the "Add Vox Strip" button on
//                 the Mixer page.  Phase G-4 (2026-04-28).
//   Inst        — dropdown (instance list + sub-pages + rename/delete; NO add).
//                 Instances are ONLY spawned via the "Add Inst Strip" button
//                 on the Mixer page.  Phase G-4 (2026-04-28).
//   Layers      — dropdown (instance list + rename/delete/add), dynamic badge
//   Bass        — dropdown (instance list + rename/delete/add), dynamic badge
//   Drums       — dropdown (Sounds / EQ), badge ②
//   PianoRoll   — unified piano-roll page (Drum Kit + every engine's roll),
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
    std::function<void(int tabId, const juce::String& newName)> onTabRenamed;
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

    // ── API ──────────────────────────────────────────────────────────────────
    int  addTab(TabType type, const juce::String& name);
    void closeTab(int tabId);
    void selectTab(int tabId);

    // Project-load / File > New helper (2026-04-24): wipes every Layers /
    // Bass / Drums entry unconditionally, bypassing closeTab's count /
    // type guards.  closeTab refuses to remove Drums or the last instance
    // of a type, which is the right UX for user clicks but wrong for
    // project load (where we need to clear defaults before restoring
    // saved tabs).  StandaloneEditor calls this then follows up with
    // addTab for each saved / default record.
    void clearAllDynamicTabs();
    // Batch 5 (2026-04-25): same as clearAllDynamicTabs but only for one
    // type — used by Load Kit to wipe Drums without disturbing Layers/Bass.
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
    const Tab* getTabById(int id) const;

    // Returns true if the tab type shows a ▾ arrow
    static bool hasDropdown(TabType type);

    // D1.4-fix: needed by editor's onSubPageSelected dispatch (active page lookup).
    int  getActiveTabForType(TabType type) const;

    // D1.4-fix (c): true when there's only one tab of `type` left.  Used by
    // the editor to refuse delete-the-last-instance with a friendly notice.
    bool isLastOfType(TabType type) const { return countTabsOfType(type) <= 1; }

private:
    static constexpr int kNumSlots = 10;  // 2026-04-28: +Vox +Inst (G-4)
    static constexpr int kTabH     = 30;
    static constexpr int kArrowW   = 22;   // hit-test width for ▾ region
    static constexpr int kBadgeR   = 8;    // badge circle radius

    // Fixed slot order
    static TabType slotType(int slotIndex);

    // Layout helpers
    juce::Rectangle<int> slotRect(int slotIndex) const;
    int hitTestSlot(juce::Point<int> pos, bool& hitArrow) const;

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

    static juce::Colour tabColour(TabType type, bool active);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RibbonTabBar)
};
