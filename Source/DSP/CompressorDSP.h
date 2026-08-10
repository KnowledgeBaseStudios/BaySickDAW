#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include <atomic>

// -- CompressorDSP ------------------------------------------------------------
// Full-featured stereo compressor with soft knee, parallel mix, sidechain
// input, look-ahead, peak/RMS detection, and a lock-free GR meter output.
//
// Phase H-2 (2026-05-01): Type umbrella adds character modes:
//   Modern -- existing algorithm bit-exact untouched (default).
//   FET    -- 1176-inspired: aggressive envelope, asymmetric attack curve,
//             tanh saturation on the gain-control signal at deep GR.
//   Opto   -- LA-2A-inspired: blended multi-stage release with program-
//             dependent memory, soft-knee floor, subtle 2nd-harmonic warmth.
// Existing presets default to Modern -- audio identical to pre-H-2 behavior.
//
// Phase I-4 (2026-05-02): CS Style Type added (4th option):
//   CS     -- BOSS CS-Style sustain compressor.  Single Sustain macro knob
//             that simultaneously lowers Threshold and increases makeup gain
//             so one knob = "more sustain".  Active tilt EQ post-comp (single
//             Tone knob: bipolar, center=flat, up=brighter, down=darker).
//             Ratio + Release fixed internally to CS-typical values when this
//             Type is active; user-facing knobs are Level / Tone / Attack /
//             Sustain only.
// -----------------------------------------------------------------------------
class CompressorDSP : public DSPBase
{
public:
    enum class Type : int
    {
        Modern = 0,   // current algorithm, bit-exact preserved
        FET    = 1,   // 1176-style nonlinear envelope + GR saturation
        Opto   = 2,   // LA-2A-style multi-stage release + soft knee + cell warmth
        CS     = 3    // I-4: BOSS CS-Style sustain comp + tilt EQ post-comp
    };

    CompressorDSP();
    ~CompressorDSP() override = default;

    // DSPBase interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;

    void getStateInformation (juce::MemoryBlock& dest)           override;
    void setStateInformation (const void* data, int sz)          override;

    // setHostBPM is a no-op for compressor
    void setHostBPM (double /*bpm*/) override {}

    // Returns the most-recent gain reduction in dB (negative = reduced)
    float getGainReductionDb() const override;

    // QA-Layout T18 (audio-first rework, Jeff 2026-08-06): publishes the input
    // envelope + GR + threshold per block so the visual shows the compressor
    // WORKING on real audio; the knee curve (drawn from the fields below) rides
    // beside it.  hasVisual() defaults through this.
    bool  hasVisualFeed() const override { return true; }

    // Latency reported to EffectRack for PDC (= look-ahead in samples).
    int getLatencySamples() const override { return mLASamples; }

    // Parameter setters (all CPU-guarded: no-op when unchanged)
    void setThreshold (float dB);        // default -12 dB
    void setRatio     (float ratio);     // 0.4..30; <1 = upward expansion
    void setAttack    (float ms);        // 0..400 ms
    void setRelease   (float ms);        // 1..4000 ms
    void setMakeup    (float dB);        // makeup gain (legacy, clamped -30..+30)
    void setGain      (float dB);        // same as setMakeup, range -30..+30 dB
    void setKnee      (float dB);        // manual soft-knee width override
    void setKneeType  (int type);        // 0=Hard,1=Med,2=Vintage,3=Soft,4-7=/R variants
    void setMix       (float mix);       // 0=dry .. 1=wet, default 1

    // 2a -- Look-ahead delay in ms (0..5). 0 = off, no latency. Changes
    // getLatencySamples(); host must be told (EffectRack calls updateBusLatencies).
    void setLookaheadMs (float ms);
    // 2b -- When true, detector uses max(|L|,|R|) and a single shared envelope
    // drives both channels. When false, each channel is independently compressed.
    void setStereoLink  (bool on);
    // 2c -- When true, makeup is computed each block so output level tracks
    // threshold automatically. Manual Gain ignored.
    void setAutoMakeup  (bool on);

    // C.4 Phase 1 (2026-04-30): SlotComponent header SC dropdown gate -- only
    // shown for effects that actually consume the SC signal.
    bool usesSidechain() const noexcept override { return true; }
    // C2 -- Sidechain high-pass filter cutoff (20..2000 Hz). Applied to the
    // detection source only (internal or external). 20 Hz = effectively off.
    void setSidechainHPF (float hz);
    // C3 -- Detection mode: false = RMS (~Det window, smoothed), true = Peak
    // (|x| with fast attack/slow decay). PRESET-SAFE: default false = current behavior.
    void setPeakDetection (bool on);
    // C4 -- Detection window / smoother time constant in ms (1..100). Applies to
    // RMS mode. Smaller = faster tracking; larger = smoother/musical.
    void setDetectionMs (float ms);

