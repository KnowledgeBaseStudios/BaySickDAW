#include "SibilanceSpectralProcessor.h"
#include <cmath>

namespace
{
    constexpr float kTwoPi = 6.28318530717958647692f;

    // Per-bin floor EMA time constants: rise slowly (an ess must not lift its
    // own floor), fall faster (recover after the phrase).  Selectivity ramp:
    // 3 dB above the floor = untouched, 12 dB = full reduction share.
    constexpr float kFloorUpTauMs = 800.0f;
    constexpr float kFloorDnTauMs = 200.0f;
    constexpr float kSelLoDb      = 3.0f;
    constexpr float kSelHiDb      = 12.0f;
    constexpr float kFloorInitDb  = -80.0f;

    inline float frameCoef (float tauMs, float hopMs) noexcept
    {
        if (tauMs <= 0.01f) return 0.0f;
        return std::exp (-hopMs / tauMs);
    }
}

SibilanceSpectralProcessor::SibilanceSpectralProcessor()
{
    mCfg[0] = { kMaxOrder,     kMaxFFT,     kMaxFFT / 4 };        // HQ 2048/512
    mCfg[1] = { kMaxOrder - 1, kMaxFFT / 2, kMaxFFT / 8 };        // LL 1024/256

    mWindowHQ.assign ((size_t) mCfg[0].fftSize, 0.0f);
    mWindowLL.assign ((size_t) mCfg[1].fftSize, 0.0f);
    for (int i = 0; i < mCfg[0].fftSize; ++i)
        mWindowHQ[(size_t) i] = 0.5f * (1.0f - std::cos (kTwoPi * i / (mCfg[0].fftSize - 1)));
    for (int i = 0; i < mCfg[1].fftSize; ++i)
        mWindowLL[(size_t) i] = 0.5f * (1.0f - std::cos (kTwoPi * i / (mCfg[1].fftSize - 1)));

    // Hann + 75% overlap, window applied twice: only the OLA Hann^2 sum (3/2)
    // needs canceling -> scale = 2/3.  juce::dsp::FFT ALREADY normalizes the
    // inverse by 1/N internally (FFTFallback + FFTW impls both multiply the
    // inverse output by 1/size), so do NOT divide by N here -- PhaseVocoder's
    // (2/3)/N carries that wrong assumption (output ~1/N; flagged separately).
    mWindowScaleHQ = 2.0f / 3.0f;
    mWindowScaleLL = 2.0f / 3.0f;

    for (auto& c : mCh)
    {
        c.inRing .assign ((size_t) kMaxFFT * 4, 0.0f);
        c.outBuf .assign ((size_t) kMaxFFT * 8, 0.0f);
        c.floorDb.assign ((size_t) kMaxBins, kFloorInitDb);
        c.gain   .assign ((size_t) kMaxBins, 1.0f);
    }
    mSpec[0].assign ((size_t) kMaxFFT, Cx());
    mSpec[1].assign ((size_t) kMaxFFT, Cx());
    mFftA.assign ((size_t) kMaxFFT, Cx());
    mFftS.assign ((size_t) kMaxFFT, Cx());
    mGainScratch.assign ((size_t) kMaxBins, 1.0f);
}

void SibilanceSpectralProcessor::prepare (double sampleRate, int maxBlockSize)
{
    // Rings are fixed-size (kMaxFFT multiples); blocks beyond ~3x the FFT
    // would wrap-corrupt.  DeEsserDSP's own guard bails first in practice.
    jassert (maxBlockSize <= kMaxFFT * 2);
    juce::ignoreUnused (maxBlockSize);
    mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
    resetState();
}

void SibilanceSpectralProcessor::reset()
{
    resetState();
}

void SibilanceSpectralProcessor::resetState()
{
    for (auto& c : mCh)
    {
        std::fill (c.inRing .begin(), c.inRing .end(), 0.0f);
        std::fill (c.outBuf .begin(), c.outBuf .end(), 0.0f);
        std::fill (c.floorDb.begin(), c.floorDb.end(), kFloorInitDb);
        std::fill (c.gain   .begin(), c.gain   .end(), 1.0f);
        c.inRead = c.inWrite = c.inAvail = 0;
        c.outReadAbs = c.outWriteAbs = 0;
        c.primeRemaining = mCfg[mActiveQuality].fftSize;   // = getLatencySamples of the active config
    }
    mLastGrDb.store (0.0f, std::memory_order_relaxed);
}

void SibilanceSpectralProcessor::setQuality (int q) noexcept
{
    mPendingQuality.store (juce::jlimit (0, 1, q), std::memory_order_relaxed);
}

