#include "EffectRack.h"
#include "DSP/CompressorDSP.h"
#include "DSP/ReverbDSP.h"
#include "DSP/ChorusDSP.h"
#include "DSP/DelayDSP.h"
#include "DSP/SaturationDSP.h"
#include "DSP/FlangerDSP.h"
#include "DSP/OverdriveDSP.h"
#include "DSP/PhaserDSP.h"
#include "DSP/TransientShaperDSP.h"
#include "DSP/TapeDSP.h"
#include "DSP/LimiterDSP.h"
#include "DSP/DeEsserDSP.h"

// ── Ctor ──────────────────────────────────────────────────────────────────────
EffectRack::EffectRack()
{
}

// ── Factory ───────────────────────────────────────────────────────────────────
std::unique_ptr<DSPBase> EffectRack::createEffect(EffectType type)
{
    switch (type)
    {
        case EffectType::Compressor:      return std::make_unique<CompressorDSP>();
        case EffectType::Reverb:          return std::make_unique<ReverbDSP>();
        case EffectType::Chorus:          return std::make_unique<ChorusDSP>();
        case EffectType::Delay:           return std::make_unique<DelayDSP>();
        case EffectType::Saturation:      return std::make_unique<SaturationDSP>();
        case EffectType::Flanger:         return std::make_unique<FlangerDSP>();
        case EffectType::Overdrive:       return std::make_unique<OverdriveDSP>();
        case EffectType::Phaser:          return std::make_unique<PhaserDSP>();
        case EffectType::TransientShaper: return std::make_unique<TransientShaperDSP>();
        case EffectType::Tape:
        {
            // H-10 cutover (2026-05-02): EffectType::Tape is now an ALIAS that
            // constructs a SaturationDSP with mSatType pre-set to Type::Tape.
            // Old projects load via this path; the Saturation umbrella runs
            // a bit-exact port of the legacy TapeDSP body.  TapeDSP class
            // stays in the source tree as an emergency-rollback safety net
            // but is no longer instantiated by the rack.
            auto sat = std::make_unique<SaturationDSP>();
            sat->setSatType ((int) SaturationDSP::Type::Tape);
            return sat;
        }
        case EffectType::Limiter:         return std::make_unique<LimiterDSP>();
        case EffectType::DeEsser:         return std::make_unique<DeEsserDSP>();

        // I-1 (2026-05-02): BaySickPedals 18-module spec entries.  DSP classes
        // land in I-5..I-13; factory returns nullptr until then so empty slots
        // are skipped on the audio path.  Picker entries can still be saved
        // and persisted as enum values without crashing.
        case EffectType::BluesDriveStyle:
        case EffectType::OverdriveStyle:
        case EffectType::DistortionStyle:
        case EffectType::FuzzStyle:
        case EffectType::NoiseGateStyle:
        case EffectType::HighGainStyle:
        case EffectType::TunerStyle:
        case EffectType::AcousticPreampStyle:
        case EffectType::GraphicEQStyle:
        case EffectType::SynthStyle:
        case EffectType::OctaveStyle:
        case EffectType::WahStyle:
        case EffectType::BassGraphicEQStyle:
        case EffectType::BassCompressorStyle:
        case EffectType::BassDriverStyle:
        case EffectType::BassOverdriveStyle:
        case EffectType::FurmanEQStyle:
            return nullptr;

        default:                          return nullptr;
    }
}

// ── Slot management ───────────────────────────────────────────────────────────
// I-0a (2026-05-02): single-slot mutations (loadEffect/clearSlot/setSlotBypassed)
// publish via per-slot swapPending atomic and DO NOT take mSlotsLock --- the
// audio thread is wait-free on these paths.  mLoadLock serializes message-
// thread mutations against each other.  Multi-slot mutations (moveSlot*,
// packSlotsToTop, setStateInformation) still take mSlotsLock because their
// atomicity spans multiple slots; audio's process() try-locks to skip during
// those rare rearrangements.

// Internal helper: spin-wait briefly for a previously-parked pending DSP to
// be consumed by the audio thread before overwriting `pending`.  Mirrors the
// 1-second budget in BaySickNAMIRProcessor::loadNamModel.  In practice the
// wait is 0 iterations because audio runs every ~10ms and the consumer flag
// clears at the top of each process().
static void waitForPendingDrain (std::atomic<bool>& flag) noexcept
{
    for (int spin = 0; spin < 1000 && flag.load (std::memory_order_acquire); ++spin)
        juce::Thread::sleep (1);
}

