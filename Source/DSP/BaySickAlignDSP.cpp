#include "BaySickAlignDSP.h"
#include "PhaseVocoder.h"
#include "PitchShifters.h"
#include <algorithm>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// BaySickAlignDSP - Phase H-6a (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    // ── Onset detection (spectral-flux-style) ───────────────────────────────
    // Splits the input into hop-aligned frames, runs an FFT on each Hann-
    // windowed frame, computes the spectral flux (sum of positive magnitude
    // increases across bins between consecutive frames), normalises the flux
    // envelope, then picks peaks above an adaptive threshold.
    //
    // Returns onset times in seconds.
    static constexpr int   kFFTOrder = 11;            // 2048
    static constexpr int   kFFTSize  = 1 << kFFTOrder;
    static constexpr int   kHopSize  = 512;
    static constexpr float kMinPeakDistanceSec = 0.040f;   // 40 ms refractory
    static constexpr float kPeakSDFactor       = 1.6f;     // adaptive threshold

    std::vector<double> detectOnsetsSec (const float* audio, int numSamples,
                                          double sampleRate)
    {
        std::vector<double> onsets;
        if (audio == nullptr || numSamples < kFFTSize) return onsets;

        juce::dsp::FFT fft (kFFTOrder);
        std::vector<float> window ((size_t) kFFTSize);
        for (int n = 0; n < kFFTSize; ++n)
            window[(size_t) n] = 0.5f * (1.0f - std::cos (2.0 * juce::MathConstants<double>::pi
                                                          * n / (double) (kFFTSize - 1)));

        const int numFrames = juce::jmax (0, (numSamples - kFFTSize) / kHopSize + 1);
        std::vector<float> flux ((size_t) numFrames, 0.0f);

        std::vector<float> prevMag ((size_t) (kFFTSize / 2 + 1), 0.0f);
        std::vector<float> curMag  ((size_t) (kFFTSize / 2 + 1), 0.0f);
        std::vector<float> fftBuf  ((size_t) (kFFTSize * 2),     0.0f);

        for (int frame = 0; frame < numFrames; ++frame)
        {
            const int start = frame * kHopSize;
            std::fill (fftBuf.begin(), fftBuf.end(), 0.0f);
            for (int n = 0; n < kFFTSize; ++n)
                fftBuf[(size_t) n] = audio[start + n] * window[(size_t) n];
            fft.performFrequencyOnlyForwardTransform (fftBuf.data());

            // performFrequencyOnlyForwardTransform writes magnitudes into
            // the first kFFTSize/2+1 entries.  Compute flux against prev frame.
            float f = 0.0f;
            for (int b = 0; b < kFFTSize / 2 + 1; ++b)
            {
                curMag[(size_t) b] = fftBuf[(size_t) b];
                const float diff = curMag[(size_t) b] - prevMag[(size_t) b];
                if (diff > 0.0f) f += diff;
            }
            flux[(size_t) frame] = f;
            std::swap (curMag, prevMag);
        }

        if (flux.empty()) return onsets;

        // Adaptive threshold = mean + kPeakSDFactor * stddev.
        double mean = 0.0;
        for (auto v : flux) mean += v;
        mean /= (double) flux.size();
        double var = 0.0;
        for (auto v : flux) { const double d = v - mean; var += d * d; }
        var /= (double) flux.size();
        const double sd       = std::sqrt (var);
        const double threshold = mean + kPeakSDFactor * sd;

        const int minPeakDistFrames = juce::jmax (1,
            (int) std::round (kMinPeakDistanceSec * sampleRate / (double) kHopSize));

        int lastPeakFrame = -minPeakDistFrames;
        for (int frame = 1; frame + 1 < numFrames; ++frame)
        {
            const float a = flux[(size_t) (frame - 1)];
            const float b = flux[(size_t) frame];
            const float c = flux[(size_t) (frame + 1)];
            if (b <= a || b < c) continue;                   // not a local max
            if ((double) b < threshold) continue;             // below adaptive thr
            if (frame - lastPeakFrame < minPeakDistFrames) continue;
            lastPeakFrame = frame;

            const double t = (frame * kHopSize + kFFTSize / 2) / sampleRate;
            onsets.push_back (t);
        }
        return onsets;
    }

    // ── Greedy nearest-neighbour pairing ─────────────────────────────────────
    // Walks dub onsets forward, pairing each with the nearest unpaired guide
    // onset within `tolerance`.  Cheap; works well on monophonic vocal
    // material where order is preserved.  More elaborate dynamic-time-warp
    // pairing can be added later if pathological reorderings turn up in
    // practice.
    struct Pair { double dub; double guide; };

    // ── Frame-YIN F0 estimate (QA-F Task 3) ─────────────────────────────────
    // One-shot cumulative-mean-normalized difference function over a 2048
    // window: de Cheveigne + Kawahara YIN, bin-level (no parabolic refine --
    // anchor pitch deltas don't need sub-cent precision).  Range 60-1200 Hz.
    // Returns 0 when unvoiced (CMNDF never dips under the threshold).
    static constexpr int   kYinWin       = 2048;
    static constexpr float kYinThreshold = 0.15f;

    float estimateF0Hz (const float* x, int available, double sampleRate)
    {
        if (x == nullptr || available < kYinWin) return 0.0f;
        const int half   = kYinWin / 2;
        const int lagMin = juce::jmax (2,   (int) (sampleRate / 1200.0));
        const int lagMax = juce::jmin (half - 1, (int) (sampleRate / 60.0));
        if (lagMax <= lagMin) return 0.0f;

        float best = 0.0f;
        int   bestLag = 0;
        float running = 0.0f;
        for (int lag = 1; lag <= lagMax; ++lag)
        {
            float d = 0.0f;
            for (int i = 0; i < half; ++i)
            {
                const float diff = x[i] - x[i + lag];
                d += diff * diff;
            }
            running += d;
            if (running <= 0.0f) continue;
            const float cmnd = d * (float) lag / running;
            if (lag >= lagMin && cmnd < kYinThreshold)
            {
                bestLag = lag;
                best    = cmnd;
                // First dip under threshold wins (canonical YIN step 4) --
                // walk forward while it keeps improving, then stop.
                for (int l2 = lag + 1; l2 <= lagMax; ++l2)
                {
                    float d2 = 0.0f;
                    for (int i = 0; i < half; ++i)
                    {
                        const float diff = x[i] - x[i + l2];
                        d2 += diff * diff;
                    }
                    running += d2;
                    const float c2 = d2 * (float) l2 / running;
                    if (c2 < best) { best = c2; bestLag = l2; }
                    else break;
                }
                break;
            }
        }
        if (bestLag <= 0) return 0.0f;
        return (float) (sampleRate / (double) bestLag);
    }

    // F0 at an absolute time position in a mono buffer (window centred).
    float f0AtSec (const float* mono, int numSamples, double sampleRate, double tSec)
    {
        const int center = (int) std::llround (tSec * sampleRate);
        const int start  = juce::jlimit (0, juce::jmax (0, numSamples - kYinWin),
                                         center - kYinWin / 2);
        return estimateF0Hz (mono + start, numSamples - start, sampleRate);
    }

    std::vector<Pair> pairOnsets (const std::vector<double>& dubOnsets,
                                   const std::vector<double>& guideOnsets,
                                   double tolerance)
    {
        std::vector<Pair> pairs;
        std::vector<bool> guideUsed (guideOnsets.size(), false);

        size_t guideHint = 0;
        for (double dt : dubOnsets)
        {
            // Walk guideHint forward to the first guide onset >= dt - tolerance
            while (guideHint < guideOnsets.size()
                   && guideOnsets[guideHint] < dt - tolerance)
                ++guideHint;

            // Find nearest unused guide onset within ±tolerance starting at hint.
            double bestDist = tolerance + 1.0;
            size_t bestIdx  = guideOnsets.size();
            for (size_t g = guideHint; g < guideOnsets.size(); ++g)
            {
                if (guideUsed[g]) continue;
                if (guideOnsets[g] > dt + tolerance) break;
                const double d = std::abs (guideOnsets[g] - dt);
                if (d < bestDist) { bestDist = d; bestIdx = g; }
            }
            if (bestIdx < guideOnsets.size())
            {
                guideUsed[bestIdx] = true;
                pairs.push_back ({ dt, guideOnsets[bestIdx] });
            }
        }
        return pairs;
    }
}

