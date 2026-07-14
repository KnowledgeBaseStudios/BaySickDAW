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
    // anchor pitch deltas don't need sub-cent precision).  Default band
    // 60-1200 Hz; QA-Fd Pitch Types pass per-side bands.  The 2048 window
    // floors the practical low bound near 43 Hz at 44.1 kHz.
    // Returns 0 when unvoiced (CMNDF never dips under the threshold).
    static constexpr int   kYinWin       = 2048;
    static constexpr float kYinThreshold = 0.15f;

    float estimateF0Hz (const float* x, int available, double sampleRate,
                        float minHz = 60.0f, float maxHz = 1200.0f)
    {
        if (x == nullptr || available < kYinWin) return 0.0f;
        const int half   = kYinWin / 2;
        const int lagMin = juce::jmax (2,   (int) (sampleRate / juce::jmax (1.0f, maxHz)));
        const int lagMax = juce::jmin (half - 1, (int) (sampleRate / juce::jmax (1.0f, minHz)));
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
        // Sub-sample (parabolic) refinement of the period (2026-07-12): fit a
        // parabola to the squared-difference function at bestLag-1/0/+1 and take
        // the vertex.  Bin-level lag left the period off by up to +-0.5 sample
        // (~4 cents at 220 Hz) -- enough, once it drives the PSOLA grain marks,
        // to walk the pitch marks off the real pulses over a sustained note.
        double refined = (double) bestLag;
        if (bestLag > lagMin && bestLag + 1 <= lagMax)
        {
            auto dAt = [&] (int lag)
            {
                float dd = 0.0f;
                for (int i = 0; i < half; ++i) { const float df = x[i] - x[i + lag]; dd += df * df; }
                return dd;
            };
            const float dm = dAt (bestLag - 1);
            const float d0 = dAt (bestLag);
            const float dp = dAt (bestLag + 1);
            const float denom = dm - 2.0f * d0 + dp;
            if (std::abs (denom) > 1.0e-9f)
                refined = (double) bestLag
                        + juce::jlimit (-0.5, 0.5, (double) (0.5f * (dm - dp) / denom));
        }
        return (float) (sampleRate / refined);
    }

    // F0 at an absolute time position in a mono buffer (window centred).
    float f0AtSec (const float* mono, int numSamples, double sampleRate, double tSec,
                   float minHz = 60.0f, float maxHz = 1200.0f)
    {
        const int center = (int) std::llround (tSec * sampleRate);
        const int start  = juce::jlimit (0, juce::jmax (0, numSamples - kYinWin),
                                         center - kYinWin / 2);
        return estimateF0Hz (mono + start, numSamples - start, sampleRate, minHz, maxHz);
    }

    // Monotone pairing: dub onsets ascend, so their paired guide onsets must
    // ascend too.  The original nearest-unused search could CROSS pairs (a
    // later dub onset grabbing an EARLIER guide onset); the publish-side
    // monotone clamp then flattened each crossing into a plateau+cliff
    // segment pair = the G2-boundary "choppy" ear verdict.  A greedy
    // first-come monotone matcher fixes the crossings but has a cascade
    // failure of its own: one early grab (a breath onset taking a real
    // word's partner) blocks every later pair from reaching back, which can
    // starve a narrow (Tight) window below the 2-pair minimum.  So: optimal
    // monotone matching -- maximize pair count, tiebreak on smallest total
    // distance.  Monotonicity is structural (both indices only advance).
    std::vector<Pair> pairOnsets (const std::vector<double>& dubOnsets,
                                   const std::vector<double>& guideOnsets,
                                   double tolerance)
    {
        std::vector<Pair> pairs;
        const size_t m = dubOnsets.size(), n = guideOnsets.size();
        if (m == 0 || n == 0) return pairs;

        // DP table cap ~4M cells (~100 MB-safe transient, message thread).
        // Beyond it (pathological hour-long composites) fall back to the
        // greedy monotone walk.
        if ((m + 1) * (n + 1) > (size_t) 4 * 1024 * 1024)
        {
            double lastGuide = -1.0e18;
            size_t guideHint = 0;
            for (double dt : dubOnsets)
            {
                while (guideHint < n && guideOnsets[guideHint] < dt - tolerance)
                    ++guideHint;
                double bestDist = tolerance + 1.0;
                size_t bestIdx  = n;
                for (size_t g = guideHint; g < n; ++g)
                {
                    if (guideOnsets[g] <= lastGuide) continue;
                    if (guideOnsets[g] > dt + tolerance) break;
                    const double d = std::abs (guideOnsets[g] - dt);
                    if (d < bestDist) { bestDist = d; bestIdx = g; }
                }
                if (bestIdx < n)
                {
                    lastGuide = guideOnsets[bestIdx];
                    pairs.push_back ({ dt, guideOnsets[bestIdx] });
                }
            }
            return pairs;
        }

        struct Cell { int count; double cost; unsigned char from; };
        // from: 0 = pair (diag), 1 = skip dub, 2 = skip guide.
        std::vector<Cell> dp ((m + 1) * (n + 1));
        auto at = [&] (size_t i, size_t j) -> Cell&
        { return dp[i * (n + 1) + j]; };

        for (size_t i = 0; i <= m; ++i)
            for (size_t j = 0; j <= n; ++j)
            {
                if (i == 0 && j == 0) { at (0, 0) = { 0, 0.0, 1 }; continue; }
                Cell best { -1, 1.0e18, 1 };
                if (i > 0)
                {
                    const Cell& c = at (i - 1, j);
                    if (c.count > best.count
                        || (c.count == best.count && c.cost < best.cost))
                        best = { c.count, c.cost, 1 };
                }
                if (j > 0)
                {
                    const Cell& c = at (i, j - 1);
                    if (c.count > best.count
                        || (c.count == best.count && c.cost < best.cost))
                        best = { c.count, c.cost, 2 };
                }
                if (i > 0 && j > 0)
                {
                    const double d = std::abs (dubOnsets[i - 1] - guideOnsets[j - 1]);
                    if (d <= tolerance)
                    {
                        const Cell& c = at (i - 1, j - 1);
                        if (c.count + 1 > best.count
                            || (c.count + 1 == best.count && c.cost + d < best.cost))
                            best = { c.count + 1, c.cost + d, 0 };
                    }
                }
                at (i, j) = best;
            }

        size_t i = m, j = n;
        while (i > 0 || j > 0)
        {
            const Cell& c = at (i, j);
            if (c.from == 0)
                { pairs.push_back ({ dubOnsets[i - 1], guideOnsets[j - 1] }); --i; --j; }
            else if (c.from == 1) { if (i > 0) --i; else --j; }
            else                  { if (j > 0) --j; else --i; }
        }
        std::reverse (pairs.begin(), pairs.end());
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

// ─── AlignPlaySnapshot ────────────────────────────────────────────────────────
void AlignPlaySnapshot::computeTangents()
{
    const size_t n = guideSec.size();
    tangent.assign (n, 1.0);
    if (n < 2) return;

    std::vector<double> hseg (n - 1), sec (n - 1);
    for (size_t i = 0; i + 1 < n; ++i)
    {
        hseg[i] = juce::jmax (1.0e-9, guideSec[i + 1] - guideSec[i]);
        sec[i]  = (dubSec[i + 1] - dubSec[i]) / hseg[i];
    }
    tangent[0]     = sec[0];
    tangent[n - 1] = sec[n - 2];
    for (size_t i = 1; i + 1 < n; ++i)
    {
        // Fritsch-Butland weighted harmonic mean: keeps the cubic monotone
        // (dub time can never read backward), flattens to 0 across a
        // direction change or plateau.
        if (sec[i - 1] * sec[i] <= 0.0) { tangent[i] = 0.0; continue; }
        const double w1 = 2.0 * hseg[i] + hseg[i - 1];
        const double w2 = hseg[i] + 2.0 * hseg[i - 1];
        tangent[i] = (w1 + w2) / (w1 / sec[i - 1] + w2 / sec[i]);
    }
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

// ─── applyWarp (QA-Fd Task 8: continuous smooth-map warp + pitch render) ─────
namespace
{
    // Phase-1 worker: ONE streaming PhaseVocoder whose ratio follows the
    // monotone-cubic slope (the live decode's glide), hop-quantized exactly
    // like the live rReq/rEff feedback.  Replaces the per-anchor-segment
    // assembly whose constant per-segment ratios stepped at every anchor.
    // outOffsetSamples = the guide-time base of dst[0] (segmented renders
    // pass the segment's start; see applyWarp's backward-jump split).
    void warpContinuous (const float* src, int srcTotal, double sampleRate,
                         const AlignPlaySnapshot& look, float* dst, int outLen,
                         juce::int64 outOffsetSamples)
    {
        if (outLen <= 0) return;
        constexpr int hop = PhaseVocoder::kHopSize;
        PhaseVocoder pv (1);

        double slope = 1.0; float semis = 0.0f;
        double dStart = 0.0;
        look.lookupAtGuideSec ((double) outOffsetSamples / sampleRate,
                               dStart, slope, semis);

        const int preN = PhaseVocoder::kFFTSize;
        juce::int64 srcCursor = (juce::int64) std::llround (dStart * sampleRate) - preN;
        double srcFrac = 0.0;

        // Review fix: feed chunks stay under the PhaseVocoder input ring
        // (8192) minus its worst-case pending frame (kFFTSize-1) -- an
        // oversized chunk overwrote unread input on steep detach ramps.
        juce::AudioBuffer<float> feed (1, 4096);
        juce::AudioBuffer<float> pull (1, 1 << 14);
        std::vector<float> outAccum;
        outAccum.reserve ((size_t) outLen + 8192);

        auto pushSrc = [&] (int nIn)
        {
            while (nIn > 0)
            {
                const int chunk = juce::jmin (nIn, feed.getNumSamples());
                feed.clear();
                float* fp = feed.getWritePointer (0);
                for (int i = 0; i < chunk; ++i)
                {
                    const juce::int64 s = srcCursor + i;
                    fp[i] = (s >= 0 && s < (juce::int64) srcTotal) ? src[s] : 0.0f;
                }
                pv.push (feed, 0, chunk);
                srcCursor += chunk;
                nIn -= chunk;
                for (int got = 0;
                     (got = pv.pull (pull, 0, pull.getNumSamples())) > 0;)
                    outAccum.insert (outAccum.end(), pull.getReadPointer (0),
                                     pull.getReadPointer (0) + got);
            }
        };

        // Per-window bookkeeping: the hop-rounded EFFECTIVE ratio decides how
        // much output each window really produced; the final map reads each
        // output window from its exact produced span so quantization never
        // accumulates as position drift (the live outRate-feedback analog).
        // Review fix: rReq clamps to the PV trio's documented [0.25, 4] --
        // the old 1/64..64 rail let a plateau/ramp window demand synthesis
        // hops beyond the vocoder's rings.  Position bookkeeping stays exact
        // (windows read what was actually produced); a railed window plays
        // time-scaled content, bounded instead of corrupted.
        struct Win { int nIn; double rEff; };
        std::vector<Win> wins;
        wins.reserve ((size_t) (outLen / hop + 2));

        double preEff = 1.0;
        {
            double d1;
            look.lookupAtGuideSec ((double) (outOffsetSamples + hop) / sampleRate,
                                   d1, slope, semis);
            const double tc0  = juce::jmax (1.0, (d1 - dStart) * sampleRate);
            const double rReq = juce::jlimit (0.25, 4.0, (double) hop / tc0);
            preEff = (double) juce::jmax (1, juce::roundToInt ((double) hop * rReq))
                     / (double) hop;
            pv.setStretchRatio (rReq);
            pushSrc (preN);
        }

        for (int op = 0; op < outLen; op += hop)
        {
            const double g0 = (double) (outOffsetSamples + op) / sampleRate;
            const double g1 = (double) (outOffsetSamples + op + hop) / sampleRate;
            double d0 = g0, d1 = g1;
            look.lookupAtGuideSec (g0, d0, slope, semis);
            look.lookupAtGuideSec (g1, d1, slope, semis);
            const double need = juce::jmax (1.0, (d1 - d0) * sampleRate) + srcFrac;
            const int nIn = juce::jmax (1, (int) need);
            srcFrac = need - (double) nIn;
            const double rReq = juce::jlimit (0.25, 4.0,
                                              (double) hop / (double) nIn);
            const double rEff = (double) juce::jmax (1, juce::roundToInt (
                                    (double) hop * rReq)) / (double) hop;
            pv.setStretchRatio (rReq);
            pushSrc (nIn);
            wins.push_back ({ nIn, rEff });
        }
        pushSrc (2 * PhaseVocoder::kFFTSize);   // tail flush

        double readPos = (double) preN * preEff;
        int written = 0;
        for (const auto& w : wins)
        {
            const double produced = (double) w.nIn * w.rEff;
            const int outThis = juce::jmin (hop, outLen - written);
            for (int j = 0; j < outThis; ++j)
            {
                const double opos = readPos
                    + ((double) j / (double) hop) * produced;
                const int ip = (int) opos;
                if (ip + 1 >= (int) outAccum.size()) break;
                const float fr = (float) (opos - (double) ip);
                dst[written + j] = outAccum[(size_t) ip]
                    + (outAccum[(size_t) ip + 1] - outAccum[(size_t) ip]) * fr;
            }
            written += outThis;
            readPos += produced;
            if (written >= outLen) break;
        }
    }
}

juce::AudioBuffer<float> BaySickAlignDSP::applyWarp (const float* dubMono, int numDubSamples,
                                                     double sampleRate,
                                                     const WarpMap& map,
                                                     int pitchAlgo,
                                                     float extraSemis,
                                                     int oversample)
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

    // ── Phase 1: continuous smooth-map warp (publish-equivalent lookup) ─────
    // Review fix: the map splits into MONOTONE-SOURCE segments at backward
    // jumps (a detached pill may reorder material) -- each segment renders
    // through its own continuous pass into its own guide span, and the
    // boundary is a clean splice: the render analog of the live forceSeek
    // cut.  Maps without reordering are a single segment (previous behavior).
    {
        auto anchors = map.anchors;
        std::sort (anchors.begin(), anchors.end(),
                   [] (const WarpAnchor& a, const WarpAnchor& b)
                   { return a.guideTimeSec < b.guideTimeSec; });

        oversample = juce::jlimit (1, 8, oversample);

        auto renderSpan = [&] (const AlignPlaySnapshot& look,
                               juce::int64 outStart, int spanLen)
        {
            if (spanLen <= 0) return;
            if (oversample <= 1)
            {
                warpContinuous (dubMono, numDubSamples, sampleRate, look,
                                dst + outStart, spanLen, outStart);
                return;
            }
            // QA-Fd 16a High-Res: run the warp at sampleRate * oversample
            // (384 kHz-class at 8x) for finer PhaseVocoder time resolution,
            // then decimate.  The map is in seconds -- rate-invariant.
            // Interpolator inputs get an 8-sample zero guard: Lagrange
            // reads a few samples past ratio*numOut.
            const int upLen  = numDubSamples * oversample;
            const int upSpan = spanLen * oversample;
            std::vector<float> srcPadded ((size_t) numDubSamples + 8, 0.0f);
            std::copy (dubMono, dubMono + numDubSamples, srcPadded.begin());
            juce::AudioBuffer<float> up (1, upLen);
            {
                juce::LagrangeInterpolator li;
                li.reset();
                li.process (1.0 / (double) oversample, srcPadded.data(),
                            up.getWritePointer (0), upLen);
            }
            std::vector<float> warpedHi ((size_t) upSpan + 8, 0.0f);
            warpContinuous (up.getReadPointer (0), upLen,
                            sampleRate * (double) oversample, look,
                            warpedHi.data(), upSpan,
                            outStart * (juce::int64) oversample);
            juce::LagrangeInterpolator down;
            down.reset();
            down.process ((double) oversample, warpedHi.data(),
                          dst + outStart, spanLen);
        };

        size_t segFirst = 0;
        for (size_t i = 1; i <= anchors.size(); ++i)
        {
            const bool split = (i == anchors.size())
                || (anchors[i].dubTimeSec < anchors[i - 1].dubTimeSec - 1.0e-4);
            if (! split) continue;

            AlignPlaySnapshot look;
            double lastDub = -1.0e18;
            for (size_t k = segFirst; k < i; ++k)
            {
                const auto& a = anchors[k];
                const double d = juce::jmax (lastDub, a.dubTimeSec);
                if (! look.guideSec.empty()
                    && a.guideTimeSec - look.guideSec.back() < 1.0e-4)
                {
                    look.dubSec.back()     = d;
                    look.pitchSemis.back() = a.pitchSemis;
                }
                else
                {
                    look.guideSec  .push_back (a.guideTimeSec);
                    look.dubSec    .push_back (d);
                    look.pitchSemis.push_back (a.pitchSemis);
                }
                lastDub = d;
            }
            if (look.guideSec.size() == 1)
            {
                // Lone anchor after a split: extend at slope 1 for tangents.
                look.guideSec  .push_back (look.guideSec.front() + 0.05);
                look.dubSec    .push_back (look.dubSec.front() + 0.05);
                look.pitchSemis.push_back (look.pitchSemis.front());
            }
            look.computeTangents();

            const juce::int64 outStart = (segFirst == 0) ? 0
                : juce::jlimit ((juce::int64) 0, (juce::int64) outLen,
                    (juce::int64) std::llround (anchors[segFirst].guideTimeSec
                                                * sampleRate));
            const juce::int64 outEnd = (i == anchors.size())
                ? (juce::int64) outLen
                : juce::jlimit (outStart, (juce::int64) outLen,
                    (juce::int64) std::llround (anchors[i].guideTimeSec
                                                * sampleRate));
            renderSpan (look, outStart, (int) (outEnd - outStart));
            segFirst = i;
        }
    }

    // ── Phase 2: pitch pass over the warped result (anchor deltas +
    //    Transpose) ─────────────────────────────────────────────────────────
    const int fade = juce::jmax (1, (int) std::round (sampleRate * 0.005));
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
                                          const AlignBuildParams& params,
                                          const std::vector<AlignSyncPoint>&     syncPoints,
                                          const std::vector<AlignProtectedArea>& protectedAreas,
                                          AlignAnalyzeDiag* diagOut)
{
    if (diagOut != nullptr)
        *diagOut = { 0, 0, 0, kMatchWindowSec };

    WarpMap map;
    map.analysisSampleRate = sampleRate;
    map.guideDurationSec   = (sampleRate > 0.0) ? numGuideSamples / sampleRate : 0.0;
    map.dubDurationSec     = (sampleRate > 0.0) ? numDubSamples   / sampleRate : 0.0;

    if (guideMono == nullptr || dubMono == nullptr
        || numGuideSamples < kFFTSize || numDubSamples < kFFTSize)
        return map;

    const auto guideOnsets  = detectOnsetsSec (guideMono, numGuideSamples, sampleRate);
    const auto dubOnsetsRaw = detectOnsetsSec (dubMono,   numDubSamples,   sampleRate);
    if (diagOut != nullptr)
    {
        diagOut->guideOnsets = (int) guideOnsets.size();
        diagOut->dubOnsets   = (int) dubOnsetsRaw.size();
    }

    // QA-Fd (locked 5/13a): pairing runs in the EDITED performance domain --
    // the pitch tab's time edits redefine where the follower's words sound,
    // and that is the timing align must match.  Onsets keep their RAW twin
    // so anchor pitch deltas can still sample the raw audio.
    const bool haveEdit = (bool) params.dubEditTransform;
    auto editedOf = [&] (double t) -> double
    { return haveEdit ? params.dubEditTransform (t) : t; };

    struct DubOnset { double edited, raw; };
    std::vector<DubOnset> dubOnsets;
    dubOnsets.reserve (dubOnsetsRaw.size());
    for (double t : dubOnsetsRaw)
        dubOnsets.push_back ({ editedOf (t), t });
    // Detached pills can reorder material in edited time.
    std::sort (dubOnsets.begin(), dubOnsets.end(),
               [] (const DubOnset& l, const DubOnset& r)
               { return l.edited < r.edited; });

    const double dubDurEdited = editedOf (map.dubDurationSec);

    // Sync points sorted by follower time = hard segmentation boundaries
    // (section 13c): auto-pairing runs independently inside each region so a
    // user-placed pair can never be crossed by an automatic pairing.  The
    // user places them on the RAW lanes; segmentation runs in edited time.
    auto sp = syncPoints;
    std::sort (sp.begin(), sp.end(),
               [] (const AlignSyncPoint& l, const AlignSyncPoint& r)
               { return l.followerSec < r.followerSec; });

    struct RawPair { double dubEdited, dubRaw, guide; };
    std::vector<RawPair> pairs;
    {
        double dubLo = 0.0, guideLo = 0.0;
        for (size_t seg = 0; seg <= sp.size(); ++seg)
        {
            const double dubHi   = (seg < sp.size()) ? editedOf (sp[seg].followerSec)
                                                     : dubDurEdited;
            const double guideHi = (seg < sp.size()) ? sp[seg].leaderSec
                                                     : map.guideDurationSec;

            std::vector<double> dubSeg, guideSeg;
            std::vector<double> dubSegRaw;
            for (const auto& o : dubOnsets)
                if (o.edited >= dubLo && o.edited < dubHi)
                    { dubSeg.push_back (o.edited); dubSegRaw.push_back (o.raw); }
            for (double t : guideOnsets) if (t >= guideLo && t < guideHi) guideSeg.push_back (t);

            auto segPairs = pairOnsets (dubSeg, guideSeg, kMatchWindowSec);
            for (const auto& p : segPairs)
            {
                // Recover the raw twin (pair dub values come verbatim from
                // dubSeg, which is sorted -- binary search by value).
                const auto it = std::lower_bound (dubSeg.begin(), dubSeg.end(), p.dub);
                const double raw = (it != dubSeg.end() && *it == p.dub)
                    ? dubSegRaw[(size_t) std::distance (dubSeg.begin(), it)]
                    : p.dub;
                pairs.push_back ({ p.dub, raw, p.guide });
            }

            dubLo = dubHi; guideLo = guideHi;
        }
    }

    if (diagOut != nullptr)
        diagOut->pairs = (int) pairs.size();

    const bool haveSync = ! sp.empty();
    if (pairs.size() < 2 && ! haveSync)
        return map;

    // ── Residual-cap anchor build (QA-Fd, locked 9a/10a/15a) ────────────────
    // Offset d = leader minus follower per pair.  Within the residual cap R
    // the word keeps its natural timing (anchor lands AT the dub time);
    // beyond R it is pulled to the cap edge, and Max Shift M bounds the
    // applied movement: move = min(max(|d| - R, 0), M).  Tight R=0 = every
    // pair lands on the leader (fully locked).  Sync points are user intent
    // and land at their exact pairing regardless of the caps.
    //
    // Raw pitch deltas are sampled at the RAW pair positions (leader's word
    // is at p.guide even when the cap leaves the anchor at dub) and stored
    // unscaled -- Blend/Variation apply at publish.
    const double R = juce::jmax (0.0, params.residualCapSec);
    const double M = juce::jmax (0.0, params.maxShiftSec);

    auto rawDeltaSemis = [&] (double dubSec, double guideSec) -> float
    {
        const float fDub   = f0AtSec (dubMono,   numDubSamples,   sampleRate,
                                      dubSec,   params.dubMinHz,   params.dubMaxHz);
        const float fGuide = f0AtSec (guideMono, numGuideSamples, sampleRate,
                                      guideSec, params.guideMinHz, params.guideMaxHz);
        if (fDub > 0.0f && fGuide > 0.0f)
            return juce::jlimit (-12.0f, 12.0f, 12.0f * std::log2 (fGuide / fDub));
        return 0.0f;
    };

    // hard = endpoint or user sync point: never dropped by the slope bound.
    struct Flagged { WarpAnchor a; bool hard; };
    std::vector<Flagged> fl;

    // Anchor at start (0,0) so the warp is well-defined from the start.
    fl.push_back ({ { 0.0, 0.0, 1.0f, 0.0f }, true });
    for (const auto& p : pairs)
    {
        WarpAnchor a;
        a.dubTimeSec   = p.dubEdited;
        const double d    = p.guide - p.dubEdited;
        const double move = juce::jlimit (0.0, M, std::abs (d) - R);
        a.guideTimeSec = p.dubEdited + (d >= 0.0 ? move : -move);
        a.weight       = 1.0f;
        a.pitchSemis   = rawDeltaSemis (p.dubRaw, p.guide);
        fl.push_back ({ a, false });
    }
    for (const auto& x : sp)
    {
        WarpAnchor a;
        a.dubTimeSec   = editedOf (x.followerSec);
        a.guideTimeSec = x.leaderSec;
        a.weight       = 1.0f;
        a.pitchSemis   = rawDeltaSemis (x.followerSec, x.leaderSec);
        fl.push_back ({ a, true });
    }

    std::sort (fl.begin(), fl.end(),
               [] (const Flagged& l, const Flagged& r)
               { return l.a.dubTimeSec < r.a.dubTimeSec; });

    // End anchor continues the LAST anchor's offset at slope 1 (natural
    // tail).  The old build pinned dubDur onto guideDur, which imposed a
    // duration-equalizing stretch on the whole tail -- the residual model
    // wants untouched timing everywhere no pairing demands movement.
    // (Edited domain: the map's input is the edited performance timeline.)
    if (dubDurEdited > 0.0 && dubDurEdited > fl.back().a.dubTimeSec)
    {
        WarpAnchor end;
        end.dubTimeSec   = dubDurEdited;
        end.guideTimeSec = fl.back().a.guideTimeSec
                         + (dubDurEdited - fl.back().a.dubTimeSec);
        end.weight       = 1.0f;
        fl.push_back ({ end, true });
    }

    // Monotonicity guard: guide times must never run backwards or applyWarp's
    // segment ratios go negative.  Sync points + tolerant pairing can produce
    // a crossing on hostile input -- clamp forward.
    for (size_t i = 1; i < fl.size(); ++i)
        fl[i].a.guideTimeSec = juce::jmax (fl[i].a.guideTimeSec,
                                           fl[i - 1].a.guideTimeSec);

    // Segment-slope bound [1/r, r] on flexRatio r: an auto anchor whose
    // segment demands more time-bend than the bound allows is a mispairing
    // or an over-reach; drop it and let the neighbors span the region.  Sync
    // points + endpoints survive (user intent; the lookup clamp bounds a
    // hard-vs-hard breach).  The live recovery-glide 2:1 cap in the decode
    // layer is a separate constant and intentionally stays.  r is fixed at
    // 2.0 (Normal) since the Flexibility picker's removal (2026-07-11); the
    // r <= 1 unbounded branch is retained for a possible return of the
    // control.
    if (params.flexRatio > 1.0)
    {
        const double rHi = params.flexRatio;
        const double rLo = 1.0 / params.flexRatio;
        for (bool changed = true; changed;)
        {
            changed = false;
            for (size_t i = 1; i < fl.size(); ++i)
            {
                const double dDub   = fl[i].a.dubTimeSec   - fl[i - 1].a.dubTimeSec;
                const double dGuide = fl[i].a.guideTimeSec - fl[i - 1].a.guideTimeSec;
                if (dDub <= 1.0e-6) continue;
                const double r = dGuide / dDub;
                if (r >= rLo && r <= rHi) continue;
                if      (! fl[i].hard)     { fl.erase (fl.begin() + (long) i);       changed = true; break; }
                else if (! fl[i - 1].hard) { fl.erase (fl.begin() + (long) (i - 1)); changed = true; break; }
            }
        }
    }

    map.anchors.clear();
    map.anchors.reserve (fl.size());
    for (const auto& f : fl)
        map.anchors.push_back (f.a);

    // ── Protected areas post-pass (section 13c) ─────────────────────────────
    // protectTime: ratio forced to 1 through the region -- anchors inside are
    // rebased to (areaEntryGuide + elapsed follower time); offset continuity
    // is preserved, the region itself never stretches.  protectPitch: anchor
    // deltas zeroed inside.  Bounds placed on the raw lanes -> compared in
    // the edited domain the anchors live in.
    for (const auto& pa : protectedAreas)
    {
        if (pa.endSec <= pa.startSec) continue;
        const double paStart = editedOf (pa.startSec);
        const double paEnd   = editedOf (pa.endSec);

        if (pa.protectTime && map.anchors.size() >= 2)
        {
            const double entryGuide = map.mapDubToGuide (paStart);
            for (auto& a : map.anchors)
                if (a.dubTimeSec >= paStart && a.dubTimeSec <= paEnd)
                    a.guideTimeSec = entryGuide + (a.dubTimeSec - paStart);
            // Re-clamp monotonicity after the rebase.
            for (size_t i = 1; i < map.anchors.size(); ++i)
                map.anchors[i].guideTimeSec = juce::jmax (map.anchors[i].guideTimeSec,
                                                          map.anchors[i - 1].guideTimeSec);
        }
        if (pa.protectPitch)
            for (auto& a : map.anchors)
                if (a.dubTimeSec >= paStart && a.dubTimeSec <= paEnd)
                    a.pitchSemis = 0.0f;
    }

    return map;
}
