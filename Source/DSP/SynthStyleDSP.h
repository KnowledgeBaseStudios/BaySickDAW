#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include "PitchTrackerYIN.h"
#include "PolyPitchTracker.h"

// ─────────────────────────────────────────────────────────────────────────────
// SynthStyleDSP - Phase I-9 (2026-05-03)
// ─────────────────────────────────────────────────────────────────────────────
// SY Style Polyphonic Synth pedal.  Real-time pitch-tracked synth in the
// style of the BOSS SY-200 / SY-1.  Drives an internal oscillator voice
// from the input's detected fundamental + amplitude envelope.
//
// Algorithm:
//   1. Sum input L+R to mono and push into PitchTrackerYIN (H-4) - runs
//      pitch detection on its own background thread; we just read the
//      atomic published Hz + confidence each block.
//   2. Per-block envelope follower on the input (tracks dynamics so the
//      synth voice swells with picking strength).
//   3. Phase-accumulator oscillator at the tracked Hz with one of 4
//      waveforms (sine / saw / square / triangle), selected by the Type +
//      Variation knobs.
//   4. State-variable lowpass filter on the synth voice driven by Tone +
//      LFO depth (LFO sweeps cutoff for movement).
//   5. Mix: out = direct * dry + effectLevel * synthVoice.
//   6. 5 Hz DC blocker.
//
// Type presets bundle reasonable defaults for waveform / octave shift /
// envelope speed / LFO target:
//   Lead   -- saw, unison, fast env, LFO -> filter
//   Pad    -- sine+saw stack, slow env, gentle LFO -> filter
//   Bass   -- square, -1 oct, fast env, no LFO
//   Seq    -- saw, unison, LFO -> amplitude (rhythmic stutter)
//
// Variation 1..11 is a sub-variation knob within each Type that interpolates
// between two characters (e.g. brighter / darker, more saw / more square).
//
// Caveat: this is a monophonic implementation (single voice tracks the
// dominant fundamental).  True polyphonic guitar tracking is out of scope
// for v1; the single-voice approximation works well on bass and lead lines
// and matches BOSS SY-1's mono behaviour.  Polyphonic mode is a future
// polish pass per Phase I-9 spec wording "REAL implementation" -- this is
// a real working synth, just not a polyphonic one yet.
// ─────────────────────────────────────────────────────────────────────────────

class SynthStyleDSP : public DSPBase
{
public:
    // QA-EffectsReview Task 4: the BOSS SY-1's 11 TYPE categories (each pairs
    // with the Variation 1-11 sub-knob -> 121 sounds).  The original 4 keep
    // their enum values (0/1/2/3) so pre-existing patches map to the same slot;
    // the 7 new types take 4..10.
    enum class Type : int
    {
        Lead1 = 0, Pad = 1, Bass = 2, Seq1 = 3,
        Lead2 = 4, Str = 5, Organ = 6, Bell = 7, Sfx1 = 8, Sfx2 = 9, Seq2 = 10
    };
    static constexpr int kNumTypes = 11;

    // QA-EffectsReview Task 4: input instrument -> sets the poly tracker's
    // frequency range (Bass tracks an octave lower + caps polyphony to 1-2).
    enum class Instrument : int { Guitar = 0, Bass = 1 };

    SynthStyleDSP() = default;
    ~SynthStyleDSP() override = default;

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;
    void releaseResources()
    {
        // Caller (audio engine) typically doesn't invoke this on DSPBase,
        // but if the trackers' workers should be torn down explicitly we expose it.
        mYin.releaseResources();
        mPoly.releaseResources();
    }

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    void setType        (int   t);   // 0..3
    void setVariation   (int   v);   // 1..11
    void setTone        (float v01); // 0..1 -> 200..8000 Hz log
    void setRate        (float v01); // 0..1 -> 0.1..10 Hz log
    void setDepth       (float v01); // 0..1 LFO depth
    void setEffectLevel (float v01); // 0..1
    void setDirectLevel (float v01); // 0..1
    void setPolyMode    (bool  poly);   // false = mono YIN voice, true = poly FFT voices
    void setInstrument  (int   inst);   // 0 = Guitar, 1 = Bass

    Type  mType        { Type::Lead1 };
    int   mVariation   { 1 };
    float mTone        { 0.6f };
    float mRate        { 0.3f };
    float mDepth       { 0.4f };
    float mEffectLevel { 0.5f };
    float mDirectLevel { 1.0f };
    bool       mPolyMode   { false };
    Instrument mInstrument { Instrument::Guitar };

private:
    PitchTrackerYIN  mYin;
    PolyPitchTracker mPoly;

    // QA-EffectsReview Task 4: poly voice pool (each voice tracks one detected note).
    struct Voice
    {
        double phase    { 0.0 };
        double phaseInc { 0.0 };
        float  freq     { 0.0f };   // assigned note Hz (0 = free)
        float  env      { 0.0f };   // per-voice envelope
        bool   gate     { false };  // note present this frame
    };
    static constexpr int kMaxVoices = 8;
    Voice mVoices[kMaxVoices];

    void updateInstrumentRange();
    void allocateVoices (const PolyPitchTracker::NoteSet& ns);

    // Phase-accumulator oscillator state (single voice).
    double mPhase     { 0.0 };
    double mPhaseInc  { 0.0 };       // updated per block from tracked Hz + Type octave shift
    float  mEnvelope  { 0.0f };      // amplitude follower
    float  mEnvCoefAtt { 0.0f };
    float  mEnvCoefRel { 0.0f };

    // LFO for filter / amplitude modulation.
    double mLfoPhase    { 0.0 };
    double mLfoPhaseInc { 0.0 };

    // Tone filter (lowpass on the synth voice).
    juce::dsp::StateVariableTPTFilter<float> mLpf;

    juce::AudioBuffer<float> mMonoIn;
    juce::AudioBuffer<float> mSynthBuf;

    // 5 Hz DC blocker on the final mix.
    float mDcXL { 0.0f }, mDcYL { 0.0f };
    float mDcXR { 0.0f }, mDcYR { 0.0f };
    float mDcCoef { 0.0f };

    void updateLfoCoef();
    void updateToneCutoff (float lfoMod);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthStyleDSP)
};
