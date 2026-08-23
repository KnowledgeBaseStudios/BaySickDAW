#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <cmath>

// ── The one pan law (QA-TrueLevel, Jeff 2026-08-22) ─────────────────────────
// Every pan knob in the app maps through here: mixer strips / buses / master
// (fold form), engine pan knobs + per-note pan (mono placement or balance
// form).  Unison spread, the SFZ `pan` opcode, vendored sfizz and hosted
// plugins own their pan and do NOT come through here (SC-6 / SC-7).
//
// Both laws are CENTER-UNITY: pan 0 yields exactly {1, 1}.  A pan law is a
// statement about how a sound sweeps, never a standing attenuation on what
// sits at center -- FL Studio measures 0 dB at center with Circular, and
// every DAW researched converges on center-unity for stereo content
// (Research Reports/daw-architecture-pan-law-stages-2026-08-22.md).  The
// previous helper skipped the law at dead center and so jumped 3 or 6 dB on
// the first tick off center; these curves are continuous through 0.
//
//   Ramped (FL "Circular"):  l = sqrt2 * cos(t * pi/2),  r = sqrt2 * sin(t * pi/2),
//                            t = (pan + 1) / 2.  100% right = {0, 1.414}: the
//                            near side RISES to +3 dB, the far side is gone.
//   Flat   (FL "Triangular"): the near side holds 1.0, the far side tapers
//                            linearly to 0.  100% right = {0, 1}.
namespace baysick::pan
{
    enum class Law : int { Ramped = 0, Flat = 1 };

    inline Law lawFromParam (float v) noexcept
    {
        return v >= 0.5f ? Law::Flat : Law::Ramped;
    }

    // THREAD MODEL: the ONE project law, published by the host processor at the
    // top of every audio block from master_pan_law (the audio thread is the
    // writer); every engine voice, the clip decode and the MT strip tasks read
    // it with a relaxed load.  Process-wide on purpose: a project has one law
    // by definition, the app is standalone-only, and a global reaches engines
    // created outside the rig (auditions, pickers) that a per-engine pointer
    // hand-off would miss.  Same precedent as MasterOutputRouting /
    // AudioClipStreamer::sOfflineRender.
    inline std::atomic<int> gLaw { (int) Law::Ramped };

    inline void publishLaw (float paramValue) noexcept
    {
        gLaw.store ((int) lawFromParam (paramValue), std::memory_order_relaxed);
    }
    inline Law currentLaw() noexcept
    {
        return (Law) gLaw.load (std::memory_order_relaxed);
    }

    struct MonoGains { float l { 1.0f }, r { 1.0f }; };

    // ll = L out of L, rr = R out of R, lr = L INTO R, rl = R INTO L.
    struct StereoMatrix
    {
        float ll { 1.0f }, rr { 1.0f }, lr { 0.0f }, rl { 0.0f };

        bool isIdentity() const noexcept
        {
            return ll == 1.0f && rr == 1.0f && lr == 0.0f && rl == 0.0f;
        }
        bool operator== (const StereoMatrix& o) const noexcept
        {
            return ll == o.ll && rr == o.rr && lr == o.lr && rl == o.rl;
        }
        bool operator!= (const StereoMatrix& o) const noexcept { return ! (*this == o); }
    };

    // Placement of a MONO signal (synth voices, mono samples, mono clip files).
    inline MonoGains monoGains (float pan, Law law) noexcept
    {
        pan = juce::jlimit (-1.0f, 1.0f, pan);
        if (law == Law::Flat)
            return { pan <= 0.0f ? 1.0f : 1.0f - pan,
                     pan >= 0.0f ? 1.0f : 1.0f + pan };

        const float t = (pan + 1.0f) * 0.5f * juce::MathConstants<float>::halfPi;
        constexpr float kSqrt2 = 1.41421356f;
        return { kSqrt2 * std::cos (t), kSqrt2 * std::sin (t) };
    }

