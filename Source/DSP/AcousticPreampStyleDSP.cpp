#include "AcousticPreampStyleDSP.h"

namespace
{
    // Standard Schroeder reverberator delay lines (Manfred Schroeder, 1962
    // -- Freeverb constants).  Lengths in samples @ 44.1 kHz; scaled for
    // the actual sample rate at prepare() time.  Stereo offset added to the
    // right channel for width.
    constexpr int kCombLensL[4]    = { 1557, 1617, 1491, 1422 };
    constexpr int kCombLensR[4]    = { 1580, 1640, 1514, 1445 };
    constexpr int kAllpassLensL[2] = { 225,  556  };
    constexpr int kAllpassLensR[2] = { 248,  579  };

    // Body-resonance frequencies for the synthetic IRs.  Each body type
    // gets 3 damped sinusoids at the listed peaks summed into a 4096-sample
    // exponential-decay envelope (~93 ms @ 44.1k).  Real IRs from
    // Resources/IRs/Acoustic/{Dreadnought,Parlor,Jumbo}.wav override these.
    struct BodyResonance { float air, top, body; };
    constexpr BodyResonance kRes[3] = {
        {  100.0f, 220.0f, 460.0f }, // Dreadnought
        {  180.0f, 320.0f, 580.0f }, // Parlor
        {   80.0f, 180.0f, 410.0f }, // Jumbo
    };

    constexpr int kSynthIRLen = 4096;

    juce::File getResourceIRFile (AcousticPreampStyleDSP::Body body)
    {
        const auto* name = (body == AcousticPreampStyleDSP::Body::Dreadnought) ? "Dreadnought.wav"
                         : (body == AcousticPreampStyleDSP::Body::Parlor)      ? "Parlor.wav"
                         : (body == AcousticPreampStyleDSP::Body::Jumbo)       ? "Jumbo.wav"
                         : "";
        if (name == nullptr || *name == 0) return {};
        auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        return exe.getParentDirectory().getChildFile ("Resources").getChildFile ("IRs")
                  .getChildFile ("Acoustic").getChildFile (name);
    }
}

AcousticPreampStyleDSP::AcousticPreampStyleDSP() = default;

void AcousticPreampStyleDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;

    juce::dsp::ProcessSpec spec { sampleRate,
                                   (juce::uint32) juce::jmax (1, maxBlockSize),
                                   2 };

    mConv.reset();
    mConv.prepare (spec);

    mNotch.prepare (spec);
    mNotch.setType (juce::dsp::StateVariableTPTFilterType::bandpass);   // we'll subtract from input for notch
    mNotch.setCutoffFrequency (juce::jlimit (50.0f, 1000.0f, mNotchHz));
    mNotch.setResonance (10.0f);
    mNotch.reset();

    // Scale Schroeder delay lengths for current sample rate.
    const double scale = sampleRate / 44100.0;
    for (int i = 0; i < 4; ++i)
    {
        mCombs[i][0].prepare ((int) std::lround (kCombLensL[i] * scale));
        mCombs[i][1].prepare ((int) std::lround (kCombLensR[i] * scale));
        mCombs[i][0].feedback = 0.84f;
        mCombs[i][1].feedback = 0.84f;
        mCombs[i][0].damp2    = 0.5f;
        mCombs[i][1].damp2    = 0.5f;
    }
    for (int i = 0; i < 2; ++i)
    {
        mAllpasses[i][0].prepare ((int) std::lround (kAllpassLensL[i] * scale));
        mAllpasses[i][1].prepare ((int) std::lround (kAllpassLensR[i] * scale));
    }

    mDryScratch.setSize (2, juce::jmax (1, maxBlockSize), false, true, true);
    mAmbScratch.setSize (2, juce::jmax (1, maxBlockSize), false, true, true);

    mBodyIRDirty = true;
    rebuildBodyIR();
}

void AcousticPreampStyleDSP::reset()
{
    mConv.reset();
    mNotch.reset();
    for (auto& row : mCombs)    for (auto& c : row) c.reset();
    for (auto& row : mAllpasses) for (auto& a : row) a.reset();
}

