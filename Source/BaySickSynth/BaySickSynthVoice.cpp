#include "BaySickSynthVoice.h"
#include <cmath>

// Batch E #9 (2026-05-01): musical-range constants extracted from hot path.
// Not user-tunable - chosen to give the synth a particular feel.  Renamed
// from naked literals so the intent is self-documenting.
namespace
{
    constexpr float kVelTrackOctaveSweep   = 4.0f;   // velocity-to-cutoff full sweep
    constexpr float kEnvFullSweepOctaves   = 4.0f;   // envAmt=1 sweeps cutoff +/- 4 oct
    constexpr float kLfoFullSweepOctaves   = 2.0f;   // LFO at full depth = +/- 2 oct
    constexpr float kModWheelSweepOctaves  = 2.0f;   // mod wheel max = +2 oct

    // Analog drift (P3.10) smoothing time constant.  Calibration: the shipped
    // voicing used a fixed one-pole coefficient of 0.0005 per sample, which is a
    // 1 / (0.0005 * 44100) = 45.35 ms time constant at 44.1 kHz.  Deriving the
    // coefficient from this duration keeps the drift glide identical in seconds
    // at every device rate (the fixed coefficient made it 4x slower at 192 kHz).
    constexpr float kDriftSmoothTauSec = 0.045351f;

    // Paul Kellett's pink-noise section poles/gains and the brown-noise leaky
    // integrator are published for a 44.1 kHz design rate.  Re-poling by
    // p' = sign(p) * |p| ^ (44100 / rate) holds each section's corner at the same
    // frequency in Hz; scaling the gain by sqrt((1 - p'^2) / (1 - p^2)) holds its
    // output variance, so both the -3 dB/oct (pink) and -6 dB/oct (brown) slopes
    // AND the loudness stay put across the rate matrix.  The exponent is 1 at
    // 44.1 kHz, so the derivation reproduces the published numbers exactly there.
    // White reuses the same reference rate for its own in-band energy gain
    // (mWhiteRateGain, see the header) so all three colors are pinned to one design point.
    constexpr double kNoiseDesignRate = 44100.0;

    constexpr float kPinkDesignPole[6] = { 0.99886f,   0.99332f,   0.96900f,
                                           0.86650f,   0.55000f,  -0.7616f };
    constexpr float kPinkDesignGain[6] = { 0.0555179f, 0.0750759f, 0.1538520f,
                                           0.3104856f, 0.5329522f, -0.0168980f };
    constexpr float kBrownDesignPole = 0.998f;
    constexpr float kBrownDesignGain = 0.02f;

    void repoleNoiseSection (double rate, float designPole, float designGain,
                             float& poleOut, float& gainOut) noexcept
    {
        const double exponent = kNoiseDesignRate / rate;
        const double mag      = std::pow ((double) std::fabs (designPole), exponent);
        const double pole     = designPole < 0.0f ? -mag : mag;
        const double design   = (double) designPole;

        poleOut = (float) pole;
        gainOut = (float) ((double) designGain
                           * std::sqrt ((1.0 - pole * pole) / (1.0 - design * design)));
    }
}

BaySickSynthVoice::BaySickSynthVoice()
{
    mOsc.buildSaw();
    mOsc2.buildSaw();
    mLFO.setDepth (1.0f);
}

//==============================================================================
// EVERY rate-dependent member is prepared here.  Before QA-BufferMatrix only the
// two oscillators were, so pitch tracked the device but the filter, both mod
// envelopes, the pitch envelope and the LFO all ran at their 44.1 kHz default --
// i.e. rate/44100 too fast (at 96 kHz a 500 Hz cutoff behaved like 1.09 kHz and a
// 5 Hz LFO ran at 10.9 Hz).  Anything new that stores a per-sample coefficient
// belongs in this function, not in a member initializer.
//
// Order-independent by construction: each helper's prepare() re-derives its
// coefficients from the user values it already holds (AdsrEnvelope re-runs
// setParameters on the stored ADSR, SynthFilter re-runs calcCoeffs on the stored
// cutoff/Q, LFO re-runs recalc on the stored rate), so a device change cannot
// reset the patch whether the DSP's setters ran before or after this call.
void BaySickSynthVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);

    // JUCE hands out a zero rate before a device exists; deriving from it would
    // poison every coefficient with inf/NaN, so keep the last valid preparation.
    if (newRate <= 0.0)
        return;

    mOsc.prepare     (newRate);
    mOsc2.prepare    (newRate);
    mFilter.prepare  (newRate);
    mAmpEnv.prepare  (newRate);
    mFltEnv.prepare  (newRate);
    mPitchEnv.prepare (newRate);
    mLFO.prepare     (newRate);

    mDriftSmoothCoef = 1.0f - std::exp (-1.0f / (kDriftSmoothTauSec * (float) newRate));

    for (int k = 0; k < 6; ++k)
        repoleNoiseSection (newRate, kPinkDesignPole[k], kPinkDesignGain[k],
                            mPinkPole[k], mPinkGain[k]);

    repoleNoiseSection (newRate, kBrownDesignPole, kBrownDesignGain,
                        mBrownPole, mBrownGain);

    mWhiteRateGain = (float) std::sqrt (newRate / kNoiseDesignRate);
}

//==============================================================================
bool BaySickSynthVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<SynthSound*> (sound) != nullptr;
}

