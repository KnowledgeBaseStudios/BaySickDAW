#include "CompressorDSP.h"
#include <cmath>
#include <algorithm>

namespace
{
    // CS-3: the single Attack knob also sets release, inversely (fast attack ->
    // long release = max sustain; slow attack -> short release = punch).  Maps the
    // CS Attack range [1,50] ms to release [800,100] ms.
    inline float csReleaseFromAttackMs (float attackMs) noexcept
    {
        return juce::jmap (juce::jlimit (1.0f, 50.0f, attackMs), 1.0f, 50.0f, 800.0f, 100.0f);
    }
}

// -----------------------------------------------------------------------------
CompressorDSP::CompressorDSP()
{
    calcCoefs();

    // A2 -- Continuous-param smoothers snap to defaults at construction;
    // prepare() re-arms them with the sample-rate-dependent ramp.
    mThresholdSmoothed.setCurrentAndTargetValue(threshold);
    mRatioSmoothed    .setCurrentAndTargetValue(ratio);
    mMixSmoothed      .setCurrentAndTargetValue(mix);
    mMakeupDbSmoothed .setCurrentAndTargetValue(makeupDb);

    // C2 -- HPF configured as highpass; coefficients materialised in prepare.
    mScHpfL.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    mScHpfR.setType(juce::dsp::StateVariableTPTFilterType::highpass);
}

// -----------------------------------------------------------------------------
void CompressorDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;
    calcCoefs();

    // Size look-ahead buffer to max possible (5 ms) + 1 for safe wrap.
    const int maxLASamples = static_cast<int>(std::ceil(kMaxLookaheadMs * 0.001 * sampleRate)) + 1;
    mLookaheadL.assign(static_cast<size_t>(maxLASamples), 0.0f);
    mLookaheadR.assign(static_cast<size_t>(maxLASamples), 0.0f);
    mLAWritePos = 0;
    // Recompute current LA size from param
    mLASamples = juce::jlimit(0,
                              (int)mLookaheadL.size() - 1,
                              static_cast<int>(lookaheadMs * 0.001 * sampleRate));

    // A4 -- GR meter decay per block (~30 dB/sec, SR-aware). Matches InsertNode / bus pattern.
    constexpr float kGrDecayDbPerSec = 30.0f;
    mGrDecayDbPerBlock = kGrDecayDbPerSec * (float) maxBlockSize
                         / (float) (sampleRate > 0.0 ? sampleRate : 44100.0);

    // A2 -- Smoothers: 20 ms ramp suppresses audible zipper on fast drags.
    const double rampSecs = 0.02;
    mThresholdSmoothed.reset(sampleRate, rampSecs);
    mRatioSmoothed    .reset(sampleRate, rampSecs);
    mMixSmoothed      .reset(sampleRate, rampSecs);
    mMakeupDbSmoothed .reset(sampleRate, rampSecs);
    mThresholdSmoothed.setCurrentAndTargetValue(threshold);
    mRatioSmoothed    .setCurrentAndTargetValue(ratio);
    mMixSmoothed      .setCurrentAndTargetValue(mix);
    mMakeupDbSmoothed .setCurrentAndTargetValue(makeupDb);

    // C2 -- Prepare SC HPF biquads (1 channel each).
    juce::dsp::ProcessSpec scSpec { sampleRate,
                                    (juce::uint32) juce::jmax(1, maxBlockSize),
                                    1 };
    mScHpfL.prepare(scSpec);
    mScHpfR.prepare(scSpec);
    mScHpfL.reset();
    mScHpfR.reset();
    updateScHpfCoefs();

    // I-4 (2026-05-02): prepare the CS Style tilt-EQ shelves at full stereo
    // width.  ProcessorDuplicator handles both channels off the same coefs.
    juce::dsp::ProcessSpec tiltSpec { sampleRate,
                                       (juce::uint32) juce::jmax(1, maxBlockSize),
                                       2 };
    mCsLowShelf .prepare (tiltSpec);
    mCsHighShelf.prepare (tiltSpec);
    mCsLowShelf .reset();
    mCsHighShelf.reset();
    updateCsToneCoefs();

    reset();
}

// -----------------------------------------------------------------------------
void CompressorDSP::setType (int t)
{
    const Type newType = static_cast<Type> (juce::jlimit (0, 3, t));   // I-4: range 0..3 (CS = 3)
    if (newType == mType) return;
    mType = newType;
    // Reset opto memory on type switch so prior history doesn't bleed into
    // a different mode's release behavior.
    mOptoHistory = 0.0f;

    // I-4 (2026-05-02): switching TO CS Style coerces ratio + release to
    // CS-typical values (BOSS CS-3 reference: ~5:1 ratio, ~200ms release)
    // and re-applies the Sustain macro.  Switching AWAY leaves the user's
    // explicit ratio/release intact -- a user who came from CS Style will
    // see whatever values the CS macro left in place, which is fine since
    // they're standard params on the other Types.
    if (mType == Type::CS)
    {
        // CS Style (BOSS CS-3): high fixed ratio; the single Attack knob also
        // sets release inversely; the Sustain macro drives INTO a fixed threshold
        // (applyCsSustainMacro).
        if (ratio != 10.0f) { ratio = 10.0f; mRatioSmoothed.setTargetValue (ratio); }
        releaseMs = csReleaseFromAttackMs (attackMs);
        calcCoefs();   // release derived from attack
        applyCsSustainMacro();
        // Refresh tilt-EQ coefs in case csTone01 was edited while a non-CS Type
        // was active (only USED in process() under CS; keeps the switch instant).
        updateCsToneCoefs();
    }

    // QA-EffectsReview Task 6: FET (1176) is a peak-detecting, hard-knee FET
    // limiter.  Force those on every FET selection (the FET panel exposes
    // neither Peak/RMS nor knee) so the mode is correct however the user got
    // here -- live switch, or the load path (mirrored in setStateInformation).
    if (mType == Type::FET)
    {
        peakDetection = true;
        kneeDb        = 0.0f;
        // 1176 Output is a makeup control; default a fresh FET to +12 dB makeup
        // (the "18" dial position) since the shared makeup default is 0 = unity.
        // Preset loads keep their saved makeup (setStateInformation, no nudge).
        if (makeupDb == 0.0f) setGain (12.0f);
    }
}

