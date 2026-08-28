// BaySickDAW - shot-harness factories for dialog components that live as
// file-scope types inside StandaloneEditor.cpp (QA-ManualPress M-1).  The
// harness cannot name those types from its own TU; these free functions hand
// it the SAME components the app's dialogs host, so a figure can never drift
// from the shipped dialog.
#pragma once

#include <JuceHeader.h>

class BuilderPage;
class BaySickDAWProcessor;

namespace shots
{
    std::unique_ptr<juce::Component> makeFileSettingsComponent();
    std::unique_ptr<juce::Component> makeExportAudioDialog (BuilderPage& builder,
                                                            BaySickDAWProcessor& proc);
    // The dialog enumerates device types/names without opening anything; a
    // never-initialised manager is its designed input.
    std::unique_ptr<juce::Component> makeAudioSettingsComponent (juce::AudioDeviceManager& dm);
}
