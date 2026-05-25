#pragma once
#include <JuceHeader.h>
#include "DSP/DSPBase.h"

// ── EffectType ────────────────────────────────────────────────────────────────
// I-1 (2026-05-02): extended with 17 new entries for the BaySickPedals 18-module
// spec.  CS-Style Compressor is NOT a new entry -- it's a 4th Type within the
// existing `Compressor` (added in I-4 via CompressorDSP::Type::CS).  All other
// 17 modules get their own EffectType.  createEffect() returns nullptr for the
// new entries until I-5+ ships the DSP classes; rack slots holding null effects
// are simply skipped on the audio path.
//
// IMPORTANT: keep the existing entries' integer values stable -- saved projects
// store EffectType as int.  New entries append at the end with explicit values.
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
    DeEsser,    // H-6 (2026-05-01) -- vocal chain rack uses this for slot 0

    // I-1: BaySickPedals 18-module spec.  DSP classes land in I-5..I-13;
    // factory returns nullptr until then.  Numeric values explicit so saved
    // projects don't drift if we re-order the enum later.
    BluesDriveStyle      = 100,   // BD - Harmonics - slots 1-6
    // OverdriveStyle (was 101) REMOVED 2026-05-02 -- folded into existing
    // OverdriveDSP as Type::Pedal alongside Type::Rack (mirrors the CS Style
    // fold on CompressorDSP).  Single picker entry "Overdrive"; Mode dropdown
    // exposes "Overdrive (Rack)" vs "Overdrive (Pedal)".  Numeric value 101
    // intentionally left dead so future enum additions don't reuse it (no
    // projects in the wild had Phase I OverdriveStyle data per locked spec).
    DistortionStyle      = 102,   // DS - Harmonics - slots 1-6
    FuzzStyle            = 103,   // FZ - Harmonics - slots 1-6
    NoiseGateStyle       = 104,   // NS - Dynamics  - slots 1-6
    HighGainStyle        = 105,   // MT - Harmonics - slots 1-6
    TunerStyle           = 106,   // TU - Time      - slot 0 (locked)
    AcousticPreampStyle  = 107,   // AD - Time      - slots 1-6
    GraphicEQStyle       = 108,   // GE - Time      - slot 7 (option 1)
    SynthStyle           = 109,   // SY - Modulation- slots 1-6
    OctaveStyle          = 110,   // OC - Harmonics - slots 1-6
    WahStyle             = 111,   // PW - Modulation- slots 1-6
    BassGraphicEQStyle   = 112,   // GEB- Time      - slot 7 (option 2)
    BassCompressorStyle  = 113,   // BC - Dynamics  - slots 1-6 (multi-band)
    BassDriverStyle      = 114,   // BB - Harmonics - slots 1-6 (multi-band)
    BassOverdriveStyle   = 115,   // ODB- Harmonics - slots 1-6
    FurmanEQStyle        = 116,   // EQFH-Time      - slot 7 (option 3)
    AcousticSimulatorStyle = 117, // AC - Modulation - slots 1-6 (added 2026-05-03 alongside AD polish)
    NAMPedalStyle          = 118, // User NAM Pedal - slots 1-6 (loads .nam capture; pedal-specific entry, BaySickPedals only)
};

// ── EffectRack ────────────────────────────────────────────────────────────────
// Holds 6 hot-swappable DSP slots.
//
// Single-slot mutations (loadEffect/clearSlot/setSlotBypassed) use a wait-free
// swap-pending pattern that mirrors BaySickNAMIRProcessor's NAM model swap:
//   * Message thread parks the new effect in `pending` and sets `swapPending`.
//   * Audio thread (top of process()) checks the flag, std::swap()s active and
//     pending, clears the flag.  Old DSP destruction defers to the next
//     message-thread mutation (which overwrites `pending`, dropping its old
//     occupant on the message thread - never on audio).
// Audio is NEVER blocked by single-slot mutations.
//
// Multi-slot mutations (moveSlotUp/Down, packSlotsToTop, setStateInformation)
// still take `mSlotsLock` (juce::SpinLock) because their atomicity spans
// multiple slots; audio's process() try-locks at the top and skips one block
// if a multi-slot op is in flight.  These ops are user-UI-rare, so the
// occasional one-block skip is acceptable.
//
// Message-thread writes are serialized through `mLoadLock` (CriticalSection)
// regardless of single- vs multi-slot scope.
// ─────────────────────────────────────────────────────────────────────────────

class EffectRack
{
public:
    static constexpr int kNumSlots = 6;

