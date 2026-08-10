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

    void getStateInformation (juce::MemoryBlock& dest) override;
    void setStateInformation (const void* data, int sz) override;

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

    // How far back the tail estimate reads the magnitude history so a note's
    // own attack is never mistaken for its reverb.  A DURATION, not a frame
    // count: the frame rate is sr/kHop, so the frame count that buys 35 ms at
    // 44.1 kHz (3 frames of 11.6 ms) buys only 16 ms at 96 kHz and 8 ms at
    // 192 kHz -- the direct sound leaks into the estimate and the voice goes
    // hollow.  35 ms is the 44.1 kHz value, preserved.
    static constexpr float kDirectGuardMs = 35.0f;

    struct Channel
    {
        std::vector<float> inRing;                  // [kFFT * 4]
        int inWrite { 0 }, inAvail { 0 }, inRead { 0 };
        std::vector<float> outBuf;                  // [kFFT * 8]
        int64_t outReadAbs { 0 }, outWriteAbs { 0 };
    };

    void processFrame (int numCh);
    void updateDecay();
    void updateGuard();   // allocates mMagHist - prepare-time only

    juce::dsp::FFT mFFT { kOrder };
    std::vector<float> mWindow;
    float mOlaScale { 0.0f };

    Channel mCh[2];
    std::vector<juce::dsp::Complex<float>> mSpec[2], mFftA, mFftS;

    std::vector<float> mMagHist;                    // [mHistFrames * kBins] ring of avg mags
    int   mHistWrite   { 0 };
    int   mGuardFrames { 3 };                       // kDirectGuardMs in frames at the live rate
    int   mHistFrames  { 4 };                       // mGuardFrames + 1 (the delayed slot is the oldest)
    std::vector<float> mRev;                        // [kBins] running tail estimate
    std::vector<float> mGain, mGainSm;              // [kBins]
    float mDecayPerHop { 0.9f };

    // The gain smoother's attack/release were bare per-FRAME coefficients, and
    // the frame rate is sr/kHop -- so their real time constants shortened with
    // the sample rate exactly like the direct-sound guard did.  These are the
    // measured 44.1 kHz durations of the old 0.6 / 0.3 constants, preserved and
    // then re-derived per rate in updateGuard().
    static constexpr float kGainAttackMs  = 12.67f;   // was coef 0.6 per frame
    static constexpr float kGainReleaseMs = 31.85f;   // was coef 0.3 per frame
    float mGainAttackCoef  { 0.6f };
    float mGainReleaseCoef { 0.3f };

    std::atomic<float> mGrDb { 0.0f };
};
