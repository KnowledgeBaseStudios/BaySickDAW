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
// prepare() is the one exception - message/worker thread only, it allocates.
//
// Latency: kFFTSize - mSynthHop samples at the file sample rate (1536 at
// stretch ratio 1; it shrinks as the ratio grows and reaches zero once the
// synthesis hop covers a whole analysis window).

class PhaseVocoder
{
public:
    static constexpr int kFFTOrder = 11;            // 2^11 = 2048
    static constexpr int kFFTSize  = 1 << kFFTOrder;
    static constexpr int kHopSize  = kFFTSize / 4;  // 512 - 75 % overlap
    static constexpr int kNumBins  = kFFTSize / 2 + 1;

    explicit PhaseVocoder (int numChannels);

    // Size the output OLA queue for the deepest per-block demand this instance
    // will ever see: the largest number of output samples the consumer draws
    // between two push() calls (host block size x fileSR/deviceSR x any pitch
    // read rate).  MESSAGE / WORKER THREAD ONLY - allocates, and resets all
    // internal state.  Never call it from the audio callback.
    //
    // There is deliberately no input-side parameter: push() drains analysis
    // frames as it writes, so the input ring's requirement is the analysis
    // window itself and does not move with block size or sample rate.
    void prepare (int maxOutputSamplesPerBlock);

    // Set time-stretch ratio.  Only recomputes if value has changed.
    void setStretchRatio (double ratio);

    // Clear all internal state (input queue, phase accumulators, output queue).
    void reset();

    // Push numSamples per-channel raw file samples into the analysis queue.
    // Internally triggers as many analysis frames as possible.  Any length is
    // safe: frames are drained AS the block is written, so the input ring
    // cannot be outrun no matter how large the caller's block is.
    void push (const juce::AudioBuffer<float>& in, int startSample, int numSamples);

    // Pull up to numSamples of pitch-preserved stretched output.
    // Returns the number of samples actually written (may be less if queue is short).
    int pull (juce::AudioBuffer<float>& out, int startSample, int numSamples);

    // QA-Ec G1-boundary fix (2026-07-08): consumers interpolate the output at
    // a fractional rate, so they need LOOKAHEAD samples they must not consume
    // - pull()'s read-and-clear meant the +2 interp lookahead was thrown away
    // every block (a 2-sample skip at every buffer boundary = audible crackle
    // on stretched clips).  peekOutput copies WITHOUT consuming (slots stay
    // uncleared, read head stays put); advanceOutput then consumes exactly
    // what the caller's fractional position advanced past (and does the OLA
    // slot-clearing pull()'s zeroing previously covered).
    int  peekOutput    (juce::AudioBuffer<float>& out, int startSample, int numSamples);
    void advanceOutput (int numSamples);

    // How many output samples are fully settled and ready to read.
    int getOutputAvailable() const;

private:
    // Floor for the OLA queue, used until prepare() supplies the live demand.
    // It is the pre-existing allocation, kept as the floor so nothing that
    // works today gets a SMALLER buffer than it had.
    static constexpr int kOutBufFloor = kFFTSize * 8;

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
        // Circular ring.  inRead points to the oldest sample of the current
        // analysis window; inAvail = samples available ahead of inRead.
        // CAPACITY LAW: push() drains a frame the moment inAvail reaches
        // kFFTSize, so inAvail is capped at kFFTSize and the ring only has to
        // hold ONE analysis window - block size, sample rate and stretch ratio
        // do not enter the sizing at all.  It is allocated at kFFTSize * 4 for
        // margin, never resized.
        std::vector<float> inRing;
        int inRead  { 0 };
        int inWrite { 0 };
        int inAvail { 0 };

        // ── Phase tracking ────────────────────────────────────────────────
        std::vector<float> lastPhase;   // [kNumBins] - previous frame's phase
        std::vector<float> phaseAccum;  // [kNumBins] - accumulated synthesis phase

        // ── Output OLA buffer ─────────────────────────────────────────────
        // outReadAbs and outWriteAbs are absolute sample positions; physical
        // index = pos % bufSize.  int64 so the counters can't overflow into a
        // negative modulo under continuous playback (an int wraps after ~12 h
        // @ 48k -> OOB read).
        // CAPACITY LAW: the whole push-then-drain block sits here between the
        // two calls, so this one IS block-size and sample-rate dependent -
        // see prepare(), which is how the live sizing gets in.  kFFTSize * 8
        // is only the floor used before prepare() is called.
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
