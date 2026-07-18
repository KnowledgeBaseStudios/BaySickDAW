#pragma once
#include <JuceHeader.h>

// QA-H per-note expression transport: the piano-roll scheduler emits CC
// 10/71/72/74 (+ 84/5/37/85 for slide/porta) immediately before each noteOn
// and voices stash-then-consume them at startNote.  juce::Synthesiser's
// default handleController only reaches voices whose
// currentPlayingMidiChannel matches - idle voices are channel 0 (cleared or
// never started), so whenever the next noteOn allocated a cold voice the
// stash was missed (the same hole Batch E's CC74 cutoff shipped with).
// This subclass broadcasts controller events to every voice unconditionally;
// pitch wheel keeps the stock path (juce replays the channel wheel state at
// startVoice, so it has no such hole).
struct BroadcastSynthesiser : public juce::Synthesiser
{
    void handleController (int /*midiChannel*/, int controllerNumber,
                           int controllerValue) override
    {
        for (int i = 0; i < getNumVoices(); ++i)
            if (auto* v = getVoice (i))
                v->controllerMoved (controllerNumber, controllerValue);
    }
};