void EffectRack::loadEffect(int slot, EffectType type, const juce::String& uuidOverride)
{
    if (slot < 0 || slot >= kNumSlots) return;

    // Build + prepare the new DSP OUTSIDE every lock (prepare can allocate).
    auto effect = createEffect(type);
    if (effect && mSampleRate > 0.0)
        effect->prepare(mSampleRate, mMaxBlock);
    if (effect)
        effect->setHostBPM(mHostBPM);

    // Stable per-slot identity for automation paramIds. Fresh UUID on each
    // user-facing load (effect-type swap drops old automation, which matches
    // intuition — different effect = different knobs). setStateInformation
    // passes the saved UUID via uuidOverride to keep automation lanes alive
    // across project save/load.
    juce::String newUuid = uuidOverride.isNotEmpty()
        ? uuidOverride
        : juce::Uuid().toString();

    const juce::ScopedLock lk (mLoadLock);

    // If a prior swap is still pending, drain it on the message thread under
    // mSlotsLock instead of waiting for audio to consume the flag.  Audio
    // could be inactive (project load before audio device starts) in which
    // case waiting forever is wrong; or rapidly active in which case taking
    // mSlotsLock briefly is cheap.  Either way, drained-by-message-thread
    // is correct -- the per-slot drain logic is identical to audio's.
    if (mSlots[slot].swapPending.load (std::memory_order_acquire))
    {
        const juce::SpinLock::ScopedLockType lkAudio (mSlotsLock);
        if (mSlots[slot].swapPending.load (std::memory_order_acquire))
        {
            std::swap (mSlots[slot].active, mSlots[slot].pending);
            mSlots[slot].swapPending.store (false, std::memory_order_release);
        }
    }

    // Move the new DSP into pending.  Any previously-parked DSP (the OLD
    // active that was demoted by the most recent swap) destructs HERE on the
    // message thread -- never on audio.
    mSlots[slot].pending = std::move (effect);
    mSlots[slot].type    = type;
    mSlots[slot].uuid    = newUuid;

    // Publish: audio's next process() will std::swap active <-> pending.
    mSlots[slot].swapPending.store (true, std::memory_order_release);
}

void EffectRack::clearSlot(int slot)
{
    if (slot < 0 || slot >= kNumSlots) return;

    const juce::ScopedLock lk (mLoadLock);

    // If a prior swap is still pending, drain it on the message thread
    // (avoids the 1-second sleep when audio isn't running -- e.g., during
    // project load).  Holds mSlotsLock briefly to lock out any audio block
    // that's currently doing its own drain.
    if (mSlots[slot].swapPending.load (std::memory_order_acquire))
    {
        const juce::SpinLock::ScopedLockType lkAudio (mSlotsLock);
        if (mSlots[slot].swapPending.load (std::memory_order_acquire))
        {
            std::swap (mSlots[slot].active, mSlots[slot].pending);
            mSlots[slot].swapPending.store (false, std::memory_order_release);
        }
    }

    // Park nullptr in pending; audio swaps active->pending so the OLD
    // active sits in pending until the NEXT loadEffect on this slot
    // destructs it on the message thread.
    mSlots[slot].pending.reset();
    mSlots[slot].type     = EffectType::None;
    mSlots[slot].bypassed.store (false, std::memory_order_relaxed);
    mSlots[slot].uuid     = {};
    mSlots[slot].swapPending.store (true, std::memory_order_release);
}

void EffectRack::moveSlotUp(int slot)
{
    if (slot <= 0 || slot >= kNumSlots) return;
    const juce::ScopedLock          lkMsg   (mLoadLock);
    const juce::SpinLock::ScopedLockType lkAudio (mSlotsLock);
    // Drain any pending swaps on the affected slots so the std::swap below
    // moves the LATEST state.  Equivalent to what audio would have done at
    // the top of process(); doing it here under mSlotsLock guarantees audio
    // can't observe a partial multi-slot rearrangement.
    auto drain = [] (Slot& s) noexcept
    {
        if (s.swapPending.load (std::memory_order_acquire))
        {
            std::swap (s.active, s.pending);
            s.swapPending.store (false, std::memory_order_release);
        }
    };
    drain (mSlots[slot]);
    drain (mSlots[slot - 1]);
    std::swap (mSlots[slot], mSlots[slot - 1]);
}

