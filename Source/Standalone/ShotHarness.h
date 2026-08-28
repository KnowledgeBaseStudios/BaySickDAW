// BaySickDAW - the manual screenshot harness (QA-ManualPress M-1).
//
//     BaySickDAW.exe --shot [--out=DIR] [--scale=X] ["Figure Name" ...]
//
// Renders manual figures to PNG with no audio device, no window, and no
// user: the real processor and the real components, painted headless via
// createComponentSnapshot (no peer needed).  Every figure the manual ships
// was previously a hand capture that went stale on every UI change; this
// does the looking instead, on demand, in seconds.  Modeled on KBS Plugins'
// kbs_shot.  Output defaults to Manuals/shots-staging - the shipped
// Manuals/figures set is only replaced after Jeff approves a diff sheet.
#pragma once

#include <JuceHeader.h>

namespace shots
{
    // Runs the whole shot suite (or the named figures) and returns the
    // process exit code.  Owns its own processor world; message thread only.
    int run (const juce::String& commandLine);
}
