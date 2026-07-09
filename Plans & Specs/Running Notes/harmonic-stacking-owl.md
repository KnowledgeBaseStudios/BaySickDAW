# Running Notes — QA-Chords (harmonic-stacking-owl)

> Append-only mid-batch log. New entry at every checkpoint. Under the BULK RUN this batch's Work
> Log entry is drafted at code-complete, HELD here under `## Held Work Log entry (apply at section
> pass)`, and applied only when its Master Test Plan §B section passes.

Pair file: [`Batch Plans/harmonic-stacking-owl.md`](../Batch Plans/harmonic-stacking-owl.md).
Conventions: Main Plan §0 + [`Batch Plans/swift-stampeding-caribou.md`](../Batch Plans/swift-stampeding-caribou.md).

## 2026-07-08 — G1 group open — plan approved + surface map (source-verified)

Locked: D (selection-based multi-resize; groups stay manual), D2 (same-delta, relative lengths
preserved), §5 Mode 1 (degree stacking off the roll's own Root+Scale) + Mode 2 (strict snap +
octave-collision resolver). Verified refs (all `Source/Standalone/PianoRoll.*` unless noted):

**Stamp:** `kChordDefs[]` `.cpp:45-60` (14 hardcoded-semitone types); `stampChordAt` `:388-408`
(root scale-snap `:392`, duration `snapUnitBeats()` `:394`, interval loop `:396-402` = the
injection point, finalize `:403-407` leaves the chord SELECTED); ghost preview mirrors `:2176-2194`
(`mStampPos` `:1198`). Menu: Chords = top-level index 3 (`:4055-4060`, handler `:4114`);
`selectChord` `:3276-3282` arms the hidden Stamp tool (`.h:648`).

**Resize (single-note today):** state `.h:307-317` (`mResizeNoteIdx` `:308`, `kResizeZone` 6px
`:226`); entry points `:1318-1329` (Draw) + `:1447-1458` (Select); `noteIndexNearRightEdge`
`:471-495`; drag apply `:1621-1646` (min-dur `:1626`; left-edge adjusts start+dur `:1635-1636`);
release `:1779-1785`. Multi-drag template = Move: `mMoveIndices` `.h:293`,
`expandForGroups(...)` at `:1352`/`:1477`, shared delta `:1603-1614`; `expandForGroups` impl
`:922-943`; `groupId` `PatternManager.h:70` (stamps do NOT set it — per D that stays true).

**Scale machinery:** grid state `.h:356-359`; `setScale` `:340-349` — GOTCHA: fills `mScaleInKey`
only when active (`:344-347`) → Task 2 makes the table unconditional, behaviors stay gated.
Container state persists independent of the toggle: `.h:695-698`; `updateScaleFromUI` `:3188-3193`;
`kScaleDefs` `:28-42` (13 scales). Primitives: `snapPitchToScale` `:356-369` (nearest, up wins
ties); `nextScalePitch` `:371-383` (= the collision-resolver step). Gated behavior sites (keep
gated): `:1608` move-snap, `:802` transpose, `:392` stamp-root, `:1912` row tint, `:3895` generate.
Mode-1 ancestor: `toolGenerateChords` `:3871-3972` — in-scale thirds `:3912-3914`, `pcToMidi`
`:3917-3922`, stack+clamp `:3929-3934`. Snap toggle: Scale menu id 28 (`:4038`, handler `:4112`,
`setScaleActive` `:3258-3262`). Undo brackets: stamp `:391`/`:405`, resize `:1322`/`:1782`.

## 2026-07-08 — Tasks 1-3 CODE-COMPLETE (both configs build clean, Jeff)

- **Task 1 — selection-based multi-note resize (D/D2).** New `beginResizeGesture(grabbedIdx)`
  helper replaces the duplicated setup at both entry points (Draw + Select): gesture set =
  `mSelection` when the grabbed note is selected and selection > 1, else just the grabbed note;
  `expandForGroups` applied either way (grabbing a grouped note resizes its group — consistent
  with Move). New parallel state `mResizeIndices`/`mResizeOrigDurs`/`mResizeOrigStarts`; the drag
  computes the delta on the GRABBED note (right edge: dur delta; left edge: start delta) and
  applies the same delta to every member with per-note `minDur` clamps. Grabbed-note math is
  byte-identical to the old single path. Release: multi-gestures capture (startBeat, midiNote)
  keys and `rebuildSelectionFromKeys` after `sortNotes` (left-edge moves starts → indices shift);
  single-note release keeps the exact pre-existing behavior (no selection change).
