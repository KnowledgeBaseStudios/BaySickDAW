#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include "EqLinearPhaseProcessor.h"

// -- EQ8DSP -------------------------------------------------------------------
// 8-band stereo parametric EQ.
// Supports multiple filter slopes (up to 4 cascaded biquads per band),
// mute/solo per band, compare banks, and phase mode switching.
//
// 5F-9 sec.12 Phase 1 applied (2026-04-18):
//   12a  getMagnitudeForFrequency() + getMagnitudeForFrequencyDb() for UI curve
//   12b  Proportional Q on Peaking bands (SSL/Neve "hardware" feel; toggle default on)
//   12c  SmoothedValue per band freq/gain/Q + deferred block-rate coef rebuild
//   12d  mIIRModSpeed wired as smoothing ramp length (was dead param)
//
// 5F-9 sec.12 Phase 2 applied (2026-04-19):
//   12h  Per-band M/S routing via Band::channel field (Stereo/Mid/Side/LOnly/ROnly).
//        EQ8MsDSP wrapper kept (Option A) but its inner encode/decode dropped -
//        both inner EQ8DSPs receive full stereo and each band routes internally.
//
// 5F-9 sec.12 Phase 3 applied (2026-04-19):
//   12g  Linear-phase / HQ modes via FFT convolution. PhaseMode (already an
//        existing UI-only enum) now actively drives DSP behaviour:
//          0 Standard      - today's IIR minimum-phase path (unchanged)
//          1 Linear        - linear-phase FFT @ 2048-pt, no oversampling
//          2 HQ Plus       - AC=on forced; no FFT path (= just oversampled IIR)
//          3 HQ Linear     - AC=on forced + linear-phase FFT @ 4096-pt
//          4 HQ Extended   - linear-phase FFT @ 512-pt (low-latency variant)
//        Linear modes use a new EqLinearPhaseProcessor (50%-OLA Hann-windowed
//        FFT) running INSIDE the AC bracket when both are active. IR is built
//        from getMagnitudeForFrequencyStatic() so dynamic-EQ GR is excluded
//        in linear modes (T2b option C - dynamic disabled while linear is on).
//        Per-band M/S routing also restricted to Stereo in linear modes
//        (T2a option B; UI greys per-band channel picker). Latency reported
//        via getLatencySamples() = AC latency + linear-phase latency (FFT/2)
//        and propagates through EQ8MsDSP + VibeGraph::updateBusLatencies into
//        host PDC. Mode-aware fade scaling (T2c) replaces the fixed 256-sample
//        ramp from 12f - new fade length = min(latencyDelta, 4096) samples on
//        every mode change, since linear modes can shift latency by 256-2048
//        samples and the old ramp was too short to mask. PRESET-SAFE additive
//        (phaseMode int defaults to Standard on missing).
//   12f  2x oversampling anti-cramping (opt-in per EQ instance, default off).
//        IIR half-band polyphase oversampler (1 stage = 2x). When enabled,
//        all band processing (biquad + TPT + M/S encode/decode + dynamic GR
//        coef rebuild) runs at 2*sampleRate inside processSamplesUp/Down
//        bracket. Filter coefs designed at 2*sr so the displayed and audible
//        curves match. Detector path (mDetInput snapshot + envelope follower
//        + GR computer) stays at HOST rate - cheaper, no Nyquist warping at
//        audible detector freqs. Spectrum feeds in EQ8MsDSP wrapper stay at
//        host rate (taps live outside this class). PRESET-SAFE additive
//        (mAntiCramping default false; serialised additively). Latency
//        reported via getLatencySamples() so EQ8MsDSP + VibeGraph PDC sum
//        propagates to the host. CPU ~2x when enabled (hence opt-in).
//   12e  TPT (State-Variable, zero-delay-feedback) filter engine for LP/HP/BP
//        band types (types 1, 2, 7). Peaking/Shelf/OFF/Tilt keep biquad because
//        TPT has no native gain control / shelf mode. Notch stays biquad because
//        juce::dsp::StateVariableTPTFilter has no notch mode and BP-subtract is
//        not worth the 2x state cost for a filter type rarely used at extreme Q.
//        Per-band useTPT flag derived from type in updateBand(). PRESET-BREAK
//        pre-v1 (slight sonic shift on LP/HP/BP at high Q near Nyquist where
//        biquad cascades warp their coefficients; TPT keeps the analog shape).
//   12j  Full Dynamic EQ. Per-band dynamic / threshold / ratio / attack /
//        release / range / upward fields. Detector (bandpass @ band freq + Q)
//        feeds envelope follower with asymmetric att/rel time constants;
//        gain computer applies downward compression (envDb > threshold) or
//        upward expansion (envDb < threshold when upward=true) with ratio +
//        range clamp. Block-rate coef rebuild applies effective gain (params.
//        gainDb + currentGrDb) to the filter. Supported only on gain-bearing
//        types (Peaking=0, LowShelf=3, HighShelf=4, Tilt=8). PRESET-SAFE
//        (dynamic=false default preserves v1 behavior exactly). Per-band
//        scSourceId APVTS scaffolding also added for future external-sidechain
//        routing (Tier 3 T11); DSP today always reads internal band input.
//
// Phase A retrospective:
//   A1  juce::ScopedNoDenormals in process()
//   A2  CPU guards on band setters
//   A3  CPU guards on misc setters
//   A6  mSpareLocked made private (was inconsistently public)
//   A8  anySoloed cached per-block (was O(n^2) via per-band bandShouldProcess)
//
// Phase 2/3 deferred to dedicated sessions (per spec's size):
//   12e TPT filter hybrid (LP/HP/BP/Notch)
//   12f 2x oversampling anti-cramping
//   12g Linear-phase / HQ modes (substantial DSP week)
//   12i Spectrum analyzer overlay (Session A companion to 12h - see EQ8MsDSP)
//   12j Full Dynamic EQ (per-band detector + envelope + gain computer)
// -----------------------------------------------------------------------------
class EQ8DSP : public DSPBase
{
public:
    static constexpr int kNumBands    = 8;

