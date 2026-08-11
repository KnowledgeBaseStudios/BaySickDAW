#include "LimiterDSP.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kMaxAheadMs = 10.0f;
    constexpr int   kOsFactor   = 4;      // 4× oversampling for TP detection

    // CL-243 character table.  Index 0 (Clean) reproduces the pre-CL-243 fixed
    // constants exactly -- 20 ms / 300 ms envelopes, a 6 dB auto-release blend
    // knee, no curve offset, no release scaling, no added saturation -- so every
    // preset written before this table restores character 0 and sounds identical.
    //
    // The other seven trade transparency against density and colour.  Read the
    // columns as: how quickly the fast envelope lets go, how slowly the slow one
    // does, how deep the GR has to be before the slow envelope takes over, which
    // way the release curve leans, how the user's Release time is scaled, and how
    // much of the mode's own soft saturation is dialled in.
    constexpr LimiterDSP::CharacterProfile kCharacters[(size_t) LimiterDSP::Character::Count] =
    {
        // name       relFast relSlow knee  curveOff relScale sat
        // (Instant's short envelopes are what make it usable with Ahead at 0;
        //  the old needsLookahead column encoded that and nothing read it.)
        { "Clean",      20.f,  300.f,  6.0f,  0.00f,  1.00f,  0.00f },
        { "Smooth",     40.f,  600.f, 10.0f,  0.30f,  1.40f,  0.00f },
        { "Tight",      12.f,  180.f,  5.0f, -0.15f,  0.70f,  0.00f },
        { "Punch",       8.f,  250.f,  3.0f, -0.25f,  0.80f,  0.00f },
        { "Glue",       60.f,  800.f, 12.0f,  0.30f,  1.60f,  0.00f },
        { "Loud",        6.f,  120.f,  4.0f, -0.30f,  0.50f,  0.35f },
        { "Warm",       25.f,  350.f,  7.0f,  0.15f,  1.10f,  0.55f },
        { "Instant",     2.f,   40.f,  2.0f, -0.40f,  0.30f,  0.20f },
    };
}

const LimiterDSP::CharacterProfile& LimiterDSP::profileFor (Character c) noexcept
{
    const auto i = (size_t) juce::jlimit (0, (int) Character::Count - 1, (int) c);
    return kCharacters[i];
}

const char* LimiterDSP::characterName (int index) noexcept
{
    return kCharacters[(size_t) juce::jlimit (0, (int) Character::Count - 1, index)].name;
}

const char* LimiterDSP::modeName (int index) noexcept
{
    return index == 1 ? "Maximizer" : "Limiter";
}

void LimiterDSP::setMode (Mode m)
{
    const auto n = (m == Mode::Maximizer) ? Mode::Maximizer : Mode::Limiter;
    if (n == mMode) return;
    mMode = n;

    // Leaving Maximizer must drop the LIVE servo state, not the stored settings:
    // an input-gain trim or ceiling trim still applied while its controls are
    // hidden would make the limiter quieter or louder than the panel reads.
    if (! maximizerActive())
    {
        mServoDb      .store (0.0f);
        mCeilingTrimDb.store (0.0f);
        mOutTpDb      .store (-144.0f);
    }

    // The effective character changed with the mode (Limiter always runs Clean),
    // so the ballistics coefficients have to be rebuilt.
    recalcCoefs();
}

// ─────────────────────────────────────────────────────────────────────────────
LimiterDSP::LimiterDSP()
{
    mScHpfL.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    mScHpfR.setType (juce::dsp::StateVariableTPTFilterType::highpass);
}

