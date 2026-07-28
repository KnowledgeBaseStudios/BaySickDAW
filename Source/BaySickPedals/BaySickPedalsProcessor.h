#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include "../EffectRack.h"     // EffectType enum, DSPBase
#include "../DSP/DSPBase.h"
#include "../Standalone/ApvtsDirtyTracker.h"

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPedalsProcessor - Phase I-1 (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// Internal 8-slot guitar/bass pedal rack hosted as the first sub-tab of the
// Inst page (peer to BaySickNAM/IR in the Inst chain).  Architecture mirrors
// EffectRack but with a fixed 8-slot config and slot-position locks:
//
//   * Slot 0 (locked, front):  Tuner  (EffectType::TunerStyle only)
//   * Slots 1-6:                User-adjustable; any non-locked EffectType;
//                               drag-to-reorder, stacking allowed.
//   * Slot 7 (locked position): EQ - exactly one of {GraphicEQStyle,
//                               BassGraphicEQStyle, FurmanEQStyle}.
//
// Slot mutation uses the same wait-free swap-pending pattern as EffectRack
// (I-0a): single-slot loadEffect/clearSlot/setSlotBypassed publish via per-
// slot atomic flag (audio is wait-free); multi-slot moves take a brief
// spinlock (audio try-lock, skip block worst case).
//
// State management:
//   * Per-tab: getStateInformation/setStateInformation embedded in the Inst
//     page's project-level XML.
//   * Standalone library: Pedalboard presets at
//     Documents/BaySickDAW/Presets/Pedalboards/{name}.xml
//
// APVTS:
//   * Prefix `bsp_` (BaySickPedals).
//   * 8x per-slot bypass: `bsp_slot{N}_bypass` (Bool, default false).
//   * No chain-level params (each pedal owns its own gain/output knobs in
//     I-5+ - locked spec call from 2026-05-02 session).
//
// Audio routing: NOT wired into the InsertNode graph yet.  G-9 (post-Phase I)
// adds the wire-up.  For I-1 the processor is instantiable + state roundtrips
// + can be hosted by InstPage's Pedals sub-tab, but no audio passes through it.
// ─────────────────────────────────────────────────────────────────────────────

class BaySickPedalsProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kNumSlots = 8;
    static constexpr int kSlotTuner = 0;          // locked front slot
    static constexpr int kSlotEQ    = kNumSlots - 1;  // locked back slot (== 7)

    // undoMgr: QA-ModelShell TS1 dormant pre-wire -- bound into apvts so
    // QA-UndoCoverage can enable undo without a ctor sweep; unused until then.
    explicit BaySickPedalsProcessor (juce::UndoManager* undoMgr = nullptr);
    ~BaySickPedalsProcessor() override;

    // ── AudioProcessor overrides ──────────────────────────────────────────────
    void prepareToPlay (double sampleRate, int blockSize) override;
    void releaseResources()                              override {}
    void processBlock  (juce::AudioBuffer<float>&,
                        juce::MidiBuffer&)               override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                  { return true; }

    const juce::String getName() const override      { return "BaySickPedals"; }
    bool acceptsMidi()  const override               { return false; }
    bool producesMidi() const override               { return false; }
    bool isMidiEffect() const override               { return false; }
    double getTailLengthSeconds() const override     { return 0.0; }

    int  getNumPrograms() override                   { return 1; }
    int  getCurrentProgram() override                { return 0; }
    void setCurrentProgram (int) override            {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int)   override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
            && layouts.getMainInputChannelSet () == juce::AudioChannelSet::stereo();
    }

    // ── Public state ──────────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;

    // 2026-05-05 dirty-flag wiring (see ApvtsDirtyTracker.h).
    void setOnAnyStateChange (std::function<void()> fn) { mDirtyTracker.onAny = std::move (fn); }
    // 2026-05-05 lifecycle dirty fire - Pedals' loadEffect / clearSlot / moveSlot
    // mutate internal slot state without touching apvts (apvts only holds
    // bypass + per-pedal CC params).  Lifecycle methods call this after the
    // mutation publishes, mirroring the apvts-listener path.
    void fireDirty() noexcept { if (mDirtyTracker.onAny) mDirtyTracker.onAny(); }

    // ── Slot data (mirrors EffectRack::Slot) ──────────────────────────────────
    struct Slot
    {
        std::unique_ptr<DSPBase> active;
        std::unique_ptr<DSPBase> pending;
        std::atomic<bool>        swapPending { false };
        EffectType               type     { EffectType::None };
        // No Slot.bypassed here -- bypass lives in APVTS as `bsp_slot{N}_bypass`
        // (UI binding via attachment).  The audio path reads via apvts.
        // No outputGainDb / scPick yet -- per-pedal master output / per-pedal SC
        // routing land in later batches.

        // QA-ApvtsAutomation: stable per-slot identity for automation paramIds,
        // mirroring EffectRack::Slot::uuid.  Automation keys MUST NOT be the slot
        // index: moveSlot/clearSlot shuffle contents between indices, so an
        // index-keyed lane would silently retarget a different pedal.  The uuid
        // travels with the effect and is persisted, so lanes survive reorder and
        // project reload.  Empty for an empty slot.
        juce::String             uuid;

        Slot() = default;
        Slot(Slot&& o) noexcept
            : active(std::move(o.active)), pending(std::move(o.pending)),
              type(o.type), uuid(std::move(o.uuid))
        {
            swapPending.store (o.swapPending.load());
        }
        Slot& operator=(Slot&& o) noexcept
        {
            if (this != &o)
            {
                active  = std::move(o.active);
                pending = std::move(o.pending);
                type    = o.type;
                uuid    = std::move(o.uuid);
                swapPending.store (o.swapPending.load());
            }
            return *this;
        }
    };

    // ── Slot management (call from message thread) ────────────────────────────
    // Loads `type` into slot if allowed by the slot's locking policy
    // (isEffectAllowedInSlot).  Returns false if disallowed (e.g., trying to
    // load OverdriveStyle into slot 0).
    // uuidOverride restores a saved slot identity (restoreFullState) so automation
    // lanes survive a reload; empty means "fresh identity", which is what a
    // user-facing effect swap wants -- different pedal, different knobs.
    bool loadEffect      (int slot, EffectType type, const juce::String& uuidOverride = {});

    // QA-ApvtsAutomation: stable automation key for this slot's current pedal.
    juce::String getSlotUuid (int slot) const
    {
        return (slot >= 0 && slot < kNumSlots) ? mSlots[(size_t) slot].uuid : juce::String();
    }

    // Clear slot.  Slot 0 (Tuner) and slot 7 (EQ) cannot be cleared --- they
    // always hold their default type.  Returns false for those slots.
    bool clearSlot       (int slot);

    // Move slot's contents from `from` to `to`.  Both indices must be in the
    // user-adjustable range [1, kNumSlots - 2] (i.e., slots 1-6).  Slots 0 and
    // 7 are locked-position and never move.
    bool moveSlot        (int from, int to);

    // True for slots with fixed-position locks (0 = Tuner, 7 = EQ).
    bool isSlotLocked    (int slot) const noexcept
    {
        return slot == kSlotTuner || slot == kSlotEQ;
    }

    // True if `type` may be loaded into `slot` per the locking policy.
    bool isEffectAllowedInSlot (int slot, EffectType type) const noexcept;

    EffectType  getSlotType  (int slot) const noexcept;
    DSPBase*    getSlotEffect(int slot) const noexcept;   // pending if swapPending else active
    bool        isSlotBypassed(int slot) const noexcept;  // reads APVTS

    // QA-OctavePedal PDC (pull model): sum of the non-bypassed slots' inherent
    // DSP latencies (the Octave doubler, drive-pedal oversamplers, ...).  Uses
    // getSlotEffect resolution (pending-aware = the effect that WILL process)
    // and honors bsp_slot{N}_bypass exactly as processBlock does, so the report
    // always matches the live audio path.  Message thread ONLY -- takes
    // mSlotsLock (mirrors moveSlot; the audio thread try-locks + drops on
    // contention, negligible at the 5 Hz solve rate).  Consumed by
    // EngineChainProcessor's Pedals special-case; the 5 Hz updateBusLatencies
    // solve carries slot swaps + bypass flips for free (no trigger plumbing).
    int getChainLatencySamples();

    // ── Pedalboard preset library ─────────────────────────────────────────────
    // Documents/BaySickDAW/Presets/Pedalboards/{name}.xml - full 8-slot snapshot
    // wrapped in <Pedalboard> envelope.  Distinct from per-pedal presets which
    // use the existing EffectPresetIO framework (folder layout
    // Documents/BaySickDAW/Presets/Effects/{TypeName}/...).
    static juce::File pedalboardPresetsRoot();
    bool savePedalboardPreset (const juce::String& name, juce::String& outErr);
    bool loadPedalboardPreset (const juce::File& xml,    juce::String& outErr);
    juce::Array<juce::File> enumeratePedalboardPresets();

    // ── Internal: helpers used by the pedalboard preset roundtrip ────────────
    juce::ValueTree captureFullState() const;       // returns a ValueTree we can serialize
    void            restoreFullState (const juce::ValueTree& state);

    // 2026-05-05 (Bug B fix): editor subscribes to this so a bulk state
    // restore (page preset load, project load, pedalboard preset load) can
    // refresh tiles even when the slot type didn't change but the underlying
    // DSP pointer DID swap.  Without this, the editor's timer-based rebuild
    // skips the slot (type comparison says "no change"), the cached widget
    // bindings stay attached to the now-destroyed DSP, and the user sees
    // stale parameter mappings (EQ frequency where boost should be, etc.).
    std::function<void()> onSlotsExternallyChanged;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // I-0a-style hot-swap helpers (mirror EffectRack).
    void publishPending (int slot, std::unique_ptr<DSPBase> effect, EffectType type);

    std::array<Slot, kNumSlots> mSlots;
    juce::CriticalSection       mLoadLock;     // serialises message-thread mutations
    juce::SpinLock              mSlotsLock;    // multi-slot ops only (audio try-locks)

    double mSampleRate { 44100.0 };
    int    mMaxBlock   { 512 };

    // 2026-05-05 dirty-flag wiring.  Declared LAST so apvts is fully constructed.
    ApvtsDirtyTracker mDirtyTracker { apvts };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickPedalsProcessor)
};
