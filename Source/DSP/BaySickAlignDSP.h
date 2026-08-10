#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <functional>
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
// Playback model (QA-Fa recovery, 2026-07-10 -- restores the locked May
// design, phantom-recording-mongoose section 5):
//   * `WarpMap` + sync points + protected areas persist as project XML on
//     the owning BaySickVocalProcessor (<AlignEdits>).
//   * Applied maps play LIVE: the clip-decode layer (decodeFilePlayClip)
//     remaps the follower channel's FilePlay read position through the
//     published AlignPlaySnapshot; the PhaseVocoder compounds warp slope x
//     tempo-stretch x per-anchor pitch ratio in one pass.
//   * Render is EXPORT ONLY: bakes to
//     `<project>/Aligned/{name}_align_v{N}.wav` + a history entry; nothing
//     is placed on the grid and playback does not change.
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
    // QA-Fd: RAW leader-minus-follower pitch delta at this anchor, semitones
    // (sampled at the raw pair positions, not the capped anchor position).
    // 0 = unvoiced on either side.  Blend/Variation apply at PUBLISH time
    // (BaySickVocalProcessor::effectiveAlignPitchSemis) so the pitch knobs
    // are live without re-analysis.  Pre-QA-Fd saves carry Range-scaled
    // values here; they read slightly under-pulled until the next analyze.
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

// Filled by analyzeOffline so a failed analysis can say WHY (onsets found
// per side, pairs within the internal matching window) instead of one
// generic line.  toleranceSec reports the internal window (QA-Fd: matching
// is no longer Mode-driven; Mode/Fine control residual tightness instead).
struct AlignAnalyzeDiag
{
    int    guideOnsets  { 0 };
    int    dubOnsets    { 0 };
    int    pairs        { 0 };
    double toleranceSec { 0.0 };
};

// QA-Fd (locked 13a): per-side pitch-detection bands for the anchor deltas.
// Combo order = param order (bsa_pitch_typeGuide / bsa_pitch_typeDub 0..4).
// Hz calibrations tuned at the G2 boundary re-listen.  Note the frame-YIN
// window is 2048 samples, so the practical low bound at 44.1 kHz is ~43 Hz
// regardless of the band's stated minimum.
struct AlignPitchBand { const char* name; float minHz; float maxHz; };
constexpr AlignPitchBand kAlignPitchBands[5] = {
    { "Normal",          60.0f,  1200.0f },
    { "High Vocal",      160.0f, 1400.0f },
    { "Low Vocal",       50.0f,  500.0f  },
    { "High Instrument", 200.0f, 2400.0f },
    { "Low Instrument",  30.0f,  350.0f  },
};

// QA-Fd align semantics rework (locked 9a/10a/14a/15a): analysis-time knobs.
// Matching is INTERNAL (wide fixed window); these control what the map DOES
// with the matched pairs.
struct AlignBuildParams
{
    // Residual tightness cap R (Mode/Fine): a paired word already within R
    // of the leader keeps its natural timing; beyond R it is pulled to the
    // cap edge.  0 = fully locked (every pair lands on the leader).
    double residualCapSec { 0.05 };
    // Per-word movement cap M: move = min(max(|d| - R, 0), M).
    double maxShiftSec    { 0.4 };
    // Segment-slope bound [1/r, r] at map build; auto anchors whose segment
    // demands more are dropped.  <= 1 = unbounded (Max All; the snapshot
    // lookup clamp still rails at 1/64..64).
    double flexRatio      { 2.0 };
    // Per-side YIN detection bands for the anchor pitch deltas.
    float  guideMinHz { 60.0f }, guideMaxHz { 1200.0f };
    float  dubMinHz   { 60.0f }, dubMaxHz   { 1200.0f };
    // QA-Fd (locked 5/13a): the follower's pitch-tab time edits redefine
    // the performance align matches -- this transforms a RAW follower
    // composite time into its EDITED performance time.  Null = identity.
    // Applied to detected dub onsets, sync-point follower times, and the
    // dub duration; anchor dub times land in EDITED time (what the decode
    // layer's composed law expects), while anchor pitch deltas still sample
    // the RAW audio at the raw positions.
    std::function<double(double)> dubEditTransform;
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

