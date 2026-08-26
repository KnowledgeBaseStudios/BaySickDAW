// KBS Plugins — DSP devices
//
// Each device owns its parameters in natural units and registers them so the
// macro layer can drive them. Devices recompute coefficients only when a
// parameter actually changed.
#pragma once

#include "MacroParameter.h"
#include <array>

namespace kbs {

inline constexpr double kPi = 3.14159265358979323846;

// ── biquad ─────────────────────────────────────────────────────────────────
// Below this there is no signal to work on, and every level-relative detector
// has to say so explicitly.
//
// They all compare a band against the programme level, and in silence both sit
// on the same floor: the difference is zero and what is left is minus the
// threshold. On any programme-relative setting that threshold is negative, so
// the subtraction turns positive and the device decides it is over the line -
// while nothing is playing. Every one of them then reported a steady reduction
// sized by wherever its knob happened to sit, which is what a user sees as a
// meter that follows the control instead of the audio.
//
// -100 dBFS: far under anything anybody fades to, far over the denormal floor.
inline constexpr float kDetectorFloor = 1.0e-5f;

// The same floor in dB, with margin for the float rounding of log10.
// A relative-threshold compressor whose PROGRAM reference sits at this
// floor has nothing to be relative to - silence is not program.
inline constexpr float kDetectorFloorDb = -99.5f;

struct BiquadCoeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

class Biquad
{
public:
    void setCoeffs (const BiquadCoeffs& c) { co = c; }

    // Direct-form I. Stereo-safe via one instance per channel.
    inline float process (float in) noexcept
    {
        const double x = in;
        const double y = co.b0 * x + co.b1 * x1 + co.b2 * x2 - co.a1 * y1 - co.a2 * y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return (float) y;
    }

    void reset() { x1 = x2 = y1 = y2 = 0.0; }

    // ── RBJ cookbook designs ──
    static BiquadCoeffs peaking (double f0, double Q, double gainDb, double sr)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f0 / sr;
        const double al = std::sin (w) / (2.0 * Q);
        const double c = std::cos (w);
        const double a0 = 1.0 + al / A;
        return normalise ({ 1.0 + al * A, -2.0 * c, 1.0 - al * A, -2.0 * c, 1.0 - al / A }, a0);
    }

    static BiquadCoeffs highpass (double f0, double sr, double Q = 0.70710678)
    {
        const double w = 2.0 * kPi * f0 / sr;
        const double c = std::cos (w), al = std::sin (w) / (2.0 * Q);
        const double a0 = 1.0 + al;
        return normalise ({ (1.0 + c) / 2.0, -(1.0 + c), (1.0 + c) / 2.0, -2.0 * c, 1.0 - al }, a0);
    }

    static BiquadCoeffs lowpass (double f0, double sr, double Q = 0.70710678)
    {
        const double w = 2.0 * kPi * f0 / sr;
        const double c = std::cos (w), al = std::sin (w) / (2.0 * Q);
        const double a0 = 1.0 + al;
        return normalise ({ (1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, -2.0 * c, 1.0 - al }, a0);
    }

    // First-order shelves via bilinear transform of H(s) = (s + wc*A)/(s + wc).
    // These are deliberately gentle — roughly 3.6-6.4 dB/oct, far shallower than
    // RBJ at S=1, which is what makes them usable as a broad tone control rather
    // than a corrective filter.
    //   low : A at DC, unity at Nyquist      high: unity at DC, A at Nyquist
    static BiquadCoeffs lowShelfFirstOrder (double f0, double gainDb, double sr)
    {
        const double A = std::pow (10.0, gainDb / 20.0);
        const double K = std::tan (kPi * f0 / sr);
        return normalise ({ A * K + 1.0, A * K - 1.0, 0.0, K - 1.0, 0.0 }, K + 1.0);
    }

    static BiquadCoeffs highShelfFirstOrder (double f0, double gainDb, double sr)
    {
        const double A = std::pow (10.0, gainDb / 20.0);
        const double K = std::tan (kPi * f0 / sr);
        return normalise ({ A + K, K - A, 0.0, K - 1.0, 0.0 }, 1.0 + K);
    }

private:
    static BiquadCoeffs normalise (BiquadCoeffs c, double a0)
    {
        return { c.b0 / a0, c.b1 / a0, c.b2 / a0, c.a1 / a0, c.a2 / a0 };
    }

    BiquadCoeffs co;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
};

// ── an EQ band, macro-addressable ──────────────────────────────────────────
enum class BandType { Peak, HighPass, LowPass, LowShelf, HighShelf };

class EqBand
{
public:
    EqBand (std::string id, BandType t) : prefix (std::move (id)), type (t) {}

