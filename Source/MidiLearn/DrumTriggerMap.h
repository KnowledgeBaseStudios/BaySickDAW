#pragma once
#include <JuceHeader.h>
#include "../BaySickConstants.h"
#include <array>
#include <atomic>

// ─────────────────────────────────────────────────────────────────────────────
// DrumTriggerMap - QA-L-Fix (2026-07-19)
// ─────────────────────────────────────────────────────────────────────────────
// Per-drum hardware trigger bindings for the drum KIT: one note-or-CC binding
// per drum page, so a pad controller plays the kit live.
//
// Sibling to MidiLearnRegistry, deliberately NOT built on it (D-13).  The
// registry maps events to APVTS *param values* and its MessageType enum has no
// Note case at all; drum triggers need note-OR-CC *triggers* that inject MIDI
// into an engine's page buffer.  Different domain, different data, own store.
//
// Threading (audio-thread contract):
//   * The audio thread reads ONLY `mPacked` - one lock-free atomic word per
//     drum.  No locks, no allocation, no juce::String anywhere on that path.
//     (The sibling registry's queue had the mirror-image defect - it destroyed
//     juce::Strings during its audio-thread drain, freeing heap in the render
//     callback.  Fixed 2026-07-19 by interning device names to ints; this class
//     avoids the whole class of problem by keeping no refcounted type on the
//     audio-thread path at all.)
//   * Message thread mutates `mBindings` under mLock and republishes the packed
//     word with a release store.  Audio thread acquire-loads.  A binding edit is
//     one word - it can never be seen half-applied.
//   * Learn capture never calls a std::function on the audio thread.  The audio
//     thread packs the captured event into `mCapturedRaw` and sets
//     `mCaptureReady`; the message-thread owner polls and commits.
//
// Device scoping: NOT carried.  Bindings match on type + number + channel only.
// The trigger path runs in the live-MIDI loop (juce::MidiMessageCollector),
// which preserves intra-block sample position but discards the source device -
// and per-event device identity is not something major DAWs carry anyway (they
// resolve identity once at the port).  Channel scoping covers the collision
// that actually occurs, pad note ranges overlapping keyboard ranges.
// ─────────────────────────────────────────────────────────────────────────────

// D-11: app-wide velocity source for kit triggers.  Namespaced global atomic
// to match the other app-wide audio-thread-read toggles
// (RenderEngine::gMultiThreadedEngineEnabled, MeterLatencyComp::gEnabled) --
// the preference loads before the processor is constructed, so it cannot live
// on the processor.  Persisted to settings.xml.
namespace DrumTriggerVelocity
{
    // false = From controller (default), true = Fixed.
    extern std::atomic<bool> gUseFixed;

    // Matches PianoNote's default velocity, i.e. what a drawn kit hit gets.
    // Used verbatim when the source is Fixed, for gear whose pads aren't
    // velocity sensitive.
    inline constexpr float kFixedVelocity = 0.8f;
}

class DrumTriggerMap
{
public:
    DrumTriggerMap() { for (auto& p : mPacked) p.store (0, std::memory_order_relaxed); }

    enum class Kind : uint8_t { None = 0, Note = 1, Cc = 2 };

    struct Binding
    {
        Kind kind    { Kind::None };
        int  number  { -1 };   // note number or CC number, 0..127
        int  channel { 0 };    // 0 = omni, 1..16

        bool isSet() const noexcept { return kind != Kind::None && number >= 0; }
    };

    // ── Mutation (message thread) ────────────────────────────────────────────
    void setBinding   (int drumIdx, const Binding& b);
    void clearBinding (int drumIdx);
    void clearAll();

    // ── Read (message thread) ────────────────────────────────────────────────
    Binding getBinding (int drumIdx) const;
    // Human-readable binding summary for the kit menu label, e.g. "CC 42 ch1"
    // or "D5 omni".  Empty when unset.  ASCII only.
    juce::String describeBinding (int drumIdx) const;

    // ── Learn capture ────────────────────────────────────────────────────────
    // beginLearn arms drumIdx; the next note-on or CC seen by the audio thread
    // is captured and suppressed from trigger dispatch.  The owner polls
    // takeCapturedBindingFor() on the message thread to commit it.
    void beginLearn  (int drumIdx);
    void cancelLearn();
    bool isLearning         () const noexcept { return mLearnTarget.load (std::memory_order_acquire) >= 0; }
    int  getLearnTargetDrum () const noexcept { return mLearnTarget.load (std::memory_order_acquire); }

    // Message thread.  Consumes the pending capture ONLY if it belongs to
    // `drumIdx`, and returns true exactly once in that case.
    //
    // Ownership is checked inside the take because the consume is destructive
    // and EVERY DrumPage polls this shared map on its own timer.  An
    // unconditional take followed by a caller-side owner check lets whichever
    // page ticks first swallow a capture meant for another drum and silently
    // drop it -- with N drum tabs open the learn then succeeds about 1/N of the
    // time.  Do not "simplify" this back into a take + compare.
    bool takeCapturedBindingFor (int drumIdx, Binding& outBinding);

    // ── Audio thread ─────────────────────────────────────────────────────────
    // Fast-path bypass: one atomic load that says whether ANY drum has a
    // binding, so the per-block dispatch can skip its kMaxDrumPages-slot scan
    // entirely until the user actually binds something
    // (reference_audio_thread_fast_path_bypass).
    bool anyBound() const noexcept { return mAnyBound.load (std::memory_order_acquire); }

    // Lock-free acquire-load of one drum's binding.
    Binding getBindingRT (int drumIdx) const noexcept;

    // If a learn is armed and `msg` is a learnable event (note-on / CC), capture
    // it and return true so the caller suppresses the event from dispatch.
    bool tryCaptureLearnRT (const juce::MidiMessage& msg) noexcept;

    // ── Persistence ──────────────────────────────────────────────────────────
    static constexpr const char* kRootTag = "DrumTriggers";

    juce::ValueTree saveToValueTree() const;
    void            loadFromValueTree (const juce::ValueTree& tree);

private:
    // Packed layout so a binding is one atomic word:
    //   bits 0-1  kind (0 none / 1 note / 2 cc)
    //   bits 2-9  number  + 1  (0 = unset, so a fully-zero word means None)
    //   bits 10-14 channel (0 = omni, 1..16)
    static uint32_t pack   (const Binding& b) noexcept;
    static Binding  unpack (uint32_t raw) noexcept;

    void publish (int drumIdx) noexcept;      // message thread -> mPacked
    void republishAnyBound() noexcept;        // call under mLock after any edit

    // Guards mBindings, which only the message thread touches (UI edits, menu
    // reads, project save/load) -- the audio thread never reads it, it reads
    // mPacked.  The lock is here because project load can arrive off the
    // message thread in a hosted context, not because of the audio thread.
    mutable juce::SpinLock                  mLock;
    std::array<Binding, kMaxDrumPages>      mBindings {};

    // Audio-thread-visible mirror.  One release-stored word per drum.
    std::array<std::atomic<uint32_t>, kMaxDrumPages> mPacked;
    std::atomic<bool> mAnyBound { false };

    // Learn handshake.  -1 = not learning.  Audio thread clears mLearnTarget
    // and publishes mCapturedRaw + mCaptureReady; message thread consumes.
    std::atomic<int>      mLearnTarget  { -1 };
    std::atomic<int>      mCapturedDrum { -1 };
    std::atomic<uint32_t> mCapturedRaw  { 0 };
    std::atomic<bool>     mCaptureReady { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumTriggerMap)
};
