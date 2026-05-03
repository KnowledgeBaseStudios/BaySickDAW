#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// BaySickAlignDSP — Phase H-6a (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────
// Offline time-alignment engine.  VocAlign-clone behaviour: computes a warp
// map that morphs a "dub" take onto a "guide" take's timing, by pairing
// transient onsets in both signals and producing a piecewise-linear time-warp
// function.  Reuses:
//   * Onset detection via spectral-flux peak picking on a Hann-windowed STFT
//     (lightweight, no extra dependencies; mirrors the YIN tracker's domain).
//   * `PhaseVocoder` for the realtime application of the warp map at playback
//     (per-block instantaneous stretch ratio derived from the warp).
//
// Save model = Option C (per H-6a locked spec):
//   * `WarpMap` is serialised as XML metadata on the project clip.
//   * Render bakes the aligned audio to a new `.wav` under
//     `<project>/Aligned/<recording>_aligned_v<N>.wav`.
//   * Per-clip flag `usesOfflineAlignment` set on render -- playback then
//     loads the rendered wav directly (no DSP cost).
//
// The DSP class is GUI-agnostic.  H-6c's BaySickAlign editor drives this
// via analyzeOffline() + commits a finished WarpMap; render path is run by
// the editor's "Render" button on the message thread (analyzes once, writes
// once; no audio-thread surface).  Playback applicator is provided here so
// projects can play aligned takes without re-rendering after every edit.
// ─────────────────────────────────────────────────────────────────────────────

struct WarpAnchor
{
    double dubTimeSec   { 0.0 };   // source (dub) time at this anchor
    double guideTimeSec { 0.0 };   // target (guide) time at this anchor
    float  weight       { 1.0f };  // confidence 0..1; UI may show as anchor opacity
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
    // Both buffers must be MONO (caller mixes stereo to mono if needed).
    // Returns a populated WarpMap on success; empty (anchors.size() < 2) on
    // failure (e.g. no transients detected, signals too quiet, etc.).
    //
    // strength = 0..1.  At 0, no warp is produced (pure passthrough map).
    // At 1.0, every dub onset is snapped tightly to the nearest guide onset.
    // Intermediate values blend between the dub's natural timing and the
    // guide's timing — matches VocAlign's "% align" knob.
    //
    // pairingToleranceSec = max time difference between a dub onset and a
    // guide onset that may be paired.  Default 0.5 sec covers typical vocal
    // ADR / BV stacks; longer windows risk wrong pairings on dense material.
    static WarpMap analyzeOffline (const float* guideMono, int numGuideSamples,
                                    const float* dubMono,   int numDubSamples,
                                    double sampleRate,
                                    float  strength            = 1.0f,
                                    double pairingToleranceSec = 0.5);

    // ── Playback application ─────────────────────────────────────────────────
    // Set the active warp map (call from the message thread between transport
    // stop / start).  Audio thread reads via load() inside processBlock.
    void setWarpMap (const WarpMap& map);
    void clearWarpMap();
    bool hasWarpMap() const noexcept;

    // Audio-thread mono-in / mono-out per-block applicator.  inSamples are
    // the source (dub) samples at the current playhead position; outSamples
    // are the warp-applied samples destined for the chain.  positionSec is
    // the current dub-side playhead in seconds (caller advances this).
    //
    // Returns the number of output samples actually written (may be < num
    // when the warp map ends).
    //
    // H-6a scope: stub passthrough that returns inSamples unchanged when
    // mWarpMap is empty.  PhaseVocoder integration for the actual stretch
    // lands in H-6c (UI batch) once the editor is wired -- the playback
    // path needs project-clip context to drive properly.
    int applyWarp (const float* in, float* out, int numSamples,
                   double positionSec) noexcept;

private:
    // ── State (audio thread reads via load) ─────────────────────────────────
    // Active map is kept in a heap-allocated holder swapped via wait-free
    // atomic exchange so message-thread setWarpMap doesn't block audio.
    std::shared_ptr<const WarpMap> mActiveMap;
    juce::SpinLock                 mMapLock;   // setWarpMap write-side only

    double mSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickAlignDSP)
};
