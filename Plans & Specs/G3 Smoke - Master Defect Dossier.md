# G3 Boundary Smoke — Master Defect & Slide-Rework Dossier

> **Purpose.** Single source of truth for every defect surfaced by the G3
> boundary smoke, plus the full scope of the guitar/bass slide rework. This is
> the input to a fresh planning session (fable) that will REVIEW this against
> source and produce the batch plan(s). This document is NOT the plan — it is
> the compiled findings + locked decisions + context + rationale.
>
> **Provenance.** Compiled 2026-07-22 from: `Files For Claude/G3 Boundary
> Smoke.txt` (the 37-item smoke script), `Files For Claude/Smoke Test Issues.txt`
> (Jeff's defect report), six read-only source-diagnosis sweeps, one adversarially-
> verified feasibility workflow (dedicated-sfizz-instance evaluation), one
> external DAW-architecture research pass (slide crossfade gain matching), and one
> full control-surface inventory of both sfizz players.
>
> **State at compile time.** All uncommitted G3 boundary work sits at HEAD
> `d6abc38b`. NOTHING in this dossier has been coded. No builds run. The G3
> boundary commit has NOT happened.
>
> **Line numbers** are as-diagnosed and should be RE-VERIFIED during planning —
> they are anchors, not gospel.

---

## 0. How to read this

- Each defect carries: the smoke-item id(s) it came from, the ROOT CAUSE with
  file:line, the FIX SIZE, and any DECISION already locked with Jeff.
- **Decisions already locked are in §13. Do not re-litigate them** — surface only
  NEW spec calls the review turns up.
- The slide/bend rework (§2) is by far the largest piece; it is its own multi-
  strand build. Everything else (§3–§10) is comparatively contained.
- Standing project rules that govern all of this: `CLAUDE.md` + `Plans &
  Specs/Main Plan.md` §0. Bulk-run conventions apply. **Every multi-option spec
  call goes to Jeff in chat prose** (numbered options, no recommendation-forcing).

---

## 1. Defect index (nothing may be dropped)

| # | Cluster | One-line | Smoke item(s) |
|---|---|---|---|
| 1 | Slide | Pitch offset: guitar +3 / bass +2 semitones sharp | 14 |
| 2 | Slide | Too loud + wrong timbre + keyswitch ignored | 14, 15 |
| 3 | Slide | Velocity decoupled; sound from nowhere | 15 |
| 4 | Slide | Infinite ring-out (2+ bars) | 17 |
| 5 | Slide | Stop button ignored | General 2 |
| 6 | Slide | Zone-boundary attack thump | 18 |
| 7 | Slide | Adjacent slides fail (2nd plays plain note) | 18 & 19 |
| 8 | Slide | Bend notes do nothing | 21 |
| 9 | Roll | Porta marker orange/left/over-name | 6 |
| 10 | Roll | Bend has no marker + mislabels "Flat" | 21 |
| 11 | Roll | Pan instant, must ramp (in-house engines) | 11 |
| 12 | Roll | Click-copy drops velocity | General 1 |
| 13 | FL | Humanize start-time ref uses wrong divisions | 22 |
| 14 | FL | Humanize distribution: only Quasi-Normal | 22 |
| 15 | FL | Humanize seed: wrong range | 22 |
| 16 | FL | Humanize knob defaults wrong | 22 |
| 17 | FL | Riff Machine steps 4–7 inert | 22 |
| 18 | FL | Swing never built (global + per-instrument) | 22 |
| 19 | FL | Riff Levels knobs: no double-click-default | 22 |
| 20 | FL | Riff rework checkbox must auto-check | 22 |
| 21 | Tracks | Grouped tracks don't move together | 23 |
| 22 | Tracks | Moving past a group steps over one member | 23 |
| 23 | Tracks | Recolor not undoable | 23 |
| 24 | Builder | Blocks shouldn't loop at all | 24 |
| 25 | Builder | Drop length should match content | 24 |
| 26 | Builder | No click-copy on builder grid | 24 |
| 27 | Builder | URGENT: blocks locked to 1-bar snap | General 4 |
| 28 | Builder | URGENT: Alt+drag missing on builder grid | General 4 |
| 29 | Builder | Inst/Clips/Rusty ghost notes missing (NOT vox) | General 6 |
| 30 | Transport | Playhead misaligned (2 confirmed + 1 latent) | General 3 |
| 30b | Transport | First block doesn't play (dropped note-ons) | General 3 |
| 31 | Transport | Playhead visibility rules wrong | General 3 |
| 32 | Drums | Recorded MIDI invisible until reload | General 5 |
| 33 | Drums | Kick/snare swapped after reload | General 5 |
| 34 | Drums | Cross-row drag deletes the block | General 5 |
| 35 | Engine | Octave-up bell too hot | 34 |
| 36 | Engine | Overlapping base/slide voice reversal | 37 |
| 37 | Engine | RP loudness ramp dead in BaySickPlayer | (found in review) |

---

## 2. THE SLIDE & BEND REWORK (the big one)

### 2.1 Architecture decision + why

**Decision: keep the custom `SlideSampler` (formant-correct zone-hopping) and
give it the FULL patch voicing — reproduce the entire per-region voice DSP and
the entire user control surface inside our slide engine. Full fidelity, no
approximations, mono switch and wobble included.**

Two alternatives were evaluated and **rejected**, with source-verified reasons:

- **Single dedicated sfizz instance driven by the pitch wheel.** REJECTED.
  sfizz applies the pitch wheel as an *unclamped resample rate on the already-
  struck sample* (`libs/sfizz/src/sfizz/Voice.cpp:495` sets `pitchRatio_` at
  note-on; `:501` sets `pitchKeycenter_`; `:1102` multiplies the read rate by
  `centsFactor(bend)` every block). Region selection is a **note-on-only event**
  — the voice never re-selects a sample as pitch moves. So a continuous wheel
  bend is **exactly the single-sample stretch we built the SlideSampler to
  avoid**: formant-shifted (chipmunk up / muddy down). Formant correctness is a
  hard REQUIREMENT (real stringed instrument, not a synth), so this is
  disqualifying. Its "no thump" property is a *direct consequence* of not
  hopping samples — it trades the thump for the formant break.
- **A pool of sfizz instances, one per zone, crossfaded.** REJECTED (also by the
  original 2026-07-20 spike). sfizz gain is **global-only per instance**
  (`sfizz.hpp:390` `setVolume` is global; there is no per-voice host gain), so
  the only way to crossfade two instances is by their whole-instance gain —
  which crossfades ALL stacked layers of each = layer mud, and *reintroduces the
  thump*. Higher cost, worse sound.

**Why our zone-hop is correct:** it picks the sample at-or-just-below the moving
pitch and micro-bends UP ≤1 semitone (formants stay put), crossfading to the
next zone at each semitone boundary. That is what Orange Tree SLIDE (the one
commercial library doing real continuous fretboard slides) describes:
chromatically sampled, "timbre-corrected bending," explicitly rejecting single-
sample pitch-stretch. Our crossfade law is already mathematically correct equal-
power (`SlideSampler.cpp:252-260`: `g_out²+g_in² == 1`), which the SFZ spec
endorses for dissimilar material. **The architecture is right; what it's missing
is the voicing.**

**Why we can't just use sfizz for the slide:** sfizz can't hop samples (region
locked at note-on) and can't crossfade voices within an instance (global-only
gain). Both are required for formant-correct hopping. So the slide DSP has to be
ours.

