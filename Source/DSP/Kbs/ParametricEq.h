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

#include "Devices.h"
#include "EqCharacter.h"        // Biquad + RBJ designs, kDetectorFloor
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
    bell = 0, lowPass, highPass, lowShelf, highShelf, notch, bandPass, tilt,
    // QA-EqFlagship W-6: unity magnitude, second-order phase rotation at
    // freq/Q - the layer-alignment tool.  Linear-phase modes flatten phase
    // by design, so there an all-pass deliberately contributes nothing.
    allPass
};

// QA-EqFlagship W-6: slope is CONTINUOUS dB/oct (filters only), 1..96 with
// Brickwall as the value past the top.  The table below is the MENU's detent
// list, nothing more - the engine no longer indexes anything by it.  In the
// linear modes any fractional slope is exact (the FIR realizes the query's
// analytic fractional Butterworth); in the IIR modes the fractional
// remainder is a staggered pole/zero ladder fit across the band, and the
// query evaluates the very pairs the audio runs, so drawn == heard.
// Brickwall is a linear-phase shape - an IIR cannot be one - so outside the
// linear modes it processes as 96 dB/oct and the editor says so.
inline constexpr int   kEqNumSlopes = 9;
inline constexpr float kEqSlopeDbPerOct[kEqNumSlopes] = { 6, 12, 18, 24, 36, 48, 72, 96, -1 };
inline constexpr int   kEqSlopeBrickwall = 8;
inline constexpr float kEqSlopeBrickwallDb = 97.0f;
inline bool eqSlopeIsBrickwall (float s) { return s >= 96.5f; }
inline constexpr int   kEqLadderPairs = 6;

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
    linearLow, linearMedium, linearHigh, linearVeryHigh, linearMaximum,
    // QA-EqFlagship W-9: the linear machinery with a frequency-weighted
    // phase floor - minimum-phase character in the lows (no bass pre-ring),
    // linear in the highs.  Appended so stored mode values keep meaning.
    mixed
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
        case EqMode::mixed:          return 12;   // room for LF min-phase tails
        default:                     return 0;
    }
}

// Mixed mode's minimum-phase weight per frequency: fully natural below
// 150 Hz (where linear-phase pre-ring is audible on bass), fully linear
// above 1.5 kHz (where minimum-phase cramping/rotation is the artifact),
// a raised-cosine ramp in log-frequency between.
inline float eqMixedMinPhaseWeight (float hz)
{
    if (hz <= 150.0f)  return 1.0f;
    if (hz >= 1500.0f) return 0.0f;
    const float t = std::log (hz / 150.0f) / std::log (10.0f);
    return 0.5f * (1.0f + std::cos ((float) kPi * t));
}

// The linear path's latency for a mode, from the same constants EqLinearPhase
// derives at prepare (taps = N/2 + 1, hop = N - taps + 1, latency =
// hop + (taps - 1) / 2).  ONE home - menu readouts must not re-derive it.
inline int eqLinearLatencySamples (EqMode m)
{
    const int order = eqLinearFftOrder (m);
    if (order <= 0) return 0;
    const int n = 1 << order;
    const int taps = n / 2 + 1;
    const int hop = n - taps + 1;
    return hop + (taps - 1) / 2;
}

// ── one band, as the user has it ───────────────────────────────────────────
struct EqBandParams
{
    bool      on = false;
    EqType    type = EqType::bell;
    float     freqHz = 1000.0f;
    float     gainDb = 0.0f;              // gain types only
    float     q = 0.707f;
    float     slope = 12.0f;              // filters only; dB/oct, continuous
                                          // 1..96; >= 96.5 = Brickwall
    float     phaseMix = 0.0f;            // W-9, linear modes only: 0 = the
                                          // mode's phase, 1 = this band
                                          // minimum-phase (no pre-ring)

    // W-12: the second, independent BELOW-threshold stage.  Engages when the
    // detector drops UNDER thresholdB, by the shortfall times the ratioB
    // slope; rangeBDb signed - positive lifts the quiet (upward compression),
    // negative pushes it further down (expansion).  0 dB of range = inert;
    // -60 threshold = effectively never (nothing real sits under it).
    float thresholdBDb = -60.0f;
    float ratioB = 2.0f;
    float rangeBDb = 0.0f;
    // W-12: onset-selective detection - 0 hears the whole signal, 1 hears
    // only transients (fast-minus-slow envelope), so dynamics can hit the
    // attack of a hit and ignore its ring.
    float onsetMix = 0.0f;

    // W-14: per-band saturation - the character stage scoped to this
    // band's region: the band-passed slice at freq/Q is softened and folded
    // back in.  0 = untouched.
    float satAmt = 0.0f;

