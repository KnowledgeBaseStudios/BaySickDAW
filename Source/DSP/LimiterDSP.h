#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include "LufsMeterDSP.h"
#include "TruePeakMeter.h"
#include <array>
#include <atomic>
#include <memory>
#include <vector>

// ── LimiterDSP ────────────────────────────────────────────────────────────────
// Look-ahead peak limiter with soft-sat pre-stage and 4× oversampled True Peak
// detection. Fruity-Limiter-style architecture:
//
//   input → InputGain → DelayLine(0–10 ms)
//                                                                ↓
//                    (pre-delay tap) → 4× TP detector (JUCE Oversampling, IIR)
//                                                                ↓
//                                            stereo-linked peak envelope
//                                     (attack/release, 2-stage auto-release,
//                                      linear↔exponential release curve)
//                                                                ↓
//                                      GainComputer: min(1, ceiling/peak)
//                                                                ↓
//                                            applied to delayed audio
//                                                                ↓
//                    tanh SoftSat(SatThresh,SatCurve) -- post-limiter (FL SAT)
//                                                                ↓
//                                                hard ceiling clamp (safety)
//                                                                ↓
//                                                            output
//
// Reports latency via getLatencySamples() = aheadSamples + oversampler latency.
// EffectRack accumulates this for PDC.
// ─────────────────────────────────────────────────────────────────────────────
class LimiterDSP : public DSPBase
{
public:
    // ── TS7: Limiter vs Maximizer ─────────────────────────────────────────────
    // Two MODES on the panel's hamburger, the same mechanism Compressor uses for
    // Modern/FET/Opto/CS (SlotComponent::modeLabel / showModeMenu).
    //
    // Limiter mode is the FL REPRODUCTION and carries none of the TS7 additions
    // (Jeff, 2026-07-29).  Everything the maximizer suite adds -- character,
    // loudness target, true-peak auto-ceiling, the LUFS/dBFS meter -- belongs to
    // Maximizer mode only.  Default is Limiter, so a preset written before this
    // existed restores unchanged.
    //
    // WHY MODE IS THE VARIANT AXIS AND CHARACTER IS NOT.  EffectParamMap keys on
    // (type, variant), so an axis with N values costs N tables.  Mode changes
    // WHICH controls exist, so it earns that.  Character changes internal
    // ballistics only and leaves the parameter set identical -- as a variant it
    // would have meant 2 modes x 8 characters = 16 tables describing one set.
    // So character stays a selector, same as the Delay panel's model chickenhead.
    //
    // Ordinals pinned: the mode is persisted as the raw int ("mode"), so an
    // insertion or re-order silently repoints every saved limiter at a different
    // mode.  NEVER reorder or insert; append only, with an explicit value.  Same
    // rule as EffectType in EffectRack.h.
    enum class Mode { Limiter = 0, Maximizer = 1 };

    void setMode (Mode m);
    Mode getMode() const noexcept { return mMode; }
    int  getModeIndex() const noexcept { return (int) mMode; }
    static const char* modeName (int index) noexcept;
    static constexpr int numModes() { return 2; }

    // ── CL-243: character algorithms ──────────────────────────────────────────
    // Eight release/colour voicings (BLU-109's Transparent/Punchy/Vintage
    // voicings fold in here).  NAMES ARE OURS -- concept parity with the
    // reference limiters' style lists, never their names (no-brand-names rule).
    //
    // A character biases the INTERNAL ballistics: the auto-release envelope pair,
    // the GR depth at which the blend hands over to the slow envelope, a release
    // curve offset, a release-time scale, and the mode's own soft-sat drive.  The
    // user's Attack / Release / Ahead / SAT knobs stay RELATIVE controls within
    // the chosen character -- which is how a character-mode limiter is expected
    // to behave, and is why the knobs are not overridden.  Monotonicity is
    // preserved in every mode (more Release is always longer).
    //
    // Clean == today's fixed constants exactly (20 / 300 / 6 dB / no offset / no
    // scale / no sat), so a preset written before CL-243 -- which restores
    // character 0 by default -- sounds bit-identical to before.
    //
    // Ordinals pinned: the character is persisted as the raw int ("character"),
    // and kCharacters in the .cpp is indexed by it, so an insertion or re-order
    // silently repoints every saved limiter at different ballistics.  NEVER
    // reorder or insert; append only, with an explicit value, ahead of Count.
    // (Count deliberately stays un-valued so it keeps tracking the real end of
    // the list.)  Same rule as EffectType in EffectRack.h.
    enum class Character
    {
        Clean   = 0,   // most transparent; the pre-CL-243 behavior
        Smooth  = 1,   // longer, gentler recovery
        Tight   = 2,   // fast and controlled
        Punch   = 3,   // transient-forward, hands to the slow envelope early
        Glue    = 4,   // bus-style, slow and programme-dependent
        Loud    = 5,   // maximum density, sat engaged
        Warm    = 6,   // saturation-forward color
        Instant = 7,   // near-zero-lookahead clip guard
        Count
    };

