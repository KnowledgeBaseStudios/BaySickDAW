#pragma once

#include <JuceHeader.h>
#include "PluginBridgeProtocol.h"

#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
 #define NOMINMAX
#endif
#include <windows.h>

// ── Hosting::Bridge::SharedAudioBlock ────────────────────────────────────────
// QA-ModelShell TS6 (BLU-302) -- the audio half of the plugin bridge, which was
// scaffolded and never built (fixed TS7, 2026-07-31).
//
// The client carried a `juce::MemoryBlock mSharedAudio`, sized in prepare() and
// released in releaseResources() and NEVER READ OR WRITTEN.  A MemoryBlock is
// process-local anyway, so even had it been used the helper could not have seen
// it: a bridged plugin had no audio path in either direction and could not make
// sound at all, which for a 32-bit VST3 -- whose ONLY route is the bridge --
// meant it could never work.
//
// A named Win32 file mapping is the transport.  Deliberately not the IPC pipe:
// the pipe would put a copy and a syscall on the audio path per block, which is
// the reason the original design named shared memory in the first place.
//
// HEADER-ONLY on purpose.  The 32-bit helper project's whole economy is that it
// builds ONE source file and links nothing but JUCE (see Helper/CMakeLists.txt);
// adding a .cpp would have to be added to both builds.  windows.h is pulled in
// here rather than in PluginBridgeProtocol.h so it reaches only the two .cpp
// files that need it and never a header JUCE UI code includes.
//
// LAYOUT is channel-major: channel 0's samples, then channel 1's, and so on.
// Both ends compute offsets from (maxBlockSize, numChannels), which the Prepare
// message carries, so neither side ever guesses.
namespace Hosting::Bridge
{

// v3: the per-block control header at the FRONT of the mapping.  Everything a
// block needs crosses here -- transport, MIDI, the reply -- so the audio thread
// never touches the IPC pipe (whose send both allocated and could block on the
// pipe timeout).  Same layout rules as the protocol header: fixed-width fields,
// packed, asserted, because the peer may be x86.
#pragma pack (push, 1)
struct SharedBlockControl
{
    std::uint32_t sequence;            // host writes before signaling
    std::uint32_t numSamples;
    std::uint32_t numMidiBytes;
    std::uint32_t nonRealtime;         // offline render flag, forwarded per block
    double        bpm;
    double        ppqPosition;
    std::int64_t  timeInSamples;
    std::uint32_t isPlaying;
    std::uint32_t timeSigNumerator;
    std::uint32_t timeSigDenominator;
    std::uint32_t replyOk;             // helper writes...
    std::uint32_t replySequence;       // ...echoing `sequence`, THEN signals done
    std::uint32_t reserved[3];
    std::uint8_t  midi[kMaxMidiBytesPerBlock];   // (int32 pos, int32 len, bytes)*
};
#pragma pack (pop)

static_assert (sizeof (SharedBlockControl) == 4 * 4 + 24 + 4 * 3 + 4 * 2 + 12 + kMaxMidiBytesPerBlock,
               "SharedBlockControl layout drifted");

class SharedAudioBlock
{
public:
    SharedAudioBlock() = default;
    ~SharedAudioBlock() { close(); }

    SharedAudioBlock (const SharedAudioBlock&)            = delete;
    SharedAudioBlock& operator= (const SharedAudioBlock&) = delete;

    // HOST side: makes the mapping + the two rendezvous events and hands their
    // shared name to the helper.
    bool create (const juce::String& name, size_t bytes)
    {
        close();
        if (bytes == 0) return false;

        mHandle = ::CreateFileMappingW (INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                        (DWORD) ((juce::uint64) bytes >> 32),
                                        (DWORD) ((juce::uint64) bytes & 0xffffffffull),
                                        name.toWideCharPointer());
        if (! finishMapping (bytes)) return false;

        // Auto-reset, so one signal wakes exactly one wait -- the rendezvous is
        // strictly one block at a time.
        mReq  = ::CreateEventW (nullptr, FALSE, FALSE, (name + "_req") .toWideCharPointer());
        mDone = ::CreateEventW (nullptr, FALSE, FALSE, (name + "_done").toWideCharPointer());
        if (mReq == nullptr || mDone == nullptr) { close(); return false; }
        std::memset (mData, 0, sizeof (SharedBlockControl));
        return true;
    }