void EffectRack::moveSlotDown(int slot)
{
    if (slot < 0 || slot >= kNumSlots - 1) return;
    const juce::ScopedLock          lkMsg   (mLoadLock);
    const juce::SpinLock::ScopedLockType lkAudio (mSlotsLock);
    auto drain = [] (Slot& s) noexcept
    {
        if (s.swapPending.load (std::memory_order_acquire))
        {
            std::swap (s.active, s.pending);
            s.swapPending.store (false, std::memory_order_release);
        }
    };
    drain (mSlots[slot]);
    drain (mSlots[slot + 1]);
    std::swap (mSlots[slot], mSlots[slot + 1]);
}

void EffectRack::setSlotBypassed(int slot, bool bypass)
{
    if (slot < 0 || slot >= kNumSlots) return;
    // Atomic store -- audio reads relaxed in process().  No spinlock; no
    // mLoadLock either since this is a single atomic write.  EffectRack short-
    // circuits before calling effect->process() based on Slot.bypassed, so
    // the DSP's own bypassed flag (DSPBase::bypassed) is no longer the source
    // of truth for rack-owned effects -- we don't sync it.
    mSlots[slot].bypassed.store (bypass, std::memory_order_relaxed);
}

void EffectRack::packSlotsToTop()
{
    // Extract non-empty slots under the lock, then release and let any
    // cleared-slot destructors run outside the lock.
    std::vector<Slot> leaving;

    {
        const juce::ScopedLock          lkMsg   (mLoadLock);
        const juce::SpinLock::ScopedLockType lkAudio (mSlotsLock);

        // Drain pending swaps on every slot first so we pack the LATEST
        // state, not stale active pointers.
        for (auto& s : mSlots)
        {
            if (s.swapPending.load (std::memory_order_acquire))
            {
                std::swap (s.active, s.pending);
                s.swapPending.store (false, std::memory_order_release);
            }
        }

        std::vector<Slot> filled;
        filled.reserve(kNumSlots);
        for (int i = 0; i < kNumSlots; ++i)
            if (mSlots[i].type != EffectType::None)
                filled.push_back(std::move(mSlots[i]));

        for (int i = 0; i < kNumSlots; ++i)
        {
            if (i < (int)filled.size())
            {
                mSlots[i] = std::move(filled[i]);
            }
            else
            {
                // Defer destructor until after lock drops.
                if (mSlots[i].active || mSlots[i].pending)
                    leaving.push_back (std::move (mSlots[i]));
                mSlots[i].active.reset();
                mSlots[i].pending.reset();
                mSlots[i].swapPending.store (false, std::memory_order_relaxed);
                mSlots[i].type     = EffectType::None;
                mSlots[i].bypassed.store (false, std::memory_order_relaxed);
                mSlots[i].uuid     = {};
            }
        }
    }
    // `leaving` destructs here — any old effect tear-downs happen outside the lock.
}

// ── Audio thread ──────────────────────────────────────────────────────────────
void EffectRack::prepare(double sampleRate, int maxBlockSize)
{
    // prepare() is called from the message thread (or audio init path before
    // processing begins). Lock to block any process() that might be racing
    // with startup init.  Multi-slot path -- holds mSlotsLock.
    const juce::ScopedLock          lkMsg   (mLoadLock);
    const juce::SpinLock::ScopedLockType lkAudio (mSlotsLock);

    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;

    // Drain pending swaps so prepare() touches the latest active DSPs.
    for (auto& s : mSlots)
    {
        if (s.swapPending.load (std::memory_order_acquire))
        {
            std::swap (s.active, s.pending);
            s.swapPending.store (false, std::memory_order_release);
        }
    }

    for (auto& s : mSlots)
        if (s.active) s.active->prepare(sampleRate, maxBlockSize);
}

