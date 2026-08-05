#pragma once
#include <JuceHeader.h>
#include "WindowChrome.h"   // TS7 §9.1: shared title-strip metrics + look
#include <functional>
#include <map>
#include <memory>
#include <set>

class Workspace;
class PageMenuBar;
class GlobalAutoRightClick;

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

    // Resize so the content area is exactly contentW x contentH (clamped to the
    // workspace, floored by the constrainer).  Used by hosted-plugin surfaces,
    // which arrive at whatever size the plugin declares.
    void sizeToContent (int contentW, int contentH);
    // Hands the page back WITHOUT destroying it -- for a reparent that must not
    // tear the page down.  Destroy-on-close does not use this (it wants the
    // page gone); it exists so a future move-between-frames is possible.
    std::unique_ptr<juce::Component> releaseContent();

    // DEFAULT OPENING SIZE, in WINDOW dimensions (chrome included -- the
    // measured numbers were taken off the whole window).
    //
    // Jeff, 2026-08-04: this is NOT a lock.  Content minimums are suspended
    // until the compact-layout task decides what they should be, so a window
    // may be dragged smaller or larger than its default freely.  The only
    // surviving constraint is the anti-degenerate clamp in the ctor.  Applying
    // this to a window that has never been sized by the session, a project or
    // the user sets its size outright; a window that HAS been sized keeps what
    // it was given.
    void setDefaultWindowSize (int w, int h);
    // False until a real default has been supplied.  Drives the editor's
    // healing sweep -- an engine-derived size is unknown while the engine is
    // still binding, and the event announcing it can be missed.
    bool hasDefaultSize() const noexcept { return mDefaultKnown; }


    // QA-Layout T5: the THREE-LIFETIME model (Jeff's 2026-07-28 ruling).
    //
    //   Lifetime 1 -- the in-memory map -- is the ONE live store.  EVERY
    //   window (Disk or Session) writes it on move/resize/close and reads it
    //   on open, so close/reopen returns to the same spot universally.
    //   Lifetime 2 -- settings.xml -- written ONCE at app exit from a
    //   FILTERED view of the map (writeSessionToSettings): sizes for every
    //   Disk-marked window, placement only for keys registered
    //   placement-persistent (the four default tabs).  Read only as a seed
    //   when the map misses.  The old parse-and-rewrite-per-close is gone.
    //   Lifetime 3 -- the project file -- StandaloneEditor serializes the
    //   full map + per-window open state and REPLACES the map on load.
    //
    //   Disk    - eligible for the settings.xml exit write.  Jeff, 2026-08-04:
    //             this is ONLY the four default tabs (Builder / Mixer /
    //             Effects / Piano Roll), size AND position.  Nothing else goes
    //             in that file.  It previously defaulted to Disk for every page
    //             window and wrote every window's SIZE globally, gating only
    //             POSITION to the four -- the exact inverse of the ruling, and
    //             what left 143 records in settings.xml feeding stale sizes to
    //             players on every cold start.
    //   Session - map only.  EVERY other window: players, effect windows,
    //             satellites.  Their size and placement are session state
    //             (lifetime 1) and project content (lifetime 3); a cold start
    //             with no project opens them at their DEFAULT size.
    enum class Persistence { Disk, Session };
    void setPersistence (Persistence p) noexcept { mPersistence = p; }

    // Lifetime plumbing (all message-thread).
    static const std::map<juce::String, juce::Rectangle<int>>& sessionBoundsMap();
    static void replaceSessionBounds (std::map<juce::String, juce::Rectangle<int>> m);
    static void registerPlacementPersistentKey (const juce::String& key);
    // QA-Layout T7 fix: true for the four default tabs, whose placement is a
    // GLOBAL preference (settings.xml) by the T5 three-lifetime ruling.  A
    // project must not carry -- or restore -- their bounds, or its stale copy
    // beats the position the user just set (Jeff, 2026-08-04: the Mixer moved
    // and "didn't save", because every project open overwrote it).
    static bool isGlobalPlacementKey (const juce::String& key);
    // Register the global-placement keys BEFORE any window is framed.  The
    // per-window registration above happens at first framing, which is too
    // late if a project loads first -- the filter would see an empty set and
    // let the project's stale copy through (Jeff, 2026-08-04: the Mixer came
    // back at the project's y instead of its own).
    static void seedGlobalPlacementKeys (const juce::StringArray& keys);

    // Programmatic repositioning (the workspace's clamp-into-view sweep) must
    // NOT reach the bounds store: it runs on every frame resize, and at startup
    // it fires while the workspace is still mid-layout.  Persisting those would
    // overwrite the user's placement with a transient clamp -- which is exactly
    // how a hand-placed window drifted to somewhere it was never put.  User
    // gestures (drag, border resize) are unaffected.
    struct ScopedSaveSuppress
    {
        ScopedSaveSuppress()  { ++sSaveSuppressed; }
        ~ScopedSaveSuppress() { --sSaveSuppressed; }
    };
    static bool savesSuppressed() noexcept { return sSaveSuppressed > 0; }
    // Re-apply this window's stored bounds (workspace-local -> parent-client).
    // Used by the workspace's grow-restore pass.
    void applySavedBounds();
    static void writeSessionToSettings();

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

    // Persist current bounds under the persist key.  Called on close, after a
    // title-drag, and from the DESTRUCTOR -- the destructor call is what makes
    // teardown and resize-only changes persist (resizes have no end-of-gesture
    // hook of their own).  Skips silently once the workspace is gone, because
    // bounds can no longer be converted into workspace-local space.
    void saveBounds() const;

    // Order-safe containment: never returns an inverted or degenerate rect, and
    // never shrinks below the constrainer's minimum (a window squeezed under it
    // produced a NEGATIVE content area, which crashed the first slider that
    // tried to paint into it).  Public because Workspace applies the SAME rule
    // on relayout -- one containment definition, two callers.
    juce::Rectangle<int> clampToWorkspace (juce::Rectangle<int> target) const;

    // QA-Layout T3 (L5, REVERSES locked call 5a): full-screen toggle -- fills
    // the workspace, and toggles back to the bounds the user last set.  The
    // restore path is what answers 5a's original objection (a maximised
    // window now has a way back out).  A manual drag or resize while filled
    // clears the flag, so the next click fills again rather than restoring a
    // stale rect.
    void toggleWorkspaceFill();
    bool isWorkspaceFilled() const noexcept { return mFilled; }

    // Soft edge magnetism (Jeff spec 2026-07-28).  While dragging, a window
    // whose edge comes within kSnapPx of another window's opposing edge (or of
    // a workspace edge) is nudged flush so they line up.  Deliberately NOT a
    // lock: the snap only adjusts the position it is given, so continuing to
    // drag pushes straight past it, and windows may still overlap freely.
    juce::Rectangle<int> applyMagnetism (juce::Rectangle<int> target) const;
    // Resize-path magnetism.  applyMagnetism translates the whole window, which
    // a resize must not do -- this snaps each edge independently and only the
    // ones that moved.  Jeff, 2026-08-04: the magnet worked on move and not at
    // all on resize, because only mouseDrag ever called for it.
    juce::Rectangle<int> applyResizeMagnetism (juce::Rectangle<int> target) const;
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
    void moved() override;
    void broughtToFront() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;

    // TS7 §9.1: aliases, not values -- the numbers live in WindowChrome with the
    // colours.  NOTE this does NOT make the two strip heights impossible to
    // diverge (an earlier version of this comment claimed it did): a desktop
    // DocumentWindow uses JUCE's own titleBarHeight, which happens to be 26 as
    // well, and nothing calls setTitleBarHeight.  They match by coincidence.
    // The existing names stay -- ~40 call sites use them.
    static constexpr int kTitleH   = WindowChrome::kTitleH;
    static constexpr int kBorderPx = WindowChrome::kBorderPx;

