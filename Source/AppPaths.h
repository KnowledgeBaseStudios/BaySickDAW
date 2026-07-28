#pragma once
#include <JuceHeader.h>

// QA-ProjectSave Task 8 (G4 sweep 1): the single authority for the user-data
// root.  Documents\BaySickDAW was hand-spelled at 40+ sites, which is how the
// resolver islands (EffectPresetIO / SampleLibrary / ProjectManager) diverged
// in the first place -- every reader and writer resolves through here.
namespace AppPaths
{
    inline juce::File appRoot()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("BaySickDAW");
    }
}