void EffectRack::process(juce::AudioBuffer<float>& buffer)
{
    if (mRackBypassed) return;

    // Try-lock against multi-slot mutations only (moveSlot/pack/setStateInfo/
    // prepare/reset/setHostBPM).  Single-slot loads/clears/bypass changes do
    // NOT take this lock -- they publish via per-slot swapPending and audio
    // is wait-free on those paths.  If a multi-slot op is currently in
    // flight, skip this block (rare, user-UI-driven).
    const juce::SpinLock::ScopedTryLockType tryLk (mSlotsLock);
    if (! tryLk.isLocked()) return;

    // Per-slot wait-free swap-pending drain (mirrors NAM/IR pattern).  Audio
    // thread is the publisher of `active`; message thread only ever writes
    // `pending`.  After std::swap, `pending` holds the OLD active DSP -- it
    // stays alive until the NEXT message-thread loadEffect on this slot
    // overwrites `pending` (and destructs the old DSP on the message thread).
    for (auto& s : mSlots)
    {
        if (s.swapPending.load (std::memory_order_acquire))
        {
            std::swap (s.active, s.pending);
            s.swapPending.store (false, std::memory_order_release);
        }
    }

    int numCh      = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    // 2026-05-02: meter ballistics + decay live entirely on the UI thread.
    // Audio computes per-block input-RMS + output-peak and CAS-maxes them
    // into the slot's atomic.  SlotComponent's vblank handler exchanges-and-
    // resets each frame to start a fresh "max within UI frame" window.  No
    // more audio-side 300 ms RMS smoothing or 30 dB/sec peak decay --
    // VUMeter's spring-damper does the visual smoothing, DBFSMeter's UI
    // ballistics do the visual decay.
    auto casMaxFloat = [] (std::atomic<float>& a, float v) noexcept
    {
        float cur = a.load (std::memory_order_relaxed);
        while (cur < v
               && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed))
        {}
    };

    for (auto& s : mSlots)
    {
        DSPBase* eff = s.active.get();
        if (!eff || s.bypassed.load (std::memory_order_relaxed)) continue;

        // Input peak (linear amplitude) -- the VU meter expects rms01 by
        // name but treating per-block PEAK as the input gives more useful
        // visual response than RMS (a transient pinging the meter is what
        // the user wants to see).  Spring-damper still smooths the needle
        // motion so the visual looks like classic VU ballistics.
        float inPeak = 0.f;
        for (int ch = 0; ch < numCh; ++ch) {
            const float* d = buffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                inPeak = juce::jmax(inPeak, std::abs(d[i]));
        }
        casMaxFloat (s.inputLevelRmsRun, juce::jlimit (0.f, 1.f, inPeak));

        // C.4 Phase 1 (2026-04-30): push SC context to this slot's effect
        // BEFORE processing so SC consumers see the right array + pick.
        if (mScBufs != nullptr)
            eff->setSidechainBuffers(mScBufs, mScCount);
        eff->setSidechainPick(s.scPick);

        eff->process(buffer);

        // Per-slot output gain
        if (s.outputGainDb != 0.f)
            buffer.applyGain(std::pow(10.f, s.outputGainDb / 20.f));

        // Output peak dBFS -- CAS-max only; UI exchange-and-resets each vblank.
        float peak = 0.f;
        for (int ch = 0; ch < numCh; ++ch) {
            const float* d = buffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                peak = juce::jmax(peak, std::abs(d[i]));
        }
        const float peakDbThisBlock = peak > 1e-6f ? 20.f * std::log10(peak) : -96.f;
        casMaxFloat (s.outputLevelDbRun, peakDbThisBlock);
    }
}

void EffectRack::setSlotOutputGain(int slot, float db)
{
    if (slot >= 0 && slot < kNumSlots) mSlots[slot].outputGainDb = db;
}
float EffectRack::getSlotOutputGain(int slot) const
{
    return (slot >= 0 && slot < kNumSlots) ? mSlots[slot].outputGainDb : 0.f;
}

// C.4 Phase 1 (2026-04-30): SC pick + array setters.
void EffectRack::setSlotSidechainPick (int slot, int pickIdx) noexcept
{
    if (slot >= 0 && slot < kNumSlots) mSlots[slot].scPick = pickIdx;
}
int EffectRack::getSlotSidechainPick (int slot) const noexcept
{
    return (slot >= 0 && slot < kNumSlots) ? mSlots[slot].scPick : -1;
}
void EffectRack::setSidechainBuffers (juce::AudioBuffer<float>* const* bufs, int count) noexcept
{
    const int n = juce::jmin(count, (int) mScArrCopy.size());
    for (int i = 0; i < (int) mScArrCopy.size(); ++i)
        mScArrCopy[(size_t) i] = (bufs != nullptr && i < count) ? bufs[i] : nullptr;
    mScBufs  = mScArrCopy.data();
    mScCount = n;
}
float EffectRack::getSlotInputLevel(int slot) const
{
    return (slot >= 0 && slot < kNumSlots)
        ? mSlots[slot].inputLevelRms.load(std::memory_order_relaxed) : 0.f;
}
float EffectRack::getSlotOutputLevel(int slot) const
{
    return (slot >= 0 && slot < kNumSlots)
        ? mSlots[slot].outputLevelDb.load(std::memory_order_relaxed) : -96.f;
}

