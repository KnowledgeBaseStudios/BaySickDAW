#include "ChorusDSP.h"
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
ChorusDSP::ChorusDSP()
{
    // Spread default LFO freqs slightly so they don't all alias together
    lfoParams[0].freq = 0.5f;
    lfoParams[1].freq = 0.7f;
    lfoParams[2].freq = 0.9f;
    mLFOPhase.fill(0.0f);

    mXoLP.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    mXoHP.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
}

// ─────────────────────────────────────────────────────────────────────────────
void ChorusDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;

    mBufSize = static_cast<int>(kMaxDelayMs * 0.001 * sampleRate) + 4;
    mBufL.assign(mBufSize, 0.0f);
    mBufR.assign(mBufSize, 0.0f);
    mWritePos = 0;

    // Spread initial LFO phases evenly to avoid phase cancellations
    for (int k = 0; k < 3; ++k)
        mLFOPhase[k] = juce::MathConstants<float>::twoPi
                        * static_cast<float>(k) / 3.0f;

    // LR4 crossover — prepare both LP and HP with matching cutoff.
    juce::dsp::ProcessSpec spec { sampleRate,
                                  (juce::uint32) juce::jmax(1, maxBlockSize),
                                  2 };
    mXoLP.prepare(spec);
    mXoHP.prepare(spec);
    mXoLP.setCutoffFrequency(crossCutoff);
    mXoHP.setCutoffFrequency(crossCutoff);
    mXoLP.reset();
    mXoHP.reset();

    // Smoothed params — 20 ms ramp is fast enough to feel responsive but kills
    // zipper artifacts on fast knob turns.
    const double rampSecs = 0.02;
    mDelaySmoothed .reset(sampleRate, rampSecs);
    mDepthSmoothed .reset(sampleRate, rampSecs);
    mStereoSmoothed.reset(sampleRate, rampSecs);
    mWetSmoothed   .reset(sampleRate, rampSecs);
    mDelaySmoothed .setCurrentAndTargetValue(delayMs);
    mDepthSmoothed .setCurrentAndTargetValue(depth);
    mStereoSmoothed.setCurrentAndTargetValue(stereo);
    mWetSmoothed   .setCurrentAndTargetValue(wet);
}

// ─────────────────────────────────────────────────────────────────────────────
void ChorusDSP::reset()
{
    std::fill(mBufL.begin(), mBufL.end(), 0.0f);
    std::fill(mBufR.begin(), mBufR.end(), 0.0f);
    mWritePos = 0;
    mXoLP.reset();
    mXoHP.reset();
    mDelaySmoothed .setCurrentAndTargetValue(delayMs);
    mDepthSmoothed .setCurrentAndTargetValue(depth);
    mStereoSmoothed.setCurrentAndTargetValue(stereo);
    mWetSmoothed   .setCurrentAndTargetValue(wet);
}