### 2.2 Why the slide sounds wrong today (the gap is voicing, not samples)

Reading the real green-twang patch: the slide **already plays the correct
samples** (`maps_green/ord.sfz`, the main sustain map — exactly what
`SlideRegionMap` extracts). The tailpiece/feedback/unison/vibrato that a naive
read calls "missing layers" are **CC-gated and OFF by default**. What the slide
actually drops is the patch's **voicing**:

- **Gain staging** — global `volume=2`, `amp_veltrack`, per-region `amp_velcurve`.
  The SlideSampler's entire gain model is `jlimit(0.05,1.0, velocity/100)`
  (`SlideSampler.cpp:185`). → **too loud + velocity wrong (#2, #3).**
- **Amp envelope** — `env.sfz`: `ampeg_hold=0.3 / decay=1.5 / sustain=100`, plus
  global `ampeg_attack=0.01 / release=0.25`. The SlideSampler applies NONE →
  **infinite ring (#4)** and wrong landing dynamics.
- **Articulation** — extraction keeps only the `sw_default` articulation
  (`SlideRegionMap.cpp:126-128`: keep only where `sw_last == sw_default`), so the
  slide is always Twang regardless of the selected keyswitch. → **wrong timbre
  (#2).**
- **Bass-only:** an **always-on lowpass filter** (`filter.sfz`, `cutoff=250,
  fil_keytrack=70`) is baked into every bass note. Without it the bass slide is
  too bright. This is REQUIRED core voicing for bass even at default controls.

### 2.3 The voiced-SlideSampler build — full control surface

**Full-fidelity decision: reproduce ALL of the below.** Organized in tiers only
to show dependency order; all tiers are in scope. Same-CC-different-meaning
between guitar and bass means **per-engine CC mapping tables, not a shared CC
handler** (see traps below).

Region extraction (`SlideRegionMap`) must be extended to capture, per region:
`volume`, `amp_veltrack`, `amp_velcurve_*`, the full `ampeg_*` set, all `lfo*` /
`pitcheg` / `fileg` opcodes, filter opcodes (`cutoff`, `resonance`,
`fil_keytrack`, `cutoff_cc*`, `fileg_*`), `offset` / `offset_cc*`, `tune`,
unison opcodes, and the extra sample maps (tailpiece `*_tp`, `feedback_*`,
release `rel*`). Today it captures only wav + key/vel bands + offset/loop/tune +
bend range (`SlideRegionMap.h:31-73`).

**TIER 0 — makes a default slide sound correct + fixes every reported bug.**
- Velocity-band / round-robin selection + `amp_velcurve` gain shaping (G).
- Global `volume` trim (G).
- Amp envelope: attack / hold / decay / sustain / release (E). *(This is also the
  fix for #4 infinite ring — apply `ampeg_release`.)*
- Articulation / keyswitch selection — play the SELECTED articulation, not always
  the default. *(Fixes the keyswitch half of #2.)*
- **Bass:** always-on keytracked lowpass `cutoff=250, fil_keytrack=70` (F).

**TIER 1 — vibrato (pitch LFOs; rides the slide's existing continuous-pitch path).**
- Guitar: cc111 Guitar-vibrato, cc112 Violin-vibrato, cc113 Tailpiece-vibrato
  (per-note depth table in every `maps_green/*` region), cc114 Speed, cc116 Fade,
  cc117 Humanize (secondary wobble LFOs — INCLUDED per full-fidelity).
- Bass: cc21 Depth, cc112 Rate, cc115 Delay, cc116 Fade, cc117 Humanize.

**TIER 2 — dynamics + the bass filter section.**
- Guitar cc70 Mute (E: `env.sfz` `ampeg_hold/decay/sustain_oncc70`).
- Bass cc24 Swell (E+G), cc114 Tremolo (G: `lfo01_volume` amplitude LFO).
- **Bass filter section (a full filter DSP block):** cc90 Cutoff, cc91 Reso,
  cc92 Veltrack (vel→cutoff), cc113 Wobble (cutoff LFO), cc93 filter-env Depth,
  cc94 filter-env Attack, cc95 filter-env Decay.

**TIER 3 — unison + extra sample layers + remaining switches.**
- Unison: guitar cc100 (gate) / cc101 Width (pan) / cc102 Detune (±7 cents);
  bass cc100 / cc101 (bass unison voices additionally HP-filtered
  `fil2_type=hpf_1p cutoff2=250`). Spawn 2 extra detuned/panned voices.
- Extra sample layers: guitar cc27 Releases (`rel*` maps), cc29 Feedback
  (`feedback_pitched` + looped `feedback_noise`), cc118 TP-bends (switches main
  voice between `ord`/`hammer`/`stac` and their `*_tp` maps); bass cc107 Release
  (`darkblack_rel_map`), cc25 Preroll (`offset_oncc25=882`).
- **Bass cc105 Mono** — voice-allocation mode (poly maps vs mono maps with
  `group`/`off_by` choke). INCLUDED per full-fidelity (no half-assing), even
  though a single sustained slide voice makes its audible effect marginal.

**Guitar↔Bass traps (same CC#, different meaning) — must be per-engine:**
- CC113: guitar = tailpiece vibrato (pitch); bass = filter wobble (cutoff).
- CC114: guitar = vibrato speed (pitch); bass = tremolo (amplitude).
- CC112: guitar = violin-vibrato depth; bass = vibrato rate.

**Architectural deltas guitar vs bass:**
- Bass has an always-on lowpass + filter env + vel→cutoff + cutoff LFO. Guitar
  has NO filter at all (grep: zero `cutoff/fileg/resonance` in the guitar tree).
- Guitar layers feedback (pitched + looped noise) and tailpiece maps; bass has
  neither.
- Bass encodes dynamics as discrete `p/mp/mf/f` sample maps and duplicates every
  articulation into mono vs poly map sets; guitar uses `amp_velcurve` over shared
  samples.

**The landing falls out for free.** Because the voiced SlideSampler now plays a
fully-voiced note, when the slide arrives it simply keeps ringing in OUR engine
at the target pitch with correct voicing. No hand-off to sfizz, no seam. The only
ear-check risk is sliding into a note then playing that exact note *normally*
(sfizz) right after — the two render paths must match closely.

**Control routing:** UI controls are kit-authored, not hard-coded — `AriaControlPanel`
parses `GUI/<program>.xml`, and each widget's `param="N"` IS the sfizz CC number,
bound via APVTS `<prefix>cc<N>` → `mSfizz->cc()` (`AriaControlPanel.cpp:807-876`;
`BaySickGuitarsProcessor.cpp:46-55`). The slide path intercepts CC84/5/37/86/85
in `processBlock` and routes to `armSlide`→`SlideSampler`, bypassing sfizz
entirely (`BaySickGuitarsProcessor.cpp:283-297, 159-185`). So today every CC is a
no-op for the slide; the voiced sampler must consume the same APVTS CC values.
`outVol` (host output gain, `…Processor.cpp:417-418`) is the ONLY control that
already reaches the slide (post-mix).

### 2.4 SlideSampler internal bug fixes (independent of voicing)

- **#3 Velocity decoupled.** CC86 is emitted from the SLIDE note, not the anchor
  (`PluginProcessor.cpp:216-217`), and `armSlide` starts a voice whenever the
  anchor is *time-connected* regardless of whether it is audible
  (`BaySickGuitarsProcessor.cpp:159-184`, anchor found by
  `findRampAnchorPitch` which discards audibility). Fix: source velocity from the
  anchor note; don't start a voice under an inaudible anchor.
- **#4 Infinite ring-out.** `SlideSampler::release()` (`:223-230`) only stops
  steering pitch — no fade, no release envelope exists. Note-off DOES arrive
  (`BaySickGuitarsProcessor.cpp:311-318`). Fix folds into Tier-0 amp envelope
  (apply `ampeg_release`).
- **#5 Stop ignored.** All-notes-off reaches the engine and silences sfizz, but
  hands the SlideSampler `release()` (the no-op). SlideSampler has no
  `stopAllNow()`/`reset()` panic entry. Fix: add one; call from the all-notes-off
  branch of both processors.
- **#6 Zone thump.** Every voice gets the same `baseGain` (`:136`) but the
  outgoing voice has been decaying while the incoming starts 30 ms into a fresh
  pluck near its peak → each hop steps level UP. **Fix (from research): time-
  align the incoming voice's start offset to the outgoing voice's elapsed
  position** (one `int64` counter, zero per-sample cost — nulls the step by
  construction); raise the 30 ms offset floor toward ~120–150 ms; then shorten
  the crossfade from 45 ms toward ~25–30 ms. Backup if needed: a coarse per-
  sample RMS envelope table (~4 KB/sample, <1.5 MB total) computed at load for an
  exact gain ratio. HISE ships a purpose-built `GainMatchingMode` for the
  identical problem (precedent; GPL — technique only, not code).
- **#7 Adjacent slides fail.** Gesture identity is inferred from a bare MIDI note
  (`BaySickGuitarsProcessor.cpp:169-174`); a copied slide has the same anchor
  pitch → misread as a continuation → `startSlide` never runs (silence) and the
  sfizz base note is never suppressed (plain note heard). Fix: a monotonic
  gesture id on the transport (or require `SlideSampler.isActive()` before taking
  the continuation branch). Intermittency disappears once #4 is fixed.
- **Idle-suspend predicate** (`Source/Engine/Tasks/InstStripTask.cpp:161-199`)
  ignores `mSlideSampler.isActive()` → a ringing slide can be hard-truncated when
  sfizz goes idle. Add `isActive()` to the predicate.
- **Voice steal** (`SlideSampler.cpp:121-127`) falls back to the voice with the
  lowest `fade` — i.e. the one currently fading IN, the worst choice. Fix the
  steal heuristic.
- **`offset=` opcode dropped.** `SlideRegionMap` captures it (`:177-180`) but
  `Zone`/`setProgram` don't carry it (`SlideSampler.h:59-64`). Latent on these
  patches (offset 0); bites any patch that uses it. Fold into extraction.

### 2.5 sfizz pitch-wheel + emit fixes (fix normal notes too)

- **#1 Pitch offset / #8 Bend dead — the pitch-wheel convention bug.** sfizz's
  `pitchWheel(delay, pitch)` takes a **centered** value (−8192..+8192,
  `sfizz.hpp:550`; `normalizeBend = clamp(v,±8191)/8191`,
  `SfzHelpers.h:190`). All three sfizz processors pass JUCE's **raw 0..16383**
  instead (guitar `:326`, recenter `:321/:332` send 8192, bend ramp `:355-356`
  send `8192 + w*target`; bass `:317/:322/:328/:352`; Rusty `:220`). Neutral 8192
  → sfizz clamps to +1.0 = **full bend up**. Since `emitNoteExpression` emits a
  wheel on EVERY note (`PluginProcessor.cpp:61-63`), **every roll-scheduled
  guitar note is +3 semitones sharp, every bass note +2** (the patch bend range),
  and Bend is pinned full-up from the first block (no ramp, no shape). In-house
  voices do it correctly (`- 8192`). **Fix: send `jlimit(-8191, 8191, value -
  8192)` at all sites, all three engines.** Closes #1, #8, and the dominant
  offset term. Verified: guitar = clean +3, bass = clean +2, no residual.
- **#3 emit** — see §2.4 (velocity from anchor).

### 2.6 Piano-roll markers for slide types

Single paint site: `PianoRoll.cpp:2664-2695` in `PianoRollGrid::paint`.
- **#9 Porta marker.** Currently a LEFT-edge orange arc (`:2693`, the sole source
  of orange) that overlaps the note-name text (name at `x+3`, arc at `x+2`). Fix:
  a right-edge arrow like RP/RT, **white border + black fill**.
- **#10 Bend marker.** No paint branch exists (`Bend` falls through the
  RampSlide/RetrigSlide/Portamento chain). Add the same white-border/black-fill
  arrow. Also: `refreshNoteTypeButton()` (`:3964-3973`) has no `Bend` case → a
  Bend note labels the toolbar button "Flat"; and the `S`-key cycle (`:658-664`)
  never enters Bend. Fix all three.
- Restructure `:2664-2695` into one arrow block gated on `type != Standard`, per-
  type fill color. ~15 lines.

### 2.7 Research findings that inform the slide work

- **sfizz streams, it does not hold whole samples resident** — preload is a
  ~16 KB head per sample (`setPreloadSize(4096)`, mono/24-bit), streamed on
  demand, freed ~5 s after a voice stops. (Relevant only if an sfizz-instance
  approach ever resurfaces; we chose the voiced SlideSampler.)
- **Memory for the voiced SlideSampler:** only the ACTIVE articulation needs to
  be resident (~110 MB guitar / ~141 MB bass decoded float32, shared across tabs
  on the same patch via `SlideSampleCache` `SharedResourcePointer`). Other
  articulations decode on keyswitch change. Measured library facts: guitar green
  `ord` one-RR-all-bands = 141 files / 82.9 MB disk; bass darkblack `reg` = 168
  files / 106 MB disk.
- **The thump fix** (time-align + offset floor + shorter crossfade) is detailed in
  §2.4; full research write-up worth saving as `Plans & Specs/Research Reports/
  daw-architecture-slide-crossfade-gain-matching-2026-07-22.md` (drafted, not yet
  saved).

---

## 3. Piano roll defects

- **#11 Pan instant, must ramp (in-house engines only).** CC10 is broadcast to
  EVERY voice, active and idle (`BroadcastSynthesiser.h:27-33`), so a slide
  note's pan snaps the still-sounding base note too → the pop. `mNotePan` is also
  never reset at note-start, so a note inherits stale pan. **Required behavior:**
  on RP/RT slide types, pan ONLY (not cutoff/reso/release) ramps from current to
  target over the glide duration. **Precedent to copy: the S-6(C) velocity ramp**
  (`BaySickSynthVoice.cpp:279-284/309-313`, `AdditiveVoice.cpp:313-319`,
  `VibePlayerDSP.h:354-366`). Emit-ordering obstacle: the ramp must be armed in
  the CC10 handler before CC5/37 arrive, so either reorder the emit or add a
  dedicated pan-ramp CC (cleaner; naturally satisfies "pan only"). **Engines:**
  BaySickSynth + BaySickBass share a voice (one fix), Harmless (one fix),
  VibePlayer (one fix). **NOTE: withdrawn for BaySickGuitars/Basses — they have
  no per-note pan control reachable in their properties panel; CC10 emit is
  harmless there (panning stays at default center).**
  - **Incidental:** `VibePlayerDSP.cpp:1455-1456` CC whitelist omits CC86 — see
    #37.
- **#12 Click-copy drops velocity.** Click memory stores only duration + type
  (`PianoRoll.cpp:1855-1856` / `:1978-1979`); placement hard-codes velocity 0.8,
  pan 0, finePitch 0 (`:2287`). Fix: add `mClickMemoryVel`, capture at both
  sites, use at placement (one behavioral line). Full parity (also copy pan /
  cutoff / reso / release / porta / type) is a localized extension using a
  `PianoNote` prototype.

---

## 4. FL tools defects (Humanize / Riff Machine / Tracks live in `PianoRoll.cpp`)

Humanize panel: `PianoRoll.cpp:4076-4370`. Riff Machine: `:5218-5951`.

- **#13 Humanize start-time ref.** Currently walks the app snap table
  (`:4150-4155`, consumed via `snapDivToTicks` at `:4282`). **Decision: replace
  with a standalone 3-entry list 1/32, 1/64, 1/128; default 1/64.** 1/128 does not
  exist in the app snap table (bottoms at 1/6 Step), so this needs its own beats
  values, not a filtered view.
- **#14 Humanize distribution.** Combo has one item and NO `onChange`; the value
  is **never read** — the distribution is hardcoded (`:4290-4292`). **Decision:
  Uniform / Triangular / Quasi-Normal (that order), default Quasi-Normal.**
  Requires adding items + an `onChange` + a 3-way switch in the `qn` lambda
  (Triangular = `(a+b)*0.5`, Uniform = `rng.nextFloat()`; all three consumers at
  `:4300-4302` go through the one lambda).
- **#15 Humanize seed.** Currently a 0–99999 IncDec slider (`:4157-4163`, fed to
  RNG at `:4280`; Regenerate rolls `nextInt(100000)` at `:4190-4193`). **Decision:
  dropdown 1–10, default 1, NO "None" (beginner-friendly). Regenerate picks a
  value in 1–10.**
- **#16 Humanize knob defaults.** Constants at `:4252`
  (`kDefStartRange=20, kDefDurRange=0, kDefVelRange=10`). **Decision (from FL
  reference screenshots): Start Time Range 10% (0.100), Duration Range 10%
  (0.100), Velocity Range 20% (0.200), all three Offsets 0.** **Presentation
  decision: KEEP our two column headers ("Range"/"Offset") and percent display —
  do NOT switch to FL's per-control labels or 0.000–1.000 decimals.** (There is
  no control literally named "Distance"; FL's "Distance" maps to our Range
  column.)
- **#17 Riff Machine steps 4–7 inert.** NOT dead UI — the enable flags default to
  steps 1,2,3,8 only (`:5249`: `i==0||i==1||i==2||i==7`). The math for Mirror /
  Levels / Artic / Groove exists and runs (`:5740-5848`) but is gated
  (`:5622-5624`). Aggravators: Dice (`:5439-5444`) and `randomizeStep`
  (`:5579-5597`) never touch the enables; Start-over (`:5427-5438`) re-forces 4–7
  off; every 4–7 default is itself a no-op value; only ONE "Step enabled"
  checkbox is visible at a time (all 8 share bounds). Fix: default all enables on,
  make Dice/reset set them, give 4–7 non-neutral reset defaults.
- **#18 Swing — NEVER BUILT.** No global swing param in APVTS, no per-instrument,
  not applied at scheduling. The lone `ComplexEnvelope::swing`
  (`PatternManager.h:177`) is write-only dead state (saved `:1349`, loaded
  `:1749`, read by nothing). The Riff Machine's Groove step is a self-contained
  destructive note-position bake (`:5824-5848`) referencing no global. **Decision:
  build BOTH — a global swing control AND a per-instrument override — and apply
  swing at scheduling time in the pattern playback path.** Net-new feature.
- **#19 Riff Levels no double-click-default.** The factory lambdas `addKnob`
  (`:5274-5287`) / `addIncDec` (`:5288-5298`) never call
  `setDoubleClickReturnValue`. Missing on ALL Riff sliders + all Humanize sliders.
  The `init` arg is already in scope. (Note per-step Reset values may differ from
  construction `init`.)
- **#20 Riff rework checkbox auto-check.** `mWorkExisting` (`:5405-5408`) is
  rebuilt false every open (panel constructed fresh in `toolRiffMachine`
  `:5935-5951`). No per-roll state exists. Fix: a `bool riffMachineUsed` field on
  `PianoRollData` (set in `accept()` `:5891-5906`, read at construction),
  serialized via `rollToValueTree`/`rollFromValueTree` (`PatternManager.cpp:1108-
  1121`). Loader defaults false (backward compatible).

---

## 5. Tracks defects (arrangement track headers, `BuilderPage.cpp`)

All in `TrackHeaderPanel::showTrackContextMenu` (`:6850-6993`). There is no drag-
to-reorder; this menu is the whole move surface.

- **#21 + #22 — ONE fix.** Move Up/Down (`:6885-6920`) is a hardcoded single-row
  swap between `row` and `row±1` (`:6888`) that also swaps the group **ids**
  (`:6907-6908`) — so moving a grouped track redistributes membership rather than
  moving the group, and a ±1 destination lands inside an adjacent group. Fix:
  compute the contiguous group span, rotate the whole span past the neighbor's
  full span; group ids travel WITH the span. Model on
  `ArrangementGrid::insertBlankRowsAt` (`:3520-3550`). Extract a
  `groupSpan(row,&first,&last)` helper (both #21 and #22 need it).
- **#23 Recolor not undoable.** "Color Group…" (`:6956-6969`) lacks the
  `beginEdit`/`commitEdit` bracket that Move/Insert have. The undo infra already
  snapshots row colors (`beginEdit :3118`, `commitEdit :3138-3154`,
  `UndoActions.h:137`, `applySnapshot :3180`) — just call it (capture the "before"
  before the async picker opens). **Group-with-Above (`:6932-6948`) and Remove-
  from-Group (`:6949-6955`) have the identical omission — fold them in.**

---

## 6. Builder grid defects (`BuilderPage.cpp`)

- **#24 Blocks shouldn't loop at all.** A prior fix made a stretched block play
  the pattern's full length but LEFT the tiling in. **Required: play content once
  from the content offset; any length beyond available content is SILENT (17th
  bar of a 16-bar pattern = blank).** Two tiling loops: audio scheduler
  (`PluginProcessor.cpp:2538-2568`, `nTiles` at `:2541`) and preview paint
  (`ArrangementGrid::drawMidiShading`, `:2335/2355/2365/2371-2386`). Collapse both
  to a single pass; the tail goes silent automatically. Ripple: `sliceOneBlock`
  `% cycleTicks` (`:5675-5676`) must drop the modulo. `getPatternContentBeats`
  stays — now as "content length past which it's silent."
  **HARD CONSTRAINT: must ship WITH #25** — else a 16-bar pattern dropped as a
  4-bar block silently loses 12 bars (tiling currently masks the wrong default).
- **#25 Drop length should match content.** Four sites default to 4 or 1 bar
  (browser drop `:5018`, drag ghost `:4964`, Draw `:5553→6302`, Paint
  `:5576/5948`). **Correction to an earlier claim: `getPatternContentBeats(int
  patternIndex)` is ALREADY generalized per index** (`PatternManager.cpp:935`,
  declared `:741`, already called with arbitrary indices) — the drop handler just
  never calls it. Fix: 4 one-line changes using the existing
  `setLengthBeats`/`lengthBars` idiom (`:6046`).
- **#26 No click-copy on builder grid.** The empty-space consumer exists
  (`:5545-5556 → :6332-6341`); the two block-hit branches (`:5460-5540`,
  `:5586-5629`) never prime the "what to place" state (`mBrowserSelection` /
  `mDropKind`, `BuilderPage.h:775-778`). Fix: prime it from the clicked block +
  add a `mClickMemoryLenBeats` field. **NOTE: must also copy block VELOCITY** —
  Jeff flagged that the roll's click-copy dropping velocity (#12) is the same
  class of bug; the builder copy must carry velocity too.
- **#27 URGENT: 1-bar snap lock.** Snap (`snapBarAlt` `:1679-1706`) is ALREADY
  zoom-aware and returns fractional bars. Two lines destroy it: the move handler
  casts to `int` (`:5915-5916`) and explicitly clears the sub-bar field
  (`:5924` `startTicks = kStartTicksUnset`). The drag origin also reads the
  integer bar (`:5529/5607`), so a sliced piece re-quantizes the instant it's
  grabbed. Fix (~6 lines): seed origins from `effectiveStartBeats`, write
  `setStartBeats` + `startBar = floor(...)` (idiom at `:6043-6045`), stop clearing
  `startTicks`, snap the delta not the absolute.
- **#28 URGENT: Alt+drag missing.** Alt IS wired (`mAltSnapActive`
  `:5101/:1675`) but truncated by the same `(int)` cast at `:5916`. Same for
  resize (`:5482/5506`). Piano-roll reference: `PianoRoll.cpp:2100-2113` (snaps
  the delta, preserving multi-selection offsets). **Not in the keybinds** —
  `KeyBindings.cpp` has no "Alt+Drag = bypass snap / fine PPQ positioning" row for
  Builder OR Piano Roll (only slice-tool Alt rows exist). Add 2 doc rows.
- **#29 Inst/Clips/Rusty ghost notes missing.** `drawMidiShading` paints
  layerRoll / bassRoll / drumRolls / legacy drumRoll only (`:2393-2396`).
  **Decision: add Inst (`instRoll`), Clips (`clipRoll`), and Rusty
  (`baySickRustyDrumsRoll`) — NOT Vox (no vox MIDI).** All three are already
  scheduled by audio (`PluginProcessor.cpp:2586-2618`) and counted by
  `getPatternContentBeats`. One loop each.

---

## 7. Transport / playhead defects

- **#30 Playhead misaligned.** **Two CONFIRMED bugs** (no TS marker involved —
  Jeff confirmed none was applied when he saw this):
  1. Builder never got the pixel-center click fix the roll has. Roll:
     `xToBeat = mBeatOff + (x+0.5)/mPPB` with `llround` (`PianoRoll.cpp:508-512`,
     the LDT-394 fix). Builder: `xToBar = mBarOff + x/mPPBar` with C truncation
     (`BuilderPage.cpp:1630-1642`) → click biased half a pixel early, snaps to the
     previous line. One-line ×2.
  2. Roll playhead has no output-latency compensation; the Builder's does
     (`BuilderPage.cpp:7473-7480` subtracts `getTotalOutputLatency`;
     `PianoRollPage.cpp:82` does not). One-line.
  - **These two account for the BUILDER half of "cursor must be on the playhead
    line." The ROLL half is unexplained** (the roll already has the pixel-center
    fix). **Decision: add a diagnostic readout to the roll (a reading for the
    plan, not a fix) so the residual can be characterized before the plan is
    written.**
  - **LATENT (separate, not what Jeff saw):** blocks are positioned by bar INDEX,
    ruler/playhead by musical MAP POSITION (`barToX(effectiveStartBars)` vs
    `barToX(barStartBeat/4)`). They agree ONLY with no TS marker; add a TS marker
    and blocks drift from the ruler. `snapBarAlt`'s `g>=384` branch mixes the two
    domains. **Decision 8A: store block start as absolute beats/ticks, derive the
    bar index for display only** (touches the block data model). Interacts with
    #27 — un-truncating the snap makes this mismatch visible, so land #27 with or
    before 8A.
- **#30b First block doesn't play (dropped note-ons).** SEPARATE mechanism from
  the line position. The song-mode scheduler wraps drums/clips/vox/inst in
  **try-locks** (`PluginProcessor.cpp:2576-2616`, pattern mode `:2645-2680`); if
  the message thread holds the lock for that one audio block, every note-on in it
  is silently DISCARDED and never retried. Layers/bass have no lock (matches
  "partially doesn't play"). Contention is likeliest right after a UI action
  (engine rebuild, pattern switch) = "sometimes." **Decision 10B: lock-free roll
  snapshot so the audio thread never waits and never discards. NON-NEGOTIABLE.**
- **#31 Playhead visibility.** Builder is correct (hidden in pattern mode,
  `:7461-7468`). The ROLL hides its playhead UNCONDITIONALLY in song mode
  (`PianoRollPage.cpp:81-82`, `beat = song ? -1.0 : …`), with no check for whether
  the viewed pattern is the one playing. **Required: pattern mode = roll only;
  song mode = builder grid AND the roll IF the viewed pattern is actively
  playing.** Fix (~15 lines): in song mode, find a block with
  `patternIndex == current` whose span contains the playhead beat and pass the
  pattern-local beat; `-1` only when none. (Simplifies once #24 removes tiling —
  local beat = `songBeat - effectiveStartBeats + ticksToBeats(offset)`, no
  modulo.)

