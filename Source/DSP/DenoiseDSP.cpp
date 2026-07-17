#include "DenoiseDSP.h"

// ── DenoiseProfile serialization ─────────────────────────────────────────────

juce::String DenoiseProfile::toBase64() const
{
    if (! isValid()) return {};
    juce::MemoryOutputStream mo;
    mo.writeInt (1);                          // version
    mo.writeDouble (sampleRate);
    mo.writeInt (kBins);
    mo.write (mag.data(), sizeof (float) * (size_t) kBins);
    return juce::Base64::toBase64 (mo.getData(), mo.getDataSize());
}

DenoiseProfile DenoiseProfile::fromBase64 (const juce::String& s)
{
    DenoiseProfile p;
    juce::MemoryOutputStream raw;
    if (! juce::Base64::convertFromBase64 (raw, s)) return p;
    juce::MemoryInputStream mi (raw.getData(), raw.getDataSize(), false);
    if (mi.readInt() != 1) return p;
    const double sr = mi.readDouble();
    const int    n  = mi.readInt();
    if (n != kBins || sr <= 0.0) return p;
    p.mag.resize ((size_t) kBins);
    if (mi.read (p.mag.data(), (int) (sizeof (float) * (size_t) kBins))
        != (int) (sizeof (float) * (size_t) kBins))
        { p.mag.clear(); return p; }
    p.sampleRate = sr;
    return p;
}

// ── DenoiseLearner ───────────────────────────────────────────────────────────

void DenoiseLearner::prepare (double sampleRate)
{
    mSampleRate = sampleRate;
    mRing.assign ((size_t) juce::jmax (1, (int) (sampleRate * 4.0)), 0.0f);
    mFFT = std::make_unique<juce::dsp::FFT> (10);   // 2^10 == DenoiseProfile::kFFT
    mWindow.resize ((size_t) DenoiseProfile::kFFT);
    for (int i = 0; i < DenoiseProfile::kFFT; ++i)
        mWindow[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                      * (float) i / (float) DenoiseProfile::kFFT);
    mFrame.resize ((size_t) DenoiseProfile::kFFT);
    mFftData.resize ((size_t) DenoiseProfile::kFFT * 2);
    reset();
}

void DenoiseLearner::reset() noexcept
{
    const juce::ScopedLock sl (mFloorLock);
    mFloor.assign ((size_t) DenoiseProfile::kBins, 0.0f);
    mFrames.store (0, std::memory_order_release);
    mRead = mWrite.load (std::memory_order_acquire);
}

void DenoiseLearner::pushBlock (const float* mono, int n) noexcept
{
    if (mono == nullptr || n <= 0 || mRing.empty()) return;
    const int size = (int) mRing.size();
    juce::int64 w = mWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
        mRing[(size_t) ((w + i) % size)] = mono[i];
    mWrite.store (w + n, std::memory_order_release);
}

void DenoiseLearner::processPending()
{
    if (mFFT == nullptr) return;
    const int size = (int) mRing.size();
    const juce::int64 w = mWrite.load (std::memory_order_acquire);
    if (w - mRead > (juce::int64) size)          // overwritten -> resync
        mRead = w - size + DenoiseProfile::kFFT;

    while (w - mRead >= (juce::int64) DenoiseProfile::kFFT)
    {
        for (int i = 0; i < DenoiseProfile::kFFT; ++i)
            mFrame[(size_t) i] = mRing[(size_t) ((mRead + i) % size)];
        mRead += kHop;

        double e = 0.0;
        for (int i = 0; i < DenoiseProfile::kFFT; ++i) e += mFrame[(size_t) i] * mFrame[(size_t) i];
        const float rms = (float) std::sqrt (e / DenoiseProfile::kFFT);
        if (rms < 1.0e-5f) continue;             // unrouted/silent strip: never learn digital zero

        for (int i = 0; i < DenoiseProfile::kFFT; ++i)
            mFftData[(size_t) i] = mFrame[(size_t) i] * mWindow[(size_t) i];
        std::fill (mFftData.begin() + DenoiseProfile::kFFT, mFftData.end(), 0.0f);
        mFFT->performRealOnlyForwardTransform (mFftData.data());

        const juce::ScopedLock sl (mFloorLock);
        const int learned = mFrames.load (std::memory_order_relaxed);

        // Voice gate: once warm, skip frames far above the tracked floor so
        // singing/speech never inflates the room fingerprint.
        if (learned > 20)
        {
            double fe = 0.0, be = 0.0;
            for (int k = 0; k < DenoiseProfile::kBins; ++k)
            {
                const float re = mFftData[(size_t) (k * 2)];
                const float im = mFftData[(size_t) (k * 2 + 1)];
                fe += std::sqrt (re * re + im * im);
                be += mFloor[(size_t) k];
            }
            if (be > 1.0e-9 && fe > 4.0 * be) continue;    // > +12 dB over floor = voice
        }

        for (int k = 0; k < DenoiseProfile::kBins; ++k)
        {
            const float re = mFftData[(size_t) (k * 2)];
            const float im = mFftData[(size_t) (k * 2 + 1)];
            const float m  = std::sqrt (re * re + im * im);
            float& fl = mFloor[(size_t) k];
            if (learned < 20)       fl = (learned == 0) ? m : juce::jmin (fl, m);
            else if (m < fl)        fl += 0.30f * (m - fl);   // fast down: chase minima
            else                    fl += 0.004f * (m - fl);  // slow up: track drifting rooms
        }
        mFrames.store (learned + 1, std::memory_order_release);
    }
}