    struct CharacterProfile
    {
        const char* name;
        float relFastMs;       // auto-release fast envelope time constant
        float relSlowMs;       // auto-release slow envelope time constant
        float autoKneeDb;      // GR depth at which the blend fully favours slow
        float curveOffset;     // added to the user's ReleaseCurve (result clamped 0..1)
        float relScale;        // multiplies the user's Release time
        float satAutoDrive;    // the mode's own soft-sat drive, on top of the SAT knob
    };

    static const CharacterProfile& profileFor (Character c) noexcept;
    static const char* characterName (int index) noexcept;
    static constexpr int numCharacters() { return (int) Character::Count; }

    LimiterDSP();
    ~LimiterDSP() override = default;

    // DSPBase interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;
    void getStateInformation (juce::MemoryBlock& dest) override;
    void setStateInformation (const void* data, int sz) override;
    float getGainReductionDb() const override { return mGrDb.load(); }

    // QA-Layout T13: this DSP publishes columns, so Menu > Visual is offered
    // rather than greyed.  The column layout it writes is documented at the
    // push site in process().
    bool hasVisualFeed() const override { return true; }
    int   getLatencySamples() const override  { return mLatencySamples; }

    // ── Setters ───────────────────────────────────────────────────────────────
    void setInputGainDb   (float dB);     // -12..+24 dB     (smoothed, 15 ms)
    void setCeilingDb     (float dB);     // -24..+12 dB     (smoothed, 15 ms; +12 = FL headroom)
    void setSatThresh     (float lin);    // 0..1 linear     (smoothed, 15 ms; 1.0 = off)
    void setSatCurve      (float v01);    // 0..1 knee shape (smoothed, 15 ms)
    void setAttackMs      (float ms);     // 0.1..20 ms
    void setReleaseMs     (float ms);     // 10..1000 ms
    void setAheadMs       (float ms);     // 0..10 ms        (delay line size)
    void setReleaseCurve  (float v01);    // 0..1 (0=linear, 1=exp)
    void setSustainMs     (float ms);     // 0..1000 ms (Fruity RMS-window hold; 0 = off)
    void setAutoRelease   (bool on);
    // C2: Sidechain HPF cutoff on detector path (20..2000 Hz; 20 = effectively off).
    void setSidechainHPF  (float hz);
    // C.4 Phase 2.1: LimiterDSP consumes external SC for the detector path.
    bool usesSidechain() const noexcept override { return true; }
    // C4: Auto-makeup gain. Boosts output by -ceilingDb so lowering the ceiling
    // doesn't quiet the signal (loudness-maximizer workflow).
    void setAutoMakeup    (bool on);
    // C5: Stereo-link detector. true = single envelope from max(|L|,|R|) drives
    // both channels (default, cleanest stereo image). false = per-channel envelopes.
    void setStereoLink    (bool on);

    // ── TS7 / CL-243: character selection ─────────────────────────────────────
    void setCharacter (Character c);
    void setCharacterIndex (int index);   // automation/serialisation-friendly
    Character getCharacter()      const noexcept { return mCharacter; }
    int       getCharacterIndex() const noexcept { return (int) mCharacter; }

