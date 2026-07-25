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
// Voices that stash a per-note glide source implement this so BroadcastSynthesiser
// can wipe the stash on every voice after a noteOn.  CC84 (glide source) is
// broadcast to ALL voices but only the started voice consumes it, so without this
// an idle voice keeps a stale source note and mis-glides a later Standard note
// allocated to it (QA-H).
struct GlideStashClearable
{
    virtual ~GlideStashClearable() = default;
    virtual void clearGlideStash() = 0;
};

// #36 (QA-G3Smoke): first-match CC85 takeover.  Two voices holding the SAME
// note number both matched the CC84 anchor and both retargeted (double bend).
// Voices implement this; handleController dispatches CC85 one voice at a time
// in voice order and stops at the first acceptor, then wipes every voice's
// one-shot stash (the wipe formerly lived in each voice's CC85 branch tail).
struct RampTakeoverAcceptor
{
    virtual ~RampTakeoverAcceptor() = default;
    virtual bool tryRampTakeover (int targetNote) = 0;
};

struct BroadcastSynthesiser : public juce::Synthesiser
{
    void handleController (int /*midiChannel*/, int controllerNumber,
                           int controllerValue) override
    {
        if (controllerNumber == 85)   // #36: first-match ramp takeover
        {
            bool claimed = false;
            for (int i = 0; i < getNumVoices(); ++i)
                if (auto* r = dynamic_cast<RampTakeoverAcceptor*> (getVoice (i)))
                    if (! claimed && r->tryRampTakeover (controllerValue))
                        claimed = true;
            for (int i = 0; i < getNumVoices(); ++i)
                if (auto* g = dynamic_cast<GlideStashClearable*> (getVoice (i)))
                    g->clearGlideStash();
            return;
        }
        for (int i = 0; i < getNumVoices(); ++i)
            if (auto* v = getVoice (i))
                v->controllerMoved (controllerNumber, controllerValue);
    }

    void noteOn (int midiChannel, int midiNoteNumber, float velocity) override
    {
        juce::Synthesiser::noteOn (midiChannel, midiNoteNumber, velocity);
        // The started voice already cleared its own glide stash at startNote;
        // wipe the rest so a broadcast CC84 arm can't linger on an idle voice and
        // mis-glide the next Standard note.  (CC85 ramp slides clear every voice
        // themselves, so this only matters after a porta/slide noteOn.)
        for (int i = 0; i < getNumVoices(); ++i)
            if (auto* g = dynamic_cast<GlideStashClearable*> (getVoice (i)))
                g->clearGlideStash();
    }
};
