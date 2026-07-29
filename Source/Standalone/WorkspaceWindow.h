#pragma once
#include <JuceHeader.h>
#include <functional>
#include <memory>

class Workspace;
class PageMenuBar;
class VibeTooltip;

// ── WorkspaceWindow — QA-ModelShell TS4 (2026-07-28) ─────────────────────────
// One contained window inside the fixed main frame (locked call 2b: FL-style
// contained workspace, not free-floating documents).
//
// WHY A NATIVE CHILD PEER RATHER THAN A DRAWN COMPONENT.  A hosted VST3 editor
// (TS6) is a foreign HWND, and a foreign HWND always draws ON TOP of everything
// JUCE paints into its parent's client area -- z-order between an OS window and
// a drawn component is not a thing the OS can express.  So a drawn window frame
// would be permanently underneath every plugin editor the user opened.  Making
// OUR windows native children too puts them in the same z-order space as the
// plugin surfaces, which is the only arrangement where "bring this window to
// front" can work for both.
//
// FRAMEWORK CONTRACT (verified against the vendored JUCE, 2026-07-28 --
// juce_Windowing_windows.cpp; there is no other user of this in the tree, so it
// is written down here rather than rediscovered):
//   * addToDesktop (flags, parentHwnd) with a non-null parent sets WS_CHILD on
//     the created window (:2239) and passes the parent to CreateWindowEx
//     (:2437).  It is a real OS child window.
//   * COORDINATE SPACE DIFFERS FROM A TOP-LEVEL PEER, in both directions:
//       - setBounds feeds SetWindowPos, and Windows reads x/y of a WS_CHILD as
//         PARENT-CLIENT relative (:1614);
//       - getBounds returns getWindowClientRect, likewise parent-relative
//         (:1653) -- the screen-relative branch above it is the
//         parentToAddTo == nullptr case only.
//     Set and get therefore agree, and both are in the PARENT WINDOW'S CLIENT
//     space.  Positions here are consequently offset by the workspace's own
//     origin within the main window, NOT by screen coordinates -- see
//     Workspace::originInParentClient.
// ─────────────────────────────────────────────────────────────────────────────
class WorkspaceWindow : public juce::Component
{
public:
    // persistKey identifies this window's saved bounds across runs.  Stable per
    // logical window (e.g. "Mixer", "Layers:2"), NOT per object instance --
    // destroy-on-close means the object is short-lived and the key is the only
    // thing that carries position forward.
    WorkspaceWindow (juce::String persistKey, juce::String title);
    ~WorkspaceWindow() override;

    // Takes ownership of the page component.  Passing nullptr empties the frame.
    void setContent (std::unique_ptr<juce::Component> content);
    // Hosts a page the CALLER still owns.  StandaloneEditor keeps its pages in
    // PageEntry::component and a large amount of existing code reaches through
    // that pointer, so moving ownership in here would have meant rewriting
    // every one of those sites for no gain.  The window lays the page out and
    // draws its frame; it does not free it.
    void setContentNonOwned (juce::Component* content);
    juce::Component* getContent() const noexcept { return mContentRaw; }
    // Hands the page back WITHOUT destroying it -- for a reparent that must not
    // tear the page down.  Destroy-on-close does not use this (it wants the
    // page gone); it exists so a future move-between-frames is possible.
    std::unique_ptr<juce::Component> releaseContent();

    // Resize floor.  Every window gets one at its layout's collision point;
    // fixed-grid panels pass their natural size so they cannot shrink at all.
    void setMinimumSize (int minW, int minH);

    void setTitle (juce::String t);

    // This window's page menu -- the hamburger/menu row that used to be a
    // single shared bar under the transport, merged into the title strip per
    // locked call 4a.  Per-window ON PURPOSE: several windows are visible at
    // once now, so one shared bar could only ever show one of their menus, and
    // re-pointing it on every focus change is exactly the view-coupling TS3
    // spent a whole task set removing.  Never null while the window lives.
    PageMenuBar* getPageMenu() const noexcept { return mPageMenu.get(); }
    const juce::String& getPersistKey() const noexcept { return mPersistKey; }

