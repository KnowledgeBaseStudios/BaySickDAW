#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include <memory>

// ─────────────────────────────────────────────────────────────────────────────
// PitchShifters - CepstralFormantEngine (QA-F Tasks 3 + 5)
// ─────────────────────────────────────────────────────────────────────────────
// The PsolaShifter / GranularShifter / PvShifter trio that once lived here was
// retired in QA-Fe when the vocal editor / Align / real-time correction moved to
// the vendored library engines (LibraryPitchShifters + Rubber Band LiveShifter).
// Only CepstralFormantEngine remains -- the Align offline throat still uses its
// static formantShiftMono (BaySickVocalProcessor).  See the class comment below.
// ─────────────────────────────────────────────────────────────────────────────

// ── CepstralFormantEngine ────────────────────────────────────────────────────
// Shared formant machinery (QA-F Tasks 3 + 5): cepstral spectral-envelope
// extraction -> envelope replacement/scaling, STFT frame-based.
//
//   Formant PRESERVE: impose the DRY (pre-shift) frame's envelope onto the
//   WET (post-shift) frame -- kills the chipmunk/munchkin effect a plain
//   time-domain shifter leaves behind.
//   THROAT SHIFT: scale the target envelope along the frequency axis by
//   2^(semis/12) -- deliberate character change without re-pitching.
//
// Envelope = cepstral liftering (log-magnitude -> IFFT -> keep quefrencies
// below ~1 ms -> FFT back).  The 1 ms cutoff sits under the shortest vocal
// pitch period (~2 ms at a 500 Hz soprano) so harmonics never leak into the
// "envelope".
//
// FFT 512 / hop 128 (75% overlap, Hann analysis + synthesis, (8/3)*Hs/N OLA
// scale -- the PhaseVocoder normalization convention).  Latency = 2*kSize -
// kHop = 896 samples (~20 ms @ 44.1k: one full frame to fire + the OLA
// settle margin) ON TOP of the upstream shifter -- acceptable for an opt-in
// toggle; the default monitor path stays shifter-only (Call 2a).
//
// THREADING: one instance per channel, all calls one thread.  No allocation
// after prepare(); audio-thread safe.
class CepstralFormantEngine
{
public:
    static constexpr int kOrder = 9;
    static constexpr int kSize  = 1 << kOrder;   // 512
    static constexpr int kHop   = kSize / 4;     // 128

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
        if (mFFT == nullptr)
            mFFT = std::make_unique<juce::dsp::FFT> (kOrder);
        for (int i = 0; i < kSize; ++i)
            mWindow[(size_t) i] = 0.5f * (1.0f - std::cos (
                juce::MathConstants<float>::twoPi * (float) i / (float) (kSize - 1)));
        // Quefrency lifter cutoff ~1 ms (see class comment).
        mLifterBins = juce::jlimit (8, kSize / 4, (int) std::round (mSampleRate * 0.001));
        reset();
    }

    void reset()
    {
        mDryRing.fill (0.0f);
        mWetRing.fill (0.0f);
        mOla.fill (0.0f);
        mInAbs = 0;
        mOutReadAbs = 0;
        mOlaWriteAbs = 0;
    }

    // Streaming pair input: dry = pre-shift sample, wet = post-shift sample.
    // Returns the formant-corrected wet stream, 2*kSize-kHop samples late.
    // Caller engages this only while (preserve || throatSemis != 0) and
    // resets on the engage edge (latency appears/disappears with the mode).
    float processSample (float dry, float wet, bool preserve, float throatSemis) noexcept
    {
        mDryRing[(size_t) (mInAbs % kSize)] = dry;
        mWetRing[(size_t) (mInAbs % kSize)] = wet;
        ++mInAbs;

        if (mInAbs >= (juce::int64) kSize && (mInAbs % kHop) == 0)
            processFrame (preserve, throatSemis);

        // Deliver only fully-settled OLA slots (every overlapping frame has
        // contributed) -- the PhaseVocoder getOutputAvailable convention.
        // Stalling during warmup IS the latency.
        float out = 0.0f;
        const juce::int64 settled = mOlaWriteAbs - (juce::int64) (kSize - kHop);
        if (mOutReadAbs < settled)
        {
            const size_t idx = (size_t) (mOutReadAbs % (juce::int64) mOla.size());
            out = mOla[idx];
            mOla[idx] = 0.0f;   // clear-after-read: future OLA lands on silence
            ++mOutReadAbs;
        }
        return out;
    }

    // Offline convenience (Align bake): impose the buffer's own envelope
    // scaled by throatSemis (formant character shift, pitch untouched).
    // Latency-compensated.  Allocates; message/worker thread only.
    static void formantShiftMono (float* buf, int n, double sampleRate, float throatSemis)
    {
        if (n <= 0 || std::abs (throatSemis) < 0.01f) return;
        CepstralFormantEngine eng;
        eng.prepare (sampleRate, 0);
        const int lat = 2 * kSize - kHop;
        std::vector<float> out ((size_t) n, 0.0f);
        for (int i = 0; i < n + lat; ++i)
        {
            const float in = (i < n) ? buf[i] : 0.0f;
            const float v  = eng.processSample (in, in, false, throatSemis);
            const int   j  = i - lat;
            if (j >= 0 && j < n) out[(size_t) j] = v;
        }
        std::copy (out.begin(), out.end(), buf);
    }

