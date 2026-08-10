#include "MidiRecorder.h"

// Matches the upper clamp in VibeSynthProcessor::settleAudioThread.  One block
// is 46 ms at 2048 samples / 44.1 kHz, so this leaves several blocks of
// headroom without stalling the message thread for a visible beat.
static constexpr juce::uint32 kSettleTimeoutMs = 250;

// THREAD SAFETY: quiescence wait on the mBlockInFlight gate -- see the header.
// Callers MUST have stored mRecording=false first: that store stops the next
// block from entering, this wait outlasts the one that may already be inside.
void MidiRecorder::settleAudioBlock() noexcept
{
    const juce::uint32 t0 = juce::Time::getMillisecondCounter();

    while (mBlockInFlight.load (std::memory_order_seq_cst))
    {
        if ((juce::uint32) (juce::Time::getMillisecondCounter() - t0) >= kSettleTimeoutMs)
            break;

        juce::Thread::sleep (1);
    }
}

void MidiRecorder::startRecording(double startBeat)
{
    // THREAD SAFETY: close the audio thread out before touching the vectors.
    // Arming a second take without an intervening stop would otherwise clear
    // and re-reserve them while a block is mid-push_back.
    mRecording.store (false, std::memory_order_seq_cst);
    settleAudioBlock();

    mActive.clear();
    mCompleted.clear();

    // Capacity is established HERE, on the message thread, because the
    // push_backs in processBlock run on the audio thread where a realloc is a
    // real-time violation.  stopRecording's std::move hands mCompleted's buffer
    // away, so every take has to re-reserve at arm time.
    mActive.reserve (128);
    mCompleted.reserve (8192);

    mElapsedBeats = 0.0;   // restart the recorder's count-in-inclusive clock
    // seq_cst, not plain release: this store is part of the mBlockInFlight
    // handshake's total order, and a store outside that order could still be
    // read by a gate test that already saw the matching stop.
    mRecording.store(true, std::memory_order_seq_cst);
    (void)startBeat;
}

void MidiRecorder::processBlock(const juce::MidiBuffer& midi,
                                 int numSamples,
                                 double beatsPerSample)
{
    // THREAD SAFETY: announce presence BEFORE testing the arm flag, both
    // seq_cst so the store cannot sink past the load.  Reversed or relaxed,
    // stopRecording can observe an idle recorder in the gap between this
    // thread's gate test and its first push_back, and drain the vectors out
    // from under it.  Every exit below must lower the flag again.
    mBlockInFlight.store (true, std::memory_order_seq_cst);

    if (!mRecording.load(std::memory_order_seq_cst))
    {
        mBlockInFlight.store (false, std::memory_order_release);
        return;
    }

    const double beatStart = mElapsedBeats;   // recorder's own clock, not the playhead
    for (const auto& meta : midi)
    {
        const auto msg = meta.getMessage();
        double beatPos = beatStart + meta.samplePosition * beatsPerSample;

        if (msg.isNoteOn())
        {
            mActive.push_back({ msg.getNoteNumber(), beatPos,
                                msg.getFloatVelocity() });
        }
        else if (msg.isNoteOff())
        {
            int note = msg.getNoteNumber();
            for (int i = (int)mActive.size() - 1; i >= 0; --i)
            {
                if (mActive[i].midiNote == note)
                {
                    PianoNote pn;
                    pn.midiNote      = note;
                    pn.startBeat     = mActive[i].startBeat;
                    pn.durationBeats = juce::jmax(1.0 / 32.0,
                                                  beatPos - mActive[i].startBeat);
                    pn.velocity      = mActive[i].velocity;
                    mCompleted.push_back(pn);
                    mActive.erase(mActive.begin() + i);
                    break;
                }
            }
        }
    }

    // Advance the recorder's clock by this block.  Runs every block from arm,
    // so the count-in bar occupies [0, preRollBeats) of the recorded timeline
    // and the commit's pre-roll trim + noodling/early-strike rules line up.
    mElapsedBeats += (double) numSamples * beatsPerSample;

    // Release pairs with settleAudioBlock's load: once the message thread sees
    // this, every push_back above is visible to it.
    mBlockInFlight.store (false, std::memory_order_release);
}

std::vector<PianoNote> MidiRecorder::stopRecording()
{
    // THREAD SAFETY: gate, then settle, THEN drain -- in that order.  The store
    // alone only stops the next block; a block already past the gate is still
    // push_backing into mActive / mCompleted, which the loop below iterates and
    // whose buffer the move at the end steals.
    mRecording.store(false, std::memory_order_seq_cst);
    settleAudioBlock();

    // Close any still-held notes with a 1/8-beat duration
    for (auto& a : mActive)
    {
        PianoNote pn;
        pn.midiNote      = a.midiNote;
        pn.startBeat     = a.startBeat;
        pn.durationBeats = 0.125;
        pn.velocity      = a.velocity;
        mCompleted.push_back(pn);
    }
    mActive.clear();

    return std::move(mCompleted);
}