// 2026-05-02: drain variants exchange the SNAPSHOT atomics (audio promotes
// Run -> Snap at end of each audio block via promoteSlotPeakSnapshots()).
// UI calls these once per vblank from SlotComponent::onVBlank.
float EffectRack::drainSlotInputLevel(int slot)
{
    if (slot < 0 || slot >= kNumSlots) return 0.f;
    return mSlots[slot].inputLevelRms.exchange(0.f, std::memory_order_relaxed);
}
float EffectRack::drainSlotOutputLevel(int slot)
{
    if (slot < 0 || slot >= kNumSlots) return -96.f;
    return mSlots[slot].outputLevelDb.exchange(-96.f, std::memory_order_relaxed);
}

// 2026-05-02: end-of-audio-block promotion of every slot's Run atomics into
// the UI-visible Snapshot atomics.  Single drain-and-CAS-max per slot per
// channel.  Called once per audio block from VibeGraph::promoteAllInsert
// PeakSnapshots() after walking every rack.
void EffectRack::promoteSlotPeakSnapshots()
{
    auto promoteOne = [] (std::atomic<float>& runMax, std::atomic<float>& snap, float resetTo) noexcept
    {
        const float v = runMax.exchange (resetTo, std::memory_order_relaxed);
        if (v == resetTo) return;
        float cur = snap.load (std::memory_order_relaxed);
        while (cur < v && ! snap.compare_exchange_weak (cur, v, std::memory_order_relaxed))
        {}
    };
    for (auto& s : mSlots)
    {
        promoteOne (s.inputLevelRmsRun, s.inputLevelRms, 0.f);
        promoteOne (s.outputLevelDbRun, s.outputLevelDb, -96.f);
    }
}

void EffectRack::reset()
{
    const juce::ScopedLock          lkMsg   (mLoadLock);
    const juce::SpinLock::ScopedLockType lkAudio (mSlotsLock);
    for (auto& s : mSlots)
    {
        if (s.swapPending.load (std::memory_order_acquire))
        {
            std::swap (s.active, s.pending);
            s.swapPending.store (false, std::memory_order_release);
        }
        if (s.active) s.active->reset();
    }
}

void EffectRack::setHostBPM(double bpm)
{
    mHostBPM = bpm;
    const juce::ScopedLock          lkMsg   (mLoadLock);
    const juce::SpinLock::ScopedLockType lkAudio (mSlotsLock);
    for (auto& s : mSlots)
    {
        if (s.swapPending.load (std::memory_order_acquire))
        {
            std::swap (s.active, s.pending);
            s.swapPending.store (false, std::memory_order_release);
        }
        if (s.active) s.active->setHostBPM(bpm);
    }
}

// ── PDC ───────────────────────────────────────────────────────────────────────
int EffectRack::getTotalLatencySamples() const
{
    if (mRackBypassed) return 0;
    int total = 0;
    for (const auto& s : mSlots)
        if (s.active && ! s.bypassed.load (std::memory_order_relaxed))
            total += s.active->getLatencySamples();
    return total;
}

// ── Serialization ─────────────────────────────────────────────────────────────
void EffectRack::getStateInformation(juce::MemoryBlock& dest)
{
    juce::ValueTree state("EffectRack");
    // Batch E #5 (2026-05-01): mRackBypassed is owned by the strip's _bypass
    // APVTS param.  XML dual-storage was overwritten on every load by the
    // APVTS listener, so the XML half was dead weight.  Removed.

    // I-0a: Capture state from `pending` if a swap is pending (audio hasn't
    // promoted it yet but the user-visible "current" effect is the pending
    // one).  Otherwise read from `active`.
    for (int i = 0; i < kNumSlots; ++i)
    {
        const bool swapPending = mSlots[i].swapPending.load (std::memory_order_acquire);
        DSPBase* eff = swapPending ? mSlots[i].pending.get() : mSlots[i].active.get();

        juce::ValueTree slotTree("Slot");
        slotTree.setProperty("index",    i,                                                       nullptr);
        slotTree.setProperty("type",     (int)mSlots[i].type,                                     nullptr);
        slotTree.setProperty("bypassed", mSlots[i].bypassed.load (std::memory_order_relaxed),     nullptr);
        // 2026-04-30: per-slot output Vol knob (post-effect gain in dB).
        // Was missing from save — every project load reset every slot's
        // Vol knob to 0 dB.  Range -24..+12 dB per EditorPanelBase.
        slotTree.setProperty("outputGainDb", mSlots[i].outputGainDb,    nullptr);
        // C13: stable per-slot UUID drives automation paramIds.  Persisted
        // so automation lanes survive project reload.  Pre-C13 saves omit
        // this property — those projects' lanes silently no-op (matches
        // the audit's existing Tier 4 stale-lane behavior).
        slotTree.setProperty("uuid",     mSlots[i].uuid,                 nullptr);
        // C.4 Phase 1: per-slot SC pick (-1 = no SC, 0..3 = strip SC line).
        slotTree.setProperty("scPick",   mSlots[i].scPick,               nullptr);

        if (eff)
        {
            juce::MemoryBlock slotData;
            eff->getStateInformation(slotData);
            slotTree.setProperty("data",
                juce::Base64::toBase64(slotData.getData(), slotData.getSize()), nullptr);
        }
        state.addChild(slotTree, -1, nullptr);
    }

    auto xml = state.createXml();
    juce::AudioProcessor::copyXmlToBinary(*xml, dest);
}

