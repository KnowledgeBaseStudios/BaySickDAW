#pragma once
#include <JuceHeader.h>

// QA-UndoCoverage Task 6 (12-iv): open a named transaction at a DISCRETE user
// gesture (menu pick, button click, combo change, type-in commit) so the
// parameter write becomes its own labeled history row instead of appending
// into whatever transaction happens to be open.  "param:<id>" is the same
// marker the vendored ParameterAttachment gesture naming uses -- the editor's
// history rebuild resolves it to an owner key + a human-readable label.
// Multi-param gestures call this ONCE before the first write; the remaining
// writes coalesce into the same transaction (one Ctrl+Z per gesture).
//
// Gesture-merge fix (2026-08-06): file any PENDING flushes into their own
// transaction BEFORE moving the boundary -- the APVTS flush timer is slower
// than quick gestures, and a previous gesture's late flush otherwise lands
// inside the NEW transaction (one Ctrl+Z then reverts both gestures).
inline void beginParamUndoGesture (juce::AudioProcessorValueTreeState& apvts,
                                   const juce::String& paramId)
{
    if (apvts.undoManager != nullptr)
    {
        juce::AudioProcessorValueTreeState::flushAllLiveInstancesToValueTrees();
        apvts.undoManager->beginNewTransaction ("param:" + paramId);
    }
}

inline void beginParamUndoGesture (juce::UndoManager* um, const juce::String& paramId)
{
    if (um != nullptr)
    {
        juce::AudioProcessorValueTreeState::flushAllLiveInstancesToValueTrees();
        um->beginNewTransaction ("param:" + paramId);
    }
}