    // H-2 (2026-05-01): Type umbrella -- switches algorithm character.
    // 0 = Modern (default, current algo), 1 = FET, 2 = Opto, 3 = CS Style.
    // Switching to CS Style coerces ratio + release to CS-typical fixed
    // values (ratio 5:1, release 200ms) and applies the current csSustain
    // macro to threshold + makeup.  Switching away leaves the user-facing
    // params as-is (Modern/FET/Opto don't read csSustain or csTone).
    void setType (int t);

    // QA-EffectsReview Task 6: FET all-buttons-in toggle (see fetAllButtons).
    void setFetAllButtons (bool on);

    // QA-EffectsReview Task 6: Opto Comp/Limit toggle (see optoLimit).
    void setOptoLimit (bool on);

    // I-4 (2026-05-02): CS Style "Sustain" macro.  Single user knob 0..1
    // that maps inversely to threshold AND boosts makeup gain in lockstep.
    //   sustain=0  -> threshold ~ -6 dB,  makeup +0 dB (light comp)
    //   sustain=1  -> threshold ~ -36 dB, makeup +12 dB (heavy comp + sustain)
    // Stored on the DSP so the panel surface stays one-knob; setSustain is
    // applied immediately when mType == CS, ignored otherwise (Modern/FET/
    // Opto continue to use the user's manual threshold + gain settings).
    void setCsSustain (float sustain01);

    // I-4 (2026-05-02): CS Style post-comp tilt EQ.  0..1, center 0.5 = flat.
    // < 0.5 = darker (low-shelf boost + high-shelf cut); > 0.5 = brighter
    // (low-shelf cut + high-shelf boost).  Applied AFTER the compression
    // stage, only when mType == CS.
    void setCsTone (float tone01);

    // I-4 (2026-05-02): CS Style output Level trim (dB), applied AFTER the
    // tilt EQ.  Range -12..+12.  Independent of the Sustain macro's auto-
    // makeup so the user can level-trim without unwinding the macro.
    void setCsLevel (float dB);

    // Public parameter values (read-only externally; use setters to change)
    Type  mType      { Type::Modern };
    float threshold  { -12.0f };
    float ratio      {   4.0f };
    float attackMs   {  10.0f };
    float releaseMs  { 100.0f };
    float makeupDb   {   0.0f };
    float kneeDb     {   6.0f };   // set by setKneeType; default Medium=6dB
    float mix        {   1.0f };
    bool  useSidechain { false };
    int   mKneeType  {   1    };   // 0=Hard,1=Med,2=Vintage,3=Soft + /R variants 4-7
    float lookaheadMs{   0.0f };
    bool  stereoLink {   true };
    bool  autoMakeup {  false };
    float sidechainHPF { 20.0f };   // C2: 20 Hz default = effectively off
    bool  peakDetection{ false };   // C3: false = RMS (default), true = peak
    float detectionMs  { 10.0f };   // C4: RMS window in ms
    // QA-EffectsReview Task 6: FET (1176) all-buttons-in mode.  When true the
    // gain computer uses a program-dependent RISING ratio (the bias-shift
    // character) instead of the fixed button ratio.  Serialized; FET-only.
    bool  fetAllButtons { false };
    // QA-EffectsReview Task 6: Opto (LA-2A) Comp/Limit mode.  true = Limit (the
    // gain computer uses the 8->80:1 soft-ceiling rising curve).  Serialized;
    // Opto-only.  A flag, NOT inferred from ratio: setRatio clamps to 30, so the
    // panel's old "ratio 100 = Limit, read ratio>50" scheme was unreachable.
    bool  optoLimit { false };

    // I-4 (2026-05-02): CS Style state.  csSustain01 [0..1] is the macro
    // knob value; csTone01 [0..1] is the post-comp tilt EQ position
    // (0.5 = flat); csLevelDb is the user's output trim added AFTER the
    // tilt EQ (range +/-12 dB; independent of macro makeup so the user can
    // shape level without disturbing the Sustain macro's auto-makeup).
    // These are STORED regardless of mType so a user who dialed in CS Style
    // settings, switched to FET briefly, and switched back doesn't lose
    // their values.  Applied in process() only when mType == CS.
    float csSustain01 { 0.0f };
    float csTone01    { 0.5f };
    float csLevelDb   { 0.0f };
    // QA-EffectsReview Task 6: CS-3 Sustain pre-amp drive in dB (= 24 * csSustain01,
    // set by applyCsSustainMacro).  DERIVED from csSustain01 -> not serialized;
    // recomputed on load.  Applied CS-only in process() to detector + audio.
    float csInputDriveDb { 0.0f };

private:
    // I-4: build tilt-EQ coefficients from csTone01 + sample rate.  Called
    // from setCsTone, setType (when switching to CS), and prepare.
    void updateCsToneCoefs();

