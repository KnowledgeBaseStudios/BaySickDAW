#include "EQ8DSP.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)

// -- Default band parameters --------------------------------------------------
// 2026-04-19: aligned with kEQDefaultFreqs in Source/Standalone/SharedUI.cpp
// (widget-side reset / double-click defaults). Was 80/200/500/2k/6k/16k/8k/4k
// which drifted from the widget side and caused B-bank loads to show different
// per-band frequencies than A (the DSP defaults captured in the spare bank
// before APVTS write-back populated DSP main with the widget defaults). Both
// arrays now must stay in sync.
static constexpr float kDefaultFreqs8[8] = {
    40.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 12000.f };
static constexpr int kDefaultTypes8[8] = {
    0, 0, 0, 0, 0, 0, 0, 0 };  // all Bell - flat starting point

// Butterworth Q values for cascaded Steep slopes - Index [numSections-1][sectionIndex]
static constexpr float kSteepQ[4][4] = {
    { 0.7071f, 0.0f,    0.0f,    0.0f   },  // 1 section (not used - slope 0)
    { 0.5412f, 1.3066f, 0.0f,    0.0f   },  // 2 sections (slope 1: Steep-4)
    { 0.5176f, 0.7071f, 1.9318f, 0.0f   },  // 3 sections (slope 2: Steep-6)
    { 0.5098f, 0.6013f, 0.9000f, 2.5628f }  // 4 sections (slope 3: Steep-8)
};

// Gentle (Linkwitz-Riley style): all sections use Q = 1/sqrt(2)
static constexpr float kGentleQ = 0.7071f;

// -- Construction -------------------------------------------------------------

EQ8DSP::EQ8DSP()
{
    for (int i = 0; i < kNumBands; ++i)
    {
        mBands[i].params.freq   = mSpare[i].freq   = kDefaultFreqs8[i];
        mBands[i].params.gainDb = mSpare[i].gainDb = 0.0f;
        mBands[i].params.q      = mSpare[i].q      = 0.707f;
        mBands[i].params.type   = mSpare[i].type   = kDefaultTypes8[i];
        mBands[i].params.slope  = mSpare[i].slope  = 0;
        mBands[i].params.on     = mSpare[i].on     = true;
        mBands[i].params.muted  = mSpare[i].muted  = false;
        mBands[i].params.soloed = mSpare[i].soloed = false;
        mBands[i].dirty = true;
    }
}

// -- DSPBase interface --------------------------------------------------------

void EQ8DSP::prepare(double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;

    // 12f: allocate / reallocate the oversampler. 2 channels, 1 stage = 2x,
    // IIR half-band polyphase (lowest latency option), useIntegerLatency=true
    // so PDC reports a clean integer sample count.
    mOversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        (size_t) 2, (size_t) 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true /*isMaximumQuality*/, true /*useIntegerLatency*/);
    if (maxBlockSize > 0)
        mOversampler->initProcessing((size_t) maxBlockSize);

    // 12f: filters and TPT instances are prepared at the DESIGN rate. When AC
    // is on, that's 2*sr; otherwise host sr. setAntiCramping() re-runs prepare()
    // to keep these in sync if the toggle flips later.
    const double designRate = mAntiCramping ? 2.0 * sampleRate : sampleRate;
    const int    designBlk  = mAntiCramping ? maxBlockSize * 2 : maxBlockSize;

    // CRITICAL: IIR::Filter::prepare() internally calls reset() which dereferences
    // coefficients->getFilterOrder(). At construction the pointer is null, so the
    // first prepare() crashes unless we seed the coefficients first.
    auto unity = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                     designRate > 0.0 ? designRate : 44100.0,
                     1000.0f, 0.7071f, 1.0f);  // flat 0 dB peak = pass-through

    juce::dsp::ProcessSpec spec { designRate, (juce::uint32) designBlk, 1 };
    for (auto& bs : mBands)
    {
        for (auto& f : bs.filtersL) { f.coefficients = unity; f.prepare(spec); }
        for (auto& f : bs.filtersR) { f.coefficients = unity; f.prepare(spec); }
        // 12e: prepare TPT filter arrays with a unity-ish pass-through default
        // (lowpass at 20 kHz, Q=0.7071). Actual cutoff/mode/resonance are set in
        // updateBand() when a band's type is LP/HP/BP. setCutoffFrequency /
        // setResonance would NaN if sampleRate == 0, so only seed defaults when
        // the host has given us a real rate; updateBand's `mSampleRate <= 0`
        // guard already handles the uninitialized path safely.
        for (auto& t : bs.tptL)
        {
            t.prepare(spec);
            if (designRate > 0.0)
            {
                t.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
                t.setCutoffFrequency(20000.0f);
                t.setResonance(0.7071f);
            }
        }
        for (auto& t : bs.tptR)
        {
            t.prepare(spec);
            if (designRate > 0.0)
            {
                t.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
                t.setCutoffFrequency(20000.0f);
                t.setResonance(0.7071f);
            }
        }
    }

    // 12h: pre-allocate M/S scratch buffers so process() never hits the heap.
    // 12f: sized for the worst case (AC on -> 2x block) so the toggle never
    // requires reallocation on the audio thread.
    mMsScratch.setSize(2, maxBlockSize * 2, false, true, true);

    // 12j: pre-allocate detector-input scratch (saved-original-input for the
    // parallel-detector model) and prepare the per-band detector biquads.
    // 12f: detector path stays at HOST rate, so this stays sized for maxBlockSize
    // and detector biquads are prepared with host sampleRate (NOT designRate).
    mDetInput.setSize(2, maxBlockSize, false, true, true);
    juce::dsp::ProcessSpec detSpec { sampleRate, (juce::uint32)maxBlockSize, 1 };
    for (auto& bs : mBands)
    {
        // Seed detector with a unity-ish bandpass at 1 kHz. updateDetector()
        // rewrites these coefs when dynamic is enabled or band freq/Q changes.
        if (sampleRate > 0.0)
        {
            auto unityBp = juce::dsp::IIR::Coefficients<float>::makeBandPass(
                               sampleRate, 1000.0f, 0.707f);
            bs.detFilterL.coefficients = unityBp;
            bs.detFilterR.coefficients = unityBp;
        }
        bs.detFilterL.prepare(detSpec);
        bs.detFilterR.prepare(detSpec);
        bs.envL = 0.0f;
        bs.envR = 0.0f;
        bs.currentGrDb.store(0.0f, std::memory_order_relaxed);
        bs.lastAppliedEffectiveGainDb = bs.params.gainDb;
    }

    // 12c + 12d: initialise smoothers with ramp length derived from mIIRModSpeed.
    refreshSmootherRamps();
    for (int i = 0; i < kNumBands; ++i)
    {
        snapBandSmoothersToParams(i);
        mBands[i].dirty = true;
    }

    updateAllBands();

    // 12g: configure the linear-phase processor for the current phase mode +
    // design rate. No-op (frees) when in Standard / HQ Plus.
    reconfigureLinearProc();
}

void EQ8DSP::reset()
{
    // Guard against null coefficients on OFF-band / unused-section filters.
    for (auto& bs : mBands)
    {
        for (auto& f : bs.filtersL) if (f.coefficients != nullptr) f.reset();
        for (auto& f : bs.filtersR) if (f.coefficients != nullptr) f.reset();
        // 12e: TPT filters always have state (no null-coef equivalent).
        for (auto& t : bs.tptL) t.reset();
        for (auto& t : bs.tptR) t.reset();
    }
}

// 2026-04-22 §P4.3 perf: per-band identity check.  A band is identity (signal
// passes through unchanged) when ANY of these are true:
//   - !on (band switched off)
//   - muted
//   - dynamic disabled AND it's a gain-bearing type (Bell/Shelf/Tilt) at 0 dB
// Filter types (LP/HP/BP/Notch) are never treated as identity here even at
// extreme cutoffs - conservative: they always count as "doing something".
// The OFF type (6) is always identity by definition.
//
// Returns true if every band is identity (so process() can be skipped entirely).
bool EQ8DSP::isIdentity() const noexcept
{
    constexpr float kZeroEpsilon = 0.001f;   // sub-millibel; user can't hear
    // D.4-Q6 fix (2026-05-01): mainLevel != 0 must NOT short-circuit the
    // EQ to bypass - the multiply happens at the end of process() so the
    // early-return path skipped it.  All-bands-flat with mainLevel = +6 dB
    // previously produced 0 dB output.  Now any non-zero mainLevel keeps
    // process() running so the gain is applied.
    if (std::abs (mMainLevelDb) >= kZeroEpsilon) return false;
    for (int i = 0; i < kNumBands; ++i)
    {
        const auto& b = mBands[i].params;
        if (!b.on)    continue;          // off = bypass = identity
        if (b.muted)  continue;          // muted = identity
        if (b.type == 6) continue;       // explicit OFF
        if (b.dynamic) return false;     // dynamic EQ produces signal-dependent gain
        const bool gainType = (b.type == 0 /*Bell*/ || b.type == 3 /*LowShelf*/
                            || b.type == 4 /*HighShelf*/ || b.type == 8 /*Tilt*/);
        if (gainType && std::abs(b.gainDb) < kZeroEpsilon) continue;   // identity
        return false;                    // any non-identity band kills early
    }
    return true;
}

