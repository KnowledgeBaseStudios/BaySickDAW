# DAW Architecture Research -- Octave / Pitch-Shift Guitar-Pedal Engine -- 2026-06-18

> Generated for QA-EffectsReview Task 5 Chunk C (Octave FFT/pitch-sync shifter build).
> Jeff chose the full shifter build over accept-faithful (2026-06-18).

## Problem statement
Replace the granular grain-shifter inside the OC-Style octave pedal (`OctaveStyleDSP`)
with a higher-fidelity engine. Priority is a tight, artifact-low octave-DOWN voice on
SINGLE notes (the signature -1 oct sound); secondary are an octave-UP voice and a
polyphonic chord-tracking mode, plus dry/shifted blend. Constraint: guitarists notice
added latency above ~10 ms, and all per-sample work lands on the audio thread.

## What BaySickDAW currently does

The shipping octave effect is `Source/DSP/OctaveStyleDSP.{h,cpp}` (Phase I-7), registered
as `EffectType::OctaveStyle = 110` (`Source/EffectRack.h:49`) and instantiated at
`Source/EffectRack.cpp:89`. It has two modes:

- **Polyphonic mode** -- a per-octave inline granular shifter
  (`OctaveStyleDSP::GranularShifter`, `OctaveStyleDSP.h:81-110`). Two read heads in an
  8192-sample ring (`kBufferSize = 8192`, ~186 ms), each reading at the pitch ratio
  (2.0 / 0.5 / 0.25), offset by half a grain and Hann-crossfaded
  (`OctaveStyleDSP.cpp:31-91`). Grain is `kGrainSize = 2048` (~46 ms). NO pitch track
  feeds in; it is a fixed-ratio time-domain resampler. This is the warble/smearing
  source the brief wants to fix -- a 2048-sample grain costs roughly half-a-grain of
  effective latency (~23 ms) and the head-jump-hidden-by-window scheme smears
  transients and produces the "mild warble" the header comment itself admits
  (`OctaveStyleDSP.h:16-18`).
- **Vintage mode** -- a low-CPU mono path: full-wave rectifier for +1 oct,
  Schmitt-trigger divide-by-2 / divide-by-4 for -1 / -2 oct
  (`OctaveStyleDSP.cpp:210-267`). Near-zero latency, "squelchy synthy" character, not
  hi-fi.

A `Range` first-order LP (300 Hz-3 kHz) pre-filters the shift paths
(`OctaveStyleDSP.cpp:144-151`), and 5 Hz DC blockers sit on the rectifier and the final
sum. Output sum is `dry*direct + p1*oct1Up + m1*oct1Down + m2*oct2Down`
(`OctaveStyleDSP.cpp:272-283`).

**Reusable assets already in the tree:**

- `Source/DSP/PhaseVocoder.{h,cpp}` -- Laroche-Dolson, FFT 2048 / hop 512 / 4x overlap /
  Hann. It is a TIME-STRETCH (pitch-preserving), NOT a pitch-shifter (`setStretchRatio`,
  `PhaseVocoder.h:37`). Its intrinsic latency is `kFFTSize - kHopSize = 1536` samples
  (~35 ms @ 44.1k, `PhaseVocoder.h:25`). Used only by the audio-clip stretch path. To make
  it shift pitch you stretch by 1/ratio then resample back -- doubling the cost and keeping
  the 1536-sample latency floor.
- `Source/DSP/PitchTrackerYIN.{h,cpp}` -- mono YIN, 2048 window / hop 512. Runs on a
  background worker thread (`PitchTrackerYIN.h:73-77`); audio thread only `pushAudio()`s
  into a lock-free `AbstractFifo`. Published latency ~50 ms. Range 40-1500 Hz.
- `Source/DSP/PolyPitchTracker.{h,cpp}` -- FFT 4096 / hop 1024, harmonic-sum + greedy
  spectral subtraction, up to 6 notes. Also a background worker
  (`PolyPitchTracker.h:14-18`); ~93 ms window. `getNotes()` is a wait-free seqlock read.
