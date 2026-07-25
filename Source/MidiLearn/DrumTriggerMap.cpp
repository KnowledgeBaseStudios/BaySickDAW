#include "DrumTriggerMap.h"

namespace DrumTriggerVelocity
{
    std::atomic<bool> gUseFixed { false };   // default: From controller
}

// ─────────────────────────────────────────────────────────────────────────────
// Packing
// ─────────────────────────────────────────────────────────────────────────────
uint32_t DrumTriggerMap::pack (const Binding& b) noexcept
{
    if (! b.isSet()) return 0;
    const uint32_t k  = (uint32_t) b.kind & 0x3u;
    const uint32_t n  = ((uint32_t) juce::jlimit (0, 127, b.number) + 1u) & 0xFFu;
    const uint32_t ch = (uint32_t) juce::jlimit (0, 16, b.channel) & 0x1Fu;
    return k | (n << 2) | (ch << 10);
}

DrumTriggerMap::Binding DrumTriggerMap::unpack (uint32_t raw) noexcept
{
    Binding b;
    const uint32_t n = (raw >> 2) & 0xFFu;
    if (raw == 0 || n == 0) return b;   // None
    b.kind    = (Kind) (raw & 0x3u);
    b.number  = (int) n - 1;
    b.channel = (int) ((raw >> 10) & 0x1Fu);
    return b;
}

void DrumTriggerMap::publish (int drumIdx) noexcept
{
    mPacked[(size_t) drumIdx].store (pack (mBindings[(size_t) drumIdx]),
                                     std::memory_order_release);
}

void DrumTriggerMap::republishAnyBound() noexcept
{
    bool any = false;
    for (const auto& b : mBindings)
        if (b.isSet()) { any = true; break; }
    mAnyBound.store (any, std::memory_order_release);
}

// ─────────────────────────────────────────────────────────────────────────────
// Mutation (message thread)
// ─────────────────────────────────────────────────────────────────────────────
void DrumTriggerMap::setBinding (int drumIdx, const Binding& b)
{
    if (drumIdx < 0 || drumIdx >= kMaxDrumPages) return;
    {
        const juce::SpinLock::ScopedLockType lk (mLock);
        mBindings[(size_t) drumIdx] = b;
        publish (drumIdx);
        republishAnyBound();
    }
}

void DrumTriggerMap::clearBinding (int drumIdx)
{
    if (drumIdx < 0 || drumIdx >= kMaxDrumPages) return;
    {
        const juce::SpinLock::ScopedLockType lk (mLock);
        mBindings[(size_t) drumIdx] = {};
        publish (drumIdx);
        republishAnyBound();
    }
}

