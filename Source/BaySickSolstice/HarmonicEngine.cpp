#include "HarmonicEngine.h"
#include <cmath>
#include <mutex>

HarmonicEngine::HarmonicEngine()
    : mFFT (kFFTOrder)
{
    setShape (Shape::Saw);   // default: sawtooth (all harmonics 1/n)
}

//==============================================================================
const float* HarmonicEngine::getSineLUT() noexcept
{
    static std::array<float, kLUTSize> lut;
    static std::once_flag flag;
    std::call_once (flag, []()
    {
        for (int i = 0; i < kLUTSize; ++i)
            lut[i] = std::sin (float (i) * juce::MathConstants<float>::twoPi
                               / float (kLUTSize));
    });
    return lut.data();
}

const float* HarmonicEngine::getWavetable() const noexcept
{
    return mReadBuf.load (std::memory_order_acquire) == 0
           ? mBufA.data()
           : mBufB.data();
}

//==============================================================================
void HarmonicEngine::buildWavetable()
{
    // Choose the buffer that is NOT currently being read by the audio thread.
    int    readIdx  = mReadBuf.load (std::memory_order_acquire);
    int    buildIdx = 1 - readIdx;
    float* buf      = (buildIdx == 0) ? mBufA.data() : mBufB.data();

    // ── 1. Make working copies so the user's partial data is never modified ──
    std::array<float, kNumPartials> workAmps   = amplitudes;
    std::array<float, kNumPartials> workPhases = phases;

    // ── 2. Apply spectral modules (all except Prism, which acts below) ────────
    // 2026-04-19 (S1) T2-F: rm_fx scales each module's amount before processing.
    // We snapshot each module's amount, apply the scale, run the module, and
    // restore - preserves the user's knob values while honouring the rm_fx
    // routing-matrix slider.
    auto withScaledAmount = [this] (auto& mod, auto&& runFn)
    {
        const float saved = mod.getAmount();
        mod.setAmount (saved * mSpectralFxScale);
        runFn();
        mod.setAmount (saved);
    };
    withScaledAmount (pluck,      [&] { pluck     .processPartials (workAmps.data(), workPhases.data(), kNumPartials); });
    withScaledAmount (blur,       [&] { blur      .processPartials (workAmps.data(), workPhases.data(), kNumPartials); });
    withScaledAmount (filterMask, [&] { filterMask.processPartials (workAmps.data(), workPhases.data(), kNumPartials); });
    withScaledAmount (phaserMask, [&] { phaserMask.processPartials (workAmps.data(), workPhases.data(), kNumPartials); });

    // 2026-04-19 (S1) T2-F: nyquist protect - soft rolloff above kNumPartials/4.
    // mNyquistProtect=0 -> no rolloff; =1 -> aggressive log-roll above corner.
    if (mNyquistProtect > 0.001f)
    {
        const int   corner = kNumPartials / 4;
        const float k      = mNyquistProtect * 0.05f;
        for (int i = corner; i < kNumPartials; ++i)
        {
            const float over = float (i - corner);
            workAmps[i] *= std::exp (-k * over);
        }
    }

    // ── 2a. Auto-gain pass (S4 AG-1) ──────────────────────────────────────────
    // Mode 0 Relative: scale all partials by a common factor so the max
    //                  partial fits inside the per-bin headroom.
    // Mode 1 Absolute: hard-clip each partial at per-bin headroom without
    //                  scaling others.
    // Per-bin headroom = 1 / kNumPartials (conservative: guarantees no
    // post-IFFT overshoot even if every partial sums constructively).
    {
        const float headroom = 1.0f / float (kNumPartials);
        if (mAutoGainMode == 0)
        {
            float peakAmp = 0.0f;
            for (int i = 0; i < kNumPartials; ++i)
                peakAmp = std::max (peakAmp, workAmps[i]);
            if (peakAmp > headroom)
            {
                const float scale = headroom / peakAmp;
                for (int i = 0; i < kNumPartials; ++i)
                    workAmps[i] *= scale;
            }
        }
        else
        {
            for (int i = 0; i < kNumPartials; ++i)
                if (workAmps[i] > headroom)
                    workAmps[i] = headroom;
        }
    }

    // ── 2b. Brownian spectral weighting (1/n^α rolloff) ───────────────────────
    // Mimics the natural harmonic decay of acoustic instruments and brown noise.
    // Applied after spectral modules so module output is weighted, not the raw input.
    if (mBrownianAmount > 0.001f)
    {
        for (int i = 0; i < kNumPartials; ++i)
            workAmps[i] *= std::pow (1.0f / float (i + 1), mBrownianAmount);
    }

    // ── 3. Assemble IFFT input ─────────────────────────────────────────────────
    std::fill (mFFTData.begin(), mFFTData.end(), 0.0f);

    const float* sinLUT = getSineLUT();
    constexpr float lutOverTwoPi =
        float (kLUTSize) / juce::MathConstants<float>::twoPi;

    for (int i = 0; i < kNumPartials; ++i)
    {
        const float amp = workAmps[i];
        if (amp < 1.0e-6f)
            continue;

        const float phase = workPhases[i];

        // Prism: fractional bin offset for inharmonicity.
        // At prism.amount=0 this is 0 and binF is the integer partial index.
        const float binF = float (i + 1) + prism.getFrequencyShift (i);

        if (binF < 1.0f || binF >= float (kFFTSize / 2 - 1))
            continue;

        // sin/cos from LUT with linear interpolation.
        float lutPos = phase * lutOverTwoPi;
        int   li     = int (lutPos) & (kLUTSize - 1);
        float lf     = lutPos - float (int (lutPos));

        float sinVal = sinLUT[li]
                     + lf * (sinLUT[(li + 1)              & (kLUTSize - 1)] - sinLUT[li]);
        float cosVal = sinLUT[(li + kLUTSize / 4)             & (kLUTSize - 1)]
                     + lf * (sinLUT[(li + kLUTSize / 4 + 1) & (kLUTSize - 1)]
                              - sinLUT[(li + kLUTSize / 4)     & (kLUTSize - 1)]);

        // Fractional bin: interpolate energy between floor and ceil bins.
        const int   binLo = int (binF);
        const int   binHi = binLo + 1;
        const float hi    = binF - float (binLo);
        const float lo    = 1.0f - hi;

        // JUCE FFT interleaved layout: d[2k] = Re[k], d[2k+1] = Im[k]
        if (binLo >= 1 && binLo < kFFTSize / 2)
        {
            mFFTData[2 * binLo]     += amp * cosVal * lo;
            mFFTData[2 * binLo + 1] += amp * sinVal * lo;
        }
        if (binHi < kFFTSize / 2)
        {
            mFFTData[2 * binHi]     += amp * cosVal * hi;
            mFFTData[2 * binHi + 1] += amp * sinVal * hi;
        }
    }

    // ── 4. Inverse FFT → time-domain waveform ─────────────────────────────────
    mFFT.performRealOnlyInverseTransform (mFFTData.data());

    // ── 5. Normalise to ±1 peak ────────────────────────────────────────────────
    float peak = 1.0e-6f;
    for (int i = 0; i < kFFTSize; ++i)
    {
        float v = mFFTData[i];
        if (std::isfinite (v)) peak = std::max (peak, std::abs (v));
    }

    const float scale = 1.0f / peak;
    for (int i = 0; i < kFFTSize; ++i)
    {
        float v = mFFTData[i] * scale;
        buf[i]  = std::isfinite (v) ? v : 0.0f;
    }

    // ── 6. Atomic publish ──────────────────────────────────────────────────────
    mReadBuf.store (buildIdx, std::memory_order_release);
    mReady.store   (true,     std::memory_order_release);

    // S4 Option 2: bump generation so voices observing this engine as a
    // template know to re-sync their per-voice copy.
    mGeneration.fetch_add (1, std::memory_order_release);
}

