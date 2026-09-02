# DSP Portability Matrix - BaySickDAW to KBS Plugins

Compiled 2026-09-01.  Covers every DSP module in the tree: 15 rack effects,
18 pedals, the 12-file vendored KBS Core EQ library, 21 instrument-engine
classes, 6 shared voice primitives, and 25 analysis / utility / infrastructure
modules.  **97 modules total.**

Three questions per module: what it does, whether it is portable to KBS
Plugins *as it stands today*, and what licence governs it.

---

## 0.  How to read the Portable column

| Verdict | Meaning |
|---|---|
| **CLEAN** | JUCE + our own code.  Copy the files, they compile. |
| **CLEAN+** | Portable, but drags a named sibling module along.  Named in the row. |
| **STUB** | Portable after swapping app services (file resolver, sample library, missing-file report, or the shared MP3-capable audio loader) for plain equivalents.  Mechanical, hours not days. |
| **REWORK** | Real app coupling to unpick - host back-pointers, the routing graph, the APVTS sweep contract. |
| **BLOCKED-GPL** | Carries GPL third-party code.  Cannot ship in a closed-source plugin without a commercial licence for that dependency. |
| **BLOCKED-LGPL** | Carries LGPL third-party code under a static link.  Shippable only with the relink obligation met, or by switching to a DLL. |

Every module we wrote is **our copyright**.  GPLv3 is the licence chosen for
*distributing BaySickDAW*; it does not bind Jeff's own reuse of his own code.
Where a row says CLEAN, there is **no remaining licence question at all** -
JUCE's free tier covers closed-source commercial use (see 1c).

---

## 1.  The legal position, in three parts

**1a.  BaySickDAW ships under GPLv3.**  `LICENSE` at the repo root is the
full GNU GPL v3 text.  That is a deliberate, valid choice for this product and
nothing here is illegal today.

**1b.  KBS Plugins is the opposite model** - commercial, closed-source, sold
under `Legal/EULA.md`, with `Manuals/licences.html` currently listing MIT
dependencies only.  So the real question is never "is this legal in the DAW",
it is "can this cross into a closed commercial binary".

**1c.  That splits three ways.**

1. **Our own code - yours to move.**  You hold the copyright on every line in
   `Source/`.  No third-party notice appears anywhere in `Source/` (grep for
   "copyright" across every `.cpp` and `.h` returns nothing, and there are no
   SPDX headers).  Algorithm attributions in comments point at published
   academic work - Schroeder 1962, the RBJ EQ Cookbook, Dattorro nested
   allpass, de Cheveigne/Kawahara YIN 2002, Laroche-Dolson phase vocoder,
   Lebart dereverberation, BS.1770 - which are techniques, not copyrightable
   expression.  We wrote the implementations.
2. **Third-party GPL - cannot cross.**  Exactly one library, Rubber Band, and
   it touches exactly **two source files**.
3. **Permissive third party - crosses with attribution.**  sfizz BSD-2, NAM
   Core MIT, WORLD modified BSD, both Signalsmith libraries MIT,
   concurrentqueue BSD-2-or-Boost, WebView2 MS-BSD-3, fontaudio MIT.

**JUCE is not a blocker.**  `juce/LICENSE.md` says the JUCE 8 modules are
dual-licensed AGPLv3 or commercial, but the commercial side has a **free tier**
for revenue under roughly $20k/year, and that free tier permits **closed-source
commercial distribution**.  Jeff confirmed this directly (2026-09-01); KBS is
pre-revenue, so it is covered.  JUCE therefore imposes no obligation on
anything in this matrix - not on the DAW, not on KBS, not on any CLEAN row.
Revisit only if KBS revenue approaches the threshold.

One consequence worth recording: because JUCE is *not* forcing copyleft here,
the DAW's GPLv3 is driven by **Rubber Band alone** (plus the ASIO SDK's GPLv3
branch, if that is the branch being relied on).  Drop Rubber Band and the
copyleft pressure on the codebase goes to zero.

---

## 2.  Dependency ledger

Every vendored library, the licence read out of its own bundled text (not
inferred from the project name), how it is linked, and what it costs you.

| Library | Version | Licence (read from file) | Linkage | Crosses to KBS? |
|---|---|---|---|---|
| **JUCE** | 8.0.12 | AGPLv3 **or commercial, whose free tier covers closed-source commercial use under ~$20k/yr revenue** (`juce/LICENSE.md`) | Static, everywhere | **Yes - no cost, no obligation at current revenue** |
| **Rubber Band** | 4.0.0 | **GPL-2.0-or-later** (`libs/rubberband/COPYING`; `meson.build:6`) | Static, `CMakeLists.txt:383` | **NO** without a Breakfast Quay commercial licence |
| **libmp3lame** | 3.100 | **LGPL v2** - the *Library* GPL, not 2.1 (`libs/lame/COPYING`) | **Static**, `CMakeLists.txt:420` | Only with the relink obligation met, or as a DLL |
| **ASIO SDK** | 2025 Steinberg | Proprietary Steinberg **or** GPLv3 | Headers only, `JUCE_ASIO=1` | Irrelevant - a VST3 never opens a device.  Do not carry it |
| **sfizz** | 1.2.3 | **BSD 2-Clause** | Static | Yes, reproduce the notice (sfizz + its 17 bundled sub-deps) |
| **NeuralAmpModelerCore** | 0.4.0 | **MIT** (c) 2023-2025 Steven Atkinson | Static | Yes, reproduce the notice |
| - Eigen (bundled in NAM) | 3.4.90 dev snapshot | **MPL 2.0** | Header-only | Yes.  The one LGPL-derived file was relicensed MPL2 by its author and is not reachable from `Eigen/Dense` |
| - nlohmann/json (bundled) | single-header | MIT | Header-only | Yes |
| **WORLD** | untagged | **Modified BSD 3-clause** | Static | Yes, notice + no-endorsement clause |
| **Signalsmith Stretch** | untagged | **MIT** | Header-only | Yes |
| **Signalsmith Linear** | untagged | **MIT** | Header-only | Yes |
| **concurrentqueue** | moodycamel | **BSD-2 *or* Boost 1.0**, your election | Header-only | Yes.  Elect Boost and attribution drops to zero |
| **fontaudio** | 1.0.0 | MIT module + **OFL 1.1 font** + **CC BY 4.0 SVGs** | JUCE module, TTF embedded | Yes, but the OFL and CC BY texts are **missing from the repo** |
| **WebView2** | SDK for Edge 140 | MS BSD-3-style | Loader DLL redistributed | Yes, ship `NOTICE.txt` |

### 2a.  The complete third-party include surface

Verified by grepping `#include` directives, not name mentions - a bare-name
grep returns ~50 false positives for sfizz alone (comments and `.sfz` path
handling).  **This is the whole list.  Nine files.**

| Library | Files that include it |
|---|---|
| **rubberband** | `Source/DSP/LibraryPitchShifters.cpp:39`, `Source/DSP/PitchCorrectorDSP.cpp:7` |
| **sfizz** | `BaySickGuitarsProcessor.cpp:6`, `BaySickBassesProcessor.cpp:6`, `BaySickRustyDrumsProcessor.cpp:7` |
| **NAM** | `BaySickNAMIRProcessor.cpp:12-13`, `Source/DSP/NAMPedalStyleDSP.cpp:7-8` |
| **world** | `Source/DSP/LibraryPitchShifters.cpp:355-358` |
| **signalsmith** | `Source/DSP/LibraryPitchShifters.cpp:235` |
| **lame** | `Source/DSP/Mp3Writer.cpp:6`, `Source/MpglibAudioFormat.h:5` |

### 2b.  Why the surgery is cheap

