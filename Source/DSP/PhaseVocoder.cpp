#include "PhaseVocoder.h"
#include <cmath>

static constexpr float kTwoPi = 6.28318530717958647692f;

// ── Constructor ───────────────────────────────────────────────────────────────

PhaseVocoder::PhaseVocoder (int numChannels)
    : mFFT   (kFFTOrder)
    , mNumCh (numChannels)
    , mWindow (kFFTSize, 0.0f)
    , mFftA  (kFFTSize)
    , mFftS  (kFFTSize)
{
    // Hann window
    for (int i = 0; i < kFFTSize; ++i)
        mWindow[i] = 0.5f * (1.0f - std::cos (kTwoPi * i / (kFFTSize - 1)));

    // OLA normalization: window applied twice (analysis + synthesis), so the
    // overlapping frames sum Hann^2 at the SYNTHESIS hop: sum_j(w[n-j*Hs]^2)
    // ~= (3/8)*N / Hs per sample -> scale = (8/3)*Hs/N (= 2/3 at Hs = N/4).
    // juce::dsp::FFT already normalizes the inverse by 1/N internally, so
    // there is NO extra N to cancel -- the old (2/3)/N assumed unnormalized
    // IFFT and made stretched output ~1/N (~-66 dB, "clips don't play").
    // Recomputed in setStretchRatio because Hs tracks the stretch ratio.
    mWindowScale = (8.0f / 3.0f) * (float) mSynthHop / (float) kFFTSize;

    mCh.resize (numChannels);
    for (auto& c : mCh)
    {
        c.inRing    .assign (kFFTSize * 4, 0.0f);
        c.lastPhase .assign (kNumBins,     0.0f);
        c.phaseAccum.assign (kNumBins,     0.0f);
        c.outBuf    .assign (kFFTSize * 8, 0.0f);
    }
}

// ── setStretchRatio ───────────────────────────────────────────────────────────

void PhaseVocoder::setStretchRatio (double ratio)
{
    if (ratio == mStretchRatio) return;
    mStretchRatio = ratio;
    mSynthHop     = juce::jmax (1, juce::roundToInt (kHopSize * ratio));
    // Overlap density changes with Hs -> keep unity output level at any ratio.
    mWindowScale  = (8.0f / 3.0f) * (float) mSynthHop / (float) kFFTSize;
}

// ── reset ─────────────────────────────────────────────────────────────────────

void PhaseVocoder::reset()
{
    for (auto& c : mCh)
    {
        std::fill (c.inRing    .begin(), c.inRing    .end(), 0.0f);
        std::fill (c.lastPhase .begin(), c.lastPhase .end(), 0.0f);
        std::fill (c.phaseAccum.begin(), c.phaseAccum.end(), 0.0f);
        std::fill (c.outBuf    .begin(), c.outBuf    .end(), 0.0f);
        c.inRead = c.inWrite = c.inAvail = 0;
        c.outReadAbs = c.outWriteAbs = 0;
    }
}

// ── push ──────────────────────────────────────────────────────────────────────

void PhaseVocoder::push (const juce::AudioBuffer<float>& in,
                          int startSample, int numSamples)
{
    const int inRingSize = (int) mCh[0].inRing.size();

    for (int ch = 0; ch < juce::jmin (mNumCh, in.getNumChannels()); ++ch)
    {
        auto& c = mCh[ch];
        const float* src = in.getReadPointer (ch) + startSample;

        for (int i = 0; i < numSamples; ++i)
        {
            c.inRing[c.inWrite] = src[i];
            c.inWrite = (c.inWrite + 1) % inRingSize;
        }
        c.inAvail += numSamples;
    }

    // Process as many complete analysis frames as possible
    while (mCh[0].inAvail >= kFFTSize)
        processFrame();
}

// ── pull ──────────────────────────────────────────────────────────────────────

int PhaseVocoder::pull (juce::AudioBuffer<float>& out,
                         int startSample, int numSamples)
{
    const int avail  = getOutputAvailable();
    const int toPull = juce::jmin (numSamples, avail);
    if (toPull <= 0) return 0;

    const int outBufSize = (int) mCh[0].outBuf.size();

    for (int ch = 0; ch < juce::jmin (mNumCh, out.getNumChannels()); ++ch)
    {
        auto& c = mCh[ch];
        float* dst = out.getWritePointer (ch) + startSample;

        for (int i = 0; i < toPull; ++i)
        {
            const int physIdx = (int) ((c.outReadAbs + i) % outBufSize);
            dst[i] = c.outBuf[physIdx];
            c.outBuf[physIdx] = 0.0f;  // clear after read - essential for OLA correctness
        }

        c.outReadAbs += toPull;
    }

    return toPull;
}

