// KBS Plugins — the parametric EQ engine.
//
// BaySickDAW vendored copy (QA-EqPro, 2026-08-26): the take-back this header
// always promised.  Two extensions over the KBS original, both fed back as
// reference: per-domain linear phase (SC-3 - the C3 defect, which the first
// build of this engine did NOT fix despite the ledger's claim) and the
// four-slot per-band sidechain (SC-4).  Everything else is verbatim.
//
// Built from BaySickDAW's EQ8 with its twenty-six recorded defects fixed on
// the way in (BaySickDAW\Files For Claude\EQ Fixes.md is the ledger), the band
// count made variable, and the DAW types removed. This header is the reference
// implementation the DAW takes back when it is done; every claim it makes is
// pinned by a section of Tests/test_core.cpp, because the engine it replaces
// taught the lesson twice - the display said one thing, the sound did another,
// and nothing measured either.
//
// ── shape ──────────────────────────────────────────────────────────────────
//
// Up to 24 bands, each: type, freq, gain, Q, slope (filters only), channel
// routing (stereo / mid / side / left / right), stereo placement (gain types),
// and a full dynamics section. Global: processing mode (minimum-phase, natural
// = decramped bells, five linear-phase precisions), 2x oversampling for the
// IIR path, proportional Q, measured auto-gain, output trim, polarity.
//
// One rule holds everything together: the questions the UI asks - magnitude,
// phase, gain reduction - are answered by the same arithmetic that filters the
// audio. The engine this replaced kept a second copy of that arithmetic in the
// display, and four of its band types drifted until the picture was fiction.
// There is no second copy here to drift.
//
// ── threads ────────────────────────────────────────────────────────────────
//
// Single-threaded by design, like every Core device: setters mark state dirty
// and process() applies it at a safe point. The plugin wrapper owns
// publication (SpinLock + generation, as SuiteProcessor already does); nothing
// here allocates once prepare() has run, except a linear-phase mode change,
// which the wrapper treats as a configuration action.
#pragma once

#include "Devices.h"        // Biquad + RBJ designs, kDetectorFloor
#include "SVF.h"            // the TPT state-variable filter
#include "Oversampler.h"
#include "EqLinearPhase.h"

#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstring>
#include <vector>

namespace kbs {

// ── vocabulary ─────────────────────────────────────────────────────────────

enum class EqType
{
    bell = 0, lowPass, highPass, lowShelf, highShelf, notch, bandPass, tilt
};

// Slope is an index into this table, filters (LP / HP / BP) only. Brickwall is
// a linear-phase shape - an IIR cannot be one - so outside the linear modes it
// processes as 96 dB/oct and the editor says so.
inline constexpr int   kEqNumSlopes = 9;
inline constexpr float kEqSlopeDbPerOct[kEqNumSlopes] = { 6, 12, 18, 24, 36, 48, 72, 96, -1 };
inline constexpr int   kEqSlopeBrickwall = 8;

// Poles per slope. 6 dB/oct is one real pole; odd orders are a pole plus
// biquads; even orders are biquads at the Butterworth angles. The section Qs
// are computed from the angles rather than tabulated - the engine this
// replaced kept the table in three places and they did not agree.
inline constexpr int kEqSlopePoles[kEqNumSlopes] = { 1, 2, 3, 4, 6, 8, 12, 16, 16 };

enum class EqChannel { stereo = 0, mid, side, left, right };

enum class EqMode
{
    zeroLatency = 0,   // minimum-phase IIR, classic RBJ bells
    natural,           // same, bells decramped (Orfanidis) - still zero latency
    linearLow, linearMedium, linearHigh, linearVeryHigh, linearMaximum
};

inline bool eqModeIsLinear (EqMode m) { return (int) m >= (int) EqMode::linearLow; }

// FFT order per linear precision. Latency is hop + taps/2 = 3N/4; at 48 kHz
// that runs 16 ms (Low) to 192 ms (Maximum), each shown beside its menu entry.
inline int eqLinearFftOrder (EqMode m)
{
    switch (m)
    {
        case EqMode::linearLow:      return 10;   // 1024
        case EqMode::linearMedium:   return 11;
        case EqMode::linearHigh:     return 12;
        case EqMode::linearVeryHigh: return 13;
        case EqMode::linearMaximum:  return 14;   // 16384
        default:                     return 0;
    }
}

// ── one band, as the user has it ───────────────────────────────────────────
struct EqBandParams
{
    bool      on = false;
    EqType    type = EqType::bell;
    float     freqHz = 1000.0f;
    float     gainDb = 0.0f;              // gain types only
    float     q = 0.707f;
    int       slope = 1;                  // filters only; index into the table
    EqChannel channel = EqChannel::stereo;
    float     placement = 0.0f;           // -1 left .. +1 right; gain types, stereo only
    bool      muted = false;
    bool      isolated = false;           // audition this band's effect alone