- **Proven integration precedent:** `Source/DSP/SynthStyleDSP.{h,cpp}` (the SY-style synth
  pedal, also a rack `DSPBase`) ALREADY wires both trackers exactly this way -- pushes mono
  on the audio thread (`SynthStyleDSP.cpp:295,332`), reads published Hz / NoteSet per block,
  and synthesizes oscillator voices from them (`allocateVoices`, `SynthStyleDSP.cpp:223`).
  This is the template for feeding a pitch track into a rack effect without blocking audio.
- `libs/rubberband` is vendored but headers-only, NOT linked -- CMake comment says
  "link rubberband.lib to enable DSP" (`CMakeLists.txt:465`). Using it needs a build-system
  change (lib build + license review; Rubber Band is GPL/commercial-dual).

## State of the art (reference behavior)

### The modern compact-octave reference (OC-5 class)
- Two distinct engines under one box. The mono octave-DOWN is a time-domain tracker that
  slips the lower octave under the original with effectively zero latency and tracks
  single notes flawlessly down to -2 oct without glitching. The polyphonic mode (and
  octave-UP) is a separate, always-poly engine that carries audibly more latency
  (~10 ms class). The two modes are NOT the same algorithm.
- Confidence MEDIUM (reviews confirm zero-latency mono octave-down + accurate tracking;
  the per-mode millisecond split is secondary snippets, not a vendor engineering doc).
- Validates a HYBRID: a dedicated low-latency mono path for the signature octave-down,
  a separate heavier poly path for chords/up.

### The clean poly-octave reference (POG class)
- Frequency-domain polyphonic octave generation -- NOT divide-down, NOT rectifier. DSP-forum
  reverse-engineering consensus: an FFT phase-vocoder with ~4x overlap, ~1024-2048 blocks.
  The poly version adds a "faded attack" control that ramps the onset of the octave voices,
  a detune, per-voice level, and an LP on the generated octaves -- i.e. it deliberately
  blends the DRY transient through while the smeared phase-vocoder octave fades in behind it.
- Confidence MEDIUM (community reverse-engineering, not official; the faded-attack / detune /
  LP feature set is well-corroborated).
- Confirms the production answer to phase-vocoder transient smearing is NOT a better
  transform -- it is (a) keep the FFT small for low latency, (b) blend the dry transient,
  (c) attack envelope + LP that hides the smear, (d) detune for thickness. Directly portable.

### TSM literature -- PSOLA/WSOLA (time-domain) vs phase vocoder (frequency-domain)
- Time-domain OLA/WSOLA/PSOLA: rearrange/overlap-add pitch-synchronous grains; cheap,
  low-latency, crisp transients on MONOPHONIC harmonic material, but fail on polyphony
  and stutter/flam transients without explicit transient detection. Frequency-domain phase
  vocoder: works on polyphony but induces phasiness and transient smearing. Best-known
  mitigation is harmonic-percussive separation (shift the harmonic part with the vocoder,
  pass the percussive/transient part through unshifted, recombine). PSOLA quality hinges on
  an accurate pitch mark / pitch track.
- Confidence HIGH for the WSOLA latency/transient mechanics; MEDIUM for the harmonic-
  percussive specifics.
- Time-domain latency is ~half the grain/window (a 2048 grain ~23 ms; drop to ~512-768 for
  ~6-9 ms). Octave-DOWN specifically is the friendly case (integer 2:1 ratio = emit each
  input period twice).

### Rubber Band Library (the vendored option)
- Production time-stretch/pitch-shift. R3 (Finer) is near-hi-fi but ~3x R2 CPU. Real-time
  pitch-shift latency for this class is ~30-50 ms.
- Confidence HIGH (changelog + API doc read).
- Highest quality on complex/poly material with least engineering, but (a) ~30-50 ms latency
  busts the 10 ms guitar budget, (b) not currently linked (build + GPL/commercial review),
  (c) overkill for a -1 oct integer shift. Good FALLBACK for the POLY voice only, not the
  mono octave-down.

## Comparative analysis