    // ── TS7 / CL-244: loudness-target mode ────────────────────────────────────
    // A slow closed-loop servo: the limiter measures its OWN OUTPUT loudness
    // (short-term, 3 s) and trims input gain toward the target.  Closed loop is
    // the correct topology -- output loudness is what the target is about, and
    // measuring it self-corrects for the limiter's own gain reduction.  The loop
    // converges over seconds by design ("after listening to a section"), which is
    // what the slew limit below is for; it is not a compressor.
    void  setLoudnessTargetOn  (bool on);
    void  setLoudnessTargetLufs (float lufs);        // -30 .. 0 LUFS
    bool  getLoudnessTargetOn()   const noexcept { return mLoudnessTargetOn; }
    float getLoudnessTargetLufs() const noexcept { return mLoudnessTargetLufs; }
    // Current servo contribution, dB.  UI readout; also tells the user why the
    // effective input gain differs from the knob.
    float getLoudnessServoDb()    const noexcept { return mServoDb.load(); }

    // ── TS7 / BLU-108: true-peak auto-ceiling ─────────────────────────────────
    // The hard clamp guarantees the SAMPLE peak stays under the ceiling; it says
    // nothing about inter-sample peaks, which is what clips a consumer DAC or a
    // lossy transcode.  With auto-ceiling on, the real output true peak is
    // measured (TruePeakMeter) and the effective ceiling is trimmed down until it
    // sits under the true-peak target -- the Spotify/YouTube margin.
    void  setAutoCeiling     (bool on);
    void  setTruePeakTargetDb (float dbTp);          // -6 .. 0 dBTP
    bool  getAutoCeiling()      const noexcept { return mAutoCeiling; }
    float getTruePeakTargetDb() const noexcept { return mTruePeakTargetDb; }
    float getCeilingTrimDb()    const noexcept { return mCeilingTrimDb.load(); }
    // -144 when auto-ceiling is off (the meter is not run, so there is no value
    // to report -- reporting a stale one would be worse than reporting none).
    float getOutputTruePeakDb() const noexcept { return mOutTpDb.load(); }

    // ── TS7 / BLU-110: LUFS beside dBFS ───────────────────────────────────────
    // The output loudness meter costs K-weighting filters per sample -- roughly a
    // biquad-heavy effect's worth per instance, and a project can hold dozens of
    // limiters -- so it runs only while something needs it: target mode (which
    // depends on it) or a panel that is watching.
    //
    // A WATCHDOG, not an on/off pair, and that is deliberate.  The obvious design
    // is enable-in-ctor / disable-in-dtor, but removing a rack effect destroys the
    // DSP SYNCHRONOUSLY inside EffectRack::packSlotsToTop while the panel window
    // is still open (the TS6 editor-outlives-instance crash was this exact
    // ordering), so a destructor that touched the DSP could touch freed memory.
    // Poking a countdown from the panel's existing 30 Hz timer needs no teardown
    // call at all: the meter simply lapses a beat after nobody is looking.
    void  pokeLufsMeter() noexcept;
    float getOutputLufsMomentary() const noexcept { return mOutLufsM.load(); }
    float getOutputLufsShortTerm() const noexcept { return mOutLufsS.load(); }
    float getOutputLufsIntegrated() const noexcept { return mOutLufsI.load(); }

    // ── Meter accessors (UI thread reads, audio thread writes) ────────────────
    float getCurrentGainReduction() const { return mGrDb.load(); }
    float getInputLevelDb()         const { return mInputDb.load(); }
    float getOutputLevelDb()        const { return mOutputDb.load(); }

    // Public read of target param values (for UI knobs / automation mirroring)
    float getInputGainDb()  const { return mInputGainTargetDb; }
    float getCeilingDb()    const { return mCeilingTargetDb; }
    float getSatThresh()    const { return mSatThreshTarget; }
    float getSatCurve()     const { return mSatCurve; }
    float getAttackMs()     const { return mAttackMs; }
    float getReleaseMs()    const { return mReleaseMs; }
    float getAheadMs()      const { return mAheadMs; }
    float getReleaseCurve() const { return mReleaseCurve; }
    float getSustainMs()    const { return mSustainMs; }
    bool  getAutoRelease()  const { return mAutoRelease; }
    float getSidechainHPF() const { return mSidechainHPF; }
    bool  getAutoMakeup()   const { return mAutoMakeup; }
    bool  getStereoLink()   const { return mStereoLink; }

private:
    // ── Param targets (also the authoritative values in serialization) ────────
    float mInputGainTargetDb { 0.0f };
    float mCeilingTargetDb   { -0.3f };
    float mSatThreshTarget   { 1.0f };
    float mSatCurve          { 0.5f };
    float mAttackMs          { 1.0f };
    float mReleaseMs         { 100.0f };
    float mAheadMs           { 2.0f };
    float mReleaseCurve      { 0.5f };
    float mSustainMs         { 0.0f };    // 0..1000 ms RMS window (0 = off)
    bool  mAutoRelease       { false };
    float mSidechainHPF      { 20.0f };   // C2: 20 Hz = effectively off
    bool  mAutoMakeup        { false };   // C4
    bool  mStereoLink        { true };    // C5