Every third-party dependency is behind a `BAYSICK_HAS_*` compile flag applied
**only to the standalone target** (`CMakeLists.txt:726-769`).  The legacy VST3
target at `CMakeLists.txt:463` already builds the same `${VIBESYNTH_DSP_SOURCES}`
against `${VIBESYNTH_LINK_LIBS}`, which is **three JUCE modules and nothing
else** (`CMakeLists.txt:235-241`).

That is a standing, working proof that the clean DSP set compiles with JUCE
alone.  You are not guessing at portability - the build already does it.

---

## 3.  Matrix A - Rack effects (15)

All derive from `DSPBase` (`prepare` / `process(AudioBuffer)` / `reset`, plus
state XML, host BPM/transport, GR metering, latency reporting, sidechain, and
an embedded `EffectVisualFeed`).  All `.cpp` files include `SafeXml.h` for the
XXE-guarded state restore - a 1-file swap on port.

| # | Module | What it does | Portable | Licence |
|---|---|---|---|---|
| 1 | **CompressorDSP** | Stereo compressor.  Soft/vintage knee gain computer, peak or RMS detection, stereo link, lookahead, parallel mix, auto-makeup, sidechain HPF.  4 types: Modern, FET (1176-style nonlinear envelope, GR saturation, all-buttons rising ratio), Opto (LA-2A program-dependent multi-stage release), CS (sustain macro driving threshold+makeup together, post tilt EQ).  8 knee types incl. TCR variants. | **CLEAN** | Ours + JUCE |
| 2 | **LimiterDSP** | Look-ahead peak limiter.  0-10 ms delay, 4x oversampled true-peak detect, stereo-linked envelope, 2-stage auto-release, linear/exp release curve, RMS sustain hold, tanh soft-sat, hard ceiling.  Modes Limiter / Maximizer (loudness target, true-peak auto-ceiling, LUFS meter).  8 characters: Clean, Smooth, Tight, Punch, Glue, Loud, Warm, Instant. | **CLEAN+** (LufsMeterDSP, TruePeakMeter) | Ours + JUCE |
| 3 | **GateDSP** | Vocal-chain noise gate.  Stereo-linked peak detect, 3 dB Schmitt hysteresis, hold stage, smoothed gain.  Threshold / range / attack / hold / release. | **CLEAN** | Ours + JUCE |
| 4 | **DeEsserDSP** | Stereo de-esser on the Waves Sibilance surface.  Sidechain HPF 4-12 kHz, saturating overshoot-to-reduction curve floored at Range.  Mode is a **continuous 0-100 blend** from Wide (full-band duck) to Split (4 kHz LR crossover).  Engines TimeDomain (zero latency) / Spectral (per-bin STFT).  M/S targeting, lookahead, Mix, and a Listen that solos the *removed* component. | **CLEAN+** (SibilanceSpectralProcessor) | Ours + JUCE |
| 5 | **TransientShaperDSP** | Dual-envelope transient designer.  Fast peak vs slow RMS, quadratic attack/sustain gain, LR4 crossover with gain applied to the high band, oversampled tanh drive 2x-16x at constant latency.  Sharp/Medium/Soft shapes per stage. | **CLEAN** | Ours + JUCE |
| 6 | **OverdriveDSP** | Pre-LPF, `x/(1+abs(drive*x+bias))` soft clip at 2-16x, post-LPF, attenuate-only post gain, hard clip, DC block, blend or parallel add.  Types Rack / Pedal (80 Hz clean-sub split, 500 Hz pre-clip notch, dual cascaded tanh, tone + level). | **CLEAN** | Ours + JUCE |
| 7 | **SaturationDSP** | Multi-engine saturator.  Sensitivity, tone-pre shelf, 350 Hz bass split inside the oversampled region, even-harmonic tanh plus three odd/even shapers per band, optional transformer, bass-relief blend, DC block, tone-post, wet/dry, auto-gain.  Types Tube / Console (Clean/Dirty + Color) / Tape (folded-in legacy TapeDSP: asymmetric sigmoid, hysteresis, wow/flutter/hiss, 10 cassette IRs).  Harmonics KeepLow/Normal/KeepHigh, vocal body, OS 2-16x. | **STUB** - loads cassette IRs from `Resources/Tape` via the shared loader (soft LAME edge; swap for a plain WAV reader and it vanishes) | Ours + JUCE |
| 8 | **ChorusDSP** | 3 LFOs, per-LFO rate and waveform (Sine/Triangle/Multi/Organic), global depth, stereo phase spread, LR4 band split so only the chosen band is chorused.  3 or 6 voices (second read offset by pi).  Wet-only for sends. | **CLEAN** | Ours + JUCE |
| 9 | **FlangerDSP** | Sine-to-triangle morphable LFO, base delay + sweep depth, positive/negative feedback (inverted comb), feedback damp LPF, invert-feedback and invert-wet, cross-channel wet mix, stereo LFO offset, 8-division BPM sync. | **CLEAN** | Ours + JUCE |
| 10 | **PhaserDSP** | All-pass phaser, runtime stage count 1-24 (24-stage buffer always allocated so changes never click), log-scaled per-sample LFO-to-frequency map, feedback with explicit invert, cross-channel feedback, min/max depth, stereo offset.  Waves Sine/Triangle/Saw/S&H, Slow/Fast range, 8-division sync. | **CLEAN** | Ours + JUCE |
| 11 | **DelayDSP** | Keep-pitch exponential slew on time change, lo-fi bit+rate crush applied on the delay-line *read* so every echo crushes, feedback chain of 4 allpass diffusers into an SVF into distortion (Limit or Saturation), LFO on time and/or cutoff, bipolar output tone, output limiter.  Models Stereo/Mono/PingPong/Off (Off = filter+distortion unit, no echoes).  Types Echo / VocalDoubler (H910-style detuned taps).  Sidechain ducking, tempo sync, slapback preset. | **CLEAN** | Ours + JUCE |
| 12 | **ReverbDSP** | 8-line FDN.  Prime delay lengths scaled by room size, normalised Hadamard H8 matrix, per-line RT60 gain, per-line HF damp and bass shelf in the feedback path, 4-stage Schroeder pre-diffusion, 500 ms pre-delay (BPM-snappable), input HP/LP, M/S width, per-line tail modulation, Freeze, 24 early-reflection taps, HF decay ratio, wet tilt.  Stereo/Mid/Side.  5 algorithms: Plate, Hall, Chamber, Room, VocalBooth.  Sidechain ducking. | **CLEAN** | Ours + JUCE |
| 13 | **DeReverbDSP** | Late-reverb suppressor.  Lebart-family estimator - per bin, reverb magnitude modeled as a delayed copy of the signal's own recent magnitude decaying at the user T60, subtracted by a floored Wiener gain.  STFT 2048/512 Hann, identity OLA, gains from channel-average magnitude so the image cannot twist. | **CLEAN** | Ours + JUCE |
| 14 | **SibilanceSpectralProcessor** | The STFT engine behind DeEsser's Spectral mode.  Per-bin real gain (phase untouched), mask = event gate times per-bin selectivity against each bin's own slow floor EMA, blended Wide-to-Split, per-bin attack/release, 3-bin frequency smoothing.  HQ 2048/512 and LL 1024/256 both preallocated; runtime switch is allocation-free. | **CLEAN** - needs not even `DSPBase`.  **The most portable module in the tree.** | Ours + JUCE |
| 15 | **StripEq** | The strip wrapper around one `kbs::ParametricEq` (24 bands with per-band domain routing), replacing the old mid+side `EQ8MsDSP` pair.  Adds audio-thread band sync that forwards only real changes, an A/B spare bank, an `isIdentity()` short-circuit that deliberately does not skip latency-bearing setups, an offline scan tap, and pre/post/sidechain spectrum feeds. | **REWORK** - its threading contract *is* the app's: the APVTS-cache sweep, the project-load shield, the caller pushing A/B swaps back to APVTS.  Portable only together with all of `Kbs/`. | Ours + JUCE |

