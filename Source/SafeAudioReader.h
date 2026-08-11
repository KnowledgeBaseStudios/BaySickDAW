#pragma once
#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
// SafeAudioReader - sanity gate on an AudioFormatReader built from a file we
// did not write.
//
// SECURITY (QA-Cleanup 2026-08-10, CL-289 Tier-1 part 2 - file-parser audit).
//
// THE LIVELOCK.  JUCE reads audio through a fixed 5,760-byte scratch buffer and
// works out how many frames fit each pass:
//
//     numThisTime = jmin (tempBufSize / bytesPerFrame, numSamples);
//     (juce_WavAudioFormat.cpp:1493-1513, and identically in
//      juce_AiffAudioFormat.cpp:598-620)
//
// `bytesPerFrame` is channels x bit-depth, both taken from the file's header
// with no upper limit (`numChannels` at juce_WavAudioFormat.cpp:1267 is never
// checked).  Declare ~3,000 channels at 16-bit and one frame is 6,000 bytes, so
// that division is ZERO: the loop reads nothing, subtracts nothing from the
// remaining count, and goes round again.  Forever, at 100% of one core.  The
// declared data-chunk size is never compared against the real file, so the file
// itself can be a hundred bytes.
//
// If that happens on the message thread the whole app is frozen - no redraw, no
// menu, no save - and Task Manager is the only way out.
//
// WHY THE EARLIER FIX DID NOT COVER IT.  BaySickPlayerDSP clamps the
// DESTINATION channel count and bounds the sample count, which stops the
// ALLOCATION being absurd.  It does not touch the loop, because
// `reader->read(...)` uses the READER's channel count, not ours.  Bounding what
// we allocate and bounding what JUCE iterates are two different problems.
//
// The ceiling is deliberately generous: the app never plays more than stereo,
// and 32 channels is far past any real instrument sample while still leaving
// room for oddities like a multichannel field recording someone drops in.
// ─────────────────────────────────────────────────────────────────────────────
namespace SafeAudioReader
{
    inline constexpr unsigned int kMaxChannels = 32;

    // The widest single FRAME the WAV read loop can make progress on.  Named for
    // what it bounds rather than where the number came from: it is compared
    // against bytes-per-frame below, and the old name (kJuceScratchBytes) read
    // like a buffer capacity, which invited someone to compare a buffer size
    // against it.  The number itself IS JUCE's scratch-buffer size - a frame
    // wider than the scratch buffer means the loop copies zero frames per
    // iteration and never advances, which is the livelock.  Kept explicit so the
    // reason survives if JUCE ever changes that size.
    inline constexpr int kMaxBytesPerFrame = 5760;

    inline bool isPlausible (const juce::AudioFormatReader* r) noexcept
    {
        if (r == nullptr) return false;

        if (r->numChannels == 0 || r->numChannels > kMaxChannels) return false;
        if (r->bitsPerSample == 0 || r->bitsPerSample > 64)       return false;
        if (r->sampleRate <= 0.0 || r->sampleRate > 768000.0)     return false;
        if (r->lengthInSamples < 0)                               return false;

        // The livelock condition itself.
        const auto bytesPerFrame = (juce::int64) r->numChannels
                                 * (juce::int64) (r->bitsPerSample / 8);
        if (bytesPerFrame <= 0 || bytesPerFrame > kMaxBytesPerFrame) return false;

        return true;
    }

    // Drops an implausible reader rather than handing it to a read loop.  A
    // null return is already every caller's "could not read this file" path, so
    // this needs no new error handling at the call sites.
    inline std::unique_ptr<juce::AudioFormatReader>
    guard (std::unique_ptr<juce::AudioFormatReader> r)
    {
        if (! isPlausible (r.get()))
            return {};

        return r;
    }

    inline std::unique_ptr<juce::AudioFormatReader>
    create (juce::AudioFormatManager& fm, const juce::File& f)
    {
        return guard (std::unique_ptr<juce::AudioFormatReader> (fm.createReaderFor (f)));
    }
}