---

## 8. Drum kit grid defects (`DrumKitGrid.cpp` / `DrumPage.cpp`) — URGENT

Model: a kit block's drum identity is **purely positional** — the array it lives
in, `Pattern::drumRolls[pageIndex]`. Row→drum via `rowToPageIndex`
(`DrumKitGrid.cpp:483-487` reading `mRowsCache[row].pageIndex`, from
`StandaloneEditor::getKitDrumList()` `:6085-6110`). Paint is self-consistent
(`:2287-2294`). Three OTHER places disagree with it:

- **#32 Recorded MIDI invisible.** Recording writes to `pat.drumRoll` — the pre-
  D1 legacy single roll nothing paints (`StandaloneEditor.cpp:12810-12812`;
  `mLastRollIndex`, correctly set to the drum's pageIndex at `:4334-4335`, is
  ignored here). No refresh after commit either (`commitRecordingResult` ends at
  `:12875` with only `markDirty()`; no `refreshAllKitViews`). **Deeper: the
  recorder can't identify WHICH drum was hit** — the raw hardware note is merged
  into `allMidi` (`PluginProcessor.cpp:2867`) while the drum fires at its own play
  note into `drumPageMidi[di]` (`:5496-5560`); the two never meet. This is the
  fallout of plan decision D-12 ("do not build a record path") in
  `eager-thumping-marmot.md:94` — the record path was single-roll by construction.
  **Fix (small-structural): demux captured notes through `mDrumTriggers` (match
  each recorded note to the drum whose binding matches), push into
  `pat.drumRolls[thatDrum]` stamped at that drum's playNote, then
  `refreshAllKitViews()`.**
