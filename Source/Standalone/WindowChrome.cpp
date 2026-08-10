#include "WindowChrome.h"

#if JUCE_WINDOWS
 #include <windows.h>   // GWLP_HWNDPARENT -- see ownToMainWindow
#endif

namespace WindowChrome
{
    namespace
    {
        // Moved here from WorkspaceWindow.cpp's anonymous namespace -- that was
        // the original and only definition, so this is a relocation, not a
        // second palette.
        const juce::Colour kFrameBg     { 0xFF1B1B1F };
        const juce::Colour kTitleBg     { 0xFF2A2A31 };
        const juce::Colour kTitleBgLive { 0xFF34343D };
        const juce::Colour kTitleText   { 0xFFE8E8EE };
        const juce::Colour kFrameEdge   { 0xFF000000 };
    }

    juce::Colour titleText() noexcept { return kTitleText; }
    juce::Colour titleBg (bool live) noexcept { return live ? kTitleBgLive : kTitleBg; }

    void paintFrame (juce::Graphics& g, juce::Rectangle<int> full)
    {
        g.fillAll (kFrameBg);
        g.setColour (kFrameEdge);
        g.drawRect (full, 1);
    }

    void paintTitleBar (juce::Graphics& g, juce::Rectangle<int> area,
                        bool live, const juce::String& title)
    {
        g.setColour (titleBg (live));
        g.fillRect (area);

        if (title.isEmpty()) return;

        g.setColour (kTitleText);
        g.setFont (juce::Font (13.0f));
        // Left-aligned with the same 8px inset the PageMenuBar uses, so a
        // desktop window's title sits where a contained window's does.
        g.drawText (title, area.reduced (8, 0), juce::Justification::centredLeft,
                    true);
    }

    void applyToDesktopWindow (juce::DocumentWindow& win)
    {
        win.setUsingNativeTitleBar (false);
        win.setTitleBarHeight (kTitleH);
    }

    void ownToMainWindow (juce::Component& win, const juce::Component& insideMain)
    {
        auto* mainTop = insideMain.getTopLevelComponent();
        if (mainTop == nullptr || mainTop == &win) return;

        auto* mainPeer = mainTop->getPeer();
        auto* ownPeer  = win.getPeer();
        if (mainPeer == nullptr || ownPeer == nullptr) return;

       #if JUCE_WINDOWS
        // NOT addToDesktop (flags, nativeParent).  That was the first attempt and
        // it is wrong: JUCE passes the handle to CreateWindowEx as the PARENT and
        // ORs in WS_CHILD (juce_Windowing_windows.cpp, "if (parentToAddTo !=
        // nullptr) result |= WS_CHILD"), which makes a CHILD window -- clipped to
        // the app frame, undraggable outside it, and with getBounds() silently
        // switching to parent-client space so any save/restore of bounds around
        // the call re-plants the window somewhere else.
        //
        // OWNED is a different relationship and Win32 has no JUCE-level API for
        // it: setting GWLP_HWNDPARENT on an ALREADY-CREATED top-level window sets
        // the owner WITHOUT WS_CHILD.  That gives exactly what §9.4 asks for --
        // the OS keeps it above its owner, above nothing else, and minimises it
        // with the owner -- while it stays a real top-level window the user can
        // drag anywhere on screen.
        auto* hwnd  = static_cast<HWND> (ownPeer ->getNativeHandle());
        auto* owner = static_cast<HWND> (mainPeer->getNativeHandle());
        if (hwnd != nullptr && owner != nullptr)
            SetWindowLongPtr (hwnd, GWLP_HWNDPARENT, (LONG_PTR) owner);
       #else
        juce::ignoreUnused (mainPeer, ownPeer);
       #endif
    }
}