---

## 4.  Matrix B - Pedal DSP (18)

All derive from `DSPBase`.  The pedalboard itself is a fixed 8-slot rack with
position locks: slot 0 is the Tuner (locked front), slots 1-6 free, slot 7 is
an EQ (locked back, one of the three EQ variants below).

| # | Module | What it does | Portable | Licence |
|---|---|---|---|---|
| 16 | **AcousticPreampStyleDSP** | Piezo/UST to mic'd-acoustic preamp.  **Adaptive** resonance: a base correction IR plus block-rate dynamics analysis (fast peak env vs slow level avg) driving a piezo de-quack cut near 2 kHz and a 3-resonator body bank with per-body shelf/peak.  Then Schroeder ambience (4 combs into 2 allpass), a defeatable feedback notch (50 Hz-1 kHz, Q~10), and level.  Bodies Dreadnought/Parlor/Jumbo/User. | **STUB** - bundled IR + user-IR path through SampleLibrary, ProjectFileResolver, MissingFileReport | Ours + JUCE |
| 17 | **AcousticSimulatorStyleDSP** | Electric-pickup acoustic simulator, **no IR in the named modes**.  Pickup de-emphasis (2.5 kHz scoop), envelope-driven transient shaper acting on >4 kHz attack content, parallel IIR body bank per mode, Schroeder reverb, level.  Modes Standard/Jumbo/Enhanced/Piezo/User. | **STUB** - same three app services as #16 | Ours + JUCE |
| 18 | **BassCompressorStyleDSP** | Multi-band bass compressor.  LR4 crossovers at 200 Hz and 2 kHz, squared-domain envelope per band, fixed 10 ms attack, shared ratio/threshold/release, GR reported as worst band. | **CLEAN** | Ours + JUCE |
| 19 | **BassDriverStyleDSP** | Dynamics-adaptive multi-band bass drive.  LR4 split at 500 Hz / 2 kHz, low band stays clean, an envelope on Mid+High produces a **per-sample adaptive drive**, Mid+High 4x oversampled and asymmetrically clipped, parallel clean blend, DC block. | **CLEAN+** (PolyphaseOversampler4x) | Ours + JUCE |
| 20 | **BassGraphicEQStyleDSP** | 7-band bass graphic EQ.  Fixed peaks at 50/120/400/500/800/4.5k/10k, Q 1.4, +/-15 dB, plus a +/-15 dB master fader as the 8th slider. | **CLEAN** | Ours + JUCE |
| 21 | **BassOverdriveStyleDSP** | 4x oversample into a hard clip at the gain-boosted level, downsample, Balance against the unclipped input, then a **post-blend** active Baxandall shelf pair (100 Hz / 3 kHz), DC block, level. | **CLEAN+** (PolyphaseOversampler4x) | Ours + JUCE |
| 22 | **BluesDriveStyleDSP** | Asymmetric tube-ish overdrive.  4x oversample into an envelope-driven dual-stage shaper - asymmetric tanh crossfading into a cubic as level rises, so the dominant harmonic shifts 2nd to 3rd as the player digs in.  1st-order LPF tone sweeping 500 Hz-5 kHz, fixed 100 Hz +3.5 dB body peak, DC block (the shaper leaves ~0.0074 DC at silence). | **CLEAN+** (PolyphaseOversampler4x) | Ours + JUCE |
| 23 | **DistortionStyleDSP** | Pre-emphasis shelf near 1 kHz (+12 dB at max drive), 4x oversample, symmetric hard clip, downsample, Big-Muff-style tilt tone (parallel 400 Hz LPF and 2 kHz HPF crossfaded, fixed mid scoop at noon), DC block, level. | **CLEAN+** (PolyphaseOversampler4x) | Ours + JUCE |
| 24 | **FuzzStyleDSP** | Three-character fuzz, all 4x oversampled, input boost 0..+20 dB.  Mode M = bias-starved (positive half clamps hard at +0.5, negative passes soft - gated and squelchy).  Mode F = germanium symmetric soft tanh.  Mode O = full-wave rectifier then hard clip for the upper-octave artefact.  DC blocker per channel (Mode O leaves a -0.5 pedestal). | **CLEAN+** (PolyphaseOversampler4x) | Ours + JUCE |
| 25 | **GraphicEQStyleDSP** | 7-band guitar graphic EQ at 100/200/400/800/1.6k/3.2k/6.4k.  Six peaks at Q 1.4 plus a **high shelf** on the top band, +/-15 dB each, +/-15 dB master. | **CLEAN** | Ours + JUCE |
| 26 | **HighGainStyleDSP** | Metal stack.  Fixed pre-clip mid boost near 1 kHz locks the honk, 4x oversample into cascading hard clip (two clamp stages with a smoothing curve between, for the stacked-transistor sound), then two fixed post-clip gyrator **boosts** at 100 Hz and 5 kHz - the V-scoop is the valley between them, not a cut - then a user 3-band EQ with sweepable mid, DC block, level. | **CLEAN+** (PolyphaseOversampler4x) | Ours + JUCE |
| 27 | **NAMPedalStyleDSP** | User NAM pedal.  Loads a `.nam` capture, mono-sums into it, runs it per block between a pre-model input/drive (-24..+24 dB) and a post-model 3-band Baxandall, blend, output trim, then dual-mono spread.  Model swap publishes through an active/pending pair; the audio thread try-locks only when a swap is actually pending. | **STUB** - **direct `<NAM/dsp.h>` include** (MIT, fine) plus SampleLibrary / ProjectFileResolver / MissingFileReport for the capture path | Ours + JUCE + **NAM Core (MIT)** |
| 28 | **NoiseGateStyleDSP** | Pedal gate / downward expander.  RMS follower over a 5 ms window, Schmitt latch (open at threshold, hold until 3 dB below) with a ~1 ms open coefficient so attacks punch through, decay-rate close.  Mode Reduction (-20 dB) / Mute (-60 dB).  Source DI (the strip's clean pre-distortion sidechain tap, the default) / Self. | **CLEAN** in code; degrades to Self without the host's sidechain routing | Ours + JUCE |
| 29 | **OctaveStyleDSP** | Parallel +1 / -1 / -2 octave voices over dry.  **Polyphonic** mode is a hybrid: -1/-2 use a pitch-synchronous PSOLA period doubler anchored to Schmitt pitch marks validated by YIN, hysteresis-switched (short fade, never a standing blend) to a granular fallback on chords and unvoiced input; +1 and the fallback use two crossfaded Hann-windowed read heads on an 8192-sample ring with runtime period-synced grain size.  **Vintage** mode is low-CPU mono: +1 = 4x oversampled rectifier, -1 = Schmitt divide-by-2, -2 = divide-by-4.  Range = input LP 300 Hz-3 kHz into the shift paths. | **CLEAN+** (PitchTrackerYIN, PolyPitchTracker) | Ours + JUCE |
| 30 | **SynthStyleDSP** | Pitch-tracked guitar synth.  Mono-sum into YIN (background thread, atomic Hz + confidence), per-block envelope follower, phase-accumulator oscillators at the tracked pitch (sine/saw/square/triangle), SVF lowpass driven by Tone plus LFO, direct/effect mix, DC block.  11 types (Lead1, Pad, Bass, Seq1, Lead2, Str, Organ, Bell, Sfx1, Sfx2, Seq2) each with an 11-way Variation.  Guitar/Bass tracker range, mono YIN or poly FFT voices. | **CLEAN+** (PitchTrackerYIN, PolyPitchTracker).  Note: carries a **non-virtual** `releaseResources()` that tears down both tracker threads - a host must call it explicitly. | Ours + JUCE |
| 31 | **TunerStyleDSP** | Tuner over `PitchTrackerYIN`.  Published Hz to note + cents through wait-free atomics for the strobe.  Chromatic / Guitar (snaps E2 A2 D3 G3 B3 E4) / Bass (E1 A1 D2 G2).  440-432 reference toggle with mode-dependent trim range, flat 0-6 semitone drop, mute that silences output while detection keeps running. | **CLEAN+** (PitchTrackerYIN) | Ours + JUCE |
| 32 | **WahStyleDSP** | Resonant TPT-SVF bandpass swept by pedal position, log-mapped 400 Hz heel to 2.2 kHz toe, Q ~4.5.  Vintage = that alone; Rich adds a parallel 200 Hz lowpass at fixed level so the fundamental survives on bass and dropped tunings.  Smallest pedal in the set. | **CLEAN** | Ours + JUCE |
| 33 | **FurmanEQStyleDSP** | Pro parametric EQ on the PQ-3 surface.  Master input preamp with tanh soft clip capped at +26 dB, Hi/Lo output gain-range switch, a published overload level for the panel LED, and 3 fully parametric peak bands (Low 25-500, Mid 150-2.5k, High 600-10k; -inf..+20 dB; Q 0.2-3.8).  A separate EQ Bypass kills the bands only - preamp saturation stays live. | **CLEAN** | Ours + JUCE |

---

## 5.  Matrix C - `Source/DSP/Kbs/`, the Core EQ library (12)

Deliberately **JUCE-free**, namespace `kbs`, so the test target builds it
standalone.  None derive from `DSPBase`; none touch APVTS, files, or app types.
This is a vendored take-back of BaySickDAW's own EQ8 with 26 recorded defects
fixed, plus two BaySickDAW-side extensions (per-domain linear phase, and the
four-slot per-band sidechain).

**Every row here is CLEAN and JUCE-free - the single most portable block in the
tree, and it came from KBS in the first place.**

| # | Module | What it does | Portable | Licence |
|---|---|---|---|---|
| 34 | **ParametricEq.h** | **The flagship engine** (~102 KB, header-only).  Up to 24 active bands, each with type, freq, gain, Q, **continuous** slope 1-96 dB/oct plus Brickwall (fractional slopes realized as an analytic FIR in linear modes and a staggered pole/zero ladder in IIR), channel routing, stereo placement, mute/isolate, and a full dynamics section (threshold, ratio, attack, release, auto-release, range, a second band-B threshold/ratio/range, onset mix, per-band saturation, LFO targeting freq/gain/Q, envelope depth, spectral density, 4-slot per-band sidechain).  9 band types.  Modes: minimum-phase, natural (decramped bells), 5 linear-phase precisions.  Globals: 2x oversampling on the IIR path, proportional Q, measured auto-gain, output trim, polarity, character stage, curve transform, delta listen, audition domain, listen band.  Governing rule: the UI's magnitude/phase/GR queries are answered by *the same arithmetic that filters the audio*. | **CLEAN**, no JUCE | Ours |
| 35 | **Devices.h** | The Core device set in natural units - `Biquad` (direct-form I plus the RBJ cookbook designs), shared detector constants, `BandType`, and macro-addressable `EqBand` / `Compressor` / `Limiter`, each recomputing coefficients only on real change. | **CLEAN**, no JUCE | Ours |
| 36 | **SVF.h** | TPT state-variable filter returning `{lp, bp, hp, notch, peak}` from one integrator pair, so morphing between responses costs only a crossfade and the filter stays stable while the cutoff sweeps.  NaN-guards its state.  Plus 5 formant vowel sets that interpolate formant *positions* rather than crossfading outputs, and `StepRandom`, a deterministic seeded S&H for reproducible renders. | **CLEAN**, no JUCE | Ours |
| 37 | **EqLinearPhase.h** | Overlap-save linear-phase FIR, written to replace an engine with three structural faults (Hann-squared at 50% not summing flat, so a 6 dB frame-rate tremolo; circular convolution smearing narrow curves; a reported delay half the imposed one).  Samples the magnitude on the FFT grid, inverse-transforms to the zero-phase impulse, rotates and Kaiser-windows, convolves zero-padded frames, keeps the wrap-free tail.  Latency pinned by an impulse test. | **CLEAN**, no JUCE | Ours |
| 38 | **EqMatch.h** | EQ Match arithmetic.  Greedy fit - smooth the difference, place a bell on the largest remaining error at the width the error actually has, subtract that bell's *true* response, repeat; stopping early always leaves the best partial fit.  240-point log grid 20 Hz-20 kHz.  Optional per-point spread so a constant problem is told apart from an occasional one. | **CLEAN**, no JUCE | Ours |
| 39 | **EqCharacter.h** | The character/saturation stage.  `shapeA` is a pure odd-order cubic softening, level-true for small signals; `shapeB` is tanh with a 0.06 x-squared asymmetry for even harmonics; both normalised so small-signal gain stays unity.  Curves kept low-order on purpose so aliasing stays sub-audible without a dedicated oversampler. | **CLEAN**, no JUCE | Ours |
| 40 | **Oversampler.h** | Polyphase oversampling for nonlinear stages, 2/4/8x.  16 taps per phase regardless of factor, windowed-sinc prototype at base Nyquist with Kaiser beta 9 (~-90 dB stopband), normalised to unity DC so oversampling never changes level.  Plus `OversampledShaper`, the per-sample form usable inside a feedback loop where the block form cannot be. | **CLEAN**, no JUCE | Ours |
| 41 | **FFT.h** | Small radix-2 DIT FFT with precomputed twiddles and a bit-reversal table, in place, inverse conjugates and scales so forward-then-inverse is the identity.  Exists rather than using JUCE's faster one so the rest of Core builds and unit-tests without JUCE. | **CLEAN**, no JUCE | Ours |
| 42 | **Feeds.h** | Two lock-free audio-to-UI structures, both allowed to fail rather than block.  `SpectrumFeed` is a 16384-sample seqlock ring storing **mid and side** in parallel (side rides along because EQ Match needs to know how the domains differ).  `HistoryRing` is fixed-rate scrolling history with no sequence counter at all, since a torn read costs one invisible point. | **CLEAN**, no JUCE | Ours |
| 43 | **SpectrumScan.h** | Offline spectrum accumulator for the Match panel's track scan.  4096-point Hann at 50% hop, accumulating mid and side dB sums plus mid squared-sums at render speed with no feed ring and no dropped frames.  Produces the match grids, a per-point standard deviation (the static-vs-dynamic split statistic), and the 6 dB dynamic-range rule that rejects a mono source's near-silent side curve. | **CLEAN**, no JUCE | Ours |
| 44 | **MacroParameter.h** | The macro layer - one user control drives several internal parameters, each across its own range and curve, with Advanced mode exposing the internals clamped to exactly the span the macro would sweep so the two views cannot disagree about reachable states.  `Param`, `ParamRegistry`, `applySkew`, `MacroLink`, `SpanEdit`. | **CLEAN**, no JUCE.  Currently pulled into the DAW only transitively - no `kbs::MacroParameter` reference exists outside `Kbs/`. | Ours |
| 45 | **TargetNames.h** | The macro-target id strings. | **CLEAN**, no JUCE | Ours |

---

## 6.  Matrix D - Instrument engines (21)

The engine *processors* are `juce::AudioProcessor` shells with their own
track-scoped APVTS, `ISidechainEngine`, and an `ApvtsDirtyTracker` wired to
`ProjectManager::markDirty`.  In a plugin the shell is the part you rewrite -
the DSP core underneath is the part you keep.  That split is called out per row.

| # | Module | What it does | Portable | Licence |
|---|---|---|---|---|
| 46 | **BaySickSynthProcessor** | AudioProcessor shell for the subtractive synth.  Own APVTS prefixed `tk_{id}_bss_`; the per-block sweep compares each cached last value before calling a DSP setter. | **REWORK** (shell only) - the DSP core below is CLEAN | Ours + JUCE |
| 47 | **BaySickSynthDSP** | Voice allocation: 16 voices in a `BroadcastSynthesiser`, plus per-mode MIDI pre-processing (Mono note-off injection, Legato retarget dispatch) before `renderNextBlock`.  Shared verbatim with BaySickBass. | **CLEAN** | Ours + JUCE |
| 48 | **BaySickSynthVoice** | One voice: two wavetable oscillators (main + dual), one SVF filter, three ADSRs (amp / filter / pitch), one LFO.  11 waveform modes, unison, osc sync, ring mod, drift, glide / legato, cut-self. | **CLEAN+** (WavetableOscillator, SynthFilter, AdsrEnvelope, LFO, PanLaw) | Ours + JUCE |
| 49 | **BaySickBassProcessor** | A second shell over the **same** `BaySickSynthDSP`, prefix `tk_{id}_bsb_`, tuned for bass.  There is no separate bass voice class. | **REWORK** (shell only) | Ours + JUCE |
| 50 | **BaySickPlayerProcessor** | Shell for the sample player, prefix `tk_{id}_bsp_`. | **REWORK** - holds a raw host back-pointer for the sample-load shield, and persists sample paths as SampleLibrary refs | Ours + JUCE |
| 51 | **BaySickPlayerDSP** | Five classes in one file: the zone map (SFZ keyswitch opcodes, round-robin, articulation groups, tune / volume offsets, pre-loaded shared buffers so voices outlive a reload), the sample manager (folder or SFZ load on the message thread, lock-free region lookup on audio), the sound, forward and reversed memory sources, the voice, and the synth. | **STUB** - MissingFileReport plus the shared MP3-capable loader (soft LAME edge) | Ours + JUCE |
| 52 | **HarmlessProcessor** | Shell for the additive synth, prefix `tk_{id}_harm_`; also owns `registerModTargets`, which *defines* the mod-matrix target ordering. | **REWORK** (shell only).  **Known perf debt:** the shape setter rebuilds a wavetable via FFT from the APVTS sweep, i.e. on the audio thread - already marked TODO in the header. | Ours + JUCE |
| 53 | **HarmlessSynth** | 16 additive voices sharing two harmonic engines (Part A / B).  Wavetables are read-only at audio time so sharing needs no lock; param changes broadcast and land via SmoothedValues. | **CLEAN** | Ours + JUCE |
| 54 | **AdditiveVoice** | One voice playing two harmonic-engine wavetables mixed, 1-9 voice unison with per-slot detune and stereo spread, per-voice stereo TPT filter, amp ADSR, tremolo, vibrato, a second filter, pitch offset.  Zero heap allocation on the audio thread - arrays pre-sized for max unison. | **CLEAN+** (HarmonicEngine, PanLaw, mod registry) | Ours + JUCE |
| 55 | **HarmonicEngine** | 516 harmonic partials (amplitude, phase, reserved per-partial detune) run through a 2048-point IFFT to a single-cycle wavetable.  Double-buffered with atomic publish so the audio thread always sees a consistent snapshot.  About 10-20 us per rebuild.  Brownian spectral rolloff and routing-matrix scale hooks. | **CLEAN** | Ours + JUCE |
| 56 | **SpectralModules** | Five spectral processors operating on *working copies* of the partial arrays before the IFFT, so hand-edited partials are never destroyed.  The Prism module is the odd one - it does not modify the arrays, it supplies a per-partial frequency shift for fractional-bin interpolation inside the wavetable build, giving piano-string inharmonicity at low amounts and bell / metallic spectra at high. | **CLEAN** | Ours + JUCE |
| 57 | **HarmlessModRegistry** | The mod matrix - targets by sources (Envelope, LFO, Velocity, Keyboard, ModX / Y / Z), each with per-tab curves, depth, length, tempo / global, and speed / tension / skew / pulse-width warp.  UI mutates targets then publishes a snapshot by bumping an atomic generation; voices cache it at note-on and rebuild target flags when it moves. | **CLEAN+**, with two caveats: a **DSP-to-UI header dependency** (it includes the mod editor header for the curve-point type), and no ABA-safe snapshot retirement - a v1 shortcut guarded only by a SpinLock | Ours + JUCE |
| 58 | **BaySickVocalProcessor** | The vocal channel strip.  Locked chain: input, pitch correction, gate, de-reverb, de-esser, compressor, saturation, limiter, output.  Owns an **embedded BaySickNAMIR**, an EffectRack for the chain stages, and the Align and Pitch offline engines. | **BLOCKED-GPL** and **REWORK**.  The most entangled engine in the tree: pulls rubberband, signalsmith, world, NAM and lame transitively, and reaches five distinct host-processor members plus EngineRig and the editor.  Do not port this - port its *stages* individually. | Mixed, includes **GPL** |
| 59 | **BaySickGuitarsProcessor** | One sfizz instance driven by piano-roll MIDI, single stereo out, per-instance APVTS with an output volume plus 128 CC params, up to 20 instances.  Slides route to the purpose-built SlideSampler instead of sfizz. | **STUB** - sfizz is BSD-2 so legally fine; needs SampleLibrary and the global UndoManager replaced | Ours + JUCE + **sfizz (BSD-2)** |
| 60 | **BaySickBassesProcessor** | Near-exact mirror of Guitars, prefix `bbb_`. | **STUB**, same as row 59 | Ours + JUCE + **sfizz (BSD-2)** |
| 61 | **BaySickRustyDrumsProcessor** | Drum engine.  One sfizz instance plus a kit loader that walks the kit's mapping masters to discover per-piece channels; each becomes a mixer strip, making this **the only multi-out instrument engine**.  Piano-roll keys are parsed from the kit's keymap defines at native kit MIDI, with no remap layer. | **REWORK** - the channel ceiling lives in `BaySickGraph.h:96`, so the *graph* caps it rather than the engine, and kit discovery is coupled to a specific on-disk layout | Ours + JUCE + **sfizz (BSD-2)** |
| 62 | **BaySickNAMIRProcessor** | Amp and cab chain: input gain, noise gate, oversampled NAM, low cut, high cut, fork to a convolution cab mixed against dry, master out, then mic simulation and mic placement - with a parallel Mic B path that **sums** rather than blends, because two real mics on one source add.  Two full A / B slots each own their own model, convolution and file paths, with a snapshot that saves and restores APVTS on flip.  Model swap is wait-free: the message thread parks a pending model, the audio thread swaps it in, deallocation happens later off-thread. | **STUB** - NAM is MIT; needs its APVTS shell and the app-root log path replaced | Ours + JUCE + **NAM Core (MIT)** |
| 63 | **BaySickPedalsProcessor** | The 8-slot pedalboard host with position locks (Tuner front, EQ back).  Single-slot mutations publish through a per-slot atomic so the audio thread stays wait-free; multi-slot moves take a brief spinlock the audio thread try-locks, worst case skipping one block. | **STUB** - depends on `EffectRack` for the type enum and factory, and on an app-root preset path | Ours + JUCE + NAM transitively |
| 64 | **SlideSampler** | The voiced blended multi-sample slide DSP.  As continuous pitch moves it picks the sample at-or-just-below and bends it **up at most one semitone**, crossfading at semitone boundaries.  Full voicing chain per voice: zone sample, Lagrange resample (keycenter delta, micro-bend, tune, unison detune, LFO pitch, pitch EG), gain (volume, velocity curve and tracking, AHDSR, LFO tremolo, crossfade sine), keytracked lowpass for bass and a 1-pole highpass at 250 Hz for unison layers, per-voice pan, stereo sum. | **CLEAN** - **no sfizz include**, a from-scratch reimplementation of the patch voicing.  CC access is an *injected* lock-free callback, so the class stays decoupled from APVTS. | Ours + JUCE |
| 65 | **SlideRegionMap** | SFZ extraction into the program the SlideSampler plays - per-zone gain staging, velocity curves, envelopes with CC modulations, LFO banks, the bass filter block, custom curve tables, group and choke policy, unison opcodes and bend ranges, across every articulation.  Modulation-family opcodes are carried **verbatim as strings** and parsed once at program set, so extraction stays exhaustive without freezing the runtime schema. | **CLEAN** | Ours + JUCE |
| 66 | **SlideSampleCache** | Shared decoded-sample store, one mono buffer per unique path held as a weak pointer so it frees when the last sampler drops it.  N tabs on the same patch cost one copy.  Decoding is synchronous inside the kit load, so there is no first-slide decode latency.  Buffers carry a 32-frame zero tail pad for interpolator overread. | **CLEAN** - but instantiated as a process-wide shared singleton | Ours + JUCE |

---

## 7.  Matrix E - Shared voice primitives (6)

All JUCE-only, zero app coupling.  These are the building blocks under
BaySickSynth, BaySickBass and Harmless.  **Every row is CLEAN.**

| # | Module | What it does | Portable | Licence |
|---|---|---|---|---|
| 67 | **AdsrEnvelope** | 5-stage (Idle / Attack / Decay / Sustain / Release) exponential-coefficient envelope, per-sample. | **CLEAN** | Ours + JUCE |
| 68 | **LFO** | 6 shapes - Sine, Triangle, Square, SawDown, SawUp, Sample-and-Hold - returning minus-depth to plus-depth. | **CLEAN** | Ours + JUCE |
| 69 | **SynthFilter** | TPT state-variable filter (two integrator states plus the g / k / a1 / a2 / a3 coefficient set), 4 modes LP / HP / BP / Notch, Q 0.1-10. | **CLEAN** | Ours + JUCE |
| 70 | **SynthSound** | Nine-line `juce::SynthesiserSound` that accepts every note and channel. | **CLEAN** | Ours + JUCE |
| 71 | **WavetableOscillator** | 2048-point table, up to 7-voice unison with per-voice phase and delta, heap-allocated tables so there is no stack-overflow risk, with saw / square / sine builders. | **CLEAN** | Ours + JUCE |
| 72 | **BroadcastSynthesiser** | A `juce::Synthesiser` subclass that broadcasts controller events to **every** voice unconditionally.  Stock JUCE only reaches voices whose playing MIDI channel matches, so idle voices missed the CC stash the piano-roll scheduler emits before each note-on.  Also implements first-match CC85 ramp takeover, which fixes two voices on the same note both retargeting and double-bending, and wipes every voice's glide stash after note-on. | **CLEAN** | Ours + JUCE |

---

## 8.  Matrix F - Analysis, utility and infrastructure DSP (25)

| # | Module | What it does | Portable | Licence |
|---|---|---|---|---|
| 73 | **LibraryPitchShifters** | The offline pitch-shift bake seam behind an `IPitchShifter` interface, dispatching to three vendored engines: **RubberBand R3 = "Balanced" (the default), Signalsmith = "Lightest (Low CPU)", WORLD = "Highest Quality (High CPU)"**.  Does a length-preserved mono shift with per-sample pitch-ratio and formant envelopes, plus a second entry point doing time-warp and pitch in one native pass (replacing an older split whose phase vocoder wobbled on sharp per-note maps). | **PARTIALLY BLOCKED-GPL** - the RubberBand branch only.  The Signalsmith (MIT) and WORLD (BSD) engines in the same file are independently `#if`-guarded and clean.  **The single chokepoint through which three of the six third-party audio libs enter the app.** | **GPL** + MIT + BSD |
| 74 | **PitchCorrectorDSP** | Realtime auto-tune.  YIN detect, MIDI float, snap to the nearest note in the active key and scale with note-change hysteresis so vibrato does not flip-flop targets, shift ratio, smoothing over retune speed, strength blend, humanize random walk in cents, into the formant-preserving live shifter (Throat maps to its formant scale). | **BLOCKED-GPL** - the whole engine is rubberband's `RubberBandLiveShifter`.  The `#else` branch is a dry passthrough, so removing the library removes the feature without breaking the build. | **GPL v2+** |
| 75 | **BaySickAlignDSP** | Offline time alignment.  Pairs transient onsets (spectral-flux peak picking on a Hann STFT) between a guide and a dub take into a piecewise-linear warp, plus frame-YIN F0 at each anchor for per-anchor pitch deltas.  User sync points are hard pairing boundaries.  Applied maps **play live** - the clip decode remaps read position through a published snapshot and the phase vocoder compounds warp slope, tempo stretch and per-anchor pitch ratio in one pass. | **BLOCKED-GPL today** (links via #73), **CLEAN after** switching the default engine.  Its own source touches only the `IPitchShifter` interface, so it is not itself a derivative work. | Ours, links **GPL** |
| 76 | **BaySickPitchDSP** | Note-level pitch editor.  YIN over the channel composite segments note regions at **absolute timeline positions**; edits (pitch shift, formant, vibrato, volume shape, elastic time moves) attach to regions and reach playback through a realtime applicator that maps running strip audio to the overlapping region by timeline position. | **BLOCKED-GPL today**, **CLEAN after** the engine swap.  Note the snapshot lifetime model: retired snapshots are held in a **ring of 8**, so safety rests on the argument that eight edit round-trips outlive any in-flight block, not on refcounting.  It currently **defaults** to the RubberBand engine at `BaySickPitchDSP.h:500`. | Ours, links **GPL** |
| 77 | **PhaseVocoder** | Laroche-Dolson phase vocoder - Hann-windowed FFT every analysis hop, instantaneous frequency from inter-frame phase delta, accumulator advanced by the synthesis hop, IFFT and overlap-add.  Window size is a **duration**, not a sample count: prepare picks the power-of-two window nearest a reference duration for the source rate, so a clip stretches identically from 44.1 to 192 kHz. | **CLEAN** - includes only JuceHeader and `<cmath>` | Ours + JUCE |
| 78 | **MonitorPitchShifter** | Low-latency time-domain shifter for the corrected live monitor.  Dual-tap delay-line crossfade (the classic live-tuner / harmonizer-pedal algorithm): two read taps sweep a 24 ms window behind the write head at the shift ratio, each gained by a half-sine peaking mid-window and hitting zero exactly where that tap wraps, so splices never click.  About 12 ms latency against roughly 48 ms for the spectral shifter. | **CLEAN** - **no rubberband, that is the entire point of it** | Ours + JUCE |
| 79 | **PitchShifters (CepstralFormantEngine)** | Cepstral spectral-envelope extraction - log magnitude, IFFT, keep the low quefrencies, FFT back - for formant PRESERVE (impose the dry frame's envelope on the wet frame, killing chipmunk artifacts) and THROAT SHIFT (scale the envelope along frequency).  The PSOLA / granular / PV trio that once lived here is retired; this file is now mostly a retirement note plus the one surviving engine. | **CLEAN** | Ours + JUCE |
| 80 | **PitchTrackerYIN** | YIN (de Cheveigne and Kawahara 2002) with CMNDF, threshold and parabolic interpolation.  Audio pushes into an SPSC ring; a background worker slides a 2048-sample window by hop 512 and publishes Hz plus confidence via atomics, roughly 50 ms to a fresh reading.  The window is a **duration** held against high sample rates by **decimating** rather than growing, because YIN's difference function is quadratic in window length - a rate-scaled window at 192 kHz would cost about 456% of one core. | **CLEAN** | Ours + JUCE |
| 81 | **PolyPitchTracker** | Polyphonic tracker for the synth pedal's poly mode - FFT magnitude spectrum, harmonic-sum scoring over a cent-spaced f0 grid, greedy iterative spectral subtraction pulling out up to 6 simultaneous fundamentals strongest first.  FFT 4096, hop 1024, publishing a note set via seqlock.  Candid in its own header that real-time polyphonic detection from summed audio is inherently approximate. | **CLEAN** | Ours + JUCE |
| 82 | **BpmDetect** | Content tempo estimation - per-frame log energy, onset energy flux, comb-scored autocorrelation.  Bounded to a 60 s read in 512-sample chunks so the whole file never loads; needs about 2 s of material.  Comb scoring is the octave-error guard.  Returns a confidence flag, and sparse or rubato material returns false. | **CLEAN** | Ours + JUCE |
| 83 | **LufsMeterDSP** | EBU R128 / BS.1770 on a stereo bus.  Momentary 400 ms and short-term 3 s ungated sliding windows; integrated gated at -70 LUFS absolute plus -10 LU relative over 400 ms blocks at 75% overlap.  K-weighting derived bilinear-from-constants per prepare, so it is exact at any rate.  The integrated gate uses a fixed-size loudness histogram, so it is allocation-free on the audio thread. | **CLEAN** | Ours + JUCE |
| 84 | **TruePeakMeter** | ITU-R BS.1770-4 Annex 2 true peak - 4x polyphase FIR, 48 taps as 4 phases of 12, peak taken across every phase.  Coefficients are **designed, not transcribed**: built from a Kaiser-windowed sinc to the same geometry as the published table, because a mis-keyed coefficient degrades the meter silently.  Flat to 20 kHz at every supported rate, stopband below -80 dB. | **CLEAN** | Ours + JUCE |
| 85 | **LoudnessSpec** | Not DSP - the one table of delivery loudness targets (Streaming -14 and -16, EBU R128 -23, ATSC A/85 -24 LKFS, BS.1770 measure-only, Custom) with tolerance, max true peak and optional short-term ceiling.  Deliberately one place so three copies of "-14 LUFS, -1 dBTP" cannot drift. | **CLEAN** | Ours + JUCE |
| 86 | **PolyphaseOversampler** | Header-only wrapper over `juce::dsp::Oversampling` with locked constructor arguments (two halving stages, max quality) so per-module argument drift cannot happen.  Exposes its latency for the rack's PDC sum. | **CLEAN** | Ours + JUCE |
| 87 | **DenoiseDSP** | Profile-based spectral subtraction in three pieces: a 513-bin noise-floor fingerprint that is base64-serializable and project-persistable; a learner whose audio-thread push is wait-free with the FFT work on a background thread, using an asymmetric per-bin follower (fast down, slow up) that tracks minima so speech never drags the floor up and skips near-silent frames so an unrouted strip does not drag it to zero; and an offline file-to-file clean whose **output sample count is identical to the input**, so every grid, beat and align offset stays valid. | **STUB** - soft LAME edge through the shared MP3-capable loader; swap the loader and it is clean | Ours + JUCE |
| 88 | **MicSimDSP** | Three-way exclusive mic fingerprint - None, Built-in (a 4-band parametric EQ over 10 mic archetypes, **generic descriptive names only, no trademarked product names in the binary**), or User IR (convolution of a user WAV; **no third-party IRs are bundled**).  Mix is wet / dry over whichever path is active. | **STUB** - SampleLibrary and ProjectFileResolver for the user-IR path, plus the soft LAME edge | Ours + JUCE |
| 89 | **MicPlacementDSP** | Virtual mic placement.  Distance 1-150 cm (inverse-distance gain, a first-order air-absorption lowpass that moves down with distance, and a proximity bass shelf that fades past about 20 cm); off-axis angle plus or minus 90 degrees (cosine rolloff scaled by polar-pattern strength plus high-shelf darkening past about 30 degrees); and 5 polar patterns - Omni, Cardioid, Supercardioid, Hypercardioid, Figure-8 with a 90-degree null and rear-lobe phase invert.  Wet / dry mix, defaulting fully wet. | **CLEAN** - **the cleanest module in this section**, includes only its own header | Ours + JUCE |
| 90 | **AudioClipStreamer** | Disk streaming for arrangement audio clips - a 4-second SPSC ring at file rate, a background prefetch client, a stateless per-block read-and-mix on audio, linear interpolation for rate conversion, and seek detection (if the read position jumps outside the ring the audio thread flags a seek, the background thread refills asynchronously, and audio returns silence meanwhile).  Files under 100 MB load entirely into RAM. | **REWORK** - carries three **process-wide statics** (offline-render flag, underrun count, peak prefill).  Also an I/O engine, not a signal processor - a plugin probably does not want it at all. | Ours + JUCE |
| 91 | **Mp3Writer** | Thin float-buffer front end over libmp3lame.  JUCE ships WAV, OGG and FLAC but no MP3 encoder, and its own LAME wrapper shells out to a `lame.exe` we would have to redistribute.  Takes JUCE's planar layout directly with no interleave step; close flushes the final frame and rewrites the VBR info header. | **BLOCKED-LGPL** - static link.  A VST3 almost certainly does not need an MP3 encoder; drop it, or ship LAME as a DLL. | **LGPL v2** |
| 92 | **EffectVisualFeed** | The audio-to-UI channel behind every effect-panel visual, and **the gate that makes an unwatched visual cost nothing**: push opens with one relaxed atomic load of a refcounted watcher count and returns immediately at zero.  A watcher is a *visible* visual; refcounted rather than boolean because a rack panel and a pedal view can watch the same DSP at once.  One column is four floats - sample envelope plus two per-effect overlays.  Torn reads are accepted. | **CLEAN** | Ours + JUCE |
| 93 | **SpectrumFeed** | Lock-free wait-free **seqlock** ring moving one 1024-sample block from audio to UI for the spectrum analyser.  Audio pushes every block; the UI polls at about 30 Hz and drops the frame rather than copying torn data.  (Distinct from the newer `kbs::SpectrumFeed`, which the EQ window uses.) | **CLEAN** | Ours + JUCE |
| 94 | **EngineSidechainHelper** | Two things: `ISidechainEngine`, the tag interface every engine processor inherits, and the per-engine member itself - 4 slots, **address-only push** (pointers are copied into a fixed array, the sample data stays owned by the graph's per-strip receive buffers), plus a published RMS any internal mod source can sample. | **REWORK** - explicitly coupled to the host render loop, and its slot count must track the routing graph's per-strip receive cap | Ours + JUCE |
| 95 | **PanLaw** | The one pan law.  Two centre-unity curves - Ramped (the circular law, near side rising 3 dB) and Flat (triangular, near side holds unity while the far side tapers linearly).  Both continuous through centre; the previous helper skipped the law at dead centre and jumped 3 or 6 dB on the first tick off it. | **REWORK** - a **process-wide mutable global**, written by the host at the top of every audio block and read by every voice.  Justified in-header for a single-project standalone app; a plugin needs it per-instance. | Ours + JUCE |
| 96 | **EffectParamMap** | Not signal DSP - the single home for which rack-effect parameters exist, their ranges, and how a value reaches **and is read back from** the DSP.  Exists because automation used to apply by driving the on-screen knob, so a lane went silently dead whenever its panel was destroyed - switching the Effects page to another channel was enough.  Both directions are needed: apply drives the DSP, read recovers the natural value for panel sync and lane seeding. | **REWORK** - a hub that includes essentially every effect header plus `EffectRack.h`, and whose suffix keys must match what the panel derives from a knob label | Ours + JUCE |
| 97 | **DSPBase** | The abstract base for every effect module.  Embeds an `EffectVisualFeed` so a new panel visual needs no new plumbing, and defines that feed's **time axis** - a column is a fixed slice of *time*, not one audio block, because publishing one column per block made the picture a function of device buffer size (the same strip spanned 1.7 s at buffer 128 and 55.7 s at 4096, stretching an 80 ms harmonic-bar hold to 2.8 s). | **CLEAN** - but holds one static the host processor sets, which a plugin must re-home | Ours + JUCE |

---

## 9.  The blocked list, in one place

Out of 97 modules, this is everything that cannot cross as it stands.

### 9a.  GPL (Rubber Band) - 2 files carry it, 3 more link it

| File | Nature |
|---|---|
| `Source/DSP/PitchCorrectorDSP.cpp:7` | **Whole engine.**  Realtime auto-tune is the live shifter.  The `#else` branch is a dry passthrough. |
| `Source/DSP/LibraryPitchShifters.cpp:39` | **One of three engines.**  Signalsmith (line 235, MIT) and WORLD (lines 355-358, BSD) in the same file are separately guarded and clean. |

Linking it today but **not derivative works** (they touch only the
`IPitchShifter` interface and a shared scale table): `BaySickAlignDSP.cpp`,
`BaySickPitchDSP.h/.cpp`, `BaySickVocalProcessor.h`, `BaySickPitchEditor.cpp`.
Those become portable the moment you build with `BAYSICK_HAS_RUBBERBAND` off
and move the default engine off RubberBand (`BaySickPitchDSP.h:500`).

**Two ways out, both cheap:**
1. Buy a Breakfast Quay commercial licence (one-time, perpetual; the standard
   tier wants an in-app credit, a non-attribution tier exists).
2. Ship KBS with Signalsmith (MIT) or WORLD (BSD) as the engine.  **Both already
   implement the same interface and are already vendored.**  For realtime
   correction specifically, `MonitorPitchShifter` is ours, JUCE-only, and was
   written precisely to avoid the spectral shifter's latency.

### 9b.  LGPL (LAME) - 2 files

`Source/DSP/Mp3Writer.cpp` and `Source/MpglibAudioFormat.h`.  Statically linked
at `CMakeLists.txt:420`, which is the one linkage mode LAME's own LICENSE tells
you to avoid.  LGPL section 6 permits static linking only if the end user can
relink against a modified LAME - object files plus linker scripts, or a written
offer.  We do neither, and the required LAME acknowledgement and link do not
appear on any credits surface.

For KBS: drop MP3 (a plugin does not need an encoder), or ship
`libmp3lame.dll` alongside.  Everything the matrix marks as a "soft LAME edge"
is a *file loader* dependency, not a DSP one - swap the loader for a plain WAV
reader and the edge disappears entirely.

### 9c.  Deep app coupling - portable only after real work

`StripEq` (owns the APVTS-sweep contract), `BaySickVocalProcessor` (reaches
five host members plus EngineRig and the editor), `BaySickRustyDrumsProcessor`
(the graph caps its channel count), `AudioClipStreamer` (process-wide statics,
and a plugin does not want it), `PanLaw` (process-wide global),
`EngineSidechainHelper` (host render-loop contract), `EffectParamMap` (a hub
over the whole effect catalogue), and the six AudioProcessor **shells** - whose
DSP cores underneath are all clean.

---

## 10.  Obligations we are not currently meeting

These are DAW-side today, and they carry over to whatever KBS reuses.

1. **No credits or acknowledgements surface exists anywhere in the repo.**  Every
   BSD / MIT / Apache dependency requires its copyright notice reproduced "in
   the documentation or other materials provided with the distribution."  That
   is sfizz plus its 17 sub-dependencies, NAM Core, WORLD, both Signalsmith
   libraries, concurrentqueue, WebView2, fontaudio, Eigen (MPL2) and Abseil
   (Apache 2.0).  One credits file covers all of it.
2. **`THIRD_PARTY_LICENSES.md` covers 4 of 14 dependencies** and says so in its
   own scope note.  It also contains the sentence "Rubber Band is GPL v2-or-later,
   compatible with this application's GPLv3 distribution" - true for the DAW,
   and exactly the sentence that stops being true for KBS.
3. **fontaudio's OFL 1.1 and CC BY 4.0 texts are missing.**  `libs/fontaudio/LICENSE`
   holds the MIT text only, but the TTF is compiled into the shipping exe as
   `FontAudioData.cpp` and OFL requires the full text to ship with it.  The
   SVGs are CC BY 4.0 and need credit.  This is the highest-priority asset item.
4. **fontaudio's embedded font contains third-party brand-logo glyphs** (FL,
   Cubase, Pro Tools, Live, Reason, VST).  **We draw none of them** - the only
   glyphs used in `Source/` are filter shapes plus solo / headphones / lock.
   Low residual risk, but it is squarely the "no real brand names in
   user-facing strings" rule.  Do not start using them.
5. **The ASIO SDK is committed to git** - 13 files under `libs/asiosdk/`, remote
   `github.com/KnowledgeBaseStudios/BaySickDAW`.  Its proprietary branch says the
   SDK "may not be distributed in parts or its entirety without prior written
   agreement by Steinberg."  If that repo is public, that is distribution.
   **I could not determine the repo's visibility** - that is the one fact that
   decides whether this is urgent or nothing.  Either way it is moot for KBS: a
   VST3 never opens an audio device.
6. **IR and filmstrip provenance is unknown.**  `Resources/Acoustic IRs/*.wav`
   (2 files), `Resources/Tape/IRs/*.wav` (10), `Resources/Tape/Samples/*_noise.wav`
   (10), `Resources/Filmstrips/*.png` (9) and `Assets/*.png` (3) carry no
   licence, README or source note.  **This is the item I would not ship a
   commercial plugin on without confirming**, because IRs are exactly the asset
   class that arrives with "free for personal use" strings attached.  If they
   are self-made, one line in a credits file settles it forever.  `Kits/`,
   `Templates/`, `Presets/` and `My Samples/` were not scanned.
7. **sfizz upstream was archived in June 2026.**  The licence is fine and we are
   pinned to 1.2.3, so there is no version skew - but sfizz parses untrusted
   input (SFZ and sample files) with no upstream security maintenance.  Any
   future CVE is ours to patch.

---

## 11.  The short answer

**Of 97 modules, exactly 4 files carry copyleft third-party code** - two GPL
(Rubber Band), two LGPL (LAME).  Everything else is either our own code or a
permissive dependency.

The **entire rack-effect set, the entire pedal set, the entire KBS Core EQ
library, every voice primitive, and every meter and tracker** is ours plus
JUCE.  The legacy VST3 target already compiles that whole DSP source set
against three JUCE modules and nothing else, which is a standing proof rather
than an estimate.

**JUCE costs nothing and blocks nothing** at current revenue (see 1c), so it is
not on this list.

What actually needs deciding, in order:

1. **Rubber Band: buy or swap?**  Swapping is close to free - two other engines
   are already vendored behind the same interface, and the realtime path has a
   JUCE-only alternative we already wrote.  This is now the **only** thing
   forcing copyleft on the codebase.
2. **MP3 in KBS: drop it.**  Almost certainly not needed in a plugin, and it
   removes the only LGPL obligation.
3. **Write one credits file.**  It closes items 1, 2, 3 and 4 of section 10 at
   once, and KBS needs the same file anyway.
4. **Confirm the IR and filmstrip provenance.**  The one open question with
   genuinely unknown risk.

---

*Compiled from three parallel audits (vendored-library licensing, effects and
pedal inventory, instrument and utility inventory).  Every licence claim was
re-verified directly against the bundled licence text; the third-party include
surface was re-verified by grepping `#include` directives rather than name
mentions, because a bare-name grep returns roughly 50 false positives for sfizz
alone.  JUCE's free-tier terms are per Jeff's own confirmation (2026-09-01).
Lower confidence, not confirmed from a file in this repo: Rubber Band commercial
pricing, sfizz's archive date, and the GitHub repo's public or private status.*
