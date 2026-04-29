#include "TapeDSP.h"
#include <cmath>

namespace
{
    constexpr float kSmoothMsFast   = 15.0f;
    constexpr float kSmoothMsMed    = 20.0f;
    constexpr float kSmoothMsShelf  = 20.0f;   // shelf gains
    constexpr float kSmoothMsVibe   = 30.0f;   // slightly slower on character params

    // Hysteresis alpha (per-sample integrator coefficient for `y += alpha * (shaped - y)`).
    // Spec formula at base rate: alpha_base = 0.2 + 0.8 * tapeSpeed
    //   -> TapeSpeed 0 (7.5 ips): alpha_base = 0.2  (~1.5 kHz cutoff at 48k; tape-dark)
    //   -> TapeSpeed 1 (30 ips):  alpha_base = 1.0  (no filtering; instantaneous tracking)
    // Scaled for OS rate: alpha_os = alpha_base / osFactor  (preserves cutoff frequency
    // whatever OS factor is selected; per spec 10c note).
    // HystAmount scales the memory: 0 = bypass (alpha=1), 1 = spec, 2 = more memory (alpha/2).
    inline float computeHystAlpha (float tapeSpeed, float hystAmount, int osFactor) noexcept
    {
        if (hystAmount <= 0.001f) return 1.0f;   // bypass -> y = shaped (no memory)
        const float alphaBase   = 0.2f + 0.8f * juce::jlimit (0.0f, 1.0f, tapeSpeed);
        const float alphaScaled = alphaBase / juce::jmax (0.01f, hystAmount);
        return juce::jlimit (0.001f, 1.0f,
                             alphaScaled / (float) juce::jmax (1, osFactor));
    }
}

// -- cubic circular helper (10e) ---------------------------------------------

