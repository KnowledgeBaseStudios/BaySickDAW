#include "HarmlessSynth.h"

HarmlessSynth::HarmlessSynth()
{
    // QA-0a (2026-05-07): placeholder sample rate before addVoice (see
    // VibePlayerDSP::VibeSynth ctor for full reasoning).
    mSynth.setCurrentPlaybackSampleRate (44100.0);
    mSynth.addSound (new SynthSound());

    for (int i = 0; i < kNumVoices; ++i)
    {
        auto* voice = new AdditiveVoice();
        voice->setEngines    (&mPartA, &mPartB);
        voice->setPartLevels (1.0f, 0.0f);   // Part A active, Part B silent by default
        mSynth.addVoice (voice);
    }

    // Default wavetables: Part A = Sawtooth, Part B = Square.
    // buildWavetable() is called inside setShape(), so both are ready immediately.
    mPartA.setShape (HarmonicEngine::Shape::Saw);
    mPartB.setShape (HarmonicEngine::Shape::Square);

    // S5 T2-P: background rebuild thread. Register mPartA + mPartB and start
    // the thread. Voice-engine rebuilds stay on the audio thread per Option A.
    mRebuildClientA.engine = &mPartA;
    mRebuildClientB.engine = &mPartB;
    mBackgroundThread.addTimeSliceClient (&mRebuildClientA);
    mBackgroundThread.addTimeSliceClient (&mRebuildClientB);
    mBackgroundThread.startThread (juce::Thread::Priority::low);
}

HarmlessSynth::~HarmlessSynth()
{
    mBackgroundThread.removeTimeSliceClient (&mRebuildClientA);
    mBackgroundThread.removeTimeSliceClient (&mRebuildClientB);
    mBackgroundThread.stopThread (500);
}

//==============================================================================
void HarmlessSynth::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mSynth.setCurrentPlaybackSampleRate (sampleRate);

    // Prepare the output phaser.
    juce::dsp::ProcessSpec phaserSpec;
    phaserSpec.sampleRate       = sampleRate;
    phaserSpec.maximumBlockSize = uint32 (maxBlockSize);
    phaserSpec.numChannels      = 2;
    mOutputPhaser.prepare (phaserSpec);
    mOutputPhaser.setRate (1.0f);
    mOutputPhaser.setDepth (0.5f);
    mOutputPhaser.setMix (0.0f);
    mOutputPhaserReady = true;

    // 2026-04-19 (S1) tilt EQ filter prep: 1-channel filters per side, seed
    // unity coefficients so the first processSample doesn't deref null.
    juce::dsp::ProcessSpec tiltSpec { sampleRate, (juce::uint32) maxBlockSize, 1 };
    auto unity = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
                     (float) sampleRate, 250.0f, 0.7071f, 1.0f);
    for (auto* f : { &mTiltLoL, &mTiltLoR, &mTiltHiL, &mTiltHiR })
    {
        f->coefficients = unity;
        f->prepare (tiltSpec);
        f->reset();
    }
    mTiltReady = true;
    rebuildTiltEQ();

    // 5 ms ramp, matching AdditiveVoice::mVolSmooth, so the routing-matrix VOL
    // trim and the per-voice master volume settle over the same wall-clock time
    // at any device rate.
    mRmVolGain.reset (sampleRate, 0.005);
    mRmVolGain.setCurrentAndTargetValue (rmVolGainFor (mRmVol));
    mRmClipDrive.reset (sampleRate, 0.005);
    mRmClipDrive.setCurrentAndTargetValue (rmClipDriveFor (mRmClip));
}

