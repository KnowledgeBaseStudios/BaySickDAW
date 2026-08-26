// KBS Plugins - the state-variable filter, and things that drive it.
//
// A biquad is the right tool for a filter that sits still. It is the wrong tool
// for one that sweeps: recomputing RBJ coefficients every sample is expensive,
// and the direct form misbehaves when the coefficients move quickly under it.
//
// The topology-preserving state-variable filter solves both. One set of cheap
// coefficients, all four responses available at once from the same state, and
// it stays stable while the cutoff is swept - because the structure is a
// discretised analogue integrator pair rather than a difference equation whose
// poles you are yanking around.
#pragma once

#include "Devices.h"

namespace kbs {

class StateVariableFilter
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { s1 = s2 = 0.0f; }

    void set (float hz, float q)
    {
        const float f = std::clamp (hz, 15.0f, (float) (sr * 0.47));
        g = std::tan ((float) kPi * f / (float) sr);
        k = 1.0f / std::clamp (q, 0.35f, 24.0f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
    }

    // Every response comes out of the same two integrators, so a morph between
    // them costs nothing but the crossfade.
    struct Out { float lp, bp, hp, notch, peak; };

    inline Out process (float x) noexcept
    {
        const float hp = (x - (g + k) * s1 - s2) * a1;
        const float v1 = g * hp;
        const float bp = v1 + s1;
        s1 = bp + v1;
        const float v2 = g * bp;
        const float lp = v2 + s2;
        s2 = lp + v2;

        if (! std::isfinite (s1)) s1 = 0.0f;
        if (! std::isfinite (s2)) s2 = 0.0f;

        return { lp, bp, hp, lp + hp, lp - hp };
    }

private:
    double sr = 48000.0;
    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, s1 = 0.0f, s2 = 0.0f;
};

// ── vowels ─────────────────────────────────────────────────────────────────
//
// A vowel is not a filter shape, it is the position of two or three resonant
// peaks. Morphing between vowels means moving those peaks, which is exactly the
// kind of many-parameters-at-once gesture the macro layer exists for.
struct Vowel { const char* name; float f1, f2, f3; float g1, g2, g3; };

inline const Vowel& vowel (int i)
{
    static const Vowel v[5] {
        //  name    f1     f2     f3     g1    g2    g3
        { "A",   730.f, 1090.f, 2440.f, 1.00f, 0.50f, 0.25f },
        { "E",   530.f, 1840.f, 2480.f, 1.00f, 0.63f, 0.32f },
        { "I",   270.f, 2290.f, 3010.f, 1.00f, 0.40f, 0.28f },
        { "O",   570.f,  840.f, 2410.f, 1.00f, 0.56f, 0.18f },
        { "U",   300.f,  870.f, 2240.f, 1.00f, 0.35f, 0.14f },
    };
    return v[std::clamp (i, 0, 4)];
}

// Position 0-1 walks A-E-I-O-U, interpolating the formants rather than
// crossfading between filter outputs - a crossfade would pass through a state
// where both vowels are audible at once, which sounds like neither.
inline Vowel vowelAt (float pos01)
{
    const float p = std::clamp (pos01, 0.0f, 1.0f) * 4.0f;
    const int i = std::min (3, (int) p);
    const float t = p - (float) i;
    const Vowel& a = vowel (i);
    const Vowel& b = vowel (i + 1);
    auto m = [t] (float x, float y) { return x + (y - x) * t; };
    return { a.name, m (a.f1, b.f1), m (a.f2, b.f2), m (a.f3, b.f3),
                     m (a.g1, b.g1), m (a.g2, b.g2), m (a.g3, b.g3) };
}

// ── a sample-and-hold source ───────────────────────────────────────────────
//
// Stepped random, for filters that jump rather than sweep. Deterministic from a
// seed so a render is reproducible, which a std::random_device would not be.
class StepRandom
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { counter = 0; value = 0.0f; state = 987654321u; }

    void setRate (float hz)
    {
        period = std::max (1, (int) (sr / std::max (0.02f, hz)));
    }

    inline float next() noexcept
    {
        if (--counter <= 0)
        {
            counter = period;
            state = state * 1664525u + 1013904223u;
            value = (float) ((int32_t) state) * 4.6566129e-10f;
        }
        return value;
    }

private:
    double sr = 48000.0;
    uint32_t state = 987654321u;
    int period = 1000, counter = 0;
    float value = 0.0f;
};

} // namespace kbs
