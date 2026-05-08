# Competitive Research Sweep — 2026-05-08

> **Status:** Candidate list — NOT yet applied to `Future State.md`. Each entry needs human triage (yes / no / maybe) before being added to the long-term roadmap. See "Triage workflow" below.

> **Method:** 4 parallel `competitive-research` subagent runs, each with WebSearch access (WebFetch was denied — entries are sourced from WebSearch result snippets that quote vendor pages directly). Each agent followed the strict "verify or stop" rules — no fabrication from training-data memory.

> **Total candidates:** 84 entries (CL-109..CL-192). Numbered sequentially by sweep order: Vocal (CL-109..CL-132), Effects/Saturation (CL-133..CL-154), Players (CL-155..CL-171), Mixer/Mastering/Metering (CL-172..CL-192). Each entry's bucket assignment is shown alongside.

> **Confidence:** every entry has a vendor URL and at least one verified search hit. Quality is "vendor SAYS this exists", not "this is genuinely novel vs what BaySickDAW already has". Triage required.

---

## Methodology + caveats (cross-sweep)

- **WebFetch was denied** during all 4 runs. Agents extracted feature claims from WebSearch result snippets that quote vendor pages. This means:
  - URL exists ✓ (WebSearch returns the link)
  - Vendor claims the feature ✓ (snippet content matches the claim)
  - But the deeper page text wasn't fetched, so version-specific feature claims may be slightly stale or imprecise.
- **All 4 agents read** `Plans & Specs/Previously Implemented.md` + `Future State.md` first to dedupe against existing entries. Their "In-plan" sections list features that map to existing CL-* / BLU-* / FSW-* / LDT-* IDs and were intentionally NOT re-proposed.
- **Confidence ratings** are vendor-page-verified (HIGH) or third-party-review (MEDIUM). LOW was avoided per agent rules.

## Triage workflow

When you start triaging:

1. **Auto-skip** any entry below HIGH confidence — defer to a future sweep with WebFetch enabled.
2. **First pass per entry:**
   - **YES, want** — clearly distinct + audience-fits + sounds useful → approve
   - **NO, drop** — duplicate of existing, audience-wrong, or marketing fluff → drop with one-line reason
   - **MAYBE** — needs deeper look → defer to second pass
3. **Second pass on MAYBE only.** By that point you've decided which categories matter.
4. Accept partial completion. 30-50% conversion to Future State entries is normal for a fire-hose like this.

## Confidence summary at a glance

| Sweep | HIGH count | MEDIUM count | Net new | Note |
|-------|------------|--------------|---------|------|
| Vocal | 24 | 0 | 24 | All vendor-page sourced |
| Effects/Saturation | 22 | 0 | 22 | All vendor-page sourced |
| Players/Synth | 17 | 0 | 17 | Confirmed both structural gaps from blocked-run |
| Mixer/Mastering | 18 | 3 | 21 | CL-181, CL-191, CL-189 are MEDIUM (synthesis from multiple vendor patterns rather than single vendor source) |

## Strategic notes (cross-sweep)

Three patterns worth flagging before you triage:

1. **Architectural-impact entries** — CL-167 (Multi-engine container `BaySickFalcon`), CL-168 (Modular patch-graph), CL-165 (Wave-sequencing engine), CL-153 (Neural-network/NCM saturation engine) would change foundational assumptions. Probably too big for V1.5; mark as V2+ if approved.
2. **Cross-cutting entries** — CL-148 (group-shared settings) touches Mixer + Effects rack, doesn't belong cleanly in Effects bucket. CL-130 (ARA2 plumbing) touches the broader recording flow. These should route to Cross-cutting Infrastructure when approved.
3. **Duplicates of existing CL-* / BLU-* entries** — several Effects entries (CL-147 / CL-148 / CL-149 multiband saturation extensions) refine BLU-172. Decide whether to fold them into BLU-172's entry as expanded scope or ship as separate items.

---

# Sweep 1 — Vocal Effects + Pitch Correction (CL-109..CL-132)

**Bucket assignment:** primarily Effects (BaySickVocal extensions); CL-129 → User Tools / Learning.

## Already-have (skipped — already shipped)

- Realtime + offline pitch correction (Auto-Tune-style + Newtone-style) → BaySickVocal H-5 hybrid hop=256/512 + BaySickPitch (FSW-127, FSW-134, FSW-141)
- VocAlign-style time alignment → BaySickAlign 3-lane GUIDE/DUB/OUTPUT (FSW-135, FSW-143, FSW-145)
- Vocal-tuned compressor with character types (Modern/FET/Opto) → FSW-130, FSW-138
- Split-band sibilant control (de-esser HPF + dynamic notch 5-10 kHz) → FSW-132, FSW-139
- Vocal-tuned saturation umbrella (Tube/Console/Tape + Vocal Body shaping) → FSW-131, FSW-147
- 5-algorithm reverb umbrella with VocalBooth + sidechain ducking + tempo-sync pre-delay → FSW-149
- Delay extensions including VocalDoubler + Slapback + sidechain ducking → FSW-148
- Pitch tracker (YIN/MPM with worker thread + atomic publish) → FSW-140
- IR / amp profile chain stage → BaySickNAM/IR (FSW-146)
- Pre-EQ8 M/S in vocal chain → FSW-142

