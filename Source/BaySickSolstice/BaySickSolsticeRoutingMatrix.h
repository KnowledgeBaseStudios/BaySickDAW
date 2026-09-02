#pragma once
#include <JuceHeader.h>
#include "../Standalone/SharedUI.h"   // TaggedSliderAttachment

// ── BaySickSolsticeRoutingMatrix ─────────────────────────────────────────────────────
// 6 rotary knobs for the mixer/routing matrix: sub, prot, clip, fx, vol, env.
class BaySickSolsticeRoutingMatrix : public juce::Component
{
public:
    BaySickSolsticeRoutingMatrix();

    // Optional: attach sliders to APVTS (call after construction)
    void attachToApvts (juce::AudioProcessorValueTreeState& apvts,
                        const juce::String& subId, const juce::String& protId,
                        const juce::String& clipId, const juce::String& fxId,
                        const juce::String& volId, const juce::String& envId);

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    static constexpr int kNumSliders = 6;

    BaySickSlider       mSliders[kNumSliders];

    using SliderAtt = TaggedSliderAttachment;
    std::unique_ptr<SliderAtt> mSliderAtts[kNumSliders];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickSolsticeRoutingMatrix)
};