void LimiterDSP::updateScHpfCoefs()
{
    if (mSampleRate <= 0.0) return;
    const float fc = juce::jlimit (20.0f, 2000.0f, mSidechainHPF);
    mScHpfL.setCutoffFrequency (fc);
    mScHpfR.setCutoffFrequency (fc);
    // Butterworth-ish: transparent rolloff
    mScHpfL.setResonance (0.7071f);
    mScHpfR.setResonance (0.7071f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Soft saturator: unity small-signal gain, tanh soft-clip with knee curve.
// drive  = 0       → identity (saturator off)
// drive  > 0       → tanh(drive*x) / drive  (unity gain at small x, soft-clips toward 1/drive)
// curve  ∈ [0, 1]  → blends between linear-ish (curve=0) and heavier saturation (curve=1)
float LimiterDSP::softSat (float x, float drive, float curve)
{
    if (drive < 0.001f) return x;
    // curve shapes the effective drive: higher curve → steeper knee
    const float effDrive = drive * (1.0f + curve * 3.0f);
    return std::tanh (effDrive * x) / effDrive;
}

// ─────────────────────────────────────────────────────────────────────────────
void LimiterDSP::recalcCoefs()
{
    if (mSampleRate <= 0.0) return;

    const double sr = mSampleRate;
    // Exponential time-constant: y[n] = coef*y[n-1] + (1-coef)*target  → 63% in timeMs
    auto expCoef = [sr] (float ms)
    {
        ms = std::max (0.01f, ms);
        return (float) std::exp (-1.0 / ((double) ms * 0.001 * sr));
    };

    // CL-243: the character supplies the envelope pair and scales the user's
    // release time.  Clean's scale is 1.0 and its pair is the old 20/300, so this
    // is a no-op at the default -- and Limiter mode always resolves to Clean.
    const auto& prof = profileFor (effectiveCharacter());
    const float relMs = std::max (1.0f, mReleaseMs * prof.relScale);

    mAttackCoef  = expCoef (mAttackMs);
    mReleaseCoef = expCoef (relMs);
    mRelFastCoef = expCoef (prof.relFastMs);
    mRelSlowCoef = expCoef (prof.relSlowMs);

    // Linear-release step sizes (linear decay toward target per sample, in linear units):
    // step = 1 / (releaseMs * 0.001 * sr)  → env falls from 1→0 over releaseMs
    auto linStep = [sr] (float ms)
    {
        const float n = std::max (1.0f, (float)(ms * 0.001 * sr));
        return 1.0f / n;
    };
    mRelStepPerSample = linStep (relMs);
    mRelStepFast      = linStep (prof.relFastMs);
    mRelStepSlow      = linStep (prof.relSlowMs);

    // SUSTAIN RMS-window coef (0 = off -> coef 0 -> meanSq tracks the peak
    // instantly, so blendedPeak == peak and the hold is a no-op).
    mSusCoef = (mSustainMs <= 0.0f) ? 0.0f : expCoef (mSustainMs);
}

// ─────────────────────────────────────────────────────────────────────────────
void LimiterDSP::allocateDelay()
{
    if (mSampleRate <= 0.0) return;
    const int maxAheadSamp = (int) std::ceil ((double) kMaxAheadMs * 0.001 * mSampleRate);
    // Size = max look-ahead + one full block + safety, so the per-block write pass
    // can never overwrite delayed samples still needed by the read pass.
    mDelaySize = maxAheadSamp + juce::jmax (1, mMaxBlock) + 4;
    mDelayL.assign ((size_t) mDelaySize, 0.0f);
    mDelayR.assign ((size_t) mDelaySize, 0.0f);
    mWritePos = 0;
    mAheadSamples = juce::jlimit (0, maxAheadSamp,
                                  (int) std::round ((double) mAheadMs * 0.001 * mSampleRate));
    mLatencySamples = mAheadSamples + mOsLatencySamples;
}

// ─────────────────────────────────────────────────────────────────────────────
void LimiterDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = juce::jmax (1, maxBlockSize);

    // ── 4× oversampled True Peak detector ─────────────────────────────────────
    // factor=2 → 2^2 = 4× oversampling. Polyphase IIR = low-latency (a few samples).
    mOversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2 /* numChannels */,
        2 /* factor (log2 of ratio): 2 → 4× */,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true /* maxQuality */);
    mOversampler->initProcessing ((size_t) mMaxBlock);
    mOversampler->reset();
    mOsLatencySamples = (int) std::ceil (mOversampler->getLatencyInSamples());

    // ── Detector sidechain buffer (stereo, pre-allocated) ─────────────────────
    mScBuf.setSize (2, mMaxBlock, false, true, true);
    mScBuf.clear();
    mTpPeaks .assign ((size_t) mMaxBlock, 0.0f);
    mTpPeaksL.assign ((size_t) mMaxBlock, 0.0f);
    mTpPeaksR.assign ((size_t) mMaxBlock, 0.0f);

    // ── Look-ahead delay line ─────────────────────────────────────────────────
    allocateDelay();

    // ── SmoothedValues (15 ms linear ramp) ────────────────────────────────────
    mInputGainSmooth.reset (sampleRate, 0.015);
    mCeilingSmooth  .reset (sampleRate, 0.015);
    mSatThreshSmooth.reset (sampleRate, 0.015);
    mSatCurveSmooth .reset (sampleRate, 0.015);   // C1
    mInputGainSmooth.setCurrentAndTargetValue (mInputGainTargetDb);
    mCeilingSmooth  .setCurrentAndTargetValue (mCeilingTargetDb);
    mSatThreshSmooth.setCurrentAndTargetValue (mSatThreshTarget);
    mSatCurveSmooth .setCurrentAndTargetValue (mSatCurve);

    // TS7: output loudness (CL-244 servo + BLU-110 display) and output true peak
    // (BLU-108 auto-ceiling).  Both prepared unconditionally -- the enable flags
    // gate the per-sample COST, not the allocation, which must not happen on the
    // audio thread if the user flips a toggle mid-playback.
    mOutLufs.prepareToPlay (sampleRate);
    mOutLufs.resetIntegrated();
    mOutTp.prepare (2);
    mLufsPrepared = true;
    // ~0.5 s of hold: comfortably longer than the panel's 33 ms poke interval, so
    // a stalled message thread does not make the meter blink, and short enough
    // that a closed panel stops costing anything almost immediately.
    mLufsHoldBlocks = juce::jmax (2, (int) std::ceil (0.5 * sampleRate
                                                      / (double) mMaxBlock));

    // C2: SC HPF biquads (per-channel, mono spec each)
    juce::dsp::ProcessSpec scSpec { sampleRate,
                                    (juce::uint32) mMaxBlock,
                                    1 };
    mScHpfL.prepare (scSpec);
    mScHpfR.prepare (scSpec);
    mScHpfL.reset();
    mScHpfR.reset();
    updateScHpfCoefs();

    // A2/A3: meter hold+decay (30 dB/sec, SR-aware). Matches InsertNode pattern.
    constexpr float kDecayDbPerSec = 30.0f;
    const float blockSecs = (float) mMaxBlock / (float) sampleRate;
    mLevelDecayDbPerBlock = kDecayDbPerSec * blockSecs;
    mGrDecayDbPerBlock    = kDecayDbPerSec * blockSecs;

    recalcCoefs();
    reset();
}