// ── getOutputAvailable ────────────────────────────────────────────────────────

int PhaseVocoder::getOutputAvailable() const
{
    if (mCh.empty()) return 0;
    const auto& c = mCh[0];
    // Output is "settled" (all overlapping frames have contributed) once the OLA
    // write head is at least (kFFTSize - mSynthHop) ahead of the read head.
    const int settling = kFFTSize - mSynthHop;
    const int produced = (int) (c.outWriteAbs - c.outReadAbs);
    return juce::jmax (0, produced - settling);
}

// ── processFrame (core algorithm) ─────────────────────────────────────────────

void PhaseVocoder::processFrame()
{
    const int inRingSize = (int) mCh[0].inRing.size();
    const int outBufSize = (int) mCh[0].outBuf.size();

    // Precomputed per-bin expected phase advance per analysis hop
    // omega[k] = 2π * k * Ha / N
    // We compute it inline to avoid a separate table.

    for (int ch = 0; ch < mNumCh; ++ch)
    {
        auto& c = mCh[ch];

        // ── 1. Build windowed analysis frame ──────────────────────────────
        // Read kFFTSize samples from [inRead .. inRead+kFFTSize) (circular).
        for (int i = 0; i < kFFTSize; ++i)
        {
            const int srcIdx = (c.inRead + i) % inRingSize;
            mFftA[i] = { c.inRing[srcIdx] * mWindow[i], 0.0f };
        }

        // ── 2. Forward FFT ────────────────────────────────────────────────
        mFFT.perform (mFftA.data(), mFftS.data(), false);

        // ── 3. Phase vocoder: instantaneous frequency → phase accumulation ─
        for (int k = 0; k <= kFFTSize / 2; ++k)
        {
            const float mag = std::abs  (mFftS[k]);
            const float phi = std::arg  (mFftS[k]);

            // Expected phase advance for this bin at the analysis hop
            const float expectedAdvance = kTwoPi * (float) k * (float) kHopSize
                                          / (float) kFFTSize;

            // Deviation from expected (inter-frame phase difference)
            float dphi = phi - c.lastPhase[k] - expectedAdvance;

            // Wrap to (−π, π]
            dphi -= kTwoPi * std::round (dphi / kTwoPi);

            // True instantaneous frequency (radians per sample)
            const float trueFreq = kTwoPi * (float) k / (float) kFFTSize
                                   + dphi / (float) kHopSize;

            // Advance accumulator by the synthesis hop (pitch is preserved)
            c.phaseAccum[k] += trueFreq * (float) mSynthHop;

            // Save current phase for next frame
            c.lastPhase[k] = phi;

            // Reconstruct bin with accumulated phase, original magnitude
            mFftA[k] = std::polar (mag, c.phaseAccum[k]);
        }

        // ── 4. Hermitian symmetry (required for real-valued IFFT output) ──
        mFftA[0]            = { mFftA[0].real(), 0.0f };                // DC - real only
        mFftA[kFFTSize / 2] = { mFftA[kFFTSize / 2].real(), 0.0f };    // Nyquist
        for (int k = 1; k < kFFTSize / 2; ++k)
            mFftA[kFFTSize - k] = std::conj (mFftA[k]);

        // ── 5. Inverse FFT ────────────────────────────────────────────────
        mFFT.perform (mFftA.data(), mFftS.data(), true);

        // ── 6. Synthesis window + normalize + overlap-add ─────────────────
        const int writeBase = (int) (c.outWriteAbs % outBufSize);

        for (int i = 0; i < kFFTSize; ++i)
        {
            const float sample = mFftS[i].real() * mWindow[i] * mWindowScale;
            const int   physIdx = (writeBase + i) % outBufSize;
            c.outBuf[physIdx] += sample;
        }

        // Advance OLA write head by one synthesis hop
        c.outWriteAbs += mSynthHop;
    }

    // ── 7. Consume one analysis hop from the input ring ───────────────────
    for (auto& c : mCh)
    {
        c.inRead  = (c.inRead + kHopSize) % inRingSize;
        c.inAvail -= kHopSize;
    }
}