void HarmlessSynth::renderNextBlock (juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midi)
{
    applyStrum (midi, buffer.getNumSamples());

    // Per-pitch note-off strip (QA-CutSelfReview): see BaySickSynthDSP for the
    // rationale - overlapping same-pitch notes must not let an earlier note's
    // note-off cascade onto the retriggered voice.  Harmless is always poly.
    {
        bool anyActive = false;
        for (int i = 0; i < mSynth.getNumVoices(); ++i)
            if (mSynth.getVoice (i)->isVoiceActive()) { anyActive = true; break; }
        if (! anyActive)
            for (int k = 0; k < 128; ++k) mNoteOnCount[k] = 0;

        juce::MidiBuffer filtered;
        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            if (msg.isNoteOn())
            {
                const int p = msg.getNoteNumber();
                if (p >= 0 && p < 128) ++mNoteOnCount[p];
                filtered.addEvent (msg, meta.samplePosition);
            }
            else if (msg.isNoteOff())
            {
                const int  p       = msg.getNoteNumber();
                bool       deliver = true;
                if (p >= 0 && p < 128)
                {
                    if (mNoteOnCount[p] > 0) --mNoteOnCount[p];
                    if (mNoteOnCount[p] > 0) deliver = false;   // a later same-pitch note still holds it
                }
                if (deliver) filtered.addEvent (msg, meta.samplePosition);
            }
            else
            {
                // CC-123 All-Notes-Off / CC-120 All-Sound-Off flush every voice
                // without per-note offs (transport stop / play-from-top / loop
                // wrap).  Zero the counter so those untracked releases can't leave
                // it drifted and strip a later note-off.
                if (msg.isAllNotesOff() || msg.isAllSoundOff())
                    for (int k = 0; k < 128; ++k) mNoteOnCount[k] = 0;
                filtered.addEvent (msg, meta.samplePosition);
            }
        }
        midi.swapWith (filtered);
    }

    // Cut Self (QA-CutSelfReview): on each incoming note-on, hard-cut the matching
    // voice (Same Pitch) or every active voice (Cut All) BEFORE rendering, so the
    // retrigger starts clean.  cutFast() is an instant ~1.5 ms click-free
    // quick-release - NOT a MIDI note-off, which runs the (possibly long) ADSR
    // release = the audible bleed.  Harmless is always poly.
    if (mCutSelf)
    {
        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            if (! msg.isNoteOn())
                continue;

            const int n = msg.getNoteNumber();
            forEachVoice ([this, n] (AdditiveVoice& v)
            {
                if (v.isVoiceActive() && (mCutAll || v.getCurrentlyPlayingNote() == n))
                    v.cutFast();
            });
        }
    }

    // 2026-04-30: T2-B lfo_vel - note-on velocity scaling.  If the global
    // lfo_vel slider is non-zero, sample the LFO at the START of this block
    // (block-rate is plenty for an LFO at sub-Hz to ~10 Hz rates) and scale
    // every incoming noteOn's velocity by 1 + LFO * depth * 0.5.  Result:
    // ±50 % per-note velocity variation when depth is at full ±1.
    if (std::abs (mGlobalLfoVelDepth) > 1.0e-4f)
    {
        const float twoPi = juce::MathConstants<float>::twoPi;
        const float ph    = mGlobalLfoPhase01;
        // Bipolar LFO output -1..+1 based on shape.  Same set of shapes as
        // applyGlobalLfoToAllTargets (0=sine 1=tri 2=saw 3=square).
        float lfoOut = 0.0f;
        switch (mGlobalLfoShape)
        {
            case 1: // triangle
                lfoOut = (ph < 0.5f) ? (4.0f * ph - 1.0f) : (3.0f - 4.0f * ph);
                break;
            case 2: // saw (rising)
                lfoOut = 2.0f * ph - 1.0f;
                break;
            case 3: // square
                lfoOut = (ph < 0.5f) ? -1.0f : 1.0f;
                break;
            default: // sine
                lfoOut = std::sin (twoPi * ph);
                break;
        }
        const float velScale = 1.0f + lfoOut * mGlobalLfoVelDepth * 0.5f;

        // Walk midi, multiply each noteOn's velocity by velScale.
        juce::MidiBuffer scaled;
        for (const auto meta : midi)
        {
            auto msg = meta.getMessage();
            if (msg.isNoteOn())
            {
                const float v01 = juce::jlimit (0.0f, 1.0f,
                    msg.getFloatVelocity() * velScale);
                msg = juce::MidiMessage::noteOn (msg.getChannel(), msg.getNoteNumber(), v01);
            }
            scaled.addEvent (msg, meta.samplePosition);
        }
        midi.swapWith (scaled);
    }

    // Advance the global LFO phase by this block's sample count so the next
    // block's noteOn velocity scaling uses the next LFO sample.
    tickGlobalLfoVel (buffer.getNumSamples());

    mSynth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    // S4 Batch 2b: aggregate SynthLevel mod targets across voices using the
    // "loudest voice wins" rule and apply to the output phaser before it
    // processes this block. Updates are block-rate - audibly smooth given
    // typical 32-512 sample blocks.
    {
        float bestLevel    = -1.0f;
        float bestPhaserMix   = 0.0f;
        float bestPhaserWidth = 0.0f;
        for (int i = 0; i < mSynth.getNumVoices(); ++i)
        {
            if (auto* v = dynamic_cast<AdditiveVoice*> (mSynth.getVoice (i)))
            {
                const float lvl = v->getCurrentEnvLevel();
                if (lvl > bestLevel)
                {
                    bestLevel        = lvl;
                    bestPhaserMix    = v->getTargetContribution ((int) ModTargetIndex::PhaserMix);
                    bestPhaserWidth  = v->getTargetContribution ((int) ModTargetIndex::PhaserWidth);
                }
            }
        }
        if (mOutputPhaserReady)
        {
            // If no voices active, fall back to user-set values (no mod).
            const float modMix   = (bestLevel > 0.0f) ? bestPhaserMix   : 0.0f;
            const float modWidth = (bestLevel > 0.0f) ? bestPhaserWidth : 0.0f;
            const float effMix   = juce::jlimit (0.f, 1.f,   mOutputPhaserMix + modMix * 0.5f);
            const float effWidth = juce::jlimit (0.f, 0.95f, 0.5f + modWidth * 0.5f);
            mOutputPhaser.setMix      (effMix);
            mOutputPhaser.setFeedback (effWidth);
        }
    }

    // 2026-04-19 (S1) post-voice processing - applied in this fixed order:
    //   tilt EQ -> output phaser -> routing-matrix vol/clip output stage.
    // Routing-matrix sub/prot/env are voice-level and were handled inside
    // AdditiveVoice render via the broadcast in setRoutingMatrix; rm_fx is
    // applied in the wavetable-build path via setSpectralFxAmount.

    // T1a output tilt EQ.
    if (mTiltReady && std::abs (mOutputEqMix) > 0.001f
                   && buffer.getNumChannels() >= 1
                   && buffer.getNumSamples()  >  0)
    {
        const int n = buffer.getNumSamples();
        if (auto* L = buffer.getWritePointer (0))
            for (int i = 0; i < n; ++i)
                L[i] = mTiltHiL.processSample (mTiltLoL.processSample (L[i]));
        if (buffer.getNumChannels() > 1)
            if (auto* R = buffer.getWritePointer (1))
                for (int i = 0; i < n; ++i)
                    R[i] = mTiltHiR.processSample (mTiltLoR.processSample (R[i]));
    }

    // Apply output phaser if mix is non-trivial.
    if (mOutputPhaserReady && mOutputPhaserMix > 0.001f)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        mOutputPhaser.process (ctx);
    }

    // T2-F routing matrix output stage: rm_vol gain trim then the rm_clip soft
    // clipper.  rm_vol default 1.0 = unity (rmVolGainFor(1) == 1); rm_clip
    // default 0.0 = drive 0 = identity, so the default path is a bare gain.
    // Both skip tests ask the smoothers rather than the raw knobs: a ramp still
    // in flight has to keep being applied, and gating on the knob was itself
    // the discontinuity - it made the last step of the rm_vol travel jump from
    // 1.5x to unity, and made rm_clip step in with makeup gain already applied.
    if (mRmVolGain.isSmoothing()   || mRmVolGain.getTargetValue() != 1.0f
     || mRmClipDrive.isSmoothing() || mRmClipDrive.getTargetValue() > 0.0f)
    {
        const int     n     = buffer.getNumSamples();
        const int     numCh = buffer.getNumChannels();
        float* const* chans = buffer.getArrayOfWritePointers();
        for (int i = 0; i < n; ++i)
        {
            // One advance per sample frame, shared by every channel - advancing
            // inside the channel loop would run the ramps numCh times too fast.
            const float gain = mRmVolGain.getNextValue();
            const float drv  = mRmClipDrive.getNextValue();
            const bool  clip = drv > kRmClipMinDrive;
            const float norm = clip ? 1.0f / drv : 1.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                float s = chans[ch][i] * gain;
                if (clip) s = std::tanh (s * drv) * norm;
                chans[ch][i] = s;
            }
        }
    }
}

