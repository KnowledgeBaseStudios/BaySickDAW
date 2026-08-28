// BaySickDAW — the EQ window's spectrum analyser (QA-EqPro).
//
// Ported from KBS EQ Pro's EqAnalyser: 8192-point FFT over the SpectrumFeed's
// ring, resampled onto the graph's log axis, with the display tilt that makes
// pink material read flat.  The display this replaces ran 1024 points with no
// tilt and normalized 12 dB low - all three were recorded defects (D6), and
// all three fixes live here.
//
// Three surfaces from one transform: the line spectrum (pre dim, post bright,
// sidechain in its own color), the spectrogram heatmap, and the collision
// band (where the sidechain and the input both carry energy - the masking
// view).
#pragma once

#include <JuceHeader.h>
#include "../../DSP/Kbs/FFT.h"
#include "../../DSP/Kbs/Feeds.h"

namespace eqview {

class EqAnalyser
{
public:
    static constexpr int kOrder = 13;               // 8192
    static constexpr int kSize = 1 << kOrder;
    static constexpr int kBins = kSize / 2;

    EqAnalyser() : fft (kOrder)
    {
        window.resize (kSize);
        for (int i = 0; i < kSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (kbs::kTwoPi * i / kSize);

        // Sized to the FEED's ring, not this FFT: poll() fills the whole ring
        // and the transform takes the newest kSize of it.  Sizing this to
        // kSize was a heap overrun in the KBS build that survived two
        // screenshot runs on luck.
        raw.resize (kbs::SpectrumFeed::kSize);
        td.resize (kSize);
        binsDb.assign (kBins, -120.0f);
        smoothDb.assign (kBins, -120.0f);
        peakDb.assign (kBins, -120.0f);
    }

    void setSampleRate (double sr) { sampleRate = sr; }

    // Speed is the display fall rate: how fast the picture lets go.
    enum class Speed { fast, medium, slow };
    Speed speed = Speed::medium;
    float tiltDbPerOct = 4.5f;    // 0 / 3 / 4.5; 4.5 reads pink as flat
    bool frozen = false;
    bool peakHold = false;

    // Pull the latest window from a feed and fold it into the smoothed
    // display state.  Returns false when the feed had nothing new.
    bool analyse (const kbs::SpectrumFeed& feed, bool sideStream = false)
    {
        if (frozen) return false;
        if (! (sideStream ? feed.pollSide (raw.data()) : feed.poll (raw.data())))
            return false;

        // The feed ring is longer than the FFT: take the most recent kSize.
        const int off = (int) raw.size() - kSize;
        for (int i = 0; i < kSize; ++i)
            td[(size_t) i] = { raw[(size_t) (off + i)] * window[(size_t) i], 0.0f };

        fft.transform (td.data(), false);

        // Single-sided magnitude, normalized so a full-scale sine reads
        // 0 dBFS: bin magnitude N/4 under a Hann window (N/2 single-sided
        // times the window's 0.5 coherent gain).  The old display divided by
        // N and sat 12 dB low for ever.
        const float norm = 4.0f / (float) kSize;
        const float fall = speed == Speed::fast ? 0.55f
                         : speed == Speed::slow ? 0.94f : 0.82f;

        for (int k = 0; k < kBins; ++k)
        {
            const float mag = std::abs (td[(size_t) k]) * norm;
            float db = 20.0f * std::log10 (std::max (mag, 1.0e-7f));

            if (tiltDbPerOct > 0.0f)
            {
                const double hz = (double) k * sampleRate / kSize;
                if (hz > 20.0)
                    db += tiltDbPerOct * (float) (std::log2 (hz / 1000.0));
            }

            binsDb[(size_t) k] = db;

            // Braced deliberately: unbraced, the second line runs every frame
            // and writes into a vector that is empty until a capture starts.
            if (averaging)
            {
                avgSum[(size_t) k] += db;
                avgSq[(size_t) k]  += (double) db * db;
            }

            // Rise instantly, fall at the display speed - an analyser that
            // smooths its attack hides the transient it exists to show.
            auto& s = smoothDb[(size_t) k];
            s = db > s ? db : s * fall + db * (1.0f - fall);

            if (peakHold)
                peakDb[(size_t) k] = std::max (peakDb[(size_t) k], db);
            if (armHold)
                armHoldDb[(size_t) k] = std::max (armHoldDb[(size_t) k], db);
        }
        if (averaging) ++avgCount;
        return true;
    }

    void clearPeaks() { std::fill (peakDb.begin(), peakDb.end(), -120.0f); }

    // Max-hold while the spectrum grab is armed: the marker hunts on the
    // loudest thing each bin has done since arming, which only ever grows -
    // so it stops chasing transients around the screen.
    void beginArmHold()
    {
        armHoldDb.assign (kBins, -120.0f);
        armHold = true;
    }
    void endArmHold() { armHold = false; }

    // ── long-term averaging, for EQ Match captures ────────────────────────
    void startAverage()
    {
        avgSum.assign (kBins, 0.0);
        avgSq.assign (kBins, 0.0);
        avgCount = 0;
        averaging = true;
    }

    void stopAverage() { averaging = false; }
    bool isAveraging() const { return averaging; }
    int averagedFrames() const { return avgCount; }

    // The average resampled onto EQ Match's log grid.  False until at least
    // a handful of frames have landed.
    bool averagedGrid (float* out, int points, double loHz, double hiHz) const
    {
        if (avgCount < 4) return false;
        for (int i = 0; i < points; ++i)
        {
            const double hz = loHz * std::pow (hiHz / loHz, (double) i / (points - 1));
            const double bin = hz * kSize / sampleRate;
            const int k = juce::jlimit (1, kBins - 2, (int) bin);
            const double t = juce::jlimit (0.0, 1.0, bin - k);
            out[i] = (float) ((avgSum[(size_t) k] * (1.0 - t)
                             + avgSum[(size_t) (k + 1)] * t) / avgCount);
        }
        return true;
    }

    // How far each point swings around its own average, in dB.  A steady band
    // and a band that only misbehaves on peaks have the same mean and very
    // different spreads, and that difference is what decides whether EQ Match
    // spends a static bell or a dynamic one.  Nearest-bin, no interpolation -
    // a spread number does not need sub-bin precision.
    bool averagedSpreadGrid (float* out, int points, double loHz, double hiHz) const
    {
        if (avgCount < 4) return false;
        for (int i = 0; i < points; ++i)
        {
            const double hz = loHz * std::pow (hiHz / loHz, (double) i / (points - 1));
            const double bin = hz * kSize / sampleRate;
            const int k = juce::jlimit (1, kBins - 2, (int) bin);
            const double mean = avgSum[(size_t) k] / avgCount;
            const double var = std::max (0.0, avgSq[(size_t) k] / avgCount - mean * mean);
            out[i] = (float) std::sqrt (var);
        }
        return true;
    }

    // ── spectrum grab: the resonance nearest a frequency ──────────────────
    struct Peak { double hz = 0.0; float db = 0.0f, prominenceDb = 0.0f, q = 1.0f; };

    bool findPeakNear (double aroundHz, Peak& out, bool useHold = false) const
    {
        const auto& spec = useHold && armHold ? armHoldDb : smoothDb;
        const double loHz = aroundHz * 0.707, hiHz = aroundHz * 1.414;
        const int kLo = juce::jlimit (2, kBins - 3, (int) (loHz * kSize / sampleRate));
        const int kHi = juce::jlimit (2, kBins - 3, (int) (hiHz * kSize / sampleRate));
        if (kHi <= kLo + 2) return false;

        // The neighbourhood floor: median over the surrounding octave.
        std::vector<float> hood;
        const int mLo = juce::jlimit (1, kBins - 2, kLo - (kHi - kLo));
        const int mHi = juce::jlimit (1, kBins - 2, kHi + (kHi - kLo));
        for (int k = mLo; k <= mHi; ++k) hood.push_back (spec[(size_t) k]);
        std::nth_element (hood.begin(), hood.begin() + (long) hood.size() / 2, hood.end());
        const float floorDb = hood[hood.size() / 2];

        int best = -1;
        float bestDb = -200.0f;
        for (int k = kLo; k <= kHi; ++k)
        {
            const float v = spec[(size_t) k];
            if (v > bestDb
                && v >= spec[(size_t) (k - 1)] && v >= spec[(size_t) (k + 1)])
            { bestDb = v; best = k; }
        }
        if (best < 0) return false;

        const float prom = bestDb - floorDb;
        if (prom < 5.0f) return false;            // nothing stands out

        // Width at half prominence, walked outward, for the Q suggestion.
        const float half = floorDb + prom * 0.5f;
        int lo = best, hi = best;
        while (lo > 1 && spec[(size_t) (lo - 1)] > half) --lo;
        while (hi < kBins - 2 && spec[(size_t) (hi + 1)] > half) ++hi;
        const double fLo = (double) lo * sampleRate / kSize;
        const double fHi = (double) hi * sampleRate / kSize;
        const double fPk = (double) best * sampleRate / kSize;

        out.hz = fPk;
        out.db = bestDb;
        out.prominenceDb = prom;
        out.q = (float) juce::jlimit (0.5, 24.0, fPk / std::max (1.0, fHi - fLo));
        return true;
    }

    void clearAll()
    {
        std::fill (smoothDb.begin(), smoothDb.end(), -120.0f);
        clearPeaks();
    }

    // dB at an arbitrary frequency, linearly interpolated between bins.
    float dbAt (double hz, bool peaks = false) const
    {
        const auto& src = peaks ? peakDb : smoothDb;
        const double bin = hz * kSize / sampleRate;
        const int k = (int) bin;
        if (k < 1 || k >= kBins - 1) return -120.0f;
        const float t = (float) (bin - k);
        return src[(size_t) k] * (1.0f - t) + src[(size_t) (k + 1)] * t;
    }

    // Build the analyser outline across an area, one point per column pair.
    juce::Path buildPath (juce::Rectangle<float> area, float floorDb,
                          const std::function<double (float x)>& xToHz,
                          bool peaks = false) const
    {
        juce::Path p;
        const int w = juce::jmax (1, (int) area.getWidth());
        p.preallocateSpace (w * 3 + 8);

        p.startNewSubPath (area.getX(), area.getBottom());
        for (int px = 0; px <= w; px += 2)
        {
            const double hz = xToHz (area.getX() + (float) px);
            const float db = juce::jlimit (floorDb, 0.0f, dbAt (hz, peaks));
            const float y = juce::jmap (db, floorDb, 0.0f, area.getBottom(), area.getY());
            p.lineTo (area.getX() + (float) px, y);
        }
        p.lineTo (area.getRight(), area.getBottom());
        return p;
    }

    // ── the spectrogram ───────────────────────────────────────────────────
    // Level as heat: quiet fades through blue and cyan into yellow, loud
    // burns red - instant to read, which one hue at varying alpha never was.
    static juce::Colour heatColour (float t)
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        struct Stop { float at; juce::uint8 r, g, b; };
        static const Stop stops[] = {
            { 0.00f,   8,  10,  20 },     // near-black blue
            { 0.30f,  20,  60, 140 },     // deep blue
            { 0.55f,  20, 160, 180 },     // cyan
            { 0.75f, 235, 200,  60 },     // yellow
            { 1.00f, 240,  60,  40 },     // red
        };
        for (int i = 1; i < 5; ++i)
        {
            if (t <= stops[i].at)
            {
                const auto& a = stops[i - 1];
                const auto& b = stops[i];
                const float u = (t - a.at) / (b.at - a.at);
                return juce::Colour ((juce::uint8) (a.r + u * (b.r - a.r)),
                                     (juce::uint8) (a.g + u * (b.g - a.g)),
                                     (juce::uint8) (a.b + u * (b.b - a.b)));
            }
        }
        return juce::Colour (240, 60, 40);
    }

    void pushSpectrogramColumn (juce::Image& img, float floorDb,
                                const std::function<double (float y01)>& y01ToHz) const
    {
        if (! img.isValid()) return;
        const int w = img.getWidth(), h = img.getHeight();

        img.moveImageSection (0, 0, 1, 0, w - 1, h);

        for (int y = 0; y < h; ++y)
        {
            const double hz = y01ToHz ((float) y / (float) h);
            const float db = juce::jlimit (floorDb, 0.0f, dbAt (hz));
            const float t = juce::jmap (db, floorDb, 0.0f, 0.0f, 1.0f);
            img.setPixelAt (w - 1, y, heatColour (t).withAlpha (0.55f + 0.45f * t));
        }
    }

private:
    kbs::FFT fft;
    double sampleRate = 48000.0;
    std::vector<float> window, raw, binsDb, smoothDb, peakDb;
    std::vector<std::complex<float>> td;
    std::vector<double> avgSum, avgSq;
    std::vector<float> armHoldDb;
    int avgCount = 0;
    bool averaging = false;
    bool armHold = false;
};

} // namespace eqview
