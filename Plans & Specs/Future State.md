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

1. **Future Items by Domain (459 items)** — every post-V1 candidate
   organised into the 10 canonical domain buckets defined in Main Plan
   §0 ("Document Formatting Conventions").  Source-doc Tier 3 items
   (`BLU-*`, `FSW-*`, `LDT-*`), Claude fire-hose additions (`CL-*`),
   and walked-from-V1 items all live together under the bucket they
   touch.  Re-organised 2026-05-08 from the original four-section
   layout.
2. **Considered & Dropped (19 items)** — items reviewed during
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

Total: 459 items, organised into the 10 canonical domain buckets.
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

### Vocal pitch / hard-tune (Sweep 1)
- **[CL-109 / AQ]** Real-time scale-quantize hard-tune with custom-savable scales (43+ western/eastern + user customs); option to map illegal notes to nearest higher/lower scale tone. _(Inspired by: Waves Tune Real-Time https://www.waves.com/plugins/waves-tune-real-time)_ MEDIUM
- **[CL-110 / AQ]** Per-note "wrong note" exclusion list. _(Inspired by: Waves Tune Real-Time)_ MEDIUM
- **[CL-111 / AQ]** Auto-Key-style automatic key + scale + tempo detector — drag any audio file, "Send to BaySickPitch" button writes detected key into corrector. _(Inspired by: Antares Auto-Key 2)_ MEDIUM
- **[CL-112 / AQ]** Throat / vocal-tract physical-model tab — 5-point graphical throat shaper (glottis -> throat -> mouth -> lips); neutralizes source vocal tract first then applies modeled tract. _(Inspired by: Antares Throat Evo https://www.antarestech.com/product/throat/)_ HIGH
- **[CL-113 / AQ]** Glottal waveform + variable breath-noise injection — pairs with CL-112 for rasp/smooth/whisper. _(Inspired by: Antares Throat Evo)_ HIGH
- **[CL-114 / AQ]** Octave pitch transpose with formant compensation — single transpose knob shifts ±octave with formant preserved. _(Inspired by: Antares Throat Evo + Auto-Tune Pro 11)_ HIGH

### Vocal harmony generation (Sweep 1)
- **[CL-115 / AQ]** 4-voice formant-corrected Harmony Player — per-voice formant + pan + level + customizable retune speed; harmonies generated in real time from lead vocal input. _(Inspired by: Auto-Tune Pro 11 Harmony Player; SoS + MusicRadar reviews WebFetched)_ HIGH
- **[CL-116 / AQ]** Choir / vocal multiplier — turn one vocal into 4/8/16/32 distinct unison voices with per-voice random pitch/timing/vibrato + stereo spread. _(Inspired by: Antares CHOIR Evo)_ MEDIUM
- **[CL-117 / AQ]** MIDI-driven harmony modes — Fixed Intervals / Scale Intervals / Chord-Degrees / Chord-Name / Chord-by-MIDI / MIDI-Omni / 4-channel-MIDI. _(Inspired by: Antares Harmony Engine https://www.antarestech.com/product/harmony-engine/)_ HIGH
- **[CL-118 / AQ]** Backing-singer style packs — 8+ presets + import-acapella to define custom backing-singer persona. _(Inspired by: iZotope Nectar 4 Backer; docs.izotope.com WebFetched)_ HIGH
- **[CL-119 / AQ]** Per-voice Throat-Modeling on each Harmony Player voice — when CL-112 + CL-115 both ship, harmony voices route through their own throat models. _(Inspired by: Antares Harmony Engine Throat integration)_ HIGH

### Vocal alignment / polish (Sweep 1)
- **[CL-120 / WP]** Multi-track group alignment in BaySickAlign — align N dub tracks to one guide in single pass with per-track tightness override. _(Inspired by: VocAlign Pro Process Groups https://www.synchroarts.com/products/vocalign-pro)_ HIGH
- **[CL-121 / WP]** Sync points + protected areas in BaySickAlign — user-defined target points + protected regions. _(Inspired by: VocAlign Pro)_ HIGH
- **[CL-122 / AQ]** SmartPitch in BaySickAlign — opt-in pitch-snap-to-guide layer that retunes only when dub is meaningfully off-pitch from guide. _(Inspired by: VocAlign Pro SmartPitch)_ HIGH
- **[CL-123 / AQ]** Vocal doubler (BaySickVocal stage) with controllable pitch variation + timing offset + formant shift — natural-sounding double generation. _(Inspired by: Revoice Pro 5 Vocal Doubler https://www.synchroarts.com/products/revoice-pro-5)_ HIGH
- **[CL-124 / AQ]** Phase-aligned multi-mic vocal stacks — automatic spectral-phase optimization via all-pass filter rotation across multiple takes/mics. _(Inspired by: Sound Radix Auto-Align 2 https://www.soundradix.com/products/auto-align/)_ HIGH
- **[CL-125 / AQ]** Sub-sample timing alignment — sub-sample-resolution time-shift between source + dub. _(Inspired by: Sound Radix Auto-Align 2)_ HIGH

### Vocal restoration tools (Sweep 1)
- **[CL-126 / AQ]** Adaptive de-noise mode (extends CL-027) — track changing noise floor in real time vs fixed-train. _(Inspired by: iZotope RX 11 Voice De-noise Adaptive)_ MEDIUM
- **[CL-127 / AQ]** Combined vocal repair-assistant umbrella over CL-027/028/029/030 — single panel + auto-classify + light/medium/aggressive intensity. _(Inspired by: iZotope RX 11 Repair Assistant)_ MEDIUM
- **[CL-128 / AQ]** Dialogue Isolate-style vocal stem extraction stage in BaySickVocal — neural-network separation as a pre-stage. Distinct from CL-084. _(Inspired by: iZotope RX 11 Dialogue Isolate)_ MEDIUM
- **[CL-129 / AQ]** Vocal Unmask spectral-sidechain ducking — sidechain instrumental bus, auto-find masked frequencies, dynamic-EQ on competing track with vocal-aware threshold. Distinct from BLU-278 vocal-rider gain leveling. _(Inspired by: iZotope Nectar 4 Vocal Unmask)_ HIGH

### Specialized vocal dynamics (Sweep 1)
- **[CL-130 / AQ]** Variable-frequency de-esser stage — sweepable sidechain HPF + ratio/threshold/attack/release. _(Inspired by: Antares SYBIL Evo)_ MEDIUM
- **[CL-131 / AQ]** Sibilant-balance per-note in BaySickPitch — drag a single note's sibilant volume independent of pitched component. _(Inspired by: Melodyne 5 Sibilant Balance; SoS + Celemony Help Center WebFetched)_ HIGH
- **[CL-132 / AQ]** Per-note fade in/out tool in BaySickPitch — works inside chords. _(Inspired by: Melodyne 5 Fade Tool)_ HIGH
- **[CL-133 / AQ]** Leveling Macro per-note — two-knob "make quiet notes louder + make loud notes quieter" amplitude leveler ignoring breaths/low-level noise. _(Inspired by: Melodyne 5 Leveling Macro)_ HIGH
- **[CL-134 / AQ]** Weighted-pitch-centre algorithm — emphasizes perceptually significant portions of sustained notes vs averaging entire pitch curve. _(Inspired by: Melodyne 5 weighted pitch centre)_ HIGH
- **[CL-135 / AQ]** Per-note pitch + sibilance separation — split each note into pitched and unpitched components, edit each independently. _(Inspired by: Melodyne 5 Melodic Algorithm with Sibilant Detection)_ HIGH
- **[CL-136 / AQ]** Chord recognition + chord track in BaySickPitch — analyze project audio harmonic content, auto-fill chord track, snap dragged notes to chord tones. _(Inspired by: Melodyne 5 Chord Recognition + Chord Track + Chord Snap)_ HIGH
- **[CL-137 / AQ]** Percussive-pitched hybrid algorithm in BaySickPitch — for material combining percussion + pitched elements; preserves transient integrity. _(Inspired by: Melodyne 5 Percussive Pitched Algorithm)_ HIGH

### Vocal effects polish (Sweep 1)
- **[CL-140 / AQ]** Auto-Level / leveling-as-alternative-to-compression module — dynamic-range learning, noise tolerance, optional sidechain input. Distinct from BLU-278. _(Inspired by: iZotope Nectar 4 Auto-Level)_ HIGH
- **[CL-141 / AQ]** Articulator / formant-extraction stage — extract formant + amplitude envelope from one vocal, apply to another audio source. Built-in noise-generator for synth-less use. _(Inspired by: Antares Articulator Evo)_ MEDIUM

### Saturation engine extensions (Sweep 2)
- **[CL-144 / AQ]** Punish/Overdrive momentary +18 dB pre-shaper boost button on SaturationDSP. _(Inspired by: Soundtoys Decapitator https://www.soundtoys.com/product/decapitator/, Thermionic Culture Vulture)_ HIGH
- **[CL-145 / AQ]** Drive + Punish two-knob hierarchy — Drive = continuous gain into shaper, Punish = momentary push button (pairs with CL-144 but exposes the two-knob UX explicitly). _(Inspired by: Decapitator manual)_ HIGH
- **[CL-146 / AQ]** Auto-Gain output compensation toggle on every saturation Type — locks output level so A/B at any drive setting stays loudness-matched. _(Inspired by: Decapitator)_ HIGH
- **[CL-147 / AQ]** Steep filter slope toggle on saturation HPF/LPF — switches between 6 dB/oct and 30 dB/oct. _(Inspired by: Decapitator)_ HIGH
- **[CL-148 / AQ]** Resonant Thump bump at low-cut shoulder — pushes resonant peak right where Low Cut filter starts to roll off; one-button voicing. _(Inspired by: Decapitator)_ HIGH
- **[CL-149 / AQ]** Bias control on SaturationDSP Tube path — varies cathode-current bias point; backward rotation starves cathode for gating artifacts. _(Inspired by: Thermionic Culture Vulture)_ HIGH
- **[CL-150 / AQ]** Triode + Pentode parallel-stage saturation mode — 4th `Type::Parallel` running even-harmonic + odd-harmonic shapers in parallel with independent Pentode + Triode + Saturation level pots. _(Inspired by: Black Box HG-2 https://blackboxanalog.com/wp-content/uploads/2015/09/HG-2-manual-REV-3.0.pdf)_ HIGH
- **[CL-151 / AQ]** Variable Air control on SaturationDSP — high-shelf "silvery sparkle" boost above 10 kHz. _(Inspired by: Black Box HG-2)_ HIGH
- **[CL-152 / AQ]** Subtle / Gentle / Warm tier triplet for Tube/Console/Tape Types — uniform 3-tier intensity ladder. _(Inspired by: FabFilter Saturn 2 https://www.fabfilter.com/products/saturn-2-multiband-distortion-saturation-plug-in)_ HIGH
- **[CL-153 / AQ]** Foldback / Breakdown FX modes — `Type::Foldback` (waveform-folding metallic artifacts) + `Type::Breakdown` (signal-mutating buffer-fragment echoes). Distinct from BLU-281 standalone Wavefolder. _(Inspired by: FabFilter Saturn 2)_ HIGH
- **[CL-154 / AQ]** Unrestricted Feedback toggle on saturation feedback path — per-band feedback can self-oscillate endlessly. _(Inspired by: FabFilter Saturn 2 Help)_ HIGH

### Multiband saturation extensions (folds under BLU-172 if approved)
- **[CL-155 / AQ]** Adjustable crossover slope (6/12/24/48 dB/oct) on multiband saturation (fold under BLU-172). _(Inspired by: FabFilter Saturn 2)_ HIGH
- **[CL-156 / AQ]** Linear-phase crossover mode — `LinPhase` toggle swaps IIR for FIR splitter; reuse EQ8 linear-phase plumbing (BLU-268..271) (fold under BLU-172). _(Inspired by: FabFilter Saturn 2)_ HIGH
- **[CL-157 / AQ]** Per-band drive/mix/feedback/dynamics/tone/level controls on multiband sat (fold under BLU-172). _(Inspired by: FabFilter Saturn 2)_ HIGH
- **[CL-158 / AQ]** Per-band Tone control (tilt-style EQ post-shaper, separate from input-side filtering) (fold under BLU-172). _(Inspired by: FabFilter Saturn 2)_ HIGH
- **[CL-159 / AQ]** True-split crossover (Mid/Side phase-coherent multiband) — guarantees Mid + Side stay phase-coherent across band split (fold under BLU-172). _(Inspired by: Brainworx bx_saturator V2 https://www.plugin-alliance.com/products/bx_saturator-v2)_ HIGH
- **[CL-160 / AQ]** Drag-and-drop modulation matrix on saturation effects — 8-12 mod slots with sources (LFO/env follower/XY/MIDI CC) draggable to any param (fold under BLU-172). _(Inspired by: FabFilter Saturn 2 50-slot matrix)_ HIGH
- **[CL-161 / AQ]** 16-step XLFO with per-step value + curve type — sequencer-style modulator beyond rate+depth (fold under BLU-172). _(Inspired by: FabFilter Saturn 2 XLFO)_ HIGH
- **[CL-162 / AQ]** Per-band MIDI Learn — interactive MIDI Learn for multiband saturation parameters (fold under BLU-172). _(Inspired by: FabFilter Saturn 2)_ HIGH

### Custom / experimental saturation (Sweep 2)
- **[CL-163 / AQ]** Custom waveshape draw editor — `Type::Custom` exposes draggable transfer-curve editor. _(Inspired by: iZotope Trash 2/5)_ MEDIUM
- **[CL-164 / AQ]** Vowel filter pre-stage — formant-bandpass front-end with A/E/I/O/U vowel shapes. _(Inspired by: iZotope Trash 2/5)_ MEDIUM
- **[CL-165 / AQ]** Distortion algorithm library expansion (60+ algos, Trash-style) — separate `Algorithm` selector inside each `Type` with 8-12 sub-algorithms (Tube > 12AX7/12AT7/6L6/EL34/...; Console > SSL/Neve/API/Trident/RCA; Tape > Studer/Ampex/Otari/cassette). 60+ total. _(Inspired by: iZotope Trash + Decapitator)_ HIGH
- **[CL-166 / AQ]** Parallel SAT branch frequency selector — when CL-150 lands, parallel branch gets 3-position freq selector (Broad/Lo/Hi). _(Inspired by: HG-2)_ HIGH
- **[CL-167 / AQ]** Per-style HPF/LPF voicing on saturation — Culture-Vulture-style internal voicing-filter, not user-cutoff control. _(Inspired by: Thermionic Culture Vulture)_ MEDIUM
- **[CL-168 / AQ]** Crush boost knob — separate input-boost distinct from Drive (Drive = saturation level; Crush = momentary input push). Continuous variant of CL-144. _(Inspired by: Kazrog True Iron https://kazrog.com/products/true-iron)_ HIGH
- **[CL-169 / AQ]** 6-transformer-model selector on Console-Type saturation — UTC 108 X / Malotki E4M-4001B / Western Electric 111C / Haufe V178 / Marinair LO1166/A / UTC O-12. _(Inspired by: Kazrog True Iron)_ HIGH

### Tape-specific extensions (Sweep 2)
- **[CL-170 / AQ]** Variable Wow & Flutter depth on TapeDSP — separate %-depth knob; 25%/50%/100% emulate pristine/average/poorly-maintained machines. _(Inspired by: Slate Digital VTM https://slatedigital.com/virtual-tape-machines/)_ HIGH
- **[CL-171 / AQ]** HF Bias control on TapeDSP — adjustable HF bias-oscillator level distinct from FSW-184 record-bias level. _(Inspired by: UAD Studer A800 manual)_ HIGH
- **[CL-172 / AQ]** Tape calibration level switch — Cal switch with +3/+6/+7.5/+9 dBu reference levels. _(Inspired by: UAD Studer A800 + Ampex ATR-102)_ HIGH
- **[CL-173 / AQ]** Sync / Repro / Input multi-path tape signal chain — 3-position Path selector swaps EQ + bias + delay characteristics (Input=source, Sync=monitor-from-record-head, Repro=monitor-from-playback-head). _(Inspired by: UAD Studer A800)_ HIGH
- **[CL-174 / AQ]** Auto-Cal one-button manufacturer-recommended calibration — snaps secondary tape controls to factory-recommended values for active Tape Formula + Speed + Emphasis + Head Type. _(Inspired by: UAD Studer A800)_ HIGH
- **[CL-175 / AQ]** Bass alignment adjustment on TapeDSP — low-shelf alignment knob targeting bass-region head bump from real-world tape machines. _(Inspired by: Slate Digital VTM)_ HIGH
- **[CL-176 / AQ]** Soft / Hard bias-mode dual-character toggle on TapeDSP — two-mode flavor switch (smoother vs grittier). _(Inspired by: Tone Empire TM700 V3)_ HIGH
- **[CL-177 / AQ]** Old / New tape-type character split — modes that change tonal balance + harmonic distribution beyond tape formula selector. _(Inspired by: Tone Empire TM700 V3)_ HIGH
- **[CL-178 / AQ]** Tape Hiss + Dropout Rate emulation toggle — separates two distinct controls (steady-state hiss vs intermittent dropouts). Distinct from BLU-198 noise floor. _(Inspired by: Tone Empire TM700 V3)_ HIGH
- **[CL-179 / AQ]** Two-band parametric EQ inline on TapeDSP — fine-tune post-tape character without leaving the module. _(Inspired by: Tone Empire TM700 V3)_ HIGH

### NCM / neural-network engines (V2+ strategic)
- **[CL-180 / AQ]** Neural-network / NCM saturation engine — opt-in `Engine::NCM` flag swaps analytical Tube/Console/Tape model for pre-trained NN model. Bundle 1-3 NCM models per Type as factory presets; allow user `.nam` / `.aida-x` files. Strategic: V2+ architecture decision. _(Inspired by: Tone Empire TM700 V3 + Neural Amp Modeler)_ HIGH _*Research Required*_

### Cab simulator extensions
- **[CL-181 / AQ]** Multi-mic cab simulator with positionable mics — when BLU-129 lands, add 2-D draggable mic-position interface: speaker pick + mic pick + drag across top-down cab + optional second mic with phase + pan + level. _(Inspired by: Neural DSP Tone King Imperial MKII)_ MEDIUM

### Bitcrusher extensions (folds under BLU-280)
- **[CL-182 / AQ]** Mid-Riser vs Mid-Tread quantization mode toggle — two decimation algorithms with drastically different dynamics response (fold under BLU-280). _(Inspired by: D16 Decimort 2 https://d16.pl/decimort2)_ HIGH
- **[CL-183 / AQ]** Image filter (post-decimation aliasing-image control) — separate pre-filter and post-filter that controls aliasing artifacts above resampling frequency (fold under BLU-280). _(Inspired by: D16 Decimort 2)_ HIGH
- **[CL-184 / AQ]** Variable Jitter on resample clock — short-period random fluctuations to resampling frequency producing harmonic distortion (fold under BLU-280). _(Inspired by: D16 Decimort 2)_ HIGH

### Clipper extensions (folds under BLU-107)
- **[CL-185 / AQ]** EBU LUFS target loudness display inside Clipper module (fold under BLU-107). _(Inspired by: Kazrog KClip 3 https://kazrog.com/products/kclip-3)_ HIGH
- **[CL-186 / AQ]** Delta "show me what I'm clipping" listening mode — monitors only residual being subtracted from source. Apply to Limiter + Clipper + SaturationDSP (fold under BLU-107). _(Inspired by: Kazrog KClip 3)_ HIGH
- **[CL-187 / AQ]** 8-mode clipper character library — Smooth / Crisp / Tube / Tape / Germanium / Silicon / Broken Speaker / Guitar Amp (fold under BLU-107). _(Inspired by: Kazrog KClip 3)_ HIGH
- **[CL-188 / AQ]** Variable Soft Clipping — single knob morphs between hard and soft clipping (fold under BLU-107). _(Inspired by: Kazrog KClip 3)_ HIGH

### FATSO-specific saturation
- **[CL-189 / AQ]** Tranny harmonic-generator soft-clipper — dedicated `Type::Tranny` voiced for FATSO low-end character. Distinct from CL-150 parallel mode. _(Inspired by: Empirical Labs FATSO https://www.empiricallabs.com/fatso)_ HIGH
- **[CL-190 / AQ]** Warmth dynamic-LPF mode — self-modulates from signal envelope; one-knob auto-de-essing voicing on top of saturation. _(Inspired by: Empirical Labs FATSO)_ HIGH

### Devastor / Output Stage
- **[CL-191 / AQ]** Output Limiter + Dynamics Flattener pair — auto makeup limiter + one-knob input compressor managing signal loudness before clipping. _(Inspired by: D16 Devastor 2 https://d16.pl/devastor-2)_ HIGH
- **[CL-192 / AQ]** Filter routing topologies — 3-position chain selector: Filter->Saturation, Saturation->Filter, Filter || Saturation parallel. Distinct from BLU-170 pre/de-emphasis EQ pair. _(Inspired by: D16 Devastor 2)_ HIGH

### Saturation cross-cutting effects
- **[CL-193 / AQ]** Mono Maker low-end mono-fold — when CL-021 Console emulation lands, expose `MonoMakerHz` knob (50-300 Hz fold to mono) on bus version. _(Inspired by: Brainworx bx_saturator V2)_ HIGH
- **[CL-198 / AQ]** Tone Empire 2-band EQ stage on TapeDSP only — when TapeDSP has its own tab/page, add a stripped EQ8 view as post-tape stage for fine-tuning. _(Inspired by: Tone Empire TM700 V3)_ HIGH
- **[CL-199 / AQ]** Per-stage isolation Pentode and Triode level knobs — when CL-150 ships, expose dedicated Pentode + Triode + Saturation level pots so each can be soloed/blended. _(Inspired by: HG-2 hardware)_ HIGH

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

### Fire-Hose net-new synth engines (V2+ strategic)
- **[CL-200 / AQ]** Wave-sequencing engine (`BaySickWaveSeq`) — multi-lane sequencer with independent Lanes for Timing / Sample / Pitch / Shape / Gate / Step Sequencer. Per-step random skip with modulatable probability 0-100%; randomize step order per pass. Strategic: V2+. _(Inspired by: Korg wavestate https://www.korg.com/us/products/synthesizers/wavestate/)_ HIGH
- **[CL-201 / AQ]** Vector synthesis engine (`BaySickVector`) — 4-corner mix between four sound sources controlled by 2D joystick + recordable mix envelope; integrates with CL-200 so each corner can host a wave-sequence. Strategic: V2+. _(Inspired by: Korg wavestate)_ HIGH
- **[CL-202 / AQ]** Hybrid multi-engine container (`BaySickFalcon` working name) — single tab with 4+ layered sub-engines per voice; each layer independently chooses synthesis mode (analog/wavetable/FM/granular/sample/modal/pluck); shared filters + amp + global mod matrix. Strategic: V2+ foundational change. _(Inspired by: UVI Falcon https://www.uvi.net/falcon + Logic Alchemy + Pigments)_ HIGH
- **[CL-203 / AQ]** Modular patch-graph engine (`BaySickModular`) — user-patchable visual-cable instrument page; reuses existing `RoutingGraph` Kahn topo-sort; polyphonic-mode flag per module. Strategic: V2+. _(Inspired by: Bitwig Poly Grid https://www.bitwig.com/the-grid/ + Phase Plant + u-he Bazille https://u-he.com/products/bazille/)_ HIGH
- **[CL-204 / AQ]** Hybrid-resonator dual-modal engine (`BaySickResonator`) — two coupled physical-modeled resonators (string/beam/drumhead/membrane/plate/tube/marimba bar/manual) with bidirectional energy-flow coupling; mallet + noise excitation modules. Distinct from CL-005 single resonator-bank. _(Inspired by: AAS Chromaphone 3 https://www.applied-acoustics.com/chromaphone-3/)_ HIGH
- **[CL-205 / AQ]** Spectral oscillator option for `BaySickWavetable` (CL-001 sub-feature) — third oscillator mode resynthesizing samples at harmonic level: warp/stretch/shift/smear/skew/tilt/shimmer over FFT frame; per-harmonic mute mask. Folds into CL-001 scope. _(Inspired by: Vital https://vital.audio/ + Serum 2 https://xferrecords.com/products/serum-2)_ HIGH
- **[CL-206 / AQ]** Phase-distortion + fractal-resonance digital engine (`BaySickPD`) — Casio-CZ-style PD per CL-004 plus fractal-resonance "fractalize" mode that creates sync-like cutting tones; oscillator runs from 0 Hz (doubles as LFO); modular signal-out routing. Folds into CL-004 scope OR splits as separate engine. _(Inspired by: u-he Bazille)_ HIGH

### Cross-cutting Player features
- **[CL-207 / AQ]** Per-engine "Performer" timeline modulator — draw up to 8 bars of modulation curve per slot, store up to 12 patterns per patch, switch via Remote-Octave keyboard control. _(Inspired by: Native Instruments Massive X Performer)_ HIGH
- **[CL-208 / AQ]** MTS-ESP + Scala microtuning support — load `.scl` / `.kbm` / `.tun`; auto-sync to MTS-ESP master plugin; applies across all four BaySick engines. _(Inspired by: ODDSound MTS-ESP https://oddsound.com/mtsespsuite.php + Surge XT)_ HIGH
- **[CL-209 / AQ]** MPE input + per-voice expression routing — accept MPE channel-per-note pitch-bend / pressure / Y-axis (ROLI / Osmose / LinnStrument / Push); per-voice route to any APVTS target. _(Inspired by: Vital + Phase Plant + Ableton Drift)_ HIGH
- **[CL-210 / AQ]** Circuit-level analog imperfections panel (extends CL-003) — per-voice voice-detune trimmer, slow drift, envelope sloppiness, glide-rate / cutoff / pulse-width variance, "Divine"-style accuracy mode toggle. _(Inspired by: u-he Diva https://u-he.com/products/diva/ Trimmers + Divine quality mode)_ HIGH
- **[CL-212 / AQ]** Per-note voicing edit panel (Pianoteq Pro-style) — per-MIDI-note panel with unique volume/detune/attack/decay/overtones/hammer-hardness overrides on top of keyboard-scaling. _(Inspired by: Pianoteq Pro https://www.modartt.com/pianoteq)_ HIGH
- **[CL-213 / AQ]** Composite-Morphing-Technique cross-engine morph — two patches loaded as A/B; morph knob blends harmonic profile of A toward B in FFT frequency domain. _(Inspired by: Spectrasonics Omnisphere CMT https://www.spectrasonics.net/products/omnisphere/overview.php)_ HIGH
- **[CL-214 / AQ]** Sample-builder pipeline — desktop-companion / in-app tool: drag folder of WAVs labeled by note + velocity + RR → auto-detects roots/RR/velocity layers → writes SFZ + binary index loadable by BaySickPlayer. _(Inspired by: Korg Sample Builder)_ HIGH
- **[CL-215 / AQ]** Polyphonic mode toggle on FX rack slots — opt-in per-slot flag runs slot per-voice instead of post-mix. Strategic: major plumbing — V2+. _(Inspired by: Phase Plant polyphonic Snapins)_ HIGH
- **[CL-216 / AQ]** "Orb" XY navigator on every player — radius + angle 2D control with inertia (rolling-ball trail), dice, record-into-pattern, attractor-pendulum. Distinct from existing Mod XYZ pad. _(Inspired by: Spectrasonics Omnisphere ORB)_ HIGH
- **[CL-218 / AQ]** Drag-WAV-onto-target auto-extract envelopes — drop audio file onto envelope slot to auto-extract gain/brightness/transient/pitch envelopes for use as modulation. Pairs with CL-013. _(Inspired by: UVI Falcon)_ HIGH

### Sample manipulation extensions
- **[CL-219 / AQ]** Granular grain controls overhaul for CL-011 + BLU-288 — Output-Portal-grade params: window-shape selector (Tukey/Hann/Gaussian/Rectangular/Triangular), grain density 1-30, grain length 0.5 ms to 1 second + tempo-synced 1/64t..1 bar, scale-locked pitch shift, per-grain randomization (jitter diamonds), tempo-synced grain delay, freeze mode. _(Inspired by: Output Portal https://output.com/products/portal + Serum 2 granular)_ HIGH
- **[CL-220 / AQ]** Pitch-splice + vocode WAV-to-wavetable converters for CL-001 — pitch-splice (slice at zero crossings / pitch period) or vocode (FFT-based) conversion strategies on dropped WAV. _(Inspired by: Vital)_ HIGH
- **[CL-221 / AQ]** Sample slicing with realtime score extraction — one-shot loop → auto-slice → per-slice MIDI score extraction with tails-mode (overlapping slice releases stay audible). Distinct from CL-010 (drum-machine playback). _(Inspired by: Serum 2)_ HIGH

### Voice-stealing & polyphony architecture
- **[CL-275 / AQ]** Voice-stealing tier hierarchy (Off > Release > Sustain > Decay > Attack with age tie-break) — replace oldest-active-only at `Source/VibePlayer/VibePlayerDSP.cpp:944-960`; mirror to BaySickSynth/Bass/Harmless. _(Source: daw-architecture-research-2026-05-08.md §4)_
- **[CL-276 / AQ]** Soft-stop on voiceCap eviction (3-10 ms fade) — replace hard-kill at `Source/VibePlayer/VibePlayerDSP.cpp:960`; force short release override on stolen voices if natural release is multi-second. _(Source: daw-architecture-research-2026-05-08.md §4)_
- **[CL-277 / AQ]** User-visible voice priority mode APVTS enum (Last / Highest / Lowest) — Surge / Cherry vocabulary; per-engine. Tier-3 disambiguation only; engine-stage priority (Tier 1/2) stays internal default. _(Source: daw-architecture-research-2026-05-08.md §4)_
- **[CL-278 / AQ]** Benchwarmer pool (1-2 preallocated extra voices per engine for parallel fade-out) — pairs with CL-276 soft-stop to enable click-free voice eviction. _(Source: daw-architecture-research-2026-05-08.md §4)_

### BaySickRustyDrums (sfizz kit playability)
- **[CL-298 / WP]** Drummer-conventional drum-note remap for BaySickRustyDrums — OPTIONAL per-kit remap that packs the Big Rusty Drums kit's useful drums (scattered across MIDI 24-96 with gaps: mechanical noises + kick-no-damp below 36, GM-style core drums 35-59, sizzle/stir/click articulations 60-96) into a tight contiguous range playable on a 61-key controller. Revives the never-implemented "J-7b drummer-conventional remap" that lives only as a comment in `StandaloneEditor.cpp` (`registerBaySickRustyDrumsPianoRoll`, ~line 5922) — current dispatch passes notes through unremapped (`BaySickRustyDrumsProcessor.cpp:214`). Design fork to resolve at scoping: (a) thin keyboard-input-only remap (small blast radius, but roll display won't match what was played) vs (b) full conventional note-space across roll + keyboard + storage, translating to kit-native only at playback dispatch (consistent, wider surface). DRUMS-ONLY — guitars/basses are pitched + keyswitch-driven and must NOT be remapped. The sfizz engine stays safe (always receives native notes; remap is upstream) — do NOT edit the vendored SFZ keymap (breaks GM-standard kick/snare, misaligns the kit graphic, diverges from kit docs). Every UI/data boundary must translate or desync: keyboard input, roll display, keyboard labels (`getPianoRollKeymap` is native), the kit graphic (native), stored pattern notes, and MIDI export (remapped notes only valid in-app unless translated on export). Needs per-kit layouts (Rusty-full vs -basic vs future kits). Next step when picked up: architecture-research pass on FL Studio FPC + other DAWs' drum-pad-to-note mapping before locking a design. _(Shelved to Future State 2026-07-01 at QA-EffectsReview (composed-foraging-rose) while investigating live-MIDI mapping for the sfizz Aria engines. Playability enhancement, not a bug.)_

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
- **[CL-046 / AQ]** Auto-mixing assist — AI suggests track levels + pan placement based on genre. _*Research Required*_
- **[CL-047 / AQ]** Stem-from-audio (drag song in, get stems) — extends LDT-428 to in-app workflow.

### Metering & Loudness
- **[CL-223 / AQ]** Per-bus integrated/short-term/momentary LUFS history strip — 3-line scrolling history view (last 10+ minutes) with target overlay + true-peak violation flags + locked-to-timeline scrub-resync. Docked into master strip + each bus. _(Inspired by: Nugen VisLM 2 https://nugenaudio.com/vislm/ ReMEM)_ HIGH
- **[CL-224 / AQ]** Multi-instance metering link via lightweight tap-plugin — drop a Relay-style tap on any strip; up to 8 sources route into single floating Insight-style spectrogram/spectrum/sound-field stack. Reuses SpectrumFeed seqlock per tap. _(Inspired by: iZotope Insight 2 + Relay)_ HIGH
- **[CL-225 / AQ]** PSR + PLR meters (AES TD-1004 micro-dynamics) — 3-second sliding peak-to-short-term ratio readout with target line (PSR > 8 recommended); integrated PLR for whole song; surface on master + per-bus. _(Inspired by: Klangfreund Multimeter https://www.klangfreund.com/multimeter/ + AES Engineering Brief 19324)_ HIGH
- **[CL-226 / AQ]** K-System scale on master meter — selectable K-12 / K-14 / K-20 ballistic with target = 0 at scale anchor; ITU BS.1770 RMS + 83 dBSPL monitor calibration callout. _(Inspired by: Bob Katz K-System + FabFilter Pro-L 2 https://www.fabfilter.com/products/pro-l-2-limiter-plug-in)_ HIGH
- **[CL-227 / AQ]** EBU R128 conformance log + offline arrangement scan — XML/CSV log with timecode-stamped LUFS / LRA / true-peak violations against EBU R128 / ITU-R BS.1770-4 / ATSC A/85; "Scan Project" command walks arrangement faster than real time. _(Inspired by: Nugen VisLM 2 ReMEM)_ HIGH
- **[CL-228 / AQ]** Multi-channel weighting modes on master meter — A / C / K weighting selector for measurement vs perceptual loudness comparison. _(Inspired by: Brainworx bx_meter https://www.plugin-alliance.com/en/products/bx_meter.html)_ HIGH
- **[CL-229 / AQ]** Per-strip Sound Field / mono-compatibility indicator — small 2-LED widget per strip: green when sum-to-mono retains >=90% energy, yellow >=70%, red below; sticky red flag if dipped during last bar. Lighter-weight per-strip companion to CL-038. _(Inspired by: iZotope Insight 2 Sound Field)_ HIGH
- **[CL-230 / AQ]** Streaming codec preview on master — re-encode master through Spotify / Apple Music / YouTube / Tidal / Amazon Music / SoundCloud / Bandcamp / Deezer codecs; feed decoded result to monitor output. Codec smear + ISP clipping audible before bounce. _(Inspired by: Nugen MasterCheck Pro https://nugenaudio.com/mastercheck/)_ HIGH
- **[CL-231 / AQ]** Loudness-penalty readout per platform — predicted dB attenuation each streaming service will apply. Distinct from CL-230 (audio preview) and CL-045 (normalization on bounce). _(Inspired by: MeterPlugs Loudness Penalty Analyzer https://www.loudnesspenalty.com/)_ HIGH
- **[CL-232 / AQ]** Loudness-matched A/B render compare with mix descriptors — render-to-buffer master, normalize playback against reference at chosen target, click-free A/B at loop point with multiple loop sections; per-comparison readouts (Match %, mix-balance suggestions, written instructions). Extends CL-041. _(Inspired by: Mastering The Mix REFERENCE 3 https://www.masteringthemix.com/products/reference)_ HIGH
- **[CL-233 / AQ]** Mix-issue lens panel — real-time flags for "harshness" / "clipping" / "over-compression" / "phase problems" with click-to-isolate location on master playback timeline. _(Inspired by: Mastering The Mix EXPOSE 2 https://www.masteringthemix.com/products/expose)_ HIGH
- **[CL-234 / AQ]** Correlation-by-frequency display on master — phase-correlation curve as function of frequency; reveals which spectral region causes mono-fold phase issues. _(Inspired by: Nugen Visualizer https://nugenaudio.com/visualizer/)_ HIGH
- **[CL-294 / AQ]** True-peak (dBTP) metering — BS.1770 true-peak readout (4x oversample + inter-sample peak detection) as a component separate from the sample-peak bars shipped by QA-RustyMeter. Plan T-f; deferred from QA-RustyMeter. Overlaps CL-036 (master-only ISP); this is the dedicated dBTP readout component. _(Inspired by: ITU-R BS.1770-4 true-peak)_ HIGH
- **[CL-295 / AQ]** Integrated LUFS + LRA (loudness range) — gated whole-program integrated loudness plus the LRA statistic, beyond the per-play/loop-resetting Momentary/Short-Term/Integrated readout QA-RustyMeter shipped on the master; this is the archival whole-render number plus LRA. Plan T-f; deferred from QA-RustyMeter. _(Inspired by: EBU R128 / ITU-R BS.1770-4)_ HIGH
- **[CL-296 / AQ]** Per-strip LUFS readout — extend the master-strip LUFS box (shipped master-only by QA-RustyMeter) to the non-master strips too. Plan T-f; deferred from QA-RustyMeter. Lighter-weight per-strip companion to CL-035 (K-weighted LUFS on every bus). HIGH

### Batch-surfaced (QA-RustyMeter 2026-05-30)
- **[CL-297 / WP]** On-load peak-meter transient (FND-6) — investigate/suppress the brief full-scale flash the per-strip dBFS PEAK bars show on first load. The peak meter is correctly floor-initialized, so it is honestly catching a real brief peak as the engines/graph spin up (or an un-cleared first-block buffer) on load, held ~1s by the peak-hold; this is NOT a meter-init bug (the RMS-ring init bug was the meter-init issue, fixed in QA-RustyMeter). Pre-existing — the peak path predates QA-RustyMeter. Candidate fixes: clear the first post-prepare block's buffer / gate the meter for the first N ms after load. Surfaced during QA-RustyMeter. MEDIUM.

### Tonal Reference & Visualization
- **[CL-235 / AQ]** Tonal balance overlay with leveled view + 30+ genre/subgenre target curves — broad-view 4-region target + fine view; each region highlights when in/out of band. Hip-hop / EDM / pop / orchestral / K-pop / rock / lo-fi etc. _(Inspired by: iZotope Tonal Balance Control 3 https://downloads.izotope.com/docs/tonal-balance-control/meters-and-target-curves/index.html)_ HIGH
- **[CL-236 / AQ]** Capture target curve from streaming source — listens to system audio (Spotify/YouTube tab playback), produces custom target curve from any audio you pipe through it. _(Inspired by: iZotope TBC 3 capture)_ HIGH
- **[CL-237 / AQ]** Weighted target blender — design custom target curve by mixing N genre profiles with per-genre weight. _(Inspired by: iZotope TBC 3 Target Blender)_ HIGH
- **[CL-238 / AQ]** Vocal balance + impact + stereo width meters — dedicated meters tracking vocal-pocket frequency band relative to mix, kick/snare impact band energy, stereo-width energy across freq. _(Inspired by: iZotope TBC 3)_ HIGH
- **[CL-239 / AQ]** Crest factor low-end indicator — visual readout for "low end is too dynamic" vs "low end is too compressed"; computed from dynamics at bottom 3 octaves. _(Inspired by: iZotope TBC 3)_ HIGH
- **[CL-240 / AQ]** Spectral intelligibility overlay on master — toggle overlays "speech intelligibility" target band (~500 Hz–4 kHz) with three preset listening environments (low/medium/high noise). _(Inspired by: iZotope Insight 2 Intelligibility)_ HIGH
- **[CL-241 / AQ]** Gain-reduction history overlay across master chain — single floating timeline view (last 30 s) stacking GR traces from every dynamics module on master bus; color-coded per slot. _(Inspired by: FabFilter Pro-L 2 + Pro-MB https://www.fabfilter.com/products/pro-mb-multiband-compressor-plug-in)_ HIGH

### Master-Bus Mastering Modules
- **[CL-242 / AQ]** Multiband limiter on master — 5-band crossover with phase-compensated filters; PLMixer-style core allocates inter-band attenuation by psychoacoustic priority. Per-band Gain / Priority / Release plus global Master Release. _(Inspired by: Waves L3-LL Multimaximizer)_ HIGH
- **[CL-243 / AQ]** 8 limiter character algorithms — extend LimiterDSP with selectable algorithm trading transparency vs character vs near-zero-lookahead permissibility. _(Inspired by: FabFilter Pro-L 2)_ HIGH
- **[CL-244 / AQ]** Loudness-target limiter mode — set desired LUFS target, limiter auto-targets after listening to track section. Distinct from BLU-108 (auto-ceiling true-peak). _(Inspired by: TC Electronic BRICKWALL HD)_ HIGH
- **[CL-245 / AQ]** Audition Limiting + Unity Gain monitor toggles — momentary "audition the part the limiter is removing" + "compensate output by inverse of input gain". _(Inspired by: FabFilter Pro-L 2)_ HIGH
- **[CL-246 / AQ]** Inflator-style harmonic loudness module — psychoacoustic "loudness without dynamic-range loss"; probabilistic resampling adds asymmetric harmonics; two-band split with Effect / Curve / Mix. _(Inspired by: Sonnox Oxford Inflator https://www.sonnox.com/plugin/oxford-inflator)_ HIGH
- **[CL-247 / AQ]** Dynamic resonance suppressor on master — auto-detects narrowband resonances, applies dynamic notch only when crossing threshold; preserves transients via "soft"/"hard" mode + sharpness + selectivity. M/S + L/R + per-band scope. _(Inspired by: oeksound soothe2 + Mastering The Mix RESO)_ HIGH
- **[CL-248 / AQ]** Ultra-high-frequency shelf "AIR" band on master EQ — 2.5 / 5 / 10 / 20 / 40 kHz selectable shelf frequencies for vocal sheen + mix bus polish. Implementable as EQ8 band-type variant. _(Inspired by: Maag Audio EQ4 AIR BAND https://www.plugin-alliance.com/en/products/maag_eq4.html)_ HIGH
- **[CL-249 / AQ]** SUB band ultra-low-frequency mastering shelf (10 Hz) — for surgical sub-rumble control. _(Inspired by: Maag EQ4 SUB Band)_ HIGH
- **[CL-250 / AQ]** Spectral dynamics on EQ8 — extend Dynamic EQ bands so each can switch from "whole-band" gain change to "spectral mode" (only triggers on specific frequencies inside band exceeding threshold) with per-band density + selectivity. _(Inspired by: FabFilter Pro-Q 4 Spectral Dynamics https://www.fabfilter.com/help/pro-q/using/spectral-dynamics)_ HIGH
- **[CL-251 / AQ]** Stem EQ on master — split master into Vocal/Drums/Bass/Other stems via ML model; each stem gets independent 8-band EQ. Distinct from CL-047 file-level export. Strategic: ML model dependency — V2+. _(Inspired by: iZotope Ozone 12 Stem EQ)_ HIGH _*Research Required*_
- **[CL-252 / AQ]** Stem Focus per-effect-module mode — on any rack effect (and EQ8), "stem target" dropdown internally splits bus signal via Master Rebalance, applies effect to selected stem only, sums back. _(Inspired by: iZotope Ozone 11 Advanced Stem Focus)_ HIGH
- **[CL-253 / AQ]** Decompressor / "Unlimiter" module — ML-based "undo" stage that estimates and reverses heavy compression to restore lost transients. Strategic: ML model dependency — V2+. _(Inspired by: iZotope Ozone 12 Unlimiter)_ HIGH _*Research Required*_
- **[CL-254 / AQ]** Impact 4-band rhythm/feel enhancer — split master into 4 bands (sub/punch/clarity/air) with per-band drive/grit/contour controls aimed at rhythmic feel rather than tonal balance. _(Inspired by: iZotope Ozone 11 Impact)_ HIGH

### Console / Channel Character
- **[CL-255 / AQ]** Console TMT-style channel variance per insert — "warmth" / "channel variance" toggle on each insert strip selects one of N (e.g. 32-72) micro-tolerance variant profiles; off by default. _(Inspired by: Brainworx bx_console TMT https://www.plugin-alliance.com/en/products/bx_console_focusrite_sc.html)_ HIGH
- **[CL-256 / AQ]** Per-channel virtual analog noise + THD — "Virtual Gain" rotary on each strip injects subtle simulated analog noise + variable continuous THD coloration. _(Inspired by: bx_console SC)_ HIGH
- **[CL-257 / AQ]** Master summing-bus character — single Mode selector on master InsertNode: Clean / API-style Thrust / Neve / SSL Glue. Adds harmonic coloration at summing node. Distinct from CL-021 (per-strip console). _(Inspired by: API Vision Console summing https://www.uaudio.com/products/api-vision-channel-strip-collection)_ MEDIUM

### Routing & Workflow
- **[CL-258 / AQ]** Mono-down / channel-solo monitor toggles on master strip — momentary "sum to mono" + "left only" + "right only" + "mid only" + "side only" listen modes with shared keyboard shortcuts. _(Inspired by: Brainworx bx_meter solo modes + bx_digital V3 Mono Maker)_ HIGH
- **[CL-259 / AQ]** Master-bus revision snapshots A/B/C/D — store up to 8 named snapshots of entire master rack (effects + params + EQ8 + limiter + sends) per project; visual diff of changed params on switch. _(Inspired by: FabFilter Pro-Q 4 A/B + iZotope Ozone undo)_ MEDIUM
- **[CL-260 / AQ]** Plugin Instance List for EQ8 / Limiter / Compressor — global overview panel showing every active EQ8 / Limiter / Compressor instance in project; one-click jump-to-strip + bulk preset recall + copy/paste params. _(Inspired by: FabFilter Pro-Q 4 Instance List https://www.fabfilter.com/help/pro-q/using/instance-list)_ HIGH
- **[CL-261 / AQ]** EQ Sketch — draw EQ curve in one mouse gesture; system snaps to optimal band placement + Q + slope based on stroke shape. _(Inspired by: FabFilter Pro-Q 4 EQ Sketch https://www.fabfilter.com/help/pro-q/using/eq-sketch)_ HIGH
- **[CL-262 / AQ]** Spectrum Grab on EQ8 — hover live spectrum, after dwell-delay enter grab-mode (existing bands dim, spectrum freezes); drag a peak to drop a Bell band with auto-Q. _(Inspired by: FabFilter Pro-Q 4 Spectrum Grab https://www.fabfilter.com/help/pro-q/using/spectrumgrab)_ HIGH
- **[CL-263 / AQ]** Headphone-correction profile loader on master — drop-in plugin slot reading JSON / wav-IR profile; ships with starter set for ~30 popular headphones + user-measured imports. _(Inspired by: Sonarworks SoundID Reference https://www.sonarworks.com/soundid-reference)_ HIGH
- **[CL-264 / AQ]** Translation-check listening simulator — single button cycles master output through "phone speaker / car stereo / club system / hi-fi / earbuds" simulated playback environments. _(Inspired by: Sonarworks SoundID Translation Check)_ HIGH
- **[CL-265 / AQ]** Speaker / room correction profile loader on master — IR-based room calibration profile slot, separate from headphones (CL-263). _(Inspired by: Sonarworks SoundID Reference room correction)_ HIGH
- **[CL-266 / AQ]** Master Assistant — analyze project's master bus over N-second pass, propose starting EQ8 + limiter + multiband chain targeted to genre profile (Pop/Rock/Hip-hop/EDM/Acoustic); each stage individually accept-or-discard; user can upload reference WAV instead of genre tag. Distinct from CL-046 (per-track) and CL-042 (static genre presets — this writes target params into them). _(Inspired by: iZotope Ozone 12 Master Assistant)_ HIGH
- **[CL-267 / AQ]** Dolby Atmos Music Panner per-tab — per-tab object panner widget (X/Y/Z + elevation modes Manual/Wedge/Dome/Ceiling) writing positional metadata into project-level Atmos bed; built-in step sequencer with tempo-sync allows automating object positions. Full Atmos render bus is CL-033; this is per-tab UI. _(Inspired by: Dolby Atmos Music Panner)_ HIGH
- **[CL-268 / AQ]** Per-platform downmix monitor formats on master — Lo/Ro stereo / Pro Logic IIx / 5.1-direct / 7.1-direct / 2.0-from-spatial-audio toggle. Surround-aware mixing to monitor downmix in real time. _(Inspired by: Logic Pro downmix and trim controls)_ MEDIUM

## System Pages

Builder, Effects Page, Audio Settings, Project Persistence, Keyboard /
Mouse docs.

### Source-doc backlog
- **[BLU-478 / WP]** TB-T1 LAT readout in ms — restore millisecond conversion. PRESET-SAFE.
- **[BLU-480 / WP]** FX-1 Rack UI refactor — sidebar picker + detail pane. PRESET-SAFE.
- **[BLU-487 / WP]** Sample Browser — real file-system tree, drag from disk. Post-Phase H/I.

### Batch-surfaced (QA-AudioMeters 2026-05-24)
- **[CL-293 / WP]** Builder grid per-row DBFS meter — add a small DBFSMeter widget to each Builder-grid audio-row strip showing the same peak-dB data as the Mixer-page Audio insert per-strip meter. Backing storage (`mAudioRowPeakDb*[row]`) already populated post-QA-AudioMeters via the unified G1 chain (`InsertNode publishPeakReading` -> `VibeGraph.audioInsertPeakDb*[row]` -> `drainAndMerge` -> `mAudioRowPeakDb*[row]`); the Builder side just needs a meter widget wired into each audio-row paint code path + a `mProcessor.drainInsertPeakDbStereo(VibeGraph::InsertKind::Audio, row)` poll per vblank in `BuilderPage::onVBlank` (or equivalent). Surfaced during QA-AudioMeters Task 3 verify when Jeff observed there's no DBFS strip on Builder tracks today + the §5/§9 "per-row Builder audio meters" naming refers only to the per-row backing storage, not to a Builder-grid display. Not in current QA-AudioMeters scope (the batch closes the bus-vs-insert architectural inconsistency; this would be a new UI feature on the data the batch already plumbs). MEDIUM.

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
- **[CL-050 / PE]** AVX2 / AVX-512 runtime detection + path selection.
- **[CL-051 / PE]** Disk-based sample streaming optimization — read-ahead tuning per sample size.
- **[CL-052 / PE]** Background sample pre-load — predict next-likely sample, pre-load into RAM cache.
- **[CL-053 / PE]** Smarter voice management — priority-based stealing across all engines, not just per-engine. Architecture research 2026-05-08 expanded scope: per-engine cap audit + revisit (Surge XT 64, Massive X 1-64, Pianoteq 256 vs BaySickDAW 1-16 default 16). BLU-353 same-pitch preempt + note-off strip validated correct vs Surge "Reuse Single" + Massive X "Reassign" — preserve, do not undo.
- **[CL-054 / PE]** Per-engine CPU budgets — cap voices per engine when CPU exceeds threshold. Architecture research 2026-05-08 expanded scope: Pianoteq-style CPU-aware auto-polyphony with Optimistic 75% / Pessimistic 50% targets.
- **[CL-055 / PE]** Smart freeze — auto-freeze tracks when CPU exceeds 80%; restore on demand.
- **[CL-056 / PE]** Adaptive block sizing — small block for live monitoring, large for offline render.
- **[CL-057 / PE]** ASIO buffer-size hot-swap — change buffer without engine restart.
- **[CL-058 / PE]** Per-effect CPU % display — which slot is the hog (in mixer strip + Effects page).
- **[CL-059 / PE]** Memory profiler — per-engine RAM usage display + leak detection.
- **[CL-060 / PE]** Faster project load — parallel page restoration; lazy-load non-active tabs.
- **[CL-061 / PE]** Async preset load — don't block UI on preset switch.
- **[CL-062 / PE]** Lock-free everywhere — audit remaining locks in audio path; replace with atomic / RCU patterns.

### Audio-Device Infrastructure (walked)
- **[FSW-121 / PE]** RAM-load <15MB clips — replaces AudioClipStreamer cold-start sputter. Post-V1 optimization (MT engine reduces but doesn't eliminate the symptom). _(walked from V1 backlog 2026-05-08; superseded by CL-281 MP3 decode-once cache 2026-05-08 per architecture research — preserved for history.)_

### Group-shared effect settings
- **[CL-194 / AQ]** Group-shared settings across plugin instances — VTM-style "group" assignment where all instances on Group 1 share Calibration / Wow & Flutter / Bias settings. Up to 8 groups. _(Inspired by: Slate VTM + VCC + Waves NLS 8 VCA groups)_ HIGH
- **[CL-195 / AQ]** Group-Bypass + Group-Drive offset on linked instances — Group Bypass kills effect across all instances; Group Drive single offset nudges drive on every instance. _(Inspired by: Waves NLS https://www.waves.com/plugins/nls-non-linear-summer)_ HIGH
- **[CL-196 / AQ]** Group-Noise master across linked instances — kills/enables analog hiss on all instances in group. _(Inspired by: Waves NLS)_ HIGH
- **[CL-197 / AQ]** Inter-Sample-Peak (ISP) detection on saturation/clipper output meters — every saturation Type's output meter measures inter-sample peaks when signal driven beyond ~-0.5 dBFS. CL-036 already covers master ISP; this is per-effect-output sweep. _(Inspired by: Kazrog KClip 3 32x oversampling + UAD Studer + Saturn 2 32x oversampling)_ HIGH

### FFT plan caching (architecture 2026-05-08)
- **[CL-269 / PE]** FftPlanCache keyed by FFT order, owned by VibeGraph — `std::shared_ptr<const juce::dsp::FFT>` shared across linear-phase EQ8 instances + PhaseVocoder + Harmless wavetable; eliminates per-instance twiddle-factor table duplication (3.2 MB at 2048 / 12.8 MB worst-case at 4096). Concrete 7-file impl sketch in source. _(Source: daw-architecture-research-2026-05-08.md §2)_
- **[CL-270 / PE]** Per-thread-pool FFT instances (MT engine fallback variant) — only if tsan flags shared-FFT issues post-QA-Md; cache key becomes `(order, threadIndex % poolSize)`. _(Source: daw-architecture-research-2026-05-08.md §2)_
- **[CL-271 / PE]** FFTW wisdom file for deterministic startup — only relevant if we ever swap `juce::dsp::FFT` -> FFTW; deferred until then. _(Source: daw-architecture-research-2026-05-08.md §2)_

### Lock-free MIDI dispatch upgrades (architecture 2026-05-08)
- **[CL-272 / AQ]** Multi-event MIDI ring (`juce::AbstractFifo` or `farbot::fifo`) for chord-strum / paste-to-roll / arpeggio-from-UI-clock — defer until use case lands; 3 atomics per engine (`mAuditionNote` / `mAuditionHoldOn` / `mAuditionHoldOff`) consolidate to one POD ring. _(Source: daw-architecture-research-2026-05-08.md §3)_
- **[CL-273 / AQ]** MIDI ring backpressure signal — caller sees overflow vs silent drop; pairs with CL-272. _(Source: daw-architecture-research-2026-05-08.md §3)_
- **[CL-274 / AQ]** farbot vendoring decision (alternative to in-tree AbstractFifo) — spec call when CL-272 ships; farbot has overwrite-or-default semantics matching current single-slot atomic pattern. _(Source: daw-architecture-research-2026-05-08.md §3)_

### Sample streaming architecture (architecture 2026-05-08)
- **[CL-279 / PE]** SamplePool with refcount + preload-head (process-wide, keyed by file path) — concrete `class SamplePool` sketch in source; foundation for CL-280..CL-286. Reuse across `AudioClipStreamer` + drum pads + sampler voices; eliminates 8x RAM duplication when same WAV used 8 times. _(Source: daw-architecture-research-2026-05-08.md §5)_
- **[CL-280 / PE]** Preload-head for streamed clips (256 KB always-in-RAM) — eliminates 3.5s synchronous prefill on first play of >100 MB streamed clip. Folds into CL-279 SamplePool. _(Source: daw-architecture-research-2026-05-08.md §5)_
- **[CL-281 / PE]** MP3 decode-once cache (decode-to-PCM at clip-add time on message thread; cache result) — supersedes walked-state FSW-121 (RAM-load <15MB clips brute-force); more elegant solution to same problem. Use PCM cache for all subsequent playback. _(Source: daw-architecture-research-2026-05-08.md §5)_
- **[CL-282 / WP]** Streaming telemetry (atomic underrun counter + peak-prefill-latency-ms gauge + `jassertfalse` in Debug) — EXS24-style "data not read in time" UI counter; surfaced on debug overlay. _(Source: daw-architecture-research-2026-05-08.md §5)_
- **[CL-283 / PE]** Sampler engines consume SamplePool — wire `VibeSampleManager` + Phase D drum engines + BaySickPlayer through `SamplePool::getOrLoadHead(path)`; this is what unlocks Kontakt-scale (multi-GB) libraries. _(Source: daw-architecture-research-2026-05-08.md §5)_
- **[CL-284 / PE]** HISE-style auto-RAM-load fallback for big-pitch samples — samples mapped across many octaves with high static pitch ratio bypass streaming and stay in RAM. _(Source: daw-architecture-research-2026-05-08.md §5)_
- **[CL-285 / PE]** DrumGizmo multi-channel collapse — dedupe simultaneous reads of same file/position from different channels; I/O-level optimization on top of CL-279 SamplePool. _(Source: daw-architecture-research-2026-05-08.md §5)_
- **[CL-286 / PE]** Sfizz-style GC thread polling — separate `juce::Thread` polling `lastReleasedNs > NOW - 5s` for unreferenced sample cleanup; not on audio thread; pairs with CL-279 SamplePool refcount. _(Source: daw-architecture-research-2026-05-08.md §5)_

### Auto-update infrastructure
- **[CL-287 / WP]** Beta / pre-release update channel — opt-in toggle in General Settings adds a second appcast manifest (`appcast-beta.xml` in GitHub Releases) that points at pre-release tagged versions. Pairs with QA-Updater stable channel (V1 batch). Deferred from V1 because populating a beta cycle would require shipping the installer infra before all V1 features are finished. _(Source: user spec call 2026-05-08; see Main Plan §9 sixth Forks entry — QA-Updater addition.)_

### API consistency / migrations
- **[CL-288 / WP]** Migrate AlertWindow call sites from convenience wrappers (`showMessageBoxAsync` / `showOkCancelBox`) to the newer `showAsync(MessageBoxOptions...)` builder API — ~25 call sites across `Source/Standalone/`, `Source/BaySickRustyDrumsPage`, `Source/BaySickPedals`, `Source/BaySickNAMIR`, `Source/VibePlayer`, etc. Behavior identical; advantage is forward-compat with future JUCE-only features (parent-window association, accessibility hooks, etc.) + alignment with current JUCE-recommended docs/tutorials. No technical debt in the strict sense (older wrappers compile to the same generated code). Deferred to a single sweep batch — never one call at a time, to avoid mixed patterns. Trigger conditions: (a) JUCE deprecates the wrappers in a major release; (b) a builder-only feature is needed; (c) Phase 6 pre-release decision picks "migrate now". _(Source: QA-Md mid-batch conversation 2026-05-08; flagged for Phase 6 QA-Audit review.)_

### Security / Hardening
- **[CL-289 / WP]** Create `/audit-security` agent — pre-release / pre-public-repo + pre-network-feature security sweep. Mirrors `/audit-licenses` cadence (heavyweight, milestone-triggered, not recurring). Risk-ranked output (Critical / High / Medium / Low) with concrete file:line + suggested mitigation. Tier-1 scope (V1 pre-release): vendored-library CVE scan against NVD/GitHub Advisories (libsndfile, sfizz, NAM, IR, JUCE, moodycamel, sqlite, etc.); file-parser audit (WAV/MP3/SFZ/project-XML/preset readers — input validation, buffer bounds, path traversal); DLL safety (search-order, hijacking patterns); save-file format audit (XXE in project XML, billion-laughs). Tier-2 scope (when QA-Updater lands): network code audit (appcast XML parsing, signature-verify chain, downloaded-binary handling). Tier-3 scope (post-V1, when cloud features land): API key handling, auth tokens, network protocol review. Decision call to make in Phase 6 QA-Audit: build-the-agent-now (so QA-RC includes the security sweep) vs build-when-network-features-land. _(Source: user spec call 2026-05-08 mid-QA-Md; flagged for Phase 6 QA-Audit review.)_

### Crash reporting / symbol-server infrastructure
- **[CL-290 / WP]** Crash-report + symbol-server pipeline — once auto-update lands and BaySickDAW ships to real users, in-the-field crashes need to symbolicate back to source line numbers without forcing the user to rebuild. Four moving parts: (1) `.pdb` (Program Database) generation + archival for every Release build (build-pipeline change — keep `.pdb` files alongside the shipped `.exe` but do NOT bundle into installer); (2) in-app crash reporter (Windows Error Reporting via `WerRegisterRuntimeExceptionModule`, OR a third-party like Sentry / Bugsnag / Crashpad — spec call); (3) symbol server (private storage where `.pdb` files live indexed by `.exe` build hash — could be a simple S3/Azure-Blob/HTTPS-folder or a proper Microsoft Symbol Server format); (4) symbolication tooling (when a user-submitted `.dmp` arrives, combine with the matching `.pdb` to produce a readable stack trace). Pairs naturally with QA-Updater scope — both are post-release support infrastructure. Decision calls in Phase 6 QA-Audit: (a) third-party SDK vs OS-native Windows Error Reporting; (b) symbol-server hosting model (private blob vs public Symbol Server vs none with on-request `.pdb` matching); (c) whether the in-app reporter prompts the user before sending or sends silently with prior consent in EULA. _(Source: user spec call 2026-05-08 mid-QA-Md; flagged for Phase 6 QA-Audit review + relevant during QA-Updater execution.)_

### DSP meter UX
- **[CL-291 / WP]** DSP meter cap V1 release value — currently raised from 2.0 (200%) to 10.0 (1000%) during QA-Md after the original 200% cap masked a 420-percentage-point Debug overload gap (Debug-MT-on=450% vs Debug-MT-off=870%, both pegged at 200% on the original cap). Active-development value is now 10.0 to support diagnostic visibility for downstream MT-touching batches; V1 release value is a UX call. Three plausible release values: (a) 2.0 — original; conservative; matches FL Studio "100% means glitch" UX convention; loses post-cap diagnosis. (b) 5.0 — middle ground; shows real overload up to 500% (covers typical Release heavy load); novice-friendly. (c) 10.0 — maximum diagnostic; honest about Debug-tier overload but may surprise novice users seeing "870%". Decision in Phase 6 QA-Audit, ideally with a few weeks of typical Release session-load data to inform. _(Source: QA-Md diagnostic findings 2026-05-09; flagged for Phase 6 QA-Audit review.)_

### MT diagnostic compile-flag gate
- **[CL-292 / WP]** Wrap MT diagnostic counters + Mixer hamburger menu item behind `#if BAYSICKDAW_MT_DIAGNOSTIC` for V1 release — counters (in `RenderEngineFlags.h` MtDiagnostic namespace), increment sites (in `VibeThreadPool.cpp` + `RenderGraphDispatcher.cpp`), and the "Run MT Diagnostic (2s capture)" Mixer hamburger menu item (in `StandaloneEditor.cpp`) all stay active during development but compile out of Release shipping builds. Prevents end users from seeing a developer-facing menu item they'd have no context for. CMake side: add `add_compile_definitions(BAYSICKDAW_MT_DIAGNOSTIC)` under the Debug-config branch in `CMakeLists.txt` (or a similar opt-in macro). Decision details: should it also be available in Release-with-symbols builds for crash-report pairing (CL-290)? Should the menu item move to a "Help -> Developer" submenu when present? Decisions in Phase 6 QA-Audit. _(Source: user spec call 2026-05-09 at QA-Md close; flagged for Phase 6 QA-Audit review.)_

## User Tools / Learning

AI helpers (Composer / Mixer / Master), tutorials, smart melody / chord
/ drum / bass generators, hover-to-hear, scale picker, beat detection,
sound-design guides.

### Fire-Hose AI / smart features
- **[CL-063 / UT]** AI assistant "Composer" — generate full song from text prompt. _*Research Required*_
- **[CL-064 / UT]** AI assistant "Mixer" — auto-mix the current project. _*Research Required*_
- **[CL-065 / UT]** AI assistant "Master" — auto-master to genre target. _*Research Required*_
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

### Vocal assistants & match
- **[CL-138 / UT]** Audiolens-style vocal tone match — drop reference vocal, auto-extract spectral character, seed BaySickVocal chain. _(Inspired by: iZotope Nectar 4 + Audiolens; docs.izotope.com WebFetched)_ HIGH
- **[CL-139 / UT]** One-knob vocal assistant — "produce my vocal" panel that ducks complexity. _(Inspired by: iZotope Nectar 4 Vocal Assistant)_ HIGH
- **[CL-143 / UT]** Six-fader CLA-style vocal macro panel — beginner-friendly companion (Bass / Treble / Compression / Reverb / Delay / Pitch-modulation). _(Inspired by: Waves CLA Vocals)_ MEDIUM

### Player scripting & sound match
- **[CL-211 / UT]** Lua-style preset scripting hook — sandboxed Lua scripting layer where patch carries script processing incoming MIDI events (humanize/arpeggiate/chord-recognize/generate/transform) before voice allocation. _(Inspired by: UVI Falcon UVIScript https://lua.uvi.net/)_ HIGH
- **[CL-217 / UT]** Sound Match + Sound Lock browser mode — when patch is open, lock selected sound aspects (envelope/filter/FX), find related patches matching the locked aspects. _(Inspired by: Spectrasonics Omnisphere Sound Match / Sound Lock)_ HIGH

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

### Vocal preset library
- **[CL-142 / WP]** Vocal preset chain library — genre-tagged starter chains (Pop Lead / Hip-Hop Lead / Acoustic Folk / Rock Wall / Lo-Fi). Subset of CL-042 mastering-chain templates. _(Inspired by: Waves CLA Vocals + LANDR)_ MEDIUM

### Hardware controller mapping
- **[CL-222 / WP]** Hardware controller mapping presets — auto-map common hardware MIDI controller layouts (M-Audio Oxygen / NI Komplete Kontrol / Ableton Push / Novation Launchpad / Korg nanoKontrol) into per-engine macro section. _(Inspired by: Spectrasonics Omnisphere 300+ hardware profiles)_ HIGH

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

Items reviewed during QA-Inventory walkthrough (or post-walk Phase-4 reclassify or post-architecture-research) and confirmed dropped, with reason.  Ensures we don't re-litigate.

Sub-categorised by drop reason (NOT by domain — that's Section 1):

- **Won't do** — design / scope / audience-fit decisions; we deliberately don't want this.
- **Can't do** — legal / proprietary blockers (third-party model weights, licensed IP, vendor restrictions); resolution requires external action.
- **Tech not yet feasible** — current technology can't deliver the value claim economically; block may resolve in future hardware / software / driver landscape.

Every sub-category appears as a `## ` header even when empty (canonical-structure rule from Main Plan §0).

---

## Won't do — design / scope (18)

### Harmless UI items dropped during QA-A STYLE / Phase 5F-3 (12)

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

### Drum engine option dropped (1)

- **[BLU-423 / OT]** Harmless as 3rd engine option for drums — not drum-tuned. Confirmed drop in walk.

### Spec items dropped as ambiguous / N/A (3)

- **[FSW-038 / OT]** F2 Edit Properties dialog (BaySickPitch) — dropped in walk.
- **[LDT-390 / OT]** 5F-5 ambiguous Event Editor items (LED ON toggle / RANGE box / Link icon / spline tool icon / target-link icons / mode-toggle radio-switch) — original spec ambiguous; dropped during 5F-5 implementation, confirmed in walk.
- **[LDT-392 / OT]** 5F-6 ambiguous Piano Roll items (Speaker icon transport / Window controls / Draggable lane divider) — ambiguous / structural / N/A; confirmed drop.

### Phase-4 verification reclassify-to-Drop (1)

- **[FSW-244 / OT]** Per-input-channel diagnostic ("Show Input Diagnostics dialog") — Phase-4 source verification couldn't find the dialog string in source (`MixerPage::showInputChannelPicker` has no diagnostic submenu). Original spec described a feature that was never built. Walked: dropped because spec intent is unclear; building from a fuzzy spec creates the wrong thing.

### QA-Inventory walk reclassify-to-Drop (1)

- **[BLU-605 / OT]** voxRoll/instRoll dead-code cleanup — originally proposed as Phase 6 cleanup. Per walk: piano-roll infrastructure is NEEDED for Inst (BaySickGuitars / BaySickBasses) and reserved for future SFZ vocal player. Not dead code; should not be removed.

## Can't do — legal / proprietary (0)

*No items in this bucket yet.*

## Tech not yet feasible (1)

- **[CL-049 / PE]** GPU offload for FFT-based effects — DROPPED 2026-05-08 per `daw-architecture-research-2026-05-08.md`. Evidence: GPU break-even is ~16K-point FFT, BaySickDAW uses 2048-point everywhere; 2× buffer-size latency tax + 100-200 µs kernel launch overhead consumes ~20% of audio budget at 96 kHz/96-sample buffer; KVR reviews of GPU Audio FIR Convolver show real integration risk (CPU spikes, dropouts, host crashes); no mainstream DAW ships GPU-FFT integration. Re-graduate if SDK / hardware / driver landscape changes.

---

**End of Future State.md.** Append-only — new items welcome. Original IDs preserved for traceability.
