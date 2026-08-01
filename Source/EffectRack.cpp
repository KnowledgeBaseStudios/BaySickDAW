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
#include "DSP/LimiterDSP.h"
#include "DSP/DeEsserDSP.h"
#include "DSP/GateDSP.h"      // QA-Fe2 vocal-chain Gate
#include "DSP/DeReverbDSP.h"  // QA-Fe2 vocal-chain De-reverb
#include "Hosting/HostedPluginEffect.h"   // QA-ModelShell TS6: hosted VST3 slot
// I-5 (2026-05-02): Harmonics drive pedals batch -- 4 new DSP classes.
#include "DSP/BluesDriveStyleDSP.h"
#include "DSP/DistortionStyleDSP.h"
#include "DSP/FuzzStyleDSP.h"
#include "DSP/HighGainStyleDSP.h"
// I-6 (2026-05-02): Harmonics bass pedals batch -- 2 multi-band drives.
#include "DSP/BassDriverStyleDSP.h"
#include "DSP/BassOverdriveStyleDSP.h"
// I-7 (2026-05-02): OC Style Octave (Polyphonic granular + Vintage divider).
#include "DSP/OctaveStyleDSP.h"
// I-8 (2026-05-02): Dynamics pedals batch -- Noise Gate + Bass Compressor.
#include "DSP/NoiseGateStyleDSP.h"
#include "DSP/BassCompressorStyleDSP.h"
// I-9 (2026-05-03): SY Style Polyphonic Synth (YIN tracker + oscillator voice).
#include "DSP/SynthStyleDSP.h"
// I-10 (2026-05-03): PW Style Wah (resonant bandpass with Vintage/Rich modes).
#include "DSP/WahStyleDSP.h"
// I-11 (2026-05-03): AD Style Acoustic Preamp (body convolution + Schroeder + notch).
#include "DSP/AcousticPreampStyleDSP.h"
// I-11 (2026-05-03): AC Style Acoustic Simulator (corrective EQ + transient + Schroeder).
#include "DSP/AcousticSimulatorStyleDSP.h"
// I-12 (2026-05-03): EQ trio batch (GE / GEB / EQFH).
#include "DSP/GraphicEQStyleDSP.h"
#include "DSP/BassGraphicEQStyleDSP.h"
#include "DSP/FurmanEQStyleDSP.h"
// I-13 (2026-05-03): TU Style Tuner (YIN-driven).
#include "DSP/TunerStyleDSP.h"
// I-15c (2026-05-03): User NAM Pedal (loads .nam capture).
#include "DSP/NAMPedalStyleDSP.h"

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
            // a bit-exact port of the legacy TapeDSP body.  That class was
            // deleted 2026-07-28 once nothing referenced it -- the port inside
            // SaturationDSP is the only tape implementation now.
            auto sat = std::make_unique<SaturationDSP>();
            sat->setSatType ((int) SaturationDSP::Type::Tape);
            return sat;
        }
        case EffectType::Limiter:         return std::make_unique<LimiterDSP>();
        case EffectType::DeEsser:         return std::make_unique<DeEsserDSP>();
        case EffectType::Gate:            return std::make_unique<GateDSP>();      // QA-Fe2
        case EffectType::DeReverb:        return std::make_unique<DeReverbDSP>();  // QA-Fe2

        // QA-ModelShell TS6: built EMPTY -- an EffectType carries no plugin
        // identity, so the picker's setPlugin() or the slot's restored state
        // blob is what names the plugin.  See HostedPluginEffect.h.
        case EffectType::VST3Plugin:      return std::make_unique<Hosting::HostedPluginEffect>();

        // I-5 (2026-05-02): BaySickPedals Harmonics drive pedals batch.
        case EffectType::BluesDriveStyle: return std::make_unique<BluesDriveStyleDSP>();
        case EffectType::DistortionStyle: return std::make_unique<DistortionStyleDSP>();
        case EffectType::FuzzStyle:       return std::make_unique<FuzzStyleDSP>();
        case EffectType::HighGainStyle:   return std::make_unique<HighGainStyleDSP>();

        // I-6 (2026-05-02): BaySickPedals Harmonics bass pedals batch.
        case EffectType::BassDriverStyle:    return std::make_unique<BassDriverStyleDSP>();
        case EffectType::BassOverdriveStyle: return std::make_unique<BassOverdriveStyleDSP>();

        // I-7 (2026-05-02): OC Style Octave (Polyphonic + Vintage modes).
        case EffectType::OctaveStyle:        return std::make_unique<OctaveStyleDSP>();

        // I-8 (2026-05-02): BaySickPedals Dynamics pedals batch.
        case EffectType::NoiseGateStyle:      return std::make_unique<NoiseGateStyleDSP>();
        case EffectType::BassCompressorStyle: return std::make_unique<BassCompressorStyleDSP>();

        // I-9 (2026-05-03): SY Style Polyphonic Synth.
        case EffectType::SynthStyle:          return std::make_unique<SynthStyleDSP>();

        // I-10 (2026-05-03): PW Style Wah.
        case EffectType::WahStyle:            return std::make_unique<WahStyleDSP>();

        // I-11 (2026-05-03): AD Style Acoustic Preamp.
        case EffectType::AcousticPreampStyle:    return std::make_unique<AcousticPreampStyleDSP>();
        // I-11 (2026-05-03): AC Style Acoustic Simulator.
        case EffectType::AcousticSimulatorStyle: return std::make_unique<AcousticSimulatorStyleDSP>();

        // I-12 (2026-05-03): EQ trio batch.
        case EffectType::GraphicEQStyle:      return std::make_unique<GraphicEQStyleDSP>();
        case EffectType::BassGraphicEQStyle:  return std::make_unique<BassGraphicEQStyleDSP>();
        case EffectType::FurmanEQStyle:       return std::make_unique<FurmanEQStyleDSP>();

        // I-13 (2026-05-03): TU Style Tuner.
        case EffectType::TunerStyle:          return std::make_unique<TunerStyleDSP>();

        // I-15c (2026-05-03): User NAM Pedal.
        case EffectType::NAMPedalStyle:       return std::make_unique<NAMPedalStyleDSP>();

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

