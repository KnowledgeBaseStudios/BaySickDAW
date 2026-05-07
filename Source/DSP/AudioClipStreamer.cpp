#include "AudioClipStreamer.h"

// ── Constructor / Destructor ──────────────────────────────────────────────────

AudioClipStreamer::AudioClipStreamer (std::unique_ptr<juce::AudioFormatReader> reader,
                                     juce::TimeSliceThread& bgThread)
    : mBgThread (bgThread)
{
    jassert (reader != nullptr);
    mReader        = std::move (reader);
    mTotalLength   = mReader->lengthInSamples;
    mFileSampleRate= mReader->sampleRate;
    mNumChannels   = (int) juce::jmin ((juce::int64) 2,
                                       (juce::int64) mReader->numChannels);

    // G-7 polish (2026-04-29): try to RAM-load the entire file if its raw
    // float-PCM size is at or under kRamThresholdBytes.  Eliminates the
    // cold-start sputter on first playback for short clips.
    const juce::int64 totalBytes = (juce::int64) sizeof (float)
                                       * (juce::int64) mNumChannels
                                       * mTotalLength;
    if (mTotalLength > 0 && totalBytes > 0 && totalBytes <= kRamThresholdBytes)
    {
        mRamMode  = true;
        mCapacity = (int) mTotalLength;
        mRing.setSize (mNumChannels, mCapacity, false, true, false);

        {
            juce::CriticalSection::ScopedLockType lk (mReaderLock);
            mReader->read (&mRing, 0, mCapacity, 0, true, true);
        }

        mRingReadHead .store (0,            std::memory_order_release);
        mRingWriteHead.store (mTotalLength, std::memory_order_release);
        mReady        .store (true,         std::memory_order_release);
        // RAM mode: no background pre-fetch needed - data is already loaded.
        return;
    }

    // Streaming mode: 4-second ring with background pre-fetch.
    mCapacity = (int) (mFileSampleRate * kBufSeconds);
    mRing.setSize (mNumChannels, mCapacity, false, true, false);

    mBgThread.addTimeSliceClient (this);
}

AudioClipStreamer::~AudioClipStreamer()
{
    if (! mRamMode)
        mBgThread.removeTimeSliceClient (this);
}

// ── seek - message thread ─────────────────────────────────────────────────────

void AudioClipStreamer::seek (int64 filePos)
{
    // G-7 polish: in RAM mode the entire file is always covered, so seek is
    // a no-op aside from clamping the read head for any external observers.
    // Don't touch mRingWriteHead (must stay at mTotalLength) or mReady
    // (must stay true).
    if (mRamMode)
    {
        const int64 clampedPos = juce::jlimit ((int64) 0, mTotalLength, filePos);
        mRingReadHead.store (clampedPos, std::memory_order_release);
        return;
    }

    mReady.store (false, std::memory_order_release);

    const int64 clampedPos = juce::jlimit ((int64) 0, mTotalLength, filePos);

    juce::CriticalSection::ScopedLockType lk (mReaderLock);

    // Reset ring to new position
    mRingReadHead .store (clampedPos, std::memory_order_release);
    mRingWriteHead.store (clampedPos, std::memory_order_release);

    // Synchronously fill kPrefillSeconds so the audio thread has data immediately
    const int prefill = (int) juce::jmin (
        (int64) (mFileSampleRate * kPrefillSeconds),
        mTotalLength - clampedPos);

    if (prefill > 0)
        fillIntoRing (clampedPos, prefill);

    mReady.store (true, std::memory_order_release);
}

// ── fillIntoRing - internal, mReaderLock must be held ────────────────────────

