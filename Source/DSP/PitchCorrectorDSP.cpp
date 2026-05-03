#include "PitchCorrectorDSP.h"
#include <cmath>
#include <algorithm>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
// PitchCorrectorDSP — Phase H-5 (2026-05-01)
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
// Shifter — 2-grain Hann-windowed time-domain pitch shifter
// ─────────────────────────────────────────────────────────────────────────────

void PitchCorrectorDSP::Shifter::prepare (double sampleRate)
{
    // Grain size ~25 ms at 44.1k (1100 samples), tuned to balance grain density
    // (more = artifacts) vs latency (more = pitch tracking lag).  Stride = half
    // for 50% overlap between two grains.
    grainSize   = juce::jmax (256, (int) std::round (sampleRate * 0.025));
    grainStride = grainSize / 2;
    // Ring sized to 4x grain so we always have past samples to read from
    // regardless of pitch ratio (up or down) without underruns.
    ringSize = grainSize * 4;
    ring.assign ((size_t) ringSize, 0.0f);
    reset();
}

void PitchCorrectorDSP::Shifter::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    writePos = 0;
    sinceLastGrain = 0;
    for (auto& g : grains) { g.active = false; g.readPos = 0.0f; g.outputAge = 0.0f; }
}

float PitchCorrectorDSP::Shifter::processSample (float in, float pitchRatio) noexcept
{
    // Write input to ring.
    ring[(size_t) writePos] = in;
    writePos = (writePos + 1) % ringSize;

    // Trigger a new grain every grainStride samples.  New grain reads from
    // (writePos - grainSize) so it covers the most recent grainSize samples.
    if (sinceLastGrain >= grainStride)
    {
        // Pick the inactive (or older) grain slot.
        int slot = 0;
        if (grains[0].active && grains[1].active)
            slot = (grains[0].outputAge > grains[1].outputAge) ? 0 : 1;
        else
            slot = grains[0].active ? 1 : 0;

        // Start position = writePos - grainSize, wrapped to ring length.
        // Use float for sub-sample read precision via linear interpolation.
        float startReal = (float) writePos - (float) grainSize;
        while (startReal < 0.0f) startReal += (float) ringSize;
        grains[slot].readPos   = startReal;
        grains[slot].outputAge = 0.0f;
        grains[slot].active    = true;
        sinceLastGrain         = 0;
    }
    sinceLastGrain++;

    // Mix active grains with Hann envelope.
    float out = 0.0f;
    for (auto& g : grains)
    {
        if (! g.active) continue;

        // Read sample with linear interpolation between adjacent ring slots.
        float fp = g.readPos;
        while (fp < 0.0f)               fp += (float) ringSize;
        while (fp >= (float) ringSize)  fp -= (float) ringSize;
        const int   ia = (int) fp;
        const int   ib = (ia + 1) % ringSize;
        const float frac = fp - (float) ia;
        const float a    = ring[(size_t) ia];
        const float b    = ring[(size_t) ib];
        const float src  = a + frac * (b - a);

        // Hann envelope from outputAge / grainSize.
        const float t   = juce::jlimit (0.0f, 1.0f,
            g.outputAge / (float) grainSize);
        const float win = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * t);

        out += src * win;

        // Advance read by ratio (>1 = pitch up; <1 = pitch down).
        g.readPos    += pitchRatio;
        g.outputAge  += 1.0f;
        if (g.outputAge >= (float) grainSize)
            g.active = false;
    }

    // 50% overlap of two Hann windows sums to 1.0 — no normalization needed.
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// PitchCorrectorDSP
// ─────────────────────────────────────────────────────────────────────────────

PitchCorrectorDSP::PitchCorrectorDSP() {}
PitchCorrectorDSP::~PitchCorrectorDSP() { releaseResources(); }

void PitchCorrectorDSP::prepare (double sampleRate, int /*maxBlockSize*/)
{
    mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
    mTracker.prepare (mSampleRate);
    for (auto& sh : mShifters) sh.prepare (mSampleRate);
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
    mCurrentShiftRatio = 1.0f;
    mHumanizePhase     = 0.0f;
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
// RNG (xorshift32 — audio-thread-safe, deterministic seed)
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
        const float detMidi    = hzToMidi (detHz);
        const float snappedMidi= snapMidiToScale (detMidi);
        const float targetHz   = midiToHz (snappedMidi);

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
    }

    // Per-sample one-pole smoothing of the active shift ratio toward target.
    // mRetuneCoef = 0 at speed=0 (snap), -> ~1 at long speed (slow tracking).
    const float retCoef = mRetuneCoef;

    for (int i = 0; i < numSamples; ++i)
    {
        mCurrentShiftRatio = retCoef * mCurrentShiftRatio
                           + (1.0f - retCoef) * targetRatio;

        const float shifted0 = mShifters[0].processSample (L[i], mCurrentShiftRatio);
        L[i] = shifted0;
        if (R != nullptr)
        {
            const float shifted1 = mShifters[1].processSample (R[i], mCurrentShiftRatio);
            R[i] = shifted1;
        }
    }

    // Publish UI feedback once per block.
    mDetectedHz       .store (detHz,       std::memory_order_release);
    mTargetHz         .store (targetHzPub, std::memory_order_release);
    mCurrentShiftCents.store (1200.0f * std::log2 (juce::jmax (mCurrentShiftRatio, 0.001f)),
                               std::memory_order_release);

    // Formant Preserve / Throat Shift toggles are stored but DSP is no-op for
    // H-5 -- a follow-up batch will add cepstral envelope swap so these
    // become audible.  The knobs are preset-safe additions today.
    juce::ignoreUnused (mFormantPreserve, mThroatSemis);
}
