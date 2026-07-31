#pragma once
#include <JuceHeader.h>
#include "../DSP/DSPBase.h"
#include "HostedPlugin.h"

// ── Hosting::HostedPluginEffect ──────────────────────────────────────────────
// QA-ModelShell TS6 (BLU-300): the FX rack's adapter onto a hosted plugin.
//
// The rack stores unique_ptr<DSPBase>, not juce::AudioProcessor, so the proxy
// cannot BE the slot -- it is wrapped by one.  Everything real lives in
// HostedPluginInstance; this class is the DSPBase shape around it.
//
// EMPTY UNTIL TOLD WHICH PLUGIN, on purpose.  EffectRack::createEffect is a
// plain EffectType switch and an EffectType carries no plugin identity, so the
// factory builds this with no plugin and one of two things fills it in:
//   * the picker calls setPlugin() with the chosen description, or
//   * setStateInformation() rebuilds it from the saved blob, which carries the
//     FULL PluginDescription -- so a project keeps loading its plugins even if
//     the user has since removed them from the added list.
// ─────────────────────────────────────────────────────────────────────────────

namespace Hosting
{

class HostedPluginEffect final : public DSPBase
{
public:
    HostedPluginEffect() = default;

    // Fresh add from the picker.  Replaces any existing instance.
    void setPlugin (const juce::PluginDescription&);

    HostedPluginInstance* getHosted() const noexcept { return mHosted.get(); }

    // Empty when no plugin has been assigned yet.
    juce::String getPluginName() const;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>&) override;
    void reset() override;

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    int getLatencySamples() const override;

    // TS7 (2026-07-31): a hosted VST3 in a rack slot had NO playhead, so its
    // tempo-synced delays, LFOs and arpeggiators had nothing to follow -- JUCE
    // builds the entire VST3 ProcessContext from one AudioPlayHead::getPosition()
    // and skips every validity flag when there is none.  This is the one effect
    // type that overrides it; the other twelve take DSPBase's no-op.
    void setHostTransport (const DSPBase::HostTransport&) override;

    // ── Automation ──────────────────────────────────────────────────────────
    // Lanes are keyed on the plugin's OWN stable parameter id
    // (HostedParameter::getParameterID -- for VST3 the id the plugin declares),
    // never on the parameter's index.  Index-keyed automation is the
    // anti-pattern the binding research singled out (REAPER's positional
    // addressing): a plugin update that inserts or reorders a parameter would
    // silently repoint every lane onto the wrong control.
    //
    // The live applicator and the offline replay both go through these two, so
    // a lane cannot behave differently in an export than it does in playback --
    // the failure mode fact 5 of the batch plan exists to prevent.
    struct AutomatableParam
    {
        juce::String id;
        juce::String name;
    };

    juce::Array<AutomatableParam> getAutomatableParams() const;

    bool  applyParamNorm (const juce::String& paramId, float v01);
    float readParamNorm  (const juce::String& paramId, float fallback) const;

    // Static forms for callers that hold a DSPBase* rather than this type --
    // the rack applicators resolve to a DSPBase at apply time.
    static bool  applyParamNorm (DSPBase*, const juce::String& paramId, float v01);
    static float readParamNorm  (DSPBase*, const juce::String& paramId, float fallback);

private:
    juce::AudioProcessorParameter* findParam (const juce::String& paramId) const;

    std::unique_ptr<HostedPluginInstance> mHosted;

    // The playhead handed to the hosted instance.  A MEMBER, not a local:
    // setPlayHead stores the POINTER, so anything on the stack would dangle the
    // instant the call returned.
    struct RackPlayHead : juce::AudioPlayHead
    {
        juce::AudioPlayHead::PositionInfo mPos;
        juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override
        { return mPos; }
    };
    RackPlayHead mPlayHead;
    bool         mPlayHeadAttached { false };

    // The rack's calling convention has no MIDI.  Cleared every block because a
    // plugin is free to write into the buffer it is handed.
    juce::MidiBuffer mMidiScratch;

    double mPreparedRate  { 0.0 };
    int    mPreparedBlock { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostedPluginEffect)
};

} // namespace Hosting
