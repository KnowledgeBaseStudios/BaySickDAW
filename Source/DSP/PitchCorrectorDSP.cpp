#include "PitchCorrectorDSP.h"
#include <cmath>
#include <algorithm>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
// PitchCorrectorDSP - Phase H-5 (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    // Scale interval masks (semitones from root).  true = note in scale.
    // QA-Fd 17b: ordered to match PitchCorrectorDSP::Scale = the piano
    // roll's kScaleDefs table (PianoRoll.cpp) -- masks copied verbatim so
    // the two surfaces can never disagree on membership.
    constexpr std::array<std::array<bool, 12>, 13> kScaleMasks = {{
        {{true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true }}, // Chromatic
        {{true,  false, true,  false, true,  true,  false, true,  false, true,  false, true }}, // Major
        {{true,  false, true,  true,  false, true,  false, true,  true,  false, true,  false}}, // Minor (natural)
        {{true,  false, true,  true,  false, true,  false, true,  false, true,  true,  false}}, // Dorian
        {{true,  true,  false, true,  false, true,  false, true,  true,  false, true,  false}}, // Phrygian
        {{true,  false, true,  false, true,  false, true,  true,  false, true,  false, true }}, // Lydian
        {{true,  false, true,  false, true,  true,  false, true,  false, true,  true,  false}}, // Mixolydian
        {{true,  true,  false, true,  false, true,  true,  false, true,  false, true,  false}}, // Locrian
        {{true,  false, true,  true,  false, true,  false, true,  true,  false, false, true }}, // Harm. Minor
        {{true,  false, true,  true,  false, true,  false, true,  false, true,  false, true }}, // Mel. Minor
        {{true,  false, true,  false, true,  false, false, true,  false, true,  false, false}}, // Pentatonic Maj
        {{true,  false, false, true,  false, true,  false, true,  false, false, true,  false}}, // Pentatonic Min
        {{true,  false, false, true,  false, true,  true,  true,  false, false, true,  false}}, // Blues
    }};

    constexpr const char* kScaleNames[13] = {
        "Chromatic", "Major", "Minor", "Dorian", "Phrygian", "Lydian",
        "Mixolydian", "Locrian", "Harm. Minor", "Mel. Minor",
        "Pentatonic Maj", "Pentatonic Min", "Blues"
    };
}

// ─── QA-Fd shared scale table (single source for both vocal editors) ─────────
const char* PitchCorrectorDSP::scaleName (int idx)
{
    return kScaleNames[(size_t) juce::jlimit (0, kNumScales - 1, idx)];
}

const std::array<bool, 12>& PitchCorrectorDSP::scaleMask (int idx)
{
    return kScaleMasks[(size_t) juce::jlimit (0, kNumScales - 1, idx)];
}

float PitchCorrectorDSP::snapMidiToScaleStatic (float midiFloat, int rootPc, int scaleIdx)
{
    if (scaleIdx <= 0)
        return std::round (midiFloat);
    const auto& mask = scaleMask (scaleIdx);
    const int midiInt = (int) std::round (midiFloat);
    // Walk outward from the rounded note for the nearest in-scale pitch
    // class (tie -> down), then keep the walk's octave arithmetic exact by
    // stepping in absolute MIDI.
    for (int step = 0; step < 12; ++step)
    {
        for (int dir = -1; dir <= 1; dir += 2)
        {
            const int cand = midiInt + dir * step;
            int rel = ((cand - rootPc) % 12 + 12) % 12;
            if (mask[(size_t) rel])
                return (float) cand;
            if (step == 0) break;   // step 0: one candidate
        }
    }
    return (float) midiInt;
}

// ─────────────────────────────────────────────────────────────────────────────
// PitchCorrectorDSP
// ─────────────────────────────────────────────────────────────────────────────
// QA-F Task 5: the H-5 2-grain granular Shifter is deleted -- the live path
// now runs PsolaShifter + CepstralFormantEngine from PitchShifters.h.

PitchCorrectorDSP::PitchCorrectorDSP() {}
PitchCorrectorDSP::~PitchCorrectorDSP() { releaseResources(); }

void PitchCorrectorDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
    mTracker.prepare (mSampleRate);
    for (auto& sh : mShifters) sh.prepare (mSampleRate, maxBlockSize);
    for (auto& fe : mFormant)  fe.prepare (mSampleRate, maxBlockSize);
    // QA-Fd 11/16a: ~40 ms engage/disengage crossfade (calibration tuned at
    // the boundary listen).
    mEngageFadeStep = (float) (1.0 / (0.04 * mSampleRate));
    recalcRetuneCoef();
    reset();
}

void PitchCorrectorDSP::releaseResources()
{
    mTracker.releaseResources();
}

