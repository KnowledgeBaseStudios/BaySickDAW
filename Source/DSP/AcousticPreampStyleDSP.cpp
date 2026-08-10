#include "AcousticPreampStyleDSP.h"
#include "../MissingFileReport.h"
#include "../ProjectFileResolver.h"
#include "../SampleLibrary.h"

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

    // Per-body voicing for the adaptive modal bank.  Mode centers follow
    // published acoustic-guitar mode regions (Helmholtz air ~75-170 Hz by
    // body size, top plate, upper body modes); weights/Qs/tilt differentiate
    // the bodies for real (Task 9 re-voicing -- the old shared-gain seeds
    // made the bodies near-indistinguishable).  All values are ear-tunable
    // calibration starting points.
    struct BodyVoicing
    {
        float air,  top,  body;                // mode centers (Hz)
        float airW, topW, bodW;                // per-mode gain weights
        float airQ, topQ, bodQ;                // per-mode Q bases
        // Broadband per-body voicing (low shelf + mid/upper-mid peak) --
        // the audible body-SIZE difference; the narrow modes alone were
        // near-indistinguishable between bodies (Jeff, 2026-07-02).  Max dB
        // at full macro depth; scales with bodyDepth so it breathes too.
        float shelfHz, shelfDb;
        float peakHz,  peakDb, peakQ;
    };
    constexpr BodyVoicing kVoicing[3] = {
        // Dreadnought: balanced warm punch -- low lift + relaxed low-mids.
        { 100.0f, 210.0f, 440.0f,   1.00f, 0.80f, 0.55f,   2.2f, 2.8f, 3.2f,
          100.0f,  4.0f,   900.0f, -3.0f, 1.0f },
        // Parlor: small boxy -- lows CUT + boxy upper-mid bump, tight Qs.
        { 170.0f, 380.0f, 780.0f,   0.55f, 1.00f, 0.75f,   3.0f, 3.4f, 3.6f,
          180.0f, -6.0f,  1100.0f,  6.0f, 1.4f },
        // Jumbo: big wide bloom -- strong low shelf + scooped low-mids.
        {  78.0f, 160.0f, 360.0f,   1.30f, 0.70f, 0.50f,   1.8f, 2.6f, 3.0f,
          130.0f,  8.0f,   700.0f, -4.5f, 1.1f },
    };

    // Bundled base correction IR (piezo-to-real fingerprint), staged next to
    // the exe by the Resources copy step.  Exact on-disk name, spaces included.
    juce::File getBaseIRFile()
    {
        auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        return exe.getParentDirectory().getChildFile ("Resources")
                  .getChildFile ("Acoustic IRs").getChildFile ("Acoustic Preamp IR.wav");
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

    mAmbScratch.setSize   (2, juce::jmax (1, maxBlockSize), false, true, true);
    mConvScratch.setSize  (2, juce::jmax (1, maxBlockSize), false, true, true);
    mNotchScratch.setSize (2, juce::jmax (1, maxBlockSize), false, true, true);

    // Adaptive-resonance state: clear envelopes + filter rings; force a coef
    // refresh on the first block (sample rate may have changed).
    mEnvFast = mEnvSlow = 0.0f;
    for (auto& m : mModes) m.reset();
    mDeQuack.reset();
    mTilt.reset();
    mBodyPeak.reset();
    mCoefBody = -1;

    reloadConvIR();
}

void AcousticPreampStyleDSP::reset()
{
    mConv.reset();
    mNotch.reset();
    for (auto& row : mCombs)    for (auto& c : row) c.reset();
    for (auto& row : mAllpasses) for (auto& a : row) a.reset();
    mEnvFast = mEnvSlow = 0.0f;
    for (auto& m : mModes) m.reset();
    mDeQuack.reset();
    mTilt.reset();
    mBodyPeak.reset();
}

