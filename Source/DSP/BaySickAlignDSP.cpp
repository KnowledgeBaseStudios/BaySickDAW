#include "BaySickAlignDSP.h"
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

int BaySickAlignDSP::applyWarp (const float* in, float* out, int numSamples,
                                 double /*positionSec*/) noexcept
{
    // H-6a scope: passthrough.  Real PhaseVocoder-driven warp playback lands
    // in H-6c when the BaySickAlign editor is built and clip-context wiring
    // hands us the source audio + position.  The infrastructure here
    // (WarpMap, analyseOffline, setWarpMap) is enough for the editor to
    // build + render the offline-aligned audio file via render-to-target.
    if (in != out && in != nullptr && out != nullptr)
        std::memcpy (out, in, (size_t) numSamples * sizeof (float));
    return numSamples;
}

// ─── analyzeOffline ────────────────────────────────────────────────────────────
WarpMap BaySickAlignDSP::analyzeOffline (const float* guideMono, int numGuideSamples,
                                          const float* dubMono,   int numDubSamples,
                                          double sampleRate,
                                          float  strength,
                                          double pairingToleranceSec)
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

    if (guideOnsets.size() < 2 || dubOnsets.size() < 2)
        return map;

    auto pairs = pairOnsets (dubOnsets, guideOnsets, pairingToleranceSec);
    if (pairs.size() < 2) return map;

    // Strength blends each anchor's guide time toward its raw dub time when
    // strength < 1.  At 0, all anchors collapse to their dub time = identity
    // map (passthrough).  At 1.0, anchors land exactly on the guide onsets.
    const float s = juce::jlimit (0.0f, 1.0f, strength);

    // Anchor at start (0,0) so the warp is well-defined from the start of
    // playback even before the first detected onset.
    map.anchors.push_back ({ 0.0, 0.0, 1.0f });
    for (const auto& p : pairs)
    {
        WarpAnchor a;
        a.dubTimeSec   = p.dub;
        a.guideTimeSec = p.dub + (p.guide - p.dub) * (double) s;
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

    // Re-sort to be safe (analyseOffline shouldn't produce out-of-order
    // anchors but defensive).
    std::sort (map.anchors.begin(), map.anchors.end(),
               [] (const WarpAnchor& l, const WarpAnchor& r) { return l.dubTimeSec < r.dubTimeSec; });

    return map;
}
