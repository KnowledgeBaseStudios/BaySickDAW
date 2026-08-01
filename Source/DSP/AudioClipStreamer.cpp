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

std::atomic<bool>        AudioClipStreamer::sOfflineRender { false };
std::atomic<juce::int64> AudioClipStreamer::sUnderrunCount { 0 };
std::atomic<float>       AudioClipStreamer::sPeakPrefillMs { 0.0f };

void AudioClipStreamer::noteMiss (int64 filePos) noexcept
{
    // CALIBRATION (Rule 6 cat. 5): one max-ish block of slack.  Consecutive
    // reads continue exactly where the last ended (readRaw) or within a
    // fraction of a sample of it (readAndMix's ratio math), so anything
    // inside this window is continuous playback and anything outside it is a
    // seek the user asked for.
    constexpr int64 kContiguitySlack = 4096;

    const bool contiguous = mLastServedEnd > 0
                         && std::llabs ((long long) (filePos - mLastServedEnd))
                              <= (long long) kContiguitySlack;

    if (contiguous || sOfflineRender.load (std::memory_order_relaxed))
    {
        sUnderrunCount.fetch_add (1, std::memory_order_relaxed);
        // The Debug-build tripwire the entry asks for: during CONTINUOUS
        // playback the ring should never be short, so this firing is a real
        // finding rather than noise.
        jassertfalse;
    }

    // Either way the next read starts a fresh run -- a seek's first block must
    // not be judged against a position from before the jump.
    mLastServedEnd = 0;
}

bool AudioClipStreamer::ensureRangeBlockingForOffline (int64 needStart)
{
    if (! sOfflineRender.load (std::memory_order_relaxed) || mRamMode)
        return false;

    needStart = juce::jmax ((int64) 0, needStart);
    if (needStart >= mTotalLength)
        return false;   // past EOF: normal silence, not an underrun

    // The seek() shape executed inline on the render thread: restart the
    // ring at the needed position and synchronously prefill.  Blocking here
    // is the point -- no live audio callback runs while the flag is set.
    juce::CriticalSection::ScopedLockType lk (mReaderLock);

    mSeekPending  .store (false,     std::memory_order_release);
    mRingReadHead .store (needStart, std::memory_order_release);
    mRingWriteHead.store (needStart, std::memory_order_release);

    const int prefill = (int) juce::jmin (
        (int64) (mFileSampleRate * kPrefillSeconds),
        mTotalLength - needStart);
    if (prefill > 0)
        fillIntoRing (needStart, prefill);

    mReady.store (true, std::memory_order_release);
    return mRingWriteHead.load (std::memory_order_acquire) > needStart;
}

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

void AudioClipStreamer::requestSeek (int64 filePos)
{
    const int64 clampedPos = juce::jlimit ((int64) 0, mTotalLength, filePos);

    if (mRamMode)
    {
        mRingReadHead.store (clampedPos, std::memory_order_release);
        return;
    }

    mSeekTarget .store (clampedPos, std::memory_order_release);
    mSeekPending.store (true,       std::memory_order_release);
    mReady      .store (false,      std::memory_order_release);
}

// ── fillIntoRing - internal, mReaderLock must be held ────────────────────────