// ─── WarpMap ──────────────────────────────────────────────────────────────────
double WarpMap::getStretchRatioAt (double dubTimeSec) const noexcept
{
    if (anchors.size() < 2) return 1.0;
    if (dubTimeSec <= anchors.front().dubTimeSec) return 1.0;
    if (dubTimeSec >= anchors.back().dubTimeSec)  return 1.0;

    // Find the segment containing dubTimeSec.
    for (size_t i = 0; i + 1 < anchors.size(); ++i)
    {
        const auto& a = anchors[i];
        const auto& b = anchors[i + 1];
        if (dubTimeSec >= a.dubTimeSec && dubTimeSec < b.dubTimeSec)
        {
            const double dDub   = b.dubTimeSec   - a.dubTimeSec;
            const double dGuide = b.guideTimeSec - a.guideTimeSec;
            if (dDub <= 1e-9) return 1.0;
            return dGuide / dDub;
        }
    }
    return 1.0;
}

double WarpMap::mapDubToGuide (double dubTimeSec) const noexcept
{
    if (anchors.size() < 2) return dubTimeSec;
    if (dubTimeSec <= anchors.front().dubTimeSec) return anchors.front().guideTimeSec;
    if (dubTimeSec >= anchors.back().dubTimeSec)  return anchors.back().guideTimeSec;

    for (size_t i = 0; i + 1 < anchors.size(); ++i)
    {
        const auto& a = anchors[i];
        const auto& b = anchors[i + 1];
        if (dubTimeSec >= a.dubTimeSec && dubTimeSec < b.dubTimeSec)
        {
            const double dDub = b.dubTimeSec - a.dubTimeSec;
            if (dDub <= 1e-9) return a.guideTimeSec;
            const double t = (dubTimeSec - a.dubTimeSec) / dDub;
            return a.guideTimeSec + t * (b.guideTimeSec - a.guideTimeSec);
        }
    }
    return dubTimeSec;
}