    // Dynamics. Supported on the gain-bearing types (bell, shelves, tilt) and
    // notch. rangeDb is signed: negative compresses above threshold, positive
    // expands below it, zero means the section is inert.
    bool  dynamic = false;
    float thresholdDb = 0.0f;             // 0 = off until the user pulls it down
    float ratio = 2.0f;                   // 1..20
    float attackMs = 10.0f;               // 0.1..500
    float releaseMs = 100.0f;             // 1..2000
    bool  autoRelease = false;            // programme-dependent release
    float rangeDb = 0.0f;                 // -30..+30
    bool  scExternal = false;             // detect from the sidechain input
    int   scSource = -1;                  // QA-EqPro SC-4: DAW strip receive
                                          // slot 0..3; -1 = internal (or the
                                          // plugin's single bus via scExternal)
};

inline bool eqTypeHasGain (EqType t)
{
    return t == EqType::bell || t == EqType::lowShelf
        || t == EqType::highShelf || t == EqType::tilt;
}

inline bool eqTypeHasSlope (EqType t)
{
    return t == EqType::lowPass || t == EqType::highPass || t == EqType::bandPass;
}

inline bool eqTypeSupportsDynamic (EqType t)
{
    return eqTypeHasGain (t) || t == EqType::notch;
}

// ── filter designs the EQ needs beyond Devices.h ───────────────────────────
//
// RBJ cookbook second-order shelves with Q, notch, and constant-peak band
// pass; and the Orfanidis peaking design that holds its shape at Nyquist.
namespace eqdesign {

inline BiquadCoeffs shelf (double f0, double sr, double q, double gainDb, bool high)
{
    const double A  = std::pow (10.0, gainDb / 40.0);
    const double w  = 2.0 * kPi * f0 / sr;
    const double c  = std::cos (w), s = std::sin (w);
    const double al = s / (2.0 * std::clamp (q, 0.05, 20.0));
    const double p  = 2.0 * std::sqrt (A) * al;

    if (high)
    {
        const double a0 = (A + 1.0) - (A - 1.0) * c + p;
        return { (A * ((A + 1.0) + (A - 1.0) * c + p)) / a0,
                 (-2.0 * A * ((A - 1.0) + (A + 1.0) * c)) / a0,
                 (A * ((A + 1.0) + (A - 1.0) * c - p)) / a0,
                 (2.0 * ((A - 1.0) - (A + 1.0) * c)) / a0,
                 ((A + 1.0) - (A - 1.0) * c - p) / a0 };
    }

    const double a0 = (A + 1.0) + (A - 1.0) * c + p;
    return { (A * ((A + 1.0) - (A - 1.0) * c + p)) / a0,
             (2.0 * A * ((A - 1.0) - (A + 1.0) * c)) / a0,
             (A * ((A + 1.0) - (A - 1.0) * c - p)) / a0,
             (-2.0 * ((A - 1.0) + (A + 1.0) * c)) / a0,
             ((A + 1.0) + (A - 1.0) * c - p) / a0 };
}

inline BiquadCoeffs notch (double f0, double sr, double q)
{
    const double w  = 2.0 * kPi * f0 / sr;
    const double c  = std::cos (w);
    const double al = std::sin (w) / (2.0 * std::clamp (q, 0.05, 20.0));
    const double a0 = 1.0 + al;
    return { 1.0 / a0, -2.0 * c / a0, 1.0 / a0, -2.0 * c / a0, (1.0 - al) / a0 };
}

// Peak gain 0 dB at centre, skirts fall away - the "constant peak" form, so a
// cascade keeps its centre level while the skirts steepen.
inline BiquadCoeffs bandPass (double f0, double sr, double q)
{
    const double w  = 2.0 * kPi * f0 / sr;
    const double al = std::sin (w) / (2.0 * std::clamp (q, 0.05, 20.0));
    const double a0 = 1.0 + al;
    return { al / a0, 0.0, -al / a0, -2.0 * std::cos (w) / a0, (1.0 - al) / a0 };
}

// Orfanidis 1997: a peaking section whose digital magnitude matches the
// analogue prototype's value at Nyquist instead of pinching to unity there.
// This is what "Natural" mode means: a 10 kHz bell at 44.1 k keeps the shape
// it would have had on hardware, with no oversampling and no latency.
// G0 = 1, bandwidth gain at the geometric mean (GB^2 = G), per the paper.
inline BiquadCoeffs peakOrfanidis (double f0, double sr, double q, double gainDb)
{
    const double G = std::pow (10.0, gainDb / 20.0);
    if (std::abs (gainDb) < 1.0e-4) return { 1, 0, 0, 0, 0 };

    const double w0 = 2.0 * kPi * std::clamp (f0, 1.0, sr * 0.497) / sr;
    const double Dw = w0 / std::clamp (q, 0.05, 30.0);

    const double G2  = G * G;
    const double GB2 = G;                      // GB = sqrt(G): symmetric edges
    const double pi2 = kPi * kPi;

    const double F   = std::abs (G2 - GB2);
    const double G00 = std::abs (G2 - 1.0);
    const double F00 = std::abs (GB2 - 1.0);

    const double d   = (w0 * w0 - pi2) * (w0 * w0 - pi2);
    const double num = d + G2 * F00 * pi2 * Dw * Dw / F;
    const double den = d + F00 * pi2 * Dw * Dw / F;
    const double G1  = std::sqrt (num / den);   // prescribed Nyquist gain
    const double G12 = G1 * G1;

    const double G01 = std::abs (G2 - G1);
    const double G11 = std::abs (G2 - G12);
    const double F01 = std::abs (GB2 - G1);
    const double F11 = std::abs (GB2 - G12);

    const double t0 = std::tan (w0 / 2.0);
    const double W2 = std::sqrt (G11 / G00) * t0 * t0;
    const double DW = (1.0 + std::sqrt (F00 / F11) * W2) * std::tan (Dw / 2.0);

    const double C = F11 * DW * DW - 2.0 * W2 * (F01 - std::sqrt (F00 * F11));
    const double D = 2.0 * W2 * (G01 - std::sqrt (G00 * G11));
    const double A = std::sqrt (std::max (0.0, (C + D) / F));
    const double B = std::sqrt (std::max (0.0, (G2 * C + GB2 * D) / F));

    const double a0 = 1.0 + W2 + A;
    return { (G1 + W2 + B) / a0,      // G0 = 1 folded in
             (-2.0 * (G1 - W2)) / a0,
             (G1 - B + W2) / a0,
             (-2.0 * (1.0 - W2)) / a0,
             (1.0 + W2 - A) / a0 };
}

// |H(e^{jw})| and arg from biquad coefficients - the query path shares its
// arithmetic with the audio path by construction.
inline void biquadResponse (const BiquadCoeffs& c, double w, double& mag, double& ph)
{
    const std::complex<double> z1 = { std::cos (-w), std::sin (-w) };
    const std::complex<double> z2 = z1 * z1;
    const auto num = c.b0 + c.b1 * z1 + c.b2 * z2;
    const auto den = 1.0  + c.a1 * z1 + c.a2 * z2;
    const auto h = num / den;
    mag = std::abs (h);
    ph  = std::arg (h);
}

// Butterworth section Q for pair k (1-based) of a filter with n poles.
inline double butterworthQ (int poles, int pair)
{
    if (poles % 2 == 0)
        return 1.0 / (2.0 * std::cos ((2.0 * pair - 1.0) * kPi / (2.0 * poles)));
    return 1.0 / (2.0 * std::cos ((double) pair * kPi / (double) poles));
}

} // namespace eqdesign

// ── the engine ─────────────────────────────────────────────────────────────
class ParametricEq
{
public:
    static constexpr int kMaxBands = 24;

    // Dynamics update interval. The envelope runs per sample; the gain it
    // commands is applied by rebuilding the band's section this often, with a
    // short smoother between updates so the steps never reach the audio. The
    // engine this replaced applied it once per host block - 10.7 ms at 512 -
    // which quantised every attack the interface offered below that. 32
    // samples is 0.7 ms at 48 kHz: finer than the fastest audible attack.
    static constexpr int kDynChunk = 32;

    // ── configuration ─────────────────────────────────────────────────────
    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        maxBlock = std::max (32, maxBlockSize);

        for (auto& b : bands) b.prepareRuntime (sr);

        osL.prepare (maxBlock);
        osR.prepare (maxBlock);
        osBufL.assign ((size_t) (maxBlock * 2 + 8), 0.0f);
        osBufR.assign ((size_t) (maxBlock * 2 + 8), 0.0f);

        scL.assign ((size_t) maxBlock, 0.0f);
        scR.assign ((size_t) maxBlock, 0.0f);
        detL.assign ((size_t) maxBlock, 0.0f);
        detR.assign ((size_t) maxBlock, 0.0f);
        for (int s = 0; s < 4; ++s)
        {
            scSlotL[(size_t) s].assign ((size_t) maxBlock, 0.0f);
            scSlotR[(size_t) s].assign ((size_t) maxBlock, 0.0f);
            scSlotValid[(size_t) s] = false;
        }

