#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include <array>
#include <vector>

// ── ChorusDSP ─────────────────────────────────────────────────────────────────
// 3-LFO chorus with per-LFO frequency and waveform, global depth multiplier,
// stereo phase spread, Linkwitz-Riley 4th-order crossover band-split,
// and 3 or 6 voice modes.
//
// Voice model:
//   3 voices  → 1 read per LFO (L and R each at phase + stereoOffset)
//   6 voices  → 2 reads per LFO (second read offset by π)
//
// Crossover: LR4 (24 dB/oct) splits signal; only the selected band passes
// through the chorus; the other band is added back dry at the output.
// `wetOnly` mode kills both the dry pass-band and the unprocessed portion
// of the processed band (for effect-send routing).
// ─────────────────────────────────────────────────────────────────────────────
class ChorusDSP : public DSPBase
{
public:
    ChorusDSP();
    ~ChorusDSP() override = default;

    // DSPBase interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;
    void setHostBPM (double) override {}

    // ── API ───────────────────────────────────────────────────────────────────
    void setLFOFreq    (int lfo, float hz);   // lfo=0/1/2, range 0.01..10 Hz
    void setLFOWave    (int lfo, int wave);   // 0=Sine 1=Triangle 2=Multi 3=Organic
    void setDelay      (float ms);            // base delay 0.5..30 ms
    void setDepth      (float ms);            // modulation depth 0..20 ms
    void setStereo     (float degrees);       // L/R LFO phase offset 0..360°
    void setCrossType  (int t);               // 0=HP (high chorused)  1=LP (low chorused)
    void setCrossCutoff(float hz);            // crossover cutoff frequency
    void setVoices     (int n);               // 3 or 6
    void setWet        (float w);             // 0..1
    void setWetOnly    (bool on);             // kill dry + pass-band return

    // ── Public state ──────────────────────────────────────────────────────────
    float delayMs     {  8.0f };
    float depth       {  8.0f };
    float wet         {  0.5f };
    int   voices      {  3    };
    float stereo      { 120.0f };  // degrees
    int   crossType   {  0    };   // 0=HP  1=LP
    float crossCutoff { 800.0f };
    bool  wetOnly     { false };

    struct LFOParams { float freq { 0.5f }; int wave { 0 }; };
    std::array<LFOParams, 3> lfoParams;

private:
    // Max delay = base (30ms) + depth (20ms) + largest per-voice prime offset (7ms) + safety
    static constexpr float kMaxDelayMs = 64.0f;

    // Per-voice prime-number base-delay offsets (ms).
    // 3-voice mode uses rows 0..2 (0/2/5); 6-voice mode uses all six (adds 1/3/7 for voices 3..5).
    // Each voice gets a unique combination of LFO phase and base offset, so even when LFOs align
    // the voices cannot collapse onto the same delay center.
    static constexpr float kVoicePrimeOffsetMs[6] = { 0.0f, 2.0f, 5.0f, 1.0f, 3.0f, 7.0f };

    // Single shared stereo delay buffer (all voices read from different positions)
    std::vector<float> mBufL, mBufR;
    int mBufSize  { 0 };
    int mWritePos { 0 };

    // Per-LFO runtime phase (not serialised — reset on prepare)
    std::array<float, 3> mLFOPhase {};

    // LR4 crossover — per-channel state lives inside the filter objects.
    juce::dsp::LinkwitzRileyFilter<float> mXoLP;
    juce::dsp::LinkwitzRileyFilter<float> mXoHP;

    // Per-sample smoothed continuous params — eliminate knob-drag zippers.
    juce::LinearSmoothedValue<float> mDelaySmoothed;
    juce::LinearSmoothedValue<float> mDepthSmoothed;
    juce::LinearSmoothedValue<float> mStereoSmoothed;
    juce::LinearSmoothedValue<float> mWetSmoothed;

    static float evalLFO    (int wave, float phase);
    static float cubicInterp(const std::vector<float>& buf, float rpos, int size);
};
