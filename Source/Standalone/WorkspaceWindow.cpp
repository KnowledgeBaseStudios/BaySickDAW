#include "WorkspaceWindow.h"
#include "SharedUI.h"   // VibeLAF + PageMenuBar
#include "../ProjectManager.h"

namespace
{
    // Contained-window bounds live alongside the main frame's own WindowState
    // in settings.xml, so window layout is a global preference rather than
    // project data -- the user's arrangement of their workspace should not
    // change when they open a different song.
    constexpr const char* kRootTag   = "WorkspaceWindows";
    constexpr const char* kWindowTag = "W";

    juce::Colour kFrameBg     { 0xFF1B1B1F };
    juce::Colour kTitleBg     { 0xFF2A2A31 };
    juce::Colour kTitleBgLive { 0xFF34343D };
    juce::Colour kTitleText   { 0xFFE8E8EE };
    juce::Colour kFrameEdge   { 0xFF000000 };
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

    // Locked call 5a: close + resize only.  No minimise -- there is nowhere for
    // a minimised window to go inside a contained frame, and no maximise --
    // the tab bar is how you get a window back.
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

    // Tooltips: see the member comment -- one per window, 600 ms like the
    // editor's, so hover help works the same inside every contained window.
    mTooltips = std::make_unique<VibeTooltip> (this, 600);

    mConstrainer.setMinimumSize (320, 200);
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
    auto* ws = mWorkspace.getComponent();
    if (ws == nullptr && mAttachAttempted && ! mReportedDeadWorkspace)
    {
        mReportedDeadWorkspace = true;
        DBG ("[TS4 SHELL] WorkspaceWindow '" << mPersistKey
             << "' outlived its Workspace -- containment/magnetism off for it.");
    }
    return ws;
}

WorkspaceWindow::~WorkspaceWindow()
{
    if (mCloseBtn) mCloseBtn->setLookAndFeel (nullptr);
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

void WorkspaceWindow::setMinimumSize (int minW, int minH)
{
    // The floor covers the whole frame, so the PAGE's minimum has to grow by
    // the chrome around it or the page would still be squeezed below its own
    // collision point.
    mConstrainer.setMinimumSize (juce::jmax (120, minW + 2 * kBorderPx),
                                 juce::jmax (80,  minH + kTitleH + 2 * kBorderPx));
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
    g.fillAll (kFrameBg);

    const bool live = isMouseOverOrDragging (true)
                      || (mContentRaw != nullptr && mContentRaw->hasKeyboardFocus (true));
    auto title = titleBarArea();
    g.setColour (live ? kTitleBgLive : kTitleBg);
    g.fillRect (title);

    g.setColour (kFrameEdge);
    g.drawRect (getLocalBounds(), 1);
}

// QA-ModelShell TS6 (Jeff 2026-07-29): a hosted plugin brings its own surface
// at its own size, so the window is sized to THAT rather than left at a
// provisional floor with dead space around the plugin.  The inverse of
// contentBounds(): add the chrome back on.
//
// Clamped to the workspace so a plugin with a huge editor cannot open a window
// bigger than the frame containing it -- and the floor is applied after the
// clamp, matching clampResizeToWorkspace's precedence (floor over trim,
// workspace over floor) so a negative content area can never be produced.
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
    if (mResizer)  mResizer->setBounds (getLocalBounds());

    auto title = titleBarArea();
    if (mCloseBtn)
        mCloseBtn->setBounds (title.removeFromRight (kTitleH).reduced (4));
    if (mPageMenu)
        mPageMenu->setBounds (title);

    if (mContentRaw) mContentRaw->setBounds (contentArea());
}

