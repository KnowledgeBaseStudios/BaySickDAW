#include "AdditiveVoice.h"
#include <cmath>

AdditiveVoice::AdditiveVoice()
{
    // Stereo filter 1 - prepare called again in setCurrentPlaybackSampleRate.
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels      = 2;
    mFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    mFilter.prepare (spec);
    mFilter.setCutoffFrequency (mFilterCutoff);
    mFilter.setResonance       (mFilterRes);

    // Stereo filter 2 - lowpass at 20 kHz by default (transparent / fully open).
    mFilter2.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    mFilter2.prepare (spec);
    mFilter2.setCutoffFrequency (mFilter2Cutoff);
    mFilter2.setResonance       (mFilter2Res);

    // T1c notch helpers - bandpass mode used to compute (input - bp) for notch.
    mFilterNotchHelper .setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    mFilterNotchHelper .prepare (spec);
    mFilterNotchHelper .setCutoffFrequency (mFilterCutoff);
    mFilterNotchHelper .setResonance       (mFilterRes);
    mFilter2NotchHelper.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    mFilter2NotchHelper.prepare (spec);
    mFilter2NotchHelper.setCutoffFrequency (mFilter2Cutoff);
    mFilter2NotchHelper.setResonance       (mFilter2Res);

    recalcUnisonSlots();
}

//==============================================================================
void AdditiveVoice::setEngines (const HarmonicEngine* partA,
                                 const HarmonicEngine* partB)
{
    mTemplateA = partA;
    mTemplateB = partB;
}

void AdditiveVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
    mSampleRate = (newRate > 0.0) ? newRate : 44100.0;

    // Recompute IIR coefficients for the new sample rate.
    // α = exp(-1 / (τ × fs)) where τ is the time constant in seconds.
    mFreqCoeff = float (std::exp (-1.0 / (0.010 * mSampleRate)));  // fast 10ms
    updateGlideCoeff();

    mVolSmooth  .reset (mSampleRate, 0.005);
    mPartASmooth.reset (mSampleRate, 0.005);
    mPartBSmooth.reset (mSampleRate, 0.005);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = mSampleRate;
    spec.maximumBlockSize = 512;
    spec.numChannels      = 2;

    mFilter.prepare (spec);
    mFilter.setCutoffFrequency (mFilterCutoff);
    mFilter.setResonance       (mFilterRes);

    mFilter2.prepare (spec);
    mFilter2.setCutoffFrequency (mFilter2Cutoff);
    mFilter2.setResonance       (mFilter2Res);

    // T1c notch helpers re-prepared at the new sample rate.
    mFilterNotchHelper .prepare (spec);
    mFilterNotchHelper .setCutoffFrequency (mFilterCutoff);
    mFilterNotchHelper .setResonance       (mFilterRes);
    mFilter2NotchHelper.prepare (spec);
    mFilter2NotchHelper.setCutoffFrequency (mFilter2Cutoff);
    mFilter2NotchHelper.setResonance       (mFilter2Res);
    invalidateFilterCoeffCache();

    // Filter-cutoff control tick, expressed as a sample count so the update
    // period is the same wall-clock slice at every sample rate.  2 kHz is the
    // calibration point: fast enough that a 5 ms filter attack still resolves
    // into ~10 steps, cheap enough to afford per voice.
    constexpr double kFilterControlRateHz = 2000.0;
    mFltCtrlInterval  = juce::jmax (1, (int) std::lround (mSampleRate / kFilterControlRateHz));
    mFltCtrlCountdown = 0;

    // juce::ADSR recalculates its per-sample rates only inside setParameters,
    // so setSampleRate alone would leave every envelope running at the OLD rate
    // until the next retriggering note-on - which in legato may never come.
    // Re-applying the LIVE parameters (not the stored defaults) preserves the
    // per-note CC72 release scale and cutFast's quick-release override.
    const auto ampP  = mAmpADSR .getParameters();
    const auto fltP1 = mFltADSR1.getParameters();
    const auto fltP2 = mFltADSR2.getParameters();
    mAmpADSR .setSampleRate (mSampleRate);  mAmpADSR .setParameters (ampP);
    mFltADSR1.setSampleRate (mSampleRate);  mFltADSR1.setParameters (fltP1);
    mFltADSR2.setSampleRate (mSampleRate);  mFltADSR2.setParameters (fltP2);

    // S4 Option 2: pre-allocate per-voice engines on the UI thread so the
    // audio thread never allocates. Idle until the mod matrix (Batch 2b)
    // flips mUseVoiceEngines for a given note.
    if (! mVoiceEngineA) mVoiceEngineA = std::make_unique<HarmonicEngine>();
    if (! mVoiceEngineB) mVoiceEngineB = std::make_unique<HarmonicEngine>();
}

//==============================================================================
bool AdditiveVoice::canPlaySound (juce::SynthesiserSound* s)
{
    return dynamic_cast<SynthSound*> (s) != nullptr;
}

