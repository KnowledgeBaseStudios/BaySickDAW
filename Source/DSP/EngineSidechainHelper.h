#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <cmath>

// ─── ISidechainEngine ───────────────────────────────────────────────────────
// Tag interface inherited by all engine processors that consume engine-level
// sidechain (Harmless / BaySickSynth / BaySickBass / VibePlayer / BaySickNAMIR
// + future: BaySickPedals (Phase I), BaySickDrumKit, etc).  PluginProcessor's
// render loop dynamic_casts the AudioProcessor* before each engine's
// processBlock and pushes the strip's SC array via setSidechainBuffers.
// ────────────────────────────────────────────────────────────────────────────
class ISidechainEngine
{
public:
    virtual ~ISidechainEngine() = default;
    virtual void setSidechainBuffers (juce::AudioBuffer<float>* const* bufs, int count) noexcept = 0;
    virtual float getSidechainLevel() const noexcept = 0;
};

// ─── EngineSidechainHelper ──────────────────────────────────────────────────
// C.4 Phase 2.2 (2026-04-30): per-engine sidechain primitive.  Each player
// engine processor (Harmless / BaySickSynth / BaySickBass / VibePlayer /
// BaySickNAMIR) holds one of these as a member.  PluginProcessor's render
// loop pushes the engine's host InsertNode SC array via setSidechainBuffers
// before processBlock; the engine calls updateLevel(numSamples) inside
// processBlock to publish a current RMS level via getLevel(), which any
// internal modulation source on the engine can sample.
//
// Address-only push: the actual sample data is owned by VibeGraph's per-strip
// scRecv buffers, filled by routeInsertOutput's SC fanout from upstream
// sources earlier in the topo-sorted process order.
//
// Per-engine internal consumers (UI routing, modulation depth, etc.) are
// out of scope for this phase - this helper is the primitive only.
// ────────────────────────────────────────────────────────────────────────────
class EngineSidechainHelper
{
public:
    static constexpr int kMaxSlots = 4;   // matches RoutingGraph::kMaxScRecvsPerStrip

    // C.4 Phase 2.2 fix (2026-05-01): COPY pointers into our own fixed-size
    // array.  Caller previously passed a stack-local array; storing its
    // pointer caused a dangling-pointer crash on the next audio block once
    // the lambda that built it had returned.  Now we hold our own copy.
    void setSidechainBuffers (juce::AudioBuffer<float>* const* bufs, int count) noexcept
    {
        const int n = juce::jlimit (0, kMaxSlots, count);
        for (int i = 0; i < n; ++i)
            mBufs[i] = bufs[i];
        for (int i = n; i < kMaxSlots; ++i)
            mBufs[i] = nullptr;
        mCount = n;
    }

    // Compute a peak-of-receivers RMS level across all connected SC slots
    // for the current block.  O(numSlots * numChans * numSamples) - cheap
    // when count is 0 (early exit) or no slot has data.
    void updateLevel (int numSamples) noexcept
    {
        if (mCount <= 0 || numSamples <= 0)
        {
            mLevel.store (0.0f, std::memory_order_relaxed);
            return;
        }

        float maxRms = 0.0f;
        for (int i = 0; i < mCount; ++i)
        {
            const auto* b = mBufs[i];
            if (b == nullptr || b->getNumSamples() < numSamples) continue;
            const int chCount = juce::jmin (2, b->getNumChannels());
            if (chCount <= 0) continue;

            float sumSq = 0.0f;
            for (int ch = 0; ch < chCount; ++ch)
            {
                const float* d = b->getReadPointer (ch);
                for (int s = 0; s < numSamples; ++s) sumSq += d[s] * d[s];
            }
            const float rms = std::sqrt (sumSq / (float) (numSamples * chCount));
            if (rms > maxRms) maxRms = rms;
        }
        mLevel.store (maxRms, std::memory_order_relaxed);
    }

    // UI-thread / modulation-source readable per-block RMS level (0..~1).
    float getLevel() const noexcept { return mLevel.load (std::memory_order_relaxed); }

    // Direct buffer access for engine internals that want raw SC samples
    // rather than the pre-computed level (e.g. a per-sample env follower).
    juce::AudioBuffer<float>* const* getBuffers() const noexcept { return mBufs; }
    int                              getCount()   const noexcept { return mCount; }

private:
    juce::AudioBuffer<float>* mBufs[kMaxSlots] { nullptr, nullptr, nullptr, nullptr };
    int                       mCount { 0 };
    std::atomic<float>        mLevel { 0.0f };
};
