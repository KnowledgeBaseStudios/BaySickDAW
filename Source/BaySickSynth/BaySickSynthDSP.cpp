#include "BaySickSynthDSP.h"
#include <algorithm>

BaySickSynthDSP::BaySickSynthDSP()
{
    // QA-0a (2026-05-07): placeholder sample rate before addVoice (see
    // VibePlayerDSP::VibeSynth ctor for full reasoning).  prepare()
    // overwrites with the real rate before any audio processing.
    mSynth.setCurrentPlaybackSampleRate (44100.0);
    mSynth.addSound (new SynthSound());
    for (int i = 0; i < kNumVoices; ++i)
    {
        auto* v = new BaySickSynthVoice();
        mVoices[i] = v;
        mSynth.addVoice (v);
    }
}

void BaySickSynthDSP::prepare (double sampleRate, int /*maxBlockSize*/)
{
    mSynth.setCurrentPlaybackSampleRate (sampleRate);
}

void BaySickSynthDSP::renderNextBlock (juce::AudioBuffer<float>& buf,
                                        juce::MidiBuffer& midi)
{
    // The per-pitch note-off counter (poly branch below) is poly-only; keep it
    // zeroed in the other modes so Poly always resumes from a clean state.
    if (mVoiceMode != BssVoiceMode::Poly)
        std::fill_n (mNoteOnCount, 128, 0);

    if (mVoiceMode == BssVoiceMode::Legato)
    {
        handleLegatoMidi (midi);
        // handleLegatoMidi leaves only non-note events in `midi` for the synth
        // (noteOns/Offs were handled directly against mLegatoVoice).
        mSynth.renderNextBlock (buf, midi, 0, buf.getNumSamples());
        return;
    }

    if (mVoiceMode != BssVoiceMode::Poly)
    {
        // Mono: inject noteOff for previous note before each noteOn to enforce
        // monophonic behaviour without changing voice count.  Every note is a
        // fresh allocation, so every note re-articulates the envelopes -- that
        // is the whole difference from Legato, which glides instead.
        juce::MidiBuffer processed;

        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();

            if (msg.isNoteOn())
            {
                if (mLastMonoNote >= 0)
                {
                    processed.addEvent (
                        juce::MidiMessage::noteOff (msg.getChannel(), mLastMonoNote),
                        meta.samplePosition);
                }
                mLastMonoNote = msg.getNoteNumber();
                processed.addEvent (msg, meta.samplePosition);
            }
            else if (msg.isNoteOff())
            {
                if (msg.getNoteNumber() == mLastMonoNote)
                    mLastMonoNote = -1;
                processed.addEvent (msg, meta.samplePosition);
            }
            else
            {
                processed.addEvent (msg, meta.samplePosition);
            }
        }

        mSynth.renderNextBlock (buf, processed, 0, buf.getNumSamples());
    }
    else
    {
        mLastMonoNote = -1;
        mLegatoHeldNotes.clear();
        mLegatoVoice     = nullptr;
        mLegatoSynthNote = -1;

        // Per-pitch note-off strip (QA-CutSelfReview): overlapping same-pitch notes
        // must not let the earlier note's note-off cascade onto the retriggered
        // voice (juce::Synthesiser matches note-offs by pitch).  Hold an earlier
        // note-off until the LAST overlapping note of that pitch ends.  Self-heals:
        // the counter zeroes whenever the synth is fully silent (no lost-note-off
        // drift can persist past a moment of silence).
        bool anyActive = false;
        for (int vi = 0; vi < kNumVoices; ++vi)
            if (mVoices[vi] != nullptr && mVoices[vi]->isVoiceActive()) { anyActive = true; break; }
        if (! anyActive)
            std::fill_n (mNoteOnCount, 128, 0);

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
                // wrap, PluginProcessor.cpp).  Zero the counter so those untracked
                // releases can't leave it drifted and strip a later note-off.
                if (msg.isAllNotesOff() || msg.isAllSoundOff())
                    std::fill_n (mNoteOnCount, 128, 0);
                filtered.addEvent (msg, meta.samplePosition);
            }
        }

        // Cut Self (Poly): on each note-on, hard-cut the matching voice (Same Pitch)
        // or every active voice (Cut All) BEFORE rendering, so the retrigger starts
        // clean.  cutFast() is an instant ~1 ms click-free fade-out - NOT a MIDI
        // note-off (which JUCE turns into an ADSR tail-off = the old bleed).  Runs
        // only on note-on events; mVoices[] is cached (no dynamic_cast).
        if (mCutSelf)
        {
            for (const auto meta : filtered)
            {
                const auto msg = meta.getMessage();
                if (! msg.isNoteOn())
                    continue;

                const int n = msg.getNoteNumber();
                for (int vi = 0; vi < kNumVoices; ++vi)
                    if (auto* v = mVoices[vi];
                        v != nullptr && v->isVoiceActive()
                        && (mCutAll || v->getCurrentlyPlayingNote() == n))
                        v->cutFast();
            }
        }

        mSynth.renderNextBlock (buf, filtered, 0, buf.getNumSamples());
    }
}