// QA-EffectsReview Task 6: CS-3 Tone is a TREBLE control (high-shelf only) --
// 12 o'clock (csTone01 = 0.5) flat, max = treble boost, min = treble cut, +/-9 dB
// at the extremes.  The low shelf is pinned flat (was a bipolar low/high tilt;
// the real CS-3 Tone doesn't touch the lows).
void CompressorDSP::updateCsToneCoefs()
{
    if (mSampleRate <= 0.0) return;
    const float toneSigned = juce::jlimit (-1.0f, 1.0f,
                                            (csTone01 - 0.5f) * 2.0f);
    constexpr float kMaxShelfDb  = 9.0f;
    constexpr float kHighShelfHz = 3000.0f;

    const float highGain = juce::Decibels::decibelsToGain (kMaxShelfDb * toneSigned, -60.0f);

    *mCsLowShelf .state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf
        ((double) mSampleRate, 300.0f, 0.7071f, 1.0f);   // pinned flat
    *mCsHighShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf
        ((double) mSampleRate, kHighShelfHz, 0.7071f, highGain);
}

void CompressorDSP::applyCsSustainMacro()
{
    // QA-EffectsReview Task 6: CS-3 Sustain = pre-amplification INTO a fixed
    // threshold (not a threshold drop).  More sustain -> more drive into the
    // high-ratio comp (= harder squash + more sustain) + a touch of makeup.
    //   s01 = 0  -> drive  0 dB, makeup 0 dB ;  s01 = 1 -> drive +24 dB, makeup +6 dB
    // Threshold is FIXED at -24 dB; csInputDriveDb is applied CS-only in process().
    const float s01 = juce::jlimit (0.0f, 1.0f, csSustain01);
    csInputDriveDb = 24.0f * s01;
    const float newThreshold = -24.0f;
    const float newMakeup    =   6.0f * s01;
    if (threshold != newThreshold)
    {
        threshold = newThreshold;
        mThresholdSmoothed.setTargetValue (threshold);
    }
    if (makeupDb != newMakeup)
    {
        makeupDb = newMakeup;
        mMakeupDbSmoothed.setTargetValue (makeupDb);
    }
}

void CompressorDSP::setCsSustain (float sustain01)
{
    sustain01 = juce::jlimit (0.0f, 1.0f, sustain01);
    if (csSustain01 == sustain01) return;
    csSustain01 = sustain01;
    if (mType == Type::CS) applyCsSustainMacro();
}

void CompressorDSP::setCsTone (float tone01)
{
    tone01 = juce::jlimit (0.0f, 1.0f, tone01);
    if (csTone01 == tone01) return;
    csTone01 = tone01;
    updateCsToneCoefs();
}

void CompressorDSP::setCsLevel (float dB)
{
    dB = juce::jlimit (-12.0f, 12.0f, dB);
    if (csLevelDb == dB) return;
    csLevelDb = dB;
}

// -----------------------------------------------------------------------------
void CompressorDSP::reset()
{
    mEnvL        = 0.0f;
    mEnvR        = 0.0f;
    mTCRLevel    = 0.0f;
    mGainReductionDb.store (0.0f);
    mRunningRmsL = 0.0f;
    mRunningRmsR = 0.0f;
    mPeakL       = 0.0f;
    mPeakR       = 0.0f;
    mOptoHistory = 0.0f;
    if (!mLookaheadL.empty()) std::fill(mLookaheadL.begin(), mLookaheadL.end(), 0.0f);
    if (!mLookaheadR.empty()) std::fill(mLookaheadR.begin(), mLookaheadR.end(), 0.0f);
    mLAWritePos = 0;
    mScHpfL.reset();
    mScHpfR.reset();
    // I-4: clear tilt-EQ filter state so a transport reset doesn't ring on
    // resume.
    mCsLowShelf .reset();
    mCsHighShelf.reset();
}

