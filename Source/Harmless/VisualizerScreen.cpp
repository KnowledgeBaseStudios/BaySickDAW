#include "VisualizerScreen.h"
#include "HarmlessSynth.h"

void VisualizerScreen::tickFromSynth (float* outBuf, int numPartials)
{
    if (mSynth != nullptr)
        mSynth->getAggregatedPartialAmplitudes (outBuf, numPartials);
    else
        std::fill (outBuf, outBuf + numPartials, 0.0f);
}
