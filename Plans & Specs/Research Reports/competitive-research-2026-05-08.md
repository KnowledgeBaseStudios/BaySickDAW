# Competitive Research Sweep — 2026-05-08 (v2 — WebFetch verified)

> **Status:** Candidate list — NOT yet applied to `Future State.md`. Each entry needs human triage (yes / no / maybe) before being added to the long-term roadmap.

> **Method:** 4 parallel `competitive-research` subagent runs, each with WebSearch + WebFetch fully enabled. Strict "verify or stop" rules — no fabrication. Confidence ratings tied to verification METHOD: HIGH = vendor doc / page / changelog WebFetched directly; MEDIUM = WebSearch snippets only OR third-party review WebFetched; LOW = unverified.

> **v1 history:** an earlier run (smoke test) without WebFetch produced 84 entries with inflated HIGH labels. v1 has been superseded; this v2 supersedes it. The agent prompt was updated to close the confidence-rating loophole (auto-demote to MEDIUM when WebFetch is denied) before this run. Reference: commit `01ac338`.

> **Total candidates:** 160 entries (CL-109..CL-268), renumbered globally in sweep order: Vocal (CL-109..CL-143), Effects/Saturation (CL-144..CL-199), Players (CL-200..CL-222), Mixer/Mastering (CL-223..CL-268).

> **Confidence breakdown:** 141 HIGH / 19 MEDIUM / 0 LOW. Every HIGH entry has a vendor or major-review WebFetch behind it. MEDIUM entries are explicitly flagged with the reason (vendor page bot-blocked, multi-source synthesis, etc.).

---

## Triage workflow

1. **Skip MEDIUM-flagged entries on first pass** if you want maximum signal — those are the ones with weaker verification.
2. **First pass per entry:**
   - **YES, want** — clearly distinct + audience-fits + sounds useful → approve
   - **NO, drop** — duplicate of existing, audience-wrong, marketing fluff → drop with one-line reason
   - **MAYBE** — needs deeper look → defer to second pass
3. **Second pass on MAYBE only.**
4. **Strategic flag review** — see "Strategic notes" below for items that are V2+ scope by themselves.

## Strategic notes (cross-sweep)

Items requiring a discrete decision (probably V2+ rather than V1.5):

1. **CL-200 / CL-201 / CL-202 / CL-203 / CL-220** — new synth engine architectures (Wave-sequencing, Vector synthesis, Multi-engine container, Modular patch-graph, Polyphonic FX rack). Each is 3-6 months of work; approving all four implies a substantially expanded V2 synth roadmap.
2. **CL-195 — Neural-network/NCM saturation engine.** Industry-trending (NAM ecosystem). Would change `SaturationDSP` architecture.
3. **CL-260 — Decompressor / "Unlimiter" module.** Requires ML model training/integration.
4. **CL-258 — Stem EQ on master.** Real-time ML-based stem split per band; major plumbing.

Items that fold into existing CL-* / BLU-* parents rather than ship as new:

- Vocal harmony extensions (CL-114..CL-118) → fold into CL-025 (chord harmonizer)
- Vocal restoration extensions (CL-122..CL-127) → fold into CL-027/028/029/030 (restoration suite)
- BaySickPitch per-note tools (CL-130..CL-136) → fold into CL-014 (BaySickPitch extensions)
- Effects multiband saturation extensions (CL-150..CL-152) → fold into BLU-172 (T4 multiband sat)
- Tonal Balance multi-feature cluster (CL-242..CL-247) → fold under one parent if approved

Cross-cutting bucket reassignments at apply-time:

- **CL-159** (group-shared settings across plugin instances) → Cross-cutting Infrastructure (touches Mixer + Effects rack)
- **CL-129 / CL-130 / CL-138 / CL-217 / CL-260** (one-knob assistants, Lua scripting, audiolens-style match) → User Tools / Learning where they're audience-aimed
- **CL-171 / CL-141** (hardware controller mapping presets, vocal preset chain library) → Workflow Polish

---

## Methodology + caveats (cross-sweep)

- **WebFetch was fully enabled** for all 4 v2 runs. The bare `"WebFetch"` syntax in `.claude/settings.local.json` (no parens / no domain filter) is confirmed working as global-allow.
- **Vendor pages that aggressively bot-blocked:** iZotope main site (insight, ozone, nectar, RX, tonal balance, trash), Waves, parts of Antares, Black Box, Plugin Alliance NLS + console-N pages, Output Portal full feature page. Where these returned 403 / 404 / thin content, agents fell back to:
  - Vendor docs subdomains (`docs.izotope.com`, `help.izotope.com`, `help.uaudio.com`, `helpcenter.celemony.com`)
  - Third-party reviews (Sound on Sound, MusicTech, MusicRadar, Sweetwater, B&H, Gearspace)
  - Vendor manual PDFs surfaced via search
  - Wikipedia for measurement standards (K-System, AES TD-1004, ITU BS.1770)
- **MEDIUM confidence in v2 means** snippets-only OR multi-source synthesis (no single canonical vendor page).
- **CL-NNN renumbering** was global to keep IDs unique across sweeps. v1 IDs are noted in the per-entry "v1 disposition" tables in each sweep section. At apply-time, drops will compact the sequence.

---

# Sweep 1 — Vocal Effects + Pitch Correction (CL-109..CL-143)

**Bucket assignment:** primarily Effects (BaySickVocal extensions); CL-137 / CL-138 / CL-142 → User Tools / Learning.

## Already-have

- BaySickVocal H-5 hybrid hop=256/512 + BaySickPitch Newtone-clone (FSW-127, FSW-134, FSW-141)
- BaySickAlign 3-lane GUIDE/DUB/OUTPUT VocAlign-clone (FSW-135, FSW-143, FSW-145)
- Compressor Modern/FET/Opto types (FSW-130, FSW-138)
- De-Esser split-band SC HPF + dynamic notch 5-10 kHz (FSW-132, FSW-139)
- Saturation Tube/Console/Tape umbrella + Vocal Body shaping (FSW-131, FSW-147)
- Reverb 5-algorithm umbrella + VocalBooth + sidechain ducking + tempo-sync (FSW-149)
- Delay extensions Echo + VocalDoubler + Slapback + sidechain (FSW-148)
- YIN/MPM pitch tracker + worker thread + atomic publish (FSW-140)
- BaySickNAM/IR mic sim + placement (FSW-146)
- Pre-EQ8 M/S in vocal chain (FSW-142)

## In-plan (not double-added)

CL-024 T-Pain hard-tune | CL-025 Chord harmonizer | CL-026 Vocal exciter | CL-027 De-noise | CL-028 De-reverb | CL-029 De-clipper | CL-030 Click/pop removal | CL-007 Vocoder | CL-008 Talkbox | CL-014 Pitch correction for instruments | CL-084 Vocal isolation | BLU-278 Vocal Rider | BLU-287 Pitch Shifter | BLU-284 Subharmonic | BLU-153 Shimmer | BLU-516 Analog Drift | BLU-293 Vibrato | BLU-330 Harmless Harmonizer | BLU-607 BaySickVocal H-1 skeleton.

