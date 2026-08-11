#pragma once
#include <JuceHeader.h>

// ── ManualsWindow ────────────────────────────────────────────────────────────
// The in-app manual reader, opened from Help > Help Index (F1).  This is the
// window the HOLD-FOR-MANUALS-WINDOW marker was held for.
//
// Renders the SAME HTML that the shipped PDF is generated from, so the in-app
// copy and the offline copy cannot drift.  WebView2 backend (SC-14): the
// alternative was reimplementing text layout, reflow, search and hyperlinks in
// JUCE to arrive somewhere worse, and only WebView2 supports
// Options::withNativeFunction, which is what lets a Manual 1 callout jump to
// the page it documents.
//
// CONTENT CONTRACT.  The manuals are a static site staged next to the
// executable at <exe dir>/Manuals/, entry point index.html.  Nothing here
// authors or validates that content; the window reports honestly when it is
// absent rather than opening a blank frame, because a clean install that has
// not yet had the manuals installed is a real state.
//
// RUNTIME DEPENDENCY.  WebView2Loader.dll is staged beside the exe by a
// CMake POST_BUILD step, and the WebView2 Runtime itself is preinstalled on
// Windows 11 and installer-provisioned elsewhere.  When the runtime is MISSING
// there is no fallback of ours: JUCE silently substitutes the legacy IE control
// (juce_WebBrowserComponent_windows.cpp, createAndInitPlatformDependentPart)
// and the window renders through that, with nothing surfaced to the user.  An
// earlier version of this comment claimed the window offers the file on disk
// instead; it does not, and the only placeholder branch in the .cpp is the one
// for a missing index.html.  CMakeLists says the same thing correctly.
// ─────────────────────────────────────────────────────────────────────────────
class ManualsWindow : public juce::DocumentWindow
{
public:
    ManualsWindow();

    void closeButtonPressed() override;

    // <exe dir>/Manuals/index.html.  Single resolver: every reader of this path
    // calls it rather than rebuilding the string, per the standing
    // single-source-of-truth rule for filesystem paths.
    static juce::File manualsIndexFile();

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ManualsWindow)
};