//==============================================================================
void HarmlessSynth::setUnison (int numVoices, float detuneCents, float spread)
{
    forEachVoice ([&] (AdditiveVoice& v)
        { v.setUnison (numVoices, detuneCents, spread); });
}

void HarmlessSynth::setPartLevels (float partALevel, float partBLevel)
{
    forEachVoice ([&] (AdditiveVoice& v)
        { v.setPartLevels (partALevel, partBLevel); });
}

void HarmlessSynth::setAmpEnv (float a, float d, float s, float r)
{
    forEachVoice ([&] (AdditiveVoice& v)
        { v.setAmpEnv (a, d, s, r); });
}

void HarmlessSynth::setFilterParams (float cutoffHz, float resonance)
{
    forEachVoice ([&] (AdditiveVoice& v)
        { v.setFilterParams (cutoffHz, resonance); });
}

void HarmlessSynth::setVolume (float vol)
{
    forEachVoice ([&] (AdditiveVoice& v) { v.setVolume (vol); });
}

void HarmlessSynth::setPan (float pan)
{
    forEachVoice ([&] (AdditiveVoice& v) { v.setPan (pan); });
}

void HarmlessSynth::setGlide (float glideTimeSec)
{
    // S2 T2-N: legato_limit caps the effective glide time. Store the user's
    // set value separately so re-broadcasting on legato_limit changes works.
    mUserGlideTime = juce::jmax (0.f, glideTimeSec);
    const float effective = juce::jmin (mUserGlideTime, mLegatoLimit);
    forEachVoice ([effective] (AdditiveVoice& v) { v.setGlide (effective); });
}

