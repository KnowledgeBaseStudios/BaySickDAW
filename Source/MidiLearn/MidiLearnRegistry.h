#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <map>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// MidiLearnRegistry - Phase I-3b (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// App-wide MIDI Learn registry.  Maps incoming hardware MIDI events to APVTS
// param updates so the user can right-click any knob, pick "MIDI Learn", and
// bind a hardware control to it.
//
// FL-mirrored mapping granularity (locked 2026-05-02):
//   * Each mapping captures {message type, CC# / pitch-bend / aftertouch,
//     channel, device name} so two keyboards on different ports can each
//     control different params.
//   * User can later edit a mapping to widen channel to Omni or change device.
//   * Pitch-bend and channel-aftertouch are first-class learnable types
//     alongside CC; poly aftertouch deferred (rare in beginner gear).
//
// Threading:
//   * Mutators (setMapping / removeMapping / load / save) on message thread
//     under mLock (juce::SpinLock).
//   * Audio thread dispatch (`dispatchEvent`) try-locks; if it can't, skips
//     the event (one missed CC frame is unnoticeable for human-driven CCs).
//   * Lookup is O(N mappings) per dispatch -- expected count is small (<100).
//     Keeps the data model simple; can switch to a hashed lookup if profiling
//     shows it.
//
// Persistence (Jeff's ruling, 2026-08-24: PROJECT-ONLY, nothing else):
//   * The table is project state, exactly like the mixer.  Every project and
//     template save embeds the full table; every load boundary (Open, New,
//     template apply) REPLACES it -- the file's table when it has one, empty
//     when it does not.  Nothing ever carries from one project to the next.
//   * The old global-defaults layer (a Documents-level MidiMappings.xml + a
//     "Save as global default" menu row) was removed under the same ruling:
//     it made bindings smear across projects through the session with no UI
//     showing which file held what.
//
// Value mapping curve:
//   * v1 ships LINEAR only (CC 0..127 -> param min..max via NormalisableRange).
//   * Per-mapping `formula` field reserved for I-3c power-user formula
//     evaluation (FL-style).  Empty formula = linear.
// ─────────────────────────────────────────────────────────────────────────────

class MidiLearnRegistry
{
public:
    MidiLearnRegistry() = default;
    ~MidiLearnRegistry() = default;

    enum class MessageType
    {
        Cc              = 0,    // Control Change (uses ccNumber 0..127)
        PitchBend       = 1,    // 14-bit value 0..16383 -> normalised 0..1
        ChannelPressure = 2,    // 7-bit value 0..127
        // PolyAftertouch deferred -- rare in beginner gear, learnable later.
    };

    struct Mapping
    {
        juce::String paramId;                            // APVTS paramId
        MessageType  msgType    { MessageType::Cc };
        int          ccNumber   { 0 };                   // 0..127 for CC
        int          channel    { 0 };                   // 0 = Omni, 1..16
        juce::String deviceName;                         // empty = any device
        juce::String formula;                            // empty = linear (I-3c)
    };

    // ── Mutation (message thread) ─────────────────────────────────────────────
    // Set/replace the mapping for this paramId.  One paramId can have at most
    // one mapping; re-learning replaces.  Notifies onChanged listener.
    void setMapping (const juce::String& paramId, const Mapping& m);

    // Remove the mapping for this paramId.  No-op if none exists.
    void removeMapping (const juce::String& paramId);

    // Wipe the registry (used by setStateInformation before restoring).
    void clear();

    // ── Read (any thread, lock-protected) ─────────────────────────────────────
    bool getMapping (const juce::String& paramId, Mapping& out) const;
    bool hasMapping (const juce::String& paramId) const;
    juce::Array<juce::String> getAllParamIds() const;

    // ── Audio-thread dispatch ────────────────────────────────────────────────
    // Given an incoming MIDI message + source device name, find any matching
    // mapping(s) and apply via apvts->getParameter()->setValueNotifyingHost.
    // Block-rate per locked spec (Jeff 2026-05-02): events fire at the audio
    // block boundary, not sample-accurate within the block.
    //
    // Returns the number of mappings that fired (informational; UI doesn't
    // need this today but I-3c "MIDI activity" indicator may use it).
    int dispatchEvent (juce::AudioProcessorValueTreeState& apvts,
                       const juce::String& sourceDeviceName,
                       const juce::MidiMessage& msg);

