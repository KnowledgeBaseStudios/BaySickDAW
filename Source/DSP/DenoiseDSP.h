#pragma once
#include <JuceHeader.h>
#include <vector>

// De-noise: profile-based spectral subtraction for recorded takes (QA-Fe2).
// Three pieces: DenoiseProfile (a per-bin noise-floor fingerprint, project-
// persistable), DenoiseLearner (feeds on live audio, tracks the room floor),
// and Denoise::cleanFile (offline file->file cleaner, output sample-count
// identical to input so every grid/beat/align offset stays valid).
// Ear-validated on the WORLD buzz/water arc (running notes 2026-07-16,
// "cleantake"): the water artifact is the take's noise layer re-rendered
// F0-gated by WORLD's synthesis; removing it at the source is the fix.

struct DenoiseProfile
{
    static constexpr int kFFT  = 1024;
    static constexpr int kBins = kFFT / 2 + 1;

    double sampleRate = 0.0;
    std::vector<float> mag;          // per-bin linear magnitude floor

    bool isValid() const noexcept    { return sampleRate > 0.0 && (int) mag.size() == kBins; }

    juce::String toBase64() const;
    static DenoiseProfile fromBase64 (const juce::String& s);
};

// Rolling room-floor tracker.  pushBlock is audio-thread wait-free (ring
// write + release publish); processPending does the FFT work and runs on a
// timer/background thread.  Asymmetric per-bin follower (fast down, slow up)
// tracks minima so speech never drags the floor up; near-digital-silence
// frames are skipped so an idle (unrouted) strip never drags it to zero.
class DenoiseLearner
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;

    void pushBlock (const float* mono, int n) noexcept;   // audio thread
    void processPending();                                // message/bg thread

    int framesLearned() const noexcept  { return mFrames.load (std::memory_order_acquire); }
    DenoiseProfile snapshot() const;

private:
    static constexpr int kHop = 256;

    double mSampleRate = 0.0;
    std::vector<float> mRing;        // 4 s mono ring
    std::atomic<juce::int64> mWrite { 0 };
    juce::int64 mRead = 0;

    std::unique_ptr<juce::dsp::FFT> mFFT;
    std::vector<float> mWindow, mFrame, mFftData;
    std::vector<float> mFloor;
    std::atomic<int> mFrames { 0 };

    mutable juce::CriticalSection mFloorLock;   // snapshot vs processPending (both non-audio)
};

namespace Denoise
{
    enum Strength { Light = 0, Strong = 1 };

    // Learn a profile from a file's own quietest frames -- the fallback when
    // no live profile exists (recorded before monitoring warmed up, Inst
    // strips, regenerate-without-stored-profile).  This offline self-learn is
    // the method the cleantake prototype validated.
    DenoiseProfile learnFromFile (const juce::File& in);

    // Clean in -> out.  Output length/rate/bit-depth match the input exactly.
    // Returns false + errorOut on failure; never leaves a partial out file.
    bool cleanFile (const juce::File& in, const juce::File& out,
                    const DenoiseProfile& profile, Strength strength,
                    juce::String& errorOut);
}