void PitchCorrectorDSP::reset()
{
    mTracker.reset();
    for (auto& sh : mShifters) sh.reset();
    for (auto& fe : mFormant)  fe.reset();
    mFormantEngaged    = false;
    mCurrentShiftRatio = 1.0f;
    mHumanizePhase     = 0.0f;
    mCurrentTargetMidi = -1.0f;
    mEngageFade        = 0.0f;
    mDetectedHz       .store (0.0f, std::memory_order_release);
    mTargetHz         .store (0.0f, std::memory_order_release);
    mCurrentShiftCents.store (0.0f, std::memory_order_release);
}

// ─────────────────────────────────────────────────────────────────────────────
// Setters
// ─────────────────────────────────────────────────────────────────────────────
void PitchCorrectorDSP::setKey (int midiPc)
{
    const int v = juce::jlimit (0, 11, midiPc);
    if (v != mKey) mKey = v;
}

void PitchCorrectorDSP::setScale (int s)
{
    const Scale ns = static_cast<Scale> (juce::jlimit (0, kNumScales - 1, s));
    if (ns != mScale) mScale = ns;
}

void PitchCorrectorDSP::setRetuneSpeedMs (float ms)
{
    const float v = juce::jlimit (0.0f, 100.0f, ms);
    if (v != mRetuneSpeedMs) { mRetuneSpeedMs = v; recalcRetuneCoef(); }
}

void PitchCorrectorDSP::setStrength (float unit)
{
    const float v = juce::jlimit (0.0f, 1.0f, unit);
    if (v != mStrength) mStrength = v;
}

void PitchCorrectorDSP::setFormantPreserve (bool on)   { mFormantPreserve = on; }
void PitchCorrectorDSP::setHumanizeCents   (float c)   { mHumanizeCents   = juce::jlimit (0.0f, 20.0f, c); }
void PitchCorrectorDSP::setThroatShiftSemis (float s)  { mThroatSemis     = juce::jlimit (-12.0f, 12.0f, s); }

void PitchCorrectorDSP::recalcRetuneCoef()
{
    // RetuneSpeed = 0 ms -> coef = 0 (snap instantly to target each sample).
    // RetuneSpeed > 0   -> exp one-pole coefficient.
    if (mRetuneSpeedMs <= 0.001f) { mRetuneCoef = 0.0f; return; }
    const double sr = (mSampleRate > 0.0) ? mSampleRate : 44100.0;
    mRetuneCoef = (float) std::exp (-1.0 / (mRetuneSpeedMs * 0.001 * sr));
}

// ─────────────────────────────────────────────────────────────────────────────
// Scale snapping
// ─────────────────────────────────────────────────────────────────────────────
// QA-Fd: delegates to the shared absolute-walk static.  The old pc-space
// walk had an octave-wrap defect (a B snapping "up" to C landed on the C an
// octave DOWN -- octave + snappedPc arithmetic ignores the wrap), audible as
// a huge corrective jump near the root boundary.
float PitchCorrectorDSP::snapMidiToScale (float midiFloat) const noexcept
{
    return snapMidiToScaleStatic (midiFloat, mKey, (int) mScale);
}