//==============================================================================
// S4 Option 2: copy all state from another engine without touching the
// wavetable buffers / FFT scratch. Caller must invoke buildWavetable() to
// produce a live wavetable from the copied state.
void HarmonicEngine::copyStateFrom (const HarmonicEngine& other) noexcept
{
    amplitudes = other.amplitudes;
    phases     = other.phases;
    detuneHz   = other.detuneHz;

    mBrownianAmount  = other.mBrownianAmount;
    mSpectralFxScale = other.mSpectralFxScale;
    mNyquistProtect  = other.mNyquistProtect;
    mAutoGainMode    = other.mAutoGainMode;

    prism      = other.prism;
    pluck      = other.pluck;
    blur       = other.blur;
    filterMask = other.filterMask;
    phaserMask = other.phaserMask;
}

//==============================================================================
void HarmonicEngine::setShape (Shape s)
{
    amplitudes.fill (0.0f);
    phases.fill     (0.0f);

    switch (s)
    {
    case Shape::Sine:
        amplitudes[0] = 1.0f;
        break;

    case Shape::Saw:
        // Full harmonic series: amplitude = 1/n
        for (int i = 0; i < kNumPartials; ++i)
            amplitudes[i] = 1.0f / float (i + 1);
        break;

    case Shape::Square:
        // Odd harmonics only: 1/n
        for (int i = 0; i < kNumPartials; i += 2)
            amplitudes[i] = 1.0f / float (i + 1);
        break;

    case Shape::Triangle:
        // Odd harmonics: 1/n², alternating phase for smooth peaks
        for (int i = 0; i < kNumPartials; i += 2)
        {
            amplitudes[i] = 1.0f / float ((i + 1) * (i + 1));
            phases[i]     = (i % 4 == 2)
                            ? juce::MathConstants<float>::pi : 0.0f;
        }
        break;
    }

    buildWavetable();
}
