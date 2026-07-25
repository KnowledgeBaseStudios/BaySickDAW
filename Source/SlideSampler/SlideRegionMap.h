#pragma once
#include <JuceHeader.h>
#include <vector>

// ── SlideRegionMap ───────────────────────────────────────────────────────────
// QA-SlideSampler Task 1 (silky-gliding-lynx) + QA-G3Smoke Task 10 (G-1/G-11):
// the extracted program the voiced SlideSampler plays for fretboard slides on
// the sfizz-backed Guitars/Basses engines.  sfizz stays the engine for every
// NORMAL note; the slide gesture routes to the purpose-built sampler.
//
// Task 10 rework: extraction now captures the FULL patch voicing per zone
// (gain staging, velocity curves, envelopes with their CC modulations, LFO
// banks, the bass filter block, custom curve tables, group/choke policy,
// unison opcodes, bend ranges) across EVERY articulation (keyed sw_last;
// keyswitch-less programs extract their single articulation so slides work on
// the staccato/twang-only programs too), for the layer sets {center, tUp/tDn
// unison, tailpiece, feedback, noise, release}.  The voiced voice DSP
// (Task 11) consumes this; until it lands, SlideSampler plays the DEFAULT
// articulation's center set exactly as before (the `samples` mirror below).
//
// Disk facts that shaped the original design (2026-07-21, read off the maps):
//  - SS-Q1: main-sustain regions are one-shots (no loop_*).  The NOISE layer
//    is the loop_mode exception, captured as its own set.
//  - The maps are FULLY CHROMATIC (one zone per semitone) -> at-or-below +
//    <=1-semi up-bend always works.
//  - Unison t-layers reference NEIGHBORING keys of the same sample pool
//    (path-keyed cache dedupes -> ~zero extra decoded RAM).
// ─────────────────────────────────────────────────────────────────────────────

// One captured zone: a recorded sample + its EFFECTIVE (scope-inherited)
// voicing.  Static scalars are typed; the modulation-family opcodes
// (ampeg_*_oncc / lfoNN_* / pitcheg_* / fileg_* / cutoff* / var* / *_oncc*)
// are carried verbatim in `modOps` -- Task 11's mod router parses them ONCE at
// setProgram (message thread) into its runtime tables, so extraction stays
// exhaustive without freezing the runtime schema here.
struct SlideSample
{
    int rootKey  = 60;          // pitch_keycenter
    int loKey    = 0;
    int hiKey    = 127;
    int loVel    = 1;
    int hiVel    = 127;
    int offsetFrames = 0;       // SFZ `offset` (noise/tailpiece sets carry real values)
    int loopStart = -1;
    int loopEnd   = -1;
    int loopMode  = 0;          // 0 = no_loop/one-shot, 1 = loop_continuous (noise layer)
    int tuneCents = 0;

    // Gain staging (Task 12 replaces the jlimit(0.05,1,vel/100) stopgap).
    float volumeDb     = 0.0f;  // `volume` (bass common.sfz carries 3)
    float amplitudePct = 100.0f;// `amplitude`
    float ampVeltrack  = 100.0f;// `amp_veltrack`
    // amp_velcurve_N points (vel 1..127 -> gain 0..1); empty = default curve.
    std::vector<std::pair<int, float>> velcurve;

    // Effective AHDSR statics (ampeg_*).  CC-modulated terms ride modOps.
    float ampegDelay = 0, ampegAttack = 0, ampegHold = 0, ampegDecay = 0;
    float ampegSustain = 100.0f, ampegRelease = 0;

    // Group / choke policy (bass cc105 Mono + kit-internal chokes).
    int   group = 0, offBy = 0, notePolyphony = 0;
    float offTime = 0.0f, rtDecay = 0.0f;

    // Every captured modulation-family opcode, effective at this zone, in
    // (key, value) form.  Includes: ampeg_*_oncc*/_curvecc*, lfoNN_* (freq /
    // delay / fade / wave / phase / pitch / volume / cutoff / cross-LFO
    // `lfoNN_freq_lfoMM_onccK` / extended-CC 131 velocity, 133 note, 135/136
    // random), pitcheg_*, fileg_*, cutoff/cutoff2/fil_keytrack/fil2_type/
    // resonance*, var01_*/var02_*, offset_oncc*, amplitude_oncc*, pan_oncc*,
    // tune_cc*/tune_oncc*.
    std::vector<std::pair<juce::String, juce::String>> modOps;

    juce::File wav;             // resolved absolute path (existence-checked)
};

// A custom <curve> table: curve_index + (x = v position 0..127 scaled 0..1,
// y = value) breakpoints.  Linear between points (SFZ semantics).
struct SlideCurve
{
    int index = -1;
    std::vector<std::pair<float, float>> points;   // ascending x in 0..1
};

// One articulation's full layer stack.
struct SlideArticulation
{
    int          swLast = -1;    // keyswitch note; -1 = the program's single articulation
    juce::String label;          // sw_label when present (diagnostics / future UI)

    // SL-6 bend range (cents) captured from this articulation's center regions.
    static constexpr int kBendUnset = -100000;
    int bendUpCents   { kBendUnset };
    int bendDownCents { kBendUnset };

    std::vector<SlideSample> center;      // main voice (all bands, default RR)
    std::vector<SlideSample> tUp;         // unison layer sampled from the key ABOVE
    std::vector<SlideSample> tDown;       // unison layer sampled from the key BELOW
    std::vector<SlideSample> tailpiece;   // guitar cc118-high alternate table
    std::vector<SlideSample> feedback;    // cc29-gated pitched feedback layer
    std::vector<SlideSample> noise;       // looped noise layer (loop_mode set)
    std::vector<SlideSample> releases;    // trigger=release maps (cc27/cc107 gated)

    // Bass cc105 Mono policy captured off the mono region set (the zones
    // themselves come from the poly set; Task 12 honors these chokes).
    bool  hasMonoSet   = false;
    int   monoGroup    = 0;
    int   monoOffBy    = 0;
    float monoOffTime  = 0.0f;
};

// The extracted program.  Rebuilt on every kit/program swap.
struct SlideRegionMap
{
    // ── Task 10 full structure ───────────────────────────────────────────
    std::vector<SlideArticulation> articulations;   // ascending swLast; -1 first
    int                            defaultArticulation = -1;   // index into articulations
    std::vector<SlideCurve>        curves;           // custom <curve> tables
    juce::String                   sourceProgram;

    // ── Compat mirror (pre-Task-11 consumers) ────────────────────────────
    // The DEFAULT articulation's center set + its bend range, exactly the
    // shape the lynx SlideSampler/StandaloneEditor already consume.  Filled
    // by extractSlideRegions; Task 11 moves consumers onto `articulations`.
    std::vector<SlideSample> samples;
    static constexpr int kBendUnset = SlideArticulation::kBendUnset;
    int bendUpCents   { kBendUnset };
    int bendDownCents { kBendUnset };

    bool isEmpty() const noexcept { return samples.empty(); }

    int numBands() const noexcept;
    int bendMaxUpSemis()   const noexcept;
    int bendMaxDownSemis() const noexcept;
};

// Parse `programSfz`, expand its #include chain (depth-bounded), and return the
// full multi-articulation extraction.  Include paths resolve relative to each
// including file; SFZ `sample=` paths resolve relative to the PROGRAM file's
// directory (sfizz's base).  The parser tokenizes MULTI-OPCODE lines and
// opcodes that share a line with a <header> (Task 10 hardening -- the shipped
// karoryfer maps are one-per-line, but program/control files are now read too).
SlideRegionMap extractSlideRegions (const juce::File& programSfz);
