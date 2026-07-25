#pragma once
#include <JuceHeader.h>
#include "../Standalone/SharedUI.h"   // TaggedSliderAttachment

// ── HarmlessFilterRow ─────────────────────────────────────────────────────────
// One dual-filter row (either Filter 1 or Filter 2) with:
//   type dropdown, ENV knob, FREQ+RES large knobs, kb_track knob,
//   plus APVTS attachments.
// The parent HarmlessEditor creates two of these (one per filter).
class HarmlessFilterRow : public juce::Component
{
public:
    HarmlessFilterRow();
    ~HarmlessFilterRow() override;

    // Must be called before adding to parent. prefix = e.g. "tk_0_harm_flt1_"
    // The apvts must have params: {prefix}type, {prefix}cutoff, {prefix}res,
    //   {prefix}env_amt, {prefix}kb_track
    // where prefix is the full param prefix including filter number.
    void attachToApvts (juce::AudioProcessorValueTreeState& apvts,
                        const juce::String& cutoffId,
                        const juce::String& resId,
                        const juce::String& envAmtId,
                        const juce::String& kbTrackId,
                        const juce::String& typeId);

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    // Controls
    juce::ComboBox  mTypeCombo;
    VibeSlider      mEnvKnob;
    VibeSlider      mFreqKnob;
    VibeSlider      mResKnob;
    VibeSlider      mKbTrack;

    // Attachments
    using SliderAtt = TaggedSliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<ComboAtt>  mTypeAtt;
    std::unique_ptr<SliderAtt> mEnvAtt;
    std::unique_ptr<SliderAtt> mFreqAtt;
    std::unique_ptr<SliderAtt> mResAtt;
    std::unique_ptr<SliderAtt> mKbTrackAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmlessFilterRow)
};
