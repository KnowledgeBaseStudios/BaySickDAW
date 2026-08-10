#include "AudioFileRecorder.h"

AudioFileRecorder::AudioFileRecorder() = default;

AudioFileRecorder::~AudioFileRecorder()
{
    stopRecording();
}

bool AudioFileRecorder::startRecording(const juce::File& outputFile,
                                        double sampleRate,
                                        int numChannels,
                                        int skipInitialSamples)
{
    if (mRecording.load()) return false;

    mNumChannels        = numChannels;
    mOutputFile         = outputFile;
    mSamplesSinceStart  = 0;
    mSkipRemaining      = juce::jmax (0, skipInitialSamples);
    mDroppedBlocks.store (0, std::memory_order_relaxed);
    mFadeSamples        = juce::jmax (1, (int) std::round (sampleRate * 0.005));  // 5 ms
    mFadedBuf.setSize (numChannels, kMaxExpectedBlock, false, true, false);

    outputFile.getParentDirectory().createDirectory();

    auto stream = outputFile.createOutputStream();
    if (! stream) return false;

    juce::WavAudioFormat wavFormat;
    // ThreadedWriter takes ownership of the underlying writer + stream.
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wavFormat.createWriterFor (stream.release(),
                                    sampleRate,
                                    (unsigned int) numChannels,
                                    32,      // 32-bit float
                                    {},
                                    0));
    if (! writer) return false;

    // One TimeSliceThread per recorder instance, owned by it and torn down in
    // stopRecording; a multi-strip take arms several at once (dry + wet per Vox
    // strip, plus the master and capture taps).  Priority::normal is the message
    // thread's own band, so a heavy repaint can delay disk writes -- the
    // ThreadedWriter buffer below is the headroom that absorbs that.
    mWriterThread = std::make_unique<juce::TimeSliceThread> ("AudioFileRecorderBG");
    mWriterThread->startThread (juce::Thread::Priority::normal);

    // 10 seconds of headroom.  One OS stall up to 10 s no longer drops
    // samples; instead ThreadedWriter catches up when the stall clears.  A
    // longer stall still costs whole blocks -- writeBlock counts those.
    const int bufferSize = (int) (sampleRate * (double) kBufferSeconds);
    mThreadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (
        writer.release(), *mWriterThread, bufferSize);

    mRecording.store (true);
    return true;
}

void AudioFileRecorder::writeBlock (const juce::AudioBuffer<float>& buffer)
{
    if (! mRecording.load() || mThreadedWriter == nullptr) return;

    const int numSamples = buffer.getNumSamples();
    const int numCh      = juce::jmin (buffer.getNumChannels(), mNumChannels);

    // QA-Fe2 PDC: consume the leading-trim countdown before anything is
    // written, so the fade-in below still ramps the first WRITTEN samples.
    int offset = 0;
    if (mSkipRemaining > 0)
    {
        const int skip = juce::jmin (mSkipRemaining, numSamples);
        mSkipRemaining -= skip;
        if (skip == numSamples) return;
        offset = skip;
    }
    const int writeCount = numSamples - offset;

    // Fade-in ramp over the first mFadeSamples.  Once past the ramp we can
    // hand the original buffer straight to ThreadedWriter (zero-copy).
    if (mSamplesSinceStart < mFadeSamples || offset > 0)
    {
        // Defensive only -- startRecording pre-sized the scratch, so this
        // fires (and heap-allocates on the audio thread) only if the device
        // hands over a block larger than kMaxExpectedBlock.  Correctness of
        // the take beats glitch-free monitoring in that pathological case.
        if (mFadedBuf.getNumChannels() < numCh || mFadedBuf.getNumSamples() < writeCount)
            mFadedBuf.setSize (numCh, writeCount, false, false, true);

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* src = buffer.getReadPointer (ch) + offset;
            float*       dst = mFadedBuf.getWritePointer (ch);
            for (int s = 0; s < writeCount; ++s)
            {
                const int absSample = mSamplesSinceStart + s;
                const float g = absSample < mFadeSamples
                                  ? (float) absSample / (float) mFadeSamples
                                  : 1.0f;
                dst[s] = src[s] * g;
            }
        }

        const float* const* chPtrs = mFadedBuf.getArrayOfReadPointers();
        noteWriteResult (mThreadedWriter->write (chPtrs, writeCount));
    }
    else
    {
        const float* const* chPtrs = buffer.getArrayOfReadPointers();
        noteWriteResult (mThreadedWriter->write (chPtrs, writeCount));
    }

    // Advanced even on a dropped block: the ramp is a position in TIME since
    // the take started, not a count of samples that reached disk.
    mSamplesSinceStart += writeCount;
}

juce::File AudioFileRecorder::stopRecording()
{
    if (! mRecording.load()) return {};
    mRecording.store (false);

    // Fade-OUT intentionally NOT applied here.  Doing it correctly would need
    // a delay-line that holds the last mFadeSamples of audio until stop, so
    // we can apply a linear ramp to them before writing.  That's a non-trivial
    // audio-thread data structure and overkill for recordings - the real
    // end-click risk is on playback, not on the file itself (an idle file
    // doesn't click; only a DAW reading it + stopping abruptly does).  F3
    // handles it on the playback side instead (clip-edge declick).
    //
    // Destroy the writer first (flushes + closes the file), then the thread.
    mThreadedWriter.reset();
    if (mWriterThread)
    {
        mWriterThread->stopThread (4000);
        mWriterThread.reset();
    }

    return mOutputFile;
}
