// KBS Core - the EQ's character stage (QA-EqFlagship W-3/W-14).
//
// Three colors and a difference mode, all program-dependent: the drive backs
// off as the material gets loud (the follower lives in the engine), so the
// color stays a shade, not a fuzz.  Curves are LOW-ORDER on purpose - a
// cubic and a soft tanh keep the added harmonics low enough that aliasing
// sits below audibility at these drives; a heavier drive stage would need
// its own oversampler and is deliberately out of this stage's reach.
#pragma once

#include <algorithm>
#include <cmath>

namespace kbs {

enum class EqCharMode { off = 0, colorA, colorB, difference };

namespace eqchar {

// Subtle: pure odd-order softening, level-true for small signals.
inline float shapeA (float x)
{
    x = std::clamp (x, -1.5f, 1.5f);
    return x - (x * x * x) * (1.0f / 6.0f);
}

// Warm: tanh with a whisper of asymmetry - the even harmonics are the
// "tube" of it.
inline float shapeB (float x)
{
    const float a = x + 0.06f * x * x;
    return std::tanh (a);
}

// One band-limited-ish soft stage: shape at drive d, normalized so the
// small-signal gain stays unity (the color must not be a level change).
inline float stage (EqCharMode m, float x, float d)
{
    const float dx = x * d;
    const float y = m == EqCharMode::colorA ? shapeA (dx) : shapeB (dx);
    return y / d;
}

} // namespace eqchar
} // namespace kbs
