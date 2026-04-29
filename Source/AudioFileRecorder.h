#pragma once
#include <JuceHeader.h>

// ── AudioFileRecorder ─────────────────────────────────────────────────────────
// Records audio to a 32-bit float WAV file using juce::AudioFormatWriter::
// ThreadedWriter, which is JUCE's built-in lock-free audio-thread -> disk
// bridge.  Audio thread calls writeBlock(); a background thread drains the
// internal buffer to disk.  This is the pattern every pro DAW uses - it
// replaces the earlier hand-rolled SPSC FIFO + thread implementation which
// silently dropped samples on OS disk stalls (antivirus, cache flush,
// indexer), causing random mid-file pops in long recordings.
//
// Also applies a short linear fade-in at start + fade-out at stop to clamp
// the first / last few milliseconds of the WAV to zero.  Prevents the
// vertical-edge clicks you hear when a recording starts / ends on a nonzero
// sample (almost always the case when transport is already running).
//
// Usage:
//   startRecording(outputFile, sampleRate, numChannels)  - message thread
//   writeBlock(buffer)                                   - audio thread, every block
//   stopRecording() -> File                              - message thread
// ─────────────────────────────────────────────────────────────────────────────
class AudioFileRecorder
{
public:
    AudioFileRecorder();
    ~AudioFileRecorder();

    // Message thread: open file + spin up ThreadedWriter's background thread.
    bool startRecording(const juce::File& outputFile,
                        double sampleRate,
                        int numChannels);

    bool isRecording() const { return mRecording.load(); }

    // Audio thread: hand the buffer to ThreadedWriter (non-blocking).
    // Applies fade-in ramp on the first kFadeSamples after start.
    void writeBlock(const juce::AudioBuffer<float>& buffer);

    // Message thread: apply fade-out tail + close writer + return file path.
    juce::File stopRecording();

private:
    // 5 ms fade at 44.1k = 221 samples.  Recomputed per-session from the
    // actual sample rate so the fade stays 5 ms regardless of device rate.
    int    mFadeSamples { 0 };
    int    mSamplesSinceStart { 0 };      // audio thread only; ramp-in counter
    double mCurrentSampleRate { 44100.0 };
    int    mNumChannels { 2 };

    // JUCE's lock-free writer (wraps a background thread + ring buffer).
    // 10 seconds of stereo float = 10 * 44100 * 2 = 882000 samples total;
    // bufferSize is samples-per-channel.
    static constexpr int kBufferSeconds = 10;
    std::unique_ptr<juce::TimeSliceThread>                 mWriterThread;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> mThreadedWriter;

    // Scratch buffer for fade-in application (avoids mutating caller's buffer).
    juce::AudioBuffer<float>     mFadedBuf;

    std::atomic<bool>            mRecording { false };
    juce::File                   mOutputFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFileRecorder)
};