    // Fired by the close button.  The owner DESTROYS this window in response --
    // the whole point of destroy-on-close -- so nothing may touch the window
    // after invoking it.
    std::function<void()> onCloseRequested;
    // Fired when the user brings this window forward, so the owner can sync tab
    // selection to it.
    std::function<void()> onBroughtToFront;

    // Creates the native child peer parented to `ws` and restores saved bounds
    // (clamped into view).  Call once, after setContent.
    void attachTo (Workspace& ws);

    // Persist current bounds under the persist key.  Called on close and on the
    // owner's teardown, so a window that dies with the app still remembers.
    void saveBounds() const;

    // Order-safe containment: never returns an inverted or degenerate rect, and
    // never shrinks below the constrainer's minimum (a window squeezed under it
    // produced a NEGATIVE content area, which crashed the first slider that
    // tried to paint into it).  Public because Workspace applies the SAME rule
    // on relayout -- one containment definition, two callers.
    juce::Rectangle<int> clampToWorkspace (juce::Rectangle<int> target) const;

    // Soft edge magnetism (Jeff spec 2026-07-28).  While dragging, a window
    // whose edge comes within kSnapPx of another window's opposing edge (or of
    // a workspace edge) is nudged flush so they line up.  Deliberately NOT a
    // lock: the snap only adjusts the position it is given, so continuing to
    // drag pushes straight past it, and windows may still overlap freely.
    juce::Rectangle<int> applyMagnetism (juce::Rectangle<int> target) const;
    static constexpr int kSnapPx = 10;

    // Containment for the RESIZE path, which is a DIFFERENT path from the drag.
    // A drag is clamped in mouseDrag; a resize goes through
    // ResizableBorderComponent -> the bounds constrainer, which knows nothing
    // about the workspace -- so stretching an edge pushed it straight through
    // the frame border (Jeff, 2026-07-28).
    //
    // This TRIMS THE EDGES that went outside instead of sliding the whole window
    // back in the way clampToWorkspace does.  During a resize the user is
    // dragging ONE edge, and moving the opposite edge to compensate is not what
    // that gesture means -- the edge being dragged should simply stop.
    juce::Rectangle<int> clampResizeToWorkspace (juce::Rectangle<int> target) const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;

    static constexpr int kTitleH   = 26;   // custom title strip (locked call 4a)
    static constexpr int kBorderPx = 4;    // resize-drag border thickness

private:
    juce::Rectangle<int> titleBarArea()  const noexcept;
    juce::Rectangle<int> contentArea()   const noexcept;
    // Saved bounds for this key, or an empty rect when the window has never
    // been placed.  Parent-client space, matching the peer contract above.
    juce::Rectangle<int> loadSavedBounds() const;