void BaySickSynthVoice::startNote (int midiNote, float velocity,
                                    juce::SynthesiserSound*, int pitchWheel)
{
    mCurrentVelocity = velocity;
    mVelRampLeft     = 0;   // S-6(C): a fresh note is not mid-slide-ramp
    mPanRampLeft     = 0;   // #11: nor mid-pan-ramp
    mPitchWheelSemis = (pitchWheel - 8192) / 8192.0f * 2.0f;

    const float targetHz = midiToHz (midiNote + mTranspose);

    // QA-H per-note glide (CC84-armed slide/porta) takes precedence over the
    // mono-context glide: the voice starts AT the source pitch and glides to
    // the target, regardless of what this voice played before.
    if (mGlideFromNote >= 0)
    {
        mCurrentFreqHz = midiToHz (mGlideFromNote + mTranspose);
        mTargetFreqHz  = targetHz;
        const float tSec = mGlideTimePending
            ? (float) ((mGlideTimeMsbMs << 7) | mGlideTimeLsbMs) * 0.001f
            : (mGlideTime > 0.001f ? mGlideTime : 0.06f);
        const float glideSamples = tSec * (float) getSampleRate();
        if (glideSamples > 1.0f && mCurrentFreqHz > 0.0f
            && mCurrentFreqHz != mTargetFreqHz)
            mGlideRatio = std::pow (mTargetFreqHz / mCurrentFreqHz, 1.0f / glideSamples);
        else
        {
            mCurrentFreqHz = targetHz;
            mGlideRatio    = 1.0f;
        }
        // #11 (G-4): an RT glide noteOn carries pan as a CC89 ramp target --
        // mNotePan (the channel snapshot this voice already tracks) glides to
        // it over the same span as the pitch.  No CC10 was emitted for this
        // note, so the ramp starts from the glide SOURCE note's pan.
        if (mPanRampPend > -2.0f)
        {
            if (glideSamples > 1.0f)
            {
                mPanRampTarget = mPanRampPend;
                mPanRampLeft   = (int) glideSamples;
                mPanRampStep   = (mPanRampTarget - mNotePan) / (float) mPanRampLeft;
            }
            else mNotePan = mPanRampPend;
        }
    }
    // Glide only when the previous note is still HELD (not already in release).
    // Otherwise, a voice being reallocated from its release tail would inherit
    // stale mCurrentFreqHz and glide from the wrong starting pitch.
    else if (mCurrentNote >= 0 && ! mInRelease && mGlideTime > 0.001f && mCurrentFreqHz > 0.0f)
    {
        mTargetFreqHz = targetHz;
        const float glideSamples = mGlideTime * (float) getSampleRate();
        if (glideSamples > 1.0f)
            mGlideRatio = std::pow (mTargetFreqHz / mCurrentFreqHz, 1.0f / glideSamples);
        else
            mGlideRatio = 1.0f;
    }
    else
    {
        mCurrentFreqHz = targetHz;
        mTargetFreqHz  = targetHz;
        mGlideRatio    = 1.0f;
    }
    mGlideFromNote    = -1;      // one-shot: never leaks into the next note
    mGlideTimePending = false;
    mPanRampPend      = -999.0f; // #11: consumed above (or irrelevant for a plain note)

    // Consume the per-note expression stashes; re-apply the base envelope with
    // this note's release scale (the DSP's setters only fire on user change).
    mActiveResOffset = mPendResOffset;
    mActiveRelScale  = mPendRelScale;
    mAmpEnv.setParameters (mAmpA, mAmpD, mAmpS,
                           juce::jmax (0.001f, mAmpR * mActiveRelScale));
    applyEffectiveFilterRes();

    mCurrentNote = midiNote;
    mInRelease   = false;

    mAmpEnv.noteOn();
    mFltEnv.noteOn();
    mPitchEnv.noteOn();
    mLFO.reset();

    // Reset inline phase accumulators
    mPhase1 = 0.0;
    mPhase2 = 0.0;
    mPhase3 = 0.0;
    mFMCarrierPhase = 0.0;
    mFMModPhase     = 0.0;
    mDeafSawState   = 0.0f;
    mOsc.reset();    // QA-EngineApvts fold-in: wavetable phase zero at every note-on
    mOsc2.reset();   // (SAW / SAW+SAW / SAW+SQUARE / SQUARE+SQUARE / SUPERSAW consistency)

    mFilter.reset();
    mLastEffCutoff = -1.0f;

    // 1 ms linear fade-in to suppress any click from hard voice termination
    // (e.g. Legato's forced voice 0 stop before this fresh allocation).
    const int fadeSamples = juce::jmax (1, (int) ((float) getSampleRate() * 0.001f));
    mDeclickGain = 0.0f;
    mDeclickStep = 1.0f / (float) fadeSamples;

    // A fresh allocation is never mid-cut (a stolen voice may have been fading).
    mCutFadeActive = false;
    mCutFadeGain   = 1.0f;

    // Arm transient injector (P3.5) - short HPF'd noise burst on noteOn.
    // Only fires when amount > 0 so the voice stays click-free by default.
    if (mTransientAmount > 0.0001f)
    {
        const float sr = (float) getSampleRate();
        mTransientSamplesTotal     = juce::jmax (1, (int) (mTransientDurationMs * 0.001f * sr));
        mTransientSamplesRemaining = mTransientSamplesTotal;
        mTransientHPFState  = 0.0f;
        mTransientPrevInput = 0.0f;
    }
    else
    {
        mTransientSamplesRemaining = 0;
    }

    // Reset multi-burst envelope counter (P3.6)
    mBurstSamplesElapsed = 0;

    // Reset hard-sync oscillator phases (P3.7)
    mSyncPhase1 = 0.0;
    mSyncPhase2 = 0.0;

    // Seed per-voice drift RNG so each voice drifts independently (P3.10).
    //   Use the startNote counter-ish mix so same voice gets a new seed each noteOn.
    mDriftRngState ^= (uint32_t) (midiNote * 2654435761u + (int) (getSampleRate() * 1000.0));
    mDriftCounter = 0;
    mDriftTargetCents = 0.0f;
    mDriftCurrentCents = 0.0f;

    // Stagger unison phases so the stack sounds naturally detuned even at t=0.
    for (int u = 0; u < 6; ++u)
    {
        mDriftRngState = mDriftRngState * 1664525u + 1013904223u;
        mUniPhase[u] = (double)((int) mDriftRngState) * (1.0 / 2147483648.0) * 0.5 + 0.5;
    }
}

void BaySickSynthVoice::stopNote (float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff)
    {
        mAmpEnv.noteOff();
        mFltEnv.noteOff();
        mPitchEnv.noteOff();
        mInRelease = true;
    }
    else
    {
        mAmpEnv.reset();
        mFltEnv.reset();
        mPitchEnv.reset();
        mCurrentNote = -1;
        mInRelease   = false;
        clearCurrentNote();
    }
}

void BaySickSynthVoice::cutFast() noexcept
{
    if (! isVoiceActive())
        return;

    // ~1 ms linear fade-out to zero (matches the declick ramp length), applied to
    // the voice output; the voice retires when the ramp completes.  Click-free
    // because it ramps continuously from the current level, unlike a MIDI note-off
    // which runs the (possibly long) ADSR release = the audible bleed we're fixing.
    const int fadeSamples = juce::jmax (1, (int) ((float) getSampleRate() * 0.001f));
    mCutFadeGain   = 1.0f;
    mCutFadeStep   = 1.0f / (float) fadeSamples;
    mCutFadeActive = true;
}

