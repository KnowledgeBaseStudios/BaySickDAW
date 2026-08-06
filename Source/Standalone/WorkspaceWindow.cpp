#include "WorkspaceWindow.h"
#include "SharedUI.h"   // VibeLAF + PageMenuBar
#include "WindowChrome.h"   // TS7 §9.1: the one title-strip look
#include "../ProjectManager.h"

namespace
{
    // Contained-window bounds live alongside the main frame's own WindowState
    // in settings.xml, so window layout is a global preference rather than
    // project data -- the user's arrangement of their workspace should not
    // change when they open a different song.
    constexpr const char* kRootTag   = "WorkspaceWindows";
    constexpr const char* kWindowTag = "W";

    // TS7 §9.1: the palette moved to WindowChrome so the desktop windows can
    // paint the same strip.  Nothing here defines a colour any more.

    // QA-Layout T3 (L5): maximize/restore glyph drawn as paths, like the
    // close button's "x" -- no dependency on a font having the glyph.
    class FillToggleButton : public juce::Button
    {
    public:
        FillToggleButton() : juce::Button ("fullscreen") {}
        std::function<bool()> isFilled;

        void paintButton (juce::Graphics& g, bool isOver, bool isDown) override
        {
            auto b = getLocalBounds().toFloat();
            if (isOver || isDown)
            {
                g.setColour (juce::Colours::white.withAlpha (isDown ? 0.22f : 0.12f));
                g.fillRect (b);
            }

            const bool filled = isFilled && isFilled();
            auto r = b.reduced (b.getWidth() * 0.30f, b.getHeight() * 0.30f);
            g.setColour (WindowChrome::titleText());
            if (! filled)
            {
                g.drawRect (r, 1.4f);
            }
            else
            {
                // Restore glyph: a second square peeking out behind the first.
                auto back = r.translated (r.getWidth() * 0.25f, -r.getHeight() * 0.25f);
                g.drawRect (back, 1.2f);
                g.setColour (WindowChrome::titleBg (true));
                g.fillRect (r);
                g.setColour (WindowChrome::titleText());
                g.drawRect (r, 1.4f);
            }
        }
    };
}

// ─────────────────────────────────────────────────────────────────────────────
WorkspaceWindow::WorkspaceWindow (juce::String persistKey, juce::String title)
    : mPersistKey (std::move (persistKey)), mTitle (std::move (title))
{
    setOpaque (true);

    mCloseBtn = std::make_unique<juce::TextButton> ("x");
    mCloseBtn->setLookAndFeel (&VibeLAF::get());
    mCloseBtn->setTooltip ("Close this window (the engine keeps running)");
    mCloseBtn->onClick = [this]
    {
        saveBounds();
        // CRASH FIX (Jeff, 2026-07-28): the owner DESTROYS this window in
        // response, and we are currently inside juce::Button::sendClickMessage
        // -- which keeps running (Button::mouseUp continues) after the callback
        // returns, on a button that no longer exists.  Repeated identical
        // access violations in ~WorkspaceWindow came through exactly this path.
        // Defer the close so the button's own click handling unwinds first.
        juce::Component::SafePointer<WorkspaceWindow> safeThis (this);
        juce::MessageManager::callAsync ([safeThis]
        {
            if (auto* w = safeThis.getComponent())
                if (w->onCloseRequested) w->onCloseRequested();
        });
    };
    addAndMakeVisible (*mCloseBtn);

    // QA-Layout T3 (L5, reverses locked call 5a's no-maximise half): a
    // workspace-fill toggle sits left of the close button.  Still no
    // minimise -- there is nowhere for a minimised window to go inside a
    // contained frame.
    {
        auto* fill = new FillToggleButton();
        fill->isFilled = [this] { return mFilled; };
        mFullBtn.reset (fill);
        mFullBtn->setTooltip ("Fill the workspace / restore the previous size");
        mFullBtn->onClick = [this] { toggleWorkspaceFill(); };
        addAndMakeVisible (*mFullBtn);
    }

    // Locked call 4a: the page's hamburger/menu row IS the title strip.  It is
    // created empty; StandaloneEditor fills it for whichever page this window
    // frames, using the same calls it used to make against the shared bar.
    mPageMenu = std::make_unique<PageMenuBar>();
    mPageMenu->setPageTitle (mTitle);
    // The menu fills the whole title strip, so without this it swallows every
    // click and the window could not be dragged at all (found on Jeff's first
    // run).  false/true = the BAR itself is click-transparent, its buttons are
    // not -- so empty title-strip area falls through to the drag below while
    // the hamburger and slots keep working.  PageMenuBar has no mouse handlers
    // of its own; everything interactive in it is a child component.
    mPageMenu->setInterceptsMouseClicks (false, true);
    addAndMakeVisible (*mPageMenu);

    // No tooltip window here -- see the member comment.  The editor's is a
    // desktop window now and covers this one too.

    // Right-click Automate: see the member comment.  Installed over this
    // window's whole subtree, which is the only tree the editor's own listener
    // cannot reach.
    mAutoRightClick = std::make_unique<GlobalAutoRightClick>();
    addMouseListener (mAutoRightClick.get(), true);

    // Jeff, 2026-08-04: the ONLY constraint is anti-degenerate.  Content-based
    // minimums are suspended until the compact-layout task decides what they
    // should be -- until then a window may be dragged to any size, and the
    // measured numbers act purely as the DEFAULT opening size
    // (setDefaultWindowSize).  The old 320x200 was a resize floor AND doubled
    // as the first-open size, which is how an unresolved window came up at
    // 320x200 and looked deliberate.
    mConstrainer.setMinimumSize (kMinDegenerateW, kMinDegenerateH);
    mResizer = std::make_unique<juce::ResizableBorderComponent> (this, &mConstrainer);
    mResizer->setBorderThickness (juce::BorderSize<int> (kBorderPx));
    addAndMakeVisible (*mResizer);

    // A click ANYWHERE in this window raises it, not just on the title strip
    // (Jeff, 2026-07-29: a window behind another only came forward when its
    // title bar was hit).  mouseDown only ever sees clicks that reach the
    // WINDOW -- the title strip and the resize border -- because the hosted page
    // and every control in it consume their own.  JUCE resolves this above the
    // component: on mouse-down it walks the clicked component's whole ancestor
    // chain and calls toFront on any link carrying this flag, so a click on a
    // knob six levels down still raises the window that contains it.
    setBroughtToFrontOnMouseClick (true);
}