// ─────────────────────────────────────────────────────────────────────────────
void LimiterDSP::reset()
{
    std::fill (mDelayL.begin(), mDelayL.end(), 0.0f);
    std::fill (mDelayR.begin(), mDelayR.end(), 0.0f);
    mWritePos = 0;
    mEnv = mEnvFast = mEnvSlow = 0.0f;
    mEnvR = mEnvFastR = mEnvSlowR = 0.0f;   // C5
    mMeanSqL = mMeanSqR = 0.0f;
    if (mOversampler) mOversampler->reset();
    mScBuf.clear();
    std::fill (mTpPeaks .begin(), mTpPeaks .end(), 0.0f);
    std::fill (mTpPeaksL.begin(), mTpPeaksL.end(), 0.0f);
    std::fill (mTpPeaksR.begin(), mTpPeaksR.end(), 0.0f);
    mInputDb .store (-96.0f);
    mOutputDb.store (-96.0f);
    mGrDb    .store (0.0f);
    mScHpfL.reset();
    mScHpfR.reset();

    // TS7: the SERVOS deliberately survive reset() -- a graph-wide reset happens
    // for wet-tail hygiene at transport and render boundaries, and dropping a
    // converged loudness servo or ceiling trim there would make the limiter
    // re-learn (and briefly overshoot) every time the user pressed stop.  The
    // meters they read from do get cleared.
    mOutTp.reset();
    if (mLufsPrepared) mOutLufs.resetIntegrated();
    mOutTpDb .store (-144.0f);
    mOutLufsM.store (-120.0f);
    mOutLufsS.store (-120.0f);
    mOutLufsI.store (-120.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// TS7 servos.
//
// THREAD CONTRACT, and it is the reason these are offsets rather than smoother
// targets.  `mInputGainSmooth` / `mCeilingSmooth` are written by the MESSAGE
// thread (the knob setters) and read by the AUDIO thread (getNextValue) -- the
// read/write race every DSP setter in this codebase already has, and which is
// benign.  Having the servos ALSO write them would make it write/write: two
// setTargetValue calls interleaving can leave one ramp with a step computed
// against the other's target.  So each servo owns a plain dB OFFSET that the
// audio thread adds in the sample loop, and the smoothers stay single-writer.
//
// No smoothing is needed on the offsets themselves: the slew limits below cap a
// block's movement at kServoDbPerSec * blockSecs (0.07 dB at 2048/44.1k) and
// kTrimDownDbPerSec * blockSecs (0.14 dB), both far below audibility as a step.

// CL-244: slew-limited closed loop from measured OUTPUT short-term loudness back
// onto input gain.  Runs once per block, never per sample -- a 3 s window cannot
// say anything new inside one block, and per-sample servo work would be pure cost.
void LimiterDSP::updateLoudnessServo (int numSamples)
{
    const float st = mOutLufs.shortTerm();
    // Below the absolute gate there is no programme to measure.  Holding the last
    // value rather than unwinding to 0 is what stops the servo from pumping the
    // gain back up through every gap in the music.
    if (st <= -70.0f) return;

    const float blockSecs = (float) numSamples / (float) juce::jmax (1.0, mSampleRate);
    const float maxStep   = kServoDbPerSec * blockSecs;
    const float err       = mLoudnessTargetLufs - st;

    float servo = mServoDb.load();
    servo += juce::jlimit (-maxStep, maxStep, err);
    mServoDb.store (juce::jlimit (-kServoRangeDb, kServoRangeDb, servo));
}

// BLU-108: pull the effective ceiling down until the measured OUTPUT true peak
// sits under the target, and let it back up slowly.  Asymmetric on purpose --
// engaging late clips, releasing early re-clips.
void LimiterDSP::updateCeilingTrim (int numSamples)
{
    const float tpDb = mOutTpDb.load();
    if (tpDb <= -140.0f) return;   // no output yet

    const float blockSecs = (float) numSamples / (float) juce::jmax (1.0, mSampleRate);
    const float over      = tpDb - mTruePeakTargetDb;

    float trim = mCeilingTrimDb.load();
    if (over > 0.0f)
        trim -= juce::jmin (over, kTrimDownDbPerSec * blockSecs);
    else if (trim < 0.0f)
        // Only recover while there is measurable headroom, so the loop settles
        // just under the target instead of oscillating across it.
        trim += juce::jmin (-trim, juce::jmin (-over, kTrimUpDbPerSec * blockSecs));

    mCeilingTrimDb.store (juce::jlimit (-kTrimMaxDb, 0.0f, trim));
}

// ─────────────────────────────────────────────────────────────────────────────
void LimiterDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;

    juce::ScopedNoDenormals noDenormals;   // A1

    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();
    if (numSamples <= 0 || numCh <= 0) return;
    if (numSamples > mMaxBlock) return;     // host exceeded declared block size - drop frame
    // Defensive: if prepare() was somehow skipped, the oversampler / delay
    // line / scratch bufs are not allocated. Skip the block rather than
    // dereference null state.
    if (mOversampler == nullptr) return;
    if (mDelayL.empty() || mDelayR.empty()) return;
    if ((int) mScBuf.getNumSamples() < numSamples) return;
    if ((int) mTpPeaks.size()        < numSamples) return;

    float* L = buffer.getWritePointer (0);
    float* R = (numCh > 1) ? buffer.getWritePointer (1) : nullptr;

    // -- 1. Input gain -> detector sidechain + delay line -
    // The TP detector reads mScBuf BEFORE the look-ahead delay while the same
    // post-gain samples enter the delay line, so the gain reduction computed from
    // a peak lands on that peak aheadSamples later. Detector source and SAT
    // position are documented in the loop below.
    float* scL = mScBuf.getWritePointer (0);
    float* scR = mScBuf.getWritePointer (1);

    // Capture block-invariant delay-line size once (same reason as readPos below)
    const int writeDelaySize = mDelaySize;
    if (writeDelaySize <= 0) return;

    float inPeakLin = 0.0f;
    const bool scHpfActive = (mSidechainHPF > 21.0f);   // only cost cycles when above floor

    // TS7: the two servo offsets, snapshotted ONCE per block.  Reading them
    // per sample would let a value change mid-block for no benefit -- the servos
    // only move once per block anyway.  Both gated on Maximizer mode: in Limiter
    // mode the knobs are the whole truth.
    const bool  maxOn      = maximizerActive();
    const float servoDb    = (maxOn && mLoudnessTargetOn) ? mServoDb.load()       : 0.0f;
    const float ceilTrimDb = (maxOn && mAutoCeiling)      ? mCeilingTrimDb.load() : 0.0f;

    // C.4 Phase 2 (2026-04-30): external-key detection.  When an SC source
    // is connected to this slot, the TP detector reads SC samples instead
    // of the limiter's own input.  Audio path (input gain + soft sat +
    // delay line + output) is unchanged -- the limiter still ducks ITS
    // input, but the duck timing is driven by the SC source.
    const juce::AudioBuffer<float>* extSc = getActiveSidechain();
    const float* extScL = nullptr;
    const float* extScR = nullptr;
    if (extSc != nullptr && extSc->getNumSamples() >= numSamples && extSc->getNumChannels() > 0)
    {
        extScL = extSc->getReadPointer(0);
        extScR = (extSc->getNumChannels() > 1) ? extSc->getReadPointer(1) : extScL;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        // CL-244's servo rides ON TOP of the knob's smoothed value, never inside
        // it -- see the thread contract above updateLoudnessServo.
        const float gainDb  = mInputGainSmooth.getNextValue() + servoDb;
        const float gainLin = juce::Decibels::decibelsToGain (gainDb);

        const float rawL = L[i] * gainLin;
        const float rawR = (R ? R[i] : rawL) * gainLin;

        // SAT moved post-limiter (FL flow: ... -> Limiter -> Saturation -> out).
        // The detector + limiter run on the clean post-gain signal so the
        // saturation no longer affects the gain reduction (SAT is applied to the
        // limited output in the gain-computer loop below).
        // Detector path: external SC when keyed, otherwise the post-gain input.
        float detL = (extScL != nullptr) ? extScL[i] : rawL;
        float detR = (extScR != nullptr) ? extScR[i] : rawR;
        if (scHpfActive)
        {
            detL = mScHpfL.processSample (0, detL);
            detR = mScHpfR.processSample (0, detR);
        }
        scL[i] = detL;
        scR[i] = detR;

        // Input meter (post-gain - what the limiter sees)
        inPeakLin = juce::jmax (inPeakLin, std::abs (rawL), std::abs (rawR));

        // Write the post-gain (un-saturated) samples to the delay line.
        mDelayL[(size_t) mWritePos] = rawL;
        mDelayR[(size_t) mWritePos] = rawR;
        mWritePos = (mWritePos + 1) % writeDelaySize;
    }

    // ── 2. Upsample detector block, collect per-sample true-peak, downsample ──
    juce::dsp::AudioBlock<float> scBlock (mScBuf.getArrayOfWritePointers(),
                                          2, 0, (size_t) numSamples);
    auto upBlock = mOversampler->processSamplesUp (scBlock);

    const int upN = (int) upBlock.getNumSamples();   // == numSamples * kOsFactor
    jassert (upN == numSamples * kOsFactor);
    // C5: when linked, mTpPeaks[i] = max(|L|,|R|) over this sample's OS group.
    // When unlinked, pack per-channel peaks in two arrays... but to keep
    // downstream loop simple, we reuse mTpPeaks for the linked case and
    // compute per-channel on the fly below in the envelope loop. Here we
    // always write the linked max -- the unlinked path re-derives per-channel
    // peaks from `upBlock` directly below.
    const bool linked = mStereoLink;
    for (int i = 0; i < numSamples; ++i)
    {
        float pMax = 0.0f;
        for (int j = 0; j < kOsFactor; ++j)
        {
            const int k = i * kOsFactor + j;
            if (k >= upN) break;
            pMax = juce::jmax (pMax, std::abs (upBlock.getSample (0, k)));
            pMax = juce::jmax (pMax, std::abs (upBlock.getSample (1, k)));
        }
        mTpPeaks[(size_t) i] = pMax;
    }
    // C5: unlinked mode - pack per-channel peaks so each envelope can run
    // independently below. Uses separate storage (not mScBuf) because the
    // oversampler still needs mScBuf intact for the processSamplesDown pair.
    if (! linked)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float pL = 0.0f, pR = 0.0f;
            for (int j = 0; j < kOsFactor; ++j)
            {
                const int k = i * kOsFactor + j;
                if (k >= upN) break;
                pL = juce::jmax (pL, std::abs (upBlock.getSample (0, k)));
                pR = juce::jmax (pR, std::abs (upBlock.getSample (1, k)));
            }
            mTpPeaksL[(size_t) i] = pL;
            mTpPeaksR[(size_t) i] = pR;
        }
    }
    // Always pair up/down to keep oversampler state coherent. Output is discarded.
    mOversampler->processSamplesDown (scBlock);

    // ── 3. Per-sample envelope + gain reduction + delayed-audio output ────────
    const float attackCoef  = mAttackCoef;
    const float releaseCoef = mReleaseCoef;
    const float relFastCoef = mRelFastCoef;
    const float relSlowCoef = mRelSlowCoef;
    const float relStep     = mRelStepPerSample;
    const float relStepFast = mRelStepFast;
    const float relStepSlow = mRelStepSlow;
    // CL-243: the character leans the curve and sets the blend knee.  Clean's
    // offset is 0 and its knee is 6 dB, so the default is the old behaviour, and
    // Limiter mode resolves to Clean regardless of the stored character.
    const auto& prof        = profileFor (effectiveCharacter());
    const float curve       = juce::jlimit (0.0f, 1.0f, mReleaseCurve + prof.curveOffset);
    const float autoKneeDb  = juce::jmax (0.5f, prof.autoKneeDb);
    const float charSatDrive = prof.satAutoDrive * 5.0f;
    const bool  autoRel     = mAutoRelease;

    // Envelope helper: instantaneous-ish attack + user-curve release.
    auto envStep = [attackCoef, releaseCoef, relStep, curve]
                   (float env, float peak) -> float
    {
        if (peak > env)
            return attackCoef * env + (1.0f - attackCoef) * peak;

        // Release: blend linear and exponential decay toward `peak`.
        const float expRel = releaseCoef * env + (1.0f - releaseCoef) * peak;
        float linRel = env - relStep;
        if (linRel < peak) linRel = peak;       // don't overshoot
        return (1.0f - curve) * linRel + curve * expRel;
    };
    auto envStepFast = [attackCoef, relFastCoef, relStepFast, curve]
                       (float env, float peak) -> float
    {
        if (peak > env)
            return attackCoef * env + (1.0f - attackCoef) * peak;
        const float expRel = relFastCoef * env + (1.0f - relFastCoef) * peak;
        float linRel = env - relStepFast;
        if (linRel < peak) linRel = peak;
        return (1.0f - curve) * linRel + curve * expRel;
    };
    auto envStepSlow = [attackCoef, relSlowCoef, relStepSlow, curve]
                       (float env, float peak) -> float
    {
        if (peak > env)
            return attackCoef * env + (1.0f - attackCoef) * peak;
        const float expRel = relSlowCoef * env + (1.0f - relSlowCoef) * peak;
        float linRel = env - relStepSlow;
        if (linRel < peak) linRel = peak;
        return (1.0f - curve) * linRel + curve * expRel;
    };

    float blockGrDb  = 0.0f;   // max GR across block (negative dB)
    float outPeakLin = 0.0f;

    // Capture block-invariant ahead + delay size once so a knob wiggle on the
    // UI thread cannot change them between samples (int writes aren't atomic
    // on all archs, and even torn reads of mAheadSamples could push readPos
    // out of range). Block the per-sample read against exactly one snapshot.
    const int  aheadSamps = mAheadSamples;
    const int  delaySize  = mDelaySize;
    if (delaySize <= 0) return;   // never processed / torn read guard

    const bool autoMakeupOn = mAutoMakeup;     // C4
    const bool linkOn       = mStereoLink;     // C5
    const bool  susOn   = mSusCoef > 0.0f;     // SUSTAIN RMS-window hold
    const float susCoef = mSusCoef;

    // Gain-computer helper: returns the gain to apply given an envelope value.
    auto computeGain = [] (float env, float ceilingLin) -> float
    {
        return (env > ceilingLin) ? (ceilingLin / std::max (env, 1e-9f)) : 1.0f;
    };

    for (int i = 0; i < numSamples; ++i)
    {
        // Read delayed audio (we already advanced mWritePos; read is aheadSamples back)
        const int readPos = (mWritePos - numSamples + i - aheadSamps + delaySize * 2) % delaySize;
        float outL = mDelayL[(size_t) readPos];
        float outR = mDelayR[(size_t) readPos];

        // Consume the ceiling smoother per sample so 15ms ramps apply here too.
        // BLU-108's trim is added on top for the same single-writer reason.
        const float ceilingDb  = mCeilingSmooth.getNextValue() + ceilTrimDb;
        const float ceilingLin = juce::Decibels::decibelsToGain (ceilingDb);

        // SAT (post-limiter, FL-faithful): smoothers consumed here (not in the
        // input loop) so the saturation colors the limited output and never
        // touches the detector/GR.  drive = 0 when SatThresh == 1 (off).
        const float satTh    = mSatThreshSmooth.getNextValue();
        const float satCv    = mSatCurveSmooth .getNextValue();
        // CL-243: the character's own drive adds to the SAT knob's, so Loud /
        // Warm / Instant carry colour even with the knob at its off position.
        const float satDrive = (1.0f - satTh) * 5.0f + charSatDrive;

        // C4: Auto-makeup = -ceilingDb of post-limit boost so the output stays
        // hot when the user lowers the ceiling. Skipped when off.
        const float makeupLin = autoMakeupOn
            ? juce::Decibels::decibelsToGain (-ceilingDb)
            : 1.0f;

        // C5: Envelope is either shared (linked) or per-channel (unlinked).
        float gainL = 1.0f, gainR = 1.0f;
        if (linkOn)
        {
            float peak = mTpPeaks[(size_t) i];
            if (susOn)   // SUSTAIN: hold via mean-square window (true-peak preserved by the max)
            {
                mMeanSqL = (1.0f - susCoef) * peak * peak + susCoef * mMeanSqL;
                peak = juce::jmax (peak, std::sqrt (mMeanSqL));
            }
            if (autoRel)
            {
                mEnvFast = envStepFast (mEnvFast, peak);
                mEnvSlow = envStepSlow (mEnvSlow, peak);
                const float env     = juce::jmax (mEnvFast, mEnvSlow);
                const float grLin   = computeGain (env, ceilingLin);
                const float grDbNow = juce::Decibels::gainToDecibels (grLin, -96.0f);
                const float blendSlow = juce::jlimit (0.0f, 1.0f, -grDbNow / autoKneeDb);
                const float blended = (1.0f - blendSlow) * mEnvFast + blendSlow * mEnvSlow;
                gainL = computeGain (blended, ceilingLin);
            }
            else
            {
                mEnv = envStep (mEnv, peak);
                gainL = computeGain (mEnv, ceilingLin);
            }
            gainR = gainL;   // single envelope drives both
        }
        else
        {
            // Per-channel envelope + gain reduction.
            float peakL = mTpPeaksL[(size_t) i];
            float peakR = mTpPeaksR[(size_t) i];
            if (susOn)   // SUSTAIN: per-channel mean-square hold
            {
                mMeanSqL = (1.0f - susCoef) * peakL * peakL + susCoef * mMeanSqL;
                peakL = juce::jmax (peakL, std::sqrt (mMeanSqL));
                mMeanSqR = (1.0f - susCoef) * peakR * peakR + susCoef * mMeanSqR;
                peakR = juce::jmax (peakR, std::sqrt (mMeanSqR));
            }
            if (autoRel)
            {
                mEnvFast  = envStepFast (mEnvFast,  peakL);
                mEnvSlow  = envStepSlow (mEnvSlow,  peakL);
                mEnvFastR = envStepFast (mEnvFastR, peakR);
                mEnvSlowR = envStepSlow (mEnvSlowR, peakR);
                auto blendOne = [&] (float fast, float slow) -> float
                {
                    const float e     = juce::jmax (fast, slow);
                    const float grLin = computeGain (e, ceilingLin);
                    const float db    = juce::Decibels::gainToDecibels (grLin, -96.0f);
                    const float bs    = juce::jlimit (0.0f, 1.0f, -db / autoKneeDb);
                    const float b     = (1.0f - bs) * fast + bs * slow;
                    return computeGain (b, ceilingLin);
                };
                gainL = blendOne (mEnvFast,  mEnvSlow);
                gainR = blendOne (mEnvFastR, mEnvSlowR);
            }
            else
            {
                mEnv  = envStep (mEnv,  peakL);
                mEnvR = envStep (mEnvR, peakR);
                gainL = computeGain (mEnv,  ceilingLin);
                gainR = computeGain (mEnvR, ceilingLin);
            }
        }

        // Apply gain reduction.  Makeup is deliberately NOT folded in here --
        // see the block below for why it moved.
        outL *= gainL;
        outR *= gainR;

        // SAT: post-limiter saturation (FL final stage) -- colors the limited
        // output.  Skipped when off (SatThresh == 1 -> drive 0).  Position
        // unchanged, so its input is the same signal it always saw.
        if (satDrive > 0.0f)
        {
            outL = softSat (outL, satDrive, satCv);
            outR = softSat (outR, satDrive, satCv);
        }

        // Hard ceiling clamp -- the limiter's actual guarantee, taken BEFORE
        // makeup.
        outL = juce::jlimit (-ceilingLin, ceilingLin, outL);
        outR = juce::jlimit (-ceilingLin, ceilingLin, outR);

        // C4 auto-makeup, now applied AFTER the ceiling clamp (TS7 fix).
        //
        // IT USED TO RUN BEFORE THE CLAMP, WHICH CANCELLED IT EXACTLY.  The
        // limiter brought the peak to ceilingLin, makeup (= 1/ceilingLin) lifted
        // it to ~1.0, and the clamp cut it straight back to ceilingLin: zero
        // level gain delivered, and every sample between ceilingLin/2 and
        // ceilingLin pushed over the ceiling and hard-clipped.  So the control
        // was not a no-op, it was a clipper whose tooltip promised the opposite.
        // BLU-108's ceiling trim made it worse by lowering ceilingDb further.
        //
        // The output ceiling SCALES with makeup rather than being pinned at
        // unity, which is what keeps the makeup-off path bit-identical: with
        // makeupLin == 1 this reduces to the same clamp as the line above,
        // including the +12 dB "headroom / no limiting" ceiling setting where a
        // hard unity clamp would have newly clipped.  With makeup on,
        // ceilingLin * makeupLin == 1 by construction, so the signal reaches
        // full scale as the control claims.
        if (autoMakeupOn)
        {
            const float outCeil = ceilingLin * makeupLin;
            outL = juce::jlimit (-outCeil, outCeil, outL * makeupLin);
            outR = juce::jlimit (-outCeil, outCeil, outR * makeupLin);
        }

        L[i] = outL;
        if (R) R[i] = outR;

        // Meter accumulation (track deepest GR across block, most negative).
        const float grDbSample = juce::Decibels::gainToDecibels (juce::jmin (gainL, gainR), -96.0f);
        if (grDbSample < blockGrDb) blockGrDb = grDbSample;
        outPeakLin = juce::jmax (outPeakLin, std::abs (outL), std::abs (outR));
    }

    // ── 4. Atomic meter updates (once per block, with hold+decay) ─────────────
    // A2/A3: same pattern as InsertNode / Compressor -- max(thisBlock, prev - decay)
    // for level meters (fall toward -96), min(thisBlock, prev + decay) for GR
    // (GR is <=0; rise toward 0).
    const float inDbNow  = juce::Decibels::gainToDecibels (inPeakLin,  -96.0f);
    const float outDbNow = juce::Decibels::gainToDecibels (outPeakLin, -96.0f);
    {
        const float prev = mInputDb.load (std::memory_order_relaxed);
        const float decayed = juce::jmax (-96.0f, prev - mLevelDecayDbPerBlock);
        mInputDb.store (juce::jmax (inDbNow, decayed), std::memory_order_relaxed);
    }
    {
        const float prev = mOutputDb.load (std::memory_order_relaxed);
        const float decayed = juce::jmax (-96.0f, prev - mLevelDecayDbPerBlock);
        mOutputDb.store (juce::jmax (outDbNow, decayed), std::memory_order_relaxed);
    }
    {
        const float prev = mGrDb.load (std::memory_order_relaxed);
        // GR is <=0 (0 = no reduction, -10 = 10dB cut). Decay toward 0.
        const float decayed = juce::jmin (0.0f, prev + mGrDecayDbPerBlock);
        mGrDb.store (juce::jmin (blockGrDb, decayed), std::memory_order_relaxed);
    }

    // QA-Layout T13: one column per block into the visual feed.  Free when no
    // Visual window is open -- push() returns on a single relaxed load.
    //
    // Everything is normalised HERE rather than in the painter, because the
    // painter has no business knowing the limiter's dB range and a second
    // effect using the same drawing helper would otherwise have to agree with
    // it by coincidence.  Mapping over a 60 dB window: -60 dBFS at the centre
    // line, 0 dBFS at full deflection.
    {
        constexpr float kRangeDb = 60.0f;

        const auto norm = [] (float db, float range)
        {
            return juce::jlimit (0.0f, 1.0f, (db + range) / range);
        };

        // Envelope is symmetric about the centre: this is a level display, not
        // a waveform, so both halves carry the same magnitude.
        const float lvl = norm (juce::jmax (-kRangeDb, outDbNow), kRangeDb);

        // a = gain reduction as a 0..1 depth FROM THE TOP of the strip, which
        // is how every limiter draws it -- the curve hangs down from the
        // ceiling by however much is being taken off.
        const float gr  = juce::jlimit (0.0f, 1.0f,
                                        -mGrDb.load (std::memory_order_relaxed) / 20.0f);

        // b = the ceiling line, in the same 0..1 top-down space, so the GR
        // curve and the line it is measured against share one coordinate system.
        const float ceilNorm = 1.0f - norm (mCeilingTargetDb + mCeilingTrimDb.load (std::memory_order_relaxed),
                                            kRangeDb);

        mVisualFeed.push (-lvl, lvl, gr, ceilNorm);
    }

    // ── 5. TS7: output loudness + true peak, then the two servos ──────────────
    // Both meters read the FINAL output -- after gain reduction, saturation and
    // the hard clamp -- because that is the signal the target is about.  Each is
    // gated on its consumer so an idle limiter pays for neither.
    // BLU-110 watchdog: one decrement per block, and the meter lapses when the
    // panel stops poking it.  Target mode holds it up independently.
    {
        const int hold = mLufsHold.load();
        if (hold > 0) mLufsHold.store (hold - 1);
    }
    // Maximizer-only: Limiter mode has no loudness readout and no servo, so it
    // must not pay the K-weighting cost either.
    const bool wantLufs = maxOn && (mLoudnessTargetOn || mLufsHold.load() > 0);
    if (mLufsPrepared && wantLufs)
    {
        // Message-thread reset request (target mode switched on), honoured
        // HERE where the meter's audio-thread contract holds.
        if (mLufsResetPending.exchange (false, std::memory_order_acq_rel))
            mOutLufs.resetIntegrated();

        mOutLufs.process (buffer);
        mOutLufsM.store (mOutLufs.momentary());
        mOutLufsS.store (mOutLufs.shortTerm());
        mOutLufsI.store (mOutLufs.integrated());
    }
    else if (! wantLufs)
    {
        // Publish the floor once the meter lapses, so a stale reading cannot sit
        // frozen on a panel that is watching again a moment later.
        mOutLufsM.store (-120.0f);
        mOutLufsS.store (-120.0f);
        mOutLufsI.store (-120.0f);
    }

    if (maxOn && mAutoCeiling)
    {
        // resetPeak, not reset: filter history carries across the block boundary,
        // so there is no seam artefact in the measurement.
        mOutTp.resetPeak();
        mOutTp.process (buffer);
        mOutTpDb.store (mOutTp.truePeakDb());
        updateCeilingTrim (numSamples);
    }

    if (maxOn && mLoudnessTargetOn)
        updateLoudnessServo (numSamples);
}