void HarmlessSynth::setLegato (bool legatoOn)
{
    forEachVoice ([&] (AdditiveVoice& v) { v.setLegato (legatoOn); });
}

//==============================================================================
// New parameter broadcasts.

void HarmlessSynth::setPhaseInit (float start, float rand) noexcept
{
    forEachVoice ([&] (AdditiveVoice& v) { v.setPhaseInit (start, rand); });
}

void HarmlessSynth::setTremoloParams (int shape, float depth, float speed, float gap) noexcept
{
    forEachVoice ([&] (AdditiveVoice& v) { v.setTremoloParams (shape, depth, speed, gap); });
}

void HarmlessSynth::setVibratoParams (int shape, float depth, float speed, float env) noexcept
{
    forEachVoice ([&] (AdditiveVoice& v) { v.setVibratoParams (shape, depth, speed, env); });
}

void HarmlessSynth::setFilter2Params (float cutoffHz, float res) noexcept
{
    forEachVoice ([&] (AdditiveVoice& v) { v.setFilter2Params (cutoffHz, res); });
}

void HarmlessSynth::setPitchOffset (float semitones, float cents) noexcept
{
    forEachVoice ([&] (AdditiveVoice& v) { v.setPitchOffset (semitones, cents); });
}

void HarmlessSynth::setOutputPhaserParams (float mix, float depth, float rate) noexcept
{
    mOutputPhaserMix = juce::jlimit (0.f, 1.f, mix);
    if (mOutputPhaserReady)
    {
        mOutputPhaser.setRate  (juce::jmax (0.01f, rate));
        mOutputPhaser.setDepth (juce::jlimit (0.f, 1.f, depth));
        mOutputPhaser.setMix   (mOutputPhaserMix);
    }
}

// 2026-04-19 (SLA-Impl) Phaser WIDTH (feedback) + OFS (centre freq).
void HarmlessSynth::setOutputPhaserExtras (float width, float ofsHz) noexcept
{
    if (! mOutputPhaserReady) return;
    mOutputPhaser.setFeedback        (juce::jlimit (0.f, 0.95f, width));
    mOutputPhaser.setCentreFrequency (juce::jlimit (50.f, 5000.f, ofsHz));
}

void HarmlessSynth::setPitchFraction (int idx) noexcept
{
    const int n = juce::jlimit (0, 6, idx);
    forEachVoice ([n] (AdditiveVoice& v) { v.setPitchFraction (n); });
}

// ── 2026-04-19 (S2) Filter envelopes / ofs / kb track broadcasts ──────────
void HarmlessSynth::setFilter1Env (float a, float d, float s, float r) noexcept
    { forEachVoice ([&] (AdditiveVoice& v) { v.setFilter1Env (a, d, s, r); }); }
void HarmlessSynth::setFilter2Env (float a, float d, float s, float r) noexcept
    { forEachVoice ([&] (AdditiveVoice& v) { v.setFilter2Env (a, d, s, r); }); }
void HarmlessSynth::setFilter1EnvAmt    (float amt)   noexcept { forEachVoice ([amt] (AdditiveVoice& v) { v.setFilter1EnvAmt (amt); }); }
void HarmlessSynth::setFilter2EnvAmt    (float amt)   noexcept { forEachVoice ([amt] (AdditiveVoice& v) { v.setFilter2EnvAmt (amt); }); }
void HarmlessSynth::setFilter1CutoffOfs (float semis) noexcept { forEachVoice ([semis] (AdditiveVoice& v) { v.setFilter1CutoffOfs (semis); }); }
void HarmlessSynth::setFilter2CutoffOfs (float semis) noexcept { forEachVoice ([semis] (AdditiveVoice& v) { v.setFilter2CutoffOfs (semis); }); }
void HarmlessSynth::setFilter1KbTrack   (float depth) noexcept { forEachVoice ([depth] (AdditiveVoice& v) { v.setFilter1KbTrack (depth); }); }
void HarmlessSynth::setFilter2KbTrack   (float depth) noexcept { forEachVoice ([depth] (AdditiveVoice& v) { v.setFilter2KbTrack (depth); }); }