    // Map a dub-side time to its guide-side equivalent (piecewise linear).
    double mapDubToGuide (double dubTimeSec) const noexcept;

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& v);
};

// ── AlignPlaySnapshot ────────────────────────────────────────────────────────
// QA-Fa recovery (2026-07-10): immutable audio-thread view of an APPLIED warp
// map, published by BaySickVocalProcessor via atomic pointer swap (retire-ring
// liveness contract identical to BaySickPitchDSP::Snapshot -- the decode layer
// holds the pointer only within one block).  Guide-axis SoA arrays so the
// per-block lookup is one binary search; guide times are monotone-clamped at
// publish, so mapping is well-defined even on a degenerate analysis.
struct AlignPlaySnapshot
{
    // Parallel arrays sorted by guideSec ascending (composite common-origin
    // seconds).  dubSec = follower-side time; pitchSemis = EFFECTIVE pull at
    // the anchor (raw map delta through the Variation cap + Blend percent,
    // applied at publish -- QA-Fd 12a live-knob contract).
    std::vector<double> guideSec;
    std::vector<double> dubSec;
    std::vector<float>  pitchSemis;

    // Monotone-cubic tangents (dDub/dGuide per anchor, Fritsch-Butland),
    // computed once at publish.  The piecewise-LINEAR lookup stepped the
    // playback rate at every anchor -- audible chop at onset cadence, worst
    // under wide pairing windows (G2 boundary ear verdict).  Empty = the
    // lookup falls back to linear.
    std::vector<double> tangent;
    void computeTangents();

    int          followerChannelId  { -1 };
    double       commonStartBeat    { 0.0 };
    juce::int64  commonStartSample  { 0 };     // timeline sample of composite t=0 (analysis-time device rate)
    double       analysisSampleRate { 44100.0 };
    bool         anyPitch           { false };

    bool isUsable() const noexcept { return guideSec.size() >= 2 && followerChannelId >= 0; }