    void registerParams (ParamRegistry& reg)
    {
        freq  = { 1000.0f, 20.0f, 20000.0f, "Hz" };
        gain  = { 0.0f, -24.0f, 24.0f, "dB" };
        q     = { 0.707f, 0.1f, 20.0f, "" };
        enabled = { 1.0f, 0.0f, 1.0f, "" };
        reg.add (prefix + "/frequency", &freq);
        reg.add (prefix + "/gain", &gain);
        reg.add (prefix + "/q", &q);
        reg.add (prefix + "/enabled", &enabled);
    }

    void prepare (double sampleRate) { sr = sampleRate; dirty = true; }

    void update()
    {
        if (! dirty) return;
        BiquadCoeffs c;
        switch (type)
        {
            case BandType::Peak:      c = Biquad::peaking (freq.value, q.value, gain.value, sr); break;
            case BandType::HighPass:  c = Biquad::highpass (freq.value, sr); break;
            case BandType::LowPass:   c = Biquad::lowpass (freq.value, sr); break;
            case BandType::LowShelf:  c = Biquad::lowShelfFirstOrder (freq.value, gain.value, sr); break;
            case BandType::HighShelf: c = Biquad::highShelfFirstOrder (freq.value, gain.value, sr); break;
        }
        for (auto& b : filters) b.setCoeffs (c);
        dirty = false;
    }

    void markDirty() { dirty = true; }

    inline float process (int ch, float in) noexcept
    {
        if (enabled.value < 0.5f) return in;
        return filters[(size_t) ch].process (in);
    }

    void reset() { for (auto& b : filters) b.reset(); }

    Param freq, gain, q, enabled;

private:
    std::string prefix;
    BandType type;
    std::array<Biquad, 2> filters;
    double sr = 48000.0;
    bool dirty = true;
};

// ── compressor ─────────────────────────────────────────────────────────────
//
// Peak detector, flat sidechain, roughly 4 dB knee, fixed ratio.
//
// Makeup is measured rather than dialled: the compressor watches what its own
// reduction took off and hands back exactly that. What is adjustable is how
// much of it comes back - see makeupAmount below - because that is a decision
// about the mix, not something the compression knob should be dragging along.
class Compressor
{
public:
    explicit Compressor (std::string id) : prefix (std::move (id)) {}

    void registerParams (ParamRegistry& reg)
    {
        // Positive values are meaningful once a reference is set: the threshold
        // is then an offset from the program level, and it has to be able to
        // sit above it (crest factor) for the compressor to read as "off".
        threshold  = { 0.0f, -60.0f, 18.0f, "dB" };
        ratio      = { 4.0f, 1.0f, 100.0f, "" };
        attackMs   = { 4.0f, 0.1f, 200.0f, "ms" };
        releaseMs  = { 80.0f, 1.0f, 3000.0f, "ms" };
        kneeDb     = { 4.0f, 0.0f, 24.0f, "dB" };

        // How much of the measured makeup to hand back. 100 % returns the whole
        // of what the reduction took, so the knob changes the sound and not the
        // loudness; 0 % returns none of it, which is a compressor as a level
        // control - it turns the loud parts down and leaves them down.
        //
        // This replaced a fixed +/-24 dB trim registered as "outputGain" that no
        // product ever linked. A trim in decibels cannot track the material and
        // a proportion of a measured figure can, which is the whole point.
        makeupAmount = { 100.0f, 0.0f, 100.0f, "%" };

        reg.add (prefix + "/threshold", &threshold);
        reg.add (prefix + "/ratio", &ratio);
        reg.add (prefix + "/attack", &attackMs);
        reg.add (prefix + "/release", &releaseMs);
        reg.add (prefix + "/knee", &kneeDb);
        reg.add (prefix + "/makeup", &makeupAmount);
    }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        env = 0.0f;
        fastEnv = 0.0f;
        aFastAtk = std::exp (-1.0f / (float) (0.002 * sampleRate));
        aFastRel = std::exp (-1.0f / (float) (0.030 * sampleRate));
        mkIn = mkOut = 0.0f;
        autoMakeup = 1.0f;
        mkCount = 0;
        mkWindow = (int) sampleRate;            // the 1-second window, in samples
        mkRelearn = (int) (0.05 * sampleRate);  // seed after a knob move
    }

