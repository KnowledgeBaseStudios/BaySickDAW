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
        c.outBuf    .assign (kOutBufFloor, 0.0f);
    }
}

// ── prepare ───────────────────────────────────────────────────────────────────

void PhaseVocoder::prepare (int maxOutputSamplesPerBlock)
{
    // Capacity law for the OLA queue.  processFrame's add spans
    // [outWriteAbs, outWriteAbs + kFFTSize) and the consumer only drains
    // between pushes, so the deepest the unread region ever gets is
    //   one block's production            = maxOutputSamplesPerBlock
    // + the settling residue it may not take yet   (< kFFTSize)
    // + one whole frame produced past that boundary (< kFFTSize)
    // + the tail of the frame being written        (= kFFTSize)
    // Anything shallower wraps the OLA add onto samples the consumer has not
    // read - the export-only garble, since the offline loop runs a different
    // block size from the device.
    const int floorSz = kOutBufFloor;
    const int need    = juce::jmax (0, maxOutputSamplesPerBlock) + kFFTSize * 3;
    const auto sz     = (size_t) juce::jmax (need, floorSz);

    for (auto& c : mCh)
        c.outBuf.assign (sz, 0.0f);

    // The absolute counters index modulo the size that just changed, and the
    // in-flight OLA tail belongs to the old geometry.
    reset();
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
    const int srcCh = juce::jmin (mNumCh, in.getNumChannels());
    if (mCh.empty() || srcCh <= 0 || numSamples <= 0) return;

    const int inRingSize = (int) mCh[0].inRing.size();

    // Frames are drained AS the block is written, not after it.  That caps
    // inAvail at one analysis window however many samples the caller hands
    // over, which is what makes the ring's capacity independent of the host
    // block size, the sample rate and the stretch ratio.  The old
    // write-everything-then-drain form needed
    //   block x (fileSR / deviceSR) / stretchRatio
    // samples of free room and silently overwrote unread input past that -
    // a 4096 block past a combined ratio of ~1.5 smeared and repeated
    // fragments with nothing logged.
    int done = 0;
    while (done < numSamples)
    {
        const int chunk = juce::jmin (numSamples - done,
                                      juce::jmax (1, kFFTSize - mCh[0].inAvail));

        for (int ch = 0; ch < (int) mCh.size(); ++ch)
        {
            auto& c = mCh[ch];
            const float* src = ch < srcCh
                ? in.getReadPointer (ch) + startSample + done : nullptr;

            int w = c.inWrite;
            for (int i = 0; i < chunk; ++i)
            {
                c.inRing[w] = (src != nullptr) ? src[i] : 0.0f;
                if (++w == inRingSize) w = 0;
            }
            c.inWrite  = w;
            // Every channel, including any the caller did not supply:
            // processFrame consumes the hop from all of them, so a partial
            // advance here drives the unsupplied ones' inAvail negative.
            c.inAvail += chunk;
        }

        while (mCh[0].inAvail >= kFFTSize)
            processFrame();

        done += chunk;
    }
}

// ── pull ──────────────────────────────────────────────────────────────────────

int PhaseVocoder::pull (juce::AudioBuffer<float>& out,
                         int startSample, int numSamples)
{
    if (mCh.empty()) return 0;

    const int avail  = getOutputAvailable();
    // Same destination clamp peekOutput carries: avail can exceed the caller's
    // buffer, and an unclamped copy writes past the heap allocation.
    const int room   = juce::jmax (0, out.getNumSamples() - startSample);
    const int toPull = juce::jmin (numSamples, juce::jmin (avail, room));
    if (toPull <= 0) return 0;

    const int outBufSize = (int) mCh[0].outBuf.size();
    const int dstCh      = juce::jmin (mNumCh, out.getNumChannels());

    for (int ch = 0; ch < (int) mCh.size(); ++ch)
    {
        auto& c = mCh[ch];
        float* dst = ch < dstCh ? out.getWritePointer (ch) + startSample : nullptr;

        for (int i = 0; i < toPull; ++i)
        {
            const int physIdx = (int) ((c.outReadAbs + i) % outBufSize);
            if (dst != nullptr) dst[i] = c.outBuf[physIdx];
            c.outBuf[physIdx] = 0.0f;  // clear after read - essential for OLA correctness
        }

        // Every channel: the read head is shared state (getOutputAvailable
        // reads channel 0's), so advancing only the destination's channels
        // drifts the rest into stale-slot territory.
        c.outReadAbs += toPull;
    }

    return toPull;
}