void EffectRack::loadEffect(int slot, EffectType type, const juce::String& uuidOverride,
                            const juce::PluginDescription* pluginDesc)
{
    if (slot < 0 || slot >= kNumSlots) return;

    // Build + prepare the new DSP OUTSIDE every lock (prepare can allocate).
    auto effect = createEffect(type);

    // QA-ModelShell TS6: instantiating the plugin happens here, outside the
    // locks, for the same reason prepare() does -- loading a VST3 allocates and
    // can block for a noticeable time.
    if (effect && type == EffectType::VST3Plugin && pluginDesc != nullptr)
        if (auto* hosted = dynamic_cast<Hosting::HostedPluginEffect*>(effect.get()))
            hosted->setPlugin(*pluginDesc);

    if (effect && mSampleRate > 0.0)
        effect->prepare(mSampleRate, mMaxBlock);
    if (effect)
        effect->setHostBPM(mHostBPM);

    // Stable per-slot identity for automation paramIds. Fresh UUID on each
    // user-facing load (effect-type swap drops old automation, which matches
    // intuition - different effect = different knobs). setStateInformation
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

    if (onSlotsChanged) onSlotsChanged();
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

    mSlots[slot].pending.reset();
    mSlots[slot].type     = EffectType::None;
    mSlots[slot].bypassed.store (false, std::memory_order_relaxed);
    mSlots[slot].uuid     = {};
    mSlots[slot].swapPending.store (true, std::memory_order_release);

    // Complete the swap HERE, the same inline drain this function already does
    // at its top, then destroy the old active on the message thread.  Leaving
    // it to "the next loadEffect on this slot" parked the removed DSP -- for a
    // hosted VST3 slot, the plugin AND its live helper process -- until a
    // touch that may never come.
    {
        const juce::SpinLock::ScopedLockType lkAudio (mSlotsLock);
        if (mSlots[slot].swapPending.load (std::memory_order_acquire))
        {
            std::swap (mSlots[slot].active, mSlots[slot].pending);
            mSlots[slot].swapPending.store (false, std::memory_order_release);
        }
    }
    mSlots[slot].pending.reset();

    if (onSlotsChanged) onSlotsChanged();
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

    if (onSlotsChanged) onSlotsChanged();
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

    if (onSlotsChanged) onSlotsChanged();
}