// Legato retarget: glide to new pitch (if glide>0), update note number and
// velocity, but DO NOT retrigger amp/filter envelopes, LFO, filter, or phase
// accumulators. Called from BaySickSynthDSP Legato MIDI dispatch.
void BaySickSynthVoice::retargetLegato (int midiNote)
{
    const float targetHz = midiToHz (midiNote + mTranspose);

    if (mGlideTime > 0.001f && mCurrentFreqHz > 0.0f)
    {
        mTargetFreqHz = targetHz;
        const float glideSamples = mGlideTime * (float) getSampleRate();
        if (glideSamples > 1.0f)
            mGlideRatio = std::pow (mTargetFreqHz / mCurrentFreqHz, 1.0f / glideSamples);
        else
            mGlideRatio = 1.0f;
    }
    else
    {
        mCurrentFreqHz = targetHz;
        mTargetFreqHz  = targetHz;
        mGlideRatio    = 1.0f;
    }

    mCurrentNote = midiNote;
    // Velocity preserved from the original noteOn - feels more musical.
}

void BaySickSynthVoice::pitchWheelMoved (int newValue)
{
    mPitchWheelSemis = (newValue - 8192) / 8192.0f * 2.0f;
}

void BaySickSynthVoice::controllerMoved (int cc, int value)
{
    if (cc == 1) // CC1 = mod wheel
        mModWheelValue = (float) value / 127.0f;
    else if (cc == 10) // S-7: per-note pan (64 = center)
    {
        // Smoke #19 declick: a SOUNDING voice glides to the new channel pan
        // over ~8 ms -- the old instant set was a gain step on every ringing
        // voice (audible click whenever any note carried a pan).  Idle voices
        // set instantly so the next startNote reads exact channel state.
        const float p = juce::jlimit (-1.0f, 1.0f, ((float) value - 64.0f) / 63.0f);
        if (isVoiceActive())
        {
            mPanRampTarget = p;
            mPanRampLeft   = juce::jmax (1, (int) (0.008 * getSampleRate()));
            mPanRampStep   = (p - mNotePan) / (float) mPanRampLeft;
        }
        else mNotePan = p;
    }
    else if (cc == 74) // CC74 = filter cutoff (Brightness) - Batch E #2
    {
        // Map 0..127 (cc) -> -2..+2 octaves, centered at 64.
        const float norm = (float) value / 127.0f;          // 0..1
        mPerNoteCutoffOctaves = (norm - 0.5f) * 4.0f;        // -2 .. +2
    }
    else if (cc == 71) // per-note resonance offset (QA-H)
        mPendResOffset = ((float) value / 127.0f - 0.5f);    // -0.5..+0.5
    else if (cc == 72) // per-note release scale (QA-H)
        mPendRelScale = std::pow (2.0f, ((float) value / 127.0f - 0.5f) * 4.0f); // 0.25x..4x
    else if (cc == 84) // glide source note (QA-H slide/porta)
        mGlideFromNote = value;
    else if (cc == 86) // S-6(C): RampSlide target loudness (stashed for CC85)
        mSlideTargetVel = (float) value / 127.0f;
    else if (cc == 89) // #11 (G-4): pan RAMP target (stashed for CC85 / RT noteOn)
        mPanRampPend = juce::jlimit (-1.0f, 1.0f, ((float) value - 64.0f) / 63.0f);
    else if (cc == 5)  { mGlideTimeMsbMs = value; mGlideTimePending = true; }
    else if (cc == 37) { mGlideTimeLsbMs = value; mGlideTimePending = true; }
    // CC85 no longer lands here -- BroadcastSynthesiser dispatches it via
    // tryRampTakeover (#36 first-match) and then wipes every voice's stash.
}

bool BaySickSynthVoice::tryRampTakeover (int targetNote)
{
    // Takeover semantics - no retrigger.  Only the voice whose (juce) playing
    // note matches the CC84 anchor bends; #36: the synth stops at the FIRST
    // acceptor so two voices holding the same note can't both retarget.
    if (! (isVoiceActive() && ! mInRelease
           && getCurrentlyPlayingNote() == mGlideFromNote))
        return false;

    const float target = midiToHz (targetNote + mTranspose);
    const float tSec = mGlideTimePending
        ? (float) ((mGlideTimeMsbMs << 7) | mGlideTimeLsbMs) * 0.001f
        : 0.06f;
    const float glideSamples = tSec * (float) getSampleRate();
    if (mCurrentFreqHz > 0.0f && glideSamples > 1.0f
        && target != mCurrentFreqHz)
    {
        mTargetFreqHz = target;
        mGlideRatio   = std::pow (target / mCurrentFreqHz, 1.0f / glideSamples);
    }
    else
    {
        mCurrentFreqHz = target;
        mTargetFreqHz  = target;
        mGlideRatio    = 1.0f;
    }
    mCurrentNote = targetNote;   // kb-track etc. follow the bend target
    // S-6(C): ramp loudness from the base velocity to the slide's over the
    // glide (parallels the pitch bend); mCurrentVelocity feeds amp + filter.
    if (mSlideTargetVel >= 0.0f && glideSamples > 1.0f)
    {
        mVelRampTarget = mSlideTargetVel;
        mVelRampLeft   = (int) glideSamples;
        mVelRampStep   = (mVelRampTarget - mCurrentVelocity) / (float) mVelRampLeft;
    }
    // #11 (G-4): pan glides current -> CC89 target over the same span.
    if (mPanRampPend > -2.0f)
    {
        if (glideSamples > 1.0f)
        {
            mPanRampTarget = mPanRampPend;
            mPanRampLeft   = (int) glideSamples;
            mPanRampStep   = (mPanRampTarget - mNotePan) / (float) mPanRampLeft;
        }
        else mNotePan = mPanRampPend;
    }
    return true;
}