void AudioClipStreamer::fillIntoRing (int64 fromFilePos, int numSamples)
{
    // Guard: don't overfill the ring
    const int64 rh    = mRingReadHead .load (std::memory_order_acquire);
    const int64 wh    = mRingWriteHead.load (std::memory_order_relaxed);
    const int   space = (int) (mCapacity - (wh - rh));
    numSamples = juce::jmin (numSamples, space);
    numSamples = (int) juce::jmin ((int64) numSamples, mTotalLength - fromFilePos);

    if (numSamples <= 0) return;

    juce::AudioBuffer<float> scratch (mNumChannels, kChunkSize);

    int filled = 0;
    while (filled < numSamples)
    {
        const int chunk      = juce::jmin (kChunkSize, numSamples - filled);
        const int64 fileAt   = fromFilePos + filled;

        scratch.clear();
        mReader->read (&scratch, 0, chunk, fileAt, true, true);

        // Write into ring - handle wrap-around
        const int64 currentWH = mRingWriteHead.load (std::memory_order_relaxed);
        const int physStart    = (int) (currentWH % mCapacity);
        const int part1        = juce::jmin (chunk, mCapacity - physStart);
        const int part2        = chunk - part1;

        for (int ch = 0; ch < mNumChannels; ++ch)
        {
            mRing.copyFrom (ch, physStart, scratch, ch, 0, part1);
            if (part2 > 0)
                mRing.copyFrom (ch, 0, scratch, ch, part1, part2);
        }

        mRingWriteHead.fetch_add (chunk, std::memory_order_release);
        filled += chunk;
    }
}

// ── useTimeSlice - background thread ─────────────────────────────────────────

int AudioClipStreamer::useTimeSlice()
{
    // Handle an async seek requested by the audio thread
    if (mSeekPending.load (std::memory_order_acquire))
    {
        const int64 target        = mSeekTarget.load (std::memory_order_acquire);
        const int64 clampedTarget = juce::jlimit ((int64) 0, mTotalLength, target);

        {
            juce::CriticalSection::ScopedLockType lk (mReaderLock);

            mRingReadHead .store (clampedTarget, std::memory_order_release);
            mRingWriteHead.store (clampedTarget, std::memory_order_release);

            const int prefill = (int) juce::jmin (
                (int64) (mFileSampleRate * kPrefillSeconds),
                mTotalLength - clampedTarget);

            if (prefill > 0)
                fillIntoRing (clampedTarget, prefill);
        }

        mSeekPending.store (false, std::memory_order_release);
        mReady      .store (true,  std::memory_order_release);
        return 0;   // come back immediately to continue filling
    }

    const int64 rh        = mRingReadHead .load (std::memory_order_acquire);
    const int64 wh        = mRingWriteHead.load (std::memory_order_relaxed);
    const int64 available = wh - rh;

    // At EOF - nothing more to write
    if (wh >= mTotalLength)
        return 50;

    // Buffer is sufficiently full (> 75%) - rest briefly
    if (available >= (int64) (mCapacity * 0.75))
        return 10;

    // Fill the next chunk
    const int chunk = (int) juce::jmin ((int64) kChunkSize, mTotalLength - wh);

    {
        juce::CriticalSection::ScopedLockType lk (mReaderLock);
        fillIntoRing (wh, chunk);
    }

    return 0;   // come back immediately if more is needed
}

// ── readRaw - audio thread ────────────────────────────────────────────────────

bool AudioClipStreamer::readRaw (juce::AudioBuffer<float>& dest,
                                  int destOffset,
                                  int numSamples,
                                  int64 filePos)
{
    if (! mReady.load (std::memory_order_acquire) || numSamples <= 0)
        return false;

    // G-7 polish: in RAM mode the entire file is loaded, so skip the ring
    // availability + seek-trigger logic.  Out-of-range reads return false
    // (silence) without resetting mReady (no bg thread to refill anyway).
    if (mRamMode)
    {
        if (filePos < 0 || filePos + numSamples > mTotalLength)
            return false;
    }
    else
    {
        const int64 wh = mRingWriteHead.load (std::memory_order_acquire);

        // The ring physically covers [wh - capacity, wh).
        // We use coveredStart (not rh) as the lower bound because the stateless
        // filePos computation (outPosInClip * readRatio) can land 1-2 samples
        // behind rh due to integer rounding - using rh would trigger a phantom
        // seek every block even though the data is right there in the ring.
        const int64 coveredStart = wh - (int64) mCapacity;
        if (filePos < coveredStart || filePos + numSamples > wh)
        {
            mSeekTarget .store (filePos, std::memory_order_release);
            mSeekPending.store (true,    std::memory_order_release);
            mReady      .store (false,   std::memory_order_release);
            return false;
        }
    }

    const int numDestCh = juce::jmin (dest.getNumChannels(), mNumChannels);
    for (int ch = 0; ch < numDestCh; ++ch)
    {
        float* dst = dest.getWritePointer (ch) + destOffset;
        for (int i = 0; i < numSamples; ++i)
            dst[i] = mRing.getSample (ch, (int) ((filePos + i) % mCapacity));
    }

    mRingReadHead.store (filePos + numSamples, std::memory_order_release);
    return true;
}

