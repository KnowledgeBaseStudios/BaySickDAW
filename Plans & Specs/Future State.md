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

Three sections:

1. **Source-Doc Tier 3 / Future-State (167 items)** — items already in
   the source docs as Tier 3 / post-v1.0 / deferred. Original IDs preserved
   (`BLU-*`, `FSW-*`, `LDT-*`). Grouped by source-doc section.
2. **Claude Fire-Hose Additions (108 items)** — proposals generated
   2026-05-08 during QA-Inventory Phase 5. Prefixed `CL-*`. Grouped by
   value category.
3. **Considered & Dropped** — populated at QA-Inventory close from
   walkthrough bucket B decisions. One-liner per item with reason.

Each entry is a one-liner: **`[ID / TAG] Title — short description.`**

Tags per category:

- **AQ** — Audio Quality (highest value)
- **PE** — Performance / Efficiency
- **UT** — User Tools / Learning Experience
- **WP** — Workflow Polish
- **OT** — Other / Platform / Cross-cutting

---

# Section 1 — Source-Doc Tier 3 / Future-State Items

Total: 167 items, preserved in their original source-doc grouping for traceability back to the parent module / decision.

## Effect Modules (70)

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

## Future Effect Modules (29)

### Dynamics
- **[BLU-274 / AQ]** Gate / Downward Expander — inverted compression envelope. PRESET-SAFE.
- **[BLU-275 / AQ]** Upward Compressor — OTT-style sound design. PRESET-SAFE.
- **[BLU-276 / AQ]** Multi-band Compressor — `juce::dsp::LinkwitzRileyFilter` crossovers. PRESET-SAFE.
- **[BLU-277 / AQ]** De-Esser — frequency-conscious compressor sidechained. PRESET-SAFE.
- **[BLU-278 / AQ]** Auto-Gain / Vocal Rider — `BallisticsFilter` for slow leveling. PRESET-SAFE.
- **[BLU-279 / AQ]** Maximizer — mastering-grade limiter variation. PRESET-SAFE.

### Harmonics
- **[BLU-280 / AQ]** Bitcrusher — bit-depth + sample-and-hold quantization. PRESET-SAFE.
- **[BLU-281 / AQ]** Wavefolder — folds waveform back, metallic synth tones. PRESET-SAFE.
- **[BLU-282 / AQ]** Exciter / Harmonic Enhancer — HPF + distortion blend. PRESET-SAFE.
- **[BLU-283 / AQ]** Ring Modulator — multiplies signal by carrier oscillator. PRESET-SAFE.
- **[BLU-284 / AQ]** Subharmonic Generator (Octaver) — pitch tracker + sine/square octave below. PRESET-SAFE.
- **[BLU-285 / AQ]** Comb Filter — short delay + heavy mix. PRESET-SAFE.

### Time-Based
- **[BLU-286 / AQ]** Convolution (IR Loader) — `juce::dsp::Convolution` + WAV IRs. PRESET-SAFE.
- **[BLU-287 / AQ]** Pitch Shifter — reuses existing `PhaseVocoder`. PRESET-SAFE.
- **[BLU-288 / AQ]** Granular Processor — 10–100 ms grains. PRESET-SAFE.
- **[BLU-289 / AQ]** Stereo Widener (Haas Effect) — tiny L/R delay. PRESET-SAFE.
- **[BLU-290 / AQ]** Reverse Buffer — circular buffer with reverse read. PRESET-SAFE.

### Modulation
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

## Player Engines (43)

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

## System Pages (3)
- **[BLU-478 / WP]** TB-T1 LAT readout in ms — restore millisecond conversion. PRESET-SAFE.
- **[BLU-480 / WP]** FX-1 Rack UI refactor — sidebar picker + detail pane. PRESET-SAFE.
- **[BLU-487 / WP]** Sample Browser — real file-system tree, drag from disk. Post-Phase H/I.

## Cross-cutting (8)
- **[BLU-489 / WP]** T3-LRX5Vignette — global lens vignette gated on GL renderer. PRESET-SAFE.
- **[BLU-490 / WP]** T3-LRX8GLSL GLSL shader realism pass — knob skirts, panel textures, spec highlights. PRESET-SAFE.
- **[BLU-494 / AQ]** Orfanidis analytical anti-cramping — replaces §12f oversampling. PRESET-SAFE.
- **[BLU-499 / WP]** Effect-panel preset loader UI — 3 approach options. PRESET-SAFE.
- **[BLU-513 / AQ]** Ring Modulator effect (new module) — Round 4 §P3-CORE Cross-Apply.
- **[BLU-514 / AQ]** Transient Injector effect (new module) — distinct from Transient Shaper.
- **[BLU-515 / AQ]** Gate / Rhythmic Tremolo effect — multi-burst engine drives.
- **[BLU-516 / AQ]** Analog Drift / Tape Pitch Wander effect — standalone subtle pitch-drift.