    // ── Learn-mode capture (used by I-3c right-click menu) ───────────────────
    // The UI calls beginLearn(paramId), then polls takeCapturedMapping() on the
    // message thread and commits with setMapping().
    //
    // 2026-07-19: the capture callback was REMOVED.  It ran on the audio thread
    // and, between the std::function copy, the callback's own
    // MessageManager::callAsync, the std::map insert, and the onChanged()
    // notify, put four separate heap allocations in the render callback.  The
    // audio thread now only fills a pre-constructed Mapping under the existing
    // lock (refcount traffic on already-owned strings, no allocation) and sets
    // a flag; every allocating step moved to the message thread.  Same handshake
    // shape as DrumTriggerMap.
    void beginLearn (const juce::String& paramId);
    void cancelLearn();
    bool isLearning() const noexcept;
    juce::String getLearnTargetParamId() const;

    // Message thread.  Returns true and fills `out` exactly once per capture.
    bool takeCapturedMapping (Mapping& out);

    // Audio-thread (or MIDI thread): if a learn target is active and this event
    // is a learnable type, record it for the message thread and return true
    // (caller suppresses the event from regular dispatch).
    bool tryCaptureLearn (const juce::String& sourceDeviceName,
                          const juce::MidiMessage& msg);

    // ── Persistence ─────────────────────────────────────────────────────────
    static constexpr const char* kRootTag = "MidiCCMappings";

    juce::ValueTree saveToValueTree() const;
    void            loadFromValueTree (const juce::ValueTree& tree);

    // Documents/BaySickDAW/MidiMappings.xml
    // Every mapping, copied under the lock -- the Help > View Projects
    // MidiMap window's data source.
    std::vector<Mapping> getAllMappings() const;

    // ── Listener ────────────────────────────────────────────────────────────
    // Fired (on the message thread) whenever any mapping changes.  UI uses
    // this to refresh the right-click menu state.
    std::function<void()> onChanged;

private:
    // Returns the normalised [0..1] value derived from `msg` for this msgType.
    // Returns -1.0f if the message doesn't match (caller should skip).
    static float extractNormalisedValue (MessageType expected,
                                          const juce::MidiMessage& msg);

    // Returns true if the mapping rule matches the incoming event (msgType +
    // ccNumber + channel + deviceName).  Channel == 0 (Omni) matches any
    // channel; deviceName.isEmpty() matches any device.
    static bool eventMatchesMapping (const Mapping& m,
                                     const juce::String& sourceDeviceName,
                                     const juce::MidiMessage& msg);

    // Build a Mapping from an incoming MIDI event for learn-capture.  Returns
    // false if the event isn't a learnable type.
    static bool buildMappingFromEvent (Mapping& outM,
                                        const juce::String& sourceDeviceName,
                                        const juce::MidiMessage& msg);

    mutable juce::SpinLock          mLock;
    std::map<juce::String, Mapping> mMappings;

    // Learn-mode state (also under mLock).
    juce::String mLearnTargetParamId;
    // Pre-constructed so the audio thread only ASSIGNS into it (refcount ops on
    // strings it does not solely own) rather than constructing/destroying one.
    Mapping           mPendingCapture;
    std::atomic<bool> mCaptureReady { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnRegistry)
};


// ─────────────────────────────────────────────────────────────────────────────
// MidiLearnEventQueue - MIDI thread -> audio thread bridge
// ─────────────────────────────────────────────────────────────────────────────
// JUCE's MidiInputCallback runs on a per-device MIDI input thread.  We need
// each event tagged with its source-device name when dispatch reaches the
// audio thread (so device-locked mappings can filter).  This queue stores
// device-tagged events between the MIDI thread (push) and the audio thread
// (drain on each processBlock).
//
// Threading:
//   * Push (MIDI input thread, possibly multiple devices = MPSC): briefly
//     locks mLock.  Push is O(1) -- a single push_back into a pre-reserved
//     vector.
//   * Drain (audio thread): try-locks; on success, swaps with the empty buffer
//     and releases the lock, then iterates events outside it.  On failure
//     (rare race with MIDI thread mid-push), skips drain this block; events
//     persist for next block.  The release matters: the callbacks reach
//     setValueNotifyingHost, and holding across them would make the MIDI input
//     thread spin-wait in push() for the whole notification.
//
// Bounded capacity: events backlog if user holds wheel for many seconds
// without audio thread running (e.g., audio device closed mid-session).
// kMaxBacklog = 1024 events; oldest dropped past that.
// ─────────────────────────────────────────────────────────────────────────────