    // W-1: spectral dynamics.  In a linear mode a spectral band watches the
    // individual bins inside its footprint and moves only the ones that
    // stand out from their own spectral neighborhood; outside the linear
    // modes the flag is inert and the band is its plain static self.
    // Density sets how surgical: low = broad neighborhoods (only real
    // spikes register), high = narrow (individual resonances).
    bool  spectral = false;
    float density = 0.5f;
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

// The home positions: 8 named defaults, then log-spaced.  ONE home - the
// parameter defaults and the A/B spare's seed both read it, so a band index
// means the same frequency wherever the band is built.
// QA-EqFlagship W-15: the band pool, ONE home.  96 pre-allocated - the
// engine NEVER grows storage under a running audio thread; the UI shows
// pages of 24 that appear as they fill, so the ceiling is invisible until
// band 97, which nobody places.
inline constexpr int kEqMaxBands = 96;

inline float eqDefaultFreq (int b)
{
    static const float k8[8] = { 40.0f, 100.0f, 250.0f, 630.0f,
                                 1600.0f, 4000.0f, 8000.0f, 12500.0f };
    if (b >= 0 && b < 8) return k8[b];
    // Bands 9..N share the same log span the 24-band law covered (56 Hz to
    // ~13.8 kHz), redistributed across however many bands the pool holds.
    const float t = (float) (b - 8) / (float) (kEqMaxBands - 9);
    return 20.0f * std::pow (1000.0f, 0.15f + 0.8f * t);
}

// A band as it ships: 1-8 on and flat at their home frequencies, the rest off.
// Every other field's struct default already IS the parameter default.
inline EqBandParams eqDefaultBand (int b)
{
    EqBandParams p;
    p.on     = (b >= 0 && b < 8);
    p.freqHz = eqDefaultFreq (b);
    return p;
}

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

// First-order pole/zero pair - the fractional-slope ladder's unit.  The
// bilinear image of (1 + s/wz) / (1 + s/wp): flat, then a 6 dB/oct ramp
// between the pole and zero, then flat again.  Normalized to unity on the
// side the filter passes (DC for a low-pass ladder, Nyquist for high-pass).
inline BiquadCoeffs firstOrderPZ (float fpole, float fzero, float sr, bool unityAtNyquist)
{
    const double wp = std::tan (kPi * std::clamp ((double) fpole, 5.0, sr * 0.47) / sr);
    const double wz = std::tan (kPi * std::clamp ((double) fzero, 5.0, sr * 0.47) / sr);
    const double a0 = 1.0 + 1.0 / wp;
    BiquadCoeffs c;
    c.b0 = (1.0 + 1.0 / wz) / a0;
    c.b1 = (1.0 - 1.0 / wz) / a0;
    c.b2 = 0.0;
    c.a1 = (1.0 - 1.0 / wp) / a0;
    c.a2 = 0.0;
    if (unityAtNyquist)
    {
        const double g = (c.b0 - c.b1) / (1.0 - c.a1);
        c.b0 /= g;
        c.b1 /= g;
    }
    return c;
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
    static constexpr int kMaxBands = kEqMaxBands;

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
        dryL.assign ((size_t) maxBlock, 0.0f);
        dryR.assign ((size_t) maxBlock, 0.0f);
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
                             || p.channel != b.p.channel || (p.on && ! b.p.on)
                             || p.spectral != b.p.spectral;
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
    double getSampleRate() const { return sr; }

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

    // W-3: the character stage.  Safe live setters - nothing reallocates.
    void setCharacter (EqCharMode m, float amount01)
    {
        charMode = m;
        charAmt = std::clamp (amount01, 0.0f, 1.0f);
    }
    EqCharMode getCharMode() const { return charMode; }
    float getCharAmount() const { return charAmt; }

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
    float bandExtentMagnitudeAt (int i, float hz, int direction = 0) const
    {
        if (i < 0 || i >= kMaxBands) return 1.0f;
        const auto& b = bands[(size_t) i];
        if (! b.p.on || b.p.muted || ! b.p.dynamic) return 1.0f;

        const float slope = 1.0f - 1.0f / std::max (1.0f, b.p.ratio);
        const float extent = std::min (std::abs (b.p.thresholdDb) * slope,
                                       std::abs (b.p.rangeDb));
        // W-12: with two stages the possible travel spans both directions.
        // direction +1 = the furthest up the curve can go, -1 = furthest
        // down, 0 = the legacy single-stage reading (the above stage's own
        // direction) so existing callers keep their meaning.
        const float slopeB = 1.0f - 1.0f / std::max (1.0f, b.p.ratioB);
        const float extentB = std::abs (b.p.rangeBDb) > 0.0f
            ? std::min ((b.p.thresholdBDb - kDetectorFloorDb) * slopeB,
                        std::abs (b.p.rangeBDb))
            : 0.0f;
        float travel;
        if (direction > 0)
            travel =  (b.p.rangeDb  > 0.0f ? extent  : 0.0f)
                    + (b.p.rangeBDb > 0.0f ? extentB : 0.0f);
        else if (direction < 0)
            travel = -((b.p.rangeDb  < 0.0f ? extent  : 0.0f)
                     + (b.p.rangeBDb < 0.0f ? extentB : 0.0f));
        else
            travel = b.p.rangeDb > 0.0f ? extent : -extent;
        const float extentGain = b.p.gainDb + travel;

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
    // W-1: the live spectral move at a frequency (dB, weighted by the
    // band's footprint) - what the graph's breathing spectral curve draws.
    // Audio-written, display-read; a torn read paints one frame odd.
    float spectralGrDbAt (int band, float hz) const
    {
        if (! eqModeIsLinear (mode) || ! anySpectral) return 0.0f;
        for (const auto& sl : spectralSlots)
        {
            if (sl.band != band) continue;
            const int n = linear.fftSize();
            if (n <= 0 || sl.gr.empty()) return 0.0f;
            const int k = std::clamp ((int) (hz * (float) n / (float) sr),
                                      0, n / 2);
            return sl.gr[(size_t) k] * sl.wt[(size_t) k];
        }
        return 0.0f;
    }

    float phaseAt (float hz) const
    {
        double ph = 0.0;
        if (eqModeIsLinear (mode))
        {
            // Post-PDC truth: pure linear content reports zero; what remains
            // is exactly the excess each band was given (W-9).
            for (const auto& b : bands)
            {
                if (! bandActive (b)) continue;
                float wgt = std::clamp (b.p.phaseMix, 0.0f, 1.0f);
                if (mode == EqMode::mixed)
                {
                    const float mw = eqMixedMinPhaseWeight (hz);
                    wgt = 1.0f - (1.0f - wgt) * (1.0f - mw);
                }
                if (wgt > 0.001f) ph += wgt * bandPhase (b, hz);
            }
        }
        else
        {
            for (const auto& b : bands)
                if (bandActive (b)) ph += bandPhase (b, hz);
        }
        if (polarityFlip) ph += kPi;
        return (float) ph;
    }

    // ── audio ─────────────────────────────────────────────────────────────
    void process (float* l, float* r, int numSamples)
    {
        if (numSamples <= 0) return;

        if (charMode == EqCharMode::difference)
        {
            std::copy (l, l + numSamples, dryL.begin());
            std::copy (r, r + numSamples, dryR.begin());
        }

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

        // W-14: per-band saturation - each armed band's slice, softened and
        // folded back, scaled by its amount.
        for (auto& b : bands)
        {
            if (! bandActive (b) || b.p.satAmt <= 0.001f
                || ! eqTypeHasGain (b.p.type)) continue;
            const float amt = b.p.satAmt;
            for (int i = 0; i < numSamples; ++i)
            {
                const float sl2 = (float) b.satFL.process (l[i]);
                const float sr2 = (float) b.satFR.process (r[i]);
                l[i] += amt * (eqchar::stage (EqCharMode::colorB, sl2, 2.5f) - sl2);
                r[i] += amt * (eqchar::stage (EqCharMode::colorB, sr2, 2.5f) - sr2);
            }
        }

        // W-3: the character stage - program-dependent drive (the follower
        // backs the color off as the material gets loud), unity small-signal
        // gain by construction, difference mode colors only what the EQ
        // changed and nulls exactly when the EQ is flat.
        if (charMode != EqCharMode::off && charAmt > 0.001f)
        {
            const float aCh = std::exp (-1.0f / (0.010f * (float) sr));
            for (int i = 0; i < numSamples; ++i)
            {
                const float pk = std::max (std::abs (l[i]), std::abs (r[i]));
                chEnv = aCh * chEnv + (1.0f - aCh) * pk;
                const float prog = std::clamp (1.0f / (0.4f + 2.0f * chEnv), 0.4f, 2.0f);
                const float d = 1.0f + charAmt * 2.5f * prog;

                if (charMode == EqCharMode::difference)
                {
                    const float dlS = l[i] - dryL[(size_t) i];
                    const float drS = r[i] - dryR[(size_t) i];
                    l[i] = dryL[(size_t) i] + eqchar::stage (EqCharMode::colorB, dlS, d);
                    r[i] = dryR[(size_t) i] + eqchar::stage (EqCharMode::colorB, drS, d);
                }
                else
                {
                    l[i] = eqchar::stage (charMode, l[i], d);
                    r[i] = eqchar::stage (charMode, r[i], d);
                }
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

        // The fractional-slope ladder (W-6): first-order pole/zero pairs as
        // biquads.  Integer slopes leave it empty.
        Biquad ladL[kEqLadderPairs], ladR[kEqLadderPairs];
        BiquadCoeffs ladC[kEqLadderPairs] {};
        int ladCount = 0;

        // Dynamics.
        Biquad detFL, detFR;
        float envL = 0.0f, envR = 0.0f;
        float envFastL = 0.0f, envFastR = 0.0f;   // onset detection pair
        float envSlowL = 0.0f, envSlowR = 0.0f;
        Biquad satFL, satFR;                      // W-14 band-scoped slice
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
            envFastL = envFastR = envSlowL = envSlowR = 0.0f;
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

            case EqType::allPass:
            {
                const double w0 = 2.0 * kPi * (double) f / designSr;
                const double alpha = std::sin (w0)
                                   / (2.0 * std::clamp ((double) qRaw, 0.1, 30.0));
                const double na = 1.0 + alpha;
                out[0].b0 = (1.0 - alpha) / na;
                out[0].b1 = (-2.0 * std::cos (w0)) / na;
                out[0].b2 = 1.0;
                out[0].a1 = out[0].b1;
                out[0].a2 = out[0].b0;
                return 1;
            }

            default: return 0;   // LP/HP/BP live on the SVF path
        }
    }

    // The fractional remainder (0..6 dB/oct) fit across the band as
    // kEqLadderPairs log-spaced pole/zero pairs.  Poles march geometrically
    // from just past the cutoff to the band edge; each pair's zero sits at
    // the ratio that makes its 6 dB/oct ramp average out to `rem` dB/oct
    // over the spacing.  Stateless and deterministic: the magnitude query
    // rebuilds the identical pairs, so drawn == heard by construction.
    int designSlopeLadder (EqType t, float fc, float rem, float designSr,
                           BiquadCoeffs* out) const
    {
        if (rem <= 0.05f) return 0;
        const bool hp = t == EqType::highPass;
        const float edge = hp ? 20.0f : std::min (designSr * 0.45f, 20000.0f);
        const float span = hp ? fc / edge : edge / fc;
        if (span < 1.6f) return 0;              // no room to shape anything
        const float R = std::pow (span, 1.0f / (float) kEqLadderPairs);
        const float zr = std::pow (R, rem / 6.0f);
        float fp = hp ? fc / std::pow (R, 0.25f) : fc * std::pow (R, 0.25f);
        for (int i = 0; i < kEqLadderPairs; ++i)
        {
            const float fz = hp ? fp / zr : fp * zr;
            out[i] = eqdesign::firstOrderPZ (fp, fz, designSr, hp);
            fp = hp ? fp / R : fp * R;
        }
        return kEqLadderPairs;
    }

    void rebuildBand (BandRt& b, float designSr)
    {
        const auto& p = b.p;

        if (eqTypeHasSlope (p.type))
        {
            const float slopeDb = eqSlopeIsBrickwall (p.slope)
                                    ? 96.0f : std::clamp (p.slope, 1.0f, 96.0f);
            const float f = std::clamp (b.cFreq, 20.0f, designSr * 0.45f);

            if (p.type == EqType::bandPass)
            {
                // Cascaded constant-peak sections at the user's Q: centre
                // stays put, skirts steepen with the slope.  A PARTIAL
                // constant-peak section is not realizable, so the IIR rounds
                // to whole sections (12 dB granularity); the linear modes
                // realize the exact fractional skirt from the query instead.
                b.svfCount = std::max (1, (int) std::round (slopeDb / 12.0f));
                b.hasPole = false;
                b.ladCount = 0;
                b.bpK = 1.0f / std::clamp (b.cQ, 0.35f, 24.0f);
                for (int s = 0; s < b.svfCount; ++s)
                {
                    b.svfL[s].set (f, b.cQ);
                    b.svfR[s].set (f, b.cQ);
                }
            }
            else
            {
                const int poles = (int) (slopeDb / 6.0f + 1.0e-4f);
                const float rem = slopeDb - 6.0f * (float) poles;
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

                b.ladCount = designSlopeLadder (p.type, f, rem, designSr, b.ladC);
                for (int s = 0; s < b.ladCount; ++s)
                {
                    b.ladL[s].setCoeffs (b.ladC[s]);
                    b.ladR[s].setCoeffs (b.ladC[s]);
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
        b.ladCount = 0;
        b.lastBuiltGr = p.dynamic ? b.grSmooth : 0.0f;

        if (p.satAmt > 0.001f)
        {
            const auto c = eqdesign::bandPass (std::clamp (b.cFreq, 30.0f, designSr * 0.4f),
                                               designSr,
                                               std::clamp (b.cQ, 0.5f, 8.0f));
            b.satFL.setCoeffs (c);
            b.satFR.setCoeffs (c);
        }
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
        b.ladCount = 0;
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

        // Onset shaping (W-12): a fast/slow SMOOTHED-magnitude pair turns
        // the detector feed into "what just changed" - at full mix a
        // sustained tone reads as silence and only attacks register.  Both
        // poles are symmetric and the fast one is slower than a carrier
        // half-cycle, or the pair rings at the carrier rate and a steady
        // tone reads as endless onsets (the first cut of this did).  The
        // factor of 2 keeps a typical attack's onset component near its raw
        // level.
        const float mix = std::clamp (p.onsetMix, 0.0f, 1.0f);
        const float aOF = std::exp (-1.0f / (0.003f * hostSr));
        const float aOS = std::exp (-1.0f / (0.060f * hostSr));

        float el = b.envL, er = b.envR;
        float fL = b.envFastL, fR = b.envFastR, sL = b.envSlowL, sR = b.envSlowR;
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
            float vL = std::abs (b.detFL.process (inL));
            float vR = std::abs (b.detFR.process (inR));
            if (mix > 0.001f)
            {
                fL = aOF * fL + (1.0f - aOF) * vL;
                fR = aOF * fR + (1.0f - aOF) * vR;
                sL = aOS * sL + (1.0f - aOS) * vL;
                sR = aOS * sR + (1.0f - aOS) * vR;
                vL = (1.0f - mix) * vL + mix * 2.0f * std::max (0.0f, fL - sL);
                vR = (1.0f - mix) * vR + mix * 2.0f * std::max (0.0f, fR - sR);
            }
            el = (vL > el ? aAtt : aRel) * el + (1.0f - (vL > el ? aAtt : aRel)) * vL;
            er = (vR > er ? aAtt : aRel) * er + (1.0f - (vR > er ? aAtt : aRel)) * vR;
        }
        b.envL = el; b.envR = er;
        b.envFastL = fL; b.envFastR = fR; b.envSlowL = sL; b.envSlowR = sR;

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

        // W-12: the independent below-threshold stage.  The silence gate
        // still applies - lifting actual silence forever is not a feature -
        // and the extent wall is the distance from thresholdB down to the
        // detector floor, mirroring the above stage's |threshold| wall.
        if (env >= kDetectorFloor && std::abs (p.rangeBDb) > 0.0f)
        {
            const float envDb = 20.0f * std::log10 (env);
            const float slopeB = 1.0f - 1.0f / std::max (1.0f, p.ratioB);
            const float under = p.thresholdBDb - envDb;
            if (under > 0.0f)
            {
                const float extentB = std::min ((p.thresholdBDb - kDetectorFloorDb) * slopeB,
                                                std::abs (p.rangeBDb));
                const float mag = std::min (under * slopeB, extentB);
                grDb += p.rangeBDb > 0.0f ? mag : -mag;
            }
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

                if (b.p.dynamic && eqTypeSupportsDynamic (b.p.type)
                    && ! (dynamicDeltaOnly && b.p.spectral))
                {
                    // A spectral band's movement is per-bin in the frames;
                    // running the whole-band dynamics on top would move the
                    // same energy twice.
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
                    if (b.biqCount == 0 && b.svfCount == 0 && ! b.hasPole
                        && b.ladCount == 0) continue;

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
        if (b.svfCount > 0 || b.hasPole || b.ladCount > 0)
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
            auto* lad = chan == 0 ? b.ladL : b.ladR;
            for (int i = 0; i < b.ladCount; ++i) x = lad[i].process (x);
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

        if (p.type == EqType::allPass) return 1.0;

        if (eqTypeHasSlope (p.type))
        {
            const bool brick = eqSlopeIsBrickwall (p.slope);
            if (brick && eqModeIsLinear (mode))
                return hzInsideBrickwall (p, hz) ? 1.0 : 1.0e-6;

            const float slopeDb = brick ? 96.0f : std::clamp (p.slope, 1.0f, 96.0f);
            const double fc = std::clamp ((double) p.freqHz, 20.0, designSr * 0.45);
            const double Om = std::tan (w / 2.0) / std::tan (kPi * fc / designSr);
            double mag = 1.0;

            if (p.type == EqType::bandPass)
            {
                const double k = 1.0 / std::clamp ((double) p.q, 0.35, 24.0);
                const double one = (k * Om) / std::sqrt (sq (1.0 - Om * Om) + sq (k * Om));
                const double n = eqModeIsLinear (mode)
                                   ? (double) slopeDb / 12.0
                                   : (double) std::max (1, (int) std::round (slopeDb / 12.0f));
                return std::pow (one, n);
            }

            const bool hp = p.type == EqType::highPass;

            if (eqModeIsLinear (mode))
            {
                // Exact fractional Butterworth: |H|^2 = 1 / (1 + Om^2n) for
                // any REAL n.  The FIR is designed from this very value, so
                // the linear modes deliver the true continuous slope.
                const double n = (double) slopeDb / 6.0;
                const double o2n = std::pow (Om, 2.0 * n);
                return (hp ? std::pow (Om, n) : 1.0) / std::sqrt (1.0 + o2n);
            }

            const int poles = (int) (slopeDb / 6.0f + 1.0e-4f);
            const float rem = slopeDb - 6.0f * (float) poles;

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

            // The ladder, rebuilt identically to the audio's pairs and
            // evaluated as responses - stateless, so the query never depends
            // on whether the band has been rebuilt yet.
            BiquadCoeffs lad[kEqLadderPairs];
            const int ln = designSlopeLadder (p.type, (float) fc, rem, designSr, lad);
            for (int i = 0; i < ln; ++i)
            {
                double m1 = 1.0, p1 = 0.0;
                eqdesign::biquadResponse (lad[i], w, m1, p1);
                mag *= m1;
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
        if (withDynamic && p.dynamic && eqTypeSupportsDynamic (p.type)
            && ! (p.spectral && eqModeIsLinear (mode)))
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
            const float slopeDb = eqSlopeIsBrickwall (p.slope)
                                    ? 96.0f : std::clamp (p.slope, 1.0f, 96.0f);
            const int poles = (int) (slopeDb / 6.0f + 1.0e-4f);
            const float rem = slopeDb - 6.0f * (float) poles;
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
            const int pairs = bp ? std::max (1, (int) std::round (slopeDb / 12.0f))
                                 : poles / 2;
            for (int i = 1; i <= pairs; ++i)
            {
                const double k = (poles == 2 || bp)
                                   ? 1.0 / std::clamp ((double) p.q, 0.35, 24.0)
                                   : 1.0 / eqdesign::butterworthQ (poles, i);
                const double num = bp ? kPi / 2.0 : (hp ? kPi : 0.0);
                ph += num - std::atan2 (k * Om, 1.0 - Om * Om);
            }
            if (! bp)
            {
                BiquadCoeffs lad[kEqLadderPairs];
                const int ln = designSlopeLadder (p.type, (float) fc, rem,
                                                  designSr, lad);
                for (int i = 0; i < ln; ++i)
                {
                    double m1 = 1.0, p1 = 0.0;
                    eqdesign::biquadResponse (lad[i], w, m1, p1);
                    ph += p1;
                }
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
            t->assign (bins, std::complex<float> (1.0f, 0.0f));
        for (auto& sl : spectralSlots)
        {
            sl.band = -1;
            sl.wt.assign (bins, 0.0f);
            sl.base.assign (bins, -120.0f);
            sl.gr.assign (bins, 0.0f);
        }
        specGainLin.assign (bins, 1.0f);
        specMagDb.assign (bins, -120.0f);
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
        updateSpectralSlots();

        bool anyDomain = false, anyPhase = mode == EqMode::mixed;
        for (const auto& b : bands)
        {
            if (! bandActive (b)) continue;
            if (b.p.channel != EqChannel::stereo) anyDomain = true;
            if (b.p.phaseMix > 0.005f) anyPhase = true;
        }

        // W-9: a band's excess phase is its own minimum-phase response,
        // weighted by the band's phaseMix OR'd with the mode's per-frequency
        // floor (mixed mode).  Magnitude is untouched - phase only decides
        // where each band's energy sits around the one reported delay.
        auto bandExcess = [this] (const BandRt& b, float hz) -> double
        {
            float wgt = std::clamp (b.p.phaseMix, 0.0f, 1.0f);
            if (mode == EqMode::mixed)
            {
                const float mw = eqMixedMinPhaseWeight (hz);
                wgt = 1.0f - (1.0f - wgt) * (1.0f - mw);
            }
            return wgt <= 0.001f ? 0.0 : wgt * bandPhase (b, hz);
        };

        if (! anyDomain && ! anyPhase)
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

        if (! anyDomain)
        {
            linear.setResponse ([&] (int bin, int) -> std::complex<float>
            {
                const float hz = (float) (bin * binHz);
                double m = 1.0, ph = 0.0;
                for (const auto& b : bands)
                {
                    if (! bandActive (b)) continue;
                    m *= bandMagnitude (b, hz, 0, false);
                    ph += bandExcess (b, hz);
                }
                return std::polar ((float) m, (float) ph);
            });
            return;
        }

        for (int k = 0; k <= n / 2; ++k)
        {
            const float hz = (float) (k * binHz);
            std::complex<double> st (1.0, 0.0), dl (1.0, 0.0), dr (1.0, 0.0),
                                 dm (1.0, 0.0), ds (1.0, 0.0);
            for (const auto& b : bands)
            {
                if (! bandActive (b)) continue;
                const auto h = std::polar (bandMagnitude (b, hz, 0, false),
                                           anyPhase ? bandExcess (b, hz) : 0.0);
                switch (b.p.channel)
                {
                    case EqChannel::stereo: st *= h; break;
                    case EqChannel::left:   dl *= h; break;
                    case EqChannel::right:  dr *= h; break;
                    case EqChannel::mid:    dm *= h; break;
                    case EqChannel::side:   ds *= h; break;
                }
            }
            domSt[(size_t) k] = std::complex<float> (st);
            domL [(size_t) k] = std::complex<float> (dl);
            domR [(size_t) k] = std::complex<float> (dr);
            domM [(size_t) k] = std::complex<float> (dm);
            domS [(size_t) k] = std::complex<float> (ds);
        }

        // The domain algebra is linear, so it holds verbatim with complex
        // entries - mono cancellation of a side band stays EXACT with phase
        // in play (pinned by test, not assumed).
        auto entry = [this] (float msSign, const std::vector<std::complex<float>>& side)
        {
            return [this, msSign, &side] (int bin, int) -> std::complex<float>
            {
                const auto k = (size_t) bin;
                return 0.5f * (domM[k] + msSign * domS[k]) * domSt[k] * side[k];
            };
        };
        linear.setResponseMatrix (entry (+1.0f, domL),    // LL
                                  entry (-1.0f, domR),    // LR
                                  entry (-1.0f, domL),    // RL
                                  entry (+1.0f, domR));   // RR
    }

    // W-1: which bands hold the spectral slots, their footprints, and
    // whether the hooks are live.  Runs with every static rebuild - slot
    // storage never allocates here.
    void updateSpectralSlots()
    {
        const int n = linear.fftSize();
        if (n <= 0) return;
        const double binHz = sr / (double) n;
        int used = 0;
        anySpectral = false;

        for (auto& sl : spectralSlots) sl.band = -1;

        for (int bi = 0; bi < kMaxBands && used < kSpectralSlots; ++bi)
        {
            const auto& b = bands[(size_t) bi];
            if (! bandActive (b) || ! b.p.spectral
                || ! eqTypeHasGain (b.p.type)) continue;

            auto& sl = spectralSlots[(size_t) used];
            sl.band = bi;
            // Footprint: the band's own bell/shelf shape at a reference
            // +6 dB, normalized 0..1 - so a spectral band with 0 dB static
            // gain still owns a region.
            EqBandParams ref = b.p;
            ref.gainDb = 6.0f;
            ref.dynamic = false;
            BiquadCoeffs cs[2];
            const int nc = designBiquads (ref, (float) sr, 6.0f, cs);
            for (int k = 0; k <= n / 2; ++k)
            {
                const double w = 2.0 * kPi * (double) (k * binHz)
                               / std::max (1.0, sr);
                double m = 1.0, ph = 0.0, tot = 1.0;
                for (int i = 0; i < nc; ++i)
                {
                    eqdesign::biquadResponse (cs[i], w, m, ph);
                    tot *= m;
                }
                const float db = (float) (20.0 * std::log10 (std::max (1.0e-6, tot)));
                sl.wt[(size_t) k] = std::clamp (db / 6.0f, 0.0f, 1.0f);
            }
            ++used;
            anySpectral = true;
        }

        // Hook install/removal - small lambdas, no allocation.
        if (anySpectral)
        {
            linear.spectralAnalyze = [this] (const std::complex<float>* xl,
                                             const std::complex<float>* xr, int nn)
            { spectralAnalyzeFrame (xl, xr, nn); };
            linear.spectralApply = [this] (std::complex<float>* y, int nn)
            { spectralApplyFrame (y, nn); };
        }
        else
        {
            linear.spectralAnalyze = nullptr;
            linear.spectralApply = nullptr;
        }
    }

    // Per frame: rate each slot's bins against their own smoothed
    // neighborhood, move the over-standers by the band's ratio slope within
    // its range, attack/release at the hop rate, spread the gain to avoid
    // musical noise, and fold every slot into one composite gain curve.
    void spectralAnalyzeFrame (const std::complex<float>* xl,
                               const std::complex<float>* xr, int n)
    {
        const int half = n / 2;
        const float hopSec = (float) (linear.fftSize() - linear.firTaps() + 1)
                           / (float) sr;

        for (size_t k = 0; k < (size_t) half + 1; ++k) specGainLin[k] = 1.0f;

        // The detector's view: Hann windowing applied IN the frequency
        // domain (the 3-tap kernel 0.5X[k] - 0.25X[k-1] - 0.25X[k+1] IS the
        // Hann window's spectrum convolution).  The frames must stay
        // rectangular for overlap-save, and rectangular leakage smears every
        // tone across the neighborhood the detector compares against.
        for (int k = 0; k <= half; ++k)
        {
            auto tap = [&] (const std::complex<float>* x) -> float
            {
                std::complex<float> h = 0.5f * x[(size_t) k];
                if (k > 0)    h -= 0.25f * x[(size_t) (k - 1)];
                if (k < half) h -= 0.25f * x[(size_t) (k + 1)];
                return std::abs (h);
            };
            const float m = std::max (tap (xl), tap (xr));
            specMagDb[(size_t) k] = std::max (-90.0f,
                20.0f * std::log10 (std::max (1.0e-7f, m)));
        }

        for (auto& sl : spectralSlots)
        {
            if (sl.band < 0) continue;
            auto& b = bands[(size_t) sl.band];
            const auto& p = b.p;

            // Neighborhood width from Density, PROPORTIONAL to frequency: a
            // flat bin count reads "a third of an octave" at 5 kHz and "two
            // octaves" at 100 Hz.  relW is the fractional bandwidth.
            const float relW = 0.03f + 0.30f * (1.0f - std::clamp (p.density, 0.0f, 1.0f));
            const float aSpread = std::exp (-1.0f / 3.0f);

            auto aBaseAt = [relW] (int k)
            {
                const float wBins = 3.0f + relW * (float) k;
                return std::exp (-1.0f / wBins);
            };

            for (int k = 0; k <= half; ++k)
                sl.base[(size_t) k] = specMagDb[(size_t) k];
            float run = sl.base[0];
            for (int k = 0; k <= half; ++k)
            {
                const float aB = aBaseAt (k);
                run = aB * run + (1.0f - aB) * sl.base[(size_t) k];
                sl.base[(size_t) k] = run;
            }
            for (int k = half; k >= 0; --k)
            {
                const float aB = aBaseAt (k);
                run = aB * run + (1.0f - aB) * sl.base[(size_t) k];
                sl.base[(size_t) k] = run;
            }

            // Required poke mirrors the band model: threshold 0 = never,
            // -60 = any amount over the neighborhood.
            const float required = std::max (0.0f, 60.0f + p.thresholdDb);

            double fpEnergy = 0.0, fpWeight = 0.0;
            for (int k = 0; k <= half; ++k)
            {
                if (sl.wt[(size_t) k] < 0.01f) continue;
                fpEnergy += (double) sl.wt[(size_t) k]
                          * std::pow (10.0, specMagDb[(size_t) k] * 0.1f);
                fpWeight += (double) sl.wt[(size_t) k];
            }
            const float gateDb = fpWeight > 0.0
                ? 10.0f * (float) std::log10 (std::max (1.0e-12, fpEnergy / fpWeight)) - 6.0f
                : 0.0f;
            const float slope = 1.0f - 1.0f / std::max (1.0f, p.ratio);
            const float cap = std::abs (p.rangeDb);
            // Effective timing floored at ~1.5 frames: the mask is computed
            // once per hop, and letting it fully re-aim every frame turns a
            // rotating leakage pattern into audible modulation spray on
            // whatever shares the footprint.
            const float attS = std::max (hopSec, 0.001f * std::max (0.1f, p.attackMs));
            const float relS = std::max (hopSec, 0.001f * std::max (1.0f, p.releaseMs));
            const float aA = std::exp (-hopSec / attS);
            const float aR = std::exp (-hopSec / relS);
            float deepest = 0.0f;

            for (int k = 0; k <= half; ++k)
            {
                if (sl.wt[(size_t) k] < 0.01f) { sl.gr[(size_t) k] *= aR; continue; }
                const float binDb = specMagDb[(size_t) k];
                const float over = (binDb - sl.base[(size_t) k]) - required;
                float target = 0.0f;
                if (over > 0.0f && binDb > kDetectorFloorDb && binDb > gateDb)
                    target = (p.rangeDb > 0.0f ? 1.0f : -1.0f)
                           * std::min (over * slope, cap);
                auto& g = sl.gr[(size_t) k];
                const float a = std::abs (target) > std::abs (g) ? aA : aR;
                g = target + a * (g - target);
            }

            // Spread the gain along k (both directions) - per-bin gating
            // without it is audible as musical noise.
            run = sl.gr[0];
            for (int k = 0; k <= half; ++k)
            { run = aSpread * run + (1.0f - aSpread) * sl.gr[(size_t) k]; sl.gr[(size_t) k] = run; }
            for (int k = half; k >= 0; --k)
            { run = aSpread * run + (1.0f - aSpread) * sl.gr[(size_t) k]; sl.gr[(size_t) k] = run; }

            for (int k = 0; k <= half; ++k)
            {
                const float db = sl.gr[(size_t) k] * sl.wt[(size_t) k];
                deepest = std::min (deepest, db);
                if (std::abs (db) > 0.01f)
                    specGainLin[(size_t) k] *= std::pow (10.0f, db * (1.0f / 20.0f));
            }
            b.grDbShown.store (deepest, std::memory_order_relaxed);
        }
    }

    void spectralApplyFrame (std::complex<float>* y, int n)
    {
        const int half = n / 2;
        for (int k = 0; k <= half; ++k)
        {
            y[(size_t) k] *= specGainLin[(size_t) k];
            if (k > 0 && k < half)
                y[(size_t) (n - k)] *= specGainLin[(size_t) k];
        }
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
    std::vector<std::complex<float>> domSt, domL, domR, domM, domS;   // matrix rebuild tables

    // W-1: spectral dynamics state.  A fixed budget of simultaneous
    // spectral bands, all storage sized at configureLinear - per-bin work
    // runs on the audio thread and must not allocate.  wt is the band's
    // normalized footprint, base the rolling spectral neighborhood, gr the
    // smoothed per-bin gain move (dB), env unused spare kept for symmetry.
    static constexpr int kSpectralSlots = 4;
    struct SpectralSlot
    {
        int band = -1;
        std::vector<float> wt, base, gr;
    };
    std::array<SpectralSlot, (size_t) kSpectralSlots> spectralSlots;
    std::vector<float> dryL, dryR;     // W-3 difference mode's dry copy
    EqCharMode charMode = EqCharMode::off;
    float charAmt = 0.5f;
    float chEnv = 0.0f;
    std::vector<float> specGainLin;    // composite per-bin linear gain
    std::vector<float> specMagDb;      // Hann-corrected magnitudes, shared per frame
    bool anySpectral = false;

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
