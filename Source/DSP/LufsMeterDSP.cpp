#include "LufsMeterDSP.h"

// K-weighting filter design — bilinear-from-constants (exact at any fs).
// Analog-prototype constants cross-checked across libebur128 + loudness.py +
// klangfreund.  ACCEPTANCE @48k: shelf ~ {b0=1.53512,b1=-2.69170,b2=1.19839,
// a1=-1.69066,a2=0.73248}; RLB ~ {1,-2,1, a1=-1.99005,a2=0.99007}.
// juce::dsp::IIR::Coefficients takes (b0,b1,b2,a0,a1,a2) with the standard
// subtract convention; we pass already-normalized values (a0 = 1).
void LufsMeterDSP::designKWeighting (double fs)
{
    auto mk = [] (double b0, double b1, double b2, double a1, double a2)
    {
        return new juce::dsp::IIR::Coefficients<float> (
            (float) b0, (float) b1, (float) b2, 1.0f, (float) a1, (float) a2);
    };

    // Stage 1 — high shelf.
    {
        const double f0 = 1681.9744509555319, G = 3.99984385397, Q = 0.7071752369554193;
        const double K  = std::tan (juce::MathConstants<double>::pi * f0 / fs);
        const double Vh = std::pow (10.0, G / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        auto* c = mk ((Vh + Vb * K / Q + K * K) / a0,
                      2.0 * (K * K - Vh) / a0,
                      (Vh - Vb * K / Q + K * K) / a0,
                      2.0 * (K * K - 1.0) / a0,
                      (1.0 - K / Q + K * K) / a0);
        mShelfL.coefficients = c;   // pointer copy (shared, immutable); state is per-channel
        mShelfR.coefficients = c;
    }

    // Stage 2 — RLB high-pass.
    {
        const double f0 = 38.13547087613982, Q = 0.5003270373253953;
        const double K  = std::tan (juce::MathConstants<double>::pi * f0 / fs);
        const double a0 = 1.0 + K / Q + K * K;
        auto* c = mk (1.0 / a0, -2.0 / a0, 1.0 / a0,
                      2.0 * (K * K - 1.0) / a0,
                      (1.0 - K / Q + K * K) / a0);
        mHpL.coefficients = c;
        mHpR.coefficients = c;
    }
}

void LufsMeterDSP::prepareToPlay (double sampleRate)
{
    mSr = (sampleRate > 0.0) ? sampleRate : 48000.0;
    designKWeighting (mSr);

    mSamplesPerBin = juce::jmax (1, (int) std::lround (mSr / (double) kBinsPerSec));
    mBinEL.assign ((size_t) kShortTermBins, 0.0);
    mBinER.assign ((size_t) kShortTermBins, 0.0);
    mBinHead = mSampleInBin = mBinsFilled = mBinsSinceGate = 0;
    mCurL = mCurR = 0.0;

    // processSample needs only coefficients (set above) + cleared state.
    mShelfL.reset(); mShelfR.reset(); mHpL.reset(); mHpR.reset();

    mM.store (-120.f, std::memory_order_relaxed);
    mS.store (-120.f, std::memory_order_relaxed);
    resetIntegrated();
}

void LufsMeterDSP::process (const juce::AudioBuffer<float>& buf)
{
    const int n  = buf.getNumSamples();
    const int nc = buf.getNumChannels();
    if (n <= 0 || nc <= 0) return;

    const float* L = buf.getReadPointer (0);
    const float* R = (nc >= 2) ? buf.getReadPointer (1) : L;

    for (int s = 0; s < n; ++s)
    {
        const float kl = mHpL.processSample (mShelfL.processSample (L[s]));   // shelf -> RLB
        const float kr = mHpR.processSample (mShelfR.processSample (R[s]));
        mCurL += (double) kl * (double) kl;
        mCurR += (double) kr * (double) kr;
        if (++mSampleInBin >= mSamplesPerBin)
            closeBin();
    }
}

void LufsMeterDSP::closeBin()
{
    mBinEL[(size_t) mBinHead] = mCurL;
    mBinER[(size_t) mBinHead] = mCurR;
    mCurL = mCurR = 0.0;
    mSampleInBin = 0;
    mBinHead = (mBinHead + 1) % kShortTermBins;
    if (mBinsFilled < kShortTermBins) ++mBinsFilled;

    mM.store (windowLufs (kMomentaryBins), std::memory_order_relaxed);
    mS.store (windowLufs (kShortTermBins), std::memory_order_relaxed);

    if (++mBinsSinceGate >= kGateStepBins)
    {
        mBinsSinceGate = 0;
        accumulateIntegrated();
    }
}

// Ungated window loudness over the most-recent `bins` (capped to what's filled).
float LufsMeterDSP::windowLufs (int bins) const
{
    const int avail = juce::jmin (bins, mBinsFilled);
    if (avail <= 0) return -120.f;

    double eL = 0.0, eR = 0.0;
    for (int i = 0; i < avail; ++i)
    {
        const int j = (mBinHead - 1 - i + 2 * kShortTermBins) % kShortTermBins;
        eL += mBinEL[(size_t) j];
        eR += mBinER[(size_t) j];
    }
    const double ms = (eL + eR) / ((double) mSamplesPerBin * (double) avail);   // G_L=G_R=1
    return (float) lufsFromMs (ms);
}

// Form one 400 ms gating block from the bin ring; histogram it (after the -70
// absolute gate); then recompute the integrated value via the -10 LU relative
// gate over the histogram.  Called once per 100 ms (75% overlap).
void LufsMeterDSP::accumulateIntegrated()
{
    if (mBinsFilled < kMomentaryBins) return;   // need a full 400 ms first

    double eL = 0.0, eR = 0.0;
    for (int i = 0; i < kMomentaryBins; ++i)
    {
        const int j = (mBinHead - 1 - i + 2 * kShortTermBins) % kShortTermBins;
        eL += mBinEL[(size_t) j];
        eR += mBinER[(size_t) j];
    }
    const double blockMs   = (eL + eR) / ((double) mSamplesPerBin * (double) kMomentaryBins);
    const double blockLufs = lufsFromMs (blockMs);
    if (blockLufs < kAbsGateLufs) return;       // absolute -70 LUFS gate (drop at insert)

    int bin = (int) std::floor ((blockLufs - kHistMinLufs) / kHistStep + 0.5);
    bin = juce::jlimit (0, kHistBins - 1, bin);
    mHistSumMs[(size_t) bin] += blockMs;
    mHistCount[(size_t) bin] += 1;

    // Pass 1 — mean energy of all absolute-gated blocks -> relative threshold.
    double    sumAll = 0.0;
    long long cntAll = 0;
    for (int b = 0; b < kHistBins; ++b) { sumAll += mHistSumMs[(size_t) b]; cntAll += mHistCount[(size_t) b]; }
    if (cntAll == 0) { mIntegratedLufs.store (-120.f, std::memory_order_relaxed); return; }

    const double meanAll       = sumAll / (double) cntAll;
    const double relThreshLufs = lufsFromMs (meanAll) + kRelGateLu;   // mean - 10 LU
    int relBin = (int) std::ceil ((relThreshLufs - kHistMinLufs) / kHistStep);
    relBin = juce::jlimit (0, kHistBins, relBin);

    // Pass 2 — mean energy of blocks above the relative gate.
    double    sumG = 0.0;
    long long cntG = 0;
    for (int b = relBin; b < kHistBins; ++b) { sumG += mHistSumMs[(size_t) b]; cntG += mHistCount[(size_t) b]; }

    const double integ = (cntG > 0) ? lufsFromMs (sumG / (double) cntG)
                                    : lufsFromMs (meanAll);
    mIntegratedLufs.store ((float) integ, std::memory_order_relaxed);
}

void LufsMeterDSP::resetIntegrated() noexcept
{
    mHistSumMs.fill (0.0);
    mHistCount.fill (0);
    mIntegratedLufs.store (-120.f, std::memory_order_relaxed);
}
