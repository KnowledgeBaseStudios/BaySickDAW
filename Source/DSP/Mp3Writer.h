#pragma once

#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
// Mp3Writer - QA-Export Task 1 (docket 7=A).
//
// Thin float-buffer front end over vendored libmp3lame (libs/lame, LGPL, 3.100).
// JUCE ships WAV/OGG/FLAC encoders but no MP3 encoder; its LAME wrapper shells
// out to a lame.exe we would have to redistribute, so the library is linked
// instead.
//
// Offline use only - export renders on a background thread, never the audio
// thread.  Allocates in open() and encode() (lame's own buffers plus the mp3
// output block), which is why this must not be reached from a render callback.
// ─────────────────────────────────────────────────────────────────────────────
class Mp3Writer
{
public:
    Mp3Writer() = default;
    ~Mp3Writer();

    Mp3Writer (const Mp3Writer&)            = delete;
    Mp3Writer& operator= (const Mp3Writer&) = delete;

    // `bitrateKbps` is CBR: 128 / 192 / 256 / 320 per the export dialog.
    // Returns false and fills `outErr` if lame init or the file open fails.
    bool open (const juce::File& destination,
               double            sampleRate,
               int               numChannels,
               int               bitrateKbps,
               juce::String&     outErr);

    // Interleaved-free: takes JUCE's channel-planar layout directly.  Accepts
    // mono or stereo; a mono source opened as stereo is duplicated by lame.
    // Sample values are the usual -1..+1 float range.
    bool write (const float* const* channelData, int numSamples);

    // Flushes lame's final frame + rewrites the LAME/Xing VBR-info header, then
    // releases everything.  Safe to call twice; the destructor calls it.
    bool close();

    bool isOpen() const noexcept { return mFlags != nullptr; }

private:
    // Opaque so lame.h stays out of every translation unit that exports audio.
    void*                             mFlags { nullptr };   // lame_global_flags*
    std::unique_ptr<juce::FileOutputStream> mStream;
    juce::File                        mFile;
    int                               mNumChannels { 2 };
    std::vector<unsigned char>        mMp3Buffer;
};
