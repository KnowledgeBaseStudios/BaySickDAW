// BaySickDAW - the --shot harness's menu capture hook (QA-ManualPress M-2).
// Headless there is no desktop for showMenuAsync, so a menu site hands its
// BUILT menu (and the Options it would have shown with) to the harness
// instead of showing it.  Armed only while the harness is shooting a menu
// figure; in a normal run maybeCapture is one null check.
//
// Site pattern, one line before the show:
//     if (shots::maybeCapture (m, opts)) return;
#pragma once

#include <JuceHeader.h>

namespace shots
{
    extern std::function<void (const juce::PopupMenu&,
                               const juce::PopupMenu::Options&)> gMenuCapture;

    bool maybeCapture (const juce::PopupMenu& m,
                       const juce::PopupMenu::Options& opts = {});
}
