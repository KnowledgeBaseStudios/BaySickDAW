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

    // CONTRACT: an empty juce::File means the snapshot is NOT on disk.  Every
    // caller has to test it, and a caller whose next step destroys what it just
    // snapshotted (deleteTabWithUndo) has to abort that step and tell the user
    // -- returning the File regardless is how a delete became unrecoverable
    // while the history still showed an undoable "Delete" entry.  The callers
    // that only swap a chain may keep bailing quietly; the destructive ones
    // may not.
    inline juce::File writeNew (const juce::String& xmlContent)
    {
        static std::atomic<int> sCounter { 0 };
        auto d = dir();
        d.createDirectory();
        const auto f = d.getChildFile ("snap_"
                          + juce::String (juce::Time::currentTimeMillis())
                          + "_" + juce::String (sCounter.fetch_add (1)) + ".xml");
        if (! f.replaceWithText (xmlContent))
            return {};

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