// -----------------------------------------------------------------------------
void CompressorDSP::calcCoefs()
{
    // One-pole smoothing coefficients: coef = exp(-1 / (time_s * sr))
    const double sr = (mSampleRate > 0.0) ? mSampleRate : 44100.0;
    mAttackCoef  = static_cast<float> (std::exp (-1.0 / (attackMs   * 0.001 * sr)));
    mReleaseCoef = static_cast<float> (std::exp (-1.0 / (releaseMs  * 0.001 * sr)));
    // C4 -- RMS smoother time constant is now user-controlled (detectionMs).
    mRmsCoef     = static_cast<float> (std::exp (-1.0 / (detectionMs * 0.001 * sr)));

    // A6 -- TCR coefficients depend only on sample rate; moved here from process().
    mTcrAttCoef  = static_cast<float>(std::exp(-1.0 / (0.5 * sr)));
    mTcrRelCoef  = static_cast<float>(std::exp(-1.0 / (1.0 * sr)));

    // C3 -- Peak detector: fast attack (~0.1 ms), slow release (= detectionMs).
    mPeakAttCoef = static_cast<float>(std::exp(-1.0 / (0.0001 * sr)));
    mPeakRelCoef = static_cast<float>(std::exp(-1.0 / (detectionMs * 0.001 * sr)));

    // H-2 -- Opto multi-stage release coefficients.  60 ms fast / 3 s slow,
    // blended by mOptoHistory which itself averages over ~1 sec.  Mirrors the
    // LA-2A two-stage release: ~60 ms to half, then a long program-dependent
    // crawl (real T4 cell is 0.5-5 s).  QA-EffectsReview Task 6: slow stage
    // lengthened 500 ms -> 3 s for the signature long LA-2A tail.
    mOptoFastRelCoef   = static_cast<float>(std::exp(-1.0 / (0.060 * sr)));
    mOptoSlowRelCoef   = static_cast<float>(std::exp(-1.0 / (3.000 * sr)));
    mOptoHistoryCoef   = static_cast<float>(std::exp(-1.0 / (1.000 * sr)));
}

// -----------------------------------------------------------------------------
void CompressorDSP::updateScHpfCoefs()
{
    if (mSampleRate <= 0.0) return;
    const float fc = juce::jlimit(20.0f, 2000.0f, sidechainHPF);
    mScHpfL.setCutoffFrequency(fc);
    mScHpfR.setCutoffFrequency(fc);
    // Butterworth Q - same as transparent StateVariable default
    mScHpfL.setResonance(0.7071f);
    mScHpfR.setResonance(0.7071f);
}

// -----------------------------------------------------------------------------
float CompressorDSP::computeGainDb (float levelDb) const noexcept
{
    // Use smoothed values where we snapshot them (levelDb and ratio come in
    // separately); this function reads the smoothed threshold/ratio via the
    // public members which are updated each sample by the caller.
    const float overshoot = levelDb - threshold;

    // Opto (LA-2A): level-dependent RISING ratio -- the T4 cell's nonlinear,
    // program-dependent curve, not a fixed ratio.  Comp rises 1.5:1 -> 4:1 (~3:1
    // average, the LA-2A Compress spec); Limit rises 8:1 -> 80:1 -- a SOFT ceiling
    // that holds the output within ~0.25 dB of threshold at the top (actually
    // limiting, but optical/musical, not a brickwall).  Mode = the optoLimit flag
    // (NOT ratio: setRatio clamps to 30, so a ratio>50 sentinel was unreachable).
    // Returns GR directly; the ~10 ms optical attack holds sustained level, not peaks.
    if (mType == Type::Opto)
    {
        if (overshoot <= 0.0f) return 0.0f;
        const float t    = juce::jlimit (0.0f, 1.0f, overshoot / 20.0f);
        const float effR = optoLimit ? (8.0f + t * (80.0f - 8.0f))
                                     : (1.5f + t * (4.0f  - 1.5f));
        return (1.0f / effR - 1.0f) * overshoot;   // negative dB gain reduction
    }

    // Vintage optical model (knee types 2 and 6): above threshold the ratio
    // relaxes from the user's ratio toward a gentle 2:1 floor over 12 dB,
    // mimicking electro-optical softening.  QA-EffectsReview Task 6 (a): the
    // relax target was 1:1, which zeroed gain reduction on peaks > 12 dB over
    // threshold (GR humped, then collapsed -- the comp stopped compressing
    // exactly when the signal was loudest).  Flooring at 2:1 keeps the slope
    // < 1 so output stays monotonic and loud peaks are still caught.  floorRatio
    // = min(ratio,2): never relax a ratio already <= 2:1 (nothing to soften), so
    // low ratios + upward expansion keep the user's setting.
    float effectiveRatio = ratio;

    // FET (1176) all-buttons-in: a program-dependent RISING ratio -- the bias
    // shift makes the effective ratio climb with level, and that shifting curve
    // (not a fixed number) is the all-buttons character.  Aggressive-but-
    // controlled: ~8:1 at threshold rising to ~20:1 by +18 dB over, capped there
    // (limits hard but the curve stays monotonic).  Overrides the button ratio.
    if (mType == Type::FET && fetAllButtons && overshoot > 0.0f)
    {
        constexpr float kAbBase = 8.0f;    // ratio at threshold
        constexpr float kAbTop  = 20.0f;   // ratio ceiling
        constexpr float kAbSpan = 18.0f;   // dB of overshoot to reach the ceiling
        const float t = juce::jlimit (0.0f, 1.0f, overshoot / kAbSpan);
        effectiveRatio = kAbBase + t * (kAbTop - kAbBase);
    }

    // Vintage knee is a Modern-algorithm character only (the FET/Opto/CS panels
    // never expose the knee selector; FET forces a hard knee).  Gate on Modern
    // so a stale mKneeType can't apply the optical taper to another Type.
    const bool isVintage = (mType == Type::Modern) && (mKneeType == 2 || mKneeType == 6);
    if (isVintage && overshoot > 0.0f)
    {
        const float floorRatio = std::min (ratio, 2.0f);
        const float t = juce::jlimit (0.0f, 1.0f, overshoot / 12.0f);
        effectiveRatio = ratio + t * (floorRatio - ratio);   // ratio -> floorRatio over 12 dB
        effectiveRatio = std::max (effectiveRatio, floorRatio);
    }

    if (kneeDb > 0.0f)
    {
        // Soft-knee region: [threshold - knee/2 .. threshold + knee/2]
        const float halfKnee = kneeDb * 0.5f;
        if (overshoot < -halfKnee)
            return 0.0f;                        // below knee - no gain change

        if (overshoot <= halfKnee)
        {
            // Inside the knee: quadratic interpolation
            const float x = (overshoot + halfKnee) / kneeDb; // 0..1
            const float gainChange = (1.0f / effectiveRatio - 1.0f) * (x * x) * halfKnee;
            return gainChange;
        }
    }
    else
    {
        if (overshoot <= 0.0f)
            return 0.0f;
    }

    // Above knee (or hard-knee above threshold)
    return threshold + overshoot / effectiveRatio - levelDb;
}