    void updateCoeffs()
    {
        aAtk = std::exp (-1.0f / (float) (0.001 * attackMs.value * sr));
        aRel = std::exp (-1.0f / (float) (0.001 * releaseMs.value * sr));
        // Held as an exponent, because makeup is a gain and pow(g, a) is exactly
        // 10^(a * dB/20): the amount slides the makeup linearly in decibels,
        // which is how it is heard. Blending towards 1.0 instead would bunch the
        // whole change into the top of the control.
        mkExp  = std::clamp (makeupAmount.value * 0.01f, 0.0f, 1.0f);
        mkFull = mkExp > 0.9999f;
        aMakeup = std::exp (-1.0f / (float) (1.0 * sr));

        // A moved knob changes what the makeup must hand back RIGHT NOW -
        // so the average restarts FROM THIS INSTANT. At the restart the
        // running mean of in and out collapses to the current sample pair,
        // whose ratio is exactly the current reduction: the makeup is
        // correct on the FIRST sample after any change - no ramp, no lag -
        // and then glides into the crest-true one-second average. (The
        // owner asked for immediate and was right to; a fast relearn was
        // still a ramp, and lookahead cannot see a knob move coming.)
        if (std::abs (threshold.value - mkLastThr) > 0.05f
            || std::abs (ratio.value - mkLastRatio) > 0.01f
            || std::abs (kneeDb.value - mkLastKnee) > 0.05f)
        {
            mkLastThr = threshold.value;
            mkLastRatio = ratio.value;
            mkLastKnee = kneeDb.value;
            mkCount = 0;
        }
    }

    // Stereo-linked: one detector fed by the larger of the two channels, so the
    // image stays put instead of wandering when one side is louder.
    inline float computeGain (float l, float r) noexcept
    {
        const float peak = std::max (std::abs (l), std::abs (r));
        const float a = peak > env ? aAtk : aRel;
        env = a * env + (1.0f - a) * peak;

        // A second, fixed-speed follower that exists only to answer "is
        // there input at all?". The knob's own release can take seconds to
        // drain the detector after the transport stops, and a program
        // reference draining at a different speed made 'over' grow into the
        // stop - the swelling-reduction burp. Slowing the reference fixed
        // the burp but recalibrated every compressor's GR during program
        // (the quarter-reduction regression), so the reference follower is
        // back to its original speed and THIS gate does the silence work:
        // thirty milliseconds after the input truly stops, the gain
        // computer stands down.
        const float af = peak > fastEnv ? aFastAtk : aFastRel;
        fastEnv = af * fastEnv + (1.0f - af) * peak;

        // Two silences gate the gain computer: a detector under the floor
        // has nothing to compress, and a relative-threshold reference under
        // the floor means the program itself is gone - without the second
        // gate the knob-relative threshold read silence as program and the
        // meter followed the knob (the Glue complaint, round two: the first
        // fix floored only the detector). Absolute-mode compressors keep
        // referenceDb at 0 and never hit the second test.
        if (env < kDetectorFloor || fastEnv < kDetectorFloor
            || referenceDb < kDetectorFloorDb)
        {
            lastGrDb = 0.0f;
            return appliedMakeup();
        }

        const float db = 20.0f * std::log10 (std::max (env, 1.0e-9f));
        const float over = db - (threshold.value + referenceDb);
        const float k = kneeDb.value;

        float grDb = 0.0f;
        if (over <= -k * 0.5f)          grDb = 0.0f;
        else if (over >= k * 0.5f)      grDb = over * (1.0f / ratio.value - 1.0f);
        else                                                   // quadratic knee
        {
            const float x = over + k * 0.5f;
            grDb = (1.0f / ratio.value - 1.0f) * (x * x) / (2.0f * k);
        }
        lastGrDb = grDb;

        // Give back exactly what the reduction took.
        //
        // This used to be a hand-written table per product, mapping knob
        // position to a number of decibels. That cannot work: how much a
        // compressor takes off depends on the crest of the material, not on
        // where the knob is. Measured across five signals the same table
        // ranged over nearly seven decibels - right on one source by luck and
        // wrong on the rest.
        //
        // Averaged over about a second: far slower than any release here, so
        // it restores loudness without flattening the dynamics the compressor
        // is there to create.
        const float grGain = std::pow (10.0f, grDb / 20.0f);
        const float post = peak * grGain;

        // Cumulative mean while the window fills, exponential once it has:
        // correct from the very first block instead of ramping in from an
        // empty average, and (via the relearn seed above) back on target
        // within ~150 ms of a knob move. Identical steady state.
        if (mkCount < mkWindow)
        {
            ++mkCount;
            const float w = 1.0f / (float) mkCount;
            mkIn  += (peak * peak - mkIn) * w;
            mkOut += (post * post - mkOut) * w;
        }
        else
        {
            mkIn  = aMakeup * mkIn  + (1.0f - aMakeup) * peak * peak;
            mkOut = aMakeup * mkOut + (1.0f - aMakeup) * post * post;
        }

        if (mkOut > 1.0e-12f)
        {
            const float want = std::sqrt (mkIn / mkOut);
            autoMakeup = std::min (8.0f, std::max (0.25f, want));
        }

        return grGain * appliedMakeup();
    }