void DrumTriggerMap::clearAll()
{
    {
        const juce::SpinLock::ScopedLockType lk (mLock);
        for (int i = 0; i < kMaxDrumPages; ++i)
        {
            mBindings[(size_t) i] = {};
            publish (i);
        }
        republishAnyBound();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Read (message thread)
// ─────────────────────────────────────────────────────────────────────────────
DrumTriggerMap::Binding DrumTriggerMap::getBinding (int drumIdx) const
{
    if (drumIdx < 0 || drumIdx >= kMaxDrumPages) return {};
    const juce::SpinLock::ScopedLockType lk (mLock);
    return mBindings[(size_t) drumIdx];
}

juce::String DrumTriggerMap::describeBinding (int drumIdx) const
{
    const Binding b = getBinding (drumIdx);
    if (! b.isSet()) return {};

    juce::String s;
    if (b.kind == Kind::Cc)
        s << "CC " << b.number;
    else
        s << juce::MidiMessage::getMidiNoteName (b.number, true, true, 5);

    s << (b.channel == 0 ? " omni" : " ch" + juce::String (b.channel));
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// Learn capture
// ─────────────────────────────────────────────────────────────────────────────
void DrumTriggerMap::beginLearn (int drumIdx)
{
    if (drumIdx < 0 || drumIdx >= kMaxDrumPages) return;
    mCaptureReady.store (false, std::memory_order_relaxed);
    mLearnTarget .store (drumIdx, std::memory_order_release);
}

void DrumTriggerMap::cancelLearn()
{
    mLearnTarget .store (-1,    std::memory_order_release);
    mCaptureReady.store (false, std::memory_order_release);
}

bool DrumTriggerMap::takeCapturedBindingFor (int drumIdx, Binding& outBinding)
{
    if (! mCaptureReady.load (std::memory_order_acquire)) return false;
    // Owner check BEFORE the destructive exchange -- see the header note.
    if (mCapturedDrum.load (std::memory_order_relaxed) != drumIdx) return false;
    if (! mCaptureReady.exchange (false, std::memory_order_acq_rel)) return false;

    outBinding = unpack (mCapturedRaw.load (std::memory_order_relaxed));
    return outBinding.isSet();
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio thread
// ─────────────────────────────────────────────────────────────────────────────
DrumTriggerMap::Binding DrumTriggerMap::getBindingRT (int drumIdx) const noexcept
{
    if (drumIdx < 0 || drumIdx >= kMaxDrumPages) return {};
    return unpack (mPacked[(size_t) drumIdx].load (std::memory_order_acquire));
}

bool DrumTriggerMap::tryCaptureLearnRT (const juce::MidiMessage& msg) noexcept
{
    const int target = mLearnTarget.load (std::memory_order_acquire);
    if (target < 0) return false;

    Binding b;
    // Note-on only: a note-off would capture on key RELEASE, binding the drum
    // the instant the user lets go rather than when they hit the pad.  JUCE's
    // isNoteOn defaults to treating velocity-0 as NOT a note-on, so gear that
    // encodes release as note-on-vel-0 cannot capture here either.
    if (msg.isNoteOn())
    {
        b.kind    = Kind::Note;
        b.number  = msg.getNoteNumber();
        b.channel = msg.getChannel();
    }
    else if (msg.isController())
    {
        // Ignore a value-0 CC: momentary pads send value-0 on RELEASE, so
        // capturing it would bind on let-go and (worse) a pad that idles at 0
        // would self-bind with no user gesture at all.
        if (msg.getControllerValue() <= 0) return false;
        b.kind    = Kind::Cc;
        b.number  = msg.getControllerNumber();
        b.channel = msg.getChannel();
    }
    else
    {
        return false;   // not learnable; fall through to normal dispatch
    }

    mCapturedDrum.store (target,   std::memory_order_relaxed);
    mCapturedRaw .store (pack (b), std::memory_order_relaxed);
    mLearnTarget .store (-1,       std::memory_order_release);
    mCaptureReady.store (true,     std::memory_order_release);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Persistence
// ─────────────────────────────────────────────────────────────────────────────
juce::ValueTree DrumTriggerMap::saveToValueTree() const
{
    juce::ValueTree root (kRootTag);
    const juce::SpinLock::ScopedLockType lk (mLock);
    for (int i = 0; i < kMaxDrumPages; ++i)
    {
        const auto& b = mBindings[(size_t) i];
        if (! b.isSet()) continue;
        juce::ValueTree c ("Trigger");
        c.setProperty ("drum",    i,               nullptr);
        c.setProperty ("kind",    (int) b.kind,    nullptr);
        c.setProperty ("number",  b.number,        nullptr);
        c.setProperty ("channel", b.channel,       nullptr);
        root.addChild (c, -1, nullptr);
    }
    return root;
}

void DrumTriggerMap::loadFromValueTree (const juce::ValueTree& tree)
{
    if (! tree.hasType (kRootTag)) return;
    {
        const juce::SpinLock::ScopedLockType lk (mLock);
        for (int i = 0; i < kMaxDrumPages; ++i)
        {
            mBindings[(size_t) i] = {};
            publish (i);
        }
        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            auto c = tree.getChild (i);
            if (! c.hasType ("Trigger")) continue;
            const int drum = (int) c.getProperty ("drum", -1);
            if (drum < 0 || drum >= kMaxDrumPages) continue;

            Binding b;
            const int k = (int) c.getProperty ("kind", 0);
            b.kind    = (k == 1) ? Kind::Note : (k == 2 ? Kind::Cc : Kind::None);
            b.number  = juce::jlimit (-1, 127, (int) c.getProperty ("number",  -1));
            b.channel = juce::jlimit (0,   16, (int) c.getProperty ("channel",  0));
            if (! b.isSet()) continue;

            mBindings[(size_t) drum] = b;
            publish (drum);
        }
        republishAnyBound();
    }
}
