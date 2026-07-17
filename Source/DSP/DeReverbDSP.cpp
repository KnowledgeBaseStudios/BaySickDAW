#include "DeReverbDSP.h"

DeReverbDSP::DeReverbDSP()
{
    mWindow.resize ((size_t) kFFT);
    for (int i = 0; i < kFFT; ++i)
        mWindow[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                      * (float) i / (float) kFFT);
    // Hann analysis x Hann synthesis at 75% overlap sums to 1.5; JUCE's
    // inverse FFT already divides by N, so 2/3 restores unity.
    mOlaScale = 2.0f / 3.0f;

    for (auto& s : mSpec) s.resize ((size_t) kFFT);
    mFftA.resize ((size_t) kFFT);
    mFftS.resize ((size_t) kFFT);
    mMagHist.assign ((size_t) (kHist * kBins), 0.0f);
    mRev.assign ((size_t) kBins, 0.0f);
    mGain.assign ((size_t) kBins, 1.0f);
    mGainSm.assign ((size_t) kBins, 1.0f);

    for (auto& c : mCh)
    {
        c.inRing.assign ((size_t) kFFT * 4, 0.0f);
        c.outBuf.assign ((size_t) kFFT * 8, 0.0f);
    }
    updateDecay();
}

void DeReverbDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;
    updateDecay();
    reset();
}

void DeReverbDSP::reset()
{
    for (auto& c : mCh)
    {
        std::fill (c.inRing.begin(), c.inRing.end(), 0.0f);
        std::fill (c.outBuf.begin(), c.outBuf.end(), 0.0f);
        c.inWrite = c.inAvail = c.inRead = 0;
        // Prime one FFT frame of silence so output timing == input + kFFT
        // for every block size (the COLA identity contract).
        c.outReadAbs  = 0;
        c.outWriteAbs = kFFT;
    }
    std::fill (mMagHist.begin(), mMagHist.end(), 0.0f);
    std::fill (mRev.begin(),     mRev.end(),     0.0f);
    std::fill (mGain.begin(),    mGain.end(),    1.0f);
    std::fill (mGainSm.begin(),  mGainSm.end(),  1.0f);
    mHistWrite = 0;
    mGrDb.store (0.0f, std::memory_order_relaxed);
}

void DeReverbDSP::updateDecay()
{
    const double sr    = juce::jmax (8000.0, mSampleRate);
    const double hopMs = 1000.0 * (double) kHop / sr;
    // T60 semantics: the modeled tail loses 60 dB over tailMs.
    mDecayPerHop = (float) std::pow (10.0, -3.0 * hopMs / juce::jmax (50.0, (double) tailMs));
}

void DeReverbDSP::setReductionPct (float pct)
{
    const float n = juce::jlimit (0.0f, 100.0f, pct);
    if (n != reductionPct) reductionPct = n;
}

void DeReverbDSP::setTailMs (float ms)
{
    const float n = juce::jlimit (100.0f, 1000.0f, ms);
    if (n != tailMs) { tailMs = n; updateDecay(); }
}

void DeReverbDSP::setMixPct (float pct)
{
    const float n = juce::jlimit (0.0f, 100.0f, pct);
    if (n != mixPct) mixPct = n;
}