void EQ8DSP::process(juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;

    juce::ScopedNoDenormals noDenormals;   // A1

    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();
    float* L = numCh > 0 ? buffer.getWritePointer(0) : nullptr;
    float* R = numCh > 1 ? buffer.getWritePointer(1) : nullptr;

    // A8: cache any-soloed once per block instead of re-scanning in every bandShouldProcess call.
    mAnySoloedCached = anySoloed();

    // 12f: detector pass runs at HOST rate, BEFORE the oversampling bracket.
    // Detectors read mDetInput at host sr, so no rate adjustment is needed in
    // their envelope-coef math. The dynamic-band coef rebuild happens below in
    // the smoother loop and will use designSr() when it builds the main filter.
    // (Same code as before; comment expanded for 12f context.)

    // 12j: save a copy of the ORIGINAL input before any band filtering, so
    // dynamic-band detectors all see the pristine signal (parallel-detector
    // model, a la FabFilter Pro-Q Dynamic). Skip the copy if no band is
    // dynamic - hot path stays allocation-free and memcpy-free in the common
    // case of a static EQ.
    bool anyDynamic = false;
    for (int i = 0; i < kNumBands; ++i)
    {
        const auto& p = mBands[i].params;
        if (p.dynamic && p.on && !p.muted && isDynamicSupportedType(p.type))
        { anyDynamic = true; break; }
    }
    if (anyDynamic && mDetInput.getNumSamples() >= numSamples)
    {
        if (L) std::copy(L, L + numSamples, mDetInput.getWritePointer(0));
        if (R) std::copy(R, R + numSamples, mDetInput.getWritePointer(1));
        else if (L) std::copy(L, L + numSamples, mDetInput.getWritePointer(1));

        // Run detectors + envelope + GR for every dynamic band, updates
        // bs.currentGrDb which the block-rate rebuild below reads.
        for (int i = 0; i < kNumBands; ++i)
        {
            const auto& p = mBands[i].params;
            if (p.dynamic && p.on && !p.muted && isDynamicSupportedType(p.type))
                processDynamic(i, numSamples);
            else
                mBands[i].currentGrDb.store(0.0f, std::memory_order_relaxed);
        }
    }
    else
    {
        // No dynamic activity this block - clear all GR readouts so stale
        // values don't linger if the user just disabled all dynamics.
        for (int i = 0; i < kNumBands; ++i)
            mBands[i].currentGrDb.store(0.0f, std::memory_order_relaxed);
    }

    // 12c: per-block smoother consumption + deferred coef rebuild. Walk smoother
    // by numSamples at a time (block rate). If the consumed value differs from
    // the last applied or the band is flagged dirty, rebuild coefs once.
    // 12j: effectiveGain = smoothedGain + currentGrDb (when band is dynamic).
    // Trigger a rebuild whenever GR moves by more than the tolerance.
    for (int i = 0; i < kNumBands; ++i)
    {
        auto& bs = mBands[i];

        // Advance smoothers by numSamples each. skip() returns void; getCurrentValue()
        // reads the value after the skip.
        bs.freqSmooth.skip (numSamples);
        bs.gainSmooth.skip (numSamples);
        bs.qSmooth   .skip (numSamples);
        const float fNow = bs.freqSmooth.getCurrentValue();
        const float gNow = bs.gainSmooth.getCurrentValue();
        const float qNow = bs.qSmooth   .getCurrentValue();

        // 12j: effective gain composes smoothed gain with dynamic GR.
        const float grDb        = (bs.params.dynamic && isDynamicSupportedType(bs.params.type))
                                      ? bs.currentGrDb.load(std::memory_order_relaxed)
                                      : 0.0f;
        const float effectiveG  = gNow + grDb;
        const bool  grMoved     = std::abs(effectiveG - bs.lastAppliedEffectiveGainDb) > 1e-3f;

        const bool smootherMoved = std::abs(fNow - bs.lastAppliedFreq)   > 1e-4f
                                 || std::abs(gNow - bs.lastAppliedGainDb)> 1e-4f
                                 || std::abs(qNow - bs.lastAppliedQ)     > 1e-5f;

        if (bs.dirty || smootherMoved || grMoved)
        {
            // Pass the effective gain override directly so the gain smoother
            // state stays intact (its target + ramp toward user-setpoint).
            updateBand(i, (grDb != 0.0f) ? effectiveG
                                         : std::numeric_limits<float>::quiet_NaN());

            bs.lastAppliedFreq              = fNow;
            bs.lastAppliedGainDb            = gNow;
            bs.lastAppliedQ                 = qNow;
            bs.lastAppliedEffectiveGainDb   = effectiveG;
            bs.dirty = false;
        }
    }

    // 12f + 12g: dispatch through (a) optional oversampling bracket and (b)
    // either the per-band IIR/TPT path (Standard / HQ Plus) or the linear-
    // phase FFT path (Linear / HQ Linear / HQ Extended). Linear modes ignore
    // per-band M/S routing (T2a option B) and dynamic GR (T2b option C) - the
    // IR is built from magnitudeForFrequencyStatic() at every dirty mark.
    juce::dsp::AudioBlock<float> hostBlock (buffer);

    auto runFilter = [&] (juce::dsp::AudioBlock<float>& block)
    {
        if (isLinearPhaseMode())
        {
            if (mLinearIRDirty) rebuildLinearIR();
            // IR rebuild also needs to fire when any band moved this block:
            // band setters mark BandState::dirty which our smoother loop above
            // uses to rebuild biquad/TPT coefs. For linear-phase we don't need
            // those coefs themselves; we just need a flag to know "something
            // changed - rebuild IR." Cheaper to refresh every time the
            // smoother loop touched anything: scan and rebuild if any dirty.
            for (int i = 0; i < kNumBands; ++i)
            {
                if (mBands[i].dirty) { mLinearIRDirty = true; break; }
            }
            if (mLinearIRDirty) rebuildLinearIR();
            mLinearProc.process (block);
        }
        else
        {
            processBands (block);
        }
    };

    if (mAntiCramping && mOversampler && numCh >= 1)
    {
        auto upBlock = mOversampler->processSamplesUp (hostBlock);
        runFilter (upBlock);
        mOversampler->processSamplesDown (hostBlock);
    }
    else
    {
        runFilter (hostBlock);
    }

    // 12f: post-toggle output fade to mask the latency-shift discontinuity.
    // Linear ramp from current progress through mAcFadeTarget; consumes
    // numSamples per block. Both L and R get the same ramp for stereo coherence.
    if (mAcFadeRemaining > 0 && mAcFadeTarget > 0)
    {
        const int n = juce::jmin (numSamples, mAcFadeRemaining);
        const float total = (float) mAcFadeTarget;
        const int   start = mAcFadeTarget - mAcFadeRemaining;   // 0..mAcFadeTarget-1
        for (int i = 0; i < n; ++i)
        {
            const float gain = (float)(start + i) / total;
            if (L) L[i] *= gain;
            if (R) R[i] *= gain;
        }
        mAcFadeRemaining -= n;
    }

    // Main level (host rate, post-bracket).
    if (mMainLevelDb != 0.0f)
    {
        float g = juce::Decibels::decibelsToGain(mMainLevelDb);
        if (L) juce::FloatVectorOperations::multiply(L, g, numSamples);
        if (R) juce::FloatVectorOperations::multiply(R, g, numSamples);
    }
}