    // ── TS7 params ────────────────────────────────────────────────────────────
    Mode      mMode               { Mode::Limiter };      // FL reproduction by default
    Character mCharacter          { Character::Clean };   // CL-243
    bool      mLoudnessTargetOn   { false };              // CL-244
    float     mLoudnessTargetLufs { -14.0f };
    bool      mAutoCeiling        { false };              // BLU-108
    float     mTruePeakTargetDb   { -1.0f };

    // ── Smoothed values (15 ms linear ramp) ───────────────────────────────────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mInputGainSmooth;   // dB
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mCeilingSmooth;     // dB
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mSatThreshSmooth;   // linear
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mSatCurveSmooth;    // 0..1 (C1)

    // C2: Sidechain HPF (TPT SVF, stereo). Applied to the pre-delay detection
    // signal only; main audio path is untouched.
    juce::dsp::StateVariableTPTFilter<float> mScHpfL, mScHpfR;
    void updateScHpfCoefs();

    // ── Envelope state ────────────────────────────────────────────────────────
    // When mStereoLink == true, only the L variants are used (single envelope
    // drives both channels). When false, each channel runs its own envelope.
    float mEnv         { 0.0f };   // user-release envelope (L / linked)
    float mEnvFast     { 0.0f };   // auto-release fast envelope (~20 ms)
    float mEnvSlow     { 0.0f };   // auto-release slow envelope (~300 ms)
    float mEnvR        { 0.0f };   // C5: R-channel user-release envelope
    float mEnvFastR    { 0.0f };   // C5: R-channel fast envelope
    float mEnvSlowR    { 0.0f };   // C5: R-channel slow envelope
    float mAttackCoef  { 0.0f };
    float mReleaseCoef { 0.0f };
    float mRelFastCoef { 0.0f };
    float mRelSlowCoef { 0.0f };
    float mRelStepPerSample { 0.0f };   // linear-release step size (linear in dB/sample)
    float mRelStepFast      { 0.0f };
    float mRelStepSlow      { 0.0f };

    // SUSTAIN (Fruity-Limiter RMS window): a mean-square smoother holds the peak
    // envelope so GR releases over the window instead of early.  blendedPeak =
    // max(truePeak, sqrt(meanSq)) keeps the true-peak guarantee.  mMeanSqL also
    // serves the linked path (single detector).
    float mSusCoef  { 0.0f };
    float mMeanSqL  { 0.0f }, mMeanSqR { 0.0f };

    // ── Look-ahead circular delay (integer-sample only) ───────────────────────
    std::vector<float> mDelayL, mDelayR;
    int   mDelaySize    { 0 };   // buffer capacity (samples)
    int   mWritePos     { 0 };
    int   mAheadSamples { 0 };

    // ── 4× oversampled TP detector (stereo) ───────────────────────────────────
    std::unique_ptr<juce::dsp::Oversampling<float>> mOversampler;
    juce::AudioBuffer<float> mScBuf;       // pre-allocated detector sidechain buffer (stereo, mMaxBlock samples)
    std::vector<float>       mTpPeaks;     // per-sample TP estimate (sized to mMaxBlock) -- linked (max L/R)
    // C5: unlinked mode writes per-channel TP peaks here. Only populated when
    // mStereoLink == false; otherwise left untouched.
    std::vector<float>       mTpPeaksL, mTpPeaksR;
    int   mOsLatencySamples { 0 };
    int   mLatencySamples   { 0 };         // aheadSamples + osLatency

