#include "MidiRecorder.h"

void MidiRecorder::startRecording(double startBeat)
{
    mActive.clear();
    mCompleted.clear();
    mElapsedBeats = 0.0;   // restart the recorder's count-in-inclusive clock
    mRecording.store(true, std::memory_order_release);
    (void)startBeat;
}

void MidiRecorder::processBlock(const juce::MidiBuffer& midi,
                                 int numSamples,
                                 double beatsPerSample)
{
    if (!mRecording.load(std::memory_order_acquire)) return;

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
}

std::vector<PianoNote> MidiRecorder::stopRecording()
{
    mRecording.store(false, std::memory_order_release);

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