    // 2026-04-22 §P4.3 perf: returns true if every band is mathematically a
    // pass-through (identity) at the moment.  Callers should skip process()
    // entirely when this is true - no IIR work, no filter coefficient updates,
    // no per-sample loop.  Cheap to call (8 small comparisons).
    bool isIdentity() const noexcept;
    static constexpr int kMaxSections = 4;

    // 12h: per-band channel routing. Stereo = today's behaviour. Mid / Side
    // encode in place on-demand (only when any band uses them). LOnly / ROnly
    // process only one channel of the stereo pair, leaving the other untouched.
    enum Channel { Stereo = 0, Mid = 1, Side = 2, LOnly = 3, ROnly = 4 };

    // filter types: 0=Peaking, 1=LP, 2=HP, 3=LowShelf, 4=HiShelf, 5=Notch, 6=OFF, 7=BandPass, 8=Tilt
    // slope:  0=Center-2 (1 section)
    //         1=Steep-4  (2 sections, Butterworth Q)
    //         2=Steep-6  (3 sections)
    //         3=Steep-8  (4 sections)
    //         4=Gentle-4 (2 sections, LR-style Q)
    //         5=Gentle-6 (3 sections)
    //         6=Gentle-8 (4 sections)
    struct Band {
        float freq   { 1000.0f };
        float gainDb { 0.0f };
        float q      { 0.707f };
        int   type   { 0 };
        int   slope  { 0 };
        bool  on     { true };
        bool  muted  { false };
        bool  soloed { false };
        int   channel{ Stereo };   // 12h: 0=Stereo 1=Mid 2=Side 3=LOnly 4=ROnly

        // 12j Dynamic EQ: enabled only on gain-bearing types (Peaking / LowShelf
        // / HighShelf / Tilt). dynamic=false default preserves v1 behaviour.
        bool  dynamic    { false };
        float threshold  { -18.0f };  // dB (-60..0)
        float ratio      { 2.0f };    // >=1; 1 = no effect (1..20)
        // C.4 follow-up (2026-04-30): rangeDb default 0 (was 12) so dynamic
        // bands start with no modulation -- matches the new APVTS default
        // and the UI Band mirror; user explicitly dials in the range.
        float attack     { 10.0f };   // ms (0.1..500)
        float release    { 100.0f };  // ms (1..2000)
        float rangeDb    { 0.0f };    // signed dB (-18..+18); 0 = no modulation
        bool  upward     { false };   // false = compress above thr; true = expand below thr

        // C.4 Phase 1 (2026-04-30): slot index into the strip's SC receive
        // array (0..3).  -1 = internal (band's own input).  When >= 0 the
        // detector swaps to mScBufs[scSourceId] so dynamic-EQ gain reduction
        // is driven by the chosen sidechain source.
        int   scSourceId { -1 };
    };

    enum class PhaseMode { Standard, Linear, HQPlus, HQL, HQE };

    EQ8DSP();
    ~EQ8DSP() override = default;

    // DSPBase interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset   ()                                     override;
    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;

