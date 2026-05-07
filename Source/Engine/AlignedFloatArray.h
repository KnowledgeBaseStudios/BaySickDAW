#pragma once

#include <JuceHeader.h>
#include <cstdlib>
#include <cstddef>
#include <cstring>

#if JUCE_WINDOWS
 #include <malloc.h>
#endif

// RAII wrapper around a 64-byte-aligned float allocation.
//
// Used by ChannelBufferArena to back per-strip output buffers without false
// sharing between adjacent strips. 64-byte alignment matches the cache line
// size on every modern x86_64 CPU (Intel + AMD), so two workers writing to
// adjacent allocations never trigger an MESI bounce.
//
// NOT real-time safe - allocate / free hit the OS allocator. Call from the
// message thread only (typically in prepareToPlay paths).
class AlignedFloatArray
{
public:
    AlignedFloatArray() = default;
    ~AlignedFloatArray() { freeStorage(); }

    AlignedFloatArray (const AlignedFloatArray&)            = delete;
    AlignedFloatArray& operator= (const AlignedFloatArray&) = delete;

    AlignedFloatArray (AlignedFloatArray&& other) noexcept
        : mData (other.mData), mSize (other.mSize)
    {
        other.mData = nullptr;
        other.mSize = 0;
    }

    AlignedFloatArray& operator= (AlignedFloatArray&& other) noexcept
    {
        if (this != &other)
        {
            freeStorage();
            mData       = other.mData;
            mSize       = other.mSize;
            other.mData = nullptr;
            other.mSize = 0;
        }
        return *this;
    }

    // (Re)allocate `numFloats` floats with 64-byte alignment. Storage is
    // zero-initialised. Calling with a smaller size frees the existing
    // allocation; calling with the same size is a no-op.
    void allocate (size_t numFloats)
    {
        if (numFloats == mSize && mData != nullptr)
            return;

        freeStorage();
        if (numFloats == 0)
            return;

        constexpr size_t kAlign = 64;
        const size_t requested = numFloats * sizeof (float);
        const size_t bytes     = ((requested + kAlign - 1) / kAlign) * kAlign;

       #if JUCE_WINDOWS
        mData = static_cast<float*> (_aligned_malloc (bytes, kAlign));
       #else
        // std::aligned_alloc (C++17) requires the size to be a multiple of
        // the alignment, which we've already rounded up above.
        mData = static_cast<float*> (std::aligned_alloc (kAlign, bytes));
       #endif

        if (mData != nullptr)
        {
            mSize = numFloats;
            std::memset (mData, 0, bytes);
        }
    }

    float*       data()       noexcept { return mData; }
    const float* data() const noexcept { return mData; }
    size_t       size() const noexcept { return mSize; }
    bool         isAllocated() const noexcept { return mData != nullptr; }

private:
    void freeStorage() noexcept
    {
        if (mData == nullptr)
            return;

       #if JUCE_WINDOWS
        _aligned_free (mData);
       #else
        std::free (mData);
       #endif

        mData = nullptr;
        mSize = 0;
    }

    float* mData = nullptr;
    size_t mSize = 0;
};
