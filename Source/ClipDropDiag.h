#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// QA-ClipDrop diagnostic trap (2026-06-02).
//
// The audio-clip drop path intermittently fails (a drop / "+ Add New Clip"
// produces no page, no strip, no browser entry) and the failure CLEARS on a
// fresh app restart -- it is session-state-dependent and will not reproduce on
// demand, so it cannot be fixed-and-verified blind.  This trap captures the
// failure live: every step of the drop cascade logs to a file, and a popup
// fires only on a bail / "produced nothing" anomaly.
//
// Debug build only (Jeff, 2026-08-07).  The trace is append-only with no cap
// or rotation, so a shipping build would grow a file the user never asked for
// and cannot clear from inside the app.  BOTH halves are gated together: the
// popup body names the log file, so keeping alert() in Release would point the
// user at something that never gets written.  Release keeps no-op stubs so the
// cascade's call sites compile unchanged.
// Rule-4 catalogued in Plans & Specs/Running Notes/fancy-kindling-dongarra.md.
// ─────────────────────────────────────────────────────────────────────────────

#include <JuceHeader.h>
#include "AppPaths.h"

namespace ClipDropDiag
{
#if JUCE_DEBUG
    inline juce::File logFile()
    {
        return AppPaths::appRoot()
                   .getChildFile ("clipdrop_diag_log.txt");
    }

    inline void log (const juce::String& stage, const juce::String& detail)
    {
        auto f = logFile();
        f.getParentDirectory().createDirectory();
        f.appendText (juce::Time::getCurrentTime().toString (true, true, true, true)
                       + "  [QA-ClipDrop DIAG] " + stage + " | " + detail + juce::newLine);
    }

    // Record + raise a popup.  Called only at a bail / "produced nothing"
    // anomaly, never on a clean drop, so it can never interrupt working audio.
    inline void alert (const juce::String& stage, const juce::String& detail)
    {
        log (stage, detail);
        const juce::String body = stage + "\n\n" + detail
                                + "\n\n(Full trace in Documents/BaySickDAW/clipdrop_diag_log.txt)";
        juce::MessageManager::callAsync ([body]
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                    "Clip-Drop Diagnostic", body);
        });
    }
#else
    inline void log   (const juce::String&, const juce::String&) {}
    inline void alert (const juce::String&, const juce::String&) {}
#endif
}