    // ---- Band setters (12c: deferred coef rebuild via smoothers + dirty flag) --
    void setBand      (int i, const Band& b);
    void setBandFreq  (int i, float freq);
    void setBandGain  (int i, float gainDb);
    void setBandQ     (int i, float q);
    void setBandType  (int i, int type);
    void setBandSlope (int i, int slope);
    void setBandOn    (int i, bool on);
    void setBandMuted (int i, bool muted);
    void setBandSoloed(int i, bool soloed);
    void setBandChannel(int i, int channel);   // 12h: 0..4 per Channel enum
    void setBandKey   (int i, int midiNote);   // freq = 440*2^((note-69)/12)

    // 12j Dynamic EQ setters (PRESET-SAFE additive).
    void setBandDynamic  (int i, bool dynamic);
    void setBandThreshold(int i, float dB);
    void setBandRatio    (int i, float ratio);
    void setBandAttack   (int i, float ms);
    void setBandRelease  (int i, float ms);
    void setBandRange    (int i, float dB);
    void setBandUpward   (int i, bool upward);
    void setBandScSource (int i, int sourceId);   // C.4 Phase 1: live - swaps detector source to mScBufs[sourceId]

    // 12j: UI polling - current gain reduction (dB, signed: negative = downward
    // compression, positive = upward expansion). Wait-free atomic read.
    float getBandGrDb (int i) const noexcept;
    // 12j: UI ghost-range endpoint - returns the max-reduction or max-expansion
    // gain (params.gainDb + sign*range) so the widget can draw a static outline
    // of "how far this band CAN move" alongside the live animated curve.
    float getBandEffectiveGainDbAtRangeLimit(int i) const noexcept;

    Band getBand(int i) const;

    // ---- Main output level ---------------------------------------------------
    void  setMainLevel(float dB);
    float getMainLevel() const noexcept { return mMainLevelDb; }

    // ---- Phase / quality mode -----------------------------------------------
    // 12g: setPhaseMode now actively reconfigures the DSP. Allocates / frees
    // the linear-phase processor as needed, force-enables AC for HQ Plus and
    // HQ Linear, marks IR dirty, arms a mode-aware output fade.
    void setPhaseMode(PhaseMode mode);
    PhaseMode getPhaseMode() const noexcept { return mPhaseMode; }
    bool      isLinearPhaseMode() const noexcept
    {
        return mPhaseMode == PhaseMode::Linear
            || mPhaseMode == PhaseMode::HQL
            || mPhaseMode == PhaseMode::HQE;
    }
    void setLinearPhasePrecision(int precision);  // 0-4 maps to FFT 256/512/1024/2048/4096
    int  getLinearPhasePrecision() const noexcept { return mLinearPrec; }

    // ---- IIR parameter smoothing (12d: wired to smoother ramp length) -------
    // 0 = instant (~1 ms), 1 = slow (~50 ms).
    void setIIRModSpeed(float speed01);
    float getIIRModSpeed() const noexcept { return mIIRModSpeed; }

    // 12b: Proportional Q toggle. When true, Peaking bands narrow their Q as
    // gain rises (SSL/Neve hardware feel). Default true. Has no effect on
    // shelves / filters / notch / bandpass.
    void setProportionalQ(bool on);
    bool getProportionalQ() const noexcept { return mProportionalQ; }

    // 12f: anti-cramping 2x oversampling toggle (opt-in per instance, default off).
    // Reports latency via getLatencySamples() - host PDC must be refreshed by the
    // caller (EffectsPage already drives VibeGraph::updateBusLatencies on rack
    // changes; toggle handler in showEQOptionsMenu does the same).
    void setAntiCramping(bool on);
    bool isAntiCramping() const noexcept { return mAntiCramping; }

    // 12f: PDC reporting. Returns the oversampler's integer-rounded latency
    // when AC is on, 0 otherwise.
    int  getLatencySamples() const override;

    // ---- Compare banks ------------------------------------------------------
    void saveToSpare();     // copy current params -> spare bank
    void swapWithSpare();   // toggle between main and spare
    void lockSpare(bool locked);

    bool isViewingSpare() const noexcept { return mViewingSpare; }
    bool isSpareLocked () const noexcept { return mSpareLocked;  }

    // ---- 12a: magnitude queries for UI curve drawing ------------------------
    // Returns combined linear magnitude at the given frequency, across every
    // active section of every enabled band, multiplied by the main-level trim.
    // Thread-safe against audio thread (const read of coefficient pointers with
    // null-guard for OFF / unused-section filters).
    float getMagnitudeForFrequency   (float freq) const noexcept;
    float getMagnitudeForFrequencyDb (float freq) const noexcept;

private:
    using IIRFilter = juce::dsp::IIR::Filter<float>;
    using Coeffs    = juce::dsp::IIR::Coefficients<float>;