juce::ValueTree WarpMap::toValueTree() const
{
    juce::ValueTree v ("WarpMap");
    v.setProperty ("dubDur",   dubDurationSec,   nullptr);
    v.setProperty ("guideDur", guideDurationSec, nullptr);
    v.setProperty ("sr",       analysisSampleRate, nullptr);
    for (const auto& a : anchors)
    {
        juce::ValueTree an ("A");
        an.setProperty ("d", a.dubTimeSec,   nullptr);
        an.setProperty ("g", a.guideTimeSec, nullptr);
        an.setProperty ("w", a.weight,       nullptr);
        an.setProperty ("p", a.pitchSemis,   nullptr);
        v.appendChild (an, nullptr);
    }
    return v;
}

void WarpMap::fromValueTree (const juce::ValueTree& v)
{
    anchors.clear();
    if (! v.hasType ("WarpMap")) return;
    dubDurationSec     = (double) v.getProperty ("dubDur",   0.0);
    guideDurationSec   = (double) v.getProperty ("guideDur", 0.0);
    analysisSampleRate = (double) v.getProperty ("sr",       44100.0);
    for (int i = 0; i < v.getNumChildren(); ++i)
    {
        auto an = v.getChild (i);
        if (! an.hasType ("A")) continue;
        WarpAnchor a;
        a.dubTimeSec   = (double) an.getProperty ("d", 0.0);
        a.guideTimeSec = (double) an.getProperty ("g", 0.0);
        a.weight       = (float)(double) an.getProperty ("w", 1.0);
        a.pitchSemis   = (float)(double) an.getProperty ("p", 0.0);
        anchors.push_back (a);
    }
    std::sort (anchors.begin(), anchors.end(),
               [] (const WarpAnchor& l, const WarpAnchor& r) { return l.dubTimeSec < r.dubTimeSec; });
}

// ─── BaySickAlignDSP ──────────────────────────────────────────────────────────
BaySickAlignDSP::BaySickAlignDSP()
{
    mActiveMap = std::make_shared<const WarpMap>();   // empty default map
}

void BaySickAlignDSP::prepare (double sampleRate, int /*maxBlockSize*/)
{
    mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
}

