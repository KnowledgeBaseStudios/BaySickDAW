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
        case EffectType::Tape:            return std::make_unique<TapeDSP>();
        case EffectType::Limiter:         return std::make_unique<LimiterDSP>();
        default:                          return nullptr;
    }
}

// ── Slot management ───────────────────────────────────────────────────────────
// All mutators take mSlotsLock so the audio thread (which try-locks in
// process()) can skip one block rather than race with an effect destructor.
void EffectRack::loadEffect(int slot, EffectType type)
{
    if (slot < 0 || slot >= kNumSlots) return;

    // Prepare the new effect OUTSIDE the lock — prepare can allocate and is
    // potentially slow; we don't want to stall the audio thread waiting.
    auto effect = createEffect(type);
    if (effect && mSampleRate > 0.0)
        effect->prepare(mSampleRate, mMaxBlock);
    if (effect)
        effect->setHostBPM(mHostBPM);

    // Old-effect tear-down + pointer swap must be atomic w.r.t. process().
    // Move the outgoing effect OUT of the slot under the lock, then release
    // the lock before destructing it — the destructor itself can be slow
    // (Limiter tears down juce::dsp::Oversampling filter state) and must not
    // hold the audio thread.
    std::unique_ptr<DSPBase> outgoing;
    {
        const juce::SpinLock::ScopedLockType lk (mSlotsLock);
        outgoing                = std::move (mSlots[slot].effect);
        mSlots[slot].effect     = std::move (effect);
        mSlots[slot].type       = type;
        mSlots[slot].bypassed   = false;
    }
    // outgoing destructs here, outside the lock — safe, audio thread can
    // already see the new pointer on its next block.
}

void EffectRack::clearSlot(int slot)
{
    if (slot < 0 || slot >= kNumSlots) return;

    std::unique_ptr<DSPBase> outgoing;
    {
        const juce::SpinLock::ScopedLockType lk (mSlotsLock);
        outgoing                = std::move (mSlots[slot].effect);
        mSlots[slot].type       = EffectType::None;
        mSlots[slot].bypassed   = false;
    }
    // outgoing destructs here, outside the lock
}

void EffectRack::moveSlotUp(int slot)
{
    if (slot <= 0 || slot >= kNumSlots) return;
    const juce::SpinLock::ScopedLockType lk (mSlotsLock);
    std::swap(mSlots[slot], mSlots[slot - 1]);
}

void EffectRack::moveSlotDown(int slot)
{
    if (slot < 0 || slot >= kNumSlots - 1) return;
    const juce::SpinLock::ScopedLockType lk (mSlotsLock);
    std::swap(mSlots[slot], mSlots[slot + 1]);
}

void EffectRack::setSlotBypassed(int slot, bool bypass)
{
    if (slot < 0 || slot >= kNumSlots) return;
    // Bool writes are safe without lock; the effect pointer only has
    // its `bypassed` field set, not swapped/destroyed.
    const juce::SpinLock::ScopedLockType lk (mSlotsLock);
    mSlots[slot].bypassed = bypass;
    if (mSlots[slot].effect)
        mSlots[slot].effect->bypassed = bypass;
}