// Legato MIDI dispatch (last-note priority).
//   noteOn  : retarget live voice if one exists; else allocate via mSynth.
//   noteOff : pop note from held stack. If any held notes remain, retarget to
//             the last remaining note. If none, trigger release on the voice.
// Non-note events (CC, pitch bend, etc.) are preserved for the synth.
void BaySickSynthDSP::handleLegatoMidi (juce::MidiBuffer& midi)
{
    juce::MidiBuffer pass;

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();

        if (msg.isNoteOn())
        {
            const int   note = msg.getNoteNumber();
            const int   ch   = msg.getChannel();
            const float vel  = msg.getFloatVelocity();
            mLegatoHeldNotes.push_back (note);

            const bool haveActiveLegatoVoice = (mLegatoVoice != nullptr)
                                             && mLegatoVoice->isVoiceActive()
                                             && ! mLegatoVoice->isInRelease();

            if (haveActiveLegatoVoice)
            {
                // Retarget the existing voice (glide + pitch change, no env retrigger).
                // mLegatoSynthNote stays pointed at the original note so the final
                // noteOff routes correctly through the synth.
                mLegatoVoice->retargetLegato (note);
            }
            else
            {
                // Fresh note. Force-terminate voice 0 so the Synthesiser picks V0
                // again (it allocates the first inactive voice by iteration order).
                // This guarantees every legato phrase uses the same voice, avoiding
                // the voice-pool ping-pong that caused inconsistent glide across
                // loop iterations.
                if (mVoices[0] != nullptr)
                    mVoices[0]->stopNote (0.0f, false);

                mSynth.noteOn (ch, note, vel);
                mLegatoVoice     = mVoices[0];
                mLegatoSynthNote = note;
            }
        }
        else if (msg.isNoteOff())
        {
            const int note = msg.getNoteNumber();
            const int ch   = msg.getChannel();
            auto it = std::find (mLegatoHeldNotes.begin(), mLegatoHeldNotes.end(), note);
            if (it != mLegatoHeldNotes.end())
                mLegatoHeldNotes.erase (it);

            if (mLegatoHeldNotes.empty())
            {
                // Last note released. Synth thinks voice plays mLegatoSynthNote
                // (the original pass-through note). Release THAT note synchronously.
                if (mLegatoSynthNote >= 0)
                    mSynth.noteOff (ch, mLegatoSynthNote, 0.0f, true);
                mLegatoVoice     = nullptr;
                mLegatoSynthNote = -1;
            }
            else if (mLegatoVoice != nullptr && mLegatoVoice->isVoiceActive())
            {
                // Retarget to the most recently held note still down.
                // Do NOT pass the user's noteOff through - synth still owns the
                // original note and would release the voice prematurely.
                mLegatoVoice->retargetLegato (mLegatoHeldNotes.back());
            }
        }
        else
        {
            pass.addEvent (msg, meta.samplePosition);
        }
    }

    midi.swapWith (pass);
}


//==============================================================================
// ── Setters - propagate to all voices ────────────────────────────────────────

void BaySickSynthDSP::setWaveform (BssWaveform w)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setWaveform (w);
}