int SibilanceSpectralProcessor::getLatencySamples() const noexcept
{
    // One full FFT frame.  (fftSize - hop) is only exact when host blocks are
    // hop-multiples; priming a full frame makes the reported latency exact for
    // EVERY block size (out(t) = y(t - fftSize) is always final).
    return mCfg[mPendingQuality.load (std::memory_order_relaxed)].fftSize;
}

void SibilanceSpectralProcessor::applyPendingQuality()
{
    const int pending = mPendingQuality.load (std::memory_order_relaxed);
    if (pending != mActiveQuality)
    {
        mActiveQuality = pending;
        resetState();
    }
}

void SibilanceSpectralProcessor::process (float* l, float* r, int numSamples,
                                           const Params& p)
{
    if (l == nullptr || numSamples <= 0) return;

    applyPendingQuality();

    const auto& cfg      = mCfg[mActiveQuality];
    const int   numCh    = (r != nullptr) ? 2 : 1;
    const int   ringSize = (int) mCh[0].inRing.size();
    const int   outSize  = (int) mCh[0].outBuf.size();

    float* chans[2] = { l, r };

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& c = mCh[ch];
        const float* src = chans[ch];
        for (int i = 0; i < numSamples; ++i)
        {
            c.inRing[(size_t) c.inWrite] = src[i];
            c.inWrite = (c.inWrite + 1) % ringSize;
        }
        c.inAvail += numSamples;
    }

    while (mCh[0].inAvail >= cfg.fftSize)
        processFrame (numCh, p);

    // Deliver exactly numSamples per call: prime exactly one FFT frame of
    // zeros, then read sequentially.  out(t) = y(t - fftSize) is always final
    // (W >= inputCount - fftSize + 1 after the first frame), so this never
    // underruns mid-stream for ANY host block size -- unlike an (N - hop)
    // gate, which shortfalls whenever blocks aren't hop-multiples.
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& c = mCh[ch];
        float* dst = chans[ch];

        const int zeros = juce::jmin (numSamples, c.primeRemaining);
        c.primeRemaining -= zeros;
        for (int i = 0; i < zeros; ++i) dst[i] = 0.0f;

        const int m = numSamples - zeros;
        for (int i = 0; i < m; ++i)
        {
            const int physIdx = (int) ((c.outReadAbs + i) % outSize);
            dst[zeros + i] = c.outBuf[(size_t) physIdx];
            c.outBuf[(size_t) physIdx] = 0.0f;   // clear after read (OLA correctness)
        }
        c.outReadAbs += m;
    }
}

