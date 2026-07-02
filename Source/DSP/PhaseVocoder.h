#pragma once
#include <JuceHeader.h>

// ── PhaseVocoder ──────────────────────────────────────────────────────────────
// True phase vocoder: pitch-preserving time stretch via FFT overlap-add.
//
// Algorithm (standard Laroche-Dolson):
//   Analysis:  Hann-windowed FFT every Ha samples.
//   Phase:     Instantaneous frequency computed from inter-frame phase delta.
//              Phase accumulator advanced by Hs (synthesis hop) preserving pitch.
//   Synthesis: IFFT → Hann window → overlap-add every Hs samples.
//
// Parameters:
//   stretchRatio = sourceBPM / projectBPM
//     < 1 → time compress  (project is faster, clip shortened)
//     > 1 → time expand    (project is slower, clip lengthened)
//     = 1 → passthrough
//
// Usage (all calls on same thread - audio thread):
//   1. setStretchRatio(sourceBPM / projectBPM)
//   2. push(fileAudio, startSample, numSamples)   - feed raw file samples
//   3. pull(output, startSample, numSamples)       - get stretched output
//   4. reset()                                      - on transport seek / loop
//
// Latency: kFFTSize - kHopSize = 1536 samples at the file sample rate.

class PhaseVocoder
{
public:
    static constexpr int kFFTOrder = 11;            // 2^11 = 2048
    static constexpr int kFFTSize  = 1 << kFFTOrder;
    static constexpr int kHopSize  = kFFTSize / 4;  // 512 - 75 % overlap
    static constexpr int kNumBins  = kFFTSize / 2 + 1;

    explicit PhaseVocoder (int numChannels);

    // Set time-stretch ratio.  Only recomputes if value has changed.
    void setStretchRatio (double ratio);

    // Clear all internal state (input queue, phase accumulators, output queue).
    void reset();

    // Push numSamples per-channel raw file samples into the analysis queue.
    // Internally triggers as many analysis frames as possible.
    void push (const juce::AudioBuffer<float>& in, int startSample, int numSamples);

    // Pull up to numSamples of pitch-preserved stretched output.
    // Returns the number of samples actually written (may be less if queue is short).
    int pull (juce::AudioBuffer<float>& out, int startSample, int numSamples);

    // How many output samples are fully settled and ready to read.
    int getOutputAvailable() const;

private:
    juce::dsp::FFT mFFT;
    int            mNumCh;
    double         mStretchRatio { 1.0 };
    int            mSynthHop     { kHopSize };

    // Hann window (analysis + synthesis)
    std::vector<float> mWindow;       // [kFFTSize]
    float              mWindowScale;  // OLA normalization

    // Per-channel state
    struct Channel
    {
        // ── Input queue ───────────────────────────────────────────────────
        // Circular ring, size = kFFTSize * 4.
        // inRead points to the oldest sample of the current analysis window.
        // inAvail = samples available ahead of inRead.
        std::vector<float> inRing;
        int inRead  { 0 };
        int inWrite { 0 };
        int inAvail { 0 };

        // ── Phase tracking ────────────────────────────────────────────────
        std::vector<float> lastPhase;   // [kNumBins] - previous frame's phase
        std::vector<float> phaseAccum;  // [kNumBins] - accumulated synthesis phase

        // ── Output OLA buffer ─────────────────────────────────────────────
        // Linear (non-circular) with absolute counters so OLA indexing is simple.
        // Size = kFFTSize * 8.  outReadAbs and outWriteAbs are absolute sample
        // positions; physical index = pos % bufSize.  int64 so the counters
        // can't overflow into a negative modulo under continuous playback (an
        // int wraps after ~12 h @ 48k -> OOB read).
        std::vector<float> outBuf;
        int64_t outReadAbs  { 0 };  // next sample to deliver to caller
        int64_t outWriteAbs { 0 };  // OLA write head (advances by mSynthHop per frame)
    };
    std::vector<Channel> mCh;

    // FFT work buffers (complex, size kFFTSize)
    using Cx = juce::dsp::Complex<float>;
    std::vector<Cx> mFftA;  // analysis  input → FFT output
    std::vector<Cx> mFftS;  // synthesis modified spectrum → IFFT output

    // Called internally by push() whenever inAvail >= kFFTSize
    void processFrame();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseVocoder)
};
