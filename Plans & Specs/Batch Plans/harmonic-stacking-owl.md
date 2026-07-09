# QA-Chords — Multi-Select Resize + Scale-Aware Dual-Mode Chord Stamp — Plan (harmonic-stacking-owl)

> **Canonical path (post-approval):** `Plans & Specs/Batch Plans/harmonic-stacking-owl.md` (mirrored
> at G1 group approval; home-dir copy deleted). **For execution:** BULK-RUN mode — no per-task
> verify pauses; Verify scripts author into Master Test Plan §B; Work Log entry drafted + HELD; one
> source commit.

## Context

§5 items: (1) a stamped chord can't be stretched/resized; (2) dual-mode Root/Scale chord
generation. Surface facts (2026-07-08 map): chord intervals are hardcoded semitones
(`kChordDefs[]`, 14 types) with only the ROOT scale-snapped; stamps land as N independent notes;
resize is single-note only (`mResizeNoteIdx`); the Session-6 scale primitives survive
(`snapPitchToScale`, `nextScalePitch`, and `toolGenerateChords`' in-scale-thirds stacking — the
direct ancestor of Mode 1).

- Risk: low-medium — piano-roll data/UI only, no audio-thread surface. Undo brackets already wrap
  every touched gesture.
- Effort: ~5-8h. Dependencies: none (second in G1).
- **Bucket:** System Pages.

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| D | Chord-as-unit resize = **selection-based multi-note resize** (general feature); groups (Shift+G) stay manual/permanent; NO auto-grouping of stamps | Jeff, G1 round. Stamp already leaves the chord selected → unit resize immediate. |
| D2 | Mixed-length selections: **same delta to each note**, relative lengths preserved, per-note min clamp | Jeff, G1 follow-up (option 1). |
| §5 Mode 1 | Snap-to-Scale OFF: dropdown generates the chord from **scale degrees** at the clicked note (roll's own Root+Scale menu state), not hardcoded semitones | Jeff's §5 spec text ("fits the selected scale degrees relative to the clicked note"). |
| §5 Mode 2 | Snap-to-Scale ON: literal template → strict per-note scale compliance → **octave-collision resolver** (duplicate shifts up to the next valid scale degree an octave up — thickness preserved, never stacked/deleted) | Jeff's §5 spec text. |

Derived (stated at presentation): Mode 1's degree-stacking means Major/Minor/Dim/Aug all resolve to
the 1-3-5 degree template — the clicked degree's NATURAL quality emerges from the scale (click the
second degree of a major scale, get its minor triad, even with "Major" selected). That is the
literal reading of "fits the selected scale degrees"; the template families are: triad (Major/
Minor/Dim/Aug), sus2 {1,2,5}, sus4 {1,4,5}, 7th family {1,3,5,7}, 9th family {1,3,5,7,9},
Add 9 {1,3,5,9}. Mode 2 keeps the 14 literal shapes and bends them into the scale.

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open.

## Files to modify

### Task 1 — multi-select resize
- `Source/Standalone/PianoRoll.h:307-317` (resize state: `mResizeNoteIdx` → `std::vector<int>
  mResizeIndices` + per-note originals)
- `Source/Standalone/PianoRoll.cpp:1318-1329` + `:1447-1458` (the two resize entry points),
  `:1621-1646` (drag apply), `:1779-1785` (release)

### Task 2 — Mode 1 (degree stacking)
- `Source/Standalone/PianoRoll.cpp:340-349` (`setScale` — populate `mScaleInKey` regardless of
  active; keep the 5 behavior sites gated on `mScaleActive`), `:45-60` (`kChordDefs` gains a
  degree-template column), `:388-408` (`stampChordAt` branches), `:2176-2194` (ghost preview
  mirrors), `:3276-3282` (`selectChord` unchanged wiring)

### Task 3 — Mode 2 (strict snap + collision resolver)
- `Source/Standalone/PianoRoll.cpp:388-408` (same injection loop), `:356-383`
  (`snapPitchToScale`/`nextScalePitch` — reused, not modified)

## Tasks

### Task 1 — selection-based multi-note resize (D/D2)

- [ ] Replace single-index resize state with `mResizeIndices` + parallel per-note
      `{origStart, origDur}` snapshots. Entry points: after `noteIndexNearRightEdge` hits note `ri`,
      populate `mResizeIndices = (mSelection contains ri && mSelection.size() > 1) ? mSelection
      : {ri}`, then `expandForGroups(mResizeIndices)` — same expansion Move uses, so Shift+G groups
      stay consistent (grabbing a grouped note resizes its group).
