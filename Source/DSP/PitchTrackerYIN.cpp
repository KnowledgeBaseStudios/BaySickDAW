#include "PitchTrackerYIN.h"
#include <cmath>
#include <algorithm>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
// PitchTrackerYIN - Phase H-4 (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    // YIN difference function - d[tau] = sum_{i=0}^{W/2-1} (x[i] - x[i+tau])^2
    //
    // Standard YIN runs over the first half of the window so max tau == W/2;
    // this gives a cleaner CMNDF normalization than running edge-to-edge.
    void computeDifference (const float* w, int windowSize,
                             std::vector<float>& outDiff)
    {
        const int halfW = windowSize / 2;
        outDiff.assign ((size_t) halfW, 0.0f);
        for (int tau = 1; tau < halfW; ++tau)
        {
            float sum = 0.0f;
            for (int i = 0; i < halfW; ++i)
            {
                const float delta = w[i] - w[i + tau];
                sum += delta * delta;
            }
            outDiff[(size_t) tau] = sum;
        }
        // d[0] is undefined in YIN -- set to a value so CMNDF anchors at 1 there.
        outDiff[0] = 0.0f;
    }

    // Cumulative Mean Normalized Difference Function.
    //   d'[0]   = 1
    //   d'[tau] = d[tau] / [(1/tau) * sum_{j=1}^{tau} d[j]]
    //
    // The CMNDF normalization tames the absolute magnitude of d[] across
    // signal levels so the threshold (0.15) is comparable across loud + quiet
    // input.  Computed in-place over the same buffer.
    void computeCMNDF (std::vector<float>& diff)
    {
        const int n = (int) diff.size();
        if (n == 0) return;
        diff[0] = 1.0f;
        float cum = 0.0f;
        for (int tau = 1; tau < n; ++tau)
        {
            cum += diff[(size_t) tau];
            diff[(size_t) tau] = (cum > 0.0f)
                ? diff[(size_t) tau] * (float) tau / cum
                : 1.0f;
        }
    }

    // Find the first absolute minimum in CMNDF below threshold.  Walks tau
    // forward until d'[tau] < threshold, then locks onto the bottom of the
    // local dip (descends until d' starts increasing again).  Returns -1 if
    // no minimum below threshold exists in [tauMin, tauMax].
    int findFirstMinimum (const std::vector<float>& cmndf,
                          int tauMin, int tauMax, float threshold)
    {
        const int n = (int) cmndf.size();
        const int tMin = juce::jlimit (1, n - 1, tauMin);
        const int tMax = juce::jlimit (1, n - 1, tauMax);
        for (int tau = tMin; tau < tMax; ++tau)
        {
            if (cmndf[(size_t) tau] < threshold)
            {
                // Descend until d' starts increasing.
                while (tau + 1 < tMax && cmndf[(size_t) (tau + 1)] < cmndf[(size_t) tau])
                    ++tau;
                return tau;
            }
        }
        return -1;
    }

    // Parabolic interpolation around the minimum bin for sub-sample period
    // precision.  Given cmndf[tau-1], cmndf[tau], cmndf[tau+1], fit a parabola
    // and return the refined period offset.
    float parabolicInterpolate (const std::vector<float>& cmndf, int tau)
    {
        const int n = (int) cmndf.size();
        if (tau <= 0 || tau >= n - 1) return (float) tau;
        const float a = cmndf[(size_t) (tau - 1)];
        const float b = cmndf[(size_t) tau];
        const float c = cmndf[(size_t) (tau + 1)];
        const float denom = a - 2.0f * b + c;
        if (std::abs (denom) < 1e-9f) return (float) tau;
        const float delta = (a - c) / (2.0f * denom);
        return (float) tau + delta;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// WorkerThread
// ─────────────────────────────────────────────────────────────────────────────

class PitchTrackerYIN::WorkerThread : public juce::Thread
{
public:
    WorkerThread (PitchTrackerYIN& o)
        : juce::Thread ("PitchTrackerYIN"), owner (o) {}

    void run() override
    {
        // Per-instance window/hop, snapshotted once (constant for the worker's
        // lifetime; set before prepare()).  Rolling window slides forward by H
        // each iteration; first analysis fires once W samples have been pulled.
        const int W = owner.mWindowSize;
        const int H = owner.mHopSize;
        std::vector<float> rolling ((size_t) W, 0.0f);
        std::vector<float> diff;
        diff.reserve ((size_t) (W / 2));

        int totalIn = 0;     // cumulative samples pulled into rolling so far
        int sinceLast = 0;   // samples pulled since last analyze()

        while (! threadShouldExit())
        {
            // Block until at least one hop's worth is ready, with a timeout
            // so we can check threadShouldExit periodically.
            const int ready = owner.mFifo.getNumReady();
            if (ready < H)
            {
                wait (5);   // ms
                continue;
            }

            // Pull as many samples as are available, capped at one hop per
            // iteration.  Larger pulls per iteration means stale rolling
            // window (analysis lags); smaller means tight loop = more CPU.
            const int pullCount = std::min (ready, H);

            int s1 = 0, b1 = 0, s2 = 0, b2 = 0;
            owner.mFifo.prepareToRead (pullCount, s1, b1, s2, b2);

            // Slide rolling window left by pullCount, then append new samples.
            std::memmove (rolling.data(),
                          rolling.data() + pullCount,
                          (size_t) (W - pullCount) * sizeof (float));
            int dest = W - pullCount;
            for (int i = 0; i < b1; ++i)
                rolling[(size_t) (dest + i)] = owner.mFifoBuf[(size_t) (s1 + i)];
            dest += b1;
            for (int i = 0; i < b2; ++i)
                rolling[(size_t) (dest + i)] = owner.mFifoBuf[(size_t) (s2 + i)];

            owner.mFifo.finishedRead (pullCount);

            totalIn   += pullCount;
            sinceLast += pullCount;

            // Run YIN once the rolling window is fully populated AND we've
            // advanced by at least one hop since last analysis.
            if (totalIn >= W && sinceLast >= H)
            {
                analyze (rolling.data(), W);
                sinceLast = 0;
            }
        }
    }

private:
    void analyze (const float* window, int W) noexcept
    {
        // 1) Difference function over half the window.
        std::vector<float> cmndf;
        computeDifference (window, W, cmndf);

        // 2) Cumulative Mean Normalized Difference Function (in-place).
        computeCMNDF (cmndf);

        // 3) tau range from min/max frequency at current sample rate.
        const double sr = (owner.mSampleRate > 0.0) ? owner.mSampleRate : 44100.0;
        const int tauMin = juce::jmax (2, (int) std::floor (sr / (double) kMaxFreqHz));
        const int tauMax = juce::jmin ((int) cmndf.size() - 2,
                                       (int) std::ceil  (sr / (double) kMinFreqHz));

        // 4) Find first sub-threshold minimum.
        const int tau = findFirstMinimum (cmndf, tauMin, tauMax, kYinThreshold);
        if (tau < 0)
        {
            // No pitch detected - silence, noise, or fully unpitched material.
            owner.mFreqHz    .store (0.0f, std::memory_order_release);
            owner.mConfidence.store (0.0f, std::memory_order_release);
            return;
        }

        // 5) Parabolic interpolation for sub-sample period precision.
        const float refinedTau = parabolicInterpolate (cmndf, tau);
        const float freqHz     = (refinedTau > 0.0f)
                               ? (float) (sr / (double) refinedTau)
                               : 0.0f;
        const float confidence = juce::jlimit (0.0f, 1.0f,
                                                1.0f - cmndf[(size_t) tau]);

        owner.mFreqHz    .store (freqHz,     std::memory_order_release);
        owner.mConfidence.store (confidence, std::memory_order_release);
    }

    PitchTrackerYIN& owner;
};

// ─────────────────────────────────────────────────────────────────────────────
// PitchTrackerYIN
// ─────────────────────────────────────────────────────────────────────────────

PitchTrackerYIN::PitchTrackerYIN()
{
    mFifoBuf.resize ((size_t) kRingSize, 0.0f);
}

PitchTrackerYIN::~PitchTrackerYIN()
{
    releaseResources();
}

void PitchTrackerYIN::setAnalysisWindow (int windowSize, int hopSize) noexcept
{
    // Call before prepare() -- the worker snapshots these at run() start.  The
    // ring (kMaxWindowSize*4) already fits any window up to kMaxWindowSize.
    mWindowSize = juce::jlimit (256, kMaxWindowSize, windowSize);
    mHopSize    = juce::jlimit (64,  mWindowSize,    hopSize);
}

void PitchTrackerYIN::prepare (double sampleRate)
{
    if (mWorker != nullptr) releaseResources();

    mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
    reset();

    mWorker = std::make_unique<WorkerThread> (*this);
    mWorker->startThread (juce::Thread::Priority::low);
}

void PitchTrackerYIN::releaseResources()
{
    if (mWorker != nullptr)
    {
        mWorker->signalThreadShouldExit();
        mWorker->stopThread (200);
        mWorker.reset();
    }
}

void PitchTrackerYIN::reset() noexcept
{
    mFifo.reset();
    std::fill (mFifoBuf.begin(), mFifoBuf.end(), 0.0f);
    mFreqHz    .store (0.0f, std::memory_order_release);
    mConfidence.store (0.0f, std::memory_order_release);
}

void PitchTrackerYIN::pushAudio (const float* mono, int numSamples) noexcept
{
    if (mono == nullptr || numSamples <= 0) return;

    int s1 = 0, b1 = 0, s2 = 0, b2 = 0;
    const int free = mFifo.getFreeSpace();
    const int n    = juce::jmin (numSamples, free);
    if (n <= 0) return;   // ring full - drop oldest reading by skipping push

    mFifo.prepareToWrite (n, s1, b1, s2, b2);
    if (b1 > 0) std::memcpy (mFifoBuf.data() + s1, mono,           (size_t) b1 * sizeof (float));
    if (b2 > 0) std::memcpy (mFifoBuf.data() + s2, mono + b1,      (size_t) b2 * sizeof (float));
    mFifo.finishedWrite (n);
}

void PitchTrackerYIN::pushAudio (const float* left, const float* right, int numSamples) noexcept
{
    if (left == nullptr || numSamples <= 0) return;
    if (right == nullptr) { pushAudio (left, numSamples); return; }

    // Mix L+R to mono in chunks via a small stack buffer to avoid allocating
    // on the audio thread.
    constexpr int kChunk = 256;
    float tmp[kChunk];
    int remaining = numSamples;
    int offset = 0;
    while (remaining > 0)
    {
        const int n = juce::jmin (remaining, kChunk);
        for (int i = 0; i < n; ++i)
            tmp[i] = 0.5f * (left[offset + i] + right[offset + i]);
        pushAudio (tmp, n);
        remaining -= n;
        offset    += n;
    }
}