void EffectRack::setSlotBypassed(int slot, bool bypass)
{
    if (slot < 0 || slot >= kNumSlots) return;

    // QA-E Task 9 (2026-05-17): value-change guard.  pushApvtsToDsp() (and
    // the per-engine APVTS->DSP sync paths) call this on EVERY audio block
    // with the current bypass value.  Firing onSlotsChanged unconditionally
    // chained through the engine dirty-hook -> ProjectManager::markDirty on
    // every block: the project re-dirtied hundreds of times/sec, so the
    // title-bar `*` appeared the instant a Vox/Inst engine started
    // processing and saveProject's clearDirty() was overwritten by the very
    // next block.  onSlotsChanged is a "slots changed" notifier -- a no-op
    // store needn't notify.  Matches the CPU-safeguarding standing rule
    // (guard every per-block setter with a value-change comparison).  A
    // genuine bypass change still notifies; refreshWindowTitle already
    // self-marshals off the audio thread (Batch 9c B2), so that path stays
    // safe + correct.
    if (mSlots[slot].bypassed.load (std::memory_order_relaxed) == bypass)
        return;

    // Atomic store -- audio reads relaxed in process().  No spinlock; no
    // mLoadLock either since this is a single atomic write.  EffectRack short-
    // circuits before calling effect->process() based on Slot.bypassed, so
    // the DSP's own bypassed flag (DSPBase::bypassed) is no longer the source
    // of truth for rack-owned effects -- we don't sync it.
    mSlots[slot].bypassed.store (bypass, std::memory_order_relaxed);

    if (onSlotsChanged) onSlotsChanged();
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
    // `leaving` destructs here - any old effect tear-downs happen outside the lock.

    if (onSlotsChanged) onSlotsChanged();
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

    // I-5 (2026-05-02): bypass-crossfade ramp length.  ~5 ms is short enough
    // to feel instant but long enough to hide the click from a +30 dB drive
    // pedal toggling on/off.  Computed in samples per call rather than
    // cached so a sample-rate change between blocks remains correct.
    constexpr float kBypassRampMs = 5.0f;
    const float rampSamples = juce::jmax (1.0f,
        (float) (mSampleRate * kBypassRampMs * 0.001));

    for (auto& s : mSlots)
    {
        DSPBase* eff = s.active.get();
        if (!eff) continue;

        const bool wantBypass = s.bypassed.load (std::memory_order_relaxed);
        const float rampTarget = wantBypass ? 0.0f : 1.0f;

        // Fully bypassed and ramp settled -- skip DSP entirely (CPU savings).
        if (rampTarget == 0.0f && s.bypassRampValue == 0.0f) continue;

        // Input peak (linear amplitude) -- the VU meter expects rms01 by
        // name but treating per-block PEAK as the input gives more useful
        // visual response than RMS (a transient pinging the meter is what
        // the user wants to see).  Spring-damper still smooths the needle
        // motion so the visual looks like classic VU ballistics.
        //
        // QA-Eg fix-up (perf-audit H4): juce::FloatVectorOperations::findMinAndMax
        // vectorizes to SSE/AVX (where available) instead of the prior scalar
        // loop.  Peak absolute value derived from the signed min/max range.
        float inPeak = 0.f;
        for (int ch = 0; ch < numCh; ++ch) {
            const float* d = buffer.getReadPointer(ch);
            const auto range = juce::FloatVectorOperations::findMinAndMax(d, numSamples);
            inPeak = juce::jmax(inPeak,
                                std::max(std::abs(range.getStart()), std::abs(range.getEnd())));
        }
        casMaxFloat (s.inputLevelRmsRun, juce::jlimit (0.f, 1.f, inPeak));

        // I-5 (2026-05-02): snapshot dry into the slot's scratch buffer
        // BEFORE running the DSP so the per-sample crossfade below has the
        // pre-DSP signal to mix back in during the ramp.  Skipped when the
        // ramp is fully wet (rampValue == 1 and target == 1) -- in that
        // steady-state case the crossfade collapses to a no-op and we save
        // the copy.
        const bool rampActive = ! (s.bypassRampValue == 1.0f && rampTarget == 1.0f);
        if (rampActive)
        {
            if (s.dryScratch.getNumChannels() < numCh
             || s.dryScratch.getNumSamples() < numSamples)
                s.dryScratch.setSize (numCh, numSamples, false, false, true);
            for (int ch = 0; ch < numCh; ++ch)
                s.dryScratch.copyFrom (ch, 0, buffer, ch, 0, numSamples);
        }

        // C.4 Phase 1 (2026-04-30): push SC context to this slot's effect
        // BEFORE processing so SC consumers see the right array + pick.
        if (mScBufs != nullptr)
            eff->setSidechainBuffers(mScBufs, mScCount);
        eff->setSidechainPick(s.scPick);

        eff->process(buffer);

        // Per-slot output gain
        if (s.outputGainDb != 0.f)
            buffer.applyGain(std::pow(10.f, s.outputGainDb / 20.f));

        // I-5: bypass crossfade.  Linearly ramp wet/dry mix toward target
        // over kBypassRampMs.  The DSP is run unconditionally above; here
        // we mix its output (wet) with the pre-DSP scratch (dry).  When
        // rampValue settles to the target the ramp goes inactive and a
        // future block may skip the DSP if target is 0.
        if (rampActive)
        {
            const float startMix = s.bypassRampValue;
            const float dist     = rampTarget - startMix;
            const float perSamp  = dist / rampSamples;
            float endMix = startMix;
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* wet = buffer.getWritePointer (ch);
                const float* dry = s.dryScratch.getReadPointer (ch);
                float mix = startMix;
                for (int i = 0; i < numSamples; ++i)
                {
                    // mix in [0..1]; out = dry*(1-mix) + wet*mix.
                    wet[i] = dry[i] + mix * (wet[i] - dry[i]);
                    mix += perSamp;
                    // Clamp so floating-point drift past target doesn't
                    // overshoot and re-engage the wet path forever.
                    if ((perSamp > 0.f && mix > rampTarget)
                     || (perSamp < 0.f && mix < rampTarget))
                        mix = rampTarget;
                }
                endMix = mix;
            }
            s.bypassRampValue = juce::jlimit (0.f, 1.f, endMix);
        }

        // Output peak dBFS -- CAS-max only; UI exchange-and-resets each vblank.
        // QA-Eg fix-up (perf-audit H4): juce::FloatVectorOperations::findMinAndMax
        // vectorizes to SSE/AVX instead of the prior scalar loop.
        float peak = 0.f;
        for (int ch = 0; ch < numCh; ++ch) {
            const float* d = buffer.getReadPointer(ch);
            const auto range = juce::FloatVectorOperations::findMinAndMax(d, numSamples);
            peak = juce::jmax(peak,
                              std::max(std::abs(range.getStart()), std::abs(range.getEnd())));
        }
        const float peakDbThisBlock = peak > 1e-6f ? 20.f * std::log10(peak) : -96.f;
        casMaxFloat (s.outputLevelDbRun, peakDbThisBlock);
    }
}