float TapeDSP::cubicCircular (const std::vector<float>& buf, int writePos,
                              int iOff, float frac, int size) noexcept
{
    // Read position is writePos - iOff - frac (frac=0 means exactly at writePos - iOff,
    // frac->1 means one full sample OLDER). Since mWowWritePos increments forward each
    // sample, moving backward in the buffer (smaller index, with wrap) goes further into
    // the past.
    //
    // 4 points needed for cubic Hermite:
    //   ym1 at writePos - iOff + 1  (one sample NEWER than y0; buffer index base+1)
    //   y0  at writePos - iOff      (newer bracket; buffer index = base)
    //   y1  at writePos - iOff - 1  (older bracket; buffer index base-1)
    //   y2  at writePos - iOff - 2  (two samples OLDER; buffer index base-2)
    const int baseIdx = (writePos - iOff + size) % size;
    const int idxYm1  = (baseIdx + 1) % size;
    const int idxY0   = baseIdx;
    const int idxY1   = (baseIdx - 1 + size) % size;
    const int idxY2   = (baseIdx - 2 + size) % size;
    const float ym1 = buf[(size_t) idxYm1];
    const float y0  = buf[(size_t) idxY0];
    const float y1  = buf[(size_t) idxY1];
    const float y2  = buf[(size_t) idxY2];

    // 4-point, 3rd-order Hermite (aka Catmull-Rom with t=frac in [0,1] between y0 and y1).
    // Verified: returns y0 exactly at frac=0 and y1 exactly at frac=1, with C1 continuity
    // across sample boundaries (so no clicks when the read position crosses integer bounds).
    const float c0 = y0;
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

// -- Helpers ------------------------------------------------------------------

void TapeDSP::updateFilters()
{
    if (mSampleRate <= 0.0) return;
    const float sr    = (float) mSampleRate;
    const float twoPi = juce::MathConstants<float>::twoPi;

    mPreShelfCoef = std::exp (-twoPi * 5000.0f / sr);
    mDeShelfCoef  = std::exp (-twoPi * 4000.0f / sr);
    mPreShelfGainLin = juce::Decibels::decibelsToGain (mPreShelfDb);
    mDeShelfGainLin  = juce::Decibels::decibelsToGain (mDeShelfDb);
    mHissHpCoef = std::exp (-twoPi * 200.0f / sr);
    mDcCoef     = 1.0f - twoPi * 5.0f / sr;

    // Hysteresis alpha computed at OS rate (spec: alpha_os = alpha_base / osFactor).
    const int osFactor = 1 << juce::jlimit (1, 4, mOsLog2);
    mHystAlphaOS = computeHystAlpha (mTapeSpeed, mHystAmount, osFactor);
}

void TapeDSP::allocateScratch()
{
    if (mMaxBlock <= 0) return;
    mBandBuf.setSize (2, mMaxBlock, false, true, true);
    mBandBuf.clear();

    const int bufSize = (int) std::ceil (kMaxWowMs * 0.001 * mSampleRate) + 16;
    mWowBufL.assign ((size_t) bufSize, 0.0f);
    mWowBufR.assign ((size_t) bufSize, 0.0f);
    mWowWritePos = 0;

    const size_t n = (size_t) mMaxBlock;
    mVibeScr         .assign (n, mVibe);
    mHystAlphaOSScr  .assign (n, mHystAlphaOS);
    mBiasScr         .assign (n, mBias);
    mPreGainScr      .assign (n, mPreShelfGainLin);
    mDeGainScr       .assign (n, mDeShelfGainLin);
    mInGainScr       .assign (n, mInputGain);
    mOutGainScr      .assign (n, mOutputGain);
    mWowDepthScr     .assign (n, mWowDepth);
    mFlutterDepthScr .assign (n, mFlutterDepth);
    mHissScr         .assign (n, mHiss);
}

void TapeDSP::snapSmoothedToTargets()
{
    mVibeSmooth         .setCurrentAndTargetValue (mVibe);
    mHystAlphaOSSmooth  .setCurrentAndTargetValue (mHystAlphaOS);
    mBiasSmooth         .setCurrentAndTargetValue (mBias);
    mPreGainSmooth      .setCurrentAndTargetValue (mPreShelfGainLin);
    mDeGainSmooth       .setCurrentAndTargetValue (mDeShelfGainLin);
    mInGainSmooth       .setCurrentAndTargetValue (mInputGain);
    mOutGainSmooth      .setCurrentAndTargetValue (mOutputGain);
    mWowDepthSmooth     .setCurrentAndTargetValue (mWowDepth);
    mFlutterDepthSmooth .setCurrentAndTargetValue (mFlutterDepth);
    mHissSmooth         .setCurrentAndTargetValue (mHiss);
}

// -- DSPBase interface --------------------------------------------------------

void TapeDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;
    updateFilters();

    mOversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2 /* numChannels */,
        juce::jlimit (1, 4, mOsLog2),
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true /* maxQuality */);
    mOversampler->initProcessing ((size_t) maxBlockSize);
    mOversampler->reset();
    mLatencySamples = (int) std::ceil (mOversampler->getLatencyInSamples());

    allocateScratch();

    mVibeSmooth         .reset (sampleRate, kSmoothMsVibe  * 0.001);
    mHystAlphaOSSmooth  .reset (sampleRate, kSmoothMsVibe  * 0.001);
    mBiasSmooth         .reset (sampleRate, kSmoothMsMed   * 0.001);
    mPreGainSmooth      .reset (sampleRate, kSmoothMsShelf * 0.001);
    mDeGainSmooth       .reset (sampleRate, kSmoothMsShelf * 0.001);
    mInGainSmooth       .reset (sampleRate, kSmoothMsFast  * 0.001);
    mOutGainSmooth      .reset (sampleRate, kSmoothMsFast  * 0.001);
    mWowDepthSmooth     .reset (sampleRate, kSmoothMsMed   * 0.001);
    mFlutterDepthSmooth .reset (sampleRate, kSmoothMsMed   * 0.001);
    mHissSmooth         .reset (sampleRate, kSmoothMsMed   * 0.001);
    snapSmoothedToTargets();

    reset();
}

void TapeDSP::reset()
{
    std::fill (mWowBufL.begin(), mWowBufL.end(), 0.0f);
    std::fill (mWowBufR.begin(), mWowBufR.end(), 0.0f);
    mWowWritePos        = 0;
    mWowPhase           = 0.0;
    mFlutterPhase       = 0.0;
    mFlutterNoiseState  = 0.0f;

    mHystL = mHystR = 0.0f;
    mPreLP_L = mPreLP_R = 0.0f;
    mDeLP_L  = mDeLP_R  = 0.0f;

    mPinkL = PinkState{};
    mPinkR = PinkState{};
    mHissHPx_L = mHissHPy_L = mHissHPx_R = mHissHPy_R = 0.0f;
    mDcX_L = mDcY_L = mDcX_R = mDcY_R = 0.0f;

    if (mOversampler) mOversampler->reset();
    mBandBuf.clear();
}

