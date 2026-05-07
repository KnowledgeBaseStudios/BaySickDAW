#include "CompositeAudioInsertTask.h"
#include "../../PluginProcessor.h"
#include "../../PatternManager.h"
#include "../../VibeGraph.h"
#include "../../DSP/EngineSidechainHelper.h"
#include "../SidechainPullHelper.h"

#include <atomic>
#include <limits>

CompositeAudioInsertTask::CompositeAudioInsertTask (int                 row,
                                                    int                 channelIdIn,
                                                    VibeGraph&          graph,
                                                    VibeSynthProcessor& processor)
    : mIndex (row),
      mGraph (&graph),
      mProcessor (&processor)
{
    this->channelId = channelIdIn;
}

void CompositeAudioInsertTask::setClipEngine (juce::AudioProcessor* engine)
{
    mClipEngine.store (engine, std::memory_order_release);
    mScEngine .store (dynamic_cast<ISidechainEngine*> (engine),
                      std::memory_order_release);
}

juce::AudioBuffer<float>& CompositeAudioInsertTask::getClipScratch (int numChannels,
                                                                    int numSamples)
{
    if (mClipScratch.getNumChannels() != numChannels
        || mClipScratch.getNumSamples() < numSamples)
    {
        mClipScratch.setSize (numChannels, numSamples,
                              /*keepExistingContent*/ false,
                              /*clearExtraSpace*/    false,
                              /*avoidReallocating*/  true);
    }
    return mClipScratch;
}

void CompositeAudioInsertTask::run()
{
    // Filled in Task 2.
}