//==============================================================================
void BaySickSynthVoice::renderNextBlock (juce::AudioBuffer<float>& buf,
                                          int startSample, int numSamples)
{
    if (!isVoiceActive()) return;
    if (buf.getNumChannels() < 2) return;

    auto* L = buf.getWritePointer (0);
    auto* R = buf.getWritePointer (1);
    const float sr  = (float) getSampleRate();
    const float twoPi = juce::MathConstants<float>::twoPi;
    const auto  panLaw = baysick::pan::currentLaw();

    for (int i = startSample; i < startSample + numSamples; ++i)
    {
        // ── 0. Slide loudness ramp (S-6(C)) ───────────────────────────────────
        //   mCurrentVelocity feeds both the amp (velScale) and the vel-tracked
        //   filter, so both interpolate base -> slide over the glide.
        if (mVelRampLeft > 0)
        {
            mCurrentVelocity += mVelRampStep;
            if (--mVelRampLeft == 0) mCurrentVelocity = mVelRampTarget;
        }
        // #11 (G-4): pan ramp (CC89 target) -- same shape as the vel ramp.
        if (mPanRampLeft > 0)
        {
            mNotePan += mPanRampStep;
            if (--mPanRampLeft == 0) mNotePan = mPanRampTarget;
        }

        // ── 1. Glide ──────────────────────────────────────────────────────────
        if (mGlideRatio != 1.0f)
        {
            mCurrentFreqHz *= mGlideRatio;
            const bool done = (mGlideRatio > 1.0f && mCurrentFreqHz >= mTargetFreqHz)
                           || (mGlideRatio < 1.0f && mCurrentFreqHz <= mTargetFreqHz);
            if (done) { mCurrentFreqHz = mTargetFreqHz; mGlideRatio = 1.0f; }
        }

        // ── 2. LFO ────────────────────────────────────────────────────────────
        const float lfoRaw = mLFO.getNextSample();   // -1..1 (depth=1)

        // Mod wheel boost to LFO amount
        float effectiveLFO = lfoRaw * mLFOAmount;
        if (mModWheelDest == 1)
            effectiveLFO += lfoRaw * mModWheelValue * mModWheelAmt;

        // ── 3. Oscillator frequency with pitch wheel + LFO + pitch env ────────
        float pitchSemis = mPitchWheelSemis;
        if (mLFODest == BssLFODest::Pitch)
            pitchSemis += effectiveLFO * 4.0f;  // ±4 semitones at full amount

        // Pitch envelope: bipolar amount (-24..+24 semitones) scaled by env value (0-1).
        // Advanced unconditionally so state tracks noteOn/Off correctly; at amount=0
        // the contribution is zero (default preserves current behaviour).
        const float pitchEnvVal = mPitchEnv.getNextSample();
        pitchSemis += pitchEnvVal * mPitchEnvAmt;

        // ── Analog drift (P3.10): slow per-voice pitch wander for warmth ──────
        //   Retargets to a new random offset every ~400 ms, smooths toward it
        //   with a ~45 ms time constant so the motion is always audible but
        //   never abrupt. ±25 cents at max = quarter-tone; typical musical use
        //   is 0.2-0.4 for subtle analog warmth.
        if (mDriftAmount > 0.0001f)
        {
            if (--mDriftCounter <= 0)
            {
                mDriftRngState = mDriftRngState * 1664525u + 1013904223u;
                const float rnd = (int) mDriftRngState * (1.0f / 2147483648.0f); // -1..1
                mDriftTargetCents = rnd * 25.0f;   // ±25 cents at max
                mDriftCounter    = (int) (sr * 0.4f);
            }
            mDriftCurrentCents += (mDriftTargetCents - mDriftCurrentCents) * mDriftSmoothCoef;
            pitchSemis += (mDriftCurrentCents / 100.0f) * mDriftAmount;
        }

        float playFreq = mCurrentFreqHz;
        if (pitchSemis != 0.0f)
            playFreq *= std::pow (2.0f, pitchSemis / 12.0f);
        playFreq = juce::jlimit (20.0f, 22000.0f, playFreq);

        // ── 4. Effective modifier (LFO can modulate) ──────────────────────────
        float effModifier = mModifier;
        if (mLFODest == BssLFODest::OscModifier)
            effModifier = juce::jlimit (0.0f, 1.0f, mModifier + effectiveLFO * 0.3f);

        // ── 5. Filter envelope ────────────────────────────────────────────────
        const float fltEnvVal = mFltEnv.getNextSample();

        // ── 6. Effective filter cutoff ─────────────────────────────────────────
        float effCutoff = mFilterBaseCutoff;

        // Keyboard tracking
        if (mFilterKbTrack > 0.0f && mCurrentNote >= 0)
            effCutoff *= std::pow (2.0f, (mCurrentNote - 60) * mFilterKbTrack / 12.0f);

        // Velocity tracking: higher velocity → higher cutoff
        if (mFilterVelTrack > 0.0f)
            effCutoff *= std::pow (2.0f, (mCurrentVelocity - 0.5f) * mFilterVelTrack * kVelTrackOctaveSweep);

        // Envelope modulation: ± kEnvFullSweepOctaves at full env amount
        if (mFilterEnvAmt != 0.0f)
            effCutoff *= std::pow (2.0f, fltEnvVal * mFilterEnvAmt * kEnvFullSweepOctaves);

        // LFO filter modulation: ± kLfoFullSweepOctaves at full amount
        if (mLFODest == BssLFODest::Filter)
            effCutoff *= std::pow (2.0f, effectiveLFO * kLfoFullSweepOctaves);

        // Mod wheel → Filter: up to + kModWheelSweepOctaves sweep
        if (mModWheelDest == 0)
            effCutoff *= std::pow (2.0f, mModWheelValue * mModWheelAmt * kModWheelSweepOctaves);

        // Batch E #2: per-note filter cutoff offset from CC74.
        if (mPerNoteCutoffOctaves != 0.0f)
            effCutoff *= std::pow (2.0f, mPerNoteCutoffOctaves);

        effCutoff = juce::jlimit (20.0f, 20000.0f, effCutoff);

        if (std::abs (effCutoff - mLastEffCutoff) > effCutoff * 0.001f)
        {
            mFilter.setCutoff (effCutoff);
            mLastEffCutoff = effCutoff;
        }

        // ── 7. Generate oscillator sample ─────────────────────────────────────
        // Noise-only mode (P3.3): osc is muted, noise becomes the primary source.
        // The waveform switch is skipped entirely; sample stays at 0 until the
        // noise stage below adds its content.
        float sample = 0.0f;

        if (! mNoiseOnlyMode)
        {
        switch (mWaveform)
        {
            // ── Saw (single wavetable) ────────────────────────────────────────
            case BssWaveform::Saw:
                mOsc.setFrequency (playFreq);
                sample = mOsc.getNextSample();
                break;

            // ── Saw + Saw (two detuned saws) ──────────────────────────────────
            case BssWaveform::SawSaw:
            {
                float osc1Hz = playFreq;
                float osc2Hz = playFreq;
                if (mDualOscMode == 1)        // Hz Offset: osc2 = osc1 + offset
                {
                    osc2Hz = juce::jlimit (20.0f, 22000.0f, playFreq + effModifier * 2000.0f);
                }
                else if (mDualOscMode == 2)   // Absolute Hz: osc2 = fixed freq
                {
                    osc2Hz = 20.0f * std::pow (1000.0f, effModifier);  // log 20..20000
                }
                else                          // Musical: current detune behaviour
                {
                    const float detHz = playFreq * (std::pow (2.0f, effModifier * 50.0f / 1200.0f) - 1.0f);
                    osc1Hz = playFreq + detHz * 0.5f;
                    osc2Hz = playFreq - detHz * 0.5f;
                }

                float s1 = 0.0f, s2 = 0.0f;
                if (mOscSync)
                {
                    // Hard sync: osc2 phase resets whenever osc1 wraps.
                    mSyncPhase1 += (double) osc1Hz / (double) sr;
                    if (mSyncPhase1 >= 1.0) { mSyncPhase1 -= 1.0; mSyncPhase2 = 0.0; }
                    mSyncPhase2 += (double) osc2Hz / (double) sr;
                    if (mSyncPhase2 >= 1.0) mSyncPhase2 -= 1.0;
                    s1 = 1.0f - 2.0f * (float) mSyncPhase1;
                    s2 = 1.0f - 2.0f * (float) mSyncPhase2;
                }
                else
                {
                    mOsc.setFrequency  (osc1Hz);
                    mOsc2.setFrequency (osc2Hz);
                    s1 = mOsc.getNextSample();
                    s2 = mOsc2.getNextSample();
                }
                sample = mRingMod ? (s1 * s2) : ((s1 + s2) * 0.5f);
                break;
            }

            // ── Pulse (variable width, inline phase) ──────────────────────────
            case BssWaveform::Pulse:
            {
                mPhase1 += (double) playFreq / (double) sr;
                if (mPhase1 >= 1.0) mPhase1 -= 1.0;
                const float pw = juce::jlimit (0.05f, 0.95f, effModifier);
                sample = ((float) mPhase1 < pw) ? 0.5f : -0.5f;
                break;
            }

            // ── Saw + Square ──────────────────────────────────────────────────
            case BssWaveform::SawSquare:
            {
                float osc1Hz = playFreq;
                float osc2Hz = playFreq;
                if (mDualOscMode == 1)
                    osc2Hz = juce::jlimit (20.0f, 22000.0f, playFreq + effModifier * 2000.0f);
                else if (mDualOscMode == 2)
                    osc2Hz = 20.0f * std::pow (1000.0f, effModifier);
                else
                {
                    const float detHz = playFreq * (std::pow (2.0f, effModifier * 50.0f / 1200.0f) - 1.0f);
                    osc1Hz = playFreq + detHz * 0.5f;
                    osc2Hz = playFreq - detHz * 0.5f;
                }

                float s1 = 0.0f, s2 = 0.0f;
                if (mOscSync)
                {
                    mSyncPhase1 += (double) osc1Hz / (double) sr;
                    if (mSyncPhase1 >= 1.0) { mSyncPhase1 -= 1.0; mSyncPhase2 = 0.0; }
                    mSyncPhase2 += (double) osc2Hz / (double) sr;
                    if (mSyncPhase2 >= 1.0) mSyncPhase2 -= 1.0;
                    s1 = 1.0f - 2.0f * (float) mSyncPhase1;
                    s2 = ((float) mSyncPhase2 < 0.5f) ? 1.0f : -1.0f;
                }
                else
                {
                    mOsc.setFrequency  (osc1Hz);
                    mOsc2.setFrequency (osc2Hz);
                    s1 = mOsc.getNextSample();
                    s2 = mOsc2.getNextSample();
                }
                sample = mRingMod ? (s1 * s2) : ((s1 + s2) * 0.5f);
                break;
            }

            // ── Square + Square ───────────────────────────────────────────────
            case BssWaveform::SquareSquare:
            {
                float osc1Hz = playFreq;
                float osc2Hz = playFreq;
                if (mDualOscMode == 1)
                    osc2Hz = juce::jlimit (20.0f, 22000.0f, playFreq + effModifier * 2000.0f);
                else if (mDualOscMode == 2)
                    osc2Hz = 20.0f * std::pow (1000.0f, effModifier);
                else
                {
                    const float detHz = playFreq * (std::pow (2.0f, effModifier * 50.0f / 1200.0f) - 1.0f);
                    osc1Hz = playFreq + detHz * 0.5f;
                    osc2Hz = playFreq - detHz * 0.5f;
                }

                float s1 = 0.0f, s2 = 0.0f;
                if (mOscSync)
                {
                    mSyncPhase1 += (double) osc1Hz / (double) sr;
                    if (mSyncPhase1 >= 1.0) { mSyncPhase1 -= 1.0; mSyncPhase2 = 0.0; }
                    mSyncPhase2 += (double) osc2Hz / (double) sr;
                    if (mSyncPhase2 >= 1.0) mSyncPhase2 -= 1.0;
                    s1 = ((float) mSyncPhase1 < 0.5f) ? 1.0f : -1.0f;
                    s2 = ((float) mSyncPhase2 < 0.5f) ? 1.0f : -1.0f;
                }
                else
                {
                    mOsc.setFrequency  (osc1Hz);
                    mOsc2.setFrequency (osc2Hz);
                    s1 = mOsc.getNextSample();
                    s2 = mOsc2.getNextSample();
                }
                sample = mRingMod ? (s1 * s2) : ((s1 + s2) * 0.5f);
                break;
            }

            // ── Supersaw (7-voice unison) ─────────────────────────────────────
            case BssWaveform::Supersaw:
                mOsc.setDetune   (effModifier * 0.5f);  // 0..0.5 semitones spread
                mOsc.setFrequency (playFreq);
                sample = mOsc.getNextSample();
                break;

            // ── Bell (FM synthesis, 2:1 ratio) ────────────────────────────────
            case BssWaveform::Bell:
            {
                const float fmFreq  = playFreq * 2.0f;
                const float fmBeta  = effModifier * 20.0f;   // FM index 0..20
                mFMModPhase += (double) fmFreq / (double) sr;
                if (mFMModPhase >= 1.0) mFMModPhase -= 1.0;
                const float modOut  = std::sin ((float) mFMModPhase * twoPi) * fmBeta;
                mFMCarrierPhase += (double) playFreq / (double) sr;
                if (mFMCarrierPhase >= 1.0) mFMCarrierPhase -= 1.0;
                sample = std::sin ((float) mFMCarrierPhase * twoPi + modOut);
                break;
            }

            // ── DeafSaw (saw through one-pole LP) ─────────────────────────────
            case BssWaveform::DeafSaw:
            {
                // Raw sawtooth
                mPhase1 += (double) playFreq / (double) sr;
                if (mPhase1 >= 1.0) mPhase1 -= 1.0;
                const float rawSaw = 1.0f - 2.0f * (float) mPhase1;

                // One-pole LP: cutoff = 100..3000 Hz driven by modifier
                const float fc    = 100.0f + effModifier * effModifier * 2900.0f;
                const float alpha = 1.0f - std::exp (-twoPi * fc / sr);
                mDeafSawState += alpha * (rawSaw - mDeafSawState);
                sample = mDeafSawState;
                break;
            }

            // ── SpreadOct (root + octave up + octave down) ────────────────────
            case BssWaveform::SpreadOct:
            {
                mPhase1 += (double) playFreq          / (double) sr;
                mPhase2 += (double) playFreq * 2.0f   / (double) sr;
                mPhase3 += (double) playFreq * 0.5f   / (double) sr;
                if (mPhase1 >= 1.0) mPhase1 -= 1.0;
                if (mPhase2 >= 1.0) mPhase2 -= 1.0;
                if (mPhase3 >= 1.0) mPhase3 -= 1.0;
                const float s1 = 1.0f - 2.0f * (float) mPhase1;
                const float s2 = 1.0f - 2.0f * (float) mPhase2;
                const float s3 = 1.0f - 2.0f * (float) mPhase3;
                const float mix = effModifier;
                sample = s1 * (1.0f - mix * 0.5f) + (s2 + s3) * mix * 0.25f;
                break;
            }

            // ── SpreadFifth (root + fifth up + fifth down) ────────────────────
            case BssWaveform::SpreadFifth:
            {
                mPhase1 += (double) playFreq               / (double) sr;
                mPhase2 += (double) playFreq * 1.5f        / (double) sr;
                mPhase3 += (double) (playFreq * 2.0f / 3.0f) / (double) sr;
                if (mPhase1 >= 1.0) mPhase1 -= 1.0;
                if (mPhase2 >= 1.0) mPhase2 -= 1.0;
                if (mPhase3 >= 1.0) mPhase3 -= 1.0;
                const float s1 = 1.0f - 2.0f * (float) mPhase1;
                const float s2 = 1.0f - 2.0f * (float) mPhase2;
                const float s3 = 1.0f - 2.0f * (float) mPhase3;
                const float mix = effModifier;
                sample = s1 * (1.0f - mix * 0.5f) + (s2 + s3) * mix * 0.25f;
                break;
            }

            // ── Sine (pure sine, P3.2) ────────────────────────────────────────
            //   Critical for 808/909 kicks, sub bass, Hammond drawbars, theremin,
            //   whistle leads, DX7 operator foundation. Modifier unused.
            case BssWaveform::Sine:
            {
                mPhase1 += (double) playFreq / (double) sr;
                if (mPhase1 >= 1.0) mPhase1 -= 1.0;
                sample = std::sin ((float) mPhase1 * twoPi);
                break;
            }

            default:
                break;
        }
        }  // end if (!mNoiseOnlyMode)

        // ── 8. Noise (P3.9: white / pink / brown colour) ──────────────────────
        //   Normal mode: mixed in on top of the oscillator sample.
        //   Noise-only mode: sample starts at 0, noise becomes the whole voice.
        if (mNoiseOnlyMode || mNoiseLevel > 0.001f)
        {
            mNoiseSeed = mNoiseSeed * 1664525u + 1013904223u;
            const float whiteN = (int) mNoiseSeed * (1.0f / 2147483648.0f); // -1..1

            // White (mNoiseColor == 0) carries the rate gain so its audible-band
            // energy matches pink and brown across the rate matrix; the pink and
            // brown sections below take the UNSCALED whiteN, since their own
            // re-poled gains already hold their level.
            float colouredN = whiteN * mWhiteRateGain;

            // Pink / brown poles + gains are re-derived per sample rate in
            // setCurrentPlaybackSampleRate; the b[6] and 0.5362 terms are flat
            // (no pole), so they carry no rate dependence and stay literal.
            if (mNoiseColor == 1)  // Pink (Paul Kellett filter, ~ -3 dB/oct slope)
            {
                mPinkB[0] = mPinkPole[0] * mPinkB[0] + whiteN * mPinkGain[0];
                mPinkB[1] = mPinkPole[1] * mPinkB[1] + whiteN * mPinkGain[1];
                mPinkB[2] = mPinkPole[2] * mPinkB[2] + whiteN * mPinkGain[2];
                mPinkB[3] = mPinkPole[3] * mPinkB[3] + whiteN * mPinkGain[3];
                mPinkB[4] = mPinkPole[4] * mPinkB[4] + whiteN * mPinkGain[4];
                mPinkB[5] = mPinkPole[5] * mPinkB[5] + whiteN * mPinkGain[5];
                colouredN = (mPinkB[0] + mPinkB[1] + mPinkB[2] + mPinkB[3]
                           + mPinkB[4] + mPinkB[5] + mPinkB[6] + whiteN * 0.5362f) * 0.11f;
                mPinkB[6] = whiteN * 0.115926f;
            }
            else if (mNoiseColor == 2)  // Brown (leaky integrator, ~ -6 dB/oct slope)
            {
                mBrownState = mBrownPole * mBrownState + whiteN * mBrownGain;
                mBrownState = juce::jlimit (-1.0f, 1.0f, mBrownState);
                colouredN   = mBrownState * 3.5f;   // level-match to white
            }

            const float noiseGain = mNoiseOnlyMode
                ? (mNoiseLevel > 0.001f ? mNoiseLevel : 1.0f)
                : mNoiseLevel;
            sample += colouredN * noiseGain;
        }

        // ── 9. Filter ─────────────────────────────────────────────────────────
        sample = mFilter.processSample (sample);

        // ── 10. Amp envelope + velocity (blendable 0..1) ──────────────────────
        //   mVelAmpTrack=1: amp fully follows velocity (current behavior)
        //   mVelAmpTrack=0: amp ignores velocity (uniform volume across keys)
        const float ampVal = mAmpEnv.getNextSample();
        const float velScale = mVelAmpTrack * mCurrentVelocity + (1.0f - mVelAmpTrack);
        sample *= ampVal * velScale;

        // ── 10a. Multi-burst modulator (P3.6) ──────────────────────────────────
        //   When on, fires N exp-decay amplitude pulses at noteOn, spaced
        //   mBurstSpacingMs apart. After all N bursts complete, multiplier
        //   returns to 1.0 so the normal amp env sustain/release takes over.
        //   This composes authentically with the amp env (real 808 handclap
        //   is pulses × decay envelope, not a dedicated "clap envelope").
        if (mBurstMode)
        {
            const float burstSpacingSamples = juce::jmax (1.0f, mBurstSpacingMs * 0.001f * sr);
            const int   burstIdx            = (int) ((float) mBurstSamplesElapsed / burstSpacingSamples);

            if (burstIdx < mBurstCount)
            {
                const float timeInBurst = std::fmod ((float) mBurstSamplesElapsed, burstSpacingSamples);
                const float tau         = burstSpacingSamples * 0.25f;
                const float burstMul    = std::exp (-timeInBurst / tau);
                sample *= burstMul;
            }
            // else: bursts complete -> multiplier = 1 (pass-through)
            ++mBurstSamplesElapsed;
        }

        // Declick fade-in (1 ms) applied after envelope so the ramp survives
        // even fast-attack settings.
        if (mDeclickGain < 1.0f)
        {
            sample *= mDeclickGain;
            mDeclickGain = juce::jmin (1.0f, mDeclickGain + mDeclickStep);
        }

        // ── 10b. Transient injector (P3.5) ────────────────────────────────────
        //   Added AFTER amp envelope + filter so the "click" plays at full
        //   level regardless of attack time / filter cutoff. This matches how
        //   analog synth click circuits work (parallel path, not env-gated).
        if (mTransientSamplesRemaining > 0)
        {
            mTransientNoiseSeed = mTransientNoiseSeed * 1664525u + 1013904223u;
            // Same in-band energy correction as the main white path: the burst is
            // flat-spectrum LCG noise, so without the gain the click thins out as
            // the device rate rises.
            const float rawNoise = (int) mTransientNoiseSeed * (1.0f / 2147483648.0f)
                                 * mWhiteRateGain;

            // One-pole HPF for colour
            const float alpha = std::exp (-twoPi * mTransientColourHz / sr);
            const float y = alpha * (mTransientHPFState + rawNoise - mTransientPrevInput);
            mTransientHPFState  = y;
            mTransientPrevInput = rawNoise;

            // Exp-decay envelope: 1.0 at t=0, ~0.02 at t=duration
            const float envT = 1.0f - (float) mTransientSamplesRemaining / (float) mTransientSamplesTotal;
            const float env  = std::exp (-envT * 4.0f);

            sample += y * env * mTransientAmount;
            --mTransientSamplesRemaining;
        }

        // ── 10c. Unison (P3.11) - detuned saw stack, stereo-spread ────────────
        //   Adds (mUnisonVoices - 1) saw copies at slightly detuned frequencies,
        //   panned symmetrically across stereo by spread. Main voice stays centred.
        //   Applied post-amp-env + velocity so overall level tracks the note.
        float Lout = sample;
        float Rout = sample;
        if (mUnisonVoices > 1 && playFreq > 1.0f)
        {
            const int   N   = mUnisonVoices;
            const float maxCents = mUnisonDetune * 50.0f;   // ±50 cents at max
            const float envGain  = ampVal * velScale;
            const float levelN   = 1.0f / std::sqrt ((float) N);

            for (int u = 1; u < N; ++u)
            {
                // Symmetric position in [-1, +1] across the N-1 extra voices
                const float pos       = ((float)(u - 1) / (float)(N - 1)) * 2.0f - 1.0f;
                const float detuneHz  = playFreq * (std::pow (2.0f, pos * maxCents / 1200.0f) - 1.0f);
                const float uniFreq   = playFreq + detuneHz;

                mUniPhase[u - 1] += (double) uniFreq / (double) sr;
                if (mUniPhase[u - 1] >= 1.0) mUniPhase[u - 1] -= 1.0;

                float uniSaw = (1.0f - 2.0f * (float) mUniPhase[u - 1]) * envGain * levelN;

                // Pan: pos * spread (-1 = left, +1 = right)
                const float pan = pos * mUnisonSpread;
                Lout += uniSaw * (0.5f * (1.0f - pan));
                Rout += uniSaw * (0.5f * (1.0f + pan));
            }
        }

        // Cut-self fast fade-out (QA-CutSelfReview): applied to the final L/R so
        // the unison stack fades with the main voice; retire when the ramp hits 0.
        if (mCutFadeActive)
        {
            Lout *= mCutFadeGain;
            Rout *= mCutFadeGain;
            mCutFadeGain -= mCutFadeStep;
        }

        // ── 11. Stereo output ─────────────────────────────────────────────────
        // Per-note pan through the project law (BaySickSynth has no engine pan
        // knob of its own; the strip pan is the other stage).  Center = unity,
        // so a centered note is bit-identical to the pre-pan output.
        const auto& pg = mPanGains.get (mNotePan, panLaw);
        L[i] += Lout * pg.l;
        R[i] += Rout * pg.r;

        if (mCutFadeActive && mCutFadeGain <= 0.0f)
        {
            mCutFadeActive = false;
            mAmpEnv.reset();
            mFltEnv.reset();
            mPitchEnv.reset();
            mCurrentNote = -1;
            mInRelease   = false;
            clearCurrentNote();
            break;
        }

        // ── 12. Deactivate when amp envelope finishes ─────────────────────────
        if (!mAmpEnv.isActive())
        {
            mCurrentNote = -1;
            clearCurrentNote();
            break;
        }
    }
}