void DeReverbDSP::processFrame (int numCh)
{
    // Analysis: windowed FFT per channel from each input ring.
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& c = mCh[ch];
        const int ringSz = (int) c.inRing.size();
        for (int i = 0; i < kFFT; ++i)
            mFftA[(size_t) i] = { c.inRing[(size_t) ((c.inRead + i) % ringSz)]
                                    * mWindow[(size_t) i], 0.0f };
        mFFT.perform (mFftA.data(), mSpec[ch].data(), false);
        c.inRead   = (c.inRead + kHop) % ringSz;
        c.inAvail -= kHop;
    }

    // Channel-average magnitude drives one shared gain field.
    float* hist = &mMagHist[(size_t) (mHistWrite * kBins)];
    const int histDelayed = (mHistWrite + kHist - kDelayFrames) % kHist;
    const float* delayed = &mMagHist[(size_t) (histDelayed * kBins)];

    const float reduction = 2.0f * (reductionPct / 100.0f);       // 0..2x over-subtract
    const float mix       = mixPct / 100.0f;
    constexpr float kGainFloor = 0.2f;                            // -14 dB max cut
    constexpr float kFeed      = 0.7f;                            // tail seed proportion
    float minG = 1.0f;

    for (int k = 0; k < kBins; ++k)
    {
        float mag = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            mag += std::abs (mSpec[ch][(size_t) k]);
        mag /= (float) numCh;

        // Running tail estimate: decayed max of itself vs the delayed signal.
        float& rev = mRev[(size_t) k];
        rev = juce::jmax (rev * mDecayPerHop, delayed[(size_t) k] * kFeed);

        float g = (mag > 1.0e-9f)
                    ? juce::jlimit (kGainFloor, 1.0f, 1.0f - reduction * rev / mag)
                    : 1.0f;
        g = 1.0f + mix * (g - 1.0f);                              // Mix = lerp toward unity
        mGain[(size_t) k] = g;

        hist[(size_t) k] = mag;
    }
    mHistWrite = (mHistWrite + 1) % kHist;

    // Musical-noise suppression: 3-bin frequency smoothing + frame-rate time
    // smoothing (suppression engages fast, releases slower).
    for (int k = 0; k < kBins; ++k)
    {
        const float a = mGain[(size_t) juce::jmax (0, k - 1)];
        const float b = mGain[(size_t) k];
        const float c2 = mGain[(size_t) juce::jmin (kBins - 1, k + 1)];
        const float target = (a + b + c2) / 3.0f;
        float& sm = mGainSm[(size_t) k];
        const float coef = (target < sm) ? 0.6f : 0.3f;
        sm += coef * (target - sm);
        minG = juce::jmin (minG, sm);
    }
    mGrDb.store (juce::Decibels::gainToDecibels (juce::jmax (1.0e-4f, minG)),
                 std::memory_order_relaxed);

    // Synthesis: apply the shared gains, IFFT, window, overlap-add.
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& c = mCh[ch];
        for (int k = 0; k < kBins; ++k)
        {
            const float g = mGainSm[(size_t) k];
            mFftA[(size_t) k] = mSpec[ch][(size_t) k] * g;
            if (k > 0 && k < kBins - 1)
                mFftA[(size_t) (kFFT - k)] = mSpec[ch][(size_t) (kFFT - k)] * g;
        }
        mFFT.perform (mFftA.data(), mFftS.data(), true);

        const int outSz = (int) c.outBuf.size();
        for (int i = 0; i < kFFT; ++i)
        {
            const int64_t abs = c.outWriteAbs + i;
            c.outBuf[(size_t) (abs % outSz)] += mFftS[(size_t) i].real()
                                                  * mWindow[(size_t) i] * mOlaScale;
        }
        c.outWriteAbs += kHop;
    }
}

void DeReverbDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals nd;
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int n     = buffer.getNumSamples();
    if (numCh <= 0 || n <= 0) return;

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& c = mCh[ch];
        const int ringSz = (int) c.inRing.size();
        const float* in = buffer.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
        {
            c.inRing[(size_t) c.inWrite] = in[i];
            c.inWrite = (c.inWrite + 1) % ringSz;
        }
        c.inAvail += n;
    }

    while (mCh[0].inAvail >= kFFT)
        processFrame (numCh);

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& c = mCh[ch];
        const int outSz = (int) c.outBuf.size();
        float* out = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            const int64_t abs = c.outReadAbs + i;
            const size_t idx = (size_t) (abs % outSz);
            out[i] = c.outBuf[idx];
            c.outBuf[idx] = 0.0f;                    // consumed -> clear for reuse
        }
        c.outReadAbs += n;
    }
}
