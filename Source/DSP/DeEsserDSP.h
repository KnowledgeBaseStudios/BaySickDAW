#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include <atomic>

// ─────────────────────────────────────────────────────────────────────────────
// DeEsserDSP — Phase H-3 (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────
// Stereo de-esser with two operating modes:
//
//   Wide  -- when sidechain HPF detects energy above threshold, the entire
//            full-band signal gets ducked.  Aggressive, simple, transparent
//            on dialog where sibilance is sparse.
//
//   Split -- the signal is split into two bands at the sidechain crossover
//            frequency; only the high band is ducked when the detector
//            exceeds threshold.  Low band passes unchanged.  Cleaner on
//            sustained-vocal material where you want to keep body + clarity.
//
// Detection path:
//   sidechain HPF (5-10 kHz, user) -> envelope follower (peak, atk/rel) ->
//   threshold gate -> dynamic notch (signal-dependent attenuation curve).
//
// Optional features:
//   * M/S processing -- detect on Mid only, Side only, or stereo (default).
//   * Lookahead (0..5 ms) -- detector sees the signal slightly ahead of the
//     audio so the duck lands ON the consonant, not after it.
//   * Range / Max Reduction -- caps how much GR the de-esser is allowed to
//     pull down regardless of how loud the consonant is.
//   * Sidechain Listen -- monitor button replaces the audio output with the
//     sidechain HPF signal so the user can find the right cutoff by ear.
//   * GR meter -- atomic published GR value for UI 30 Hz polling.
//
// Threading model:
//   prepare() / setters run on message thread; process() audio thread; GR
//   atomic published wait-free per block.
// ─────────────────────────────────────────────────────────────────────────────

class DeEsserDSP : public DSPBase
{
public:
    enum class Mode : int
    {
        Wide  = 0,    // full-band ducking when sibilance detected
        Split = 1     // band-split, only high band ducks
    };

    enum class MsMode : int
    {
        Stereo  = 0,  // detect on stereo signal (L+R), apply to stereo
        MidOnly = 1,  // detect + apply to Mid component only
        SideOnly= 2   // detect + apply to Side component only
    };

    DeEsserDSP();
    ~DeEsserDSP() override = default;

    // ── DSPBase interface ─────────────────────────────────────────────────────
    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    void setHostBPM (double /*bpm*/) override {}

    float getGainReductionDb() const override
    {
        return mGainReductionDb.load (std::memory_order_relaxed);
    }

    int getLatencySamples() const override { return mLASamples; }

    // ── Parameter setters (CPU-guarded; no-op when unchanged) ─────────────────
    void setMode          (int m);          // 0=Wide, 1=Split
    void setMsMode        (int m);          // 0=Stereo, 1=Mid, 2=Side
    void setFrequencyHz   (float hz);       // 4000..12000  (sidechain HPF + split crossover)
    void setQ             (float q);        // 0.5..4.0
    void setThresholdDb   (float dB);       // -40..0
    void setRangeDb       (float dB);       // -20..0   max reduction (negative dB)
    void setAttackMs      (float ms);       // 0.1..30
    void setReleaseMs     (float ms);       // 10..500
    void setLookaheadMs   (float ms);       // 0..5
    void setMix           (float v);        // 0..1
    void setListen        (bool on);        // true = output sidechain HPF for monitoring

    // ── Public read-only parameter values (use setters to change) ─────────────
    Mode    mMode          { Mode::Wide };
    MsMode  mMsMode        { MsMode::Stereo };
    float   mFreqHz        { 6500.0f };
    float   mQ             { 1.4f };
    float   mThresholdDb   { -24.0f };
    float   mRangeDb       { -12.0f };   // signed dB, negative = max reduction
    float   mAttackMs      { 1.0f };
    float   mReleaseMs     { 80.0f };
    float   mLookaheadMs   { 0.0f };
    float   mMix           { 1.0f };
    bool    mListen        { false };

private:
    void calcCoefs();
    void updateScCoefs();    // sidechain HPF + split crossover

    // ── Sidechain HPF (detection-only path) ──────────────────────────────────
    juce::dsp::StateVariableTPTFilter<float> mScHpfL, mScHpfR;

    // ── Split-mode crossover filters (audio path) ───────────────────────────
    // Linkwitz-Riley style: HPF + LPF at the same cutoff form a clean split.
    juce::dsp::StateVariableTPTFilter<float> mAudHpfL, mAudHpfR;   // high band
    juce::dsp::StateVariableTPTFilter<float> mAudLpfL, mAudLpfR;   // low band

    // ── Envelope follower (peak detector with atk/rel) ──────────────────────
    float mEnvL { 0.0f };
    float mEnvR { 0.0f };
    float mAttackCoef  { 0.0f };
    float mReleaseCoef { 0.0f };

    // ── Lookahead delay lines ────────────────────────────────────────────────
    std::vector<float> mLookaheadL, mLookaheadR;
    int                mLAWritePos { 0 };
    int                mLASamples  { 0 };
    static constexpr float kMaxLookaheadMs = 5.0f;

    // ── GR meter (atomic for wait-free UI read) ─────────────────────────────
    std::atomic<float> mGainReductionDb { 0.0f };
    float mGrDecayDbPerBlock { 0.35f };

    // ── Smoothers for zipper-free knob drags ────────────────────────────────
    juce::LinearSmoothedValue<float> mThresholdSmoothed;
    juce::LinearSmoothedValue<float> mRangeSmoothed;
    juce::LinearSmoothedValue<float> mMixSmoothed;
};