// Fired for every route that raises this window -- the flag above, the title
// drag, and programmatic toFront from tab selection.  Owning the ribbon sync
// here rather than in mouseDown is what keeps the tab bar honest now that
// content clicks raise windows too.
void WorkspaceWindow::broughtToFront()
{
    juce::Component::broughtToFront();
    if (onBroughtToFront) onBroughtToFront();
}

Workspace* WorkspaceWindow::workspace() const noexcept
{
    return mWorkspace.getComponent();
}

WorkspaceWindow::~WorkspaceWindow()
{
    // NO saveBounds() here (removed 2026-08-04).  A destructor runs while the
    // window is being dismantled, so what it reported was not the geometry the
    // user left -- and because it ran LAST, it overwrote the good value on the
    // way out.  That is why a hand-placed Mixer kept coming back somewhere
    // else after an app restart.  Every real route saves explicitly now:
    // drag-release, border resize + move (resized/moved), the close button,
    // the fill toggle, and the exit flush that runs before teardown
    // (StandaloneEditor::flushWindowBoundsNow).
    if (mCloseBtn) mCloseBtn->setLookAndFeel (nullptr);
    // Unhook before the listener object dies -- a page detached here (see
    // below) outlives this window and would otherwise keep a dangling entry.
    if (mAutoRightClick) removeMouseListener (mAutoRightClick.get());
    if (auto* ws = mWorkspace.getComponent()) ws->removeWindow (this);
    // Peer teardown is Component's job.  An OWNED page dies with mContent; a
    // non-owned one is detached here and outlives us by design.
    if (mContentRaw != nullptr && mContent == nullptr)
        removeChildComponent (mContentRaw);
}

void WorkspaceWindow::setContent (std::unique_ptr<juce::Component> content)
{
    if (mContentRaw) removeChildComponent (mContentRaw);
    mContent = std::move (content);
    mContentRaw = mContent.get();
    if (mContentRaw)
    {
        addAndMakeVisible (*mContentRaw);
        // Behind the resize border + title strip in z-order, so an edge drag
        // hits the border rather than the page.
        mContentRaw->toBack();
    }
    resized();
}

void WorkspaceWindow::setContentNonOwned (juce::Component* content)
{
    if (mContentRaw) removeChildComponent (mContentRaw);
    mContent.reset();
    mContentRaw = content;
    if (mContentRaw)
    {
        addAndMakeVisible (*mContentRaw);
        mContentRaw->toBack();
    }
    resized();
}

std::unique_ptr<juce::Component> WorkspaceWindow::releaseContent()
{
    if (mContentRaw) removeChildComponent (mContentRaw);
    mContentRaw = nullptr;
    return std::move (mContent);
}

void WorkspaceWindow::setDefaultWindowSize (int w, int h)
{
    mDefaultSize  = { juce::jmax (kMinDegenerateW, w), juce::jmax (kMinDegenerateH, h) };
    mDefaultKnown = true;

    // Jeff, 2026-08-05: the measured size is the MINIMUM again as well as the
    // opening size.  These numbers are the "smallest still readable" ones, so a
    // window that refuses to go below its own is correct behaviour -- the
    // suspension was only ever pending T8, and compact layouts moved to Future
    // State (CL-306).  A view that genuinely needs to be smaller declares its
    // own smaller default via setDefaultWindowSize (the pedals Compact view).
    mConstrainer.setMinimumSize (mDefaultSize.x, mDefaultSize.y);

    // ONLY a window that opened without a remembered size takes the default.
    // A size that came from the session map, a project or the user's own drag
    // is never overwritten -- that is the three-lifetime contract, and the
    // default exists to answer "what size when nothing has an opinion".
    if (mOpenedAtPlaceholder)
    {
        mOpenedAtPlaceholder = false;
        setSize (mDefaultSize.x, mDefaultSize.y);
        return;
    }

    // A remembered size from before this window's default was known -- or from
    // a run where the minimum was suspended -- must come up to the new floor.
    if (getWidth() < mDefaultSize.x || getHeight() < mDefaultSize.y)
        setSize (juce::jmax (getWidth(),  mDefaultSize.x),
                 juce::jmax (getHeight(), mDefaultSize.y));
}

void WorkspaceWindow::setResizeFloor (int minW, int minH)
{
    // Never below the degenerate floor -- a window still has to be grabbable
    // whatever its content claims it can shrink to.
    mConstrainer.setMinimumSize (juce::jmax (kMinDegenerateW, minW),
                                 juce::jmax (kMinDegenerateH, minH));
}

void WorkspaceWindow::setTitle (juce::String t)
{
    if (mTitle == t) return;
    mTitle = std::move (t);
    if (mPageMenu) mPageMenu->setPageTitle (mTitle);
    repaint (titleBarArea());
}

juce::Rectangle<int> WorkspaceWindow::titleBarArea() const noexcept
{
    return getLocalBounds().reduced (kBorderPx).removeFromTop (kTitleH);
}

juce::Rectangle<int> WorkspaceWindow::contentArea() const noexcept
{
    auto b = getLocalBounds().reduced (kBorderPx);
    b.removeFromTop (kTitleH);
    return b;
}

void WorkspaceWindow::paint (juce::Graphics& g)
{
    const bool live = isMouseOverOrDragging (true)
                      || (mContentRaw != nullptr && mContentRaw->hasKeyboardFocus (true));

    // No title string passed: the PageMenuBar filling this strip draws it (locked
    // call 4a -- the page's menu row IS the title strip), so drawing it here too
    // would double it.
    WindowChrome::paintFrame (g, getLocalBounds());
    WindowChrome::paintTitleBar (g, titleBarArea(), live);
}