int PhaseVocoder::peekOutput (juce::AudioBuffer<float>& out,
                              int startSample, int numSamples)
{
    if (mCh.empty()) return 0;

    const int avail  = getOutputAvailable();
    // G1 smoke round 4: clamp to the destination's capacity - avail can
    // exceed the caller's scratch buffer (the OLA ring is several FFT frames
    // deep); an unclamped copy wrote past the heap allocation.  A runaway
    // request degrades to a short read, never memory corruption.
    const int room   = juce::jmax (0, out.getNumSamples() - startSample);
    const int toPeek = juce::jmin (numSamples, juce::jmin (avail, room));
    if (toPeek <= 0) return 0;

    const int outBufSize = (int) mCh[0].outBuf.size();

    for (int ch = 0; ch < juce::jmin (mNumCh, out.getNumChannels()); ++ch)
    {
        auto& c = mCh[ch];
        float* dst = out.getWritePointer (ch) + startSample;
        for (int i = 0; i < toPeek; ++i)
            dst[i] = c.outBuf[(int) ((c.outReadAbs + i) % outBufSize)];
        // NO clear, NO read-head advance - advanceOutput() consumes.
    }
    return toPeek;
}

void PhaseVocoder::advanceOutput (int numSamples)
{
    const int n = juce::jmin (numSamples, getOutputAvailable());
    if (n <= 0) return;

    const int outBufSize = (int) mCh[0].outBuf.size();

    for (auto& c : mCh)
    {
        // Zero the consumed OLA slots (pull()'s clear-after-read contract -
        // future overlap-add must land on silence, not stale data).
        for (int i = 0; i < n; ++i)
            c.outBuf[(int) ((c.outReadAbs + i) % outBufSize)] = 0.0f;
        c.outReadAbs += n;
    }
}

// ── getOutputAvailable ────────────────────────────────────────────────────────

int PhaseVocoder::getOutputAvailable() const
{
    if (mCh.empty()) return 0;
    const auto& c = mCh[0];
    // Output is "settled" (all overlapping frames have contributed) once the OLA
    // write head is at least (kFFTSize - mSynthHop) ahead of the read head.
    // Past mSynthHop >= kFFTSize the synthesis frames stop overlapping at all,
    // so there is nothing left to wait for: the floor at 0 stops a negative
    // margin from reporting MORE samples than were ever written.
    const int64_t settling = juce::jmax ((int64_t) 0,
                                         (int64_t) kFFTSize - (int64_t) mSynthHop);
    // A read cannot outrun the physical queue either - beyond one queue length
    // the modulo hands back slots the write head has already lapped.
    const int64_t produced = juce::jmin ((int64_t) c.outBuf.size(),
                                         c.outWriteAbs - c.outReadAbs);
    return (int) juce::jmax ((int64_t) 0, produced - settling);
}

// ── processFrame (core algorithm) ─────────────────────────────────────────────

void PhaseVocoder::processFrame()
{
    const int inRingSize = (int) mCh[0].inRing.size();
    const int outBufSize = (int) mCh[0].outBuf.size();

    // AUDIO THREAD: capacity guard, clamp only - growing the queue here would
    // be an allocation in the callback.  If this frame's OLA span would wrap
    // onto output the consumer has not read, skip PRODUCING it but consume the
    // input hop anyway: file consumption keeps tracking the caller's position
    // law (so playback does not slide out of sync), push()'s drain loop still
    // terminates, and the failure degrades to a dropout instead of silently
    // corrupting the queue.  With prepare() called this never fires.
    if (mCh[0].outWriteAbs - mCh[0].outReadAbs + (int64_t) kFFTSize
            > (int64_t) outBufSize)
    {
        for (auto& c : mCh)
        {
            c.inRead   = (c.inRead + kHopSize) % inRingSize;
            c.inAvail -= kHopSize;
        }
        return;
    }

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