//==============================================================================
// ── Setters ───────────────────────────────────────────────────────────────────

void BaySickSynthVoice::setWaveform (BssWaveform w)
{
    if (w == mWaveform) return;
    mWaveform = w;
    rebuildOscWavetable();
}

void BaySickSynthVoice::setTranspose  (int semitones) { mTranspose  = juce::jlimit (-24, 24, semitones); }
void BaySickSynthVoice::setModifier   (float m)       { mModifier   = juce::jlimit (0.0f, 1.0f, m); }
void BaySickSynthVoice::setDualOscMode (int mode)     { mDualOscMode = juce::jlimit (0, 2, mode); }
void BaySickSynthVoice::setOscSync    (bool on)       { mOscSync = on; }
void BaySickSynthVoice::setRingMod    (bool on)       { mRingMod = on; }
void BaySickSynthVoice::setDriftAmount (float amount) { mDriftAmount = juce::jlimit (0.0f, 1.0f, amount); }
void BaySickSynthVoice::setUnisonVoices (int voices)  { mUnisonVoices = juce::jlimit (1, 7, voices); }
void BaySickSynthVoice::setUnisonDetune (float a)     { mUnisonDetune = juce::jlimit (0.0f, 1.0f, a); }
void BaySickSynthVoice::setUnisonSpread (float a)     { mUnisonSpread = juce::jlimit (0.0f, 1.0f, a); }
void BaySickSynthVoice::setNoiseLevel (float n)       { mNoiseLevel = juce::jlimit (0.0f, 1.0f, n); }
void BaySickSynthVoice::setNoiseOnlyMode (bool on)    { mNoiseOnlyMode = on; }
void BaySickSynthVoice::setNoiseColor    (int color)  { mNoiseColor = juce::jlimit (0, 2, color); }
void BaySickSynthVoice::setGlideTime  (float secs)    { mGlideTime  = juce::jmax (0.0f, secs); }
void BaySickSynthVoice::setVoiceMode  (BssVoiceMode mode) { mVoiceMode = mode; }

