#include "PolyPitchTracker.h"
#include <cmath>
#include <algorithm>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
// WorkerThread - pulls from the ring, windowed FFT, polyphonic detect, publish.
// ─────────────────────────────────────────────────────────────────────────────
class PolyPitchTracker::WorkerThread : public juce::Thread
{
public:
    WorkerThread (PolyPitchTracker& o)
        : juce::Thread ("PolyPitchTracker"), owner (o) {}

    void run() override
    {
        const int W = kWindowSize;
        std::vector<float> rolling ((size_t) W, 0.0f);
        std::vector<float> hann    ((size_t) W);
        for (int i = 0; i < W; ++i)
            hann[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                        * (float) i / (float) (W - 1));

        juce::dsp::FFT     fft    (kFftOrder);
        std::vector<float> fftBuf ((size_t) (2 * W), 0.0f);
        mWork.assign ((size_t) (W / 2), 0.0f);

        int totalIn = 0, sinceLast = 0;

        while (! threadShouldExit())
        {
            const int ready = owner.mFifo.getNumReady();
            if (ready < kHopSize) { wait (5); continue; }

            const int pull = std::min (ready, kHopSize);
            int s1 = 0, b1 = 0, s2 = 0, b2 = 0;
            owner.mFifo.prepareToRead (pull, s1, b1, s2, b2);
            std::memmove (rolling.data(), rolling.data() + pull,
                          (size_t) (W - pull) * sizeof (float));
            int dest = W - pull;
            for (int i = 0; i < b1; ++i) rolling[(size_t) (dest + i)] = owner.mFifoBuf[(size_t) (s1 + i)];
            dest += b1;
            for (int i = 0; i < b2; ++i) rolling[(size_t) (dest + i)] = owner.mFifoBuf[(size_t) (s2 + i)];
            owner.mFifo.finishedRead (pull);

            totalIn   += pull;
            sinceLast += pull;
            if (totalIn >= W && sinceLast >= kHopSize)
            {
                analyze (rolling.data(), hann.data(), fft, fftBuf);
                sinceLast = 0;
            }
        }
    }

private:
    void analyze (const float* window, const float* hann,
                  juce::dsp::FFT& fft, std::vector<float>& fftBuf)
    {
        const int W = kWindowSize;
        for (int i = 0; i < W; ++i) fftBuf[(size_t) i] = window[i] * hann[i];
        std::fill (fftBuf.begin() + W, fftBuf.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftBuf.data());   // magnitudes in [0, W/2]

        const int    half  = W / 2;
        const double sr    = (owner.mSampleRate > 0.0) ? owner.mSampleRate : 44100.0;
        const float  binHz = (float) (sr / (double) W);
        const float  minHz = juce::jmax (binHz * 2.0f, owner.mMinHz.load (std::memory_order_relaxed));
        const float  maxHz = juce::jmin (owner.mMaxHz.load (std::memory_order_relaxed),
                                         (float) (sr * 0.45));
        const int    maxNotes = juce::jlimit (1, kMaxNotes,
                                              owner.mMaxNotesReq.load (std::memory_order_relaxed));

        // Working magnitude copy (claimed harmonics get zeroed out per note).
        std::copy (fftBuf.begin(), fftBuf.begin() + half, mWork.begin());

        // Noise-floor estimate -> a real note must stack harmonics well above it.
        float meanMag = 0.0f;
        for (int k = 1; k < half; ++k) meanMag += mWork[(size_t) k];
        meanMag /= (float) juce::jmax (1, half - 1);

        constexpr int H = 8;                                  // harmonics summed
        const float scoreThresh = meanMag * (float) H * 3.0f;

        auto binOf   = [binHz] (float f) { return (int) std::lround (f / binHz); };
        auto scoreAt = [&] (float f)
        {
            float s = 0.0f;
            for (int h = 1; h <= H; ++h)
            {
                const int b = binOf (f * (float) h);
                if (b < 1 || b >= half) break;
                s += mWork[(size_t) b] / (float) h;           // weight lower harmonics
            }
            return s;
        };

        NoteSet ns;
        for (int note = 0; note < maxNotes; ++note)
        {
            // Coarse search over a 50-cent f0 grid.
            float bestS = 0.0f, bestF = 0.0f;
            for (float f = minHz; f <= maxHz; f *= 1.02930224f /* +50 cents */)
            {
                const float s = scoreAt (f);
                if (s > bestS) { bestS = s; bestF = f; }
            }
            if (bestF <= 0.0f || bestS < scoreThresh) break;

            // Refine +/- 1 semitone at ~10-cent steps.
            float refF = bestF, refS = bestS;
            for (float f = bestF * 0.9438f; f <= bestF * 1.0595f; f *= 1.005777f)
            {
                const float s = scoreAt (f);
                if (s > refS) { refS = s; refF = f; }
            }
            ns.freqs[ns.count++] = refF;

            // Subtract the claimed note's harmonics from the working spectrum.
            for (int h = 1; h <= H; ++h)
            {
                const int b = binOf (refF * (float) h);
                if (b < 1 || b >= half) break;
                for (int d = -2; d <= 2; ++d)
                    if (b + d >= 1 && b + d < half) mWork[(size_t) (b + d)] = 0.0f;
            }
        }

        // Publish via seqlock (odd -> write -> even).
        owner.mSeq.fetch_add (1, std::memory_order_acq_rel);
        owner.mShared = ns;
        owner.mSeq.fetch_add (1, std::memory_order_release);
    }

