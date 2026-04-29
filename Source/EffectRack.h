#pragma once
#include <JuceHeader.h>
#include "DSP/DSPBase.h"

// ── EffectType ────────────────────────────────────────────────────────────────
enum class EffectType
{
    None = 0,
    Compressor,
    Reverb,
    Chorus,
    Delay,
    Saturation,
    Flanger,
    Overdrive,
    Phaser,
    TransientShaper,
    Tape,
    Limiter,
};

// ── EffectRack ────────────────────────────────────────────────────────────────
// Holds 6 hot-swappable DSP slots.  Slot content can be changed at any time;
// the old effect is deleted on the message thread after the audio thread has
// already switched to the new one (pointer swap via juce::AbstractFifo handoff).
// ─────────────────────────────────────────────────────────────────────────────

class EffectRack
{
public:
    static constexpr int kNumSlots = 6;

    struct Slot
    {
        std::unique_ptr<DSPBase> effect;
        EffectType               type     { EffectType::None };
        bool                     bypassed { false };
        float                    outputGainDb { 0.f };     // per-slot post-effect gain
        // Level feeds for meters (written audio thread, read UI thread — accept glitches)
        std::atomic<float> inputLevelRms  { 0.f };    // 0..1 linear RMS
        std::atomic<float> outputLevelDb  { -96.f };  // dBFS output level

        Slot() = default;
        Slot(Slot&& o) noexcept
            : effect(std::move(o.effect)), type(o.type), bypassed(o.bypassed),
              outputGainDb(o.outputGainDb)
        {
            inputLevelRms.store(o.inputLevelRms.load());
            outputLevelDb.store(o.outputLevelDb.load());
        }
        Slot& operator=(Slot&& o) noexcept
        {
            if (this != &o) {
                effect = std::move(o.effect);
                type = o.type; bypassed = o.bypassed; outputGainDb = o.outputGainDb;
                inputLevelRms.store(o.inputLevelRms.load());
                outputLevelDb.store(o.outputLevelDb.load());
            }
            return *this;
        }
    };

    EffectRack();
    ~EffectRack() = default;

    // ── Slot management (call from message thread) ────────────────────────────
    // Load a new effect into a slot. Calls prepare() immediately.
    void loadEffect(int slot, EffectType type);

    // Clear a slot (set to None / nullptr)
    void clearSlot(int slot);

    // Swap slot order (move slot up/down in the chain)
    void moveSlotUp  (int slot);
    void moveSlotDown(int slot);

    // Compact non-empty slots toward slot 0 (call after removing a slot)
    void packSlotsToTop();

    // Bypass / un-bypass a single slot
    void setSlotBypassed(int slot, bool bypassed);

    // Per-slot output gain (dB, applied after effect, before next slot)
    void  setSlotOutputGain(int slot, float db);
    float getSlotOutputGain(int slot) const;

    // Level accessors (UI thread reads, audio thread writes)
    float getSlotInputLevel (int slot) const;   // 0..1 linear RMS
    float getSlotOutputLevel(int slot) const;   // dBFS

    // FX master switch — bypasses entire rack without destroying slot data
    void setRackBypassed(bool bypass) { mRackBypassed = bypass; }
    bool isRackBypassed() const       { return mRackBypassed; }

    // Sum of getLatencySamples() across all active, non-bypassed slots.
    // Returns 0 when rack is bypassed. Used by VibeGraph for PDC.
    int getTotalLatencySamples() const;

    // Read-only slot access
    const Slot& getSlot(int slot) const { return mSlots[slot]; }
    EffectType  getSlotType(int slot)   const { return mSlots[slot].type; }
    bool        isSlotBypassed(int slot) const { return mSlots[slot].bypassed; }

    // ── Audio thread ──────────────────────────────────────────────────────────
    void prepare(double sampleRate, int maxBlockSize);
    void process(juce::AudioBuffer<float>& buffer);
    void reset();

    // BPM propagated to all time-based effects
    void setHostBPM(double bpm);

    // ── Serialization ─────────────────────────────────────────────────────────
    void getStateInformation(juce::MemoryBlock& dest);
    void setStateInformation(const void* data, int sz);

private:
    std::array<Slot, kNumSlots> mSlots;

    bool   mRackBypassed  { false };
    double mSampleRate    { 44100.0 };
    int    mMaxBlock      { 512 };
    double mHostBPM       { 120.0 };

    // Guards slot mutation (loadEffect/clearSlot/moveSlot*/packSlotsToTop/
    // setStateInformation) against audio-thread reads in process(). Audio
    // thread try-locks and skips the block if busy — one silent block is far
    // better than a use-after-free on an effect's destructor racing with
    // process().
    juce::SpinLock mSlotsLock;

    // Factory: creates the DSPBase subclass for a given EffectType
    std::unique_ptr<DSPBase> createEffect(EffectType type);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectRack)
};
