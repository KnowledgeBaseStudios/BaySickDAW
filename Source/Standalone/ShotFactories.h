// BaySickDAW - shot-harness factories for dialog components that live as
// file-scope types inside StandaloneEditor.cpp (QA-ManualPress M-1).  The
// harness cannot name those types from its own TU; these free functions hand
// it the SAME components the app's dialogs host, so a figure can never drift
// from the shipped dialog.
#pragma once

#include <JuceHeader.h>

class BuilderPage;
class BaySickDAWProcessor;
class MasterAnalyzerView;
class VersionCapture;

namespace shots
{
    std::unique_ptr<juce::Component> makeFileSettingsComponent();
    std::unique_ptr<juce::Component> makeExportAudioDialog (BuilderPage& builder,
                                                            BaySickDAWProcessor& proc);
    // The dialog enumerates device types/names without opening anything; a
    // never-initialised manager is its designed input.
    std::unique_ptr<juce::Component> makeAudioSettingsComponent (juce::AudioDeviceManager& dm);

    // Build-only halves of two editor-resident menus (QA-ManualPress M-2):
    // the editor's builders call these then show; the harness calls them and
    // renders, so the imaged items can never drift from the app.
    juce::PopupMenu buildMixerTitleMenu (BaySickDAWProcessor& proc,
                                         juce::AudioDeviceManager& dm);
    juce::PopupMenu buildAnalyzerMenu (MasterAnalyzerView& view, VersionCapture* vc);
}
