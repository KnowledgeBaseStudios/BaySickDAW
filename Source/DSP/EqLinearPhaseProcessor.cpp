#include "EqLinearPhaseProcessor.h"

// ── Per-mode FFT sizes (T2d locked HQE at 512) ───────────────────────────────
int EqLinearPhaseProcessor::fftSizeForMode (int phaseMode) noexcept
{
    // 0 Standard, 2 HQ+ -> no linear processing
    // 1 Linear         -> 2048
    // 3 HQ Linear      -> 4096
    // 4 HQ Extended    -> 512  ("economical" - lower latency, less precision)
    switch (phaseMode)
    {
        case 1: return 2048;
        case 3: return 4096;
        case 4: return 512;
        default: return 0;
    }
}

// ── prepare ──────────────────────────────────────────────────────────────────
void EqLinearPhaseProcessor::prepare (int fftSize, int numChannels, double sampleRate)
{
    if (fftSize <= 0 || numChannels <= 0 || sampleRate <= 0.0)
    {
        // Disabled state: free FFT, drop scratch. process() short-circuits when
        // mFft is null (isReady()==false).
        mFft.reset();
        mFftSize = 0;
        mHop     = 0;
        mNumCh   = 0;
        mSampleRate = 0.0;
        mChannels.clear();
        mWindow.clear();
        mIrMagBins.clear();
        mFftScratch.clear();
        return;
    }

    // Round to nearest power of two (juce::dsp::FFT requires power-of-two order).
    int order = 0;
    int n = 1;
    while (n < fftSize) { n <<= 1; ++order; }
    fftSize = n;

    mFftSize    = fftSize;
    mHop        = fftSize / 2;
    mNumCh      = numChannels;
    mSampleRate = sampleRate;

    mFft = std::make_unique<juce::dsp::FFT> (order);

    // Hann window. The classic 50%-OLA reconstruction with double-windowing
    // (window applied at analysis AND synthesis) sums to a constant factor:
    //   sum_{i=0..N-1} w[i]^2 / hop = 1.5 / 1 (for Hann)  -> divide by 1.5
    // We bake this into mNormScale combined with the FFT's 1/N convention so
    // forward+inverse round-trips at unity gain.
    mWindow.assign (mFftSize, 0.0f);
    double sumW2 = 0.0;
    const double pi = juce::MathConstants<double>::pi;
    for (int i = 0; i < mFftSize; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * pi * (double) i / (double) mFftSize);
        mWindow[(size_t) i] = (float) w;
        sumW2 += w * w;
    }

    // OLA reconstruction scale: with Hann at 50% OLA + double-windowing, the
    // overlap-summed window-squared per output sample equals sumW2 / mHop.
    // Divide by that to recover unity-gain pass-through (when IR mags are 1).
    const double olaSum = sumW2 / (double) mHop;
    mNormScale = (float) (1.0 / olaSum);

    // IR magnitude bins (real, length N/2+1). Zero-magnitude default = silent
    // until rebuildIR() is called; EQ8DSP::setPhaseMode triggers a rebuild
    // immediately after every mode change so this is only seen on cold-start.
    mIrMagBins.assign ((size_t)(mFftSize / 2 + 1), 0.0f);

    mFftScratch.assign ((size_t) mFftSize, juce::dsp::Complex<float> (0.0f, 0.0f));

    mChannels.clear();
    mChannels.resize ((size_t) mNumCh);
    for (auto& c : mChannels)
    {
        c.inRing .assign ((size_t) mFftSize, 0.0f);
        c.outRing.assign ((size_t) mFftSize, 0.0f);
        c.inWritePos        = 0;
        c.samplesSinceFrame = 0;
        c.outReadPos        = 0;
        // Output write head leads the read head by mHop so the first frame
        // we generate writes into "future" output samples that the read head
        // hasn't reached yet. This is what creates the mFftSize/2 latency.
        c.outWritePos       = mHop;
    }
}

// ── reset ────────────────────────────────────────────────────────────────────
void EqLinearPhaseProcessor::reset()
{
    for (auto& c : mChannels)
    {
        std::fill (c.inRing .begin(), c.inRing .end(), 0.0f);
        std::fill (c.outRing.begin(), c.outRing.end(), 0.0f);
        c.inWritePos        = 0;
        c.samplesSinceFrame = 0;
        c.outReadPos        = 0;
        c.outWritePos       = mHop;
    }
}

