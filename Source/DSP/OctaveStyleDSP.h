#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include "PitchTrackerYIN.h"
#include <array>

// ─────────────────────────────────────────────────────────────────────────────
// OctaveStyleDSP - Phase I-7 (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// OC Style Octave pedal.  Adds parallel pitch-shifted copies (+1 oct, -1 oct,
// -2 oct) on top of the dry signal.  Two modes:
//
//   Polyphonic -- granular pitch shifter per octave path.  Two crossfaded
//                 read heads in a ring buffer, played back at pitch-ratio
//                 speed (2.0 / 0.5 / 0.25 respectively).  Hann-windowed
//                 50 ms grains; produces clean shifts on monophonic bass at
//                 the cost of mild "warble" character on dense polyphony.
//                 Reuses the same windowed-grain spectral approach as our
//                 PhaseVocoder / BaySickPitch chain (locked spec wording);
//                 inline implementation here keeps the OC-Style module self-
//                 contained without touching the existing PV class.
//
//   Vintage    -- low-CPU mono path:
//                   +1 oct  = full-wave rectifier (4x oversampled abs(x))
//                              -- the classic Octavia upper-octave artefact.
//                   -1 oct  = Schmitt-trigger zero-crossing divide-by-2.
//                   -2 oct  = divide-by-4.
//                 Trades clarity for character; the squelchy synthy quality
//                 of vintage octave pedals.
//
//   Range      -- input LP cutoff (300 Hz - 3 kHz log-mapped) feeding the
//                 pitch-shift paths.  Limits the analyser's working band so
//                 the fundamental dominates and high-harmonic chatter
//                 doesn't trick the granular shifter into octave errors.
//
// UI knobs: Direct Level / +1 Oct / -1 Oct / -2 Oct / Range.
// Mode chickenhead: Polyphonic / Vintage.
// ─────────────────────────────────────────────────────────────────────────────

class OctaveStyleDSP : public DSPBase
{
public:
    enum class Mode : int
    {
        Polyphonic = 0,
        Vintage    = 1
    };

    OctaveStyleDSP() = default;
    ~OctaveStyleDSP() override = default;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    void setMode         (int  m);
    void setDirectLevel  (float v01);
    void setOct1Up       (float v01);
    void setOct1Down     (float v01);
    void setOct2Down     (float v01);
    void setRange        (float v01);   // 0..1 -> 300..3000 Hz log

    Mode  mMode        { Mode::Polyphonic };
    float mDirectLevel { 1.0f };
    float mOct1Up      { 0.0f };
    float mOct1Down    { 0.5f };
    float mOct2Down    { 0.0f };
    float mRange       { 0.6f };

private:
    void updateRangeCoefs();

    // ── Granular pitch shifter (Polyphonic mode) ─────────────────────────────
    // One instance per octave path.  Two read heads in a ring buffer, each
    // moving at pitchRatio * 1 sample per output sample; offset by grainSize/2
    // and crossfaded with Hann windows so a continuous output stream comes
    // out.  When a read head approaches the write head (within a grain) it
    // jumps back by grainSize, hidden by the other head's window.
    struct GranularShifter
    {
        static constexpr int kBufferSize = 8192;   // ~186 ms @ 44.1 k

        std::array<float, kBufferSize> ring {};
        int    writePos   { 0 };
        double readPos1   { 0.0 };
        double readPos2   { 1024.0 };
        float  pitchRatio { 1.0f };
        int    grainSize  { 2048 };   // QA-EffectsReview Task 5 (C2): runtime, period-synced from YIN

        void reset();
        void setPitchRatio (float r) { pitchRatio = r; }
        void setGrainSize  (int g)   { grainSize = juce::jlimit (256, kBufferSize / 2, g); }

        // Process numSamples in-place: input[] writes into the ring; output[]
        // receives the pitch-shifted samples.  input and output may alias.
        void process (const float* input, float* output, int numSamples);