    // What actually gets handed back, after the amount has had its say. At the
    // default of 100 % this is the measured figure untouched and costs nothing;
    // at 0 % it is unity, so the reduction stands.
    inline float appliedMakeup() const noexcept
    {
        if (mkFull)        return autoMakeup;
        if (mkExp <= 0.0f) return 1.0f;
        return std::pow (autoMakeup, mkExp);
    }

    float getGainReductionDb() const { return lastGrDb; }

    // Slides the threshold with the program level, so a band compressor can be
    // told "this far below the whole signal" instead of an absolute dBFS point.
    // Zero — the default — leaves the threshold absolute, which is what a
    // broadband compressor wants.
    void setReferenceDb (float db) noexcept { referenceDb = db; }

    Param threshold, ratio, attackMs, releaseMs, kneeDb, makeupAmount;

private:
    std::string prefix;
    double sr = 48000.0;
    float env = 0.0f, aAtk = 0.0f, aRel = 0.0f, lastGrDb = 0.0f;
    float fastEnv = 0.0f, aFastAtk = 0.0f, aFastRel = 0.0f;
    float mkExp = 1.0f;
    bool  mkFull = true;
    int   mkCount = 0, mkWindow = 48000, mkRelearn = 2400;
    float mkLastThr = 1.0e9f, mkLastRatio = 0.0f, mkLastKnee = 0.0f;
    float mkIn = 0.0f, mkOut = 0.0f, aMakeup = 0.0f, autoMakeup = 1.0f;
    float referenceDb = 0.0f;
};

// ── brickwall-ish output limiter ───────────────────────────────────────────
class Limiter
{
public:
    explicit Limiter (std::string id) : prefix (std::move (id)) {}

    void registerParams (ParamRegistry& reg)
    {
        ceiling   = { 0.0f, -48.0f, 0.0f, "dB" };
        releaseMs = { 50.0f, 10.0f, 400.0f, "ms" };
        reg.add (prefix + "/ceiling", &ceiling);
        reg.add (prefix + "/release", &releaseMs);
    }

    // env is a gain, so unity is the resting state. Starting it at zero meant
    // the limiter faded in from silence over several release constants every
    // time playback started.
    void prepare (double sampleRate) { sr = sampleRate; reset(); }

    // Hosts call reset to clear tails between takes and before an offline
    // bounce. Without it the envelope carries over and the same audio renders
    // differently depending on what was played before it.
    void reset() { env = 1.0f; lastGrDb = 0.0f; }

    inline void process (float& l, float& r) noexcept
    {
        const float ceilLin = std::pow (10.0f, ceiling.value / 20.0f);
        const float peak = std::max (std::abs (l), std::abs (r));

        if (peak <= ceilLin) { decay(); }
        else
        {
            const float needed = ceilLin / std::max (peak, 1.0e-9f);
            if (needed < env) env = needed;
        }

        applyGain (l, r);
        lastGrDb = 20.0f * std::log10 (std::max (env, 1.0e-6f));
    }

    // Negative dB, for the fader's reduction bar.
    float getGainReductionDb() const { return lastGrDb; }

    Param ceiling, releaseMs;

private:
    void decay()
    {
        const float aRel = std::exp (-1.0f / (float) (0.001 * releaseMs.value * sr));
        env = env + (1.0f - aRel) * (1.0f - env);
        env = std::min (env, 1.0f);
    }
    void applyGain (float& l, float& r) const { l *= env; r *= env; }

    std::string prefix;
    double sr = 48000.0;
    float env = 1.0f, lastGrDb = 0.0f;
};

} // namespace kbs