// -----------------------------------------------------------------------------
void CompressorDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed)
        return;

    juce::ScopedNoDenormals noDenormals;  // A3

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0)
        return;

    // C.4 Phase 1 (2026-04-30): pull SC source from the strip's SC array via
    // mScPick (set each block by EffectRack from slot.scPick).  Overrides
    // any legacy setSidechainBuffer / setUseSidechain wiring -- the strip's
    // SC array is the single source of truth now.  scPick == -1 (no source
    // selected) leaves SC off and detection falls back to the input buffer.
    if (auto* scBuf = getActiveSidechain())
    {
        useSidechain     = true;
        mSidechainBuffer = scBuf;
    }
    else
    {
        useSidechain     = false;
        mSidechainBuffer = nullptr;
    }

    // -- Detection source (sidechain or input) -----------------------------
    const juce::AudioBuffer<float>* detSrc = &buffer;
    if (useSidechain && mSidechainBuffer != nullptr &&
        mSidechainBuffer->getNumSamples() >= numSamples &&
        mSidechainBuffer->getNumChannels() > 0)
    {
        detSrc = mSidechainBuffer;
    }

    const int    detChans = detSrc->getNumChannels();
    const float* detL     = detSrc->getReadPointer(0);
    const float* detR     = (detChans > 1) ? detSrc->getReadPointer(1) : detL;

    // 2c -- Auto-makeup: compute once per block from current params.
    // Reference formula: makeup = -computeGainDb(threshold + 6 dB). This matches
    // FL Studio / Ableton behavior - the compressor compensates for the gain
    // reduction a signal sitting 6 dB above threshold would see. That reference
    // is closer to typical program level than 0 dBFS, so quiet passages aren't
    // artificially boosted louder than the uncompressed source, while heavy
    // transients still get audibly squashed.
    const float manualMakeupLin = juce::Decibels::decibelsToGain(makeupDb);
    float makeupLinBlock = manualMakeupLin;
    if (autoMakeup)
    {
        const float referenceDb   = threshold + 6.0f;
        const float autoMakeupDb  = -computeGainDb(referenceDb);
        makeupLinBlock = juce::Decibels::decibelsToGain(autoMakeupDb);
    }

    const bool isTCR = (mKneeType >= 4);

    const float att  = mAttackCoef;
    const float rel  = mReleaseCoef;
    const float rmsC = mRmsCoef;
    const float pAtt = mPeakAttCoef;
    const float pRel = mPeakRelCoef;

    const double sr = (mSampleRate > 0.0) ? mSampleRate : 44100.0;

    float* outL = buffer.getWritePointer(0);
    float* outR = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    // 2a -- Look-ahead delay line (main path only)
    const int  laSize       = (int)mLookaheadL.size();
    const int  laSamples    = mLASamples;
    const bool useLookahead = (laSamples > 0 && laSize > 0);

    // C3 -- Detection helpers.
    auto msToDb = [](float ms) { return 10.0f * std::log10(std::max(ms, 1e-12f)); };

    // Safety clamp on detected level: physically impossible for audio to exceed
    // +60 dBFS in any sane signal path, and a -120 dB floor beats the denormal
    // edge case if a log-domain calculation ever trips a subnormal.
    auto clampLevelDb = [](float db) {
        return juce::jlimit(-120.0f, 60.0f, db);
    };

    // CS-3 Sustain pre-amplification: drive the signal INTO the fixed threshold.
    // csInputDriveDb (= 24 * sustain) lifts BOTH the detected level and the audio
    // so the high-ratio comp squashes the driven signal (the "dig in" sustain).
    // 0 for every non-CS Type -> no-op.
    const float csDriveDb = (mType == Type::CS) ? csInputDriveDb : 0.0f;
    const float csDriveGn = juce::Decibels::decibelsToGain (csDriveDb);

    for (int i = 0; i < numSamples; ++i)
    {
        // -- Continuous-param smoothers (A2) --------------------------------
        threshold = mThresholdSmoothed.getNextValue();
        ratio     = mRatioSmoothed    .getNextValue();
        const float curMix      = mMixSmoothed   .getNextValue();
        const float curMakeupDb = mMakeupDbSmoothed.getNextValue();

        // -- 1. Detection from (un-delayed) detection source --------------
        float dL = detL[i];
        float dR = detR[i];

        // C2 -- Sidechain HPF on detection source. When cutoff is near the
        // 20 Hz floor, it's effectively a pass-through; still costs a couple
        // of multiplies per sample but no audible effect.
        dL = mScHpfL.processSample(0, dL);
        dR = mScHpfR.processSample(0, dR);

        float levelDbL, levelDbR;

        if (peakDetection)
        {
            // C3 -- Peak detection with fast attack / slow release.
            const float absL = std::abs(dL);
            const float absR = std::abs(dR);
            const float coefL = (absL > mPeakL) ? pAtt : pRel;
            const float coefR = (absR > mPeakR) ? pAtt : pRel;
            mPeakL = coefL * mPeakL + (1.0f - coefL) * absL;
            mPeakR = coefR * mPeakR + (1.0f - coefR) * absR;

            if (stereoLink)
            {
                const float pk = std::max(mPeakL, mPeakR);
                levelDbL = levelDbR = 20.0f * std::log10(std::max(pk, 1e-9f));
            }
            else
            {
                levelDbL = 20.0f * std::log10(std::max(mPeakL, 1e-9f));
                levelDbR = 20.0f * std::log10(std::max(mPeakR, 1e-9f));
            }
        }
        else
        {
            // 2d -- Per-sample running mean-square (default, classic).
            mRunningRmsL = rmsC * mRunningRmsL + (1.0f - rmsC) * dL * dL;
            mRunningRmsR = rmsC * mRunningRmsR + (1.0f - rmsC) * dR * dR;

            if (stereoLink)
            {
                const float msLink = std::max(mRunningRmsL, mRunningRmsR);
                levelDbL = levelDbR = msToDb(msLink);
            }
            else
            {
                levelDbL = msToDb(mRunningRmsL);
                levelDbR = msToDb(mRunningRmsR);
            }
        }

        // CS-3 Sustain: lift the detected level so more of the signal exceeds the
        // fixed threshold (harder compression as Sustain rises).  0 for non-CS.
        levelDbL += csDriveDb;
        levelDbR += csDriveDb;

        // Safety clamp before gain computer.
        levelDbL = clampLevelDb(levelDbL);
        levelDbR = clampLevelDb(levelDbR);

        // Slow TCR tracker (release acceleration). TCR tracks magnitude
        // regardless of detection mode; use current running RMS level for
        // comparability with v1 behavior in RMS mode, or peak envelope in peak mode.
        if (isTCR)
        {
            const float curLevel = peakDetection
                ? std::max(mPeakL, mPeakR)
                : std::sqrt(std::max(mRunningRmsL, mRunningRmsR));
            const float coef = (curLevel > mTCRLevel) ? mTcrAttCoef : mTcrRelCoef;
            mTCRLevel = coef * mTCRLevel + (1.0f - coef) * curLevel;
        }

        // -- 2. Gain computer ---------------------------------------------
        const float targetDbL = computeGainDb(levelDbL);
        const float targetDbR = computeGainDb(levelDbR);

        // -- 3. Per-sample envelope smoothing (attack/release) -----------
        // H-2 (2026-05-01): Type umbrella character modes alter release path:
        //   Opto -- blend fast (60ms) + slow (500ms) release coefs based on
        //           mOptoHistory.  Sustained material -> more slow weight ->
        //           characteristic LA-2A "memory" recovery.
        //   FET / Modern -- standard release coef (or TCR dynamic when /R kneeType active).
        auto smoothOne = [&](float& env, float target) -> float
        {
            const bool releasing = (target >= env);
            float rCoef = rel;
            if (mType == Type::Opto && releasing)
            {
                // mOptoHistory is the running magnitude of GR (positive dB).
                // Map 0..6 dB GR history to 0..1 weight, blend the two coefs.
                const float w = juce::jlimit (0.0f, 1.0f, mOptoHistory / 6.0f);
                rCoef = (1.0f - w) * mOptoFastRelCoef + w * mOptoSlowRelCoef;
            }
            else if (isTCR && releasing)
            {
                const float tcrNorm = juce::jlimit(0.0f, 1.0f, mTCRLevel / 0.1f);
                const float dynRel  = std::max(1.0f, releaseMs / (1.0f + tcrNorm * 4.0f));
                rCoef = static_cast<float>(std::exp(-1.0 / (dynRel * 0.001 * sr)));
            }
            const float c = (target < env) ? att : rCoef;
            env = c * env + (1.0f - c) * target;
            return env;
        };

        float grL = smoothOne(mEnvL, targetDbL);
        float grR = stereoLink ? (mEnvR = grL) : smoothOne(mEnvR, targetDbR);

        // H-2 -- Opto memory tracker: one-pole running magnitude of current GR
        // over a ~1 sec window.  Drives the fast/slow release blend on next
        // sample.  Always updated when Type=Opto, even outside release phase,
        // so sustained material's history is always current.
        if (mType == Type::Opto)
        {
            const float curGrMag = -std::min (grL, grR);   // negative GR -> positive magnitude
            mOptoHistory = mOptoHistoryCoef * mOptoHistory
                         + (1.0f - mOptoHistoryCoef) * curGrMag;
        }

        // -- 4. Apply to look-ahead-delayed audio + makeup + mix ---------
        const float drySrcL = outL[i];
        const float drySrcR = outR ? outR[i] : drySrcL;

        float audioL = drySrcL;
        float audioR = drySrcR;
        if (useLookahead)
        {
            const int readPos = (mLAWritePos + laSize - laSamples) % laSize;
            audioL = mLookaheadL[(size_t)readPos];
            audioR = mLookaheadR[(size_t)readPos];
            mLookaheadL[(size_t)mLAWritePos] = drySrcL;
            mLookaheadR[(size_t)mLAWritePos] = drySrcR;
            mLAWritePos = (mLAWritePos + 1) % laSize;
        }

        // Manual makeup uses the smoothed makeup knob; auto-makeup is computed
        // once per block above and applied uniformly.
        const float makeupLin = autoMakeup
            ? makeupLinBlock
            : juce::Decibels::decibelsToGain(curMakeupDb);

        const float gL = juce::Decibels::decibelsToGain(grL) * makeupLin;
        const float gR = juce::Decibels::decibelsToGain(grR) * makeupLin;

        // CS-3 Sustain pre-amp: drive the audio into the comp (matches the detector
        // drive) so the output is the driven-then-compressed signal.  Unity for non-CS.
        audioL *= csDriveGn;
        audioR *= csDriveGn;

        float wetL = audioL * gL;
        float wetR = audioR * gR;

        // FET (1176) gain-stage color: always-on tanh harmonic shaping on the WET
        // output (QA-EffectsReview Task 6 moved it here from the GR control signal,
        // where it was gated > 6 dB GR and nearly inaudible).  /(1+drv) normalizes
        // to UNITY at low level (no rest-level jump); drive grows with GR depth
        // (-gr) but is CAPPED at 12 dB GR so heavy all-buttons crush colors rather
        // than fuzzes out (aggressive-but-controlled).  gr is negative dB.
        if (mType == Type::FET)
        {
            const float grCapL = std::min (12.0f, std::max (0.0f, -grL));
            const float grCapR = std::min (12.0f, std::max (0.0f, -grR));
            const float drvL = 0.08f + 0.05f * grCapL;
            const float drvR = 0.08f + 0.05f * grCapR;
            wetL = std::tanh (wetL * (1.0f + drvL)) / (1.0f + drvL);
            wetR = std::tanh (wetR * (1.0f + drvR)) / (1.0f + drvR);
        }

        // Opto (LA-2A) tube warmth: gentle EVEN-harmonic (2nd) coloration via an
        // asymmetric tanh -- the small bias offset makes the curve asymmetric
        // (generates 2nd harmonic); subtracting tanh(bias) removes the static
        // offset and /(1+drv) keeps low level ~unity (no level jump).  Drive +
        // bias grow subtly with GR depth, capped (LA-2A is a gentle leveler).
        if (mType == Type::Opto)
        {
            const float grCapL = std::min (12.0f, std::max (0.0f, -grL));
            const float grCapR = std::min (12.0f, std::max (0.0f, -grR));
            const float drvL  = 0.04f + 0.03f * grCapL;
            const float drvR  = 0.04f + 0.03f * grCapR;
            const float biasL = 0.20f * drvL;
            const float biasR = 0.20f * drvR;
            wetL = (std::tanh (wetL * (1.0f + drvL) + biasL) - std::tanh (biasL)) / (1.0f + drvL);
            wetR = (std::tanh (wetR * (1.0f + drvR) + biasR) - std::tanh (biasR)) / (1.0f + drvR);
        }

        outL[i] = audioL + curMix * (wetL - audioL);
        if (outR) outR[i] = audioR + curMix * (wetR - audioR);
    }

    // I-4 (2026-05-02): CS Style post-comp tilt EQ + output Level trim.
    // Tilt EQ sits on the compressed signal (BOSS CS-3 Tone-pot semantic);
    // Level trim runs LAST so it nets the user-visible "how loud is this
    // pedal" knob.  Both stages active only when mType == CS.
    if (mType == Type::CS && numChannels >= 1)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        mCsLowShelf .process (ctx);
        mCsHighShelf.process (ctx);
        if (csLevelDb != 0.0f)
            buffer.applyGain (juce::Decibels::decibelsToGain (csLevelDb, -60.0f));
    }

    // A4 -- GR meter hold + decay (30 dB/sec). max-negative is the deepest GR
    // seen this block; keep the deepest value but let it decay back toward 0
    // so the meter both catches transient dips AND returns to rest visibly.
    const float blockGr   = std::min(mEnvL, mEnvR);
    const float prevGr    = mGainReductionDb.load(std::memory_order_relaxed);
    // GR is <=0; "deeper" means more negative; "decay toward rest" = toward 0.
    const float decayedGr = juce::jmin(0.0f, prevGr + mGrDecayDbPerBlock);
    mGainReductionDb.store(juce::jmin(blockGr, decayedGr), std::memory_order_relaxed);
}