| Dimension | Current granular (2048 grain) | PhaseVocoder reuse (stretch+resample) | PSOLA octave-down (mono, 2:1) | Hybrid (PSOLA down + small-FFT PV poly/up) | Rubber Band R3 |
|---|---|---|---|---|---|
| Octave-DOWN single-note fidelity (PRIORITY) | Poor -- warble + smear | Fair -- phasy, smeared transients | Excellent -- crisp, tight | Excellent (uses PSOLA for down) | Very good but latent |
| Polyphonic chord tracking | Fair (warble) | Good | N/A (mono only) | Good (PV/granular handles poly) | Excellent |
| Added latency | ~23 ms (half grain) | ~35 ms x2 path | ~3-9 ms | down ~6-9 ms, poly/up ~12-23 ms | ~30-50 ms |
| Audio-thread CPU | Low | High (2 FFT/frame, x2 for shift) | Low-moderate | Moderate (down cheap, poly is the FFT cost) | High |
| Code reuse | n/a | PhaseVocoder + needs resampler | PitchTrackerYIN + new PSOLA core | YIN + PolyPitchTracker + PhaseVocoder/granular + SynthStyle wiring | Vendored but unlinked |
| JUCE / license fit | Native | Native | Native | Native | GPL-or-commercial; build change |

Cross-cutting facts that drive the call:

1. **The pitch trackers are 50-93 ms latent and on background threads.** They CANNOT be in
   the sample-accurate shift loop. They are a CONTROL signal -- read once per block like
   the SY-style synth does. The octave-down does NOT need a sample-accurate tracker because
   a 2:1 down-shift is integer: emit each detected period twice. The tracker just stabilizes
   the period estimate and rejects octave errors.
2. **Octave-DOWN is the easy pitch-shift case** (exactly 2:1). A pitch-synchronous (or even
   zero-crossing-synchronous, as Vintage already does) overlap-add that repeats each period
   once is tight and low-latency, no FFT.
3. **Phase-vocoder transient smearing is solved in production by dry-blend + attack/LP, not
   by a better transform.** We already sum the dry signal (`OctaveStyleDSP.cpp:279`), so the
   transient is present at full bandwidth; the fix is to make the shifted voice fade IN
   behind it and roll off its highs.

## Recommendation

**Adopt the hybrid: make the octave-DOWN a PSOLA-style period-doubling TIME-DOMAIN shifter
(NOT a phase vocoder), and keep a SMALL-FFT path (reuse the existing granular, shrunk, or a
thin PhaseVocoder pitch-shift wrapper) for the octave-UP and the polyphonic mode. Do NOT
adopt Rubber Band for v1, and do NOT route the mono octave-down through the existing
PhaseVocoder.**

- **Octave-DOWN priority -> PSOLA period-doubling.** Top priority (tight single-note -1 oct)
  is exactly where time-domain wins and phase vocoder loses. 2:1 is integer, so we need a
  period-synchronous OLA that re-emits each pitch period twice with a short crossfade. Period
  source two ways: the existing Vintage Schmitt zero-crossing detector
  (`OctaveStyleDSP.cpp:219-229`) for a coarse mark, refined/validated by `PitchTrackerYIN`'s
  published Hz to reject octave-error glitches. Latency ~one period + crossfade (~6-9 ms),
  inside budget.
- **Octave-UP and POLY -> small-FFT spectral, behind a dry-blend.** Reuse, don't rebuild:
  either shrink the existing `GranularShifter` grain 2048 -> ~512-768, or add a thin
  pitch-SHIFT wrapper around `PhaseVocoder` (FFT order 10 / hop 256, latency ~768 ~17 ms).
  Hide residual smear the POG way: attack-fade + LP on the shifted voices.
- **Reuse maximized.** PitchTrackerYIN, PolyPitchTracker, the SynthStyleDSP background-tracker
  wiring pattern, and the granular/PhaseVocoder machinery -- all already present. Net-new is
  the PSOLA period-doubler core (~150-250 lines) + the attack/LP voicing. No new threads, no
  build deps.
- **Why not Rubber Band:** ~30-50 ms latency fails the guitar budget, unlinked (build + GPL
  review), overkill for 2:1. Park as a Future State offline/poly upgrade.