// 12f: band-dispatch loop extracted from process() so it can run on either
// the host-rate buffer (AC off) or the oversampler's upsampled block (AC on).
// All references to numSamples / L / R now read from `block`. Filter state +
// coefficients are designed at designSr() (which is 2*sr when AC is on), so
// the same processSample calls produce a correct result at either rate.
void EQ8DSP::processBands(juce::dsp::AudioBlock<float>& block)
{
    const int numSamples = (int) block.getNumSamples();
    const int numCh      = (int) block.getNumChannels();
    float* L = numCh > 0 ? block.getChannelPointer(0) : nullptr;
    float* R = numCh > 1 ? block.getChannelPointer(1) : nullptr;

    // 12h: per-band channel dispatch. Biquad chains are LTI, so we can group
    // bands by channel domain without changing the resulting transfer function.
    //   - Stereo / LOnly / ROnly bands live in L/R domain: process in place.
    //   - Mid / Side bands live in M/S domain: encode once, process, decode once.

    // L/R-domain pass (Stereo / LOnly / ROnly).
    for (int bi = 0; bi < kNumBands; ++bi)
    {
        if (!bandShouldProcess(bi)) continue;

        auto& bs = mBands[bi];
        const int ch = bs.params.channel;
        if (ch == Channel::Mid || ch == Channel::Side) continue;

        const int  ns   = bs.activeSections;
        const bool useL = (ch == Channel::Stereo || ch == Channel::LOnly);
        const bool useR = (ch == Channel::Stereo || ch == Channel::ROnly);

        if (bs.useTPT)
        {
            for (int s = 0; s < ns; ++s)
            {
                if (useL && L)
                    for (int n = 0; n < numSamples; ++n)
                        L[n] = bs.tptL[s].processSample(0, L[n]);
                if (useR && R)
                    for (int n = 0; n < numSamples; ++n)
                        R[n] = bs.tptR[s].processSample(0, R[n]);
            }
        }
        else
        {
            for (int s = 0; s < ns; ++s)
            {
                if (useL && L && bs.filtersL[s].coefficients)
                    for (int n = 0; n < numSamples; ++n)
                        L[n] = bs.filtersL[s].processSample(L[n]);

                if (useR && R && bs.filtersR[s].coefficients)
                    for (int n = 0; n < numSamples; ++n)
                        R[n] = bs.filtersR[s].processSample(R[n]);
            }
        }
    }

    // M/S-domain pass (Mid / Side). Only runs when any enabled band uses M/S
    // routing AND the block is stereo.
    if (L && R && mMsScratch.getNumSamples() >= numSamples)
    {
        bool needMS = false;
        for (int bi = 0; bi < kNumBands; ++bi)
        {
            if (!bandShouldProcess(bi)) continue;
            const int ch = mBands[bi].params.channel;
            if (ch == Channel::Mid || ch == Channel::Side) { needMS = true; break; }
        }

        if (needMS)
        {
            float* M = mMsScratch.getWritePointer(0);
            float* S = mMsScratch.getWritePointer(1);

            for (int n = 0; n < numSamples; ++n)
            {
                M[n] = 0.5f * (L[n] + R[n]);
                S[n] = 0.5f * (L[n] - R[n]);
            }

            for (int bi = 0; bi < kNumBands; ++bi)
            {
                if (!bandShouldProcess(bi)) continue;
                auto& bs = mBands[bi];
                const int ch = bs.params.channel;
                if (ch != Channel::Mid && ch != Channel::Side) continue;

                float* target = (ch == Channel::Mid) ? M : S;
                const int ns = bs.activeSections;

                if (bs.useTPT)
                {
                    for (int s = 0; s < ns; ++s)
                        for (int n = 0; n < numSamples; ++n)
                            target[n] = bs.tptL[s].processSample(0, target[n]);
                }
                else
                {
                    for (int s = 0; s < ns; ++s)
                    {
                        if (!bs.filtersL[s].coefficients) continue;
                        for (int n = 0; n < numSamples; ++n)
                            target[n] = bs.filtersL[s].processSample(target[n]);
                    }
                }
            }

            for (int n = 0; n < numSamples; ++n)
            {
                L[n] = M[n] + S[n];
                R[n] = M[n] - S[n];
            }
        }
    }
}

// -- Update band DSP coefficients ---------------------------------------------

int EQ8DSP::getSectionCount(int slope) const
{
    // slope 0=1, 1=2, 2=3, 3=4, 4=2(gentle), 5=3(gentle), 6=4(gentle)
    static const int kCount[] = { 1, 2, 3, 4, 2, 3, 4 };
    return kCount[juce::jlimit(0, 6, slope)];
}

// 12b: proportional Q - Peaking bands narrow as gain rises (+-18 dB -> up to 2x narrower).
float EQ8DSP::effectiveQ(float userQ, float gainDb, int type) const noexcept
{
    if (!mProportionalQ || type != 0) return userQ;
    const float gainFactor = 1.0f + std::abs(gainDb) / 18.0f;
    return userQ * gainFactor;
}

EQ8DSP::Coeffs::Ptr EQ8DSP::makeOneSection(float sr, float freq,
                                             float gainDb, float q, int type) const
{
    float f  = juce::jlimit(20.0f, sr * 0.499f, freq);
    float qv = juce::jlimit(0.05f, 20.0f, q);
    float g  = gainDb;

    switch (type)
    {
        case 0:  return Coeffs::makePeakFilter  (sr, f, qv, juce::Decibels::decibelsToGain(g));
        case 1:  return Coeffs::makeLowPass     (sr, f, qv);
        case 2:  return Coeffs::makeHighPass    (sr, f, qv);
        case 3:  return Coeffs::makeLowShelf    (sr, f, qv, juce::Decibels::decibelsToGain(g));
        case 4:  return Coeffs::makeHighShelf   (sr, f, qv, juce::Decibels::decibelsToGain(g));
        case 5:  return Coeffs::makeNotch       (sr, f, qv);
        case 6:  return nullptr;   // OFF - no filter
        case 7:  return Coeffs::makeBandPass    (sr, f, qv);
        default: return Coeffs::makePeakFilter  (sr, f, qv, juce::Decibels::decibelsToGain(g));
    }
}

void EQ8DSP::updateBand(int i, float gainOverrideDb)
{
    if (mSampleRate <= 0.0) return;

    auto& bs = mBands[i];
    auto& p  = bs.params;
    // 12f: main filter coefs designed at designSr() (= 2*host when AC on).
    // Detector coefs stay at HOST sample rate since detectors run pre-bracket.
    const float sr     = designSr();
    const float hostSr = (float) mSampleRate;

    // 12c: read smoothed values (may differ from p.freq/gainDb/q during a ramp).
    // 12j: gainOverrideDb allows dynamic-band rebuilds to supply their effective
    // gain (smoothedGain + GR) without disturbing the gain smoother state.
    const float fNow = bs.freqSmooth .getCurrentValue();
    const float gNow = std::isnan(gainOverrideDb) ? bs.gainSmooth.getCurrentValue()
                                                  : gainOverrideDb;
    const float qNow = bs.qSmooth    .getCurrentValue();

    // 12j: keep detector coefs in sync with band freq/Q when dynamic is on.
    // Called at block rate along with the main-filter rebuild - cheap.
    // 12f: detector coefs use HOST sample rate (detector runs pre-bracket).
    if (p.dynamic && isDynamicSupportedType(p.type))
    {
        const float fClamped = juce::jlimit(20.0f, hostSr * 0.499f, fNow);
        const float qClamped = juce::jlimit(0.05f, 20.0f, qNow);
        auto bpCoefs = Coeffs::makeBandPass(hostSr, fClamped, qClamped);
        bs.detFilterL.coefficients = bpCoefs;
        bs.detFilterR.coefficients = bpCoefs;
    }

    // OFF type: clear all sections
    if (p.type == 6)
    {
        bs.activeSections = 1;
        bs.filtersL[0].coefficients = nullptr;
        bs.filtersR[0].coefficients = nullptr;
        return;
    }

    // Tilt type (8): low shelf boost + high shelf cut, same gain magnitude, pivot at freq.
    if (p.type == 8)
    {
        const float f = juce::jlimit(20.0f, sr * 0.499f, fNow);
        const float g = gNow;
        auto cLo = Coeffs::makeLowShelf (sr, f, qNow, juce::Decibels::decibelsToGain( g));
        auto cHi = Coeffs::makeHighShelf(sr, f, qNow, juce::Decibels::decibelsToGain(-g));

        bs.activeSections = 2;
        if (cLo) { bs.filtersL[0].coefficients = cLo; bs.filtersR[0].coefficients = cLo; }
        else      { bs.filtersL[0].coefficients = nullptr; bs.filtersR[0].coefficients = nullptr; }
        if (cHi) { bs.filtersL[1].coefficients = cHi; bs.filtersR[1].coefficients = cHi; }
        else      { bs.filtersL[1].coefficients = nullptr; bs.filtersR[1].coefficients = nullptr; }
        for (int s = 2; s < kMaxSections; ++s)
        {
            bs.filtersL[s].coefficients = nullptr;
            bs.filtersR[s].coefficients = nullptr;
        }
        return;
    }

    const int numSections = getSectionCount(p.slope);
    bs.activeSections = numSections;

    const bool isGentle = (p.slope >= 4);
    // 12e: LP/HP/BP use the TPT filter engine. isLPHPBP covers the cascade-Q logic
    // shared by all three; the actual dispatch (TPT vs biquad) is via bs.useTPT
    // below.
    const bool isLPHP   = (p.type == 1 || p.type == 2);
    const bool isBP     = (p.type == 7);
    const bool isLPHPBP = (isLPHP || isBP);
    const bool isBell   = (p.type == 0 || p.type == 3 || p.type == 4);
    bs.useTPT = isLPHPBP;   // derived, not serialised

    // 12b: apply proportional-Q narrowing only to Peaking (type 0).
    const float propQ = effectiveQ(qNow, gNow, p.type);

    // 12e: TPT engine path for LP/HP/BP. Biquad coefs null'd for active sections
    // so the biquad dispatch in process() short-circuits on coefficients==null.
    if (bs.useTPT)
    {
        using SVFType = juce::dsp::StateVariableTPTFilterType;
        const SVFType mode = (p.type == 1) ? SVFType::lowpass
                           : (p.type == 2) ? SVFType::highpass
                                           : SVFType::bandpass;   // type == 7
        const float fClamped = juce::jlimit(20.0f, sr * 0.499f, fNow);

        for (int s = 0; s < numSections; ++s)
        {
            float sectionQ;
            if (isGentle)               sectionQ = kGentleQ;
            else if (numSections == 1)  sectionQ = qNow;            // slope 0 uses user Q
            else                        sectionQ = kSteepQ[numSections - 1][s];
            sectionQ = juce::jlimit(0.05f, 20.0f, sectionQ);

            bs.tptL[s].setType(mode);
            bs.tptL[s].setCutoffFrequency(fClamped);
            bs.tptL[s].setResonance(sectionQ);
            bs.tptR[s].setType(mode);
            bs.tptR[s].setCutoffFrequency(fClamped);
            bs.tptR[s].setResonance(sectionQ);
            bs.tptCutoffHz[s]  = fClamped;
            bs.tptResonance[s] = sectionQ;

            bs.filtersL[s].coefficients = nullptr;
            bs.filtersR[s].coefficients = nullptr;
        }
        for (int s = numSections; s < kMaxSections; ++s)
        {
            bs.filtersL[s].coefficients = nullptr;
            bs.filtersR[s].coefficients = nullptr;
        }
        return;
    }

    // Biquad path (Peaking, shelves, notch, OFF/Tilt handled earlier).
    for (int s = 0; s < numSections; ++s)
    {
        float sectionQ;
        float sectionGain;

        if (isBell)
        {
            sectionGain = gNow / (float)numSections;
            sectionQ    = propQ;  // 12b proportional Q applies here (Peak); shelf passes through unchanged (propQ==qNow for type 3/4)
        }
        else
        {
            sectionQ    = qNow;
            sectionGain = gNow;
        }

        auto c = makeOneSection(sr, fNow, sectionGain, sectionQ, p.type);
        if (c)
        {
            bs.filtersL[s].coefficients = c;
            bs.filtersR[s].coefficients = c;
        }
        else
        {
            bs.filtersL[s].coefficients = nullptr;
            bs.filtersR[s].coefficients = nullptr;
        }
    }

    for (int s = numSections; s < kMaxSections; ++s)
    {
        bs.filtersL[s].coefficients = nullptr;
        bs.filtersR[s].coefficients = nullptr;
    }
}

