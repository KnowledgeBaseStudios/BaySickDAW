#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// BaySickAlignDSP - Phase H-6a (2026-05-01), QA-F Task 3 build-out (2026-07-09)
// ─────────────────────────────────────────────────────────────────────────────
// Offline time-alignment engine: computes a warp map that morphs a follower
// ("dub") take onto a leader ("guide") take's timing, by pairing transient
// onsets in both signals into a piecewise-linear time-warp function, plus a
// per-anchor pitch delta (leader F0 vs follower F0) so the +Pitch presets
// can pull the follower's pitch contour toward the leader's.
//   * Onset detection via spectral-flux peak picking on a Hann-windowed STFT.
//   * Frame-YIN F0 estimation at each anchor for the pitch deltas.
//   * `PhaseVocoder` time-stretch + the PitchShifters trio for the offline
//     applyWarp render.
//
// Save model = Option C (per H-6a locked spec, confirmed by the G2-warp
// design lock 2026-07-09):
//   * `WarpMap` + sync points + protected areas persist as project XML on
//     the owning BaySickVocalProcessor (<AlignEdits>).
//   * Render bakes the aligned audio to
//     `<project>/Aligned/{name}_align_v{N}.wav`; playback then plays the
//     baked file (no realtime warp DSP -- the whole pipeline is offline,
//     message-thread only).
//
// The DSP class is GUI-agnostic: the BaySickAlign editor feeds it channel
// composites (VibeSynthProcessor::renderChannelComposite) and drives
// analyzeOffline / applyWarp from user actions.
// ─────────────────────────────────────────────────────────────────────────────

struct WarpAnchor
{
    double dubTimeSec   { 0.0 };   // source (follower) time at this anchor
    double guideTimeSec { 0.0 };   // target (leader) time at this anchor
    float  weight       { 1.0f };  // confidence 0..1; UI may show as anchor opacity
    // QA-F Task 3: leader-minus-follower pitch delta at this anchor,
    // semitones, already scaled by the Pitch box Range amount at analysis
    // time.  0 = no pitch pull (unvoiced or Range 0).
    float  pitchSemis   { 0.0f };
};

// QA-F Task 3: user-placed manual anchor (Sync Point, section 13c).  Hard
// pairing boundary: auto-pairing only pairs onsets BETWEEN sync points, and
// each sync point becomes a full-weight anchor itself.
struct AlignSyncPoint
{
    double followerSec { 0.0 };
    double leaderSec   { 0.0 };

    juce::ValueTree toValueTree() const
    {
        juce::ValueTree v ("SP");
        v.setProperty ("f", followerSec, nullptr);
        v.setProperty ("l", leaderSec,   nullptr);
        return v;
    }
    static AlignSyncPoint fromValueTree (const juce::ValueTree& v)
    {
        AlignSyncPoint p;
        p.followerSec = (double) v.getProperty ("f", 0.0);
        p.leaderSec   = (double) v.getProperty ("l", 0.0);
        return p;
    }
};

// QA-F Task 3: follower-side region exempted from warp and/or pitch
// (Protected Area, section 13c).  Right-click toggles which dimensions the
// area protects.
struct AlignProtectedArea
{
    double startSec     { 0.0 };
    double endSec       { 0.0 };
    bool   protectTime  { true };
    bool   protectPitch { true };

    juce::ValueTree toValueTree() const
    {
        juce::ValueTree v ("PA");
        v.setProperty ("s",  startSec,          nullptr);
        v.setProperty ("e",  endSec,            nullptr);
        v.setProperty ("t",  protectTime  ? 1 : 0, nullptr);
        v.setProperty ("p",  protectPitch ? 1 : 0, nullptr);
        return v;
    }
    static AlignProtectedArea fromValueTree (const juce::ValueTree& v)
    {
        AlignProtectedArea a;
        a.startSec     = (double) v.getProperty ("s", 0.0);
        a.endSec       = (double) v.getProperty ("e", 0.0);
        a.protectTime  = ((int) v.getProperty ("t", 1)) != 0;
        a.protectPitch = ((int) v.getProperty ("p", 1)) != 0;
        return a;
    }
};

struct WarpMap
{
    // Anchors sorted by dubTimeSec ascending.  Two anchors at index i and i+1
    // define a piecewise-linear segment: dub[a..b] -> guide[c..d] at the
    // ratio (d - c) / (b - a) per block.  Values outside [0..duration] are
    // clamped at playback time.
    std::vector<WarpAnchor> anchors;

    // Source / target durations recorded at analysis time (seconds).
    double dubDurationSec   { 0.0 };
    double guideDurationSec { 0.0 };