// RBJ cookbook peaking EQ (constant-Q), written straight into the biquad.
// No allocation -- safe at block rate on the audio thread (JUCE's
// coefficient makers return heap-allocated objects, so they aren't).
void AcousticPreampStyleDSP::makePeakCoefs (StereoBiquad& bq, double sr,
                                             float fc, float q, float gainDb)
{
    const float A     = std::pow (10.0f, gainDb * (1.0f / 40.0f));
    const float w0    = juce::MathConstants<float>::twoPi * fc / (float) sr;
    const float cw    = std::cos (w0);
    const float alpha = std::sin (w0) / (2.0f * juce::jmax (0.1f, q));
    const float a0inv = 1.0f / (1.0f + alpha / A);
    bq.b0 = (1.0f + alpha * A) * a0inv;
    bq.b1 = (-2.0f * cw)       * a0inv;
    bq.b2 = (1.0f - alpha * A) * a0inv;
    bq.a1 = (-2.0f * cw)       * a0inv;
    bq.a2 = (1.0f - alpha / A) * a0inv;
}

// RBJ cookbook low shelf, same no-allocation rationale.
void AcousticPreampStyleDSP::makeLowShelfCoefs (StereoBiquad& bq, double sr,
                                                 float fc, float q, float gainDb)
{
    const float A     = std::pow (10.0f, gainDb * (1.0f / 40.0f));
    const float w0    = juce::MathConstants<float>::twoPi * fc / (float) sr;
    const float cw    = std::cos (w0);
    const float alpha = std::sin (w0) / (2.0f * juce::jmax (0.1f, q));
    const float sqA   = std::sqrt (A);
    const float a0    = (A + 1.0f) + (A - 1.0f) * cw + 2.0f * sqA * alpha;
    const float a0inv = 1.0f / a0;
    bq.b0 = (        A * ((A + 1.0f) - (A - 1.0f) * cw + 2.0f * sqA * alpha)) * a0inv;
    bq.b1 = ( 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cw))                      * a0inv;
    bq.b2 = (        A * ((A + 1.0f) - (A - 1.0f) * cw - 2.0f * sqA * alpha)) * a0inv;
    bq.a1 = (-2.0f *     ((A - 1.0f) + (A + 1.0f) * cw))                      * a0inv;
    bq.a2 = (            ((A + 1.0f) + (A - 1.0f) * cw - 2.0f * sqA * alpha)) * a0inv;
}

void AcousticPreampStyleDSP::reloadConvIR()
{
    if (mBody != Body::User)
    {
        // Named bodies: the bundled base correction IR (piezo-to-real
        // fingerprint).  Missing file -> adaptive-only (mBaseIRLoaded gates
        // the conv stage in process()).
        auto f = getBaseIRFile();
        mBaseIRLoaded = f.existsAsFile();
        if (mBaseIRLoaded)
            mConv.loadImpulseResponse (f, juce::dsp::Convolution::Stereo::yes,
                                          juce::dsp::Convolution::Trim::yes,
                                          0,
                                          juce::dsp::Convolution::Normalise::yes);
        return;
    }

    if (mUserIRPath.isNotEmpty())
    {
        // mUserIRPath is whatever was persisted -- a bundled project stores
        // "Samples/<name>.wav", which a bare juce::File would resolve against
        // the process working directory and silently fall through to the
        // identity IR below.
        const juce::File f = ProjectFileResolver::resolve (mUserIRPath);
        if (f.existsAsFile())
        {
            mConv.loadImpulseResponse (f, juce::dsp::Convolution::Stereo::yes,
                                          juce::dsp::Convolution::Trim::yes,
                                          0,
                                          juce::dsp::Convolution::Normalise::yes);
            return;
        }
    }
    // User mode with no path -> single-sample identity IR (acts as bypass;
    // user blends with Resonance knob).
    juce::AudioBuffer<float> empty (2, 1);
    empty.clear();
    empty.setSample (0, 0, 1.0f);
    empty.setSample (1, 0, 1.0f);
    mConv.loadImpulseResponse (std::move (empty), mSampleRate,
                                juce::dsp::Convolution::Stereo::yes,
                                juce::dsp::Convolution::Trim::yes,
                                juce::dsp::Convolution::Normalise::yes);
}