// QA-ModelShell TS6 (Jeff 2026-07-29): a hosted plugin brings its own surface
// at its own size, so the window is sized to THAT rather than left at a
// provisional floor with dead space around the plugin.  The inverse of
// contentBounds(): add the chrome back on.
//
// Clamped to the workspace so a plugin with a huge editor cannot open a window
// bigger than the frame containing it -- with the constrainer's floor applied
// LAST, so HERE the floor wins over the workspace fit.  Deliberately the
// OPPOSITE order from clampResizeToWorkspace (there the workspace caps the
// floor): this is a one-shot programmatic size, and a window slightly
// overflowing a tiny workspace beats one squeezed under its own minimum into
// a negative content area.
void WorkspaceWindow::sizeToContent (int contentW, int contentH)
{
    if (contentW <= 0 || contentH <= 0)
        return;

    int w = contentW + 2 * kBorderPx;
    int h = contentH + kTitleH + 2 * kBorderPx;

    if (auto* ws = workspace())
    {
        const auto avail = ws->getLocalBounds();

        if (avail.getWidth()  > 0) w = juce::jmin (w, avail.getWidth());
        if (avail.getHeight() > 0) h = juce::jmin (h, avail.getHeight());
    }

    w = juce::jmax (w, mConstrainer.getMinimumWidth());
    h = juce::jmax (h, mConstrainer.getMinimumHeight());

    if (w != getWidth() || h != getHeight())
        setSize (w, h);
}

void WorkspaceWindow::resized()
{
    // QA-Layout T7 fix: persist on EVERY settled geometry change, not just the
    // title-drag release + teardown.  A border resize has no end-of-gesture
    // hook, so before this the only thing that captured it was the destructor
    // -- and anything that ended the session another way lost it.  The
    // isOnDesktop guard keeps attachTo's pre-reparent setBounds (which is in
    // workspace-local space, before the origin is applied) out of the store.
    if (isOnDesktop()) saveBounds();

    if (mResizer)  mResizer->setBounds (getLocalBounds());

    // L5 button order, right-to-left: close, fill toggle; the preset button
    // (when a page mounts one) rides the PageMenuBar's right-extras cluster,
    // which ends left of these.
    auto title = titleBarArea();
    if (mCloseBtn)
        mCloseBtn->setBounds (title.removeFromRight (kTitleH).reduced (4));
    if (mFullBtn)
        mFullBtn->setBounds (title.removeFromRight (kTitleH).reduced (4));
    if (mPageMenu) mPageMenu->setBounds (title);

    if (mContentRaw) mContentRaw->setBounds (contentArea());
}

void WorkspaceWindow::mouseDown (const juce::MouseEvent& e)
{
    mDraggingTitle   = titleBarArea().contains (e.getPosition());
    mDragStartBounds = getBounds();
    mDragStartScreen = e.getScreenPosition();

    // Raise AFTER capturing the drag anchor.  toFront on a native child peer is
    // a real SetWindowPos/SetFocus, and doing it before the anchor is recorded
    // can move the window out from under the gesture that started it.
    // (The ribbon sync moved to broughtToFront(), which this reaches -- keeping
    // it here too would fire it twice per title click.)
    toFront (true);
}

void WorkspaceWindow::mouseDrag (const juce::MouseEvent& e)
{
    if (! mDraggingTitle) return;

    // L5: a manual move leaves the filled state -- the toggle should fill
    // again from here, not restore a rect the user has since abandoned.
    if (mFilled)
    {
        mFilled = false;
        if (mFullBtn) mFullBtn->repaint();
    }

    const auto delta = e.getScreenPosition() - mDragStartScreen;
    const auto desired = mDragStartBounds.translated (delta.x, delta.y);

    // Contained workspace (locked call 2b): a window lives INSIDE the frame, so
    // it is clamped to the workspace rather than allowed to wander off it.
    //
    // CRASH FIX (Jeff, 2026-07-28): the previous clamp fed jlimit a range whose
    // lower bound could EXCEED its upper bound -- any window wider than the
    // workspace minus the keep-visible margin produced that -- and jlimit has
    // no defence against an inverted range.  clampToWorkspace orders the bounds
    // explicitly and is shared with the resize/relayout path so the two cannot
    // drift.
    const auto snapped = applyMagnetism (desired);

    // THE BOUND IS THE CURSOR, NOT THE WINDOW (Jeff, 2026-08-05, superseding
    // locked call 2b).  A window may hang off any edge; the POINTER may not
    // leave the workspace, which preserves the only guarantee that matters --
    // a window can never be dragged somewhere it cannot be grabbed back from.
    //
    // This also drops the size fit from the drag path.  clampToWorkspace both
    // moves AND resizes, and sharing it here meant the first drag of a window
    // wider than the workspace snapped it to workspace width, destroying a size
    // the user had chosen.  A drag does not change size.
    juce::Rectangle<int> finalBounds = snapped;

    if (auto* ws = workspace())
    {
        const auto wsScreen = ws->getScreenBounds();
        const auto cursor   = e.getScreenPosition();
        const juce::Point<int> bounded {
            juce::jlimit (wsScreen.getX(), wsScreen.getRight()  - 1, cursor.x),
            juce::jlimit (wsScreen.getY(), wsScreen.getBottom() - 1, cursor.y)
        };
        finalBounds = snapped.translated (bounded.x - cursor.x, bounded.y - cursor.y);
    }

    // Keep the cursor STUCK to the window (Jeff spec 2026-07-28).  Without this
    // the window stops at the workspace edge while the mouse keeps travelling,
    // so the pointer slides off the title bar and the window no longer tracks
    // the grab point when you come back.
    //
    // The correction is measured from the SNAPPED position, not the raw desired
    // one, so ONLY containment moves the cursor.  Letting magnetism warp too
    // turned the soft snap into a trap (Jeff, 2026-07-28: "hard snapping to the
    // outer edge ... takes a couple seconds to pull it off"): a small move away
    // from a snapped edge is pulled back by the magnet, the warp then dragged
    // the pointer back to match, and the next event started from the snapped
    // position again -- so escaping required out-running the snap inside a
    // SINGLE mouse event, which at normal polling rates is not achievable by
    // moving slowly.  Measuring from the snapped position makes the magnet cost
    // nothing: the window offsets under the cursor by up to kSnapPx and pulling
    // away simply works.
    //
    // DO NOT ALSO RE-BASELINE mDragStartScreen HERE.  The first attempt did, and
    // the two adjustments cancel exactly: the synthetic move arrives at
    // screen+corr, the anchor has moved by the same corr, so the delta -- and
    // therefore `desired` -- is unchanged, the clamp produces the same
    // correction, and it warps again on every event forever.  That is the
    // "cursor detaches and the window flickers" failure.
    //
    // Warping ALONE converges in one event: the next event's delta grows by
    // corr, so `desired` becomes the already-clamped position, the correction
    // falls to zero and the warping stops.
    const auto corr = finalBounds.getPosition() - snapped.getPosition();
    if (corr.x != 0 || corr.y != 0)
    {
        // Safety valve, not part of the algorithm.  The convergence above is a
        // proof about this code as written; the cap is what keeps a FUTURE
        // mistake in the clamp or the magnet from making the app unusable
        // rather than merely dropping the cursor.
        if (++mWarpsThisDrag <= kMaxWarpsPerDrag)
            juce::Desktop::getInstance().setMousePosition (e.getScreenPosition() + corr);
    }
    else
    {
        mWarpsThisDrag = 0;
    }

    setBounds (finalBounds);
}