void EQ8DSP::updateAllBands()
{
    for (int i = 0; i < kNumBands; ++i) updateBand(i);
}

// 12d: recompute smoother ramp length from mIIRModSpeed. Called when the speed
// changes or at prepare() time. Ramp range 1..50 ms (0 = instant-ish, 1 = slow).
void EQ8DSP::refreshSmootherRamps()
{
    if (mSampleRate <= 0.0) return;
    const double rampSec = 0.001 + juce::jlimit(0.0f, 1.0f, mIIRModSpeed) * 0.049;
    for (auto& bs : mBands)
    {
        bs.freqSmooth.reset(mSampleRate, rampSec);
        bs.gainSmooth.reset(mSampleRate, rampSec);
        bs.qSmooth   .reset(mSampleRate, rampSec);
    }
}

void EQ8DSP::snapBandSmoothersToParams(int i)
{
    auto& bs = mBands[i];
    bs.freqSmooth.setCurrentAndTargetValue(bs.params.freq);
    bs.gainSmooth.setCurrentAndTargetValue(bs.params.gainDb);
    bs.qSmooth   .setCurrentAndTargetValue(bs.params.q);
    bs.lastAppliedFreq   = bs.params.freq;
    bs.lastAppliedGainDb = bs.params.gainDb;
    bs.lastAppliedQ      = bs.params.q;
}

// -- Solo / mute helpers ------------------------------------------------------

bool EQ8DSP::anySoloed() const
{
    for (int i = 0; i < kNumBands; ++i)
        if (mBands[i].params.soloed && mBands[i].params.on)
            return true;
    return false;
}

bool EQ8DSP::bandShouldProcess(int bi) const
{
    const auto& p = mBands[bi].params;
    if (!p.on || p.muted || p.type == 6) return false;
    // A8: use cached any-soloed flag (populated once per block in process()).
    if (mAnySoloedCached && !p.soloed) return false;
    return true;
}

// -- Band setters (A2: CPU-guarded; 12c: targets smoothers, defers coef rebuild) --

void EQ8DSP::setBand(int i, const Band& b)
{
    if (i < 0 || i >= kNumBands) return;
    auto& bs = mBands[i];
    const bool differs =
        bs.params.freq     != b.freq     || bs.params.gainDb    != b.gainDb    ||
        bs.params.q        != b.q        || bs.params.type      != b.type      ||
        bs.params.slope    != b.slope    || bs.params.on        != b.on        ||
        bs.params.muted    != b.muted    || bs.params.soloed    != b.soloed    ||
        bs.params.channel  != b.channel  || bs.params.dynamic   != b.dynamic   ||
        bs.params.threshold!= b.threshold|| bs.params.ratio     != b.ratio     ||
        bs.params.attack   != b.attack   || bs.params.release   != b.release   ||
        bs.params.rangeDb  != b.rangeDb  || bs.params.upward    != b.upward    ||
        bs.params.scSourceId != b.scSourceId;
    if (!differs) return;
    const bool channelChanged = (bs.params.channel != b.channel);
    const bool dynamicToggled = (bs.params.dynamic != b.dynamic);
    bs.params = b;
    bs.freqSmooth.setTargetValue(b.freq);
    bs.gainSmooth.setTargetValue(b.gainDb);
    bs.qSmooth   .setTargetValue(b.q);
    bs.dirty = true;
    if (channelChanged)
    {
        // Input domain (L vs M vs S) just changed; reset filter state to avoid transients.
        for (auto& f : bs.filtersL) if (f.coefficients) f.reset();
        for (auto& f : bs.filtersR) if (f.coefficients) f.reset();
    }
    if (dynamicToggled)
    {
        // 12j: re-seed detector + reset envelope + zero the GR readout.
        updateDetector(i);
        bs.detFilterL.reset();
        bs.detFilterR.reset();
        bs.envL = 0.0f;
        bs.envR = 0.0f;
        bs.currentGrDb.store(0.0f, std::memory_order_relaxed);
    }
}

void EQ8DSP::setBandFreq(int i, float freq)
{
    if (i < 0 || i >= kNumBands) return;
    auto& bs = mBands[i];
    if (freq == bs.params.freq) return;
    bs.params.freq = freq;
    bs.freqSmooth.setTargetValue(freq);
    // dirty flag not required; smoother delta triggers rebuild in process().
}

void EQ8DSP::setBandGain(int i, float gainDb)
{
    if (i < 0 || i >= kNumBands) return;
    auto& bs = mBands[i];
    // Gain disabled for LP/HP/BP/Notch
    const int t = bs.params.type;
    if (t == 1 || t == 2 || t == 5 || t == 7) return;
    if (gainDb == bs.params.gainDb) return;
    bs.params.gainDb = gainDb;
    bs.gainSmooth.setTargetValue(gainDb);
}

void EQ8DSP::setBandQ(int i, float q)
{
    if (i < 0 || i >= kNumBands) return;
    auto& bs = mBands[i];
    if (q == bs.params.q) return;
    bs.params.q = q;
    bs.qSmooth.setTargetValue(q);
}

void EQ8DSP::setBandType(int i, int type)
{
    if (i < 0 || i >= kNumBands) return;
    auto& bs = mBands[i];
    if (type == bs.params.type) return;
    bs.params.type = type;
    bs.dirty = true;   // type change needs immediate coef rebuild path via dirty flag
    // 12e: a type change may flip between biquad and TPT engines, which means the
    // previously-active engine's filter state is suddenly fed from the other
    // engine's just-processed output. Reset both sets so the incoming engine
    // starts clean - cheap insurance against transient clicks on type swap.
    for (auto& f : bs.filtersL) if (f.coefficients) f.reset();
    for (auto& f : bs.filtersR) if (f.coefficients) f.reset();
    for (auto& t : bs.tptL) t.reset();
    for (auto& t : bs.tptR) t.reset();
    // Zero the band's gain when the new type is non-gain-bearing (LP/HP/Notch/BP/Off).
    // setBandGain's early-return for those types would otherwise leave stale gain
    // visible in the widget after a type swap (e.g. Peaking +6 dB -> LP keeps the
    // +6 in params.gainDb, fader cap sits at +6). Bypass the type-guard by writing
    // directly to the stored value + smoother target.
    const bool nonGainBearing = (type == 1 || type == 2 || type == 5
                               || type == 6 || type == 7);
    if (nonGainBearing && bs.params.gainDb != 0.0f)
    {
        bs.params.gainDb = 0.0f;
        bs.gainSmooth.setTargetValue(0.0f);
    }
}

void EQ8DSP::setBandSlope(int i, int slope)
{
    if (i < 0 || i >= kNumBands) return;
    const int n = juce::jlimit(0, 6, slope);
    auto& bs = mBands[i];
    if (n == bs.params.slope) return;
    bs.params.slope = n;
    bs.dirty = true;   // slope change rebuilds section count
}

void EQ8DSP::setBandOn(int i, bool on)
{
    if (i < 0 || i >= kNumBands) return;
    auto& bs = mBands[i];
    if (on == bs.params.on) return;
    bs.params.on = on;
}