void AcousticPreampStyleDSP::setBody (int body)
{
    const Body b = static_cast<Body> (juce::jlimit (0, 3, body));
    if (mBody != b)
    {
        // Reload only across the named<->User boundary (all named bodies
        // share the bundled base IR).  Message-thread callers only.
        const bool needReload = (b == Body::User) != (mBody == Body::User);
        mBody = b;
        if (needReload) reloadConvIR();
    }
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

void AcousticPreampStyleDSP::setNotchOn (bool on)
{
    if (on != mNotchOn) mNotchOn = on;
}

void AcousticPreampStyleDSP::setLevelDb (float db)
{
    mLevelDb = juce::jlimit (-24.0f, 12.0f, db);
}

bool AcousticPreampStyleDSP::loadUserIR (const juce::File& file, juce::String& outErr)
{
    if (file == juce::File())        // documented clear
    {
        mUserIRPath = {};
        if (mBody == Body::User) reloadConvIR();
        return true;
    }

    if (! file.existsAsFile())
    {
        outErr = "The file is missing:\n" + file.getFullPathName();
        return false;
    }

    // juce::dsp::Convolution::loadImpulseResponse reports nothing back, and
    // reloadConvIR falls back to a one-sample identity IR -- so an unreadable
    // or non-audio pick used to land as "the effect silently does nothing".
    // Probe with a reader before committing the path.
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
    {
        outErr = "This file could not be read as audio:\n" + file.getFullPathName();
        return false;
    }

    // Persisted form, not the absolute path: an IR under Core Library or My
    // Samples has to come back on another install or another Windows account,
    // and an absolute path embeds this machine's user name.  refForPersist
    // returns the absolute path for anything outside those roots, which is what
    // the bundler then rewrites when a project is made self-contained.
    mUserIRPath = SampleLibrary::refForPersist (file);
    if (mBody == Body::User) reloadConvIR();   // message-thread caller (panel)
    return true;
}

void AcousticPreampStyleDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    // Signal flow: input -> Acoustic Resonance (base-IR conv + adaptive
    // de-quack / modal bank / size tilt; User body = static conv wet/dry)
    // -> Schroeder ambience -> Notch (when on) -> Level -> output.
    // Conv IRs load on the message thread / prepare only -- never here.

    // ── Acoustic Resonance ───────────────────────────────────────────────────
    if (mBody == Body::User)
    {
        // Static convolution wet/dry -- a user IR is a fixed capture (locked
        // no-regression).  Scratch preallocated in prepare(); the size guard
        // only fires if the host exceeds the prepared block size.
        if (mConvScratch.getNumChannels() < numCh || mConvScratch.getNumSamples() < n)
            mConvScratch.setSize (juce::jmax (2, numCh), n, false, false, true);
        for (int ch = 0; ch < numCh; ++ch)
            mConvScratch.copyFrom (ch, 0, buffer, ch, 0, n);
        juce::dsp::AudioBlock<float> wb (mConvScratch);
        auto wsub = wb.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> wctx (wsub);
        mConv.process (wctx);

        const float wetGain = mResonance01;
        const float dryGain = 1.0f - mResonance01 * 0.5f;   // gentle dry duck so wet has room
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* dst = buffer.getWritePointer (ch);
            const float* w = mConvScratch.getReadPointer (ch);
            for (int i = 0; i < n; ++i) dst[i] = dst[i] * dryGain + w[i] * wetGain;
        }
    }
    else if (mResonance01 > 0.001f)
    {
        // 1. Base correction IR (the bundled piezo-to-real fingerprint),
        //    wet/dry under the same Resonance macro.  Missing file ->
        //    adaptive-only (no conv).
        if (mBaseIRLoaded)
        {
            if (mConvScratch.getNumChannels() < numCh || mConvScratch.getNumSamples() < n)
                mConvScratch.setSize (juce::jmax (2, numCh), n, false, false, true);
            for (int ch = 0; ch < numCh; ++ch)
                mConvScratch.copyFrom (ch, 0, buffer, ch, 0, n);
            juce::dsp::AudioBlock<float> wb (mConvScratch);
            auto wsub = wb.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
            juce::dsp::ProcessContextReplacing<float> wctx (wsub);
            mConv.process (wctx);

            const float wetGain = mResonance01;
            const float dryGain = 1.0f - mResonance01 * 0.5f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* dst = buffer.getWritePointer (ch);
                const float* w = mConvScratch.getReadPointer (ch);
                for (int i = 0; i < n; ++i) dst[i] = dst[i] * dryGain + w[i] * wetGain;
            }
        }

        // 2. Block-rate dynamics analysis (mono-summed peak).
        float blockPeak = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* p = buffer.getReadPointer (ch);
            for (int i = 0; i < n; ++i) blockPeak = juce::jmax (blockPeak, std::abs (p[i]));
        }
        // Fast: instant attack / ~80 ms release (pick transients).
        // Slow: ~400 ms one-pole (playing level).  Block-rate is plenty for
        // body-resonance breathing; coefs below only refresh at block rate.
        const float blockSec = (float) n / (float) mSampleRate;
        const float fastRel  = std::exp (-blockSec / 0.080f);
        const float slowCf   = 1.0f - std::exp (-blockSec / 0.400f);
        mEnvFast  = juce::jmax (blockPeak, mEnvFast * fastRel);
        mEnvSlow += slowCf * (blockPeak - mEnvSlow);

        // Playing level mapped over -42..-10 dBFS; transient = fast-over-slow.
        const float slowDb      = juce::Decibels::gainToDecibels (mEnvSlow, -60.0f);
        const float level01     = juce::jlimit (0.0f, 1.0f, (slowDb + 42.0f) / 32.0f);
        const float transient01 = juce::jlimit (0.0f, 1.0f,
            (mEnvFast - mEnvSlow) / juce::jmax (mEnvSlow, 1.0e-4f) * 0.5f);

        // 3. Macro map -- the Resonance knob is the CEILING of the adaptive
        //    depth.  Calibration starting points (ear-tunable):
        //    de-quack fully in by noon; body depth breathes 55-100 % with
        //    level; pick attacks bloom the modes; the air (lowest) mode gains
        //    extra weight past noon ("adds bass" upper half).  Per-body
        //    voicing (centers / weights / Qs / size tilt) from kVoicing[].
        const float k         = mResonance01;
        const float quackDb   = -7.0f * juce::jmin (1.0f, k * 2.0f);
        const float bodyDepth = k * (0.55f + 0.45f * level01);
        const float bloom     = transient01 * k;
        const float lowExtra  = juce::jmax (0.0f, k - 0.5f) * 2.0f;

        const int   bodyIdx = juce::jlimit (0, 2, (int) mBody);
        const auto& v       = kVoicing[bodyIdx];
        const float fcs[3]  = { v.air,  v.top,  v.body };
        const float qbs[3]  = { v.airQ, v.topQ, v.bodQ };
        const float modeDb[3] = {
            (bodyDepth * (5.0f + 4.0f * lowExtra) + bloom * 2.0f) * v.airW,   // air
            (bodyDepth * 4.5f                     + bloom * 3.0f) * v.topW,   // top (pick knock)
            (bodyDepth * 3.5f                     + bloom * 1.5f) * v.bodW    // body
        };
        // Broadband body voice scales with the KNOB only (k), not the
        // dynamics depth -- a body's size is static; tying it to bodyDepth
        // halved the A/B contrast at moderate playing levels.  The adaptive
        // breathing stays on the modes + bloom above.
        const float tiltDb = v.shelfDb * k;
        const float peakDb = v.peakDb  * k;

        // 4. CPU-guarded coef refresh (only on real movement / body switch).
        const bool bodyChanged = (mCoefBody != bodyIdx);
        for (int m = 0; m < 3; ++m)
        {
            const float qEff = qbs[m] / (1.0f + 0.30f * transient01);   // transients widen
            if (bodyChanged || std::abs (modeDb[m] - mAppliedModeDb[m]) > 0.05f
                            || std::abs (qEff     - mAppliedModeQ[m])  > 0.02f)
            {
                makePeakCoefs (mModes[m], mSampleRate, fcs[m], qEff, modeDb[m]);
                mAppliedModeDb[m] = modeDb[m];
                mAppliedModeQ[m]  = qEff;
            }
        }
        if (bodyChanged || std::abs (quackDb - mAppliedQuackDb) > 0.05f)
        {
            makePeakCoefs (mDeQuack, mSampleRate, 2000.0f, 1.1f, quackDb);
            mAppliedQuackDb = quackDb;
        }
        if (bodyChanged || std::abs (tiltDb - mAppliedTiltDb) > 0.05f)
        {
            makeLowShelfCoefs (mTilt, mSampleRate, v.shelfHz, 0.707f, tiltDb);
            mAppliedTiltDb = tiltDb;
        }
        if (bodyChanged || std::abs (peakDb - mAppliedPeakDb) > 0.05f)
        {
            makePeakCoefs (mBodyPeak, mSampleRate, v.peakHz, v.peakQ, peakDb);
            mAppliedPeakDb = peakDb;
        }
        if (bodyChanged)
        {
            mCoefBody = bodyIdx;
            for (auto& m : mModes) m.reset();   // clear stale ring on body switch
            mTilt.reset();
            mBodyPeak.reset();
        }

        // 5. In-place series pass:
        //    de-quack -> air -> top -> body -> shelf -> voicing peak.
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* p = buffer.getWritePointer (ch);
            if (ch == 0)
                for (int i = 0; i < n; ++i)
                {
                    float y = mDeQuack.processL (p[i]);
                    y = mModes[0].processL (y);
                    y = mModes[1].processL (y);
                    y = mModes[2].processL (y);
                    y = mTilt.processL (y);
                    p[i] = mBodyPeak.processL (y);
                }
            else
                for (int i = 0; i < n; ++i)
                {
                    float y = mDeQuack.processR (p[i]);
                    y = mModes[0].processR (y);
                    y = mModes[1].processR (y);
                    y = mModes[2].processR (y);
                    y = mTilt.processR (y);
                    p[i] = mBodyPeak.processR (y);
                }
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

    // ── Notch (band-stop, last in chain - surgical feedback rejection) ──────
    // OFF unless the panel knob leaves its bottom position (reference default).
    // Process bandpass tap and subtract from main buffer to get band-stop.
    if (mNotchOn)
    {
        if (mNotchScratch.getNumChannels() < numCh || mNotchScratch.getNumSamples() < n)
            mNotchScratch.setSize (juce::jmax (2, numCh), n, false, false, true);
        for (int ch = 0; ch < numCh; ++ch)
            mNotchScratch.copyFrom (ch, 0, buffer, ch, 0, n);
        juce::dsp::AudioBlock<float> bpBlk (mNotchScratch);
        auto bpSub = bpBlk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> bpCtx (bpSub);
        mNotch.process (bpCtx);
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* dst = buffer.getWritePointer (ch);
            const float* bp = mNotchScratch.getReadPointer (ch);
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
    state.setProperty ("notchOn",   (int) mNotchOn,       nullptr);
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
    setNotchOn   (((int)           state.getProperty ("notchOn",   0)) != 0);
    setLevelDb   ((float)(double)  state.getProperty ("levelDb",   0.0));
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;

    // Report here, not in reloadConvIR() -- prepare() and User-boundary body
    // flips re-run that outside the project-load drain window.
    if (mBody == Body::User && mUserIRPath.isNotEmpty()
        && ! ProjectFileResolver::resolve (mUserIRPath).existsAsFile())
        MissingFileReport::add ("Acoustic Preamp user IR", mUserIRPath);

    reloadConvIR();   // message-thread caller (preset/project load)
}