    // I-4: apply csSustain01 macro to threshold + makeup.  Called from
    // setCsSustain when mType == CS (and from setType when switching TO CS).
    void applyCsSustainMacro();
    // Recalculates attack/release/RMS/TCR coefficients from mSampleRate + stored ms values
    void calcCoefs();
    // Rebuilds sidechain HPF biquad coefficients from sidechainHPF + mSampleRate
    void updateScHpfCoefs();

    // Applies gain-computer curve (with soft/vintage knee) to a level in dB.
    // Returns the gain *change* in dB (typically <= 0).
    float computeGainDb (float levelDb) const noexcept;

    // Per-sample envelopes. When stereoLink=true only mEnvL is used
    // (mEnvR is kept in sync for meter reads); when false, both run independently.
    float mEnvL        { 0.0f };
    float mEnvR        { 0.0f };
    float mAttackCoef  { 0.0f };
    float mReleaseCoef { 0.0f };

    // TCR (/R) mode: slow-envelope of signal level used to accelerate release
    float mTCRLevel    { 0.0f };
    float mTcrAttCoef  { 0.0f };   // cached per sample-rate (was recomputed per block)
    float mTcrRelCoef  { 0.0f };

    // 2d -- Per-sample running mean-square for level detection
    float mRunningRmsL { 0.0f };
    float mRunningRmsR { 0.0f };
    float mRmsCoef     { 0.0f };

    // C3 -- Peak detector state (when peakDetection==true)
    float mPeakL       { 0.0f };
    float mPeakR       { 0.0f };
    float mPeakAttCoef { 0.0f };   // very fast (~0.1 ms)
    float mPeakRelCoef { 0.0f };   // slower (derived from detectionMs)

    // H-2 -- Opto multi-stage release state.
    // mOptoHistory tracks recent gain-reduction magnitude over a ~1 sec window
    // (one-pole running average); higher value -> more weight to the slow
    // release stage, less to the fast.  This is the "memory effect" that lets
    // sustained material relax slower than transient material on an LA-2A.
    float mOptoHistory       { 0.0f };
    float mOptoHistoryCoef   { 0.0f };   // ~1 sec one-pole, computed in calcCoefs
    float mOptoFastRelCoef   { 0.0f };   // ~60 ms exp coef
    float mOptoSlowRelCoef   { 0.0f };   // ~500 ms exp coef

    // C2 -- Sidechain HPF biquad state (per channel). Simple TPT highpass.
    juce::dsp::StateVariableTPTFilter<float> mScHpfL, mScHpfR;

    // I-4: CS Style post-comp tilt EQ.  Two stereo IIR shelves whose gains
    // move in opposite directions as csTone01 sweeps from 0.5 (flat) toward
    // 0 (darker) or 1 (brighter).  Applied only when mType == CS.
    using TiltShelf = juce::dsp::ProcessorDuplicator<
                          juce::dsp::IIR::Filter<float>,
                          juce::dsp::IIR::Coefficients<float>>;
    TiltShelf mCsLowShelf;
    TiltShelf mCsHighShelf;

    // 2a -- Look-ahead delay lines (main audio path only; detector is un-delayed)
    std::vector<float> mLookaheadL, mLookaheadR;
    int                mLAWritePos  { 0 };
    int                mLASamples   { 0 };   // current delay in samples
    static constexpr float kMaxLookaheadMs = 5.0f;

    std::atomic<float> mGainReductionDb { 0.0f };
    // A4 -- Peak-hold decay (dB/block) so transient GR isn't missed between UI polls.
    float mGrDecayDbPerBlock { 0.35f };

    // A2 -- Per-sample smoothed continuous parameters (zipper suppression).
    juce::LinearSmoothedValue<float> mThresholdSmoothed;
    juce::LinearSmoothedValue<float> mRatioSmoothed;
    juce::LinearSmoothedValue<float> mMixSmoothed;
    juce::LinearSmoothedValue<float> mMakeupDbSmoothed;

    juce::AudioBuffer<float>* mSidechainBuffer { nullptr };
};
