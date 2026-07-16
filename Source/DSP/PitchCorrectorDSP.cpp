#include "PitchCorrectorDSP.h"
#include <cmath>
#include <algorithm>
#include <cstring>

#if BAYSICK_HAS_RUBBERBAND
 #include <rubberband/RubberBandLiveShifter.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
// PitchCorrectorDSP - realtime vocal pitch correction (QA-Fe Task 4, 2026-07-14)
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
// MonoFifo - single-threaded (audio-thread) mono ring; preallocated in prepare().
// ─────────────────────────────────────────────────────────────────────────────
void PitchCorrectorDSP::MonoFifo::init (int capacity)
{
    cap = juce::jmax (1, capacity);
    buf.assign ((size_t) cap, 0.0f);
    head = 0; count = 0;
}

void PitchCorrectorDSP::MonoFifo::clear() noexcept
{
    head = 0; count = 0;
    std::fill (buf.begin(), buf.end(), 0.0f);
}

void PitchCorrectorDSP::MonoFifo::push (const float* src, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        const int w = (head + count) % cap;
        buf[(size_t) w] = src[i];
        if (count < cap) ++count; else head = (head + 1) % cap;
    }
}

void PitchCorrectorDSP::MonoFifo::pushZeros (int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        const int w = (head + count) % cap;
        buf[(size_t) w] = 0.0f;
        if (count < cap) ++count; else head = (head + 1) % cap;
    }
}

