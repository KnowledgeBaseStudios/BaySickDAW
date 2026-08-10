#pragma once
#include <JuceHeader.h>
#include <memory>

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
// Latency: getLatencySamples() = live window - synthesis hop, in FILE samples
// (1536 at stretch ratio 1 for a 44.1 / 48 kHz file; it shrinks as the ratio
// grows and reaches zero once the synthesis hop covers a whole analysis
// window).  Nothing outside this class currently reports it into host PDC -
// the stretched decode is position-mapped by its caller, not delay-compensated.
//
// WINDOW SIZE IS A DURATION, NOT A SAMPLE COUNT.  A fixed 2048-point window is
// about 46 ms of a 44.1 kHz file but only 10.7 ms of a 192 kHz one, so the same
// clip stretched with the same ratio smeared differently depending purely on
// what rate it was recorded at.  prepare() takes the SOURCE FILE's rate and
// picks the power-of-two window nearest that reference duration, so the
// analysis sees the same slice of music across the 44.1 - 192 kHz range.
// OUTSIDE that range the duration is NOT held: the order floors at kFFTOrder
// (a 22.05 kHz file gets a 93 ms window, double the reference) and caps at
// kMaxFFTOrder (a 384 kHz file gets 21 ms, half of it).  Both bounds are
// deliberate - see fftOrderForFileRate, where the floor is load-bearing for
// the external hop grid, not just a cost cap.

class PhaseVocoder
{
public:
    // REFERENCE geometry - the window and hop a 44.1 / 48 kHz file uses, and
    // the EXTERNAL CONTRACT other translation units compile against.
    //
    // kHopSize is a wire format, not a tuning knob.  PluginProcessor's live
    // warp path, its offline stretch decode and BaySickAlignDSP's warp worker
    // all derive the effective stretch as
    //     jmax (1, roundToInt (kHopSize * ratio)) / kHopSize
    // and use that as EXACT produced-sample bookkeeping (the comment at the
    // offline site spells out what a mismatch costs: ~0.1 % position drift,
    // tens of ms over a minute).  So the realized synthesis/analysis hop ratio
    // must land on that grid at EVERY window size - see updateSynthesisHop,
    // which quantizes on kHopSize and then scales by the whole-number overlap
    // factor, keeping the identity exact instead of approximate.
    static constexpr int    kFFTOrder      = 11;            // 2^11 = 2048
    static constexpr int    kFFTSize       = 1 << kFFTOrder;
    static constexpr int    kHopSize       = kFFTSize / 4;  // 512 - 75 % overlap
    static constexpr int    kNumBins       = kFFTSize / 2 + 1;
    static constexpr double kReferenceRate = 44100.0;

    // Ceiling on the live window.  Order 13 carries the reference duration up
    // through 176.4 / 192 kHz; faster files clamp here rather than paying for
    // an FFT the stretch quality does not need.
    static constexpr int kMaxFFTOrder = 13;
    static constexpr int kMaxFFTSize  = 1 << kMaxFFTOrder;

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
    //
    // fileSampleRate is the rate of the MATERIAL being stretched, not the
    // device rate - it is what sets the live window size (see the duration
    // note at the top of this file).  Pass 0 (the default) only when the
    // source rate is genuinely unknown; the window then stays on the
    // 44.1 / 48 kHz reference geometry, which is exactly the pre-2026-08-10
    // behavior.
    void prepare (int maxOutputSamplesPerBlock, double fileSampleRate = 0.0);

    // Live geometry.  The window and the analysis hop both move with the file
    // rate; the synthesis hop also moves with the stretch ratio.
    int getFFTSize()      const noexcept { return mFFTSize; }
    int getAnalysisHop()  const noexcept { return mAnalysisHop; }
    int getSynthesisHop() const noexcept { return mSynthHop; }

    // OLA group delay in FILE samples - see the Latency note at the top.
    int getLatencySamples() const noexcept
        { return juce::jmax (0, mFFTSize - mSynthHop); }

    // Power-of-two FFT order whose window duration is nearest kFFTSize at
    // kReferenceRate, floored at kFFTOrder and capped at kMaxFFTOrder.
    // Public so a caller that has to pre-roll or position-map against the
    // window can ask for the geometry BEFORE the vocoder exists.
    static int fftOrderForFileRate (double fileSampleRate) noexcept;

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

    // juce::dsp::FFT fixes its order at construction and is not assignable, so
    // a live window size forces the indirection: prepare() rebuilds it (message
    // thread, allocates) only when the order actually changes.
    std::unique_ptr<juce::dsp::FFT> mFFT;

    int            mNumCh;
    double         mStretchRatio { 1.0 };
    int            mFFTOrder     { kFFTOrder };
    int            mFFTSize      { kFFTSize };
    int            mAnalysisHop  { kHopSize };   // always mFFTSize / 4
    int            mSynthHop     { kHopSize };

    // Hann window (analysis + synthesis)
    std::vector<float> mWindow;       // [mFFTSize]
    float              mWindowScale;  // OLA normalization

    // Per-channel state
    struct Channel
    {
        // ── Input queue ───────────────────────────────────────────────────
        // Circular ring.  inRead points to the oldest sample of the current
        // analysis window; inAvail = samples available ahead of inRead.
        // CAPACITY LAW: push() drains a frame the moment inAvail reaches the
        // live window, so inAvail is capped there and the ring only has to hold
        // ONE analysis window - block size, device rate and stretch ratio do not
        // enter the sizing at all.  It is allocated at four windows for margin,
        // and only ever re-sized by prepare() when the window itself changes.
        std::vector<float> inRing;
        int inRead  { 0 };
        int inWrite { 0 };
        int inAvail { 0 };

        // ── Phase tracking ────────────────────────────────────────────────
        std::vector<float> lastPhase;   // [mFFTSize/2 + 1] - previous frame's phase
        std::vector<float> phaseAccum;  // [mFFTSize/2 + 1] - accumulated synthesis phase

        // ── Output OLA buffer ─────────────────────────────────────────────
        // outReadAbs and outWriteAbs are absolute sample positions; physical
        // index = pos % bufSize.  int64 so the counters can't overflow into a
        // negative modulo under continuous playback (an int wraps after ~12 h
        // @ 48k -> OOB read).
        // CAPACITY LAW: the whole push-then-drain block sits here between the
        // two calls, so this one IS block-size and sample-rate dependent -
        // see prepare(), which is how the live sizing gets in.  Eight windows
        // is only the floor used before prepare() is called.
        std::vector<float> outBuf;
        int64_t outReadAbs  { 0 };  // next sample to deliver to caller
        int64_t outWriteAbs { 0 };  // OLA write head (advances by mSynthHop per frame)
    };
    std::vector<Channel> mCh;

    // FFT work buffers (complex, size mFFTSize)
    using Cx = juce::dsp::Complex<float>;
    std::vector<Cx> mFftA;  // analysis  input → FFT output
    std::vector<Cx> mFftS;  // synthesis modified spectrum → IFFT output

    // Called internally by push() whenever inAvail >= mFFTSize
    void processFrame();

    // Re-derives mSynthHop + mWindowScale from the current ratio and geometry.
    // Both the ratio and the window can move, and Hs depends on both.
    void updateSynthesisHop();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseVocoder)
};
