#pragma once

#include <JuceHeader.h>
#include <cmath>

// IdleSuspendFade
// ---------------
// Shutdown envelope shared by the two idle-suspend gates (InstStripTask and
// RustyDrumsProducerTask).  The gate decides WHEN a silent tab may stop paying
// for its chain; this decides HOW it stops.  Dropping straight to a cleared
// buffer guillotines whatever the chain is still ringing out - a reverb or
// delay tail, a NAM cab's decay - in a single sample, and that is an audible
// click across the whole strip rather than just the sampler.
//
// So the hold expiring starts a ramp instead of a clear: the chain keeps
// running at a falling gain, and only once the gain has reached zero does the
// caller take the cheap skip-everything path.  Steady idle therefore costs
// exactly what it did before - the extra work is bounded by kFadeOutSeconds,
// not by how long the tab stays quiet.
//
// THREAD SAFETY / OWNERSHIP: one instance per task, touched only from that
// task's run() on the audio thread.  No allocation, no locks.
struct IdleSuspendFade
{
    // MAGIC-NUMBER CALIBRATION.  Fade-out reuses the 15 ms that is already
    // calibrated as click-free for the Inst monitor fork, and spends 15% of the
    // 100 ms idle hold (BaySickDAWProcessor::kIdleSuspendSeconds) - long enough
    // for a decaying tail to reach zero smoothly, short enough not to move the
    // idle window meaningfully.
    //
    // Wake is deliberately asymmetric and much faster.  It only has to bridge
    // the step from a part-faded tail back to unity, whereas any note arriving
    // inside the fade window has its attack ramped in by the same envelope - so
    // a long fade-in would randomly soften transients depending on exactly when
    // the player hit the key.  3 ms is the usual de-zipper length: past
    // audibility as a step, short of audibility as a softened transient.
    static constexpr double kFadeOutSeconds = 0.015;
    static constexpr double kFadeInSeconds  = 0.003;

    struct Ramp
    {
        float startGain   = 1.0f;
        float endGain     = 1.0f;
        // The sweep is confined to the first rampSamples of the block; the
        // remaining holdSamples sit flat at endGain.  Sweeping the whole block
        // instead would make the fade LENGTH a function of the buffer size,
        // which is the exact defect class this pass is closing: a 3 ms wake ramp
        // smeared over a 4096-sample block is 93 ms, and the note that woke the
        // chain is the one whose attack gets scaled away.
        int   rampSamples = 0;
        int   holdSamples = 0;
        // Whole block is zero either way: the caller skips its entire chain.
        bool  suspended   = false;

        bool isUnity() const noexcept { return startGain >= 1.0f && endGain >= 1.0f; }
    };

    // `holdExpired` is the idle gate's verdict for THIS block.  Position carries
    // in mGain, so a fade spanning several short blocks behaves identically to
    // one that fits inside a single long one - the length is a duration, never a
    // block count, at every buffer size from 32 to 4096.
    Ramp advance (bool holdExpired, int numSamples, double sampleRate) noexcept
    {
        const int    n      = juce::jmax (0, numSamples);
        const double secs   = holdExpired ? kFadeOutSeconds : kFadeInSeconds;
        const float  target = holdExpired ? 0.0f : 1.0f;

        // Samples a FULL 0 -> 1 sweep takes at the live rate.
        const int full = juce::jmax (1, (int) (secs * juce::jmax (1.0, sampleRate)));

        Ramp r;
        r.startGain = mGain;

        const int needed = (int) std::ceil ((double) std::fabs (target - mGain)
                                            * (double) full);
        r.rampSamples = juce::jlimit (0, n, needed);
        r.holdSamples = n - r.rampSamples;

        const float moved = (float) r.rampSamples / (float) full;
        mGain = (mGain < target) ? juce::jmin (target, mGain + moved)
                                 : juce::jmax (target, mGain - moved);

        r.endGain   = mGain;
        r.suspended = holdExpired && r.startGain <= 0.0f;
        return r;
    }

    // Shared so the two call sites cannot drift on the ramp/hold split.
    static void apply (juce::AudioBuffer<float>& buffer, const Ramp& r) noexcept
    {
        if (r.isUnity()) return;

        if (r.rampSamples > 0)
            buffer.applyGainRamp (0, r.rampSamples, r.startGain, r.endGain);

        if (r.holdSamples > 0)
            buffer.applyGain (r.rampSamples, r.holdSamples, r.endGain);
    }

private:
    float mGain { 1.0f };
};