void BaySickSynthVoice::setModWheelDest (int dest) { mModWheelDest = juce::jlimit (0, 1, dest); }
void BaySickSynthVoice::setModWheelAmt  (float a)  { mModWheelAmt  = juce::jlimit (0.0f, 1.0f, a); }

void BaySickSynthVoice::setAmpEnv (float a, float d, float s, float r)
{
    mAmpA = a; mAmpD = d; mAmpS = s; mAmpR = r;
    mAmpEnv.setParameters (a, d, s, juce::jmax (0.001f, r * mActiveRelScale));
}

void BaySickSynthVoice::setVelAmpTrack (float amount)
{
    mVelAmpTrack = juce::jlimit (0.0f, 1.0f, amount);
}

void BaySickSynthVoice::setPitchEnv (float a, float d, float s, float r)
{
    mPitchEnv.setParameters (a, d, s, r);
}

void BaySickSynthVoice::setPitchEnvAmt (float semitones)
{
    mPitchEnvAmt = juce::jlimit (-24.0f, 24.0f, semitones);
}

void BaySickSynthVoice::setTransientAmount (float amount)
{
    mTransientAmount = juce::jlimit (0.0f, 1.0f, amount);
}

void BaySickSynthVoice::setTransientDuration (float ms)
{
    mTransientDurationMs = juce::jlimit (0.0f, 20.0f, ms);
}