// ── 2026-04-19 (S4) Mod XYZ pad input broadcast (dest routing moved to registry)
void HarmlessSynth::setModXYZ (float x, float y, float z) noexcept
    { forEachVoice ([x, y, z] (AdditiveVoice& v) { v.setModXYZ (x, y, z); }); }

// ── 2026-04-19 (S4 AG-1) Auto-gain mode forwarder ─────────────────────────
void HarmlessSynth::setAutoGainMode (int mode) noexcept
{
    mPartA.setAutoGainMode (mode);
    mPartB.setAutoGainMode (mode);
    mPartA.requestRebuild();
    mPartB.requestRebuild();
}

// ── 2026-04-20 (S4 Batch 2b) Mod registry + tempo broadcast ──────────────
void HarmlessSynth::setModRegistry (const HarmlessModRegistry* r) noexcept
{
    mModRegistry = r;
    forEachVoice ([r] (AdditiveVoice& v) { v.setModRegistry (r); });
}

void HarmlessSynth::setBeatsPerSecond (double bps) noexcept
{
    // 2026-04-30: also feed the global LFO clock used for lfo_vel scaling
    // so it stays in sync when the user changes BPM mid-session.
    mGlobalLfoBps = juce::jmax (1.0e-3, bps);
    forEachVoice ([bps] (AdditiveVoice& v) { v.setBeatsPerSecond (bps); });
}

// ── 2026-04-20 (S5 T2-M) Aggregated partial amplitudes for spectrogram ────
void HarmlessSynth::getAggregatedPartialAmplitudes (float* outBuf, int numPartials) const
{
    std::fill (outBuf, outBuf + numPartials, 0.0f);
    auto& syn = const_cast<BroadcastSynthesiser&> (mSynth);
    for (int i = 0; i < syn.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<const AdditiveVoice*> (syn.getVoice (i)))
            v->accumulatePartialAmplitudes (outBuf, numPartials);
}

// ── 2026-04-20 (S4 Batch 4 fix) Global LFO macro ──────────────────────────
// Copies the global LFO settings into every target's LFO source in the mod
// registry. Per-target overrides in the mod editor still work after this -
// they just get reset next time the user moves a global knob.
void HarmlessSynth::applyGlobalLfoToAllTargets (int rateIdx, int shape, bool tempoSync)
{
    if (mModRegistry == nullptr) return;

    static const float kLens[13] = {
        0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f,
        1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f
    };
    const float cycleLen = kLens[juce::jlimit (0, 12, rateIdx)];

    // Thread-safety: reached from the audio thread (processBlock ->
    // updateFromApvts), so the registry's edit lock is deliberately NOT taken -
    // blocking on a lock the UI holds across vector edits and ValueTree walks
    // would stall the render callback.  Nothing here needs it: mTargets is
    // written only at construction and is stable for the registry's lifetime,
    // ModTarget::sources is a fixed std::array, and the only writes are leaf
    // scalars - the same lock-free footing AdditiveVoice already reads them on.
    for (const auto& tgt : mModRegistry->getAllTargets())
    {
        auto& src = tgt->sources[(int) ModSource::LFO];
        src.length    = cycleLen;
        src.lfoShape  = juce::jlimit (0, 3, shape);
        src.tempoSync = tempoSync;
    }
    const_cast<HarmlessModRegistry*> (mModRegistry)->publishSnapshot();
}

// ── 2026-04-30: T2-B LFO depth shortcuts (restored after S4 strip) ──────
// Each macro writes the LFO source's `depth` field on ONE specific mod
// registry target.  Replace semantics: the global slider's value clobbers
// whatever per-target depth the user set in the in-player Mod Editor.
void HarmlessSynth::applyGlobalLfoVolDepth (float depth) noexcept
{
    // The Volume mod target was registered with paramId pid("volume") in
    // HarmlessProcessor::registerModTargets - but that paramId already
    // includes the per-track prefix (e.g. "tk_lay_0_harm_volume").  We can't
    // reconstruct the full id here; loop the targets and match the suffix
    // instead.  16 targets, called only when the user moves the slider, so
    // the cost is negligible.
    if (mModRegistry == nullptr) return;
    auto* mut = const_cast<HarmlessModRegistry*> (mModRegistry);
    // Thread-safety: audio thread, one leaf-scalar write - no edit lock, for
    // the same reason as applyGlobalLfoToAllTargets above.
    for (const auto& tgt : mModRegistry->getAllTargets())
    {
        if (tgt->paramId.endsWith ("_volume"))
        {
            tgt->sources[(int) ModSource::LFO].depth = juce::jlimit (-1.0f, 1.0f, depth);
            break;
        }
    }
    mut->publishSnapshot();
}