    juce::String mPersistKey, mTitle;
    // mContent owns only when setContent was used; mContentRaw is what gets
    // laid out either way (and is the non-owning case's only handle).
    std::unique_ptr<juce::Component>                 mContent;
    juce::Component*                                 mContentRaw { nullptr };
    std::unique_ptr<juce::TextButton>                mCloseBtn;
    std::unique_ptr<PageMenuBar>                    mPageMenu;
    // Each contained window needs its OWN tooltip window: juce::TooltipWindow
    // only monitors components inside its own desktop window, and these are
    // separate native windows from the main frame.  Without this the editor's
    // tooltip window stopped covering every page (Jeff, 2026-07-28 -- the
    // knob info tooltips vanished).  Same workaround KeyBindsWindow already
    // uses for the same reason.
    std::unique_ptr<VibeTooltip>                    mTooltips;
    // Routes the resize path through clampResizeToWorkspace.  Declared BEFORE
    // mResizer on purpose: the resizer holds a raw pointer to it, and members
    // destruct in reverse declaration order, so being declared first means
    // outliving the thing that points at it.
    struct Constrainer : juce::ComponentBoundsConstrainer
    {
        explicit Constrainer (WorkspaceWindow& o) : owner (o) {}
        void applyBoundsToComponent (juce::Component&, juce::Rectangle<int>) override;
        WorkspaceWindow& owner;
    };
    Constrainer                                      mConstrainer { *this };
    std::unique_ptr<juce::ResizableBorderComponent>  mResizer;
    // SafePointer, not a raw Workspace*.  Three crashes in a row (drag,
    // magnetism, window-array teardown) all bottomed out in reads through this
    // pointer with garbage addresses, and reading the code did not explain how
    // it goes bad.  A SafePointer cannot: if the Workspace dies this reads
    // NULL, every user early-returns, and the DBG in workspace() names the
    // moment -- turning an unexplainable crash into a signal we can act on.
    juce::Component::SafePointer<Workspace> mWorkspace;
    Workspace* workspace() const noexcept;
    mutable bool mReportedDeadWorkspace { false };
    // Set by attachTo.  Without it the "no workspace" warning fired during the
    // layout that setContentNonOwned triggers -- which runs BEFORE attachTo --
    // and the one-shot latch was spent there, so the warning never once
    // described the state it was written to catch.
    bool mAttachAttempted { false };
    // Manual title drag.  ComponentDragger + ComponentBoundsConstrainer are
    // NOT used for the move: they route a desktop component's bounds through
    // screen-space limit maths, and this window's bounds are PARENT-CLIENT (see
    // the peer contract above), so the two disagree about what a coordinate
    // means.  A screen-space DELTA is identical in both spaces -- the spaces
    // differ only by a translation -- so applying the delta to the bounds we
    // captured at mouse-down is correct without any conversion at all.
    bool                 mDraggingTitle { false };
    juce::Rectangle<int> mDragStartBounds;
    juce::Point<int>     mDragStartScreen;
    // Runaway guard for the cursor-pinning warp in mouseDrag.  See the long
    // comment there: the warp is proven to converge, and this exists purely so
    // a future change that breaks that proof degrades to "the cursor comes
    // unstuck" instead of "the window flickers and the app is unusable".
    int                  mWarpsThisDrag { 0 };
    static constexpr int kMaxWarpsPerDrag = 64;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorkspaceWindow)
};

// ── Workspace ────────────────────────────────────────────────────────────────
// The region of the main frame that contained windows live in.  It is an
// ordinary child Component (it draws the empty backdrop), but its job is to be
// the ANCHOR: it supplies the native parent handle every WorkspaceWindow
// attaches to, and the origin those windows' parent-client coordinates are
// measured from.
// ─────────────────────────────────────────────────────────────────────────────
class Workspace : public juce::Component
{
public:
    Workspace();

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

    // The HWND (or platform equivalent) contained windows parent to.  This is
    // the MAIN WINDOW'S handle, not one of our own: a Workspace is a
    // lightweight child component and has no peer of its own, so getPeer()
    // walks up to the frame.  Null before the frame is on screen.
    void* getNativeParentHandle() const;

    // Where this component sits inside that parent's client area.  Contained
    // windows add this to their workspace-local position, because their peer
    // bounds are parent-client relative (see the contract note above).
    juce::Point<int> originInParentClient() const;

    // Attach any window that asked to attach BEFORE the frame had a peer.
    // Pages are built in StandaloneEditor's CONSTRUCTOR, which runs before the
    // editor is set as the window's content -- so at that moment there is no
    // HWND to parent to.  Without this deferral every window was created and
    // then silently never attached or shown.
    void attachPendingWindows();
    void queueForAttach (WorkspaceWindow* w);
    void addWindow    (WorkspaceWindow* w);
    void removeWindow (WorkspaceWindow* w);
    const juce::Array<juce::Component::SafePointer<WorkspaceWindow>>& getWindows() const noexcept { return mWindows; }

private:
    // Pull every window back into view after the workspace changes size.  Same
    // duty as the main window's monitor-overlap check (QA-ProjectSave): a
    // window whose saved spot is no longer reachable must not become
    // unreachable, it must come back.
    void clampWindowsIntoView();

    // SafePointer, not a raw pointer: a window that dies without its
    // destructor reaching removeWindow (any teardown-ordering mistake, now or
    // later) would otherwise leave a dangling entry that the next iteration
    // dereferences.  Both crashes Jeff hit were reads through this array, so
    // the type now makes that failure impossible rather than relying on the
    // bookkeeping being perfect.
    juce::Array<juce::Component::SafePointer<WorkspaceWindow>> mWindows;
    juce::Array<juce::Component::SafePointer<WorkspaceWindow>> mPendingAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Workspace)
};
