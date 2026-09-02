# Factory Preset Data Audit — 2026-07 (QA-K Task 4 / DSP-01)

**Date:** 2026-07-18. **Scope:** every factory preset, every engine — 857 on-disk XMLs
under `Presets/` + the 36 code-defined `EffectPresetIO::factoryTable()` entries + 2
BaySickNAMIR library files. **Method:** four parallel read-only agent sweeps split by
preset format (BaySickSolstice / BaySickSynth+Bass+synth-format drums / BaySickPlayer+player
formats / Effects+pages), each reading the owning engine's `createLayout` + voice/DSP
source FIRST, bulk-extracting values by script, then hand-verifying every candidate flag
against source with file:line citations. Parent session spot-verified the systemic
seeding premise (`EffectPresetIO.cpp:728` skip-if-exists) and the Limiter floor
violation (`:611`) directly.

**Already fixed in QA-K:** `Presets/BaySickSolstice/Leads & Solos/Lasersaw.xml` — `amp_s`
0.0 -> 0.85 (dead-in-0.4s bug, marathon #9's named case).

**Deliverable contract:** report only. No preset data beyond Lasersaw was changed; no
code was changed. Fix routing = Jeff's call (dockets posed at the Task 4 gate).

---

## Spot-test priority (the ear list)

**HIGH — provably silent/near-silent (the real kills):**

1. `Presets/BaySickSynth/Plucks & Mallets/Tropical Pluck.xml` — HP filter pinned at
   20 kHz, no env/kb/vel/LFO opener, no post-filter path: near-silent whisper.
2. `Presets/BaySickDrums/606 Group/606 Closed Hat.xml` — same HP@20k pattern on a
   noise hat (working siblings use HP@7-9 kHz): hat effectively dead.
3. `Presets/BaySickDrums/Hand Percussion/Sleigh Bells.xml` — same pattern: hiss residue
   instead of bells.
4. `Presets/Effects/Reverb/Factory/Vocal Tame.xml` — stale seed predates the
   VocalBooth `algorithm` key; this machine plays the old hall engine while fresh
   installs get VocalBooth (divergent factory sound).

**MEDIUM — degraded or intent-deviating:**

5. `BaySickSynth/Cinematic & Drones/Shimmer Drone.xml` + `Pads & Atmospheres/Shimmer
   Pad.xml` — center osc silenced by the same HP@20k; only the detuned unison saws
   sound (hollow, quieter than authored).
6. `BaySickSolstice/Prog House/Lasersaw Stab.xml` — the EXACT pre-fix Lasersaw envelope
   signature (amp_s 0, amp_d 0.4, no pluck tilt). "Stab" name argues intent; numbers
   argue sibling-of-bug.
7. Transpose out-of-range (+/-36 vs +/-24 range; clamps a full octave off authored
   intent, 7 files): `BaySickSynth/Cinematic & Drones/Horror Squeak.xml` (+36),
   `Impact Hit.xml` (-36); `BaySickBass/Midtempo & Cyberpunk/Doom Saw.xml` (-36),
   `Sub Bass & 808s/Cinema Rumble Sub.xml` (-36); `BaySickDrums/606 Group/Master Sync
   Tick.xml`, `Yamaha Group/Shimmering Ride.xml`, `Yamaha Group/Synthetic
   Triangle.xml` (all +36).
8. DeEsser factory trio (`Vocal Light` / `Vocal Heavy` / `Sibilant Tame`) — stale
   pre-Task-8 format loads full-Wideband here vs 50% split-band blend on fresh
   installs (divergent de-ess character).
9. Reverb `70s Plate.xml` / `Cathedral.xml` — no `algorithm` pin: preset sound depends
   on whatever algorithm the slot was last in ("70s Plate" also isn't the Plate
   topology even in the code table).

**LOW — judgment calls / benign-but-noted (ear-test only if time allows):** BaySickSolstice
`Filtered Saw Bass` / `Boom-Bap 808` / `Bell Lead` / `Harpsichord Stab` (zero-sustain
patches whose names don't scream percussive); `BaySickDrums/808 Group/808 Claves.xml`
(~15 ms click-only body vs a real clave's ~40 ms ring).

---

## Per-engine detail

### BaySickSolstice — 152 scanned, 9 flags

Source facts the flags rest on: audible gain chain `(rm_env*envGain + (1-rm_env)) *
volume * trem` (AdditiveVoice.cpp:588-591) — amp_s=0 silences a held note because every
preset has rm_env=1.0. **pluck_decay is NOT a temporal decay source** — it is a static
per-partial spectral tilt at wavetable build (SpectralModules.h:86-99); it cannot rescue
a zero-sustain patch (premise correction vs the batch plan's Lasersaw note). Filter
types 0=LP/1=HP/2=BP/3=Notch (AdditiveVoice.h:192), serial (AdditiveVoice.cpp:544-569).
No preset hits total silence: volume=0.8, rm_vol=1.0, rm_env=1.0, partA_level=1.0 in
all 152.

| Preset | Param(s) | Flag | Source basis | Symptom | Conf |
|---|---|---|---|---|---|
| `Prog House/Lasersaw Stab.xml` | amp_s=0.0, amp_d=0.4, pluck_decay=0 | Exact pre-fix Lasersaw signature | AdditiveVoice.cpp:588-591 | Body gone ~0.4 s into held note | MED |
| `Synth-Pop/Filtered Saw Bass.xml` | amp_s=0.0, amp_d=0.3 | Zero sustain, non-percussive name | same | Held bass dies ~0.3 s (has flt env 0.4 — may be deliberate) | LOW |
| `Modern Hip-Hop/Boom-Bap 808.xml` | amp_s=0.0, amp_d=0.45 | 808 that cuts at ~0.45 s | same | Long 808 notes die mid-note | LOW |
| `Prog House/Bell Lead.xml` | amp_s=0.0, amp_d=0.5 | "Lead" name + zero sustain (matches bell-family convention) | same | Bell if intended, broken if lead | LOW |
| `Neo-Soul/Harpsichord Stab.xml` | amp_s=0, amp_d=0.18 | Shortest non-chip body, no pluck tilt | same | Click/blip, not a harpsichord | LOW |
| `Chiptune & 8-Bit/` x4 (8-Bit Lead, Bell Chip, Gameboy Pulse, Square Chip) | amp_r=0.0 (min 0.001) | Below declared range | BaySickSolsticeProcessor.cpp:168-180 | None — clamps to 1 ms (chip-authentic); data error only | HIGH/benign |

Verified-and-dropped (premises checked, intentional): `Psybient/Sub Hum` (LP@100 Hz sub
drone), `Psytrance/Hi-Pass Pad` (HP@1500 by name), `Wind Howl` (mask 1500 = dark wash),
the two trance-gate patches (trem_depth 1.0 IS the gate), the other 36 amp_s=0 patches
(all Pluck/Stab/Arp/Bell/Chip/Clav/Mallet-named). Informational: 4 layout params
(`cutSelfMode`, `lfo_vel`, `lfo_vol`, `lfo_pitch`) appear in NO preset (post-generator
additions; engine defaults apply — benign, uniform).

### BaySickSynth — 144 scanned / BaySickBass — 110 / BaySickDrums (all synth-format) — 172

Key mechanism for the HIGH flags: **StateVariableTPT highpass pinned at 20 kHz is a
brick wall** (the documented CLAUDE.md gotcha, here as preset data): SynthFilter.cpp:42
+ effCutoff modifiers all zero (BaySickSynthVoice.cpp:352-385) + whole voice runs
through the filter (:651-652). Post-filter escapes verified where present (transient
injector :691-712, unison stack :714-744).

| Preset | Param(s) | Flag | Symptom | Conf |
|---|---|---|---|---|
| `BaySickSynth/Plucks & Mallets/Tropical Pluck.xml` | HP@20000, no opener, unison=1 | Filter fully closed | Near-silent tinny whisper | HIGH |
| `BaySickDrums/606 Group/606 Closed Hat.xml` | noiseOnly + HP@20000 (siblings: HP@7-9k) | Filter fully closed | Hat effectively dead | HIGH |
| `BaySickDrums/Hand Percussion/Sleigh Bells.xml` | noiseOnly + HP@20000 | Filter fully closed | Hiss residue | HIGH |
| `BaySickSynth/Cinematic & Drones/Shimmer Drone.xml` | HP@20000, unison=4 | Center osc silenced; unison saws bypass filter | Hollow, center-less | MED |
| `BaySickSynth/Pads & Atmospheres/Shimmer Pad.xml` | HP@20000, unison=4 | same | same | MED |
| Transpose +/-36 x7 (list in priority item 7) | outside -24..24 | Clamps 1 octave off authored intent | Pitched wrong octave | MED |
| `BaySickDrums/808 Group/808 Claves.xml` | A=1ms, D=15ms, S=0 | Borderline click-only (~15 ms) | Thin tick vs 40 ms clave ring | LOW |
| amp attack/release 0.0 x4 files (Trance Gate Arp, 8-Bit Lead, BSS; Gameboy Pulse, BSB) | below 0.001 min | Clamps to 1 ms | None | LOW |
| `BaySickBass/My Bass Preset.xml` | — | User-saved preset at factory tree ROOT (belongs in My Presets/) | Browser pollution only | LOW |

Range summary: 21,301 values checked across the three trees; 11 violations total (7
transpose, 4 env-zero), all auto-clamped on load — no crash/NaN risk. All files
well-formed, correct roots, no unknown ids. `outVol`/`cutSelfMode` absent from every
generated file (defaults 0.8/false apply — benign). Verified non-flags: noiseOnly=1
with noise=0 falls back to full noise gain (BaySickSynthVoice.cpp:645-647); sustain-0
percussion with decay >= 0.03 s is by design.

**Duplicate files (BaySickDrums): 4 byte-identical pairs** shipped in BOTH `Hand
Percussion/` and `Tuned Percussion/`: `Cabasa Shaker`, `Rimshot Acoustic`, `Stick-Hit
Drum`, `Tambourine`. Root cause: the 2026-04-25 category move in
`Tools/gen_factory_presets.py` (~line 333) copied without deleting the originals.
Picker shows each sound twice.

### BaySickPlayer — 217 / Rusty Player — 2

**Cleanest family in the app.** All 217 `<Sample>` refs are `library:`-relative —
zero absolute paths — and all resolve on disk **case-exact** under the resolved root
(`%LOCALAPPDATA%\BaySickDAW\CoreLibrary`, per SampleLibrary.cpp:9-20). 75 referenced
SFZs all exist and parse with 0 missing `sample=` refs (default_path-resolved); 142
referenced WAVs all exist, none zero-byte. 6,727 param values, 0 range violations, no
zeroed volumes, no root-note hazards reachable (no folder-kind refs; file-kind roots at
60).

| Item | Problem | Severity |
|---|---|---|
| All 217 (systematic) | `cutSelfMode` absent from generator output; JUCE replaceState keeps the slot's prior value on load. Only bites if a user toggles the mode, loads a factory preset, re-enables Cut Self | Fact HIGH / severity LOW |
| `Rusty Player/My Presets/My Rusty Player.xml` + `My Rusty Player 2.xml` (user files) | Pre-J-11 format, no `<Program>` element — CCs apply to whichever program is active at load | MED |
| Source-side finding (not preset data) | Rusty player-preset save loop caps at cc<128 (BaySickRustyDrumsPage.cpp:391-393) while the layout registers 512 CCs — extended CCs (hi-hat macro CC400/401) never save into player presets | Code-fix candidate |
| `Presets/BaySickDrums/BaySick Kit 1.bsd` | Orphaned legacy kit blob (pre-Phase-D `BaySickDrumsState` inside a copyXmlToBinary wrapper) — unreachable from every current picker (kit loader wants plain-XML `BaySickKit`; walkers filter `*.xml` / audio) | Deletion candidate (Jeff's call) |

### Effects (39 on-disk) + EffectPresetIO (36 code-defined)

**Systemic finding:** `seedFactoryPresets()` skips any existing file
(EffectPresetIO.cpp:728 — parent-verified) and this repo IS the app's Documents tree,
so the on-disk factory XMLs are a frozen 2026-05-02 snapshot (`7bddbed2`) from a
pre-H-9-serializer binary. They never regenerate when the code table or serializers
change; fresh installs seed from current code and can sound DIFFERENT. Divergences
found:

| Item | Problem | Symptom | Conf |
|---|---|---|---|
| `Reverb/Factory/Vocal Tame.xml` | Missing `algorithm` key; code table sets VocalBooth(4) | Plays old hall engine here; VocalBooth on fresh installs | HIGH |
| `Reverb/Factory/70s Plate.xml`, `Cathedral.xml` | No algorithm pin (code table never calls setAlgorithm for them either) | Sound depends on slot's prior algorithm; "Plate" isn't plate topology | MED |
| `DeEsser/Factory/` trio | Stale pre-Task-8 format: loads mode=0 -> full Wideband; fresh installs get modeBlend=50 | Divergent de-ess character across installs | MED |
| Code table: Limiter "Brick Wall" `setReleaseMs(5.0f)` | Below the 10..1000 ms clamp (LimiterDSP.cpp:580-584; parent-verified at EffectPresetIO.cpp:611) | Behaves as 10 ms — barely audible; range violation is real | HIGH/low-audibility |
| Code table: Compressor "Drum Bus" `setKnee(4)` after `setType(FET)` | Dead assignment — FET forces hard knee at set + load (CompressorDSP.cpp:124-132, 904-908) | None (inert) | HIGH/inert |
| ~21 stale-format factory files (Compressor/Limiter/Overdrive/Flanger/Phaser/Saturation) | Newer keys absent BUT every load-path fallback verified to match current defaults key-by-key | None — verified benign | HIGH |
| `SaturationDSP.cpp:786-788` | Comment claims fresh default "IR on"; header default is false | Comment-only | HIGH |

No effect preset is silent/no-op/bypassed. On-disk range violations: 0. Chorus/Delay/
TransientShaper factory files match the code table value-for-value (incl. Slapback).

### Page presets (21) + BaySickNAMIR

**Premise correction:** `Presets/BaySickNAMIR/` is NOT empty — it holds the 2 library
files (`NAM/NPN-1.nam`, `IR/1960-V30.wav`; `MIC IR/` is an empty unused slot). All 9
NAM references across Inst/Vox presets resolve; all sfizz kit paths resolve; all engine
labels/types match their loaders; all 13 Inst `sourceMode` values valid; `Pedalboards/
Jeff 1.xml` fully valid (8 slots, types match inner state, tuner muted=0 — passes
audio).

| Item | Problem | Conf |
|---|---|---|
| `Guitars Page/My Presets/There's Always One.xml` | Orphaned folder: no PageKind maps to "Guitars Page" (Guitars is an Inst source mode now); zero source references — preset invisible to every load menu despite being valid | HIGH |
| `Clip Page/My Presets/My Clip.xml` | Sample ref lives inside a project folder (`...\Untitled Project (7)\Samples\...`) — resolves today, breaks if that project is deleted/renamed | LOW (fragility) |
| `Rusty Drums Page/My Presets/My Rusty Drums Setup.xml` | v1 format (back-compat shim handles it; preEq left as-is on load rather than restored) | LOW |

---

## Duplicate names

BaySickDrums: the 4 byte-identical pairs above. Everywhere else: none (cross-effect-type
name reuse like "Vocal Light" on Compressor + DeEsser is benign — separate menus).

## Clean bills

BaySickSolstice: 14 of 19 subfolders zero-flag. BaySickSynth: 5 subfolders. BaySickBass: 7.
BaySickDrums: 5. BaySickPlayer + Rusty Player: all references resolve, zero range
violations. Effects: Chorus/Delay/TransientShaper trees byte-match the code table. All
857 XMLs well-formed; zero malformed files anywhere.

## Code-fix candidates surfaced by the audit (routing = Jeff's call, per plan)

1. Rusty player-preset save CC cap (<128 vs 512 registered; extended hi-hat CCs never
   save) — BaySickRustyDrumsPage.cpp:391-393.
2. EffectPresetIO frozen-seed systemic (skip-if-exists + repo-is-Documents = stale
   factory effect presets never regenerate; needs a versioned-seed or force-regen
   decision) — EffectPresetIO.cpp:728.
3. Limiter "Brick Wall" `setReleaseMs(5.0f)` below floor — EffectPresetIO.cpp:611.
4. Reverb factory entries don't pin `algorithm` (Vocal Tame does in code but the stale
   file blocks it; 70s Plate/Cathedral never do) — EffectPresetIO.cpp:355-386.
5. `Tools/gen_factory_presets.py` category-move copied-without-delete (the 4 dup drum
   presets).
6. SaturationDSP.cpp:786-788 wrong comment (comment-only).
7. Orphan deletions: `BaySick Kit 1.bsd`, `Guitars Page/` folder, `BaySickBass/My Bass
   Preset.xml` at factory root, the two pre-J-11 Rusty Player user presets.

## Preset-DATA fix candidates (the flagged XMLs themselves; post-spot-test decision)

HP@20000 kills x3 (+2 Shimmer partials), transpose +/-36 x7, amp release/attack-zero
x8 (benign; tidy-only), Lasersaw Stab (pending ear verdict), dup-file deletions x4.