        configureLinear();

        listenSvfL.prepare (sr);
        listenSvfR.prepare (sr);

        aAutoGain = std::exp (-1.0f / (float) (1.0 * sr));
        reset();
    }

    void reset()
    {
        for (auto& b : bands) b.resetRuntime();
        osL.reset();
        osR.reset();
        linear.resetStreams();
        agIn = agOut = 0.0f;
        autoGainLin = 1.0f;
        listenSvfL.reset();
        listenSvfR.reset();
        markAllDirty();
    }

    // ── parameters ────────────────────────────────────────────────────────
    void setBand (int i, const EqBandParams& p)
    {
        if (i < 0 || i >= kMaxBands) return;
        auto& b = bands[(size_t) i];

        // A structural change resets the band's filter memory; a value change
        // must not, or every drag would crackle. Structural = type, slope,
        // channel, or coming on.
        const bool structural = p.type != b.p.type || p.slope != b.p.slope
                             || p.channel != b.p.channel || (p.on && ! b.p.on);
        b.p = p;
        b.dirty = true;
        if (structural) b.resetRuntime();   // also snaps the glides to target
        staticCurveDirty = true;
    }

    EqBandParams getBand (int i) const
    {
        if (i < 0 || i >= kMaxBands) return {};
        return bands[(size_t) i].p;
    }

    void setMode (EqMode m)
    {
        if (m == mode) return;
        mode = m;
        configureLinear();
        markAllDirty();
    }
    EqMode getMode() const { return mode; }

    // 2x for the IIR path. Ignored while a linear mode is active: the FIR is
    // already free of cramping (it is designed on the digital grid), and
    // nesting it in a resampler would double its latency for nothing.
    void setOversampling (bool on)
    {
        if (on == oversampling) return;
        oversampling = on;
        osL.reset();
        osR.reset();
        markAllDirty();
    }
    bool getOversampling() const { return oversampling; }

    void setProportionalQ (bool on)
    {
        if (on == proportionalQ) return;
        proportionalQ = on;
        markAllDirty();
    }
    bool getProportionalQ() const { return proportionalQ; }

    // Measured auto-gain: the EQ watches its own in and out and rides the
    // output so the loudness holds - the same follower the compressors use
    // (Devices.h), with the same power-law amount, because a gain interpolates
    // honestly only in decibels.
    void setAutoGain (bool on, float amount01 = 1.0f)
    {
        autoGainOn = on;
        agExp = std::clamp (amount01, 0.0f, 1.0f);
        if (! on) autoGainLin = 1.0f;
    }
    bool getAutoGain() const { return autoGainOn; }
    float getAutoGainDb() const
    {
        return 20.0f * std::log10 (std::max (appliedAutoGain(), 1.0e-6f));
    }

    void setOutputGainDb (float db) { outGainDb = std::clamp (db, -24.0f, 24.0f); }
    void setPolarityFlip (bool flip) { polarityFlip = flip; }

    // A band auditioned on its own: the output becomes a band-pass at the
    // band's frequency and Q, which is also exactly what its dynamic detector
    // hears. -1 = nobody listening.
    // Written by the editor thread, read per block: atomic, and the one
    // engine setter that is deliberately safe to call from anywhere.
    void setListenBand (int i) { listenBand.store ((i >= 0 && i < kMaxBands) ? i : -1,
                                                   std::memory_order_relaxed); }
    int getListenBand() const { return listenBand.load (std::memory_order_relaxed); }

    // External sidechain for this block. Null = the bands detect their own
    // input. Copied, so the caller's buffer can be reused immediately.
    void setSidechain (const float* l, const float* r, int numSamples)
    {
        scValid = (l != nullptr && numSamples > 0);
        if (! scValid) return;
        const int nn = std::min (numSamples, (int) scL.size());
        std::memcpy (scL.data(), l, (size_t) nn * sizeof (float));
        std::memcpy (scR.data(), r != nullptr ? r : l, (size_t) nn * sizeof (float));
    }

    // One of the DAW strip's four receive lines for this block (QA-EqPro
    // SC-4).  Same one-block copied contract as setSidechain.
    void setSidechainSlot (int slot, const float* l, const float* r, int numSamples)
    {
        if (slot < 0 || slot >= 4) return;
        const bool valid = (l != nullptr && numSamples > 0);
        scSlotValid[(size_t) slot] = valid;
        if (! valid) return;
        auto& bl = scSlotL[(size_t) slot];
        auto& br = scSlotR[(size_t) slot];
        const int nn = std::min (numSamples, (int) bl.size());
        std::memcpy (bl.data(), l, (size_t) nn * sizeof (float));
        std::memcpy (br.data(), r != nullptr ? r : l, (size_t) nn * sizeof (float));
    }

    // ── reporting ─────────────────────────────────────────────────────────
    int latencySamples() const
    {
        if (eqModeIsLinear (mode)) return linear.latencySamples();
        return oversampling ? Oversampler<2>::latencySamples() : 0;
    }

    float bandGrDb (int i) const
    {
        if (i < 0 || i >= kMaxBands) return 0.0f;
        return bands[(size_t) i].grDbShown.load (std::memory_order_relaxed);
    }

    // ── the queries the display draws from ────────────────────────────────
    //
    // Same design functions as the audio path, evaluated at one frequency.
    // withDynamic folds each band's current gain reduction in, which is what
    // makes the drawn curve breathe; the static form is what the linear-phase
    // FIR is designed from.
    float magnitudeAt (float hz, int channel = 0, bool withDynamic = true) const
    {
        double m = 1.0;
        for (const auto& b : bands)
            if (bandActive (b)) m *= bandMagnitude (b, hz, channel, withDynamic);
        return (float) m * std::pow (10.0f, outGainDb / 20.0f);
    }

    // One band's own contribution, for the display's per-band fills. Same
    // arithmetic as the audio; a band that is off contributes unity.
    float bandMagnitudeAt (int i, float hz, int channel = 0, bool withDynamic = true) const
    {
        if (i < 0 || i >= kMaxBands) return 1.0f;
        const auto& b = bands[(size_t) i];
        if (! b.p.on || b.p.muted) return 1.0f;
        return (float) bandMagnitude (b, hz, channel, withDynamic);
    }

    // The band at its full dynamic extent: static gain plus the furthest
    // the gain computer can travel, |threshold| x ratio slope. This is the
    // dotted line the display draws, it moves with the THRESHOLD knob, and
    // the live curve cannot pass it because the same expression bounds the
    // gain computer.
    float bandExtentMagnitudeAt (int i, float hz) const
    {
        if (i < 0 || i >= kMaxBands) return 1.0f;
        const auto& b = bands[(size_t) i];
        if (! b.p.on || b.p.muted || ! b.p.dynamic) return 1.0f;

        const float slope = 1.0f - 1.0f / std::max (1.0f, b.p.ratio);
        const float extent = std::min (std::abs (b.p.thresholdDb) * slope,
                                       std::abs (b.p.rangeDb));
        const float extentGain = b.p.gainDb + (b.p.rangeDb > 0.0f ? extent : -extent);

        BiquadCoeffs cs[2];
        const float designSr = (float) ((oversampling && ! eqModeIsLinear (mode)) ? sr * 2.0 : sr);
        const double w = 2.0 * kPi * std::clamp ((double) hz, 1.0, sr * 0.499) / designSr;
        const int n = designBiquads (b.p, designSr, extentGain, cs,
                                     b.p.freqHz, b.p.q);
        double mag = 1.0, ph = 0.0, tot = 1.0;
        for (int k = 0; k < n; ++k)
        {
            eqdesign::biquadResponse (cs[k], w, mag, ph);
            tot *= mag;
        }
        return (float) tot;
    }

