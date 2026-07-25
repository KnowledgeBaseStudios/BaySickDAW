#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// [G3 PLAYHEAD] diagnostic readout (QA-G3Smoke Task 1, spec call G-9).
//
// The roll-side playhead residual (G3 dossier #30) is unexplained: the roll
// already has the pixel-center click fix (LDT-394), yet clicks still land off
// the playhead line.  This is a READING to characterize the residual so the
// fix can be routed (Rule 3) — not a fix.  Logs every roll click (x → raw /
// snapped beat, snap div, playhead beat) and every playhead paint tick (beat,
// output latency samples, sample rate).
//
// Debug build only.  Mirrors the ClipDropDiag / namirLog / pedalsLog one-off
// file-logger convention (Documents/BaySickDAW/*.txt) so the lines are
// readable without a debugger.  Rule-4 catalogued in
// Plans & Specs/Running Notes/burly-restringing-bison.md; strip at batch close.
// ─────────────────────────────────────────────────────────────────────────────

#include <JuceHeader.h>

namespace G3PlayheadDiag
{
#if JUCE_DEBUG
    inline void log (const juce::String& line)
    {
        auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                     .getChildFile ("BaySickDAW")
                     .getChildFile ("g3_playhead_log.txt");
        f.getParentDirectory().createDirectory();
        f.appendText (juce::Time::getCurrentTime().toString (true, true, true, true)
                       + "  [G3 PLAYHEAD] " + line + juce::newLine);
    }

    // [G3 PAN] (smoke #19 sweep half): pan-ramp arm readout -- one line per
    // RP takeover arm + one per mid-ramp CC10 stomp.  The arm chain reads
    // correct in code, so this reading discriminates the two candidates:
    // short/fallback span vs a channel CC10 stomping the ramp.  Audio-thread
    // caller, Debug-only, fires once per gesture.  Rule-4 catalogued.
    inline void logPan (const juce::String& line)
    {
        auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                     .getChildFile ("BaySickDAW")
                     .getChildFile ("g3_playhead_log.txt");
        f.getParentDirectory().createDirectory();
        f.appendText (juce::Time::getCurrentTime().toString (true, true, true, true)
                       + "  [G3 PAN] " + line + juce::newLine);
    }

    // [G3 BAR1] (smoke General-1): bar-1 note dropout reading.  Logs the
    // scheduling windows on blocks that touch beat 0 + every noteOn emitted
    // with absStart < 0.05, so a dropped first note shows as either a window
    // that opened past 0 (scheduler side) or an emitted-but-silent note
    // (engine side).  Fires ~once per loop pass; Debug-only; Rule-4 catalogued.
    inline void logBar1 (const juce::String& line)
    {
        auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                     .getChildFile ("BaySickDAW")
                     .getChildFile ("g3_playhead_log.txt");
        f.getParentDirectory().createDirectory();
        f.appendText (juce::Time::getCurrentTime().toString (true, true, true, true)
                       + "  [G3 BAR1] " + line + juce::newLine);
    }
#else
    inline void log (const juce::String&) {}
    inline void logPan (const juce::String&) {}
    inline void logBar1 (const juce::String&) {}
#endif
}