void WorkspaceWindow::moved()
{
    // Same reasoning as resized(): every settled position lands in the store,
    // including programmatic moves (snap, workspace clamp) that never pass
    // through the title-drag release below.
    if (isOnDesktop()) saveBounds();
}

void WorkspaceWindow::mouseUp (const juce::MouseEvent&)
{
    if (! mDraggingTitle) return;
    mDraggingTitle = false;
    mWarpsThisDrag = 0;
    saveBounds();
}

void WorkspaceWindow::toggleWorkspaceFill()
{
    auto* ws = workspace();
    if (ws == nullptr || ws->getWidth() <= 0 || ws->getHeight() <= 0) return;

    if (! mFilled)
    {
        mRestoreBounds = getBounds();
        mFilled = true;
        // Fill rect in parent-client space: workspace origin + its size (same
        // translation attachTo applies to a restored position).
        const auto o = ws->originInParentClient();
        setBounds (clampToWorkspace ({ o.x, o.y, ws->getWidth(), ws->getHeight() }));
    }
    else
    {
        mFilled = false;
        if (! mRestoreBounds.isEmpty())
            setBounds (clampToWorkspace (mRestoreBounds));
    }
    if (mFullBtn) mFullBtn->repaint();
    saveBounds();
}

void WorkspaceWindow::attachTo (Workspace& ws)
{
    mWorkspace = &ws;
    ws.addWindow (this);

    // Parent-client space: the peer's coordinates are measured from the MAIN
    // window's client origin, so the workspace's own offset inside the frame
    // has to be added on.  (See the contract note in the header -- this is the
    // one line that differs from how a top-level JUCE window is positioned.)
    //
    // WAIT FOR A LAID-OUT WORKSPACE, not just a peer (fix 2026-08-04).  A
    // zero-size Workspace reports originInParentClient() as (0,0), so every
    // window restored at that moment landed short by the real origin -- saved
    // at origin (1,91), restored against (0,0), i.e. 91px too high, every
    // launch.  It also collapsed the first-open default size, which is a
    // fraction of the workspace, to its 480x320 floor.  Both symptoms, one
    // cause.  Queue instead; Workspace::resized drains the queue once the
    // frame is laid out and the handle + bounds are both real.
    auto* parent = ws.getNativeParentHandle();
    if (parent == nullptr || ws.getWidth() <= 0 || ws.getHeight() <= 0)
    {
        ws.queueForAttach (this);
        return;
    }

    bool hasPos = true;
    auto saved = loadSavedBounds (hasPos);
    // First open: the window's OWN minimum is its natural size -- those are
    // the measured "smallest still readable" numbers, which Jeff set as both
    // the floor and the default (2026-08-04).  Offset per already-open window
    // so a fresh window never lands exactly on top of the one before it.  A
    // SIZE-ONLY seed (T5: player sizes carry across projects, placement does
    // not) keeps its size and takes the default spot.
    const int step = 28 * juce::jmin (6, ws.getWindows().size() - 1);
    if (saved.isEmpty())
    {
        // Nothing remembered.  Use the default if it is already known; if it is
        // not (engine-derived size, engine still binding) open at a neutral
        // placeholder and flag it, so setDefaultWindowSize snaps the window the
        // moment the default arrives.  It still OPENS now rather than waiting:
        // a page whose engine never binds must not end up with no window.
        mOpenedAtPlaceholder = ! mDefaultKnown;
        const auto sz = mDefaultKnown ? mDefaultSize
                                      : juce::Point<int> { 480, 320 };
        saved = juce::Rectangle<int> (24 + step, 24 + step, sz.x, sz.y);
    }
    else if (! hasPos)
        saved.setPosition (24 + step, 24 + step);
    // Never reopen below the window's own minimum.  When the default is not yet
    // known the constrainer still holds the anti-degenerate clamp, and
    // setDefaultWindowSize raises the window when the real number lands.
    saved.setSize (juce::jmax (saved.getWidth(),  mConstrainer.getMinimumWidth()),
                   juce::jmax (saved.getHeight(), mConstrainer.getMinimumHeight()));
    setBounds (saved);

    addToDesktop (0, parent);
    const auto o = ws.originInParentClient();
    setTopLeftPosition (saved.getX() + o.x, saved.getY() + o.y);

    // LAND ON SCREEN (Jeff, 2026-08-05).  attachTo was the one path that never
    // checked a window's opening position -- clampWindowsIntoView runs only from
    // Workspace::resized(), so a window opened after the last workspace layout
    // was placed verbatim and never looked at.
    //
    // TWO GUARDS, both learned the hard way when the first attempt shipped
    // without either and had to be reverted:
    //
    //  1. SAVES SUPPRESSED.  This runs AFTER addToDesktop, so setTopLeftPosition
    //     reaches moved() -> saveBounds().  Without the suppression the clamped
    //     position is written into the store as though the user had chosen it,
    //     and the real placement is destroyed on the spot.
    //  2. POSITION ONLY.  Never resize to fit; a window is allowed to exceed the
    //     workspace, it just may not START outside it.
    //
    // The partial-workspace case is covered by the machinery that already
    // exists rather than by testing for it here: the frame passes through a
    // smaller size on its way to full, this clamp pulls windows in against it,
    // and then Workspace::resized() sees a GROWING workspace and calls
    // restoreWindowsToSavedBounds() -- which is only able to put them back
    // because guard 1 kept the store clean.
    {
        const ScopedSaveSuppress noSave;
        setBounds (clampPositionToWorkspace (getBounds()));
    }

    setVisible (true);
    toFront (true);
}