    // HELPER side: opens the mapping + events the host named in Prepare.
    bool open (const juce::String& name, size_t bytes)
    {
        close();
        if (bytes == 0) return false;

        mHandle = ::OpenFileMappingW (FILE_MAP_ALL_ACCESS, FALSE, name.toWideCharPointer());
        if (! finishMapping (bytes)) return false;

        mReq  = ::OpenEventW (EVENT_ALL_ACCESS, FALSE, (name + "_req") .toWideCharPointer());
        mDone = ::OpenEventW (EVENT_ALL_ACCESS, FALSE, (name + "_done").toWideCharPointer());
        if (mReq == nullptr || mDone == nullptr) { close(); return false; }
        return true;
    }

    void close()
    {
        if (mData   != nullptr) { ::UnmapViewOfFile (mData); mData = nullptr; }
        if (mHandle != nullptr) { ::CloseHandle (mHandle);   mHandle = nullptr; }
        if (mReq    != nullptr) { ::CloseHandle (mReq);      mReq    = nullptr; }
        if (mDone   != nullptr) { ::CloseHandle (mDone);     mDone   = nullptr; }
        mBytes = 0;
    }

    size_t sizeBytes() const noexcept { return mBytes; }
    bool   isValid()   const noexcept { return mData != nullptr; }

    SharedBlockControl* control() const noexcept
    { return reinterpret_cast<SharedBlockControl*> (mData); }

    // ── The v3 rendezvous ────────────────────────────────────────────────────
    // Host: fill control + channels -> signalRequest -> waitDone(deadline).
    // Helper audio thread: waitRequest(poll) -> process -> signalDone.
    // Raw Win32 event calls: no allocation, kernel-bounded waits -- exactly the
    // properties the audio thread needs and the pipe send never had.
    void signalRequest() const noexcept { if (mReq  != nullptr) ::SetEvent (mReq); }
    void signalDone()    const noexcept { if (mDone != nullptr) ::SetEvent (mDone); }
    bool waitRequest (int ms) const noexcept
    { return mReq  != nullptr && ::WaitForSingleObject (mReq,  (DWORD) ms) == WAIT_OBJECT_0; }
    bool waitDone (int ms) const noexcept
    { return mDone != nullptr && ::WaitForSingleObject (mDone, (DWORD) ms) == WAIT_OBJECT_0; }

    // Channel pointer for the agreed channel-major layout, AFTER the control
    // header.  Returns null rather than running off the end when the caller's
    // idea of the geometry disagrees with what was mapped -- on the audio
    // thread that is a silent block, not a crash in someone else's plugin.
    float* channel (int index, int maxBlockSize) const noexcept
    {
        if (mData == nullptr || index < 0 || maxBlockSize <= 0) return nullptr;
        const size_t offsetBytes = sizeof (SharedBlockControl)
                                 + (size_t) index * (size_t) maxBlockSize * sizeof (float);
        if (offsetBytes + (size_t) maxBlockSize * sizeof (float) > mBytes) return nullptr;
        return reinterpret_cast<float*> (mData + offsetBytes);
    }

    static size_t bytesFor (int maxBlockSize, int numChannels) noexcept
    {
        return sizeof (SharedBlockControl)
             + (size_t) juce::jmax (0, maxBlockSize)
             * (size_t) juce::jmax (0, numChannels) * sizeof (float);
    }

private:
    bool finishMapping (size_t bytes)
    {
        if (mHandle == nullptr) return false;

        mData = static_cast<std::uint8_t*> (::MapViewOfFile (mHandle, FILE_MAP_ALL_ACCESS, 0, 0, bytes));

        if (mData == nullptr) { close(); return false; }

        mBytes = bytes;
        return true;
    }

    void*         mHandle { nullptr };
    std::uint8_t* mData   { nullptr };
    size_t        mBytes  { 0 };
    void*         mReq    { nullptr };
    void*         mDone   { nullptr };
};

} // namespace Hosting::Bridge
