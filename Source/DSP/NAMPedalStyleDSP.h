#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"

namespace nam { class DSP; }

// ─────────────────────────────────────────────────────────────────────────────
// NAMPedalStyleDSP - Phase I-15c (2026-05-03)
// ─────────────────────────────────────────────────────────────────────────────
// User NAM Pedal.  Loads a .nam Neural Amp Modeler capture file and runs it
// per-block, sandwiched between a pre-model gain stage (Input/Drive) and a
// post-model 3-band Baxandall EQ + dry/wet blend + output trim.
//
// Signal flow:
//   input -> Input/Drive (-24..+24 dB) -> mono-sum -> NAM model ->
//     dual-mono spread -> 3-band EQ (Low 100Hz / Mid 1kHz / High 5kHz) ->
//     blend with dry input -> Output (-24..+12 dB) -> output
//
// Knob spec (locked 2026-05-03):
//   Input/Drive   -24..+24 dB    pre-model gain (push hotter signal into NAM)
//   Low           +/-15 dB        100 Hz low shelf (post-model)
//   Mid           +/-15 dB        1 kHz peak Q=0.7 (post-model)
//   High          +/-15 dB        5 kHz high shelf (post-model)
//   Blend         0..1            0 = dry, 1 = wet (default 1.0)
//   Output        -24..+12 dB     final output trim
//
// Threading: model load is wait-free swap (mirrors BaySickNAMIRProcessor).
// Audio thread sees published model via active/pending pair; new model
// parked in pending, swap drained at top of process().
// ─────────────────────────────────────────────────────────────────────────────

class NAMPedalStyleDSP : public DSPBase
{
public:
    NAMPedalStyleDSP();
    ~NAMPedalStyleDSP() override;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    void setInputDb  (float db);    // -24..+24
    void setLowDb    (float db);    // -15..+15
    void setMidDb    (float db);    // -15..+15
    void setHighDb   (float db);    // -15..+15
    void setBlend    (float v01);   // 0..1
    void setOutputDb (float db);    // -24..+12

    // Load a .nam file.  Returns false + outErr filled on failure.
    bool loadModel (const juce::File& file, juce::String& outErr);

    juce::String getModelPath() const { return mModelPath; }
    juce::String getModelName() const;   // filename without extension, "" if no model

    float mInputDb  { 0.0f };
    float mLowDb    { 0.0f };
    float mMidDb    { 0.0f };
    float mHighDb   { 0.0f };
    float mBlend01  { 1.0f };
    float mOutputDb { 0.0f };

private:
    void rebuildEQ();
    void drainPendingSwap();   // audio-thread

    using IIRDup = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                    juce::dsp::IIR::Coefficients<float>>;

    double mSampleRate { 44100.0 };
    int    mMaxBlock   { 0 };
    bool   mPrepared   { false };

    // Wait-free model swap.
    std::unique_ptr<nam::DSP> mNamActive;
    std::unique_ptr<nam::DSP> mNamPending;
    std::atomic<bool>         mSwapPending { false };
    juce::CriticalSection     mLoadLock;
    juce::String              mModelPath;

    // Pre-allocated mono buffers for NAM (NAM model expects double-precision mono).
    std::vector<double>       mMonoIn;
    std::vector<double>       mMonoOut;

    // Scratch buffers for dry path + wet processing.
    juce::AudioBuffer<float>  mDryScratch;

    // Post-model EQ.
    IIRDup mLowShelf;
    IIRDup mMidPeak;
    IIRDup mHighShelf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMPedalStyleDSP)
};