void EffectRack::setStateInformation(const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary(data, sz);
    if (!xml) return;

    auto state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid()) return;

    // Batch E #5: do NOT read "bypassed" here -- APVTS _bypass param drives it.
    // (Old projects with a stale XML "bypassed" property are silently ignored.)

    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild(i);
        if (child.hasType("Slot"))
        {
            int slotIdx = (int)child.getProperty("index", -1);
            if (slotIdx < 0 || slotIdx >= kNumSlots) continue;

            auto type = (EffectType)(int)child.getProperty("type", 0);
            bool byp  = (bool)(int)child.getProperty("bypassed", 0);
            // 2026-04-30: restore per-slot Vol knob (default 0 dB if the
            // saved project predates this fix and didn't write it).
            float vol = (float)(double)child.getProperty("outputGainDb", 0.0);
            // C13: restore saved UUID so automation paramIds match across
            // load.  Empty when loading a pre-C13 project; loadEffect will
            // generate a fresh UUID in that case.
            juce::String uuid = child.getProperty("uuid", "").toString();
            // C.4 Phase 1: restore per-slot SC pick (default -1 = no SC).
            int scPick = (int) child.getProperty("scPick", -1);

            if (type != EffectType::None)
            {
                // I-0a fix (2026-05-02): we cannot use loadEffect followed by
                // a write to pending -- audio could run between the two and
                // swap pending into active (leaving pending null) before we
                // get to write the state blob.  Instead, build the new DSP
                // locally, apply the saved state to it on this thread, THEN
                // park it atomically via the same swap-pending publish that
                // loadEffect uses.  Audio sees a fully-restored DSP on the
                // first swap.
                auto effect = createEffect(type);
                if (effect)
                {
                    if (mSampleRate > 0.0)
                        effect->prepare(mSampleRate, mMaxBlock);
                    effect->setHostBPM(mHostBPM);

                    juce::String b64 = child.getProperty("data", "");
                    if (b64.isNotEmpty())
                    {
                        juce::MemoryOutputStream mos;
                        juce::Base64::convertFromBase64(mos, b64);
                        juce::MemoryBlock decoded = mos.getMemoryBlock();
                        effect->setStateInformation(
                            decoded.getData(), (int)decoded.getSize());
                    }
                }

                juce::String newUuid = uuid.isNotEmpty()
                    ? uuid
                    : juce::Uuid().toString();

                // Bulk-restore path: install the fully-prepared DSP into
                // `active` directly under mSlotsLock (audio thread try-locks
                // and skips the block).  Avoids the swap-pending drain wait
                // during project load when the audio thread isn't yet producing
                // blocks to consume the flag -- which previously caused a
                // 1-second-per-slot hang adding up to minutes for a full project.
                {
                    const juce::ScopedLock                lkMsg   (mLoadLock);
                    const juce::SpinLock::ScopedLockType  lkAudio (mSlotsLock);

                    if (mSlots[slotIdx].swapPending.load (std::memory_order_acquire))
                    {
                        std::swap (mSlots[slotIdx].active, mSlots[slotIdx].pending);
                        mSlots[slotIdx].swapPending.store (false, std::memory_order_release);
                    }
                    mSlots[slotIdx].active = std::move (effect);
                    mSlots[slotIdx].pending.reset();
                    mSlots[slotIdx].type   = type;
                    mSlots[slotIdx].uuid   = newUuid;
                }

                setSlotBypassed(slotIdx, byp);
                setSlotOutputGain(slotIdx, vol);
                setSlotSidechainPick(slotIdx, scPick);
            }
            else
            {
                clearSlot(slotIdx);
            }
        }
    }
}