void BaySickSynthDSP::setTranspose (int semitones)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setTranspose (semitones);
}

void BaySickSynthDSP::setModifier (float m)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setModifier (m);
}

void BaySickSynthDSP::setDualOscMode (int mode)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setDualOscMode (mode);
}

void BaySickSynthDSP::setOscSync (bool on)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setOscSync (on);
}

void BaySickSynthDSP::setRingMod (bool on)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setRingMod (on);
}

void BaySickSynthDSP::setDriftAmount (float amount)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setDriftAmount (amount);
}

void BaySickSynthDSP::setUnisonVoices (int voices)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setUnisonVoices (voices);
}

void BaySickSynthDSP::setUnisonDetune (float amount)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setUnisonDetune (amount);
}

void BaySickSynthDSP::setUnisonSpread (float amount)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setUnisonSpread (amount);
}

void BaySickSynthDSP::setNoiseLevel (float n)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setNoiseLevel (n);
}

void BaySickSynthDSP::setNoiseOnlyMode (bool on)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setNoiseOnlyMode (on);
}

void BaySickSynthDSP::setNoiseColor (int color)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setNoiseColor (color);
}

void BaySickSynthDSP::setGlideTime (float secs)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setGlideTime (secs);
}

void BaySickSynthDSP::setVoiceMode (BssVoiceMode mode)
{
    mVoiceMode = mode;
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setVoiceMode (mode);
}

void BaySickSynthDSP::setCutSelf (bool on)
{
    mCutSelf = on;
}

void BaySickSynthDSP::setCutSelfMode (bool cutAll)
{
    mCutAll = cutAll;
}

void BaySickSynthDSP::setModWheelDest (int dest)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setModWheelDest (dest);
}

void BaySickSynthDSP::setModWheelAmt (float amt)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setModWheelAmt (amt);
}

void BaySickSynthDSP::setAmpEnv (float a, float d, float s, float r)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setAmpEnv (a, d, s, r);
}

void BaySickSynthDSP::setVelAmpTrack (float amount)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setVelAmpTrack (amount);
}

void BaySickSynthDSP::setPitchEnv (float a, float d, float s, float r)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setPitchEnv (a, d, s, r);
}

void BaySickSynthDSP::setPitchEnvAmt (float semitones)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setPitchEnvAmt (semitones);
}

void BaySickSynthDSP::setTransientAmount (float amount)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setTransientAmount (amount);
}

void BaySickSynthDSP::setTransientDuration (float ms)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setTransientDuration (ms);
}

void BaySickSynthDSP::setTransientColour (float hz)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setTransientColour (hz);
}

void BaySickSynthDSP::setBurstMode (bool on)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setBurstMode (on);
}

void BaySickSynthDSP::setBurstCount (int count)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setBurstCount (count);
}

void BaySickSynthDSP::setBurstSpacing (float ms)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setBurstSpacing (ms);
}

void BaySickSynthDSP::setFilterType (BssFilterType type)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setFilterType (type);
}

void BaySickSynthDSP::setFilterCutoff (float hz)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setFilterCutoff (hz);
}

void BaySickSynthDSP::setFilterRes (float res)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setFilterRes (res);
}

void BaySickSynthDSP::setFilterEnvAmt (float amount)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setFilterEnvAmt (amount);
}

void BaySickSynthDSP::setFilterKbTrack (float amount)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setFilterKbTrack (amount);
}

void BaySickSynthDSP::setFilterVelTrack (float amount)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setFilterVelTrack (amount);
}

void BaySickSynthDSP::setFltEnv (float a, float d, float s, float r)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setFltEnv (a, d, s, r);
}

void BaySickSynthDSP::setLFOShape (LFOShape shape)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setLFOShape (shape);
}

void BaySickSynthDSP::setLFODest (BssLFODest dest)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setLFODest (dest);
}

void BaySickSynthDSP::setLFORate (float hz)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setLFORate (hz);
}

void BaySickSynthDSP::setLFOAmount (float amount)
{
    for (int i = 0; i < kNumVoices; ++i) mVoices[i]->setLFOAmount (amount);
}
