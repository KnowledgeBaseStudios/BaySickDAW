#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"

// ─────────────────────────────────────────────────────────────────────────────
// AcousticPreampStyleDSP - Phase I-11 (2026-05-03)
// QA-EffectsReview Task 9 (2026-07-02): adaptive Acoustic Resonance rework.
// ─────────────────────────────────────────────────────────────────────────────
// AD Style Acoustic Preamp.  Turns a piezo / under-saddle pickup signal into
// a mic'd-acoustic-style tone.
//
// ACOUSTIC RESONANCE is ADAPTIVE, per the reference pedal's official
// description ("analyzing the input signal as you play", one knob driving
// "multiple interlocked parameters"):
//   - BASE CORRECTION IR: the bundled capture at
//     `Resources/Acoustic IRs/Acoustic Preamp IR.wav` (exe-relative) is the
//     piezo-to-real tonal fingerprint, convolved wet/dry under the Resonance
//     macro for ALL named bodies (the reference pedal has ONE voice -- our
//     Body picker is seasoning on top).  Missing file -> adaptive-only.
//   - block-rate dynamics analysis: fast peak envelope (instant attack,
//     ~80 ms release) + slow level average (~400 ms) -> playing-level +
//     transient control signals
//   - piezo de-quack: a peaking CUT (~2 kHz, up to -7 dB) that deepens over
//     the knob's lower half -- kills the under-saddle mid honk first, which
//     is the reference's observed low-knob behavior
//   - BodyResonanceBank: 3 modal peaking resonators + a broadband per-body
//     voicing pair (low shelf + mid/upper-mid peak) per channel, all from
//     kVoicing[] (Dreadnought warm punch / Parlor small boxy w/ low cut /
//     Jumbo big bloom).  The broadband pair carries the audible body-SIZE
//     difference; the modes season it.  Gain breathes with playing level,
//     pick transients bloom the gain and widen the Q, and the air mode gains
//     extra weight past noon (the reference "adds bass" in the upper half).
//     Coefs refresh at block rate, CPU-guarded.
//   - the Resonance knob is the CEILING of the whole macro (IR wet +
//     adaptive depth together).
//
// Body::User swaps the bundled base IR for a user-supplied capture and
// bypasses the adaptive bank (a user IR is a fixed capture -- static
// wet/dry, locked no-regression).  Convolution IRs load on the message
// thread / prepare only (Tape rule) -- never from process().
//
// Schroeder ambience: 4 parallel comb filters (delays 1557 / 1617 / 1491 /
// 1422 samples @ 44.1k, decay-tuned to ~0.4s) -> 2 series allpass filters
// (delays 225 / 556 samples).  Tight room character; NOT ReverbDSP's FDN
// topology (that's a hall reverb).
//
// Notch: SVF band-stop, 50 Hz - 1 kHz log-swept, Q fixed at ~10.  OFF by
// default and OFF at the panel knob's counterclockwise end -- matches the
// reference face ("normally you should leave this in the OFF position").
//
// Signal flow (2026-07-02):
//   input -> Acoustic Resonance (de-quack + modal bank; User = conv wet/dry)
//     -> Schroeder ambience -> Notch (band-stop, only when on) ->
//     Level (output gain) -> output
//
// Knobs:
//   Resonance - adaptive resonance depth ceiling (User body: IR wet/dry)
//   Ambience  - Schroeder reverberator wet/dry mix
//   Notch     - band-stop frequency, knob bottom = OFF
//   Level     - output gain (-24 dB to +12 dB)
// ─────────────────────────────────────────────────────────────────────────────

class AcousticPreampStyleDSP : public DSPBase
{
public:
    enum class Body : int
    {
        Dreadnought = 0,
        Parlor      = 1,
        Jumbo       = 2,
        User        = 3
    };

    AcousticPreampStyleDSP();
    ~AcousticPreampStyleDSP() override = default;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    void setBody         (int body);     // 0..3
    void setResonance    (float v01);    // adaptive depth ceiling (User: IR mix)
    void setAmbience     (float v01);    // Schroeder Dry/Wet
    void setNotchHz      (float hz);     // 50..1000
    void setNotchOn      (bool on);      // knob bottom = OFF (reference default)
    void setLevelDb      (float db);     // -24..+12

    bool getNotchOn() const noexcept { return mNotchOn; }

    // User IR file (Body::User).  Path is stored verbatim in save state.
    // Pass empty path to clear; panel button calls this from the file picker.
    // Returns false + outErr when the pick is missing or not readable audio,
    // leaving the current IR in place: the convolution stage reports nothing
    // back, so an unchecked failure reads as the effect doing nothing.
    bool loadUserIR      (const juce::File& file, juce::String& outErr);
    juce::String getUserIRPath() const { return mUserIRPath; }

