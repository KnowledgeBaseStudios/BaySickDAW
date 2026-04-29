#pragma once
#include <JuceHeader.h>
#include "SynthSound.h"
#include "OscStack.h"

enum class OscMode    { Wavetable, Classic, FM, Noise };
enum class ClassicShape { Sine, Saw, Square, Triangle };

class SynthVoice : public juce::SynthesiserVoice
{
public:
    // Max oscillator layers this voice can hold.
    // Kept at 4 for BaySickSynth reuse (Phase 2); only layer 0 is active by default.
    static constexpr int kMaxLayers = 4;

    SynthVoice();

    bool canPlaySound (juce::SynthesiserSound* sound) override;
    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int pitchWheelPos) override;
    void stopNote  (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newValue) override;
    void controllerMoved (int controllerNumber, int newValue) override;
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample, int numSamples) override;

    // ── Per-layer setters ─────────────────────────────────────────────────
    void setLayerEnabled     (int layer, bool on);
    void setLayerOscMode     (int layer, OscMode mode);
    void setLayerClassicShape(int layer, ClassicShape shape);
    void setLayerWavetablePos(int layer, float pos);
    void setLayerUnisonVoices(int layer, int voices, float spread);
    void setLayerDetune      (int layer, float semitones);
    void setLayerFMRatio     (int layer, float ratio);
    void setLayerFMIndex     (int layer, float index);
    void setLayerNoiseAmount (int layer, float amount);
    void setLayerGain        (int layer, float gain);

    void setLayerFilterMode  (int layer, FilterMode mode);
    void setLayerFilterCutoff(int layer, float hz);
    void setLayerFilterRes   (int layer, float q);
    void setLayerFilterEnvAmt(int layer, float semitones);

    void setLayerAmpEnv      (int layer, float a, float d, float s, float r);
    void setLayerFilterEnv   (int layer, float a, float d, float s, float r);

    void setLayerLFORate     (int layer, float hz);
    void setLayerLFODepth    (int layer, float depth);
    void setLayerLFOShape    (int layer, LFOShape shape);

private:
    OscLayer    mLayers[kMaxLayers];
    OscMode     mOscMode[kMaxLayers];
    ClassicShape mClassicShape[kMaxLayers];
    float       mNoiseAmount[kMaxLayers];
    float       mFilterBaseCutoff[kMaxLayers];
    float       mFilterEnvAmt[kMaxLayers];
    float       mPitchWheelSemis { 0.0f };

    int   mCurrentNote     { -1 };
    float mCurrentVelocity { 1.0f };

    juce::Random mRandom;

    float noteToHz (int midiNote, float extraSemis = 0.0f) const;
    float classicSample (int layer, float phase) const;

    bool anyLayerActive() const;
};