    private:
        // Hann window value at phase [0..1].  Cached lookup keeps process()
        // branch-free.
        static float hann (float phase01) noexcept
        {
            return 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * phase01));
        }

        // Linear interpolation of the ring buffer at fractional sample
        // position.  Wraps around buffer edges.
        float interp (double pos) const noexcept;
    };

    // 2 channels x 3 octaves of granular shifters (mono per channel; we
    // shift L and R independently so stereo width is preserved).
    std::array<std::array<GranularShifter, 3>, 2> mShifters;
    enum { kPlusOne = 0, kMinusOne = 1, kMinusTwo = 2 };

    // ── PSOLA period-doubler (Polyphonic mode, octave-DOWN -- QA-EffectsReview T5) ─
    // Re-emits each detected pitch period N times (N=2 for -1 oct, N=4 for -2 oct),
    // crossfading at the period-aligned seams so the down-octave is tight + crisp
    // (vs the granular shifter's warble).  Driven by PitchTrackerYIN's published
    // period; on low pitch confidence the doubler output is faded out (it's a
    // single-note engine -- the granular fallback for chords lands in the next
    // sub-step).  Latency ~ one pitch period (well inside the guitar threshold).
    struct PeriodDoubler
    {
        static constexpr int kRingSize  = 8192;
        static constexpr int kXfade     = 64;     // seam crossfade (samples)
        static constexpr int kMinPeriod = 24;     // ~1.8 kHz @ 44.1 k
        static constexpr int kMaxPeriod = 1024;   // ~43 Hz @ 44.1 k

        std::array<float, kRingSize> ring {};
        int    writePos        { 0 };
        double readPos         { 0.0 };
        double xfadeOldPos     { 0.0 };
        int    xfadeLeft       { 0 };
        int    period          { 256 };
        int    pendingPeriod   { 256 };
        int    grainCount      { 0 };
        int    repeat          { 0 };
        int    repeatsPerGrain { 2 };   // 2 = -1 oct, 4 = -2 oct

        void reset();
        void setRepeats (int r)       { repeatsPerGrain = juce::jlimit (2, 4, r); }
        void setPendingPeriod (int p) { pendingPeriod   = juce::jlimit (kMinPeriod, kMaxPeriod, p); }
        void process (const float* input, float* output, int numSamples);

    private:
        float interp (double pos) const noexcept;
    };

    // 2 channels x 2 down-octave paths ([-1 oct, -2 oct]).
    std::array<std::array<PeriodDoubler, 2>, 2> mDoublers;
    enum { kDdMinusOne = 0, kDdMinusTwo = 1 };

    PitchTrackerYIN mYin;
    float mConf { 0.0f };   // smoothed pitch confidence (drives the doubler fade)

    // QA-EffectsReview Task 5 (C3): POG-style voicing on the shifted streams -- a
    // transient duck (let the dry attack lead, fade the shifted voices in behind it)
    // + a gentle low-pass to mask residual pitch-shift smear.
    float mFastEnv { 0.0f }, mSlowEnv { 0.0f };
    float mDuck    { 1.0f };
    float mFastAtkCoef { 0.0f }, mFastRelCoef { 0.0f }, mSlowCoef { 0.0f };
    float mDuckRecoverCoef { 0.0f };
    float mVoiceLpCoef { 0.0f };
    float mVoiceLpState[3][2] { { 0.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f } };

    // ── Vintage mode state ───────────────────────────────────────────────────
    // Schmitt trigger flag + counters for divide-by-2 and divide-by-4.  We
    // run from a mono mix (L+R)/2 and broadcast the result back to stereo.
    bool   mSchmittHi          { false };   // current Schmitt high/low state
    int    mDivBy2Counter      { 0 };
    int    mDivBy4Counter      { 0 };
    float  mDivBy2OutSign      { 1.0f };
    float  mDivBy4OutSign      { 1.0f };
    static constexpr float kSchmittHi =  0.05f;
    static constexpr float kSchmittLo = -0.05f;

    // Vintage +1 oct: full-wave rectifier with a 5 Hz DC blocker per channel.
    float mFwrDcXL { 0.0f }, mFwrDcYL { 0.0f };
    float mFwrDcXR { 0.0f }, mFwrDcYR { 0.0f };

    // ── Range LP filter (input to pitch-shift paths only) ────────────────────
    using StereoLP = juce::dsp::ProcessorDuplicator<
                        juce::dsp::IIR::Filter<float>,
                        juce::dsp::IIR::Coefficients<float>>;
    StereoLP mRangeLpf;

    // Output 5 Hz DC blocker (granular shifters can leak DC at low pitch
    // ratios; full-wave rectifier always does).
    float mDcXL { 0.0f }, mDcYL { 0.0f };
    float mDcXR { 0.0f }, mDcYR { 0.0f };
    float mDcCoef { 0.0f };

    // Scratch buffers: filtered input + per-octave-path output sums.
    juce::AudioBuffer<float> mFilteredBuf;
    juce::AudioBuffer<float> mShiftedBuf[3];   // [+1, -1, -2]
    // QA-EffectsReview Task 5 (C2): doubler output for the -1/-2 paths, crossfaded
    // against the granular shifter by pitch confidence (PSOLA on confident single
    // notes, granular on chords / unvoiced).
    juce::AudioBuffer<float> mDoublerScratch[2];   // [-1, -2]

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OctaveStyleDSP)
};