// ─────────────────────────────────────────────────────────────────────────────
// Setters - CPU-guarded per CLAUDE.md rule
// ─────────────────────────────────────────────────────────────────────────────
void LimiterDSP::setInputGainDb (float dB)
{
    const float n = juce::jlimit (-12.0f, 24.0f, dB);
    if (n != mInputGainTargetDb)
    {
        mInputGainTargetDb = n;
        mInputGainSmooth.setTargetValue (n);
    }
}

void LimiterDSP::setCeilingDb (float dB)
{
    const float n = juce::jlimit (-24.0f, 12.0f, dB);
    if (n != mCeilingTargetDb)
    {
        mCeilingTargetDb = n;
        mCeilingSmooth.setTargetValue (n);
    }
}

// ── TS7 setters ──────────────────────────────────────────────────────────────
void LimiterDSP::setCharacter (Character c)
{
    const auto n = (Character) juce::jlimit (0, (int) Character::Count - 1, (int) c);
    if (n != mCharacter) { mCharacter = n; recalcCoefs(); }
}

void LimiterDSP::setCharacterIndex (int index)
{
    setCharacter ((Character) index);
}

void LimiterDSP::setLoudnessTargetOn (bool on)
{
    if (on == mLoudnessTargetOn) return;
    mLoudnessTargetOn = on;
    if (! on)
    {
        // Turning the mode off must give the knob back: an invisible servo trim
        // left on the input gain would make the limiter louder than the panel says.
        mServoDb.store (0.0f);
    }
    else
    {
        // Start the loop from a clean programme measurement rather than whatever
        // the display meter happened to be showing.  DEFERRED to process():
        // this setter runs on the message thread and LufsMeterDSP's contract is
        // audio-thread-only -- resetting here raced a block mid-measurement.
        mLufsResetPending.store (true, std::memory_order_release);
    }
}

