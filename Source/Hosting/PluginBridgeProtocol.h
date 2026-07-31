#pragma once
#include <JuceHeader.h>
#include <cstdint>

// ── Hosting::Bridge ─────────────────────────────────────────────────────────
// QA-ModelShell TS6 (BLU-302): the wire format between BaySickDAW and a
// plugin-host helper process.
//
// ARCHITECTURE-NEUTRAL BY CONSTRUCTION, and that is the entire design
// constraint rather than a nicety.  The helper is built TWICE -- x64 and x86 --
// because a process can only load a plugin of its own architecture, which is
// the whole reason a 32-bit plugin needs a bridge at all.  A 64-bit host
// therefore talks to a 32-bit peer across every message, so:
//
//   * every field is a FIXED-WIDTH integer or float.  No int, no long, no
//     size_t, no bool (1 byte is not guaranteed to pack the same), no enum
//     without an explicit underlying type.
//   * no pointer EVER crosses the wire.  Window handles cross as uint64 and
//     are cast at the receiving end.
//   * structs are explicitly packed and asserted, so a compiler that pads
//     differently between the two builds fails the build rather than
//     misreading messages at runtime.
//
// Getting this right up front costs nothing; retrofitting it onto a protocol
// written 64-to-64 would mean re-auditing every message.  That is why 7b (the
// 32-bit build) is cheap and why it was not deferred.
// ─────────────────────────────────────────────────────────────────────────────

namespace Hosting::Bridge
{

// Bumped whenever the layout below changes.  The helper reports its version in
// the handshake and the host refuses a mismatch -- a stale helper binary left
// over from an older install would otherwise misparse every message.
// 2 (TS7, 2026-07-31): ProcessPayload gained transport, and the MIDI trailer
// the original comment promised is now actually sent.  Host and helper are built
// from this header by the same do_build.bat run, so they cannot disagree.
inline constexpr std::uint32_t kProtocolVersion = 2;

// Fits comfortably inside JUCE's InterprocessConnection message framing while
// leaving room for the largest realistic plugin state blob; anything bigger is
// chunked by the sender.
inline constexpr std::uint32_t kMaxChunkBytes = 1u << 20;   // 1 MiB

enum class MessageType : std::uint32_t
{
    // host -> helper
    Handshake      = 1,
    LoadPlugin     = 2,
    Prepare        = 3,
    Process        = 4,
    SetParameter   = 5,
    GetState       = 6,
    SetState       = 7,
    OpenEditor     = 8,
    CloseEditor    = 9,
    Shutdown       = 10,

    // helper -> host
    HandshakeReply = 101,
    LoadReply      = 102,
    ProcessReply   = 103,
    StateBlob      = 104,
    ParameterList  = 105,
    EditorOpened   = 106,
    Error          = 107,
};

#pragma pack (push, 1)

// Every message opens with this.  `payloadBytes` counts what follows the
// header, so a reader can skip a message type it does not know.
struct Header
{
    std::uint32_t type;          // MessageType
    std::uint32_t payloadBytes;
    std::uint32_t sequence;      // echoed in replies, so a late reply is discardable
    std::uint32_t reserved;      // keeps the header 16 bytes on both architectures
};

struct HandshakePayload
{
    std::uint32_t protocolVersion;
    std::uint32_t hostArchBits;   // 32 or 64 -- logged, never used to branch layout
};

struct PreparePayload
{
    double        sampleRate;
    std::uint32_t maxBlockSize;
    std::uint32_t numChannels;
};

// Audio itself does NOT ride this protocol -- it moves through the shared
// memory block the LoadReply names, because copying every block through the
// IPC pipe would put an unbounded allocation and a syscall on the audio path.
// This message is the per-block doorbell only.
struct ProcessPayload
{
    std::uint32_t numSamples;
    std::uint32_t numMidiBytes;    // MIDI trailer follows; see kMaxMidiBytesPerBlock

