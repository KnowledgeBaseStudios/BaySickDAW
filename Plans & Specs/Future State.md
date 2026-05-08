# Future State — Post-V1 Roadmap

> **Append-only.** This document holds work that is NOT V1 material but
> remains on the long-term roadmap.  Includes:
>
> - Tier 3 / future-state items from the source documents
>   (`Final Stretch Work.txt`, `vibedaw_blueprint.md`, `lucky-discovering-tiger.md`).
> - Claude-proposed additions based on current build state + the project's
>   stated ambitions (feature-rich DAW for beginners + audio quality at
>   the Valhalla / Omnisphere / Keyscape tier).
> - Items considered & dropped during QA-Inventory and later sweeps (with
>   reason captured so we don't re-litigate).

**Distinct from `Main Plan.md` §5**, which holds active V1 batches.  Items
in this doc are intentionally NOT scheduled.  A future-state item may
graduate to the active plan if priorities shift; that promotion gets a
§9 Forks entry in the main plan.

---

## How to read this doc

Two top-level sections:

1. **Future Items by Domain (281 items)** — every post-V1 candidate
   organised into the 10 canonical domain buckets defined in Main Plan
   §0 ("Document Formatting Conventions").  Source-doc Tier 3 items
   (`BLU-*`, `FSW-*`, `LDT-*`), Claude fire-hose additions (`CL-*`),
   and walked-from-V1 items all live together under the bucket they
   touch.  Re-organised 2026-05-08 from the original four-section
   layout.
2. **Considered & Dropped (18 items)** — items reviewed during
   QA-Inventory walkthrough (or post-walk Phase-4 reclassify) and
   confirmed dropped, with reason.  Stays grouped by drop reason
   (not by domain) so we don't re-litigate; cross-reference if you
   ever change your mind.

Each Future Items entry is a one-liner: **`[ID / TAG] Title — short
description.`**  Domain answers "what part of the app this touches"
(Effects, Players, etc.); value tag answers "why ship it" (AQ / PE /
UT / WP / OT).

Tags per category:

- **AQ** — Audio Quality (highest value)
- **PE** — Performance / Efficiency
- **UT** — User Tools / Learning Experience
- **WP** — Workflow Polish
- **OT** — Other / Platform / Cross-cutting

## Header conventions

Cross-doc rules live in [Main Plan.md](Main Plan.md) §0 "Document
Formatting Conventions".  Local layout for this doc:

- `#` — document title.
- `# Section N — <Title>` — major band (Section 1 / Section 2).
- `## <Bucket>` — top-level canonical domain bucket inside Section 1.
  Every bucket from Main Plan §0 appears as a `## ` header here even
  when the bucket has zero items in Future State (stubbed with a
  one-liner) so cross-doc grep `^## <BucketName>` finds the section
  in every doc.
- `### <Sub-cluster>` — sub-cluster inside a bucket (`### §1 ChorusDSP`,
  `### §P1 Harmless`, `### Fire-Hose net-new effect modules`, etc.).
- `- **[<ID> / <TAG>]** <Title> — <description>.` — single-line entry.

Grep patterns:

- `^# Section ` finds all major bands.
- `^## ` finds all canonical buckets.
- `^### ` finds all sub-clusters.
- `^- \*\*\[` finds all entries.

Append-only.  New items go under their natural domain bucket alongside
the existing source-doc + fire-hose + walked entries from the same
domain.  Section 2 (Considered & Dropped) stays grouped by drop reason.

---

# Section 1 — Future Items by Domain

Total: 281 items, organised into the 10 canonical domain buckets.
Each entry preserves its original source ID (`BLU-*` from
`vibedaw_blueprint.md`, `FSW-*` from `Final Stretch Work.txt`,
`LDT-*` from `lucky-discovering-tiger.md`, `CL-*` from Claude
fire-hose additions on 2026-05-08).

## Effects

DSP modules + FX rack mechanics + future / restoration effects.

### §1 ChorusDSP
- **[BLU-016 / AQ]** Algorithm voicing presets — Chorus / Ensemble / Dimension-D preset chooser per voicing. PRESET-SAFE.
- **[BLU-017 / AQ]** Per-LFO phase tap offsets to UI — expose hardcoded `extraPh = π` for voices 3-5. PRESET-SAFE.
- **[BLU-018 / AQ]** Tempo-sync on LFO freqs — per-LFO bool + division selector. PRESET-SAFE.
- **[BLU-019 / AQ]** Feedback-into-delay (C5) — float 0..0.95 fractional feed of chorus output back to delay write. PRESET-SAFE.

### §2 CompressorDSP
- **[BLU-038 / AQ]** Multi-stage program-dependent release — LA-2A opto-style chain. PRESET-SAFE.
- **[BLU-039 / AQ]** Feedforward vs feedback topology toggle — 1176 character via feedback. PRESET-SAFE.
- **[BLU-040 / AQ]** External sidechain source routing UI — mixer-channel dropdown sets `sidechainSourceId`. PRESET-BREAK at graph level.
- **[BLU-041 / AQ]** LUFS-referenced detection for mastering — K-weighting filter + integrated LUFS. PRESET-SAFE.

### §3 DelayDSP
- **[BLU-064 / AQ]** Dual stereo independent delay times — L/R different base delays with own sync. PRESET-SAFE.
- **[BLU-065 / AQ]** Tape-mode emulation preset bundle — keep-pitch off + wow/flutter LFO + lo-fi. PRESET-SAFE.
- **[BLU-066 / AQ]** Reverse delay mode — new `mDelayModel` enum value. PRESET-SAFE.
- **[BLU-067 / AQ]** Dedicated wow/flutter LFO — separate from `mModRate`. PRESET-SAFE.
- **[BLU-068 / AQ]** Spectral delay (per-band delay times via FFT) — block-processing subsystem. PRESET-BREAK; APVTS scaffolding shipped in v1.

### §4 FlangerDSP
- **[BLU-088 / AQ]** Dry / Wet / CrossLevel dB knobs (C5) — Flanger Tier 2 deferred. DSP exists but only Cross UI-exposed. PRESET-SAFE. _(walked from V1 backlog 2026-05-08.)_
- **[BLU-089 / WP]** Pro mix section redesign — replace 0..1 Wet blend with WetDb+DryDb+CrossDb trims. PRESET-SAFE.
- **[BLU-090 / AQ]** Through-zero (TZ) flanging mode — dual delay lines for jet-flanger character. PRESET-SAFE.
- **[BLU-091 / AQ]** Secondary modulator (env follower → Rate/Depth) — per pro flangers like MXR 117. PRESET-SAFE.
- **[BLU-092 / AQ]** Independent L/R base-delay (stereo offset split). PRESET-SAFE.
- **[BLU-093 / AQ]** Resonant feedback-filter — BP or peaking EQ in feedback loop. PRESET-SAFE.

### §5 LimiterDSP
- **[BLU-107 / AQ]** Spectral / multi-band limiting — separate limiter per band for mastering. PRESET-SAFE.
- **[BLU-108 / AQ]** Auto-ceiling (true-peak aware ceiling) — Spotify/YouTube margin. PRESET-SAFE.
- **[BLU-109 / AQ]** Release-character voicing presets — Transparent / Punchy / Vintage. PRESET-SAFE.
- **[BLU-110 / AQ]** LUFS / dBFS side-by-side output meter. PRESET-SAFE.
- **[BLU-111 / AQ]** Oversampling factor UI — 2x / 4x / 8x; currently hardcoded 4x. PRESET-SAFE.

### §6 OverdriveDSP
- **[BLU-127 / AQ]** Multi-stage cascade shaper — 2 or 3 atan/sigmoid stages in series. PRESET-SAFE.
- **[BLU-128 / AQ]** Pre-emphasis + de-emphasis EQ curves — guitar-amp-style frequency shaping. PRESET-SAFE.
- **[BLU-129 / AQ]** Cab simulator IR post-stage — IR-based cab. PRESET-SAFE.
- **[BLU-130 / AQ]** Character voicing presets — Tube / Fuzz / Distortion / BitCrush bundles. PRESET-SAFE.

### §7 PhaserDSP
- **[BLU-146 / AQ]** C6 SmoothedValue crossfade on Stages count — Phaser Tier 2 deferred. §7b state-preserve covers in practice. PRESET-SAFE. _(walked from V1 backlog 2026-05-08.)_
- **[BLU-147 / AQ]** Dry/Wet/CrossLevel dB knobs — Phaser Tier 2 deferred; consistency follow-on to §4 Flanger. PRESET-SAFE. _(walked from V1 backlog 2026-05-08.)_
- **[BLU-148 / AQ]** Per-stage resonance LFO offset (T1) — Mu-Tron warble. PRESET-SAFE.
- **[BLU-149 / AQ]** Secondary modulator env follower (T2) — mirror of §4 Flanger T3. PRESET-SAFE.
- **[BLU-150 / AQ]** Feedback filter LP/HP/BP (T3) — mirrors §3 Delay FB filter. PRESET-SAFE.
- **[BLU-151 / AQ]** Barberpole phaser mode (T4) — Shepard-tone effect. PRESET-SAFE.

### §8 ReverbDSP
- **[BLU-153 / AQ]** Shimmer / pitch-shift feedback — octave/user-selectable pitch shift. PRESET-SAFE.
- **[BLU-154 / AQ]** Ducking reverb (dynamic feedback) — env follower on input sidechains feedback. PRESET-SAFE.
- **[BLU-155 / AQ]** Algorithm presets (Hall/Chamber/Plate/Room) — preset chooser, optional FDN topology swap. PRESET-SAFE.
- **[BLU-156 / AQ]** Per-tap ER editor UI — visual editor for individual tap times+gains. PRESET-SAFE.
- **[BLU-157 / AQ]** Dedicated Reverse / Gated modes — reverse-envelope or noise-gate the tail. PRESET-SAFE.

### §9 SaturationDSP
- **[BLU-169 / AQ]** Asymmetric sigmoid as Tube Type D (T1) — even harmonics. PRESET-SAFE.
- **[BLU-170 / AQ]** Pre/de-emphasis EQ curves (T2) — guitar-amp style frequency-shaped distortion. PRESET-SAFE.
- **[BLU-171 / AQ]** Multi-stage cascade shaper (T3) — 2-3 shapers in series. PRESET-SAFE.
- **[BLU-172 / AQ]** Multi-band saturation (T4) — 3 bands with own Flowers/Dabs/Type. PRESET-SAFE.
- **[BLU-173 / AQ]** Character voicing presets (T5) — Tube/Console/Tape/Fuzz; depends on preset loader UI. PRESET-SAFE.

### §10 TapeDSP
- **[BLU-194 / AQ]** IR-based cassette frequency profile (T1) — `juce::dsp::Convolution` Type I/II IRs. PRESET-SAFE.
- **[BLU-195 / AQ]** Type II / IV cassette variant presets (T2) — different emphasis curves + hiss. PRESET-SAFE.
- **[BLU-196 / AQ]** Isolation mode (T3) — mode enum to bypass individual stages. PRESET-SAFE.
- **[BLU-197 / AQ]** Multi-head simulation (T4) — 3-head tape machines. PRESET-SAFE.
- **[BLU-198 / AQ]** Drop-outs (T5) — brief random amplitude dips. PRESET-SAFE.
- **[BLU-199 / AQ]** Print-through (T6) — ghost pre-echoes from adjacent tape layers. PRESET-SAFE.

### §11 TransientShaperDSP
- **[BLU-214 / AQ]** Multiband transient shaper (T1) — 3+ bands with per-band shaping. PRESET-SAFE.
- **[BLU-215 / AQ]** Look-ahead detector (T2) — preemptive attack boost. PRESET-SAFE.
- **[BLU-216 / AQ]** Sidechain input (T3) — external-trigger shaping. PRESET-SAFE.
- **[BLU-217 / AQ]** Character voicing presets (T4) — Punchy/Tight/Soft/Retro. PRESET-SAFE.

### §12 EQ8DSP
- **[BLU-254 / AQ]** Orfanidis analytical anti-cramping (T1) — alternative to 12f oversampling. PRESET-SAFE.
- **[BLU-255 / AQ]** Match-EQ (T2) — reference track spectrum analysis. PRESET-SAFE.
- **[BLU-256 / UT]** Auto-suggest cuts (T3) — AI-assisted mixing. PRESET-SAFE.
- **[BLU-257 / AQ]** EQ preset bank (T4) — Vocal/guitar/bass/drums tonal presets. PRESET-SAFE.
- **[BLU-258 / AQ]** M/S spectrum analyzer overlay (T5) — per-band M/S spectrum display. PRESET-SAFE.
- **[BLU-259 / AQ]** User-selectable filter engine per band (T6) — Biquad / TPT / Auto override. PRESET-SAFE.
- **[BLU-260 / AQ]** Notch via TPT BP-subtract (T7) — 2x filter state per notch. PRESET-BREAK if enacted.
- **[BLU-261 / AQ]** TPT shelves (T8) — SVF mode-mixing for shelves. PRESET-SAFE.
- **[BLU-262 / AQ]** Ladder / Moog-style filter engine (T9) — third per-band filter topology. PRESET-SAFE.
- **[BLU-263 / AQ]** Bandpass Q remapping (T10) — engine-consistent Q. PRESET-BREAK if retro.
- **[BLU-264 / AQ]** External sidechain detector for Dynamic EQ (T11). PRESET-SAFE.
- **[BLU-265 / AQ]** Variable oversampling factor for AC (T12) — 2x/4x/8x chicken-head. PRESET-SAFE.
- **[BLU-266 / AQ]** FIR-equiripple oversampler option for AC (T13) — alternative to IIR half-band. PRESET-SAFE.
- **[BLU-267 / PE]** Auto-enable AC heuristic (T14) — when band freq exceeds sr/4. PRESET-SAFE.
- **[BLU-268 / AQ]** Multi-IR linear-phase per-band M/S (T15) — up to 4 parallel linear-phase IRs. PRESET-SAFE.
- **[BLU-269 / PE]** Per-block dynamic IR rebuild in linear-phase (T16) — 5-10% CPU per dynamic band. PRESET-SAFE.
- **[BLU-270 / AQ]** User-selectable FFT size for linear-phase (T17) — 256/512/1024/2048/4096/8192. PRESET-SAFE.
- **[BLU-271 / AQ]** Linear-phase IR crossfade transitions (T18) — double-buffer IR crossfade. PRESET-SAFE.
- **[BLU-273 / OT]** Extract shared `EQ8Defaults` header (T20) — single source of truth for default freqs. PRESET-SAFE.
- **[BLU-494 / AQ]** Orfanidis analytical anti-cramping — replaces §12f oversampling. PRESET-SAFE.

### Future Effect Modules — Dynamics
- **[BLU-274 / AQ]** Gate / Downward Expander — inverted compression envelope. PRESET-SAFE.
- **[BLU-275 / AQ]** Upward Compressor — OTT-style sound design. PRESET-SAFE.
- **[BLU-276 / AQ]** Multi-band Compressor — `juce::dsp::LinkwitzRileyFilter` crossovers. PRESET-SAFE.
- **[BLU-277 / AQ]** De-Esser — frequency-conscious compressor sidechained. PRESET-SAFE.
- **[BLU-278 / AQ]** Auto-Gain / Vocal Rider — `BallisticsFilter` for slow leveling. PRESET-SAFE.
- **[BLU-279 / AQ]** Maximizer — mastering-grade limiter variation. PRESET-SAFE.

### Future Effect Modules — Harmonics
- **[BLU-280 / AQ]** Bitcrusher — bit-depth + sample-and-hold quantization. PRESET-SAFE.
- **[BLU-281 / AQ]** Wavefolder — folds waveform back, metallic synth tones. PRESET-SAFE.
- **[BLU-282 / AQ]** Exciter / Harmonic Enhancer — HPF + distortion blend. PRESET-SAFE.
- **[BLU-283 / AQ]** Ring Modulator — multiplies signal by carrier oscillator. PRESET-SAFE.
- **[BLU-284 / AQ]** Subharmonic Generator (Octaver) — pitch tracker + sine/square octave below. PRESET-SAFE.
- **[BLU-285 / AQ]** Comb Filter — short delay + heavy mix. PRESET-SAFE.

### Future Effect Modules — Time-Based
- **[BLU-286 / AQ]** Convolution (IR Loader) — `juce::dsp::Convolution` + WAV IRs. PRESET-SAFE.
- **[BLU-287 / AQ]** Pitch Shifter — reuses existing `PhaseVocoder`. PRESET-SAFE.
- **[BLU-288 / AQ]** Granular Processor — 10–100 ms grains. PRESET-SAFE.
- **[BLU-289 / AQ]** Stereo Widener (Haas Effect) — tiny L/R delay. PRESET-SAFE.
- **[BLU-290 / AQ]** Reverse Buffer — circular buffer with reverse read. PRESET-SAFE.

### Future Effect Modules — Modulation
- **[BLU-291 / AQ]** Tremolo — LFO modulating amplitude. PRESET-SAFE.
- **[BLU-292 / AQ]** Auto-Pan — LFO modulating `juce::dsp::Panner`. PRESET-SAFE.
- **[BLU-293 / AQ]** Vibrato — chorus with dry muted. PRESET-SAFE.
- **[BLU-294 / AQ]** Envelope Filter (Auto-Wah) — env follower + LadderFilter. PRESET-SAFE.
- **[BLU-295 / AQ]** Frequency Shifter — Hilbert transform analytic signal. PRESET-SAFE.
- **[BLU-296 / AQ]** Rotary Speaker (Leslie Sim) — Tremolo + AutoPan + Vibrato combination. PRESET-SAFE.

### VST3 Plugin Hosting (~2-3 weeks total)
- **[BLU-297 / OT]** VST3 Plugin Hosting (umbrella) — 3rd-party VST3 effect plugins in FX rack.
- **[BLU-298 / OT]** Plugin scanner — background thread, build known-plugins list. ~3-5 days.
- **[BLU-299 / OT]** Plugin browser UI — search/filter scanned plugins. ~2-3 days.
- **[BLU-300 / OT]** EffectRack integration — new slot type `EffectType::VST3Plugin`. ~3-4 days.
- **[BLU-301 / OT]** Latency reporting — read plugin `getLatencySamples()`. ~1 day.
- **[BLU-302 / OT]** Crash protection (sub-process hosting) — optional, ~3-6 weeks.

### New effect modules from §P3-CORE Cross-Apply (Round 4)
- **[BLU-513 / AQ]** Ring Modulator effect (new module) — Round 4 §P3-CORE Cross-Apply.
- **[BLU-514 / AQ]** Transient Injector effect (new module) — distinct from Transient Shaper.
- **[BLU-515 / AQ]** Gate / Rhythmic Tremolo effect — multi-burst engine drives.
- **[BLU-516 / AQ]** Analog Drift / Tape Pitch Wander effect — standalone subtle pitch-drift.

### Effect-panel preset loader
- **[BLU-499 / WP]** Effect-panel preset loader UI — 3 approach options. PRESET-SAFE.

### Fire-Hose net-new effect modules
- **[CL-015 / AQ]** Convolution body resonance — short IRs of guitar bodies / drum shells; per-engine optional.
- **[CL-016 / AQ]** Multiband EQ-comp combo — single panel that does EQ + dynamic gain per band (Soothe/Pro-MB style).
- **[CL-017 / AQ]** Multiband stereo widener — per-band M/S width control (distinct from BLU-289 Haas).
- **[CL-018 / AQ]** Bass enhancer — psychoacoustic harmonics for perceived bass on small speakers (MaxxBass-style).
- **[CL-019 / AQ]** Sub bass synthesis from kick — track kick transients, generate sine sub locked to fundamental.
- **[CL-020 / AQ]** Multitrack tape emulation — full multitrack tape with crosstalk, head bump, per-track wear.
- **[CL-021 / AQ]** Console emulation — master bus saturation modeling SSL/Neve/API character per channel.
- **[CL-022 / AQ]** Spring reverb dedicated — Hammond-organ / Fender-amp style spring.
- **[CL-023 / AQ]** Stutter / glitch effect — beat-divided stutter with random length, gate, retrigger.
- **[CL-024 / AQ]** Auto-tune effect (T-Pain style) — colorful pitch-correction distinct from `BaySickPitch` precision tool.
- **[CL-025 / AQ]** Chord harmonizer — key-aware harmony generator that adds 3rd/5th/7th to monophonic input.
- **[CL-026 / AQ]** Vocal exciter — high-frequency harmonic generation specifically tuned for vocal presence.
- **[CL-027 / AQ]** De-noise (adaptive spectral) — train on noise floor, subtract.
- **[CL-028 / AQ]** De-reverb (adaptive) — phase-vocoder based reverb subtraction.
- **[CL-029 / AQ]** De-clipper — reconstruct clipped audio waveform.
- **[CL-030 / AQ]** Click/pop removal — transient detect + interpolate.
- **[CL-031 / AQ]** Adaptive equalizer — multiband level-matching to a reference (extends BLU-255 Match-EQ to be runtime-adaptive).
- **[CL-032 / AQ]** Dynamic resonance taming — Soothe-style auto-EQ-cut on resonant peaks.

## Players

Every sound-producing engine: Harmless, BaySickPlayer, BaySick family
(Synth + Bass), dynamic-drum work (Phase D), BaySickVocal, BaySickPedals,
BaySickRustyDrums, BaySickGuitars, BaySickBasses, BaySickNAM/IR + future
engines (Wavetable / FM / Analog / Modal / Strings / Vocoder / etc.).

### §P1 Harmless
- **[BLU-330 / AQ]** Harmonizer module (T3-Harm) — re-add 6 `harm_*` + Harmonizer panel + DSP. PRESET-SAFE.
- **[BLU-331 / AQ]** Image resynthesis (T3-Img) — drag PNG → 516 partials.
- **[BLU-332 / AQ]** Audio resynthesis (T3-Aud) — drag WAV → harmonic analysis.
- **[BLU-333 / AQ]** Spectral unit-order reordering (T3-Reorder) — Advanced tab drag-to-reorder.
- **[BLU-334 / AQ]** Up to 9-voice unison expansion (T3-9Voice) — if T2-C ships smaller.
- **[BLU-335 / AQ]** Curve editor enhancements (T3-Curve) — Bezier tension, looping segments.
- **[BLU-336 / AQ]** Legato curve-shape toggles (T3-LegatoCurves) — Linear/exp/log/S-curve. PRESET-SAFE.
- **[BLU-337 / AQ]** Filter osc rate knob (T3-FltOsc) — Filter-FM modulation. PRESET-SAFE.
- **[BLU-338 / AQ]** Filter secondary topology dropdown (T3-FltShape) — 2-pole/4-pole/ladder/SVF. PRESET-SAFE.
- **[BLU-339 / AQ]** Pluck curve-shape toggle (T3-PluckCurve) — alternative pluck-decay envelopes. PRESET-SAFE.
- **[BLU-340 / AQ]** Phaser LFO waveform dropdown (T3-PhaserShape) — Triangle/Sine/Saw/Square. PRESET-SAFE.
- **[BLU-341 / AQ]** EQ output-curve dropdown (T3-EQShape) — Peak/shelf/parametric beyond tilt. PRESET-SAFE.
- **[BLU-342 / AQ]** Per-source A/B/Both routing (T3-PerPartModRouting) — re-adds GLOBAL toggle. PRESET-SAFE.
- **[BLU-343 / AQ]** Harmor-style Advanced envelope tab (T3-HarmlessADV) — unit order, poly-rel, ramp. PRESET-SAFE.
- **[BLU-344 / AQ]** Pattern-time automation of mod editor knobs (T3-ModMatrixAutomation) — DEPTH/LENGTH not APVTS-backed. PRESET-SAFE.
- **[BLU-345 / AQ]** Note duration plumbing (T3-NoteDurationAwareEnvelopes) — custom MIDI CC pair. PRESET-SAFE.
- **[BLU-346 / WP]** Explicit sustain markers (T3-PerPointSustain) — right-click point Mark as sustain. PRESET-SAFE.
- **[BLU-347 / AQ]** Harmor IMG tab (T3-HarmlessImgTab) — spectral image resynthesis. PRESET-SAFE.
- **[BLU-348 / AQ]** Real-time per-voice additive synthesis (T3-RealTimeAdditive) — true Harmor architecture. PRESET-SAFE.

### §P2 BaySickPlayer (VibePlayer-prefix retained)
- **[BLU-362 / AQ]** Stretch-to-note-length — note-duration-aware sample stretch. PRESET-SAFE.
- **[BLU-363 / AQ]** VPFilterUI / VPLoFiUI / VPArticUI — expose existing READ-ONLY params. PRESET-SAFE.
- **[BLU-364 / WP]** Knob-styling audit (T3-VPKnobStyle).
- **[BLU-365 / WP]** PAN outer-ring indicator (T3-VPPanRing). PRESET-SAFE.
- **[BLU-366 / WP]** SFZ loop-opcode UI (T3-VPLoopUI). PRESET-SAFE.
- **[BLU-367 / AQ]** Keyboard-tracking filter amount (T3-VPKbTrack). PRESET-SAFE.
- **[BLU-368 / AQ]** Full Mono Mode + legato + portamento (T3-MonoMode). PRESET-SAFE.
- **[BLU-369 / OT]** BaySickDrums componentID pass (stale per Phase D rename — defunct).
- **[BLU-370 / WP]** Re-enable global vignette when GL renderer lands (T3-LRX5Vignette). PRESET-SAFE.

### §P3 BaySick family (Synth + Bass)
- **[BLU-401 / AQ]** Oversampling on DeafSaw / FM / Bell — part of Phase 5F-9. PRESET-SAFE.
- **[BLU-403 / AQ]** Anti-click fades on param jumps. PRESET-SAFE.
- **[BLU-404 / AQ]** Per-note portamento via CC / pattern metadata — requires pattern-infra plumbing. PRESET-SAFE.
- **[BLU-405 / AQ]** Aftertouch → cutoff / volume — aftertouch routing. PRESET-SAFE.
- **[BLU-406 / AQ]** Waveform morph/blend knob — crossfade between two waveforms. PRESET-SAFE.
- **[BLU-407 / AQ]** T3.7 Filter KB-track as cents/oct slider — BaySick family. _(walked from Considered & Dropped → future-state 2026-05-08; the Section 2 Drop entry is preserved for traceability.)_
- **[BLU-408 / AQ]** LFO sync-to-host re-promotion if T2.1 picks remove later. PRESET-SAFE.

### §P4 DrumsPage (Discovery deferred)
- **[BLU-424 / WP]** Per-slot automation lanes — PATTERN-BREAK; PatternManager refactor.
- **[BLU-425 / AQ]** Per-slot sidechain routing — extends §P3 routing.
- **[BLU-426 / UT]** Kit morph (A/B + morph%).
- **[BLU-427 / WP]** Per-slot freeze-to-audio — needs offline-render scaffolding.
- **[BLU-428 / WP]** External MIDI controller pad mapping — settings.json scope.
- **[BLU-429 / UT]** TR-808 step sequencer view — alt editor for `drumRoll`.
- **[BLU-430 / WP]** Custom slot count — KIT-BREAK MAJOR.

### §P6 BaySickVocal (full module)
- **[BLU-607 / AQ]** BaySickVocal H-1 Skeleton — APVTS layout covering every chain stage.

### §P8 VST3 Instrument Hosting
- **[BLU-447 / OT]** VST3 Instrument Hosting — 3rd-party VST3 instruments per tab. ~1-2 weeks.

### Fire-Hose new synth engines (peer to Harmless / BaySickSynth / BaySickBass)
- **[CL-001 / AQ]** Wavetable synthesizer (`BaySickWavetable`) — full standalone engine with morph, harmonics editor, multi-frame wavetables, FFT-based distort/transform, drag-WAV-to-wavetable import.
- **[CL-002 / AQ]** FM synthesizer (`BaySickFM`) — 4-op or 6-op DX-style with algorithm presets, ratio + fixed Hz operator modes, integrated envelopes per op.
- **[CL-003 / AQ]** Subtractive analog synth (`BaySickAnalog`) — Minimoog-style 3-osc + LFO + filter envelope + amp envelope with character-specific drift modeling.
- **[CL-004 / AQ]** Phase-distortion synth (`BaySickCZ`) — Casio CZ-style 8-waveform PD with envelope-modulated phase shaping.
- **[CL-005 / AQ]** Modal/physical-modeling percussion synth (`BaySickModal`) — bell, marimba, drum membrane, gong via modal resonator banks.
- **[CL-006 / AQ]** Karplus-Strong plucked-string synth (`BaySickStrings`) — physical-modeled string with pluck position, damping, body resonance.
- **[CL-007 / AQ]** Vocoder engine (`BaySickVocoder`) — carrier/modulator vocoder with 16-32 bands, formant preservation, hold mode.
- **[CL-008 / AQ]** Talkbox emulation — formant-shaping the carrier through tracked-formant filters.

### Fire-Hose sample / sample manipulation
- **[CL-009 / AQ]** Pitched-sample mode for VibePlayer — auto-stretch a one-shot into a sustained instrument across the keyboard.
- **[CL-010 / AQ]** Reslicer — auto-slice loops into per-transient slices, drum-machine playback.
- **[CL-011 / AQ]** Granular FX module (rack effect, distinct from BLU-288 sampler) — apply granular to any audio bus.
- **[CL-012 / AQ]** Spectral freezer — capture spectral state, hold indefinitely, blend with current.
- **[CL-013 / AQ]** Resynthesis (drag audio → harmonics) — distinct from BLU-332; standalone tool that produces a spectrum file usable by multiple engines.
- **[CL-014 / AQ]** Pitch-correction for instruments — `BaySickPitch` extended to monophonic guitar/bass/horn input.

## Mixer / Routing

Mixer page, mixer strips, routing graph, sends + aux strips, cable
overlay, mastering chain, metering.

### Fire-Hose mixing / mastering / metering
- **[CL-033 / AQ]** Spatial audio output (Dolby Atmos / binaural) — extends LDT-427 surround.
- **[CL-034 / AQ]** Headphone monitor mode (HRTF crossfeed) — speaker-emulation on headphones.
- **[CL-035 / AQ]** K-weighted LUFS metering on every bus — distinct from BLU-110 (limiter only).
- **[CL-036 / AQ]** True-peak monitoring on master — 4× oversampled inter-sample peak detector.
- **[CL-037 / AQ]** Phase correlation meter per bus — sticky bar meter for mono compatibility.
- **[CL-038 / AQ]** Goniometer / vectorscope per channel — visual stereo-image diagnostic.
- **[CL-039 / AQ]** Spectrum animation in mixer strips — mini real-time spectrum on each strip.
- **[CL-040 / AQ]** Stem export (remix-ready) — render each tab's output as separate WAV.
- **[CL-041 / AQ]** Reference-track A/B comparison tool — drag a reference WAV, A/B with project mix.
- **[CL-042 / AQ]** Master bus chain templates — genre-specific mastering chain presets (Pop/Rock/Hip-hop/EDM/Acoustic).
- **[CL-043 / AQ]** Mastering-grade dither — selectable dither algorithms (POW-r, TPDF, noise-shaped).
- **[CL-044 / AQ]** Real-time spectrum analyzer master out — large floating window with peak/avg traces.
- **[CL-045 / AQ]** Loudness normalization on bounce — target Spotify/YouTube/Apple loudness on export.
- **[CL-046 / AQ]** Auto-mixing assist — AI suggests track levels + pan placement based on genre.
- **[CL-047 / AQ]** Stem-from-audio (drag song in, get stems) — extends LDT-428 to in-app workflow.

## System Pages

Builder, Effects Page, Audio Settings, Project Persistence, Keyboard /
Mouse docs.

### Source-doc backlog
- **[BLU-478 / WP]** TB-T1 LAT readout in ms — restore millisecond conversion. PRESET-SAFE.
- **[BLU-480 / WP]** FX-1 Rack UI refactor — sidebar picker + detail pane. PRESET-SAFE.
- **[BLU-487 / WP]** Sample Browser — real file-system tree, drag from disk. Post-Phase H/I.

## UI / L&F / Theming

VibeLAF, palette, themes, layouts, pattern colours, ribbon visuals,
knob styling, look-and-feel cross-cuts.

### LRX-5 / LRX-8 — GLSL shader / vignette
- **[BLU-489 / WP]** T3-LRX5Vignette — global lens vignette gated on GL renderer. PRESET-SAFE.
- **[BLU-490 / WP]** T3-LRX8GLSL GLSL shader realism pass — knob skirts, panel textures, spec highlights. PRESET-SAFE.
- **[FSW-334 / WP]** LRX-8 GLSL Shader pass — post-v1.0 GPU realism path.
- **[LDT-139 / WP]** LRX-8 GLSL Shader (master checklist L&F).
- **[LDT-231 / WP]** GLSL shaders — DEFERRED post-v1.0.
- **[LDT-286 / WP]** GLSL Shader (Optional GPU Path) — OpenGL shader.
- **[LDT-422 / WP]** GLSL/OpenGL shader rendering (LRX-8) — complexity + GPU compatibility.

## Cross-cutting Infrastructure

Engine (MT render path), `RenderGraphDispatcher`, BlockContext, audio
device init, MIDI input, recording lifecycle, project persistence,
audio-device infrastructure, performance / efficiency optimization.

### Performance / Efficiency (Fire-Hose)
- **[CL-048 / PE]** SIMD vectorization audit — across all DSP loops; `juce::dsp::SIMDRegister` where partial.
- **[CL-049 / PE]** GPU offload for FFT-based effects — linear-phase EQ, reverb, granular via OpenGL compute shaders.
- **[CL-050 / PE]** AVX2 / AVX-512 runtime detection + path selection.
- **[CL-051 / PE]** Disk-based sample streaming optimization — read-ahead tuning per sample size.
- **[CL-052 / PE]** Background sample pre-load — predict next-likely sample, pre-load into RAM cache.
- **[CL-053 / PE]** Smarter voice management — priority-based stealing across all engines, not just per-engine.
- **[CL-054 / PE]** Per-engine CPU budgets — cap voices per engine when CPU exceeds threshold.
- **[CL-055 / PE]** Smart freeze — auto-freeze tracks when CPU exceeds 80%; restore on demand.
- **[CL-056 / PE]** Adaptive block sizing — small block for live monitoring, large for offline render.
- **[CL-057 / PE]** ASIO buffer-size hot-swap — change buffer without engine restart.
- **[CL-058 / PE]** Per-effect CPU % display — which slot is the hog (in mixer strip + Effects page).
- **[CL-059 / PE]** Memory profiler — per-engine RAM usage display + leak detection.
- **[CL-060 / PE]** Faster project load — parallel page restoration; lazy-load non-active tabs.
- **[CL-061 / PE]** Async preset load — don't block UI on preset switch.
- **[CL-062 / PE]** Lock-free everywhere — audit remaining locks in audio path; replace with atomic / RCU patterns.

### Audio-Device Infrastructure (walked)
- **[FSW-121 / PE]** RAM-load <15MB clips — replaces AudioClipStreamer cold-start sputter. Post-V1 optimization (MT engine reduces but doesn't eliminate the symptom). _(walked from V1 backlog 2026-05-08.)_

## User Tools / Learning

AI helpers (Composer / Mixer / Master), tutorials, smart melody / chord
/ drum / bass generators, hover-to-hear, scale picker, beat detection,
sound-design guides.

### Fire-Hose AI / smart features
- **[CL-063 / UT]** AI assistant "Composer" — generate full song from text prompt.
- **[CL-064 / UT]** AI assistant "Mixer" — auto-mix the current project.
- **[CL-065 / UT]** AI assistant "Master" — auto-master to genre target.
- **[CL-066 / UT]** Smart melody generator — key/scale-aware melody from chord progression.
- **[CL-067 / UT]** Smart chord progression generator — genre-aware progression suggestions.
- **[CL-068 / UT]** Smart drum pattern generator — style-aware (trap/house/rock/funk) based on tempo + signature.
- **[CL-069 / UT]** Smart bass line generator — follows kick + chord progression.
- **[CL-070 / UT]** Generative MIDI (variations on a theme) — input phrase, get N variations.
- **[CL-071 / UT]** Style transfer for patterns — apply a style preset to existing MIDI.
- **[CL-072 / UT]** Tutorial overlay system — step-by-step in-app guide for every page.
- **[CL-073 / UT]** Interactive lessons — music theory, mixing, mastering.
- **[CL-074 / UT]** Hover-to-hear tooltips — hover an effect / preset / sample for instant audio preview.
- **[CL-075 / UT]** Interactive scale picker — key + mode → relevant chord families displayed.
- **[CL-076 / UT]** Chord builder UI — drag note shapes onto a strip to build progressions.
- **[CL-077 / UT]** Smart quantize (preserve groove) — quantize while keeping micro-timing feel.
- **[CL-078 / UT]** Groove templates — extract groove from MIDI/audio, apply to other patterns.
- **[CL-079 / UT]** Genre starter packs — load a genre starter (drum + bass + chord pads) as a template.
- **[CL-080 / UT]** Practice mode — loop a section with metronome + record over takes.
- **[CL-081 / UT]** Recording session presets — vocal / guitar / podcast templates.
- **[CL-082 / UT]** Sound design tutorials — pre-loaded guided synth patches teaching subtractive/FM/wavetable.
- **[CL-083 / UT]** Beat detection on imported audio — auto-detect tempo + downbeat, snap to grid.
- **[CL-084 / UT]** Vocal isolation — extract vocal stem from any audio (uses CL-047 stem-from-audio underneath).
- **[CL-085 / UT]** Visual EQ matching — drag reference track, EQ matches its spectral profile.
- **[CL-086 / UT]** Built-in song idea library — hundreds of starting templates, browseable + filterable.

## Workflow Polish

Multi-window, project snapshots / version control, cloud sync, sharing,
comments, macros, performance pad, drum-pad mode, track grouping,
section markers, action recorder.

### Fire-Hose QoL
- **[CL-087 / WP]** Multi-window UI — detach mixer / piano roll / browser to separate windows on second monitor.
- **[CL-088 / WP]** Theme variants — light, dark, high-contrast, sepia.
- **[CL-089 / WP]** Custom theme builder — user-defined color palettes for entire UI.
- **[CL-090 / WP]** DPI scaling — 1x / 1.25x / 1.5x / 2x for high-DPI displays.
- **[CL-091 / WP]** Touch-friendly mode — larger hit areas, tablet-friendly UI (lays groundwork for LDT-425 tablet DJ).
- **[CL-092 / WP]** Project version control / snapshots — manual + auto snapshots, browseable history.
- **[CL-093 / WP]** Cloud project sync — Dropbox / OneDrive / iCloud integration.
- **[CL-094 / WP]** Project sharing — generate shareable read-only link.
- **[CL-095 / WP]** Collaborative editing — real-time co-edit (large scope; phase 2 if pursued).
- **[CL-096 / WP]** Comment threads — leave inline comments on patterns / blocks for self / collaborators.
- **[CL-097 / WP]** Macro controls — single knob → multiple destinations across the project.
- **[CL-098 / WP]** Performance pad — 8×8 launchpad-style trigger surface with computer keyboard support.
- **[CL-099 / WP]** Drum pad mode — keyboard layout for drum trigger (S/D/F/G... = kick/snare/hat/clap...).
- **[CL-100 / WP]** Live record loop / overdub — extend recording with cumulative overdub mode.
- **[CL-101 / WP]** Track grouping (folder tracks) — organize Layers / Bass into collapsible folders.
- **[CL-102 / WP]** Track template — save track config (engine + effects + routing) for quick recall.
- **[CL-103 / WP]** Sample preview at correct pitch/key — auto-pitch a one-shot to project key on hover.
- **[CL-104 / WP]** Drag-to-tempo-match — drop sample, auto-fit project tempo (uses PhaseVocoder).
- **[CL-105 / WP]** Music theory overlay — show Roman numeral analysis on chord progression in real time.
- **[CL-106 / WP]** Section markers (verse/chorus/bridge) — color-coded named markers in Builder.
- **[CL-107 / WP]** Bookmark positions in Builder — jump-to keyboard shortcut.
- **[CL-108 / WP]** Action recorder — record a sequence of UI actions, replay later (macro for repetitive workflows).

### Routing (walked)
- **[FSW-330 / WP]** Pre-fader sends in routing — currently all sends are treated as post-fader by audio path; pre-fader wiring deferred. _(walked from V1 backlog 2026-05-08.)_

## Other / Platform / Deferred

VST/AU plugin hosting (instrument + effect), surround / Atmos, tablet
DJ app, multi-touch, hardware MIDI output, MIDI export.

### VST / AU plugin hosting (post-v1.0 review)
- **[LDT-219 / OT]** VST/AU hosting — post-v1.0 review.
- **[LDT-423 / OT]** VST/AU plugin hosting — major separate effort.

### VST instrument builds (separate target)
- **[LDT-424 / OT]** VST instrument builds — investigate making engines into standalone VSTs.

### Tablet DJ + multi-touch
- **[LDT-425 / OT]** Tablet DJ app — party DJing app for tablets.
- **[LDT-426 / OT]** Multi-touch support — feeds into tablet DJ.

### Surround / stem extraction
- **[LDT-427 / AQ]** Surround sound (5.1/7.1) — niche.
- **[LDT-428 / AQ]** Stem extraction (ML-based) — requires ML model.

### MIDI export / hardware MIDI
- **[LDT-429 / WP]** MIDI Export — standard MIDI file from piano roll.
- **[LDT-430 / OT]** Hardware MIDI output routing — send MIDI to external synths.

## Meta

_No future-state items in the Meta bucket — Sessions / Decisions /
Standing Parallel Work are bookkeeping, not roadmap items._

---

# Section 2 — Considered & Dropped

Items reviewed during QA-Inventory walkthrough (or post-walk Phase-4 reclassify) and confirmed dropped, with reason. Ensures we don't re-litigate. Grouped by drop reason (not by domain) — if you ever change your mind on a drop, that context lives here.

## Harmless UI items dropped during QA-A STYLE / Phase 5F-3 (12)

These were proposed in the source docs but explicitly dropped during the Harmless layout / SLA audit work. Confirmed drop in QA-Inventory walk.

- **[BLU-629 / OT]** Timbre Fade horizontal slider (#4) — right-click-modulate covers the use case. PRESET-SAFE.
- **[BLU-630 / OT]** Routing Matrix 6 LED toggles (#6) — S1 work already covered. PRESET-SAFE.
- **[BLU-631 / OT]** Blur 2 curve-routing toggles (#10) — right-click-modulate covers. PRESET-SAFE.
- **[BLU-632 / OT]** Prism "from vol" toggle (#15) — right-click-modulate covers. PRESET-SAFE.
- **[BLU-633 / OT]** Filter width knob x2 (#32) — redundant with RES. PRESET-SAFE.
- **[BLU-634 / OT]** Filter 6 tiny toggles per filter (#37) — Harmor-specific, not applicable. PRESET-SAFE.
- **[BLU-635 / OT]** Phaser 4 toggles oct/Hz/harm/kb.t (#46) — same family not applicable. PRESET-SAFE.
- **[BLU-636 / OT]** Global porta knob (#51) — redundant with glide_time. PRESET-SAFE.
- **[BLU-637 / OT]** Global link/chain toggle (#53) — implicit via timbre_blend. PRESET-SAFE.
- **[BLU-638 / OT]** LFO pre/post fx labels (#56) — no pre/post distinction in this DSP. PRESET-SAFE.
- **[BLU-639 / OT]** Info Bar text area (#59) — removed as confusing per user direction. PRESET-SAFE.
- **[BLU-407 / OT]** T3.7 Filter KB-track as cents/oct slider — DROPPED 2026-04-21 as synth-nerd feature. (Note: walked entry reclassified to Future State Section 1 → Players → §P3 BaySick family; both routings preserved for traceability.)

## Drum engine option dropped (1)

- **[BLU-423 / OT]** Harmless as 3rd engine option for drums — not drum-tuned. Confirmed drop in walk.

## Spec items dropped as ambiguous / N/A (3)

- **[FSW-038 / OT]** F2 Edit Properties dialog (BaySickPitch) — dropped in walk.
- **[LDT-390 / OT]** 5F-5 ambiguous Event Editor items (LED ON toggle / RANGE box / Link icon / spline tool icon / target-link icons / mode-toggle radio-switch) — original spec ambiguous; dropped during 5F-5 implementation, confirmed in walk.
- **[LDT-392 / OT]** 5F-6 ambiguous Piano Roll items (Speaker icon transport / Window controls / Draggable lane divider) — ambiguous / structural / N/A; confirmed drop.

## Phase-4 verification reclassify-to-Drop (1)

- **[FSW-244 / OT]** Per-input-channel diagnostic ("Show Input Diagnostics dialog") — Phase-4 source verification couldn't find the dialog string in source (`MixerPage::showInputChannelPicker` has no diagnostic submenu). Original spec described a feature that was never built. Walked: dropped because spec intent is unclear; building from a fuzzy spec creates the wrong thing.

## QA-Inventory walk reclassify-to-Drop (1)

- **[BLU-605 / OT]** voxRoll/instRoll dead-code cleanup — originally proposed as Phase 6 cleanup. Per walk: piano-roll infrastructure is NEEDED for Inst (BaySickGuitars / BaySickBasses) and reserved for future SFZ vocal player. Not dead code; should not be removed.

---

**End of Future State.md.** Append-only — new items welcome. Original IDs preserved for traceability.