void HarmlessSynth::applyGlobalLfoPitchDepth (float depth) noexcept
{
    if (mModRegistry == nullptr) return;
    auto* mut = const_cast<HarmlessModRegistry*> (mModRegistry);
    // Thread-safety: audio thread, one leaf-scalar write - no edit lock, for
    // the same reason as applyGlobalLfoToAllTargets above.
    for (const auto& tgt : mModRegistry->getAllTargets())
    {
        if (tgt->paramId.endsWith ("_pitch_semitones"))
        {
            tgt->sources[(int) ModSource::LFO].depth = juce::jlimit (-1.0f, 1.0f, depth);
            break;
        }
    }
    mut->publishSnapshot();
}

void HarmlessSynth::setGlobalLfoVelRate (float rateLen, int shape, bool tempoSync) noexcept
{
    mGlobalLfoRateLen   = juce::jmax (1.0e-3f, rateLen);
    mGlobalLfoShape     = juce::jlimit (0, 3, shape);
    mGlobalLfoTempoSync = tempoSync;
}

void HarmlessSynth::tickGlobalLfoVel (int numSamples) noexcept
{
    // Period in seconds: rateLen / bps when tempo-synced, else rateLen
    // already in seconds.  Always uses the current bps - so BPM changes
    // mid-session shift the LFO period the next block, no recompute call
    // needed from the processor side.
    const float periodSec = mGlobalLfoTempoSync
                              ? (float) (mGlobalLfoRateLen / juce::jmax (1.0e-3, mGlobalLfoBps))
                              : mGlobalLfoRateLen;
    const float cycleSamples = (float) (periodSec * mSampleRate);
    if (cycleSamples > 0.0f)
    {
        mGlobalLfoPhase01 += (float) numSamples / cycleSamples;
        if (mGlobalLfoPhase01 >= 1.0f)
            mGlobalLfoPhase01 -= std::floor (mGlobalLfoPhase01);
    }
}

// ── 2026-04-19 (S2 SLA) Blur extensions on engines ─────────────────────────
void HarmlessSynth::setBlurTimeA (float t)
{
    mPartA.blur.setTimeScale (t);
    mPartA.requestRebuild();
}
void HarmlessSynth::setBlurTimeB (float t)
{
    mPartB.blur.setTimeScale (t);
    mPartB.requestRebuild();
}
void HarmlessSynth::setBlurHarmA (float h)
{
    mPartA.blur.setHarmAxis (h);
    mPartA.requestRebuild();
}
void HarmlessSynth::setBlurHarmB (float h)
{
    mPartB.blur.setHarmAxis (h);
    mPartB.requestRebuild();
}

// ── 2026-04-19 (S3 T2-C) Unison Type / Alt / Phase + Part B shape ──────────
void HarmlessSynth::setUnisonType (int type)
{
    forEachVoice ([type] (AdditiveVoice& v) { v.setUnisonType (type); });
}
void HarmlessSynth::setUnisonAlt (bool on)
{
    forEachVoice ([on] (AdditiveVoice& v) { v.setUnisonAlt (on); });
}
void HarmlessSynth::setUnisonPhase (float amt)
{
    forEachVoice ([amt] (AdditiveVoice& v) { v.setUnisonPhase (amt); });
}
void HarmlessSynth::setPartBShape (int shape)
{
    static constexpr HarmonicEngine::Shape kShapes[] = {
        HarmonicEngine::Shape::Sine,
        HarmonicEngine::Shape::Saw,
        HarmonicEngine::Shape::Square,
        HarmonicEngine::Shape::Triangle
    };
    mPartB.setShape (kShapes[juce::jlimit (0, 3, shape)]);
}

