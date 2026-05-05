#include "SynthStyleDSP.h"

namespace
{
    constexpr float kToneMinHz = 200.0f;
    constexpr float kToneMaxHz = 8000.0f;
    constexpr float kRateMinHz = 0.1f;
    constexpr float kRateMaxHz = 10.0f;

    // Per-Type envelope-follower attack/release (ms).
    struct TypeProfile
    {
        float envAttackMs;
        float envReleaseMs;
        float octaveShift;     // semitones; -12 = down 1 oct
        bool  lfoModulatesAmp; // true => LFO multiplies synth amp (Seq)
    };

    constexpr TypeProfile kProfiles[4] =
    {
        // Lead: fast env, LFO -> filter
        { 5.0f,   80.0f,   0.0f,    false },
        // Pad: slow env, LFO -> filter
        { 80.0f,  300.0f,  0.0f,    false },
        // Bass: fast env, -1 oct, no LFO
        { 5.0f,   60.0f,  -12.0f,   false },
        // Seq: fast env, LFO -> amp
        { 5.0f,   60.0f,   0.0f,    true  },
    };

    // Variation maps to a wave-mix within each Type.  Returns three
    // crossfading weights for {sine, saw, square} that sum to 1.
    struct WaveMix { float sine, saw, sqr; };

    WaveMix variationMix (SynthStyleDSP::Type t, int variation)
    {
        const float v01 = juce::jlimit (0.0f, 1.0f, (float) (variation - 1) / 10.0f);
        switch (t)
        {
            case SynthStyleDSP::Type::Lead:
                // Saw <-> Square (brighter/edgier as variation rises)
                return { 0.0f, 1.0f - v01, v01 };
            case SynthStyleDSP::Type::Pad:
                // Sine <-> Sine+Saw (more harmonic content as variation rises)
                return { 1.0f - 0.5f * v01, 0.5f * v01, 0.0f };
            case SynthStyleDSP::Type::Bass:
                // Square <-> Saw (smoother as variation rises)
                return { 0.0f, v01, 1.0f - v01 };
            case SynthStyleDSP::Type::Seq:
                // Saw <-> Square+Sine
                return { 0.3f * v01, 1.0f - v01, 0.7f * v01 };
        }
        return { 0.0f, 1.0f, 0.0f };
    }

    inline float waveSample (double phase01, const WaveMix& mix)
    {
        const float twoPi = juce::MathConstants<float>::twoPi;
        const float sineVal = std::sin ((float) phase01 * twoPi);
        // Polynomial saw: 2*x - 1 where x in [0,1)
        const float sawVal  = 2.0f * (float) phase01 - 1.0f;
        const float sqrVal  = (phase01 < 0.5) ? 1.0f : -1.0f;
        return mix.sine * sineVal + mix.saw * sawVal + mix.sqr * sqrVal;
    }
}

void SynthStyleDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;

    mYin.prepare (sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate,
                                   (juce::uint32) juce::jmax (1, maxBlockSize),
                                   2 };
    mLpf.prepare (spec);
    mLpf.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    mLpf.setResonance (juce::MathConstants<float>::sqrt2 * 0.5f);
    mLpf.reset();

    const int n = juce::jmax (1, maxBlockSize);
    mMonoIn  .setSize (1, n, false, true, true);
    mSynthBuf.setSize (2, n, false, true, true);

    // Default env coefs from Lead profile (real values reset via setType).
    mEnvCoefAtt = (float) std::exp (-1.0 / (kProfiles[0].envAttackMs  * 0.001 * sampleRate));
    mEnvCoefRel = (float) std::exp (-1.0 / (kProfiles[0].envReleaseMs * 0.001 * sampleRate));

    updateLfoCoef();
    updateToneCutoff (0.0f);

    mDcCoef = 1.0f - (float) (juce::MathConstants<double>::twoPi * 5.0 / sampleRate);
    mDcXL = mDcYL = mDcXR = mDcYR = 0.0f;

    reset();
}

void SynthStyleDSP::reset()
{
    mYin.reset();
    mLpf.reset();
    mPhase = mLfoPhase = 0.0;
    mEnvelope = 0.0f;
    mDcXL = mDcYL = mDcXR = mDcYR = 0.0f;
}