private:
    juce::Rectangle<int> titleBarArea()  const noexcept;
    juce::Rectangle<int> contentArea()   const noexcept;
    // Saved bounds for this key, or an empty rect when the window has never
    // been placed.  WORKSPACE-LOCAL space (attachTo adds the origin).
    // outHasPosition is false when only a SIZE was saved (a settings.xml
    // record whose placement was filtered out -- player windows).
    juce::Rectangle<int> loadSavedBounds (bool& outHasPosition) const;

    juce::String mPersistKey, mTitle;
    // Session by default: only the four default tabs are promoted to Disk, and
    // hostPageInWindow does that explicitly by key.
    Persistence  mPersistence { Persistence::Session };
    // Lifetime-1 bounds (workspace-local), keyed exactly like the disk
    // records.  Static because the window object itself is short-lived --
    // closing destroys it, and the key is the only thing carrying position
    // to the next open.
    static std::map<juce::String, juce::Rectangle<int>>& sessionBounds();
    // Keys eligible for the settings.xml exit write (every Disk window that
    // saved this session) and the subset whose PLACEMENT persists there.
    static std::set<juce::String>& diskEligibleKeys();
    static std::set<juce::String>& placementKeys();
    static int sSaveSuppressed;   // ScopedSaveSuppress depth
    // mContent owns only when setContent was used; mContentRaw is what gets
    // laid out either way (and is the non-owning case's only handle).
    std::unique_ptr<juce::Component>                 mContent;
    juce::Component*                                 mContentRaw { nullptr };
    std::unique_ptr<juce::TextButton>                mCloseBtn;
    std::unique_ptr<juce::Button>                    mFullBtn;      // L5 fill toggle
    juce::Rectangle<int>                             mRestoreBounds;   // pre-fill (parent-client)
    bool                                             mFilled { false };
    std::unique_ptr<PageMenuBar>                    mPageMenu;
    // NO per-window tooltip since 2026-08-04.  One per window was the fix for
    // "knob info tooltips vanished" (Jeff, 2026-07-28) back when the editor's
    // tooltip was parented to the frame and therefore could not reach in here.
    // The editor's is parentless now -- a real desktop window that floats above
    // every contained window and serves all of them -- so a local one would
    // only produce a second tip on top of the first.
    //
    // Each contained window DOES still need its OWN right-click Automate
    // listener, for the same peer-boundary reason the tooltip used to need one.
    // StandaloneEditor installs one GlobalAutoRightClick over its own
    // component tree, but a contained window is a separate native peer with a
    // separate tree, so that listener never sees a click in here.  VKnob-based
    // controls kept working (a VKnob listens to its own slider and tags it so
    // the global handler skips it); every VibeSlider did not, because
    // VibeSlider swallows the right-click on purpose and depends entirely on
    // this listener to raise the menu.  That is what silently cost the players
    // and the mixer strips their Automate menu when pages moved into contained
    // windows (Jeff, 2026-08-04).  The two listener scopes are disjoint, so
    // this cannot double up the menu.
    std::unique_ptr<GlobalAutoRightClick>           mAutoRightClick;
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
    // Jeff, 2026-08-04.  The ctor installs a 320x200 PLACEHOLDER minimum, and
    // attachTo uses the minimum as the first-open size.  That was harmless
    // while every floor was known at creation, but an engine-driven floor is
    // now deliberately unknown until the engine binds -- so such a window first
    // opened at 320x200 and only reached its real size later, if at all.
    // mFloorKnown says whether a REAL floor has been installed;
    // mOpenedAtPlaceholder marks a window that opened before one arrived and
    // has never been sized by the user, so the floor can snap it exactly rather
    // than merely grow it.
    bool                                             mDefaultKnown { false };
    bool                                             mOpenedAtPlaceholder { false };
    juce::Point<int>                                 mDefaultSize { 0, 0 };