## Other / Deferred from source docs (12)
- **[FSW-334 / WP]** LRX-8 GLSL Shader pass — post-v1.0 GPU realism path.
- **[LDT-139 / WP]** LRX-8 GLSL Shader (master checklist L&F).
- **[LDT-219 / OT]** VST/AU hosting — post-v1.0 review.
- **[LDT-231 / WP]** GLSL shaders — DEFERRED post-v1.0.
- **[LDT-286 / WP]** GLSL Shader (Optional GPU Path) — OpenGL shader.
- **[LDT-422 / WP]** GLSL/OpenGL shader rendering (LRX-8) — complexity + GPU compatibility.
- **[LDT-423 / OT]** VST/AU plugin hosting — major separate effort.
- **[LDT-424 / OT]** VST instrument builds — investigate making engines into standalone VSTs.
- **[LDT-425 / OT]** Tablet DJ app — party DJing app for tablets.
- **[LDT-426 / OT]** Multi-touch support — feeds into tablet DJ.
- **[LDT-427 / AQ]** Surround sound (5.1/7.1) — niche.
- **[LDT-428 / AQ]** Stem extraction (ML-based) — requires ML model.
- **[LDT-429 / WP]** MIDI Export — standard MIDI file from piano roll.
- **[LDT-430 / OT]** Hardware MIDI output routing — send MIDI to external synths.

---

# Section 2 — Claude Fire-Hose Additions

Generated 2026-05-08 during QA-Inventory Phase 5. Net-new beyond what's already in the source docs Tier 3. Grouped by value category. Comprehensive, intentionally unfiltered per user direction ("every plausible idea, not bounded — between 'cool ad' and 'I WISH WE HAD THIS NOW BUT I SHOULD FINALLY RELEASE'").

## Audio Quality (47)

### New synth engines (peer to Harmless / BaySickSynth / BaySickBass)
- **[CL-001 / AQ]** Wavetable synthesizer (`BaySickWavetable`) — full standalone engine with morph, harmonics editor, multi-frame wavetables, FFT-based distort/transform, drag-WAV-to-wavetable import.
- **[CL-002 / AQ]** FM synthesizer (`BaySickFM`) — 4-op or 6-op DX-style with algorithm presets, ratio + fixed Hz operator modes, integrated envelopes per op.
- **[CL-003 / AQ]** Subtractive analog synth (`BaySickAnalog`) — Minimoog-style 3-osc + LFO + filter envelope + amp envelope with character-specific drift modeling.
- **[CL-004 / AQ]** Phase-distortion synth (`BaySickCZ`) — Casio CZ-style 8-waveform PD with envelope-modulated phase shaping.
- **[CL-005 / AQ]** Modal/physical-modeling percussion synth (`BaySickModal`) — bell, marimba, drum membrane, gong via modal resonator banks.
- **[CL-006 / AQ]** Karplus-Strong plucked-string synth (`BaySickStrings`) — physical-modeled string with pluck position, damping, body resonance.
- **[CL-007 / AQ]** Vocoder engine (`BaySickVocoder`) — carrier/modulator vocoder with 16-32 bands, formant preservation, hold mode.
- **[CL-008 / AQ]** Talkbox emulation — formant-shaping the carrier through tracked-formant filters.

### Sample / sample manipulation
- **[CL-009 / AQ]** Pitched-sample mode for VibePlayer — auto-stretch a one-shot into a sustained instrument across the keyboard.
- **[CL-010 / AQ]** Reslicer — auto-slice loops into per-transient slices, drum-machine playback.
- **[CL-011 / AQ]** Granular FX module (rack effect, distinct from BLU-288 sampler) — apply granular to any audio bus.
- **[CL-012 / AQ]** Spectral freezer — capture spectral state, hold indefinitely, blend with current.
- **[CL-013 / AQ]** Resynthesis (drag audio → harmonics) — distinct from BLU-332; standalone tool that produces a spectrum file usable by multiple engines.
- **[CL-014 / AQ]** Pitch-correction for instruments — `BaySickPitch` extended to monophonic guitar/bass/horn input.

### New effect modules / DSP coverage
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

### Mixing / mastering / metering
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

## Performance / Efficiency (15)
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

## User Tools / Learning Experience (24)
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

## Workflow Polish (22)
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

---

# Section 3 — Considered & Dropped

Items reviewed during QA-Inventory walkthrough (bucket B) and confirmed dropped, with reason. Ensures we don't re-litigate.

(populated at QA-Inventory close from Phase 6 routing of bucket B decisions; placeholder for now)

---

**End of Future State.md.** Append-only — new items welcome. Original IDs preserved for traceability.