void WorkspaceWindow::applySavedBounds()
{
    auto* ws = workspace();
    if (ws == nullptr || ws->getWidth() <= 0 || ws->getHeight() <= 0) return;

    bool hasPos = true;
    const auto saved = loadSavedBounds (hasPos);
    if (saved.isEmpty() || ! hasPos) return;

    const auto o = ws->originInParentClient();
    setBounds (saved.getX() + o.x, saved.getY() + o.y,
               juce::jmax (saved.getWidth(),  mConstrainer.getMinimumWidth()),
               juce::jmax (saved.getHeight(), mConstrainer.getMinimumHeight()));
}

juce::Rectangle<int> WorkspaceWindow::clampToWorkspace (juce::Rectangle<int> target) const
{
    auto* ws = workspace();
    if (ws == nullptr || ws->getWidth() <= 0 || ws->getHeight() <= 0)
        return target;

    const int wsW = ws->getWidth();
    const int wsH = ws->getHeight();

    // Never below the resize floor, never bigger than the workspace.  Order
    // matters: the floor is applied AFTER the fit, so a small workspace cannot
    // squeeze a window into a negative content area (that crashed the first
    // slider that painted into it).
    const int minW = juce::jmax (1, mConstrainer.getMinimumWidth());
    const int minH = juce::jmax (1, mConstrainer.getMinimumHeight());
    target.setSize (juce::jmax (minW, juce::jmin (target.getWidth(),  wsW)),
                    juce::jmax (minH, juce::jmin (target.getHeight(), wsH)));

    // Containment is computed as an OFFSET FROM THE WINDOW'S CURRENT POSITION,
    // measured in screen space (Jeff, 2026-07-28: a window could reach the top
    // of the workspace but stopped short of the bottom).
    //
    // Deriving the workspace origin in parent-client space and comparing
    // against it needs BOTH conversions to be exactly right, and one of them
    // was not.  Screen space is the one frame both rectangles can be stated in
    // without any assumption about peer coordinate conventions -- and since
    // screen and parent-client differ only by a translation, an offset computed
    // in one is valid in the other.
    // The window has not MOVED yet -- `target` is where it wants to go -- so the
    // desired screen rect is the current one shifted by the same delta.
    const auto moveDelta = target.getPosition() - getBounds().getPosition();
    const auto wsScreen  = ws->getScreenBounds();
    const auto winScreen = getScreenBounds().translated (moveDelta.x, moveDelta.y)
                                            .withSize (target.getWidth(), target.getHeight());

    int dx = 0, dy = 0;
    if (winScreen.getX() < wsScreen.getX())            dx = wsScreen.getX() - winScreen.getX();
    else if (winScreen.getRight() > wsScreen.getRight()) dx = wsScreen.getRight() - winScreen.getRight();
    if (winScreen.getY() < wsScreen.getY())            dy = wsScreen.getY() - winScreen.getY();
    else if (winScreen.getBottom() > wsScreen.getBottom()) dy = wsScreen.getBottom() - winScreen.getBottom();

    return target.translated (dx, dy);
}

juce::Rectangle<int> WorkspaceWindow::clampPositionToWorkspace (juce::Rectangle<int> target) const
{
    // clampToWorkspace WITHOUT the size fit.  The size fit is what made the
    // first drag of an oversized window snap it to the workspace width; landing
    // a window on screen needs only the move half.
    auto* ws = workspace();
    if (ws == nullptr || ws->getWidth() <= 0 || ws->getHeight() <= 0)
        return target;

    const auto moveDelta = target.getPosition() - getBounds().getPosition();
    const auto wsScreen  = ws->getScreenBounds();
    const auto winScreen = getScreenBounds().translated (moveDelta.x, moveDelta.y)
                                            .withSize (target.getWidth(), target.getHeight());

    // Left/top win when the window is BIGGER than the workspace: that corner
    // holds the title bar, so it is the one that has to stay grabbable.
    int dx = 0, dy = 0;
    if (winScreen.getX() < wsScreen.getX())               dx = wsScreen.getX() - winScreen.getX();
    else if (winScreen.getRight() > wsScreen.getRight())  dx = juce::jmax (wsScreen.getX() - winScreen.getX(),
                                                                           wsScreen.getRight() - winScreen.getRight());
    if (winScreen.getY() < wsScreen.getY())               dy = wsScreen.getY() - winScreen.getY();
    else if (winScreen.getBottom() > wsScreen.getBottom()) dy = juce::jmax (wsScreen.getY() - winScreen.getY(),
                                                                            wsScreen.getBottom() - winScreen.getBottom());

    return target.translated (dx, dy);
}