// ── 2026-04-19 (S3.5) Per-part spectral module setters ─────────────────────
void HarmlessSynth::setPrismAmountA      (float a) { mPartA.prism.setAmount (a);     mPartA.requestRebuild(); }
void HarmlessSynth::setPrismAmountB      (float a) { mPartB.prism.setAmount (a);     mPartB.requestRebuild(); }
void HarmlessSynth::setPluckDecayA       (float a) { mPartA.pluck.setAmount (a);     mPartA.requestRebuild(); }
void HarmlessSynth::setPluckDecayB       (float a) { mPartB.pluck.setAmount (a);     mPartB.requestRebuild(); }
void HarmlessSynth::setBlurSizeA         (float a) { mPartA.blur.setAmount (a);      mPartA.requestRebuild(); }
void HarmlessSynth::setBlurSizeB         (float a) { mPartB.blur.setAmount (a);      mPartB.requestRebuild(); }
void HarmlessSynth::setFilterMaskAmountA (float a) { mPartA.filterMask.setAmount(a); mPartA.requestRebuild(); }
void HarmlessSynth::setFilterMaskAmountB (float a) { mPartB.filterMask.setAmount(a); mPartB.requestRebuild(); }
void HarmlessSynth::setPhaserMaskRateA   (float r) { mPartA.phaserMask.setRate (r);  mPartA.requestRebuild(); }
void HarmlessSynth::setPhaserMaskRateB   (float r) { mPartB.phaserMask.setRate (r);  mPartB.requestRebuild(); }
void HarmlessSynth::setBrownianAmountA   (float a) { mPartA.setBrownianAmount (a);   mPartA.requestRebuild(); }
void HarmlessSynth::setBrownianAmountB   (float a) { mPartB.setBrownianAmount (a);   mPartB.requestRebuild(); }

//==============================================================================
// Strum preprocessing - staggers simultaneous note-ons across time.
// Uses a fixed-size stack buffer; no heap allocation on the audio thread.

void HarmlessSynth::applyStrum (juce::MidiBuffer& midi, int numSamples)
{
    if (mStrumTimeSec < 0.001f)
        return;

    // Collect note-on events; pass everything else through unchanged.
    struct NoteEntry { int pos; juce::MidiMessage msg; };
    NoteEntry noteOns[128];
    int noteCount = 0;

    juce::MidiBuffer passThrough;
    for (const auto meta : midi)
    {
        const auto& m = meta.getMessage();
        if (m.isNoteOn() && noteCount < 128)
            noteOns[noteCount++] = { meta.samplePosition, m };
        else
            passThrough.addEvent (m, meta.samplePosition);
    }

    if (noteCount <= 1)
        return;   // nothing to stagger - leave midi unchanged

    // Sort / shuffle according to strum direction.
    if (mStrumDir == 1)
    {
        // Down strum: high-to-low (descending pitch)
        std::sort (noteOns, noteOns + noteCount,
                   [] (const NoteEntry& a, const NoteEntry& b)
                   { return a.msg.getNoteNumber() > b.msg.getNoteNumber(); });
    }
    else if (mStrumDir == 2)
    {
        // Random: pseudo-random shuffle using note numbers as seed
        for (int i = noteCount - 1; i > 0; --i)
        {
            int j = (noteOns[i].msg.getNoteNumber() * 7 + i * 3) % (i + 1);
            std::swap (noteOns[i], noteOns[j]);
        }
    }
    else
    {
        // Up strum (default): low-to-high (ascending pitch)
        std::sort (noteOns, noteOns + noteCount,
                   [] (const NoteEntry& a, const NoteEntry& b)
                   { return a.msg.getNoteNumber() < b.msg.getNoteNumber(); });
    }

    const int strumSamples = int (mStrumTimeSec * float (mSampleRate));
    // Old "step = strumSamples / (noteCount-1)" replaced by the per-note tension
    // curve below; kept the variable name for code-archaeology context.

    midi = passThrough;
    for (int i = 0; i < noteCount; ++i)
    {
        // T2-N strum tension: -1 = bunched at end, 0 = linear, +1 = bunched at start.
        // Apply a power curve to the [0,1] position fraction.
        const float t = (noteCount > 1) ? (float) i / (float) (noteCount - 1) : 0.f;
        const float exponent = std::exp (-mStrumTns * 1.5f);   // tns +1 -> ~0.22, -1 -> ~4.5
        const float tCurved  = std::pow (t, exponent);
        const int   pos      = juce::jmin (noteOns[i].pos + (int)(tCurved * (float) strumSamples),
                                           numSamples - 1);
        midi.addEvent (noteOns[i].msg, pos);
    }
}

//==============================================================================
// 2026-04-19 (S1) newly-wired ghost params + routing matrix.