// -----------------------------------------------------------------------------
float CompressorDSP::getGainReductionDb() const
{
    return mGainReductionDb.load();
}

// -----------------------------------------------------------------------------
// Setters (A1 -- all CPU-guarded: no-op if value unchanged)
// -----------------------------------------------------------------------------
void CompressorDSP::setThreshold (float dB)
{
    if (threshold != dB)
    {
        threshold = dB;
        mThresholdSmoothed.setTargetValue(dB);
    }
}

void CompressorDSP::setRatio (float r)
{
    r = juce::jlimit (0.4f, 30.0f, r);
    if (ratio != r)
    {
        ratio = r;
        mRatioSmoothed.setTargetValue(r);
    }
}

void CompressorDSP::setAttack (float ms)
{
    ms = juce::jlimit (0.0f, 400.0f, ms);
    if (attackMs != ms)
    {
        attackMs = ms;
        // CS-3: the Attack knob also sets release, inversely (fast attack -> long
        // release = sustain; slow attack -> short release = punch).
        if (mType == Type::CS)
            releaseMs = csReleaseFromAttackMs (attackMs);
        calcCoefs();
    }
}

void CompressorDSP::setRelease (float ms)
{
    ms = juce::jlimit (1.0f, 4000.0f, ms);
    if (releaseMs != ms)
    {
        releaseMs = ms;
        calcCoefs();
    }
}

