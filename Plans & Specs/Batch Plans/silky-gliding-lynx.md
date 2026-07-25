# QA-SlideSampler — Blended Multi-Sample Slide + Native Bend for sfizz Engines (Guitars/Basses) — Plan (silky-gliding-lynx)

> **Canonical path:** `Plans & Specs/Batch Plans/silky-gliding-lynx.md`.
> **For execution:** implementation plan for the guitar/bass SLIDE + BEND story that QA-SlideSliceGlide
> (wistful-sliding-otter) DEFERRED at its A-1 STOP. Design + feasibility were established in a full
> workshop + spike on 2026-07-20 — the shape below is decided; the OPEN sub-spec calls are flagged.
> **Read first:** the feasibility spike report
> [`Plans & Specs/Research Reports/daw-architecture-sample-based-continuous-pitch-slide-2026-07-20.md`](../Research Reports/daw-architecture-sample-based-continuous-pitch-slide-2026-07-20.md)
> and the QA-SlideSliceGlide running notes Task-6 section (the capability checks + patch analysis).

## Context

sfizz has NO per-note MPE bend and mixes ALL voices into one buffer, and the karoryfer guitar/bass
patches ship only small, up-only (guitar ~+3 semi) / ±2 (bass) native pitch-bend. So a real slide
(continuous, any direction, any distance) CANNOT come from sfizz. The workshop landed on Jeff's own
model: **play the real per-note samples along the pitch path and crossfade them** (finger on a
fretboard), NOT one sample stretched.

