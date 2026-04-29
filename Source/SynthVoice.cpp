#include "SynthVoice.h"

SynthVoice::SynthVoice()
{
    for (int i = 0; i < kMaxLayers; ++i)
    {
        mOscMode[i]          = OscMode::Wavetable;
        mClassicShape[i]     = ClassicShape::Saw;
        mNoiseAmount[i]      = 0.0f;
        mFilterBaseCutoff[i] = 4000.0f;
        mFilterEnvAmt[i]     = 24.0f;
    }
    mLayers[0].enabled = true;  // layer 0 on by default
}

bool SynthVoice::canPlaySound (juce::SynthesiserSound* s)
{
    return dynamic_cast<SynthSound*>(s) != nullptr;
}

void SynthVoice::startNote (int midiNoteNumber, float velocity,
                             juce::SynthesiserSound*, int)
{
    mCurrentNote     = midiNoteNumber;
    mCurrentVelocity = velocity;
    float hz = noteToHz (midiNoteNumber, mPitchWheelSemis);

    for (int i = 0; i < kMaxLayers; ++i)
    {
        auto& L = mLayers[i];
        L.reset();
        L.wavetable.setFrequency (hz);
        L.fmOsc.setCarrierFrequency (hz);
        L.classicDelta = static_cast<float> (hz / getSampleRate());
        if (L.enabled) L.noteOn();
    }
}

void SynthVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
        for (auto& L : mLayers) L.noteOff();
    else
    {
        for (auto& L : mLayers) L.reset();
        clearCurrentNote();
        mCurrentNote = -1;
    }
}

void SynthVoice::pitchWheelMoved (int newValue)
{
    mPitchWheelSemis = (static_cast<float>(newValue) / 8191.5f - 1.0f) * 2.0f;
    if (mCurrentNote >= 0)
    {
        float hz = noteToHz (mCurrentNote, mPitchWheelSemis);
        for (auto& L : mLayers)
        {
            L.wavetable.setFrequency (hz);
            L.fmOsc.setCarrierFrequency (hz);
            L.classicDelta = static_cast<float> (hz / getSampleRate());
        }
    }
}

void SynthVoice::controllerMoved (int, int) {}

void SynthVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                   int startSample, int numSamples)
{
    if (!anyLayerActive())
    {
        clearCurrentNote();
        mCurrentNote = -1;
        return;
    }

    for (int s = 0; s < numSamples; ++s)
    {
        float mixedOut = 0.0f;
        int   activeLayers = 0;

        for (int i = 0; i < kMaxLayers; ++i)
        {
            auto& L = mLayers[i];
            if (!L.enabled || !L.ampEnv.isActive()) continue;
            ++activeLayers;

            // LFO pitch mod
            float lfoVal   = L.lfo.getNextSample();
            float pitchMod = std::pow (2.0f, lfoVal / 12.0f);

            if (mCurrentNote >= 0)
            {
                float hz = noteToHz (mCurrentNote, mPitchWheelSemis) * pitchMod;
                L.wavetable.setFrequency (hz);
                L.fmOsc.setCarrierFrequency (hz);
                L.classicDelta = static_cast<float> (hz / getSampleRate());
            }

            // Oscillator
            float oscSample = 0.0f;
            switch (mOscMode[i])
            {
                case OscMode::Wavetable:
                    oscSample = L.wavetable.getNextSample();
                    break;
                case OscMode::Classic:
                    oscSample = classicSample (i, L.classicPhase);
                    L.classicPhase += L.classicDelta;
                    if (L.classicPhase >= 1.0f) L.classicPhase -= 1.0f;
                    break;
                case OscMode::FM:
                    oscSample = L.fmOsc.getNextSample();
                    break;
                case OscMode::Noise:
                    oscSample = mRandom.nextFloat() * 2.0f - 1.0f;
                    break;
            }

            // Noise blend
            if (mNoiseAmount[i] > 0.0f)
            {
                float noise = mRandom.nextFloat() * 2.0f - 1.0f;
                oscSample = oscSample * (1.0f - mNoiseAmount[i]) + noise * mNoiseAmount[i];
            }

            // Filter env
            float fEnv   = L.filtEnv.getNextSample();
            float cutoff = mFilterBaseCutoff[i]
                           * std::pow (2.0f, fEnv * mFilterEnvAmt[i] / 12.0f);
            L.filter.setCutoff (juce::jlimit (20.0f, 20000.0f, cutoff));
            float filtered = L.filter.processSample (oscSample);

            // Amp env
            float ampVal = L.ampEnv.getNextSample();
            mixedOut += filtered * ampVal * mCurrentVelocity * L.gain;
        }

        if (activeLayers > 1)
            mixedOut /= static_cast<float> (activeLayers);

        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
            outputBuffer.addSample (ch, startSample + s, mixedOut);
    }
}

