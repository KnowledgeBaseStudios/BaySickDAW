#include "SlideSampleCache.h"
#include "SafeAudioReader.h"   // channel/frame sanity gate (QA-Cleanup)
#include "SafeAudioFormats.h"   // MP3 decode via vendored LAME (QA-Cleanup)

SlideSampleCache::SlideSampleCache()
{
    SafeAudioFormats::registerAll (mFormats);
}

std::shared_ptr<DecodedSlideSample> SlideSampleCache::getHandle (const juce::File& wav)
{
    const std::string key = wav.getFullPathName().toStdString();
    const juce::ScopedLock sl (mLock);

    auto it = mMap.find (key);
    if (it != mMap.end())
        if (auto live = it->second.lock())
            return live;

    auto fresh = std::make_shared<DecodedSlideSample>();
    mMap[key] = fresh;      // weak -- freed when the last SlideSampler drops it
    return fresh;
}

void SlideSampleCache::decodeNow (const std::shared_ptr<DecodedSlideSample>& handle,
                                  const juce::File& wav)
{
    if (handle == nullptr || handle->ready.load (std::memory_order_acquire))
        return;             // already decoded (shared from another tab)

    auto reader = SafeAudioReader::guard (
        std::unique_ptr<juce::AudioFormatReader> (mFormats.createReaderFor (wav)));
    if (reader != nullptr && reader->lengthInSamples > 0)
    {
        // SECURITY (QA-Cleanup 2026-08-10): lengthInSamples comes from the
        // file's own header and JUCE does not clamp it against the real file
        // size, so a truncating (int) cast could produce a wild allocation
        // request from a tiny file.  Bound it before it is narrowed.
        constexpr juce::int64 kMaxSamples = 192000LL * 60LL;   // 60 s at 192 kHz
        const int n  = (int) juce::jmin (reader->lengthInSamples, kMaxSamples);
        const int nc = juce::jlimit (1, 2, (int) reader->numChannels);

        handle->numFrames  = n;
        handle->sourceRate = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
        handle->buffer.setSize (1, n + DecodedSlideSample::kTailPad, false, true, false);
        handle->buffer.clear();

        juce::AudioBuffer<float> tmp (nc, n);
        reader->read (&tmp, 0, n, 0, true, nc > 1);

        // Mono fold: these karoryfer sustain samples are mono; a stereo file
        // averages down so the slide voice stays a single centered string.
        handle->buffer.copyFrom (0, 0, tmp, 0, 0, n);
        if (nc > 1)
        {
            handle->buffer.addFrom  (0, 0, tmp, 1, 0, n);
            handle->buffer.applyGain (0, 0, n, 0.5f);
        }
    }

    handle->ready.store (true, std::memory_order_release);
}