void EQ8DSP::setBandMuted(int i, bool muted)
{
    if (i < 0 || i >= kNumBands) return;
    auto& bs = mBands[i];
    if (muted == bs.params.muted) return;
    bs.params.muted = muted;
}

void EQ8DSP::setBandSoloed(int i, bool soloed)
{
    if (i < 0 || i >= kNumBands) return;
    auto& bs = mBands[i];
    if (soloed == bs.params.soloed) return;
    bs.params.soloed = soloed;
}

// 12h: per-band M/S routing. State reset on channel change avoids transient click
// when the filter suddenly sees a different input domain (L -> M, etc).
// 12e: also resets TPT filter state for bands using the TPT engine.
void EQ8DSP::setBandChannel(int i, int channel)
{
    if (i < 0 || i >= kNumBands) return;
    const int n = juce::jlimit(0, 4, channel);
    auto& bs = mBands[i];
    if (n == bs.params.channel) return;
    bs.params.channel = n;
    for (auto& f : bs.filtersL) if (f.coefficients) f.reset();
    for (auto& f : bs.filtersR) if (f.coefficients) f.reset();
    for (auto& t : bs.tptL) t.reset();
    for (auto& t : bs.tptR) t.reset();
}

// ── 12j Dynamic EQ helpers ───────────────────────────────────────────────────

bool EQ8DSP::isDynamicSupportedType(int type) const noexcept
{
    // Only gain-bearing types support dynamic mode. LP/HP/Notch/BP/OFF have no
    // meaningful "gain" axis for a dynamic curve to modulate. Tilt is two
    // shelves so it's gain-bearing too.
    return type == 0 /*Peaking*/ || type == 3 /*LowShelf*/
        || type == 4 /*HighShelf*/ || type == 8 /*Tilt*/;
}

void EQ8DSP::updateDetector(int i)
{
    if (mSampleRate <= 0.0) return;
    if (i < 0 || i >= kNumBands) return;
    auto& bs = mBands[i];
    const float sr  = (float) mSampleRate;
    const float f   = juce::jlimit(20.0f, sr * 0.499f, bs.params.freq);
    const float qv  = juce::jlimit(0.05f, 20.0f, bs.params.q);
    auto bpCoefs = juce::dsp::IIR::Coefficients<float>::makeBandPass(sr, f, qv);
    bs.detFilterL.coefficients = bpCoefs;
    bs.detFilterR.coefficients = bpCoefs;
}

void EQ8DSP::processDynamic(int i, int numSamples)
{
    auto& bs = mBands[i];
    const auto& p = bs.params;

    if (!p.dynamic || !isDynamicSupportedType(p.type) || p.type == 6 /*OFF*/)
    {
        bs.currentGrDb.store(0.0f, std::memory_order_relaxed);
        return;
    }
    if (mSampleRate <= 0.0) return;

    // Make sure detector coefs match the current band freq/Q.
    if (!bs.detFilterL.coefficients) updateDetector(i);

    // Detector source: the saved original input (parallel-detector model).
    // C.4 Phase 1 (2026-04-30): if this band has scSourceId in 0..3, swap
    // the detector source for the strip's SC array entry at that index.
    // Pre-C.4 scSourceId was channel-id scaffolding; under the new SC
    // infrastructure it's a slot index into the strip's SC receive array.
    // -1 = no external SC, fall back to the parallel-detector internal copy.
    const juce::AudioBuffer<float>* detSrc = &mDetInput;
    if (p.scSourceId >= 0 && mScBufs != nullptr
        && p.scSourceId < mScCount && mScBufs[p.scSourceId] != nullptr)
    {
        auto* cand = mScBufs[p.scSourceId];
        if (cand->getNumSamples() >= numSamples && cand->getNumChannels() > 0)
            detSrc = cand;
    }

    // Pick the relevant channel domain from the chosen detector source based
    // on the band's channel routing. Stereo + LOnly use L; ROnly uses R; Mid
    // uses (L+R)/2; Side uses (L-R)/2 computed on the fly.
    const float* detSrcL = detSrc->getReadPointer(0);
    const float* detSrcR = detSrc->getNumChannels() > 1
                         ? detSrc->getReadPointer(1) : detSrcL;

    // Envelope follower constants (one-pole smoother with asymmetric times).
    // coef = exp(-1 / (time_ms * sr / 1000)) is the standard formula.
    const float sr      = (float) mSampleRate;
    const float attMs   = juce::jmax(0.1f, p.attack);
    const float relMs   = juce::jmax(1.0f, p.release);
    const float attCoef = std::exp(-1.0f / (0.001f * attMs  * sr));
    const float relCoef = std::exp(-1.0f / (0.001f * relMs  * sr));

    // Run the detector over the block (both channels) and advance envelope.
    // Envelope tracks absolute detector output per channel; we combine at the
    // end by taking the max (louder of L/R drives GR). This matches the
    // "stereo-linked peak detector" behaviour of most mastering dynamic EQs.
    float envLlocal = bs.envL;
    float envRlocal = bs.envR;

    for (int n = 0; n < numSamples; ++n)
    {
        float inL = 0.0f, inR = 0.0f;
        switch (p.channel)
        {
            case Channel::Stereo: inL = detSrcL[n]; inR = detSrcR[n]; break;
            case Channel::Mid:    inL = 0.5f * (detSrcL[n] + detSrcR[n]); inR = inL; break;
            case Channel::Side:   inL = 0.5f * (detSrcL[n] - detSrcR[n]); inR = inL; break;
            case Channel::LOnly:  inL = detSrcL[n];  inR = 0.0f; break;
            case Channel::ROnly:  inL = 0.0f;        inR = detSrcR[n]; break;
            default:              inL = detSrcL[n]; inR = detSrcR[n]; break;
        }
        const float detL = std::abs(bs.detFilterL.processSample(inL));
        const float detR = std::abs(bs.detFilterR.processSample(inR));

        const float coefL = (detL > envLlocal) ? attCoef : relCoef;
        const float coefR = (detR > envRlocal) ? attCoef : relCoef;
        envLlocal = coefL * envLlocal + (1.0f - coefL) * detL;
        envRlocal = coefR * envRlocal + (1.0f - coefR) * detR;
    }
    bs.envL = envLlocal;
    bs.envR = envRlocal;

    // Stereo-link: louder-of-L/R drives GR.
    const float env = juce::jmax(envLlocal, envRlocal);
    const float envDb = juce::Decibels::gainToDecibels(env + 1.0e-12f, -120.0f);

    // 12j follow-up Q2: direction + amount derived from signed rangeDb.
    //   rangeDb < 0  -> downward compression up to |range| dB (cut above threshold)
    //   rangeDb > 0  -> upward expansion up to +range dB (boost below threshold)
    //   rangeDb == 0 -> no modulation (GR stays at 0)
    // Old separate `upward` field is unused; kept for APVTS preset stability.
    const float thr     = p.threshold;
    const float ratio   = juce::jmax(1.0f, p.ratio);
    const bool  isUp    = (p.rangeDb > 0.0f);
    const float rangeMag= std::abs(p.rangeDb);
    float grDb = 0.0f;

    if (rangeMag > 0.0f)
    {
        if (isUp)
        {
            // C.4 follow-up (2026-04-30): the noise-floor gate at
            // thr - 2*rangeMag was removed. It produced a tight expansion
            // window [thr - 2*rangeMag, thr] that made "Range positive does
            // nothing" the typical user experience -- the band's detector
            // level had to land in that narrow band to fire. Pro-Q3 / Neutron
            // / etc. don't gate; they expand whenever envDb < thr, capped at
            // rangeMag. The original concern (curve visually pegged at
            // max boost during silence) is a cosmetic artifact only -- silence
            // x gain is still silence; the dotted "outer range" line showing
            // full extent on silence is actually the correct semantic.
            const float under = thr - envDb;
            if (under > 0.0f)
                grDb = under * (1.0f - 1.0f / ratio);   // positive = boost
            grDb = juce::jmin(grDb, rangeMag);
        }
        else
        {
            const float over = envDb - thr;
            if (over > 0.0f)
                grDb = -over * (1.0f - 1.0f / ratio);   // negative = cut
            grDb = juce::jmax(grDb, -rangeMag);
        }
    }

    bs.currentGrDb.store(grDb, std::memory_order_relaxed);
}

// ── 12j Dynamic EQ setters ───────────────────────────────────────────────────

void EQ8DSP::setBandDynamic(int i, bool dynamic)
{
    if (i < 0 || i >= kNumBands) return;
    auto& bs = mBands[i];
    if (dynamic == bs.params.dynamic) return;
    bs.params.dynamic = dynamic;
    // Re-seed detector coefs + reset envelope state on enable/disable.
    updateDetector(i);
    bs.detFilterL.reset();
    bs.detFilterR.reset();
    bs.envL = 0.0f;
    bs.envR = 0.0f;
    bs.currentGrDb.store(0.0f, std::memory_order_relaxed);
    bs.dirty = true;   // force coef rebuild on next process block
}