- **Task 2 — Mode 1 degree stacking.** `ChordDef` gained a `degrees` template column (14 chords:
  triads {0,2,4} incl. Major/Minor/Dim/Aug collapsing by design; sus2 {0,1,4}; sus4 {0,3,4}; 7ths
  {0,2,4,6}; 9ths {0,2,4,6,8}; Add9 {0,2,4,8}). `setScale` now fills `mScaleInKey` UNCONDITIONALLY
  (audited all five gated behaviors — each checks `mScaleActive` before reading the table, so
  nothing else changes). `setStampChord` carries both shapes; both call sites (`:2713` container
  init + `selectChord`) pass them. New `resolveStampNotes(clickedNote)`: Snap OFF → nearest
  in-scale root (ungated outward search) + degree stacking through the sorted in-scale pitch-class
  list, strictly ascending, octave-lift per wrap; **fallback to the literal shape** when the scale
  is Chromatic (12-in-key degree offsets = tone clusters), the table is empty, or no degree
  template exists.
- **Task 3 — Mode 2 strict snap + octave-collision resolver.** Snap ON → literal intervals, each
  `snapPitchToScale`d; a duplicate jumps +12 then re-snaps; still-colliding walks `nextScalePitch`
  upward; notes only ever drop off the TOP of MIDI range, never merged. Ghost preview now renders
  `resolveStampNotes` output — preview == stamp in both modes.
- **Behavior notes for the campaign:** grabbing one note of a Shift+G group now resizes the whole
  group (new, consistent with group-move); Mode 1's Major/Minor/Dim/Aug all yield the clicked
  degree's natural triad (the spec'd "fits the selected scale degrees" reading — re-spec at
  section pass if it surprises).
- **Diagnostics:** none added (nothing for the Rule 4 catalog).
- **Files:** `PianoRoll.h` (resize state + gesture helper + resolveStampNotes + 2-arg
  setStampChord + mStampDegrees), `PianoRoll.cpp` (ChordDef/kChordDefs, setScale, stamp/preview,
  resize paths), test plan §B.2 (+§B.1 `blocks:` hash backfilled `d6d46cf`).

## Held Work Log entry (apply at section pass)

> Apply verbatim at §B.2 section pass; fill `<hash>` with the batch's source commit and the
> section-pass date/outcome. Group review line fills at the G1 boundary.

### <APPLY-DATE> — QA-Chords — Selection-based multi-note resize (same-delta, group-consistent) + scale-aware dual-mode chord stamp (Mode 1 degree-stacking off the roll's Root+Scale with Snap OFF; Mode 2 strict snap + octave-collision resolver with Snap ON; ghost preview mirrors both)

**Bucket:** System Pages

#### Done

- **Task 1 — multi-select resize (Jeff's D/D2 locks: selection-based mechanism, same-delta,
  relative lengths preserved).** `beginResizeGesture` unifies both resize entry points; gesture =
  selection (when grabbed note ∈ selection, size > 1) else single note, group-expanded like Move.
  Same-delta application with per-note min-duration floors, both edges; multi-gesture release
  rebuilds selection across the re-sort; single-note behavior byte-identical to pre-batch. A fresh
  stamp leaves the chord selected, so chord-as-unit resize works immediately (§5 item 1 closed).
- **Task 2 — Mode 1 (Snap-to-Scale OFF).** Degree templates on all 14 chord types; `setScale`
  fills the pitch-class table unconditionally (behaviors stay gated on `mScaleActive` — five sites
  audited); `resolveStampNotes` stacks the template through the roll's Root+Scale at the
  nearest-in-scale clicked note; Chromatic/empty-table/missing-template fall back to the literal
  shape. Natural degree quality by design (Major/Minor/Dim/Aug share the triad template).
- **Task 3 — Mode 2 (Snap-to-Scale ON).** Literal shape → strict per-note snap → collision
  resolver (+12 re-snap, then `nextScalePitch` upward walk); thickness preserved, drop only off
  the top of range. Ghost preview shares `resolveStampNotes` — preview equals result in both modes.

#### Found along the way

- Old release path never rebuilt the selection after `sortNotes` — harmless for right-edge
  (startBeats unchanged) but a latent stale-selection risk on single-note LEFT-edge resize.
  Multi-resize made rebuild mandatory; the single-note path was left exactly as it was (no
  unprompted behavior change) — the latent single-note case is noted here for the campaign.

#### What was done about each finding

- Multi path: fixed structurally (keys captured + rebuilt). Single-note left-edge latent case:
  intentionally untouched; log-only (route at section pass if the campaign reproduces a stale
  selection).

#### Group review (R3 — one /review-batch per checkpoint group)

- <G1-boundary outcome — filled at group review>

#### Diagnostic Instrumentation Catalog

- None added.

#### Files touched

`Source/Standalone/PianoRoll.h`, `Source/Standalone/PianoRoll.cpp`,
`Plans & Specs/Test Plans/v1-master-test-plan.md` (§B.2 + §B.1 hash backfill), paired plan +
running notes.

#### Commit(s)

`<hash>` (Tasks 1-3 + §B.2 + held entry + running notes — single batch commit per the bulk-run
model). Verified via Master Test Plan §B.2, <section-pass date/outcome>.

#### Next action

- <filled at apply: next unchecked §B section>.