void BaySickSynthVoice::setTransientColour (float hz)
{
    mTransientColourHz = juce::jlimit (200.0f, 10000.0f, hz);
}

void BaySickSynthVoice::setBurstMode (bool on)
{
    mBurstMode = on;
}

void BaySickSynthVoice::setBurstCount (int count)
{
    mBurstCount = juce::jlimit (1, 8, count);
}

void BaySickSynthVoice::setBurstSpacing (float ms)
{
    mBurstSpacingMs = juce::jlimit (1.0f, 100.0f, ms);
}

void BaySickSynthVoice::setFilterType (BssFilterType type)
{
    if (type == mFilterType) return;
    mFilterType = type;
    updateFilterType();
}

void BaySickSynthVoice::setFilterCutoff (float hz)
{
    mFilterBaseCutoff = juce::jlimit (20.0f, 20000.0f, hz);
    mLastEffCutoff = -1.0f;
}

void BaySickSynthVoice::setFilterRes (float res)
{
    mFilterRes = juce::jlimit (0.0f, 1.0f, res);
    applyEffectiveFilterRes();
}

void BaySickSynthVoice::applyEffectiveFilterRes()
{
    const float eff = juce::jlimit (0.0f, 1.0f, mFilterRes + mActiveResOffset);
    const float q   = 0.707f + eff * (10.0f - 0.707f);
    mFilter.setResonance (q);
}