- **#33 Kick/snare swapped after reload.** Because #32 put notes in
  `pat.drumRoll`, load runs the legacy Phase-C1 migration `slotIndex = 51 -
  midiNote` (`PatternManager.cpp:1862-1870`, folded `:1879-1893`) — a DESCENDING
  map, so adjacent pads return reversed. **Falls out of #32** (recorder targets
  `drumRolls[]` directly → migration never entered). Gate the migration so it's
  unreachable for notes written by today's recorder.
- **#34 Cross-row drag deletes the block.** `DrumKitGrid.cpp:1759-1771` reads the
  note from the ORIGINAL row's array (`rowToPageIndex(oldRow)`, `:1762`) using an
  index that belongs to `mMoveRefs[i].row` (the CURRENT row). After the first
  cross-row event the two arrays differ → the read bails on bounds (`:1765`),
  `tmps` empties, but the erase (`:1773-1784`) still runs → **note deleted.**
  Second bug on the same path: the moved note keeps the SOURCE drum's `midiNote`
  (`:1766-1770/:1793`), never re-stamped to `playNoteForPage(destPage)` → on a
  one-shot sampler it lands silent. **Fix: one-line (`:1762` →
  `rowToPageIndex(mMoveRefs[i].row)`) + re-stamp pitch at the re-insert
  (`:1791-1793`), guarded so a deliberately re-pitched hit is preserved per D-6.**