    struct BandState {
        Band params;
        std::array<IIRFilter, kMaxSections> filtersL;
        std::array<IIRFilter, kMaxSections> filtersR;
        // 12e: parallel TPT filter arrays used only when params.type is LP/HP/BP.
        // Biquad arrays stay allocated but idle for those bands (coefs nulled in
        // updateBand). One TPT instance per channel per section - matches the
        // biquad structure so per-band channel-routing state reset (12h) is
        // symmetric. ~200 bytes per band x 8 bands = ~1.6 KB per EQ instance.
        std::array<juce::dsp::StateVariableTPTFilter<float>, kMaxSections> tptL;
        std::array<juce::dsp::StateVariableTPTFilter<float>, kMaxSections> tptR;
        int activeSections { 1 };

        // 12e: true when this band's type uses the TPT engine (LP/HP/BP).
        // Derived from params.type in updateBand(); not serialised.
        bool useTPT { false };
        // 12e: per-section effective cutoff + resonance - used by
        // getMagnitudeForFrequency's TPT branch since StateVariableTPTFilter
        // doesn't expose a magnitude-query method directly.
        std::array<float, kMaxSections> tptCutoffHz   {};
        std::array<float, kMaxSections> tptResonance  {};

        // 12c: SmoothedValue per band - freq/gain/Q smoothed at block rate.
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> freqSmooth;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainSmooth;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> qSmooth;

        // Dirty flag: any of type/slope/on/muted/soloed changed since last rebuild,
        // OR any of freq/gain/Q smoother target differs from last-applied value.
        bool dirty { true };

        // Last applied values (to detect smoother-driven changes worth rebuilding for).
        float lastAppliedFreq   { 0.0f };
        float lastAppliedGainDb { 0.0f };
        float lastAppliedQ      { 0.0f };

        // 12j Dynamic EQ state. Detector is a bandpass at band.freq / band.q
        // that isolates the band's frequency region for envelope detection -
        // separate from the main filter so dynamic behaviour is driven by
        // content energy in the band, independent of filter type. One detector
        // biquad per channel (L + R mirrored; mono source for M/S routings).
        IIRFilter detFilterL;
        IIRFilter detFilterR;
        // Envelope follower state (sample-delayed smoothed detector level).
        float envL                  { 0.0f };
        float envR                  { 0.0f };
        // Current GR in dB, signed (negative = downward compression, positive
        // = upward expansion). Atomic for UI polling. Updated per block.
        std::atomic<float> currentGrDb { 0.0f };
        // Block-rate effective gain (params.gainDb + currentGrDb). Tracked so
        // we only rebuild coefs when |effectiveGain - lastApplied| > tolerance.
        float lastAppliedEffectiveGainDb { 0.0f };
    };

    std::array<BandState, kNumBands> mBands;
    Band mSpare[kNumBands];
    bool mViewingSpare { false };
    bool mSpareLocked  { false };   // A6: was public; now private with isSpareLocked()

    float     mMainLevelDb   { 0.0f };
    PhaseMode mPhaseMode     { PhaseMode::Standard };
    int       mLinearPrec    { 2 };
    float     mIIRModSpeed   { 1.0f };   // 12d: wired to smoother ramp time
    bool      mProportionalQ { true };   // 12b: default on - SSL/Neve "hardware" feel
    bool      mAntiCramping  { false };  // 12f: opt-in 2x oversampling

    // 12f: oversampler. Allocated in prepare() with 2 channels, 1 stage IIR
    // half-band polyphase (lowest-latency option), useIntegerLatency=true so
    // getLatencyInSamples() returns an integer-padded sample count for clean
    // PDC reporting.
    std::unique_ptr<juce::dsp::Oversampling<float>> mOversampler;

    // 12f: short post-toggle output fade (samples remaining). The AC flip
    // changes reported latency mid-stream which the host realigns by shifting
    // its PDC compensation - the resulting one-block discontinuity is audible
    // as a click on a sustained note. Linear-ramp the EQ output over the first
    // mAcFadeTarget after a toggle to soften the seam in both directions.
    // 12g: mode-aware fade scaling (T2c) - target = min(latencyDelta, kMaxFade)
    // recomputed at every AC or phase-mode change. AC alone uses ~256 samples
    // (latency delta ~5-8 samples - 256 is the minimum); linear modes scale
    // up to 4096 samples to mask the much larger latency shift.
    static constexpr int kMaxFadeSamples = 4096;
    static constexpr int kMinFadeSamples = 256;
    int mAcFadeRemaining { 0 };
    int mAcFadeTarget    { 0 };