void CompressorDSP::setMakeup (float dB)
{
    // A5 -- legacy setter now matches setGain clamp to prevent runaway gain.
    dB = juce::jlimit (-30.0f, 30.0f, dB);
    if (makeupDb != dB)
    {
        makeupDb = dB;
        mMakeupDbSmoothed.setTargetValue(dB);
    }
}

void CompressorDSP::setGain (float dB)
{
    dB = juce::jlimit (-30.0f, 30.0f, dB);
    if (makeupDb != dB)
    {
        makeupDb = dB;
        mMakeupDbSmoothed.setTargetValue(dB);
    }
}

void CompressorDSP::setKnee (float dB)
{
    dB = std::max (0.0f, dB);
    if (kneeDb != dB) kneeDb = dB;
}

void CompressorDSP::setMix (float m)
{
    m = juce::jlimit (0.0f, 1.0f, m);
    if (mix != m)
    {
        mix = m;
        mMixSmoothed.setTargetValue(m);
    }
}

void CompressorDSP::setKneeType (int type)
{
    type = juce::jlimit (0, 7, type);
    if (mKneeType == type) return;
    mKneeType = type;
    // Map type index to knee width in dB
    // Types 0-3: Hard/Medium/Vintage/Soft. Types 4-7 = /R variants of same widths.
    static constexpr float kWidths[4] = { 0.0f, 6.0f, 7.0f, 15.0f };
    kneeDb = kWidths[mKneeType % 4];
}

