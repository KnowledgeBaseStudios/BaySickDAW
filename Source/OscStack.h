#pragma once
#include <JuceHeader.h>
#include "VibesynthConstants.h"
#include "WavetableOscillator.h"
#include "FMOscillator.h"
#include "AdsrEnvelope.h"
#include "SynthFilter.h"
#include "LFO.h"

// One oscillator layer - holds all DSP for a single osc slot
struct OscLayer
{
    // DSP
    WavetableOscillator wavetable;
    FMOscillator        fmOsc;
    AdsrEnvelope        ampEnv;
    AdsrEnvelope        filtEnv;
    SynthFilter         filter;
    LFO                 lfo;

    // State
    bool   enabled      { false };  // layer 0 defaults to true in SynthVoice
    float  gain         { 0.7f };
    float  classicPhase { 0.0f };
    float  classicDelta { 0.0f };

    void prepare (double sampleRate)
    {
        wavetable.prepare (sampleRate);
        fmOsc.prepare     (sampleRate);
        ampEnv.prepare    (sampleRate);
        filtEnv.prepare   (sampleRate);
        filter.prepare    (sampleRate);
        lfo.prepare       (sampleRate);
    }

    void noteOn()  { ampEnv.noteOn();  filtEnv.noteOn();  }
    void noteOff() { ampEnv.noteOff(); filtEnv.noteOff(); }
    void reset()
    {
        wavetable.reset();
        fmOsc.reset();
        ampEnv.reset();
        filtEnv.reset();
        filter.reset();
        lfo.reset();
        classicPhase = 0.0f;
    }

    bool isActive() const { return enabled && ampEnv.isActive(); }
};