The spike (source-verified + external research) killed the "pool of sfizz instances" option (it
crossfades 4-6-layer MUD per note and can't micro-bend down) and confirmed the winning recipe is what
Kontakt/Ample/Shreddage do: 2 voices steady + a brief 3rd on handoff, equal-power crossfade of the
real SUSTAIN samples, a sample OFFSET to hide the re-pluck attack, and a small per-sample micro-bend.
The key trick (spike-confirmed sound): **always grab the sample at-or-just-below the current pitch and
bend it UP ≤1 semitone** — keeps formants fixed (no chipmunk) and works in BOTH slide directions,
sidestepping the up-only wall.

**Risk:** high — a new audio-thread sampler + crossfade DSP + SFZ region extraction + an engine-aware
Note Properties redo. **Effort:** ~2-4 weeks honest (the spike's estimate). **Dependencies:** builds on
QA-SlideSliceGlide's slide/note-type/Note-Properties plumbing (CC84/CC5/CC85 transport, NoteType enum,
`portaLengthBeats`, the NotePropsPanel) — all shipped there.

## The decided design (Option C — hybrid)

- **sfizz stays the sole engine for every NORMAL note** (full-fat patch: layers, keyswitches, RR,
  velocity, unison, feedback — unchanged).
- **A new purpose-built `SlideSampler` handles the slide gesture only.** At patch load, extend the
  existing `loadKit` `#include`-walker to also build a `note -> {wavPath, rootKey, sampleOffset,
  loopStart, loopEnd, tuneCents}` table for the MAIN sustain articulation (ONE velocity layer + ONE
  round-robin — a slide holds a fixed layer). The SlideSampler preloads those wavs (a fraction of the
  full patch: guitar 560 MB / bass 1.1 GB full; one articulation is far smaller) and, for a slide,
  crossfades 2-3 of them with the ≤1-semi micro-bend + attack-offset. The "lost" layering is the
  CORRECT thinner sound for a slide (one string ringing, not the unison/feedback stack).
- **Note Properties redo for Guitars/Basses (engine-aware).** Those rolls STOP showing the in-house
  RP/RT/Porta buttons (which sfizz never decoded — dead since forever). They get:
  - **"RP Slide"** = the new blended SlideSampler slide (feels like the in-house slide; any direction).
  - **"Bend"** = the native sfizz global pitch-wheel bend, with a **semitone-amount dropdown gated to
    the engine's real range** (guitar: up to +3, up-only; bass: ±2 both ways), block length = duration.
  - The always-on **notice** at the bottom of the box (from QA-SlideSliceGlide's deferred plan):
    "Note: on BaySickGuitars and BaySickBasses, a slide bends every playing note together not just one.
    Useful for solos, not for chord bends." — reword for the split (Bend vs Slide) as needed.

## Spec calls already locked (2026-07-20 workshop + spike)

| ID | Decision |
|----|----------|
| SL-1 | Approach = Option C (hybrid: sfizz for normal notes + custom SlideSampler for slides). NOT a pool of sfizz instances (crossfades layer-mud, still up-only). |
| SL-2 | Slide sample-pick = at-or-just-below the current pitch, bend UP ≤1 semitone; crossfade to the next zone as the path crosses each boundary. Works both directions, no chipmunk. |
| SL-3 | Attack-hiding = sample OFFSET (start into the sustain body) + short amp fade-in on every voice except the slide's first note. |
| SL-4 | Region extraction reuses/extends the loadKit `#include`-walker (do NOT couple to sfizz's private `Region` type). ONE vel layer + ONE RR held for the whole slide. |
| SL-5 | Note-props redo: strip the dead in-house slide buttons on Guitars/Basses; add "RP Slide" (SlideSampler) + "Bend" (native, gated semitone dropdown) + the notice. Engine-aware (reuse the QA-SlideSliceGlide engine-context plumbing). |
| SL-6 | "Bend" uses the patch's NATIVE range, read at load (guitar up-only ~+3, bass ±2). The dropdown reflects the actual capability per engine. |
| SL-7 | Start MONO-slide only (one sliding voice). Per-string poly-slide (Ample-style) is a later add if wanted (multiplies SlideSampler voices). |

## Sub-spec calls OPEN (resolve during this batch — surface via chat before landing)

- **SS-Q1 — do the karoryfer main-articulation regions carry usable `loop_start/loop_end`** (so a slide
  can sustain indefinitely), or are they one-shots (slide length capped by sample length)? Read the
  map .sfz at build start.
- **SS-Q2 — exact on-disk size of ONE sustain articulation per patch** (validates the "light memory"
  claim vs a full sfizz instance). Measure against the installed library.
- **SS-Q3 — new NoteType(s) vs per-engine reinterpretation.** The enum is `{Standard, RampSlide,
  Portamento, RetrigSlide}`. "Bend" needs a per-note semitone-amount field (mirror `portaLengthBeats`).
  Decide: new "Bend" NoteType + field, or reuse an existing type on sfizz engines. Serialization impact.
- **SS-Q4 — handoff on slide-end:** sustain in the SlideSampler to note-off, or hand back to sfizz with
  a clean landing noteOn crossfaded under the SlideSampler tail? (Spike leaned: sustain in SlideSampler
  is simpler; test both for the landing transient.)
- **SS-Q5 — crossfade/offset tuning budget** (the "black-art" step): equal-power law, crossfade length,
  per-sample offset, loop-phase alignment so zone boundaries don't click/comb. Budget generously.

## Files to modify (indicative)

- `Source/BaySickGuitars/BaySickGuitarsProcessor.h/.cpp`, `Source/BaySickBasses/BaySickBassesProcessor.h/.cpp`
  — region extraction (extend the loadKit walker), route the slide note to the SlideSampler, suppress
  the sfizz voice for the sliding note during the gesture, native "Bend" pitch-wheel range read.
- NEW `Source/.../SlideSampler.h/.cpp` (shared, or per-engine-owned) — the crossfade slide DSP.
- `Source/PluginProcessor.cpp` — the scheduler already emits CC84/CC5/CC85; route sfizz-engine slide
  notes to the SlideSampler control input (no new transport needed).
- `Source/Standalone/PianoRoll.cpp` — NotePropsPanel engine-aware redo (RP Slide + Bend + dropdown +
  notice on Guitars/Basses; strip the in-house slide buttons there). Reuse the engine-context plumbing.
- `Source/PatternManager.h/.cpp` — a per-note Bend-amount field + serialization (SS-Q3).

## Tasks (from the spike's implementation sketch)

### Task 1 — Region extraction
- [ ] Extend the loadKit `#include`-walker (Guitars + Basses) to capture the `note -> sample` table for
      the MAIN sustain articulation (one vel layer + one RR): `sample=`, `lokey/hikey/key=`,
      `pitch_keycenter=`, `offset=`, `loop_start/end=`, `tune=`. Answer SS-Q1/SS-Q2 here.
- [ ] Build gate.

### Task 2 — SlideSampler component
- [ ] A small polyphonic sampler (2-3 voices): equal-power crossfade, per-voice resample via
      `juce::LagrangeInterpolator` / WindowedSinc for the ≤1-semi micro-bend (SL-2), attack-offset +
      short amp fade-in on non-first voices (SL-3). Preload the extracted wavs. Audio-thread-safe
      (no alloc/lock in the render path).
- [ ] Build gate.

### Task 3 — Slide path + handoff
- [ ] Pitch-path driver: per block advance source->target over the glide ms; pick the sample
      at-or-just-below (bend up ≤1) and crossfade to the next zone at each boundary. Route the
      "RP Slide" note on Guitars/Basses to the SlideSampler; duck/suppress the sfizz voice for the
      sliding note. Handoff on slide-end (SS-Q4). Reuse the CC84/CC5/CC85 transport.
- [ ] Build gate.

### Task 4 — Native "Bend" + Note Properties redo
- [ ] "Bend" note type + per-note semitone-amount field + serialization (SS-Q3); the native
      pitch-wheel ramp scaled to the patch range (read at load, SL-6). Engine-aware NotePropsPanel:
      Guitars/Basses show Flat / RP Slide / Bend + the gated semitone dropdown + the notice; the
      in-house RP/RT/Porta buttons are stripped THERE only. Reuse the engine-context plumbing.
- [ ] Build gate.

### Task 5 — Crossfade/offset QA + tuning (SS-Q5)
- [ ] Tune crossfade length, equal-power law, per-sample offset, loop-phase alignment so boundaries
      don't click/comb. Offline two-adjacent-sample comb test before committing wide.
- [ ] Build gate.

### Task 6 — BaySickPlayer reuse review (Jeff's explicit ask)
- [ ] **Evaluate whether the SlideSampler can ALSO back BaySickPlayer (VibePlayer).** VibePlayer loads
      SFZ files (`loadSFZ`) too, and its current glide is one-sample-stretch (same artifact). If the
      region-extraction + SlideSampler layer generalize, BaySickPlayer's SFZ-loaded sounds could get
      the same blended slide (and possibly its folder/single-file sounds, chromatic-mapped). Assess:
      (a) does VibePlayer's SFZ path expose enough to build the note->sample table, or does the walker
      generalize; (b) does routing a slide note through the SlideSampler for VibePlayer conflict with
      its existing per-sample glide; (c) scope — fold into this batch or a follow-on. Surface the
      finding to Jeff before building the VibePlayer path.
- [ ] Build gate (if any VibePlayer wiring lands).

### Task 7 — Close (bulk-run)
- [ ] Author the §B section (supersede SS-A in §B.22 — the sfizz slide is now real): RP Slide + Bend
      scenarios on Guitars/Basses (mono slide any direction; Bend gated per engine), the note-props
      redo, and the BaySickPlayer path if it landed. Physically-executable gestures.
- [ ] `/review-batch`.
- [ ] Draft + HOLD the Implemented Work Log entry; §5 STATUS flip + §9 back-refs (this reopens A-1).
- [ ] Surface the Rule-9 commit one-liner + FULL git status; commit on Jeff's explicit approval.

## Verification

Per bulk-run, scenarios author into §B at code-complete (Task 7), not run mid-batch. Build gates only.

## Routing notes (Rule 3)

- §9 Forks entry: this batch REOPENS A-1 (the QA-SlideSliceGlide net-new sfizz-slide scope) and delivers
  it as a purpose-built SlideSampler + native Bend, superseding the "global pitch-wheel" approach that
  batch STOPPED on. Back-ref the QA-SlideSliceGlide §9 entry.
- The engine-aware Note Properties redo touches the QA-H NotePropsPanel — annotate QA-H's §5 entry.

## Carry-Forward Reference touch points

- §1-3 (architectural primitives, file index) at Task 1 + Task 2 (sfizz MIDI path + sample loading).
- The QA-SlideSliceGlide running notes Task-6 section (capability checks, patch bend analysis, the
  spike conclusion) — the WHY behind every decision above.