// ─────────────────────────────────────────────────────────────────────────────
// RNG (xorshift32 - audio-thread-safe, deterministic seed)
// ─────────────────────────────────────────────────────────────────────────────
float PitchCorrectorDSP::nextRandPm1() noexcept
{
    juce::uint32 x = mRngState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    mRngState = x;
    return ((float) x / (float) 0xFFFFFFFFu) * 2.0f - 1.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// process()
// ─────────────────────────────────────────────────────────────────────────────
void PitchCorrectorDSP::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();
    if (numSamples <= 0 || numCh <= 0) return;

    float* L = buffer.getWritePointer (0);
    float* R = (numCh > 1) ? buffer.getWritePointer (1) : nullptr;

    // Always feed the tracker so UI / future stages see live pitch.  Stereo
    // input is averaged inside pushAudio().
    if (R != nullptr) mTracker.pushAudio (L, R, numSamples);
    else              mTracker.pushAudio (L, numSamples);

    // QA-Fd 11/16a engage-tick fix: fully-bypassed fast path keeps the
    // shifter rings warm (copy only -- no synthesis, no added latency) and
    // tracks the period, so an engage starts on real current history.
    const bool wantOn = ! bypassed;
    if (! wantOn && mEngageFade <= 0.0f)
    {
        const float idleHz = mTracker.getFrequencyHz();
        if (idleHz > 0.0f)
        {
            const float period = (float) (mSampleRate / (double) idleHz);
            for (auto& sh : mShifters) sh.setPeriodSamples (period);
        }
        for (int i = 0; i < numSamples; ++i)
        {
            mShifters[0].feedSample (L[i]);
            if (R != nullptr) mShifters[1].feedSample (R[i]);
        }
        mCurrentTargetMidi = -1.0f;
        mCurrentShiftRatio = 1.0f;
        return;
    }

    if (wantOn && mEngageFade <= 0.0f)
    {
        // Engage edge: synthesis anchors re-sync onto the warm ring; the
        // per-sample crossfade below masks the grain spin-up.  The formant
        // engines reset too -- if formant mode stayed logically engaged
        // across a bypass, their rings hold stale audio that would blend
        // into the fade-in (review NIT).
        for (auto& sh : mShifters) sh.resyncToWriteHead();
        for (auto& fe : mFormant)  fe.reset();
    }

    // Read the latest pitch reading once per block.  Tracker publishes ~50 ms
    // behind the audio so we don't get sample-accurate updates anyway.
    const float detHz       = mTracker.getFrequencyHz();
    const float confidence  = mTracker.getConfidence();

    // No detected pitch -> hold last shift, fade ratio toward 1.0 over RetuneSpeed
    // so silence / unpitched frames don't get correction artifacts.
    float targetRatio = 1.0f;
    float targetHzPub = 0.0f;

    if (detHz > 0.0f && confidence > 0.4f)
    {
        const float detMidi = hzToMidi (detHz);

        // QA-F Task 5 note-change hysteresis: hold the current target note
        // until the sung pitch commits to a different one.  0.65 st sits
        // past the semitone midpoint, so vibrato riding a note boundary no
        // longer flip-flops the target every tracker frame (the "warble" /
        // robotic artifact at default settings).
        if (mCurrentTargetMidi < 0.0f
            || std::abs (detMidi - mCurrentTargetMidi) > 0.65f)
            mCurrentTargetMidi = snapMidiToScale (detMidi);

        const float targetHz = midiToHz (mCurrentTargetMidi);

        // Strength blends between dry pitch (1.0 ratio) and full snap.
        const float fullRatio  = targetHz / juce::jmax (detHz, 1.0f);
        targetRatio            = 1.0f + (fullRatio - 1.0f) * mStrength;

        // Humanize: small slow random walk in cents, additive on top of ratio.
        if (mHumanizeCents > 0.001f)
        {
            // Update walk every block; multiply random cents into ratio.
            mHumanizePhase += nextRandPm1() * mHumanizeCents * 0.05f;
            mHumanizePhase = juce::jlimit (-mHumanizeCents,
                                            mHumanizeCents,
                                            mHumanizePhase);
            // cents -> ratio: 2^(cents/1200)
            const float wobbleRatio = std::pow (2.0f, mHumanizePhase / 1200.0f);
            targetRatio *= wobbleRatio;
        }

        targetHzPub = targetHz;

        // Feed the PSOLA epoch grid from the tracker (period in samples).
        const float period = (float) (mSampleRate / (double) detHz);
        for (auto& sh : mShifters)
            sh.setPeriodSamples (period);
    }
    else
    {
        // Unvoiced: release the held target so the next phrase re-snaps
        // fresh; the shifters keep their last period (PSOLA at ratio->1 is
        // a near-identity at any period).
        mCurrentTargetMidi = -1.0f;
    }

    // Formant machinery engages only while audible work exists; the engage
    // edge resets the engines (their latency appears with the mode).
    const bool wantFormant = mFormantPreserve || std::abs (mThroatSemis) > 0.01f;
    if (wantFormant != mFormantEngaged)
    {
        mFormantEngaged = wantFormant;
        for (auto& fe : mFormant) fe.reset();
    }

    // Per-sample one-pole smoothing of the active shift ratio toward target.
    // mRetuneCoef = 0 at speed=0 (snap), -> ~1 at long speed (slow tracking).
    const float retCoef   = mRetuneCoef;
    const float fadeTgt   = wantOn ? 1.0f : 0.0f;
    constexpr float halfPi = juce::MathConstants<float>::halfPi;

    for (int i = 0; i < numSamples; ++i)
    {
        mCurrentShiftRatio = retCoef * mCurrentShiftRatio
                           + (1.0f - retCoef) * targetRatio;

        // QA-Fd 11/16a: equal-power engage/disengage crossfade.
        mEngageFade = (fadeTgt > mEngageFade)
            ? juce::jmin (1.0f, mEngageFade + mEngageFadeStep)
            : juce::jmax (0.0f, mEngageFade - mEngageFadeStep);
        const float gW = std::sin (mEngageFade * halfPi);
        const float gD = std::cos (mEngageFade * halfPi);

        const float dryL = L[i];
        float wetL = mShifters[0].processSample (dryL, mCurrentShiftRatio);
        if (mFormantEngaged)
            wetL = mFormant[0].processSample (dryL, wetL, mFormantPreserve, mThroatSemis);
        L[i] = dryL * gD + wetL * gW;

        if (R != nullptr)
        {
            const float dryR = R[i];
            float wetR = mShifters[1].processSample (dryR, mCurrentShiftRatio);
            if (mFormantEngaged)
                wetR = mFormant[1].processSample (dryR, wetR, mFormantPreserve, mThroatSemis);
            R[i] = dryR * gD + wetR * gW;
        }
    }

    // Publish UI feedback once per block.
    mDetectedHz       .store (detHz,       std::memory_order_release);
    mTargetHz         .store (targetHzPub, std::memory_order_release);
    mCurrentShiftCents.store (1200.0f * std::log2 (juce::jmax (mCurrentShiftRatio, 0.001f)),
                               std::memory_order_release);
}