    // ── Meters (atomic; updated once per block, with hold+decay) ──────────────
    // A2/A3 -- hold+decay so transient in/out dBFS and GR are caught between
    // 30 Hz UI polls (audio runs at 86+ blocks/sec so overwrite-only stores
    // miss ~2-in-3 peaks). max(thisBlock, prev +/- decayPerBlock).
    std::atomic<float> mInputDb  { -96.0f };
    std::atomic<float> mOutputDb { -96.0f };
    std::atomic<float> mGrDb     { 0.0f };
    float mLevelDecayDbPerBlock { 0.35f };   // for mInputDb / mOutputDb (falls toward -96)
    float mGrDecayDbPerBlock    { 0.35f };   // for mGrDb (rises toward 0 - GR is <=0)

    // ── TS7 servo + meter state ───────────────────────────────────────────────
    // Servo values are atomics because the UI reads them while the audio thread
    // writes them; both are single-writer so a plain store/load is sufficient.
    std::atomic<float> mServoDb      { 0.0f };   // CL-244 input-gain trim
    std::atomic<float> mCeilingTrimDb{ 0.0f };   // BLU-108 ceiling trim (<= 0)
    std::atomic<float> mOutTpDb      { -144.0f };
    std::atomic<float> mOutLufsM     { -120.0f };
    std::atomic<float> mOutLufsS     { -120.0f };
    std::atomic<float> mOutLufsI     { -120.0f };

    // BLU-110 watchdog: blocks remaining before the display meter lapses.  Poked
    // by the panel's timer, decremented by process().  See pokeLufsMeter().
    std::atomic<int> mLufsHold      { 0 };
    // Message-thread request for an integrated-window reset, consumed by
    // process() -- the meter itself is audio-thread-only by contract.
    std::atomic<bool> mLufsResetPending { false };
    int              mLufsHoldBlocks { 32 };   // recomputed in prepare() for ~0.5 s

    // CL-244 / BLU-110: loudness of the limiter's OWN OUTPUT.  Only processed
    // while the target servo is on or the panel watchdog is holding it up
    // (mLoudnessTargetOn || mLufsHold > 0).
    LufsMeterDSP  mOutLufs;
    // BLU-108: true peak of the output.  Only processed when mAutoCeiling.
    TruePeakMeter mOutTp;
    bool          mLufsPrepared { false };

    // Servo rate limits.  CALIBRATION (Rule 6 cat. 5): 1.5 dB/s is slow enough
    // that a loudness servo never reads as gain-riding on musical material, and
    // fast enough to settle inside the couple of seconds a short-term window
    // needs to fill.  +-12 dB of authority covers a badly-gain-staged mix
    // without letting a mistake run away.
    static constexpr float kServoDbPerSec  = 1.5f;
    static constexpr float kServoRangeDb   = 12.0f;
    // Ceiling trim recovers slower than it engages: pulling down late clips,
    // letting go early re-clips.  Asymmetric on purpose.
    static constexpr float kTrimDownDbPerSec = 3.0f;
    static constexpr float kTrimUpDbPerSec   = 0.5f;
    static constexpr float kTrimMaxDb        = 6.0f;   // trim floor, as magnitude

    // TS7: is the maximizer half live?  Gating on this rather than zeroing the
    // stored values means Limiter mode is the pure FL reproduction AND flipping
    // back to Maximizer restores the user's target, character and ceiling
    // settings intact.  Zeroing them on the way out would silently discard work;
    // leaving them ungated would let hidden state drive the sound, which is the
    // defect class this codebase keeps paying for.
    bool maximizerActive() const noexcept { return mMode == Mode::Maximizer; }
    // The character in force.  Limiter mode always runs Clean -- the pre-TS7
    // ballistics -- whatever the stored character says.
    Character effectiveCharacter() const noexcept
        { return maximizerActive() ? mCharacter : Character::Clean; }

    // ── Helpers ───────────────────────────────────────────────────────────────
    void recalcCoefs();       // attack/release coefs from times
    void allocateDelay();     // size delay line from mAheadMs
    // Once per block, audio thread.  They move mServoDb / mCeilingTrimDb only --
    // deliberately NOT the SmoothedValues, which stay single-writer (message
    // thread).  Full reasoning at their definitions.
    void updateLoudnessServo (int numSamples);   // CL-244
    void updateCeilingTrim   (int numSamples);   // BLU-108
    static float softSat (float x, float drive, float curve);
};