// ── rebuildIR ────────────────────────────────────────────────────────────────
void EqLinearPhaseProcessor::rebuildIR (const std::function<float(float)>& magnitudeFn)
{
    if (mFftSize <= 0 || ! magnitudeFn) return;
    const float sr   = (float) mSampleRate;
    const int   nyq  = mFftSize / 2;
    const float binHz = sr / (float) mFftSize;
    for (int k = 0; k <= nyq; ++k)
    {
        const float f = (float) k * binHz;
        // Clamp magnitude to a sane range to defend against pathological
        // band setups (e.g. boosted Peaking with extreme Q at Nyquist).
        const float mag = juce::jlimit (0.0f, 256.0f, magnitudeFn (f));
        mIrMagBins[(size_t) k] = mag;
    }
}

// ── process ──────────────────────────────────────────────────────────────────
void EqLinearPhaseProcessor::process (juce::dsp::AudioBlock<float>& block)
{
    if (! isReady()) return;

    const int numSamples = (int) block.getNumSamples();
    const int numCh      = juce::jmin ((int) block.getNumChannels(), (int) mChannels.size());

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = block.getChannelPointer ((size_t) ch);
        auto&  cs   = mChannels[(size_t) ch];

        for (int n = 0; n < numSamples; ++n)
        {
            // Append input sample
            cs.inRing[(size_t) cs.inWritePos] = data[n];
            cs.inWritePos = (cs.inWritePos + 1) % mFftSize;

            // Replace input sample with delayed-and-EQd output from OLA ring
            data[n] = cs.outRing[(size_t) cs.outReadPos];
            // Clear after read so the next OLA accumulation lands on zero
            cs.outRing[(size_t) cs.outReadPos] = 0.0f;
            cs.outReadPos = (cs.outReadPos + 1) % mFftSize;

            // Fire a frame every mHop samples
            ++cs.samplesSinceFrame;
            if (cs.samplesSinceFrame >= mHop)
            {
                cs.samplesSinceFrame = 0;
                processFrame (ch);
            }
        }
    }
}

// ── processFrame ─────────────────────────────────────────────────────────────
// Called when mHop new input samples have accumulated. Snapshots the most
// recent N samples from the input ring (which is exactly the ring's full
// content given size==N), windows, FFTs, multiplies by IR magnitudes, IFFTs,
// re-windows, and OLAs into the output ring at the current write head.
void EqLinearPhaseProcessor::processFrame (int channelIndex)
{
    auto& cs = mChannels[(size_t) channelIndex];

    // The most recent N samples are precisely the entire ring, in order
    // [inWritePos, inWritePos+1, ..., inWritePos+N-1] (mod N).
    for (int i = 0; i < mFftSize; ++i)
    {
        const int srcIdx = (cs.inWritePos + i) % mFftSize;
        const float winSample = cs.inRing[(size_t) srcIdx] * mWindow[(size_t) i];
        mFftScratch[(size_t) i] = juce::dsp::Complex<float> (winSample, 0.0f);
    }

    // Forward FFT in place
    mFft->perform (mFftScratch.data(), mFftScratch.data(), false);

    // Multiply each bin by IR magnitude (real). Bin k mirrors to bin N-k for
    // a real-valued signal, but juce::dsp::FFT gives the full N-bin output;
    // we apply the magnitude symmetrically so the inverse stays real.
    const int nyq = mFftSize / 2;
    for (int k = 0; k <= nyq; ++k)
    {
        const float mag = mIrMagBins[(size_t) k];
        mFftScratch[(size_t) k] = mFftScratch[(size_t) k] * mag;
        if (k > 0 && k < nyq)
        {
            const int mirror = mFftSize - k;
            mFftScratch[(size_t) mirror] = mFftScratch[(size_t) mirror] * mag;
        }
    }

    // Inverse FFT in place
    mFft->perform (mFftScratch.data(), mFftScratch.data(), true);

    // Apply synthesis window + OLA-add into the output ring at the write head.
    // Write head wraps modulo N. After the frame we advance it by mHop so the
    // next frame's window overlaps with this one's second half.
    int writeIdx = cs.outWritePos;
    for (int i = 0; i < mFftSize; ++i)
    {
        const float synth = mFftScratch[(size_t) i].real() * mWindow[(size_t) i] * mNormScale;
        cs.outRing[(size_t) writeIdx] += synth;
        writeIdx = (writeIdx + 1) % mFftSize;
    }
    cs.outWritePos = (cs.outWritePos + mHop) % mFftSize;
}