void BaySickSynthVoice::setFilterEnvAmt   (float amount) { mFilterEnvAmt   = juce::jlimit (-1.0f, 1.0f, amount); }
void BaySickSynthVoice::setFilterKbTrack  (float amount) { mFilterKbTrack  = juce::jlimit (0.0f,  1.0f, amount); }
void BaySickSynthVoice::setFilterVelTrack (float amount) { mFilterVelTrack = juce::jlimit (0.0f,  1.0f, amount); }

void BaySickSynthVoice::setFltEnv (float a, float d, float s, float r)
{
    mFltEnv.setParameters (a, d, s, r);
}

void BaySickSynthVoice::setLFOShape  (LFOShape shape) { mLFO.setShape (shape); }
void BaySickSynthVoice::setLFODest   (BssLFODest dest) { mLFODest = dest; }
void BaySickSynthVoice::setLFORate   (float hz)        { mLFO.setRate (hz); }
void BaySickSynthVoice::setLFOAmount (float amount)    { mLFOAmount = juce::jlimit (0.0f, 1.0f, amount); }

//==============================================================================
// ── Private helpers ───────────────────────────────────────────────────────────

float BaySickSynthVoice::midiToHz (int note, float extraSemis) noexcept
{
    return 440.0f * std::pow (2.0f, (note - 69 + extraSemis) / 12.0f);
}

void BaySickSynthVoice::rebuildOscWavetable()
{
    switch (mWaveform)
    {
        case BssWaveform::Saw:
            mOsc.buildSaw();
            break;

        case BssWaveform::SawSaw:
            mOsc.buildSaw();
            mOsc2.buildSaw();
            break;

        case BssWaveform::Pulse:
            // Inline phase - no wavetable needed; reset osc to harmless state
            mOsc.buildSine();
            break;

        case BssWaveform::SawSquare:
            mOsc.buildSaw();
            mOsc2.buildSquare();
            break;

        case BssWaveform::SquareSquare:
            mOsc.buildSquare();
            mOsc2.buildSquare();
            break;

        case BssWaveform::Supersaw:
            mOsc.setUnisonVoices (7, 0.7f);
            mOsc.buildSaw();
            break;

        case BssWaveform::Bell:
        case BssWaveform::DeafSaw:
        case BssWaveform::SpreadOct:
        case BssWaveform::SpreadFifth:
            // All inline - reset osc to harmless state
            mOsc.setUnisonVoices (1, 0.0f);
            mOsc.buildSine();
            break;

        default:
            break;
    }
}

void BaySickSynthVoice::updateFilterType()
{
    static constexpr FilterMode kModes[] = {
        FilterMode::LowPass, FilterMode::HighPass,
        FilterMode::BandPass, FilterMode::Notch
    };
    const int idx = juce::jlimit (0, 3, (int) mFilterType);
    mFilter.setMode (kModes[idx]);
}