void AdditiveVoice::startNote (int midiNote, float velocity,
                                juce::SynthesiserSound*, int pitchWheelPos)
{
    mPitchWheelSemis = float (pitchWheelPos - 8192) / 8192.0f * 2.0f;
    mNoteHz          = midiNoteToHz (midiNote, mPitchWheelSemis);
    mTargetHz        = mNoteHz;

    const bool wasActive = isVoiceActive();
    // Bug-L1 (2026-04-19): "sustained" means held + not yet released. If the
    // previous note is in release tail, treat this incoming note as a fresh
    // trigger - otherwise the ADSR stays stuck in release and the new note
    // plays silent. Symptom: clicking LEGATO ON, the held note finishes its
    // release naturally, then every subsequent noteOn produced silence until
    // legato was clicked off.
    const bool wasSustained = wasActive && ! mInRelease;

    // Legato: if the voice was sustaining (held) and glide is enabled, let the
    // frequency smooth naturally from the current pitch to the new target.
    // Otherwise snap to the new pitch immediately.
    if (!mLegatoMode || !wasSustained || mGlideTimeSec < 0.001f)
        mCurrentHz = mNoteHz;   // instant snap (no glide)
    // else: mCurrentHz retains its current value → portamento glide to mTargetHz

    // QA-H per-note glide (CC84-armed slide/porta) overrides both paths: the
    // voice starts AT the source pitch and smooths to the target.  Slide
    // carries its own time (CC5/37 = the note's length); porta uses the
    // engine glide param, falling back to 60 ms when it is 0.
    mPerNoteGlideActive = false;
    mPanRampLeft        = 0;   // #11: a fresh note is not mid-pan-ramp (RT arm below may re-set)
    if (mGlideFromNote >= 0)
    {
        mCurrentHz = midiNoteToHz (mGlideFromNote, mPitchWheelSemis);
        const float tSec = mGlideTimePending
            ? (float) ((mGlideTimeMsbMs << 7) | mGlideTimeLsbMs) * 0.001f
            : (mGlideTimeSec > 0.001f ? mGlideTimeSec : 0.06f);
        // tau = t/3 -> ~95% of the sweep lands inside the requested time.
        mPerNoteGlideCoeff  = float (std::exp (-3.0 / (juce::jmax (0.005f, tSec) * mSampleRate)));
        mPerNoteGlideActive = (mCurrentHz != mNoteHz);
        // #11 (G-4): an RT glide noteOn carries pan as a CC89 ramp target --
        // mNotePan (the channel snapshot this voice already tracks) glides to
        // it over the same span as the pitch (no CC10 was emitted, so the
        // ramp starts from the glide SOURCE note's pan).
        if (mPanRampPend > -2.0f)
        {
            const float glideSamples = juce::jmax (0.005f, tSec) * (float) mSampleRate;
            if (glideSamples > 1.0f)
            {
                mPanRampTarget = mPanRampPend;
                mPanRampLeft   = (int) glideSamples;
                mPanRampStep   = (mPanRampTarget - mNotePan) / (float) mPanRampLeft;
            }
            else mNotePan = mPanRampPend;
        }
    }
    mGlideFromNote    = -1;      // one-shot: never leaks into the next note
    mGlideTimePending = false;
    mPanRampPend      = -999.0f; // #11: consumed above (or irrelevant for a plain note)

    // Consume the per-note expression stashes (QA-H).
    mActiveResOffset = mPendResOffset;
    mActiveRelScale  = mPendRelScale;

    // S3 T2-N: vel_link refactor. mVolSmooth no longer pre-multiplies velocity;
    // we store velocity separately and apply per-part in the render loop based
    // on mVelLink (ON: both A+B scale with velocity; OFF: only A scales).
    mNoteVelocity = velocity;
    mVelRampLeft  = 0;   // S-6(C): a fresh note is not mid-slide-ramp
    mVolSmooth.setCurrentAndTargetValue (mVolume);
    mPartASmooth.setCurrentAndTargetValue (mPartALevel);
    mPartBSmooth.setCurrentAndTargetValue (mPartBLevel);

    // Legato: skip envelope retrigger only if the previous note is still
    // sustained. If it had been released, retrigger the envelope normally.
    const bool retrigger = !mLegatoMode || !wasSustained;

    if (retrigger)
    {
        // T1g: use per-voice mRng (seeded once in ctor on message thread)
        // instead of constructing a juce::Random on the audio thread, which
        // calls Time::getHighResolutionTicks() = syscall.
        // S3 T2-C: unison_phase distributes per-slot phase staggers evenly
        // across the wavetable. amount=0 -> all slots use same fixedPos+rand;
        // amount=1 -> slots evenly spread across [0, kFFTSize).
        const float fixedPos = mPhaseStart * float (HarmonicEngine::kFFTSize);
        const float wtF = float (HarmonicEngine::kFFTSize);
        for (int u = 0; u < mNumUnison; ++u)
        {
            const float rndA = mRng.nextFloat() * wtF;
            const float rndB = mRng.nextFloat() * wtF;
            const float baseA = fixedPos + mPhaseRand * (rndA - fixedPos);
            const float baseB = fixedPos + mPhaseRand * (rndB - fixedPos);
            const float slotStaggerPos = (mNumUnison > 1)
                                       ? (float (u) / float (mNumUnison)) * wtF
                                       : 0.0f;
            mPhaseA[u] = baseA + mUnisonPhaseAmount * slotStaggerPos;
            mPhaseB[u] = baseB + mUnisonPhaseAmount * slotStaggerPos;
            // Wrap into [0, wtF)
            while (mPhaseA[u] >= wtF) mPhaseA[u] -= wtF;
            while (mPhaseB[u] >= wtF) mPhaseB[u] -= wtF;
        }

        // Reset LFO phases on retrigger.
        mTremPhase  = 0.0f;
        mVibPhase   = 0.0f;
        mVibEnvPos  = 0.0f;

        mAmpADSR.setSampleRate (mSampleRate);
        auto scaled = mAmpParams;   // QA-H: per-note CC72 release scale
        scaled.release = juce::jmax (0.001f, scaled.release * mActiveRelScale);
        mAmpADSR.setParameters (scaled);
        mAmpADSR.noteOn();
        // S2 T2-A: filter envelopes also retrigger on fresh noteOn.
        mFltADSR1.setSampleRate (mSampleRate);
        mFltADSR1.setParameters (mFltParams1);
        mFltADSR1.noteOn();
        mFltADSR2.setSampleRate (mSampleRate);
        mFltADSR2.setParameters (mFltParams2);
        mFltADSR2.noteOn();
        // Force a cutoff tick on the note's first sample so the attack starts
        // from the retriggered envelope value, not the previous note's tail.
        mFltCtrlCountdown = 0;
        mFilter.reset();
        mFilter2.reset();
        mFilterNotchHelper .reset();   // T1c

        // S4 Batch 2b: per-voice mod-matrix note-on capture.
        mModGateHeld = true;
        if (mModRegistry != nullptr)
        {
            mModSnapGen = mModRegistry->getSnapshotGeneration();
            const float vel01 = juce::jlimit (0.f, 1.f, velocity);
            const float key01 = juce::jlimit (0.f, 1.f, float (midiNote) / 127.0f);
            for (auto& t : mTargets)
            {
                t.envPhase    = 0.0f;
                t.lfoPhase    = 0.0f;
                t.capturedVel = vel01;
                t.capturedKey = key01;
                t.contribution = 0.0f;
            }
            refreshModActiveFlags();
            // Also reset the VoiceEngine contribution cache so the voice engines
            // sync fresh on the first block after note-on (if any mod is active).
            mLastPluckContrib = 0.0f;
            mLastPrismContrib = 0.0f;
            mLastBlurContrib  = 0.0f;
        }
        mFilter2NotchHelper.reset();
        mSubPhase = 0.0f;              // T2-F sub osc phase reset on note start
        mInRelease = false;            // Bug-L1: fresh trigger -> not in release
    }
}

void AdditiveVoice::stopNote (float /*velocity*/, bool allowTailOff)
{
    // S4: mod gate flips to released. If a mod envelope has not reached the
    // end of its curve, the remainder advances over the voice's amp-ADSR
    // release time, so short notes still fade through the unplayed segment.
    mModGateHeld = false;

    if (allowTailOff)
    {
        mAmpADSR.noteOff();
        // S2 T2-A: filter envelopes also enter release.
        mFltADSR1.noteOff();
        mFltADSR2.noteOff();
        mInRelease = true;   // Bug-L1: voice is now in release tail
    }
    else
    {
        mAmpADSR.reset();
        mFltADSR1.reset();
        mFltADSR2.reset();
        mInRelease = false;
        clearCurrentNote();
    }
}

void AdditiveVoice::cutFast() noexcept
{
    if (! isVoiceActive())
        return;

    // Instant click-free hard cut: override the amp release to ~1.5 ms and release.
    // The voice fades over the quick-release and auto-retires when mAmpADSR goes
    // idle (renderNextBlock clears it).  startNote re-applies mAmpParams, so the
    // override never leaks into the next note on this voice.
    auto quick = mAmpParams;
    quick.release = 0.0015f;   // ~1.5 ms (matches the BaySickPlayer voice-steal fade)
    mAmpADSR.setParameters (quick);
    mAmpADSR.noteOff();
    mFltADSR1.noteOff();
    mFltADSR2.noteOff();
    mInRelease = true;
}

void AdditiveVoice::pitchWheelMoved (int newValue)
{
    mPitchWheelSemis = float (newValue - 8192) / 8192.0f * 2.0f;
    if (isVoiceActive())
        mTargetHz = midiNoteToHz (getCurrentlyPlayingNote(), mPitchWheelSemis);
}

void AdditiveVoice::controllerMoved (int controllerNumber, int newValue)
{
    // Batch E #2 (2026-05-01): CC 74 (Brightness) = per-note filter cutoff
    // offset.  Map 0..127 -> -2..+2 octaves.  Applied multiplicatively to
    // both filter cutoffs on the filter-cutoff control tick (mFltCtrlInterval).
    if (controllerNumber == 10)       // S-7: per-note pan (64 = center)
    {
        // Smoke #19 declick: sounding voices glide to the new channel pan
        // over ~8 ms (instant set = gain step = click on ringing voices);
        // idle voices set instantly for exact startNote state.
        const float p = juce::jlimit (-1.0f, 1.0f, ((float) newValue - 64.0f) / 63.0f);
        if (isVoiceActive())
        {
            mPanRampTarget = p;
            mPanRampLeft   = juce::jmax (1, (int) (0.008f * mSampleRate));
            mPanRampStep   = (p - mNotePan) / (float) mPanRampLeft;
        }
        else mNotePan = p;
    }
    else if (controllerNumber == 74)
    {
        const float norm = (float) newValue / 127.0f;          // 0..1
        mPerNoteCutoffOctaves = (norm - 0.5f) * 4.0f;          // -2..+2
    }
    else if (controllerNumber == 71)  // per-note resonance offset (QA-H)
        mPendResOffset = ((float) newValue / 127.0f - 0.5f) * 2.0f;   // -1..+1 Q
    else if (controllerNumber == 72)  // per-note release scale (QA-H)
        mPendRelScale = std::pow (2.0f, ((float) newValue / 127.0f - 0.5f) * 4.0f);
    else if (controllerNumber == 84)  // glide source note (QA-H slide/porta)
        mGlideFromNote = newValue;
    else if (controllerNumber == 86)  // S-6(C): RampSlide target loudness (for CC85)
        mSlideTargetVel = (float) newValue / 127.0f;
    else if (controllerNumber == 89)  // #11 (G-4): pan RAMP target (for CC85 / RT noteOn)
        mPanRampPend = juce::jlimit (-1.0f, 1.0f, ((float) newValue - 64.0f) / 63.0f);
    else if (controllerNumber == 5)   { mGlideTimeMsbMs = newValue; mGlideTimePending = true; }
    else if (controllerNumber == 37)  { mGlideTimeLsbMs = newValue; mGlideTimePending = true; }
    // CC85 no longer lands here -- BroadcastSynthesiser dispatches it via
    // tryRampTakeover (#36 first-match) and then wipes every voice's stash.
}