void EQ8DSP::setBandThreshold(int i, float dB)
{
    if (i < 0 || i >= kNumBands) return;
    const float v = juce::jlimit(-60.0f, 0.0f, dB);
    auto& bs = mBands[i];
    if (v == bs.params.threshold) return;
    bs.params.threshold = v;
}

void EQ8DSP::setBandRatio(int i, float ratio)
{
    if (i < 0 || i >= kNumBands) return;
    const float v = juce::jlimit(1.0f, 20.0f, ratio);
    auto& bs = mBands[i];
    if (v == bs.params.ratio) return;
    bs.params.ratio = v;
}

void EQ8DSP::setBandAttack(int i, float ms)
{
    if (i < 0 || i >= kNumBands) return;
    const float v = juce::jlimit(0.1f, 500.0f, ms);
    auto& bs = mBands[i];
    if (v == bs.params.attack) return;
    bs.params.attack = v;
}

void EQ8DSP::setBandRelease(int i, float ms)
{
    if (i < 0 || i >= kNumBands) return;
    const float v = juce::jlimit(1.0f, 2000.0f, ms);
    auto& bs = mBands[i];
    if (v == bs.params.release) return;
    bs.params.release = v;
}

void EQ8DSP::setBandRange(int i, float dB)
{
    if (i < 0 || i >= kNumBands) return;
    // 12j follow-up Q2: bipolar range. Negative = max downward compression,
    // positive = max upward expansion, zero = no modulation.
    // 2026-04-19 follow-up: clamp matched to Gain param magnitude (-18..+18)
    // so Range can't exceed what the band's gain setter allows on its own.
    const float v = juce::jlimit(-18.0f, 18.0f, dB);
    auto& bs = mBands[i];
    if (v == bs.params.rangeDb) return;
    bs.params.rangeDb = v;
}

void EQ8DSP::setBandUpward(int i, bool upward)
{
    if (i < 0 || i >= kNumBands) return;
    auto& bs = mBands[i];
    if (upward == bs.params.upward) return;
    bs.params.upward = upward;
    bs.currentGrDb.store(0.0f, std::memory_order_relaxed);   // re-polarise from zero
}

void EQ8DSP::setBandScSource(int i, int sourceId)
{
    if (i < 0 || i >= kNumBands) return;
    auto& bs = mBands[i];
    if (sourceId == bs.params.scSourceId) return;
    bs.params.scSourceId = sourceId;
    // Option B scaffolding - no DSP behaviour today (detector always reads
    // internal). When the sidechain-infrastructure session ships, this field
    // drives the routing lookup.
}

// ── 12j UI query helpers ─────────────────────────────────────────────────────

float EQ8DSP::getBandGrDb(int i) const noexcept
{
    if (i < 0 || i >= kNumBands) return 0.0f;
    return mBands[i].currentGrDb.load(std::memory_order_relaxed);
}

EQ8DSP::Band EQ8DSP::getBand(int i) const
{
    if (i < 0 || i >= kNumBands) return {};
    return mBands[i].params;
}

// -- Main level, phase mode, IIR mod speed (A3: CPU-guarded) -----------------

void EQ8DSP::setMainLevel(float dB)
{
    if (dB != mMainLevelDb) mMainLevelDb = dB;
}

// 12g: actual phase-mode dispatch.
//   Standard      -> IIR path (today's behaviour)
//   Linear        -> linear FFT at the Linear Phase Precision size (default 2048)
//   HQ Plus       -> AC=on forced, no FFT
//   HQ Linear     -> AC=on forced + linear FFT @ 4096
//   HQ Extended   -> linear FFT @ 512 (low-latency variant)
// On any mode change: reconfigure linear processor + force AC on/off as
// appropriate + mark IR + filter coefs dirty + arm a mode-aware output fade
// scaled to the latency delta so the seam is masked.
void EQ8DSP::setPhaseMode(PhaseMode mode)
{
    if (mode == mPhaseMode) return;

    const int  oldLatency = getLatencySamples();
    const bool wasAcForced = (mPhaseMode == PhaseMode::HQPlus
                            || mPhaseMode == PhaseMode::HQL);
    const bool nowAcForced = (mode == PhaseMode::HQPlus || mode == PhaseMode::HQL);

    mPhaseMode = mode;

    // Force AC on for HQ Plus / HQ Linear; clear when leaving those modes IF
    // the user hadn't explicitly enabled AC outside the forced-mode context.
    // Simpler policy that matches user expectation: when leaving a forced-AC
    // mode, also turn AC off. (User who wants AC alone can pick HQ Plus.)
    if (nowAcForced && ! mAntiCramping)
    {
        // Internal AC flip - inline to avoid the public setter's recursive
        // prepare call that we'll do once below covering everything.
        mAntiCramping = true;
    }
    else if (! nowAcForced && wasAcForced && mAntiCramping)
    {
        mAntiCramping = false;
    }

    // Re-prepare so TPT instances + oversampler scratch resize to the new
    // design rate (AC may have just toggled), then reconfigure the linear
    // processor for the new mode + design rate.
    if (mSampleRate > 0.0 && mMaxBlock > 0)
        prepare(mSampleRate, mMaxBlock);

    if (mOversampler) mOversampler->reset();

    // Mode-aware output fade (T2c): scale to the latency delta so the seam is
    // covered. Floor at kMinFadeSamples (always at least 256 so AC-only flips
    // still get the original soft ramp), ceiling at kMaxFadeSamples (4096) so
    // an HQE -> HQL flip doesn't soft-mute audio for ~80 ms.
    const int newLatency = getLatencySamples();
    const int delta = std::abs (newLatency - oldLatency);
    mAcFadeTarget    = juce::jlimit (kMinFadeSamples, kMaxFadeSamples,
                                     juce::jmax (delta, kMinFadeSamples));
    mAcFadeRemaining = mAcFadeTarget;
}

// QA-Soundness: the Linear Phase Precision control governs the PLAIN Linear
// mode only.  HQ Linear (4096) and HQ Extended (512) keep the sizes locked spec
// 12g pinned to them, because a phase mode's ONLY effect on
// EqLinearPhaseProcessor is its FFT size -- a global precision override would
// make HQ Extended bit-identical to Linear and leave the mode selling nothing.
int EQ8DSP::linearFftSize (PhaseMode mode, int precision) noexcept
{
    if (mode == PhaseMode::Linear)
        return kPrecFftSizes[juce::jlimit (0, kNumLinearPrecisions - 1, precision)];

    return EqLinearPhaseProcessor::fftSizeForMode ((int) mode);
}

// 12g: lazily allocate or free the linear-phase processor based on the
// current phase mode + precision + sample rate + design rate (which is 2*sr
// when AC is on, matching HQ Plus / HQ Linear).
void EQ8DSP::reconfigureLinearProc()
{
    const int fftSize = linearFftSize (mPhaseMode, mLinearPrec);
    if (fftSize == 0)
    {
        // Standard / HQ Plus -> no linear processing.
        mLinearProc.prepare (0, 0, 0.0);
        mLinearIRDirty = true;
        return;
    }

    // Linear modes always run on stereo (T2a option B = per-band M/S routing
    // restricted to Stereo when in linear mode, so the processor only needs
    // to handle 2 independent channels).
    const double designRate = mAntiCramping ? 2.0 * mSampleRate : mSampleRate;
    mLinearProc.prepare (fftSize, 2, designRate);
    mLinearIRDirty = true;
}

// 12g: walk N/2+1 bins, query magnitudeForFrequencyStatic at each bin freq,
// and hand the lambda to the processor for IR rebuild.
void EQ8DSP::rebuildLinearIR()
{
    if (! mLinearProc.isReady()) return;
    mLinearProc.rebuildIR ([this] (float freq) {
        return magnitudeForFrequencyStatic (freq);
    });
    mLinearIRDirty = false;
}