    PolyPitchTracker&  owner;
    std::vector<float> mWork;
};

// ─────────────────────────────────────────────────────────────────────────────
// PolyPitchTracker
// ─────────────────────────────────────────────────────────────────────────────

PolyPitchTracker::PolyPitchTracker()
{
    mFifoBuf.resize ((size_t) kRingSize, 0.0f);
}

PolyPitchTracker::~PolyPitchTracker()
{
    releaseResources();
}

void PolyPitchTracker::prepare (double sampleRate)
{
    if (mWorker != nullptr) releaseResources();
    mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
    reset();
    mWorker = std::make_unique<WorkerThread> (*this);
    mWorker->startThread (juce::Thread::Priority::low);
}

void PolyPitchTracker::releaseResources()
{
    if (mWorker != nullptr)
    {
        mWorker->signalThreadShouldExit();
        mWorker->stopThread (200);
        mWorker.reset();
    }
}

void PolyPitchTracker::reset() noexcept
{
    mFifo.reset();
    std::fill (mFifoBuf.begin(), mFifoBuf.end(), 0.0f);
    mSeq.store (0, std::memory_order_release);
    mShared = NoteSet{};
}

void PolyPitchTracker::pushAudio (const float* mono, int numSamples) noexcept
{
    if (mono == nullptr || numSamples <= 0) return;
    int s1 = 0, b1 = 0, s2 = 0, b2 = 0;
    const int n = juce::jmin (numSamples, mFifo.getFreeSpace());
    if (n <= 0) return;
    mFifo.prepareToWrite (n, s1, b1, s2, b2);
    if (b1 > 0) std::memcpy (mFifoBuf.data() + s1, mono,      (size_t) b1 * sizeof (float));
    if (b2 > 0) std::memcpy (mFifoBuf.data() + s2, mono + b1, (size_t) b2 * sizeof (float));
    mFifo.finishedWrite (n);
}

PolyPitchTracker::NoteSet PolyPitchTracker::getNotes() const noexcept
{
    NoteSet ns;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        const uint32_t s1 = mSeq.load (std::memory_order_acquire);
        if (s1 & 1u) continue;                 // write in progress
        ns = mShared;                          // snapshot
        const uint32_t s2 = mSeq.load (std::memory_order_acquire);
        if (s1 == s2) break;                   // clean read
    }
    ns.count = juce::jlimit (0, kMaxNotes, ns.count);
    return ns;
}
