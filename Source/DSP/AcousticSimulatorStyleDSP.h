#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"

// ─────────────────────────────────────────────────────────────────────────────
// AcousticSimulatorStyleDSP - Phase I-11 (2026-05-03)
// ─────────────────────────────────────────────────────────────────────────────
// AC Style Acoustic Simulator.  Turns an electric guitar pickup signal into
// an acoustic-style tone via corrective EQ + transient shaping (NO impulse
// response).  Distinct from AD Style which uses convolution.
//
// Modes (chickenhead, 5 options):
//   Standard - scooped low-mids, flat highs (general acoustic emulation)
//   Jumbo    - massive low-end shelf boost, slightly scooped mids
//   Enhanced - high-shelf boost (hi-fi, mix-ready tone)
//   Piezo    - pronounced upper-mid peak (~2-3 kHz) (under-saddle quack)
//   User     - user-loaded IR replaces the body modeler stage (hybrid mode;
//              transient shaper + reverb + level still run).  Body knob
//              becomes IR wet/dry mix in this mode.
//
// Signal flow:
//   input -> Pickup De-Emphasis (static mid-scoop @ 2.5 kHz to flatten
//            electric pickup resonance) ->
//     Transient Shaper (envelope-follower-driven HF shelf, "Top" knob ±15 dB,
//                       acts on >4 kHz attack content) ->
//     Body Modeler (parallel IIR bank tuned per Mode; "Body" knob ±15 dB
//                   scales curve depth.  In User mode, Convolution replaces
//                   the bank and Body knob becomes wet/dry) ->
//     Schroeder Reverb (4 parallel comb + 2 series allpass; "Reverb" knob
//                       0..1; OWN state, NOT ReverbDSP's FDN) ->
//     Level (output gain, -24 dB to +12 dB) -> output
// ─────────────────────────────────────────────────────────────────────────────

class AcousticSimulatorStyleDSP : public DSPBase
{
public:
    enum class Mode : int
    {
        Standard = 0,
        Jumbo    = 1,
        Enhanced = 2,
        Piezo    = 3,
        User     = 4
    };

    AcousticSimulatorStyleDSP();
    ~AcousticSimulatorStyleDSP() override = default;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    void setMode      (int   m);             // 0..4
    void setTopDb     (float db);            // -15..+15 (transient HF amount)
    void setBodyDb    (float db);            // -15..+15 (body curve depth, OR User-mode IR mix)
    void setReverb    (float v01);           // Schroeder wet/dry
    void setLevelDb   (float db);            // -24..+12

    void loadUserIR   (const juce::File& file);
    juce::String getUserIRPath() const { return mUserIRPath; }

    Mode  mMode      { Mode::Standard };
    float mTopDb     { 0.0f };
    float mBodyDb    { 0.0f };
    float mReverb01  { 0.2f };
    float mLevelDb   { 0.0f };

private:
    void rebuildBodyEQ();
    void rebuildPickupEQ();
    void rebuildTopShelf();
    // Loads the conv IR for the CURRENT mode: named modes get the bundled
    // acoustic body capture (`Resources/Acoustic IRs/Acoustic Simulator
    // IR.wav`, exe-relative); User gets the user file (identity fallback).
    // Message thread / prepare only -- process() never loads (Tape rule).
    void reloadConvIR();

    double mSampleRate { 44100.0 };
    int    mMaxBlock   { 0 };
    bool   mSimIRLoaded { false };   // bundled body IR found + loaded

    using IIRDup = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                    juce::dsp::IIR::Coefficients<float>>;

    // Pickup de-emphasis: fixed peaking biquad ~2.5 kHz, -4 dB, Q ~0.7.
    IIRDup mPickupEQ;

    // Body modeler: 3 parallel-stage filters per Mode.  Each stage = 1 IIR.
    // Mode-specific topology:
    //   Standard: low-shelf cut + high-shelf flat (cut depth scales with Body knob)
    //   Jumbo:    low-shelf boost + low-mid cut
    //   Enhanced: high-shelf boost
    //   Piezo:    upper-mid peak
    IIRDup mBodyA;
    IIRDup mBodyB;

    // Top transient shaper:
    //   - envelope follower (per-sample peak tracker)
    //   - high-shelf coefficients refreshed when Top knob moves
    IIRDup mTopShelf;

    // Convolution: bundled acoustic body IR for the named modes (Task 9 --
    // the shared "acoustic-ness" base under the per-mode voicing curves), or
    // the user IR in Mode::User.
    juce::dsp::Convolution mConv;
    juce::String           mUserIRPath;

    // Transient envelope state per channel.
    float mEnvAttackCoef  { 0.0f };
    float mEnvReleaseCoef { 0.0f };
    float mEnv[2] { 0.0f, 0.0f };
    float mEnvAvg { 0.0f }; // smoothed long-term average to detect transients

    juce::AudioBuffer<float> mShelfScratch;
    juce::AudioBuffer<float> mRevScratch;
    juce::AudioBuffer<float> mConvScratch;   // conv wet (preallocated -- no audio-thread alloc)

    // Schroeder reverberator (own instance, separate from AD Style's).
    struct CombFilter
    {
        std::vector<float> buf;
        int   pos { 0 };
        float feedback { 0.82f };
        float damp1 { 0.0f };
        float damp2 { 0.4f };
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
    CombFilter    mCombs[4][2];
    AllpassFilter mAllpasses[2][2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcousticSimulatorStyleDSP)
};
