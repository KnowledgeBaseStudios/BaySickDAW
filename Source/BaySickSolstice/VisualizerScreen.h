#pragma once
#include <JuceHeader.h>
#include "HarmonicEngine.h"

class BaySickSolsticeSynth;

// ── VisualizerScreen (S5 T2-M rewrite) ────────────────────────────────────────
// Real-time 516-partial spectrogram. Polls BaySickSolsticeSynth at 30 Hz and renders
// vertical amber bars. Aggregation is done in BaySickSolsticeSynth - this component
// only owns a smoothing buffer + the render.
//
// Smoothing: each tick the new per-partial amplitude gets max-merged with a
// decaying-by-kDecay version of the previous tick's value, so the visual has
// natural "peak fall" behaviour without jittering.
//
// Thread safety: the aggregator reads voices' amplitude arrays on the GUI
// thread. The audio thread writes those arrays via copyStateFrom during
// note-on / voice-engine rebuilds - torn reads at 30 Hz polling rate are
// not visually detectable. No locking.
// ─────────────────────────────────────────────────────────────────────────────
class VisualizerScreen : public juce::Component,
                         private juce::Timer
{
public:
    VisualizerScreen()
    {
        mAmplitudes.fill (0.0f);
        startTimerHz (30);
    }

    void setSynth (const BaySickSolsticeSynth* s) noexcept { mSynth = s; }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        // Pitch-black background with subtle 4-line vertical grid.
        g.setColour (juce::Colour (0xFF000000));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (juce::Colour (0xFF111113));
        for (int i = 1; i < 4; ++i)
        {
            const float y = bounds.getHeight() * float (i) / 4.0f;
            g.drawHorizontalLine (int (y), bounds.getX(), bounds.getRight());
        }

        const int numBars = HarmonicEngine::kNumPartials;   // 516
        const float bw    = bounds.getWidth() / float (numBars);
        const float h     = bounds.getHeight() - 4.0f;

        // Amplitude -> bar height mapping:
        //   Use approximate dB scale so small partials are visible.
        //   floor of -60 dB (amp = 0.001); 0 dB full height.
        auto ampToBarH = [h] (float amp) -> float
        {
            if (amp <= 1.0e-4f) return 0.0f;
            const float dB = 20.0f * std::log10 (amp);
            const float normalised = juce::jlimit (0.0f, 1.0f, (dB + 60.0f) / 60.0f);
            return normalised * h;
        };

        const auto orange = juce::Colour (0xFFFF6600);
        for (int i = 0; i < numBars; ++i)
        {
            const float amp = mAmplitudes[(size_t) i];
            const float barH = ampToBarH (amp);
            if (barH < 0.5f) continue;
            const float bx   = bounds.getX() + float (i) * bw;
            const float by   = bounds.getBottom() - barH - 2.0f;
            g.setColour (orange.withAlpha (0.9f));
            g.fillRect (bx, by, juce::jmax (0.5f, bw - 0.15f), barH);
        }

        // Border
        g.setColour (juce::Colour (0xFF2A2A2C));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
    }

private:
    void timerCallback() override
    {
        if (mSynth == nullptr)
        {
            repaint();
            return;
        }
        std::array<float, HarmonicEngine::kNumPartials> fresh {};
        // Aggregator is declared on BaySickSolsticeSynth; forward-declared above, so
        // the method call lives in the .cpp? Actually no - this header calls
        // into BaySickSolsticeSynth so we need the full type. See VisualizerScreen.cpp
        // for the timer body. (Timer callback is kept out-of-line so we don't
        // pull BaySickSolsticeSynth.h into every includer of this header.)
        tickFromSynth (fresh.data(), HarmonicEngine::kNumPartials);

        constexpr float kDecay = 0.85f;
        for (size_t i = 0; i < mAmplitudes.size(); ++i)
            mAmplitudes[i] = juce::jmax (fresh[i], mAmplitudes[i] * kDecay);

        repaint();
    }

    // Out-of-line so BaySickSolsticeSynth's full type only needs to be visible in
    // VisualizerScreen.cpp, not in every translation unit that includes this.
    void tickFromSynth (float* outBuf, int numPartials);

    const BaySickSolsticeSynth* mSynth { nullptr };
    std::array<float, HarmonicEngine::kNumPartials> mAmplitudes {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VisualizerScreen)
};