void WorkspaceWindow::mouseDown (const juce::MouseEvent& e)
{
    mDraggingTitle   = titleBarArea().contains (e.getPosition());
    mDragStartBounds = getBounds();
    mDragStartScreen = e.getScreenPosition();

    // Both the clamp and the magnet fail SILENTLY (each early-returns its input),
    // so which guard tripped cannot be recovered from the symptom.
    if (mDraggingTitle)
    {
        if (auto* ws = workspace())
            DBG ("[TS4 SHELL] drag '" << mPersistKey << "' wsSize=" << ws->getWidth() << "x" << ws->getHeight()
                 << " wsScreen=" << ws->getScreenBounds().toString()
                 << " winScreen=" << getScreenBounds().toString()
                 << " siblings=" << ws->getWindows().size());
        else
            DBG ("[TS4 SHELL] drag '" << mPersistKey << "' NO WORKSPACE -- clamp and magnet both off.");
    }

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
    const auto snapped     = applyMagnetism (desired);
    const auto finalBounds = clampToWorkspace (snapped);

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

void WorkspaceWindow::mouseUp (const juce::MouseEvent&)
{
    if (! mDraggingTitle) return;
    mDraggingTitle = false;
    mWarpsThisDrag = 0;
    saveBounds();
}

void WorkspaceWindow::attachTo (Workspace& ws)
{
    mAttachAttempted = true;
    mWorkspace = &ws;
    ws.addWindow (this);

    // Diagnostic (2026-07-28): the log proved the pending QUEUE is never used
    // -- neither queue message ever printed -- yet windows created at page
    // build time behave as though mWorkspace is null (no clamp, no magnet,
    // first window never shows), while windows created on REOPEN work.  Since
    // mWorkspace is only ever assigned here, exactly one of two assumptions is
    // false: either attachTo is not running for those windows, or it IS
    // running but with a parent handle that is somehow non-null this early.
    // This line distinguishes them in a single run.
    DBG ("[TS4 SHELL] attachTo '" << mPersistKey << "' parentHandle="
         << (ws.getNativeParentHandle() != nullptr ? "OK" : "NULL")
         << " wsSize=" << ws.getWidth() << "x" << ws.getHeight());

    auto saved = loadSavedBounds();
    if (saved.isEmpty())
    {
        // First open: a readable default inset from the workspace origin, offset
        // per already-open window so a fresh window never lands exactly on top
        // of the one before it.
        const int step = 28 * juce::jmin (6, ws.getWindows().size() - 1);
        saved = juce::Rectangle<int> (24 + step, 24 + step,
                                      juce::jmax (480, ws.getWidth()  * 2 / 3),
                                      juce::jmax (320, ws.getHeight() * 2 / 3));
    }
    setBounds (saved);

    // Parent-client space: the peer's coordinates are measured from the MAIN
    // window's client origin, so the workspace's own offset inside the frame
    // has to be added on.  (See the contract note in the header -- this is the
    // one line that differs from how a top-level JUCE window is positioned.)
    auto* parent = ws.getNativeParentHandle();
    if (parent == nullptr)
    {
        // No peer yet -- the editor builds its pages in its constructor, before
        // it is handed to the window.  Queue and attach once the frame exists.
        ws.queueForAttach (this);
        return;
    }

    addToDesktop (0, parent);
    const auto o = ws.originInParentClient();
    setTopLeftPosition (saved.getX() + o.x, saved.getY() + o.y);
    setVisible (true);
    toFront (true);
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

void WorkspaceWindow::Constrainer::applyBoundsToComponent (juce::Component& c,
                                                           juce::Rectangle<int> bounds)
{
    juce::ComponentBoundsConstrainer::applyBoundsToComponent (c, owner.clampResizeToWorkspace (bounds));
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

juce::Rectangle<int> WorkspaceWindow::loadSavedBounds() const
{
    if (mPersistence == Persistence::Session)
    {
        auto& m = sessionBounds();
        auto it = m.find (mPersistKey);
        return it != m.end() ? it->second : juce::Rectangle<int>();
    }

    auto xml = juce::XmlDocument::parse (ProjectManager::getSettingsFile());
    if (! xml) return {};
    auto* root = xml->getChildByName (kRootTag);
    if (root == nullptr) return {};

    for (auto* w : root->getChildWithTagNameIterator (kWindowTag))
    {
        if (w->getStringAttribute ("key") != mPersistKey) continue;
        return { w->getIntAttribute ("x"), w->getIntAttribute ("y"),
                 w->getIntAttribute ("w"), w->getIntAttribute ("h") };
    }
    return {};
}

void WorkspaceWindow::saveBounds() const
{
    if (mPersistKey.isEmpty()) return;

    if (mPersistence == Persistence::Session)
    {
        sessionBounds()[mPersistKey] = getBounds();
        return;
    }

    const auto file = ProjectManager::getSettingsFile();
    auto xml = juce::XmlDocument::parse (file);
    // MUST match the tag every other settings writer uses (ProjectManager
    // :605).  Inventing a different root here would produce a settings.xml
    // that parses but that no other reader recognises.
    if (! xml) xml = std::make_unique<juce::XmlElement> ("BaySickDAWSettings");

    auto* root = xml->getChildByName (kRootTag);
    if (root == nullptr) root = xml->createNewChildElement (kRootTag);

    juce::XmlElement* rec = nullptr;
    for (auto* w : root->getChildWithTagNameIterator (kWindowTag))
        if (w->getStringAttribute ("key") == mPersistKey) { rec = w; break; }
    if (rec == nullptr)
    {
        rec = root->createNewChildElement (kWindowTag);
        rec->setAttribute ("key", mPersistKey);
    }

    // Store WORKSPACE-LOCAL bounds, not the peer's parent-client bounds: the
    // workspace moves when the main chrome changes height, and a stored
    // parent-client position would drift by that delta on the next run.
    auto b = getBounds();
    if (auto* ws2 = workspace())
    {
        const auto o = ws2->originInParentClient();
        b.translate (-o.x, -o.y);
    }
    rec->setAttribute ("x", b.getX());
    rec->setAttribute ("y", b.getY());
    rec->setAttribute ("w", b.getWidth());
    rec->setAttribute ("h", b.getHeight());

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

    if (getNativeParentHandle() == nullptr)
    {
        // Deliberately NOT silent: a queue that never drains is the failure
        // this whole path exists to avoid, and it is invisible from the UI.
        DBG ("[TS4 SHELL] " << mPendingAttach.size()
             << " window(s) still queued -- frame has no peer yet.");
        return;
    }

    auto pending = mPendingAttach;
    mPendingAttach.clear();
    for (auto& sp : pending)
        if (auto* w = sp.getComponent())
        {
            DBG ("[TS4 SHELL] attaching queued window '" << w->getPersistKey() << "'");
            w->attachTo (*this);
        }
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

    for (auto& sp : mWindows)
    {
        auto* w = sp.getComponent();
        if (w == nullptr || ! w->isOnDesktop()) continue;

        // One containment rule, shared with the drag path so the two cannot
        // disagree about where a window is allowed to be.
        w->setBounds (w->clampToWorkspace (w->getBounds()));
    }
}