- **Why not PhaseVocoder for the mono down:** its 1536-sample (~35 ms) floor and transient
  smearing are the two things the octave-down must avoid.

## Implementation sketch

Files touched (all under `Source/DSP/`, no CMake/threading changes):

1. **`OctaveStyleDSP.h/.cpp` -- replace the granular octave-DOWN path.**
   - Add `PitchTrackerYIN mYin;`; `mYin.prepare(sampleRate)` in `prepare()`, `mYin.reset()`
     in `reset()`, `mYin.pushAudio(mono, n)` at the top of `process()` -- copy the SY-style
     pattern verbatim.
   - New `struct PeriodDoublerDown` (the -1 oct PSOLA core): a mono ring + current period
     estimate. Per block, read `mYin` Hz/confidence -> period in samples; gate with the
     existing Schmitt zero-crossing to phase-align grain boundaries. Emit each input period
     TWICE with a short raised-cosine crossfade (one-period delay -> latency ~one period +
     crossfade). For -2 oct, emit 4x (or chain two stages). On low confidence / unpitched,
     fall back to the current granular or mute the down voice (avoid glitch).
2. **Octave-UP + Polyphonic chord mode -> small-FFT spectral path.**
   - Lowest-risk: shrink `GranularShifter::kGrainSize` 2048 -> 768 and `kBufferSize`
     accordingly; cuts latency/smear materially for +1/+2 and dense chords.
   - Higher quality (optional follow-up): `PitchShiftPV` = `PhaseVocoder` (order 10 / hop
     256) + linear-interp resampler.
   - Feed `PolyPitchTracker` range hints (guitar vs bass), mirroring the SY-style setup.
3. **POG-style transient/voicing polish on ALL shifted voices (cheap, high payoff).**
   - Per-voice attack-fade: on input transient (envelope-derivative spike), briefly duck the
     shifted voice so the DRY transient leads, then fade the shifted voice in over ~10-30 ms.
   - Per-voice one-pole LP (~3-5 kHz) on the shifted streams to mask residual smear.
   - Keep the existing `Range` LP, DC blockers, and dry-sum mix structure unchanged.

Latency budget (44.1 kHz): octave-down PSOLA ~6-9 ms; octave-up/poly small-grain ~9-17 ms;
dry path 0. Signature voice stays around the ~10 ms threshold.

Chunk-C sub-sequencing: (1) PSOLA period-doubler for -1/-2 oct + YIN wiring (priority);
(2) shrink granular / add small-FFT poly path + PolyPitchTracker hints; (3) attack-fade + LP
voicing polish + `/test-signal OctaveStyleDSP` validation.

## Open questions
- Schmitt zero-crossing robustness as a grain-boundary mark on a full-range guitar signal vs
  after the `Range` LP -- may need the mark detector fed from a lower-cutoff LP so the
  fundamental dominates. Validate with `/test-signal`.
- -2 oct: two cascaded 2:1 doublers vs one 4:1 quadrupler (cascade simpler, +1 period latency).
- Rubber Band as an offline/poly-only Future State upgrade (needs the license review the CMake
  comment defers).

## Confidence + caveats
- Recommendation confidence HIGH: rests on directly-read codebase facts (PhaseVocoder is a
  stretch not a shifter with a 35 ms floor; trackers are 50-93 ms background-thread control
  signals; SynthStyleDSP already proves the wiring; octave-down is integer 2:1), which
  converge with the TSM literature and the OC-5/POG production split.
- The per-mode latency split and the POG internal FFT details are MEDIUM-confidence (reviews +
  DSP-forum reverse-engineering; vendor algorithms are proprietary; some primary PDFs 403'd).
- Files read + cited: `OctaveStyleDSP.{h,cpp}`, `PhaseVocoder.{h,cpp}`, `PitchTrackerYIN.{h,cpp}`,
  `PolyPitchTracker.{h,cpp}`, `SynthStyleDSP.{h,cpp}`, `EffectRack.{h,cpp}`, `CMakeLists.txt`.