// 12g: static-design magnitude lookup. Mirrors getMagnitudeForFrequency but
// uses bs.params.gainDb directly (no GR injection) so dynamic-EQ behaviour is
// excluded from the linear-phase IR (T2b option C). Per-band M/S routing is
// also flattened to "all bands contribute" since linear-phase IR convolves the
// combined response on full L/R (T2a option B - per-band channel restricted
// to Stereo when in linear mode).
float EQ8DSP::magnitudeForFrequencyStatic (float freq) const noexcept
{
    if (mSampleRate <= 0.0) return 1.0f;
    double mag = 1.0;
    const double designRate = mAntiCramping ? 2.0 * mSampleRate : mSampleRate;

    auto tptMag = [](int svfType, double freq_, double f0, double Q) -> double
    {
        if (f0 <= 0.0 || Q <= 0.0) return 1.0;
        const double x  = freq_ / f0;
        const double x2 = x * x;
        const double xoQ = x / Q;
        const double denom = (1.0 - x2) * (1.0 - x2) + xoQ * xoQ;
        if (denom <= 0.0) return 1.0;
        double num;
        switch (svfType)
        {
            case 1: num = 1.0;       break;   // LP
            case 2: num = x2 * x2;   break;   // HP
            case 7: num = xoQ * xoQ; break;   // BP
            default: return 1.0;
        }
        return std::sqrt (num / denom);
    };

    for (int bi = 0; bi < kNumBands; ++bi)
    {
        if (! bandShouldProcess (bi)) continue;
        const auto& bs = mBands[bi];
        const auto& p  = bs.params;
        const int ns = bs.activeSections;

        if (bs.useTPT)
        {
            for (int s = 0; s < ns; ++s)
                mag *= tptMag (p.type, (double) freq,
                               (double) bs.tptCutoffHz[s],
                               (double) bs.tptResonance[s]);
        }
        else
        {
            // Static design gain (no GR) - the whole point of T2b option C.
            const float effG  = p.gainDb;
            const bool  isBell = (p.type == 0);
            const float propQ = (isBell && mProportionalQ)
                                    ? p.q * (1.0f + std::abs (effG) / 18.0f)
                                    : p.q;
            for (int s = 0; s < ns; ++s)
            {
                float sectionGain, sectionQ;
                if (isBell || p.type == 3 || p.type == 4 /*shelves*/ || p.type == 8 /*tilt*/)
                {
                    sectionGain = (isBell ? effG / (float) ns : effG);
                    sectionQ    = (isBell ? propQ : p.q);
                }
                else
                {
                    sectionGain = effG;
                    sectionQ    = p.q;
                }
                auto c = makeOneSection ((float) designRate, p.freq,
                                         sectionGain, sectionQ, p.type);
                if (c)
                    mag *= c->getMagnitudeForFrequency ((double) freq, designRate);
            }
        }
    }

    mag *= (double) juce::Decibels::decibelsToGain (mMainLevelDb);
    return (float) mag;
}

// A precision change resizes the linear-phase FFT, which shifts reported latency
// exactly like a mode flip does - so it needs setPhaseMode's mode-aware output
// fade too, or the host's PDC realignment lands as a click on a sustained note.
// Reallocation happens inside reconfigureLinearProc, message thread only.
void EQ8DSP::setLinearPhasePrecision(int precision)
{
    const int n = juce::jlimit(0, kNumLinearPrecisions - 1, precision);
    if (n == mLinearPrec) return;

    const int oldLatency = getLatencySamples();
    mLinearPrec = n;
    reconfigureLinearProc();

    const int delta = std::abs (getLatencySamples() - oldLatency);
    mAcFadeTarget    = juce::jlimit (kMinFadeSamples, kMaxFadeSamples,
                                     juce::jmax (delta, kMinFadeSamples));
    mAcFadeRemaining = mAcFadeTarget;
}

void EQ8DSP::setIIRModSpeed(float speed01)
{
    const float n = juce::jlimit(0.0f, 1.0f, speed01);
    if (n == mIIRModSpeed) return;
    mIIRModSpeed = n;
    // 12d: re-derive smoother ramp length. Smoothers keep current value, just
    // adjust their ramp rate going forward.
    refreshSmootherRamps();
}

void EQ8DSP::setProportionalQ(bool on)
{
    if (on == mProportionalQ) return;
    mProportionalQ = on;
    // Changing propQ toggles how Peaking bands compute section Q -> rebuild all bands.
    for (auto& bs : mBands) bs.dirty = true;
}

// 12f: anti-cramping toggle. Flipping this rewrites the design sample rate
// for the main filters (host vs 2*host), so all coefs need to be rebuilt and
// every filter / TPT instance / oversampler stage gets reset to avoid clicks
// from latency-shift mid-stream. After this returns the caller (popup-menu
// handler) must trigger PDC refresh - getLatencySamples() now reports the
// oversampler latency that the host needs to compensate for.
void EQ8DSP::setAntiCramping(bool on)
{
    if (on == mAntiCramping) return;
    mAntiCramping = on;

    // Re-prepare so TPT instances (which store their sample rate internally)
    // and oversampler scratch are sized for the new design rate. mSampleRate /
    // mMaxBlock are preserved by prepare() - it just rewrites internals.
    if (mSampleRate > 0.0 && mMaxBlock > 0)
        prepare(mSampleRate, mMaxBlock);

    if (mOversampler) mOversampler->reset();

    // Arm the post-toggle fade so the latency-shift discontinuity is masked
    // by a short linear ramp on the EQ output (both directions of toggle).
    // 12g: AC alone shifts latency by ~5-8 samples - the kMinFadeSamples (256)
    // floor is plenty. Phase-mode flips into / out of linear modes get the
    // larger mode-aware target via setPhaseMode's own fade-arm path.
    mAcFadeTarget    = kMinFadeSamples;
    mAcFadeRemaining = mAcFadeTarget;
}

// 12f: PDC. Returns the integer-rounded oversampler latency when AC is on.
// 12g: also adds the linear-phase FFT latency (FFT/2) when in a linear mode.
// Both contribute - HQ Linear stacks both (~2050 samples at 4096-pt FFT
// linear @ 2x OS).
int EQ8DSP::getLatencySamples() const
{
    int total = 0;
    if (mAntiCramping && mOversampler)
        total += (int) std::ceil (mOversampler->getLatencyInSamples());
    if (mLinearProc.isReady())
        total += mLinearProc.getLatencySamples();
    return total;
}

// -- Compare banks ------------------------------------------------------------

void EQ8DSP::saveToSpare()
{
    if (mSpareLocked) return;
    for (int i = 0; i < kNumBands; ++i)
        mSpare[i] = mBands[i].params;
}

void EQ8DSP::swapWithSpare()
{
    for (int i = 0; i < kNumBands; ++i)
    {
        Band tmp         = mBands[i].params;
        mBands[i].params = mSpare[i];
        mSpare[i]        = tmp;
        // Snap smoothers to the new params (compare is an intentional "instant swap").
        snapBandSmoothersToParams(i);
        mBands[i].dirty = true;
    }
    mViewingSpare = !mViewingSpare;
}

void EQ8DSP::lockSpare(bool locked)
{
    mSpareLocked = locked;
}

// -- 12a: magnitude queries ---------------------------------------------------

float EQ8DSP::getMagnitudeForFrequency(float freq) const noexcept
{
    if (mSampleRate <= 0.0) return 1.0f;
    double mag = 1.0;   // multiplicative accumulator

    // 12f: when AC is on, biquad coefs are designed at 2*sr - the magnitude
    // query must use the same rate so the displayed curve matches the audible
    // response. TPT path already uses sample-rate-agnostic analog prototypes.
    const double designRate = mAntiCramping ? 2.0 * mSampleRate : mSampleRate;

    // 12e helper: closed-form analog-prototype magnitude for TPT LP/HP/BP.
    // TPT's zero-delay-feedback topology preserves the analog transfer function
    // nearly exactly (the purpose of the zero-delay algorithm), so the analog
    // formula is correct to within ~1% across the audible range - plenty for
    // UI curve drawing.
    //
    //   Let x = freq / f0
    //   Denominator D = (1 - x^2)^2 + (x/Q)^2
    //   LP: |H|^2 = 1         / D
    //   HP: |H|^2 = x^4       / D
    //   BP: |H|^2 = (x/Q)^2   / D
    auto tptMag = [](int svfType, double freq_, double f0, double Q) -> double
    {
        if (f0 <= 0.0 || Q <= 0.0) return 1.0;
        const double x  = freq_ / f0;
        const double x2 = x * x;
        const double xoQ = x / Q;
        const double denom = (1.0 - x2) * (1.0 - x2) + xoQ * xoQ;
        if (denom <= 0.0) return 1.0;
        double num;
        switch (svfType)
        {
            case 1: num = 1.0;                           break;   // LP
            case 2: num = x2 * x2;                       break;   // HP
            case 7: num = xoQ * xoQ;                     break;   // BP
            default: return 1.0;
        }
        return std::sqrt(num / denom);
    };

    for (int bi = 0; bi < kNumBands; ++bi)
    {
        if (!bandShouldProcess(bi)) continue;
        const auto& bs = mBands[bi];
        const int ns = bs.activeSections;

        if (bs.useTPT)
        {
            // 12e: TPT engine - analog prototype per section (cascaded product).
            for (int s = 0; s < ns; ++s)
                mag *= tptMag(bs.params.type, (double) freq,
                              (double) bs.tptCutoffHz[s],
                              (double) bs.tptResonance[s]);
        }
        else if (bs.params.dynamic && isDynamicSupportedType(bs.params.type))
        {
            // 12j: for dynamic bands, compose the curve from a locally rebuilt
            // coefficient set using the EFFECTIVE gain (designGain + currentGrDb).
            // This is what makes the curve physically animate as the envelope
            // pushes GR up/down, matching Ozone/Neutron's "line moves in the band
            // area" visual. Coef rebuild is cheap (const, no DSP state touched).
            const float effG = bs.params.gainDb
                             + bs.currentGrDb.load(std::memory_order_relaxed);
            // Rebuild per-section the same way updateBand does (Bell/Shelf types
            // share the same section-gain fan-out logic).
            const bool isBell = (bs.params.type == 0);
            const float propQ = (isBell && mProportionalQ)
                                    ? bs.params.q * (1.0f + std::abs(effG) / 18.0f)
                                    : bs.params.q;
            for (int s = 0; s < ns; ++s)
            {
                float sectionGain, sectionQ;
                if (isBell)
                {
                    sectionGain = effG / (float)ns;
                    sectionQ    = propQ;
                }
                else
                {
                    sectionGain = effG;
                    sectionQ    = bs.params.q;
                }
                auto c = makeOneSection((float) designRate, bs.params.freq,
                                        sectionGain, sectionQ, bs.params.type);
                if (c)
                    mag *= c->getMagnitudeForFrequency((double) freq, designRate);
            }
        }
        else
        {
            for (int s = 0; s < ns; ++s)
            {
                const auto& c = bs.filtersL[s].coefficients;
                if (c == nullptr) continue;
                mag *= c->getMagnitudeForFrequency((double) freq, designRate);
            }
        }
    }

    // Include the main-level trim in the curve so the UI shows the full output.
    mag *= (double) juce::Decibels::decibelsToGain(mMainLevelDb);
    return (float) mag;
}

