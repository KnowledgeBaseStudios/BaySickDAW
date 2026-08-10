#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "BaySickConstants.h"

// ── VST Editor stub ───────────────────────────────────────────────────────────
// BaySickDAW is standalone-only. The VST editor is a minimal placeholder that
// informs the host user to run the standalone app.
// All engine UI lives in StandaloneEditor (Source/Standalone/).
class BaySickDAWPluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit BaySickDAWPluginEditor(BaySickDAWProcessor&);
    ~BaySickDAWPluginEditor() override = default;

    void paint  (juce::Graphics&) override;
    void resized() override {}

private:
    BaySickDAWProcessor& mProcessor;
    static constexpr int kW = 520;
    static constexpr int kH = 160;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BaySickDAWPluginEditor)
};