    struct Slot
    {
        // Wait-free swap-pending pair (mirrors BaySickNAMIRProcessor pattern).
        // Audio thread reads `active` for processing; message thread writes
        // `pending` and sets `swapPending` to publish.  Audio swaps the two
        // unique_ptrs at top of process() when the flag is set, then clears
        // the flag.  After the swap, `pending` holds the OLD active DSP --
        // its destructor runs on the next message-thread loadEffect on this
        // slot (which overwrites `pending`).
        std::unique_ptr<DSPBase> active;
        std::unique_ptr<DSPBase> pending;
        std::atomic<bool>        swapPending { false };

        EffectType               type     { EffectType::None };
        std::atomic<bool>        bypassed { false };          // audio reads, both threads write
        float                    outputGainDb { 0.f };        // per-slot post-effect gain
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
        // Level feeds for meters (written audio thread, read UI thread).
        // 2026-05-02: split into Run + Snapshot pair so the UI sees end-of-
        // audio-block coherent state across every slot (no mid-rack-process
        // partial-write timing window).  Audio CAS-maxes Run during the
        // rack's process; PluginProcessor's end-of-block promote lifts Run
        // -> Snapshot.  UI exchanges Snapshot in SlotComponent::onVBlank.
        std::atomic<float> inputLevelRms     { 0.f };    // snapshot, UI-visible (peak 0..1)
        std::atomic<float> outputLevelDb     { -96.f };  // snapshot, UI-visible (dBFS)
        std::atomic<float> inputLevelRmsRun  { 0.f };    // running max written by audio
        std::atomic<float> outputLevelDbRun  { -96.f };  // running max written by audio

        // I-5 (2026-05-02): bypass crossfade state.  Audio-thread-only.
        // bypassRampValue is the current wet mix coefficient (0 = fully dry,
        // 1 = fully wet); it ramps toward (1 - bypassed) over kBypassRampMs
        // milliseconds when the bypass flag toggles.  While the ramp is in
        // motion the DSP keeps running and the rack mixes wet+dry; once
        // ramp settles to 0 (fully bypassed) the DSP is skipped to save CPU.
        // Without this, drive pedals at high gain produce big audible clicks
        // on bypass toggle because the dry/wet level mismatch is large.
        // Per-slot scratch buffer holds the dry input snapshot during the
        // crossfade.  Lazy-resized in process() to match the current block.
        float                    bypassRampValue { 1.f };
        juce::AudioBuffer<float> dryScratch;

