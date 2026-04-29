#pragma once
#include <JuceHeader.h>
#include <functional>

// ── EqLinearPhaseProcessor ────────────────────────────────────────────────────
// Linear-phase FIR EQ processor implemented via 50%-overlap-add Hann-windowed
// short-time Fourier convolution. Produced for §12 EQ8 Phase 3 item 12g.
//
// Algorithm (standard windowed-spectrum-processor):
//   prepare:
//     mFftSize = N (per-mode: HQE=512, Linear=2048, HQ Linear=4096)
//     mHop = N/2
//     mWindow = Hann(N) - both analysis and synthesis windowed for OLA reconstruction
//   rebuildIR:
//     Walk N/2+1 frequency bins; mIrMagBins[k] = magnitudeFn(k * sr / N).
//     Cached as REAL magnitude (zero phase = linear phase).
//   processBlock:
//     For each input sample:
//       - Append to per-channel input ring
//       - Pop one sample from the OLA output ring (the delayed, EQ-filtered sample)
//       - When mHop new samples have accumulated:
//           - Snapshot the most recent N samples from the ring
//           - Window with Hann
//           - FFT
//           - Multiply each bin by mIrMagBins[k] (real magnitude)
//           - IFFT
//           - Window again with Hann (synthesis window for OLA reconstruction)
//           - OLA-add into the output ring at the current write head, advance by mHop
//
// Latency = N/2 samples (the centroid of the Hann window). Reported via
// getLatencySamples() and summed into the host PDC by EQ8DSP/EQ8MsDSP/VibeGraph.
//
// Cross-feature interactions (handled by the caller, EQ8DSP):
//   - Per-band M/S (12h): restricted to Stereo in linear modes (T2a option B).
//     This processor sees pristine L+R and processes them independently.
//   - Dynamic EQ (12j): disabled in linear modes (T2b option C). Magnitude
//     callback uses static design gain only - GR is not summed in.
//   - TPT engine (12e): folded in via the analog-prototype magnitude already
//     served by EQ8DSP::getMagnitudeForFrequency. No special handling here.
//   - Anti-cramping (12f): outer caller wraps this processor in the AC bracket
//     when both AC and a linear mode are active. designSr() drives the IR
//     bin frequencies through the magnitude callback.
//
// CPU per channel per block:
//   - Hop comparisons + ring writes: ~negligible
//   - When a frame fires (every mHop samples):
//       - 1 FFT + 1 IFFT of size N
//       - O(N) windowing and OLA copies
//   At 48 kHz, frame rate = sr/hop. For N=2048, hop=1024 -> 47 frames/sec/ch.
//   For N=4096, hop=2048 -> 23 frames/sec/ch. Cheap.
// ─────────────────────────────────────────────────────────────────────────────
class EqLinearPhaseProcessor
{
public:
    EqLinearPhaseProcessor() = default;
    ~EqLinearPhaseProcessor() = default;

    // Configure for a given FFT size + channel count + sample rate. Allocates
    // FFT, scratch and per-channel ring buffers. Calling with the same fftSize
    // is a cheap re-init (state reset only).
    void prepare (int fftSize, int numChannels, double sampleRate);

    // Clear all per-channel ring + OLA state. Cached IR spectrum preserved.
    void reset();

    // Rebuild the IR magnitude bins from a callback returning linear magnitude
    // at a given frequency in Hz. Cheap (N/2+1 callback calls + no FFT - bins
    // are stored directly as real magnitudes; the IFFT happens implicitly each
    // frame via the spectrum-domain multiply).
    void rebuildIR (const std::function<float(float)>& magnitudeFn);

    // Process a stereo (or mono) block in place. Output is delayed by
    // getLatencySamples() = mFftSize/2. Takes an AudioBlock so the EQ8DSP
    // caller can pass the oversampled block directly when in HQ Linear mode.
    void process (juce::dsp::AudioBlock<float>& block);

    int  getLatencySamples() const noexcept { return mFftSize > 0 ? mFftSize / 2 : 0; }
    int  getFftSize()        const noexcept { return mFftSize; }
    bool isReady()           const noexcept { return mFft != nullptr; }

    // Per-mode FFT size lookup (T2d locked HQE at 512). EQ8DSP::PhaseMode
    // values: 0 Standard / 1 Linear / 2 HQ+ / 3 HQ Linear / 4 HQ Extended.
    // Standard + HQ+ return 0 (no linear processing); the others return the
    // per-spec FFT size.
    static int fftSizeForMode (int phaseMode) noexcept;

private:
    int mFftSize { 0 };
    int mHop     { 0 };
    int mNumCh   { 0 };
    double mSampleRate { 0.0 };

    std::unique_ptr<juce::dsp::FFT> mFft;

    std::vector<float> mWindow;                                  // [N]
    std::vector<float> mIrMagBins;                               // [N/2+1] real
    std::vector<juce::dsp::Complex<float>> mFftScratch;          // [N]

    struct Channel
    {
        std::vector<float> inRing;       // [N], circular
        int                inWritePos { 0 };
        int                samplesSinceFrame { 0 };

        std::vector<float> outRing;      // [N], circular
        int                outReadPos { 0 };
        int                outWritePos { 0 };
    };
    std::vector<Channel> mChannels;

    float mNormScale { 1.0f };           // Hann-OLA reconstruction scale

    void processFrame (int channelIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqLinearPhaseProcessor)
};