// ── Per-layer setters ─────────────────────────────────────────────────────────
void SynthVoice::setLayerEnabled      (int i, bool on)           { if (i<kMaxLayers) mLayers[i].enabled = on; }
void SynthVoice::setLayerOscMode      (int i, OscMode m)         { if (i<kMaxLayers) mOscMode[i] = m; }
void SynthVoice::setLayerClassicShape (int i, ClassicShape s)    { if (i<kMaxLayers) mClassicShape[i] = s; }
void SynthVoice::setLayerWavetablePos (int i, float p)           { if (i<kMaxLayers) mLayers[i].wavetable.setWavetablePos(p); }
void SynthVoice::setLayerUnisonVoices (int i, int v, float sp)   { if (i<kMaxLayers) mLayers[i].wavetable.setUnisonVoices(v,sp); }
void SynthVoice::setLayerDetune       (int i, float s)           { if (i<kMaxLayers) mLayers[i].wavetable.setDetune(s); }
void SynthVoice::setLayerFMRatio      (int i, float r)           { if (i<kMaxLayers) mLayers[i].fmOsc.setRatio(r); }
void SynthVoice::setLayerFMIndex      (int i, float idx)         { if (i<kMaxLayers) mLayers[i].fmOsc.setIndex(idx); }
void SynthVoice::setLayerNoiseAmount  (int i, float a)           { if (i<kMaxLayers) mNoiseAmount[i] = a; }
void SynthVoice::setLayerGain         (int i, float g)           { if (i<kMaxLayers) mLayers[i].gain = g; }
void SynthVoice::setLayerFilterMode   (int i, FilterMode m)      { if (i<kMaxLayers) mLayers[i].filter.setMode(m); }
void SynthVoice::setLayerFilterCutoff (int i, float hz)          { if (i<kMaxLayers) mFilterBaseCutoff[i] = hz; }
void SynthVoice::setLayerFilterRes    (int i, float q)           { if (i<kMaxLayers) mLayers[i].filter.setResonance(q); }
void SynthVoice::setLayerFilterEnvAmt (int i, float s)           { if (i<kMaxLayers) mFilterEnvAmt[i] = s; }
void SynthVoice::setLayerAmpEnv       (int i, float a, float d, float s, float r) { if (i<kMaxLayers) mLayers[i].ampEnv.setParameters(a,d,s,r); }
void SynthVoice::setLayerFilterEnv    (int i, float a, float d, float s, float r) { if (i<kMaxLayers) mLayers[i].filtEnv.setParameters(a,d,s,r); }
void SynthVoice::setLayerLFORate      (int i, float hz)          { if (i<kMaxLayers) mLayers[i].lfo.setRate(hz); }
void SynthVoice::setLayerLFODepth     (int i, float d)           { if (i<kMaxLayers) mLayers[i].lfo.setDepth(d); }
void SynthVoice::setLayerLFOShape     (int i, LFOShape s)        { if (i<kMaxLayers) mLayers[i].lfo.setShape(s); }

float SynthVoice::noteToHz (int midiNote, float extraSemis) const
{
    return 440.0f * std::pow (2.0f, (midiNote - 69 + extraSemis) / 12.0f);
}

float SynthVoice::classicSample (int i, float phase) const
{
    switch (mClassicShape[i])
    {
        case ClassicShape::Sine:     return std::sin (juce::MathConstants<float>::twoPi * phase);
        case ClassicShape::Saw:      return 2.0f * phase - 1.0f;
        case ClassicShape::Square:   return (phase < 0.5f) ? 1.0f : -1.0f;
        case ClassicShape::Triangle: return (phase < 0.5f) ? (4.0f*phase-1.0f) : (3.0f-4.0f*phase);
        default:                     return 0.0f;
    }
}

bool SynthVoice::anyLayerActive() const
{
    for (auto& L : mLayers)
        if (L.isActive()) return true;
    return false;
}
