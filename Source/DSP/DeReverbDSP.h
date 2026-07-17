#pragma once
#include "DSPBase.h"
#include <atomic>
#include <vector>

// De-reverb - QA-Fe2 (2026-07-16).  Late-reverberation suppressor, slot 1 of
// the locked vocal chain (ahead of the compressor so the tail can't be
// pumped back up).  Classic Lebart-family estimator, NOT ML/RX-grade (stated
// + accepted at spec lock): per bin, the reverb magnitude is modeled as a
// delayed copy of the signal's own recent magnitude decaying at the
// user-set Tail rate; a Wiener-style gain with a floor subtracts it.
//
// STFT 2048/512 Hann analysis+synthesis, identity OLA (COLA scheme borrowed
// from SibilanceSpectralProcessor, single fixed quality).  Latency = one
// full FFT frame (2048), reported for PDC like the spectral De-esser.
// Gains are computed from the channel-average magnitude and applied to both
// channels so the stereo image cannot twist.  Mix is a per-bin gain lerp
// toward unity (phase-coherent; no separate dry delay line needed).
class DeReverbDSP : public DSPBase
{
public:
    DeReverbDSP();

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    int getLatencySamples() const noexcept override { return kFFT; }
    float getGainReductionDb() const noexcept override
        { return mGrDb.load (std::memory_order_relaxed); }

    // Parameter setters (all CPU-guarded: no-op when unchanged)
    void setReductionPct (float pct);    // 0..100
    void setTailMs       (float ms);     // 100..1000 (T60 of the modeled tail)
    void setMixPct       (float pct);    // 0..100

    // Parameters are public for UI read; use the setters to change.
    float reductionPct { 50.0f };
    float tailMs       { 400.0f };
    float mixPct       { 100.0f };

private:
    static constexpr int kOrder = 11;
    static constexpr int kFFT   = 1 << kOrder;      // 2048
    static constexpr int kHop   = kFFT / 4;         // 75% overlap
    static constexpr int kBins  = kFFT / 2 + 1;
    static constexpr int kHist  = 8;                // magnitude history frames
    static constexpr int kDelayFrames = 3;          // skip direct sound (~35 ms)

    struct Channel
    {
        std::vector<float> inRing;                  // [kFFT * 4]
        int inWrite { 0 }, inAvail { 0 }, inRead { 0 };
        std::vector<float> outBuf;                  // [kFFT * 8]
        int64_t outReadAbs { 0 }, outWriteAbs { 0 };
    };

    void processFrame (int numCh);
    void updateDecay();

    juce::dsp::FFT mFFT { kOrder };
    std::vector<float> mWindow;
    float mOlaScale { 0.0f };

    Channel mCh[2];
    std::vector<juce::dsp::Complex<float>> mSpec[2], mFftA, mFftS;

    std::vector<float> mMagHist;                    // [kHist * kBins] ring of avg mags
    int   mHistWrite { 0 };
    std::vector<float> mRev;                        // [kBins] running tail estimate
    std::vector<float> mGain, mGainSm;              // [kBins]
    float mDecayPerHop { 0.9f };

    std::atomic<float> mGrDb { 0.0f };
};
