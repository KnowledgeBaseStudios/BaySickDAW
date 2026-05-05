#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <map>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// MidiLearnRegistry — Phase I-3b (2026-05-02)
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
// Persistence:
//   * Per-project: serialised inside the project's PluginProcessor::getState
//     output as a `<MidiCCMappings>` ValueTree child.  Loaded by
//     setStateInformation on project open.
//   * Global default: Documents/BaySickDAW/MidiMappings.xml -- saved when
//     the user picks "Save as global default" in the right-click menu (I-3c).
//     Loaded at app launch BEFORE per-project state restores; per-project
//     mappings overlay on top of globals.
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

    // ── Learn-mode capture hook (used by I-3c right-click menu) ──────────────
    // When the user clicks "MIDI Learn" on a knob, the UI calls
    // beginLearn(paramId, callback).  The next incoming hardware MIDI message
    // that matches a learnable type triggers the callback with the captured
    // Mapping fields populated; the callback is then cleared.  The callback
    // returning true commits the mapping; false cancels (e.g., user pressed
    // Escape while the message was in flight).  Audio thread calls
    // tryCaptureLearn() before normal dispatch so the capture event isn't
    // also dispatched as a regular CC update.
    using LearnCaptureFn = std::function<bool(const Mapping&)>;
    void beginLearn (const juce::String& paramId, LearnCaptureFn fn);
    void cancelLearn();
    bool isLearning() const noexcept;
    juce::String getLearnTargetParamId() const;

    // Audio-thread (or MIDI thread): if a learn target is active, see if this
    // event matches a learnable type; if so, fire the capture callback and
    // return true (caller suppresses the event from regular dispatch).
    bool tryCaptureLearn (const juce::String& sourceDeviceName,
                          const juce::MidiMessage& msg);

    // ── Persistence ─────────────────────────────────────────────────────────
    static constexpr const char* kRootTag = "MidiCCMappings";

    juce::ValueTree saveToValueTree() const;
    void            loadFromValueTree (const juce::ValueTree& tree);

    // Documents/BaySickDAW/MidiMappings.xml
    static juce::File globalDefaultsFile();
    bool saveAsGlobalDefaults() const;
    bool loadGlobalDefaults();   // returns true if file existed and parsed

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
    juce::String   mLearnTargetParamId;
    LearnCaptureFn mLearnCaptureFn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnRegistry)
};


// ─────────────────────────────────────────────────────────────────────────────
// MidiLearnEventQueue — MIDI thread -> audio thread bridge
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
//   * Drain (audio thread): try-locks; on success, swaps with empty buffer
//     and releases lock, then iterates events outside the lock.  On failure
//     (rare race with MIDI thread mid-push), skips drain this block; events
//     persist for next block.
//
// Bounded capacity: events backlog if user holds wheel for many seconds
// without audio thread running (e.g., audio device closed mid-session).
// kMaxBacklog = 1024 events; oldest dropped past that.
// ─────────────────────────────────────────────────────────────────────────────

class MidiLearnEventQueue
{
public:
    struct Event
    {
        juce::String     deviceName;
        juce::MidiMessage message;
    };

    static constexpr int kMaxBacklog = 1024;

    MidiLearnEventQueue()
    {
        mPending .reserve (kMaxBacklog);
        mDraining.reserve (kMaxBacklog);
    }

    // MIDI input thread (multi-producer).  Brief lock held while appending.
    void push (const juce::String& deviceName, const juce::MidiMessage& msg)
    {
        const juce::SpinLock::ScopedLockType lk (mLock);
        if ((int) mPending.size() >= kMaxBacklog)
            mPending.erase (mPending.begin());   // drop oldest
        mPending.push_back ({ deviceName, msg });
    }

    // Audio thread (single consumer).  Swap-and-process pattern: take the
    // pending vector under lock, process events outside the lock to keep MIDI
    // input threads unblocked.  Returns true if at least one event was
    // processed.
    template <typename Fn>
    bool drainAndProcess (Fn&& fn)
    {
        const juce::SpinLock::ScopedTryLockType tryLk (mLock);
        if (! tryLk.isLocked()) return false;
        std::swap (mPending, mDraining);
        // Release lock here -- ScopedTryLockType exits scope after we swap.
        // Process outside lock (we're the only consumer).
        for (const auto& e : mDraining)
            fn (e.deviceName, e.message);
        const bool any = ! mDraining.empty();
        mDraining.clear();
        return any;
    }

private:
    juce::SpinLock     mLock;
    std::vector<Event> mPending;
    std::vector<Event> mDraining;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnEventQueue)
};
