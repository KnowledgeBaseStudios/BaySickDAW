#include "NoiseGateStyleDSP.h"

namespace
{
    constexpr float kRmsWindowMs = 5.0f;
}

void NoiseGateStyleDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;
    recomputeCoefs();
    reset();
}

void NoiseGateStyleDSP::reset()
{
    mEnvL = mEnvR = 0.0f;
    mGainL = mGainR = 1.0f;
}

void NoiseGateStyleDSP::recomputeCoefs()
{
    if (mSampleRate <= 0.0) return;
    // RMS one-pole coefficient: y = coef * y + (1-coef) * x^2
    mRmsCoef   = (float) std::exp (-1.0 / (kRmsWindowMs * 0.001 * mSampleRate));
    // Decay coefficient -- gate close release time.
    const float decay = juce::jlimit (1.0f, 1000.0f, mDecayMs);
    mDecayCoef = (float) std::exp (-1.0 / (decay * 0.001 * mSampleRate));
}

void NoiseGateStyleDSP::setThresholdDb (float dB)
{
    dB = juce::jlimit (-60.0f, 0.0f, dB);
    if (mThresholdDb != dB) mThresholdDb = dB;
}

void NoiseGateStyleDSP::setDecayMs (float ms)
{
    ms = juce::jlimit (1.0f, 1000.0f, ms);
    if (mDecayMs != ms) { mDecayMs = ms; recomputeCoefs(); }
}

void NoiseGateStyleDSP::setMode (int m)
{
    const Mode newMode = static_cast<Mode> (juce::jlimit (0, 1, m));
    if (mMode != newMode) mMode = newMode;
}

void NoiseGateStyleDSP::setSource (int s)
{
    const Source newSrc = static_cast<Source> (juce::jlimit (0, 1, s));
    if (mSource != newSrc) mSource = newSrc;
}

void NoiseGateStyleDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    // Pick the detector source.  DI mode falls back to Self if no sidechain
    // is wired (so the gate still does something useful before the user
    // sets up a sidechain feed).
    const juce::AudioBuffer<float>* detSrc = &buffer;
    if (mSource == Source::DI)
    {
        if (auto* sc = getActiveSidechain())
            if (sc->getNumSamples() >= n && sc->getNumChannels() > 0)
                detSrc = sc;
    }

    const float threshLin = juce::Decibels::decibelsToGain (mThresholdDb, -120.0f);
    const float threshSq  = threshLin * threshLin;
    const float floorDb   = (mMode == Mode::Mute) ? kMuteFloorDb : kReductionFloorDb;
    const float floorLin  = juce::Decibels::decibelsToGain (floorDb, -120.0f);

    const int detCh = detSrc->getNumChannels();
    for (int ch = 0; ch < numCh; ++ch)
    {
        const int detChIdx = juce::jmin (ch, detCh - 1);
        const float* det = detSrc->getReadPointer (detChIdx);
        float* aud       = buffer .getWritePointer (ch);
        float& env       = (ch == 0) ? mEnvL  : mEnvR;
        float& gain      = (ch == 0) ? mGainL : mGainR;

        for (int i = 0; i < n; ++i)
        {
            // RMS one-pole envelope (squared domain to avoid sqrt per sample).
            const float x = det[i];
            env = mRmsCoef * env + (1.0f - mRmsCoef) * (x * x);

            // Gate decision: open when env >= threshold, otherwise ramp toward
            // floor at decay rate.
            const float targetGain = (env >= threshSq) ? 1.0f : floorLin;
            gain = mDecayCoef * gain + (1.0f - mDecayCoef) * targetGain;

            aud[i] *= gain;
        }
    }
}

void NoiseGateStyleDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("NoiseGateStyleDSP");
    state.setProperty ("threshold", mThresholdDb,   nullptr);
    state.setProperty ("decay",     mDecayMs,       nullptr);
    state.setProperty ("mode",      (int) mMode,    nullptr);
    state.setProperty ("source",    (int) mSource,  nullptr);
    state.setProperty ("bypassed",  (int) bypassed, nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void NoiseGateStyleDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (! xml || ! xml->hasTagName ("NoiseGateStyleDSP")) return;
    auto state = juce::ValueTree::fromXml (*xml);
    setThresholdDb ((float)(double) state.getProperty ("threshold", -40.0));
    setDecayMs     ((float)(double) state.getProperty ("decay",     100.0));
    setMode        ((int)            state.getProperty ("mode",     (int) Mode::Reduction));
    setSource      ((int)            state.getProperty ("source",   (int) Source::DI));
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;
}
