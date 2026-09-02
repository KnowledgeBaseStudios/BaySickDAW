#pragma once
#include <JuceHeader.h>
#include "SpectralModules.h"
#include <array>
#include <atomic>

// ── HarmonicEngine ────────────────────────────────────────────────────────────
// Stores up to kNumPartials harmonic partials (amplitude + phase).
// buildWavetable() runs a 2048-point IFFT to produce a single-cycle waveform.
//
// Thread safety:
//   Double-buffered - buildWavetable() writes into the inactive buffer then
//   atomically publishes it. The audio thread always reads a complete,
//   consistent snapshot via getWavetable().
//
// CPU note:
//   buildWavetable() takes ~10–20µs (2048-pt IFFT). Guard calls with a
//   value-change check so it is not invoked every processBlock.
// ─────────────────────────────────────────────────────────────────────────────
class HarmonicEngine
{
public:
    static constexpr int kNumPartials = 516;
    static constexpr int kFFTOrder    = 11;           // 2^11 = 2048 points
    static constexpr int kFFTSize     = 1 << kFFTOrder;
    static constexpr int kLUTSize     = 4096;         // sine look-up table size

    HarmonicEngine();

    // ── Partial data ──────────────────────────────────────────────────────────
    // amplitudes[i]: linear amplitude of partial (i+1); 0 = silence
    // phases[i]:     initial phase in [0, 2π)
    // detuneHz[i]:   per-partial pitch offset (reserved for future use)
    std::array<float, kNumPartials> amplitudes {};
    std::array<float, kNumPartials> phases     {};
    std::array<float, kNumPartials> detuneHz   {};

    // ── Brownian spectral weighting ────────────────────────────────────────────
    // Applies a 1/n^α rolloff to partial amplitudes before the IFFT.
    // 1.0 = natural brown-noise rolloff (~6 dB/octave); 0.0 = flat (no weighting).
    // Default 1.0 so the synth sounds organic out of the box.
    float mBrownianAmount { 1.0f };
    void  setBrownianAmount (float a) noexcept { mBrownianAmount = a; }

    // 2026-04-19 (S1) T2-F routing-matrix hooks.
    // mSpectralFxScale (rm_fx) multiplies the spectral-FX module amounts at
    // build time without disturbing the user-set knob values. 0 = bypass FX
    // entirely; 1 = full FX (default).
    float mSpectralFxScale { 1.0f };
    void  setSpectralFxScale (float s) noexcept
        { mSpectralFxScale = juce::jlimit (0.f, 1.f, s); }
    // mNyquistProtect (rm_prot) rolls off high partials with a soft curve.
    // 0 = no rolloff; 1 = aggressive rolloff above partial ~kNumPartials/4.
    float mNyquistProtect { 0.0f };
    void  setNyquistProtect (float p) noexcept
        { mNyquistProtect = juce::jlimit (0.f, 1.f, p); }

    // 2026-04-19 (S4 AG-1) Auto-gain mode (per Harmor Advanced Timbre Section).
    //   0 = Relative: when summed partial peak exceeds 1.0, scale all partials
    //       equally to fit. Preserves spectral balance, drops overall level.
    //   1 = Absolute: each partial hard-capped at its own per-bin ceiling
    //       (1 / kNumPartials). Preserves per-partial level, alters balance
    //       at extremes but guarantees no post-IFFT clipping.
    int  mAutoGainMode { 0 };
    void setAutoGainMode (int mode) noexcept
        { mAutoGainMode = juce::jlimit (0, 1, mode); }

    // ── Preset shapes ─────────────────────────────────────────────────────────
    enum class Shape { Sine, Saw, Square, Triangle };

    // Sets amplitudes/phases to a classic harmonic series and rebuilds.
    void setShape (Shape s);

    // ── Spectral modules ──────────────────────────────────────────────────────
    // Applied (in order) to working copies of amplitudes/phases before the IFFT.
    // Set amounts via the public module objects; call buildWavetable() to apply.
    PrismModule      prism;
    PluckModule      pluck;
    BlurModule       blur;
    FilterMaskModule filterMask;
    PhaserMaskModule phaserMask;

    // ── Wavetable generation ──────────────────────────────────────────────────
    // Call off audio-thread (or infrequently with CPU guard).
    // Builds from current amplitudes/phases; atomically swaps the read buffer.
    void buildWavetable();

    // ── Audio-thread read interface ────────────────────────────────────────────
    // Safe to call from renderNextBlock - returns pointer to the last fully
    // built buffer; never null after the first buildWavetable() call.
    const float* getWavetable()   const noexcept;
    bool          wavetableReady() const noexcept
        { return mReady.load(std::memory_order_acquire); }

    // 2026-04-20 (S4 Option 2) Per-voice-engine support.
    //
    // Generation counter - bumped every time buildWavetable() produces a new
    // wavetable. Voices cache the template's generation; when it changes,
    // they re-copy template state, re-apply their per-voice mods, and rebuild
    // their private wavetable. Atomic so the audio thread can observe
    // message-thread writes safely.
    juce::uint64 getGeneration() const noexcept
        { return mGeneration.load (std::memory_order_acquire); }

    // Copies all partial data + module state from `other`. Does NOT copy the
    // wavetable buffers - caller must invoke buildWavetable() afterward. Used
    // by AdditiveVoice to sync its per-voice engine to the shared template
    // before applying its own modulation deltas.
    void copyStateFrom (const HarmonicEngine& other) noexcept;

    // 2026-04-20 (S5 T2-P) Background rebuild: instead of calling
    // buildWavetable() synchronously from the audio thread (which holds the
    // ~15 us IFFT on a real-time deadline), UI-initiated code paths set this
    // atomic flag and the BaySickSolsticeSynth background TimeSliceThread polls it
    // and calls buildWavetable() off-thread. Voice-mod rebuilds (Option A
    // scope) stay synchronous on the audio thread since they're sparse and
    // need immediate audibility.
    void requestRebuild() noexcept
        { mRebuildRequested.store (true, std::memory_order_release); }

    // True if a rebuild is pending but not yet completed. Background thread
    // checks this + calls buildWavetable() if set, then clears.
    bool consumeRebuildRequest() noexcept
    {
        bool expected = true;
        return mRebuildRequested.compare_exchange_strong (
            expected, false, std::memory_order_acq_rel);
    }

private:
    // Double-buffered wavetable (one for reading, one for building)
    std::array<float, kFFTSize> mBufA {};
    std::array<float, kFFTSize> mBufB {};
    std::atomic<int>  mReadBuf { 0 };   // 0 = read from A, 1 = read from B
    std::atomic<bool> mReady   { false };

    // S4 Option 2: generation bumps each buildWavetable(). Voices poll this to
    // know when the template has been re-baked by a UI change and their
    // per-voice copy is stale.
    std::atomic<juce::uint64> mGeneration { 0 };

    // S5 T2-P: set by requestRebuild(), consumed by consumeRebuildRequest()
    // on the background thread. Ensures UI-initiated rebuilds don't block
    // the audio thread.
    std::atomic<bool> mRebuildRequested { false };

    // Scratch buffer for IFFT - layout: [Re0,Im0, Re1,Im1, ... Re(N-1),Im(N-1)]
    std::array<float, kFFTSize * 2> mFFTData {};

    // juce::dsp::FFT is NOT default-constructible; must be in the init list.
    juce::dsp::FFT mFFT;

    // Sine LUT - initialised once via std::call_once, shared across instances.
    static const float* getSineLUT() noexcept;
};
