#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <unordered_map>

// ── SlideSampleCache ─────────────────────────────────────────────────────────
// QA-SlideSampler Task 2/3: the shared decoded-sample store behind the blended
// slide.  One decoded mono buffer per unique wav PATH, shared across every
// SlideSampler (so N tabs on the same patch cost ONE copy, per Jeff's memory
// call).  Held weakly in the map -> freed when the last SlideSampler drops it.
//
// Decoding is SYNCHRONOUS on the caller (option (b), locked 2026-07-21): the
// bands decode inside SlideSampler::setProgram, which runs during loadKit's
// "Loading Instrument..." block, so every band is ready BEFORE the tab can play.
// That guarantees no first-slide decode latency (a mid-play sample-load would be
// an audible musical glitch, worse than a slightly longer load).  Message thread
// only; the audio thread reads DecodedSlideSample::buffer after ready == true
// (release/acquire), which never flips until decodeNow finishes.
//
// Instantiate via juce::SharedResourcePointer so all tabs share one cache.
// ─────────────────────────────────────────────────────────────────────────────

// A decoded mono sample.  `buffer` carries `numFrames` valid frames followed by
// kTailPad zero frames so the interpolator's forward kernel can overread safely.
struct DecodedSlideSample
{
    static constexpr int kTailPad = 32;

    juce::AudioBuffer<float> buffer;      // 1 channel, size = numFrames + kTailPad
    double            sourceRate = 44100.0;
    int               numFrames  = 0;
    std::atomic<bool> ready { false };    // decodeNow publishes true (release)
};

class SlideSampleCache
{
public:
    SlideSampleCache();

    // Message thread: get (or create) the shared handle for a path.  Does NOT
    // decode -- decodeNow fills it.  Live handles are shared across callers.
    std::shared_ptr<DecodedSlideSample> getHandle (const juce::File& wav);

    // Message thread (during loadKit): synchronously decode into the handle if it
    // is not already ready (a handle shared from another tab is decoded once).
    void decodeNow (const std::shared_ptr<DecodedSlideSample>& handle, const juce::File& wav);

private:
    juce::AudioFormatManager mFormats;

    juce::CriticalSection mLock;   // guards mMap (message thread; cheap insurance)
    std::unordered_map<std::string, std::weak_ptr<DecodedSlideSample>> mMap;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlideSampleCache)
};