void LimiterDSP::setLoudnessTargetLufs (float lufs)
{
    const float n = juce::jlimit (-30.0f, 0.0f, lufs);
    if (n != mLoudnessTargetLufs) mLoudnessTargetLufs = n;
}

void LimiterDSP::setAutoCeiling (bool on)
{
    if (on == mAutoCeiling) return;
    mAutoCeiling = on;
    if (! on)
    {
        // Same contract as the loudness servo: the ceiling knob is authoritative
        // again the moment the automatic half is switched off.
        mCeilingTrimDb.store (0.0f);
        mOutTpDb.store (-144.0f);
    }
    else
    {
        mOutTp.reset();
    }
}

void LimiterDSP::setTruePeakTargetDb (float dbTp)
{
    const float n = juce::jlimit (-6.0f, 0.0f, dbTp);
    if (n != mTruePeakTargetDb) mTruePeakTargetDb = n;
}

void LimiterDSP::pokeLufsMeter() noexcept
{
    mLufsHold.store (mLufsHoldBlocks);
}

void LimiterDSP::setSatThresh (float lin)
{
    const float n = juce::jlimit (0.0f, 1.0f, lin);
    if (n != mSatThreshTarget)
    {
        mSatThreshTarget = n;
        mSatThreshSmooth.setTargetValue (n);
    }
}