    Body  mBody        { Body::Dreadnought };
    float mResonance01 { 0.5f };
    float mAmbience01  { 0.2f };
    float mNotchHz     { 250.0f };
    bool  mNotchOn     { false };   // reference default = OFF
    float mLevelDb     { 0.0f };

private:
    // Loads the conv IR for the CURRENT body: named bodies get the bundled
    // base correction IR; User gets the user file (identity fallback).
    // Message thread / prepare only -- process() never loads.
    void reloadConvIR();

    double mSampleRate { 44100.0 };
    int    mMaxBlock   { 0 };
    bool   mBaseIRLoaded { false };   // bundled base IR found + loaded

    juce::dsp::Convolution mConv;
    juce::AudioBuffer<float> mAmbScratch;   // Schroeder return scratch
    juce::AudioBuffer<float> mConvScratch;  // User-body conv wet (preallocated)
    juce::AudioBuffer<float> mNotchScratch; // notch bandpass tap (preallocated)

    // ── Acoustic Resonance: adaptive engine ────────────────────────────────
    // 2-pole peaking biquad with independent L/R state (Biquad2P pattern).
    struct StereoBiquad
    {
        float b0{1.f}, b1{0.f}, b2{0.f}, a1{0.f}, a2{0.f};
        float x1L{0.f}, x2L{0.f}, y1L{0.f}, y2L{0.f};
        float x1R{0.f}, x2R{0.f}, y1R{0.f}, y2R{0.f};
        void  reset() { x1L=x2L=y1L=y2L=x1R=x2R=y1R=y2R=0.f; }
        float processL (float x)
        {
            const float y = b0*x + b1*x1L + b2*x2L - a1*y1L - a2*y2L;
            x2L=x1L; x1L=x; y2L=y1L; y1L=y;
            return y;
        }
        float processR (float x)
        {
            const float y = b0*x + b1*x1R + b2*x2R - a1*y1R - a2*y2R;
            x2R=x1R; x1R=x; y2R=y1R; y1R=y;
            return y;
        }
    };
    // RBJ cookbook EQ coefs, written straight into the biquad -- JUCE's
    // coefficient makers heap-allocate, so they must not run on the audio
    // thread.
    static void makePeakCoefs     (StereoBiquad& bq, double sr, float fc, float q, float gainDb);
    static void makeLowShelfCoefs (StereoBiquad& bq, double sr, float fc, float q, float gainDb);

    StereoBiquad mModes[3];      // air / top / body modal resonators
    StereoBiquad mDeQuack;       // piezo mid-cut (~2 kHz)
    StereoBiquad mTilt;          // per-body broadband low shelf
    StereoBiquad mBodyPeak;      // per-body mid/upper-mid voicing peak

    // Last-applied targets: coefs recompute at block rate ONLY on real
    // movement (CPU-guard rule).  mCoefBody -1 forces the first refresh.
    float mAppliedModeDb[3] { 0.0f, 0.0f, 0.0f };
    float mAppliedModeQ [3] { 0.0f, 0.0f, 0.0f };
    float mAppliedQuackDb   { 0.0f };
    float mAppliedTiltDb    { 0.0f };
    float mAppliedPeakDb    { 0.0f };
    int   mCoefBody         { -1 };

    // Block-rate dynamics analysis state (mono-summed peak).
    float mEnvFast { 0.0f };
    float mEnvSlow { 0.0f };

    // Schroeder reverberator: 4 parallel comb -> 2 series allpass.
    struct CombFilter
    {
        std::vector<float> buf;
        int   pos { 0 };
        float feedback { 0.84f };
        float damp1 { 0.0f };
        float damp2 { 0.5f };
        void  prepare (int delaySamples) { buf.assign ((size_t) juce::jmax (1, delaySamples), 0.0f); pos = 0; damp1 = 0.0f; }
        float process (float in)
        {
            float out = buf[(size_t) pos];
            damp1 = out * (1.0f - damp2) + damp1 * damp2;
            buf[(size_t) pos] = in + damp1 * feedback;
            if (++pos >= (int) buf.size()) pos = 0;
            return out;
        }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); damp1 = 0.0f; pos = 0; }
    };
    struct AllpassFilter
    {
        std::vector<float> buf;
        int   pos { 0 };
        float feedback { 0.5f };
        void  prepare (int delaySamples) { buf.assign ((size_t) juce::jmax (1, delaySamples), 0.0f); pos = 0; }
        float process (float in)
        {
            float bufout = buf[(size_t) pos];
            float out = -in + bufout;
            buf[(size_t) pos] = in + bufout * feedback;
            if (++pos >= (int) buf.size()) pos = 0;
            return out;
        }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }
    };
    CombFilter    mCombs[4][2];      // [comb][channel]
    AllpassFilter mAllpasses[2][2];  // [allpass][channel]

    juce::dsp::StateVariableTPTFilter<float> mNotch;

    juce::String mUserIRPath;        // last loaded user IR (if any)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcousticPreampStyleDSP)
};