void WorkspaceWindow::Constrainer::applyBoundsToComponent (juce::Component& c,
                                                           juce::Rectangle<int> bounds)
{
    // L5: a manual resize leaves the filled state (same rule as the drag).
    // Only user gestures route through here (ResizableBorderComponent);
    // programmatic setBounds does not.
    if (owner.mFilled)
    {
        owner.mFilled = false;
        if (owner.mFullBtn) owner.mFullBtn->repaint();
    }
    // Jeff, 2026-08-04: the magnet works when MOVING a window but did nothing
    // when resizing one, because applyMagnetism was only ever called from
    // mouseDrag.  Snap first, then contain -- same order as the drag path.
    const auto snapped = owner.applyResizeMagnetism (bounds);
    juce::ComponentBoundsConstrainer::applyBoundsToComponent (c, owner.clampResizeToWorkspace (snapped));
}

// Edge-independent magnetism, which is what a RESIZE needs.  The move path's
// applyMagnetism finds one dx/dy and TRANSLATES the whole window -- correct for
// a drag, wrong here: dragging the right edge must not haul the left edge along.
// This snaps each edge on its own and only considers edges that actually MOVED
// (compared against the current bounds), so the edges the user is not dragging
// are left exactly where they are.
juce::Rectangle<int> WorkspaceWindow::applyResizeMagnetism (juce::Rectangle<int> target) const
{
    auto* ws = workspace();
    if (ws == nullptr) return target;

    const auto cur = getBounds();

    std::vector<int> xs, ys;
    auto addRect = [&] (juce::Rectangle<int> r)
    {
        xs.push_back (r.getX()); xs.push_back (r.getRight());
        ys.push_back (r.getY()); ys.push_back (r.getBottom());
    };

    // Same candidate set the drag path magnetises against: every sibling plus
    // the workspace itself, all in this window's own coordinate space.
    for (auto& sp : ws->getWindows())
        if (auto* w = sp.getComponent())
            if (w != this && w->isVisible())
                addRect (w->getBounds());
    {
        const auto wsLocal = ws->getBounds();
        const auto o       = ws->originInParentClient();
        addRect (juce::Rectangle<int> (o.x, o.y, wsLocal.getWidth(), wsLocal.getHeight()));
    }

    auto nearest = [] (const std::vector<int>& cands, int edge, int& out) -> bool
    {
        int bestDist = kSnapPx + 1;
        for (int c : cands)
        {
            const int d = std::abs (c - edge);
            if (d <= kSnapPx && d < bestDist) { bestDist = d; out = c; }
        }
        return bestDist <= kSnapPx;
    };

    int l = target.getX(), r = target.getRight();
    int t = target.getY(), b = target.getBottom();
    int v = 0;

    if (l != cur.getX()      && nearest (xs, l, v)) l = v;
    if (r != cur.getRight()  && nearest (xs, r, v)) r = v;
    if (t != cur.getY()      && nearest (ys, t, v)) t = v;
    if (b != cur.getBottom() && nearest (ys, b, v)) b = v;

    // A snap must never invert the rectangle or breach the anti-degenerate
    // clamp; the dragged edge gives way rather than the window folding.
    if (r - l < kMinDegenerateW) r = l + kMinDegenerateW;
    if (b - t < kMinDegenerateH) b = t + kMinDegenerateH;

    return { l, t, r - l, b - t };
}

juce::Rectangle<int> WorkspaceWindow::clampResizeToWorkspace (juce::Rectangle<int> target) const
{
    auto* ws = workspace();
    if (ws == nullptr || ws->getWidth() <= 0 || ws->getHeight() <= 0)
        return target;

    // Screen space, for the same reason clampToWorkspace uses it: it is the one
    // frame both rectangles can be stated in without depending on the child-peer
    // coordinate convention, and it differs from the caller's space only by a
    // translation -- so an offset computed here is valid there.
    const auto wsScreen  = ws->getScreenBounds();
    const auto moveDelta = target.getPosition() - getBounds().getPosition();
    const auto winScreen = getScreenBounds().translated (moveDelta.x, moveDelta.y)
                                            .withSize (target.getWidth(), target.getHeight());

    // Every bound below is built with an explicit jmax/jmin pair rather than
    // jlimit: a workspace smaller than this window's own floor would hand jlimit
    // an inverted range, which is exactly the crash this file already hit once.
    const int left   = juce::jmax (winScreen.getX(),      wsScreen.getX());
    const int top    = juce::jmax (winScreen.getY(),      wsScreen.getY());
    const int right  = juce::jmin (winScreen.getRight(),  wsScreen.getRight());
    const int bottom = juce::jmin (winScreen.getBottom(), wsScreen.getBottom());

    // The floor wins over the trim, and the workspace wins over the floor -- so
    // a window can never be squeezed to a negative content area, and never ends
    // up larger than the region that contains it.
    const int minW = juce::jmax (1, mConstrainer.getMinimumWidth());
    const int minH = juce::jmax (1, mConstrainer.getMinimumHeight());
    const int w = juce::jmin (juce::jmax (minW, right - left),  wsScreen.getWidth());
    const int h = juce::jmin (juce::jmax (minH, bottom - top), wsScreen.getHeight());

    const int x = juce::jmax (wsScreen.getX(), juce::jmin (left, wsScreen.getRight()  - w));
    const int y = juce::jmax (wsScreen.getY(), juce::jmin (top,  wsScreen.getBottom() - h));

    const auto offset = juce::Point<int> (x, y) - winScreen.getPosition();
    return target.translated (offset.x, offset.y).withSize (w, h);
}

