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
// Mirrors the namirLog() / pedalsLog() one-off file-logger convention
// (Documents/BaySickDAW/*.txt, works in both Debug and Release).  Rule-4
// catalogued in Plans & Specs/Running Notes/fancy-kindling-dongarra.md; stripped
// or kept at batch close per DS-2.
// ─────────────────────────────────────────────────────────────────────────────

#include <JuceHeader.h>

namespace ClipDropDiag
{
    inline juce::File logFile()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("BaySickDAW")
                   .getChildFile ("clipdrop_diag_log.txt");
    }

    // Append-only file record (Debug + Release).  Every cascade step calls this.
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
}