    // For per-sample render loops whose position glides (note-pan ramps, pan
    // modulation): trig only when the position or the law actually changes,
    // which is never for a static note (CPU Safeguarding).
    struct GainCache
    {
        MonoGains gains;
        float     pos { 0.0f };
        Law       law { Law::Ramped };

        const MonoGains& get (float position, Law l) noexcept
        {
            if (position != pos || l != law)
            {
                pos = position; law = l;
                gains = monoGains (position, l);
            }
            return gains;
        }
    };

    // Balance of STEREO material: each side scaled by its mono gain, no
    // crossfeed.  Engine pans on stereo samples / stereo clip files (SFZ-spec
    // semantics: placement for mono, balance for stereo).
    inline StereoMatrix balance (float pan, Law law) noexcept
    {
        const auto g = monoGains (pan, law);
        return { g.l, g.r, 0.0f, 0.0f };
    }

    // FL fold, the MIXER form (strip / bus / master).  The far side crossfades
    // INTO the near side as you pan, so 100% is the mono-sum of both sides in
    // one channel.  Jeff measured it in FL (2026-08-22, SC-3): left-only
    // material panned 100% right reads -3 dB, so at 100% each side lands in
    // the near channel at 0.707; both-sides material then reads +3 dB there.
    //   pan p >= 0:  ll = gL(p),  rr = a(p),  lr = b(p),  rl = 0
    //   a + b = gR(p),  a(0) = 1,  b(0) = 0,  a(1) = b(1) = gR(1) / 2,
    //   b(p) = gR(p) * p / 2  -- smooth, and it lands on the measured ends.
    // Mirrored for p < 0.  Center is the identity for both laws.
    inline StereoMatrix fold (float pan, Law law) noexcept
    {
        pan = juce::jlimit (-1.0f, 1.0f, pan);
        if (std::abs (pan) < 1.0e-6f) return {};

        const auto g = monoGains (pan, law);
        StereoMatrix m;
        if (pan > 0.0f)
        {
            const float b = g.r * pan * 0.5f;
            m.ll = g.l;  m.rr = g.r - b;  m.lr = b;  m.rl = 0.0f;
        }
        else
        {
            const float b = g.l * (-pan) * 0.5f;
            m.rr = g.r;  m.ll = g.l - b;  m.rl = b;  m.lr = 0.0f;
        }
        return m;
    }

    // Applies the matrix in place, ramped linearly across the block from
    // `from` to `to` (the fader ramps; a pan that did not would click on
    // automation).  Identity -> identity is a no-op, which is the common case
    // for an untouched knob.  Stereo only; mono buffers pass through.
    inline void apply (juce::AudioBuffer<float>& buf,
                       const StereoMatrix& from, const StereoMatrix& to) noexcept
    {
        if (buf.getNumChannels() < 2) return;
        if (from.isIdentity() && to.isIdentity()) return;

        float* L = buf.getWritePointer (0);
        float* R = buf.getWritePointer (1);
        const int n = buf.getNumSamples();
        if (n <= 0) return;

        if (from == to)
        {
            if (to.lr == 0.0f && to.rl == 0.0f)
            {
                buf.applyGain (0, 0, n, to.ll);
                buf.applyGain (1, 0, n, to.rr);
                return;
            }
            for (int i = 0; i < n; ++i)
            {
                const float l = L[i], r = R[i];
                L[i] = to.ll * l + to.rl * r;
                R[i] = to.rr * r + to.lr * l;
            }
            return;
        }

        const float inv = 1.0f / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const float k  = (float) i * inv;
            const float ll = from.ll + (to.ll - from.ll) * k;
            const float rr = from.rr + (to.rr - from.rr) * k;
            const float lr = from.lr + (to.lr - from.lr) * k;
            const float rl = from.rl + (to.rl - from.rl) * k;
            const float l = L[i], r = R[i];
            L[i] = ll * l + rl * r;
            R[i] = rr * r + lr * l;
        }
    }
}