juce::Rectangle<int> WorkspaceWindow::applyMagnetism (juce::Rectangle<int> target) const
{
    auto* ws = workspace();
    if (ws == nullptr) return target;

    // Everything is compared in the window's own coordinate space, which every
    // sibling shares (they are all children of the same parent), so no
    // conversion is involved.
    int bestDx = 0, bestDistX = kSnapPx + 1;
    int bestDy = 0, bestDistY = kSnapPx + 1;

    auto considerX = [&] (int targetEdge, int otherEdge)
    {
        const int d = otherEdge - targetEdge;
        if (std::abs (d) <= kSnapPx && std::abs (d) < bestDistX)
        { bestDistX = std::abs (d); bestDx = d; }
    };
    auto considerY = [&] (int targetEdge, int otherEdge)
    {
        const int d = otherEdge - targetEdge;
        if (std::abs (d) <= kSnapPx && std::abs (d) < bestDistY)
        { bestDistY = std::abs (d); bestDy = d; }
    };

    auto snapAgainst = [&] (juce::Rectangle<int> r)
    {
        // Flush-to-flush (my left to their right, etc.) and aligned edges.
        considerX (target.getX(),     r.getRight());
        considerX (target.getRight(), r.getX());
        considerX (target.getX(),     r.getX());
        considerX (target.getRight(), r.getRight());

        considerY (target.getY(),      r.getBottom());
        considerY (target.getBottom(), r.getY());
        considerY (target.getY(),      r.getY());
        considerY (target.getBottom(), r.getBottom());
    };

    for (auto& sp : ws->getWindows())
        if (auto* w = sp.getComponent())
            if (w != this && w->isVisible())
                snapAgainst (w->getBounds());

    // The workspace's own edges magnetise too, so a window can be flushed into
    // a corner as easily as against a neighbour.
    {
        auto wsLocal = ws->getBounds();
        const auto o = ws->originInParentClient();
        snapAgainst (juce::Rectangle<int> (o.x, o.y, wsLocal.getWidth(), wsLocal.getHeight()));
    }

    return target.translated (bestDx, bestDy);
}

std::map<juce::String, juce::Rectangle<int>>& WorkspaceWindow::sessionBounds()
{
    static std::map<juce::String, juce::Rectangle<int>> m;
    return m;
}

std::set<juce::String>& WorkspaceWindow::diskEligibleKeys()
{
    static std::set<juce::String> s;
    return s;
}

std::set<juce::String>& WorkspaceWindow::placementKeys()
{
    static std::set<juce::String> s;
    return s;
}

const std::map<juce::String, juce::Rectangle<int>>& WorkspaceWindow::sessionBoundsMap()
{
    return sessionBounds();
}

void WorkspaceWindow::replaceSessionBounds (std::map<juce::String, juce::Rectangle<int>> m)
{
    sessionBounds() = std::move (m);
}

void WorkspaceWindow::registerPlacementPersistentKey (const juce::String& key)
{
    if (key.isNotEmpty()) placementKeys().insert (key);
}

bool WorkspaceWindow::isGlobalPlacementKey (const juce::String& key)
{
    return placementKeys().count (key) > 0;
}

int WorkspaceWindow::sSaveSuppressed = 0;

void WorkspaceWindow::seedGlobalPlacementKeys (const juce::StringArray& keys)
{
    for (const auto& k : keys)
        if (k.isNotEmpty()) placementKeys().insert (k);
}

juce::Rectangle<int> WorkspaceWindow::loadSavedBounds (bool& outHasPosition) const
{
    outHasPosition = true;

    // Lifetime 1 first: close/reopen returns to the same spot for EVERY
    // window, players included -- no store rule qualifies this.
    {
        auto& m = sessionBounds();
        auto it = m.find (mPersistKey);
        if (it != m.end()) return it->second;
    }
    if (mPersistence == Persistence::Session) return {};

    // Map miss on a Disk window: seed from settings.xml (the layout the last
    // exit flushed).  A record without x/y is a SIZE-ONLY seed -- player
    // placement is filtered out of the global file by design.
    auto xml = juce::XmlDocument::parse (ProjectManager::getSettingsFile());
    if (! xml) return {};
    auto* root = xml->getChildByName (kRootTag);
    if (root == nullptr) return {};

    for (auto* w : root->getChildWithTagNameIterator (kWindowTag))
    {
        if (w->getStringAttribute ("key") != mPersistKey) continue;
        outHasPosition = w->hasAttribute ("x") && w->hasAttribute ("y");
        return { w->getIntAttribute ("x"), w->getIntAttribute ("y"),
                 w->getIntAttribute ("w"), w->getIntAttribute ("h") };
    }
    return {};
}

void WorkspaceWindow::saveBounds() const
{
    // A programmatic clamp is not a placement the user chose -- see
    // ScopedSaveSuppress.
    if (savesSuppressed()) return;
    if (mPersistKey.isEmpty()) return;
    // No workspace = no way to convert into workspace-local space; a raw save
    // here would be the coordinate-space drift bug again.  Keep the last good
    // save instead.
    if (workspace() == nullptr) return;

    // Lifetime 1: the map is the one live store.  Workspace-local, not
    // parent-client -- storing raw peer bounds re-added the workspace origin
    // on every close/reopen, so windows crept down-right each cycle; and the
    // workspace moves when the main chrome changes height.
    auto b = getBounds();
    if (auto* ws = workspace())
    {
        const auto o = ws->originInParentClient();
        b.translate (-o.x, -o.y);
    }
    sessionBounds()[mPersistKey] = b;

    // Disk windows flush to settings.xml ONCE, at app exit
    // (writeSessionToSettings) -- the per-close parse-and-rewrite of the
    // whole file is gone (T5).
    if (mPersistence == Persistence::Disk)
        diskEligibleKeys().insert (mPersistKey);
}