void EffectRack::packSlotsToTop()
{
    // Extract non-empty slots under the lock, then release and let any
    // cleared-slot destructors run outside the lock.
    std::vector<Slot> leaving;

    {
        const juce::SpinLock::ScopedLockType lk (mSlotsLock);

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
                if (mSlots[i].effect) leaving.push_back (std::move (mSlots[i]));
                mSlots[i].effect.reset();
                mSlots[i].type     = EffectType::None;
                mSlots[i].bypassed = false;
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
    // with startup init.
    const juce::SpinLock::ScopedLockType lk (mSlotsLock);

    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;

    for (auto& s : mSlots)
        if (s.effect) s.effect->prepare(sampleRate, maxBlockSize);
}

void EffectRack::process(juce::AudioBuffer<float>& buffer)
{
    if (mRackBypassed) return;

    // Try-lock against slot mutations. If the message thread is currently
    // loading/clearing/moving/packing a slot, skip this block — one silent
    // block is far better than a use-after-free when an effect destructor
    // runs concurrently with process().
    const juce::SpinLock::ScopedTryLockType tryLk (mSlotsLock);
    if (! tryLk.isLocked()) return;

    int numCh      = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    // Per-block meter ballistic constants:
    //   VU RMS  → 300 ms integration (ANSI C16.5-1942 spec window). Smooths
    //             per-block RMS snapshots into a stable target for the VU
    //             meter's spring-damper. Without this, VU needle "jumps" on
    //             percussive signals because each block's RMS varies.
    //   DBFS    → 30 dB/sec peak hold-and-decay. Audio blocks at ~86/sec but
    //             UI polls at 30 Hz — previously 2 of 3 transient peaks got
    //             overwritten before the UI read them.
    const float blockDurSec   = (mSampleRate > 0.0)
                                 ? (float) numSamples / (float) mSampleRate
                                 : (float) numSamples / 44100.0f;
    const float vuAlpha       = 1.0f - std::exp(-blockDurSec / 0.300f);
    const float peakDecayDb   = 30.0f * blockDurSec;

    for (auto& s : mSlots)
    {
        if (!s.effect || s.bypassed) continue;

        // Input RMS (smoothed — 300 ms time constant drives VU ballistic)
        float sumSq = 0.f;
        for (int ch = 0; ch < numCh; ++ch) {
            const float* d = buffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i) sumSq += d[i] * d[i];
        }
        const float rmsThisBlock = std::sqrt(sumSq / (float)juce::jmax(1, numCh * numSamples));
        const float prevRms = s.inputLevelRms.load(std::memory_order_relaxed);
        s.inputLevelRms.store(prevRms + vuAlpha * (rmsThisBlock - prevRms),
                              std::memory_order_relaxed);

        s.effect->process(buffer);

        // Per-slot output gain
        if (s.outputGainDb != 0.f)
            buffer.applyGain(std::pow(10.f, s.outputGainDb / 20.f));

        // Output peak dBFS (hold-and-decay so transients aren't missed)
        float peak = 0.f;
        for (int ch = 0; ch < numCh; ++ch) {
            const float* d = buffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                peak = juce::jmax(peak, std::abs(d[i]));
        }
        const float peakDbThisBlock = peak > 1e-6f ? 20.f * std::log10(peak) : -96.f;
        const float prevPeakDb      = s.outputLevelDb.load(std::memory_order_relaxed);
        const float decayedPeakDb   = juce::jmax(-96.0f, prevPeakDb - peakDecayDb);
        s.outputLevelDb.store(juce::jmax(peakDbThisBlock, decayedPeakDb),
                              std::memory_order_relaxed);
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

void EffectRack::reset()
{
    const juce::SpinLock::ScopedLockType lk (mSlotsLock);
    for (auto& s : mSlots)
        if (s.effect) s.effect->reset();
}

void EffectRack::setHostBPM(double bpm)
{
    mHostBPM = bpm;
    const juce::SpinLock::ScopedLockType lk (mSlotsLock);
    for (auto& s : mSlots)
        if (s.effect) s.effect->setHostBPM(bpm);
}

// ── PDC ───────────────────────────────────────────────────────────────────────
int EffectRack::getTotalLatencySamples() const
{
    if (mRackBypassed) return 0;
    int total = 0;
    for (const auto& s : mSlots)
        if (s.effect && !s.bypassed)
            total += s.effect->getLatencySamples();
    return total;
}

// ── Serialization ─────────────────────────────────────────────────────────────
void EffectRack::getStateInformation(juce::MemoryBlock& dest)
{
    juce::ValueTree state("EffectRack");
    state.setProperty("bypassed", mRackBypassed, nullptr);

    for (int i = 0; i < kNumSlots; ++i)
    {
        juce::ValueTree slotTree("Slot");
        slotTree.setProperty("index",    i,                              nullptr);
        slotTree.setProperty("type",     (int)mSlots[i].type,            nullptr);
        slotTree.setProperty("bypassed", mSlots[i].bypassed,             nullptr);
        // 2026-04-30: per-slot output Vol knob (post-effect gain in dB).
        // Was missing from save — every project load reset every slot's
        // Vol knob to 0 dB.  Range -24..+12 dB per EditorPanelBase.
        slotTree.setProperty("outputGainDb", mSlots[i].outputGainDb,    nullptr);

        if (mSlots[i].effect)
        {
            juce::MemoryBlock slotData;
            mSlots[i].effect->getStateInformation(slotData);
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

    mRackBypassed = (bool)(int)state.getProperty("bypassed", 0);

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

            if (type != EffectType::None)
            {
                loadEffect(slotIdx, type);
                setSlotBypassed(slotIdx, byp);
                setSlotOutputGain(slotIdx, vol);

                juce::String b64 = child.getProperty("data", "");
                if (b64.isNotEmpty() && mSlots[slotIdx].effect)
                {
                    juce::MemoryOutputStream mos;
                    juce::Base64::convertFromBase64(mos, b64);
                    juce::MemoryBlock decoded = mos.getMemoryBlock();
                    mSlots[slotIdx].effect->setStateInformation(
                        decoded.getData(), (int)decoded.getSize());
                }
            }
            else
            {
                clearSlot(slotIdx);
            }
        }
    }
}
