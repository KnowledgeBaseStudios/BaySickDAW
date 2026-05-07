#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

#include "AlignedFloatArray.h"
#include "RenderEngineFlags.h"

// Pre-allocated, cache-aligned arena of stereo output buffers, one per strip
// channel id (matches MixerChannelIds id space, 0 .. kMaxStripChannels - 1).
//
// Layout:
//   for each strip i:
//     [ ch0 samples (alignedSamples floats) ]
//     [ ch1 samples (alignedSamples floats) ]
//     [ 16-float guard region ]
//
// `alignedSamples` is `maxBlockSize` rounded up to a multiple of 16 floats
// (= 64 bytes), so each channel's float* lands on a cache-line boundary. The
// guard region between strips ensures the boundary itself never spans a
// shared cache line. Combined: two workers writing to adjacent strips' sample
// data cannot trigger false sharing.
//
// (False sharing on the dependency-counter atomic is handled separately by
// alignas(64) on the counter inside RenderTask.)
//
// Lifetime
// --------
// Constructed in PluginProcessor as a value member. prepare() (re)allocates
// when the host changes block size. releaseResources is a no-op - the arena
// stays alive for the plugin's lifetime to avoid allocator churn during
// sample-rate / buffer-size transitions.
class ChannelBufferArena
{
public:
    static constexpr int kChannelsPerStrip = 2;

    // (Re)allocate to fit `maxBlockSize` samples per channel. Must NOT be
    // called on the audio thread. Calling with the same size is a no-op.
    void prepare (int maxBlockSize)
    {
        if (maxBlockSize <= 0)
            return;

        if (maxBlockSize == mMaxBlockSize && mArena.isAllocated())
            return;

        // Round samples per channel up to a multiple of 16 floats (64 bytes).
        const int alignedSamples = ((maxBlockSize + 15) / 16) * 16;

        // Add a 16-float guard between adjacent strips so the boundary never
        // spans a shared cache line.
        const int floatsPerStrip = alignedSamples * kChannelsPerStrip + 16;

        const size_t totalFloats = static_cast<size_t> (floatsPerStrip)
                                 * static_cast<size_t> (RenderEngine::kMaxStripChannels);

        mArena.allocate (totalFloats);
        mMaxBlockSize   = maxBlockSize;
        mAlignedSamples = alignedSamples;
        mFloatsPerStrip = floatsPerStrip;

        rebuildViews();
    }

    // Returns a JUCE AudioBuffer view onto strip `channelId`'s storage, or
    // nullptr if the id is out of range or the arena hasn't been prepared.
    // The returned pointer remains valid until the next prepare() call.
    juce::AudioBuffer<float>* getStripBuffer (int channelId) noexcept
    {
        if (channelId < 0 || channelId >= RenderEngine::kMaxStripChannels)
            return nullptr;
        if (! mArena.isAllocated())
            return nullptr;
        return &mViews[(size_t) channelId];
    }

    // Zero a strip's storage. RT-safe (no allocation).
    void clearStripBuffer (int channelId) noexcept
    {
        if (auto* buf = getStripBuffer (channelId))
            buf->clear();
    }

    int getMaxBlockSize() const noexcept { return mMaxBlockSize; }

private:
    void rebuildViews()
    {
        // juce::AudioBuffer::setDataToReferTo stores the pointer-to-array
        // (NOT a copy), so the channel-pointer array must outlive the view.
        // We keep one stable std::array per strip in mChannelPtrs and hand
        // the AudioBuffer its address there.
        mChannelPtrs.resize ((size_t) RenderEngine::kMaxStripChannels);
        mViews      .resize ((size_t) RenderEngine::kMaxStripChannels);

        for (int i = 0; i < RenderEngine::kMaxStripChannels; ++i)
        {
            float* base = mArena.data() + (size_t) i * (size_t) mFloatsPerStrip;
            mChannelPtrs[(size_t) i][0] = base;
            mChannelPtrs[(size_t) i][1] = base + mAlignedSamples;

            mViews[(size_t) i].setDataToReferTo (mChannelPtrs[(size_t) i].data(),
                                                 kChannelsPerStrip,
                                                 mMaxBlockSize);
        }
    }

    AlignedFloatArray                                  mArena;
    std::vector<std::array<float*, kChannelsPerStrip>> mChannelPtrs;
    std::vector<juce::AudioBuffer<float>>              mViews;
    int mMaxBlockSize   = 0;
    int mAlignedSamples = 0;
    int mFloatsPerStrip = 0;
};