// -- Serialization ------------------------------------------------------------

// One field list for both banks: the live bands and the A/B spare must never
// diverge on what a saved band carries.
static juce::ValueTree makeBandTree(int index, const EQ8DSP::Band& p)
{
    juce::ValueTree band("Band");
    band.setProperty("index",  index,    nullptr);
    band.setProperty("freq",   p.freq,   nullptr);
    band.setProperty("gainDb", p.gainDb, nullptr);
    band.setProperty("q",      p.q,      nullptr);
    band.setProperty("type",   p.type,   nullptr);
    band.setProperty("slope",  p.slope,  nullptr);
    band.setProperty("on",        p.on,        nullptr);
    band.setProperty("muted",     p.muted,     nullptr);
    band.setProperty("soloed",    p.soloed,    nullptr);
    band.setProperty("channel",   p.channel,   nullptr);   // 12h
    // 12j: dynamic EQ fields (all PRESET-SAFE additive; defaults preserve
    // v1 behavior so missing attributes on load just stay off).
    band.setProperty("dynamic",   p.dynamic,   nullptr);
    band.setProperty("threshold", p.threshold, nullptr);
    band.setProperty("ratio",     p.ratio,     nullptr);
    band.setProperty("attack",    p.attack,    nullptr);
    band.setProperty("release",   p.release,   nullptr);
    band.setProperty("rangeDb",   p.rangeDb,   nullptr);
    band.setProperty("upward",    p.upward,    nullptr);
    band.setProperty("scSource",  p.scSourceId,nullptr);   // Option B scaffolding
    return band;
}

// Fallbacks are the caller's CURRENT values, so an attribute missing from an
// older file leaves that field at whatever the constructor seeded.
static void readBandXml(const juce::XmlElement& src, EQ8DSP::Band& p)
{
    p.freq   = (float)src.getDoubleAttribute("freq",   p.freq);
    p.gainDb = (float)src.getDoubleAttribute("gainDb", p.gainDb);
    p.q      = (float)src.getDoubleAttribute("q",      p.q);
    p.type   =        src.getIntAttribute   ("type",   p.type);
    p.slope  =        src.getIntAttribute   ("slope",  p.slope);
    p.on     =        src.getBoolAttribute  ("on",     p.on);
    p.muted  =        src.getBoolAttribute  ("muted",  p.muted);
    p.soloed =        src.getBoolAttribute  ("soloed", p.soloed);
    p.channel= juce::jlimit(0, 4, src.getIntAttribute("channel", p.channel));   // 12h - fallback = current default (Stereo for bare, Mid/Side for wrapper-owned)
    p.dynamic   =        src.getBoolAttribute  ("dynamic",   p.dynamic);
    p.threshold = (float)src.getDoubleAttribute("threshold", p.threshold);
    p.ratio     = (float)src.getDoubleAttribute("ratio",     p.ratio);
    p.attack    = (float)src.getDoubleAttribute("attack",    p.attack);
    p.release   = (float)src.getDoubleAttribute("release",   p.release);
    p.rangeDb   = (float)src.getDoubleAttribute("rangeDb",   p.rangeDb);
    p.upward    =        src.getBoolAttribute  ("upward",    p.upward);
    p.scSourceId=        src.getIntAttribute   ("scSource",  p.scSourceId);
}

void EQ8DSP::getStateInformation(juce::MemoryBlock& dest)
{
    juce::ValueTree state("EQ8DSP");
    for (int i = 0; i < kNumBands; ++i)
        state.appendChild(makeBandTree(i, mBands[i].params), nullptr);

    // A/B compare spare bank, nested under ONE child rather than emitted as
    // sibling <Band> nodes: setStateInformation's child loop has no tag-name
    // check and keys purely off "index", so siblings would overwrite the live
    // bank on load.
    juce::ValueTree spare("Spare");
    for (int i = 0; i < kNumBands; ++i)
        spare.appendChild(makeBandTree(i, mSpare[i]), nullptr);
    state.appendChild(spare, nullptr);

    // Which bank the <Band> set above IS.  swapWithSpare() physically exchanges
    // mBands and mSpare, so the saved bands are the B bank whenever this is true.
    // Without it the reader cannot tell A from B and every A/B label inverts.
    state.setProperty("viewingSpare",  mViewingSpare,   nullptr);

    state.setProperty("mainLevel",     mMainLevelDb,    nullptr);
    state.setProperty("iirModSpeed",   mIIRModSpeed,    nullptr);   // 12d
    state.setProperty("proportionalQ", mProportionalQ,  nullptr);   // 12b
    state.setProperty("antiCramping",  mAntiCramping,   nullptr);   // 12f
    state.setProperty("phaseMode",     (int) mPhaseMode, nullptr);  // 12g
    state.setProperty("linearPrec",    mLinearPrec,      nullptr);  // 12g Linear FFT size
    auto xml = state.createXml();
    if (xml) juce::AudioProcessor::copyXmlToBinary(*xml, dest);
}

void EQ8DSP::setStateInformation(const void* data, int sz)
{
    auto xml = SafeXml::parseBinaryBlob (data, sz);
    if (!xml) return;

    bool isOld = (xml->getTagName() == "EQ6DSP");
    bool isNew = (xml->getTagName() == "EQ8DSP");
    if (!isOld && !isNew) return;

    for (auto* child : xml->getChildIterator())
    {
        int idx = child->getIntAttribute("index", -1);
        if (idx < 0 || idx >= kNumBands) continue;
        readBandXml(*child, mBands[idx].params);
        // 12c: snap smoothers to restored values so the first process() block after
        // state-load doesn't ramp across a potentially large delta.
        snapBandSmoothersToParams(idx);
        mBands[idx].dirty = true;
    }

    // A/B spare bank.  Inert data - swapWithSpare() is the only thing that ever
    // copies it into mBands - so no smoother snap or band rebuild is owed here.
    // Files without the child (and the EQ6DSP legacy path) keep the seeded spare.
    if (auto* sp = xml->getChildByName("Spare"))
        for (auto* sb : sp->getChildIterator())
        {
            const int idx = sb->getIntAttribute("index", -1);
            if (idx < 0 || idx >= kNumBands) continue;
            readBandXml(*sb, mSpare[idx]);
        }

    if (isNew)
    {
        mMainLevelDb   = (float)xml->getDoubleAttribute("mainLevel",     0.0);
        mIIRModSpeed   = (float)xml->getDoubleAttribute("iirModSpeed",   mIIRModSpeed);
        mProportionalQ =        xml->getBoolAttribute  ("proportionalQ", mProportionalQ);
        // Falls back to false, not to the current member: permanent bus EQ
        // instances outlive a project load, so carrying the previous project's
        // bank over would relabel this one's curve.
        mViewingSpare  =        xml->getBoolAttribute  ("viewingSpare",  false);
        // 12f: PRESET-SAFE additive. Defaults false on missing (older state).
        const bool acSaved = xml->getBoolAttribute("antiCramping", mAntiCramping);
        if (acSaved != mAntiCramping)
            setAntiCramping(acSaved);   // re-prepare TPT/oversampler at new rate

        // 12g: PRESET-SAFE additive. Defaults to the current member on missing
        // (older state), which for precision is the 2048 that 12g had fixed
        // Linear at - so state written before this was persisted sounds the same.
        // Precision is restored BEFORE the mode so the mode's own reconfigure
        // already sees the right FFT size.
        const int lpSaved = xml->getIntAttribute("linearPrec", mLinearPrec);
        setLinearPhasePrecision (juce::jlimit (0, kNumLinearPrecisions - 1, lpSaved));

        const int pmSaved = xml->getIntAttribute("phaseMode", (int) mPhaseMode);
        const PhaseMode pmEnum = (PhaseMode) juce::jlimit (0, (int) PhaseMode::Count - 1, pmSaved);
        if (pmEnum != mPhaseMode)
            setPhaseMode(pmEnum);   // reconfigures linear processor + AC if forced
    }
    refreshSmootherRamps();   // iirModSpeed may have changed
    updateAllBands();
}
