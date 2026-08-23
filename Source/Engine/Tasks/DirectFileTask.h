#pragma once
#include <JuceHeader.h>
#include "../RenderTask.h"
#include "../BlockContext.h"
#include <memory>

class BaySickGraph;
class AudioClipStreamer;

// QA-TrueLevel SC-10 (Jeff, 2026-08-22): a Direct to Master strip's render
// task.  No engine and no page -- the strip's input IS a file, read at the
// transport's sample position from bar 1 whenever the transport plays, then
// run through the strip's insert chain (rack / EQ / fader / pan) and routed
// like any insert (its _sendTo is locked to the master).  Same read shape as a
// frozen tab's playback (FrozenSourceRead.h); the streamer is owned here
// because nothing else refers to it.
class DirectFileTask : public RenderTask
{
public:
    DirectFileTask (int index, int channelIdIn, BaySickGraph& graph,
                    std::unique_ptr<AudioClipStreamer> streamer);
    ~DirectFileTask() override;

    void run() override;

    AudioClipStreamer* getStreamer() const noexcept { return mStreamer.get(); }

private:
    int                                mIndex = 0;
    BaySickGraph*                      mGraph = nullptr;
    std::unique_ptr<AudioClipStreamer> mStreamer;
};