    // 12g: linear-phase processor (one per EQ instance, owns its own ring +
    // FFT). Allocated lazily by setPhaseMode when a linear mode is selected,
    // freed when reverting to Standard / HQ Plus.
    EqLinearPhaseProcessor mLinearProc;
    bool mLinearIRDirty { true };

    // 12g: helper to (re)configure mLinearProc for the current phase mode +
    // sample rate + design rate (AC composition). Called from setPhaseMode
    // and from prepare() when re-init crosses a mode boundary.
    void reconfigureLinearProc();
    // 12g: rebuild the linear-phase IR from the current static band design
    // (no dynamic GR per T2b option C). Cheap: N/2+1 magnitude callback calls.
    void rebuildLinearIR();
    // 12g: static magnitude lookup at a given freq, using design gain only
    // (skips dynamic GR injection). Used as the magnitude callback for the
    // linear-phase IR design. Mirrors getMagnitudeForFrequency but ignores
    // bs.params.dynamic and falls back to the regular biquad / TPT magnitude
    // formula with bs.params.gainDb.
    float magnitudeForFrequencyStatic (float freq) const noexcept;

    // A8: any-soloed flag cached per-block (computed once at start of process()
    // instead of re-scanning 8 bands inside bandShouldProcess for every band).
    bool mAnySoloedCached { false };

    // 12h: scratch buffers for Mid/Side encode/decode. Pre-allocated in prepare()
    // to avoid audio-thread heap allocation. Only touched when any enabled band
    // has channel == Mid or channel == Side.
    juce::AudioBuffer<float> mMsScratch;   // 2 channels: [0]=M, [1]=S

    // 12j: saved-original-input scratch for dynamic-band detectors. Parallel-
    // detector model (FabFilter Pro-Q Dynamic style): every dynamic band's
    // detector sees the ORIGINAL block input regardless of the in-series main
    // filter order, so band N's GR doesn't affect band N's own detection. Only
    // populated when at least one enabled band has dynamic=true.
    juce::AudioBuffer<float> mDetInput;    // 2 channels: [0]=L, [1]=R

    // 12f: design-rate helper. Coefficients are computed at this rate so they
    // map correctly to the (host vs oversampled) processing rate. Detector path
    // does NOT use this - detector stays at host rate.
    float designSr() const noexcept
    {
        return mAntiCramping ? 2.0f * (float) mSampleRate : (float) mSampleRate;
    }

    // 12f: band-dispatch loop extracted so it can run on either the host-rate
    // buffer (AC off) or the oversampler's upsampled block (AC on). Operates on
    // a juce::dsp::AudioBlock view so both cases share the same code.
    void processBands(juce::dsp::AudioBlock<float>& block);

    // ---- Private helpers ---------------------------------------------------
    // 12j: updateBand optionally accepts an effective-gain override (used by
    // dynamic bands so the coef rebuild applies GR without disturbing the gain
    // smoother state). NaN = read smoother value as before (non-dynamic path).
    void  updateBand(int i, float gainOverrideDb = std::numeric_limits<float>::quiet_NaN());
    void  updateAllBands();
    void  refreshSmootherRamps();   // 12d: recompute ramp length from mIIRModSpeed
    void  snapBandSmoothersToParams(int i);
    int   getSectionCount(int slope) const;
    bool  anySoloed() const;
    bool  bandShouldProcess(int bi) const;

    // Builds one biquad section for a given type/freq/gain/Q.
    Coeffs::Ptr makeOneSection(float sr, float freq, float gainDb,
                               float q, int type) const;

    // 12b helper: returns the effective Q to use for a given band's section,
    // applying proportional-Q narrowing on Peaking bands when enabled.
    float effectiveQ(float userQ, float gainDb, int type) const noexcept;

    // 12j: Dynamic EQ helpers.
    // Rebuilds the detector bandpass biquad from the band's current freq + Q.
    // Called when a dynamic band's freq/q/dynamic-flag changes.
    void  updateDetector (int i);
    // Processes dynamic bands for the current block: runs detectors over the
    // saved input scratch, advances envelope followers, computes GR, and
    // rebuilds the band's main filter coefs with the effective gain. No-op
    // when the band is not dynamic or its type is not gain-bearing.
    void  processDynamic (int i, int numSamples);
    // True when the band's type supports dynamic mode (Peaking / LowShelf /
    // HighShelf / Tilt). Other types ignore the `dynamic` flag.
    bool  isDynamicSupportedType (int type) const noexcept;
};