// ── readAndMix - audio thread ─────────────────────────────────────────────────

float AudioClipStreamer::readAndMix (juce::AudioBuffer<float>& dest,
                                     int    destOffset,
                                     int    numOutputSamples,
                                     int64  fileStartPos,
                                     double readRatio,
                                     int    numDestChannels,
                                     float  gain)
{
    if (! mReady.load (std::memory_order_acquire) || numOutputSamples <= 0)
        return 0.0f;

    // How many file samples we need (+ 2 for interpolation lookahead)
    const int numFileSamples = (int) std::ceil ((double) numOutputSamples * readRatio) + 2;

    // G-7 polish: in RAM mode the entire file is loaded, so skip the ring
    // availability + seek-trigger logic.  Reads past EOF still bail with
    // silence rather than triggering a phantom seek.
    if (mRamMode)
    {
        if (fileStartPos < 0 || fileStartPos >= mTotalLength)
            return 0.0f;
        // Per-sample bounds checks below already break out at EOF - fine.
    }
    else
    {
        const int64 wh = mRingWriteHead.load (std::memory_order_acquire);

        // The ring physically covers [wh - capacity, wh).
        // Use coveredStart (not rh) as the lower bound - the stateless filePos
        // (outPosInClip * readRatio) lands 1-2 samples behind rh every block due
        // to the +2 interpolation lookahead in numFileSamples.  Using rh caused a
        // phantom seek on every single block even though the data was present.
        const int64 coveredStart = wh - (int64) mCapacity;
        if (fileStartPos < coveredStart || fileStartPos + numFileSamples > wh)
        {
            // Genuine position jump (backward scrub, loop, or first-block miss) -
            // signal the background thread to seek.
            mSeekTarget .store (fileStartPos, std::memory_order_release);
            mSeekPending.store (true,         std::memory_order_release);
            mReady      .store (false,        std::memory_order_release);
            return 0.0f;
        }
    }

    float peak = 0.0f;

    if (readRatio == 1.0)
    {
        // ── Fast path: 1:1 copy, no interpolation ────────────────────────
        for (int ch = 0; ch < numDestChannels; ++ch)
        {
            const int srcCh = ch % mNumChannels;
            for (int i = 0; i < numOutputSamples; ++i)
            {
                const int64 fp = fileStartPos + i;
                if (fp >= mTotalLength) break;

                const float v = mRing.getSample (srcCh, (int) (fp % mCapacity)) * gain;
                dest.addSample (ch, destOffset + i, v);
                peak = juce::jmax (peak, std::abs (v));
            }
        }
    }
    else
    {
        // ── Interpolated path: linear interpolation for SR conversion ─────
        for (int i = 0; i < numOutputSamples; ++i)
        {
            const double exactFP = (double) fileStartPos + (double) i * readRatio;
            const int64  ip      = (int64) exactFP;
            const float  frac    = (float) (exactFP - (double) ip);

            if (ip + 1 >= mTotalLength) break;

            for (int ch = 0; ch < numDestChannels; ++ch)
            {
                const int   srcCh = ch % mNumChannels;
                const float s0    = mRing.getSample (srcCh, (int) (ip       % mCapacity));
                const float s1    = mRing.getSample (srcCh, (int) ((ip + 1) % mCapacity));
                const float v     = (s0 + frac * (s1 - s0)) * gain;
                dest.addSample (ch, destOffset + i, v);
                peak = juce::jmax (peak, std::abs (v));
            }
        }
    }

    // Advance the ring read head - these file samples are consumed;
    // the background thread can now overwrite that space.
    mRingReadHead.store (fileStartPos + numFileSamples, std::memory_order_release);

    return peak;
}
