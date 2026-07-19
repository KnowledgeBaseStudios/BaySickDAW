#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include "PitchTrackerYIN.h"
#include "PolyPitchTracker.h"
#include <array>

// ─────────────────────────────────────────────────────────────────────────────
// OctaveStyleDSP - Phase I-7 (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// OC Style Octave pedal.  Adds parallel pitch-shifted copies (+1 oct, -1 oct,
// -2 oct) on top of the dry signal.  Two modes:
//
//   Polyphonic -- hybrid per the 2026-06-18 octave-engine research: the -1/-2
//                 oct paths are a pitch-synchronous PSOLA period-doubler on
//                 confident single-note input (grain seams anchored to Schmitt
//                 pitch marks validated by YIN), hysteresis-SWITCHED (short
//                 fade, never a standing blend) to a granular fallback on
//                 chords / unvoiced input.  The +1 oct path and the fallback
//                 are the granular shifter: two crossfaded read heads in a
//                 ring, played at pitch-ratio speed, Hann-windowed grains.
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

    // PDC (QA-OctavePedal): the Polyphonic pitch-shift path is inherently
    // latent (doubler ~1-2 pitch periods; granular ~half its period-synced
    // grain).  Report a STABLE representative figure -- a per-block-varying
    // value would thrash the host's delay-comp (a click each poll).  ~512 smp
    // = half the ~1024-smp period-synced grain; SR-independent (the grain is
    // sample-defined) so it never changes underfoot.  Vintage's Schmitt divider
    // is sample-instantaneous -> 0.  The board pull-sum + the 5 Hz PDC solve
    // carry this into the Inst-strip compensation (mode flips picked up free).
    static constexpr int kPolyLatencySamples = 512;
    int getLatencySamples() const override
    {
        return mMode == Mode::Polyphonic ? kPolyLatencySamples : 0;
    }

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

    // ── Pitch-synchronous octave-down shifter (Polyphonic mode) ──────────────
    // QA-OctavePedal rework.  Plays each inter-mark segment at 1/octaveDiv
    // speed (sub-sample interpolated read -- every harmonic halves/quarters, a
    // genuine full-spectrum octave-down even on stationary tones), then hops
    // forward octaveDiv-1 marks at the seam so consumption stays balanced with
    // real time.  The pre-rework build replayed each period back-to-back at
    // 1x speed, which is an IDENTITY on a stationary tone (replaying period A
    // equals playing period B when B ~= A) -- its only "octave" content was
    // period-to-period difference + seam discontinuity, i.e. the broken bell.
    //
    // Grain boundaries sit on Schmitt-crossing pitch marks (sub-sample
    // refined, YIN-validated, pushed by the owner via pushMark()), so every
    // seam joins matched waveform phase; segment lengths are fractional mark
    // distances -- no integer-period quantization beat.  Seam crossfades are
    // period-proportional.  Lag corrections happen only AT seams as one mark
    // more / fewer to hop (one period per step) through the normal crossfade
    // -- never a hard readPos snap.
    //
    // Coordinates are UNWRAPPED sample counts in doubles; interp() maps into
    // the ring with a single O(1) fmod per call, so per-sample cost is bounded
    // (no wrap loops -- the climbing-CPU hazard is the loop pattern, not the
    // magnitude) and the 53-bit mantissa keeps sub-sample precision for years
    // of continuous audio.  Latency ~ one-to-two pitch periods.
    struct PeriodDoubler
    {
        static constexpr int    kRingSize  = 16384;  // lag reaches ~4 periods mid-cycle at octaveDiv 4 + correction headroom at kMaxPeriod
        static constexpr int    kMarkQueue = 128;    // spans max lag + one max block even at kMinPeriod mark spacing
        static constexpr int    kXfadeMin  = 4;
        static constexpr int    kXfadeMax  = 64;
        static constexpr double kMinPeriod = 24.0;   // ~1.8 kHz @ 44.1 k -- above this the doubler is ineligible
        static constexpr double kMaxPeriod = 2048.0; // ~23 Hz @ 48 k (YIN's analysis window is the real low bound)

        std::array<float,  kRingSize>  ring  {};
        std::array<double, kMarkQueue> marks {};   // ascending unwrapped positions
        int    markCount   { 0 };
        int    writeIdx    { 0 };
        double writeAbs    { 0.0 };
        double readAbs     { 0.0 };
        double readRate    { 0.5 };     // 1 / octaveDiv; exact binary fractions, no drift
        double xfadeOldAbs { 0.0 };
        int    xfadeLeft   { 0 };
        int    xfadeLen    { kXfadeMin };
        double period      { 256.0 };   // YIN-tracked period: seam-fade length, starvation synthesis, lag bounds
        double segStart    { 0.0 };
        double segEnd      { 0.0 };
        int    octaveDiv   { 2 };       // 2 = -1 oct, 4 = -2 oct
        bool   primed      { false };

        void reset();
        void setOctaveDivisor (int d)    { octaveDiv = juce::jlimit (2, 4, d); readRate = 1.0 / (double) octaveDiv; }
        void setTrackedPeriod (double p) { period = juce::jlimit (kMinPeriod, kMaxPeriod, p); }
        void pushMark (double absPos);
        void process (const float* input, float* output, int numSamples);

    private:
        float interp (double absPos) const noexcept;
        void  beginSeam (double target);
        bool  markAfter (double x, int k, double& out) const noexcept;
        void  purgeMarksBelow (double x) noexcept;
    };

    // 2 channels x 2 down-octave paths ([-1 oct, -2 oct]).
    std::array<std::array<PeriodDoubler, 2>, 2> mDoublers;
    enum { kDdMinusOne = 0, kDdMinusTwo = 1 };

    PitchTrackerYIN  mYin;    // mono: drives the doubler (single-note octave-down)
    PolyPitchTracker mPoly;   // QA-OctavePedal #12: chord detection -> granular grain sizing (no longer mono-driven)
    float  mDoublerMix    { 0.0f };    // 0 = granular, 1 = doubler; hysteresis-switched, short fade between
    bool   mDoublerOn     { false };
    double mTrackedPeriod { 256.0 };   // last YIN period accepted into doubler range (samples, fractional)

    // Pitch-mark detector state (Polyphonic doubler).  Same Schmitt thresholds
    // as the Vintage divider -- the shared crossing detector is the mark source
    // per the 2026-06-18 research -- but separate state so the two modes never
    // entangle across a mode switch.
    bool   mMarkSchmittHi { false };
    float  mMarkPrevMono  { 0.0f };
    double mLastMarkAbs   { -1.0e12 };   // sentinel: first crossing always accepted; synth-fill waits for a real mark
    double mMarkClockAbs  { 0.0 };       // unwrapped write-side clock, lockstep with every doubler's writeAbs

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
    juce::AudioBuffer<float> mMonoScratch;   // L+R average fed to both pitch trackers + the mark detector
    juce::AudioBuffer<float> mShiftedBuf[3];   // [+1, -1, -2]
    // QA-EffectsReview Task 5 (C2): doubler output for the -1/-2 paths, crossfaded
    // against the granular shifter by pitch confidence (PSOLA on confident single
    // notes, granular on chords / unvoiced).
    juce::AudioBuffer<float> mDoublerScratch[2];   // [-1, -2]

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OctaveStyleDSP)
};