- Context: `eager-thumping-marmot.md` (per-drum MIDI note + MIDI Learn plan) and
  its running notes describe what just landed here. The per-drum-MIDI feature also
  has its own follow-on items (kit-only menu split, learn-a-note prompt, trigger
  persistence) tracked in the Master Test Plan §B.18 L-9..L-14.

---

## 9. Engine defects

- **#35 Octave-up bell too hot.** There is NO bell generator and NO bell level
  constant. The metallic ring is the granular shifter's overlap-add artifact:
  `setGrainSize` (`OctaveStyleDSP.h:109`) retunes `grainSize` every block
  (`.cpp:479`) WITHOUT re-anchoring the two read heads, so the Hann window pair
  stops summing to 1.0 (unity assumed at `.cpp:101`, established only in `reset()`
  `:29`) → up to **+6 dB amplitude modulation** = "vibrating," which tears gain-
  driven downstream. Shows on the up voice because the jump fires twice as fast at
  ratio 2.0 (`.cpp:289/546-547`) AND the octave-pedal commit moved −1/−2 onto the
  clean `PeriodDoubler` (`:486-491/560-576`), removing the same artifact from the
  down voices. The level knob `mOct1Up` (`.cpp:688`) is user-facing; no internal
  trim. **Fix: normalize the OLA (one-line at `.cpp:102`:
  `out = (w1+w2>1e-4)?(s1*w1+s2*w2)/(w1+w2):0`) OR re-anchor heads in
  `setGrainSize` + hysteresis on `:479`.**