// -- Setters (A2: CPU-guarded) ------------------------------------------------

void TapeDSP::setVibe (float v)
{
    const float n = juce::jlimit (0.0f, 1.0f, v);
    if (n != mVibe) { mVibe = n; mVibeSmooth.setTargetValue (n); }
}

void TapeDSP::setTapeSpeed (float v)
{
    const float n = juce::jlimit (0.0f, 1.0f, v);
    if (n != mTapeSpeed)
    {
        mTapeSpeed = n;
        const int osFactor = 1 << juce::jlimit (1, 4, mOsLog2);
        mHystAlphaOS = computeHystAlpha (mTapeSpeed, mHystAmount, osFactor);
        mHystAlphaOSSmooth.setTargetValue (mHystAlphaOS);
    }
}

void TapeDSP::setHystAmount (float v)
{
    const float n = juce::jlimit (0.0f, 2.0f, v);
    if (n != mHystAmount)
    {
        mHystAmount = n;
        const int osFactor = 1 << juce::jlimit (1, 4, mOsLog2);
        mHystAlphaOS = computeHystAlpha (mTapeSpeed, mHystAmount, osFactor);
        mHystAlphaOSSmooth.setTargetValue (mHystAlphaOS);
    }
}

void TapeDSP::setWowRate (float hz)
{
    const float n = juce::jlimit (0.1f, 5.0f, hz);
    if (n != mWowRate) mWowRate = n;
}

void TapeDSP::setWowDepth (float d)
{
    const float n = juce::jlimit (0.0f, 1.0f, d);
    if (n != mWowDepth) { mWowDepth = n; mWowDepthSmooth.setTargetValue (n); }
}

void TapeDSP::setFlutterRate (float hz)
{
    const float n = juce::jlimit (1.0f, 15.0f, hz);
    if (n != mFlutterRate) mFlutterRate = n;
}

void TapeDSP::setFlutterDepth (float d)
{
    const float n = juce::jlimit (0.0f, 1.0f, d);
    if (n != mFlutterDepth) { mFlutterDepth = n; mFlutterDepthSmooth.setTargetValue (n); }
}

void TapeDSP::setInputGain (float linGain)
{
    const float n = juce::jmax (0.0f, linGain);
    if (n != mInputGain) { mInputGain = n; mInGainSmooth.setTargetValue (n); }
}

void TapeDSP::setOutputGain (float linGain)
{
    const float n = juce::jmax (0.0f, linGain);
    if (n != mOutputGain) { mOutputGain = n; mOutGainSmooth.setTargetValue (n); }
}

void TapeDSP::setHiss (float h)
{
    const float n = juce::jlimit (0.0f, 1.0f, h);
    if (n != mHiss) { mHiss = n; mHissSmooth.setTargetValue (n); }
}

void TapeDSP::setBias (float v)
{
    const float n = juce::jlimit (0.0f, 10.0f, v);
    if (n != mBias) { mBias = n; mBiasSmooth.setTargetValue (n); }
}

void TapeDSP::setPreShelfDb (float dB)
{
    const float n = juce::jlimit (-12.0f, 12.0f, dB);
    if (n != mPreShelfDb)
    {
        mPreShelfDb = n;
        mPreShelfGainLin = juce::Decibels::decibelsToGain (n);
        mPreGainSmooth.setTargetValue (mPreShelfGainLin);
    }
}

void TapeDSP::setDeShelfDb (float dB)
{
    const float n = juce::jlimit (-12.0f, 12.0f, dB);
    if (n != mDeShelfDb)
    {
        mDeShelfDb = n;
        mDeShelfGainLin = juce::Decibels::decibelsToGain (n);
        mDeGainSmooth.setTargetValue (mDeShelfGainLin);
    }
}

void TapeDSP::setOsLog2 (int factorLog2)
{
    const int n = juce::jlimit (1, 4, factorLog2);
    if (n == mOsLog2) return;
    mOsLog2 = n;
    if (mSampleRate > 0.0)
        prepare (mSampleRate, mMaxBlock);
}

// -- Process block ------------------------------------------------------------

void TapeDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;

    juce::ScopedNoDenormals noDenormals;   // A1

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0) return;
    if (numSamples > mMaxBlock) return;
    if (mOversampler == nullptr) return;
    if (mWowBufL.empty()) return;
    if (mBandBuf.getNumSamples() < numSamples) return;

    const int numCh   = std::min (buffer.getNumChannels(), 2);
    const int numChOS = 2;
    const int kOsNow  = 1 << juce::jlimit (1, 4, mOsLog2);
    const int wowSize = (int) mWowBufL.size();

    const float sr        = (float) mSampleRate;
    const float a_preC    = mPreShelfCoef;
    const float a_deC     = mDeShelfCoef;
    const float a_hissHp  = mHissHpCoef;
    const float a_dc      = mDcCoef;

    // ---- Phase 0: consume smoothers into scratch arrays --------------------
    for (int n = 0; n < numSamples; ++n)
    {
        mVibeScr[(size_t) n]         = mVibeSmooth        .getNextValue();
        mHystAlphaOSScr[(size_t) n]  = mHystAlphaOSSmooth .getNextValue();
        mBiasScr[(size_t) n]         = mBiasSmooth        .getNextValue();
        mPreGainScr[(size_t) n]      = mPreGainSmooth     .getNextValue();
        mDeGainScr[(size_t) n]       = mDeGainSmooth      .getNextValue();
        mInGainScr[(size_t) n]       = mInGainSmooth      .getNextValue();
        mOutGainScr[(size_t) n]      = mOutGainSmooth     .getNextValue();
        mWowDepthScr[(size_t) n]     = mWowDepthSmooth    .getNextValue();
        mFlutterDepthScr[(size_t) n] = mFlutterDepthSmooth.getNextValue();
        mHissScr[(size_t) n]         = mHissSmooth        .getNextValue();
    }

    // ---- Phase 1: input gain + pre-emphasis (base rate) --------------------
    // Write into mBandBuf (will be handed to oversampler for Phase 2).
    for (int ch = 0; ch < numChOS; ++ch)
    {
        float* dst = mBandBuf.getWritePointer (ch);
        if (ch < numCh)
        {
            const float* src = buffer.getReadPointer (ch);
            float& preLP = (ch == 0) ? mPreLP_L : mPreLP_R;
            for (int n = 0; n < numSamples; ++n)
            {
                float x = src[n] * mInGainScr[(size_t) n];
                // Pre-emphasis: shelf_out = G*x + (1-G)*LP(x)
                preLP = (1.0f - a_preC) * x + a_preC * preLP;
                const float gPre = mPreGainScr[(size_t) n];
                x = gPre * x + (1.0f - gPre) * preLP;
                dst[n] = x;
            }
        }
        else
        {
            // Mono input: pad channel 1 from channel 0's processed stream.
            const float* src0 = mBandBuf.getReadPointer (0);
            for (int n = 0; n < numSamples; ++n)
                dst[n] = src0[n];
        }
    }

    // ---- Phase 2: oversampled shaper + hysteresis --------------------------
    juce::dsp::AudioBlock<float> bandBlock (mBandBuf.getArrayOfWritePointers(),
                                             (size_t) numChOS, 0, (size_t) numSamples);
    auto upBlock = mOversampler->processSamplesUp (bandBlock);
    const int upN = (int) upBlock.getNumSamples();
    jassert (upN == numSamples * kOsNow);

    for (int ch = 0; ch < numChOS; ++ch)
    {
        float* up = upBlock.getChannelPointer ((size_t) ch);
        float& hyst = (ch == 0) ? mHystL : mHystR;
        for (int osN = 0; osN < upN; ++osN)
        {
            const int baseIdx = osN / kOsNow;
            const float vibeNow   = mVibeScr[(size_t) baseIdx];
            const float biasNow   = mBiasScr[(size_t) baseIdx];
            const float alphaNow  = mHystAlphaOSScr[(size_t) baseIdx];

            const float x = up[osN];
            // C5 Bias: pre-shaper DC offset injection (-0.1..+0.1 at extremes).
            const float biasedX = x + (biasNow - 5.0f) * 0.02f;
            // 10b Asymmetric sigmoid; k = 0.3 * Vibe (spec default relationship).
            const float k = 0.3f * vibeNow;
            const float shaped = asymShaper (biasedX, k);
            // 10a Hysteresis: y += alpha * (shaped - y)
            hyst += alphaNow * (shaped - hyst);
            up[osN] = hyst;
        }
    }
    mOversampler->processSamplesDown (bandBlock);   // mBandBuf now holds shaped+hyst signal

    // ---- Phase 3: de-emphasis + wow/flutter + hiss + DC + output -----------
    float* Lout = numCh > 0 ? buffer.getWritePointer (0) : nullptr;
    float* Rout = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    const float* processedL = mBandBuf.getReadPointer (0);
    const float* processedR = mBandBuf.getReadPointer (1);

    const float maxWowSamples = (float) (kMaxWowMs * 0.5 * 0.001 * mSampleRate);
    const double centreDelay  = kMaxWowMs * 0.5 * 0.001 * mSampleRate;

    for (int n = 0; n < numSamples; ++n)
    {
        float sampL = processedL[n];
        float sampR = (numCh > 1) ? processedR[n] : sampL;

        // De-emphasis (base rate shelf)
        const float gDe = mDeGainScr[(size_t) n];
        mDeLP_L = (1.0f - a_deC) * sampL + a_deC * mDeLP_L;
        sampL   = gDe * sampL + (1.0f - gDe) * mDeLP_L;
        mDeLP_R = (1.0f - a_deC) * sampR + a_deC * mDeLP_R;
        sampR   = gDe * sampR + (1.0f - gDe) * mDeLP_R;

        // Write shaped+de-emphasized signal into the wow/flutter delay buffer.
        mWowBufL[(size_t) mWowWritePos] = sampL;
        mWowBufR[(size_t) mWowWritePos] = sampR;

        // LFO update - A3 while-wrap (replaces std::fmod).
        const float wowLfo     = (float) std::sin (juce::MathConstants<double>::twoPi * mWowPhase);
        const float flutterSin = (float) std::sin (juce::MathConstants<double>::twoPi * mFlutterPhase);
        // Flutter noise: smoothed white, heavy LP (alpha 0.001 = ~7.6 Hz cutoff at 48k).
        // Real tape mechanical capstan wobble is sub-20 Hz; higher bandwidth here becomes
        // audible as delay-line jitter/static when used as a modulator. Original spec
        // suggested ~300 Hz LP but that produced harsh "static" when flutter depth rose
        // above ~0.13 (noise-modulated delay creates rapid pitch jitter in the audible band).
        const float rawNoise = mRandomL.nextFloat() * 2.0f - 1.0f;
        mFlutterNoiseState = 0.999f * mFlutterNoiseState + 0.001f * rawNoise;
        // Mix: 90% clean sine, 10% sub-audible noise drift - keeps flutter musical at full depth.
        const float flutter = 0.9f * flutterSin + 0.1f * mFlutterNoiseState;

        const float wowDepthNow     = mWowDepthScr[(size_t) n];
        const float flutterDepthNow = mFlutterDepthScr[(size_t) n];
        const float totalModSmp =
            maxWowSamples * (wowDepthNow * wowLfo + flutterDepthNow * flutter * 0.3f);

        const float readOffsetF = juce::jlimit (0.0f, (float) (wowSize - 4),
                                                (float) centreDelay + totalModSmp);
        const int   iOff = (int) readOffsetF;
        const float frac = readOffsetF - (float) iOff;

        // 10e Cubic-interp delay reads (replaces linear).
        float wowL = cubicCircular (mWowBufL, mWowWritePos, iOff, frac, wowSize);
        float wowR = cubicCircular (mWowBufR, mWowWritePos, iOff, frac, wowSize);

        // 10g Pink-filtered hiss + HPF 200 Hz (independent L/R RNG streams).
        const float hissNow = mHissScr[(size_t) n];
        const float whiteL = mRandomL.nextFloat() * 2.0f - 1.0f;
        const float whiteR = mRandomR.nextFloat() * 2.0f - 1.0f;
        const float pinkL = mPinkL.process (whiteL);
        const float pinkR = mPinkR.process (whiteR);
        const float hpL = a_hissHp * (mHissHPy_L + pinkL - mHissHPx_L);
        const float hpR = a_hissHp * (mHissHPy_R + pinkR - mHissHPx_R);
        mHissHPx_L = pinkL; mHissHPy_L = hpL;
        mHissHPx_R = pinkR; mHissHPy_R = hpR;
        wowL += hissNow * hpL * 0.1f;
        wowR += hissNow * hpR * 0.1f;

        // 10h DC blocker (5 Hz SR-aware).
        const float dcInL = wowL;
        wowL = dcInL - mDcX_L + a_dc * mDcY_L;
        mDcX_L = dcInL; mDcY_L = wowL;
        const float dcInR = wowR;
        wowR = dcInR - mDcX_R + a_dc * mDcY_R;
        mDcX_R = dcInR; mDcY_R = wowR;

        // Output gain
        const float outLin = mOutGainScr[(size_t) n];
        if (Lout) Lout[n] = wowL * outLin;
        if (Rout) Rout[n] = wowR * outLin;

        mWowWritePos = (mWowWritePos + 1) % wowSize;

        // A3 while-wrap LFO phases (cheap: positive deltas only).
        mWowPhase     += (double) mWowRate     / (double) sr;
        while (mWowPhase     >= 1.0) mWowPhase     -= 1.0;
        mFlutterPhase += (double) mFlutterRate / (double) sr;
        while (mFlutterPhase >= 1.0) mFlutterPhase -= 1.0;
    }
}