    // Radians at hz. Zero across the board in linear modes - after the host
    // compensates the reported delay, that is the truth, and drawing the IIR
    // phase there was one of the recorded defects.
    float phaseAt (float hz) const
    {
        if (eqModeIsLinear (mode)) return 0.0f;
        double ph = 0.0;
        for (const auto& b : bands)
            if (bandActive (b)) ph += bandPhase (b, hz);
        if (polarityFlip) ph += kPi;
        return (float) ph;
    }

    // ── audio ─────────────────────────────────────────────────────────────
    void process (float* l, float* r, int numSamples)
    {
        if (numSamples <= 0) return;

        // The detectors hear the block as it arrived, whatever the bands do
        // to it afterwards - the parallel model, so a band's own reduction
        // never starves its own detector.
        const int nDet = std::min (numSamples, (int) detL.size());
        std::memcpy (detL.data(), l, (size_t) nDet * sizeof (float));
        std::memcpy (detR.data(), r, (size_t) nDet * sizeof (float));

        anyIsolated = false;
        for (auto& b : bands)
            if (b.p.on && b.p.isolated) { anyIsolated = true; break; }

        const bool linearMode = eqModeIsLinear (mode);

        if (linearMode && staticCurveDirty)
        {
            rebuildLinearCurve();
            staticCurveDirty = false;
        }

        // Auto-gain measures the input before anything touches it.
        if (autoGainOn)
            for (int i = 0; i < numSamples; ++i)
            {
                const float pk = std::max (std::abs (l[i]), std::abs (r[i]));
                agIn = aAutoGain * agIn + (1.0f - aAutoGain) * pk * pk;
            }

        if (linearMode)
        {
            // Static curve through the FIR; each dynamic band contributes only
            // its moving part as a minimum-phase section afterwards. The
            // static half of a dynamic band lives in the FIR with everything
            // else, so the reduction rides on a linear-phase bed - the same
            // split Pro-Q makes, because a gain that moves cannot be
            // linear-phase without lookahead artefacts.
            linear.processStereo (l, r, numSamples);
            processBands (l, r, numSamples, (float) sr, true);
        }
        else if (oversampling)
        {
            // One resampler per channel: they carry history, and the engine
            // this replaced had exactly this class of shared-state fault.
            const int nOs = osL.upsample (l, numSamples);
            std::memcpy (osBufL.data(), osL.upsampleBuffer(), (size_t) nOs * sizeof (float));
            osR.upsample (r, numSamples);
            std::memcpy (osBufR.data(), osR.upsampleBuffer(), (size_t) nOs * sizeof (float));

            processBands (osBufL.data(), osBufR.data(), nOs, (float) (sr * 2.0), false);

            osL.downsample (osBufL.data(), l, numSamples);
            osR.downsample (osBufR.data(), r, numSamples);
        }
        else
        {
            processBands (l, r, numSamples, (float) sr, false);
        }

        // Listen replaces the output with the band's own slice of the input.
        const int lb = listenBand.load (std::memory_order_relaxed);
        if (lb >= 0 && bands[(size_t) lb].p.on)
        {
            auto& b = bands[(size_t) lb];
            listenSvfL.set (std::clamp (b.p.freqHz, 20.0f, (float) (sr * 0.45)),
                            std::clamp (b.p.q, 0.5f, 20.0f));
            listenSvfR.set (std::clamp (b.p.freqHz, 20.0f, (float) (sr * 0.45)),
                            std::clamp (b.p.q, 0.5f, 20.0f));
            for (int i = 0; i < nDet; ++i)
            {
                l[i] = listenSvfL.process (detL[(size_t) i]).bp;
                r[i] = listenSvfR.process (detR[(size_t) i]).bp;
            }
        }

        // Output stage: measured auto-gain, trim, polarity.
        float outLin = std::pow (10.0f, outGainDb / 20.0f);
        if (polarityFlip) outLin = -outLin;

        if (autoGainOn)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const float pk = std::max (std::abs (l[i]), std::abs (r[i]));
                agOut = aAutoGain * agOut + (1.0f - aAutoGain) * pk * pk;
            }
            if (agOut > 1.0e-12f && agIn > 1.0e-12f)
            {
                const float want = std::sqrt (agIn / agOut);
                autoGainLin = std::min (4.0f, std::max (0.25f, want));
            }
            outLin *= appliedAutoGain();
        }

        for (int i = 0; i < numSamples; ++i) { l[i] *= outLin; r[i] *= outLin; }

        scValid = false;   // a sidechain is one block's worth of truth
        scSlotValid.fill (false);
    }