void EffectRack::setSlotOutputGain(int slot, float db)
{
    if (slot < 0 || slot >= kNumSlots) return;
    if (mSlots[slot].outputGainDb == db) return;   // skip dirty fire on no-op
    mSlots[slot].outputGainDb = db;
    if (onSlotsChanged) onSlotsChanged();
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

// QA-EffectsReview Task 1: per-slot Basic/Advanced UI disclosure state.
void EffectRack::setSlotBasicMode (int slot, bool basic) noexcept
{
    if (slot < 0 || slot >= kNumSlots) return;
    if (mSlots[slot].basicMode == basic) return;   // skip dirty fire on no-op
    mSlots[slot].basicMode = basic;
    if (onSlotsChanged) onSlotsChanged();
}
bool EffectRack::getSlotBasicMode (int slot) const noexcept
{
    return (slot >= 0 && slot < kNumSlots) ? mSlots[slot].basicMode : true;
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

// TS7: the per-block push.  Does BOTH setHostBPM and setHostTransport in ONE
// slot walk -- the node call sites used to call setHostBPM here every block, and
// adding a second call would have doubled the lock traffic on the audio thread
// for no gain.
void EffectRack::setHostTransport(const DSPBase::HostTransport& tp)
{
    mHostBPM = tp.bpm;
    const juce::ScopedLock          lkMsg   (mLoadLock);
    const juce::SpinLock::ScopedLockType lkAudio (mSlotsLock);
    for (auto& s : mSlots)
    {
        if (s.swapPending.load (std::memory_order_acquire))
        {
            std::swap (s.active, s.pending);
            s.swapPending.store (false, std::memory_order_release);
        }
        if (s.active)
        {
            s.active->setHostBPM (tp.bpm);
            s.active->setHostTransport (tp);
        }
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
        // Was missing from save - every project load reset every slot's
        // Vol knob to 0 dB.  Range -24..+12 dB per EditorPanelBase.
        slotTree.setProperty("outputGainDb", mSlots[i].outputGainDb,    nullptr);
        // C13: stable per-slot UUID drives automation paramIds.  Persisted
        // so automation lanes survive project reload.  Pre-C13 saves omit
        // this property - those projects' lanes silently no-op (matches
        // the audit's existing Tier 4 stale-lane behavior).
        slotTree.setProperty("uuid",     mSlots[i].uuid,                 nullptr);
        // C.4 Phase 1: per-slot SC pick (-1 = no SC, 0..3 = strip SC line).
        slotTree.setProperty("scPick",   mSlots[i].scPick,               nullptr);
        // QA-EffectsReview Task 1: per-slot Basic/Advanced UI state (default Basic).
        slotTree.setProperty("basicMode", (int) mSlots[i].basicMode,     nullptr);

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
            // QA-EffectsReview Task 1: restore Basic/Advanced (default 1=Basic for old projects).
            bool basicMode = ((int) child.getProperty("basicMode", 1)) != 0;

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
                setSlotBasicMode(slotIdx, basicMode);
            }
            else
            {
                clearSlot(slotIdx);
            }
        }
    }
}