// -- Serialisation ------------------------------------------------------------

void TapeDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("TapeDSP");
    state.setProperty ("vibe",         mVibe,         nullptr);
    state.setProperty ("tapeSpeed",    mTapeSpeed,    nullptr);   // new
    state.setProperty ("hystAmount",   mHystAmount,   nullptr);   // new C1
    state.setProperty ("wowRate",      mWowRate,      nullptr);
    state.setProperty ("wowDepth",     mWowDepth,     nullptr);
    state.setProperty ("flutterRate",  mFlutterRate,  nullptr);   // new 10d
    state.setProperty ("flutterDepth", mFlutterDepth, nullptr);   // new 10d
    state.setProperty ("inputGain",    mInputGain,    nullptr);
    state.setProperty ("outputGain",   mOutputGain,   nullptr);
    state.setProperty ("hiss",         mHiss,         nullptr);
    state.setProperty ("bias",         mBias,         nullptr);   // new C5
    state.setProperty ("preShelfDb",   mPreShelfDb,   nullptr);   // new C3
    state.setProperty ("deShelfDb",    mDeShelfDb,    nullptr);   // new C3
    state.setProperty ("osLog2",       mOsLog2,       nullptr);   // new C2
    auto xml = state.createXml();
    if (xml) juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void TapeDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (!xml || xml->getTagName() != "TapeDSP") return;

    mVibe         = (float) xml->getDoubleAttribute ("vibe",         mVibe);
    mTapeSpeed    = (float) xml->getDoubleAttribute ("tapeSpeed",    mTapeSpeed);
    mHystAmount   = (float) xml->getDoubleAttribute ("hystAmount",   mHystAmount);
    mWowRate      = (float) xml->getDoubleAttribute ("wowRate",      mWowRate);
    mWowDepth     = (float) xml->getDoubleAttribute ("wowDepth",     mWowDepth);
    mFlutterRate  = (float) xml->getDoubleAttribute ("flutterRate",  mFlutterRate);
    mFlutterDepth = (float) xml->getDoubleAttribute ("flutterDepth", mFlutterDepth);
    mInputGain    = (float) xml->getDoubleAttribute ("inputGain",    mInputGain);
    mOutputGain   = (float) xml->getDoubleAttribute ("outputGain",   mOutputGain);
    mHiss         = (float) xml->getDoubleAttribute ("hiss",         mHiss);
    mBias         = (float) xml->getDoubleAttribute ("bias",         mBias);
    mPreShelfDb   = (float) xml->getDoubleAttribute ("preShelfDb",   mPreShelfDb);
    mDeShelfDb    = (float) xml->getDoubleAttribute ("deShelfDb",    mDeShelfDb);
    const int loadedOs = xml->getIntAttribute ("osLog2", mOsLog2);
    const bool osChanged = (loadedOs != mOsLog2);
    mOsLog2 = juce::jlimit (1, 4, loadedOs);

    // Re-derive dependent state + snap smoothers.
    snapSmoothedToTargets();
    if (mSampleRate > 0.0)
    {
        if (osChanged) prepare (mSampleRate, mMaxBlock);
        else           updateFilters();
        reset();
    }
}