void AcousticPreampStyleDSP::buildSyntheticIR (Body body, juce::AudioBuffer<float>& dest, double sr)
{
    const int idx = juce::jlimit (0, 2, (int) body);
    const auto& r = kRes[idx];

    dest.setSize (2, kSynthIRLen, false, true, true);
    dest.clear();

    const float dec = 4.0f / (float) kSynthIRLen;   // exp decay tau ~ N/4 samples
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float fs = (float) sr;

    for (int ch = 0; ch < 2; ++ch)
    {
        float* p = dest.getWritePointer (ch);
        const float airW = ch == 0 ? 1.0f : 0.92f;     // slight stereo decorrelation
        const float topW = ch == 0 ? 0.7f : 0.78f;
        const float bodW = ch == 0 ? 0.45f : 0.5f;
        for (int i = 0; i < kSynthIRLen; ++i)
        {
            const float t = (float) i / fs;
            const float env = std::exp (-(float) i * dec);
            float s = airW * std::sin (twoPi * r.air  * t) * env;
            s     += topW * std::sin (twoPi * r.top  * t) * env * std::exp (-(float) i * dec * 1.6f);
            s     += bodW * std::sin (twoPi * r.body * t) * env * std::exp (-(float) i * dec * 2.4f);
            p[i] = s;
        }

        // Add a tiny click at sample 0 so the IR has DC-balanced low-end response
        // (otherwise the all-sinusoid IR has no fundamental impulse).
        p[0] += 1.0f;
    }

    // Normalize peak to 0 dBFS so per-body gain stays comparable.
    float peak = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* p = dest.getReadPointer (ch);
        for (int i = 0; i < kSynthIRLen; ++i) peak = juce::jmax (peak, std::abs (p[i]));
    }
    if (peak > 1.0e-6f)
    {
        const float g = 0.95f / peak;
        for (int ch = 0; ch < 2; ++ch) dest.applyGain (ch, 0, kSynthIRLen, g);
    }
}

void AcousticPreampStyleDSP::rebuildBodyIR()
{
    mBodyIRDirty = false;

    if (mBody == Body::User)
    {
        if (mUserIRPath.isNotEmpty())
        {
            juce::File f (mUserIRPath);
            if (f.existsAsFile())
            {
                mConv.loadImpulseResponse (f, juce::dsp::Convolution::Stereo::yes,
                                              juce::dsp::Convolution::Trim::yes,
                                              0,
                                              juce::dsp::Convolution::Normalise::yes);
                return;
            }
        }
        // Fall through: User mode with no path -> single-sample identity IR
        // (acts as bypass; user blends with Resonance knob).
        juce::AudioBuffer<float> empty (2, 1);
        empty.clear();
        empty.setSample (0, 0, 1.0f);
        empty.setSample (1, 0, 1.0f);
        mConv.loadImpulseResponse (std::move (empty), mSampleRate,
                                    juce::dsp::Convolution::Stereo::yes,
                                    juce::dsp::Convolution::Trim::yes,
                                    juce::dsp::Convolution::Normalise::yes);
        return;
    }

    // Try to load a real IR file for this body type if Jeff drops one in
    // Resources/IRs/Acoustic/.  Falls back to synthetic if not found.
    auto resourceFile = getResourceIRFile (mBody);
    if (resourceFile.existsAsFile())
    {
        mConv.loadImpulseResponse (resourceFile, juce::dsp::Convolution::Stereo::yes,
                                                  juce::dsp::Convolution::Trim::yes,
                                                  0,
                                                  juce::dsp::Convolution::Normalise::yes);
        return;
    }

    juce::AudioBuffer<float> ir;
    buildSyntheticIR (mBody, ir, mSampleRate);
    mConv.loadImpulseResponse (std::move (ir), mSampleRate,
                                juce::dsp::Convolution::Stereo::yes,
                                juce::dsp::Convolution::Trim::yes,
                                juce::dsp::Convolution::Normalise::yes);
}

void AcousticPreampStyleDSP::setBody (int body)
{
    const Body b = static_cast<Body> (juce::jlimit (0, 3, body));
    if (mBody != b) { mBody = b; mBodyIRDirty = true; }
}

void AcousticPreampStyleDSP::setResonance (float v01)
{
    mResonance01 = juce::jlimit (0.0f, 1.0f, v01);
}

void AcousticPreampStyleDSP::setAmbience (float v01)
{
    mAmbience01 = juce::jlimit (0.0f, 1.0f, v01);
}

void AcousticPreampStyleDSP::setNotchHz (float hz)
{
    hz = juce::jlimit (50.0f, 1000.0f, hz);
    if (! juce::approximatelyEqual (mNotchHz, hz))
    {
        mNotchHz = hz;
        mNotch.setCutoffFrequency (hz);
    }
}

void AcousticPreampStyleDSP::setLevelDb (float db)
{
    mLevelDb = juce::jlimit (-24.0f, 12.0f, db);
}

void AcousticPreampStyleDSP::loadUserIR (const juce::File& file)
{
    mUserIRPath = file.existsAsFile() ? file.getFullPathName() : juce::String();
    if (mBody == Body::User) mBodyIRDirty = true;
}

void AcousticPreampStyleDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    if (mBodyIRDirty) rebuildBodyIR();

    // Signal flow: input -> split (dry/wet=Convolution) -> sum -> Schroeder
    // ambience -> Notch (last) -> Level -> output.

    // ── Body convolution wet/dry ─────────────────────────────────────────────
    {
        juce::AudioBuffer<float> wet (numCh, n);
        for (int ch = 0; ch < numCh; ++ch)
            wet.copyFrom (ch, 0, buffer, ch, 0, n);
        juce::dsp::AudioBlock<float> wb (wet);
        auto wsub = wb.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> wctx (wsub);
        mConv.process (wctx);

        const float wetGain = mResonance01;
        const float dryGain = 1.0f - mResonance01 * 0.5f;   // gentle dry duck so wet has room
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* dst = buffer.getWritePointer (ch);
            const float* w = wet.getReadPointer (ch);
            for (int i = 0; i < n; ++i) dst[i] = dst[i] * dryGain + w[i] * wetGain;
        }
    }

    // ── Schroeder ambience wet/dry ──────────────────────────────────────────
    if (mAmbience01 > 0.001f)
    {
        if (mAmbScratch.getNumChannels() < numCh || mAmbScratch.getNumSamples() < n)
            mAmbScratch.setSize (numCh, n, false, false, true);

        for (int ch = 0; ch < numCh; ++ch)
        {
            const int chIdx = juce::jmin (ch, 1);
            float* amb = mAmbScratch.getWritePointer (ch);
            const float* in = buffer.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
            {
                const float x = in[i];
                // 4 parallel combs summed
                float c = 0.0f;
                for (int k = 0; k < 4; ++k) c += mCombs[k][chIdx].process (x);
                c *= 0.25f;
                // 2 series allpass
                float y = mAllpasses[0][chIdx].process (c);
                y       = mAllpasses[1][chIdx].process (y);
                amb[i] = y;
            }
        }

        const float wetGain = mAmbience01;
        const float dryGain = 1.0f - mAmbience01 * 0.5f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* dst = buffer.getWritePointer (ch);
            const float* amb = mAmbScratch.getReadPointer (ch);
            for (int i = 0; i < n; ++i) dst[i] = dst[i] * dryGain + amb[i] * wetGain;
        }
    }

    // ── Notch (band-stop, last in chain — surgical feedback rejection) ──────
    // Process bandpass tap and subtract from main buffer to get band-stop.
    {
        juce::AudioBuffer<float> bpScratch (numCh, n);
        for (int ch = 0; ch < numCh; ++ch)
            bpScratch.copyFrom (ch, 0, buffer, ch, 0, n);
        juce::dsp::AudioBlock<float> bpBlk (bpScratch);
        auto bpSub = bpBlk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> bpCtx (bpSub);
        mNotch.process (bpCtx);
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* dst = buffer.getWritePointer (ch);
            const float* bp = bpScratch.getReadPointer (ch);
            for (int i = 0; i < n; ++i) dst[i] -= bp[i];
        }
    }

    // ── Level (final output gain) ───────────────────────────────────────────
    if (! juce::approximatelyEqual (mLevelDb, 0.0f))
    {
        const float g = juce::Decibels::decibelsToGain (mLevelDb, -60.0f);
        buffer.applyGain (g);
    }
}

void AcousticPreampStyleDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("AcousticPreampStyleDSP");
    state.setProperty ("body",      (int) mBody,          nullptr);
    state.setProperty ("resonance", mResonance01,         nullptr);
    state.setProperty ("ambience",  mAmbience01,          nullptr);
    state.setProperty ("notchHz",   mNotchHz,             nullptr);
    state.setProperty ("levelDb",   mLevelDb,             nullptr);
    state.setProperty ("userIR",    mUserIRPath,          nullptr);
    state.setProperty ("bypassed",  (int) bypassed,       nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void AcousticPreampStyleDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (! xml || ! xml->hasTagName ("AcousticPreampStyleDSP")) return;
    auto state = juce::ValueTree::fromXml (*xml);

    mUserIRPath  = state.getProperty ("userIR",    juce::String()).toString();
    setBody      ((int)            state.getProperty ("body",      (int) Body::Dreadnought));
    setResonance ((float)(double)  state.getProperty ("resonance", 0.5));
    setAmbience  ((float)(double)  state.getProperty ("ambience",  0.2));
    setNotchHz   ((float)(double)  state.getProperty ("notchHz",   250.0));
    setLevelDb   ((float)(double)  state.getProperty ("levelDb",   0.0));
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;

    mBodyIRDirty = true;
}