void SibilanceSpectralProcessor::processFrame (int numCh, const Params& p)
{
    const auto&  cfg      = mCfg[mActiveQuality];
    const int    N        = cfg.fftSize;
    const int    H        = cfg.hop;
    const int    numBins  = N / 2 + 1;
    const int    ringSize = (int) mCh[0].inRing.size();
    const int    outSize  = (int) mCh[0].outBuf.size();
    const float* window   = (mActiveQuality == 0) ? mWindowHQ.data() : mWindowLL.data();
    const float  wScale   = (mActiveQuality == 0) ? mWindowScaleHQ : mWindowScaleLL;
    juce::dsp::FFT& fft   = (mActiveQuality == 0) ? mFftHQ : mFftLL;

    const double sr    = mSampleRate;
    const float  hopMs = (float) (1000.0 * H / sr);

    // Sibilance band bins: [detector cutoff .. 16 kHz].  Cutoff >= 4 kHz by
    // the caller's clamp, so the reference's fixed-4 kHz Split floor holds.
    const int loBin = juce::jlimit (1, numBins - 1,
                        (int) std::floor (p.detectLoHz * N / sr));
    const int hiBin = juce::jlimit (loBin, numBins - 1,
                        (int) std::ceil (juce::jmin ((double) kBandHiHz, sr * 0.5) * N / sr));

    // Bin magnitude -> dBFS-ish calibration: an in-band full-scale sine peaks
    // its bin at ~A*N/4 under Hann (coherent gain 0.5, JUCE FFT gain N/2).
    const float magNorm = 4.0f / (float) N;

    const float atkCoef     = frameCoef (p.attackMs,  hopMs);
    const float relCoef     = frameCoef (p.releaseMs, hopMs);
    const float floorUpCoef = frameCoef (kFloorUpTauMs, hopMs);
    const float floorDnCoef = frameCoef (kFloorDnTauMs, hopMs);
    const float blend       = juce::jlimit (0.0f, 1.0f, p.blend01);

    // ── Pass 1: analyze both channels, band level per channel ──────────────
    float bandDb[2] = { -120.0f, -120.0f };
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& c = mCh[ch];
        for (int i = 0; i < N; ++i)
        {
            const int srcIdx = (c.inRead + i) % ringSize;
            mFftA[(size_t) i] = { c.inRing[(size_t) srcIdx] * window[i], 0.0f };
        }
        fft.perform (mFftA.data(), mSpec[ch].data(), false);

        // Peak in-band bin, not mean RMS: the mean over ~400 bins reads
        // 15-25 dB below the time-domain path's peak envelope of the same
        // material, which would desensitize the SHARED Thresh knob when
        // switching engines.  Peak-bin tracks the (spectrally concentrated)
        // ess and keeps knob parity.
        float peak = 0.0f;
        for (int k = loBin; k <= hiBin; ++k)
            peak = juce::jmax (peak, std::abs (mSpec[ch][(size_t) k]) * magNorm);
        bandDb[ch] = 20.0f * std::log10 (juce::jmax (peak, 1.0e-9f));
    }

    // Stereo-linked event gate (louder channel drives both) -- same saturating
    // overshoot->reduction curve as the time-domain detector.
    const float envDb     = (numCh > 1) ? juce::jmax (bandDb[0], bandDb[1]) : bandDb[0];
    const float overshoot = envDb - p.thresholdDb;
    const float eventT    = (overshoot > 0.0f) ? (1.0f - std::exp (-overshoot / 6.0f)) : 0.0f;
    const float eventDb   = p.rangeDb * eventT;   // <= 0

    float frameMinGainDb = 0.0f;

    // ── Pass 2: per-channel mask + apply + resynthesize ────────────────────
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& c = mCh[ch];

        for (int k = 0; k < numBins; ++k)
        {
            const bool inBand = (k >= loBin && k <= hiBin);

            float sel = 0.0f;
            if (inBand)
            {
                const float magDb = 20.0f * std::log10 (
                    juce::jmax (std::abs (mSpec[ch][(size_t) k]) * magNorm, 1.0e-9f));
                float& fl = c.floorDb[(size_t) k];
                const float fc = (magDb > fl) ? floorUpCoef : floorDnCoef;
                fl = fc * fl + (1.0f - fc) * magDb;
                sel = juce::jlimit (0.0f, 1.0f, (magDb - fl - kSelLoDb) / (kSelHiDb - kSelLoDb));
            }

            // Wide = uniform event duck; Split = selectivity-shaped, in-band only.
            const float tgtDb = eventDb * ((1.0f - blend) + blend * (inBand ? sel : 0.0f));
            const float tgt   = juce::Decibels::decibelsToGain (tgtDb);

            float& g = c.gain[(size_t) k];
            const float gc = (tgt < g) ? atkCoef : relCoef;
            g = gc * g + (1.0f - gc) * tgt;
        }

        // 3-bin frequency smoothing of the gain mask (prevents isolated bins
        // gating on/off frame-to-frame -> "birdies").
        mGainScratch[0] = c.gain[0];
        for (int k = 1; k < numBins - 1; ++k)
            mGainScratch[(size_t) k] = (c.gain[(size_t) (k - 1)]
                                      + c.gain[(size_t) k]
                                      + c.gain[(size_t) (k + 1)]) * (1.0f / 3.0f);
        mGainScratch[(size_t) (numBins - 1)] = c.gain[(size_t) (numBins - 1)];

        for (int k = loBin; k <= hiBin; ++k)
        {
            const float gDb = 20.0f * std::log10 (juce::jmax (mGainScratch[(size_t) k], 1.0e-6f));
            if (gDb < frameMinGainDb) frameMinGainDb = gDb;
        }

        // Apply (magnitude-only: scaling the complex bin preserves phase).
        // Monitor reconstructs the removed component (1-g) through the same
        // pipeline -- phase/latency identical to the wet path by construction.
        for (int k = 0; k < numBins; ++k)
        {
            const float g = p.outputRemoved ? (1.0f - mGainScratch[(size_t) k])
                                            : mGainScratch[(size_t) k];
            mFftA[(size_t) k] = mSpec[ch][(size_t) k] * g;
        }
        mFftA[0]     = { mFftA[0].real(), 0.0f };                    // DC real-only
        mFftA[(size_t) (N / 2)] = { mFftA[(size_t) (N / 2)].real(), 0.0f };  // Nyquist
        for (int k = 1; k < N / 2; ++k)
            mFftA[(size_t) (N - k)] = std::conj (mFftA[(size_t) k]);

        fft.perform (mFftA.data(), mFftS.data(), true);

        const int writeBase = (int) (c.outWriteAbs % outSize);
        for (int i = 0; i < N; ++i)
        {
            const float sample  = mFftS[(size_t) i].real() * window[i] * wScale;
            const int   physIdx = (writeBase + i) % outSize;
            c.outBuf[(size_t) physIdx] += sample;
        }
        c.outWriteAbs += H;
    }

    mLastGrDb.store (frameMinGainDb, std::memory_order_relaxed);

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& c = mCh[ch];
        c.inRead  = (c.inRead + H) % ringSize;
        c.inAvail -= H;
    }
}
