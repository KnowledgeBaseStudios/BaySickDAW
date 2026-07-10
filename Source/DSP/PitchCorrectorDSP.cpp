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
    // Ordered to match PitchCorrectorDSP::Scale enum.
    constexpr std::array<std::array<bool, 12>, 9> kScaleMasks = {{
        {{true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true }}, // Chromatic
        {{true,  false, true,  false, true,  true,  false, true,  false, true,  false, true }}, // Major
        {{true,  false, true,  true,  false, true,  false, true,  true,  false, true,  false}}, // Minor (natural)
        {{true,  false, true,  true,  false, true,  false, true,  true,  false, false, true }}, // Harmonic Minor
        {{true,  false, true,  true,  false, true,  false, true,  false, true,  true,  false}}, // Dorian
        {{true,  false, true,  false, true,  true,  false, true,  false, true,  true,  false}}, // Mixolydian
        {{true,  true,  false, true,  false, true,  false, true,  true,  false, true,  false}}, // Phrygian
        {{true,  false, true,  false, true,  false, true,  true,  false, true,  false, true }}, // Lydian
        {{true,  true,  false, true,  false, true,  true,  false, true,  false, true,  false}}, // Locrian
    }};
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
    const Scale ns = static_cast<Scale> (juce::jlimit (0, 9, s));
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

void PitchCorrectorDSP::setCustomScaleNotes (const std::array<bool, 12>& notes)
{
    mCustomScale = notes;
}

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
bool PitchCorrectorDSP::isNoteInScale (int pc, int rootPc, Scale s) const noexcept
{
    // Wrap to 0..11 relative to root.
    int rel = (pc - rootPc) % 12;
    if (rel < 0) rel += 12;
    if (s == Scale::Custom) return mCustomScale[(size_t) rel];
    if ((int) s < 0 || (int) s >= (int) kScaleMasks.size()) return true;
    return kScaleMasks[(size_t) s][(size_t) rel];
}

int PitchCorrectorDSP::snapPcToScale (int pc, int rootPc, Scale s) const noexcept
{
    if (s == Scale::Chromatic) return pc;
    // Walk outward from pc looking for nearest scale note (tie -> down).
    for (int step = 0; step < 12; ++step)
    {
        const int candDown = ((pc - step) % 12 + 12) % 12;
        if (isNoteInScale (candDown, rootPc, s)) return candDown;
        const int candUp   = ((pc + step) % 12 + 12) % 12;
        if (isNoteInScale (candUp, rootPc, s))   return candUp;
    }
    return pc;
}

float PitchCorrectorDSP::snapMidiToScale (float midiFloat) const noexcept
{
    if (mScale == Scale::Chromatic)
        return std::round (midiFloat);   // chromatic = nearest semitone

    const int  midiInt = (int) std::round (midiFloat);
    const int  pc      = ((midiInt % 12) + 12) % 12;
    const int  octave  = midiInt - pc;
    const int  snappedPc = snapPcToScale (pc, mKey, mScale);
    return (float) (octave + snappedPc);
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

    if (bypassed) return;

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
    const float retCoef = mRetuneCoef;

    for (int i = 0; i < numSamples; ++i)
    {
        mCurrentShiftRatio = retCoef * mCurrentShiftRatio
                           + (1.0f - retCoef) * targetRatio;

        const float dryL = L[i];
        float wetL = mShifters[0].processSample (dryL, mCurrentShiftRatio);
        if (mFormantEngaged)
            wetL = mFormant[0].processSample (dryL, wetL, mFormantPreserve, mThroatSemis);
        L[i] = wetL;

        if (R != nullptr)
        {
            const float dryR = R[i];
            float wetR = mShifters[1].processSample (dryR, mCurrentShiftRatio);
            if (mFormantEngaged)
                wetR = mFormant[1].processSample (dryR, wetR, mFormantPreserve, mThroatSemis);
            R[i] = wetR;
        }
    }

    // Publish UI feedback once per block.
    mDetectedHz       .store (detHz,       std::memory_order_release);
    mTargetHz         .store (targetHzPub, std::memory_order_release);
    mCurrentShiftCents.store (1200.0f * std::log2 (juce::jmax (mCurrentShiftRatio, 0.001f)),
                               std::memory_order_release);
}
