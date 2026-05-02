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
        // Stable identity for automation paramIds. Generated on loadEffect (and
        // restored from XML on setStateInformation). Travels with the slot via
        // std::swap on moveSlotUp/Down/packSlotsToTop so reorder preserves
        // automation lanes. Empty slots have empty uuid.
        juce::String             uuid;
        // C.4 Phase 1 (2026-04-30): which SC receive line drives this slot's
        // effect (-1 = no SC, 0..3 = strip's SC array index).  Stored on the
        // Slot rather than APVTS because routing config is static (not
        // automatable).  Persisted in get/setStateInformation.
        int                      scPick   { -1 };
        // Level feeds for meters (written audio thread, read UI thread — accept glitches)
        std::atomic<float> inputLevelRms  { 0.f };    // 0..1 linear RMS
        std::atomic<float> outputLevelDb  { -96.f };  // dBFS output level

        Slot() = default;
        Slot(Slot&& o) noexcept
            : effect(std::move(o.effect)), type(o.type), bypassed(o.bypassed),
              outputGainDb(o.outputGainDb), uuid(std::move(o.uuid)), scPick(o.scPick)
        {
            inputLevelRms.store(o.inputLevelRms.load());
            outputLevelDb.store(o.outputLevelDb.load());
        }
        Slot& operator=(Slot&& o) noexcept
        {
            if (this != &o) {
                effect = std::move(o.effect);
                type = o.type; bypassed = o.bypassed; outputGainDb = o.outputGainDb;
                uuid = std::move(o.uuid);
                scPick = o.scPick;
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
    // uuidOverride is used by setStateInformation to restore a saved UUID;
    // user-facing call sites leave it empty so a fresh UUID is generated.
    void loadEffect(int slot, EffectType type, const juce::String& uuidOverride = {});

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

    // C.4 Phase 1 (2026-04-30): per-slot SC receive line pick.  -1 = no SC,
    // 0..3 = strip's SC array index.  Pushed to slot.effect->setSidechainPick
    // each block by setSidechainContext().
    void setSlotSidechainPick (int slot, int pickIdx) noexcept;
    int  getSlotSidechainPick (int slot) const noexcept;
    // Push the strip's SC receive array to the rack.  Stored and forwarded to
    // every loaded slot's effect each process() call.  bufs may contain
    // nullptr entries for unused slots; count is RoutingGraph::kMaxScRecvsPerStrip.
    void setSidechainBuffers  (juce::AudioBuffer<float>* const* bufs, int count) noexcept;

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
    const Slot&  getSlot(int slot) const { return mSlots[slot]; }
    EffectType   getSlotType(int slot)   const { return mSlots[slot].type; }
    bool         isSlotBypassed(int slot) const { return mSlots[slot].bypassed; }
    juce::String getSlotUuid(int slot)   const { return (slot >= 0 && slot < kNumSlots) ? mSlots[slot].uuid : juce::String(); }

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

    // C.4 Phase 1 (2026-04-30): cached SC context.  setSidechainBuffers
    // copies the caller's pointer array into mScArrCopy so the rack owns
    // the storage; process() forwards mScArrCopy.data() to each slot.effect
    // each block.  Lifetime is no longer dependent on the caller's storage.
    std::array<juce::AudioBuffer<float>*, 4> mScArrCopy {};
    juce::AudioBuffer<float>* const*         mScBufs    { nullptr };
    int                                      mScCount   { 0 };

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