DenoiseProfile DenoiseLearner::snapshot() const
{
    DenoiseProfile p;
    const juce::ScopedLock sl (mFloorLock);
    if (mFrames.load (std::memory_order_acquire) < 40) return p;   // < ~0.25 s learned: immature
    p.sampleRate = mSampleRate;
    p.mag = mFloor;
    return p;
}

// ── Offline cleaner ──────────────────────────────────────────────────────────

namespace
{
    std::unique_ptr<juce::AudioFormatReader> openWav (const juce::File& f)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        return std::unique_ptr<juce::AudioFormatReader> (fm.createReaderFor (f));
    }

    // Profile bins live on the profile's own sample-rate grid; remap by
    // frequency when the file rate differs (regenerate after an SR change).
    float profileAt (const DenoiseProfile& p, double freqHz)
    {
        const double bin = freqHz / (p.sampleRate / (double) DenoiseProfile::kFFT);
        const int    b0  = juce::jlimit (0, DenoiseProfile::kBins - 1, (int) bin);
        const int    b1  = juce::jmin (DenoiseProfile::kBins - 1, b0 + 1);
        const float  fr  = (float) (bin - (double) b0);
        return p.mag[(size_t) b0] * (1.0f - fr) + p.mag[(size_t) b1] * fr;
    }
}

DenoiseProfile Denoise::learnFromFile (const juce::File& in)
{
    DenoiseProfile p;
    auto reader = openWav (in);
    if (reader == nullptr || reader->lengthInSamples <= 0) return p;

    DenoiseLearner learner;
    learner.prepare (reader->sampleRate);

    const int block = 32768;
    juce::AudioBuffer<float> buf ((int) reader->numChannels, block);
    std::vector<float> mono ((size_t) block);
    for (juce::int64 pos = 0; pos < reader->lengthInSamples; pos += block)
    {
        const int n = (int) juce::jmin ((juce::int64) block, reader->lengthInSamples - pos);
        reader->read (&buf, 0, n, pos, true, true);
        for (int i = 0; i < n; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < (int) reader->numChannels; ++ch)
                s += buf.getSample (ch, i);
            mono[(size_t) i] = s / (float) reader->numChannels;
        }
        learner.pushBlock (mono.data(), n);
        learner.processPending();
    }
    return learner.snapshot();
}