private:
    // ── per-band runtime ──────────────────────────────────────────────────
    struct BandRt
    {
        EqBandParams p;

        // Biquad path: bell, shelves, notch, tilt. Two sections at most
        // (tilt); separate L/R because placement can split their gains.
        Biquad biqL[2], biqR[2];
        BiquadCoeffs coefL[2] {}, coefR[2] {};
        int biqCount = 0;

        // Filter path: LP / HP / BP as TPT sections plus an optional real
        // pole for the odd orders.
        StateVariableFilter svfL[8], svfR[8];
        float poleStateL = 0.0f, poleStateR = 0.0f;
        float poleG = 0.0f;
        int svfCount = 0;
        bool hasPole = false;
        // The SVF's raw bp output peaks at Q; multiplying by k = 1/Q holds the
        // centre at unity, which is what the magnitude query promises. Stored
        // here so the audio applies exactly the k the query divides by.
        float bpK = 1.0f;

        // Dynamics.
        Biquad detFL, detFR;
        float envL = 0.0f, envR = 0.0f;
        float grDb = 0.0f;          // target from the gain computer
        float grSmooth = 0.0f;      // what the filter actually carries
        float overSec = 0.0f;       // how long the signal has been over - auto release
        std::atomic<float> grDbShown { 0.0f };
        float lastBuiltGr = 1.0e9f;

        bool dirty = true;

        // Glided parameter state: what the filters are actually built from.
        // Targets live in p; these walk toward them one dynamics chunk at a
        // time (about 0.7 ms), so a host automating Freq sweeps instead of
        // stepping at block rate - the DAW's block-rate defect, from the
        // parameter side. Structural changes snap them.
        float cFreq = 1000.0f, cGain = 0.0f, cQ = 0.707f;

        void prepareRuntime (double sampleRate)
        {
            for (auto& s : svfL) s.prepare (sampleRate);
            for (auto& s : svfR) s.prepare (sampleRate);
            resetRuntime();
        }

        void resetRuntime()
        {
            for (auto& f : biqL) f.reset();
            for (auto& f : biqR) f.reset();
            for (auto& s : svfL) s.reset();
            for (auto& s : svfR) s.reset();
            poleStateL = poleStateR = 0.0f;
            detFL.reset(); detFR.reset();
            envL = envR = 0.0f;
            grDb = grSmooth = 0.0f;
            overSec = 0.0f;
            grDbShown.store (0.0f, std::memory_order_relaxed);
            lastBuiltGr = 1.0e9f;
            cFreq = p.freqHz; cGain = p.gainDb; cQ = p.q;
            dirty = true;
        }
    };

    bool bandActive (const BandRt& b) const
    {
        if (! b.p.on || b.p.muted) return false;
        if (anyIsolated && ! b.p.isolated) return false;
        return true;
    }

    // Placement splits a gain band's dB across the channels: full weight on
    // its own side, fading on the other. Gain types only - a filter has no
    // gain to split, which is why the editor hides placement there.
    void placementWeights (const EqBandParams& p, float& wl, float& wr) const
    {
        wl = wr = 1.0f;
        if (p.channel != EqChannel::stereo) return;
        const float pl = std::clamp (p.placement, -1.0f, 1.0f);
        if (pl > 0.0f) wl = 1.0f - pl;
        else if (pl < 0.0f) wr = 1.0f + pl;
    }

    // ── section design ────────────────────────────────────────────────────
    //
    // One function designs a band's sections; the audio path installs them,
    // the query path evaluates them, the FIR designer samples them. gainDb is
    // taken as an argument rather than read from the params so the dynamic
    // rebuild and the static design go through the identical arithmetic.
    int designBiquads (const EqBandParams& p, float designSr, float gainDb,
                       BiquadCoeffs* out,
                       float freqOverride = -1.0f, float qOverride = -1.0f) const
    {
        const float fRaw = freqOverride > 0.0f ? freqOverride : p.freqHz;
        const float qRaw = qOverride > 0.0f ? qOverride : p.q;
        const float f = std::clamp (fRaw, 20.0f, designSr * 0.497f);
        float qEff = qRaw;
        if (proportionalQ && p.type == EqType::bell)
            qEff = qRaw * (1.0f + std::abs (gainDb) / 18.0f);
        const float q = std::clamp (qEff, 0.05f, 30.0f);

        switch (p.type)
        {
            case EqType::bell:
                out[0] = (mode == EqMode::zeroLatency)
                           ? Biquad::peaking (f, q, gainDb, designSr)
                           : eqdesign::peakOrfanidis (f, designSr, q, gainDb);
                return 1;

            case EqType::lowShelf:
                out[0] = eqdesign::shelf (f, designSr, q, gainDb, false);
                return 1;

            case EqType::highShelf:
                out[0] = eqdesign::shelf (f, designSr, q, gainDb, true);
                return 1;

            case EqType::notch:
                // A dynamic notch is a de-resonator: at rest it does nothing,
                // and the detector cuts it in by up to range. That is a bell
                // driven by GR, not a full notch that blinks - a filter to the
                // floor appearing per-syllable would be an effect, not an EQ.
                if (p.dynamic)
                {
                    out[0] = (mode == EqMode::zeroLatency)
                               ? Biquad::peaking (f, q, gainDb, designSr)
                               : eqdesign::peakOrfanidis (f, designSr, q, gainDb);
                    return 1;
                }
                out[0] = eqdesign::notch (f, designSr, q);
                return 1;

            case EqType::tilt:
                out[0] = eqdesign::shelf (f, designSr, q, +gainDb, false);
                out[1] = eqdesign::shelf (f, designSr, q, -gainDb, true);
                return 2;

            default: return 0;   // LP/HP/BP live on the SVF path
        }
    }

    void rebuildBand (BandRt& b, float designSr)
    {
        const auto& p = b.p;

        if (eqTypeHasSlope (p.type))
        {
            const int slope = std::clamp (p.slope, 0, kEqNumSlopes - 1);
            const int poles = kEqSlopePoles[slope == kEqSlopeBrickwall
                                            && ! eqModeIsLinear (mode)
                                              ? kEqSlopeBrickwall - 1 : slope];
            const float f = std::clamp (b.cFreq, 20.0f, designSr * 0.45f);

            if (p.type == EqType::bandPass)
            {
                // Cascaded constant-peak sections at the user's Q: centre
                // stays put, skirts steepen with the slope. Minimum one
                // full section - a 6 dB band pass is not a thing.
                b.svfCount = std::max (1, poles / 2);
                b.hasPole = false;
                b.bpK = 1.0f / std::clamp (b.cQ, 0.35f, 24.0f);
                for (int s = 0; s < b.svfCount; ++s)
                {
                    b.svfL[s].set (f, b.cQ);
                    b.svfR[s].set (f, b.cQ);
                }
            }
            else
            {
                b.hasPole = (poles % 2) == 1;
                b.svfCount = poles / 2;
                b.poleG = std::tan ((float) kPi * f / designSr);

                for (int s = 0; s < b.svfCount; ++s)
                {
                    // Slope 12 with one section takes the user's Q - that is
                    // the resonant filter everybody expects; the cascades take
                    // the Butterworth Qs so the knee stays maximally flat.
                    const double bq = (poles == 2)
                                        ? (double) std::clamp (b.cQ, 0.35f, 24.0f)
                                        : eqdesign::butterworthQ (poles, s + 1);
                    b.svfL[s].set (f, (float) bq);
                    b.svfR[s].set (f, (float) bq);
                }
            }
            b.biqCount = 0;
            return;
        }

        float wl, wr;
        placementWeights (p, wl, wr);
        const float g = b.cGain + (p.dynamic ? b.grSmooth : 0.0f);

        b.biqCount = designBiquads (p, designSr, g * wl, b.coefL, b.cFreq, b.cQ);
        if (wl == wr)
        {
            for (int s = 0; s < b.biqCount; ++s) b.coefR[s] = b.coefL[s];
        }
        else
        {
            designBiquads (p, designSr, g * wr, b.coefR, b.cFreq, b.cQ);
        }
        for (int s = 0; s < b.biqCount; ++s)
        {
            b.biqL[s].setCoeffs (b.coefL[s]);
            b.biqR[s].setCoeffs (b.coefR[s]);
        }
        b.svfCount = 0;
        b.hasPole = false;
        b.lastBuiltGr = p.dynamic ? b.grSmooth : 0.0f;
    }

    // In linear modes a dynamic band's IIR carries only the moving part; its
    // static gain is in the FIR.
    void rebuildDynamicDelta (BandRt& b, float designSr)
    {
        b.biqCount = designBiquads (b.p, designSr, b.grSmooth, b.coefL, b.cFreq, b.cQ);
        for (int s = 0; s < b.biqCount; ++s)
        {
            b.coefR[s] = b.coefL[s];
            b.biqL[s].setCoeffs (b.coefL[s]);
            b.biqR[s].setCoeffs (b.coefR[s]);
        }
        b.svfCount = 0;
        b.hasPole = false;
        b.lastBuiltGr = b.grSmooth;
    }

    // ── dynamics ──────────────────────────────────────────────────────────
    void advanceDynamics (BandRt& b, int from, int to)
    {
        const auto& p = b.p;
        const float hostSr = (float) sr;

        const float attMs = std::max (0.1f, p.attackMs);
        float relMs = std::max (1.0f, p.releaseMs);

        // Programme-dependent release: transient overs let go fast, material
        // that sits over the threshold gets a long, unobtrusive tail. The
        // over-time follower is the programme sensor.
        if (p.autoRelease)
            relMs = 60.0f + 440.0f * std::clamp (b.overSec / 0.4f, 0.0f, 1.0f);

        const float aAtt = std::exp (-1.0f / (0.001f * attMs * hostSr));
        const float aRel = std::exp (-1.0f / (0.001f * relMs * hostSr));

        // Detector source: a picked strip receive line first (QA-EqPro SC-4),
        // then the plugin's single sidechain bus, then the band's own input.
        const float* dl = detL.data();
        const float* dr = detR.data();
        if (p.scSource >= 0 && p.scSource < 4 && scSlotValid[(size_t) p.scSource])
        {
            dl = scSlotL[(size_t) p.scSource].data();
            dr = scSlotR[(size_t) p.scSource].data();
        }
        else if (p.scExternal && scValid)
        {
            dl = scL.data();
            dr = scR.data();
        }

        float el = b.envL, er = b.envR;
        for (int n = from; n < to; ++n)
        {
            float inL, inR;
            switch (p.channel)
            {
                case EqChannel::mid:   inL = inR = 0.5f * (dl[n] + dr[n]); break;
                case EqChannel::side:  inL = inR = 0.5f * (dl[n] - dr[n]); break;
                case EqChannel::left:  inL = dl[n]; inR = 0.0f; break;
                case EqChannel::right: inL = 0.0f;  inR = dr[n]; break;
                default:               inL = dl[n]; inR = dr[n]; break;
            }
            const float vL = std::abs (b.detFL.process (inL));
            const float vR = std::abs (b.detFR.process (inR));
            el = (vL > el ? aAtt : aRel) * el + (1.0f - (vL > el ? aAtt : aRel)) * vL;
            er = (vR > er ? aAtt : aRel) * er + (1.0f - (vR > er ? aAtt : aRel)) * vR;
        }
        b.envL = el; b.envR = er;

        const float env = std::max (el, er);

        // Silence gate: below the floor there is nothing to react to, and a
        // relative threshold read against silence is how the old meters came
        // to follow the knob instead of the audio.
        //
        // Both directions use the same unified over-threshold trigger: the
        // band moves - down for compression, up for expansion - when the
        // detector EXCEEDS the threshold, by the excess times the ratio
        // slope. That gives the semantics the product spec (Jeff, test pass
        // five) states outright: threshold at 0 dB means nothing ever
        // engages; threshold at -60 means everything does, pinned. The
        // travel can never exceed |threshold| x slope, which is exactly the
        // dotted extent line the display draws - the maths cannot cross it,
        // so neither can the curve. The first build expanded BELOW the
        // threshold, which inverted the knob for the UP direction and let
        // the curve sail past its own extent.
        float grDb = 0.0f;
        if (env >= kDetectorFloor && std::abs (p.rangeDb) > 0.0f)
        {
            const float envDb = 20.0f * std::log10 (env);
            const float ratio = std::max (1.0f, p.ratio);
            const float slope = 1.0f - 1.0f / ratio;
            const float over = envDb - p.thresholdDb;

            if (over > 0.0f)
            {
                const float extent = std::min (std::abs (p.thresholdDb) * slope,
                                               std::abs (p.rangeDb));
                const float mag = std::min (over * slope, extent);
                grDb = p.rangeDb > 0.0f ? mag : -mag;
                b.overSec = std::min (2.0f, b.overSec + (float) (to - from) / (float) sr);
            }
            else
                b.overSec = std::max (0.0f, b.overSec - (float) (to - from) / (float) sr);
        }
        else
        {
            b.overSec = std::max (0.0f, b.overSec - (float) (to - from) / (float) sr);
        }

        b.grDb = grDb;

        // The chunk smoother: what actually reaches the coefficients. About
        // 1.5 ms, so a chunk-to-chunk step of several dB arrives as a ramp.
        const float aChunk = std::exp (-(float) (to - from) / (0.0015f * hostSr));
        b.grSmooth = grDb + aChunk * (b.grSmooth - grDb);
        b.grDbShown.store (b.grSmooth, std::memory_order_relaxed);
    }

    void updateDetector (BandRt& b)
    {
        const float f = std::clamp (b.p.freqHz, 20.0f, (float) (sr * 0.45));
        const float q = std::clamp (b.p.q, 0.3f, 20.0f);
        const auto c = eqdesign::bandPass (f, sr, q);
        b.detFL.setCoeffs (c);
        b.detFR.setCoeffs (c);
    }

    // ── the band loop ─────────────────────────────────────────────────────
    void processBands (float* l, float* r, int numSamples, float designSr,
                       bool dynamicDeltaOnly)
    {
        // Rebuilds first: parameter edits, then dynamics per chunk.
        for (auto& b : bands)
        {
            if (! b.p.on) continue;
            if (b.dirty)
            {
                if (b.p.dynamic && eqTypeSupportsDynamic (b.p.type))
                    updateDetector (b);
                if (dynamicDeltaOnly)
                {
                    if (b.p.dynamic && eqTypeSupportsDynamic (b.p.type))
                        rebuildDynamicDelta (b, designSr);
                    else
                        b.biqCount = b.svfCount = 0;   // static part is in the FIR
                }
                else
                {
                    rebuildBand (b, designSr);
                }
                b.dirty = false;
            }
        }

        // The oversampled path hands us 2x the samples; dynamics stay at the
        // host rate on the detector snapshot, so the chunk boundary in host
        // samples maps to 2x here.
        const int rateFactor = std::max (1, (int) std::lround (designSr / (float) sr));

        for (int start = 0; start < numSamples; start += kDynChunk * rateFactor)
        {
            const int end = std::min (numSamples, start + kDynChunk * rateFactor);
            const int hostFrom = start / rateFactor;
            const int hostTo   = std::max (hostFrom + 1, end / rateFactor);

            // About 15 ms to target, advanced once per chunk.
            const float aGlide = 1.0f - std::exp (-(float) kDynChunk / (0.015f * (float) sr));

            for (auto& b : bands)
            {
                if (! bandActive (b)) continue;

                bool moved = false;
                const bool gliding = std::abs (b.cFreq - b.p.freqHz) > 1.0e-3f * b.p.freqHz
                                  || std::abs (b.cGain - b.p.gainDb) > 1.0e-3f
                                  || std::abs (b.cQ - b.p.q) > 1.0e-4f;
                if (gliding)
                {
                    b.cFreq += (b.p.freqHz - b.cFreq) * aGlide;
                    b.cGain += (b.p.gainDb - b.cGain) * aGlide;
                    b.cQ    += (b.p.q      - b.cQ)    * aGlide;
                    moved = true;
                }

                if (b.p.dynamic && eqTypeSupportsDynamic (b.p.type))
                {
                    advanceDynamics (b, hostFrom, std::min (hostTo, (int) detL.size()));
                    if (std::abs (b.grSmooth - b.lastBuiltGr) > 0.005f) moved = true;
                }

                if (moved)
                {
                    if (dynamicDeltaOnly) rebuildDynamicDelta (b, designSr);
                    else                  rebuildBand (b, designSr);
                }
            }

            for (int n = start; n < end; ++n)
            {
                float xl = l[n], xr = r[n];

                for (auto& b : bands)
                {
                    if (! bandActive (b)) continue;
                    if (b.biqCount == 0 && b.svfCount == 0 && ! b.hasPole) continue;

                    // Channel routing, per band, in place.
                    float m = 0.0f, s = 0.0f;
                    const auto ch = b.p.channel;
                    const bool ms = (ch == EqChannel::mid || ch == EqChannel::side);
                    if (ms)
                    {
                        m = 0.5f * (xl + xr);
                        s = 0.5f * (xl - xr);
                    }

                    float aIn  = ms ? (ch == EqChannel::mid ? m : s)
                               : (ch == EqChannel::right ? xr : xl);
                    float bIn  = (ch == EqChannel::stereo) ? xr : 0.0f;

                    float aOut = bandSample (b, aIn, 0);
                    float bOut = (ch == EqChannel::stereo) ? bandSample (b, bIn, 1) : 0.0f;

                    switch (ch)
                    {
                        case EqChannel::stereo: xl = aOut; xr = bOut; break;
                        case EqChannel::mid:    xl = aOut + s; xr = aOut - s; break;
                        case EqChannel::side:   xl = m + aOut; xr = m - aOut; break;
                        case EqChannel::left:   xl = aOut; break;
                        case EqChannel::right:  xr = aOut; break;
                    }
                }

                l[n] = xl; r[n] = xr;
            }
        }
    }

    inline float bandSample (BandRt& b, float x, int chan) noexcept
    {
        if (b.svfCount > 0 || b.hasPole)
        {
            // One-pole first on odd orders, then the SVF cascade, taking the
            // response that matches the band's type.
            if (b.hasPole)
            {
                float& s0 = chan == 0 ? b.poleStateL : b.poleStateR;
                const float G = b.poleG / (1.0f + b.poleG);
                const float v = (x - s0) * G;
                const float lp = v + s0;
                s0 = lp + v;
                x = (b.p.type == EqType::highPass) ? x - lp : lp;
            }
            auto* svf = chan == 0 ? b.svfL : b.svfR;
            for (int i = 0; i < b.svfCount; ++i)
            {
                const auto o = svf[i].process (x);
                x = b.p.type == EqType::lowPass  ? o.lp
                  : b.p.type == EqType::highPass ? o.hp
                                                 : o.bp * b.bpK;
            }
            return x;
        }

        auto* biq = chan == 0 ? b.biqL : b.biqR;
        for (int i = 0; i < b.biqCount; ++i) x = biq[i].process (x);
        return x;
    }

    // ── queries share the design ──────────────────────────────────────────
    double bandMagnitude (const BandRt& b, float hz, int channel, bool withDynamic) const
    {
        const float designSr = (float) ((oversampling && ! eqModeIsLinear (mode)) ? sr * 2.0 : sr);
        const double w = 2.0 * kPi * std::clamp ((double) hz, 1.0, sr * 0.499) / designSr;
        const auto& p = b.p;

        if (eqTypeHasSlope (p.type))
        {
            const int slope = std::clamp (p.slope, 0, kEqNumSlopes - 1);
            if (slope == kEqSlopeBrickwall && eqModeIsLinear (mode))
                return hzInsideBrickwall (p, hz) ? 1.0 : 1.0e-6;

            const int poles = kEqSlopePoles[slope == kEqSlopeBrickwall ? kEqSlopeBrickwall - 1 : slope];
            const double fc = std::clamp ((double) p.freqHz, 20.0, designSr * 0.45);
            const double Om = std::tan (w / 2.0) / std::tan (kPi * fc / designSr);
            double mag = 1.0;

            if (p.type == EqType::bandPass)
            {
                const int n = std::max (1, poles / 2);
                const double k = 1.0 / std::clamp ((double) p.q, 0.35, 24.0);
                for (int i = 0; i < n; ++i)
                    mag *= (k * Om) / std::sqrt (sq (1.0 - Om * Om) + sq (k * Om));
                return mag;
            }

            const bool hp = p.type == EqType::highPass;
            if ((poles % 2) == 1)
            {
                const double one = 1.0 / std::sqrt (1.0 + Om * Om);
                mag *= hp ? Om * one : one;
            }
            const int pairs = poles / 2;
            for (int i = 1; i <= pairs; ++i)
            {
                const double k = (poles == 2)
                                   ? 1.0 / std::clamp ((double) p.q, 0.35, 24.0)
                                   : 1.0 / eqdesign::butterworthQ (poles, i);
                const double den = std::sqrt (sq (1.0 - Om * Om) + sq (k * Om));
                mag *= (hp ? Om * Om : 1.0) / den;
            }
            return mag;
        }

        // Biquad types: build the same coefficients the audio built -
        // except placement. PAN splits the AUDIO across the channels; the
        // line the display draws is the band's nominal response and does not
        // move with the PAN knob. A curve that bent when you panned was
        // indistinguishable from dynamics doing it (test pass six).
        (void) channel;
        float g = p.gainDb;
        if (withDynamic && p.dynamic && eqTypeSupportsDynamic (p.type))
            g += b.grSmooth;

        BiquadCoeffs cs[2];
        const int n = designBiquads (p, designSr, g, cs);
        double mag = 1.0, ph = 0.0, mTot = 1.0;
        for (int i = 0; i < n; ++i)
        {
            eqdesign::biquadResponse (cs[i], w, mag, ph);
            mTot *= mag;
        }
        return mTot;
    }

    double bandPhase (const BandRt& b, float hz) const
    {
        const float designSr = (float) (oversampling ? sr * 2.0 : sr);
        const double w = 2.0 * kPi * std::clamp ((double) hz, 1.0, sr * 0.499) / designSr;
        const auto& p = b.p;

        if (eqTypeHasSlope (p.type))
        {
            const int slope = std::clamp (p.slope, 0, kEqNumSlopes - 1);
            const int poles = kEqSlopePoles[slope == kEqSlopeBrickwall ? kEqSlopeBrickwall - 1 : slope];
            const double fc = std::clamp ((double) p.freqHz, 20.0, designSr * 0.45);
            const double Om = std::tan (w / 2.0) / std::tan (kPi * fc / designSr);
            double ph = 0.0;

            const bool hp = p.type == EqType::highPass;
            const bool bp = p.type == EqType::bandPass;
            if ((poles % 2) == 1 && ! bp)
            {
                // 1 / (1 + jOm), numerator jOm for HP.
                ph += (hp ? kPi / 2.0 : 0.0) - std::atan2 (Om, 1.0);
            }
            const int pairs = bp ? std::max (1, poles / 2) : poles / 2;
            for (int i = 1; i <= pairs; ++i)
            {
                const double k = (poles == 2 || bp)
                                   ? 1.0 / std::clamp ((double) p.q, 0.35, 24.0)
                                   : 1.0 / eqdesign::butterworthQ (poles, i);
                const double num = bp ? kPi / 2.0 : (hp ? kPi : 0.0);
                ph += num - std::atan2 (k * Om, 1.0 - Om * Om);
            }
            return ph;
        }

        float wl, wr;
        placementWeights (p, wl, wr);
        BiquadCoeffs cs[2];
        const int n = designBiquads (p, designSr, p.gainDb * std::max (wl, wr), cs);
        double mag = 1.0, phs = 0.0, tot = 0.0;
        for (int i = 0; i < n; ++i)
        {
            eqdesign::biquadResponse (cs[i], w, mag, phs);
            tot += phs;
        }
        return tot;
    }

    bool hzInsideBrickwall (const EqBandParams& p, float hz) const
    {
        if (p.type == EqType::lowPass)  return hz <= p.freqHz;
        if (p.type == EqType::highPass) return hz >= p.freqHz;
        const float half = p.freqHz / std::max (0.35f, p.q) * 0.5f;
        return std::abs (hz - p.freqHz) <= half;
    }

    // ── linear plumbing ───────────────────────────────────────────────────
    void configureLinear()
    {
        if (! eqModeIsLinear (mode)) return;
        linear.prepare (eqLinearFftOrder (mode), 2);
        // Domain-product tables for the matrix rebuild: sized here because
        // rebuilds run on the audio thread and must not allocate.
        const size_t bins = (size_t) (linear.fftSize() / 2 + 1);
        for (auto* t : { &domSt, &domL, &domR, &domM, &domS })
            t->assign (bins, 1.0f);
        staticCurveDirty = true;
    }

    void rebuildLinearCurve()
    {
        // The FIR carries the static curve of every active band - including
        // the static half of dynamic bands - and never their gain reduction.
        //
        // QA-EqPro SC-3 (the C3 defect, unfixed in the first build of this
        // engine): per-band domain routing must survive into the linear path.
        // With only stereo bands one shared FIR is exact and cheap; the moment
        // any active band routes mid / side / left / right, the response is a
        // 2x2 matrix in the L/R domain and the engine designs all four
        // entries.  Composition order is fixed as L/R-domain first, then the
        // M/S stage - band index order cannot be honoured across the two
        // groups by a single frequency-domain design, and either order is a
        // valid reading of "these bands each process their domain".
        bool anyDomain = false;
        for (const auto& b : bands)
            if (bandActive (b) && b.p.channel != EqChannel::stereo) { anyDomain = true; break; }

        if (! anyDomain)
        {
            linear.setMagnitude ([this] (float hz)
            {
                double m = 1.0;
                for (const auto& b : bands)
                    if (bandActive (b))
                        m *= bandMagnitude (b, hz, 0, false);
                return (float) m;
            }, sr);
            return;
        }

        const int n = linear.fftSize();
        const double binHz = sr / (double) n;
        for (int k = 0; k <= n / 2; ++k)
        {
            const float hz = (float) (k * binHz);
            double st = 1.0, dl = 1.0, dr = 1.0, dm = 1.0, ds = 1.0;
            for (const auto& b : bands)
            {
                if (! bandActive (b)) continue;
                const double m = bandMagnitude (b, hz, 0, false);
                switch (b.p.channel)
                {
                    case EqChannel::stereo: st *= m; break;
                    case EqChannel::left:   dl *= m; break;
                    case EqChannel::right:  dr *= m; break;
                    case EqChannel::mid:    dm *= m; break;
                    case EqChannel::side:   ds *= m; break;
                }
            }
            domSt[(size_t) k] = (float) st;
            domL [(size_t) k] = (float) dl;
            domR [(size_t) k] = (float) dr;
            domM [(size_t) k] = (float) dm;
            domS [(size_t) k] = (float) ds;
        }

        auto entry = [this] (float msSign, const std::vector<float>& side)
        {
            return [this, msSign, &side] (int bin, int) -> float
            {
                const auto k = (size_t) bin;
                return 0.5f * (domM[k] + msSign * domS[k]) * domSt[k] * side[k];
            };
        };
        linear.setMagnitudeMatrix (entry (+1.0f, domL),    // LL
                                   entry (-1.0f, domR),    // LR
                                   entry (-1.0f, domL),    // RL
                                   entry (+1.0f, domR));   // RR
    }

    void markAllDirty()
    {
        for (auto& b : bands) b.dirty = true;
        staticCurveDirty = true;
    }

    float appliedAutoGain() const
    {
        if (! autoGainOn) return 1.0f;
        if (agExp >= 0.9999f) return autoGainLin;
        if (agExp <= 0.0f) return 1.0f;
        return std::pow (autoGainLin, agExp);
    }

    static double sq (double x) { return x * x; }

    // ── state ─────────────────────────────────────────────────────────────
    std::array<BandRt, kMaxBands> bands;

    double sr = 48000.0;
    int maxBlock = 512;

    EqMode mode = EqMode::zeroLatency;
    bool oversampling = false;
    bool proportionalQ = true;
    bool autoGainOn = false;
    float agExp = 1.0f;
    float outGainDb = 0.0f;
    bool polarityFlip = false;
    std::atomic<int> listenBand { -1 };
    bool anyIsolated = false;

    Oversampler<2> osL, osR;
    std::vector<float> osBufL, osBufR;

    EqLinearPhase linear;
    bool staticCurveDirty = true;
    std::vector<float> domSt, domL, domR, domM, domS;   // matrix rebuild tables

    std::vector<float> scL, scR, detL, detR;
    bool scValid = false;

    // QA-EqPro SC-4: the DAW's four per-strip sidechain receive lines, each a
    // copied block like scL/scR.  A band picks one by index; the plugin's
    // single sidechain bus keeps using scExternal + setSidechain.
    std::array<std::vector<float>, 4> scSlotL, scSlotR;
    std::array<bool, 4> scSlotValid {};

    float agIn = 0.0f, agOut = 0.0f, aAutoGain = 0.0f, autoGainLin = 1.0f;

    StateVariableFilter listenSvfL, listenSvfR;
};

} // namespace kbs
