#pragma once

#include <JuceHeader.h>

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

class SharedAudioBlock
{
public:
    SharedAudioBlock() = default;
    ~SharedAudioBlock() { close(); }

    SharedAudioBlock (const SharedAudioBlock&)            = delete;
    SharedAudioBlock& operator= (const SharedAudioBlock&) = delete;

    // HOST side: makes the mapping and hands its name to the helper.
    bool create (const juce::String& name, size_t bytes)
    {
        close();
        if (bytes == 0) return false;

        mHandle = ::CreateFileMappingW (INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                        (DWORD) ((juce::uint64) bytes >> 32),
                                        (DWORD) ((juce::uint64) bytes & 0xffffffffull),
                                        name.toWideCharPointer());
        return finishMapping (bytes);
    }

    // HELPER side: opens the mapping the host named in Prepare.
    bool open (const juce::String& name, size_t bytes)
    {
        close();
        if (bytes == 0) return false;

        mHandle = ::OpenFileMappingW (FILE_MAP_ALL_ACCESS, FALSE, name.toWideCharPointer());
        return finishMapping (bytes);
    }

    void close()
    {
        if (mData   != nullptr) { ::UnmapViewOfFile (mData); mData = nullptr; }
        if (mHandle != nullptr) { ::CloseHandle (mHandle);   mHandle = nullptr; }
        mBytes = 0;
    }

    float* data()      const noexcept { return mData; }
    size_t sizeBytes() const noexcept { return mBytes; }
    bool   isValid()   const noexcept { return mData != nullptr; }

    // Channel pointer for the agreed channel-major layout.  Returns null rather
    // than running off the end when the caller's idea of the geometry disagrees
    // with what was mapped -- on the audio thread that is a silent block, not a
    // crash in someone else's plugin.
    float* channel (int index, int maxBlockSize) const noexcept
    {
        if (mData == nullptr || index < 0 || maxBlockSize <= 0) return nullptr;
        const size_t offsetFloats = (size_t) index * (size_t) maxBlockSize;
        if ((offsetFloats + (size_t) maxBlockSize) * sizeof (float) > mBytes) return nullptr;
        return mData + offsetFloats;
    }

    static size_t bytesFor (int maxBlockSize, int numChannels) noexcept
    {
        return (size_t) juce::jmax (0, maxBlockSize)
             * (size_t) juce::jmax (0, numChannels) * sizeof (float);
    }

private:
    bool finishMapping (size_t bytes)
    {
        if (mHandle == nullptr) return false;

        mData = static_cast<float*> (::MapViewOfFile (mHandle, FILE_MAP_ALL_ACCESS, 0, 0, bytes));

        if (mData == nullptr) { close(); return false; }

        mBytes = bytes;
        return true;
    }

    void*  mHandle { nullptr };
    float* mData   { nullptr };
    size_t mBytes  { 0 };
};

} // namespace Hosting::Bridge