## In-plan (not double-added)

CL-024 (T-Pain hard-tune), CL-025 (Chord harmonizer), CL-026 (Vocal exciter), CL-027 (De-noise), CL-028 (De-reverb), CL-029 (De-clipper), CL-030 (Click/pop removal), CL-084 (Vocal isolation) + LDT-428, BLU-278 (Vocal Rider), BLU-287 (Pitch Shifter), BLU-284 (Subharmonic), BLU-153 (Shimmer), BLU-516 (Analog Drift), BLU-293 (Vibrato), CL-007 (Vocoder), CL-008 (Talkbox), CL-014 (Pitch correction for instruments), BLU-330 (Harmless Harmonizer).

## Proposed additions

### Pitch correction / hard-tune extensions (Effects bucket)

- **[CL-109 / AQ]** Real-time scale-quantize hard-tune in BaySickPitch with custom scales — 43+ western/eastern scales + user-savable customs. Extends CL-024 with the scale library that powers it. _(Inspired by: Waves Tune Real-Time https://www.waves.com/plugins/waves-tune-real-time)_ HIGH
- **[CL-110 / AQ]** Per-note "wrong note" exclusion list — designate scale degrees the corrector is forbidden to snap to (handles passing tones / chromatic singers). _(Inspired by: Waves Tune Real-Time)_ HIGH
- **[CL-111 / AQ]** Auto-Key 2-style automatic key + scale + tempo detector — drop a song reference, auto-populate scale + project tempo. _(Inspired by: Antares Auto-Key 2 https://www.antarestech.com/products/auto-tune/auto-key)_ HIGH
- **[CL-112 / AQ]** Throat / vocal-tract physical model post-pitch-shift — 5-point graphical throat shaper for gender / age / character morph independent of pitch. _(Inspired by: Antares THROAT Evo + Auto-Tune Pro 11 https://www.antarestech.com/community/introducing-auto-tune-pro-11)_ HIGH
- **[CL-113 / AQ]** Glottal waveform + breath-noise injection — variable breath profile (rasp -> smooth -> whisper) tied to throat modeling. _(Inspired by: Antares THROAT Evo)_ HIGH

### Harmony generation (Effects bucket)

- **[CL-114 / AQ]** 4-voice formant-corrected harmony generator with per-voice formant + pan + level + EQ + gate + stereo width — per-voice mixer + envelope on top of CL-025 algorithm. _(Inspired by: Auto-Tune Pro 11 Harmony Player)_ HIGH
- **[CL-115 / AQ]** Choir / vocal multiplier — turn one vocal into 4 / 8 / 16 / 32 distinct unison voices with per-voice pitch / timing / vibrato / spread. _(Inspired by: Antares CHOIR Evo)_ HIGH
- **[CL-116 / AQ]** MIDI-driven harmony — feed harmony notes from MIDI track / external controller / piano roll. _(Inspired by: Antares Harmony Engine Evo)_ HIGH
- **[CL-117 / AQ]** Backing-singer style packs — preset chooser of 8+ backing-singer styles (gospel / pop / soft-pad / oohs-aahs). _(Inspired by: iZotope Nectar 4 Backer module)_ HIGH

### Vocal alignment / production polish (Effects bucket)

- **[CL-118 / WP]** Multi-track group alignment — align N dub tracks to one guide in single pass with per-track tightness override + ARA-style group editing. Extends BaySickAlign single-guide+dub. _(Inspired by: VocAlign Ultra Process Groups)_ HIGH
- **[CL-119 / AQ]** Vocal doubler effect with controllable pitch variation + timing offset + formant shift — offline / look-ahead pitched-double synthesis. _(Inspired by: Revoice Pro 5 Vocal Doubling)_ HIGH
- **[CL-120 / WP]** Sync points + protected areas in BaySickAlign — user-defined target points + protected regions. _(Inspired by: VocAlign Ultra)_ HIGH
- **[CL-121 / AQ]** Phase-aligned multi-mic vocal stacks — automatic sub-sample delay + all-pass phase rotation across multiple takes/mics. _(Inspired by: Sound Radix Auto-Align 2)_ HIGH

### Vocal restoration tools (Effects bucket)

- **[CL-122 / AQ]** Adaptive de-noise mode — track changing noise floor across the take in real time (vs CL-027 fixed-train). _(Inspired by: iZotope RX 11 Voice De-noise Adaptive)_ HIGH
- **[CL-123 / AQ]** Combined dialogue/vocal repair-assistant — single panel runs de-noise + de-reverb + de-click + de-clip with auto-classify of input type. _(Inspired by: iZotope RX 11 Repair Assistant)_ HIGH

### Specialized vocal dynamics (Effects bucket)

- **[CL-124 / AQ]** Variable-frequency de-esser — sweepable sidechain HPF + spectral notch with auto-track of sibilant frequency. _(Inspired by: Antares SYBIL Evo)_ HIGH
- **[CL-125 / AQ]** Sibilant-balance tool in BaySickPitch (per-note) — drag a single note's sibilant volume independent of the pitched component. _(Inspired by: Melodyne 5)_ HIGH
- **[CL-126 / AQ]** Per-note fade in/out tool in BaySickPitch — fade individual notes within polyphonic content. _(Inspired by: Melodyne 5)_ HIGH
- **[CL-127 / AQ]** Leveling macro in BaySickPitch — two-knob "make quiet notes louder + make loud notes quieter" per-note amplitude leveler. _(Inspired by: Melodyne 5 Leveling Macro)_ HIGH

### Vocal workflow polish (mixed buckets)

- **[CL-128 / UT]** Audiolens-style vocal tone match — drag any reference vocal track, separate the vocal stem, seed BaySickVocal chain with starting point matching reference's spectral character. _(Inspired by: iZotope Nectar 4 + Audiolens)_ HIGH
- **[CL-129 / UT]** One-knob vocal assistant — "produce my vocal" panel that ducks complexity behind guided UI. **Bucket: User Tools / Learning** (audience fit). _(Inspired by: iZotope Nectar 4 Vocal Assistant)_ HIGH
- **[CL-130 / AQ]** ARA2 / direct-DAW integration plumbing — BaySick is standalone-only so ARA itself is N/A; underlying offline-region-edit-without-render-then-reimport plumbing maps to in-app non-destructive offline-edit on Inst/Vox audio clips. **Bucket: Cross-cutting Infrastructure**. _(Inspired by: Auto-Tune Pro 11 ARA2)_ HIGH
- **[CL-131 / AQ]** Articulator / formant-extraction module — extract formant + amplitude envelope from one vocal, apply to another audio source (vocal -> synth bridge). _(Inspired by: Antares ARTICULATOR Evo)_ HIGH
- **[CL-132 / WP]** Vocal preset chain library — genre-tagged starter chains (Pop Lead / Hip-Hop Lead / Acoustic Folk / Rock Wall / Lo-Fi). Subset of CL-042 mastering-chain templates. _(Inspired by: Waves CLA Vocals + LANDR)_ HIGH

---

# Sweep 2 — Effects / Saturation Modules (CL-133..CL-154)

**Bucket assignment:** all Effects unless noted.

## Already-have (skipped)

Decapitator A/E/N/T/P style emulations (FSW-147, FSW-150 + BLU-161/163/165), Saturn 2 multiband (BLU-172, CL-016, CL-017), VTM/J37/Kramer wow/flutter (BLU-185..192), ATR-102/Studer multi-head (BLU-197), Kramer/J37 noise + dropouts + print-through (BLU-198/199), VCC/NLS console (CL-021), A800 multitrack tape with crosstalk (CL-020), Decimort 2 bitcrusher (BLU-280), Wavefolder (BLU-281), Exciter (BLU-282 + CL-026), Asymmetric tube (BLU-169), Pre/de-emphasis (BLU-170), Multi-stage cascade (BLU-171), Tube/Console/Tape/Fuzz presets (BLU-173), IR cassette profile (BLU-194), Type II/IV cassette (BLU-195), Per-stage isolation (BLU-196), Cab IR (BLU-129/286, CL-015), Auto-ceiling true-peak (BLU-108), KClip 3 multiband clipper (BLU-107), FATSO Maximizer (BLU-279).

## Proposed additions

- **[CL-133 / AQ]** Punish / Overdrive switch on SaturationDSP — momentary +18 dB pre-shaper boost, mirrors Decapitator "Punish" + Vulture "Overdrive". _(Inspired by: Soundtoys Decapitator https://www.soundtoys.com/product/decapitator/, Thermionic Culture Vulture)_ HIGH
- **[CL-134 / AQ]** Bias control on SaturationDSP Tube path — adds a Bias rotary varying cathode-current bias point; backward rotation starves cathode for gating / signal-collapse artifacts. _(Inspired by: Thermionic Culture Vulture)_ HIGH
- **[CL-135 / AQ]** Triode + Pentode parallel-stage saturation mode — 4th SaturationType `Type::Parallel` running even-harmonic + odd-harmonic shapers in parallel with independent drive + freq selector (Broad/Lo/Hi). _(Inspired by: Black Box HG-2 https://blackboxanalog.com/hg-2/)_ HIGH
- **[CL-136 / AQ]** Variable Air control on SaturationDSP — high-shelf "silvery sparkle" boost above 10 kHz. _(Inspired by: Black Box HG-2)_ HIGH
- **[CL-137 / AQ]** "Subtle" intensity tier on Tube/Console/Tape Types — gentle low-intensity variants, scales transfer-curve gradient + dials harmonic generator amplitude back ~6-9 dB. _(Inspired by: FabFilter Saturn 2 https://www.fabfilter.com/products/saturn-2-multiband-distortion-saturation-plug-in)_ HIGH
- **[CL-138 / AQ]** Foldback / Breakdown FX modes on SaturationDSP — `Type::Foldback` (waveform-folding metallic artifacts) + `Type::Breakdown` (signal-mutating buffer-fragment echoes). Distinct from BLU-281 standalone Wavefolder. _(Inspired by: FabFilter Saturn 2)_ HIGH
- **[CL-139 / AQ]** Adjustable crossover slope on multiband saturation — when BLU-172 lands, expose 6/12/24/48 dB/oct slopes per split point. _(Inspired by: FabFilter Saturn 2)_ HIGH
- **[CL-140 / AQ]** Linear-phase crossover mode on multiband saturation — `LinPhase` toggle swaps IIR splitter for IR-based splitter; reuse EQ8DSP linear-phase plumbing (BLU-268..271). _(Inspired by: FabFilter Saturn 2)_ HIGH
- **[CL-141 / AQ]** Per-band drive / mix / feedback / dynamics — when BLU-172 ships, each band gets Drive, Mix, Feedback, Dynamics, Tone, Output. _(Inspired by: FabFilter Saturn 2)_ HIGH
- **[CL-142 / AQ]** Drag-and-drop modulation matrix on saturation effects — 8-12 mod slots per saturation instance with sources (LFO/env follower/XY/MIDI CC) draggable to any param. _(Inspired by: FabFilter Saturn 2)_ HIGH
- **[CL-143 / AQ]** Custom waveshape draw editor on SaturationDSP — `Type::Custom` exposes draggable transfer-curve editor. _(Inspired by: iZotope Trash 2)_ HIGH
- **[CL-144 / AQ]** Vowel filter pre-stage on SaturationDSP — formant-bandpass front-end with A/E/I/O/U vowel shapes for talkbox-style colorization. _(Inspired by: iZotope Trash 2)_ HIGH
- **[CL-145 / AQ]** Variable Wow & Flutter depth on TapeDSP — separate %-depth knob (0/25/50/100%) scaling LFO modulation. 25% = pristine, 50% = average, 100% = poorly-maintained. _(Inspired by: Slate Digital VTM)_ HIGH
- **[CL-146 / AQ]** HF Bias control on TapeDSP — adjustable HF bias-oscillator level. Distinct from FSW-184 record-bias level. _(Inspired by: UAD Studer A800)_ HIGH
- **[CL-147 / AQ]** Tape calibration level switch on TapeDSP — Cal switch with +3/+6/+7.5/+9 dBu reference levels. _(Inspired by: UAD Studer A800 + Ampex ATR-102)_ HIGH
- **[CL-148 / AQ]** Group-shared settings across plugin instances — VTM-style "group" assignment where all instances on Group 1 share Calibration / Wow & Flutter / Bias settings. Up to 8 groups. **Bucket: Cross-cutting Infrastructure** (touches mixer + rack). _(Inspired by: Slate Digital VTM + VCC)_ HIGH
- **[CL-149 / AQ]** Mono Maker low-end mono-fold — when CL-021 Console emulation lands, expose `MonoMakerHz` knob (50-300 Hz fold to mono). _(Inspired by: Brainworx bx_saturator V2)_ HIGH
- **[CL-150 / AQ]** Multi-mic cab simulator with positionable mics — when BLU-129 lands, add 2-D draggable mic-position interface: pick speaker (4×12 cab presets) + mic (SM57/421/C414/U87) + drag across top-down cab + optional second mic with phase + pan + level. _(Inspired by: Neural DSP Tone King Imperial MKII)_ HIGH
- **[CL-151 / AQ]** Neural-network / deep-learning component-modeled saturation — opt-in `Engine::NCM` flag swaps analytical Tube/Console/Tape model for pre-trained NN model. Bundle 1-3 NCM models per Type as factory presets; allow user `.nam` / `.aida-x` files. **Strategic decision: V2+ feature** — major architecture change. _(Inspired by: Tone Empire TM700 V3, Neural Amp Modeler)_ HIGH
- **[CL-152 / AQ]** Distortion algorithm library expansion (60+ algos, Trash-style) — when SaturationDSP `Type` enum opens up, separate `Algorithm` selector inside each Type with 8-12 sub-algorithms (e.g., Tube > 12AX7/12AT7/6L6/EL34/6V6/KT88/EL84). 60+ total algos. Pairs with BLU-173 character voicing presets. _(Inspired by: iZotope Trash 2 + Soundtoys Decapitator)_ HIGH
- **[CL-153 / AQ]** Parallel SAT branch frequency selector — when CL-135 Triode+Pentode parallel lands, parallel branch gets 3-position freq selector (Broad / Lo-band / Hi-band). _(Inspired by: Black Box HG-2)_ HIGH
- **[CL-154 / AQ]** Per-style filter HPF/LPF on SaturationDSP — Culture-Vulture-style 9 kHz / 6 kHz fixed switchable filters fine-tuning distortion flavor by limiting saturated bandwidth. _(Inspired by: Thermionic Culture Vulture)_ HIGH

---

# Sweep 3 — Players / Synth Engines (CL-155..CL-171)

**Bucket assignment:** all Players unless noted. **Both structural gaps from previous (blocked) run confirmed real and current** → CL-155 (wave-sequencing) + CL-158 (modular patch-graph).

## Already-have (skipped)

Wavetable engine (Harmless 2048-pt IFFT + BaySickSynth 10 waveforms incl. Bell FM), sample player + SFZ + disk streaming + per-pitch preemption (BaySickPlayer/VibePlayer), subtractive + filter envelopes + LFO + mod XYZ pad (BaySickSynth + Harmless), 516-partial spectrogram (BLU-327), background wavetable rebuild (BLU-328), per-engine output volume, voicing modes (Poly N / Mono).

## In-plan (not double-added)

CL-001..008 (Wavetable, FM, Analog, Phase-distortion, Modal, Karplus, Vocoder, Talkbox), CL-009..013 (Pitched-sample, Reslicer, Granular FX, Spectral freezer, Drag-audio resynth), BLU-331 (Image resynthesis), BLU-333 (Spectral reorder), BLU-334 (9-voice unison), BLU-447 (VST3 Instrument Hosting).

## Proposed additions

### Net-new synth engines (Players bucket)

- **[CL-155 / AQ]** Wave-sequencing engine (`BaySickWaveSeq`) — multi-lane wave-sequencing in Wavestate-2.0 lineage: independent Lanes for Timing / Sample-step / Melody (each Lane has its own step count, start, end, loop point); modifier Lanes for Shape / Gate / Step-seq value. Distinct from CL-001 (continuous frame interpolation) — this is timeline-driven per-step transitions across heterogeneous samples + waveforms. **Strategic: V2+ feature** — major engine. _(Inspired by: Korg wavestate https://www.korg.com/us/products/synthesizers/wavestate/)_ HIGH
- **[CL-156 / AQ]** Vector synthesis engine (`BaySickVector`) — 4-corner mix between four sound sources (oscillators or layers) controlled by 2-D joystick + recordable, loopable mix envelope; integrates with CL-155 so each corner can host a wave-sequence. Distinct from existing Mod XYZ (destination-side modulation). **Strategic: V2+** — major engine. _(Inspired by: Korg Wavestation + wavestate)_ HIGH
- **[CL-157 / AQ]** Hybrid / multi-engine container (`BaySickFalcon` working name) — single tab with N (e.g. 4) layered oscillators, each layer independently chooses synthesis mode (analog/wavetable/FM/granular/sample/modal); shared filters + amp + global mod matrix; per-layer mod section. Distinct from current "one engine per tab" architecture. **Strategic: changes foundational arch** — V2+. _(Inspired by: UVI Falcon + Logic Alchemy + Pigments)_ HIGH
- **[CL-158 / AQ]** Modular patch-graph engine (`BaySickModular`) — user-patchable visual-cable instrument page with categorized module library (oscillators, filters, envelopes, LFOs, S&H, slew, logic, math, audio-rate mod, CV utilities). Re-uses existing `RoutingGraph` Kahn topo-sort. Polyphonic-mode flag per module (Phase Plant style). **Strategic: V2+** — major. _(Inspired by: Bitwig Poly Grid + Phase Plant + Reaktor Blocks)_ HIGH
- **[CL-159 / AQ]** Hybrid-resonator dual-modal engine (`BaySickResonator`) — two coupled physical-modeled resonators (string/beam/drumhead/membrane/plate/tube/bar) with bidirectional energy-flow coupling + excitation source (mallet/pluck/impulse/noise/mic input). Distinct from CL-005 (single resonator-bank). _(Inspired by: AAS Chromaphone 3)_ HIGH
- **[CL-160 / AQ]** Spectral oscillator (`BaySickSpectral`) — third oscillator option for CL-001 BaySickWavetable that operates on harmonic spectrum directly: stretch/shift/smear/skew/window-tilt/shimmer over FFT frame, with per-harmonic mute mask. Distinct from BLU-333 one-time reorder. _(Inspired by: Vital + Serum 2)_ HIGH

### Cross-cutting Player features (Players bucket)

- **[CL-161 / AQ]** Per-engine "Performer" timeline modulator — draw N-bar modulation curve, assign to any param. "Octave bank" lets user switch 8 patterns via pitched-key control octave (live-performance affordance). Distinct from existing mod editor (per-note 0..1 phase). _(Inspired by: Massive X Performer)_ HIGH
- **[CL-162 / AQ]** MTS-ESP + Scala microtuning support — load `.scl` / `.kbm` / `.tun` / MTS SysEx; auto-pick from any MTS-ESP master plugin. Applies globally. _(Inspired by: ODDSound MTS-ESP + Surge XT)_ HIGH
- **[CL-163 / AQ]** MPE input + per-voice expression routing — accept MPE channel-per-note pitch-bend / pressure / Y-axis (ROLI / Osmose / Linnstrument); per-voice route to filter / amp / pitch. _(Inspired by: Ableton Drift MPE + Phase Plant)_ HIGH
- **[CL-164 / AQ]** Circuit-level analog imperfections panel (extends CL-003) — per-voice voice-detune trimmer, slow drift, envelope sloppiness, oscillator zero-cross sync wobble; "Divine"-style accuracy mode toggle. _(Inspired by: u-he Diva Trimmers + Divine quality mode)_ HIGH
- **[CL-165 / UT]** Lua-style preset scripting hook — small embedded scripting layer where engine preset carries a script that processes incoming MIDI (humanize, arpeggiate, generate, transform) before voice allocation. Sandboxed (no FS / no graph mutation). **Bucket: User Tools / Learning**. _(Inspired by: UVI Falcon Lua scripts)_ HIGH
- **[CL-166 / AQ]** Per-note voicing edit (Pianoteq Pro-style) — per-note panel with unique volume/detune/attack/decay/overtone overrides on top of keyboard-scaling. Persists in patch XML keyed by MIDI note. _(Inspired by: Pianoteq Note Edit)_ HIGH
- **[CL-167 / AQ]** Composite morphing technique (CMT) cross-engine morph — two patches loaded as A/B; morph knob blends harmonic profile of A toward B (FFT-based for tonal content; envelope-aligned for amplitude). Harmonic-domain morph, not layered playback. _(Inspired by: Spectrasonics Omnisphere CMT)_ HIGH
- **[CL-168 / AQ]** Sample-builder pipeline for user multisamples — desktop-companion or in-app tool: folder of WAVs labeled by note + velocity + RR → packed multisample bundle (auto-detects roots/RR/velocity layers; writes SFZ + binary index). _(Inspired by: Korg Sample Builder + Kontakt mapping editor)_ HIGH
- **[CL-169 / AQ]** Engine "polyphonic mode" toggle on FX rack slots — option to run a rack slot per-voice instead of post-mix; voice-internal flanger/phaser/chorus stays coherent with source voice. **Strategic: major plumbing — flag for now**. _(Inspired by: Phase Plant polyphonic Snapins)_ HIGH

### Sample / sample-manipulation (Players bucket)

- **[CL-170 / AQ]** Granular grain controls overhaul — expose Output Portal-grade params on CL-011 + BLU-288: window-shape selector (Tukey/Hann/Gaussian/Rectangular/Triangular), grain density 1-30, grain length 0-N ms with 1 ms resolution, scale-locked pitch shift, humanize, tempo-synced grain delay 1/64t..8 bars. _(Inspired by: Output Portal + Serum 2 granular)_ HIGH
- **[CL-171 / WP]** Hardware controller mapping presets — auto-map common hardware synth/MIDI controller layouts (Oxygen / KOMPLETE Kontrol / Push / Launchpad / nanoKontrol) into player macro section. **Bucket: Workflow Polish**. _(Inspired by: Omnisphere hardware integration)_ HIGH

---

# Sweep 4 — Mixer / Mastering / Metering (CL-172..CL-192)

**Bucket assignment:** all Mixer / Routing unless noted.

## Already-have (skipped)

Per-band M/S equalization (BLU-251, LDT-071, LDT-073), Dynamic EQ (LDT-073, LDT-081, LDT-412), spectrum analyzer overlay (LDT-071, LDT-412), True-peak limiting (LimiterDSP shipped, UI deferred LDT-412), Per-strip routing graph + cable + cycle (5F-4b), Aux strips (5F-4b).

## In-plan (not double-added)

CL-033 (Atmos/binaural), CL-034 (HRTF crossfeed), CL-035 (LUFS per bus), CL-036 (True-peak master), CL-037 (Phase correlation), CL-038 (Goniometer), CL-039 (Spectrum-in-strip), CL-040 (Stem export), CL-041 (Reference A/B), CL-042 (Master chain templates), CL-043 (Dither), CL-044 (RTA master), CL-045 (Loudness norm), CL-046 (Auto-mix), CL-047 (Stem-from-audio) + LDT-428, CL-085 (Visual EQ matching) + BLU-255 + CL-031.

## Proposed additions

- **[CL-172 / AQ]** Per-bus integrated/short-term/momentary LUFS history strip — three-line scrolling history view (last 10 min) with target-line overlay + time-coded true-peak violation flags, one per bus. Distinct from CL-035 (single-instant readout). _(Inspired by: Nugen VisLM 2 https://nugenaudio.com/vislm/)_ HIGH
- **[CL-173 / AQ]** Multi-instance metering link — any insert/bus instance of spectrum/vectorscope/spectrogram receives audio data from up to 8 other strips simultaneously, single floating "bus stack" canvas. _(Inspired by: iZotope Insight 2 + Relay)_ HIGH
- **[CL-174 / AQ]** PSR / PLR meter on master — 3-second sliding peak-to-short-term ratio readout with target line (PSR > 8 recommended); integrated PLR for whole song. _(Inspired by: AES Engineering brief on PSR + Klangfreund)_ HIGH
- **[CL-175 / AQ]** Tonal Balance overlay on master — broad-view 4-region target curve with 30+ genre presets + capture target from reference WAV + blend two saved targets. Read-only display (no built-in EQ — relies on existing EQ8). _(Inspired by: iZotope Tonal Balance Control 3)_ HIGH
- **[CL-176 / AQ]** Loudness-matched A/B render compare — render-to-buffer current master, normalize playback against reference WAV at chosen target, click-free A/B at loop point with multiple loop sections markable. _(Inspired by: Mastering The Mix REFERENCE 3)_ HIGH
- **[CL-177 / AQ]** Streaming codec preview on master — re-encode master in real time through Spotify/YouTube/Apple/Tidal/SoundCloud codecs (CBR/VBR), feed decoded result to speakers. Codec smear + clipping audible before bounce. _(Inspired by: Nugen MasterCheck Pro)_ HIGH
- **[CL-178 / AQ]** EBU R128 conformance log + offline scan — XML/CSV log with timecode-stamped LUFS/LRA/true-peak violations against EBU R128 / ITU-R BS.1770-4 / ATSC A/85 + "scan project" command walking arrangement faster than real time. _(Inspired by: Nugen VisLM 2 ReMEM + AudioSuite scan)_ HIGH
- **[CL-179 / AQ]** K-System scale on master meter — selectable K-12/K-14/K-20 ballistic with target = 0 at scale anchor; 83 dBSPL monitor calibration callout in tooltip. _(Inspired by: Bob Katz K-System + Sonoris K-Meter)_ HIGH
- **[CL-180 / AQ]** Mono-down / channel-solo monitor toggles on master strip — momentary "sum to mono" + "left only" + "right only" + "mid only" + "side only" listening modes; default keyboard shortcut. _(Inspired by: Brainworx bx_digital V3 Mono Maker + bx_meter solo modes)_ HIGH
- **[CL-181 / AQ]** Master-chain A/B revision snapshots — store up to 8 named snapshots of entire master-bus rack (effects, params, EQ8 state, limiter state, sends) per project; visual diff of changed params. Distinct from CL-092 (project-level VCS). _(Inspired by: iZotope Ozone undo + Melda snapshots A/B/C/D)_ MEDIUM (synthesis from multiple vendor patterns)
- **[CL-182 / AQ]** Gain-reduction history overlay across master chain — single timeline view (last 30 s) stacking GR traces of every dynamics module on master bus, color-coded by slot. _(Inspired by: FabFilter Pro-L 2 dynamics graph + Pro-MB band graph)_ HIGH
- **[CL-183 / AQ]** Multiband limiter on master — 5-band crossover with phase-compensated filters; single peak-limiting mixer allocates inter-band attenuation by psychoacoustic priority. Extends LimiterDSP from full-band to multiband; complements CL-016. _(Inspired by: Waves L3-LL Multimaximizer PLMixer)_ HIGH
- **[CL-184 / AQ]** Per-strip Sound Field / mono compat indicator — tiny 2-LED widget per mixer strip: green when sum-to-mono preserves >=90% energy, yellow >=70%, red below. Sticky red flag if dipped during last bar. Lighter-weight per-strip companion to CL-038. _(Inspired by: iZotope Insight 2 Sound Field)_ HIGH
- **[CL-185 / AQ]** Spectral dynamics on EQ8 — extend Dynamic EQ bands so each band can switch from "whole-band" gain change to "spectral" mode (only triggers on specific frequencies inside the band exceeding threshold) with per-band density slider. One step beyond LDT-073 / LDT-081. _(Inspired by: FabFilter Pro-Q 4 Spectral Dynamics)_ HIGH
- **[CL-186 / AQ]** Headphone correction profile loader on master — drop-in plugin slot reading JSON / wav-IR profile file; ships with starter set for 30+ headphones + custom user-measured profiles. Distinct from CL-034 (HRTF spatialization) — this is frequency-response correction. _(Inspired by: Sonarworks SoundID Reference)_ HIGH
- **[CL-187 / AQ]** Inflator-style harmonic loudness module — psychoacoustic "loudness without dynamic-range loss": probabilistic resampling adding asymmetric harmonics to push perceived loudness without acting as compressor/limiter; two-band split. _(Inspired by: Sonnox Oxford Inflator)_ HIGH
- **[CL-188 / AQ]** Dolby Atmos Music Panner on tabs — per-tab object panner widget (X/Y/Z + elevation modes Manual/Wedge/Dome/Ceiling) writing positional metadata into project-level Atmos bed. Full Atmos render bus is CL-033; this is the per-tab UI feeding it. _(Inspired by: Dolby Atmos Music Panner)_ HIGH
- **[CL-189 / AQ]** Console TMT-style channel variance per insert — "warmth" / "channel variance" toggle on each insert strip selects one of N (e.g. 32) micro-tolerance variant profiles; off by default. _(Inspired by: Brainworx bx_console TMT)_ MEDIUM (TMT-per-insert + off-by-default is a BaySickDAW-tasteful adaptation, not a verbatim Brainworx feature)
- **[CL-190 / AQ]** Stem Focus per-effect-module mode — on any rack effect (and EQ8), "stem target" dropdown internally runs Master Rebalance and applies effect only to vocals/drums/bass/other within bus signal, then sums back. Builds on CL-047 underneath, moves workflow to effect-module level. _(Inspired by: iZotope Ozone 11 Advanced Stem Focus)_ HIGH
- **[CL-191 / AQ]** Master spectrum with intelligibility curve overlay — toggle on master spectrum overlays "speech intelligibility" target band (~500 Hz - 4 kHz) with three preset listening environments (low/medium/high noise). _(Inspired by: iZotope Insight 2 Intelligibility meter)_ MEDIUM (overlay-on-master-spectrum is a synthesis; Insight 2 has it as its own pane)
- **[CL-192 / AQ]** Master Assistant — analyze project's master bus over N-second pass, propose starting EQ8 + limiter + multiband chain targeted to genre profile (Pop / Rock / Hip-hop / EDM / Acoustic), each stage individually accept-or-discard. Distinct from CL-046 (per-track mix) and CL-042 (genre presets — this writes into them dynamically). _(Inspired by: iZotope Ozone 11 Master Assistant)_ HIGH

---

# Cross-sweep observations

## Top-3 strategic-impact items (V2+ candidates)

1. **CL-151 — Neural-network / NCM saturation engine.** Industry-trending (NAM ecosystem). Would change `SaturationDSP` architecture. Worth a discrete decision call with the user before approving.
2. **CL-155, CL-156, CL-157, CL-158 — new engine architectures (wave-sequencing, vector synthesis, multi-engine container, modular patch-graph).** Each is a major engine (3-6 months of work each). Approving all four implies a V2 with a substantially expanded synth roadmap; approving none implies V1.5 stays focused on what's already in plan. Worth deciding the scale of intent here.
3. **CL-169 — polyphonic FX rack slots.** Touches the 6-slot rack architecture — would require per-voice routing through inserts. Major plumbing.

## Cross-cutting items (re-bucket on apply)

- **CL-130** (ARA2 plumbing) → Cross-cutting Infrastructure
- **CL-148** (group-shared settings across plugin instances) → Cross-cutting Infrastructure (touches Mixer + Effects rack)
- **CL-129** (one-knob vocal assistant), **CL-165** (Lua scripting), **CL-128** (Audiolens-style tone match) → User Tools / Learning
- **CL-171** (hardware controller mapping presets), **CL-132** (vocal preset chain library) → Workflow Polish

## Likely fold-into-existing rather than new entries

These extend existing CL-* / BLU-* entries; consider folding into the parent entry rather than shipping as separate items:

- **CL-139, CL-140, CL-141** → fold into BLU-172 (multiband saturation T4)
- **CL-142** → fold into a new modulation matrix entry that doesn't yet exist (or BLU-326 Mod matrix Harmless S4 expansion)
- **CL-114, CL-115, CL-116, CL-117** → fold into CL-025 (chord harmonizer extension)
- **CL-122, CL-123** → fold into CL-027/CL-028/CL-029/CL-030 (restoration suite extension)
- **CL-125, CL-126, CL-127** → fold into CL-014 (BaySickPitch extensions)

## What was NOT found that I'd expect to find

(Listed across all 4 sweeps for re-run targeting if WebFetch is later enabled)

- LARM (Loudness And Range Meter) integration with Waves Dorrough — search returned nothing concrete
- Real-time loudness-matched mono-down per channel strip (not just master) — no vendor seems to ship this per-strip
- Phase-scope history (correlation over time as scrolling waveform) — found instantaneous readouts but not time-series
- Per-channel LUFS history in surround/Atmos — Insight 2 supports up to 7.1.2 but per-channel UI not verified
- Standalone "tape echo / dub-style tape delay" with saturation — partially BLU-510 + DelayDSP
- Sylenth2-specific feature deltas vs Sylenth1 — Sylenth2 sparsely documented in WebSearch
- u-he Hive / Repro / Bazille unique items — likely overlap with already-drafted entries (CL-161 Performer, CL-164 Diva trimmers)

## Unblock for next pass

To re-run with WebFetch enabled (deeper page reads vs WebSearch snippets), the project's `.claude/settings.local.json` needs an entry that grants WebFetch globally. Current allowlist has `WebSearch` (works) + `WebFetch(domain:*)` (denied — wildcard syntax may not be supported). Try:

- `"WebFetch"` (no parens) to allow all domains, OR
- Per-domain entries for each vendor host (high cost; ~80 entries needed)

If next-pass WebFetch is enabled, the 84 candidate entries here should be re-validated against canonical vendor doc pages rather than just search snippets, and the MEDIUM-confidence entries (CL-181, CL-189, CL-191) can be promoted to HIGH or dropped based on direct doc verification.

---

**End of report.** Triage + apply happens separately. When ready, send the YES list and I'll renumber any CL-IDs that get dropped to keep the final Future State sequence contiguous.