bool AdditiveVoice::tryRampTakeover (int targetNote)
{
    // Takeover semantics - no retrigger.  Only the voice whose (juce) playing
    // note matches the CC84 anchor bends; #36: the synth stops at the FIRST
    // acceptor so two voices holding the same note can't both retarget.
    if (! (isVoiceActive() && ! mInRelease
           && getCurrentlyPlayingNote() == mGlideFromNote))
        return false;

    // mCurrentHz keeps its value; moving mNoteHz re-aims the
    // per-sample target and the per-note coefficient carries the bend.
    mNoteHz   = midiNoteToHz (targetNote, mPitchWheelSemis);
    mTargetHz = mNoteHz;
    const float tSec = mGlideTimePending
        ? (float) ((mGlideTimeMsbMs << 7) | mGlideTimeLsbMs) * 0.001f
        : 0.06f;
    mPerNoteGlideCoeff  = float (std::exp (-3.0 / (juce::jmax (0.005f, tSec) * mSampleRate)));
    mPerNoteGlideActive = true;
    // S-6(C): ramp loudness base -> slide over the glide (parallels the bend).
    const float glideSamples = juce::jmax (0.005f, tSec) * (float) mSampleRate;
    if (mSlideTargetVel >= 0.0f && glideSamples > 1.0f)
    {
        mVelRampTarget = mSlideTargetVel;
        mVelRampLeft   = (int) glideSamples;
        mVelRampStep   = (mVelRampTarget - mNoteVelocity) / (float) mVelRampLeft;
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
void AdditiveVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                      int startSample, int numSamples)
{
    if (!isVoiceActive())
        return;

    if (!mAmpADSR.isActive())
    {
        clearCurrentNote();
        return;
    }

    // S4 Batch 2b: advance + sample per-target mod curves once per block.
    // Also triggers a voice-engine wavetable rebuild if VoiceEngine contributions
    // moved. Both cheap when all targets inactive (the common case).
    updateModContributions (numSamples);

    const auto* engA = activeEngineA();
    const auto* engB = activeEngineB();
    const float* wtA = (engA && engA->wavetableReady())
                       ? engA->getWavetable() : nullptr;
    const float* wtB = (engB && engB->wavetableReady())
                       ? engB->getWavetable() : nullptr;

    if (!wtA && !wtB)
        return;

    constexpr int   wtSz    = HarmonicEngine::kFFTSize;
    constexpr float wtSzF   = float (wtSz);
    constexpr int   wtMask  = wtSz - 1;   // valid bitmask: kFFTSize is always a power of 2

    float* outL = outputBuffer.getWritePointer (0, startSample);
    float* outR = (outputBuffer.getNumChannels() > 1)
                  ? outputBuffer.getWritePointer (1, startSample) : nullptr;

    // 1/sqrt(N), not 1/N (Jeff, 2026-08-10): detuned unison voices are
    // INCOHERENT, so their powers add rather than their amplitudes - dividing by
    // the count made Harmless get quieter every time a voice was added, while
    // BaySickSynth/Bass already used the power-correct law.  That mismatch is
    // most of why the two engines never sat at comparable levels.
    const float uniNorm  = 1.0f / std::sqrt (float (mNumUnison));
    // Precompute the constant factor - avoids a float division per unison slot per sample.
    const float invSrWt  = wtSzF / float (mSampleRate);

    // Null checks are hoisted outside both loops - the pointers never change mid-block.
    const bool hasA = (wtA != nullptr);
    const bool hasB = (wtB != nullptr);

    // Choose the frequency smoothing coefficient:
    // per-note glide (QA-H slide/porta) wins while its sweep is in flight
    // (the land-check in the sample loop hands back to normal selection),
    // then glide coefficient when portamento is active, else the fast snap.
    const float freqCoeff = mPerNoteGlideActive ? mPerNoteGlideCoeff
                          : (mGlideTimeSec > 0.001f) ? mGlideCoeff : mFreqCoeff;

    // S2 T2-A + SLA #34 + T2-N: filter cutoff.
    // Effective cutoff = baseCutoff * 2^((envAmt*envValue*48 + cutoffOfs +
    //                                     kbTrack*noteSemisAbove60 + ...) / 12)
    // The 4-octave (48-semitone) swing on full env scaling is the standard
    // synth-filter range. T2-E mod XYZ destination FilterCutoff (#1) adds
    // another +-2 octaves of modulation summed in.
    // Everything but the envelope term is constant over the block; both filter
    // envelopes advance once per SAMPLE inside the loop and the cutoff is
    // re-derived on the mFltCtrlInterval tick.

    // S4 Batch 2b: VoiceLocal mod contributions for filter cutoff + resonance.
    // Pitch contribution added later where pitchOffsetSemis is computed.
    const float modFlt1Semis = mTargets[(size_t) ModTargetIndex::Filter1Cutoff].contribution * 24.0f; // ±2 octaves
    const float modFlt2Semis = mTargets[(size_t) ModTargetIndex::Filter2Cutoff].contribution * 24.0f;
    const float modFlt1Res   = mTargets[(size_t) ModTargetIndex::Filter1Res]   .contribution * 0.5f;   // ±0.5 Q
    const float modFlt2Res   = mTargets[(size_t) ModTargetIndex::Filter2Res]   .contribution * 0.5f;

    // Note pitch in semitones above middle (60). For kb_track scaling.
    const float noteSemisAbove60 = juce::jlimit (-60.f, 60.f,
        12.0f * std::log2 (juce::jmax (1.0f, mNoteHz) / 261.6256f));

    // Batch E #2: per-note CC74 cutoff offset, -2..+2 octaves = -24..+24 semis.
    const float perNoteSemis = mPerNoteCutoffOctaves * 12.0f;
    const float flt1StaticSemis = mFlt1CutoffOfs
                                + mFlt1KbTrack * noteSemisAbove60
                                + modFlt1Semis
                                + perNoteSemis;
    const float flt2StaticSemis = mFlt2CutoffOfs
                                + mFlt2KbTrack * noteSemisAbove60
                                + modFlt2Semis
                                + perNoteSemis;
    const float flt1EnvSemisPerUnit = mFlt1EnvAmt * 48.0f;
    const float flt2EnvSemisPerUnit = mFlt2EnvAmt * 48.0f;

    // Control tick: re-derive both cutoffs from the live envelope values.
    // Guarded so a held (sustaining or zero-amount) envelope costs no
    // coefficient recalculation - StateVariableTPTFilter::setCutoffFrequency
    // runs a tan() per call.
    auto applyCutoffTick = [&] (float env1, float env2)
    {
        const float c1 = juce::jlimit (20.f, 20000.f, mFilterCutoff
            * std::pow (2.0f, (flt1StaticSemis + flt1EnvSemisPerUnit * env1) / 12.0f));
        const float c2 = juce::jlimit (20.f, 20000.f, mFilter2Cutoff
            * std::pow (2.0f, (flt2StaticSemis + flt2EnvSemisPerUnit * env2) / 12.0f));
        if (c1 != mAppliedCutoff1)
        {
            mAppliedCutoff1 = c1;
            if (mFilterType != 3)
                mFilter.setCutoffFrequency (c1);
            mFilterNotchHelper.setCutoffFrequency (c1);
        }
        if (c2 != mAppliedCutoff2)
        {
            mAppliedCutoff2 = c2;
            if (mFilter2Type != 3)
                mFilter2.setCutoffFrequency (c2);
            mFilter2NotchHelper.setCutoffFrequency (c2);
        }
    };

    // S4 Batch 2b: resonance mod on top of the base value.
    // QA-H: per-note CC71 offset (+-1 Q) applies to both filters.
    // No envelope feeds resonance, so it stays a block-rate update.
    const float effRes1 = juce::jlimit (0.05f, 10.0f, mFilterRes  + modFlt1Res + mActiveResOffset);
    const float effRes2 = juce::jlimit (0.05f, 10.0f, mFilter2Res + modFlt2Res + mActiveResOffset);
    if (effRes1 != mAppliedRes1)
    {
        mAppliedRes1 = effRes1;
        if (mFilterType != 3)
            mFilter.setResonance (effRes1);
        mFilterNotchHelper.setResonance (effRes1);
    }
    if (effRes2 != mAppliedRes2)
    {
        mAppliedRes2 = effRes2;
        if (mFilter2Type != 3)
            mFilter2.setResonance (effRes2);
        mFilter2NotchHelper.setResonance (effRes2);
    }

    // S4 Batch 2b: VoiceLocal mod dispatch.
    //   Bipolar (`contribution`, centered on 0 = no change):
    //     Pitch (±12 semis), Pan (±1), TimbreBlend (±1).
    //   Unipolar (`uniMult`, centered on 1.0 = no change, 0 = silent, 2 = 2x):
    //     Volume, PartA Level, PartB Level. Multi-source composition uses
    //     updateModContributions's `blendUni` product rule.
    const float modPitchSemis  = mTargets[(size_t) ModTargetIndex::Pitch]      .contribution * 12.0f;
    const float modPan         = mTargets[(size_t) ModTargetIndex::Pan]        .contribution;
    const auto  panLaw         = baysick::pan::currentLaw();
    const float modTimbreBlend = mTargets[(size_t) ModTargetIndex::TimbreBlend].contribution;
    const float modVolMult     = mTargets[(size_t) ModTargetIndex::Volume]    .uniMult;
    const float modPartAMul    = mTargets[(size_t) ModTargetIndex::PartALevel].uniMult;
    const float modPartBMul    = mTargets[(size_t) ModTargetIndex::PartBLevel].uniMult;
    // Unison pitch thickness mod: scales each slot's detune deviation from
    // 1.0 by 2^(contribution/1). -1 = minimum spread, +1 = doubled spread.
    const float unisonThicknessScale = std::pow (2.0f,
        juce::jlimit (-1.f, 1.f, mTargets[(size_t) ModTargetIndex::UnisonPitch].contribution));

    for (int s = 0; s < numSamples; ++s)
    {
        // Filter envelopes advance exactly one sample per sample, so their
        // wall-clock length is the user's set time at any buffer size.  The
        // derived cutoff is applied on the sample-counted control tick; the
        // countdown carries over from the previous block, so a short final
        // block or a device change cannot shift the control rate.
        const float fltEnv1 = mFltADSR1.getNextSample();
        const float fltEnv2 = mFltADSR2.getNextSample();
        if (--mFltCtrlCountdown <= 0)
        {
            mFltCtrlCountdown = mFltCtrlInterval;
            applyCutoffTick (fltEnv1, fltEnv2);
        }

        // ── Vibrato - pitch modulation (before frequency computation) ──────────
        float vibSemis = 0.0f;
        if (mVibDepth > 0.001f)
        {
            const float envRampRate = 1.0f / (0.001f + mVibEnv * float (mSampleRate));
            mVibEnvPos = juce::jmin (1.0f, mVibEnvPos + envRampRate);
            const float vibLFO = lfoSample (mVibShape, mVibPhase);
            vibSemis = vibLFO * mVibDepth * mVibEnvPos;
            mVibPhase += juce::MathConstants<float>::twoPi * mVibSpeed / float (mSampleRate);
            if (mVibPhase >= juce::MathConstants<float>::twoPi)
                mVibPhase -= juce::MathConstants<float>::twoPi;
        }

        // Apply pitch offset + vibrato to target Hz.
        // mTargetHz is set from mNoteHz + pitch wheel in startNote/pitchWheelMoved.
        // Per-sample, we compute the full pitch including offset + vibrato +
        // S4 mod-matrix pitch contribution (modPitchSemis, stubbed to 0 in B1).
        const float pitchOffsetSemis = mPitchSemitones + mPitchCents / 100.0f
                                      + vibSemis + modPitchSemis;
        // SLA-Impl #19: fraction multiplier applied first (fundamental scale),
        // then semitone+cents offsets on top.
        const float targetWithPitch  = mNoteHz * mPitchFracMult
                                      * std::pow (2.0f, pitchOffsetSemis / 12.0f);

        // One-pole IIR frequency smoothing - safe for portamento since it
        // never resets the current value unlike juce::SmoothedValue::reset().
        mCurrentHz += (targetWithPitch - mCurrentHz) * (1.0f - freqCoeff);
        // QA-H per-note glide landed (~3 cents of the live target): flag off
        // so the next block returns to the normal coefficient selection.
        if (mPerNoteGlideActive
            && std::abs (mCurrentHz - targetWithPitch) < 0.002f * targetWithPitch)
            mPerNoteGlideActive = false;

        const float envGain = mAmpADSR.getNextSample();
        const float vol     = mVolSmooth  .getNextValue();
        // S-6(C): advance the RampSlide loudness ramp - mNoteVelocity drives the
        // per-part gain below, so loudness interpolates base -> slide over the glide.
        if (mVelRampLeft > 0)
        {
            mNoteVelocity += mVelRampStep;
            if (--mVelRampLeft == 0) mNoteVelocity = mVelRampTarget;
        }
        // #11 (G-4): pan ramp (CC89 target) -- same shape as the vel ramp.
        if (mPanRampLeft > 0)
        {
            mNotePan += mPanRampStep;
            if (--mPanRampLeft == 0) mNotePan = mPanRampTarget;
        }
        // S3 T2-N: vel_link gates whether Part B is velocity-scaled.
        // Always: Part A scales with velocity. vel_link ON: Part B too.
        const float velA    = mNoteVelocity;
        const float velB    = mVelLink ? mNoteVelocity : 1.0f;
        // S4 Batch 2b: timbre-blend + per-part level mod.
        // modTimbreBlend in [-1,+1]: positive pushes toward Part B, negative Part A.
        const float blendPushB = juce::jlimit (0.f, 1.f,  modTimbreBlend);
        const float blendPushA = juce::jlimit (0.f, 1.f, -modTimbreBlend);
        const float pA      = mPartASmooth.getNextValue() * velA * modPartAMul * (1.0f - blendPushB);
        const float pB      = mPartBSmooth.getNextValue() * velB * modPartBMul * (1.0f - blendPushA);

        // Partial culling: skip wavetable reads when a part's level is below
        // audibility. Phases still advance so continuity is preserved on fade-in.
        const bool readA = hasA && (pA > 1.0e-4f);
        const bool readB = hasB && (pB > 1.0e-4f);

        float sampleL = 0.0f;
        float sampleR = 0.0f;

        for (int u = 0; u < mNumUnison; ++u)
        {
            // S4 Batch 2b: Unison Pitch target scales each slot's deviation
            // from 1.0 (= center pitch) by unisonThicknessScale.
            const float rawDev  = mUnisonMult[u] - 1.0f;
            const float scaled  = 1.0f + rawDev * unisonThicknessScale;
            const float inc = mCurrentHz * scaled * invSrWt;

            const float sa = readA ? readWavetable (wtA, wtMask, mPhaseA[u]) : 0.0f;
            const float sb = readB ? readWavetable (wtB, wtMask, mPhaseB[u]) : 0.0f;
            const float sample = sa * pA + sb * pB;

            sampleL += sample * mPanL[u];
            sampleR += sample * mPanR[u];

            mPhaseA[u] += inc;
            if (mPhaseA[u] >= wtSzF) mPhaseA[u] -= wtSzF;

            mPhaseB[u] += inc;
            if (mPhaseB[u] >= wtSzF) mPhaseB[u] -= wtSzF;
        }

        sampleL *= uniNorm;
        sampleR *= uniNorm;

        // T2-F rm_sub: add a sine partial one octave below the fundamental.
        if (mSubOscGain > 1.0e-4f)
        {
            const float subInc = mCurrentHz * 0.5f * juce::MathConstants<float>::twoPi
                               / float (mSampleRate);
            const float subSample = std::sin (mSubPhase) * mSubOscGain;
            mSubPhase += subInc;
            if (mSubPhase >= juce::MathConstants<float>::twoPi)
                mSubPhase -= juce::MathConstants<float>::twoPi;
            sampleL += subSample;
            sampleR += subSample;
        }

        // T1c filter type dispatch (Filter 1).
        // 0 LP, 1 HP, 2 BP - all native to StateVariableTPTFilter via setType.
        // 3 Notch - implemented as input - bandpass(input) using mFilterNotchHelper.
        if (mFilterType == 3)
        {
            const float bpL = mFilterNotchHelper.processSample (0, sampleL);
            const float bpR = mFilterNotchHelper.processSample (1, sampleR);
            sampleL -= bpL;
            sampleR -= bpR;
        }
        else
        {
            sampleL = mFilter.processSample (0, sampleL);
            sampleR = mFilter.processSample (1, sampleR);
        }

        // T1c Filter 2 type dispatch (mirrors Filter 1 but with mFilter2NotchHelper).
        if (mFilter2Type == 3)
        {
            const float bpL = mFilter2NotchHelper.processSample (0, sampleL);
            const float bpR = mFilter2NotchHelper.processSample (1, sampleR);
            sampleL -= bpL;
            sampleR -= bpR;
        }
        else
        {
            sampleL = mFilter2.processSample (0, sampleL);
            sampleR = mFilter2.processSample (1, sampleR);
        }

        // ── Tremolo - amplitude modulation (after ADSR) ────────────────────────
        float tremMult = 1.0f;
        if (mTremDepth > 0.001f)
        {
            const float tremLFO = lfoSample (mTremShape, mTremPhase);
            // gap: dead zone around 0 - if |lfo| < gap treat as 0
            const float gapped = (std::abs (tremLFO) > mTremGap) ? tremLFO : 0.0f;
            tremMult = 1.0f - mTremDepth * 0.5f * (gapped + 1.0f);
            tremMult = juce::jlimit (0.0f, 1.0f, tremMult);
            mTremPhase += juce::MathConstants<float>::twoPi * mTremSpeed / float (mSampleRate);
            if (mTremPhase >= juce::MathConstants<float>::twoPi)
                mTremPhase -= juce::MathConstants<float>::twoPi;
        }

        // T2-F rm_env: blend between full envelope (depth=1) and constant
        // amplitude (depth=0). When depth=0 the envelope still gates the voice
        // active state but its instantaneous value is replaced by 1.0.
        const float envEffective = mEnvDepth * envGain + (1.0f - mEnvDepth);
        // S4 Batch 2b: mod-matrix contributions for volume + pan. modVolMult
        // is a unipolar envelope (1.0 = unchanged); modPan bipolar.
        const float gain = envEffective * vol * tremMult * modVolMult;
        // Master pan + note pan (CC10) + pan modulation as ONE position through
        // the project law; the cache only recomputes when the position moves
        // (a ramp or a modulator), never for a static note.
        const auto& pg = mPanGains.get (mMasterPan + mNotePan + modPan, panLaw);
        outL[s] += sampleL * pg.l * gain;
        // T1h: skip stereo write entirely if mono path (caller never allocated R).
        if (outR) outR[s] += sampleR * pg.r * gain;
    }

    if (!mAmpADSR.isActive())
        clearCurrentNote();
}

//==============================================================================
void AdditiveVoice::setUnison (int numVoices, float detuneCents, float spread)
{
    mNumUnison   = juce::jlimit (1, kMaxUnison, numVoices);
    mDetuneCents = detuneCents;
    mSpread      = spread;
    recalcUnisonSlots();
}

void AdditiveVoice::setPartLevels (float partALevel, float partBLevel)
{
    mPartALevel = partALevel;
    mPartBLevel = partBLevel;
    mPartASmooth.setTargetValue (partALevel);
    mPartBSmooth.setTargetValue (partBLevel);
}

void AdditiveVoice::setAmpEnv (float a, float d, float s, float r)
{
    mAmpParams = { a, d, s, r };
    mAmpADSR.setParameters (mAmpParams);
}

void AdditiveVoice::setFilterParams (float cutoffHz, float resonance)
{
    const float c = juce::jlimit (20.0f,   20000.0f, cutoffHz);
    // juce::dsp::StateVariableTPTFilter: resonance = Q factor.
    // Q=0.7071 (Butterworth) is transparent. Minimum 0.1 prevents severe overdamping.
    const float r = juce::jlimit (0.1f,    1.0f,     resonance);
    if (c != mFilterCutoff || r != mFilterRes)
    {
        mFilterCutoff = c;
        mFilterRes    = r;
        // T1c: keep the notch helper in sync; mFilter mode unchanged here
        // (setFilterType handles that). Default mode set by ctor = lowpass.
        mFilter.setCutoffFrequency (c);
        mFilter.setResonance       (r);
        mFilterNotchHelper.setCutoffFrequency (c);
        mFilterNotchHelper.setResonance       (r);
        invalidateFilterCoeffCache();
    }
}

void AdditiveVoice::setVolume (float vol)
{
    mVolume = vol;
    mVolSmooth.setTargetValue (vol);
}

void AdditiveVoice::setPan (float pan)
{
    mMasterPan = juce::jlimit (-1.0f, 1.0f, pan);
}

void AdditiveVoice::setGlide (float glideTimeSec)
{
    mGlideTimeSec = juce::jmax (0.0f, glideTimeSec);
    updateGlideCoeff();
}

//==============================================================================
// T1f CPU guards added on every setter below.

void AdditiveVoice::setPhaseInit (float start, float rnd) noexcept
{
    const float s = juce::jlimit (0.f, 1.f, start);
    const float r = juce::jlimit (0.f, 1.f, rnd);
    if (s == mPhaseStart && r == mPhaseRand) return;
    mPhaseStart = s; mPhaseRand = r;
}

void AdditiveVoice::setTremoloParams (int shape, float depth, float speed, float gap) noexcept
{
    const int   sh = juce::jlimit (0, 3, shape);
    const float dp = juce::jlimit (0.f, 1.f, depth);
    const float sp = juce::jmax (0.1f, speed);
    const float gp = juce::jlimit (0.f, 1.f, gap);
    if (sh == mTremShape && dp == mTremDepth && sp == mTremSpeed && gp == mTremGap) return;
    mTremShape = sh; mTremDepth = dp; mTremSpeed = sp; mTremGap = gp;
}

void AdditiveVoice::setVibratoParams (int shape, float depth, float speed, float env) noexcept
{
    const int   sh = juce::jlimit (0, 3, shape);
    const float dp = juce::jlimit (0.f, 2.f, depth);
    const float sp = juce::jmax (0.1f, speed);
    const float en = juce::jlimit (0.f, 1.f, env);
    if (sh == mVibShape && dp == mVibDepth && sp == mVibSpeed && en == mVibEnv) return;
    mVibShape = sh; mVibDepth = dp; mVibSpeed = sp; mVibEnv = en;
}

void AdditiveVoice::setFilter2Params (float cutoffHz, float res) noexcept
{
    const float c = juce::jlimit (20.f, 20000.f, cutoffHz);
    const float r = juce::jlimit (0.1f,  1.f,   res);
    if (c != mFilter2Cutoff || r != mFilter2Res)
    {
        mFilter2Cutoff = c;
        mFilter2Res    = r;
        // T1c: respect the user-selected filter type instead of forcing lowpass.
        // Notch type uses mFilter2NotchHelper externally - keep the helper synced.
        if (mFilter2Type != 3)
        {
            using SVF = juce::dsp::StateVariableTPTFilterType;
            const SVF mode = (mFilter2Type == 1) ? SVF::highpass
                           : (mFilter2Type == 2) ? SVF::bandpass
                                                 : SVF::lowpass;
            mFilter2.setType (mode);
            mFilter2.setCutoffFrequency (c);
            mFilter2.setResonance (r);
        }
        mFilter2NotchHelper.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        mFilter2NotchHelper.setCutoffFrequency (c);
        mFilter2NotchHelper.setResonance (r);
        invalidateFilterCoeffCache();
    }
}

void AdditiveVoice::setPitchOffset (float semitones, float cents) noexcept
{
    const float st = juce::jlimit (-24.f, 24.f, semitones);
    const float ct = juce::jlimit (-100.f, 100.f, cents);
    if (st == mPitchSemitones && ct == mPitchCents) return;
    mPitchSemitones = st; mPitchCents = ct;
}

// ── 2026-04-19 (S1) newly-wired ghost params + routing matrix hooks ──────────

void AdditiveVoice::setTimbreBlend (float blend) noexcept
{
    const float b = juce::jlimit (0.f, 1.f, blend);
    if (b == mTimbreBlend) return;
    mTimbreBlend = b;
    // Re-broadcast smoothed targets with blend applied on top of the per-part
    // levels. partA effective = partA * (1 - blend); partB effective = partB
    // * blend + partA * blend (so blend=0 leaves levels exactly as set).
    const float aEff = mPartALevel * (1.0f - mTimbreBlend);
    const float bEff = mPartBLevel * (1.0f) + mPartALevel * mTimbreBlend;
    mPartASmooth.setTargetValue (aEff);
    mPartBSmooth.setTargetValue (bEff);
}

void AdditiveVoice::setSubOscGain (float gain) noexcept
{
    const float g = juce::jlimit (0.f, 1.f, gain);
    if (g == mSubOscGain) return;
    mSubOscGain = g;
}

void AdditiveVoice::setEnvDepth (float depth) noexcept
{
    const float d = juce::jlimit (0.f, 1.f, depth);
    if (d == mEnvDepth) return;
    mEnvDepth = d;
}

void AdditiveVoice::setFilterType (int type) noexcept
{
    const int t = juce::jlimit (0, 3, type);
    if (t == mFilterType) return;
    mFilterType = t;
    // Update mFilter's mode in-place. Notch (t=3) uses BP-subtract via the
    // helper - mFilter is unused in that branch, so we don't need to touch it.
    if (t != 3)
    {
        using SVF = juce::dsp::StateVariableTPTFilterType;
        const SVF mode = (t == 1) ? SVF::highpass
                       : (t == 2) ? SVF::bandpass
                                  : SVF::lowpass;
        mFilter.setType (mode);
    }
    // Always keep helper synced so toggling to/from notch produces no click.
    mFilterNotchHelper.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    mFilterNotchHelper.setCutoffFrequency (mFilterCutoff);
    mFilterNotchHelper.setResonance (mFilterRes);
    invalidateFilterCoeffCache();
}

// ── 2026-04-19 (S2 T2-A) Filter envelopes / cutoff ofs / kb track ──────────
void AdditiveVoice::setFilter1Env (float a, float d, float s, float r) noexcept
{
    juce::ADSR::Parameters p { a, d, s, r };
    if (p.attack == mFltParams1.attack && p.decay == mFltParams1.decay
     && p.sustain == mFltParams1.sustain && p.release == mFltParams1.release) return;
    mFltParams1 = p;
    mFltADSR1.setParameters (mFltParams1);
}
void AdditiveVoice::setFilter2Env (float a, float d, float s, float r) noexcept
{
    juce::ADSR::Parameters p { a, d, s, r };
    if (p.attack == mFltParams2.attack && p.decay == mFltParams2.decay
     && p.sustain == mFltParams2.sustain && p.release == mFltParams2.release) return;
    mFltParams2 = p;
    mFltADSR2.setParameters (mFltParams2);
}
void AdditiveVoice::setFilter1EnvAmt (float amt) noexcept
{
    const float v = juce::jlimit (-1.f, 1.f, amt);
    if (v == mFlt1EnvAmt) return;
    mFlt1EnvAmt = v;
}
void AdditiveVoice::setFilter2EnvAmt (float amt) noexcept
{
    const float v = juce::jlimit (-1.f, 1.f, amt);
    if (v == mFlt2EnvAmt) return;
    mFlt2EnvAmt = v;
}
void AdditiveVoice::setFilter1CutoffOfs (float semis) noexcept
{
    const float v = juce::jlimit (-24.f, 24.f, semis);
    if (v == mFlt1CutoffOfs) return;
    mFlt1CutoffOfs = v;
}
void AdditiveVoice::setFilter2CutoffOfs (float semis) noexcept
{
    const float v = juce::jlimit (-24.f, 24.f, semis);
    if (v == mFlt2CutoffOfs) return;
    mFlt2CutoffOfs = v;
}
void AdditiveVoice::setFilter1KbTrack (float depth) noexcept
{
    const float v = juce::jlimit (0.f, 1.f, depth);
    if (v == mFlt1KbTrack) return;
    mFlt1KbTrack = v;
}
void AdditiveVoice::setFilter2KbTrack (float depth) noexcept
{
    const float v = juce::jlimit (0.f, 1.f, depth);
    if (v == mFlt2KbTrack) return;
    mFlt2KbTrack = v;
}

// ── 2026-04-19 (S4) Mod XYZ pad input ────────────────────────────────────
// S2's destination-dropdown routing has been removed; the XYZ pad value is
// held here and consumed by the HarmlessModRegistry per-target mod matrix
// in Batch 2.
void AdditiveVoice::setModXYZ (float x, float y, float z) noexcept
{
    mModX = juce::jlimit (-1.f, 1.f, x);
    mModY = juce::jlimit (-1.f, 1.f, y);
    mModZ = juce::jlimit (-1.f, 1.f, z);
}

// ── 2026-04-19 (S3 T2-C) Unison Type / Alt / Phase ───────────────────────
void AdditiveVoice::setUnisonType (int type) noexcept
{
    const int n = juce::jlimit (0, 3, type);
    if (n == mUnisonType) return;
    mUnisonType = n;
    recalcUnisonSlots();
}
void AdditiveVoice::setUnisonAlt (bool on) noexcept
{
    if (on == mUnisonAlt) return;
    mUnisonAlt = on;
    recalcUnisonSlots();
}
void AdditiveVoice::setUnisonPhase (float amt) noexcept
{
    const float v = juce::jlimit (0.f, 1.f, amt);
    if (v == mUnisonPhaseAmount) return;
    mUnisonPhaseAmount = v;
    // Phase distribution applied at startNote time, not per-block.
}

void AdditiveVoice::setPitchFraction (int idx) noexcept
{
    static constexpr float kFracTable[7] = {
        1.0f,    // 0 = 1/1 (default)
        0.5f,    // 1 = 1/2 (one octave down)
        0.25f,   // 2 = 1/4
        0.125f,  // 3 = 1/8 (three octaves down)
        2.0f,    // 4 = x2  (one octave up)
        4.0f,    // 5 = x4
        8.0f     // 6 = x8  (three octaves up)
    };
    const int n = juce::jlimit (0, 6, idx);
    const float m = kFracTable[n];
    if (m == mPitchFracMult) return;
    mPitchFracMult = m;
}

void AdditiveVoice::setFilter2Type (int type) noexcept
{
    const int t = juce::jlimit (0, 3, type);
    if (t == mFilter2Type) return;
    mFilter2Type = t;
    if (t != 3)
    {
        using SVF = juce::dsp::StateVariableTPTFilterType;
        const SVF mode = (t == 1) ? SVF::highpass
                       : (t == 2) ? SVF::bandpass
                                  : SVF::lowpass;
        mFilter2.setType (mode);
    }
    mFilter2NotchHelper.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    mFilter2NotchHelper.setCutoffFrequency (mFilter2Cutoff);
    mFilter2NotchHelper.setResonance (mFilter2Res);
    invalidateFilterCoeffCache();
}

//==============================================================================
// static
float AdditiveVoice::lfoSample (int shape, float phase) noexcept
{
    const float t = phase / juce::MathConstants<float>::twoPi; // 0-1
    switch (shape)
    {
        case 1: return 1.0f - 4.0f * std::abs (t - 0.5f);  // triangle
        case 2: return 2.0f * t - 1.0f;                    // sawtooth
        case 3: return (t < 0.5f) ? 1.0f : -1.0f;          // square
        default: return std::sin (phase);                   // sine
    }
}

//==============================================================================
void AdditiveVoice::updateGlideCoeff()
{
    if (mGlideTimeSec < 0.001f || mSampleRate <= 0.0)
        mGlideCoeff = mFreqCoeff;   // fall back to fast snap
    else
        mGlideCoeff = float (std::exp (-1.0 / (mGlideTimeSec * mSampleRate)));
}

void AdditiveVoice::recalcUnisonSlots()
{
    if (mNumUnison == 1)
    {
        mUnisonMult[0] = 1.0f;
        mPanL[0]       = 1.0f;
        mPanR[0]       = 1.0f;
        return;
    }

    // S3 T2-C: per-Type detune + alt distribution.
    //   Type 0 Pure: symmetric linear (-detune .. +detune)
    //   Type 1 Random: each slot draws random offset within +/-detune
    //   Type 2 Drifting: like Pure but each slot also offset by a static
    //                    pseudo-LFO bias (gives a more "alive" stack)
    //   Type 3 Alt-only: alternating signs without centre
    for (int u = 0; u < mNumUnison; ++u)
    {
        float t = float (u) / float (mNumUnison - 1);   // 0..1
        float bias = 0.0f;                              // multiplied by mDetuneCents

        switch (mUnisonType)
        {
            case 1: // Random
                // Use the per-voice rng so it stays consistent across recalc
                // calls within a single voice's lifetime, but still varies.
                bias = (mRng.nextFloat() * 2.0f - 1.0f);
                break;
            case 2: // Drifting
                bias = juce::jmap (t, -1.0f, 1.0f)
                     + 0.15f * std::sin (float (u) * 2.7f);   // small static drift
                break;
            case 3: // Alt-only (no centre, alternating)
                bias = ((u & 1) ? 1.0f : -1.0f) * (0.5f + 0.5f * t);
                break;
            default: // 0 Pure linear
                bias = juce::jmap (t, -1.0f, 1.0f);
                break;
        }

        // Alt button: superimpose alternating sign on top of the base bias.
        if (mUnisonAlt && (u & 1)) bias = -bias;

        const float detuneThis = bias * mDetuneCents;
        mUnisonMult[u] = std::pow (2.0f, detuneThis / 1200.0f);

        // Pan position follows the base linear t (independent of type) so
        // the stereo image stays predictable.
        const float panPos = juce::jmap (t, -mSpread, mSpread);
        const float angle  = (juce::jlimit (-1.0f, 1.0f, panPos) + 1.0f)
                             * juce::MathConstants<float>::halfPi * 0.5f;
        mPanL[u] = std::cos (angle);
        mPanR[u] = std::sin (angle);
    }
}

float AdditiveVoice::readWavetable (const float* wt, int wtMask,
                                     float phase) const noexcept
{
    // wtMask must be (power-of-2 table size - 1) - bitmask replaces expensive modulo.
    const int   i    = int (phase) & wtMask;
    const float frac = phase - float (int (phase));
    const int   i1   = (i + 1) & wtMask;
    return wt[i] + frac * (wt[i1] - wt[i]);
}

float AdditiveVoice::midiNoteToHz (int note, float extraSemitones) noexcept
{
    return 440.0f * std::pow (2.0f, (float (note) - 69.0f + extraSemitones) / 12.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// S4 Batch 2b - Per-voice mod matrix implementation
// ─────────────────────────────────────────────────────────────────────────────

float AdditiveVoice::getCurrentEnvLevel() const noexcept
{
    // juce::ADSR has no peek API; this method is only called by the synth-level
    // aggregation path (block-rate, off the inner render loop), so a proxy that
    // returns 1.0 while active and 0.0 otherwise is acceptable. Voice steals
    // are correctly handled because isActive goes false on voice end.
    return mAmpADSR.isActive() ? 1.0f : 0.0f;
}

void AdditiveVoice::accumulatePartialAmplitudes (float* out, int numPartials) const noexcept
{
    if (! mAmpADSR.isActive()) return;
    const auto* engA = activeEngineA();
    const auto* engB = activeEngineB();
    const float env  = 1.0f;   // isActive proxy (no peek API on juce::ADSR)
    const float lvlA = mPartALevel * env;
    const float lvlB = mPartBLevel * env;
    const int n = juce::jmin (numPartials, HarmonicEngine::kNumPartials);
    for (int i = 0; i < n; ++i)
    {
        const float a = engA ? engA->amplitudes[(size_t) i] : 0.0f;
        const float b = engB ? engB->amplitudes[(size_t) i] : 0.0f;
        out[i] += lvlA * a + lvlB * b;
    }
}

float AdditiveVoice::getTargetContribution (int targetIndex) const noexcept
{
    if (targetIndex < 0 || targetIndex >= (int) mTargets.size())
        return 0.0f;
    return mTargets[(size_t) targetIndex].contribution;
}

void AdditiveVoice::refreshModActiveFlags() noexcept
{
    if (mModRegistry == nullptr) return;
    const auto& all = mModRegistry->getAllTargets();
    const int n    = juce::jmin ((int) all.size(), (int) mTargets.size());
    for (int ti = 0; ti < (int) mTargets.size(); ++ti) mTargets[(size_t) ti].active = false;

    constexpr float kMinDepth = 0.001f;
    for (int ti = 0; ti < n; ++ti)
    {
        const auto& tgt = *all[(size_t) ti];
        bool any = false;
        for (const auto& src : tgt.sources)
            if (std::abs (src.depth) > kMinDepth) { any = true; break; }
        mTargets[(size_t) ti].active = any;
    }

    // If any VoiceEngine target is active, flip mUseVoiceEngines. When it
    // transitions off->on we must sync the voice engines + rebuild immediately
    // so the first sample of the new note reads from a valid voice wavetable.
    const bool anyVE = mTargets[(size_t) ModTargetIndex::PluckDecay].active
                    || mTargets[(size_t) ModTargetIndex::PrismAmount].active
                    || mTargets[(size_t) ModTargetIndex::BlurHarm].active;
    if (anyVE && mVoiceEngineA && mVoiceEngineB && mTemplateA && mTemplateB)
    {
        mVoiceEngineA->copyStateFrom (*mTemplateA);
        mVoiceEngineB->copyStateFrom (*mTemplateB);
        mVoiceEngineA->buildWavetable();
        mVoiceEngineB->buildWavetable();
        mTemplateGenA    = mTemplateA->getGeneration();
        mTemplateGenB    = mTemplateB->getGeneration();
        mUseVoiceEngines = true;
    }
    else
    {
        mUseVoiceEngines = false;
    }
}

void AdditiveVoice::updateModContributions (int numSamples) noexcept
{
    if (mModRegistry == nullptr) return;

    // Re-read active flags if registry generation changed (UI edit published).
    const auto curGen = mModRegistry->getSnapshotGeneration();
    if (curGen != mModSnapGen)
    {
        mModSnapGen = curGen;
        refreshModActiveFlags();
    }

    const auto& all = mModRegistry->getAllTargets();
    const int   n   = juce::jmin ((int) all.size(), (int) mTargets.size());
    const float sr  = float (mSampleRate);

    for (int ti = 0; ti < n; ++ti)
    {
        auto& voiceT = mTargets[(size_t) ti];
        if (! voiceT.active) { voiceT.contribution = 0.0f; voiceT.uniMult = 1.0f; continue; }

        const auto& tgt = *all[(size_t) ti];
        float sum = 0.0f;
        // Unipolar multiplier: starts at 1.0 (no change), each active source
        // pulls it toward `curve_value` scaled by that source's |depth|.
        // Effective per-source blend: 1 - |d| + |d| * curve_value  (linear
        // interpolation between "no mod" and "curve directly drives").
        // Negative depth inverts the curve before compositing.
        float uni = 1.0f;
        auto blendUni = [&uni] (float curve01, float depth)
        {
            const float d = juce::jlimit (-1.f, 1.f, depth);
            const float absD = std::abs (d);
            const float v    = (d >= 0.0f) ? curve01 : (1.0f - curve01);
            uni *= (1.0f - absD) + absD * v;
        };

        // --- Envelope source (WYSIWYG model) ---
        // While the note is held, phase advances 0 -> 1 at rate 1/durSec.
        // Once phase reaches 1, it holds at the curve's final value. On
        // note-off, if phase < 1, the remainder advances over the voice's
        // amp-ADSR release time so short notes get a naturally-paced fade
        // through whatever curve segment wasn't reached yet.
        {
            const auto& src = tgt.sources[(int) ModSource::Envelope];
            if (std::abs (src.depth) > 0.001f)
            {
                const auto& curve = src.tabs[(size_t) juce::jlimit (0, (int) src.tabs.size() - 1, src.activeTab)];
                const float durSec = src.tempoSync
                    ? (src.length / juce::jmax (0.001f, float (mBeatsPerSecond)))
                    : juce::jmax (0.001f, src.length);
                const float spdMul  = std::pow (2.0f, (curve.spd - 0.5f) * 4.0f);
                const float blockSec = float (numSamples) / sr;
                if (mModGateHeld)
                {
                    const float advance = spdMul * blockSec / durSec;
                    voiceT.envPhase = juce::jmin (1.0f, voiceT.envPhase + advance);
                }
                else if (voiceT.envPhase < 1.0f)
                {
                    const float releaseSec = juce::jmax (0.01f, mAmpParams.release);
                    const float advance = blockSec / releaseSec;
                    voiceT.envPhase = juce::jmin (1.0f, voiceT.envPhase + advance);
                }
                const float raw = HarmlessModCurve::sample (curve, voiceT.envPhase);
                sum += HarmlessModCurve::toBipolar (raw, src.depth);
                blendUni (raw, src.depth);
            }
        }

        // --- LFO source ---
        // src.length carries cycle duration: beats-per-cycle when tempoSync,
        // seconds-per-cycle otherwise. Same knob (LENGTH) drives both Env and
        // LFO so users learn one concept. src.lfoShape picks the waveform.
        {
            const auto& src = tgt.sources[(int) ModSource::LFO];
            if (std::abs (src.depth) > 0.001f)
            {
                const auto& curve = src.tabs[(size_t) juce::jlimit (0, (int) src.tabs.size() - 1, src.activeTab)];
                const float cycleDurSec = src.tempoSync
                    ? (src.length / juce::jmax (0.001f, float (mBeatsPerSecond)))
                    : juce::jmax (0.001f, src.length);
                const float rateHz = 1.0f / cycleDurSec;
                const float spdMul = std::pow (2.0f, (curve.spd - 0.5f) * 4.0f);
                const float inc = spdMul * rateHz * float (numSamples) / sr;
                voiceT.lfoPhase += inc;
                voiceT.lfoPhase -= std::floor (voiceT.lfoPhase);
                // Two ways to interpret LFO curve: use its points if drawn, else
                // fall back to the standard lfoShape LUT. For v1 prefer the drawn
                // curve when the user has edited it (>2 points), else the LUT.
                float raw;
                if (curve.points.size() > 2)
                    raw = HarmlessModCurve::sample (curve, voiceT.lfoPhase);
                else
                    raw = HarmlessModCurve::sampleLfoShape (src.lfoShape, voiceT.lfoPhase);
                sum += HarmlessModCurve::toBipolar (raw, src.depth);
                blendUni (raw, src.depth);
            }
        }

        // --- Velocity (static, captured at note-on) ---
        {
            const auto& src = tgt.sources[(int) ModSource::Velocity];
            if (std::abs (src.depth) > 0.001f)
            {
                const auto& curve = src.tabs[(size_t) juce::jlimit (0, (int) src.tabs.size() - 1, src.activeTab)];
                const float raw = HarmlessModCurve::sample (curve, voiceT.capturedVel);
                sum += HarmlessModCurve::toBipolar (raw, src.depth);
                blendUni (raw, src.depth);
            }
        }
        // --- Keyboard (static) ---
        {
            const auto& src = tgt.sources[(int) ModSource::Keyboard];
            if (std::abs (src.depth) > 0.001f)
            {
                const auto& curve = src.tabs[(size_t) juce::jlimit (0, (int) src.tabs.size() - 1, src.activeTab)];
                const float raw = HarmlessModCurve::sample (curve, voiceT.capturedKey);
                sum += HarmlessModCurve::toBipolar (raw, src.depth);
                blendUni (raw, src.depth);
            }
        }
        // --- ModX / Y / Z (live pad input) ---
        const float padVals[3] = {
            juce::jlimit (0.f, 1.f, (mModX + 1.0f) * 0.5f),
            juce::jlimit (0.f, 1.f, (mModY + 1.0f) * 0.5f),
            juce::jlimit (0.f, 1.f, (mModZ + 1.0f) * 0.5f)
        };
        for (int si = 0; si < 3; ++si)
        {
            const auto& src = tgt.sources[(int) ModSource::ModX + si];
            if (std::abs (src.depth) > 0.001f)
            {
                const auto& curve = src.tabs[(size_t) juce::jlimit (0, (int) src.tabs.size() - 1, src.activeTab)];
                const float raw = HarmlessModCurve::sample (curve, padVals[si]);
                sum += HarmlessModCurve::toBipolar (raw, src.depth);
                blendUni (raw, src.depth);
            }
        }

        voiceT.contribution = juce::jlimit (-1.5f, 1.5f, sum);
        voiceT.uniMult      = juce::jlimit (0.0f,  2.0f, uni);
    }

    applyVoiceEngineMods();
}

void AdditiveVoice::applyVoiceEngineMods()
{
    if (! mUseVoiceEngines || ! mVoiceEngineA || ! mVoiceEngineB
        || ! mTemplateA || ! mTemplateB) return;

    const float pluckC = mTargets[(size_t) ModTargetIndex::PluckDecay].contribution;
    const float prismC = mTargets[(size_t) ModTargetIndex::PrismAmount].contribution;
    const float blurC  = mTargets[(size_t) ModTargetIndex::BlurHarm].contribution;

    // Template change: always re-sync from template when generation advances.
    const auto genA = mTemplateA->getGeneration();
    const auto genB = mTemplateB->getGeneration();
    const bool templateDirty = (genA != mTemplateGenA) || (genB != mTemplateGenB);

    // Mod delta: rebuild if any VoiceEngine contribution moved > threshold.
    constexpr float kRebuildThreshold = 0.01f;
    const bool modDirty = std::abs (pluckC - mLastPluckContrib) > kRebuildThreshold
                       || std::abs (prismC - mLastPrismContrib) > kRebuildThreshold
                       || std::abs (blurC  - mLastBlurContrib)  > kRebuildThreshold;

    if (! (templateDirty || modDirty)) return;

    // Copy from template then apply mod deltas.
    mVoiceEngineA->copyStateFrom (*mTemplateA);
    mVoiceEngineB->copyStateFrom (*mTemplateB);

    // Apply bipolar contributions as additive scaling of module amounts. The
    // baseline amount stays from the template; mod adds ± half-range.
    auto applyDeltas = [&] (HarmonicEngine& eng)
    {
        eng.pluck.setAmount (juce::jlimit (0.f, 1.f, eng.pluck.getAmount() + pluckC * 0.5f));
        eng.prism.setAmount (juce::jlimit (0.f, 1.f, eng.prism.getAmount() + prismC * 0.5f));
        eng.blur .setHarmAxis (juce::jlimit (0.f, 1.f,
            (blurC > 0.0f) ? blurC : 0.0f));   // blurHarm: 0..1 unipolar from positive contrib
    };
    applyDeltas (*mVoiceEngineA);
    applyDeltas (*mVoiceEngineB);

    mVoiceEngineA->buildWavetable();
    mVoiceEngineB->buildWavetable();

    mTemplateGenA = genA;
    mTemplateGenB = genB;
    mLastPluckContrib = pluckC;
    mLastPrismContrib = prismC;
    mLastBlurContrib  = blurC;
}