public:
    // Anti-degenerate clamp only -- small enough to be no content rule, big
    // enough that a window can always be grabbed and dragged back.
    static constexpr int kMinDegenerateW = 120;
    static constexpr int kMinDegenerateH = 80;
private:
    std::unique_ptr<juce::ResizableBorderComponent>  mResizer;
    // SafePointer, not a raw Workspace*.  Three crashes in a row (drag,
    // magnetism, window-array teardown) all bottomed out in reads through this
    // pointer with garbage addresses, and reading the code did not explain how
    // it goes bad.  A SafePointer cannot: if the Workspace dies this reads
    // NULL and every user early-returns, so a dead Workspace degrades to
    // containment and magnetism being off rather than to a crash.
    juce::Component::SafePointer<Workspace> mWorkspace;
    Workspace* workspace() const noexcept;
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
    // A GROWING workspace must put windows BACK where they were saved.  The
    // clamp above squeezes them into whatever the workspace measured at the
    // time, and at launch that is the frame mid-layout -- so a window saved
    // below the not-yet-full-height workspace got pulled up and nothing ever
    // moved it back (Jeff, 2026-08-04: the Mixer kept reappearing higher than
    // he left it).  Suppressed from the bounds store: this is a restore, not
    // a placement.
    void restoreWindowsToSavedBounds();
    juce::Point<int> mLastSize;

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