    // TS7 (2026-07-31): TRANSPORT.  Added because a hosted plugin had no
    // playhead at all -- unbridged it never got setPlayHead forwarded to the
    // inner instance, and bridged there was nowhere to put it.  Anything with an
    // arpeggiator, step sequencer or tempo-synced delay/LFO therefore had no
    // host tempo to follow, which is also why forwarding a controller's MIDI
    // clock was papering over the real hole rather than fixing it.
    double        bpm;
    double        ppqPosition;
    std::int64_t  timeInSamples;
    std::uint32_t isPlaying;
    std::uint32_t timeSigNumerator;
    std::uint32_t timeSigDenominator;
    std::uint32_t reserved;        // keeps the struct a round 48 bytes on both arches
};

// MIDI trailer cap.  A block never carries anywhere near this; the bound exists
// so a runaway buffer cannot make the frame unbounded on the audio thread.
inline constexpr std::uint32_t kMaxMidiBytesPerBlock = 4096;

// Trailer wire format, written and read by hand rather than by copying JUCE's
// MidiBuffer storage: the host is x64 and a bridged helper may be x86, and this
// file's whole premise is that a layout difference between the two silently
// shifts every field after it.  Per event: int32 samplePosition, int32 numBytes,
// then numBytes of raw MIDI.

struct SetParameterPayload
{
    std::uint32_t paramIndex;
    float         value01;
};

struct EditorPayload
{
    std::uint64_t parentWindowHandle;   // HWND as an integer -- never a pointer type
    std::int32_t  width;
    std::int32_t  height;
};

struct LoadReplyPayload
{
    std::uint32_t ok;                 // 0 = failed; message text follows on Error
    std::uint32_t numParameters;
    std::uint32_t latencySamples;
    std::uint32_t sharedMemoryBytes;
};

#pragma pack (pop)

// A padding difference between the x64 and x86 builds would silently shift
// every field after it, so the layout is asserted rather than trusted.
static_assert (sizeof (Header)              == 16, "Bridge::Header must be 16 bytes on every target");
static_assert (sizeof (HandshakePayload)    == 8,  "HandshakePayload layout drifted");
static_assert (sizeof (PreparePayload)      == 16, "PreparePayload layout drifted");
static_assert (sizeof (ProcessPayload)      == 48, "ProcessPayload layout drifted");
static_assert (sizeof (SetParameterPayload) == 8,  "SetParameterPayload layout drifted");
static_assert (sizeof (EditorPayload)       == 16, "EditorPayload layout drifted");
static_assert (sizeof (LoadReplyPayload)    == 16, "LoadReplyPayload layout drifted");

// Helper for building a framed message: header + payload + optional trailer
// (state blob / MIDI bytes / a UTF-8 string).
inline juce::MemoryBlock frame (MessageType type, std::uint32_t sequence,
                                const void* payload, std::uint32_t payloadBytes,
                                const void* trailer = nullptr, std::uint32_t trailerBytes = 0)
{
    Header h {};
    h.type         = static_cast<std::uint32_t> (type);
    h.payloadBytes = payloadBytes + trailerBytes;
    h.sequence     = sequence;
    h.reserved     = 0;

    juce::MemoryBlock mb;
    mb.append (&h, sizeof (h));

    if (payload != nullptr && payloadBytes > 0)
        mb.append (payload, payloadBytes);

    if (trailer != nullptr && trailerBytes > 0)
        mb.append (trailer, trailerBytes);

    return mb;
}

// The pipe name both sides agree on.  The helper receives it on its command
// line; including the PID keeps two host instances (Jeff runs Debug and Release
// side by side) from colliding on the same pipe.
inline juce::String pipeNameFor (std::uint32_t hostPid, std::uint32_t slotId)
{
    return "BaySickPluginBridge_" + juce::String ((int) hostPid)
         + "_" + juce::String ((int) slotId);
}

} // namespace Hosting::Bridge