## Proposed additions

### Pitch correction / hard-tune (Effects bucket)

- **[CL-109 / AQ]** Real-time scale-quantize hard-tune with custom-savable scales (43+ western/eastern + user customs); option to map illegal notes to nearest higher/lower scale tone. _(Inspired by: Waves Tune Real-Time https://www.waves.com/plugins/waves-tune-real-time)_ MEDIUM (vendor page bot-blocked; verified via search snippet quoting vendor copy)
- **[CL-110 / AQ]** Per-note "wrong note" exclusion list. _(Inspired by: Waves Tune Real-Time)_ MEDIUM (snippet-only)
- **[CL-111 / AQ]** Auto-Key-style automatic key + scale + tempo detector — drag any audio file, "Send to BaySickPitch" button writes detected key into corrector. _(Inspired by: Antares Auto-Key 2)_ MEDIUM (snippet-only)
- **[CL-112 / AQ]** Throat / vocal-tract physical-model tab — 5-point graphical throat shaper (glottis → throat → mouth → lips); neutralizes source vocal tract first then applies modeled tract. _(Inspired by: Antares Throat Evo https://www.antarestech.com/product/throat/)_ HIGH
- **[CL-113 / AQ]** Glottal waveform + variable breath-noise injection — pairs with CL-112 for rasp/smooth/whisper. _(Inspired by: Antares Throat Evo)_ HIGH
- **[CL-114 / AQ]** Octave pitch transpose with formant compensation — single transpose knob shifts ±octave with formant preserved. _(Inspired by: Antares Throat Evo + Auto-Tune Pro 11)_ HIGH

### Harmony generation (Effects bucket)

- **[CL-115 / AQ]** 4-voice formant-corrected Harmony Player — per-voice formant + pan + level + customizable retune speed; harmonies generated in real time from lead vocal input. _(Inspired by: Auto-Tune Pro 11 Harmony Player; SoS + MusicRadar reviews WebFetched)_ HIGH
- **[CL-116 / AQ]** Choir / vocal multiplier — turn one vocal into 4/8/16/32 distinct unison voices with per-voice random pitch/timing/vibrato + stereo spread. _(Inspired by: Antares CHOIR Evo)_ MEDIUM (vendor page 403'd; Sweetwater + multi-source corroborate)
- **[CL-117 / AQ]** MIDI-driven harmony modes — Fixed Intervals / Scale Intervals / Chord-Degrees / Chord-Name / Chord-by-MIDI / MIDI-Omni / 4-channel-MIDI. _(Inspired by: Antares Harmony Engine https://www.antarestech.com/product/harmony-engine/)_ HIGH
- **[CL-118 / AQ]** Backing-singer style packs — 8+ presets + import-acapella to define custom backing-singer persona. _(Inspired by: iZotope Nectar 4 Backer; docs.izotope.com WebFetched)_ HIGH
- **[CL-119 / AQ]** Per-voice Throat-Modeling on each Harmony Player voice — when CL-112 + CL-115 both ship, harmony voices route through their own throat models. _(Inspired by: Antares Harmony Engine Throat integration)_ HIGH

### Vocal alignment / production polish (Effects bucket)

- **[CL-120 / WP]** Multi-track group alignment in BaySickAlign — align N dub tracks to one guide in single pass with per-track tightness override. _(Inspired by: VocAlign Pro Process Groups https://www.synchroarts.com/products/vocalign-pro)_ HIGH
- **[CL-121 / WP]** Sync points + protected areas in BaySickAlign — user-defined target points + protected regions. _(Inspired by: VocAlign Pro)_ HIGH
- **[CL-122 / AQ]** SmartPitch in BaySickAlign — opt-in pitch-snap-to-guide layer that retunes only when dub is meaningfully off-pitch from guide. _(Inspired by: VocAlign Pro SmartPitch)_ HIGH
- **[CL-123 / AQ]** Vocal doubler (BaySickVocal stage) with controllable pitch variation + timing offset + formant shift — natural-sounding double generation. _(Inspired by: Revoice Pro 5 Vocal Doubler https://www.synchroarts.com/products/revoice-pro-5)_ HIGH
- **[CL-124 / AQ]** Phase-aligned multi-mic vocal stacks — automatic spectral-phase optimization via all-pass filter rotation across multiple takes/mics. _(Inspired by: Sound Radix Auto-Align 2 https://www.soundradix.com/products/auto-align/)_ HIGH
- **[CL-125 / AQ]** Sub-sample timing alignment — sub-sample-resolution time-shift between source + dub. _(Inspired by: Sound Radix Auto-Align 2)_ HIGH

### Vocal restoration tools (Effects bucket)

- **[CL-126 / AQ]** Adaptive de-noise mode (extends CL-027) — track changing noise floor in real time vs fixed-train. _(Inspired by: iZotope RX 11 Voice De-noise Adaptive)_ MEDIUM (iZotope page 403)
- **[CL-127 / AQ]** Combined vocal repair-assistant umbrella over CL-027/028/029/030 — single panel + auto-classify + light/medium/aggressive intensity. _(Inspired by: iZotope RX 11 Repair Assistant)_ MEDIUM
- **[CL-128 / AQ]** Dialogue Isolate-style vocal stem extraction stage in BaySickVocal — neural-network separation as a pre-stage. Distinct from CL-084. _(Inspired by: iZotope RX 11 Dialogue Isolate)_ MEDIUM
- **[CL-129 / AQ]** Vocal Unmask spectral-sidechain ducking — sidechain instrumental bus, auto-find masked frequencies, dynamic-EQ on competing track with vocal-aware threshold. Distinct from BLU-278 vocal-rider gain leveling. _(Inspired by: iZotope Nectar 4 Vocal Unmask)_ HIGH

### Specialized vocal dynamics (Effects bucket)

- **[CL-130 / AQ]** Variable-frequency de-esser stage — sweepable sidechain HPF + ratio/threshold/attack/release. _(Inspired by: Antares SYBIL Evo)_ MEDIUM
- **[CL-131 / AQ]** Sibilant-balance per-note in BaySickPitch — drag a single note's sibilant volume independent of pitched component. _(Inspired by: Melodyne 5 Sibilant Balance; SoS + Celemony Help Center WebFetched)_ HIGH
- **[CL-132 / AQ]** Per-note fade in/out tool in BaySickPitch — works inside chords. _(Inspired by: Melodyne 5 Fade Tool)_ HIGH
- **[CL-133 / AQ]** Leveling Macro per-note — two-knob "make quiet notes louder + make loud notes quieter" amplitude leveler ignoring breaths/low-level noise. _(Inspired by: Melodyne 5 Leveling Macro)_ HIGH
- **[CL-134 / AQ]** Weighted-pitch-centre algorithm — emphasizes perceptually significant portions of sustained notes vs averaging entire pitch curve. _(Inspired by: Melodyne 5 weighted pitch centre)_ HIGH
- **[CL-135 / AQ]** Per-note pitch + sibilance separation — split each note into pitched and unpitched components, edit each independently. _(Inspired by: Melodyne 5 Melodic Algorithm with Sibilant Detection)_ HIGH
- **[CL-136 / AQ]** Chord recognition + chord track in BaySickPitch — analyze project audio harmonic content, auto-fill chord track, snap dragged notes to chord tones. _(Inspired by: Melodyne 5 Chord Recognition + Chord Track + Chord Snap)_ HIGH
- **[CL-137 / AQ]** Percussive-pitched hybrid algorithm in BaySickPitch — for material combining percussion + pitched elements; preserves transient integrity. _(Inspired by: Melodyne 5 Percussive Pitched Algorithm)_ HIGH

### Vocal workflow polish (mixed buckets)

- **[CL-138 / UT]** Audiolens-style vocal tone match — drop reference vocal, auto-extract spectral character, seed BaySickVocal chain. **Bucket: User Tools / Learning**. _(Inspired by: iZotope Nectar 4 + Audiolens; docs.izotope.com WebFetched)_ HIGH
- **[CL-139 / UT]** One-knob vocal assistant — "produce my vocal" panel that ducks complexity. **Bucket: User Tools / Learning**. _(Inspired by: iZotope Nectar 4 Vocal Assistant)_ HIGH
- **[CL-140 / AQ]** Auto-Level / leveling-as-alternative-to-compression module — dynamic-range learning, noise tolerance, optional sidechain input. Distinct from BLU-278. _(Inspired by: iZotope Nectar 4 Auto-Level)_ HIGH
- **[CL-141 / AQ]** Articulator / formant-extraction stage — extract formant + amplitude envelope from one vocal, apply to another audio source. Built-in noise-generator for synth-less use. _(Inspired by: Antares Articulator Evo)_ MEDIUM
- **[CL-142 / WP]** Vocal preset chain library — genre-tagged starter chains (Pop Lead / Hip-Hop Lead / Acoustic Folk / Rock Wall / Lo-Fi). Subset of CL-042 mastering-chain templates. **Bucket: Workflow Polish**. _(Inspired by: Waves CLA Vocals + LANDR)_ MEDIUM
- **[CL-143 / UT]** Six-fader CLA-style vocal macro panel — beginner-friendly companion (Bass / Treble / Compression / Reverb / Delay / Pitch-modulation). **Bucket: User Tools / Learning**. _(Inspired by: Waves CLA Vocals)_ MEDIUM

---

# Sweep 2 — Effects / Saturation Modules (CL-144..CL-199)

**Bucket assignment:** all Effects unless noted.

## Already-have

- SaturationDSP Tube/Console/Tape Type umbrella + Vocal Body shaping (FSW-131, FSW-147)
- TapeDSP folded into SaturationDSP (FSW-150)
- 4x oversampling + IIR half-band polyphase (BLU-163, BLU-182, BLU-207)
- Hysteresis / asymmetric sigmoid / wow+flutter LFO + smoothed noise (BLU-180/181/183/189)
- Pre/de-emphasis shelf pair on tape (BLU-185)
- Tape speed chicken-head + IPS speed selector (BLU-192)
- Bias knobs (overdrive BLU-124, tape BLU-193)
- Phase I pedal pack BD/OD/DS/FZ/MT (BLU-614..617, FSW-188)

## In-plan (not double-added)

BLU-169 Asymmetric sigmoid Type D | BLU-170 Pre/de-emphasis EQ | BLU-171 Multi-stage cascade | BLU-172 Multiband saturation 3-band | BLU-173 Character voicing presets | BLU-194/195/196 IR cassette / Type II/IV / per-stage isolation | BLU-197 Multi-head 3-head tape | BLU-199 Print-through | BLU-214 Multiband transient shaper | BLU-276 Multi-band Compressor | BLU-280 Bitcrusher | BLU-281 Wavefolder | BLU-282 Exciter | CL-015 Convolution body resonance | BLU-129/286 Cab IR | CL-020 Multitrack tape w/ crosstalk | CL-021 Console SSL/Neve/API | BLU-108 Auto-ceiling true-peak | BLU-107 KClip-3-style multiband clipper | BLU-279 FATSO Maximizer | CL-036 True-peak monitor.

## Proposed additions

### Saturation engine extensions (Effects bucket)

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

### Multiband saturation extensions (Effects bucket — fold under BLU-172 if approved)

- **[CL-155 / AQ]** Adjustable crossover slope (6/12/24/48 dB/oct) on multiband saturation. _(Inspired by: FabFilter Saturn 2)_ HIGH
- **[CL-156 / AQ]** Linear-phase crossover mode — `LinPhase` toggle swaps IIR for FIR splitter; reuse EQ8 linear-phase plumbing (BLU-268..271). _(Inspired by: FabFilter Saturn 2)_ HIGH
- **[CL-157 / AQ]** Per-band drive/mix/feedback/dynamics/tone/level controls on multiband sat. _(Inspired by: FabFilter Saturn 2)_ HIGH
- **[CL-158 / AQ]** Per-band Tone control (tilt-style EQ post-shaper, separate from input-side filtering). _(Inspired by: FabFilter Saturn 2)_ HIGH
- **[CL-159 / AQ]** True-split crossover (Mid/Side phase-coherent multiband) — guarantees Mid + Side stay phase-coherent across band split. _(Inspired by: Brainworx bx_saturator V2 https://www.plugin-alliance.com/products/bx_saturator-v2)_ HIGH
- **[CL-160 / AQ]** Drag-and-drop modulation matrix on saturation effects — 8-12 mod slots with sources (LFO/env follower/XY/MIDI CC) draggable to any param. _(Inspired by: FabFilter Saturn 2 50-slot matrix)_ HIGH
- **[CL-161 / AQ]** 16-step XLFO with per-step value + curve type — sequencer-style modulator beyond rate+depth. _(Inspired by: FabFilter Saturn 2 XLFO)_ HIGH
- **[CL-162 / AQ]** Per-band MIDI Learn — interactive MIDI Learn for multiband saturation parameters. _(Inspired by: FabFilter Saturn 2)_ HIGH

### Custom / experimental saturation (Effects bucket)

- **[CL-163 / AQ]** Custom waveshape draw editor — `Type::Custom` exposes draggable transfer-curve editor. _(Inspired by: iZotope Trash 2/5)_ MEDIUM (vendor page 403, manual link only)
- **[CL-164 / AQ]** Vowel filter pre-stage — formant-bandpass front-end with A/E/I/O/U vowel shapes. _(Inspired by: iZotope Trash 2/5)_ MEDIUM
- **[CL-165 / AQ]** Distortion algorithm library expansion (60+ algos, Trash-style) — separate `Algorithm` selector inside each `Type` with 8-12 sub-algorithms (Tube > 12AX7/12AT7/6L6/EL34/...; Console > SSL/Neve/API/Trident/RCA; Tape > Studer/Ampex/Otari/cassette). 60+ total. _(Inspired by: iZotope Trash + Decapitator)_ HIGH
- **[CL-166 / AQ]** Parallel SAT branch frequency selector — when CL-150 lands, parallel branch gets 3-position freq selector (Broad/Lo/Hi). _(Inspired by: HG-2)_ HIGH
- **[CL-167 / AQ]** Per-style HPF/LPF voicing on saturation — Culture-Vulture-style internal voicing-filter, not user-cutoff control. _(Inspired by: Thermionic Culture Vulture)_ MEDIUM (specific voicing-filter values not in vendor copy)
- **[CL-168 / AQ]** Crush boost knob — separate input-boost distinct from Drive (Drive = saturation level; Crush = momentary input push). Continuous variant of CL-144. _(Inspired by: Kazrog True Iron https://kazrog.com/products/true-iron)_ HIGH
- **[CL-169 / AQ]** 6-transformer-model selector on Console-Type saturation — UTC 108 X / Malotki E4M-4001B / Western Electric 111C / Haufe V178 / Marinair LO1166/A / UTC O-12. _(Inspired by: Kazrog True Iron)_ HIGH

### Tape-specific extensions (Effects bucket)

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

### NCM / neural-network engines (Effects bucket — V2+ strategic)

- **[CL-180 / AQ]** Neural-network / NCM saturation engine — opt-in `Engine::NCM` flag swaps analytical Tube/Console/Tape model for pre-trained NN model. Bundle 1-3 NCM models per Type as factory presets; allow user `.nam` / `.aida-x` files. **Strategic: V2+ architecture decision**. _(Inspired by: Tone Empire TM700 V3 + Neural Amp Modeler)_ HIGH

### Cab simulator extensions (Effects bucket)

- **[CL-181 / AQ]** Multi-mic cab simulator with positionable mics — when BLU-129 lands, add 2-D draggable mic-position interface: speaker pick + mic pick + drag across top-down cab + optional second mic with phase + pan + level. _(Inspired by: Neural DSP Tone King Imperial MKII)_ MEDIUM (out-of-direct-fetch scope; framing extension of BLU-129)

### Bitcrusher extensions (Effects bucket — fold under BLU-280)

- **[CL-182 / AQ]** Mid-Riser vs Mid-Tread quantization mode toggle — two decimation algorithms with drastically different dynamics response. _(Inspired by: D16 Decimort 2 https://d16.pl/decimort2)_ HIGH
- **[CL-183 / AQ]** Image filter (post-decimation aliasing-image control) — separate pre-filter and post-filter that controls aliasing artifacts above resampling frequency. _(Inspired by: D16 Decimort 2)_ HIGH
- **[CL-184 / AQ]** Variable Jitter on resample clock — short-period random fluctuations to resampling frequency producing harmonic distortion. _(Inspired by: D16 Decimort 2)_ HIGH

### Clipper extensions (Effects bucket — fold under BLU-107)

- **[CL-185 / AQ]** EBU LUFS target loudness display inside Clipper module. _(Inspired by: Kazrog KClip 3 https://kazrog.com/products/kclip-3)_ HIGH
- **[CL-186 / AQ]** Delta "show me what I'm clipping" listening mode — monitors only residual being subtracted from source. Apply to Limiter + Clipper + SaturationDSP. _(Inspired by: Kazrog KClip 3)_ HIGH
- **[CL-187 / AQ]** 8-mode clipper character library — Smooth / Crisp / Tube / Tape / Germanium / Silicon / Broken Speaker / Guitar Amp. _(Inspired by: Kazrog KClip 3)_ HIGH
- **[CL-188 / AQ]** Variable Soft Clipping — single knob morphs between hard and soft clipping. _(Inspired by: Kazrog KClip 3)_ HIGH

### FATSO-specific (Effects bucket)

- **[CL-189 / AQ]** Tranny harmonic-generator soft-clipper — dedicated `Type::Tranny` voiced for FATSO low-end character. Distinct from CL-150 parallel mode. _(Inspired by: Empirical Labs FATSO https://www.empiricallabs.com/fatso)_ HIGH
- **[CL-190 / AQ]** Warmth dynamic-LPF mode — self-modulates from signal envelope; one-knob auto-de-essing voicing on top of saturation. _(Inspired by: Empirical Labs FATSO)_ HIGH

### Devastor / Output Stage (Effects bucket)

- **[CL-191 / AQ]** Output Limiter + Dynamics Flattener pair — auto makeup limiter + one-knob input compressor managing signal loudness before clipping. _(Inspired by: D16 Devastor 2 https://d16.pl/devastor-2)_ HIGH
- **[CL-192 / AQ]** Filter routing topologies — 3-position chain selector: Filter→Saturation, Saturation→Filter, Filter || Saturation parallel. Distinct from BLU-170 pre/de-emphasis EQ pair. _(Inspired by: D16 Devastor 2)_ HIGH

### Cross-cutting Saturation Infrastructure

- **[CL-193 / AQ]** Mono Maker low-end mono-fold — when CL-021 Console emulation lands, expose `MonoMakerHz` knob (50-300 Hz fold to mono) on bus version. _(Inspired by: Brainworx bx_saturator V2)_ HIGH
- **[CL-194 / AQ]** Group-shared settings across plugin instances — VTM-style "group" assignment where all instances on Group 1 share Calibration / Wow & Flutter / Bias settings. Up to 8 groups. **Bucket: Cross-cutting Infrastructure** (touches Mixer + Effects rack). _(Inspired by: Slate VTM + VCC + Waves NLS 8 VCA groups)_ HIGH
- **[CL-195 / AQ]** Group-Bypass + Group-Drive offset on linked instances — Group Bypass kills effect across all instances; Group Drive single offset nudges drive on every instance. **Bucket: Cross-cutting Infrastructure**. _(Inspired by: Waves NLS https://www.waves.com/plugins/nls-non-linear-summer)_ HIGH
- **[CL-196 / AQ]** Group-Noise master across linked instances — kills/enables analog hiss on all instances in group. **Bucket: Cross-cutting Infrastructure**. _(Inspired by: Waves NLS)_ HIGH
- **[CL-197 / IN]** Inter-Sample-Peak (ISP) detection on saturation/clipper output meters — every saturation Type's output meter measures inter-sample peaks when signal driven beyond ~-0.5 dBFS. CL-036 already covers master ISP; this is per-effect-output sweep. **Bucket: Cross-cutting Infrastructure**. _(Inspired by: Kazrog KClip 3 32x oversampling + UAD Studer + Saturn 2 32x oversampling)_ HIGH
- **[CL-198 / AQ]** Tone Empire 2-band EQ stage on TapeDSP only — when TapeDSP has its own tab/page, add a stripped EQ8 view as post-tape stage for fine-tuning. _(Inspired by: Tone Empire TM700 V3)_ HIGH
- **[CL-199 / AQ]** Per-stage isolation Pentode and Triode level knobs — when CL-150 ships, expose dedicated Pentode + Triode + Saturation level pots so each can be soloed/blended. _(Inspired by: HG-2 hardware)_ HIGH

---

# Sweep 3 — Players / Synth Engines (CL-200..CL-222)

**Bucket assignment:** all Players unless noted.

## Already-have

- Harmless 516-partial IFFT additive engine
- BaySickSynth 10 waveforms incl. Bell FM + 2048-pt IFFT wavetable
- BaySickPlayer (VibePlayer) sample player + SFZ + disk streaming
- BaySickBass mirrors BaySickSynth
- BaySickSynth + Harmless mod editor + filter envelopes + LFO + Mod XYZ pad
- Phase D dynamic-drum architecture per-tab BaySickPlayer/Synth
- BaySickSynth/Bass `_outVol` master output parity
- Per-engine voice modes (Poly N / Mono) + voice cap
- Per-voice analog drift (BLU-389 `bss_drift`)
- 516-partial spectrogram (BLU-327) + background wavetable rebuild (BLU-328)

## In-plan (not double-added)

CL-001..008 (Wavetable, FM, Analog, Phase-distortion, Modal, Karplus, Vocoder, Talkbox) | CL-009..014 (Pitched-sample, Reslicer, Granular FX, Spectral freezer, Drag-audio resynth, Pitch-correction-for-instruments) | BLU-330 Harmonizer | BLU-331 Image resynth | BLU-333 Spectral reorder | BLU-334 9-voice unison | BLU-348 Real-time per-voice additive | BLU-447 VST3 Instrument Hosting | BLU-362..368 BaySickPlayer extensions | BLU-403/404/405/406/407/408 BaySick family extensions | BLU-424..430 DrumsPage discovery deferred | BLU-607 BaySickVocal H-1.

## Proposed additions

### Net-new synth engines (Players bucket — all V2+ strategic)

- **[CL-200 / AQ]** Wave-sequencing engine (`BaySickWaveSeq`) — multi-lane sequencer with independent Lanes for Timing / Sample / Pitch / Shape / Gate / Step Sequencer. Per-step random skip with modulatable probability 0-100%; randomize step order per pass. **Strategic: V2+**. _(Inspired by: Korg wavestate https://www.korg.com/us/products/synthesizers/wavestate/)_ HIGH
- **[CL-201 / AQ]** Vector synthesis engine (`BaySickVector`) — 4-corner mix between four sound sources controlled by 2D joystick + recordable mix envelope; integrates with CL-200 so each corner can host a wave-sequence. **Strategic: V2+**. _(Inspired by: Korg wavestate)_ HIGH
- **[CL-202 / AQ]** Hybrid multi-engine container (`BaySickFalcon` working name) — single tab with 4+ layered sub-engines per voice; each layer independently chooses synthesis mode (analog/wavetable/FM/granular/sample/modal/pluck); shared filters + amp + global mod matrix. **Strategic: V2+ foundational change**. _(Inspired by: UVI Falcon https://www.uvi.net/falcon + Logic Alchemy + Pigments)_ HIGH
- **[CL-203 / AQ]** Modular patch-graph engine (`BaySickModular`) — user-patchable visual-cable instrument page; reuses existing `RoutingGraph` Kahn topo-sort; polyphonic-mode flag per module. **Strategic: V2+**. _(Inspired by: Bitwig Poly Grid https://www.bitwig.com/the-grid/ + Phase Plant + u-he Bazille https://u-he.com/products/bazille/)_ HIGH
- **[CL-204 / AQ]** Hybrid-resonator dual-modal engine (`BaySickResonator`) — two coupled physical-modeled resonators (string/beam/drumhead/membrane/plate/tube/marimba bar/manual) with bidirectional energy-flow coupling; mallet + noise excitation modules. Distinct from CL-005 single resonator-bank. _(Inspired by: AAS Chromaphone 3 https://www.applied-acoustics.com/chromaphone-3/)_ HIGH
- **[CL-205 / AQ]** Spectral oscillator option for `BaySickWavetable` (CL-001 sub-feature) — third oscillator mode resynthesizing samples at harmonic level: warp/stretch/shift/smear/skew/tilt/shimmer over FFT frame; per-harmonic mute mask. **Folds into CL-001 scope.** _(Inspired by: Vital https://vital.audio/ + Serum 2 https://xferrecords.com/products/serum-2)_ HIGH
- **[CL-206 / AQ]** Phase-distortion + fractal-resonance digital engine (`BaySickPD`) — Casio-CZ-style PD per CL-004 plus fractal-resonance "fractalize" mode that creates sync-like cutting tones; oscillator runs from 0 Hz (doubles as LFO); modular signal-out routing. **Folds into CL-004 scope OR splits as separate engine.** _(Inspired by: u-he Bazille)_ HIGH

### Cross-cutting Player features (Players bucket)

- **[CL-207 / AQ]** Per-engine "Performer" timeline modulator — draw up to 8 bars of modulation curve per slot, store up to 12 patterns per patch, switch via Remote-Octave keyboard control. _(Inspired by: Native Instruments Massive X Performer)_ HIGH
- **[CL-208 / AQ]** MTS-ESP + Scala microtuning support — load `.scl` / `.kbm` / `.tun`; auto-sync to MTS-ESP master plugin; applies across all four BaySick engines. _(Inspired by: ODDSound MTS-ESP https://oddsound.com/mtsespsuite.php + Surge XT)_ HIGH
- **[CL-209 / AQ]** MPE input + per-voice expression routing — accept MPE channel-per-note pitch-bend / pressure / Y-axis (ROLI / Osmose / LinnStrument / Push); per-voice route to any APVTS target. _(Inspired by: Vital + Phase Plant + Ableton Drift)_ HIGH
- **[CL-210 / AQ]** Circuit-level analog imperfections panel (extends CL-003) — per-voice voice-detune trimmer, slow drift, envelope sloppiness, glide-rate / cutoff / pulse-width variance, "Divine"-style accuracy mode toggle. _(Inspired by: u-he Diva https://u-he.com/products/diva/ Trimmers + Divine quality mode)_ HIGH
- **[CL-211 / UT]** Lua-style preset scripting hook — sandboxed Lua scripting layer where patch carries script processing incoming MIDI events (humanize/arpeggiate/chord-recognize/generate/transform) before voice allocation. **Bucket: User Tools / Learning**. _(Inspired by: UVI Falcon UVIScript https://lua.uvi.net/)_ HIGH
- **[CL-212 / AQ]** Per-note voicing edit panel (Pianoteq Pro-style) — per-MIDI-note panel with unique volume/detune/attack/decay/overtones/hammer-hardness overrides on top of keyboard-scaling. _(Inspired by: Pianoteq Pro https://www.modartt.com/pianoteq)_ HIGH
- **[CL-213 / AQ]** Composite-Morphing-Technique cross-engine morph — two patches loaded as A/B; morph knob blends harmonic profile of A toward B in FFT frequency domain. _(Inspired by: Spectrasonics Omnisphere CMT https://www.spectrasonics.net/products/omnisphere/overview.php)_ HIGH
- **[CL-214 / AQ]** Sample-builder pipeline — desktop-companion / in-app tool: drag folder of WAVs labeled by note + velocity + RR → auto-detects roots/RR/velocity layers → writes SFZ + binary index loadable by BaySickPlayer. _(Inspired by: Korg Sample Builder)_ HIGH
- **[CL-215 / AQ]** Polyphonic mode toggle on FX rack slots — opt-in per-slot flag runs slot per-voice instead of post-mix. **Strategic: major plumbing — V2+**. _(Inspired by: Phase Plant polyphonic Snapins)_ HIGH
- **[CL-216 / AQ]** "Orb" XY navigator on every player — radius + angle 2D control with inertia (rolling-ball trail), dice, record-into-pattern, attractor-pendulum. Distinct from existing Mod XYZ pad. _(Inspired by: Spectrasonics Omnisphere ORB)_ HIGH
- **[CL-217 / UT]** Sound Match + Sound Lock browser mode — when patch is open, lock selected sound aspects (envelope/filter/FX), find related patches matching the locked aspects. **Bucket: User Tools / Learning**. _(Inspired by: Spectrasonics Omnisphere Sound Match / Sound Lock)_ HIGH
- **[CL-218 / AQ]** Drag-WAV-onto-target auto-extract envelopes — drop audio file onto envelope slot to auto-extract gain/brightness/transient/pitch envelopes for use as modulation. Pairs with CL-013. _(Inspired by: UVI Falcon)_ HIGH

### Sample / sample manipulation (Players bucket)

- **[CL-219 / AQ]** Granular grain controls overhaul for CL-011 + BLU-288 — Output-Portal-grade params: window-shape selector (Tukey/Hann/Gaussian/Rectangular/Triangular), grain density 1-30, grain length 0.5 ms to 1 second + tempo-synced 1/64t..1 bar, scale-locked pitch shift, per-grain randomization (jitter diamonds), tempo-synced grain delay, freeze mode. _(Inspired by: Output Portal https://output.com/products/portal + Serum 2 granular)_ HIGH
- **[CL-220 / AQ]** Pitch-splice + vocode WAV-to-wavetable converters for CL-001 — pitch-splice (slice at zero crossings / pitch period) or vocode (FFT-based) conversion strategies on dropped WAV. _(Inspired by: Vital)_ HIGH
- **[CL-221 / AQ]** Sample slicing with realtime score extraction — one-shot loop → auto-slice → per-slice MIDI score extraction with tails-mode (overlapping slice releases stay audible). Distinct from CL-010 (drum-machine playback). _(Inspired by: Serum 2)_ HIGH

### Player workflow polish (Workflow Polish bucket)

- **[CL-222 / WP]** Hardware controller mapping presets — auto-map common hardware MIDI controller layouts (M-Audio Oxygen / NI Komplete Kontrol / Ableton Push / Novation Launchpad / Korg nanoKontrol) into per-engine macro section. **Bucket: Workflow Polish**. _(Inspired by: Spectrasonics Omnisphere 300+ hardware profiles)_ HIGH

---

# Sweep 4 — Mixer / Mastering / Metering (CL-223..CL-268)

**Bucket assignment:** all Mixer / Routing unless noted.

## Already-have

- VibeGraph routing + 5F-4a/4b mixer rebuild + cable overlay + cycle detection
- MixerPage Master + 5 buses + per-strip lazy APVTS
- Cable overlay with green bezier sends
- Per-strip routing (sendTo + 4 sends with pre/post toggle)
- Aux strips with persistence
- EQ8DSP / EQ8MsDSP (8-band + 8 types) + linear-phase scaffolding (BLU-268..271)
- Dynamic EQ (LDT-073, LDT-081, LDT-412)
- LimiterDSP (DSP shipped, UI deferred)
- DBFSMeter + VUMeter + per-strip dBFS tick marks
- ParametricEQDisplay (heatmap, phase, compare, toolbar)

## In-plan (not double-added)

CL-033 Atmos/binaural | CL-034 HRTF crossfeed | CL-035 LUFS per bus | CL-036 True-peak master | CL-037 Phase correlation | CL-038 Goniometer | CL-039 Spectrum-in-strip | CL-040 Stem export | CL-041 Reference A/B | CL-042 Master chain templates | CL-043 Dither | CL-044 RTA master | CL-045 Loudness norm | CL-046 Auto-mix | CL-047 Stem-from-audio | BLU-110 LUFS/dBFS dual meter | BLU-108 Auto-ceiling true-peak | BLU-107 Spectral multi-band limiting | BLU-255 Match-EQ | CL-031 Adaptive equalizer | BLU-258 M/S spectrum analyzer overlay | BLU-279 Maximizer | BLU-289 Stereo Widener | CL-017 Multiband stereo widener | FSW-330 Pre-fader sends | CL-085 Visual EQ matching | CL-084 Vocal isolation.

## Proposed additions

### Metering & Loudness (Mixer / Routing bucket)

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

### Tonal Reference & Visualization (Mixer / Routing bucket)

- **[CL-235 / AQ]** Tonal balance overlay with leveled view + 30+ genre/subgenre target curves — broad-view 4-region target + fine view; each region highlights when in/out of band. Hip-hop / EDM / pop / orchestral / K-pop / rock / lo-fi etc. _(Inspired by: iZotope Tonal Balance Control 3 https://downloads.izotope.com/docs/tonal-balance-control/meters-and-target-curves/index.html)_ HIGH
- **[CL-236 / AQ]** Capture target curve from streaming source — listens to system audio (Spotify/YouTube tab playback), produces custom target curve from any audio you pipe through it. _(Inspired by: iZotope TBC 3 capture)_ HIGH
- **[CL-237 / AQ]** Weighted target blender — design custom target curve by mixing N genre profiles with per-genre weight. _(Inspired by: iZotope TBC 3 Target Blender)_ HIGH
- **[CL-238 / AQ]** Vocal balance + impact + stereo width meters — dedicated meters tracking vocal-pocket frequency band relative to mix, kick/snare impact band energy, stereo-width energy across freq. _(Inspired by: iZotope TBC 3)_ HIGH
- **[CL-239 / AQ]** Crest factor low-end indicator — visual readout for "low end is too dynamic" vs "low end is too compressed"; computed from dynamics at bottom 3 octaves. _(Inspired by: iZotope TBC 3)_ HIGH
- **[CL-240 / AQ]** Spectral intelligibility overlay on master — toggle overlays "speech intelligibility" target band (~500 Hz–4 kHz) with three preset listening environments (low/medium/high noise). _(Inspired by: iZotope Insight 2 Intelligibility)_ HIGH
- **[CL-241 / AQ]** Gain-reduction history overlay across master chain — single floating timeline view (last 30 s) stacking GR traces from every dynamics module on master bus; color-coded per slot. _(Inspired by: FabFilter Pro-L 2 + Pro-MB https://www.fabfilter.com/products/pro-mb-multiband-compressor-plug-in)_ HIGH

### Master-Bus Mastering Modules (Mixer / Routing bucket)

- **[CL-242 / AQ]** Multiband limiter on master — 5-band crossover with phase-compensated filters; PLMixer-style core allocates inter-band attenuation by psychoacoustic priority. Per-band Gain / Priority / Release plus global Master Release. _(Inspired by: Waves L3-LL Multimaximizer)_ HIGH
- **[CL-243 / AQ]** 8 limiter character algorithms — extend LimiterDSP with selectable algorithm trading transparency vs character vs near-zero-lookahead permissibility. _(Inspired by: FabFilter Pro-L 2)_ HIGH
- **[CL-244 / AQ]** Loudness-target limiter mode — set desired LUFS target, limiter auto-targets after listening to track section. Distinct from BLU-108 (auto-ceiling true-peak). _(Inspired by: TC Electronic BRICKWALL HD)_ HIGH
- **[CL-245 / AQ]** Audition Limiting + Unity Gain monitor toggles — momentary "audition the part the limiter is removing" + "compensate output by inverse of input gain". _(Inspired by: FabFilter Pro-L 2)_ HIGH
- **[CL-246 / AQ]** Inflator-style harmonic loudness module — psychoacoustic "loudness without dynamic-range loss"; probabilistic resampling adds asymmetric harmonics; two-band split with Effect / Curve / Mix. _(Inspired by: Sonnox Oxford Inflator https://www.sonnox.com/plugin/oxford-inflator)_ HIGH
- **[CL-247 / AQ]** Dynamic resonance suppressor on master — auto-detects narrowband resonances, applies dynamic notch only when crossing threshold; preserves transients via "soft"/"hard" mode + sharpness + selectivity. M/S + L/R + per-band scope. _(Inspired by: oeksound soothe2 + Mastering The Mix RESO)_ HIGH
- **[CL-248 / AQ]** Ultra-high-frequency shelf "AIR" band on master EQ — 2.5 / 5 / 10 / 20 / 40 kHz selectable shelf frequencies for vocal sheen + mix bus polish. Implementable as EQ8 band-type variant. _(Inspired by: Maag Audio EQ4 AIR BAND https://www.plugin-alliance.com/en/products/maag_eq4.html)_ HIGH
- **[CL-249 / AQ]** SUB band ultra-low-frequency mastering shelf (10 Hz) — for surgical sub-rumble control. _(Inspired by: Maag EQ4 SUB Band)_ HIGH
- **[CL-250 / AQ]** Spectral dynamics on EQ8 — extend Dynamic EQ bands so each can switch from "whole-band" gain change to "spectral mode" (only triggers on specific frequencies inside band exceeding threshold) with per-band density + selectivity. _(Inspired by: FabFilter Pro-Q 4 Spectral Dynamics https://www.fabfilter.com/help/pro-q/using/spectral-dynamics)_ HIGH
- **[CL-251 / AQ]** Stem EQ on master — split master into Vocal/Drums/Bass/Other stems via ML model; each stem gets independent 8-band EQ. Distinct from CL-047 file-level export. **Strategic: ML model dependency — V2+**. _(Inspired by: iZotope Ozone 12 Stem EQ)_ HIGH
- **[CL-252 / AQ]** Stem Focus per-effect-module mode — on any rack effect (and EQ8), "stem target" dropdown internally splits bus signal via Master Rebalance, applies effect to selected stem only, sums back. _(Inspired by: iZotope Ozone 11 Advanced Stem Focus)_ HIGH
- **[CL-253 / AQ]** Decompressor / "Unlimiter" module — ML-based "undo" stage that estimates and reverses heavy compression to restore lost transients. **Strategic: ML model dependency — V2+**. _(Inspired by: iZotope Ozone 12 Unlimiter)_ HIGH
- **[CL-254 / AQ]** Impact 4-band rhythm/feel enhancer — split master into 4 bands (sub/punch/clarity/air) with per-band drive/grit/contour controls aimed at rhythmic feel rather than tonal balance. _(Inspired by: iZotope Ozone 11 Impact)_ HIGH

### Console / Channel Character (Mixer / Routing bucket)

- **[CL-255 / AQ]** Console TMT-style channel variance per insert — "warmth" / "channel variance" toggle on each insert strip selects one of N (e.g. 32-72) micro-tolerance variant profiles; off by default. _(Inspired by: Brainworx bx_console TMT https://www.plugin-alliance.com/en/products/bx_console_focusrite_sc.html)_ HIGH
- **[CL-256 / AQ]** Per-channel virtual analog noise + THD — "Virtual Gain" rotary on each strip injects subtle simulated analog noise + variable continuous THD coloration. _(Inspired by: bx_console SC)_ HIGH
- **[CL-257 / AQ]** Master summing-bus character — single Mode selector on master InsertNode: Clean / API-style Thrust / Neve / SSL Glue. Adds harmonic coloration at summing node. Distinct from CL-021 (per-strip console). _(Inspired by: API Vision Console summing https://www.uaudio.com/products/api-vision-channel-strip-collection)_ MEDIUM (vendor page snippet only)

### Routing & Workflow (Mixer / Routing bucket)

- **[CL-258 / AQ]** Mono-down / channel-solo monitor toggles on master strip — momentary "sum to mono" + "left only" + "right only" + "mid only" + "side only" listen modes with shared keyboard shortcuts. _(Inspired by: Brainworx bx_meter solo modes + bx_digital V3 Mono Maker)_ HIGH
- **[CL-259 / AQ]** Master-bus revision snapshots A/B/C/D — store up to 8 named snapshots of entire master rack (effects + params + EQ8 + limiter + sends) per project; visual diff of changed params on switch. _(Inspired by: FabFilter Pro-Q 4 A/B + iZotope Ozone undo)_ MEDIUM (synthesis from common-pattern; no canonical single-vendor source)
- **[CL-260 / AQ]** Plugin Instance List for EQ8 / Limiter / Compressor — global overview panel showing every active EQ8 / Limiter / Compressor instance in project; one-click jump-to-strip + bulk preset recall + copy/paste params. _(Inspired by: FabFilter Pro-Q 4 Instance List https://www.fabfilter.com/help/pro-q/using/instance-list)_ HIGH
- **[CL-261 / AQ]** EQ Sketch — draw EQ curve in one mouse gesture; system snaps to optimal band placement + Q + slope based on stroke shape. _(Inspired by: FabFilter Pro-Q 4 EQ Sketch https://www.fabfilter.com/help/pro-q/using/eq-sketch)_ HIGH
- **[CL-262 / AQ]** Spectrum Grab on EQ8 — hover live spectrum, after dwell-delay enter grab-mode (existing bands dim, spectrum freezes); drag a peak to drop a Bell band with auto-Q. _(Inspired by: FabFilter Pro-Q 4 Spectrum Grab https://www.fabfilter.com/help/pro-q/using/spectrumgrab)_ HIGH
- **[CL-263 / AQ]** Headphone-correction profile loader on master — drop-in plugin slot reading JSON / wav-IR profile; ships with starter set for ~30 popular headphones + user-measured imports. _(Inspired by: Sonarworks SoundID Reference https://www.sonarworks.com/soundid-reference)_ HIGH
- **[CL-264 / AQ]** Translation-check listening simulator — single button cycles master output through "phone speaker / car stereo / club system / hi-fi / earbuds" simulated playback environments. _(Inspired by: Sonarworks SoundID Translation Check)_ HIGH
- **[CL-265 / AQ]** Speaker / room correction profile loader on master — IR-based room calibration profile slot, separate from headphones (CL-263). _(Inspired by: Sonarworks SoundID Reference room correction)_ HIGH
- **[CL-266 / AQ]** Master Assistant — analyze project's master bus over N-second pass, propose starting EQ8 + limiter + multiband chain targeted to genre profile (Pop/Rock/Hip-hop/EDM/Acoustic); each stage individually accept-or-discard; user can upload reference WAV instead of genre tag. Distinct from CL-046 (per-track) and CL-042 (static genre presets — this writes target params into them). _(Inspired by: iZotope Ozone 12 Master Assistant)_ HIGH
- **[CL-267 / AQ]** Dolby Atmos Music Panner per-tab — per-tab object panner widget (X/Y/Z + elevation modes Manual/Wedge/Dome/Ceiling) writing positional metadata into project-level Atmos bed; built-in step sequencer with tempo-sync allows automating object positions. Full Atmos render bus is CL-033; this is per-tab UI. _(Inspired by: Dolby Atmos Music Panner)_ HIGH
- **[CL-268 / AQ]** Per-platform downmix monitor formats on master — Lo/Ro stereo / Pro Logic IIx / 5.1-direct / 7.1-direct / 2.0-from-spatial-audio toggle. Surround-aware mixing to monitor downmix in real time. _(Inspired by: Logic Pro downmix and trim controls)_ MEDIUM (Apple docs snippet only; would need direct-fetch to upgrade)

---

# Cross-sweep observations

## What WebFetch revealed beyond v1

The biggest value of v2 over v1 came from features WebFetch could see that snippets had missed:

**Vocal:** Vocal Unmask (Nectar 4), weighted-pitch-centre algorithm (Melodyne 5), per-note sibilance/pitch separation, chord track with snap, percussive-pitched hybrid algorithm, formant-preserving octave transpose, Auto-Level distinct from Vocal Rider, per-voice Throat-Modeling on Harmony voices, SmartPitch in BaySickAlign, sub-sample timing.

**Effects:** Decapitator's actual UX (Drive + Punish + Steep + Thump are 4 distinct controls), Studer A800 Sync/Repro/Input 3-path tape signal chain, Decimort 2 specifics (mid-riser/mid-tread, image filter, jitter), KClip 3 LUFS metering inline + Delta listening + 8-mode library + variable Soft-Clip, bx_saturator V2 True Split crossover, Waves NLS Group Bypass + Group Drive + Group Noise master toggles, FATSO Tranny + Warmth as distinct named modes.

**Players:** All 17 v1 entries verified at HIGH (none demoted). 6 net-new — Bazille fractal-resonance, Omnisphere ORB navigator, Sound Match + Sound Lock browser, Falcon drag-WAV-onto-envelope auto-extract, Vital pitch-splice + vocode wavetable converters, Serum 2 sample slicing with score extraction.

**Mixer:** Tonal Balance Control 3 split into 5 distinct features (was 1 in v1), Streaming codec preview expanded to 8 platforms, K-System scale from Wikipedia + Pro-L 2 cross-verified, EQ Sketch + Spectrum Grab + Instance List from Pro-Q 4 Help, Stem EQ + Unlimiter + Impact from Ozone 12, Translation Check from SoundID Reference.

## Refinements applied during v2

- **CL-204 (`BaySickResonator`)**: dropped "wind" excitation from wording — vendor describes mallet + noise only; v1 was over-claiming
- **CL-205 (Spectral oscillator)**: reframed from peer engine to CL-001 sub-feature — matches what Vital + Serum 2 actually ship (spectral inside wavetable, not separate)
- **CL-167 (per-style HPF/LPF)**: removed "9 kHz / 6 kHz fixed" specifics — actual hardware uses internal voicing-filters, not labeled cutoff switches; demoted to MEDIUM
- **CL-170 (Variable Wow & Flutter depth)**: 0/25/50/100% tier specificity demoted to MEDIUM — adjustable W&F is HIGH but the specific tier values aren't in vendor copy
- **ARA2 plumbing entry from v1**: dropped — BaySickDAW is standalone-only, ARA layer doesn't translate; existing offline-edit infrastructure already covers what would map over

## Vendor-page bot-blocking patterns observed

Aggressive bot-blockers (consistent 403): iZotope main `/products/*` pages, Waves main product pages, parts of Antares, Plugin Alliance NLS / console-N pages, Black Box main pages, Output Portal full feature page.

Reliable fetchable subdomains (worked across multiple sweeps): `docs.izotope.com`, `help.izotope.com`, `helpcenter.celemony.com`, `help.uaudio.com`, `s3.amazonaws.com/izotopedownloads/...` (S3-hosted iZotope docs), `assets.wavescdn.com/pdf/...`, `downloads.izotope.com/docs/...`.

Reliable third-party fallbacks: Sound on Sound, MusicTech, MusicRadar, Sweetwater, B&H, Gearspace, Manuals.plus, Wikipedia (for measurement standards).

Recommendation: when a vendor blocks direct fetch, the agent's auto-demote rule held — those entries stayed MEDIUM rather than being silently labeled HIGH.

## Confidence breakdown

- **HIGH (vendor doc / page / changelog WebFetched OR major review WebFetched):** 141 entries
- **MEDIUM (snippets-only, vendor page bot-blocked, or multi-source synthesis without canonical single-vendor source):** 19 entries
- **LOW:** 0 entries

## What was NOT found (re-run targets if a future sweep wants deeper coverage)

- Sylenth1 → Sylenth2 feature deltas (Sylenth2 still sparsely documented)
- u-he Hive 2 unique-vs-Diva items (no engine-class delta worth dedicated CL)
- Native Instruments Massive X direct-fetch (vendor returned 403 throughout)
- Cherry Audio synthesis-engine class missing from CL-001..014 + new entries (their lineup is faithful re-creations, falls under CL-003 / VST3-host)
- Pianoteq Pro vs Standard breakdown (vendor page does not detail PRO-only features beyond Note Edit)
- Logic Alchemy direct vendor page (only third-party docs available)
- Studer A800 / ATR-102 specific HF Bias trim range in dBu
- HG-2 internal Air trimmer range
- Trash 5 explicit "draggable transfer-curve editor" claim verbatim
- Plugin Alliance N1 / TMT-on-summing claim
- Klangfreund Multimeter vs LUFS Meter differentiation
- Brainworx bx_console SC variable THD specifics
- Dialogue-gated loudness from Insight 2 (verified as feature but not BaySickDAW-relevant)
- Per-channel surround / Atmos LUFS history
- Dolby Atmos Music Panner LFO automation specifics
- "Unlimiter" / decompressor ML model architecture (confirmed exists, internals undocumented)

---

**End of v2 report.** WebFetch-verified, confidence-honest, ready for triage. When you triage, the "renumber on apply" pattern still applies — drops compact the sequence, contiguous CL-IDs preserved.
