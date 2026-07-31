#pragma once
#include <JuceHeader.h>

// -- WindowChrome -- QA-ModelShell TS7 §9.1 -------------------------------------
// ONE source of truth for how a floating window's frame and title strip LOOK.
//
// Before this, the look lived inline in WorkspaceWindow::paint while every
// desktop window (Event Editor, Key Binds, Undo History, Plugins, Rusty Drums
// Map, Pitch Sub-Editor, and the four dialogs) drew either the OS title bar or
// stock LookAndFeel_V4 chrome.  Two looks in one app, and any change to the
// shell's would have had to be re-made by hand everywhere else -- which is the
// drift this extraction exists to prevent.
//
// It is a set of PAINT HELPERS, not a Component, on purpose.  The two hosts
// cannot share a component: WorkspaceWindow owns its strip directly (PageMenuBar
// + close button as children), while juce::DocumentWindow builds and positions
// its own title bar and asks its LookAndFeel to draw it.  Sharing the pixels
// and letting each host keep its own plumbing is the only split that actually
// holds; a shared Component would have meant reimplementing DocumentWindow.
//
// SCOPE (§9.6): the base only.  What GOES IN a title strip -- preset dropdowns,
// engine pickers -- is the layout batch's call, not this one's.
// ------------------------------------------------------------------------------
namespace WindowChrome
{
    // Locked call 4a.  kBorderPx is the resize-drag thickness the contained
    // shell uses; desktop windows keep their own JUCE border.
    inline constexpr int kTitleH   = 26;
    inline constexpr int kBorderPx = 4;

    juce::Colour frameBg()   noexcept;
    juce::Colour titleBg (bool live) noexcept;
    juce::Colour titleText() noexcept;
    juce::Colour frameEdge() noexcept;

    // The window body behind everything, plus the 1px edge that separates one
    // floating window from the one under it.
    void paintFrame (juce::Graphics&, juce::Rectangle<int> full);

    // The title strip itself.  `live` is the focused/hovered state -- the shell
    // and the desktop windows compute it differently (mouse-over plus content
    // focus vs the peer's active state), so it is passed in rather than derived.
    // An empty title draws no text, which is what the shell wants: its
    // PageMenuBar draws the title as part of the menu row.
    void paintTitleBar (juce::Graphics&, juce::Rectangle<int> area,
                        bool live, const juce::String& title = {});

    // Puts a desktop window on the shared chrome: non-native title bar (so the
    // VibeLAF overrides paint it) AND the shared strip height.
    //
    // The height call is the load-bearing half.  Without it a DocumentWindow uses
    // JUCE's own titleBarHeight, which is also 26 -- so the shell and the desktop
    // windows matched by COINCIDENCE, and changing kTitleH here would have moved
    // one and left the other. Routing both through this makes §9.1's "one source
    // of truth" true of the metric as well as the paint.
    void applyToDesktopWindow (juce::DocumentWindow& win);

    // -- TS7 §9.4 ---------------------------------------------------------------
    // Makes `win` an OWNED window of the main application window, where
    // `insideMain` is any component living in that main window.  Call it AFTER
    // the window is visible -- it needs a live peer.
    //
    // This is the actual fix for what setAlwaysOnTop was papering over.  A plain
    // top-level window loses to the main window the instant the main window takes
    // focus back -- which the editor does on purpose (deferred grabKeyboardFocus)
    // -- so a satellite would vanish behind it the moment it opened.  Always-on-
    // top cured that by floating the satellite above EVERY application, including
    // whatever the user alt-tabbed to, which is a worse bug than the one it fixed.
    //
    // An owned window is kept above its owner BY THE OS and above nothing else,
    // and it minimises with the owner.  That is exactly the relationship wanted.
    void ownToMainWindow (juce::Component& win, const juce::Component& insideMain);
}