void WorkspaceWindow::writeSessionToSettings()
{
    if (diskEligibleKeys().empty()) return;

    const auto file = ProjectManager::getSettingsFile();
    auto xml = juce::XmlDocument::parse (file);
    // MUST match the tag every other settings writer uses (ProjectManager
    // :605).  Inventing a different root here would produce a settings.xml
    // that parses but that no other reader recognises.
    if (! xml) xml = std::make_unique<juce::XmlElement> ("BaySickDAWSettings");

    auto* root = xml->getChildByName (kRootTag);
    if (root == nullptr) root = xml->createNewChildElement (kRootTag);

    auto& m = sessionBounds();
    for (const auto& key : diskEligibleKeys())
    {
        auto it = m.find (key);
        if (it == m.end()) continue;

        juce::XmlElement* rec = nullptr;
        for (auto* w : root->getChildWithTagNameIterator (kWindowTag))
            if (w->getStringAttribute ("key") == key) { rec = w; break; }
        if (rec == nullptr)
        {
            rec = root->createNewChildElement (kWindowTag);
            rec->setAttribute ("key", key);
        }

        // Jeff, 2026-08-04: size AND position, for the four default tabs only.
        // Only they are ever Disk-marked now, so reaching this loop already
        // means "one of the four".  The old code wrote SIZE for every window
        // and gated only POSITION -- the inverse of the ruling, and the reason
        // a player's size survived a cold start when it should have reverted
        // to its default.
        rec->setAttribute ("w", it->second.getWidth());
        rec->setAttribute ("h", it->second.getHeight());
        if (placementKeys().count (key) > 0)
        {
            rec->setAttribute ("x", it->second.getX());
            rec->setAttribute ("y", it->second.getY());
        }
        else
        {
            rec->removeAttribute ("x");
            rec->removeAttribute ("y");
        }
    }
    xml->writeTo (file);
}

// ─────────────────────────────────────────────────────────────────────────────
Workspace::Workspace()
{
    setOpaque (true);
}

void Workspace::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF141417));
}

void Workspace::restoreWindowsToSavedBounds()
{
    const WorkspaceWindow::ScopedSaveSuppress noSave;
    for (auto& sp : mWindows)
        if (auto* w = sp.getComponent())
            if (w->isOnDesktop())
                w->applySavedBounds();
}

void Workspace::resized()
{
    // ALSO drive the pending queue here, not just from parentHierarchyChanged.
    // The first window is created in StandaloneEditor's CONSTRUCTOR, before the
    // frame has a peer, so it goes on the queue.  parentHierarchyChanged can
    // fire while the peer STILL does not exist -- and if it never fires again,
    // that window stays queued forever: no peer, never shown, and its
    // mWorkspace reference never completes.  resized() runs once the frame is
    // laid out and on screen, by which point the handle is real.  That is
    // exactly the "first window does not pop up, later ones do" report -- later
    // windows attach immediately because the peer already exists by then.
    attachPendingWindows();

    // The workspace GREW -- the frame finishing its layout at launch, or the
    // user enlarging it.  Windows were clamped into whatever size the
    // workspace measured before, so put them back where they were saved
    // BEFORE clamping again; otherwise a launch-time partial size (the frame
    // passes through ~1098x608 on its way to full size) permanently pulls
    // every window up and in.  Restoring is not a placement, so it does not
    // touch the store.
    const juce::Point<int> now { getWidth(), getHeight() };
    if (now.x > mLastSize.x || now.y > mLastSize.y)
        restoreWindowsToSavedBounds();
    mLastSize = now;

    clampWindowsIntoView();
}

void* Workspace::getNativeParentHandle() const
{
    // A Workspace has no peer of its own -- getPeer() resolves to the frame
    // that contains it, which is exactly the HWND contained windows parent to.
    if (auto* peer = getPeer())
        return peer->getNativeHandle();
    return nullptr;
}

juce::Point<int> Workspace::originInParentClient() const
{
    if (auto* peer = getPeer())
    {
        auto& frame = peer->getComponent();
        if (&frame != this)
        {
            // The frame's origin expressed in OUR space; negating it gives our
            // position in the frame's space, which is what the child peers'
            // parent-client coordinates are measured from.
            const auto frameOrigin = getLocalPoint (&frame, juce::Point<int> (0, 0));
            return { -frameOrigin.x, -frameOrigin.y };
        }
    }
    return {};
}

void Workspace::parentHierarchyChanged()
{
    attachPendingWindows();
}

void Workspace::queueForAttach (WorkspaceWindow* w)
{
    if (w == nullptr) return;
    for (auto& sp : mPendingAttach) if (sp.getComponent() == w) return;
    mPendingAttach.add (juce::Component::SafePointer<WorkspaceWindow> (w));
}

void Workspace::attachPendingWindows()
{
    if (mPendingAttach.isEmpty()) return;

    // No peer yet -- the queue stays put and is retried from
    // parentHierarchyChanged / resized.
    if (getNativeParentHandle() == nullptr)
        return;

    auto pending = mPendingAttach;
    mPendingAttach.clear();
    for (auto& sp : pending)
        if (auto* w = sp.getComponent())
            w->attachTo (*this);
}

void Workspace::addWindow (WorkspaceWindow* w)
{
    if (w == nullptr) return;
    for (auto& sp : mWindows) if (sp.getComponent() == w) return;
    mWindows.add (juce::Component::SafePointer<WorkspaceWindow> (w));
}

void Workspace::removeWindow (WorkspaceWindow* w)
{
    auto drop = [w] (juce::Array<juce::Component::SafePointer<WorkspaceWindow>>& a)
    {
        for (int i = a.size(); --i >= 0;)
        {
            auto* c = a.getReference (i).getComponent();
            if (c == w || c == nullptr) a.remove (i);   // also reaps dead entries
        }
    };
    drop (mWindows);
    drop (mPendingAttach);
}

void Workspace::clampWindowsIntoView()
{
    if (getWidth() <= 0 || getHeight() <= 0) return;

    // The clamp is OURS, not the user's: keep it out of the bounds store so a
    // mid-layout workspace size can't overwrite hand-placed windows.
    const WorkspaceWindow::ScopedSaveSuppress noSave;

    for (auto& sp : mWindows)
    {
        auto* w = sp.getComponent();
        if (w == nullptr || ! w->isOnDesktop()) continue;

        // One containment rule, shared with the drag path so the two cannot
        // disagree about where a window is allowed to be.
        w->setBounds (w->clampToWorkspace (w->getBounds()));
    }
}
