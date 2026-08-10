#pragma once
#include <JuceHeader.h>

// ── AudioFileRecorder ─────────────────────────────────────────────────────────
// Records audio to a 32-bit float WAV file using juce::AudioFormatWriter::
// ThreadedWriter, which is JUCE's built-in lock-free audio-thread -> disk
// bridge.  Audio thread calls writeBlock(); a background thread drains the
// internal buffer to disk.  This is the pattern every pro DAW uses - it
// replaces the earlier hand-rolled SPSC FIFO + thread implementation which
// silently dropped samples on OS disk stalls (antivirus, cache flush,
// indexer), causing random mid-file pops in long recordings.  A stall longer
// than the ring's headroom still costs whole blocks, so the drop is COUNTED
// (getDroppedBlockCount) rather than swallowed - a take with a splice in it
// must never be handed back looking complete.
//
// Also applies a short linear fade-in at start to clamp the first few
// milliseconds of the WAV to zero.  Prevents the vertical-edge click you hear
// when a recording starts on a nonzero sample (almost always the case when
// transport is already running).  There is deliberately NO fade-out -- see
// stopRecording.
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
    // skipInitialSamples drops that many leading samples before anything is
    // written -- QA-Fe2 PDC: the master capture tap sits after the compensated
    // graph, so its content arrives totalLatencySamples late vs the transport
    // grid; the skip re-aligns the WAV.  Strip recorders tap pre-chain raw
    // input and pass 0.
    bool startRecording(const juce::File& outputFile,
                        double sampleRate,
                        int numChannels,
                        int skipInitialSamples = 0);

    bool isRecording() const { return mRecording.load(); }

    // Message thread, valid from startRecording until the next one: how many
    // blocks the disk writer refused.  ThreadedWriter::write() rejects the
    // WHOLE block when its ring is short of room, so a refusal is audio lost,
    // not audio deferred -- the take has a splice in it and the caller has to
    // say so rather than hand back a file that looks complete.
    int getDroppedBlockCount() const noexcept
    {
        return mDroppedBlocks.load (std::memory_order_relaxed);
    }

    // Audio thread: hand the buffer to ThreadedWriter (non-blocking).
    // Applies fade-in ramp on the first mFadeSamples after start.
    void writeBlock(const juce::AudioBuffer<float>& buffer);

    // Message thread: close writer + return file path.  No fade-out is applied
    // -- see stopRecording in the .cpp: end-click declick lives on the playback
    // side (F3 clip-edge), not in the file.
    juce::File stopRecording();

private:
    // Audio thread.  ThreadedWriter::write() asks its FIFO for room for the
    // WHOLE block and returns false without copying anything when it is short
    // (juce_AudioFormatWriter.cpp), so a false is lost audio, never a partial
    // or deferred write.  Allocation-free by construction - one relaxed add.
    void noteWriteResult (bool wroteOk) noexcept
    {
        if (! wroteOk)
            mDroppedBlocks.fetch_add (1, std::memory_order_relaxed);
    }

    // 5 ms fade at 44.1k = 221 samples.  Recomputed per-session from the
    // actual sample rate so the fade stays 5 ms regardless of device rate.
    int    mFadeSamples { 0 };
    int    mSamplesSinceStart { 0 };      // audio thread only; ramp-in counter
    int    mSkipRemaining { 0 };          // audio thread only; PDC leading-trim countdown
    int    mNumChannels { 2 };

    // JUCE's lock-free writer (wraps a background thread + ring buffer).
    // 10 seconds of stereo float = 10 * 44100 * 2 = 882000 samples total;
    // bufferSize is samples-per-channel.
    static constexpr int kBufferSeconds = 10;
    std::unique_ptr<juce::TimeSliceThread>                 mWriterThread;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> mThreadedWriter;

    // Scratch buffer for fade-in application (avoids mutating caller's buffer).
    // Pre-sized in startRecording (message thread): recorders are constructed
    // fresh per take, so a lazy first-block grow would be a heap allocation
    // inside the audio callback on every Record press.  kMaxExpectedBlock =
    // 8192 sits comfortably above any device buffer size the audio settings
    // offer; writeBlock keeps a defensive resize for anything larger.
    static constexpr int kMaxExpectedBlock = 8192;
    juce::AudioBuffer<float>     mFadedBuf;

    std::atomic<bool>            mRecording { false };

    // Audio thread increments, message thread reads (usually after stop).
    // Relaxed on both ends: nothing else is published with it and the reader
    // only needs "was anything dropped", not an ordering against the audio.
    std::atomic<int>             mDroppedBlocks { 0 };

    juce::File                   mOutputFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFileRecorder)
};