void SynthStyleDSP::updateLfoCoef()
{
    if (mSampleRate <= 0.0) return;
    const float r  = juce::jlimit (0.0f, 1.0f, mRate);
    const float hz = kRateMinHz * std::pow (kRateMaxHz / kRateMinHz, r);
    mLfoPhaseInc = (double) hz / mSampleRate;
}

void SynthStyleDSP::updateToneCutoff (float lfoMod)
{
    if (mSampleRate <= 0.0) return;
    // Tone knob log-mapped + LFO modulation around it (depth scales the swing).
    const float t   = juce::jlimit (0.0f, 1.0f, mTone);
    const float baseHz = kToneMinHz * std::pow (kToneMaxHz / kToneMinHz, t);
    // LFO sweeps cutoff up to +/- 1 octave at full depth.
    const float octaves = juce::jlimit (-1.0f, 1.0f, lfoMod) * mDepth;
    const float modHz   = baseHz * std::pow (2.0f, octaves);
    mLpf.setCutoffFrequency (juce::jlimit (kToneMinHz * 0.25f,
                                              juce::jmin (kToneMaxHz, (float) mSampleRate * 0.45f),
                                              modHz));
}

void SynthStyleDSP::setType (int t)
{
    const Type newType = static_cast<Type> (juce::jlimit (0, 3, t));
    if (mType == newType) return;
    mType = newType;
    if (mSampleRate > 0.0)
    {
        const auto& p = kProfiles[(int) mType];
        mEnvCoefAtt = (float) std::exp (-1.0 / (p.envAttackMs  * 0.001 * mSampleRate));
        mEnvCoefRel = (float) std::exp (-1.0 / (p.envReleaseMs * 0.001 * mSampleRate));
    }
}

void SynthStyleDSP::setVariation (int v)
{
    v = juce::jlimit (1, 11, v);
    if (mVariation != v) mVariation = v;
}

void SynthStyleDSP::setTone (float v01)
{
    v01 = juce::jlimit (0.0f, 1.0f, v01);
    if (mTone != v01) mTone = v01;
}

void SynthStyleDSP::setRate (float v01)
{
    v01 = juce::jlimit (0.0f, 1.0f, v01);
    if (mRate != v01) { mRate = v01; updateLfoCoef(); }
}

void SynthStyleDSP::setDepth (float v01)
{
    v01 = juce::jlimit (0.0f, 1.0f, v01);
    if (mDepth != v01) mDepth = v01;
}

void SynthStyleDSP::setEffectLevel (float v01)
{
    v01 = juce::jlimit (0.0f, 1.0f, v01);
    if (mEffectLevel != v01) mEffectLevel = v01;
}

void SynthStyleDSP::setDirectLevel (float v01)
{
    v01 = juce::jlimit (0.0f, 1.0f, v01);
    if (mDirectLevel != v01) mDirectLevel = v01;
}

void SynthStyleDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    if (mMonoIn  .getNumSamples() < n) mMonoIn  .setSize (1,    n, false, false, true);
    if (mSynthBuf.getNumSamples() < n
     || mSynthBuf.getNumChannels() < numCh) mSynthBuf.setSize (numCh, n, false, false, true);

    // 1. Build mono mix for YIN.
    {
        float* mono = mMonoIn.getWritePointer (0);
        const float* l = buffer.getReadPointer (0);
        const float* r = (numCh > 1) ? buffer.getReadPointer (1) : l;
        for (int i = 0; i < n; ++i)
            mono[i] = 0.5f * (l[i] + r[i]);
    }
    mYin.pushAudio (mMonoIn.getReadPointer (0), n);

    // 2. Compute pitch + phase increment for the synth voice.
    const float trackedHz = mYin.getFrequencyHz();
    const float confidence = mYin.getConfidence();
    const auto& profile = kProfiles[(int) mType];
    if (trackedHz > 0.0f && confidence > 0.2f)
    {
        const float shifted = trackedHz * std::pow (2.0f, profile.octaveShift / 12.0f);
        mPhaseInc = (double) shifted / mSampleRate;
    }
    // Else: keep last mPhaseInc (continues last note for a bit, then envelope decays).

    // 3. Synthesise voice + apply envelope follower.
    const WaveMix mix = variationMix (mType, mVariation);
    float* synthL = mSynthBuf.getWritePointer (0);
    float* synthR = (numCh > 1) ? mSynthBuf.getWritePointer (1) : synthL;
    const float* monoIn = mMonoIn.getReadPointer (0);

    for (int i = 0; i < n; ++i)
    {
        // Envelope follower (asymmetric attack/release).
        const float absX = std::abs (monoIn[i]);
        const float coef = (absX > mEnvelope) ? mEnvCoefAtt : mEnvCoefRel;
        mEnvelope = coef * mEnvelope + (1.0f - coef) * absX;

        // LFO update (always advance).
        mLfoPhase += mLfoPhaseInc;
        if (mLfoPhase >= 1.0) mLfoPhase -= 1.0;

        // Generate waveform sample.
        float s = waveSample (mPhase, mix);
        mPhase += mPhaseInc;
        if (mPhase >= 1.0) mPhase -= 1.0;

        // Apply envelope (synth amp tracks input dynamics).
        s *= mEnvelope;

        // Seq type: LFO modulates amplitude (tremolo/stutter character).
        if (profile.lfoModulatesAmp)
        {
            const float lfo01  = 0.5f * (1.0f + std::sin ((float) mLfoPhase
                                                            * juce::MathConstants<float>::twoPi));
            const float ampMod = 1.0f - mDepth + mDepth * lfo01;
            s *= ampMod;
        }

        synthL[i] = s;
        if (numCh > 1) synthR[i] = s;
    }

    // 4. Filter sweep.  LFO modulation is applied at block boundary (not
    // per-sample) for cost; sounds fine for the 0.1-10 Hz range.
    if (! profile.lfoModulatesAmp)
    {
        const float lfoMod = std::sin ((float) mLfoPhase
                                          * juce::MathConstants<float>::twoPi);
        updateToneCutoff (lfoMod);
    }
    else
    {
        updateToneCutoff (0.0f);
    }

    {
        juce::dsp::AudioBlock<float> blk (mSynthBuf);
        auto sub = blk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        mLpf.process (ctx);
    }

    // 5. Mix: out = direct * dry + effectLevel * synthVoice.
    for (int ch = 0; ch < numCh; ++ch)
    {
        float* out = buffer.getWritePointer (ch);
        const float* synth = mSynthBuf.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
            out[i] = out[i] * mDirectLevel + synth[i] * mEffectLevel;
    }

    // 6. 5 Hz DC blocker.
    {
        const float R = mDcCoef;
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            float& xPrev = (ch == 0) ? mDcXL : mDcXR;
            float& yPrev = (ch == 0) ? mDcYL : mDcYR;
            for (int i = 0; i < n; ++i)
            {
                const float in = d[i];
                const float y  = in - xPrev + R * yPrev;
                xPrev = in;
                yPrev = y;
                d[i]  = y;
            }
        }
    }
}

void SynthStyleDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("SynthStyleDSP");
    state.setProperty ("type",      (int) mType,    nullptr);
    state.setProperty ("variation", mVariation,     nullptr);
    state.setProperty ("tone",      mTone,          nullptr);
    state.setProperty ("rate",      mRate,          nullptr);
    state.setProperty ("depth",     mDepth,         nullptr);
    state.setProperty ("effect",    mEffectLevel,   nullptr);
    state.setProperty ("direct",    mDirectLevel,   nullptr);
    state.setProperty ("bypassed",  (int) bypassed, nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void SynthStyleDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (! xml || ! xml->hasTagName ("SynthStyleDSP")) return;
    auto state = juce::ValueTree::fromXml (*xml);
    setType        ((int)            state.getProperty ("type",      (int) Type::Lead));
    setVariation   ((int)            state.getProperty ("variation", 1));
    setTone        ((float)(double) state.getProperty ("tone",       0.6));
    setRate        ((float)(double) state.getProperty ("rate",       0.3));
    setDepth       ((float)(double) state.getProperty ("depth",      0.4));
    setEffectLevel ((float)(double) state.getProperty ("effect",     0.5));
    setDirectLevel ((float)(double) state.getProperty ("direct",     1.0));
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;
}