class MidiLearnEventQueue
{
public:
    // Device names are INTERNED to small integer ids rather than stored per
    // event (2026-07-19).  The queue previously held a juce::String per Event;
    // juce::String is refcounted, the queue was the sole owner by the time the
    // audio thread drained (MidiInput::getName() returns by value, and the
    // MIDI-thread local died when its callback returned), so the drain's
    // clear() dropped the last reference and ran free() INSIDE the render
    // callback.  An int id is trivially destructible, so the drain now frees
    // nothing.
    struct Event
    {
        int               deviceId { -1 };   // index into the interned name table
        juce::MidiMessage message;
    };

    static constexpr int kMaxBacklog = 1024;
    // Interned device names live for the queue's lifetime.  Fixed array, never
    // resized, entries never destroyed -- that is what makes it safe for the
    // audio thread to hold a const& into it while the MIDI thread appends.
    static constexpr int kMaxDevices = 32;

    MidiLearnEventQueue()
    {
        mPending .reserve (kMaxBacklog);
        mDraining.reserve (kMaxBacklog);
    }

    // MIDI input thread (multi-producer).  Interning takes its own lock and
    // completes BEFORE the queue lock is taken -- juce::SpinLock is not
    // recursive, so these must never nest.
    void push (const juce::String& deviceName, const juce::MidiMessage& msg)
    {
        const int id = internDeviceName (deviceName);

        const juce::SpinLock::ScopedLockType lk (mLock);
        if ((int) mPending.size() >= kMaxBacklog)
            mPending.erase (mPending.begin());   // drop oldest
        mPending.push_back ({ id, msg });
    }

    // Audio thread (single consumer).  `fn` is called as
    // fn (const juce::String& deviceName, const juce::MidiMessage&) -- the
    // name is a reference INTO the intern table, never a copy, so no
    // refcounted type is constructed or destroyed on this thread.
    //
    // RT-safety note: juce::MidiMessage heap-allocates only above 8 bytes
    // (PackedData union).  Everything this queue admits -- CC, pitch-bend,
    // channel pressure -- is 2-3 bytes and lives inline, so ~MidiMessage frees
    // nothing either.  Admitting sysex here would reintroduce an audio-thread
    // free; filter before pushing, not after draining.
    template <typename Fn>
    bool drainAndProcess (Fn&& fn)
    {
        {
            const juce::SpinLock::ScopedTryLockType tryLk (mLock);
            if (! tryLk.isLocked()) return false;
            std::swap (mPending, mDraining);
        }
        // Lock released before the callbacks (restored 2026-07-19; it had been
        // held across the whole loop, which the class comment never matched).
        // Safe because mDraining is audio-thread-private once swapped -- the
        // MIDI thread only ever touches mPending, and only under the lock.
        // Worth keeping that way: `fn` reaches setValueNotifyingHost, so
        // holding here makes the MIDI input thread spin-wait in push() for the
        // duration of parameter notification.
        for (const auto& e : mDraining)
            fn (deviceNameForId (e.deviceId), e.message);
        const bool any = ! mDraining.empty();
        mDraining.clear();   // ints only -- frees nothing
        return any;
    }

private:
    // MIDI thread.  Linear scan is fine: device count is tiny and this runs
    // once per incoming event, not per sample.  Returns -1 when the table is
    // full (event still queues; it just reports an empty device name).
    int internDeviceName (const juce::String& name)
    {
        const juce::SpinLock::ScopedLockType lk (mDeviceLock);
        const int n = mDeviceCount.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
            if (mDeviceNames[(size_t) i] == name)
                return i;
        if (n >= kMaxDevices) return -1;
        mDeviceNames[(size_t) n] = name;
        // Release: publishes the name write before the audio thread can see
        // an id that indexes it.
        mDeviceCount.store (n + 1, std::memory_order_release);
        return n;
    }

    // Audio thread.  Safe without the device lock: slots below the acquired
    // count are fully written and never mutated again.
    const juce::String& deviceNameForId (int id) const noexcept
    {
        static const juce::String kNoDevice;
        if (id < 0 || id >= mDeviceCount.load (std::memory_order_acquire))
            return kNoDevice;
        return mDeviceNames[(size_t) id];
    }

    juce::SpinLock     mLock;
    std::vector<Event> mPending;
    std::vector<Event> mDraining;

    juce::SpinLock                            mDeviceLock;
    std::array<juce::String, kMaxDevices>     mDeviceNames;
    std::atomic<int>                          mDeviceCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnEventQueue)
};