    // Piecewise-linear lookup at a guide-domain time: the follower-side read
    // time, the local dDub/dGuide slope, and the lerped pitch delta.  Outside
    // the anchor range: offset-continued at slope 1, pitch 0.
    void lookupAtGuideSec (double g, double& outDubSec,
                           double& outDubPerGuide, float& outSemis) const noexcept
    {
        const size_t n = guideSec.size();
        if (n < 2)
        {
            outDubSec = g; outDubPerGuide = 1.0; outSemis = 0.0f;
            return;
        }
        if (g <= guideSec.front())
        {
            outDubSec = dubSec.front() + (g - guideSec.front());
            outDubPerGuide = 1.0;
            outSemis = 0.0f;
            return;
        }
        if (g >= guideSec.back())
        {
            outDubSec = dubSec.back() + (g - guideSec.back());
            outDubPerGuide = 1.0;
            outSemis = 0.0f;
            return;
        }
        size_t lo = 0, hi = n - 1;
        while (hi - lo > 1)
        {
            const size_t mid = (lo + hi) / 2;
            if (guideSec[mid] <= g) lo = mid; else hi = mid;
        }
        const double gSpan = juce::jmax (1.0e-9, guideSec[hi] - guideSec[lo]);
        const double f     = (g - guideSec[lo]) / gSpan;
        if (tangent.size() == n)
        {
            // Cubic Hermite between anchors: C1-continuous position AND
            // slope, so the decode rate glides through anchors instead of
            // stepping at each one.
            const double t2  = f * f, t3 = t2 * f;
            const double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
            const double h10 = t3 - 2.0 * t2 + f;
            const double h01 = -2.0 * t3 + 3.0 * t2;
            const double h11 = t3 - t2;
            const double y0  = dubSec[lo],   y1 = dubSec[hi];
            const double m0  = tangent[lo],  m1 = tangent[hi];
            outDubSec = h00 * y0 + h10 * gSpan * m0
                      + h01 * y1 + h11 * gSpan * m1;
            const double d00 = 6.0 * t2 - 6.0 * f;
            const double d10 = 3.0 * t2 - 4.0 * f + 1.0;
            const double d11 = 3.0 * t2 - 2.0 * f;
            outDubPerGuide = juce::jlimit (1.0 / 64.0, 64.0,
                (d00 * y0 + d10 * gSpan * m0
                 - d00 * y1 + d11 * gSpan * m1) / gSpan);
        }
        else
        {
            outDubSec      = dubSec[lo] + f * (dubSec[hi] - dubSec[lo]);
            outDubPerGuide = juce::jlimit (1.0 / 64.0, 64.0,
                                           (dubSec[hi] - dubSec[lo]) / gSpan);
        }
        outSemis = pitchSemis[lo] + (float) f * (pitchSemis[hi] - pitchSemis[lo]);
    }
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
    // QA-Fd semantics rework (locked 9a/10a): onset matching runs inside a
    // FIXED internal window (kMatchWindowSec); params.residualCapSec /
    // maxShiftSec / flexRatio shape the resulting map (see AlignBuildParams).
    //
    // syncPoints (section 13c): hard boundaries -- auto-pairing only pairs
    // onsets between consecutive sync points, and each sync point lands as a
    // full-weight anchor of its own (caps do not dilute user intent).
    //
    // protectedAreas (section 13c): follower-side regions post-processed out
    // of the map -- protectTime forces ratio 1 through the region (offset
    // continuity preserved); protectPitch zeroes anchor pitch deltas inside.
    //
    // Anchor pitch deltas are ALWAYS computed and stored RAW (leader minus
    // follower, sampled at the raw pair positions through the per-side
    // bands); Blend/Variation apply at publish so the pitch knobs stay live.
    static constexpr double kMatchWindowSec = 0.4;
    static WarpMap analyzeOffline (const float* guideMono, int numGuideSamples,
                                    const float* dubMono,   int numDubSamples,
                                    double sampleRate,
                                    const AlignBuildParams& params = {},
                                    const std::vector<AlignSyncPoint>&     syncPoints     = {},
                                    const std::vector<AlignProtectedArea>& protectedAreas = {},
                                    AlignAnalyzeDiag*       diagOut = nullptr);

    // ── Map state (message-thread convenience mirror) ────────────────────────
    void setWarpMap (const WarpMap& map);
    void clearWarpMap();

    // ── Offline warp render (QA-Fd Task 8 smooth-map port) ──────────────────
    // MESSAGE/WORKER THREAD ONLY (allocates; PhaseVocoder passes).  Renders
    // the follower composite onto the leader timeline:
    //   * Phase 1: ONE streaming PhaseVocoder whose ratio follows the same
    //     monotone-cubic (Fritsch-Butland) slope the live decode glides on --
    //     renders no longer step at anchors (the old per-anchor-segment
    //     assembly ran each segment at a constant ratio).
    //   * Phase 2: pitch pass over the warped result applying the anchors'
    //     pitch deltas (lerped in guide time) via the chosen algo:
    //     0 = PSOLA, 1 = Granular, 2 = Phase Vocoder.
    // extraSemis = flat transpose added on top of the anchor deltas.
    // oversample > 1 (QA-Fd 16a High-Res): Phase 1 runs at sampleRate *
    // oversample (~384 kHz-class at 8x) and downsamples before Phase 2 (the
    // pitch algos stay at the device rate -- their trackers assume it).
    // Returns the warped mono buffer (length = mapDubToGuide(dubDuration)).
    // An invalid map returns a straight copy.
    static juce::AudioBuffer<float> applyWarp (const float* dubMono, int numDubSamples,
                                               double sampleRate,
                                               const WarpMap& map,
                                               int pitchAlgo,
                                               float extraSemis = 0.0f,
                                               int oversample = 1);

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
