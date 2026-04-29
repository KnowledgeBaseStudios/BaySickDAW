#pragma once
#include <JuceHeader.h>

// ── DSPBase ───────────────────────────────────────────────────────────────────
// Abstract base for all Phase 2+ standalone DSP effect modules.
// Matches the FXBase calling convention (process(AudioBuffer)) for easy
// integration with EffectRack.
// ─────────────────────────────────────────────────────────────────────────────
class DSPBase
{
public:
    virtual ~DSPBase() = default;

    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void process(juce::AudioBuffer<float>& buffer)   = 0;
    virtual void reset()                                      = 0;

    // Serialization — ValueTree XML stored as raw bytes
    virtual void getStateInformation(juce::MemoryBlock& dest) { (void)dest; }
    virtual void setStateInformation(const void* data, int sz) { (void)data; (void)sz; }

    // BPM sync — only meaningful for time-based effects; default is no-op
    virtual void setHostBPM(double /*bpm*/) {}

    // Gain-reduction meter (0 = no reduction, negative dB = compressed)
    virtual float getGainReductionDb() const { return 0.0f; }

    // Algorithmic latency introduced by this effect in samples (look-ahead, FFT, etc.).
    // Does NOT include user-chosen effect parameters (reverb pre-delay, delay time, etc.)
    // Return non-zero only for inherent processing latency the user didn't choose.
    // EffectRack accumulates these for PDC.
    virtual int getLatencySamples() const { return 0; }

    bool bypassed { false };

protected:
    double mSampleRate  { 44100.0 };
    int    mMaxBlock    { 512 };
};