void AudioClipStreamer::fillIntoRing (int64 fromFilePos, int numSamples)
{
    // CL-282: the fill IS the disk read, so its wall-clock is the number that
    // predicts an underrun -- a fill slower than the ring's remaining headroom
    // starves the next audio-thread read.  Session peak via CAS-max; runs on
    // whichever thread is filling (message seek / background prefetch /
    // offline blocking), never the audio thread.
    const double fillT0 = juce::Time::getMillisecondCounterHiRes();
    struct ScopedFillClock
    {
        double t0;
        ~ScopedFillClock()
        {
            const float ms = (float) (juce::Time::getMillisecondCounterHiRes() - t0);
            float cur = sPeakPrefillMs.load (std::memory_order_relaxed);
            while (ms > cur
                   && ! sPeakPrefillMs.compare_exchange_weak (cur, ms,
                                                              std::memory_order_relaxed))
            {}
        }
    } fillClock { fillT0 };

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
    if (numSamples <= 0)
        return false;

    if (! mReady.load (std::memory_order_acquire))
    {
        // CL-282: offline renders refill synchronously instead of returning
        // the silence a live callback would have to.
        if (! ensureRangeBlockingForOffline (filePos))
        {
            if (filePos >= 0 && filePos < mTotalLength)
                noteMiss (filePos);
            return false;
        }
    }

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
        int64 wh = mRingWriteHead.load (std::memory_order_acquire);

        // The ring physically covers [wh - capacity, wh).
        // We use coveredStart (not rh) as the lower bound because the stateless
        // filePos computation (outPosInClip * readRatio) can land 1-2 samples
        // behind rh due to integer rounding - using rh would trigger a phantom
        // seek every block even though the data is right there in the ring.
        int64 coveredStart = wh - (int64) mCapacity;
        if (filePos < coveredStart || filePos + numSamples > wh)
        {
            // CL-282: one synchronous refill attempt before conceding.
            if (ensureRangeBlockingForOffline (filePos))
            {
                wh           = mRingWriteHead.load (std::memory_order_acquire);
                coveredStart = wh - (int64) mCapacity;
            }
            if (filePos < coveredStart || filePos + numSamples > wh)
            {
                if (filePos >= 0 && filePos + numSamples <= mTotalLength)
                    noteMiss (filePos);
                mSeekTarget .store (filePos, std::memory_order_release);
                mSeekPending.store (true,    std::memory_order_release);
                mReady      .store (false,   std::memory_order_release);
                return false;
            }
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
    mLastServedEnd = filePos + numSamples;   // CL-282 contiguity anchor
    return true;
}

// ── readAndMix - audio thread ─────────────────────────────────────────────────

float AudioClipStreamer::readAndMix (juce::AudioBuffer<float>& dest,
                                     int    destOffset,
                                     int    numOutputSamples,
                                     double fileStartPos,
                                     double readRatio,
                                     int    numDestChannels,
                                     float  gain)
{
    if (numOutputSamples <= 0)
        return 0.0f;

    // G1 smoke round 5: at a true 1:1 rate the fractional offset is a
    // CONSTANT sub-sample delay (beat-domain start positions are fractional
    // even for unstretched clips), and interpolating for it traded bit-exact
    // playback for program-dependent interp error - an audible fizz floor on
    // dense material.  Snap to the nearest frame: a +-0.5-frame constant
    // timing shift (inaudible), the advance stays exactly 1 frame/sample so
    // consecutive blocks remain continuous, and the exact-copy fast path
    // takes over (zero interpolation error).
    if (readRatio == 1.0)
        fileStartPos = (double) (int64) std::llround (fileStartPos);

    // QA-Ec G1-boundary fix: the start position is fractional now (tempo-
    // follow / varispeed rates).  Integer window/seek math uses its floor;
    // +3 lookahead (was +2) covers the sub-sample start offset.
    const int64 startFloor = (int64) std::floor (fileStartPos);
    const int numFileSamples = (int) std::ceil ((double) numOutputSamples * readRatio) + 3;

    if (! mReady.load (std::memory_order_acquire))
    {
        // CL-282: offline renders refill synchronously (Catmull-Rom reads one
        // frame behind, hence -1) instead of returning a live callback's
        // forced silence.
        if (! ensureRangeBlockingForOffline (startFloor - 1))
        {
            if (startFloor >= 0 && startFloor < mTotalLength)
                noteMiss (startFloor);
            return 0.0f;
        }
    }

    // G-7 polish: in RAM mode the entire file is loaded, so skip the ring
    // availability + seek-trigger logic.  Reads past EOF still bail with
    // silence rather than triggering a phantom seek.
    if (mRamMode)
    {
        if (startFloor < 0 || startFloor >= mTotalLength)
            return 0.0f;
        // Per-sample bounds checks below already break out at EOF - fine.
    }
    else
    {
        int64 wh = mRingWriteHead.load (std::memory_order_acquire);

        // The ring physically covers [wh - capacity, wh).
        // Use coveredStart (not rh) as the lower bound - the stateless filePos
        // (outPosInClip * readRatio) lands 1-2 samples behind rh every block due
        // to the interpolation lookahead in numFileSamples.  Using rh caused a
        // phantom seek on every single block even though the data was present.
        int64 coveredStart = wh - (int64) mCapacity;
        // G1 smoke round 5: lower bound extended by 1 - the Catmull-Rom
        // kernel below reads one frame BEHIND the integer position.
        if (startFloor - 1 < coveredStart || startFloor + numFileSamples > wh)
        {
            // CL-282: one synchronous refill attempt before conceding.  Live
            // playback keeps the strict window test EXACTLY as before (the
            // refill helper no-ops when not offline).
            bool stillMissing = true;
            if (ensureRangeBlockingForOffline (startFloor - 1))
            {
                wh           = mRingWriteHead.load (std::memory_order_acquire);
                coveredStart = wh - (int64) mCapacity;
                // Post-refill (offline only): covered-to-EOF counts as
                // covered -- the refill spans [startFloor-1, EOF-or-prefill)
                // by construction (4 s ring vs one block's need), and the
                // per-sample EOF breaks below handle a clip tail shorter
                // than the interpolation lookahead.
                stillMissing = (startFloor - 1 < coveredStart)
                            || (startFloor + numFileSamples > wh && wh < mTotalLength);
            }
            if (stillMissing)
            {
                if (startFloor >= 0 && startFloor < mTotalLength)
                    noteMiss (startFloor);
                // Genuine position jump (backward scrub, loop, or first-block miss) -
                // signal the background thread to seek.
                mSeekTarget .store (startFloor, std::memory_order_release);
                mSeekPending.store (true,       std::memory_order_release);
                mReady      .store (false,      std::memory_order_release);
                return 0.0f;
            }
        }
    }

    float peak = 0.0f;

    if (readRatio == 1.0 && fileStartPos == (double) startFloor)
    {
        // ── Fast path: 1:1 copy from an integral start, no interpolation ──
        for (int ch = 0; ch < numDestChannels; ++ch)
        {
            const int srcCh = ch % mNumChannels;
            for (int i = 0; i < numOutputSamples; ++i)
            {
                const int64 fp = startFloor + i;
                if (fp >= mTotalLength) break;

                const float v = mRing.getSample (srcCh, (int) (fp % mCapacity)) * gain;
                dest.addSample (ch, destOffset + i, v);
                peak = juce::jmax (peak, std::abs (v));
            }
        }
    }
    else
    {
        // ── Interpolated path: 4-point Catmull-Rom for fractional rates ───
        // (varispeed / tempo-follow / SR conversion).  G1 smoke round 5:
        // linear interpolation's error tracks the program material (audible
        // fizz on dense mixes); Catmull-Rom drops that floor by orders of
        // magnitude for a few extra ops.  The fractional block-start carries
        // into exactFP, so consecutive blocks stay sub-sample continuous.
        // Edge frames clamp (file head / EOF) - window validity for ip-1 is
        // guaranteed by the -1 lower-bound check above.
        for (int i = 0; i < numOutputSamples; ++i)
        {
            const double exactFP = fileStartPos + (double) i * readRatio;
            const int64  ip      = (int64) exactFP;
            const float  frac    = (float) (exactFP - (double) ip);

            if (ip + 1 >= mTotalLength) break;

            const int64 im1 = juce::jmax ((int64) 0, ip - 1);
            const int64 ip2 = juce::jmin (ip + 2, mTotalLength - 1);

            for (int ch = 0; ch < numDestChannels; ++ch)
            {
                const int   srcCh = ch % mNumChannels;
                const float p0    = mRing.getSample (srcCh, (int) (im1      % mCapacity));
                const float p1    = mRing.getSample (srcCh, (int) (ip       % mCapacity));
                const float p2    = mRing.getSample (srcCh, (int) ((ip + 1) % mCapacity));
                const float p3    = mRing.getSample (srcCh, (int) (ip2      % mCapacity));
                const float v     = (p1 + 0.5f * frac * ((p2 - p0)
                                  + frac * (2.f * p0 - 5.f * p1 + 4.f * p2 - p3
                                  + frac * (3.f * (p1 - p2) + p3 - p0)))) * gain;
                dest.addSample (ch, destOffset + i, v);
                peak = juce::jmax (peak, std::abs (v));
            }
        }
    }

    // Advance the ring read head CONSERVATIVELY - only past samples the next
    // block can never need again (floor of the true consumption).  Advancing
    // by the full numFileSamples (which includes the interp LOOKAHEAD) marked
    // still-needed samples overwritable, letting the background thread race
    // the next block's read of them (QA-Ec G1-boundary fix).  Round 5: -2
    // retention margin so the Catmull kernel's one-frame-behind read (ip-1)
    // of the NEXT block is never marked overwritable either.
    mRingReadHead.store (juce::jmax ((int64) 0,
                             startFloor - 2
                             + (int64) std::floor ((double) numOutputSamples * readRatio)),
                         std::memory_order_release);

    // CL-282 contiguity anchor: where the NEXT block's read is expected to
    // start (ratio-scaled), so a continuation miss is distinguishable from a
    // user seek.  The slack in noteMiss absorbs the fractional rounding.
    mLastServedEnd = (int64) std::llround (fileStartPos
                                           + (double) numOutputSamples * readRatio);
    return peak;
}