void LimiterDSP::setSatCurve (float v01)
{
    const float n = juce::jlimit (0.0f, 1.0f, v01);
    if (n != mSatCurve)
    {
        mSatCurve = n;
        mSatCurveSmooth.setTargetValue (n);   // C1
    }
}

void LimiterDSP::setSidechainHPF (float hz)
{
    const float n = juce::jlimit (20.0f, 2000.0f, hz);
    if (n != mSidechainHPF)
    {
        mSidechainHPF = n;
        updateScHpfCoefs();
    }
}

void LimiterDSP::setAutoMakeup (bool on)
{
    if (on != mAutoMakeup) mAutoMakeup = on;
}

void LimiterDSP::setStereoLink (bool on)
{
    if (on != mStereoLink) mStereoLink = on;
}

void LimiterDSP::setAttackMs (float ms)
{
    const float n = juce::jlimit (0.1f, 20.0f, ms);
    if (n != mAttackMs) { mAttackMs = n; recalcCoefs(); }
}

void LimiterDSP::setReleaseMs (float ms)
{
    const float n = juce::jlimit (10.0f, 1000.0f, ms);
    if (n != mReleaseMs) { mReleaseMs = n; recalcCoefs(); }
}

void LimiterDSP::setAheadMs (float ms)
{
    const float n = juce::jlimit (0.0f, kMaxAheadMs, ms);
    if (n != mAheadMs)
    {
        mAheadMs = n;
        if (mSampleRate > 0.0)
        {
            mAheadSamples = juce::jlimit (0, juce::jmax (0, mDelaySize - 1),
                                          (int) std::round ((double) mAheadMs * 0.001 * mSampleRate));
            mLatencySamples = mAheadSamples + mOsLatencySamples;
        }
    }
}

