#pragma once
#include <JuceHeader.h>

// ── AudioClipStreamer ─────────────────────────────────────────────────────────
// Disk-streaming audio engine for arrangement audio clips.
//
// Architecture:
//   - SPSC ring buffer (4 seconds at file sample rate).
//   - Background thread (TimeSliceClient) pre-fetches audio from disk.
//   - Audio thread calls readAndMix() with a stateless file position each block.
//   - Linear interpolation for sample-rate conversion.
//   - Seek detection: if filePos jumps outside the ring window, the audio thread
//     flags a seek and the background thread handles the refill asynchronously.
//     The audio thread returns silence for any block during a seek.
//
// Part 2 note: readAndMix() accepts readRatio from the caller so the phase
// vocoder (Part 2) can slot in above this layer without changing the streamer.

class AudioClipStreamer : public juce::TimeSliceClient
{
public:
    // Ring buffer covers kBufSeconds of audio at the file's native sample rate.
    static constexpr double kBufSeconds     = 4.0;
    // G-7 polish (2026-04-29): initial pre-fill bumped 2.0 → 3.5 so the ring
    // is ~88% full at playback start.  Combined with the bg-thread priority
    // bump (low → normal), this eliminates the first-bar skip on MP3 clips
    // whose per-chunk decode time was previously starving the ring before
    // the bg thread could refill.
    static constexpr double kPrefillSeconds = 3.5;   // synchronous pre-fill on seek
    static constexpr int    kChunkSize      = 8192;  // samples per background read

    // G-7 polish (2026-04-29): files at or under this raw float-PCM byte size
    // load entirely into RAM at construction so the audio thread never waits
    // on disk pre-fill or hits the cold-start sputter on first playback.
    //
    // Bumped 15 MB → 100 MB after Jeff hit MP3-specific cold-start at the
    // end of the pre-fill window even after raising kPrefillSeconds and
    // bg-thread priority.  Root cause: MP3 decode is ~10x slower per chunk
    // than WAV, so the streaming-path bg thread can't keep up with the
    // audio thread for long-form MP3s.  RAM-loading sidesteps decode in
    // the audio thread entirely — decode happens once on message thread
    // at clip-add, audio thread just reads from already-decoded memory.
    //
    // 100 MB ≈ 9.5 min stereo @ 44.1k — covers virtually every drum loop,
    // vocal phrase, and music sample users will drop on the timeline.
    // Files bigger than this fall back to the streaming path (WAVs for
    // long-form audio decode trivially fast even via streaming).
    static constexpr juce::int64 kRamThresholdBytes = 100 * 1024 * 1024;

    // Takes ownership of reader. Immediately starts background pre-fetching.
    AudioClipStreamer (std::unique_ptr<juce::AudioFormatReader> reader,
                      juce::TimeSliceThread& bgThread);
    ~AudioClipStreamer() override;

    // Seek to filePos and synchronously pre-fill kPrefillSeconds of audio.
    // Call from the message thread before playback begins (or on transport seek).
    void seek (int64 filePos);

    // ── Audio-thread read ─────────────────────────────────────────────────────
    // Reads numOutputSamples of output audio, linearly interpolated from
    // file position fileStartPos using readRatio (fileSamples per outputSample).
    // Mixes into dest[0..numDestCh-1][destOffset..+numOutputSamples] with gain.
    // Returns peak magnitude (post-gain) for metering.
    // Returns 0 (silence) if the ring is not ready or a seek is in progress.
    float readAndMix (juce::AudioBuffer<float>& dest,
                      int    destOffset,
                      int    numOutputSamples,
                      int64  fileStartPos,
                      double readRatio,
                      int    numDestChannels,
                      float  gain);

    // Audio-thread: read numSamples raw file samples (no interpolation, no mixing)
    // from file position filePos into dest[0..numDestCh-1][destOffset..].
    // Returns true if the data was in the ring; false on underrun or seek.
    bool readRaw (juce::AudioBuffer<float>& dest,
                  int destOffset,
                  int numSamples,
                  int64 filePos);

    int64  getTotalLength()    const { return mTotalLength; }
    double getFileSampleRate() const { return mFileSampleRate; }
    int    getNumChannels()    const { return mNumChannels; }
    bool   isRamLoaded()       const { return mRamMode; }   // G-7 polish

    // juce::TimeSliceClient — called by background thread
    int useTimeSlice() override;

private:
    // ── File reader ────────────────────────────────────────────────────────
    // Accessed only under mReaderLock (message thread + background thread).
    juce::CriticalSection                    mReaderLock;
    std::unique_ptr<juce::AudioFormatReader> mReader;
    int64  mTotalLength   { 0 };
    double mFileSampleRate{ 44100.0 };
    int    mNumChannels   { 2 };

    juce::TimeSliceThread& mBgThread;

    // ── SPSC ring buffer ───────────────────────────────────────────────────
    // Ring covers absolute file positions [mRingReadHead, mRingWriteHead).
    // Physical index for any file position fp: (int)(fp % mCapacity).
    // Valid because: as the ring advances, fp % mCapacity naturally cycles.
    int                      mCapacity { 0 };
    juce::AudioBuffer<float> mRing;

    // Written by audio thread (read by background to know safe-to-overwrite zone)
    std::atomic<int64> mRingReadHead  { 0 };
    // Written by background thread (read by audio thread to check availability)
    std::atomic<int64> mRingWriteHead { 0 };

    // true once ring has valid data at mRingReadHead
    std::atomic<bool>  mReady { false };

    // ── Async seek request (audio → background) ───────────────────────────
    std::atomic<bool>  mSeekPending { false };
    std::atomic<int64> mSeekTarget  { 0 };

    // G-7 polish (2026-04-29): true when the entire file fit in RAM at ctor;
    // skips background pre-fetch + skips seek-trigger checks since data is
    // always covered.
    bool               mRamMode     { false };

    // ── Internal helpers ───────────────────────────────────────────────────
    // Fill numSamples from file position fromFilePos into the ring.
    // Caller must hold mReaderLock. Advances mRingWriteHead.
    void fillIntoRing (int64 fromFilePos, int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioClipStreamer)
};