        Slot() = default;
        Slot(Slot&& o) noexcept
            : active(std::move(o.active)), pending(std::move(o.pending)),
              type(o.type),
              outputGainDb(o.outputGainDb), uuid(std::move(o.uuid)), scPick(o.scPick),
              bypassRampValue(o.bypassRampValue),
              dryScratch(std::move(o.dryScratch))
        {
            swapPending     .store(o.swapPending     .load());
            bypassed        .store(o.bypassed        .load());
            inputLevelRms   .store(o.inputLevelRms   .load());
            outputLevelDb   .store(o.outputLevelDb   .load());
            inputLevelRmsRun.store(o.inputLevelRmsRun.load());
            outputLevelDbRun.store(o.outputLevelDbRun.load());
        }
        Slot& operator=(Slot&& o) noexcept
        {
            if (this != &o) {
                active = std::move(o.active);
                pending = std::move(o.pending);
                type = o.type; outputGainDb = o.outputGainDb;
                uuid = std::move(o.uuid);
                scPick = o.scPick;
                bypassRampValue = o.bypassRampValue;
                dryScratch = std::move(o.dryScratch);
                swapPending     .store(o.swapPending     .load());
                bypassed        .store(o.bypassed        .load());
                inputLevelRms   .store(o.inputLevelRms   .load());
                outputLevelDb   .store(o.outputLevelDb   .load());
                inputLevelRmsRun.store(o.inputLevelRmsRun.load());
                outputLevelDbRun.store(o.outputLevelDbRun.load());
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

    // 2026-05-05 dirty-flag wiring: fired from loadEffect / clearSlot /
    // moveSlotUp / moveSlotDown / packSlotsToTop / setSlotBypassed /
    // setSlotOutputGain / setStateInformation.  StandaloneEditor wires it
    // to ProjectManager::markDirty so rack lifecycle changes flip the
    // project dirty bit (per-slot APVTS edits already do via the main
    // PluginProcessor's listener).  Per-slot bypass also writes APVTS, so
    // its dirty fires twice (harmless - markDirty is idempotent).
    std::function<void()> onSlotsChanged;

    // Per-slot output gain (dB, applied after effect, before next slot)
    void  setSlotOutputGain(int slot, float db);
    float getSlotOutputGain(int slot) const;

    // C.4 Phase 1 (2026-04-30): per-slot SC receive line pick.  -1 = no SC,
    // 0..3 = strip's SC array index.  Pushed to slot's active DSP via setSidechainPick
    // each block by setSidechainContext().
    void setSlotSidechainPick (int slot, int pickIdx) noexcept;
    int  getSlotSidechainPick (int slot) const noexcept;
    // Push the strip's SC receive array to the rack.  Stored and forwarded to
    // every loaded slot's effect each process() call.  bufs may contain
    // nullptr entries for unused slots; count is RoutingGraph::kMaxScRecvsPerStrip.
    void setSidechainBuffers  (juce::AudioBuffer<float>* const* bufs, int count) noexcept;

    // Level accessors (UI thread reads, audio thread writes)
    float getSlotInputLevel (int slot) const;   // 0..1 linear (peak)
    float getSlotOutputLevel(int slot) const;   // dBFS
    // 2026-05-02: drain variants for vblank-locked metering -- exchange the
    // SNAPSHOT atomics with sentinel values and return the values.  Audio
    // CAS-maxes the Run companion atomics; promoteSlotPeakSnapshots() lifts
    // Run -> snapshot at end of each audio block so UI reads see consistent
    // end-of-block state across every slot in every rack.
    float drainSlotInputLevel (int slot);       // 0..1 linear (peak)
    float drainSlotOutputLevel(int slot);       // dBFS

    // 2026-05-02: end-of-audio-block promotion of all slot atomics.  Called
    // by VibeGraph::promoteAllRackSlotSnapshots() once per audio block (renamed
    // from promoteAllInsertPeakSnapshots in QA-AudioMeters 2026-05-24 when the
    // per-insert peakDbSnap layer was removed; rack-slot promotion is the
    // surviving half), walking every rack across every node + insert so the
    // entire app's effect-panel meter state ends each block coherent.
    void promoteSlotPeakSnapshots();

    // FX master switch - bypasses entire rack without destroying slot data
    void setRackBypassed(bool bypass) { mRackBypassed = bypass; }
    bool isRackBypassed() const       { return mRackBypassed; }

    // Sum of getLatencySamples() across all active, non-bypassed slots.
    // Returns 0 when rack is bypassed. Used by VibeGraph for PDC.
    int getTotalLatencySamples() const;

    // Read-only slot access
    const Slot&  getSlot(int slot) const { return mSlots[slot]; }
    EffectType   getSlotType(int slot)   const { return mSlots[slot].type; }
    bool         isSlotBypassed(int slot) const { return mSlots[slot].bypassed.load(std::memory_order_relaxed); }
    juce::String getSlotUuid(int slot)   const { return (slot >= 0 && slot < kNumSlots) ? mSlots[slot].uuid : juce::String(); }

    // I-0a: Returns the slot's currently-loaded DSP from the message/UI-thread
    // perspective.  If a swap is pending (audio hasn't promoted it yet) returns
    // `pending` (the NEW DSP just loaded by the user); otherwise returns
    // `active` (the running DSP).  Audio thread inside process() uses
    // `slot.active.get()` directly after draining the swap-pending flag.
    // Returns nullptr for empty slots or out-of-range indices.
    DSPBase*     getSlotEffect(int slot) const noexcept
    {
        if (slot < 0 || slot >= kNumSlots) return nullptr;
        const auto& s = mSlots[slot];
        if (s.swapPending.load(std::memory_order_acquire))
            return s.pending.get();
        return s.active.get();
    }

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
    // the storage; process() forwards mScArrCopy.data() to each slot's
    // active effect each block.  Lifetime is no longer dependent on the
    // caller's storage.
    std::array<juce::AudioBuffer<float>*, 4> mScArrCopy {};
    juce::AudioBuffer<float>* const*         mScBufs    { nullptr };
    int                                      mScCount   { 0 };

    // I-0a (2026-05-02): serializes message-thread mutations against each
    // other (loadEffect, clearSlot, setSlotBypassed, moveSlot*, etc.) so
    // two UI events can't tear pending/active state.  Audio thread NEVER
    // acquires this -- it's purely message-thread serialization.
    juce::CriticalSection mLoadLock;

    // Guards multi-slot mutations (moveSlotUp/Down, packSlotsToTop,
    // setStateInformation, prepare, reset, setHostBPM) against the audio
    // thread.  Single-slot mutations (loadEffect/clearSlot/setSlotBypassed)
    // do NOT take this lock -- they publish via the per-slot swapPending
    // flag and are wait-free on the audio side.  Audio's process() try-locks
    // to skip blocks during multi-slot rearrangements (rare UI clicks).
    juce::SpinLock mSlotsLock;

    // Factory: creates the DSPBase subclass for a given EffectType.
    // Static + public so non-rack callers (EffectPresetIO factory-preset
    // seeding, e.g.) can spin up a temp DSP for state extraction without
    // owning a full EffectRack instance.
public:
    static std::unique_ptr<DSPBase> createEffect(EffectType type);
private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectRack)
};
