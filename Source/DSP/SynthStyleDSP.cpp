#include "SynthStyleDSP.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)

namespace
{
    constexpr float kToneMinHz = 200.0f;
    constexpr float kToneMaxHz = 8000.0f;
    constexpr float kRateMinHz = 0.1f;
    constexpr float kRateMaxHz = 10.0f;

    // Per-Type voice profile.  QA-EffectsReview Task 4: extended with `brightness`
    // (a tone-cutoff bias multiplier) so the 11 SY-1 types read as distinct.
    struct TypeProfile
    {
        float envAttackMs;
        float envReleaseMs;
        float octaveShift;     // semitones; -12 = down 1 oct
        bool  lfoModulatesAmp; // true => LFO multiplies synth amp (Organ rotary / Seq stutter)
        float brightness;      // tone-cutoff bias multiplier (lower = darker)
    };

    // Ordered by enum value: Lead1(0) Pad(1) Bass(2) Seq1(3) Lead2(4) Str(5)
    // Organ(6) Bell(7) Sfx1(8) Sfx2(9) Seq2(10).
    constexpr TypeProfile kProfiles[SynthStyleDSP::kNumTypes] =
    {
        /* Lead1 */ {   5.0f,  80.0f,   0.0f, false, 1.00f },
        /* Pad   */ {  80.0f, 300.0f,   0.0f, false, 0.60f },
        /* Bass  */ {   5.0f,  60.0f, -12.0f, false, 0.45f },
        /* Seq1  */ {   5.0f,  60.0f,   0.0f, false, 0.90f },
        /* Lead2 */ {   5.0f,  90.0f,   0.0f, false, 1.30f },
        /* Str   */ { 120.0f, 400.0f,   0.0f, false, 0.70f },
        /* Organ */ {   3.0f,  90.0f,   0.0f, true,  0.85f },   // amp LFO = rotary
        /* Bell  */ {   1.0f,  45.0f,  12.0f, false, 1.45f },   // bright, fast decay, +1 oct
        /* Sfx1  */ {   8.0f, 180.0f,   0.0f, false, 1.05f },
        /* Sfx2  */ {   5.0f, 140.0f,   0.0f, false, 1.15f },
        /* Seq2  */ {   5.0f,  60.0f,   0.0f, true,  0.70f },   // amp LFO = stutter
    };

    // Variation maps to a wave-mix within each Type.  Returns three
    // crossfading weights for {sine, saw, square}.
    struct WaveMix { float sine, saw, sqr; };