// ─────────────────────────────────────────────────────────────────────────────
float ChorusDSP::evalLFO (int wave, float phase)
{
    switch (wave)
    {
        case 0:  // Sine
            return std::sin(phase);

        case 1:  // Triangle  (2/π) * asin(sin(phase))
            return (2.0f / juce::MathConstants<float>::pi) * std::asin(std::sin(phase));

        case 2:  // Multi-sine  0.7*sin(φ) + 0.3*sin(3φ) — harmonic blend
            return 0.7f * std::sin(phase) + 0.3f * std::sin(3.0f * phase);

        case 3:  // Organic  sin(φ) + sin(0.37φ) — non-harmonic, slower secondary wobble
        default:
            return 0.5f * (std::sin(phase) + std::sin(0.37f * phase));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Catmull-Rom cubic interpolation from a circular buffer.
float ChorusDSP::cubicInterp (const std::vector<float>& buf, float rpos, int size)
{
    if (size == 0) return 0.0f;

    const int i1 = static_cast<int>(rpos);
    const float fr = rpos - static_cast<float>(i1);

    auto wrap = [size](int idx) -> int {
        idx %= size;
        return idx < 0 ? idx + size : idx;
    };

    const float y0 = buf[wrap(i1 - 1)];
    const float y1 = buf[wrap(i1    )];
    const float y2 = buf[wrap(i1 + 1)];
    const float y3 = buf[wrap(i1 + 2)];

    const float a = -0.5f*y0 + 1.5f*y1 - 1.5f*y2 + 0.5f*y3;
    const float b =        y0 - 2.5f*y1 + 2.0f*y2 - 0.5f*y3;
    const float c = -0.5f*y0 + 0.5f*y2;

    return ((a*fr + b)*fr + c)*fr + y1;
}

// ─────────────────────────────────────────────────────────────────────────────
void ChorusDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;

    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels < 1 || mBufSize == 0) return;

    float* L = buffer.getWritePointer(0);
    float* R = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    const int   numVoices    = (voices >= 6) ? 6 : 3;

    // Pre-compute per-voice prime base-delay offsets in samples.
    std::array<float, 6> voicePrimeOffsetSamples;
    for (int v = 0; v < 6; ++v)
        voicePrimeOffsetSamples[v] = static_cast<float>(
            kVoicePrimeOffsetMs[v] * 0.001 * mSampleRate);

    // Pre-compute LFO increments per sample
    std::array<float, 3> lfoInc;
    for (int k = 0; k < 3; ++k)
        lfoInc[k] = static_cast<float>(
            juce::MathConstants<double>::twoPi * lfoParams[k].freq / mSampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        // Pull smoothed values once per sample
        const float curDelayMs  = mDelaySmoothed .getNextValue();
        const float curDepthMs  = mDepthSmoothed .getNextValue();
        const float curStereoDg = mStereoSmoothed.getNextValue();
        const float curWet      = mWetSmoothed   .getNextValue();

        const float depthSamples   = curDepthMs * 0.001f * (float) mSampleRate;
        const float baseSamplesRaw = std::max(depthSamples + 2.0f,
                                      curDelayMs * 0.001f * (float) mSampleRate);
        const float stereoRad      = curStereoDg * (juce::MathConstants<float>::pi / 180.0f);

        const float inL = L[i];
        const float inR = R ? R[i] : inL;

        // ── LR4 crossover ────────────────────────────────────────────────────
        const float lpL = mXoLP.processSample(0, inL);
        const float lpR = mXoLP.processSample(1, inR);
        const float hpL = mXoHP.processSample(0, inL);
        const float hpR = mXoHP.processSample(1, inR);

        // Select which band gets chorused
        float procL, procR, passL, passR;
        if (crossType == 0)   // HP: chorus the high band
        {
            procL = hpL;  procR = hpR;
            passL = lpL;  passR = lpR;
        }
        else                  // LP: chorus the low band
        {
            procL = lpL;  procR = lpR;
            passL = hpL;  passR = hpR;
        }

        // Write processed band into shared delay buffer
        mBufL[mWritePos] = procL;
        mBufR[mWritePos] = procR;

        // ── Sum chorused voices ──────────────────────────────────────────────
        float sumL = 0.0f, sumR = 0.0f;

        for (int v = 0; v < numVoices; ++v)
        {
            const int   lfoIdx  = v % 3;
            // Second batch of 3 voices (6-voice mode) gets 180° extra phase
            const float extraPh = (v >= 3) ? juce::MathConstants<float>::pi : 0.0f;

            const float phL = mLFOPhase[lfoIdx] + extraPh;
            const float phR = phL + stereoRad;

            const float lfoValL = evalLFO(lfoParams[lfoIdx].wave, phL);
            const float lfoValR = evalLFO(lfoParams[lfoIdx].wave, phR);

            // Per-voice base offset — each of the up-to-6 voices has a unique prime offset.
            const float voiceBase = baseSamplesRaw + voicePrimeOffsetSamples[v];

            // Read positions (write − delay), wrapped to [0, mBufSize)
            float rposL = static_cast<float>(mWritePos) - (voiceBase + depthSamples * lfoValL);
            float rposR = static_cast<float>(mWritePos) - (voiceBase + depthSamples * lfoValR);
            rposL -= static_cast<float>(mBufSize) * std::floor(rposL / static_cast<float>(mBufSize));
            rposR -= static_cast<float>(mBufSize) * std::floor(rposR / static_cast<float>(mBufSize));

            sumL += cubicInterp(mBufL, rposL, mBufSize);
            sumR += cubicInterp(mBufR, rposR, mBufSize);
        }

        const float scale = 1.0f / static_cast<float>(numVoices);
        sumL *= scale;
        sumR *= scale;

        // ── Mix ──────────────────────────────────────────────────────────────
        // Normal: out = passBand + (1-wet)*procBandDry + wet*chorusedProcBand
        // WetOnly: out = wet * chorusedProcBand (kills dry pass-band + dry proc-band)
        if (wetOnly)
        {
            L[i] = curWet * sumL;
            if (R) R[i] = curWet * sumR;
        }
        else
        {
            L[i] = passL + (1.0f - curWet) * procL + curWet * sumL;
            if (R) R[i] = passR + (1.0f - curWet) * procR + curWet * sumR;
        }

        // ── Advance LFO phases ───────────────────────────────────────────────
        for (int k = 0; k < 3; ++k)
        {
            mLFOPhase[k] += lfoInc[k];
            while (mLFOPhase[k] >= juce::MathConstants<float>::twoPi)
                mLFOPhase[k] -= juce::MathConstants<float>::twoPi;
        }

        mWritePos = (mWritePos + 1) % mBufSize;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Setters (CPU-guarded: only mutate state when the incoming value changes)
// ─────────────────────────────────────────────────────────────────────────────
void ChorusDSP::setLFOFreq (int lfo, float hz)
{
    if (lfo < 0 || lfo >= 3) return;
    hz = juce::jlimit(0.01f, 10.0f, hz);
    if (lfoParams[lfo].freq != hz)
        lfoParams[lfo].freq = hz;
}

void ChorusDSP::setLFOWave (int lfo, int wave)
{
    if (lfo < 0 || lfo >= 3) return;
    wave = juce::jlimit(0, 3, wave);
    if (lfoParams[lfo].wave != wave)
        lfoParams[lfo].wave = wave;
}

void ChorusDSP::setDelay (float ms)
{
    ms = juce::jlimit(0.5f, 30.0f, ms);
    if (delayMs != ms)
    {
        delayMs = ms;
        mDelaySmoothed.setTargetValue(ms);
    }
}

void ChorusDSP::setDepth (float ms)
{
    ms = juce::jlimit(0.0f, 20.0f, ms);
    if (depth != ms)
    {
        depth = ms;
        mDepthSmoothed.setTargetValue(ms);
    }
}

void ChorusDSP::setStereo (float degrees)
{
    degrees = juce::jlimit(0.0f, 360.0f, degrees);
    if (stereo != degrees)
    {
        stereo = degrees;
        mStereoSmoothed.setTargetValue(degrees);
    }
}

void ChorusDSP::setVoices (int n)
{
    const int clamped = (n >= 6) ? 6 : 3;
    if (voices != clamped) voices = clamped;
}

void ChorusDSP::setWet (float w)
{
    w = juce::jlimit(0.0f, 1.0f, w);
    if (wet != w)
    {
        wet = w;
        mWetSmoothed.setTargetValue(w);
    }
}

void ChorusDSP::setWetOnly (bool on)
{
    if (wetOnly != on) wetOnly = on;
}

void ChorusDSP::setCrossType (int t)
{
    const int clamped = (t == 1) ? 1 : 0;
    if (crossType != clamped) crossType = clamped;
}

void ChorusDSP::setCrossCutoff (float hz)
{
    // Upper clamp keeps cutoff safely below Nyquist so LR4 coefficients
    // stay well-conditioned across sample rates. Floor at 20 Hz.
    const float nyquistMargin = (mSampleRate > 0.0)
        ? 0.45f * (float) mSampleRate : 20000.0f;
    hz = juce::jlimit(20.0f, juce::jmin(20000.0f, nyquistMargin), hz);
    if (crossCutoff != hz)
    {
        crossCutoff = hz;
        mXoLP.setCutoffFrequency(hz);
        mXoHP.setCutoffFrequency(hz);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void ChorusDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state("ChorusDSP2");

    state.setProperty("delayMs",     delayMs,          nullptr);
    state.setProperty("depth",       depth,            nullptr);
    state.setProperty("wet",         wet,              nullptr);
    state.setProperty("voices",      voices,           nullptr);
    state.setProperty("stereo",      stereo,           nullptr);
    state.setProperty("crossType",   crossType,        nullptr);
    state.setProperty("crossCutoff", crossCutoff,      nullptr);
    state.setProperty("wetOnly",     (int) wetOnly,    nullptr);
    state.setProperty("bypassed",    (int)bypassed,    nullptr);

    for (int k = 0; k < 3; ++k)
    {
        state.setProperty("lfoFreq" + juce::String(k), lfoParams[k].freq, nullptr);
        state.setProperty("lfoWave" + juce::String(k), lfoParams[k].wave, nullptr);
    }

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml) juce::AudioProcessor::copyXmlToBinary(*xml, dest);
}

// ─────────────────────────────────────────────────────────────────────────────
void ChorusDSP::setStateInformation (const void* data, int sz)
{
    std::unique_ptr<juce::XmlElement> xml(juce::AudioProcessor::getXmlFromBinary(data, sz));
    if (!xml) return;

    // Accept both old tag (ChorusDSP) and new tag (ChorusDSP2)
    if (!xml->hasTagName("ChorusDSP") && !xml->hasTagName("ChorusDSP2")) return;

    juce::ValueTree state = juce::ValueTree::fromXml(*xml);

    delayMs     = state.getProperty("delayMs",     delayMs);
    depth       = state.getProperty("depth",       depth);
    wet         = state.getProperty("wet",         wet);
    voices      = state.getProperty("voices",      voices);
    stereo      = state.getProperty("stereo",      stereo);
    crossType   = state.getProperty("crossType",   crossType);
    crossCutoff = state.getProperty("crossCutoff", crossCutoff);
    wetOnly     = ((int) state.getProperty("wetOnly", 0)) != 0;
    bypassed    = ((int)state.getProperty("bypassed", 0)) != 0;

    for (int k = 0; k < 3; ++k)
    {
        lfoParams[k].freq = state.getProperty("lfoFreq" + juce::String(k), lfoParams[k].freq);
        lfoParams[k].wave = juce::jlimit(0, 3,
                             (int) state.getProperty("lfoWave" + juce::String(k), lfoParams[k].wave));
    }

    voices = (voices >= 6) ? 6 : 3;

    // Snap smoothers to loaded values so there's no post-load ramp
    mDelaySmoothed .setCurrentAndTargetValue(delayMs);
    mDepthSmoothed .setCurrentAndTargetValue(depth);
    mStereoSmoothed.setCurrentAndTargetValue(stereo);
    mWetSmoothed   .setCurrentAndTargetValue(wet);

    // Refresh crossover cutoff + clear filter state (prevents coefficient-swap clicks).
    if (mSampleRate > 0.0)
    {
        mXoLP.setCutoffFrequency(crossCutoff);
        mXoHP.setCutoffFrequency(crossCutoff);
        mXoLP.reset();
        mXoHP.reset();
    }
}