void BaySickAlignDSP::releaseResources()
{
    juce::SpinLock::ScopedLockType lk (mMapLock);
    mActiveMap = std::make_shared<const WarpMap>();
}

void BaySickAlignDSP::setWarpMap (const WarpMap& map)
{
    auto fresh = std::make_shared<const WarpMap> (map);
    juce::SpinLock::ScopedLockType lk (mMapLock);
    mActiveMap = fresh;
}

void BaySickAlignDSP::clearWarpMap()
{
    juce::SpinLock::ScopedLockType lk (mMapLock);
    mActiveMap = std::make_shared<const WarpMap>();
}

bool BaySickAlignDSP::hasWarpMap() const noexcept
{
    juce::SpinLock::ScopedLockType lk (mMapLock);
    return mActiveMap && mActiveMap->isValid();
}

// ─── applyWarp (QA-F Task 3: offline PhaseVocoder warp + pitch render) ────────
namespace
{
    // Exact-length PhaseVocoder stretch of src[0..n) to out[0..outN): pre-roll
    // primes the OLA ramp-in, tail flushes the settle margin, and the final
    // map resamples at the EFFECTIVE ratio (integer-rounded synthesis hop) so
    // the segment end lands sample-exact.  Same recipe as the channel-
    // composite stretch path in PluginProcessor.
    void pvStretchExact (const float* src, juce::int64 srcOff, juce::int64 srcTotal,
                         int n, double ratio, float* out, int outN)
    {
        if (n <= 0 || outN <= 0) return;
        ratio = juce::jlimit (0.25, 4.0, ratio);
        const double effRatio =
            (double) juce::jmax (1, juce::roundToInt (
                (double) PhaseVocoder::kHopSize * ratio))
            / (double) PhaseVocoder::kHopSize;

        PhaseVocoder pv (1);
        pv.setStretchRatio (ratio);

        const juce::int64 preN  = juce::jmin (srcOff, (juce::int64) std::ceil (
            (double) PhaseVocoder::kFFTSize / juce::jmin (1.0, ratio)));
        const juce::int64 tailN = (juce::int64) std::ceil (
            2.0 * (double) PhaseVocoder::kFFTSize / juce::jmin (1.0, ratio));

        std::vector<float> outAccum;
        outAccum.reserve ((size_t) ((double) (preN + n + tailN) * effRatio) + 4096);

        juce::AudioBuffer<float> feed (1, PhaseVocoder::kHopSize);
        juce::AudioBuffer<float> pull (1, 1 << 14);

        juce::int64 fpos = srcOff - preN;
        const juce::int64 feedEnd = srcOff + (juce::int64) n + tailN;
        while (fpos < feedEnd)
        {
            const int chunk = (int) juce::jmin (
                (juce::int64) PhaseVocoder::kHopSize, feedEnd - fpos);
            feed.clear();
            for (int i = 0; i < chunk; ++i)
            {
                const juce::int64 s = fpos + i;
                if (s >= 0 && s < srcTotal)
                    feed.setSample (0, i, src[s]);
            }
            pv.push (feed, 0, chunk);
            for (int got = 0;
                 (got = pv.pull (pull, 0, pull.getNumSamples())) > 0;)
                outAccum.insert (outAccum.end(),
                                 pull.getReadPointer (0),
                                 pull.getReadPointer (0) + got);
            fpos += chunk;
        }

        const double outSkip      = (double) preN * effRatio;
        const double stretchedLen = juce::jmax (1.0, (double) n * effRatio);
        for (int j = 0; j < outN; ++j)
        {
            const double op = outSkip + ((double) j / (double) outN) * stretchedLen;
            const int    ip = (int) op;
            if (ip + 1 >= (int) outAccum.size()) { out[j] = 0.0f; continue; }
            const float  fr = (float) (op - (double) ip);
            out[j] = outAccum[(size_t) ip]
                   + (outAccum[(size_t) ip + 1] - outAccum[(size_t) ip]) * fr;
        }
    }

}