    WaveMix variationMix (SynthStyleDSP::Type t, int variation)
    {
        const float v = juce::jlimit (0.0f, 1.0f, (float) (variation - 1) / 10.0f);
        using T = SynthStyleDSP::Type;
        switch (t)
        {
            case T::Lead1: return { 0.0f, 1.0f - v, v };                        // saw -> square
            case T::Pad:   return { 1.0f - 0.5f * v, 0.5f * v, 0.0f };          // sine -> sine+saw
            case T::Bass:  return { 0.0f, v, 1.0f - v };                        // square -> saw
            case T::Seq1:  return { 0.0f, 1.0f - v, v };                        // rhythmic saw -> square
            case T::Lead2: return { 0.0f, 0.4f * (1.0f - v), 0.6f + 0.4f * v }; // edgier square lead
            case T::Str:   return { 0.15f, 0.85f, 0.0f };                       // saw ensemble
            case T::Organ: return { 1.0f, 0.0f, 0.0f };                         // sine (rotary via amp LFO)
            case T::Bell:  return { 0.7f, 0.0f, 0.3f };                         // sine + metallic edge
            case T::Sfx1:  return { 0.0f, 1.0f - v, v };                        // sweep
            case T::Sfx2:  return { 0.3f * v, 1.0f - v, 0.7f * v };             // animated
            case T::Seq2:  return { 0.3f * v, 1.0f - v, 0.7f * v };             // stutter
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
    mPoly.prepare (sampleRate);
    updateInstrumentRange();

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
    mPoly.reset();
    for (auto& v : mVoices) v = Voice{};
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
    const float baseHz = kToneMinHz * std::pow (kToneMaxHz / kToneMinHz, t)
                       * kProfiles[(int) mType].brightness;   // Task 4: per-type tone bias
    // LFO sweeps cutoff up to +/- 1 octave at full depth.
    const float octaves = juce::jlimit (-1.0f, 1.0f, lfoMod) * mDepth;
    const float modHz   = baseHz * std::pow (2.0f, octaves);
    mLpf.setCutoffFrequency (juce::jlimit (kToneMinHz * 0.25f,
                                              juce::jmin (kToneMaxHz, (float) mSampleRate * 0.45f),
                                              modHz));
}

void SynthStyleDSP::setType (int t)
{
    const Type newType = static_cast<Type> (juce::jlimit (0, kNumTypes - 1, t));
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

void SynthStyleDSP::setPolyMode (bool poly)
{
    if (mPolyMode == poly) return;
    mPolyMode = poly;
    for (auto& v : mVoices) v = Voice{};   // clear voices on mode change
}

void SynthStyleDSP::setInstrument (int inst)
{
    const Instrument n = (inst == 1) ? Instrument::Bass : Instrument::Guitar;
    if (mInstrument == n) return;
    mInstrument = n;
    updateInstrumentRange();
}

void SynthStyleDSP::updateInstrumentRange()
{
    if (mInstrument == Instrument::Bass)
    {
        mPoly.setFreqRange (35.0f, 400.0f);   // ~B0..G4
        mPoly.setMaxNotes  (2);               // bass: mono / low poly
    }
    else
    {
        mPoly.setFreqRange (70.0f, 1300.0f);  // ~D2..E6 guitar range
        mPoly.setMaxNotes  (PolyPitchTracker::kMaxNotes);
    }
}

// Block-rate voice allocation: keep ringing voices for held notes, assign free/
// stolen voices for new notes, release the rest.  Per-sample envelope ramping +
// synthesis happen in process().
void SynthStyleDSP::allocateVoices (const PolyPitchTracker::NoteSet& ns)
{
    for (auto& v : mVoices) v.gate = false;

    const float profOct = std::pow (2.0f, kProfiles[(int) mType].octaveShift / 12.0f);

    for (int k = 0; k < ns.count; ++k)
    {
        const float f = ns.freqs[k];
        if (f <= 0.0f) continue;

        int vi = -1;
        // a) a voice already on this note (within ~3%)?
        for (int j = 0; j < kMaxVoices; ++j)
            if (mVoices[j].freq > 0.0f && std::abs (mVoices[j].freq - f) < f * 0.03f) { vi = j; break; }
        // b) else a free (released) voice?
        if (vi < 0)
            for (int j = 0; j < kMaxVoices; ++j)
                if (mVoices[j].env < 1.0e-3f && ! mVoices[j].gate) { vi = j; break; }
        // c) else steal the quietest.
        if (vi < 0)
        {
            float lowest = 1.0e9f;
            for (int j = 0; j < kMaxVoices; ++j)
                if (mVoices[j].env < lowest) { lowest = mVoices[j].env; vi = j; }
        }

        if (vi >= 0)
        {
            Voice& v = mVoices[vi];
            if (v.env < 1.0e-3f) v.phase = 0.0;   // fresh voice: restart phase (no click on held)
            v.freq     = f;
            v.phaseInc = (double) (f * profOct) / mSampleRate;
            v.gate     = true;
        }
    }

    // Fully-decayed, un-gated voices free up.
    for (auto& v : mVoices)
        if (! v.gate && v.env < 1.0e-4f) v.freq = 0.0f;
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
    // 2-3. QA-EffectsReview Task 4: mono (YIN, 1 voice) vs poly (FFT tracker + voice pool).
    const auto&  profile = kProfiles[(int) mType];
    const WaveMix mix    = variationMix (mType, mVariation);
    float* synthL = mSynthBuf.getWritePointer (0);
    float* synthR = (numCh > 1) ? mSynthBuf.getWritePointer (1) : synthL;
    const float* monoIn = mMonoIn.getReadPointer (0);

    if (mPolyMode)
    {
        mPoly.pushAudio (monoIn, n);
        allocateVoices (mPoly.getNotes());

        for (int i = 0; i < n; ++i)
        {
            const float absX  = std::abs (monoIn[i]);
            const float ecoef = (absX > mEnvelope) ? mEnvCoefAtt : mEnvCoefRel;
            mEnvelope = ecoef * mEnvelope + (1.0f - ecoef) * absX;

            mLfoPhase += mLfoPhaseInc;
            if (mLfoPhase >= 1.0) mLfoPhase -= 1.0;

            float s = 0.0f;
            for (auto& v : mVoices)
            {
                if (v.env < 1.0e-4f && ! v.gate) continue;
                const float target = v.gate ? 1.0f : 0.0f;
                const float vcoef  = v.gate ? mEnvCoefAtt : mEnvCoefRel;
                v.env = vcoef * v.env + (1.0f - vcoef) * target;
                s += waveSample (v.phase, mix) * v.env;
                v.phase += v.phaseInc;
                if (v.phase >= 1.0) v.phase -= 1.0;
            }
            s *= 0.4f * mEnvelope;   // poly headroom + input-dynamics scaling

            if (profile.lfoModulatesAmp)
            {
                const float lfo01 = 0.5f * (1.0f + std::sin ((float) mLfoPhase
                                                              * juce::MathConstants<float>::twoPi));
                s *= (1.0f - mDepth + mDepth * lfo01);
            }
            synthL[i] = s;
            if (numCh > 1) synthR[i] = s;
        }
    }
    else
    {
        mYin.pushAudio (monoIn, n);
        const float trackedHz  = mYin.getFrequencyHz();
        const float confidence = mYin.getConfidence();
        if (trackedHz > 0.0f && confidence > 0.2f)
        {
            const float shifted = trackedHz * std::pow (2.0f, profile.octaveShift / 12.0f);
            mPhaseInc = (double) shifted / mSampleRate;
        }
        // Else: keep last mPhaseInc (note rings on, then the envelope decays).

        for (int i = 0; i < n; ++i)
        {
            const float absX = std::abs (monoIn[i]);
            const float coef = (absX > mEnvelope) ? mEnvCoefAtt : mEnvCoefRel;
            mEnvelope = coef * mEnvelope + (1.0f - coef) * absX;

            mLfoPhase += mLfoPhaseInc;
            if (mLfoPhase >= 1.0) mLfoPhase -= 1.0;

            float s = waveSample (mPhase, mix);
            mPhase += mPhaseInc;
            if (mPhase >= 1.0) mPhase -= 1.0;
            s *= mEnvelope;

            if (profile.lfoModulatesAmp)
            {
                const float lfo01 = 0.5f * (1.0f + std::sin ((float) mLfoPhase
                                                              * juce::MathConstants<float>::twoPi));
                s *= (1.0f - mDepth + mDepth * lfo01);
            }
            synthL[i] = s;
            if (numCh > 1) synthR[i] = s;
        }
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
    state.setProperty ("poly",       (int) mPolyMode,   nullptr);
    state.setProperty ("instrument", (int) mInstrument, nullptr);
    state.setProperty ("bypassed",  (int) bypassed, nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void SynthStyleDSP::setStateInformation (const void* data, int sz)
{
    auto xml = SafeXml::parseBinaryBlob (data, sz);
    if (! xml || ! xml->hasTagName ("SynthStyleDSP")) return;
    auto state = juce::ValueTree::fromXml (*xml);
    setType        ((int)            state.getProperty ("type",      (int) Type::Lead1));
    setVariation   ((int)            state.getProperty ("variation", 1));
    setTone        ((float)(double) state.getProperty ("tone",       0.6));
    setRate        ((float)(double) state.getProperty ("rate",       0.3));
    setDepth       ((float)(double) state.getProperty ("depth",      0.4));
    setEffectLevel ((float)(double) state.getProperty ("effect",     0.5));
    setDirectLevel ((float)(double) state.getProperty ("direct",     1.0));
    setPolyMode    (((int) state.getProperty ("poly", 0)) != 0);
    setInstrument  ( (int) state.getProperty ("instrument", 0));
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;
}
