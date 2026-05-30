#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

// -- LufsMeterDSP --------------------------------------------------------------
// EBU R128 / BS.1770 loudness on a stereo bus (the master bus).
//   Momentary  (400 ms)  + Short-Term (3 s)  = UNgated sliding windows.
//   Integrated (gated)   = -70 LUFS absolute + -10 LU relative gate over
//                          400 ms / 75%-overlap gating blocks; resets on
//                          transport play-from-top / loop (the owner node calls
//                          resetIntegrated()).
//
// K-weighting (Stage 1 high-shelf + Stage 2 RLB high-pass) is derived
// bilinear-from-constants per prepareToPlay so it is exact at any sample rate
// (44.1 / 48 / 96).  Recipe + the 48 kHz acceptance table live in
//   Plans & Specs/Research Reports/
//   daw-architecture-lufs-momentary-shortterm-metering-2026-05-29.md
//
// All three values broadcast via relaxed atomics (current value, NOT a peak --
// no CAS-max); the UI reads them once per vblank.
//
// Audio-thread alloc-free after prepareToPlay: the Integrated gate uses a
// fixed-size loudness histogram (libebur128's method), so no per-block growth.
class LufsMeterDSP
{
public:
    // Setup (message/prepare thread): derive K-weighting + size the bin ring.
    void prepareToPlay (double sampleRate);

    // Audio thread: feed the post-fader/post-width master stereo sum.
    void process (const juce::AudioBuffer<float>& buf);

    // Audio thread: clear the Integrated accumulation (play-from-top / loop).
    // Does NOT touch Momentary / Short-Term (those keep tracking continuously).
    void resetIntegrated() noexcept;

    // UI thread: current values (LUFS; -120 floor when silent / not yet filled).
    float momentary()  const noexcept { return mM.load (std::memory_order_relaxed); }
    float shortTerm()  const noexcept { return mS.load (std::memory_order_relaxed); }
    float integrated() const noexcept { return mIntegratedLufs.load (std::memory_order_relaxed); }

private:
    // ── Window / bin geometry ────────────────────────────────────────────────
    static constexpr int    kBinsPerSec    = 20;     // 50 ms bins (>=10 Hz EBU hop)
    static constexpr int    kMomentaryBins = 8;      // 400 ms
    static constexpr int    kShortTermBins = 60;     // 3 s (also the ring size)
    static constexpr int    kGateStepBins  = 2;      // 100 ms gating step (75% overlap)
    static constexpr double kOffset        = -0.691; // BS.1770 absolute offset
    static constexpr double kAbsGateLufs   = -70.0;  // Integrated absolute gate
    static constexpr double kRelGateLu     = -10.0;  // Integrated relative gate (mean - 10)

    // ── Integrated loudness histogram (fixed, alloc-free) ────────────────────
    static constexpr double kHistMinLufs = -70.0;    // = the absolute gate
    static constexpr double kHistStep    = 0.1;      // 0.1 LU resolution
    static constexpr int    kHistBins    = 751;      // -70.0 .. +5.0 LUFS inclusive

    void  designKWeighting (double fs);
    void  closeBin();
    float windowLufs (int bins) const;
    void  accumulateIntegrated();
    static double lufsFromMs (double ms) noexcept
        { return ms > 1.0e-12 ? (kOffset + 10.0 * std::log10 (ms)) : -120.0; }

    // K-weighting: 2 stages x 2 channels (per-channel filter state, shared coeffs).
    juce::dsp::IIR::Filter<float> mShelfL, mShelfR, mHpL, mHpR;

    double mSr            = 48000.0;
    int    mSamplesPerBin = 2400;
    int    mSampleInBin   = 0;
    int    mBinHead       = 0;     // next write index into the energy ring
    int    mBinsFilled    = 0;     // how many ring slots hold real energy (capped)
    int    mBinsSinceGate = 0;     // bins since the last 400 ms gating block
    double mCurL = 0.0, mCurR = 0.0;   // running K-weighted sum-of-squares this bin

    std::vector<double> mBinEL, mBinER;   // per-bin K-weighted energy ring (kShortTermBins)
    std::array<double, kHistBins> mHistSumMs { };   // sum of gated block mean-squares per bin
    std::array<int,    kHistBins> mHistCount { };   // gated block count per bin

    std::atomic<float> mM             { -120.f };
    std::atomic<float> mS             { -120.f };
    std::atomic<float> mIntegratedLufs{ -120.f };
};
