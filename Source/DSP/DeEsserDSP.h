#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include "SibilanceSpectralProcessor.h"
#include <atomic>

// ─────────────────────────────────────────────────────────────────────────────
// DeEsserDSP - Phase H-3 (2026-05-01); reference rework QA-EffectsReview Task 8
// ─────────────────────────────────────────────────────────────────────────────
// Stereo de-esser modeled on Waves Sibilance's control surface.  Mode is a
// CONTINUOUS 0..100 blend (Sibilance's Mode knob), not a switch:
//
//   0 (Wide)   -- when the detector exceeds threshold, the entire full-band
//                 signal gets ducked.
//   100 (Split)-- the signal splits at a FIXED 4 kHz crossover (the reference
//                 hardcodes 4 kHz); only the high band is ducked, the low band
//                 passes unchanged.
//   between    -- crossfade of the two results.
//
// Detection path:
//   sidechain HPF (4-12 kHz, user; the Basic "Detection" macro maps width to
//   freq+Q) -> envelope follower (peak, atk/rel) -> threshold gate ->
//   saturating overshoot->reduction curve floored at Range.
//
// Extras beyond the reference (Advanced in the panel):
//   * M/S processing -- detect + duck Mid only, Side only, or stereo (default).
//   * Lookahead (0..12 ms) -- detector sees the signal ahead of the audio so
//     the duck lands ON the consonant.  Reference exposes on/off (~11 ms);
//     the continuous range is ours.  Changes getLatencySamples() -> host PDC
//     must be refreshed (panel pokes onLatencyChanged).
//   * Mix -- wet/dry blend (reference has none).
//
// Listen (reference "Monitor"): solos the removed component, computed from
// the gain reduction directly ((1-gr) x the ducked band) -- NOT dry-minus-wet
// (the split filters' phase rotation makes plain subtraction leak the whole
// program even at zero GR).  Silent when nothing is detected.
//
// Threading model:
//   prepare() / setters run on message thread; process() audio thread; GR
//   atomic published wait-free per block.
// ─────────────────────────────────────────────────────────────────────────────

class DeEsserDSP : public DSPBase
{
public:
    // TimeDomain = the zero-latency envelope de-esser (default); Spectral =
    // the STFT per-bin engine (reference character, adds FFT latency).
    enum class Engine : int
    {
        TimeDomain = 0,
        Spectral   = 1
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

    // Spectral engine replaces the lookahead latency (lookahead is a no-op
    // there -- the frame analysis inherently sees ahead).
    int getLatencySamples() const override
    {
        return mEngine == Engine::Spectral ? mSpectral.getLatencySamples()
                                           : mLASamples;
    }

    // ── Parameter setters (CPU-guarded; no-op when unchanged) ─────────────────
    void setModeBlend     (float v);        // 0..100  (0=Wide, 100=Split@4kHz, between=blend)
    // Legacy 0/1 Wide/Split shim -- BaySickVocal's bsv_deesser_mode APVTS param
    // (Int 0/1) drives this per block; maps onto the blend endpoints.
    void setMode (int m) { setModeBlend (m != 0 ? 100.0f : 0.0f); }
    void setMsMode        (int m);          // 0=Stereo, 1=Mid, 2=Side
    void setFrequencyHz   (float hz);       // 4000..12000  (sidechain detector HPF)
    void setQ             (float q);        // 0.5..4.0
    void setThresholdDb   (float dB);       // -80..0
    void setRangeDb       (float dB);       // -48..0   max reduction (negative dB)
    void setAttackMs      (float ms);       // 0.1..30
    void setReleaseMs     (float ms);       // 10..500
    void setLookaheadMs   (float ms);       // 0..12  (changes latency -> refresh PDC)
    void setMix           (float v);        // 0..1
    void setListen        (bool on);        // true = solo the removed signal (monitor)
    void setEngine        (int e);          // 0=TimeDomain, 1=Spectral (changes latency -> refresh PDC)
    void setSpectralQuality (int q);        // 0=HQ 2048 (~43 ms), 1=LL 1024 (~21 ms); latency change

    static constexpr float kMaxLookaheadMs = 12.0f;   // fits the reference's ~11 ms on/off

    // ── Public read-only parameter values (use setters to change) ─────────────
    Engine  mEngine        { Engine::TimeDomain };
    int     mSpectralQuality { 0 };      // 0=HQ, 1=LL
    float   mModeBlend     { 50.0f };    // reference default = mid blend
    MsMode  mMsMode        { MsMode::Stereo };
    float   mFreqHz        { 6500.0f };
    float   mQ             { 1.4f };
    float   mThresholdDb   { -12.5f };   // reference default
    float   mRangeDb       { -14.0f };   // signed dB, negative = max reduction (reference default)
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

    // ── Spectral engine (Engine::Spectral path) ─────────────────────────────
    void processSpectral (juce::AudioBuffer<float>& buffer);
    SibilanceSpectralProcessor mSpectral;
    // Latency-aligned rings: dry L/R for the Mix crossfade + the untouched
    // M/S axis for recompose -- both must be delayed by the spectral latency
    // or the blend/recompose combs against the delayed wet.
    std::vector<float> mSpDryL, mSpDryR, mSpOther;
    std::vector<float> mSpAxis;          // scratch: M/S axis fed to the STFT
    int                mSpRingPos  { 0 };
    int                mSpLastLat  { -1 };   // detects quality/engine swaps -> re-zero rings
    bool               mSpectralPrimed { false };

    // ── GR meter (atomic for wait-free UI read) ─────────────────────────────
    std::atomic<float> mGainReductionDb { 0.0f };
    float mGrDecayDbPerBlock { 0.35f };

    // ── Smoothers for zipper-free knob drags ────────────────────────────────
    juce::LinearSmoothedValue<float> mThresholdSmoothed;
    juce::LinearSmoothedValue<float> mRangeSmoothed;
    juce::LinearSmoothedValue<float> mMixSmoothed;
    juce::LinearSmoothedValue<float> mModeSmoothed;   // blend 0..1 (Wide->Split)
};
