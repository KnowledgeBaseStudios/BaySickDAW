#include "AudioFileRecorder.h"

AudioFileRecorder::AudioFileRecorder() = default;

AudioFileRecorder::~AudioFileRecorder()
{
    stopRecording();
}

bool AudioFileRecorder::startRecording(const juce::File& outputFile,
                                        double sampleRate,
                                        int numChannels)
{
    if (mRecording.load()) return false;

    mNumChannels        = numChannels;
    mCurrentSampleRate  = sampleRate;
    mOutputFile         = outputFile;
    mSamplesSinceStart  = 0;
    mFadeSamples        = juce::jmax (1, (int) std::round (sampleRate * 0.005));  // 5 ms

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

    // Background thread shared by all recorders (one per instance is fine;
    // each capture is short-lived).  Lower priority than audio; higher than
    // normal UI so disk writes keep up during heavy GUI redraws.
    mWriterThread = std::make_unique<juce::TimeSliceThread> ("AudioFileRecorderBG");
    mWriterThread->startThread (juce::Thread::Priority::normal);

    // 10 seconds of headroom.  One OS stall up to 10 s no longer drops
    // samples; instead ThreadedWriter catches up when the stall clears.
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

    // Fade-in ramp over the first mFadeSamples.  Once past the ramp we can
    // hand the original buffer straight to ThreadedWriter (zero-copy).
    if (mSamplesSinceStart < mFadeSamples)
    {
        if (mFadedBuf.getNumChannels() < numCh || mFadedBuf.getNumSamples() < numSamples)
            mFadedBuf.setSize (numCh, numSamples, false, false, true);

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* src = buffer.getReadPointer (ch);
            float*       dst = mFadedBuf.getWritePointer (ch);
            for (int s = 0; s < numSamples; ++s)
            {
                const int absSample = mSamplesSinceStart + s;
                const float g = absSample < mFadeSamples
                                  ? (float) absSample / (float) mFadeSamples
                                  : 1.0f;
                dst[s] = src[s] * g;
            }
        }

        const float* const* chPtrs = mFadedBuf.getArrayOfReadPointers();
        mThreadedWriter->write (chPtrs, numSamples);
    }
    else
    {
        const float* const* chPtrs = buffer.getArrayOfReadPointers();
        mThreadedWriter->write (chPtrs, numSamples);
    }

    mSamplesSinceStart += numSamples;
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