void CompressorDSP::setLookaheadMs (float ms)
{
    const float n = juce::jlimit (0.0f, kMaxLookaheadMs, ms);
    if (n == lookaheadMs) return;
    lookaheadMs = n;
    if (mSampleRate > 0.0 && !mLookaheadL.empty())
    {
        // Compute the new sample count into a local first so an audio-thread
        // read of mLASamples mid-update can't observe a half-computed value.
        const int newLA = juce::jlimit (0,
                                        (int) mLookaheadL.size() - 1,
                                        static_cast<int> (lookaheadMs * 0.001 * mSampleRate));
        mLASamples = newLA;
    }
}

void CompressorDSP::setStereoLink  (bool on) { if (on != stereoLink) stereoLink = on; }
void CompressorDSP::setAutoMakeup  (bool on) { if (on != autoMakeup) autoMakeup = on; }
void CompressorDSP::setUseSidechain (bool on) { if (on != useSidechain) useSidechain = on; }

void CompressorDSP::setSidechainHPF (float hz)
{
    hz = juce::jlimit (20.0f, 2000.0f, hz);
    if (sidechainHPF != hz)
    {
        sidechainHPF = hz;
        updateScHpfCoefs();
    }
}

void CompressorDSP::setPeakDetection (bool on)
{
    if (on != peakDetection) peakDetection = on;
}

void CompressorDSP::setFetAllButtons (bool on)
{
    if (on != fetAllButtons) fetAllButtons = on;
}

void CompressorDSP::setOptoLimit (bool on)
{
    if (on != optoLimit) optoLimit = on;
}

void CompressorDSP::setDetectionMs (float ms)
{
    ms = juce::jlimit (1.0f, 100.0f, ms);
    if (detectionMs != ms)
    {
        detectionMs = ms;
        calcCoefs();
    }
}

void CompressorDSP::setSidechainSourceId (int channelId)
{
    // -1 = internal detection path (no external source selected).
    // No clamp on positive range: MixerChannelIds space is 0..999 and will
    // expand; we rely on VibeGraph to validate at routing time.
    if (channelId < -1) channelId = -1;
    if (sidechainSourceId != channelId) sidechainSourceId = channelId;
}

void CompressorDSP::setSidechainBuffer (juce::AudioBuffer<float>* buf)
{
    mSidechainBuffer = buf;
}

// -----------------------------------------------------------------------------
void CompressorDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("CompressorDSP");
    state.setProperty ("threshold",     threshold,           nullptr);
    state.setProperty ("ratio",         ratio,               nullptr);
    state.setProperty ("attackMs",      attackMs,            nullptr);
    state.setProperty ("releaseMs",     releaseMs,           nullptr);
    state.setProperty ("makeupDb",      makeupDb,            nullptr);
    state.setProperty ("kneeDb",        kneeDb,              nullptr);
    state.setProperty ("mix",           mix,                 nullptr);
    state.setProperty ("useSidechain",  (int)useSidechain,   nullptr);
    state.setProperty ("kneeType",      mKneeType,           nullptr);
    state.setProperty ("bypassed",      (int)bypassed,       nullptr);
    state.setProperty ("lookaheadMs",   lookaheadMs,         nullptr);
    state.setProperty ("stereoLink",    (int)stereoLink,     nullptr);
    state.setProperty ("autoMakeup",    (int)autoMakeup,     nullptr);
    // C2/C3/C4/scaffolding
    state.setProperty ("sidechainHPF",  sidechainHPF,        nullptr);
    state.setProperty ("peakDetection", (int)peakDetection,  nullptr);
    state.setProperty ("detectionMs",   detectionMs,         nullptr);
    state.setProperty ("scSourceId",    sidechainSourceId,   nullptr);
    // H-2 (2026-05-01) -- Type umbrella character mode (Modern/FET/Opto/CS).
    // Default 0 = Modern preserves old presets bit-exact on load.
    state.setProperty ("type",          (int) mType,         nullptr);
    state.setProperty ("fetAllButtons", (int) fetAllButtons, nullptr);
    state.setProperty ("optoLimit",     (int) optoLimit,     nullptr);
    // I-4 (2026-05-02) -- CS Style state.  Persisted regardless of active
    // Type so a user who set up CS Style settings, switched to FET briefly,
    // and saved doesn't lose their CS values on reload.
    state.setProperty ("csSustain",     csSustain01,         nullptr);
    state.setProperty ("csTone",        csTone01,            nullptr);
    state.setProperty ("csLevel",       csLevelDb,           nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml)
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