juce::AudioBuffer<float> BaySickAlignDSP::applyWarp (const float* dubMono, int numDubSamples,
                                                     double sampleRate,
                                                     const WarpMap& map,
                                                     int pitchAlgo,
                                                     float extraSemis)
{
    juce::AudioBuffer<float> out;
    if (dubMono == nullptr || numDubSamples <= 0 || sampleRate <= 0.0)
        return out;

    if (! map.isValid())
    {
        out.setSize (1, numDubSamples);
        out.copyFrom (0, 0, dubMono, numDubSamples);
        return out;
    }

    const double dubDurSec = (double) numDubSamples / sampleRate;
    const int outLen = juce::jmax (1, (int) std::llround (
        map.mapDubToGuide (dubDurSec) * sampleRate));
    out.setSize (1, outLen);
    out.clear();
    float* dst = out.getWritePointer (0);

    // ── Phase 1: time-warp assembly, segment-wise, 5 ms seam crossfades ─────
    const int fade = juce::jmax (1, (int) std::round (sampleRate * 0.005));
    std::vector<float> segBuf;

    // Walk anchor segments extended to the buffer edges (the map clamps
    // outside its anchor range, so lead-in/tail render at ratio 1 via the
    // edge anchors).
    for (size_t i = 0; i + 1 < map.anchors.size(); ++i)
    {
        const auto& a = map.anchors[i];
        const auto& b = map.anchors[i + 1];

        const juce::int64 srcA = juce::jlimit ((juce::int64) 0, (juce::int64) numDubSamples,
            (juce::int64) std::llround (a.dubTimeSec * sampleRate));
        const juce::int64 srcB = juce::jlimit (srcA, (juce::int64) numDubSamples,
            (juce::int64) std::llround (b.dubTimeSec * sampleRate));
        const juce::int64 dstA = juce::jlimit ((juce::int64) 0, (juce::int64) outLen,
            (juce::int64) std::llround (a.guideTimeSec * sampleRate));
        const juce::int64 dstB = juce::jlimit (dstA, (juce::int64) outLen,
            (juce::int64) std::llround (b.guideTimeSec * sampleRate));

        const int srcN = (int) (srcB - srcA);
        const int dstN = (int) (dstB - dstA);
        if (srcN <= 0 || dstN <= 0) continue;

        const double ratio = (double) dstN / (double) srcN;

        // Render the segment extended by one fade on each side (sourced
        // proportionally) so adjacent segments crossfade-sum to unity.
        const int extDst = dstN + 2 * fade;
        const int extSrc = juce::jmax (1, (int) std::llround ((double) extDst / ratio));
        const juce::int64 extSrcOff = srcA - (juce::int64) std::llround ((double) fade / ratio);

        segBuf.assign ((size_t) extDst, 0.0f);
        if (std::abs (ratio - 1.0) < 0.001)
        {
            for (int j = 0; j < extDst; ++j)
            {
                const juce::int64 s = extSrcOff + j;
                segBuf[(size_t) j] = (s >= 0 && s < (juce::int64) numDubSamples)
                                       ? dubMono[s] : 0.0f;
            }
        }
        else
        {
            pvStretchExact (dubMono, juce::jmax ((juce::int64) 0, extSrcOff),
                            (juce::int64) numDubSamples,
                            extSrc, ratio, segBuf.data(), extDst);
        }

        const juce::int64 writeBase = dstA - fade;
        for (int j = 0; j < extDst; ++j)
        {
            const juce::int64 p = writeBase + j;
            if (p < 0 || p >= (juce::int64) outLen) continue;
            float g = 1.0f;
            if (j < 2 * fade)                 g = (float) (j + 1) / (float) (2 * fade);
            if (j >= extDst - 2 * fade)
                g = juce::jmin (g, (float) (extDst - j) / (float) (2 * fade));
            dst[p] += segBuf[(size_t) j] * g;
        }
    }

    // ── Phase 2: pitch pass over the warped result (anchor deltas +
    //    Transpose) ─────────────────────────────────────────────────────────
    bool anyPitch = std::abs (extraSemis) > 0.01f;
    for (const auto& a : map.anchors)
        if (std::abs (a.pitchSemis) > 0.01f) { anyPitch = true; break; }
    if (! anyPitch)
        return out;

    if (pitchAlgo == 2)
    {
        // Phase Vocoder: per anchor-segment constant delta (segment midpoint),
        // 5 ms crossfades between differently-shifted spans.
        std::vector<float> shifted ((size_t) outLen, 0.0f);
        std::vector<float> acc     ((size_t) outLen, 0.0f);
        std::vector<float> wsum    ((size_t) outLen, 0.0f);
        for (size_t i = 0; i + 1 < map.anchors.size(); ++i)
        {
            const juce::int64 dstA = juce::jlimit ((juce::int64) 0, (juce::int64) outLen,
                (juce::int64) std::llround (map.anchors[i].guideTimeSec * sampleRate));
            const juce::int64 dstB = juce::jlimit (dstA, (juce::int64) outLen,
                (juce::int64) std::llround (map.anchors[i + 1].guideTimeSec * sampleRate));
            const int n = (int) (dstB - dstA);
            if (n <= 0) continue;
            const float semis = 0.5f * (map.anchors[i].pitchSemis
                                        + map.anchors[i + 1].pitchSemis) + extraSemis;
            const juce::int64 ea = juce::jmax ((juce::int64) 0, dstA - fade);
            const juce::int64 eb = juce::jmin ((juce::int64) outLen, dstB + fade);
            const int en = (int) (eb - ea);
            if (en <= 0) continue;
            shifted.assign ((size_t) en, 0.0f);
            if (std::abs (semis) > 0.01f)
                PvShifter::shiftBufferMono (dst + ea, shifted.data(), en,
                                            std::pow (2.0, (double) semis / 12.0));
            else
                std::copy (dst + ea, dst + ea + en, shifted.begin());
            for (int j = 0; j < en; ++j)
            {
                float g = 1.0f;
                if (j < 2 * fade)      g = (float) (j + 1) / (float) (2 * fade);
                if (j >= en - 2 * fade) g = juce::jmin (g, (float) (en - j) / (float) (2 * fade));
                acc [(size_t) (ea + j)] += shifted[(size_t) j] * g;
                wsum[(size_t) (ea + j)] += g;
            }
        }
        for (int j = 0; j < outLen; ++j)
            dst[j] = (wsum[(size_t) j] > 0.01f) ? acc[(size_t) j] / wsum[(size_t) j]
                                                 : dst[j];
    }
    else
    {
        // PSOLA / Granular: one continuous streaming pass with a per-sample
        // delta lerp.  The period track (PSOLA) is PRE-computed at a 2048
        // hop -- per-sample YIN would take seconds per render -- and the
        // semis lerp advances an anchor cursor instead of re-scanning the
        // anchor list every sample.  A lead-in of real audio primes the
        // shifter; its (approximate) latency is skipped so the result stays
        // time-aligned -- residual error is a few ms, inside alignment
        // tolerance.
        PsolaShifter    psola;
        GranularShifter gran;
        psola.prepare (sampleRate, 512);
        gran .prepare (sampleRate, 512);

        const bool usePsola = (pitchAlgo == 0);
        const int  lat      = usePsola ? 600 : 1200;   // ~2 mean periods / ~1 grain

        std::vector<float> periodTrack;
        if (usePsola)
        {
            const int hop = 2048;
            const int nFrames = outLen / hop + 1;
            periodTrack.assign ((size_t) nFrames, 256.0f);
            float last = 256.0f;
            for (int f = 0; f < nFrames; ++f)
            {
                const int start = juce::jlimit (0, juce::jmax (0, outLen - kYinWin), f * hop);
                const float f0 = estimateF0Hz (dst + start, outLen - start, sampleRate);
                if (f0 > 0.0f)
                    last = (float) (sampleRate / (double) f0);
                periodTrack[(size_t) f] = last;
            }
        }

        std::vector<float> shifted ((size_t) outLen, 0.0f);
        const auto& an = map.anchors;
        size_t ai = 0;

        for (int j = -lat; j < outLen; ++j)
        {
            const int src = juce::jlimit (0, outLen - 1, j + lat);
            if (usePsola && ((j + lat) & 2047) == 0)
                psola.setPeriodSamples (periodTrack[(size_t) juce::jmin (
                    (int) periodTrack.size() - 1, src / 2048)]);

            // Anchor-cursor semis lerp (guide-time domain).
            const double gSec = (double) juce::jmax (0, j) / sampleRate;
            while (ai + 2 < an.size() && gSec >= an[ai + 1].guideTimeSec)
                ++ai;
            float semis;
            {
                const auto& a = an[ai];
                const auto& b = an[ai + 1];
                if (gSec <= a.guideTimeSec)      semis = a.pitchSemis;
                else if (gSec >= b.guideTimeSec) semis = b.pitchSemis;
                else
                {
                    const double d = b.guideTimeSec - a.guideTimeSec;
                    const double t = (d > 1e-9) ? (gSec - a.guideTimeSec) / d : 0.0;
                    semis = a.pitchSemis + (float) t * (b.pitchSemis - a.pitchSemis);
                }
            }
            semis += extraSemis;
            const float ratio = std::pow (2.0f, semis / 12.0f);
            const float v = usePsola ? psola.processSample (dst[src], ratio)
                                     : gran .processSample (dst[src], ratio);
            if (j >= 0)
                shifted[(size_t) j] = v;
        }
        std::copy (shifted.begin(), shifted.end(), dst);
    }

    return out;
}