bool Denoise::cleanFile (const juce::File& in, const juce::File& out,
                         const DenoiseProfile& profile, Strength strength,
                         juce::String& errorOut)
{
    errorOut.clear();
    if (! profile.isValid()) { errorOut = "No noise profile available."; return false; }
    auto reader = openWav (in);
    if (reader == nullptr)   { errorOut = "Could not read " + in.getFileName(); return false; }

    const int    numCh = (int) reader->numChannels;
    const juce::int64 len = reader->lengthInSamples;
    const double sr = reader->sampleRate;
    if (len <= 0 || numCh <= 0) { errorOut = "Empty source file."; return false; }

    // Calibration (ear-validated on the QA-Fe WORLD arc): Strong == the
    // "cleantake" prototype (2x over-subtraction, gain floor -12 dB);
    // Light == half-depth for takes that only need a trim.
    const float overSub   = (strength == Strong) ? 2.00f : 1.25f;
    const float gainFloor = (strength == Strong) ? 0.25f : 0.50f;

    juce::AudioBuffer<float> src (numCh, (int) len);
    reader->read (&src, 0, (int) len, 0, true, true);

    constexpr int N = DenoiseProfile::kFFT, hop = DenoiseProfile::kFFT / 4, bins = DenoiseProfile::kBins;
    juce::dsp::FFT fft (10);
    std::vector<float> window ((size_t) N);
    for (int i = 0; i < N; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N);

    // Profile magnitudes were measured with the same window/FFT the STFT
    // below uses, so they subtract 1:1 on this grid (after SR remap).
    std::vector<float> prof ((size_t) bins);
    for (int k = 0; k < bins; ++k)
        prof[(size_t) k] = profileAt (profile, (double) k * sr / (double) N);

    juce::AudioBuffer<float> dst (numCh, (int) len);
    dst.clear();
    std::vector<float> wsum ((size_t) len, 0.0f);
    std::vector<float> fftData ((size_t) N * 2);
    std::vector<float> gain ((size_t) bins, 1.0f), gPrev ((size_t) bins, 1.0f), gSm ((size_t) bins);

    for (juce::int64 start = 0; start < len; start += hop)
    {
        // Gains from the mono fold; applied identically to every channel so
        // stereo takes keep their image.
        for (int i = 0; i < N; ++i)
        {
            const juce::int64 idx = start + i;
            float s = 0.0f;
            if (idx < len)
                for (int ch = 0; ch < numCh; ++ch) s += src.getSample (ch, (int) idx);
            fftData[(size_t) i] = (s / (float) numCh) * window[(size_t) i];
        }
        std::fill (fftData.begin() + N, fftData.end(), 0.0f);
        fft.performRealOnlyForwardTransform (fftData.data());

        for (int k = 0; k < bins; ++k)
        {
            const float re = fftData[(size_t) (k * 2)];
            const float im = fftData[(size_t) (k * 2 + 1)];
            const float m  = std::sqrt (re * re + im * im);
            gain[(size_t) k] = (m > 1.0e-12f)
                ? juce::jmax (gainFloor, 1.0f - overSub * prof[(size_t) k] / m)
                : gainFloor;
        }
        // Musical-noise ("birdies") suppression: 3-bin frequency smoothing +
        // one-pole time smoothing of the gain field.
        gSm[0] = gain[0];
        for (int k = 1; k < bins - 1; ++k)
            gSm[(size_t) k] = (gain[(size_t) (k - 1)] + gain[(size_t) k] + gain[(size_t) (k + 1)]) / 3.0f;
        gSm[(size_t) (bins - 1)] = gain[(size_t) (bins - 1)];
        for (int k = 0; k < bins; ++k)
        {
            gPrev[(size_t) k] += 0.5f * (gSm[(size_t) k] - gPrev[(size_t) k]);
            const float g = gPrev[(size_t) k];
            fftData[(size_t) (k * 2)]     *= g;
            if (k > 0 && k < bins - 1) fftData[(size_t) (k * 2 + 1)] *= g;
        }

        fft.performRealOnlyInverseTransform (fftData.data());

        for (int ch = 0; ch < numCh; ++ch)
        {
            // Per-channel resynthesis reuses the mono gain field: filter each
            // channel's frame with it via a second FFT round-trip only when
            // stereo; the common mono take path skips straight to OLA.
            if (numCh == 1)
            {
                for (int i = 0; i < N; ++i)
                {
                    const juce::int64 idx = start + i;
                    if (idx < len)
                        dst.addSample (0, (int) idx, fftData[(size_t) i] * window[(size_t) i]);
                }
            }
        }
        if (numCh > 1)
        {
            std::vector<float> chData ((size_t) N * 2);
            for (int ch = 0; ch < numCh; ++ch)
            {
                for (int i = 0; i < N; ++i)
                {
                    const juce::int64 idx = start + i;
                    chData[(size_t) i] = (idx < len) ? src.getSample (ch, (int) idx) * window[(size_t) i] : 0.0f;
                }
                std::fill (chData.begin() + N, chData.end(), 0.0f);
                fft.performRealOnlyForwardTransform (chData.data());
                for (int k = 0; k < bins; ++k)
                {
                    const float g = gPrev[(size_t) k];
                    chData[(size_t) (k * 2)] *= g;
                    if (k > 0 && k < bins - 1) chData[(size_t) (k * 2 + 1)] *= g;
                }
                fft.performRealOnlyInverseTransform (chData.data());
                for (int i = 0; i < N; ++i)
                {
                    const juce::int64 idx = start + i;
                    if (idx < len)
                        dst.addSample (ch, (int) idx, chData[(size_t) i] * window[(size_t) i]);
                }
            }
        }
        for (int i = 0; i < N; ++i)
        {
            const juce::int64 idx = start + i;
            if (idx < len)
                wsum[(size_t) idx] += window[(size_t) i] * window[(size_t) i];
        }
    }

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* d = dst.getWritePointer (ch);
        for (juce::int64 i = 0; i < len; ++i)
            d[i] = (wsum[(size_t) i] > 1.0e-6f) ? d[i] / wsum[(size_t) i] : src.getSample (ch, (int) i);
    }

    const int bits = (reader->usesFloatingPointData || reader->bitsPerSample > 24)
                       ? 32 : juce::jmax (16, (int) reader->bitsPerSample);
    out.deleteFile();
    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::FileOutputStream> os (out.createOutputStream());
    if (os == nullptr) { errorOut = "Could not write " + out.getFileName(); return false; }
    std::unique_ptr<juce::AudioFormatWriter> writer (
        fmt.createWriterFor (os.get(), sr, (unsigned int) numCh, bits, {}, 0));
    if (writer == nullptr) { errorOut = "Could not create WAV writer."; return false; }
    os.release();   // writer owns the stream now
    if (! writer->writeFromAudioSampleBuffer (dst, 0, (int) len))
        { writer.reset(); out.deleteFile(); errorOut = "WAV write failed."; return false; }
    writer.reset();
    return true;
}