- [ ] Drag apply: compute the delta from the grabbed note's edge exactly as today, then apply the
      SAME delta to every index (right edge → `durationBeats`; left edge → `startBeat` +
      `durationBeats`), clamping each note independently at min duration (`snapUnitBeats()`, or
      1 tick with Alt/snap-off) so short notes floor without blocking the rest.
- [ ] Release path unchanged (`sortNotes` + `commitEdit`); the existing "Resize" undo bracket makes
      the multi-resize one undo step.
- [ ] Single-note behavior byte-identical when selection is empty or the grabbed note is outside it.

### Task 2 — Mode 1: scale-degree chord generation (Snap OFF)

- [ ] `setScale` populates `mScaleInKey` even when `active == false` (audit: the five gated
      behaviors — move-snap :1608, transpose :802, stamp-root :392, row tint :1912, generate :3895 —
      keep their `mScaleActive` gates; only the TABLE becomes unconditional).
- [ ] Extend `kChordDefs` with degree templates (per the derived table above). `stampChordAt`, when
      `!mScaleActive`: clicked note → nearest scale pitch (reuse the outward search) → its scale
      degree; stack the template's degree offsets through the sorted in-scale pitch-class list
      (`pcToMidi` + wrap-to-octave logic lifted from `toolGenerateChords` :3917-3934); emit notes
      (vel 0.8, `snapUnitBeats()` duration, same selection/undo finalize as today).
- [ ] Ghost preview (:2176-2194) renders the same Mode 1 result while hovering.

### Task 3 — Mode 2: strict compliance + octave-collision resolver (Snap ON)

- [ ] When `mScaleActive`: literal semitone template → `snapPitchToScale` each note → ascending
      walk; on a duplicate MIDI value, shift the duplicate +12 then snap; while still colliding
      (sparse scales), `nextScalePitch(n, +1)` until free; clamp 0-127 (drop only if the resolver
      runs off the top — never delete a lower note).
- [ ] Ghost preview mirrors Mode 2 output.
- [ ] Rule 6 pass on all touched regions (the `kChordDefs` header comment gains the degree-template
      why; no WHAT narration).

### Task 4 — batch close (bulk-run shape)

- [ ] Author Master Test Plan §B "QA-Chords" from the Verify scripts (`blocks:` = batch commit).
- [ ] Draft + HOLD Work Log entry in `Running Notes/harmonic-stacking-owl.md`; code-complete
      running-notes entry.
- [ ] One source commit (Rule 9): message + full status → approval → commit.

## Verify scripts (→ Master Test Plan §B; Debug first, then Release)

1. Stamp a Major 7 → immediately drag one member's right edge: all four notes stretch together by
   the same amount; one Ctrl+Z restores all.
2. Marquee-select notes of different lengths → drag an edge: each changes by the same delta,
   relative differences preserved; the shortest floors at min duration without stopping the others.
3. Grabbing a note OUTSIDE the selection resizes only it (single-note path intact).
4. Shift+G a set, deselect, grab one member's edge → the whole group resizes (matches group-move
   consistency).
5. Snap-to-Scale OFF, roll scale A Minor: stamp "Major" on E → E-G-B (the degree's natural minor
   triad, all in scale); stamp a 7th/9th/sus template → correct degree structures, zero
   out-of-scale notes.
6. Snap-to-Scale ON, scale Pentatonic Minor: stamp a 9th chord whose snapped intervals collide →
   duplicates resolve an octave up on valid degrees; note count preserved (thickness kept).
7. Ghost preview matches the stamped result in both modes while hovering.
8. Snap OFF + scale never touched (defaults C Major): stamping still degree-stacks sensibly.
9. Existing behaviors untouched: single-note draw/resize, chord stamp in a drum-kit context absent
   (unchanged — no chord stamp there), P-key Generate Chords unchanged.
10. Save → reload: stamped notes persist as plain notes (no new fields, no format change).

## Routing notes (Rule 3 application during execution)

If Mode 1's quality-from-degree behavior surprises at the campaign (e.g. Jeff wants forced-quality
with borrowed notes), that's a re-spec call at section pass — log, ask, route. Any control-lane/
selection findings route to QA-H (same surface, G3).

## Carry-Forward Reference touch points

- §1 piano-roll primitives skim before Task 1. The 2026-07-08 chord surface map (running-notes
  seed) is the authoritative line index — §5's hint refs (350-352/689-692) are off by a few lines.
