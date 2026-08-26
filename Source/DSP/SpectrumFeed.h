#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <algorithm>

// ─── SpectrumFeed ────────────────────────────────────────────────────────────
// Lock-free, wait-free seqlock ring for moving one block of audio samples from
// the audio thread to the UI thread for spectrum-analyser display.
// The audio thread calls push() every block; the UI thread polls at ~30 Hz.
// If the UI thread happens to poll mid-write, it drops the frame (fine for a
// visualiser) rather than copying torn data.
//
// Extracted from BaySickGraph for reuse by DSP modules that want to expose their
// own pre/post spectrum taps (the master analyzer's taps still ride this;
// the EQ window's feeds moved to the vendored kbs SpectrumFeed).
// `BaySickGraph::SpectrumFeed` is retained as a `using` alias for back-compat.
// ─────────────────────────────────────────────────────────────────────────────
struct SpectrumFeed
{
    static constexpr int kSize = 1024;

    // Audio thread - called every block.
    void push(const float* src, int n)
    {
        mSeq.fetch_add(1, std::memory_order_release);   // odd  = writing
        mCount = juce::jmin(n, kSize);
        std::copy(src, src + mCount, mData);
        mSeq.fetch_add(1, std::memory_order_release);   // even = done
    }

    // UI thread (30 Hz timer) - copies into caller's buffer.
    // Returns false if audio was writing mid-copy (frame dropped - fine for a visualiser).
    bool poll(float* outData, int& outCount)
    {
        uint32_t s1 = mSeq.load(std::memory_order_acquire);
        if (s1 & 1u) return false;

        outCount = mCount;
        std::copy(mData, mData + outCount, outData);

        uint32_t s2 = mSeq.load(std::memory_order_acquire);
        return s1 == s2;
    }

private:
    std::atomic<uint32_t> mSeq   { 0 };
    float                 mData[kSize] {};
    int                   mCount { 0 };
};
