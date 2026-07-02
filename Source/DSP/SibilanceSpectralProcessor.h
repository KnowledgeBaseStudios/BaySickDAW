#pragma once
#include <JuceHeader.h>
#include <atomic>

// ── SibilanceSpectralProcessor - QA-EffectsReview Task 8 (2026-07-01) ─────────
// STFT de-esser engine for DeEsserDSP's Spectral mode.  Emulates the
// reference's per-event transparency (Waves Sibilance / ORS) via the standard
// public-domain route: per-bin magnitude masking (iZotope-RX-style), NOT a
// literal ORS reimplementation (proprietary).
//
// Pipeline per frame (identity OLA scaffolded on PhaseVocoder's ring/OLA
// mechanics, minus phase manipulation):
//   Hann window -> FFT -> per-bin real gain (phase untouched) -> IFFT ->
//   Hann window -> overlap-add.  With all gains 1 the output is the input
//   delayed by exactly fftSize samples (COLA identity; see the latency note).
//
// Mask model (mirrors the time-domain detector's semantics):
//   * Event gate: sibilance-band RMS (dBFS-calibrated) vs threshold ->
//     saturating overshoot curve (same 1-exp(-x/6) as the time path) ->
//     event reduction toward rangeDb.
//   * Per-bin selectivity: each in-band bin vs its own slow floor EMA
//     (rises slowly so an ess can't lift its own floor, falls faster) --
//     bins spiking above the vowel bed take the reduction, bins at the bed
//     stay untouched.  That's the "only touches the ess" behavior.
//   * Mode blend: 0 = Wide (uniform event duck across the whole spectrum),
//     1 = Split (selectivity-shaped, in-band only).  In-band starts at the
//     detector cutoff (>= 4 kHz), so the reference's fixed-4 kHz split
//     restriction holds automatically.
//   * Per-bin attack/release smoothing at frame rate + 3-bin frequency
//     smoothing (musical-noise / birdie suppression).
//
// Quality configs (both preallocated; runtime switch is allocation-free):
//   HQ = 2048/512 (latency 2048, ~43 ms @ 48k), LL = 1024/256 (latency 1024,
//   ~21 ms).  Latency = one full FFT frame -- exact for EVERY host block size
//   (the delivery primes fftSize zeros; an (N - hop) contract only holds for
//   hop-multiple blocks).  setQuality
//   stores a pending index; process() applies the swap + reset on the audio
//   thread at the next block (the UI toggle is playback-locked, but the audio
//   callback still runs while stopped -- swapping buffers from the message
//   thread would race process()).  getLatencySamples reports the PENDING
//   config so the UI's PDC refresh right after the toggle reads the imminent
//   truth, not the one-block-stale active value.
//
// Threading: prepare/reset message thread (audio stopped); process audio
// thread; setQuality message thread (pending-atomic handoff as above).
// ─────────────────────────────────────────────────────────────────────────────

class SibilanceSpectralProcessor
{
public:
    struct Params
    {
        float detectLoHz    { 6500.0f };  // sibilance band low edge (band top is 16 kHz)
        float thresholdDb   { -12.5f };
        float rangeDb       { -14.0f };   // max reduction (negative)
        float blend01       { 0.5f };     // 0=Wide, 1=Split
        float attackMs      { 1.0f };
        float releaseMs     { 80.0f };
        bool  outputRemoved { false };    // Monitor: reconstruct (1-gain) bins instead
    };

    SibilanceSpectralProcessor();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // 0 = HQ (2048/512), 1 = LL (1024/256).  Applied on the audio thread at
    // the next process() call (see header comment).
    void setQuality (int q) noexcept;
    int  getQuality() const noexcept { return mPendingQuality.load (std::memory_order_relaxed); }

    // Latency of the PENDING config (one FFT frame): 2048 HQ / 1024 LL.
    int getLatencySamples() const noexcept;

    // Most negative in-band gain applied in the last analyzed frame (dB, <= 0).
    float getLastReductionDb() const noexcept
    {
        return mLastGrDb.load (std::memory_order_relaxed);
    }

    // In-place block process; r may be nullptr (mono / M-S axis).  Output is
    // delayed by getLatencySamples() of the ACTIVE config.
    void process (float* l, float* r, int numSamples, const Params& p);

private:
    static constexpr int kMaxOrder   = 11;               // HQ 2^11 = 2048
    static constexpr int kMaxFFT     = 1 << kMaxOrder;
    static constexpr int kMaxBins    = kMaxFFT / 2 + 1;
    static constexpr float kBandHiHz = 16000.0f;         // sibilance band top

    struct Config
    {
        int fftOrder { kMaxOrder };
        int fftSize  { kMaxFFT };
        int hop      { kMaxFFT / 4 };                     // 75% overlap (both configs)
    };

    struct Channel
    {
        std::vector<float> inRing;       // [kMaxFFT * 4]
        int inRead  { 0 };
        int inWrite { 0 };
        int inAvail { 0 };
        std::vector<float> outBuf;       // [kMaxFFT * 8], absolute counters like PhaseVocoder
        // int64 so the counters can't overflow into a negative modulo under
        // continuous playback (an int wraps after ~12 h @ 48k -> OOB read).
        int64_t outReadAbs  { 0 };
        int64_t outWriteAbs { 0 };
        int primeRemaining { 0 };        // zeros still owed = one FFT frame after reset
        std::vector<float> floorDb;      // [kMaxBins] per-bin slow floor EMA
        std::vector<float> gain;         // [kMaxBins] per-bin smoothed gain (0..1)
    };

    void applyPendingQuality();          // audio thread; swap + reset when pending != active
    void processFrame (int numCh, const Params& p);
    void resetState();

    Config mCfg[2];                       // [0]=HQ, [1]=LL
    int    mActiveQuality { 0 };
    std::atomic<int> mPendingQuality { 0 };

    juce::dsp::FFT mFftHQ { kMaxOrder };
    juce::dsp::FFT mFftLL { kMaxOrder - 1 };

    std::vector<float> mWindowHQ;         // Hann per config
    std::vector<float> mWindowLL;
    float mWindowScaleHQ { 0.0f };        // 2/3 OLA normalization (Hann, 75%; JUCE IFFT already 1/N)
    float mWindowScaleLL { 0.0f };

    Channel mCh[2];

    using Cx = juce::dsp::Complex<float>;
    std::vector<Cx> mSpec[2];             // [kMaxFFT] per-channel analysis spectra
    std::vector<Cx> mFftA, mFftS;         // [kMaxFFT] work buffers
    std::vector<float> mGainScratch;      // [kMaxBins] frequency-smoothing scratch

    double mSampleRate { 44100.0 };
    std::atomic<float> mLastGrDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SibilanceSpectralProcessor)
};
