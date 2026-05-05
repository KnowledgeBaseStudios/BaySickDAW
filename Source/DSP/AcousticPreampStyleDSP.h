#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"

// ─────────────────────────────────────────────────────────────────────────────
// AcousticPreampStyleDSP — Phase I-11 (2026-05-03)
// ─────────────────────────────────────────────────────────────────────────────
// AD Style Acoustic Preamp.  Turns a piezo / under-saddle pickup signal into
// a mic'd-acoustic-style tone via a body-resonance convolution + Schroeder
// reverberator + parametric notch (feedback rejection).
//
// Body IR — chickenhead picker:
//   Dreadnought  -- punchy low-mids, body resonance ~100 Hz
//   Parlor       -- brighter, smaller body, resonance ~180 Hz
//   Jumbo        -- deep bass, biggest body, resonance ~80 Hz
//   User         -- user-supplied IR loaded via the panel file picker.
//                   Path persists in save state (absolute or library-relative).
//
// Synthetic IRs (Dreadnought / Parlor / Jumbo) are generated inline — short
// (4096-sample @ 44.1k) decaying sums of damped sinusoids tuned to that body
// type's main resonance peaks (air mode, top mode, body mode).  Drop a real
// .wav into `Resources/IRs/Acoustic/{Dreadnought,Parlor,Jumbo}.wav` and the
// loader will use that instead of the synthetic fallback.
//
// Schroeder ambience: 4 parallel comb filters (delays 1557 / 1617 / 1491 /
// 1422 samples @ 44.1k, decay-tuned to ~0.4s) -> 2 series allpass filters
// (delays 225 / 556 samples).  Tight room character; NOT ReverbDSP's FDN
// topology (that's a hall reverb).
//
// Notch: SVF band-stop, frequency 50 Hz - 1 kHz log-swept, Q fixed at ~10.
//
// Signal flow (locked 2026-05-03 per Jeff's spec):
//   input -> split (dry / wet=Convolution) -> sum -> Schroeder ambience ->
//     Notch (band-stop, last in chain for surgical feedback rejection) ->
//     Level (output gain) -> output
//
// Knobs:
//   Resonance — body IR wet/dry mix (Dry/Wet)
//   Ambience  — Schroeder reverberator wet/dry mix
//   Notch     — band-stop notch frequency (50 Hz - 1 kHz log)
//   Level     — output gain (-24 dB to +12 dB)
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
    void setResonance    (float v01);    // body IR Dry/Wet
    void setAmbience     (float v01);    // Schroeder Dry/Wet
    void setNotchHz      (float hz);     // 50..1000
    void setLevelDb      (float db);     // -24..+12

    // User IR file (Body::User).  Path is stored verbatim in save state.
    // Pass empty path to clear; panel button calls this from the file picker.
    void loadUserIR      (const juce::File& file);
    juce::String getUserIRPath() const { return mUserIRPath; }

    Body  mBody        { Body::Dreadnought };
    float mResonance01 { 0.5f };
    float mAmbience01  { 0.2f };
    float mNotchHz     { 250.0f };
    float mLevelDb     { 0.0f };

private:
    void rebuildBodyIR();
    void buildSyntheticIR (Body body, juce::AudioBuffer<float>& dest, double sr);

    double mSampleRate { 44100.0 };
    int    mMaxBlock   { 0 };

    juce::dsp::Convolution mConv;
    juce::AudioBuffer<float> mDryScratch;   // for body wet/dry blend
    juce::AudioBuffer<float> mAmbScratch;   // Schroeder return scratch

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
    bool         mBodyIRDirty { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcousticPreampStyleDSP)
};
