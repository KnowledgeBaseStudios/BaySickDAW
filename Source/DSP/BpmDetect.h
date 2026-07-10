#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>

// QA-Ec G1-boundary (Jeff picks A/A, 2026-07-08): content tempo estimation.
// Onset-energy-flux -> comb-scored autocorrelation.  Used at import (a
// confident estimate becomes the clip's natural tempo identity instead of
// the project-tempo assumption) and by the per-clip Properties display.
// Message thread only (file I/O).  Loops and full-band material detect
// reliably; sparse/rubato material returns confident=false (callers fall
// back / display "not detectable").  The comb scoring is the octave-error
// guard: a true tempo's half and double lags also resonate.
namespace BpmDetect
{
    struct Estimate
    {
        double bpm       { 0.0 };
        bool   confident  { false };
    };

    inline Estimate detect (juce::AudioFormatReader& reader)
    {
        Estimate out;
        const double sr = reader.sampleRate;
        if (sr <= 0.0 || reader.lengthInSamples <= 0 || reader.numChannels == 0)
            return out;

        // Bounded analysis: up to 60 s, read in envelope-frame chunks so the
        // whole file never loads.  512-sample frames at 44.1k = ~86 Hz
        // envelope - plenty for 60-200 BPM periodicity.
        constexpr double kMaxAnalyzeSeconds = 60.0;
        constexpr int    kFrame             = 512;
        const juce::int64 total = juce::jmin (reader.lengthInSamples,
                                              (juce::int64) (sr * kMaxAnalyzeSeconds));
        const int nFrames = (int) (total / kFrame);
        if (nFrames < 172)   // < ~2 s of material: nothing to lock onto
            return out;

        // 1) per-frame log energy.
        std::vector<float> env ((size_t) nFrames, 0.0f);
        juce::AudioBuffer<float> chunk ((int) reader.numChannels, kFrame);
        for (int f = 0; f < nFrames; ++f)
        {
            reader.read (&chunk, 0, kFrame, (juce::int64) f * kFrame, true, true);
            double e = 0.0;
            for (int ch = 0; ch < chunk.getNumChannels(); ++ch)
            {
                const float* d = chunk.getReadPointer (ch);
                for (int i = 0; i < kFrame; ++i)
                    e += (double) d[i] * d[i];
            }
            env[(size_t) f] = std::log10 (1.0f + (float) e);
        }

        // 2) onset strength = half-wave-rectified energy flux, mean-removed
        //    so the autocorrelation measures periodicity, not DC.
        std::vector<float> flux ((size_t) nFrames, 0.0f);
        double mean = 0.0;
        for (int f = 1; f < nFrames; ++f)
        {
            flux[(size_t) f] = juce::jmax (0.0f, env[(size_t) f] - env[(size_t) f - 1]);
            mean += flux[(size_t) f];
        }
        mean /= (double) juce::jmax (1, nFrames - 1);
        for (auto& v : flux) v -= (float) mean;

        // 3) autocorrelation over the 60-200 BPM lag range (+ double-lag
        //    headroom for the comb), comb-scored: score(L) = r(L) + r(2L)/2
        //    + r(L/2)/3.
        const double fps    = sr / (double) kFrame;
        const int    minLag = juce::jmax (2, (int) std::floor (fps * 60.0 / 200.0));
        const int    maxLag = (int) std::ceil (fps * 60.0 / 60.0);
        if (maxLag * 2 + 1 >= nFrames) return out;

        std::vector<double> r ((size_t) (maxLag * 2 + 2), 0.0);
        for (int lag = juce::jmax (1, minLag / 2); lag <= maxLag * 2 + 1; ++lag)
        {
            double s = 0.0;
            for (int f = 0; f + lag < nFrames; ++f)
                s += (double) flux[(size_t) f] * flux[(size_t) (f + lag)];
            r[(size_t) lag] = s / (double) (nFrames - lag);
        }

        double bestScore = 0.0, meanScore = 0.0;
        int    bestLag = 0, scored = 0;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double s = r[(size_t) lag];
            if ((size_t) (lag * 2) < r.size()) s += 0.5  * r[(size_t) (lag * 2)];
            if (lag / 2 >= juce::jmax (1, minLag / 2)) s += 0.33 * r[(size_t) (lag / 2)];
            meanScore += s;
            ++scored;
            if (s > bestScore) { bestScore = s; bestLag = lag; }
        }
        if (bestLag <= 0 || scored == 0 || bestScore <= 0.0)
            return out;
        meanScore /= (double) scored;

        // 4) parabolic peak refinement -> sub-BPM precision (the .01 display).
        double lagF = (double) bestLag;
        if (bestLag > minLag && bestLag < maxLag)
        {
            const double y0 = r[(size_t) (bestLag - 1)];
            const double y1 = r[(size_t) bestLag];
            const double y2 = r[(size_t) (bestLag + 1)];
            const double den = y0 - 2.0 * y1 + y2;
            if (std::abs (den) > 1e-12)
                lagF += 0.5 * (y0 - y2) / den;
        }

        out.bpm = juce::jlimit (40.0, 260.0, 60.0 * fps / lagF);
        // Confidence: the winning comb peak must clearly dominate the field.
        // 2.0x-over-mean is a starting calibration - tune at the campaign if
        // real material over/under-triggers.
        out.confident = meanScore > 1e-12 && bestScore > 2.0 * meanScore;
        return out;
    }
}