void BaySickAlignDSP::estimateF0Track (const float* mono, int numSamples,
                                       double sampleRate, int hopSamples,
                                       std::vector<float>& outHz)
{
    outHz.clear();
    if (mono == nullptr || numSamples <= 0 || hopSamples <= 0) return;
    const int nFrames = numSamples / hopSamples + 1;
    outHz.reserve ((size_t) nFrames);
    for (int f = 0; f < nFrames; ++f)
    {
        const int start = juce::jlimit (0, juce::jmax (0, numSamples - kYinWin),
                                        f * hopSamples);
        outHz.push_back (estimateF0Hz (mono + start, numSamples - start, sampleRate));
    }
}

// ─── analyzeOffline ────────────────────────────────────────────────────────────
WarpMap BaySickAlignDSP::analyzeOffline (const float* guideMono, int numGuideSamples,
                                          const float* dubMono,   int numDubSamples,
                                          double sampleRate,
                                          float  strength,
                                          double pairingToleranceSec,
                                          const std::vector<AlignSyncPoint>&     syncPoints,
                                          const std::vector<AlignProtectedArea>& protectedAreas,
                                          float  pitchRange01)
{
    WarpMap map;
    map.analysisSampleRate = sampleRate;
    map.guideDurationSec   = (sampleRate > 0.0) ? numGuideSamples / sampleRate : 0.0;
    map.dubDurationSec     = (sampleRate > 0.0) ? numDubSamples   / sampleRate : 0.0;

    if (guideMono == nullptr || dubMono == nullptr
        || numGuideSamples < kFFTSize || numDubSamples < kFFTSize)
        return map;

    const auto guideOnsets = detectOnsetsSec (guideMono, numGuideSamples, sampleRate);
    const auto dubOnsets   = detectOnsetsSec (dubMono,   numDubSamples,   sampleRate);

    // Sync points sorted by follower time = hard segmentation boundaries
    // (section 13c): auto-pairing runs independently inside each region so a
    // user-placed pair can never be crossed by an automatic pairing.
    auto sp = syncPoints;
    std::sort (sp.begin(), sp.end(),
               [] (const AlignSyncPoint& l, const AlignSyncPoint& r)
               { return l.followerSec < r.followerSec; });

    std::vector<Pair> pairs;
    {
        double dubLo = 0.0, guideLo = 0.0;
        for (size_t seg = 0; seg <= sp.size(); ++seg)
        {
            const double dubHi   = (seg < sp.size()) ? sp[seg].followerSec : map.dubDurationSec;
            const double guideHi = (seg < sp.size()) ? sp[seg].leaderSec   : map.guideDurationSec;

            std::vector<double> dubSeg, guideSeg;
            for (double t : dubOnsets)   if (t >= dubLo   && t < dubHi)   dubSeg.push_back (t);
            for (double t : guideOnsets) if (t >= guideLo && t < guideHi) guideSeg.push_back (t);

            auto segPairs = pairOnsets (dubSeg, guideSeg, pairingToleranceSec);
            pairs.insert (pairs.end(), segPairs.begin(), segPairs.end());

            dubLo = dubHi; guideLo = guideHi;
        }
    }

    const bool haveSync = ! sp.empty();
    if (pairs.size() < 2 && ! haveSync)
        return map;

    // Strength blends each auto anchor's guide time toward its raw dub time
    // when strength < 1.  At 0 the auto anchors collapse to identity.  Sync
    // points are user intent -- they land at FULL strength regardless.
    const float s = juce::jlimit (0.0f, 1.0f, strength);

    // Anchor at start (0,0) so the warp is well-defined from the start.
    map.anchors.push_back ({ 0.0, 0.0, 1.0f, 0.0f });
    for (const auto& p : pairs)
    {
        WarpAnchor a;
        a.dubTimeSec   = p.dub;
        a.guideTimeSec = p.dub + (p.guide - p.dub) * (double) s;
        a.weight       = 1.0f;
        map.anchors.push_back (a);
    }
    for (const auto& x : sp)
    {
        WarpAnchor a;
        a.dubTimeSec   = x.followerSec;
        a.guideTimeSec = x.leaderSec;
        a.weight       = 1.0f;
        map.anchors.push_back (a);
    }
    // Anchor at end (dubDur, guideDur) so the tail isn't unbounded.
    if (map.dubDurationSec > 0.0 && map.guideDurationSec > 0.0)
    {
        WarpAnchor end;
        end.dubTimeSec   = map.dubDurationSec;
        end.guideTimeSec = map.dubDurationSec + (map.guideDurationSec - map.dubDurationSec) * (double) s;
        end.weight       = 1.0f;
        map.anchors.push_back (end);
    }

    std::sort (map.anchors.begin(), map.anchors.end(),
               [] (const WarpAnchor& l, const WarpAnchor& r) { return l.dubTimeSec < r.dubTimeSec; });

    // Monotonicity guard: guide times must never run backwards or applyWarp's
    // segment ratios go negative.  Sync points + tolerant pairing can produce
    // a crossing on hostile input -- clamp forward.
    for (size_t i = 1; i < map.anchors.size(); ++i)
        map.anchors[i].guideTimeSec = juce::jmax (map.anchors[i].guideTimeSec,
                                                  map.anchors[i - 1].guideTimeSec);

    // ── Per-anchor pitch deltas (QA-F Task 3, sections 13f/13g) ─────────────
    // Frame-YIN both sides of every anchor; scaled leader-minus-follower
    // delta drives the +Pitch presets.  Range 0 skips the analysis entirely.
    if (pitchRange01 > 0.001f)
    {
        const float range = juce::jlimit (0.0f, 1.0f, pitchRange01);
        for (auto& a : map.anchors)
        {
            const float fDub   = f0AtSec (dubMono,   numDubSamples,   sampleRate, a.dubTimeSec);
            const float fGuide = f0AtSec (guideMono, numGuideSamples, sampleRate, a.guideTimeSec);
            if (fDub > 0.0f && fGuide > 0.0f)
                a.pitchSemis = juce::jlimit (-12.0f, 12.0f,
                    12.0f * std::log2 (fGuide / fDub)) * range;
        }
    }

    // ── Protected areas post-pass (section 13c) ─────────────────────────────
    // protectTime: ratio forced to 1 through the region -- anchors inside are
    // rebased to (areaEntryGuide + elapsed follower time); offset continuity
    // is preserved, the region itself never stretches.  protectPitch: anchor
    // deltas zeroed inside.
    for (const auto& pa : protectedAreas)
    {
        if (pa.endSec <= pa.startSec) continue;

        if (pa.protectTime && map.anchors.size() >= 2)
        {
            const double entryGuide = map.mapDubToGuide (pa.startSec);
            for (auto& a : map.anchors)
                if (a.dubTimeSec >= pa.startSec && a.dubTimeSec <= pa.endSec)
                    a.guideTimeSec = entryGuide + (a.dubTimeSec - pa.startSec);
            // Re-clamp monotonicity after the rebase.
            for (size_t i = 1; i < map.anchors.size(); ++i)
                map.anchors[i].guideTimeSec = juce::jmax (map.anchors[i].guideTimeSec,
                                                          map.anchors[i - 1].guideTimeSec);
        }
        if (pa.protectPitch)
            for (auto& a : map.anchors)
                if (a.dubTimeSec >= pa.startSec && a.dubTimeSec <= pa.endSec)
                    a.pitchSemis = 0.0f;
    }

    return map;
}
