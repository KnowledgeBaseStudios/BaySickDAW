#pragma once
#include <JuceHeader.h>
#include <memory>

// ─────────────────────────────────────────────────────────────────────────────
// PolyphaseOversampler4x — Phase I-2 (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// Shared 4x oversampling helper for non-linear DSP modules (BaySickPedals
// drive / distortion / fuzz / fuzz-octave / multi-band drive).  Centralises
// the construction-arg drift across modules: every consumer gets the same
// filter type, same numStages (=2 halving stages), and same isMaxQuality
// flag that BaySickNAM/IR's oversampling chain uses.
//
// Usage pattern (mono):
//   PolyphaseOversampler4x os;
//   os.prepare (1, maxBlockSize);
//   ...
//   void processBlock (juce::AudioBuffer<float>& buf) {
//     juce::dsp::AudioBlock<float> block (buf);
//     auto upBlock = os.upsample (block);   // 4*N samples, same channels
//     // ... your non-linear DSP at 4x sample rate on upBlock ...
//     os.downsample (block);                // 1x output back into buf
//   }
//
// PDC: caller queries `getLatencySamples()` and forwards into DSPBase's
// `getLatencySamples()` so EffectRack can sum per-slot latency for the
// PluginProcessor's setLatencySamples.
//
// Header-only -- the implementation is a thin wrapper around
// juce::dsp::Oversampling<float> with our locked constructor args.
// ─────────────────────────────────────────────────────────────────────────────

class PolyphaseOversampler4x
{
public:
    PolyphaseOversampler4x() = default;
    ~PolyphaseOversampler4x() = default;

    // numChannels: 1 (mono pre-sum) or 2 (stereo).  Most pedals oversample
    // post-mono-sum (numChannels=1) since their non-linear stages are mono.
    // maxBlockSize: largest host block size we'll see; passed verbatim to
    // initProcessing so internal buffers are sized correctly.
    void prepare (int numChannels, int maxBlockSize)
    {
        using OS = juce::dsp::Oversampling<float>;
        // numStages=2 = two halving filter stages = 4x oversampling factor.
        // filterHalfBandPolyphaseIIR is the cheap-and-clean default
        // (matches BaySickNAMIRProcessor::rebuildOversampling).
        // isMaxQuality=true leans toward fewer artifacts.
        mOS = std::make_unique<OS> ((size_t) numChannels,
                                    /*numStages*/ (size_t) 2,
                                    OS::filterHalfBandPolyphaseIIR,
                                    /*isMaxQuality*/ true);
        mOS->initProcessing ((size_t) maxBlockSize);
        mNumChannels = numChannels;
    }

    void reset()
    {
        if (mOS) mOS->reset();
    }

    bool isPrepared() const noexcept { return mOS != nullptr; }

    int  getNumChannels() const noexcept { return mNumChannels; }

    // Round-trip latency (in 1x samples) introduced by the polyphase IIR
    // chain.  Forward this from your DSPBase::getLatencySamples() so the
    // host's PDC sum is correct.
    int getLatencySamples() const noexcept
    {
        if (! mOS) return 0;
        return (int) std::round (mOS->getLatencyInSamples());
    }

    // Returns a 4x-sample-count block sharing channels with the input.
    // The returned block is owned by the internal Oversampling<float> --
    // do NOT outlive the next downsample() call.
    juce::dsp::AudioBlock<float> upsample (juce::dsp::AudioBlock<float> in)
    {
        jassert (mOS != nullptr);
        return mOS->processSamplesUp (in);
    }

    // Writes the downsampled result back into the original 1x block.  Caller
    // passes the same 1x block they upsampled (or any compatible block).
    void downsample (juce::dsp::AudioBlock<float> out)
    {
        jassert (mOS != nullptr);
        mOS->processSamplesDown (out);
    }

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> mOS;
    int mNumChannels { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolyphaseOversampler4x)
};