void PitchCorrectorDSP::MonoFifo::pop (float* dst, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        if (count > 0) { dst[i] = buf[(size_t) head]; head = (head + 1) % cap; --count; }
        else           { dst[i] = 0.0f; }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// PitchCorrectorDSP
// ─────────────────────────────────────────────────────────────────────────────
PitchCorrectorDSP::PitchCorrectorDSP() {}
PitchCorrectorDSP::~PitchCorrectorDSP() { releaseResources(); }

void PitchCorrectorDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
    mMaxBlock   = juce::jmax (1, maxBlockSize);
    mTracker.prepare (mSampleRate);

#if BAYSICK_HAS_RUBBERBAND
    using RBL = RubberBand::RubberBandLiveShifter;
    const int opts = RBL::OptionFormantPreserved | RBL::OptionWindowShort;
    mLive = std::make_unique<RBL> ((size_t) mSampleRate, 1, opts);
    mLive->setPitchScale (1.0);                       // set before querying start delay
    mBlockSize      = (int) mLive->getBlockSize();
    mLatencySamples = (int) mLive->getStartDelay() + mBlockSize;
    mLivePreserve   = true;                           // matches the constructed option
#else
    mBlockSize      = 0;
    mLatencySamples = 0;
#endif

    mBlockSize = juce::jmax (0, mBlockSize);
    mInBlock .assign ((size_t) juce::jmax (1, mBlockSize), 0.0f);
    mOutBlock.assign ((size_t) juce::jmax (1, mBlockSize), 0.0f);
    mMonoIn  .assign ((size_t) mMaxBlock, 0.0f);
    mWetOut  .assign ((size_t) mMaxBlock, 0.0f);
    const int cap = 2 * (mBlockSize + mMaxBlock) + 64;
    mInFifo .init (cap);
    mOutFifo.init (cap);

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
#if BAYSICK_HAS_RUBBERBAND
    if (mLive) { mLive->reset(); mLive->setPitchScale (1.0); mLive->setFormantScale (0.0); }
#endif
    mInFifo .clear();
    mOutFifo.clear();
    if (mBlockSize > 0) mOutFifo.pushZeros (mBlockSize);   // prime the block-adapter latency
    mLiveActive       = false;
    mLivePitchScale   = 1.0;
    mLiveFormantScale = 0.0;
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

#if BAYSICK_HAS_RUBBERBAND
    const bool wantOn = ! bypassed;

    // Settled bypass: dry passthrough with the shifter kept cold -- the ~48 ms
    // engine never runs while correction is off (fast-path bypass).  On the
    // just-settled edge, re-prime so the next engage is a clean cold start.
    if (! wantOn && mEngageFade <= 0.0f)
    {
        if (mLiveActive)
        {
            if (mLive) mLive->reset();
            mInFifo.clear(); mOutFifo.clear();
            if (mBlockSize > 0) mOutFifo.pushZeros (mBlockSize);
            mLiveActive = false;
        }
        mCurrentTargetMidi = -1.0f;
        mCurrentShiftRatio = 1.0f;
        return;
    }

    // Engage edge: cold-start the shifter -- the delay is acquired here (B3/B6);
    // the crossfade below masks the dry-side click.
    if (! mLiveActive)
    {
        if (mLive)
        {
            mLive->reset();
            mLive->setPitchScale   (mLivePitchScale);
            mLive->setFormantScale (mLiveFormantScale);
        }
        mInFifo.clear(); mOutFifo.clear();
        if (mBlockSize > 0) mOutFifo.pushZeros (mBlockSize);
        mLiveActive = true;
    }

    // Read the latest pitch reading once per block (tracker is ~50 ms behind).
    const float detHz      = mTracker.getFrequencyHz();
    const float confidence = mTracker.getConfidence();

    float targetRatio = 1.0f;
    float targetHzPub = 0.0f;

    if (detHz > 0.0f && confidence > 0.4f)
    {
        const float detMidi = hzToMidi (detHz);

        // QA-F Task 5 note-change hysteresis: hold the current target note until
        // the sung pitch commits to a different one.  0.65 st past the semitone
        // midpoint stops vibrato riding a boundary from flip-flopping the target.
        if (mCurrentTargetMidi < 0.0f
            || std::abs (detMidi - mCurrentTargetMidi) > 0.65f)
            mCurrentTargetMidi = snapMidiToScale (detMidi);

        const float targetHz  = midiToHz (mCurrentTargetMidi);
        const float fullRatio = targetHz / juce::jmax (detHz, 1.0f);
        targetRatio           = 1.0f + (fullRatio - 1.0f) * mStrength;

        if (mHumanizeCents > 0.001f)
        {
            mHumanizePhase += nextRandPm1() * mHumanizeCents * 0.05f;
            mHumanizePhase  = juce::jlimit (-mHumanizeCents, mHumanizeCents, mHumanizePhase);
            targetRatio    *= std::pow (2.0f, mHumanizePhase / 1200.0f);   // cents -> ratio
        }

        targetHzPub = targetHz;
    }
    else
    {
        mCurrentTargetMidi = -1.0f;   // unvoiced: re-snap fresh next phrase
    }

    // Formant Preserve maps to the engine's native option; Throat to its formant
    // scale (0.0 == auto-preserve).  Applied only on change (RT-safe setters).
    if (mLive && mFormantPreserve != mLivePreserve)
    {
        using RBL = RubberBand::RubberBandLiveShifter;
        mLive->setFormantOption (mFormantPreserve ? RBL::OptionFormantPreserved
                                                  : RBL::OptionFormantShifted);
        mLivePreserve = mFormantPreserve;
    }
    const double fScale = (std::abs (mThroatSemis) < 0.01f)
                        ? 0.0 : std::pow (2.0, (double) mThroatSemis / 12.0);

    // Accumulate mono input, then drain whole shifter blocks.
    if (R != nullptr)
        for (int i = 0; i < numSamples; ++i) mMonoIn[(size_t) i] = 0.5f * (L[i] + R[i]);
    else
        for (int i = 0; i < numSamples; ++i) mMonoIn[(size_t) i] = L[i];
    mInFifo.push (mMonoIn.data(), numSamples);

    const float retCoef = mRetuneCoef;
    while (mBlockSize > 0 && mInFifo.size() >= mBlockSize)
    {
        mInFifo.pop (mInBlock.data(), mBlockSize);
        // Advance the ratio smoothing across the block so the RetuneSpeed time
        // constant is unchanged by the shifter's block granularity.
        for (int k = 0; k < mBlockSize; ++k)
            mCurrentShiftRatio = retCoef * mCurrentShiftRatio + (1.0f - retCoef) * targetRatio;

        if (mLive)
        {
            const double ps = juce::jlimit (0.25, 4.0, (double) mCurrentShiftRatio);
            if (ps != mLivePitchScale)       { mLive->setPitchScale   (ps);     mLivePitchScale   = ps; }
            if (fScale != mLiveFormantScale) { mLive->setFormantScale (fScale); mLiveFormantScale = fScale; }
            const float* ip[1] = { mInBlock.data() };
            float*       op[1] = { mOutBlock.data() };
            mLive->shift (ip, op);
            mOutFifo.push (mOutBlock.data(), mBlockSize);
        }
        else
        {
            mOutFifo.push (mInBlock.data(), mBlockSize);   // shifter absent: passthrough
        }
    }

    // Pop this block's worth of wet, then equal-power crossfade dry <-> wet.
    // The wet stream carries the shifter latency; at the engage edge it is the
    // primed silence filling in -- the documented one-time delay jump (B6).
    mOutFifo.pop (mWetOut.data(), numSamples);

    const float fadeTgt = wantOn ? 1.0f : 0.0f;
    constexpr float halfPi = juce::MathConstants<float>::halfPi;
    for (int i = 0; i < numSamples; ++i)
    {
        mEngageFade = (fadeTgt > mEngageFade)
            ? juce::jmin (1.0f, mEngageFade + mEngageFadeStep)
            : juce::jmax (0.0f, mEngageFade - mEngageFadeStep);
        const float gW  = std::sin (mEngageFade * halfPi);
        const float gD  = std::cos (mEngageFade * halfPi);
        const float wet = mWetOut[(size_t) i];

        L[i] = L[i] * gD + wet * gW;
        if (R != nullptr) R[i] = R[i] * gD + wet * gW;   // mono wet to both channels
    }

    mDetectedHz       .store (detHz,       std::memory_order_release);
    mTargetHz         .store (targetHzPub, std::memory_order_release);
    mCurrentShiftCents.store (1200.0f * std::log2 (juce::jmax (mCurrentShiftRatio, 0.001f)),
                               std::memory_order_release);
#else
    juce::ignoreUnused (L, R);   // corrector unavailable -> dry passthrough
#endif
}
