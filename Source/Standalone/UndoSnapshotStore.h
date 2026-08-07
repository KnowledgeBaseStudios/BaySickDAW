#pragma once
#include <JuceHeader.h>
#include "../AppPaths.h"

// QA-UndoCoverage Task 7: session-scoped store for structural-undo snapshot
// temp files (Jeff's 2026-08-06 ruling: engine-state "screenshots" like the
// page presets, captured at the destructive edge).  Files live under
// Documents/BaySickDAW/UndoSnapshots/, are owned by the StructuralOpAction
// that references them (deleted when the action falls off the depth cap or
// the manager clears), and the whole folder is swept at startup, project
// load, and exit -- undo history never survives the session, so neither do
// its snapshots.
namespace UndoSnapshotStore
{
    inline juce::File dir()
    {
        return AppPaths::appRoot().getChildFile ("UndoSnapshots");
    }

    inline juce::File writeNew (const juce::String& xmlContent)
    {
        static std::atomic<int> sCounter { 0 };
        auto d = dir();
        d.createDirectory();
        const auto f = d.getChildFile ("snap_"
                          + juce::String (juce::Time::currentTimeMillis())
                          + "_" + juce::String (sCounter.fetch_add (1)) + ".xml");
        f.replaceWithText (xmlContent);
        return f;
    }

    inline void sweepAll()
    {
        auto d = dir();
        if (d.isDirectory())
            for (const auto& f : d.findChildFiles (juce::File::findFiles, false, "snap_*.xml"))
                f.deleteFile();
    }
}