// -----------------------------------------------------------------------------
void CompressorDSP::setStateInformation (const void* data, int sz)
{
    std::unique_ptr<juce::XmlElement> xml (juce::AudioProcessor::getXmlFromBinary (data, sz));
    if (!xml || !xml->hasTagName ("CompressorDSP"))
        return;

    juce::ValueTree state = juce::ValueTree::fromXml (*xml);
    threshold    = state.getProperty ("threshold",    threshold);
    ratio        = state.getProperty ("ratio",        ratio);
    attackMs     = state.getProperty ("attackMs",     attackMs);
    releaseMs    = state.getProperty ("releaseMs",    releaseMs);
    makeupDb     = state.getProperty ("makeupDb",     makeupDb);
    kneeDb       = state.getProperty ("kneeDb",       kneeDb);
    mix          = state.getProperty ("mix",          mix);
    useSidechain = ((int)state.getProperty ("useSidechain", 0)) != 0;
    mKneeType    = (int)state.getProperty ("kneeType", mKneeType);
    bypassed     = ((int)state.getProperty ("bypassed",    0)) != 0;
    lookaheadMs  = (float)(double)state.getProperty ("lookaheadMs", 0.0);
    stereoLink   = ((int)state.getProperty ("stereoLink",  1)) != 0;
    autoMakeup   = ((int)state.getProperty ("autoMakeup",  0)) != 0;
    sidechainHPF       = (float)(double)state.getProperty ("sidechainHPF",  20.0);
    peakDetection      = ((int)state.getProperty ("peakDetection", 0)) != 0;
    detectionMs        = (float)(double)state.getProperty ("detectionMs",   10.0);
    sidechainSourceId  = (int)state.getProperty ("scSourceId",  -1);
    // H-2 -- Type umbrella; absent in old projects (default 0 = Modern).
    // I-4: range bumped 0..2 -> 0..3 to admit CS Style.
    mType = static_cast<Type> (juce::jlimit (0, 3,
        (int) state.getProperty ("type", 0)));
    mOptoHistory = 0.0f;
    // QA-EffectsReview Task 6: mirror setType's FET coercion on the load path
    // (setStateInformation sets mType directly, bypassing setType), so old FET
    // presets saved before peak/hard-knee was forced still load correct.
    if (mType == Type::FET)
    {
        peakDetection = true;
        kneeDb        = 0.0f;
    }
    // All-buttons-in flag.  Legacy FET all-buttons presets stored a high sentinel
    // ratio (e.g. 1000) with no flag, so treat ratio > 25 on a FET preset as
    // all-buttons -> the program-dependent rising-ratio curve.
    fetAllButtons = ((int) state.getProperty ("fetAllButtons", 0)) != 0;
    if (mType == Type::FET && ratio > 25.0f)
        fetAllButtons = true;
    // Opto Comp/Limit flag.  Legacy Opto Limit presets stored a high ratio (panel
    // wrote 100, clamped to 30 by setRatio), so ratio > 16 on an Opto preset
    // migrates to the flag.
    optoLimit = ((int) state.getProperty ("optoLimit", 0)) != 0;
    if (mType == Type::Opto && ratio > 16.0f)
        optoLimit = true;
    // I-4 -- CS Style state.  Defaults preserve neutral behaviour for old
    // projects (csSustain=0 => light comp; csTone=0.5 => flat tilt EQ).
    csSustain01 = (float)(double) state.getProperty ("csSustain", 0.0);
    csTone01    = (float)(double) state.getProperty ("csTone",    0.5);
    csLevelDb   = (float)(double) state.getProperty ("csLevel",   0.0);
    // QA-EffectsReview Task 6: CS-3 derives release from attack + the Sustain
    // macro now drives a fixed threshold (recompute csInputDriveDb).  Re-applying
    // here migrates old CS presets (threshold-drop -> input-drive) on load.
    if (mType == Type::CS)
    {
        releaseMs = csReleaseFromAttackMs (attackMs);
        applyCsSustainMacro();
    }

    // Rebuild derived state
    if (mSampleRate > 0.0 && !mLookaheadL.empty())
        mLASamples = juce::jlimit(0,
                                  (int)mLookaheadL.size() - 1,
                                  static_cast<int>(lookaheadMs * 0.001 * mSampleRate));

    calcCoefs();
    updateScHpfCoefs();
    // I-4: refresh CS tilt-EQ coefs from the just-loaded csTone01.
    updateCsToneCoefs();
    // Snap smoothers to loaded values (no ramp on state load).
    mThresholdSmoothed.setCurrentAndTargetValue(threshold);
    mRatioSmoothed    .setCurrentAndTargetValue(ratio);
    mMixSmoothed      .setCurrentAndTargetValue(mix);
    mMakeupDbSmoothed .setCurrentAndTargetValue(makeupDb);
    // Clear filter state so coefficient jump doesn't click.
    mScHpfL.reset();
    mScHpfR.reset();
    mCsLowShelf .reset();
    mCsHighShelf.reset();
}