- **#36 Overlapping base/slide voice reversal.** `rampChainDurationBeats`
  (`PluginProcessor.cpp:139-160`) extends a base note's note-off across its slide
  chain by matching **time only** — no pitch check, butt-joined counts. Bar-2's C5
  slide starting exactly at beat 4 is absorbed into bar-1's C5 chain → bar-1's
  note-off deferred to beat 8 → it plays through all of bar 2. CC85 takeover also
  matches by MIDI note number with no first-match guard
  (`BaySickSynthVoice.cpp:256-257`, broadcast `BroadcastSynthesiser.h:27-33`).
  Secondary (if the slide is RetrigSlide/Portamento): `findGlideSourcePitch`
  (`:111-132`) has no end-time check and the blind mono-cut (`:2455-2457/2476-
  2478`) can note-off the held base at its own start. **Fix: add a pitch/lineage
  predicate to `rampChainDurationBeats` (only absorb ramps whose own
  `findRampAnchorPitch` resolves to the same source) + the missing end-check on
  `findGlideSourcePitch`. Proper fix: a stable per-note voice token through
  CC84/85 instead of `getCurrentlyPlayingNote() == mGlideFromNote`.**

---

## 10. Cross-cutting

- **#37 RP loudness ramp dead in BaySickPlayer.** The CC whitelist
  (`VibePlayerDSP.cpp:1455-1456`) omits CC86, so `mSlideTargetVel` is never set
  and the S-6(C) loudness ramp (`VibePlayerDSP.h:354-366`) is dead code there —
  the option-(C) ramp Jeff signed off only ever worked on the synth. **Any new
  pan-ramp or slide CC will hit the same hole unless the whitelist is extended.**

