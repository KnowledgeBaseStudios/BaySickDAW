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
// the original comment promised is now actually sent.
// 3 (2026-07-31, batch review): the per-block path LEFT THE PIPE ENTIRELY --
// audio, MIDI, transport and the reply all ride the shared mapping's control
// header, with a pair of named events as the rendezvous.  The pipe send was
// never audio-thread-safe: framing allocated a MemoryBlock per block, and a
// stalled helper let the pipe write block the audio thread for up to the
// 15-second pipe timeout.  LoadPlugin's trailer became path + '\n' +
// identifier, because the identifier alone contains no path and the helper
// could never resolve a plugin from it.  Host and helper are built from this
// header by the same do_build.bat run, so they cannot disagree.
// 4 (2026-08-02): ProgramInfo (helper -> host) -- current program index/count
// + the current program's name as a UTF-8 trailer, pushed after load and on
// every program change the plugin reports.  Feeds the tab/strip preset-name
// linkage; plugins that never publish programs simply never send a name.
// 5 (2026-08-02): ParamTouched (helper -> host) -- the parameter the USER just
// moved inside the plugin's own UI, as value + "id \t name" trailer.  Feeds
// "Automate last touched" for bridged plugins (a foreign editor has no
// right-click hook of ours).  Bursts coalesce helper-side, and changes the
// HOST itself just sent (automation playback) are suppressed at the source so
// lane replay does not masquerade as user touches.
inline constexpr std::uint32_t kProtocolVersion = 5;

enum class MessageType : std::uint32_t
{
    // host -> helper
    Handshake      = 1,
    LoadPlugin     = 2,
    Prepare        = 3,
    Process        = 4,   // RESERVED since v3 -- per-block work rides the shared mapping
    SetParameter   = 5,
    GetState       = 6,
    SetState       = 7,
    OpenEditor     = 8,
    CloseEditor    = 9,
    Shutdown       = 10,

    // helper -> host
    HandshakeReply = 101,
    LoadReply      = 102,
    ProcessReply   = 103,   // RESERVED since v3 -- the reply rides the shared mapping
    StateBlob      = 104,
    ParameterList  = 105,
    EditorOpened   = 106,
    Error          = 107,
    ProgramInfo    = 108,   // v4: payload + current program's name as trailer
    ParamTouched   = 109,   // v5: payload + "id \t name" trailer
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

// Per-block MIDI cap.  A block never carries anywhere near this; the bound
// exists so a runaway buffer cannot overrun the control header's fixed area.
inline constexpr std::uint32_t kMaxMidiBytesPerBlock = 4096;

// v3: NOTHING per-block rides this protocol.  Audio, MIDI, transport and the
// reply all live in the shared mapping's control header (see
// BridgeSharedMemory.h SharedBlockControl), and a pair of named auto-reset
// events is the rendezvous -- the audio thread never touches the pipe.  MIDI
// wire format inside the control area, written and read by hand rather than by
// copying JUCE's MidiBuffer storage (the helper may be x86): per event,
// int32 samplePosition, int32 numBytes, then numBytes of raw MIDI.

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
    // v3: what the loaded plugin actually IS.  A 32-bit plugin cannot be
    // opened by the 64-bit scanner, so its scan-time description is a
    // filename-only guess -- the first real load corrects it from here.
    std::uint32_t isInstrument;       // 0/1
    std::uint32_t acceptsMidi;        // 0/1
};

// v4: current program state.  Only the CURRENT program's name crosses (as the
// message trailer) -- the naming linkage needs nothing else, and a full
// program-list sync would be dead weight for the majority of plugins that
// report a single unnamed program.
struct ProgramInfoPayload
{
    std::int32_t numPrograms;
    std::int32_t currentProgram;
};

// v5: a user touch inside the plugin's own editor.  Trailer carries the
// stable parameter id and display name ("id \t name") -- lanes key on the id,
// the menu shows the name.
struct ParamTouchedPayload
{
    float value01;
};

#pragma pack (pop)

// A padding difference between the x64 and x86 builds would silently shift
// every field after it, so the layout is asserted rather than trusted.
static_assert (sizeof (Header)              == 16, "Bridge::Header must be 16 bytes on every target");
static_assert (sizeof (HandshakePayload)    == 8,  "HandshakePayload layout drifted");
static_assert (sizeof (PreparePayload)      == 16, "PreparePayload layout drifted");
static_assert (sizeof (SetParameterPayload) == 8,  "SetParameterPayload layout drifted");
static_assert (sizeof (EditorPayload)       == 16, "EditorPayload layout drifted");
static_assert (sizeof (LoadReplyPayload)    == 24, "LoadReplyPayload layout drifted");
static_assert (sizeof (ProgramInfoPayload)  == 8,  "ProgramInfoPayload layout drifted");
static_assert (sizeof (ParamTouchedPayload) == 4,  "ParamTouchedPayload layout drifted");

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

} // namespace Hosting::Bridge
