#include "MidiRecorder.h"

void MidiRecorder::startRecording(double startBeat)
{
    mActive.clear();
    mCompleted.clear();
    mRecording.store(true, std::memory_order_release);
    (void)startBeat;
}

void MidiRecorder::processBlock(const juce::MidiBuffer& midi,
                                 double beatStart,
                                 double beatsPerSample)
{
    if (!mRecording.load(std::memory_order_acquire)) return;

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