---

## 11. Hard sequencing constraints (independent of how the batch is sliced)

1. **#24 (remove loop) and #25 (drop length) MUST ship together** — else a 16-bar
   pattern dropped as a 4-bar block silently loses 12 bars.
2. **#27 (builder snap) lands with or before 8A (playhead domain unification)** —
   un-truncating the snap makes the bar-index-vs-map-position mismatch visible.
3. **The sfizz pitch-wheel fix (#1/#8) is a PREREQUISITE for the slide rework** —
   with the voiced SlideSampler, the slide path and Bend both depend on a correct
   wheel; it also fixes normal-note pitch, so it's foundational.
4. **#32/#33/#34 (drums) are interdependent** — #33 falls out of #32; do them as
   one unit.
5. **#30b dropped-notes fix is non-negotiable** (Jeff's word) and independent of
   the line-position fixes in #30.

---

## 12. Open items to resolve during planning

- **Roll-side playhead residual (#30).** A diagnostic must be added and Jeff runs
  a Debug build to characterize it before that half is planned.
- **Slide landing A/B (#2 tail).** Ear-check: slide into a note, then play that
  exact note normally (sfizz) — the voiced-SlideSampler render must match sfizz
  closely. This is the one fidelity seam.
- **Any NEW spec calls** the review surfaces — route to Jeff, do not guess.

---

## 13. Decision log (locked — do not re-litigate)

1. **Slide architecture:** voiced SlideSampler, formant-correct zone-hop,
   reproducing the FULL patch voicing + FULL control surface. WHY: single-sfizz-
   wheel stretches one sample (formant break, disqualifying for a real string);
   sfizz pool = layer mud + thump + cost; our zone-hop stays formant-correct.
2. **Fidelity:** everything, no approximations — **including bass Mono switch
   (cc105) and the Humanize/secondary wobble LFOs (cc117).** No half-assing.
3. **Scope:** the ENTIRE dossier is in scope. Jeff builds it with fable, so batch
   size is not a limiting factor.
4. **Pitch offset (#1):** guitar +3 / bass +2 is the clean pitch-wheel bug — no
   residual to chase (Jeff re-tested both).
5. **Humanize seed (#15):** dropdown 1–10, default 1, NO "None" (beginner-
   friendly). Regenerate picks a value in 1–10.
6. **Humanize presentation (#16):** keep our column headers ("Range"/"Offset") +
   percent display; do NOT adopt FL's per-control labels or decimals.
7. **Humanize distribution (#14):** Uniform / Triangular / Quasi-Normal, default
   Quasi-Normal.
8. **Humanize interval (#13):** 1/32, 1/64, 1/128, default 1/64 (standalone list).
9. **Humanize defaults (#16):** Start Range 10%, Duration Range 10%, Velocity
   Range 20%, offsets 0.
10. **Pan ramp (#11):** ramp pan-only over the glide on RP/RT for IN-HOUSE engines
    (BaySickSynth/Bass, Harmless, VibePlayer) + fix the broadcast-to-all-voices
    pop + `mNotePan` reset. **Withdrawn for Guitars/Basses** (no reachable pan
    control).
11. **Playhead domain (#30 latent):** 8A — store block start as absolute beats,
    derive bar index for display.
12. **Dropped notes at Play (#30b):** 10B — lock-free roll snapshot. Non-
    negotiable.
13. **Swing (#18):** build global swing + per-instrument override, applied at
    scheduling time.
14. **Ghost notes (#29):** add Inst + Clips + Rusty. NOT Vox.
15. **Roll playhead residual (#30):** add a diagnostic reading (not a fix) for the
    plan.
16. **Slide markers (#9/#10):** right-edge, white border, black fill, for Porta
    and Bend.

**Corrections logged (were said, now fixed):** the "2 vs 3 semitone" direction-
split was a mis-test (it's clean +3 guitar / +2 bass); "thin" slide was wrong
(Jeff heard LOUD + wrong timbre); pan-on-guitars/basses was unreachable
(withdrawn); the content-length helper was NOT hardcoded (already generalized).

---

## 14. Source references

- Smoke script: `Files For Claude/G3 Boundary Smoke.txt`
- Defect report: `Files For Claude/Smoke Test Issues.txt`
- Prior slide batches (context for what shipped): `Plans & Specs/Batch Plans/
  wistful-sliding-otter.md` (+ running notes), `Plans & Specs/Batch Plans/
  silky-gliding-lynx.md` (+ running notes — the SlideSampler as-built + the SS-Q5
  tuning checklist).
- Per-drum MIDI batch (context for §8): `Plans & Specs/Batch Plans/
  eager-thumping-marmot.md` (+ running notes). D-12 = "no record path"; D-6 =
  preserve deliberately re-pitched hits.
- Master Test Plan §B.18 L-9..L-14 (per-drum-MIDI follow-ons).
- Governing rules: `CLAUDE.md`, `Plans & Specs/Main Plan.md` §0.
- Slide crossfade research (to be saved): `Plans & Specs/Research Reports/
  daw-architecture-slide-crossfade-gain-matching-2026-07-22.md`.