void HarmlessSynth::setTimbreBlend (float blend) noexcept
{
    const float b = juce::jlimit (0.f, 1.f, blend);
    forEachVoice ([b] (AdditiveVoice& v) { v.setTimbreBlend (b); });
}

void HarmlessSynth::setPrismModeA (int mode)
{
    mPartA.prism.setMode (juce::jlimit (0, 2, mode));
    mPartA.requestRebuild();
}
void HarmlessSynth::setPrismModeB (int mode)
{
    mPartB.prism.setMode (juce::jlimit (0, 2, mode));
    mPartB.requestRebuild();
}

void HarmlessSynth::setPluckBlurA (bool blurOn)
{
    mPartA.pluck.setBlur (blurOn);
    mPartA.requestRebuild();
}
void HarmlessSynth::setPluckBlurB (bool blurOn)
{
    mPartB.pluck.setBlur (blurOn);
    mPartB.requestRebuild();
}

void HarmlessSynth::setOutputEQMix (float mix) noexcept
{
    const float m = juce::jlimit (0.f, 1.f, mix);
    if (std::abs (m - mOutputEqMix) < 1e-4f) return;
    mOutputEqMix = m;
    rebuildTiltEQ();
}

void HarmlessSynth::rebuildTiltEQ()
{
    if (! mTiltReady || mSampleRate <= 0.0) return;
    // Tilt EQ: low shelf at 250 Hz with +6 dB * mix, high shelf at 4 kHz
    // with -6 dB * mix. Smile / frown shape depending on mix sign (we only
    // use 0..1 here so always smile = bass + treble cut implies dark sound).
    // For simplicity: positive mix = treble lift; bass dimmed via complement.
    const float gainDb = mOutputEqMix * 6.0f;          // up to +6 dB
    const float lowGainDb = -gainDb;                    // opposite sign on low shelf
    auto loCoefs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
                       (float) mSampleRate, 250.0f, 0.7071f,
                       juce::Decibels::decibelsToGain (lowGainDb));
    auto hiCoefs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                       (float) mSampleRate, 4000.0f, 0.7071f,
                       juce::Decibels::decibelsToGain (gainDb));
    mTiltLoL.coefficients = loCoefs;
    mTiltLoR.coefficients = loCoefs;
    mTiltHiL.coefficients = hiCoefs;
    mTiltHiR.coefficients = hiCoefs;
}

void HarmlessSynth::setFilterType (int t)
{
    const int n = juce::jlimit (0, 3, t);
    forEachVoice ([n] (AdditiveVoice& v) { v.setFilterType (n); });
}

void HarmlessSynth::setFilter2Type (int t)
{
    const int n = juce::jlimit (0, 3, t);
    forEachVoice ([n] (AdditiveVoice& v) { v.setFilter2Type (n); });
}

void HarmlessSynth::setRoutingMatrix (float sub, float prot, float clip,
                                       float fx,  float vol,  float env) noexcept
{
    mRmSub  = juce::jlimit (0.f, 1.f, sub);
    mRmProt = juce::jlimit (0.f, 1.f, prot);
    mRmClip = juce::jlimit (0.f, 1.f, clip);
    mRmFx   = juce::jlimit (0.f, 1.f, fx);
    mRmVol  = juce::jlimit (0.f, 1.f, vol);
    mRmEnv  = juce::jlimit (0.f, 1.f, env);
    mRmVolGain.setTargetValue (rmVolGainFor (mRmVol));
    mRmClipDrive.setTargetValue (rmClipDriveFor (mRmClip));

    // Voice-level pieces broadcast: sub osc gain + env-on-amp depth.
    forEachVoice ([this] (AdditiveVoice& v)
    {
        v.setSubOscGain (mRmSub);
        v.setEnvDepth   (mRmEnv);
    });

    // FX wet (rm_fx) scales the spectral-FX amounts on the wavetable build.
    // Apply by re-broadcasting prism/pluck/blur with the multiplier.
    // Since each set* triggers a rebuild, batch-multiply via a single pass
    // through HarmonicEngine - we don't have a pure rmFx scalar there yet,
    // so just apply on-the-fly via a setter on the engine.
    mPartA.setSpectralFxScale (mRmFx);
    mPartB.setSpectralFxScale (mRmFx);
    mPartA.requestRebuild();
    mPartB.requestRebuild();

    // rm_prot = high-partial rolloff intensity, also a wavetable-build factor.
    mPartA.setNyquistProtect (mRmProt);
    mPartB.setNyquistProtect (mRmProt);
}