    // Wraps the project sampleRate the warp was authored against.  Used for
    // sample-rate-aware playback when the project rate differs from analysis.
    double analysisSampleRate { 44100.0 };

    bool isValid() const noexcept { return anchors.size() >= 2; }

    // Compute the source-side stretch ratio at a given dub time:
    //    ratio = d(guideTime)/d(dubTime) at this position
    // Returns 1.0 outside any anchor range (passthrough).
    double getStretchRatioAt (double dubTimeSec) const noexcept;

    // Map a dub-side time to its guide-side equivalent (piecewise linear).
    double mapDubToGuide (double dubTimeSec) const noexcept;

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& v);
};

class BaySickAlignDSP
{
public:
    BaySickAlignDSP();
    ~BaySickAlignDSP() = default;

    void prepare (double sampleRate, int maxBlockSize);
    void releaseResources();

    // ── Offline analysis ─────────────────────────────────────────────────────
    // Both buffers must be MONO (channel composites are already mono).
    // Returns a populated WarpMap on success; empty (anchors.size() < 2) on
    // failure (e.g. no transients detected, signals too quiet, etc.).
    //
    // strength = 0..1 blend between the follower's natural timing (0) and a
    // full snap of every paired onset to the leader's timing (1).
    //
    // pairingToleranceSec = max time difference between a follower onset and
    // a leader onset that may be paired -- the Fine Tune "max difference"
    // control (Mode base 150/100/50 ms +/- 50 ms, section 13e).
    //
    // syncPoints (section 13c): hard boundaries -- auto-pairing only pairs
    // onsets between consecutive sync points, and each sync point lands as a
    // full-weight anchor of its own (strength does not dilute user intent).
    //
    // protectedAreas (section 13c): follower-side regions post-processed out
    // of the map -- protectTime forces ratio 1 through the region (offset
    // continuity preserved); protectPitch zeroes anchor pitch deltas inside.
    //
    // pitchRange01 = the Pitch box Range amount (0..1).  > 0 runs frame-YIN
    // at every anchor in both composites and stores the scaled leader-minus-
    // follower delta on the anchor (0 when either side is unvoiced).
    static WarpMap analyzeOffline (const float* guideMono, int numGuideSamples,
                                    const float* dubMono,   int numDubSamples,
                                    double sampleRate,
                                    float  strength            = 1.0f,
                                    double pairingToleranceSec = 0.5,
                                    const std::vector<AlignSyncPoint>&     syncPoints     = {},
                                    const std::vector<AlignProtectedArea>& protectedAreas = {},
                                    float  pitchRange01        = 0.0f);

    // ── Map state (message-thread convenience mirror) ────────────────────────
    void setWarpMap (const WarpMap& map);
    void clearWarpMap();
    bool hasWarpMap() const noexcept;

    // ── Offline warp render (QA-F Task 3 -- replaces the H-6a passthrough) ──
    // MESSAGE/WORKER THREAD ONLY (allocates; PhaseVocoder passes).  Renders
    // the follower composite onto the leader timeline:
    //   * per anchor segment: PhaseVocoder time-stretch at dGuide/dDub with
    //     an exact-length map (effective-hop-corrected), 5 ms crossfades at
    //     segment seams;
    //   * then one pitch pass over the warped result applying the anchors'
    //     pitch deltas (lerped between anchors in guide time) via the chosen
    //     algo: 0 = PSOLA, 1 = Granular, 2 = Phase Vocoder (per-segment
    //     constant delta).
    // extraSemis = flat transpose added on top of the anchor deltas (the
    // Pitch box Transpose control).
    // Returns the warped mono buffer (length = mapDubToGuide(dubDuration)).
    // An invalid map returns a straight copy.
    static juce::AudioBuffer<float> applyWarp (const float* dubMono, int numDubSamples,
                                               double sampleRate,
                                               const WarpMap& map,
                                               int pitchAlgo,
                                               float extraSemis = 0.0f);

    // QA-F Task 4: coarse frame-YIN F0 track (one value per hop; 0 =
    // unvoiced frame).  Feeds the editor's Pitch view mode.  MESSAGE THREAD.
    static void estimateF0Track (const float* mono, int numSamples,
                                 double sampleRate, int hopSamples,
                                 std::vector<float>& outHz);

private:
    // ── State (audio thread reads via load) ─────────────────────────────────
    // Active map is kept in a heap-allocated holder swapped via wait-free
    // atomic exchange so message-thread setWarpMap doesn't block audio.
    std::shared_ptr<const WarpMap> mActiveMap;
    juce::SpinLock                 mMapLock;   // setWarpMap write-side only

    double mSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickAlignDSP)
};