void LimiterDSP::setReleaseCurve (float v01)
{
    const float n = juce::jlimit (0.0f, 1.0f, v01);
    if (n != mReleaseCurve) mReleaseCurve = n;
}

void LimiterDSP::setSustainMs (float ms)
{
    const float n = juce::jlimit (0.0f, 1000.0f, ms);
    if (n != mSustainMs) { mSustainMs = n; recalcCoefs(); }
}

void LimiterDSP::setAutoRelease (bool on)
{
    if (on != mAutoRelease) mAutoRelease = on;
}

// ─────────────────────────────────────────────────────────────────────────────
// Serialisation
// ─────────────────────────────────────────────────────────────────────────────
void LimiterDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("LimiterDSP");
    state.setProperty ("inputGainDb",  mInputGainTargetDb, nullptr);
    state.setProperty ("ceilingDb",    mCeilingTargetDb,   nullptr);
    state.setProperty ("satThresh",    mSatThreshTarget,   nullptr);
    state.setProperty ("satCurve",     mSatCurve,          nullptr);
    state.setProperty ("attackMs",     mAttackMs,          nullptr);
    state.setProperty ("releaseMs",    mReleaseMs,         nullptr);
    state.setProperty ("aheadMs",      mAheadMs,           nullptr);
    state.setProperty ("releaseCurve", mReleaseCurve,      nullptr);
    state.setProperty ("sustainMs",    mSustainMs,         nullptr);
    state.setProperty ("autoRelease",  (int) mAutoRelease, nullptr);
    state.setProperty ("sidechainHPF", mSidechainHPF,      nullptr);   // C2
    state.setProperty ("autoMakeup",   (int) mAutoMakeup,  nullptr);   // C4
    state.setProperty ("stereoLink",   (int) mStereoLink,  nullptr);   // C5
    // TS7.  Absent in a pre-TS7 preset, and every default below reproduces the
    // old behaviour: character 0 is the old fixed constants, both automatic modes
    // are off.  So no migration is needed and none is written.
    state.setProperty ("mode",            (int) mMode,             nullptr);   // Limiter / Maximizer
    state.setProperty ("character",       (int) mCharacter,        nullptr);   // CL-243
    state.setProperty ("loudTargetOn",    (int) mLoudnessTargetOn, nullptr);   // CL-244
    state.setProperty ("loudTargetLufs",  mLoudnessTargetLufs,     nullptr);
    state.setProperty ("autoCeiling",     (int) mAutoCeiling,      nullptr);   // BLU-108
    state.setProperty ("truePeakTargetDb", mTruePeakTargetDb,      nullptr);
    state.setProperty ("bypassed",     (int) bypassed,     nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml) juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void LimiterDSP::setStateInformation (const void* data, int sz)
{
    std::unique_ptr<juce::XmlElement> xml (SafeXml::parseBinaryBlob (data, sz));
    if (! xml) return;
    if (! xml->hasTagName ("LimiterDSP")) return;

    juce::ValueTree state = juce::ValueTree::fromXml (*xml);

    mInputGainTargetDb = state.getProperty ("inputGainDb",  mInputGainTargetDb);
    mCeilingTargetDb   = state.getProperty ("ceilingDb",    mCeilingTargetDb);
    mSatThreshTarget   = state.getProperty ("satThresh",    mSatThreshTarget);
    mSatCurve          = state.getProperty ("satCurve",     mSatCurve);
    mAttackMs          = state.getProperty ("attackMs",     mAttackMs);
    mReleaseMs         = state.getProperty ("releaseMs",    mReleaseMs);
    mAheadMs           = state.getProperty ("aheadMs",      mAheadMs);
    mReleaseCurve      = state.getProperty ("releaseCurve", mReleaseCurve);
    mSustainMs         = (float)(double) state.getProperty ("sustainMs", 0.0);   // default off (old presets)
    mAutoRelease       = ((int) state.getProperty ("autoRelease", 0)) != 0;
    mSidechainHPF      = (float)(double) state.getProperty ("sidechainHPF", 20.0);   // C2
    mAutoMakeup        = ((int) state.getProperty ("autoMakeup", 0)) != 0;           // C4
    mStereoLink        = ((int) state.getProperty ("stereoLink", 1)) != 0;           // C5 (default link on)

    // TS7: defaults reproduce the pre-TS7 behaviour exactly for old presets --
    // mode 0 is Limiter, the FL reproduction, with the maximizer half gated off.
    mMode               = (((int) state.getProperty ("mode", 0)) == 1)
                            ? Mode::Maximizer : Mode::Limiter;
    mCharacter          = (Character) juce::jlimit (0, (int) Character::Count - 1,
                                                    (int) state.getProperty ("character", 0));
    mLoudnessTargetOn   = ((int) state.getProperty ("loudTargetOn", 0)) != 0;
    mLoudnessTargetLufs = (float)(double) state.getProperty ("loudTargetLufs", -14.0);
    mAutoCeiling        = ((int) state.getProperty ("autoCeiling", 0)) != 0;
    mTruePeakTargetDb   = (float)(double) state.getProperty ("truePeakTargetDb", -1.0);
    // A loaded preset must not inherit the previous slot's converged servo state.
    mServoDb      .store (0.0f);
    mCeilingTrimDb.store (0.0f);

    bypassed           = ((int) state.getProperty ("bypassed",    0)) != 0;

    // Snap smoothed values + refresh coefs/delay
    mInputGainSmooth.setCurrentAndTargetValue (mInputGainTargetDb);
    mCeilingSmooth  .setCurrentAndTargetValue (mCeilingTargetDb);
    mSatThreshSmooth.setCurrentAndTargetValue (mSatThreshTarget);
    mSatCurveSmooth .setCurrentAndTargetValue (mSatCurve);   // C1
    updateScHpfCoefs();

    if (mSampleRate > 0.0)
    {
        recalcCoefs();
        // Recompute mAheadSamples from restored mAheadMs without reallocating
        mAheadSamples = juce::jlimit (0, juce::jmax (0, mDelaySize - 1),
                                      (int) std::round ((double) mAheadMs * 0.001 * mSampleRate));
        mLatencySamples = mAheadSamples + mOsLatencySamples;
        // Clear delay / envelope / oversampler state so preset load doesn't pop
        std::fill (mDelayL.begin(), mDelayL.end(), 0.0f);
        std::fill (mDelayR.begin(), mDelayR.end(), 0.0f);
        mWritePos = 0;
        mEnv = mEnvFast = mEnvSlow = 0.0f;
        mEnvR = mEnvFastR = mEnvSlowR = 0.0f;
        mMeanSqL = mMeanSqR = 0.0f;
        if (mOversampler) mOversampler->reset();
        std::fill (mTpPeaks .begin(), mTpPeaks .end(), 0.0f);
        std::fill (mTpPeaksL.begin(), mTpPeaksL.end(), 0.0f);
        std::fill (mTpPeaksR.begin(), mTpPeaksR.end(), 0.0f);
        mScHpfL.reset();
        mScHpfR.reset();
        // TS7: the meters restart with the preset, and the smoothers are re-snapped
        // above with the (now zeroed) trims already folded in.
        mOutTp.reset();
        if (mLufsPrepared) mOutLufs.resetIntegrated();
        mOutTpDb .store (-144.0f);
        mOutLufsM.store (-120.0f);
        mOutLufsS.store (-120.0f);
        mOutLufsI.store (-120.0f);
    }
}