private:
    void processFrame (bool preserve, float throatSemis) noexcept
    {
        // Windowed frames end at the current input head.
        const juce::int64 frameStart = mInAbs - kSize;
        for (int i = 0; i < kSize; ++i)
        {
            const size_t src = (size_t) ((frameStart + i) % kSize);
            mFrameWet[(size_t) i] = { mWetRing[src] * mWindow[(size_t) i], 0.0f };
            if (preserve)
                mFrameDry[(size_t) i] = { mDryRing[src] * mWindow[(size_t) i], 0.0f };
        }
        mFFT->perform (mFrameWet.data(), mSpecWet.data(), false);
        if (preserve)
            mFFT->perform (mFrameDry.data(), mSpecDry.data(), false);

        // Log-magnitude envelopes via cepstral lifter.
        computeEnvelope (mSpecWet, mEnvWet);
        if (preserve)
            computeEnvelope (mSpecDry, mEnvDry);

        // Target envelope: dry's (preserve) or wet's own (throat-only),
        // frequency-scaled by the throat ratio.
        const float scale = std::pow (2.0f, -throatSemis / 12.0f);   // sample source bin = k*scale
        auto& srcEnv = preserve ? mEnvDry : mEnvWet;
        for (int k = 0; k <= kSize / 2; ++k)
        {
            const float pos = (float) k * scale;
            const int   k0  = (int) pos;
            float target;
            if (k0 + 1 <= kSize / 2)
            {
                const float fr = pos - (float) k0;
                target = srcEnv[(size_t) k0] * (1.0f - fr) + srcEnv[(size_t) k0 + 1] * fr;
            }
            else
                target = srcEnv[(size_t) (kSize / 2)];

            // Correction in the log domain, clamped +/-24 dB per bin so a
            // near-silent envelope can't blow the frame up.
            const float corr = juce::jlimit (-2.7631f, 2.7631f, target - mEnvWet[(size_t) k]);
            const float g    = std::exp (corr);
            mSpecWet[(size_t) k] *= g;
            if (k > 0 && k < kSize / 2)
                mSpecWet[(size_t) (kSize - k)] = std::conj (mSpecWet[(size_t) k]);
        }

        mFFT->perform (mSpecWet.data(), mFrameWet.data(), true);

        // Hann synthesis window + (8/3)*Hs/N OLA normalization (PhaseVocoder
        // convention: window applied twice, 75% overlap).
        const float olaScale = (8.0f / 3.0f) * (float) kHop / (float) kSize;
        const int olaSize = (int) mOla.size();
        for (int i = 0; i < kSize; ++i)
        {
            const size_t idx = (size_t) ((mOlaWriteAbs + i) % (juce::int64) olaSize);
            mOla[idx] += mFrameWet[(size_t) i].real() * mWindow[(size_t) i] * olaScale;
        }
        mOlaWriteAbs += kHop;
    }

    void computeEnvelope (const std::array<juce::dsp::Complex<float>, kSize>& spec,
                          std::array<float, kSize / 2 + 1>& env) noexcept
    {
        for (int k = 0; k < kSize; ++k)
        {
            const float m = std::abs (spec[(size_t) k]);
            mCepIn[(size_t) k] = { std::log (juce::jmax (m, 1.0e-9f)), 0.0f };
        }
        mFFT->perform (mCepIn.data(), mCepOut.data(), true);   // -> quefrency
        for (int q = 0; q < kSize; ++q)
        {
            const bool keep = (q <= mLifterBins) || (q >= kSize - mLifterBins);
            if (! keep) mCepOut[(size_t) q] = { 0.0f, 0.0f };
        }
        mFFT->perform (mCepOut.data(), mCepIn.data(), false);  // -> log-spectral envelope
        for (int k = 0; k <= kSize / 2; ++k)
            env[(size_t) k] = mCepIn[(size_t) k].real();
    }

    std::unique_ptr<juce::dsp::FFT> mFFT;
    std::array<float, kSize>  mWindow {};
    std::array<float, kSize>  mDryRing {};
    std::array<float, kSize>  mWetRing {};
    // 3x frame size: peak occupancy is write-frontier + kSize minus the
    // settle-lagged read head = exactly 2*kSize -- 3x leaves real margin.
    std::array<float, kSize * 3> mOla {};
    std::array<juce::dsp::Complex<float>, kSize> mFrameWet {}, mFrameDry {};
    std::array<juce::dsp::Complex<float>, kSize> mSpecWet {},  mSpecDry {};
    std::array<juce::dsp::Complex<float>, kSize> mCepIn {},    mCepOut {};
    std::array<float, kSize / 2 + 1> mEnvWet {}, mEnvDry {};
    juce::int64 mInAbs       { 0 };
    juce::int64 mOutReadAbs  { 0 };
    juce::int64 mOlaWriteAbs { 0 };
    int         mLifterBins  { 44 };
    double      mSampleRate  { 44100.0 };
};
